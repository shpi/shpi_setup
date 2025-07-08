// SPDX-License-Identifier: GPL-2.0-only
/*
 * Single Wire driver for Diodes AL3050 Backlight driver chip
 *
 * Copyright (C) 2020 SHPI GmbH
 * Author: Lutz Harder <harder.lutz@gmail.com>
 *
 * datasheet: https://www.diodes.com/assets/Datasheets/AL3050.pdf
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/backlight.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/io.h>

#define AL3050_MAX_RETRIES 3

#define ADDRESS_AL3050 	0x5800
#define T_DELAY_NS 	100000
#define T_DETECTION_NS 	450000
#define T_START_NS 	4500
#define T_EOS_NS 	3500
#define T_RESET_MS 	5
#define T_LOGIC_1_NS 	3500
#define T_LOGIC_0_NS 	10000
#define BRIGHTNESS_MAX 31
#define BRIGHTNESS_BMASK 0x1f
#define RFA_BMASK 0x80
#define RFA_ACK_WAIT 3500
#define RFA_MAX_ACK_TIME 1000000

#define BCM_PERI_BASE 0x20000000
#define GPIO_BASE (BCM_PERI_BASE + 0x200000) // = 0x20200000
#define GPIO_FSEL1 0x04
#define GPIO_SET0 0x1C
#define GPIO_CLR0 0x28
#define GPIO_PIN_18_SHIFT ((18 % 10) * 3)
#define GPIO_ALT5 0b010
#define GPIO_OUTPUT 0b001
#define GPIO_FSEL0 0x00
#define GPIO_PIN_2_SHIFT (2 * 3) // = 6

struct al3050_bl_data {
	struct device *fbdev;
	struct device *dev;
	struct gpio_desc *gpiod;
	int last_brightness;
	int power;
	int rfa_en;
	int init_display;
	struct mutex lock;
	struct delayed_work update_work;
	int pending_brightness;
	bool update_pending;
	unsigned long last_update_jiffies;
	int update_seq;
	int handled_seq;
};

#define GPPUD 0x94
#define GPPUDCLK0 0x98


/*
#include <linux/ktime.h>
static void busy_wait_ns(u64 nsecs)
{
	ktime_t start = ktime_get();
	while (ktime_to_ns(ktime_sub(ktime_get(), start)) < nsecs)
		cpu_relax();
}
*/


static void __iomem *gpio_base;

static void gpio19_pullup(void)
{
	if (!gpio_base)
		gpio_base = ioremap(0x20200000, SZ_4K);

	writel(2, gpio_base + 0x94);
	udelay(5);
	writel(1 << 19, gpio_base + 0x98);
	udelay(5);
	writel(0, gpio_base + 0x94);
	writel(0, gpio_base + 0x98);
}

static void a035vw01_reinit(struct al3050_bl_data *pchip)
{
	unsigned long flags;
	static void __iomem *gpio_base;
	u32 val;
	int i;
	const u16 cmds[] = { 0x0080, 0x01f8, 0x0246, 0x0305, 0x0b76,
			     0x0440, 0x0540, 0x0640, 0x0740, 0x0840,
			     0x0940, 0x00CA, 0x0A03 };

	local_irq_save(flags);

	if (!gpio_base)
		gpio_base = ioremap(0x20200000, SZ_4K); // GPIO base Pi Zero

	// we wait for toggling vsync to make repogram of display less visible (less flicker)
	//while (!(readl(gpio_base + 0x34) & (1 << 2))) {ndelay(10);} //while high
	//while ((readl(gpio_base + 0x34) & (1 << 2))) {ndelay(10);} //while low

	//gpio0 clk off
	//val = readl(gpio_base + 0x00);           // GPFSEL0 (für GPIO 0-9)
	//val &= ~(0b111 << 0);                    // GPIO0: Bits 0-2 löschen
	//val |=  (0b001 << 0);                    // Output-Modus setzen
	//writel(val, gpio_base + 0x00);
	//writel(1 << 0, gpio_base + 0x1C); // GPSET0: Set GPIO0 auf HIGH
	//	writel(1 << 0, gpio_base + 0x28); GPIO0 auf LOW

	//gpio3 hsync off
	val = readl(gpio_base + 0x00); // GPFSEL0 (für GPIO 0-9)
	val &= ~(0b111 << 9); // GPIO3: Bits 9-11 löschen
	val |= (0b001 << 9); // Output-Modus für GPIO3 setzen (001)
	writel(val, gpio_base + 0x00);
	writel(1 << 3, gpio_base + 0x1C); // GPSET0: Set GPIO3 auf HIGH

	// Set GPIO2 (CLK) to output
	val = readl(gpio_base + 0x00);
	val &= ~(0b111 << 6); // GPIO2 shift
	val |= (0b001 << 6); // output
	writel(val, gpio_base + 0x00);

	// Set GPIO18 (MOSI) to output
	val = readl(gpio_base + 0x04);
	val &= ~(0b111 << 24); // GPIO18 shift
	val |= (0b001 << 24); // output
	writel(val, gpio_base + 0x04);

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		u16 data = cmds[i];
		int b;

		gpiod_direction_output(pchip->gpiod, 0);

		for (b = 15; b >= 0; b--) {
			if (data & (1 << b))
				writel(1 << 18, gpio_base + 0x1C); // MOSI high
			else
				writel(1 << 18, gpio_base + 0x28); // MOSI low

			writel(1 << 2, gpio_base + 0x28); // CLK low
			ndelay(300);
			writel(1 << 2, gpio_base + 0x1C); // CLK high
			ndelay(300);
		}

		writel(1 << 18, gpio_base + 0x28); // MOSI low
		gpiod_direction_output(pchip->gpiod, 1);
		gpiod_direction_input(pchip->gpiod);
	}
	local_irq_restore(flags);
	// GPIO2 → ALT2
	val = readl(gpio_base + 0x00);
	val &= ~(0b111 << 6);
	val |= (0b110 << 6); // ALT2
	writel(val, gpio_base + 0x00);

	// GPIO3 wieder auf ALT2
	val = readl(gpio_base + 0x00);
	val &= ~(0b111 << 9);
	val |= (0b110 << 9);
	writel(val, gpio_base + 0x00);

	val = readl(gpio_base + 0x00);
	val &= ~(0b111 << 0); // GPIO0: Bits 0-2 löschen
	val |= (0b110 << 0); // ALT2-Modus setzen
	writel(val, gpio_base + 0x00);

	// GPIO18 → ALT5 (PWM0)
	val = readl(gpio_base + 0x04);
	val &= ~(0b111 << 24);
	val |= (0b010 << 24); // ALT5
	writel(val, gpio_base + 0x04);
}

static void al3050_init(struct al3050_bl_data *pchip)
{
	gpiod_direction_output(pchip->gpiod, 0);
	mdelay(T_RESET_MS);
	gpio19_pullup();
	gpiod_direction_input(pchip->gpiod);
	ndelay(T_DELAY_NS);
	gpiod_direction_output(pchip->gpiod, 0);
	ndelay(T_DETECTION_NS);
	gpiod_direction_input(pchip->gpiod);
	gpio19_pullup();
}

static int al3050_backlight_set_value_once(struct al3050_bl_data *pchip,
					   int brightness)
{
	unsigned int data, max_bmask, addr_bmask;
	unsigned long flags;
	max_bmask = 0x1 << 16;
	addr_bmask = max_bmask >> 8;
	data = ADDRESS_AL3050 | (brightness & BRIGHTNESS_BMASK);
	if (pchip->rfa_en)
		data |= RFA_BMASK;

	gpiod_direction_input(pchip->gpiod);
	ndelay(T_START_NS);

	local_irq_save(flags);

	for (max_bmask >>= 1; max_bmask > 0x0; max_bmask >>= 1) {
		int t_low = (data & max_bmask) ? T_LOGIC_1_NS : T_LOGIC_0_NS;

		gpiod_direction_output(pchip->gpiod, 0);
		ndelay(t_low);
		gpiod_direction_input(pchip->gpiod);
		ndelay((T_LOGIC_1_NS + T_LOGIC_0_NS) - t_low);

		if (max_bmask == addr_bmask) {
			gpiod_direction_output(pchip->gpiod, 0);
			ndelay(T_EOS_NS);
			gpiod_direction_input(pchip->gpiod);
			ndelay(T_START_NS);
		}
	}

	if (pchip->rfa_en) {
		gpiod_direction_output(pchip->gpiod, 0);
		ndelay(RFA_ACK_WAIT); //t VAL_ACKN
		gpiod_direction_input(pchip->gpiod);
		ndelay(1);
		int max_ack_time = RFA_MAX_ACK_TIME;
		local_irq_restore(flags);

		while (max_ack_time > 0) {
			if (gpiod_get_value_cansleep(pchip->gpiod) == 0)
				return 0; // ACK OK!
			max_ack_time -= RFA_ACK_WAIT;
		}
		return -EIO; // kein ACK!
	} else
		local_irq_restore(flags);

	return 0;
}

static int al3050_backlight_set_value(struct al3050_bl_data *pchip,
				      int brightness)
{
	int ret = -EIO, retries;
	if (pchip->init_display)
		al3050_init(pchip);

	for (retries = 0; retries < AL3050_MAX_RETRIES; retries++) {
		ret = al3050_backlight_set_value_once(pchip, brightness);
		if (ret == 0)
			break;
	}

	if (ret < 0) {
		dev_err(pchip->dev, "AL3050 no ack after %d retries, reinit\n",
			AL3050_MAX_RETRIES);
		if (!(pchip->init_display)) {
			al3050_init(pchip);
			ret = al3050_backlight_set_value_once(pchip,
							      brightness);
		}
	}

	if (pchip->init_display) {
		while (gpiod_get_value_cansleep(pchip->gpiod) == 0)
			udelay(10);
		a035vw01_reinit(pchip);
	}
	pchip->last_brightness = brightness;

	return ret;
}

static void al3050_bl_update_work(struct work_struct *work)
{
	struct al3050_bl_data *pchip = container_of(
		to_delayed_work(work), struct al3050_bl_data, update_work);
	int my_seq;

	mutex_lock(&pchip->lock);
	my_seq = pchip->update_seq;

	// Check: Wurde seit Scheduling noch mal geändert?
	if (pchip->handled_seq == my_seq) {
		// Schon erledigt -> Nichts tun!
		mutex_unlock(&pchip->lock);
		return;
	}
	pchip->handled_seq = my_seq;

	if (pchip->pending_brightness != pchip->last_brightness) {
		al3050_backlight_set_value(pchip, pchip->pending_brightness);
		pchip->last_update_jiffies = jiffies;
	}
	pchip->update_pending = false;

	mutex_unlock(&pchip->lock);
}

static int al3050_backlight_update_status(struct backlight_device *bl)
{
	struct al3050_bl_data *pchip = bl_get_data(bl);
	int brightness = bl->props.brightness;
	unsigned long now = jiffies;

	mutex_lock(&pchip->lock);

	if (bl->props.power != FB_BLANK_UNBLANK ||
	    bl->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK)) {
		gpiod_direction_output(pchip->gpiod, 0);
		bl->props.brightness = 0;
		pchip->power = 1;
	} else {
		if (pchip->power == 1) {
			if (!pchip->init_display)
				al3050_init(pchip);
			pchip->power = bl->props.power;
			bl->props.brightness = pchip->last_brightness;
		}
		if (time_after_eq(now, pchip->last_update_jiffies + HZ)) {
			if (brightness != pchip->last_brightness) {
				al3050_backlight_set_value(pchip, brightness);
				pchip->last_update_jiffies = now;
			}

		} else {
			// Schedule the update for after 1s since last update
			unsigned long delay =
				(pchip->last_update_jiffies + HZ) - now;

			if (!pchip->update_pending ||
			    brightness != pchip->pending_brightness) {
				pchip->pending_brightness = brightness;
				pchip->update_pending = true;
				pchip->update_seq++;
				schedule_delayed_work(&pchip->update_work,
						      delay);
			}
			dev_info(
				pchip->dev,
				"AL3050 backlight change delayed by rate limit\n");
		}
	}

	mutex_unlock(&pchip->lock);
	return 0;
}

static const struct backlight_ops al3050_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = al3050_backlight_update_status,
};

static int al3050_backlight_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct al3050_bl_data *pchip;
	struct backlight_device *bl;
	struct backlight_properties props;
	u32 value;
	int ret;

	pchip = devm_kzalloc(dev, sizeof(*pchip), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	pchip->dev = dev;
	mutex_init(&pchip->lock);

	pchip->gpiod = devm_gpiod_get(dev, "bl", GPIOD_OUT_LOW);
	if (IS_ERR(pchip->gpiod))
		return PTR_ERR(pchip->gpiod);

	ret = of_property_read_u32(dev->of_node, "rfa_en", &value);
	pchip->rfa_en = (ret < 0) ? 0 : value;

	ret = of_property_read_u32(dev->of_node, "init_display", &value);
	pchip->init_display = (ret < 0) ? 0 : value;

	dev_info(dev, "AL3050 rfa_en=%d, gpio18_low=%d\n", pchip->rfa_en,
		 pchip->init_display);

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = BRIGHTNESS_MAX;
	props.brightness = BRIGHTNESS_MAX;
	pchip->update_seq = 0;
	pchip->handled_seq = 0;
	pchip->last_brightness = -1;
	bl = devm_backlight_device_register(dev, dev_name(dev), dev, pchip,
					    &al3050_bl_ops, &props);
	if (IS_ERR(bl))
		return PTR_ERR(bl);

	platform_set_drvdata(pdev, bl);

	al3050_init(pchip);

	INIT_DELAYED_WORK(&pchip->update_work, al3050_bl_update_work);
	pchip->update_pending = false;
	pchip->last_update_jiffies =
		jiffies - HZ; // so first update is immediate

	dev_info(dev, "AL3050 backlight initialized\n");
	return 0;
}

static const struct of_device_id al3050_of_match[] = {
	{ .compatible = "al3050_bl" },
	{},
};
MODULE_DEVICE_TABLE(of, al3050_of_match);

static struct platform_driver al3050_backlight_driver = {
	.driver = {
		.name = "al3050_bl",
		.of_match_table = al3050_of_match,
	},
	.probe = al3050_backlight_probe,
};

module_platform_driver(al3050_backlight_driver);

MODULE_DESCRIPTION("Single Wire AL3050 Backlight Driver");
MODULE_AUTHOR("Lutz Harder <harder.lutz@gmail.com>");
MODULE_LICENSE("GPL");

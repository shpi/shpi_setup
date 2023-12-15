// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2020
 * Lutz Harder, SHPI GmbH <lh@shpi.de>
 *
 * LED driver for the WS2812B SPI 
 */

#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/types.h>
#include <asm/io.h>

#define WS2812_SYMBOL_LENGTH                     4
#define LED_COLOURS                              3
#define LED_RESET_US                             60
#define LED_RESET_WAIT_TIME                      300
#define SYMBOL_HIGH                              0b1100 // 0.6us high 0.6us low
#define SYMBOL_LOW                               0b1000 // 0.3us high, 0.9us low
#define WS2812_FREQ                              800000


/* constans below for SHPI.zero lite, for setting MOSI as Input
 * and disable RISING EDGE detection while sending colors */

#define GPREN0		          		0x4c    /* Pin Rising Edge Detect Enable */
#define GPREN1          			0x50    /* Pin Rising Edge Detect Enable */
#define GPFSEL          			0x00    /* function selector */
#define BCM2708_PERI_BASE        		0x20000000
#define GPIO_BASE                		(BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


struct ws2812_led {
	struct led_classdev	ldev;
	int			id;
	char			name[sizeof("ws2812-red-00")]; //red grn bl
	struct ws2812		*priv;
	};


struct ws2812 {
  struct spi_device	*spi;
  struct mutex		mutex;
  u8                    *rawstream;
  u8			is_zero_lite;
  int                   num_leds;
  int			spi_byte_count;
  struct ws2812_led 	leds[];
};



static void set_gpio_mode(uint8_t pin, uint8_t mode)
{       int regnum = pin / 10;
        int offset = (pin % 10) * 3;
        uint8_t funcmap[] = { 4, 5, 6, 7, 3, 2, 1, 0 }; /* 4 -> ALT0 , 0 -> input, 1 -> output */
        void __iomem *regs2 = ioremap(GPIO_BASE + GPFSEL + (regnum*4),  4);
        uint32_t gpfsel = readl(regs2);
        gpfsel &= ~(0x7 << offset);
        gpfsel |= ((funcmap[mode]) << offset);
        writel(gpfsel, regs2);
        iounmap(regs2);
}
static void set_gpio_ren(uint8_t pin, uint8_t mode) /* rising edge detection */
{       int regnum = pin >> 5;
        int offset = (pin & 0x1f);
        void __iomem *regs = ioremap(GPIO_BASE + GPFSEL + GPREN0 + (4*regnum),  4);
        uint32_t ren = readl(regs);
        if (mode) ren |= (1 << offset);
        else ren &= (0xFFFFFFFF & (0 << offset));
        writel(ren, regs);
        iounmap(regs); 
}



static int  ws2812_render(struct ws2812 *priv)
{
    volatile u8	*rawstream = priv->rawstream;
    int i, k, l;
    int bitpos =  7;
    int bytepos = 0;    // SPI
    int z = priv->num_leds*LED_COLOURS;
    int ret;
    unsigned long flags;

    for (i = 0; i < z; i++)                // Leds * Colorchannels
        {

                for (k = 7; k >= 0; k--)                   // Bit
                {

                    uint8_t symbol = SYMBOL_LOW;
                    if (priv->leds[i].ldev.brightness & (1 << k))
                        symbol = SYMBOL_HIGH;

                    for (l = WS2812_SYMBOL_LENGTH; l > 0; l--)               // Symbol
                    {
                        volatile u8  *byteptr = &rawstream[bytepos];    // SPI

                        *byteptr &= ~(1 << bitpos);
                        if (symbol & (1 << l))
                                *byteptr |= (1 << bitpos);
                        bitpos--;
                        if (bitpos < 0)
                        {
                                bytepos++;
                                bitpos = 7;
                            }
                        }
                    }
                }
        local_irq_save(flags);
	if (priv->is_zero_lite > 0) {
		set_gpio_mode(10,0);
		//set_gpio_ren(10,0);
	}

	ret = spi_write(priv->spi, priv->rawstream, priv->spi_byte_count);
        
	if (priv->is_zero_lite > 0)	{
		set_gpio_mode(10,7);
		//set_gpio_ren(10,1);
	}
        local_irq_restore(flags);
	return ret;
}




static int ws2812_set_brightness_blocking(struct led_classdev *ldev,
                                      enum led_brightness brightness)
{
        struct ws2812_led *led = container_of(ldev, struct ws2812_led,
                                                  ldev);


        int ret;

        mutex_lock(&led->priv->mutex);
        ret = ws2812_render(led->priv);
        mutex_unlock(&led->priv->mutex);

        return ret;
}



static int ws2812_probe(struct spi_device *spi)
{
	struct ws2812		*priv;
	struct ws2812_led	*led;
	const char		*color_order; //RGB,  GRB, ...
	int 			i, ret, spi_byte_count;
	u8 			is_zero_lite;
	int			num_leds;
	long 			led_bit_count;



	ret = device_property_read_u32(&spi->dev, "num-leds", &num_leds);
	if (ret < 0)
		num_leds = 1;

	dev_err(&spi->dev,"Number of WS2812 to be registered:%d\n",num_leds);


        ret = device_property_read_string(&spi->dev, "color-order", &color_order);
        if (ret < 0)
		color_order = "GRB";


	led_bit_count = ((num_leds * LED_COLOURS * 8 * WS2812_SYMBOL_LENGTH) + ((LED_RESET_US * \
                                                  (WS2812_FREQ * WS2812_SYMBOL_LENGTH)) / 1000000));

	spi_byte_count = ((((led_bit_count >> 3) & ~0x7) + 4) + 4);

        dev_err(&spi->dev,"WS2812 Buffer size:%d\n",spi_byte_count);


	priv = devm_kzalloc(&spi->dev, struct_size(priv, leds, num_leds*LED_COLOURS), GFP_KERNEL);

	if (!priv)
		return -ENOMEM;

	priv->rawstream = devm_kzalloc(&spi->dev, spi_byte_count, GFP_KERNEL);

	if (!priv->rawstream)
		return -ENOMEM;

  	spi->mode = SPI_MODE_0;
  	spi->bits_per_word = 8;
  	spi->max_speed_hz = WS2812_FREQ * WS2812_SYMBOL_LENGTH;
	priv->spi = spi;

	is_zero_lite = 0;

	if (device_property_read_bool(&spi->dev, "is-zero-lite"))
	{
		is_zero_lite = 1;
		dev_err(&spi->dev,"WS2812, setup as Zero Lite: %d\n",is_zero_lite);

	}
	priv->is_zero_lite = is_zero_lite;

	priv->num_leds = num_leds;
	priv->spi_byte_count = spi_byte_count;

	for (i = 0; i < num_leds*LED_COLOURS; i++) {

		led		= &priv->leds[i];
		led->id		= i;
		led->priv	= priv;

		if (color_order[i % LED_COLOURS] == 'G') snprintf(led->name, sizeof(led->name), "ws2812-grn-%d", (i/LED_COLOURS));
		if (color_order[i % LED_COLOURS] == 'R') snprintf(led->name, sizeof(led->name), "ws2812-red-%d", (i/LED_COLOURS));
		if (color_order[i % LED_COLOURS] == 'B') snprintf(led->name, sizeof(led->name), "ws2812-blu-%d", (i/LED_COLOURS));

		mutex_init(&led->priv->mutex);
		led->ldev.name = led->name;
		led->ldev.brightness = LED_OFF;
		led->ldev.max_brightness = 0xff;
		led->ldev.brightness_set_blocking = ws2812_set_brightness_blocking;

		ret = led_classdev_register(&spi->dev, &led->ldev);

		if (ret < 0)
			goto eledcr;
	}

	spi_set_drvdata(spi, priv);

        set_gpio_mode(8,2); //disable chipselect of shpi
	ws2812_render(priv);

	return 0;

eledcr:
	while (i--)
		led_classdev_unregister(&priv->leds[i].ldev);

	return ret;
}

static void ws2812_remove(struct spi_device *spi)
{
	struct ws2812	*priv = spi_get_drvdata(spi);
	int i;

	for (i = 0; i < (priv->num_leds*LED_COLOURS); i++)
		led_classdev_unregister(&priv->leds[i].ldev);

	
}

static struct spi_driver ws2812_driver = {
	.probe		= ws2812_probe,
	.remove		= ws2812_remove,
	.driver = {
		.name	= "ws2812",
	},
};

module_spi_driver(ws2812_driver);

MODULE_AUTHOR("Lutz Harder <lh@shpi.de>");
MODULE_DESCRIPTION("WS2812 SPI LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:ws2812");

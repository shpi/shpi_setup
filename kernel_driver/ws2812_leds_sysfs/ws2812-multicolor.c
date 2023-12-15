// SPDX-License-Identifier: GPL-2.0
/*
 * WS2812 SPI led driver
 *
 * (C) 2020 Lutz Harder <harder.lutz@gmail.com>
 */

#include <linux/led-class-multicolor.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/leds.h>
#include <linux/mutex.h>


#define NUM_LEDS			2
#define WS2812_LED_NUM_CHANNELS		3

#define WS2812_SYMBOL_LENGTH            4
#define LED_RESET_US                    60
#define LED_BIT_COUNT                   ((NUM_LEDS * WS2812_LED_NUM_CHANNELS * 8 * 3) + ((LED_RESET_US * \
                                         (WS2812_FREQ * WS2812_SYMBOL_LENGTH)) / 1000000))

#define LED_RESET_WAIT_TIME             300
#define SPI_BYTE_COUNT                  ((((LED_BIT_COUNT >> 3) & ~0x7) + 4) + 4)
#define SYMBOL_HIGH                     0b1100 // 0.6us high 0.6us low
#define SYMBOL_LOW                      0b1000 // 0.3us high, 0.9us low
#define WS2812_FREQ                     800000
#define LED_COLOR_ID_MULTI		8

struct ws2812_led {
	struct led_classdev_mc mc_cdev;
	struct mc_subled subled_info[WS2812_LED_NUM_CHANNELS];
	int reg;
};

#define to_ws2812_led(l)			container_of(l, struct ws2812_led, mc_cdev)

struct ws2812_leds {
  struct spi_device *spi;
  uint8_t rawstream[SPI_BYTE_COUNT];
  struct mutex lock;
  struct ws2812_led leds[]; 
};



static void  ws2812_render(struct ws2812_leds* leds)
{
    int i, k, l;
    unsigned j;
    int bitpos =  7;
    int bytepos = 0;    // SPI

    for (i = 0; i < NUM_LEDS; i++)                // Led
        {
            for (j = 0; j < 3; j++)               // Color
            {
                for (k = 7; k >= 0; k--)                   // Bit
                {
                    uint8_t symbol = SYMBOL_LOW;
                    if (leds->leds[i].subled_info[j].brightness & (1 << k))
                        symbol = SYMBOL_HIGH;

                    for (l = WS2812_SYMBOL_LENGTH; l > 0; l--)               // Symbol
                    {
                        volatile uint8_t  *byteptr = &leds->rawstream[bytepos];    // SPI

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
    }

}


static int ws2812_led_brightness_set_blocking(struct led_classdev *cdev,
					     enum led_brightness brightness)
{
	struct led_classdev_mc *mc_cdev = lcdev_to_mccdev(cdev);
	struct ws2812_leds *leds = dev_get_drvdata(cdev->dev->parent);
	struct ws2812_led *led = to_ws2812_led(mc_cdev);
	int ret;

	led_mc_calc_color_components(&led->mc_cdev, brightness);

	ws2812_render(leds);
	//add mutex later
	ret = spi_write(leds->spi, &leds->rawstream ,  SPI_BYTE_COUNT);
	return ret;
}

static int ws2812_led_register(struct spi_device *spi, struct ws2812_led *led,
			      struct device_node *np)
{
	struct led_init_data init_data = {};
	struct device *dev = &spi->dev;
	struct led_classdev *cdev;
	int ret, color;

	ret = of_property_read_u32(np, "reg", &led->reg);
	if (ret || led->reg >= NUM_LEDS) {
		dev_warn(dev,
			 "Node %pOF: must contain 'reg' property with values between 0 and %i\n",
			 np, NUM_LEDS - 1);
		return 0;
	}

/*	ret = of_property_read_u32(np, "color", &color);
	if (ret || color != LED_COLOR_ID_MULTI) {
		dev_warn(dev,
			 "Node %pOF: must contain 'color' property with value LED_COLOR_ID_MULTI\n",
			 np);
		return 0;
	} */

	led->subled_info[0].color_index = LED_COLOR_ID_RED;
	led->subled_info[0].channel = 0;
	led->subled_info[1].color_index = LED_COLOR_ID_GREEN;
	led->subled_info[1].channel = 1;
	led->subled_info[2].color_index = LED_COLOR_ID_BLUE;
	led->subled_info[2].channel = 2;

	led->mc_cdev.subled_info = led->subled_info;
	led->mc_cdev.num_colors = WS2812_LED_NUM_CHANNELS;

	init_data.fwnode = &np->fwnode;

	cdev = &led->mc_cdev.led_cdev;
	cdev->max_brightness = 255;
	cdev->brightness_set_blocking = ws2812_led_brightness_set_blocking;

	of_property_read_string(np, "linux,default-trigger", &cdev->default_trigger);


	ret = devm_led_classdev_multicolor_register_ext(dev, &led->mc_cdev, &init_data);
	if (ret < 0) {
		dev_err(dev, "Cannot register LED %pOF: %i\n", np, ret);
		return ret;
	}

	return 1;
}



static int ws2812_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct device_node *np = dev->of_node, *child;
	struct ws2812_leds *leds;
	struct ws2812_led *led;
	int ret, count;

	count = of_get_available_child_count(np);
        dev_err(dev, "Starting WS2812 module\n");

	if (!count) {
		dev_err(dev, "LEDs are not defined in device tree!\n");
		return -ENODEV;
	} else if (count > NUM_LEDS) {
		dev_err(dev, "Too many LEDs defined in device tree!\n");
		return -EINVAL;
	}

        //leds->rawstream =  kmalloc(SPI_BYTE_COUNT, GFP_KERNEL);
	leds = devm_kzalloc(&spi->dev, struct_size(leds, leds, count), GFP_KERNEL);


	if (!leds){
		dev_err(dev, "ERROR devm_kzalloc!\n");
		return -ENOMEM;
	}


        spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	spi->max_speed_hz = WS2812_FREQ * WS2812_SYMBOL_LENGTH;

	leds->spi = spi;
	spi_setup(spi);

	spi_set_drvdata(spi, leds);

	mutex_init(&leds->lock);

	led = &leds->leds[0];
        dev_err(dev, "Checking WS2812 nodes!\n");

	for_each_available_child_of_node(np, child) {
		ret = ws2812_led_register(spi, led, child);
		if (ret < 0)
			return ret;

		led += ret;
	}


	return 0;
}




static const struct spi_device_id ws2812_id[] = {
	{ "ws2812", 0 },
	{ }
};


MODULE_DEVICE_TABLE(spi, ws2812_id);

static const struct of_device_id ws2812_of_spi_match[] = {
	{ .compatible = "ws2812" },
	{ }
};

MODULE_DEVICE_TABLE(of, ws2812_of_spi_match);

static struct spi_driver ws2812_driver = {
	.driver = {
		.name = "ws2812",
		.of_match_table = ws2812_of_spi_match,
	},
	.probe = ws2812_probe,
	.id_table = ws2812_id,
};

module_spi_driver(ws2812_driver);

MODULE_AUTHOR("Lutz Harder <harder.lutz@gmail.com>");
MODULE_DESCRIPTION("WS2812 SPI driver");
MODULE_LICENSE("GPL v2");

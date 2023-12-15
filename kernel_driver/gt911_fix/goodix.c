/*
 *  Driver for Goodix Touchscreens
 *
 *  Modified for writing X2Y, width and height for screens without rst/int pins
 *
 *  Copyright (c) 2014 Red Hat Inc.
 *  Copyright (c) 2015 K. Merker <merker@debian.org>
 *
 *  This code is based on gt9xx.c authored by andrew@goodix.com:
 *
 *  2010 - 2012 Goodix Technology.
 */
//#define DEBUG

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2 of the License.
 */

#include <linux/kernel.h>
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <asm/unaligned.h>

struct goodix_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	int abs_x_max;
	int abs_y_max;
	bool swapped_x_y;
	bool inverted_x;
	bool inverted_y;
	unsigned int max_touch_num;
	unsigned int int_trigger_type;
	int cfg_len;
	struct gpio_desc *gpiod_int;
	struct gpio_desc *gpiod_rst;
	u16 id;
	u16 version;
	const char *cfg_name;
	struct completion firmware_loading_complete;
	unsigned long irq_flags;
        bool Y2Y;
        bool X2X;
	bool X2Y;
	bool requestedX2Y;
	u16  requestedXSize;
	u16  requestedYSize;
	u8   requestedRefreshRate;
};



#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

#define GOODIX_GPIO_INT_NAME		"irq"
#define GOODIX_GPIO_RST_NAME		"reset"

#define GOODIX_MAX_HEIGHT		480
#define GOODIX_MAX_WIDTH		800
#define GOODIX_INT_TRIGGER		1
#define GOODIX_CONTACT_SIZE		8
#define GOODIX_MAX_CONTACTS		10

#define GOODIX_CONFIG_MAX_LENGTH	240
#define GOODIX_CONFIG_911_LENGTH	186
#define GOODIX_CONFIG_967_LENGTH	228

/* Register defines */
#define GOODIX_REG_COMMAND		0x8040
#define GOODIX_CMD_SCREEN_OFF		0x05

#define GOODIX_READ_COOR_ADDR		0x814E
#define GOODIX_REG_CONFIG_DATA		0x8047
#define GOODIX_REG_ID			0x8140

#define GOODIX_BUFFER_STATUS_READY	BIT(7)
#define GOODIX_BUFFER_STATUS_TIMEOUT	20

#define RESOLUTION_LOC		1
#define RESOLUTION_LOC_X	1
#define RESOLUTION_LOC_Y	3
#define MAX_CONTACTS_LOC	5
#define TRIGGER_LOC		6

static const unsigned long goodix_irq_flags[] = {
	IRQ_TYPE_EDGE_RISING,
	IRQ_TYPE_EDGE_FALLING,
	IRQ_TYPE_LEVEL_LOW,
	IRQ_TYPE_LEVEL_HIGH,
};

/*
 * Those tablets have their coordinates origin at the bottom right
 * of the tablet, as if rotated 180 degrees
 */
static const struct dmi_system_id rotated_screen[] = {
#if defined(CONFIG_DMI) && defined(CONFIG_X86)
	{
		.ident = "WinBook TW100",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "WinBook"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TW100")
		}
	},
	{
		.ident = "WinBook TW700",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "WinBook"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TW700")
		},
	},
#endif
	{}
};


static int checkFirmware(struct goodix_ts_data *ts, u8 *config);


/**
 * goodix_i2c_read - read data from a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to read from.
 * @buf: raw write data buffer.
 * @len: length of the buffer to write
 */
static int goodix_i2c_read(struct i2c_client *client,
			   u16 reg, u8 *buf, int len)
{
	struct i2c_msg msgs[2];
	u16 wbuf = cpu_to_be16(reg);
	int ret;

	msgs[0].flags = 0;
	msgs[0].addr  = client->addr;
	msgs[0].len   = 2;
	msgs[0].buf   = (u8 *)&wbuf;

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len;
	msgs[1].buf   = buf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}

/**
 * goodix_i2c_write - write data to a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to write to.
 * @buf: raw data buffer to write.
 * @len: length of the buffer to write
 */
static int goodix_i2c_write(struct i2c_client *client, u16 reg, const u8 *buf,
			    unsigned len)
{
	u8 *addr_buf;
	struct i2c_msg msg;
	int ret;

	addr_buf = kmalloc(len + 2, GFP_KERNEL);
	if (!addr_buf)
		return -ENOMEM;

	addr_buf[0] = reg >> 8;
	addr_buf[1] = reg & 0xFF;
	memcpy(&addr_buf[2], buf, len);

	msg.flags = 0;
	msg.addr = client->addr;
	msg.buf = addr_buf;
	msg.len = len + 2;

	ret = i2c_transfer(client->adapter, &msg, 1);
	kfree(addr_buf);
	return ret < 0 ? ret : (ret != 1 ? -EIO : 0);
}

static int goodix_i2c_write_u8(struct i2c_client *client, u16 reg, u8 value)
{
	return goodix_i2c_write(client, reg, &value, sizeof(value));
}


static int goodix_get_cfg_len(u16 id)
{
	switch (id) {
	case 911:
	case 9271:
	case 9110:
	case 927:
	case 928:
		return GOODIX_CONFIG_911_LENGTH;

	case 912:
	case 967:
		return GOODIX_CONFIG_967_LENGTH;

	default:
		return GOODIX_CONFIG_MAX_LENGTH;
	}
}

static int goodix_ts_read_input_report(struct goodix_ts_data *ts, u8 *data)
{
	unsigned long max_timeout;
	int touch_num;
	int error;

	/*
	 * The 'buffer status' bit, which indicates that the data is valid, is
	 * not set as soon as the interrupt is raised, but slightly after.
	 * This takes around 10 ms to happen, so we poll for 20 ms.
	 */
	max_timeout = jiffies + msecs_to_jiffies(GOODIX_BUFFER_STATUS_TIMEOUT);
	do {
		error = goodix_i2c_read(ts->client, GOODIX_READ_COOR_ADDR,
					data, GOODIX_CONTACT_SIZE + 1);
		if (error) {
			dev_err(&ts->client->dev, "I2C transfer error: %d\n",
					error);
			return error;
		}

		if (data[0] & GOODIX_BUFFER_STATUS_READY) {
			touch_num = data[0] & 0x0f;
			if (touch_num > ts->max_touch_num)
				return -EPROTO;

			if (touch_num > 1) {
				data += 1 + GOODIX_CONTACT_SIZE;
				error = goodix_i2c_read(ts->client,
						GOODIX_READ_COOR_ADDR +
							1 + GOODIX_CONTACT_SIZE,
						data,
						GOODIX_CONTACT_SIZE *
							(touch_num - 1));
				if (error)
					return error;
			}

			return touch_num;
		}

		usleep_range(1000, 2000); /* Poll every 1 - 2 ms */
	} while (time_before(jiffies, max_timeout));

	/*
	 * The Goodix panel will send spurious interrupts after a
	 * 'finger up' event, which will always cause a timeout.
	 */
	return 0;
}

static void goodix_ts_report_touch(struct goodix_ts_data *ts, u8 *coor_data)
{
	int id = coor_data[0] & 0x0F;
	int input_x = get_unaligned_le16(&coor_data[1]);
	int input_y = get_unaligned_le16(&coor_data[3]);
	int input_w = get_unaligned_le16(&coor_data[5]);

	if(ts->X2Y)
	{
		/* Inversions have to happen before axis swapping */
		if (ts->inverted_x)
			input_x = ts->abs_x_max - input_x;
		if (ts->inverted_y)
			input_y = ts->abs_y_max - input_y;
		if (ts->swapped_x_y)
			swap(input_x, input_y);
	}
	else
	{
		/* Inversions have to happen after axis swapping */
		if (ts->swapped_x_y)
			swap(input_x, input_y);
		if (ts->inverted_x)
			input_x = ts->abs_x_max - input_x;
		if (ts->inverted_y)
			input_y = ts->abs_y_max - input_y;
	}

	input_mt_slot(ts->input_dev, id);
	input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, input_w);


}

/**
 * goodix_process_events - Process incoming events
 *
 * @ts: our goodix_ts_data pointer
 *
 * Called when the IRQ is triggered. Read the current device state, and push
 * the input events to the user space.
 */
static void goodix_process_events(struct goodix_ts_data *ts)
{
	u8  point_data[1 + GOODIX_CONTACT_SIZE * GOODIX_MAX_CONTACTS];
	int touch_num;
	int i;

	touch_num = goodix_ts_read_input_report(ts, point_data);
	if (touch_num < 0)
		return;

	/*
	 * Bit 4 of the first byte reports the status of the capacitive
	 * Windows/Home button.
	 */
	input_report_key(ts->input_dev, KEY_LEFTMETA, point_data[0] & BIT(4));

	for (i = 0; i < touch_num; i++)
		goodix_ts_report_touch(ts,
				&point_data[1 + GOODIX_CONTACT_SIZE * i]);

	input_mt_sync_frame(ts->input_dev);
	input_sync(ts->input_dev);
}

/**
 * goodix_ts_irq_handler - The IRQ handler
 *
 * @irq: interrupt number.
 * @dev_id: private data pointer.
 */
static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;


	goodix_process_events(ts);

	if (goodix_i2c_write_u8(ts->client, GOODIX_READ_COOR_ADDR, 0) < 0)
		dev_err(&ts->client->dev, "I2C write end_cmd error\n");

	return IRQ_HANDLED;
}

static void goodix_free_irq(struct goodix_ts_data *ts)
{
	devm_free_irq(&ts->client->dev, ts->client->irq, ts);
}

static int goodix_request_irq(struct goodix_ts_data *ts)
{
	return devm_request_threaded_irq(&ts->client->dev, ts->client->irq,
					 NULL, goodix_ts_irq_handler,
					 ts->irq_flags, ts->client->name, ts);
}

/**
 * goodix_check_cfg - Checks if config fw is valid
 *
 * @ts: goodix_ts_data pointer
 * @cfg: firmware config data
 */
static int goodix_check_cfg(struct goodix_ts_data *ts,
			    const struct firmware *cfg)
{
	int i, raw_cfg_len;
	u8 check_sum = 0;

	if (cfg->size > GOODIX_CONFIG_MAX_LENGTH) {
		dev_err(&ts->client->dev,
			"The length of the config fw is not correct");
		return -EINVAL;
	}

	raw_cfg_len = cfg->size - 2;
	for (i = 0; i < raw_cfg_len; i++)
		check_sum += cfg->data[i];
	check_sum = (~check_sum) + 1;
	if (check_sum != cfg->data[raw_cfg_len]) {
		dev_err(&ts->client->dev,
			"The checksum of the config fw is not correct");
		return -EINVAL;
	}

	if (cfg->data[raw_cfg_len + 1] != 1) {
		dev_err(&ts->client->dev,
			"Config fw must have Config_Fresh register set");
		return -EINVAL;
	}

	return 0;
}

/**
 * goodix_send_cfg - Write fw config to device
 *
 * @ts: goodix_ts_data pointer
 * @cfg: config firmware to write to device
 */
static int goodix_send_cfg(struct goodix_ts_data *ts,
			   const struct firmware *cfg)
{
	int error;

	error = goodix_check_cfg(ts, cfg);
	if (error)
		return error;

	error = goodix_i2c_write(ts->client, GOODIX_REG_CONFIG_DATA, cfg->data,
				 cfg->size);
	if (error) {
		dev_err(&ts->client->dev, "Failed to write config data: %d",
			error);
		return error;
	}
	dev_info(&ts->client->dev, "Config sent successfully.");

	/* Let the firmware reconfigure itself, so sleep for 10ms */
	usleep_range(10000, 11000);

	return 0;
}

static int goodix_int_sync(struct goodix_ts_data *ts)
{
	int error;

	error = gpiod_direction_output(ts->gpiod_int, 0);
	if (error)
		return error;

	msleep(50);				/* T5: 50ms */

	error = gpiod_direction_input(ts->gpiod_int);
	if (error)
		return error;

	return 0;
}

/**
 * goodix_reset - Reset device during power on
 *
 * @ts: goodix_ts_data pointer
 */
static int goodix_reset(struct goodix_ts_data *ts)
{
	int error;

	/* begin select I2C slave addr */
	error = gpiod_direction_output(ts->gpiod_rst, 0);
	if (error)
		return error;

	msleep(20);				/* T2: > 10ms */

	/* HIGH: 0x28/0x29, LOW: 0xBA/0xBB */
	error = gpiod_direction_output(ts->gpiod_int, ts->client->addr == 0x14);
	if (error)
		return error;

	usleep_range(100, 2000);		/* T3: > 100us */

	error = gpiod_direction_output(ts->gpiod_rst, 1);
	if (error)
		return error;

	usleep_range(6000, 10000);		/* T4: > 5ms */

	/* end select I2C slave addr */
	error = gpiod_direction_input(ts->gpiod_rst);
	if (error)
		return error;

	error = goodix_int_sync(ts);
	if (error)
		return error;

	return 0;
}

/**
 * goodix_get_gpio_config - Get GPIO config from ACPI/DT
 *
 * @ts: goodix_ts_data pointer
 */
static int goodix_get_gpio_config(struct goodix_ts_data *ts)
{
	int error;
	struct device *dev;
	struct gpio_desc *gpiod;

	if (!ts->client)
		return -EINVAL;
	dev = &ts->client->dev;

	/* Get the interrupt GPIO pin number */
	gpiod = devm_gpiod_get_optional(dev, GOODIX_GPIO_INT_NAME, GPIOD_IN);
	if (IS_ERR(gpiod)) {
		error = PTR_ERR(gpiod);
		if (error != -EPROBE_DEFER)
			dev_info(dev, "Failed to get %s GPIO: %d\n",
				GOODIX_GPIO_INT_NAME, error);
		return error;
	}

	ts->gpiod_int = gpiod;

	/* Get the reset line GPIO pin number */
	gpiod = devm_gpiod_get_optional(dev, GOODIX_GPIO_RST_NAME, GPIOD_IN);
	if (IS_ERR(gpiod)) {
		error = PTR_ERR(gpiod);
		if (error != -EPROBE_DEFER)
			dev_info(dev, "Failed to get %s GPIO: %d\n",
				GOODIX_GPIO_RST_NAME, error);
		return error;
	}

	ts->gpiod_rst = gpiod;

	return 0;
}

/**
 * goodix_read_config - Read the embedded configuration of the panel
 *
 * @ts: our goodix_ts_data pointer
 *
 * Must be called during probe
 */
static void goodix_read_config(struct goodix_ts_data *ts)
{
	u8 config[GOODIX_CONFIG_MAX_LENGTH];
	int error;

	error = goodix_i2c_read(ts->client, GOODIX_REG_CONFIG_DATA,
				config, ts->cfg_len);
	if (error) {
		dev_warn(&ts->client->dev,
			 "Error reading config (%d), using defaults\n",
			 error);
		ts->abs_x_max = GOODIX_MAX_WIDTH;
		ts->abs_y_max = GOODIX_MAX_HEIGHT;
		if (ts->swapped_x_y)
			swap(ts->abs_x_max, ts->abs_y_max);
		ts->int_trigger_type = GOODIX_INT_TRIGGER;
		ts->max_touch_num = GOODIX_MAX_CONTACTS;
		return;
	}

	//dev_info(&ts->client->dev, "Current size = (%u, %u), X2Y = %u, refresh = %u", ts->requestedXSize, ts->requestedYSize, ts->X2Y, ts->requestedRefreshRate);

	checkFirmware(ts, config);

	ts->abs_x_max = get_unaligned_le16(&config[RESOLUTION_LOC]);
	ts->abs_y_max = get_unaligned_le16(&config[RESOLUTION_LOC + 2]);
	ts->X2Y = CHECK_BIT(config[0x804d - GOODIX_REG_CONFIG_DATA], 3);
        ts->Y2Y = CHECK_BIT(config[0x804d - GOODIX_REG_CONFIG_DATA], 7);
        ts->X2X = CHECK_BIT(config[0x804d - GOODIX_REG_CONFIG_DATA], 6);

	dev_info(&ts->client->dev, "Current size = (%u, %u), X2Y = %u, Y2Y = %u, X2X = %u, refresh = %u", ts->requestedXSize, ts->requestedYSize, ts->X2Y, ts->Y2Y, ts->X2X, ts->requestedRefreshRate);

	if (ts->swapped_x_y)
		swap(ts->abs_x_max, ts->abs_y_max);
	ts->int_trigger_type = config[TRIGGER_LOC] & 0x03;
	ts->max_touch_num = config[MAX_CONTACTS_LOC] & 0x0f;
	if (!ts->abs_x_max || !ts->abs_y_max || !ts->max_touch_num) {
		dev_err(&ts->client->dev,
			"Invalid config, using defaults\n");
		ts->abs_x_max = GOODIX_MAX_WIDTH;
		ts->abs_y_max = GOODIX_MAX_HEIGHT;
		if (ts->swapped_x_y)
			swap(ts->abs_x_max, ts->abs_y_max);
		ts->max_touch_num = GOODIX_MAX_CONTACTS;
	}

	if (dmi_check_system(rotated_screen)) {
		ts->inverted_x = true;
		ts->inverted_y = true;
		dev_info(&ts->client->dev,
			 "Applying '180 degrees rotated screen' quirk\n");
	}
}


static ssize_t goodix_dump_config_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	u8 config[GOODIX_CONFIG_MAX_LENGTH];
	int error, count = 0, i;

	wait_for_completion(&ts->firmware_loading_complete);

	error = goodix_i2c_read(ts->client, GOODIX_REG_CONFIG_DATA,
				config, ts->cfg_len);
	if (error) {
		dev_warn(&ts->client->dev,
			 "Error reading config (%d)\n",  error);
		return error;
	}

	for (i = 0; i < ts->cfg_len; i++)
		count += scnprintf(buf + count, PAGE_SIZE - count, "%02x ",
				   config[i]);

                       u8 config2[] = {65, 224, 1, 32, 3, 5, 4, 0, 1, 10, 40, 10, 80, 50, 3, 5, 0, 0, 0, 0, 0,
                       0, 0, 24, 25, 30, 20, 135, 38, 8, 67, 65, 12, 8, 0, 0, 1, 2, 3, 29, 0,
                       0, 0, 0, 0, 3, 100, 50, 0, 0, 0, 30, 80, 148, 197, 2, 7, 0, 0, 0, 178,
                       33, 0, 192, 40, 0, 125, 49, 0, 107, 59, 0, 92, 72, 0, 92, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 6, 8, 10, 12, 14, 16, 255, 255, 255, 255,
                       255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 6,
                       8, 10, 12, 29, 30, 31, 32, 33, 34, 255, 255, 255, 255, 255, 255, 255,
                       255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 132, 1};


        error = goodix_i2c_write(ts->client, GOODIX_REG_CONFIG_DATA, config2,186);
        usleep_range(10000, 11000);


	return count;
}

static DEVICE_ATTR(dump_config, S_IRUGO, goodix_dump_config_show, NULL);

static struct attribute *goodix_attrs[] = {
	&dev_attr_dump_config.attr,
	NULL
};

static const struct attribute_group goodix_attr_group = {
	.attrs = goodix_attrs,
};




/**
 * goodix_read_version - Read goodix touchscreen version
 *
 * @ts: our goodix_ts_data pointer
 */
static int goodix_read_version(struct goodix_ts_data *ts)
{
	int error;
	u8 buf[6];
	char id_str[5];

	error = goodix_i2c_read(ts->client, GOODIX_REG_ID, buf, sizeof(buf));
	if (error) {
		dev_err(&ts->client->dev, "read version failed: %d\n", error);
		return error;
	}

	memcpy(id_str, buf, 4);
	id_str[4] = 0;
	if (kstrtou16(id_str, 10, &ts->id))
		ts->id = 0x1001;

	ts->version = get_unaligned_le16(&buf[4]);

	dev_info(&ts->client->dev, "ID %d, version: %04x\n", ts->id,
		 ts->version);

	return 0;
}

/**
 * goodix_i2c_test - I2C test function to check if the device answers.
 *
 * @client: the i2c client
 */
static int goodix_i2c_test(struct i2c_client *client)
{
	int retry = 0;
	int error;
	u8 test;

	while (retry++ < 2) {
		error = goodix_i2c_read(client, GOODIX_REG_CONFIG_DATA,
					&test, 1);
		if (!error)
			return 0;

		dev_err(&client->dev, "i2c test failed attempt %d: %d\n",
			retry, error);
		msleep(20);
	}

	return error;
}

/**
 * goodix_request_input_dev - Allocate, populate and register the input device
 *
 * @ts: our goodix_ts_data pointer
 *
 * Must be called during probe
 */
static int goodix_request_input_dev(struct goodix_ts_data *ts)
{
	int error;

	ts->input_dev = devm_input_allocate_device(&ts->client->dev);
	if (!ts->input_dev) {
		dev_err(&ts->client->dev, "Failed to allocate input device.");
		return -ENOMEM;
	}

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
			     0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
			     0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

	input_mt_init_slots(ts->input_dev, ts->max_touch_num,
			    INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);

	ts->input_dev->name = "Goodix Capacitive TouchScreen";
	ts->input_dev->phys = "input/ts";
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0x0416;
	ts->input_dev->id.product = ts->id;
	ts->input_dev->id.version = ts->version;

	/* Capacitive Windows/Home button on some devices */
	input_set_capability(ts->input_dev, EV_KEY, KEY_LEFTMETA);

	error = input_register_device(ts->input_dev);
	if (error) {
		dev_err(&ts->client->dev,
			"Failed to register input device: %d", error);
		return error;
	}

	return 0;
}

/**
 * goodix_configure_dev - Finish device initialization
 *
 * @ts: our goodix_ts_data pointer
 *
 * Must be called from probe to finish initialization of the device.
 * Contains the common initialization code for both devices that
 * declare gpio pins and devices that do not. It is either called
 * directly from probe or from request_firmware_wait callback.
 */
static int goodix_configure_dev(struct goodix_ts_data *ts)
{
	int error;
	u32 sizeX, sizeY, refreshRate;

	// X2Y setting
	ts->requestedX2Y = device_property_read_bool(&ts->client->dev,
								"touchscreen-x2y");

	if(device_property_read_u32(&ts->client->dev, "touchscreen-refresh-rate", &refreshRate))
	{
		dev_info(&ts->client->dev, "touchscreen-refresh-rate not found");
		ts->requestedRefreshRate = 5;
	}
	else
	{
		dev_info(&ts->client->dev, "touchscreen-refresh-rate found %u", sizeX);
		ts->requestedRefreshRate = (u8)refreshRate;
	}

	if(device_property_read_u32(&ts->client->dev, "touchscreen-size-x", &sizeX))
	{
		dev_info(&ts->client->dev, "touchscreen-size-x not found");
		ts->requestedXSize = 0;
	}
	else
	{
		dev_info(&ts->client->dev, "touchscreen-size-x found %u", sizeX);
		ts->requestedXSize = (u16)sizeX;
	}

	if(device_property_read_u32(&ts->client->dev, "touchscreen-size-y", &sizeY))
	{
		dev_info(&ts->client->dev, "touchscreen-size-y not found");
		ts->requestedYSize = 0;
	}
	else
	{
		dev_info(&ts->client->dev, "touchscreen-size-y found %u", sizeY);
		ts->requestedYSize = (u16)sizeY;
	}

	dev_info(&ts->client->dev, "requested size (%u, %u)", ts->requestedXSize, ts->requestedYSize);

	ts->swapped_x_y = device_property_read_bool(&ts->client->dev,
						    "touchscreen-swapped-x-y");
	ts->inverted_x = device_property_read_bool(&ts->client->dev,
						   "touchscreen-inverted-x");
	ts->inverted_y = device_property_read_bool(&ts->client->dev,
						   "touchscreen-inverted-y");

	goodix_read_config(ts);

	error = goodix_request_input_dev(ts);
	if (error)
		return error;

	ts->irq_flags = goodix_irq_flags[ts->int_trigger_type] | IRQF_ONESHOT;
	error = goodix_request_irq(ts);
	if (error) {
		dev_err(&ts->client->dev, "request IRQ failed: %d\n", error);
		return error;
	}

	return 0;
}

/**
 * goodix_config_cb - Callback to finish device init
 *
 * @ts: our goodix_ts_data pointer
 *
 * request_firmware_wait callback that finishes
 * initialization of the device.
 */
static void goodix_config_cb(const struct firmware *cfg, void *ctx)
{
	struct goodix_ts_data *ts = ctx;
	int error;

	if (cfg) {
		/* send device configuration to the firmware */
		error = goodix_send_cfg(ts, cfg);
		if (error)
			goto err_release_cfg;
	}

	goodix_configure_dev(ts);

err_release_cfg:
	release_firmware(cfg);
	complete_all(&ts->firmware_loading_complete);
}

static int goodix_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct goodix_ts_data *ts;
	int error;

	dev_info(&client->dev, "I2C Address: 0x%02x\n", client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality failed.\n");
		return -ENXIO;
	}

	ts = devm_kzalloc(&client->dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->client = client;
	i2c_set_clientdata(client, ts);
	init_completion(&ts->firmware_loading_complete);
  
  error = goodix_i2c_test(client);
	if (error) {
		dev_err(&client->dev, "I2C communication failure: %d\n", error);
		return error;
	}



	error = goodix_read_version(ts);
	if (error) {
		dev_err(&client->dev, "Read version failed.\n");
		return error;
	}

	error = goodix_get_gpio_config(ts);
	if (error)   {
    gpiod_put(ts->gpiod_int);
    gpiod_put(ts->gpiod_rst);
    goodix_free_irq(ts);
		return error;
                }

	if (ts->gpiod_int /*&& ts->gpiod_rst*/) {


           error = sysfs_create_group(&client->dev.kobj,  &goodix_attr_group);
		if (error) {
			dev_err(&client->dev,
				"Failed to create sysfs group: %d\n",
				error);
			return error;
		}		/* reset the controller */
		if (ts->gpiod_rst) {error = goodix_reset(ts);
		if (error) {
			dev_err(&client->dev, "Controller reset failed.\n");
			return error;
		}}
	}




	ts->cfg_len = goodix_get_cfg_len(ts->id);

	if (ts->gpiod_int && ts->gpiod_rst) {
		/* update device config */
		ts->cfg_name = devm_kasprintf(&client->dev, GFP_KERNEL,
					      "goodix_%d_cfg.bin", ts->id);
		if (!ts->cfg_name)
			{return -ENOMEM;
                        goto err_sysfs_remove_group;}


		error = request_firmware_nowait(THIS_MODULE, true, ts->cfg_name,
						&client->dev, GFP_KERNEL, ts,
						goodix_config_cb);
		if (error) {
			dev_err(&client->dev,
				"Failed to invoke firmware loader: %d\n",
				error);
			return error;
                        goto err_sysfs_remove_group;
		}

		return 0;
	} else {
		error = goodix_configure_dev(ts);
		if (error)
			return error;
	}

	return 0;

err_sysfs_remove_group:
        sysfs_remove_group(&client->dev.kobj, &goodix_attr_group);
	return error;


}

static int goodix_ts_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->gpiod_int && ts->gpiod_rst)
		wait_for_completion(&ts->firmware_loading_complete);
    
  goodix_free_irq(ts);
  gpiod_put(ts->gpiod_int);
  gpiod_put(ts->gpiod_rst);
  sysfs_remove_group(&client->dev.kobj, &goodix_attr_group);
	return 0;
}

static int __maybe_unused goodix_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	int error;

	/* We need gpio pins to suspend/resume */
	if (!ts->gpiod_int || !ts->gpiod_rst) {
		disable_irq(client->irq);
		return 0;
	}

	wait_for_completion(&ts->firmware_loading_complete);

	/* Free IRQ as IRQ pin is used as output in the suspend sequence */
	goodix_free_irq(ts);

	/* Output LOW on the INT pin for 5 ms */
	error = gpiod_direction_output(ts->gpiod_int, 0);
	if (error) {
		goodix_request_irq(ts);
		return error;
	}

	usleep_range(5000, 6000);

	error = goodix_i2c_write_u8(ts->client, GOODIX_REG_COMMAND,
				    GOODIX_CMD_SCREEN_OFF);
	if (error) {
		dev_err(&ts->client->dev, "Screen off command failed\n");
		gpiod_direction_input(ts->gpiod_int);
		goodix_request_irq(ts);
		return -EAGAIN;
	}

	/*
	 * The datasheet specifies that the interval between sending screen-off
	 * command and wake-up should be longer than 58 ms. To avoid waking up
	 * sooner, delay 58ms here.
	 */
	msleep(58);
	return 0;
}

static int __maybe_unused goodix_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	int error;

	if (!ts->gpiod_int || !ts->gpiod_rst) {
		enable_irq(client->irq);
		return 0;
	}

	/*
	 * Exit sleep mode by outputting HIGH level to INT pin
	 * for 2ms~5ms.
	 */
	error = gpiod_direction_output(ts->gpiod_int, 1);
	if (error)
		return error;

	usleep_range(2000, 5000);

	error = goodix_int_sync(ts);
	if (error)
		return error;

	error = goodix_request_irq(ts);
	if (error)
		return error;

	return 0;
}



static int checkFirmware(struct goodix_ts_data *ts, u8 *config)
{
	int result = 0;
        int rawCfgLlen = GOODIX_CONFIG_911_LENGTH;
        int i;

        for (i = 0; i < rawCfgLlen; i++) {
                        dev_info(&ts->client->dev, "config: %u", config[i]);
                       }


	bool bUpdate = false;
	bool bNewX2Y = ts->requestedX2Y;
	bool bCurrentX2Y = CHECK_BIT(config[0x804d - GOODIX_REG_CONFIG_DATA], 3);
	u8 currentRefreshRate = config[0x8056 - GOODIX_REG_CONFIG_DATA];


        dev_info(&ts->client->dev, "Goodix X Inversion: %d \n",CHECK_BIT(config[0x804d - GOODIX_REG_CONFIG_DATA], 6));
        dev_info(&ts->client->dev, "Goodix Y Inversion: %d \n",CHECK_BIT(config[0x804d - GOODIX_REG_CONFIG_DATA], 7));





	dev_info(&ts->client->dev, "Checking firmware");
	if(ts->requestedRefreshRate != currentRefreshRate)
	{
		dev_info(&ts->client->dev, "Requested refresh rate = %u, orig = %u", ts->requestedRefreshRate, currentRefreshRate);
		config[0x8056 - GOODIX_REG_CONFIG_DATA] =  (config[0x8056 - GOODIX_REG_CONFIG_DATA] & 0xF0) +  (ts->requestedRefreshRate & 0x0F);
		bUpdate = true;
	}

	if(ts->requestedXSize)
	{
		u16 xOrig;

		xOrig = get_unaligned_le16(&config[RESOLUTION_LOC_X]);
		dev_info(&ts->client->dev, "Requested X size = %u, orig = %u", ts->requestedXSize, xOrig);
		if(ts->requestedXSize != xOrig)
		{
			dev_info(&ts->client->dev, "Requested X size needs firmware update");
			put_unaligned_le16(ts->requestedXSize, &config[RESOLUTION_LOC_X]);
			bUpdate = true;
		}
	}

	if(ts->requestedYSize)
	{
		u16 yOrig;

		yOrig = get_unaligned_le16(&config[RESOLUTION_LOC_Y]);
		dev_info(&ts->client->dev, "Requested Y size = %u, orig = %u", ts->requestedYSize, yOrig);
		if(ts->requestedYSize != yOrig)
		{
			dev_info(&ts->client->dev, "Requested Y size needs firmware update");
			put_unaligned_le16(ts->requestedYSize, &config[RESOLUTION_LOC_Y]);
			bUpdate = true;
		}
	}

	if(bNewX2Y != bCurrentX2Y)
	{
		dev_info(&ts->client->dev, "Requested X2Y size needs firmware update");

		if(bNewX2Y)
			config[0x804d - GOODIX_REG_CONFIG_DATA] |= 0x08;
		else
			config[0x804d - GOODIX_REG_CONFIG_DATA] &= 0xf7;

		bUpdate = true;
	}


	if (CHECK_BIT(config[0x804d - GOODIX_REG_CONFIG_DATA], 6) || 
		CHECK_BIT(config[0x804d - GOODIX_REG_CONFIG_DATA], 7)) 
	{
		dev_info(&ts->client->dev, "Changing X & Y Inversion to normal.");
		config[0x804d - GOODIX_REG_CONFIG_DATA] &= 0x3F;
		bUpdate = true;


	}



	if(bUpdate)
	{
		int error;
		struct firmware fw;
		int check;
		u8 checkSum = 0;

		dev_info(&ts->client->dev, "Updateing firmware");


		fw.data = config;
		fw.size = GOODIX_CONFIG_911_LENGTH;
		config[GOODIX_CONFIG_911_LENGTH-1]=1;	// flash fw flag

		// calculate checksum
		rawCfgLlen = GOODIX_CONFIG_911_LENGTH- 2;
		for (i = 0; i < rawCfgLlen; i++)
			checkSum += config[i];
		checkSum = (~checkSum) + 1;
		config[rawCfgLlen] = checkSum;

		// check firmware is ok
		check =  goodix_check_cfg(ts,  &fw);

		if(!check)
		{
			error = goodix_i2c_write(ts->client, GOODIX_REG_CONFIG_DATA, fw.data, fw.size);
			if (error)
				dev_err(&ts->client->dev, "Failed to write config data: (%d)",	error);
			else
				dev_info(&ts->client->dev, "Updated firmware correctly.");


			/* Let the firmware reconfigure itself, so sleep for 10ms */
			usleep_range(10000, 11000);
		}
		else
		{
			dev_err(&ts->client->dev, "Firmware data is invalid.");
			result = 1;
		}
	}
	else
		dev_info(&ts->client->dev, "Firmware already updated with correct values");

	return result;
}


static SIMPLE_DEV_PM_OPS(goodix_pm_ops, goodix_suspend, goodix_resume);

static const struct i2c_device_id goodix_ts_id[] = {
	{ "GDIX1001:00", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, goodix_ts_id);

#ifdef CONFIG_ACPI
static const struct acpi_device_id goodix_acpi_match[] = {
	{ "GDIX1001", 0 },
	{ "GDIX1002", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, goodix_acpi_match);
#endif

#ifdef CONFIG_OF
static const struct of_device_id goodix_of_match[] = {
	{ .compatible = "goodix,gt911" },
	{ .compatible = "goodix,gt9110" },
	{ .compatible = "goodix,gt912" },
	{ .compatible = "goodix,gt927" },
	{ .compatible = "goodix,gt9271" },
	{ .compatible = "goodix,gt928" },
	{ .compatible = "goodix,gt9286" },
	{ .compatible = "goodix,gt967" },
	{ }
};
MODULE_DEVICE_TABLE(of, goodix_of_match);
#endif

static struct i2c_driver goodix_ts_driver = {
	.probe = goodix_ts_probe,
	.remove = goodix_ts_remove,
	.id_table = goodix_ts_id,
	.driver = {
		.name = "Goodix-TS",
		.acpi_match_table = ACPI_PTR(goodix_acpi_match),
		.of_match_table = of_match_ptr(goodix_of_match),
		.pm = &goodix_pm_ops,
	},
};
module_i2c_driver(goodix_ts_driver);

MODULE_AUTHOR("Benjamin Tissoires <benjamin.tissoires@gmail.com>");
MODULE_AUTHOR("Bastien Nocera <hadess@hadess.net>");
MODULE_DESCRIPTION("Goodix touchscreen driver");
MODULE_LICENSE("GPL v2");


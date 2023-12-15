// SPDX-License-Identifier: GPL-2.0-or-later
/* Asair AHT10 / AHT15 humidity and temperature sensor driver
 *
 * Copyright (C) 2020 Lutz Harder, SHPI GmbH
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/delay.h>



/* I2C command bytes */

#define AHT10_SOFT_RESET_CMD	0xBA  //soft reset command

#define AHT10_INIT_CMD		0xE1  //initialization command for AHT10/AHT15
#define AHT10_INIT_CMD_1	0x08  //set factory calibration bit
#define AHT10_INIT_CMD_2	0x00  //function unknown

#define AHT10_MEASURE_CMD	0xAC  //start temperature & humidity, measuring
#define AHT10_MEASURE_ADC_CMD	0x33  //ADC SETTING 0x00 slow measurement
				      //.. 0x33 factory standard ..
				      //0xFF quickest measurement, but faulty
#define AHT10_MEASURE_2_CMD	0x00  //function unknown


/* AHT10 delays */

#define AHT10_MEASURE_DELAY	120000    //80 milliseconds
#define AHT10_RESET_DELAY	40000    //40 milliseconds
#define AHT10_INIT_DELAY	10000    //from latest datasheet

/* AHT10 control bit */

#define AHT10_CONTROL_BUSY_SHIFT   7	// Bit 7 of control bit indicates business
#define AHT10_CONTROL_BUSY_MASK	   0x1  // 1 Bit
#define AHT10_CONTROL_CAL_SHIFT	   3	// Bit 3 indicates calibration status
#define AHT10_CONTROL_CAL_MASK     0x1	// 1 Bit
#define AHT10_CONTROL_MODE_SHIFT   5	// Bit 5&6 indicates acutal working mode
#define AHT10_CONTROL_MODE_MASK    0x3  // 2 Bits


/**
 * struct aht10 - AHT10 device specific data
 * @client: I2C client device
 * @lock: mutex to protect measurement values
 * @last_update: time of last update (jiffies)
 * @temperature: cached temperature measurement value
 * @humidity: cached humidity measurement value
 * @valid: only 0 before first measurement is taken
 */

struct aht10 {
	struct i2c_client *client;
	struct mutex lock;
	unsigned long last_update;
	int temperature;
	int humidity;
	char valid;
};




/**
 * aht10_temp_raw_to_millicelsius() - convert raw temperature ticks to
 * milli celsius
 * @raw: temperature raw data received from sensor
 */
static inline int aht10_temp_raw_to_millicelsius(int raw)
{
	/*
         * Temperature = ((200.0 * (float) raw_temperature) / 1048576.0) - 50.0;
	 *  converted for integer fixed point arithmetic
	 */

	return (int)(((200000 * (uint64_t)raw) >> 20) - 50000);

}

/**
 * aht10_rh_raw_to_per_cent_mille() - convert raw humidity ticks to
 * one-thousandths of a percent relative humidity
 * @raw: humidity raw received from sensor
 */
static inline int aht10_rh_raw_to_per_cent_mille(int raw)
{
	/*
	 * humidity = (float) raw_humidity * 100.0 / 1048576.0;
	 * optimized for integer fixed point (3 digits) arithmetic
	 */
	return (int)(((100000 * (uint64_t)raw) >> 20));
}

/**
 * aht10_update_measurements() - get updated measurements from device
 * @dev: device
 *
 * Returns 0 on success, else negative errno.
 */


static int aht10_calibrate(struct i2c_client *client)
{
	int ret = 0;

	unsigned char wbuf[3] = {0,};

	wbuf[0] = AHT10_INIT_CMD;
	wbuf[1] = AHT10_INIT_CMD_1;
	wbuf[2] = AHT10_INIT_CMD_2;


        ret = i2c_master_send(client, wbuf, 3);

	if (ret <= 0)
		return -EIO;

        usleep_range(AHT10_INIT_DELAY, AHT10_INIT_DELAY + 1000);
	dev_err(&client->dev, "AHT10 calibration loaded.\n");


        return ret >= 0 ? 0 : ret;

}



static int aht10_soft_reset(struct i2c_client *client)
{
        int ret = 0;
	char cmd;
        cmd = AHT10_SOFT_RESET_CMD;

        ret = i2c_master_send(client, &cmd, 1);
        if (ret <= 0)
                return -EIO;

        usleep_range(AHT10_RESET_DELAY, AHT10_RESET_DELAY + 1000);
        dev_err(&client->dev, "AHT10 reset.\n");


        return ret >= 0 ? 0 : ret;

}


static int aht10_update_measurements(struct device *dev)
{
	int ret = 0;
	struct aht10 *aht10 = dev_get_drvdata(dev);
	struct i2c_client *client = aht10->client;
	unsigned char wbuf[3];
	unsigned char rbuf[6];

	mutex_lock(&aht10->lock);
	  /*
	 * recommended 1 measurements 8 per seconds.
	 */
	if (time_after(jiffies, aht10->last_update + HZ*8) || !aht10->valid) {

	        wbuf[0] = AHT10_MEASURE_CMD;
		wbuf[1] = AHT10_MEASURE_ADC_CMD;
		wbuf[2] = AHT10_MEASURE_2_CMD;
	        ret = i2c_master_send(client, wbuf, 3);

		if (ret <= 0)
			goto out;

		usleep_range(AHT10_MEASURE_DELAY, AHT10_MEASURE_DELAY + 1000);

		ret = i2c_master_recv(client, &(rbuf[0]), 6);
		//ret = i2c_smbus_read_i2c_block_data(client, 0x00, 6, &(rbuf[0]));
		if (ret <= 0) {
			dev_err(&client->dev, "i2c_master_recv failed\n");
                        aht10_soft_reset(client);
			goto out;
		}



                if (((rbuf[0] >> AHT10_CONTROL_CAL_SHIFT) & AHT10_CONTROL_CAL_MASK) == 0)
		{
				aht10_calibrate(client);
				aht10->valid = 0;
				dev_err(&client->dev, "sensor not calibrated, calibrating...\n");
				goto out;

		}


                if (((rbuf[0] >> AHT10_CONTROL_BUSY_SHIFT) & AHT10_CONTROL_BUSY_MASK) == 0)
		{
				if (!(rbuf[4] == 0x00 && rbuf[5] ==  0x00)) // catch temp errors
				{
					aht10->temperature = aht10_temp_raw_to_millicelsius
					((((rbuf[3] & 0x0F) << 16) | (rbuf[4] << 8) | rbuf[5]));
				}
				else
				{
					dev_err(&client->dev, "temperature reading error, ignoring...\n");
				}

				if (!(rbuf[1] == 0x00 && rbuf[2] ==  0x00)) // catch humidity errors
					aht10->humidity = aht10_rh_raw_to_per_cent_mille
					((((rbuf[1] << 16) | (rbuf[2] << 8) | rbuf[3]) >> 4));
				else
				{
					dev_err(&client->dev, "humidity reading error, ignoring...\n");
				}
				aht10->last_update = jiffies;
				aht10->valid = 1;
		} else
		{
			dev_err(&client->dev, "sensor busy, consider increasing measurement delay\n");
                        aht10_soft_reset(client);
		}
	} //else  dev_err(&client->dev, "max 1 measurement per 8 seconds, using cache\n");

out:
	mutex_unlock(&aht10->lock);
	return ret >= 0 ? 0 : ret;
}

/**
 * aht10_show_temperature() - show temperature measurement value in sysfs
 * @dev: device
 * @attr: device attribute
 * @buf: sysfs buffer (PAGE_SIZE) where measurement values are written to
 *
 * Will be called on read access to temp1_input sysfs attribute.
 * Returns number of bytes written into buffer, negative errno on error.
 */

static ssize_t aht10_temperature_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct aht10 *aht10 = dev_get_drvdata(dev);
	int ret;

	ret = aht10_update_measurements(dev);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", aht10->temperature);
}

/**
 * aht10_show_humidity() - show humidity measurement value in sysfs
 * @dev: device
 * @attr: device attribute
 * @buf: sysfs buffer (PAGE_SIZE) where measurement values are written to
 *
 * Will be called on read access to humidity1_input sysfs attribute.
 * Returns number of bytes written into buffer, negative errno on error.
 */
static ssize_t aht10_humidity_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aht10 *aht10 = dev_get_drvdata(dev);
	int ret;

	ret = aht10_update_measurements(dev);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", aht10->humidity);
}



/* sysfs attributes */
static SENSOR_DEVICE_ATTR_RO(temp1_input, aht10_temperature, 0);
static SENSOR_DEVICE_ATTR_RO(humidity1_input, aht10_humidity, 0);



static struct attribute *aht10_attrs[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_humidity1_input.dev_attr.attr,
	NULL
};

ATTRIBUTE_GROUPS(aht10);


static int aht10_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device *hwmon_dev;
	struct aht10 *aht10;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter,
				      I2C_FUNC_I2C)) {
		dev_err(&client->dev,
			"adapter does not support I2C transactions\n");
		return -ENODEV;
	}

	aht10 = devm_kzalloc(dev, sizeof(*aht10), GFP_KERNEL);
	if (!aht10)
		return -ENOMEM;

	aht10->client = client;


	mutex_init(&aht10->lock);


	hwmon_dev = devm_hwmon_device_register_with_groups(dev, client->name,
							   aht10, aht10_groups);

        aht10_soft_reset(client);
        aht10_calibrate(client);

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

/* Device ID table */
static const struct i2c_device_id aht10_id[] = {
	{ "aht10", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aht10_id);

static struct i2c_driver aht10_driver = {
	.driver.name = "aht10",
	.probe       = aht10_probe,
	.id_table    = aht10_id,
};

module_i2c_driver(aht10_driver);

MODULE_AUTHOR("Lutz Harder <lh@shpi.de>");
MODULE_DESCRIPTION("ASAIR AHT10/15 humidity and temperature sensor driver");
MODULE_LICENSE("GPL");

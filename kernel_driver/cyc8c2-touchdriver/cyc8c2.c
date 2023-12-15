/*  Driver for cyc8c2  Touchscreen  */

//#define DEBUG

/* DTparameters:
 *        x-invert     = <&cyc8c2>, "touchscreen-inverted-x";
 *        y-invert     = <&cyc8c2>, "touchscreen-inverted-y";
 *        xy-swap      = <&cyc8c2>, "touchscreen-swapped-x-y:0";
 *        x-size       = <&cyc8c2>, "touchscreen-size-x:800";
 *        y-size       = <&cyc8c2>, "touchscreen-size-y:480";
 *        x2y          = <&cyc8c2>, "touchscreen-x2y:0";
 */

#include <asm/io.h>
#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>

// Define necessary constants for GPIO ALT of raspberry
#define BCM2708_PERI_BASE 0x20000000
#define BCM2835_GPIO_BASE (BCM2708_PERI_BASE + 0x200000) /* GPIO */
#define GPIO_FSEL0_OFFSET 0x00                           // FSEL0 offset
#define GPIO_ALT2 0b110                                  // ALT2 function
#define GPIO_PIN_2_SHIFT 6 // Shift for GPIO pin 2 in FSEL0
#define GPIO_PIN_8_SHIFT 24

#define CYC8C2_MAX_WIDTH 800
#define CYC8C2_MAX_HEIGHT 480
#define CYC8C2_POWER 0x70
#define CYC8C2_NUM_TOUCHES 0x6D
#define CYC8C2_INT_SETTINGS 0x6E
#define CYC8C2_INT_WIDTH 0x6F
#define CYC8C2_Y_SENSIVITY 0x67
#define CYC8C2_X_SENSIVITY 0x68
#define CYC8C2_COORDS 0x40

struct cyc8c2_ts_data {
  struct i2c_client *client;
  struct input_dev *input_dev;
  struct gpio_desc *gpiod_int;
  struct task_struct *thread;
  u16 id;
  u16 version;
  int last_x1;
  int last_y1;
  int last_x2;
  int last_y2;
  int last_numTouches;
  bool X2Y;
  u32 abs_x_max;
  u32 abs_y_max;
  bool swapped_x_y;
  bool inverted_x;
  bool inverted_y;
  unsigned long irq_flags;
};

static int cyc8c2_i2c_read(struct i2c_client *client, u16 reg, u8 *buf,
                           int len) {
  int bytes_read;
  bytes_read = i2c_smbus_read_i2c_block_data(client, reg, len, buf);
  return (bytes_read != len ? -EIO : 0);
}

static int cyc8c2_i2c_write(struct i2c_client *client, u16 reg, const u8 *buf,
                            unsigned len) {
  int bytes_written;
  bytes_written = i2c_smbus_write_i2c_block_data(client, reg, len, buf);
  return (bytes_written != len ? -EIO : 0);
}

static void cyc8c2_ts_translate_coords(struct cyc8c2_ts_data *ts, u16 *x,
                                       u16 *y) {

  if (!ts->X2Y && ts->swapped_x_y)
    swap(*x, *y);
  if (ts->inverted_x)
    *x = ts->abs_x_max - *x;
  if (ts->inverted_y)
    *y = ts->abs_y_max - *y;
  if (ts->X2Y && ts->swapped_x_y)
    swap(*x, *y);
}

static void cyc8c2_ts_process_coords(struct cyc8c2_ts_data *ts, u16 x1, u16 y1,
                                     u16 x2, u16 y2, u8 numTouches) {
  int output;
  cyc8c2_ts_translate_coords(ts, &x1, &y1);
  cyc8c2_ts_translate_coords(ts, &x2, &y2);

  output = 0;
  if (numTouches) {
    if (!ts->last_numTouches) {
      /* new press */

      output |= 1;
      input_mt_slot(ts->input_dev, 0);
      input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
      input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
      input_report_key(ts->input_dev, BTN_TOUCH, 1);

      if (y1 < CYC8C2_MAX_HEIGHT && y1 > 0) {
        input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y1);
        input_report_abs(ts->input_dev, ABS_Y, y1);
      } else
        dev_dbg(&ts->client->dev, "y1 out of range: (%d)", y1);

      if (x1 < CYC8C2_MAX_WIDTH && x1 > 0) {
        input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x1);
        input_report_abs(ts->input_dev, ABS_X, x1);
      } else
        dev_dbg(&ts->client->dev, "x1 out of range: (%d)", x1);

      dev_dbg(&ts->client->dev, "Press 1: (%d,%d)", x1, y1);
    } else {

      output |= 1;
      input_mt_slot(ts->input_dev, 0);

      input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
      input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);

      if (x1 < CYC8C2_MAX_WIDTH && x1 > 0) {
        input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x1);
        input_report_abs(ts->input_dev, ABS_X, x1);
      } else
        dev_dbg(&ts->client->dev, "x1 out of range: (%d)", x1);

      if (y1 < CYC8C2_MAX_HEIGHT && y1 > 0) {
        input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y1);
        input_report_abs(ts->input_dev, ABS_Y, y1);
      } else
        dev_dbg(&ts->client->dev, "y1 out of range: (%d)", y1);

      dev_dbg(&ts->client->dev, "Move 1: (%d,%d)", x1, y1);
    }
  } else if (ts->last_numTouches) {
    /* new release */
    output |= 1;
    input_mt_slot(ts->input_dev, 0);

    input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
    input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
    input_report_key(ts->input_dev, BTN_TOUCH, 0);
    dev_dbg(&ts->client->dev, "Release 1");
  }

  if (numTouches == 2) {
    if (ts->last_numTouches < 2) {
      /* new 2. press */
      output |= 2;
      input_mt_slot(ts->input_dev, 1);

      input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 1);
      input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
      input_report_key(ts->input_dev, BTN_TOUCH, 1);

      if (x2 < CYC8C2_MAX_WIDTH && x2 > 0) {
        input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x2);
        input_report_abs(ts->input_dev, ABS_X, x2);
      } else
        dev_dbg(&ts->client->dev, "x2 out of range: (%d)", x2);

      if (y2 < CYC8C2_MAX_HEIGHT && y2 > 0) {
        input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y2);
        input_report_abs(ts->input_dev, ABS_Y, y2);
      } else
        dev_dbg(&ts->client->dev, "y2 out of range: (%d)", y2);

      dev_dbg(&ts->client->dev, "Press 2: (%d,%d)", x2, y2);
    } else {
      output |= 2;
      input_mt_slot(ts->input_dev, 1);

      input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 1);
      input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);

      if (x2 < CYC8C2_MAX_WIDTH && x2 > 0) {
        input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x2);
        input_report_abs(ts->input_dev, ABS_X, x2);
      } else
        dev_dbg(&ts->client->dev, "x2 out of range: (%d)", x2);

      if (y2 < CYC8C2_MAX_HEIGHT && y2 > 0) {
        input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y2);
        input_report_abs(ts->input_dev, ABS_Y, y2);
      } else
        dev_dbg(&ts->client->dev, "y2 out of range: (%d)", y2);

      dev_dbg(&ts->client->dev, "Move 2: (%d,%d)", x2, y2);
    }
  } else if (ts->last_numTouches == 2) {
    /* new 2. release */
    output |= 2;
    input_mt_slot(ts->input_dev, 1);

    input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
    input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
    input_report_key(ts->input_dev, BTN_TOUCH, 0);
    dev_dbg(&ts->client->dev, "Release 2");
  }

  if (output)
    input_sync(ts->input_dev);

  ts->last_x1 = x1;
  ts->last_y1 = y1;
  ts->last_x2 = x2;
  ts->last_y2 = y2;
  ts->last_numTouches = numTouches;
}

static void cyc8c2_process_events(struct cyc8c2_ts_data *ts) {

  u16 x1, y1, x2, y2;
  u8 numTouches;
  cyc8c2_i2c_read(ts->client, CYC8C2_NUM_TOUCHES, &numTouches, 1);

  if (numTouches == 2) {
    u8 rawdata[8];

    cyc8c2_i2c_read(ts->client, CYC8C2_COORDS, rawdata, 8);
    x1 = rawdata[0] | (rawdata[4] << 8);
    y1 = rawdata[1] | (rawdata[5] << 8);
    x2 = rawdata[2] | (rawdata[6] << 8);
    y2 = rawdata[3] | (rawdata[7] << 8);
  } else if (numTouches == 1) {
    u8 rawdata[6];
    cyc8c2_i2c_read(ts->client, CYC8C2_COORDS, rawdata, 6);
    x1 = rawdata[0] | (rawdata[4] << 8);
    y1 = rawdata[1] | (rawdata[5] << 8);
    x2 = 0;
    y2 = 0;
  } else {
    x1 = 0;
    y1 = 0;
    x2 = 0;
    y2 = 0;
  }
  cyc8c2_ts_process_coords(ts, x1, y1, x2, y2, numTouches);
}

static irqreturn_t cyc8c2_ts_irq_handler(int irq, void *dev_id) {
  struct cyc8c2_ts_data *ts = dev_id;
  cyc8c2_process_events(ts);
  return IRQ_HANDLED;
}

static void cyc8c2_free_irq(struct cyc8c2_ts_data *ts) {
  devm_free_irq(&ts->client->dev, ts->client->irq, ts);
}

static int cyc8c2_request_irq(struct cyc8c2_ts_data *ts) {
  return devm_request_threaded_irq(&ts->client->dev, ts->client->irq, NULL,
                                   cyc8c2_ts_irq_handler, ts->irq_flags,
                                   ts->client->name, ts);
}

static void cyc8c2_ts_remove(struct i2c_client *client) {
  struct cyc8c2_ts_data *ts = i2c_get_clientdata(client);

  if (ts->gpiod_int)
    devm_gpiod_put(&client->dev, ts->gpiod_int);

  cyc8c2_free_irq(ts);
}

static int cyc8c2_get_gpio_config(struct cyc8c2_ts_data *ts) {
  int error;
  struct device *dev;
  struct gpio_desc *gpiod;
  u32 sizeX, sizeY;

  if (!ts->client)
    return -EINVAL;
  dev = &ts->client->dev;

  /* Get the interrupt GPIO pin description */
  gpiod = devm_gpiod_get_optional(dev, "irq", GPIOD_IN);
  if (IS_ERR(gpiod)) {
    error = PTR_ERR(gpiod);
    if (error != -EPROBE_DEFER)
      dev_dbg(dev, "Failed to get %s GPIO: %d\n", "irq", error);
    return error;
  }

  ts->gpiod_int = gpiod;
  gpiod_direction_input(ts->gpiod_int);

  ts->last_numTouches = 0;
  ts->abs_x_max = CYC8C2_MAX_WIDTH;
  ts->abs_y_max = CYC8C2_MAX_HEIGHT;

  // Read DT configuration parameters
  ts->X2Y = device_property_read_bool(&ts->client->dev, "touchscreen-x2y");
  dev_dbg(&ts->client->dev, "touchscreen-x2y %u", ts->X2Y);

  if (device_property_read_u32(&ts->client->dev, "touchscreen-size-x", &sizeX))
    dev_dbg(&ts->client->dev,
            "touchscreen-size-x not found. Using default of %u", ts->abs_x_max);

  else {
    ts->abs_x_max = (u16)sizeX;
    dev_dbg(&ts->client->dev, "touchscreen-size-x found %u", ts->abs_x_max);
  }

  if (device_property_read_u32(&ts->client->dev, "touchscreen-size-y", &sizeY))
    dev_dbg(&ts->client->dev,
            "touchscreen-size-y not found. Using default of %u", ts->abs_y_max);
  else {
    ts->abs_y_max = (u16)sizeY;
    dev_dbg(&ts->client->dev, "touchscreen-size-y found %u", ts->abs_y_max);
  }

  dev_dbg(&ts->client->dev, "requested size (%u, %u)", ts->abs_x_max,
          ts->abs_y_max);

  ts->swapped_x_y =
      device_property_read_bool(&ts->client->dev, "touchscreen-swapped-x-y");
  dev_dbg(&ts->client->dev, "touchscreen-swapped-x-y %u", ts->swapped_x_y);

  ts->inverted_x =
      device_property_read_bool(&ts->client->dev, "touchscreen-inverted-x");
  dev_dbg(&ts->client->dev, "touchscreen-inverted-x %u", ts->inverted_x);

  ts->inverted_y =
      device_property_read_bool(&ts->client->dev, "touchscreen-inverted-y");
  dev_dbg(&ts->client->dev, "touchscreen-inverted-y %u", ts->inverted_y);

  return 0;
}

static int cyc8c2_request_input_dev(struct cyc8c2_ts_data *ts) {
  int error;

  ts->input_dev = devm_input_allocate_device(&ts->client->dev);
  if (!ts->input_dev) {
    dev_err(&ts->client->dev, "Failed to allocate touchscreen device.");
    return -ENOMEM;
  }

  /* Set up device parameters */
  input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0,
                       0);
  input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0,
                       0);
  input_mt_init_slots(ts->input_dev, 2, INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);

  ts->input_dev->name = "cyc8c2 touchscreen";
  ts->input_dev->phys = "input/ts";
  ts->input_dev->id.bustype = BUS_I2C;
  ts->input_dev->id.vendor = 0x0001;
  ts->input_dev->id.product = 0x0001;
  ts->input_dev->id.version = 0x0001;

  error = input_register_device(ts->input_dev);
  if (error) {
    dev_err(&ts->client->dev, "Failed to register touchscreen device: %d",
            error);
    return error;
  }

  return 0;
}

static int cyc8c2_ts_probe(struct i2c_client *client,
                           const struct i2c_device_id *id) {

  struct cyc8c2_ts_data *ts;
  struct gpio_desc *gpiock, *gpiomosi, *gpiocs;
  int ret, error, x;
  u8 buf;
  uint8_t count;
  uint16_t a035vw01[] = {
      0x0080, 0x01F8, 0x0246, 0x0305, 0x0440, 0x0540,
      0x0640, 0x0740, 0x0840, 0x0940, 0x0A03}; // only for A035VW01

  int32_t a035vl01[] = {
      0x0001, -1,     0x00c1, 0x01a8, 0x01b1, 0x0145,
      0x0104, 0x00b1, 0x0106, 0x0146, 0x0105, 0x00c5, 0x0180, 0x016c, 0x00c6,
      0x01bd, 0x0184, 0x00c7, 0x01bd, 0x0184, 0x00bd, 0x0102, 0x0011, -1,
      0x0100, 0x0100, 0x0182, 0x0026, 0x0108, 0x00e0, 0x0100, 0x0104, 0x0108,
      0x010b, 0x010c, 0x010d, 0x010e, 0x0100, 0x0104, 0x0108, 0x0113, 0x0114,
      0x012f, 0x0129, 0x0124, 0x00e1, 0x0100, 0x0104, 0x0108, 0x010b, 0x010c,
      0x0111, 0x010d, 0x010e, 0x0100, 0x0104, 0x0108, 0x0113, 0x0114, 0x012f,
      0x0129, 0x0124, 0x0026, 0x0108, 0x00fd, 0x0100, 0x0108, 0x0029};

  void __iomem *gpio_base;
  uint32_t *fsel_reg, val;

  dev_err(&client->dev, "CYC8C TOUCH DRIVER LOADING.\n");

  ts = devm_kzalloc(&client->dev, sizeof(*ts), GFP_KERNEL);
  if (!ts)
    return -ENOMEM;

  ts->client = client;
  i2c_set_clientdata(client, ts);

  ret = cyc8c2_get_gpio_config(ts);
  if (ret)
    return ret;

  dev_dbg(&client->dev, "I2C address: 0x%02x\n", client->addr);

  if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
    dev_err(&client->dev, "I2C check failed.\n");
    return -ENXIO;
  }

  
   /*
  gpiock =
      gpiod_get(&client->dev, "clk", GPIOD_ASIS | GPIOD_FLAGS_BIT_NONEXCLUSIVE);
  if (IS_ERR(gpiock)) {
    dev_err(&client->dev, "Failed to get clk GPIO\n");
    // return PTR_ERR(gpiock);
  }
  else {
  gpiomosi = gpiod_get(&client->dev, "mosi",
                       GPIOD_ASIS | GPIOD_FLAGS_BIT_NONEXCLUSIVE);
  if (IS_ERR(gpiomosi)) {
    dev_err(&client->dev, "Failed to get mosi GPIO\n");
    // return PTR_ERR(gpiomosi);
  }
  else {
  gpiocs =
      gpiod_get(&client->dev, "cs", GPIOD_ASIS | GPIOD_FLAGS_BIT_NONEXCLUSIVE);
  if (IS_ERR(gpiocs)) {
    dev_err(&client->dev, "Failed to get cs GPIO\n");
    // return PTR_ERR(gpiocs);
  } else {

 

  if (!IS_ERR(gpiock) && !IS_ERR(gpiomosi) && !IS_ERR(gpiocs))

  {
    dev_err(&client->dev, "Display init running.\n");

    gpiod_direction_output(gpiocs, 1);
    gpiod_direction_output(gpiock, 1);
    gpiod_direction_output(gpiomosi, 1);

    for (x = 0; x < sizeof(a035vw01) / sizeof(uint16_t); x++) {


        count = 16;

        gpiod_direction_output(gpiocs, 0);

        do {
          gpiod_direction_output(
              gpiomosi, (((a035vw01[x] & (1 << (count - 1))) != 0) << 2));
          gpiod_direction_output(gpiock, 0);
          ndelay(50);
          gpiod_direction_output(gpiock, 1);
          ndelay(50);
          count--;

        } while (count);

        gpiod_direction_output(gpiomosi, 0);
        gpiod_direction_output(gpiocs, 1);
        ndelay(50);
      }


    for (x = 0; x < (sizeof(a035vl01) / sizeof(int32_t)); x++) {
      if (a035vl01[x] == -1) {
        mdelay(120);
        continue;
      }

      if (1) {

        count = 9;

        gpiod_direction_output(gpiocs, 0);

        do {
          gpiod_direction_output(
              gpiomosi,
              ((((uint16_t)a035vl01[x] & (1 << (count - 1))) != 0) << 2));
          gpiod_direction_output(gpiock, 0);
          ndelay(50);
          gpiod_direction_output(gpiock, 1);
          ndelay(50);
          count--;

        } while (count);

        gpiod_direction_output(gpiomosi, 0);
        gpiod_direction_output(gpiocs, 1);
        ndelay(50);
      }
    }
  }

  gpiod_direction_output(gpiocs, 1);
  gpiod_put(gpiomosi);
  gpiod_put(gpiocs);
  gpiod_put(gpiock);
  }}}
  // Map GPIO base address
  gpio_base = ioremap(BCM2835_GPIO_BASE, SZ_16K);

  // GPIO Function Select Register 2 (FSEL2)
  fsel_reg = (uint32_t *)(gpio_base + GPIO_FSEL0_OFFSET);

  // Read current value and modify only GPIO 2 bits
  val = ioread32(fsel_reg);
  val &= ~(0b111 << GPIO_PIN_2_SHIFT);    // Clear current function of GPIO 2
  val |= (GPIO_ALT2 << GPIO_PIN_2_SHIFT); // Set function to ALT2
  val &= ~(0b111 << GPIO_PIN_8_SHIFT);    // Clear current function of GPIO 2
  val |= (GPIO_ALT2 << GPIO_PIN_8_SHIFT); // Set function to ALT2


  // Write back the modified value
  iowrite32(val, fsel_reg);

  // Unmap GPIO base address
  iounmap(gpio_base);
  */

  buf = 0b00001101; // Enable_Int, INT_POL, INT_MODE1, INT_MODE2 -> interrupt
                    // only on changed coords!
  cyc8c2_i2c_write(client, CYC8C2_INT_SETTINGS, &buf, 1);
  buf = 0b11110000; // 0:4 idle , 0 , ALLOWSLEEP, POWERMODE, POWERMODE -> auto
                    // sleep
  cyc8c2_i2c_write(client, CYC8C2_POWER, &buf, 1);

  buf = 0x01;

  cyc8c2_i2c_write(client, 0x6F, &buf, 1); // int width small as possible


  buf = 0x64;
  cyc8c2_i2c_write(client, CYC8C2_Y_SENSIVITY, &buf,
                   1); // -> 255 lowest sensivity
  cyc8c2_i2c_write(client, CYC8C2_X_SENSIVITY, &buf, 1);

  cyc8c2_request_input_dev(ts);

  dev_dbg(&client->dev, "cyc8c2 driver installed\n");

  dev_dbg(&ts->client->dev, "setting up IRQ");
  ts->irq_flags = IRQ_TYPE_EDGE_RISING | IRQF_ONESHOT;
  error = cyc8c2_request_irq(ts);
  if (error) {
    dev_err(&ts->client->dev, "request IRQ failed: %d\n", error);
    return error;
  }

  pr_info("Using: cyc8c2");

  return 0;
}

static const struct i2c_device_id cyc8c2_ts_id[] = {{"cyc8c2:00", 0}, {}};

MODULE_DEVICE_TABLE(i2c, cyc8c2_ts_id);

#ifdef CONFIG_OF
static const struct of_device_id cyc8c2_of_match[] = {{.compatible = "cyc8c2"},
                                                      {}};
MODULE_DEVICE_TABLE(of, cyc8c2_of_match);
#endif

static struct i2c_driver cyc8c2_ts_driver = {
    .probe = cyc8c2_ts_probe,
    .remove = cyc8c2_ts_remove,
    .id_table = cyc8c2_ts_id,
    .driver =
        {
            .name = "cyc8c2-touchscreen",
            .of_match_table = of_match_ptr(cyc8c2_of_match),
        },
};
module_i2c_driver(cyc8c2_ts_driver);

MODULE_AUTHOR("LH");
MODULE_DESCRIPTION("cyc8c2 touchscreen driver");
MODULE_LICENSE("GPL v2");

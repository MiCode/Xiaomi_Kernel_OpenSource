/**
 * Driver for Goodix GT911 touchscreen.
 *
 * Copyright (c) 2014 Intel Corporation
 *
 * Based on Goodix GT9xx driver:
 *	(c) 2010 - 2013 Goodix Technology.
 *	Version: 2.0
 *	Authors: andrew@goodix.com, meta@goodix.com
 *	Release Date: 2013/04/25
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/irq.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/input/mt.h>
#include <linux/delay.h>
#ifdef CONFIG_PM
#include <linux/power_hal_sysfs.h>
#endif

#define GT9XX_MAX_TOUCHES		5

enum gt9xx_status_bits {
	/* bits 0 .. GT9XX_MAX_TOUCHES - 1 are use to track touches */
	GT9XX_STATUS_SLEEP_BIT = GT9XX_MAX_TOUCHES,
	GT9XX_STATUS_BITS,
};

struct gt9xx_ts {
	struct i2c_client *client;
	struct input_dev *input;
	char phys[32];

	struct gpio_desc *gpiod_int;
	struct gpio_desc *gpiod_rst;
	int irq_type;

	u16 max_x;
	u16 max_y;

	DECLARE_BITMAP(status, GT9XX_STATUS_BITS);
};

/* Registers define */
#define GT9XX_REG_CMD			0x8040
#define GT9XX_REG_CONFIG		0x8047
#define GT9XX_REG_ID			0x8140
#define GT9XX_REG_STATUS		0x814E
#define GT9XX_REG_DATA			0x814F

static int gt9xx_i2c_read(struct i2c_client *client, u16 addr,
			  void *buf, unsigned len)
{
	u8 addr_buf[2];
	struct i2c_msg msgs[2];
	int ret;

	addr_buf[0] = addr >> 8;
	addr_buf[1] = addr & 0xFF;

	msgs[0].flags = 0;
	msgs[0].addr = client->addr;
	msgs[0].buf = addr_buf;
	msgs[0].len = sizeof(addr_buf);

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr = client->addr;
	msgs[1].buf = buf;
	msgs[1].len = len;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret != 2)
		dev_err(&client->dev, "I2C read @0x%04X (%d) failed: %d", addr,
			len, ret);

	return 0;
}

static int gt9xx_i2c_write(struct i2c_client *client, u16 addr, void *buf,
			  unsigned len)
{
	u8 *addr_buf;
	struct i2c_msg msg;
	int ret;

	addr_buf = kmalloc(len + 2, GFP_KERNEL);
	if (!addr_buf)
		return -ENOMEM;

	addr_buf[0] = addr >> 8;
	addr_buf[1] = addr & 0xFF;

	memcpy(&addr_buf[2], buf, len);

	msg.flags = 0;
	msg.addr = client->addr;
	msg.buf = addr_buf;
	msg.len = len + 2;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1)
		dev_err(&client->dev, "I2C write @0x%04X (%d) failed: %d", addr,
			len, ret);

	kfree(addr_buf);

	return 0;
}

static int gt9xx_i2c_write_u8(struct i2c_client *client, u16 addr, u8 value)
{
	return gt9xx_i2c_write(client, addr, &value, sizeof(value));
}

static void gt9xx_irq_disable(struct gt9xx_ts *ts, bool no_sync)
{
	if (no_sync)
		disable_irq_nosync(ts->client->irq);
	else
		disable_irq(ts->client->irq);
	gpiod_unlock_as_irq(ts->gpiod_int);
}

static void gt9xx_irq_enable(struct gt9xx_ts *ts)
{
	gpiod_lock_as_irq(ts->gpiod_int);
	enable_irq(ts->client->irq);
}

static irqreturn_t gt9xx_irq_handler(int irq, void *arg)
{
	struct gt9xx_ts *ts = arg;

	gt9xx_irq_disable(ts, true);
	return IRQ_WAKE_THREAD;
}

#define GT9XX_STATUS_REG_MASK_TOUCHES		0x0F
#define GT9XX_STATUS_REG_MASK_VALID		0x80

static irqreturn_t gt9xx_thread_handler(int irq, void *arg)
{
	struct gt9xx_ts *ts = arg;
	struct gt9xx_touch_data {
		u8 id;
		__le16 x;
		__le16 y;
		__le16 witdh;
		u8 reserved;
	} __packed data[GT9XX_MAX_TOUCHES];
	int touches;
	DECLARE_BITMAP(active_touches, GT9XX_MAX_TOUCHES);
	u8 status;
	int ret;
	int i;

	ret = gt9xx_i2c_read(ts->client, GT9XX_REG_STATUS, &status, 1);
	if (ret)
		goto out;

	if (!(status & GT9XX_STATUS_REG_MASK_VALID))
		goto out;

	touches = status & GT9XX_STATUS_REG_MASK_TOUCHES;
	if (touches > GT9XX_MAX_TOUCHES) {
		dev_err(&ts->client->dev, "invalid number of touches");
		goto out;
	}

	if (touches) {
		int len = touches * sizeof(struct gt9xx_touch_data);

		ret = gt9xx_i2c_read(ts->client, GT9XX_REG_DATA, data, len);
		if (ret < 0)
			goto out;
	}

	bitmap_clear(active_touches, 0, GT9XX_MAX_TOUCHES);

	input_report_key(ts->input, BTN_TOUCH, touches);

	/* generate touch down events */
	for (i = 0; i < touches; i++) {
		int id = data[i].id;
		int x = le16_to_cpu(data[i].x);
		int y = le16_to_cpu(data[i].y);
		int w = le16_to_cpu(data[i].witdh);

		set_bit(id, active_touches);

		input_mt_slot(ts->input, id);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
		input_report_abs(ts->input, ABS_MT_POSITION_X, x);
		input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, w);
		input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, w);
	}

	/* generate touch up events */
	for (i = 0; i < GT9XX_MAX_TOUCHES; i++) {
		if (test_bit(i, active_touches)) {
			set_bit(i, ts->status);
			continue;
		} else {
			if (!test_and_clear_bit(i, ts->status))
				continue;
		}

		input_mt_slot(ts->input, i);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
	}

	input_sync(ts->input);

out:
	gt9xx_i2c_write_u8(ts->client, GT9XX_REG_STATUS, 0);

	gt9xx_irq_enable(ts);

	return IRQ_HANDLED;
}

static int gt9xx_i2c_test(struct i2c_client *client)
{
	u8 test;

	return gt9xx_i2c_read(client, GT9XX_REG_CONFIG, &test, sizeof(test));
}

static int gt9xx_get_info(struct gt9xx_ts *ts)
{
	struct gt9xx_config {
		u8 version;
		__le16 max_x;
		__le16 max_y;
		u8 reserved, touch_no:4;
		u8 reserved2:2, stretch_rank:2, x2y:1, sito:1, int_trigger:2;
		u8 data[182];
	} __packed cfg;
	const int irq_table[] = {
		IRQ_TYPE_EDGE_RISING,
		IRQ_TYPE_EDGE_FALLING,
		IRQ_TYPE_LEVEL_LOW,
		IRQ_TYPE_LEVEL_HIGH,
	};
	struct {
		u8 id[4];	/* may not be NULL terminated */
		__le16 fw_version;
	} __packed id;
	char id_str[5];
	int ret;

	ret = gt9xx_i2c_read(ts->client, GT9XX_REG_ID, &id, sizeof(id));
	if (ret) {
		dev_err(&ts->client->dev, "read id failed");
		return ret;
	}

	memcpy(id_str, id.id, 4);
	id_str[4] = 0;
	if (kstrtou16(id_str, 10, &ts->input->id.product))
		ts->input->id.product = 0;
	ts->input->id.version = le16_to_cpu(id.fw_version);

	dev_info(&ts->client->dev, "version: %d_%04x", ts->input->id.product,
		 ts->input->id.version);

	ret = gt9xx_i2c_read(ts->client, GT9XX_REG_CONFIG, &cfg, sizeof(cfg));
	if (ret)
		return ret;

	ts->max_x = le16_to_cpu(cfg.max_x);
	ts->max_y = le16_to_cpu(cfg.max_y);
	ts->irq_type = irq_table[cfg.int_trigger];

	dev_info(&ts->client->dev, "max_x = %d, max_y = %d, irq_type = 0x%02x",
		 ts->max_x, ts->max_y, ts->irq_type);

	return 0;
}

static void gt9xx_int_sync(struct gt9xx_ts *ts)
{
	gpiod_direction_output(ts->gpiod_int, 0);
	msleep(50);
	gpiod_direction_input(ts->gpiod_int);
}

static void gt9xx_reset(struct gt9xx_ts *ts)
{
	/* begin select I2C slave addr */
	gpiod_direction_output(ts->gpiod_rst, 0);
	msleep(20);				/* T2: > 10ms */
	/* HIGH: 0x28/0x29, LOW: 0xBA/0xBB */
	gpiod_direction_output(ts->gpiod_int, ts->client->addr == 0x14);
	msleep(2);				/* T3: > 100us */
	gpiod_direction_output(ts->gpiod_rst, 1);
	msleep(6);				/* T4: > 5ms */
	/* end select I2C slave addr */
	gpiod_direction_input(ts->gpiod_rst);

	gt9xx_int_sync(ts);
}

static int gt9xx_acpi_probe(struct gt9xx_ts *ts)
{
	struct device *dev = &ts->client->dev;
	const struct acpi_device_id *acpi_id;
	struct gpio_desc *gpiod;

	if (!ACPI_HANDLE(dev))
		return -ENODEV;

	acpi_id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!acpi_id) {
		dev_err(dev, "failed to get ACPI info\n");
		return -ENODEV;
	}

	/* Get interrupt GPIO pin number */
	gpiod = devm_gpiod_get_index(dev, "gt9xx_gpio_int", 0);
	if (IS_ERR(gpiod)) {
		int err = PTR_ERR(gpiod);

		dev_err(dev, "get gt9xx_gpio_int failed: %d\n", err);
		return err;
	}

	gpiod_direction_input(gpiod);
	ts->client->irq = gpiod_to_irq(gpiod);
	ts->gpiod_int = gpiod;

	/* get the reset line GPIO pin number */
	gpiod = devm_gpiod_get_index(dev, "gt9xx_gpio_rst", 1);
	if (IS_ERR(gpiod)) {
		int err = PTR_ERR(gpiod);

		dev_err(dev, "get gt9xx_gpio_rst failed: %d\n", err);
		return err;
	}

	gpiod_direction_input(gpiod);
	ts->gpiod_rst = gpiod;

	/* reset the controller */
	gt9xx_reset(ts);

	return 0;
}

#ifdef CONFIG_PM
static void gt9xx_sleep(struct gt9xx_ts *ts)
{
	int ret;

	if (test_and_set_bit(GT9XX_STATUS_SLEEP_BIT, ts->status))
		return;

	gt9xx_irq_disable(ts, false);

	gpiod_direction_output(ts->gpiod_int, 0);
	msleep(5);

	ret = gt9xx_i2c_write_u8(ts->client, GT9XX_REG_CMD, 5);
	if (ret) {
		dev_err(&ts->client->dev, "sleep cmd failed");
		gpiod_direction_input(ts->gpiod_int);
		gt9xx_irq_enable(ts);
		return;
	}

	/* To avoid waking up while is not sleeping,
	   delay 48 + 10ms to ensure reliability
	 */
	msleep(58);

	dev_dbg(&ts->client->dev, "sleeping");
}

static void gt9xx_wakeup(struct gt9xx_ts *ts)
{
	int ret;

	if (!test_and_clear_bit(GT9XX_STATUS_SLEEP_BIT, ts->status))
		return;

	gpiod_direction_output(ts->gpiod_int, 1);
	msleep(10);

	ret = gt9xx_i2c_test(ts->client);
	if (ret) {
		dev_err(&ts->client->dev, "wakeup failed");
		return;
	}

	gt9xx_int_sync(ts);
	gt9xx_irq_enable(ts);

	dev_dbg(&ts->client->dev, "woke up");
}

static ssize_t gt9xx_power_hal_suspend_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct gt9xx_ts *ts = input_get_drvdata(input);
	static DEFINE_MUTEX(mutex);

	mutex_lock(&mutex);
	if (!strncmp(buf, POWER_HAL_SUSPEND_ON, POWER_HAL_SUSPEND_STATUS_LEN))
		gt9xx_sleep(ts);
	else
		gt9xx_wakeup(ts);
	mutex_unlock(&mutex);

	return count;
}

static DEVICE_POWER_HAL_SUSPEND_ATTR(gt9xx_power_hal_suspend_store);
#endif

static int gt9xx_ts_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -1;
	struct gt9xx_ts *ts;
	struct device *dev = &client->dev;

	dev_info(dev, "probing GT911 @ 0x%02x", client->addr);

	ts = devm_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->client = client;
	i2c_set_clientdata(client, ts);

	ts->input = devm_input_allocate_device(dev);
	if (!ts->input)
		return -ENOMEM;

	__set_bit(EV_SYN, ts->input->evbit);
	__set_bit(EV_ABS, ts->input->evbit);
	__set_bit(EV_KEY, ts->input->evbit);
	__set_bit(BTN_TOUCH, ts->input->keybit);

	input_mt_init_slots(ts->input, GT9XX_MAX_TOUCHES, 0);
	input_set_abs_params(ts->input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	snprintf(ts->phys, sizeof(ts->phys), "%s/input0", dev_name(dev));
	ts->input->name = "goodix_ts";
	ts->input->phys = ts->phys;
	ts->input->id.bustype = BUS_I2C;
	ts->input->dev.parent = dev;
	input_set_drvdata(ts->input, ts);

	ret = gt9xx_acpi_probe(ts);
	if (ret)
		return ret;

	ret = gt9xx_get_info(ts);
	if (ret)
		return ret;

	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, ts->max_x, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, ts->max_y, 0, 0);

	ret = input_register_device(ts->input);
	if (ret)
		return ret;

	ret = devm_request_threaded_irq(dev, client->irq, gt9xx_irq_handler,
					gt9xx_thread_handler, ts->irq_type,
					client->name, ts);
	if (ret) {
		dev_err(dev, "request IRQ failed: %d", ret);
		input_unregister_device(ts->input);
		return -1;
	}

#ifdef CONFIG_PM
	ret = device_create_file(dev, &dev_attr_power_HAL_suspend);
	if (ret < 0) {
		dev_err(dev, "unable to create suspend entry");
		goto out;
	}

	ret = register_power_hal_suspend_device(dev);
	if (ret < 0)
		dev_err(dev, "unable to register for power hal");
out:
#endif

	return 0;
}

static int gt9xx_ts_remove(struct i2c_client *client)
{
	struct gt9xx_ts *ts = i2c_get_clientdata(client);

#ifdef CONFIG_PM
	device_remove_file(&client->dev, &dev_attr_power_HAL_suspend);
	unregister_power_hal_suspend_device(&ts->input->dev);
#endif
	i2c_set_clientdata(client, NULL);
	gpiod_direction_input(ts->gpiod_int);
	input_unregister_device(ts->input);

	return 0;
}

static const struct i2c_device_id gt9xx_ts_id[] = {
	{ "GODX0911", 0 },
	{ "GOOD9271", 0 },
	{ }
};

static struct acpi_device_id gt9xx_acpi_match[] = {
	{ "GODX0911", 0 },
	{ "GOOD9271", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, gt9xx_acpi_match);

static struct i2c_driver gt9xx_ts_driver = {
	.probe      = gt9xx_ts_probe,
	.remove     = gt9xx_ts_remove,
	.id_table   = gt9xx_ts_id,
	.driver = {
		.name = "gt9xx_ts",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(gt9xx_acpi_match),
	},
};

module_i2c_driver(gt9xx_ts_driver);

MODULE_DESCRIPTION("Goodix GT911 Touchscreen Driver");
MODULE_AUTHOR("Octavian Purdila <octavian.purdila@intel.com>");
MODULE_LICENSE("GPLv2");

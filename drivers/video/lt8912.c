/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <linux/irqreturn.h>
#include <linux/kd.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <asm/irq.h>

#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/regmap.h>

#define MIPI_1080P
#define device_name	"lt8912"

/**
 * struct lt8912_data - Cached chip configuration data
 * @client: I2C client
 * @dev: device structure
 * @input_hotplug: hotplug input device structure
 * @hotplug_work: hotplug work structure
 *
 */
struct lt8912_data {
	struct i2c_client *lt8912_client;
	struct regmap           *regmap;
	struct input_dev        *input_hotplug;
	struct delayed_work hotplug_work;
	int reset_gpio;
	int last_val;

};

static unsigned int lt8912_i2c_read_byte(struct lt8912_data *data,
							unsigned int reg)
{
	int rc = 0;
	int val = 0;

	rc = regmap_read(data->regmap, reg, &val);
	if (rc) {
		dev_err(&data->lt8912_client->dev, "read 0x%x failed.(%d)\n",
				reg, rc);
		return rc;
	}
	dev_dbg(&data->lt8912_client->dev, "read 0x%x value = 0x%x\n",
				reg, val);
	return val;
}

static int lt8912_i2c_write_byte(struct lt8912_data *data,
					unsigned int reg, unsigned int val)
{
	int rc = 0;

	rc = regmap_write(data->regmap, reg, val);
	if (rc) {
		dev_err(&data->lt8912_client->dev,
				"write 0x%x register failed\n", reg);
		return rc;
	}

	return 0;
}

static const struct of_device_id of_rk_lt8912_match[] = {
	{ .compatible = "lontium,lt8912"},
	{  },
};

static const struct i2c_device_id lt8912_id[] = {
	{device_name, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, lt8912_id);

static void lt8912_process_data(struct lt8912_data *data)
{
	int val = 0;
	ktime_t timestamp;

	timestamp = ktime_get_boottime();

	val = lt8912_i2c_read_byte(data, 0xc1);

	if (val != data->last_val) {
		input_report_abs(data->input_hotplug, ABS_MISC, val);
		input_event(data->input_hotplug, EV_SYN, SYN_CONFIG,
					ktime_to_timespec(timestamp).tv_sec);
		input_event(data->input_hotplug, EV_SYN, SYN_CONFIG,
					ktime_to_timespec(timestamp).tv_nsec);
		input_sync(data->input_hotplug);

		dev_dbg(&data->lt8912_client->dev,
				"input report val = %d\n", val);
	}

	data->last_val = val;
}

static void lt8912_input_work_fn(struct work_struct *work)
{
	struct lt8912_data *data;

	data = container_of((struct delayed_work *)work,
					struct lt8912_data, hotplug_work);

	lt8912_process_data(data);

	schedule_delayed_work(&data->hotplug_work,
						  msecs_to_jiffies(2000));
}

static int digital_clock_enable(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x08, 0xff);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x09, 0xff);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x0a, 0xff);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x0b, 0xff);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x0c, 0xff);
	if (rc)
		return rc;

	return 0;
}

static int tx_analog(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x31, 0xa1);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x32, 0xa1);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x33, 0x03);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x37, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x38, 0x22);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x60, 0x82);
	if (rc)
		return rc;
	return 0;
}

static int cbus_analog(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x39, 0x45);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3b, 0x00);
	if (rc)
		return rc;

	return 0;
}

static int hdmi_pll_analog(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x44, 0x31);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x55, 0x44);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x57, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5a, 0x02);
	if (rc)
		return rc;
	return 0;
}

static int mipi_rx_logic_res(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x03, 0x7f);
	if (rc)
		return rc;

	msleep(100);

	rc = lt8912_i2c_write_byte(data, 0x03, 0xff);
	if (rc)
		return rc;

	return 0;
}

static int mipi_basic_set(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;

	/* term en  To analog phy for trans lp mode to hs mode */
	rc = lt8912_i2c_write_byte(data, 0x10, 0x20);
	if (rc)
		return rc;

	/* settle Set timing for dphy trans state from PRPR to SOT state */
	rc = lt8912_i2c_write_byte(data, 0x11, 0x04);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x12, 0x04);
	if (rc)
		return rc;

	/* 4 lane, 01 lane, 02 2 lane, 03 3lane */
	rc = lt8912_i2c_write_byte(data, 0x13, 0x02);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x14, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x15, 0x00);
	if (rc)
		return rc;

	/* hshift 3 */
	rc = lt8912_i2c_write_byte(data, 0x1a, 0x03);
	if (rc)
		return rc;

	/* vshift 3 */
	rc = lt8912_i2c_write_byte(data, 0x1b, 0x03);
	if (rc)
		return rc;
	return 0;
}

#ifdef MIPI_1080P
static int mipi_dig_set_res(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;
	rc = lt8912_i2c_write_byte(data, 0x18, 0x2c);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x19, 0x05);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1c, 0x80);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1d, 0x07);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2f, 0x0c);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x34, 0x98);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x35, 0x08);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x36, 0x65);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x37, 0x04);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x38, 0x24);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x39, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3a, 0x04);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3b, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3c, 0x94);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3d, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3e, 0x58);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3f, 0x00);
	if (rc)
		return rc;
	return 0;
}

#endif

#ifdef MIPI_720P
static int mipi_dig_set_res(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;
	rc = lt8912_i2c_write_byte(data, 0x18, 0x28);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x19, 0x05);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1c, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1d, 0x05);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1e, 0x67);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2f, 0x0c);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x34, 0x72);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x35, 0x06);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x36, 0xee);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x37, 0x02);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x38, 0x14);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x39, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3a, 0x05);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3b, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3c, 0xdc);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3d, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3e, 0x6e);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3f, 0x00);
	if (rc)
		return rc;
	return 0;
}

#endif

#ifdef MIPI_480P
static int mipi_dig_set_res(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;
	rc = lt8912_i2c_write_byte(data, 0x18, 0x60);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x19, 0x02);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1c, 0x80);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1d, 0x02);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1e, 0x67);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2f, 0x0c);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x34, 0x20);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x35, 0x03);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x36, 0x0d);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x37, 0x02);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x38, 0x20);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x39, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3a, 0x0a);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3b, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3c, 0x30);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3d, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3e, 0x10);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3f, 0x00);
	if (rc)
		return rc;
	return 0;
}
#endif

static void hdmi_status_init(struct lt8912_data *data)
{
	int val = 0;

	val = lt8912_i2c_read_byte(data, 0xc1);
	data->last_val = val;
}

static int dds_config(struct lt8912_data *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;

	/* strm_sw_freq_word[ 7: 0] */
	rc = lt8912_i2c_write_byte(data, 0x4e, 0x6A);
	if (rc)
		return rc;

	/* strm_sw_freq_word[15: 8] */
	rc = lt8912_i2c_write_byte(data, 0x4f, 0x4D);
	if (rc)
		return rc;

	/* strm_sw_freq_word[23:16] */
	rc = lt8912_i2c_write_byte(data, 0x50, 0xF3);
	if (rc)
		return rc;

	/* [0]=strm_sw_freq_word[24]//[7]=strm_sw_freq_word_en=0,
	[6]=strm_err_clr=0 */
	rc = lt8912_i2c_write_byte(data, 0x51, 0x80);
	if (rc)
		return rc;

	/* full_value  464 */
	rc = lt8912_i2c_write_byte(data, 0x1f, 0x90);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x20, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x21, 0x68);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x22, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x23, 0x5E);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x24, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x25, 0x54);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x26, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x27, 0x90);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x28, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x29, 0x68);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2a, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2b, 0x5E);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2c, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2d, 0x54);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2e, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x42, 0x64);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x43, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x44, 0x04);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x45, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x46, 0x59);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x47, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x48, 0xf2);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x49, 0x06);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x4a, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x4b, 0x72);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x4c, 0x45);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x4d, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x52, 0x08);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x53, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x54, 0xb2);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x55, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x56, 0xe4);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x57, 0x0d);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x58, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x59, 0xe4);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5a, 0x8a);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5b, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5c, 0x34);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1e, 0x4f);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x51, 0x00);
	if (rc)
		return rc;
	return 0;
}

static int lt8912_init_input(struct lt8912_data *data)
{
	struct input_dev *input;
	int status;

	input = devm_input_allocate_device(&data->lt8912_client->dev);
	if (!input) {
		dev_err(&data->lt8912_client->dev,
			"allocate light input device failed\n");
		return PTR_ERR(input);
	}

	input->name = "lt8912";
	input->phys = "lt8912/input0";
	input->id.bustype = BUS_I2C;

	__set_bit(EV_ABS, input->evbit);
	input_set_abs_params(input, ABS_MISC, 0, 655360, 0, 0);

	status = input_register_device(input);
	if (status) {
		dev_err(&data->lt8912_client->dev,
			"register light input device failed.\n");
		return status;
	}

	data->input_hotplug = input;
	return 0;
}

static int lt8912_parse_dt(struct device *dev, struct lt8912_data *data)
{
	struct device_node *np = dev->of_node;

	data->reset_gpio = of_get_named_gpio(np, "qcom,hdmi-reset", 0);

	if (data->reset_gpio < 0)
		return data->reset_gpio;

	return 0;
}

static ssize_t lt8912_register_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lt8912_data *data = dev_get_drvdata(dev);
	unsigned int val = 0;
	int i = 0;
	ssize_t count = 0;

	val = lt8912_i2c_read_byte(data, 0x00);

	count += snprintf(&buf[count], PAGE_SIZE,
					  "0x%x: 0x%x\n", 0x00, val);
	val = lt8912_i2c_read_byte(data, 0x01);

	count += snprintf(&buf[count], PAGE_SIZE,
					  "0x%x: 0x%x\n", 0x01, (char)val);

	val = lt8912_i2c_read_byte(data, 0x9c);

	count += snprintf(&buf[count], PAGE_SIZE,
					  "0x%x: 0x%x\n", 0x9c, (char)val);

	val = lt8912_i2c_read_byte(data, 0x9d);

	count += snprintf(&buf[count], PAGE_SIZE,
					  "0x%x: 0x%x\n", 0x9d, (char)val);

	val = lt8912_i2c_read_byte(data, 0x9e);

	count += snprintf(&buf[count], PAGE_SIZE,
					  "0x%x: 0x%x\n", 0x9e, (char)val);

	val = lt8912_i2c_read_byte(data, 0x9f);

	count += snprintf(&buf[count], PAGE_SIZE,
					  "0x%x: 0x%x\n", 0x9f, (char)val);

	data->lt8912_client->addr = 0x49;
	for (i = 0x00; i <= 0x60; i++) {
		val = lt8912_i2c_read_byte(data, i);

		count += snprintf(&buf[count], PAGE_SIZE,
						  "0x%x: 0x%x\n", i, val);
	}
	data->lt8912_client->addr = 0x48;

	return count;
}

static DEVICE_ATTR(register, S_IWUSR | S_IRUGO,
				   lt8912_register_show,
				   NULL);
static struct attribute *lt8912_attr[] = {
	&dev_attr_register.attr,
	NULL
};

static const struct attribute_group lt8912_attr_group = {
	.attrs = lt8912_attr,
};

static struct regmap_config lt8912_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int lontium_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct lt8912_data *data;
	int ret;

	dev_dbg(&client->dev, "probing lt8912\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "lt8912 i2c check failed.\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct lt8912_data),
						GFP_KERNEL);

	data->lt8912_client = client;

	if (client->dev.of_node) {
		ret = lt8912_parse_dt(&client->dev, data);
		if (ret) {
			dev_err(&client->dev,
				"unable to parse device tree.(%d)\n", ret);
			goto out;
		}
	} else {
		dev_err(&client->dev, "device tree not found.\n");
		ret = -ENODEV;
		goto out;
	}

	dev_set_drvdata(&client->dev, data);

	if (gpio_is_valid(data->reset_gpio)) {
		ret = gpio_request(data->reset_gpio, "lt8912_reset_gpio");
		if (ret) {
			dev_err(&client->dev, "reset gpio request failed");
			goto out;
		}
		dev_dbg(&client->dev, "enter gpio_request\n");

		ret = gpio_direction_output(data->reset_gpio, 0);
		if (ret) {
			dev_err(&client->dev,
				"set_direction for reset gpio failed\n");
			goto free_reset_gpio;
		}

		msleep(20);
		gpio_set_value_cansleep(data->reset_gpio, 1);
		dev_dbg(&client->dev, "enter gpio_set_value_cansleep\n");
	}

	data->regmap = devm_regmap_init_i2c(client, &lt8912_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&client->dev, "init regmap failed.(%ld)\n",
				PTR_ERR(data->regmap));
		ret = PTR_ERR(data->regmap);
		goto free_reset_gpio;
	}

	dev_dbg(&client->dev, "enter lontium chip_init\n");

	ret = digital_clock_enable(data);
	if (ret)
		goto free_reset_gpio;
	ret = tx_analog(data);
	if (ret)
		goto free_reset_gpio;

	ret = cbus_analog(data);
	if (ret)
		goto free_reset_gpio;

	ret = hdmi_pll_analog(data);
	if (ret)
		goto free_reset_gpio;

	ret = mipi_basic_set(data);
	if (ret)
		goto free_reset_gpio;

	ret = mipi_dig_set_res(data);
	if (ret)
		goto free_reset_gpio;

	ret = dds_config(data);
	if (ret)
		goto free_reset_gpio;

	ret = mipi_rx_logic_res(data);
	if (ret)
		goto free_reset_gpio;

	ret = sysfs_create_group(&client->dev.kobj, &lt8912_attr_group);
	if (ret) {
		dev_err(&client->dev, "sysfs create group failed\n");
		goto free_reset_gpio;
	}

	hdmi_status_init(data);

	ret = lt8912_init_input(data);
	if (ret) {
		dev_err(&client->dev, "input init failed\n");
		goto free_reset_gpio;
	}

	INIT_DELAYED_WORK(&data->hotplug_work, lt8912_input_work_fn);
	schedule_delayed_work(&data->hotplug_work, msecs_to_jiffies(2000));

	return 0;

free_reset_gpio:
	if (gpio_is_valid(data->reset_gpio))
		gpio_free(data->reset_gpio);
out:
	return ret;
}

static int lontium_i2c_remove(struct i2c_client *client)
{
	struct lt8912_data *data = dev_get_drvdata(&client->dev);

	if (gpio_is_valid(data->reset_gpio))
		gpio_free(data->reset_gpio);
	sysfs_remove_group(&client->dev.kobj, &lt8912_attr_group);

	return 0;
}

static struct i2c_driver lontium_i2c_driver = {

	.driver = {
		.name = "lontium_i2c",
		.owner = THIS_MODULE,
		.of_match_table = of_rk_lt8912_match,
	},
	.probe = lontium_i2c_probe,
	.remove = lontium_i2c_remove,
	.id_table = lt8912_id,
};

static int __init lontium_i2c_test_init(void)
{
	int ret;

	ret = i2c_add_driver(&lontium_i2c_driver);

	return ret;
}

static void __exit lontium_i2c_test_exit(void)
{
	i2c_del_driver(&lontium_i2c_driver);
}

module_init(lontium_i2c_test_init);
module_exit(lontium_i2c_test_exit);

MODULE_DESCRIPTION("I2C_TEST");

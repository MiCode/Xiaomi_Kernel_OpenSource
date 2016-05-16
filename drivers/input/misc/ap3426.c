/*
 * This file is part of the AP3426, AP3212C and AP3216C sensor driver.
 * AP3426 is combined proximity and ambient light sensor.
 * AP3216C is combined proximity, ambient light sensor and IRLED.
 *
 * Contact: John Huang <john.huang@dyna-image.com>
 *		Templeton Tsai <templeton.tsai@dyna-image.com>
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 *
 * Filename: ap3426.c
 *
 * Summary:
 *	AP3426 device driver.
 *
 * Modification History:
 * Date	 By	   Summary
 * -------- -------- -------------------------------------------------------
 * 02/02/12 YC	   1. Modify irq function to seperate two interrupt routine.
 *					 2. Fix the index of reg array error in em write.
 * 02/22/12 YC	   3. Merge AP3426 and AP3216C into the same driver. (ver 1.8)
 * 03/01/12 YC	   Add AP3212C into the driver. (ver 1.8)
 * 07/25/14 John	  Ver.2.1 , ported for Nexus 7
 * 08/21/14 Templeton AP3426 Ver 1.0, ported for Nexus 7
 * 09/24/14 kevin	Modify for Qualcomm8x10 to support device tree
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/string.h>
#include <mach/gpio.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/of_gpio.h>
#include "ap3426.h"
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#include <linux/hardware_info.h>

#define AP3426_DRV_NAME		"ap3426"
#define DRIVER_VERSION		"1.0"

#define PL_TIMER_DELAY 200

/* misc define */
#define MIN_ALS_POLL_DELAY_MS	110


#define AP3426_VDD_MIN_UV	2000000
#define AP3426_VDD_MAX_UV	3300000
#define AP3426_VIO_MIN_UV	1750000
#define AP3426_VIO_MAX_UV	1950000


#ifdef LSC_DBG
#define LDBG(s, args...)	{printk("LDBG: func [%s], line [%d], ", __func__, __LINE__); printk(s, ## args); }
#else
#define LDBG(s, args...) {}
#endif

static void pl_timer_callback(unsigned long pl_data);
static int ap3426_power_ctl(struct ap3426_data *data, bool on);
static int ap3426_power_init(struct ap3426_data *data, bool on);

static struct ap3426_data *private_pl_data;

static u8 ap3426_reg_to_idx_array[AP3426_MAX_REG_NUM] = {
	0, 	1, 	2, 	0xff, 	0xff, 	0xff, 	3, 	0xff,
	0xff, 	0xff, 	4, 	5, 	6, 	7, 	8, 	9,
	10, 	0xff, 	0xff, 	0xff, 	0xff, 	0xff, 	0xff, 	0xff,
	0xff, 	0xff, 	11, 	12, 	13, 	14, 	0xff, 	0xff,
	15, 	16, 	17, 	18, 	19, 	20, 	21, 	0xff,
	22, 	23, 	24, 	25, 	26, 	27
};
static u8 ap3426_reg[AP3426_NUM_CACHABLE_REGS] = {
	0x00, 0x01, 0x02, 0x06, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x1A, 0x1B, 0x1C, 0x1D, 0x20, 0x21, 0x22, 0x23, 0x24,
	0x25, 0x26, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D
};

static int ap3426_range[4] = {32768, 8192, 2048, 512};


static u8 *reg_array = ap3426_reg;
static int *range = ap3426_range;

static int cali = 100;
static int misc_ps_opened;
static int misc_ls_opened;
static int misc_ht_opened;
struct regulator *vdd;
struct regulator *vio;
bool power_enabled;

/*
 * register access helpers
 */

/*
fixed for msm8916 kevindang20141010
*/
static struct sensors_classdev sensors_light_cdev = {
	.name = "light",
	.vendor = "DI",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "6500",
	.resolution = "0.0625",
	.sensor_power = "0.09",
	.min_delay = 0, 	/* us */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};


static struct sensors_classdev sensors_proximity_cdev = {
	.name = "proximity",
	.vendor = "DI",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5.0",
	.resolution = "5.0",
	.sensor_power = "0.1",
	.min_delay = 0,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static int __ap3426_read_reg(struct i2c_client *client,
		u32 reg, u8 mask, u8 shift)
{
	struct ap3426_data *data = i2c_get_clientdata(client);

	return (data->reg_cache[ap3426_reg_to_idx_array[reg]] & mask) >> shift;
}

static int __ap3426_write_reg(struct i2c_client *client,
	u32 reg, u8 mask, u8 shift, u8 val)
{
	struct ap3426_data *data = i2c_get_clientdata(client);
	int ret = 0;
	u8 tmp;

	tmp = data->reg_cache[ap3426_reg_to_idx_array[reg]];
	tmp &= ~mask;
	tmp |= val << shift;

	ret = i2c_smbus_write_byte_data(client, reg, tmp);
	if (!ret)
		data->reg_cache[ap3426_reg_to_idx_array[reg]] = tmp;

	return ret;
}

/*
 * internally used functions
 */

/* range */
static int ap3426_get_range(struct i2c_client *client)
{
	u8 idx = __ap3426_read_reg(client, AP3426_REG_ALS_CONF,
			AP3426_ALS_RANGE_MASK, AP3426_ALS_RANGE_SHIFT);
	return range[idx];
}

static int ap3426_set_range(struct i2c_client *client, int range)
{
	return __ap3426_write_reg(client, AP3426_REG_ALS_CONF,
			AP3426_ALS_RANGE_MASK, AP3426_ALS_RANGE_SHIFT, range);
}

/* mode */
static int ap3426_get_mode(struct i2c_client *client)
{
	int ret;

	ret = __ap3426_read_reg(client, AP3426_REG_SYS_CONF,
			AP3426_REG_SYS_CONF_MASK, AP3426_REG_SYS_CONF_SHIFT);
	return ret;
}

static int ap3426_set_mode(struct i2c_client *client, int mode)
{
	int ret;

	misc_ps_opened = mode & AP3426_SYS_ALS_ENABLE;
	misc_ls_opened = mode & AP3426_SYS_PS_ENABLE;

	ret = __ap3426_write_reg(client, AP3426_REG_SYS_CONF,
			AP3426_REG_SYS_CONF_MASK, AP3426_REG_SYS_CONF_SHIFT, mode);
	return ret;
}

/* ALS low threshold */
static int ap3426_get_althres(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __ap3426_read_reg(client, AP3426_REG_ALS_THDL_L,
			AP3426_REG_ALS_THDL_L_MASK, AP3426_REG_ALS_THDL_L_SHIFT);
	msb = __ap3426_read_reg(client, AP3426_REG_ALS_THDL_H,
			AP3426_REG_ALS_THDL_H_MASK, AP3426_REG_ALS_THDL_H_SHIFT);
	return (msb << 8) | lsb;
}

static int ap3426_set_althres(struct i2c_client *client, int val)
{

	int lsb, msb, err;

	msb = val >> 8;
	lsb = val & AP3426_REG_ALS_THDL_L_MASK;

	err = __ap3426_write_reg(client, AP3426_REG_ALS_THDL_L,
			AP3426_REG_ALS_THDL_L_MASK, AP3426_REG_ALS_THDL_L_SHIFT, lsb);
	if (err)
		return err;

	err = __ap3426_write_reg(client, AP3426_REG_ALS_THDL_H,
			AP3426_REG_ALS_THDL_H_MASK, AP3426_REG_ALS_THDL_H_SHIFT, msb);

	return err;
}

/* ALS high threshold */
static int ap3426_get_ahthres(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __ap3426_read_reg(client, AP3426_REG_ALS_THDH_L,
			AP3426_REG_ALS_THDH_L_MASK, AP3426_REG_ALS_THDH_L_SHIFT);
	msb = __ap3426_read_reg(client, AP3426_REG_ALS_THDH_H,
			AP3426_REG_ALS_THDH_H_MASK, AP3426_REG_ALS_THDH_H_SHIFT);
	return (msb << 8) | lsb;
}

static int ap3426_set_ahthres(struct i2c_client *client, int val)
{
	int lsb, msb, err;

	msb = val >> 8;
	lsb = val & AP3426_REG_ALS_THDH_L_MASK;

	err = __ap3426_write_reg(client, AP3426_REG_ALS_THDH_L,
			AP3426_REG_ALS_THDH_L_MASK, AP3426_REG_ALS_THDH_L_SHIFT, lsb);
	if (err)
		return err;

	err = __ap3426_write_reg(client, AP3426_REG_ALS_THDH_H,
			AP3426_REG_ALS_THDH_H_MASK, AP3426_REG_ALS_THDH_H_SHIFT, msb);

	return err;
}

/* PX low threshold */
static int ap3426_get_plthres(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __ap3426_read_reg(client, AP3426_REG_PS_THDL_L,
			AP3426_REG_PS_THDL_L_MASK, AP3426_REG_PS_THDL_L_SHIFT);
	msb = __ap3426_read_reg(client, AP3426_REG_PS_THDL_H,
			AP3426_REG_PS_THDL_H_MASK, AP3426_REG_PS_THDL_H_SHIFT);
	return (msb << 8) | lsb;
}

static int ap3426_set_plthres(struct i2c_client *client, int val)
{
	int lsb, msb, err;

	msb = val >> 8;
	lsb = val & AP3426_REG_PS_THDL_L_MASK;

	err = __ap3426_write_reg(client, AP3426_REG_PS_THDL_L,
			AP3426_REG_PS_THDL_L_MASK, AP3426_REG_PS_THDL_L_SHIFT, lsb);
	if (err)
		return err;

	err = __ap3426_write_reg(client, AP3426_REG_PS_THDL_H,
			AP3426_REG_PS_THDL_H_MASK, AP3426_REG_PS_THDL_H_SHIFT, msb);

	return err;
}

/* PX high threshold */
static int ap3426_get_phthres(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __ap3426_read_reg(client, AP3426_REG_PS_THDH_L,
			AP3426_REG_PS_THDH_L_MASK, AP3426_REG_PS_THDH_L_SHIFT);
	msb = __ap3426_read_reg(client, AP3426_REG_PS_THDH_H,
			AP3426_REG_PS_THDH_H_MASK, AP3426_REG_PS_THDH_H_SHIFT);
	return (msb << 8) | lsb;
}

static int ap3426_set_phthres(struct i2c_client *client, int val)
{
	int lsb, msb, err;

	msb = val >> 8;
	lsb = val & AP3426_REG_PS_THDH_L_MASK;

	err = __ap3426_write_reg(client, AP3426_REG_PS_THDH_L,
			AP3426_REG_PS_THDH_L_MASK, AP3426_REG_PS_THDH_L_SHIFT, lsb);
	if (err)
		return err;

	err = __ap3426_write_reg(client, AP3426_REG_PS_THDH_H,
			AP3426_REG_PS_THDH_H_MASK, AP3426_REG_PS_THDH_H_SHIFT, msb);

	return err;
}

static int ap3426_get_adc_value(struct i2c_client *client)
{
	unsigned int lsb, msb, val;
#ifdef LSC_DBG
	unsigned int tmp, range;
#endif

	lsb = i2c_smbus_read_byte_data(client, AP3426_REG_ALS_DATA_LOW);

	if (lsb < 0) {
		return lsb;
	}

	msb = i2c_smbus_read_byte_data(client, AP3426_REG_ALS_DATA_HIGH);

	if (msb < 0)
		return msb;

#ifdef LSC_DBG
	range = ap3426_get_range(client);
	tmp = (((msb << 8) | lsb) * range) >> 16;
	tmp = tmp * cali / 100;
	LDBG("ALS val=%d lux\n", tmp);
#endif
	val = msb << 8 | lsb;

	return val;
}


static int ap3426_get_object(struct i2c_client *client)
{
	int val;

	val = i2c_smbus_read_byte_data(client, AP3426_OBJ_COMMAND);
	LDBG("val=%x\n", val);
	val &= AP3426_OBJ_MASK;


	return !(val >> AP3426_OBJ_SHIFT);
}

static int ap3426_get_intstat(struct i2c_client *client)
{
	int val;

	val = i2c_smbus_read_byte_data(client, AP3426_REG_SYS_INTSTATUS);
	val &= AP3426_REG_SYS_INT_MASK;

	return val >> AP3426_REG_SYS_INT_SHIFT;
}

static int ap3426_get_px_value(struct i2c_client *client)
{
	int lsb, msb;

	lsb = i2c_smbus_read_byte_data(client, AP3426_REG_PS_DATA_LOW);

	if (lsb < 0)
		return lsb;

	LDBG("%s, IR = %d\n", __func__, (u32)(lsb));
	msb = i2c_smbus_read_byte_data(client, AP3426_REG_PS_DATA_HIGH);

	if (msb < 0)
		return msb;

	LDBG("%s, IR = %d\n", __func__, (u32)(msb));
	return (u32)(((msb & AL3426_REG_PS_DATA_HIGH_MASK) << 8) | (lsb & AL3426_REG_PS_DATA_LOW_MASK));
}

static int ap3426_ps_enable(struct ap3426_data *ps_data, int enable)
{
	int32_t ret;
	if (misc_ps_opened == enable)
				return 0;
	misc_ps_opened = enable;
	ret = __ap3426_write_reg(ps_data->client,
		AP3426_REG_SYS_CONF, AP3426_REG_SYS_INT_PMASK, 1, enable);
	if (ret < 0) {
		printk("ps enable error!!!!!!\n");
	}

	if (enable) {
		enable_irq(ps_data->client->irq);
		wake_lock(&ps_data->ps_wake_lock);
	} else {
		disable_irq_nosync(ps_data->client->irq);
		wake_unlock(&ps_data->ps_wake_lock);
	}

	ret = mod_timer(&ps_data->pl_timer, jiffies + msecs_to_jiffies(PL_TIMER_DELAY));

	return ret;
}
static int ap3426_ls_enable(struct ap3426_data *ps_data, int enable)
{
	int32_t ret;
	LDBG("%s, misc_ls_opened = %d, enable=%d\n", __func__, misc_ls_opened, enable);
	if (ps_data->rels_enable == 1) {

		ps_data->rels_enable = 0;
	} else if (misc_ls_opened == enable) {

		return 0;
	}
	misc_ls_opened = enable;
	ret = __ap3426_write_reg(ps_data->client,
		AP3426_REG_SYS_CONF, AP3426_REG_SYS_INT_AMASK, 0, enable);
	if (ret < 0) {
		printk("ls enable error!!!!!!\n");
	}

	if (enable)
		enable_irq(ps_data->client->irq);
	else
		disable_irq_nosync(ps_data->client->irq);


	ret = mod_timer(&ps_data->pl_timer, jiffies + msecs_to_jiffies(PL_TIMER_DELAY));
	return ret;
}

/*********************************************************************
light sensor register & unregister
********************************************************************/
static ssize_t ls_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int32_t ret;

	ret = misc_ls_opened;
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t ls_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct ap3426_data *ps_data =  dev_get_drvdata(dev);
	uint8_t en;
	if (sysfs_streq(buf, "1"))
		en = 1;
	else if (sysfs_streq(buf, "0"))
		en = 0;
	else {
		printk(KERN_ERR "%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
	LDBG("%s, en = %d\n", __func__, (u32)(en));
	ap3426_ls_enable(ps_data, en);
	return size;
}

static struct device_attribute ls_enable_attribute = __ATTR(enable, 0666, ls_enable_show, ls_enable_store);

static struct attribute *ap3426_ls_attrs[] = {
		&ls_enable_attribute.attr,
		NULL
};

static struct attribute_group ap3426_ls_attribute_group = {
		.attrs = ap3426_ls_attrs,
};

static int ap3426_register_lsensor_device(struct i2c_client *client, struct ap3426_data *data)
{
	struct input_dev *input_dev;
	int rc;

	LDBG("allocating input device lsensor\n");
	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "%s: could not allocate input device for lsensor\n", __FUNCTION__);
		rc = -ENOMEM;
		goto done;
	}
	data->lsensor_input_dev = input_dev;
	input_set_drvdata(input_dev, data);
	input_dev->name = "light";
	input_dev->dev.parent = &client->dev;
	set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_MISC, 0, ((1 << 16) - 1), 0, 0);

	rc = input_register_device(input_dev);
	if (rc < 0) {
		pr_err("%s: could not register input device for lsensor\n", __FUNCTION__);
		goto done;
	}
	rc = sysfs_create_group(&input_dev->dev.kobj, &ap3426_ls_attribute_group);
done:
	return rc;
}

static void ap3426_unregister_lsensor_device(struct i2c_client *client, struct ap3426_data *data)
{
	input_unregister_device(data->lsensor_input_dev);
}

/*********************************************************************
heartbeat sensor register & unregister
********************************************************************/
static int ap3426_register_heartbeat_sensor_device(struct i2c_client *client, struct ap3426_data *data)
{
	struct input_dev *input_dev;
	int rc;

	LDBG("allocating input device heartbeat sensor\n");
	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "%s: could not allocate input device for heartbeat sensor\n", __FUNCTION__);
		rc = -ENOMEM;
		goto done;
	}
	data->hsensor_input_dev = input_dev;
	input_set_drvdata(input_dev, data);
	input_dev->name = "heartbeat";
	input_dev->dev.parent = &client->dev;
	set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_WHEEL, 0, 8, 0, 0);

	rc = input_register_device(input_dev);
	if (rc < 0) {
		pr_err("%s: could not register input device for heartbeat sensor\n", __FUNCTION__);
		goto done;
	}
done:
	return rc;
}

static void ap3426_unregister_heartbeat_device(struct i2c_client *client, struct ap3426_data *data)
{
	input_unregister_device(data->hsensor_input_dev);
}

/*********************************************************************
proximity sensor register & unregister
********************************************************************/
static ssize_t ps_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int32_t ret;

	ret = misc_ls_opened;
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t ps_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct ap3426_data *ps_data =  dev_get_drvdata(dev);
	uint8_t en;
	if (sysfs_streq(buf, "1"))
		en = 1;
	else if (sysfs_streq(buf, "0"))
		en = 0;
	else {
		printk(KERN_ERR "%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
	LDBG("%s, en = %d\n", __func__, (u32)(en));
	ap3426_ps_enable(ps_data, en);
	return size;
}

static struct device_attribute ps_enable_attribute = __ATTR(enable, 0666, ps_enable_show, ps_enable_store);

static struct attribute *ap3426_ps_attrs[] = {
	&ps_enable_attribute.attr,
	NULL
};

static struct attribute_group ap3426_ps_attribute_group = {
		.attrs = ap3426_ps_attrs,
};

static int ap3426_register_psensor_device(struct i2c_client *client, struct ap3426_data *data)
{
	struct input_dev *input_dev;
	int rc;

	LDBG("allocating input device psensor\n");
	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "%s: could not allocate input device for psensor\n", __FUNCTION__);
		rc = -ENOMEM;
		goto done;
	}
	data->psensor_input_dev = input_dev;
	input_set_drvdata(input_dev, data);
	input_dev->name = "proximity";
	input_dev->dev.parent = &client->dev;
	set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	rc = input_register_device(input_dev);
	if (rc < 0) {
		pr_err("%s: could not register input device for psensor\n", __FUNCTION__);
		goto done;
	}

	rc = sysfs_create_group(&input_dev->dev.kobj, &ap3426_ps_attribute_group);

done:
	return rc;
}

static void ap3426_unregister_psensor_device(struct i2c_client *client, struct ap3426_data *data)
{
	input_unregister_device(data->psensor_input_dev);
}

/* range */
static ssize_t ap3426_show_range(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	return sprintf(buf, "%i\n", ap3426_get_range(data->client));
}

static ssize_t ap3426_store_range(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if ((strict_strtoul(buf, 10, &val) < 0) || (val > 3))
	return -EINVAL;

	ret = ap3426_set_range(data->client, val);

	return (ret < 0) ? ret:count;
}



static ssize_t ap3426_als_enable_set(struct sensors_classdev *sensors_cdev,
						unsigned int enabled)
{
	struct ap3426_data *als_data = container_of(sensors_cdev,
						struct ap3426_data, als_cdev);
	int err;

	err = ap3426_ls_enable(als_data, enabled);


	if (err < 0)
		return err;
	return 0;
}

static ssize_t ap3426_als_poll_delay_set(struct sensors_classdev *sensors_cdev,
					   unsigned int delay_msec)
{
	struct ap3426_data *als_data = container_of(sensors_cdev,
					   struct ap3426_data, als_cdev);

	int ret;

	if (delay_msec < MIN_ALS_POLL_DELAY_MS) {
		ret = mod_timer(&als_data->pl_timer, jiffies + msecs_to_jiffies(MIN_ALS_POLL_DELAY_MS));
	}

	if (delay_msec > PL_TIMER_DELAY) {
		ret = mod_timer(&als_data->pl_timer, jiffies + msecs_to_jiffies(PL_TIMER_DELAY));
	}

	return 0;
}

static ssize_t ap3426_ps_enable_set(struct sensors_classdev *sensors_cdev,
			unsigned int enabled)
{
	struct ap3426_data *ps_data = container_of(sensors_cdev,
				struct ap3426_data, ps_cdev);
	int err;

	err = ap3426_ps_enable(ps_data, enabled);

	if (err < 0)
		return err;
	return 0;
}



static int ap3426_power_ctl(struct ap3426_data *data, bool on)
{
	int ret = 0;

	if (!on && data->power_enabled) {
		ret = regulator_disable(data->vdd);
		if (ret) {
			dev_err(&data->client->dev,
				"Regulator vdd disable failed ret=%d\n", ret);
			return ret;
		}

		ret = regulator_disable(data->vio);
		if (ret) {
			dev_err(&data->client->dev,
				"Regulator vio disable failed ret=%d\n", ret);
			ret = regulator_enable(data->vdd);
			if (ret) {
				dev_err(&data->client->dev,
					"Regulator vdd enable failed ret=%d\n",
					ret);
			}
			return ret;
		}

		data->power_enabled = on;
		printk(KERN_INFO "%s: disable ap3426 power", __func__);
		dev_dbg(&data->client->dev, "ap3426_power_ctl on=%d\n",
				on);
	} else if (on && !data->power_enabled) {
		ret = regulator_enable(data->vdd);
		if (ret) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed ret=%d\n", ret);
			return ret;
		}

		ret = regulator_enable(data->vio);
		if (ret) {
			dev_err(&data->client->dev,
				"Regulator vio enable failed ret=%d\n", ret);
			regulator_disable(data->vdd);
			return ret;
		}

		data->power_enabled = on;
		printk(KERN_INFO "%s: enable ap3426 power", __func__);
	} else {
		dev_warn(&data->client->dev,
				"Power on=%d. enabled=%d\n",
				on, data->power_enabled);
	}

	return ret;
}

static int ap3426_power_init(struct ap3426_data *data, bool on)
{
	int ret;

	if (!on) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd,
					0, AP3426_VDD_MAX_UV);

		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio,
					0, AP3426_VIO_MAX_UV);

		regulator_put(data->vio);
	} else {
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			ret = PTR_ERR(data->vdd);
			dev_err(&data->client->dev,
				"Regulator get failed vdd ret=%d\n", ret);
			return ret;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			ret = regulator_set_voltage(data->vdd,
					AP3426_VDD_MIN_UV,
					AP3426_VDD_MAX_UV);
			if (ret) {
				dev_err(&data->client->dev,
					"Regulator set failed vdd ret=%d\n",
					ret);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) {
			ret = PTR_ERR(data->vio);
			dev_err(&data->client->dev,
				"Regulator get failed vio ret=%d\n", ret);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0) {
			ret = regulator_set_voltage(data->vio,
					AP3426_VIO_MIN_UV,
					AP3426_VIO_MAX_UV);
			if (ret) {
				dev_err(&data->client->dev,
				"Regulator set failed vio ret=%d\n", ret);
				goto reg_vio_put;
			}
		}
	}

	return 0;

reg_vio_put:
	regulator_put(data->vio);
reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, AP3426_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return ret;
}


static DEVICE_ATTR(range, S_IWUSR | S_IRUGO,
		ap3426_show_range, ap3426_store_range);


/* mode */
static ssize_t ap3426_show_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", ap3426_get_mode(data->client));
}

static ssize_t ap3426_store_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if ((strict_strtoul(buf, 10, &val) < 0) || (val > 7))
		return -EINVAL;

	ret = ap3426_set_mode(data->client, val);

	if (ret < 0)
		return ret;
	LDBG("Starting timer to fire in 200ms (%ld)\n", jiffies);
	ret = mod_timer(&data->pl_timer, jiffies + msecs_to_jiffies(PL_TIMER_DELAY));

	if (ret)
	LDBG("Timer Error\n");
	return count;
}

static DEVICE_ATTR(mode, S_IRUGO | S_IWUGO,
		ap3426_show_mode, ap3426_store_mode);


/* lux */
static ssize_t ap3426_show_lux(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);

	/* No LUX data if power down */
	if (ap3426_get_mode(data->client) == AP3426_SYS_DEV_DOWN)
		return sprintf((char *) buf, "%s\n", "Please power up first!");

	return sprintf(buf, "%d\n", ap3426_get_adc_value(data->client));
}

static DEVICE_ATTR(lux, S_IRUGO, ap3426_show_lux, NULL);


/* Px data */
static ssize_t ap3426_show_pxvalue(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);

	/* No Px data if power down */
	if (ap3426_get_mode(data->client) == AP3426_SYS_DEV_DOWN)
	return -EBUSY;

	return sprintf(buf, "%d\n", ap3426_get_px_value(data->client));
}

static DEVICE_ATTR(pxvalue, S_IRUGO, ap3426_show_pxvalue, NULL);


/* proximity object detect */
static ssize_t ap3426_show_object(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", ap3426_get_object(data->client));
}

static DEVICE_ATTR(object, S_IRUGO, ap3426_show_object, NULL);


/* ALS low threshold */
static ssize_t ap3426_show_althres(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", ap3426_get_althres(data->client));
}

static ssize_t ap3426_store_althres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

	ret = ap3426_set_althres(data->client, val);
	if (ret < 0)
	return ret;

	return count;
}

static DEVICE_ATTR(althres, S_IWUSR | S_IRUGO,
		ap3426_show_althres, ap3426_store_althres);


/* ALS high threshold */
static ssize_t ap3426_show_ahthres(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", ap3426_get_ahthres(data->client));
}

static ssize_t ap3426_store_ahthres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

	ret = ap3426_set_ahthres(data->client, val);
	if (ret < 0)
	return ret;

	return count;
}

static DEVICE_ATTR(ahthres, S_IWUSR | S_IRUGO,
	ap3426_show_ahthres, ap3426_store_ahthres);

/* Px low threshold */
static ssize_t ap3426_show_plthres(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", ap3426_get_plthres(data->client));
}

static ssize_t ap3426_store_plthres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

	ret = ap3426_set_plthres(data->client, val);
	if (ret < 0)
	return ret;

	return count;
}

static DEVICE_ATTR(plthres, S_IWUSR | S_IRUGO,
		ap3426_show_plthres, ap3426_store_plthres);

/* Px high threshold */
static ssize_t ap3426_show_phthres(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", ap3426_get_phthres(data->client));
}

static ssize_t ap3426_store_phthres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

	ret = ap3426_set_phthres(data->client, val);
	if (ret < 0)
	return ret;

	return count;
}

static DEVICE_ATTR(phthres, S_IWUSR | S_IRUGO,
		ap3426_show_phthres, ap3426_store_phthres);


/* calibration */
static ssize_t ap3426_show_calibration_state(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", cali);
}

static ssize_t ap3426_store_calibration_state(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	int stdls, lux;
	char tmp[10];

	LDBG("DEBUG ap3426_store_calibration_state..\n");

	/* No LUX data if not operational */
	if (ap3426_get_mode(data->client) == AP3426_SYS_DEV_DOWN) {
		printk("Please power up first!");
		return -EINVAL;
	}

	cali = 100;
	sscanf(buf, "%d %s", &stdls, tmp);

	if (!strncmp(tmp, "-setcv", 6)) {
		cali = stdls;
		return -EBUSY;
	}

	if (stdls < 0) {
		printk("Std light source: [%d] < 0 !!!\nCheck again, please.\n\
			Set calibration factor to 100.\n", stdls);
		return -EBUSY;
	}

	lux = ap3426_get_adc_value(data->client);
	cali = stdls * 100 / lux;

	return -EBUSY;
}

static DEVICE_ATTR(calibration, S_IWUSR | S_IRUGO,
		ap3426_show_calibration_state, ap3426_store_calibration_state);

#ifdef LSC_DBG
/* engineer mode */
static ssize_t ap3426_em_read(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ap3426_data *data = i2c_get_clientdata(client);
	int i;
	u8 tmp;

	LDBG("DEBUG ap3426_em_read..\n");

	for (i = 0; i < AP3426_NUM_CACHABLE_REGS; i++) {
		tmp = i2c_smbus_read_byte_data(data->client, reg_array[i]);

		printk("Reg[0x%x] Val[0x%x]\n", reg_array[i], tmp);
	}

	return 0;
}

static ssize_t ap3426_em_write(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ap3426_data *data = i2c_get_clientdata(client);
	u32 addr, val;
	int ret = 0;

	LDBG("DEBUG ap3426_em_write..\n");

	sscanf(buf, "%x%x", &addr, &val);

	printk("Write [%x] to Reg[%x]...\n", val, addr);

	ret = i2c_smbus_write_byte_data(data->client, addr, val);
	if (!ret)
		data->reg_cache[ap3426_reg_to_idx_array[addr]] = val;

	return count;
}
static DEVICE_ATTR(em, S_IWUSR | S_IRUGO,
	ap3426_em_read, ap3426_em_write);
#endif

static struct attribute *ap3426_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_mode.attr,
	&dev_attr_lux.attr,
	&dev_attr_object.attr,
	&dev_attr_pxvalue.attr,
	&dev_attr_althres.attr,
	&dev_attr_ahthres.attr,
	&dev_attr_plthres.attr,
	&dev_attr_phthres.attr,
	&dev_attr_calibration.attr,
#ifdef LSC_DBG
	&dev_attr_em.attr,
#endif
	NULL
};

static const struct attribute_group ap3426_attr_group = {
	.attrs = ap3426_attributes,
};

static int ap3426_init_client(struct i2c_client *client)
{
	struct ap3426_data *data = i2c_get_clientdata(client);
	int i;

	i2c_smbus_write_byte_data(client, 0x02, 0x80);


	LDBG("DEBUG ap3426_init_client..\n");
		/*lsensor high low thread*/
	i2c_smbus_write_byte_data(client, 0x1A, 0);
	i2c_smbus_write_byte_data(client, 0x1B, 0);

	i2c_smbus_write_byte_data(client, 0x1C, 0xFF);
	i2c_smbus_write_byte_data(client, 0x1D, 0XFF);
		/*psensor high low thread*/

	i2c_smbus_write_byte_data(client, 0x2A, 0x50);
	i2c_smbus_write_byte_data(client, 0x2B, 0x00);

	i2c_smbus_write_byte_data(client, 0x2C, 0xA0);
	i2c_smbus_write_byte_data(client, 0x2D, 0x00);

	/* read all the registers once to fill the cache.
	 * if one of the reads fails, we consider the init failed */
	for (i = 0; i < AP3426_NUM_CACHABLE_REGS; i++) {
	int v = i2c_smbus_read_byte_data(client, reg_array[i]);
	if (v < 0)
		return -ENODEV;
	data->reg_cache[i] = v;
	}
	/* set defaults */
	ap3426_set_range(client, AP3426_ALS_RANGE_0);
	ap3426_set_mode(client, AP3426_SYS_DEV_DOWN);

	return 0;
}

static int ap3426_check_id(struct ap3426_data *data)
{
	return 0;
}
void pl_timer_callback(unsigned long pl_data)
{
	struct ap3426_data *data;
	int ret = 0;
	data = private_pl_data;

	if (1 == misc_ps_opened) {
		queue_work(data->psensor_wq, &data->psensor_work);
	}

	if (1 == misc_ls_opened) {
		queue_work(data->lsensor_wq, &data->lsensor_work);
	}

	if (1 == misc_ps_opened || 1 == misc_ls_opened)
		ret = mod_timer(&private_pl_data->pl_timer, jiffies + msecs_to_jiffies(PL_TIMER_DELAY));

	if (ret) {
		LDBG("Timer Error\n");
	}

}

static void psensor_work_handler(struct work_struct *w)
{

	struct ap3426_data *data =
	container_of(w, struct ap3426_data, psensor_work);
	int distance, pxvalue;

	distance = ap3426_get_object(data->client);
	pxvalue = ap3426_get_px_value(data->client);

	input_report_abs(data->psensor_input_dev, ABS_DISTANCE, distance);
	input_sync(data->psensor_input_dev);
}

static void lsensor_work_handler(struct work_struct *w)
{

	struct ap3426_data *data =
	container_of(w, struct ap3426_data, lsensor_work);
	int value;
	value = ap3426_get_adc_value(data->client);
	input_report_abs(data->lsensor_input_dev, ABS_MISC, value);
	input_sync(data->lsensor_input_dev);
}

static void ap3426_work_handler(struct work_struct *w)
{

	struct ap3426_data *data =
	container_of(w, struct ap3426_data, ap3426_work);
	u8 int_stat;
	int pxvalue;
	int distance;
	int value;
	int_stat = ap3426_get_intstat(data->client);

	if ((1 == misc_ps_opened) && (int_stat & AP3426_REG_SYS_INT_PMASK)) {
		distance = ap3426_get_object(data->client);
		pxvalue = ap3426_get_px_value(data->client);

		printk("distance=%d pxvalue=%d\n", distance, pxvalue);
		input_report_abs(data->psensor_input_dev, ABS_DISTANCE, distance);
		input_sync(data->psensor_input_dev);
	}

	if (1 == misc_ht_opened) {
		pxvalue = ap3426_get_px_value(data->client);
		input_report_abs(data->hsensor_input_dev, ABS_WHEEL, pxvalue);
		input_sync(data->hsensor_input_dev);
	}


	if ((1 == misc_ls_opened) && (int_stat & AP3426_REG_SYS_INT_AMASK)) {
		value = ap3426_get_adc_value(data->client);
		input_report_abs(data->lsensor_input_dev, ABS_MISC, value);
		input_sync(data->lsensor_input_dev);
	}

	enable_irq(data->client->irq);
}


static irqreturn_t ap3426_irq(int irq, void *data_)
{
	struct ap3426_data *data = data_;

	printk("!!!!!!!!!!!!!!!!!!!!!ap3426_irq !!!!!!!!!!!!!!!!  %d , %d\n", data->client->irq, gpio_to_irq(data->int_pin));
	disable_irq_nosync(data->client->irq);
	queue_work(data->ap3426_wq, &data->ap3426_work);

	return IRQ_HANDLED;
}


#ifdef CONFIG_OF
static int ap3426_parse_dt(struct device *dev, struct ap3426_data *pdata)
{
	struct device_node *dt = dev->of_node;

	if (pdata == NULL) {
		LDBG("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}
	pdata->int_pin = of_get_named_gpio_flags(dt, "ap3426, irq-gpio",
								0, &pdata->irq_flags);
	if (pdata->int_pin < 0) {
		dev_err(dev, "Unable to read irq-gpio\n");
		return pdata->int_pin;
	}
	return 0;
}
#endif

static int ap3426_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ap3426_data *data;
	int err = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		err = -EIO;
		goto exit_free_gpio;
	}

	data = kzalloc(sizeof(struct ap3426_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit_free_gpio;
	}

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		LDBG("Device Tree parsing.");

		err = ap3426_parse_dt(&client->dev, data);
		if (err) {
			dev_err(&client->dev, "%s: ap3426_parse_dt "
					"for pdata failed. err = %d",
					__func__, err);
			goto exit_parse_dt_fail;
		}
	}
#else
	data->irq = client->irq;
#endif


	data->client = client;
	i2c_set_clientdata(client, data);

	err = ap3426_power_init(data, true);
	if (err)
		goto err_power_on;

	err = ap3426_power_ctl(data, true);
	if (err)
		goto err_power_ctl;

	/* initialize the AP3426 chip */
	err = ap3426_init_client(client);
	if (err)
		goto exit_kfree;

	if (ap3426_check_id(data) != 0) {
		dev_err(&client->dev, "failed to check ap3426 id\n");
		goto err_power_on;
	}

	err = ap3426_register_lsensor_device(client, data);
	if (err) {
		dev_err(&client->dev, "failed to register_lsensor_device\n");
		goto exit_kfree;
	}

	err = ap3426_register_psensor_device(client, data);
	if (err) {
		dev_err(&client->dev, "failed to register_psensor_device\n");
		goto exit_free_ls_device;
	}

	err = ap3426_register_heartbeat_sensor_device(client, data);
	if (err) {
		dev_err(&client->dev, "failed to register_heartbeatsensor_device\n");
		goto exit_free_heartbeats_device;
	}
	/* register sysfs hooks */
	if (err)
		goto exit_free_ps_device;

	err = gpio_request(data->int_pin, "ap3426-int");
	if (err < 0) {
		printk(KERN_ERR "%s: gpio_request, err=%d", __func__, err);
		return err;
	}

	err = gpio_direction_input(data->int_pin);
	if (err < 0) {
		printk(KERN_ERR "%s: gpio_direction_input, err=%d", __func__, err);
		return err;
	}

	err = request_threaded_irq(gpio_to_irq(data->int_pin), NULL, ap3426_irq,
		IRQF_TRIGGER_LOW  | IRQF_ONESHOT,
		"ap3426", data);
	if (err) {
		dev_err(&client->dev, "ret: %d, could not get IRQ %d\n", err, gpio_to_irq(data->int_pin));
		goto exit_free_ps_device;
	}

	disable_irq_nosync(data->client->irq);

	data->psensor_wq = create_singlethread_workqueue("psensor_wq");
	if (!data->psensor_wq) {
		LDBG("%s: create psensor_wq workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_wq_failed;
	}
	INIT_WORK(&data->psensor_work, psensor_work_handler);



	data->lsensor_wq = create_singlethread_workqueue("lsensor_wq");
	if (!data->lsensor_wq) {
		LDBG("%s: create lsensor_wq workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_wq_failed;
	}
	INIT_WORK(&data->lsensor_work, lsensor_work_handler);


	setup_timer(&data->pl_timer, pl_timer_callback, 0);

	wake_lock_init(&data->ps_wake_lock, WAKE_LOCK_SUSPEND, "ps_wakelock");

	err = sysfs_create_group(&data->client->dev.kobj, &ap3426_attr_group);


	data->ap3426_wq = create_singlethread_workqueue("ap3426_wq");
	if (!data->ap3426_wq) {
		LDBG("%s: create ap3426_wq workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_wq_failed;
	}
	INIT_WORK(&data->ap3426_work, ap3426_work_handler);


	if (err)
		goto err_power_ctl;

	data->als_cdev = sensors_light_cdev;
	data->als_cdev.sensors_enable = ap3426_als_enable_set;
	data->als_cdev.sensors_poll_delay = ap3426_als_poll_delay_set;
	err = sensors_classdev_register(&client->dev, &data->als_cdev);
	if (err)
		goto exit_pwoer_ctl;

	data->ps_cdev = sensors_proximity_cdev;
	data->ps_cdev.sensors_enable = ap3426_ps_enable_set;
	err = sensors_classdev_register(&client->dev, &data->ps_cdev);
	if (err)
		goto err_power_on;


	private_pl_data = data;

	err = ap3426_power_ctl(data, true);
	if (err)
		goto err_power_on;

	dev_info(&client->dev, "Driver version %s enabled\n", DRIVER_VERSION);


	return 0;
err_create_wq_failed:
	if (&data->pl_timer != NULL)
	del_timer(&data->pl_timer);
	if (data->psensor_wq)
	destroy_workqueue(data->psensor_wq);
	if (data->lsensor_wq)
	destroy_workqueue(data->lsensor_wq);
	if (data->ap3426_wq)
	destroy_workqueue(data->ap3426_wq);
exit_free_ps_device:
	ap3426_unregister_psensor_device(client, data);

exit_free_heartbeats_device:
	ap3426_unregister_heartbeat_device(client, data);
exit_free_ls_device:
	ap3426_unregister_lsensor_device(client, data);
exit_pwoer_ctl:
	ap3426_power_ctl(data, false);
	sensors_classdev_unregister(&data->ps_cdev);

err_power_on:
	ap3426_power_init(data, false);
	sensors_classdev_unregister(&data->als_cdev);

err_power_ctl:

exit_kfree:
	kfree(data);
#ifdef CONFIG_OF
exit_parse_dt_fail:

LDBG("dts initialize failed.");
return err;
/*
	if (client->dev.of_node && data->client->dev.platform_data)
	kfree(data->client->dev.platform_data);
*/
#endif
exit_free_gpio:
	return err;
}

static int ap3426_remove(struct i2c_client *client)
{
	struct ap3426_data *data = i2c_get_clientdata(client);
	free_irq(gpio_to_irq(data->int_pin), data);

	ap3426_power_ctl(data, false);

	sysfs_remove_group(&data->client->dev.kobj, &ap3426_attr_group);

	sysfs_remove_group(&data->psensor_input_dev->dev.kobj, &ap3426_ps_attribute_group);
	sysfs_remove_group(&data->lsensor_input_dev->dev.kobj, &ap3426_ls_attribute_group);

	ap3426_unregister_psensor_device(client, data);
	ap3426_unregister_lsensor_device(client, data);
	ap3426_unregister_heartbeat_device(client, data);

	ap3426_power_init(data, false);


	ap3426_set_mode(client, 0);
	kfree(i2c_get_clientdata(client));

	if (data->psensor_wq)
	destroy_workqueue(data->psensor_wq);
	if (data->lsensor_wq)
	destroy_workqueue(data->lsensor_wq);
	if (data->ap3426_wq)
	destroy_workqueue(data->ap3426_wq);
	if (&data->pl_timer)
	del_timer(&data->pl_timer);
	return 0;
}

static const struct i2c_device_id ap3426_id[] =
{
	{AP3426_DRV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ap3426_id);

#ifdef CONFIG_OF
static struct of_device_id ap3426_match_table[] = {
	{.compatible = "di_ap3426"},
	{},
};
#else
#define ap3426_match_table NULL
#endif

static int ap3426_suspend(struct device *dev)
{


	struct ap3426_data *ps_data = dev_get_drvdata(dev);

	if (misc_ps_opened == 1)
		return 0;

	if (misc_ls_opened == 1) {
		ap3426_ls_enable(ps_data, false);
		ps_data->rels_enable = 1;
	}

	ap3426_power_init(ps_data, false);
	ap3426_power_ctl(ps_data, false);

		return 0;
}

static int ap3426_resume(struct device *dev)
{

	struct ap3426_data *ps_data = dev_get_drvdata(dev);

	if (misc_ps_opened == 1)
		return 0;

	ap3426_power_init(ps_data, true);
	ap3426_power_ctl(ps_data, true);

	if (ps_data->rels_enable == 1) {

		ap3426_init_client(ps_data->client);
		ap3426_ls_enable(ps_data, true);
	}

		return 0;
}

static SIMPLE_DEV_PM_OPS(ap3426_pm_ops, ap3426_suspend, ap3426_resume);

static struct i2c_driver ap3426_driver = {
	.driver = {
		.name	= AP3426_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = ap3426_match_table,
		.pm = &ap3426_pm_ops,
	},
	.probe	= ap3426_probe,
	.remove	= ap3426_remove,
	.id_table = ap3426_id,
};

static int __init ap3426_init(void)
{
	int ret;

	ret = i2c_add_driver(&ap3426_driver);
	return ret;

}

static void __exit ap3426_exit(void)
{
	i2c_del_driver(&ap3426_driver);
}

module_init(ap3426_init);
module_exit(ap3426_exit);
MODULE_AUTHOR("Kevin.dang, <kevin.dang@dyna-image.com>");
MODULE_DESCRIPTION("AP3426 driver.");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

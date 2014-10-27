/******************************************************************************
 * isl29044a.c - Linux kernel module for Intersil isl29044a ambient light sensor
 *				and proximity sensor
 *
 * Copyright 2008-2012 Intersil Inc..
 *
 * DESCRIPTION:
 *	- This is the linux driver for isl29044a.
 *		Kernel version 3.0.8
 *
 * modification history
 * --------------------
 * v1.0   2010/04/06, Shouxian Chen(Simon Chen) create this file
 * v1.1   2012/06/05, Shouxian Chen(Simon Chen) modified for Android 4.0 and
 *			linux 3.0.8
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/idr.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/sensors.h>

/* chip config struct */
struct isl29044a_cfg_t {
	u8 als_range;	/* als range, 0: 125 Lux, 1: 250, 2:2000, 3:4000 */
	u8 ps_lt;		/* ps low limit */
	u8 ps_ht;		/* ps high limit */
	/* led driver current, 0:31.25mA, 1:62.5mA, 2:125mA, 3:250mA*/
	u8 ps_led_drv_cur;
	u8 ps_offset;		/* ps offset comp */
	u8 als_ir_comp;		/* als ir comp */
	int glass_factor;	/* glass factor for als, percent */
};

#define ISL29044A_ADDR	0x44
#define	DEVICE_NAME	"isl29044a"
#define	DRIVER_VERSION	"1.3"

#define ALS_EN_MSK	(1 << 0)
#define PS_EN_MSK	(1 << 1)

#define PS_POLL_TIME	100	 /* unit is ms */

/* POWER SUPPLY VOLTAGE RANGE */
#define ISL_VDD_MIN_UV	2000000
#define ISL_VDD_MAX_UV	3300000
#define ISL_VIO_MIN_UV	1750000
#define ISL_VIO_MAX_UV	1950000
#define ISL29044A_PS_MAX_DELAY	100
#define ISL29044A_ALS_MAX_DELAY	1000
#define PROX_THRESHOLD_DELTA_LO		15
#define PROX_THRESHOLD_DELTA_HI		15

/* Each client has this additional data */
struct isl29044a_data_t {
	struct i2c_client* client;
	struct isl29044a_cfg_t *cfg;
	struct sensors_classdev als_cdev;
	struct sensors_classdev ps_cdev;
	atomic_t als_pwr_status;
	atomic_t ps_pwr_status;
	u8 ps_led_drv_cur;	/* led driver current, 0: 110mA, 1: 220mA */
	atomic_t als_range;	/* als range, 0: 125 Lux, 1: 2000Lux */
	u8 als_mode;		/* als mode, 0: Visible light, 1: IR light */
	u8 ps_lt;		/* ps low limit */
	u8 ps_ht;		/* ps high limit */
	atomic_t poll_delay;	/* poll delay set by hal */
	atomic_t als_delay;
	atomic_t ps_delay;
	atomic_t show_als_raw;	/* show als raw data flag, used for debug */
	atomic_t show_ps_raw;	/* show als raw data flag, used for debug */
	struct timer_list als_timer;	/* als poll timer */
	struct timer_list ps_timer;	/* ps poll timer */
	struct work_struct als_work;
	struct work_struct ps_work;
	struct workqueue_struct *als_wq;
	struct workqueue_struct *ps_wq;
	struct input_dev *als_input_dev;
	struct input_dev *ps_input_dev;
	int last_ps;
	u8 als_range_using;		/* the als range using now */
	u8 als_pwr_before_suspend;
	u8 ps_pwr_before_suspend;
	bool	power_enabled;
	struct regulator *vdd;
	struct regulator *vio;
	atomic_t show_pdata;
	u8 ps_filter_cnt;
	int last_lux;
	int last_ps_raw;

	int als_chg_range_delay_cnt;
	u8 is_do_factory_calib;

	struct cdev cdev;
};

/* Do not scan isl29044a automatic */
static const unsigned short normal_i2c[] = {ISL29044A_ADDR, I2C_CLIENT_END };
static struct sensors_classdev sensors_light_cdev = {
	.name = "isl29044a-light",
	.vendor = "intersil",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "30000",
	.resolution = "0.0125",
	.sensor_power = "0.20",
	.min_delay = 100000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev sensors_proximity_cdev = {
	.name = "isl29044a-proximity",
	.vendor = "intersil",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "3",
	.min_delay = 1000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static void do_als_timer(unsigned long arg)
{
	struct isl29044a_data_t *dev_dat;

	dev_dat = (struct isl29044a_data_t *)arg;

	if (atomic_read(&dev_dat->als_pwr_status) == 0)
		return ;

	/* start a work queue, I cannot do i2c operation in timer context for
	   this context is atomic and i2c function maybe sleep. */
	queue_work(dev_dat->als_wq, &dev_dat->als_work);
}

static void do_ps_timer(unsigned long arg)
{
	struct isl29044a_data_t *dev_dat;

	dev_dat = (struct isl29044a_data_t *)arg;

	if (atomic_read(&dev_dat->ps_pwr_status) == 0)
		return ;

	/* start a work queue, I cannot do i2c operation in timer context for
	   this context is atomic and i2c function maybe sleep. */
	queue_work(dev_dat->ps_wq, &dev_dat->ps_work);
}

static void do_als_work(struct work_struct *work)
{
	struct isl29044a_data_t *dev_dat;
	int ret;
	static int als_dat;
	u8 show_raw_dat;
	int lux;
	u8 als_range;

	dev_dat = container_of(work, struct isl29044a_data_t, als_work);

	show_raw_dat = atomic_read(&dev_dat->show_als_raw);

	als_range = dev_dat->als_range_using;

	ret = i2c_smbus_read_byte_data(dev_dat->client, 0x09);
	if (ret < 0)
		goto err_rd;

	als_dat = (u8)ret;

	ret = i2c_smbus_read_byte_data(dev_dat->client, 0x0a);
	if (ret < 0)
		goto err_rd;

	als_dat = als_dat + ( ((u8)ret & 0x0f) << 8 );

	if (als_range)
		lux = (als_dat * 3200) / 4096;
	else
		lux = (als_dat * 200) / 4096;

	input_report_abs(dev_dat->als_input_dev, ABS_MISC, lux);
	input_sync(dev_dat->als_input_dev);
	if (show_raw_dat)
		dev_info(&dev_dat->als_input_dev->dev,
			"now als raw data is = %d, LUX = %d\n",
			als_dat, lux);

	/* restart timer */
	if (atomic_read(&dev_dat->als_pwr_status) == 0)
		return ;

	dev_dat->als_timer.expires = jiffies +
				(HZ * atomic_read(&dev_dat->poll_delay)) / 1000;
	add_timer(&dev_dat->als_timer);

	return ;

err_rd:
	dev_err(&dev_dat->als_input_dev->dev,
		"Read als sensor error, ret = %d\n", ret);
	return ;
}

static void do_ps_work(struct work_struct *work)
{
	struct isl29044a_data_t *dev_dat;
	int last_ps;
	int ret;
	u8 show_raw_dat;

	dev_dat = container_of(work, struct isl29044a_data_t, ps_work);

	show_raw_dat = atomic_read(&dev_dat->show_ps_raw);

	ret = i2c_smbus_read_byte_data(dev_dat->client, 0x02);
	if (ret < 0)
		goto err_rd;
	last_ps = dev_dat->last_ps;
	dev_dat->last_ps = (ret & 0x80) ? 0 : 1;


	ret = i2c_smbus_read_byte_data(dev_dat->client, 0x08);
	if (ret < 0)
		goto err_rd;

	atomic_set(&dev_dat->show_pdata, ret);

	if (last_ps != dev_dat->last_ps) {
		input_report_abs(dev_dat->ps_input_dev, ABS_DISTANCE,
				dev_dat->last_ps);
		input_sync(dev_dat->ps_input_dev);
		if (show_raw_dat)
			dev_info(&dev_dat->ps_input_dev->dev,
				"ps status changed, now = %d\n",
				dev_dat->last_ps);
	}

	/* restart timer */
	if (atomic_read(&dev_dat->ps_pwr_status) == 0)
		return ;
	dev_dat->ps_timer.expires = jiffies + (HZ * PS_POLL_TIME) / 1000;
	add_timer(&dev_dat->ps_timer);

	return ;

err_rd:
	dev_err(&dev_dat->ps_input_dev->dev, "Read ps sensor error, ret = %d\n",
		ret);
	return ;
}

/* enable to run als */
static int set_sensor_reg(struct isl29044a_data_t *dev_dat)
{
	u8 reg_dat[5];
	int i, ret;
	dev_dbg(&dev_dat->client->dev, "set_sensor_reg()\n");
	reg_dat[2] = 0x22;
	reg_dat[3] = dev_dat->ps_lt;
	reg_dat[4] = dev_dat->ps_ht;

	reg_dat[1] = 0x50;	/* set ps sleep time to 50ms */

	if (atomic_read(&dev_dat->als_pwr_status))
		reg_dat[1] |= 0x04;
	if (atomic_read(&dev_dat->ps_pwr_status))
		reg_dat[1] |= 0x80;

	if (dev_dat->als_mode)
		reg_dat[1] |= 0x01;
	if (atomic_read(&dev_dat->als_range))
		reg_dat[1] |= 0x02;
	if (dev_dat->ps_led_drv_cur)
		reg_dat[1] |= 0x08;

	for (i = 2 ; i <= 4; i++) {
		ret = i2c_smbus_write_byte_data(dev_dat->client, i, reg_dat[i]);
		if (ret < 0) {
			pr_err("set_sensor_reg: write i2c error!!! i2c address is: %x",
				dev_dat->client->addr);
			return ret;
		}
	}

	ret = i2c_smbus_write_byte_data(dev_dat->client, 0x01, reg_dat[1]);
	if (ret < 0) {
		pr_err("set_sensor_reg: write i2c command 0x01 error!!!");
		return ret;
	}
	return 0;
}

/* set power status */
static int set_als_pwr_st(u8 state, struct isl29044a_data_t *dat)
{
	int ret = 0;

	if (state) {
		if (atomic_read(&dat->als_pwr_status))
			return ret;
		atomic_set(&dat->als_pwr_status, 1);
		ret = set_sensor_reg(dat);
		if (ret < 0) {
			dev_err(&dat->als_input_dev->dev,
				"set light sensor reg error, ret = %d\n",
				ret);
			atomic_set(&dat->als_pwr_status, 0);
			return ret;
		}

		/* start timer */
		dat->als_timer.function = &do_als_timer;
		dat->als_timer.data = (unsigned long)dat;
		dat->als_timer.expires = jiffies +
				(HZ * atomic_read(&dat->poll_delay)) / 1000;

		dat->als_range_using = atomic_read(&dat->als_range);
		add_timer(&dat->als_timer);
	} else {
		if (atomic_read(&dat->als_pwr_status) == 0)
			return ret;
		atomic_set(&dat->als_pwr_status, 0);
		ret = set_sensor_reg(dat);

		/* delete timer */
		del_timer_sync(&dat->als_timer);
	}

	return ret;
}

static int set_ps_pwr_st(u8 state, struct isl29044a_data_t *dat)
{
	int ret = 0;

	if (state) {
		if (atomic_read(&dat->ps_pwr_status))
			return ret;
		atomic_set(&dat->ps_pwr_status, 1);

		dat->last_ps = -1;
		ret = set_sensor_reg(dat);
		if (ret < 0) {
			dev_err(&dat->ps_input_dev->dev,
				"set proximity sensor reg error, ret = %d\n",
				ret);
			atomic_set(&dat->ps_pwr_status, 0);
			return ret;
		}

		/* start timer */
		dat->ps_timer.function = &do_ps_timer;
		dat->ps_timer.data = (unsigned long)dat;
		dat->ps_timer.expires = jiffies + (HZ * PS_POLL_TIME) / 1000;
		add_timer(&dat->ps_timer);
	} else {
		if (atomic_read(&dat->ps_pwr_status) == 0)
			return ret;
		atomic_set(&dat->ps_pwr_status, 0);

		ret = set_sensor_reg(dat);

		/* delete timer */
		del_timer_sync(&dat->ps_timer);
	}

	return ret;
}

/* device attribute */
/* enable als attribute */
static ssize_t show_enable_als_sensor(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct isl29044a_data_t *dat;
	u8 pwr_status;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	pwr_status = atomic_read(&dat->als_pwr_status);

	return snprintf(buf, PAGE_SIZE, "%d\n", pwr_status);
}
static ssize_t store_enable_als_sensor(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct isl29044a_data_t *dat;
	ssize_t ret;
	unsigned long val;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	val = kstrtoul(buf, 10, NULL);
	ret = set_als_pwr_st(val, dat);

	if (ret == 0)
		ret = count;
	return ret;
}
static DEVICE_ATTR(enable_als_sensor, S_IRUGO|S_IWUSR|S_IWGRP,
	show_enable_als_sensor, store_enable_als_sensor);

/* enable ps attribute */
static ssize_t show_enable_ps_sensor(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct isl29044a_data_t *dat;
	u8 pwr_status;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	pwr_status = atomic_read(&dat->ps_pwr_status);

	return snprintf(buf, PAGE_SIZE, "%d\n", pwr_status);
}
static ssize_t store_enable_ps_sensor(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct isl29044a_data_t *dat;
	ssize_t ret;
	unsigned long val;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	val = kstrtoul(buf, 10, NULL);
	ret = set_ps_pwr_st(val, dat);

	if (ret == 0)
		ret = count;
	return ret;
}
static DEVICE_ATTR(enable_ps_sensor, S_IRUGO|S_IWUSR|S_IWGRP,
		show_enable_ps_sensor, store_enable_ps_sensor);

/* ps led driver current attribute */
static ssize_t show_ps_led_drv(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct isl29044a_data_t *dat;

	dat = (struct isl29044a_data_t *)dev->platform_data;
	return snprintf(buf, PAGE_SIZE, "%d\n", dat->ps_led_drv_cur);
}
static ssize_t store_ps_led_drv(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct isl29044a_data_t *dat;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	dat = (struct isl29044a_data_t *)dev->platform_data;
	if (val)
		dat->ps_led_drv_cur = 1;
	else
		dat->ps_led_drv_cur = 0;

	return count;
}
static DEVICE_ATTR(ps_led_driver_current, S_IRUGO|S_IWUSR|S_IWGRP,
		show_ps_led_drv, store_ps_led_drv);

/* als range attribute */
static ssize_t show_als_range(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct isl29044a_data_t *dat;
	u8 range;

	dat = (struct isl29044a_data_t *)dev->platform_data;
	range = atomic_read(&dat->als_range);

	return snprintf(buf, PAGE_SIZE, "%d\n", range);
}
static ssize_t store_als_range(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct isl29044a_data_t *dat;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	if (val)
		atomic_set(&dat->als_range, 1);
	else
		atomic_set(&dat->als_range, 0);

	return count;
}
static DEVICE_ATTR(als_range, S_IRUGO|S_IWUSR|S_IWGRP, show_als_range,
		store_als_range);

/* als mode attribute */
static ssize_t show_als_mode(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct isl29044a_data_t *dat;

	dat = (struct isl29044a_data_t *)dev->platform_data;
	return snprintf(buf, PAGE_SIZE, "%d\n", dat->als_mode);
}
static ssize_t store_als_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct isl29044a_data_t *dat;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	dat = (struct isl29044a_data_t *)dev->platform_data;
	if (val)
		dat->als_mode = 1;
	else
		dat->als_mode = 0;

	return count;
}
static DEVICE_ATTR(als_mode, S_IRUGO|S_IWUSR|S_IWGRP, show_als_mode,
		store_als_mode);

/* ps limit range attribute */
static ssize_t show_ps_limit(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct isl29044a_data_t *dat;

	dat = (struct isl29044a_data_t *)dev->platform_data;
	return snprintf(buf, PAGE_SIZE, "%d %d\n", dat->ps_lt, dat->ps_ht);
}
static ssize_t store_ps_limit(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct isl29044a_data_t *dat;
	int lt, ht;

	if (sscanf(buf, "%d %d", &lt, &ht) != 2)
		return -EINVAL;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	if (lt > 255)
		dat->ps_lt = 255;
	else if (lt < 0)
		dat->ps_lt = 0;
	else
		dat->ps_lt = lt;

	if (ht > 255)
		dat->ps_ht = 255;
	else if (ht < 0)
		dat->ps_ht = 0;
	else
		dat->ps_ht = ht;

	return count;
}
static DEVICE_ATTR(ps_limit, S_IRUGO|S_IWUSR|S_IWGRP, show_ps_limit,
		store_ps_limit);

/* poll delay attribute */
static ssize_t show_poll_delay (struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct isl29044a_data_t *dat;
	int delay;

	dat = (struct isl29044a_data_t *)dev->platform_data;
	delay = atomic_read(&dat->poll_delay);

	return snprintf(buf, PAGE_SIZE, "%d\n", delay);
}
static ssize_t store_poll_delay (struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct isl29044a_data_t *dat;
	int64_t ns;
	int delay;

	if (sscanf(buf, "%lld", &ns) != 1)
		return -EINVAL;
	delay = (int)ns/1000/1000;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	if (delay  < 120)
		atomic_set(&dat->poll_delay, 120);
	else if (delay > 65535)
		atomic_set(&dat->poll_delay, 65535);
	else
		atomic_set(&dat->poll_delay, delay);

	return count;
}
static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP, show_poll_delay,
	store_poll_delay);

/* show als raw data attribute */
static ssize_t show_als_show_raw (struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct isl29044a_data_t *dat;
	u8 flag;

	dat = (struct isl29044a_data_t *)dev->platform_data;
	flag = atomic_read(&dat->show_als_raw);

	return snprintf(buf, PAGE_SIZE, "%d\n", flag);
}

static ssize_t store_als_show_raw (struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct isl29044a_data_t *dat;
	int flag;

	if (sscanf(buf, "%d", &flag) != 1)
		return -EINVAL;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	if (flag == 0)
		atomic_set(&dat->show_als_raw, (u8)0);
	else
		atomic_set(&dat->show_als_raw, (u8)1);

	return count;
}
static DEVICE_ATTR(als_show_raw, S_IRUGO|S_IWUSR|S_IWGRP, show_als_show_raw,
	store_als_show_raw);

/* show ps raw data attribute */
static ssize_t show_ps_show_raw (struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct isl29044a_data_t *dat;
	u8 flag;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	flag = atomic_read(&dat->show_pdata);

	return snprintf(buf, PAGE_SIZE, "%d\n", flag);
}

static ssize_t store_ps_show_raw (struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct isl29044a_data_t *dat;
	int flag;

	if (sscanf(buf, "%d", &flag) != 1)
		return -EINVAL;

	dat = (struct isl29044a_data_t *)dev->platform_data;

	if (flag == 0)
		atomic_set(&dat->show_ps_raw, 0);
	else
		atomic_set(&dat->show_ps_raw, 1);

	return count;
}
static DEVICE_ATTR(ps_show_raw, S_IRUGO|S_IWUSR|S_IWGRP, show_ps_show_raw,
	store_ps_show_raw);

static int isl29044a_als_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	int ret = 0;

	struct isl29044a_data_t *dat = container_of(sensors_cdev,
			struct isl29044a_data_t, als_cdev);
	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	ret = set_als_pwr_st(enable, dat);

	return ret;

};

static int isl29044a_ps_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	int ret = 0;

	struct isl29044a_data_t *dat = container_of(sensors_cdev,
			struct isl29044a_data_t, ps_cdev);
	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	ret = set_ps_pwr_st(enable, dat);

	return ret;
};

static int isl29044a_als_poll_delay_enable(struct sensors_classdev *sensors_cdev,
				unsigned int delay_ms)
{
		struct isl29044a_data_t *dat = container_of(sensors_cdev,
			struct isl29044a_data_t, als_cdev);

		if (delay_ms > ISL29044A_ALS_MAX_DELAY)
			delay_ms = ISL29044A_ALS_MAX_DELAY;
		atomic_set(&dat->als_delay, (unsigned int) delay_ms);

		return 0;
}

static int isl29044a_ps_poll_delay_enable(struct sensors_classdev *sensors_cdev,
				unsigned int delay_ms)
{
		struct isl29044a_data_t *dat = container_of(sensors_cdev,
			struct isl29044a_data_t, ps_cdev);

		if (delay_ms > ISL29044A_PS_MAX_DELAY)
			delay_ms = ISL29044A_PS_MAX_DELAY;
		atomic_set(&dat->ps_delay, (unsigned int) delay_ms);

		return 0;
}

static struct attribute *als_attr[] = {
	&dev_attr_enable_als_sensor.attr,
	&dev_attr_als_range.attr,
	&dev_attr_als_mode.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_als_show_raw.attr,
	NULL
};

static struct attribute_group als_attr_grp = {
	.name = "light sensor",
	.attrs = als_attr
};

static struct attribute *ps_attr[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_ps_led_driver_current.attr,
	&dev_attr_ps_limit.attr,
	&dev_attr_ps_show_raw.attr,
	NULL
};

static struct attribute_group ps_attr_grp = {
	.name = "proximity sensor",
	.attrs = ps_attr
};

/* initial and register a input device for sensor */
static int init_input_dev(struct isl29044a_data_t *dev_dat)
{
	int err;
	struct input_dev *als_dev;
	struct input_dev *ps_dev;

	als_dev = input_allocate_device();
	if (!als_dev)
		return -ENOMEM;

	ps_dev = input_allocate_device();
	if (!ps_dev) {
		err = -ENOMEM;
		goto err_free_als;
	}

	als_dev->name = "light";
	als_dev->id.bustype = BUS_I2C;
	als_dev->id.vendor  = 0x0001;
	als_dev->id.product = 0x0001;
	als_dev->id.version = 0x0100;
	als_dev->evbit[0] = BIT_MASK(EV_ABS);
	als_dev->absbit[BIT_WORD(ABS_MISC)] |= BIT_MASK(ABS_MISC);
	als_dev->dev.platform_data = dev_dat;
	input_set_abs_params(als_dev, ABS_MISC, 0, 2000, 0, 0);

	ps_dev->name = "proximity";
	ps_dev->id.bustype = BUS_I2C;
	ps_dev->id.vendor  = 0x0001;
	ps_dev->id.product = 0x0002;
	ps_dev->id.version = 0x0100;
	ps_dev->evbit[0] = BIT_MASK(EV_ABS);
	ps_dev->absbit[BIT_WORD(ABS_DISTANCE)] |= BIT_MASK(ABS_DISTANCE);
	ps_dev->dev.platform_data = dev_dat;
	input_set_abs_params(ps_dev, ABS_DISTANCE, 0, 1, 0, 0);

	err = input_register_device(als_dev);
	if (err)
		goto err_free_als;

	err = input_register_device(ps_dev);
	if (err)
		goto err_free_ps;

	err = sysfs_create_group(&als_dev->dev.kobj, &als_attr_grp);
	if (err) {
		pr_err("isl29044a: device create als file failed\n");
		goto err_free_als_sysfs;
	}

	err = sysfs_create_group(&ps_dev->dev.kobj, &ps_attr_grp);
	if (err) {
		pr_err("isl29044a: device create ps file failed\n");
		goto err_free_ps_sysfs;
	}

	dev_dat->als_input_dev = als_dev;
	dev_dat->ps_input_dev = ps_dev;

	return 0;

err_free_ps_sysfs:
	sysfs_remove_group(&ps_dev->dev.kobj, &ps_attr_grp);
err_free_als_sysfs:
	sysfs_remove_group(&als_dev->dev.kobj, &als_attr_grp);
err_free_ps:
	input_free_device(ps_dev);
err_free_als:
	input_free_device(als_dev);
	pr_err("init_input_dev failed!\n");
	return err;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
static int isl29044a_detect(struct i2c_client *client,
	struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	dev_dbg(&client->dev, "In isl29044a_detect()\n");
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_BYTE_DATA
						| I2C_FUNC_SMBUS_READ_BYTE)) {
		pr_warn("I2c adapter don't support ISL29044A\n");
		return -ENODEV;
	}

	/* probe that if isl29044a is at the i2 address */
	if (i2c_smbus_xfer(adapter, client->addr, 0, I2C_SMBUS_WRITE,
						0, I2C_SMBUS_QUICK, NULL) < 0)
		return -ENODEV;

	strlcpy(info->type, "isl29044a", I2C_NAME_SIZE);
	pr_info("%s is found at i2c device address %d\n", info->type,
		client->addr);

	pr_info("isl29044a_detect OK!\n");
	return 0;
}

static int isl_power_on(struct isl29044a_data_t *data, bool on)
{
	int rc = 0;

	if (!on && data->power_enabled) {
		rc = regulator_disable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(data->vio);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vio disable failed rc=%d\n", rc);
			rc = regulator_enable(data->vdd);
		}

		data->power_enabled = false;
	} else if (on && !data->power_enabled) {
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(data->vio);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vio enable failed rc=%d\n", rc);
			regulator_disable(data->vdd);
		}

		data->power_enabled = true;
	} else {
		dev_warn(&data->client->dev,
				"Power on=%d. enabled=%d\n",
				on, data->power_enabled);
	}

	return rc;
}

static int isl_power_init(struct isl29044a_data_t *data, bool on)
{
	int rc;

	if (!on) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0, ISL_VDD_MAX_UV);

		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0, ISL_VIO_MAX_UV);

		regulator_put(data->vio);
	} else {
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			rc = PTR_ERR(data->vdd);
			dev_err(&data->client->dev,
				"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			rc = regulator_set_voltage(data->vdd, ISL_VDD_MIN_UV,
						   ISL_VDD_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,
					"Regulator set failed vdd rc=%d\n",
					rc);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) {
			rc = PTR_ERR(data->vio);
			dev_err(&data->client->dev,
				"Regulator get failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0) {
			rc = regulator_set_voltage(data->vio, ISL_VIO_MIN_UV,
						   ISL_VIO_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,
				"Regulator set failed vio rc=%d\n", rc);
				goto reg_vio_put;
			}
		}
	}

	return 0;
reg_vio_put:
	regulator_put(data->vio);
reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, ISL_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
}

static int sensor_parse_dt(struct device *dev, struct isl29044a_cfg_t *cfg)
{
	struct device_node *np = dev->of_node;
	unsigned int tmp;
	int rc = 0;

	rc = of_property_read_u32(np, "intersil,als-range", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read als-range\n");
		return rc;
	}
	cfg->als_range = tmp;

	rc = of_property_read_u32(np, "intersil,ps-ht", &tmp);
	 if (rc) {
		dev_err(dev, "Unable to read intersil,ps-ht\n");
		return rc;
	}
	cfg->ps_ht = tmp;

	rc = of_property_read_u32(np, "intersil,ps-lt", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read intersil,ps-lt\n");
		return rc;
	}
	cfg->ps_lt = tmp;

	return 0;
}

/* isl29044a probed */
static int isl29044a_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err, i,ret1,ret2;
	u8 reg_dat[8];
	struct isl29044a_data_t *isl29044a_data;
	struct isl29044a_cfg_t *cfgdata;

	if (client->dev.of_node) {
		cfgdata = devm_kzalloc(&client->dev,
				sizeof(struct isl29044a_cfg_t),
				GFP_KERNEL);
		if (!cfgdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		client->dev.platform_data = cfgdata;
		err = sensor_parse_dt(&client->dev, cfgdata);
		if (err) {
			pr_err("%s: sensor_parse_dt() err\n", __func__);
			return err;
		}
	} else {
		cfgdata = client->dev.platform_data;
		if (!cfgdata) {
			dev_err(&client->dev, "No platform data\n");
			return -ENODEV;
		}
	}
	/* initial device data struct */
	isl29044a_data = devm_kzalloc(&client->dev,
				sizeof(struct isl29044a_data_t),
				GFP_KERNEL);
	if (!isl29044a_data) {
		dev_err(&client->dev,
			"failed to allocate memory for module data:""%d\n",
			err);
		return -ENOMEM;
	}

	isl29044a_data->cfg = cfgdata;
	isl29044a_data->client = client;

	atomic_set(&isl29044a_data->als_pwr_status, 0);
	atomic_set(&isl29044a_data->ps_pwr_status, 0);
	isl29044a_data->ps_led_drv_cur = 0;
	atomic_set(&isl29044a_data->als_range, cfgdata->als_range);
	isl29044a_data->ps_lt = cfgdata->ps_lt;
	isl29044a_data->ps_ht = cfgdata->ps_ht;
	atomic_set(&isl29044a_data->poll_delay, 100);
	atomic_set(&isl29044a_data->show_als_raw, 0);
	atomic_set(&isl29044a_data->show_ps_raw, 0);

	INIT_WORK(&isl29044a_data->als_work, &do_als_work);
	INIT_WORK(&isl29044a_data->ps_work, &do_ps_work);
	init_timer(&isl29044a_data->als_timer);
	init_timer(&isl29044a_data->ps_timer);

	isl29044a_data->als_wq = create_workqueue("als wq");
	if (!isl29044a_data->als_wq) {
		destroy_workqueue(isl29044a_data->als_wq);
		return -ENOMEM;
	}

	isl29044a_data->ps_wq = create_workqueue("ps wq");
	if (!isl29044a_data->ps_wq) {
		destroy_workqueue(isl29044a_data->ps_wq);
		return -ENOMEM;
	}

	i2c_set_clientdata(client,isl29044a_data);

	ret1 = isl_power_init(isl29044a_data,true);
	if (ret1 < 0) {
		dev_err(&client->dev, "%s:isl29044 power init error!\n",
			__func__);
	}

	ret2 = isl_power_on(isl29044a_data,true);
	if (ret2 < 0) {
		dev_err(&client->dev, "%s:isl29044 power on error!\n",
			__func__);
	}

	/* initial isl29044a */
	err = set_sensor_reg(isl29044a_data);
	if (err < 0) {
		pr_err("isl29044 set_sensor_reg error\n");
		return err;
	}
	/* initial als interrupt limit to low = 0, high = 4095, so als cannot
	   trigger a interrupt. We use ps interrupt only */
	reg_dat[5] = 0x00;
	reg_dat[6] = 0xf0;
	reg_dat[7] = 0xff;
	for (i = 5; i <= 7; i++) {
		err = i2c_smbus_write_byte_data(client, i, reg_dat[i]);
		if (err < 0) {
			dev_err(&client->dev,
				"isl29044 write i2c error in probe.\n");
			return err;
		}
	}

	/* Add input device register here */
	err = init_input_dev(isl29044a_data);
	if (err < 0) {
		dev_err(&client->dev,
			"isl29044 init_input_dev error in probe.\n");
		destroy_workqueue(isl29044a_data->als_wq);
		destroy_workqueue(isl29044a_data->ps_wq);
		return -ENOMEM;
	}

	isl29044a_data->als_cdev = sensors_light_cdev;
	isl29044a_data->als_cdev.sensors_enable = isl29044a_als_set_enable;
	isl29044a_data->als_cdev.sensors_poll_delay =
		isl29044a_als_poll_delay_enable;

	isl29044a_data->ps_cdev = sensors_proximity_cdev;
	isl29044a_data->ps_cdev.sensors_enable = isl29044a_ps_set_enable;
	isl29044a_data->ps_cdev.sensors_poll_delay =
		isl29044a_ps_poll_delay_enable;

	err = sensors_classdev_register(&client->dev,
					&isl29044a_data->als_cdev);
	if (err)
		dev_err(&client->dev,
			"create als_cdev class device file failed!\n");

	err = sensors_classdev_register(&client->dev, &isl29044a_data->ps_cdev);
	if (err) {
		dev_err(&client->dev,
			"create ps_cdev class device file failed!\n");
		err = -EINVAL;
	}

	return err;
}

static int isl29044a_remove(struct i2c_client *client)
{
	struct input_dev *als_dev;
	struct input_dev *ps_dev;
	struct isl29044a_data_t *isl29044a_data = i2c_get_clientdata(client);
	pr_info("%s at address %d is removed\n", client->name, client->addr);

	/* clean the isl29044a data struct when isl29044a device remove */
	isl29044a_data->client = NULL;
	atomic_set(&isl29044a_data->als_pwr_status, 0);
	atomic_set(&isl29044a_data->ps_pwr_status, 0);

	als_dev = isl29044a_data->als_input_dev;
	ps_dev = isl29044a_data->ps_input_dev;

	sysfs_remove_group(&als_dev->dev.kobj, &als_attr_grp);
	sysfs_remove_group(&ps_dev->dev.kobj, &ps_attr_grp);

	input_unregister_device(als_dev);
	input_unregister_device(ps_dev);
	sensors_classdev_unregister(&isl29044a_data->als_cdev);
	sensors_classdev_unregister(&isl29044a_data->ps_cdev);

	destroy_workqueue(isl29044a_data->ps_wq);
	destroy_workqueue(isl29044a_data->als_wq);
	isl29044a_data->als_input_dev = NULL;
	isl29044a_data->ps_input_dev = NULL;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
/* if define power manager, define suspend and resume function */
static int isl29044a_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct isl29044a_data_t *dat = i2c_get_clientdata(client);
	int ret;

	dat->als_pwr_before_suspend = atomic_read(&dat->als_pwr_status);
	ret = set_als_pwr_st(0, dat);
	if (ret < 0)
		dev_err(dev, "%s:could not set ALS power state.\n", __func__);

	return 0;
}

static int isl29044a_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct isl29044a_data_t *dat = i2c_get_clientdata(client);
	int ret;

	ret = set_als_pwr_st(dat->als_pwr_before_suspend, dat);
	if (ret < 0)
		return ret;

	ret = set_ps_pwr_st(dat->ps_pwr_before_suspend, dat);
	if (ret < 0)
		return ret;

	return 0;
}
#else
#define	isl29044a_suspend 	NULL
#define isl29044a_resume	NULL
#endif		/*ifdef CONFIG_PM_SLEEP end*/

static SIMPLE_DEV_PM_OPS(isl29044a_pm_ops, isl29044a_suspend, isl29044a_resume);

static const struct i2c_device_id isl29044a_id[] = {
	{ "isl29044a", 0 },
	{ }
};

static struct of_device_id intersil_match_table[] = {
            { .compatible = "intersil,isl29044a",},
            { },
    };

static struct i2c_driver isl29044a_driver = {
	.driver = {
		.name = "isl29044a",
		.pm = &isl29044a_pm_ops,
		.of_match_table = intersil_match_table,
	},
	.probe			= isl29044a_probe,
	.remove			= isl29044a_remove,
	.id_table		= isl29044a_id,
	.detect			= isl29044a_detect,
	.address_list	= normal_i2c,
};

struct i2c_client *isl29044a_client;

static int __init isl29044a_init(void)
{
	int ret;

	/* register the i2c driver for isl29044a */
	ret = i2c_add_driver(&isl29044a_driver);
	if (ret < 0)
		pr_err("Add isl29044a driver error, ret = %d\n", ret);
	pr_debug("init isl29044a module\n");

	return ret;
}

static void __exit isl29044a_exit(void)
{
	pr_debug("exit isl29044a module\n");
	i2c_del_driver(&isl29044a_driver);
}

MODULE_AUTHOR("Chen Shouxian");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("isl29044a ambient light sensor driver");
MODULE_VERSION(DRIVER_VERSION);

module_init(isl29044a_init);
module_exit(isl29044a_exit);

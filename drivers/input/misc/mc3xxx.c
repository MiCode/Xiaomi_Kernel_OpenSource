/*****************************************************************************
 *
 * Copyright (c) 2013-2014 mCube, Inc.  All rights reserved.
 *
 * This source is subject to the mCube Software License.
 * This software is protected by Copyright and the information and source code
 * contained herein is confidential. The software including the source code
 * may not be copied and the information contained herein may not be used or
 * disclosed except with the written permission of mCube Inc.
 *
 * All other rights reserved.
 *
 * This code and information are provided "as is" without warranty of any
 * kind, either expressed or implied, including but not limited to the
 * implied warranties of merchantability and/or fitness for a
 * particular purpose.
 *
 * The following software/firmware and/or related documentation
 * ("mCube Software") have been modified by mCube Inc. All revisions are
 * subject to any receiver's applicable license agreements with mCube Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/sensors.h>
#include <asm/page.h>

#include <linux/miscdevice.h>

#include <linux/fs.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef MCUBE_FUNC_DEBUG
    #define MC_PRINT(x...)        pr_info(x)
#else
    #define MC_PRINT(x...)
#endif

#define MC3XXX_I2C_ADDR			0x4c
#define MC3XXX_DEV_NAME			"mc3xxx"
#define MC3XXX_INPUT_NAME		"accelerometer"
#define MC3XXX_DRIVER_VERSION		"1.0.0"
#define MC3XXX_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define MC3XXX_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

/* Register address define */
#define MC3XXX_REG_XOUT			0x00
#define MC3XXX_REG_INTEN		0x06
#define MC3XXX_REG_MODE			0x07
#define MC3XXX_REG_SRFA			0x08
#define MC3XXX_REG_XOUT_EX_L		0x0D
#define MC3XXX_REG_OUTCFG		0x20
#define MC3XXX_REG_XOFFL		0x21
#define MC3XXX_REG_PCODE		0x3B

/* Mode */
#define MC3XXX_MODE_AUTO		0
#define MC3XXX_MODE_WAKE		1
#define MC3XXX_MODE_SNIFF		2
#define MC3XXX_MODE_STANDBY		3

/* Range */
#define MC3XXX_RANGE_2G			0
#define MC3XXX_RANGE_4G			1
#define MC3XXX_RANGE_8G_10BIT		2
#define MC3XXX_RANGE_8G_14BIT		3

/* Bandwidth */
#define MC3XXX_BW_512HZ			0
#define MC3XXX_BW_256HZ			1
#define MC3XXX_BW_128HZ			2
#define MC3XXX_BW_64HZ			3
#define MC3XXX_BW_32HZ			4
#define MC3XXX_BW_16HZ			5
#define MC3XXX_BW_8HZ			6

/* initial value */
#define MC3XXX_RANGE_SET	MC3XXX_RANGE_8G_14BIT  /* +/-8g, 14bit */
#define MC3XXX_BW_SET		MC3XXX_BW_128HZ /* 128HZ  */
#define MC3XXX_MAX_DELAY		200
#define MC3XXX_MIN_DELAY		50
#define ABSMIN				(-8 * 1024)
#define ABSMAX				(8 * 1024)

/* product code */
#define MC3XXX_PCODE_3210		0x90
#define MC3XXX_PCODE_3230		0x19
#define MC3XXX_PCODE_3250		0x88
#define MC3XXX_PCODE_3410		0xA8
#define MC3XXX_PCODE_3410N		0xB8
#define MC3XXX_PCODE_3430		0x29
#define MC3XXX_PCODE_3430N		0x39
#define MC3XXX_PCODE_3510B		0x40
#define MC3XXX_PCODE_3530B		0x30
#define MC3XXX_PCODE_3510C		0x10
#define MC3XXX_PCODE_3530C		0x60

/* 1g constant value */
#define GRAVITY_1G_VALUE		1000

/* Polling delay in msecs */
#define POLL_INTERVAL_MIN	10
#define POLL_INTERVAL_MAX	10000
#define POLL_INTERVAL		1 /* msecs */

#define IS_MC35XX() \
	((MC3XXX_PCODE_3510B == s_bPCODE) || \
	(MC3XXX_PCODE_3510C == s_bPCODE) || \
	(MC3XXX_PCODE_3530B == s_bPCODE) || \
	(MC3XXX_PCODE_3530C == s_bPCODE))

enum mc3xxx_orientation {
	MC3XXX_TOP_LEFT_DOWN = 0,	/* 0: top, left-down */
	MC3XXX_TOP_RIGHT_DOWN,		/* 1: top, reight-down */
	MC3XXX_TOP_RIGHT_UP,		/* 2: top, right-up */
	MC3XXX_TOP_LEFT_UP,		/* 3: top, left-up */
	MC3XXX_BOTTOM_LEFT_DOWN,	/* 4: bottom, left-down */
	MC3XXX_BOTTOM_RIGHT_DOWN,	/* 5: bottom, right-down */
	MC3XXX_BOTTOM_RIGHT_UP,		/* 6: bottom, right-up */
	MC3XXX_BOTTOM_LEFT_UP		/* 7: bottom, left-up */
};

struct mc3xxx_hwmsen_convert {
	signed char sign[3];
	unsigned char map[3];
};

enum mc3xxx_axis {
	MC3XXX_AXIS_X = 0,
	MC3XXX_AXIS_Y,
	MC3XXX_AXIS_Z,
	MC3XXX_AXIS_NUM
};

struct mc3xxxacc {
	signed short x, y, z;
};

struct mc3xxx_platform_data {
	int position;
};

struct mc3xxx_data {
	struct i2c_client *mc3xxx_client;
	struct mc3xxx_platform_data *pdata;
	atomic_t delay;
	atomic_t enable;
	atomic_t selftest_result;
	struct input_dev *input;
	struct mc3xxxacc value;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct delayed_work work;
	struct work_struct irq_work;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int IRQ;
	unsigned char mode;
	unsigned char orientation;
	unsigned char data_trace_enable;
	/* unsigned int gain; */
	struct sensors_classdev cdev;
};

static struct sensors_classdev sensors_cdev = {
	.name = "mc3430",
	.vendor = "mCube",
	.version = 1,
	.handle = 0,
	.type = 1,
	.max_range = "78.4",
	.resolution = "0.001",
	.sensor_power = "0.2",
	.min_delay = 2000,	/* microsecond */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,	/* millisecond */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static union{
	unsigned short dirty_addr_buf[2];
	const unsigned short normal_i2c[2];
} u_i2c_addr = {{0x00},};

static unsigned char s_bPCODE;
static unsigned short s_uiGain;

static const struct mc3xxx_hwmsen_convert mc3xxx_cvt[] = {
	/* 0: top, left-down */
	{{1, 1, 1}, { MC3XXX_AXIS_X, MC3XXX_AXIS_Y, MC3XXX_AXIS_Z} },
	/* 1: top, right-down */
	{{ -1, 1, 1}, { MC3XXX_AXIS_Y, MC3XXX_AXIS_X, MC3XXX_AXIS_Z} },
	/* 2: top, right-up */
	{{ -1, -1, 1}, { MC3XXX_AXIS_X, MC3XXX_AXIS_Y, MC3XXX_AXIS_Z} },
	/* 3: top, left-up */
	{{ 1, -1, 1}, { MC3XXX_AXIS_Y, MC3XXX_AXIS_X, MC3XXX_AXIS_Z} },
	/* 4: bottom, left-down */
	{{ -1, 1, -1}, { MC3XXX_AXIS_X, MC3XXX_AXIS_Y, MC3XXX_AXIS_Z} },
	/* 5: bottom, right-down */
	{{ 1, 1, -1}, { MC3XXX_AXIS_Y, MC3XXX_AXIS_X, MC3XXX_AXIS_Z} },
	/* 6: bottom, right-up */
	{{ 1, -1, -1}, { MC3XXX_AXIS_X,  MC3XXX_AXIS_Y,  MC3XXX_AXIS_Z} },
	/* 7: bottom, left-up */
	{{ -1, -1, -1}, { MC3XXX_AXIS_Y,  MC3XXX_AXIS_X,  MC3XXX_AXIS_Z} },
};

static const unsigned short mc3xxx_data_resolution[4] = {
	256, /* +/- 2g, 10bit */
	128, /* +/- 4g, 10bit */
	64,  /* +/- 8g, 10bit */
	1024 /* +/- 8g, 14bit */
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mc3xxx_early_suspend(struct early_suspend *h);
static void mc3xxx_early_resume(struct early_suspend *h);
#endif

static int mc3xxx_smbus_read_byte(struct i2c_client *client,
				  unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0)
		return -EINVAL;
	*data = dummy & 0x000000ff;

	return 0;
}

static int mc3xxx_smbus_write_byte(struct i2c_client *client,
				   unsigned char reg_addr, unsigned char data)
{
	s32 dummy;


	dummy = i2c_smbus_write_byte_data(client, reg_addr, data);

	if (dummy < 0)
		return -EINVAL;

	return 0;
}

static int mc3xxx_smbus_read_block(struct i2c_client *client,
					unsigned char reg_addr,
					unsigned char len, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return -EINVAL;
	return 0;
}

static bool mc3xxx_validate_pcode(unsigned char bPCode)
{
	if ((MC3XXX_PCODE_3210  == bPCode) || (MC3XXX_PCODE_3230  == bPCode) ||
		(MC3XXX_PCODE_3250  == bPCode) || (MC3XXX_PCODE_3410  == bPCode)
		|| (MC3XXX_PCODE_3430  == bPCode) ||
		(MC3XXX_PCODE_3410N == bPCode) || (MC3XXX_PCODE_3430N == bPCode)
		|| (MC3XXX_PCODE_3510B == bPCode) ||
		(MC3XXX_PCODE_3530B == bPCode) || (MC3XXX_PCODE_3510C == bPCode)
		|| (MC3XXX_PCODE_3530C == bPCode))
		return true;

	return false;
}

static bool mc3xxx_is_high_end(unsigned char pcode)
{
	if ((MC3XXX_PCODE_3230 == pcode) || (MC3XXX_PCODE_3430 == pcode) ||
	     (MC3XXX_PCODE_3430N == pcode) || (MC3XXX_PCODE_3530B == pcode) ||
	     (MC3XXX_PCODE_3530C == pcode))
		return false;
	else
		return true;
}

static bool mc3xxx_is_mc3510(unsigned char pcode)
{
	if ((MC3XXX_PCODE_3510B == pcode) || (MC3XXX_PCODE_3510C == pcode))
		return true;
	else
		return false;
}

static bool mc3xxx_is_mc3530(unsigned char pcode)
{
	if ((MC3XXX_PCODE_3530B == pcode) || (MC3XXX_PCODE_3530C == pcode))
		return true;
	else
		return false;
}

static int mc3xxx_set_mode(struct i2c_client *client, unsigned char mode)
{
	int comres = 0;
	unsigned char data;

	MC_PRINT("%s called\n", __func__);

	if (4 > mode) {
		data = 0x40 | mode;
		comres = mc3xxx_smbus_write_byte(client, MC3XXX_REG_MODE, data);
	} else
		comres = -1;

	return comres;
}

static int mc3xxx_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres = 0;

	MC_PRINT("%s called\n", __func__);

	comres = mc3xxx_smbus_read_byte(client, MC3XXX_REG_MODE, mode);
	*mode &= 0x03;

	return comres;
}

static int mc3xxx_set_range(struct i2c_client *client, unsigned char range)
{
	int comres = 0;
	unsigned char data = 0;

	MC_PRINT("%s called\n", __func__);

	if (4 > range) {
		if (mc3xxx_is_mc3510(s_bPCODE)) {
			data = 0x25;
			comres = mc3xxx_smbus_write_byte(client,
						MC3XXX_REG_OUTCFG, data);
			if (0 == comres)
				s_uiGain = 1024;
			return comres;
		} else if (mc3xxx_is_mc3530(s_bPCODE))	{
			data = 0x02;
			comres = mc3xxx_smbus_write_byte(client,
						MC3XXX_REG_OUTCFG, data);
			if (0 == comres)
				s_uiGain = 64;
			return comres;
		}

		if (mc3xxx_is_high_end(s_bPCODE)) {
			data = (range << 2) | 0x33;
			comres = mc3xxx_smbus_write_byte(client,
						MC3XXX_REG_OUTCFG, data);
			if (0 == comres)
				s_uiGain = mc3xxx_data_resolution[range];
		} else {
			/* data = 0x32; */
			s_uiGain = 86;
		}
	} else
		comres = -1;

	return comres;
}

static int mc3xxx_get_range(struct i2c_client *client, unsigned char *range)
{
	int comres = 0;
	unsigned char data;

	MC_PRINT("%s called\n", __func__);

	comres = mc3xxx_smbus_read_byte(client, MC3XXX_REG_OUTCFG, &data);
	*range = ((data >> 2) & 0x03);

	return comres;
}

static int mc3xxx_set_bandwidth(struct i2c_client *client, unsigned char BW)
{
	int comres = 0;
	unsigned char data = 0;

	MC_PRINT("%s called\n", __func__);

	if (7 > BW) {
		comres = mc3xxx_smbus_read_byte(client,
					MC3XXX_REG_OUTCFG, &data);
		data &= ~(0x07 << 4);
		data |= (BW << 4);
		comres += mc3xxx_smbus_write_byte(client,
					MC3XXX_REG_OUTCFG, data);
	} else
		comres = -1;

	return comres;
}

static int mc3xxx_get_bandwidth(struct i2c_client *client, unsigned char *BW)
{
	int comres = 0;
	unsigned char data = 0;

	MC_PRINT("%s called\n", __func__);

	comres = mc3xxx_smbus_read_byte(client, MC3XXX_REG_OUTCFG, &data);
	*BW = ((data >> 4) & 0x07);

	return comres;
}

static int mc3xxx_read_accel_xyz(struct mc3xxx_data *mc3xxx,
				 struct mc3xxxacc *acc)
{
	int comres;
	unsigned char data[6];
	signed short raw[3] = { 0 };
	const struct mc3xxx_hwmsen_convert *pCvt;

	if (true == mc3xxx_is_high_end(s_bPCODE)) {
		comres = mc3xxx_smbus_read_block(mc3xxx->mc3xxx_client,
					MC3XXX_REG_XOUT_EX_L, 6, data);
		raw[0] = (signed short)(data[0] + (data[1] << 8));
		raw[1] = (signed short)(data[2] + (data[3] << 8));
		raw[2] = (signed short)(data[4] + (data[5] << 8));
	} else {
		comres = mc3xxx_smbus_read_block(mc3xxx->mc3xxx_client,
					MC3XXX_REG_XOUT, 3, data);
		raw[0] = (signed char)data[0];
		raw[1] = (signed char)data[1];
		raw[2] = (signed char)data[2];
	}

	if (comres) {
		pr_err("%s: i2c error!\n", __func__);
		return comres;
	}

	if (mc3xxx->data_trace_enable)
		pr_info("%s: %d, %d, %d\n", __func__, raw[0], raw[1], raw[2]);
	else
		MC_PRINT("%s: %d, %d, %d\n", __func__, raw[0], raw[1], raw[2]);

	if (MC3XXX_PCODE_3250 == s_bPCODE) {
		int _nTemp = 0;
		_nTemp = raw[0];
		raw[0] = raw[1];
		raw[1] = -_nTemp;
	}
	if ((MC3XXX_PCODE_3410N == s_bPCODE) ||
		(MC3XXX_PCODE_3430N == s_bPCODE)) {
		raw[0] = -raw[0];
		raw[1] = -raw[1];
	}
	if (IS_MC35XX()) {
		raw[0] = -raw[0];
		raw[1] = -raw[1];
	}

	pCvt = &mc3xxx_cvt[mc3xxx->orientation];
	acc->x = pCvt->sign[MC3XXX_AXIS_X] * raw[pCvt->map[MC3XXX_AXIS_X]];
	acc->y = pCvt->sign[MC3XXX_AXIS_Y] * raw[pCvt->map[MC3XXX_AXIS_Y]];
	acc->z = pCvt->sign[MC3XXX_AXIS_Z] * raw[pCvt->map[MC3XXX_AXIS_Z]];


	acc->x = acc->x * GRAVITY_1G_VALUE / s_uiGain;
	acc->y = acc->y * GRAVITY_1G_VALUE / s_uiGain;
	acc->z = acc->z * GRAVITY_1G_VALUE / s_uiGain;

	return comres;
}

static void mc3xxx_work_func(struct work_struct *work)
{
	struct mc3xxx_data *mc3xxx = container_of((struct delayed_work *)work,
						  struct mc3xxx_data, work);
	static struct mc3xxxacc acc;
	unsigned long delay = msecs_to_jiffies(atomic_read(&mc3xxx->delay));

	mc3xxx_read_accel_xyz(mc3xxx, &acc);
	input_report_abs(mc3xxx->input, ABS_X, -acc.y);
	input_report_abs(mc3xxx->input, ABS_Y, -acc.x);
	input_report_abs(mc3xxx->input, ABS_Z, -acc.z);

	input_sync(mc3xxx->input);
	mutex_lock(&mc3xxx->value_mutex);
	mc3xxx->value = acc;
	mutex_unlock(&mc3xxx->value_mutex);
	schedule_delayed_work(&mc3xxx->work, delay);
}

static ssize_t mc3xxx_register_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int address, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	sscanf(buf, "%d%d", &address, &value);

	if (mc3xxx_smbus_write_byte(mc3xxx->mc3xxx_client,
			(unsigned char)address, (unsigned char)value) < 0)
		return -EINVAL;

	return count;
}
static ssize_t mc3xxx_register_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	size_t count = 0;
	u8 reg[0x3f];
	int i;

	for (i = 0; i < 0x3f; i++) {
		mc3xxx_smbus_read_byte(mc3xxx->mc3xxx_client, i, reg + i);

		count += snprintf(&buf[count], sizeof(buf[count]),
				"0x%x: %d\n", i, reg[i]);
	}
	return count;

}
static ssize_t mc3xxx_range_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	if (mc3xxx_get_range(mc3xxx->mc3xxx_client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", data);
}

static ssize_t mc3xxx_range_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (mc3xxx_set_range(mc3xxx->mc3xxx_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t mc3xxx_bandwidth_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	if (mc3xxx_get_bandwidth(mc3xxx->mc3xxx_client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", data);

}

static ssize_t mc3xxx_bandwidth_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (mc3xxx_set_bandwidth(mc3xxx->mc3xxx_client,
				 (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t mc3xxx_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	if (mc3xxx_get_mode(mc3xxx->mc3xxx_client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", data);
}

static ssize_t mc3xxx_mode_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (mc3xxx_set_mode(mc3xxx->mc3xxx_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t mc3xxx_value_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct mc3xxx_data *mc3xxx = input_get_drvdata(input);
	struct mc3xxxacc acc_value;

	MC_PRINT("%s called\n", __func__);

	mutex_lock(&mc3xxx->value_mutex);
	acc_value = mc3xxx->value;
	mutex_unlock(&mc3xxx->value_mutex);

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n",
				acc_value.x, acc_value.y, acc_value.z);
}

static ssize_t mc3xxx_delay_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&mc3xxx->delay));

}

static ssize_t mc3xxx_delay_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (data > MC3XXX_MAX_DELAY)
		data = MC3XXX_MAX_DELAY;
	else if (data < MC3XXX_MIN_DELAY)
		data = MC3XXX_MIN_DELAY;
	atomic_set(&mc3xxx->delay, (unsigned int)data);

	return count;
}

static ssize_t mc3xxx_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&mc3xxx->enable));

}

static void mc3xxx_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&mc3xxx->enable);

	MC_PRINT("%s called\n", __func__);

	mutex_lock(&mc3xxx->enable_mutex);
	if (enable) {
		if (pre_enable == 0) {
			mc3xxx_set_mode(mc3xxx->mc3xxx_client,
					MC3XXX_MODE_WAKE);
			schedule_delayed_work(&mc3xxx->work,
					      msecs_to_jiffies(atomic_read
							       (&mc3xxx->
								delay)));
			atomic_set(&mc3xxx->enable, 1);
		}

	} else {
		if (pre_enable == 1) {
			mc3xxx_set_mode(mc3xxx->mc3xxx_client,
					MC3XXX_MODE_STANDBY);
			cancel_delayed_work_sync(&mc3xxx->work);
			atomic_set(&mc3xxx->enable, 0);
		}
	}
	mutex_unlock(&mc3xxx->enable_mutex);

}

static ssize_t mc3xxx_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned long data;
	int error;

	MC_PRINT("%s called\n", __func__);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if ((data == 0) || (data == 1))
		mc3xxx_set_enable(dev, data);

	return count;
}

static ssize_t mc3xxx_orientation_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%d\n", mc3xxx->orientation);
}

static ssize_t mc3xxx_orientation_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (8 > data)
		mc3xxx->orientation = data;

	return count;
}

static ssize_t mc3xxx_dte_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);
	return snprintf(buf, PAGE_SIZE, "%d\n", mc3xxx->data_trace_enable);
}

static ssize_t mc3xxx_dte_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);
	unsigned char data;
	int error;

	MC_PRINT("%s called\n", __func__);

	error = kstrtou8(buf, 10, &data);
	if (error)
		return error;

	mc3xxx->data_trace_enable = data;

	return count;
}

static DEVICE_ATTR(range, S_IRUGO | S_IWUSR | S_IWGRP,
		   mc3xxx_range_show, mc3xxx_range_store);
static DEVICE_ATTR(bandwidth, S_IRUGO | S_IWUSR | S_IWGRP,
		   mc3xxx_bandwidth_show, mc3xxx_bandwidth_store);
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR | S_IWGRP,
		   mc3xxx_mode_show, mc3xxx_mode_store);
static DEVICE_ATTR(value, S_IRUGO, mc3xxx_value_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   mc3xxx_delay_show, mc3xxx_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   mc3xxx_enable_show, mc3xxx_enable_store);
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR | S_IWGRP,
		   mc3xxx_register_show, mc3xxx_register_store);
static DEVICE_ATTR(orientation, S_IRUGO | S_IWUSR | S_IWGRP,
		   mc3xxx_orientation_show, mc3xxx_orientation_store);
static DEVICE_ATTR(dte, S_IRUGO | S_IWUSR | S_IWGRP,
		   mc3xxx_dte_show, mc3xxx_dte_store);

static struct attribute *mc3xxx_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_mode.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_reg.attr,
	&dev_attr_orientation.attr,
	&dev_attr_dte.attr,
	NULL
};

static struct attribute_group mc3xxx_attribute_group = {
	.attrs = mc3xxx_attributes
};


#ifdef CONFIG_HAS_EARLYSUSPEND
static void mc3xxx_early_suspend(struct early_suspend *h)
{
	struct mc3xxx_data *data =
	    container_of(h, struct mc3xxx_data, early_suspend);

	MC_PRINT("%s called\n", __func__);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		mc3xxx_set_mode(data->mc3xxx_client, MC3XXX_MODE_STANDBY);
		cancel_delayed_work_sync(&data->work);
	}
	mutex_unlock(&data->enable_mutex);
}

static void mc3xxx_early_resume(struct early_suspend *h)
{
	struct mc3xxx_data *data =
	    container_of(h, struct mc3xxx_data, early_suspend);

	MC_PRINT("%s called\n", __func__);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		mc3xxx_set_mode(data->mc3xxx_client, MC3XXX_MODE_WAKE);
		schedule_delayed_work(&data->work,
				      msecs_to_jiffies(atomic_read
						       (&data->delay)));
	}
	mutex_unlock(&data->enable_mutex);
}
#endif

static int gsensor_fetch_sysconfig_para(void)
{
	u_i2c_addr.dirty_addr_buf[0] = MC3XXX_I2C_ADDR;
	u_i2c_addr.dirty_addr_buf[1] = I2C_CLIENT_END;

	return 0;
}

static bool mc3xxx_i2c_auto_probe(struct i2c_client *client)
{
	static const unsigned short mc3xxx_i2c_auto_probe_addr[] = {0x4C, 0x6C};
	unsigned char _baDataBuf[2] = {0};
	int _nProbeAddrCount = (sizeof(mc3xxx_i2c_auto_probe_addr) /
					sizeof(mc3xxx_i2c_auto_probe_addr[0]));
	int _nCount = 0;

	MC_PRINT("%s called.\n", __func__);

	s_bPCODE = 0x00;

	for (_nCount = 0; _nCount < _nProbeAddrCount; _nCount++) {
		client->addr = mc3xxx_i2c_auto_probe_addr[_nCount];

		MC_PRINT("%s: probing addr is 0x%X.\n", __func__, client->addr);

		if (mc3xxx_smbus_read_byte(client, 0x3b, _baDataBuf)) {
			pr_err("%s: 0x%X fail to communicate!\n",
						__func__, client->addr);
			continue;
		}

		MC_PRINT("%s: addr 0x%X ok to read REG(0x3B): 0x%X.\n",
						__func__, client->addr,
						_baDataBuf[0]);

		if (true == mc3xxx_validate_pcode(_baDataBuf[0])) {
			MC_PRINT("%s: addr 0x%X confirmed ok to use.\n",
						__func__, client->addr);

			s_bPCODE = _baDataBuf[0];

			return true;
		}
	}

	return false;
}

static int mc3xxx_chip_init(struct i2c_client *client)
{
	unsigned char  _baDataBuf[2] = { 0 };

	_baDataBuf[0] = MC3XXX_REG_MODE;
	_baDataBuf[1] = 0x43;
	mc3xxx_smbus_write_byte(client, _baDataBuf[0], _baDataBuf[1]);

	_baDataBuf[0] = MC3XXX_REG_SRFA;
	_baDataBuf[1] = 0x00;
	if (IS_MC35XX())
		_baDataBuf[1] = 0x0A;
	mc3xxx_smbus_write_byte(client, _baDataBuf[0], _baDataBuf[1]);

	_baDataBuf[0] = MC3XXX_REG_INTEN;
	_baDataBuf[1] = 0x00;
	mc3xxx_smbus_write_byte(client, _baDataBuf[0], _baDataBuf[1]);

	mc3xxx_set_bandwidth(client, MC3XXX_BW_SET);
	mc3xxx_set_range(client, MC3XXX_RANGE_SET);

	_baDataBuf[0] = MC3XXX_REG_MODE;
	_baDataBuf[1] = 0x41;
	mc3xxx_smbus_write_byte(client, _baDataBuf[0], _baDataBuf[1]);

	return 0;
}

static int mc3xxx_enable_set(struct sensors_classdev *sensors_cdev,
			unsigned int enabled)
{
	struct mc3xxx_data *data = container_of(sensors_cdev,
				struct mc3xxx_data, cdev);

	if ((enabled != 0) && (enabled != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enabled);
		return -EINVAL;
	}

	mc3xxx_set_enable(&data->mc3xxx_client->dev, (uint8_t)enabled);

	return 0;
}

static int mc3xxx_poll_delay_set(struct sensors_classdev *sensors_cdev,
			unsigned int delay_msec)
{
	struct mc3xxx_data *data = container_of(sensors_cdev,
				struct mc3xxx_data, cdev);

	if (delay_msec > MC3XXX_MAX_DELAY)
		delay_msec = MC3XXX_MAX_DELAY;
	else if (delay_msec < MC3XXX_MIN_DELAY)
		delay_msec = MC3XXX_MIN_DELAY;
	atomic_set(&data->delay, (unsigned int)delay_msec);

	return 0;
}

static int sensors_parse_dt(struct device *dev,
				struct mc3xxx_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp_val;
	int rc;

	rc = of_property_read_u32(np, "mcube,position", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read position\n");
		return rc;
	} else {
		pdata->position = temp_val;
	}
	return 0;
}

static int mc3xxx_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err = 0;
	struct mc3xxx_data *data;
	struct mc3xxx_platform_data *platdata;
	struct input_dev *dev;

	MC_PRINT("%s called.\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		pr_err("%s: i2c_check_functionality error!\n", __func__);

	if (true != mc3xxx_i2c_auto_probe(client)) {
		pr_err("%s: fail to probe mCube g-sensor!\n", __func__);
		goto exit;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct mc3xxx_data),
		GFP_KERNEL);
	if (!data) {
		pr_err("%s: alloc memory error!\n", __func__);
		err = -ENOMEM;
		goto exit;
	}

	platdata = devm_kzalloc(&client->dev, sizeof(*platdata), GFP_KERNEL);
	if (!platdata) {
			dev_err(&client->dev,
			"failed to allocate memory for platform data\n");
			return -ENOMEM;
	}
	if (client->dev.of_node) {
		memset(platdata, 0 , sizeof(*platdata));
		err = sensors_parse_dt(&client->dev, platdata);
		if (err) {
			dev_err(&client->dev,
				"Unable to parse platfrom data err=%d\n", err);
			return err;
		}
	} else {
		memset(platdata, 0 , sizeof(*platdata));
	}

	data->mc3xxx_client = client;
	i2c_set_clientdata(client, data);

	mutex_init(&data->value_mutex);
	mutex_init(&data->enable_mutex);

	data->orientation = platdata->position;
	data->data_trace_enable = 0;

	mc3xxx_chip_init(client);

	INIT_DELAYED_WORK(&data->work, mc3xxx_work_func);
	atomic_set(&data->delay, MC3XXX_MAX_DELAY);
	atomic_set(&data->enable, 0);

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s: fail to allocate device!\n", __func__);
		err = -ENOMEM;
		goto exit;
	}

	set_bit(EV_ABS, dev->evbit);

	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);

	dev->name = MC3XXX_INPUT_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		goto exit;
	}

	data->input = dev;
	data->input->dev.parent = &data->mc3xxx_client->dev;

	err = sysfs_create_group(&data->input->dev.kobj,
					&mc3xxx_attribute_group);
	if (err < 0) {
		pr_err("%s: fail to create group!\n", __func__);
		goto error_sysfs;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = mc3xxx_early_suspend;
	data->early_suspend.resume = mc3xxx_early_resume;
	register_early_suspend(&data->early_suspend);
#endif

	data->cdev = sensors_cdev;
	/* The min_delay is used by userspace and the unit is microsecond. */
	data->cdev.min_delay = POLL_INTERVAL_MIN * 1000;
	data->cdev.delay_msec = atomic_read(&data->delay);
	data->cdev.sensors_enable = mc3xxx_enable_set;
	data->cdev.sensors_poll_delay = mc3xxx_poll_delay_set;
	err = sensors_classdev_register(&client->dev, &data->cdev);
	if (err) {
		dev_err(&client->dev, "class device create failed: %d\n", err);
		goto error_sysfs_group;
	}

/******************************************************************/
	printk("%s probe ok!\n", __func__);

	return 0;
error_sysfs_group:
	sysfs_remove_group(&data->input->dev.kobj, &mc3xxx_attribute_group);
error_sysfs:
	input_unregister_device(data->input);
exit:
	return err;
}

static int mc3xxx_remove(struct i2c_client *client)
{
	struct mc3xxx_data *data = i2c_get_clientdata(client);

	MC_PRINT("%s called.\n", __func__);

	mc3xxx_set_enable(&client->dev, 0);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &mc3xxx_attribute_group);
	sensors_classdev_unregister(&data->cdev);
	input_unregister_device(data->input);

	return 0;
}

#ifdef CONFIG_PM

static int mc3xxx_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct mc3xxx_data *data = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		mc3xxx_set_mode(data->mc3xxx_client, MC3XXX_MODE_STANDBY);
		cancel_delayed_work_sync(&data->work);
	}
	mutex_unlock(&data->enable_mutex);

	return 0;
}

static int mc3xxx_resume(struct i2c_client *client)
{
	struct mc3xxx_data *data = i2c_get_clientdata(client);

	MC_PRINT("%s called\n", __func__);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		mc3xxx_set_mode(data->mc3xxx_client, MC3XXX_MODE_WAKE);
		schedule_delayed_work(&data->work,
				      msecs_to_jiffies(atomic_read
						       (&data->delay)));
	}
	mutex_unlock(&data->enable_mutex);

	return 0;
}

#else

#define mc3xxx_suspend		NULL
#define mc3xxx_resume		NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id mc3xxx_id[] = {
	{MC3XXX_DEV_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, mc3xxx_id);

static const struct of_device_id mc3xxx_of_match[] = {
	{.compatible = "mcube, mc3xxx",},
	{},
}

MODULE_DEVICE_TABLE(of, mc3xxx_of_match);

static struct i2c_driver mc3xxx_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = MC3XXX_DEV_NAME,
		   .of_match_table = mc3xxx_of_match,
		   },
	.suspend = mc3xxx_suspend,
	.resume = mc3xxx_resume,
	.id_table = mc3xxx_id,
	.probe = mc3xxx_probe,
	.remove = mc3xxx_remove,
	.address_list  = u_i2c_addr.normal_i2c,
};

static int __init mc3xxx_init(void)
{
	MC_PRINT("%s called.\n", __func__);

	if (gsensor_fetch_sysconfig_para()) {
		pr_err("%s: gsensor_fetch_sysconfig_para err.\n", __func__);
		return -EINVAL;
	}

	return i2c_add_driver(&mc3xxx_driver);
}

static void __exit mc3xxx_exit(void)
{
	MC_PRINT("%s called.\n", __func__);

	i2c_del_driver(&mc3xxx_driver);
}


module_init(mc3xxx_init);
module_exit(mc3xxx_exit);

MODULE_DESCRIPTION("mc3xxx accelerometer driver");
MODULE_AUTHOR("mCube-inc");
MODULE_LICENSE("GPL");
MODULE_VERSION(MC3XXX_DRIVER_VERSION);

/*
 *  mma8x5x.c - Linux kernel modules for 3-Axis Orientation/Motion
 *  Detection Sensor MMA8451/MMA8452/MMA8453
 *
 *  Copyright (c) 2013-2014, The Linux Foundation. All Rights Reserved.
 *  Linux Foundation chooses to take subject only to the GPLv2 license
 *  terms, and distributes only under these terms.
 *  Copyright (C) 2010-2011 Freescale Semiconductor, Inc. All Rights Reserved.
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
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input-polldev.h>
#include <linux/sensors.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>

#define ACCEL_INPUT_DEV_NAME		"accelerometer"
#define MMA8451_ID			0x1A
#define MMA8452_ID			0x2A
#define MMA8453_ID			0x3A
#define MMA8652_ID			0x4A
#define MMA8653_ID			0x5A

/* Polling delay in msecs */
#define POLL_INTERVAL_MIN	1
#define POLL_INTERVAL_MAX	10000
#define POLL_INTERVAL		100 /* msecs */

/* if sensor is standby ,set POLL_STOP_TIME to slow down the poll */
#define POLL_STOP_TIME		10000
#define INPUT_FUZZ			32
#define INPUT_FLAT			32
#define INPUT_DATA_DIVIDER	16
#define MODE_CHANGE_DELAY_MS	100

#define MMA8X5X_STATUS_ZYXDR	0x08
#define MMA8X5X_BUF_SIZE	7

#define	MMA_SHUTTEDDOWN		(1 << 31)
#define MMA_STATE_MASK		(~MMA_SHUTTEDDOWN)

static struct sensors_classdev sensors_cdev = {
	.name = "mma8x5x-accel",
	.vendor = "Freescale",
	.version = 1,
	.handle = SENSORS_ACCELERATION_HANDLE,
	.type = SENSOR_TYPE_ACCELEROMETER,
	.max_range = "19.6",
	.resolution = "0.01",
	.sensor_power = "0.2",
	.min_delay = 2000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = POLL_INTERVAL,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

#define MMA_WAKE_CFG		0x02
#define MMA_INT_EN_DRDY		0x01
#define MMA_INT_EN_FF_MT	0x04
#define MMA_INT_ROUTING_CFG	0x01

#define MMA_POWER_CFG_MASK	0xFE

struct sensor_regulator {
	struct regulator *vreg;
	const char *name;
	u32	min_uV;
	u32	max_uV;
};

static struct sensor_regulator mma_vreg[] = {
	{NULL, "vdd", 2850000, 2850000},
	{NULL, "vio", 1800000, 1800000},
};

struct mma_odr_selection_table {
	u32 odr_cfg;
	u32 delay_bottom;
	u32 delay_top;
};

static struct mma_odr_selection_table mma_odr_table[] = {
	{0x00, 0, 1500},
	{0x08, 1501, 3500},
	{0x10, 3501, 7500},
	{0x18, 7501, 15000},
	{0x20, 15001, 50000},
	{0x28, 50001, 120000},
	{0x30, 120001, 400000},
	{0x38, 400001, 10000000},
};

/* register enum for mma8x5x registers */
enum {
	MMA8X5X_STATUS = 0x00,
	MMA8X5X_OUT_X_MSB,
	MMA8X5X_OUT_X_LSB,
	MMA8X5X_OUT_Y_MSB,
	MMA8X5X_OUT_Y_LSB,
	MMA8X5X_OUT_Z_MSB,
	MMA8X5X_OUT_Z_LSB,

	MMA8X5X_F_SETUP = 0x09,
	MMA8X5X_TRIG_CFG,
	MMA8X5X_SYSMOD,
	MMA8X5X_INT_SOURCE,
	MMA8X5X_WHO_AM_I,
	MMA8X5X_XYZ_DATA_CFG,
	MMA8X5X_HP_FILTER_CUTOFF,

	MMA8X5X_PL_STATUS,
	MMA8X5X_PL_CFG,
	MMA8X5X_PL_COUNT,
	MMA8X5X_PL_BF_ZCOMP,
	MMA8X5X_P_L_THS_REG,

	MMA8X5X_FF_MT_CFG,
	MMA8X5X_FF_MT_SRC,
	MMA8X5X_FF_MT_THS,
	MMA8X5X_FF_MT_COUNT,

	MMA8X5X_TRANSIENT_CFG = 0x1D,
	MMA8X5X_TRANSIENT_SRC,
	MMA8X5X_TRANSIENT_THS,
	MMA8X5X_TRANSIENT_COUNT,

	MMA8X5X_PULSE_CFG,
	MMA8X5X_PULSE_SRC,
	MMA8X5X_PULSE_THSX,
	MMA8X5X_PULSE_THSY,
	MMA8X5X_PULSE_THSZ,
	MMA8X5X_PULSE_TMLT,
	MMA8X5X_PULSE_LTCY,
	MMA8X5X_PULSE_WIND,

	MMA8X5X_ASLP_COUNT,
	MMA8X5X_CTRL_REG1,
	MMA8X5X_CTRL_REG2,
	MMA8X5X_CTRL_REG3,
	MMA8X5X_CTRL_REG4,
	MMA8X5X_CTRL_REG5,

	MMA8X5X_OFF_X,
	MMA8X5X_OFF_Y,
	MMA8X5X_OFF_Z,

	MMA8X5X_REG_END,
};

/* The sensitivity is represented in counts/g. In 2g mode the
sensitivity is 1024 counts/g. In 4g mode the sensitivity is 512
counts/g and in 8g mode the sensitivity is 256 counts/g.
 */
enum {
	MODE_2G = 0,
	MODE_4G,
	MODE_8G,
};

enum {
	MMA_STANDBY = 0,
	MMA_ACTIVED,
};
struct mma8x5x_data_axis {
	short x;
	short y;
	short z;
};
struct mma8x5x_data {
	struct i2c_client *client;
	struct input_polled_dev *poll_dev;
	struct input_dev *idev;
	struct mutex data_lock;
	struct sensors_classdev cdev;
	int active;
	int position;
	u8 chip_id;
	int mode;
	int int_pin;
	u32 int_flags;
	int poll_delay;
	bool use_int;
};
/* Addresses scanned */
static const unsigned short normal_i2c[] = {0x1c, 0x1d, I2C_CLIENT_END};

static int mma8x5x_chip_id[] = {
	MMA8451_ID,
	MMA8452_ID,
	MMA8453_ID,
	MMA8652_ID,
	MMA8653_ID,
};
static char *mma8x5x_names[] = {
	"mma8451",
	"mma8452",
	"mma8453",
	"mma8652",
	"mma8653",
};
static int mma8x5x_position_setting[8][3][3] = {
	{{ 0, -1,  0}, { 1,  0,	0}, {0, 0,	1} },
	{{-1,  0,  0}, { 0, -1,	0}, {0, 0,	1} },
	{{ 0,  1,  0}, {-1,  0,	0}, {0, 0,	1} },
	{{ 1,  0,  0}, { 0,  1,	0}, {0, 0,	1} },
	{{ 0, -1,  0}, {-1,  0,	0}, {0, 0,  -1} },
	{{-1,  0,  0}, { 0,  1,	0}, {0, 0,  -1} },
	{{ 0,  1,  0}, { 1,  0,	0}, {0, 0,  -1} },
	{{ 1,  0,  0}, { 0, -1,	0}, {0, 0,  -1} },
};
static struct mma8x5x_data *drv_data;
static int mma8x5x_config_regulator(struct i2c_client *client, bool on)
{
	int rc = 0, i;
	int num_vreg = sizeof(mma_vreg)/sizeof(struct sensor_regulator);

	if (on) {
		for (i = 0; i < num_vreg; i++) {
			mma_vreg[i].vreg = regulator_get(&client->dev,
					mma_vreg[i].name);
			if (IS_ERR(mma_vreg[i].vreg)) {
				rc = PTR_ERR(mma_vreg[i].vreg);
				dev_err(&client->dev, "%s:regulator get failed rc=%d\n",
						__func__, rc);
				mma_vreg[i].vreg = NULL;
				goto error_vdd;
			}
			if (regulator_count_voltages(mma_vreg[i].vreg) > 0) {
				rc = regulator_set_voltage(mma_vreg[i].vreg,
					mma_vreg[i].min_uV, mma_vreg[i].max_uV);
				if (rc) {
					dev_err(&client->dev, "%s:set_voltage failed rc=%d\n",
							__func__, rc);
					regulator_put(mma_vreg[i].vreg);
					mma_vreg[i].vreg = NULL;
					goto error_vdd;
				}
			}
			rc = regulator_enable(mma_vreg[i].vreg);
			if (rc) {
				dev_err(&client->dev, "%s: regulator_enable failed rc =%d\n",
						__func__, rc);
				if (regulator_count_voltages(mma_vreg[i].vreg)
						> 0) {
					regulator_set_voltage(mma_vreg[i].vreg,
							0, mma_vreg[i].max_uV);
				}
				regulator_put(mma_vreg[i].vreg);
				mma_vreg[i].vreg = NULL;
				goto error_vdd;
			}
		}
		return rc;
	} else {
		i = num_vreg;
	}
error_vdd:
	while (--i >= 0) {
		if (!IS_ERR_OR_NULL(mma_vreg[i].vreg)) {
			if (regulator_count_voltages(
				mma_vreg[i].vreg) > 0) {
				regulator_set_voltage(mma_vreg[i].vreg, 0,
						mma_vreg[i].max_uV);
			}
			regulator_disable(mma_vreg[i].vreg);
			regulator_put(mma_vreg[i].vreg);
			mma_vreg[i].vreg = NULL;
		}
	}
	return rc;
}

static int mma8x5x_data_convert(struct mma8x5x_data *pdata,
		struct mma8x5x_data_axis *axis_data)
{
	short rawdata[3], data[3];
	int i, j;
	int position = pdata->position ;
	if (position < 0 || position > 7)
		position = 0;
	rawdata[0] = axis_data->x;
	rawdata[1] = axis_data->y;
	rawdata[2] = axis_data->z;
	for (i = 0; i < 3 ; i++) {
		data[i] = 0;
		for (j = 0; j < 3; j++)
			data[i] += rawdata[j] *
				mma8x5x_position_setting[position][i][j];
	}
	axis_data->x = data[0]/INPUT_DATA_DIVIDER;
	axis_data->y = data[1]/INPUT_DATA_DIVIDER;
	axis_data->z = data[2]/INPUT_DATA_DIVIDER;
	return 0;
}
static int mma8x5x_check_id(int id)
{
	int i = 0;
	for (i = 0; i < sizeof(mma8x5x_chip_id)/sizeof(mma8x5x_chip_id[0]);
			i++)
		if (id == mma8x5x_chip_id[i])
			return 1;
	return 0;
}
static char *mma8x5x_id2name(u8 id)
{
	return mma8x5x_names[(id >> 4)-1];
}
static int mma8x5x_device_init(struct i2c_client *client)
{
	int result;
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, 0);
	if (result < 0)
		goto out;

	result = i2c_smbus_write_byte_data(client, MMA8X5X_XYZ_DATA_CFG,
					   pdata->mode);
	if (result < 0)
		goto out;
	pdata->active = MMA_STANDBY;
	msleep(MODE_CHANGE_DELAY_MS);
	return 0;
out:
	dev_err(&client->dev, "error when init mma8x5x:(%d)", result);
	return result;
}
static int mma8x5x_device_stop(struct i2c_client *client)
{
	u8 val;
	val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
	i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, val & 0xfe);
	return 0;
}

static int mma8x5x_device_start(struct i2c_client *client)
{
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);

	if (i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, 0))
		goto err_out;
	if (i2c_smbus_write_byte_data(client, MMA8X5X_XYZ_DATA_CFG,
			pdata->mode))
		goto err_out;

	/* The BT(boot time) for mma8x5x is 1.55ms according to
	  *Freescale mma8450Q document. Document Number:MMA8450Q
	  *Rev: 9.1, 04/2012
	  */
	usleep_range(1600, 2000);
	return 0;

err_out:
	dev_err(&client->dev, "%s:start device failed", __func__);
	return -EIO;
}

static int mma8x5x_delay2odr(u32 delay_ms)
{
	int i;
	u32 delay_us;

	delay_us = delay_ms * 1000;
	for (i = 0; i < sizeof(mma_odr_table) /
		sizeof(struct mma_odr_selection_table); i++) {
		if ((delay_us <= mma_odr_table[i].delay_top) &&
			(delay_us > mma_odr_table[i].delay_bottom))
			break;
	}
	if (i < sizeof(mma_odr_table) /
		sizeof(struct mma_odr_selection_table))
		return mma_odr_table[i].odr_cfg;
	else
		return -EINVAL;
}

static int mma8x5x_device_set_odr(struct i2c_client *client, u32 delay_ms)
{
	int result;
	u8 val;

	result = mma8x5x_delay2odr(delay_ms);
	if (result < 0)
		goto out;
	val = (u8)result;

	result = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
	if (result < 0)
		goto out;

	val = (u8)result | val;
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1,
					   (val & MMA_POWER_CFG_MASK));
	if (result < 0)
		goto out;

	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG4,
					   MMA_INT_EN_DRDY);
	if (result < 0)
		goto out;

	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG5,
					   MMA_INT_ROUTING_CFG);
	if (result < 0)
		goto out;

	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1,
					   val);
	if (result < 0)
		goto out;

	return 0;
out:
	dev_err(&client->dev, "error when set ODR mma8x5x:(%d)", result);
	return result;
}
static int mma8x5x_device_int_init(struct i2c_client *client)
{
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);
	int result;
	int val;

	result = mma8x5x_device_set_odr(client, pdata->poll_delay);
	if (result < 0)
		goto out;

	val = MMA_WAKE_CFG;
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG3,
					   val);
	if (result < 0)
		goto out;

	val = MMA_INT_EN_DRDY;
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG4,
					   val);
	if (result < 0)
		goto out;

	val = MMA_INT_ROUTING_CFG;
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG5,
					   val);
	if (result < 0)
		goto out;

	return 0;
out:
	dev_err(&client->dev, "error when int init mma8x5x:(%d)", result);
	return result;
}
static int mma8x5x_read_data(struct i2c_client *client,
		struct mma8x5x_data_axis *data)
{
	u8 tmp_data[MMA8X5X_BUF_SIZE];
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client,
					MMA8X5X_OUT_X_MSB, 7, tmp_data);
	if (ret < MMA8X5X_BUF_SIZE) {
		dev_err(&client->dev, "i2c block read failed\n");
		return -EIO;
	}
	data->x = ((tmp_data[0] << 8) & 0xff00) | tmp_data[1];
	data->y = ((tmp_data[2] << 8) & 0xff00) | tmp_data[3];
	data->z = ((tmp_data[4] << 8) & 0xff00) | tmp_data[5];
	return 0;
}

static void mma8x5x_report_data(struct mma8x5x_data *pdata)
{
	struct input_polled_dev *poll_dev = pdata->poll_dev;
	struct mma8x5x_data_axis data;
	mutex_lock(&pdata->data_lock);
	if ((pdata->active & MMA_STATE_MASK) == MMA_STANDBY) {
		poll_dev->poll_interval = POLL_STOP_TIME;
		/* if standby ,set as 10s to slow the poll. */
		goto out;
	} else {
		if (poll_dev->poll_interval == POLL_STOP_TIME)
			poll_dev->poll_interval = pdata->poll_delay;
	}
	if (mma8x5x_read_data(pdata->client, &data) != 0)
		goto out;
	mma8x5x_data_convert(pdata, &data);
	input_report_abs(poll_dev->input, ABS_X, data.x);
	input_report_abs(poll_dev->input, ABS_Y, data.y);
	input_report_abs(poll_dev->input, ABS_Z, data.z);
	input_sync(poll_dev->input);
out:
	mutex_unlock(&pdata->data_lock);
}

static void mma8x5x_dev_poll(struct input_polled_dev *dev)
{
	struct mma8x5x_data *pdata = (struct mma8x5x_data *)dev->private;
	mma8x5x_report_data(pdata);
}

static irqreturn_t mma8x5x_interrupt(int vec, void *data)
{
	struct i2c_client *client = (struct i2c_client *)data;
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);
	struct input_dev *idev = pdata->idev;
	struct mma8x5x_data_axis data_axis;

	mutex_lock(&pdata->data_lock);

	if (mma8x5x_read_data(pdata->client, &data_axis) != 0)
		goto out;
	mma8x5x_data_convert(pdata, &data_axis);
	input_report_abs(idev, ABS_X, data_axis.x);
	input_report_abs(idev, ABS_Y, data_axis.y);
	input_report_abs(idev, ABS_Z, data_axis.z);
	input_sync(idev);
out:
	mutex_unlock(&pdata->data_lock);

	return IRQ_HANDLED;
}

static int mma8x5x_enable_set(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct mma8x5x_data *pdata = container_of(sensors_cdev,
			struct mma8x5x_data, cdev);
	struct i2c_client *client = pdata->client;
	int ret;
	u8 val = 0;

	mutex_lock(&pdata->data_lock);
	if (enable) {
		if (pdata->active & MMA_SHUTTEDDOWN) {
			ret = mma8x5x_config_regulator(client, 1);
			if (ret)
				goto err_failed;

			ret = mma8x5x_device_start(client);
			if (ret)
				goto err_failed;

			ret = mma8x5x_device_set_odr(client, pdata->poll_delay);
			if (ret)
				goto err_failed;
			pdata->active &= ~MMA_SHUTTEDDOWN;
		}
		if (pdata->active == MMA_STANDBY) {
			val = i2c_smbus_read_byte_data(client,
					MMA8X5X_CTRL_REG1);
			if (val < 0) {
				dev_err(&client->dev, "read device state failed!");
				ret = val;
				goto err_failed;
			}

			ret = i2c_smbus_write_byte_data(client,
					MMA8X5X_CTRL_REG1, val | 0x01);
			if (ret) {
				dev_err(&client->dev, "change device state failed!");
				goto err_failed;
			}
			pdata->active = MMA_ACTIVED;
			dev_dbg(&client->dev, "%s:mma enable setting active.\n",
					__func__);
		}
	} else if (enable == 0) {
		if (pdata->active == MMA_ACTIVED) {
			val = i2c_smbus_read_byte_data(client,
					MMA8X5X_CTRL_REG1);
			if (val < 0) {
				dev_err(&client->dev, "read device state failed!");
				ret = val;
				goto err_failed;
			}

			ret = i2c_smbus_write_byte_data(client,
				MMA8X5X_CTRL_REG1, val & 0xFE);
			if (ret) {
				dev_err(&client->dev, "change device state failed!");
				goto err_failed;
			}

			pdata->active = MMA_STANDBY;
			dev_dbg(&client->dev, "%s:mma enable setting inactive.\n",
					__func__);
		}
		if (!mma8x5x_config_regulator(client, 0))
			pdata->active |= MMA_SHUTTEDDOWN;
	}
	mutex_unlock(&pdata->data_lock);
	return 0;

err_failed:
	mutex_unlock(&pdata->data_lock);
	return ret;
}

static ssize_t mma8x5x_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata = drv_data;
	struct i2c_client *client;
	u8 val;
	int enable;

	if (!pdata) {
		dev_err(dev, "Invalid driver private data!");
		return -EINVAL;
	}
	client = pdata->client;

	mutex_lock(&pdata->data_lock);
	val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
	if ((val & 0x01) && pdata->active == MMA_ACTIVED)
		enable = 1;
	else
		enable = 0;
	mutex_unlock(&pdata->data_lock);
	return snprintf(buf, PAGE_SIZE, "%d\n", enable);
}

static ssize_t mma8x5x_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mma8x5x_data *pdata = drv_data;
	struct i2c_client *client;
	int ret;
	unsigned long enable;

	if (!pdata) {
		dev_err(dev, "Invalid driver private data!");
		return -EINVAL;
	}
	client = pdata->client;
	ret = kstrtoul(buf, 10, &enable);
	if (ret)
		return ret;
	enable = (enable > 0) ? 1 : 0;
	ret = mma8x5x_enable_set(&pdata->cdev, enable);
	if (ret < 0)
		return ret;
	return count;
}
static ssize_t mma8x5x_position_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata = drv_data;
	int position = 0;

	if (!pdata) {
		dev_err(dev, "Invalid driver private data!");
		return -EINVAL;
	}
	mutex_lock(&pdata->data_lock);
	position = pdata->position ;
	mutex_unlock(&pdata->data_lock);
	return snprintf(buf, PAGE_SIZE, "%d\n", position);
}

static ssize_t mma8x5x_position_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mma8x5x_data *pdata = drv_data;
	int position;
	int ret;

	if (!pdata) {
		dev_err(dev, "Invalid driver private data!");
		return -EINVAL;
	}
	ret = kstrtoint(buf, 10, &position);
	if (ret)
		return ret;
	mutex_lock(&pdata->data_lock);
	pdata->position = position;
	mutex_unlock(&pdata->data_lock);
	return count;
}

static int mma8x5x_poll_delay_set(struct sensors_classdev *sensors_cdev,
		unsigned int delay_ms)
{
	struct mma8x5x_data *pdata = container_of(sensors_cdev,
			struct mma8x5x_data, cdev);
	int ret;

	if (pdata->use_int) {
		mutex_lock(&pdata->data_lock);
		pdata->poll_delay = delay_ms;
		ret = mma8x5x_device_set_odr(pdata->client, delay_ms);
		mutex_unlock(&pdata->data_lock);
		if (ret < 0)
			return ret;
	} else {
		mutex_lock(&pdata->data_lock);
		pdata->poll_delay = delay_ms;
		pdata->poll_dev->poll_interval = pdata->poll_delay;
		mutex_unlock(&pdata->data_lock);
	}

	return 0;
}

static ssize_t mma8x5x_poll_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata = drv_data;

	if (!pdata) {
		dev_err(dev, "Invalid driver private data!");
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->poll_delay);
}

static ssize_t mma8x5x_poll_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mma8x5x_data *pdata = drv_data;
	int delay;
	int ret;

	if (!pdata) {
		dev_err(dev, "Invalid driver private data!");
		return -EINVAL;
	}
	ret = kstrtoint(buf, 10, &delay);
	if (ret)
		return ret;
	if (delay <= POLL_INTERVAL_MIN)
		delay = POLL_INTERVAL_MIN;
	if (delay > POLL_INTERVAL_MAX)
		delay = POLL_INTERVAL_MAX;

	mma8x5x_poll_delay_set(&pdata->cdev, delay);

	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
		   mma8x5x_enable_show, mma8x5x_enable_store);
static DEVICE_ATTR(position, S_IWUSR | S_IRUGO,
		   mma8x5x_position_show, mma8x5x_position_store);
static DEVICE_ATTR(poll_delay, S_IWUSR | S_IRUGO,
		   mma8x5x_poll_delay_show, mma8x5x_poll_delay_store);

static struct attribute *mma8x5x_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_position.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static const struct attribute_group mma8x5x_attr_group = {
	.attrs = mma8x5x_attributes,
};
static int mma8x5x_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int chip_id;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -ENODEV;
	chip_id = i2c_smbus_read_byte_data(client, MMA8X5X_WHO_AM_I);
	if (!mma8x5x_check_id(chip_id))
		return -ENODEV;
	dev_dbg(&client->dev, "%s,check %s i2c address 0x%x.\n",
		__func__, mma8x5x_id2name(chip_id), client->addr);
	strlcpy(info->type, "mma8x5x", I2C_NAME_SIZE);
	return 0;
}

static int mma8x5x_parse_dt(struct device *dev, struct mma8x5x_data *data)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;

	data->int_pin = of_get_named_gpio_flags(np, "fsl,irq-gpio",
				0, &data->int_flags);
	if (data->int_pin < 0) {
		dev_err(dev, "Unable to read irq-gpio\n");
		return data->int_pin;
	}

	rc = of_property_read_u32(np, "fsl,sensors-position", &temp_val);
	if (!rc)
		data->position = temp_val;
	else {
		dev_err(dev, "Unable to read sensors-position\n");
		return rc;
	}

	data->use_int = of_property_read_bool(np, "fsl,use-interrupt");

	return 0;
}

static int __devinit mma8x5x_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int result, chip_id;
	struct input_dev *idev;
	struct mma8x5x_data *pdata;
	struct i2c_adapter *adapter;
	struct input_polled_dev *poll_dev;
	adapter = to_i2c_adapter(client->dev.parent);
	/* power on the device */
	result = mma8x5x_config_regulator(client, 1);
	if (result)
		goto err_power_on;

	result = i2c_check_functionality(adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if (!result)
		goto err_out;

	chip_id = i2c_smbus_read_byte_data(client, MMA8X5X_WHO_AM_I);

	if (!mma8x5x_check_id(chip_id)) {
		dev_err(&client->dev,
			"read chip ID 0x%x is not equal to 0x%x,0x%x,0x%x,0x%x,0x%x!\n",
			chip_id, MMA8451_ID, MMA8452_ID, MMA8453_ID,
			MMA8652_ID, MMA8653_ID);
		result = -EINVAL;
		goto err_out;
	}
	/* set the private data */
	pdata = kzalloc(sizeof(struct mma8x5x_data), GFP_KERNEL);
	if (!pdata) {
		result = -ENOMEM;
		dev_err(&client->dev, "alloc data memory error!\n");
		goto err_out;
	}

	if (client->dev.of_node) {
		result = mma8x5x_parse_dt(&client->dev, pdata);
		if (result)
			goto err_parse_dt;
	} else {
		pdata->position = CONFIG_SENSORS_MMA_POSITION;
		pdata->int_pin = -1;
		pdata->int_flags = 0;
	}

	/* Initialize the MMA8X5X chip */
	drv_data = pdata;
	pdata->client = client;
	pdata->chip_id = chip_id;
	pdata->mode = MODE_2G;
	pdata->poll_delay = POLL_STOP_TIME;
	pdata->poll_dev = NULL;

	mutex_init(&pdata->data_lock);
	i2c_set_clientdata(client, pdata);
	/* Initialize the MMA8X5X chip */
	mma8x5x_device_init(client);
	if (pdata->use_int) {
		if (pdata->int_pin >= 0)
			client->irq = gpio_to_irq(pdata->int_pin);

		if (gpio_is_valid(pdata->int_pin)) {
			result = gpio_request(pdata->int_pin,
				"mma8x5x_irq_gpio");
			if (result) {
				dev_err(&client->dev, "irq gpio(%d) request failed",
					pdata->int_pin);
				goto err_request_gpio;
			}
			result = gpio_direction_input(pdata->int_pin);
			if (result) {
				dev_err(&client->dev,
					"set_direction for irq gpio failed\n");
				goto err_set_direction;
			}
		}
		idev = input_allocate_device();
		if (!idev) {
			result = -ENOMEM;
			dev_err(&client->dev, "alloc input device failed!\n");
			goto err_alloc_poll_device;
		}
		input_set_drvdata(idev, pdata);
		idev->name = ACCEL_INPUT_DEV_NAME;
		idev->uniq = mma8x5x_id2name(pdata->chip_id);
		idev->id.bustype = BUS_I2C;
		idev->evbit[0] = BIT_MASK(EV_ABS);
		input_set_abs_params(idev, ABS_X, -0x7fff, 0x7fff, 0, 0);
		input_set_abs_params(idev, ABS_Y, -0x7fff, 0x7fff, 0, 0);
		input_set_abs_params(idev, ABS_Z, -0x7fff, 0x7fff, 0, 0);
		result = input_register_device(idev);
		if (result) {
			dev_err(&client->dev, "register input device failed!\n");
			goto err_register_device;
		}
		pdata->idev = idev;
		device_init_wakeup(&client->dev, true);
		enable_irq_wake(client->irq);
		result = request_threaded_irq(client->irq, NULL,
			mma8x5x_interrupt,
			IRQ_TYPE_EDGE_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			ACCEL_INPUT_DEV_NAME, (void *)client);
		if (result) {
			dev_err(&client->dev, "Could not allocate irq(%d) !\n",
				client->irq);
			goto err_register_irq;
		}
		mma8x5x_device_int_init(client);
	} else {
		/* create the input poll device */
		poll_dev = input_allocate_polled_device();
		if (!poll_dev) {
			result = -ENOMEM;
			dev_err(&client->dev, "alloc poll device failed!\n");
			goto err_alloc_poll_device;
		}
		pdata->poll_dev = poll_dev;
		pdata->idev = NULL;
		poll_dev->poll = mma8x5x_dev_poll;
		poll_dev->poll_interval = POLL_STOP_TIME;
		poll_dev->poll_interval_min = POLL_INTERVAL_MIN;
		poll_dev->poll_interval_max = POLL_INTERVAL_MAX;
		poll_dev->private = pdata;
		idev = poll_dev->input;
		idev->name = ACCEL_INPUT_DEV_NAME;
		idev->uniq = mma8x5x_id2name(pdata->chip_id);
		idev->id.bustype = BUS_I2C;
		idev->evbit[0] = BIT_MASK(EV_ABS);
		input_set_abs_params(idev, ABS_X, -0x7fff, 0x7fff, 0, 0);
		input_set_abs_params(idev, ABS_Y, -0x7fff, 0x7fff, 0, 0);
		input_set_abs_params(idev, ABS_Z, -0x7fff, 0x7fff, 0, 0);
		result = input_register_polled_device(pdata->poll_dev);
		if (result) {
			dev_err(&client->dev, "register poll device failed!\n");
			goto err_register_device;
		}
	}
	result = sysfs_create_group(&idev->dev.kobj, &mma8x5x_attr_group);
	if (result) {
		dev_err(&client->dev, "create device file failed!\n");
		result = -EINVAL;
		goto err_create_sysfs;
	}
	pdata->cdev = sensors_cdev;
	pdata->cdev.min_delay = POLL_INTERVAL_MIN * 1000;
	pdata->cdev.delay_msec = pdata->poll_delay;
	pdata->cdev.sensors_enable = mma8x5x_enable_set;
	pdata->cdev.sensors_poll_delay = mma8x5x_poll_delay_set;
	result = sensors_classdev_register(&client->dev, &pdata->cdev);
	if (result) {
		dev_err(&client->dev, "create class device file failed!\n");
		result = -EINVAL;
		goto err_create_class_sysfs;
	}
	dev_info(&client->dev,
		"%s:mma8x5x device driver probe successfully, position =%d\n",
		__func__, pdata->position);

	return 0;
err_create_class_sysfs:
	sysfs_remove_group(&idev->dev.kobj, &mma8x5x_attr_group);
err_create_sysfs:
	input_unregister_polled_device(pdata->poll_dev);
err_register_irq:
	if (pdata->use_int)
		device_init_wakeup(&client->dev, false);
err_register_device:
	if (pdata->use_int)
		input_free_device(idev);
	else
		input_free_polled_device(pdata->poll_dev);
err_alloc_poll_device:
err_set_direction:
	if (gpio_is_valid(pdata->int_pin) && pdata->use_int)
		gpio_free(pdata->int_pin);
err_request_gpio:
err_parse_dt:
	kfree(pdata);
err_out:
	mma8x5x_config_regulator(client, 0);
err_power_on:
	return result;
}
static int __devexit mma8x5x_remove(struct i2c_client *client)
{
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);
	struct input_polled_dev *poll_dev;
	mma8x5x_device_stop(client);
	if (pdata) {
		poll_dev = pdata->poll_dev;
		input_unregister_polled_device(poll_dev);
		input_free_polled_device(poll_dev);
		kfree(pdata);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mma8x5x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);

	if (pdata->use_int && pdata->active == MMA_ACTIVED)
		return 0;
	if (pdata->active == MMA_ACTIVED)
		mma8x5x_device_stop(client);
	if (pdata->active & MMA_SHUTTEDDOWN)
		return 0;
	if (!mma8x5x_config_regulator(client, 0))
		/* The highest bit sotres the power state */
		pdata->active |= MMA_SHUTTEDDOWN;
	return 0;
}

static int mma8x5x_resume(struct device *dev)
{
	int val = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);

	if (pdata->use_int && pdata->active == MMA_ACTIVED)
		return 0;
	/* No need to power on while device is shutdowned from standby state */
	if (pdata->active == (MMA_SHUTTEDDOWN | MMA_STANDBY))
		return 0;
	if (pdata->active & MMA_SHUTTEDDOWN) {
		if (mma8x5x_config_regulator(client, 1))
			goto out;

		if (mma8x5x_device_start(client))
			goto out;
		pdata->active &= ~MMA_SHUTTEDDOWN;
	}
	if (pdata->active == MMA_ACTIVED) {
		val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
		i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, val|0x01);
	}

	return 0;
out:
	dev_err(&client->dev, "%s:failed during resume operation", __func__);
	return -EIO;

}
#endif

static const struct i2c_device_id mma8x5x_id[] = {
	{"mma8x5x", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, mma8x5x_id);

static const struct of_device_id mma8x5x_of_match[] = {
	{ .compatible = "fsl,mma8x5x", },
	{ },
};

static SIMPLE_DEV_PM_OPS(mma8x5x_pm_ops, mma8x5x_suspend, mma8x5x_resume);
static struct i2c_driver mma8x5x_driver = {
	.class  = I2C_CLASS_HWMON,
	.driver = {
		.name = "mma8x5x",
		.owner = THIS_MODULE,
		.pm = &mma8x5x_pm_ops,
		.of_match_table = mma8x5x_of_match,
	},
	.probe = mma8x5x_probe,
	.remove = __devexit_p(mma8x5x_remove),
	.id_table = mma8x5x_id,
	.detect = mma8x5x_detect,
	.address_list = normal_i2c,
};

static int __init mma8x5x_init(void)
{
	/* register driver */
	int res;

	res = i2c_add_driver(&mma8x5x_driver);
	if (res < 0) {
		pr_info("%s:add mma8x5x i2c driver failed\n", __func__);
		return -ENODEV;
	}
	return res;
}

static void __exit mma8x5x_exit(void)
{
	i2c_del_driver(&mma8x5x_driver);
}

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MMA8X5X 3-Axis Orientation/Motion Detection Sensor driver");
MODULE_LICENSE("GPL");

module_init(mma8x5x_init);
module_exit(mma8x5x_exit);

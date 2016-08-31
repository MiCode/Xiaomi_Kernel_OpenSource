/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (C) 2016 XiaoMi, Inc.
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
*/

/**
 *  @addtogroup  DRIVERS
 *  @brief       Hardware drivers.
 *
 *  @{
 *      @file    inv_ami306_core.c
 *      @brief   Invensense implementation for AMI306
 *      @details This driver currently works for the AMI306
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>

#include "inv_ami306_iio.h"
#include "sysfs.h"
#include "inv_test/inv_counters.h"

static unsigned char late_initialize = true;

s32 i2c_write(const struct i2c_client *client,
		u8 command, u8 length, const u8 *values)
{
	INV_I2C_INC_COMPASSWRITE(3);
	return i2c_smbus_write_i2c_block_data(client, command, length, values);
}

s32 i2c_read(const struct i2c_client *client,
		u8 command, u8 length, u8 *values)
{
	INV_I2C_INC_COMPASSWRITE(3);
	INV_I2C_INC_COMPASSREAD(length);
	return i2c_smbus_read_i2c_block_data(client, command, length, values);
}

static int ami306_read_param(struct inv_ami306_state_s *st)
{
	int result = 0;
	unsigned char regs[AMI_PARAM_LEN];
	struct ami_sensor_parametor *param = &st->param;

	result = i2c_read(st->i2c, REG_AMI_SENX,
			AMI_PARAM_LEN, regs);
	if (result < 0)
		return result;

	/* Little endian 16 bit registers */
	param->m_gain.x = le16_to_cpup((__le16 *)(&regs[0]));
	param->m_gain.y = le16_to_cpup((__le16 *)(&regs[2]));
	param->m_gain.z = le16_to_cpup((__le16 *)(&regs[4]));

	param->m_interference.xy = regs[7];
	param->m_interference.xz = regs[6];
	param->m_interference.yx = regs[9];
	param->m_interference.yz = regs[8];
	param->m_interference.zx = regs[11];
	param->m_interference.zy = regs[10];

	param->m_offset.x = AMI_STANDARD_OFFSET;
	param->m_offset.y = AMI_STANDARD_OFFSET;
	param->m_offset.z = AMI_STANDARD_OFFSET;

	param->m_gain_cor.x = AMI_GAIN_COR_DEFAULT;
	param->m_gain_cor.y = AMI_GAIN_COR_DEFAULT;
	param->m_gain_cor.z = AMI_GAIN_COR_DEFAULT;

	return 0;
}

static int ami306_write_offset(const struct i2c_client *client,
				unsigned char *fine)
{
	int result = 0;
	unsigned char dat[3];
	dat[0] = (0x7f & fine[0]);
	dat[1] = 0;
	result = i2c_write(client, REG_AMI_OFFX, 2, dat);
	dat[0] = (0x7f & fine[1]);
	dat[1] = 0;
	result = i2c_write(client, REG_AMI_OFFY, 2, dat);
	dat[0] = (0x7f & fine[2]);
	dat[1] = 0;
	result = i2c_write(client, REG_AMI_OFFZ, 2, dat);

	return result;
}

static int ami306_wait_data_ready(struct inv_ami306_state_s *st,
				unsigned long usecs, unsigned long times)
{
	int result = 0;
	unsigned char buf;

	for (; 0 < times; --times) {
		udelay(usecs);
		result = i2c_read(st->i2c, REG_AMI_STA1, 1, &buf);
		if (result < 0)
			return INV_ERROR_COMPASS_DATA_NOT_READY;
		if (buf & AMI_STA1_DRDY_BIT)
			return 0;
		else if (buf & AMI_STA1_DOR_BIT)
			return INV_ERROR_COMPASS_DATA_OVERFLOW;
	}

	return INV_ERROR_COMPASS_DATA_NOT_READY;
}
int ami306_read_raw_data(struct inv_ami306_state_s *st,
			short dat[3])
{
	int result;
	unsigned char buf[6];
	result = i2c_read(st->i2c, REG_AMI_DATAX, sizeof(buf), buf);
	if (result < 0)
		return result;
	dat[0] = le16_to_cpup((__le16 *)(&buf[0]));
	dat[1] = le16_to_cpup((__le16 *)(&buf[2]));
	dat[2] = le16_to_cpup((__le16 *)(&buf[4]));

	return 0;
}

#define AMI_WAIT_DATAREADY_RETRY                3       /* retry times */
#define AMI_DRDYWAIT                            800     /* u(micro) sec */
static int ami306_force_measurement(struct inv_ami306_state_s *st,
					short ver[3])
{
	int result;
	int status;
	char buf;
	buf = AMI_CTRL3_FORCE_BIT;
	result = i2c_write(st->i2c, REG_AMI_CTRL3, 1, &buf);
	if (result < 0)
		return result;

	result = ami306_wait_data_ready(st,
			AMI_DRDYWAIT, AMI_WAIT_DATAREADY_RETRY);
	if (result && result != INV_ERROR_COMPASS_DATA_OVERFLOW)
		return result;
	/*  READ DATA X,Y,Z */
	status = ami306_read_raw_data(st, ver);
	if (status)
		return status;

	return result;
}

static int ami306_initial_b0_adjust(struct inv_ami306_state_s *st)
{
	int result;
	unsigned char fine[3] = { 0 };
	short data[3];
	int diff[3] = { 0x7fff, 0x7fff, 0x7fff };
	int fn = 0;
	int ax = 0;
	unsigned char buf[3];

	buf[0] = AMI_CTRL2_DREN;
	result = i2c_write(st->i2c, REG_AMI_CTRL2, 1, buf);
	if (result)
		return result;

	buf[0] = AMI_CTRL4_HS & 0xFF;
	buf[1] = (AMI_CTRL4_HS >> 8) & 0xFF;
	result = i2c_write(st->i2c, REG_AMI_CTRL4, 2, buf);
	if (result < 0)
		return result;

	for (fn = 0; fn < AMI_FINE_MAX; ++fn) { /* fine 0 -> 95 */
		fine[0] = fine[1] = fine[2] = fn;
		result = ami306_write_offset(st->i2c, fine);
		if (result)
			return result;

		result = ami306_force_measurement(st, data);
		if (result)
			return result;

		for (ax = 0; ax < 3; ax++) {
			/* search point most close to zero. */
			if (diff[ax] > abs(data[ax])) {
				st->fine[ax] = fn;
				diff[ax] = abs(data[ax]);
			}
		}
	}
	result = ami306_write_offset(st->i2c, st->fine);
	if (result)
		return result;

	/* Software Reset */
	buf[0] = AMI_CTRL3_SRST_BIT;
	result = i2c_write(st->i2c, REG_AMI_CTRL3, 1, buf);
	if (result < 0)
		return result;
	else
		return 0;
}

static int ami306_start_sensor(struct inv_ami306_state_s *st)
{
	int result = 0;
	unsigned char buf[2];

	/* Step 1 */
	buf[0] = (AMI_CTRL1_PC1 | AMI_CTRL1_FS1_FORCE);
	result = i2c_write(st->i2c, REG_AMI_CTRL1, 1, buf);
	if (result < 0)
		return result;
	/* Step 2 */
	buf[0] = AMI_CTRL2_DREN;
	result = i2c_write(st->i2c, REG_AMI_CTRL2, 1, buf);
	if (result < 0)
		return result;
	/* Step 3 */
	buf[0] = (AMI_CTRL4_HS & 0xFF);
	buf[1] = (AMI_CTRL4_HS >> 8) & 0xFF;

	result = i2c_write(st->i2c, REG_AMI_CTRL4, 2, buf);
	if (result < 0)
		return result;

	/* Step 4 */
	result = ami306_write_offset(st->i2c, st->fine);

	return result;
}

int set_ami306_enable(struct iio_dev *indio_dev, int state)
{
	struct inv_ami306_state_s *st = iio_priv(indio_dev);
	int result;
	char buf;

	buf = (AMI_CTRL1_PC1 | AMI_CTRL1_FS1_FORCE);
	result = i2c_write(st->i2c, REG_AMI_CTRL1, 1, &buf);
	if (result < 0)
		return result;

	result =  ami306_read_param(st);
	if (result)
		return result;
	if (late_initialize) {
		result = ami306_initial_b0_adjust(st);
		if (result)
			return result;
		late_initialize = false;
	}
	result = ami306_start_sensor(st);
	if (result)
		return result;
	buf = AMI_CTRL3_FORCE_BIT;
	st->timestamp = iio_get_time_ns();
	result = i2c_write(st->i2c, REG_AMI_CTRL3, 1, &buf);
	if (result)
		return result;

	return 0;
}

/**
 *  ami306_read_raw() - read raw method.
 */
static int ami306_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val,
			      int *val2,
			      long mask) {
	struct inv_ami306_state_s  *st = iio_priv(indio_dev);

	switch (mask) {
	case 0:
		if (!(iio_buffer_enabled(indio_dev)))
			return -EINVAL;
		if (chan->type == IIO_MAGN) {
			*val = st->compass_data[chan->channel2 - IIO_MOD_X];
			return IIO_VAL_INT;
		}

		return -EINVAL;
	case IIO_CHAN_INFO_SCALE:
		if (chan->type == IIO_MAGN) {
			*val = AMI_SCALE;
			return IIO_VAL_INT;
		}
		return -EINVAL;
	default:
		return -EINVAL;
	}
}

/**
 * inv_compass_matrix_show() - show orientation matrix
 */
static ssize_t inv_compass_matrix_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	signed char *m;
	struct inv_ami306_state_s *st = iio_priv(indio_dev);
	m = st->plat_data.orientation;
	return sprintf(buf,
	"%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		m[0],  m[1],  m[2],  m[3], m[4], m[5], m[6], m[7], m[8]);
}

static ssize_t ami306_rate_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_ami306_state_s *st = iio_priv(indio_dev);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (0 == data)
		return -EINVAL;
	/* transform rate to delay in ms */
	data = 1000 / data;
	if (data > AMI_MAX_DELAY)
		data = AMI_MAX_DELAY;
	if (data < AMI_MIN_DELAY)
		data = AMI_MIN_DELAY;
	st->delay = data;
	return count;
}

static ssize_t ami306_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_ami306_state_s *st = iio_priv(indio_dev);
	/* transform delay in ms to rate */
	return sprintf(buf, "%d\n", 1000 / st->delay);
}


static void ami306_work_func(struct work_struct *work)
{
	struct inv_ami306_state_s *st =
		container_of((struct delayed_work *)work,
			struct inv_ami306_state_s, work);
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	unsigned long delay = msecs_to_jiffies(st->delay);

	mutex_lock(&indio_dev->mlock);
	if (!(iio_buffer_enabled(indio_dev)))
		goto error_ret;

	st->timestamp = iio_get_time_ns();
	schedule_delayed_work(&st->work, delay);
	inv_read_ami306_fifo(indio_dev);
	INV_I2C_INC_COMPASSIRQ();

error_ret:
	mutex_unlock(&indio_dev->mlock);
}

static const struct iio_chan_spec compass_channels[] = {
	{
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_X,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_AMI306_SCAN_MAGN_X,
		.scan_type = IIO_ST('s', 16, 16, 0)
	}, {
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_Y,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_AMI306_SCAN_MAGN_Y,
		.scan_type = IIO_ST('s', 16, 16, 0)
	}, {
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_Z,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_AMI306_SCAN_MAGN_Z,
		.scan_type = IIO_ST('s', 16, 16, 0)
	},
	IIO_CHAN_SOFT_TIMESTAMP(INV_AMI306_SCAN_TIMESTAMP)
};

static DEVICE_ATTR(compass_matrix, S_IRUGO, inv_compass_matrix_show, NULL);
static DEVICE_ATTR(sampling_frequency, S_IRUGO | S_IWUSR, ami306_rate_show,
		ami306_rate_store);

static struct attribute *inv_ami306_attributes[] = {
	&dev_attr_compass_matrix.attr,
	&dev_attr_sampling_frequency.attr,
	NULL,
};
static const struct attribute_group inv_attribute_group = {
	.name = "ami306",
	.attrs = inv_ami306_attributes
};

static const struct iio_info ami306_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &ami306_read_raw,
	.attrs = &inv_attribute_group,
};

/*constant IIO attribute */
/**
 *  inv_ami306_probe() - probe function.
 */
static int inv_ami306_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct inv_ami306_state_s *st;
	struct iio_dev *indio_dev;
	int result;
	char data;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENODEV;
		goto out_no_free;
	}
	indio_dev = iio_allocate_device(sizeof(*st));
	if (indio_dev == NULL) {
		result =  -ENOMEM;
		goto out_no_free;
	}
	st = iio_priv(indio_dev);
	st->i2c = client;
	st->plat_data =
		*(struct mpu_platform_data *)dev_get_platdata(&client->dev);
	st->delay = 10;

	/* Make state variables available to all _show and _store functions. */
	i2c_set_clientdata(client, indio_dev);
	result = i2c_read(st->i2c, REG_AMI_WIA, 1, &data);
	if (result < 0)
		goto out_free;
	if (data != DATA_WIA)
		goto out_free;

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = id->name;
	indio_dev->channels = compass_channels;
	indio_dev->num_channels = ARRAY_SIZE(compass_channels);
	indio_dev->info = &ami306_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	result = inv_ami306_configure_ring(indio_dev);
	if (result)
		goto out_free;
	result = iio_buffer_register(indio_dev, indio_dev->channels,
					indio_dev->num_channels);
	if (result)
		goto out_unreg_ring;
	result = inv_ami306_probe_trigger(indio_dev);
	if (result)
		goto out_remove_ring;

	result = iio_device_register(indio_dev);
	if (result)
		goto out_remove_trigger;
	INIT_DELAYED_WORK(&st->work, ami306_work_func);
	pr_info("%s: Probe name %s\n", __func__, id->name);
	return 0;
out_remove_trigger:
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_ami306_remove_trigger(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_ami306_unconfigure_ring(indio_dev);
out_free:
	iio_free_device(indio_dev);
out_no_free:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, result);
	return -EIO;
}

/**
 *  inv_ami306_remove() - remove function.
 */
static int inv_ami306_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_ami306_state_s *st = iio_priv(indio_dev);
	cancel_delayed_work_sync(&st->work);
	iio_device_unregister(indio_dev);
	inv_ami306_remove_trigger(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_ami306_unconfigure_ring(indio_dev);
	iio_free_device(indio_dev);

	dev_info(&client->adapter->dev, "inv-ami306-iio module removed.\n");
	return 0;
}
static const unsigned short normal_i2c[] = { I2C_CLIENT_END };
/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct i2c_device_id inv_ami306_id[] = {
	{"ami306", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, inv_ami306_id);

static struct i2c_driver inv_ami306_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		=	inv_ami306_probe,
	.remove		=	inv_ami306_remove,
	.id_table	=	inv_ami306_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"inv-ami306-iio",
	},
	.address_list = normal_i2c,
};

static int __init inv_ami306_init(void)
{
	int result = i2c_add_driver(&inv_ami306_driver);
	if (result) {
		pr_err("%s failed\n", __func__);
		return result;
	}
	return 0;
}

static void __exit inv_ami306_exit(void)
{
	i2c_del_driver(&inv_ami306_driver);
}

module_init(inv_ami306_init);
module_exit(inv_ami306_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv-ami306-iio");
/**
 *  @}
 */


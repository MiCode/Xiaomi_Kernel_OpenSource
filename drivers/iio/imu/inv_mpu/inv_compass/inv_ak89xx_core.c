/*
* Copyright (C) 2013 Invensense, Inc.
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

#include "inv_ak89xx_iio.h"
#include "sysfs.h"
#include "inv_test/inv_counters.h"

static s64 get_time_ns(void)
{
	struct timespec ts;
	ktime_get_ts(&ts);
	return timespec_to_ns(&ts);
}

/**
 *  inv_serial_read() - Read one or more bytes from the device registers.
 *  @st:     Device driver instance.
 *  @reg:    First device register to be read from.
 *  @length: Number of bytes to read.
 *  @data:   Data read from device.
 *  NOTE:    The slave register will not increment when reading from the FIFO.
 */
int inv_serial_read(struct inv_ak89xx_state_s *st, u8 reg, u16 length, u8 *data)
{
	int result;
	INV_I2C_INC_COMPASSWRITE(3);
	INV_I2C_INC_COMPASSREAD(length);
	result = i2c_smbus_read_i2c_block_data(st->i2c, reg, length, data);
	if (result != length) {
		if (result < 0)
			return result;
		else
			return -EINVAL;
	} else {
		return 0;
	}
}

/**
 *  inv_serial_single_write() - Write a byte to a device register.
 *  @st:	Device driver instance.
 *  @reg:	Device register to be written to.
 *  @data:	Byte to write to device.
 */
int inv_serial_single_write(struct inv_ak89xx_state_s *st, u8 reg, u8 data)
{
	u8 d[1];
	d[0] = data;
	INV_I2C_INC_COMPASSWRITE(3);

	return i2c_smbus_write_i2c_block_data(st->i2c, reg, 1, d);
}

static int ak89xx_init(struct inv_ak89xx_state_s *st)
{
	int result = 0;
	unsigned char serial_data[3];

	result = inv_serial_single_write(st, AK89XX_REG_CNTL,
					 AK89XX_CNTL_MODE_POWER_DOWN);
	if (result) {
		pr_err("%s, line=%d\n", __func__, __LINE__);
		return result;
	}
	/* Wait at least 100us */
	udelay(100);

	result = inv_serial_single_write(st, AK89XX_REG_CNTL,
					 AK89XX_CNTL_MODE_FUSE_ACCESS);
	if (result) {
		pr_err("%s, line=%d\n", __func__, __LINE__);
		return result;
	}

	/* Wait at least 200us */
	udelay(200);

	result = inv_serial_read(st, AK89XX_FUSE_ASAX, 3, serial_data);
	if (result) {
		pr_err("%s, line=%d\n", __func__, __LINE__);
		return result;
	}

	st->asa[0] = serial_data[0];
	st->asa[1] = serial_data[1];
	st->asa[2] = serial_data[2];

	result = inv_serial_single_write(st, AK89XX_REG_CNTL,
					 AK89XX_CNTL_MODE_POWER_DOWN);
	if (result) {
		pr_err("%s, line=%d\n", __func__, __LINE__);
		return result;
	}
	udelay(100);

	return result;
}

int ak89xx_read(struct inv_ak89xx_state_s *st, short rawfixed[3])
{
	unsigned char regs[8];
	unsigned char *stat = &regs[0];
	unsigned char *stat2 = &regs[7];
	int result = 0;
	int status = 0;

	result = inv_serial_read(st, AK89XX_REG_ST1, 8, regs);
	if (result) {
		pr_err("%s, line=%d\n", __func__, __LINE__);
	return result;
	}

	rawfixed[0] = (short)((regs[2]<<8) | regs[1]);
	rawfixed[1] = (short)((regs[4]<<8) | regs[3]);
	rawfixed[2] = (short)((regs[6]<<8) | regs[5]);

	/*
	 * ST : data ready -
	 * Measurement has been completed and data is ready to be read.
	 */
	if (*stat & 0x01)
		status = 0;

	/*
	 * ST2 : data error -
	 * occurs when data read is started outside of a readable period;
	 * data read would not be correct.
	 * Valid in continuous measurement mode only.
	 * In single measurement mode this error should not occour but we
	 * stil account for it and return an error, since the data would be
	 * corrupted.
	 * DERR bit is self-clearing when ST2 register is read.
	 */
	if (*stat2 & 0x04)
		status = 0x04;
	/*
	 * ST2 : overflow -
	 * the sum of the absolute values of all axis |X|+|Y|+|Z| < 2400uT.
	 * This is likely to happen in presence of an external magnetic
	 * disturbance; it indicates, the sensor data is incorrect and should
	 * be ignored.
	 * An error is returned.
	 * HOFL bit clears when a new measurement starts.
	 */
	if (*stat2 & 0x08)
		status = 0x08;
	/*
	 * ST : overrun -
	 * the previous sample was not fetched and lost.
	 * Valid in continuous measurement mode only.
	 * In single measurement mode this error should not occour and we
	 * don't consider this condition an error.
	 * DOR bit is self-clearing when ST2 or any meas. data register is
	 * read.
	 */
	if (*stat & 0x02) {
		/* status = INV_ERROR_COMPASS_DATA_UNDERFLOW; */
		status = 0;
	}

	/*
	 * trigger next measurement if:
	 *	- stat is non zero;
	 *	- if stat is zero and stat2 is non zero.
	 * Won't trigger if data is not ready and there was no error.
	 */
	if (1) {
		unsigned char scale = 0;
		if (st->compass_id == COMPASS_ID_AK8963)
			scale = st->compass_scale;
		result = inv_serial_single_write(st, AK89XX_REG_CNTL,
				(scale << 4) | AK89XX_CNTL_MODE_SNG_MEASURE);
		if (result) {
			pr_err("%s, line=%d\n", __func__, __LINE__);
			return result;
		}
	} else
		pr_err("%s, no next measure(0x%x,0x%x)\n", __func__,
			*stat, *stat2);

	if (status)
		pr_err("%s, line=%d, status=%d\n", __func__, __LINE__, status);

	return status;
}

/**
 *  ak89xx_read_raw() - read raw method.
 */
static int ak89xx_read_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan,
			 int *val,
			 int *val2,
			 long mask) {
	struct inv_ak89xx_state_s  *st = iio_priv(indio_dev);
	int scale = 0;

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
			if (st->compass_id == COMPASS_ID_AK8975)
				scale = 9830;
			else if (st->compass_id == COMPASS_ID_AK8972)
				scale = 19661;
			else if (st->compass_id == COMPASS_ID_AK8963) {
				if (st->compass_scale)
					scale = 4915;	/* 16 bit */
				else
					scale = 19661;	/* 14 bit */
		}
		scale *= (1L << 15);
		*val = scale;
			return IIO_VAL_INT;
		}
		return -EINVAL;
	default:
		return -EINVAL;
	}
}

static ssize_t ak89xx_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);
	short c[3];

	mutex_lock(&indio_dev->mlock);
	c[0] = st->compass_data[0];
	c[1] = st->compass_data[1];
	c[2] = st->compass_data[2];
	mutex_unlock(&indio_dev->mlock);
	return sprintf(buf, "%d, %d, %d\n", c[0], c[1], c[2]);
}

static ssize_t ak89xx_scale_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);
	int scale = 0;

	if (st->compass_id == COMPASS_ID_AK8975)
		scale = 9830;
	else if (st->compass_id == COMPASS_ID_AK8972)
		scale = 19661;
	else if (st->compass_id == COMPASS_ID_AK8963) {
		if (st->compass_scale)
			scale = 4915;	/* 16 bit */
		else
			scale = 19661;	/* 14 bit */
	}
	scale *= (1L << 15);
	return sprintf(buf, "%d\n", scale);
}

static ssize_t ak89xx_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);
	/* transform delay in ms to rate */
	return sprintf(buf, "%d\n", (1000 / st->delay));
}

/**
 * ak89xx_matrix_show() - show orientation matrix
 */
static ssize_t ak89xx_matrix_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	signed char *m;
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);
	m = st->plat_data.orientation;
	return sprintf(buf,
		"%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		m[0],  m[1],  m[2],  m[3], m[4], m[5], m[6], m[7], m[8]);
}

void set_ak89xx_enable(struct iio_dev *indio_dev, bool enable)
{
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);
	int result = 0;
	unsigned char scale = 0;

	if (st->compass_id == COMPASS_ID_AK8963)
		scale = st->compass_scale;

	if (enable) {
			result = inv_serial_single_write(st, AK89XX_REG_CNTL,
				(scale << 4) | AK89XX_CNTL_MODE_SNG_MEASURE);
			if (result)
				pr_err("%s, line=%d\n", __func__, __LINE__);
			schedule_delayed_work(&st->work,
				msecs_to_jiffies(st->delay));
	} else {
			cancel_delayed_work_sync(&st->work);
			result = inv_serial_single_write(st, AK89XX_REG_CNTL,
				(scale << 4) | AK89XX_CNTL_MODE_POWER_DOWN);
			if (result)
				pr_err("%s, line=%d\n", __func__, __LINE__);
			mdelay(1);	/* wait at least 100us */
	}
}

static ssize_t ak89xx_scale_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);
	unsigned long data, result;

	result = kstrtoul(buf, 10, &data);
	if (result)
		return result;
	if (st->compass_id == COMPASS_ID_AK8963)
		st->compass_scale = !!data;
	return count;
}

static ssize_t ak89xx_rate_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	/* transform rate to delay in ms */
	data = 1000 / data;
	if (data > AK89XX_MAX_DELAY)
		data = AK89XX_MAX_DELAY;
	if (data < AK89XX_MIN_DELAY)
		data = AK89XX_MIN_DELAY;
	st->delay = (unsigned int) data;
	return count;
}

static void ak89xx_work_func(struct work_struct *work)
{
	struct inv_ak89xx_state_s *st =
		container_of((struct delayed_work *)work,
			struct inv_ak89xx_state_s, work);
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	unsigned long delay = msecs_to_jiffies(st->delay);

	mutex_lock(&indio_dev->mlock);
	if (!(iio_buffer_enabled(indio_dev)))
		goto error_ret;

	st->timestamp = get_time_ns();
	schedule_delayed_work(&st->work, delay);
	inv_read_ak89xx_fifo(indio_dev);
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
		.scan_index = INV_AK89XX_SCAN_MAGN_X,
		.scan_type = IIO_ST('s', 16, 16, 0)
	}, {
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_Y,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_AK89XX_SCAN_MAGN_Y,
		.scan_type = IIO_ST('s', 16, 16, 0)
	}, {
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_Z,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_AK89XX_SCAN_MAGN_Z,
		.scan_type = IIO_ST('s', 16, 16, 0)
	},
	IIO_CHAN_SOFT_TIMESTAMP(INV_AK89XX_SCAN_TIMESTAMP)
};

static DEVICE_ATTR(value, S_IRUGO, ak89xx_value_show, NULL);
static DEVICE_ATTR(scale, S_IRUGO | S_IWUSR, ak89xx_scale_show,
						ak89xx_scale_store);
static DEVICE_ATTR(sampling_frequency, S_IRUGO | S_IWUSR, ak89xx_rate_show,
						ak89xx_rate_store);
static DEVICE_ATTR(compass_matrix, S_IRUGO, ak89xx_matrix_show, NULL);

static struct attribute *inv_ak89xx_attributes[] = {
	&dev_attr_value.attr,
	&dev_attr_scale.attr,
	&dev_attr_sampling_frequency.attr,
	&dev_attr_compass_matrix.attr,
	NULL,
};

static const struct attribute_group inv_attribute_group = {
	.name = "ak89xx",
	.attrs = inv_ak89xx_attributes
};

static const struct iio_info ak89xx_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &ak89xx_read_raw,
	.attrs = &inv_attribute_group,
};

/*constant IIO attribute */
/**
 *  inv_ak89xx_probe() - probe function.
 */
static int inv_ak89xx_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct inv_ak89xx_state_s *st;
	struct iio_dev *indio_dev;
	int result;
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
	st->sl_handle = client->adapter;
	st->plat_data =
		*(struct mpu_platform_data *)dev_get_platdata(&client->dev);
	st->i2c_addr = client->addr;
	st->delay = AK89XX_DEFAULT_DELAY;
	st->compass_id = id->driver_data;
	st->compass_scale = 0;

	i2c_set_clientdata(client, indio_dev);
	result = ak89xx_init(st);
	if (result)
		goto out_free;

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = id->name;
	indio_dev->channels = compass_channels;
	indio_dev->num_channels = ARRAY_SIZE(compass_channels);
	indio_dev->info = &ak89xx_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	result = inv_ak89xx_configure_ring(indio_dev);
	if (result)
		goto out_free;
	result = iio_buffer_register(indio_dev, indio_dev->channels,
					indio_dev->num_channels);
	if (result)
		goto out_unreg_ring;
	result = inv_ak89xx_probe_trigger(indio_dev);
	if (result)
		goto out_remove_ring;

	result = iio_device_register(indio_dev);
	if (result)
		goto out_remove_trigger;
	INIT_DELAYED_WORK(&st->work, ak89xx_work_func);
	pr_info("%s: Probe name %s\n", __func__, id->name);
	return 0;
out_remove_trigger:
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_ak89xx_remove_trigger(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_ak89xx_unconfigure_ring(indio_dev);
out_free:
	iio_free_device(indio_dev);
out_no_free:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, result);
	return -EIO;
}

/**
 *  inv_ak89xx_remove() - remove function.
 */
static int inv_ak89xx_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_ak89xx_state_s *st = iio_priv(indio_dev);
	cancel_delayed_work_sync(&st->work);
	iio_device_unregister(indio_dev);
	inv_ak89xx_remove_trigger(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_ak89xx_unconfigure_ring(indio_dev);
	iio_free_device(indio_dev);

	dev_info(&client->adapter->dev, "inv-ak89xx-iio module removed.\n");
	return 0;
}

static const unsigned short normal_i2c[] = { I2C_CLIENT_END };

/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct i2c_device_id inv_ak89xx_id[] = {
	{"akm8975", COMPASS_ID_AK8975},
	{"akm8972", COMPASS_ID_AK8972},
	{"akm8963", COMPASS_ID_AK8963},
	{}
};

MODULE_DEVICE_TABLE(i2c, inv_ak89xx_id);

static struct i2c_driver inv_ak89xx_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		=	inv_ak89xx_probe,
	.remove		=	inv_ak89xx_remove,
	.id_table	=	inv_ak89xx_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"inv-ak89xx-iio",
	},
	.address_list = normal_i2c,
};

static int __init inv_ak89xx_init(void)
{
	int result = i2c_add_driver(&inv_ak89xx_driver);
	if (result) {
		pr_err("%s failed\n", __func__);
		return result;
	}
	return 0;
}

static void __exit inv_ak89xx_exit(void)
{
	i2c_del_driver(&inv_ak89xx_driver);
}

module_init(inv_ak89xx_init);
module_exit(inv_ak89xx_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv-ak89xx-iio");


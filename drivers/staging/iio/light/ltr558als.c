/* Lite-On LTR-558ALS Linux Driver
 *
 * Copyright (c) 2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>

#include "ltr558als.h"
#include "../iio.h"
#include "../sysfs.h"

#define DRIVER_VERSION "1.0"
#define DEVICE_NAME "LTR_558ALS"

struct ltr558_chip {
	struct i2c_client	*client;
	struct mutex	lock;
	int		irq;

	bool	is_als_enable;
	bool	als_enabled_before_suspend;
	int		als_gainrange;
	int		als_persist;
	int		als_reading;
	int		als_high_thres;
	int		als_low_thres;

	bool	is_prox_enable;
	bool	prox_enabled_before_suspend;
	int		ps_gainrange;
	int		prox_persist;
	int		prox_reading;
	int		prox_low_thres;
	int		prox_high_thres;
};

static int ltr558_i2c_read_reg(struct i2c_client *client, u8 regnum)
{
	return i2c_smbus_read_byte_data(client, regnum);
}

static int ltr558_i2c_write_reg(struct i2c_client *client, u8 regnum, u8 value)
{
	int writeerror;

	writeerror = i2c_smbus_write_byte_data(client, regnum, value);
	if (writeerror < 0)
		return writeerror;
	else
		return 0;
}

static int ltr558_ps_enable(struct i2c_client *client, int gainrange)
{
	int error;
	int setgain;

	switch (gainrange) {
		case PS_RANGE4:
			setgain = MODE_PS_ON_Gain4;
			break;

		case PS_RANGE8:
			setgain = MODE_PS_ON_Gain8;
			break;

		case PS_RANGE16:
			setgain = MODE_PS_ON_Gain16;
			break;

		case PS_RANGE1:
		default:
			setgain = MODE_PS_ON_Gain1;
			break;
	}

	error = ltr558_i2c_write_reg(client, LTR558_PS_CONTR, setgain);
	mdelay(WAKEUP_DELAY);
	return error;
}

static int ltr558_ps_disable(struct i2c_client *client)
{
	return ltr558_i2c_write_reg(client, LTR558_PS_CONTR, MODE_PS_StdBy);
}

static int ltr558_ps_read(struct i2c_client *client)
{
	int psval_lo = 0, psval_hi = 0, psdata = 0;
	psval_lo = ltr558_i2c_read_reg(client, LTR558_PS_DATA_0);
	if (psval_lo < 0){
		psdata = psval_lo;
		goto out;
	}

	psval_hi = ltr558_i2c_read_reg(client, LTR558_PS_DATA_1);
	if (psval_hi < 0){
		psdata = psval_hi;
		goto out;
	}

	psdata = ((psval_hi & 0x07) * 256) + psval_lo;
out:
	return psdata;
}

static int ltr558_als_enable(struct i2c_client *client, int gainrange)
{
	int error = -1;

	if (gainrange == ALS_RANGE1_320)
		error = ltr558_i2c_write_reg(client, LTR558_ALS_CONTR,
				MODE_ALS_ON_Range1);
	else if (gainrange == ALS_RANGE2_64K)
		error = ltr558_i2c_write_reg(client, LTR558_ALS_CONTR,
				MODE_ALS_ON_Range2);

	mdelay(WAKEUP_DELAY);
	return error;
}

static int ltr558_als_disable(struct i2c_client *client)
{
	return ltr558_i2c_write_reg(client, LTR558_ALS_CONTR, MODE_ALS_StdBy);
}

static int ltr558_als_read(struct i2c_client *client)
{
	int alsval_ch0_lo, alsval_ch0_hi;
	int alsval_ch1_lo, alsval_ch1_hi;
	unsigned int alsval_ch0 = 0, alsval_ch1 = 0;
	int luxdata = 0, ratio = 0;
	long ch0_coeff = 0, ch1_coeff = 0;

	alsval_ch1_lo = ltr558_i2c_read_reg(client, LTR558_ALS_DATA_CH1_0);
	alsval_ch1_hi = ltr558_i2c_read_reg(client, LTR558_ALS_DATA_CH1_1);
	alsval_ch1 = (alsval_ch1_hi * 256) + alsval_ch1_lo;

	alsval_ch0_lo = ltr558_i2c_read_reg(client, LTR558_ALS_DATA_CH0_0);
	alsval_ch0_hi = ltr558_i2c_read_reg(client, LTR558_ALS_DATA_CH0_1);
	alsval_ch0 = (alsval_ch0_hi * 256) + alsval_ch0_lo;

	if (alsval_ch0 == 0 && alsval_ch1 == 0)
		return 0;

	/* lux formula */
	ratio = (100 * alsval_ch1)/(alsval_ch1 + alsval_ch0);

	if (ratio < 45) {
		ch0_coeff = 17743;
		ch1_coeff = -11059;
	}
	else if ((ratio >= 45) && (ratio < 64)) {
		ch0_coeff = 37725;
		ch1_coeff = 13363;
	}
	else if ((ratio >= 64) && (ratio < 85)) {
		ch0_coeff = 16900;
		ch1_coeff = 1690;
	}
	else if (ratio >= 85) {
		ch0_coeff = 0;
		ch1_coeff = 0;
	}

	luxdata = ((alsval_ch0 * ch0_coeff) - (alsval_ch1 * ch1_coeff))/10000;
	return luxdata;
}

static bool ltr558_set_proxim_high_threshold(struct i2c_client *client,
		u32 thresh)
{
	bool st;
	st = ltr558_i2c_write_reg(client, LTR558_PS_THRES_UP_0,
			thresh & 0xFF);
	if (!st)
		st = ltr558_i2c_write_reg(client, LTR558_PS_THRES_UP_1,
				(thresh >> 8) & 0x07);
	return st;
}

static bool ltr558_set_proxim_low_threshold(struct i2c_client *client,
		u32 thresh)
{
	bool st;
	st = ltr558_i2c_write_reg(client, LTR558_PS_THRES_LOW_0,
			thresh & 0xFF);
	if (!st)
		st = ltr558_i2c_write_reg(client, LTR558_PS_THRES_LOW_1,
				(thresh >> 8) & 0x07);
	return st;
}

static bool ltr558_set_als_high_threshold(struct i2c_client *client, u32 thresh)
{
	bool st;
	st = ltr558_i2c_write_reg(client, LTR558_ALS_THRES_UP_0,
			thresh & 0xFF);
	if (!st)
		st = ltr558_i2c_write_reg(client, LTR558_ALS_THRES_UP_1,
				(thresh >> 8) & 0xFF);
	return st;
}

static bool ltr558_set_als_low_threshold(struct i2c_client *client, u32 thresh)
{
	bool st;
	st = ltr558_i2c_write_reg(client, LTR558_ALS_THRES_LOW_0,
			thresh & 0xFF);
	if (!st)
		st = ltr558_i2c_write_reg(client, LTR558_ALS_THRES_LOW_1,
				((thresh >> 8) & 0xFF));
	return st;
}

/* Sysfs interface */

/* proximity enable/disable  */
static ssize_t show_prox_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	if (chip->is_prox_enable)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t store_prox_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	int err = 0;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;
	if ((lval != 1) && (lval != 0)) {
		dev_err(dev, "illegal value %lu\n", lval);
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	if (lval == 1)
		err = ltr558_ps_enable(client, PS_RANGE1);
	else
		err = ltr558_ps_disable(client);

	if (err < 0)
		dev_err(dev, "Error in enabling proximity\n");
	else
		chip->is_prox_enable = (lval) ? true : false;

	mutex_unlock(&chip->lock);
	return count;
}

/* Proximity low thresholds  */
static ssize_t show_proxim_low_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->prox_low_thres);
}

static ssize_t store_proxim_low_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 0x7FF) || (lval < 0x0)) {
		dev_err(dev, "The threshold is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	st = ltr558_set_proxim_low_threshold(client, (u8)lval);
	if (!st)
		chip->prox_low_thres = (int)lval;
	else
		dev_err(dev, "Error in setting proximity low threshold\n");

	mutex_unlock(&chip->lock);
	return count;
}

/* Proximity high thresholds  */
static ssize_t show_proxim_high_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->prox_high_thres);
}

static ssize_t store_proxim_high_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 0x7FF) || (lval < 0x0)) {
		dev_err(dev, "The threshold is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	st = ltr558_set_proxim_high_threshold(client, lval);
	if (!st)
		chip->prox_high_thres = (int)lval;
	else
		dev_err(dev, "Error in setting proximity high threshold\n");

	mutex_unlock(&chip->lock);
	return count;
}

/* als enable/disable  */
static ssize_t show_als_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	if (chip->is_als_enable)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t store_als_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	int err = 0;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;
	if ((lval != 1) && (lval != 0)) {
		dev_err(dev, "illegal value %lu\n", lval);
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	if (lval == 1)
		err = ltr558_als_enable(client, chip->als_gainrange);
	else
		err = ltr558_als_disable(client);

	if (err < 0)
		dev_err(dev, "Error in enabling ALS\n");
	else
		chip->is_als_enable = (lval) ? true : false;

	mutex_unlock(&chip->lock);
	return count;
}

/* als low thresholds  */
static ssize_t show_als_low_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->als_low_thres);
}

static ssize_t store_als_low_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 0xFFFF) || (lval < 0x0)) {
		dev_err(dev, "The ALS threshold is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	st = ltr558_set_als_low_threshold(client, (int)lval);
	if (!st)
		chip->als_low_thres = (int)lval;
	else
		dev_err(dev, "Error in setting als low threshold\n");
	mutex_unlock(&chip->lock);
	return count;
}

/* Als high thresholds  */
static ssize_t show_als_high_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->als_high_thres);
}

static ssize_t store_als_high_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 0xFFFF) || (lval < 0x0)) {
		dev_err(dev, "The als threshold is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	st = ltr558_set_als_high_threshold(client, (int)lval);
	if (!st)
		chip->als_high_thres = (int)lval;
	else
		dev_err(dev, "Error in setting als high threshold\n");
	mutex_unlock(&chip->lock);
	return count;
}

/* Proximity persist  */
static ssize_t show_proxim_persist(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->prox_persist);
}

static ssize_t store_proxim_persist(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 16) || (lval < 0x0)) {
		dev_err(dev, "The proximity persist is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	chip->prox_persist = (int)lval;
	mutex_unlock(&chip->lock);
	return count;
}

/* als/ir persist  */
static ssize_t show_als_persist(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->als_persist);
}

static ssize_t store_als_persist(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 16) || (lval < 0x0)) {
		dev_err(dev, "The als persist is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	chip->als_persist = (int)lval;
	mutex_unlock(&chip->lock);
	return count;
}

/* Display proxim data  */
static ssize_t show_proxim_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	int prox_data = 0;
	ssize_t buf_count = 0;

	dev_vdbg(dev, "%s()\n", __func__);
	mutex_lock(&chip->lock);

	if (chip->is_prox_enable) {
		prox_data = ltr558_ps_read(chip->client);
		chip->prox_reading = prox_data;
		buf_count = sprintf(buf, "%d\n", prox_data);
	}
	else
		buf_count = sprintf(buf, "%d\n", chip->prox_reading);
	mutex_unlock(&chip->lock);
	return buf_count;
}

/* Display als data  */
static ssize_t show_als_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	ssize_t buf_count = 0;
	int als_data = 0;

	dev_vdbg(dev, "%s()\n", __func__);

	mutex_lock(&chip->lock);
	if (chip->is_als_enable) {
		als_data = ltr558_als_read(chip->client);
		buf_count = sprintf(buf, "%d\n", als_data);
		chip->als_reading = als_data;
	}
	else
		buf_count = sprintf(buf, "%d\n", chip->als_reading);
	mutex_unlock(&chip->lock);

	return buf_count;
}

/* Read name */
static ssize_t show_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	return sprintf(buf, "%s\n", chip->client->name);
}

static IIO_DEVICE_ATTR(proximity_low_threshold, S_IRUGO | S_IWUSR,
		show_proxim_low_threshold, store_proxim_low_threshold, 0);
static IIO_DEVICE_ATTR(proximity_high_threshold, S_IRUGO | S_IWUSR,
		show_proxim_high_threshold, store_proxim_high_threshold, 0);
static IIO_DEVICE_ATTR(proximity_persist, S_IRUGO | S_IWUSR,
		show_proxim_persist, store_proxim_persist, 0);
static IIO_DEVICE_ATTR(proximity_enable, S_IRUGO | S_IWUSR,
		show_prox_enable, store_prox_enable, 0);
static IIO_DEVICE_ATTR(proximity_value, S_IRUGO,
		show_proxim_data, NULL, 0);

static IIO_DEVICE_ATTR(als_low_threshold, S_IRUGO | S_IWUSR,
		show_als_low_threshold, store_als_low_threshold, 0);
static IIO_DEVICE_ATTR(als_high_threshold, S_IRUGO | S_IWUSR,
		show_als_high_threshold, store_als_high_threshold, 0);
static IIO_DEVICE_ATTR(als_persist, S_IRUGO | S_IWUSR,
		show_als_persist, store_als_persist, 0);
static IIO_DEVICE_ATTR(als_enable, S_IRUGO | S_IWUSR,
		show_als_enable, store_als_enable, 0);
static IIO_DEVICE_ATTR(als_value, S_IRUGO,
		show_als_data, NULL, 0);

static IIO_DEVICE_ATTR(name, S_IRUGO, show_name, NULL, 0);

static struct attribute *ltr558_attributes[] = {
	&iio_dev_attr_name.dev_attr.attr,

	&iio_dev_attr_als_low_threshold.dev_attr.attr,
	&iio_dev_attr_als_high_threshold.dev_attr.attr,
	&iio_dev_attr_als_enable.dev_attr.attr,
	&iio_dev_attr_als_persist.dev_attr.attr,
	&iio_dev_attr_als_value.dev_attr.attr,

	&iio_dev_attr_proximity_low_threshold.dev_attr.attr,
	&iio_dev_attr_proximity_high_threshold.dev_attr.attr,
	&iio_dev_attr_proximity_enable.dev_attr.attr,
	&iio_dev_attr_proximity_persist.dev_attr.attr,
	&iio_dev_attr_proximity_value.dev_attr.attr,
	NULL
};

static const struct attribute_group ltr558_group = {
	.attrs = ltr558_attributes,
};

static int ltr558_chip_init(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	int error = 0;

	mdelay(PON_DELAY);

	chip->is_prox_enable  = 0;
	chip->prox_low_thres = 0;
	chip->prox_high_thres = 0x7FF;
	chip->prox_reading = 0;

	chip->als_low_thres = 0;
	chip->als_high_thres = 0xFFFF;
	chip->als_reading = 0;

	chip->is_als_enable = 0;
	chip->prox_persist = 0;
	chip->als_persist = 0;

	/* Enable PS to Gain1 at startup */
	chip->ps_gainrange = PS_RANGE1;
	error = ltr558_ps_enable(client, chip->ps_gainrange);
	if (error < 0)
		goto out;


	/* Enable ALS to Full Range at startup */
	chip->als_gainrange = ALS_RANGE2_64K;
	error = ltr558_als_enable(client, chip->als_gainrange);
	if (error < 0)
		goto out;

out:
	return error;
}

static const struct iio_info ltr558_info = {
	.attrs = &ltr558_group,
	.driver_module = THIS_MODULE,
};

static irqreturn_t threshold_isr(int irq, void *irq_data)
{
	struct ltr558_chip *chip = (struct ltr558_chip *)irq_data;
	s32 int_reg;
	struct i2c_client *client = chip->client;

	int_reg = i2c_smbus_read_byte_data(client, LTR558_ALS_PS_STATUS);
	if (int_reg < 0) {
		dev_err(&client->dev, "Error in reading register %d, error %d\n",
				LTR558_ALS_PS_STATUS, int_reg);
		return IRQ_HANDLED;
	}

	if (int_reg & STATUS_ALS_INT_TRIGGER) {
		if (int_reg & STATUS_ALS_NEW_DATA)
			chip->als_reading = ltr558_als_read(client);
	}

	if (int_reg & STATUS_PS_INT_TRIGGER) {
		if (int_reg & STATUS_PS_NEW_DATA)
			chip->prox_reading = ltr558_ps_read(client);
	}

	return IRQ_HANDLED;
}

static int ltr558_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct ltr558_chip *chip;
	struct iio_dev *indio_dev;

	/* data memory allocation */
	indio_dev = iio_allocate_device(sizeof(*chip));
	if (indio_dev == NULL) {
		dev_err(&client->dev, "iio allocation fails\n");
		ret = -ENOMEM;
		goto exit;
	}
	chip = iio_priv(indio_dev);

	i2c_set_clientdata(client, indio_dev);
	chip->client = client;
	chip->irq = client->irq;

	if (chip->irq > 0) {
		ret = request_threaded_irq(chip->irq, NULL, threshold_isr,
						IRQF_SHARED, "LTR558_ALS", chip);
		if (ret) {
			dev_err(&client->dev, "Unable to register irq %d; "
				"ret %d\n", chip->irq, ret);
			goto exit_iio_free;
		}
	}

	mutex_init(&chip->lock);

	ret = ltr558_chip_init(client);
	if (ret)
		goto exit_irq;

	indio_dev->info = &ltr558_info;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&client->dev, "iio registration fails\n");
		goto exit_irq;
	}

	dev_dbg(&client->dev, "%s() success\n", __func__);
	return 0;

exit_irq:
	if (chip->irq > 0)
		free_irq(chip->irq, chip);
exit_iio_free:
	iio_free_device(indio_dev);
exit:
	return ret;
}


static int ltr558_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ltr558_chip *chip = iio_priv(indio_dev);

	dev_dbg(&client->dev, "%s()\n", __func__);
	if (chip->irq > 0)
		free_irq(chip->irq, chip);
	ltr558_ps_disable(client);
	ltr558_als_disable(client);
	iio_device_unregister(indio_dev);
	return 0;
}


static int ltr558_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	int ret;

	if (chip->is_als_enable == 1)
		chip->als_enabled_before_suspend = 1;
	if (chip->is_prox_enable == 1)
		chip->prox_enabled_before_suspend = 1;

	ret = ltr558_ps_disable(client);
	if (ret == 0)
		ret = ltr558_als_disable(client);

	return ret;
}


static int ltr558_resume(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ltr558_chip *chip = iio_priv(indio_dev);
	int error = 0;

	mdelay(PON_DELAY);

	if (chip->prox_enabled_before_suspend == 1) {
		error = ltr558_ps_enable(client, chip->ps_gainrange);
		if (error < 0)
			goto out;
	}

	if (chip->als_enabled_before_suspend == 1) {
		error = ltr558_als_enable(client, chip->als_gainrange);
	}
out:
	return error;
}


static const struct i2c_device_id ltr558_id[] = {
	{ DEVICE_NAME, 0 },
	{}
};


static struct i2c_driver ltr558_driver = {
	.class	= I2C_CLASS_HWMON,
	.probe = ltr558_probe,
	.remove = ltr558_remove,
	.id_table = ltr558_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
	},
	.suspend = ltr558_suspend,
	.resume = ltr558_resume,
};


static int __init ltr558_driverinit(void)
{
	return i2c_add_driver(&ltr558_driver);
}


static void __exit ltr558_driverexit(void)
{
	i2c_del_driver(&ltr558_driver);
}


module_init(ltr558_driverinit)
module_exit(ltr558_driverexit)

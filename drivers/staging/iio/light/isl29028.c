/*
 * A iio driver for the light sensor ISL 29028.
 *
 * IIO Light driver for monitoring ambient light intensity in lux and proximity
 * ir.
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include "../iio.h"
#include "../sysfs.h"

#define ISL29028_PROX_PERIOD		800
#define CONVERSION_TIME_MS		100

#define ISL29028_REG_ADD_CONFIGURE	0x01

#define CONFIGURE_PROX_EN_MASK		(1 << 7)
#define CONFIGURE_PROX_EN_SH		7

#define CONFIGURE_PROX_SLP_SH		4
#define CONFIGURE_PROX_SLP_MASK		(7 << CONFIGURE_PROX_SLP_SH)

#define CONFIGURE_PROX_DRIVE		(1 << 3)

#define CONFIGURE_ALS_EN		1
#define CONFIGURE_ALS_DIS		0
#define CONFIGURE_ALS_EN_SH		2
#define CONFIGURE_ALS_EN_MASK		(1 << CONFIGURE_ALS_EN_SH)


#define CONFIGURE_ALS_RANGE_LOW_LUX	0
#define CONFIGURE_ALS_RANGE_HIGH_LUX	1
#define CONFIGURE_ALS_RANGE_SH		1
#define CONFIGURE_ALS_RANGE_MASK	(1 << CONFIGURE_ALS_RANGE_SH)

#define CONFIGURE_ALS_IR_MODE_MASK	1
#define CONFIGURE_ALS_IR_MODE_SH	0
#define CONFIGURE_ALS_IR_MODE_IR	1
#define CONFIGURE_ALS_IR_MODE_ALS	0

#define ISL29028_REG_ADD_INTERRUPT	0x02
#define INTERRUPT_PROX_FLAG_MASK	(1 << 7)
#define INTERRUPT_PROX_FLAG_SH		7
#define INTERRUPT_PROX_FLAG_EN		1
#define INTERRUPT_PROX_FLAG_DIS		0

#define INTERRUPT_PROX_PERSIST_SH	5
#define INTERRUPT_PROX_PERSIST_MASK	(3 << 5)

#define INTERRUPT_ALS_FLAG_MASK		(1 << 3)
#define INTERRUPT_ALS_FLAG_SH		3
#define INTERRUPT_ALS_FLAG_EN		1
#define INTERRUPT_ALS_FLAG_DIS		0

#define INTERRUPT_ALS_PERSIST_SH	1
#define INTERRUPT_ALS_PERSIST_MASK	(3 << 1)

#define ISL29028_REG_ADD_PROX_LOW_THRES		0x03
#define ISL29028_REG_ADD_PROX_HIGH_THRES	0x04

#define ISL29028_REG_ADD_ALSIR_LOW_THRES	0x05
#define ISL29028_REG_ADD_ALSIR_LH_THRES		0x06
#define ISL29028_REG_ADD_ALSIR_LH_THRES_L_SH	0
#define ISL29028_REG_ADD_ALSIR_LH_THRES_H_SH	4
#define ISL29028_REG_ADD_ALSIR_HIGH_THRES	0x07

#define ISL29028_REG_ADD_PROX_DATA		0x08
#define ISL29028_REG_ADD_ALSIR_L		0x09
#define ISL29028_REG_ADD_ALSIR_U		0x0A

#define ISL29028_REG_ADD_TEST1_MODE		0x0E
#define ISL29028_REG_ADD_TEST2_MODE		0x0F

#define ISL29028_MAX_REGS		ISL29028_REG_ADD_TEST2_MODE

enum {
	MODE_NONE = 0,
	MODE_ALS,
	MODE_IR
};

struct isl29028_chip {
	struct i2c_client	*client;
	struct mutex		lock;
	struct regulator	*isl_reg;
	int			irq;

	int			prox_period;
	int			prox_low_thres;
	int			prox_high_thres;
	int			prox_persist;
	bool			is_prox_enable;
	int			prox_reading;

	int			als_high_thres;
	int			als_low_thres;
	int			als_persist;
	int			als_range;
	int			als_reading;
	int			als_ir_mode;

	int			ir_high_thres;
	int			ir_low_thres;
	int			ir_reading;

	bool			is_int_enable;
	bool			is_proxim_int_waiting;
	bool			is_als_int_waiting;
	struct completion	prox_completion;
	struct completion	als_completion;
	u8			reg_cache[ISL29028_MAX_REGS];
	int			shutdown_complete;
};

static bool isl29028_write_data(struct i2c_client *client, u8 reg,
	u8 val, u8 mask, u8 shift)
{
	u8 regval;
	int ret = 0;
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	if (chip && chip->shutdown_complete)
		return false;

	regval = chip->reg_cache[reg];
	regval &= ~mask;
	regval |= val << shift;

	ret = i2c_smbus_write_byte_data(client, reg, regval);
	if (ret) {
		dev_err(&client->dev, "Write to device reg %d fails status "
				"%x\n", reg, ret);
		return false;
	}
	chip->reg_cache[reg] = regval;
	return true;
}

static bool isl29028_set_proxim_period(struct i2c_client *client,
		bool is_enable,	int period)
{
	int prox_period[] = {0, 12, 50, 75, 100, 200, 400, 800};
	int i;
	int sel;
	bool st;
	if (period < 12)
		sel = 7;
	else {
		for (i = 1; i < ARRAY_SIZE(prox_period) - 1; ++i) {
			if ((prox_period[i] <= period) &&
						period < prox_period[i + 1])
				break;
		}
		sel = 7 - i;
	}

	if (!is_enable) {
		dev_dbg(&client->dev, "Disabling proximity sensing\n");
		st = isl29028_write_data(client, ISL29028_REG_ADD_CONFIGURE,
			0, CONFIGURE_PROX_EN_MASK, CONFIGURE_PROX_EN_SH);
	} else {
		dev_dbg(&client->dev, "Enabling proximity sensing with period "
			"of %d ms sel %d period %d\n", prox_period[7 - sel],
			sel, period);
		st = isl29028_write_data(client, ISL29028_REG_ADD_CONFIGURE,
			sel, CONFIGURE_PROX_SLP_MASK, CONFIGURE_PROX_SLP_SH);
		if (st)
			st = isl29028_write_data(client,
				ISL29028_REG_ADD_CONFIGURE, 1,
				CONFIGURE_PROX_EN_MASK, CONFIGURE_PROX_EN_SH);
	}
	return st;
}

static bool isl29028_set_proxim_persist(struct i2c_client *client,
		bool is_enable,	int persist)
{
	int prox_perstant[] = {1, 4, 8, 16};
	int i;
	int sel;
	bool st;
	if (is_enable) {
		for (i = 0; i < ARRAY_SIZE(prox_perstant) - 1; ++i) {
			if ((prox_perstant[i] <= persist) &&
						persist < prox_perstant[i+1])
				break;
		}
		sel = i;
	}

	if (is_enable) {
		dev_dbg(&client->dev, "Enabling proximity threshold interrupt\n");
		st = isl29028_write_data(client, ISL29028_REG_ADD_INTERRUPT,
			sel, INTERRUPT_PROX_PERSIST_MASK,
			INTERRUPT_PROX_PERSIST_SH);
		if (st)
			st = isl29028_write_data(client,
				ISL29028_REG_ADD_INTERRUPT,
				INTERRUPT_PROX_FLAG_EN,
				INTERRUPT_PROX_FLAG_MASK,
				INTERRUPT_PROX_FLAG_SH);
	} else {
		st = isl29028_write_data(client,
			ISL29028_REG_ADD_INTERRUPT, INTERRUPT_PROX_FLAG_DIS,
			INTERRUPT_PROX_FLAG_MASK, INTERRUPT_PROX_FLAG_SH);
	}
	return st;
}

static bool isl29028_set_als_persist(struct i2c_client *client, bool is_enable,
			int persist)
{
	int prox_perstant[] = {1, 4, 8, 16};
	int i;
	int sel;
	bool st;
	if (is_enable) {
		for (i = 0; i < ARRAY_SIZE(prox_perstant) - 1; ++i) {
			if ((prox_perstant[i] <= persist) &&
						persist < prox_perstant[i+1])
				break;
		}
		sel = i;
	}

	if (is_enable) {
		dev_dbg(&client->dev, "Enabling als threshold interrupt\n");
		st = isl29028_write_data(client, ISL29028_REG_ADD_INTERRUPT,
			sel, INTERRUPT_ALS_PERSIST_MASK,
			INTERRUPT_ALS_PERSIST_SH);
		if (st)
			st = isl29028_write_data(client,
				ISL29028_REG_ADD_INTERRUPT,
				INTERRUPT_ALS_FLAG_EN,
				INTERRUPT_ALS_FLAG_MASK,
				INTERRUPT_ALS_FLAG_SH);
	} else {
		st = isl29028_write_data(client,
			ISL29028_REG_ADD_INTERRUPT, INTERRUPT_ALS_FLAG_DIS,
			INTERRUPT_ALS_FLAG_MASK, INTERRUPT_ALS_FLAG_SH);
	}
	return st;
}

static bool isl29028_set_proxim_high_threshold(struct i2c_client *client, u8 th)
{
	return isl29028_write_data(client, ISL29028_REG_ADD_PROX_HIGH_THRES,
		th, 0xFF, 0);
}

static bool isl29028_set_proxim_low_threshold(struct i2c_client *client, u8 th)
{
	return isl29028_write_data(client, ISL29028_REG_ADD_PROX_LOW_THRES,
		th, 0xFF, 0);
}

static bool isl29028_set_irals_high_threshold(struct i2c_client *client,
				u32 als)
{
	bool st;
	st = isl29028_write_data(client, ISL29028_REG_ADD_ALSIR_HIGH_THRES,
		(als >> 4) & 0xFF, 0xFF, 0);
	if (st)
		st = isl29028_write_data(client,
			ISL29028_REG_ADD_ALSIR_LH_THRES, als & 0xF,
			0xF << ISL29028_REG_ADD_ALSIR_LH_THRES_H_SH,
			ISL29028_REG_ADD_ALSIR_LH_THRES_H_SH);
	return st;
}

static bool isl29028_set_irals_low_threshold(struct i2c_client *client, u32 als)
{
	bool st;
	st = isl29028_write_data(client,
		ISL29028_REG_ADD_ALSIR_LH_THRES, (als >> 8) & 0xF,
		0xF << ISL29028_REG_ADD_ALSIR_LH_THRES_L_SH,
		ISL29028_REG_ADD_ALSIR_LH_THRES_L_SH);
	if (st)
		st = isl29028_write_data(client,
			ISL29028_REG_ADD_ALSIR_LOW_THRES,
			als & 0xFF, 0xFF, 0);
	return st;
}

static bool isl29028_set_als_ir_mode(struct i2c_client *client, bool is_enable,
	bool is_als)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	bool st;
	if (is_enable) {
		if (is_als) {
			dev_dbg(&client->dev, "Enabling ALS mode\n");
			st = isl29028_write_data(client,
				ISL29028_REG_ADD_CONFIGURE,
				CONFIGURE_ALS_IR_MODE_ALS,
				CONFIGURE_ALS_IR_MODE_MASK,
				CONFIGURE_ALS_IR_MODE_SH);
			if (st)
				st = isl29028_write_data(client,
					ISL29028_REG_ADD_CONFIGURE,
					CONFIGURE_ALS_RANGE_HIGH_LUX,
					CONFIGURE_ALS_RANGE_MASK,
					CONFIGURE_ALS_RANGE_SH);
			if (st)
				st = isl29028_set_irals_high_threshold(client,
					chip->als_high_thres);
			if (st)
				st = isl29028_set_irals_low_threshold(client,
					chip->als_low_thres);
		} else {
			dev_dbg(&client->dev, "Enabling IR mode\n");
			st = isl29028_write_data(client,
				ISL29028_REG_ADD_CONFIGURE,
				CONFIGURE_ALS_IR_MODE_IR,
				CONFIGURE_ALS_IR_MODE_MASK,
				CONFIGURE_ALS_IR_MODE_SH);
			if (st)
				st = isl29028_set_irals_high_threshold(client,
					chip->ir_high_thres);
			if (st)
				st = isl29028_set_irals_low_threshold(client,
					chip->ir_low_thres);
		}
		if (st)
			st = isl29028_write_data(client,
				ISL29028_REG_ADD_CONFIGURE,
				CONFIGURE_ALS_EN,
				CONFIGURE_ALS_EN_MASK,
				CONFIGURE_ALS_EN_SH);
	} else {
		st = isl29028_write_data(client,
			ISL29028_REG_ADD_CONFIGURE,
			CONFIGURE_ALS_DIS,
			CONFIGURE_ALS_EN_MASK,
			CONFIGURE_ALS_EN_SH);
	}
	return st;
}

static bool isl29028_read_als_ir(struct i2c_client *client, int *als_ir)
{
	s32 lsb;
	s32 msb;
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	if (chip && chip->shutdown_complete)
		return false;

	lsb = i2c_smbus_read_byte_data(client, ISL29028_REG_ADD_ALSIR_L);
	if (lsb < 0) {
		dev_err(&client->dev, "Error in reading register %d, error %d\n",
				ISL29028_REG_ADD_ALSIR_L, lsb);
		return false;
	}

	msb = i2c_smbus_read_byte_data(client, ISL29028_REG_ADD_ALSIR_U);
	if (msb < 0) {
		dev_err(&client->dev, "Error in reading register %d, error %d\n",
				ISL29028_REG_ADD_ALSIR_U, lsb);
		return false;
	}
	*als_ir = ((msb & 0xF) << 8) | (lsb & 0xFF);
	return true;
}

static bool isl29028_read_proxim(struct i2c_client *client, int *prox)
{
	s32 data;
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	if (chip && chip->shutdown_complete)
		return false;

	data = i2c_smbus_read_byte_data(client, ISL29028_REG_ADD_PROX_DATA);
	if (data < 0) {
		dev_err(&client->dev, "Error in reading register %d, error %d\n",
				ISL29028_REG_ADD_PROX_DATA, data);
		return false;
	}
	*prox = (int)data;
	return true;
}

/* Sysfs interface */
/* proximity period  */
static ssize_t show_prox_period(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->prox_period);
}

static ssize_t store_prox_period(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	mutex_lock(&chip->lock);
	st = isl29028_set_proxim_period(client, chip->is_prox_enable,
				(int)lval);
	if (st)
		chip->prox_period = (int)lval;
	else
		dev_err(dev, "Error in setting the proximity period\n");

	mutex_unlock(&chip->lock);
	return count;
}

/* proximity enable/disable  */
static ssize_t show_prox_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);

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
	struct isl29028_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st = true;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;
	if ((lval != 1) && (lval != 0)) {
		dev_err(dev, "illegal value %lu\n", lval);
		return -EINVAL;
	}

	mutex_lock(&chip->lock);

	if (lval == chip->is_prox_enable) {
		mutex_unlock(&chip->lock);
		return count;
	}

	if (lval) {
		switch (chip->als_ir_mode) {
		case MODE_NONE:
			if (chip->isl_reg)
				regulator_enable(chip->isl_reg);
		case MODE_ALS:
		case MODE_IR:
			st = isl29028_set_proxim_period(client, true,
							chip->prox_period);
		}
	} else {
		switch (chip->als_ir_mode) {
		case MODE_ALS:
		case MODE_IR:
			st = isl29028_set_proxim_period(client, false,
							chip->prox_period);
			break;
		case MODE_NONE:
			if (chip->isl_reg)
				regulator_disable(chip->isl_reg);
		}
	}
	if (st)
		chip->is_prox_enable = (lval) ? true : false;
	else
		dev_err(dev, "Error in enabling proximity\n");

	mutex_unlock(&chip->lock);
	return count;
}

/* als/ir enable/disable  */
static ssize_t show_als_ir_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "Current Mode: %d [0:None, 1:ALS, 2:IR]\n",
			chip->als_ir_mode);
}

static ssize_t store_als_ir_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st = true;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;
	if (lval > 2) {
		dev_err(dev, "illegal value %lu\n", lval);
		return -EINVAL;
	}

	mutex_lock(&chip->lock);

	if (lval == chip->is_prox_enable) {
		mutex_unlock(&chip->lock);
		return count;
	}

	switch (lval) {
	case MODE_NONE:
		if (chip->is_prox_enable)
			st = isl29028_set_als_ir_mode(client, false, false);
		else if (chip->isl_reg)
			regulator_disable(chip->isl_reg);
			break;
	case MODE_ALS:
	case MODE_IR:
		if (!chip->is_prox_enable && chip->isl_reg)
			regulator_enable(chip->isl_reg);
		st = isl29028_set_als_ir_mode(client, true, (lval == MODE_ALS));
	}

	if (st)
		chip->als_ir_mode = (int)lval;
	else
		dev_err(dev, "Error in enabling als/ir mode\n");

	mutex_unlock(&chip->lock);
	return count;
}

/* Proximity low thresholds  */
static ssize_t show_proxim_low_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->prox_low_thres);
}

static ssize_t store_proxim_low_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 0xFF) || (lval < 0x0)) {
		dev_err(dev, "The threshold is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	st = isl29028_set_proxim_low_threshold(client, (u8)lval);
	if (st)
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
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->prox_high_thres);
}

static ssize_t store_proxim_high_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 0xFF) || (lval < 0x0)) {
		dev_err(dev, "The threshold is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	st = isl29028_set_proxim_high_threshold(client, (u8)lval);
	if (st)
		chip->prox_high_thres = (int)lval;
	else
		dev_err(dev, "Error in setting proximity high threshold\n");

	mutex_unlock(&chip->lock);
	return count;
}

/* als low thresholds  */
static ssize_t show_als_low_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->als_low_thres);
}

static ssize_t store_als_low_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
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
	if (chip->als_ir_mode == MODE_ALS) {
		st = isl29028_set_irals_low_threshold(client, (int)lval);
		if (st)
			chip->als_low_thres = (int)lval;
		else
			dev_err(dev, "Error in setting als low threshold\n");
	} else
		chip->als_low_thres = (int)lval;
	mutex_unlock(&chip->lock);
	return count;
}

/* Als high thresholds  */
static ssize_t show_als_high_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->als_high_thres);
}

static ssize_t store_als_high_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 0xFFF) || (lval < 0x0)) {
		dev_err(dev, "The als threshold is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	if (chip->als_ir_mode == MODE_ALS) {
		st = isl29028_set_irals_high_threshold(client, (int)lval);
		if (st)
			chip->als_high_thres = (int)lval;
		else
			dev_err(dev, "Error in setting als high threshold\n");
	} else
		chip->als_high_thres = (int)lval;
	mutex_unlock(&chip->lock);
	return count;
}

/* IR low thresholds  */
static ssize_t show_ir_low_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->ir_low_thres);
}

static ssize_t store_ir_low_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;
	unsigned long lval;

	dev_vdbg(dev, "%s()\n", __func__);

	if (strict_strtoul(buf, 10, &lval))
		return -EINVAL;

	if ((lval > 0xFFF) || (lval < 0x0)) {
		dev_err(dev, "The IR threshold is not supported\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	if (chip->als_ir_mode == MODE_IR) {
		st = isl29028_set_irals_low_threshold(client, (int)lval);
		if (st)
			chip->ir_low_thres = (int)lval;
		else
			dev_err(dev, "Error in setting als low threshold\n");
	} else
		chip->ir_low_thres = (int)lval;
	mutex_unlock(&chip->lock);
	return count;
}

/* IR high thresholds  */
static ssize_t show_ir_high_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->ir_high_thres);
}

static ssize_t store_ir_high_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
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
	if (chip->als_ir_mode == MODE_IR) {
		st = isl29028_set_irals_high_threshold(client, (int)lval);
		if (st)
			chip->ir_high_thres = (int)lval;
		else
			dev_err(dev, "Error in setting als high threshold\n");
	} else
		chip->ir_high_thres = (int)lval;
	mutex_unlock(&chip->lock);
	return count;
}

/* Proximity persist  */
static ssize_t show_proxim_persist(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->prox_persist);
}

static ssize_t store_proxim_persist(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
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
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_vdbg(dev, "%s()\n", __func__);
	return sprintf(buf, "%d\n", chip->als_persist);
}

static ssize_t store_als_persist(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
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
	struct isl29028_chip *chip = iio_priv(indio_dev);
	int prox_data = 0;
	bool st;
	ssize_t buf_count = 0;

	dev_vdbg(dev, "%s()\n", __func__);
	mutex_lock(&chip->lock);

	if (chip->is_prox_enable) {
		st = isl29028_read_proxim(chip->client, &prox_data);
		if (st) {
			buf_count = sprintf(buf, "%d\n", prox_data);
			chip->prox_reading = prox_data;
		}
	} else
		buf_count = sprintf(buf, "%d\n", chip->prox_reading);

	mutex_unlock(&chip->lock);
	return buf_count;
}

/* Display als data  */
static ssize_t show_als_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	int als_ir_data;
	bool st;
	ssize_t buf_count = 0;

	dev_vdbg(dev, "%s()\n", __func__);
	mutex_lock(&chip->lock);

	if (chip->als_ir_mode == MODE_ALS) {
		st = isl29028_read_als_ir(chip->client, &als_ir_data);
		if (st) {
			/* convert als data count to lux */
			/* if als_range = 0, lux = count * 0.0326 */
			/* if als_range = 1, lux = count * 0.522 */
			if (!chip->als_range)
				als_ir_data = (als_ir_data * 326) / 10000;
			else
				als_ir_data = (als_ir_data * 522) / 1000;

			buf_count = sprintf(buf, "%d\n", als_ir_data);
			chip->als_reading = als_ir_data;
		}
	} else
		buf_count = sprintf(buf, "%d\n", chip->als_reading);
	mutex_unlock(&chip->lock);
	return buf_count;
}

/* Display IR data  */
static ssize_t show_ir_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	int als_ir_data;
	bool st;
	ssize_t buf_count = 0;

	dev_vdbg(dev, "%s()\n", __func__);
	mutex_lock(&chip->lock);

	if (chip->als_ir_mode == MODE_IR) {
		st = isl29028_read_als_ir(chip->client, &als_ir_data);
		if (st) {
			buf_count = sprintf(buf, "%d\n", als_ir_data);
			chip->ir_reading = als_ir_data;
		}
	} else
		buf_count = sprintf(buf, "%d\n", chip->ir_reading);
	mutex_unlock(&chip->lock);
	return buf_count;
}

/* Wait for the proximity threshold interrupt*/
static ssize_t show_wait_proxim_int(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;

	dev_vdbg(dev, "%s()\n", __func__);
	if (!chip->is_int_enable) {
		dev_err(dev, "%s() Interrupt mode not supported\n", __func__);
		return sprintf(buf, "error\n");
	}

	mutex_lock(&chip->lock);
	st = isl29028_set_proxim_persist(client, true, chip->prox_persist);
	if (!st) {
		dev_err(dev, "%s() Error in configuration\n", __func__);
		mutex_unlock(&chip->lock);
		return sprintf(buf, "error\n");
	}

	chip->is_proxim_int_waiting =  true;
	mutex_unlock(&chip->lock);
	wait_for_completion(&chip->prox_completion);
	mutex_lock(&chip->lock);
	chip->is_proxim_int_waiting =  false;
	isl29028_set_proxim_persist(client, false, chip->prox_persist);
	mutex_unlock(&chip->lock);
	return sprintf(buf, "done\n");
}

/* Wait for the als/ir interrupt*/
static ssize_t show_wait_als_ir_int(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	struct i2c_client *client = chip->client;
	bool st;

	dev_vdbg(dev, "%s()\n", __func__);
	if (!chip->is_int_enable) {
		dev_err(dev, "%s() Interrupt mode not supported\n", __func__);
		return sprintf(buf, "error\n");
	}

	mutex_lock(&chip->lock);

	st = isl29028_set_als_persist(client, true, chip->als_persist);
	if (!st) {
		dev_err(dev, "%s() Error in als ir int configuration\n",
					 __func__);
		mutex_unlock(&chip->lock);
		return sprintf(buf, "error\n");
	}

	chip->is_als_int_waiting =  true;
	mutex_unlock(&chip->lock);
	wait_for_completion(&chip->als_completion);
	mutex_lock(&chip->lock);
	chip->is_als_int_waiting =  false;
	st = isl29028_set_als_persist(client, false, chip->als_persist);
	mutex_unlock(&chip->lock);
	return sprintf(buf, "done\n");
}

/* Read name */
static ssize_t show_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	return sprintf(buf, "%s\n", chip->client->name);
}

static IIO_DEVICE_ATTR(proximity_low_threshold, S_IRUGO | S_IWUSR,
		show_proxim_low_threshold, store_proxim_low_threshold, 0);
static IIO_DEVICE_ATTR(proximity_high_threshold, S_IRUGO | S_IWUSR,
		show_proxim_high_threshold, store_proxim_high_threshold, 0);
static IIO_DEVICE_ATTR(proximity_persist, S_IRUGO | S_IWUSR,
		show_proxim_persist, store_proxim_persist, 0);
static IIO_DEVICE_ATTR(proximity_period, S_IRUGO | S_IWUSR,
		show_prox_period, store_prox_period, 0);
static IIO_DEVICE_ATTR(proximity_enable, S_IRUGO | S_IWUSR,
		show_prox_enable, store_prox_enable, 0);
static IIO_DEVICE_ATTR(wait_proxim_thres, S_IRUGO,
		show_wait_proxim_int, NULL, 0);
static IIO_DEVICE_ATTR(proximity_value, S_IRUGO,
		show_proxim_data, NULL, 0);

static IIO_DEVICE_ATTR(als_low_threshold, S_IRUGO | S_IWUSR,
		show_als_low_threshold, store_als_low_threshold, 0);
static IIO_DEVICE_ATTR(als_high_threshold, S_IRUGO | S_IWUSR,
		show_als_high_threshold, store_als_high_threshold, 0);
static IIO_DEVICE_ATTR(als_persist, S_IRUGO | S_IWUSR,
		show_als_persist, store_als_persist, 0);
static IIO_DEVICE_ATTR(als_ir_mode, S_IRUGO | S_IWUSR,
		show_als_ir_mode, store_als_ir_mode, 0);
static IIO_DEVICE_ATTR(als_value, S_IRUGO,
		show_als_data, NULL, 0);
static IIO_DEVICE_ATTR(wait_als_ir_thres, S_IRUGO,
		show_wait_als_ir_int, NULL, 0);

static IIO_DEVICE_ATTR(ir_value, S_IRUGO,
		show_ir_data, NULL, 0);
static IIO_DEVICE_ATTR(ir_low_threshold, S_IRUGO | S_IWUSR,
		show_ir_low_threshold, store_ir_low_threshold, 0);
static IIO_DEVICE_ATTR(ir_high_threshold, S_IRUGO | S_IWUSR,
		show_ir_high_threshold, store_ir_high_threshold, 0);

static IIO_DEVICE_ATTR(name, S_IRUGO, show_name, NULL, 0);

static struct attribute *isl29028_attributes[] = {
	&iio_dev_attr_name.dev_attr.attr,

	&iio_dev_attr_ir_value.dev_attr.attr,

	&iio_dev_attr_als_low_threshold.dev_attr.attr,
	&iio_dev_attr_als_high_threshold.dev_attr.attr,
	&iio_dev_attr_als_persist.dev_attr.attr,
	&iio_dev_attr_als_ir_mode.dev_attr.attr,
	&iio_dev_attr_als_value.dev_attr.attr,
	&iio_dev_attr_wait_als_ir_thres.dev_attr.attr,
	&iio_dev_attr_ir_low_threshold.dev_attr.attr,
	&iio_dev_attr_ir_high_threshold.dev_attr.attr,

	&iio_dev_attr_proximity_low_threshold.dev_attr.attr,
	&iio_dev_attr_proximity_high_threshold.dev_attr.attr,
	&iio_dev_attr_proximity_enable.dev_attr.attr,
	&iio_dev_attr_proximity_period.dev_attr.attr,
	&iio_dev_attr_proximity_persist.dev_attr.attr,
	&iio_dev_attr_proximity_value.dev_attr.attr,
	&iio_dev_attr_wait_proxim_thres.dev_attr.attr,
	NULL
};

static const struct attribute_group isl29108_group = {
	.attrs = isl29028_attributes,
};

static int isl29028_chip_init(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct isl29028_chip *chip = iio_priv(indio_dev);
	int i;
	bool st;

	for (i = 0; i < ARRAY_SIZE(chip->reg_cache); i++)
		chip->reg_cache[i] = 0;

	chip->is_prox_enable  = 0;
	chip->prox_low_thres = 0;
	chip->prox_high_thres = 0xFF;
	chip->prox_period = ISL29028_PROX_PERIOD;
	chip->prox_reading = 0;

	chip->als_low_thres = 0;
	chip->als_high_thres = 0xFFF;
	chip->als_range = 1;
	chip->als_reading = 0;
	chip->als_ir_mode = 0;

	chip->ir_high_thres = 0xFFF;
	chip->ir_low_thres = 0;
	chip->ir_reading = 0;

	chip->is_int_enable = false;
	chip->prox_persist = 1;
	chip->als_persist = 1;
	chip->is_proxim_int_waiting = false;
	chip->is_als_int_waiting = false;
	chip->shutdown_complete = 0;

	/* if regulator is not available, then proceed with i2c write or
	 * if regulator is turned on then proceed with i2c write
	 */
	if (chip->isl_reg && !regulator_is_enabled(chip->isl_reg))
		return 0;

	st = isl29028_write_data(client, ISL29028_REG_ADD_TEST1_MODE,
					0x0, 0xFF, 0);
	if (st)
		st = isl29028_write_data(client, ISL29028_REG_ADD_TEST2_MODE,
					0x0, 0xFF, 0);
	if (st)
		st = isl29028_write_data(client, ISL29028_REG_ADD_CONFIGURE,
					0x0, 0xFF, 0);
	if (st)
		msleep(1);
	if (!st) {
		dev_err(&client->dev, "%s(): fails\n", __func__);
		return -ENODEV;
	}
	return 0;
}

static irqreturn_t threshold_isr(int irq, void *irq_data)
{
	struct isl29028_chip *chip = (struct isl29028_chip *)irq_data;
	s32 int_reg;
	struct i2c_client *client = chip->client;

	if (chip && chip->shutdown_complete)
		return -ENODEV;

	int_reg = i2c_smbus_read_byte_data(client, ISL29028_REG_ADD_INTERRUPT);
	if (int_reg < 0) {
		dev_err(&client->dev, "Error in reading register %d, error %d\n",
				ISL29028_REG_ADD_INTERRUPT, int_reg);
		return IRQ_HANDLED;
	}

	if (int_reg & INTERRUPT_PROX_FLAG_MASK) {
		/* Write 0 to clear */
		isl29028_write_data(client,
			ISL29028_REG_ADD_INTERRUPT, INTERRUPT_PROX_FLAG_DIS,
			INTERRUPT_PROX_FLAG_MASK, INTERRUPT_PROX_FLAG_SH);
		if (chip->is_proxim_int_waiting)
			complete(&chip->prox_completion);
	}

	if (int_reg & INTERRUPT_ALS_FLAG_MASK) {
		/* Write 0 to clear */
		isl29028_write_data(client,
			ISL29028_REG_ADD_INTERRUPT, INTERRUPT_ALS_FLAG_DIS,
			INTERRUPT_ALS_FLAG_MASK, INTERRUPT_ALS_FLAG_SH);
		if (chip->is_als_int_waiting)
			complete(&chip->als_completion);
	}

	return IRQ_HANDLED;
}

static const struct iio_info isl29028_info = {
	.attrs = &isl29108_group,
	.driver_module = THIS_MODULE,
};

static int __devinit isl29028_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct isl29028_chip *chip;
	struct iio_dev *indio_dev;
	struct regulator *isl_reg = regulator_get(&client->dev, "vdd");
	int err;

	dev_dbg(&client->dev, "%s() called\n", __func__);

	if (IS_ERR_OR_NULL(isl_reg)) {
		dev_info(&client->dev, "no regulator found," \
			"continue without regulator\n");
		isl_reg = NULL;
	}

	indio_dev = iio_allocate_device(sizeof(*chip));
	if (indio_dev == NULL) {
		dev_err(&client->dev, "iio allocation fails\n");
		err = -ENOMEM;
		goto exit;
	}
	chip = iio_priv(indio_dev);

	i2c_set_clientdata(client, indio_dev);
	chip->client = client;
	chip->irq = client->irq;
	chip->isl_reg = isl_reg;

	mutex_init(&chip->lock);

	err = isl29028_chip_init(client);
	if (err)
		goto exit_iio_free;

	init_completion(&chip->prox_completion);
	init_completion(&chip->als_completion);

	if (chip->irq > 0) {
		err = request_threaded_irq(chip->irq, NULL, threshold_isr,
						IRQF_SHARED, "ISL29028", chip);
		if (err) {
			dev_err(&client->dev, "Unable to register irq %d; "
				"error %d\n", chip->irq, err);
			goto exit_iio_free;
		}
		chip->is_int_enable = true;
	}

	indio_dev->info = &isl29028_info;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	err = iio_device_register(indio_dev);
	if (err) {
		dev_err(&client->dev, "iio registration fails\n");
		goto exit_irq;
	}
	dev_dbg(&client->dev, "%s() success\n", __func__);
	return 0;

exit_irq:
	if (chip->irq > 0)
		free_irq(chip->irq, chip);
exit_iio_free:
	if (chip->isl_reg)
		regulator_put(chip->isl_reg);
	iio_free_device(indio_dev);
exit:
	return err;
}

static int __devexit isl29028_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_dbg(&client->dev, "%s()\n", __func__);
	if (chip->irq > 0)
		free_irq(chip->irq, chip);
	if (chip->isl_reg)
		regulator_put(chip->isl_reg);
	iio_device_unregister(indio_dev);
	return 0;
}

static void isl29028_shutdown(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	mutex_lock(&chip->lock);
	if (chip->irq > 0)
		free_irq(chip->irq, chip);
	if (chip->isl_reg)
		regulator_put(chip->isl_reg);
	chip->shutdown_complete = 1;
	mutex_unlock(&chip->lock);
	iio_device_unregister(indio_dev);
	iio_free_device(indio_dev);
}

static const struct i2c_device_id isl29028_id[] = {
	{"isl29028", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, isl29028_id);

#ifdef CONFIG_PM
static int isl29028_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_dbg(&client->dev, "%s()\n", __func__);
	mutex_lock(&chip->lock);
	/* if regulator is available and regulator is enabled by isl29028
	 * then disable it
	 */
	if (chip->isl_reg && (chip->is_prox_enable || chip->als_ir_mode))
		regulator_disable(chip->isl_reg);
	/* if regulator is still enabled, put the device into shutdown */
	if (chip->isl_reg && regulator_is_enabled(chip->isl_reg))
		isl29028_write_data(client, ISL29028_REG_ADD_CONFIGURE,
					0x0, 0xFF, 0);
	mutex_unlock(&chip->lock);
	return 0;
}

static int isl29028_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct isl29028_chip *chip = iio_priv(indio_dev);

	dev_dbg(&client->dev, "%s()\n", __func__);
	mutex_lock(&chip->lock);
	if (chip->isl_reg && (chip->is_prox_enable || chip->als_ir_mode))
		regulator_enable(chip->isl_reg);
	switch (chip->als_ir_mode) {
	case MODE_ALS:
		isl29028_set_als_ir_mode(client, true, true);
		break;
	case MODE_IR:
		isl29028_set_als_ir_mode(client, true, false);
	}
	if (chip->is_prox_enable)
		isl29028_set_proxim_period(client, true, chip->prox_period);
	mutex_unlock(&chip->lock);
	return 0;
}

static SIMPLE_DEV_PM_OPS(isl29028_pm_ops, isl29028_suspend, isl29028_resume);
#define ISL29028_PM_OPS (&isl29028_pm_ops)
#else
#define ISL29028_PM_OPS NULL
#endif

static struct i2c_driver isl29028_driver = {
	.class	= I2C_CLASS_HWMON,
	.driver  = {
		.name = "isl29028",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = ISL29028_PM_OPS,
#endif
	},
	.probe	 = isl29028_probe,
	.remove  = __devexit_p(isl29028_remove),
	.id_table = isl29028_id,
	.shutdown = isl29028_shutdown,
};

static int __init isl29028_init(void)
{
	return i2c_add_driver(&isl29028_driver);
}

static void __exit isl29028_exit(void)
{
	i2c_del_driver(&isl29028_driver);
}

module_init(isl29028_init);
module_exit(isl29028_exit);

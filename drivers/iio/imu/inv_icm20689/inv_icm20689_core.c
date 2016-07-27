/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* Code was copied and modified from
* drivers/iio/imu/inv_mpu6050/inv_mpu_core.c
*
* Copyright (C) 2012 Invensense, Inc.
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
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include "inv_icm20689_iio.h"

/*
 * this is the gyro scale translated from dynamic range plus/minus
 * {250, 500, 1000, 2000} to rad/s
 */
static const int gyro_scale_20689[] = {133090, 266181, 532362, 1064724};

/*
 * this is the accel scale translated from dynamic range plus/minus
 * {2, 4, 8, 16} to m/s^2
 */
static const int accel_scale[] = {598, 1196, 2392, 4785};

static const struct inv_icm20689_reg_map reg_set_20689 = {
	.sample_rate_div        = INV_ICM20689_REG_SAMPLE_RATE_DIV,
	.config                 = INV_ICM20689_REG_CONFIG,
	.user_ctrl              = INV_ICM20689_REG_USER_CTRL,
	.fifo_en                = INV_ICM20689_REG_FIFO_EN,
	.gyro_config            = INV_ICM20689_REG_GYRO_CONFIG,
	.accl_config            = INV_ICM20689_REG_ACCEL_CONFIG,
	.accl_config2           = INV_ICM20689_REG_ACCEL_CONFIG2,
	.fifo_count_h           = INV_ICM20689_REG_FIFO_COUNT_H,
	.fifo_r_w               = INV_ICM20689_REG_FIFO_R_W,
	.raw_gyro               = INV_ICM20689_REG_RAW_GYRO,
	.raw_accl               = INV_ICM20689_REG_RAW_ACCEL,
	.temperature            = INV_ICM20689_REG_TEMPERATURE,
	.int_enable             = INV_ICM20689_REG_INT_ENABLE,
	.pwr_mgmt_1             = INV_ICM20689_REG_PWR_MGMT_1,
	.pwr_mgmt_2             = INV_ICM20689_REG_PWR_MGMT_2,
	.whoami                 = INV_ICM20689_REG_WHOAMI,
};

static struct inv_icm20689_chip_config chip_config_20689[] = {
	/* this is for 8K fifo rate */
	[0] = {
	.fifo_rate = INV_ICM20689_GYRO_8K_RATE,
	.gyro_lpf = INV_ICM20689_GYRO_LFP_250HZ,
	.acc_lpf  = INV_ICM20689_ACC_LFP_218HZ,
	.gyro_fsr = INV_ICM20689_FS_500DPS,
	.acc_fsr  = INV_ICM20689_FS_04G,
	.gyro_fifo_enable = false,
	.accl_fifo_enable = false,
	},

	/* this is for the init config */
	[1] = {
	.fifo_rate = INV_ICM20689_INIT_FIFO_RATE,
	.gyro_lpf = INV_ICM20689_GYRO_LFP_20HZ,
	.acc_lpf  = INV_ICM20689_ACC_LFP_21HZ,
	.gyro_fsr = INV_ICM20689_FS_250DPS,
	.acc_fsr  = INV_ICM20689_FS_02G,
	.gyro_fifo_enable = false,
	.accl_fifo_enable = false,
	},

};

static struct inv_icm20689_hw hw_info[INV_NUM_PARTS] = {
	{
		.num_reg = 117,
		.name = "icm20689",
		.reg = &reg_set_20689,
		.config = &chip_config_20689[1],
	},
};

int inv_icm20689_write_reg(struct inv_icm20689_state *st, int reg, u8 d)
{
	int rc;
	struct spi_message  m;
	struct spi_transfer t;

	memset(&t, 0, sizeof(t));
	t.speed_hz = MPU_SPI_FREQUENCY_5MHZ;

	spi_message_init(&m);

	st->tx_buf[0] = reg;
	st->tx_buf[1] = d;
	t.rx_buf = NULL;
	t.tx_buf = st->tx_buf;
	t.len = 2;
	t.bits_per_word = 8;

	spi_message_add_tail(&t, &m);
	rc = spi_sync(st->spi, &m);
	if (rc)
		pr_err("write icm20689 reg %u failed, rc %d\n", reg, rc);

	return rc;
}

int inv_icm20689_read_reg(struct inv_icm20689_state *st, uint8_t reg,
		uint8_t *val)
{
	int rc;
	struct spi_message m;
	struct spi_transfer t;

	memset(&t, 0, sizeof(t));
	t.speed_hz = MPU_SPI_FREQUENCY_5MHZ;

	spi_message_init(&m);

	/* register high bit=1 for read */
	st->tx_buf[0] = reg | 0x80;

	t.rx_buf = st->rx_buf;
	t.tx_buf = st->tx_buf;
	t.len = 2;
	t.bits_per_word = 8;

	spi_message_add_tail(&t, &m);
	rc = spi_sync(st->spi, &m);

	if (rc)
		pr_err("mpu get reg %u failed, rc %d\n", reg, rc);
	else
		*val = st->rx_buf[1];

	return rc;
}

int inv_icm20689_spi_bulk_read(struct inv_icm20689_state *st, int reg,
		uint8_t length, uint8_t *buf)
{
	int rc;
	struct spi_message m;
	struct spi_transfer t;

	if (length > (MPU_SPI_BUF_LEN - 1)) {
		pr_err("SPI Bytes requested (%u) exceed buffer size (%u)\n",
				length, MPU_SPI_BUF_LEN - 1);
	    return -EINVAL;
	}

	memset(&t, 0, sizeof(t));
	t.speed_hz = MPU_SPI_FREQUENCY_8MHZ;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	/* register high bit=1 for read */
	st->tx_buf[0] = reg | 0x80;

	t.rx_buf = st->rx_buf;
	t.tx_buf = st->tx_buf;
	t.len = length + 1;
	t.bits_per_word = 8;
	rc = spi_sync(st->spi, &m);

	if (rc)
		return -EIO;

	memcpy(buf, st->rx_buf + 1, length);

	return 0;
}

int inv_icm20689_set_power_itg(struct inv_icm20689_state *st, bool power_on)
{
	int result;

	if (power_on)
		result = inv_icm20689_write_reg(st, st->reg->pwr_mgmt_1, 0);
	else
		result = inv_icm20689_write_reg(st, st->reg->pwr_mgmt_1,
						INV_ICM20689_BIT_SLEEP);
	if (result)
		return result;

	if (power_on)
		msleep(INV_ICM20689_SENSOR_UP_TIME);

	return 0;
}

int inv_icm20689_switch_engine(struct inv_icm20689_state *st,
		bool en, u32 mask)
{
	u8 d, mgmt_1;
	int result;

	/* switch clock needs to be careful. Only when gyro is on, can
	   clock source be switched to gyro. Otherwise, it must be set to
	   internal clock */
	if (INV_ICM20689_BIT_PWR_GYRO_STBY == mask) {
		result = inv_icm20689_read_reg(st, st->reg->pwr_mgmt_1,
				&mgmt_1);
		if (result != 1)
			return result;

		mgmt_1 &= ~INV_ICM20689_BIT_CLK_MASK;
	}

	if ((INV_ICM20689_BIT_PWR_GYRO_STBY == mask) && (!en)) {
		/* turning off gyro requires switch to internal clock first.
		   Then turn off gyro engine */
		mgmt_1 |= INV_ICM20689_CLK_INTERNAL;
		result = inv_icm20689_write_reg(st, st->reg->pwr_mgmt_1,
				mgmt_1);
		if (result)
			return result;
	}

	result = inv_icm20689_read_reg(st, st->reg->pwr_mgmt_2, &d);
	if (result != 1)
		return result;
	if (en)
		d &= ~mask;
	else
		d |= mask;
	result = inv_icm20689_write_reg(st, st->reg->pwr_mgmt_2, d);
	if (result)
		return result;

	if (en) {
		/* Wait for output stable */
		msleep(INV_ICM20689_TEMP_UP_TIME);
		if (INV_ICM20689_BIT_PWR_GYRO_STBY == mask) {
			/* switch internal clock to PLL */
			mgmt_1 |= INV_ICM20689_CLK_PLL;
			result = inv_icm20689_write_reg(st, st->reg->pwr_mgmt_1,
					mgmt_1);
			if (result)
				return result;
		}
	}

	return 0;
}

static int inv_icm20689_init_config(struct iio_dev *indio_dev)
{
	int result;
	u8 d;
	struct inv_icm20689_state *st = iio_priv(indio_dev);

	result = inv_icm20689_set_power_itg(st, true);
	if (result)
		return result;

	d = 0;
	if (st->hw->config->fifo_rate <= INV_ICM20689_MAX_FIFO_RATE)
		d = INV_ICM20689_ONE_K_HZ / (st->hw->config->fifo_rate) - 1;
	result = inv_icm20689_write_reg(st, st->reg->sample_rate_div, d);
	if (result)
		return result;

	/* gyro lpf */
	result = inv_icm20689_write_reg(st, st->reg->config,
			st->hw->config->gyro_lpf);
	if (result)
		return result;

	/* gyro fs */
	d = st->hw->config->gyro_fsr << INV_ICM20689_GYRO_CONFIG_FSR_SHIFT;
	result = inv_icm20689_write_reg(st, st->reg->gyro_config, d);
	if (result)
		return result;

	/* acc lpf */
	result = inv_icm20689_write_reg(st, st->reg->accl_config2,
			st->hw->config->acc_lpf);
	if (result)
		return result;

	/* acc fs */
	d = st->hw->config->acc_fsr << 3;
	result = inv_icm20689_write_reg(st, st->reg->accl_config, d);
	if (result)
		return result;

	memcpy(&st->chip_config, hw_info[st->chip_type].config,
		sizeof(struct inv_icm20689_chip_config));
	result = inv_icm20689_set_power_itg(st, false);

	return result;
}

static int inv_icm20689_sensor_show(struct inv_icm20689_state  *st,
		int reg, int axis, int *val)
{
	int ind, result;
	__be16 d;

	ind = (axis - IIO_MOD_X) * 2;

	result = inv_icm20689_spi_bulk_read(st, reg + ind, 2, (u8 *)&d);

	if (result)
		return -EINVAL;
	*val = (short)be16_to_cpup(&d);

	return IIO_VAL_INT;
}

static int inv_icm20689_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val,
			      int *val2,
			      long mask) {
	struct inv_icm20689_state  *st = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
	{
		int ret, result;

		ret = IIO_VAL_INT;
		result = 0;
		mutex_lock(&indio_dev->mlock);
		if (!st->chip_config.enable) {
			result = inv_icm20689_set_power_itg(st, true);
			if (result)
				goto error_read_raw;
		}
		/* when enable is on, power is already on */
		switch (chan->type) {
		case IIO_ANGL_VEL:
			if (!st->chip_config.gyro_fifo_enable ||
					!st->chip_config.enable) {
				result = inv_icm20689_switch_engine(st, true,
						INV_ICM20689_BIT_PWR_GYRO_STBY);
				if (result)
					goto error_read_raw;
			}
			ret =  inv_icm20689_sensor_show(st, st->reg->raw_gyro,
						chan->channel2, val);
			if (!st->chip_config.gyro_fifo_enable ||
					!st->chip_config.enable) {
				result = inv_icm20689_switch_engine(st, false,
						INV_ICM20689_BIT_PWR_GYRO_STBY);
				if (result)
					goto error_read_raw;
			}
			break;
		case IIO_ACCEL:
			if (!st->chip_config.accl_fifo_enable ||
					!st->chip_config.enable) {
				result = inv_icm20689_switch_engine(st, true,
						INV_ICM20689_BIT_PWR_ACCL_STBY);
				if (result)
					goto error_read_raw;
			}
			ret = inv_icm20689_sensor_show(st, st->reg->raw_accl,
						chan->channel2, val);
			if (!st->chip_config.accl_fifo_enable ||
					!st->chip_config.enable) {
				result = inv_icm20689_switch_engine(st, false,
						INV_ICM20689_BIT_PWR_ACCL_STBY);
				if (result)
					goto error_read_raw;
			}
			break;
		case IIO_TEMP:
			/* wait for stable */
			msleep(INV_ICM20689_SENSOR_UP_TIME);
			ret = inv_icm20689_sensor_show(st, st->reg->temperature,
							IIO_MOD_X, val);
			break;
		default:
			ret = -EINVAL;
			break;
		}
error_read_raw:
		if (!st->chip_config.enable)
			result |= inv_icm20689_set_power_itg(st, false);
		mutex_unlock(&indio_dev->mlock);
		if (result)
			return result;

		return ret;
	}
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_ANGL_VEL:
			*val  = 0;
			*val2 = gyro_scale_20689[st->chip_config.gyro_fsr];

			return IIO_VAL_INT_PLUS_NANO;
		case IIO_ACCEL:
			*val = 0;
			*val2 = accel_scale[st->chip_config.acc_fsr];

			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_TEMP:
			*val = 0;
			*val2 = INV_ICM20689_TEMP_SCALE;

			return IIO_VAL_INT_PLUS_MICRO;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		switch (chan->type) {
		case IIO_TEMP:
			*val = INV_ICM20689_TEMP_OFFSET;

			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

static int inv_icm20689_write_fsr(struct inv_icm20689_state *st, int fsr)
{
	int result;
	u8 d;

	if (fsr < 0 || fsr > INV_ICM20689_MAX_GYRO_FS_PARAM)
		return -EINVAL;
	if (fsr == st->chip_config.gyro_fsr)
		return 0;

	d = (fsr << INV_ICM20689_GYRO_CONFIG_FSR_SHIFT);
	result = inv_icm20689_write_reg(st, st->reg->gyro_config, d);
	if (result)
		return result;
	st->chip_config.gyro_fsr = fsr;

	return 0;
}

static int inv_icm20689_write_accel_fs(struct inv_icm20689_state *st, int fs)
{
	int result;
	u8 d;

	if (fs < 0 || fs > INV_ICM20689_MAX_ACCL_FS_PARAM)
		return -EINVAL;
	if (fs == st->chip_config.acc_fsr)
		return 0;

	d = (fs << INV_ICM20689_ACCL_CONFIG_FSR_SHIFT);
	result = inv_icm20689_write_reg(st, st->reg->accl_config, d);
	if (result)
		return result;
	st->chip_config.acc_fsr = fs;

	return 0;
}

static int inv_icm20689_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask) {
	struct inv_icm20689_state  *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	/* we should only update scale when the chip is disabled, i.e.,
		not running */
	if (st->chip_config.enable) {
		result = -EBUSY;
		goto error_write_raw;
	}
	result = inv_icm20689_set_power_itg(st, true);
	if (result)
		goto error_write_raw;

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_ANGL_VEL:
			result = inv_icm20689_write_fsr(st, val);
			break;
		case IIO_ACCEL:
			result = inv_icm20689_write_accel_fs(st, val);
			break;
		default:
			result = -EINVAL;
			break;
		}
		break;
	default:
		result = -EINVAL;
		break;
	}

error_write_raw:
	result |= inv_icm20689_set_power_itg(st, false);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

/**
 *  inv_icm20689_set_gyro_lpf() - set low pass filer based on fifo rate.
 *
 *                  Based on the Nyquist principle, the sampling rate must
 *                  exceed twice of the bandwidth of the signal, or there
 *                  would be alising. This function basically search for the
 *                  correct low pass parameters based on the fifo rate, e.g,
 *                  sampling frequency.
 */
static int inv_icm20689_set_gyro_lpf(struct inv_icm20689_state *st, int rate)
{
	const int hz[] = {250, 176, 92, 41, 20, 10};
	const int d[] = {INV_ICM20689_GYRO_LFP_250HZ,
			INV_ICM20689_GYRO_LFP_176HZ,
			INV_ICM20689_GYRO_LFP_92HZ, INV_ICM20689_GYRO_LFP_41HZ,
			INV_ICM20689_GYRO_LFP_20HZ, INV_ICM20689_GYRO_LFP_10HZ};
	int i, h;
	u8 data;

	h = (rate >> 1);
	i = 0;
	while ((h < hz[i]) && (i < ARRAY_SIZE(d) - 1))
		i++;
	data = d[i];

	st->chip_config.gyro_lpf = data;

	return 0;
}

/**
 * inv_icm20689_fifo_rate_store() - Set fifo rate.
 */
static ssize_t inv_icm20689_fifo_rate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	s32 fifo_rate;
	int result;
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20689_state *st = iio_priv(indio_dev);

	if (kstrtoint(buf, 10, &fifo_rate))
		return -EINVAL;
#if INV20689_SMD_IRQ_TRIGGER
	if (fifo_rate < INV_ICM20689_MIN_FIFO_RATE ||
			(fifo_rate > INV_ICM20689_MAX_FIFO_RATE &&
				fifo_rate != INV_ICM20689_GYRO_8K_RATE))
		return -EINVAL;
#else
	if (fifo_rate < INV_ICM20689_MIN_FIFO_RATE ||
			fifo_rate > INV_ICM20689_MAX_FIFO_RATE)
		return -EINVAL;
#endif

	if (fifo_rate == st->chip_config.fifo_rate)
		return count;

	mutex_lock(&indio_dev->mlock);
	if (st->chip_config.enable) {
		result = -EBUSY;
		goto fifo_rate_fail;
	}

	inv_icm20689_set_gyro_lpf(st, fifo_rate);

#if INV20689_SMD_IRQ_TRIGGER
	if (INV_ICM20689_GYRO_8K_RATE == fifo_rate)
		hw_info[st->chip_type].config = &chip_config_20689[0];
	else
#endif
		hw_info[st->chip_type].config = &chip_config_20689[1];

	st->hw->config->gyro_lpf = st->chip_config.gyro_lpf;
	st->hw->config->fifo_rate = fifo_rate;

	result = inv_icm20689_init_config(indio_dev);
	if (result)
		goto fifo_rate_fail;

	st->chip_config.fifo_rate = fifo_rate;

fifo_rate_fail:
	result |= inv_icm20689_set_power_itg(st, false);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

/**
 * inv_fifo_rate_show() - Get the current sampling rate.
 */
static ssize_t inv_fifo_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_icm20689_state *st = iio_priv(dev_to_iio_dev(dev));

	return snprintf(buf, 8, "%d\n", st->chip_config.fifo_rate);
}

/**
 * inv_attr_show() - calling this function will show current
 *                    parameters.
 */
static ssize_t inv_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_icm20689_state *st = iio_priv(dev_to_iio_dev(dev));
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	s8 *m;

	switch (this_attr->address) {
	/* In icm20689, the two matrix are the same because gyro and accel
	   are integrated in one chip */
	case ATTR_ICM20689_GYRO_MATRIX:
	case ATTR_ICM20689_ACCL_MATRIX:
		m = st->plat_data.orientation;

		return snprintf(buf, 28,
			"%d, %d, %d; %d, %d, %d; %d, %d, %d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	default:
		return -EINVAL;
	}
}

/**
 * inv_icm20689_validate_trigger() - validate_trigger callback for invensense
 *                                  icm20689 device.
 * @indio_dev: The IIO device
 * @trig: The new trigger
 *
 * Returns: 0 if the 'trig' matches the trigger registered by the icm20689
 * device, -EINVAL otherwise.
 */
static int inv_icm20689_validate_trigger(struct iio_dev *indio_dev,
					struct iio_trigger *trig)
{
	struct inv_icm20689_state *st = iio_priv(indio_dev);

	if (st->trig != trig)
		return -EINVAL;

	return 0;
}

#define INV_ICM20689_CHAN(_type, _channel2, _index)                    \
	{                                                             \
		.type = _type,                                        \
		.modified = 1,                                        \
		.channel2 = _channel2,                                \
		.info_mask_shared_by_type =  BIT(IIO_CHAN_INFO_SCALE), \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),         \
		.scan_index = _index,                                 \
		.scan_type = {                                        \
				.sign = 's',                          \
				.realbits = 16,                       \
				.storagebits = 16,                    \
				.shift = 0 ,                          \
				.endianness = IIO_BE,                 \
			     },                                       \
	}

static const struct iio_chan_spec inv_mpu_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(INV_ICM20689_SCAN_TIMESTAMP),

	{
		.type = IIO_TEMP,
		.info_mask_separate =  BIT(IIO_CHAN_INFO_RAW)
				| BIT(IIO_CHAN_INFO_OFFSET)
				| BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = INV_ICM20689_SCAN_TEMP,
		.channel2 = IIO_MOD_X,
		.scan_type = {
				.sign = 's',
				.realbits = 16,
				.storagebits = 16,
				.shift = 0,
				.endianness = IIO_BE,
			     },
	},
	INV_ICM20689_CHAN(IIO_ANGL_VEL, IIO_MOD_X, INV_ICM20689_SCAN_GYRO_X),
	INV_ICM20689_CHAN(IIO_ANGL_VEL, IIO_MOD_Y, INV_ICM20689_SCAN_GYRO_Y),
	INV_ICM20689_CHAN(IIO_ANGL_VEL, IIO_MOD_Z, INV_ICM20689_SCAN_GYRO_Z),

	INV_ICM20689_CHAN(IIO_ACCEL, IIO_MOD_X, INV_ICM20689_SCAN_ACCL_X),
	INV_ICM20689_CHAN(IIO_ACCEL, IIO_MOD_Y, INV_ICM20689_SCAN_ACCL_Y),
	INV_ICM20689_CHAN(IIO_ACCEL, IIO_MOD_Z, INV_ICM20689_SCAN_ACCL_Z),
};

/* constant IIO attribute */
#if INV20689_SMD_IRQ_TRIGGER
static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("50 100 200 8000");
#else
static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("50 100 200");
#endif
static IIO_DEV_ATTR_SAMP_FREQ(S_IRUGO | S_IWUSR, inv_fifo_rate_show,
	inv_icm20689_fifo_rate_store);
static IIO_DEVICE_ATTR(in_gyro_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_ICM20689_GYRO_MATRIX);
static IIO_DEVICE_ATTR(in_accel_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_ICM20689_ACCL_MATRIX);

static struct attribute *inv_attributes[] = {
	&iio_dev_attr_in_gyro_matrix.dev_attr.attr,
	&iio_dev_attr_in_accel_matrix.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group inv_attribute_group = {
	.attrs = inv_attributes
};

static const struct iio_info mpu_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &inv_icm20689_read_raw,
	.write_raw = &inv_icm20689_write_raw,
	.attrs = &inv_attribute_group,
	.validate_trigger = inv_icm20689_validate_trigger,
};

static int inv_icm20689_detect_gyro(struct inv_icm20689_state *st)
{
	int retry = 0;
	uint8_t b = 0;

	while (retry < 5) {
		/* get version (expecting 0x98 for the 20689) */
		inv_icm20689_read_reg(st, st->reg->whoami, &b);
		if (b == 0x98)
			break;
		retry++;
	}
	if (b != 0x98)
		return -EIO;

	return 0;
}

/**
 *  inv_check_and_setup_chip() - check and setup chip.
 */
static int inv_check_and_setup_chip(struct inv_icm20689_state *st)
{
	int result;
	u8 usr_ctrl;

	st->chip_type = INV_ICM20689;
	st->hw  = &hw_info[st->chip_type];
	st->reg = hw_info[st->chip_type].reg;

	/* reset to make sure previous state are not there */
	result = inv_icm20689_write_reg(st, st->reg->pwr_mgmt_1,
					INV_ICM20689_BIT_H_RESET);
	if (result)
		return result;
	msleep(INV_ICM20689_POWER_UP_TIME);
	/* toggle power state. After reset, the sleep bit could be on
		or off depending on the OTP settings. Toggling power would
		make it in a definite state as well as making the hardware
		state align with the software state */
	result = inv_icm20689_set_power_itg(st, false);
	if (result)
		return result;
	result = inv_icm20689_set_power_itg(st, true);
	if (result)
		return result;

	result = inv_icm20689_write_reg(st, st->reg->pwr_mgmt_2, 0);
	if (result)
		return result;
	msleep(INV_ICM20689_SENSOR_UP_TIME);

	/* disable INT */
	result = inv_icm20689_write_reg(st, st->reg->int_enable, 0);
	if (result)
		return result;

	/* disable FIFO */
	result = inv_icm20689_write_reg(st, st->reg->fifo_en, 0);
	if (result)
		return result;

	/* disable I2C */
	usr_ctrl = 0;
	usr_ctrl |= INV_ICM20689_BIT_I2C_MST_DIS;
	result = inv_icm20689_write_reg(st, st->reg->user_ctrl, usr_ctrl);
	if (result)
		return result;

	usr_ctrl |= INV_ICM20689_BIT_FIFO_RST;
	result = inv_icm20689_write_reg(st, st->reg->user_ctrl, usr_ctrl);
	if (result)
		return result;

	if (inv_icm20689_detect_gyro(st) != 0)
		return -EIO;

	return 0;
}

/**
 *  inv_mpu_probe() - probe function.
 *
 *  Returns 0 on success, a negative error code otherwise.
 */
static int inv_icm20689_probe(struct spi_device *spi)
{
	struct inv_icm20689_state *st;
	struct iio_dev *indio_dev;
	struct inv_mpu6050_platform_data *pdata;
	int result;

	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		result =  -ENOMEM;
		goto out_no_free;
	}
	st = iio_priv(indio_dev);
	st->spi = spi;
	spi->bits_per_word = 8;

#if INV20689_DEVICE_IRQ_TRIGGER
	/* use spi device irq */
	st->gpio = of_get_named_gpio(spi->dev.of_node, "invn,icm20689-irq", 0);
	result = gpio_request(st->gpio, "icm20689-irq");
	if (result)
		goto out_no_free;
	spi->irq = gpio_to_irq(st->gpio);
#endif

	pdata = (struct inv_mpu6050_platform_data *)
				dev_get_platdata(&spi->dev);
	if (pdata)
		st->plat_data = *pdata;

	/* init buff */
	st->rx_buf = kmalloc(MPU_SPI_BUF_LEN, GFP_ATOMIC);
	if (!(st->rx_buf)) {
		result = -ENOMEM;
		goto out_free;
	}

	st->tx_buf = kmalloc(MPU_SPI_BUF_LEN, GFP_ATOMIC);
	if (!(st->tx_buf)) {
		result = -ENOMEM;
		goto out_free_spi1;
	}

	/* power is turned on inside check chip type*/
	result = inv_check_and_setup_chip(st);
	if (result)
		goto out_free_spi2;

	result = inv_icm20689_init_config(indio_dev);
	if (result) {
		dev_err(&spi->dev,
			"Could not initialize device.\n");
		goto out_free_spi2;
	}

	dev_set_drvdata(&spi->dev, indio_dev);
	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = ICM20689_DEV_NAME;
	indio_dev->channels = inv_mpu_channels;
	indio_dev->num_channels = ARRAY_SIZE(inv_mpu_channels);

	indio_dev->info = &mpu_info;
	indio_dev->modes = INDIO_BUFFER_TRIGGERED;

	result = iio_triggered_buffer_setup(indio_dev,
					    inv_icm20689_irq_handler,
					    inv_icm20689_read_fifo_fn,
					    NULL);
	if (result) {
		dev_err(&st->spi->dev, "configure buffer fail %d\n",
				result);
		goto out_free;
	}
	result = inv_icm20689_probe_trigger(indio_dev);
	if (result) {
		dev_err(&st->spi->dev, "trigger probe fail %d\n", result);
		goto out_unreg_ring;
	}

	INIT_KFIFO(st->timestamps);
	spin_lock_init(&st->time_stamp_lock);
	result = iio_device_register(indio_dev);
	if (result) {
		dev_err(&st->spi->dev, "IIO register fail %d\n", result);
		goto out_remove_trigger;
	}

	return 0;

out_remove_trigger:
	inv_icm20689_remove_trigger(st);
out_unreg_ring:
	iio_triggered_buffer_cleanup(indio_dev);
out_free_spi2:
	kfree(st->tx_buf);
out_free_spi1:
	kfree(st->rx_buf);
out_free:
	iio_device_free(indio_dev);
#if INV20689_DEVICE_IRQ_TRIGGER
	gpio_free(st->gpio);
#endif
out_no_free:

	return result;
}

static int inv_icm20689_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = dev_get_drvdata(&spi->dev);
	struct inv_icm20689_state *st = iio_priv(indio_dev);

	kfree(st->tx_buf);
	kfree(st->rx_buf);
#if INV20689_DEVICE_IRQ_TRIGGER
	gpio_free(st->gpio);
#endif
	iio_device_unregister(indio_dev);
	inv_icm20689_remove_trigger(st);
	iio_triggered_buffer_cleanup(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

#ifdef CONFIG_PM
static int inv_icm20689_suspend(struct spi_device *spi, pm_message_t state)
{
	struct iio_dev *indio_dev = dev_get_drvdata(&spi->dev);
	struct inv_icm20689_state *st = iio_priv(indio_dev);

	inv_icm20689_set_power_itg(st, false);
	return 0;
}

static int inv_icm20689_resume(struct spi_device *spi)
{
	struct iio_dev *indio_dev = dev_get_drvdata(&spi->dev);
	struct inv_icm20689_state *st = iio_priv(indio_dev);

	inv_icm20689_set_power_itg(st, true);

	return 0;
}
#else
#define icm20689_suspend NULL
#define icm20689_resume NULL
#endif

static struct of_device_id icm20689_match_table[] = {
	{
		.compatible = "invn,icm20689",
	},
	{}
};

static struct spi_driver icm20689_driver = {
	.driver = {
		.name = "invn-icm20689",
		.of_match_table = icm20689_match_table,
		.owner = THIS_MODULE,
	},
	.probe = inv_icm20689_probe,
	.remove = inv_icm20689_remove,
#ifdef CONFIG_PM
	.suspend = inv_icm20689_suspend,
	.resume = inv_icm20689_resume,
#endif
};

static int __init icm20689_init(void)
{
	return spi_register_driver(&icm20689_driver);
}

static void __exit icm20689_exit(void)
{
	spi_unregister_driver(&icm20689_driver);
}

module_init(icm20689_init);
module_exit(icm20689_exit);
MODULE_DESCRIPTION("icm20689 IMU driver");
MODULE_LICENSE("GPL");

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
#include <linux/debugfs.h>

#include "inv_mpu9250_iio.h"

/**
*0 disable debug log
*1 enable debug log
*/
int mpu9250_debug_enable = 0;
static int inv_mpu9250_init_device(struct inv_mpu9250_state *st);
static int inv_mpu9250_detect(struct inv_mpu9250_state *st);
static int mpu9250_read_context(struct inv_mpu9250_state *st, char *buf);
static int mpu9250_bulk_read_raw(struct inv_mpu9250_state *st,
							char *buf,
							int size);

static const struct inv_mpu9250_reg_map reg_set_map = {
	.sample_rate_div        = MPU9250_REG_SMPLRT_DIV,
	.config                 = MPU9250_REG_CONFIG,
	.user_ctrl              = MPU9250_REG_USER_CTRL,
	.fifo_en                = MPU9250_REG_FIFO_ENABLE,
	.gyro_config            = MPU9250_REG_GYRO_CONFIG,
	.accl_config            = MPU9250_REG_ACCEL_CONFIG,
	.accl_config2           = MPU9250_REG_ACCEL_CONFIG2,
	.fifo_count_h           = MPU9250_REG_FIFO_COUNTH,
	.fifo_count_l           = MPU9250_REG_FIFO_COUNTL,
	.i2c_mst_ctrl           = MPU9250_REG_I2C_MST_CTRL,
	.i2c_slv0_addr          = MPU9250_REG_I2C_SLV0_ADDR,
	.i2c_slv0_reg           = MPU9250_REG_I2C_SLV0_REG,
	.i2c_slv0_ctrl          = MPU9250_REG_I2C_SLV0_CTRL,
	.i2c_slv1_addr          = MPU9250_REG_I2C_SLV1_ADDR,
	.i2c_slv1_reg           = MPU9250_REG_I2C_SLV1_REG,
	.i2c_slv1_ctrl          = MPU9250_REG_I2C_SLV1_CTRL,
	.i2c_slv1_do            = MPU9250_REG_I2C_SLV1_DO,
	.i2c_slv4_addr          = MPU9250_REG_I2C_SLV4_ADDR,
	.i2c_slv4_reg           = MPU9250_REG_I2C_SLV4_REG,
	.i2c_slv4_ctrl          = MPU9250_REG_I2C_SLV4_CTRL,
	.i2c_slv4_di            = MPU9250_REG_I2C_SLV4_DI,
	.i2c_slv4_do            = MPU9250_REG_I2C_SLV4_DO,
	.i2c_mst_status         = MPU9250_REG_I2C_MST_STATUS,
	.i2c_mst_delay_ctrl     = MPU9250_REG_I2C_MST_DELAY_CTRL,
	.fifo_r_w               = MPU9250_REG_FIFO_RW,
	.int_enable             = MPU9250_REG_INT_EN,
	.pwr_mgmt_1             = MPU9250_REG_PWR_MGMT1,
	.pwr_mgmt_2             = MPU9250_REG_PWR_MGMT2,
	.whoami                 = MPU9250_REG_WHOAMI,
};

static struct mpu9250_chip_config chip_config[] = {
	/** this is for the init config */
	[0] = {
		.gyro_lpf = MPU9250_GYRO_LPF_20HZ,
		.acc_lpf  = MPU9250_ACC_LPF_20HZ,
		.gyro_fsr = MPU9250_GYRO_FSR_500DPS,
		.acc_fsr  = MPU9250_ACC_FSR_4G,
		.gyro_sample_rate = MPU9250_SAMPLE_RATE_200HZ,
		.compass_enabled = false,
		.compass_sample_rate = MPU9250_COMPASS_SAMPLE_RATE_100HZ,
		.fifo_enabled = true,
		.fifo_en_mask = BIT_TEMP_FIFO_EN
						|BIT_GYRO_FIFO_EN
						|BIT_ACCEL_FIFO_EN,
	},
	/** this is for measure  rate */
	[1] = {
		.gyro_lpf = MPU9250_GYRO_LPF_20HZ,
		.acc_lpf  = MPU9250_ACC_LPF_20HZ,
		.gyro_fsr = MPU9250_GYRO_FSR_500DPS,
		.acc_fsr  = MPU9250_ACC_FSR_4G,
		.gyro_sample_rate = MPU9250_SAMPLE_RATE_1000HZ,
		.compass_enabled = false,
		.compass_sample_rate = MPU9250_COMPASS_SAMPLE_RATE_100HZ,
		.fifo_enabled = true,
		.fifo_en_mask = BIT_TEMP_FIFO_EN
						|BIT_GYRO_FIFO_EN
						|BIT_ACCEL_FIFO_EN,
	},
	/** this is for fifo poll data main */
	[2] = {
		.gyro_lpf = MPU9250_GYRO_LPF_250HZ,
		.acc_lpf  = MPU9250_ACC_LPF_20HZ,
		.gyro_fsr = MPU9250_GYRO_FSR_500DPS,
		.acc_fsr  = MPU9250_ACC_FSR_4G,
		.gyro_sample_rate = MPU9250_SAMPLE_RATE_8000HZ,
		.compass_enabled = false,
		.compass_sample_rate = MPU9250_COMPASS_SAMPLE_RATE_100HZ,
		.fifo_enabled = true,
		.fifo_en_mask = BIT_TEMP_FIFO_EN
						|BIT_GYRO_FIFO_EN
						|BIT_ACCEL_FIFO_EN,
	},
};

static struct inv_mpu9250_hw hw_info[INV_NUM_PARTS] = {
	{
		.num_reg = 117,
		.name = "mpu9250",
		.reg = &reg_set_map,
	},
};

static int gyro_sample_rate_enum_to_hz(enum gyro_sample_rate_e rate)
{
	switch (rate) {
	case(MPU9250_SAMPLE_RATE_100HZ):
		return 100;
	case(MPU9250_SAMPLE_RATE_200HZ):
		return 200;
	case(MPU9250_SAMPLE_RATE_500HZ):
		return 500;
	case(MPU9250_SAMPLE_RATE_1000HZ):
		return 1000;
	case(MPU9250_SAMPLE_RATE_8000HZ):
		return 8000;
	default:
		return -MPU_FAIL;
	}
}

static int compass_sample_rate_enum_to_hz(enum compass_sample_rate_e rate)
{
	switch (rate) {
	case(MPU9250_COMPASS_SAMPLE_RATE_100HZ):
		return 100;
	default:
		return -MPU_FAIL;
	}
}

static  int gyro_fsr_enum_to_dps(enum gyro_fsr_e fsr)
{
	switch (fsr) {
	case (MPU9250_GYRO_FSR_250DPS):
		return 250;
	case (MPU9250_GYRO_FSR_500DPS):
		return 500;
	case (MPU9250_GYRO_FSR_1000DPS):
		return 1000;
	case (MPU9250_GYRO_FSR_2000DPS):
		return 2000;
	default:
		return -MPU_FAIL;
	}
}

static int gyro_lpf_enum_to_hz(enum gyro_lpf_e lpf)
{
	switch (lpf) {
	case MPU9250_GYRO_LPF_250HZ:
		return 250;
	case MPU9250_GYRO_LPF_184HZ:
		return 184;
	case MPU9250_GYRO_LPF_92HZ:
		return 92;
	case MPU9250_GYRO_LPF_41HZ:
		return 41;
	case MPU9250_GYRO_LPF_20HZ:
		return 20;
	case MPU9250_GYRO_LPF_10HZ:
		return 10;
	case MPU9250_GYRO_LPF_5HZ:
		return 5;
	default:
		return 0;
	}
}

static int accel_lpf_enum_to_hz(enum acc_lpf_e lpf)
{
	switch (lpf) {
	case (MPU9250_ACC_LPF_460HZ):
		return 460;
	case (MPU9250_ACC_LPF_184HZ):
		return 184;
	case (MPU9250_ACC_LPF_92HZ):
		return 92;
	case (MPU9250_ACC_LPF_41HZ):
		return 41;
	case (MPU9250_ACC_LPF_20HZ):
		return 20;
	case (MPU9250_ACC_LPF_10HZ):
		return 10;
	case (MPU9250_ACC_LPF_5HZ):
		return 5;
	default:
		return 0;
	}
}

int inv_mpu9250_write_reg(struct inv_mpu9250_state *st, int reg, u8 value)
{
	int result = MPU_SUCCESS;
	char txbuf[2] = {0x0, 0x0};

	txbuf[0] = reg;
	txbuf[1] = value;
	result = spi_write_then_read(st->spi, &txbuf[0], 2, NULL, 0);
	if (result) {
		dev_dbgerr("mpu write reg %u failed, rc %d\n", reg, value);
		result = -MPU_READ_FAIL;
	}
	return result;
}

int inv_mpu9250_read_reg(struct inv_mpu9250_state *st,
					uint8_t reg,
					uint8_t *val)
{
	int result = MPU_SUCCESS;
	char txbuf[2] = {0x0, 0x0};
	char rxbuf[2] = {0x0, 0x0};

	/** register high bit=1 for read */
	txbuf[0] = reg | 0x80;
	result = spi_write_then_read(st->spi, &txbuf[0], 1, &rxbuf[0], 1);
	if (result) {
		dev_dbgerr("mpu read reg %u failed, rc %d\n", reg, result);
		result = -MPU_READ_FAIL;
	}

	*val = rxbuf[0];
	return result;
}

int inv_mpu9250_set_verify_reg(struct inv_mpu9250_state *st,
							uint8_t reg,
							uint8_t value)
{
	int result = MPU_SUCCESS;
	uint8_t tmp_value = 0;
	char retry = 5;

	/** set and verify reg */
	while (retry) {
		retry--;
		result |= inv_mpu9250_write_reg(st, reg, value);
		result |= inv_mpu9250_read_reg(st, reg, &tmp_value);
		if (value == tmp_value)
			break;
	}

	if (value != tmp_value) {
		dev_dbgerr("mpu write reg %u val 0x%x  failed, rc %d\n",
					reg, value, result);
	}

	return result;
}

int inv_mpu9250_set_power_itg(struct inv_mpu9250_state *st, bool power_on)
{
	int result = MPU_SUCCESS;

	if (power_on)
		result = inv_mpu9250_write_reg(st, st->reg->pwr_mgmt_1, 0);
	else
		result = inv_mpu9250_write_reg(st,
					st->reg->pwr_mgmt_1, BIT_SLEEP);
	if (result) {
		dev_dbgerr("set power failed power %d  err %d\n",
					power_on, result);
		return result;
	}

	if (power_on)
		msleep(INV_MPU9250_SENSOR_UP_TIME);

	return result;
}

int inv_mpu9250_get_interrupt_status(struct inv_mpu9250_state *st)
{
	int result = MPU_SUCCESS;
	uint8_t value = 0;

	if (!st)
		return -MPU_READ_FAIL;

	result = inv_mpu9250_read_reg(st, MPU9250_REG_INT_STATUS, &value);
	if (result) {
		dev_dbgerr("get interrupt status %d  failed\n", value);
		return -MPU_READ_FAIL;
	}

	if (value & 0x10)
		dev_dbgerr("interrupt status: fifo overflow interrupt\n");

	return result;
}

static int inv_mpu9250_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val,
			      int *val2,
			      long mask)
{
	struct inv_mpu9250_state  *st = iio_priv(indio_dev);

	dev_dbginfo("read raw : st %p chan->type %d mask %ld\n",
		st, chan->type, mask);
	return 0;
}

static int inv_mpu9250_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask)
{
	struct inv_mpu9250_state  *st = iio_priv(indio_dev);

	dev_dbginfo("write raw : st %p chan->type %d mask %ld\n",
		st, chan->type, mask);
	return 0;
}

static ssize_t inv_select_config_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int enum_config = 0;
	int result = MPU_SUCCESS;
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu9250_state *st = iio_priv(indio_dev);
	struct reg_cfg *reg_cfg_info = NULL;

	if (!st)
		return -EINVAL;

	if (kstrtoint(buf, 10, &enum_config))
		return -EINVAL;

	reg_cfg_info = &st->reg_cfg_info;

	if (enum_config == reg_cfg_info->init_config)
		return count;

	st->chip_type = INV_MPU9250;
	st->hw	= &hw_info[st->chip_type];
	st->reg = hw_info[st->chip_type].reg;

	switch (enum_config) {
	case INIT_200HZ:
		st->config = &chip_config[INIT_200HZ];
		reg_cfg_info->init_config = INIT_200HZ;
		break;
	case INIT_1000HZ:
		st->config = &chip_config[INIT_1000HZ];
		reg_cfg_info->init_config = INIT_1000HZ;
		break;
	case INIT_8000HZ:
		st->config = &chip_config[INIT_8000HZ];
		reg_cfg_info->init_config = INIT_8000HZ;
		break;
	default:
		st->config = &chip_config[INIT_200HZ];
		reg_cfg_info->init_config = INIT_200HZ;
		break;
	}

	result |= inv_mpu9250_detect(st);
	result |= inv_mpu9250_init_device(st);
	if (result)
		dev_dbgerr("inv_select_config_store failed\n");

	return count;
}

static ssize_t inv_select_config_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu9250_state *st = iio_priv(dev_to_iio_dev(dev));
	struct reg_cfg *reg_cfg_info = NULL;

	reg_cfg_info = &st->reg_cfg_info;
	return snprintf(buf, 8, "%d\n", reg_cfg_info->init_config);
}

static ssize_t inv_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu9250_state *st = iio_priv(dev_to_iio_dev(dev));
	int result = MPU_SUCCESS;

	if (st == NULL || buf == NULL)
		return 0;

	result = mpu9250_bulk_read_raw(st, buf, st->fifo_packet_size);
	if (result)
		dev_dbgerr("mpu raw data show failed\n");
	return result;
}

static ssize_t fifo_cnt_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu9250_state *st = iio_priv(dev_to_iio_dev(dev));

	return snprintf(buf, PAGE_SIZE, "%d\n", st->fifo_cnt_threshold);
}

static ssize_t fifo_cnt_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int enum_config = 0;
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_mpu9250_state *st = iio_priv(indio_dev);
	struct reg_cfg *reg_cfg_info = NULL;

	if (!st)
		return -EINVAL;

	if (kstrtoint(buf, 10, &enum_config))
		return -EINVAL;

	reg_cfg_info = &st->reg_cfg_info;

	if ((enum_config > 140)
		&& (reg_cfg_info->init_config <= INIT_1000HZ))
		enum_config = st->fifo_cnt_threshold;

	st->fifo_cnt_threshold = enum_config;
	return count;
}

static ssize_t inv_inv_read_context_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu9250_state *st = iio_priv(dev_to_iio_dev(dev));
	int result = MPU_SUCCESS;

	if (st == NULL || buf == NULL)
		return 0;

	result = mpu9250_read_context(st, buf);
	if (result)
		dev_dbgerr("mpu read context failed\n");
	return result;
}

static int inv_mpu9250_validate_trigger(struct iio_dev *indio_dev,
					struct iio_trigger *trig)
{
	struct inv_mpu9250_state *st = iio_priv(indio_dev);

	if (st->trig != trig)
		return -EINVAL;

	return MPU_SUCCESS;
}

#define INV_MPU9250_CHAN(_type, _channel2, _index)                    \
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

static const struct iio_chan_spec inv_mpu9250_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(INV_MPU9250_SCAN_TIMESTAMP),
	{
		.type = IIO_TEMP,
		.info_mask_separate =  BIT(IIO_CHAN_INFO_RAW)
				| BIT(IIO_CHAN_INFO_OFFSET)
				| BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = INV_MPU9250_SCAN_TEMP,
		.channel2 = IIO_MOD_X,
		.scan_type = {
				.sign = 's',
				.realbits = 16,
				.storagebits = 16,
				.shift = 0,
				.endianness = IIO_BE,
			     },
	},
	INV_MPU9250_CHAN(IIO_ANGL_VEL, IIO_MOD_X, INV_MPU9250_SCAN_GYRO_X),
	INV_MPU9250_CHAN(IIO_ANGL_VEL, IIO_MOD_Y, INV_MPU9250_SCAN_GYRO_Y),
	INV_MPU9250_CHAN(IIO_ANGL_VEL, IIO_MOD_Z, INV_MPU9250_SCAN_GYRO_Z),

	INV_MPU9250_CHAN(IIO_ACCEL, IIO_MOD_X, INV_MPU9250_SCAN_ACCL_X),
	INV_MPU9250_CHAN(IIO_ACCEL, IIO_MOD_Y, INV_MPU9250_SCAN_ACCL_Y),
	INV_MPU9250_CHAN(IIO_ACCEL, IIO_MOD_Z, INV_MPU9250_SCAN_ACCL_Z),
};

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("200 1000 8000");
static IIO_DEVICE_ATTR(inv_raw_data, S_IRUGO, inv_raw_data_show, NULL, 0);

static IIO_DEVICE_ATTR(
						inv_fifo_cnt_threshold,
						S_IRUGO | S_IWUSR,
						fifo_cnt_threshold_show,
						fifo_cnt_threshold_store,
						0);

static IIO_DEVICE_ATTR(
						inv_read_context,
						S_IRUGO,
						inv_inv_read_context_show,
						NULL,
						0);
static IIO_DEV_ATTR_SAMP_FREQ(S_IRUGO | S_IWUSR, inv_select_config_show,
	inv_select_config_store);

static struct attribute *inv_attributes[] = {
	&iio_dev_attr_inv_read_context.dev_attr.attr,
	&iio_dev_attr_inv_raw_data.dev_attr.attr,
	&iio_dev_attr_inv_fifo_cnt_threshold.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group inv_attribute_group = {
	.attrs = inv_attributes
};

static const struct iio_info mpu9250_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &inv_mpu9250_read_raw,
	.write_raw = &inv_mpu9250_write_raw,
	.attrs = &inv_attribute_group,
	.validate_trigger = inv_mpu9250_validate_trigger,
};

static int inv_mpu9250_detect_gyro(struct inv_mpu9250_state *st)
{
	int result = MPU_SUCCESS;
	int retry = 0;
	uint8_t val = 0;

	/** get who am i register*/
	while (retry < 5) {
		/** get version (expecting 0x71 for the mpu9250) */
		inv_mpu9250_read_reg(st, st->reg->whoami, &val);
		if (val == WHO_AM_I_REG_VAL)
			break;
		retry++;
	}

	if (val != WHO_AM_I_REG_VAL) {
		dev_dbgerr("detect mpu fail,whoami 0x%x\n", val);
		result = -MPU_FAIL;
	} else
		dev_dbginfo("detect mpu ok,whoami 0x%x\n", val);

	return result;
}

int compass_write_register(struct inv_mpu9250_state *st,
							uint8_t reg,
							uint8_t val)
{
	int result = MPU_SUCCESS;
	int timeout = 10;
	uint8_t value = 0;

	/**
	*I2C_SLV4_ADDR
	*write operation on compass address 0x0C
	*/
	result |= inv_mpu9250_set_verify_reg(st, st->reg->i2c_slv4_addr, 0x0c);
	/**
	*I2C_SLV4_REG
	*set the compass register address to write to
	**/
	result |= inv_mpu9250_set_verify_reg(st, st->reg->i2c_slv4_reg, reg);

	/**
	*I2C_SLV4_DO
	*set the value to write in I2C_SLV4_DO register
	*/
	result |= inv_mpu9250_set_verify_reg(st, st->reg->i2c_slv4_do, val);

	result |= inv_mpu9250_read_reg(st, st->reg->i2c_slv4_ctrl, &value);
	value |= 0x80;
	result = inv_mpu9250_write_reg(st, st->reg->i2c_slv4_ctrl, value);
	if (result) {
		dev_dbgerr("compa write 0x%x val 0x%x fail\n", reg, val);
		return result;
	}

	value = 0;
	while (timeout && ((value & 0x40) == 0x00)) {
		timeout--;
		result |= inv_mpu9250_read_reg(st,
				st->reg->i2c_mst_status, &value);
		if ((value & 0x40) == 0x40)
			break;
		msleep(100);
	}

	if ((value & 0x40) == 0x00)
		dev_dbgerr("compa write 0x%x val 0x%x fail\n", reg, val);
	else
		dev_dbginfo("compa write 0x%x val 0x%x ok\n", reg, val);

	return result;
}

int compass_read_register(struct inv_mpu9250_state *st,
						uint8_t reg,
						uint8_t *val)
{
	int result = MPU_SUCCESS;
	int timeout = 10;
	uint8_t value = 0;

	/** set and verify read addr */
	result |= inv_mpu9250_set_verify_reg(st, st->reg->i2c_slv4_addr, 0x8c);

	result |= inv_mpu9250_set_verify_reg(st, st->reg->i2c_slv4_reg, reg);

	/**
	*set I2C_SLV4_EN bit in I2C_SL4_CTRL register without overwriting other
	*bits, which specifies the sample rate
	*/
	result |= inv_mpu9250_read_reg(st, st->reg->i2c_slv4_ctrl, &value);
	value |= 0x80;
	result = inv_mpu9250_write_reg(st, st->reg->i2c_slv4_ctrl, value);

	if (result) {
		dev_dbgerr("mpu9250 compass read reg 0x%x failed\n", reg);
		return result;
	}

	/** wait for I2C completion*/
	value = 0;
	while (timeout && ((value & 0x40) == 0x00)) {
		timeout--;
		result |= inv_mpu9250_read_reg(st,
			st->reg->i2c_mst_status, &value);
		if ((value & 0x40) == 0x40)
			break;
		msleep(100);
	}

	if ((value & 0x40) == 0x00) {
		dev_dbgerr("mpu9250 compass read reg 0x%x time out\n", reg);
		return result;
	}

	result |= inv_mpu9250_read_reg(st, st->reg->i2c_slv4_di, val);
	if (result) {
		dev_dbgerr("mpu9250 compass read reg 0x%x failed\n", reg);
		return result;
	}

	dev_dbginfo("compass read reg 0x%x success val 0x%x\n", reg, *val);
	return result;
}

static int mpu9250_compass_senitivity_adjustment(struct inv_mpu9250_state *st)
{
	int result = MPU_SUCCESS;
	uint8_t asa[3] = {0x0, 0x0, 0x0};
	int i = 0;

	if (compass_write_register(st, MPU9250_COMP_REG_CNTL1, 0x00)
		!= MPU_SUCCESS) {
		return -MPU_FAIL;
	}
	msleep(20);
	/**
	*enable FUSE ROM, since the sensitivity adjustment data is stored in
	*compass registers 0x10, 0x11 and 0x12 which is only accessible in Fuse
	*access mode.
	*/
	if (compass_write_register(st, MPU9250_COMP_REG_CNTL1, 0x1f)
		!= MPU_SUCCESS) {
		result = -MPU_FAIL;
		return result;
	}
	msleep(20);

	/**
	*get compass calibration register 0x10, 0x11, 0x12
	*store into context
	*/
	for (i = 0; i < 3; i++) {
		if (compass_read_register(st, MPU9250_COMP_REG_ASAX + i,
						&asa[i]) != 0) {
			dev_dbgerr("compass senitivity adjustment failed\n");
			result = -MPU_FAIL;
			return result;
		}
	}

	/**Set power-down mode*/
	if (compass_write_register(st, MPU9250_COMP_REG_CNTL1, 0x00) != 0) {
		dev_dbgerr("compass senitivity power down failed\n");
		return -MPU_FAIL;
	}
	 msleep(20);

	return result;
}

static int mpu9250_detect_compass(struct inv_mpu9250_state *st)
{
	int result = MPU_SUCCESS;
	uint8_t value = 0;

	result = compass_read_register(st, MPU9250_COMP_REG_WIA, &value);
	if (result || (value != MPU9250_AKM_DEV_ID))
		dev_dbgerr("detect compass failed,whoami reg 0x%x\n", value);
	else
		dev_dbginfo("detect compass ok,whoami reg 0x%x\n", value);

	return result;
}

static int inv_mpu9250_detect(struct inv_mpu9250_state *st)
{
	int result = MPU_SUCCESS;
	uint8_t retry = 0, val = 0;
	uint8_t usr_ctrl = 0;

	/** reset to make sure previous state are not there */
	result = inv_mpu9250_write_reg(st, st->reg->pwr_mgmt_1, BIT_H_RESET);
	if (result) {
		dev_dbgerr("mpu write reg 0x%x value 0x%x failed\n",
		st->reg->pwr_mgmt_1,
		BIT_H_RESET);
		return result;
	}

	/** the power up delay*/
	msleep(INV_MPU9250_POWER_UP_TIME);

	/** disable I2C interface*/
	usr_ctrl = 0;
	usr_ctrl |= BIT_I2C_IF_DIS;
	result = inv_mpu9250_write_reg(st, st->reg->user_ctrl, usr_ctrl);
	if (result) {
		dev_dbgerr("mpu write reg 0x%x value 0x%x failed\n",
			st->reg->user_ctrl, usr_ctrl);
		return result;
	}

	/** out of sleep*/
	result = inv_mpu9250_set_power_itg(st, true);
	if (result)
		return result;

	/** get who am i register*/
	while (retry < 10) {
		/** get version (expecting 0x71 for the mpu9250) */
		inv_mpu9250_read_reg(st, st->reg->whoami, &val);
		if (val == WHO_AM_I_REG_VAL)
			break;
		retry++;
	}

	if (val != WHO_AM_I_REG_VAL) {
		dev_dbgerr("detect mpu failed,whoami reg 0x%x\n", val);
		result = -MPU_FAIL;
	} else {
		dev_dbginfo("detect mpu ok,whoami reg 0x%x\n", val);
	}

	return result;
}

static int mpu9250_get_gyro_lpf(struct inv_mpu9250_state *st, int *value)
{
	int result = MPU_SUCCESS;
	uint8_t config, gyro_config;
	uint8_t fchoice, dlpf_config;

	/** read gyro config register*/
	if (inv_mpu9250_read_reg(st, st->reg->gyro_config, &gyro_config))
		return -MPU_FAIL;

	fchoice = (gyro_config & 0x03) ^ 0x03;

	/** DLPF_CFG is only effective if FCHOICE is set with 0x03*/
	if (fchoice != 0x03) {
		*value = 0;
		return -MPU_FAIL;
	}

	/** read config register*/
	result = inv_mpu9250_read_reg(st, st->reg->config, &config);
	dlpf_config = config & 0x07;

	*value = gyro_lpf_enum_to_hz(dlpf_config);

	dev_dbginfo("mpu9250_get_gyro_lpf : value 0x%x\n", *value);
	return result;
}

static int mpu9250_get_accel_lpf(struct inv_mpu9250_state *st, int *value)
{
	int result = MPU_SUCCESS;
	uint8_t accel_config_2;
	uint8_t fchoice, a_dlpfcfg;

	if (inv_mpu9250_read_reg(st, st->reg->accl_config2, &accel_config_2))
		return -MPU_FAIL;

	fchoice = ((accel_config_2 & 0x08) >> 3) ^ 0x01;
	a_dlpfcfg = accel_config_2 & 0x07;

	/** To use DLPF_CFG, FCHOICE must be set with 0x03*/
	if (fchoice == 0) {
		*value = 0;
		return result;
	}

	*value = accel_lpf_enum_to_hz(a_dlpfcfg);

	dev_dbginfo("get accel lpf : value 0x%x\n", *value);
	return result;
}

static int mpu9250_get_gyro_sample_rate(struct inv_mpu9250_state *st,
		int *value)
{
	int result = MPU_SUCCESS;
	uint8_t smplrt_div;
	int gyro_lpf;

	result = mpu9250_get_gyro_lpf(st, &gyro_lpf);
	if (gyro_lpf == 250) {
		*value = 8000;
		dev_dbginfo("get gyro sample rate: rate %d\n", *value);
		return result;
	}

	result = inv_mpu9250_read_reg(st, st->reg->sample_rate_div,
				&smplrt_div);
	*value = 1000 / (1 + smplrt_div);

	dev_dbginfo("get gyro sample rate: rate %d\n", *value);
	return result;
}

static int mpu9250_get_accel_sample_rate(struct inv_mpu9250_state *st,
		int *value)
{
	int result = MPU_SUCCESS;
	uint8_t smplrt_div = 0;
	uint8_t fchoice = 0;
	uint8_t accel_config_2 = 0;

	if (inv_mpu9250_read_reg(st, st->reg->accl_config2, &accel_config_2))
		return -MPU_FAIL;

	fchoice = ((accel_config_2 & 0x08) >> 3) ^ 0x01;
	if (fchoice == 0) {
		*value = 4000;
		dev_dbginfo("get accel sample rate: sample rate %d\n", *value);
		return 0;
	}

	result = inv_mpu9250_read_reg(st, st->reg->sample_rate_div,
				&smplrt_div);
	*value = 1000 / (1 + smplrt_div);

	dev_dbginfo("get accel sample rate: rate %d\n", *value);
	return result;
}

static int mpu9250_get_compass_sample_rate(struct inv_mpu9250_state *st,
			int *value)
{
	int result = MPU_SUCCESS;
	int sample_rate;
	uint8_t i2c_slv4_ctrl;
	uint8_t i2c_mst_dly;

	if (mpu9250_get_gyro_sample_rate(st, &sample_rate))
		return  -MPU_FAIL;

	if (inv_mpu9250_read_reg(st, st->reg->i2c_slv4_ctrl, &i2c_slv4_ctrl))
		return  -MPU_FAIL;

	i2c_mst_dly = i2c_slv4_ctrl & 0x1f;
	*value = sample_rate / (1 + i2c_mst_dly);

	dev_dbginfo("get compass sample rate: rate %d\n", *value);
	return result;
}

static int mpu9250_get_gyro_fsr(struct inv_mpu9250_state *st, int *value)
{
	int result = MPU_SUCCESS;
	uint8_t gyro_config;
	uint8_t gyro_fs_sel;

	if (inv_mpu9250_read_reg(st, st->reg->gyro_config, &gyro_config))
		return -MPU_FAIL;

	gyro_fs_sel = (gyro_config >> 3) & 0x03;
	*value = gyro_fsr_enum_to_dps(gyro_fs_sel);

	dev_dbginfo("mpu9250_get_gyro_fsr : fsr %d\n", *value);
	return result;
}

static int mpu9250_get_accel_fsr(struct inv_mpu9250_state *st, int *value)
{
	int result = MPU_SUCCESS;
	uint8_t accel_config;
	uint8_t accel_fs_sel;

	if (inv_mpu9250_read_reg(st, st->reg->accl_config, &accel_config))
		return -MPU_FAIL;

	accel_fs_sel = (accel_config >> 3) & 0x03;
	*value = 1 << (accel_fs_sel + 1);

	dev_dbginfo("mpu9250_get_accel_fsr : fsr %d\n", *value);
	return result;
}

static int mpu9250_get_compass_fsr(struct inv_mpu9250_state *st, int *value)
{
	int result = MPU_SUCCESS;

	*value = MPU9250_AK89xx_FSR;
	return result;
}

int mpu9250_bulk_read(struct inv_mpu9250_state *st,
			int reg, char *buf, int size)
{
	int result = MPU_SUCCESS;
	char tx_buf[2] = {0x0, 0x0};

	if (!st || !buf)
		return -MPU_FAIL;

	tx_buf[0] = reg | 0x80;
	result = spi_write_then_read(st->spi, &tx_buf[0], 1, buf, size);
	if (result) {
		dev_dbgerr("fifo_r_w 0x%x size 0x%x failed\n",
			st->reg->fifo_r_w, size);
		result = -MPU_READ_FAIL;
	}

	return result;
}

static int mpu9250_bulk_read_raw(struct inv_mpu9250_state *st,
								char *buf,
								int size)
{
	int result = MPU_SUCCESS;

	if (!st || !buf)
		return -MPU_FAIL;

	result = mpu9250_bulk_read(st, 0x3b, buf, size);
	if (result) {
		dev_dbgerr("mpu9250 read raw failed\n");
		return -MPU_FAIL;
	}

	return result;
}

static int mpu9250_read_fifo(struct inv_mpu9250_state *st,
							char *buf,
							int size)
{
	int result = MPU_SUCCESS;
	int read_count = 0, max_count = 0;
	uint8_t value_h = 0, value_l = 0;
	int fifo_count = 0;
	int packet_read = 0, tmp_size = 0;

	if (!st || !buf)
		return -MPU_FAIL;

	/** read fifo count*/
	result |= inv_mpu9250_read_reg(st, st->reg->fifo_count_h, &value_h);
	result |= inv_mpu9250_read_reg(st, st->reg->fifo_count_l, &value_l);
	if (result) {
		dev_dbgerr("mpu9250 read fifo failed\n");
		return -MPU_FAIL;
	}

	fifo_count = (value_h << 8) | value_l;
	read_count = fifo_count / st->fifo_packet_size;
	max_count = MPU9250_FIFO_SINGLE_READ_MAX_BYTES / st->fifo_packet_size;

	while (read_count) {
		packet_read = (read_count < max_count ? read_count : max_count);
		tmp_size = packet_read * st->fifo_packet_size;
		/** single read max byte is 256*/
		result = mpu9250_bulk_read(st, st->reg->fifo_r_w, buf, tmp_size);
		if (result) {
			dev_dbgerr("mpu9250 read fifo failed\n");
			break;
		}
		buf += tmp_size;
		read_count = read_count - packet_read;
	}

	return result;
}

int mpu9250_start_fifo(struct inv_mpu9250_state *st)
{
	struct mpu9250_chip_config *config = NULL;
	struct reg_cfg *reg_cfg_info = NULL;

	config = st->config;
	reg_cfg_info = &st->reg_cfg_info;

	/** enable fifo */
	if (config->fifo_enabled) {
		reg_cfg_info->user_ctrl |= BIT_FIFO_EN;
		if (inv_mpu9250_write_reg(st, st->reg->user_ctrl,
						reg_cfg_info->user_ctrl)) {
			dev_dbgerr("mpu9250 start fifo failed\n");
			return -MPU_FAIL;
		}
		/** enable interrupt*/
		reg_cfg_info->int_en |= BIT_RAW_RDY_EN;
		if (inv_mpu9250_write_reg(st, st->reg->int_enable,
						reg_cfg_info->int_en)) {
			dev_dbgerr("mpu9250 set raw rdy failed\n");
			return -MPU_FAIL;
		}
	}

	return MPU_SUCCESS;
}

static int mpu9250_stop_fifo(struct inv_mpu9250_state *st)
{
	struct mpu9250_chip_config *config = NULL;
	struct reg_cfg *reg_cfg_info = NULL;

	config = st->config;
	reg_cfg_info = &st->reg_cfg_info;

	/** disable fifo */
	if (config->fifo_enabled) {
		reg_cfg_info->user_ctrl &= ~BIT_FIFO_EN;
		if (inv_mpu9250_write_reg(st, st->reg->user_ctrl,
				reg_cfg_info->user_ctrl | BIT_FIFO_RST)) {
			dev_dbgerr("mpu9250 stop fifo failed\n");
			return -MPU_FAIL;
		}
	}

	return MPU_SUCCESS;
}

static int mpu9250_config_fifo(struct inv_mpu9250_state *st)
{
	struct mpu9250_chip_config *config = NULL;
	struct reg_cfg *reg_cfg_info = NULL;

	config = st->config;
	reg_cfg_info = &st->reg_cfg_info;

	if (config->fifo_enabled != true)
		return MPU_SUCCESS;
	/**
	*Set CONFIG.FIFO_MODE = 1, i.e. when FIFO is full, additional writes will
	*not be written to FIFO
	*/
	reg_cfg_info->config |= 0x40;
	if (inv_mpu9250_write_reg(st, st->reg->config, reg_cfg_info->config))
		return -MPU_FAIL;

	/** reset fifo*/
	if (inv_mpu9250_write_reg(st, st->reg->user_ctrl,
				reg_cfg_info->user_ctrl | BIT_FIFO_RST)) {
		return -MPU_FAIL;
	}

	/** Enable FIFO on specified sensors*/
	reg_cfg_info->fifo_enable |= config->fifo_en_mask;
	if (inv_mpu9250_write_reg(st, st->reg->fifo_en,
				reg_cfg_info->fifo_enable)) {
		return -MPU_FAIL;
	}

	st->fifo_packet_size = 0;
	if (config->fifo_en_mask & BIT_TEMP_FIFO_EN)
		st->fifo_packet_size += 2;
	if (config->fifo_en_mask & BIT_GYRO_FIFO_EN)
		st->fifo_packet_size += 6;
	if (config->fifo_en_mask & BIT_ACCEL_FIFO_EN)
		st->fifo_packet_size += 6;


	if (mpu9250_start_fifo(st))
		return -MPU_FAIL;

	dev_dbginfo("mpu config fifo finished\n");
	return MPU_SUCCESS;
}

static int mpu9250_read_context(struct inv_mpu9250_state *st, char *buf)
{
	struct mpu9250_chip_config *config = NULL;
	int result = MPU_SUCCESS;
	struct mpu9250_context context;

	if (st == NULL)
		return -MPU_FAIL;

	config = st->config;
	result |= mpu9250_get_gyro_lpf(st, &context.gyro_lpf);
	result |= mpu9250_get_accel_lpf(st, &context.accel_lpf);
	result |= mpu9250_get_gyro_sample_rate(st, &context.gyro_sample_rate);
	result |= mpu9250_get_accel_sample_rate(st, &context.accel_sample_rate);
	if (config->compass_enabled) {
		result |= mpu9250_get_compass_sample_rate(st,
				&context.compass_sample_rate);
	} else {
		context.compass_sample_rate = 0;
		dev_dbginfo("mpu9250 read context : compass disable\n");
	}
	result |= mpu9250_get_gyro_fsr(st, &context.gyro_fsr);
	result |= mpu9250_get_accel_fsr(st, &context.accel_fsr);
	result |= mpu9250_get_compass_fsr(st, &context.compass_fsr);

	if (result) {
		dev_dbgerr("mpu9250_read_context : failed\n");
	} else {
		if (buf)
			memcpy(buf, (char *)&context,
				sizeof(struct mpu9250_context));
		dev_dbginfo("mpu9250_read_context : success\n");
	}

	return result;
}

static int mpu9250_initialize_compass(struct inv_mpu9250_state *st)
{
	struct mpu9250_chip_config *config = NULL;
	struct reg_cfg *reg_cfg_info = NULL;
	uint8_t i2c_mst_delay;
	int compass_sample_rate = 0;
	int gyro_sample_rate = 0;
	int result = MPU_SUCCESS;

	if (st == NULL)
		return -MPU_FAIL;

	reg_cfg_info = &st->reg_cfg_info;
	config = st->config;
	gyro_sample_rate = gyro_sample_rate_enum_to_hz(
					config->gyro_sample_rate);
	compass_sample_rate = compass_sample_rate_enum_to_hz(
					config->compass_sample_rate);

	reg_cfg_info->i2c_mst_ctrl &= ~BIT_WAIT_FOR_ES;
	reg_cfg_info->i2c_mst_ctrl |= 0x0d;
	result |= inv_mpu9250_write_reg(st, st->reg->i2c_mst_ctrl,
					reg_cfg_info->i2c_mst_ctrl);

	/**
	*Enable I2C master module
	*Configure mpu9250 as I2C master
	*to communicate with compass slave device
	*/
	reg_cfg_info->user_ctrl |= BIT_I2C_MST_EN;
	result |= inv_mpu9250_write_reg(st, st->reg->user_ctrl,
					reg_cfg_info->user_ctrl);

	result |= mpu9250_detect_compass(st);

	/** get compass calibraion data from Fuse ROM*/
	result |= mpu9250_compass_senitivity_adjustment(st);

	/** Slave 0 reads from slave address 0x0C, i.e. compass*/
	result |= inv_mpu9250_write_reg(st, st->reg->i2c_slv0_addr, 0x8c);

	/** start reading from register 0x02 ST1*/
	result |= inv_mpu9250_write_reg(st, st->reg->i2c_slv0_reg, 0x02);

	/**
	*Enable 8 bytes reading from slave 0 at gyro sample rate
	*NOTE: compass sample rate is lower than gyro sample rate, so
	*the same measurement samples are transferred
	*to SLV0 at gyro sample rate.
	*but this is fine
	*/
	result |= inv_mpu9250_write_reg(st, st->reg->i2c_slv0_ctrl, 0x88);

	/** Slave 1 sets compass measurement mode*/
	result |= inv_mpu9250_write_reg(st, st->reg->i2c_slv1_addr, 0x0c);
	/** write to compass CNTL1 register*/
	result |= inv_mpu9250_write_reg(st, st->reg->i2c_slv1_reg,
					MPU9250_COMP_REG_CNTL1);

	result |= inv_mpu9250_write_reg(st, st->reg->i2c_slv1_ctrl, 0x81);

	/**
	*set slave 1 data to write
	*0x10 indicates use 16bit compass measurement
	*0x01 indicates using single measurement mode
	*/
	result |= inv_mpu9250_write_reg(st, st->reg->i2c_slv1_do, 0x11);

	/**
	*conduct 1 trasnfer at delayed sample rate
	*I2C_MST_DLY = (gryo_sample_rate / campass_sample_rate - 1)
	*/
	i2c_mst_delay = gyro_sample_rate/compass_sample_rate - 1;
	result |= inv_mpu9250_write_reg(st,
					st->reg->i2c_slv4_ctrl, i2c_mst_delay);

	/**
	*trigger slave 1 transfer every (1+I2C_MST_DLY) samples
	*every time slave 1 transfer is triggered,
	*a compass measurement conducted
	*reg_cfg_info->i2c_mst_delay_ctrl |= BIT_DELAY_ES_SHADOW;
	*/
	reg_cfg_info->i2c_mst_delay_ctrl |= BIT_SLV1_DLY_EN;
	result |= inv_mpu9250_write_reg(st, st->reg->i2c_mst_delay_ctrl,
					reg_cfg_info->i2c_mst_delay_ctrl);
	if (result)
		dev_dbgerr("mpu9250 init compass failed\n");
	else
		dev_dbginfo("mpu9250 init compass success\n");

	return result;
}

static int mpu9250_initialize_gyro(struct inv_mpu9250_state *st)
{
	struct mpu9250_chip_config *config = NULL;
	struct reg_cfg *reg_cfg_info = NULL;
	int result = MPU_SUCCESS;
	int sample_rate;
	uint8_t fchoice_b;

	if (st == NULL)
		return -MPU_FAIL;

	/**
	*MPU9250 supports gyro sampling rate up to 32KHz when fchoice_b != 0x00
	*In our driver, we supports up to 8KHz
	*thus always set fchoice_b to 0x00;
	*/
	config = st->config;
	fchoice_b = 0x00;
	sample_rate = gyro_sample_rate_enum_to_hz(config->gyro_sample_rate);
	/**
	*SAPLRT_DIV in MPU9250_REG_SMPLRT_DIV is only used for 1kHz internal
	*sampling, i.e. fchoice_b in MPU9250_REG_GYRO_CONFIG is 00
	*and 0 < dlpf_cfg in MPU9250_REG_CONFIG < 7
	*SAMPLE_RATE=Internal_Sample_Rate / (1 + SMPLRT_DIV)
	*/
	reg_cfg_info = &st->reg_cfg_info;
	if (config->gyro_sample_rate <= MPU9250_SAMPLE_RATE_1000HZ) {
		reg_cfg_info->smplrt_div =
			MPU9250_INTERNAL_SAMPLE_RATE_HZ / sample_rate - 1;
	}

	/** Set SMPLRT_DIV with the calculated divider*/
	result |= inv_mpu9250_write_reg(st, st->reg->sample_rate_div,
						reg_cfg_info->smplrt_div);

	/** Set gyro LPF*/
	reg_cfg_info->config |= config->gyro_lpf;
	result |= inv_mpu9250_write_reg(st, st->reg->config,
						reg_cfg_info->config);

	/** Set gyro full scale range*/
	reg_cfg_info->gyro_config |= fchoice_b + (config->gyro_fsr << 3);
	result |= inv_mpu9250_write_reg(st, st->reg->gyro_config,
						reg_cfg_info->gyro_config);

	/** Set Accel full scale range*/
	reg_cfg_info->accel_config |= config->acc_fsr << 3;
	result |= inv_mpu9250_write_reg(st, st->reg->accl_config,
						reg_cfg_info->accel_config);

	/**
	*Set accel LPF
	*Support accel sample rate up to 1KHz, thus set accel_fchoice_b to 0x00
	*The actual accel sample rate is 1KHz/(1+SMPLRT_DIV)
	*/
	if ((reg_cfg_info->accel_config2 & 0x08) == 0) {
		reg_cfg_info->accel_config2 |= config->acc_lpf;
		result |= inv_mpu9250_write_reg(st, st->reg->accl_config2,
						reg_cfg_info->accel_config2);
	}

	if (result) {
		dev_dbgerr("mpu9250 init gyro failed\n");
		return -MPU_FAIL;
	}

	dev_dbginfo("mpu9250 init gyro success\n");
	return result;
}

static int mpu9250_validate_configuration(struct mpu9250_chip_config *config)
{
	int result = MPU_SUCCESS;

	if (config == NULL)
		return -MPU_FAIL;

	return result;
}

static int inv_mpu9250_init_device(struct inv_mpu9250_state *st)
{
	int result = MPU_SUCCESS;
	struct mpu9250_chip_config *config = NULL;
	struct reg_cfg *reg_cfg_info = NULL;

	if (st == NULL)
		return -MPU_FAIL;

	if (mpu9250_validate_configuration(st->config)) {
		dev_dbgerr("mpu9250 validate config failed\n");
		return -MPU_FAIL;
	}

    /** clear the cached reg cfg info*/
	reg_cfg_info = &st->reg_cfg_info;
	memset(reg_cfg_info, 0x0, sizeof(struct reg_cfg));

	/** turn on gyro and accel*/
	reg_cfg_info->pwr_mgmt2 = 0x0;
	result |= inv_mpu9250_write_reg(st, st->reg->pwr_mgmt_2,
						reg_cfg_info->pwr_mgmt2);
	msleep(INV_MPU9250_POWER_UP_TIME);

	/** disable INT*/
	reg_cfg_info->int_en = 0x0;
	result |= inv_mpu9250_write_reg(st, st->reg->int_enable,
						reg_cfg_info->int_en);

	/** disbale FIFO*/
	reg_cfg_info->fifo_enable = 0x0;
	result |= inv_mpu9250_write_reg(st, st->reg->fifo_en,
					reg_cfg_info->fifo_enable);

    /** disbale FIFO I2C DMP*/
	reg_cfg_info->user_ctrl = 0x0;
	result |= inv_mpu9250_write_reg(st, st->reg->user_ctrl,
					reg_cfg_info->user_ctrl);

	/** disable I2C IF*/
	reg_cfg_info->user_ctrl |= BIT_I2C_IF_DIS;
	result |= inv_mpu9250_write_reg(st, st->reg->user_ctrl,
					reg_cfg_info->user_ctrl);

	/** reset FIFO*/
	result |= inv_mpu9250_write_reg(st, st->reg->user_ctrl,
					reg_cfg_info->user_ctrl | BIT_FIFO_RST);
	msleep(INV_MPU9250_POWER_UP_TIME);

	/** detect gyro*/
	if (inv_mpu9250_detect_gyro(st)) {
		dev_dbgerr("mpu9250 init device failed\n");
		return -MPU_FAIL;
	}

	/** init gyro and accel*/
	if (mpu9250_initialize_gyro(st)) {
		dev_dbgerr("mpu9250 init device failed\n");
		return -MPU_FAIL;
	}

	/** if compass enable, init compass*/
	config = st->config;
	if (config->compass_enabled) {
		if (mpu9250_initialize_compass(st)) {
			dev_dbgerr("mpu9250 init compass failed\n");
			return -MPU_FAIL;
		}
	}

	/** if FIFO enable, config FIFO*/
	if (config->fifo_enabled) {
		if (mpu9250_config_fifo(st)) {
			dev_dbgerr("mpu9250 init config fifo failed\n");
			return -MPU_FAIL;
		}
	} else {
		st->fifo_packet_size = 0;
		if (config->fifo_en_mask & BIT_TEMP_FIFO_EN)
			st->fifo_packet_size += 2;
		if (config->fifo_en_mask & BIT_GYRO_FIFO_EN)
			st->fifo_packet_size += 6;
		if (config->fifo_en_mask & BIT_ACCEL_FIFO_EN)
			st->fifo_packet_size += 6;

		/** enable interrupt*/
		reg_cfg_info->int_en |= BIT_RAW_RDY_EN;
		if (inv_mpu9250_write_reg(st, st->reg->int_enable,
						reg_cfg_info->int_en)) {
			dev_dbgerr("mpu9250 set raw rdy failed\n");
			return -MPU_FAIL;
		}
	}

	dev_dbginfo("mpu9250 init device finished\n");
	return result;
}

static int inv_check_and_setup_chip(struct inv_mpu9250_state *st)
{
	int result = MPU_SUCCESS;

	st->chip_type = INV_MPU9250;
	st->hw  = &hw_info[st->chip_type];
	st->config = &chip_config[INIT_200HZ];
	st->reg = hw_info[st->chip_type].reg;

	result = inv_mpu9250_detect(st);
	if (result) {
		dev_dbgerr("mpu9250 detect failed\n");
		return result;
	}

	result = inv_mpu9250_init_device(st);
	if (result) {
		dev_dbgerr("mpu9250 device init failed\n");
		return result;
	}

	dev_dbginfo("mpu9250 check and set up chip success\n");
	return result;
}

#ifdef CONFIG_DEBUG_FS
static int debugfs_inv_mpu9250_set(void *data, u64 val)
{
	int result = MPU_SUCCESS;
	char *data_buf = NULL;
	struct inv_mpu9250_state *st = (struct inv_mpu9250_state *)data;

	if (st == NULL) {
		dev_dbgerr("debugfs_inv_mpu9250_set st is NULL\n");
		return -MPU_FAIL;
	}

	data_buf = kzalloc(4096, GFP_ATOMIC);
	if (!data_buf)
		dev_dbgerr("debugfs read fifo buf alloc failed\n");

	switch (val) {
	case SET_DETECT:
		result = inv_check_and_setup_chip(st);
		if (result)
			dev_dbgerr("debugfs detect sensor failed\n");
		break;
	case SET_DEBUG:
		mpu9250_debug_enable = 1;
		break;
	case UNSET_DEBUG:
		mpu9250_debug_enable = 0;
		break;
	case READ_FIFO:
		mpu9250_read_fifo(st, data_buf, 4096);
		break;
	case BULK_READ_RAW:
		mpu9250_bulk_read_raw(st, data_buf, st->fifo_packet_size);
		break;
	default:
		dev_dbginfo("debugfs_inv_mpu9250_set :  default do nothing\n");
	}

	kfree(data_buf);
	data_buf = NULL;
	return result;
}

static int debugfs_inv_mpu9250_get(void *data, u64 *val)
{
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_inv_mpu9250, debugfs_inv_mpu9250_get,
			debugfs_inv_mpu9250_set, "0x%08llx\n");

static void inv_mpu9250_debugfs_init(struct inv_mpu9250_state *st)
{
	char dir_name[20];
	struct dentry *dent_spi;

	scnprintf(dir_name, sizeof(dir_name), "%s_dbg", "mpu9250");
	dent_spi = debugfs_create_dir(dir_name, NULL);
	if (dent_spi) {
		debugfs_create_file(
			       "mpu9250-start",
			       S_IRUGO | S_IWUSR,
			       dent_spi, st,
			       &fops_inv_mpu9250);
	}
}

#else
static void inv_mpu9250_debugfs_init(void) {}
#endif

static int of_populate_mpu9250_dt(struct inv_mpu9250_state *st)
{
	int result = MPU_SUCCESS;

	/** use spi device irq */
	st->gpio = of_get_named_gpio(st->spi->dev.of_node, "invn,mpu9250-irq", 0);
	result = gpio_request(st->gpio, "mpu9250-irq");
	if (result) {
		dev_dbgerr("gpio request %d failed\n", st->gpio);
		return -MPU_FAIL;
	}
	st->spi->irq = gpio_to_irq(st->gpio);

	return result;
}

static int inv_mpu9250_probe(struct spi_device *spi)
{
	struct inv_mpu9250_state *st;
	struct iio_dev *indio_dev;
	struct inv_mpu9250_platform_data *pdata;
	int result = MPU_SUCCESS;

	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		result =  -ENOMEM;
		dev_dbgerr("alloc iio device failed\n");
		goto out_no_free;
	}
	st = iio_priv(indio_dev);
	st->spi = spi;
	spi->bits_per_word = 8;
	st->fifo_cnt_threshold = 140;

	result = of_populate_mpu9250_dt(st);
	if (result) {
		dev_dbgerr("populate dt failed\n");
		goto out_free;
	}

	pdata = (struct inv_mpu9250_platform_data *)dev_get_platdata(&spi->dev);
	if (pdata)
		st->plat_data = *pdata;

	/** init buff */
	st->rx_buf = kzalloc(MPU_SPI_BUF_LEN, GFP_ATOMIC);
	if (!(st->rx_buf)) {
		result = -ENOMEM;
		dev_dbgerr("malloc rx buffer failed\n");
		goto out_free;
	}

	st->tx_buf = kzalloc(MPU_SPI_BUF_LEN, GFP_ATOMIC);
	if (!(st->tx_buf)) {
		result = -ENOMEM;
		dev_dbgerr("malloc tx buffer failed\n");
		goto out_free_spi1;
	}

	dev_set_drvdata(&spi->dev, indio_dev);
	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = MPU9250_DEV_NAME;
	indio_dev->channels = inv_mpu9250_channels;
	indio_dev->num_channels = ARRAY_SIZE(inv_mpu9250_channels);

	indio_dev->info = &mpu9250_info;
	indio_dev->modes = INDIO_BUFFER_TRIGGERED;

	result = iio_triggered_buffer_setup(indio_dev,
					    inv_mpu9250_irq_handler,
					    inv_mpu9250_read_fifo_fn,
					    NULL);
	if (result) {
		dev_err(&st->spi->dev, " configure buffer fail %d\n",
				result);
		goto out_free_spi2;
	}
	result = inv_mpu9250_probe_trigger(indio_dev);
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

#ifdef CONFIG_DEBUG_FS
		inv_mpu9250_debugfs_init(st);
#endif

	return 0;

out_remove_trigger:
	inv_mpu9250_remove_trigger(st);
out_unreg_ring:
	iio_triggered_buffer_cleanup(indio_dev);
out_free_spi2:
	kfree(st->tx_buf);
out_free_spi1:
	kfree(st->rx_buf);
out_free:
	iio_device_free(indio_dev);
	gpio_free(st->gpio);
out_no_free:

	return 0;
}

static int inv_mpu9250_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = dev_get_drvdata(&spi->dev);
	struct inv_mpu9250_state *st = iio_priv(indio_dev);

	mpu9250_stop_fifo(st);
	kfree(st->tx_buf);
	kfree(st->rx_buf);
	gpio_free(st->gpio);
	iio_device_unregister(indio_dev);
	inv_mpu9250_remove_trigger(st);
	iio_triggered_buffer_cleanup(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

#ifdef CONFIG_PM
static int inv_mpu9250_suspend(struct spi_device *spi, pm_message_t state)
{
	struct iio_dev *indio_dev = dev_get_drvdata(&spi->dev);
	struct inv_mpu9250_state *st = iio_priv(indio_dev);

	inv_mpu9250_set_power_itg(st, false);
	return 0;
}

static int inv_mpu9250_resume(struct spi_device *spi)
{
	struct iio_dev *indio_dev = dev_get_drvdata(&spi->dev);
	struct inv_mpu9250_state *st = iio_priv(indio_dev);

	inv_mpu9250_set_power_itg(st, true);
	return 0;
}
#else
#define inv_mpu9250_suspend NULL
#define inv_mpu9250_resume NULL
#endif

static struct of_device_id inv_mpu9250_match_table[] = {
	{
		.compatible = "invn,mpu9250",
	},
	{}
};

static struct spi_driver inv_mpu9250_driver = {
	.driver = {
		.name = "inv-mpu9250",
		.of_match_table = inv_mpu9250_match_table,
		.owner = THIS_MODULE,
	},
	.probe = inv_mpu9250_probe,
	.remove = inv_mpu9250_remove,
#ifdef CONFIG_PM
	.suspend = inv_mpu9250_suspend,
	.resume = inv_mpu9250_resume,
#endif
};

static int __init inv_mpu9250_init(void)
{
	return spi_register_driver(&inv_mpu9250_driver);
}

static void __exit inv_mpu9250_exit(void)
{
	spi_unregister_driver(&inv_mpu9250_driver);
}

module_init(inv_mpu9250_init);
module_exit(inv_mpu9250_exit);
MODULE_DESCRIPTION("Invensense device MPU9250 driver");
MODULE_LICENSE("GPL v2");

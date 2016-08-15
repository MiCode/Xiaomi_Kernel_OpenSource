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
 *      @file    inv_mpu_core.c
 *      @brief   A sysfs device driver for Invensense devices
 *      @details This driver currently works for the
 *               MPU3050/MPU6050/MPU9150/MPU6500/MPU9250 devices.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

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

#include "inv_mpu_iio.h"
#include "sysfs.h"
#include "inv_test/inv_counters.h"

s64 get_time_ns(void)
{
	struct timespec ts;
	ktime_get_ts(&ts);
	return timespec_to_ns(&ts);
}

static const short AKM8975_ST_Lower[3] = {-100, -100, -1000};
static const short AKM8975_ST_Upper[3] = {100, 100, -300};

static const short AKM8972_ST_Lower[3] = {-50, -50, -500};
static const short AKM8972_ST_Upper[3] = {50, 50, -100};

static const short AKM8963_ST_Lower[3] = {-200, -200, -3200};
static const short AKM8963_ST_Upper[3] = {200, 200, -800};

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{119, "ITG3500"},
	{63, "MPU3050"},
	{117, "MPU6050"},
	{118, "MPU9150"},
	{119, "MPU6500"},
	{118, "MPU9250"},
};

static void inv_setup_reg(struct inv_reg_map_s *reg)
{
	reg->sample_rate_div	= REG_SAMPLE_RATE_DIV;
	reg->lpf		= REG_CONFIG;
	reg->bank_sel		= REG_BANK_SEL;
	reg->user_ctrl		= REG_USER_CTRL;
	reg->fifo_en		= REG_FIFO_EN;
	reg->gyro_config	= REG_GYRO_CONFIG;
	reg->accl_config	= REG_ACCEL_CONFIG;
	reg->fifo_count_h	= REG_FIFO_COUNT_H;
	reg->fifo_r_w		= REG_FIFO_R_W;
	reg->raw_gyro		= REG_RAW_GYRO;
	reg->raw_accl		= REG_RAW_ACCEL;
	reg->temperature	= REG_TEMPERATURE;
	reg->int_enable		= REG_INT_ENABLE;
	reg->int_status		= REG_INT_STATUS;
	reg->pwr_mgmt_1		= REG_PWR_MGMT_1;
	reg->pwr_mgmt_2		= REG_PWR_MGMT_2;
	reg->mem_start_addr	= REG_MEM_START_ADDR;
	reg->mem_r_w		= REG_MEM_RW;
	reg->prgm_strt_addrh	= REG_PRGM_STRT_ADDRH;
};

/**
 *  inv_i2c_read() - Read one or more bytes from the device registers.
 *  @st:	Device driver instance.
 *  @reg:	First device register to be read from.
 *  @length:	Number of bytes to read.
 *  @data:	Data read from device.
 *  NOTE:This is not re-implementation of i2c_smbus_read because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_read_base(struct inv_mpu_iio_s *st, u16 i2c_addr,
	u8 reg, u16 length, u8 *data)
{
	struct i2c_msg msgs[2];
	int res;

	if (!data)
		return -EINVAL;

	msgs[0].addr = i2c_addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = i2c_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].len = length;

	res = i2c_transfer(st->sl_handle, msgs, 2);

	if (res < 2) {
		if (res >= 0)
			res = -EIO;
	} else
		res = 0;

	INV_I2C_INC_MPUWRITE(3);
	INV_I2C_INC_MPUREAD(length);
#if CONFIG_DYNAMIC_DEBUG
	{
		char *read = 0;
		pr_debug("%s RD%02X%02X%02X -> %s%s\n", st->hw->name,
			 i2c_addr, reg, length,
			 wr_pr_debug_begin(data, length, read),
			 wr_pr_debug_end(read));
	}
#endif
	return res;
}

/**
 *  inv_i2c_single_write() - Write a byte to a device register.
 *  @st:	Device driver instance.
 *  @reg:	Device register to be written to.
 *  @data:	Byte to write to device.
 *  NOTE:This is not re-implementation of i2c_smbus_write because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_single_write_base(struct inv_mpu_iio_s *st,
	u16 i2c_addr, u8 reg, u8 data)
{
	u8 tmp[2];
	struct i2c_msg msg;
	int res;

	tmp[0] = reg;
	tmp[1] = data;

	msg.addr = i2c_addr;
	msg.flags = 0;	/* write */
	msg.buf = tmp;
	msg.len = 2;

	pr_debug("%s WR%02X%02X%02X\n", st->hw->name, i2c_addr, reg, data);
	INV_I2C_INC_MPUWRITE(3);

	res = i2c_transfer(st->sl_handle, &msg, 1);
	if (res < 1) {
		if (res == 0)
			res = -EIO;
		return res;
	} else
		return 0;
}

static int inv_switch_engine(struct inv_mpu_iio_s *st, bool en, u32 mask)
{
	struct inv_reg_map_s *reg;
	u8 data, mgmt_1;
	int result;
	reg = &st->reg;
	/* switch clock needs to be careful. Only when gyro is on, can
	   clock source be switched to gyro. Otherwise, it must be set to
	   internal clock */
	if (BIT_PWR_GYRO_STBY == mask) {
		result = inv_i2c_read(st, reg->pwr_mgmt_1, 1, &mgmt_1);
		if (result)
			return result;

		mgmt_1 &= ~BIT_CLK_MASK;
	}

	if ((BIT_PWR_GYRO_STBY == mask) && (!en)) {
		/* turning off gyro requires switch to internal clock first.
		   Then turn off gyro engine */
		mgmt_1 |= INV_CLK_INTERNAL;
		result = inv_i2c_single_write(st, reg->pwr_mgmt_1,
						mgmt_1);
		if (result)
			return result;
	}

	result = inv_i2c_read(st, reg->pwr_mgmt_2, 1, &data);
	if (result)
		return result;
	if (en)
		data &= (~mask);
	else
		data |= mask;
	result = inv_i2c_single_write(st, reg->pwr_mgmt_2, data);
	if (result)
		return result;

	if ((BIT_PWR_GYRO_STBY == mask) && en) {
		/* only gyro on needs sensor up time */
		msleep(SENSOR_UP_TIME);
		/* after gyro is on & stable, switch internal clock to PLL */
		mgmt_1 |= INV_CLK_PLL;
		result = inv_i2c_single_write(st, reg->pwr_mgmt_1,
						mgmt_1);
		if (result)
			return result;
	}

	return 0;
}

/**
 *  inv_lpa_freq() - store current low power frequency setting.
 */
static int inv_lpa_freq(struct inv_mpu_iio_s *st, int lpa_freq)
{
	unsigned long result;
	u8 d;
	struct inv_reg_map_s *reg;
	/* this mapping makes 6500 and 6050 setting close */
	/* 2, 4, 6, 7 corresponds to 0.98, 3.91, 15.63, 31.25 */
	const u8 mpu6500_lpa_mapping[] = {2, 4, 6, 7};

	if (lpa_freq > MAX_LPA_FREQ_PARAM)
		return -EINVAL;

	if (INV_MPU6500 == st->chip_type) {
		d = mpu6500_lpa_mapping[lpa_freq];
		result = inv_i2c_single_write(st, REG_6500_LP_ACCEL_ODR, d);
		if (result)
			return result;
	} else {
		reg = &st->reg;
		result = inv_i2c_read(st, reg->pwr_mgmt_2, 1, &d);
		if (result)
			return result;
		d &= ~BIT_LPA_FREQ;
		d |= (u8)(lpa_freq << LPA_FREQ_SHIFT);
		result = inv_i2c_single_write(st, reg->pwr_mgmt_2, d);
		if (result)
			return result;
	}
	st->chip_config.lpa_freq = lpa_freq;

	return 0;
}

static int set_power_itg(struct inv_mpu_iio_s *st, bool power_on)
{
	struct inv_reg_map_s *reg;
	u8 data;
	int result;

	reg = &st->reg;
	if (power_on)
		data = 0;
	else
		data = BIT_SLEEP;
	result = inv_i2c_single_write(st, reg->pwr_mgmt_1, data);
	if (result)
		return result;

	if (power_on) {
		msleep(POWER_UP_TIME);
		result = inv_switch_engine(st, st->chip_config.gyro_enable,
					BIT_PWR_GYRO_STBY);
		if (result)
			return result;
		result = inv_switch_engine(st, st->chip_config.accl_enable,
					BIT_PWR_ACCL_STBY);
		if (result)
			return result;
		result = inv_lpa_freq(st, st->chip_config.lpa_freq);
		if (result)
			return result;
	}
	st->chip_config.is_asleep = !power_on;

	return 0;
}

/**
 *  inv_init_config() - Initialize hardware, disable FIFO.
 *  @indio_dev:	Device driver instance.
 *  Initial configuration:
 *  FSR: +/- 2000DPS
 *  DLPF: 42Hz
 *  FIFO rate: 50Hz
 */
static int inv_init_config(struct iio_dev *indio_dev)
{
	struct inv_reg_map_s *reg;
	int result;
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);

	if (st->chip_config.is_asleep)
		return -EPERM;

	reg = &st->reg;
	result = set_inv_enable(indio_dev, false);
	if (result)
		return result;

	result = inv_i2c_single_write(st, reg->gyro_config,
		INV_FSR_2000DPS << GYRO_CONFIG_FSR_SHIFT);
	if (result)
		return result;

	st->chip_config.fsr = INV_FSR_2000DPS;

	result = inv_i2c_single_write(st, reg->lpf, INV_FILTER_42HZ);
	if (result)
		return result;
	st->chip_config.lpf = INV_FILTER_42HZ;

	result = inv_i2c_single_write(st, reg->sample_rate_div,
					ONE_K_HZ / INIT_FIFO_RATE - 1);
	if (result)
		return result;
	st->chip_config.fifo_rate = INIT_FIFO_RATE;
	st->irq_dur_ns = INIT_DUR_TIME;
	st->chip_config.prog_start_addr = DMP_START_ADDR;
	st->chip_config.gyro_enable = 1;
	st->chip_config.gyro_fifo_enable = 1;
	st->chip_config.dmp_output_rate = INIT_DMP_OUTPUT_RATE;
	if (INV_ITG3500 != st->chip_type) {
		st->chip_config.accl_enable = 1;
		st->chip_config.accl_fifo_enable = 1;
		st->chip_config.accl_fs = INV_FS_02G;
		result = inv_i2c_single_write(st, reg->accl_config,
			(INV_FS_02G << ACCL_CONFIG_FSR_SHIFT));
		if (result)
			return result;
		st->tap.time = INIT_TAP_TIME;
		st->tap.thresh = INIT_TAP_THRESHOLD;
		st->tap.min_count = INIT_TAP_MIN_COUNT;

		result = inv_i2c_single_write(st, REG_ACCEL_MOT_DUR,
						INIT_MOT_DUR);
		if (result)
			return result;
		st->mot_int.mot_dur = INIT_MOT_DUR;

		result = inv_i2c_single_write(st, REG_ACCEL_MOT_THR,
						INIT_MOT_THR);
		if (result)
			return result;
		st->mot_int.mot_thr = INIT_MOT_THR;
	}

	return 0;
}

/**
 *  inv_compass_scale_show() - show compass scale.
 */
static int inv_compass_scale_show(struct inv_mpu_iio_s *st, int *scale)
{
	if (COMPASS_ID_AK8975 == st->plat_data.sec_slave_id)
		*scale = DATA_AKM8975_SCALE;
	else if (COMPASS_ID_AK8972 == st->plat_data.sec_slave_id)
		*scale = DATA_AKM8972_SCALE;
	else if (COMPASS_ID_AK8963 == st->plat_data.sec_slave_id)
		if (st->compass_scale)
			*scale = DATA_AKM8963_SCALE1;
		else
			*scale = DATA_AKM8963_SCALE0;
	else
		return -EINVAL;

	return IIO_VAL_INT;
}

/**
 *  mpu_read_raw() - read raw method.
 */
static int mpu_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	int result;
	if (st->chip_config.is_asleep)
		return -EINVAL;
	switch (mask) {
	case 0:
		if (!st->chip_config.enable)
			return -EPERM;
		switch (chan->type) {
		case IIO_ANGL_VEL:
			if (!st->chip_config.gyro_enable)
				return -EPERM;
			*val = st->raw_gyro[chan->channel2 - IIO_MOD_X];
			return IIO_VAL_INT;
		case IIO_ACCEL:
			if (!st->chip_config.accl_enable)
				return -EPERM;
			*val = st->raw_accel[chan->channel2 - IIO_MOD_X];
			return IIO_VAL_INT;
		case IIO_MAGN:
			if (!st->chip_config.compass_enable)
				return -EPERM;
			*val = st->raw_compass[chan->channel2 - IIO_MOD_X];
			return IIO_VAL_INT;
		case IIO_QUATERNION:
			if (!(st->chip_config.dmp_on
				&& st->chip_config.quaternion_on))
				return -EPERM;
			if (IIO_MOD_R == chan->channel2)
				*val = st->raw_quaternion[0];
			else
				*val = st->raw_quaternion[chan->channel2 -
							  IIO_MOD_X + 1];
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
		return -EINVAL;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_ANGL_VEL:
		{
			const s16 gyro_scale[] = {250, 500, 1000, 2000};

			*val = gyro_scale[st->chip_config.fsr];

			return IIO_VAL_INT;
		}
		case IIO_ACCEL:
		{
			const s16 accel_scale[] = {2, 4, 8, 16};
			*val = accel_scale[st->chip_config.accl_fs];
			return IIO_VAL_INT;
		}
		case IIO_MAGN:
			return inv_compass_scale_show(st, val);
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_CALIBBIAS:
		/* return bias=0 for both accel and gyro for MPU6500;
		   self test not supported yet */
		if (INV_MPU6500 == st->chip_type) {
			*val = 0;
			return IIO_VAL_INT;
		}
		if (st->chip_config.self_test_run_once == 0) {
			result = inv_do_test(st, 0, st->gyro_bias,
				st->accel_bias);
			/* Reset Accel and Gyro full scale range
			   back to default value */
			inv_recover_setting(st);
			if (result)
				return result;
			st->chip_config.self_test_run_once = 1;
		}

		switch (chan->type) {
		case IIO_ANGL_VEL:
			*val = st->gyro_bias[chan->channel2 - IIO_MOD_X];
			return IIO_VAL_INT;
		case IIO_ACCEL:
			*val = st->accel_bias[chan->channel2 - IIO_MOD_X] *
					st->chip_info.multi;
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		switch (chan->type) {
		case IIO_ACCEL:
			*val = st->input_accel_bias[chan->channel2 - IIO_MOD_X];
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

/**
 *  inv_write_fsr() - Configure the gyro's scale range.
 */
static int inv_write_fsr(struct inv_mpu_iio_s *st, int fsr)
{
	struct inv_reg_map_s *reg;
	int result;
	reg = &st->reg;
	if ((fsr < 0) || (fsr > MAX_GYRO_FS_PARAM))
		return -EINVAL;
	if (fsr == st->chip_config.fsr)
		return 0;

	if (INV_MPU3050 == st->chip_type)
		result = inv_i2c_single_write(st, reg->lpf,
			(fsr << GYRO_CONFIG_FSR_SHIFT) | st->chip_config.lpf);
	else
		result = inv_i2c_single_write(st, reg->gyro_config,
			fsr << GYRO_CONFIG_FSR_SHIFT);

	if (result)
		return result;
	st->chip_config.fsr = fsr;

	return 0;
}

/**
 *  inv_write_accel_fs() - Configure the accelerometer's scale range.
 */
static int inv_write_accel_fs(struct inv_mpu_iio_s *st, int fs)
{
	int result;
	struct inv_reg_map_s *reg;
	reg = &st->reg;

	if (fs < 0 || fs > MAX_ACCL_FS_PARAM)
		return -EINVAL;
	if (fs == st->chip_config.accl_fs)
		return 0;
	if (INV_MPU3050 == st->chip_type)
		result = st->mpu_slave->set_fs(st, fs);
	else
		result = inv_i2c_single_write(st, reg->accl_config,
				(fs << ACCL_CONFIG_FSR_SHIFT));
	if (result)
		return result;

	st->chip_config.accl_fs = fs;

	return 0;
}

/**
 *  inv_write_compass_scale() - Configure the compass's scale range.
 */
static int inv_write_compass_scale(struct inv_mpu_iio_s *st, int data)
{
	char d, en;
	int result;
	if (COMPASS_ID_AK8963 != st->plat_data.sec_slave_id)
		return 0;
	en = !!data;
	if (st->compass_scale == en)
		return 0;
	d = (DATA_AKM_MODE_SM | (st->compass_scale << AKM8963_SCALE_SHIFT));
	result = inv_i2c_single_write(st, REG_I2C_SLV1_DO, d);
	if (result)
		return result;
	st->compass_scale = en;

	return 0;
}

static inline int check_enable(struct inv_mpu_iio_s *st)
{
	return st->chip_config.is_asleep | st->chip_config.enable;
}

static inline int check_dmp_on(struct inv_mpu_iio_s *st)
{
	return (!st->chip_config.is_asleep) &&
		st->chip_config.enable && st->chip_config.dmp_on;
}

/**
 *  mpu_write_raw() - write raw method.
 */
static int mpu_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask) {
	struct inv_mpu_iio_s  *st = iio_priv(indio_dev);
	int result;
	if (check_enable(st))
		return -EPERM;
	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_ANGL_VEL:
			return inv_write_fsr(st, val);
		case IIO_ACCEL:
			return inv_write_accel_fs(st, val);
		case IIO_MAGN:
			return inv_write_compass_scale(st, val);
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		switch (chan->type) {
		case IIO_ACCEL:
			if (!st->chip_config.firmware_loaded)
				return -EPERM;
			result = inv_set_accel_bias_dmp(st);
			if (result)
				return result;
			st->input_accel_bias[chan->channel2 - IIO_MOD_X] = val;
			return 0;
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 *  inv_set_lpf() - set low pass filer based on fifo rate.
 */
static int inv_set_lpf(struct inv_mpu_iio_s *st, int rate)
{
	const short hz[] = {188, 98, 42, 20, 10, 5};
	const int d[] = {INV_FILTER_188HZ, INV_FILTER_98HZ,
			INV_FILTER_42HZ, INV_FILTER_20HZ,
			INV_FILTER_10HZ, INV_FILTER_5HZ};
	int i, h, data, result;
	struct inv_reg_map_s *reg;
	reg = &st->reg;
	h = (rate >> 1);
	i = 0;
	while ((h < hz[i]) && (i < ARRAY_SIZE(d) - 1))
		i++;
	data = d[i];
	if (INV_MPU3050 == st->chip_type) {
		if (st->mpu_slave != NULL) {
			result = st->mpu_slave->set_lpf(st, rate);
			if (result)
				return result;
		}
		result = inv_i2c_single_write(st, reg->lpf, data |
			(st->chip_config.fsr << GYRO_CONFIG_FSR_SHIFT));
	} else
		result = inv_i2c_single_write(st, reg->lpf, data);

	if (result)
		return result;
	st->chip_config.lpf = data;

	return 0;
}

/**
 *  inv_fifo_rate_store() - Set fifo rate.
 */
static ssize_t inv_fifo_rate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u32 fifo_rate;
	u8 data;
	int result;
	struct inv_mpu_iio_s *st = iio_priv(dev_get_drvdata(dev));
	struct inv_reg_map_s *reg;
	reg = &st->reg;

	if (check_enable(st))
		return -EPERM;
	if (kstrtouint(buf, 10, &fifo_rate))
		return -EINVAL;
	if ((fifo_rate < MIN_FIFO_RATE) || (fifo_rate > MAX_FIFO_RATE))
		return -EINVAL;
	if (fifo_rate == st->chip_config.fifo_rate)
		return count;
	if (st->chip_config.has_compass) {
		st->compass_divider = COMPASS_RATE_SCALE * fifo_rate /
					ONE_K_HZ;
		if (st->compass_divider > 0)
			st->compass_divider -= 1;
		st->compass_counter = 0;
	}
	data = ONE_K_HZ / fifo_rate - 1;
	result = inv_i2c_single_write(st, reg->sample_rate_div, data);
	if (result)
		return result;
	st->chip_config.fifo_rate = fifo_rate;
	result = inv_set_lpf(st, fifo_rate);
	if (result)
		return result;
	st->irq_dur_ns = (data + 1) * NSEC_PER_MSEC;

	return count;
}

/**
 *  inv_fifo_rate_show() - Get the current sampling rate.
 */
static ssize_t inv_fifo_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_iio_s *st = iio_priv(dev_get_drvdata(dev));

	return sprintf(buf, "%d\n", st->chip_config.fifo_rate);
}

/**
 *  inv_power_state_store() - Turn device on/off.
 */
static ssize_t inv_power_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int result;
	u32 power_state;
	struct inv_mpu_iio_s *st = iio_priv(dev_get_drvdata(dev));
	if (kstrtouint(buf, 10, &power_state))
		return -EINVAL;
	if ((!power_state) == st->chip_config.is_asleep)
		return count;
	result = st->set_power_state(st, power_state);

	return count;
}

/**
 *  inv_reg_dump_show() - Register dump for testing.
 */
static ssize_t inv_reg_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ii;
	char data;
	ssize_t bytes_printed = 0;
	struct inv_mpu_iio_s *st = iio_priv(dev_get_drvdata(dev));

	for (ii = 0; ii < st->hw->num_reg; ii++) {
		/* don't read fifo r/w register */
		if (ii == st->reg.fifo_r_w)
			data = 0;
		else
			inv_i2c_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}

	return bytes_printed;
}

int write_be32_key_to_mem(struct inv_mpu_iio_s *st,
					u32 data, int key)
{
	cpu_to_be32s(&data);
	return mem_w_key(key, sizeof(data), (u8 *)&data);
}

/**
 * inv_dmp_attr_store() -  calling this function will store current
 *                        dmp parameter settings
 */
static ssize_t inv_dmp_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;
	mutex_lock(&indio_dev->mlock);
	if (st->chip_config.is_asleep | (!st->chip_config.firmware_loaded) |
				st->chip_config.enable) {
		result = -EINVAL;
		goto dmp_attr_store_fail;
	}
	result = kstrtoint(buf, 10, &data);
	if (result)
		goto dmp_attr_store_fail;
	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		result = write_be32_key_to_mem(st, data, KEY_D_PEDSTD_STEPCTR);
		if (result)
			goto dmp_attr_store_fail;
		break;
	case ATTR_DMP_PEDOMETER_TIME:
		result = write_be32_key_to_mem(st, data, KEY_D_PEDSTD_TIMECTR);
		if (result)
			goto dmp_attr_store_fail;
		break;
	case ATTR_DMP_TAP_THRESHOLD: {
		const char ax[] = {INV_TAP_AXIS_X, INV_TAP_AXIS_Y,
							INV_TAP_AXIS_Z};
		int i;
		if (data < 0 || data > USHRT_MAX) {
			result = -EINVAL;
			goto dmp_attr_store_fail;
		}
		for (i = 0; i < ARRAY_SIZE(ax); i++) {
			result = inv_set_tap_threshold_dmp(st, ax[i], data);
			if (result)
				goto dmp_attr_store_fail;
		}
		st->tap.thresh = data;
		break;
	}
	case ATTR_DMP_TAP_MIN_COUNT:
		if (data < 0 || data > USHRT_MAX) {
			result = -EINVAL;
			goto dmp_attr_store_fail;
		}
		result = inv_set_min_taps_dmp(st, data);
		if (result)
			goto dmp_attr_store_fail;
		st->tap.min_count = data;
		break;
	case ATTR_DMP_TAP_ON:
		result = inv_enable_tap_dmp(st, !!data);
		if (result)
			goto dmp_attr_store_fail;
		st->chip_config.tap_on = !!data;
		break;
	case ATTR_DMP_TAP_TIME:
		if (data < 0 || data > USHRT_MAX) {
			result = -EINVAL;
			goto dmp_attr_store_fail;
		}
		result = inv_set_tap_time_dmp(st, data);
		if (result)
			goto dmp_attr_store_fail;
		st->tap.time = data;
		break;
	case ATTR_DMP_ON:
		st->chip_config.dmp_on = !!data;
		break;
	case ATTR_DMP_INT_ON:
		st->chip_config.dmp_int_on = !!data;
		break;
	case ATTR_DMP_EVENT_INT_ON:
		result = inv_set_interrupt_on_gesture_event(st, !!data);
		if (result)
			goto dmp_attr_store_fail;
		st->chip_config.dmp_event_int_on = !!data;
		break;
	case ATTR_DMP_OUTPUT_RATE:
		if (data <= 0 || data > USHRT_MAX) {
			result = -EINVAL;
			goto dmp_attr_store_fail;
		}
		result = inv_set_fifo_rate(st, data);
		if (result)
			goto dmp_attr_store_fail;
		if (st->chip_config.has_compass) {
			st->compass_dmp_divider = COMPASS_RATE_SCALE * data /
							ONE_K_HZ;
			if (st->compass_dmp_divider > 0)
				st->compass_dmp_divider -= 1;
			st->compass_counter = 0;
		}
		st->chip_config.dmp_output_rate = data;
		break;
	case ATTR_DMP_DISPLAY_ORIENTATION_ON:
		result = inv_set_display_orient_interrupt_dmp(st, !!data);
		if (result)
			goto dmp_attr_store_fail;
		st->chip_config.display_orient_on = !!data;
		break;
	default:
		result = -EINVAL;
		goto dmp_attr_store_fail;
	}
	result = count;

dmp_attr_store_fail:
	mutex_unlock(&indio_dev->mlock);

	return result;
}

/**
 * inv_attr_show() -  calling this function will show current
 *                        dmp parameters.
 */
static ssize_t inv_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_iio_s *st = iio_priv(dev_get_drvdata(dev));
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	s8 d[4];
	int result, data;
	s8 *m;

	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		if (!check_dmp_on(st))
			return -EPERM;
		result = mpu_memory_read(st, st->i2c_addr,
			inv_dmp_get_address(KEY_D_PEDSTD_STEPCTR), 4, d);
		if (result)
			return result;
		data = be32_to_cpup((int *)d);
		return sprintf(buf, "%d\n", data);
	case ATTR_DMP_PEDOMETER_TIME:
		if (!check_dmp_on(st))
			return -EPERM;
		result = mpu_memory_read(st, st->i2c_addr,
			inv_dmp_get_address(KEY_D_PEDSTD_TIMECTR), 4, d);
		if (result)
			return result;
		data = be32_to_cpup((int *)d);
		return sprintf(buf, "%d\n", data * MS_PER_DMP_TICK);
	case ATTR_DMP_TAP_THRESHOLD:
		return sprintf(buf, "%d\n", st->tap.thresh);
	case ATTR_DMP_TAP_MIN_COUNT:
		return sprintf(buf, "%d\n", st->tap.min_count);
	case ATTR_DMP_TAP_ON:
		return sprintf(buf, "%d\n", st->chip_config.tap_on);
	case ATTR_DMP_TAP_TIME:
		return sprintf(buf, "%d\n", st->tap.time);
	case ATTR_DMP_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_on);
	case ATTR_DMP_INT_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_int_on);
	case ATTR_DMP_EVENT_INT_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_event_int_on);
	case ATTR_DMP_OUTPUT_RATE:
		return sprintf(buf, "%d\n", st->chip_config.dmp_output_rate);
	case ATTR_DMP_QUATERNION_ON:
		return sprintf(buf, "%d\n", st->chip_config.quaternion_on);
	case ATTR_DMP_DISPLAY_ORIENTATION_ON:
		return sprintf(buf, "%d\n",
			st->chip_config.display_orient_on);
	case ATTR_LPA_FREQ:{
		const char *f[] = {"1.25", "5", "20", "40"};
		return sprintf(buf, "%s\n", f[st->chip_config.lpa_freq]);
	}
	case ATTR_SELF_TEST:
		if (INV_MPU3050 == st->chip_type)
			result = 1;
		else if (INV_MPU6500 == st->chip_type)
			result = inv_hw_self_test_6500(st);
		else
			result = inv_hw_self_test(st);
		return sprintf(buf, "%d\n", result);
	case ATTR_GYRO_MATRIX:
		m = st->plat_data.orientation;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_ACCL_MATRIX:
		if (st->plat_data.sec_slave_type == SECONDARY_SLAVE_TYPE_ACCEL)
			m = st->plat_data.secondary_orientation;
		else
			m = st->plat_data.orientation;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_COMPASS_MATRIX:
		if (st->plat_data.sec_slave_type ==
				SECONDARY_SLAVE_TYPE_COMPASS)
			m = st->plat_data.secondary_orientation;
		else
			return -ENODEV;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_GYRO_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.gyro_enable);
	case ATTR_ACCL_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.accl_enable);
	case ATTR_COMPASS_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.compass_enable);
	case ATTR_POWER_STATE:
		return sprintf(buf, "%d\n", !st->chip_config.is_asleep);
	case ATTR_FIRMWARE_LOADED:
		return sprintf(buf, "%d\n", st->chip_config.firmware_loaded);
	case ATTR_MOTION_ON:
		return sprintf(buf, "%d\n", st->mot_int.mot_on);
	case ATTR_MOTION_DURATION:
		return sprintf(buf, "%d\n", st->mot_int.mot_dur);
	case ATTR_MOTION_THRESHOLD:
		return sprintf(buf, "%d\n", st->mot_int.mot_thr);
#ifdef CONFIG_INV_TESTING
	case ATTR_REG_WRITE:
		return sprintf(buf, "1\n");
#endif
	default:
		return -EPERM;
	}
}

/**
 * inv_dmp_display_orient_show() -  calling this function will
 *			show orientation This event must use poll.
 */
static ssize_t inv_dmp_display_orient_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_iio_s *st = iio_priv(dev_get_drvdata(dev));
	return sprintf(buf, "%d\n", st->display_orient_data);
}

/**
 * inv_accel_motion_show() -  calling this function showes motion interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_accel_motion_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

/**
 * inv_dmp_tap_show() -  calling this function will show tap
 *                         This event must use poll.
 */
static ssize_t inv_dmp_tap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_iio_s *st = iio_priv(dev_get_drvdata(dev));
	return sprintf(buf, "%d\n", st->tap_data);
}

/**
 *  inv_temperature_show() - Read temperature data directly from registers.
 */
static ssize_t inv_temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inv_mpu_iio_s *st = iio_priv(dev_get_drvdata(dev));
	struct inv_reg_map_s *reg;
	int result;
	short temp;
	long scale_t;
	u8 data[2];
	reg = &st->reg;

	if (st->chip_config.is_asleep)
		return -EPERM;
	result = inv_i2c_read(st, reg->temperature, 2, data);
	if (result) {
		pr_err("Could not read temperature register.\n");
		return result;
	}
	temp = (signed short)(be16_to_cpup((short *)&data[0]));

	if (INV_MPU3050 == st->chip_type)
		scale_t = MPU3050_TEMP_OFFSET +
			inv_q30_mult((int)temp << MPU_TEMP_SHIFT,
				     MPU3050_TEMP_SCALE);
	else
		scale_t = MPU6050_TEMP_OFFSET +
			inv_q30_mult((int)temp << MPU_TEMP_SHIFT,
				     MPU6050_TEMP_SCALE);

	INV_I2C_INC_TEMPREAD(1);

	return sprintf(buf, "%ld %lld\n", scale_t, get_time_ns());
}

/**
 * inv_firmware_loaded() -  calling this function will change
 *                        firmware load
 */
static int inv_firmware_loaded(struct inv_mpu_iio_s *st, int data)
{
	if (data)
		return -EINVAL;
	st->chip_config.firmware_loaded = 0;
	st->chip_config.dmp_on = 0;
	st->chip_config.quaternion_on = 0;

	return 0;
}

/**
 * inv_quaternion_on() -  calling this function will store
 *                                 current quaternion on
 */
static int inv_quaternion_on(struct inv_mpu_iio_s *st,
				 struct iio_buffer *ring, bool en)
{
	st->chip_config.quaternion_on = en;
	if (!en) {
		clear_bit(INV_MPU_SCAN_QUAT_R, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_QUAT_X, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_QUAT_Y, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_QUAT_Z, ring->scan_mask);
	}

	return 0;
}

static int inv_switch_gyro_engine(struct inv_mpu_iio_s *st, bool en)
{
	return inv_switch_engine(st, en, BIT_PWR_GYRO_STBY);
}

static int inv_switch_accl_engine(struct inv_mpu_iio_s *st, bool en)
{
	return inv_switch_engine(st, en, BIT_PWR_ACCL_STBY);
}

/**
 *  inv_gyro_enable() - Enable/disable gyro.
 */
static int inv_gyro_enable(struct inv_mpu_iio_s *st,
				 struct iio_buffer *ring, bool en)
{
	int result;
	if (en == st->chip_config.gyro_enable)
		return 0;
	result = st->switch_gyro_engine(st, en);
	if (result)
		return result;

	if (!en) {
		st->chip_config.gyro_fifo_enable = 0;
		clear_bit(INV_MPU_SCAN_GYRO_X, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_GYRO_Y, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_GYRO_Z, ring->scan_mask);
	}
	st->chip_config.gyro_enable = en;

	return 0;
}

/**
 *  inv_accl_enable() - Enable/disable accl.
 */
static ssize_t inv_accl_enable(struct inv_mpu_iio_s *st,
				 struct iio_buffer *ring, bool en)
{
	int result;
	if (en == st->chip_config.accl_enable)
		return 0;
	result = st->switch_accl_engine(st, en);
	if (result)
		return result;
	st->chip_config.accl_enable = en;
	if (!en) {
		st->chip_config.accl_fifo_enable = 0;
		clear_bit(INV_MPU_SCAN_ACCL_X, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_ACCL_Y, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_ACCL_Z, ring->scan_mask);
	}

	return 0;
}

/**
 * inv_compass_enable() -  calling this function will store compass
 *                         enable
 */
static ssize_t inv_compass_enable(struct inv_mpu_iio_s *st,
				 struct iio_buffer *ring, bool en)
{
	if (en == st->chip_config.compass_enable)
		return 0;
	st->chip_config.compass_enable = en;
	if (!en) {
		st->chip_config.compass_fifo_enable = 0;
		clear_bit(INV_MPU_SCAN_MAGN_X, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_MAGN_Y, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_MAGN_Z, ring->scan_mask);
	}

	return 0;
}

/**
 * inv_attr_store() -  calling this function will store current
 *                        non-dmp parameter settings
 */
static ssize_t inv_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data;
	u8  d;
	int result;

	mutex_lock(&indio_dev->mlock);
	if (check_enable(st)) {
		result = -EINVAL;
		goto attr_store_fail;
	}

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto attr_store_fail;
	switch (this_attr->address) {
	case ATTR_GYRO_ENABLE:
		result = inv_gyro_enable(st, ring, !!data);
		break;
	case ATTR_ACCL_ENABLE:
		result = inv_accl_enable(st, ring, !!data);
		break;
	case ATTR_COMPASS_ENABLE:
		result = inv_compass_enable(st, ring, !!data);
		break;
	case ATTR_DMP_QUATERNION_ON:
		result = inv_quaternion_on(st, ring, !!data);
		break;
	case ATTR_LPA_FREQ:
		result = inv_lpa_freq(st, data);
		break;
	case ATTR_FIRMWARE_LOADED:
		result = inv_firmware_loaded(st, data);
		break;
	case ATTR_MOTION_ON:
		if (INV_MPU6500 == st->chip_type) {
			if (data)
				/* enable and put in MPU6500 mode */
				d = BIT_ACCEL_INTEL_ENABLE
					| BIT_ACCEL_INTEL_MODE;
			else
				d = 0;
			result = inv_i2c_single_write(st,
						REG_6500_ACCEL_INTEL_CTRL, d);
			if (result)
				goto attr_store_fail;
		}
		st->mot_int.mot_on = !!data;
		st->chip_config.lpa_mode = !!data;
		break;
	case ATTR_MOTION_DURATION:
		if (INV_MPU6500 != st->chip_type) {
			result = inv_i2c_single_write(st, REG_ACCEL_MOT_DUR,
					      MPU6050_MOTION_DUR_DEFAULT);
			if (result)
				goto attr_store_fail;
		}
		st->mot_int.mot_dur = data;
		break;
	case ATTR_MOTION_THRESHOLD:
		if ((data > MPU6XXX_MAX_MOTION_THRESH) || (data < 0)) {
			result = -EINVAL;
			goto attr_store_fail;
		}
		d = (u8)(data >> MPU6XXX_MOTION_THRESH_SHIFT);
		data = (d << MPU6XXX_MOTION_THRESH_SHIFT);
		result = inv_i2c_single_write(st, REG_ACCEL_MOT_THR, d);
		if (result)
			goto attr_store_fail;
		st->mot_int.mot_thr = data;
		break;
	default:
		result = -EINVAL;
		goto attr_store_fail;
	};
	if (result)
		goto attr_store_fail;
	result = count;

attr_store_fail:
	mutex_unlock(&indio_dev->mlock);

	return result;
}

#ifdef CONFIG_INV_TESTING
/**
 * inv_reg_write_store() - register write command for testing.
 *                         Format: WSRRDD, where RR is the register in hex,
 *                                         and DD is the data in hex.
 */
static ssize_t inv_reg_write_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	u32 result;
	u8 wreg, wval;
	int temp;
	char local_buf[10];

	if ((buf[0] != 'W' && buf[0] != 'w') ||
	    (buf[1] != 'S' && buf[1] != 's'))
		return -EINVAL;
	if (strlen(buf) < 6)
		return -EINVAL;

	strncpy(local_buf, buf, 7);
	local_buf[6] = 0;
	result = sscanf(&local_buf[4], "%x", &temp);
	if (result == 0)
		return -EINVAL;
	wval = temp;
	local_buf[4] = 0;
	sscanf(&local_buf[2], "%x", &temp);
	if (result == 0)
		return -EINVAL;
	wreg = temp;

	result = inv_i2c_single_write(st, wreg, wval);
	if (result)
		return result;

	return count;
}
#endif /* CONFIG_INV_TESTING */

#define INV_MPU_CHAN(_type, _channel2, _index)                \
	{                                                         \
		.type = _type,                                        \
		.modified = 1,                                        \
		.channel2 = _channel2,                                \
		.info_mask =  (IIO_CHAN_INFO_CALIBBIAS_SEPARATE_BIT | \
				IIO_CHAN_INFO_SCALE_SHARED_BIT),              \
		.scan_index = _index,                                 \
		.scan_type  = IIO_ST('s', 16, 16, 0)                  \
	}

#define INV_ACCL_CHAN(_type, _channel2, _index)                \
	{                                                         \
		.type = _type,                                        \
		.modified = 1,                                        \
		.channel2 = _channel2,                                \
		.info_mask =  (IIO_CHAN_INFO_CALIBBIAS_SEPARATE_BIT | \
				IIO_CHAN_INFO_SCALE_SHARED_BIT |     \
				IIO_CHAN_INFO_OFFSET_SEPARATE_BIT),  \
		.scan_index = _index,                                 \
		.scan_type  = IIO_ST('s', 16, 16, 0)                  \
	}

#define INV_MPU_QUATERNION_CHAN(_channel2, _index)            \
	{                                                         \
		.type = IIO_QUATERNION,                               \
		.modified = 1,                                        \
		.channel2 = _channel2,                                \
		.scan_index = _index,                                 \
		.scan_type  = IIO_ST('s', 32, 32, 0)                  \
	}

#define INV_MPU_MAGN_CHAN(_channel2, _index)                  \
	{                                                         \
		.type = IIO_MAGN,                                     \
		.modified = 1,                                        \
		.channel2 = _channel2,                                \
		.info_mask =  IIO_CHAN_INFO_SCALE_SHARED_BIT,         \
		.scan_index = _index,                                 \
		.scan_type  = IIO_ST('s', 16, 16, 0)                  \
	}

static const struct iio_chan_spec inv_mpu_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(INV_MPU_SCAN_TIMESTAMP),

	INV_MPU_CHAN(IIO_ANGL_VEL, IIO_MOD_X, INV_MPU_SCAN_GYRO_X),
	INV_MPU_CHAN(IIO_ANGL_VEL, IIO_MOD_Y, INV_MPU_SCAN_GYRO_Y),
	INV_MPU_CHAN(IIO_ANGL_VEL, IIO_MOD_Z, INV_MPU_SCAN_GYRO_Z),

	INV_ACCL_CHAN(IIO_ACCEL, IIO_MOD_X, INV_MPU_SCAN_ACCL_X),
	INV_ACCL_CHAN(IIO_ACCEL, IIO_MOD_Y, INV_MPU_SCAN_ACCL_Y),
	INV_ACCL_CHAN(IIO_ACCEL, IIO_MOD_Z, INV_MPU_SCAN_ACCL_Z),

	INV_MPU_QUATERNION_CHAN(IIO_MOD_R, INV_MPU_SCAN_QUAT_R),
	INV_MPU_QUATERNION_CHAN(IIO_MOD_X, INV_MPU_SCAN_QUAT_X),
	INV_MPU_QUATERNION_CHAN(IIO_MOD_Y, INV_MPU_SCAN_QUAT_Y),
	INV_MPU_QUATERNION_CHAN(IIO_MOD_Z, INV_MPU_SCAN_QUAT_Z),

	INV_MPU_MAGN_CHAN(IIO_MOD_X, INV_MPU_SCAN_MAGN_X),
	INV_MPU_MAGN_CHAN(IIO_MOD_Y, INV_MPU_SCAN_MAGN_Y),
	INV_MPU_MAGN_CHAN(IIO_MOD_Z, INV_MPU_SCAN_MAGN_Z),
};

/*constant IIO attribute */
static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("10 20 50 100 200 500");
static IIO_DEV_ATTR_SAMP_FREQ(S_IRUGO | S_IWUSR, inv_fifo_rate_show,
	inv_fifo_rate_store);
static DEVICE_ATTR(temperature, S_IRUGO, inv_temperature_show, NULL);
static IIO_DEVICE_ATTR(power_state, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_power_state_store, ATTR_POWER_STATE);
static IIO_DEVICE_ATTR(firmware_loaded, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_attr_store, ATTR_FIRMWARE_LOADED);
static IIO_DEVICE_ATTR(motion_lpa_freq, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_attr_store, ATTR_LPA_FREQ);
static IIO_DEVICE_ATTR(motion_lpa_on, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_attr_store, ATTR_MOTION_ON);
static IIO_DEVICE_ATTR(motion_lpa_duration, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_attr_store, ATTR_MOTION_DURATION);
static IIO_DEVICE_ATTR(motion_lpa_threshold, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_attr_store, ATTR_MOTION_THRESHOLD);
static DEVICE_ATTR(reg_dump, S_IRUGO, inv_reg_dump_show, NULL);
static IIO_DEVICE_ATTR(self_test, S_IRUGO, inv_attr_show, NULL,
	ATTR_SELF_TEST);
static IIO_DEVICE_ATTR(gyro_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_GYRO_MATRIX);
static IIO_DEVICE_ATTR(accl_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_ACCL_MATRIX);
static IIO_DEVICE_ATTR(compass_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_COMPASS_MATRIX);
static IIO_DEVICE_ATTR(dmp_on, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_ON);
static IIO_DEVICE_ATTR(dmp_int_on, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_INT_ON);
static IIO_DEVICE_ATTR(dmp_event_int_on, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_EVENT_INT_ON);
static IIO_DEVICE_ATTR(dmp_output_rate, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_OUTPUT_RATE);
static IIO_DEVICE_ATTR(quaternion_on, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_attr_store, ATTR_DMP_QUATERNION_ON);
static IIO_DEVICE_ATTR(display_orientation_on, S_IRUGO | S_IWUSR,
	inv_attr_show, inv_dmp_attr_store, ATTR_DMP_DISPLAY_ORIENTATION_ON);
static IIO_DEVICE_ATTR(tap_on, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_TAP_ON);
static IIO_DEVICE_ATTR(tap_time, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_TAP_TIME);
static IIO_DEVICE_ATTR(tap_min_count, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_TAP_MIN_COUNT);
static IIO_DEVICE_ATTR(tap_threshold, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_TAP_THRESHOLD);
static IIO_DEVICE_ATTR(pedometer_time, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_PEDOMETER_TIME);
static IIO_DEVICE_ATTR(pedometer_steps, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_dmp_attr_store, ATTR_DMP_PEDOMETER_STEPS);
static DEVICE_ATTR(event_tap, S_IRUGO, inv_dmp_tap_show, NULL);
static DEVICE_ATTR(event_display_orientation, S_IRUGO,
	inv_dmp_display_orient_show, NULL);
static DEVICE_ATTR(event_accel_motion, S_IRUGO, inv_accel_motion_show, NULL);
static IIO_DEVICE_ATTR(gyro_enable, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_attr_store, ATTR_GYRO_ENABLE);
static IIO_DEVICE_ATTR(accl_enable, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_attr_store, ATTR_ACCL_ENABLE);
static IIO_DEVICE_ATTR(compass_enable, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_attr_store, ATTR_COMPASS_ENABLE);
#ifdef CONFIG_INV_TESTING
static IIO_DEVICE_ATTR(reg_write, S_IRUGO | S_IWUSR, inv_attr_show,
	inv_reg_write_store, ATTR_REG_WRITE);
#endif

static const struct attribute *inv_gyro_attributes[] = {
	&iio_dev_attr_gyro_enable.dev_attr.attr,
	&dev_attr_temperature.attr,
	&iio_dev_attr_power_state.dev_attr.attr,
	&dev_attr_reg_dump.attr,
	&iio_dev_attr_self_test.dev_attr.attr,
	&iio_dev_attr_gyro_matrix.dev_attr.attr,
#ifdef CONFIG_INV_TESTING
	&iio_dev_attr_reg_write.dev_attr.attr,
#endif
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
};

static const struct attribute *inv_mpu6050_attributes[] = {
	&iio_dev_attr_accl_enable.dev_attr.attr,
	&iio_dev_attr_accl_matrix.dev_attr.attr,
	&iio_dev_attr_firmware_loaded.dev_attr.attr,
	&iio_dev_attr_motion_lpa_freq.dev_attr.attr,
	&iio_dev_attr_motion_lpa_on.dev_attr.attr,
	&iio_dev_attr_motion_lpa_duration.dev_attr.attr,
	&iio_dev_attr_motion_lpa_threshold.dev_attr.attr,
	&iio_dev_attr_dmp_on.dev_attr.attr,
	&iio_dev_attr_dmp_int_on.dev_attr.attr,
	&iio_dev_attr_dmp_event_int_on.dev_attr.attr,
	&iio_dev_attr_dmp_output_rate.dev_attr.attr,
	&iio_dev_attr_quaternion_on.dev_attr.attr,
	&iio_dev_attr_display_orientation_on.dev_attr.attr,
	&iio_dev_attr_tap_on.dev_attr.attr,
	&iio_dev_attr_tap_time.dev_attr.attr,
	&iio_dev_attr_tap_min_count.dev_attr.attr,
	&iio_dev_attr_tap_threshold.dev_attr.attr,
	&iio_dev_attr_pedometer_time.dev_attr.attr,
	&iio_dev_attr_pedometer_steps.dev_attr.attr,
	&dev_attr_event_display_orientation.attr,
	&dev_attr_event_tap.attr,
	&dev_attr_event_accel_motion.attr,
};

static const struct attribute *inv_compass_attributes[] = {
	&iio_dev_attr_compass_matrix.dev_attr.attr,
	&iio_dev_attr_compass_enable.dev_attr.attr,
};

static const struct attribute *inv_mpu3050_attributes[] = {
	&iio_dev_attr_accl_matrix.dev_attr.attr,
	&iio_dev_attr_accl_enable.dev_attr.attr,
};

static struct attribute *inv_attributes[ARRAY_SIZE(inv_gyro_attributes) +
				ARRAY_SIZE(inv_mpu6050_attributes) +
				ARRAY_SIZE(inv_compass_attributes) + 1];

static const struct attribute_group inv_attribute_group = {
	.name = "mpu",
	.attrs = inv_attributes
};

static const struct iio_info mpu_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &mpu_read_raw,
	.write_raw = &mpu_write_raw,
	.attrs = &inv_attribute_group,
};

/**
 *  inv_setup_compass() - Configure compass.
 */
static int inv_setup_compass(struct inv_mpu_iio_s *st)
{
	int result;
	u8 data[4];

	result = inv_i2c_read(st, REG_YGOFFS_TC, 1, data);
	if (result)
		return result;
	data[0] &= ~BIT_I2C_MST_VDDIO;
	if (st->plat_data.level_shifter)
		data[0] |= BIT_I2C_MST_VDDIO;
	/*set up VDDIO register */
	result = inv_i2c_single_write(st, REG_YGOFFS_TC, data[0]);
	if (result)
		return result;
	/* set to bypass mode */
	result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
				st->plat_data.int_config | BIT_BYPASS_EN);
	if (result)
		return result;
	/*read secondary i2c ID register */
	result = inv_secondary_read(REG_AKM_ID, 1, data);
	if (result)
		return result;
	if (data[0] != DATA_AKM_ID)
		return -ENXIO;
	/*set AKM to Fuse ROM access mode */
	result = inv_secondary_write(REG_AKM_MODE, DATA_AKM_MODE_FR);
	if (result)
		return result;
	result = inv_secondary_read(REG_AKM_SENSITIVITY, THREE_AXIS,
					st->chip_info.compass_sens);
	if (result)
		return result;
	/*revert to power down mode */
	result = inv_secondary_write(REG_AKM_MODE, DATA_AKM_MODE_PD);
	if (result)
		return result;
	pr_debug("%s senx=%d, seny=%d, senz=%d\n",
		 st->hw->name,
		 st->chip_info.compass_sens[0],
		 st->chip_info.compass_sens[1],
		 st->chip_info.compass_sens[2]);
	/*restore to non-bypass mode */
	result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
			st->plat_data.int_config);
	if (result)
		return result;

	/*setup master mode and master clock and ES bit*/
	result = inv_i2c_single_write(st, REG_I2C_MST_CTRL, BIT_WAIT_FOR_ES);
	if (result)
		return result;
	/* slave 0 is used to read data from compass */
	/*read mode */
	result = inv_i2c_single_write(st, REG_I2C_SLV0_ADDR, BIT_I2C_READ|
		st->plat_data.secondary_i2c_addr);
	if (result)
		return result;
	/* AKM status register address is 2 */
	result = inv_i2c_single_write(st, REG_I2C_SLV0_REG, REG_AKM_STATUS);
	if (result)
		return result;
	/* slave 0 is enabled at the beginning, read 8 bytes from here */
	result = inv_i2c_single_write(st, REG_I2C_SLV0_CTRL, BIT_SLV_EN |
				NUM_BYTES_COMPASS_SLAVE);
	if (result)
		return result;
	/*slave 1 is used for AKM mode change only*/
	result = inv_i2c_single_write(st, REG_I2C_SLV1_ADDR,
		st->plat_data.secondary_i2c_addr);
	if (result)
		return result;
	/* AKM mode register address is 0x0A */
	result = inv_i2c_single_write(st, REG_I2C_SLV1_REG, REG_AKM_MODE);
	if (result)
		return result;
	/* slave 1 is enabled, byte length is 1 */
	result = inv_i2c_single_write(st, REG_I2C_SLV1_CTRL, BIT_SLV_EN | 1);
	if (result)
		return result;
	/* output data for slave 1 is fixed, single measure mode*/
	st->compass_scale = 1;
	if (COMPASS_ID_AK8975 == st->plat_data.sec_slave_id) {
		st->compass_st_upper = AKM8975_ST_Upper;
		st->compass_st_lower = AKM8975_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else if (COMPASS_ID_AK8972 == st->plat_data.sec_slave_id) {
		st->compass_st_upper = AKM8972_ST_Upper;
		st->compass_st_lower = AKM8972_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else if (COMPASS_ID_AK8963 == st->plat_data.sec_slave_id) {
		st->compass_st_upper = AKM8963_ST_Upper;
		st->compass_st_lower = AKM8963_ST_Lower;
		data[0] = DATA_AKM_MODE_SM |
			  (st->compass_scale << AKM8963_SCALE_SHIFT);
	}
	result = inv_i2c_single_write(st, REG_I2C_SLV1_DO, data[0]);
	if (result)
		return result;
	/* slave 0 and 1 timer action is enabled every sample*/
	result = inv_i2c_single_write(st, REG_I2C_MST_DELAY_CTRL,
				BIT_SLV0_DLY_EN | BIT_SLV1_DLY_EN);
	return result;
}

static void inv_setup_func_ptr(struct inv_mpu_iio_s *st)
{
	if (st->chip_type == INV_MPU3050) {
		st->set_power_state    = set_power_mpu3050;
		st->switch_gyro_engine = inv_switch_3050_gyro_engine;
		st->switch_accl_engine = inv_switch_3050_accl_engine;
		st->init_config        = inv_init_config_mpu3050;
		st->setup_reg          = inv_setup_reg_mpu3050;
	} else {
		st->set_power_state    = set_power_itg;
		st->switch_gyro_engine = inv_switch_gyro_engine;
		st->switch_accl_engine = inv_switch_accl_engine;
		st->init_config        = inv_init_config;
		st->setup_reg          = inv_setup_reg;
		/*MPU6XXX special functions */
		st->compass_en         = inv_compass_enable;
		st->quaternion_en      = inv_quaternion_on;
		st->gyro_en            = inv_gyro_enable;
		st->accl_en            = inv_accl_enable;
	}
}

static int inv_detect_6xxx(struct inv_mpu_iio_s *st)
{
	int result;
	u8 d;

	result = inv_i2c_read(st, REG_WHOAMI, 1, &d);
	if (result)
		return result;
	if (d == MPU6500_ID) {
		st->chip_type = INV_MPU6500;
		strcpy(st->name, "mpu6500");
	} else
		strcpy(st->name, "mpu6050");

	return 0;
}

/**
 *  inv_check_chip_type() - check and setup chip type.
 */
static int inv_check_chip_type(struct inv_mpu_iio_s *st,
		const struct i2c_device_id *id)
{
	struct inv_reg_map_s *reg;
	int result;
	int t_ind;

	if (!strcmp(id->name, "itg3500"))
		st->chip_type = INV_ITG3500;
	else if (!strcmp(id->name, "mpu3050"))
		st->chip_type = INV_MPU3050;
	else if (!strcmp(id->name, "mpu6050"))
		st->chip_type = INV_MPU6050;
	else if (!strcmp(id->name, "mpu9150"))
		st->chip_type = INV_MPU6050;
	else if (!strcmp(id->name, "mpu6500"))
		st->chip_type = INV_MPU6500;
	else if (!strcmp(id->name, "mpu9250"))
		st->chip_type = INV_MPU6500;
	else if (!strcmp(id->name, "mpu6xxx"))
		st->chip_type = INV_MPU6050;
	else
		return -EPERM;
	inv_setup_func_ptr(st);
	st->hw = &hw_info[st->chip_type];
	st->mpu_slave = NULL;
	reg = &st->reg;
	st->setup_reg(reg);
	st->chip_config.gyro_enable = 1;
	/* reset to make sure previous state are not there */
	result = inv_i2c_single_write(st, reg->pwr_mgmt_1, BIT_H_RESET);
	if (result)
		return result;
	msleep(POWER_UP_TIME);

	/* turn off and turn on power to ensure gyro engine is on */
	result = st->set_power_state(st, false);
	if (result)
		return result;
	result = st->set_power_state(st, true);
	if (result)
		return result;

	if (!strcmp(id->name, "mpu6xxx")) {
		result = inv_detect_6xxx(st);
		if (result)
			return result;
	}

	switch (st->chip_type) {
	case INV_ITG3500:
		st->num_channels = INV_CHANNEL_NUM_GYRO;
		break;
	case INV_MPU6050:
	case INV_MPU6500:
		if (SECONDARY_SLAVE_TYPE_COMPASS ==
		    st->plat_data.sec_slave_type) {
			st->chip_config.has_compass = 1;
			st->num_channels =
				INV_CHANNEL_NUM_GYRO_ACCL_QUANTERNION_MAGN;
		} else {
			st->chip_config.has_compass = 0;
			st->num_channels =
				INV_CHANNEL_NUM_GYRO_ACCL_QUANTERNION;
		}
		break;
	case INV_MPU3050:
		if (SECONDARY_SLAVE_TYPE_ACCEL ==
		    st->plat_data.sec_slave_type) {
			if (ACCEL_ID_BMA250 == st->plat_data.sec_slave_id)
				inv_register_mpu3050_slave(st);
			st->num_channels = INV_CHANNEL_NUM_GYRO_ACCL;
		} else {
			st->num_channels = INV_CHANNEL_NUM_GYRO;
		}
		break;
	default:
		result = st->set_power_state(st, false);
		return -ENODEV;
	}

	switch (st->chip_type) {
	case INV_MPU6050:
		result = inv_get_silicon_rev_mpu6050(st);
		break;
	case INV_MPU6500:
		result = inv_get_silicon_rev_mpu6500(st);
		break;
	default:
		result = 0;
		break;
	}
	if (result) {
		pr_err("read silicon rev error\n");
		st->set_power_state(st, false);
		return result;
	}
	if (st->chip_config.has_compass) {
		result = inv_setup_compass(st);
		if (result) {
			pr_err("compass setup failed\n");
			st->set_power_state(st, false);
			return result;
		}
	}

	t_ind = 0;
	memcpy(&inv_attributes[t_ind], inv_gyro_attributes,
			sizeof(inv_gyro_attributes));
	t_ind += ARRAY_SIZE(inv_gyro_attributes);

	if (INV_MPU3050 == st->chip_type && st->mpu_slave != NULL) {
		memcpy(&inv_attributes[t_ind], inv_mpu3050_attributes,
				sizeof(inv_mpu3050_attributes));
		t_ind += ARRAY_SIZE(inv_mpu3050_attributes);
		inv_attributes[t_ind] = NULL;
		return 0;
	}

	if ((INV_MPU6050 == st->chip_type) || (INV_MPU6500 == st->chip_type)) {
		memcpy(&inv_attributes[t_ind], inv_mpu6050_attributes,
				sizeof(inv_mpu6050_attributes));
		t_ind += ARRAY_SIZE(inv_mpu6050_attributes);
	}

	if (st->chip_config.has_compass) {
		memcpy(&inv_attributes[t_ind], inv_compass_attributes,
				sizeof(inv_compass_attributes));
		t_ind += ARRAY_SIZE(inv_compass_attributes);
	}
	inv_attributes[t_ind] = NULL;

	return 0;
}

/**
 *  inv_create_dmp_sysfs() - create binary sysfs dmp entry.
 */
static const struct bin_attribute dmp_firmware = {
	.attr = {
		.name = "dmp_firmware",
		.mode = S_IRUGO | S_IWUSR
	},
	.size = 4096,
	.read = inv_dmp_firmware_read,
	.write = inv_dmp_firmware_write,
};

static int inv_create_dmp_sysfs(struct iio_dev *ind)
{
	int result;
	result = sysfs_create_bin_file(&ind->dev.kobj, &dmp_firmware);

	return result;
}

/**
 *  inv_mpu_probe() - probe function.
 */
static int inv_mpu_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct inv_mpu_iio_s *st;
	struct iio_dev *indio_dev;
	int result;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENOSYS;
		pr_err("I2c function error\n");
		goto out_no_free;
	}
	indio_dev = iio_allocate_device(sizeof(*st));
	if (indio_dev == NULL) {
		pr_err("memory allocation failed\n");
		result =  -ENOMEM;
		goto out_no_free;
	}
	st = iio_priv(indio_dev);
	st->client = client;
	st->sl_handle = client->adapter;
	st->i2c_addr = client->addr;
	st->plat_data =
		*(struct mpu_platform_data *)dev_get_platdata(&client->dev);
	/* power is turned on inside check chip type*/
	result = inv_check_chip_type(st, id);
	if (result)
		goto out_free;

	result = st->init_config(indio_dev);
	if (result) {
		dev_err(&client->adapter->dev,
			"Could not initialize device.\n");
		goto out_free;
	}
	result = st->set_power_state(st, false);
	if (result) {
		dev_err(&client->adapter->dev,
			"%s could not be turned off.\n", st->hw->name);
		goto out_free;
	}

	/* Make state variables available to all _show and _store functions. */
	i2c_set_clientdata(client, indio_dev);
	indio_dev->dev.parent = &client->dev;
	if (!strcmp(id->name, "mpu6xxx"))
		indio_dev->name = st->name;
	else
		indio_dev->name = id->name;
	indio_dev->channels = inv_mpu_channels;
	indio_dev->num_channels = st->num_channels;

	indio_dev->info = &mpu_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		pr_err("configure ring buffer fail\n");
		goto out_free;
	}
	result = iio_buffer_register(indio_dev, indio_dev->channels,
					indio_dev->num_channels);
	if (result) {
		pr_err("ring buffer register fail\n");
		goto out_unreg_ring;
	}
	st->irq = client->irq;
	result = inv_mpu_probe_trigger(indio_dev);
	if (result) {
		pr_err("trigger probe fail\n");
		goto out_remove_ring;
	}

	/* Tell the i2c counter, we have an IRQ */
	INV_I2C_SETIRQ(MPU, client->irq);

	result = iio_device_register(indio_dev);
	if (result) {
		pr_err("IIO device register fail\n");
		goto out_remove_trigger;
	}

	if (INV_MPU6050 == st->chip_type ||
	    INV_MPU6500 == st->chip_type) {
		result = inv_create_dmp_sysfs(indio_dev);
		if (result) {
			pr_err("create dmp sysfs failed\n");
			goto out_unreg_iio;
		}
	}

	INIT_KFIFO(st->timestamps);
	spin_lock_init(&st->time_stamp_lock);
	dev_info(&client->adapter->dev, "%s is ready to go!\n",
					indio_dev->name);

	return 0;
out_unreg_iio:
	iio_device_unregister(indio_dev);
out_remove_trigger:
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mpu_remove_trigger(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_mpu_unconfigure_ring(indio_dev);
out_free:
	iio_free_device(indio_dev);
out_no_free:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, result);

	return -EIO;
}

static void inv_mpu_shutdown(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	struct inv_reg_map_s *reg;
	int result;

	reg = &st->reg;
	dev_dbg(&client->adapter->dev, "Shutting down %s...\n", st->hw->name);

	/* reset to make sure previous state are not there */
	result = inv_i2c_single_write(st, reg->pwr_mgmt_1, BIT_H_RESET);
	if (result)
		dev_err(&client->adapter->dev, "Failed to reset %s\n",
			st->hw->name);
	msleep(POWER_UP_TIME);
	/* turn off power to ensure gyro engine is off */
	result = st->set_power_state(st, false);
	if (result)
		dev_err(&client->adapter->dev, "Failed to turn off %s\n",
			st->hw->name);
}

/**
 *  inv_mpu_remove() - remove function.
 */
static int inv_mpu_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	kfifo_free(&st->timestamps);
	iio_device_unregister(indio_dev);
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mpu_remove_trigger(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_mpu_unconfigure_ring(indio_dev);
	iio_free_device(indio_dev);

	dev_info(&client->adapter->dev, "inv-mpu-iio module removed.\n");

	return 0;
}

#ifdef CONFIG_PM
static int inv_mpu_resume(struct device *dev)
{
	struct inv_mpu_iio_s *st =
			iio_priv(i2c_get_clientdata(to_i2c_client(dev)));
	pr_debug("%s inv_mpu_resume\n", st->hw->name);
	return st->set_power_state(st, true);
}

static int inv_mpu_suspend(struct device *dev)
{
	struct inv_mpu_iio_s *st =
			iio_priv(i2c_get_clientdata(to_i2c_client(dev)));
	pr_debug("%s inv_mpu_suspend\n", st->hw->name);
	return st->set_power_state(st, false);
}
static const struct dev_pm_ops inv_mpu_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(inv_mpu_suspend, inv_mpu_resume)
};
#define INV_MPU_PMOPS (&inv_mpu_pmops)
#else
#define INV_MPU_PMOPS NULL
#endif /* CONFIG_PM */

static const u16 normal_i2c[] = { I2C_CLIENT_END };
/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct i2c_device_id inv_mpu_id[] = {
	{"itg3500", INV_ITG3500},
	{"mpu3050", INV_MPU3050},
	{"mpu6050", INV_MPU6050},
	{"mpu9150", INV_MPU9150},
	{"mpu6500", INV_MPU6500},
	{"mpu9250", INV_MPU9250},
	{"mpu6xxx", INV_MPU6XXX},
	{}
};

MODULE_DEVICE_TABLE(i2c, inv_mpu_id);

static struct i2c_driver inv_mpu_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		=	inv_mpu_probe,
	.remove		=	inv_mpu_remove,
	.shutdown	=	inv_mpu_shutdown,
	.id_table	=	inv_mpu_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"inv-mpu-iio",
		.pm     =       INV_MPU_PMOPS,
	},
	.address_list = normal_i2c,
};

static int __init inv_mpu_init(void)
{
	int result = i2c_add_driver(&inv_mpu_driver);
	if (result) {
		pr_err("failed\n");
		return result;
	}
	return 0;
}

static void __exit inv_mpu_exit(void)
{
	i2c_del_driver(&inv_mpu_driver);
}

module_init(inv_mpu_init);
module_exit(inv_mpu_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv-mpu-iio");

/**
 *  @}
 */

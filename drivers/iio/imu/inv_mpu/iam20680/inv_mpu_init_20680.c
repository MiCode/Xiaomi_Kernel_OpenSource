/*
 * Copyright (C) 2017-2018 InvenSense, Inc.
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
#define pr_fmt(fmt) "inv_mpu: " fmt
#include "../inv_mpu_iio.h"

static int inv_calc_gyro_sf(s8 pll)
{
	int a, r;
	int value, t;

	t = 102870L + 81L * pll;
	a = (1L << 30) / t;
	r = (1L << 30) - a * t;
	value = a * 797 * DMP_DIVIDER;
	value += (s64) ((a * 1011387LL * DMP_DIVIDER) >> 20);
	value += r * 797L * DMP_DIVIDER / t;
	value += (s32) ((s64) ((r * 1011387LL * DMP_DIVIDER) >> 20)) / t;
	value <<= 1;

	return value;
}

static int inv_read_timebase(struct inv_mpu_state *st)
{

	inv_plat_single_write(st, REG_CONFIG, 3);

	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_ACCEL].base_time_1k = NSEC_PER_SEC;
	/* talor expansion to calculate base time unit */
	st->eng_info[ENGINE_GYRO].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_GYRO].base_time_1k = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time_1k = NSEC_PER_SEC;

	st->eng_info[ENGINE_ACCEL].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_GYRO].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_I2C].orig_rate = BASE_SAMPLE_RATE;

	st->gyro_sf = inv_calc_gyro_sf(0);

	return 0;
}

int inv_set_gyro_sf(struct inv_mpu_state *st)
{
	int result;

	result = inv_plat_single_write(st, REG_GYRO_CONFIG,
				   st->chip_config.fsr << SHIFT_GYRO_FS_SEL);

	return result;
}

int inv_set_accel_sf(struct inv_mpu_state *st)
{
	int result;

	result = inv_plat_single_write(st, REG_ACCEL_CONFIG,
				st->chip_config.accel_fs << SHIFT_ACCEL_FS);
	return result;
}

// dummy for 20602
int inv_set_accel_intel(struct inv_mpu_state *st)
{
	return 0;
}

static void inv_init_sensor_struct(struct inv_mpu_state *st)
{
	int i;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE;

	st->sensor[SENSOR_ACCEL].sample_size = BYTES_PER_SENSOR;
	st->sensor[SENSOR_TEMP].sample_size = BYTES_FOR_TEMP;
	st->sensor[SENSOR_GYRO].sample_size = BYTES_PER_SENSOR;

	st->sensor_l[SENSOR_L_SIXQ].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_PEDQ].base = SENSOR_GYRO;

	st->sensor_l[SENSOR_L_SIXQ_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].base = SENSOR_GYRO;

	st->sensor[SENSOR_ACCEL].a_en = true;
	st->sensor[SENSOR_GYRO].a_en = false;

	st->sensor[SENSOR_ACCEL].g_en = false;
	st->sensor[SENSOR_GYRO].g_en = true;

	st->sensor[SENSOR_ACCEL].c_en = false;
	st->sensor[SENSOR_GYRO].c_en = false;

	st->sensor[SENSOR_ACCEL].p_en = false;
	st->sensor[SENSOR_GYRO].p_en = false;

	st->sensor[SENSOR_ACCEL].engine_base = ENGINE_ACCEL;
	st->sensor[SENSOR_GYRO].engine_base = ENGINE_GYRO;

	st->sensor_l[SENSOR_L_ACCEL].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GESTURE_ACCEL].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_GYRO_CAL].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_EIS_GYRO].base = SENSOR_GYRO;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO_WAKE].base = SENSOR_GYRO;

	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].base = SENSOR_GYRO;

	st->sensor_l[SENSOR_L_ACCEL].header = ACCEL_HDR;
	st->sensor_l[SENSOR_L_GESTURE_ACCEL].header = ACCEL_HDR;
	st->sensor_l[SENSOR_L_GYRO].header = GYRO_HDR;
	st->sensor_l[SENSOR_L_GYRO_CAL].header = GYRO_CALIB_HDR;

	st->sensor_l[SENSOR_L_EIS_GYRO].header = EIS_GYRO_HDR;
	st->sensor_l[SENSOR_L_SIXQ].header = SIXQUAT_HDR;
	st->sensor_l[SENSOR_L_THREEQ].header = LPQ_HDR;
	st->sensor_l[SENSOR_L_NINEQ].header = NINEQUAT_HDR;
	st->sensor_l[SENSOR_L_PEDQ].header = PEDQUAT_HDR;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].header = ACCEL_WAKE_HDR;
	st->sensor_l[SENSOR_L_GYRO_WAKE].header = GYRO_WAKE_HDR;
	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].header = GYRO_CALIB_WAKE_HDR;
	st->sensor_l[SENSOR_L_MAG_WAKE].header = COMPASS_WAKE_HDR;
	st->sensor_l[SENSOR_L_MAG_CAL_WAKE].header = COMPASS_CALIB_WAKE_HDR;
	st->sensor_l[SENSOR_L_SIXQ_WAKE].header = SIXQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_NINEQ_WAKE].header = NINEQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].header = PEDQUAT_WAKE_HDR;

	st->sensor_l[SENSOR_L_ACCEL].wake_on = false;
	st->sensor_l[SENSOR_L_GYRO].wake_on = false;
	st->sensor_l[SENSOR_L_GYRO_CAL].wake_on = false;
	st->sensor_l[SENSOR_L_MAG].wake_on = false;
	st->sensor_l[SENSOR_L_MAG_CAL].wake_on = false;
	st->sensor_l[SENSOR_L_EIS_GYRO].wake_on = false;
	st->sensor_l[SENSOR_L_SIXQ].wake_on = false;
	st->sensor_l[SENSOR_L_NINEQ].wake_on = false;
	st->sensor_l[SENSOR_L_PEDQ].wake_on = false;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_GYRO_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_MAG_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_SIXQ_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_NINEQ_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].wake_on = true;
}

static int inv_init_config(struct inv_mpu_state *st)
{
	int res, i;

	st->batch.overflow_on = 0;
	st->chip_config.fsr = MPU_INIT_GYRO_SCALE;
	st->chip_config.accel_fs = MPU_INIT_ACCEL_SCALE;
	st->ped.int_thresh = MPU_INIT_PED_INT_THRESH;
	st->ped.step_thresh = MPU_INIT_PED_STEP_THRESH;
	st->chip_config.low_power_gyro_on = 1;
	st->eis.count_precision = NSEC_PER_MSEC;
	st->firmware = 0;
	st->fifo_count_mode = BYTE_MODE;
#ifdef TIMER_BASED_BATCHING
	st->batch_timeout = 0;
	st->is_batch_timer_running = false;
#endif

	st->eng_info[ENGINE_GYRO].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;

	inv_init_sensor_struct(st);
	res = inv_read_timebase(st);
	if (res)
		return res;

	res = inv_set_gyro_sf(st);
	if (res)
		return res;
	res = inv_set_accel_sf(st);
	if (res)
		return res;
	res =  inv_set_accel_intel(st);
	if (res)
		return res;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].ts = 0;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].previous_ts = 0;

	return res;
}

int inv_mpu_initialize(struct inv_mpu_state *st)
{
	u8 v;
	int result;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;

	conf = &st->chip_config;
	plat = &st->plat_data;

	/* verify whoami */
	result = inv_plat_read(st, REG_WHO_AM_I, 1, &v);
	if (result)
		return result;
	pr_info("whoami= %x\n", v);
	if (v == 0x00 || v == 0xff)
		return -ENODEV;

	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		return result;
	usleep_range(REG_UP_TIME_USEC, REG_UP_TIME_USEC);
	msleep(100);
	/* toggle power state */
	result = inv_set_power(st, false);
	if (result)
		return result;
	result = inv_set_power(st, true);
	if (result)
		return result;

	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;
	result = inv_init_config(st);
	if (result)
		return result;

	result = mem_r(MPU_SOFT_REV_ADDR, 1, &v);
	pr_info("sw_rev=%x, res=%d\n", v, result);
	if (result)
		return result;
	st->chip_config.lp_en_mode_off = 0;

	pr_info("%s: Mask %X, v = %X, lp mode = %d\n", __func__,
		MPU_SOFT_REV_MASK, v, st->chip_config.lp_en_mode_off);
	result = inv_set_power(st, false);

	pr_info("%s: initialize result is %d....\n", __func__, result);
	return 0;
}

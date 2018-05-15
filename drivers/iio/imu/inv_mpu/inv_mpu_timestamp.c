/*
 * Copyright (C) 2012-2018 InvenSense, Inc.
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
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/math64.h>

#include "inv_mpu_iio.h"

#define INV_TIME_CALIB_THRESHOLD_1 2

#define MIN_DELAY (3 * NSEC_PER_MSEC)
#define JITTER_THRESH ( 1 * NSEC_PER_MSEC)

int inv_update_dmp_ts(struct inv_mpu_state *st, int ind)
{
	int i;
	u32 counter;
	u64 ts;
	enum INV_ENGINE en_ind;
	struct inv_timestamp_algo *ts_algo = &st->ts_algo;
	u32 base_time;
	u64 cal_period;

	if (st->mode_1k_on)
		cal_period = (NSEC_PER_SEC >> 2);
	else
		cal_period = 2 * NSEC_PER_SEC;

	ts = ts_algo->last_run_time - st->sensor[ind].time_calib;
	counter = st->sensor[ind].sample_calib;
	en_ind = st->sensor[ind].engine_base;
	if (en_ind != ts_algo->clock_base)
		return 0;
	/* we average over 2 seconds period to do the timestamp calculation */
	if (ts < cal_period)
		return 0;
	/* this is the first time we do timestamp averaging, return */
	/* after resume from suspend, the clock of linux has up to 1 seconds
	   drift. We should start from the resume clock instead of using clock
	   before resume */
	if ((!st->sensor[ind].calib_flag) || ts_algo->resume_flag) {
		st->sensor[ind].sample_calib = 0;
		st->sensor[ind].time_calib = ts_algo->last_run_time;
		st->sensor[ind].calib_flag = 1;
		ts_algo->resume_flag = false;

		return 0;
	}
	/* if the sample number in current FIFO is not zero and between now and
		last update time is more than 2 seconds, we do calculation */
	if ((counter > 0) &&
		(ts_algo->last_run_time - st->eng_info[en_ind].last_update_time >
		 cal_period)) {
		/* duration for each sensor */
		st->sensor[ind].dur = (u32) div_u64(ts, counter);
		/* engine duration derived from each sensor */
		if (st->sensor[ind].div)
			st->eng_info[en_ind].dur = st->sensor[ind].dur /
							st->sensor[ind].div;
		else
			pr_err("sensor %d divider zero!\n", ind);
		/* update base time for each sensor */
		if (st->eng_info[en_ind].divider) {
			base_time = (st->eng_info[en_ind].dur /
					st->eng_info[en_ind].divider) *
					st->eng_info[en_ind].orig_rate;
			if (st->mode_1k_on)
				st->eng_info[en_ind].base_time_1k = base_time;
			else
				st->eng_info[en_ind].base_time = base_time;
		} else {
			pr_err("engine %d divider zero!\n", en_ind);
		}

		st->eng_info[en_ind].last_update_time = ts_algo->last_run_time;
		/* update all the sensors duration based on the same engine */
		for (i = 0; i < SENSOR_NUM_MAX; i++) {
			if (st->sensor[i].on &&
			    (st->sensor[i].engine_base == en_ind))
				st->sensor[i].dur = st->sensor[i].div *
				    st->eng_info[en_ind].dur;
		}

	}
	st->sensor[ind].sample_calib = 0;
	st->sensor[ind].time_calib = ts_algo->last_run_time;

	return 0;
}
/**
 *     int inv_get_last_run_time_non_dmp_record_mode(struct inv_mpu_state *st)
 *     This is the function to get last run time in non dmp and record mode.
 *     This function will update the last_run_time, which is important parameter
 *     in overall timestamp algorithm.
 *     return value: this function returns fifo count value.
*/
int inv_get_last_run_time_non_dmp_record_mode(struct inv_mpu_state *st)
{
	long long t_pre, t_post, dur;
	int fifo_count;
#ifndef SENSOR_DATA_FROM_REGISTERS
	int res;
	u8 data[2];
#endif

	t_pre = get_time_ns();
#ifndef SENSOR_DATA_FROM_REGISTERS
	res = inv_plat_read(st, REG_FIFO_COUNT_H, FIFO_COUNT_BYTE, data);
	if (res) {
		pr_info("read REG_FIFO_COUNT_H failed= %d\n", res);
		return 0;
	}
#endif
	t_post = get_time_ns();

#ifdef SENSOR_DATA_FROM_REGISTERS
	if (st->fifo_count_mode == BYTE_MODE)
		fifo_count = st->batch.pk_size;
	else
		fifo_count = 1;
#else
	fifo_count = be16_to_cpup((__be16 *) (data));
#endif
	pr_debug("fifc=%d\n", fifo_count);
	if (!fifo_count)
		return 0;
	if (st->special_mag_mode && (fifo_count == 2)) {
		pr_debug("special trigger\n");
		fifo_count = 1;
	}

	/* In non DMP mode, either gyro or accel duration is the duration
           for each sample */
	if (st->chip_config.gyro_enable)
		dur = st->eng_info[ENGINE_GYRO].dur;
	else
		dur = st->eng_info[ENGINE_ACCEL].dur;

	if (st->fifo_count_mode == BYTE_MODE) {
		fifo_count /= st->batch.pk_size;
	}

	/* In record mode, each number in fifo_count is 1 record or 1 sample */
	st->ts_algo.last_run_time += dur * fifo_count;
	if (st->ts_algo.last_run_time < t_pre)
		st->ts_algo.last_run_time = t_pre;
	if (st->ts_algo.last_run_time > t_post)
		st->ts_algo.last_run_time = t_post;

	return fifo_count;
}

int inv_get_dmp_ts(struct inv_mpu_state *st, int i)
{
	u64 current_time;
	int expected_lower_duration, expected_upper_duration;

	current_time = get_time_ns();

	st->sensor[i].ts += st->sensor[i].dur + st->sensor[i].ts_adj;

	if (st->sensor[i].ts < st->sensor[i].previous_ts)
		st->sensor[i].ts = st->sensor[i].previous_ts + st->sensor[i].dur;

	//hifi sensor limits ts jitter to +/- 2%
	expected_upper_duration = st->eng_info[st->sensor[i].engine_base].divider * 1020000;
	expected_lower_duration = st->eng_info[st->sensor[i].engine_base].divider * 980000;
#if defined(CONFIG_INV_MPU_IIO_ICM20602) || defined(CONFIG_INV_MPU_IIO_ICM20690) || defined(CONFIG_INV_MPU_IIO_IAM20680)
	if (st->sensor[i].ts < st->sensor[i].previous_ts + expected_lower_duration)
		st->sensor[i].ts = st->sensor[i].previous_ts + expected_lower_duration;
	if (st->sensor[i].ts > st->sensor[i].previous_ts + expected_upper_duration)
		st->sensor[i].ts = st->sensor[i].previous_ts + expected_upper_duration;
#endif
	if (st->sensor[i].ts > current_time )
		st->sensor[i].ts = current_time;

	st->sensor[i].previous_ts = st->sensor[i].ts;

	pr_debug("ts=%lld, reset=%lld\n", st->sensor[i].ts, st->ts_algo.reset_ts);
	if (st->sensor[i].ts < st->ts_algo.reset_ts) {
		pr_debug("less than reset\n");
		st->sensor[i].send = false;
	} else {
		st->sensor[i].send = true;
	}

	if (st->header_count == 1)
		inv_update_dmp_ts(st, i);

	return 0;
}

static void process_sensor_bounding(struct inv_mpu_state *st, int i)
{
	s64 elaps_time, thresh1, thresh2;
	struct inv_timestamp_algo *ts_algo = &st->ts_algo;
	u32 dur;

	elaps_time = ((u64) (st->sensor[i].dur)) * st->sensor[i].count;
	thresh1 = ts_algo->last_run_time - elaps_time;

	dur = max(st->sensor[i].dur, (int)MIN_DELAY);
	thresh2 = thresh1 - dur;
	if (thresh1 < 0)
		thresh1 = 0;
	if (thresh2 < 0)
		thresh2 = 0;
	st->sensor[i].ts_adj = 0;
	if ((ts_algo->calib_counter >= INV_TIME_CALIB_THRESHOLD_1) &&
						(!ts_algo->resume_flag)) {
		if (st->sensor[i].ts < thresh2)
			st->sensor[i].ts_adj = thresh2 - st->sensor[i].ts;
	} else if ((ts_algo->calib_counter >=
		INV_TIME_CALIB_THRESHOLD_1) && ts_algo->resume_flag) {
		if (st->sensor[i].ts < thresh2)
			st->sensor[i].ts = ts_algo->last_run_time -
						elaps_time - JITTER_THRESH;
	} else {
		st->sensor[i].ts = ts_algo->last_run_time - elaps_time -
							JITTER_THRESH;
		st->sensor[i].previous_ts = st->sensor[i].ts;
	}

	if (st->sensor[i].ts > thresh1)
		st->sensor[i].ts_adj = thresh1 - st->sensor[i].ts;
	pr_debug("cali=%d\n", st->ts_algo.calib_counter);
	pr_debug("adj= %lld\n", st->sensor[i].ts_adj);
	pr_debug("dur= %d count= %d last= %lld\n", st->sensor[i].dur,
				st->sensor[i].count, ts_algo->last_run_time);
	if (st->sensor[i].ts_adj && (st->sensor[i].count > 1))
		st->sensor[i].ts_adj = div_s64(st->sensor[i].ts_adj,
							st->sensor[i].count);
}
/* inv_bound_timestamp (struct inv_mpu_state *st)
	The purpose this function is to give a generic bound to each
	sensor timestamp. The timestamp cannot exceed current time.
	The timestamp cannot backwards one sample time either, otherwise, there
	would be another sample in between. Using this principle, we can bound
	the sensor samples */
int inv_bound_timestamp(struct inv_mpu_state *st)
{
	int i;
	struct inv_timestamp_algo *ts_algo = &st->ts_algo;

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			if (st->sensor[i].count) {
				process_sensor_bounding(st, i);
			} else if (ts_algo->calib_counter <
				   INV_TIME_CALIB_THRESHOLD_1) {
				st->sensor[i].ts = ts_algo->reset_ts;
				st->sensor[i].previous_ts = st->sensor[i].ts;
			}
		}
	}

	return 0;
}

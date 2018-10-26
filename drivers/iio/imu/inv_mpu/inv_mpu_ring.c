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

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>

#include "inv_mpu_iio.h"

static void inv_push_timestamp(struct iio_dev *indio_dev, u64 t)
{
	u8 buf[IIO_BUFFER_BYTES];
	struct inv_mpu_state *st;

	st = iio_priv(indio_dev);
	if (st->poke_mode_on)
		memcpy(buf, &st->poke_ts, sizeof(t));
	else
		memcpy(buf, &t, sizeof(t));
	iio_push_to_buffers(indio_dev, buf);
}

int inv_push_marker_to_buffer(struct inv_mpu_state *st, u16 hdr, int data)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];

	memcpy(buf, &hdr, sizeof(hdr));
	memcpy(&buf[4], &data, sizeof(data));
	iio_push_to_buffers(indio_dev, buf);

	return 0;
}
static int inv_calc_precision(struct inv_mpu_state *st)
{
	int diff;
	int init;

	if (st->eis.voting_state != 8)
		return 0;
	diff = abs(st->eis.fsync_delay_s[1] - st->eis.fsync_delay_s[0]);
	init = 0;
	if (diff)
		init = st->sensor[SENSOR_GYRO].dur / diff;

	if (abs(init - NSEC_PER_USEC) < (NSEC_PER_USEC >> 3))
		 st->eis.count_precision = init;
	else
		st->eis.voting_state = 0;

	pr_debug("dur= %d prc= %d\n", st->sensor[SENSOR_GYRO].dur,
						st->eis.count_precision);

	return 0;
}

static s64 calc_frame_ave(struct inv_mpu_state *st, int delay)
{
	s64 ts;

	ts = st->eis.current_timestamp - delay;
#if defined(CONFIG_INV_MPU_IIO_ICM20648) | defined(CONFIG_INV_MPU_IIO_ICM20690)
	ts -= st->ts_algo.gyro_ts_shift;
#endif
	pr_debug("shift= %d ts = %lld\n", st->ts_algo.gyro_ts_shift, ts);

	return ts;
}

static void inv_push_eis_ring(struct inv_mpu_state *st, int *q, bool sync,
								s64 t)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	struct inv_eis *eis = &st->eis;
	u8 buf[IIO_BUFFER_BYTES];
	int tmp, ii;

	buf[0] = (EIS_GYRO_HDR & 0xff);
	buf[1] = (EIS_GYRO_HDR >> 8);
	memcpy(buf + 4, &q[0], sizeof(q[0]));
	iio_push_to_buffers(indio_dev, buf);
	for (ii = 0; ii < 2; ii++)
		memcpy(buf + 4 * ii, &q[ii + 1], sizeof(q[ii]));
	iio_push_to_buffers(indio_dev, buf);
	tmp = eis->frame_count;
	if (sync)
		tmp |= 0x80000000;
	memcpy(buf, &tmp, sizeof(tmp));
	iio_push_to_buffers(indio_dev, buf);
	inv_push_timestamp(indio_dev, t);
}
static int inv_do_interpolation_gyro(struct inv_mpu_state *st, int *prev,
	s64 prev_t, int *curr, s64 curr_t, s64 t, bool trigger)
{
	int i;
	int out[3];
#if defined(CONFIG_INV_MPU_IIO_ICM20648) | defined(CONFIG_INV_MPU_IIO_ICM20690)
	prev_t -= st->ts_algo.gyro_ts_shift;
	prev_t += MPU_4X_TS_GYRO_SHIFT;
	curr_t -= st->ts_algo.gyro_ts_shift;
	curr_t += MPU_4X_TS_GYRO_SHIFT;
#endif
	if ((t > prev_t) && (t < curr_t)) {
		for (i = 0; i < 3; i++)
			out[i] = (int)div_s64((s64)(curr[i] - prev[i]) *
				(s64)(t - prev_t), curr_t - prev_t) + prev[i];
	} else if (t < prev_t) {
		for (i = 0; i < 3; i++)
			out[i] = prev[i];
	} else {
		for (i = 0; i < 3; i++)
			out[i] = curr[i];
	}
	pr_debug("prev= %lld t = %lld curr= %lld\n", prev_t, t, curr_t);
	pr_debug("prev = %d, %d, %d\n", prev[0], prev[1], prev[2]);
	pr_debug("curr = %d, %d, %d\n", curr[0], curr[1], curr[2]);
	pr_debug("out = %d, %d, %d\n", out[0], out[1], out[2]);
	inv_push_eis_ring(st, out, trigger, t);

	return 0;
}
#if defined(CONFIG_INV_MPU_IIO_ICM20648) | defined(CONFIG_INV_MPU_IIO_ICM20690)
static void inv_handle_triggered_eis(struct inv_mpu_state *st)
{
	struct inv_eis *eis = &st->eis;
	int delay;

	if (st->eis.eis_frame) {
		inv_calc_precision(st);
		delay = ((int)st->eis.fsync_delay) * st->eis.count_precision;
		eis->fsync_timestamp = calc_frame_ave(st, delay);
		inv_do_interpolation_gyro(st,
			st->eis.prev_gyro,    st->eis.prev_timestamp,
			st->eis.current_gyro, st->eis.current_timestamp,
			eis->fsync_timestamp, true);
		pr_debug("fsync=%lld, curr=%lld, delay=%d\n",
			eis->fsync_timestamp, eis->current_timestamp, delay);
		inv_push_eis_ring(st, st->eis.current_gyro, false,
			st->eis.current_timestamp - st->ts_algo.gyro_ts_shift
						+ MPU_4X_TS_GYRO_SHIFT);
		eis->last_fsync_timestamp = eis->fsync_timestamp;
	} else {
		pr_debug("cur= %lld\n", st->eis.current_timestamp);
		inv_push_eis_ring(st, st->eis.current_gyro, false,
			st->eis.current_timestamp - st->ts_algo.gyro_ts_shift
						+ MPU_4X_TS_GYRO_SHIFT);
	}
}
#else
static void inv_handle_triggered_eis(struct inv_mpu_state *st)
{
	struct inv_eis *eis = &st->eis;
	int delay;

	if ((st->eis.eis_frame && (st->eis.fsync_delay != 5)) ||
		(st->eis.eis_frame && (st->eis.fsync_delay == 5) &&
		(!st->eis.current_sync))
		) {
		inv_calc_precision(st);
		delay = ((int)st->eis.fsync_delay) * st->eis.count_precision;
		eis->fsync_timestamp = calc_frame_ave(st, delay);
		inv_do_interpolation_gyro(st,
			st->eis.prev_gyro,    st->eis.prev_timestamp,
			st->eis.current_gyro, st->eis.current_timestamp,
			eis->fsync_timestamp, true);
		pr_debug("fsync=%lld, curr=%lld, delay=%d\n",
			eis->fsync_timestamp, eis->current_timestamp, delay);
		inv_push_eis_ring(st, st->eis.current_gyro, false,
				st->eis.current_timestamp);
		eis->last_fsync_timestamp = eis->fsync_timestamp;
		st->eis.eis_frame = false;
	} else {
		st->eis.current_sync = false;
		pr_debug("cur= %lld\n", st->eis.current_timestamp);
		inv_push_eis_ring(st, st->eis.current_gyro, false,
				st->eis.current_timestamp);
	}
}
#endif
static void inv_push_eis_buffer(struct inv_mpu_state *st, u64 t, int *q)
{
	int ii;

	if (st->eis.eis_triggered) {
		for (ii = 0; ii < 3; ii++)
			st->eis.prev_gyro[ii] = st->eis.current_gyro[ii];
		st->eis.prev_timestamp = st->eis.current_timestamp;

		for (ii = 0; ii < 3; ii++)
			st->eis.current_gyro[ii] = q[ii];
		st->eis.current_timestamp = t;
		inv_handle_triggered_eis(st);
	} else {
		for (ii = 0; ii < 3; ii++)
			st->eis.current_gyro[ii] = q[ii];
		st->eis.current_timestamp = t;
	}
}
static int inv_push_16bytes_final(struct inv_mpu_state *st, int j,
						s32 *q, u64 t, s16 accur)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int ii;

	memcpy(buf, &st->sensor_l[j].header, sizeof(st->sensor_l[j].header));
	memcpy(buf + 2, &accur, sizeof(accur));
	memcpy(buf + 4, &q[0], sizeof(q[0]));
	iio_push_to_buffers(indio_dev, buf);
	for (ii = 0; ii < 2; ii++)
		memcpy(buf + 4 * ii, &q[ii + 1], sizeof(q[ii]));
	iio_push_to_buffers(indio_dev, buf);
	inv_push_timestamp(indio_dev, t);
	st->sensor_l[j].counter = 0;
	if (st->sensor_l[j].wake_on)
		st->wake_sensor_received = true;

	return 0;
}
int inv_push_16bytes_buffer(struct inv_mpu_state *st, u16 sensor,
				    u64 t, int *q, s16 accur)
{
	int j;

	for (j = 0; j < SENSOR_L_NUM_MAX; j++) {
		if (st->sensor_l[j].on && (st->sensor_l[j].base == sensor)) {
			st->sensor_l[j].counter++;
			if ((st->sensor_l[j].div != 0xffff) &&
				(st->sensor_l[j].counter >=
						st->sensor_l[j].div)) {
				pr_debug(
	"Sensor_l = %d sensor = %d header [%04X] div [%d] ts [%lld] %d %d %d\n",
					j, sensor,
					st->sensor_l[j].header,
					st->sensor_l[j].div,
					t, q[0], q[1], q[2]);
				inv_push_16bytes_final(st, j, q, t, accur);
			}
		}
	}
	return 0;
}

void inv_convert_and_push_16bytes(struct inv_mpu_state *st, u16 hdr,
							u8 *d, u64 t, s8 *m)
{
	int i, j;
	s32 in[3], out[3];

	for (i = 0; i < 3; i++)
		in[i] = be32_to_int(d + i * 4);
	/* multiply with orientation matrix can be optimized like this */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			if (m[i * 3 + j])
				out[i] = in[j] * m[i * 3 + j];

	inv_push_16bytes_buffer(st, hdr, t, out, 0);
}

void inv_convert_and_push_8bytes(struct inv_mpu_state *st, u16 hdr,
						u8 *d, u64 t, s8 *m)
{
	int i, j;
	s16 in[3], out[3];

	for (i = 0; i < 3; i++)
		in[i] = be16_to_cpup((__be16 *) (d + i * 2));

	/* multiply with orientation matrix can be optimized like this */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			if (m[i * 3 + j])
				out[i] = in[j] * m[i * 3 + j];

	inv_push_8bytes_buffer(st, hdr, t, out);
}

int inv_push_special_8bytes_buffer(struct inv_mpu_state *st,
				   u16 hdr, u64 t, s16 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int j;

	memcpy(buf, &hdr, sizeof(hdr));
	memcpy(&buf[2], &d[0], sizeof(d[0]));
	for (j = 0; j < 2; j++)
		memcpy(&buf[4 + j * 2], &d[j + 1], sizeof(d[j]));
	iio_push_to_buffers(indio_dev, buf);
	inv_push_timestamp(indio_dev, t);

	return 0;
}

static int inv_s16_gyro_push(struct inv_mpu_state *st, int i, s16 *raw, u64 t)
{
	if (st->sensor_l[i].on) {
		st->sensor_l[i].counter++;
		if ((st->sensor_l[i].div != 0xffff) &&
			(st->sensor_l[i].counter >= st->sensor_l[i].div)) {
			inv_push_special_8bytes_buffer(st,
					st->sensor_l[i].header, t, raw);
			st->sensor_l[i].counter = 0;
			if (st->sensor_l[i].wake_on)
				st->wake_sensor_received = true;
		}
	}

	return 0;
}

static int inv_s32_gyro_push(struct inv_mpu_state *st, int i, s32 *calib, u64 t)
{
	if (st->sensor_l[i].on) {
		st->sensor_l[i].counter++;
		if ((st->sensor_l[i].div != 0xffff) &&
			(st->sensor_l[i].counter >= st->sensor_l[i].div)) {
			inv_push_16bytes_final(st, i, calib, t, 0);
			st->sensor_l[i].counter = 0;
			if (st->sensor_l[i].wake_on)
				st->wake_sensor_received = true;
		}
	}

	return 0;
}

int inv_push_gyro_data(struct inv_mpu_state *st, s16 *raw, s32 *calib, u64 t)
{
	int gyro_data[] = {SENSOR_L_GYRO, SENSOR_L_GYRO_WAKE};
	int calib_data[] = {SENSOR_L_GYRO_CAL, SENSOR_L_GYRO_CAL_WAKE};
	int i;

	if (st->sensor_l[SENSOR_L_EIS_GYRO].on)
		inv_push_eis_buffer(st, t, calib);

	for (i = 0; i < 2; i++)
		inv_s16_gyro_push(st, gyro_data[i], raw, t);
	for (i = 0; i < 2; i++)
		inv_s32_gyro_push(st, calib_data[i], calib, t);

	return 0;
}
int inv_push_8bytes_buffer(struct inv_mpu_state *st, u16 sensor, u64 t, s16 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int ii, j;

	if ((sensor == STEP_DETECTOR_HDR) ||
					(sensor == STEP_DETECTOR_WAKE_HDR)) {
		memcpy(buf, &sensor, sizeof(sensor));
		memcpy(&buf[2], &d[0], sizeof(d[0]));
		for (j = 0; j < 2; j++)
			memcpy(&buf[4 + j * 2], &d[j + 1], sizeof(d[j]));
		iio_push_to_buffers(indio_dev, buf);
		inv_push_timestamp(indio_dev, t);
		if (sensor == STEP_DETECTOR_WAKE_HDR)
			st->wake_sensor_received = true;
		return 0;
	}
	for (ii = 0; ii < SENSOR_L_NUM_MAX; ii++) {
		if (st->sensor_l[ii].on &&
		    (st->sensor_l[ii].base == sensor) &&
		    (st->sensor_l[ii].div != 0xffff)) {
			st->sensor_l[ii].counter++;
			if (st->sensor_l[ii].counter >= st->sensor_l[ii].div) {
				pr_debug(
	"Sensor_l = %d sensor = %d header [%04X] div [%d] ts [%lld] %d %d %d\n",
	ii, sensor, st->sensor_l[ii].header,
	st->sensor_l[ii].div, t, d[0], d[1], d[2]);

				memcpy(buf, &st->sensor_l[ii].header,
				       sizeof(st->sensor_l[ii].header));
				memcpy(&buf[2], &d[0], sizeof(d[0]));
				for (j = 0; j < 2; j++)
					memcpy(&buf[4 + j * 2], &d[j + 1],
					       sizeof(d[j]));

				iio_push_to_buffers(indio_dev, buf);
				inv_push_timestamp(indio_dev, t);
				st->sensor_l[ii].counter = 0;
				if (st->sensor_l[ii].wake_on)
					st->wake_sensor_received = true;
			}
		}
	}

	return 0;
}
#ifdef CONFIG_INV_MPU_IIO_ICM20648
/* Implemented activity to string function for BAC test */
#define TILT_DETECTED  0x1000
#define NONE 0x00
#define DRIVE 0x01
#define WALK 0x02
#define RUN 0x04
#define BIKE 0x08
#define TILT 0x10
#define STILL 0x20
#define DRIVE_WALK (DRIVE | WALK)
#define DRIVE_RUN (DRIVE | RUN)

char *act_string(s16 data)
{
	data &= (~TILT);
	switch (data) {
	case NONE:
		return "None";
	case DRIVE:
		return "Drive";
	case WALK:
		return "Walk";
	case RUN:
		return "Run";
	case BIKE:
		return "Bike";
	case STILL:
		return "Still";
	case DRIVE_WALK:
		return "drive and walk";
	case DRIVE_RUN:
		return "drive and run";
	default:
		return "Unknown";
	}
	return "Unknown";
}

char *inv_tilt_check(s16 data)
{
	if (data & TILT)
		return "Tilt";
	else
		return "None";
}

int inv_push_8bytes_kf(struct inv_mpu_state *st, u16 hdr, u64 t, s16 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int i;

	if (st->chip_config.activity_on) {
		memcpy(buf, &hdr, sizeof(hdr));
		for (i = 0; i < 3; i++)
			memcpy(&buf[2 + i * 2], &d[i], sizeof(d[i]));

		kfifo_in(&st->kf, buf, IIO_BUFFER_BYTES);
		memcpy(buf, &t, sizeof(t));
		kfifo_in(&st->kf, buf, IIO_BUFFER_BYTES);
		st->activity_size += IIO_BUFFER_BYTES * 2;
	}
	if (st->chip_config.tilt_enable) {
		pr_debug("d[0] = %04X,  [%X : %s] to [%X : %s]",
		d[0], d[0] & 0x00FF,
		inv_tilt_check(d[0] & 0x00FF),
		(d[0] & 0xFF00) >> 8,  inv_tilt_check((d[0] & 0xFF00) >> 8));
		sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_tilt");
	}

	pr_debug("d[0] = %04X,  [%X : %s] to [%X : %s]", d[0], d[0] & 0x00FF,
		act_string(d[0] & 0x00FF),
		(d[0] & 0xFF00) >> 8,  act_string((d[0] & 0xFF00) >> 8));

	read_be32_from_mem(st, &st->bac_drive_conf, BAC_DRIVE_CONFIDENCE);
	read_be32_from_mem(st, &st->bac_walk_conf, BAC_WALK_CONFIDENCE);
	read_be32_from_mem(st, &st->bac_smd_conf, BAC_SMD_CONFIDENCE);
	read_be32_from_mem(st, &st->bac_bike_conf, BAC_BIKE_CONFIDENCE);
	read_be32_from_mem(st, &st->bac_still_conf, BAC_STILL_CONFIDENCE);
	read_be32_from_mem(st, &st->bac_run_conf, BAC_RUN_CONFIDENCE);

	return 0;
}
#endif

int inv_send_steps(struct inv_mpu_state *st, int step, u64 ts)
{
	s16 s[3];

	s[0] = 0;
	s[1] = (s16) (step & 0xffff);
	s[2] = (s16) ((step >> 16) & 0xffff);
	if (st->step_counter_l_on)
		inv_push_special_8bytes_buffer(st, STEP_COUNTER_HDR, ts, s);
	if (st->step_counter_wake_l_on) {
		inv_push_special_8bytes_buffer(st, STEP_COUNTER_WAKE_HDR,
					       ts, s);
		st->wake_sensor_received = true;
	}
	return 0;
}

void inv_push_step_indicator(struct inv_mpu_state *st, u64 t)
{
	s16 sen[3];
#define STEP_INDICATOR_HEADER 0x0001

	sen[0] = 0;
	sen[1] = 0;
	sen[2] = 0;
	inv_push_8bytes_buffer(st, STEP_INDICATOR_HEADER, t, sen);
}

/*
 *  inv_irq_handler() - Cache a timestamp at each data ready interrupt.
 */
static irqreturn_t inv_irq_handler(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

#ifdef TIMER_BASED_BATCHING
static enum hrtimer_restart inv_batch_timer_handler(struct hrtimer *timer)
{
	struct inv_mpu_state *st =
		container_of(timer, struct inv_mpu_state, hr_batch_timer);

	if (st->chip_config.gyro_enable || st->chip_config.accel_enable) {
		hrtimer_forward_now(&st->hr_batch_timer,
			ns_to_ktime(st->batch_timeout));
		schedule_work(&st->batch_work);
		return HRTIMER_RESTART;
	}
	st->is_batch_timer_running = 0;
	return HRTIMER_NORESTART;
}
#endif

void inv_mpu_unconfigure_ring(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
#ifdef KERNEL_VERSION_4_X
	devm_free_irq(st->dev, st->irq, st);
	devm_iio_kfifo_free(st->dev, indio_dev->buffer);
#else
	free_irq(st->irq, st);
	iio_kfifo_free(indio_dev->buffer);
#endif
};
EXPORT_SYMBOL_GPL(inv_mpu_unconfigure_ring);

#ifndef KERNEL_VERSION_4_X
static int inv_predisable(struct iio_dev *indio_dev)
{
	return 0;
}

static int inv_preenable(struct iio_dev *indio_dev)
{
	return 0;
}

static const struct iio_buffer_setup_ops inv_mpu_ring_setup_ops = {
	.preenable = &inv_preenable,
	.predisable = &inv_predisable,
};
#endif

int inv_mpu_configure_ring(struct iio_dev *indio_dev)
{
	int ret;
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_buffer *ring;

#ifdef TIMER_BASED_BATCHING
	/* configure hrtimer */
	hrtimer_init(&st->hr_batch_timer, CLOCK_BOOTTIME, HRTIMER_MODE_REL);
	st->hr_batch_timer.function = inv_batch_timer_handler;
	INIT_WORK(&st->batch_work, inv_batch_work);
#endif
#ifdef KERNEL_VERSION_4_X
	ring = devm_iio_kfifo_allocate(st->dev);
	if (!ring)
		return -ENOMEM;
	ring->scan_timestamp = true;
	iio_device_attach_buffer(indio_dev, ring);
	ret = devm_request_threaded_irq(st->dev,
		st->irq,
		inv_irq_handler,
		inv_read_fifo,
		IRQF_TRIGGER_RISING | IRQF_SHARED,
		"inv_irq",
		st);
	if (ret) {
		devm_iio_kfifo_free(st->dev, ring);
		return ret;
	}

	// this mode does not use ops
	indio_dev->modes = INDIO_ALL_BUFFER_MODES;

	return ret;
#else
	ring = iio_kfifo_allocate(indio_dev);
	if (!ring)
		return -ENOMEM;
	indio_dev->buffer = ring;
	/* setup ring buffer */
	ring->scan_timestamp = true;
	indio_dev->setup_ops = &inv_mpu_ring_setup_ops;
	ret = request_threaded_irq(st->irq,
			inv_irq_handler,
			inv_read_fifo,
			IRQF_TRIGGER_RISING | IRQF_SHARED,
			"inv_irq",
			st);
	if (ret)
		goto error_iio_sw_rb_free;

	indio_dev->modes |= INDIO_BUFFER_HARDWARE;

	return 0;
error_iio_sw_rb_free:
	iio_kfifo_free(indio_dev->buffer);

	return ret;
#endif
}
EXPORT_SYMBOL_GPL(inv_mpu_configure_ring);

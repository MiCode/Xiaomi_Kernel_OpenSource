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
 *      @file    inv_yas53x_core.c
 *      @brief   Invensense implementation for yas530/yas532/yas533.
 *      @details This driver currently works for yas530/yas532/yas533.
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

#include "inv_yas53x_iio.h"
#include "sysfs.h"
#include "inv_test/inv_counters.h"

/* -------------------------------------------------------------------------- */
static int Cx, Cy1, Cy2;
static int /*a1, */ a2, a3, a4, a5, a6, a7, a8, a9;
static int k;

static u8 dx, dy1, dy2;
static u8 d2, d3, d4, d5, d6, d7, d8, d9, d0;
static u8 dck, ver;

/**
 *  inv_serial_read() - Read one or more bytes from the device registers.
 *  @st:	Device driver instance.
 *  @reg:	First device register to be read from.
 *  @length:	Number of bytes to read.
 *  @data:	Data read from device.
 *  NOTE: The slave register will not increment when reading from the FIFO.
 */
int inv_serial_read(struct inv_compass_state *st, u8 reg, u16 length, u8 *data)
{
	int result;
	INV_I2C_INC_COMPASSWRITE(3);
	INV_I2C_INC_COMPASSREAD(length);
	result = i2c_smbus_read_i2c_block_data(st->client, reg, length, data);
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
int inv_serial_single_write(struct inv_compass_state *st, u8 reg, u8 data)
{
	u8 d[1];
	d[0] = data;
	INV_I2C_INC_COMPASSWRITE(3);

	return i2c_smbus_write_i2c_block_data(st->client, reg, 1, d);
}

static int set_hardware_offset(struct inv_compass_state *st,
			char offset_x, char offset_y1, char offset_y2)
{
	char data;
	int result = 0;

	data = offset_x & 0x3f;
	result = inv_serial_single_write(st, YAS530_REGADDR_OFFSET_X, data);
	if (result)
		return result;

	data = offset_y1 & 0x3f;
	result = inv_serial_single_write(st, YAS530_REGADDR_OFFSET_Y1, data);
	if (result)
		return result;

	data = offset_y2 & 0x3f;
	result = inv_serial_single_write(st, YAS530_REGADDR_OFFSET_Y2, data);
	return result;
}

static int set_measure_command(struct inv_compass_state *st)
{
	int result = 0;
	result = inv_serial_single_write(st,
					 YAS530_REGADDR_MEASURE_COMMAND, 0x01);
	return result;
}

static int measure_normal(struct inv_compass_state *st,
			  int *busy, unsigned short *t,
			  unsigned short *x, unsigned short *y1,
			  unsigned short *y2)
{
	int result;
	ktime_t sleeptime;
	result = set_measure_command(st);
	sleeptime = ktime_set(0, 2 * NSEC_PER_MSEC);
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_hrtimeout(&sleeptime, HRTIMER_MODE_REL);

	result = st->read_data(st, busy, t, x, y1, y2);

	return result;
}

static int measure_int(struct inv_compass_state *st,
			  int *busy, unsigned short *t,
			  unsigned short *x, unsigned short *y1,
			  unsigned short *y2)
{
	int result;
	if (st->first_read_after_reset) {
		st->first_read_after_reset = 0;
		result = 1;
	} else {
		result = st->read_data(st, busy, t, x, y1, y2);
	}
	result |= set_measure_command(st);

	return result;
}

static int yas530_read_data(struct inv_compass_state *st,
			  int *busy, u16 *t, u16 *x, u16 *y1, u16 *y2)
{
	u8 data[8];
	u16 b, to, xo, y1o, y2o;
	int result;

	result = inv_serial_read(st,
				 YAS530_REGADDR_MEASURE_DATA, 8, data);
	if (result)
		return result;

	b = (data[0] >> 7) & 0x01;
	to = (s16)(((data[0] << 2) & 0x1fc) | ((data[1] >> 6) & 0x03));
	xo = (s16)(((data[2] << 5) & 0xfe0) | ((data[3] >> 3) & 0x1f));
	y1o = (s16)(((data[4] << 5) & 0xfe0) | ((data[5] >> 3) & 0x1f));
	y2o = (s16)(((data[6] << 5) & 0xfe0) | ((data[7] >> 3) & 0x1f));

	*busy = b;
	*t = to;
	*x = xo;
	*y1 = y1o;
	*y2 = y2o;

	return 0;
}

static int yas532_533_read_data(struct inv_compass_state *st,
			  int *busy, u16 *t, u16 *x, u16 *y1, u16 *y2)
{
	u8 data[8];
	u16 b, to, xo, y1o, y2o;
	int result;

	result = inv_serial_read(st,
				 YAS530_REGADDR_MEASURE_DATA, 8, data);
	if (result)
		return result;

	b = (data[0] >> 7) & 0x01;
	to = (s16)((((s32)data[0] << 3) & 0x3f8) | ((data[1] >> 5) & 0x07));
	xo = (s16)((((s32)data[2] << 6) & 0x1fc0) | ((data[3] >> 2) & 0x3f));
	y1o = (s16)((((s32)data[4] << 6) & 0x1fc0) | ((data[5] >> 2) & 0x3f));
	y2o = (s16)((((s32)data[6] << 6) & 0x1fc0) | ((data[7] >> 2) & 0x3f));

	*busy = b;
	*t = to;
	*x = xo;
	*y1 = y1o;
	*y2 = y2o;

	return 0;
}

static int check_offset(struct inv_compass_state *st,
			char offset_x, char offset_y1, char offset_y2,
			int *flag_x, int *flag_y1, int *flag_y2)
{
	int result;
	int busy;
	short t, x, y1, y2;

	result = set_hardware_offset(st, offset_x, offset_y1, offset_y2);
	if (result)
		return result;
	result = measure_normal(st, &busy, &t, &x, &y1, &y2);
	if (result)
		return result;
	*flag_x = 0;
	*flag_y1 = 0;
	*flag_y2 = 0;

	if (x > st->center)
		*flag_x = 1;
	if (y1 > st->center)
		*flag_y1 = 1;
	if (y2 > st->center)
		*flag_y2 = 1;
	if (x < st->center)
		*flag_x = -1;
	if (y1 < st->center)
		*flag_y1 = -1;
	if (y2 < st->center)
		*flag_y2 = -1;

	return result;
}

static int measure_and_set_offset(struct inv_compass_state *st,
				  char *offset)
{
	int i;
	int result = 0;
	char offset_x = 0, offset_y1 = 0, offset_y2 = 0;
	int flag_x = 0, flag_y1 = 0, flag_y2 = 0;
	static const int correct[5] = {16, 8, 4, 2, 1};

	for (i = 0; i < 5; i++) {
		result = check_offset(st,
				      offset_x, offset_y1, offset_y2,
				      &flag_x, &flag_y1, &flag_y2);
		if (result)
			return result;
		if (flag_x)
			offset_x += flag_x * correct[i];
		if (flag_y1)
			offset_y1 += flag_y1 * correct[i];
		if (flag_y2)
			offset_y2 += flag_y2 * correct[i];
	}

	result = set_hardware_offset(st, offset_x, offset_y1, offset_y2);
	if (result)
		return result;
	offset[0] = offset_x;
	offset[1] = offset_y1;
	offset[2] = offset_y2;

	return result;
}

static void coordinate_conversion(short x, short y1, short y2, short t,
				  int *xo, int *yo, int *zo)
{
	int sx, sy1, sy2, sy, sz;
	int hx, hy, hz;

	sx = x - (Cx * t) / 100;
	sy1 = y1 - (Cy1 * t) / 100;
	sy2 = y2 - (Cy2 * t) / 100;

	sy = sy1 - sy2;
	sz = -sy1 - sy2;

	hx = k * ((100 * sx + a2 * sy + a3 * sz) / 10);
	hy = k * ((a4 * sx + a5 * sy + a6 * sz) / 10);
	hz = k * ((a7 * sx + a8 * sy + a9 * sz) / 10);

	*xo = hx;
	*yo = hy;
	*zo = hz;
}

static int get_cal_data_yas532_533(struct inv_compass_state *st)
{
	u8 data[YAS_YAS532_533_CAL_DATA_SIZE];
	int result;

	result = inv_serial_read(st, YAS530_REGADDR_CAL,
				YAS_YAS532_533_CAL_DATA_SIZE, data);
	if (result)
		return result;
	/* CAL data Second Read */
	result = inv_serial_read(st, YAS530_REGADDR_CAL,
				YAS_YAS532_533_CAL_DATA_SIZE, data);
	if (result)
		return result;

	dx = data[0];
	dy1 = data[1];
	dy2 = data[2];
	d2 = (data[3] >> 2) & 0x03f;
	d3 = (u8)(((data[3] << 2) & 0x0c) | ((data[4] >> 6) & 0x03));
	d4 = (u8)(data[4] & 0x3f);
	d5 = (data[5] >> 2) & 0x3f;
	d6 = (u8)(((data[5] << 4) & 0x30) | ((data[6] >> 4) & 0x0f));
	d7 = (u8)(((data[6] << 3) & 0x78) | ((data[7] >> 5) & 0x07));
	d8 = (u8)(((data[7] << 1) & 0x3e) | ((data[8] >> 7) & 0x01));
	d9 = (u8)(((data[8] << 1) & 0xfe) | ((data[9] >> 7) & 0x01));
	d0 = (u8)((data[9] >> 2) & 0x1f);
	dck = (u8)(((data[9] << 1) & 0x06) | ((data[10] >> 7) & 0x01));
	ver = (u8)((data[13]) & 0x01);

	Cx  = dx * 10 - 1280;
	Cy1 = dy1 * 10 - 1280;
	Cy2 = dy2 * 10 - 1280;
	a2  = d2 - 32;
	a3  = d3 - 8;
	a4  = d4 - 32;
	a5  = d5 + 38;
	a6  = d6 - 32;
	a7  = d7 - 64;
	a8  = d8 - 32;
	a9  = d9;
	k   = d0;

	return 0;
}

static int get_cal_data_yas530(struct inv_compass_state *st)
{
	u8 data[YAS_YAS530_CAL_DATA_SIZE];
	int result;
	/* CAL data read */
	result = inv_serial_read(st, YAS530_REGADDR_CAL,
				YAS_YAS530_CAL_DATA_SIZE, data);
	if (result)
		return result;
	/* CAL data Second Read */
	result = inv_serial_read(st, YAS530_REGADDR_CAL,
				YAS_YAS530_CAL_DATA_SIZE, data);
	if (result)
		return result;
	/*Cal data */
	dx = data[0];
	dy1 = data[1];
	dy2 = data[2];
	d2 = (data[3] >> 2) & 0x03f;
	d3 = ((data[3] << 2) & 0x0c) | ((data[4] >> 6) & 0x03);
	d4 = data[4] & 0x3f;
	d5 = (data[5] >> 2) & 0x3f;
	d6 = ((data[5] << 4) & 0x30) | ((data[6] >> 4) & 0x0f);
	d7 = ((data[6] << 3) & 0x78) | ((data[7] >> 5) & 0x07);
	d8 = ((data[7] << 1) & 0x3e) | ((data[8] >> 7) & 0x01);
	d9 = ((data[8] << 1) & 0xfe) | ((data[9] >> 7) & 0x01);
	d0 = (data[9] >> 2) & 0x1f;
	dck = ((data[9] << 1) & 0x06) | ((data[10] >> 7) & 0x01);
	ver = (u8)((data[15]) & 0x03);

	/*Correction Data */
	Cx = (int)dx * 6 - 768;
	Cy1 = (int)dy1 * 6 - 768;
	Cy2 = (int)dy2 * 6 - 768;
	a2 = (int)d2 - 32;
	a3 = (int)d3 - 8;
	a4 = (int)d4 - 32;
	a5 = (int)d5 + 38;
	a6 = (int)d6 - 32;
	a7 = (int)d7 - 64;
	a8 = (int)d8 - 32;
	a9 = (int)d9;
	k = (int)d0 + 10;

	return 0;
}


static void thresh_filter_init(struct yas_thresh_filter *thresh_filter,
				int threshold)
{
	thresh_filter->threshold = threshold;
	thresh_filter->last = 0;
}

static void
adaptive_filter_init(struct yas_adaptive_filter *adap_filter, int len,
		int noise)
{
	int i;

	adap_filter->num = 0;
	adap_filter->index = 0;
	adap_filter->filter_noise = noise;
	adap_filter->filter_len = len;

	for (i = 0; i < adap_filter->filter_len; ++i)
		adap_filter->sequence[i] = 0;
}

static void yas_init_adap_filter(struct inv_compass_state *st)
{
	struct yas_filter *f;
	int i;
	int noise[] = {YAS_MAG_DEFAULT_FILTER_NOISE_X,
			YAS_MAG_DEFAULT_FILTER_NOISE_Y,
			YAS_MAG_DEFAULT_FILTER_NOISE_Z};

	f = &st->filter;
	f->filter_len = YAS_MAG_DEFAULT_FILTER_LEN;
	for (i = 0; i < 3; i++)
		f->filter_noise[i] = noise[i];

	for (i = 0; i < 3; i++) {
		adaptive_filter_init(&f->adap_filter[i], f->filter_len,
				f->filter_noise[i]);
		thresh_filter_init(&f->thresh_filter[i], f->filter_thresh);
	}
}

int yas53x_resume(struct inv_compass_state *st)
{
	int result = 0;

	unsigned char dummyData = 0x00;
	unsigned char read_reg[1];

	/* =============================================== */

	/* Step 1 - Test register initialization */
	dummyData = 0x00;
	result = inv_serial_single_write(st,
					 YAS530_REGADDR_TEST1, dummyData);
	if (result)
		return result;
	result =
	    inv_serial_single_write(st,
				    YAS530_REGADDR_TEST2, dummyData);
	if (result)
		return result;
	/* Device ID read  */
	result = inv_serial_read(st,
				 YAS530_REGADDR_DEVICE_ID, 1, read_reg);

	/*Step 2 Read the CAL register */
	st->get_cal_data(st);

	/*Obtain the [49:47] bits */
	dck &= 0x07;

	/*Step 3 : Storing the CONFIG with the CLK value */
	dummyData = 0x00 | (dck << 2);
	result = inv_serial_single_write(st,
					 YAS530_REGADDR_CONFIG, dummyData);
	if (result)
		return result;
	/*Step 4 : Set Acquisition Interval Register */
	dummyData = 0x00;
	result = inv_serial_single_write(st,
					 YAS530_REGADDR_MEASURE_INTERVAL,
					 dummyData);
	if (result)
		return result;

	/*Step 5 : Reset Coil */
	dummyData = 0x00;
	result = inv_serial_single_write(st,
					 YAS530_REGADDR_ACTUATE_INIT_COIL,
					 dummyData);
	if (result)
		return result;
	/* Offset Measurement and Set */
	result = measure_and_set_offset(st, st->offset);
	if (result)
		return result;
	st->first_measure_after_reset = 1;
	st->first_read_after_reset = 1;
	st->reset_timer = 0;

	yas_init_adap_filter(st);

	return result;
}

static int inv_check_range(struct inv_compass_state *st, s16 x, s16 y1, s16 y2)
{
	int result = 0;

	if (x == 0)
		result |= 0x01;
	if (x == st->overflow_bound)
		result |= 0x02;
	if (y1 == 0)
		result |= 0x04;
	if (y1 == st->overflow_bound)
		result |= 0x08;
	if (y2 == 0)
		result |= 0x10;
	if (y2 == st->overflow_bound)
		result |= 0x20;

	return result;
}
static int square(int data)
{
	return data * data;
}

static int
adaptive_filter_filter(struct yas_adaptive_filter *adap_filter, int in)
{
	int avg, sum;
	int i;

	if (adap_filter->filter_len == 0)
		return in;
	if (adap_filter->num < adap_filter->filter_len) {
		adap_filter->sequence[adap_filter->index++] = in / 100;
		adap_filter->num++;
		return in;
	}
	if (adap_filter->filter_len <= adap_filter->index)
		adap_filter->index = 0;
	adap_filter->sequence[adap_filter->index++] = in / 100;

	avg = 0;
	for (i = 0; i < adap_filter->filter_len; i++)
		avg += adap_filter->sequence[i];
	avg /= adap_filter->filter_len;

	sum = 0;
	for (i = 0; i < adap_filter->filter_len; i++)
		sum += square(avg - adap_filter->sequence[i]);
	sum /= adap_filter->filter_len;

	if (sum <= adap_filter->filter_noise)
		return avg * 100;

	return ((in/100 - avg) * (sum - adap_filter->filter_noise) / sum + avg)
		* 100;
}

static int
thresh_filter_filter(struct yas_thresh_filter *thresh_filter, int in)
{
	if (in < thresh_filter->last - thresh_filter->threshold
			|| thresh_filter->last
			+ thresh_filter->threshold < in) {
		thresh_filter->last = in;
		return in;
	} else {
		return thresh_filter->last;
	}
}

static void
filter_filter(struct yas_filter *d, int *orig, int *filtered)
{
	int i;

	for (i = 0; i < 3; i++) {
		filtered[i] = adaptive_filter_filter(&d->adap_filter[i],
				orig[i]);
		filtered[i] = thresh_filter_filter(&d->thresh_filter[i],
				filtered[i]);
	}
}

int yas53x_read(struct inv_compass_state *st, short rawfixed[3],
				int *overunderflow)
{
	int result = 0;

	int busy, i, ov;
	short t, x, y1, y2;
	s32 xyz[3], disturb[3];

	result = measure_int(st, &busy, &t, &x, &y1, &y2);
	if (result)
		return result;
	if (busy)
		return -EPERM;
	coordinate_conversion(x, y1, y2, t, &xyz[0], &xyz[1], &xyz[2]);
	filter_filter(&st->filter, xyz, xyz);
	for (i = 0; i < 3; i++)
		rawfixed[i] = (short)(xyz[i] / 100);

	if (st->first_measure_after_reset) {
		for (i = 0; i < 3; i++)
			st->base_compass_data[i] = rawfixed[i];
		st->first_measure_after_reset = 0;
	}
	ov = 0;
	for (i = 0; i < 3; i++) {
		disturb[i] = abs(st->base_compass_data[i] - rawfixed[i]);
		if (disturb[i] > YAS_MAG_DISTURBURNCE_THRESHOLD)
			ov = 1;
	}
	if (ov)
		st->reset_timer += st->delay;
	else
		st->reset_timer = 0;

	if (st->reset_timer > YAS_RESET_COIL_TIME_THRESHOLD)
		*overunderflow = (1<<8);
	else
		*overunderflow = 0;
	*overunderflow |= inv_check_range(st, x, y1, y2);

	return 0;
}

/**
 *  yas53x_read_raw() - read raw method.
 */
static int yas53x_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val,
			      int *val2,
			      long mask) {
	struct inv_compass_state  *st = iio_priv(indio_dev);

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
			*val = YAS530_SCALE;
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
	struct inv_compass_state *st = iio_priv(indio_dev);
	m = st->plat_data.orientation;
	return sprintf(buf,
	"%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		m[0],  m[1],  m[2],  m[3], m[4], m[5], m[6], m[7], m[8]);
}

static ssize_t yas53x_rate_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	u32 data;
	int error;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_compass_state *st = iio_priv(indio_dev);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (0 == data)
		return -EINVAL;
	/* transform rate to delay in ms */
	data = MSEC_PER_SEC / data;

	if (data > YAS530_MAX_DELAY)
		data = YAS530_MAX_DELAY;
	if (data < YAS530_MIN_DELAY)
		data = YAS530_MIN_DELAY;
	st->delay = data;

	return count;
}

static ssize_t yas53x_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_compass_state *st = iio_priv(indio_dev);
	/* transform delay in ms to rate */
	return sprintf(buf, "%d\n", (int)MSEC_PER_SEC / st->delay);
}

static ssize_t yas53x_overunderflow_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	u32 data;
	int error;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_compass_state *st = iio_priv(indio_dev);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (data)
		return -EINVAL;
	st->overunderflow = data;

	return count;
}

static ssize_t yas53x_overunderflow_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_compass_state *st = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", st->overunderflow);
}

void set_yas53x_enable(struct iio_dev *indio_dev, bool enable)
{
	struct inv_compass_state *st = iio_priv(indio_dev);

	yas_init_adap_filter(st);
	st->first_measure_after_reset = 1;
	st->first_read_after_reset = 1;
	schedule_delayed_work(&st->work, msecs_to_jiffies(st->delay));
}

static void yas53x_work_func(struct work_struct *work)
{
	struct inv_compass_state *st =
		container_of((struct delayed_work *)work,
			struct inv_compass_state, work);
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u32 delay = msecs_to_jiffies(st->delay);

	mutex_lock(&indio_dev->mlock);
	if (!(iio_buffer_enabled(indio_dev)))
		goto error_ret;

	schedule_delayed_work(&st->work, delay);
	inv_read_yas53x_fifo(indio_dev);
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
		.scan_index = INV_YAS53X_SCAN_MAGN_X,
		.scan_type = IIO_ST('s', 16, 16, 0)
	}, {
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_Y,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_YAS53X_SCAN_MAGN_Y,
		.scan_type = IIO_ST('s', 16, 16, 0)
	}, {
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_Z,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_YAS53X_SCAN_MAGN_Z,
		.scan_type = IIO_ST('s', 16, 16, 0)
	},
	IIO_CHAN_SOFT_TIMESTAMP(INV_YAS53X_SCAN_TIMESTAMP)
};

static DEVICE_ATTR(compass_matrix, S_IRUGO, inv_compass_matrix_show, NULL);
static DEVICE_ATTR(sampling_frequency, S_IRUGO | S_IWUSR, yas53x_rate_show,
		yas53x_rate_store);
static DEVICE_ATTR(overunderflow, S_IRUGO | S_IWUSR,
		yas53x_overunderflow_show, yas53x_overunderflow_store);

static struct attribute *inv_yas53x_attributes[] = {
	&dev_attr_compass_matrix.attr,
	&dev_attr_sampling_frequency.attr,
	&dev_attr_overunderflow.attr,
	NULL,
};
static const struct attribute_group inv_attribute_group = {
	.name = "yas53x",
	.attrs = inv_yas53x_attributes
};

static const struct iio_info yas53x_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &yas53x_read_raw,
	.attrs = &inv_attribute_group,
};

/*constant IIO attribute */
/**
 *  inv_yas53x_probe() - probe function.
 */
static int inv_yas53x_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct inv_compass_state *st;
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
	st->client = client;
	st->plat_data =
		*(struct mpu_platform_data *)dev_get_platdata(&client->dev);
	st->delay = 10;

	i2c_set_clientdata(client, indio_dev);

	if (!strcmp(id->name, "yas530")) {
		st->read_data    = yas530_read_data;
		st->get_cal_data = get_cal_data_yas530;
		st->overflow_bound = YAS_YAS530_DATA_OVERFLOW;
		st->center     = YAS_YAS530_DATA_CENTER;
		st->filter.filter_thresh = YAS530_MAG_DEFAULT_FILTER_THRESH;
	} else {
		st->read_data    = yas532_533_read_data;
		st->get_cal_data = get_cal_data_yas532_533;
		st->overflow_bound = YAS_YAS532_533_DATA_OVERFLOW;
		st->center     = YAS_YAS532_533_DATA_CENTER;
		st->filter.filter_thresh = YAS532_MAG_DEFAULT_FILTER_THRESH;
	}
	st->upper_bound = st->center + (st->center >> 1);
	st->lower_bound = (st->center >> 1);

	result = yas53x_resume(st);
	if (result)
		goto out_free;

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = id->name;
	indio_dev->channels = compass_channels;
	indio_dev->num_channels = ARRAY_SIZE(compass_channels);
	indio_dev->info = &yas53x_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	result = inv_yas53x_configure_ring(indio_dev);
	if (result)
		goto out_free;
	result = iio_buffer_register(indio_dev, indio_dev->channels,
					indio_dev->num_channels);
	if (result)
		goto out_unreg_ring;
	result = inv_yas53x_probe_trigger(indio_dev);
	if (result)
		goto out_remove_ring;

	result = iio_device_register(indio_dev);
	if (result)
		goto out_remove_trigger;
	INIT_DELAYED_WORK(&st->work, yas53x_work_func);
	pr_info("%s: Probe name %s\n", __func__, id->name);

	return 0;
out_remove_trigger:
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_yas53x_remove_trigger(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_yas53x_unconfigure_ring(indio_dev);
out_free:
	iio_free_device(indio_dev);
out_no_free:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, result);
	return -EIO;
}

/**
 *  inv_yas53x_remove() - remove function.
 */
static int inv_yas53x_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_compass_state *st = iio_priv(indio_dev);
	cancel_delayed_work_sync(&st->work);
	iio_device_unregister(indio_dev);
	inv_yas53x_remove_trigger(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_yas53x_unconfigure_ring(indio_dev);
	iio_free_device(indio_dev);

	dev_info(&client->adapter->dev, "inv_yas53x_iio module removed.\n");
	return 0;
}
static const unsigned short normal_i2c[] = { I2C_CLIENT_END };
/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct i2c_device_id inv_yas53x_id[] = {
	{"yas530", 0},
	{"yas532", 0},
	{"yas533", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, inv_yas53x_id);

static struct i2c_driver inv_yas53x_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		=	inv_yas53x_probe,
	.remove		=	inv_yas53x_remove,
	.id_table	=	inv_yas53x_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"inv_yas53x_iio",
	},
	.address_list = normal_i2c,
};

static int __init inv_yas53x_init(void)
{
	int result = i2c_add_driver(&inv_yas53x_driver);
	if (result) {
		pr_err("%s failed\n", __func__);
		return result;
	}
	return 0;
}

static void __exit inv_yas53x_exit(void)
{
	i2c_del_driver(&inv_yas53x_driver);
}

module_init(inv_yas53x_init);
module_exit(inv_yas53x_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv_yas53x_iio");
/**
 *  @}
 */


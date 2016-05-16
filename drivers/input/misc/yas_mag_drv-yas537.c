/*
 * Copyright (c) 2014 Yamaha Corporation
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *	claim that you wrote the original software. If you use this software
 *	in a product, an acknowledgment in the product documentation would be
 *	appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *	misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "yas.h"
#include <linux/kernel.h>


#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS537

#define YAS537_REG_DIDR			(0x80)
#define YAS537_REG_CMDR			(0x81)
#define YAS537_REG_CONFR		(0x82)
#define YAS537_REG_INTRVLR		(0x83)
#define YAS537_REG_OXR			(0x84)
#define YAS537_REG_OY1R			(0x85)
#define YAS537_REG_OY2R			(0x86)
#define YAS537_REG_AVRR			(0x87)
#define YAS537_REG_HCKR			(0x88)
#define YAS537_REG_LCKR			(0x89)
#define YAS537_REG_SRSTR		(0x90)
#define YAS537_REG_ADCCALR		(0x91)
#define YAS537_REG_MTCR			(0x93)
#define YAS537_REG_OCR			(0x9e)
#define YAS537_REG_TRMR			(0x9f)
#define YAS537_REG_DATAR		(0xb0)
#define YAS537_REG_CALR			(0xc0)

#define YAS537_DATA_UNDERFLOW		(0)
#define YAS537_DATA_OVERFLOW		(16383)
#define YAS537_DEVICE_ID		(0x07)	/* YAS537 (MS-3T) */

#define YAS_X_OVERFLOW			(0x01)
#define YAS_X_UNDERFLOW			(0x02)
#define YAS_Y1_OVERFLOW			(0x04)
#define YAS_Y1_UNDERFLOW		(0x08)
#define YAS_Y2_OVERFLOW			(0x10)
#define YAS_Y2_UNDERFLOW		(0x20)
#define YAS_OVERFLOW	(YAS_X_OVERFLOW|YAS_Y1_OVERFLOW|YAS_Y2_OVERFLOW)
#define YAS_UNDERFLOW	(YAS_X_UNDERFLOW|YAS_Y1_UNDERFLOW|YAS_Y2_UNDERFLOW)

#define YAS537_MAG_STATE_NORMAL		(0)
#define YAS537_MAG_STATE_INIT_COIL	(1)
#define YAS537_MAG_INITCOIL_TIMEOUT	(1000)	/* msec */
#define YAS537_MAG_POWER_ON_RESET_TIME	(4000)	/* usec */
#define YAS537_MAG_NOTRANS_POSITION	(2)

#define YAS537_MAG_AVERAGE_8		(0)
#define YAS537_MAG_AVERAGE_16		(1)
#define YAS537_MAG_AVERAGE_32		(2)
#define YAS537_MAG_AVERAGE_64		(3)
#define YAS537_MAG_AVERAGE_128		(4)
#define YAS537_MAG_AVERAGE_256		(5)

#define YAS537_MAG_RCOIL_TIME		(65)

#define set_vector(to, from) \
	{int _l; for (_l = 0; _l < 3; _l++) (to)[_l] = (from)[_l]; }

struct yas_cal {
	int8_t a2, a3, a4, a6, a7, a8;
	int16_t a5, a9, cxy1y2[3];
	uint8_t k, ver;
};

struct yas_cdriver {
	int initialized;
	struct yas_driver_callback cbk;
	int measure_state;
	int invalid_data;
	uint32_t invalid_data_time;
	int position;
	int delay;
	int enable;
	uint8_t dev_id;
	const int8_t *transform;
	int record_data;
	int average;
	int8_t hard_offset[3];
	uint32_t current_time;
	uint16_t last_raw[4];
	uint16_t last_after_rcoil[3];
	struct yas_cal cal;
	int16_t overflow[3], underflow[3];
	struct yas_matrix static_matrix;
	int noise_rcoil_flag;
};



static const struct yas_matrix no_conversion
	= {{9673, 53, 196, 143, 10195, -268, -56, -41, 10142} };

static const int measure_time_worst[] = {800, 1100, 1500, 3000, 6000, 12000};

static const int8_t YAS537_TRANSFORMATION[][9] = {
	{-1, 0, 0, 0, -1, 0, 0, 0, 1},
	{0, -1, 0, 1, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 1, 0, 0, 0, 1},
	{0, 1, 0, -1, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, -1, 0, 0, 0, -1},
	{0, 1, 0, 1, 0, 0, 0, 0, -1},
	{-1, 0, 0, 0, 1, 0, 0, 0, -1},
	{0, -1, 0, -1, 0, 0, 0, 0, -1},
};
static struct yas_cdriver driver;

static int yas_set_enable_wrap(int enable, int rcoil);
static int yas_set_enable(int enable);
static int single_read(int ldtc, int fors, int *busy, uint16_t *t,
		uint16_t *xy1y2, int *ouflow);

static int yas_open(void)
{
	if (driver.cbk.device_open(YAS_TYPE_MAG) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	driver.cbk.usleep(YAS537_MAG_POWER_ON_RESET_TIME);
	return YAS_NO_ERROR;
}
#define yas_read(a, b, c) \
	(driver.cbk.device_read(YAS_TYPE_MAG, (a), (b), (c)))
static int yas_single_write(uint8_t addr, uint8_t data)
{
	return driver.cbk.device_write(YAS_TYPE_MAG, addr, &data, 1);
}

static void apply_matrix(struct yas_vector *xyz, struct yas_matrix *m)
{
	int32_t tmp[3];
	int i;
	if (m == NULL)
		return;
	for (i = 0; i < 3; i++)
		tmp[i] = ((m->m[i*3]/10) * (xyz->v[0]/10)
				+ (m->m[i*3+1]/10) * (xyz->v[1]/10)
				+ (m->m[i*3+2]/10) * (xyz->v[2]/10)) / 100;
	for (i = 0; i < 3; i++)
		xyz->v[i] = tmp[i];
}

static uint32_t curtime(void)
{
	if (driver.cbk.current_time)
		return driver.cbk.current_time();
	else
		return driver.current_time;
}

static int invalid_magnetic_field(uint16_t *cur, uint16_t *last)
{
	int16_t invalid_thresh[] = {1500, 1500, 1500};
	int i;
	for (i = 0; i < 3; i++)
		if (invalid_thresh[i] < ABS(cur[i] - last[i]))
			return 1;
	return 0;
}

static int start_yas537(int ldtc, int fors, int cont)
{
	uint8_t data = 0x01;
	data = (uint8_t)(data | (ldtc<<1));
	data = (uint8_t)(data | (fors<<2));
	data = (uint8_t)(data | (cont<<5));
	if (yas_single_write(YAS537_REG_CMDR, data) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	return YAS_NO_ERROR;
}

static int cont_start_yas537(void)
{
	if (start_yas537(0, 0, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	/* wait for the first measurement */
	driver.cbk.usleep(measure_time_worst[driver.average]);
	return YAS_NO_ERROR;
}

static int read_yas537(int *busy, uint16_t *t, uint16_t *xy1y2,
		int *ouflow)
{
	uint8_t data[8];
	int i;
	if (yas_read(YAS537_REG_DATAR, data, 8) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	*busy = data[2]>>7;
	*t = (uint16_t)((data[0]<<8) | data[1]);
	xy1y2[0] = (uint16_t)(((data[2]&0x3f)<<8) | data[3]);
	xy1y2[1] = (uint16_t)((data[4]<<8) | data[5]);
	xy1y2[2] = (uint16_t)((data[6]<<8) | data[7]);
	for (i = 0; i < 3; i++)
		driver.last_raw[i] = xy1y2[i];
	driver.last_raw[i] = *t;
	if (driver.cal.ver == 1) {
		struct yas_cal *c = &driver.cal;
		int32_t h[3], s[3];
		for (i = 0; i < 3; i++)
			s[i] = xy1y2[i] - 8192;
		h[0] = (c->k *   (128*s[0] + c->a2*s[1] + c->a3*s[2])) / 8192;
		h[1] = (c->k * (c->a4*s[0] + c->a5*s[1] + c->a6*s[2])) / 8192;
		h[2] = (c->k * (c->a7*s[0] + c->a8*s[1] + c->a9*s[2])) / 8192;
		for (i = 0; i < 3; i++) {
			if (h[i] < -8192)
				h[i] = -8192;
			if (8191 < h[i])
				h[i] = 8191;
			xy1y2[i] = h[i] + 8192;
		}
	}
	*ouflow = 0;
	for (i = 0; i < 3; i++) {
		if (driver.overflow[i] <= xy1y2[i])
			*ouflow |= (1<<(i*2));
		if (xy1y2[i] <= driver.underflow[i])
			*ouflow |= (1<<(i*2+1));
	}
	return YAS_NO_ERROR;
}

static int update_intrvlr(int delay)
{
	uint8_t data;
	/* delay 4.1 x SMPLTIM [7:0] msec */
	if ((4100 * 255 + measure_time_worst[driver.average]) / 1000 < delay)
		delay = 4100 * 255 + measure_time_worst[driver.average];
	else
		delay *= 1000;
	delay = (delay - measure_time_worst[driver.average]) / 4100;
	if (delay <= 1)
		data = 2;
	else
		data = (uint8_t) delay;
	if (yas_single_write(YAS537_REG_INTRVLR, data) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	return YAS_NO_ERROR;
}

static int reset_yas537(int rcoil)
{
	struct yas_cal *c = &driver.cal;
	static const uint8_t avrr[] = {0x50, 0x60, 0x70, 0x71, 0x72, 0x73};
	int cal_valid = 0, i, rt, busy, ouflow;
	uint16_t xy1y2[3], t;
	uint8_t data[17];
	if (yas_single_write(YAS537_REG_SRSTR, 0x02) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (yas_read(YAS537_REG_CALR, data, 17) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	c->ver = data[16] >> 6;
	for (i = 0; i < 17; i++) {
		if (i < 16 && data[i] != 0)
			cal_valid = 1;
		if (i == 16 && (data[i] & 0x3f) != 0)
			cal_valid = 1;
	}
	if (!cal_valid)
		return YAS_ERROR_CALREG;
	if (c->ver == 0) {
		for (i = 0; i < 17; i++) {
			if (i < 12) {
				if (yas_single_write(YAS537_REG_MTCR+i,
							data[i]) < 0)
					return YAS_ERROR_DEVICE_COMMUNICATION;
			} else if (i < 15) {
				if (yas_single_write(YAS537_REG_OXR+i-12,
							data[i]) < 0)
					return YAS_ERROR_DEVICE_COMMUNICATION;
				driver.hard_offset[i-12] = data[i];
			} else {
				if (yas_single_write(YAS537_REG_OXR+i-11,
							data[i]) < 0)
					return YAS_ERROR_DEVICE_COMMUNICATION;
			}
		}
		for (i = 0; i < 3; i++) {
			driver.overflow[i] = YAS537_DATA_OVERFLOW;
			driver.underflow[i] = YAS537_DATA_UNDERFLOW;
		}
	} else if (c->ver == 1) {
		for (i = 0; i < 3; i++) {
			if (yas_single_write(YAS537_REG_MTCR+i,
						data[i]) < 0)
				return YAS_ERROR_DEVICE_COMMUNICATION;
			if (yas_single_write(YAS537_REG_OXR+i,
						data[i+12]) < 0)
				return YAS_ERROR_DEVICE_COMMUNICATION;
			driver.hard_offset[i] = data[i+12];
		}
		if (yas_single_write(YAS537_REG_MTCR+i,
					(data[i] & 0xe0) | 0x10) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		if (yas_single_write(YAS537_REG_HCKR, (data[15]>>3)&0x1e) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		if (yas_single_write(YAS537_REG_LCKR, (data[15]<<1)&0x1e) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		if (yas_single_write(YAS537_REG_OCR, data[16]&0x3f) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		c->cxy1y2[0] = ((data[0]<<1) | (data[1]>>7)) - 256;
		c->cxy1y2[1] = (((data[1]<<2)&0x1fc) | (data[2]>>6)) - 256;
		c->cxy1y2[2] = (((data[2]<<3)&0x1f8) | (data[3]>>5)) - 256;
		c->a2 = (((data[3]<<2)&0x7c) | (data[4]>>6)) - 64;
		c->a3 = (((data[4]<<1)&0x7e) | (data[5]>>7)) - 64;
		c->a4 = (((data[5]<<1)&0xfe) | (data[6]>>7)) - 128;
		c->a5 = (((data[6]<<2)&0x1fc) | (data[7]>>6)) - 112;
		c->a6 = (((data[7]<<1)&0x7e) | (data[8]>>7)) - 64;
		c->a7 = (((data[8]<<1)&0xfe) | (data[9]>>7)) - 128;
		c->a8 = (data[9]&0x7f) - 64;
		c->a9 = (((data[10]<<1)&0x1fe) | (data[11]>>7)) - 112;
		c->k = data[11]&0x7f;
		for (i = 0; i < 3; i++) {
			int16_t a[3];
			a[0] = 128;
			a[1] = c->a5;
			a[2] = c->a9;
			driver.overflow[i] = 8192 + c->k * a[i] * (8192
					- ABS(c->cxy1y2[i]) * 325 / 16 - 192)
				/ 8192;
			driver.underflow[i] = 8192 - c->k * a[i] * (8192
					- ABS(c->cxy1y2[i]) * 325 / 16 - 192)
				/ 8192;
			if (YAS537_DATA_OVERFLOW < driver.overflow[i])
				driver.overflow[i] = YAS537_DATA_OVERFLOW;
			if (driver.underflow[i] < YAS537_DATA_UNDERFLOW)
				driver.underflow[i] = YAS537_DATA_UNDERFLOW;
		}
	} else
		return YAS_ERROR_CALREG;
	if (yas_single_write(YAS537_REG_ADCCALR, 0x03) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (yas_single_write(YAS537_REG_ADCCALR+1, 0xf8) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (yas_single_write(YAS537_REG_TRMR, 0xff) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (update_intrvlr(driver.delay) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (yas_single_write(YAS537_REG_AVRR, avrr[driver.average]) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (rcoil) {
		if (yas_single_write(YAS537_REG_CONFR, 0x08) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		driver.cbk.usleep(YAS537_MAG_RCOIL_TIME);
		rt = single_read(0, 0, &busy, &t, xy1y2, &ouflow);
		if (rt < 0)
			return rt;
		if (busy)
			return YAS_ERROR_BUSY;
		set_vector(driver.last_after_rcoil, xy1y2);
	}
	return YAS_NO_ERROR;
}

static int single_read(int ldtc, int fors, int *busy, uint16_t *t,
		uint16_t *xy1y2, int *ouflow)
{
	if (start_yas537(ldtc, fors, 0) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	driver.cbk.usleep(measure_time_worst[driver.average]);
	if (read_yas537(busy, t, xy1y2, ouflow) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	return YAS_NO_ERROR;
}

static void xy1y2_to_xyz(uint16_t *xy1y2, int32_t *xyz)
{
	xyz[0] = (xy1y2[0] - 8192) * 300;
	xyz[1] = (xy1y2[1] - xy1y2[2]) * 1732 / 10;
	xyz[2] = (-xy1y2[1] - xy1y2[2] + 16384) * 300;
}

static int yas_cdrv_sensitivity_measuremnet(int32_t *sx, int32_t *sy)
{
	uint16_t p[3], m[3], xy1y2[3], t;
	struct yas_cal *c = &driver.cal;
	int busy, flowon = 0, flowoff = 0, rt, i;
	rt = single_read(1, 0, &busy, &t, xy1y2, &flowon);
	if (rt < 0)
		return rt;
	if (busy)
		return YAS_ERROR_BUSY;
	for (i = 0; i < 3; i++)
		p[i] = driver.last_raw[i];
	rt = single_read(1, 1, &busy, &t, xy1y2, &flowoff);
	if (rt < 0)
		return rt;
	if (busy)
		return YAS_ERROR_BUSY;
	for (i = 0; i < 3; i++)
		m[i] = driver.last_raw[i];
	if (driver.cal.ver == 1) {
		*sx = c->k * 128 * (p[0] - m[0]) / 8192 * 300 / YAS_MAG_VCORE;
		*sy = c->k * (c->a5 * (p[1] - m[1]) - c->a9 * (p[2] - m[2]))
			/ 8192 * 1732 / YAS_MAG_VCORE / 10;
	} else
		return YAS_ERROR_CALREG;
	return flowon | flowoff;
}

static int yas_get_position(void)
{
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	return driver.position;
}

static int yas_set_position(int position)
{
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	if (position < 0 || 7 < position)
		return YAS_ERROR_ARG;
	if (position == YAS537_MAG_NOTRANS_POSITION)
		driver.transform = NULL;
	else
		driver.transform = YAS537_TRANSFORMATION[position];
	driver.position = position;
	return YAS_NO_ERROR;
}

static int yas_measure(struct yas_data *data, int num, int *ouflow)
{
	int32_t xyz_tmp[3];
	int i, busy, rt;
	uint16_t t, xy1y2[3];
	uint32_t tm;
	*ouflow = 0;
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	if (data == NULL || num < 0)
		return YAS_ERROR_ARG;
	if (driver.cbk.current_time == NULL)
		driver.current_time += (uint32_t)driver.delay;
	if (num == 0)
		return 0;
	if (!driver.enable)
		return 0;
	if (driver.measure_state == YAS537_MAG_STATE_INIT_COIL) {
		tm = curtime();
		if (YAS537_MAG_INITCOIL_TIMEOUT
				<= tm - driver.invalid_data_time) {
			driver.invalid_data_time = tm;
			rt = reset_yas537(1);
			if (rt < 0)
				return rt;
			if (cont_start_yas537() < 0)
				return YAS_ERROR_DEVICE_COMMUNICATION;
			driver.measure_state = YAS537_MAG_STATE_NORMAL;
		}
	}
	if (read_yas537(&busy, &t, xy1y2, ouflow) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	xy1y2_to_xyz(xy1y2, data->xyz.v);
	if (driver.transform != NULL) {
		for (i = 0; i < 3; i++) {
			xyz_tmp[i] = driver.transform[i*3] * data->xyz.v[0]
				+ driver.transform[i*3+1] * data->xyz.v[1]
				+ driver.transform[i*3+2] * data->xyz.v[2];
		}
		set_vector(data->xyz.v, xyz_tmp);
	}
	apply_matrix(&data->xyz, &driver.static_matrix);
	for (i = 0; i < 3; i++) {
		data->xyz.v[i] -= data->xyz.v[i] % 10;
		if (*ouflow & (1<<(i*2)))
			data->xyz.v[i] += 1; /* set overflow */
		if (*ouflow & (1<<(i*2+1)))
			data->xyz.v[i] += 2; /* set underflow */
	}
	tm = curtime();
	data->type = YAS_TYPE_MAG;
	if (driver.cbk.current_time)
		data->timestamp = tm;
	else
		data->timestamp = 0;
	data->accuracy = 0;
	if (busy)
		return YAS_ERROR_BUSY;
	if (0 < *ouflow || invalid_magnetic_field(xy1y2,
				driver.last_after_rcoil)) {
		if (!driver.invalid_data)
			driver.invalid_data_time = tm;
		driver.invalid_data = 1;
		driver.measure_state = YAS537_MAG_STATE_INIT_COIL;
		for (i = 0; i < 3; i++) {
			if (!*ouflow)
				data->xyz.v[i] += 3;
		}
	} else
		driver.invalid_data = 0;
	return 1;
}

static int yas_measure_wrap(struct yas_data *data, int num)
{
	int ouflow;
	return yas_measure(data, num, &ouflow);
}

static int yas_get_delay(void)
{
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	return driver.delay;
}

static int yas_set_delay(int delay)
{
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	if (delay < 0)
		return YAS_ERROR_ARG;
	driver.delay = delay;
	if (!driver.enable)
		return YAS_NO_ERROR;
	yas_set_enable(0);
	yas_set_enable_wrap(1, 0);
	return YAS_NO_ERROR;
}

static int yas_get_enable(void)
{
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	return driver.enable;
}

static int yas_set_enable_wrap(int enable, int rcoil)
{
	int rt = YAS_NO_ERROR;
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	enable = !!enable;
	if (driver.enable == enable)
		return YAS_NO_ERROR;
	if (enable) {
		if (yas_open() < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		rt = reset_yas537(rcoil);
		if (rt < 0) {
			driver.cbk.device_close(YAS_TYPE_MAG);
			return rt;
		}
		if (cont_start_yas537() < 0) {
			driver.cbk.device_close(YAS_TYPE_MAG);
			return YAS_ERROR_DEVICE_COMMUNICATION;
		}
		if (rcoil)
			driver.measure_state = YAS537_MAG_STATE_NORMAL;
	} else {
		yas_single_write(YAS537_REG_SRSTR, 0x02);
		driver.cbk.device_close(YAS_TYPE_MAG);
	}
	driver.enable = enable;
	return rt;
}

static int yas_set_enable(int enable)
{
	return yas_set_enable_wrap(enable, 1);
}

static int yas_ext(int32_t cmd, void *p)
{
	struct yas537_self_test_result *r;
	struct yas_data data;
	int32_t *xyz;
	int16_t *ouflow, *m;
	int8_t average, *hard_offset;
	int rt, i, enable, overflow, busy, position;
	uint16_t t, xy1y2[3];
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	if (p == NULL)
		return YAS_ERROR_ARG;
	switch (cmd) {
	case YAS537_SELF_TEST:
		r = (struct yas537_self_test_result *) p;
		r->id = driver.dev_id;
		enable = driver.enable;
		if (!enable) {
			if (yas_open() < 0)
				return YAS_ERROR_DEVICE_COMMUNICATION;
		}
		rt = reset_yas537(1);
		if (rt < 0)
			goto self_test_exit;
		rt = single_read(0, 0, &busy, &t, xy1y2, &overflow);
		if (rt < 0)
			goto self_test_exit;
		if (busy) {
			rt = YAS_ERROR_BUSY;
			goto self_test_exit;
		}
		xy1y2_to_xyz(xy1y2, r->xyz);
		for (i = 0; i < 3; i++)
			r->xyz[i] = r->xyz[i] / 1000;
		if (overflow & YAS_OVERFLOW) {
			rt = YAS_ERROR_OVERFLOW;
			goto self_test_exit;
		}
		if (overflow & YAS_UNDERFLOW) {
			rt = YAS_ERROR_UNDERFLOW;
			goto self_test_exit;
		}
		if (r->xyz[0] == 0 && r->xyz[1] == 0 && r->xyz[2] == 0) {
			rt = YAS_ERROR_DIRCALC;
			goto self_test_exit;
		}
		r->dir = 99;
		rt = yas_cdrv_sensitivity_measuremnet(&r->sx, &r->sy);
		if (rt < 0)
			goto self_test_exit;
		if (rt & YAS_OVERFLOW) {
			rt = YAS_ERROR_OVERFLOW;
			goto self_test_exit;
		}
		if (rt & YAS_UNDERFLOW) {
			rt = YAS_ERROR_UNDERFLOW;
			goto self_test_exit;
		}
		rt = YAS_NO_ERROR;
self_test_exit:
		if (enable)
			cont_start_yas537();
		else
			driver.cbk.device_close(YAS_TYPE_MAG);
		return rt;
	case YAS537_SELF_TEST_NOISE:
		xyz = (int32_t *) p;
		enable = driver.enable;
		if (!enable) {
			if (driver.noise_rcoil_flag)
				rt = yas_set_enable_wrap(1, 1);
			else
				rt = yas_set_enable_wrap(1, 0);
			if (rt < 0)
				return rt;
			driver.noise_rcoil_flag = 0;
		}
		position = yas_get_position();
		yas_set_position(YAS537_MAG_NOTRANS_POSITION);
		rt = yas_measure(&data, 1, &overflow);
		yas_set_position(position);
		if (rt < 0) {
			if (!enable)
				yas_set_enable(0);
			return rt;
		}
		for (i = 0; i < 3; i++)
			xyz[i] = data.xyz.v[i] / 300;
		if (!enable)
			yas_set_enable(0);
		return YAS_NO_ERROR;
	case YAS537_GET_LAST_RAWDATA:
		for (i = 0; i < 4; i++)
			((uint16_t *) p)[i] = driver.last_raw[i];
		return YAS_NO_ERROR;
	case YAS537_GET_AVERAGE_SAMPLE:
		*(int8_t *) p = driver.average;
		return YAS_NO_ERROR;
	case YAS537_SET_AVERAGE_SAMPLE:
		average = *(int8_t *) p;
		if (average < YAS537_MAG_AVERAGE_8
				|| YAS537_MAG_AVERAGE_256 < average)
			return YAS_ERROR_ARG;
		driver.average = average;
		if (!driver.enable)
			return YAS_NO_ERROR;
		yas_set_enable(0);
		yas_set_enable_wrap(1, 0);
		return YAS_NO_ERROR;
	case YAS537_GET_HW_OFFSET:
		hard_offset = (int8_t *) p;
		for (i = 0; i < 3; i++)
			hard_offset[i] = driver.hard_offset[i];
		return YAS_NO_ERROR;
	case YAS537_GET_STATIC_MATRIX:
		m = (int16_t *) p;
		for (i = 0; i < 9; i++)
			m[i] = driver.static_matrix.m[i];
		return YAS_NO_ERROR;
	case YAS537_SET_STATIC_MATRIX:
		m = (int16_t *) p;
		for (i = 0; i < 9; i++)
			driver.static_matrix.m[i] = m[i];
		return YAS_NO_ERROR;
	case YAS537_GET_OUFLOW_THRESH:
		ouflow = (int16_t *) p;
		for (i = 0; i < 3; i++) {
			ouflow[i] = driver.overflow[i];
			ouflow[i+3] = driver.underflow[i];
		}
		return YAS_NO_ERROR;
	default:
		break;
	}
	return YAS_ERROR_ARG;
}

static int yas_init(void)
{
	int i;
	uint8_t data;
	if (driver.initialized)
		return YAS_ERROR_INITIALIZE;
	if (yas_open() < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (yas_read(YAS537_REG_DIDR, &data, 1) < 0) {
		driver.cbk.device_close(YAS_TYPE_MAG);
		return YAS_ERROR_DEVICE_COMMUNICATION;
	}
	driver.dev_id = data;
	if (driver.dev_id != YAS537_DEVICE_ID) {
		driver.cbk.device_close(YAS_TYPE_MAG);
		return YAS_ERROR_CHIP_ID;
	}
	driver.cbk.device_close(YAS_TYPE_MAG);

	driver.measure_state = YAS537_MAG_STATE_NORMAL;
	if (driver.cbk.current_time)
		driver.current_time =  driver.cbk.current_time();
	else
		driver.current_time = 0;
	driver.invalid_data = 0;
	driver.invalid_data_time = driver.current_time;
	driver.position = YAS537_MAG_NOTRANS_POSITION;
	driver.delay = YAS_DEFAULT_SENSOR_DELAY;
	driver.enable = 0;
	driver.transform = NULL;
	driver.record_data = 0;
	driver.average = YAS537_MAG_AVERAGE_32;
	driver.noise_rcoil_flag = 1;
	for (i = 0; i < 3; i++) {
		driver.hard_offset[i] = -128;
		driver.last_after_rcoil[i] = 0;
		driver.overflow[i] = -1;
		driver.underflow[i] = -1;
	}
	for (i = 0; i < 4; i++)
		driver.last_raw[i] = 0;
	driver.static_matrix = no_conversion;
	driver.initialized = 1;
	return YAS_NO_ERROR;
}

static int yas_term(void)
{
	int rt;
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	rt = yas_set_enable(0);
	driver.initialized = 0;
	return rt;
}

int yas_mag_driver_init(struct yas_mag_driver *f)
{
	if (f == NULL || f->callback.device_open == NULL
			|| f->callback.device_close == NULL
			|| f->callback.device_read == NULL
			|| f->callback.device_write == NULL
			|| f->callback.usleep == NULL
	   )
		return YAS_ERROR_ARG;
	f->init = yas_init;
	f->term = yas_term;
	f->get_delay = yas_get_delay;
	f->set_delay = yas_set_delay;
	f->get_enable = yas_get_enable;
	f->set_enable = yas_set_enable;
	f->get_position = yas_get_position;
	f->set_position = yas_set_position;
	f->measure = yas_measure_wrap;
	f->ext = yas_ext;
	driver.cbk = f->callback;
	yas_term();
	return YAS_NO_ERROR;
}
#endif

/*
 * Copyright (c) 2013-2014 Yamaha Corporation
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

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS532 \
	|| YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS533

#define YAS532_REG_DEVID		(0x80)
#define YAS532_REG_RCOILR		(0x81)
#define YAS532_REG_CMDR			(0x82)
#define YAS532_REG_CONFR		(0x83)
#define YAS532_REG_DLYR			(0x84)
#define YAS532_REG_OXR			(0x85)
#define YAS532_REG_OY1R			(0x86)
#define YAS532_REG_OY2R			(0x87)
#define YAS532_REG_TEST1R		(0x88)
#define YAS532_REG_TEST2R		(0x89)
#define YAS532_REG_CALR			(0x90)
#define YAS532_REG_DATAR		(0xB0)

#define YAS532_VERSION_AC_COEF_X	(850)
#define YAS532_VERSION_AC_COEF_Y1	(750)
#define YAS532_VERSION_AC_COEF_Y2	(750)
#define YAS532_DATA_CENTER		(4096)
#define YAS532_DATA_UNDERFLOW		(0)
#define YAS532_DATA_OVERFLOW		(8190)
#define YAS532_DEVICE_ID		(0x02)	/* YAS532 (MS-3R/3F) */
#define YAS532_TEMP20DEGREE_TYPICAL	(390)

#define YAS_X_OVERFLOW			(0x01)
#define YAS_X_UNDERFLOW			(0x02)
#define YAS_Y1_OVERFLOW			(0x04)
#define YAS_Y1_UNDERFLOW		(0x08)
#define YAS_Y2_OVERFLOW			(0x10)
#define YAS_Y2_UNDERFLOW		(0x20)
#define YAS_OVERFLOW	(YAS_X_OVERFLOW|YAS_Y1_OVERFLOW|YAS_Y2_OVERFLOW)
#define YAS_UNDERFLOW	(YAS_X_UNDERFLOW|YAS_Y1_UNDERFLOW|YAS_Y2_UNDERFLOW)

#define YAS532_MAG_STATE_NORMAL		(0)
#define YAS532_MAG_STATE_INIT_COIL	(1)
#define YAS532_MAG_STATE_MEASURE_OFFSET	(2)
#define YAS532_MAG_INITCOIL_TIMEOUT	(1000)	/* msec */
#define YAS532_MAG_TEMPERATURE_LOG	(10)
#define YAS532_MAG_NOTRANS_POSITION	(3)
#if YAS532_DRIVER_NO_SLEEP
#define YAS_MAG_MAX_BUSY_LOOP		(1000)
#endif

#define set_vector(to, from) \
	{int _l; for (_l = 0; _l < 3; _l++) (to)[_l] = (from)[_l]; }
#define is_valid_offset(a) \
	(((a)[0] <= 31) && ((a)[1] <= 31) && ((a)[2] <= 31) \
		&& (-31 <= (a)[0]) && (-31 <= (a)[1]) && (-31 <= (a)[2]))

struct yas_cal_data {
	int8_t rxy1y2[3];
	uint8_t fxy1y2[3];
	int32_t cx, cy1, cy2;
	int32_t a2, a3, a4, a5, a6, a7, a8, a9, k;
};
#if 1 < YAS532_MAG_TEMPERATURE_LOG
struct yas_temperature_filter {
	uint16_t log[YAS532_MAG_TEMPERATURE_LOG];
	int num;
	int idx;
};
#endif
struct yas_cdriver {
	int initialized;
	struct yas_cal_data cal;
	struct yas_driver_callback cbk;
	int measure_state;
	int8_t hard_offset[3];
	int32_t coef[3];
	int overflow;
	uint32_t overflow_time;
	int position;
	int delay;
	int enable;
	uint8_t dev_id;
	const int8_t *transform;
#if 1 < YAS532_MAG_TEMPERATURE_LOG
	struct yas_temperature_filter t;
#endif
	uint32_t current_time;
	uint16_t last_raw[4];
#if YAS532_DRIVER_NO_SLEEP
	int start_flag;
	int wait_flag;
#endif
	struct yas_matrix static_matrix;
};

static const int yas532_version_ac_coef[] = {YAS532_VERSION_AC_COEF_X,
	YAS532_VERSION_AC_COEF_Y1, YAS532_VERSION_AC_COEF_Y2};
static const int8_t INVALID_OFFSET[] = {0x7f, 0x7f, 0x7f};
static const struct yas_matrix no_conversion
	= {{10000, 0, 0, 0, 10000, 0, 0, 0, 10000} };
static const int8_t YAS532_TRANSFORMATION[][9] = {
	{0, 1, 0, -1, 0, 0, 0, 0, 1},
	{-1, 0, 0, 0, -1, 0, 0, 0, 1},
	{0, -1, 0, 1, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 1, 0, 0, 0, 1},
	{0, -1, 0, -1, 0, 0, 0, 0, -1},
	{1, 0, 0, 0, -1, 0, 0, 0, -1},
	{0, 1, 0, 1, 0, 0, 0, 0, -1},
	{-1, 0, 0, 0, 1, 0, 0, 0, -1},
};
static struct yas_cdriver driver;

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

static void xy1y2_to_linear(uint16_t *xy1y2, int32_t *xy1y2_linear)
{
	static const uint16_t cval[] = {3721, 3971, 4221, 4471};
	int i;
	for (i = 0; i < 3; i++)
		xy1y2_linear[i] = xy1y2[i] - cval[driver.cal.fxy1y2[i]]
			+ (driver.hard_offset[i] - driver.cal.rxy1y2[i])
			* driver.coef[i];
}

static int get_cal_data_yas532(struct yas_cal_data *c)
{
	uint8_t data[14]; int i;
	if (yas_read(YAS532_REG_CALR, data, 14) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (yas_read(YAS532_REG_CALR, data, 14) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	c->fxy1y2[0] = (uint8_t)(((data[10]&0x01)<<1) | ((data[11]>>7)&0x01));
	c->rxy1y2[0] = ((int8_t)(((data[10]>>1) & 0x3f)<<2))>>2;
	c->fxy1y2[1] = (uint8_t)(((data[11]&0x01)<<1) | ((data[12]>>7)&0x01));
	c->rxy1y2[1] = ((int8_t)(((data[11]>>1) & 0x3f)<<2))>>2;
	c->fxy1y2[2] = (uint8_t)(((data[12]&0x01)<<1) | ((data[13]>>7)&0x01));
	c->rxy1y2[2] = ((int8_t)(((data[12]>>1) & 0x3f)<<2))>>2;
	c->cx = data[0] * 10 - 1280;
	c->cy1 = data[1] * 10 - 1280;
	c->cy2 = data[2] * 10 - 1280;
	c->a2 = ((data[3]>>2)&0x03f) - 32;
	c->a3 = (uint8_t)(((data[3]<<2) & 0x0c) | ((data[4]>>6) & 0x03)) - 8;
	c->a4 = (uint8_t)(data[4] & 0x3f) - 32;
	c->a5 = ((data[5]>>2) & 0x3f) + 38;
	c->a6 = (uint8_t)(((data[5]<<4) & 0x30) | ((data[6]>>4) & 0x0f)) - 32;
	c->a7 = (uint8_t)(((data[6]<<3) & 0x78) | ((data[7]>>5) & 0x07)) - 64;
	c->a8 = (uint8_t)(((data[7]<<1) & 0x3e) | ((data[8]>>7) & 0x01)) - 32;
	c->a9 = (uint8_t)(((data[8]<<1) & 0xfe) | ((data[9]>>7) & 0x01));
	c->k = (uint8_t)((data[9]>>2) & 0x1f);
	for (i = 0; i < 13; i++)
		if (data[i] != 0)
			return YAS_NO_ERROR;
	if (data[13] & 0x80)
		return YAS_NO_ERROR;
	return YAS_ERROR_CALREG;
}

#if YAS532_DRIVER_NO_SLEEP
static int busy_wait(void)
{
	int i;
	uint8_t busy;
	for (i = 0; i < YAS_MAG_MAX_BUSY_LOOP; i++) {
		if (yas_read(YAS532_REG_DATAR, &busy, 1) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		if (!(busy & 0x80))
			return YAS_NO_ERROR;
	}
	return YAS_ERROR_BUSY;
}

static int wait_if_busy(void)
{
	int rt;
	if (driver.start_flag && driver.wait_flag) {
		rt = busy_wait();
		if (rt < 0)
			return rt;
		driver.wait_flag = 0;
	}
	return YAS_NO_ERROR;
}
#endif

static int measure_start_yas532(int ldtc, int fors, int wait)
{
	uint8_t data = 0x01;
	data = (uint8_t)(data | (((!!ldtc)<<1) & 0x02));
	data = (uint8_t)(data | (((!!fors)<<2) & 0x04));
	if (yas_single_write(YAS532_REG_CMDR, data) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
#if YAS532_DRIVER_NO_SLEEP
	if (wait) {
		int rt;
		rt = busy_wait();
		if (rt < 0)
			return rt;
		driver.wait_flag = 0;
	} else
		driver.wait_flag = 1;
	driver.start_flag = 1;
#else
	(void) wait;
	driver.cbk.usleep(1500);
#endif
	return YAS_NO_ERROR;
}

static int measure_normal_yas532(int ldtc, int fors, int *busy, uint16_t *t,
		uint16_t *xy1y2, int *ouflow)
{
	uint8_t data[8];
	int i, rt;
#if YAS532_DRIVER_NO_SLEEP
	if (!driver.start_flag) {
#endif
		rt = measure_start_yas532(ldtc, fors, 1);
		if (rt < 0)
			return rt;
#if YAS532_DRIVER_NO_SLEEP
	}
#endif
	if (yas_read(YAS532_REG_DATAR, data, 8) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
#if YAS532_DRIVER_NO_SLEEP
	driver.start_flag = 0;
#endif
	*busy = (data[0]>>7) & 0x01;
	*t = (uint16_t)((((int32_t)data[0]<<3) & 0x3f8)|((data[1]>>5) & 0x07));
	xy1y2[0] = (uint16_t)((((int32_t)data[2]<<6) & 0x1fc0)
			| ((data[3]>>2) & 0x3f));
	xy1y2[1] = (uint16_t)((((int32_t)data[4]<<6) & 0x1fc0)
			| ((data[5]>>2) & 0x3f));
	xy1y2[2] = (uint16_t)((((int32_t)data[6]<<6) & 0x1fc0)
			| ((data[7]>>2) & 0x3f));
	*ouflow = 0;
	for (i = 0; i < 3; i++) {
		if (xy1y2[i] == YAS532_DATA_OVERFLOW)
			*ouflow |= (1<<(i*2));
		if (xy1y2[i] == YAS532_DATA_UNDERFLOW)
			*ouflow |= (1<<(i*2+1));
	}
	return YAS_NO_ERROR;
}

static int yas_cdrv_set_offset(const int8_t *offset)
{
	if (yas_single_write(YAS532_REG_OXR, (uint8_t)offset[0]) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (yas_single_write(YAS532_REG_OY1R, (uint8_t)offset[1]) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (yas_single_write(YAS532_REG_OY2R, (uint8_t)offset[2]) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	set_vector(driver.hard_offset, offset);
	return YAS_NO_ERROR;
}

static int yas_cdrv_measure_and_set_offset(void)
{
	static const int correct[5] = {16, 8, 4, 2, 1};
	int8_t hard_offset[3] = {0, 0, 0};
	uint16_t t, xy1y2[3];
	int32_t flag[3];
	int i, j, busy, ouflow, rt;
#if YAS532_DRIVER_NO_SLEEP
	driver.start_flag = 0;
#endif
	for (i = 0; i < 5; i++) {
		rt = yas_cdrv_set_offset(hard_offset);
		if (rt < 0)
			return rt;
		rt = measure_normal_yas532(0, 0, &busy, &t, xy1y2, &ouflow);
		if (rt < 0)
			return rt;
		if (busy)
			return YAS_ERROR_BUSY;
		for (j = 0; j < 3; j++) {
			if (YAS532_DATA_CENTER == xy1y2[j])
				flag[j] = 0;
			if (YAS532_DATA_CENTER < xy1y2[j])
				flag[j] = 1;
			if (xy1y2[j] < YAS532_DATA_CENTER)
				flag[j] = -1;
		}
		for (j = 0; j < 3; j++)
			if (flag[j])
				hard_offset[j] = (int8_t)(hard_offset[j]
						+ flag[j] * correct[i]);
	}
	return yas_cdrv_set_offset(hard_offset);
}

static int yas_cdrv_sensitivity_measuremnet(int32_t *sx, int32_t *sy)
{
	struct yas_cal_data *c = &driver.cal;
	uint16_t xy1y2_on[3], xy1y2_off[3], t;
	int busy, flowon = 0, flowoff = 0;
	if (measure_normal_yas532(1, 0, &busy, &t, xy1y2_on, &flowon) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (busy)
		return YAS_ERROR_BUSY;
	if (measure_normal_yas532(1, 1, &busy, &t, xy1y2_off, &flowoff) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (busy)
		return YAS_ERROR_BUSY;
	*sx = c->k * (xy1y2_on[0] - xy1y2_off[0]) * 10 / YAS_MAG_VCORE;
	*sy = c->k * c->a5 * ((xy1y2_on[1] - xy1y2_off[1])
			- (xy1y2_on[2] - xy1y2_off[2])) / 10 / YAS_MAG_VCORE;
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
	if (position == YAS532_MAG_NOTRANS_POSITION)
		driver.transform = NULL;
	else
		driver.transform = YAS532_TRANSFORMATION[position];
	driver.position = position;
	return YAS_NO_ERROR;
}

static int yas_set_offset(const int8_t *hard_offset)
{
	if (!driver.enable) {
		set_vector(driver.hard_offset, hard_offset);
		return YAS_NO_ERROR;
	}
	if (is_valid_offset(hard_offset)) {
#if YAS532_DRIVER_NO_SLEEP
		int rt;
		rt = wait_if_busy();
		if (rt < 0)
			return rt;
#endif
		if (yas_cdrv_set_offset(hard_offset) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		driver.measure_state = YAS532_MAG_STATE_NORMAL;
	} else {
		set_vector(driver.hard_offset, INVALID_OFFSET);
		driver.measure_state = YAS532_MAG_STATE_MEASURE_OFFSET;
	}
	return YAS_NO_ERROR;
}

static int yas_measure(struct yas_data *data, int num, int temp_correction,
		int *ouflow)
{
	struct yas_cal_data *c = &driver.cal;
	int32_t xy1y2_linear[3];
	int32_t xyz_tmp[3], tmp;
	int32_t sx, sy1, sy2, sy, sz;
	int i, busy;
	uint16_t t, xy1y2[3];
	uint32_t tm;
	int rt;
#if 1 < YAS532_MAG_TEMPERATURE_LOG
	int32_t sum = 0;
#endif
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
	switch (driver.measure_state) {
	case YAS532_MAG_STATE_INIT_COIL:
		tm = curtime();
		if (tm - driver.overflow_time < YAS532_MAG_INITCOIL_TIMEOUT)
			break;
		driver.overflow_time = tm;
		if (yas_single_write(YAS532_REG_RCOILR, 0x00) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		if (!driver.overflow && is_valid_offset(driver.hard_offset)) {
			driver.measure_state = YAS532_MAG_STATE_NORMAL;
			break;
		}
		/* FALLTHRU */
	case YAS532_MAG_STATE_MEASURE_OFFSET:
		rt = yas_cdrv_measure_and_set_offset();
		if (rt < 0)
			return rt;
		driver.measure_state = YAS532_MAG_STATE_NORMAL;
		break;
	}

	if (measure_normal_yas532(0, 0, &busy, &t, xy1y2, ouflow) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	xy1y2_to_linear(xy1y2, xy1y2_linear);
#if 1 < YAS532_MAG_TEMPERATURE_LOG
	driver.t.log[driver.t.idx++] = t;
	if (YAS532_MAG_TEMPERATURE_LOG <= driver.t.idx)
		driver.t.idx = 0;
	driver.t.num++;
	if (YAS532_MAG_TEMPERATURE_LOG <= driver.t.num)
		driver.t.num = YAS532_MAG_TEMPERATURE_LOG;
	for (i = 0; i < driver.t.num; i++)
		sum += driver.t.log[i];
	tmp = sum * 10 / driver.t.num - YAS532_TEMP20DEGREE_TYPICAL * 10;
#else
	tmp = (t - YAS532_TEMP20DEGREE_TYPICAL) * 10;
#endif
	sx  = xy1y2_linear[0];
	sy1 = xy1y2_linear[1];
	sy2 = xy1y2_linear[2];
	if (temp_correction) {
		sx  -= (c->cx  * tmp) / 1000;
		sy1 -= (c->cy1 * tmp) / 1000;
		sy2 -= (c->cy2 * tmp) / 1000;
	}
	sy = sy1 - sy2;
	sz = -sy1 - sy2;
	data->xyz.v[0] = c->k * ((100   * sx + c->a2 * sy + c->a3 * sz) / 10);
	data->xyz.v[1] = c->k * ((c->a4 * sx + c->a5 * sy + c->a6 * sz) / 10);
	data->xyz.v[2] = c->k * ((c->a7 * sx + c->a8 * sy + c->a9 * sz) / 10);
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
	if (0 < *ouflow) {
		if (!driver.overflow)
			driver.overflow_time = tm;
		driver.overflow = 1;
		driver.measure_state = YAS532_MAG_STATE_INIT_COIL;
	} else
		driver.overflow = 0;
	for (i = 0; i < 3; i++)
		driver.last_raw[i] = xy1y2[i];
	driver.last_raw[i] = t;
#if YAS532_DRIVER_NO_SLEEP
	rt = measure_start_yas532(0, 0, 0);
	if (rt < 0)
		return rt;
#endif
	return 1;
}

static int yas_measure_wrap(struct yas_data *data, int num)
{
	int ouflow;
	return yas_measure(data, num, 1, &ouflow);
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
	return YAS_NO_ERROR;
}

static int yas_get_enable(void)
{
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	return driver.enable;
}

static int yas_set_enable(int enable)
{
	int rt = YAS_NO_ERROR;
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	enable = !!enable;
	if (driver.enable == enable)
		return YAS_NO_ERROR;
	if (enable) {
		if (driver.cbk.device_open(YAS_TYPE_MAG) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		if (yas_single_write(YAS532_REG_TEST1R, 0x00) < 0) {
			driver.cbk.device_close(YAS_TYPE_MAG);
			return YAS_ERROR_DEVICE_COMMUNICATION;
		}
		if (yas_single_write(YAS532_REG_TEST2R, 0x00) < 0) {
			driver.cbk.device_close(YAS_TYPE_MAG);
			return YAS_ERROR_DEVICE_COMMUNICATION;
		}
		if (yas_single_write(YAS532_REG_RCOILR, 0x00) < 0) {
			driver.cbk.device_close(YAS_TYPE_MAG);
			return YAS_ERROR_DEVICE_COMMUNICATION;
		}
		if (is_valid_offset(driver.hard_offset)) {
			if (yas_cdrv_set_offset(driver.hard_offset) < 0) {
				driver.cbk.device_close(YAS_TYPE_MAG);
				return YAS_ERROR_DEVICE_COMMUNICATION;
			}
			driver.measure_state = YAS532_MAG_STATE_NORMAL;
		} else {
			set_vector(driver.hard_offset, INVALID_OFFSET);
			driver.measure_state = YAS532_MAG_STATE_MEASURE_OFFSET;
		}
	} else {
#if YAS532_DRIVER_NO_SLEEP
		rt = wait_if_busy();
#endif
		driver.cbk.device_close(YAS_TYPE_MAG);
	}
	driver.enable = enable;
	return rt;
}

static int yas_ext(int32_t cmd, void *p)
{
	struct yas532_self_test_result *r;
	struct yas_data data;
	int32_t xy1y2_linear[3], *raw_xyz;
	int16_t *m;
	int rt, i, enable, ouflow, position;
	if (!driver.initialized)
		return YAS_ERROR_INITIALIZE;
	if (p == NULL)
		return YAS_ERROR_ARG;
	switch (cmd) {
	case YAS532_SELF_TEST:
		r = (struct yas532_self_test_result *) p;
		r->id = driver.dev_id;
		enable = driver.enable;
		if (!enable) {
			rt = yas_set_enable(1);
			if (rt < 0)
				return rt;
		}
#if YAS532_DRIVER_NO_SLEEP
		rt = wait_if_busy();
		if (rt < 0)
			return rt;
#endif
		if (yas_single_write(YAS532_REG_RCOILR, 0x00) < 0) {
			if (!enable)
				yas_set_enable(0);
			return YAS_ERROR_DEVICE_COMMUNICATION;
		}
		yas_set_offset(INVALID_OFFSET);
		position = yas_get_position();
		yas_set_position(YAS532_MAG_NOTRANS_POSITION);
		rt = yas_measure(&data, 1, 0, &ouflow);
		yas_set_position(position);
		set_vector(r->xy1y2, driver.hard_offset);
		if (rt < 0) {
			if (!enable)
				yas_set_enable(0);
			return rt;
		}
		if (ouflow & YAS_OVERFLOW) {
			if (!enable)
				yas_set_enable(0);
			return YAS_ERROR_OVERFLOW;
		}
		if (ouflow & YAS_UNDERFLOW) {
			if (!enable)
				yas_set_enable(0);
			return YAS_ERROR_UNDERFLOW;
		}
		if (data.xyz.v[0] == 0 && data.xyz.v[1] == 0
				&& data.xyz.v[2] == 0) {
			if (!enable)
				yas_set_enable(0);
			return YAS_ERROR_DIRCALC;
		}
		r->dir = 99;
		for (i = 0; i < 3; i++)
			r->xyz[i] = data.xyz.v[i] / 1000;
#if YAS532_DRIVER_NO_SLEEP
		rt = wait_if_busy();
		if (rt < 0) {
			if (!enable)
				yas_set_enable(0);
			return rt;
		}
		driver.start_flag = 0;
#endif
		rt = yas_cdrv_sensitivity_measuremnet(&r->sx, &r->sy);
		if (rt < 0) {
			if (!enable)
				yas_set_enable(0);
			return rt;
		}
		if (rt & YAS_OVERFLOW) {
			if (!enable)
				yas_set_enable(0);
			return YAS_ERROR_OVERFLOW;
		}
		if (rt & YAS_UNDERFLOW) {
			if (!enable)
				yas_set_enable(0);
			return YAS_ERROR_UNDERFLOW;
		}
		if (!enable)
			yas_set_enable(0);
		return YAS_NO_ERROR;
	case YAS532_SELF_TEST_NOISE:
		raw_xyz = (int32_t *) p;
		enable = driver.enable;
		if (!enable) {
			rt = yas_set_enable(1);
			if (rt < 0)
				return rt;
		}
#if YAS532_DRIVER_NO_SLEEP
		rt = wait_if_busy();
		if (rt < 0)
			return rt;
#endif
		rt = yas_measure(&data, 1, 0, &ouflow);
		if (rt < 0) {
			if (!enable)
				yas_set_enable(0);
			return rt;
		}
#if YAS532_DRIVER_NO_SLEEP
		rt = wait_if_busy();
		if (rt < 0) {
			if (!enable)
				yas_set_enable(0);
			return rt;
		}
#endif
		xy1y2_to_linear(driver.last_raw, xy1y2_linear);
		raw_xyz[0] = xy1y2_linear[0];
		raw_xyz[1] = xy1y2_linear[1] - xy1y2_linear[2];
		raw_xyz[2] = -xy1y2_linear[1] - xy1y2_linear[2];
		if (!enable)
			yas_set_enable(0);
		return YAS_NO_ERROR;
	case YAS532_GET_HW_OFFSET:
		set_vector((int8_t *) p, driver.hard_offset);
		return YAS_NO_ERROR;
	case YAS532_SET_HW_OFFSET:
		return yas_set_offset((int8_t *) p);
	case YAS532_GET_LAST_RAWDATA:
		for (i = 0; i < 4; i++)
			((uint16_t *) p)[i] = driver.last_raw[i];
		return YAS_NO_ERROR;
	case YAS532_GET_STATIC_MATRIX:
		m = (int16_t *) p;
		for (i = 0; i < 9; i++)
			m[i] = driver.static_matrix.m[i];
		return YAS_NO_ERROR;
	case YAS532_SET_STATIC_MATRIX:
		m = (int16_t *) p;
		for (i = 0; i < 9; i++)
			driver.static_matrix.m[i] = m[i];
		return YAS_NO_ERROR;
	default:
		break;
	}
	return YAS_ERROR_ARG;
}

static int yas_init(void)
{
	int i, rt;
	uint8_t data;
	if (driver.initialized)
		return YAS_ERROR_INITIALIZE;
	if (driver.cbk.device_open(YAS_TYPE_MAG) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (yas_read(YAS532_REG_DEVID, &data, 1) < 0) {
		driver.cbk.device_close(YAS_TYPE_MAG);
		return YAS_ERROR_DEVICE_COMMUNICATION;
	}
	driver.dev_id = data;
	if (driver.dev_id != YAS532_DEVICE_ID) {
		driver.cbk.device_close(YAS_TYPE_MAG);
		return YAS_ERROR_CHIP_ID;
	}
	rt = get_cal_data_yas532(&driver.cal);
	if (rt < 0) {
		driver.cbk.device_close(YAS_TYPE_MAG);
		return rt;
	}
	driver.cbk.device_close(YAS_TYPE_MAG);

	driver.measure_state = YAS532_MAG_STATE_INIT_COIL;
	set_vector(driver.hard_offset, INVALID_OFFSET);
	driver.overflow = 0;
	driver.overflow_time = driver.current_time;
	driver.position = YAS532_MAG_NOTRANS_POSITION;
	driver.delay = YAS_DEFAULT_SENSOR_DELAY;
	driver.enable = 0;
	driver.transform = NULL;
#if YAS532_DRIVER_NO_SLEEP
	driver.start_flag = 0;
	driver.wait_flag = 0;
#endif
#if 1 < YAS532_MAG_TEMPERATURE_LOG
	driver.t.num = driver.t.idx = 0;
#endif
	driver.current_time = curtime();
	for (i = 0; i < 3; i++) {
		driver.coef[i] = yas532_version_ac_coef[i];
		driver.last_raw[i] = 0;
	}
	driver.last_raw[3] = 0;
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
#if !YAS532_DRIVER_NO_SLEEP
			|| f->callback.usleep == NULL
#endif
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

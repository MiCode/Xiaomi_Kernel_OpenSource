/*
 * Copyright (c) 2010-2012 Yamaha Corporation
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
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "yas.h"

struct utimeval {
	int32_t tv_sec;
	int32_t tv_msec;
};

struct utimer {
	struct utimeval prev_time;
	struct utimeval total_time;
	struct utimeval delay_ms;
};

static int utimeval_init(struct utimeval *val);
static int utimeval_is_initial(struct utimeval *val);
static int utimeval_is_overflow(struct utimeval *val);
static struct utimeval utimeval_plus(struct utimeval *first,
		struct utimeval *second);
static struct utimeval utimeval_minus(struct utimeval *first,
		struct utimeval *second);
static int utimeval_greater_than(struct utimeval *first,
		struct utimeval *second);
static int utimeval_greater_or_equal(struct utimeval *first,
		struct utimeval *second);
static int utimeval_greater_than_zero(struct utimeval *val);
static int utimeval_less_than_zero(struct utimeval *val);
static struct utimeval *msec_to_utimeval(struct utimeval *result,
		uint32_t msec);
static uint32_t utimeval_to_msec(struct utimeval *val);

static struct utimeval utimer_calc_next_time(struct utimer *ut,
	struct utimeval *cur);
static struct utimeval utimer_current_time(void);
static int utimer_is_timeout(struct utimer *ut);
static int utimer_clear_timeout(struct utimer *ut);
static uint32_t utimer_get_total_time(struct utimer *ut);
static uint32_t utimer_get_delay(struct utimer *ut);
static int utimer_set_delay(struct utimer *ut, uint32_t delay_ms);
static int utimer_update(struct utimer *ut);
static int utimer_update_with_curtime(struct utimer *ut, struct utimeval *cur);
static uint32_t utimer_sleep_time(struct utimer *ut);
static uint32_t utimer_sleep_time_with_curtime(struct utimer *ut,
		struct utimeval *cur);
static int utimer_init(struct utimer *ut, uint32_t delay_ms);
static int utimer_clear(struct utimer *ut);
static void utimer_lib_init(void (*func)(int *sec, int *msec));

#define YAS_REGADDR_DEVICE_ID		(0x80)
#define YAS_REGADDR_ACTUATE_INIT_COIL	(0x81)
#define YAS_REGADDR_MEASURE_COMMAND	(0x82)
#define YAS_REGADDR_CONFIG		(0x83)
#define YAS_REGADDR_MEASURE_INTERVAL	(0x84)
#define YAS_REGADDR_OFFSET_X		(0x85)
#define YAS_REGADDR_OFFSET_Y1		(0x86)
#define YAS_REGADDR_OFFSET_Y2		(0x87)
#define YAS_REGADDR_TEST1		(0x88)
#define YAS_REGADDR_TEST2		(0x89)
#define YAS_REGADDR_CAL			(0x90)
#define YAS_REGADDR_MEASURE_DATA	(0xb0)
#define YAS_YAS530_DEVICE_ID		(0x01)	/* YAS530  (MS-3E) */
#define YAS_YAS530_VERSION_A		(0)	/* YAS530  (MS-3E Aver) */
#define YAS_YAS530_VERSION_B		(1)	/* YAS530B (MS-3E Bver) */
#define YAS_YAS530_VERSION_A_COEF	(380)
#define YAS_YAS530_VERSION_B_COEF	(550)
#define YAS_YAS530_DATA_CENTER		(2048)
#define YAS_YAS530_DATA_OVERFLOW	(4095)

#define YAS_YAS532_DEVICE_ID		(0x02)	/* YAS532   (MS-3R) */
#define YAS_YAS532_VERSION_AB		(0)	/* YAS532AB (MS-3R ABver) */
#define YAS_YAS532_VERSION_AC		(1)	/* YAS532AC (MS-3R ACver) */
#define YAS_YAS532_VERSION_AB_COEF	(1800)
#define YAS_YAS532_VERSION_AC_COEF	(900)
#define YAS_YAS532_DATA_CENTER		(4096)
#define YAS_YAS532_DATA_OVERFLOW	(8190)

#define YAS_YAS530_CAL_SINGLE_READ

struct yas_machdep_func {
	int (*device_open)(void);
	int (*device_close)(void);
	int (*device_write)(uint8_t slave, uint8_t addr, const uint8_t *buf, int len);
	int (*device_read)(uint8_t slave, uint8_t addr, uint8_t *buf, int len);
	void (*msleep)(int msec);
};

static int yas_cdrv_actuate_initcoil(void);
static int yas_cdrv_set_offset(const int8_t *offset);
static int yas_cdrv_recalc_calib_offset(int32_t *prev_calib_offset,
		int32_t *new_calib_offset, int8_t *prev_offset,
		int8_t *new_offset);
static int yas_cdrv_set_transformatiom_matrix(const int8_t *transform);
static int yas_cdrv_measure_and_set_offset(int8_t *offset);
static int yas_cdrv_measure(int32_t *msens, int32_t *raw, int16_t *t);
static int yas_cdrv_init(const int8_t *transform,
		struct yas_machdep_func *func);
static int yas_cdrv_term(void);

static void (*current_time)(int *sec, int *msec);

static int
utimeval_init(struct utimeval *val)
{
	if (val == NULL)
		return -1;
	val->tv_sec = val->tv_msec = 0;
	return 0;
}

static int
utimeval_is_initial(struct utimeval *val)
{
	if (val == NULL)
		return 0;
	return val->tv_sec == 0 && val->tv_msec == 0;
}

static int
utimeval_is_overflow(struct utimeval *val)
{
	int32_t max;

	if (val == NULL)
		return 0;

	max = (int32_t) ((uint32_t) 0xffffffff / (uint32_t) 1000);
	if (val->tv_sec > max) {
		return 1; /* overflow */
	} else if (val->tv_sec == max) {
		if (val->tv_msec > (int32_t)((uint32_t)0xffffffff
					% (uint32_t)1000))
			return 1; /* overflow */
	}

	return 0;
}

static struct utimeval
utimeval_plus(struct utimeval *first, struct utimeval *second)
{
	struct utimeval result = {0, 0};
	int32_t tmp;

	if (first == NULL || second == NULL)
		return result;

	tmp = first->tv_sec + second->tv_sec;
	if (first->tv_sec >= 0 && second->tv_sec >= 0 && tmp < 0)
		goto overflow;
	if (first->tv_sec < 0 && second->tv_sec < 0 && tmp >= 0)
		goto underflow;

	result.tv_sec = tmp;
	result.tv_msec = first->tv_msec + second->tv_msec;
	if (1000 <= result.tv_msec) {
		tmp = result.tv_sec + result.tv_msec / 1000;
		if (result.tv_sec >= 0 && result.tv_msec >= 0 && tmp < 0)
			goto overflow;
		result.tv_sec = tmp;
		result.tv_msec = result.tv_msec % 1000;
	}
	if (result.tv_msec < 0) {
		tmp = result.tv_sec + result.tv_msec / 1000 - 1;
		if (result.tv_sec < 0 && result.tv_msec < 0 && tmp >= 0)
			goto underflow;
		result.tv_sec = tmp;
		result.tv_msec = result.tv_msec % 1000 + 1000;
	}

	return result;

overflow:
	result.tv_sec = (int32_t)0x7fffffff;
	result.tv_msec = 999;
	return result;

underflow:
	result.tv_sec = (int32_t)0x80000000;
	result.tv_msec = 0;
	return result;
}

static struct utimeval
utimeval_minus(struct utimeval *first, struct utimeval *second)
{
	struct utimeval result = {0, 0}, tmp;

	if (first == NULL || second == NULL
			|| second->tv_sec == (int)0x80000000)
		return result;

	tmp.tv_sec = -second->tv_sec;
	tmp.tv_msec = -second->tv_msec;
	return utimeval_plus(first, &tmp);
}

static int
utimeval_less_than(struct utimeval *first, struct utimeval *second)
{
	if (first == NULL || second == NULL)
		return 0;

	if (first->tv_sec > second->tv_sec) {
		return 1;
	} else if (first->tv_sec < second->tv_sec) {
		return 0;
	} else {
		if (first->tv_msec > second->tv_msec)
			return 1;
		else
			return 0;
	}
}

static int
utimeval_greater_than(struct utimeval *first, struct utimeval *second)
{
	if (first == NULL || second == NULL)
		return 0;

	if (first->tv_sec < second->tv_sec) {
		return 1;
	} else if (first->tv_sec > second->tv_sec) {
		return 0;
	} else {
		if (first->tv_msec < second->tv_msec)
			return 1;
		else
			return 0;
	}
}

static int
utimeval_greater_or_equal(struct utimeval *first,
						 struct utimeval *second)
{
	return !utimeval_less_than(first, second);
}

static int
utimeval_greater_than_zero(struct utimeval *val)
{
	struct utimeval zero = {0, 0};

	return utimeval_greater_than(&zero, val);
}

static int
utimeval_less_than_zero(struct utimeval *val)
{
	struct utimeval zero = {0, 0};

	return utimeval_less_than(&zero, val);
}

static struct utimeval *
msec_to_utimeval(struct utimeval *result, uint32_t msec)
{
	if (result == NULL)
		return result;
	result->tv_sec = (int32_t)(msec / 1000);
	result->tv_msec = (int32_t)(msec % 1000);

	return result;
}

static uint32_t
utimeval_to_msec(struct utimeval *val)
{
	if (val == NULL)
		return 0;
	if (utimeval_less_than_zero(val))
		return 0;
	if (utimeval_is_overflow(val))
		return 0xffffffff;

	return (uint32_t)(val->tv_sec * 1000 + val->tv_msec);
}

static struct utimeval
utimer_calc_next_time(struct utimer *ut, struct utimeval *cur)
{
	struct utimeval result = {0, 0}, delay;

	if (ut == NULL || cur == NULL)
		return result;

	utimer_update_with_curtime(ut, cur);
	if (utimer_is_timeout(ut)) {
		result = *cur;
	} else {
		delay = utimeval_minus(&ut->delay_ms, &ut->total_time);
		result = utimeval_plus(cur, &delay);
	}

	return result;
}

static struct utimeval
utimer_current_time(void)
{
	struct utimeval tv;
	int sec, msec;

	if (current_time != NULL)
		current_time(&sec, &msec);
	else
		sec = 0, msec = 0;

	tv.tv_sec = sec;
	tv.tv_msec = msec;

	return tv;
}

static int
utimer_clear(struct utimer *ut)
{
	if (ut == NULL)
		return -1;
	utimeval_init(&ut->prev_time);
	utimeval_init(&ut->total_time);

	return 0;
}

static int
utimer_update_with_curtime(struct utimer *ut, struct utimeval *cur)
{
	struct utimeval tmp;

	if (ut == NULL || cur == NULL)
		return -1;
	if (utimeval_is_initial(&ut->prev_time))
		ut->prev_time = *cur;
	if (utimeval_greater_than_zero(&ut->delay_ms)) {
		tmp = utimeval_minus(cur, &ut->prev_time);
		if (utimeval_less_than_zero(&tmp)) {
			utimeval_init(&ut->total_time);
		} else {
			ut->total_time = utimeval_plus(&tmp, &ut->total_time);
			if (utimeval_is_overflow(&ut->total_time))
				utimeval_init(&ut->total_time);
		}
		ut->prev_time = *cur;
	}

	return 0;
}

static int
utimer_update(struct utimer *ut)
{
	struct utimeval cur;

	if (ut == NULL)
		return -1;
	cur = utimer_current_time();
	utimer_update_with_curtime(ut, &cur);
	return 0;
}

static int
utimer_is_timeout(struct utimer *ut)
{
	if (ut == NULL)
		return 0;
	if (utimeval_greater_than_zero(&ut->delay_ms))
		return utimeval_greater_or_equal(&ut->delay_ms,
				&ut->total_time);
	else
		return 1;
}

static int
utimer_clear_timeout(struct utimer *ut)
{
	uint32_t delay, total;

	if (ut == NULL)
		return -1;

	delay = utimeval_to_msec(&ut->delay_ms);
	if (delay == 0 || utimeval_is_overflow(&ut->total_time)) {
		total = 0;
	} else {
		if (utimeval_is_overflow(&ut->total_time)) {
			total = 0;
		} else {
			total = utimeval_to_msec(&ut->total_time);
			total = total % delay;
		}
	}
	msec_to_utimeval(&ut->total_time, total);

	return 0;
}

static uint32_t
utimer_sleep_time_with_curtime(struct utimer *ut, struct utimeval *cur)
{
	struct utimeval tv;

	if (ut == NULL || cur == NULL)
		return 0;
	tv = utimer_calc_next_time(ut, cur);
	tv = utimeval_minus(&tv, cur);
	if (utimeval_less_than_zero(&tv))
		return 0;

	return utimeval_to_msec(&tv);
}

static uint32_t
utimer_sleep_time(struct utimer *ut)
{
	struct utimeval cur;

	if (ut == NULL)
		return 0;

	cur = utimer_current_time();
	return utimer_sleep_time_with_curtime(ut, &cur);
}

static int
utimer_init(struct utimer *ut, uint32_t delay_ms)
{
	if (ut == NULL)
		return -1;
	utimer_clear(ut);
	msec_to_utimeval(&ut->delay_ms, delay_ms);

	return 0;
}

static uint32_t
utimer_get_total_time(struct utimer *ut)
{
	return utimeval_to_msec(&ut->total_time);
}

static uint32_t
utimer_get_delay(struct utimer *ut)
{
	if (ut == NULL)
		return 0;
	return utimeval_to_msec(&ut->delay_ms);
}

static int
utimer_set_delay(struct utimer *ut, uint32_t delay_ms)
{
	return utimer_init(ut, delay_ms);
}

static void
utimer_lib_init(void (*func)(int *sec, int *msec))
{
	current_time = func;
}

struct yas_cal_data {
	uint8_t dx, dy1, dy2;
	uint8_t d2, d3, d4, d5, d6, d7, d8, d9, d0;
	uint8_t dck;
	uint8_t ver;
};
struct yas_correction_data {
	int32_t Cx, Cy1, Cy2;
	int32_t a2, a3, a4, a5, a6, a7, a8, a9, k;
};
struct yas_cdriver {
	struct yas_cal_data cal;
	struct yas_correction_data correct;
	struct yas_machdep_func func;
	int8_t transform[9];
	int16_t temperature;
	uint8_t dev_id;
	int32_t coef;
	int16_t center;
	int16_t overflow;
};
static struct yas_cdriver cdriver;

static int
device_open(void)
{
	if (cdriver.func.device_open == NULL)
		return -1;
	return cdriver.func.device_open();
}

static int
device_close(void)
{
	if (cdriver.func.device_close == NULL)
		return -1;
	return cdriver.func.device_close();
}

static int
device_write(uint8_t addr, const uint8_t *buf, int len)
{
	if (cdriver.func.device_write == NULL)
		return -1;
	return cdriver.func.device_write(YAS_MAG_I2C_SLAVEADDR, addr, buf, len);
}

static int
device_read(uint8_t addr, uint8_t *buf, int len)
{
	if (cdriver.func.device_read == NULL)
		return -1;
	return cdriver.func.device_read(YAS_MAG_I2C_SLAVEADDR, addr, buf, len);
}

static void
sleep(int millisec)
{
	if (cdriver.func.msleep == NULL)
		return;
	cdriver.func.msleep(millisec);
}

static int
init_test_register(void)
{
	uint8_t data;

	data = 0x00;
	if (device_write(YAS_REGADDR_TEST1, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	data = 0x00;
	if (device_write(YAS_REGADDR_TEST2, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	return YAS_NO_ERROR;
}

static int
get_device_id(uint8_t *id)
{
	uint8_t data = 0;

	if (device_read(YAS_REGADDR_DEVICE_ID, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	*id = data;

	return YAS_NO_ERROR;
}

static int
get_cal_data_yas530(struct yas_cal_data *cal)
{
	uint8_t data[16];
#ifdef YAS_YAS530_CAL_SINGLE_READ
	int i;

	for (i = 0; i < 16; i++) { /* dummy read */
		if (device_read(YAS_REGADDR_CAL + i, &data[i], 1) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
	}
	for (i = 0; i < 16; i++) {
		if (device_read(YAS_REGADDR_CAL + i, &data[i], 1) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
	}
#else
	if (device_read(YAS_REGADDR_CAL, data, 16) < 0) /* dummy read */
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (device_read(YAS_REGADDR_CAL, data, 16) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
#endif

	cal->dx = data[0];
	cal->dy1 = data[1];
	cal->dy2 = data[2];
	cal->d2 = (data[3]>>2) & 0x03f;
	cal->d3 = (uint8_t)(((data[3]<<2) & 0x0c) | ((data[4]>>6) & 0x03));
	cal->d4 = (uint8_t)(data[4] & 0x3f);
	cal->d5 = (data[5]>>2) & 0x3f;
	cal->d6 = (uint8_t)(((data[5]<<4) & 0x30) | ((data[6]>>4) & 0x0f));
	cal->d7 = (uint8_t)(((data[6]<<3) & 0x78) | ((data[7]>>5) & 0x07));
	cal->d8 = (uint8_t)(((data[7]<<1) & 0x3e) | ((data[8]>>7) & 0x01));
	cal->d9 = (uint8_t)(((data[8]<<1) & 0xfe) | ((data[9]>>7) & 0x01));
	cal->d0 = (uint8_t)((data[9]>>2) & 0x1f);
	cal->dck = (uint8_t)(((data[9]<<1) & 0x06) | ((data[10]>>7) & 0x01));
	cal->ver = (uint8_t)((data[15]) & 0x03);

	return YAS_NO_ERROR;
}

static void
get_correction_value_yas530(struct yas_cal_data *cal,
		struct yas_correction_data *correct)
{
	correct->Cx = cal->dx * 6 - 768;
	correct->Cy1 = cal->dy1 * 6 - 768;
	correct->Cy2 = cal->dy2 * 6 - 768;
	correct->a2 = cal->d2 - 32;
	correct->a3 = cal->d3 - 8;
	correct->a4 = cal->d4 - 32;
	correct->a5 = cal->d5 + 38;
	correct->a6 = cal->d6 - 32;
	correct->a7 = cal->d7 - 64;
	correct->a8 = cal->d8 - 32;
	correct->a9 = cal->d9;
	correct->k = cal->d0 + 10;
}

static int
get_cal_data_yas532(struct yas_cal_data *cal)
{
	uint8_t data[14];
#ifdef YAS_YAS530_CAL_SINGLE_READ
	int i;

	for (i = 0; i < 14; i++) { /* dummy read */
		if (device_read(YAS_REGADDR_CAL + i, &data[i], 1) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
	}
	for (i = 0; i < 14; i++) {
		if (device_read(YAS_REGADDR_CAL + i, &data[i], 1) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
	}
#else
	if (device_read(YAS_REGADDR_CAL, data, 14) < 0) /* dummy read */
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (device_read(YAS_REGADDR_CAL, data, 14) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
#endif

	cal->dx = data[0];
	cal->dy1 = data[1];
	cal->dy2 = data[2];
	cal->d2 = (data[3]>>2) & 0x03f;
	cal->d3 = (uint8_t)(((data[3]<<2) & 0x0c) | ((data[4]>>6) & 0x03));
	cal->d4 = (uint8_t)(data[4] & 0x3f);
	cal->d5 = (data[5]>>2) & 0x3f;
	cal->d6 = (uint8_t)(((data[5]<<4) & 0x30) | ((data[6]>>4) & 0x0f));
	cal->d7 = (uint8_t)(((data[6]<<3) & 0x78) | ((data[7]>>5) & 0x07));
	cal->d8 = (uint8_t)(((data[7]<<1) & 0x3e) | ((data[8]>>7) & 0x01));
	cal->d9 = (uint8_t)(((data[8]<<1) & 0xfe) | ((data[9]>>7) & 0x01));
	cal->d0 = (uint8_t)((data[9]>>2) & 0x1f);
	cal->dck = (uint8_t)(((data[9]<<1) & 0x06) | ((data[10]>>7) & 0x01));
	cal->ver = (uint8_t)((data[13]) & 0x01);

	return YAS_NO_ERROR;
}

static void
get_correction_value_yas532(struct yas_cal_data *cal,
		struct yas_correction_data *correct)
{
	correct->Cx = cal->dx * 10 - 1280;
	correct->Cy1 = cal->dy1 * 10 - 1280;
	correct->Cy2 = cal->dy2 * 10 - 1280;
	correct->a2 = cal->d2 - 32;
	correct->a3 = cal->d3 - 8;
	correct->a4 = cal->d4 - 32;
	correct->a5 = cal->d5 + 38;
	correct->a6 = cal->d6 - 32;
	correct->a7 = cal->d7 - 64;
	correct->a8 = cal->d8 - 32;
	correct->a9 = cal->d9;
	correct->k = cal->d0;
}

static int
set_configuration(int inton, int inthact, int cck)
{
	uint8_t data = 0;

	data = (uint8_t)(data | ((!!inton) & 0x01));
	data = (uint8_t)(data | (((!!inthact)<<1) & 0x02));
	data = (uint8_t)(data | ((cck<<2) & 0x1c));

	if (device_write(YAS_REGADDR_CONFIG, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	return YAS_NO_ERROR;
}

static int
get_measure_interval(int32_t *msec)
{
	uint8_t data;
	int mult = 7;

	if (device_read(YAS_REGADDR_MEASURE_INTERVAL, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	switch (cdriver.dev_id) {
	case YAS_YAS532_DEVICE_ID:
		mult = 4;
		break;
	case YAS_YAS530_DEVICE_ID:
	default:
		mult = 7;
		break;
	}

	*msec = data * mult;

	return YAS_NO_ERROR;
}

static int
set_measure_interval(int32_t msec)
{
	uint8_t data = 0;
	int mult = 7;

	switch (cdriver.dev_id) {
	case YAS_YAS532_DEVICE_ID:
		mult = 4;
		break;
	case YAS_YAS530_DEVICE_ID:
	default:
		mult = 7;
		break;
	}

	if (msec > mult*0xff)
		data = 0xff;
	else
		if (msec % mult == 0)
			data = (uint8_t)(msec / mult);
		else
			data = (uint8_t)(msec / mult + 1);
	if (device_write(YAS_REGADDR_MEASURE_INTERVAL, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	return YAS_NO_ERROR;
}

static int
set_measure_command(int ldtc, int fors, int dlymes)
{
	uint8_t data = 0;

	data = (uint8_t)(data | 0x01);
	data = (uint8_t)(data | (((!!ldtc)<<1) & 0x02));
	data = (uint8_t)(data | (((!!fors)<<2) & 0x04));
	data = (uint8_t)(data | (((!!dlymes)<<4) & 0x10));

	if (device_write(YAS_REGADDR_MEASURE_COMMAND, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	return YAS_NO_ERROR;
}

static int
measure_normal_yas530(int *busy, int16_t *t,
		int16_t *x, int16_t *y1, int16_t *y2)
{
	uint8_t data[8];

	if (set_measure_command(0, 0, 0) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	sleep(2);

	if (device_read(YAS_REGADDR_MEASURE_DATA, data, 8) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	*busy = (data[0]>>7) & 0x01;
	*t = (int16_t)((((int32_t)data[0]<<2) & 0x1fc)
			| ((data[1]>>6) & 0x03));
	*x = (int16_t)((((int32_t)data[2]<<5) & 0xfe0)
			| ((data[3]>>3) & 0x1f));
	*y1 = (int16_t)((((int32_t)data[4]<<5) & 0xfe0)
			| ((data[5]>>3) & 0x1f));
	*y2 = (int16_t)((((int32_t)data[6]<<5) & 0xfe0)
			| ((data[7]>>3) & 0x1f));
	/*YLOGD(("f[%d] t[%d] x[%d] y1[%d] y2[%d]\n",
		*busy, *t, *x, *y1, *y2));*/

	return YAS_NO_ERROR;
}

static int
measure_normal_yas532(int *busy, int16_t *t,
		int16_t *x, int16_t *y1, int16_t *y2)
{
	uint8_t data[8];

	if (set_measure_command(0, 0, 0) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	sleep(2);

	if (device_read(YAS_REGADDR_MEASURE_DATA, data, 8) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	*busy = (data[0]>>7) & 0x01;
	*t = (int16_t)((((int32_t)data[0]<<3) & 0x3f8)
			| ((data[1]>>5) & 0x07));
	*x = (int16_t)((((int32_t)data[2]<<6) & 0x1fc0)
			| ((data[3]>>2) & 0x3f));
	*y1 = (int16_t)((((int32_t)data[4]<<6) & 0x1fc0)
			| ((data[5]>>2) & 0x3f));
	*y2 = (int16_t)((((int32_t)data[6]<<6) & 0x1fc0)
			| ((data[7]>>2) & 0x3f));
	/*YLOGD(("f[%d] t[%d] x[%d] y1[%d] y2[%d]\n",
		*busy, *t, *x, *y1, *y2));*/

	return YAS_NO_ERROR;
}

static int
measure_normal(int *busy, int16_t *t, int16_t *x, int16_t *y1, int16_t *y2)
{
	int result;

	switch (cdriver.dev_id) {
	case YAS_YAS532_DEVICE_ID:
		result = measure_normal_yas532(busy, t, x, y1, y2);
		break;
	case YAS_YAS530_DEVICE_ID:
	default:
		result = measure_normal_yas530(busy, t, x, y1, y2);
		break;
	}

	return result;
}

static int
coordinate_conversion(int32_t x, int32_t y1, int32_t y2, int16_t t,
		int32_t *xo, int32_t *yo, int32_t *zo,
		struct yas_correction_data *c)
{
	int32_t sx, sy1, sy2, sy, sz;
	int32_t hx, hy, hz;

	sx  = x  - (c->Cx  * t) / 100;
	sy1 = y1 - (c->Cy1 * t) / 100;
	sy2 = y2 - (c->Cy2 * t) / 100;

	sy = sy1 - sy2;
	sz = -sy1 - sy2;

	hx = c->k * ((100   * sx + c->a2 * sy + c->a3 * sz) / 10);
	hy = c->k * ((c->a4 * sx + c->a5 * sy + c->a6 * sz) / 10);
	hz = c->k * ((c->a7 * sx + c->a8 * sy + c->a9 * sz) / 10);

	*xo = cdriver.transform[0] * hx
			+ cdriver.transform[1] * hy
			+ cdriver.transform[2] * hz;
	*yo = cdriver.transform[3] * hx
			+ cdriver.transform[4] * hy
			+ cdriver.transform[5] * hz;
	*zo = cdriver.transform[6] * hx
			+ cdriver.transform[7] * hy
			+ cdriver.transform[8] * hz;

	return YAS_NO_ERROR;
}

static int
set_hardware_offset(int8_t offset_x, int8_t offset_y1, int8_t offset_y2)
{
	uint8_t data;

	data = (uint8_t)(offset_x & 0x3f);
	if (device_write(YAS_REGADDR_OFFSET_X, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	data = (uint8_t)(offset_y1 & 0x3f);
	if (device_write(YAS_REGADDR_OFFSET_Y1, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	data = (uint8_t)(offset_y2 & 0x3f);
	if (device_write(YAS_REGADDR_OFFSET_Y2, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	return YAS_NO_ERROR;
}

static int
yas_cdrv_actuate_initcoil(void)
{
	uint8_t data = 0;

	if (device_write(YAS_REGADDR_ACTUATE_INIT_COIL, &data, 1) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	return YAS_NO_ERROR;
}

static int
check_offset(int8_t offset_x, int8_t offset_y1, int8_t offset_y2,
		int *flag_x, int *flag_y1, int *flag_y2)
{
	int busy;
	int16_t t, x, y1, y2;

	if (set_hardware_offset(offset_x, offset_y1, offset_y2) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (measure_normal(&busy, &t, &x, &y1, &y2) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	*flag_x = *flag_y1 = *flag_y2 = 0;
	if (x  > cdriver.center)
		*flag_x  =  1;
	if (y1 > cdriver.center)
		*flag_y1 =  1;
	if (y2 > cdriver.center)
		*flag_y2 =  1;
	if (x  < cdriver.center)
		*flag_x  = -1;
	if (y1 < cdriver.center)
		*flag_y1 = -1;
	if (y2 < cdriver.center)
		*flag_y2 = -1;

	return YAS_NO_ERROR;
}

static int
yas_cdrv_measure_and_set_offset(int8_t *offset)
{
	int i;
	int8_t offset_x = 0, offset_y1 = 0, offset_y2 = 0;
	int flag_x = 0, flag_y1 = 0, flag_y2 = 0;
	static const int correct[5] = {16, 8, 4, 2, 1};

	for (i = 0; i < 5; i++) {
		if (check_offset(offset_x, offset_y1, offset_y2,
					&flag_x, &flag_y1, &flag_y2) < 0) {
			return YAS_ERROR_DEVICE_COMMUNICATION;
		}
		YLOGD(("offset[%d][%d][%d] flag[%d][%d][%d]\n",
				offset_x, offset_y1, offset_y2,
				flag_x, flag_y1, flag_y2));
		if (flag_x)
			offset_x  = (int8_t)(offset_x + flag_x * correct[i]);
		if (flag_y1)
			offset_y1 = (int8_t)(offset_y1 + flag_y1 * correct[i]);
		if (flag_y2)
			offset_y2 = (int8_t)(offset_y2 + flag_y2 * correct[i]);
	}
	if (set_hardware_offset(offset_x, offset_y1, offset_y2) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	offset[0] = offset_x;
	offset[1] = offset_y1;
	offset[2] = offset_y2;

	return YAS_NO_ERROR;
}

static int
yas_cdrv_set_offset(const int8_t *offset)
{
	if (set_hardware_offset(offset[0], offset[1], offset[2]) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	return YAS_NO_ERROR;
}

static int
yas_cdrv_measure(int32_t *data, int32_t *raw, int16_t *temperature)
{
	int busy;
	int16_t x, y1, y2, t;
	int32_t xx, yy1, yy2;
	int result = 0;

	if (measure_normal(&busy, &t, &x, &y1, &y2) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;

	if (x == 0)
		result |= 0x01;
	if (x == cdriver.overflow)
		result |= 0x02;
	if (y1 == 0)
		result |= 0x04;
	if (y1 == cdriver.overflow)
		result |= 0x08;
	if (y2 == 0)
		result |= 0x10;
	if (y2 == cdriver.overflow)
		result |= 0x20;

	xx = x;
	yy1 = y1;
	yy2 = y2;

	if (coordinate_conversion(xx, yy1, yy2, t, &data[0], &data[1],
				&data[2], &cdriver.correct) < 0) {
		return YAS_ERROR_DEVICE_COMMUNICATION;
	}
	cdriver.temperature = t;

	if (raw != NULL)
		raw[0] = xx, raw[1] = yy1, raw[2] = yy2;
	if (temperature != NULL)
		*temperature = t;

	return result;
}

static int
yas_cdrv_recalc_calib_offset(int32_t *prev_calib_offset,
		int32_t *new_calib_offset, int8_t *prev_offset,
		int8_t *new_offset)
{
	int32_t tmp[3], resolution[9], base[3];
	int32_t raw[3];
	int32_t diff, i;

	if (prev_calib_offset == NULL || new_calib_offset == NULL
			|| prev_offset == NULL || new_offset == NULL) {
		return YAS_ERROR_ARG;
	}

	raw[0] = raw[1] = raw[2] = 0;
	if (coordinate_conversion(raw[0], raw[1], raw[2], cdriver.temperature,
				&base[0], &base[1], &base[2],
				&cdriver.correct) < 0) {
		return YAS_ERROR_ERROR;
	}
	for (i = 0; i < 3; i++) {
		raw[0] = raw[1] = raw[2] = 0;
		raw[i] = cdriver.coef;
		if (coordinate_conversion(raw[0], raw[1], raw[2],
					cdriver.temperature,
					&resolution[i*3 + 0],
					&resolution[i*3 + 1],
					&resolution[i*3 + 2],
					&cdriver.correct) < 0) {
			return YAS_ERROR_ERROR;
		}
		resolution[i*3 + 0] -= base[0];
		resolution[i*3 + 1] -= base[1];
		resolution[i*3 + 2] -= base[2];
	}

	for (i = 0; i < 3; i++)
		tmp[i] = prev_calib_offset[i];
	for (i = 0; i < 3; i++) {
		diff = (int32_t)new_offset[i] - (int32_t)prev_offset[i];
		while (diff > 0) {
			tmp[0] -= resolution[i*3 + 0];
			tmp[1] -= resolution[i*3 + 1];
			tmp[2] -= resolution[i*3 + 2];
			diff--;
		}
		while (diff < 0) {
			tmp[0] += resolution[i*3 + 0];
			tmp[1] += resolution[i*3 + 1];
			tmp[2] += resolution[i*3 + 2];
			diff++;
		}
	}
	for (i = 0; i < 3; i++)
		new_calib_offset[i] = tmp[i];

	return YAS_NO_ERROR;
}

static int
yas_cdrv_set_transformatiom_matrix(const int8_t *transform)
{
	int i;

	if (transform == NULL)
		return YAS_ERROR_ARG;
	for (i = 0; i < 9; i++)
		cdriver.transform[i] = transform[i];

	return YAS_NO_ERROR;
}

static int
yas_cdrv_init(const int8_t *transform, struct yas_machdep_func *func)
{
	int interval, i;
	uint8_t id;

	if (transform == NULL || func == NULL)
		return YAS_ERROR_ARG;

	for (i = 0; i < 9; i++)
		cdriver.transform[i] = transform[i];
	cdriver.func = *func;

	if (device_open() < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (init_test_register() < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (get_device_id(&id) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	YLOGD(("device id:%02x\n", id));

	switch (id) {
	case YAS_YAS530_DEVICE_ID:
		if (get_cal_data_yas530(&cdriver.cal) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		get_correction_value_yas530(&cdriver.cal, &cdriver.correct);
		cdriver.center = YAS_YAS530_DATA_CENTER;
		cdriver.overflow = YAS_YAS530_DATA_OVERFLOW;
		switch (cdriver.cal.ver) {
		case YAS_YAS530_VERSION_B:
			cdriver.coef = YAS_YAS530_VERSION_B_COEF;
			break;
		case YAS_YAS530_VERSION_A:
		default:
			cdriver.coef = YAS_YAS530_VERSION_A_COEF;
			break;
		}
		break;

	case YAS_YAS532_DEVICE_ID:
		if (get_cal_data_yas532(&cdriver.cal) < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
		get_correction_value_yas532(&cdriver.cal, &cdriver.correct);
		cdriver.center = YAS_YAS532_DATA_CENTER;
		cdriver.overflow = YAS_YAS532_DATA_OVERFLOW;
		switch (cdriver.cal.ver) {
		case YAS_YAS532_VERSION_AC:
			cdriver.coef = YAS_YAS532_VERSION_AC_COEF;
			break;
		case YAS_YAS532_VERSION_AB:
		default:
			cdriver.coef = YAS_YAS532_VERSION_AB_COEF;
			break;
		}
		break;

	default:
		return YAS_ERROR_DEVICE_COMMUNICATION;
	}
	cdriver.dev_id = id;
	YLOGD(("%s %s\n",
				cdriver.dev_id == YAS_YAS532_DEVICE_ID
				? "MS-3R" : "MS-3E",
				cdriver.cal.ver == YAS_YAS532_VERSION_AC
				? "ACver" : "ABver"));
	YLOGD(("dx[%d] dy1[%d] dy2[%d] d2[%d] d3[%d] d4[%d] d5[%d] d6[%d] "
				"d7[%d] d8[%d] d9[%d] d0[%d] dck[%d] "
				"ver[%d]\n",
			cdriver.cal.dx, cdriver.cal.dy1, cdriver.cal.dy2,
			cdriver.cal.d2, cdriver.cal.d3, cdriver.cal.d4,
			cdriver.cal.d5, cdriver.cal.d6, cdriver.cal.d7,
			cdriver.cal.d8, cdriver.cal.d9, cdriver.cal.d0,
			cdriver.cal.dck, cdriver.cal.ver));
	YLOGD(("Cx[%4d] Cy1[%4d] Cy2[%4d] a2[%4d] a3[%4d] a4[%4d] a5[%4d] "
				"a6[%4d] a7[%4d] a8[%4d] a9[%4d] k[%4d]\n",
			cdriver.correct.Cx, cdriver.correct.Cy1,
			cdriver.correct.Cy2, cdriver.correct.a2,
			cdriver.correct.a3, cdriver.correct.a4,
			cdriver.correct.a5, cdriver.correct.a6,
			cdriver.correct.a7, cdriver.correct.a8,
			cdriver.correct.a9, cdriver.correct.k));
	if (set_configuration(0, 0, cdriver.cal.dck) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (set_measure_interval(0) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (get_measure_interval(&interval) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	YLOGD(("interval[%d]\n", interval));

	return YAS_NO_ERROR;
}

static int
yas_cdrv_term(void)
{
	device_close();
	return YAS_NO_ERROR;
}

#define YAS_DEFAULT_CALIB_INTERVAL	(50)    /* 50 msecs */
#define YAS_DEFAULT_DATA_INTERVAL	(200)   /* 200 msecs */
#define YAS_INITCOIL_INTERVAL		(3000)  /* 3 seconds */
#define YAS_INITCOIL_GIVEUP_INTERVAL	(180000) /* 180 seconds */
#define YAS_DETECT_OVERFLOW_INTERVAL	(0)	   /* 0 second */

#define YAS_MAG_ERROR_DELAY		(200)
#define YAS_MAG_STATE_NORMAL		(0)
#define YAS_MAG_STATE_INIT_COIL		(1)
#define YAS_MAG_STATE_MEASURE_OFFSET	(2)

static const int8_t YAS_TRANSFORMATION[][9] = {
	{ 0,  1,  0, -1,  0,  0,  0,  0,  1 },
	{-1,  0,  0,  0, -1,  0,  0,  0,  1 },
	{ 0, -1,  0,  1,  0,  0,  0,  0,  1 },
	{ 1,  0,  0,  0,  1,  0,  0,  0,  1 },
	{ 0, -1,  0, -1,  0,  0,  0,  0, -1 },
	{ 1,  0,  0,  0, -1,  0,  0,  0, -1 },
	{ 0,  1,  0,  1,  0,  0,  0,  0, -1 },
	{-1,  0,  0,  0,  1,  0,  0,  0, -1 },
};

static const int supported_data_interval[] = {10, 20, 50, 60, 100, 200, 1000};
static const int supported_calib_interval[] = {60, 60, 50, 60, 50, 50, 50};
static const int32_t INVALID_CALIB_OFFSET[]
	= {0x7fffffff, 0x7fffffff, 0x7fffffff};
static const int8_t INVALID_OFFSET[] = {0x7f, 0x7f, 0x7f};

struct yas_adaptive_filter {
	int num;
	int index;
	int filter_len;
	int filter_noise;
	int32_t sequence[YAS_MAG_MAX_FILTER_LEN];
};

struct yas_thresh_filter {
	int32_t threshold;
	int32_t last;
};

struct yas_driver {
	int initialized;
	struct yas_mag_driver_callback callback;
	struct utimer data_timer;
	struct utimer initcoil_timer;
	struct utimer initcoil_giveup_timer;
	struct utimer detect_overflow_timer;
	int32_t prev_mag[3];
	int32_t prev_xy1y2[3];
	int32_t prev_mag_w_offset[3];
	int16_t prev_temperature;
	int measure_state;
	int active;
	int overflow;
	int initcoil_gaveup;
	int position;
	int delay_timer_use_data;
	int delay_timer_interval;
	int delay_timer_counter;
	int filter_enable;
	int filter_len;
	int filter_thresh;
	int filter_noise[3];
	struct yas_adaptive_filter adap_filter[3];
	struct yas_thresh_filter thresh_filter[3];
	struct yas_mag_offset offset;
#ifdef YAS_MAG_MANUAL_OFFSET
	struct yas_vector manual_offset;
#endif
	struct yas_matrix static_matrix;
	struct yas_matrix dynamic_matrix;
};

static struct yas_driver this_driver;

static int
lock(void)
{
	if (this_driver.callback.lock != NULL) {
		if (this_driver.callback.lock() < 0)
			return YAS_ERROR_RESTARTSYS;
	}
	return 0;
}

static int
unlock(void)
{
	if (this_driver.callback.unlock != NULL) {
		if (this_driver.callback.unlock() < 0)
			return YAS_ERROR_RESTARTSYS;
	}
	return 0;
}

static int32_t
square(int32_t data)
{
	return data * data;
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

static int32_t
adaptive_filter_filter(struct yas_adaptive_filter *adap_filter, int32_t in)
{
	int32_t avg, sum;
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

static void
thresh_filter_init(struct yas_thresh_filter *thresh_filter, int threshold)
{
	thresh_filter->threshold = threshold;
	thresh_filter->last = 0;
}

static int32_t
thresh_filter_filter(struct yas_thresh_filter *thresh_filter, int32_t in)
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
filter_init(struct yas_driver *d)
{
	int i;

	for (i = 0; i < 3; i++) {
		adaptive_filter_init(&d->adap_filter[i], d->filter_len,
				d->filter_noise[i]);
		thresh_filter_init(&d->thresh_filter[i], d->filter_thresh);
	}
}

static void
filter_filter(struct yas_driver *d, int32_t *orig, int32_t *filtered)
{
	int i;

	for (i = 0; i < 3; i++) {
		filtered[i] = adaptive_filter_filter(&d->adap_filter[i],
				orig[i]);
		filtered[i] = thresh_filter_filter(&d->thresh_filter[i],
				filtered[i]);
	}
}

static int
is_valid_offset(const int8_t *p)
{
	return p != NULL && (p[0] <= 31) && (p[1] <= 31) && (p[2] <= 31)
		&& (-31 <= p[0]) && (-31 <= p[1]) && (-31 <= p[2]);
}

int
is_valid_calib_offset(const int32_t *p)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (p[i] != INVALID_CALIB_OFFSET[i])
			return 1;
	}
	return 0;
}

static int
is_offset_differ(const int8_t *p0, const int8_t *p1)
{
	return p0[0] != p1[0] || p0[1] != p1[1] || p0[2] != p1[2];
}

static int
is_calib_offset_differ(const int32_t *p0, const int32_t *p1)
{
	return p0[0] != p1[0] || p0[1] != p1[1] || p0[2] != p1[2];
}

static int
get_overflow(struct yas_driver *d)
{
	return d->overflow;
}

static void
set_overflow(struct yas_driver *d, const int overflow)
{
	if (d->overflow != overflow)
		d->overflow = overflow;
}

static int
get_initcoil_gaveup(struct yas_driver *d)
{
	return d->initcoil_gaveup;
}

static void
set_initcoil_gaveup(struct yas_driver *d, const int initcoil_gaveup)
{
	d->initcoil_gaveup = initcoil_gaveup;
}

static int32_t *
get_calib_offset(struct yas_driver *d)
{
	return d->offset.calib_offset.v;
}

static void
set_calib_offset(struct yas_driver *d, const int32_t *offset)
{
	int i;

	if (is_calib_offset_differ(d->offset.calib_offset.v, offset)) {
		for (i = 0; i < 3; i++)
			d->offset.calib_offset.v[i] = offset[i];
	}
}

#ifdef YAS_MAG_MANUAL_OFFSET
static int32_t *
get_manual_offset(struct yas_driver *d)
{
	return d->manual_offset.v;
}

static void
set_manual_offset(struct yas_driver *d, const int32_t *offset)
{
	int i;

	for (i = 0; i < 3; i++)
		d->manual_offset.v[i] = offset[i];
}
#endif

static int32_t *
get_static_matrix(struct yas_driver *d)
{
	return d->static_matrix.matrix;
}

static void
set_static_matrix(struct yas_driver *d, const int32_t *matrix)
{
	int i;

	for (i = 0; i < 9; i++)
		d->static_matrix.matrix[i] = matrix[i];
}

static int32_t *
get_dynamic_matrix(struct yas_driver *d)
{
	return d->dynamic_matrix.matrix;
}

static void
set_dynamic_matrix(struct yas_driver *d, const int32_t *matrix)
{
	int i;

	for (i = 0; i < 9; i++)
		d->dynamic_matrix.matrix[i] = matrix[i];
}

static int8_t *
get_offset(struct yas_driver *d)
{
	return d->offset.hard_offset;
}

static void
set_offset(struct yas_driver *d, const int8_t *offset)
{
	int i;

	if (is_offset_differ(d->offset.hard_offset, offset)) {
		for (i = 0; i < 3; i++)
			d->offset.hard_offset[i] = offset[i];
	}
}

static int
get_active(struct yas_driver *d)
{
	return d->active;
}

static void
set_active(struct yas_driver *d, const int active)
{
	d->active = active;
}

static int
get_position(struct yas_driver *d)
{
	return d->position;
}

static void
set_position(struct yas_driver *d, const int position)
{
	d->position = position;
}

static int
get_measure_state(struct yas_driver *d)
{
	return d->measure_state;
}

static void
set_measure_state(struct yas_driver *d, const int state)
{
	YLOGI(("state(%d)\n", state));
	d->measure_state = state;
}

static struct utimer *
get_data_timer(struct yas_driver *d)
{
	return &d->data_timer;
}

static struct utimer *
get_initcoil_timer(struct yas_driver *d)
{
	return &d->initcoil_timer;
}

static struct utimer *
get_initcoil_giveup_timer(struct yas_driver *d)
{
	return &d->initcoil_giveup_timer;
}

static struct utimer *
get_detect_overflow_timer(struct yas_driver *d)
{
	return &d->detect_overflow_timer;
}

static int
get_delay_timer_use_data(struct yas_driver *d)
{
	return d->delay_timer_use_data;
}

static void
set_delay_timer_use_data(struct yas_driver *d, int flag)
{
	d->delay_timer_use_data = !!flag;
}

static int
get_delay_timer_interval(struct yas_driver *d)
{
	return d->delay_timer_interval;
}

static void
set_delay_timer_interval(struct yas_driver *d, int interval)
{
	d->delay_timer_interval = interval;
}

static int
get_delay_timer_counter(struct yas_driver *d)
{
	return d->delay_timer_counter;
}

static void
set_delay_timer_counter(struct yas_driver *d, int counter)
{
	d->delay_timer_counter = counter;
}

static int
get_filter_enable(struct yas_driver *d)
{
	return d->filter_enable;
}

static void
set_filter_enable(struct yas_driver *d, int enable)
{
	if (enable)
		filter_init(d);
	d->filter_enable = !!enable;
}

static int
get_filter_len(struct yas_driver *d)
{
	return d->filter_len;
}

static void
set_filter_len(struct yas_driver *d, int len)
{
	if (len < 0)
		return;
	if (len > YAS_MAG_MAX_FILTER_LEN)
		return;
	d->filter_len = len;
	filter_init(d);
}

static int
get_filter_noise(struct yas_driver *d, int *noise)
{
	int i;

	for (i = 0; i < 3; i++)
		noise[i] = d->filter_noise[i];
	return 0;
}

static void
set_filter_noise(struct yas_driver *d, int *noise)
{
	int i;

	if (noise == NULL)
		return;
	for (i = 0; i < 3; i++) {
		if (noise[i] < 0)
			noise[i] = 0;
		d->filter_noise[i] = noise[i];
	}
	filter_init(d);
}

static int
get_filter_thresh(struct yas_driver *d)
{
	return d->filter_thresh;
}

static void
set_filter_thresh(struct yas_driver *d, int threshold)
{
	if (threshold < 0)
		return;
	d->filter_thresh = threshold;
	filter_init(d);
}

static int32_t*
get_previous_mag(struct yas_driver *d)
{
	return d->prev_mag;
}

static void
set_previous_mag(struct yas_driver *d, int32_t *data)
{
	int i;

	for (i = 0; i < 3; i++)
		d->prev_mag[i] = data[i];
}

static int32_t*
get_previous_xy1y2(struct yas_driver *d)
{
	return d->prev_xy1y2;
}

static void
set_previous_xy1y2(struct yas_driver *d, int32_t *data)
{
	int i;

	for (i = 0; i < 3; i++)
		d->prev_xy1y2[i] = data[i];
}

static int32_t*
get_previous_mag_w_offset(struct yas_driver *d)
{
	return d->prev_mag_w_offset;
}

static void
set_previous_mag_w_offset(struct yas_driver *d, int32_t *data)
{
	int i;

	for (i = 0; i < 3; i++)
		d->prev_mag_w_offset[i] = data[i];
}

static int16_t
get_previous_temperature(struct yas_driver *d)
{
	return d->prev_temperature;
}

static void
set_previous_temperature(struct yas_driver *d, int16_t temperature)
{
	d->prev_temperature = temperature;
}

static int
init_coil(struct yas_driver *d)
{
	int rt;

	YLOGD(("init_coil IN\n"));

	utimer_update(get_initcoil_timer(d));
	if (!get_initcoil_gaveup(d)) {
		utimer_update(get_initcoil_giveup_timer(d));
		if (utimer_is_timeout(get_initcoil_giveup_timer(d))) {
			utimer_clear_timeout(get_initcoil_giveup_timer(d));
			set_initcoil_gaveup(d, TRUE);
		}
	}
	if (utimer_is_timeout(get_initcoil_timer(d))
			&& !get_initcoil_gaveup(d)) {
		utimer_clear_timeout(get_initcoil_timer(d));
		YLOGI(("init_coil!\n"));
		rt = yas_cdrv_actuate_initcoil();
		if (rt < 0) {
			YLOGE(("yas_cdrv_actuate_initcoil failed[%d]\n", rt));
			return rt;
		}
		if (get_overflow(d) || !is_valid_offset(get_offset(d)))
			set_measure_state(d, YAS_MAG_STATE_MEASURE_OFFSET);
		else
			set_measure_state(d, YAS_MAG_STATE_NORMAL);
	}

	YLOGD(("init_coil OUT\n"));

	return 0;
}

static int
measure_offset(struct yas_driver *d)
{
	int8_t offset[3];
	int32_t moffset[3];
	int rt, result = 0, i;

	YLOGI(("measure_offset IN\n"));

	rt = yas_cdrv_measure_and_set_offset(offset);
	if (rt < 0) {
		YLOGE(("yas_cdrv_measure_offset failed[%d]\n", rt));
		return rt;
	}

	YLOGI(("offset[%d][%d][%d]\n", offset[0], offset[1], offset[2]));

	for (i = 0; i < 3; i++)
		moffset[i] = get_calib_offset(d)[i];
	if (is_offset_differ(get_offset(d), offset)) {
		if (is_valid_offset(get_offset(d))
				&& is_valid_calib_offset(get_calib_offset(d))) {
			yas_cdrv_recalc_calib_offset(get_calib_offset(d),
					moffset,
					get_offset(d),
					offset);
			result |= YAS_REPORT_CALIB_OFFSET_CHANGED;
		}
	}
	result |= YAS_REPORT_HARD_OFFSET_CHANGED;
	set_offset(d, offset);
	if (is_valid_calib_offset(moffset))
		set_calib_offset(d, moffset);
	set_measure_state(d, YAS_MAG_STATE_NORMAL);

	YLOGI(("measure_offset OUT\n"));

	return result;
}

static int
measure_msensor_normal(struct yas_driver *d, int32_t *magnetic,
		int32_t *mag_w_offset, int32_t *xy1y2, int16_t *temperature)
{
	int rt = 0, result, i;
	int32_t tmp[3];

	YLOGD(("measure_msensor_normal IN\n"));

	result = 0;
	rt = yas_cdrv_measure(mag_w_offset, tmp, temperature);
	if (rt < 0) {
		YLOGE(("yas_cdrv_measure failed[%d]\n", rt));
		return rt;
	}
	for (i = 0; i < 3; i++)
		xy1y2[i] = tmp[i];
#ifdef YAS_MAG_MANUAL_OFFSET
	for (i = 0; i < 3; i++)
		mag_w_offset[i] -= get_manual_offset(d)[i];
#endif
	if (rt > 0) {
		YLOGW(("yas_cdrv_measure under/overflow "
					"x[%c%c] y[%c%c] z[%c%c]\n",
				(rt&0x01) ? 'u' : ' ',
				(rt&0x02) ? 'o' : ' ',
				(rt&0x04) ? 'u' : ' ',
				(rt&0x08) ? 'o' : ' ',
				(rt&0x10) ? 'u' : ' ',
				(rt&0x20) ? 'o' : ' '));
		utimer_update(get_detect_overflow_timer(d));
		set_overflow(d, TRUE);
		if (utimer_is_timeout(get_detect_overflow_timer(d))) {
			utimer_clear_timeout(get_detect_overflow_timer(d));
			result |= YAS_REPORT_OVERFLOW_OCCURED;
		}
		if (get_measure_state(d) == YAS_MAG_STATE_NORMAL)
			set_measure_state(d, YAS_MAG_STATE_INIT_COIL);
	} else {
		utimer_clear(get_detect_overflow_timer(d));
		set_overflow(d, FALSE);
		if (get_measure_state(d) == YAS_MAG_STATE_NORMAL) {
			utimer_clear(get_initcoil_timer(d));
			utimer_clear(get_initcoil_giveup_timer(d));
		}
	}

	for (i = 0; i < 3; i++) {
		tmp[i]
			= (get_static_matrix(d)[i*3+0]/10
					* (mag_w_offset[0]/10)) / 100
			+ (get_static_matrix(d)[i*3+1]/10
					* (mag_w_offset[1]/10)) / 100
			+ (get_static_matrix(d)[i*3+2]/10
					* (mag_w_offset[2]/10)) / 100;
	}
	for (i = 0; i < 3; i++)
		magnetic[i] = mag_w_offset[i] = tmp[i];
	if (is_valid_calib_offset(get_calib_offset(d))) {
		for (i = 0; i < 3; i++)
			magnetic[i] -= get_calib_offset(d)[i];
	}
	for (i = 0; i < 3; i++) {
		tmp[i]
			= (get_dynamic_matrix(d)[i*3+0]/10
					* (magnetic[0]/10)) / 100
			+ (get_dynamic_matrix(d)[i*3+1]/10
					* (magnetic[1]/10)) / 100
			+ (get_dynamic_matrix(d)[i*3+2]/10
					* (magnetic[2]/10)) / 100;
	}
	for (i = 0; i < 3; i++)
		magnetic[i] = tmp[i];
	if (get_filter_enable(d))
		filter_filter(d, magnetic, magnetic);

	YLOGD(("measure_msensor_normal OUT\n"));

	return result;
}

static int
measure_msensor(struct yas_driver *d, int32_t *magnetic, int32_t *mag_w_offset,
		int32_t *xy1y2, int16_t *temperature)
{
	int result, i;

	YLOGD(("measure_msensor IN\n"));

	for (i = 0; i < 3; i++) {
		magnetic[i] = get_previous_mag(d)[i];
		mag_w_offset[i] = get_previous_mag_w_offset(d)[i];
		xy1y2[i] = get_previous_xy1y2(d)[i];
		*temperature = get_previous_temperature(d);
	}

	result = 0;
	switch (get_measure_state(d)) {
	case YAS_MAG_STATE_INIT_COIL:
		result = init_coil(d);
		break;
	case YAS_MAG_STATE_MEASURE_OFFSET:
		result = measure_offset(d);
		break;
	case YAS_MAG_STATE_NORMAL:
		result = 0;
		break;
	default:
		result = -1;
		break;
	}

	if (result < 0)
		return result;

	if (!(result & YAS_REPORT_OVERFLOW_OCCURED)) {
		result |= measure_msensor_normal(d, magnetic, mag_w_offset,
				xy1y2, temperature);
	}
	set_previous_mag(d, magnetic);
	set_previous_xy1y2(d, xy1y2);
	set_previous_mag_w_offset(d, mag_w_offset);
	set_previous_temperature(d, *temperature);

	YLOGD(("measure_msensor OUT\n"));

	return result;
}

static int
measure(struct yas_driver *d, int32_t *magnetic, int32_t *mag_w_offset,
		int32_t *xy1y2, int16_t *temperature, uint32_t *time_delay)
{
	int result;
	int counter;
	uint32_t total = 0;

	YLOGD(("measure IN\n"));

	utimer_update(get_data_timer(d));

	result = measure_msensor(d, magnetic, mag_w_offset, xy1y2, temperature);
	if (result < 0)
		return result;

	counter = get_delay_timer_counter(d);
	total = utimer_get_total_time(get_data_timer(d));
	if (utimer_get_delay(get_data_timer(d)) > 0)
		counter -= (int)(total / utimer_get_delay(get_data_timer(d)));
	else
		counter = 0;

	if (utimer_is_timeout(get_data_timer(d))) {
		utimer_clear_timeout(get_data_timer(d));

		if (get_delay_timer_use_data(d)) {
			result |= YAS_REPORT_DATA;
			if (counter <= 0)
				result |= YAS_REPORT_CALIB;
		} else {
			result |= YAS_REPORT_CALIB;
			if (counter <= 0)
				result |= YAS_REPORT_DATA;
		}
	}

	if (counter <= 0)
		set_delay_timer_counter(d, get_delay_timer_interval(d));
	else
		set_delay_timer_counter(d, counter);

	*time_delay = utimer_sleep_time(get_data_timer(d));

	YLOGD(("measure OUT [%d]\n", result));

	return result;
}

static int
resume(struct yas_driver *d)
{
	int32_t zero[] = {0, 0, 0};
	struct yas_machdep_func func;
	int rt;

	YLOGI(("resume IN\n"));

	func.device_open = d->callback.device_open;
	func.device_close = d->callback.device_close;
	func.device_write = d->callback.device_write;
	func.device_read = d->callback.device_read;
	func.msleep = d->callback.msleep;

	rt = yas_cdrv_init(YAS_TRANSFORMATION[get_position(d)], &func);
	if (rt < 0) {
		YLOGE(("yas_cdrv_init failed[%d]\n", rt));
		return rt;
	}

	utimer_clear(get_data_timer(d));
	utimer_clear(get_initcoil_giveup_timer(d));
	utimer_clear(get_initcoil_timer(d));
	utimer_clear(get_detect_overflow_timer(d));

	set_previous_mag(d, zero);
	set_previous_xy1y2(d, zero);
	set_previous_mag_w_offset(d, zero);
	set_previous_temperature(d, 0);
	set_overflow(d, FALSE);
	set_initcoil_gaveup(d, FALSE);

	filter_init(d);

	if (is_valid_offset(d->offset.hard_offset)) {
		yas_cdrv_set_offset(d->offset.hard_offset);
		rt = yas_cdrv_actuate_initcoil();
		if (rt < 0) {
			YLOGE(("yas_cdrv_actuate_initcoil failed[%d]\n", rt));
			set_measure_state(d, YAS_MAG_STATE_INIT_COIL);
		} else {
			set_measure_state(d, YAS_MAG_STATE_NORMAL);
		}
	} else {
		rt = yas_cdrv_actuate_initcoil();
		if (rt < 0) {
			YLOGE(("yas_cdrv_actuate_initcoil failed[%d]\n", rt));
			set_measure_state(d, YAS_MAG_STATE_INIT_COIL);
		} else {
			set_measure_state(d, YAS_MAG_STATE_MEASURE_OFFSET);
		}
	}

	YLOGI(("resume OUT\n"));
	return 0;
}

static int
suspend(struct yas_driver *d)
{
	YLOGI(("suspend IN\n"));

	(void) d;
	yas_cdrv_term();

	YLOGI(("suspend OUT\n"));
	return 0;
}

static int
check_interval(int ms)
{
	int index;

	if (ms <= supported_data_interval[0])
		ms = supported_data_interval[0];
	for (index = 0; index < NELEMS(supported_data_interval); index++) {
		if (ms == supported_data_interval[index])
			return index;
	}
	return -1;
}

static int
yas_get_delay_nolock(struct yas_driver *d, int *ms)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	if (get_delay_timer_use_data(d))
		*ms = (int)utimer_get_delay(get_data_timer(d));
	else
		*ms = (int)utimer_get_delay(get_data_timer(d))
			* get_delay_timer_interval(d);
	return YAS_NO_ERROR;
}

static int
yas_set_delay_nolock(struct yas_driver *d, int ms)
{
	int index;
	uint32_t delay_data, delay_calib;

	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	index = check_interval(ms);
	if (index < 0)
		return YAS_ERROR_ARG;
	delay_data = (uint32_t)supported_data_interval[index];
	delay_calib = (uint32_t)supported_calib_interval[index];
	set_delay_timer_use_data(d, delay_data < delay_calib);
	if (delay_data < delay_calib) {
		set_delay_timer_interval(d, (int)(delay_calib / delay_data));
		set_delay_timer_counter(d, (int)(delay_calib / delay_data));
		utimer_set_delay(get_data_timer(d),
				(uint32_t)supported_data_interval[index]);
	} else {
		set_delay_timer_interval(d, (int)(delay_data / delay_calib));
		set_delay_timer_counter(d, (int)(delay_data / delay_calib));
		utimer_set_delay(get_data_timer(d),
				(uint32_t)supported_calib_interval[index]);
	}

	return YAS_NO_ERROR;
}

static int
yas_get_offset_nolock(struct yas_driver *d, struct yas_mag_offset *offset)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	*offset = d->offset;
	return YAS_NO_ERROR;
}

static int
yas_set_offset_nolock(struct yas_driver *d, struct yas_mag_offset *offset)
{
	int32_t zero[] = {0, 0, 0};
	int rt;

	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	if (!get_active(d)) {
		d->offset = *offset;
		return YAS_NO_ERROR;
	}

	if (!is_valid_offset(offset->hard_offset)
			|| is_offset_differ(offset->hard_offset,
				d->offset.hard_offset)) {
		filter_init(d);

		utimer_clear(get_data_timer(d));
		utimer_clear(get_initcoil_giveup_timer(d));
		utimer_clear(get_initcoil_timer(d));
		utimer_clear(get_detect_overflow_timer(d));

		set_previous_mag(d, zero);
		set_previous_xy1y2(d, zero);
		set_previous_mag_w_offset(d, zero);
		set_previous_temperature(d, 0);
		set_overflow(d, FALSE);
		set_initcoil_gaveup(d, FALSE);
	}
	d->offset = *offset;

	if (is_valid_offset(d->offset.hard_offset)) {
		yas_cdrv_set_offset(d->offset.hard_offset);
	} else {
		rt = yas_cdrv_actuate_initcoil();
		if (rt < 0) {
			YLOGE(("yas_cdrv_actuate_initcoil failed[%d]\n", rt));
			set_measure_state(d, YAS_MAG_STATE_INIT_COIL);
		} else {
			set_measure_state(d, YAS_MAG_STATE_MEASURE_OFFSET);
		}
	}

	return YAS_NO_ERROR;
}

#ifdef YAS_MAG_MANUAL_OFFSET

static int
yas_get_manual_offset_nolock(struct yas_driver *d, struct yas_vector *offset)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;

	*offset = d->manual_offset;

	return YAS_NO_ERROR;
}

static int
yas_set_manual_offset_nolock(struct yas_driver *d, struct yas_vector *offset)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;

	set_manual_offset(d, offset->v);

	return YAS_NO_ERROR;
}

#endif

static int
yas_get_static_matrix_nolock(struct yas_driver *d, struct yas_matrix *matrix)
{
	int i;

	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	for (i = 0; i < 9; i++)
		matrix->matrix[i] = get_static_matrix(d)[i];
	return YAS_NO_ERROR;
}

static int
yas_set_static_matrix_nolock(struct yas_driver *d, struct yas_matrix *matrix)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	set_static_matrix(d, matrix->matrix);
	return YAS_NO_ERROR;
}

static int
yas_get_dynamic_matrix_nolock(struct yas_driver *d, struct yas_matrix *matrix)
{
	int i;

	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	for (i = 0; i < 9; i++)
		matrix->matrix[i] = get_dynamic_matrix(d)[i];

	return YAS_NO_ERROR;
}

static int
yas_set_dynamic_matrix_nolock(struct yas_driver *d, struct yas_matrix *matrix)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	set_dynamic_matrix(d, matrix->matrix);

	return YAS_NO_ERROR;
}

static int
yas_get_enable_nolock(struct yas_driver *d)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	return get_active(d);
}

static int
yas_set_enable_nolock(struct yas_driver *d, int active)
{
	int rt;

	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	if (active) {
		if (get_active(d))
			return YAS_NO_ERROR;
		rt = resume(d);
		if (rt < 0)
			return rt;
		set_active(d, TRUE);
	} else {
		if (!get_active(d))
			return YAS_NO_ERROR;
		rt = suspend(d);
		if (rt < 0)
			return rt;
		set_active(d, FALSE);
	}
	return YAS_NO_ERROR;
}

static int
yas_get_filter_nolock(struct yas_driver *d, struct yas_mag_filter *filter)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	filter->len = get_filter_len(d);
	get_filter_noise(d, filter->noise);
	filter->threshold = get_filter_thresh(d);
	return YAS_NO_ERROR;
}

static int
yas_set_filter_nolock(struct yas_driver *d, struct yas_mag_filter *filter)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	set_filter_len(d, filter->len);
	set_filter_noise(d, filter->noise);
	set_filter_thresh(d, filter->threshold);
	return YAS_NO_ERROR;
}

static int
yas_get_filter_enable_nolock(struct yas_driver *d)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	return get_filter_enable(d);
}

static int
yas_set_filter_enable_nolock(struct yas_driver *d, int enable)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	set_filter_enable(d, enable);
	return YAS_NO_ERROR;
}

static int
yas_get_position_nolock(struct yas_driver *d, int *position)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	*position = get_position(d);
	return YAS_NO_ERROR;
}

static int
yas_set_position_nolock(struct yas_driver *d, int position)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	if (get_active(d))
		yas_cdrv_set_transformatiom_matrix(
				YAS_TRANSFORMATION[position]);
	set_position(d, position);
	filter_init(d);
	return YAS_NO_ERROR;
}

static int
yas_read_reg_nolock(struct yas_driver *d, uint8_t addr, uint8_t *buf, int len)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	if (!get_active(d)) {
		if (d->callback.device_open() < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
	}
	if (d->callback.device_read(YAS_MAG_I2C_SLAVEADDR, addr, buf, len) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (!get_active(d)) {
		if (d->callback.device_close() < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
	}

	return YAS_NO_ERROR;
}

static int
yas_write_reg_nolock(struct yas_driver *d, uint8_t addr, const uint8_t *buf,
		int len)
{
	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	if (!get_active(d)) {
		if (d->callback.device_open() < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
	}
	if (d->callback.device_write(YAS_MAG_I2C_SLAVEADDR, addr, buf, len) < 0)
		return YAS_ERROR_DEVICE_COMMUNICATION;
	if (!get_active(d)) {
		if (d->callback.device_close() < 0)
			return YAS_ERROR_DEVICE_COMMUNICATION;
	}

	return YAS_NO_ERROR;
}

static int
yas_measure_nolock(struct yas_driver *d, struct yas_mag_data *data,
		int *time_delay_ms)
{
	uint32_t time_delay = YAS_MAG_ERROR_DELAY;
	int rt, i;

	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	*time_delay_ms = YAS_MAG_ERROR_DELAY;

	if (!get_active(d)) {
		for (i = 0; i < 3; i++) {
			data->xyz.v[i] = get_previous_mag(d)[i];
			data->raw.v[i] = get_previous_mag_w_offset(d)[i];
			data->xy1y2.v[i] = get_previous_xy1y2(d)[i];
		}
		data->temperature = get_previous_temperature(d);
		return YAS_NO_ERROR;
	}

	rt = measure(d, data->xyz.v, data->raw.v, data->xy1y2.v,
			&data->temperature, &time_delay);
	if (rt >= 0) {
		*time_delay_ms = (int)time_delay;
		if (*time_delay_ms > 0)
			*time_delay_ms += 1; /* for the system that the time is
						in usec unit */
	}

	return rt;
}

static int
yas_init_nolock(struct yas_driver *d)
{
#ifdef YAS_MAG_MANUAL_OFFSET
	int32_t zero[] = {0, 0, 0};
#endif
	int32_t notransform[] = {10000, 0, 0, 0, 10000, 0, 0, 0, 10000};
	int noise[] = {
		YAS_MAG_DEFAULT_FILTER_NOISE_X,
		YAS_MAG_DEFAULT_FILTER_NOISE_Y,
		YAS_MAG_DEFAULT_FILTER_NOISE_Z
	};

	YLOGI(("yas_init_nolock IN\n"));

	if (d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;

	utimer_lib_init(this_driver.callback.current_time);
	utimer_init(get_data_timer(d), 50);
	utimer_init(get_initcoil_timer(d), YAS_INITCOIL_INTERVAL);
	utimer_init(get_initcoil_giveup_timer(d), YAS_INITCOIL_GIVEUP_INTERVAL);
	utimer_init(get_detect_overflow_timer(d), YAS_DETECT_OVERFLOW_INTERVAL);

	set_delay_timer_use_data(d, 0);
	set_delay_timer_interval(d,
			YAS_DEFAULT_DATA_INTERVAL / YAS_DEFAULT_CALIB_INTERVAL);
	set_delay_timer_counter(d,
			YAS_DEFAULT_DATA_INTERVAL / YAS_DEFAULT_CALIB_INTERVAL);

	set_filter_enable(d, FALSE);
	set_filter_len(d, YAS_MAG_DEFAULT_FILTER_LEN);
	set_filter_thresh(d, YAS_MAG_DEFAULT_FILTER_THRESH);
	set_filter_noise(d, noise);
	filter_init(d);
	set_calib_offset(d, INVALID_CALIB_OFFSET);
#ifdef YAS_MAG_MANUAL_OFFSET
	set_manual_offset(d, zero);
#endif
	set_static_matrix(d, notransform);
	set_dynamic_matrix(d, notransform);
	set_offset(d, INVALID_OFFSET);
	set_active(d, FALSE);
	set_position(d, 0);

	d->initialized = 1;

	YLOGI(("yas_init_nolock OUT\n"));

	return YAS_NO_ERROR;
}

static int
yas_term_nolock(struct yas_driver *d)
{
	YLOGI(("yas_term_nolock\n"));

	if (!d->initialized)
		return YAS_ERROR_NOT_INITIALIZED;
	if (get_active(d))
		suspend(d);
	d->initialized = 0;

	YLOGI(("yas_term_nolock out\n"));
	return YAS_NO_ERROR;
}

static int
yas_get_delay(void)
{
	int ms = 0, rt;

	YLOGI(("yas_get_delay\n"));

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_get_delay_nolock(&this_driver, &ms);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_get_delay[%d] OUT\n", ms));

	return rt < 0 ? rt : ms;
}

static int
yas_set_delay(int delay)
{
	int rt;

	YLOGI(("yas_set_delay\n"));

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_set_delay_nolock(&this_driver, delay);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_set_delay OUT\n"));

	return rt;
}

static int
yas_get_offset(struct yas_mag_offset *offset)
{
	int rt;

	YLOGI(("yas_get_offset\n"));

	if (offset == NULL)
		return YAS_ERROR_ARG;
	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_get_offset_nolock(&this_driver, offset);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_get_offset[%d] OUT\n", rt));

	return rt;
}

static int
yas_set_offset(struct yas_mag_offset *offset)
{
	int rt;

	YLOGI(("yas_set_offset IN\n"));

	if (offset == NULL)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_set_offset_nolock(&this_driver, offset);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_set_offset OUT\n"));

	return rt;
}

#ifdef YAS_MAG_MANUAL_OFFSET

static int
yas_get_manual_offset(struct yas_vector *offset)
{
	int rt;

	YLOGI(("yas_get_manual_offset\n"));

	if (offset == NULL)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_get_manual_offset_nolock(&this_driver, offset);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_get_manual_offset[%d] OUT\n", rt));

	return rt;
}

static int
yas_set_manual_offset(struct yas_vector *offset)
{
	int rt;

	YLOGI(("yas_set_manual_offset IN\n"));

	if (offset == NULL)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_set_manual_offset_nolock(&this_driver, offset);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_set_manual_offset OUT\n"));

	return rt;
}

#endif

static int
yas_get_static_matrix(struct yas_matrix *matrix)
{
	int rt;

	YLOGI(("yas_get_static_matrix\n"));

	if (matrix == NULL)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_get_static_matrix_nolock(&this_driver, matrix);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_get_static_matrix[%d] OUT\n", rt));

	return rt;
}

static int
yas_set_static_matrix(struct yas_matrix *matrix)
{
	int rt;

	YLOGI(("yas_set_static_matrix IN\n"));

	if (matrix == NULL)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_set_static_matrix_nolock(&this_driver, matrix);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_set_static_matrix OUT\n"));

	return rt;
}

static int
yas_get_dynamic_matrix(struct yas_matrix *matrix)
{
	int rt;

	YLOGI(("yas_get_dynamic_matrix\n"));

	if (matrix == NULL)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_get_dynamic_matrix_nolock(&this_driver, matrix);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_get_dynamic_matrix[%d] OUT\n", rt));

	return rt;
}

static int
yas_set_dynamic_matrix(struct yas_matrix *matrix)
{
	int rt;

	YLOGI(("yas_set_dynamic_matrix IN\n"));

	if (matrix == NULL)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_set_dynamic_matrix_nolock(&this_driver, matrix);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_set_dynamic_matrix OUT\n"));

	return rt;
}

static int
yas_get_enable(void)
{
	int rt;

	YLOGI(("yas_get_enable\n"));

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_get_enable_nolock(&this_driver);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_get_enable OUT[%d]\n", rt));

	return rt;
}

static int
yas_set_enable(int enable)
{
	int rt;

	YLOGI(("yas_set_enable IN\n"));

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_set_enable_nolock(&this_driver, enable);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_set_enable OUT\n"));

	return rt;
}

static int
yas_get_filter(struct yas_mag_filter *filter)
{
	int rt;

	YLOGI(("yas_get_filter\n"));

	if (filter == NULL)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_get_filter_nolock(&this_driver, filter);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_get_filter[%d] OUT\n", rt));

	return rt;
}

static int
yas_set_filter(struct yas_mag_filter *filter)
{
	int rt, i;

	YLOGI(("yas_set_filter IN\n"));

	if (filter == NULL
			|| filter->len < 0
			|| YAS_MAG_MAX_FILTER_LEN < filter->len
			|| filter->threshold < 0) {
		return YAS_ERROR_ARG;
	}
	for (i = 0; i < 3; i++) {
		if (filter->noise[i] < 0)
			return YAS_ERROR_ARG;
	}

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_set_filter_nolock(&this_driver, filter);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_set_filter OUT\n"));

	return rt;
}

static int
yas_get_filter_enable(void)
{
	int rt;

	YLOGI(("yas_get_filter_enable\n"));

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_get_filter_enable_nolock(&this_driver);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_get_filter_enable OUT[%d]\n", rt));

	return rt;
}

static int
yas_set_filter_enable(int enable)
{
	int rt;

	YLOGI(("yas_set_filter_enable IN\n"));

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_set_filter_enable_nolock(&this_driver, enable);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_set_filter_enable OUT\n"));

	return rt;
}

static int
yas_get_position(void)
{
	int position = 0;
	int rt;

	YLOGI(("yas_get_position\n"));

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_get_position_nolock(&this_driver, &position);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_get_position[%d] OUT\n", position));

	return rt < 0 ? rt : position;
}

static int
yas_set_position(int position)
{
	int rt;

	YLOGI(("yas_set_position\n"));

	if (position < 0 || 7 < position)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_set_position_nolock(&this_driver, position);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_set_position[%d] OUT\n", position));

	return rt;
}

static int
yas_read_reg(uint8_t addr, uint8_t *buf, int len)
{
	int rt;

	YLOGI(("yas_read_reg\n"));

	if (buf == NULL || len <= 0)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_read_reg_nolock(&this_driver, addr, buf, len);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_read_reg[%d] OUT\n", rt));

	return rt;
}

static int
yas_write_reg(uint8_t addr, const uint8_t *buf, int len)
{
	int rt;

	YLOGI(("yas_write_reg\n"));

	if (buf == NULL || len <= 0)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_write_reg_nolock(&this_driver, addr, buf, len);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGI(("yas_write_reg[%d] OUT\n", rt));

	return rt;
}

static int
yas_measure(struct yas_mag_data *data, int *time_delay_ms)
{
	int rt;

	YLOGD(("yas_measure IN\n"));

	if (data == NULL || time_delay_ms == NULL)
		return YAS_ERROR_ARG;

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_measure_nolock(&this_driver, data, time_delay_ms);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	YLOGD(("yas_measure OUT[%d]\n", rt));

	return rt;
}

static int
yas_init(void)
{
	int rt;

	YLOGI(("yas_init\n"));

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_init_nolock(&this_driver);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	return rt;
}

static int
yas_term(void)
{
	int rt;

	YLOGI(("yas_term\n"));

	if (lock() < 0)
		return YAS_ERROR_RESTARTSYS;

	rt = yas_term_nolock(&this_driver);

	if (unlock() < 0)
		return YAS_ERROR_RESTARTSYS;

	return rt;
}

int
yas_mag_driver_init(struct yas_mag_driver *f)
{
	if (f == NULL)
		return YAS_ERROR_ARG;
	if (f->callback.device_open == NULL
			|| f->callback.device_close == NULL
			|| f->callback.device_read == NULL
			|| f->callback.device_write == NULL
			|| f->callback.msleep == NULL
			|| f->callback.current_time == NULL) {
		return YAS_ERROR_ARG;
	}

	f->init = yas_init;
	f->term = yas_term;
	f->get_delay = yas_get_delay;
	f->set_delay = yas_set_delay;
	f->get_offset = yas_get_offset;
	f->set_offset = yas_set_offset;
#ifdef YAS_MAG_MANUAL_OFFSET
	f->get_manual_offset = yas_get_manual_offset;
	f->set_manual_offset = yas_set_manual_offset;
#endif
	f->get_static_matrix = yas_get_static_matrix;
	f->set_static_matrix = yas_set_static_matrix;
	f->get_dynamic_matrix = yas_get_dynamic_matrix;
	f->set_dynamic_matrix = yas_set_dynamic_matrix;
	f->get_enable = yas_get_enable;
	f->set_enable = yas_set_enable;
	f->get_filter = yas_get_filter;
	f->set_filter = yas_set_filter;
	f->get_filter_enable = yas_get_filter_enable;
	f->set_filter_enable = yas_set_filter_enable;
	f->get_position = yas_get_position;
	f->set_position = yas_set_position;
	f->read_reg = yas_read_reg;
	f->write_reg = yas_write_reg;
	f->measure = yas_measure;

	if ((f->callback.lock == NULL && f->callback.unlock != NULL)
			|| (f->callback.lock != NULL
				&& f->callback.unlock == NULL)) {
		this_driver.callback.lock = NULL;
		this_driver.callback.unlock = NULL;
	} else {
		this_driver.callback.lock = f->callback.lock;
		this_driver.callback.unlock = f->callback.unlock;
	}
	this_driver.callback.device_open = f->callback.device_open;
	this_driver.callback.device_close = f->callback.device_close;
	this_driver.callback.device_write = f->callback.device_write;
	this_driver.callback.device_read = f->callback.device_read;
	this_driver.callback.msleep = f->callback.msleep;
	this_driver.callback.current_time = f->callback.current_time;
	yas_term();

	return YAS_NO_ERROR;
}

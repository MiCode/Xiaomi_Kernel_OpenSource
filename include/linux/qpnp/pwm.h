/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __QPNP_PWM_H__
#define __QPNP_PWM_H__

#include <linux/pwm.h>

/* usec: 19.2M, n=6, m=0, pre=2 */
#define PM_PWM_PERIOD_MIN			7
/* 1K, n=9, m=7, pre=6 */
#define PM_PWM_PERIOD_MAX			(384 * USEC_PER_SEC)
#define PM_PWM_LUT_RAMP_STEP_TIME_MAX		499
#define PM_PWM_MAX_PAUSE_CNT			8191
/*
 * Formula from HSID,
 * pause_time (hi/lo) = (pause_code - 1)*(duty_ms)
 */
#define PM_PWM_LUT_PAUSE_MAX \
	((PM_PWM_MAX_PAUSE_CNT - 1) * PM_PWM_LUT_RAMP_STEP_TIME_MAX) /* ms */

/* Flags for Look Up Table */
#define PM_PWM_LUT_LOOP			0x01
#define PM_PWM_LUT_RAMP_UP		0x02
#define PM_PWM_LUT_REVERSE		0x04
#define PM_PWM_LUT_PAUSE_HI_EN		0x08
#define PM_PWM_LUT_PAUSE_LO_EN		0x10

#define PM_PWM_LUT_NO_TABLE		0x20
#define PM_PWM_LUT_USE_RAW_VALUE	0x40

/*
 * PWM frequency/period control
 *
 * PWM Frequency = ClockFrequency / (N * T)
 *   or
 * PWM Period = Clock Period * (N * T)
 *   where
 * N = 2^9 or 2^6 for 9-bit or 6-bit PWM size
 * T = Pre-divide * 2^m, m = 0..7 (exponent)
 */

/*
 * enum pm_pwm_size - PWM bit mode selection
 * %PM_PWM_SIZE_6BIT - Select 6 bit mode; 64 levels
 * %PM_PWM_SIZE_9BIT - Select 9 bit mode; 512 levels
 */
enum pm_pwm_size {
	PM_PWM_SIZE_6BIT =	6,
	PM_PWM_SIZE_9BIT =	9,
};

/*
 * enum pm_pwm_clk - PWM clock selection
 * %PM_PWM_CLK_1KHZ - 1KHz clock
 * %PM_PWM_CLK_32KHZ - 32KHz clock
 * %PM_PWM_CLK_19P2MHZ - 19.2MHz clock
 * Note: Here 1KHz = 1024Hz
 */
enum pm_pwm_clk {
	PM_PWM_CLK_1KHZ,
	PM_PWM_CLK_32KHZ,
	PM_PWM_CLK_19P2MHZ,
};

/* PWM pre-divider selection */
enum pm_pwm_pre_div {
	PM_PWM_PDIV_2,
	PM_PWM_PDIV_3,
	PM_PWM_PDIV_5,
	PM_PWM_PDIV_6,
};

/*
 * struct pwm_period_config - PWM period configuration
 * @pwm_size: enum pm_pwm_size
 * @clk: enum pm_pwm_clk
 * @pre_div: enum pm_pwm_pre_div
 * @pre_div_exp: exponent of 2 as part of pre-divider: 0..7
 */
struct pwm_period_config {
	enum pm_pwm_size	pwm_size;
	enum pm_pwm_clk		clk;
	enum pm_pwm_pre_div	pre_div;
	int			pre_div_exp;
};

/*
 * struct pwm_duty_cycles - PWM duty cycle info
 * duty_pcts - pointer to an array of duty percentage for a pwm period
 * num_duty_pcts - total entries in duty_pcts array
 * duty_ms - duty cycle time in ms
 * start_idx - index in the LUT
 */
struct pwm_duty_cycles {
	int *duty_pcts;
	int num_duty_pcts;
	int duty_ms;
	int start_idx;
};

int pwm_config_period(struct pwm_device *pwm,
			     struct pwm_period_config *pwm_p);

int pwm_config_pwm_value(struct pwm_device *pwm, int pwm_value);

/*
 * lut_params: Lookup table (LUT) parameters
 * @start_idx: start index in lookup table from 0 to MAX-1
 * @idx_len: number of index
 * @pause_lo: pause time in millisecond at low index
 * @pause_hi: pause time in millisecond at high index
 * @ramp_step_ms: time before loading next LUT pattern in millisecond
 * @flags: control flags
 */
struct lut_params {
	int start_idx;
	int idx_len;
	int lut_pause_hi;
	int lut_pause_lo;
	int ramp_step_ms;
	int flags;
};

int pwm_lut_config(struct pwm_device *pwm, int period_us,
		int duty_pct[], struct lut_params lut_params);

int pwm_lut_enable(struct pwm_device *pwm, int start);

/* Standard APIs supported */
/*
 * pwm_request - request a PWM device
 * @pwm_id: PWM id or channel
 * @label: the label to identify the user
 */

/*
 * pwm_free - free a PWM device
 * @pwm: the PWM device
 */

/*
 * pwm_config - change a PWM device configuration
 * @pwm: the PWM device
 * @period_us: period in microsecond
 * @duty_us: duty cycle in microsecond
 */

/*
 * pwm_enable - start a PWM output toggling
 * @pwm: the PWM device
 */

/*
 * pwm_disable - stop a PWM output toggling
 * @pwm: the PWM device
 */

#endif /* __QPNP_PWM_H__ */

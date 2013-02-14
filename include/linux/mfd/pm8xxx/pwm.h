/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#ifndef __PM8XXX_PWM_H__
#define __PM8XXX_PWM_H__

#include <linux/pwm.h>

#define PM8XXX_PWM_DEV_NAME	"pm8xxx-pwm"

#define PM8XXX_PWM_PERIOD_MIN	7 /* usec: 19.2M, n=6, m=0, pre=2 */
#define PM8XXX_PWM_PERIOD_MAX	(384 * USEC_PER_SEC) /* 1K, n=9, m=7, pre=6 */
#define PM_PWM_LUT_SIZE			64
#define PM_PWM_LUT_DUTY_TIME_MAX	512	/* ms */
#define PM_PWM_LUT_PAUSE_MAX		(7000 * PM_PWM_LUT_DUTY_TIME_MAX)

/* Flags for Look Up Table */
#define PM_PWM_LUT_LOOP		0x01
#define PM_PWM_LUT_RAMP_UP	0x02
#define PM_PWM_LUT_REVERSE	0x04
#define PM_PWM_LUT_PAUSE_HI_EN	0x10
#define PM_PWM_LUT_PAUSE_LO_EN	0x20

#define PM_PWM_LUT_NO_TABLE	0x100

/**
 * PWM frequency/period control
 *
 * PWM Frequency = ClockFrequency / (N * T)
 *   or
 * PWM Period = Clock Period * (N * T)
 *   where
 * N = 2^9 or 2^6 for 9-bit or 6-bit PWM size
 * T = Pre-divide * 2^m, m = 0..7 (exponent)
 *
 */

enum pm_pwm_size {
	PM_PWM_SIZE_6BIT =	6,
	PM_PWM_SIZE_9BIT =	9,
};

enum pm_pwm_clk {
	PM_PWM_CLK_1KHZ,
	PM_PWM_CLK_32KHZ,
	PM_PWM_CLK_19P2MHZ,
};

enum pm_pwm_pre_div {
	PM_PWM_PDIV_2,
	PM_PWM_PDIV_3,
	PM_PWM_PDIV_5,
	PM_PWM_PDIV_6,
};

/**
 * struct pm8xxx_pwm_period - PWM period structure
 * @pwm_size: enum pm_pwm_size
 * @clk: enum pm_pwm_clk
 * @pre_div: enum pm_pwm_pre_div
 * @pre_div_exp: exponent of 2 as part of pre-divider: 0..7
 */
struct pm8xxx_pwm_period {
	enum pm_pwm_size	pwm_size;
	enum pm_pwm_clk		clk;
	enum pm_pwm_pre_div	pre_div;
	int			pre_div_exp;
};

/**
 * struct pm8xxx_pwm_duty_cycles - PWM duty cycle info
 * duty_pcts - pointer to an array of duty percentage for a pwm period
 * num_duty_pcts - total entries in duty_pcts array
 * duty_ms - duty cycle time in ms
 * start_idx - index in the LUT
 */
struct pm8xxx_pwm_duty_cycles {
	int *duty_pcts;
	int num_duty_pcts;
	int duty_ms;
	int start_idx;
};

/**
 * struct pm8xxx_pwm_platform_data - PWM platform data
 * dtest_channel - Enable LPG DTEST mode for this LPG channel
 */
struct pm8xxx_pwm_platform_data {
	int dtest_channel;
};

/**
 * pm8xxx_pwm_config_period - change PWM period
 *
 * @pwm: the PWM device
 * @pwm_p: period in struct pm8xxx_pwm_period
 */
int pm8xxx_pwm_config_period(struct pwm_device *pwm,
			     struct pm8xxx_pwm_period *pwm_p);

/**
 * pm8xxx_pwm_config_pwm_value - change a PWM device configuration
 * @pwm: the PWM device
 * @pwm_value: the duty cycle in raw PWM value (< 2^pwm_size)
 */
int pm8xxx_pwm_config_pwm_value(struct pwm_device *pwm, int pwm_value);

/**
 * pm8xxx_pwm_lut_config - change a PWM device configuration to use LUT
 * @pwm: the PWM device
 * @period_us: period in micro second
 * @duty_pct: arrary of duty cycles in percent, like 20, 50.
 * @duty_time_ms: time for each duty cycle in millisecond
 * @start_idx: start index in lookup table from 0 to MAX-1
 * @idx_len: number of index
 * @pause_lo: pause time in millisecond at low index
 * @pause_hi: pause time in millisecond at high index
 * @flags: control flags
 */
int pm8xxx_pwm_lut_config(struct pwm_device *pwm, int period_us,
			  int duty_pct[], int duty_time_ms, int start_idx,
			  int len, int pause_lo, int pause_hi, int flags);

/**
 * pm8xxx_pwm_lut_enable - control a PWM device to start/stop LUT ramp
 * @pwm: the PWM device
 * @start: to start (1), or stop (0)
 */
int pm8xxx_pwm_lut_enable(struct pwm_device *pwm, int start);

/* Standard APIs supported */
/**
 * pwm_request - request a PWM device
 * @pwm_id: PWM id or channel
 * @label: the label to identify the user
 */

/**
 * pwm_free - free a PWM device
 * @pwm: the PWM device
 */

/**
 * pwm_config - change a PWM device configuration
 * @pwm: the PWM device
 * @period_us: period in microsecond
 * @duty_us: duty cycle in microsecond
 */

/**
 * pwm_enable - start a PWM output toggling
 * @pwm: the PWM device
 */

/**
 * pwm_disable - stop a PWM output toggling
 * @pwm: the PWM device
 */

#endif /* __PM8XXX_PWM_H__ */

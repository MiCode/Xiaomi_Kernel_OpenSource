/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __PMIC8058_PWM_H__
#define __PMIC8058_PWM_H__

/* The MAX value is computation limit. Hardware limit is 393 seconds. */
#define	PM_PWM_PERIOD_MAX		(274 * USEC_PER_SEC)
/* The MIN value is hardware limit. */
#define	PM_PWM_PERIOD_MIN		7 /* micro seconds */

struct pm8058_pwm_pdata {
	int 	(*config)(struct pwm_device *pwm, int ch, int on);
	int 	(*enable)(struct pwm_device *pwm, int ch, int on);
};

#define	PM_PWM_LUT_SIZE			64
#define	PM_PWM_LUT_DUTY_TIME_MAX	512	/* ms */
#define	PM_PWM_LUT_PAUSE_MAX		(7000 * PM_PWM_LUT_DUTY_TIME_MAX)

/* Flags for Look Up Table */
#define	PM_PWM_LUT_LOOP		0x01
#define	PM_PWM_LUT_RAMP_UP	0x02
#define	PM_PWM_LUT_REVERSE	0x04
#define	PM_PWM_LUT_PAUSE_HI_EN	0x10
#define	PM_PWM_LUT_PAUSE_LO_EN	0x20

#define	PM_PWM_LUT_NO_TABLE	0x100

/* PWM LED ID */
#define	PM_PWM_LED_0		0
#define	PM_PWM_LED_1		1
#define	PM_PWM_LED_2		2
#define	PM_PWM_LED_KPD		3
#define	PM_PWM_LED_FLASH	4
#define	PM_PWM_LED_FLASH1	5

/* PWM LED configuration mode */
#define	PM_PWM_CONF_NONE	0x0
#define	PM_PWM_CONF_PWM1	0x1
#define	PM_PWM_CONF_PWM2	0x2
#define	PM_PWM_CONF_PWM3	0x3
#define	PM_PWM_CONF_DTEST1	0x4
#define	PM_PWM_CONF_DTEST2	0x5
#define	PM_PWM_CONF_DTEST3	0x6
#define	PM_PWM_CONF_DTEST4	0x7

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
 * struct pm8058_pwm_period - PWM period structure
 * @pwm_size: enum pm_pwm_size
 * @clk: enum pm_pwm_clk
 * @pre_div: enum pm_pwm_pre_div
 * @pre_div_exp: exponent of 2 as part of pre-divider: 0..7
 */
struct pm8058_pwm_period {
	enum pm_pwm_size	pwm_size;
	enum pm_pwm_clk		clk;
	enum pm_pwm_pre_div	pre_div;
	int			pre_div_exp;
};

/**
 * pm8058_pwm_config_period - change PWM period
 *
 * @pwm: the PWM device
 * @pwm_p: period in struct pm8058_pwm_period
 */
int pm8058_pwm_config_period(struct pwm_device *pwm,
			     struct pm8058_pwm_period *pwm_p);

/**
 * pm8058_pwm_config_duty_cycle - change PWM duty cycle
 *
 * @pwm: the PWM device
 * @pwm_value: the duty cycle in raw PWM value (< 2^pwm_size)
 */
int pm8058_pwm_config_duty_cycle(struct pwm_device *pwm, int pwm_value);

/**
 * pm8058_pwm_lut_config - change a PWM device configuration to use LUT
 *
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
int pm8058_pwm_lut_config(struct pwm_device *pwm, int period_us,
			  int duty_pct[], int duty_time_ms, int start_idx,
			  int len, int pause_lo, int pause_hi, int flags);

/**
 * pm8058_pwm_lut_enable - control a PWM device to start/stop LUT ramp
 *
 * @pwm: the PWM device
 * @start: to start (1), or stop (0)
 */
int pm8058_pwm_lut_enable(struct pwm_device *pwm, int start);

int pm8058_pwm_set_dtest(struct pwm_device *pwm, int enable);

int pm8058_pwm_config_led(struct pwm_device *pwm, int id,
			  int mode, int max_current);

#endif /* __PMIC8058_PWM_H__ */

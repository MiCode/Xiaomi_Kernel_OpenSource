/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#define PM8XXX_PWM_PERIOD_MAX		(327 * USEC_PER_SEC)
#define PM8XXX_PWM_PERIOD_MIN		7 /* micro seconds */

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

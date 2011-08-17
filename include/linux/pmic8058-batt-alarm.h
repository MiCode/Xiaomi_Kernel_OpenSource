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
/*
 * Qualcomm PMIC 8058 Battery Alarm Device driver
 *
 */
#ifndef __PMIC8058_BATT_ALARM_H__
#define __PMIC8058_BATT_ALARM_H__

#include <linux/bitops.h>

/**
 * enum pm8058_batt_alarm_hold_time - hold time required for out of range
 *	battery voltage needed to trigger a status change.  Enum names denote
 *	hold time in milliseconds.
 */
enum pm8058_batt_alarm_hold_time {
	PM8058_BATT_ALARM_HOLD_TIME_0p125_MS = 0,
	PM8058_BATT_ALARM_HOLD_TIME_0p25_MS,
	PM8058_BATT_ALARM_HOLD_TIME_0p5_MS,
	PM8058_BATT_ALARM_HOLD_TIME_1_MS,
	PM8058_BATT_ALARM_HOLD_TIME_2_MS,
	PM8058_BATT_ALARM_HOLD_TIME_4_MS,
	PM8058_BATT_ALARM_HOLD_TIME_8_MS,
	PM8058_BATT_ALARM_HOLD_TIME_16_MS,
};

/*
 * Bits that are set in the return value of pm8058_batt_alarm_status_read
 * to indicate crossing of the upper or lower threshold.
 */
#define PM8058_BATT_ALARM_STATUS_BELOW_LOWER	BIT(0)
#define PM8058_BATT_ALARM_STATUS_ABOVE_UPPER	BIT(1)

/**
 * pm8058_batt_alarm_state_set - enable or disable the threshold comparators
 * @enable_lower_comparator: 1 = enable comparator, 0 = disable comparator
 * @enable_upper_comparator: 1 = enable comparator, 0 = disable comparator
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_batt_alarm_state_set(int enable_lower_comparator,
				int enable_upper_comparator);

/**
 * pm8058_batt_alarm_threshold_set - set the lower and upper alarm thresholds
 * @lower_threshold_mV: battery undervoltage threshold in millivolts
 * @upper_threshold_mV: battery overvoltage threshold in millivolts
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_batt_alarm_threshold_set(int lower_threshold_mV,
				    int upper_threshold_mV);

/**
 * pm8058_batt_alarm_status_read - get status of both threshold comparators
 *
 * RETURNS:	< 0	   = error
 *		  0	   = battery voltage ok
 *		BIT(0) set = battery voltage below lower threshold
 *		BIT(1) set = battery voltage above upper threshold
 */
int pm8058_batt_alarm_status_read(void);

/**
 * pm8058_batt_alarm_register_notifier - register a notifier to run when a
 *	battery voltage change interrupt fires
 * @nb:	notifier block containing callback function to register
 *
 * nb->notifier_call must point to a function of this form -
 * int (*notifier_call)(struct notifier_block *nb, unsigned long status,
 *			void *unused);
 * "status" will receive the battery alarm status; "unused" will be NULL.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_batt_alarm_register_notifier(struct notifier_block *nb);

/**
 * pm8058_batt_alarm_unregister_notifier - unregister a notifier that is run
 *	when a battery voltage change interrupt fires
 * @nb:	notifier block containing callback function to unregister
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_batt_alarm_unregister_notifier(struct notifier_block *nb);

/**
 * pm8058_batt_alarm_hold_time_set - set hold time of interrupt output *
 * @hold_time:	amount of time that battery voltage must remain outside of the
 *		threshold range before the battery alarm interrupt triggers
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_batt_alarm_hold_time_set(enum pm8058_batt_alarm_hold_time hold_time);

/**
 * pm8058_batt_alarm_pwm_rate_set - set battery alarm update rate *
 * @use_pwm:		1 = use PWM update rate, 0 = comparators always active
 * @clock_scaler:	PWM clock scaler = 2 to 9
 * @clock_divider:	PWM clock divider = 2 to 8
 *
 * This function sets the rate at which the battery alarm module enables
 * the threshold comparators.  The rate is determined by the following equation:
 *
 * f_update = (1024 Hz) / (clock_divider * (2 ^ clock_scaler))
 *
 * Thus, the update rate can range from 0.25 Hz to 128 Hz.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_batt_alarm_pwm_rate_set(int use_pwm, int clock_scaler,
				   int clock_divider);

#endif /* __PMIC8058_BATT_ALARM_H__ */

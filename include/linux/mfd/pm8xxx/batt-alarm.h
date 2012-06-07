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
 * Qualcomm PMIC PM8xxx Battery Alarm driver
 *
 */
#ifndef __MFD_PM8XXX_BATT_ALARM_H__
#define __MFD_PM8XXX_BATT_ALARM_H__

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/notifier.h>

#define PM8XXX_BATT_ALARM_DEV_NAME	"pm8xxx-batt-alarm"

/**
 * enum pm8xxx_batt_alarm_core_data - PMIC core specific core passed into the
 *	batter alarm driver as platform data
 * @irq_name:
 * @reg_addr_batt_alarm_threshold:	PMIC threshold register address
 * @reg_addr_batt_alarm_ctrl1:		PMIC control 1 register address
 * @reg_addr_batt_alarm_ctrl2:		PMIC control 2 register address
 * @reg_addr_batt_alarm_pwm_ctrl:	PMIC PWM control register address
 */
struct pm8xxx_batt_alarm_core_data {
	char	*irq_name;
	u16	reg_addr_threshold;
	u16	reg_addr_ctrl1;
	u16	reg_addr_ctrl2;
	u16	reg_addr_pwm_ctrl;
};

/**
 * enum pm8xxx_batt_alarm_comparator - battery alarm comparator ID values
 */
enum pm8xxx_batt_alarm_comparator {
	PM8XXX_BATT_ALARM_LOWER_COMPARATOR,
	PM8XXX_BATT_ALARM_UPPER_COMPARATOR,
};

/**
 * enum pm8xxx_batt_alarm_hold_time - hold time required for out of range
 *	battery voltage needed to trigger a status change.  Enum names denote
 *	hold time in milliseconds.
 */
enum pm8xxx_batt_alarm_hold_time {
	PM8XXX_BATT_ALARM_HOLD_TIME_0p125_MS = 0,
	PM8XXX_BATT_ALARM_HOLD_TIME_0p25_MS,
	PM8XXX_BATT_ALARM_HOLD_TIME_0p5_MS,
	PM8XXX_BATT_ALARM_HOLD_TIME_1_MS,
	PM8XXX_BATT_ALARM_HOLD_TIME_2_MS,
	PM8XXX_BATT_ALARM_HOLD_TIME_4_MS,
	PM8XXX_BATT_ALARM_HOLD_TIME_8_MS,
	PM8XXX_BATT_ALARM_HOLD_TIME_16_MS,
};

/*
 * Bits that are set in the return value of pm8xxx_batt_alarm_status_read
 * to indicate crossing of the upper or lower threshold.
 */
#define PM8XXX_BATT_ALARM_STATUS_BELOW_LOWER	BIT(0)
#define PM8XXX_BATT_ALARM_STATUS_ABOVE_UPPER	BIT(1)

#if defined(CONFIG_MFD_PM8XXX_BATT_ALARM) \
	|| defined(CONFIG_MFD_PM8XXX_BATT_ALARM_MODULE)

/**
 * pm8xxx_batt_alarm_enable - enable one of the battery voltage threshold
 *			      comparators
 * @comparator:	selects which comparator to enable
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_enable(enum pm8xxx_batt_alarm_comparator comparator);

/**
 * pm8xxx_batt_alarm_disable - disable one of the battery voltage threshold
 *			       comparators
 * @comparator:	selects which comparator to disable
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_disable(enum pm8xxx_batt_alarm_comparator comparator);


/**
 * pm8xxx_batt_alarm_threshold_set - set the lower and upper alarm thresholds
 * @comparator:		selects which comparator to set the threshold of
 * @threshold_mV:	battery voltage threshold in millivolts
 *			set points = 2500-5675 mV in 25 mV steps
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_threshold_set(
	enum pm8xxx_batt_alarm_comparator comparator, int threshold_mV);

/**
 * pm8xxx_batt_alarm_status_read - get status of both threshold comparators
 *
 * RETURNS:	< 0	   = error
 *		  0	   = battery voltage ok
 *		BIT(0) set = battery voltage below lower threshold
 *		BIT(1) set = battery voltage above upper threshold
 */
int pm8xxx_batt_alarm_status_read(void);

/**
 * pm8xxx_batt_alarm_register_notifier - register a notifier to run when a
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
int pm8xxx_batt_alarm_register_notifier(struct notifier_block *nb);

/**
 * pm8xxx_batt_alarm_unregister_notifier - unregister a notifier that is run
 *	when a battery voltage change interrupt fires
 * @nb:	notifier block containing callback function to unregister
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_unregister_notifier(struct notifier_block *nb);

/**
 * pm8xxx_batt_alarm_hold_time_set - set hold time of interrupt output *
 * @hold_time:	amount of time that battery voltage must remain outside of the
 *		threshold range before the battery alarm interrupt triggers
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_batt_alarm_hold_time_set(enum pm8xxx_batt_alarm_hold_time hold_time);

/**
 * pm8xxx_batt_alarm_pwm_rate_set - set battery alarm update rate *
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
int pm8xxx_batt_alarm_pwm_rate_set(int use_pwm, int clock_scaler,
				   int clock_divider);
#else

static inline int
pm8xxx_batt_alarm_enable(enum pm8xxx_batt_alarm_comparator comparator)
{ return -ENODEV; }

static inline int
pm8xxx_batt_alarm_disable(enum pm8xxx_batt_alarm_comparator comparator)
{ return -ENODEV; }

static inline int
pm8xxx_batt_alarm_threshold_set(enum pm8xxx_batt_alarm_comparator comparator,
				int threshold_mV)
{ return -ENODEV; }

static inline int pm8xxx_batt_alarm_status_read(void)
{ return -ENODEV; }

static inline int pm8xxx_batt_alarm_register_notifier(struct notifier_block *nb)
{ return -ENODEV; }

static inline int
pm8xxx_batt_alarm_unregister_notifier(struct notifier_block *nb)
{ return -ENODEV; }

static inline int
pm8xxx_batt_alarm_hold_time_set(enum pm8xxx_batt_alarm_hold_time hold_time)
{ return -ENODEV; }

static inline int
pm8xxx_batt_alarm_pwm_rate_set(int use_pwm, int clock_scaler, int clock_divider)
{ return -ENODEV; }

#endif


#endif /* __MFD_PM8XXX_BATT_ALARM_H__ */

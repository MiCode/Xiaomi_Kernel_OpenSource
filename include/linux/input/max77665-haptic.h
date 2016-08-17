/*
 * MAX77665 Haptic Driver
 *
 * Copyright (c) 2012, NVIDIA Corporation, All Rights Reserved
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _LINUX_INPUT_MAX77665_HAPTIC_H
#define _LINUX_INPUT_MAX77665_HAPTIC_H

#include <linux/platform_device.h>
#include <linux/mfd/max77665.h>

#define MAX77665_HAPTIC_REG_GENERAL	0x00
#define MAX77665_HAPTIC_REG_CONF1	0x01
#define MAX77665_HAPTIC_REG_CONF2	0x02
#define MAX77665_HAPTIC_REG_DRVCONF	0x03
#define MAX77665_HAPTIC_REG_CYCLECONF1	0x04
#define MAX77665_HAPTIC_REG_CYCLECONF2	0x05
#define MAX77665_HAPTIC_REG_SIGCONF1	0x06
#define MAX77665_HAPTIC_REG_SIGCONF2	0x07
#define MAX77665_HAPTIC_REG_SIGCONF3	0x08
#define MAX77665_HAPTIC_REG_SIGCONF4	0x09
#define MAX77665_HAPTIC_REG_SIGDC1	0x0a
#define MAX77665_HAPTIC_REG_SIGDC2	0x0b
#define MAX77665_HAPTIC_REG_SIGPWMDC1	0x0c
#define MAX77665_HAPTIC_REG_SIGPWMDC2	0x0d
#define MAX77665_HAPTIC_REG_SIGPWMDC3	0x0e
#define MAX77665_HAPTIC_REG_SIGPWMDC4	0x0f
#define MAX77665_HAPTIC_REG_MTR_REV	0x10
#define MAX77665_HAPTIC_REG_END		0x11

#define MAX77665_PMIC_REG_LSCNFG	0x2B

/* Haptic configuration 1 register */
#define MAX77665_INVERT_SHIFT		7
#define MAX77665_CONT_MODE_SHIFT	6
#define MAX77665_MOTOR_STRT_SHIFT	3

/* Haptic configuration 2 register */
#define MAX77665_MOTOR_TYPE_SHIFT	7
#define MAX77665_ENABLE_SHIFT		6
#define MAX77665_MODE_SHIFT		5

/* Haptic driver configuration register */
#define MAX77665_CYCLE_SHIFT		6
#define MAX77665_SIG_PERIOD_SHIFT	4
#define MAX77665_SIG_DUTY_SHIFT		2
#define MAX77665_PWM_DUTY_SHIFT		0

enum max77665_haptic_motor_type {
	MAX77665_HAPTIC_ERM,
	MAX77665_HAPTIC_LRA,
};

enum max77665_haptic_pulse_mode {
	MAX77665_EXTERNAL_MODE,
	MAX77665_INTERNAL_MODE,
};

enum max77665_haptic_pwm_divisor {
	MAX77665_PWM_DIVISOR_32,
	MAX77665_PWM_DIVISOR_64,
	MAX77665_PWM_DIVISOR_128,
	MAX77665_PWM_DIVISOR_256,
};

enum max77665_haptic_invert {
	MAX77665_INVERT_OFF,
	MAX77665_INVERT_ON,
};

enum max77665_haptic_continous_mode {
	MAX77665_NORMAL_MODE,
	MAX77665_CONT_MODE,
};

enum max77665_haptic_edp_states {
	MAX77665_HAPTIC_EDP_HIGH,
	MAX77665_HAPTIC_EDP_LOW,
	MAX77665_HAPTIC_EDP_NUM_STATES,
};

struct max77665_haptic_platform_data {
	int pwm_channel_id;
	int pwm_period;

	enum max77665_haptic_motor_type type;
	enum max77665_haptic_pulse_mode mode;
	enum max77665_haptic_pwm_divisor pwm_divisor;
	enum max77665_haptic_invert invert;
	enum max77665_haptic_continous_mode cont_mode;

	int internal_mode_pattern;
	int pattern_cycle;
	int pattern_signal_period;
	int feedback_duty_cycle;
	int motor_startup_val;
	int scf_val;

	unsigned int edp_states[MAX77665_HAPTIC_EDP_NUM_STATES];
};

#endif

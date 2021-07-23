/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2014-2015, Linux Foundation.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __DRV8846_H__
#define __DRV8846_H__

#define DEBUG

#include <linux/types.h>
#include <linux/ioctl.h>



#define UP 1
#define DOWN 0

#define MOTOR_IOC_MAGIC           'D'
#define MOTOR_PRIVATE             103

#define DRV8846_CLASS_NAME   "drv8846"
#define DRV8846_DEV_NAME     "ti,drv8846"
#define DRV8846_DRV_NAME     "ti-drv8846"

#define DEFAULT_PARAMS       0

#define PERIOD_DEFAULT       DEFAULT_PARAMS
#define DUTY_DEFAULT         DEFAULT_PARAMS
#define DURATION_DEFAULT     DEFAULT_PARAMS

#define DEFAULT_STEP_MODE    2

enum running_state {
	STILL = 10,
	SPEEDUP,
	SLOWDOWN,
	STOP
};

struct drv8846_private_data {
	uint32_t dir;                  /*Stepper motor diraction*/
	uint32_t speed_duration;       /*high speed mode motor running time, ms*/
	uint32_t speed_period;         /*high speed mode PWM period, ns*/
	uint32_t slow_duration;        /*slow mode motor running time, ms*/
	uint32_t slow_period;          /*slow mode PWM period, ns*/
	long pwm_time;                 /*PWM running time*/
	enum running_state pwm_state;  /*PWM device state*/
};


#define MOTOR_IOC_SET_AUTO \
	_IOW(MOTOR_IOC_MAGIC, MOTOR_PRIVATE + 0x01, struct drv8846_private_data)

#define MOTOR_IOC_SET_MANUAL \
	_IOW(MOTOR_IOC_MAGIC, MOTOR_PRIVATE + 0x02, struct drv8846_private_data)

#define MOTOR_IOC_GET_REMAIN_TIME \
	_IOR(MOTOR_IOC_MAGIC, MOTOR_PRIVATE + 0x10, struct drv8846_private_data)

#define MOTOR_IOC_GET_STATE \
	_IOR(MOTOR_IOC_MAGIC, MOTOR_PRIVATE + 0x11, struct drv8846_private_data)

#endif /* __DRV8846_H__ */

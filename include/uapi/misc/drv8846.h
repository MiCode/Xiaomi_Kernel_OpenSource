/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2014-2015, Linux Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _UAPI_MISC_DRV8846_H
#define _UAPI_MISC_DRV8846_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define DRV8846_MISC_NAME		"drv8846_dev"

#define UP 1
#define DOWN 0

/* Move back add delta ms to guarantee go home success */
#define DELTAMS 5

enum running_state {
	STILL = 0,
	SPEEDUP,
	FULLSTEAM,
	SLOWDOWN,
	UNIFORMSPEED
};

struct op_parameter {
	uint32_t dir;
	uint32_t duration_ms;
	uint32_t period_ns;
};

/* ioctl numbers */
#define MOTOR_MAGIC 0xD3
/* AFU devices */
#define MOTOR_IOC_SET_AUTORUN		_IOW(MOTOR_MAGIC, 0x01, uint8_t)
#define MOTOR_IOC_SET_MANUALRUN		_IOW(MOTOR_MAGIC, 0x02, struct op_parameter)
#define MOTOR_IOC_GET_REMAIN_TIME	_IOR(MOTOR_MAGIC, 0x10, long)
#define MOTOR_IOC_GET_STATE			_IOR(MOTOR_MAGIC, 0x11, enum running_state)

#endif /* _UAPI_MISC_DRV8846_H */

/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __ACC_FACTORY_H__
#define __ACC_FACTORY_H__

#include <linux/uaccess.h>
#include <sensors_io.h>
#include "cust_acc.h"
#include "accel.h"

struct accel_factory_fops {
	int (*enable_sensor)(bool enable_disable, int64_t sample_periods_ms);
	int (*get_data)(int32_t data[3], int *status);
	int (*get_raw_data)(int32_t data[3]);
	int (*enable_calibration)(void);
	int (*clear_cali)(void);
	int (*set_cali)(int32_t offset[3]);
	int (*get_cali)(int32_t offset[3]);
	int (*do_self_test)(void);
};

struct accel_factory_public {
	uint32_t gain;
	uint32_t sensitivity;
	struct accel_factory_fops *fops;
};
int accel_factory_device_register(struct accel_factory_public *dev);
int accel_factory_device_deregister(struct accel_factory_public *dev);
#endif

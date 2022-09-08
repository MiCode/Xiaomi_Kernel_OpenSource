/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GYRO_FACTORY_H__
#define __GYRO_FACTORY_H__

#include "cust_gyro.h"
#include "gyroscope.h"

struct gyro_factory_fops {
	int (*enable_sensor)(bool enable_disable, int64_t sample_periods_ms);
	int (*get_data)(int32_t data[3], int *status);
	int (*get_raw_data)(int32_t data[3]);
	int (*enable_calibration)(void);
	int (*clear_cali)(void);
	int (*set_cali)(int32_t offset[3]);
	int (*get_cali)(int32_t offset[3]);
	int (*do_self_test)(void);
};

struct gyro_factory_public {
	uint32_t gain;
	uint32_t sensitivity;
	struct gyro_factory_fops *fops;
};
int gyro_factory_device_register(struct gyro_factory_public *dev);
int gyro_factory_device_deregister(struct gyro_factory_public *dev);
#endif

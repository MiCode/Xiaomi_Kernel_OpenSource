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

#ifndef __MAG_FACTORY_H__
#define __MAG_FACTORY_H__

#include "mag.h"
#include "cust_mag.h"

extern struct mag_context *mag_context_obj;

#define SETCALI 1
#define CLRCALI 2
#define GETCALI 3

struct mag_factory_fops {
	int (*enable_sensor)(bool enable_disable, int64_t sample_periods_ms);
	int (*get_data)(int32_t data[3], int *status);
	int (*get_raw_data)(int32_t data[3]);
	int (*enable_calibration)(void);
	int (*clear_cali)(void);
	int (*set_cali)(int32_t offset[3]);
	int (*get_cali)(int32_t offset[3]);
	int (*do_self_test)(void);
};

struct mag_factory_public {
	uint32_t gain;
	uint32_t sensitivity;
	struct mag_factory_fops *fops;
};
int mag_factory_device_register(struct mag_factory_public *dev);
int mag_factory_device_deregister(struct mag_factory_public *dev);


#endif


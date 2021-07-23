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

#ifndef __BARO_FACTORY_H__
#define __BARO_FACTORY_H__

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/atomic.h>

#include <hwmsensor.h>
#include <sensors_io.h>
#include <hwmsen_helper.h>

#include "barometer.h"
#include "cust_baro.h"

struct baro_factory_fops {
	int (*enable_sensor)(bool enable_disable, int64_t sample_periods_ms);
	int (*get_data)(int32_t *data);
	int (*get_raw_data)(int32_t *data);
	int (*enable_calibration)(void);
	int (*clear_cali)(void);
	int (*set_cali)(int32_t offset);
	int (*get_cali)(int32_t *offset);
	int (*do_self_test)(void);
};

struct baro_factory_public {
	uint32_t gain;
	uint32_t sensitivity;
	struct baro_factory_fops *fops;
};
int baro_factory_device_register(struct baro_factory_public *dev);
int baro_factory_device_deregister(struct baro_factory_public *dev);
#endif

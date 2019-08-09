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

#ifndef __ALSPS_FACTORY_H__
#define __ALSPS_FACTORY_H__

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <hwmsen_helper.h>
#include <hwmsensor.h>
#include <sensors_io.h>

/*#include <mach/mt_typedefs.h>*/
/*#include <mach/mt_gpio.h>*/
/*#include <mach/mt_pm_ldo.h>*/

#include "alsps.h"
#include "cust_alsps.h"

struct alsps_factory_fops {
	int (*als_enable_sensor)(bool enable_disable,
				 int64_t sample_periods_ms);
	int (*als_get_data)(int32_t *data);
	int (*als_get_raw_data)(int32_t *data);
	int (*als_enable_calibration)(void);
	int (*als_clear_cali)(void);
	int (*als_set_cali)(int32_t offset);
	int (*als_get_cali)(int32_t *offset);

	int (*ps_enable_sensor)(bool enable_disable, int64_t sample_periods_ms);
	int (*ps_get_data)(int32_t *data);
	int (*ps_get_raw_data)(int32_t *data);
	int (*ps_enable_calibration)(void);
	int (*ps_clear_cali)(void);
	int (*ps_set_cali)(int32_t offset);
	int (*ps_get_cali)(int32_t *offset);
	int (*ps_set_threshold)(int32_t threshold[2]);
	int (*ps_get_threshold)(int32_t threshold[2]);
};

struct alsps_factory_public {
	uint32_t gain;
	uint32_t sensitivity;
	struct alsps_factory_fops *fops;
};
int alsps_factory_device_register(struct alsps_factory_public *dev);
int alsps_factory_device_deregister(struct alsps_factory_public *dev);
#endif

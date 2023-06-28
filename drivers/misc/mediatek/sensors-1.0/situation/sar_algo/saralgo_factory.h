/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SARALGO_FACTORY_H__
#define __SARALGO_FACTORY_H__

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


struct saralgo_factory_fops {
	int (*enable_sensor)(bool enabledisable,
			int64_t sample_periods_ms);
	int (*enable_top_sensor)(bool enabledisable,
			int64_t sample_periods_ms);
	int (*get_data)(int32_t sensor_data[3]);
	int (*step_set_cfg)(int32_t* step_en);
};

struct saralgo_factory_public {
	uint32_t gain;
	uint32_t sensitivity;
	struct saralgo_factory_fops *fops;
};
int saralgo_factory_device_register(struct saralgo_factory_public *dev);
int saralgo_factory_device_deregister(struct saralgo_factory_public *dev);
#endif

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

#ifndef __BIOMETRIC_H__
#define __BIOMETRIC_H__


#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <hwmsensor.h>
#include <linux/poll.h>
#include "sensor_attr.h"
#include "sensor_event.h"
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/i2c.h>
#include <sensors_io.h>

enum {
	ekg = 0,
	ppg1,
	ppg2,
	max_biometric_support,
};

struct biometric_control_path {
	int (*open_report_data)(int open);
	int (*batch)(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs);
	int (*flush)(void);
	bool is_support_batch;
};

struct biometric_data_path {
	int (*get_data)(int *raw_data, int *amb_data,
		int *agc_data, int8_t *status, u32 *length);
};

struct biometric_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct biometric_data_control_context {
	struct biometric_control_path biometric_ctl;
	struct biometric_data_path biometric_data;
	bool is_active_data;
	bool is_active_nodata;
	bool is_batch_enable;
	int power;
	int enable;
	int64_t delay_ns;
	int64_t latency_ns;
	int sn;
	int64_t read_time;
	int data[1024];
	int amb_data[1024];
	int agc_data[1024];
	int8_t status[1024];
};

struct biometric_context {
	struct sensor_attr_t mdev;
	struct mutex biometric_op_mutex;
	struct biometric_data_control_context
		ctl_context[max_biometric_support];
};

extern int biometric_data_report(int handle);
extern int biometric_flush_report(int handle);
extern int biometric_driver_add(struct biometric_init_info *obj, int handle);
extern int biometric_register_control_path(struct biometric_control_path *ctl,
					    int handle);
extern int biometric_register_data_path(struct biometric_data_path *data,
					 int handle);

#endif

/*
* Copyright (C) 2015 MediaTek Inc.
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

#ifndef __HUMIDITY_H__
#define __HUMIDITY_H__

#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <batch.h>
#include <sensors_io.h>
#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include "humidity_factory.h"

#define HMDY_TAG					"<HUMIDITY> "
#define HMDY_ERR(fmt, args...)	pr_err(HMDY_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define HMDY_VER(fmt, args...)	pr_debug(HMDY_TAG"%s: "fmt, __func__, ##args)

#define HMDY_LOGLEVEL 0

#if ((HMDY_LOGLEVEL) >= 0)
#define HMDY_FUN(f)				pr_debug(HMDY_TAG"%s\n", __func__)
#else
#define HMDY_FUN(f)
#endif

#if ((HMDY_LOGLEVEL) >= 1)
#define HMDY_LOG(fmt, args...)	pr_debug(HMDY_TAG fmt, ##args)
#else
#define HMDY_LOG(fmt, args...)
#endif


#define OP_HMDY_DELAY		0X01
#define	OP_HMDY_ENABLE		0X02
#define	OP_HMDY_GET_DATA	0X04

#define HMDY_INVALID_VALUE -1

#define EVENT_TYPE_HMDY_VALUE	REL_X
#define EVENT_TYPE_HMDY_STATUS	ABS_WHEEL

#define HMDY_VALUE_MAX (32767)
#define HMDY_VALUE_MIN (-32768)
#define HMDY_STATUS_MIN (0)
#define HMDY_STATUS_MAX (64)
#define HMDY_DIV_MAX (32767)
#define HMDY_DIV_MIN (1)

#define MAX_CHOOSE_HMDY_NUM 5

struct hmdy_control_path {
	int (*open_report_data)(int open);
	int (*enable_nodata)(int en);
	int (*set_delay)(u64 delay);
	int (*hmdyess_data_fifo)(void);
	bool is_report_input_direct;
	bool is_support_batch;
	bool is_use_common_factory;
};

struct hmdy_data_path {
	int (*get_data)(int *value, int *status);
	int (*get_raw_data)(int type, int *value);
	int vender_div;
};

struct hmdy_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct hmdy_data {
	struct hwm_sensor_data hmdy_data;
	int data_updata;
};

struct hmdy_drv_obj {
	void *self;
	int polling;
	int (*hmdy_operate)(void *self, uint32_t command, void *buff_in, int size_in, void *buff_out, int size_out,
			     int *actualout);
};

struct hmdy_context {
	struct input_dev *idev;
	struct miscdevice mdev;
	struct work_struct report;
	struct mutex hmdy_op_mutex;
	atomic_t delay;
	atomic_t wake;
	struct timer_list timer;
	atomic_t trace;

	struct hmdy_data drv_data;
	struct hmdy_control_path hmdy_ctl;
	struct hmdy_data_path hmdy_data;
	bool is_active_nodata;
	bool is_active_data;
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;
};

extern int hmdy_driver_add(struct hmdy_init_info *obj);
extern int hmdy_data_report(struct input_dev *dev, int value, int status);
extern int hmdy_register_control_path(struct hmdy_control_path *ctl);
extern int hmdy_register_data_path(struct hmdy_data_path *data);

#endif

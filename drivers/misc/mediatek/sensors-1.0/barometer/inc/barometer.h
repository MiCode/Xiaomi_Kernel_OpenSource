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

#ifndef __BARO_H__
#define __BARO_H__

#include <linux/atomic.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uaccess.h>
//#include <linux/wakelock.h>
#include "barometer_factory.h"
#include "sensor_attr.h"
#include "sensor_event.h"
#include <hwmsensor.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <sensors_io.h>

#define OP_BARO_DELAY 0X01
#define OP_BARO_ENABLE 0X02
#define OP_BARO_GET_DATA 0X04

#define BARO_INVALID_VALUE -1

#define EVENT_TYPE_BARO_VALUE REL_X
#define EVENT_TYPE_BARO_STATUS ABS_WHEEL
#define EVENT_TYPE_BARO_TIMESTAMP_HI REL_HWHEEL
#define EVENT_TYPE_BARO_TIMESTAMP_LO REL_DIAL

#define BARO_VALUE_MAX (32767)
#define BARO_VALUE_MIN (-32768)
#define BARO_STATUS_MIN (0)
#define BARO_STATUS_MAX (64)
#define BARO_DIV_MAX (32767)
#define BARO_DIV_MIN (1)

#define MAX_CHOOSE_BARO_NUM 5

struct baro_control_path {
	int (*open_report_data)(int open);
	int (*enable_nodata)(int en);
	int (*set_delay)(u64 delay);
	int (*batch)(int flag, int64_t samplingPeriodNs,
		     int64_t maxBatchReportLatencyNs);
	int (*flush)(void); /* open data rerport to HAL */
	int (*baroess_data_fifo)(void);
	bool is_report_input_direct;
	bool is_support_batch;
	bool is_use_common_factory;
};

struct baro_data_path {
	int (*get_data)(int *value, int *status);
	int (*get_raw_data)(int type, int *value);
	int vender_div;
};

struct baro_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct baro_data {
	struct hwm_sensor_data baro_data;
	int data_updata;
};

struct baro_drv_obj {
	void *self;
	int polling;
	int (*baro_operate)(void *self, uint32_t command, void *buff_in,
			    int size_in, void *buff_out, int size_out,
			    int *actualout);
};

struct baro_context {
	struct input_dev *idev;
	struct sensor_attr_t mdev;
	struct work_struct report;
	struct mutex baro_op_mutex;
	atomic_t delay;
	atomic_t wake;
	struct timer_list timer;
	struct hrtimer hrTimer;
	ktime_t target_ktime;
	atomic_t trace;
	struct workqueue_struct *baro_workqueue;

	struct baro_data drv_data;
	struct baro_control_path baro_ctl;
	struct baro_data_path baro_data;
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;
	int power;
	int enable;
	int64_t delay_ns;
	int64_t latency_ns;
};

extern int baro_driver_add(struct baro_init_info *obj);
extern int baro_data_report(int value, int status, int64_t nt);
extern int baro_flush_report(void);
extern int baro_register_control_path(struct baro_control_path *ctl);
extern int baro_register_data_path(struct baro_data_path *data);

#endif

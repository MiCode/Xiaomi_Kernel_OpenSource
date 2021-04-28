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

#ifndef __GYROSCOPE_H__
#define __GYROSCOPE_H__

//#include <linux/wakelock.h>
#include "sensor_attr.h"
#include <hwmsen_helper.h>
#include <hwmsensor.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <sensors_io.h>

#include "gyro_factory.h"
#include "sensor_event.h"

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (1)
#endif

#define OP_GYRO_DELAY 0X01
#define OP_GYRO_ENABLE 0X02
#define OP_GYRO_GET_DATA 0X04
#define GYRO_INVALID_VALUE -1
#define EVENT_TYPE_GYRO_X ABS_X
#define EVENT_TYPE_GYRO_Y ABS_Y
#define EVENT_TYPE_GYRO_Z ABS_Z
#define EVENT_TYPE_GYRO_TEMPERATURE ABS_MISC
#define EVENT_TYPE_GYRO_UPDATE REL_X
#define EVENT_TYPE_GYRO_STATUS ABS_WHEEL
#define EVENT_TYPE_GYRO_UPDATE REL_X
#define EVENT_TYPE_GYRO_TIMESTAMP_HI REL_HWHEEL
#define EVENT_TYPE_GYRO_TIMESTAMP_LO REL_DIAL

#define GYRO_VALUE_MAX (32767)
#define GYRO_VALUE_MIN (-32768)
#define GYRO_STATUS_MIN (0)
#define GYRO_STATUS_MAX (64)
#define GYRO_DIV_MAX (32767)
#define GYRO_DIV_MIN (1)
#define GYRO_AXIS_X 0
#define GYRO_AXIS_Y 1
#define GYRO_AXIS_Z 2
#define MAX_CHOOSE_GYRO_NUM 5
#define GYRO_AXES_NUM 3

struct gyro_control_path {
	int (*open_report_data)(int open);
	int (*enable_nodata)(int en);
	int (*set_delay)(u64 delay);
	int (*batch)(int flag, int64_t samplingPeriodNs,
		     int64_t maxBatchReportLatencyNs);
	int (*flush)(void);
	int (*set_cali)(uint8_t *data, uint8_t count);
	bool is_report_input_direct;
	bool is_support_batch;
	bool is_use_common_factory;
};

struct gyro_data_path {
	int (*get_data)(int *x, int *y, int *z, int *status);
	int (*get_raw_data)(int *x, int *y, int *z);
	int (*get_temperature)(int *temperature);
	int vender_div;
};

struct gyro_init_info {
	char *name;
	int (*init)(struct platform_device *pdev);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct gyro_data {
	int8_t status;
	int8_t reserved[3];
	int x;
	int y;
	int z;
	int64_t timestamp;
	int temperature;
	void *reserved1;
};

struct gyro_drv_obj {
	void *self;
	int polling;
	int (*gyro_operate)(void *self, uint32_t command, void *buff_in,
			    int size_in, void *buff_out, int size_out,
			    int *actualout);
};

struct gyro_context {
	struct input_dev *idev;
	struct sensor_attr_t mdev;
	struct work_struct report;
	struct mutex gyro_op_mutex;
	atomic_t delay; /*polling period for reporting input event*/
	atomic_t wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list timer; /* polling timer */
	struct hrtimer hrTimer;
	ktime_t target_ktime;
	atomic_t trace;
	struct workqueue_struct *gyro_workqueue;
	struct gyro_data drv_data;
	int cali_sw[GYRO_AXES_NUM + 1];
	struct gyro_control_path gyro_ctl;
	struct gyro_data_path gyro_data;
	/*Active, but HAL don't need data sensor. such as  orientation need*/
	bool is_active_nodata;
	bool is_active_data;   /* Active and HAL need data .*/
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;
	int power;
	int enable;
	int64_t delay_ns;
	int64_t latency_ns;
};

extern int gyro_driver_add(struct gyro_init_info *obj);
extern int gyro_data_report(struct gyro_data *data);
extern int gyro_bias_report(struct gyro_data *data);
extern int gyro_cali_report(struct gyro_data *data);
extern int gyro_temp_report(int32_t *temp);
extern int gyro_flush_report(void);
extern int gyro_register_control_path(struct gyro_control_path *ctl);
extern int gyro_register_data_path(struct gyro_data_path *data);
#endif

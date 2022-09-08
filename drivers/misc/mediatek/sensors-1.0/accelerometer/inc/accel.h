/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ACC_H__
#define __ACC_H__

#include "sensor_attr.h"
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/ioctl.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/uaccess.h>

#include "sensor_event.h"
#include <hwmsen_helper.h>
#include <hwmsensor.h>
#include <linux/poll.h>
#include <sensors_io.h>

#include "accel_factory.h"

#define OP_ACC_DELAY 0X01
#define OP_ACC_ENABLE 0X02
#define OP_ACC_GET_DATA 0X04

#define ACC_INVALID_VALUE -1

#define ACC_AXIS_X 0
#define ACC_AXIS_Y 1
#define ACC_AXIS_Z 2

#define MAX_CHOOSE_G_NUM 5
#define ACC_AXES_NUM 3
struct acc_control_path {
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

struct acc_data_path {
	int (*get_data)(int *x, int *y, int *z, int *status);
	int (*get_raw_data)(int *x, int *y, int *z);
	int vender_div;
};

struct acc_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct acc_data {
	int8_t status;
	int8_t reserved[3];
	int x;
	int y;
	int z;
	int64_t timestamp;
	void *reserved1;
} __packed;

struct acc_drv_obj {
	void *self;
	int polling;
	int (*acc_operate)(void *self, uint32_t command, void *buff_in,
			   int size_in, void *buff_out, int size_out,
			   int *actualout);
};

struct acc_context {
	struct input_dev *idev;
	struct sensor_attr_t mdev;
	struct work_struct report;
	struct mutex acc_op_mutex;
	atomic_t delay; /*polling period for reporting input event*/
	atomic_t wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list timer; /* polling timer */
	struct hrtimer hrTimer;
	ktime_t target_ktime;
	atomic_t trace;
	struct workqueue_struct *accel_workqueue;

	atomic_t early_suspend;
	/* struct acc_drv_obj    drv_obj; */
	struct acc_data drv_data;
	int cali_sw[ACC_AXES_NUM + 1];
	struct acc_control_path acc_ctl;
	struct acc_data_path acc_data;
	/* Active, but HAL don't need data sensor. such as orientation need */
	bool is_active_nodata;
	bool is_active_data; /* Active and HAL need data . */
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable; /* v2.judging whether sensor is in batch mode */
	int power;
	int enable;
	int64_t delay_ns;
	int64_t latency_ns;
};

/* for auto detect */
extern int acc_driver_add(struct acc_init_info *obj);
extern int acc_data_report(struct acc_data *data);
extern int acc_bias_report(struct acc_data *data);
extern int acc_cali_report(struct acc_data *data);
extern int acc_flush_report(void);
extern int acc_register_control_path(struct acc_control_path *ctl);
extern int acc_register_data_path(struct acc_data_path *data);
extern int acc_probe(void);
extern int acc_remove(void);

#endif

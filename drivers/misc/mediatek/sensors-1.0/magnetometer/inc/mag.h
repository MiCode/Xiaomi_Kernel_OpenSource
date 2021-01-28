/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MAG_H__
#define __MAG_H__

//#include <linux/wakelock.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include "mag_factory.h"
#include "sensor_attr.h"
#include "sensor_event.h"
#include <hwmsen_helper.h>
#include <hwmsensor.h>
#include <sensors_io.h>


#define OP_MAG_DELAY 0X01
#define OP_MAG_ENABLE 0X02
#define OP_MAG_GET_DATA 0X04

#define MAG_INVALID_VALUE -1

#define EVENT_TYPE_MAGEL_X ABS_X
#define EVENT_TYPE_MAGEL_Y ABS_Y
#define EVENT_TYPE_MAGEL_Z ABS_Z
#define EVENT_TYPE_MAGEL_UPDATE REL_X
#define EVENT_DIV_MAGEL ABS_RUDDER
#define EVENT_TYPE_MAGEL_STATUS ABS_WHEEL
#define EVENT_TYPE_MAG_UPDATE REL_X
#define EVENT_TYPE_MAG_TIMESTAMP_HI REL_HWHEEL
#define EVENT_TYPE_MAG_TIMESTAMP_LO REL_DIAL

#define MAG_DIV_MAX (32767)
#define MAG_DIV_MIN (1)

#define MAG_VALUE_MAX (32767)
#define MAG_VALUE_MIN (-32768)
#define MAG_STATUS_MIN (0)
#define MAG_STATUS_MAX (64)

#define MAX_CHOOSE_G_NUM 5

#define MAX_M_V_SENSOR 5

#define ID_M_V_MAGNETIC 0

struct mag_data_path {
	int div;
	int (*get_data)(int *x, int *y, int *z, int *status);
	int (*get_raw_data)(int *x, int *y, int *z);
};

struct mag_libinfo_t {
	char libname[64];
	int32_t layout;
	int32_t deviceid;
};

struct mag_control_path {
	int (*open_report_data)(int en);
	int (*set_delay)(u64 delay);
	int (*enable)(int en);
	int (*batch)(int flag, int64_t samplingPeriodNs,
		     int64_t maxBatchReportLatencyNs);
	int (*flush)(void);
	int (*set_cali)(uint8_t *data, uint8_t count);
	bool is_report_input_direct;
	bool is_support_batch;
	bool is_use_common_factory;
	struct mag_libinfo_t libinfo;
};

struct mag_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct mag_data {
	int8_t status;
	int8_t reserved[3];
	int x;
	int y;
	int z;
	int64_t timestamp;
	void *reserved1;
};

struct mag_drv_obj {
	void *self;
	int polling;
	int (*mag_operate)(void *self, uint32_t command, void *buff_in,
			   int size_in, void *buff_out, int size_out,
			   int *actualout);
};

struct mag_context {
	struct sensor_attr_t mdev;
	struct work_struct report;
	struct mutex mag_op_mutex;
	atomic_t delay; /*polling period for reporting input event*/
	atomic_t wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list timer; /* polling timer */
	struct hrtimer hrTimer;
	ktime_t target_ktime;
	atomic_t trace;
	struct workqueue_struct *mag_workqueue;

	struct mag_data_path mag_dev_data;
	struct mag_control_path mag_ctl;
	atomic_t early_suspend;
	struct mag_data drv_data;
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;
	int power;
	int enable;
	int64_t delay_ns;
	int64_t latency_ns;
};

extern int mag_attach(int sensor, struct mag_drv_obj *obj);

extern int mag_driver_add(struct mag_init_info *obj);
extern int mag_data_report(struct mag_data *data);
extern int mag_bias_report(struct mag_data *data);
extern int mag_flush_report(void);
extern int mag_info_record(struct mag_libinfo_t *p_mag_info);
extern int mag_cali_report(int32_t *param);
extern int mag_register_control_path(struct mag_control_path *ctl);
extern int mag_register_data_path(struct mag_data_path *ctl);
#endif

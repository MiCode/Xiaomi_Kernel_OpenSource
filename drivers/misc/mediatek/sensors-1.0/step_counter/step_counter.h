/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __STEP_C_H__
#define __STEP_C_H__


//#include <linux/pm_wakeup.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/atomic.h>
#include <linux/ioctl.h>

#include <sensors_io.h>
#include <hwmsen_helper.h>
#include <hwmsensor.h>
#include <linux/poll.h>
#include "sensor_attr.h"
#include "sensor_event.h"

#define	OP_STEP_C_DELAY		0X01
#define	OP_STEP_C_ENABLE		0X02
#define	OP_STEP_C_GET_DATA	0X04

#define STEP_C_INVALID_VALUE -1

#define EVENT_TYPE_STEP_C_VALUE				ABS_X
#define EVENT_TYPE_STEP_C_STATUS			ABS_WHEEL
#define EVENT_TYPE_STEP_DETECTOR_VALUE		REL_Y
#define EVENT_TYPE_SIGNIFICANT_VALUE		REL_Z



#define STEP_C_VALUE_MAX (32767)
#define STEP_C_VALUE_MIN (-32768)
#define STEP_C_STATUS_MIN (0)
#define STEP_C_STATUS_MAX (64)
#define STEP_C_DIV_MAX (32767)
#define STEP_C_DIV_MIN (1)


#define MAX_CHOOSE_STEP_C_NUM 5

struct step_c_control_path {
	int (*open_report_data)(int open);/* open data rerport to HAL */
	int (*enable_nodata)(int en);/* only enable not report event to HAL */
	int (*enable_step_detect)(int en);
	int (*enable_significant)(int en);
	int (*enable_floor_c)(int en);
	int (*step_c_set_delay)(u64 delay);
	int (*step_d_set_delay)(u64 delay);
	int (*floor_c_set_delay)(u64 delay);
	int (*step_c_batch)(int flag,
		int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
	int (*step_c_flush)(void);/* open data rerport to HAL */
	int (*step_d_batch)(int flag,
		int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
	int (*step_d_flush)(void);/* open data rerport to HAL */
	int (*smd_batch)(int flag,
		int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
	int (*smd_flush)(void);/* open data rerport to HAL */
	int (*floor_c_batch)(int flag,
		int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
	int (*floor_c_flush)(void);/* open data rerport to HAL */
	bool is_report_input_direct;
	bool is_counter_support_batch;/* version2. batch M support flag */
	bool is_detector_support_batch;/* version2. batch M support flag */
	bool is_smd_support_batch;/* version2. batch M support flag */
	bool is_floor_c_support_batch;/* version2. batch M support flag */
};

struct step_c_data_path {
	int (*get_data)(uint32_t *value, int *status);
	int (*get_data_step_d)(uint32_t *value, int *status);
	int (*get_data_significant)(uint32_t *value, int *status);
	int (*get_data_floor_c)(uint32_t *value, int *status);
	int vender_div;
};

struct step_c_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct step_c_data {
	uint32_t counter;
	int status;
	int data_updata;
	uint32_t floor_counter;
	int floor_c_status;
	int floor_c_data_updata;
};

struct step_c_drv_obj {
	void *self;
	int polling;
	int (*step_c_operate)(void *self, uint32_t command, void *buff_in,
		int size_in, void *buff_out, int size_out, int *actualout);
};

struct step_c_context {
	struct input_dev   *idev;
	struct sensor_attr_t   mdev;
	struct work_struct  report;
	struct mutex step_c_op_mutex;
	atomic_t delay; /*polling period for reporting input event*/
	atomic_t wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;

	atomic_t                early_suspend;

	struct step_c_data       drv_data;
	struct step_c_control_path   step_c_ctl;
	struct step_c_data_path   step_c_data;
	bool is_active_nodata;
	bool is_active_data;		/* Active and HAL need */
	bool is_floor_c_active_data;	/* Active and HAL need */
	bool is_first_data_after_enable;
	bool is_first_floor_c_data_after_enable;
	bool is_polling_run;
	bool is_step_c_batch_enable;	/* v2. judge whether sensor in batch M*/
	bool is_step_d_batch_enable;	/* v2. judge whether sensor in batch M*/
	bool is_floor_c_batch_enable;   /* v2. judge whether sensor in batch M*/
};

/* for auto detect */
enum STEP_NOTIFY_TYPE {
	TYPE_STEP_NON   = 0,
	TYPE_STEP_DETECTOR  = 1,
	TYPE_SIGNIFICANT = 2
};

extern int  step_notify_t(enum STEP_NOTIFY_TYPE type, int64_t time_stamp);
extern int  step_notify(enum STEP_NOTIFY_TYPE type);

extern int step_c_driver_add(struct step_c_init_info *obj);
extern int step_c_data_report_t(uint32_t new_counter, int status,
	int64_t time_stamp);
extern int step_c_data_report(uint32_t new_counter, int status);
extern int step_c_flush_report(void);
extern int step_d_flush_report(void);
extern int smd_flush_report(void);
int floor_c_data_report_t(uint32_t new_counter, int status, int64_t time_stamp);
int floor_c_data_report(uint32_t new_counter, int status);
int floor_c_flush_report(void);
extern int step_c_register_control_path(struct step_c_control_path *ctl);
extern int step_c_register_data_path(struct step_c_data_path *data);

#endif

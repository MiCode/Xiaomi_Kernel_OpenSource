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

#ifndef __ALSPS_H__
#define __ALSPS_H__

#include <linux/atomic.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uaccess.h>
//#include <linux/wakelock.h>
#include "alsps_factory.h"
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

#define ALSPS_TAG "<ALS/PS> "

#define OP_ALSPS_DELAY 0X01
#define OP_ALSPS_ENABLE 0X02
#define OP_ALSPS_GET_DATA 0X04

#define ALSPS_INVALID_VALUE -1

#define EVENT_TYPE_ALS_VALUE ABS_X
#define EVENT_TYPE_PS_VALUE REL_Z
#define EVENT_TYPE_ALS_STATUS ABS_WHEEL
#define EVENT_TYPE_PS_STATUS REL_Y

#define ALSPS_VALUE_MAX (32767)
#define ALSPS_VALUE_MIN (-32768)
#define ALSPS_STATUS_MIN (0)
#define ALSPS_STATUS_MAX (64)
#define ALSPS_DIV_MAX (32767)
#define ALSPS_DIV_MIN (1)

#define MAX_CHOOSE_ALSPS_NUM 5

struct als_control_path {
	int (*open_report_data)(int open); /* open data rerport to HAL */
	int (*enable_nodata)(int en); /* only enable not report event to HAL */
	int (*set_delay)(u64 delay);
	int (*batch)(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs);
	int (*flush)(void);	    /* open data rerport to HAL */
	int (*set_cali)(uint8_t *data, uint8_t count);
	int (*rgbw_enable)(int en);
	int (*rgbw_batch)(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs);
	int (*rgbw_flush)(void);
	int (*access_data_fifo)(void);
	bool is_report_input_direct;
	bool is_support_batch;
	bool is_polling_mode;
	bool is_use_common_factory;
};

struct ps_control_path {
	int (*open_report_data)(int open); /* open data rerport to HAL */
	int (*enable_nodata)(int en); /* only enable not report event to HAL */
	int (*set_delay)(u64 delay);
	int (*batch)(int flag, int64_t samplingPeriodNs,
		     int64_t maxBatchReportLatencyNs);
	int (*flush)(void);	    /* open data rerport to HAL */
	int (*access_data_fifo)(void); /* version2.used for flush operate */
	int (*ps_calibration)(int type, int value);
	int (*set_cali)(uint8_t *data, uint8_t count);
	bool is_report_input_direct;
	bool is_support_batch; /* version2.used for batch mode support flag */
	bool is_polling_mode;
	bool is_use_common_factory;
};

struct als_data_path {
	int (*get_data)(int *als_value, int *status);
	int (*als_get_raw_data)(int *als_value);
	int vender_div;
};

struct ps_data_path {
	int (*get_data)(int *ps_value, int *status);
	int (*ps_get_raw_data)(int *ps_value);
	int vender_div;
};

struct alsps_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct alsps_data {
	struct hwm_sensor_data als_data;
	struct hwm_sensor_data ps_data;
	int data_updata;
};

struct alsps_drv_obj {
	void *self;
	int polling;
	int (*alsps_operate)(void *self, uint32_t command, void *buff_in,
			     int size_in, void *buff_out, int size_out,
			     int *actualout);
};

struct alsps_context {
	struct input_dev *idev;
	struct sensor_attr_t als_mdev;
	struct sensor_attr_t ps_mdev;
	struct work_struct report_ps;
	struct work_struct report_als;
	struct mutex alsps_op_mutex;
	struct timer_list timer_als; /*als polling timer */
	struct timer_list timer_ps;  /* ps polling timer */

	atomic_t trace;
	atomic_t delay_als; /*als polling period for reporting input event*/
	atomic_t delay_ps;  /*ps polling period for reporting input event*/
	atomic_t wake;      /*user-space request to wake-up, used with stop*/

	atomic_t early_suspend;

	struct alsps_data drv_data;
	struct als_control_path als_ctl;
	struct als_data_path als_data;
	struct ps_control_path ps_ctl;
	struct ps_data_path ps_data;
	/* Active, but HAL don't need data sensor.such as orientation need */
	bool is_als_active_nodata;
	bool is_als_active_data;   /* Active and HAL need data . */
	/* Active, but HAL don't need data sensor.such as orientation need */
	bool is_ps_active_nodata;
	bool is_ps_active_data;    /* Active and HAL need data . */

	bool is_als_first_data_after_enable;
	bool is_ps_first_data_after_enable;
	bool is_als_polling_run;
	bool is_ps_polling_run;
	/* v2.judging whether sensor is in batch mode */
	bool is_als_batch_enable;
	bool is_ps_batch_enable;
	bool is_get_valid_ps_data_after_enable;
	bool is_get_valid_als_data_after_enable;
	int als_power;
	int rgbw_power;
	int als_enable;
	int rgbw_enable;
	int64_t als_delay_ns;
	int64_t als_latency_ns;
	int64_t rgbw_delay_ns;
	int64_t rgbw_latency_ns;
	int ps_power;
	int ps_enable;
	int64_t ps_delay_ns;
	int64_t ps_latency_ns;
};

/* AAL Functions */
extern int alsps_aal_enable(int enable);
extern int alsps_aal_get_status(void);
extern int alsps_aal_get_data(void);

/* for auto detect */
extern int alsps_driver_add(struct alsps_init_info *obj);
extern int ps_report_interrupt_data(int value);
extern int ps_flush_report(void);
extern int als_data_report(int value, int status);
extern int ps_cali_report(int *value);
extern int als_cali_report(int *value);
extern int als_flush_report(void);
extern int rgbw_data_report(int value[4]);
extern int rgbw_flush_report(void);
extern int als_register_control_path(struct als_control_path *ctl);
extern int als_register_data_path(struct als_data_path *data);
extern int ps_data_report(int value, int status);
extern int ps_register_control_path(struct ps_control_path *ctl);
extern int ps_register_data_path(struct ps_data_path *data);
extern struct platform_device *get_alsps_platformdev(void);
#endif

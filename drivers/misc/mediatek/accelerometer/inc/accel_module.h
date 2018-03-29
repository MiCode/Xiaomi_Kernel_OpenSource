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

#ifndef __ACC_H__
#define __ACC_H__

#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/atomic.h>
#include <linux/ioctl.h>

#include <batch.h>
#include <sensors_io.h>
#include <hwmsen_helper.h>
#include <hwmsensor.h>
#include <hwmsen_dev.h>


#define ACC_TAG						"<ACCELEROMETER> "
#define ACC_ERR(fmt, args...)		pr_err(ACC_TAG fmt, ##args)
#define ACC_LOG(fmt, args...)		pr_err(ACC_TAG fmt, ##args)
#define ACC_VER(fmt, args...)		pr_err(ACC_TAG fmt, ##args)

#define OP_ACC_DELAY	0X01
#define	OP_ACC_ENABLE	0X02
#define	OP_ACC_GET_DATA	0X04

#define ACC_INVALID_VALUE -1

#define EVENT_TYPE_ACCEL_X			ABS_X
#define EVENT_TYPE_ACCEL_Y			ABS_Y
#define EVENT_TYPE_ACCEL_Z			ABS_Z
#define EVENT_TYPE_ACCEL_UPDATE              REL_X
#define EVENT_TYPE_ACCEL_TIMESTAMP_HI		REL_HWHEEL
#define EVENT_TYPE_ACCEL_TIMESTAMP_LO		REL_DIAL
#define EVENT_TYPE_ACCEL_STATUS     ABS_WHEEL
#define EVENT_TYPE_ACCEL_DIV        ABS_GAS


#define ACC_VALUE_MAX (32767)
#define ACC_VALUE_MIN (-32768)
#define ACC_STATUS_MIN (0)
#define ACC_STATUS_MAX (64)
#define ACC_DIV_MAX (32767)
#define ACC_DIV_MIN (1)
#define ACC_AXIS_X 0
#define ACC_AXIS_Y 1
#define ACC_AXIS_Z 2

#define MAX_CHOOSE_G_NUM 5
#define ACC_AXES_NUM 3
struct acc_control_path {
	int (*open_report_data)(int open);/* open data rerport to HAL */
	int (*enable_nodata)(int en);/* only enable not report event to HAL */
	int (*set_delay)(u64 delay);
	int (*access_data_fifo)(void);/* version2.used for flush operate */
	bool is_report_input_direct;
	bool is_support_batch;/* version2.used for batch mode support flag */
	bool is_use_common_factory;
	int (*acc_calibration)(int type, int cali[3]);/* version3 sensor common layer factory mode API1 */
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
	struct hwm_sensor_data acc_data;
	int data_updata;
	/* struct mutex lock; */
};

struct acc_drv_obj {
	void *self;
	int polling;
	int (*acc_operate)(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout);
};

struct acc_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex acc_op_mutex;
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	struct hrtimer		hrTimer;
	ktime_t			target_ktime;
	atomic_t            trace;
	struct workqueue_struct	*accel_workqueue;

	atomic_t                early_suspend;
	/* struct acc_drv_obj    drv_obj; */
	struct acc_data       drv_data;
	int                   cali_sw[ACC_AXES_NUM+1];
	struct acc_control_path   acc_ctl;
	struct acc_data_path   acc_data;
	/* Active, but HAL don't need data sensor. such as orientation need */
	bool			is_active_nodata;
	bool			is_active_data;		/* Active and HAL need data . */
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;	/* version2.this is used for judging whether sensor is in batch mode */
};


/* for auto detect */
extern int acc_driver_add(struct acc_init_info *obj);
extern int acc_data_report(int x, int y, int z, int status, int64_t nt);
extern int acc_register_control_path(struct acc_control_path *ctl);
extern int acc_register_data_path(struct acc_data_path *data);

#endif

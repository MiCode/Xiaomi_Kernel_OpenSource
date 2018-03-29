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
#include <linux/kernel.h>

#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include <hwmsen_helper.h>
#include <batch.h>

#include "gyro_factory.h"

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (1)
#endif

#define GYRO_TAG					"<GYROSCOPE> "
#define GYRO_FUN(f)				pr_debug(GYRO_TAG"%s\n", __func__)
#define GYRO_ERR(fmt, args...)	pr_err(GYRO_TAG fmt, ##args)
#define GYRO_LOG(fmt, args...)	pr_debug(GYRO_TAG fmt, ##args)
#define GYRO_VER(fmt, args...)  pr_debug(GYRO_TAG fmt, ##args)

#define OP_GYRO_DELAY	0X01
#define	OP_GYRO_ENABLE	0X02
#define	OP_GYRO_GET_DATA	0X04
#define GYRO_INVALID_VALUE -1
#define EVENT_TYPE_GYRO_X			ABS_X
#define EVENT_TYPE_GYRO_Y			ABS_Y
#define EVENT_TYPE_GYRO_Z			ABS_Z
#define EVENT_TYPE_GYRO_UPDATE               REL_X
#define EVENT_TYPE_GYRO_STATUS     ABS_WHEEL
#define EVENT_TYPE_GYRO_UPDATE                 REL_X
#define EVENT_TYPE_GYRO_TIMESTAMP_HI		REL_HWHEEL
#define EVENT_TYPE_GYRO_TIMESTAMP_LO		REL_DIAL


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
	bool is_report_input_direct;
	bool is_support_batch;
	int (*gyro_calibration)(int type, int cali[3]);
	bool is_use_common_factory;
};

struct gyro_data_path {
	int (*get_data)(int *x, int *y, int *z, int *status);
	int (*get_raw_data)(int *x, int *y, int *z);
	int vender_div;
};

struct gyro_init_info {
	char *name;
	int (*init)(struct platform_device *pdev);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct gyro_data {
	struct hwm_sensor_data gyro_data;
	int data_updata;
};

struct gyro_drv_obj {
	void *self;
	int polling;
	int (*gyro_operate)(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout);
};

struct gyro_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex gyro_op_mutex;
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	struct hrtimer      hrTimer;
	ktime_t             target_ktime;
	atomic_t            trace;
	struct workqueue_struct *gyro_workqueue;
	struct gyro_data       drv_data;
	int                    cali_sw[GYRO_AXES_NUM+1];
	struct gyro_control_path   gyro_ctl;
	struct gyro_data_path   gyro_data;
	bool is_active_nodata;		/*Active, but HAL don't need data sensor. such as orientation need*/
	bool is_active_data;		/* Active and HAL need data .*/
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;
};

extern int gyro_driver_add(struct gyro_init_info *obj);
extern int gyro_data_report(int x, int y, int z, int status, int64_t nt);
extern int gyro_register_control_path(struct gyro_control_path *ctl);
extern int gyro_register_data_path(struct gyro_data_path *data);
#endif

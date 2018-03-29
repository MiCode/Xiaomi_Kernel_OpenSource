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

#ifndef __UNCALI_MAG_H__
#define __UNCALI_MAG_H__


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

#define DEBUG

#ifdef DEBUG
#define UNCALI_MAG_TAG						"<UNCALI_MAG> "
#define UNCALI_MAG_FUN(f)					pr_debug(UNCALI_MAG_TAG"%s\n", __func__)
#define UNCALI_MAG_ERR(fmt, args...)		pr_err(UNCALI_MAG_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define UNCALI_MAG_LOG(fmt, args...)		pr_debug(UNCALI_MAG_TAG fmt, ##args)
#define UNCALI_MAG_VER(fmt, args...)		pr_debug(UNCALI_MAG_TAG"%s: "fmt, __func__, ##args) /* ((void)0) */
#define UNCALI_MAG_DBGMSG					pr_debug("%s, %d\n", __func__, __LINE__)
#else
#define UNCALI_MAG_TAG					"<UNCALI_MAG> "
#define UNCALI_MAG_FUN(f)
#define UNCALI_MAG_ERR(fmt, args...)
#define UNCALI_MAG_LOG(fmt, args...)
#define UNCALI_MAG_VER(fmt, args...)
#define UNCALI_MAG_DBGMSG
#endif
#define OP_UNCALI_MAG_DELAY	0X01
#define	OP_UNCALI_MAG_ENABLE	0X02
#define	OP_UNCALI_MAG_GET_DATA	0X04

#define UNCALI_MAG_INVALID_VALUE -1

#define EVENT_TYPE_UNCALI_MAG_X					ABS_X
#define EVENT_TYPE_UNCALI_MAG_Y					ABS_Y
#define EVENT_TYPE_UNCALI_MAG_Z					ABS_Z
#define EVENT_TYPE_UNCALI_MAG_X_BIAS			ABS_RX
#define EVENT_TYPE_UNCALI_MAG_Y_BIAS			ABS_RY
#define EVENT_TYPE_UNCALI_MAG_Z_BIAS			ABS_RZ
#define EVENT_TYPE_UNCALI_MAG_UPDATE			REL_X
#define EVENT_TYPE_UNCALI_MAG_TIMESTAMP_HI		REL_HWHEEL
#define EVENT_TYPE_UNCALI_MAG_TIMESTAMP_LO		REL_DIAL

#define UNCALI_MAG_VALUE_MAX (32767)
#define UNCALI_MAG_VALUE_MIN (-32768)
#define UNCALI_MAG_STATUS_MIN (0)
#define UNCALI_MAG_STATUS_MAX (64)
#define UNCALI_MAG_DIV_MAX (32767)
#define UNCALI_MAG_DIV_MIN (1)


#define MAX_CHOOSE_UNCALI_MAG_NUM 5

struct uncali_mag_control_path {
	int (*open_report_data)(int open);/* open data rerport to HAL */
	int (*enable_nodata)(int en);/* only enable not report event to HAL */
	int (*set_delay)(u64 delay);
	int (*access_data_fifo)(void);/* version2.used for flush operate */
	bool is_report_input_direct;
	bool is_support_batch;/* version2.used for batch mode support flag */
	int (*uncali_mag_calibration)(int type, int cali[3]);/* version3 sensor common layer factory mode API1 */
};

struct uncali_mag_data_path {
	int (*get_data)(int *dat, int *offset, int *status);
	int vender_div;
};

struct uncali_mag_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct uncali_mag_data {
	struct hwm_sensor_data uncali_mag_data;
	int data_updata;
};

struct uncali_mag_drv_obj {
	void *self;
	int polling;
	int (*uncali_mag_operate)(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout);
};

struct uncali_mag_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex uncali_mag_op_mutex;
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;
	atomic_t            enable;
	/* struct uncali_mag_drv_obj    drv_obj; */
	struct uncali_mag_data       drv_data;
	struct uncali_mag_control_path   uncali_mag_ctl;
	struct uncali_mag_data_path   uncali_mag_data;
	bool			is_active_nodata;
	bool			is_active_data;		/* Active and HAL need data . */
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;	/* version2.this is used for judging whether sensor is in batch mode */
};

/* driver API for internal */
/* extern int uncali_mag_enable_nodata(int enable); */
/* extern int uncali_mag_attach(struct uncali_mag_drv_obj *obj); */
/* driver API for third party vendor */

/* for auto detect */
extern int uncali_mag_driver_add(struct uncali_mag_init_info *obj);
extern int uncali_mag_data_report(int *data, int status, int64_t nt);
extern int uncali_mag_register_control_path(struct uncali_mag_control_path *ctl);
extern int uncali_mag_register_data_path(struct uncali_mag_data_path *data);

#endif

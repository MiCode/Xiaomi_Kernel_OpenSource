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


#ifndef __GMRV_H__
#define __GMRV_H__


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


/* #define DEBUG */

#ifdef DEBUG
#define GMRV_TAG					"<GMRV> "
#define GMRV_FUN(f)				pr_debug(GMRV_TAG"%s\n", __func__)
#define GMRV_ERR(fmt, args...)		pr_err(GMRV_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GMRV_LOG(fmt, args...)		pr_debug(GMRV_TAG fmt, ##args)
#define GMRV_VER(fmt, args...)		pr_debug(GMRV_TAG"%s: "fmt, __func__, ##args)	/* ((void)0) */
#else
#define GMRV_TAG					"<GMRV> "
#define GMRV_FUN(f)
#define GMRV_ERR(fmt, args...)
#define GMRV_LOG(fmt, args...)
#define GMRV_VER(fmt, args...)
#endif
#define OP_GMRV_DELAY	0X01
#define	OP_GMRV_ENABLE	0X02
#define	OP_GMRV_GET_DATA	0X04

#define GMRV_INVALID_VALUE -1

#define EVENT_TYPE_GMRV_X				REL_RX
#define EVENT_TYPE_GMRV_Y				REL_RY
#define EVENT_TYPE_GMRV_Z				REL_RZ
#define EVENT_TYPE_GMRV_SCALAR			REL_WHEEL
#define EVENT_TYPE_GMRV_STATUS			REL_X
#define EVENT_TYPE_GMRV_TIMESTAMP_HI	REL_HWHEEL
#define EVENT_TYPE_GMRV_TIMESTAMP_LO	REL_DIAL

#define GMRV_VALUE_MAX (32767)
#define GMRV_VALUE_MIN (-32768)
#define GMRV_STATUS_MIN (0)
#define GMRV_STATUS_MAX (64)
#define GMRV_DIV_MAX (32767)
#define GMRV_DIV_MIN (1)


#define MAX_CHOOSE_GMRV_NUM 5

struct gmrv_control_path {
	int (*open_report_data)(int open);	/* open data rerport to HAL */
	int (*enable_nodata)(int en);	/* only enable not report event to HAL */
	int (*set_delay)(u64 delay);
	int (*access_data_fifo)(void);	/* version2.used for flush operate */
	bool is_report_input_direct;
	bool is_support_batch;
	int (*gmrv_calibration)(int type, int cali[3]);
};

struct gmrv_data_path {
	int (*get_data)(int *x, int *y, int *z, int *scalar, int *status);
	int (*get_raw_data)(int *x, int *y, int *z, int *scalar);
	int vender_div;
};

struct gmrv_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct gmrv_data {
	struct hwm_sensor_data gmrv_data;
	int data_updata;
	/* struct mutex lock; */
};

struct gmrv_drv_obj {
	void *self;
	int polling;
	int (*gmrv_operate)(void *self, uint32_t command, void *buff_in, int size_in,
			     void *buff_out, int size_out, int *actualout);
};

struct gmrv_context {
	struct input_dev *idev;
	struct miscdevice mdev;
	struct work_struct report;
	struct mutex gmrv_op_mutex;
	atomic_t delay;		/*polling period for reporting input event */
	atomic_t wake;		/*user-space request to wake-up, used with stop */
	struct timer_list timer;	/* polling timer */
	atomic_t trace;
	atomic_t			enable;
	atomic_t early_suspend;
	/* struct gmrv_drv_obj    drv_obj; */
	struct gmrv_data drv_data;
	struct gmrv_control_path gmrv_ctl;
	struct gmrv_data_path gmrv_data;
	bool is_active_nodata;	/* Active, but HAL don't need data sensor. such as orientation need */
	bool is_active_data;	/* Active and HAL need data . */
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;	/* version2.this is used for judging whether sensor is in batch mode */
};

/* driver API for internal */
/* extern int gmrv_enable_nodata(int enable); */
/* extern int gmrv_attach(struct gmrv_drv_obj *obj); */
/* driver API for third party vendor */

/* for auto detect */
extern int gmrv_driver_add(struct gmrv_init_info *obj);
extern int gmrv_data_report(int x, int y, int z, int scalar, int status, int64_t nt);
extern int gmrv_register_control_path(struct gmrv_control_path *ctl);
extern int gmrv_register_data_path(struct gmrv_data_path *data);

#endif

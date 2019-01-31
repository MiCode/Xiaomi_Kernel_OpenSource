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


#ifndef __FUSION_H__
#define __FUSION_H__

//#include <linux/pm_wakeup.h>
#include <linux/interrupt.h>
#include "sensor_attr.h"
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/atomic.h>
#include <linux/ioctl.h>

#include <sensors_io.h>
#include <hwmsen_helper.h>
#include <hwmsensor.h>
#include "sensor_event.h"

#define OP_FUSION_DELAY	0X01
#define	OP_FUSION_ENABLE	0X02
#define	OP_FUSION_GET_DATA	0X04

#define FUSION_INVALID_VALUE -1

/* ORIENTATION, GRV, GMRV, RV, LA, GRAVITY, UNCALI_GYRO, UNCALI_MAG, PDR */
enum fusion_handle {
	orientation,
	grv,
	gmrv,
	rv,
	la,
	grav,
	ungyro,
	unmag,
	pdr,
	max_fusion_support,
};

struct fusion_control_path {
	int (*open_report_data)(int open);	/* open data rerport to HAL */
	int (*enable_nodata)(int en);  /* only enable not report event to HAL */
	int (*set_delay)(u64 delay);
	int (*batch)(int flag,
		int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
	int (*flush)(void);/* open data rerport to HAL */
	int (*access_data_fifo)(void);/* version2.used for flush operate */
	bool is_report_input_direct;
	bool is_support_batch;/* version2.used for batch mode support flag */
	int (*fusion_calibration)(int type, int cali[3]);/* v3  factory API1 */
};

struct fusion_data_path {
	int (*get_data)(int *x, int *y, int *z, int *scalar, int *status);
	int (*get_raw_data)(int *x, int *y, int *z, int *scalar);/* v3 API2 */
	int vender_div;
};

struct fusion_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
};

struct fusion_data {
	struct hwm_sensor_data fusion_data;
	int data_updata;
	/* struct mutex lock; */
};

struct fusion_drv_obj {
	void *self;
	int polling;
	int (*fusion_operate)(void *self, uint32_t command, void *buff_in,
		int size_in, void *buff_out, int size_out, int *actualout);
};

struct fusion_control_context {
	struct fusion_data drv_data;
	struct fusion_control_path fusion_ctl;
	struct fusion_data_path fusion_data;
	bool is_active_nodata;
	bool is_active_data;
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;
	int power;
	int enable;
	int64_t delay_ns;
	int64_t latency_ns;
};
struct fusion_context {
	struct sensor_attr_t mdev;
	struct work_struct report;
	struct mutex fusion_op_mutex;
	atomic_t trace;
	struct fusion_control_context fusion_context[max_fusion_support];
	/* struct fusion_drv_obj    drv_obj; */
};

/* driver API for internal */
/* extern int fusion_enable_nodata(int enable); */
/* extern int fusion_attach(struct fusion_drv_obj *obj); */
/* driver API for third party vendor */

/* for auto detect */
extern int fusion_driver_add(struct fusion_init_info *obj, int handle);
extern int fusion_register_control_path(struct fusion_control_path *ctl,
	int handle);
extern int fusion_register_data_path(struct fusion_data_path *data,
	int handle);
extern int rv_data_report(int x, int y, int z,
	int scalar, int status, int64_t nt);
extern int rv_flush_report(void);
extern int grv_data_report(int x, int y, int z,
	int scalar, int status, int64_t nt);
extern int grv_flush_report(void);
extern int gmrv_data_report(int x, int y, int z,
	int scalar, int status, int64_t nt);
extern int gmrv_flush_report(void);
extern int grav_data_report(int x, int y, int z, int status, int64_t nt);
extern int grav_flush_report(void);
extern int la_data_report(int x, int y, int z, int status, int64_t nt);
extern int la_flush_report(void);
extern int orientation_data_report(int x, int y, int z, int status, int64_t nt);
extern int orientation_flush_report(void);
extern int orientation_data_report(int x, int y, int z, int status, int64_t nt);
extern int orientation_flush_report(void);
extern int uncali_gyro_data_report(int *data, int status, int64_t nt);
extern int uncali_gyro_flush_report(void);
extern int uncali_mag_data_report(int *data, int status, int64_t nt);
extern int uncali_mag_flush_report(void);
#endif

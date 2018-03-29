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


#ifndef __PEDOMETER_V1_H__
#define __PEDOMETER_V1_H__

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



#define PEDO_TAG					"<PEDOMETER> "
#define PEDO_FUN(f)				pr_debug(PEDO_TAG"%s\n", __func__)
#define PEDO_ERR(fmt, args...)	pr_err(PEDO_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define PEDO_LOG(fmt, args...)	pr_debug(PEDO_TAG fmt, ##args)
#define PEDO_VER(fmt, args...)  pr_debug(PEDO_TAG"%s: "fmt, __func__, ##args)

#define OP_PEDO_DELAY	0X01
#define	OP_PEDO_ENABLE	0X02
#define	OP_PEDO_GET_DATA	0X04

#define PEDO_INVALID_VALUE -1

#define EVENT_TYPE_PEDO_LENGTH			REL_X
#define EVENT_TYPE_PEDO_FREQUENCY		REL_Y
#define EVENT_TYPE_PEDO_COUNT			REL_Z
#define EVENT_TYPE_PEDO_DISTANCE		REL_RX
#define EVENT_TYPE_PEDO_STATUS			ABS_WHEEL
#define EVENT_TYPE_PEDO_TIMESTAMP_HI		REL_HWHEEL
#define EVENT_TYPE_PEDO_TIMESTAMP_LO		REL_DIAL


#define PEDO_VALUE_MAX (32767)
#define PEDO_VALUE_MIN (-32768)
#define PEDO_STATUS_MIN (0)
#define PEDO_STATUS_MAX (64)
#define PEDO_DIV_MAX (32767)
#define PEDO_DIV_MIN (1)

#define MAX_CHOOSE_PEDO_NUM 5

struct pedo_control_path {
	int (*open_report_data)(int open);
	int (*enable_nodata)(int en);
	int (*set_delay)(u64 delay);
	bool is_report_input_direct;
	bool is_support_batch;
};

typedef struct {
	uint32_t length;
	uint32_t frequency;
	uint32_t count;
	uint32_t distance;
} pedometer_t;

struct pedo_data_path {
	int (*get_data)(struct hwm_sensor_data *pedo_data, int *status);
	int vender_div;
};

struct pedo_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct pedo_data {
	struct hwm_sensor_data pedo_data;
	int data_updata;
};

struct pedo_drv_obj {
	void *self;
	int polling;
	int (*pedo_operate)(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout);
};

struct pedo_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex pedo_op_mutex;
	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;
	atomic_t			enable;
	struct pedo_data       drv_data;
	struct pedo_control_path   pedo_ctl;
	struct pedo_data_path   pedo_data;
	bool			is_active_nodata;
	bool			is_active_data;
	bool is_first_data_after_enable;
	bool is_polling_run;
	bool is_batch_enable;
};

extern int pedo_driver_add(struct pedo_init_info *obj);
extern int pedo_data_report(struct hwm_sensor_data *data, int status);
extern int pedo_register_control_path(struct pedo_control_path *ctl);
extern int pedo_register_data_path(struct pedo_data_path *data);
#endif

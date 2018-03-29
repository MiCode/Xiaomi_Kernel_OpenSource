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

#ifndef __TILT_H__
#define __TILT_H__


#include <linux/wakelock.h>
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
#include <hwmsen_dev.h>
#include <linux/poll.h>
#include "sensor_attr.h"
#include "sensor_event.h"

#define TILT_TAG		"<TILT_DETECTOR> "
#define TILT_FUN(f)		pr_debug(TILT_TAG"%s\n", __func__)
#define TILT_ERR(fmt, args...)	pr_err(TILT_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define TILT_LOG(fmt, args...)	pr_debug(TILT_TAG fmt, ##args)
#define TILT_VER(fmt, args...)  pr_debug(TILT_TAG"%s: "fmt, __func__, ##args)	/* ((void)0) */

/* #define OP_TILT_DELAY         0X01 */
#define	OP_TILT_ENABLE		0X02
/* #define OP_TILT_GET_DATA      0X04 */

#define TILT_INVALID_VALUE -1

#define EVENT_TYPE_TILT_VALUE		REL_X

#define TILT_VALUE_MAX (32767)
#define TILT_VALUE_MIN (-32768)
#define TILT_STATUS_MIN (0)
#define TILT_STATUS_MAX (64)
#define TILT_DIV_MAX (32767)
#define TILT_DIV_MIN (1)

typedef enum {
	TILT_DEACTIVATE,
	TILT_ACTIVATE,
	TILT_SUSPEND,
	TILT_RESUME
} tilt_state_e;

struct tilt_control_path {
/* int (*enable_nodata)(int en);//only enable not report event to HAL */
	int (*open_report_data)(int open);	/* open data rerport to HAL */
	int (*set_delay)(uint64_t delay);	/* open data rerport to HAL */
	int (*batch)(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
	int (*flush)(void);/* open data rerport to HAL */
	bool is_report_input_direct;
	bool is_support_batch;/* version2.used for batch mode support flag */
};

struct tilt_data_path {
	int (*get_data)(int *value, int *status);
};

struct tilt_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct tilt_data {
	struct hwm_sensor_data tilt_data;
	int data_updata;
	/* struct mutex lock; */
};

struct tilt_drv_obj {
	void *self;
	int polling;
	int (*tilt_operate)(void *self, uint32_t command, void *buff_in, int size_in,
			     void *buff_out, int size_out, int *actualout);
};

struct tilt_context {
	struct input_dev *idev;
	struct sensor_attr_t mdev;
	struct work_struct report;
	struct mutex tilt_op_mutex;
	atomic_t wake;		/*user-space request to wake-up, used with stop */
	atomic_t trace;

	atomic_t early_suspend;
	atomic_t suspend;

	struct tilt_data drv_data;
	struct tilt_control_path tilt_ctl;
	struct tilt_data_path tilt_data;
	bool is_active_nodata;	/* Active, but HAL don't need data sensor. such as orientation need */
	bool is_active_data;	/* Active and HAL need data . */
	bool is_batch_enable;	/* version2.this is used for judging whether sensor is in batch mode */
};

extern int tilt_notify(void);
extern int tilt_driver_add(struct tilt_init_info *obj);
extern int tilt_flush_report(void);
extern int tilt_register_control_path(struct tilt_control_path *ctl);
extern int tilt_register_data_path(struct tilt_data_path *data);

#endif

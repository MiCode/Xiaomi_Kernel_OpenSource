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

#ifndef __ANSWER_CALL_H__
#define __ANSWER_CALL_H__


#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <hwmsensor.h>
#include <hwmsen_dev.h>


#define ANCALL_TAG		"<ANSWER_CALL> "
#define ANCALL_FUN(f)		pr_warn(ANCALL_TAG"%s\n", __func__)
#define ANCALL_ERR(fmt, args...)	pr_warn(ANCALL_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define ANCALL_LOG(fmt, args...)	pr_warn(ANCALL_TAG fmt, ##args)
#define ANCALL_VER(fmt, args...)  pr_warn(ANCALL_TAG"%s: "fmt, __func__, ##args)


#define	OP_ANCALL_ENABLE		0X02
#define GLG_INVALID_VALUE -1
#define EVENT_TYPE_ANSWER_CALL_VALUE		REL_X

#define ANSWER_CALL_VALUE_MAX (32767)
#define ANSWER_CALL_VALUE_MIN (-32768)
#define ANSWER_CALL_STATUS_MIN (0)
#define ANSWER_CALL_STATUS_MAX (64)
#define ANSWER_CALL_DIV_MAX (32767)
#define ANSWER_CALL_DIV_MIN (1)

typedef enum {
	ANSWER_CALL_DEACTIVATE,
	ANSWER_CALL_ACTIVATE,
	ANSWER_CALL_SUSPEND,
	ANSWER_CALL_RESUME
} ancall_state_e;

struct ancall_control_path {
/* int (*enable_nodata)(int en);//only enable not report event to HAL */
	int (*open_report_data)(int open);	/* open data rerport to HAL */
/* int (*enable)(int en); */
	/* bool is_support_batch;//version2.used for batch mode support flag */
};

struct ancall_data_path {
	int (*get_data)(int *value, int *status);
};

struct ancall_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct ancall_data {
	struct hwm_sensor_data ancall_data;
	int data_updata;
	/* struct mutex lock; */
};

struct ancall_drv_obj {
	void *self;
	int polling;
	int (*ancall_operate)(void *self, uint32_t command, void *buff_in, int size_in,
			    void *buff_out, int size_out, int *actualout);
};

struct ancall_context {
	struct input_dev *idev;
	struct miscdevice mdev;
	struct work_struct report;
	struct mutex ancall_op_mutex;
	atomic_t wake;		/*user-space request to wake-up, used with stop */
	atomic_t trace;
	struct timer_list notify_timer;

	atomic_t early_suspend;
	atomic_t suspend;

	struct ancall_data drv_data;
	struct ancall_control_path ancall_ctl;
	struct ancall_data_path ancall_data;
	bool is_active_nodata;	/* Active, but HAL don't need data sensor. such as orientation need */
	bool is_active_data;	/* Active and HAL need data . */
	bool is_batch_enable;	/* version2.this is used for judging whether sensor is in batch mode */
};

extern int ancall_notify(void);
extern int ancall_driver_add(struct ancall_init_info *obj);
extern int ancall_register_control_path(struct ancall_control_path *ctl);
extern int ancall_register_data_path(struct ancall_data_path *data);

#endif

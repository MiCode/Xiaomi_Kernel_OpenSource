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

#ifndef __FREEFALL_H__
#define __FREEFALL_H__


#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/hwmsensor.h>
#include <linux/earlysuspend.h>
#include <linux/hwmsen_dev.h>


#define FREEFALL_TAG		"<FREEFALL> "
#define FREEFALL_FUN(f)		printk(FREEFALL_TAG"%s\n", __func__)
#define FREEFALL_ERR(fmt, args...)	printk(FREEFALL_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define FREEFALL_LOG(fmt, args...)	printk(FREEFALL_TAG fmt, ##args)
#define FREEFALL_VER(fmt, args...)  printk(FREEFALL_TAG"%s: "fmt, __func__, ##args)	/* ((void)0) */

/* #define OP_FREEFALL_DELAY             0X01 */
#define	OP_FREEFALL_ENABLE		0X02
/* #define OP_FREEFALL_GET_DATA  0X04 */

#define FREEFALL_INVALID_VALUE -1

#define EVENT_TYPE_FREEFALL_VALUE		REL_X

#define FREEFALL_VALUE_MAX (32767)
#define FREEFALL_VALUE_MIN (-32768)
#define FREEFALL_STATUS_MIN (0)
#define FREEFALL_STATUS_MAX (64)
#define FREEFALL_DIV_MAX (32767)
#define FREEFALL_DIV_MIN (1)

typedef enum {
	FREEFALL_DEACTIVATE,
	FREEFALL_ACTIVATE,
	FREEFALL_SUSPEND,
	FREEFALL_RESUME
} freefall_state_e;

struct freefall_control_path {
	/* int (*enable_nodata)(int en);//only enable not report event to HAL */
	int (*open_report_data)(int open);	/* open data rerport to HAL */
	/* int (*enable)(int en); */
	/* bool is_support_batch;//version2.used for batch mode support flag */
};

struct freefall_data_path {
	int (*get_data)(u16 *value, int *status);
};

struct freefall_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct freefall_data {
	hwm_sensor_data freefall_data;
	int data_updata;
};

struct freefall_drv_obj {
	void *self;
	int polling;
	int (*freefall_operate)(void *self, uint32_t command, void *buff_in, int size_in,
				 void *buff_out, int size_out, int *actualout);
};

struct freefall_context {
	struct input_dev *idev;
	struct miscdevice mdev;
	struct work_struct report;
	struct mutex freefall_op_mutex;
	atomic_t wake;		/*user-space request to wake-up, used with stop */
	atomic_t trace;
	struct timer_list notify_timer;

	struct early_suspend early_drv;
	atomic_t early_suspend;
	atomic_t suspend;

	struct freefall_data drv_data;
	struct freefall_control_path freefall_ctl;
	struct freefall_data_path freefall_data;
	bool is_active_nodata;	/* Active, but HAL don't need data sensor. such as orientation need */
	bool is_active_data;	/* Active and HAL need data . */
	bool is_batch_enable;	/* version2.this is used for judging whether sensor is in batch mode */
};

extern int freefall_notify(void);
extern int freefall_driver_add(struct freefall_init_info *obj);
extern int freefall_register_control_path(struct freefall_control_path *ctl);
extern int freefall_register_data_path(struct freefall_data_path *data);

#endif

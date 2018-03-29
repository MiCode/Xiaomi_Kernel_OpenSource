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

#ifndef __SHK_H__
#define __SHK_H__


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


#define SHK_TAG		"<SHAKE> "
#define SHK_FUN(f)		pr_debug(SHK_TAG"%s\n", __func__)
#define SHK_ERR(fmt, args...)	pr_err(SHK_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define SHK_LOG(fmt, args...)	pr_debug(SHK_TAG fmt, ##args)
#define SHK_VER(fmt, args...)  pr_debug(SHK_TAG"%s: "fmt, __func__, ##args)	/* ((void)0) */

/* #define OP_SHK_DELAY          0X01 */
#define	OP_SHK_ENABLE		0X02
/* #define OP_SHK_GET_DATA       0X04 */

#define SHK_INVALID_VALUE -1

#define EVENT_TYPE_SHK_VALUE		REL_X

#define SHK_VALUE_MAX (32767)
#define SHK_VALUE_MIN (-32768)
#define SHK_STATUS_MIN (0)
#define SHK_STATUS_MAX (64)
#define SHK_DIV_MAX (32767)
#define SHK_DIV_MIN (1)

typedef enum {
	SHK_DEACTIVATE,
	SHK_ACTIVATE,
	SHK_SUSPEND,
	SHK_RESUME
} shk_state_e;

struct shk_control_path {
/* int (*enable_nodata)(int en);//only enable not report event to HAL */
	int (*open_report_data)(int open);	/* open data rerport to HAL */
/* int (*enable)(int en); */
	/* bool is_support_batch;//version2.used for batch mode support flag */
};

struct shk_data_path {
	int (*get_data)(u16 *value, int *status);
};

struct shk_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct shk_data {
	hwm_sensor_data shk_data;
	int data_updata;
	/* struct mutex lock; */
};

struct shk_drv_obj {
	void *self;
	int polling;
	int (*shk_operate)(void *self, uint32_t command, void *buff_in, int size_in,
			    void *buff_out, int size_out, int *actualout);
};

struct shk_context {
	struct input_dev *idev;
	struct miscdevice mdev;
	struct work_struct report;
	struct mutex shk_op_mutex;
	atomic_t wake;		/*user-space request to wake-up, used with stop */
	atomic_t trace;

	struct early_suspend early_drv;
	atomic_t early_suspend;
	atomic_t suspend;

	struct shk_data drv_data;
	struct shk_control_path shk_ctl;
	struct shk_data_path shk_data;
	bool is_active_nodata;	/* Active, but HAL don't need data sensor. such as orientation need */
	bool is_active_data;	/* Active and HAL need data . */
	bool is_batch_enable;	/* version2.this is used for judging whether sensor is in batch mode */
};

extern int shk_notify(void);
extern int shk_driver_add(struct shk_init_info *obj);
extern int shk_register_control_path(struct shk_control_path *ctl);
extern int shk_register_data_path(struct shk_data_path *data);

#endif

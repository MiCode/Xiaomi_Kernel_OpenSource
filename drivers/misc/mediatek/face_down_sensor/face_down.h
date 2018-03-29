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

#ifndef __FDN_H__
#define __FDN_H__


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


#define FDN_TAG		"<FACE_DOWN> "
#define FDN_FUN(f)		pr_debug(FDN_TAG"%s\n", __func__)
#define FDN_ERR(fmt, args...)	pr_err(FDN_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define FDN_LOG(fmt, args...)	pr_debug(FDN_TAG fmt, ##args)
#define FDN_VER(fmt, args...)  pr_debug(FDN_TAG"%s: "fmt, __func__, ##args)	/* ((void)0) */

/* #define OP_FDN_DELAY          0X01 */
#define	OP_FDN_ENABLE		0X02
/* #define OP_FDN_GET_DATA       0X04 */

#define FDN_INVALID_VALUE -1

#define EVENT_TYPE_FDN_VALUE		REL_X

#define FDN_VALUE_MAX (32767)
#define FDN_VALUE_MIN (-32768)
#define FDN_STATUS_MIN (0)
#define FDN_STATUS_MAX (64)
#define FDN_DIV_MAX (32767)
#define FDN_DIV_MIN (1)

typedef enum {
	FDN_DEACTIVATE,
	FDN_ACTIVATE,
	FDN_SUSPEND,
	FDN_RESUME
} fdn_state_e;

struct fdn_control_path {
/* int (*enable_nodata)(int en);//only enable not report event to HAL */
	int (*open_report_data)(int open);	/* open data rerport to HAL */
/* int (*enable)(int en); */
	/* bool is_support_batch;//version2.used for batch mode support flag */
};

struct fdn_data_path {
	int (*get_data)(u16 *value, int *status);
};

struct fdn_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct fdn_data {
	hwm_sensor_data fdn_data;
	int data_updata;
	/* struct mutex lock; */
};

struct fdn_drv_obj {
	void *self;
	int polling;
	int (*fdn_operate)(void *self, uint32_t command, void *buff_in, int size_in,
			    void *buff_out, int size_out, int *actualout);
};

struct fdn_context {
	struct input_dev *idev;
	struct miscdevice mdev;
	struct work_struct report;
	struct mutex fdn_op_mutex;
	atomic_t wake;		/*user-space request to wake-up, used with stop */
	atomic_t trace;

	struct early_suspend early_drv;
	atomic_t early_suspend;
	atomic_t suspend;

	struct fdn_data drv_data;
	struct fdn_control_path fdn_ctl;
	struct fdn_data_path fdn_data;
	bool is_active_nodata;	/* Active, but HAL don't need data sensor. such as orientation need */
	bool is_active_data;	/* Active and HAL need data . */
	bool is_batch_enable;	/* version2.this is used for judging whether sensor is in batch mode */
};

extern int fdn_notify(void);
extern int fdn_driver_add(struct fdn_init_info *obj);
extern int fdn_register_control_path(struct fdn_control_path *ctl);
extern int fdn_register_data_path(struct fdn_data_path *data);

#endif

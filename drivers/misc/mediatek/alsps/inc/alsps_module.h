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

#ifndef __ALSPS_H__
#define __ALSPS_H__

#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <batch.h>
#include <sensors_io.h>
#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include "alsps_factory.h"

#define ALSPS_TAG					"<ALS/PS> "
#define ALSPS_FUN(f)			printk(ALSPS_TAG"%s\n", __func__)
#define ALSPS_ERR(fmt, args...)	printk(ALSPS_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define ALSPS_LOG(fmt, args...)	printk(ALSPS_TAG fmt, ##args)
#define ALSPS_VER(fmt, args...)   printk(ALSPS_TAG"%s: "fmt, __func__, ##args) /* ((void)0) */

#define   OP_ALSPS_DELAY	0X01
#define	OP_ALSPS_ENABLE	0X02
#define	OP_ALSPS_GET_DATA	0X04

#define ALSPS_INVALID_VALUE -1

#define EVENT_TYPE_ALS_VALUE			ABS_X
#define EVENT_TYPE_PS_VALUE			REL_Z
#define EVENT_TYPE_ALS_STATUS		ABS_WHEEL
#define EVENT_TYPE_PS_STATUS			REL_Y


#define ALSPS_VALUE_MAX (32767)
#define ALSPS_VALUE_MIN (-32768)
#define ALSPS_STATUS_MIN (0)
#define ALSPS_STATUS_MAX (64)
#define ALSPS_DIV_MAX (32767)
#define ALSPS_DIV_MIN (1)


#define MAX_CHOOSE_ALSPS_NUM 5

struct als_control_path {
	int (*open_report_data)(int open);/* open data rerport to HAL */
	int (*enable_nodata)(int en);/* only enable not report event to HAL */
	int (*set_delay)(u64 delay);
	int (*access_data_fifo)(void);/* version2.used for flush operate */
	bool is_report_input_direct;
	bool is_support_batch;/* version2.used for batch mode support flag */
	bool is_polling_mode;
	bool is_use_common_factory;
};

struct ps_control_path {
	int (*open_report_data)(int open);/* open data rerport to HAL */
	int (*enable_nodata)(int en);/* only enable not report event to HAL */
	int (*set_delay)(u64 delay);
	int (*access_data_fifo)(void);/* version2.used for flush operate */
	int (*ps_calibration)(int type, int value);
	int (*ps_threshold_setting)(int type, int value[2]);
	bool is_report_input_direct;
	bool is_support_batch;/* version2.used for batch mode support flag */
	bool is_polling_mode;
	bool is_use_common_factory;
};

struct als_data_path {
	int (*get_data)(int *als_value, int *status);
	int (*als_get_raw_data)(int *als_value);
	int vender_div;
};

struct ps_data_path {
	int (*get_data)(int *ps_value, int *status);
	int (*ps_get_raw_data)(int *ps_value);
	int vender_div;
};

struct alsps_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct alsps_data {
	struct hwm_sensor_data als_data;
	struct hwm_sensor_data ps_data;
	int data_updata;
};

struct alsps_drv_obj {
	void *self;
	int polling;
	int (*alsps_operate)(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout);
};

struct alsps_context {
	struct input_dev		*idev;
	struct miscdevice	mdev;
	struct work_struct	report_ps;
	struct work_struct	report_als;
	struct mutex			alsps_op_mutex;
	struct timer_list		timer_als;  /*als polling timer */
	struct timer_list		timer_ps;  /* ps polling timer */

	atomic_t			trace;
	atomic_t			delay_als; /*als polling period for reporting input event*/
	atomic_t				delay_ps;/*ps polling period for reporting input event*/
	atomic_t			wake;  /*user-space request to wake-up, used with stop*/

	atomic_t			early_suspend;

	struct alsps_data	drv_data;
	struct als_control_path	als_ctl;
	struct als_data_path	als_data;
	struct ps_control_path ps_ctl;
	struct ps_data_path	ps_data;

	bool is_als_active_nodata;/* Active, but HAL don't need data sensor. such as orientation need */
	bool is_als_active_data;/* Active and HAL need data . */
	bool is_ps_active_nodata;/* Active, but HAL don't need data sensor. such as orientation need */
	bool is_ps_active_data;/* Active and HAL need data . */

	bool is_als_first_data_after_enable;
	bool is_ps_first_data_after_enable;
	bool is_als_polling_run;
	bool is_ps_polling_run;
	bool is_als_batch_enable;/* version2.this is used for judging whether sensor is in batch mode */
	bool is_ps_batch_enable;	/* version2.this is used for judging whether sensor is in batch mode */
	bool is_get_valid_ps_data_after_enable;
	bool is_get_valid_als_data_after_enable;
};

/* AAL Functions */
extern int alsps_aal_enable(int enable);
extern int alsps_aal_get_status(void);
extern int alsps_aal_get_data(void);

/* for auto detect */
extern int alsps_driver_add(struct alsps_init_info *obj);
extern int ps_report_interrupt_data(int value);
extern int als_data_report(struct input_dev *dev, int value, int status);
extern int als_register_control_path(struct als_control_path *ctl);
extern int als_register_data_path(struct als_data_path *data);
extern int ps_data_report(struct input_dev *dev, int value, int status);
extern int ps_register_control_path(struct ps_control_path *ctl);
extern int ps_register_data_path(struct ps_data_path *data);
extern struct platform_device *get_alsps_platformdev(void);
#endif

/*
* Copyright (C) 2013 MediaTek Inc.
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

#ifndef __BATCH_H__
#define __BATCH_H__

#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "hwmsensor.h"
#include <linux/string.h>
#include "hwmsen_dev.h"

#define BATCH_TAG					"<BATCHDEV> "
#define BATCH_FUN(f)				pr_debug(BATCH_TAG"%s\n", __func__)
#define BATCH_ERR(fmt, args...)	pr_err(BATCH_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define BATCH_LOG(fmt, args...)	pr_debug(BATCH_TAG fmt, ##args)
#define BATCH_VER(fmt, args...)   pr_debug(BATCH_TAG"%s: "fmt, __func__, ##args)

#define OP_BATCH_DELAY		0X01
#define OP_BATCH_ENABLE		0X02
#define OP_BATCH_GET_DATA	0X04

#define BATCH_INVALID_VALUE -1

#define EVENT_TYPE_BATCH_X				ABS_X
#define EVENT_TYPE_BATCH_Y				ABS_Y
#define EVENT_TYPE_BATCH_Z				ABS_Z
#define EVENT_TYPE_BATCH_STATUS			ABS_WHEEL
#define EVENT_TYPE_SENSORTYPE							REL_RZ
#define EVENT_TYPE_BATCH_VALUE		ABS_RX
#define EVENT_TYPE_END_FLAG				REL_RY
#define EVENT_TYPE_TIMESTAMP_HI			REL_HWHEEL
#define EVENT_TYPE_TIMESTAMP_LO			REL_DIAL
#define EVENT_TYPE_BATCH_READY                    REL_X

#define BATCH_VALUE_MAX (32767)
#define BATCH_VALUE_MIN (-32768)
#define BATCH_STATUS_MIN (0)
#define BATCH_STATUS_MAX (64)
#define BATCH_TYPE_MIN (0)
#define BATCH_TYPE_MAX (64)
#define BATCH_DIV_MAX (32767)
#define BATCH_DIV_MIN (1)

#define SENSOR_DEACTIVE_BATCH_FLUSH			0
#define SENSOR_ACTIVE_BATCH_FLUSH			1
#define SENSOR_TIMEOUT_BATCH_FLUSH			2
#define SENSOR_DELAYCHANGE_BATCH_FLUSH		3
#define SENSOR_BATCH_TO_NORMAL_FLUSH		4
#define SENSOR_FLUSH_FIFO					5
#define SENSOR_TIMEOUTCHANGE_BATCH_FLUSH	6
#define SENSOR_MAX_BATCH_FLUSH_CMD			7

enum {
	SENSORS_BATCH_DRY_RUN               = 0x00000001,
	SENSORS_BATCH_WAKE_UPON_FIFO_FULL   = 0x00000002
};
#define MAX_CHOOSE_BATCH_NUM 5
struct batch_init_info {
	char *name;
	int (*init)(void);
	int (*uninit)(void);
	struct platform_driver *platform_diver_addr;
};

struct batch_control_path {
	int (*enable_hw_batch)(int handle, int enable, int flag, long long samplingPeriodNs,
		long long maxBatchReportLatencyNs);
	int (*flush)(int handle);/* open data rerport to HAL */
};

struct batch_timestamp_info {
	int64_t start_t;
	int64_t end_t;
	uint32_t total_count;
	uint32_t num;
};

struct batch_data_path {
	/* sensor data is got one by one, return value: 1 stands for data read not finish 0 stands for read data done */
	int (*get_data)(int handle, struct hwm_sensor_data *data);
	int (*get_fifo_status)(int *len, int *status, char *reserved,
		struct batch_timestamp_info *p_batch_timestampe_info);
	int (*batch_timeout)(int handle, int cmd);
	int samplingPeriodMs;
	int maxBatchReportLatencyMs;/* report latency for every sensor */
	int flags;/* reserved */
	int is_batch_supported;/* batch mode supporting status */
	int div;
	int is_timestamp_supported;
};

struct batch_dev_list {
	/* ctl_dev[max] is used for sensor HUB driver to control sensor HUB , ctl_dev[1]...
	*are for single sensor batch mode control */
	struct batch_control_path	ctl_dev[ID_SENSOR_MAX_HANDLE+1];
	/* data_dev[max] is used for sensor HUB driver to access single fifo sensor data,
	*data_dev[1]... are for single sensor fifo sensor data */
	struct batch_data_path		data_dev[ID_SENSOR_MAX_HANDLE+1];
};


struct batch_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct work_struct  report;
	struct mutex		batch_op_mutex;

	atomic_t            delay; /*polling period for reporting input event*/
	atomic_t            wake;  /*user-space request to wake-up, used with stop*/
	struct timer_list   timer;  /* polling timer */
	atomic_t            trace;

	struct wakeup_source read_data_wake_lock;
	struct batch_dev_list	dev_list;

	uint64_t		active_sensor;
	uint64_t		batch_sensor;
	int				batch_result;
	int				flush_result;
	bool			is_first_data_after_enable;
	bool			is_polling_run;
	int			div_flag;
	int				numOfDataLeft;
	int                 force_wake_upon_fifo_full;
	atomic_t		min_timeout_handle;
	struct batch_timestamp_info timestamp_info[ID_SENSOR_MAX_HANDLE+1];
};

enum BATCH_NOTIFY_TYPE {
	TYPE_NON   = 0,
	TYPE_MOTION  = 1,
	TYPE_GESTURE = 2,
	TYPE_BATCHTIMEOUT   = 3,
	TYPE_BATCHFULL   = 4,
	TYPE_ERROR = 5,
	TYPE_DATAREADY   = 6,
	TYPE_DIRECT_PUSH   = 7
};

/* driver API for third party vendor */
extern int  batch_notify(enum BATCH_NOTIFY_TYPE type);
extern int  batch_driver_add(struct batch_init_info *obj);
extern void report_batch_data(struct input_dev *dev, struct hwm_sensor_data *data);
extern void report_batch_finish(struct input_dev *dev, int handle);
extern int batch_register_control_path(int handle, struct batch_control_path *ctl);
extern int batch_register_data_path(int handle, struct batch_data_path *data);
extern int batch_register_support_info(int handle, int support, int div, int timestamp_supported);
#endif

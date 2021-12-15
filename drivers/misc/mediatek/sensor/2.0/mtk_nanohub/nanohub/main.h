// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */


#ifndef _NANOHUB_MAIN_H
#define _NANOHUB_MAIN_H

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/pm_wakeup.h>

#include "comms.h"

#define NANOHUB_NAME "nanohub"

struct nanohub_device {
	void *drv_data;
};

struct nanohub_data;

struct nanohub_io {
	struct device *dev;
	struct nanohub_data *data;
	wait_queue_head_t buf_wait;
	struct list_head buf_list;
};

struct nanohub_data {
	/* indices for io[] array */
	#define ID_NANOHUB_SENSOR 0
	#define ID_NANOHUB_COMMS 1
	#define ID_NANOHUB_MAX 2

	struct nanohub_device *nanohub_dev;
	struct nanohub_io io[ID_NANOHUB_MAX];

	struct nanohub_comms comms;
	const struct nanohub_platform_data *pdata;
	int irq1;
	int irq2;

	atomic_t kthread_run;
	atomic_t thread_state;
	wait_queue_head_t kthread_wait;

	struct wakeup_source ws;

	struct nanohub_io free_pool;

	atomic_t lock_mode;
	/* these 3 vars should be accessed only with wakeup_wait.lock held */
	atomic_t wakeup_cnt;
	atomic_t wakeup_lock_cnt;
	atomic_t wakeup_acquired;
	wait_queue_head_t wakeup_wait;

	u32 interrupts[8];

	int err_cnt;
	void *vbuf;
	struct task_struct *thread;

	struct mutex comms_lock;
};

int nanohub_wait_for_interrupt(struct nanohub_data *data);
int nanohub_wakeup_eom(struct nanohub_data *data, bool repeat);
struct nanohub_device *
nanohub_probe(struct device *dev, struct nanohub_device *nano_dev);
int nanohub_suspend(struct nanohub_device *nano_dev);
int nanohub_resume(struct nanohub_device *nano_dev);

static inline int request_wakeup_timeout(struct nanohub_data *data, int timeout)
{
	return 0;
}

static inline int request_wakeup(struct nanohub_data *data)
{
	return 0;
}

static inline void release_wakeup(struct nanohub_data *data)
{
}

#endif

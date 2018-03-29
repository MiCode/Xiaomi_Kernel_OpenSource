/*
 * Copyright (C) 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _NANOHUB_MAIN_H
#define _NANOHUB_MAIN_H

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/wakelock.h>

#include "comms.h"
#include "bl.h"

#define NANOHUB_NAME "nanohub"

struct nanohub_buf {
	struct list_head list;
	uint8_t buffer[255];
	uint8_t length;
};

struct nanohub_data;

struct nanohub_io {
	struct device *dev;
	struct nanohub_data *data;
	wait_queue_head_t buf_wait;
	struct list_head buf_list;
};

static inline struct nanohub_data *dev_get_nanohub_data(struct device *dev)
{
	struct nanohub_io *io = dev_get_drvdata(dev);

	return io->data;
}

struct nanohub_data {
	/* indices for io[] array */
	#define ID_NANOHUB_SENSOR 0
	#define ID_NANOHUB_COMMS 1
	#define ID_NANOHUB_MAX 2

	struct iio_dev *iio_dev;
	struct nanohub_io io[ID_NANOHUB_MAX];

	struct nanohub_comms comms;
	struct nanohub_bl bl;
	const struct nanohub_platform_data *pdata;
	int irq1;
	int irq2;

	atomic_t kthread_run;
	atomic_t thread_state;
	wait_queue_head_t kthread_wait;

	struct wake_lock wakelock_read;

	struct nanohub_io free_pool;

	atomic_t lock_mode;
	/* these 3 vars should be accessed only with wakeup_wait.lock held */
	atomic_t wakeup_cnt;
	atomic_t wakeup_lock_cnt;
	atomic_t wakeup_acquired;
	wait_queue_head_t wakeup_wait;

	uint32_t interrupts[8];

	int err_cnt;
	void *vbuf;
	struct task_struct *thread;
};

enum {
	KEY_WAKEUP_NONE,
	KEY_WAKEUP,
	KEY_WAKEUP_LOCK,
};

enum {
	LOCK_MODE_NONE,
	LOCK_MODE_NORMAL,
	LOCK_MODE_IO,
	LOCK_MODE_RESET,
	LOCK_MODE_SUSPEND_RESUME,
};

int request_wakeup_ex(struct nanohub_data *data, long timeout,
		      int key, int lock_mode);
void release_wakeup_ex(struct nanohub_data *data, int key, int lock_mode);
int nanohub_wait_for_interrupt(struct nanohub_data *data);
int nanohub_wakeup_eom(struct nanohub_data *data, bool repeat);
struct iio_dev *nanohub_probe(struct device *dev, struct iio_dev *iio_dev);
int nanohub_reset(struct nanohub_data *data);
int nanohub_remove(struct iio_dev *iio_dev);
int nanohub_suspend(struct iio_dev *iio_dev);
int nanohub_resume(struct iio_dev *iio_dev);

static inline int nanohub_irq1_fired(struct nanohub_data *data)
{
	const struct nanohub_platform_data *pdata = data->pdata;

	return !gpio_get_value(pdata->irq1_gpio);
}

static inline int nanohub_irq2_fired(struct nanohub_data *data)
{
	const struct nanohub_platform_data *pdata = data->pdata;

	return data->irq2 && !gpio_get_value(pdata->irq2_gpio);
}

static inline int request_wakeup_timeout(struct nanohub_data *data, int timeout)
{
	return request_wakeup_ex(data, timeout, KEY_WAKEUP, LOCK_MODE_NORMAL);
}

static inline int request_wakeup(struct nanohub_data *data)
{
	return request_wakeup_ex(data, MAX_SCHEDULE_TIMEOUT, KEY_WAKEUP,
				 LOCK_MODE_NORMAL);
}

static inline void release_wakeup(struct nanohub_data *data)
{
	release_wakeup_ex(data, KEY_WAKEUP, LOCK_MODE_NORMAL);
}

#endif

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[sensor_list] " fmt

#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/mutex.h>

#include "hf_sensor_type.h"
#include "sensor_comm.h"
#include "sensor_list.h"

static DEFINE_MUTEX(sensor_list_lock);
static struct completion *sensor_list_done;
static uint32_t current_write_position;

static void sensor_list_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	struct completion *done = READ_ONCE(sensor_list_done);

	if (done) {
		current_write_position = n->value[0];
		complete(done);
	}
}

static int get_sensor_list(struct sensor_info *list, unsigned int size)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int ret = 0;
	int timeout = 0;
	uint32_t write_position = 0;
	struct sensor_comm_notify notify;

	notify.sensor_type = SENSOR_TYPE_INVALID;
	notify.command = SENS_COMM_NOTIFY_LIST_CMD;
	ret = sensor_comm_notify(&notify);
	if (ret < 0)
		return ret;

	sensor_list_done = &done;
	timeout = wait_for_completion_timeout(&done, msecs_to_jiffies(500));
	if (!timeout) {
		pr_err("%s wait completion timeout!", __func__);
		return -ETIMEDOUT;
	}

	write_position = READ_ONCE(current_write_position);
	/* share_mem_read_buffer */
	return 0;
}

int sensor_list_get_list(struct sensor_info *list, unsigned int size)
{
	int retry = 0, ret = 0;
	const int max_retry = 10;

	mutex_lock(&sensor_list_lock);
	do {
		ret = get_sensor_list(list, size);
	} while (retry++ < max_retry && ret < 0);
	mutex_unlock(&sensor_list_lock);

	return ret;
}

int sensor_list_init(void)
{
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_LIST_CMD,
		sensor_list_notify_handler, NULL);
	return 0;
}

void sensor_list_exit(void)
{
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_LIST_CMD);
}

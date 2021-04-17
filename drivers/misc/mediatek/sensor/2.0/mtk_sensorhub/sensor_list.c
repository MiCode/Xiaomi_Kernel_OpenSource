// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[sensor_list] " fmt

#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

#include "hf_sensor_type.h"
#include "sensor_comm.h"
#include "sensor_list.h"

static DEFINE_MUTEX(bus_user_lock);
static atomic_t sensor_list_sequence;
static struct completion sensor_list_done;
static DEFINE_SPINLOCK(rx_notify_lock);
struct sensor_comm_notify rx_notify;

static void sensor_list_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	spin_lock(&rx_notify_lock);
	memcpy(&rx_notify, n, sizeof(rx_notify));
	spin_unlock(&rx_notify_lock);
	complete(&sensor_list_done);
}

static int seq_get_sensor_list(struct sensor_info *list, unsigned int size)
{
	unsigned long flags = 0;
	int ret = 0;
	int timeout = 0;
	uint32_t write_position = 0;
	struct sensor_comm_notify notify;

	/*
	 * must init_completion before sensor_comm_notify
	 * wrong sequence:
	 * sensor_comm_notify -----> init_completion -> wait_for_completion
	 *                      |
	 *                   complete
	 * complete before init_completion, lose this complete
	 * right sequence:
	 * init_completion -> sensor_comm_notify -> wait_for_completion
	 */
	init_completion(&sensor_list_done);

	notify.sensor_type = SENSOR_TYPE_INVALID;
	notify.command = SENS_COMM_NOTIFY_LIST_CMD;
	notify.length = 0;
	notify.sequence = atomic_inc_return(&sensor_list_sequence);
	ret = sensor_comm_notify(&notify);
	if (ret < 0) {
		pr_err("%s notify failed %d!", __func__, ret);
		return ret;
	}

	timeout = wait_for_completion_timeout(&sensor_list_done,
		msecs_to_jiffies(500));
	if (!timeout) {
		pr_err("%s wait completion timeout!", __func__);
		return -ETIMEDOUT;
	}

	spin_lock_irqsave(&rx_notify_lock, flags);
	if (rx_notify.sensor_type != notify.sensor_type &&
	    rx_notify.command != notify.command &&
	    rx_notify.sequence != notify.sequence) {
		pr_err("%s reply failed!", __func__);
		spin_unlock_irqrestore(&rx_notify_lock, flags);
		return -EREMOTEIO;
	}
	write_position = rx_notify.value[0];
	spin_unlock_irqrestore(&rx_notify_lock, flags);
	/* share_mem_read_buffer */
	return 0;
}

int sensor_list_get_list(struct sensor_info *list, unsigned int size)
{
	int retry = 0, ret = 0;
	const int max_retry = 10;

	mutex_lock(&bus_user_lock);
	do {
		ret = seq_get_sensor_list(list, size);
	} while (retry++ < max_retry && ret < 0);
	mutex_unlock(&bus_user_lock);

	return ret;
}

int sensor_list_init(void)
{
	atomic_set(&sensor_list_sequence, 0);
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_LIST_CMD,
		sensor_list_notify_handler, NULL);
	return 0;
}

void sensor_list_exit(void)
{
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_LIST_CMD);
}

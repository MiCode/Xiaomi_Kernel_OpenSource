// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[sensor_list] " fmt

#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/slab.h>

#include "hf_sensor_type.h"
#include "sensor_comm.h"
#include "sensor_list.h"
#include "share_memory.h"

static DEFINE_MUTEX(bus_user_lock);
static atomic_t sensor_list_sequence;
static DECLARE_COMPLETION(sensor_list_wp_completion);
static DEFINE_SPINLOCK(rx_notify_lock);
struct sensor_comm_notify rx_notify;
static struct share_mem sensor_list_shm_reader;

static void sensor_list_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	spin_lock(&rx_notify_lock);
	memcpy(&rx_notify, n, sizeof(rx_notify));
	spin_unlock(&rx_notify_lock);
	complete(&sensor_list_wp_completion);
}

static int sensor_list_seq_get_list(struct sensor_info *list,
		unsigned int num)
{
	int ret = 0;
	int timeout = 0;
	unsigned long flags = 0;
	struct sensor_comm_notify notify;
	uint32_t write_position = 0;
	struct share_mem_info share_list;
	unsigned int i = 0;

	ret = share_mem_read_reset(&sensor_list_shm_reader);
	if (ret < 0) {
		pr_err("%s share_mem_read_reset fail, ret:%d\n", __func__, ret);
		return ret;
	}

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
	init_completion(&sensor_list_wp_completion);

	notify.sensor_type = SENSOR_TYPE_INVALID;
	notify.command = SENS_COMM_NOTIFY_LIST_CMD;
	notify.length = 0;
	/* safe sequence given by atomic, round from 0 to 255 */
	notify.sequence = atomic_inc_return(&sensor_list_sequence);
	ret = sensor_comm_notify(&notify);
	if (ret < 0) {
		pr_err("%s notify failed ret:%d!\n", __func__, ret);
		return ret;
	}

	timeout = wait_for_completion_timeout(&sensor_list_wp_completion,
		msecs_to_jiffies(500));

	if (!timeout) {
		pr_err("%s wait completion timeout!\n", __func__);
		return -ETIMEDOUT;
	}

	spin_lock_irqsave(&rx_notify_lock, flags);
	if (rx_notify.sensor_type != notify.sensor_type &&
	    rx_notify.command != notify.command &&
	    rx_notify.sequence != notify.sequence) {
		pr_err("%s reply failed!\n", __func__);
		spin_unlock_irqrestore(&rx_notify_lock, flags);
		return -EREMOTEIO;
	}
	write_position = rx_notify.value[0];
	spin_unlock_irqrestore(&rx_notify_lock, flags);

	/* share_mem_read_buffer */
	ret = share_mem_seek(&sensor_list_shm_reader, write_position);
	if (ret < 0) {
		pr_err("%s share_mem_seek fail, ret:%d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < num; i++) {
		ret = share_mem_read(&sensor_list_shm_reader,
			&share_list, sizeof(share_list));
		if (ret == 0)
			break;

		if (ret != sizeof(share_list)) {
			pr_err("%s share_mem_read fail, ret:%d\n",
				__func__, ret);
			break;
		}

		list[i].sensor_type = share_list.sensor_type;
		list[i].gain = share_list.gain;
		strlcpy(list[i].name, share_list.name, sizeof(list[i].name));
		strlcpy(list[i].vendor,
			share_list.vendor, sizeof(list[i].vendor));
	}
	return i;
}

int sensor_list_get_list(struct sensor_info *list, unsigned int num)
{
	int retry = 0, ret = 0;
	const int max_retry = 10;

	mutex_lock(&bus_user_lock);
	do {
		ret = sensor_list_seq_get_list(list, num);
	} while (retry++ < max_retry && ret < 0);
	mutex_unlock(&bus_user_lock);

	return ret;
}

static int sensor_list_share_mem_cfg(struct share_mem_cfg *cfg,
		void *private_data)
{
	sensor_list_shm_reader.name = "sensor_list_r";
	sensor_list_shm_reader.item_size = sizeof(struct share_mem_info);
	sensor_list_shm_reader.buffer_full_detect = false;

	return share_mem_init(&sensor_list_shm_reader, cfg);
}

int sensor_list_init(void)
{
	atomic_set(&sensor_list_sequence, 0);

	spin_lock(&rx_notify_lock);
	memset(&rx_notify, 0x00, sizeof(rx_notify));
	spin_unlock(&rx_notify_lock);

	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_LIST_CMD,
		sensor_list_notify_handler, NULL);
	share_mem_config_handler_register(SENS_COMM_NOTIFY_LIST_CMD,
		sensor_list_share_mem_cfg, NULL);
	return 0;
}

void sensor_list_exit(void)
{
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_LIST_CMD);
	share_mem_config_handler_unregister(SENS_COMM_NOTIFY_LIST_CMD);
}

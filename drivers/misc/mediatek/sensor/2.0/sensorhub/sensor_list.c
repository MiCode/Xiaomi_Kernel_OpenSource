/*
 * Copyright (C) 2020 MediaTek Inc.
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

#define pr_fmt(fmt) "sensor_list " fmt

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
static DECLARE_COMPLETION(sensor_list_done);
static DEFINE_SPINLOCK(rx_notify_lock);
static struct sensor_comm_notify rx_notify;
static struct share_mem sensor_list_shm_reader;

static void sensor_list_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	spin_lock(&rx_notify_lock);
	memcpy(&rx_notify, n, sizeof(rx_notify));
	spin_unlock(&rx_notify_lock);
	complete(&sensor_list_done);
}

static int sensor_list_seq_get_list(struct sensor_info *list,
		unsigned int num)
{
	int ret = 0;
	int timeout = 0;
	unsigned long flags = 0;
	struct sensor_comm_notify notify;
	uint32_t write_position = 0;
	struct share_mem_info info;
	unsigned int i = 0;

	ret = share_mem_read_reset(&sensor_list_shm_reader);
	if (ret < 0) {
		pr_err("reset fail %d\n", ret);
		return ret;
	}

	/*
	 * NOTE: must reinit_completion before sensor_comm_notify
	 * wrong sequence:
	 * sensor_comm_notify ---> reinit_completion -> wait_for_completion
	 *                     |
	 *                  complete
	 * if complete before reinit_completion, will lose this complete
	 * right sequence:
	 * reinit_completion -> sensor_comm_notify -> wait_for_completion
	 */
	reinit_completion(&sensor_list_done);

	/* safe sequence given by atomic, round from 0 to 255 */
	notify.sequence = atomic_inc_return(&sensor_list_sequence);
	notify.sensor_type = SENSOR_TYPE_INVALID;
	notify.command = SENS_COMM_NOTIFY_LIST_CMD;
	notify.length = 0;
	ret = sensor_comm_notify(&notify);
	if (ret < 0) {
		pr_err("notify fail %d\n", ret);
		return ret;
	}

	timeout = wait_for_completion_timeout(&sensor_list_done,
		msecs_to_jiffies(100));
	if (!timeout) {
		pr_err("wait completion timeout\n");
		return -ETIMEDOUT;
	}

	spin_lock_irqsave(&rx_notify_lock, flags);
	if (rx_notify.sequence != notify.sequence &&
	    rx_notify.sensor_type != notify.sensor_type &&
	    rx_notify.command != notify.command) {
		pr_err("reply fail\n");
		spin_unlock_irqrestore(&rx_notify_lock, flags);
		return -EREMOTEIO;
	}
	write_position = rx_notify.value[0];
	spin_unlock_irqrestore(&rx_notify_lock, flags);

	ret = share_mem_seek(&sensor_list_shm_reader, write_position);
	if (ret < 0) {
		pr_err("seek fail %d\n", ret);
		return ret;
	}
	/* NOTE: i++ must not put into for loop */
	for (i = 0; i < num; ) {
		ret = share_mem_read(&sensor_list_shm_reader,
			&info, sizeof(info));
		if (ret == 0)
			break;
		if (ret != sizeof(info)) {
			pr_err("read fail %d\n", ret);
			break;
		}
		if (info.sensor_type >= SENSOR_TYPE_SENSOR_MAX ||
				!info.sensor_type || !info.gain) {
			pr_err("read wrong sensor info\n");
			continue;
		}
		list[i].sensor_type = info.sensor_type;
		list[i].gain = info.gain;
		strlcpy(list[i].name, info.name, sizeof(list[i].name));
		strlcpy(list[i].vendor, info.vendor, sizeof(list[i].vendor));
		i++;
	}
	return i;
}

int sensor_list_get_list(struct sensor_info *list, unsigned int num)
{
	int retry = 0, ret = 0;
	const int max_retry = 3;

	mutex_lock(&bus_user_lock);
	do {
		ret = sensor_list_seq_get_list(list, num);
	} while (retry++ < max_retry && ret < 0);
	mutex_unlock(&bus_user_lock);

	return ret;
}

static int sensor_list_share_mem_cfg(struct share_mem_config *cfg,
		void *private_data)
{
	sensor_list_shm_reader.name = "sensor_list_r";
	sensor_list_shm_reader.item_size = sizeof(struct share_mem_info);
	sensor_list_shm_reader.buffer_full_detect = false;

	return share_mem_init(&sensor_list_shm_reader, cfg);
}

int sensor_list_init(void)
{
	unsigned long flags = 0;

	atomic_set(&sensor_list_sequence, 0);

	spin_lock_irqsave(&rx_notify_lock, flags);
	memset(&rx_notify, 0, sizeof(rx_notify));
	spin_unlock_irqrestore(&rx_notify_lock, flags);

	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_LIST_CMD,
		sensor_list_notify_handler, NULL);
	share_mem_config_handler_register(SHARE_MEM_LIST_PAYLOAD_TYPE,
		sensor_list_share_mem_cfg, NULL);
	return 0;
}

void sensor_list_exit(void)
{
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_LIST_CMD);
	share_mem_config_handler_unregister(SHARE_MEM_LIST_PAYLOAD_TYPE);
}

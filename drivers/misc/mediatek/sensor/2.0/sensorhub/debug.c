// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "debug " fmt

#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/slab.h>

#include "hf_sensor_type.h"
#include "sensor_comm.h"
#include "debug.h"
#include "share_memory.h"

static DEFINE_MUTEX(bus_user_lock);
static atomic_t debug_sequence;
static DECLARE_COMPLETION(debug_done);
static DEFINE_SPINLOCK(rx_notify_lock);
static struct sensor_comm_notify rx_notify;
static struct share_mem debug_shm_reader;

static void debug_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	spin_lock(&rx_notify_lock);
	memcpy(&rx_notify, n, sizeof(rx_notify));
	spin_unlock(&rx_notify_lock);
	complete(&debug_done);
}

static int debug_seq_get_debug(uint8_t sensor_type, uint8_t *buffer,
		unsigned int len)
{
	int ret = 0;
	int timeout = 0;
	unsigned long flags = 0;
	struct sensor_comm_ctrl *ctrl = NULL;
	struct share_mem_debug *shm_debug = NULL;
	uint32_t write_position = 0;
	uint32_t ctrl_size = 0;

	ret = share_mem_read_reset(&debug_shm_reader);
	if (ret < 0) {
		pr_err("%u reset fail %d\n", sensor_type, ret);
		return ret;
	}

	/*
	 * NOTE: must reinit_completion before sensor_comm_notify
	 * wrong sequence:
	 * sensor_comm_ctrl_send ---> reinit_completion -> wait_for_completion
	 *                        |
	 *                    complete
	 * if complete before reinit_completion, will lose this complete
	 * right sequence:
	 * reinit_completion -> sensor_comm_ctrl_send -> wait_for_completion
	 */
	reinit_completion(&debug_done);

	ctrl_size = ipi_comm_size(sizeof(*ctrl) + sizeof(ctrl->data[0]));
	ctrl = kzalloc(ctrl_size, GFP_KERNEL);
	ctrl->sensor_type = sensor_type;
	ctrl->command = SENS_COMM_CTRL_DEBUG_CMD;
	ctrl->length = sizeof(ctrl->data[0]);
	/* safe sequence given by atomic, round from 0 to 255 */
	ctrl->data[0] = (uint8_t)atomic_inc_return(&debug_sequence);

	ret = sensor_comm_ctrl_send(ctrl, ctrl_size);
	if (ret < 0) {
		pr_err("%u send fail %d\n", sensor_type, ret);
		goto out1;
	}

	timeout = wait_for_completion_timeout(&debug_done,
		msecs_to_jiffies(100));
	if (!timeout) {
		pr_err("%u wait completion timeout\n", sensor_type);
		ret = -ETIMEDOUT;
		goto out1;
	}

	spin_lock_irqsave(&rx_notify_lock, flags);
	if (rx_notify.sequence != ctrl->data[0] &&
	    rx_notify.sensor_type != ctrl->sensor_type &&
	    rx_notify.command != ctrl->command) {
		pr_err("%u reply fail\n", sensor_type);
		spin_unlock_irqrestore(&rx_notify_lock, flags);
		ret = -EREMOTEIO;
		goto out1;
	}
	write_position = rx_notify.value[0];
	spin_unlock_irqrestore(&rx_notify_lock, flags);

	ret = share_mem_seek(&debug_shm_reader, write_position);
	if (ret < 0) {
		pr_err("%u seek fail %d\n", sensor_type, ret);
		goto out1;
	}
	shm_debug = kzalloc(sizeof(*shm_debug), GFP_KERNEL);
	ret = share_mem_read(&debug_shm_reader, shm_debug, sizeof(*shm_debug));
	if (ret < 0) {
		pr_err("%u read fail %d\n", sensor_type, ret);
		goto out2;
	} else if (ret != sizeof(*shm_debug)) {
		pr_err("%u size fail %d\n", sensor_type, ret);
		ret = -EPROTO;
		goto out2;
	}
	len = len < shm_debug->written ? len : shm_debug->written;
	strlcpy(buffer, shm_debug->buffer, len);
	ret = len;
out2:
	kfree(shm_debug);
out1:
	kfree(ctrl);
	return ret;
}

/* SENSOR_TYPE_INVALID for get sensor_manager debug information */
int debug_get_debug(uint8_t sensor_type, uint8_t *buffer, unsigned int len)
{
	int retry = 0, ret = 0;
	const int max_retry = 3;

	mutex_lock(&bus_user_lock);
	do {
		ret = debug_seq_get_debug(sensor_type, buffer, len);
	} while (retry++ < max_retry && ret < 0);
	mutex_unlock(&bus_user_lock);

	return ret;
}

static int debug_share_mem_cfg(struct share_mem_config *cfg,
		void *private_data)
{
	debug_shm_reader.name = "debug_r";
	debug_shm_reader.item_size = sizeof(struct share_mem_debug);
	debug_shm_reader.buffer_full_detect = false;

	return share_mem_init(&debug_shm_reader, cfg);
}

int debug_init(void)
{
	unsigned long flags = 0;

	atomic_set(&debug_sequence, 0);

	spin_lock_irqsave(&rx_notify_lock, flags);
	memset(&rx_notify, 0, sizeof(rx_notify));
	spin_unlock_irqrestore(&rx_notify_lock, flags);

	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_DEBUG_CMD,
		debug_notify_handler, NULL);
	share_mem_config_handler_register(SHARE_MEM_DEBUG_PAYLOAD_TYPE,
		debug_share_mem_cfg, NULL);
	return 0;
}

void debug_exit(void)
{
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_DEBUG_CMD);
	share_mem_config_handler_unregister(SHARE_MEM_DEBUG_PAYLOAD_TYPE);
}

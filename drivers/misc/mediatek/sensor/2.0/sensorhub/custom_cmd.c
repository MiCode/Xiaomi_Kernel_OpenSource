// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt) "cust_cmd " fmt

#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

#include "sensor_comm.h"
#include "custom_cmd.h"
#include "share_memory.h"

static DEFINE_MUTEX(bus_user_lock);
static atomic_t cust_cmd_sequence;
static DECLARE_COMPLETION(cust_cmd_done);
static DEFINE_SPINLOCK(rx_notify_lock);
static struct sensor_comm_notify rx_notify;
static DECLARE_COMPLETION(cust_cmd_fast_done);
static DEFINE_SPINLOCK(rx_fast_notify_lock);
static struct sensor_comm_notify rx_fast_notify;
static struct share_mem cust_cmd_shm_rx;
static struct share_mem cust_cmd_shm_tx;


static void custom_cmd_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	spin_lock(&rx_notify_lock);
	memcpy(&rx_notify, n, sizeof(rx_notify));
	spin_unlock(&rx_notify_lock);
	complete(&cust_cmd_done);
}

static void custom_cmd_fast_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	spin_lock(&rx_fast_notify_lock);
	memcpy(&rx_fast_notify, n, sizeof(rx_fast_notify));
	spin_unlock(&rx_fast_notify_lock);
	complete(&cust_cmd_fast_done);
}

static int custom_cmd_slow_seq(int sensor_type, struct share_mem_cmd *shm_cmd)
{
	int ret = 0;
	int timeout = 0;
	unsigned long flags = 0;
	uint32_t write_position = 0;
	struct share_mem_notify notify;

	/*
	 * NOTE: must reinit_completion before sensor_comm_notify
	 * wrong sequence:
	 * sensor_comm_notify ---> reinit_completion -> wait_for_completion
	 *		       |
	 *		    complete
	 * if complete before reinit_completion, will lose this complete
	 * right sequence:
	 * reinit_completion -> sensor_comm_notify -> wait_for_completion
	 */
	reinit_completion(&cust_cmd_done);

	share_mem_write_reset(&cust_cmd_shm_tx);
	ret = share_mem_write(&cust_cmd_shm_tx, shm_cmd, sizeof(*shm_cmd));
	if (ret < 0)
		return ret;
	if (!ret)
		return -ENOMEM;
	/* safe sequence given by atomic, round from 0 to 255 */
	notify.sequence = atomic_inc_return(&cust_cmd_sequence);
	notify.sensor_type = sensor_type;
	notify.notify_cmd = SENS_COMM_NOTIFY_CUSTOM_CMD;
	ret = share_mem_flush(&cust_cmd_shm_tx, &notify);
	if (ret < 0)
		return ret;

	timeout = wait_for_completion_timeout(&cust_cmd_done,
		msecs_to_jiffies(100));
	if (!timeout)
		return -ETIMEDOUT;

	spin_lock_irqsave(&rx_notify_lock, flags);
	if (rx_notify.sequence != notify.sequence &&
	    rx_notify.sensor_type != notify.sensor_type &&
	    rx_notify.command != notify.notify_cmd) {
		spin_unlock_irqrestore(&rx_notify_lock, flags);
		return -EREMOTEIO;
	}
	write_position = rx_notify.value[0];
	spin_unlock_irqrestore(&rx_notify_lock, flags);

	ret = share_mem_seek(&cust_cmd_shm_rx, write_position);
	if (ret < 0)
		return ret;
	ret = share_mem_read(&cust_cmd_shm_rx, shm_cmd, sizeof(*shm_cmd));
	if (ret < 0)
		return ret;
	if (!ret)
		return -ENOMEM;
	share_mem_read_reset(&cust_cmd_shm_rx);
	return 0;
}

static int custom_cmd_slow_comm(int sensor_type, struct custom_cmd *cust_cmd)
{
	int retry = 0, ret = 0;
	const int max_retry = 3;
	struct share_mem_cmd shm_cmd;

	if (sizeof(shm_cmd) != sizeof(*cust_cmd) ||
		sizeof(shm_cmd.data) != sizeof(cust_cmd->data) ||
		offsetof(typeof(shm_cmd), data) !=
		offsetof(typeof(*cust_cmd), data)) {
		pr_err("%u %u struct not match\n",
			sensor_type, cust_cmd->command);
		return -EINVAL;
	}

	shm_cmd.command = cust_cmd->command;
	shm_cmd.tx_len = cust_cmd->tx_len;
	shm_cmd.rx_len = cust_cmd->rx_len;
	memcpy(shm_cmd.data, cust_cmd->data, shm_cmd.tx_len);

	mutex_lock(&bus_user_lock);
	do {
		ret = custom_cmd_slow_seq(sensor_type, &shm_cmd);
	} while (retry++ < max_retry && ret < 0);
	mutex_unlock(&bus_user_lock);

	if (ret < 0) {
		pr_err("%u %u comm fail %d\n",
			sensor_type, cust_cmd->command, ret);
		return ret;
	}
	if (shm_cmd.command != cust_cmd->command ||
		shm_cmd.rx_len > sizeof(shm_cmd.data)) {
		pr_err("%u %u param error\n",
			sensor_type, cust_cmd->command);
		return -EINVAL;
	}

	cust_cmd->rx_len = shm_cmd.rx_len;
	memcpy(cust_cmd->data, shm_cmd.data, cust_cmd->rx_len);

	return 0;
}

static int custom_cmd_fast_seq(int sensor_type, struct custom_cmd *cust_cmd)
{
	int ret = 0;
	int timeout = 0;
	unsigned long flags = 0;
	struct sensor_comm_notify notify;
	struct custom_cmd *rx_cmd = NULL;

	/*
	 * NOTE: must reinit_completion before sensor_comm_notify
	 * wrong sequence:
	 * sensor_comm_notify ---> reinit_completion -> wait_for_completion
	 *		       |
	 *		    complete
	 * if complete before reinit_completion, will lose this complete
	 * right sequence:
	 * reinit_completion -> sensor_comm_notify -> wait_for_completion
	 */
	reinit_completion(&cust_cmd_fast_done);

	/* safe sequence given by atomic, round from 0 to 255 */
	notify.sequence = atomic_inc_return(&cust_cmd_sequence);
	notify.sensor_type = sensor_type;
	notify.command = SENS_COMM_NOTIFY_FAST_CUST_CMD;
	notify.length = offsetof(typeof(*cust_cmd), data) + cust_cmd->tx_len;
	memcpy(notify.value, cust_cmd, notify.length);
	ret = sensor_comm_notify(&notify);
	if (ret < 0)
		return ret;

	timeout = wait_for_completion_timeout(&cust_cmd_fast_done,
		msecs_to_jiffies(100));
	if (!timeout)
		return -ETIMEDOUT;

	spin_lock_irqsave(&rx_fast_notify_lock, flags);
	if (rx_fast_notify.sequence != notify.sequence &&
	    rx_fast_notify.sensor_type != notify.sensor_type &&
	    rx_fast_notify.command != notify.command) {
		spin_unlock_irqrestore(&rx_fast_notify_lock, flags);
		return -EREMOTEIO;
	}
	rx_cmd = (struct custom_cmd *)rx_fast_notify.value;
	if (rx_cmd->command != cust_cmd->command ||
		rx_cmd->rx_len > sizeof(cust_cmd->data) ||
		rx_cmd->rx_len > cust_cmd->rx_len) {
		spin_unlock_irqrestore(&rx_fast_notify_lock, flags);
		return -EIO;
	}
	cust_cmd->rx_len = rx_cmd->rx_len;
	memcpy(cust_cmd->data, rx_cmd->data, cust_cmd->rx_len);
	spin_unlock_irqrestore(&rx_fast_notify_lock, flags);

	return 0;
}

static int custom_cmd_fast_comm(int sensor_type, struct custom_cmd *cust_cmd)
{
	int retry = 0, ret = 0;
	const int max_retry = 3;

	mutex_lock(&bus_user_lock);
	do {
		ret = custom_cmd_fast_seq(sensor_type, cust_cmd);
	} while (retry++ < max_retry && ret < 0);
	mutex_unlock(&bus_user_lock);

	if (ret < 0)
		pr_err("%u %u fast comm fail %d\n",
			sensor_type, cust_cmd->command, ret);

	return ret;
}

int custom_cmd_comm_with(int sensor_type, struct custom_cmd *cust_cmd)
{
	int ret = 0;
	uint8_t header_len = offsetof(typeof(*cust_cmd), data);
	uint16_t tx_len = header_len + cust_cmd->tx_len;
	uint16_t rx_len = header_len + cust_cmd->rx_len;

	if (tx_len > sizeof(rx_fast_notify.value) ||
		rx_len > sizeof(rx_fast_notify.value))
		ret = custom_cmd_slow_comm(sensor_type, cust_cmd);
	else
		ret = custom_cmd_fast_comm(sensor_type, cust_cmd);

	return ret;
}

static int custom_cmd_w_shm_cfg(struct share_mem_config *cfg,
		void *private_data)
{
	cust_cmd_shm_tx.name = "cust_cmd_w";
	cust_cmd_shm_tx.item_size = sizeof(struct share_mem_cmd);
	cust_cmd_shm_tx.buffer_full_detect = false;

	return share_mem_init(&cust_cmd_shm_tx, cfg);
}

static int custom_cmd_r_shm_cfg(struct share_mem_config *cfg,
		void *private_data)
{
	cust_cmd_shm_rx.name = "cust_cmd_r";
	cust_cmd_shm_rx.item_size = sizeof(struct share_mem_cmd);
	cust_cmd_shm_rx.buffer_full_detect = false;

	return share_mem_init(&cust_cmd_shm_rx, cfg);
}

int custom_cmd_init(void)
{
	unsigned long flags = 0;

	atomic_set(&cust_cmd_sequence, 0);

	spin_lock_irqsave(&rx_notify_lock, flags);
	memset(&rx_notify, 0, sizeof(rx_notify));
	spin_unlock_irqrestore(&rx_notify_lock, flags);

	spin_lock_irqsave(&rx_fast_notify_lock, flags);
	memset(&rx_fast_notify, 0, sizeof(rx_fast_notify));
	spin_unlock_irqrestore(&rx_fast_notify_lock, flags);

	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_CUSTOM_CMD,
		custom_cmd_notify_handler, NULL);
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_FAST_CUST_CMD,
		custom_cmd_fast_notify_handler, NULL);
	share_mem_config_handler_register(SHARE_MEM_CUSTOM_W_PAYLOAD_TYPE,
		custom_cmd_w_shm_cfg, NULL);
	share_mem_config_handler_register(SHARE_MEM_CUSTOM_R_PAYLOAD_TYPE,
		custom_cmd_r_shm_cfg, NULL);
	return 0;
}

void custom_cmd_exit(void)
{
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_CUSTOM_CMD);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_FAST_CUST_CMD);
	share_mem_config_handler_unregister(SHARE_MEM_CUSTOM_W_PAYLOAD_TYPE);
	share_mem_config_handler_unregister(SHARE_MEM_CUSTOM_R_PAYLOAD_TYPE);
}

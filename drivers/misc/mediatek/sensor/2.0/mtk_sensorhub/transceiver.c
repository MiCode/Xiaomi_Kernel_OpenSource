// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[transceiver] " fmt

#include <linux/err.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>

#include "ready.h"
#include "sensor_comm.h"
#include "sensor_list.h"
#include "share_memory.h"

#define TRANSCEIVER_SHARE_MEM_DATA_BUFFER_NUM 8
#define TRANSCEIVER_READ_WP_NUM 32

DECLARE_COMPLETION(transceiver_read_wp_completion);
DEFINE_KFIFO(transceiver_read_wp_fifo, uint32_t, TRANSCEIVER_READ_WP_NUM);
DEFINE_SPINLOCK(transceiver_read_wp_lock);

static struct share_mem transceiver_shm_data_reader;
static struct share_mem_data
share_mem_read_buffer[TRANSCEIVER_SHARE_MEM_DATA_BUFFER_NUM];

static void transceiver_notify_func(struct sensor_comm_notify *n,
		void *private_data)
{
	int ret = 0;
	uint32_t wp = 0;

	if (n->command != SENS_COMM_NOTIFY_DATA_CMD &&
		n->command != SENS_COMM_NOTIFY_FULL_CMD) {
		pr_err("%s fail, type:%d command:%d value:%d\n",
			__func__, n->sensor_type, n->command, n->value[0]);
		return;
	}

	spin_lock(&transceiver_read_wp_lock);

	if (kfifo_is_full(&transceiver_read_wp_fifo)) {
		ret = kfifo_out(&transceiver_read_wp_fifo, &wp, 1);
		pr_err_ratelimited("%s drop wp:%d ret:%d\n", __func__, wp, ret);
	}
	wp = n->dnotify.write_position;
	ret = kfifo_in(&transceiver_read_wp_fifo, &wp, 1);

	spin_unlock(&transceiver_read_wp_lock);

	if (ret >= 1)
		complete(&transceiver_read_wp_completion);
	else
		pr_err("%s kfifo in fail wp:%d ret:%d\n", __func__, wp, ret);
}

static void transceiver_data_report(struct share_mem_data *data)
{
	pr_err("%s type:%u, action:%u, time:%lld, data:[%d,%d,%d,%d,%d] [%d]\n",
		__func__, data->sensor_type, data->action, data->timestamp,
		data->value[0], data->value[1], data->value[2],
		data->value[3], data->value[4], data->value[5]);
}

static void transceiver_data_process(void)
{
	int i = 0;
	int size = 0;
	int item_size = 0;
	int buffer_size = 0;

	item_size = sizeof(share_mem_read_buffer[0]);
	buffer_size = sizeof(share_mem_read_buffer);
	do {
		size = share_mem_read(&transceiver_shm_data_reader,
			share_mem_read_buffer, buffer_size);
		if (size < 0 || size > buffer_size) {
			pr_err("%s share_mem_read fail, size:%d\n",
				__func__, size);
			break;
		}

		if (size == 0)
			break;

		for (i = 0; i < (size / item_size); i++)
			transceiver_data_report(&share_mem_read_buffer[i]);

	} while (size == buffer_size);
}

static int transceiver_data_handler(void *data)
{
	int ret = 0;
	uint32_t wp = 0;
	unsigned long flags = 0;

	while (1) {
		wait_for_completion(&transceiver_read_wp_completion);

		while (1) {
			spin_lock_irqsave(&transceiver_read_wp_lock, flags);
			ret = kfifo_out(&transceiver_read_wp_fifo, &wp, 1);
			if (ret <= 0) {
				spin_unlock_irqrestore(
					&transceiver_read_wp_lock, flags);
				break;
			}
			ret = share_mem_seek(&transceiver_shm_data_reader, wp);
			if (ret < 0) {
				pr_err("%s share_mem_seek fail, ret:%d\n",
					__func__, ret);
				spin_unlock_irqrestore(
					&transceiver_read_wp_lock, flags);
				continue;
			}
			spin_unlock_irqrestore(&transceiver_read_wp_lock,
				flags);

			transceiver_data_process();
		}
	}
	return 0;
}

static void transceiver_power_up(void)
{
	share_mem_config();
}

static int transceiver_ready_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	if (event)
		transceiver_power_up();

	return NOTIFY_DONE;
}

static struct notifier_block transceiver_ready_notifier = {
	.notifier_call = transceiver_ready_notifier_call,
	.priority = READY_HIGHPRI,
};

static int transceiver_share_mem_cfg(struct share_mem_cfg *cfg,
		void *private_data)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&transceiver_read_wp_lock, flags);
	kfifo_reset(&transceiver_read_wp_fifo);
	spin_unlock_irqrestore(&transceiver_read_wp_lock, flags);

	transceiver_shm_data_reader.name = "trans_data_r";
	transceiver_shm_data_reader.item_size =
		sizeof(struct share_mem_data);
	transceiver_shm_data_reader.buffer_full_detect = false;

	return share_mem_init(&transceiver_shm_data_reader, cfg);
}

static int __init transceiver_init(void)
{
	int ret = 0;
	struct task_struct *task = NULL;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	ret = sensor_comm_init();
	if (ret < 0) {
		pr_err("Failed sensor comm init, ret=%d\n", ret);
		return ret;
	}
	ret = host_ready_init();
	if (ret < 0) {
		pr_err("Failed host ready init, ret=%d\n", ret);
		goto out_sensor_comm;
	}

	sensor_ready_notifier_chain_register(&transceiver_ready_notifier);

	ret = sensor_list_init();
	if (ret < 0) {
		pr_err("Failed sensor_list_init, ret=%d\n", ret);
		goto out_ready;
	}

	task = kthread_run(transceiver_data_handler, NULL,
		"transceiver_data_handler");
	if (IS_ERR(task)) {
		ret = -ENOMEM;
		pr_err("Failed create data thread ret:%d\n", ret);
		goto out_sensor_list;
	}
	sched_setscheduler(task, SCHED_FIFO, &param);

	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_DATA_CMD,
		transceiver_notify_func, NULL);
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_FULL_CMD,
		transceiver_notify_func, NULL);
	share_mem_config_handler_register(SENS_COMM_NOTIFY_DATA_CMD,
		transceiver_share_mem_cfg, NULL);
	return 0;

out_sensor_list:
	sensor_list_exit();
out_ready:
	sensor_ready_notifier_chain_unregister(&transceiver_ready_notifier);
	host_ready_exit();
out_sensor_comm:
	sensor_comm_exit();
	return ret;
}

static void __exit transceiver_exit(void)
{
	share_mem_config_handler_unregister(SENS_COMM_NOTIFY_DATA_CMD);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_FULL_CMD);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_DATA_CMD);
	sensor_list_exit();
	sensor_ready_notifier_chain_unregister(&transceiver_ready_notifier);
	host_ready_exit();
	sensor_comm_exit();
}

module_init(transceiver_init);
module_exit(transceiver_exit);
MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("transceiver driver");
MODULE_LICENSE("GPL");

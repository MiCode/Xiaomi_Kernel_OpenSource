// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "transceiver " fmt

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
#include "debug.h"

#define TRANSCEIVER_SHM_BUFFER_NUM 8
#define TRANSCEIVER_WP_NUM 32

DECLARE_COMPLETION(transceiver_wp_done);
DEFINE_KFIFO(transceiver_wp_fifo, uint32_t, TRANSCEIVER_WP_NUM);
DEFINE_SPINLOCK(transceiver_wp_fifo_lock);

static struct task_struct *transceiver_task;
static struct share_mem transceiver_shm_reader;
static struct share_mem_data share_mem_buffer[TRANSCEIVER_SHM_BUFFER_NUM];

static void transceiver_notify_func(struct sensor_comm_notify *n,
		void *private_data)
{
	uint32_t wp = 0;

	if (n->command != SENS_COMM_NOTIFY_DATA_CMD &&
	    n->command != SENS_COMM_NOTIFY_FULL_CMD)
		return;

	spin_lock(&transceiver_wp_fifo_lock);
	if (kfifo_is_full(&transceiver_wp_fifo)) {
		kfifo_out(&transceiver_wp_fifo, &wp, 1);
		pr_err_ratelimited("drop old write position\n");
	}
	wp = n->dnotify.write_position;
	kfifo_in(&transceiver_wp_fifo, &wp, 1);
	complete(&transceiver_wp_done);
	spin_unlock(&transceiver_wp_fifo_lock);
}

static void transceiver_report(struct share_mem_data *data)
{
	pr_err("report type:%u, action:%u, time:%lld, data:[%d,%d,%d,%d,%d] [%d]\n",
		data->sensor_type, data->action, data->timestamp,
		data->value[0], data->value[1], data->value[2],
		data->value[3], data->value[4], data->value[5]);
}

static void transceiver_read(uint32_t write_position)
{
	int ret = 0;
	int i = 0;

	ret = share_mem_seek(&transceiver_shm_reader, write_position);
	if (ret < 0) {
		pr_err("seek fail %d\n", ret);
		return;
	}

	do {
		ret = share_mem_read(&transceiver_shm_reader,
			share_mem_buffer, sizeof(share_mem_buffer));
		if (ret < 0 || ret > sizeof(share_mem_buffer)) {
			pr_err("read fail %d\n", ret);
			break;
		}

		if (ret == 0)
			break;

		for (i = 0; i < (ret / sizeof(share_mem_buffer[0])); i++)
			transceiver_report(&share_mem_buffer[i]);
	} while (ret == sizeof(share_mem_buffer));
}

static int transceiver_thread(void *data)
{
	unsigned int ret = 0;
	uint32_t wp = 0;
	unsigned long flags = 0;

	while (!kthread_should_stop()) {
		wait_for_completion(&transceiver_wp_done);
		while (1) {
			spin_lock_irqsave(&transceiver_wp_fifo_lock, flags);
			ret = kfifo_out(&transceiver_wp_fifo, &wp, 1);
			spin_unlock_irqrestore(&transceiver_wp_fifo_lock,
				flags);
			if (!ret)
				break;
			transceiver_read(wp);
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

static int transceiver_share_mem_cfg(struct share_mem_config *cfg,
		void *private_data)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&transceiver_wp_fifo_lock, flags);
	kfifo_reset(&transceiver_wp_fifo);
	spin_unlock_irqrestore(&transceiver_wp_fifo_lock, flags);

	transceiver_shm_reader.name = "trans_data_r";
	transceiver_shm_reader.item_size = sizeof(struct share_mem_data);
	transceiver_shm_reader.buffer_full_detect = false;

	return share_mem_init(&transceiver_shm_reader, cfg);
}

static int __init transceiver_init(void)
{
	int ret = 0;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	ret = sensor_comm_init();
	if (ret < 0) {
		pr_err("sensor comm init fail %d\n", ret);
		return ret;
	}
	ret = host_ready_init();
	if (ret < 0) {
		pr_err("host ready init fail %d\n", ret);
		goto out_sensor_comm;
	}

	sensor_ready_notifier_chain_register(&transceiver_ready_notifier);

	ret = sensor_list_init();
	if (ret < 0) {
		pr_err("sensor list init fail %d\n", ret);
		goto out_ready;
	}

	ret = debug_init();
	if (ret < 0) {
		pr_err("debug init fail %d\n", ret);
		goto out_sensor_list;
	}

	transceiver_task = kthread_run(transceiver_thread, NULL, "transceiver");
	if (IS_ERR(transceiver_task)) {
		ret = -ENOMEM;
		pr_err("create thread fail %d\n", ret);
		goto out_debug;
	}
	sched_setscheduler(transceiver_task, SCHED_FIFO, &param);

	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_DATA_CMD,
		transceiver_notify_func, NULL);
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_FULL_CMD,
		transceiver_notify_func, NULL);
	share_mem_config_handler_register(SENS_COMM_NOTIFY_DATA_CMD,
		transceiver_share_mem_cfg, NULL);
	return 0;

out_debug:
	debug_exit();
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
	if (!IS_ERR(transceiver_task))
		kthread_stop(transceiver_task);
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

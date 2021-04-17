// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[sensor_ready] " fmt

#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "hf_sensor_type.h"
#include "scp_helper.h"
#include "sensor_comm.h"
#include "ready.h"

static DEFINE_SPINLOCK(sensor_ready_lock);
static bool platform_ready;
static bool scp_ready;
static bool sensor_ready;
static BLOCKING_NOTIFIER_HEAD(sensor_ready_notifier_head);
static struct workqueue_struct *sensor_ready_workqueue;
static struct work_struct sensor_ready_work;

void __attribute__((weak)) scp_A_register_notify(struct notifier_block *nb)
{

}

void __attribute__((weak)) scp_A_unregister_notify(struct notifier_block *nb)
{

}

void sensor_ready_notifier_chain_register(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&sensor_ready_notifier_head, nb);
	if (READ_ONCE(sensor_ready))
		nb->notifier_call(nb, true, NULL);
}

void sensor_ready_notifier_chain_unregister(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&sensor_ready_notifier_head, nb);
}

static void scp_ready_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&sensor_ready_lock, flags);
	scp_ready = true;
	if (platform_ready && scp_ready) {
		sensor_ready = true;
		queue_work(sensor_ready_workqueue, &sensor_ready_work);
	}
	spin_unlock_irqrestore(&sensor_ready_lock, flags);
}

static int platform_ready_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	unsigned long flags = 0;
	struct sensor_comm_notify notify;

	if (event == SCP_EVENT_STOP) {
		spin_lock_irqsave(&sensor_ready_lock, flags);
		platform_ready = false;
		scp_ready = false;
		sensor_ready = false;
		queue_work(sensor_ready_workqueue, &sensor_ready_work);
		spin_unlock_irqrestore(&sensor_ready_lock, flags);
	} else if (event == SCP_EVENT_READY) {
		notify.sensor_type = SENSOR_TYPE_INVALID;
		notify.command = SENS_COMM_NOTIFY_READY_CMD;
		notify.length = 0;
		notify.sequence = 0;
		if (sensor_comm_notify(&notify) < 0)
			pr_err("Failed notify ready to scp\n");
		spin_lock_irqsave(&sensor_ready_lock, flags);
		platform_ready = true;
		if (platform_ready && scp_ready) {
			sensor_ready = true;
			queue_work(sensor_ready_workqueue,
				&sensor_ready_work);
		}
		spin_unlock_irqrestore(&sensor_ready_lock, flags);
	}

	return NOTIFY_DONE;
}

static struct notifier_block platform_ready_notifier = {
	.notifier_call = platform_ready_notifier_call,
};

static void sensor_ready_work_fn(struct work_struct *work)
{
	blocking_notifier_call_chain(&sensor_ready_notifier_head,
		READ_ONCE(sensor_ready), NULL);
}

int host_ready_init(void)
{
	INIT_WORK(&sensor_ready_work, sensor_ready_work_fn);
	sensor_ready_workqueue = alloc_workqueue("sensor_ready",
		WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
	if (!sensor_ready_workqueue) {
		pr_err("Failed to alloc workqueue\n");
		return -ENOMEM;
	}
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_READY_CMD,
		scp_ready_notify_handler, NULL);
	scp_A_register_notify(&platform_ready_notifier);
	return 0;
}

void host_ready_exit(void)
{
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_READY_CMD);
	scp_A_unregister_notify(&platform_ready_notifier);
	destroy_workqueue(sensor_ready_workqueue);
}

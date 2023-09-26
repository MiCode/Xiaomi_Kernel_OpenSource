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

#define pr_fmt(fmt) "sensor_ready " fmt

#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "hf_sensor_type.h"
#include "scp_helper.h"
#include "sensor_comm.h"
#include "ready.h"

static DEFINE_SPINLOCK(sensor_ready_lock);
static bool scp_platform_ready;
static bool scp_sensor_ready;
static bool sensor_ready;
static BLOCKING_NOTIFIER_HEAD(sensor_ready_notifier_head);
static struct workqueue_struct *sensor_ready_workqueue;
static struct work_struct sensor_ready_work;
static struct delayed_work sensor_rescure_work;

void sensor_ready_notifier_chain_register(struct notifier_block *nb)
{
	unsigned long flags = 0;
	bool status = false;

	blocking_notifier_chain_register(&sensor_ready_notifier_head, nb);
	/*
	 * NOTE: must copy sensor_ready to status to avoid sensor_ready
	 * modified during notifier calling, keep notify the same status.
	 */
	spin_lock_irqsave(&sensor_ready_lock, flags);
	status = sensor_ready;
	spin_unlock_irqrestore(&sensor_ready_lock, flags);
	if (status)
		nb->notifier_call(nb, status, NULL);
}

void sensor_ready_notifier_chain_unregister(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&sensor_ready_notifier_head, nb);
}

static void scp_sensor_ready_notify_handler(struct sensor_comm_notify *n,
		void *private_data)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&sensor_ready_lock, flags);
	scp_sensor_ready = true;
	if (scp_platform_ready && scp_sensor_ready) {
		sensor_ready = true;
		cancel_delayed_work(&sensor_rescure_work);
		queue_work(sensor_ready_workqueue, &sensor_ready_work);
	}
	spin_unlock_irqrestore(&sensor_ready_lock, flags);
}

static int scp_platform_ready_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	unsigned long flags = 0;
	int ret = 0;
	struct sensor_comm_notify notify;

	if (event == SCP_EVENT_STOP) {
		cancel_delayed_work(&sensor_rescure_work);
		spin_lock_irqsave(&sensor_ready_lock, flags);
		scp_platform_ready = false;
		scp_sensor_ready = false;
		sensor_ready = false;
		queue_work(sensor_ready_workqueue, &sensor_ready_work);
		spin_unlock_irqrestore(&sensor_ready_lock, flags);
	} else if (event == SCP_EVENT_READY) {
		notify.sequence = 0;
		notify.sensor_type = SENSOR_TYPE_INVALID;
		notify.command = SENS_COMM_NOTIFY_READY_CMD;
		notify.length = 0;
		ret = sensor_comm_notify_bypass(&notify);
		if (ret < 0)
			pr_err("notify ready to scp fail %d\n", ret);
		queue_delayed_work(sensor_ready_workqueue,
			&sensor_rescure_work, msecs_to_jiffies(5000));
		spin_lock_irqsave(&sensor_ready_lock, flags);
		scp_platform_ready = true;
		if (scp_platform_ready && scp_sensor_ready) {
			sensor_ready = true;
			queue_work(sensor_ready_workqueue,
				&sensor_ready_work);
		}
		spin_unlock_irqrestore(&sensor_ready_lock, flags);
	}

	return NOTIFY_DONE;
}

static struct notifier_block scp_platform_ready_notifier = {
	.notifier_call = scp_platform_ready_notifier_call,
};

static void sensor_ready_work_fn(struct work_struct *work)
{
	unsigned long flags = 0;
	bool status = false;

	/*
	 * NOTE: must copy sensor_ready to status to avoid sensor_ready
	 * modified during notifier calling, keep notify the same status.
	 */
	spin_lock_irqsave(&sensor_ready_lock, flags);
	status = sensor_ready;
	spin_unlock_irqrestore(&sensor_ready_lock, flags);
	blocking_notifier_call_chain(&sensor_ready_notifier_head,
		status, NULL);
}

static void sensor_rescure_work_fn(struct work_struct *work)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&sensor_ready_lock, flags);
	if (scp_platform_ready && !scp_sensor_ready) {
		pr_alert("rescure sensor by scp reset due to no ready ack\n");
		//scp_wdt_reset(0);
	}
	spin_unlock_irqrestore(&sensor_ready_lock, flags);
}

int host_ready_init(void)
{
	INIT_WORK(&sensor_ready_work, sensor_ready_work_fn);
	INIT_DELAYED_WORK(&sensor_rescure_work, sensor_rescure_work_fn);
	sensor_ready_workqueue = alloc_workqueue("sensor_ready",
		WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
	if (!sensor_ready_workqueue) {
		pr_err("alloc workqueue fail\n");
		return -ENOMEM;
	}
	/*
	 * NOTE: sensor comm notify must before scp register notify
	 * to avoid lost sensor ready notify.
	 */
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_READY_CMD,
		scp_sensor_ready_notify_handler, NULL);
	scp_A_register_notify(&scp_platform_ready_notifier);
	return 0;
}

void host_ready_exit(void)
{
	scp_A_unregister_notify(&scp_platform_ready_notifier);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_READY_CMD);
	flush_workqueue(sensor_ready_workqueue);
	destroy_workqueue(sensor_ready_workqueue);
}

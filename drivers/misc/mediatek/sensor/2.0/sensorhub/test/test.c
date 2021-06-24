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

#define pr_fmt(fmt) "test_sensor " fmt

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>

#include "hf_sensor_type.h"
#include "sensor_comm.h"
#include "ready.h"

struct test_sensor_t {
	char *name;
	struct hrtimer test_timer;
	struct work_struct test_work;
	struct workqueue_struct *workqueue;
};

static struct test_sensor_t test_sensor = {
	.name = "test_sensor",
};

static int enable_disable_count;

static void test_work_func(struct work_struct *work)
{
	int ret = 0;
	struct sensor_comm_ctrl *ctrl;
	struct sensor_comm_batch *batch;

	if (enable_disable_count++ % 2) {
		ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
		ctrl->sensor_type = SENSOR_TYPE_ACCELEROMETER;
		ctrl->command = SENS_COMM_CTRL_DISABLE_CMD;
		ctrl->length = 0;
	} else {
		ctrl = kzalloc(sizeof(*ctrl) + sizeof(*batch), GFP_KERNEL);
		ctrl->sensor_type = SENSOR_TYPE_ACCELEROMETER;
		ctrl->command = SENS_COMM_CTRL_ENABLE_CMD;
		ctrl->length = sizeof(*batch);
		batch = (struct sensor_comm_batch *)ctrl->data;
		batch->delay = 20000000;
		batch->latency = 1000000000;
	}
	ret = sensor_comm_ctrl_send(ctrl, sizeof(*ctrl) + ctrl->length);
	if (ret < 0)
		pr_err("enable disable fail %d\n", ret);
	kfree(ctrl);
}

static enum hrtimer_restart test_timer_func(struct hrtimer *timer)
{
	queue_work(test_sensor.workqueue, &test_sensor.test_work);
	hrtimer_forward_now(&test_sensor.test_timer, ns_to_ktime(10000000000));
	return HRTIMER_RESTART;
}

static int test_ready_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	return NOTIFY_DONE;
}

static struct notifier_block test_ready_notifier = {
	.notifier_call = test_ready_notifier_call,
	.priority = READY_STDPRI,
};

static int __init test_init(void)
{
	INIT_WORK(&test_sensor.test_work, test_work_func);
	test_sensor.workqueue = create_singlethread_workqueue(test_sensor.name);
	hrtimer_init(&test_sensor.test_timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	test_sensor.test_timer.function = test_timer_func;
	hrtimer_start(&test_sensor.test_timer,
			ns_to_ktime(10000000000), HRTIMER_MODE_REL);
	sensor_ready_notifier_chain_register(&test_ready_notifier);
	return 0;
}

static void __exit test_exit(void)
{

}

module_init(test_init);
module_exit(test_exit);
MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("test driver");
MODULE_LICENSE("GPL");

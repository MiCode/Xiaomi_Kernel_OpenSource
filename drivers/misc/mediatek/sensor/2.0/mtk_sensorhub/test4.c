// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[test_sensor4] " fmt

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>

#include "hf_sensor_type.h"
#include "sensor_comm.h"

struct test_sensor_t {
	int count;
	char *name;
	struct hrtimer test_timer;
	struct work_struct test_work;
	struct workqueue_struct *workqueue;
};

static struct test_sensor_t test_sensor = {
	.name = "test_sensor4",
};

static void test_work_func(struct work_struct *work)
{
	struct sensor_comm_notify notify;

	notify.sensor_type = SENSOR_TYPE_ACCELEROMETER;
	notify.command = SENS_COMM_NOTIFY_DEBUG_CMD;
	notify.value[0] = test_sensor.count++;
	pr_err("%s send notify sensor_type=%u, command=%u value=%d\n",
		__func__, notify.sensor_type, notify.command, notify.value[0]);
	sensor_comm_notify(&notify);
}

static enum hrtimer_restart test_timer_func(struct hrtimer *timer)
{
	queue_work(test_sensor.workqueue, &test_sensor.test_work);
	return HRTIMER_NORESTART;
}

static void notify_func(struct sensor_comm_notify *n, void *private_data)
{
	pr_err("%s receive notify sensor_type=%u, command=%u value=%d\n",
		__func__, n->sensor_type, n->command, n->value[0]);
	hrtimer_start(&test_sensor.test_timer,
		ns_to_ktime(10000000000), HRTIMER_MODE_REL);
}

static int __init test_init(void)
{
	sensor_comm_notify_handler_register(
		SENS_COMM_NOTIFY_DEBUG_CMD, notify_func, NULL);
	INIT_WORK(&test_sensor.test_work, test_work_func);
	test_sensor.workqueue = create_singlethread_workqueue(test_sensor.name);
	hrtimer_init(&test_sensor.test_timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	test_sensor.test_timer.function = test_timer_func;
	hrtimer_start(&test_sensor.test_timer,
		ns_to_ktime(10000000000), HRTIMER_MODE_REL);
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

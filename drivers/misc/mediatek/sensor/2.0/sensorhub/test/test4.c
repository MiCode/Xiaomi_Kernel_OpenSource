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

#define pr_fmt(fmt) "test_sensor4 " fmt

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>

#include "hf_sensor_type.h"
#include "hf_manager.h"
#include "sensor_comm.h"
#include "sensor_list.h"

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

static struct sensor_info sensor_list[SENSOR_TYPE_SENSOR_MAX];

static void test_work_func(struct work_struct *work)
{
	int ret = 0;
	struct sensor_comm_notify notify;
	unsigned int i = 0;

	notify.sequence = 0;
	notify.sensor_type = SENSOR_TYPE_ACCELEROMETER;
	notify.command = SENS_COMM_NOTIFY_TEST_CMD;
	notify.length = sizeof(notify.value[0]);
	notify.value[0] = test_sensor.count++;
	ret = sensor_comm_notify(&notify);
	if (ret < 0)
		pr_err("notify failed %d\n", ret);

	memset(sensor_list, 0, sizeof(sensor_list));
	ret = sensor_list_get_list(sensor_list, ARRAY_SIZE(sensor_list));
	if (ret < 0) {
		pr_err("get sensor list failed %d\n", ret);
		return;
	}

	for (i = 0; i < ret; i++) {
		if (sensor_list[i].sensor_type == SENSOR_TYPE_INVALID)
			continue;

		pr_err("sensor list type:%u, gain:%u, name:%s, vendor:%s\n",
			sensor_list[i].sensor_type,
			sensor_list[i].gain, sensor_list[i].name,
			sensor_list[i].vendor);
	}
}

static enum hrtimer_restart test_timer_func(struct hrtimer *timer)
{
	queue_work(test_sensor.workqueue, &test_sensor.test_work);
	return HRTIMER_NORESTART;
}

static void notify_func(struct sensor_comm_notify *n, void *private_data)
{
	hrtimer_start(&test_sensor.test_timer,
		ns_to_ktime(10000000000), HRTIMER_MODE_REL);
}

static int __init test_init(void)
{
	sensor_comm_notify_handler_register(
		SENS_COMM_NOTIFY_TEST_CMD, notify_func, NULL);
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

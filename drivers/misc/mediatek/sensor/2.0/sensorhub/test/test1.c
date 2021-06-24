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

#define pr_fmt(fmt) "test_sensor1 " fmt

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/time.h>
#include <asm/arch_timer.h>

#include "hf_sensor_type.h"
#include "sensor_comm.h"

struct test_sensor_t {
	char *name;
	struct hrtimer test_timer;
	struct work_struct test_work;
	struct workqueue_struct *workqueue;
};

static struct test_sensor_t test_sensor = {
	.name = "test_sensor1",
};

static void test_work_func(struct work_struct *work)
{
	int ret = 0;
	struct sensor_comm_ctrl *ctrl = NULL;
	struct sensor_comm_timesync *timesync = NULL;

	ctrl = kzalloc(sizeof(*ctrl) + sizeof(*timesync), GFP_KERNEL);
	ctrl->sensor_type = SENSOR_TYPE_INVALID;
	ctrl->command = SENS_COMM_CTRL_TIMESYNC_CMD;
	ctrl->length = sizeof(*timesync);
	timesync = (struct sensor_comm_timesync *)ctrl->data;
	local_irq_disable();
	timesync->host_timestamp = ktime_get_boot_ns();
	timesync->host_archcounter = arch_counter_get_cntvct();
	local_irq_enable();
	ret = sensor_comm_ctrl_send(ctrl, sizeof(*ctrl) + ctrl->length);
	if (ret < 0)
		pr_err("timesync fail %d\n", ret);
	kfree(ctrl);
}

static enum hrtimer_restart test_timer_func(struct hrtimer *timer)
{
	queue_work(test_sensor.workqueue, &test_sensor.test_work);
	hrtimer_forward_now(&test_sensor.test_timer, ns_to_ktime(10000000000));
	return HRTIMER_RESTART;
}

static int __init test_init(void)
{
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

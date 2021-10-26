/*
 * Copyright (C) 2016 MediaTek Inc.
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

#define pr_fmt(fmt) "[test_app2] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kobject.h>

#include "hf_manager.h"

struct test_app_t {
	struct task_struct *task;
	struct hf_client *client;
};

static struct test_app_t test_app;

static int test_app_kthread(void *arg)
{
	struct hf_client *client = test_app.client;
	struct hf_manager_event data[4];
	int size = 0, i = 0;

	if (!client)
		return -EINVAL;

	while (!kthread_should_stop()) {
		memset(data, 0, sizeof(data));
		/*
		 * must use timeout api to wakeup kthread and do exit
		 * otherwise kthread_stop will be blocked forever
		 */
		size = hf_client_poll_sensor_timeout(client, data,
			ARRAY_SIZE(data), msecs_to_jiffies(500));
		if (size < 0)
			continue;
		for (i = 0; i < size; ++i) {
			pr_info_ratelimited("[%d,%d,%lld,%d,%d,%d]\n",
				data[i].sensor_type,
				data[i].action,
				data[i].timestamp,
				data[i].word[0],
				data[i].word[1],
				data[i].word[2]);
		}
	}
	return 0;
}

#define test_app_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.store	= _name##_store,		\
}

static ssize_t control_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	int ret = 0;
	int sensor_type = 0, val1 = 0, val2 = 0;
	struct hf_manager_cmd cmd;

	ret = sscanf(buf, "%u,%u,%u", &sensor_type, &val1, &val2);
	if (ret != 3) {
		pr_err("control store param error\n");
		return -EINVAL;
	}

	if (val1 == HF_MANAGER_SENSOR_ENABLE) {
		if (test_app.client)
			return -EINVAL;
		test_app.client = hf_client_create();
		if (!test_app.client) {
			pr_err("hf_client_create fail\n");
			return -ENOMEM;
		}
		if (!test_app.task) {
			test_app.task = kthread_run(test_app_kthread,
				&test_app, "test_app2");
			if (IS_ERR(test_app.task)) {
				pr_err("kthread_run create fail\n");
				return -ENOMEM;
			}
		}
		ret = hf_client_find_sensor(test_app.client, sensor_type);
		if (ret < 0) {
			pr_err("hf_client_find_sensor %u fail\n",
				sensor_type);
			return -EINVAL;
		}
		memset(&cmd, 0, sizeof(cmd));
		cmd.sensor_type = sensor_type;
		cmd.action = val1;
		cmd.delay = val2;
		cmd.latency = 0;
		ret = hf_client_control_sensor(test_app.client, &cmd);
		if (ret < 0) {
			pr_err("hf_client_control_sensor %u fail\n",
				sensor_type);
			return -EINVAL;
		}
	} else if (val1 == HF_MANAGER_SENSOR_DISABLE) {
		if (test_app.client) {
			memset(&cmd, 0, sizeof(cmd));
			cmd.sensor_type = sensor_type;
			cmd.action = val1;
			hf_client_control_sensor(test_app.client, &cmd);
		}
		if (test_app.task) {
			kthread_stop(test_app.task);
			test_app.task = NULL;
		}
		if (test_app.client) {
			hf_client_destroy(test_app.client);
			test_app.client = NULL;
		}
	}

	return n;
}

test_app_attr(control);

static struct attribute *attr[] = {
	&control_attr.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attr,
};

static int __init test_app_init(void)
{
	struct kobject *kobj = kobject_create_and_add("test_app2", NULL);

	if (!kobj) {
		pr_err("kobject create fail\n");
		return -ENOMEM;
	}
	if (sysfs_create_group(kobj, &attr_group)) {
		pr_err("sysfs create fail\n");
		return -EFAULT;
	}
	return 0;
}

module_init(test_app_init);

MODULE_DESCRIPTION("high frequency manager test");
MODULE_AUTHOR("Hongxu Zhao <hongxu.zhao@mediatek.com>");
MODULE_LICENSE("GPL v2");

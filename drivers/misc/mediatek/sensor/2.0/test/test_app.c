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

#define pr_fmt(fmt) "[test_app] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kobject.h>

#include "hf_manager.h"

struct test_app_t {
	struct task_struct *task;
	struct hf_client *client;
	struct kobject *kobj;
	int sensor_type;
	int val1;
	int val2;
};

static struct test_app_t test_app;

static int test_app_kthread(void *arg)
{
	struct hf_client *client = NULL;
	struct hf_manager_event data[4];
	int size = 0, i = 0;

	client = hf_client_create();
	if (!client) {
		pr_err("hf_client_create fail\n");
		return -ENOMEM;
	}
	test_app.client = client;

	while (!kthread_should_stop()) {
		memset(data, 0, sizeof(data));
		size = hf_client_poll_sensor(client, data, ARRAY_SIZE(data));
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

			/* need derequest sensor cali */
			switch (data[i].action) {
			case CALI_ACTION:
				hf_client_request_sensor_cali(test_app.client,
					test_app.sensor_type,
					HF_MANAGER_REQUEST_CALI_DATA,
					false);
				break;
			case TEST_ACTION:
				hf_client_request_sensor_cali(test_app.client,
					test_app.sensor_type,
					HF_MANAGER_REQUEST_TEST_DATA,
					false);
				break;
			}
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
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

static ssize_t control_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return sprintf(buf, "sensor_type=%u,val1=%u,val2=%u\n",
		test_app.sensor_type,
		test_app.val1,
		test_app.val2);
}

static ssize_t control_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	int ret = 0;
	struct hf_manager_cmd cmd;

	if (!test_app.client)
		goto out;

	ret = sscanf(buf, "%u,%u,%u", &test_app.sensor_type,
		&test_app.val1, &test_app.val2);
	if (ret != 3) {
		pr_err("control store param error\n");
		goto out;
	}

	ret = hf_client_find_sensor(test_app.client, test_app.sensor_type);
	if (ret < 0) {
		pr_err("hf_client_find_sensor %u fail\n",
			test_app.sensor_type);
		goto out;
	}

	switch (test_app.val1) {
	case HF_MANAGER_SENSOR_ENABLE_CALI:
		hf_client_request_sensor_cali(test_app.client,
			test_app.sensor_type,
			HF_MANAGER_REQUEST_CALI_DATA,
			true);
		break;
	case HF_MANAGER_SENSOR_SELFTEST:
		hf_client_request_sensor_cali(test_app.client,
			test_app.sensor_type,
			HF_MANAGER_REQUEST_TEST_DATA,
			true);
		break;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.sensor_type = test_app.sensor_type;
	cmd.action = test_app.val1;
	cmd.delay = test_app.val2;
	cmd.latency = 0;
	ret = hf_client_control_sensor(test_app.client, &cmd);
	if (ret < 0) {
		pr_err("hf_client_control_sensor %u fail\n",
			test_app.sensor_type);
		goto out;
	}
out:
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
	test_app.task = kthread_run(test_app_kthread,
		&test_app, "test_app");
	if (IS_ERR(test_app.task))
		pr_err("kthread_run create fail\n");

	test_app.kobj = kobject_create_and_add("test_app", NULL);
	if (!test_app.kobj) {
		pr_err("kobject create fail\n");
		return -ENOMEM;
	}
	if (sysfs_create_group(test_app.kobj, &attr_group)) {
		pr_err("sysfs create fail\n");
		return -EFAULT;
	}
	return 0;
}

module_init(test_app_init);

MODULE_DESCRIPTION("high frequency manager test");
MODULE_AUTHOR("Hongxu Zhao <hongxu.zhao@mediatek.com>");
MODULE_LICENSE("GPL v2");

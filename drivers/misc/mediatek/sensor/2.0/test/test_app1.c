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

#define pr_fmt(fmt) "[test_app1] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>

#include "hf_manager.h"

enum {
	CUST_CMD_CALI,
	MAX_CUST_CMD,
};

#define test_app_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
}

static ssize_t test_app1_cmd(char *buf, int sensor_type,
		int action, unsigned int request)
{
	ssize_t ret = 0;
	struct hf_client *client = NULL;
	struct hf_manager_cmd cmd;
	struct hf_manager_event data[1];

	client = hf_client_create();
	if (!client) {
		pr_err("hf_client_create fail\n");
		return -ENOMEM;
	}
	ret = hf_client_find_sensor(client, sensor_type);
	if (ret < 0) {
		pr_err("hf_client_find_sensor %u fail\n", sensor_type);
		goto out;
	}
	hf_client_request_sensor_cali(client, sensor_type,
		request, true);
	memset(&cmd, 0, sizeof(cmd));
	cmd.sensor_type = sensor_type;
	cmd.action = action;
	ret = hf_client_control_sensor(client, &cmd);
	if (ret < 0) {
		pr_err("hf_client_control_sensor %u %u fail\n",
			sensor_type, action);
		goto out;
	}
	ret = hf_client_poll_sensor_timeout(client, data, ARRAY_SIZE(data),
		msecs_to_jiffies(3000));
	hf_client_request_sensor_cali(client, sensor_type,
		request, false);
	if (ret >= 0)
		ret = sprintf(buf, "[%d,%d,%d,%lld,%d,%d,%d]\n",
				data[0].sensor_type,
				data[0].action,
				data[0].accurancy,
				data[0].timestamp,
				data[0].word[0],
				data[0].word[1],
				data[0].word[2]);
out:
	hf_client_destroy(client);
	return ret;
}

static ssize_t test_app1_cust(char *buf, int sensor_type,
		int action)
{
	ssize_t ret = 0;
	struct hf_client *client = NULL;
	struct custom_cmd cmd;

	client = hf_client_create();
	if (!client) {
		pr_err("hf_client_create fail\n");
		return -ENOMEM;
	}
	ret = hf_client_find_sensor(client, sensor_type);
	if (ret < 0) {
		pr_err("hf_client_find_sensor %u fail\n", sensor_type);
		goto out;
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = action;
	cmd.tx_len = 0;
	cmd.rx_len = 48;
	ret = hf_client_custom_cmd(client, sensor_type, &cmd);
	if (ret >= 0)
		ret = sprintf(buf, "[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]\n",
				cmd.data[0],
				cmd.data[1],
				cmd.data[2],
				cmd.data[3],
				cmd.data[4],
				cmd.data[5],
				cmd.data[6],
				cmd.data[7],
				cmd.data[8],
				cmd.data[9],
				cmd.data[10],
				cmd.data[11]);
out:
	hf_client_destroy(client);
	return ret;
}

static ssize_t acc_cali_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return test_app1_cmd(buf, SENSOR_TYPE_ACCELEROMETER,
			HF_MANAGER_SENSOR_ENABLE_CALI,
			HF_MANAGER_REQUEST_CALI_DATA);
}

static ssize_t acc_cust_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return test_app1_cust(buf, SENSOR_TYPE_ACCELEROMETER,
			CUST_CMD_CALI);
}


static ssize_t acc_seltest_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return test_app1_cmd(buf, SENSOR_TYPE_ACCELEROMETER,
			HF_MANAGER_SENSOR_SELFTEST,
			HF_MANAGER_REQUEST_TEST_DATA);
}

static ssize_t gyro_cali_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return test_app1_cmd(buf, SENSOR_TYPE_GYROSCOPE,
			HF_MANAGER_SENSOR_ENABLE_CALI,
			HF_MANAGER_REQUEST_CALI_DATA);
}

static ssize_t gyro_cust_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return test_app1_cust(buf, SENSOR_TYPE_GYROSCOPE,
			CUST_CMD_CALI);
}

static ssize_t gyro_selftest_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return test_app1_cmd(buf, SENSOR_TYPE_GYROSCOPE,
			HF_MANAGER_SENSOR_SELFTEST,
			HF_MANAGER_REQUEST_TEST_DATA);
}

test_app_attr(acc_cali);
test_app_attr(acc_cust);
test_app_attr(acc_seltest);
test_app_attr(gyro_cali);
test_app_attr(gyro_cust);
test_app_attr(gyro_selftest);

static struct attribute *attr[] = {
	&acc_cali_attr.attr,
	&acc_cust_attr.attr,
	&acc_seltest_attr.attr,
	&gyro_cali_attr.attr,
	&gyro_cust_attr.attr,
	&gyro_selftest_attr.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attr,
};

static int __init test_app_init(void)
{
	struct kobject *kobj = kobject_create_and_add("test_app1", NULL);

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

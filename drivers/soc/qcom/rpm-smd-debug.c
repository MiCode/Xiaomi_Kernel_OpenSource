/* Copyright (c) 2013-2014, 2017,  The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "rpm-smd-debug: %s(): " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <soc/qcom/rpm-smd.h>

#define MAX_MSG_BUFFER 350
#define MAX_KEY_VALUE_PAIRS 20

static struct dentry *rpm_debugfs_dir;

static u32 string_to_uint(const u8 *str)
{
	int i, len;
	u32 output = 0;

	len = strnlen(str, sizeof(u32));
	for (i = 0; i < len; i++)
		output |= str[i] << (i * 8);

	return output;
}

static ssize_t rsc_ops_write(struct file *fp, const char __user *user_buffer,
						size_t count, loff_t *position)
{
	char buf[MAX_MSG_BUFFER], rsc_type_str[6] = {}, rpm_set[8] = {},
						key_str[6] = {};
	int i, pos = -1, set = -1, nelems = -1;
	char *cmp;
	uint32_t rsc_type = 0, rsc_id = 0, key = 0, data = 0;
	struct msm_rpm_request *req;

	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(&buf, user_buffer, count))
		return -EFAULT;
	buf[count] = '\0';
	cmp = strstrip(buf);

	if (sscanf(cmp, "%7s %5s %u %d %n", rpm_set, rsc_type_str,
				&rsc_id, &nelems, &pos) != 4) {
		pr_err("Invalid number of arguments passed\n");
		goto err;
	}

	if (strlen(rpm_set) > 6 || strlen(rsc_type_str) > 4) {
		pr_err("Invalid value of set or resource type\n");
		goto err;
	}

	if (!strcmp(rpm_set, "active"))
		set = 0;
	else if (!strcmp(rpm_set, "sleep"))
		set = 1;

	rsc_type = string_to_uint(rsc_type_str);

	if (set < 0 || nelems < 0) {
		pr_err("Invalid value of set or nelems\n");
		goto err;
	}
	if (nelems > MAX_KEY_VALUE_PAIRS) {
		pr_err("Exceeded max no of key-value entries\n");
		goto err;
	}

	req = msm_rpm_create_request(set, rsc_type, rsc_id, nelems);
	if (!req)
		return -ENOMEM;

	for (i = 0; i < nelems; i++) {
		cmp += pos;
		if (sscanf(cmp, "%5s %n", key_str, &pos) != 1) {
			pr_err("Invalid number of arguments passed\n");
			goto err_request;
		}

		if (strlen(key_str) > 4) {
			pr_err("Key value cannot be more than 4 charecters");
			goto err_request;
		}
		key = string_to_uint(key_str);
		if (!key) {
			pr_err("Key values entered incorrectly\n");
			goto err_request;
		}

		cmp += pos;
		if (sscanf(cmp, "%u %n", &data, &pos) != 1) {
			pr_err("Invalid number of arguments passed\n");
			goto err_request;
		}

		if (msm_rpm_add_kvp_data(req, key,
				(void *)&data, sizeof(data)))
			goto err_request;
	}

	if (msm_rpm_wait_for_ack(msm_rpm_send_request(req)))
		pr_err("Sending the RPM message failed\n");

err_request:
	msm_rpm_free_request(req);
err:
	return count;
}

static const struct file_operations rsc_ops = {
	.write = rsc_ops_write,
};

static int __init rpm_smd_debugfs_init(void)
{
	rpm_debugfs_dir = debugfs_create_dir("rpm_send_msg", NULL);
	if (!rpm_debugfs_dir)
		return -ENOMEM;

	if (!debugfs_create_file("message", 0200, rpm_debugfs_dir, NULL,
								&rsc_ops))
		return -ENOMEM;

	return 0;
}
late_initcall(rpm_smd_debugfs_init);

static void __exit rpm_smd_debugfs_exit(void)
{
	debugfs_remove_recursive(rpm_debugfs_dir);
}
module_exit(rpm_smd_debugfs_exit);

MODULE_DESCRIPTION("RPM SMD Debug Driver");
MODULE_LICENSE("GPL v2");

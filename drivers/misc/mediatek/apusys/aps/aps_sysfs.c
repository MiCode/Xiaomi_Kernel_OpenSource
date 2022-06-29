// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include "aps_sysfs.h"
#include "aps_ipi.h"
#include "aps_utils.h"

static struct kobject *root_dir;

static ssize_t loglevel_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	uint64_t level = 0;

	if (sprintf(buf, "%d", (uint32_t)level) < 0)
		return -EINVAL;
	APS_INFO("%s, level= %d\n", __func__, (uint32_t)level);

	aps_ipi_recv(APS_LOG_LEVEL, &level);

	return 0;
}

static ssize_t loglevel_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *cmd, size_t count)
{
	uint64_t ret = 0;
	int level = 0;

	ret = kstrtoint(cmd, 10, &level);

	if (!ret) {
		APS_INFO("%s, level= %d\n", __func__, (uint32_t)level);
		aps_ipi_send(APS_LOG_LEVEL, level);
	} else {
		APS_ERR("%s[%d]: get invalid cmd\n", __func__, __LINE__);
	}
	return count;

}

static struct kobj_attribute loglevel = {
	.attr = {
		.name = "loglevel",
		.mode = 0644,
	},
	.show = loglevel_show,
	.store = loglevel_store,
};

int aps_sysfs_init(void)
{

	int ret = 0;

	/* create /sys/kernel/aps */
	APS_INFO("%s +\n", __func__);
	root_dir = kobject_create_and_add("aps", kernel_kobj);
	if (!root_dir) {
		APS_ERR("%s kobject_create_and_add fail for aps, ret %d\n",
			__func__, ret);
		return -EINVAL;
	}

	ret = sysfs_create_file(root_dir, &loglevel.attr);

	return ret;
}

void aps_sysfs_exit(void)
{
	kobject_del(root_dir);
}

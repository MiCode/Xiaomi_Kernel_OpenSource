// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include "mvpu_sysfs.h"
#include "mvpu_ipi.h"


static struct kobject *root_dir;


static ssize_t loglevel_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	uint64_t level = 0;

	sprintf(buf, "%d", level);
	pr_info("%s, level= %d\n", __func__, (uint32_t)level);

	mvpu_ipi_recv(MVPU_LOG_LEVEL, &level);

	return 0;
}

static ssize_t loglevel_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *cmd, size_t count)
{
	uint64_t ret = 0;
	int level = 0;

	ret = kstrtoint(cmd, 10, &level);

	if (!ret) {
		pr_info("%s, level= %d\n", __func__, (uint32_t)level);
		mvpu_ipi_send(MVPU_LOG_LEVEL, level);
	} else {
		pr_info("%s[%d]: get invalid cmd\n", __func__, __LINE__);
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

int mvpu_sysfs_init(void)
{

	int ret = 0;

	pr_info("%s\n", __func__);

	/* create /sys/kernel/mvpu */
	root_dir = kobject_create_and_add("mvpu", kernel_kobj);
	if (!root_dir) {
		pr_info("%s kobject_create_and_add fail for mvpu, ret %d\n",
			__func__, ret);
		return -EINVAL;
	}

	sysfs_create_file(root_dir, &loglevel.attr);

	return ret;
}

void mvpu_sysfs_exit(void)
{
	kobject_put(root_dir);
}

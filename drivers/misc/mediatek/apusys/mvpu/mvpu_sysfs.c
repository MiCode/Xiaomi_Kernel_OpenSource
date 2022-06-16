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

#include "mvpu_sec.h"

static struct kobject *root_dir;

#ifndef MVPU_SECURITY
#define MVPU_SECURITY
#endif

#ifdef MVPU_SECURITY
static uint64_t ptn_total_size;

static ssize_t mvpu_img_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	int ret = 0;

	if (ptn_total_size == 0)
		ptn_total_size = get_ptn_total_size();
	else
		pr_info("[MVPU] already get ptn_total_size: 0x%x\n", ptn_total_size);

	ret = sprintf(buf, "0x%llx", ptn_total_size);
	if (ret < 0) {
		pr_info("[MVPU] %s, sprintf error\n", __func__);
		return -1;
	}
	pr_info("[MVPU] %s, ptn_size = 0x%x\n", __func__, (uint32_t)ptn_total_size);

	return 0;
}

static ssize_t mvpu_img_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *cmd, size_t count)
{
	return count;
}

static struct kobj_attribute get_mvpu_img = {
	.attr = {
		.name = "mvpu_img_sz",
		.mode = 0644,
	},
	.show = mvpu_img_show,
	.store = mvpu_img_store,
};
#endif

static ssize_t loglevel_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	uint64_t level = 0;
	int ret = sprintf(buf, "%llu", level);

	if (ret < 0) {
		pr_info("[MVPU] %s, sprintf error\n", __func__);
		return -1;
	}
	pr_info("[MVPU] %s, level= %d\n", __func__, (uint32_t)level);

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
		pr_info("[MVPU] %s, level= %d\n", __func__, (uint32_t)level);
		mvpu_ipi_send(MVPU_LOG_LEVEL, level);
	} else {
		pr_info("[MVPU] %s[%d]: get invalid cmd\n", __func__, __LINE__);
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

#ifdef MVPU_SECURITY
	sysfs_create_file(root_dir, &get_mvpu_img.attr);
#endif

	return ret;
}

void mvpu_sysfs_exit(void)
{
	kobject_put(root_dir);
}

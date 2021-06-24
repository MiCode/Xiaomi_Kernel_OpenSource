// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include "mdw_cmn.h"

static uint32_t g_sched_plcy_show;

static ssize_t dsp_task_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return -EINVAL;

}
static DEVICE_ATTR_RO(dsp_task_num);

static ssize_t dla_task_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return -EINVAL;
}
static DEVICE_ATTR_RO(dla_task_num);

static ssize_t dma_task_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return -EINVAL;
}
static DEVICE_ATTR_RO(dma_task_num);

static struct attribute *mdw_task_attrs[] = {
	&dev_attr_dsp_task_num.attr,
	&dev_attr_dla_task_num.attr,
	&dev_attr_dma_task_num.attr,
	NULL,
};

static struct attribute_group mdw_devinfo_attr_group = {
	.name	= "queue",
	.attrs	= mdw_task_attrs,
};

static ssize_t policy_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t policy_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	return 0;
}
static DEVICE_ATTR_RW(policy);

static struct attribute *mdw_sched_attrs[] = {
	&dev_attr_policy.attr,
	NULL,
};

static struct attribute_group mdw_sched_attr_group = {
	.name	= "sched",
	.attrs	= mdw_sched_attrs,
};

static ssize_t mem_statistics_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int n = 0;

	//ToDo
	//mdw_usr_sys_aee_mem(buf, &n);

	return n;
}
static DEVICE_ATTR_RO(mem_statistics);

static struct attribute *mdw_mem_attrs[] = {
	&dev_attr_mem_statistics.attr,
	NULL,
};

static struct attribute_group mdw_mem_attr_group = {
	.name	= "memory",
	.attrs	= mdw_mem_attrs,
};

int mdw_sysfs_init(struct mdw_device *mdev)
{
	int ret = 0;

	g_sched_plcy_show = 0;

	/* create /sys/devices/platform/apusys/device/xxx */
	ret = sysfs_create_group(&mdev->pdev->dev.kobj,
		&mdw_devinfo_attr_group);
	if (ret)
		mdw_drv_err("create mdw devinfo attr fail, ret %d\n", ret);
	ret = sysfs_create_group(&mdev->pdev->dev.kobj,
		&mdw_sched_attr_group);
	if (ret)
		mdw_drv_err("create mdw sched attr fail, ret %d\n", ret);

	ret = sysfs_create_group(&mdev->pdev->dev.kobj,
		&mdw_mem_attr_group);
	if (ret)
		mdw_drv_err("create mdw mem attr fail, ret %d\n", ret);
	return ret;
}

void mdw_sysfs_deinit(struct mdw_device *mdev)
{
	sysfs_remove_group(&mdev->pdev->dev.kobj, &mdw_devinfo_attr_group);
}

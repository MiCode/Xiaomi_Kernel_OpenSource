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
	struct mdw_device *mdev = mdw_dev;
	int ret = 0;
	uint32_t num = 0;

	if (!mdev) {
		mdw_drv_err("no mdw device\n");
		ret = -ENODEV;
		goto out;
	}

	/* get dma normal task num */
	num = mdev->dev_funcs->get_info(mdev, MDW_INFO_NORMAL_TASK_DSP);
	ret = sprintf(buf, "%u\n", num);
	if (ret < 0)
		mdw_drv_warn("show dsp task num fail(%d)\n", ret);

out:
	return ret;
}
static DEVICE_ATTR_RO(dsp_task_num);

static ssize_t dla_task_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdw_device *mdev = mdw_dev;
	int ret = 0;
	uint32_t num = 0;

	if (!mdev) {
		mdw_drv_err("no mdw device\n");
		ret = -ENODEV;
		goto out;
	}

	/* get dla normal task num */
	num = mdev->dev_funcs->get_info(mdev, MDW_INFO_NORMAL_TASK_DLA);
	ret = sprintf(buf, "%u\n", num);
	if (ret < 0)
		mdw_drv_warn("show dla task num fail(%d)\n", ret);

out:
	return ret;
}
static DEVICE_ATTR_RO(dla_task_num);

static ssize_t dma_task_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdw_device *mdev = mdw_dev;
	int ret = 0;
	uint32_t num = 0;

	if (!mdev) {
		mdw_drv_err("no mdw device\n");
		ret = -ENODEV;
		goto out;
	}

	/* get dma normal task num */
	num = mdev->dev_funcs->get_info(mdev, MDW_INFO_NORMAL_TASK_DMA);
	ret = sprintf(buf, "%u\n", num);
	if (ret < 0)
		mdw_drv_warn("show dma task num fail(%d)\n", ret);

out:
	return ret;
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

//------------------------------
static ssize_t policy_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return -EINVAL;
}

static ssize_t policy_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	return -EINVAL;
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

//------------------------------
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

//------------------------------
static ssize_t ulog_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdw_device *mdev = mdw_dev;
	int ret = 0;
	uint32_t log_lv = 0;

	if (!mdev) {
		mdw_drv_err("no mdw device\n");
		ret = -ENODEV;
		goto out;
	}

	log_lv = mdev->dev_funcs->get_info(mdev, MDW_INFO_ULOG);
	ret = sprintf(buf, "%u\n", log_lv);
	if (ret < 0)
		mdw_drv_warn("show ulog fail(%d)\n", log_lv);

out:
	return ret;
}

static ssize_t ulog_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct mdw_device *mdev = mdw_dev;
	uint32_t val = 0;

	if (!mdev) {
		mdw_drv_err("no mdw device\n");
		return -ENODEV;
	}

	if (!kstrtouint(buf, 10, &val)) {
		mdw_drv_info("set ulog(%u)\n", val);
		mdev->dev_funcs->set_param(mdev, MDW_INFO_ULOG, val);
	}

	return count;
}
static DEVICE_ATTR_RW(ulog);

static ssize_t klog_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdw_device *mdev = mdw_dev;
	int ret = 0;
	uint32_t log_lv = 0;

	if (!mdev) {
		mdw_drv_err("no mdw device\n");
		ret = -ENODEV;
		goto out;
	}

	log_lv = mdev->dev_funcs->get_info(mdev, MDW_INFO_KLOG);
	ret = sprintf(buf, "%u\n", log_lv);
	if (ret < 0)
		mdw_drv_warn("show klog fail(%d)\n", log_lv);

out:
	return ret;
}

static ssize_t klog_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct mdw_device *mdev = mdw_dev;
	uint32_t val = 0;

	if (!mdev) {
		mdw_drv_err("no mdw device\n");
		return -ENODEV;
	}

	if (!kstrtouint(buf, 10, &val)) {
		mdw_drv_info("set klog(%u)\n", val);
		mdev->dev_funcs->set_param(mdev, MDW_INFO_KLOG, val);
	}

	return count;
}
static DEVICE_ATTR_RW(klog);

static ssize_t tlog_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	uint32_t log_lv = 0;

	log_lv = cfg_apusys_trace;
	ret = sprintf(buf, "%u\n", log_lv);
	if (ret < 0)
		mdw_drv_warn("show tlog fail(%d)\n", log_lv);

	return ret;
}

static ssize_t tlog_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	uint32_t val = 0;

	if (!kstrtouint(buf, 10, &val)) {
		mdw_drv_info("set tlog(%u)\n", val);
		cfg_apusys_trace = val;
	}

	return count;
}
static DEVICE_ATTR_RW(tlog);

static struct attribute *mdw_log_attrs[] = {
	&dev_attr_ulog.attr,
	&dev_attr_klog.attr,
	&dev_attr_tlog.attr,
	NULL,
};

static struct attribute_group mdw_log_attr_group = {
	.name	= "log",
	.attrs	= mdw_log_attrs,
};

//------------------------------
int mdw_sysfs_init(struct mdw_device *mdev)
{
	int ret = 0;

	g_sched_plcy_show = 0;

	/* create /sys/class/misc/apusys/xxx */
	ret = sysfs_create_group(&mdev->misc_dev->this_device->kobj,
		&mdw_devinfo_attr_group);
	if (ret)
		mdw_drv_err("create mdw devinfo attr fail, ret %d\n", ret);

	ret = sysfs_create_group(&mdev->misc_dev->this_device->kobj,
		&mdw_sched_attr_group);
	if (ret)
		mdw_drv_err("create mdw sched attr fail, ret %d\n", ret);

	ret = sysfs_create_group(&mdev->misc_dev->this_device->kobj,
		&mdw_log_attr_group);
	if (ret)
		mdw_drv_err("create mdw log attr fail, ret %d\n", ret);

	ret = sysfs_create_group(&mdev->misc_dev->this_device->kobj,
		&mdw_mem_attr_group);
	if (ret)
		mdw_drv_err("create mdw mem attr fail, ret %d\n", ret);

	return ret;
}

void mdw_sysfs_deinit(struct mdw_device *mdev)
{
	struct device *dev = mdev->misc_dev->this_device;

	sysfs_remove_group(&dev->kobj, &mdw_mem_attr_group);
	sysfs_remove_group(&dev->kobj, &mdw_log_attr_group);
	sysfs_remove_group(&dev->kobj, &mdw_sched_attr_group);
	sysfs_remove_group(&dev->kobj, &mdw_devinfo_attr_group);
}

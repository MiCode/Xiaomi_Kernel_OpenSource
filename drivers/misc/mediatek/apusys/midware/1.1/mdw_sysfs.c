/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include "mdw_rsc.h"
#include "mdw_queue.h"
#include "mdw_cmn.h"
#include "mdw_usr.h"

static struct device *mdw_dev;
static uint32_t g_sched_plcy_show;

static ssize_t dsp_task_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdw_queue *mq = NULL;
	int ret = 0;

	mq = mdw_rsc_get_queue(APUSYS_DEVICE_VPU);
	if (!mq)
		return -EINVAL;

	ret = sprintf(buf, "%u\n", mq->normal_task_num);
	if (ret < 0)
		mdw_drv_warn("show dsp task num fail(%d)\n", ret);

	return ret;
}
static DEVICE_ATTR_RO(dsp_task_num);

static ssize_t dla_task_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdw_queue *mq = NULL;
	int ret = 0;

	mq = mdw_rsc_get_queue(APUSYS_DEVICE_MDLA);
	if (!mq)
		return -EINVAL;

	ret = sprintf(buf, "%u\n", mq->normal_task_num);
	if (ret < 0)
		mdw_drv_warn("show dla task num fail(%d)\n", ret);

	return ret;
}
static DEVICE_ATTR_RO(dla_task_num);

static ssize_t dma_task_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdw_queue *mq = NULL;
	int ret = 0;

	mq = mdw_rsc_get_queue(APUSYS_DEVICE_EDMA);
	if (!mq)
		return -EINVAL;

	ret = sprintf(buf, "%u\n", mq->normal_task_num);
	if (ret < 0)
		mdw_drv_warn("show dma task num fail(%d)\n", ret);

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

static ssize_t policy_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char *p = buf;

	mdw_drv_debug("g_sched_plcy_show(%u)\n", g_sched_plcy_show);
	if (!g_sched_plcy_show)
		p += sprintf(p, "%u\n", mdw_rsc_get_preempt_plcy());
	else {
		p += sprintf(p, "preemption(%u)\n", mdw_rsc_get_preempt_plcy());
		p += sprintf(p, "  0: rr,simple\n");
		p += sprintf(p, "  1: rr,prefer lower priority\n");
	}

	WARN_ON(p - buf >= PAGE_SIZE);

	return p - buf;
}

static ssize_t policy_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	uint32_t in = 0;
	int ret = 0;
	char plcy[32];

	if (sscanf(buf, "%31s %u", plcy, &in) != 2)
		return -EPERM;

	mdw_drv_debug("plcy(%s), in(%u)\n", plcy, in);
	if (!strcmp(plcy, "preemption")) {
		ret = mdw_rsc_set_preempt_plcy(in);
		if (ret)
			mdw_drv_err("set preempt plcy(%u) fail(%d)\n",
				in, ret);
	} else if (!strcmp(plcy, "show_info")) {
		g_sched_plcy_show = in;
	}

	return count;
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

	mdw_usr_sys_aee_mem(buf, &n);

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


int mdw_sysfs_init(struct device *kdev)
{
	int ret = 0;
	g_sched_plcy_show = 0;

	/* create /sys/class/misc/apusys/xxx */
	mdw_dev = kdev;
	ret = sysfs_create_group(&mdw_dev->kobj, &mdw_devinfo_attr_group);
	if (ret)
		mdw_drv_err("create mdw devinfo attr fail, ret %d\n", ret);
	ret = sysfs_create_group(&mdw_dev->kobj, &mdw_sched_attr_group);
	if (ret)
		mdw_drv_err("create mdw sched attr fail, ret %d\n", ret);

	ret = sysfs_create_group(&mdw_dev->kobj, &mdw_mem_attr_group);
	if (ret)
		mdw_drv_err("create mdw mem attr fail, ret %d\n", ret);

	return ret;
}

void mdw_sysfs_exit(void)
{
	if (mdw_dev) {
		sysfs_remove_group(&mdw_dev->kobj, &mdw_mem_attr_group);
		sysfs_remove_group(&mdw_dev->kobj, &mdw_sched_attr_group);
		sysfs_remove_group(&mdw_dev->kobj, &mdw_devinfo_attr_group);
	}
	mdw_dev = NULL;
}

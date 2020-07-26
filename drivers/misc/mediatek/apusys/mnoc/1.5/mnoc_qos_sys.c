// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/device.h>

#include "mnoc_qos.h"
#include "mnoc_api.h"

static ssize_t mnoc_apu_qos_boost_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if MNOC_TIME_PROFILE && MNOC_QOS_BOOST_ENABLE
	return sprintf(buf, "apu_qos_boost_flag = %d\n", apu_qos_boost_flag);
#endif
}

static ssize_t mnoc_apu_qos_boost_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned int val;

	if (kstrtoint(buf, 10, &val) == 0) {
#if MNOC_QOS_BOOST_ENABLE
		if (val == 0) {
			mutex_lock(&apu_qos_boost_mtx);
			apu_qos_boost_flag = false;
			apu_qos_boost_end();
			mutex_unlock(&apu_qos_boost_mtx);
		} else if (val == 1) {
			mutex_lock(&apu_qos_boost_mtx);
			apu_qos_boost_flag = true;
			apu_qos_boost_start();
			mutex_unlock(&apu_qos_boost_mtx);
		}
#endif
		dev_dbg(dev, "set boost %d\n", val);
	}

	return count;
}
static DEVICE_ATTR_RW(mnoc_apu_qos_boost);

static ssize_t mnoc_cmd_qos_start_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if MNOC_TIME_PROFILE
	return sprintf(buf, "sum_start = %lu, cnt_start = %d, avg = %lu\n",
		sum_start, cnt_start, sum_start/cnt_start);
#endif
}

static ssize_t mnoc_cmd_qos_start_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned int cmd_id, sub_cmd_id;
	unsigned int dev_type, devcore, boost;

	if (sscanf(buf, "%d %d %d %d %d", &cmd_id, &sub_cmd_id,
		&dev_type, &devcore, &boost) == 4)
		apu_cmd_qos_start((uint64_t) cmd_id,
			(uint64_t) sub_cmd_id, dev_type, devcore, boost);
out:
	return count;
}
static DEVICE_ATTR_RW(mnoc_cmd_qos_start);

static ssize_t mnoc_cmd_qos_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if MNOC_TIME_PROFILE
	return sprintf(buf, "sum_suspend = %lu, cnt_suspend = %d, avg = %lu\n",
		sum_suspend, cnt_suspend, sum_suspend/cnt_suspend);
#endif
}

static ssize_t mnoc_cmd_qos_suspend_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned int cmd_id, sub_cmd_id;
	unsigned int dev_type, devcore;

	if (sscanf(buf, "%d %d %d %d", &cmd_id, &sub_cmd_id,
		&dev_type, &devcore) == 4)
		apu_cmd_qos_suspend((uint64_t) cmd_id,
			(uint64_t) sub_cmd_id, dev_type, devcore);

out:
	return count;
}
static DEVICE_ATTR_RW(mnoc_cmd_qos_suspend);

static ssize_t mnoc_cmd_qos_end_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if MNOC_TIME_PROFILE
	return sprintf(buf, "sum_end = %lu, cnt_end = %d, avg = %lu\n",
		sum_end, cnt_end, sum_end/cnt_end);
#endif
}

static ssize_t mnoc_cmd_qos_end_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned int cmd_id, sub_cmd_id;
	unsigned int dev_type, devcore;

	if (sscanf(buf, "%d %d %d %d", &cmd_id, &sub_cmd_id,
		&dev_type, &devcore) == 4)
		apu_cmd_qos_end((uint64_t) cmd_id,
			(uint64_t) sub_cmd_id, dev_type, devcore);

out:
	return count;
}
static DEVICE_ATTR_RW(mnoc_cmd_qos_end);

static struct attribute *qos_attrs[] = {
	&dev_attr_mnoc_apu_qos_boost.attr,
	&dev_attr_mnoc_cmd_qos_start.attr,
	&dev_attr_mnoc_cmd_qos_suspend.attr,
	&dev_attr_mnoc_cmd_qos_end.attr,
	NULL,
};

static struct attribute_group mnoc_qos_attr_group = {
	.name	= "qos", /* create separate directory to put qos attributes */
	.attrs	= qos_attrs,
};

int mnoc_qos_create_sys(struct device *dev)
{
	int rc = 0;

	rc = sysfs_create_group(&dev->kobj, &mnoc_qos_attr_group);
	if (!rc)
		dev_info(dev, "%s fail, ret %d\n", __func__, rc);
	return rc;
}

void mnoc_qos_remove_sys(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &mnoc_qos_attr_group);
}

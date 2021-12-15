// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>

#include "mnoc_qos.h"
#include "mnoc_api.h"
#include "mnoc_drv.h"

static ssize_t mnoc_apu_qos_boost_show(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       char *buf)
{
	int ret = 0;
#if MNOC_QOS_BOOST_ENABLE
	ret = sprintf(buf, "apu_qos_boost_flag = %d\n", apu_qos_boost_flag);
#endif
	if (ret < 0)
		LOG_ERR("%s failed, ret %d\n", __func__, ret);
	return ret;
}

static ssize_t mnoc_apu_qos_boost_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
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
		LOG_DEBUG("set boost %d\n", val);
	}

	return count;
}

static const struct kobj_attribute apu_qos_boost_attr =
	__ATTR(mnoc_apu_qos_boost, 0660, mnoc_apu_qos_boost_show,
	       mnoc_apu_qos_boost_store);

static ssize_t mnoc_cmd_qos_start_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
#if MNOC_TIME_PROFILE
	ret = sprintf(buf, "sum_start = %lu, cnt_start = %d, avg = %lu\n",
		      sum_start, cnt_start, sum_start / cnt_start);
#endif
	return ret;
}

static ssize_t mnoc_cmd_qos_start_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned int cmd_id, sub_cmd_id;
	unsigned int dev_type, devcore, boost;

	if (sscanf(buf, "%d %d %d %d %d", &cmd_id, &sub_cmd_id,
		&dev_type, &devcore, &boost) == 4) {
		apu_cmd_qos_start((uint64_t)cmd_id, (uint64_t)sub_cmd_id,
				  dev_type, devcore, boost);
	} else {
		LOG_ERR("%s input order is wrong\n", __func__);
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(mnoc_cmd_qos_start);

static ssize_t mnoc_cmd_qos_suspend_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;

#if MNOC_TIME_PROFILE
	ret = sprintf(buf, "sum_suspend = %lu, cnt_suspend = %d, avg = %lu\n",
		      sum_suspend, cnt_suspend, sum_suspend / cnt_suspend);
#endif
	return ret;
}

static ssize_t mnoc_cmd_qos_suspend_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned int cmd_id, sub_cmd_id;
	unsigned int dev_type, devcore;

	if (sscanf(buf, "%d %d %d %d", &cmd_id, &sub_cmd_id,
		   &dev_type, &devcore) == 4) {
		apu_cmd_qos_suspend((uint64_t)cmd_id, (uint64_t)sub_cmd_id,
				    dev_type, devcore);
	} else {
		LOG_ERR("%s input order is wrong\n", __func__);
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(mnoc_cmd_qos_suspend);

static ssize_t mnoc_cmd_qos_end_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int ret = 0;
#if MNOC_TIME_PROFILE
	ret = sprintf(buf, "sum_end = %lu, cnt_end = %d, avg = %lu\n",
		      sum_end, cnt_end, sum_end / cnt_end);
#endif
	return ret;
}

static ssize_t mnoc_cmd_qos_end_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned int cmd_id, sub_cmd_id;
	unsigned int dev_type, devcore;

	if (sscanf(buf, "%d %d %d %d", &cmd_id, &sub_cmd_id,
		&dev_type, &devcore) == 4) {
		apu_cmd_qos_end((uint64_t)cmd_id, (uint64_t)sub_cmd_id,
				dev_type, devcore);
	} else {
		LOG_ERR("%s input order is wrong\n", __func__);
		return -EINVAL;
	}
	return count;
}
static DEVICE_ATTR_RW(mnoc_cmd_qos_end);

static struct attribute *qos_attrs[] = {
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
	int ret = 0;
	struct apu_mnoc *p_mnoc = dev_get_drvdata(dev);

	/* create /sys/kernel/apusys */
	p_mnoc->root_dir = kobject_create_and_add("apusys", kernel_kobj);
	if (!p_mnoc->root_dir)
		return -EINVAL;

	/* create /sys/kernel/apusys/mnoc_apu_qos_boost */
	ret = sysfs_create_file(p_mnoc->root_dir, &apu_qos_boost_attr.attr);
	if (ret)
		goto out;

	ret = kobject_uevent(p_mnoc->root_dir, KOBJ_ADD);
	if (ret)
		goto out;

	/* create /sys/devices/platform/xxxx/qos */
	ret = sysfs_create_group(&dev->kobj, &mnoc_qos_attr_group);
	if (ret)
		goto out;

	ret = kobject_uevent(&dev->kobj, KOBJ_CHANGE);
out:
	return ret;
}

void mnoc_qos_remove_sys(struct device *dev)
{
	struct apu_mnoc *p_mnoc = dev_get_drvdata(dev);

	sysfs_remove_file(p_mnoc->root_dir, &apu_qos_boost_attr.attr);
	sysfs_remove_group(&dev->kobj, &mnoc_qos_attr_group);
}

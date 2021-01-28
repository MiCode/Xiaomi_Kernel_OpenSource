// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>

#include "mtk_qos_bound.h"

static ssize_t qos_bound_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_qos_bound_enabled());
}
static ssize_t qos_bound_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_bound_enable(val);

	return count;
}
static DEVICE_ATTR_RW(qos_bound_enable);

static ssize_t qos_bound_stress_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_qos_bound_stress_enabled());
}
static ssize_t qos_bound_stress_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_bound_stress_enable(val);

	return count;
}
static DEVICE_ATTR_RW(qos_bound_stress_enable);

static ssize_t qos_bound_log_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_qos_bound_log_enabled());
}
static ssize_t qos_bound_log_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_bound_log_enable(val);

	return count;
}
static DEVICE_ATTR_RW(qos_bound_log_enable);

static ssize_t qos_bound_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int qos_bound_count = get_qos_bound_count();
	unsigned int *qos_bound_buf = get_qos_bound_buf();

	return sprintf(buf, "count: %d, free: %d, cong: %d, full: %d\n",
			qos_bound_count, qos_bound_buf[0],
			qos_bound_buf[1], qos_bound_buf[2]);
}
static DEVICE_ATTR_RO(qos_bound_status);

static struct attribute *qos_attrs[] = {
	&dev_attr_qos_bound_enable.attr,
	&dev_attr_qos_bound_stress_enable.attr,
	&dev_attr_qos_bound_log_enable.attr,
	&dev_attr_qos_bound_status.attr,
	NULL,
};

static struct attribute_group qos_attr_group = {
	.name = "qos",
	.attrs = qos_attrs,
};

int qos_add_interface(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &qos_attr_group);
}

void qos_remove_interface(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &qos_attr_group);
}

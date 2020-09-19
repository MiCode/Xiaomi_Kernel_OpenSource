/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>

#include "mtk_qos_bound.h"
#ifdef QOS_PREFETCH_SUPPORT
#include "mtk_qos_prefetch.h"
#endif /* QOS_PREFETCH_SUPPORT */

#ifdef QOS_PREFETCH_SUPPORT
static ssize_t qos_prefetch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_qos_prefetch_enabled());
}
static ssize_t qos_prefetch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_prefetch_enable(val);

	return count;
}
static DEVICE_ATTR(qos_prefetch_enable, 0644,
		qos_prefetch_enable_show, qos_prefetch_enable_store);

static ssize_t qos_prefetch_force_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_qos_prefetch_force());
}
static ssize_t qos_prefetch_force_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_prefetch_force(val);

	return count;
}
static DEVICE_ATTR(qos_prefetch_force, 0644,
		qos_prefetch_force_show, qos_prefetch_force_store);

static ssize_t qos_prefetch_log_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_qos_prefetch_log_enabled());
}
static ssize_t qos_prefetch_log_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_prefetch_log_enable(val);

	return count;
}
static DEVICE_ATTR(qos_prefetch_log_enable, 0644,
		qos_prefetch_log_enable_show, qos_prefetch_log_enable_store);

static ssize_t qos_prefetch_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int qos_prefetch_count = get_qos_prefetch_count();
	unsigned int *qos_prefetch_buf = get_qos_prefetch_buf();

	return sprintf(buf, "count: %d, free: %d, cong: %d, full: %d\n",
			qos_prefetch_count, qos_prefetch_buf[0],
			qos_prefetch_buf[1], qos_prefetch_buf[2]);
}
static DEVICE_ATTR(qos_prefetch_status, 0444,
		qos_prefetch_status_show, NULL);

static ssize_t qos_prefetch_val_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "prefetch_val: 0x%x\n", prefetch_val);
}
static DEVICE_ATTR(qos_prefetch_val, 0444,
		qos_prefetch_val_show, NULL);
#endif /* QOS_PREFETCH_SUPPORT */

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
static DEVICE_ATTR(qos_bound_enable, 0644,
		qos_bound_enable_show, qos_bound_enable_store);

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
static DEVICE_ATTR(qos_bound_stress_enable, 0644,
		qos_bound_stress_enable_show, qos_bound_stress_enable_store);

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
static DEVICE_ATTR(qos_bound_log_enable, 0644,
		qos_bound_log_enable_show, qos_bound_log_enable_store);

static ssize_t qos_bound_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int qos_bound_count = get_qos_bound_count();
	unsigned int *qos_bound_buf = get_qos_bound_buf();

	return sprintf(buf, "count: %d, free: %d, cong: %d, full: %d\n",
			qos_bound_count, qos_bound_buf[0],
			qos_bound_buf[1], qos_bound_buf[2]);
}
static DEVICE_ATTR(qos_bound_status, 0444,
		qos_bound_status_show, NULL);

static struct attribute *qos_attrs[] = {
	&dev_attr_qos_bound_enable.attr,
	&dev_attr_qos_bound_stress_enable.attr,
	&dev_attr_qos_bound_log_enable.attr,
	&dev_attr_qos_bound_status.attr,
#ifdef QOS_PREFETCH_SUPPORT
	&dev_attr_qos_prefetch_enable.attr,
	&dev_attr_qos_prefetch_force.attr,
	&dev_attr_qos_prefetch_log_enable.attr,
	&dev_attr_qos_prefetch_status.attr,
	&dev_attr_qos_prefetch_val.attr,
#endif /* QOS_PREFETCH_SUPPORT */
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

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
#include <linux/pm_qos.h>
#include <linux/sysfs.h>

#include <apu_dvfs.h>


static struct pm_qos_request dvfs_vvpu_opp_req;
static struct pm_qos_request dvfs_vvpu2_opp_req;



static ssize_t dvfs_req_vvpu_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	pm_qos_update_request(&dvfs_vvpu_opp_req, val);

	return count;
}
static DEVICE_ATTR(dvfs_req_vvpu_opp, 0200,
		NULL, dvfs_req_vvpu_opp_store);

static ssize_t dvfs_req_vvpu2_opp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	pm_qos_update_request(&dvfs_vvpu2_opp_req, val);

	return count;
}
static DEVICE_ATTR(dvfs_req_vvpu2_opp, 0200,
		NULL, dvfs_req_vvpu2_opp_store);


static ssize_t dvfs_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;

	p = apu_dvfs_dump_reg(p);

	return p - buf;
}

static DEVICE_ATTR(dvfs_dump, 0444, dvfs_dump_show, NULL);


static struct attribute *apu_dvfs_attrs[] = {

	&dev_attr_dvfs_req_vvpu_opp.attr,
	&dev_attr_dvfs_req_vvpu2_opp.attr,
	&dev_attr_dvfs_dump.attr,

	NULL,
};

static struct attribute_group apu_dvfs_attr_group = {
	.name = "apu_dvfs",
	.attrs = apu_dvfs_attrs,
};

int apu_dvfs_add_interface(struct device *dev)
{
	pm_qos_add_request(&dvfs_vvpu_opp_req, PM_QOS_VVPU_OPP,
			PM_QOS_VVPU_OPP_DEFAULT_VALUE);
	pm_qos_add_request(&dvfs_vvpu2_opp_req, PM_QOS_VVPU_OPP,
			PM_QOS_VVPU_OPP_DEFAULT_VALUE);

	return sysfs_create_group(&dev->kobj, &apu_dvfs_attr_group);
}

void apu_dvfs_remove_interface(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &apu_dvfs_attr_group);
}

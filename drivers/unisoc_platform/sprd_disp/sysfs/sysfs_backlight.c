// SPDX-License-Identifier: GPL-2.0-only
/*
 * sysfs_backlight.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/backlight.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/sysfs.h>

#include "sprd_bl.h"

static ssize_t max_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	unsigned long maxbrightness;
	struct backlight_device *bd = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 0, &maxbrightness);
	if (rc)
		return rc;

	mutex_lock(&bd->ops_lock);
	if (bd->ops) {
		pr_debug("set max_brightness to %lu\n", maxbrightness);
		bd->props.max_brightness = maxbrightness;
		if (bd->props.brightness > maxbrightness) {
			bd->props.brightness = maxbrightness;
			backlight_update_status(bd);
		}
	}
	mutex_unlock(&bd->ops_lock);

	return count;
}

static ssize_t max_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct backlight_device *bd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", bd->props.max_brightness);
}
static DEVICE_ATTR_RW(max_brightness);

static struct attribute *backlight_attrs[] = {
	&dev_attr_max_brightness.attr,
	NULL,
};
ATTRIBUTE_GROUPS(backlight);

int sprd_backlight_sysfs_init(struct device *dev)
{
	int rc;

	rc = sysfs_create_groups(&dev->kobj, backlight_groups);
	if (rc)
		pr_err("create backlight attr node failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL(sprd_backlight_sysfs_init);

MODULE_AUTHOR("Gaofeng Sheng <gaofeng.sheng@unisoc.com>");
MODULE_DESCRIPTION("Add backlight attribute nodes for userspace");
MODULE_LICENSE("GPL v2");

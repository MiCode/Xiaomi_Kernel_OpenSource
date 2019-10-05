/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>

#include "edma_driver.h"
#include "edma_dbgfs.h"
#include "edma_reg.h"

static ssize_t show_edma_initialize(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct edma_device *edma_device = dev_get_drvdata(dev);

	if (!edma_device->edma_init_done) {
		dev_notice(dev, "edma not initialize!\n");
		return count;
	}

	count = scnprintf(buf, PAGE_SIZE, "edma initialize done!\n");

	return count;
}

static ssize_t set_edma_initialize(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct edma_device *edma_device = dev_get_drvdata(dev);
	int ret;

	if (edma_device->edma_init_done) {
		dev_notice(dev, "edma initialize done!\n");
		return count;
	}

	ret = edma_initialize(edma_device);
	if (ret)
		pr_notice("fail to initialize edma");

	return count;
}

static ssize_t show_edma_register(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0, i, core, desp;
	struct edma_device *edma_device = dev_get_drvdata(dev);

	if (!edma_device->edma_init_done) {
		dev_notice(dev, "edma not initialize!\n");
		return count;
	}

	for (core = 0; core < edma_device->edma_sub_num; core++) {
		struct edma_sub *edma_sub = edma_device->edma_sub[core];

		count += scnprintf(buf + count, PAGE_SIZE,
				 "core: %d\n", core);
		for (i = 0; i < (EDMA_REG_SHOW_RANGE >> 2); i++) {
			count += scnprintf(buf + count, PAGE_SIZE,
				 "0x%03x: 0x%08x\n", i * 4,
				 edma_read_reg32(edma_sub->base_addr, i * 4));
		}

		for (desp = 0; desp < 4; desp++) {
			u32 base;

			count += scnprintf(buf + count, PAGE_SIZE,
				 "descriptor %d\n", desp);
			base = APU_EDMA2_DESP0_0 + desp * APU_EDMA2_DESP_OFFSET;
			for (i = 0; i < (APU_EDMA2_EX_DESP_OFFSET >> 2); i++) {
				count += scnprintf(buf + count, PAGE_SIZE,
					 "0x%03x: 0x%08x\n", base + i * 4,
					 edma_read_reg32(edma_sub->base_addr,
								base + i * 4));
			}
		}
	}

	return count;
}


DEVICE_ATTR(edma_initialize, 0644, show_edma_initialize,
	    set_edma_initialize);
DEVICE_ATTR(edma_register, 0644, show_edma_register,
	    NULL);

static struct attribute *edma_sysfs_entries[] = {
	&dev_attr_edma_initialize.attr,
	&dev_attr_edma_register.attr,
	NULL,
};

static const struct attribute_group edma_attr_group = {
	.attrs = edma_sysfs_entries,
};

int edma_create_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &edma_attr_group);
	if (ret)
		dev_notice(dev, "create sysfs group error, %d\n", ret);

	return ret;
}

void edma_remove_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &edma_attr_group);
}


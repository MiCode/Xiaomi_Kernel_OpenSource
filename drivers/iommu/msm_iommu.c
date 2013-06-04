/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/export.h>
#include <linux/iommu.h>
#include <mach/iommu.h>

static DEFINE_MUTEX(iommu_list_lock);
static LIST_HEAD(iommu_list);

static struct iommu_access_ops *iommu_access_ops;

struct bus_type msm_iommu_sec_bus_type = {
	.name = "msm_iommu_sec_bus",
};

void msm_set_iommu_access_ops(struct iommu_access_ops *ops)
{
	iommu_access_ops = ops;
}

struct iommu_access_ops *msm_get_iommu_access_ops()
{
	BUG_ON(iommu_access_ops == NULL);
	return iommu_access_ops;
}
EXPORT_SYMBOL(msm_get_iommu_access_ops);

void msm_iommu_add_drv(struct msm_iommu_drvdata *drv)
{
	mutex_lock(&iommu_list_lock);
	list_add(&drv->list, &iommu_list);
	mutex_unlock(&iommu_list_lock);
}

void msm_iommu_remove_drv(struct msm_iommu_drvdata *drv)
{
	mutex_lock(&iommu_list_lock);
	list_del(&drv->list);
	mutex_unlock(&iommu_list_lock);
}

static int find_iommu_ctx(struct device *dev, void *data)
{
	struct msm_iommu_ctx_drvdata *c;

	c = dev_get_drvdata(dev);
	if (!c || !c->name)
		return 0;

	return !strcmp(data, c->name);
}

static struct device *find_context(struct device *dev, const char *name)
{
	return device_find_child(dev, (void *)name, find_iommu_ctx);
}

struct device *msm_iommu_get_ctx(const char *ctx_name)
{
	struct msm_iommu_drvdata *drv;
	struct device *dev = NULL;

	mutex_lock(&iommu_list_lock);
	list_for_each_entry(drv, &iommu_list, list) {
		dev = find_context(drv->dev, ctx_name);
		if (dev)
			break;
	}
	mutex_unlock(&iommu_list_lock);

	put_device(dev);

	if (!dev || !dev_get_drvdata(dev)) {
		pr_debug("Could not find context <%s>\n", ctx_name);
		dev = ERR_PTR(-EPROBE_DEFER);
	}

	return dev;
}
EXPORT_SYMBOL(msm_iommu_get_ctx);


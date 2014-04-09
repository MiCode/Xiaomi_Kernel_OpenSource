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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include <linux/memory_alloc.h>
#include <linux/io.h>
#include <linux/of.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "adreno.h"

struct coresight_attr {
	struct device_attribute attr;
	int regname;
};

#define CORESIGHT_CREATE_REG_ATTR(_attrname, _regname) \
	struct coresight_attr coresight_attr_##_attrname = \
	{ __ATTR(_attrname, S_IRUGO | S_IWUSR, gfx_show_reg, gfx_store_reg),\
		_regname}

/**
 * adreno_coresight_enable() - Generic function to enable coresight debugging
 * @csdev: Pointer to coresight's device struct
 *
 * This is a generic function to enable coresight debug bus on adreno
 * devices. This should be used in all cases of enabling
 * coresight debug bus for adreno devices. This function in turn calls
 * the adreno device specific function through gpudev hook.
 * This function is registered as the coresight enable function
 * with coresight driver. It should only be called through coresight driver
 * as that would ensure that the necessary setup required to be done
 * on coresight driver's part is also done.
 */
int adreno_coresight_enable(struct coresight_device *csdev)
{
	struct kgsl_device *device = dev_get_drvdata(csdev->dev.parent);
	struct adreno_device *adreno_dev;

	if (device == NULL)
		return -ENODEV;

	adreno_dev = ADRENO_DEVICE(device);

	/* Check if coresight compatible device, return error otherwise */
	if (adreno_dev->gpudev->coresight_enable)
		return adreno_dev->gpudev->coresight_enable(device);
	else
		return -ENODEV;
}

/**
 * adreno_coresight_disable() - Generic function to disable coresight debugging
 * @csdev: Pointer to coresight's device struct
 *
 * This is a generic function to disable coresight debug bus on adreno
 * devices. This should be used in all cases of disabling
 * coresight debug bus for adreno devices. This function in turn calls
 * the adreno device specific function through the gpudev hook.
 * This function is registered as the coresight disable function
 * with coresight driver. It should only be called through coresight driver
 * as that would ensure that the necessary setup required to be done on
 * coresight driver's part is also done.
 */
void adreno_coresight_disable(struct coresight_device *csdev)
{
	struct kgsl_device *device = dev_get_drvdata(csdev->dev.parent);
	struct adreno_device *adreno_dev;

	if (device == NULL)
		return;

	adreno_dev = ADRENO_DEVICE(device);

	/* Check if coresight compatible device, bail otherwise */
	if (adreno_dev->gpudev->coresight_disable)
		return adreno_dev->gpudev->coresight_disable(device);
}

static const struct coresight_ops_source adreno_coresight_ops_source = {
	.enable = adreno_coresight_enable,
	.disable = adreno_coresight_disable,
};

static const struct coresight_ops adreno_coresight_cs_ops = {
	.source_ops = &adreno_coresight_ops_source,
};

void adreno_coresight_remove(struct platform_device *pdev)
{
	struct kgsl_device_platform_data *pdata = pdev->dev.platform_data;
	coresight_unregister(pdata->csdev);
}

static ssize_t coresight_read_reg(struct kgsl_device *device,
		unsigned int offset, char *buf)
{
	unsigned int regval = 0;

	mutex_lock(&device->mutex);
	if (!kgsl_active_count_get(device)) {
		kgsl_regread(device, offset, &regval);
		kgsl_active_count_put(device);
	}
	mutex_unlock(&device->mutex);
	return snprintf(buf, PAGE_SIZE, "0x%X", regval);
}

static inline unsigned int coresight_convert_reg(const char *buf)
{
	long regval = 0;
	int rv = 0;

	rv = kstrtoul(buf, 16, &regval);
	if (!rv)
		return (unsigned int)regval;
	else
		return rv;
}

static ssize_t gfx_show_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev->parent);
	struct coresight_attr *csight_attr = container_of(attr,
			struct coresight_attr, attr);

	if (device == NULL)
		return -ENODEV;

	return coresight_read_reg(device, csight_attr->regname, buf);
}

static ssize_t gfx_store_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct kgsl_device *device = dev_get_drvdata(dev->parent);
	struct adreno_device *adreno_dev;
	struct coresight_attr *csight_attr = container_of(attr,
			struct coresight_attr, attr);
	unsigned int regval = 0;

	if (device == NULL)
		return -ENODEV;

	adreno_dev = ADRENO_DEVICE(device);

	regval = coresight_convert_reg(buf);

	if (adreno_dev->gpudev->coresight_config_debug_reg)
		adreno_dev->gpudev->coresight_config_debug_reg(device,
				csight_attr->regname, regval);
	return size;
}

CORESIGHT_CREATE_REG_ATTR(config_debug_bus, DEBUG_BUS_CTL);
CORESIGHT_CREATE_REG_ATTR(config_trace_stop_cnt, TRACE_STOP_CNT);
CORESIGHT_CREATE_REG_ATTR(config_trace_start_cnt, TRACE_START_CNT);
CORESIGHT_CREATE_REG_ATTR(config_trace_period_cnt, TRACE_PERIOD_CNT);
CORESIGHT_CREATE_REG_ATTR(config_trace_cmd, TRACE_CMD);
CORESIGHT_CREATE_REG_ATTR(config_trace_bus_ctl, TRACE_BUS_CTL);

static struct attribute *gfx_attrs[] = {
	&coresight_attr_config_debug_bus.attr.attr,
	&coresight_attr_config_trace_start_cnt.attr.attr,
	&coresight_attr_config_trace_stop_cnt.attr.attr,
	&coresight_attr_config_trace_period_cnt.attr.attr,
	&coresight_attr_config_trace_cmd.attr.attr,
	&coresight_attr_config_trace_bus_ctl.attr.attr,
	NULL,
};

static struct attribute_group gfx_attr_grp = {
	.attrs = gfx_attrs,
};

static const struct attribute_group *gfx_attr_grps[] = {
	&gfx_attr_grp,
	NULL,
};

int adreno_coresight_init(struct platform_device *pdev)
{
	int ret = 0;
	struct kgsl_device_platform_data *pdata = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct coresight_desc *desc;

	if (IS_ERR_OR_NULL(pdata->coresight_pdata))
		return -ENODATA;


	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;


	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_BUS;
	desc->ops = &adreno_coresight_cs_ops;
	desc->pdata = pdata->coresight_pdata;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->groups = gfx_attr_grps;
	pdata->csdev = coresight_register(desc);
	if (IS_ERR(pdata->csdev)) {
		ret = PTR_ERR(pdata->csdev);
		goto err;
	}

	return 0;

err:
	devm_kfree(dev, desc);
	return ret;
}


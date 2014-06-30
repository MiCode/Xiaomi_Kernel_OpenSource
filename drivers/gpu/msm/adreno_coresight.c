/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>

#include "adreno.h"

#define TO_ADRENO_CORESIGHT_ATTR(_attr) \
	container_of(_attr, struct adreno_coresight_attr, attr)

ssize_t adreno_coresight_show_register(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int val = 0;
	struct kgsl_device *device = dev_get_drvdata(dev->parent);
	struct adreno_device *adreno_dev;
	struct adreno_coresight_attr *cattr = TO_ADRENO_CORESIGHT_ATTR(attr);

	if (device == NULL)
		return -EINVAL;

	adreno_dev = ADRENO_DEVICE(device);

	if (cattr->reg == NULL)
		return -EINVAL;

	/*
	 * Return the current value of the register if coresight is enabled,
	 * otherwise report 0
	 */

	mutex_lock(&device->mutex);
	if (test_bit(ADRENO_DEVICE_CORESIGHT, &adreno_dev->priv)) {

		/*
		 * If the device isn't power collapsed read the actual value
		 * from the hardware - otherwise return the cached value
		 */

		if (device->state == KGSL_STATE_ACTIVE ||
			device->state == KGSL_STATE_NAP) {
			if (!kgsl_active_count_get(device)) {
				kgsl_regread(device, cattr->reg->offset,
					&cattr->reg->value);
				kgsl_active_count_put(device);
			}
		}

		val = cattr->reg->value;
	}
	mutex_unlock(&device->mutex);

	return snprintf(buf, PAGE_SIZE, "0x%X", val);
}

ssize_t adreno_coresight_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct kgsl_device *device = dev_get_drvdata(dev->parent);
	struct adreno_device *adreno_dev;
	struct adreno_coresight_attr *cattr = TO_ADRENO_CORESIGHT_ATTR(attr);
	unsigned long val;
	int ret;

	if (device == NULL)
		return -EINVAL;

	adreno_dev = ADRENO_DEVICE(device);

	if (cattr->reg == NULL)
		return -EINVAL;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);

	/* Ignore writes while coresight is off */
	if (!test_bit(ADRENO_DEVICE_CORESIGHT, &adreno_dev->priv))
		goto out;

	cattr->reg->value = val;

	/* Program the hardware if it is not power collapsed */
	if (device->state == KGSL_STATE_ACTIVE ||
		device->state == KGSL_STATE_NAP) {
		if (!kgsl_active_count_get(device)) {
			kgsl_regwrite(device, cattr->reg->offset,
					cattr->reg->value);
			kgsl_active_count_put(device);
		}
	}

out:
	mutex_unlock(&device->mutex);
	return size;
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
static void adreno_coresight_disable(struct coresight_device *csdev)
{
	struct kgsl_device *device = dev_get_drvdata(csdev->dev.parent);
	struct adreno_device *adreno_dev;
	struct adreno_gpudev *gpudev;
	struct adreno_coresight *coresight;
	int i;

	if (device == NULL)
		return;

	adreno_dev = ADRENO_DEVICE(device);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	coresight = gpudev->coresight;

	if (coresight == NULL)
		return;

	mutex_lock(&device->mutex);

	if (!kgsl_active_count_get(device)) {
		for (i = 0; i < coresight->count; i++)
			kgsl_regwrite(device, coresight->registers[i].offset,
				0);

		kgsl_active_count_put(device);
	}

	clear_bit(ADRENO_DEVICE_CORESIGHT, &adreno_dev->priv);

	mutex_unlock(&device->mutex);
}

static int _adreno_coresight_get(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_coresight *coresight = gpudev->coresight;
	int i;

	if (coresight == NULL)
		return -ENODEV;

	if (!kgsl_active_count_get(device)) {
		for (i = 0; i < coresight->count; i++)
			kgsl_regread(device, coresight->registers[i].offset,
				&coresight->registers[i].value);

		kgsl_active_count_put(device);
	}

	return 0;
}

static int _adreno_coresight_set(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_coresight *coresight = gpudev->coresight;
	int i;

	if (coresight == NULL)
		return -ENODEV;

	if (!kgsl_active_count_get(device)) {
		for (i = 0; i < coresight->count; i++)
			kgsl_regwrite(device, coresight->registers[i].offset,
				coresight->registers[i].value);

		kgsl_active_count_put(device);
	}

	return 0;
}
/**
 * adreno_coresight_enable() - Generic function to enable coresight debugging
 * @csdev: Pointer to coresight's device struct
 *
 * This is a generic function to enable coresight debug bus on adreno
 * devices. This should be used in all cases of enabling
 * coresight debug bus for adreno devices. This function is registered as the
 * coresight enable function with coresight driver. It should only be called
 * through coresight driver as that would ensure that the necessary setup
 * required to be done on coresight driver's part is also done.
 */
static int adreno_coresight_enable(struct coresight_device *csdev)
{
	struct kgsl_device *device = dev_get_drvdata(csdev->dev.parent);
	struct adreno_device *adreno_dev;
	struct adreno_gpudev *gpudev;
	struct adreno_coresight *coresight;
	int ret = 0;

	if (device == NULL)
		return -ENODEV;

	adreno_dev = ADRENO_DEVICE(device);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	coresight = gpudev->coresight;

	if (coresight == NULL)
		return -ENODEV;

	mutex_lock(&device->mutex);
	if (!test_and_set_bit(ADRENO_DEVICE_CORESIGHT, &adreno_dev->priv)) {
		int i;

		/* Reset all the debug registers to their default values */

		for (i = 0; i < coresight->count; i++)
			coresight->registers[i].value =
				coresight->registers[i].initial;

		ret = _adreno_coresight_set(adreno_dev);
	}

	mutex_unlock(&device->mutex);

	return ret;
}

/**
 * adreno_coresight_start() - Reprogram coresight registers after power collapse
 * @adreno_dev: Pointer to the adreno device structure
 *
 * Cache the current coresight register values so they can be restored after
 * power collapse
 */
void adreno_coresight_stop(struct adreno_device *adreno_dev)
{
	if (test_bit(ADRENO_DEVICE_CORESIGHT, &adreno_dev->priv))
		_adreno_coresight_get(adreno_dev);
}

/**
 * adreno_coresight_start() - Reprogram coresight registers after power collapse
 * @adreno_dev: Pointer to the adreno device structure
 *
 * Reprogram the cached values to the coresight registers on power up
 */
void adreno_coresight_start(struct adreno_device *adreno_dev)
{
	if (test_bit(ADRENO_DEVICE_CORESIGHT, &adreno_dev->priv))
		_adreno_coresight_set(adreno_dev);
}

static const struct coresight_ops_source adreno_coresight_source_ops = {
	.enable = adreno_coresight_enable,
	.disable = adreno_coresight_disable,
};

static const struct coresight_ops adreno_coresight_ops = {
	.source_ops = &adreno_coresight_source_ops,
};

void adreno_coresight_remove(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_device_platform_data *pdata =
		dev_get_platdata(&device->pdev->dev);

	coresight_unregister(pdata->csdev);
	pdata->csdev = NULL;
}

int adreno_coresight_init(struct adreno_device *adreno_dev)
{
	int ret = 0;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_device_platform_data *pdata =
		dev_get_platdata(&device->pdev->dev);
	struct coresight_desc desc;

	if (pdata == NULL)
		return -ENODEV;

	if (gpudev->coresight == NULL)
		return -ENODEV;

	if (IS_ERR_OR_NULL(pdata->coresight_pdata))
		return -ENODEV;

	if (pdata->csdev != NULL)
		return 0;

	memset(&desc, 0, sizeof(desc));

	desc.type = CORESIGHT_DEV_TYPE_SOURCE;
	desc.subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_BUS;
	desc.ops = &adreno_coresight_ops;
	desc.pdata = pdata->coresight_pdata;
	desc.dev = &device->pdev->dev;
	desc.owner = THIS_MODULE;
	desc.groups = gpudev->coresight->groups;

	pdata->csdev = coresight_register(&desc);

	if (IS_ERR(pdata->csdev))
		ret = PTR_ERR(pdata->csdev);

	return ret;
}

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/coresight.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "adreno.h"

enum gpu_coresight_sources {
	GPU_CORESIGHT_GX = 0,
	GPU_CORESIGHT_CX = 1,
};

#define TO_ADRENO_CORESIGHT_ATTR(_attr) \
	container_of(_attr, struct adreno_coresight_attr, attr)

static int adreno_coresight_identify(const char *name)
{
	if (!strcmp(name, "coresight-gfx"))
		return GPU_CORESIGHT_GX;
	else if (!strcmp(name, "coresight-gfx-cx"))
		return GPU_CORESIGHT_CX;
	else
		return -EINVAL;
}

ssize_t adreno_coresight_show_register(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int val = 0;
	struct kgsl_device *device = dev_get_drvdata(dev->parent);
	struct adreno_device *adreno_dev;
	struct adreno_coresight_attr *cattr = TO_ADRENO_CORESIGHT_ATTR(attr);
	bool is_cx;

	if (device == NULL)
		return -EINVAL;

	adreno_dev = ADRENO_DEVICE(device);

	if (cattr->reg == NULL)
		return -EINVAL;

	is_cx = adreno_is_cx_dbgc_register(device, cattr->reg->offset);
	/*
	 * Return the current value of the register if coresight is enabled,
	 * otherwise report 0
	 */

	mutex_lock(&device->mutex);
	if ((is_cx && test_bit(ADRENO_DEVICE_CORESIGHT_CX, &adreno_dev->priv))
		|| (!is_cx && test_bit(ADRENO_DEVICE_CORESIGHT,
			&adreno_dev->priv))) {
		/*
		 * If the device isn't power collapsed read the actual value
		 * from the hardware - otherwise return the cached value
		 */

		if (device->state == KGSL_STATE_ACTIVE ||
				kgsl_state_is_nap_or_minbw(device)) {
			if (!adreno_active_count_get(adreno_dev)) {
				if (!is_cx)
					kgsl_regread(device, cattr->reg->offset,
						&cattr->reg->value);
				else
					adreno_cx_dbgc_regread(device,
						cattr->reg->offset,
						&cattr->reg->value);
				adreno_active_count_put(adreno_dev);
			}
		}

		val = cattr->reg->value;
	}
	mutex_unlock(&device->mutex);

	return scnprintf(buf, PAGE_SIZE, "0x%X\n", val);
}

ssize_t adreno_coresight_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct kgsl_device *device = dev_get_drvdata(dev->parent);
	struct adreno_device *adreno_dev;
	struct adreno_coresight_attr *cattr = TO_ADRENO_CORESIGHT_ATTR(attr);
	unsigned long val;
	int ret, is_cx;

	if (device == NULL)
		return -EINVAL;

	adreno_dev = ADRENO_DEVICE(device);

	if (cattr->reg == NULL)
		return -EINVAL;

	is_cx = adreno_is_cx_dbgc_register(device, cattr->reg->offset);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);

	/* Ignore writes while coresight is off */
	if (!((is_cx && test_bit(ADRENO_DEVICE_CORESIGHT_CX, &adreno_dev->priv))
		|| (!is_cx && test_bit(ADRENO_DEVICE_CORESIGHT,
		&adreno_dev->priv))))
		goto out;

	cattr->reg->value = val;

	/* Program the hardware if it is not power collapsed */
	if (device->state == KGSL_STATE_ACTIVE ||
			kgsl_state_is_nap_or_minbw(device)) {
		if (!adreno_active_count_get(adreno_dev)) {
			if (!is_cx)
				kgsl_regwrite(device, cattr->reg->offset,
					cattr->reg->value);
			else
				adreno_cx_dbgc_regwrite(device,
					cattr->reg->offset,
					cattr->reg->value);

			adreno_active_count_put(adreno_dev);
		}
	}

out:
	mutex_unlock(&device->mutex);
	return size;
}

/*
 * This is a generic function to disable coresight debug bus on Adreno
 * devices. This function in turn calls the device specific function
 * through the gpudev hook.
 */
static void adreno_coresight_disable(struct coresight_device *csdev,
					struct perf_event *event)
{
	struct kgsl_device *device = dev_get_drvdata(csdev->dev.parent);
	struct adreno_device *adreno_dev;
	const struct adreno_gpudev *gpudev;
	struct adreno_coresight *coresight;
	int i, cs_id;

	if (device == NULL)
		return;

	adreno_dev = ADRENO_DEVICE(device);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	cs_id = adreno_coresight_identify(dev_name(&csdev->dev));

	if (cs_id < 0)
		return;

	coresight = gpudev->coresight[cs_id];

	if (coresight == NULL)
		return;

	mutex_lock(&device->mutex);

	if (!adreno_active_count_get(adreno_dev)) {
		if (cs_id == GPU_CORESIGHT_GX)
			for (i = 0; i < coresight->count; i++)
				kgsl_regwrite(device,
					coresight->registers[i].offset, 0);
		else if (cs_id == GPU_CORESIGHT_CX)
			for (i = 0; i < coresight->count; i++)
				adreno_cx_dbgc_regwrite(device,
					coresight->registers[i].offset, 0);

		adreno_active_count_put(adreno_dev);
	}

	if (cs_id == GPU_CORESIGHT_GX)
		clear_bit(ADRENO_DEVICE_CORESIGHT, &adreno_dev->priv);
	else if (cs_id == GPU_CORESIGHT_CX)
		clear_bit(ADRENO_DEVICE_CORESIGHT_CX, &adreno_dev->priv);

	mutex_unlock(&device->mutex);
}

static int _adreno_coresight_get_and_clear(struct adreno_device *adreno_dev,
						int cs_id)
{
	int i;
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_coresight *coresight = gpudev->coresight[cs_id];

	if (coresight == NULL)
		return -ENODEV;

	kgsl_pre_hwaccess(device);
	/*
	 * Save the current value of each coresight register
	 * and then clear each register
	 */
	if (cs_id == GPU_CORESIGHT_GX) {
		for (i = 0; i < coresight->count; i++) {
			kgsl_regread(device, coresight->registers[i].offset,
				&coresight->registers[i].value);
			kgsl_regwrite(device, coresight->registers[i].offset,
				0);
		}
	} else if (cs_id == GPU_CORESIGHT_CX) {
		for (i = 0; i < coresight->count; i++) {
			adreno_cx_dbgc_regread(device,
				coresight->registers[i].offset,
				&coresight->registers[i].value);
			adreno_cx_dbgc_regwrite(device,
				coresight->registers[i].offset, 0);
		}
	}

	return 0;
}

static int _adreno_coresight_set(struct adreno_device *adreno_dev, int cs_id)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_coresight *coresight = gpudev->coresight[cs_id];
	int i;

	if (coresight == NULL)
		return -ENODEV;

	if (cs_id == GPU_CORESIGHT_GX) {
		for (i = 0; i < coresight->count; i++)
			kgsl_regwrite(device, coresight->registers[i].offset,
				coresight->registers[i].value);
	} else if (cs_id == GPU_CORESIGHT_CX) {
		for (i = 0; i < coresight->count; i++)
			adreno_cx_dbgc_regwrite(device,
				coresight->registers[i].offset,
				coresight->registers[i].value);
	}
	return 0;
}
/* Generic function to enable coresight debug bus on adreno devices */
static int adreno_coresight_enable(struct coresight_device *csdev,
				struct perf_event *event, u32 mode)
{
	struct kgsl_device *device = dev_get_drvdata(csdev->dev.parent);
	struct adreno_device *adreno_dev;
	const struct adreno_gpudev *gpudev;
	struct adreno_coresight *coresight;
	int ret = 0, adreno_dev_flag = -EINVAL, cs_id;

	if (device == NULL)
		return -ENODEV;

	adreno_dev = ADRENO_DEVICE(device);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	cs_id = adreno_coresight_identify(dev_name(&csdev->dev));

	if (cs_id < 0)
		return -ENODEV;

	coresight = gpudev->coresight[cs_id];

	if (coresight == NULL)
		return -ENODEV;

	if (cs_id == GPU_CORESIGHT_GX)
		adreno_dev_flag = ADRENO_DEVICE_CORESIGHT;
	else if (cs_id == GPU_CORESIGHT_CX)
		adreno_dev_flag = ADRENO_DEVICE_CORESIGHT_CX;
	else
		return -ENODEV;

	mutex_lock(&device->mutex);
	if (!test_and_set_bit(adreno_dev_flag, &adreno_dev->priv)) {
		int i;

		/* Reset all the debug registers to their default values */

		for (i = 0; i < coresight->count; i++)
			coresight->registers[i].value =
				coresight->registers[i].initial;

		if (kgsl_state_is_awake(device)) {
			ret = adreno_active_count_get(adreno_dev);
			if (!ret) {
				ret = _adreno_coresight_set(adreno_dev, cs_id);
				adreno_active_count_put(adreno_dev);
			}
		}
	}

	mutex_unlock(&device->mutex);

	return ret;
}

void adreno_coresight_stop(struct adreno_device *adreno_dev)
{
	if (test_bit(ADRENO_DEVICE_CORESIGHT, &adreno_dev->priv))
		_adreno_coresight_get_and_clear(adreno_dev, GPU_CORESIGHT_GX);

	if (test_bit(ADRENO_DEVICE_CORESIGHT_CX, &adreno_dev->priv))
		_adreno_coresight_get_and_clear(adreno_dev, GPU_CORESIGHT_CX);
}

void adreno_coresight_start(struct adreno_device *adreno_dev)
{
	if (test_bit(ADRENO_DEVICE_CORESIGHT, &adreno_dev->priv))
		_adreno_coresight_set(adreno_dev, GPU_CORESIGHT_GX);

	if (test_bit(ADRENO_DEVICE_CORESIGHT_CX, &adreno_dev->priv))
		_adreno_coresight_set(adreno_dev, GPU_CORESIGHT_CX);
}

static int adreno_coresight_trace_id(struct coresight_device *csdev)
{
	struct kgsl_device *device = dev_get_drvdata(csdev->dev.parent);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(ADRENO_DEVICE(device));
	int cs_id;

	cs_id = adreno_coresight_identify(dev_name(&csdev->dev));

	if (cs_id < 0)
		return -ENODEV;

	return gpudev->coresight[cs_id]->atid;
}

static const struct coresight_ops_source adreno_coresight_source_ops = {
	.trace_id = adreno_coresight_trace_id,
	.enable = adreno_coresight_enable,
	.disable = adreno_coresight_disable,
};

static const struct coresight_ops adreno_coresight_ops = {
	.source_ops = &adreno_coresight_source_ops,
};

void adreno_coresight_remove(struct adreno_device *adreno_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(adreno_dev->csdev); i++)
		if (!IS_ERR_OR_NULL(adreno_dev->csdev[i]))
			coresight_unregister(adreno_dev->csdev[i]);
}

static struct coresight_device *
adreno_coresight_dev_probe(struct kgsl_device *device,
		struct adreno_coresight *coresight, struct device_node *node)
{
	struct platform_device *pdev = of_find_device_by_node(node);
	struct coresight_desc desc;
	u32 atid;

	if (!pdev)
		return ERR_PTR(-ENODEV);

	if (of_property_read_u32(node, "coresight-atid", &atid))
		return ERR_PTR(-ENODEV);

	desc.pdata = coresight_get_platform_data(&pdev->dev);
	platform_device_put(pdev);

	if (IS_ERR(desc.pdata))
		return ERR_CAST(desc.pdata);

	desc.type = CORESIGHT_DEV_TYPE_SOURCE;
	desc.subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE;
	desc.ops = &adreno_coresight_ops;
	desc.dev = &device->pdev->dev;
	desc.groups = coresight->groups;

	coresight->atid = atid;

	return coresight_register(&desc);
}

void adreno_coresight_init(struct adreno_device *adreno_dev)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int i = 0;
	struct device_node *node, *child;

	node = of_find_compatible_node(device->pdev->dev.of_node,
					NULL, "qcom,gpu-coresight");
	if (!node)
		return;

	for_each_child_of_node(node, child) {
		adreno_dev->csdev[i] = adreno_coresight_dev_probe(device,
			gpudev->coresight[i], child);

		i++;
	}

	of_node_put(node);
}

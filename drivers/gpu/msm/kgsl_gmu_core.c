// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>

#include "adreno.h"
#include "kgsl_device.h"
#include "kgsl_gmu_core.h"
#include "kgsl_trace.h"

static const struct of_device_id gmu_match_table[] = {
	{ .compatible = "qcom,gpu-gmu", .data = &a6xx_gmu_driver },
	{ .compatible = "qcom,gpu-rgmu", .data = &a6xx_rgmu_driver },
	{},
};

void __init gmu_core_register(void)
{
	const struct of_device_id *match;
	struct device_node *node;

	node = of_find_matching_node_and_match(NULL, gmu_match_table,
		&match);
	if (!node)
		return;

	platform_driver_register((struct platform_driver *) match->data);
	of_node_put(node);
}

void __exit gmu_core_unregister(void)
{
	const struct of_device_id *match;
	struct device_node *node;

	node = of_find_matching_node_and_match(NULL, gmu_match_table,
		&match);
	if (!node)
		return;

	platform_driver_unregister((struct platform_driver *) match->data);
	of_node_put(node);
}

bool gmu_core_isenabled(struct kgsl_device *device)
{
	return test_bit(GMU_ENABLED, &device->gmu_core.flags);
}

bool gmu_core_gpmu_isenabled(struct kgsl_device *device)
{
	return (device->gmu_core.dev_ops != NULL);
}

bool gmu_core_scales_bandwidth(struct kgsl_device *device)
{
	const struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->scales_bandwidth)
		return ops->scales_bandwidth(device);

	return false;
}

int gmu_core_dev_acd_set(struct kgsl_device *device, bool val)
{
	const struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->acd_set)
		return ops->acd_set(device, val);

	return -EINVAL;
}

bool gmu_core_is_register_offset(struct kgsl_device *device,
				unsigned int offsetwords)
{
	return (gmu_core_isenabled(device) &&
		(offsetwords >= device->gmu_core.gmu2gpu_offset) &&
		((offsetwords - device->gmu_core.gmu2gpu_offset) *
			sizeof(uint32_t) < device->gmu_core.reg_len));
}

void gmu_core_regread(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int *value)
{
	void __iomem *reg;

	if (WARN(!gmu_core_is_register_offset(device, offsetwords),
			"Out of bounds register read: 0x%x\n", offsetwords))
		return;

	offsetwords -= device->gmu_core.gmu2gpu_offset;

	reg = device->gmu_core.reg_virt + (offsetwords << 2);

	*value = __raw_readl(reg);

	/*
	 * ensure this read finishes before the next one.
	 * i.e. act like normal readl()
	 */
	rmb();
}

void gmu_core_regwrite(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int value)
{
	void __iomem *reg;

	if (WARN(!gmu_core_is_register_offset(device, offsetwords),
			"Out of bounds register write: 0x%x\n", offsetwords))
		return;

	trace_kgsl_regwrite(device, offsetwords, value);

	offsetwords -= device->gmu_core.gmu2gpu_offset;
	reg = device->gmu_core.reg_virt + (offsetwords << 2);

	/*
	 * ensure previous writes post before this one,
	 * i.e. act like normal writel()
	 */
	wmb();
	__raw_writel(value, reg);
}

void gmu_core_blkwrite(struct kgsl_device *device, unsigned int offsetwords,
		const void *buffer, size_t size)
{
	void __iomem *base;

	if (WARN_ON(!gmu_core_is_register_offset(device, offsetwords)))
		return;

	offsetwords -= device->gmu_core.gmu2gpu_offset;
	base = device->gmu_core.reg_virt + (offsetwords << 2);

	memcpy_toio(base, buffer, size);
}

void gmu_core_regrmw(struct kgsl_device *device,
		unsigned int offsetwords,
		unsigned int mask, unsigned int bits)
{
	unsigned int val = 0;

	if (WARN(!gmu_core_is_register_offset(device, offsetwords),
			"Out of bounds register rmw: 0x%x\n", offsetwords))
		return;

	gmu_core_regread(device, offsetwords, &val);
	val &= ~mask;
	gmu_core_regwrite(device, offsetwords, val | bits);
}

int gmu_core_dev_oob_set(struct kgsl_device *device, enum oob_request req)
{
	const struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->oob_set)
		return ops->oob_set(device, req);

	return 0;
}

void gmu_core_dev_oob_clear(struct kgsl_device *device, enum oob_request req)
{
	const struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->oob_clear)
		ops->oob_clear(device, req);
}

void gmu_core_dev_cooperative_reset(struct kgsl_device *device)
{

	const struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->cooperative_reset)
		ops->cooperative_reset(device);
}

bool gmu_core_dev_gx_is_on(struct kgsl_device *device)
{
	const struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->gx_is_on)
		return ops->gx_is_on(device);

	return true;
}

int gmu_core_dev_ifpc_show(struct kgsl_device *device)
{
	const struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->ifpc_show)
		return ops->ifpc_show(device);

	return 0;
}

int gmu_core_dev_ifpc_store(struct kgsl_device *device, unsigned int val)
{
	const struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->ifpc_store)
		return ops->ifpc_store(device, val);

	return -EINVAL;
}

int gmu_core_dev_wait_for_active_transition(struct kgsl_device *device)
{
	const struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->wait_for_active_transition)
		return ops->wait_for_active_transition(device);

	return 0;
}

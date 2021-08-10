// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/iopoll.h>

#include "adreno.h"
#include "kgsl_device.h"
#include "kgsl_gmu_core.h"
#include "kgsl_trace.h"

static const struct of_device_id gmu_match_table[] = {
	{ .compatible = "qcom,gpu-gmu", .data = &a6xx_gmu_driver },
	{ .compatible = "qcom,gpu-rgmu", .data = &a6xx_rgmu_driver },
	{ .compatible = "qcom,gen7-gmu", .data = &gen7_gmu_driver },
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

void gmu_core_regread(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int *value)
{
	u32 val = kgsl_regmap_read(&device->regmap, offsetwords);
	*value  = val;
}

void gmu_core_regwrite(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int value)
{
	kgsl_regmap_write(&device->regmap, value, offsetwords);
}

void gmu_core_blkwrite(struct kgsl_device *device, unsigned int offsetwords,
		const void *buffer, size_t size)
{
	kgsl_regmap_bulk_write(&device->regmap, offsetwords,
		buffer, size >> 2);
}

void gmu_core_regrmw(struct kgsl_device *device,
		unsigned int offsetwords,
		unsigned int mask, unsigned int bits)
{
	kgsl_regmap_rmw(&device->regmap, offsetwords, mask, bits);
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

void gmu_core_fault_snapshot(struct kgsl_device *device)
{
	device->gmu_fault = true;
	kgsl_device_snapshot(device, NULL, true);
}

int gmu_core_timed_poll_check(struct kgsl_device *device,
		unsigned int offset, unsigned int expected_ret,
		unsigned int timeout_ms, unsigned int mask)
{
	u32 val;

	return kgsl_regmap_read_poll_timeout(&device->regmap, offset,
		val, (val & mask) == expected_ret, 100, timeout_ms * 1000);
}

int gmu_core_map_memdesc(struct iommu_domain *domain, struct kgsl_memdesc *memdesc,
		u64 gmuaddr, int attrs)
{
	size_t mapped;

	if (!memdesc->pages) {
		mapped = iommu_map_sg(domain, gmuaddr, memdesc->sgt->sgl,
			memdesc->sgt->nents, attrs);
	} else {
		struct sg_table sgt = { 0 };
		int ret;

		ret = sg_alloc_table_from_pages(&sgt, memdesc->pages,
			memdesc->page_count, 0, memdesc->size, GFP_KERNEL);

		if (ret)
			return ret;

		mapped = iommu_map_sg(domain, gmuaddr, sgt.sgl, sgt.nents, attrs);
		sg_free_table(&sgt);
	}

	return mapped == 0 ? -ENOMEM : 0;
}

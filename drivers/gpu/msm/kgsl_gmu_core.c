/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/types.h>

#include "kgsl_device.h"
#include "kgsl_gmu_core.h"
#include "kgsl_trace.h"
#include "adreno.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl_gmu."

static bool nogmu;
module_param(nogmu, bool, 0444);
MODULE_PARM_DESC(nogmu, "Disable the GMU");

static const struct {
	char *compat;
	struct gmu_core_ops *core_ops;
	enum gmu_coretype type;
} gmu_subtypes[] = {
		{"qcom,gpu-gmu", &gmu_ops, GMU_CORE_TYPE_CM3},
		{"qcom,gpu-rgmu", &rgmu_ops, GMU_CORE_TYPE_PCC},
};

struct oob_entry {
	enum oob_request req;
	const char *str;
};

const char *gmu_core_oob_type_str(enum oob_request req)
{
	int i;
	struct oob_entry table[] =  {
			{ oob_gpu, "oob_gpu"},
			{ oob_perfcntr, "oob_perfcntr"},
			{ oob_boot_slumber, "oob_boot_slumber"},
			{ oob_dcvs, "oob_dcvs"},
	};

	for (i = 0; i < ARRAY_SIZE(table); i++)
		if (req == table[i].req)
			return table[i].str;
	return "UNKNOWN";
}

int gmu_core_probe(struct kgsl_device *device)
{
	struct device_node *node;
	struct gmu_core_ops *gmu_core_ops;
	int i = 0, ret = -ENXIO;

	device->gmu_core.flags = ADRENO_FEATURE(ADRENO_DEVICE(device),
			ADRENO_GPMU) ? BIT(GMU_GPMU) : 0;

	for (i = 0; i < ARRAY_SIZE(gmu_subtypes); i++) {
		node = of_find_compatible_node(device->pdev->dev.of_node,
				NULL, gmu_subtypes[i].compat);

		if (node != NULL) {
			gmu_core_ops = gmu_subtypes[i].core_ops;
			device->gmu_core.type = gmu_subtypes[i].type;
			break;
		}
	}

	/* No GMU in dt, no worries...hopefully */
	if (node == NULL) {
		/* If we are trying to use GPMU and no GMU, that's bad */
		if (device->gmu_core.flags & BIT(GMU_GPMU))
			return ret;
		/* Otherwise it's ok and nothing to do */
		return 0;
	}

	if (gmu_core_ops && gmu_core_ops->probe) {
		ret = gmu_core_ops->probe(device, node);
		if (ret == 0)
			device->gmu_core.core_ops = gmu_core_ops;
	}

	return ret;
}

void gmu_core_remove(struct kgsl_device *device)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->remove)
		gmu_core_ops->remove(device);
}

bool gmu_core_isenabled(struct kgsl_device *device)
{
	return test_bit(GMU_ENABLED, &device->gmu_core.flags);
}

bool gmu_core_gpmu_isenabled(struct kgsl_device *device)
{
	return test_bit(GMU_GPMU, &device->gmu_core.flags) && !nogmu;
}

bool gmu_core_scales_bandwidth(struct kgsl_device *device)
{
	if (device->gmu_core.type == GMU_CORE_TYPE_PCC)
		return false;
	else
		return gmu_core_gpmu_isenabled(device) &&
		(adreno_is_a640(ADRENO_DEVICE(device)) ||
			adreno_is_a680(ADRENO_DEVICE(device)));
}

int gmu_core_start(struct kgsl_device *device)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->start)
		return gmu_core_ops->start(device);

	return -EINVAL;
}

void gmu_core_stop(struct kgsl_device *device)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->stop)
		gmu_core_ops->stop(device);
}

int gmu_core_suspend(struct kgsl_device *device)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->suspend)
		return gmu_core_ops->suspend(device);

	return -EINVAL;
}

void gmu_core_snapshot(struct kgsl_device *device)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->snapshot)
		gmu_core_ops->snapshot(device);
}

int gmu_core_dcvs_set(struct kgsl_device *device, unsigned int gpu_pwrlevel,
		unsigned int bus_level)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->dcvs_set)
		return gmu_core_ops->dcvs_set(device, gpu_pwrlevel, bus_level);

	return -EINVAL;
}

int gmu_core_acd_set(struct kgsl_device *device, unsigned int val)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->acd_set)
		return gmu_core_ops->acd_set(device, val);

	return -EINVAL;
}

bool gmu_core_regulator_isenabled(struct kgsl_device *device)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->regulator_isenabled)
		return gmu_core_ops->regulator_isenabled(device);

	return false;
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

	if (!gmu_core_is_register_offset(device, offsetwords)) {
		WARN(1, "Out of bounds register read: 0x%x\n", offsetwords);
		return;
	}

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

	if (!gmu_core_is_register_offset(device, offsetwords)) {
		WARN(1, "Out of bounds register write: 0x%x\n", offsetwords);
		return;
	}

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

	if (!gmu_core_is_register_offset(device, offsetwords)) {
		WARN(1, "Out of bounds register write: 0x%x\n", offsetwords);
		return;
	}

	offsetwords -= device->gmu_core.gmu2gpu_offset;
	base = device->gmu_core.reg_virt + (offsetwords << 2);

	memcpy_toio(base, buffer, size);
}

void gmu_core_regrmw(struct kgsl_device *device,
		unsigned int offsetwords,
		unsigned int mask, unsigned int bits)
{
	unsigned int val = 0;

	if (!gmu_core_is_register_offset(device, offsetwords)) {
		WARN(1, "Out of bounds register rmw: 0x%x\n", offsetwords);
		return;
	}

	gmu_core_regread(device, offsetwords, &val);
	val &= ~mask;
	gmu_core_regwrite(device, offsetwords, val | bits);
}

bool gmu_core_dev_cx_is_on(struct kgsl_device *device)
{
	struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->cx_is_on)
		return ops->cx_is_on(device);

	return true;
}

bool gmu_core_is_initialized(struct kgsl_device *device)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->is_initialized)
		return gmu_core_ops->is_initialized(device);

	return false;
}

u64 gmu_core_dev_read_ao_counter(struct kgsl_device *device)
{
	struct gmu_dev_ops *ops = GMU_DEVICE_OPS(device);

	if (ops && ops->read_ao_counter)
		return ops->read_ao_counter(device);

	return 0;
}

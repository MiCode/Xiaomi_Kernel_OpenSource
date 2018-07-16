/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include "a6xx_reg.h"
#include "adreno.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl_gmu."

static bool nogmu;
module_param(nogmu, bool, 0444);
MODULE_PARM_DESC(nogmu, "Disable the GMU");

static const struct {
	char *compat;
	struct gmu_core_ops *core_ops;
} gmu_subtypes[] = {
		{"qcom,gpu-gmu", &gmu_ops},
};

int gmu_core_probe(struct kgsl_device *device)
{
	struct device_node *node;
	struct gmu_core_ops *gmu_core_ops;
	unsigned long flags;
	int i = 0, ret = -ENXIO;

	flags = ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_GPMU) ?
			BIT(GMU_GPMU) : 0;

	for (i = 0; i < ARRAY_SIZE(gmu_subtypes); i++) {
		node = of_find_compatible_node(device->pdev->dev.of_node,
				NULL, gmu_subtypes[i].compat);

		if (node != NULL)
			gmu_core_ops = gmu_subtypes[i].core_ops;
	}

	/* No GMU in dt, no worries...hopefully */
	if (node == NULL) {
		/* If we are trying to use GPMU and no GMU, that's bad */
		if (flags & BIT(GMU_GPMU))
			return ret;
		/* Otherwise it's ok and nothing to do */
		return 0;
	}

	if (gmu_core_ops && gmu_core_ops->probe) {
		ret = gmu_core_ops->probe(device, node, flags);
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
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->isenabled)
		return !nogmu && gmu_core_ops->isenabled(device);

	return false;
}

bool gmu_core_gpmu_isenabled(struct kgsl_device *device)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->gpmu_isenabled)
		return gmu_core_ops->gpmu_isenabled(device);

	return false;
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

void gmu_core_setbit(struct kgsl_device *device, enum gmu_core_flags flag)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->set_bit)
		return gmu_core_ops->set_bit(device, flag);
}

void gmu_core_clearbit(struct kgsl_device *device, enum gmu_core_flags flag)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->clear_bit)
		return gmu_core_ops->clear_bit(device, flag);
}

int gmu_core_testbit(struct kgsl_device *device, enum gmu_core_flags flag)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (gmu_core_ops && gmu_core_ops->test_bit)
		return gmu_core_ops->test_bit(device, flag);

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
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (!gmu_core_is_register_offset(device, offsetwords)) {
		WARN(1, "Out of bounds register read: 0x%x\n", offsetwords);
		return;
	}

	if (gmu_core_ops && gmu_core_ops->regread)
		gmu_core_ops->regread(device, offsetwords, value);
	else
		*value = 0;
}

void gmu_core_regwrite(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int value)
{
	struct gmu_core_ops *gmu_core_ops = GMU_CORE_OPS(device);

	if (!gmu_core_is_register_offset(device, offsetwords)) {
		WARN(1, "Out of bounds register write: 0x%x\n", offsetwords);
		return;
	}

	if (gmu_core_ops && gmu_core_ops->regwrite)
		gmu_core_ops->regwrite(device, offsetwords, value);
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

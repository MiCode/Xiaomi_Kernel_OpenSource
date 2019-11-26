/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/reset-controller.h>

#include "reset.h"

static int msm_reset(struct reset_controller_dev *rcdev, unsigned long id)
{
	rcdev->ops->assert(rcdev, id);
	udelay(1);
	rcdev->ops->deassert(rcdev, id);
	return 0;
}

static int
msm_reset_assert(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct msm_reset_controller *rst;
	const struct msm_reset_map *map;
	u32 regval;

	rst = to_msm_reset_controller(rcdev);
	map = &rst->reset_map[id];

	regval = readl_relaxed(rst->base + map->reg);
	regval |= BIT(map->bit);
	writel_relaxed(regval, rst->base + map->reg);

	/* Make sure the reset is asserted */
	mb();

	return 0;
}

static int
msm_reset_deassert(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct msm_reset_controller *rst;
	const struct msm_reset_map *map;
	u32 regval;

	rst = to_msm_reset_controller(rcdev);
	map = &rst->reset_map[id];

	regval = readl_relaxed(rst->base + map->reg);
	regval &= ~BIT(map->bit);
	writel_relaxed(regval, rst->base + map->reg);

	/* Make sure the reset is de-asserted */
	mb();

	return 0;
}

struct reset_control_ops msm_reset_ops = {
	.reset = msm_reset,
	.assert = msm_reset_assert,
	.deassert = msm_reset_deassert,
};
EXPORT_SYMBOL(msm_reset_ops);

int msm_reset_controller_register(struct platform_device *pdev,
	const struct msm_reset_map *map, unsigned int num_resets,
	void __iomem *virt_base)
{
	struct msm_reset_controller *reset;
	int ret = 0;

	reset = devm_kzalloc(&pdev->dev, sizeof(*reset), GFP_KERNEL);
	if (!reset)
		return -ENOMEM;

	reset->rcdev.of_node = pdev->dev.of_node;
	reset->rcdev.ops = &msm_reset_ops;
	reset->rcdev.owner = pdev->dev.driver->owner;
	reset->rcdev.nr_resets = num_resets;
	reset->reset_map = map;
	reset->base = virt_base;

	ret = reset_controller_register(&reset->rcdev);
	if (ret)
		dev_err(&pdev->dev, "Failed to register with reset controller\n");

	return ret;
}
EXPORT_SYMBOL(msm_reset_controller_register);

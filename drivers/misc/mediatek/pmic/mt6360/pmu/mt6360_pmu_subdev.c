/*
 *  drivers/misc/mediatek/pmic/mt6360/mt6360_pmu_subdev.c
 *  Driver for MT6360 PMIC SubDev
 *
 *  Copyright (C) 2018 Mediatek Technology Inc.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/mfd/core.h>
#include <linux/of_irq.h>

#include "../inc/mt6360_pmu.h"

static struct mfd_cell mt6360_pmu_subdevs[MT6360_PMUDEV_MAX] = {
	{
		.name = "mt6360_pmu_core",
		.of_compatible = "mediatek,mt6360_pmu_core",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "mt6360_pmu_adc",
		.of_compatible = "mediatek,mt6360_pmu_adc",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "mt6360_pmu_chg",
		.of_compatible = "mediatek,mt6360_pmu_chg",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "mt6360_pmu_fled",
		.of_compatible = "mediatek,mt6360_pmu_fled",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "mt6360_pmu_rgbled",
		.of_compatible = "mediatek,mt6360_pmu_rgbled",
		.pm_runtime_no_callbacks = true,
	},
};

static int mt6360_pmu_init_of_subdevs(struct mt6360_pmu_info *mpi)
{
	struct mfd_cell *cell = NULL;
	struct device_node *np = NULL;
	struct resource *irq_res = NULL;
	int i, irq_cnt, ret;

	for (i = 0; i < MT6360_PMUDEV_MAX; i++) {
		cell = mt6360_pmu_subdevs + i;
		np = NULL;
		for_each_child_of_node(mpi->dev->of_node, np) {
			if (of_device_is_compatible(np, cell->of_compatible))
				break;
		}
		if (!np)
			continue;
		irq_cnt = of_irq_count(np);
		if (!irq_cnt)
			continue;
		irq_res = devm_kzalloc(mpi->dev,
				       irq_cnt * sizeof(*irq_res), GFP_KERNEL);
		if (!irq_res)
			return -ENOMEM;
		ret = of_irq_to_resource_table(np, irq_res, irq_cnt);
		cell->resources = irq_res;
		cell->num_resources = ret;
	}
	return 0;
}

static int mt6360_pmu_init_nonof_subdevs(struct mt6360_pmu_info *mpi)
{
	struct mt6360_pmu_platform_data *pdata = dev_get_platdata(mpi->dev);
	struct mfd_cell *cell = NULL;
	struct resource *res = NULL;
	int i, j, ret;

	for (i = 0; i < MT6360_PMUDEV_MAX; i++) {
		cell = mt6360_pmu_subdevs + i;
		for (j = 0; j < pdata->dev_irq_res_cnt[i]; j++) {
			res = pdata->dev_irq_resources[i] + j;
			ret = irq_create_mapping(mpi->irq_domain,
						 res->start);
			res->start = res->end = ret;
		}
		cell->platform_data = pdata->dev_platform_data[i];
		cell->pdata_size = pdata->dev_pdata_size[i];
		cell->resources = pdata->dev_irq_resources[i];
		cell->num_resources = pdata->dev_irq_res_cnt[i];
	}
	return 0;
}

static inline int mt6360_pmu_init_subdevs(struct mt6360_pmu_info *mpi)
{
	return (mpi->dev->of_node ? mt6360_pmu_init_of_subdevs(mpi) :
		mt6360_pmu_init_nonof_subdevs(mpi));
}

int mt6360_pmu_subdev_register(struct mt6360_pmu_info *mpi)
{
	int ret;

	ret = mt6360_pmu_init_subdevs(mpi);
	if (ret < 0) {
		dev_err(mpi->dev, "fail to init subdevs\n");
		return ret;
	}
	ret = mfd_add_devices(mpi->dev, PLATFORM_DEVID_AUTO, mt6360_pmu_subdevs,
			      ARRAY_SIZE(mt6360_pmu_subdevs), NULL, 0, NULL);
	if (ret < 0) {
		dev_err(mpi->dev, "fail to add subdevs\n");
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mt6360_pmu_subdev_register);

void mt6360_pmu_subdev_unregister(struct mt6360_pmu_info *mpi)
{
	mfd_remove_devices(mpi->dev);
}
EXPORT_SYMBOL_GPL(mt6360_pmu_subdev_unregister);

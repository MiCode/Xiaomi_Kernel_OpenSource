/*
 *  drivers/mfd/rt5081_pmu_irq.c
 *  Driver to Richtek RT5081 PMU IRQ.
 *
 *  Copyright (C) 2016 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/of_irq.h>

#include "inc/rt5081_pmu.h"

static struct mfd_cell rt5081_pmu_subdev_cells[RT5081_PMU_DEV_MAX] = {
	{
		.name = "rt5081_pmu_core",
		.of_compatible = "richtek,rt5081_pmu_core",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "rt5081_pmu_charger",
		.of_compatible = "richtek,rt5081_pmu_charger",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "rt5081_pmu_fled",
		.of_compatible = "richtek,rt5081_pmu_fled1",
		.pm_runtime_no_callbacks = true,
		.id = 1,
	},
	{
		.name = "rt5081_pmu_fled",
		.of_compatible = "richtek,rt5081_pmu_fled2",
		.pm_runtime_no_callbacks = true,
		.id = 2,
	},
	{
		.name = "rt5081_pmu_ldo",
		.of_compatible = "richtek,rt5081_pmu_ldo",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "rt5081_pmu_rgbled",
		.of_compatible = "richtek,rt5081_pmu_rgbled",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "rt5081_pmu_bled",
		.of_compatible = "richtek,rt5081_pmu_bled",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "rt5081_pmu_dsv",
		.of_compatible = "richtek,rt5081_pmu_dsv",
		.pm_runtime_no_callbacks = true,
	},
};

static inline int rt5081_pmu_init_of_subdevs(struct rt5081_pmu_chip *chip)
{
	struct resource *res;
	struct mfd_cell *cell;
	struct device_node *np;
	int i, j, irq_cnt, ret = 0;

	for (i = 0; i < RT5081_PMU_DEV_MAX; i++) {
		cell = rt5081_pmu_subdev_cells + i;
		np = NULL;
		for_each_child_of_node(chip->dev->of_node, np) {
			if (of_device_is_compatible(np, cell->of_compatible))
				break;
		}
		if (!np)
			continue;
		irq_cnt = 0;
		while (true) {
			const char *name = NULL;

			ret = of_property_read_string_index(np,
							    "interrupt-names",
							    irq_cnt, &name);
			if (ret < 0)
				break;
			irq_cnt++;
		}
		if (!irq_cnt)
			continue;
		res = devm_kzalloc(chip->dev,
				   sizeof(*res) * irq_cnt, GFP_KERNEL);
		if (!res)
			return -ENOMEM;
		for (j = 0; j < irq_cnt; j++) {
			const char *name = NULL;

			of_property_read_string_index(np, "interrupt-names",
						      j, &name);
			res[j].name = name;
			ret = rt5081_pmu_get_virq_number(chip, name);
			res[j].start = res[j].end = ret;
			if (ret < 0)
				continue;
			res[j].flags = IORESOURCE_IRQ;
		}
		/* store to original platform data */
		cell->num_resources = irq_cnt;
		cell->resources = res;
	}
	return 0;
}

static inline int rt5081_pmu_init_nonof_subdevs(struct rt5081_pmu_chip *chip)
{
	struct rt5081_pmu_platform_data *pdata = dev_get_platdata(chip->dev);
	struct mfd_cell *cell;
	struct resource *res;
	int i, j, ret = 0;

	for (i = 0; i < RT5081_PMU_DEV_MAX; i++) {
		cell = rt5081_pmu_subdev_cells + i;
		res = devm_kzalloc(chip->dev,
				   pdata->num_irq_enable[i] * sizeof(*res),
				   GFP_KERNEL);
		if (!res)
			return -ENOMEM;
		for (j = 0; j < pdata->num_irq_enable[i]; j++) {
			ret = rt5081_pmu_get_virq_number(chip,
							 pdata->irq_enable[i][j]);
			res[j].name = pdata->irq_enable[i][j];
			res[j].start = res[j].end = ret;
		}
		cell->num_resources = pdata->num_irq_enable[i];
		cell->resources = res;
		cell->platform_data =  pdata->platform_data[i];
		cell->pdata_size = pdata->pdata_size[i];
	}
	return 0;
}

static inline int rt5081_pmu_init_subdevs(struct rt5081_pmu_chip *chip)
{
	return (chip->dev->of_node ? rt5081_pmu_init_of_subdevs(chip) :
		rt5081_pmu_init_nonof_subdevs(chip));
}

int rt5081_pmu_subdevs_register(struct rt5081_pmu_chip *chip)
{
	int ret = 0;

	ret = rt5081_pmu_init_subdevs(chip);
	if (ret < 0)
		return ret;
#if 1 /*(LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)) */
	return mfd_add_devices(chip->dev, -1, rt5081_pmu_subdev_cells,
			RT5081_PMU_DEV_MAX, NULL, 0, NULL);
#else
	return mfd_add_devices(chip->dev, -1, rt5081_pmu_subdev_cells,
			RT5081_PMU_DEV_MAX, NULL, 0);
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)) */
}
EXPORT_SYMBOL(rt5081_pmu_subdevs_register);

void rt5081_pmu_subdevs_unregister(struct rt5081_pmu_chip *chip)
{
	mfd_remove_devices(chip->dev);
}
EXPORT_SYMBOL(rt5081_pmu_subdevs_unregister);

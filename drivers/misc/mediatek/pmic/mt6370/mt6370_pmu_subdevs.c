// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/of_irq.h>

#include "inc/mt6370_pmu.h"

static struct mfd_cell mt6370_pmu_subdev_cells[MT6370_PMU_DEV_MAX] = {
	{
		.name = "mt6370_pmu_core",
		.of_compatible = "mediatek,mt6370_pmu_core",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "mt6370_pmu_charger",
		.of_compatible = "mediatek,mt6370_pmu_charger",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "mt6370_pmu_fled",
		.of_compatible = "mediatek,mt6370_pmu_fled1",
		.pm_runtime_no_callbacks = true,
		.id = 1,
	},
	{
		.name = "mt6370_pmu_fled",
		.of_compatible = "mediatek,mt6370_pmu_fled2",
		.pm_runtime_no_callbacks = true,
		.id = 2,
	},
	{
		.name = "mt6370_pmu_ldo",
		.of_compatible = "mediatek,mt6370_pmu_ldo",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "mt6370_pmu_rgbled",
		.of_compatible = "mediatek,mt6370_pmu_rgbled",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "mt6370_pmu_bled",
		.of_compatible = "mediatek,mt6370_pmu_bled",
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "mt6370_pmu_dsv",
		.of_compatible = "mediatek,mt6370_pmu_dsv",
		.pm_runtime_no_callbacks = true,
	},
};

static inline int mt6370_pmu_init_of_subdevs(struct mt6370_pmu_chip *chip)
{
	struct resource *res;
	struct mfd_cell *cell;
	struct device_node *np;
	int i, j, irq_cnt, ret = 0;

	for (i = 0; i < MT6370_PMU_DEV_MAX; i++) {
		cell = mt6370_pmu_subdev_cells + i;
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
			ret = mt6370_pmu_get_virq_number(chip, name);
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

static inline int mt6370_pmu_init_nonof_subdevs(struct mt6370_pmu_chip *chip)
{
	struct mt6370_pmu_platform_data *pdata = dev_get_platdata(chip->dev);
	struct mfd_cell *cell;
	struct resource *res;
	int i, j, ret = 0;

	for (i = 0; i < MT6370_PMU_DEV_MAX; i++) {
		cell = mt6370_pmu_subdev_cells + i;
		res = devm_kzalloc(chip->dev,
				   pdata->num_irq_enable[i] * sizeof(*res),
				   GFP_KERNEL);
		if (!res)
			return -ENOMEM;
		for (j = 0; j < pdata->num_irq_enable[i]; j++) {
			ret = mt6370_pmu_get_virq_number(chip,
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

static inline int mt6370_pmu_init_subdevs(struct mt6370_pmu_chip *chip)
{
	return (chip->dev->of_node ? mt6370_pmu_init_of_subdevs(chip) :
		mt6370_pmu_init_nonof_subdevs(chip));
}

int mt6370_pmu_subdevs_register(struct mt6370_pmu_chip *chip)
{
	int ret = 0;

	ret = mt6370_pmu_init_subdevs(chip);
	if (ret < 0)
		return ret;

	return mfd_add_devices(chip->dev, -1, mt6370_pmu_subdev_cells,
			MT6370_PMU_DEV_MAX, NULL, 0, NULL);
}
EXPORT_SYMBOL(mt6370_pmu_subdevs_register);

void mt6370_pmu_subdevs_unregister(struct mt6370_pmu_chip *chip)
{
	mfd_remove_devices(chip->dev);
}
EXPORT_SYMBOL(mt6370_pmu_subdevs_unregister);

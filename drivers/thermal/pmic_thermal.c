/*
 * pmic_thermal.c - PMIC thermal driver
 *
 * Copyright (C) 2012-2015 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Iyer, Yegnesh S <yegnesh.s.iyer@intel.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/mfd/intel_soc_pmic.h>

static int pmic_irq_count;

static irqreturn_t pmic_thermal_irq_handler(int irq, void *data)
{
	struct pmic_thermal_data *td = data;
	struct thermal_zone_device *tzd;
	int i, j;
	int ret;
	u16 reg;
	u16 evt_stat_reg;
	u8 mask;
	u8 evt_mask;
	u8 irq_stat;
	u8 trip;
	u8 event;

	/* Resolve thermal irqs */
	for (i = 0; i < td->num_maps; i++) {
		for (j = 0; j < td->maps[i].num_trips; j++) {
			reg = td->maps[i].trip_config[j].irq_reg;
			mask = td->maps[i].trip_config[j].irq_mask;
			/* Read the irq register to resolve whether the
			interrupt was triggered for this sensor */
			ret = intel_soc_pmic_readb(reg);
			if (ret < 0)
				return IRQ_HANDLED;
			irq_stat = ((u8)ret & mask);
			if (irq_stat) {
				/* Read the status register to find out what
				event occured i.e a high or a low */
				evt_stat_reg =
					td->maps[i].trip_config[j].evt_stat;
				evt_mask =
					td->maps[i].trip_config[j].evt_mask;
				ret = intel_soc_pmic_readb(evt_stat_reg);
				if (ret < 0)
					return IRQ_HANDLED;

				event = ((u8)ret & evt_mask);
				trip = td->maps[i].trip_config[j].trip_num;
				tzd = thermal_zone_get_zone_by_name(
						td->maps[i].handle);
				if (!IS_ERR(tzd)) {
					tzd->crossed_trip = trip;
					tzd->event = event ? 1 : 0;
					thermal_zone_device_update(tzd);
				}
			}
		}
	}

	return IRQ_HANDLED;
}

static void pmic_thermal_free_irqs(struct platform_device *pdev)
{
	struct pmic_thermal_data *thermal_data =
			(struct pmic_thermal_data *)pdev->dev.platform_data;
	int irq;

	while  (pmic_irq_count >= 0) {
		irq = platform_get_irq(pdev, (pmic_irq_count));
		free_irq(irq, thermal_data);
		pmic_irq_count--;
	}
}

static int pmic_thermal_probe(struct platform_device *pdev)
{
	struct pmic_thermal_data *thermal_data;
	int irq;
	int ret;
	int i, j;
	u16 reg;
	u8 mask;

	thermal_data = (struct pmic_thermal_data *)pdev->dev.platform_data;
	if (thermal_data == NULL) {
		dev_err(&pdev->dev, "No thermal data initialized!!\n");
		return -ENODEV;
	}

	pmic_irq_count = 0;
	while ((irq = platform_get_irq(pdev, pmic_irq_count)) != -ENXIO) {
		ret = request_threaded_irq(irq, NULL, pmic_thermal_irq_handler,
			      IRQF_ONESHOT, "pmic_thermal", thermal_data);
		if (ret) {
			dev_err(&pdev->dev, "request irq failed: %d\n",
				ret);
			pmic_irq_count--;
			pmic_thermal_free_irqs(pdev);
			return ret;
		}
		pmic_irq_count++;
	}
	pmic_irq_count--;

	/* Enable thermal interrupts */
	for (i = 0; i < thermal_data->num_maps; i++) {
		for (j = 0; j < thermal_data->maps[i].num_trips; j++) {
			reg = thermal_data->maps[i].trip_config[j].irq_en;
			mask = thermal_data->maps[i].trip_config[j].irq_en_mask;
			ret = intel_soc_pmic_update(reg, 0x00, mask);
			if (ret < 0) {
				pmic_thermal_free_irqs(pdev);
				return ret;
			}
		}
	}

	return 0;
}

static int pmic_thermal_remove(struct platform_device *pdev)
{

	pmic_thermal_free_irqs(pdev);

	return 0;
}

static struct platform_device_id pmic_thermal_id_table[] = {
	{ .name = "crystal_cove_thermal" },
	{ .name = "whiskey_cove_thermal" },
	{},
};

static struct platform_driver pmic_thermal_driver = {
	.probe = pmic_thermal_probe,
	.remove = pmic_thermal_remove,
	.driver = {
		.name = "pmic_thermal",
	},
	.id_table = pmic_thermal_id_table,
};

MODULE_DEVICE_TABLE(platform, pmic_thermal_id_table);
module_platform_driver(pmic_thermal_driver);

MODULE_AUTHOR("Iyer, Yegnesh S <yegnesh.s.iyer@intel.com>");
MODULE_DESCRIPTION("PMIC thermal Driver");
MODULE_LICENSE("GPL");

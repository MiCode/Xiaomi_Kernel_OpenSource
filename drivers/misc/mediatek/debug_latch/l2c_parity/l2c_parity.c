/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Mars.Cheng <mars.cheng@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/printk.h>
#include <linux/module.h>
#include <asm/cacheflush.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>

void __iomem *parity_debug_base;
unsigned int l2_err_status_offset;
unsigned int l3_err_status_offset;

static irqreturn_t l2c_parity_interrupt(int irq, void *dev_id)
{
#ifndef CONFIG_MTK_ENG_BUILD
	unsigned int l2c_reg[17];
	int i;
#endif
	pr_debug(">>> L2 Parity Error!! <<<\n");

#ifndef CONFIG_MTK_ENG_BUILD
	if (l2_err_status_offset) {
		for (i = 0; i < 17; i++)
			l2c_reg[i] = readl(parity_debug_base +
				l2_err_status_offset + 4*i);

		writel(0xFF, parity_debug_base + l2_err_status_offset);
		dsb(sy);
		writel(0x0, parity_debug_base + l2_err_status_offset);
		dsb(sy);

		pr_debug("%s%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x\n\
			%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x\n \
			%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x\n \
			%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x\n",
			"L2C parity error.\n",
			"error status", l2c_reg[0],
			"CPU0 info1", l2c_reg[1], "CPU0 info2", l2c_reg[2],
			"CPU1 info1", l2c_reg[3], "CPU1 info2", l2c_reg[4],
			"CPU2 info1", l2c_reg[5], "CPU2 info2", l2c_reg[6],
			"CPU3 info1", l2c_reg[7], "CPU3 info2", l2c_reg[8],
			"CPU4 info1", l2c_reg[9], "CPU4 info2", l2c_reg[10],
			"CPU5 info1", l2c_reg[11], "CPU5 info2", l2c_reg[12],
			"CPU6 info1", l2c_reg[13], "CPU6 info2", l2c_reg[14],
			"CPU7 info1", l2c_reg[15], "CPU7 info2", l2c_reg[16]);

		aee_kernel_exception("L2C parity Error",
			"%s%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x\n \
			%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x\n \
			%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x\n \
			%s = 0x%x,%s = 0x%x,%s = 0x%x,%s = 0x%x\n",
			"L2C parity error.\n",
			"error status", l2c_reg[0],
			"CPU0 info1", l2c_reg[1], "CPU0 info2", l2c_reg[2],
			"CPU1 info1", l2c_reg[3], "CPU1 info2", l2c_reg[4],
			"CPU2 info1", l2c_reg[5], "CPU2 info2", l2c_reg[6],
			"CPU3 info1", l2c_reg[7], "CPU3 info2", l2c_reg[8],
			"CPU4 info1", l2c_reg[9], "CPU4 info2", l2c_reg[10],
			"CPU5 info1", l2c_reg[11], "CPU5 info2", l2c_reg[12],
			"CPU6 info1", l2c_reg[13], "CPU6 info2", l2c_reg[14],
			"CPU7 info1", l2c_reg[15], "CPU7 info2", l2c_reg[16]);
	}
#else
	BUG();
#endif
	return IRQ_NONE;
}

static irqreturn_t l3c_parity_interrupt(int irq, void *dev_id)
{
	pr_debug(">>> L3 Parity Error!! <<<\n");

#ifndef CONFIG_MTK_ENG_BUILD
	if (l3_err_status_offset) {
		pr_debug("%s%s = 0x%x,%s = 0x%x,%s = 0x%x,\n",
			"L2C parity error.\n", "L3 parity 1",
			readl(parity_debug_base + l3_err_status_offset),
			"L3 parity 2",
			readl(parity_debug_base + l3_err_status_offset + 4),
			"L3 parity 3",
			readl(parity_debug_base + l3_err_status_offset + 8));

		aee_kernel_exception("L3C parity Error",
			"%s%s = 0x%x,%s = 0x%x,%s = 0x%x,\n",
			"L2C parity error.\n", "L3 parity 1",
			readl(parity_debug_base + l3_err_status_offset),
			"L3 parity 2",
			readl(parity_debug_base + l3_err_status_offset + 4),
			"L3 parity 3",
			readl(parity_debug_base + l3_err_status_offset + 8));

		writel(0x1, parity_debug_base + l3_err_status_offset + 8);
		dsb(sy);
		writel(0x0, parity_debug_base + l3_err_status_offset);
		dsb(sy);
	}
#else
	BUG();
#endif
	return IRQ_NONE;
}

static int l2c_parity_probe(struct platform_device *pdev)
{
	int irq = -1;
	int i = 1;
	int target_all;
	const char *irq_name;
	int ret;

	parity_debug_base = of_iomap(pdev->dev.of_node, 0);
	if (!parity_debug_base)
		return -ENOMEM;

	ret = of_property_read_u32(pdev->dev.of_node, "l2_err_status_offset",
		&l2_err_status_offset);
	if (ret)
		return ret;

	ret = of_property_read_u32(pdev->dev.of_node, "l3_err_status_offset",
		&l3_err_status_offset);
	if (ret)
		return ret;

	ret = of_property_read_u32(pdev->dev.of_node, "target_all",
		&target_all);
	if (ret)
		return ret;

	/* L3C Parity */
	irq = of_irq_get(pdev->dev.of_node, 0);
	if (irq > 0)
		if (request_irq(irq, &l3c_parity_interrupt,
				IRQF_TRIGGER_NONE, "L3C_irq", 0))
			return -EIO;

	/* L2C parity */
	while ((irq = of_irq_get(pdev->dev.of_node, i)) > 0) {
		ret = of_property_read_string_index(pdev->dev.of_node,
			"interrupt-names", i, &irq_name);

		if (ret < 0)
			irq_name = "L2C_irq";

		if ((i-1) > target_all) {
			if (irq_set_affinity(irq, cpumask_of(i-1)))
				return -EIO;
		}

		if (request_irq(irq, &l2c_parity_interrupt,
				IRQF_TRIGGER_NONE, irq_name, 0))
			return -EIO;
		i++;
	}

	return 0;
}

static const struct of_device_id l2c_parity_of_ids[] = {
	{   .compatible = "mediatek,l2c_parity-v1", },
	{}
};

static struct platform_driver l2c_parity_drv = {
	.driver = {
		.name = "l2_parity",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = l2c_parity_of_ids,
	},
	.probe = l2c_parity_probe,
};

static int __init l2c_parity_init(void)
{
	return platform_driver_register(&l2c_parity_drv);
}

module_init(l2c_parity_init);

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
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>

static irqreturn_t l2c_parity_interrupt(int irq, void *dev_id)
{
	pr_debug(">>> L2 Parity Error!! <<<\n");
	BUG();

	return IRQ_NONE;
}

static int l2c_parity_probe(struct platform_device *pdev)
{
	int irq = -1;

	irq = of_irq_get(pdev->dev.of_node, 0);
	if (irq > 0)
		if (request_irq(irq, &l2c_parity_interrupt,
				IRQF_TRIGGER_NONE, "L2C_irq-0", 0))
			return -EIO;

	irq = of_irq_get(pdev->dev.of_node, 1);
	if (irq > 0)
		if (request_irq(irq, &l2c_parity_interrupt,
				IRQF_TRIGGER_NONE, "L2C_irq-1", 0))
			return -EIO;
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

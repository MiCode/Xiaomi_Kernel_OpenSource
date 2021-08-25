/*
 * Copyright 2018 GoldenRiver Technologies Co., Ltd. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

/* should not reach here */
static irqreturn_t irq_handler(int irq, void *data)
{
	WARN_ON(true);
	return IRQ_HANDLED;
}

static int hee_irq_probe(struct platform_device *pdev)
{
	int ret;
	u32 irq, cpu;
	ulong hwirq;
	void __iomem *gicd_base;
	struct irq_desc *desc;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "platform_get_irq err %d\n", ret);
		return -EINVAL;
	}

	desc = irq_to_desc(irq);
	if (!desc) {
		dev_err(&pdev->dev, "irq_desc(%d) null\n", irq);
		return -EINVAL;
	}
	hwirq = desc->irq_data.hwirq;

	ret = devm_request_irq(&pdev->dev, irq, irq_handler,
			       IRQF_TRIGGER_RISING, "hee_irq", NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq irq %d err %d\n", irq, ret);
		return ret;
	}

	gicd_base = ioremap_nocache(0x08000000, 1024 * 1024);

	for_each_possible_cpu(cpu) {
		if (cpumask_test_cpu(cpu, cpu_online_mask))
			continue;

		/* Aff1/2/3 is 0 in Virt. :-) */
		gic_write_irouter(cpu, gicd_base + GICD_IROUTER + hwirq * 8);
		break;
	}

	iounmap(gicd_base);

	dev_dbg(&pdev->dev, "irq %d hwirq %lu configured.\n", irq,
		desc->irq_data.hwirq);

	return 0;
}

static const struct of_device_id hee_irq_of_match[] = {
	{
		.compatible = "nbl,hee-irq",
	},
	{},
};

static struct platform_driver hee_irq_driver = {
	.probe = hee_irq_probe,
	.driver = {
		.name = "trusty-hee-irq",
		.owner = THIS_MODULE,
		.of_match_table = hee_irq_of_match,
	},
};

static int __init hee_irq_driver_init(void)
{
	return platform_driver_register(&hee_irq_driver);
}

arch_initcall(hee_irq_driver_init);

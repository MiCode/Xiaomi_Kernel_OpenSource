/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "mtk_qos_ipi.h"
#include "mtk_qos_bound.h"
#include "mtk_qos_sram.h"
#include "mtk_qos_sysfs.h"

static int mtk_qos_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *regs;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);
	qos_sram_init(regs);

	qos_add_interface(&pdev->dev);
	qos_ipi_init();
	qos_bound_init();

	return 0;
}

static int mtk_qos_remove(struct platform_device *pdev)
{
	qos_remove_interface(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_qos_of_match[] = {
	{ .compatible = "mediatek,qos" },
	{ .compatible = "mediatek,qos-2.0" },
	{ },
};

static struct platform_driver mtk_qos_platdrv = {
	.probe	= mtk_qos_probe,
	.remove	= mtk_qos_remove,
	.driver	= {
		.name	= "mtk-qos",
		.of_match_table = mtk_qos_of_match,
	},
};

static int __init mtk_qos_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_qos_platdrv);

	return ret;
}

late_initcall(mtk_qos_init)

static void __exit mtk_qos_exit(void)
{
	platform_driver_unregister(&mtk_qos_platdrv);
}
module_exit(mtk_qos_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek QoS driver");

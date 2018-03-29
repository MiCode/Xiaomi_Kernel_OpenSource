/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>

static const struct of_device_id scpsys_bring_up_id_table[] = {
	{ .compatible = "mediatek,scpsys-bring-up",},
	{ },
};
MODULE_DEVICE_TABLE(of, scpsys_bring_up_id_table);

static int scpsys_bring_up_probe(struct platform_device *pdev)
{
	if (!pdev->dev.pm_domain)
		return -EPROBE_DEFER;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	return 0;
}

static int scpsys_bring_up_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver scpsys_bring_up = {
	.probe		= scpsys_bring_up_probe,
	.remove		= scpsys_bring_up_remove,
	.driver		= {
		.name	= "scpsys_bring_up",
		.owner	= THIS_MODULE,
		.of_match_table = scpsys_bring_up_id_table,
	},
};

module_platform_driver(scpsys_bring_up);

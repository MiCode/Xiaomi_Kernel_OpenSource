// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>

static const struct of_device_id bring_up_id_table[] = {
	{ .compatible = "mediatek,scpsys-bringup",},
	{ .compatible = "mediatek,scpsys-bringup-mt6779",},
	{ },
};
MODULE_DEVICE_TABLE(of, bring_up_id_table);

static int bring_up_probe(struct platform_device *pdev)
{
	if (!pdev->dev.pm_domain)
		return -EPROBE_DEFER;

	pm_runtime_enable(&pdev->dev);

	/* always enabled in lifetime */
	pm_runtime_get_sync(&pdev->dev);

	return 0;
}

static int bring_up_remove(struct platform_device *pdev)
{
	pm_runtime_put_sync(&pdev->dev);
	return 0;
}

static struct platform_driver bring_up = {
	.probe		= bring_up_probe,
	.remove		= bring_up_remove,
	.driver		= {
		.name	= "bring_up",
		.owner	= THIS_MODULE,
		.of_match_table = bring_up_id_table,
	},
};
module_platform_driver(bring_up);

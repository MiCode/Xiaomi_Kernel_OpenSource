// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>

static const struct of_device_id scpsys_disable_unused_id_table[] = {
	{ .compatible = "mediatek,scpsys-disable-unused",},
	{ },
};

MODULE_DEVICE_TABLE(of, scpsys_disable_unused_id_table);

static int scpsys_disable_unused_probe(struct platform_device *pdev)
{
	pm_runtime_enable(&pdev->dev);

	return 0;
}

static struct platform_driver scpsys_disable_unused = {
	.probe		= scpsys_disable_unused_probe,
	.driver		= {
		.name	= "scpsys_disable_unused",
		.owner	= THIS_MODULE,
		.of_match_table = scpsys_disable_unused_id_table,
	},
};

static int __init scpsys_disable_unused_init(void)
{
	return platform_driver_register(&scpsys_disable_unused);
}

static void __exit scpsys_disable_unused_exit(void)
{
	platform_driver_unregister(&scpsys_disable_unused);
}

late_initcall_sync(scpsys_disable_unused_init);
module_exit(scpsys_disable_unused_exit);
MODULE_LICENSE("GPL");

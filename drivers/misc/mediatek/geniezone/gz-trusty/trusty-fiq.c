// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */
/*
 * This is interrupt driver
 *
 * GZ does not support virtual interrupt, interrupt forwarding driver is
 * need for passing GZ and hypervisor-TEE interrupts
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>

#include "trusty-fiq.h"

static int trusty_fiq_remove_child(struct device *dev, void *data)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static int trusty_fiq_probe(struct platform_device *pdev)
{
	int ret;

	ret = trusty_fiq_arch_probe(pdev);
	if (ret)
		goto err_set_fiq_return;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret < 0) {
		dev_info(&pdev->dev, "Failed to add children: %d\n", ret);
		goto err_add_children;
	}

	return 0;

err_add_children:
	device_for_each_child(&pdev->dev, NULL, trusty_fiq_remove_child);
	trusty_fiq_arch_remove(pdev);
err_set_fiq_return:
	return ret;
}

static int trusty_fiq_remove(struct platform_device *pdev)
{
	device_for_each_child(&pdev->dev, NULL, trusty_fiq_remove_child);
	trusty_fiq_arch_remove(pdev);
	return 0;
}

static const struct of_device_id trusty_fiq_of_match[] = {
	{ .compatible = "android,trusty-fiq-v1", },
	{},
};

static struct platform_driver trusty_fiq_driver = {
	.probe = trusty_fiq_probe,
	.remove = trusty_fiq_remove,
	.driver	= {
		.name = "trusty-fiq",
		.owner = THIS_MODULE,
		.of_match_table = trusty_fiq_of_match,
	},
};

static int __init trusty_fiq_driver_init(void)
{
	return platform_driver_register(&trusty_fiq_driver);
}

static void __exit trusty_fiq_driver_exit(void)
{
	platform_driver_unregister(&trusty_fiq_driver);
}

subsys_initcall(trusty_fiq_driver_init);
module_exit(trusty_fiq_driver_exit);

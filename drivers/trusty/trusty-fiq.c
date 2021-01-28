/*
 * Copyright (C) 2013 Google, Inc.
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
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/trusty.h>

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
		dev_err(&pdev->dev, "Failed to add children: %d\n", ret);
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

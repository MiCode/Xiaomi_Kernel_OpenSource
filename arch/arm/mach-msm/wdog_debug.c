/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <mach/scm.h>
#include <linux/slab.h>

#define MODULE_NAME "wdog_debug"
#define WDOG_DEBUG_EN 17
#define GCC_WDOG_DEBUG_OFFSET 0x780

struct msm_wdog_debug_data {
	unsigned int __iomem phys_base;
	size_t size;
	void __iomem *base;
	struct device *dev;
};

static struct msm_wdog_debug_data *wdog_data;

void msm_disable_wdog_debug(void)
{
	unsigned long int value;

	if (wdog_data == NULL)
		return;
	value = readl_relaxed(wdog_data->base + GCC_WDOG_DEBUG_OFFSET);
	value &= ~BIT(WDOG_DEBUG_EN);
	writel_relaxed(value, wdog_data->base + GCC_WDOG_DEBUG_OFFSET);
}
EXPORT_SYMBOL(msm_disable_wdog_debug);

void msm_enable_wdog_debug(void)
{
	unsigned long int value;

	if (wdog_data == NULL)
		return;
	value = readl_relaxed(wdog_data->base + GCC_WDOG_DEBUG_OFFSET);
	value |= BIT(WDOG_DEBUG_EN);
	writel_relaxed(value, wdog_data->base + GCC_WDOG_DEBUG_OFFSET);
}
EXPORT_SYMBOL(msm_enable_wdog_debug);

static int msm_wdog_debug_remove(struct platform_device *pdev)
{
	kfree(wdog_data);
	wdog_data = NULL;
	pr_info("MSM wdog_debug Exit - Deactivated\n");
	return 0;
}

static int msm_wdog_debug_dt_to_pdata(struct platform_device *pdev,
					struct msm_wdog_debug_data *pdata)
{
	struct resource *wdog_resource;

	wdog_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!wdog_resource) {
		dev_err(&pdev->dev, \
		"%s cannot allocate resource for wdog_debug\n", \
		 __func__);
		return -ENXIO;
	}
	pdata->size = resource_size(wdog_resource);
	pdata->phys_base = wdog_resource->start;
	if (unlikely(!(devm_request_region(&pdev->dev, pdata->phys_base,
					pdata->size, "msm-wdog-debug")))) {
		dev_err(&pdev->dev, "%s cannot reserve wdog_debug region\n",
								__func__);
		return -ENXIO;
	}
	pdata->base  = devm_ioremap(&pdev->dev, pdata->phys_base,
							pdata->size);
	if (!pdata->base) {
		dev_err(&pdev->dev, "%s cannot map wdog register space\n",
				__func__);
		return -ENXIO;
	}

	return 0;
}

static int msm_wdog_debug_probe(struct platform_device *pdev)
{
	int ret;
	if (!pdev->dev.of_node)
		return -ENODEV;
	wdog_data = kzalloc(sizeof(struct msm_wdog_debug_data), GFP_KERNEL);
	if (!wdog_data)
		return -ENOMEM;
	ret = msm_wdog_debug_dt_to_pdata(pdev, wdog_data);
	if (ret)
		goto err;
	wdog_data->dev = &pdev->dev;
	platform_set_drvdata(pdev, wdog_data);
	msm_enable_wdog_debug();
	return 0;
err:
	kzfree(wdog_data);
	wdog_data = NULL;
	return ret;
}

static struct of_device_id msm_wdog_debug_match_table[] = {
	{ .compatible = "qcom,msm-wdog-debug" },
	{}
};

static struct platform_driver msm_wdog_debug_driver = {
	.probe = msm_wdog_debug_probe,
	.remove = msm_wdog_debug_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_wdog_debug_match_table,
	},
};

static int wdog_debug_init(void)
{
	return platform_driver_register(&msm_wdog_debug_driver);
}
module_init(wdog_debug_init);

static void __exit wdog_debug_exit(void)
{
	platform_driver_unregister(&msm_wdog_debug_driver);
}
module_exit(wdog_debug_exit);

MODULE_DESCRIPTION("MSM Driver to disable debug Image");
MODULE_LICENSE("GPL v2");

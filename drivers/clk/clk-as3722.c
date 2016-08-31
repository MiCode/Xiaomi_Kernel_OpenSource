/*
 * Clock driver for ams AS3722 device.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/mfd/as3722.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct as3722_clks {
	struct device *dev;
	struct as3722 *as3722;
	bool enabled_at_boot;
};

static int as3722_clks_prepare(struct as3722_clks *as3722_clks)
{
	int ret;

	ret = as3722_update_bits(as3722_clks->as3722, AS3722_RTC_CONTROL_REG,
			AS3722_RTC_CLK32K_OUT_EN, AS3722_RTC_CLK32K_OUT_EN);
	if (ret < 0)
		dev_err(as3722_clks->dev, "RTC_CONTROL_REG update failed, %d\n",
			ret);

	return ret;
}

static void as3722_clks_unprepare(struct as3722_clks *as3722_clks)
{
	int ret;

	ret = as3722_update_bits(as3722_clks->as3722, AS3722_RTC_CONTROL_REG,
			AS3722_RTC_CLK32K_OUT_EN, 0);
	if (ret < 0)
		dev_err(as3722_clks->dev, "RTC_CONTROL_REG update failed, %d\n",
			ret);
}

static int as3722_clks_probe(struct platform_device *pdev)
{
	struct as3722_clks *as3722_clks;
	struct as3722 *as3722 = dev_get_drvdata(pdev->dev.parent);
	struct device_node *np = pdev->dev.parent->of_node;
	struct as3722_platform_data *pdata = as3722->dev->platform_data;
	bool clk32k_enable = false;
	int ret;

	if (pdata) {
		clk32k_enable = pdata->enable_clk32k_out;
	} else {
		if (np)
			clk32k_enable = of_property_read_bool(np,
						"ams,enable-clock32k-out");
	}

	as3722_clks = devm_kzalloc(&pdev->dev, sizeof(*as3722_clks),
				GFP_KERNEL);
	if (!as3722_clks)
		return -ENOMEM;

	platform_set_drvdata(pdev, as3722_clks);

	as3722_clks->dev = &pdev->dev;
	as3722_clks->as3722 = as3722;
	as3722_clks->enabled_at_boot = clk32k_enable;

	if (clk32k_enable) {
		ret = as3722_clks_prepare(as3722_clks);
		if (ret < 0)
			dev_err(&pdev->dev,
				"Fail to enable clk32k out %d\n", ret);
		return ret;
	} else {
		as3722_clks_unprepare(as3722_clks);
	}

	return 0;
}

static struct platform_driver as3722_clks_driver = {
	.driver = {
		.name = "as3722-clk",
		.owner = THIS_MODULE,
	},
	.probe = as3722_clks_probe,
};

static int __init as3722_clk_init(void)
{
	return platform_driver_register(&as3722_clks_driver);
}
subsys_initcall(as3722_clk_init);

static void __exit as3722_clk_exit(void)
{
	platform_driver_unregister(&as3722_clks_driver);
}
module_exit(as3722_clk_exit);

MODULE_DESCRIPTION("Clock driver for ams AS3722 PMIC Device");
MODULE_ALIAS("platform:as3722-clk");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");

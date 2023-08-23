/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/reset-controller.h>

#include <dt-bindings/clock/qcom,cmn-blk-pll.h>

#include "common.h"

#define SW_RESET_LOGIC_MASK	BIT(6)

struct cmn_blk_pll {
	struct regmap *regmap;
	struct clk *misc_reset;
	struct clk *ahb_clk;
	struct clk *aon_clk;
	struct reset_control *reset;
};
static struct cmn_blk_pll pll;

static unsigned long clk_cmn_blk_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	return to_clk_fixed_rate(hw)->fixed_rate;
}

static int clk_cmn_blk_pll_prepare(struct clk_hw *hw)
{
	int ret;

	ret = clk_prepare(pll.misc_reset);
	if (ret)
		return ret;

	ret = clk_prepare(pll.aon_clk);
	if (ret)
		return ret;

	return clk_prepare(pll.ahb_clk);
}

static int clk_cmn_blk_pll_enable(struct clk_hw *hw)
{
	u32 val, ret;

	ret = reset_control_reset(pll.reset);
	if (ret)
		return ret;

	ret = clk_enable(pll.misc_reset);
	if (ret)
		return ret;

	ret = clk_enable(pll.aon_clk);
	if (ret)
		return ret;

	ret = clk_enable(pll.ahb_clk);
	if (ret)
		return ret;

	ret = regmap_read(pll.regmap, 0, &val);
	if (ret)
		return ret;

	if (val & SW_RESET_LOGIC_MASK) {
		val &= ~BIT(6);
		regmap_write(pll.regmap, 0, val);
	}

	val |= BIT(6);
	regmap_write(pll.regmap, 0, val);

	return 0;
}

static void clk_cmn_blk_pll_unprepare(struct clk_hw *hw)
{
	clk_unprepare(pll.misc_reset);
	clk_unprepare(pll.aon_clk);
	clk_unprepare(pll.ahb_clk);
}

static void clk_cmn_blk_pll_disable(struct clk_hw *hw)
{
	u32 val;

	regmap_read(pll.regmap, 0, &val);
	val &= ~BIT(6);
	regmap_write(pll.regmap, 0, val);

	clk_disable(pll.misc_reset);
	clk_disable(pll.aon_clk);
	clk_disable(pll.ahb_clk);
}

const struct clk_ops clk_cmn_blk_ops = {
	.prepare = clk_cmn_blk_pll_prepare,
	.enable = clk_cmn_blk_pll_enable,
	.disable = clk_cmn_blk_pll_disable,
	.unprepare = clk_cmn_blk_pll_unprepare,
	.recalc_rate = clk_cmn_blk_recalc_rate,
};

static struct clk_fixed_rate cmn_blk_pll = {
	.fixed_rate = 100000000,
	.hw.init = &(struct clk_init_data){
		.name = "cmn_blk_pll",
		.parent_names = (const char *[]){ "cxo" },
		.num_parents = 1,
		.ops = &clk_cmn_blk_ops,
	},
};

struct clk_hw *cmn_blk_pll_hws[] = {
	[CMN_BLK_PLL] = &cmn_blk_pll.hw,
};

static const struct regmap_config cmn_blk_pll_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x8,
	.fast_io	= true,
};

static const struct of_device_id cmn_blk_pll_match_table[] = {
	{ .compatible = "qcom,cmn_blk_pll" },
	{ }
};
MODULE_DEVICE_TABLE(of, cmn_blk_pll_match_table);

static int cmn_blk_pll_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk_hw_onecell_data *clk_data;
	struct device *dev = &pdev->dev;
	void __iomem *base;
	int i, ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	pll.regmap = devm_regmap_init_mmio(&pdev->dev, base,
						&cmn_blk_pll_regmap_config);
	if (IS_ERR(pll.regmap))
		return PTR_ERR(pll.regmap);

	pll.misc_reset = devm_clk_get(&pdev->dev, "misc_reset_clk");
	if (IS_ERR(pll.misc_reset)) {
		if (PTR_ERR(pll.misc_reset) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get misc_reset clock\n");
		return PTR_ERR(pll.misc_reset);
	}

	pll.ahb_clk = devm_clk_get(&pdev->dev, "ahb_clk");
	if (IS_ERR(pll.ahb_clk)) {
		if (PTR_ERR(pll.ahb_clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get ahb_clk clock\n");
		return PTR_ERR(pll.ahb_clk);
	}

	pll.aon_clk = devm_clk_get(&pdev->dev, "aon_clk");
	if (IS_ERR(pll.aon_clk)) {
		if (PTR_ERR(pll.aon_clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get aon_clk clock\n");
		return PTR_ERR(pll.aon_clk);
	}

	pll.reset = devm_reset_control_get(&pdev->dev, "cmn_blk_pll_reset");
	if (IS_ERR(pll.reset)) {
		if (PTR_ERR(pll.reset) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get cmn_blk\n");
		return PTR_ERR(pll.reset);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cmn_blk");
	if (!res) {
		dev_err(&pdev->dev, "Register base not defined\n");
		return -ENOMEM;
	}

	clk_data = devm_kzalloc(dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->num = ARRAY_SIZE(cmn_blk_pll_hws);

	for (i = 0; i < clk_data->num; i++) {
		ret = devm_clk_hw_register(dev, cmn_blk_pll_hws[i]);
		if (ret) {
			dev_err(&pdev->dev, "Failed to register cmn_blk_pll\n");
			return ret;
		}
		clk_data->hws[i] = cmn_blk_pll_hws[i];
	}

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, clk_data);
	if (ret) {
		dev_err(dev, "Unable to register cmn_blk_pll provider ret = %d\n"
									, ret);
		return ret;
	}

	dev_info(&pdev->dev, "Registered CMN BLK PLL\n");
	return 0;
}

static struct platform_driver cmn_blk_pll_driver = {
	.probe  = cmn_blk_pll_probe,
	.driver = {
		.name = "cmn_blk_pll",
		.of_match_table = cmn_blk_pll_match_table,
	},
};

static int __init cmn_blk_pll_init(void)
{
	return platform_driver_register(&cmn_blk_pll_driver);
}
subsys_initcall(cmn_blk_pll_init);

static void __exit cmn_blk_pll_exit(void)
{
	platform_driver_unregister(&cmn_blk_pll_driver);
}
module_exit(cmn_blk_pll_exit);

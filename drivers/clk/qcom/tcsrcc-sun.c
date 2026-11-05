// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,tcsrcc-sun.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

static struct clk_branch tcsr_pcie_0_clkref_en = {
	.halt_reg = 0x0,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "tcsr_pcie_0_clkref_en",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch tcsr_ufs_clkref_en = {
	.halt_reg = 0x1000,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "tcsr_ufs_clkref_en",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch tcsr_usb2_clkref_en = {
	.halt_reg = 0x2000,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x2000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "tcsr_usb2_clkref_en",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch tcsr_usb3_clkref_en = {
	.halt_reg = 0x3000,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x3000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "tcsr_usb3_clkref_en",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *tcsr_cc_sun_clocks[] = {
	[TCSR_PCIE_0_CLKREF_EN] = &tcsr_pcie_0_clkref_en.clkr,
	[TCSR_UFS_CLKREF_EN] = &tcsr_ufs_clkref_en.clkr,
	[TCSR_USB2_CLKREF_EN] = &tcsr_usb2_clkref_en.clkr,
	[TCSR_USB3_CLKREF_EN] = &tcsr_usb3_clkref_en.clkr,
};

static const struct regmap_config tcsr_cc_sun_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x3000,
	.fast_io = true,
};

static const struct qcom_cc_desc tcsr_cc_sun_desc = {
	.config = &tcsr_cc_sun_regmap_config,
	.clks = tcsr_cc_sun_clocks,
	.num_clks = ARRAY_SIZE(tcsr_cc_sun_clocks),
};

static const struct of_device_id tcsr_cc_sun_match_table[] = {
	{ .compatible = "qcom,sun-tcsrcc" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcsr_cc_sun_match_table);

static int tcsr_cc_sun_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &tcsr_cc_sun_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = qcom_cc_really_probe(pdev, &tcsr_cc_sun_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register TCSR CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered TCSR CC clocks\n");

	return ret;
}

static void tcsr_cc_sun_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &tcsr_cc_sun_desc);
}

static struct platform_driver tcsr_cc_sun_driver = {
	.probe = tcsr_cc_sun_probe,
	.driver = {
		.name = "tcsr_cc-sun",
		.of_match_table = tcsr_cc_sun_match_table,
		.sync_state = tcsr_cc_sun_sync_state,
	},
};

static int __init tcsr_cc_sun_init(void)
{
	return platform_driver_register(&tcsr_cc_sun_driver);
}
subsys_initcall(tcsr_cc_sun_init);

static void __exit tcsr_cc_sun_exit(void)
{
	platform_driver_unregister(&tcsr_cc_sun_driver);
}
module_exit(tcsr_cc_sun_exit);

MODULE_DESCRIPTION("QTI TCSR_CC SUN Driver");
MODULE_LICENSE("GPL");

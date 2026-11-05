// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

static struct clk_branch tcsr_pcie_1_clkref_en = {
	.halt_reg = 0x1c,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "tcsr_pcie_1_clkref_en",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch tcsr_ufs_clkref_en = {
	.halt_reg = 0x8,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "tcsr_ufs_clkref_en",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch tcsr_usb2_clkref_en = {
	.halt_reg = 0x4,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "tcsr_usb2_clkref_en",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch tcsr_usb3_clkref_en = {
	.halt_reg = 0x10,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x10,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "tcsr_usb3_clkref_en",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch tcsr_hdmi_clkref_en = {
	.halt_reg = 0x14,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x14,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "tcsr_hdmi_clkref_en",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *tcsr_cc_tuna_clocks[] = {
	[TCSR_PCIE_0_CLKREF_EN] = &tcsr_pcie_0_clkref_en.clkr,
	[TCSR_PCIE_1_CLKREF_EN] = &tcsr_pcie_1_clkref_en.clkr,
	[TCSR_UFS_CLKREF_EN] = &tcsr_ufs_clkref_en.clkr,
	[TCSR_USB2_CLKREF_EN] = &tcsr_usb2_clkref_en.clkr,
	[TCSR_USB3_CLKREF_EN] = &tcsr_usb3_clkref_en.clkr,
	[TCSR_HDMI_CLKREF_EN] = &tcsr_hdmi_clkref_en.clkr,
};

static const struct regmap_config tcsr_cc_tuna_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x1c,
	.fast_io = true,
};

static const struct qcom_cc_desc tcsr_cc_tuna_desc = {
	.config = &tcsr_cc_tuna_regmap_config,
	.clks = tcsr_cc_tuna_clocks,
	.num_clks = ARRAY_SIZE(tcsr_cc_tuna_clocks),
};

static const struct of_device_id tcsr_cc_tuna_match_table[] = {
	{ .compatible = "qcom,tuna-tcsrcc" },
	{ .compatible = "qcom,kera-tcsrcc" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcsr_cc_tuna_match_table);

static int tcsr_cc_tuna_fixup(struct platform_device *pdev)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || compatlen <= 0)
		return -EINVAL;

	if (strcmp(compat, "qcom,tuna-tcsrcc"))
		return 0;

	tcsr_cc_tuna_clocks[TCSR_PCIE_1_CLKREF_EN] = NULL;
	tcsr_cc_tuna_clocks[TCSR_HDMI_CLKREF_EN] = NULL;

	return 0;
}

static int tcsr_cc_tuna_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &tcsr_cc_tuna_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = tcsr_cc_tuna_fixup(pdev);
	if (ret)
		return ret;

	ret = qcom_cc_really_probe(pdev, &tcsr_cc_tuna_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register TCSR CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered TCSR CC clocks\n");

	return ret;
}

static void tcsr_cc_tuna_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &tcsr_cc_tuna_desc);
}

static struct platform_driver tcsr_cc_tuna_driver = {
	.probe = tcsr_cc_tuna_probe,
	.driver = {
		.name = "tcsr_cc-tuna",
		.of_match_table = tcsr_cc_tuna_match_table,
		.sync_state = tcsr_cc_tuna_sync_state,
	},
};

static int __init tcsr_cc_tuna_init(void)
{
	return platform_driver_register(&tcsr_cc_tuna_driver);
}
subsys_initcall(tcsr_cc_tuna_init);

static void __exit tcsr_cc_tuna_exit(void)
{
	platform_driver_unregister(&tcsr_cc_tuna_driver);
}
module_exit(tcsr_cc_tuna_exit);

MODULE_DESCRIPTION("QTI TCSR_CC TUNA Driver");
MODULE_LICENSE("GPL");

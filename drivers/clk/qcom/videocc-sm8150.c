// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/pm_clock.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,videocc-sm8150.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "common.h"
#include "reset.h"
#include "vdd-level-sm8150.h"

static DEFINE_VDD_REGULATORS(vdd_mm, VDD_MM_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_CHIP_SLEEP_CLK,
	P_CORE_BI_PLL_TEST_SE,
	P_VIDEO_PLL0_OUT_EVEN,
	P_VIDEO_PLL0_OUT_MAIN,
	P_VIDEO_PLL0_OUT_ODD,
};

static struct pll_vco trion_vco[] = {
	{ 249600000, 2000000000, 0 },
};

/* 400 MHz configuration */
static struct alpha_pll_config video_pll0_config = {
	.l = 0x14,
	.alpha = 0xD555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002267,
	.config_ctl_hi1_val = 0x00000024,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000002,
	.test_ctl_hi1_val = 0x00000020,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x000000D0,
};

static struct clk_alpha_pll video_pll0 = {
	.offset = 0x42c,
	.vco_table = trion_vco,
	.num_vco = ARRAY_SIZE(trion_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TRION],
	.config = &video_pll0_config,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "video_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_trion_pll_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mm,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000},
		},
	},
};

static const struct parent_map video_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_VIDEO_PLL0_OUT_MAIN, 1 },
	{ P_VIDEO_PLL0_OUT_EVEN, 2 },
	{ P_VIDEO_PLL0_OUT_ODD, 3 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data video_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo", },
	{ .hw = &video_pll0.clkr.hw },
	{ .hw = &video_pll0.clkr.hw },
	{ .hw = &video_pll0.clkr.hw },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct freq_tbl ftbl_video_cc_iris_clk_src[] = {
	F(200000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(225000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(300000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(365000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(432000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(480000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_video_cc_iris_clk_src_sm8150_v2[] = {
	F(200000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(240000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(338000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(365000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(444000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(533000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_iris_clk_src = {
	.cmd_rcgr = 0x7f0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_0,
	.freq_tbl = ftbl_video_cc_iris_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_iris_clk_src",
		.parent_data = video_cc_parent_data_0,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 200000000,
			[VDD_LOWER] = 225000000,
			[VDD_LOW] = 300000000,
			[VDD_LOW_L1] = 365000000,
			[VDD_NOMINAL] = 432000000,
			[VDD_HIGH] = 480000000},
	},
};

static struct clk_branch video_cc_iris_ahb_clk = {
	.halt_reg = 0x8f4,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x8f4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_iris_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &video_cc_iris_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs0_core_clk = {
	.halt_reg = 0x890,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x890,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs0_core_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &video_cc_iris_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs1_core_clk = {
	.halt_reg = 0x8d0,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x8d0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs1_core_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &video_cc_iris_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvsc_core_clk = {
	.halt_reg = 0x850,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x850,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvsc_core_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &video_cc_iris_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_xo_clk = {
	.halt_reg = 0x984,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x984,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_xo_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *video_cc_sm8150_clocks[] = {
	[VIDEO_PLL0] = &video_pll0.clkr,
	[VIDEO_CC_IRIS_AHB_CLK] = &video_cc_iris_ahb_clk.clkr,
	[VIDEO_CC_IRIS_CLK_SRC] = &video_cc_iris_clk_src.clkr,
	[VIDEO_CC_MVS0_CORE_CLK] = &video_cc_mvs0_core_clk.clkr,
	[VIDEO_CC_MVS1_CORE_CLK] = &video_cc_mvs1_core_clk.clkr,
	[VIDEO_CC_MVSC_CORE_CLK] = &video_cc_mvsc_core_clk.clkr,
	[VIDEO_CC_XO_CLK] = &video_cc_xo_clk.clkr,
};

static const struct qcom_reset_map video_cc_sm8150_resets[] = {
	[VIDEO_CC_INTERFACE_BCR] = { 0x8f0 },
	[VIDEO_CC_MVS0_BCR] = { 0x870 },
	[VIDEO_CC_MVS1_BCR] = { 0x8b0 },
	[VIDEO_CC_MVSC_BCR] = { 0x810 },
	[VIDEO_CC_MVSC_CORE_CLK_BCR] = { 0x850, 2 },
};

static const struct regmap_config video_cc_sm8150_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xb94,
	.fast_io = true,
};

static const struct qcom_cc_desc video_cc_sm8150_desc = {
	.config = &video_cc_sm8150_regmap_config,
	.clks = video_cc_sm8150_clocks,
	.num_clks = ARRAY_SIZE(video_cc_sm8150_clocks),
	.resets = video_cc_sm8150_resets,
	.num_resets = ARRAY_SIZE(video_cc_sm8150_resets),
};

static const struct of_device_id video_cc_sm8150_match_table[] = {
	{ .compatible = "qcom,sm8150-videocc" },
	{ .compatible = "qcom,sm8150-videocc-v2" },
	{ .compatible = "qcom,sa8155-videocc" },
	{ .compatible = "qcom,sa8155-videocc-v2" },
	{ }
};
MODULE_DEVICE_TABLE(of, video_cc_sm8150_match_table);

static void video_cc_sm8150_fixup_sm8150v2(struct regmap *regmap)
{
	video_pll0.config->test_ctl_hi_val = 0x00000000;

	video_cc_iris_clk_src.freq_tbl = ftbl_video_cc_iris_clk_src_sm8150_v2;
	video_cc_iris_clk_src.clkr.vdd_data.rate_max[VDD_LOWER] = 240000000;
	video_cc_iris_clk_src.clkr.vdd_data.rate_max[VDD_LOW] = 338000000;
	video_cc_iris_clk_src.clkr.vdd_data.rate_max[VDD_NOMINAL] = 444000000;
	video_cc_iris_clk_src.clkr.vdd_data.rate_max[VDD_HIGH] = 533000000;
}

static int video_cc_sm8150_fixup(struct platform_device *pdev,
	struct regmap *regmap)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,sm8150-videocc-v2") ||
			!strcmp(compat, "qcom,sa8155-videocc-v2"))
		video_cc_sm8150_fixup_sm8150v2(regmap);

	return 0;
}

static int video_cc_sm8150_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	vdd_mm.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mm");
	if (IS_ERR(vdd_mm.regulator[0])) {
		if (PTR_ERR(vdd_mm.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_mm regulator\n");
		return PTR_ERR(vdd_mm.regulator[0]);
	}

	pm_runtime_enable(&pdev->dev);
	ret = pm_clk_create(&pdev->dev);
	if (ret)
		goto disable_pm_runtime;

	ret = pm_clk_add(&pdev->dev, "cfg_ahb_clk");
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to get ahb clock handle\n");
		goto destroy_pm_clk;
	}

	regmap = qcom_cc_map(pdev, &video_cc_sm8150_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the Video CC registers\n");
		ret = PTR_ERR(regmap);
		goto destroy_pm_clk;
	}

	ret = video_cc_sm8150_fixup(pdev, regmap);
	if (ret)
		goto destroy_pm_clk;

	clk_trion_pll_configure(&video_pll0, regmap, video_pll0.config);

	ret = qcom_cc_really_probe(pdev, &video_cc_sm8150_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register VIDEO CC clocks\n");
		goto destroy_pm_clk;
	}

	dev_info(&pdev->dev, "Registered VIDEO CC clocks\n");

	return 0;

destroy_pm_clk:
	pm_clk_destroy(&pdev->dev);

disable_pm_runtime:
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static const struct dev_pm_ops video_cc_sm8150_pm_ops = {
	SET_RUNTIME_PM_OPS(pm_clk_suspend, pm_clk_resume, NULL)
};

static struct platform_driver video_cc_sm8150_driver = {
	.probe = video_cc_sm8150_probe,
	.driver = {
		.name = "video_cc-sm8150",
		.of_match_table = video_cc_sm8150_match_table,
		.pm = &video_cc_sm8150_pm_ops,
	},
};

static int __init video_cc_sm8150_init(void)
{
	return platform_driver_register(&video_cc_sm8150_driver);
}
subsys_initcall(video_cc_sm8150_init);

static void __exit video_cc_sm8150_exit(void)
{
	platform_driver_unregister(&video_cc_sm8150_driver);
}
module_exit(video_cc_sm8150_exit);

MODULE_DESCRIPTION("QTI VIDEO_CC SM8150 Driver");
MODULE_LICENSE("GPL v2");

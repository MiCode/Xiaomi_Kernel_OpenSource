// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include <dt-bindings/clock/qcom,videocc-lito.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "common.h"
#include "gdsc.h"
#include "reset.h"
#include "vdd-level-lito.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);

#define IRIS_DISABLE_MULTIPIPE	21
#define IRIS_DISABLE_VP_FMAX	27

enum {
	P_BI_TCXO,
	P_CHIP_SLEEP_CLK,
	P_CORE_BI_PLL_TEST_SE,
	P_VIDEO_PLL0_OUT_EVEN,
	P_VIDEO_PLL0_OUT_MAIN,
	P_VIDEO_PLL0_OUT_ODD,
};

static const struct parent_map video_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_VIDEO_PLL0_OUT_MAIN, 1 },
	{ P_VIDEO_PLL0_OUT_EVEN, 2 },
	{ P_VIDEO_PLL0_OUT_ODD, 3 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const video_cc_parent_names_0[] = {
	"bi_tcxo",
	"video_pll0",
	"video_pll0_out_even",
	"video_pll0_out_odd",
	"core_bi_pll_test_se",
};

static const struct parent_map video_cc_parent_map_1[] = {
	{ P_CHIP_SLEEP_CLK, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const video_cc_parent_names_1[] = {
	"chip_sleep_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map video_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const video_cc_parent_names_2_ao[] = {
	"bi_tcxo_ao",
	"core_bi_pll_test_se",
};

static struct pll_vco lucid_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static struct alpha_pll_config video_pll0_config = {
	.l = 0x19,
	.cal_l = 0x44,
	.alpha = 0x0,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
};

static struct clk_alpha_pll video_pll0 = {
	.offset = 0x42c,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "video_pll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ops,
			.vdd_class = &vdd_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000},
		},
	},
};

static const struct freq_tbl ftbl_video_cc_iris_clk_src[] = {
	F(240000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(338000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(365000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(444000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(533000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_video_cc_iris_multipipe_clk_src[] = {
	F(200000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
};

static const struct freq_tbl ftbl_video_cc_iris_fmax_clk_src[] = {
	F(240000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(338000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(365000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
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
		.parent_names = video_cc_parent_names_0,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 240000000,
			[VDD_LOW] = 338000000,
			[VDD_LOW_L1] = 365000000,
			[VDD_NOMINAL] = 444000000,
			[VDD_HIGH] = 533000000},
	},
};

static const struct freq_tbl ftbl_video_cc_sleep_clk_src[] = {
	F(32000, P_CHIP_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_sleep_clk_src = {
	.cmd_rcgr = 0x984,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_1,
	.freq_tbl = ftbl_video_cc_sleep_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_sleep_clk_src",
		.parent_names = video_cc_parent_names_1,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 32000},
	},
};

static const struct freq_tbl ftbl_video_cc_xo_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_xo_clk_src = {
	.cmd_rcgr = 0x960,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_2,
	.freq_tbl = ftbl_video_cc_xo_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_xo_clk_src",
		.parent_names = video_cc_parent_names_2_ao,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_branch video_cc_apb_clk = {
	.halt_reg = 0xa4c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa4c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_apb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs0_axi_clk = {
	.halt_reg = 0x9ec,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9ec,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs0_axi_clk",
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
			.parent_names = (const char *[]){
				"video_cc_iris_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs1_axi_clk = {
	.halt_reg = 0xa0c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa0c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs1_axi_clk",
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
			.parent_names = (const char *[]){
				"video_cc_iris_clk_src",
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
			.parent_names = (const char *[]){
				"video_cc_iris_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvsc_ctl_axi_clk = {
	.halt_reg = 0x9cc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9cc,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvsc_ctl_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_sleep_clk = {
	.halt_reg = 0x9a4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9a4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_sleep_clk",
			.parent_names = (const char *[]){
				"video_cc_sleep_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_venus_ahb_clk = {
	.halt_reg = 0xa6c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa6c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_venus_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_xo_clk = {
	.halt_reg = 0x980,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x980,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_xo_clk",
			.parent_names = (const char *[]){
				"video_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *video_cc_lito_clocks[] = {
	[VIDEO_PLL0] = &video_pll0.clkr,
	[VIDEO_CC_APB_CLK] = &video_cc_apb_clk.clkr,
	[VIDEO_CC_IRIS_CLK_SRC] = &video_cc_iris_clk_src.clkr,
	[VIDEO_CC_MVS0_AXI_CLK] = &video_cc_mvs0_axi_clk.clkr,
	[VIDEO_CC_MVS0_CORE_CLK] = &video_cc_mvs0_core_clk.clkr,
	[VIDEO_CC_MVS1_AXI_CLK] = &video_cc_mvs1_axi_clk.clkr,
	[VIDEO_CC_MVS1_CORE_CLK] = &video_cc_mvs1_core_clk.clkr,
	[VIDEO_CC_MVSC_CORE_CLK] = &video_cc_mvsc_core_clk.clkr,
	[VIDEO_CC_MVSC_CTL_AXI_CLK] = &video_cc_mvsc_ctl_axi_clk.clkr,
	[VIDEO_CC_SLEEP_CLK] = &video_cc_sleep_clk.clkr,
	[VIDEO_CC_SLEEP_CLK_SRC] = &video_cc_sleep_clk_src.clkr,
	[VIDEO_CC_VENUS_AHB_CLK] = &video_cc_venus_ahb_clk.clkr,
	[VIDEO_CC_XO_CLK] = &video_cc_xo_clk.clkr,
	[VIDEO_CC_XO_CLK_SRC] = &video_cc_xo_clk_src.clkr,
};

static const struct qcom_reset_map video_cc_lito_resets[] = {
	[VIDEO_CC_INTERFACE_BCR] = { 0x9ac },
	[VIDEO_CC_MVS0_BCR] = { 0x870 },
	[VIDEO_CC_MVS1_BCR] = { 0x8b0 },
	[VIDEO_CC_MVSC_BCR] = { 0x810 },
};

static const struct regmap_config video_cc_lito_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xb94,
	.fast_io = true,
};

static const struct qcom_cc_desc video_cc_lito_desc = {
	.config = &video_cc_lito_regmap_config,
	.clks = video_cc_lito_clocks,
	.num_clks = ARRAY_SIZE(video_cc_lito_clocks),
	.resets = video_cc_lito_resets,
	.num_resets = ARRAY_SIZE(video_cc_lito_resets),
};

static const struct of_device_id video_cc_lito_match_table[] = {
	{ .compatible = "qcom,lito-videocc" },
	{ }
};
MODULE_DEVICE_TABLE(of, video_cc_lito_match_table);

static int video_multipipe_fixup(struct platform_device *pdev,
				struct regmap *regmap)
{
	u32 val, val_fmax;
	int ret;

	ret = nvmem_cell_read_u32(&pdev->dev, "iris-bin", &val);
	if (ret)
		return ret;

	val_fmax = (val >> IRIS_DISABLE_VP_FMAX) & 0x1;
	val = (val >> IRIS_DISABLE_MULTIPIPE) & 0x1;

	if (val) {
		video_pll0_config.l = 0x14;
		video_cc_iris_clk_src.freq_tbl =
				ftbl_video_cc_iris_multipipe_clk_src;
		video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_LOWER] =
				200000000;
		video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_LOW] =
				200000000;
		video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_LOW_L1] =
				200000000;
		video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_NOMINAL] =
				200000000;
		video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_HIGH] =
				200000000;
		return 0;
	}

	if (val_fmax) {
		video_pll0_config.l = 0x14;
		video_cc_iris_clk_src.freq_tbl =
				ftbl_video_cc_iris_fmax_clk_src;
		video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_NOMINAL] =
				365000000;
		video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_HIGH] =
				365000000;
	}

	return 0;
}

static int video_cc_lito_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	struct clk *clk;
	int ret;

	regmap = qcom_cc_map(pdev, &video_cc_lito_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the Video CC registers\n");
		return PTR_ERR(regmap);
	}

	clk = devm_clk_get(&pdev->dev, "cfg_ahb_clk");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get ahb clock handle\n");
		return PTR_ERR(clk);
	}
	devm_clk_put(&pdev->dev, clk);

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (PTR_ERR(vdd_cx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	ret = video_multipipe_fixup(pdev, regmap);
	if (ret)
		return ret;

	clk_lucid_pll_configure(&video_pll0, regmap, &video_pll0_config);

	ret = qcom_cc_really_probe(pdev, &video_cc_lito_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register Video CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Video CC clocks\n");

	return 0;
}

static struct platform_driver video_cc_lito_driver = {
	.probe = video_cc_lito_probe,
	.driver = {
		.name = "lito-videocc",
		.of_match_table = video_cc_lito_match_table,
	},
};

static int __init video_cc_lito_init(void)
{
	return platform_driver_register(&video_cc_lito_driver);
}
subsys_initcall(video_cc_lito_init);

static void __exit video_cc_lito_exit(void)
{
	platform_driver_unregister(&video_cc_lito_driver);
}
module_exit(video_cc_lito_exit);

MODULE_DESCRIPTION("QTI VIDEO_CC LITO Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:video_cc-lito");

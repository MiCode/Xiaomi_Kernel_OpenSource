/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,videocc-sdmmagpie.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "clk-alpha-pll.h"
#include "vdd-level-sdmmagpie.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
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

static const struct parent_map video_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const video_cc_parent_names_2[] = {
	"bi_tcxo_ao",
	"core_bi_pll_test_se",
};

static struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
	{ 125000000, 1000000000, 1 },
};

static const struct alpha_pll_config video_pll0_config = {
	.l = 0x19,
	.frac = 0x0,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002067,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00004805,
	.test_ctl_hi_val = 0x40000000,
};

static struct clk_alpha_pll video_pll0 = {
	.offset = 0x42c,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "video_pll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_fabia_pll_ops,
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

static const struct freq_tbl ftbl_video_cc_xo_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_xo_clk_src = {
	.cmd_rcgr = 0xa98,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_2,
	.freq_tbl = ftbl_video_cc_xo_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_xo_clk_src",
		.parent_names = video_cc_parent_names_2,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
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
			.parent_names = (const char *[]){
				"video_cc_iris_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
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
	.halt_reg = 0x984,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x984,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_xo_clk",
			.parent_names = (const char *[]){
				"video_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *video_cc_sdmmagpie_clocks[] = {
	[VIDEO_PLL0] = &video_pll0.clkr,
	[VIDEO_CC_IRIS_AHB_CLK] = &video_cc_iris_ahb_clk.clkr,
	[VIDEO_CC_IRIS_CLK_SRC] = &video_cc_iris_clk_src.clkr,
	[VIDEO_CC_MVS0_AXI_CLK] = &video_cc_mvs0_axi_clk.clkr,
	[VIDEO_CC_MVS0_CORE_CLK] = &video_cc_mvs0_core_clk.clkr,
	[VIDEO_CC_MVS1_AXI_CLK] = &video_cc_mvs1_axi_clk.clkr,
	[VIDEO_CC_MVS1_CORE_CLK] = &video_cc_mvs1_core_clk.clkr,
	[VIDEO_CC_MVSC_CORE_CLK] = &video_cc_mvsc_core_clk.clkr,
	[VIDEO_CC_MVSC_CTL_AXI_CLK] = &video_cc_mvsc_ctl_axi_clk.clkr,
	[VIDEO_CC_VENUS_AHB_CLK] = &video_cc_venus_ahb_clk.clkr,
	[VIDEO_CC_XO_CLK] = &video_cc_xo_clk.clkr,
	[VIDEO_CC_XO_CLK_SRC] = &video_cc_xo_clk_src.clkr,
};

static const struct regmap_config video_cc_sdmmagpie_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0xb94,
	.fast_io	= true,
};

static const struct qcom_cc_desc video_cc_sdmmagpie_desc = {
	.config = &video_cc_sdmmagpie_regmap_config,
	.clks = video_cc_sdmmagpie_clocks,
	.num_clks = ARRAY_SIZE(video_cc_sdmmagpie_clocks),
};

static const struct of_device_id video_cc_sdmmagpie_match_table[] = {
	{ .compatible = "qcom,videocc-sdmmagpie" },
	{ }
};
MODULE_DEVICE_TABLE(of, video_cc_sdmmagpie_match_table);

static int video_cc_sdmmagpie_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (PTR_ERR(vdd_cx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
					"Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	regmap = qcom_cc_map(pdev, &video_cc_sdmmagpie_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the video_cc registers\n");
		return PTR_ERR(regmap);
	}

	clk_fabia_pll_configure(&video_pll0, regmap, &video_pll0_config);

	ret = qcom_cc_really_probe(pdev, &video_cc_sdmmagpie_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register Video CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Video CC clocks\n");
	return ret;
}

static struct platform_driver video_cc_sdmmagpie_driver = {
	.probe = video_cc_sdmmagpie_probe,
	.driver = {
		.name = "video_cc-sdmmagpie",
		.of_match_table = video_cc_sdmmagpie_match_table,
	},
};

static int __init video_cc_sdmmagpie_init(void)
{
	return platform_driver_register(&video_cc_sdmmagpie_driver);
}
subsys_initcall(video_cc_sdmmagpie_init);

static void __exit video_cc_sdmmagpie_exit(void)
{
	platform_driver_unregister(&video_cc_sdmmagpie_driver);
}
module_exit(video_cc_sdmmagpie_exit);

MODULE_DESCRIPTION("QTI VIDEO_CC SDMMAGPIE Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:video_cc-sdmmagpie");

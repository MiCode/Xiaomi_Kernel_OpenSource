/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <linux/reset-controller.h>

#include <dt-bindings/clock/qcom,videocc-sdm845.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "reset.h"
#include "clk-alpha-pll.h"
#include "vdd-level-sdm845.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_CX_NUM, 1, vdd_corner);

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
	{ P_CORE_BI_PLL_TEST_SE, 4 },
};

static const char * const video_cc_parent_names_0[] = {
	"bi_tcxo",
	"video_pll0",
	"video_pll0_out_even",
	"video_pll0_out_odd",
	"core_bi_pll_test_se",
};

static struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
	{ 125000000, 1000000000, 1 },
};

static const struct pll_config video_pll0_config = {
	.l = 0x10,
	.frac = 0xaaab,
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
			VDD_CX_FMAX_MAP4(
				MIN, 615000000,
				LOW, 1066000000,
				LOW_L1, 1600000000,
				NOMINAL, 2000000000),
		},
	},
};

static const struct freq_tbl ftbl_video_cc_venus_clk_src[] = {
	F(100000000, P_VIDEO_PLL0_OUT_MAIN, 4, 0, 0),
	F(200000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(320000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(380000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(444000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(533000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_video_cc_venus_clk_src_sdm845_v2[] = {
	F(100000000, P_VIDEO_PLL0_OUT_MAIN, 4, 0, 0),
	F(200000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(330000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(404000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(444000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(533000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_video_cc_venus_clk_src_sdm670[] = {
	F(100000000, P_VIDEO_PLL0_OUT_MAIN, 4, 0, 0),
	F(200000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(330000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(364700000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(404000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(444000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(533000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_venus_clk_src = {
	.cmd_rcgr = 0x7f0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_0,
	.freq_tbl = ftbl_video_cc_venus_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_venus_clk_src",
		.parent_names = video_cc_parent_names_0,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP6(
			MIN, 100000000,
			LOWER, 200000000,
			LOW, 320000000,
			LOW_L1, 380000000,
			NOMINAL, 444000000,
			HIGH, 533000000),
	},
};

static struct clk_branch video_cc_apb_clk = {
	.halt_reg = 0x990,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x990,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_apb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_at_clk = {
	.halt_reg = 0x9f0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9f0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_at_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_qdss_trig_clk = {
	.halt_reg = 0x970,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x970,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_qdss_trig_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_qdss_tsctr_div8_clk = {
	.halt_reg = 0x9d0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9d0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_qdss_tsctr_div8_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_vcodec0_axi_clk = {
	.halt_reg = 0x930,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x930,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_vcodec0_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_vcodec0_core_clk = {
	.halt_reg = 0x890,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x890,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_vcodec0_core_clk",
			.parent_names = (const char *[]){
				"video_cc_venus_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_vcodec1_axi_clk = {
	.halt_reg = 0x950,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x950,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_vcodec1_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_vcodec1_core_clk = {
	.halt_reg = 0x8d0,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x8d0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_vcodec1_core_clk",
			.parent_names = (const char *[]){
				"video_cc_venus_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_venus_ahb_clk = {
	.halt_reg = 0x9b0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9b0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_venus_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_venus_ctl_axi_clk = {
	.halt_reg = 0x910,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x910,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_venus_ctl_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_venus_ctl_core_clk = {
	.halt_reg = 0x850,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x850,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_venus_ctl_core_clk",
			.parent_names = (const char *[]){
				"video_cc_venus_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *video_cc_sdm845_clocks[] = {
	[VIDEO_CC_APB_CLK] = &video_cc_apb_clk.clkr,
	[VIDEO_CC_AT_CLK] = &video_cc_at_clk.clkr,
	[VIDEO_CC_QDSS_TRIG_CLK] = &video_cc_qdss_trig_clk.clkr,
	[VIDEO_CC_QDSS_TSCTR_DIV8_CLK] = &video_cc_qdss_tsctr_div8_clk.clkr,
	[VIDEO_CC_VCODEC0_AXI_CLK] = &video_cc_vcodec0_axi_clk.clkr,
	[VIDEO_CC_VCODEC0_CORE_CLK] = &video_cc_vcodec0_core_clk.clkr,
	[VIDEO_CC_VCODEC1_AXI_CLK] = &video_cc_vcodec1_axi_clk.clkr,
	[VIDEO_CC_VCODEC1_CORE_CLK] = &video_cc_vcodec1_core_clk.clkr,
	[VIDEO_CC_VENUS_AHB_CLK] = &video_cc_venus_ahb_clk.clkr,
	[VIDEO_CC_VENUS_CLK_SRC] = &video_cc_venus_clk_src.clkr,
	[VIDEO_CC_VENUS_CTL_AXI_CLK] = &video_cc_venus_ctl_axi_clk.clkr,
	[VIDEO_CC_VENUS_CTL_CORE_CLK] = &video_cc_venus_ctl_core_clk.clkr,
	[VIDEO_PLL0] = &video_pll0.clkr,
};

static const struct regmap_config video_cc_sdm845_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0xb90,
	.fast_io	= true,
};

static const struct qcom_cc_desc video_cc_sdm845_desc = {
	.config = &video_cc_sdm845_regmap_config,
	.clks = video_cc_sdm845_clocks,
	.num_clks = ARRAY_SIZE(video_cc_sdm845_clocks),
};

static const struct of_device_id video_cc_sdm845_match_table[] = {
	{ .compatible = "qcom,video_cc-sdm845" },
	{ .compatible = "qcom,video_cc-sdm845-v2" },
	{ .compatible = "qcom,video_cc-sdm670" },
	{ }
};
MODULE_DEVICE_TABLE(of, video_cc_sdm845_match_table);

static void video_cc_sdm845_fixup_sdm845v2(void)
{
	video_cc_venus_clk_src.freq_tbl = ftbl_video_cc_venus_clk_src_sdm845_v2;
	video_cc_venus_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 330000000;
	video_cc_venus_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW_L1] =
		404000000;
}

static void video_cc_sdm845_fixup_sdm670(void)
{
	video_cc_venus_clk_src.freq_tbl = ftbl_video_cc_venus_clk_src_sdm670;
	video_cc_venus_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 330000000;
	video_cc_venus_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW_L1] =
		404000000;
}

static int video_cc_sdm845_fixup(struct platform_device *pdev)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,video_cc-sdm845-v2"))
		video_cc_sdm845_fixup_sdm845v2();
	else if (!strcmp(compat, "qcom,video_cc-sdm670"))
		video_cc_sdm845_fixup_sdm670();

	return 0;
}

static int video_cc_sdm845_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret = 0;

	regmap = qcom_cc_map(pdev, &video_cc_sdm845_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the Video CC registers\n");
		return PTR_ERR(regmap);
	}

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	ret = video_cc_sdm845_fixup(pdev);
	if (ret)
		return ret;

	clk_fabia_pll_configure(&video_pll0, regmap, &video_pll0_config);

	ret = qcom_cc_really_probe(pdev, &video_cc_sdm845_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register Video CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Video CC clocks\n");
	return ret;
}

static struct platform_driver video_cc_sdm845_driver = {
	.probe		= video_cc_sdm845_probe,
	.driver		= {
		.name	= "video_cc-sdm845",
		.of_match_table = video_cc_sdm845_match_table,
	},
};

static int __init video_cc_sdm845_init(void)
{
	return platform_driver_register(&video_cc_sdm845_driver);
}
subsys_initcall(video_cc_sdm845_init);

static void __exit video_cc_sdm845_exit(void)
{
	platform_driver_unregister(&video_cc_sdm845_driver);
}
module_exit(video_cc_sdm845_exit);

MODULE_DESCRIPTION("QCOM VIDEO_CC SDM845 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:video_cc-sdm845");

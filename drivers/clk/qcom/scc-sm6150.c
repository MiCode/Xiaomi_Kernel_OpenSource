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
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,scc-sm6150.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "clk-alpha-pll.h"
#include "vdd-level-sm6150.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGULATORS(vdd_scc_cx, VDD_NUM, 1, vdd_corner);

enum {
	P_AON_SLEEP_CLK,
	P_AOSS_CC_RO_CLK,
	P_CORE_PI_CXO_CLK,
	P_QDSP6SS_PLL_OUT_AUX,
	P_SCC_PLL_OUT_AUX,
	P_SCC_PLL_OUT_AUX2,
	P_SCC_PLL_OUT_EARLY,
	P_SSC_BI_PLL_TEST_SE,
};

static const struct parent_map scc_parent_map_0[] = {
	{ P_AOSS_CC_RO_CLK, 0 },
	{ P_AON_SLEEP_CLK, 1 },
	{ P_SCC_PLL_OUT_AUX2, 2 },
	{ P_CORE_PI_CXO_CLK, 3 },
	{ P_SCC_PLL_OUT_AUX, 4 },
	{ P_QDSP6SS_PLL_OUT_AUX, 5 },
	{ P_SCC_PLL_OUT_EARLY, 6 },
	{ P_SSC_BI_PLL_TEST_SE, 7 },
};

static const char * const scc_parent_names_0[] = {
	"bi_tcxo",
	"aon_sleep_clk",
	"scc_pll_out_aux2",
	"bi_tcxo",
	"scc_pll_out_aux",
	"qdsp6ss_pll_out_aux",
	"scc_pll",
	"ssc_bi_pll_test_se",
};

static struct pll_vco scc_pll_vco[] = {
	{ 500000000, 1000000000, 2 },
};

/* 600MHz configuration */
static const struct alpha_pll_config scc_pll_config = {
	.l = 0x1F,
	.alpha_u = 0x40,
	.alpha_en_mask = BIT(24),
	.vco_val = 0x2 << 20,
	.vco_mask = 0x3 << 20,
	.post_div_val = 0x3 << 15,
	.post_div_mask = 0x7 << 15,
	.aux_output_mask = BIT(1),
	.aux2_output_mask = BIT(2),
	.config_ctl_val = 0x4001055b,
	.test_ctl_hi_mask = 0x1,
};

static struct clk_alpha_pll scc_pll_out_aux2 = {
	.offset = 0x0,
	.vco_table = scc_pll_vco,
	.num_vco = ARRAY_SIZE(scc_pll_vco),
	.clkr.hw.init = &(struct clk_init_data){
		.name = "scc_pll_out_aux2",
		.parent_names = (const char *[]){ "bi_tcxo" },
		.num_parents = 1,
		.ops = &clk_alpha_pll_ops,
		.vdd_class = &vdd_scc_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 1000000000,
			[VDD_NOMINAL] = 2000000000},
	},
};

static struct clk_alpha_pll_postdiv scc_pll_out_aux = {
	.offset = 0x0,
	.width = 2,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "scc_pll_out_aux",
		.parent_names = (const char *[]){ "scc_pll_out_aux2" },
		.num_parents = 1,
		.ops = &clk_alpha_pll_postdiv_ops,
	},
};

static const struct freq_tbl ftbl_scc_main_rcg_clk_src[] = {
	F(100000000, P_SCC_PLL_OUT_AUX, 2, 0, 0),
	F(200000000, P_SCC_PLL_OUT_AUX, 1, 0, 0),
	F(300000000, P_SCC_PLL_OUT_AUX2, 2, 0, 0),
	{ }
};

static struct clk_rcg2 scc_main_rcg_clk_src = {
	.cmd_rcgr = 0x1000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = scc_parent_map_0,
	.freq_tbl = ftbl_scc_main_rcg_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "scc_main_rcg_clk_src",
		.parent_names = scc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_scc_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 300000000,
			[VDD_LOW] = 600000000},
	},
};

static const struct freq_tbl ftbl_scc_qupv3_se0_clk_src[] = {
	F(7372800, P_SCC_PLL_OUT_AUX, 1, 576, 15625),
	F(14745600, P_SCC_PLL_OUT_AUX, 1, 1152, 15625),
	F(19200000, P_AOSS_CC_RO_CLK, 1, 0, 0),
	F(29491200, P_SCC_PLL_OUT_AUX, 1, 2304, 15625),
	F(32000000, P_SCC_PLL_OUT_AUX, 1, 4, 25),
	F(48000000, P_SCC_PLL_OUT_AUX, 1, 6, 25),
	F(64000000, P_SCC_PLL_OUT_AUX, 1, 8, 25),
	F(80000000, P_SCC_PLL_OUT_AUX, 1, 2, 5),
	F(96000000, P_SCC_PLL_OUT_AUX, 1, 12, 25),
	F(100000000, P_SCC_PLL_OUT_AUX, 2, 0, 0),
	F(102400000, P_SCC_PLL_OUT_AUX, 1, 64, 125),
	F(112000000, P_SCC_PLL_OUT_AUX, 1, 14, 25),
	F(117964800, P_SCC_PLL_OUT_AUX, 1, 9216, 15625),
	F(120000000, P_SCC_PLL_OUT_AUX, 1, 3, 5),
	F(128000000, P_SCC_PLL_OUT_AUX, 1, 16, 25),
	{ }
};

static struct clk_rcg2 scc_qupv3_se0_clk_src = {
	.cmd_rcgr = 0x2004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = scc_parent_map_0,
	.freq_tbl = ftbl_scc_qupv3_se0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "scc_qupv3_se0_clk_src",
		.parent_names = scc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_scc_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 50000000,
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 150000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static struct clk_rcg2 scc_qupv3_se1_clk_src = {
	.cmd_rcgr = 0x3004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = scc_parent_map_0,
	.freq_tbl = ftbl_scc_qupv3_se0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "scc_qupv3_se1_clk_src",
		.parent_names = scc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_scc_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 50000000,
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 150000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static struct clk_rcg2 scc_qupv3_se2_clk_src = {
	.cmd_rcgr = 0x4004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = scc_parent_map_0,
	.freq_tbl = ftbl_scc_qupv3_se0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "scc_qupv3_se2_clk_src",
		.parent_names = scc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_scc_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 50000000,
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 150000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static struct clk_rcg2 scc_qupv3_se3_clk_src = {
	.cmd_rcgr = 0xb004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = scc_parent_map_0,
	.freq_tbl = ftbl_scc_qupv3_se0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "scc_qupv3_se3_clk_src",
		.parent_names = scc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_scc_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 50000000,
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 150000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static struct clk_rcg2 scc_qupv3_se4_clk_src = {
	.cmd_rcgr = 0xc004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = scc_parent_map_0,
	.freq_tbl = ftbl_scc_qupv3_se0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "scc_qupv3_se4_clk_src",
		.parent_names = scc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_scc_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 50000000,
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 150000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static struct clk_rcg2 scc_qupv3_se5_clk_src = {
	.cmd_rcgr = 0xd004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = scc_parent_map_0,
	.freq_tbl = ftbl_scc_qupv3_se0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "scc_qupv3_se5_clk_src",
		.parent_names = scc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_scc_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 50000000,
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 150000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static struct clk_branch scc_qupv3_2xcore_clk = {
	.halt_reg = 0x5008,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(10),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_2xcore_clk",
			.parent_names = (const char *[]){
				"scc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch scc_qupv3_core_clk = {
	.halt_reg = 0x5010,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(11),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_core_clk",
			.parent_names = (const char *[]){
				"scc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch scc_qupv3_m_hclk_clk = {
	.halt_reg = 0x9070,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x9064,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_m_hclk_clk",
			.parent_names = (const char *[]){
				"scc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch scc_qupv3_s_hclk_clk = {
	.halt_reg = 0x906c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x9060,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_s_hclk_clk",
			.parent_names = (const char *[]){
				"scc_main_rcg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch scc_qupv3_se0_clk = {
	.halt_reg = 0x2130,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(3),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_se0_clk",
			.parent_names = (const char *[]){
				"scc_qupv3_se0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch scc_qupv3_se1_clk = {
	.halt_reg = 0x3130,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_se1_clk",
			.parent_names = (const char *[]){
				"scc_qupv3_se1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch scc_qupv3_se2_clk = {
	.halt_reg = 0x4130,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(5),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_se2_clk",
			.parent_names = (const char *[]){
				"scc_qupv3_se2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch scc_qupv3_se3_clk = {
	.halt_reg = 0xb130,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(6),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_se3_clk",
			.parent_names = (const char *[]){
				"scc_qupv3_se3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch scc_qupv3_se4_clk = {
	.halt_reg = 0xc130,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(7),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_se4_clk",
			.parent_names = (const char *[]){
				"scc_qupv3_se4_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch scc_qupv3_se5_clk = {
	.halt_reg = 0xd130,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(8),
		.hw.init = &(struct clk_init_data){
			.name = "scc_qupv3_se5_clk",
			.parent_names = (const char *[]){
				"scc_qupv3_se5_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *scc_sm6150_clocks[] = {
	[SCC_PLL_OUT_AUX2] = &scc_pll_out_aux2.clkr,
	[SCC_PLL_OUT_AUX] = &scc_pll_out_aux.clkr,
	[SCC_MAIN_RCG_CLK_SRC] = &scc_main_rcg_clk_src.clkr,
	[SCC_QUPV3_2XCORE_CLK] = &scc_qupv3_2xcore_clk.clkr,
	[SCC_QUPV3_CORE_CLK] = &scc_qupv3_core_clk.clkr,
	[SCC_QUPV3_M_HCLK_CLK] = &scc_qupv3_m_hclk_clk.clkr,
	[SCC_QUPV3_S_HCLK_CLK] = &scc_qupv3_s_hclk_clk.clkr,
	[SCC_QUPV3_SE0_CLK] = &scc_qupv3_se0_clk.clkr,
	[SCC_QUPV3_SE0_CLK_SRC] = &scc_qupv3_se0_clk_src.clkr,
	[SCC_QUPV3_SE1_CLK] = &scc_qupv3_se1_clk.clkr,
	[SCC_QUPV3_SE1_CLK_SRC] = &scc_qupv3_se1_clk_src.clkr,
	[SCC_QUPV3_SE2_CLK] = &scc_qupv3_se2_clk.clkr,
	[SCC_QUPV3_SE2_CLK_SRC] = &scc_qupv3_se2_clk_src.clkr,
	[SCC_QUPV3_SE3_CLK] = &scc_qupv3_se3_clk.clkr,
	[SCC_QUPV3_SE3_CLK_SRC] = &scc_qupv3_se3_clk_src.clkr,
	[SCC_QUPV3_SE4_CLK] = &scc_qupv3_se4_clk.clkr,
	[SCC_QUPV3_SE4_CLK_SRC] = &scc_qupv3_se4_clk_src.clkr,
	[SCC_QUPV3_SE5_CLK] = &scc_qupv3_se5_clk.clkr,
	[SCC_QUPV3_SE5_CLK_SRC] = &scc_qupv3_se5_clk_src.clkr,
};

static struct clk_dfs scc_dfs_clocks[] = {
	{ &scc_qupv3_se0_clk_src, DFS_ENABLE_RCG },
	{ &scc_qupv3_se1_clk_src, DFS_ENABLE_RCG },
	{ &scc_qupv3_se2_clk_src, DFS_ENABLE_RCG },
	{ &scc_qupv3_se3_clk_src, DFS_ENABLE_RCG },
	{ &scc_qupv3_se4_clk_src, DFS_ENABLE_RCG },
	{ &scc_qupv3_se5_clk_src, DFS_ENABLE_RCG },
};

static const struct qcom_cc_dfs_desc scc_sm6150_dfs_desc = {
	.clks = scc_dfs_clocks,
	.num_clks = ARRAY_SIZE(scc_dfs_clocks),
};

static const struct regmap_config scc_sm6150_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x30000,
	.fast_io	= true,
};

static const struct qcom_cc_desc scc_sm6150_desc = {
	.config = &scc_sm6150_regmap_config,
	.clks = scc_sm6150_clocks,
	.num_clks = ARRAY_SIZE(scc_sm6150_clocks),
};

static const struct of_device_id scc_sm6150_match_table[] = {
	{ .compatible = "qcom,scc-sm6150" },
	{ }
};
MODULE_DEVICE_TABLE(of, scc_sm6150_match_table);

static int scc_sm6150_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	vdd_scc_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_scc_cx");
	if (IS_ERR(vdd_scc_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_scc_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_scc_cx regulator\n");
		return PTR_ERR(vdd_scc_cx.regulator[0]);
	}

	regmap = qcom_cc_map(pdev, &scc_sm6150_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the scc registers\n");
		return PTR_ERR(regmap);
	}

	clk_alpha_pll_configure(&scc_pll_out_aux2, regmap, &scc_pll_config);

	ret = qcom_cc_really_probe(pdev, &scc_sm6150_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register SCC clocks\n");
		return ret;
	}

	/* DFS clock registration */
	ret = qcom_cc_register_rcg_dfs(pdev, &scc_sm6150_dfs_desc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register with DFS!\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered SCC clocks\n");

	return ret;
}

static struct platform_driver scc_sm6150_driver = {
	.probe		= scc_sm6150_probe,
	.driver		= {
		.name = "scc-sm6150",
		.of_match_table = scc_sm6150_match_table,
	},
};

static int __init scc_sm6150_init(void)
{
	return platform_driver_register(&scc_sm6150_driver);
}
subsys_initcall(scc_sm6150_init);

static void __exit scc_sm6150_exit(void)
{
	platform_driver_unregister(&scc_sm6150_driver);
}
module_exit(scc_sm6150_exit);

MODULE_DESCRIPTION("QTI SCC SM6150 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:scc-sm6150");

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,dispcc-scuba.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_CORE_BI_PLL_TEST_SE,
	P_DISP_CC_PLL0_OUT_MAIN,
	P_DSI0_PHY_PLL_OUT_BYTECLK,
	P_DSI0_PHY_PLL_OUT_DSICLK,
	P_DSI1_PHY_PLL_OUT_DSICLK,
	P_GPLL0_OUT_MAIN,
	P_SLEEP_CLK,
};

static const struct parent_map disp_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_BYTECLK, 1 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_0[] = {
	"bi_tcxo",
	"dsi0_phy_pll_out_byteclk",
	"core_bi_pll_test_se",
};

static const struct parent_map disp_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_1[] = {
	"bi_tcxo",
	"core_bi_pll_test_se",
};
static const char * const disp_cc_parent_names_1_ao[] = {
	"bi_tcxo_ao",
	"core_bi_pll_test_se",
};

static const struct parent_map disp_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 4 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_2[] = {
	"bi_tcxo",
	"gcc_disp_gpll0_div_clk_src",
	"core_bi_pll_test_se",
};

static const struct parent_map disp_cc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_DISP_CC_PLL0_OUT_MAIN, 1 },
	{ P_GPLL0_OUT_MAIN, 4 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_3[] = {
	"bi_tcxo",
	"disp_cc_pll0",
	"gpll0",
	"core_bi_pll_test_se",
};

static const struct parent_map disp_cc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_DSICLK, 1 },
	{ P_DSI1_PHY_PLL_OUT_DSICLK, 2 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_4[] = {
	"bi_tcxo",
	"dsi0_phy_pll_out_dsiclk",
	"dsi1_phy_pll_out_dsiclk",
	"core_bi_pll_test_se",
};

static const struct parent_map disp_cc_parent_map_5[] = {
	{ P_SLEEP_CLK, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_5[] = {
	"chip_sleep_clk",
	"core_bi_pll_test_se",
};

static struct pll_vco spark_vco[] = {
	{ 500000000, 1000000000, 2 },
};

/* 768MHz configuration */
static const struct alpha_pll_config disp_cc_pll0_config = {
	.l = 0x28,
	.alpha = 0x0,
	.alpha_en_mask = BIT(24),
	.vco_val = 0x2 << 20,
	.vco_mask = GENMASK(21, 20),
	.main_output_mask = BIT(0),
	.config_ctl_val = 0x4001055B,
	.test_ctl_hi1_val = 0x1,
	.test_ctl_hi_mask = 0x1,
};

static struct clk_alpha_pll disp_cc_pll0 = {
	.offset = 0x0,
	.vco_table = spark_vco,
	.num_vco = ARRAY_SIZE(spark_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_DEFAULT],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_pll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			.vdd_class = &vdd_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 1000000000,
				[VDD_NOMINAL] = 2000000000},
		},
	},
};

static struct clk_regmap_div disp_cc_mdss_byte0_div_clk_src = {
	.reg = 0x20bc,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_mdss_byte0_div_clk_src",
		.parent_names =
			(const char *[]){ "disp_cc_mdss_byte0_clk_src" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_regmap_div_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(37500000, P_GPLL0_OUT_MAIN, 8, 0, 0),
	F(75000000, P_GPLL0_OUT_MAIN, 4, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_ahb_clk_src = {
	.cmd_rcgr = 0x2154,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_mdss_ahb_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_ahb_clk_src",
		.parent_names = disp_cc_parent_names_2,
		.num_parents = 3,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOW] = 37500000,
			[VDD_NOMINAL] = 75000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_byte0_clk_src = {
	.cmd_rcgr = 0x20a4,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_byte0_clk_src",
		.parent_names = disp_cc_parent_names_0,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_byte2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOWER] = 164000000,
			[VDD_LOW] = 187500000},
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_esc0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_esc0_clk_src = {
	.cmd_rcgr = 0x20c0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_mdss_esc0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_esc0_clk_src",
		.parent_names = disp_cc_parent_names_0,
		.num_parents = 3,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000},
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_mdp_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(192000000, P_DISP_CC_PLL0_OUT_MAIN, 4, 0, 0),
	F(256000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(307200000, P_DISP_CC_PLL0_OUT_MAIN, 2.5, 0, 0),
	F(384000000, P_DISP_CC_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_mdp_clk_src = {
	.cmd_rcgr = 0x2074,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_3,
	.freq_tbl = ftbl_disp_cc_mdss_mdp_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_mdp_clk_src",
		.parent_names = disp_cc_parent_names_3,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOWER] = 192000000,
			[VDD_LOW] = 256000000,
			[VDD_LOW_L1] = 307200000,
			[VDD_NOMINAL] = 384000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_pclk0_clk_src = {
	.cmd_rcgr = 0x205c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_4,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_pclk0_clk_src",
		.parent_names = disp_cc_parent_names_4,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_pixel_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOWER] = 183310056,
			[VDD_LOW] = 250000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_vsync_clk_src = {
	.cmd_rcgr = 0x208c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_mdss_esc0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_vsync_clk_src",
		.parent_names = disp_cc_parent_names_1,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000},
	},
};

static const struct freq_tbl ftbl_disp_cc_sleep_clk_src[] = {
	F(32764, P_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_sleep_clk_src = {
	.cmd_rcgr = 0x6050,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_5,
	.freq_tbl = ftbl_disp_cc_sleep_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_sleep_clk_src",
		.parent_names = disp_cc_parent_names_5,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 32764},
	},
};

static struct clk_rcg2 disp_cc_xo_clk_src = {
	.cmd_rcgr = 0x6034,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_mdss_esc0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_xo_clk_src",
		.parent_names = disp_cc_parent_names_1_ao,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_branch disp_cc_mdss_ahb_clk = {
	.halt_reg = 0x2044,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2044,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_ahb_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte0_clk = {
	.halt_reg = 0x201c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x201c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte0_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_byte0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte0_intf_clk = {
	.halt_reg = 0x2020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte0_intf_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_byte0_div_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_esc0_clk = {
	.halt_reg = 0x2024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_esc0_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_esc0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_clk = {
	.halt_reg = 0x2008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_mdp_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_mdp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_lut_clk = {
	.halt_reg = 0x2010,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x2010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_mdp_lut_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_mdp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_non_gdsc_ahb_clk = {
	.halt_reg = 0x4004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_non_gdsc_ahb_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_pclk0_clk = {
	.halt_reg = 0x2004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_pclk0_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_pclk0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_vsync_clk = {
	.halt_reg = 0x2018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_vsync_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_vsync_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_sleep_clk = {
	.halt_reg = 0x6068,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x6068,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_sleep_clk",
			.parent_names = (const char *[]){
				"disp_cc_sleep_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_xo_clk = {
	.halt_reg = 0x604c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x604c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_xo_clk",
			.parent_names = (const char *[]){
				"disp_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *disp_cc_scuba_clocks[] = {
	[DISP_CC_MDSS_AHB_CLK] = &disp_cc_mdss_ahb_clk.clkr,
	[DISP_CC_MDSS_AHB_CLK_SRC] = &disp_cc_mdss_ahb_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_CLK] = &disp_cc_mdss_byte0_clk.clkr,
	[DISP_CC_MDSS_BYTE0_CLK_SRC] = &disp_cc_mdss_byte0_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_DIV_CLK_SRC] = &disp_cc_mdss_byte0_div_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_INTF_CLK] = &disp_cc_mdss_byte0_intf_clk.clkr,
	[DISP_CC_MDSS_ESC0_CLK] = &disp_cc_mdss_esc0_clk.clkr,
	[DISP_CC_MDSS_ESC0_CLK_SRC] = &disp_cc_mdss_esc0_clk_src.clkr,
	[DISP_CC_MDSS_MDP_CLK] = &disp_cc_mdss_mdp_clk.clkr,
	[DISP_CC_MDSS_MDP_CLK_SRC] = &disp_cc_mdss_mdp_clk_src.clkr,
	[DISP_CC_MDSS_MDP_LUT_CLK] = &disp_cc_mdss_mdp_lut_clk.clkr,
	[DISP_CC_MDSS_NON_GDSC_AHB_CLK] = &disp_cc_mdss_non_gdsc_ahb_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK] = &disp_cc_mdss_pclk0_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK_SRC] = &disp_cc_mdss_pclk0_clk_src.clkr,
	[DISP_CC_MDSS_VSYNC_CLK] = &disp_cc_mdss_vsync_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK_SRC] = &disp_cc_mdss_vsync_clk_src.clkr,
	[DISP_CC_PLL0] = &disp_cc_pll0.clkr,
	[DISP_CC_SLEEP_CLK] = &disp_cc_sleep_clk.clkr,
	[DISP_CC_SLEEP_CLK_SRC] = &disp_cc_sleep_clk_src.clkr,
	[DISP_CC_XO_CLK] = &disp_cc_xo_clk.clkr,
	[DISP_CC_XO_CLK_SRC] = &disp_cc_xo_clk_src.clkr,
};

static const struct regmap_config disp_cc_scuba_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x10000,
	.fast_io = true,
};

static const struct qcom_cc_desc disp_cc_scuba_desc = {
	.config = &disp_cc_scuba_regmap_config,
	.clks = disp_cc_scuba_clocks,
	.num_clks = ARRAY_SIZE(disp_cc_scuba_clocks),
};

static const struct of_device_id disp_cc_scuba_match_table[] = {
	{ .compatible = "qcom,scuba-dispcc" },
	{ }
};
MODULE_DEVICE_TABLE(of, disp_cc_scuba_match_table);

static int disp_cc_scuba_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	struct clk *clk;
	int ret;

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (PTR_ERR(vdd_cx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	regmap = qcom_cc_map(pdev, &disp_cc_scuba_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	clk = clk_get(&pdev->dev, "cfg_ahb_clk");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get ahb clock handle\n");
		return PTR_ERR(clk);
	}
	clk_put(clk);

	clk_alpha_pll_configure(&disp_cc_pll0, regmap, &disp_cc_pll0_config);

	ret = qcom_cc_really_probe(pdev, &disp_cc_scuba_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register DISP CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered DISP CC clocks\n");

	return ret;
}

static struct platform_driver disp_cc_scuba_driver = {
	.probe = disp_cc_scuba_probe,
	.driver = {
		.name = "dispcc-scuba",
		.of_match_table = disp_cc_scuba_match_table,
	},
};

static int __init disp_cc_scuba_init(void)
{
	return platform_driver_register(&disp_cc_scuba_driver);
}
subsys_initcall(disp_cc_scuba_init);

static void __exit disp_cc_scuba_exit(void)
{
	platform_driver_unregister(&disp_cc_scuba_driver);
}
module_exit(disp_cc_scuba_exit);

MODULE_DESCRIPTION("QTI DISP_CC SCUBA Driver");
MODULE_LICENSE("GPL v2");

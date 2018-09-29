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
#include <linux/clk-provider.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,dispcc-sm6150.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "clk-alpha-pll.h"
#include "vdd-level-sm6150.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_CHIP_SLEEP_CLK,
	P_CORE_BI_PLL_TEST_SE,
	P_DISP_CC_PLL0_OUT_MAIN,
	P_DP_PHY_PLL_LINK_CLK,
	P_DP_PHY_PLL_VCO_DIV_CLK,
	P_DSI0_PHY_PLL_OUT_BYTECLK,
	P_DSI0_PHY_PLL_OUT_DSICLK,
	P_DSI1_PHY_PLL_OUT_DSICLK,
	P_GPLL0_OUT_MAIN,
};

static const struct parent_map disp_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_DP_PHY_PLL_LINK_CLK, 1 },
	{ P_DP_PHY_PLL_VCO_DIV_CLK, 2 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_0[] = {
	"bi_tcxo",
	"dp_phy_pll_link_clk",
	"dp_phy_pll_vco_div_clk",
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

static const struct parent_map disp_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_BYTECLK, 1 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_2[] = {
	"bi_tcxo",
	"dsi0_phy_pll_out_byteclk",
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
	"disp_cc_pll0_out_main",
	"gcc_disp_gpll0_div_clk_src",
	"core_bi_pll_test_se",
};

static const struct parent_map disp_cc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 4 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_4[] = {
	"bi_tcxo",
	"gcc_disp_gpll0_div_clk_src",
	"core_bi_pll_test_se",
};

static const struct parent_map disp_cc_parent_map_5[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_DSICLK, 1 },
	{ P_DSI1_PHY_PLL_OUT_DSICLK, 2 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const disp_cc_parent_names_5[] = {
	"bi_tcxo",
	"dsi0_phy_pll_out_dsiclk",
	"dsi1_phy_pll_out_dsiclk",
	"core_bi_pll_test_se",
};

static struct pll_vco disp_cc_pll_vco[] = {
	{ 500000000, 1000000000, 2 },
};

/* 576MHz configuration */
static const struct alpha_pll_config disp_cc_pll0_config = {
	.l = 0x1E,
	.vco_val = 0x2 << 20,
	.vco_mask = 0x3 << 20,
	.main_output_mask = BIT(0),
	.config_ctl_val = 0x4001055b,
	.test_ctl_hi_mask = 0x1,
};

static struct clk_alpha_pll disp_cc_pll0_out_main = {
	.offset = 0x0,
	.vco_table = disp_cc_pll_vco,
	.num_vco = ARRAY_SIZE(disp_cc_pll_vco),
	.flags = SUPPORTS_DYNAMIC_UPDATE,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_pll0_out_main",
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

static const struct freq_tbl ftbl_disp_cc_mdss_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(37500000, P_GPLL0_OUT_MAIN, 8, 0, 0),
	F(75000000, P_GPLL0_OUT_MAIN, 4, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_ahb_clk_src = {
	.cmd_rcgr = 0x2170,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_4,
	.freq_tbl = ftbl_disp_cc_mdss_ahb_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_ahb_clk_src",
		.parent_names = disp_cc_parent_names_4,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 37500000,
			[VDD_NOMINAL] = 75000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_byte0_clk_src = {
	.cmd_rcgr = 0x20c0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_byte0_clk_src",
		.parent_names = disp_cc_parent_names_2,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_byte2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 164000000,
			[VDD_LOW] = 187500000},
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_dp_aux1_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_dp_aux_clk_src = {
	.cmd_rcgr = 0x2158,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_mdss_dp_aux1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_dp_aux_clk_src",
		.parent_names = disp_cc_parent_names_1,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 19200000},
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_dp_crypto_clk_src[] = {
	F( 180000, P_DP_PHY_PLL_LINK_CLK,   1.5,   0,   0),
	F( 360000, P_DP_PHY_PLL_LINK_CLK,   1.5,   0,   0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_dp_crypto_clk_src = {
	.cmd_rcgr = 0x2110,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_mdss_dp_crypto_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_dp_crypto_clk_src",
		.parent_names = disp_cc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 180000,
			[VDD_LOW_L1] = 360000},
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_dp_link_clk_src[] = {
	F( 162000, P_DP_PHY_PLL_LINK_CLK,   1,   0,   0),
	F( 270000, P_DP_PHY_PLL_LINK_CLK,   1,   0,   0),
	F( 540000, P_DP_PHY_PLL_LINK_CLK,   1,   0,   0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_dp_link_clk_src = {
	.cmd_rcgr = 0x20f4,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_mdss_dp_link_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_dp_link_clk_src",
		.parent_names = disp_cc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 270000,
			[VDD_LOW_L1] = 540000},
	},
};

static struct clk_rcg2 disp_cc_mdss_dp_pixel1_clk_src = {
	.cmd_rcgr = 0x2140,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_dp_pixel1_clk_src",
		.parent_names = disp_cc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_dp_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 337500},
	},
};

static struct clk_rcg2 disp_cc_mdss_dp_pixel_clk_src = {
	.cmd_rcgr = 0x2128,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_dp_pixel_clk_src",
		.parent_names = disp_cc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_dp_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 337500},
	},
};

static struct clk_rcg2 disp_cc_mdss_esc0_clk_src = {
	.cmd_rcgr = 0x20dc,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_mdss_dp_aux1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_esc0_clk_src",
		.parent_names = disp_cc_parent_names_2,
		.num_parents = 3,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 19200000},
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_mdp_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(192000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(256000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(307000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_mdp_clk_src = {
	.cmd_rcgr = 0x2078,
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
			[VDD_LOWER] = 192000000,
			[VDD_LOW] = 256000000,
			[VDD_LOW_L1] = 307000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_pclk0_clk_src = {
	.cmd_rcgr = 0x2060,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_5,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_pclk0_clk_src",
		.parent_names = disp_cc_parent_names_5,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_pixel_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 183310056,
			[VDD_LOW] = 250000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_rot_clk_src = {
	.cmd_rcgr = 0x2090,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_3,
	.freq_tbl = ftbl_disp_cc_mdss_mdp_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_rot_clk_src",
		.parent_names = disp_cc_parent_names_3,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 192000000,
			[VDD_LOW] = 256000000,
			[VDD_LOW_L1] = 307000000},
	},
};

static struct clk_rcg2 disp_cc_mdss_vsync_clk_src = {
	.cmd_rcgr = 0x20a8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_mdss_dp_aux1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "disp_cc_mdss_vsync_clk_src",
		.parent_names = disp_cc_parent_names_1,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 19200000},
	},
};

static struct clk_branch disp_cc_mdss_ahb_clk = {
	.halt_reg = 0x2048,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2048,
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
	.halt_reg = 0x2024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2024,
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
	.halt_reg = 0x2028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte0_intf_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_byte0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_aux_clk = {
	.halt_reg = 0x2044,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2044,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_aux_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_dp_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_crypto_clk = {
	.halt_reg = 0x2038,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2038,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_crypto_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_dp_crypto_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_link_clk = {
	.halt_reg = 0x2030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_link_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_dp_link_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_link_intf_clk = {
	.halt_reg = 0x2034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_link_intf_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_dp_link_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_pixel1_clk = {
	.halt_reg = 0x2040,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_pixel1_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_dp_pixel1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_pixel_clk = {
	.halt_reg = 0x203c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x203c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_pixel_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_dp_pixel_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_esc0_clk = {
	.halt_reg = 0x202c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x202c,
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
	.halt_reg = 0x2018,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x2018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_mdp_lut_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_mdp_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_non_gdsc_ahb_clk = {
	.halt_reg = 0x4004,
	.halt_check = BRANCH_VOTED,
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

static struct clk_branch disp_cc_mdss_rot_clk = {
	.halt_reg = 0x2010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_rot_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_rot_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_rscc_ahb_clk = {
	.halt_reg = 0x400c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x400c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_rscc_ahb_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_rscc_vsync_clk = {
	.halt_reg = 0x4008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_rscc_vsync_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_vsync_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_vsync_clk = {
	.halt_reg = 0x2020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2020,
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

static struct clk_branch disp_cc_xo_clk = {
	.halt_reg = 0x6054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x6054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_xo_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *disp_cc_sm6150_clocks[] = {
	[DISP_CC_MDSS_AHB_CLK] = &disp_cc_mdss_ahb_clk.clkr,
	[DISP_CC_MDSS_AHB_CLK_SRC] = &disp_cc_mdss_ahb_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_CLK] = &disp_cc_mdss_byte0_clk.clkr,
	[DISP_CC_MDSS_BYTE0_CLK_SRC] = &disp_cc_mdss_byte0_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_INTF_CLK] = &disp_cc_mdss_byte0_intf_clk.clkr,
	[DISP_CC_MDSS_DP_AUX_CLK] = &disp_cc_mdss_dp_aux_clk.clkr,
	[DISP_CC_MDSS_DP_AUX_CLK_SRC] = &disp_cc_mdss_dp_aux_clk_src.clkr,
	[DISP_CC_MDSS_DP_CRYPTO_CLK] = &disp_cc_mdss_dp_crypto_clk.clkr,
	[DISP_CC_MDSS_DP_CRYPTO_CLK_SRC] = &disp_cc_mdss_dp_crypto_clk_src.clkr,
	[DISP_CC_MDSS_DP_LINK_CLK] = &disp_cc_mdss_dp_link_clk.clkr,
	[DISP_CC_MDSS_DP_LINK_CLK_SRC] = &disp_cc_mdss_dp_link_clk_src.clkr,
	[DISP_CC_MDSS_DP_LINK_INTF_CLK] = &disp_cc_mdss_dp_link_intf_clk.clkr,
	[DISP_CC_MDSS_DP_PIXEL1_CLK] = &disp_cc_mdss_dp_pixel1_clk.clkr,
	[DISP_CC_MDSS_DP_PIXEL1_CLK_SRC] = &disp_cc_mdss_dp_pixel1_clk_src.clkr,
	[DISP_CC_MDSS_DP_PIXEL_CLK] = &disp_cc_mdss_dp_pixel_clk.clkr,
	[DISP_CC_MDSS_DP_PIXEL_CLK_SRC] = &disp_cc_mdss_dp_pixel_clk_src.clkr,
	[DISP_CC_MDSS_ESC0_CLK] = &disp_cc_mdss_esc0_clk.clkr,
	[DISP_CC_MDSS_ESC0_CLK_SRC] = &disp_cc_mdss_esc0_clk_src.clkr,
	[DISP_CC_MDSS_MDP_CLK] = &disp_cc_mdss_mdp_clk.clkr,
	[DISP_CC_MDSS_MDP_CLK_SRC] = &disp_cc_mdss_mdp_clk_src.clkr,
	[DISP_CC_MDSS_MDP_LUT_CLK] = &disp_cc_mdss_mdp_lut_clk.clkr,
	[DISP_CC_MDSS_NON_GDSC_AHB_CLK] = &disp_cc_mdss_non_gdsc_ahb_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK] = &disp_cc_mdss_pclk0_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK_SRC] = &disp_cc_mdss_pclk0_clk_src.clkr,
	[DISP_CC_MDSS_ROT_CLK] = &disp_cc_mdss_rot_clk.clkr,
	[DISP_CC_MDSS_ROT_CLK_SRC] = &disp_cc_mdss_rot_clk_src.clkr,
	[DISP_CC_MDSS_RSCC_AHB_CLK] = &disp_cc_mdss_rscc_ahb_clk.clkr,
	[DISP_CC_MDSS_RSCC_VSYNC_CLK] = &disp_cc_mdss_rscc_vsync_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK] = &disp_cc_mdss_vsync_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK_SRC] = &disp_cc_mdss_vsync_clk_src.clkr,
	[DISP_CC_PLL0_OUT_MAIN] = &disp_cc_pll0_out_main.clkr,
	[DISP_CC_XO_CLK] = &disp_cc_xo_clk.clkr,
};

static const struct regmap_config disp_cc_sm6150_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x10000,
	.fast_io = true,
};

static const struct qcom_cc_desc disp_cc_sm6150_desc = {
	.config = &disp_cc_sm6150_regmap_config,
	.clks = disp_cc_sm6150_clocks,
	.num_clks = ARRAY_SIZE(disp_cc_sm6150_clocks),
};

static const struct of_device_id disp_cc_sm6150_match_table[] = {
	{ .compatible = "qcom,dispcc-sm6150" },
	{ }
};
MODULE_DEVICE_TABLE(of, disp_cc_sm6150_match_table);

static int disp_cc_sm6150_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret = 0;

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	regmap = qcom_cc_map(pdev, &disp_cc_sm6150_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the disp_cc registers\n");
		return PTR_ERR(regmap);
	}

	clk_alpha_pll_configure(&disp_cc_pll0_out_main, regmap,
				&disp_cc_pll0_config);

	ret = qcom_cc_really_probe(pdev, &disp_cc_sm6150_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register Display CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Display CC clocks\n");

	return ret;
}

static struct platform_driver disp_cc_sm6150_driver = {
	.probe = disp_cc_sm6150_probe,
	.driver = {
		.name = "disp_cc-sm6150",
		.of_match_table = disp_cc_sm6150_match_table,
	},
};

static int __init disp_cc_sm6150_init(void)
{
	return platform_driver_register(&disp_cc_sm6150_driver);
}
subsys_initcall(disp_cc_sm6150_init);

static void __exit disp_cc_sm6150_exit(void)
{
	platform_driver_unregister(&disp_cc_sm6150_driver);
}
module_exit(disp_cc_sm6150_exit);

MODULE_DESCRIPTION("QTI DISP_CC SDM6150 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:disp_cc-sm6150");

/*
 * Copyright (c) 2016-2018 The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>
#include <dt-bindings/clock/qcom,mmcc-sdm660.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "common.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-voter.h"
#include "reset.h"
#include "vdd-level-660.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }
#define F_SLEW(f, s, h, m, n, src_freq) { (f), (s), (2 * (h) - 1), (m), (n), \
				(src_freq) }

enum vdd_a_levels {
	VDDA_NONE,
	VDDA_LOWER,             /* SVS2 */
	VDDA_NUM,
};

static int vdda_levels[] = {
	0,
	1800000,
};

#define VDDA_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdda,                   \
	.rate_max = (unsigned long[VDDA_NUM]) {   \
		[VDDA_##l1] = (f1),           \
	},                                      \
	.num_rate_max = VDDA_NUM

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_DIG_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdda, VDDA_NUM, 1, vdda_levels);

enum {
	P_CXO,
	P_CORE_BI_PLL_TEST_SE,
	P_CORE_PI_SLEEP_CLK,
	P_DP_PHY_PLL_LINK_CLK,
	P_DP_PHY_PLL_VCO_DIV,
	P_DSI0_PHY_PLL_OUT_BYTECLK,
	P_DSI0_PHY_PLL_OUT_DSICLK,
	P_DSI1_PHY_PLL_OUT_BYTECLK,
	P_DSI1_PHY_PLL_OUT_DSICLK,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_MMPLL0_PLL_OUT_MAIN,
	P_MMPLL10_PLL_OUT_MAIN,
	P_MMPLL3_PLL_OUT_MAIN,
	P_MMPLL4_PLL_OUT_MAIN,
	P_MMPLL5_PLL_OUT_MAIN,
	P_MMPLL6_PLL_OUT_MAIN,
	P_MMPLL7_PLL_OUT_MAIN,
	P_MMPLL8_PLL_OUT_MAIN,
};

static const struct parent_map mmcc_parent_map_0[] = {
	{ P_CXO, 0 },
	{ P_MMPLL0_PLL_OUT_MAIN, 1 },
	{ P_MMPLL4_PLL_OUT_MAIN, 2 },
	{ P_MMPLL7_PLL_OUT_MAIN, 3 },
	{ P_MMPLL8_PLL_OUT_MAIN, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_0[] = {
	"xo",
	"mmpll0_pll_out_main",
	"mmpll4_pll_out_main",
	"mmpll7_pll_out_main",
	"mmpll8_pll_out_main",
	"gcc_mmss_gpll0_clk",
	"gcc_mmss_gpll0_div_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_1[] = {
	{ P_CXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_BYTECLK, 1 },
	{ P_DSI1_PHY_PLL_OUT_BYTECLK, 2 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_1[] = {
	"xo",
	"dsi0pll_byte_clk_mux",
	"dsi1pll_byte_clk_mux",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_2[] = {
	{ P_CXO, 0 },
	{ P_MMPLL0_PLL_OUT_MAIN, 1 },
	{ P_MMPLL4_PLL_OUT_MAIN, 2 },
	{ P_MMPLL7_PLL_OUT_MAIN, 3 },
	{ P_MMPLL10_PLL_OUT_MAIN, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_2[] = {
	"xo",
	"mmpll0_pll_out_main",
	"mmpll4_pll_out_main",
	"mmpll7_pll_out_main",
	"mmpll10_pll_out_main",
	"gcc_mmss_gpll0_clk",
	"gcc_mmss_gpll0_div_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_3[] = {
	{ P_CXO, 0 },
	{ P_MMPLL4_PLL_OUT_MAIN, 1 },
	{ P_MMPLL7_PLL_OUT_MAIN, 2 },
	{ P_MMPLL10_PLL_OUT_MAIN, 3 },
	{ P_CORE_PI_SLEEP_CLK, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_3[] = {
	"xo",
	"mmpll4_pll_out_main",
	"mmpll7_pll_out_main",
	"mmpll10_pll_out_main",
	"core_pi_sleep_clk",
	"gcc_mmss_gpll0_clk",
	"gcc_mmss_gpll0_div_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_4[] = {
	{ P_CXO, 0 },
	{ P_MMPLL0_PLL_OUT_MAIN, 1 },
	{ P_MMPLL7_PLL_OUT_MAIN, 2 },
	{ P_MMPLL10_PLL_OUT_MAIN, 3 },
	{ P_CORE_PI_SLEEP_CLK, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_4[] = {
	"xo",
	"mmpll0_pll_out_main",
	"mmpll7_pll_out_main",
	"mmpll10_pll_out_main",
	"core_pi_sleep_clk",
	"gcc_mmss_gpll0_clk",
	"gcc_mmss_gpll0_div_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_5[] = {
	{ P_CXO, 0 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_5[] = {
	"xo",
	"gcc_mmss_gpll0_clk",
	"gcc_mmss_gpll0_div_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_6[] = {
	{ P_CXO, 0 },
	{ P_DP_PHY_PLL_LINK_CLK, 1 },
	{ P_DP_PHY_PLL_VCO_DIV, 2 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_6[] = {
	"xo",
	"dp_link_2x_clk_divsel_five",
	"dp_vco_divided_clk_src_mux",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_7[] = {
	{ P_CXO, 0 },
	{ P_MMPLL0_PLL_OUT_MAIN, 1 },
	{ P_MMPLL5_PLL_OUT_MAIN, 2 },
	{ P_MMPLL7_PLL_OUT_MAIN, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_7[] = {
	"xo",
	"mmpll0_pll_out_main",
	"mmpll5_pll_out_main",
	"mmpll7_pll_out_main",
	"gcc_mmss_gpll0_clk",
	"gcc_mmss_gpll0_div_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_8[] = {
	{ P_CXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_DSICLK, 1 },
	{ P_DSI1_PHY_PLL_OUT_DSICLK, 2 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_8[] = {
	"xo",
	"dsi0pll_pixel_clk_mux",
	"dsi1pll_pixel_clk_mux",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_9[] = {
	{ P_CXO, 0 },
	{ P_MMPLL0_PLL_OUT_MAIN, 1 },
	{ P_MMPLL4_PLL_OUT_MAIN, 2 },
	{ P_MMPLL7_PLL_OUT_MAIN, 3 },
	{ P_MMPLL10_PLL_OUT_MAIN, 4 },
	{ P_MMPLL6_PLL_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_9[] = {
	"xo",
	"mmpll0_pll_out_main",
	"mmpll4_pll_out_main",
	"mmpll7_pll_out_main",
	"mmpll10_pll_out_main",
	"mmpll6_pll_out_main",
	"gcc_mmss_gpll0_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_10[] = {
	{ P_CXO, 0 },
	{ P_MMPLL0_PLL_OUT_MAIN, 1 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_10[] = {
	"xo",
	"mmpll0_pll_out_main",
	"gcc_mmss_gpll0_clk",
	"gcc_mmss_gpll0_div_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_11[] = {
	{ P_CXO, 0 },
	{ P_MMPLL0_PLL_OUT_MAIN, 1 },
	{ P_MMPLL4_PLL_OUT_MAIN, 2 },
	{ P_MMPLL7_PLL_OUT_MAIN, 3 },
	{ P_MMPLL10_PLL_OUT_MAIN, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_MMPLL6_PLL_OUT_MAIN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_11[] = {
	"xo",
	"mmpll0_pll_out_main",
	"mmpll4_pll_out_main",
	"mmpll7_pll_out_main",
	"mmpll10_pll_out_main",
	"gcc_mmss_gpll0_clk",
	"mmpll6_pll_out_main",
	"core_bi_pll_test_se",
};

static const struct parent_map mmcc_parent_map_12[] = {
	{ P_CXO, 0 },
	{ P_MMPLL0_PLL_OUT_MAIN, 1 },
	{ P_MMPLL8_PLL_OUT_MAIN, 2 },
	{ P_MMPLL3_PLL_OUT_MAIN, 3 },
	{ P_MMPLL6_PLL_OUT_MAIN, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_MMPLL7_PLL_OUT_MAIN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const mmcc_parent_names_12[] = {
	"xo",
	"mmpll0_pll_out_main",
	"mmpll8_pll_out_main",
	"mmpll3_pll_out_main",
	"mmpll6_pll_out_main",
	"gcc_mmss_gpll0_clk",
	"mmpll7_pll_out_main",
	"core_bi_pll_test_se",
};

/* Voteable PLL */
static struct clk_alpha_pll mmpll0_pll_out_main = {
	.offset = 0xc000,
	.clkr = {
		.enable_reg = 0x1f0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmpll0_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			VDD_MMSS_PLL_DIG_FMAX_MAP2(LOWER, 404000000,
						LOW, 808000000),
		},
	},
};

static struct clk_alpha_pll mmpll6_pll_out_main =  {
	.offset = 0xf0,
	.clkr = {
		.enable_reg = 0x1f0,
		.enable_mask = BIT(2),
		.hw.init = &(struct clk_init_data){
			.name = "mmpll6_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			VDD_MMSS_PLL_DIG_FMAX_MAP2(LOWER, 540000000,
						LOW_L1, 1080000000),
		},
	},
};

/* APSS controlled PLLs */
static struct pll_vco vco[] = {
	{ 1000000000, 2000000000, 0 },
	{ 750000000, 1500000000, 1 },
	{ 500000000, 1000000000, 2 },
	{ 250000000, 500000000, 3 },
};

static const struct alpha_pll_config mmpll10_config = {
	.l = 0x1e,
	.config_ctl_val = 0x00004289,
	.main_output_mask = 0x1,
};

static struct clk_alpha_pll mmpll10_pll_out_main = {
	.offset = 0x190,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "mmpll10_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			VDDA_FMAX_MAP1(LOWER, 576000000),
		},
	},
};

static struct pll_vco mmpll3_vco[] = {
	{ 750000000, 1500000000, 1 },
};

static const struct alpha_pll_config mmpll3_config = {
	.l = 0x2e,
	.config_ctl_val = 0x4001055b,
	.vco_val = 0x1 << 20,
	.vco_mask = 0x3 << 20,
	.main_output_mask = 0x1,
};

static struct clk_alpha_pll mmpll3_pll_out_main = {
	.offset = 0x0,
	.vco_table = mmpll3_vco,
	.num_vco = ARRAY_SIZE(mmpll3_vco),
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "mmpll3_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_slew_ops,
			VDD_MMSS_PLL_DIG_FMAX_MAP2(LOWER, 441600000,
						NOMINAL, 1036800000),
		},
	},
};

static const struct alpha_pll_config mmpll4_config = {
	.l = 0x28,
	.config_ctl_val = 0x4001055b,
	.vco_val = 0x2 << 20,
	.vco_mask = 0x3 << 20,
	.main_output_mask = 0x1,
};

static struct clk_alpha_pll mmpll4_pll_out_main = {
	.offset = 0x50,
	.vco_table = vco,
	.num_vco = ARRAY_SIZE(vco),
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "mmpll4_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			VDD_MMSS_PLL_DIG_FMAX_MAP2(LOWER, 384000000,
						LOW, 768000000),
		},
	},
};

static const struct alpha_pll_config mmpll5_config = {
	.l = 0x2a,
	.config_ctl_val = 0x4001055b,
	.alpha_u = 0xf8,
	.alpha_en_mask = BIT(24),
	.vco_val = 0x2 << 20,
	.vco_mask = 0x3 << 20,
	.main_output_mask = 0x1,
};

static struct clk_alpha_pll mmpll5_pll_out_main = {
	.offset = 0xa0,
	.vco_table = vco,
	.num_vco = ARRAY_SIZE(vco),
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "mmpll5_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			VDD_MMSS_PLL_DIG_FMAX_MAP2(LOWER, 421500000,
						LOW, 825000000),
		},
	},
};

static const struct alpha_pll_config mmpll7_config = {
	.l = 0x32,
	.config_ctl_val = 0x4001055b,
	.vco_val = 0x2 << 20,
	.vco_mask = 0x3 << 20,
	.main_output_mask = 0x1,
};

static struct clk_alpha_pll mmpll7_pll_out_main = {
	.offset = 0x140,
	.vco_table = vco,
	.num_vco = ARRAY_SIZE(vco),
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "mmpll7_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			VDD_MMSS_PLL_DIG_FMAX_MAP2(LOWER, 480000000,
						LOW, 960000000),
		},
	},
};

static const struct alpha_pll_config mmpll8_config = {
	.l = 0x30,
	.alpha_u = 0x70,
	.alpha_en_mask = BIT(24),
	.config_ctl_val = 0x4001055b,
	.vco_val = 0x2 << 20,
	.vco_mask = 0x3 << 20,
	.main_output_mask = 0x1,
};

static struct clk_alpha_pll mmpll8_pll_out_main = {
	.offset = 0x1c0,
	.vco_table = vco,
	.num_vco = ARRAY_SIZE(vco),
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "mmpll8_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			VDD_MMSS_PLL_DIG_FMAX_MAP2(LOWER, 465000000,
						LOW, 930000000),
		},
	},
};

static const struct freq_tbl ftbl_ahb_clk_src[] = {
	F(19200000, P_CXO, 1, 0, 0),
	F(40000000, P_GPLL0_OUT_MAIN_DIV, 7.5, 0, 0),
	F(80800000, P_MMPLL0_PLL_OUT_MAIN, 10, 0, 0),
	{ }
};

static struct clk_rcg2 ahb_clk_src = {
	.cmd_rcgr = 0x5000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_10,
	.freq_tbl = ftbl_ahb_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "ahb_clk_src",
		.parent_names = mmcc_parent_names_10,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(
			LOWER, 19200000,
			LOW, 40000000,
			NOMINAL, 80800000),
	},
};

static struct clk_rcg2 byte0_clk_src = {
	.cmd_rcgr = 0x2120,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_1,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "byte0_clk_src",
		.parent_names = mmcc_parent_names_1,
		.num_parents = 4,
		.ops = &clk_byte2_ops,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		VDD_DIG_FMAX_MAP3(
			LOWER, 131250000,
			LOW, 210000000,
			NOMINAL, 262500000),
	},
};

static struct clk_rcg2 byte1_clk_src = {
	.cmd_rcgr = 0x2140,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_1,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "byte1_clk_src",
		.parent_names = mmcc_parent_names_1,
		.num_parents = 4,
		.ops = &clk_byte2_ops,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		VDD_DIG_FMAX_MAP3(
			LOWER, 131250000,
			LOW, 210000000,
			NOMINAL, 262500000),
	},
};

static const struct freq_tbl ftbl_camss_gp0_clk_src[] = {
	F(10000, P_CXO, 16, 1, 120),
	F(24000, P_CXO, 16, 1, 50),
	F(6000000, P_GPLL0_OUT_MAIN_DIV, 10, 1, 5),
	F(12000000, P_GPLL0_OUT_MAIN_DIV, 10, 2, 5),
	F(13043478, P_GPLL0_OUT_MAIN_DIV, 1, 1, 23),
	F(24000000, P_GPLL0_OUT_MAIN_DIV, 1, 2, 25),
	F(50000000, P_GPLL0_OUT_MAIN_DIV, 6, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN_DIV, 3, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 camss_gp0_clk_src = {
	.cmd_rcgr = 0x3420,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_4,
	.freq_tbl = ftbl_camss_gp0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "camss_gp0_clk_src",
		.parent_names = mmcc_parent_names_4,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static struct clk_rcg2 camss_gp1_clk_src = {
	.cmd_rcgr = 0x3450,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_4,
	.freq_tbl = ftbl_camss_gp0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "camss_gp1_clk_src",
		.parent_names = mmcc_parent_names_4,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static const struct freq_tbl ftbl_cci_clk_src[] = {
	F(37500000, P_GPLL0_OUT_MAIN_DIV, 8, 0, 0),
	F(50000000, P_GPLL0_OUT_MAIN_DIV, 6, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	{ }
};

static struct clk_rcg2 cci_clk_src = {
	.cmd_rcgr = 0x3300,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_4,
	.freq_tbl = ftbl_cci_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cci_clk_src",
		.parent_names = mmcc_parent_names_4,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(
			LOWER, 37500000,
			LOW, 50000000,
			NOMINAL, 100000000),
	},
};

static const struct freq_tbl ftbl_cpp_clk_src[] = {
	F(120000000, P_GPLL0_OUT_MAIN, 5, 0, 0),
	F(256000000, P_MMPLL4_PLL_OUT_MAIN, 3, 0, 0),
	F(384000000, P_MMPLL4_PLL_OUT_MAIN, 2, 0, 0),
	F(480000000, P_MMPLL7_PLL_OUT_MAIN, 2, 0, 0),
	F(540000000, P_MMPLL6_PLL_OUT_MAIN, 2, 0, 0),
	F(576000000, P_MMPLL10_PLL_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cpp_clk_src = {
	.cmd_rcgr = 0x3640,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_11,
	.freq_tbl = ftbl_cpp_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cpp_clk_src",
		.parent_names = mmcc_parent_names_11,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP6(
			LOWER, 120000000,
			LOW, 256000000,
			LOW_L1, 384000000,
			NOMINAL, 480000000,
			NOMINAL_L1, 540000000,
			HIGH, 576000000),
	},
};

static const struct freq_tbl ftbl_csi0_clk_src[] = {
	F(100000000, P_GPLL0_OUT_MAIN_DIV, 3, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(310000000, P_MMPLL8_PLL_OUT_MAIN, 3, 0, 0),
	F(404000000, P_MMPLL0_PLL_OUT_MAIN, 2, 0, 0),
	F(465000000, P_MMPLL8_PLL_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 csi0_clk_src = {
	.cmd_rcgr = 0x3090,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_0,
	.freq_tbl = ftbl_csi0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "csi0_clk_src",
		.parent_names = mmcc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(
			LOWER, 100000000,
			LOW, 200000000,
			LOW_L1, 310000000,
			NOMINAL, 404000000,
			NOMINAL_L1, 465000000),
	},
};

static const struct freq_tbl ftbl_csi0phytimer_clk_src[] = {
	F(100000000, P_GPLL0_OUT_MAIN_DIV, 3, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(269333333, P_MMPLL0_PLL_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 csi0phytimer_clk_src = {
	.cmd_rcgr = 0x3000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_2,
	.freq_tbl = ftbl_csi0phytimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "csi0phytimer_clk_src",
		.parent_names = mmcc_parent_names_2,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(
			LOWER, 100000000,
			LOW, 200000000,
			LOW_L1, 269333333),
	},
};

static struct clk_rcg2 csi1_clk_src = {
	.cmd_rcgr = 0x3100,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_0,
	.freq_tbl = ftbl_csi0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "csi1_clk_src",
		.parent_names = mmcc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(
			LOWER, 100000000,
			LOW, 200000000,
			LOW_L1, 310000000,
			NOMINAL, 404000000,
			NOMINAL_L1, 465000000),
	},
};

static struct clk_rcg2 csi1phytimer_clk_src = {
	.cmd_rcgr = 0x3030,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_2,
	.freq_tbl = ftbl_csi0phytimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "csi1phytimer_clk_src",
		.parent_names = mmcc_parent_names_2,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(
			LOWER, 100000000,
			LOW, 200000000,
			LOW_L1, 269333333),
	},
};

static struct clk_rcg2 csi2_clk_src = {
	.cmd_rcgr = 0x3160,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_0,
	.freq_tbl = ftbl_csi0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "csi2_clk_src",
		.parent_names = mmcc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(
			LOWER, 100000000,
			LOW, 200000000,
			LOW_L1, 310000000,
			NOMINAL, 404000000,
			NOMINAL_L1, 465000000),
	},
};

static struct clk_rcg2 csi2phytimer_clk_src = {
	.cmd_rcgr = 0x3060,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_2,
	.freq_tbl = ftbl_csi0phytimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "csi2phytimer_clk_src",
		.parent_names = mmcc_parent_names_2,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(
			LOWER, 100000000,
			LOW, 200000000,
			LOW_L1, 269333333),
	},
};

static struct clk_rcg2 csi3_clk_src = {
	.cmd_rcgr = 0x31c0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_0,
	.freq_tbl = ftbl_csi0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "csi3_clk_src",
		.parent_names = mmcc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(
			LOWER, 100000000,
			LOW, 200000000,
			LOW_L1, 310000000,
			NOMINAL, 404000000,
			NOMINAL_L1, 465000000),
	},
};

static const struct freq_tbl ftbl_csiphy_clk_src[] = {
	F(100000000, P_GPLL0_OUT_MAIN_DIV, 3, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(269333333, P_MMPLL0_PLL_OUT_MAIN, 3, 0, 0),
	F(320000000, P_MMPLL7_PLL_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 csiphy_clk_src = {
	.cmd_rcgr = 0x3800,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_0,
	.freq_tbl = ftbl_csiphy_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "csiphy_clk_src",
		.parent_names = mmcc_parent_names_0,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP4(
			LOWER, 100000000,
			LOW, 200000000,
			LOW_L1, 269333333,
			NOMINAL, 320000000),
	},
};

static const struct freq_tbl ftbl_dp_aux_clk_src[] = {
	F(19200000, P_CXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 dp_aux_clk_src = {
	.cmd_rcgr = 0x2260,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_5,
	.freq_tbl = ftbl_dp_aux_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "dp_aux_clk_src",
		.parent_names = mmcc_parent_names_5,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(
			LOWER, 19200000),
	},
};

static const struct freq_tbl ftbl_dp_crypto_clk_src[] = {
	F(101250, P_DP_PHY_PLL_VCO_DIV, 4, 0, 0),
	F(168750, P_DP_PHY_PLL_VCO_DIV, 4, 0, 0),
	F(337500, P_DP_PHY_PLL_VCO_DIV, 4, 0, 0),
	{ }
};

static struct clk_rcg2 dp_crypto_clk_src = {
	.cmd_rcgr = 0x2220,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_6,
	.freq_tbl = ftbl_dp_crypto_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "dp_crypto_clk_src",
		.parent_names = mmcc_parent_names_6,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(
			LOWER, 101250,
			LOW, 168750,
			NOMINAL, 337500),
	},
};

static const struct freq_tbl ftbl_dp_gtc_clk_src[] = {
	F(40000000, P_GPLL0_OUT_MAIN_DIV, 7.5, 0, 0),
	F(60000000, P_GPLL0_OUT_MAIN, 10, 0, 0),
	{ }
};

static struct clk_rcg2 dp_gtc_clk_src = {
	.cmd_rcgr = 0x2280,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_5,
	.freq_tbl = ftbl_dp_gtc_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "dp_gtc_clk_src",
		.parent_names = mmcc_parent_names_5,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(
			LOWER, 40000000,
			LOW, 60000000),
	},
};

static const struct freq_tbl ftbl_dp_link_clk_src[] = {
	F(162000, P_DP_PHY_PLL_LINK_CLK, 2, 0, 0),
	F(270000, P_DP_PHY_PLL_LINK_CLK, 2, 0, 0),
	F(540000, P_DP_PHY_PLL_LINK_CLK, 2, 0, 0),
	{ }
};

static struct clk_rcg2 dp_link_clk_src = {
	.cmd_rcgr = 0x2200,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_6,
	.freq_tbl = ftbl_dp_link_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "dp_link_clk_src",
		.parent_names = mmcc_parent_names_6,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
		VDD_DIG_FMAX_MAP3(
			LOWER, 162000,
			LOW, 270000,
			NOMINAL, 540000),
	},
};

static struct clk_rcg2 dp_pixel_clk_src = {
	.cmd_rcgr = 0x2240,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_6,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "dp_pixel_clk_src",
		.parent_names = mmcc_parent_names_6,
		.num_parents = 4,
		.ops = &clk_dp_ops,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
		VDD_DIG_FMAX_MAP3(
			LOWER, 154000,
			LOW, 296740,
			NOMINAL, 593470),
	},
};

static struct clk_rcg2 esc0_clk_src = {
	.cmd_rcgr = 0x2160,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_1,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "esc0_clk_src",
		.parent_names = mmcc_parent_names_1,
		.num_parents = 4,
		.ops = &clk_esc_ops,
		VDD_DIG_FMAX_MAP1(
			LOWER, 19200000),
	},
};

static struct clk_rcg2 esc1_clk_src = {
	.cmd_rcgr = 0x2180,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_1,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "esc1_clk_src",
		.parent_names = mmcc_parent_names_1,
		.num_parents = 4,
		.ops = &clk_esc_ops,
		VDD_DIG_FMAX_MAP1(
			LOWER, 19200000),
	},
};

static const struct freq_tbl ftbl_jpeg0_clk_src[] = {
	F(66666667, P_GPLL0_OUT_MAIN_DIV, 4.5, 0, 0),
	F(133333333, P_GPLL0_OUT_MAIN, 4.5, 0, 0),
	F(219428571, P_MMPLL4_PLL_OUT_MAIN, 3.5, 0, 0),
	F(320000000, P_MMPLL7_PLL_OUT_MAIN, 3, 0, 0),
	F(480000000, P_MMPLL7_PLL_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 jpeg0_clk_src = {
	.cmd_rcgr = 0x3500,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_2,
	.freq_tbl = ftbl_jpeg0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "jpeg0_clk_src",
		.parent_names = mmcc_parent_names_2,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(
			LOWER, 66666667,
			LOW, 133333333,
			LOW_L1, 219428571,
			NOMINAL, 320000000,
			NOMINAL_L1, 480000000),
	},
};

static const struct freq_tbl ftbl_mclk0_clk_src[] = {
	F(4800000, P_CXO, 4, 0, 0),
	F(6000000, P_GPLL0_OUT_MAIN_DIV, 10, 1, 5),
	F(8000000, P_GPLL0_OUT_MAIN_DIV, 1, 2, 75),
	F(9600000, P_CXO, 2, 0, 0),
	F(16666667, P_GPLL0_OUT_MAIN_DIV, 2, 1, 9),
	F(19200000, P_CXO, 1, 0, 0),
	F(24000000, P_MMPLL10_PLL_OUT_MAIN, 1, 1, 24),
	F(33333333, P_GPLL0_OUT_MAIN_DIV, 1, 1, 9),
	F(48000000, P_GPLL0_OUT_MAIN, 1, 2, 25),
	F(66666667, P_GPLL0_OUT_MAIN, 1, 1, 9),
	{ }
};

static struct clk_rcg2 mclk0_clk_src = {
	.cmd_rcgr = 0x3360,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_3,
	.freq_tbl = ftbl_mclk0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "mclk0_clk_src",
		.parent_names = mmcc_parent_names_3,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(
			LOWER, 33333333,
			LOW, 66666667),
	},
};

static struct clk_rcg2 mclk1_clk_src = {
	.cmd_rcgr = 0x3390,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_3,
	.freq_tbl = ftbl_mclk0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "mclk1_clk_src",
		.parent_names = mmcc_parent_names_3,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(
			LOWER, 33333333,
			LOW, 66666667),
	},
};

static struct clk_rcg2 mclk2_clk_src = {
	.cmd_rcgr = 0x33c0,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_3,
	.freq_tbl = ftbl_mclk0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "mclk2_clk_src",
		.parent_names = mmcc_parent_names_3,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(
			LOWER, 33333333,
			LOW, 66666667),
	},
};

static struct clk_rcg2 mclk3_clk_src = {
	.cmd_rcgr = 0x33f0,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_3,
	.freq_tbl = ftbl_mclk0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "mclk3_clk_src",
		.parent_names = mmcc_parent_names_3,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(
			LOWER, 33333333,
			LOW, 66666667),
	},
};

static const struct freq_tbl ftbl_mdp_clk_src[] = {
	F(100000000, P_GPLL0_OUT_MAIN_DIV, 3, 0, 0),
	F(150000000, P_GPLL0_OUT_MAIN_DIV, 2, 0, 0),
	F(171428571, P_GPLL0_OUT_MAIN, 3.5, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(275000000, P_MMPLL5_PLL_OUT_MAIN, 3, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	F(330000000, P_MMPLL5_PLL_OUT_MAIN, 2.5, 0, 0),
	F(412500000, P_MMPLL5_PLL_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 mdp_clk_src = {
	.cmd_rcgr = 0x2040,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_7,
	.freq_tbl = ftbl_mdp_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "mdp_clk_src",
		.parent_names = mmcc_parent_names_7,
		.num_parents = 7,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(
			LOWER, 171428571,
			LOW, 275000000,
			LOW_L1, 300000000,
			NOMINAL, 330000000,
			HIGH, 412500000),
	},
};

static struct clk_rcg2 pclk0_clk_src = {
	.cmd_rcgr = 0x2000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_8,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "pclk0_clk_src",
		.parent_names = mmcc_parent_names_8,
		.num_parents = 4,
		.ops = &clk_pixel_ops,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		VDD_DIG_FMAX_MAP3(
			LOWER, 175000000,
			LOW, 280000000,
			NOMINAL, 350000000),
	},
};

static struct clk_rcg2 pclk1_clk_src = {
	.cmd_rcgr = 0x2020,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_8,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "pclk1_clk_src",
		.parent_names = mmcc_parent_names_8,
		.num_parents = 4,
		.ops = &clk_pixel_ops,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		VDD_DIG_FMAX_MAP3(
			LOWER, 175000000,
			LOW, 280000000,
			NOMINAL, 350000000),
	},
};

static const struct freq_tbl ftbl_rot_clk_src[] = {
	F(171428571, P_GPLL0_OUT_MAIN, 3.5, 0, 0),
	F(275000000, P_MMPLL5_PLL_OUT_MAIN, 3, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	F(330000000, P_MMPLL5_PLL_OUT_MAIN, 2.5, 0, 0),
	F(412500000, P_MMPLL5_PLL_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 rot_clk_src = {
	.cmd_rcgr = 0x21a0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_7,
	.freq_tbl = ftbl_rot_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "rot_clk_src",
		.parent_names = mmcc_parent_names_7,
		.num_parents = 7,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(
			LOWER, 171428571,
			LOW, 275000000,
			LOW_L1, 300000000,
			NOMINAL, 330000000,
			HIGH, 412500000),
	},
};

static const struct freq_tbl ftbl_vfe0_clk_src[] = {
	F(120000000, P_GPLL0_OUT_MAIN, 5, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(256000000, P_MMPLL4_PLL_OUT_MAIN, 3, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	F(404000000, P_MMPLL0_PLL_OUT_MAIN, 2, 0, 0),
	F(480000000, P_MMPLL7_PLL_OUT_MAIN, 2, 0, 0),
	F(540000000, P_MMPLL6_PLL_OUT_MAIN, 2, 0, 0),
	F(576000000, P_MMPLL10_PLL_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 vfe0_clk_src = {
	.cmd_rcgr = 0x3600,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_9,
	.freq_tbl = ftbl_vfe0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "vfe0_clk_src",
		.parent_names = mmcc_parent_names_9,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP6(
			LOWER, 120000000,
			LOW, 256000000,
			LOW_L1, 404000000,
			NOMINAL, 480000000,
			NOMINAL_L1, 540000000,
			HIGH, 576000000),
	},
};

static struct clk_rcg2 vfe1_clk_src = {
	.cmd_rcgr = 0x3620,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_9,
	.freq_tbl = ftbl_vfe0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "vfe1_clk_src",
		.parent_names = mmcc_parent_names_9,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP6(
			LOWER, 120000000,
			LOW, 256000000,
			LOW_L1, 404000000,
			NOMINAL, 480000000,
			NOMINAL_L1, 540000000,
			HIGH, 576000000),
	},
};

static const struct freq_tbl ftbl_video_core_clk_src[] = {
	F_SLEW(133333333, P_GPLL0_OUT_MAIN, 4.5, 0, 0, FIXED_FREQ_SRC),
	F_SLEW(269333333, P_MMPLL0_PLL_OUT_MAIN, 3, 0, 0, FIXED_FREQ_SRC),
	F_SLEW(320000000, P_MMPLL7_PLL_OUT_MAIN, 3, 0, 0, FIXED_FREQ_SRC),
	F_SLEW(404000000, P_MMPLL0_PLL_OUT_MAIN, 2, 0, 0, FIXED_FREQ_SRC),
	F_SLEW(441600000, P_MMPLL3_PLL_OUT_MAIN, 2, 0, 0, 883200000),
	F_SLEW(518400000, P_MMPLL3_PLL_OUT_MAIN, 2, 0, 0, 1036800000),
	{ }
};

static struct clk_rcg2 video_core_clk_src = {
	.cmd_rcgr = 0x1000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_12,
	.freq_tbl = ftbl_video_core_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_core_clk_src",
		.parent_names = mmcc_parent_names_12,
		.num_parents = 8,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP6(
			LOWER, 133333333,
			LOW, 269333333,
			LOW_L1, 320000000,
			NOMINAL, 404000000,
			NOMINAL_L1, 441600000,
			HIGH, 518400000),
	},
};

static struct clk_rcg2 vsync_clk_src = {
	.cmd_rcgr = 0x2080,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = mmcc_parent_map_5,
	.freq_tbl = ftbl_dp_aux_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "vsync_clk_src",
		.parent_names = mmcc_parent_names_5,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(
			LOWER, 19200000),
	},
};

static struct clk_branch mmss_bimc_smmu_ahb_clk = {
	.halt_reg = 0xe004,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0xe004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_bimc_smmu_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.flags = CLK_ENABLE_HAND_OFF,
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_bimc_smmu_axi_clk = {
	.halt_reg = 0xe008,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0xe008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_bimc_smmu_axi_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_ahb_clk = {
	.halt_reg = 0x348c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x348c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cci_ahb_clk = {
	.halt_reg = 0x3348,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3348,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cci_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cci_clk = {
	.halt_reg = 0x3344,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3344,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cci_clk",
			.parent_names = (const char *[]){
				"cci_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cphy_csid0_clk = {
	.halt_reg = 0x3730,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3730,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cphy_csid0_clk",
			.parent_names = (const char *[]){
				"csiphy_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cphy_csid1_clk = {
	.halt_reg = 0x3734,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3734,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cphy_csid1_clk",
			.parent_names = (const char *[]){
				"csiphy_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cphy_csid2_clk = {
	.halt_reg = 0x3738,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3738,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cphy_csid2_clk",
			.parent_names = (const char *[]){
				"csiphy_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cphy_csid3_clk = {
	.halt_reg = 0x373c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x373c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cphy_csid3_clk",
			.parent_names = (const char *[]){
				"csiphy_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cpp_ahb_clk = {
	.halt_reg = 0x36b4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36b4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cpp_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cpp_axi_clk = {
	.halt_reg = 0x36c4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36c4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cpp_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cpp_clk = {
	.halt_reg = 0x36b0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36b0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cpp_clk",
			.parent_names = (const char *[]){
				"cpp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_cpp_vbif_ahb_clk = {
	.halt_reg = 0x36c8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36c8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_cpp_vbif_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi0_ahb_clk = {
	.halt_reg = 0x30bc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x30bc,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi0_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi0_clk = {
	.halt_reg = 0x30b4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x30b4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi0_clk",
			.parent_names = (const char *[]){
				"csi0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi0phytimer_clk = {
	.halt_reg = 0x3024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi0phytimer_clk",
			.parent_names = (const char *[]){
				"csi0phytimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi0pix_clk = {
	.halt_reg = 0x30e4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x30e4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi0pix_clk",
			.parent_names = (const char *[]){
				"csi0_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi0rdi_clk = {
	.halt_reg = 0x30d4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x30d4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi0rdi_clk",
			.parent_names = (const char *[]){
				"csi0_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi1_ahb_clk = {
	.halt_reg = 0x3128,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3128,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi1_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi1_clk = {
	.halt_reg = 0x3124,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3124,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi1_clk",
			.parent_names = (const char *[]){
				"csi1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi1phytimer_clk = {
	.halt_reg = 0x3054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi1phytimer_clk",
			.parent_names = (const char *[]){
				"csi1phytimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi1pix_clk = {
	.halt_reg = 0x3154,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3154,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi1pix_clk",
			.parent_names = (const char *[]){
				"csi1_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi1rdi_clk = {
	.halt_reg = 0x3144,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3144,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi1rdi_clk",
			.parent_names = (const char *[]){
				"csi1_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi2_ahb_clk = {
	.halt_reg = 0x3188,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3188,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi2_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi2_clk = {
	.halt_reg = 0x3184,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3184,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi2_clk",
			.parent_names = (const char *[]){
				"csi2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi2phytimer_clk = {
	.halt_reg = 0x3084,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3084,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi2phytimer_clk",
			.parent_names = (const char *[]){
				"csi2phytimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi2pix_clk = {
	.halt_reg = 0x31b4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x31b4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi2pix_clk",
			.parent_names = (const char *[]){
				"csi2_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi2rdi_clk = {
	.halt_reg = 0x31a4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x31a4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi2rdi_clk",
			.parent_names = (const char *[]){
				"csi2_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi3_ahb_clk = {
	.halt_reg = 0x31e8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x31e8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi3_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi3_clk = {
	.halt_reg = 0x31e4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x31e4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi3_clk",
			.parent_names = (const char *[]){
				"csi3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi3pix_clk = {
	.halt_reg = 0x3214,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3214,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi3pix_clk",
			.parent_names = (const char *[]){
				"csi3_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi3rdi_clk = {
	.halt_reg = 0x3204,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3204,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi3rdi_clk",
			.parent_names = (const char *[]){
				"csi3_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi_vfe0_clk = {
	.halt_reg = 0x3704,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3704,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi_vfe0_clk",
			.parent_names = (const char *[]){
				"vfe0_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csi_vfe1_clk = {
	.halt_reg = 0x3714,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3714,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csi_vfe1_clk",
			.parent_names = (const char *[]){
				"vfe1_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csiphy0_clk = {
	.halt_reg = 0x3740,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3740,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csiphy0_clk",
			.parent_names = (const char *[]){
				"csiphy_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csiphy1_clk = {
	.halt_reg = 0x3744,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3744,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csiphy1_clk",
			.parent_names = (const char *[]){
				"csiphy_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_csiphy2_clk = {
	.halt_reg = 0x3748,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3748,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_csiphy2_clk",
			.parent_names = (const char *[]){
				"csiphy_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_gp0_clk = {
	.halt_reg = 0x3444,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3444,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_gp0_clk",
			.parent_names = (const char *[]){
				"camss_gp0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_gp1_clk = {
	.halt_reg = 0x3474,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3474,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_gp1_clk",
			.parent_names = (const char *[]){
				"camss_gp1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_ispif_ahb_clk = {
	.halt_reg = 0x3224,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3224,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_ispif_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_jpeg0_clk = {
	.halt_reg = 0x35a8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x35a8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_jpeg0_clk",
			.parent_names = (const char *[]){
				"jpeg0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static DEFINE_CLK_VOTER(mmss_camss_jpeg0_vote_clk, mmss_camss_jpeg0_clk, 0);
static DEFINE_CLK_VOTER(mmss_camss_jpeg0_dma_vote_clk,
					mmss_camss_jpeg0_clk, 0);

static struct clk_branch mmss_camss_jpeg_ahb_clk = {
	.halt_reg = 0x35b4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x35b4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_jpeg_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_jpeg_axi_clk = {
	.halt_reg = 0x35b8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x35b8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_jpeg_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_throttle_camss_axi_clk = {
	.halt_reg = 0x3c3c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3c3c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_throttle_camss_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_mclk0_clk = {
	.halt_reg = 0x3384,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3384,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_mclk0_clk",
			.parent_names = (const char *[]){
				"mclk0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_mclk1_clk = {
	.halt_reg = 0x33b4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x33b4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_mclk1_clk",
			.parent_names = (const char *[]){
				"mclk1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_mclk2_clk = {
	.halt_reg = 0x33e4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x33e4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_mclk2_clk",
			.parent_names = (const char *[]){
				"mclk2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_mclk3_clk = {
	.halt_reg = 0x3414,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3414,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_mclk3_clk",
			.parent_names = (const char *[]){
				"mclk3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_micro_ahb_clk = {
	.halt_reg = 0x3494,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3494,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_micro_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_top_ahb_clk = {
	.halt_reg = 0x3484,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3484,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_top_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_vfe0_ahb_clk = {
	.halt_reg = 0x3668,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3668,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_vfe0_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_vfe0_clk = {
	.halt_reg = 0x36a8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36a8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_vfe0_clk",
			.parent_names = (const char *[]){
				"vfe0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_vfe0_stream_clk = {
	.halt_reg = 0x3720,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3720,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_vfe0_stream_clk",
			.parent_names = (const char *[]){
				"vfe0_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_vfe1_ahb_clk = {
	.halt_reg = 0x3678,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3678,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_vfe1_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_vfe1_clk = {
	.halt_reg = 0x36ac,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36ac,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_vfe1_clk",
			.parent_names = (const char *[]){
				"vfe1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_vfe1_stream_clk = {
	.halt_reg = 0x3724,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3724,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_vfe1_stream_clk",
			.parent_names = (const char *[]){
				"vfe1_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_vfe_vbif_ahb_clk = {
	.halt_reg = 0x36b8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36b8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_vfe_vbif_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_camss_vfe_vbif_axi_clk = {
	.halt_reg = 0x36bc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36bc,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_camss_vfe_vbif_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_csiphy_ahb2crif_clk = {
	.halt_reg = 0x374c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x374c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_csiphy_ahb2crif_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_ahb_clk = {
	.halt_reg = 0x2308,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2308,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.flags = CLK_ENABLE_HAND_OFF,
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_axi_clk = {
	.halt_reg = 0x2310,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2310,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_throttle_mdss_axi_clk = {
	.halt_reg = 0x246c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x246c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_throttle_mdss_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_byte0_clk = {
	.halt_reg = 0x233c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x233c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_byte0_clk",
			.parent_names = (const char *[]){
				"byte0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_byte0_intf_clk = {
	.halt_reg = 0x2374,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2374,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_byte0_intf_clk",
			.parent_names = (const char *[]){
				"mmss_mdss_byte0_intf_div_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap_div mmss_mdss_byte0_intf_div_clk = {
	.reg = 0x237c,
	.shift = 0,
	.width = 2,
	/*
	 * NOTE: Op does not work for div-3. Current assumption is that div-3
	 * is not a recommended setting for this divider.
	 */
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_byte0_intf_div_clk",
			.parent_names = (const char *[]){
					"byte0_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_regmap_div_ops,
			.flags = CLK_GET_RATE_NOCACHE,
		},
	},
};

static struct clk_branch mmss_mdss_byte1_clk = {
	.halt_reg = 0x2340,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2340,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_byte1_clk",
			.parent_names = (const char *[]){
				"byte1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_byte1_intf_clk = {
	.halt_reg = 0x2378,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2378,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_byte1_intf_clk",
			.parent_names = (const char *[]){
				"mmss_mdss_byte1_intf_div_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap_div mmss_mdss_byte1_intf_div_clk = {
	.reg = 0x2380,
	.shift = 0,
	.width = 2,
	/*
	 * NOTE: Op does not work for div-3. Current assumption is that div-3
	 * is not a recommended setting for this divider.
	 */
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_byte1_intf_div_clk",
			.parent_names = (const char *[]){
					"byte1_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_regmap_div_ops,
			.flags = CLK_GET_RATE_NOCACHE,
		},
	},
};

static struct clk_branch mmss_mdss_dp_aux_clk = {
	.halt_reg = 0x2364,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2364,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_dp_aux_clk",
			.parent_names = (const char *[]){
				"dp_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_dp_crypto_clk = {
	.halt_reg = 0x235c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x235c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_dp_crypto_clk",
			.parent_names = (const char *[]){
				"dp_crypto_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_dp_gtc_clk = {
	.halt_reg = 0x2368,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2368,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_dp_gtc_clk",
			.parent_names = (const char *[]){
				"dp_gtc_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_dp_link_clk = {
	.halt_reg = 0x2354,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2354,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_dp_link_clk",
			.parent_names = (const char *[]){
				"dp_link_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

/* Reset state of MMSS_MDSS_DP_LINK_INTF_DIV is 0x3 (div-4) */
static struct clk_branch mmss_mdss_dp_link_intf_clk = {
	.halt_reg = 0x2358,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2358,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_dp_link_intf_clk",
			.parent_names = (const char *[]){
				"dp_link_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_dp_pixel_clk = {
	.halt_reg = 0x2360,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2360,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_dp_pixel_clk",
			.parent_names = (const char *[]){
				"dp_pixel_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_esc0_clk = {
	.halt_reg = 0x2344,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2344,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_esc0_clk",
			.parent_names = (const char *[]){
				"esc0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_esc1_clk = {
	.halt_reg = 0x2348,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2348,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_esc1_clk",
			.parent_names = (const char *[]){
				"esc1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_hdmi_dp_ahb_clk = {
	.halt_reg = 0x230c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x230c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_hdmi_dp_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_mdp_clk = {
	.halt_reg = 0x231c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x231c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_mdp_clk",
			.parent_names = (const char *[]){
				"mdp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_pclk0_clk = {
	.halt_reg = 0x2314,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2314,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_pclk0_clk",
			.parent_names = (const char *[]){
				"pclk0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_pclk1_clk = {
	.halt_reg = 0x2318,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2318,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_pclk1_clk",
			.parent_names = (const char *[]){
				"pclk1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_rot_clk = {
	.halt_reg = 0x2350,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2350,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_rot_clk",
			.parent_names = (const char *[]){
				"rot_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mdss_vsync_clk = {
	.halt_reg = 0x2328,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2328,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mdss_vsync_clk",
			.parent_names = (const char *[]){
				"vsync_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_misc_ahb_clk = {
	.halt_reg = 0x328,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x328,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_misc_ahb_clk",
			/*
			 * Dependency to be enabled before the branch is
			 * enabled.
			 */
			.parent_names = (const char *[]){
				"mmss_mnoc_ahb_clk",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_misc_cxo_clk = {
	.halt_reg = 0x324,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x324,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_misc_cxo_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_mnoc_ahb_clk = {
	.halt_reg = 0x5024,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x5024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_mnoc_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_snoc_dvm_axi_clk = {
	.halt_reg = 0xe040,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_snoc_dvm_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_video_ahb_clk = {
	.halt_reg = 0x1030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_video_ahb_clk",
			.parent_names = (const char *[]){
				"ahb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_video_axi_clk = {
	.halt_reg = 0x1034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_video_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_throttle_video_axi_clk = {
	.halt_reg = 0x118c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x118c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_throttle_video_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_video_core_clk = {
	.halt_reg = 0x1028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_video_core_clk",
			.parent_names = (const char *[]){
				"video_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch mmss_video_subcore0_clk = {
	.halt_reg = 0x1048,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "mmss_video_subcore0_clk",
			.parent_names = (const char *[]){
				"video_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

struct clk_hw *mmcc_sdm660_hws[] = {
	[MMSS_CAMSS_JPEG0_VOTE_CLK] = &mmss_camss_jpeg0_vote_clk.hw,
	[MMSS_CAMSS_JPEG0_DMA_VOTE_CLK] = &mmss_camss_jpeg0_dma_vote_clk.hw,
};

static struct clk_regmap *mmcc_660_clocks[] = {
	[AHB_CLK_SRC] = &ahb_clk_src.clkr,
	[BYTE0_CLK_SRC] = &byte0_clk_src.clkr,
	[BYTE1_CLK_SRC] = &byte1_clk_src.clkr,
	[CAMSS_GP0_CLK_SRC] = &camss_gp0_clk_src.clkr,
	[CAMSS_GP1_CLK_SRC] = &camss_gp1_clk_src.clkr,
	[CCI_CLK_SRC] = &cci_clk_src.clkr,
	[CPP_CLK_SRC] = &cpp_clk_src.clkr,
	[CSI0_CLK_SRC] = &csi0_clk_src.clkr,
	[CSI0PHYTIMER_CLK_SRC] = &csi0phytimer_clk_src.clkr,
	[CSI1_CLK_SRC] = &csi1_clk_src.clkr,
	[CSI1PHYTIMER_CLK_SRC] = &csi1phytimer_clk_src.clkr,
	[CSI2_CLK_SRC] = &csi2_clk_src.clkr,
	[CSI2PHYTIMER_CLK_SRC] = &csi2phytimer_clk_src.clkr,
	[CSI3_CLK_SRC] = &csi3_clk_src.clkr,
	[CSIPHY_CLK_SRC] = &csiphy_clk_src.clkr,
	[DP_AUX_CLK_SRC] = &dp_aux_clk_src.clkr,
	[DP_CRYPTO_CLK_SRC] = &dp_crypto_clk_src.clkr,
	[DP_GTC_CLK_SRC] = &dp_gtc_clk_src.clkr,
	[DP_LINK_CLK_SRC] = &dp_link_clk_src.clkr,
	[DP_PIXEL_CLK_SRC] = &dp_pixel_clk_src.clkr,
	[ESC0_CLK_SRC] = &esc0_clk_src.clkr,
	[ESC1_CLK_SRC] = &esc1_clk_src.clkr,
	[JPEG0_CLK_SRC] = &jpeg0_clk_src.clkr,
	[MCLK0_CLK_SRC] = &mclk0_clk_src.clkr,
	[MCLK1_CLK_SRC] = &mclk1_clk_src.clkr,
	[MCLK2_CLK_SRC] = &mclk2_clk_src.clkr,
	[MCLK3_CLK_SRC] = &mclk3_clk_src.clkr,
	[MDP_CLK_SRC] = &mdp_clk_src.clkr,
	[MMPLL0_PLL] = &mmpll0_pll_out_main.clkr,
	[MMPLL10_PLL] = &mmpll10_pll_out_main.clkr,
	[MMPLL3_PLL] = &mmpll3_pll_out_main.clkr,
	[MMPLL4_PLL] = &mmpll4_pll_out_main.clkr,
	[MMPLL5_PLL] = &mmpll5_pll_out_main.clkr,
	[MMPLL6_PLL] = &mmpll6_pll_out_main.clkr,
	[MMPLL7_PLL] = &mmpll7_pll_out_main.clkr,
	[MMPLL8_PLL] = &mmpll8_pll_out_main.clkr,
	[MMSS_BIMC_SMMU_AHB_CLK] = &mmss_bimc_smmu_ahb_clk.clkr,
	[MMSS_BIMC_SMMU_AXI_CLK] = &mmss_bimc_smmu_axi_clk.clkr,
	[MMSS_CAMSS_AHB_CLK] = &mmss_camss_ahb_clk.clkr,
	[MMSS_CAMSS_CCI_AHB_CLK] = &mmss_camss_cci_ahb_clk.clkr,
	[MMSS_CAMSS_CCI_CLK] = &mmss_camss_cci_clk.clkr,
	[MMSS_CAMSS_CPHY_CSID0_CLK] = &mmss_camss_cphy_csid0_clk.clkr,
	[MMSS_CAMSS_CPHY_CSID1_CLK] = &mmss_camss_cphy_csid1_clk.clkr,
	[MMSS_CAMSS_CPHY_CSID2_CLK] = &mmss_camss_cphy_csid2_clk.clkr,
	[MMSS_CAMSS_CPHY_CSID3_CLK] = &mmss_camss_cphy_csid3_clk.clkr,
	[MMSS_CAMSS_CPP_AHB_CLK] = &mmss_camss_cpp_ahb_clk.clkr,
	[MMSS_CAMSS_CPP_AXI_CLK] = &mmss_camss_cpp_axi_clk.clkr,
	[MMSS_CAMSS_CPP_CLK] = &mmss_camss_cpp_clk.clkr,
	[MMSS_CAMSS_CPP_VBIF_AHB_CLK] = &mmss_camss_cpp_vbif_ahb_clk.clkr,
	[MMSS_CAMSS_CSI0_AHB_CLK] = &mmss_camss_csi0_ahb_clk.clkr,
	[MMSS_CAMSS_CSI0_CLK] = &mmss_camss_csi0_clk.clkr,
	[MMSS_CAMSS_CSI0PHYTIMER_CLK] = &mmss_camss_csi0phytimer_clk.clkr,
	[MMSS_CAMSS_CSI0PIX_CLK] = &mmss_camss_csi0pix_clk.clkr,
	[MMSS_CAMSS_CSI0RDI_CLK] = &mmss_camss_csi0rdi_clk.clkr,
	[MMSS_CAMSS_CSI1_AHB_CLK] = &mmss_camss_csi1_ahb_clk.clkr,
	[MMSS_CAMSS_CSI1_CLK] = &mmss_camss_csi1_clk.clkr,
	[MMSS_CAMSS_CSI1PHYTIMER_CLK] = &mmss_camss_csi1phytimer_clk.clkr,
	[MMSS_CAMSS_CSI1PIX_CLK] = &mmss_camss_csi1pix_clk.clkr,
	[MMSS_CAMSS_CSI1RDI_CLK] = &mmss_camss_csi1rdi_clk.clkr,
	[MMSS_CAMSS_CSI2_AHB_CLK] = &mmss_camss_csi2_ahb_clk.clkr,
	[MMSS_CAMSS_CSI2_CLK] = &mmss_camss_csi2_clk.clkr,
	[MMSS_CAMSS_CSI2PHYTIMER_CLK] = &mmss_camss_csi2phytimer_clk.clkr,
	[MMSS_CAMSS_CSI2PIX_CLK] = &mmss_camss_csi2pix_clk.clkr,
	[MMSS_CAMSS_CSI2RDI_CLK] = &mmss_camss_csi2rdi_clk.clkr,
	[MMSS_CAMSS_CSI3_AHB_CLK] = &mmss_camss_csi3_ahb_clk.clkr,
	[MMSS_CAMSS_CSI3_CLK] = &mmss_camss_csi3_clk.clkr,
	[MMSS_CAMSS_CSI3PIX_CLK] = &mmss_camss_csi3pix_clk.clkr,
	[MMSS_CAMSS_CSI3RDI_CLK] = &mmss_camss_csi3rdi_clk.clkr,
	[MMSS_CAMSS_CSI_VFE0_CLK] = &mmss_camss_csi_vfe0_clk.clkr,
	[MMSS_CAMSS_CSI_VFE1_CLK] = &mmss_camss_csi_vfe1_clk.clkr,
	[MMSS_CAMSS_CSIPHY0_CLK] = &mmss_camss_csiphy0_clk.clkr,
	[MMSS_CAMSS_CSIPHY1_CLK] = &mmss_camss_csiphy1_clk.clkr,
	[MMSS_CAMSS_CSIPHY2_CLK] = &mmss_camss_csiphy2_clk.clkr,
	[MMSS_CAMSS_GP0_CLK] = &mmss_camss_gp0_clk.clkr,
	[MMSS_CAMSS_GP1_CLK] = &mmss_camss_gp1_clk.clkr,
	[MMSS_CAMSS_ISPIF_AHB_CLK] = &mmss_camss_ispif_ahb_clk.clkr,
	[MMSS_CAMSS_JPEG0_CLK] = &mmss_camss_jpeg0_clk.clkr,
	[MMSS_CAMSS_JPEG_AHB_CLK] = &mmss_camss_jpeg_ahb_clk.clkr,
	[MMSS_CAMSS_JPEG_AXI_CLK] = &mmss_camss_jpeg_axi_clk.clkr,
	[MMSS_CAMSS_MCLK0_CLK] = &mmss_camss_mclk0_clk.clkr,
	[MMSS_CAMSS_MCLK1_CLK] = &mmss_camss_mclk1_clk.clkr,
	[MMSS_CAMSS_MCLK2_CLK] = &mmss_camss_mclk2_clk.clkr,
	[MMSS_CAMSS_MCLK3_CLK] = &mmss_camss_mclk3_clk.clkr,
	[MMSS_CAMSS_MICRO_AHB_CLK] = &mmss_camss_micro_ahb_clk.clkr,
	[MMSS_CAMSS_TOP_AHB_CLK] = &mmss_camss_top_ahb_clk.clkr,
	[MMSS_CAMSS_VFE0_AHB_CLK] = &mmss_camss_vfe0_ahb_clk.clkr,
	[MMSS_CAMSS_VFE0_CLK] = &mmss_camss_vfe0_clk.clkr,
	[MMSS_CAMSS_VFE0_STREAM_CLK] = &mmss_camss_vfe0_stream_clk.clkr,
	[MMSS_CAMSS_VFE1_AHB_CLK] = &mmss_camss_vfe1_ahb_clk.clkr,
	[MMSS_CAMSS_VFE1_CLK] = &mmss_camss_vfe1_clk.clkr,
	[MMSS_CAMSS_VFE1_STREAM_CLK] = &mmss_camss_vfe1_stream_clk.clkr,
	[MMSS_CAMSS_VFE_VBIF_AHB_CLK] = &mmss_camss_vfe_vbif_ahb_clk.clkr,
	[MMSS_CAMSS_VFE_VBIF_AXI_CLK] = &mmss_camss_vfe_vbif_axi_clk.clkr,
	[MMSS_CSIPHY_AHB2CRIF_CLK] = &mmss_csiphy_ahb2crif_clk.clkr,
	[MMSS_MDSS_AHB_CLK] = &mmss_mdss_ahb_clk.clkr,
	[MMSS_MDSS_AXI_CLK] = &mmss_mdss_axi_clk.clkr,
	[MMSS_MDSS_BYTE0_CLK] = &mmss_mdss_byte0_clk.clkr,
	[MMSS_MDSS_BYTE0_INTF_CLK] = &mmss_mdss_byte0_intf_clk.clkr,
	[MMSS_MDSS_BYTE0_INTF_DIV_CLK] = &mmss_mdss_byte0_intf_div_clk.clkr,
	[MMSS_MDSS_BYTE1_CLK] = &mmss_mdss_byte1_clk.clkr,
	[MMSS_MDSS_BYTE1_INTF_CLK] = &mmss_mdss_byte1_intf_clk.clkr,
	[MMSS_MDSS_DP_AUX_CLK] = &mmss_mdss_dp_aux_clk.clkr,
	[MMSS_MDSS_DP_CRYPTO_CLK] = &mmss_mdss_dp_crypto_clk.clkr,
	[MMSS_MDSS_DP_GTC_CLK] = &mmss_mdss_dp_gtc_clk.clkr,
	[MMSS_MDSS_DP_LINK_CLK] = &mmss_mdss_dp_link_clk.clkr,
	[MMSS_MDSS_DP_LINK_INTF_CLK] = &mmss_mdss_dp_link_intf_clk.clkr,
	[MMSS_MDSS_DP_PIXEL_CLK] = &mmss_mdss_dp_pixel_clk.clkr,
	[MMSS_MDSS_ESC0_CLK] = &mmss_mdss_esc0_clk.clkr,
	[MMSS_MDSS_ESC1_CLK] = &mmss_mdss_esc1_clk.clkr,
	[MMSS_MDSS_HDMI_DP_AHB_CLK] = &mmss_mdss_hdmi_dp_ahb_clk.clkr,
	[MMSS_MDSS_MDP_CLK] = &mmss_mdss_mdp_clk.clkr,
	[MMSS_MDSS_PCLK0_CLK] = &mmss_mdss_pclk0_clk.clkr,
	[MMSS_MDSS_PCLK1_CLK] = &mmss_mdss_pclk1_clk.clkr,
	[MMSS_MDSS_ROT_CLK] = &mmss_mdss_rot_clk.clkr,
	[MMSS_MDSS_VSYNC_CLK] = &mmss_mdss_vsync_clk.clkr,
	[MMSS_MISC_AHB_CLK] = &mmss_misc_ahb_clk.clkr,
	[MMSS_MISC_CXO_CLK] = &mmss_misc_cxo_clk.clkr,
	[MMSS_MNOC_AHB_CLK] = &mmss_mnoc_ahb_clk.clkr,
	[MMSS_SNOC_DVM_AXI_CLK] = &mmss_snoc_dvm_axi_clk.clkr,
	[MMSS_THROTTLE_CAMSS_AXI_CLK] = &mmss_throttle_camss_axi_clk.clkr,
	[MMSS_THROTTLE_MDSS_AXI_CLK] = &mmss_throttle_mdss_axi_clk.clkr,
	[MMSS_THROTTLE_VIDEO_AXI_CLK] = &mmss_throttle_video_axi_clk.clkr,
	[MMSS_VIDEO_AHB_CLK] = &mmss_video_ahb_clk.clkr,
	[MMSS_VIDEO_AXI_CLK] = &mmss_video_axi_clk.clkr,
	[MMSS_VIDEO_CORE_CLK] = &mmss_video_core_clk.clkr,
	[MMSS_VIDEO_SUBCORE0_CLK] = &mmss_video_subcore0_clk.clkr,
	[PCLK0_CLK_SRC] = &pclk0_clk_src.clkr,
	[PCLK1_CLK_SRC] = &pclk1_clk_src.clkr,
	[ROT_CLK_SRC] = &rot_clk_src.clkr,
	[VFE0_CLK_SRC] = &vfe0_clk_src.clkr,
	[VFE1_CLK_SRC] = &vfe1_clk_src.clkr,
	[VIDEO_CORE_CLK_SRC] = &video_core_clk_src.clkr,
	[VSYNC_CLK_SRC] = &vsync_clk_src.clkr,
	[MMSS_MDSS_BYTE1_INTF_DIV_CLK] = &mmss_mdss_byte1_intf_div_clk.clkr,
};

static const struct qcom_reset_map mmcc_660_resets[] = {
	[CAMSS_MICRO_BCR] = { 0x3490 },
};

static const struct regmap_config mmcc_660_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x10004,
	.fast_io	= true,
};

static const struct qcom_cc_desc mmcc_660_desc = {
	.config = &mmcc_660_regmap_config,
	.clks = mmcc_660_clocks,
	.num_clks = ARRAY_SIZE(mmcc_660_clocks),
	.hwclks = mmcc_sdm660_hws,
	.num_hwclks = ARRAY_SIZE(mmcc_sdm660_hws),
	.resets = mmcc_660_resets,
	.num_resets = ARRAY_SIZE(mmcc_660_resets),
};

static const struct of_device_id mmcc_660_match_table[] = {
	{ .compatible = "qcom,mmcc-sdm660" },
	{ .compatible = "qcom,mmcc-sdm630" },
	{ }
};
MODULE_DEVICE_TABLE(of, mmcc_660_match_table);

static int mmcc_660_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct regmap *regmap;
	bool is_sdm630 = 0;

	regmap = qcom_cc_map(pdev, &mmcc_660_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	is_sdm630 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,mmcc-sdm630");

	/* PLLs connected on Mx rails of MMSS_CC  */
	vdd_mx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mx_mmss");
	if (IS_ERR(vdd_mx.regulator[0])) {
		if (!(PTR_ERR(vdd_mx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_mx_mmss regulator\n");
		return PTR_ERR(vdd_mx.regulator[0]);
	}

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig_mmss");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_dig regulator\n");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	/* MMPLL10 connected to the Analog Rail */
	vdda.regulator[0] = devm_regulator_get(&pdev->dev, "vdda");
	if (IS_ERR(vdda.regulator[0])) {
		if (!(PTR_ERR(vdda.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdda regulator\n");
		return PTR_ERR(vdda.regulator[0]);
	}

	clk_alpha_pll_configure(&mmpll3_pll_out_main, regmap, &mmpll3_config);
	clk_alpha_pll_configure(&mmpll4_pll_out_main, regmap, &mmpll4_config);
	clk_alpha_pll_configure(&mmpll5_pll_out_main, regmap, &mmpll5_config);
	clk_alpha_pll_configure(&mmpll7_pll_out_main, regmap, &mmpll7_config);
	clk_alpha_pll_configure(&mmpll8_pll_out_main, regmap, &mmpll8_config);
	clk_alpha_pll_configure(&mmpll10_pll_out_main, regmap, &mmpll10_config);

	if (is_sdm630) {
		mmcc_660_desc.clks[BYTE1_CLK_SRC] = 0;
		mmcc_660_desc.clks[MMSS_MDSS_BYTE1_CLK] = 0;
		mmcc_660_desc.clks[MMSS_MDSS_BYTE1_INTF_DIV_CLK] = 0;
		mmcc_660_desc.clks[MMSS_MDSS_BYTE1_INTF_CLK] = 0;
		mmcc_660_desc.clks[ESC1_CLK_SRC] = 0;
		mmcc_660_desc.clks[MMSS_MDSS_ESC1_CLK] = 0;
		mmcc_660_desc.clks[PCLK1_CLK_SRC] = 0;
		mmcc_660_desc.clks[MMSS_MDSS_PCLK1_CLK] = 0;
	}

	ret = qcom_cc_really_probe(pdev, &mmcc_660_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register MMSS clocks\n");
		return ret;
	}

	dev_err(&pdev->dev, "Registered MMSS clocks\n");

	return ret;
}

static struct platform_driver mmcc_660_driver = {
	.probe		= mmcc_660_probe,
	.driver		= {
		.name	= "mmcc-sdm660",
		.of_match_table = mmcc_660_match_table,
	},
};

static int __init mmcc_660_init(void)
{
	return platform_driver_register(&mmcc_660_driver);
}
core_initcall_sync(mmcc_660_init);

static void __exit mmcc_660_exit(void)
{
	platform_driver_unregister(&mmcc_660_driver);
}
module_exit(mmcc_660_exit);

/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include <dt-bindings/clock/qcom,camcc-sdm845.h>

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
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_CX_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_CAM_CC_PLL0_OUT_EVEN,
	P_CAM_CC_PLL1_OUT_EVEN,
	P_CAM_CC_PLL2_OUT_EVEN,
	P_CAM_CC_PLL2_OUT_ODD,
	P_CAM_CC_PLL3_OUT_EVEN,
	P_CORE_BI_PLL_TEST_SE,
};

static const struct parent_map cam_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_CAM_CC_PLL2_OUT_EVEN, 1 },
	{ P_CAM_CC_PLL1_OUT_EVEN, 2 },
	{ P_CAM_CC_PLL3_OUT_EVEN, 5 },
	{ P_CAM_CC_PLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const cam_cc_parent_names_0[] = {
	"bi_tcxo",
	"cam_cc_pll2_out_even",
	"cam_cc_pll1_out_even",
	"cam_cc_pll3_out_even",
	"cam_cc_pll0_out_even",
	"core_bi_pll_test_se",
};

static const struct parent_map cam_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_CAM_CC_PLL2_OUT_EVEN, 1 },
	{ P_CAM_CC_PLL1_OUT_EVEN, 2 },
	{ P_CAM_CC_PLL2_OUT_ODD, 4 },
	{ P_CAM_CC_PLL3_OUT_EVEN, 5 },
	{ P_CAM_CC_PLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const cam_cc_parent_names_1[] = {
	"bi_tcxo",
	"cam_cc_pll2_out_even",
	"cam_cc_pll1_out_even",
	"cam_cc_pll2_out_odd",
	"cam_cc_pll3_out_even",
	"cam_cc_pll0_out_even",
	"core_bi_pll_test_se",
};

static struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
	{ 125000000, 1000000000, 1 },
};

static const struct pll_config cam_cc_pll0_config = {
	.l = 0x1f,
	.frac = 0x4000,
};

static struct clk_alpha_pll cam_cc_pll0 = {
	.offset = 0x0,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll0",
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

static const struct clk_div_table post_div_table_fabia_even[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{ }
};

static struct clk_alpha_pll_postdiv cam_cc_pll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_fabia_even,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll0_out_even",
		.parent_names = (const char *[]){ "cam_cc_pll0" },
		.num_parents = 1,
		.ops = &clk_generic_pll_postdiv_ops,
	},
};

static const struct pll_config cam_cc_pll1_config = {
	.l = 0x2a,
	.frac = 0x1556,
};

static struct clk_alpha_pll cam_cc_pll1 = {
	.offset = 0x1000,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll1",
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

static struct clk_alpha_pll_postdiv cam_cc_pll1_out_even = {
	.offset = 0x1000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_fabia_even,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll1_out_even",
		.parent_names = (const char *[]){ "cam_cc_pll1" },
		.num_parents = 1,
		.ops = &clk_generic_pll_postdiv_ops,
	},
};

static const struct pll_config cam_cc_pll2_config = {
	.l = 0x32,
	.frac = 0x0,
};

static struct clk_alpha_pll cam_cc_pll2 = {
	.offset = 0x2000,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll2",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_fabia_pll_ops,
			VDD_MX_FMAX_MAP4(
				MIN, 615000000,
				LOW, 1066000000,
				LOW_L1, 1600000000,
				NOMINAL, 2000000000),
		},
	},
};

static struct clk_alpha_pll_postdiv cam_cc_pll2_out_even = {
	.offset = 0x2000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_fabia_even,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll2_out_even",
		.parent_names = (const char *[]){ "cam_cc_pll2" },
		.num_parents = 1,
		.ops = &clk_generic_pll_postdiv_ops,
	},
};

static const struct clk_div_table post_div_table_fabia_odd[] = {
	{ 0x0, 1 },
	{ 0x3, 3 },
	{ 0x5, 5 },
	{ 0x7, 7 },
	{ }
};

static struct clk_alpha_pll_postdiv cam_cc_pll2_out_odd = {
	.offset = 0x2000,
	.post_div_shift = 12,
	.post_div_table = post_div_table_fabia_odd,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_odd),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll2_out_odd",
		.parent_names = (const char *[]){ "cam_cc_pll2" },
		.num_parents = 1,
		.ops = &clk_generic_pll_postdiv_ops,
	},
};

static const struct pll_config cam_cc_pll3_config = {
	.l = 0x14,
	.frac = 0x0,
};

static struct clk_alpha_pll cam_cc_pll3 = {
	.offset = 0x3000,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll3",
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

static struct clk_alpha_pll_postdiv cam_cc_pll3_out_even = {
	.offset = 0x3000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_fabia_even,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll3_out_even",
		.parent_names = (const char *[]){ "cam_cc_pll3" },
		.num_parents = 1,
		.ops = &clk_generic_pll_postdiv_ops,
	},
};

static const struct freq_tbl ftbl_cam_cc_bps_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(100000000, P_CAM_CC_PLL0_OUT_EVEN, 6, 0, 0),
	F(200000000, P_CAM_CC_PLL0_OUT_EVEN, 3, 0, 0),
	F(404000000, P_CAM_CC_PLL1_OUT_EVEN, 2, 0, 0),
	F(480000000, P_CAM_CC_PLL2_OUT_EVEN, 1, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_bps_clk_src = {
	.cmd_rcgr = 0x600c,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_bps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_bps_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 200000000,
			LOW, 404000000,
			LOW_L1, 480000000,
			NOMINAL, 600000000),
	},
};

static const struct freq_tbl ftbl_cam_cc_cci_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(37500000, P_CAM_CC_PLL0_OUT_EVEN, 16, 0, 0),
	F(50000000, P_CAM_CC_PLL0_OUT_EVEN, 12, 0, 0),
	F(100000000, P_CAM_CC_PLL0_OUT_EVEN, 6, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_cci_clk_src = {
	.cmd_rcgr = 0xb0d8,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_cci_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_cci_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 37500000,
			LOW, 50000000,
			NOMINAL, 100000000),
	},
};

static const struct freq_tbl ftbl_cam_cc_cphy_rx_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(300000000, P_CAM_CC_PLL0_OUT_EVEN, 2, 0, 0),
	F(320000000, P_CAM_CC_PLL2_OUT_ODD, 3, 0, 0),
	F(384000000, P_CAM_CC_PLL3_OUT_EVEN, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_cam_cc_cphy_rx_clk_src_sdm845_v2[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(384000000, P_CAM_CC_PLL3_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_cphy_rx_clk_src = {
	.cmd_rcgr = 0x9060,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_cphy_rx_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_cphy_rx_clk_src",
		.parent_names = cam_cc_parent_names_1,
		.num_parents = 7,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 300000000,
			LOW, 320000000,
			HIGH, 384000000),
	},
};

static const struct freq_tbl ftbl_cam_cc_csi0phytimer_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(240000000, P_CAM_CC_PLL2_OUT_EVEN, 2, 0, 0),
	F(269333333, P_CAM_CC_PLL1_OUT_EVEN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_csi0phytimer_clk_src = {
	.cmd_rcgr = 0x5004,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi0phytimer_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 240000000,
			LOW, 269333333),
	},
};

static struct clk_rcg2 cam_cc_csi1phytimer_clk_src = {
	.cmd_rcgr = 0x5028,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi1phytimer_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 240000000,
			LOW, 269333333),
	},
};

static struct clk_rcg2 cam_cc_csi2phytimer_clk_src = {
	.cmd_rcgr = 0x504c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi2phytimer_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 240000000,
			LOW, 269333333),
	},
};

static struct clk_rcg2 cam_cc_csi3phytimer_clk_src = {
	.cmd_rcgr = 0x5070,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi3phytimer_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 240000000,
			LOW, 269333333),
	},
};

static const struct freq_tbl ftbl_cam_cc_fast_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(50000000, P_CAM_CC_PLL0_OUT_EVEN, 12, 0, 0),
	F(100000000, P_CAM_CC_PLL0_OUT_EVEN, 6, 0, 0),
	F(200000000, P_CAM_CC_PLL0_OUT_EVEN, 3, 0, 0),
	F(300000000, P_CAM_CC_PLL0_OUT_EVEN, 2, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_EVEN, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_fast_ahb_clk_src = {
	.cmd_rcgr = 0x6038,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_fast_ahb_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_fast_ahb_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 100000000,
			LOW, 200000000,
			LOW_L1, 300000000,
			NOMINAL, 400000000),
	},
};

static const struct freq_tbl ftbl_cam_cc_fd_core_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(320000000, P_CAM_CC_PLL2_OUT_EVEN, 1.5, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_EVEN, 1.5, 0, 0),
	F(538666667, P_CAM_CC_PLL1_OUT_EVEN, 1.5, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_cam_cc_fd_core_clk_src_sdm845_v2[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(384000000, P_CAM_CC_PLL3_OUT_EVEN, 1, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_EVEN, 1.5, 0, 0),
	F(538666667, P_CAM_CC_PLL1_OUT_EVEN, 1.5, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_fd_core_clk_src = {
	.cmd_rcgr = 0xb0b0,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_fd_core_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_fd_core_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 320000000,
			LOW, 400000000,
			LOW_L1, 538666667,
			NOMINAL, 600000000),
	},
};

static const struct freq_tbl ftbl_cam_cc_icp_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(320000000, P_CAM_CC_PLL2_OUT_EVEN, 1.5, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_EVEN, 1.5, 0, 0),
	F(538666667, P_CAM_CC_PLL1_OUT_EVEN, 1.5, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_cam_cc_icp_clk_src_sdm845_v2[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(384000000, P_CAM_CC_PLL3_OUT_EVEN, 1, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_EVEN, 1.5, 0, 0),
	F(538666667, P_CAM_CC_PLL1_OUT_EVEN, 1.5, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_icp_clk_src = {
	.cmd_rcgr = 0xb088,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_icp_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_icp_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 320000000,
			LOW, 400000000,
			LOW_L1, 538666667,
			NOMINAL, 600000000),
	},
};

static const struct freq_tbl ftbl_cam_cc_ife_0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(100000000, P_CAM_CC_PLL0_OUT_EVEN, 6, 0, 0),
	F(320000000, P_CAM_CC_PLL2_OUT_EVEN, 1.5, 0, 0),
	F(404000000, P_CAM_CC_PLL1_OUT_EVEN, 2, 0, 0),
	F(480000000, P_CAM_CC_PLL2_OUT_EVEN, 1, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ife_0_clk_src = {
	.cmd_rcgr = 0x900c,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_ife_0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_0_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 320000000,
			LOW, 404000000,
			LOW_L1, 480000000,
			NOMINAL, 600000000),
	},
};

static const struct freq_tbl ftbl_cam_cc_ife_0_csid_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(75000000, P_CAM_CC_PLL0_OUT_EVEN, 8, 0, 0),
	F(384000000, P_CAM_CC_PLL3_OUT_EVEN, 1, 0, 0),
	F(538666667, P_CAM_CC_PLL1_OUT_EVEN, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ife_0_csid_clk_src = {
	.cmd_rcgr = 0x9038,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_ife_0_csid_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_0_csid_clk_src",
		.parent_names = cam_cc_parent_names_1,
		.num_parents = 7,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 384000000,
			NOMINAL, 538666667),
	},
};

static struct clk_rcg2 cam_cc_ife_1_clk_src = {
	.cmd_rcgr = 0xa00c,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_ife_0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_1_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 320000000,
			LOW, 404000000,
			LOW_L1, 480000000,
			NOMINAL, 600000000),
	},
};

static struct clk_rcg2 cam_cc_ife_1_csid_clk_src = {
	.cmd_rcgr = 0xa030,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_ife_0_csid_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_1_csid_clk_src",
		.parent_names = cam_cc_parent_names_1,
		.num_parents = 7,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 384000000,
			NOMINAL, 538666667),
	},
};

static struct clk_rcg2 cam_cc_ife_lite_clk_src = {
	.cmd_rcgr = 0xb004,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_ife_0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_lite_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 320000000,
			LOW, 404000000,
			LOW_L1, 480000000,
			NOMINAL, 600000000),
	},
};

static struct clk_rcg2 cam_cc_ife_lite_csid_clk_src = {
	.cmd_rcgr = 0xb024,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_ife_0_csid_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_lite_csid_clk_src",
		.parent_names = cam_cc_parent_names_1,
		.num_parents = 7,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 384000000,
			NOMINAL, 538666667),
	},
};

static const struct freq_tbl ftbl_cam_cc_ipe_0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(100000000, P_CAM_CC_PLL0_OUT_EVEN, 6, 0, 0),
	F(240000000, P_CAM_CC_PLL0_OUT_EVEN, 2.5, 0, 0),
	F(404000000, P_CAM_CC_PLL1_OUT_EVEN, 2, 0, 0),
	F(480000000, P_CAM_CC_PLL2_OUT_EVEN, 1, 0, 0),
	F(538666667, P_CAM_CC_PLL1_OUT_EVEN, 1.5, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ipe_0_clk_src = {
	.cmd_rcgr = 0x700c,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_ipe_0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ipe_0_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP6(
			MIN, 19200000,
			LOWER, 240000000,
			LOW, 404000000,
			LOW_L1, 480000000,
			NOMINAL, 538666667,
			HIGH, 600000000),
	},
};

static struct clk_rcg2 cam_cc_ipe_1_clk_src = {
	.cmd_rcgr = 0x800c,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_ipe_0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ipe_1_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP6(
			MIN, 19200000,
			LOWER, 240000000,
			LOW, 404000000,
			LOW_L1, 480000000,
			NOMINAL, 538666667,
			HIGH, 600000000),
	},
};

static struct clk_rcg2 cam_cc_jpeg_clk_src = {
	.cmd_rcgr = 0xb04c,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_bps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_jpeg_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 200000000,
			LOW, 404000000,
			LOW_L1, 480000000,
			NOMINAL, 600000000),
	},
};

static const struct freq_tbl ftbl_cam_cc_lrme_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(100000000, P_CAM_CC_PLL0_OUT_EVEN, 6, 0, 0),
	F(200000000, P_CAM_CC_PLL0_OUT_EVEN, 3, 0, 0),
	F(384000000, P_CAM_CC_PLL2_OUT_ODD, 2.5, 0, 0),
	F(480000000, P_CAM_CC_PLL2_OUT_EVEN, 1, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_cam_cc_lrme_clk_src_sdm845_v2[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(100000000, P_CAM_CC_PLL0_OUT_EVEN, 6, 0, 0),
	F(200000000, P_CAM_CC_PLL0_OUT_EVEN, 3, 0, 0),
	F(269333333, P_CAM_CC_PLL1_OUT_EVEN, 3, 0, 0),
	F(320000000, P_CAM_CC_PLL2_OUT_EVEN, 1.5, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_EVEN, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_lrme_clk_src = {
	.cmd_rcgr = 0xb0f8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_1,
	.enable_safe_config = true,
	.freq_tbl = ftbl_cam_cc_lrme_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_lrme_clk_src",
		.parent_names = cam_cc_parent_names_1,
		.num_parents = 7,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 200000000,
			LOW, 384000000,
			LOW_L1, 480000000,
			NOMINAL, 600000000),
	},
};

static const struct freq_tbl ftbl_cam_cc_mclk0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(24000000, P_CAM_CC_PLL2_OUT_EVEN, 10, 1, 2),
	F(33333333, P_CAM_CC_PLL0_OUT_EVEN, 2, 1, 9),
	F(34285714, P_CAM_CC_PLL2_OUT_EVEN, 14, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_mclk0_clk_src = {
	.cmd_rcgr = 0x4004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk0_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP2(
			MIN, 19200000,
			LOWER, 34285714),
	},
};

static struct clk_rcg2 cam_cc_mclk1_clk_src = {
	.cmd_rcgr = 0x4024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk1_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP2(
			MIN, 19200000,
			LOWER, 34285714),
	},
};

static struct clk_rcg2 cam_cc_mclk2_clk_src = {
	.cmd_rcgr = 0x4044,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk2_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP2(
			MIN, 19200000,
			LOWER, 34285714),
	},
};

static struct clk_rcg2 cam_cc_mclk3_clk_src = {
	.cmd_rcgr = 0x4064,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk3_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP2(
			MIN, 19200000,
			LOWER, 34285714),
	},
};

static const struct freq_tbl ftbl_cam_cc_slow_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(60000000, P_CAM_CC_PLL0_OUT_EVEN, 10, 0, 0),
	F(66666667, P_CAM_CC_PLL0_OUT_EVEN, 9, 0, 0),
	F(73846154, P_CAM_CC_PLL2_OUT_EVEN, 6.5, 0, 0),
	F(80000000, P_CAM_CC_PLL2_OUT_EVEN, 6, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_slow_ahb_clk_src = {
	.cmd_rcgr = 0x6054,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_slow_ahb_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_slow_ahb_clk_src",
		.parent_names = cam_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 19200000,
			LOWER, 60000000,
			LOW, 66666667,
			LOW_L1, 73846154,
			NOMINAL, 80000000),
	},
};

static struct clk_branch cam_cc_bps_ahb_clk = {
	.halt_reg = 0x606c,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x606c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_bps_ahb_clk",
			.parent_names = (const char *[]){
				"cam_cc_slow_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_bps_areg_clk = {
	.halt_reg = 0x6050,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x6050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_bps_areg_clk",
			.parent_names = (const char *[]){
				"cam_cc_fast_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_bps_axi_clk = {
	.halt_reg = 0x6034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x6034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_bps_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_bps_clk = {
	.halt_reg = 0x6024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x6024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_bps_clk",
			.parent_names = (const char *[]){
				"cam_cc_bps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_camnoc_atb_clk = {
	.halt_reg = 0xb12c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb12c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_camnoc_atb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_camnoc_axi_clk = {
	.halt_reg = 0xb124,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb124,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_camnoc_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_cci_clk = {
	.halt_reg = 0xb0f0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb0f0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_cci_clk",
			.parent_names = (const char *[]){
				"cam_cc_cci_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_cpas_ahb_clk = {
	.halt_reg = 0xb11c,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0xb11c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_cpas_ahb_clk",
			.parent_names = (const char *[]){
				"cam_cc_slow_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi0phytimer_clk = {
	.halt_reg = 0x501c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x501c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi0phytimer_clk",
			.parent_names = (const char *[]){
				"cam_cc_csi0phytimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi1phytimer_clk = {
	.halt_reg = 0x5040,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x5040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi1phytimer_clk",
			.parent_names = (const char *[]){
				"cam_cc_csi1phytimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi2phytimer_clk = {
	.halt_reg = 0x5064,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x5064,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi2phytimer_clk",
			.parent_names = (const char *[]){
				"cam_cc_csi2phytimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi3phytimer_clk = {
	.halt_reg = 0x5088,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x5088,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi3phytimer_clk",
			.parent_names = (const char *[]){
				"cam_cc_csi3phytimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy0_clk = {
	.halt_reg = 0x5020,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x5020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy0_clk",
			.parent_names = (const char *[]){
				"cam_cc_cphy_rx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy1_clk = {
	.halt_reg = 0x5044,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x5044,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy1_clk",
			.parent_names = (const char *[]){
				"cam_cc_cphy_rx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy2_clk = {
	.halt_reg = 0x5068,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x5068,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy2_clk",
			.parent_names = (const char *[]){
				"cam_cc_cphy_rx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy3_clk = {
	.halt_reg = 0x508c,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x508c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy3_clk",
			.parent_names = (const char *[]){
				"cam_cc_cphy_rx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_fd_core_clk = {
	.halt_reg = 0xb0c8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb0c8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_fd_core_clk",
			.parent_names = (const char *[]){
				"cam_cc_fd_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_fd_core_uar_clk = {
	.halt_reg = 0xb0d0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb0d0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_fd_core_uar_clk",
			.parent_names = (const char *[]){
				"cam_cc_fd_core_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_icp_apb_clk = {
	.halt_reg = 0xb084,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb084,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_icp_apb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_icp_atb_clk = {
	.halt_reg = 0xb078,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb078,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_icp_atb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_icp_clk = {
	.halt_reg = 0xb0a0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb0a0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_icp_clk",
			.parent_names = (const char *[]){
				"cam_cc_icp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_icp_cti_clk = {
	.halt_reg = 0xb07c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb07c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_icp_cti_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_icp_ts_clk = {
	.halt_reg = 0xb080,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb080,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_icp_ts_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_axi_clk = {
	.halt_reg = 0x907c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x907c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_clk = {
	.halt_reg = 0x9024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_clk",
			.parent_names = (const char *[]){
				"cam_cc_ife_0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_cphy_rx_clk = {
	.halt_reg = 0x9078,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x9078,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_cphy_rx_clk",
			.parent_names = (const char *[]){
				"cam_cc_cphy_rx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_csid_clk = {
	.halt_reg = 0x9050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_csid_clk",
			.parent_names = (const char *[]){
				"cam_cc_ife_0_csid_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_dsp_clk = {
	.halt_reg = 0x9034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_dsp_clk",
			.parent_names = (const char *[]){
				"cam_cc_ife_0_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_axi_clk = {
	.halt_reg = 0xa054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_clk = {
	.halt_reg = 0xa024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_clk",
			.parent_names = (const char *[]){
				"cam_cc_ife_1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_cphy_rx_clk = {
	.halt_reg = 0xa050,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0xa050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_cphy_rx_clk",
			.parent_names = (const char *[]){
				"cam_cc_cphy_rx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_csid_clk = {
	.halt_reg = 0xa048,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_csid_clk",
			.parent_names = (const char *[]){
				"cam_cc_ife_1_csid_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_dsp_clk = {
	.halt_reg = 0xa02c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa02c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_dsp_clk",
			.parent_names = (const char *[]){
				"cam_cc_ife_1_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_lite_clk = {
	.halt_reg = 0xb01c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb01c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_lite_clk",
			.parent_names = (const char *[]){
				"cam_cc_ife_lite_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_lite_cphy_rx_clk = {
	.halt_reg = 0xb044,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0xb044,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_lite_cphy_rx_clk",
			.parent_names = (const char *[]){
				"cam_cc_cphy_rx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_lite_csid_clk = {
	.halt_reg = 0xb03c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb03c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_lite_csid_clk",
			.parent_names = (const char *[]){
				"cam_cc_ife_lite_csid_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_0_ahb_clk = {
	.halt_reg = 0x703c,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x703c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_0_ahb_clk",
			.parent_names = (const char *[]){
				"cam_cc_slow_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_0_areg_clk = {
	.halt_reg = 0x7038,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x7038,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_0_areg_clk",
			.parent_names = (const char *[]){
				"cam_cc_fast_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_0_axi_clk = {
	.halt_reg = 0x7034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_0_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_0_clk = {
	.halt_reg = 0x7024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_0_clk",
			.parent_names = (const char *[]){
				"cam_cc_ipe_0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_1_ahb_clk = {
	.halt_reg = 0x803c,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x803c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_1_ahb_clk",
			.parent_names = (const char *[]){
				"cam_cc_slow_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_1_areg_clk = {
	.halt_reg = 0x8038,
	.halt_check = BRANCH_HALT,
	.aggr_sibling_rates = true,
	.clkr = {
		.enable_reg = 0x8038,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_1_areg_clk",
			.parent_names = (const char *[]){
				"cam_cc_fast_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_1_axi_clk = {
	.halt_reg = 0x8034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_1_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_1_clk = {
	.halt_reg = 0x8024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_1_clk",
			.parent_names = (const char *[]){
				"cam_cc_ipe_1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_jpeg_clk = {
	.halt_reg = 0xb064,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb064,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_jpeg_clk",
			.parent_names = (const char *[]){
				"cam_cc_jpeg_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_lrme_clk = {
	.halt_reg = 0xb110,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb110,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_lrme_clk",
			.parent_names = (const char *[]){
				"cam_cc_lrme_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk0_clk = {
	.halt_reg = 0x401c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x401c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk0_clk",
			.parent_names = (const char *[]){
				"cam_cc_mclk0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk1_clk = {
	.halt_reg = 0x403c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x403c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk1_clk",
			.parent_names = (const char *[]){
				"cam_cc_mclk1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk2_clk = {
	.halt_reg = 0x405c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x405c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk2_clk",
			.parent_names = (const char *[]){
				"cam_cc_mclk2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk3_clk = {
	.halt_reg = 0x407c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x407c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk3_clk",
			.parent_names = (const char *[]){
				"cam_cc_mclk3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_pll_test_clk = {
	.halt_reg = 0xc014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll_test_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_soc_ahb_clk = {
	.halt_reg = 0xb13c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb13c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_soc_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sys_tmr_clk = {
	.halt_reg = 0xb0a8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb0a8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sys_tmr_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *cam_cc_sdm845_clocks[] = {
	[CAM_CC_BPS_AHB_CLK] = &cam_cc_bps_ahb_clk.clkr,
	[CAM_CC_BPS_AREG_CLK] = &cam_cc_bps_areg_clk.clkr,
	[CAM_CC_BPS_AXI_CLK] = &cam_cc_bps_axi_clk.clkr,
	[CAM_CC_BPS_CLK] = &cam_cc_bps_clk.clkr,
	[CAM_CC_BPS_CLK_SRC] = &cam_cc_bps_clk_src.clkr,
	[CAM_CC_CAMNOC_ATB_CLK] = &cam_cc_camnoc_atb_clk.clkr,
	[CAM_CC_CAMNOC_AXI_CLK] = &cam_cc_camnoc_axi_clk.clkr,
	[CAM_CC_CCI_CLK] = &cam_cc_cci_clk.clkr,
	[CAM_CC_CCI_CLK_SRC] = &cam_cc_cci_clk_src.clkr,
	[CAM_CC_CPAS_AHB_CLK] = &cam_cc_cpas_ahb_clk.clkr,
	[CAM_CC_CPHY_RX_CLK_SRC] = &cam_cc_cphy_rx_clk_src.clkr,
	[CAM_CC_CSI0PHYTIMER_CLK] = &cam_cc_csi0phytimer_clk.clkr,
	[CAM_CC_CSI0PHYTIMER_CLK_SRC] = &cam_cc_csi0phytimer_clk_src.clkr,
	[CAM_CC_CSI1PHYTIMER_CLK] = &cam_cc_csi1phytimer_clk.clkr,
	[CAM_CC_CSI1PHYTIMER_CLK_SRC] = &cam_cc_csi1phytimer_clk_src.clkr,
	[CAM_CC_CSI2PHYTIMER_CLK] = &cam_cc_csi2phytimer_clk.clkr,
	[CAM_CC_CSI2PHYTIMER_CLK_SRC] = &cam_cc_csi2phytimer_clk_src.clkr,
	[CAM_CC_CSI3PHYTIMER_CLK] = NULL,
	[CAM_CC_CSI3PHYTIMER_CLK_SRC] = NULL,
	[CAM_CC_CSIPHY0_CLK] = &cam_cc_csiphy0_clk.clkr,
	[CAM_CC_CSIPHY1_CLK] = &cam_cc_csiphy1_clk.clkr,
	[CAM_CC_CSIPHY2_CLK] = &cam_cc_csiphy2_clk.clkr,
	[CAM_CC_CSIPHY3_CLK] = NULL,
	[CAM_CC_FAST_AHB_CLK_SRC] = &cam_cc_fast_ahb_clk_src.clkr,
	[CAM_CC_FD_CORE_CLK] = &cam_cc_fd_core_clk.clkr,
	[CAM_CC_FD_CORE_CLK_SRC] = &cam_cc_fd_core_clk_src.clkr,
	[CAM_CC_FD_CORE_UAR_CLK] = &cam_cc_fd_core_uar_clk.clkr,
	[CAM_CC_ICP_APB_CLK] = &cam_cc_icp_apb_clk.clkr,
	[CAM_CC_ICP_ATB_CLK] = &cam_cc_icp_atb_clk.clkr,
	[CAM_CC_ICP_CLK] = &cam_cc_icp_clk.clkr,
	[CAM_CC_ICP_CLK_SRC] = &cam_cc_icp_clk_src.clkr,
	[CAM_CC_ICP_CTI_CLK] = &cam_cc_icp_cti_clk.clkr,
	[CAM_CC_ICP_TS_CLK] = &cam_cc_icp_ts_clk.clkr,
	[CAM_CC_IFE_0_AXI_CLK] = &cam_cc_ife_0_axi_clk.clkr,
	[CAM_CC_IFE_0_CLK] = &cam_cc_ife_0_clk.clkr,
	[CAM_CC_IFE_0_CLK_SRC] = &cam_cc_ife_0_clk_src.clkr,
	[CAM_CC_IFE_0_CPHY_RX_CLK] = &cam_cc_ife_0_cphy_rx_clk.clkr,
	[CAM_CC_IFE_0_CSID_CLK] = &cam_cc_ife_0_csid_clk.clkr,
	[CAM_CC_IFE_0_CSID_CLK_SRC] = &cam_cc_ife_0_csid_clk_src.clkr,
	[CAM_CC_IFE_0_DSP_CLK] = &cam_cc_ife_0_dsp_clk.clkr,
	[CAM_CC_IFE_1_AXI_CLK] = &cam_cc_ife_1_axi_clk.clkr,
	[CAM_CC_IFE_1_CLK] = &cam_cc_ife_1_clk.clkr,
	[CAM_CC_IFE_1_CLK_SRC] = &cam_cc_ife_1_clk_src.clkr,
	[CAM_CC_IFE_1_CPHY_RX_CLK] = &cam_cc_ife_1_cphy_rx_clk.clkr,
	[CAM_CC_IFE_1_CSID_CLK] = &cam_cc_ife_1_csid_clk.clkr,
	[CAM_CC_IFE_1_CSID_CLK_SRC] = &cam_cc_ife_1_csid_clk_src.clkr,
	[CAM_CC_IFE_1_DSP_CLK] = &cam_cc_ife_1_dsp_clk.clkr,
	[CAM_CC_IFE_LITE_CLK] = &cam_cc_ife_lite_clk.clkr,
	[CAM_CC_IFE_LITE_CLK_SRC] = &cam_cc_ife_lite_clk_src.clkr,
	[CAM_CC_IFE_LITE_CPHY_RX_CLK] = &cam_cc_ife_lite_cphy_rx_clk.clkr,
	[CAM_CC_IFE_LITE_CSID_CLK] = &cam_cc_ife_lite_csid_clk.clkr,
	[CAM_CC_IFE_LITE_CSID_CLK_SRC] = &cam_cc_ife_lite_csid_clk_src.clkr,
	[CAM_CC_IPE_0_AHB_CLK] = &cam_cc_ipe_0_ahb_clk.clkr,
	[CAM_CC_IPE_0_AREG_CLK] = &cam_cc_ipe_0_areg_clk.clkr,
	[CAM_CC_IPE_0_AXI_CLK] = &cam_cc_ipe_0_axi_clk.clkr,
	[CAM_CC_IPE_0_CLK] = &cam_cc_ipe_0_clk.clkr,
	[CAM_CC_IPE_0_CLK_SRC] = &cam_cc_ipe_0_clk_src.clkr,
	[CAM_CC_IPE_1_AHB_CLK] = &cam_cc_ipe_1_ahb_clk.clkr,
	[CAM_CC_IPE_1_AREG_CLK] = &cam_cc_ipe_1_areg_clk.clkr,
	[CAM_CC_IPE_1_AXI_CLK] = &cam_cc_ipe_1_axi_clk.clkr,
	[CAM_CC_IPE_1_CLK] = &cam_cc_ipe_1_clk.clkr,
	[CAM_CC_IPE_1_CLK_SRC] = &cam_cc_ipe_1_clk_src.clkr,
	[CAM_CC_JPEG_CLK] = &cam_cc_jpeg_clk.clkr,
	[CAM_CC_JPEG_CLK_SRC] = &cam_cc_jpeg_clk_src.clkr,
	[CAM_CC_LRME_CLK] = &cam_cc_lrme_clk.clkr,
	[CAM_CC_LRME_CLK_SRC] = &cam_cc_lrme_clk_src.clkr,
	[CAM_CC_MCLK0_CLK] = &cam_cc_mclk0_clk.clkr,
	[CAM_CC_MCLK0_CLK_SRC] = &cam_cc_mclk0_clk_src.clkr,
	[CAM_CC_MCLK1_CLK] = &cam_cc_mclk1_clk.clkr,
	[CAM_CC_MCLK1_CLK_SRC] = &cam_cc_mclk1_clk_src.clkr,
	[CAM_CC_MCLK2_CLK] = &cam_cc_mclk2_clk.clkr,
	[CAM_CC_MCLK2_CLK_SRC] = &cam_cc_mclk2_clk_src.clkr,
	[CAM_CC_MCLK3_CLK] = &cam_cc_mclk3_clk.clkr,
	[CAM_CC_MCLK3_CLK_SRC] = &cam_cc_mclk3_clk_src.clkr,
	[CAM_CC_PLL0] = &cam_cc_pll0.clkr,
	[CAM_CC_PLL0_OUT_EVEN] = &cam_cc_pll0_out_even.clkr,
	[CAM_CC_PLL1] = &cam_cc_pll1.clkr,
	[CAM_CC_PLL1_OUT_EVEN] = &cam_cc_pll1_out_even.clkr,
	[CAM_CC_PLL2] = &cam_cc_pll2.clkr,
	[CAM_CC_PLL2_OUT_EVEN] = &cam_cc_pll2_out_even.clkr,
	[CAM_CC_PLL2_OUT_ODD] = &cam_cc_pll2_out_odd.clkr,
	[CAM_CC_PLL3] = &cam_cc_pll3.clkr,
	[CAM_CC_PLL3_OUT_EVEN] = &cam_cc_pll3_out_even.clkr,
	[CAM_CC_PLL_TEST_CLK] = &cam_cc_pll_test_clk.clkr,
	[CAM_CC_SLOW_AHB_CLK_SRC] = &cam_cc_slow_ahb_clk_src.clkr,
	[CAM_CC_SOC_AHB_CLK] = &cam_cc_soc_ahb_clk.clkr,
	[CAM_CC_SYS_TMR_CLK] = &cam_cc_sys_tmr_clk.clkr,
};

static const struct qcom_reset_map cam_cc_sdm845_resets[] = {
	[TITAN_CAM_CC_CCI_BCR] = { 0xb0d4 },
	[TITAN_CAM_CC_CPAS_BCR] = { 0xb118 },
	[TITAN_CAM_CC_CSI0PHY_BCR] = { 0x5000 },
	[TITAN_CAM_CC_CSI1PHY_BCR] = { 0x5024 },
	[TITAN_CAM_CC_CSI2PHY_BCR] = { 0x5048 },
	[TITAN_CAM_CC_MCLK0_BCR] = { 0x4000 },
	[TITAN_CAM_CC_MCLK1_BCR] = { 0x4020 },
	[TITAN_CAM_CC_MCLK2_BCR] = { 0x4040 },
	[TITAN_CAM_CC_MCLK3_BCR] = { 0x4060 },
	[TITAN_CAM_CC_TITAN_TOP_BCR] = { 0xb130 },
};

static const struct regmap_config cam_cc_sdm845_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0xd004,
	.fast_io	= true,
};

static const struct qcom_cc_desc cam_cc_sdm845_desc = {
	.config = &cam_cc_sdm845_regmap_config,
	.clks = cam_cc_sdm845_clocks,
	.num_clks = ARRAY_SIZE(cam_cc_sdm845_clocks),
	.resets = cam_cc_sdm845_resets,
	.num_resets = ARRAY_SIZE(cam_cc_sdm845_resets),
};

static const struct of_device_id cam_cc_sdm845_match_table[] = {
	{ .compatible = "qcom,cam_cc-sdm845" },
	{ .compatible = "qcom,cam_cc-sdm845-v2" },
	{ .compatible = "qcom,cam_cc-sdm670" },
	{ }
};
MODULE_DEVICE_TABLE(of, cam_cc_sdm845_match_table);

static void cam_cc_sdm845_fixup_sdm845v2(void)
{
	cam_cc_sdm845_clocks[CAM_CC_CSI3PHYTIMER_CLK] =
		&cam_cc_csi3phytimer_clk.clkr;
	cam_cc_sdm845_clocks[CAM_CC_CSIPHY3_CLK] = &cam_cc_csiphy3_clk.clkr;
	cam_cc_sdm845_clocks[CAM_CC_CSI3PHYTIMER_CLK_SRC] =
		&cam_cc_csi3phytimer_clk_src.clkr;
	cam_cc_bps_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_bps_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_cci_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_cci_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_cphy_rx_clk_src.freq_tbl = ftbl_cam_cc_cphy_rx_clk_src_sdm845_v2;
	cam_cc_cphy_rx_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_cphy_rx_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_cphy_rx_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 384000000;
	cam_cc_csi0phytimer_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_csi0phytimer_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_csi1phytimer_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_csi1phytimer_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_csi2phytimer_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_csi2phytimer_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_fast_ahb_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_fast_ahb_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_fd_core_clk_src.freq_tbl = ftbl_cam_cc_fd_core_clk_src_sdm845_v2;
	cam_cc_fd_core_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_fd_core_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_icp_clk_src.freq_tbl = ftbl_cam_cc_icp_clk_src_sdm845_v2;
	cam_cc_icp_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_icp_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_icp_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW_L1] = 600000000;
	cam_cc_ife_0_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_ife_0_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_ife_0_csid_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_ife_0_csid_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_ife_0_csid_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] =
		384000000;
	cam_cc_ife_1_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_ife_1_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_ife_1_csid_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_ife_1_csid_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_ife_1_csid_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] =
		384000000;
	cam_cc_ife_lite_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_ife_lite_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_ife_lite_csid_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_ife_lite_csid_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_ife_lite_csid_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] =
		384000000;
	cam_cc_ipe_0_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_ipe_0_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_ipe_0_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] = 600000000;
	cam_cc_ipe_1_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_ipe_1_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_ipe_1_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] = 600000000;
	cam_cc_jpeg_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_jpeg_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_lrme_clk_src.freq_tbl = ftbl_cam_cc_lrme_clk_src_sdm845_v2;
	cam_cc_lrme_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_lrme_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_lrme_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 269333333;
	cam_cc_lrme_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW_L1] = 320000000;
	cam_cc_lrme_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] = 400000000;
	cam_cc_mclk0_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_mclk0_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_mclk0_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 34285714;
	cam_cc_mclk1_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_mclk1_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_mclk1_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 34285714;
	cam_cc_mclk2_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_mclk2_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_mclk2_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 34285714;
	cam_cc_mclk3_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_mclk3_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_mclk3_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 34285714;
	cam_cc_slow_ahb_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] = 0;
	cam_cc_slow_ahb_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 0;
	cam_cc_slow_ahb_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 80000000;
	cam_cc_slow_ahb_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW_L1] =
		80000000;
}

static void cam_cc_sdm845_fixup_sdm670(void)
{
	cam_cc_sdm845_clocks[CAM_CC_CSI3PHYTIMER_CLK] =
		&cam_cc_csi3phytimer_clk.clkr;
	cam_cc_sdm845_clocks[CAM_CC_CSIPHY3_CLK] = &cam_cc_csiphy3_clk.clkr;
	cam_cc_sdm845_clocks[CAM_CC_CSI3PHYTIMER_CLK_SRC] =
		&cam_cc_csi3phytimer_clk_src.clkr;
	cam_cc_cphy_rx_clk_src.freq_tbl = ftbl_cam_cc_cphy_rx_clk_src_sdm845_v2;
	cam_cc_cphy_rx_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 384000000;
	cam_cc_cphy_rx_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 384000000;
	cam_cc_fd_core_clk_src.freq_tbl = ftbl_cam_cc_fd_core_clk_src_sdm845_v2;
	cam_cc_fd_core_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 384000000;
	cam_cc_icp_clk_src.freq_tbl = ftbl_cam_cc_icp_clk_src_sdm845_v2;
	cam_cc_icp_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 384000000;
	cam_cc_icp_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW_L1] = 600000000;
	cam_cc_ipe_0_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] = 600000000;
	cam_cc_ipe_1_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] = 600000000;
	cam_cc_lrme_clk_src.freq_tbl = ftbl_cam_cc_lrme_clk_src_sdm845_v2;
	cam_cc_lrme_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 269333333;
	cam_cc_lrme_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW_L1] = 320000000;
	cam_cc_lrme_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] = 400000000;
	cam_cc_slow_ahb_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 80000000;
	cam_cc_slow_ahb_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 80000000;
	cam_cc_slow_ahb_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW_L1] =
		80000000;
}

static int cam_cc_sdm845_fixup(struct platform_device *pdev)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,cam_cc-sdm845-v2"))
		cam_cc_sdm845_fixup_sdm845v2();
	else if (!strcmp(compat, "qcom,cam_cc-sdm670"))
		cam_cc_sdm845_fixup_sdm670();

	return 0;
}

static int cam_cc_sdm845_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret = 0;

	regmap = qcom_cc_map(pdev, &cam_cc_sdm845_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the Camera CC registers\n");
		return PTR_ERR(regmap);
	}

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	vdd_mx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(vdd_mx.regulator[0])) {
		if (!(PTR_ERR(vdd_mx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_mx regulator\n");
		return PTR_ERR(vdd_mx.regulator[0]);
	}

	ret = cam_cc_sdm845_fixup(pdev);
	if (ret)
		return ret;

	clk_fabia_pll_configure(&cam_cc_pll0, regmap, &cam_cc_pll0_config);
	clk_fabia_pll_configure(&cam_cc_pll1, regmap, &cam_cc_pll1_config);
	clk_fabia_pll_configure(&cam_cc_pll2, regmap, &cam_cc_pll2_config);
	clk_fabia_pll_configure(&cam_cc_pll3, regmap, &cam_cc_pll3_config);

	ret = qcom_cc_really_probe(pdev, &cam_cc_sdm845_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register Camera CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Camera CC clocks\n");
	return ret;
}

static struct platform_driver cam_cc_sdm845_driver = {
	.probe		= cam_cc_sdm845_probe,
	.driver		= {
		.name	= "cam_cc-sdm845",
		.of_match_table = cam_cc_sdm845_match_table,
	},
};

static int __init cam_cc_sdm845_init(void)
{
	return platform_driver_register(&cam_cc_sdm845_driver);
}
subsys_initcall(cam_cc_sdm845_init);

static void __exit cam_cc_sdm845_exit(void)
{
	platform_driver_unregister(&cam_cc_sdm845_driver);
}
module_exit(cam_cc_sdm845_exit);

MODULE_DESCRIPTION("QTI CAM_CC SDM845 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cam_cc-sdm845");

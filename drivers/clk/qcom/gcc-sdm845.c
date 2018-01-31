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

#include <dt-bindings/clock/qcom,gcc-sdm845.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "reset.h"
#include "clk-alpha-pll.h"
#include "vdd-level-sdm845.h"

#define GCC_MMSS_MISC				0x09FFC
#define GCC_GPU_MISC				0x71028

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_CX_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_cx_ao, VDD_CX_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_AUD_REF_CLK,
	P_CORE_BI_PLL_TEST_SE,
	P_GPLL0_OUT_EVEN,
	P_GPLL0_OUT_MAIN,
	P_GPLL1_OUT_MAIN,
	P_GPLL4_OUT_MAIN,
	P_GPLL6_OUT_MAIN,
	P_SLEEP_CLK,
};

static const struct parent_map gcc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_0[] = {
	"bi_tcxo",
	"gpll0",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_SLEEP_CLK, 5 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_1[] = {
	"bi_tcxo",
	"gpll0",
	"core_pi_sleep_clk",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_SLEEP_CLK, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_2[] = {
	"bi_tcxo",
	"core_pi_sleep_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_3[] = {
	"bi_tcxo",
	"gpll0",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_4[] = {
	"bi_tcxo",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_5[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL4_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_5[] = {
	"bi_tcxo",
	"gpll0",
	"gpll4",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_6[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_AUD_REF_CLK, 2 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_6[] = {
	"bi_tcxo",
	"gpll0",
	"aud_ref_clk",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const char * const gcc_parent_names_7[] = {
	"bi_tcxo_ao",
	"gpll0",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const char * const gcc_parent_names_8[] = {
	"bi_tcxo_ao",
	"gpll0",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_9[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL1_OUT_MAIN, 4 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_9[] = {
	"bi_tcxo",
	"gpll0",
	"gpll1",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_10[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL4_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_10[] = {
	"bi_tcxo",
	"gpll0",
	"gpll4",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_7[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL6_OUT_MAIN, 2 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_11[] = {
	"bi_tcxo",
	"gpll0",
	"gpll6",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static struct clk_dummy measure_only_snoc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_snoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cnoc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_cnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_bimc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_bimc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ipa_2x_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_ipa_2x_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
	{ 125000000, 1000000000, 1 },
};

static struct clk_alpha_pll gpll0 = {
	.offset = 0x0,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.enable_reg = 0x52000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_fabia_fixed_pll_ops,
			VDD_CX_FMAX_MAP4(
				MIN, 615000000,
				LOW, 1066000000,
				LOW_L1, 1600000000,
				NOMINAL, 2000000000),
		},
	},
};

static struct clk_alpha_pll gpll4 = {
	.offset = 0x76000,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.enable_reg = 0x52000,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data){
			.name = "gpll4",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_fabia_fixed_pll_ops,
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

static struct clk_alpha_pll_postdiv gpll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_fabia_even,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpll0_out_even",
		.parent_names = (const char *[]){ "gpll0" },
		.num_parents = 1,
		.ops = &clk_generic_pll_postdiv_ops,
	},
};

static struct clk_alpha_pll gpll6 = {
	.offset = 0x13000,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.enable_reg = 0x52000,
		.enable_mask = BIT(6),
		.hw.init = &(struct clk_init_data){
			.name = "gpll6",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_fabia_fixed_pll_ops,
			VDD_CX_FMAX_MAP4(
				MIN, 615000000,
				LOW, 1066000000,
				LOW_L1, 1600000000,
				NOMINAL, 2000000000),
		},
	},
};

static const struct freq_tbl ftbl_gcc_cpuss_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_cpuss_ahb_clk_src = {
	.cmd_rcgr = 0x48014,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_cpuss_ahb_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_cpuss_ahb_clk_src",
		.parent_names = gcc_parent_names_7,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3_AO(
			MIN, 19200000,
			LOW, 50000000,
			NOMINAL, 100000000),
	},
};

static const struct freq_tbl ftbl_gcc_cpuss_rbcpr_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gcc_cpuss_rbcpr_clk_src_sdm670[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(50000000, P_GPLL0_OUT_MAIN, 12, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_cpuss_rbcpr_clk_src = {
	.cmd_rcgr = 0x4815c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_3,
	.freq_tbl = ftbl_gcc_cpuss_rbcpr_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_cpuss_rbcpr_clk_src",
		.parent_names = gcc_parent_names_8,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1_AO(
			MIN, 19200000),
	},
};

static const struct freq_tbl ftbl_gcc_gp1_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(25000000, P_GPLL0_OUT_EVEN, 12, 0, 0),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_gp1_clk_src = {
	.cmd_rcgr = 0x64004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_1,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_gp1_clk_src",
		.parent_names = gcc_parent_names_1,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static struct clk_rcg2 gcc_gp2_clk_src = {
	.cmd_rcgr = 0x65004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_1,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_gp2_clk_src",
		.parent_names = gcc_parent_names_1,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static struct clk_rcg2 gcc_gp3_clk_src = {
	.cmd_rcgr = 0x66004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_1,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_gp3_clk_src",
		.parent_names = gcc_parent_names_1,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static const struct freq_tbl ftbl_gcc_pcie_0_aux_clk_src[] = {
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_pcie_0_aux_clk_src = {
	.cmd_rcgr = 0x6b028,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_2,
	.freq_tbl = ftbl_gcc_pcie_0_aux_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_pcie_0_aux_clk_src",
		.parent_names = gcc_parent_names_2,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP2(
			MIN, 9600000,
			LOW, 19200000),
	},
};

static struct clk_rcg2 gcc_pcie_1_aux_clk_src = {
	.cmd_rcgr = 0x8d028,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_2,
	.freq_tbl = ftbl_gcc_pcie_0_aux_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_pcie_1_aux_clk_src",
		.parent_names = gcc_parent_names_2,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP2(
			MIN, 9600000,
			LOW, 19200000),
	},
};

static const struct freq_tbl ftbl_gcc_pcie_phy_refgen_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_pcie_phy_refgen_clk_src = {
	.cmd_rcgr = 0x6f014,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_pcie_phy_refgen_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_pcie_phy_refgen_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP2(
			MIN, 19200000,
			LOW, 100000000),
	},
};

static const struct freq_tbl ftbl_gcc_pdm2_clk_src[] = {
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(60000000, P_GPLL0_OUT_MAIN, 10, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_pdm2_clk_src = {
	.cmd_rcgr = 0x33010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_pdm2_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_pdm2_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 60000000),
	},
};

static const struct freq_tbl ftbl_gcc_qupv3_wrap0_s0_clk_src[] = {
	F(7372800, P_GPLL0_OUT_EVEN, 1, 384, 15625),
	F(14745600, P_GPLL0_OUT_EVEN, 1, 768, 15625),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(29491200, P_GPLL0_OUT_EVEN, 1, 1536, 15625),
	F(32000000, P_GPLL0_OUT_EVEN, 1, 8, 75),
	F(38400000, P_GPLL0_OUT_EVEN, 1, 16, 125),
	F(48000000, P_GPLL0_OUT_EVEN, 1, 4, 25),
	F(64000000, P_GPLL0_OUT_EVEN, 1, 16, 75),
	F(80000000, P_GPLL0_OUT_EVEN, 1, 4, 15),
	F(96000000, P_GPLL0_OUT_EVEN, 1, 8, 25),
	F(100000000, P_GPLL0_OUT_EVEN, 3, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2[] = {
	F(7372800, P_GPLL0_OUT_EVEN, 1, 384, 15625),
	F(14745600, P_GPLL0_OUT_EVEN, 1, 768, 15625),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(29491200, P_GPLL0_OUT_EVEN, 1, 1536, 15625),
	F(32000000, P_GPLL0_OUT_EVEN, 1, 8, 75),
	F(48000000, P_GPLL0_OUT_EVEN, 1, 4, 25),
	F(64000000, P_GPLL0_OUT_EVEN, 1, 16, 75),
	F(80000000, P_GPLL0_OUT_EVEN, 1, 4, 15),
	F(96000000, P_GPLL0_OUT_EVEN, 1, 8, 25),
	F(100000000, P_GPLL0_OUT_EVEN, 3, 0, 0),
	F(102400000, P_GPLL0_OUT_EVEN, 1, 128, 375),
	F(112000000, P_GPLL0_OUT_EVEN, 1, 28, 75),
	F(117964800, P_GPLL0_OUT_EVEN, 1, 6144, 15625),
	F(120000000, P_GPLL0_OUT_EVEN, 2.5, 0, 0),
	F(128000000, P_GPLL0_OUT_MAIN, 1, 16, 75),
	{ }
};

static struct clk_rcg2 gcc_qupv3_wrap0_s0_clk_src = {
	.cmd_rcgr = 0x17034,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap0_s0_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap0_s1_clk_src = {
	.cmd_rcgr = 0x17164,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap0_s1_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap0_s2_clk_src = {
	.cmd_rcgr = 0x17294,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap0_s2_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap0_s3_clk_src = {
	.cmd_rcgr = 0x173c4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap0_s3_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap0_s4_clk_src = {
	.cmd_rcgr = 0x174f4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap0_s4_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap0_s5_clk_src = {
	.cmd_rcgr = 0x17624,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap0_s5_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap0_s6_clk_src = {
	.cmd_rcgr = 0x17754,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap0_s6_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap0_s7_clk_src = {
	.cmd_rcgr = 0x17884,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap0_s7_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap1_s0_clk_src = {
	.cmd_rcgr = 0x18018,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap1_s0_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap1_s1_clk_src = {
	.cmd_rcgr = 0x18148,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap1_s1_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap1_s2_clk_src = {
	.cmd_rcgr = 0x18278,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap1_s2_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap1_s3_clk_src = {
	.cmd_rcgr = 0x183a8,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap1_s3_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap1_s4_clk_src = {
	.cmd_rcgr = 0x184d8,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap1_s4_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap1_s5_clk_src = {
	.cmd_rcgr = 0x18608,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap1_s5_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap1_s6_clk_src = {
	.cmd_rcgr = 0x18738,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap1_s6_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static struct clk_rcg2 gcc_qupv3_wrap1_s7_clk_src = {
	.cmd_rcgr = 0x18868,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_qupv3_wrap0_s0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_qupv3_wrap1_s7_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 75000000,
			LOW, 100000000),
	},
};

static const struct freq_tbl ftbl_gcc_sdcc1_ice_core_clk_src[] = {
	F(75000000, P_GPLL0_OUT_EVEN, 4, 0, 0),
	F(150000000, P_GPLL0_OUT_MAIN, 4, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_sdcc1_ice_core_clk_src = {
	.cmd_rcgr = 0x26010,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_sdcc1_ice_core_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_sdcc1_ice_core_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 75000000,
			LOW, 150000000,
			NOMINAL, 300000000),
	},
};

static const struct freq_tbl ftbl_gcc_sdcc1_apps_clk_src[] = {
	F(144000, P_BI_TCXO, 16, 3, 25),
	F(400000, P_BI_TCXO, 12, 1, 4),
	F(20000000, P_GPLL0_OUT_EVEN, 5, 1, 3),
	F(25000000, P_GPLL0_OUT_EVEN, 6, 1, 2),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	F(192000000, P_GPLL6_OUT_MAIN, 2, 0, 0),
	F(384000000, P_GPLL6_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_sdcc1_apps_clk_src = {
	.cmd_rcgr = 0x26028,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_7,
	.freq_tbl = ftbl_gcc_sdcc1_apps_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_sdcc1_apps_clk_src",
		.parent_names = gcc_parent_names_11,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 384000000),
	},
};

static const struct freq_tbl ftbl_gcc_sdcc2_apps_clk_src[] = {
	F(400000, P_BI_TCXO, 12, 1, 4),
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(25000000, P_GPLL0_OUT_EVEN, 12, 0, 0),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	F(201500000, P_GPLL4_OUT_MAIN, 4, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_sdcc2_apps_clk_src = {
	.cmd_rcgr = 0x1400c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_10,
	.freq_tbl = ftbl_gcc_sdcc2_apps_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_sdcc2_apps_clk_src",
		.parent_names = gcc_parent_names_10,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 100000000,
			LOW_L1, 201500000),
	},
};

static const struct freq_tbl ftbl_gcc_sdcc4_apps_clk_src[] = {
	F(400000, P_BI_TCXO, 12, 1, 4),
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(25000000, P_GPLL0_OUT_MAIN, 12, 1, 2),
	F(50000000, P_GPLL0_OUT_MAIN, 12, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gcc_sdcc4_apps_clk_src_sdm670[] = {
	F(400000, P_BI_TCXO, 12, 1, 4),
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(25000000, P_GPLL0_OUT_EVEN, 12, 0, 0),
	F(33333333, P_GPLL0_OUT_EVEN, 9, 0, 0),
	F(50000000, P_GPLL0_OUT_MAIN, 12, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_sdcc4_apps_clk_src = {
	.cmd_rcgr = 0x1600c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_sdcc4_apps_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_sdcc4_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 50000000,
			NOMINAL, 100000000),
	},
};

static const struct freq_tbl ftbl_gcc_tsif_ref_clk_src[] = {
	F(105495, P_BI_TCXO, 2, 1, 91),
	{ }
};

static struct clk_rcg2 gcc_tsif_ref_clk_src = {
	.cmd_rcgr = 0x36010,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_6,
	.freq_tbl = ftbl_gcc_tsif_ref_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_tsif_ref_clk_src",
		.parent_names = gcc_parent_names_6,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1(
			MIN, 105495),
	},
};

static const struct freq_tbl ftbl_gcc_ufs_card_axi_clk_src[] = {
	F(25000000, P_GPLL0_OUT_EVEN, 12, 0, 0),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gcc_ufs_card_axi_clk_src_sdm845_v2[] = {
	F(25000000, P_GPLL0_OUT_EVEN, 12, 0, 0),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(240000000, P_GPLL0_OUT_MAIN, 2.5, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_ufs_card_axi_clk_src = {
	.cmd_rcgr = 0x7501c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_ufs_card_axi_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_ufs_card_axi_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static const struct freq_tbl ftbl_gcc_ufs_card_ice_core_clk_src[] = {
	F(37500000, P_GPLL0_OUT_EVEN, 8, 0, 0),
	F(75000000, P_GPLL0_OUT_EVEN, 4, 0, 0),
	F(150000000, P_GPLL0_OUT_MAIN, 4, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_ufs_card_ice_core_clk_src = {
	.cmd_rcgr = 0x7505c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_ufs_card_ice_core_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_ufs_card_ice_core_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 75000000,
			LOW, 150000000,
			NOMINAL, 300000000),
	},
};

static struct clk_rcg2 gcc_ufs_card_phy_aux_clk_src = {
	.cmd_rcgr = 0x75090,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_4,
	.freq_tbl = ftbl_gcc_cpuss_rbcpr_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_ufs_card_phy_aux_clk_src",
		.parent_names = gcc_parent_names_4,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1(
			MIN, 19200000),
	},
};

static const struct freq_tbl ftbl_gcc_ufs_card_unipro_core_clk_src[] = {
	F(37500000, P_GPLL0_OUT_EVEN, 8, 0, 0),
	F(75000000, P_GPLL0_OUT_MAIN, 8, 0, 0),
	F(150000000, P_GPLL0_OUT_MAIN, 4, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_ufs_card_unipro_core_clk_src = {
	.cmd_rcgr = 0x75074,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_ufs_card_unipro_core_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_ufs_card_unipro_core_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 37500000,
			LOW, 75000000,
			NOMINAL, 150000000),
	},
};

static const struct freq_tbl ftbl_gcc_ufs_phy_axi_clk_src[] = {
	F(25000000, P_GPLL0_OUT_EVEN, 12, 0, 0),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(240000000, P_GPLL0_OUT_MAIN, 2.5, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_ufs_phy_axi_clk_src = {
	.cmd_rcgr = 0x7701c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_ufs_phy_axi_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_ufs_phy_axi_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000,
			HIGH, 240000000),
	},
};

static struct clk_rcg2 gcc_ufs_phy_ice_core_clk_src = {
	.cmd_rcgr = 0x7705c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_ufs_card_ice_core_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_ufs_phy_ice_core_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 75000000,
			LOW, 150000000,
			NOMINAL, 300000000),
	},
};

static struct clk_rcg2 gcc_ufs_phy_phy_aux_clk_src = {
	.cmd_rcgr = 0x77090,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_4,
	.freq_tbl = ftbl_gcc_pcie_0_aux_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_ufs_phy_phy_aux_clk_src",
		.parent_names = gcc_parent_names_4,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1(
			MIN, 19200000),
	},
};

static struct clk_rcg2 gcc_ufs_phy_unipro_core_clk_src = {
	.cmd_rcgr = 0x77074,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_ufs_card_unipro_core_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_ufs_phy_unipro_core_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 37500000,
			LOW, 75000000,
			NOMINAL, 150000000),
	},
};

static const struct freq_tbl ftbl_gcc_usb30_prim_master_clk_src[] = {
	F(33333333, P_GPLL0_OUT_EVEN, 9, 0, 0),
	F(66666667, P_GPLL0_OUT_EVEN, 4.5, 0, 0),
	F(133333333, P_GPLL0_OUT_MAIN, 4.5, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(240000000, P_GPLL0_OUT_MAIN, 2.5, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_usb30_prim_master_clk_src = {
	.cmd_rcgr = 0xf018,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_usb30_prim_master_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb30_prim_master_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 33333333,
			LOWER, 66666667,
			LOW, 133333333,
			NOMINAL, 200000000,
			HIGH, 240000000),
	},
};

static const struct freq_tbl ftbl_gcc_usb30_prim_mock_utmi_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(20000000, P_GPLL0_OUT_EVEN, 15, 0, 0),
	F(40000000, P_GPLL0_OUT_EVEN, 7.5, 0, 0),
	F(60000000, P_GPLL0_OUT_MAIN, 10, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_usb30_prim_mock_utmi_clk_src = {
	.cmd_rcgr = 0xf030,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_usb30_prim_mock_utmi_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb30_prim_mock_utmi_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 40000000,
			LOW, 60000000),
	},
};

static struct clk_rcg2 gcc_usb30_sec_master_clk_src = {
	.cmd_rcgr = 0x10018,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_usb30_prim_master_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb30_sec_master_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 33333333,
			LOWER, 66666667,
			LOW, 133333333,
			NOMINAL, 200000000,
			HIGH, 240000000),
	},
};

static struct clk_rcg2 gcc_usb30_sec_mock_utmi_clk_src = {
	.cmd_rcgr = 0x10030,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_usb30_prim_mock_utmi_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb30_sec_mock_utmi_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 40000000,
			LOW, 60000000),
	},
};

static struct clk_rcg2 gcc_usb3_prim_phy_aux_clk_src = {
	.cmd_rcgr = 0xf05c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_2,
	.freq_tbl = ftbl_gcc_cpuss_rbcpr_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb3_prim_phy_aux_clk_src",
		.parent_names = gcc_parent_names_2,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1(
			MIN, 19200000),
	},
};

static struct clk_rcg2 gcc_usb3_sec_phy_aux_clk_src = {
	.cmd_rcgr = 0x1005c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_2,
	.freq_tbl = ftbl_gcc_cpuss_rbcpr_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb3_sec_phy_aux_clk_src",
		.parent_names = gcc_parent_names_2,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1(
			MIN, 19200000),
	},
};

static struct clk_rcg2 gcc_vs_ctrl_clk_src = {
	.cmd_rcgr = 0x7a030,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_3,
	.freq_tbl = ftbl_gcc_cpuss_rbcpr_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_vs_ctrl_clk_src",
		.parent_names = gcc_parent_names_3,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1(
			MIN, 19200000),
	},
};

static const struct freq_tbl ftbl_gcc_vsensor_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	F(600000000, P_GPLL0_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_vsensor_clk_src = {
	.cmd_rcgr = 0x7a018,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_9,
	.freq_tbl = ftbl_gcc_vsensor_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_vsensor_clk_src",
		.parent_names = gcc_parent_names_9,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOW, 300000000,
			LOW_L1, 600000000),
	},
};

static struct clk_branch gcc_aggre_noc_pcie_tbu_clk = {
	.halt_reg = 0x90014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x90014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_aggre_noc_pcie_tbu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_aggre_ufs_card_axi_clk = {
	.halt_reg = 0x82028,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x82028,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x82028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_aggre_ufs_card_axi_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_card_axi_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_aggre_ufs_card_axi_hw_ctl_clk = {
	.halt_reg = 0x82028,
	.clkr = {
		.enable_reg = 0x82028,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_aggre_ufs_card_axi_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_aggre_ufs_card_axi_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_branch gcc_aggre_ufs_phy_axi_clk = {
	.halt_reg = 0x82024,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x82024,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x82024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_aggre_ufs_phy_axi_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_phy_axi_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_aggre_ufs_phy_axi_hw_ctl_clk = {
	.halt_reg = 0x82024,
	.clkr = {
		.enable_reg = 0x82024,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_aggre_ufs_phy_axi_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_aggre_ufs_phy_axi_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_branch gcc_aggre_usb3_prim_axi_clk = {
	.halt_reg = 0x8201c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8201c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_aggre_usb3_prim_axi_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_prim_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_aggre_usb3_sec_axi_clk = {
	.halt_reg = 0x82020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x82020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_aggre_usb3_sec_axi_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_sec_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_apc_vs_clk = {
	.halt_reg = 0x7a050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7a050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_apc_vs_clk",
			.parent_names = (const char *[]){
				"gcc_vsensor_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_boot_rom_ahb_clk = {
	.halt_reg = 0x38004,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x38004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(10),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_boot_rom_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camera_ahb_clk = {
	.halt_reg = 0xb008,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0xb008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0xb008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_camera_ahb_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camera_axi_clk = {
	.halt_reg = 0xb020,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0xb020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_camera_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camera_xo_clk = {
	.halt_reg = 0xb02c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb02c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_camera_xo_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce1_ahb_clk = {
	.halt_reg = 0x4100c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x4100c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(3),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ce1_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce1_axi_clk = {
	.halt_reg = 0x41008,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ce1_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce1_clk = {
	.halt_reg = 0x41004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(5),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ce1_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_cfg_noc_usb3_prim_axi_clk = {
	.halt_reg = 0x502c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x502c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_cfg_noc_usb3_prim_axi_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_prim_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_cfg_noc_usb3_sec_axi_clk = {
	.halt_reg = 0x5030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x5030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_cfg_noc_usb3_sec_axi_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_sec_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_cpuss_ahb_clk = {
	.halt_reg = 0x48000,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(21),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_cpuss_ahb_clk",
			.parent_names = (const char *[]){
				"gcc_cpuss_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_cpuss_dvm_bus_clk = {
	.halt_reg = 0x48190,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x48190,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_cpuss_dvm_bus_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_cpuss_gnoc_clk = {
	.halt_reg = 0x48004,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x48004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(22),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_cpuss_gnoc_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_cpuss_rbcpr_clk = {
	.halt_reg = 0x48008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x48008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_cpuss_rbcpr_clk",
			.parent_names = (const char *[]){
				"gcc_cpuss_rbcpr_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ddrss_gpu_axi_clk = {
	.halt_reg = 0x44038,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x44038,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ddrss_gpu_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_disp_ahb_clk = {
	.halt_reg = 0xb00c,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0xb00c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0xb00c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_disp_ahb_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_disp_axi_clk = {
	.halt_reg = 0xb024,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0xb024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_disp_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_gate2 gcc_disp_gpll0_clk_src = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(18),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_disp_gpll0_clk_src",
			.parent_names = (const char *[]){
				"gpll0",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_gate2 gcc_disp_gpll0_div_clk_src = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(19),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_disp_gpll0_div_clk_src",
			.parent_names = (const char *[]){
				"gpll0_out_even",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_branch gcc_disp_xo_clk = {
	.halt_reg = 0xb030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_disp_xo_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp1_clk = {
	.halt_reg = 0x64000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x64000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gp1_clk",
			.parent_names = (const char *[]){
				"gcc_gp1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp2_clk = {
	.halt_reg = 0x65000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x65000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gp2_clk",
			.parent_names = (const char *[]){
				"gcc_gp2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp3_clk = {
	.halt_reg = 0x66000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x66000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gp3_clk",
			.parent_names = (const char *[]){
				"gcc_gp3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gpu_cfg_ahb_clk = {
	.halt_reg = 0x71004,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x71004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x71004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gpu_cfg_ahb_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_gate2 gcc_gpu_gpll0_clk_src = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(15),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gpu_gpll0_clk_src",
			.parent_names = (const char *[]){
				"gpll0",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_gate2 gcc_gpu_gpll0_div_clk_src = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(16),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gpu_gpll0_div_clk_src",
			.parent_names = (const char *[]){
				"gpll0_out_even",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_branch gcc_gpu_iref_clk = {
	.halt_reg = 0x8c010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8c010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gpu_iref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gpu_memnoc_gfx_clk = {
	.halt_reg = 0x7100c,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x7100c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gpu_memnoc_gfx_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gpu_snoc_dvm_gfx_clk = {
	.halt_reg = 0x71018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x71018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gpu_snoc_dvm_gfx_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gpu_vs_clk = {
	.halt_reg = 0x7a04c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7a04c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_gpu_vs_clk",
			.parent_names = (const char *[]){
				"gcc_vsensor_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_mss_axis2_clk = {
	.halt_reg = 0x8a008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8a008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_mss_axis2_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_mss_cfg_ahb_clk = {
	.halt_reg = 0x8a000,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x8a000,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x8a000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_mss_cfg_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_gate2 gcc_mss_gpll0_div_clk_src = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(17),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_mss_gpll0_div_clk_src",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_branch gcc_mss_mfab_axis_clk = {
	.halt_reg = 0x8a004,
	.halt_check = BRANCH_VOTED,
	.hwcg_reg = 0x8a004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x8a004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_mss_mfab_axis_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_mss_q6_memnoc_axi_clk = {
	.halt_reg = 0x8a154,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x8a154,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_mss_q6_memnoc_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_mss_snoc_axi_clk = {
	.halt_reg = 0x8a150,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8a150,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_mss_snoc_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_mss_vs_clk = {
	.halt_reg = 0x7a048,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7a048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_mss_vs_clk",
			.parent_names = (const char *[]){
				"gcc_vsensor_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_0_aux_clk = {
	.halt_reg = 0x6b01c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(3),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_0_aux_clk",
			.parent_names = (const char *[]){
				"gcc_pcie_0_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_0_cfg_ahb_clk = {
	.halt_reg = 0x6b018,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x6b018,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(2),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_0_cfg_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_0_clkref_clk = {
	.halt_reg = 0x8c00c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8c00c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_0_clkref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_0_mstr_axi_clk = {
	.halt_reg = 0x6b014,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_0_mstr_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_0_pipe_clk = {
	.halt_reg = 0x6b020,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_0_pipe_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_0_slv_axi_clk = {
	.halt_reg = 0x6b010,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x6b010,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_0_slv_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_0_slv_q2a_axi_clk = {
	.halt_reg = 0x6b00c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(5),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_0_slv_q2a_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_1_aux_clk = {
	.halt_reg = 0x8d01c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(29),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_1_aux_clk",
			.parent_names = (const char *[]){
				"gcc_pcie_1_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_1_cfg_ahb_clk = {
	.halt_reg = 0x8d018,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x8d018,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(28),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_1_cfg_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_1_clkref_clk = {
	.halt_reg = 0x8c02c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8c02c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_1_clkref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_1_mstr_axi_clk = {
	.halt_reg = 0x8d014,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(27),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_1_mstr_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_1_pipe_clk = {
	.halt_reg = 0x8d020,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(30),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_1_pipe_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_1_slv_axi_clk = {
	.halt_reg = 0x8d010,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x8d010,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(26),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_1_slv_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_1_slv_q2a_axi_clk = {
	.halt_reg = 0x8d00c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(25),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_1_slv_q2a_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_phy_aux_clk = {
	.halt_reg = 0x6f004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x6f004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_phy_aux_clk",
			.parent_names = (const char *[]){
				"gcc_pcie_0_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_phy_refgen_clk = {
	.halt_reg = 0x6f02c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x6f02c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_phy_refgen_clk",
			.parent_names = (const char *[]){
				"gcc_pcie_phy_refgen_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pdm2_clk = {
	.halt_reg = 0x3300c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3300c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pdm2_clk",
			.parent_names = (const char *[]){
				"gcc_pdm2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pdm_ahb_clk = {
	.halt_reg = 0x33004,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x33004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x33004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pdm_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pdm_xo4_clk = {
	.halt_reg = 0x33008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x33008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pdm_xo4_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_prng_ahb_clk = {
	.halt_reg = 0x34004,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x34004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(13),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_prng_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qmip_camera_ahb_clk = {
	.halt_reg = 0xb014,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0xb014,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0xb014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qmip_camera_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qmip_disp_ahb_clk = {
	.halt_reg = 0xb018,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0xb018,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0xb018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qmip_disp_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qmip_video_ahb_clk = {
	.halt_reg = 0xb010,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0xb010,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0xb010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qmip_video_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap0_s0_clk = {
	.halt_reg = 0x17030,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(10),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap0_s0_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap0_s0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap0_s1_clk = {
	.halt_reg = 0x17160,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(11),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap0_s1_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap0_s1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap0_s2_clk = {
	.halt_reg = 0x17290,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(12),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap0_s2_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap0_s2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap0_s3_clk = {
	.halt_reg = 0x173c0,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(13),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap0_s3_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap0_s3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap0_s4_clk = {
	.halt_reg = 0x174f0,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(14),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap0_s4_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap0_s4_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap0_s5_clk = {
	.halt_reg = 0x17620,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(15),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap0_s5_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap0_s5_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap0_s6_clk = {
	.halt_reg = 0x17750,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(16),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap0_s6_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap0_s6_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap0_s7_clk = {
	.halt_reg = 0x17880,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(17),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap0_s7_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap0_s7_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap1_s0_clk = {
	.halt_reg = 0x18014,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(22),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap1_s0_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap1_s0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap1_s1_clk = {
	.halt_reg = 0x18144,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(23),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap1_s1_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap1_s1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap1_s2_clk = {
	.halt_reg = 0x18274,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(24),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap1_s2_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap1_s2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap1_s3_clk = {
	.halt_reg = 0x183a4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(25),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap1_s3_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap1_s3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap1_s4_clk = {
	.halt_reg = 0x184d4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(26),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap1_s4_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap1_s4_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap1_s5_clk = {
	.halt_reg = 0x18604,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(27),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap1_s5_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap1_s5_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap1_s6_clk = {
	.halt_reg = 0x18734,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(28),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap1_s6_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap1_s6_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap1_s7_clk = {
	.halt_reg = 0x18864,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(29),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap1_s7_clk",
			.parent_names = (const char *[]){
				"gcc_qupv3_wrap1_s7_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap_0_m_ahb_clk = {
	.halt_reg = 0x17004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(6),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap_0_m_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap_0_s_ahb_clk = {
	.halt_reg = 0x17008,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x17008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(7),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap_0_s_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap_1_m_ahb_clk = {
	.halt_reg = 0x1800c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(20),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap_1_m_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_qupv3_wrap_1_s_ahb_clk = {
	.halt_reg = 0x18010,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x18010,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x5200c,
		.enable_mask = BIT(21),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_qupv3_wrap_1_s_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc1_ice_core_clk = {
	.halt_reg = 0x2600c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2600c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sdcc1_ice_core_clk",
			.parent_names = (const char *[]){
				"gcc_sdcc1_ice_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc1_ahb_clk = {
	.halt_reg = 0x26008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x26008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sdcc1_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc1_apps_clk = {
	.halt_reg = 0x26004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x26004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sdcc1_apps_clk",
			.parent_names = (const char *[]){
				"gcc_sdcc1_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc2_ahb_clk = {
	.halt_reg = 0x14008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x14008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sdcc2_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc2_apps_clk = {
	.halt_reg = 0x14004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x14004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sdcc2_apps_clk",
			.parent_names = (const char *[]){
				"gcc_sdcc2_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc4_ahb_clk = {
	.halt_reg = 0x16008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x16008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sdcc4_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc4_apps_clk = {
	.halt_reg = 0x16004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x16004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sdcc4_apps_clk",
			.parent_names = (const char *[]){
				"gcc_sdcc4_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sys_noc_cpuss_ahb_clk = {
	.halt_reg = 0x414c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x52004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sys_noc_cpuss_ahb_clk",
			.parent_names = (const char *[]){
				"gcc_cpuss_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_tsif_ahb_clk = {
	.halt_reg = 0x36004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_tsif_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_tsif_inactivity_timers_clk = {
	.halt_reg = 0x3600c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3600c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_tsif_inactivity_timers_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_tsif_ref_clk = {
	.halt_reg = 0x36008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x36008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_tsif_ref_clk",
			.parent_names = (const char *[]){
				"gcc_tsif_ref_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_ahb_clk = {
	.halt_reg = 0x75010,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x75010,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x75010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_axi_clk = {
	.halt_reg = 0x7500c,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x7500c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x7500c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_axi_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_card_axi_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_axi_hw_ctl_clk = {
	.halt_reg = 0x7500c,
	.clkr = {
		.enable_reg = 0x7500c,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_axi_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_card_axi_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_clkref_clk = {
	.halt_reg = 0x8c004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8c004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_clkref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_ice_core_clk = {
	.halt_reg = 0x75058,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x75058,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x75058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_ice_core_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_card_ice_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_ice_core_hw_ctl_clk = {
	.halt_reg = 0x75058,
	.clkr = {
		.enable_reg = 0x75058,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_ice_core_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_card_ice_core_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_phy_aux_clk = {
	.halt_reg = 0x7508c,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x7508c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x7508c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_phy_aux_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_card_phy_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_phy_aux_hw_ctl_clk = {
	.halt_reg = 0x7508c,
	.clkr = {
		.enable_reg = 0x7508c,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_phy_aux_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_card_phy_aux_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_gate2 gcc_ufs_card_rx_symbol_0_clk = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x75018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_rx_symbol_0_clk",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_gate2 gcc_ufs_card_rx_symbol_1_clk = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x750a8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_rx_symbol_1_clk",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_gate2 gcc_ufs_card_tx_symbol_0_clk = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x75014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_tx_symbol_0_clk",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_unipro_core_clk = {
	.halt_reg = 0x75054,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x75054,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x75054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_unipro_core_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_card_unipro_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_card_unipro_core_hw_ctl_clk = {
	.halt_reg = 0x75054,
	.clkr = {
		.enable_reg = 0x75054,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_card_unipro_core_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_card_unipro_core_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_branch gcc_ufs_mem_clkref_clk = {
	.halt_reg = 0x8c000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8c000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_mem_clkref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_phy_ahb_clk = {
	.halt_reg = 0x77010,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x77010,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x77010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_phy_axi_clk = {
	.halt_reg = 0x7700c,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x7700c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x7700c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_axi_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_phy_axi_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_phy_axi_hw_ctl_clk = {
	.halt_reg = 0x7700c,
	.clkr = {
		.enable_reg = 0x7700c,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_axi_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_phy_axi_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_branch gcc_ufs_phy_ice_core_clk = {
	.halt_reg = 0x77058,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x77058,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x77058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_ice_core_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_phy_ice_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_phy_ice_core_hw_ctl_clk = {
	.halt_reg = 0x77058,
	.clkr = {
		.enable_reg = 0x77058,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_ice_core_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_phy_ice_core_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_branch gcc_ufs_phy_phy_aux_clk = {
	.halt_reg = 0x7708c,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x7708c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x7708c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_phy_aux_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_phy_phy_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_phy_phy_aux_hw_ctl_clk = {
	.halt_reg = 0x7708c,
	.clkr = {
		.enable_reg = 0x7708c,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_phy_aux_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_phy_phy_aux_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_gate2 gcc_ufs_phy_rx_symbol_0_clk = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x77018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_rx_symbol_0_clk",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_gate2 gcc_ufs_phy_rx_symbol_1_clk = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x770a8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_rx_symbol_1_clk",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_gate2 gcc_ufs_phy_tx_symbol_0_clk = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x77014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_tx_symbol_0_clk",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_phy_unipro_core_clk = {
	.halt_reg = 0x77054,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x77054,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x77054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_unipro_core_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_phy_unipro_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ufs_phy_unipro_core_hw_ctl_clk = {
	.halt_reg = 0x77054,
	.clkr = {
		.enable_reg = 0x77054,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ufs_phy_unipro_core_hw_ctl_clk",
			.parent_names = (const char *[]){
				"gcc_ufs_phy_unipro_core_clk",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_hw_ctl_ops,
		},
	},
};

static struct clk_branch gcc_usb30_prim_master_clk = {
	.halt_reg = 0xf00c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xf00c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_prim_master_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_prim_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_prim_mock_utmi_clk = {
	.halt_reg = 0xf014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xf014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_prim_mock_utmi_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_prim_mock_utmi_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_prim_sleep_clk = {
	.halt_reg = 0xf010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xf010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_prim_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_sec_master_clk = {
	.halt_reg = 0x1000c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1000c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_sec_master_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_sec_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_sec_mock_utmi_clk = {
	.halt_reg = 0x10014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_sec_mock_utmi_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_sec_mock_utmi_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_sec_sleep_clk = {
	.halt_reg = 0x10010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_sec_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb3_prim_clkref_clk = {
	.halt_reg = 0x8c008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8c008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_prim_clkref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb3_prim_phy_aux_clk = {
	.halt_reg = 0xf04c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xf04c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_prim_phy_aux_clk",
			.parent_names = (const char *[]){
				"gcc_usb3_prim_phy_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb3_prim_phy_com_aux_clk = {
	.halt_reg = 0xf050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xf050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_prim_phy_com_aux_clk",
			.parent_names = (const char *[]){
				"gcc_usb3_prim_phy_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_gate2 gcc_usb3_prim_phy_pipe_clk = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0xf054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_prim_phy_pipe_clk",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_branch gcc_usb3_sec_clkref_clk = {
	.halt_reg = 0x8c028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8c028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_sec_clkref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb3_sec_phy_aux_clk = {
	.halt_reg = 0x1004c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1004c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_sec_phy_aux_clk",
			.parent_names = (const char *[]){
				"gcc_usb3_sec_phy_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb3_sec_phy_com_aux_clk = {
	.halt_reg = 0x10050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_sec_phy_com_aux_clk",
			.parent_names = (const char *[]){
				"gcc_usb3_sec_phy_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_gate2 gcc_usb3_sec_phy_pipe_clk = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0x10054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_sec_phy_pipe_clk",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_branch gcc_usb_phy_cfg_ahb2phy_clk = {
	.halt_reg = 0x6a004,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x6a004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x6a004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb_phy_cfg_ahb2phy_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_vdda_vs_clk = {
	.halt_reg = 0x7a00c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7a00c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_vdda_vs_clk",
			.parent_names = (const char *[]){
				"gcc_vsensor_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_vddcx_vs_clk = {
	.halt_reg = 0x7a004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7a004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_vddcx_vs_clk",
			.parent_names = (const char *[]){
				"gcc_vsensor_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_vddmx_vs_clk = {
	.halt_reg = 0x7a008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7a008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_vddmx_vs_clk",
			.parent_names = (const char *[]){
				"gcc_vsensor_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_video_ahb_clk = {
	.halt_reg = 0xb004,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0xb004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0xb004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_video_ahb_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_video_axi_clk = {
	.halt_reg = 0xb01c,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0xb01c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_video_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_video_xo_clk = {
	.halt_reg = 0xb028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_video_xo_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_vs_ctrl_ahb_clk = {
	.halt_reg = 0x7a014,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x7a014,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x7a014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_vs_ctrl_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_vs_ctrl_clk = {
	.halt_reg = 0x7a010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7a010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_vs_ctrl_clk",
			.parent_names = (const char *[]){
				"gcc_vs_ctrl_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

struct clk_hw *gcc_sdm845_hws[] = {
	[MEASURE_ONLY_SNOC_CLK] = &measure_only_snoc_clk.hw,
	[MEASURE_ONLY_CNOC_CLK] = &measure_only_cnoc_clk.hw,
	[MEASURE_ONLY_BIMC_CLK] = &measure_only_bimc_clk.hw,
	[MEASURE_ONLY_IPA_2X_CLK] = &measure_only_ipa_2x_clk.hw,
};

static struct clk_regmap *gcc_sdm845_clocks[] = {
	[GCC_AGGRE_NOC_PCIE_TBU_CLK] = &gcc_aggre_noc_pcie_tbu_clk.clkr,
	[GCC_AGGRE_UFS_CARD_AXI_CLK] = &gcc_aggre_ufs_card_axi_clk.clkr,
	[GCC_AGGRE_UFS_CARD_AXI_HW_CTL_CLK] =
				&gcc_aggre_ufs_card_axi_hw_ctl_clk.clkr,
	[GCC_AGGRE_UFS_PHY_AXI_CLK] = &gcc_aggre_ufs_phy_axi_clk.clkr,
	[GCC_AGGRE_UFS_PHY_AXI_HW_CTL_CLK] =
				&gcc_aggre_ufs_phy_axi_hw_ctl_clk.clkr,
	[GCC_AGGRE_USB3_PRIM_AXI_CLK] = &gcc_aggre_usb3_prim_axi_clk.clkr,
	[GCC_AGGRE_USB3_SEC_AXI_CLK] = &gcc_aggre_usb3_sec_axi_clk.clkr,
	[GCC_APC_VS_CLK] = &gcc_apc_vs_clk.clkr,
	[GCC_BOOT_ROM_AHB_CLK] = &gcc_boot_rom_ahb_clk.clkr,
	[GCC_CAMERA_AHB_CLK] = &gcc_camera_ahb_clk.clkr,
	[GCC_CAMERA_AXI_CLK] = &gcc_camera_axi_clk.clkr,
	[GCC_CAMERA_XO_CLK] = &gcc_camera_xo_clk.clkr,
	[GCC_CE1_AHB_CLK] = &gcc_ce1_ahb_clk.clkr,
	[GCC_CE1_AXI_CLK] = &gcc_ce1_axi_clk.clkr,
	[GCC_CE1_CLK] = &gcc_ce1_clk.clkr,
	[GCC_CFG_NOC_USB3_PRIM_AXI_CLK] = &gcc_cfg_noc_usb3_prim_axi_clk.clkr,
	[GCC_CFG_NOC_USB3_SEC_AXI_CLK] = &gcc_cfg_noc_usb3_sec_axi_clk.clkr,
	[GCC_CPUSS_AHB_CLK] = &gcc_cpuss_ahb_clk.clkr,
	[GCC_CPUSS_AHB_CLK_SRC] = &gcc_cpuss_ahb_clk_src.clkr,
	[GCC_CPUSS_DVM_BUS_CLK] = &gcc_cpuss_dvm_bus_clk.clkr,
	[GCC_CPUSS_GNOC_CLK] = &gcc_cpuss_gnoc_clk.clkr,
	[GCC_CPUSS_RBCPR_CLK] = &gcc_cpuss_rbcpr_clk.clkr,
	[GCC_CPUSS_RBCPR_CLK_SRC] = &gcc_cpuss_rbcpr_clk_src.clkr,
	[GCC_DDRSS_GPU_AXI_CLK] = &gcc_ddrss_gpu_axi_clk.clkr,
	[GCC_DISP_AHB_CLK] = &gcc_disp_ahb_clk.clkr,
	[GCC_DISP_AXI_CLK] = &gcc_disp_axi_clk.clkr,
	[GCC_DISP_GPLL0_CLK_SRC] = &gcc_disp_gpll0_clk_src.clkr,
	[GCC_DISP_GPLL0_DIV_CLK_SRC] = &gcc_disp_gpll0_div_clk_src.clkr,
	[GCC_DISP_XO_CLK] = &gcc_disp_xo_clk.clkr,
	[GCC_GP1_CLK] = &gcc_gp1_clk.clkr,
	[GCC_GP1_CLK_SRC] = &gcc_gp1_clk_src.clkr,
	[GCC_GP2_CLK] = &gcc_gp2_clk.clkr,
	[GCC_GP2_CLK_SRC] = &gcc_gp2_clk_src.clkr,
	[GCC_GP3_CLK] = &gcc_gp3_clk.clkr,
	[GCC_GP3_CLK_SRC] = &gcc_gp3_clk_src.clkr,
	[GCC_GPU_CFG_AHB_CLK] = &gcc_gpu_cfg_ahb_clk.clkr,
	[GCC_GPU_GPLL0_CLK_SRC] = &gcc_gpu_gpll0_clk_src.clkr,
	[GCC_GPU_GPLL0_DIV_CLK_SRC] = &gcc_gpu_gpll0_div_clk_src.clkr,
	[GCC_GPU_IREF_CLK] = &gcc_gpu_iref_clk.clkr,
	[GCC_GPU_MEMNOC_GFX_CLK] = &gcc_gpu_memnoc_gfx_clk.clkr,
	[GCC_GPU_SNOC_DVM_GFX_CLK] = &gcc_gpu_snoc_dvm_gfx_clk.clkr,
	[GCC_GPU_VS_CLK] = &gcc_gpu_vs_clk.clkr,
	[GCC_MSS_AXIS2_CLK] = &gcc_mss_axis2_clk.clkr,
	[GCC_MSS_CFG_AHB_CLK] = &gcc_mss_cfg_ahb_clk.clkr,
	[GCC_MSS_GPLL0_DIV_CLK_SRC] = &gcc_mss_gpll0_div_clk_src.clkr,
	[GCC_MSS_MFAB_AXIS_CLK] = &gcc_mss_mfab_axis_clk.clkr,
	[GCC_MSS_Q6_MEMNOC_AXI_CLK] = &gcc_mss_q6_memnoc_axi_clk.clkr,
	[GCC_MSS_SNOC_AXI_CLK] = &gcc_mss_snoc_axi_clk.clkr,
	[GCC_MSS_VS_CLK] = &gcc_mss_vs_clk.clkr,
	[GCC_PCIE_0_AUX_CLK] = &gcc_pcie_0_aux_clk.clkr,
	[GCC_PCIE_0_AUX_CLK_SRC] = &gcc_pcie_0_aux_clk_src.clkr,
	[GCC_PCIE_0_CFG_AHB_CLK] = &gcc_pcie_0_cfg_ahb_clk.clkr,
	[GCC_PCIE_0_CLKREF_CLK] = &gcc_pcie_0_clkref_clk.clkr,
	[GCC_PCIE_0_MSTR_AXI_CLK] = &gcc_pcie_0_mstr_axi_clk.clkr,
	[GCC_PCIE_0_PIPE_CLK] = &gcc_pcie_0_pipe_clk.clkr,
	[GCC_PCIE_0_SLV_AXI_CLK] = &gcc_pcie_0_slv_axi_clk.clkr,
	[GCC_PCIE_0_SLV_Q2A_AXI_CLK] = &gcc_pcie_0_slv_q2a_axi_clk.clkr,
	[GCC_PCIE_1_AUX_CLK] = &gcc_pcie_1_aux_clk.clkr,
	[GCC_PCIE_1_AUX_CLK_SRC] = &gcc_pcie_1_aux_clk_src.clkr,
	[GCC_PCIE_1_CFG_AHB_CLK] = &gcc_pcie_1_cfg_ahb_clk.clkr,
	[GCC_PCIE_1_CLKREF_CLK] = &gcc_pcie_1_clkref_clk.clkr,
	[GCC_PCIE_1_MSTR_AXI_CLK] = &gcc_pcie_1_mstr_axi_clk.clkr,
	[GCC_PCIE_1_PIPE_CLK] = &gcc_pcie_1_pipe_clk.clkr,
	[GCC_PCIE_1_SLV_AXI_CLK] = &gcc_pcie_1_slv_axi_clk.clkr,
	[GCC_PCIE_1_SLV_Q2A_AXI_CLK] = &gcc_pcie_1_slv_q2a_axi_clk.clkr,
	[GCC_PCIE_PHY_AUX_CLK] = &gcc_pcie_phy_aux_clk.clkr,
	[GCC_PCIE_PHY_REFGEN_CLK] = &gcc_pcie_phy_refgen_clk.clkr,
	[GCC_PCIE_PHY_REFGEN_CLK_SRC] = &gcc_pcie_phy_refgen_clk_src.clkr,
	[GCC_PDM2_CLK] = &gcc_pdm2_clk.clkr,
	[GCC_PDM2_CLK_SRC] = &gcc_pdm2_clk_src.clkr,
	[GCC_PDM_AHB_CLK] = &gcc_pdm_ahb_clk.clkr,
	[GCC_PDM_XO4_CLK] = &gcc_pdm_xo4_clk.clkr,
	[GCC_PRNG_AHB_CLK] = &gcc_prng_ahb_clk.clkr,
	[GCC_QMIP_CAMERA_AHB_CLK] = &gcc_qmip_camera_ahb_clk.clkr,
	[GCC_QMIP_DISP_AHB_CLK] = &gcc_qmip_disp_ahb_clk.clkr,
	[GCC_QMIP_VIDEO_AHB_CLK] = &gcc_qmip_video_ahb_clk.clkr,
	[GCC_QUPV3_WRAP0_S0_CLK] = &gcc_qupv3_wrap0_s0_clk.clkr,
	[GCC_QUPV3_WRAP0_S0_CLK_SRC] = &gcc_qupv3_wrap0_s0_clk_src.clkr,
	[GCC_QUPV3_WRAP0_S1_CLK] = &gcc_qupv3_wrap0_s1_clk.clkr,
	[GCC_QUPV3_WRAP0_S1_CLK_SRC] = &gcc_qupv3_wrap0_s1_clk_src.clkr,
	[GCC_QUPV3_WRAP0_S2_CLK] = &gcc_qupv3_wrap0_s2_clk.clkr,
	[GCC_QUPV3_WRAP0_S2_CLK_SRC] = &gcc_qupv3_wrap0_s2_clk_src.clkr,
	[GCC_QUPV3_WRAP0_S3_CLK] = &gcc_qupv3_wrap0_s3_clk.clkr,
	[GCC_QUPV3_WRAP0_S3_CLK_SRC] = &gcc_qupv3_wrap0_s3_clk_src.clkr,
	[GCC_QUPV3_WRAP0_S4_CLK] = &gcc_qupv3_wrap0_s4_clk.clkr,
	[GCC_QUPV3_WRAP0_S4_CLK_SRC] = &gcc_qupv3_wrap0_s4_clk_src.clkr,
	[GCC_QUPV3_WRAP0_S5_CLK] = &gcc_qupv3_wrap0_s5_clk.clkr,
	[GCC_QUPV3_WRAP0_S5_CLK_SRC] = &gcc_qupv3_wrap0_s5_clk_src.clkr,
	[GCC_QUPV3_WRAP0_S6_CLK] = &gcc_qupv3_wrap0_s6_clk.clkr,
	[GCC_QUPV3_WRAP0_S6_CLK_SRC] = &gcc_qupv3_wrap0_s6_clk_src.clkr,
	[GCC_QUPV3_WRAP0_S7_CLK] = &gcc_qupv3_wrap0_s7_clk.clkr,
	[GCC_QUPV3_WRAP0_S7_CLK_SRC] = &gcc_qupv3_wrap0_s7_clk_src.clkr,
	[GCC_QUPV3_WRAP1_S0_CLK] = &gcc_qupv3_wrap1_s0_clk.clkr,
	[GCC_QUPV3_WRAP1_S0_CLK_SRC] = &gcc_qupv3_wrap1_s0_clk_src.clkr,
	[GCC_QUPV3_WRAP1_S1_CLK] = &gcc_qupv3_wrap1_s1_clk.clkr,
	[GCC_QUPV3_WRAP1_S1_CLK_SRC] = &gcc_qupv3_wrap1_s1_clk_src.clkr,
	[GCC_QUPV3_WRAP1_S2_CLK] = &gcc_qupv3_wrap1_s2_clk.clkr,
	[GCC_QUPV3_WRAP1_S2_CLK_SRC] = &gcc_qupv3_wrap1_s2_clk_src.clkr,
	[GCC_QUPV3_WRAP1_S3_CLK] = &gcc_qupv3_wrap1_s3_clk.clkr,
	[GCC_QUPV3_WRAP1_S3_CLK_SRC] = &gcc_qupv3_wrap1_s3_clk_src.clkr,
	[GCC_QUPV3_WRAP1_S4_CLK] = &gcc_qupv3_wrap1_s4_clk.clkr,
	[GCC_QUPV3_WRAP1_S4_CLK_SRC] = &gcc_qupv3_wrap1_s4_clk_src.clkr,
	[GCC_QUPV3_WRAP1_S5_CLK] = &gcc_qupv3_wrap1_s5_clk.clkr,
	[GCC_QUPV3_WRAP1_S5_CLK_SRC] = &gcc_qupv3_wrap1_s5_clk_src.clkr,
	[GCC_QUPV3_WRAP1_S6_CLK] = &gcc_qupv3_wrap1_s6_clk.clkr,
	[GCC_QUPV3_WRAP1_S6_CLK_SRC] = &gcc_qupv3_wrap1_s6_clk_src.clkr,
	[GCC_QUPV3_WRAP1_S7_CLK] = &gcc_qupv3_wrap1_s7_clk.clkr,
	[GCC_QUPV3_WRAP1_S7_CLK_SRC] = &gcc_qupv3_wrap1_s7_clk_src.clkr,
	[GCC_QUPV3_WRAP_0_M_AHB_CLK] = &gcc_qupv3_wrap_0_m_ahb_clk.clkr,
	[GCC_QUPV3_WRAP_0_S_AHB_CLK] = &gcc_qupv3_wrap_0_s_ahb_clk.clkr,
	[GCC_QUPV3_WRAP_1_M_AHB_CLK] = &gcc_qupv3_wrap_1_m_ahb_clk.clkr,
	[GCC_QUPV3_WRAP_1_S_AHB_CLK] = &gcc_qupv3_wrap_1_s_ahb_clk.clkr,
	[GCC_SDCC2_AHB_CLK] = &gcc_sdcc2_ahb_clk.clkr,
	[GCC_SDCC2_APPS_CLK] = &gcc_sdcc2_apps_clk.clkr,
	[GCC_SDCC2_APPS_CLK_SRC] = &gcc_sdcc2_apps_clk_src.clkr,
	[GCC_SDCC4_AHB_CLK] = &gcc_sdcc4_ahb_clk.clkr,
	[GCC_SDCC4_APPS_CLK] = &gcc_sdcc4_apps_clk.clkr,
	[GCC_SDCC4_APPS_CLK_SRC] = &gcc_sdcc4_apps_clk_src.clkr,
	[GCC_SYS_NOC_CPUSS_AHB_CLK] = &gcc_sys_noc_cpuss_ahb_clk.clkr,
	[GCC_TSIF_AHB_CLK] = &gcc_tsif_ahb_clk.clkr,
	[GCC_TSIF_INACTIVITY_TIMERS_CLK] =
					&gcc_tsif_inactivity_timers_clk.clkr,
	[GCC_TSIF_REF_CLK] = &gcc_tsif_ref_clk.clkr,
	[GCC_TSIF_REF_CLK_SRC] = &gcc_tsif_ref_clk_src.clkr,
	[GCC_UFS_CARD_AHB_CLK] = &gcc_ufs_card_ahb_clk.clkr,
	[GCC_UFS_CARD_AXI_CLK] = &gcc_ufs_card_axi_clk.clkr,
	[GCC_UFS_CARD_AXI_HW_CTL_CLK] = &gcc_ufs_card_axi_hw_ctl_clk.clkr,
	[GCC_UFS_CARD_AXI_CLK_SRC] = &gcc_ufs_card_axi_clk_src.clkr,
	[GCC_UFS_CARD_CLKREF_CLK] = &gcc_ufs_card_clkref_clk.clkr,
	[GCC_UFS_CARD_ICE_CORE_CLK] = &gcc_ufs_card_ice_core_clk.clkr,
	[GCC_UFS_CARD_ICE_CORE_HW_CTL_CLK] =
				&gcc_ufs_card_ice_core_hw_ctl_clk.clkr,
	[GCC_UFS_CARD_ICE_CORE_CLK_SRC] = &gcc_ufs_card_ice_core_clk_src.clkr,
	[GCC_UFS_CARD_PHY_AUX_CLK] = &gcc_ufs_card_phy_aux_clk.clkr,
	[GCC_UFS_CARD_PHY_AUX_HW_CTL_CLK] =
				&gcc_ufs_card_phy_aux_hw_ctl_clk.clkr,
	[GCC_UFS_CARD_PHY_AUX_CLK_SRC] = &gcc_ufs_card_phy_aux_clk_src.clkr,
	[GCC_UFS_CARD_RX_SYMBOL_0_CLK] = &gcc_ufs_card_rx_symbol_0_clk.clkr,
	[GCC_UFS_CARD_RX_SYMBOL_1_CLK] = &gcc_ufs_card_rx_symbol_1_clk.clkr,
	[GCC_UFS_CARD_TX_SYMBOL_0_CLK] = &gcc_ufs_card_tx_symbol_0_clk.clkr,
	[GCC_UFS_CARD_UNIPRO_CORE_CLK] = &gcc_ufs_card_unipro_core_clk.clkr,
	[GCC_UFS_CARD_UNIPRO_CORE_HW_CTL_CLK] =
				&gcc_ufs_card_unipro_core_hw_ctl_clk.clkr,
	[GCC_UFS_CARD_UNIPRO_CORE_CLK_SRC] =
					&gcc_ufs_card_unipro_core_clk_src.clkr,
	[GCC_UFS_MEM_CLKREF_CLK] = &gcc_ufs_mem_clkref_clk.clkr,
	[GCC_UFS_PHY_AHB_CLK] = &gcc_ufs_phy_ahb_clk.clkr,
	[GCC_UFS_PHY_AXI_CLK] = &gcc_ufs_phy_axi_clk.clkr,
	[GCC_UFS_PHY_AXI_HW_CTL_CLK] = &gcc_ufs_phy_axi_hw_ctl_clk.clkr,
	[GCC_UFS_PHY_AXI_CLK_SRC] = &gcc_ufs_phy_axi_clk_src.clkr,
	[GCC_UFS_PHY_ICE_CORE_CLK] = &gcc_ufs_phy_ice_core_clk.clkr,
	[GCC_UFS_PHY_ICE_CORE_HW_CTL_CLK] =
				&gcc_ufs_phy_ice_core_hw_ctl_clk.clkr,
	[GCC_UFS_PHY_ICE_CORE_CLK_SRC] = &gcc_ufs_phy_ice_core_clk_src.clkr,
	[GCC_UFS_PHY_PHY_AUX_CLK] = &gcc_ufs_phy_phy_aux_clk.clkr,
	[GCC_UFS_PHY_PHY_AUX_HW_CTL_CLK] = &gcc_ufs_phy_phy_aux_hw_ctl_clk.clkr,
	[GCC_UFS_PHY_PHY_AUX_CLK_SRC] = &gcc_ufs_phy_phy_aux_clk_src.clkr,
	[GCC_UFS_PHY_RX_SYMBOL_0_CLK] = &gcc_ufs_phy_rx_symbol_0_clk.clkr,
	[GCC_UFS_PHY_RX_SYMBOL_1_CLK] = &gcc_ufs_phy_rx_symbol_1_clk.clkr,
	[GCC_UFS_PHY_TX_SYMBOL_0_CLK] = &gcc_ufs_phy_tx_symbol_0_clk.clkr,
	[GCC_UFS_PHY_UNIPRO_CORE_CLK] = &gcc_ufs_phy_unipro_core_clk.clkr,
	[GCC_UFS_PHY_UNIPRO_CORE_HW_CTL_CLK] =
				&gcc_ufs_phy_unipro_core_hw_ctl_clk.clkr,
	[GCC_UFS_PHY_UNIPRO_CORE_CLK_SRC] =
					&gcc_ufs_phy_unipro_core_clk_src.clkr,
	[GCC_USB30_PRIM_MASTER_CLK] = &gcc_usb30_prim_master_clk.clkr,
	[GCC_USB30_PRIM_MASTER_CLK_SRC] = &gcc_usb30_prim_master_clk_src.clkr,
	[GCC_USB30_PRIM_MOCK_UTMI_CLK] = &gcc_usb30_prim_mock_utmi_clk.clkr,
	[GCC_USB30_PRIM_MOCK_UTMI_CLK_SRC] =
					&gcc_usb30_prim_mock_utmi_clk_src.clkr,
	[GCC_USB30_PRIM_SLEEP_CLK] = &gcc_usb30_prim_sleep_clk.clkr,
	[GCC_USB30_SEC_MASTER_CLK] = &gcc_usb30_sec_master_clk.clkr,
	[GCC_USB30_SEC_MASTER_CLK_SRC] = &gcc_usb30_sec_master_clk_src.clkr,
	[GCC_USB30_SEC_MOCK_UTMI_CLK] = &gcc_usb30_sec_mock_utmi_clk.clkr,
	[GCC_USB30_SEC_MOCK_UTMI_CLK_SRC] =
					&gcc_usb30_sec_mock_utmi_clk_src.clkr,
	[GCC_USB30_SEC_SLEEP_CLK] = &gcc_usb30_sec_sleep_clk.clkr,
	[GCC_USB3_PRIM_CLKREF_CLK] = &gcc_usb3_prim_clkref_clk.clkr,
	[GCC_USB3_PRIM_PHY_AUX_CLK] = &gcc_usb3_prim_phy_aux_clk.clkr,
	[GCC_USB3_PRIM_PHY_AUX_CLK_SRC] = &gcc_usb3_prim_phy_aux_clk_src.clkr,
	[GCC_USB3_PRIM_PHY_COM_AUX_CLK] = &gcc_usb3_prim_phy_com_aux_clk.clkr,
	[GCC_USB3_PRIM_PHY_PIPE_CLK] = &gcc_usb3_prim_phy_pipe_clk.clkr,
	[GCC_USB3_SEC_CLKREF_CLK] = &gcc_usb3_sec_clkref_clk.clkr,
	[GCC_USB3_SEC_PHY_AUX_CLK] = &gcc_usb3_sec_phy_aux_clk.clkr,
	[GCC_USB3_SEC_PHY_AUX_CLK_SRC] = &gcc_usb3_sec_phy_aux_clk_src.clkr,
	[GCC_USB3_SEC_PHY_COM_AUX_CLK] = &gcc_usb3_sec_phy_com_aux_clk.clkr,
	[GCC_USB3_SEC_PHY_PIPE_CLK] = &gcc_usb3_sec_phy_pipe_clk.clkr,
	[GCC_USB_PHY_CFG_AHB2PHY_CLK] = &gcc_usb_phy_cfg_ahb2phy_clk.clkr,
	[GCC_VDDA_VS_CLK] = &gcc_vdda_vs_clk.clkr,
	[GCC_VDDCX_VS_CLK] = &gcc_vddcx_vs_clk.clkr,
	[GCC_VDDMX_VS_CLK] = &gcc_vddmx_vs_clk.clkr,
	[GCC_VIDEO_AHB_CLK] = &gcc_video_ahb_clk.clkr,
	[GCC_VIDEO_AXI_CLK] = &gcc_video_axi_clk.clkr,
	[GCC_VIDEO_XO_CLK] = &gcc_video_xo_clk.clkr,
	[GCC_VS_CTRL_AHB_CLK] = &gcc_vs_ctrl_ahb_clk.clkr,
	[GCC_VS_CTRL_CLK] = &gcc_vs_ctrl_clk.clkr,
	[GCC_VS_CTRL_CLK_SRC] = &gcc_vs_ctrl_clk_src.clkr,
	[GCC_VSENSOR_CLK_SRC] = &gcc_vsensor_clk_src.clkr,
	[GPLL0] = &gpll0.clkr,
	[GPLL0_OUT_EVEN] = &gpll0_out_even.clkr,
	[GPLL4] = &gpll4.clkr,
	[GCC_SDCC1_AHB_CLK] = NULL,
	[GCC_SDCC1_APPS_CLK] = NULL,
	[GCC_SDCC1_ICE_CORE_CLK] = NULL,
	[GCC_SDCC1_APPS_CLK_SRC] = NULL,
	[GCC_SDCC1_ICE_CORE_CLK_SRC] = NULL,
	[GPLL6] = NULL,
};

static const struct qcom_reset_map gcc_sdm845_resets[] = {
	[GCC_MMSS_BCR] = { 0xb000 },
	[GCC_PCIE_0_BCR] = { 0x6b000 },
	[GCC_PCIE_1_BCR] = { 0x8d000 },
	[GCC_PCIE_PHY_BCR] = { 0x6f000 },
	[GCC_PDM_BCR] = { 0x33000 },
	[GCC_PRNG_BCR] = { 0x34000 },
	[GCC_QUPV3_WRAPPER_0_BCR] = { 0x17000 },
	[GCC_QUPV3_WRAPPER_1_BCR] = { 0x18000 },
	[GCC_QUSB2PHY_PRIM_BCR] = { 0x12000 },
	[GCC_QUSB2PHY_SEC_BCR] = { 0x12004 },
	[GCC_SDCC2_BCR] = { 0x14000 },
	[GCC_SDCC4_BCR] = { 0x16000 },
	[GCC_TSIF_BCR] = { 0x36000 },
	[GCC_UFS_CARD_BCR] = { 0x75000 },
	[GCC_UFS_PHY_BCR] = { 0x77000 },
	[GCC_USB30_PRIM_BCR] = { 0xf000 },
	[GCC_USB30_SEC_BCR] = { 0x10000 },
	[GCC_USB3_PHY_PRIM_BCR] = { 0x50000 },
	[GCC_USB3PHY_PHY_PRIM_BCR] = { 0x50004 },
	[GCC_USB3_DP_PHY_PRIM_BCR] = { 0x50008 },
	[GCC_USB3_PHY_SEC_BCR] = { 0x5000c },
	[GCC_USB3PHY_PHY_SEC_BCR] = { 0x50010 },
	[GCC_USB3_DP_PHY_SEC_BCR] = { 0x50014 },
	[GCC_USB_PHY_CFG_AHB2PHY_BCR] = { 0x6a000 },
	[GCC_PCIE_0_PHY_BCR] = { 0x6c01c },
	[GCC_PCIE_1_PHY_BCR] = { 0x8e01c },
	[GCC_SDCC1_BCR] = { 0x26000 },
};

/* List of RCG clocks and corresponding flags requested for DFS Mode */
static struct clk_dfs gcc_dfs_clocks[] = {
	{ &gcc_qupv3_wrap0_s0_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap0_s1_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap0_s2_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap0_s3_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap0_s4_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap0_s5_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap0_s6_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap0_s7_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap1_s0_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap1_s1_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap1_s2_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap1_s3_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap1_s4_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap1_s5_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap1_s6_clk_src, DFS_ENABLE_RCG },
	{ &gcc_qupv3_wrap1_s7_clk_src, DFS_ENABLE_RCG },
};

static const struct qcom_cc_dfs_desc gcc_sdm845_dfs_desc = {
	.clks = gcc_dfs_clocks,
	.num_clks = ARRAY_SIZE(gcc_dfs_clocks),
};

static const struct regmap_config gcc_sdm845_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x182090,
	.fast_io	= true,
};

static const struct qcom_cc_desc gcc_sdm845_desc = {
	.config = &gcc_sdm845_regmap_config,
	.clks = gcc_sdm845_clocks,
	.num_clks = ARRAY_SIZE(gcc_sdm845_clocks),
	.resets = gcc_sdm845_resets,
	.num_resets = ARRAY_SIZE(gcc_sdm845_resets),
};

static const struct of_device_id gcc_sdm845_match_table[] = {
	{ .compatible = "qcom,gcc-sdm845" },
	{ .compatible = "qcom,gcc-sdm845-v2" },
	{ .compatible = "qcom,gcc-sdm845-v2.1" },
	{ .compatible = "qcom,gcc-sdm670" },
	{ }
};
MODULE_DEVICE_TABLE(of, gcc_sdm845_match_table);

static void gcc_sdm845_fixup_sdm845v2(void)
{
	gcc_qupv3_wrap0_s0_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap0_s0_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap0_s0_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap0_s1_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap0_s1_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap0_s1_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap0_s2_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap0_s2_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap0_s2_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap0_s3_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap0_s3_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap0_s3_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap0_s4_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap0_s4_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap0_s4_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap0_s5_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap0_s5_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap0_s5_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap0_s6_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap0_s6_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap0_s6_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap0_s7_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap0_s7_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap0_s7_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap1_s0_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap1_s0_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap1_s0_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap1_s1_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap1_s1_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap1_s1_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap1_s2_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap1_s2_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap1_s2_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap1_s3_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap1_s3_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap1_s3_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap1_s4_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap1_s4_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap1_s4_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap1_s5_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap1_s5_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap1_s5_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap1_s6_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap1_s6_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap1_s6_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_qupv3_wrap1_s7_clk_src.freq_tbl =
		ftbl_gcc_qupv3_wrap0_s0_clk_src_sdm845_v2;
	gcc_qupv3_wrap1_s7_clk_src.clkr.hw.init->rate_max[VDD_CX_MIN] =
		50000000;
	gcc_qupv3_wrap1_s7_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		128000000;
	gcc_ufs_card_axi_clk_src.freq_tbl =
		ftbl_gcc_ufs_card_axi_clk_src_sdm845_v2;
	gcc_ufs_card_axi_clk_src.clkr.hw.init->rate_max[VDD_CX_HIGH] =
		240000000;
	gcc_ufs_phy_axi_clk_src.freq_tbl =
		ftbl_gcc_ufs_card_axi_clk_src_sdm845_v2;
	gcc_vsensor_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 600000000;
}

static void gcc_sdm845_fixup_sdm670(void)
{
	gcc_sdm845_fixup_sdm845v2();

	gcc_sdm845_clocks[GCC_SDCC1_AHB_CLK] = &gcc_sdcc1_ahb_clk.clkr;
	gcc_sdm845_clocks[GCC_SDCC1_APPS_CLK] = &gcc_sdcc1_apps_clk.clkr;
	gcc_sdm845_clocks[GCC_SDCC1_ICE_CORE_CLK] =
					&gcc_sdcc1_ice_core_clk.clkr;
	gcc_sdm845_clocks[GCC_SDCC1_APPS_CLK_SRC] =
					&gcc_sdcc1_apps_clk_src.clkr;
	gcc_sdm845_clocks[GCC_SDCC1_ICE_CORE_CLK_SRC] =
					&gcc_sdcc1_ice_core_clk_src.clkr;
	gcc_sdm845_clocks[GPLL6] = &gpll6.clkr;
	gcc_sdm845_clocks[GCC_AGGRE_UFS_CARD_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_AGGRE_UFS_CARD_AXI_HW_CTL_CLK] = NULL;
	gcc_sdm845_clocks[GCC_AGGRE_USB3_SEC_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_AGGRE_NOC_PCIE_TBU_CLK] = NULL;
	gcc_sdm845_clocks[GCC_CFG_NOC_USB3_SEC_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_0_AUX_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_0_AUX_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_0_CFG_AHB_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_0_CLKREF_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_0_MSTR_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_0_PIPE_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_0_SLV_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_0_SLV_Q2A_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_1_AUX_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_1_AUX_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_1_CFG_AHB_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_1_CLKREF_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_1_MSTR_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_1_PIPE_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_1_SLV_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_1_SLV_Q2A_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_PHY_AUX_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_PHY_REFGEN_CLK] = NULL;
	gcc_sdm845_clocks[GCC_PCIE_PHY_REFGEN_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_AHB_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_AXI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_AXI_HW_CTL_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_AXI_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_CLKREF_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_ICE_CORE_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_ICE_CORE_HW_CTL_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_ICE_CORE_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_PHY_AUX_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_PHY_AUX_HW_CTL_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_PHY_AUX_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_RX_SYMBOL_0_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_RX_SYMBOL_1_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_TX_SYMBOL_0_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_UNIPRO_CORE_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_UNIPRO_CORE_HW_CTL_CLK] = NULL;
	gcc_sdm845_clocks[GCC_UFS_CARD_UNIPRO_CORE_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_UFS_PHY_RX_SYMBOL_1_CLK] = NULL;
	gcc_sdm845_clocks[GCC_USB30_SEC_MASTER_CLK] = NULL;
	gcc_sdm845_clocks[GCC_USB30_SEC_MASTER_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_USB30_SEC_MOCK_UTMI_CLK] = NULL;
	gcc_sdm845_clocks[GCC_USB30_SEC_MOCK_UTMI_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_USB30_SEC_SLEEP_CLK] = NULL;
	gcc_sdm845_clocks[GCC_USB3_SEC_CLKREF_CLK] = NULL;
	gcc_sdm845_clocks[GCC_USB3_SEC_PHY_AUX_CLK] = NULL;
	gcc_sdm845_clocks[GCC_USB3_SEC_PHY_AUX_CLK_SRC] = NULL;
	gcc_sdm845_clocks[GCC_USB3_SEC_PHY_COM_AUX_CLK] = NULL;
	gcc_sdm845_clocks[GCC_USB3_SEC_PHY_PIPE_CLK] = NULL;

	gcc_cpuss_rbcpr_clk_src.freq_tbl = ftbl_gcc_cpuss_rbcpr_clk_src_sdm670;
	gcc_cpuss_rbcpr_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		50000000;
	gcc_sdcc2_apps_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 50000000;
	gcc_sdcc2_apps_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW_L1] =
		100000000;
	gcc_sdcc2_apps_clk_src.clkr.hw.init->rate_max[VDD_CX_NOMINAL] =
		201500000;
	gcc_sdcc4_apps_clk_src.freq_tbl = ftbl_gcc_sdcc4_apps_clk_src_sdm670;
	gcc_sdcc4_apps_clk_src.clkr.hw.init->rate_max[VDD_CX_LOWER] = 33333333;
}

static int gcc_sdm845_fixup(struct platform_device *pdev)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,gcc-sdm845-v2") ||
			!strcmp(compat, "qcom,gcc-sdm845-v2.1"))
		gcc_sdm845_fixup_sdm845v2();
	else if (!strcmp(compat, "qcom,gcc-sdm670"))
		gcc_sdm845_fixup_sdm670();

	return 0;
}

static int gcc_sdm845_probe(struct platform_device *pdev)
{
	struct clk *clk;
	struct regmap *regmap;
	int i, ret = 0;

	regmap = qcom_cc_map(pdev, &gcc_sdm845_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	vdd_cx_ao.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx_ao");
	if (IS_ERR(vdd_cx_ao.regulator[0])) {
		if (!(PTR_ERR(vdd_cx_ao.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_cx_ao regulator\n");
		return PTR_ERR(vdd_cx_ao.regulator[0]);
	}

	ret = gcc_sdm845_fixup(pdev);
	if (ret)
		return ret;

	/* Register the dummy measurement clocks */
	for (i = 0; i < ARRAY_SIZE(gcc_sdm845_hws); i++) {
		clk = devm_clk_register(&pdev->dev, gcc_sdm845_hws[i]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
	}

	ret = qcom_cc_really_probe(pdev, &gcc_sdm845_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GCC clocks\n");
		return ret;
	}

	/* Disable the GPLL0 active input to MMSS and GPU via MISC registers */
	regmap_update_bits(regmap, GCC_MMSS_MISC, 0x3, 0x3);
	regmap_update_bits(regmap, GCC_GPU_MISC, 0x3, 0x3);

	/* Keep this clock on all the times except on SDM845 v2.1 */
	if (!of_device_is_compatible(pdev->dev.of_node, "qcom,gcc-sdm845-v2.1"))
		clk_prepare_enable(gcc_aggre_noc_pcie_tbu_clk.clkr.hw.clk);

	/* DFS clock registration */
	ret = qcom_cc_register_rcg_dfs(pdev, &gcc_sdm845_dfs_desc);
	if (ret)
		dev_err(&pdev->dev, "Failed to register with DFS!\n");

	dev_info(&pdev->dev, "Registered GCC clocks\n");
	return ret;
}

static struct platform_driver gcc_sdm845_driver = {
	.probe		= gcc_sdm845_probe,
	.driver		= {
		.name	= "gcc-sdm845",
		.of_match_table = gcc_sdm845_match_table,
	},
};

static int __init gcc_sdm845_init(void)
{
	return platform_driver_register(&gcc_sdm845_driver);
}
subsys_initcall(gcc_sdm845_init);

static void __exit gcc_sdm845_exit(void)
{
	platform_driver_unregister(&gcc_sdm845_driver);
}
module_exit(gcc_sdm845_exit);

MODULE_DESCRIPTION("QTI GCC SDM845 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gcc-sdm845");

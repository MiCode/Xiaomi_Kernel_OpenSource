/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <dt-bindings/clock/qcom,gcc-sdxpoorwills.h>

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
static DEFINE_VDD_REGULATORS(vdd_cx_ao, VDD_CX_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_CORE_BI_PLL_TEST_SE,
	P_GPLL0_OUT_EVEN,
	P_GPLL0_OUT_MAIN,
	P_GPLL4_OUT_EVEN,
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

static const char * const gcc_parent_names_ao_0[] = {
	"bi_tcxo_ao",
	"gpll0",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_1[] = {
	"bi_tcxo",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_SLEEP_CLK, 5 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_2[] = {
	"bi_tcxo",
	"gpll0",
	"core_pi_sleep_clk",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_SLEEP_CLK, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_3[] = {
	"bi_tcxo",
	"core_pi_sleep_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL4_OUT_EVEN, 2 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_4[] = {
	"bi_tcxo",
	"gpll0",
	"gpll4_out_even",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static struct pll_vco trion_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static struct clk_alpha_pll gpll0 = {
	.offset = 0x0,
	.vco_table = trion_vco,
	.num_vco = ARRAY_SIZE(trion_vco),
	.type = TRION_PLL,
	.clkr = {
		.enable_reg = 0x6d000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_trion_fixed_pll_ops,
		},
	},
};

static const struct clk_div_table post_div_table_trion_even[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{ }
};

static struct clk_alpha_pll_postdiv gpll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_trion_even,
	.num_post_div = ARRAY_SIZE(post_div_table_trion_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpll0_out_even",
		.parent_names = (const char *[]){ "gpll0" },
		.num_parents = 1,
		.ops = &clk_trion_pll_postdiv_ops,
	},
};

static struct clk_alpha_pll gpll4 = {
	.offset = 0x76000,
	.vco_table = trion_vco,
	.num_vco = ARRAY_SIZE(trion_vco),
	.type = TRION_PLL,
	.clkr = {
		.enable_reg = 0x6d000,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data){
			.name = "gpll4",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_trion_fixed_pll_ops,
			VDD_CX_FMAX_MAP4(
				MIN, 615000000,
				LOW, 1066000000,
				LOW_L1, 1600000000,
				NOMINAL, 2000000000),
		},
	},
};

static struct clk_alpha_pll_postdiv gpll4_out_even = {
	.offset = 0x76000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_trion_even,
	.num_post_div = ARRAY_SIZE(post_div_table_trion_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpll4_out_even",
		.parent_names = (const char *[]){ "gpll4" },
		.num_parents = 1,
		.ops = &clk_trion_pll_postdiv_ops,
	},
};

static const struct freq_tbl ftbl_gcc_blsp1_qup1_i2c_apps_clk_src[] = {
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(50000000, P_GPLL0_OUT_MAIN, 12, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr = 0x11024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_qup1_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_qup1_i2c_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 50000000),
	},
};

static const struct freq_tbl ftbl_gcc_blsp1_qup1_spi_apps_clk_src[] = {
	F(960000, P_BI_TCXO, 10, 1, 2),
	F(4800000, P_BI_TCXO, 4, 0, 0),
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(15000000, P_GPLL0_OUT_EVEN, 5, 1, 4),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(24000000, P_GPLL0_OUT_MAIN, 12.5, 1, 2),
	F(25000000, P_GPLL0_OUT_MAIN, 12, 1, 2),
	F(50000000, P_GPLL0_OUT_MAIN, 12, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr = 0x1100c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_qup1_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_qup1_spi_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 25000000,
			NOMINAL, 50000000),
	},
};

static struct clk_rcg2 gcc_blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr = 0x13024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_qup1_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_qup2_i2c_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 50000000),
	},
};

static struct clk_rcg2 gcc_blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr = 0x1300c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_qup1_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_qup2_spi_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 25000000,
			NOMINAL, 50000000),
	},
};

static struct clk_rcg2 gcc_blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr = 0x15024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_qup1_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_qup3_i2c_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 50000000),
	},
};

static struct clk_rcg2 gcc_blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr = 0x1500c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_qup1_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_qup3_spi_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 25000000,
			NOMINAL, 50000000),
	},
};

static struct clk_rcg2 gcc_blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr = 0x17024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_qup1_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_qup4_i2c_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 50000000),
	},
};

static struct clk_rcg2 gcc_blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr = 0x1700c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_qup1_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_qup4_spi_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 25000000,
			NOMINAL, 50000000),
	},
};

static const struct freq_tbl ftbl_gcc_blsp1_uart1_apps_clk_src[] = {
	F(3686400, P_GPLL0_OUT_EVEN, 1, 192, 15625),
	F(7372800, P_GPLL0_OUT_EVEN, 1, 384, 15625),
	F(9600000, P_BI_TCXO, 2, 0, 0),
	F(14745600, P_GPLL0_OUT_EVEN, 1, 768, 15625),
	F(16000000, P_GPLL0_OUT_EVEN, 1, 4, 75),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(19354839, P_GPLL0_OUT_MAIN, 15.5, 1, 2),
	F(20000000, P_GPLL0_OUT_MAIN, 15, 1, 2),
	F(20689655, P_GPLL0_OUT_MAIN, 14.5, 1, 2),
	F(21428571, P_GPLL0_OUT_MAIN, 14, 1, 2),
	F(22222222, P_GPLL0_OUT_MAIN, 13.5, 1, 2),
	F(23076923, P_GPLL0_OUT_MAIN, 13, 1, 2),
	F(24000000, P_GPLL0_OUT_MAIN, 5, 1, 5),
	F(25000000, P_GPLL0_OUT_MAIN, 12, 1, 2),
	F(26086957, P_GPLL0_OUT_MAIN, 11.5, 1, 2),
	F(27272727, P_GPLL0_OUT_MAIN, 11, 1, 2),
	F(28571429, P_GPLL0_OUT_MAIN, 10.5, 1, 2),
	F(32000000, P_GPLL0_OUT_MAIN, 1, 4, 75),
	F(40000000, P_GPLL0_OUT_MAIN, 15, 0, 0),
	F(46400000, P_GPLL0_OUT_MAIN, 1, 29, 375),
	F(48000000, P_GPLL0_OUT_MAIN, 12.5, 0, 0),
	F(51200000, P_GPLL0_OUT_MAIN, 1, 32, 375),
	F(56000000, P_GPLL0_OUT_MAIN, 1, 7, 75),
	F(58982400, P_GPLL0_OUT_MAIN, 1, 1536, 15625),
	F(60000000, P_GPLL0_OUT_MAIN, 10, 0, 0),
	F(63157895, P_GPLL0_OUT_MAIN, 9.5, 0, 0),
	{ }
};


static struct clk_rcg2 gcc_blsp1_uart1_apps_clk_src = {
	.cmd_rcgr = 0x1200c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_uart1_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_uart1_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 48000000,
			NOMINAL, 63157895),
	},
};

static struct clk_rcg2 gcc_blsp1_uart2_apps_clk_src = {
	.cmd_rcgr = 0x1400c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_uart1_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_uart2_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 48000000,
			NOMINAL, 63157895),
	},
};

static struct clk_rcg2 gcc_blsp1_uart3_apps_clk_src = {
	.cmd_rcgr = 0x1600c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_uart1_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_uart3_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 48000000,
			NOMINAL, 63157895),
	},
};

static struct clk_rcg2 gcc_blsp1_uart4_apps_clk_src = {
	.cmd_rcgr = 0x1800c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_blsp1_uart1_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_blsp1_uart4_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 48000000,
			NOMINAL, 63157895),
	},
};

static const struct freq_tbl ftbl_gcc_cpuss_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_cpuss_ahb_clk_src = {
	.cmd_rcgr = 0x24010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_cpuss_ahb_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_cpuss_ahb_clk_src",
		.parent_names = gcc_parent_names_ao_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1_AO(
				MIN, 19200000),
	},
};

static const struct freq_tbl ftbl_gcc_cpuss_rbcpr_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_cpuss_rbcpr_clk_src = {
	.cmd_rcgr = 0x2402c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_cpuss_rbcpr_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_cpuss_rbcpr_clk_src",
		.parent_names = gcc_parent_names_ao_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1_AO(
			MIN, 19200000),
	},
};

static const struct freq_tbl ftbl_gcc_emac_clk_src[] = {
	F(2500000, P_BI_TCXO, 1, 25, 192),
	F(5000000, P_BI_TCXO, 1, 25, 96),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(25000000, P_GPLL0_OUT_EVEN, 12, 0, 0),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(125000000, P_GPLL4_OUT_EVEN, 4, 0, 0),
	F(250000000, P_GPLL4_OUT_EVEN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_emac_clk_src = {
	.cmd_rcgr = 0x47020,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_4,
	.freq_tbl = ftbl_gcc_emac_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_emac_clk_src",
		.parent_names = gcc_parent_names_4,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 125000000,
			NOMINAL, 250000000),
	},
};

static const struct freq_tbl ftbl_gcc_emac_ptp_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(25000000, P_GPLL0_OUT_EVEN, 12, 0, 0),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(125000000, P_GPLL4_OUT_EVEN, 4, 0, 0),
	F(250000000, P_GPLL4_OUT_EVEN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_emac_ptp_clk_src = {
	.cmd_rcgr = 0x47038,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_4,
	.freq_tbl = ftbl_gcc_emac_ptp_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_emac_ptp_clk_src",
		.parent_names = gcc_parent_names_4,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 125000000,
			NOMINAL, 250000000),
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
	.cmd_rcgr = 0x2b004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_2,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_gp1_clk_src",
		.parent_names = gcc_parent_names_2,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static struct clk_rcg2 gcc_gp2_clk_src = {
	.cmd_rcgr = 0x2c004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_2,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_gp2_clk_src",
		.parent_names = gcc_parent_names_2,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static struct clk_rcg2 gcc_gp3_clk_src = {
	.cmd_rcgr = 0x2d004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_2,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_gp3_clk_src",
		.parent_names = gcc_parent_names_2,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static const struct freq_tbl ftbl_gcc_pcie_aux_phy_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_pcie_aux_phy_clk_src = {
	.cmd_rcgr = 0x37030,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_3,
	.freq_tbl = ftbl_gcc_pcie_aux_phy_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_pcie_aux_phy_clk_src",
		.parent_names = gcc_parent_names_3,
		.num_parents = 3,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1(
			MIN, 19200000),
	},
};

static const struct freq_tbl ftbl_gcc_pcie_phy_refgen_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN, 6, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_pcie_phy_refgen_clk_src = {
	.cmd_rcgr = 0x39010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_pcie_phy_refgen_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_pcie_phy_refgen_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
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
	.cmd_rcgr = 0x19010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_pdm2_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_pdm2_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 9600000,
			LOWER, 19200000,
			LOW, 60000000),
	},
};

static struct clk_rcg2 gcc_sdcc1_apps_clk_src = {
	.cmd_rcgr = 0xf00c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_sdcc1_apps_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP4(
			MIN, 19200000,
			LOWER, 50000000,
			LOW, 100000000,
			NOMINAL, 200000000),
	},
};

static struct clk_rcg2 gcc_spmi_fetcher_clk_src = {
	.cmd_rcgr = 0x3f00c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_1,
	.freq_tbl = ftbl_gcc_pcie_aux_phy_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_spmi_fetcher_clk_src",
		.parent_names = gcc_parent_names_1,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1(
			MIN, 19200000),
	},
};

static const struct freq_tbl ftbl_gcc_usb30_master_clk_src[] = {
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(75000000, P_GPLL0_OUT_EVEN, 4, 0, 0),
	F(120000000, P_GPLL0_OUT_MAIN, 5, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(240000000, P_GPLL0_OUT_MAIN, 2.5, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_usb30_master_clk_src = {
	.cmd_rcgr = 0xb01c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_usb30_master_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb30_master_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP5(
			MIN, 50000000,
			LOWER, 75000000,
			LOW, 120000000,
			NOMINAL, 200000000,
			HIGH, 240000000),
	},
};

static const struct freq_tbl ftbl_gcc_usb30_mock_utmi_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(40000000, P_GPLL0_OUT_EVEN, 7.5, 0, 0),
	F(60000000, P_GPLL0_OUT_MAIN, 10, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_usb30_mock_utmi_clk_src = {
	.cmd_rcgr = 0xb034,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_usb30_mock_utmi_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb30_mock_utmi_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP3(
			MIN, 19200000,
			LOWER, 40000000,
			LOW, 60000000),
	},
};

static const struct freq_tbl ftbl_gcc_usb3_phy_aux_clk_src[] = {
	F(1000000, P_BI_TCXO, 1, 5, 96),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_usb3_phy_aux_clk_src = {
	.cmd_rcgr = 0xb05c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_3,
	.freq_tbl = ftbl_gcc_usb3_phy_aux_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb3_phy_aux_clk_src",
		.parent_names = gcc_parent_names_3,
		.num_parents = 3,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP1(
			MIN, 19200000),
	},
};

static struct clk_branch gcc_blsp1_ahb_clk = {
	.halt_reg = 0x10004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d004,
		.enable_mask = BIT(25),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup1_i2c_apps_clk = {
	.halt_reg = 0x11008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x11008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup1_i2c_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_qup1_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup1_spi_apps_clk = {
	.halt_reg = 0x11004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x11004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup1_spi_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_qup1_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup2_i2c_apps_clk = {
	.halt_reg = 0x13008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x13008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup2_i2c_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_qup2_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup2_spi_apps_clk = {
	.halt_reg = 0x13004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x13004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup2_spi_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_qup2_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup3_i2c_apps_clk = {
	.halt_reg = 0x15008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x15008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup3_i2c_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_qup3_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup3_spi_apps_clk = {
	.halt_reg = 0x15004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x15004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup3_spi_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_qup3_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup4_i2c_apps_clk = {
	.halt_reg = 0x17008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x17008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup4_i2c_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_qup4_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup4_spi_apps_clk = {
	.halt_reg = 0x17004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x17004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup4_spi_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_qup4_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_sleep_clk = {
	.halt_reg = 0x10008,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d004,
		.enable_mask = BIT(26),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart1_apps_clk = {
	.halt_reg = 0x12004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x12004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart1_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_uart1_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart2_apps_clk = {
	.halt_reg = 0x14004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x14004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart2_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_uart2_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart3_apps_clk = {
	.halt_reg = 0x16004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x16004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart3_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_uart3_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart4_apps_clk = {
	.halt_reg = 0x18004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x18004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart4_apps_clk",
			.parent_names = (const char *[]){
				"gcc_blsp1_uart4_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_boot_rom_ahb_clk = {
	.halt_reg = 0x1c004,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x1c004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x6d004,
		.enable_mask = BIT(10),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_boot_rom_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce1_ahb_clk = {
	.halt_reg = 0x2100c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x2100c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x6d004,
		.enable_mask = BIT(3),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ce1_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce1_axi_clk = {
	.halt_reg = 0x21008,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d004,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ce1_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce1_clk = {
	.halt_reg = 0x21004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d004,
		.enable_mask = BIT(5),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ce1_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_cpuss_ahb_clk = {
	.halt_reg = 0x24000,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d004,
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

static struct clk_branch gcc_cpuss_gnoc_clk = {
	.halt_reg = 0x24004,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x24004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x6d004,
		.enable_mask = BIT(22),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_cpuss_gnoc_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_cpuss_rbcpr_clk = {
	.halt_reg = 0x24008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x24008,
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

static struct clk_branch gcc_eth_axi_clk = {
	.halt_reg = 0x4701c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4701c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_eth_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_eth_ptp_clk = {
	.halt_reg = 0x47018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x47018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_eth_ptp_clk",
			.parent_names = (const char *[]){
				"gcc_emac_ptp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_eth_rgmii_clk = {
	.halt_reg = 0x47010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x47010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_eth_rgmii_clk",
			.parent_names = (const char *[]){
				"gcc_emac_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_eth_slave_ahb_clk = {
	.halt_reg = 0x47014,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x47014,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x47014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_eth_slave_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp1_clk = {
	.halt_reg = 0x2b000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2b000,
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
	.halt_reg = 0x2c000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2c000,
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
	.halt_reg = 0x2d000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2d000,
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

static struct clk_branch gcc_pcie_0_clkref_clk = {
	.halt_reg = 0x88004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x88004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_0_clkref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_aux_clk = {
	.halt_reg = 0x37020,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x6d00c,
		.enable_mask = BIT(3),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_aux_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_cfg_ahb_clk = {
	.halt_reg = 0x3701c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x3701c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x6d00c,
		.enable_mask = BIT(2),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_cfg_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_mstr_axi_clk = {
	.halt_reg = 0x37018,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d00c,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_mstr_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_phy_refgen_clk = {
	.halt_reg = 0x39028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x39028,
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

static struct clk_branch gcc_pcie_pipe_clk = {
	.halt_reg = 0x37028,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x6d00c,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_pipe_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_sleep_clk = {
	.halt_reg = 0x37024,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d00c,
		.enable_mask = BIT(6),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_sleep_clk",
			.parent_names = (const char *[]){
				"gcc_pcie_aux_phy_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_slv_axi_clk = {
	.halt_reg = 0x37014,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x37014,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x6d00c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_slv_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_slv_q2a_axi_clk = {
	.halt_reg = 0x37010,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d00c,
		.enable_mask = BIT(5),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_slv_q2a_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pdm2_clk = {
	.halt_reg = 0x1900c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1900c,
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
	.halt_reg = 0x19004,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0x19004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x19004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pdm_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pdm_xo4_clk = {
	.halt_reg = 0x19008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x19008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pdm_xo4_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_prng_ahb_clk = {
	.halt_reg = 0x1a004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d004,
		.enable_mask = BIT(13),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_prng_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc1_ahb_clk = {
	.halt_reg = 0xf008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xf008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sdcc1_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc1_apps_clk = {
	.halt_reg = 0xf004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xf004,
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

static struct clk_branch gcc_spmi_fetcher_ahb_clk = {
	.halt_reg = 0x3f008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3f008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_spmi_fetcher_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_spmi_fetcher_clk = {
	.halt_reg = 0x3f004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3f004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_spmi_fetcher_clk",
			.parent_names = (const char *[]){
				"gcc_spmi_fetcher_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sys_noc_cpuss_ahb_clk = {
	.halt_reg = 0x400c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d004,
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

static struct clk_branch gcc_sys_noc_usb3_clk = {
	.halt_reg = 0x4018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sys_noc_usb3_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_master_clk = {
	.halt_reg = 0xb010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_master_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_mock_utmi_clk = {
	.halt_reg = 0xb018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_mock_utmi_clk",
			.parent_names = (const char *[]){
				"gcc_usb30_mock_utmi_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_sleep_clk = {
	.halt_reg = 0xb014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb3_phy_aux_clk = {
	.halt_reg = 0xb050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_phy_aux_clk",
			.parent_names = (const char *[]){
				"gcc_usb3_phy_aux_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_gate2 gcc_usb3_phy_pipe_clk = {
	.udelay = 500,
	.clkr = {
		.enable_reg = 0xb054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_phy_pipe_clk",
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_branch gcc_usb3_prim_clkref_clk = {
	.halt_reg = 0x88000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x88000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_prim_clkref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_phy_cfg_ahb2phy_clk = {
	.halt_reg = 0xe004,
	.halt_check = BRANCH_HALT,
	.hwcg_reg = 0xe004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0xe004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb_phy_cfg_ahb2phy_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

/* Measure-only clock for gcc_ipa_2x_clk. */
static struct clk_dummy measure_only_ipa_2x_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_ipa_2x_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *gcc_sdxpoorwills_hws[] = {
	[MEASURE_ONLY_IPA_2X_CLK] = &measure_only_ipa_2x_clk.hw,
};

static struct clk_regmap *gcc_sdxpoorwills_clocks[] = {
	[GCC_BLSP1_AHB_CLK] = &gcc_blsp1_ahb_clk.clkr,
	[GCC_BLSP1_QUP1_I2C_APPS_CLK] = &gcc_blsp1_qup1_i2c_apps_clk.clkr,
	[GCC_BLSP1_QUP1_I2C_APPS_CLK_SRC] =
		&gcc_blsp1_qup1_i2c_apps_clk_src.clkr,
	[GCC_BLSP1_QUP1_SPI_APPS_CLK] = &gcc_blsp1_qup1_spi_apps_clk.clkr,
	[GCC_BLSP1_QUP1_SPI_APPS_CLK_SRC] =
		&gcc_blsp1_qup1_spi_apps_clk_src.clkr,
	[GCC_BLSP1_QUP2_I2C_APPS_CLK] = &gcc_blsp1_qup2_i2c_apps_clk.clkr,
	[GCC_BLSP1_QUP2_I2C_APPS_CLK_SRC] =
		&gcc_blsp1_qup2_i2c_apps_clk_src.clkr,
	[GCC_BLSP1_QUP2_SPI_APPS_CLK] = &gcc_blsp1_qup2_spi_apps_clk.clkr,
	[GCC_BLSP1_QUP2_SPI_APPS_CLK_SRC] =
		&gcc_blsp1_qup2_spi_apps_clk_src.clkr,
	[GCC_BLSP1_QUP3_I2C_APPS_CLK] = &gcc_blsp1_qup3_i2c_apps_clk.clkr,
	[GCC_BLSP1_QUP3_I2C_APPS_CLK_SRC] =
		&gcc_blsp1_qup3_i2c_apps_clk_src.clkr,
	[GCC_BLSP1_QUP3_SPI_APPS_CLK] = &gcc_blsp1_qup3_spi_apps_clk.clkr,
	[GCC_BLSP1_QUP3_SPI_APPS_CLK_SRC] =
		&gcc_blsp1_qup3_spi_apps_clk_src.clkr,
	[GCC_BLSP1_QUP4_I2C_APPS_CLK] = &gcc_blsp1_qup4_i2c_apps_clk.clkr,
	[GCC_BLSP1_QUP4_I2C_APPS_CLK_SRC] =
		&gcc_blsp1_qup4_i2c_apps_clk_src.clkr,
	[GCC_BLSP1_QUP4_SPI_APPS_CLK] = &gcc_blsp1_qup4_spi_apps_clk.clkr,
	[GCC_BLSP1_QUP4_SPI_APPS_CLK_SRC] =
		&gcc_blsp1_qup4_spi_apps_clk_src.clkr,
	[GCC_BLSP1_SLEEP_CLK] = &gcc_blsp1_sleep_clk.clkr,
	[GCC_BLSP1_UART1_APPS_CLK] = &gcc_blsp1_uart1_apps_clk.clkr,
	[GCC_BLSP1_UART1_APPS_CLK_SRC] = &gcc_blsp1_uart1_apps_clk_src.clkr,
	[GCC_BLSP1_UART2_APPS_CLK] = &gcc_blsp1_uart2_apps_clk.clkr,
	[GCC_BLSP1_UART2_APPS_CLK_SRC] = &gcc_blsp1_uart2_apps_clk_src.clkr,
	[GCC_BLSP1_UART3_APPS_CLK] = &gcc_blsp1_uart3_apps_clk.clkr,
	[GCC_BLSP1_UART3_APPS_CLK_SRC] = &gcc_blsp1_uart3_apps_clk_src.clkr,
	[GCC_BLSP1_UART4_APPS_CLK] = &gcc_blsp1_uart4_apps_clk.clkr,
	[GCC_BLSP1_UART4_APPS_CLK_SRC] = &gcc_blsp1_uart4_apps_clk_src.clkr,
	[GCC_BOOT_ROM_AHB_CLK] = &gcc_boot_rom_ahb_clk.clkr,
	[GCC_CE1_AHB_CLK] = &gcc_ce1_ahb_clk.clkr,
	[GCC_CE1_AXI_CLK] = &gcc_ce1_axi_clk.clkr,
	[GCC_CE1_CLK] = &gcc_ce1_clk.clkr,
	[GCC_CPUSS_AHB_CLK] = &gcc_cpuss_ahb_clk.clkr,
	[GCC_CPUSS_AHB_CLK_SRC] = &gcc_cpuss_ahb_clk_src.clkr,
	[GCC_CPUSS_GNOC_CLK] = &gcc_cpuss_gnoc_clk.clkr,
	[GCC_CPUSS_RBCPR_CLK] = &gcc_cpuss_rbcpr_clk.clkr,
	[GCC_CPUSS_RBCPR_CLK_SRC] = &gcc_cpuss_rbcpr_clk_src.clkr,
	[GCC_EMAC_CLK_SRC] = &gcc_emac_clk_src.clkr,
	[GCC_EMAC_PTP_CLK_SRC] = &gcc_emac_ptp_clk_src.clkr,
	[GCC_ETH_AXI_CLK] = &gcc_eth_axi_clk.clkr,
	[GCC_ETH_PTP_CLK] = &gcc_eth_ptp_clk.clkr,
	[GCC_ETH_RGMII_CLK] = &gcc_eth_rgmii_clk.clkr,
	[GCC_ETH_SLAVE_AHB_CLK] = &gcc_eth_slave_ahb_clk.clkr,
	[GCC_GP1_CLK] = &gcc_gp1_clk.clkr,
	[GCC_GP1_CLK_SRC] = &gcc_gp1_clk_src.clkr,
	[GCC_GP2_CLK] = &gcc_gp2_clk.clkr,
	[GCC_GP2_CLK_SRC] = &gcc_gp2_clk_src.clkr,
	[GCC_GP3_CLK] = &gcc_gp3_clk.clkr,
	[GCC_GP3_CLK_SRC] = &gcc_gp3_clk_src.clkr,
	[GCC_PCIE_0_CLKREF_CLK] = &gcc_pcie_0_clkref_clk.clkr,
	[GCC_PCIE_AUX_CLK] = &gcc_pcie_aux_clk.clkr,
	[GCC_PCIE_AUX_PHY_CLK_SRC] = &gcc_pcie_aux_phy_clk_src.clkr,
	[GCC_PCIE_CFG_AHB_CLK] = &gcc_pcie_cfg_ahb_clk.clkr,
	[GCC_PCIE_MSTR_AXI_CLK] = &gcc_pcie_mstr_axi_clk.clkr,
	[GCC_PCIE_PHY_REFGEN_CLK] = &gcc_pcie_phy_refgen_clk.clkr,
	[GCC_PCIE_PHY_REFGEN_CLK_SRC] = &gcc_pcie_phy_refgen_clk_src.clkr,
	[GCC_PCIE_PIPE_CLK] = &gcc_pcie_pipe_clk.clkr,
	[GCC_PCIE_SLEEP_CLK] = &gcc_pcie_sleep_clk.clkr,
	[GCC_PCIE_SLV_AXI_CLK] = &gcc_pcie_slv_axi_clk.clkr,
	[GCC_PCIE_SLV_Q2A_AXI_CLK] = &gcc_pcie_slv_q2a_axi_clk.clkr,
	[GCC_PDM2_CLK] = &gcc_pdm2_clk.clkr,
	[GCC_PDM2_CLK_SRC] = &gcc_pdm2_clk_src.clkr,
	[GCC_PDM_AHB_CLK] = &gcc_pdm_ahb_clk.clkr,
	[GCC_PDM_XO4_CLK] = &gcc_pdm_xo4_clk.clkr,
	[GCC_PRNG_AHB_CLK] = &gcc_prng_ahb_clk.clkr,
	[GCC_SDCC1_AHB_CLK] = &gcc_sdcc1_ahb_clk.clkr,
	[GCC_SDCC1_APPS_CLK] = &gcc_sdcc1_apps_clk.clkr,
	[GCC_SDCC1_APPS_CLK_SRC] = &gcc_sdcc1_apps_clk_src.clkr,
	[GCC_SPMI_FETCHER_AHB_CLK] = &gcc_spmi_fetcher_ahb_clk.clkr,
	[GCC_SPMI_FETCHER_CLK] = &gcc_spmi_fetcher_clk.clkr,
	[GCC_SPMI_FETCHER_CLK_SRC] = &gcc_spmi_fetcher_clk_src.clkr,
	[GCC_SYS_NOC_CPUSS_AHB_CLK] = &gcc_sys_noc_cpuss_ahb_clk.clkr,
	[GCC_SYS_NOC_USB3_CLK] = &gcc_sys_noc_usb3_clk.clkr,
	[GCC_USB30_MASTER_CLK] = &gcc_usb30_master_clk.clkr,
	[GCC_USB30_MASTER_CLK_SRC] = &gcc_usb30_master_clk_src.clkr,
	[GCC_USB30_MOCK_UTMI_CLK] = &gcc_usb30_mock_utmi_clk.clkr,
	[GCC_USB30_MOCK_UTMI_CLK_SRC] = &gcc_usb30_mock_utmi_clk_src.clkr,
	[GCC_USB30_SLEEP_CLK] = &gcc_usb30_sleep_clk.clkr,
	[GCC_USB3_PHY_AUX_CLK] = &gcc_usb3_phy_aux_clk.clkr,
	[GCC_USB3_PHY_AUX_CLK_SRC] = &gcc_usb3_phy_aux_clk_src.clkr,
	[GCC_USB3_PHY_PIPE_CLK] = &gcc_usb3_phy_pipe_clk.clkr,
	[GCC_USB3_PRIM_CLKREF_CLK] = &gcc_usb3_prim_clkref_clk.clkr,
	[GCC_USB_PHY_CFG_AHB2PHY_CLK] = &gcc_usb_phy_cfg_ahb2phy_clk.clkr,
	[GPLL0] = &gpll0.clkr,
	[GPLL0_OUT_EVEN] = &gpll0_out_even.clkr,
	[GPLL4] = &gpll4.clkr,
	[GPLL4_OUT_EVEN] = &gpll4_out_even.clkr,
};

static const struct qcom_reset_map gcc_sdxpoorwills_resets[] = {
	[GCC_BLSP1_QUP1_BCR] = { 0x11000 },
	[GCC_BLSP1_QUP2_BCR] = { 0x13000 },
	[GCC_BLSP1_QUP3_BCR] = { 0x15000 },
	[GCC_BLSP1_QUP4_BCR] = { 0x17000 },
	[GCC_BLSP1_UART2_BCR] = { 0x14000 },
	[GCC_BLSP1_UART3_BCR] = { 0x16000 },
	[GCC_BLSP1_UART4_BCR] = { 0x18000 },
	[GCC_CE1_BCR] = { 0x21000 },
	[GCC_EMAC_BCR] = { 0x47000 },
	[GCC_PCIE_BCR] = { 0x37000 },
	[GCC_PCIE_PHY_BCR] = { 0x39000 },
	[GCC_PDM_BCR] = { 0x19000 },
	[GCC_PRNG_BCR] = { 0x1a000 },
	[GCC_SDCC1_BCR] = { 0xf000 },
	[GCC_SPMI_FETCHER_BCR] = { 0x3f000 },
	[GCC_USB30_BCR] = { 0xb000 },
	[GCC_USB3_PHY_BCR] = { 0xc000 },
	[GCC_USB3PHY_PHY_BCR] = { 0xc004 },
	[GCC_QUSB2PHY_BCR] = { 0xd000 },
	[GCC_USB_PHY_CFG_AHB2PHY_BCR] = { 0xe000 },
};


static const struct regmap_config gcc_sdxpoorwills_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x9b040,
	.fast_io	= true,
};

static const struct qcom_cc_desc gcc_sdxpoorwills_desc = {
	.config = &gcc_sdxpoorwills_regmap_config,
	.clks = gcc_sdxpoorwills_clocks,
	.num_clks = ARRAY_SIZE(gcc_sdxpoorwills_clocks),
	.resets = gcc_sdxpoorwills_resets,
	.num_resets = ARRAY_SIZE(gcc_sdxpoorwills_resets),
};

static const struct of_device_id gcc_sdxpoorwills_match_table[] = {
	{ .compatible = "qcom,gcc-sdxpoorwills" },
	{ }
};
MODULE_DEVICE_TABLE(of, gcc_sdxpoorwills_match_table);

static int gcc_sdxpoorwills_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct clk *clk;
	struct regmap *regmap;

	regmap = qcom_cc_map(pdev, &gcc_sdxpoorwills_desc);
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

	/* Register the dummy measurement clocks */
	for (i = 0; i < ARRAY_SIZE(gcc_sdxpoorwills_hws); i++) {
		clk = devm_clk_register(&pdev->dev, gcc_sdxpoorwills_hws[i]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
	}

	ret = qcom_cc_really_probe(pdev, &gcc_sdxpoorwills_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GCC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered GCC clocks\n");

	return ret;
}

static struct platform_driver gcc_sdxpoorwills_driver = {
	.probe		= gcc_sdxpoorwills_probe,
	.driver		= {
		.name	= "gcc-sdxpoorwills",
		.of_match_table = gcc_sdxpoorwills_match_table,
	},
};

static int __init gcc_sdxpoorwills_init(void)
{
	return platform_driver_register(&gcc_sdxpoorwills_driver);
}
subsys_initcall(gcc_sdxpoorwills_init);

static void __exit gcc_sdxpoorwills_exit(void)
{
	platform_driver_unregister(&gcc_sdxpoorwills_driver);
}
module_exit(gcc_sdxpoorwills_exit);

static int gcc_cpuss_ahb_clk_update_rate(void)
{
	clk_set_rate(gcc_cpuss_ahb_clk.clkr.hw.clk, 19200000);
	clk_set_rate(gcc_sys_noc_cpuss_ahb_clk.clkr.hw.clk, 19200000);

	return 0;
}
late_initcall(gcc_cpuss_ahb_clk_update_rate);

MODULE_DESCRIPTION("QTI GCC SDXPOORWILLS Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gcc-sdxpoorwills");

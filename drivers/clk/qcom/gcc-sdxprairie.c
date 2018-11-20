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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,gcc-sdxprairie.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "reset.h"
#include "clk-alpha-pll.h"
#include "vdd-level.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_cx_ao, VDD_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_CORE_BI_PLL_TEST_SE,
	P_GPLL0_OUT_EVEN,
	P_GPLL0_OUT_MAIN,
	P_GPLL4_OUT_EVEN,
	P_GPLL5_OUT_MAIN,
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
static const char * const gcc_parent_names_0_ao[] = {
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
	{ P_GPLL4_OUT_EVEN, 2 },
	{ P_GPLL5_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_2[] = {
	"bi_tcxo",
	"gpll0",
	"gpll4_out_even",
	"gpll5",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_SLEEP_CLK, 5 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_3[] = {
	"bi_tcxo",
	"gpll0",
	"sleep_clk",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_SLEEP_CLK, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_4[] = {
	"bi_tcxo",
	"sleep_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map gcc_parent_map_5[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL4_OUT_EVEN, 2 },
	{ P_GPLL0_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gcc_parent_names_5[] = {
	"bi_tcxo",
	"gpll0",
	"gpll4_out_even",
	"gpll0_out_even",
	"core_bi_pll_test_se",
};

static struct pll_vco lucid_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static struct clk_alpha_pll gpll0 = {
	.offset = 0x0,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.clkr = {
		.enable_reg = 0x6d000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_fixed_lucid_ops,
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

static const struct clk_div_table post_div_table_lucid_even[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{ }
};

static struct clk_alpha_pll_postdiv gpll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_lucid_even,
	.num_post_div = ARRAY_SIZE(post_div_table_lucid_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpll0_out_even",
		.parent_names = (const char *[]){ "gpll0" },
		.num_parents = 1,
		.ops = &clk_alpha_pll_postdiv_lucid_ops,
	},
};

static struct clk_alpha_pll gpll4 = {
	.offset = 0x76000,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.clkr = {
		.enable_reg = 0x6d000,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data){
			.name = "gpll4",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_fixed_lucid_ops,
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

static struct clk_alpha_pll_postdiv gpll4_out_even = {
	.offset = 0x76000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_lucid_even,
	.num_post_div = ARRAY_SIZE(post_div_table_lucid_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpll4_out_even",
		.parent_names = (const char *[]){ "gpll4" },
		.num_parents = 1,
		.ops = &clk_alpha_pll_postdiv_lucid_ops,
	},
};

static struct clk_alpha_pll gpll5 = {
	.offset = 0x74000,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.clkr = {
		.enable_reg = 0x6d000,
		.enable_mask = BIT(5),
		.hw.init = &(struct clk_init_data){
			.name = "gpll5",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_fixed_lucid_ops,
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 50000000},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 25000000,
			[VDD_NOMINAL] = 50000000},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 50000000},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 25000000,
			[VDD_NOMINAL] = 50000000},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 50000000},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 25000000,
			[VDD_NOMINAL] = 50000000},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 50000000},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 25000000,
			[VDD_NOMINAL] = 50000000},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 48000000,
			[VDD_NOMINAL] = 63157895},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 48000000,
			[VDD_NOMINAL] = 63157895},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 48000000,
			[VDD_NOMINAL] = 63157895},
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 48000000,
			[VDD_NOMINAL] = 63157895},
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
		.parent_names = gcc_parent_names_0_ao,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx_ao,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000},
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
		.parent_names = gcc_parent_names_0_ao,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx_ao,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000},
	},
};

static const struct freq_tbl ftbl_gcc_emac_clk_src[] = {
	F(2500000, P_BI_TCXO, 1, 25, 192),
	F(5000000, P_BI_TCXO, 1, 25, 96),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(25000000, P_GPLL0_OUT_EVEN, 12, 0, 0),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(250000000, P_GPLL4_OUT_EVEN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_emac_clk_src = {
	.cmd_rcgr = 0x47020,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_5,
	.freq_tbl = ftbl_gcc_emac_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_emac_clk_src",
		.parent_names = gcc_parent_names_5,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOWER] = 50000000,
			[VDD_LOW] = 250000000},
	},
};

static const struct freq_tbl ftbl_gcc_emac_ptp_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(50000000, P_GPLL0_OUT_EVEN, 6, 0, 0),
	F(230400000, P_GPLL5_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_emac_ptp_clk_src = {
	.cmd_rcgr = 0x47038,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_2,
	.freq_tbl = ftbl_gcc_emac_ptp_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_emac_ptp_clk_src",
		.parent_names = gcc_parent_names_2,
		.num_parents = 6,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOWER] = 50000000,
			[VDD_LOW] = 230400000},
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
	.parent_map = gcc_parent_map_3,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_gp1_clk_src",
		.parent_names = gcc_parent_names_3,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOWER] = 50000000,
			[VDD_LOW] = 100000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static struct clk_rcg2 gcc_gp2_clk_src = {
	.cmd_rcgr = 0x2c004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_3,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_gp2_clk_src",
		.parent_names = gcc_parent_names_3,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOWER] = 50000000,
			[VDD_LOW] = 100000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static struct clk_rcg2 gcc_gp3_clk_src = {
	.cmd_rcgr = 0x2d004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_3,
	.freq_tbl = ftbl_gcc_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_gp3_clk_src",
		.parent_names = gcc_parent_names_3,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOWER] = 50000000,
			[VDD_LOW] = 100000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static struct clk_rcg2 gcc_pcie_aux_phy_clk_src = {
	.cmd_rcgr = 0x37034,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_4,
	.freq_tbl = ftbl_gcc_cpuss_rbcpr_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_pcie_aux_phy_clk_src",
		.parent_names = gcc_parent_names_4,
		.num_parents = 3,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000},
	},
};

static const struct freq_tbl ftbl_gcc_pcie_rchng_phy_clk_src[] = {
	F(100000000, P_GPLL0_OUT_EVEN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_pcie_rchng_phy_clk_src = {
	.cmd_rcgr = 0x37050,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_3,
	.freq_tbl = ftbl_gcc_pcie_rchng_phy_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_pcie_rchng_phy_clk_src",
		.parent_names = gcc_parent_names_3,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 100000000},
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
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 9600000,
			[VDD_LOWER] = 19200000,
			[VDD_LOW] = 60000000},
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
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000,
			[VDD_LOWER] = 50000000,
			[VDD_LOW] = 100000000,
			[VDD_NOMINAL] = 200000000},
	},
};

static const struct freq_tbl ftbl_gcc_usb30_master_clk_src[] = {
	F(200000000, P_GPLL0_OUT_EVEN, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_usb30_master_clk_src = {
	.cmd_rcgr = 0xb024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_usb30_master_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb30_master_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 200000000},
	},
};

static const struct freq_tbl ftbl_gcc_usb30_mock_utmi_clk_src[] = {
	F(60000000, P_GPLL0_OUT_EVEN, 5, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_usb30_mock_utmi_clk_src = {
	.cmd_rcgr = 0xb03c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_0,
	.freq_tbl = ftbl_gcc_usb30_mock_utmi_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb30_mock_utmi_clk_src",
		.parent_names = gcc_parent_names_0,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 60000000},
	},
};

static const struct freq_tbl ftbl_gcc_usb3_phy_aux_clk_src[] = {
	F(1000000, P_BI_TCXO, 1, 5, 96),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gcc_usb3_phy_aux_clk_src = {
	.cmd_rcgr = 0xb064,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_parent_map_4,
	.freq_tbl = ftbl_gcc_usb3_phy_aux_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gcc_usb3_phy_aux_clk_src",
		.parent_names = gcc_parent_names_4,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 19200000},
	},
};

static struct clk_branch gcc_ahb_pcie_link_clk = {
	.halt_reg = 0x22004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x22004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_ahb_pcie_link_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_ahb_clk = {
	.halt_reg = 0x10004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d008,
		.enable_mask = BIT(14),
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
		.enable_reg = 0x6d008,
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
		.enable_reg = 0x6d008,
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
		.enable_reg = 0x6d008,
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
		.enable_reg = 0x6d008,
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
		.enable_reg = 0x6d008,
		.enable_mask = BIT(21),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_cpuss_ahb_clk",
			.parent_names = (const char *[]){
				"gcc_cpuss_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
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
		.enable_reg = 0x6d008,
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
	.halt_reg = 0x37024,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d010,
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
	.clkr = {
		.enable_reg = 0x6d010,
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
		.enable_reg = 0x6d010,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_mstr_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_pipe_clk = {
	.halt_reg = 0x3702c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d010,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_pipe_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_rchng_phy_clk = {
	.halt_reg = 0x37020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d010,
		.enable_mask = BIT(7),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_pcie_rchng_phy_clk",
			.parent_names = (const char *[]){
				"gcc_pcie_rchng_phy_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pcie_sleep_clk = {
	.halt_reg = 0x37028,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d010,
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
		.enable_reg = 0x6d010,
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
		.enable_reg = 0x6d010,
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

static struct clk_branch gcc_sys_noc_cpuss_ahb_clk = {
	.halt_reg = 0x4010,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x6d008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_sys_noc_cpuss_ahb_clk",
			.parent_names = (const char *[]){
				"gcc_cpuss_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
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
	.halt_reg = 0xb020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb020,
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

static struct clk_branch gcc_usb30_mstr_axi_clk = {
	.halt_reg = 0xb014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_mstr_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_sleep_clk = {
	.halt_reg = 0xb01c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb01c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_slv_ahb_clk = {
	.halt_reg = 0xb018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb30_slv_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb3_phy_aux_clk = {
	.halt_reg = 0xb058,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb058,
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

static struct clk_branch gcc_usb3_phy_pipe_clk = {
	.halt_reg = 0xb05c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb05c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_usb3_phy_pipe_clk",
			.ops = &clk_branch2_ops,
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

static struct clk_branch gcc_xo_pcie_link_clk = {
	.halt_reg = 0x22008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x22008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gcc_xo_pcie_link_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

/* Measure-only clock for ddrss_gcc_debug_clk. */
static struct clk_dummy measure_only_bimc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_bimc_clk",
		.ops = &clk_dummy_ops,
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

/* Measure-only clock for gcc_sys_noc_axi_clk. */
static struct clk_dummy measure_only_snoc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_snoc_clk",
		.ops = &clk_dummy_ops,
	},
};

struct clk_hw *gcc_sdxprairie_hws[] = {
	[MEASURE_ONLY_BIMC_CLK] = &measure_only_bimc_clk.hw,
	[MEASURE_ONLY_IPA_2X_CLK] = &measure_only_ipa_2x_clk.hw,
	[MEASURE_ONLY_SNOC_CLK] = &measure_only_snoc_clk.hw,
};

static struct clk_regmap *gcc_sdxprairie_clocks[] = {
	[GCC_AHB_PCIE_LINK_CLK] = &gcc_ahb_pcie_link_clk.clkr,
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
	[GCC_PCIE_PIPE_CLK] = &gcc_pcie_pipe_clk.clkr,
	[GCC_PCIE_RCHNG_PHY_CLK] = &gcc_pcie_rchng_phy_clk.clkr,
	[GCC_PCIE_RCHNG_PHY_CLK_SRC] = &gcc_pcie_rchng_phy_clk_src.clkr,
	[GCC_PCIE_SLEEP_CLK] = &gcc_pcie_sleep_clk.clkr,
	[GCC_PCIE_SLV_AXI_CLK] = &gcc_pcie_slv_axi_clk.clkr,
	[GCC_PCIE_SLV_Q2A_AXI_CLK] = &gcc_pcie_slv_q2a_axi_clk.clkr,
	[GCC_PDM2_CLK] = &gcc_pdm2_clk.clkr,
	[GCC_PDM2_CLK_SRC] = &gcc_pdm2_clk_src.clkr,
	[GCC_PDM_AHB_CLK] = &gcc_pdm_ahb_clk.clkr,
	[GCC_PDM_XO4_CLK] = &gcc_pdm_xo4_clk.clkr,
	[GCC_SDCC1_AHB_CLK] = &gcc_sdcc1_ahb_clk.clkr,
	[GCC_SDCC1_APPS_CLK] = &gcc_sdcc1_apps_clk.clkr,
	[GCC_SDCC1_APPS_CLK_SRC] = &gcc_sdcc1_apps_clk_src.clkr,
	[GCC_SYS_NOC_CPUSS_AHB_CLK] = &gcc_sys_noc_cpuss_ahb_clk.clkr,
	[GCC_USB30_MASTER_CLK] = &gcc_usb30_master_clk.clkr,
	[GCC_USB30_MASTER_CLK_SRC] = &gcc_usb30_master_clk_src.clkr,
	[GCC_USB30_MOCK_UTMI_CLK] = &gcc_usb30_mock_utmi_clk.clkr,
	[GCC_USB30_MOCK_UTMI_CLK_SRC] = &gcc_usb30_mock_utmi_clk_src.clkr,
	[GCC_USB30_MSTR_AXI_CLK] = &gcc_usb30_mstr_axi_clk.clkr,
	[GCC_USB30_SLEEP_CLK] = &gcc_usb30_sleep_clk.clkr,
	[GCC_USB30_SLV_AHB_CLK] = &gcc_usb30_slv_ahb_clk.clkr,
	[GCC_USB3_PHY_AUX_CLK] = &gcc_usb3_phy_aux_clk.clkr,
	[GCC_USB3_PHY_AUX_CLK_SRC] = &gcc_usb3_phy_aux_clk_src.clkr,
	[GCC_USB3_PHY_PIPE_CLK] = &gcc_usb3_phy_pipe_clk.clkr,
	[GCC_USB3_PRIM_CLKREF_CLK] = &gcc_usb3_prim_clkref_clk.clkr,
	[GCC_USB_PHY_CFG_AHB2PHY_CLK] = &gcc_usb_phy_cfg_ahb2phy_clk.clkr,
	[GCC_XO_PCIE_LINK_CLK] = &gcc_xo_pcie_link_clk.clkr,
	[GPLL0] = &gpll0.clkr,
	[GPLL0_OUT_EVEN] = &gpll0_out_even.clkr,
	[GPLL4] = &gpll4.clkr,
	[GPLL4_OUT_EVEN] = &gpll4_out_even.clkr,
	[GPLL5] = &gpll5.clkr,
};

static const struct qcom_reset_map gcc_sdxprairie_resets[] = {
	[GCC_EMAC_BCR] = { 0x47000 },
	[GCC_PCIE_BCR] = { 0x37000 },
	[GCC_PCIE_LINK_DOWN_BCR] = { 0x77000 },
	[GCC_PCIE_PHY_BCR] = { 0x39000 },
	[GCC_PCIE_PHY_COM_BCR] = { 0x78004 },
	[GCC_QUSB2PHY_BCR] = { 0xd000 },
	[GCC_USB30_BCR] = { 0xb000 },
	[GCC_USB3_PHY_BCR] = { 0xc000 },
	[GCC_USB3PHY_PHY_BCR] = { 0xc004 },
	[GCC_USB_PHY_CFG_AHB2PHY_BCR] = { 0xe000 },
};

static const struct regmap_config gcc_sdxprairie_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x9b040,
	.fast_io	= true,
};

static const struct qcom_cc_desc gcc_sdxprairie_desc = {
	.config = &gcc_sdxprairie_regmap_config,
	.clks = gcc_sdxprairie_clocks,
	.num_clks = ARRAY_SIZE(gcc_sdxprairie_clocks),
	.hwclks = gcc_sdxprairie_hws,
	.num_hwclks = ARRAY_SIZE(gcc_sdxprairie_hws),
	.resets = gcc_sdxprairie_resets,
	.num_resets = ARRAY_SIZE(gcc_sdxprairie_resets),
};

static const struct of_device_id gcc_sdxprairie_match_table[] = {
	{ .compatible = "qcom,gcc-sdxprairie" },
	{ }
};
MODULE_DEVICE_TABLE(of, gcc_sdxprairie_match_table);

static int gcc_sdxprairie_probe(struct platform_device *pdev)
{
	struct clk *clk;
	struct device *dev = &pdev->dev;
	int ret = 0;

	clk = devm_clk_get(dev, "bi_tcxo");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get cxo clock\n");
		return PTR_ERR(clk);
	}

	vdd_cx.regulator[0] = devm_regulator_get(dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(dev, "Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	vdd_cx_ao.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx_ao");
	if (IS_ERR(vdd_cx_ao.regulator[0])) {
		if (!(PTR_ERR(vdd_cx_ao.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_cx_ao regulator\n");
		return PTR_ERR(vdd_cx_ao.regulator[0]);
	}

	vdd_mx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(vdd_mx.regulator[0])) {
		if (!(PTR_ERR(vdd_mx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get vdd_mx regulator\n");
		return PTR_ERR(vdd_mx.regulator[0]);
	}

	ret = qcom_cc_probe(pdev, &gcc_sdxprairie_desc);
	if (ret)
		dev_err(&pdev->dev, "Failed to register GCC clocks\n");

	return ret;
}

static struct platform_driver gcc_sdxprairie_driver = {
	.probe = gcc_sdxprairie_probe,
	.driver = {
		.name = "gcc-sdxprairie",
		.of_match_table = gcc_sdxprairie_match_table,
	},
};

static int __init gcc_sdxprairie_init(void)
{
	return platform_driver_register(&gcc_sdxprairie_driver);
}
subsys_initcall(gcc_sdxprairie_init);

static void __exit gcc_sdxprairie_exit(void)
{
	platform_driver_unregister(&gcc_sdxprairie_driver);
}
module_exit(gcc_sdxprairie_exit);

MODULE_DESCRIPTION("QTI GCC SDXPRAIRIE Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gcc-sdxprairie");

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include <dt-bindings/clock/qcom,npucc-lito.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "common.h"
#include "reset.h"
#include "vdd-level-lito.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);

#define HM0_CRC_SID_FSM_CTRL		0x11A0
#define CRC_SID_FSM_CTRL_SETTING	0x800000
#define HM0_CRC_MND_CFG			0x11A4
#define CRC_MND_CFG_SETTING		0x15011

enum {
	P_BI_TCXO,
	P_CORE_BI_PLL_TEST_SE,
	P_GCC_NPU_GPLL0_CLK,
	P_GCC_NPU_GPLL0_DIV_CLK,
	P_NPU_CC_PLL0_OUT_EVEN,
	P_NPU_CC_PLL1_OUT_EVEN,
	P_NPU_Q6SS_PLL_OUT_MAIN,
	P_NPU_CC_CRC_DIV,
};

static const struct parent_map npu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_NPU_CC_PLL1_OUT_EVEN, 1 },
	{ P_NPU_CC_PLL0_OUT_EVEN, 2 },
	{ P_GCC_NPU_GPLL0_CLK, 4 },
	{ P_GCC_NPU_GPLL0_DIV_CLK, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const npu_cc_parent_names_0[] = {
	"bi_tcxo",
	"npu_cc_pll1_out_even",
	"npu_cc_pll0_out_even",
	"gcc_npu_gpll0_clk_src",
	"gcc_npu_gpll0_div_clk_src",
	"core_bi_pll_test_se",
};

static const struct parent_map npu_cc_parent_map_0_crc[] = {
	{ P_BI_TCXO, 0 },
	{ P_NPU_CC_PLL1_OUT_EVEN, 1 },
	{ P_NPU_CC_CRC_DIV, 2 },
	{ P_GCC_NPU_GPLL0_CLK, 4 },
	{ P_GCC_NPU_GPLL0_DIV_CLK, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const npu_cc_parent_names_0_crc[] = {
	"bi_tcxo",
	"npu_cc_pll1_out_even",
	"npu_cc_crc_div",
	"gcc_npu_gpll0_clk_src",
	"gcc_npu_gpll0_div_clk_src",
	"core_bi_pll_test_se",
};

static const struct parent_map npu_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const npu_cc_parent_names_1[] = {
	"bi_tcxo",
	"core_bi_pll_test_se",
};

static const struct parent_map npu_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_NPU_Q6SS_PLL_OUT_MAIN, 1 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const npu_cc_parent_names_2[] = {
	"bi_tcxo",
	"npu_q6ss_pll",
	"core_bi_pll_test_se",
};

static struct pll_vco lucid_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static const u32 crc_reg_offset[] = {
	HM0_CRC_MND_CFG, HM0_CRC_SID_FSM_CTRL,
};

static const u32 crc_reg_val[] = {
	CRC_MND_CFG_SETTING, CRC_SID_FSM_CTRL_SETTING,
};

static const u32 no_crc_reg_val[] = {
	0x0, 0x0,
};

static struct alpha_pll_config npu_cc_pll0_config = {
	.l = 0x14,
	.cal_l = 0x49,
	.alpha = 0xD555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x2A9A699C,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
	.custom_reg_offset = crc_reg_offset,
	.custom_reg_val = crc_reg_val,
	.num_custom_reg = ARRAY_SIZE(crc_reg_offset),
};

static struct clk_alpha_pll npu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.config = &npu_cc_pll0_config,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_pll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ops,
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

static const struct clk_div_table post_div_table_npu_cc_pll0_out_even[] = {
	{ 0x0, 1 },
	{ }
};

static struct clk_alpha_pll_postdiv npu_cc_pll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_npu_cc_pll0_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_npu_cc_pll0_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_pll0_out_even",
		.parent_names = (const char *[]){ "npu_cc_pll0" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_lucid_ops,
	},
};

static struct alpha_pll_config npu_cc_pll1_config = {
	.l = 0x4E,
	.cal_l = 0x44,
	.alpha = 0x2000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
};

static struct clk_alpha_pll npu_cc_pll1 = {
	.offset = 0x400,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.config = &npu_cc_pll1_config,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_pll1",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ops,
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

static const struct clk_div_table post_div_table_npu_cc_pll1_out_even[] = {
	{ 0x0, 1 },
	{ }
};

static struct clk_alpha_pll_postdiv npu_cc_pll1_out_even = {
	.offset = 0x400,
	.post_div_shift = 8,
	.post_div_table = post_div_table_npu_cc_pll1_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_npu_cc_pll1_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_pll1_out_even",
		.parent_names = (const char *[]){ "npu_cc_pll1" },
		.num_parents = 1,
		.ops = &clk_alpha_pll_postdiv_lucid_ops,
	},
};

static struct alpha_pll_config npu_q6ss_pll_config = {
	.l = 0xD,
	.cal_l = 0x44,
	.alpha = 0x555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
};

static struct clk_alpha_pll npu_q6ss_pll = {
	.offset = 0x0,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.config = &npu_q6ss_pll_config,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "npu_q6ss_pll",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ops,
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

static struct clk_fixed_factor npu_cc_crc_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "npu_cc_crc_div",
		.parent_names = (const char *[]){ "npu_cc_pll0_out_even" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static const struct freq_tbl ftbl_npu_cc_cal_hm0_clk_src[] = {
	F(200000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(230000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(422000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(557000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(729000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(844000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(1000000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_npu_cc_cal_hm0_clk_src_no_crc[] = {
	F(200000000, P_NPU_CC_CRC_DIV, 2, 0, 0),
	F(230000000, P_NPU_CC_CRC_DIV, 2, 0, 0),
	F(422000000, P_NPU_CC_CRC_DIV, 2, 0, 0),
	F(557000000, P_NPU_CC_CRC_DIV, 2, 0, 0),
	F(729000000, P_NPU_CC_CRC_DIV, 2, 0, 0),
	F(844000000, P_NPU_CC_CRC_DIV, 2, 0, 0),
	F(1000000000, P_NPU_CC_CRC_DIV, 2, 0, 0),
	{ }
};

static struct clk_rcg2 npu_cc_cal_hm0_clk_src = {
	.cmd_rcgr = 0x1100,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_0_crc,
	.freq_tbl = ftbl_npu_cc_cal_hm0_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_cal_hm0_clk_src",
		.parent_names = npu_cc_parent_names_0_crc,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 200000000,
			[VDD_LOWER] = 230000000,
			[VDD_LOW] = 422000000,
			[VDD_LOW_L1] = 557000000,
			[VDD_NOMINAL] = 729000000,
			[VDD_HIGH] = 844000000,
			[VDD_HIGH_L1] = 1000000000},
	},
};

static const struct freq_tbl ftbl_npu_cc_core_clk_src[] = {
	F(60000000, P_GCC_NPU_GPLL0_DIV_CLK, 5, 0, 0),
	F(100000000, P_GCC_NPU_GPLL0_DIV_CLK, 3, 0, 0),
	F(200000000, P_GCC_NPU_GPLL0_CLK, 3, 0, 0),
	F(333333333, P_NPU_CC_PLL1_OUT_EVEN, 4.5, 0, 0),
	F(428571429, P_NPU_CC_PLL1_OUT_EVEN, 3.5, 0, 0),
	F(500000000, P_NPU_CC_PLL1_OUT_EVEN, 3, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_npu_cc_core_clk_src_v2[] = {
	F(60000000, P_GCC_NPU_GPLL0_DIV_CLK, 5, 0, 0),
	F(100000000, P_GCC_NPU_GPLL0_DIV_CLK, 3, 0, 0),
	F(200000000, P_GCC_NPU_GPLL0_CLK, 3, 0, 0),
	F(333333333, P_NPU_CC_PLL1_OUT_EVEN, 3, 0, 0),
	F(400000000, P_NPU_CC_PLL1_OUT_EVEN, 2.5, 0, 0),
	F(500000000, P_NPU_CC_PLL1_OUT_EVEN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 npu_cc_core_clk_src = {
	.cmd_rcgr = 0x1010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_0,
	.freq_tbl = ftbl_npu_cc_core_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_core_clk_src",
		.parent_names = npu_cc_parent_names_0,
		.num_parents = 6,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 60000000,
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 200000000,
			[VDD_LOW_L1] = 333333333,
			[VDD_NOMINAL] = 428571429,
			[VDD_HIGH] = 500000000},
	},
};

static const struct freq_tbl ftbl_npu_cc_lmh_clk_src[] = {
	F(60000000, P_GCC_NPU_GPLL0_DIV_CLK, 5, 0, 0),
	F(100000000, P_GCC_NPU_GPLL0_DIV_CLK, 3, 0, 0),
	F(200000000, P_GCC_NPU_GPLL0_CLK, 3, 0, 0),
	F(214285714, P_NPU_CC_PLL1_OUT_EVEN, 7, 0, 0),
	F(300000000, P_NPU_CC_PLL1_OUT_EVEN, 5, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_npu_cc_lmh_clk_src_v2[] = {
	F(60000000, P_GCC_NPU_GPLL0_DIV_CLK, 5, 0, 0),
	F(100000000, P_GCC_NPU_GPLL0_DIV_CLK, 3, 0, 0),
	F(200000000, P_GCC_NPU_GPLL0_CLK, 3, 0, 0),
	F(285714286, P_NPU_CC_PLL1_OUT_EVEN, 3.5, 0, 0),
	{ }
};

static struct clk_rcg2 npu_cc_lmh_clk_src = {
	.cmd_rcgr = 0x1060,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_0,
	.freq_tbl = ftbl_npu_cc_lmh_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_lmh_clk_src",
		.parent_names = npu_cc_parent_names_0,
		.num_parents = 6,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 60000000,
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 200000000,
			[VDD_LOW_L1] = 214285714,
			[VDD_NOMINAL] = 300000000},
	},
};

static const struct freq_tbl ftbl_npu_cc_xo_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 npu_cc_xo_clk_src = {
	.cmd_rcgr = 0x1400,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_1,
	.freq_tbl = ftbl_npu_cc_xo_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_xo_clk_src",
		.parent_names = npu_cc_parent_names_1,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_npu_dsp_core_clk_src[] = {
	F(250000000, P_NPU_Q6SS_PLL_OUT_MAIN, 1, 0, 0),
	F(300000000, P_NPU_Q6SS_PLL_OUT_MAIN, 1, 0, 0),
	F(400000000, P_NPU_Q6SS_PLL_OUT_MAIN, 1, 0, 0),
	F(500000000, P_NPU_Q6SS_PLL_OUT_MAIN, 1, 0, 0),
	F(660000000, P_NPU_Q6SS_PLL_OUT_MAIN, 1, 0, 0),
	F(800000000, P_NPU_Q6SS_PLL_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 npu_dsp_core_clk_src = {
	.cmd_rcgr = 0x28,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_2,
	.freq_tbl = ftbl_npu_dsp_core_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_dsp_core_clk_src",
		.parent_names = npu_cc_parent_names_2,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 250000000,
			[VDD_LOWER] = 300000000,
			[VDD_LOW] = 400000000,
			[VDD_LOW_L1] = 500000000,
			[VDD_NOMINAL] = 660000000,
			[VDD_HIGH] = 800000000},
	},
};

static struct clk_branch npu_cc_bto_core_clk = {
	.halt_reg = 0x10dc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10dc,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_bto_core_clk",
			.parent_names = (const char *[]){
				"npu_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_bwmon_clk = {
	.halt_reg = 0x10d8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10d8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_bwmon_clk",
			.parent_names = (const char *[]){
				"npu_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm0_cdc_clk = {
	.halt_reg = 0x1098,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1098,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_cal_hm0_cdc_clk",
			.parent_names = (const char *[]){
				"npu_cc_cal_hm0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm0_clk = {
	.halt_reg = 0x1110,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1110,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_cal_hm0_clk",
			.parent_names = (const char *[]){
				"npu_cc_cal_hm0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm0_dpm_ip_clk = {
	.halt_reg = 0x109c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x109c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_cal_hm0_dpm_ip_clk",
			.parent_names = (const char *[]){
				"npu_cc_cal_hm0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm0_perf_cnt_clk = {
	.halt_reg = 0x10a0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10a0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_cal_hm0_perf_cnt_clk",
			.parent_names = (const char *[]){
				"npu_cc_cal_hm0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_core_clk = {
	.halt_reg = 0x1030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_core_clk",
			.parent_names = (const char *[]){
				"npu_cc_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dl_dpm_clk = {
	.halt_reg = 0x1238,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1238,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dl_dpm_clk",
			.parent_names = (const char *[]){
				"npu_cc_lmh_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dl_llm_clk = {
	.halt_reg = 0x1234,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1234,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dl_llm_clk",
			.parent_names = (const char *[]){
				"npu_cc_lmh_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dpm_clk = {
	.halt_reg = 0x107c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x107c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dpm_clk",
			.parent_names = (const char *[]){
				"npu_cc_lmh_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dpm_temp_clk = {
	.halt_reg = 0x10c4,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x10c4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dpm_temp_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dpm_xo_clk = {
	.halt_reg = 0x1094,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1094,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dpm_xo_clk",
			.parent_names = (const char *[]){
				"npu_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_ahbm_clk = {
	.halt_reg = 0x1214,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1214,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dsp_ahbm_clk",
			.parent_names = (const char *[]){
				"npu_cc_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_ahbs_clk = {
	.halt_reg = 0x1210,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x1210,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dsp_ahbs_clk",
			.parent_names = (const char *[]){
				"npu_cc_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_axi_clk = {
	.halt_reg = 0x121c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x121c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dsp_axi_clk",
			.parent_names = (const char *[]){
				"gcc_npu_noc_axi_clk"
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_bwmon_ahb_clk = {
	.halt_reg = 0x1218,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1218,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dsp_bwmon_ahb_clk",
			.parent_names = (const char *[]){
				"npu_cc_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_bwmon_clk = {
	.halt_reg = 0x1224,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1224,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_dsp_bwmon_clk",
			.parent_names = (const char *[]){
				"npu_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_isense_clk = {
	.halt_reg = 0x1078,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1078,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_isense_clk",
			.parent_names = (const char *[]){
				"npu_cc_lmh_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_llm_clk = {
	.halt_reg = 0x1074,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1074,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_llm_clk",
			.parent_names = (const char *[]){
				"npu_cc_lmh_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_llm_curr_clk = {
	.halt_reg = 0x10d4,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x10d4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_llm_curr_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_llm_temp_clk = {
	.halt_reg = 0x10c8,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x10c8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_llm_temp_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_llm_xo_clk = {
	.halt_reg = 0x1090,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1090,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_llm_xo_clk",
			.parent_names = (const char *[]){
				"npu_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_noc_ahb_clk = {
	.halt_reg = 0x10c0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10c0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_noc_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_noc_axi_clk = {
	.halt_reg = 0x10b8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10b8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_noc_axi_clk",
			.parent_names = (const char *[]){
				"gcc_npu_noc_axi_clk"
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_noc_dma_clk = {
	.halt_reg = 0x10b0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10b0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_noc_dma_clk",
			.parent_names = (const char *[]){
				"gcc_npu_noc_dma_clk"
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_rsc_xo_clk = {
	.halt_reg = 0x10e0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10e0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_rsc_xo_clk",
			.parent_names = (const char *[]){
				"npu_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_s2p_clk = {
	.halt_reg = 0x10cc,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x10cc,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_s2p_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_xo_clk = {
	.halt_reg = 0x1410,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1410,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_xo_clk",
			.flags = CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *npu_cc_lito_clocks[] = {
	[NPU_CC_BTO_CORE_CLK] = &npu_cc_bto_core_clk.clkr,
	[NPU_CC_BWMON_CLK] = &npu_cc_bwmon_clk.clkr,
	[NPU_CC_CAL_HM0_CDC_CLK] = &npu_cc_cal_hm0_cdc_clk.clkr,
	[NPU_CC_CAL_HM0_CLK] = &npu_cc_cal_hm0_clk.clkr,
	[NPU_CC_CAL_HM0_CLK_SRC] = &npu_cc_cal_hm0_clk_src.clkr,
	[NPU_CC_CAL_HM0_DPM_IP_CLK] = &npu_cc_cal_hm0_dpm_ip_clk.clkr,
	[NPU_CC_CAL_HM0_PERF_CNT_CLK] = &npu_cc_cal_hm0_perf_cnt_clk.clkr,
	[NPU_CC_CORE_CLK] = &npu_cc_core_clk.clkr,
	[NPU_CC_CORE_CLK_SRC] = &npu_cc_core_clk_src.clkr,
	[NPU_CC_DL_DPM_CLK] = &npu_cc_dl_dpm_clk.clkr,
	[NPU_CC_DL_LLM_CLK] = &npu_cc_dl_llm_clk.clkr,
	[NPU_CC_DPM_CLK] = &npu_cc_dpm_clk.clkr,
	[NPU_CC_DPM_TEMP_CLK] = &npu_cc_dpm_temp_clk.clkr,
	[NPU_CC_DPM_XO_CLK] = &npu_cc_dpm_xo_clk.clkr,
	[NPU_CC_DSP_AHBM_CLK] = &npu_cc_dsp_ahbm_clk.clkr,
	[NPU_CC_DSP_AHBS_CLK] = &npu_cc_dsp_ahbs_clk.clkr,
	[NPU_CC_DSP_AXI_CLK] = &npu_cc_dsp_axi_clk.clkr,
	[NPU_CC_DSP_BWMON_AHB_CLK] = &npu_cc_dsp_bwmon_ahb_clk.clkr,
	[NPU_CC_DSP_BWMON_CLK] = &npu_cc_dsp_bwmon_clk.clkr,
	[NPU_CC_ISENSE_CLK] = &npu_cc_isense_clk.clkr,
	[NPU_CC_LLM_CLK] = &npu_cc_llm_clk.clkr,
	[NPU_CC_LLM_CURR_CLK] = &npu_cc_llm_curr_clk.clkr,
	[NPU_CC_LLM_TEMP_CLK] = &npu_cc_llm_temp_clk.clkr,
	[NPU_CC_LLM_XO_CLK] = &npu_cc_llm_xo_clk.clkr,
	[NPU_CC_LMH_CLK_SRC] = &npu_cc_lmh_clk_src.clkr,
	[NPU_CC_NOC_AHB_CLK] = &npu_cc_noc_ahb_clk.clkr,
	[NPU_CC_NOC_AXI_CLK] = &npu_cc_noc_axi_clk.clkr,
	[NPU_CC_NOC_DMA_CLK] = &npu_cc_noc_dma_clk.clkr,
	[NPU_CC_PLL0] = &npu_cc_pll0.clkr,
	[NPU_CC_PLL0_OUT_EVEN] = &npu_cc_pll0_out_even.clkr,
	[NPU_CC_PLL1] = &npu_cc_pll1.clkr,
	[NPU_CC_PLL1_OUT_EVEN] = &npu_cc_pll1_out_even.clkr,
	[NPU_CC_RSC_XO_CLK] = &npu_cc_rsc_xo_clk.clkr,
	[NPU_CC_S2P_CLK] = &npu_cc_s2p_clk.clkr,
	[NPU_CC_XO_CLK] = &npu_cc_xo_clk.clkr,
	[NPU_CC_XO_CLK_SRC] = &npu_cc_xo_clk_src.clkr,
};

static struct clk_regmap *npu_qdsp6ss_lito_clocks[] = {
	[NPU_DSP_CORE_CLK_SRC] = &npu_dsp_core_clk_src.clkr,
};

static struct clk_regmap *npu_qdsp6ss_pll_lito_clocks[] = {
	[NPU_Q6SS_PLL] = &npu_q6ss_pll.clkr,
};

static const struct qcom_reset_map npu_cc_lito_resets[] = {
	[NPU_CC_CAL_HM0_BCR] = { 0x10f0 },
	[NPU_CC_CAL_HM1_BCR] = { 0x1130 },
	[NPU_CC_CORE_BCR] = { 0x1000 },
	[NPU_CC_DPM_TEMP_CLK_ARES] = { 0x10c4, 2 },
	[NPU_CC_LLM_TEMP_CLK_ARES] = { 0x10c8, 2 },
	[NPU_CC_LLM_CURR_CLK_ARES] = { 0x10d4, 2 },
	[NPU_CC_DSP_BCR] = { 0x1200 },
};

static const struct regmap_config npu_cc_lito_regmap_config = {
	.name = "cc",
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xa060,
	.fast_io = true,
};

static const struct regmap_config npu_qdsp6ss_lito_regmap_config = {
	.name = "qdsp6ss",
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x203c,
	.fast_io = true,
};

static const struct regmap_config npu_qdsp6ss_pll_lito_regmap_config = {
	.name = "qdsp6ss_pll",
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x50,
	.fast_io = true,
};

static const struct qcom_cc_desc npu_cc_lito_desc = {
	.config = &npu_cc_lito_regmap_config,
	.clks = npu_cc_lito_clocks,
	.num_clks = ARRAY_SIZE(npu_cc_lito_clocks),
	.resets = npu_cc_lito_resets,
	.num_resets = ARRAY_SIZE(npu_cc_lito_resets),
};

static const struct qcom_cc_desc npu_qdsp6ss_lito_desc = {
	.config = &npu_qdsp6ss_lito_regmap_config,
	.clks = npu_qdsp6ss_lito_clocks,
	.num_clks = ARRAY_SIZE(npu_qdsp6ss_lito_clocks),
};

static const struct qcom_cc_desc npu_qdsp6ss_pll_lito_desc = {
	.config = &npu_qdsp6ss_pll_lito_regmap_config,
	.clks = npu_qdsp6ss_pll_lito_clocks,
	.num_clks = ARRAY_SIZE(npu_qdsp6ss_pll_lito_clocks),
};

static const struct of_device_id npu_cc_lito_match_table[] = {
	{ .compatible = "qcom,lito-npucc" },
	{ .compatible = "qcom,lito-npucc-v2" },
	{ }
};
MODULE_DEVICE_TABLE(of, npu_cc_lito_match_table);

static int npu_cc_lito_fixup(struct platform_device *pdev)
{
	u32 val;
	int ret;

	ret = nvmem_cell_read_u32(&pdev->dev, "npu-bin", &val);
	if (ret)
		return ret;

	val = val & GENMASK(17, 10);
	if (val) {
		npu_cc_pll0_config.custom_reg_val = no_crc_reg_val;
		npu_cc_crc_div.div = 1;
		npu_cc_cal_hm0_clk_src.freq_tbl =
					ftbl_npu_cc_cal_hm0_clk_src_no_crc;
	}

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,lito-npucc-v2")) {
		npu_cc_pll1_config.l = 0x34;
		npu_cc_pll1_config.alpha = 0x1555;

		npu_cc_lmh_clk_src.freq_tbl = ftbl_npu_cc_lmh_clk_src_v2;
		npu_cc_lmh_clk_src.clkr.hw.init->rate_max[VDD_LOW_L1] =
								200000000;
		npu_cc_lmh_clk_src.clkr.hw.init->rate_max[VDD_NOMINAL] =
								285714286;

		npu_cc_core_clk_src.freq_tbl = ftbl_npu_cc_core_clk_src_v2;
		npu_cc_core_clk_src.clkr.hw.init->rate_max[VDD_NOMINAL] =
								400000000;
	}

	return 0;
}

static int npu_clocks_lito_probe(struct platform_device *pdev,
					const struct qcom_cc_desc *desc)
{
	struct regmap *regmap;
	struct resource *res;
	void __iomem *base;
	int ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							desc->config->name);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	regmap = devm_regmap_init_mmio(&pdev->dev, base, desc->config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	if (!strcmp("cc", desc->config->name)) {
		clk_lucid_pll_configure(&npu_cc_pll0, regmap,
					&npu_cc_pll0_config);
		clk_lucid_pll_configure(&npu_cc_pll1, regmap,
					&npu_cc_pll1_config);

		/* Register the fixed factor clock for CRC divider */
		ret = devm_clk_hw_register(&pdev->dev, &npu_cc_crc_div.hw);
		if (ret) {
			dev_err(&pdev->dev, "Failed to register CRC divider clock, ret=%d\n",
									ret);
			return ret;
		}
	} else if (!strcmp("qdsp6ss_pll", desc->config->name)) {
		clk_lucid_pll_configure(&npu_q6ss_pll, regmap,
						&npu_q6ss_pll_config);
	}

	return qcom_cc_really_probe(pdev, desc, regmap);
}

static int npu_cc_lito_probe(struct platform_device *pdev)
{
	int ret;

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		ret = PTR_ERR(vdd_cx.regulator[0]);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_cx regulator, ret=%d\n",
									ret);
		return ret;
	}

	ret = npu_cc_lito_fixup(pdev);
	if (ret)
		return ret;

	ret = npu_clocks_lito_probe(pdev, &npu_cc_lito_desc);
	if (ret < 0) {
		dev_err(&pdev->dev, "npu_cc clock registration failed, ret=%d\n",
									ret);
		return ret;
	}

	ret = npu_clocks_lito_probe(pdev, &npu_qdsp6ss_lito_desc);
	if (ret < 0) {
		dev_err(&pdev->dev, "npu_qdsp6ss clock registration failed, ret=%d\n",
									ret);
		return ret;
	}

	ret = npu_clocks_lito_probe(pdev, &npu_qdsp6ss_pll_lito_desc);
	if (ret < 0) {
		dev_err(&pdev->dev, "npu_qdsp6ss_pll clock registration failed, ret=%d\n",
			ret);
		return ret;
	}

	dev_info(&pdev->dev, "Registered NPU_CC clocks\n");

	return 0;

}

static struct platform_driver npu_cc_lito_driver = {
	.probe = npu_cc_lito_probe,
	.driver = {
		.name = "lito_npucc",
		.of_match_table = npu_cc_lito_match_table,
	},
};

static int __init npu_cc_lito_init(void)
{
	return platform_driver_register(&npu_cc_lito_driver);
}
subsys_initcall(npu_cc_lito_init);

static void __exit npu_cc_lito_exit(void)
{
	platform_driver_unregister(&npu_cc_lito_driver);
}
module_exit(npu_cc_lito_exit);

MODULE_DESCRIPTION("QTI NPU_CC LITO Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:npu_cc-lito");

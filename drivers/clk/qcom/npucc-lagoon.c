// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,npucc-lagoon.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "common.h"
#include "reset.h"
#include "vdd-level-lagoon.h"

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
	"npu_cc_pll1",
	"npu_cc_pll0",
	"gcc_npu_gpll0_clk",
	"gcc_npu_gpll0_div_clk",
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
	"npu_cc_pll1",
	"npu_cc_crc_div",
	"gcc_npu_gpll0_clk",
	"gcc_npu_gpll0_div_clk",
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
	{ P_NPU_Q6SS_PLL_OUT_MAIN, 2 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const npu_cc_parent_names_2[] = {
	"bi_tcxo",
	"npu_q6ss_pll",
	"core_bi_pll_test_se",
};

static const u32 crc_reg_offset[] = {
	HM0_CRC_MND_CFG, HM0_CRC_SID_FSM_CTRL,
};

static const u32 crc_reg_val[] = {
	CRC_MND_CFG_SETTING, CRC_SID_FSM_CTRL_SETTING,
};

static struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
};

/* 537.60MHz Configuration */
static struct alpha_pll_config npu_cc_pll0_config = {
	.l = 0x1C,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002067,
	.test_ctl_val = 0x40000000,
	.test_ctl_hi_val = 0x00000002,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00004805,
	.custom_reg_offset = crc_reg_offset,
	.custom_reg_val = crc_reg_val,
	.num_custom_reg = ARRAY_SIZE(crc_reg_offset),
};

static struct clk_alpha_pll npu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_FABIA],
	.config = &npu_cc_pll0_config,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_pll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_fabia_ops,
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

/* 300MHz Configuration */
static struct alpha_pll_config npu_cc_pll1_config = {
	.l = 0xF,
	.alpha = 0xA000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002067,
	.test_ctl_val = 0x40000000,
	.test_ctl_hi_val = 0x00000002,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00004805,
};

static struct clk_alpha_pll npu_cc_pll1 = {
	.offset = 0x400,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_FABIA],
	.config = &npu_cc_pll1_config,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_pll1",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_fabia_ops,
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

/* 250MHz Configuration */
static struct alpha_pll_config npu_q6ss_pll_config = {
	.l = 0xD,
	.alpha = 0x555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002067,
	.test_ctl_val = 0x40000000,
	.test_ctl_hi_val = 0x00000002,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00004805,
};

static struct clk_alpha_pll npu_q6ss_pll = {
	.offset = 0x0,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_FABIA],
	.config = &npu_q6ss_pll_config,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "npu_q6ss_pll",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_fabia_ops,
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
		.parent_names = (const char *[]){ "npu_cc_pll0" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static const struct freq_tbl ftbl_npu_cc_cal_hm0_clk_src[] = {
	F(100000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(268800000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(403200000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(515000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(650000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(850000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
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
			[VDD_MIN] = 100000000,
			[VDD_LOWER] = 268800000,
			[VDD_LOW] = 403200000,
			[VDD_LOW_L1] = 515000000,
			[VDD_NOMINAL] = 650000000,
			[VDD_HIGH] = 850000000},
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
		.flags = CLK_SET_RATE_PARENT,
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

static struct clk_regmap *npu_cc_lagoon_clocks[] = {
	[NPU_CC_BTO_CORE_CLK] = &npu_cc_bto_core_clk.clkr,
	[NPU_CC_BWMON_CLK] = &npu_cc_bwmon_clk.clkr,
	[NPU_CC_CAL_HM0_CDC_CLK] = &npu_cc_cal_hm0_cdc_clk.clkr,
	[NPU_CC_CAL_HM0_CLK] = &npu_cc_cal_hm0_clk.clkr,
	[NPU_CC_CAL_HM0_CLK_SRC] = &npu_cc_cal_hm0_clk_src.clkr,
	[NPU_CC_CAL_HM0_PERF_CNT_CLK] = &npu_cc_cal_hm0_perf_cnt_clk.clkr,
	[NPU_CC_CORE_CLK] = &npu_cc_core_clk.clkr,
	[NPU_CC_CORE_CLK_SRC] = &npu_cc_core_clk_src.clkr,
	[NPU_CC_DSP_AHBM_CLK] = &npu_cc_dsp_ahbm_clk.clkr,
	[NPU_CC_DSP_AHBS_CLK] = &npu_cc_dsp_ahbs_clk.clkr,
	[NPU_CC_DSP_AXI_CLK] = &npu_cc_dsp_axi_clk.clkr,
	[NPU_CC_NOC_AHB_CLK] = &npu_cc_noc_ahb_clk.clkr,
	[NPU_CC_NOC_AXI_CLK] = &npu_cc_noc_axi_clk.clkr,
	[NPU_CC_NOC_DMA_CLK] = &npu_cc_noc_dma_clk.clkr,
	[NPU_CC_PLL0] = &npu_cc_pll0.clkr,
	[NPU_CC_PLL1] = &npu_cc_pll1.clkr,
	[NPU_CC_RSC_XO_CLK] = &npu_cc_rsc_xo_clk.clkr,
	[NPU_CC_S2P_CLK] = &npu_cc_s2p_clk.clkr,
	[NPU_CC_XO_CLK] = &npu_cc_xo_clk.clkr,
	[NPU_CC_XO_CLK_SRC] = &npu_cc_xo_clk_src.clkr,
};

static struct clk_regmap *npu_qdsp6ss_lagoon_clocks[] = {
	[NPU_DSP_CORE_CLK_SRC] = &npu_dsp_core_clk_src.clkr,
};

static struct clk_regmap *npu_qdsp6ss_pll_lagoon_clocks[] = {
	[NPU_Q6SS_PLL] = &npu_q6ss_pll.clkr,
};

static const struct qcom_reset_map npu_cc_lagoon_resets[] = {
	[NPU_CC_CAL_HM0_BCR] = { 0x10f0 },
	[NPU_CC_CORE_BCR] = { 0x1000 },
	[NPU_CC_DSP_BCR] = { 0x1200 },
};

static const struct regmap_config npu_cc_lagoon_regmap_config = {
	.name = "cc",
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xa060,
	.fast_io = true,
};

static const struct regmap_config npu_qdsp6ss_lagoon_regmap_config = {
	.name = "qdsp6ss",
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x203c,
	.fast_io = true,
};

static const struct regmap_config npu_qdsp6ss_pll_lagoon_regmap_config = {
	.name = "qdsp6ss_pll",
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x50,
	.fast_io = true,
};

static const struct qcom_cc_desc npu_cc_lagoon_desc = {
	.config = &npu_cc_lagoon_regmap_config,
	.clks = npu_cc_lagoon_clocks,
	.num_clks = ARRAY_SIZE(npu_cc_lagoon_clocks),
	.resets = npu_cc_lagoon_resets,
	.num_resets = ARRAY_SIZE(npu_cc_lagoon_resets),
};

static const struct qcom_cc_desc npu_qdsp6ss_lagoon_desc = {
	.config = &npu_qdsp6ss_lagoon_regmap_config,
	.clks = npu_qdsp6ss_lagoon_clocks,
	.num_clks = ARRAY_SIZE(npu_qdsp6ss_lagoon_clocks),
};

static const struct qcom_cc_desc npu_qdsp6ss_pll_lagoon_desc = {
	.config = &npu_qdsp6ss_pll_lagoon_regmap_config,
	.clks = npu_qdsp6ss_pll_lagoon_clocks,
	.num_clks = ARRAY_SIZE(npu_qdsp6ss_pll_lagoon_clocks),
};

static const struct of_device_id npu_cc_lagoon_match_table[] = {
	{ .compatible = "qcom,lagoon-npucc" },
	{ }
};
MODULE_DEVICE_TABLE(of, npu_cc_lagoon_match_table);

static int npu_clocks_lagoon_probe(struct platform_device *pdev,
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
		clk_fabia_pll_configure(&npu_cc_pll0, regmap,
					&npu_cc_pll0_config);
		clk_fabia_pll_configure(&npu_cc_pll1, regmap,
					&npu_cc_pll1_config);

		/* Register the fixed factor clock for CRC divider */
		ret = devm_clk_hw_register(&pdev->dev, &npu_cc_crc_div.hw);
		if (ret) {
			dev_err(&pdev->dev,
			"Failed to register CRC divider clock, ret=%d\n", ret);
			return ret;
		}
	} else if (!strcmp("qdsp6ss_pll", desc->config->name)) {
		clk_fabia_pll_configure(&npu_q6ss_pll, regmap,
						&npu_q6ss_pll_config);
	}

	ret = qcom_cc_really_probe(pdev, desc, regmap);
	if (ret)
		dev_err(&pdev->dev, "Failed to register NPU CC clocks\n");

	return ret;
}

static int npu_cc_lagoon_probe(struct platform_device *pdev)
{
	int ret;

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	ret = npu_clocks_lagoon_probe(pdev, &npu_cc_lagoon_desc);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"npu_cc clock registration failed, ret=%d\n", ret);
		return ret;
	}

	ret = npu_clocks_lagoon_probe(pdev, &npu_qdsp6ss_lagoon_desc);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"npu_qdsp6ss clock registration failed, ret=%d\n",
			ret);
		return ret;
	}

	ret = npu_clocks_lagoon_probe(pdev, &npu_qdsp6ss_pll_lagoon_desc);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"npu_qdsp6ss_pll clock registration failed, ret=%d\n",
			ret);
		return ret;
	}

	dev_info(&pdev->dev, "Registered NPU CC clocks\n");

	return ret;
}

static struct platform_driver npu_cc_lagoon_driver = {
	.probe = npu_cc_lagoon_probe,
	.driver = {
		.name = "lagoon-npucc",
		.of_match_table = npu_cc_lagoon_match_table,
	},
};

static int __init npu_cc_lagoon_init(void)
{
	return platform_driver_register(&npu_cc_lagoon_driver);
}
core_initcall(npu_cc_lagoon_init);

static void __exit npu_cc_lagoon_exit(void)
{
	platform_driver_unregister(&npu_cc_lagoon_driver);
}
module_exit(npu_cc_lagoon_exit);

MODULE_DESCRIPTION("QTI NPU_CC LAGOON Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:npu_cc-lagoon");

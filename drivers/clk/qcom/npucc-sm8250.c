// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,npucc-sm8250.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "clk-regmap-divider.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_HIGH_L1 + 1, 1, vdd_corner);

static struct clk_vdd_class *npu_cc_sm8250_regulators[] = {
	&vdd_cx,
};

enum {
	P_BI_TCXO,
	P_GCC_NPU_GPLL0_CLK,
	P_GCC_NPU_GPLL0_DIV_CLK,
	P_NPU_CC_PLL0_OUT_EVEN,
	P_NPU_CC_PLL1_OUT_EVEN,
	P_NPU_Q6SS_PLL_OUT_EVEN,
	P_NPU_Q6SS_PLL_OUT_MAIN,
};

static const struct pll_vco lucid_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static const struct alpha_pll_config npu_cc_pll0_config = {
	.l = 0x1F,
	.cal_l = 0x44,
	.alpha = 0x4000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll npu_cc_pll0 = {
	.offset = 0x180000,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr = {
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000,
				[VDD_HIGH] = 2000000000},
		},
	},
};

static const struct alpha_pll_config npu_cc_pll1_config = {
	.l = 0x4E,
	.cal_l = 0x44,
	.alpha = 0x2000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll npu_cc_pll1 = {
	.offset = 0x180400,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr = {
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_pll1",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000,
				[VDD_HIGH] = 2000000000},
		},
	},
};

static const struct alpha_pll_config npu_q6ss_pll_config = {
	.l = 0xF,
	.cal_l = 0x44,
	.alpha = 0xA000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll npu_q6ss_pll = {
	.offset = 0x10000,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr = {
		.hw.init = &(const struct clk_init_data){
			.name = "npu_q6ss_pll",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000,
				[VDD_HIGH] = 2000000000},
		},
	},
};

static const struct parent_map npu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_NPU_CC_PLL1_OUT_EVEN, 1 },
	{ P_NPU_CC_PLL0_OUT_EVEN, 2 },
	{ P_GCC_NPU_GPLL0_CLK, 4 },
	{ P_GCC_NPU_GPLL0_DIV_CLK, 5 },
};

static const struct clk_parent_data npu_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &npu_cc_pll1.clkr.hw },
	{ .hw = &npu_cc_pll0.clkr.hw },
	{ .fw_name = "gcc_npu_gpll0_clk" },
	{ .fw_name = "gcc_npu_gpll0_div_clk" },
};

static const struct parent_map npu_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
};

static const struct clk_parent_data npu_cc_parent_data_1_ao[] = {
	{ .fw_name = "bi_tcxo_ao" },
};

static const struct parent_map npu_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_NPU_Q6SS_PLL_OUT_EVEN, 1 },
	{ P_NPU_Q6SS_PLL_OUT_MAIN, 2 },
};

static const struct clk_parent_data npu_cc_parent_data_2[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &npu_q6ss_pll.clkr.hw },
	{ .hw = &npu_q6ss_pll.clkr.hw },
};

static const struct freq_tbl ftbl_npu_cc_cal_hm0_clk_src[] = {
	F(300000000, P_NPU_CC_PLL0_OUT_EVEN, 1, 0, 0),
	F(406000000, P_NPU_CC_PLL0_OUT_EVEN, 1, 0, 0),
	F(533000000, P_NPU_CC_PLL0_OUT_EVEN, 1, 0, 0),
	F(730000000, P_NPU_CC_PLL0_OUT_EVEN, 1, 0, 0),
	F(920000000, P_NPU_CC_PLL0_OUT_EVEN, 1, 0, 0),
	F(1000000000, P_NPU_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 npu_cc_cal_hm0_clk_src = {
	.cmd_rcgr = 0x181100,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_0,
	.freq_tbl = ftbl_npu_cc_cal_hm0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "npu_cc_cal_hm0_clk_src",
		.parent_data = npu_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(npu_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOW] = 406000000,
			[VDD_NOMINAL] = 730000000,
			[VDD_NOMINAL_L1] = 850000000,
			[VDD_HIGH] = 920000000,
			[VDD_HIGH_L1] = 1000000000},
	},
};

static struct clk_rcg2 npu_cc_cal_hm1_clk_src = {
	.cmd_rcgr = 0x181140,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_0,
	.freq_tbl = ftbl_npu_cc_cal_hm0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "npu_cc_cal_hm1_clk_src",
		.parent_data = npu_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(npu_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOW] = 406000000,
			[VDD_NOMINAL] = 730000000,
			[VDD_NOMINAL_L1] = 850000000,
			[VDD_HIGH] = 920000000,
			[VDD_HIGH_L1] = 1000000000},
	},
};

static const struct freq_tbl ftbl_npu_cc_core_clk_src[] = {
	F(100000000, P_GCC_NPU_GPLL0_DIV_CLK, 3, 0, 0),
	F(200000000, P_GCC_NPU_GPLL0_CLK, 3, 0, 0),
	F(333333333, P_NPU_CC_PLL1_OUT_EVEN, 4.5, 0, 0),
	F(428571429, P_NPU_CC_PLL1_OUT_EVEN, 3.5, 0, 0),
	F(500000000, P_NPU_CC_PLL1_OUT_EVEN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 npu_cc_core_clk_src = {
	.cmd_rcgr = 0x181010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_0,
	.freq_tbl = ftbl_npu_cc_core_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "npu_cc_core_clk_src",
		.parent_data = npu_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(npu_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 200000000,
			[VDD_LOW_L1] = 333333333,
			[VDD_NOMINAL] = 428571429,
			[VDD_HIGH] = 500000000},
	},
};

static const struct freq_tbl ftbl_npu_cc_lmh_clk_src[] = {
	F(100000000, P_GCC_NPU_GPLL0_DIV_CLK, 3, 0, 0),
	F(200000000, P_GCC_NPU_GPLL0_CLK, 3, 0, 0),
	F(214285714, P_NPU_CC_PLL1_OUT_EVEN, 7, 0, 0),
	F(300000000, P_NPU_CC_PLL1_OUT_EVEN, 5, 0, 0),
	{ }
};

static struct clk_rcg2 npu_cc_lmh_clk_src = {
	.cmd_rcgr = 0x181060,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_0,
	.freq_tbl = ftbl_npu_cc_lmh_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "npu_cc_lmh_clk_src",
		.parent_data = npu_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(npu_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
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
	.cmd_rcgr = 0x181400,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_1,
	.freq_tbl = ftbl_npu_cc_xo_clk_src,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "npu_cc_xo_clk_src",
		.parent_data = npu_cc_parent_data_1_ao,
		.num_parents = ARRAY_SIZE(npu_cc_parent_data_1_ao),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_npu_dsp_core_clk_src[] = {
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
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "npu_dsp_core_clk_src",
		.parent_data = npu_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(npu_cc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000,
			[VDD_LOW] = 400000000,
			[VDD_LOW_L1] = 500000000,
			[VDD_NOMINAL] = 660000000,
			[VDD_HIGH] = 800000000},
	},
};

static struct clk_branch npu_cc_atb_clk = {
	.halt_reg = 0x1810d0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810d0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_atb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_bto_core_clk = {
	.halt_reg = 0x1810dc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810dc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_bto_core_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_DONT_HOLD_STATE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_bwmon_clk = {
	.halt_reg = 0x1810d8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810d8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_bwmon_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm0_cdc_clk = {
	.halt_reg = 0x181098,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181098,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_cal_hm0_cdc_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_cal_hm0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm0_clk = {
	.halt_reg = 0x181110,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181110,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_cal_hm0_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_cal_hm0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm0_dpm_ip_clk = {
	.halt_reg = 0x18109c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x18109c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_cal_hm0_dpm_ip_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_cal_hm0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm0_perf_cnt_clk = {
	.halt_reg = 0x1810a0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810a0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_cal_hm0_perf_cnt_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_cal_hm0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm1_cdc_clk = {
	.halt_reg = 0x1810a4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810a4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_cal_hm1_cdc_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_cal_hm0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm1_clk = {
	.halt_reg = 0x181150,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181150,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_cal_hm1_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_cal_hm0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm1_dpm_ip_clk = {
	.halt_reg = 0x1810a8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810a8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_cal_hm1_dpm_ip_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_cal_hm0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_hm1_perf_cnt_clk = {
	.halt_reg = 0x1810ac,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810ac,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_cal_hm1_perf_cnt_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_cal_hm0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_core_clk = {
	.halt_reg = 0x181030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181030,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_core_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dl_dpm_clk = {
	.halt_reg = 0x181238,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181238,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dl_dpm_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_lmh_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dl_llm_clk = {
	.halt_reg = 0x181234,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181234,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dl_llm_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_lmh_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dpm_clk = {
	.halt_reg = 0x18107c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x18107c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dpm_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_lmh_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dpm_temp_clk = {
	.halt_reg = 0x1810c4,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1810c4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dpm_temp_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dpm_xo_clk = {
	.halt_reg = 0x181094,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181094,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dpm_xo_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_ahbm_clk = {
	.halt_reg = 0x181214,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x181214,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dsp_ahbm_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_ahbs_clk = {
	.halt_reg = 0x181210,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x181210,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dsp_ahbs_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_axi_clk = {
	.halt_reg = 0x18121c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x18121c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dsp_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_bwmon_ahb_clk = {
	.halt_reg = 0x181218,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181218,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dsp_bwmon_ahb_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_dsp_bwmon_clk = {
	.halt_reg = 0x181224,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181224,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_dsp_bwmon_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_isense_clk = {
	.halt_reg = 0x181078,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181078,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_isense_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_lmh_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_llm_clk = {
	.halt_reg = 0x181074,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181074,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_llm_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_lmh_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_llm_curr_clk = {
	.halt_reg = 0x1810d4,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1810d4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_llm_curr_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_llm_temp_clk = {
	.halt_reg = 0x1810c8,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1810c8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_llm_temp_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_llm_xo_clk = {
	.halt_reg = 0x181090,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x181090,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_llm_xo_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_noc_ahb_clk = {
	.halt_reg = 0x1810c0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810c0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_noc_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_noc_axi_clk = {
	.halt_reg = 0x1810b8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810b8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_noc_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_noc_dma_clk = {
	.halt_reg = 0x1810b0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810b0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_noc_dma_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_rsc_xo_clk = {
	.halt_reg = 0x1810e0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1810e0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_rsc_xo_clk",
			.parent_hws = (const struct clk_hw*[]){
				&npu_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_s2p_clk = {
	.halt_reg = 0x1810cc,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1810cc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "npu_cc_s2p_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *npu_cc_sm8250_clocks[] = {
	[NPU_CC_ATB_CLK] = &npu_cc_atb_clk.clkr,
	[NPU_CC_BTO_CORE_CLK] = &npu_cc_bto_core_clk.clkr,
	[NPU_CC_BWMON_CLK] = &npu_cc_bwmon_clk.clkr,
	[NPU_CC_CAL_HM0_CDC_CLK] = &npu_cc_cal_hm0_cdc_clk.clkr,
	[NPU_CC_CAL_HM0_CLK] = &npu_cc_cal_hm0_clk.clkr,
	[NPU_CC_CAL_HM0_CLK_SRC] = &npu_cc_cal_hm0_clk_src.clkr,
	[NPU_CC_CAL_HM0_DPM_IP_CLK] = &npu_cc_cal_hm0_dpm_ip_clk.clkr,
	[NPU_CC_CAL_HM0_PERF_CNT_CLK] = &npu_cc_cal_hm0_perf_cnt_clk.clkr,
	[NPU_CC_CAL_HM1_CDC_CLK] = &npu_cc_cal_hm1_cdc_clk.clkr,
	[NPU_CC_CAL_HM1_CLK] = &npu_cc_cal_hm1_clk.clkr,
	[NPU_CC_CAL_HM1_CLK_SRC] = &npu_cc_cal_hm1_clk_src.clkr,
	[NPU_CC_CAL_HM1_DPM_IP_CLK] = &npu_cc_cal_hm1_dpm_ip_clk.clkr,
	[NPU_CC_CAL_HM1_PERF_CNT_CLK] = &npu_cc_cal_hm1_perf_cnt_clk.clkr,
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
	[NPU_CC_PLL1] = &npu_cc_pll1.clkr,
	[NPU_CC_RSC_XO_CLK] = &npu_cc_rsc_xo_clk.clkr,
	[NPU_CC_S2P_CLK] = &npu_cc_s2p_clk.clkr,
	[NPU_CC_XO_CLK_SRC] = &npu_cc_xo_clk_src.clkr,
	[NPU_DSP_CORE_CLK_SRC] = &npu_dsp_core_clk_src.clkr,
	[NPU_Q6SS_PLL] = &npu_q6ss_pll.clkr,
};

static const struct qcom_reset_map npu_cc_sm8250_resets[] = {
	[NPU_CC_CAL_HM0_BCR] = { 0x1810f0 },
	[NPU_CC_CAL_HM1_BCR] = { 0x181130 },
	[NPU_CC_CORE_BCR] = { 0x181000 },
	[NPU_CC_DSP_BCR] = { 0x181200 },
};

static const struct regmap_config npu_cc_sm8250_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x18a060,
	.fast_io	= true,
};

static const struct qcom_cc_desc npu_cc_sm8250_desc = {
	.config = &npu_cc_sm8250_regmap_config,
	.clks = npu_cc_sm8250_clocks,
	.num_clks = ARRAY_SIZE(npu_cc_sm8250_clocks),
	.resets = npu_cc_sm8250_resets,
	.num_resets = ARRAY_SIZE(npu_cc_sm8250_resets),
	.clk_regulators = npu_cc_sm8250_regulators,
	.num_clk_regulators = ARRAY_SIZE(npu_cc_sm8250_regulators),
};

static const struct of_device_id npu_cc_sm8250_match_table[] = {
	{ .compatible = "qcom,sm8250-npucc" },
	{ }
};
MODULE_DEVICE_TABLE(of, npu_cc_sm8250_match_table);

static int npu_cc_sm8250_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &npu_cc_sm8250_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	clk_lucid_pll_configure(&npu_cc_pll0, regmap, &npu_cc_pll0_config);
	clk_lucid_pll_configure(&npu_cc_pll1, regmap, &npu_cc_pll1_config);
	clk_lucid_pll_configure(&npu_q6ss_pll, regmap, &npu_q6ss_pll_config);

	/*
	 * Keep clocks always enabled:
	 *	npu_cc_xo_clk
	 */
	regmap_update_bits(regmap, 0x181410, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(pdev, &npu_cc_sm8250_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register NPU CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered NPU CC clocks\n");

	return ret;
}

static void npu_cc_sm8250_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &npu_cc_sm8250_desc);
}

static struct platform_driver npu_cc_sm8250_driver = {
	.probe = npu_cc_sm8250_probe,
	.driver = {
		.name = "sm8250-npucc",
		.of_match_table = npu_cc_sm8250_match_table,
		.sync_state = npu_cc_sm8250_sync_state,
	},
};

static int __init npu_cc_sm8250_init(void)
{
	return platform_driver_register(&npu_cc_sm8250_driver);
}
subsys_initcall(npu_cc_sm8250_init);

static void __exit npu_cc_sm8250_exit(void)
{
	platform_driver_unregister(&npu_cc_sm8250_driver);
}
module_exit(npu_cc_sm8250_exit);

MODULE_DESCRIPTION("QTI NPU_CC SM8250 Driver");
MODULE_LICENSE("GPL v2");

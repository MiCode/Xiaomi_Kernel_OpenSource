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

#include <dt-bindings/clock/qcom,npucc-sdmmagpie.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "reset.h"
#include "clk-alpha-pll.h"
#include "vdd-level-sdmmagpie.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

#define CRC_SID_FSM_CTRL		0x100c
#define CRC_SID_FSM_CTRL_SETTING	0x800000
#define CRC_MND_CFG			0x1010
#define CRC_MND_CFG_SETTING		0x15011

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);

enum {
	P_BI_TCXO,
	P_CORE_BI_PLL_TEST_SE,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_NPU_CC_PLL0_OUT_EVEN,
	P_NPU_CC_PLL1_OUT_EVEN,
	P_NPU_CC_CRC_DIV,
};

static const struct parent_map npu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_NPU_CC_PLL1_OUT_EVEN, 1 },
	{ P_NPU_CC_PLL0_OUT_EVEN, 2 },
	{ P_GPLL0_OUT_MAIN, 4 },
	{ P_GPLL0_OUT_MAIN_DIV, 5 },
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

static const struct parent_map npu_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_NPU_CC_PLL1_OUT_EVEN, 1 },
	{ P_NPU_CC_CRC_DIV, 2 },
	{ P_GPLL0_OUT_MAIN, 4 },
	{ P_GPLL0_OUT_MAIN_DIV, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const npu_cc_parent_names_1[] = {
	"bi_tcxo",
	"npu_cc_pll1_out_even",
	"npu_cc_crc_div",
	"gcc_npu_gpll0_clk_src",
	"gcc_npu_gpll0_div_clk_src",
	"core_bi_pll_test_se",
};


static struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
	{ 125000000, 1000000000, 1 },
};

static const struct alpha_pll_config npu_cc_pll0_config = {
	.l = 0x1F,
	.frac = 0x4000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002067,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00004805,
	.test_ctl_hi_val = 0x40000000,
};

static struct clk_alpha_pll npu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_pll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_fabia_pll_ops,
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

static const struct clk_div_table post_div_table_fabia_even[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{ }
};

static struct clk_alpha_pll_postdiv npu_cc_pll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_fabia_even,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_pll0_out_even",
		.parent_names = (const char *[]){ "npu_cc_pll0" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_generic_pll_postdiv_ops,
	},
};

static const struct alpha_pll_config npu_cc_pll1_config = {
	.l = 0xF,
	.frac = 0xA000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002067,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00004805,
	.test_ctl_hi_val = 0x40000000,
};

static struct clk_alpha_pll npu_cc_pll1 = {
	.offset = 0x400,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_pll1",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_fabia_pll_ops,
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

static struct clk_alpha_pll_postdiv npu_cc_pll1_out_even = {
	.offset = 0x400,
	.post_div_shift = 8,
	.post_div_table = post_div_table_fabia_even,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_pll1_out_even",
		.parent_names = (const char *[]){ "npu_cc_pll1" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_generic_pll_postdiv_ops,
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

static const struct freq_tbl ftbl_npu_cc_cal_dp_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(300000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(350000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(400000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(466500000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(600000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	F(700000000, P_NPU_CC_CRC_DIV, 1, 0, 0),
	{ }
};

static struct clk_rcg2 npu_cc_cal_dp_clk_src = {
	.cmd_rcgr = 0x1004,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_1,
	.freq_tbl = ftbl_npu_cc_cal_dp_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_cal_dp_clk_src",
		.parent_names = npu_cc_parent_names_1,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000,
			[VDD_LOW] = 400000000,
			[VDD_LOW_L1] = 466500000,
			[VDD_NOMINAL] = 600000000,
			[VDD_HIGH] = 700000000},
	},
};

static const struct freq_tbl ftbl_npu_cc_npu_core_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(100000000, P_GPLL0_OUT_MAIN_DIV, 3, 0, 0),
	F(150000000, P_GPLL0_OUT_MAIN, 4, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	F(400000000, P_GPLL0_OUT_MAIN, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 npu_cc_npu_core_clk_src = {
	.cmd_rcgr = 0x1030,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = npu_cc_parent_map_0,
	.freq_tbl = ftbl_npu_cc_npu_core_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "npu_cc_npu_core_clk_src",
		.parent_names = npu_cc_parent_names_0,
		.num_parents = 6,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.flags = CLK_SET_RATE_PARENT,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 150000000,
			[VDD_LOW_L1] = 200000000,
			[VDD_NOMINAL] = 300000000,
			[VDD_HIGH] = 400000000},
	},
};

static struct clk_branch npu_cc_armwic_core_clk = {
	.halt_reg = 0x1058,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_armwic_core_clk",
			.parent_names = (const char *[]){
				"npu_cc_npu_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_bto_core_clk = {
	.halt_reg = 0x1090,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1090,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_bto_core_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_bwmon_clk = {
	.halt_reg = 0x1088,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1088,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_bwmon_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_dp_cdc_clk = {
	.halt_reg = 0x1068,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1068,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_cal_dp_cdc_clk",
			.parent_names = (const char *[]){
				"npu_cc_cal_dp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_cal_dp_clk = {
	.halt_reg = 0x101c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x101c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_cal_dp_clk",
			.parent_names = (const char *[]){
				"npu_cc_cal_dp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_comp_noc_axi_clk = {
	.halt_reg = 0x106c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x106c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_comp_noc_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_conf_noc_ahb_clk = {
	.halt_reg = 0x1074,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1074,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_conf_noc_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_npu_core_apb_clk = {
	.halt_reg = 0x1080,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1080,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_npu_core_apb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_npu_core_atb_clk = {
	.halt_reg = 0x1078,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1078,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_npu_core_atb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_npu_core_clk = {
	.halt_reg = 0x1048,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_npu_core_clk",
			.parent_names = (const char *[]){
				"npu_cc_npu_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_npu_core_cti_clk = {
	.halt_reg = 0x107c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x107c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_npu_core_cti_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_npu_cpc_clk = {
	.halt_reg = 0x1050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_npu_cpc_clk",
			.parent_names = (const char *[]){
				"npu_cc_npu_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_npu_cpc_timer_clk = {
	.halt_reg = 0x105c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x105c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_npu_cpc_timer_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_perf_cnt_clk = {
	.halt_reg = 0x108c,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x108c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_perf_cnt_clk",
			.parent_names = (const char *[]){
				"npu_cc_cal_dp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_qtimer_core_clk = {
	.halt_reg = 0x1060,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1060,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_qtimer_core_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_sleep_clk = {
	.halt_reg = 0x1064,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1064,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch npu_cc_xo_clk = {
	.halt_reg = 0x3020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "npu_cc_xo_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *npu_cc_sdmmagpie_clocks[] = {
	[NPU_CC_PLL0] = &npu_cc_pll0.clkr,
	[NPU_CC_PLL0_OUT_EVEN] = &npu_cc_pll0_out_even.clkr,
	[NPU_CC_PLL1] = &npu_cc_pll1.clkr,
	[NPU_CC_PLL1_OUT_EVEN] = &npu_cc_pll1_out_even.clkr,
	[NPU_CC_ARMWIC_CORE_CLK] = &npu_cc_armwic_core_clk.clkr,
	[NPU_CC_BTO_CORE_CLK] = &npu_cc_bto_core_clk.clkr,
	[NPU_CC_BWMON_CLK] = &npu_cc_bwmon_clk.clkr,
	[NPU_CC_CAL_DP_CDC_CLK] = &npu_cc_cal_dp_cdc_clk.clkr,
	[NPU_CC_CAL_DP_CLK] = &npu_cc_cal_dp_clk.clkr,
	[NPU_CC_CAL_DP_CLK_SRC] = &npu_cc_cal_dp_clk_src.clkr,
	[NPU_CC_COMP_NOC_AXI_CLK] = &npu_cc_comp_noc_axi_clk.clkr,
	[NPU_CC_CONF_NOC_AHB_CLK] = &npu_cc_conf_noc_ahb_clk.clkr,
	[NPU_CC_NPU_CORE_APB_CLK] = &npu_cc_npu_core_apb_clk.clkr,
	[NPU_CC_NPU_CORE_ATB_CLK] = &npu_cc_npu_core_atb_clk.clkr,
	[NPU_CC_NPU_CORE_CLK] = &npu_cc_npu_core_clk.clkr,
	[NPU_CC_NPU_CORE_CLK_SRC] = &npu_cc_npu_core_clk_src.clkr,
	[NPU_CC_NPU_CORE_CTI_CLK] = &npu_cc_npu_core_cti_clk.clkr,
	[NPU_CC_NPU_CPC_CLK] = &npu_cc_npu_cpc_clk.clkr,
	[NPU_CC_NPU_CPC_TIMER_CLK] = &npu_cc_npu_cpc_timer_clk.clkr,
	[NPU_CC_PERF_CNT_CLK] = &npu_cc_perf_cnt_clk.clkr,
	[NPU_CC_QTIMER_CORE_CLK] = &npu_cc_qtimer_core_clk.clkr,
	[NPU_CC_SLEEP_CLK] = &npu_cc_sleep_clk.clkr,
	[NPU_CC_XO_CLK] = &npu_cc_xo_clk.clkr,
};

static const struct qcom_reset_map npu_cc_sdmmagpie_resets[] = {
	[NPU_CC_CAL_DP_BCR] = { 0x1000 },
	[NPU_CC_NPU_CORE_BCR] = { 0x1024 },
};

static const struct regmap_config npu_cc_sdmmagpie_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x8000,
	.fast_io	= true,
};

static const struct qcom_cc_desc npu_cc_sdmmagpie_desc = {
	.config = &npu_cc_sdmmagpie_regmap_config,
	.clks = npu_cc_sdmmagpie_clocks,
	.num_clks = ARRAY_SIZE(npu_cc_sdmmagpie_clocks),
	.resets = npu_cc_sdmmagpie_resets,
	.num_resets = ARRAY_SIZE(npu_cc_sdmmagpie_resets),
};

static const struct of_device_id npu_cc_sdmmagpie_match_table[] = {
	{ .compatible = "qcom,npucc-sdmmagpie" },
	{ }
};
MODULE_DEVICE_TABLE(of, npu_cc_sdmmagpie_match_table);

static int enable_npu_crc(struct regmap *regmap, struct regulator *npu_gdsc)
{
	int ret;

	/* Set npu_cc_cal_cp_clk to the lowest supported frequency */
	clk_set_rate(npu_cc_cal_dp_clk.clkr.hw.clk,
			clk_round_rate(npu_cc_cal_dp_clk_src.clkr.hw.clk, 1));

	/* Turn on the NPU GDSC */
	ret = regulator_enable(npu_gdsc);
	if (ret) {
		pr_err("Failed to enable the NPU GDSC during CRC sequence\n");
		return ret;
	}

	/* Enable npu_cc_cal_cp_clk */
	ret = clk_prepare_enable(npu_cc_cal_dp_clk.clkr.hw.clk);
	if (ret) {
		pr_err("Failed to enable npu_cc_cal_dp_clk during CRC sequence\n");
		regulator_disable(npu_gdsc);
		return ret;
	}

	/* Enable MND RC */
	regmap_write(regmap, CRC_MND_CFG, CRC_MND_CFG_SETTING);
	regmap_write(regmap, CRC_SID_FSM_CTRL, CRC_SID_FSM_CTRL_SETTING);

	/* Wait for 16 cycles before continuing */
	udelay(1);

	/* Disable npu_cc_cal_cp_clk */
	clk_disable_unprepare(npu_cc_cal_dp_clk.clkr.hw.clk);

	/* Turn off the NPU GDSC */
	regulator_disable(npu_gdsc);

	return 0;
}

static int npu_cc_sdmmagpie_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	struct regulator *npu_gdsc;
	int ret = 0;

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	npu_gdsc = devm_regulator_get(&pdev->dev, "npu_gdsc");
	if (IS_ERR(npu_gdsc)) {
		if (!(PTR_ERR(npu_gdsc) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get npu_gdsc regulator\n");
		return PTR_ERR(npu_gdsc);
	}

	regmap = qcom_cc_map(pdev, &npu_cc_sdmmagpie_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the npu CC registers\n");
		return PTR_ERR(regmap);
	}

	clk_fabia_pll_configure(&npu_cc_pll0, regmap, &npu_cc_pll0_config);
	clk_fabia_pll_configure(&npu_cc_pll1, regmap, &npu_cc_pll1_config);

	/* Register the fixed factor clock for CRC divide */
	ret = devm_clk_hw_register(&pdev->dev, &npu_cc_crc_div.hw);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register CRC divide clock\n");
		return ret;
	}

	ret = qcom_cc_really_probe(pdev, &npu_cc_sdmmagpie_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register NPU CC clocks\n");
		return ret;
	}

	ret = enable_npu_crc(regmap, npu_gdsc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable CRC for NPU cal RCG\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered NPU CC clocks\n");
	return ret;
}

static struct platform_driver npu_cc_sdmmagpie_driver = {
	.probe	= npu_cc_sdmmagpie_probe,
	.driver	= {
		.name = "npu_cc-sdmmagpie",
		.of_match_table = npu_cc_sdmmagpie_match_table,
	},
};

static int __init npu_cc_sdmmagpie_init(void)
{
	return platform_driver_register(&npu_cc_sdmmagpie_driver);
}
subsys_initcall(npu_cc_sdmmagpie_init);

static void __exit npu_cc_sdmmagpie_exit(void)
{
	platform_driver_unregister(&npu_cc_sdmmagpie_driver);
}
module_exit(npu_cc_sdmmagpie_exit);

MODULE_DESCRIPTION("QTI NPU_CC SDMMAGPIE Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:npu_cc-sdmmagpie");

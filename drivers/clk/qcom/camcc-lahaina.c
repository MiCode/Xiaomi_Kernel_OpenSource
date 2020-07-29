// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

#include <dt-bindings/clock/qcom,camcc-lahaina.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "common.h"
#include "clk-regmap-mux.h"
#include "clk-regmap-divider.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_mm, VDD_NOMINAL + 1, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_HIGH + 1, 1, vdd_corner);

static struct clk_vdd_class *cam_cc_lahaina_regulators[] = {
	&vdd_mm,
	&vdd_mx,
};

enum {
	P_BI_TCXO,
	P_CAM_CC_PLL0_OUT_EVEN,
	P_CAM_CC_PLL0_OUT_MAIN,
	P_CAM_CC_PLL0_OUT_ODD,
	P_CAM_CC_PLL1_OUT_EVEN,
	P_CAM_CC_PLL2_OUT_AUX2,
	P_CAM_CC_PLL2_OUT_EARLY,
	P_CAM_CC_PLL3_OUT_EVEN,
	P_CAM_CC_PLL4_OUT_EVEN,
	P_CAM_CC_PLL5_OUT_EVEN,
	P_CAM_CC_PLL6_OUT_EVEN,
	P_CORE_BI_PLL_TEST_SE,
	P_SLEEP_CLK,
};

static struct pll_vco lucid_5lpe_vco[] = {
	{ 249600000, 1750000000, 0 },
};

static struct pll_vco zonda_5lpe_vco[] = {
	{ 595200000, 3600000000, 0 },
};

static const struct alpha_pll_config cam_cc_pll0_config = {
	.l = 0x3E,
	.cal_l = 0x44,
	.alpha = 0x8000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x2A9A699C,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
	.user_ctl_val = 0x00003100,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll cam_cc_pll0 = {
	.offset = 0x0,
	.vco_table = lucid_5lpe_vco,
	.num_vco = ARRAY_SIZE(lucid_5lpe_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
				.name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_5lpe_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000},
		},
	},
};

static const struct clk_div_table post_div_table_cam_cc_pll0_out_even[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv cam_cc_pll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_cam_cc_pll0_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_cam_cc_pll0_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll0_out_even",
		.parent_data = &(const struct clk_parent_data){
			.hw = &cam_cc_pll0.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_lucid_5lpe_ops,
	},
};

static const struct clk_div_table post_div_table_cam_cc_pll0_out_odd[] = {
	{ 0x3, 3 },
	{ }
};

static struct clk_alpha_pll_postdiv cam_cc_pll0_out_odd = {
	.offset = 0x0,
	.post_div_shift = 12,
	.post_div_table = post_div_table_cam_cc_pll0_out_odd,
	.num_post_div = ARRAY_SIZE(post_div_table_cam_cc_pll0_out_odd),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll0_out_odd",
		.parent_data = &(const struct clk_parent_data){
			.hw = &cam_cc_pll0.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_lucid_5lpe_ops,
	},
};

static const struct alpha_pll_config cam_cc_pll1_config = {
	.l = 0x1F,
	.cal_l = 0x44,
	.alpha = 0x4000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x2A9A699C,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
	.user_ctl_val = 0x00000100,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll cam_cc_pll1 = {
	.offset = 0x1000,
	.vco_table = lucid_5lpe_vco,
	.num_vco = ARRAY_SIZE(lucid_5lpe_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll1",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
				.name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_5lpe_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000},
		},
	},
};

static const struct clk_div_table post_div_table_cam_cc_pll1_out_even[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv cam_cc_pll1_out_even = {
	.offset = 0x1000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_cam_cc_pll1_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_cam_cc_pll1_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll1_out_even",
		.parent_data = &(const struct clk_parent_data){
			.hw = &cam_cc_pll1.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_lucid_5lpe_ops,
	},
};

static const struct alpha_pll_config cam_cc_pll2_config = {
	.l = 0x32,
	.cal_l = 0x32,
	.alpha = 0x0,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008011,
	.config_ctl_hi1_val = 0x00000000,
};

static const struct alpha_pll_config cam_cc_pll2_config_sm8350_v2 = {
	.l = 0x32,
	.cal_l = 0x32,
	.alpha = 0x0,
	.config_ctl_val = 0x08200800,
	.config_ctl_hi_val = 0x05028011,
	.config_ctl_hi1_val = 0x08000000,
};

static struct clk_alpha_pll cam_cc_pll2 = {
	.offset = 0x2000,
	.vco_table = zonda_5lpe_vco,
	.num_vco = ARRAY_SIZE(zonda_5lpe_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_ZONDA_5LPE],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll2",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
				.name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_zonda_5lpe_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_LOWER] = 1800000000,
				[VDD_LOW] = 2400000000,
				[VDD_NOMINAL] = 3000000000,
				[VDD_HIGH] = 3600000000},
		},
	},
};

static const struct alpha_pll_config cam_cc_pll3_config = {
	.l = 0x23,
	.cal_l = 0x44,
	.alpha = 0x3555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x2A9A699C,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
	.user_ctl_val = 0x00000100,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll cam_cc_pll3 = {
	.offset = 0x3000,
	.vco_table = lucid_5lpe_vco,
	.num_vco = ARRAY_SIZE(lucid_5lpe_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll3",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
				.name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_5lpe_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000},
		},
	},
};

static const struct clk_div_table post_div_table_cam_cc_pll3_out_even[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv cam_cc_pll3_out_even = {
	.offset = 0x3000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_cam_cc_pll3_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_cam_cc_pll3_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll3_out_even",
		.parent_data = &(const struct clk_parent_data){
			.hw = &cam_cc_pll3.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_lucid_5lpe_ops,
	},
};

static const struct alpha_pll_config cam_cc_pll4_config = {
	.l = 0x23,
	.cal_l = 0x44,
	.alpha = 0x3555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x2A9A699C,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
	.user_ctl_val = 0x00000100,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll cam_cc_pll4 = {
	.offset = 0x4000,
	.vco_table = lucid_5lpe_vco,
	.num_vco = ARRAY_SIZE(lucid_5lpe_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll4",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
				.name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_5lpe_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000},
		},
	},
};

static const struct clk_div_table post_div_table_cam_cc_pll4_out_even[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv cam_cc_pll4_out_even = {
	.offset = 0x4000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_cam_cc_pll4_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_cam_cc_pll4_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll4_out_even",
		.parent_data = &(const struct clk_parent_data){
			.hw = &cam_cc_pll4.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_lucid_5lpe_ops,
	},
};

static const struct alpha_pll_config cam_cc_pll5_config = {
	.l = 0x23,
	.cal_l = 0x44,
	.alpha = 0x3555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x2A9A699C,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
	.user_ctl_val = 0x00000100,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll cam_cc_pll5 = {
	.offset = 0x5000,
	.vco_table = lucid_5lpe_vco,
	.num_vco = ARRAY_SIZE(lucid_5lpe_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll5",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
				.name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_5lpe_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000},
		},
	},
};

static const struct clk_div_table post_div_table_cam_cc_pll5_out_even[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv cam_cc_pll5_out_even = {
	.offset = 0x5000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_cam_cc_pll5_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_cam_cc_pll5_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll5_out_even",
		.parent_data = &(const struct clk_parent_data){
			.hw = &cam_cc_pll5.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_lucid_5lpe_ops,
	},
};

static const struct alpha_pll_config cam_cc_pll6_config = {
	.l = 0x32,
	.cal_l = 0x44,
	.alpha = 0x0,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x2A9A699C,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
	.user_ctl_val = 0x00000100,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll cam_cc_pll6 = {
	.offset = 0x6000,
	.vco_table = lucid_5lpe_vco,
	.num_vco = ARRAY_SIZE(lucid_5lpe_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_pll6",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
				.name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_5lpe_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000},
		},
	},
};

static const struct clk_div_table post_div_table_cam_cc_pll6_out_even[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv cam_cc_pll6_out_even = {
	.offset = 0x6000,
	.post_div_shift = 8,
	.post_div_table = post_div_table_cam_cc_pll6_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_cam_cc_pll6_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_pll6_out_even",
		.parent_data = &(const struct clk_parent_data){
			.hw = &cam_cc_pll6.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_lucid_5lpe_ops,
	},
};

static const struct parent_map cam_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_CAM_CC_PLL0_OUT_MAIN, 1 },
	{ P_CAM_CC_PLL0_OUT_EVEN, 2 },
	{ P_CAM_CC_PLL0_OUT_ODD, 3 },
	{ P_CAM_CC_PLL6_OUT_EVEN, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data cam_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo", .name = "bi_tcxo" },
	{ .hw = &cam_cc_pll0.clkr.hw },
	{ .hw = &cam_cc_pll0_out_even.clkr.hw },
	{ .hw = &cam_cc_pll0_out_odd.clkr.hw },
	{ .hw = &cam_cc_pll6_out_even.clkr.hw },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map cam_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_CAM_CC_PLL2_OUT_AUX2, 3 },
	{ P_CAM_CC_PLL2_OUT_EARLY, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data cam_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo", .name = "bi_tcxo" },
	{ .hw = &cam_cc_pll2.clkr.hw },
	{ .hw = &cam_cc_pll2.clkr.hw },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map cam_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_CAM_CC_PLL3_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data cam_cc_parent_data_2[] = {
	{ .fw_name = "bi_tcxo", .name = "bi_tcxo" },
	{ .hw = &cam_cc_pll3_out_even.clkr.hw },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map cam_cc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_CAM_CC_PLL4_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data cam_cc_parent_data_3[] = {
	{ .fw_name = "bi_tcxo", .name = "bi_tcxo" },
	{ .hw = &cam_cc_pll4_out_even.clkr.hw },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map cam_cc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_CAM_CC_PLL5_OUT_EVEN, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data cam_cc_parent_data_4[] = {
	{ .fw_name = "bi_tcxo", .name = "bi_tcxo" },
	{ .hw = &cam_cc_pll5_out_even.clkr.hw },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map cam_cc_parent_map_5[] = {
	{ P_BI_TCXO, 0 },
	{ P_CAM_CC_PLL1_OUT_EVEN, 4 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data cam_cc_parent_data_5[] = {
	{ .fw_name = "bi_tcxo", .name = "bi_tcxo" },
	{ .hw = &cam_cc_pll1_out_even.clkr.hw },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map cam_cc_parent_map_6[] = {
	{ P_SLEEP_CLK, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data cam_cc_parent_data_6[] = {
	{ .fw_name = "sleep_clk", .name = "sleep_clk" },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map cam_cc_parent_map_7[] = {
	{ P_BI_TCXO, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data cam_cc_parent_data_7_ao[] = {
	{ .fw_name = "bi_tcxo_ao", .name = "bi_tcxo_ao" },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct freq_tbl ftbl_cam_cc_bps_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(200000000, P_CAM_CC_PLL0_OUT_ODD, 2, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_ODD, 1, 0, 0),
	F(480000000, P_CAM_CC_PLL6_OUT_EVEN, 1, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_bps_clk_src = {
	.cmd_rcgr = 0x7010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_bps_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_bps_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 200000000,
			[VDD_LOW] = 400000000,
			[VDD_LOW_L1] = 480000000,
			[VDD_NOMINAL] = 600000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_camnoc_axi_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(300000000, P_CAM_CC_PLL0_OUT_EVEN, 2, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_ODD, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_camnoc_axi_clk_src = {
	.cmd_rcgr = 0xc0d4,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_camnoc_axi_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_camnoc_axi_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000,
			[VDD_LOW] = 400000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_cci_0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(37500000, P_CAM_CC_PLL0_OUT_EVEN, 16, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_cci_0_clk_src = {
	.cmd_rcgr = 0xc098,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_cci_0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_cci_0_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 37500000},
	},
};

static struct clk_rcg2 cam_cc_cci_1_clk_src = {
	.cmd_rcgr = 0xc0b4,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_cci_0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_cci_1_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 37500000},
	},
};

static const struct freq_tbl ftbl_cam_cc_cphy_rx_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_ODD, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_cphy_rx_clk_src = {
	.cmd_rcgr = 0xa068,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_cphy_rx_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_cphy_rx_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 400000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_csi0phytimer_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(300000000, P_CAM_CC_PLL0_OUT_EVEN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_csi0phytimer_clk_src = {
	.cmd_rcgr = 0xe0a8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi0phytimer_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000},
	},
};

static struct clk_rcg2 cam_cc_csi1phytimer_clk_src = {
	.cmd_rcgr = 0xe0c8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi1phytimer_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000},
	},
};

static struct clk_rcg2 cam_cc_csi2phytimer_clk_src = {
	.cmd_rcgr = 0xe0e8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi2phytimer_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000},
	},
};

static struct clk_rcg2 cam_cc_csi3phytimer_clk_src = {
	.cmd_rcgr = 0xe10c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi3phytimer_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000},
	},
};

static struct clk_rcg2 cam_cc_csi4phytimer_clk_src = {
	.cmd_rcgr = 0xe12c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi4phytimer_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000},
	},
};

static struct clk_rcg2 cam_cc_csi5phytimer_clk_src = {
	.cmd_rcgr = 0xe14c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_csi0phytimer_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_csi5phytimer_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_fast_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(100000000, P_CAM_CC_PLL0_OUT_EVEN, 6, 0, 0),
	F(200000000, P_CAM_CC_PLL0_OUT_EVEN, 3, 0, 0),
	F(300000000, P_CAM_CC_PLL0_OUT_MAIN, 4, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_fast_ahb_clk_src = {
	.cmd_rcgr = 0x703c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_fast_ahb_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_fast_ahb_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 100000000,
			[VDD_LOW] = 200000000,
			[VDD_LOW_L1] = 300000000,
			[VDD_NOMINAL] = 400000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_icp_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_ODD, 1, 0, 0),
	F(480000000, P_CAM_CC_PLL6_OUT_EVEN, 1, 0, 0),
	F(600000000, P_CAM_CC_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_icp_clk_src = {
	.cmd_rcgr = 0xc074,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_icp_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_icp_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 400000000,
			[VDD_LOW] = 480000000,
			[VDD_LOW_L1] = 600000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_ife_0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(338000000, P_CAM_CC_PLL3_OUT_EVEN, 1, 0, 0),
	F(475000000, P_CAM_CC_PLL3_OUT_EVEN, 1, 0, 0),
	F(600000000, P_CAM_CC_PLL3_OUT_EVEN, 1, 0, 0),
	F(720000000, P_CAM_CC_PLL3_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ife_0_clk_src = {
	.cmd_rcgr = 0xa010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_2,
	.freq_tbl = ftbl_cam_cc_ife_0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_0_clk_src",
		.parent_data = cam_cc_parent_data_2,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 338000000,
			[VDD_LOW] = 475000000,
			[VDD_LOW_L1] = 600000000,
			[VDD_NOMINAL] = 720000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_ife_0_csid_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(75000000, P_CAM_CC_PLL0_OUT_EVEN, 8, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_ODD, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ife_0_csid_clk_src = {
	.cmd_rcgr = 0xa040,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_ife_0_csid_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_0_csid_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 400000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_ife_1_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(338000000, P_CAM_CC_PLL4_OUT_EVEN, 1, 0, 0),
	F(475000000, P_CAM_CC_PLL4_OUT_EVEN, 1, 0, 0),
	F(600000000, P_CAM_CC_PLL4_OUT_EVEN, 1, 0, 0),
	F(720000000, P_CAM_CC_PLL4_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ife_1_clk_src = {
	.cmd_rcgr = 0xb010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_3,
	.freq_tbl = ftbl_cam_cc_ife_1_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_1_clk_src",
		.parent_data = cam_cc_parent_data_3,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 338000000,
			[VDD_LOW] = 475000000,
			[VDD_LOW_L1] = 600000000,
			[VDD_NOMINAL] = 720000000},
	},
};

static struct clk_rcg2 cam_cc_ife_1_csid_clk_src = {
	.cmd_rcgr = 0xb040,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_ife_0_csid_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_1_csid_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 400000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_ife_2_clk_src[] = {
	F(338000000, P_CAM_CC_PLL5_OUT_EVEN, 1, 0, 0),
	F(475000000, P_CAM_CC_PLL5_OUT_EVEN, 1, 0, 0),
	F(600000000, P_CAM_CC_PLL5_OUT_EVEN, 1, 0, 0),
	F(720000000, P_CAM_CC_PLL5_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ife_2_clk_src = {
	.cmd_rcgr = 0xb07c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_4,
	.freq_tbl = ftbl_cam_cc_ife_2_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_2_clk_src",
		.parent_data = cam_cc_parent_data_4,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 338000000,
			[VDD_LOW] = 475000000,
			[VDD_LOW_L1] = 600000000,
			[VDD_NOMINAL] = 720000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_ife_2_csid_clk_src[] = {
	F(400000000, P_CAM_CC_PLL0_OUT_ODD, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ife_2_csid_clk_src = {
	.cmd_rcgr = 0xb0a8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_ife_2_csid_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_2_csid_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 400000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_ife_lite_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(400000000, P_CAM_CC_PLL0_OUT_ODD, 1, 0, 0),
	F(480000000, P_CAM_CC_PLL6_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ife_lite_clk_src = {
	.cmd_rcgr = 0xc000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_ife_lite_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_lite_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 400000000,
			[VDD_LOW] = 480000000},
	},
};

static struct clk_rcg2 cam_cc_ife_lite_csid_clk_src = {
	.cmd_rcgr = 0xc01c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_cphy_rx_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ife_lite_csid_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 400000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_ipe_0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(300000000, P_CAM_CC_PLL1_OUT_EVEN, 1, 0, 0),
	F(450000000, P_CAM_CC_PLL1_OUT_EVEN, 1, 0, 0),
	F(525000000, P_CAM_CC_PLL1_OUT_EVEN, 1, 0, 0),
	F(700000000, P_CAM_CC_PLL1_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_ipe_0_clk_src = {
	.cmd_rcgr = 0x8010,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_5,
	.freq_tbl = ftbl_cam_cc_ipe_0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_ipe_0_clk_src",
		.parent_data = cam_cc_parent_data_5,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000,
			[VDD_LOW] = 450000000,
			[VDD_LOW_L1] = 525000000,
			[VDD_NOMINAL] = 700000000},
	},
};

static struct clk_rcg2 cam_cc_jpeg_clk_src = {
	.cmd_rcgr = 0xc048,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_bps_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_jpeg_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 200000000,
			[VDD_LOW] = 400000000,
			[VDD_LOW_L1] = 480000000,
			[VDD_NOMINAL] = 600000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_mclk0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(24000000, P_CAM_CC_PLL2_OUT_EARLY, 10, 1, 4),
	F(68571429, P_CAM_CC_PLL2_OUT_EARLY, 14, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_mclk0_clk_src = {
	.cmd_rcgr = 0xe000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk0_clk_src",
		.parent_data = cam_cc_parent_data_1,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 68571429},
	},
};

static struct clk_rcg2 cam_cc_mclk1_clk_src = {
	.cmd_rcgr = 0xe01c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk1_clk_src",
		.parent_data = cam_cc_parent_data_1,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 68571429},
	},
};

static struct clk_rcg2 cam_cc_mclk2_clk_src = {
	.cmd_rcgr = 0xe038,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk2_clk_src",
		.parent_data = cam_cc_parent_data_1,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 68571429},
	},
};

static struct clk_rcg2 cam_cc_mclk3_clk_src = {
	.cmd_rcgr = 0xe054,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk3_clk_src",
		.parent_data = cam_cc_parent_data_1,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 68571429},
	},
};

static struct clk_rcg2 cam_cc_mclk4_clk_src = {
	.cmd_rcgr = 0xe070,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk4_clk_src",
		.parent_data = cam_cc_parent_data_1,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 68571429},
	},
};

static struct clk_rcg2 cam_cc_mclk5_clk_src = {
	.cmd_rcgr = 0xe08c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_1,
	.freq_tbl = ftbl_cam_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_mclk5_clk_src",
		.parent_data = cam_cc_parent_data_1,
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 68571429},
	},
};

static struct clk_rcg2 cam_cc_sbi_csid_clk_src = {
	.cmd_rcgr = 0x901c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_cphy_rx_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_sbi_csid_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 400000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_sleep_clk_src[] = {
	F(32000, P_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_sleep_clk_src = {
	.cmd_rcgr = 0xc14c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_6,
	.freq_tbl = ftbl_cam_cc_sleep_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_sleep_clk_src",
		.parent_data = cam_cc_parent_data_6,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 32000},
	},
};

static const struct freq_tbl ftbl_cam_cc_slow_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(80000000, P_CAM_CC_PLL0_OUT_EVEN, 7.5, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_slow_ahb_clk_src = {
	.cmd_rcgr = 0x7058,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_0,
	.freq_tbl = ftbl_cam_cc_slow_ahb_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_slow_ahb_clk_src",
		.parent_data = cam_cc_parent_data_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 80000000},
	},
};

static const struct freq_tbl ftbl_cam_cc_xo_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_cc_xo_clk_src = {
	.cmd_rcgr = 0xc130,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_cc_parent_map_7,
	.freq_tbl = ftbl_cam_cc_xo_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "cam_cc_xo_clk_src",
		.parent_data = cam_cc_parent_data_7_ao,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_regmap_div cam_cc_sbi_div_clk_src = {
	.reg = 0x9010,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "cam_cc_sbi_div_clk_src",
		.parent_data = &(const struct clk_parent_data){
			.hw = &cam_cc_ife_0_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch cam_cc_bps_ahb_clk = {
	.halt_reg = 0x7070,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7070,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_bps_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_bps_areg_clk = {
	.halt_reg = 0x7054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_bps_areg_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_fast_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_bps_axi_clk = {
	.halt_reg = 0x7038,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7038,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_bps_axi_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_camnoc_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_bps_clk = {
	.halt_reg = 0x7028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x7028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_bps_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_bps_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_camnoc_axi_clk = {
	.halt_reg = 0xc0f0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc0f0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_camnoc_axi_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_camnoc_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_camnoc_dcd_xo_clk = {
	.halt_reg = 0xc0f8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc0f8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_camnoc_dcd_xo_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_cci_0_clk = {
	.halt_reg = 0xc0b0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc0b0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_cci_0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cci_0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_cci_1_clk = {
	.halt_reg = 0xc0cc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc0cc,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_cci_1_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cci_1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_core_ahb_clk = {
	.halt_reg = 0xc12c,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0xc12c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_core_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_cpas_ahb_clk = {
	.halt_reg = 0xc0d0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc0d0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_cpas_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi0phytimer_clk = {
	.halt_reg = 0xe0c0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe0c0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi0phytimer_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_csi0phytimer_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi1phytimer_clk = {
	.halt_reg = 0xe0e0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe0e0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi1phytimer_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_csi1phytimer_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi2phytimer_clk = {
	.halt_reg = 0xe100,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe100,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi2phytimer_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_csi2phytimer_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi3phytimer_clk = {
	.halt_reg = 0xe124,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe124,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi3phytimer_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_csi3phytimer_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi4phytimer_clk = {
	.halt_reg = 0xe144,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe144,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi4phytimer_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_csi4phytimer_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csi5phytimer_clk = {
	.halt_reg = 0xe164,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe164,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csi5phytimer_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_csi5phytimer_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy0_clk = {
	.halt_reg = 0xe0c4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe0c4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy1_clk = {
	.halt_reg = 0xe0e4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe0e4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy1_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy2_clk = {
	.halt_reg = 0xe104,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe104,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy2_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy3_clk = {
	.halt_reg = 0xe128,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe128,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy3_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy4_clk = {
	.halt_reg = 0xe148,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe148,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy4_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_csiphy5_clk = {
	.halt_reg = 0xe168,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe168,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_csiphy5_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_icp_ahb_clk = {
	.halt_reg = 0xc094,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc094,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_icp_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_icp_clk = {
	.halt_reg = 0xc08c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc08c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_icp_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_icp_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_ahb_clk = {
	.halt_reg = 0xa088,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa088,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_areg_clk = {
	.halt_reg = 0xa030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_areg_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_fast_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_axi_clk = {
	.halt_reg = 0xa084,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa084,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_axi_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_camnoc_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_clk = {
	.halt_reg = 0xa028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_cphy_rx_clk = {
	.halt_reg = 0xa080,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa080,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_cphy_rx_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_csid_clk = {
	.halt_reg = 0xa058,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_csid_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_0_csid_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_0_dsp_clk = {
	.halt_reg = 0xa03c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa03c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_0_dsp_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_ahb_clk = {
	.halt_reg = 0xb068,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb068,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_areg_clk = {
	.halt_reg = 0xb030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_areg_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_fast_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_axi_clk = {
	.halt_reg = 0xb064,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb064,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_axi_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_camnoc_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_clk = {
	.halt_reg = 0xb028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_cphy_rx_clk = {
	.halt_reg = 0xb060,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb060,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_cphy_rx_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_csid_clk = {
	.halt_reg = 0xb058,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_csid_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_1_csid_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_1_dsp_clk = {
	.halt_reg = 0xb03c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb03c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_1_dsp_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_2_ahb_clk = {
	.halt_reg = 0xb0d0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb0d0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_2_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_2_areg_clk = {
	.halt_reg = 0xb09c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb09c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_2_areg_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_fast_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_2_axi_clk = {
	.halt_reg = 0xb0cc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb0cc,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_2_axi_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_camnoc_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_2_clk = {
	.halt_reg = 0xb094,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb094,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_2_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_2_cphy_rx_clk = {
	.halt_reg = 0xb0c8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb0c8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_2_cphy_rx_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_2_csid_clk = {
	.halt_reg = 0xb0c0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb0c0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_2_csid_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_2_csid_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_lite_ahb_clk = {
	.halt_reg = 0xc040,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_lite_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_lite_axi_clk = {
	.halt_reg = 0xc044,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc044,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_lite_axi_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_camnoc_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_lite_clk = {
	.halt_reg = 0xc018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_lite_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_lite_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_lite_cphy_rx_clk = {
	.halt_reg = 0xc03c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc03c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_lite_cphy_rx_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ife_lite_csid_clk = {
	.halt_reg = 0xc034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ife_lite_csid_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_lite_csid_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_0_ahb_clk = {
	.halt_reg = 0x8040,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_0_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_0_areg_clk = {
	.halt_reg = 0x803c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x803c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_0_areg_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_fast_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_0_axi_clk = {
	.halt_reg = 0x8038,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8038,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_0_axi_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_camnoc_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_ipe_0_clk = {
	.halt_reg = 0x8028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_ipe_0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ipe_0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_jpeg_clk = {
	.halt_reg = 0xc060,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc060,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_jpeg_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_jpeg_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk0_clk = {
	.halt_reg = 0xe018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_mclk0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk1_clk = {
	.halt_reg = 0xe034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk1_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_mclk1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk2_clk = {
	.halt_reg = 0xe050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk2_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_mclk2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk3_clk = {
	.halt_reg = 0xe06c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe06c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk3_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_mclk3_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk4_clk = {
	.halt_reg = 0xe088,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe088,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk4_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_mclk4_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_mclk5_clk = {
	.halt_reg = 0xe0a4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe0a4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_mclk5_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_mclk5_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_ahb_clk = {
	.halt_reg = 0x9048,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_slow_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_axi_clk = {
	.halt_reg = 0x9044,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9044,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_axi_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_camnoc_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_clk = {
	.halt_reg = 0x9014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_sbi_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_cphy_rx_0_clk = {
	.halt_reg = 0x903c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x903c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_cphy_rx_0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_cphy_rx_1_clk = {
	.halt_reg = 0x9040,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_cphy_rx_1_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_cphy_rx_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_csid_0_clk = {
	.halt_reg = 0x9034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_csid_0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_sbi_csid_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_csid_1_clk = {
	.halt_reg = 0x9038,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9038,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_csid_1_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_sbi_csid_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_ife_0_clk = {
	.halt_reg = 0x904c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x904c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_ife_0_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_ife_1_clk = {
	.halt_reg = 0x9050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_ife_1_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sbi_ife_2_clk = {
	.halt_reg = 0x9054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sbi_ife_2_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_ife_2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_cc_sleep_clk = {
	.halt_reg = 0xc164,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc164,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "cam_cc_sleep_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &cam_cc_sleep_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *cam_cc_lahaina_clocks[] = {
	[CAM_CC_BPS_AHB_CLK] = &cam_cc_bps_ahb_clk.clkr,
	[CAM_CC_BPS_AREG_CLK] = &cam_cc_bps_areg_clk.clkr,
	[CAM_CC_BPS_AXI_CLK] = &cam_cc_bps_axi_clk.clkr,
	[CAM_CC_BPS_CLK] = &cam_cc_bps_clk.clkr,
	[CAM_CC_BPS_CLK_SRC] = &cam_cc_bps_clk_src.clkr,
	[CAM_CC_CAMNOC_AXI_CLK] = &cam_cc_camnoc_axi_clk.clkr,
	[CAM_CC_CAMNOC_AXI_CLK_SRC] = &cam_cc_camnoc_axi_clk_src.clkr,
	[CAM_CC_CAMNOC_DCD_XO_CLK] = &cam_cc_camnoc_dcd_xo_clk.clkr,
	[CAM_CC_CCI_0_CLK] = &cam_cc_cci_0_clk.clkr,
	[CAM_CC_CCI_0_CLK_SRC] = &cam_cc_cci_0_clk_src.clkr,
	[CAM_CC_CCI_1_CLK] = &cam_cc_cci_1_clk.clkr,
	[CAM_CC_CCI_1_CLK_SRC] = &cam_cc_cci_1_clk_src.clkr,
	[CAM_CC_CORE_AHB_CLK] = &cam_cc_core_ahb_clk.clkr,
	[CAM_CC_CPAS_AHB_CLK] = &cam_cc_cpas_ahb_clk.clkr,
	[CAM_CC_CPHY_RX_CLK_SRC] = &cam_cc_cphy_rx_clk_src.clkr,
	[CAM_CC_CSI0PHYTIMER_CLK] = &cam_cc_csi0phytimer_clk.clkr,
	[CAM_CC_CSI0PHYTIMER_CLK_SRC] = &cam_cc_csi0phytimer_clk_src.clkr,
	[CAM_CC_CSI1PHYTIMER_CLK] = &cam_cc_csi1phytimer_clk.clkr,
	[CAM_CC_CSI1PHYTIMER_CLK_SRC] = &cam_cc_csi1phytimer_clk_src.clkr,
	[CAM_CC_CSI2PHYTIMER_CLK] = &cam_cc_csi2phytimer_clk.clkr,
	[CAM_CC_CSI2PHYTIMER_CLK_SRC] = &cam_cc_csi2phytimer_clk_src.clkr,
	[CAM_CC_CSI3PHYTIMER_CLK] = &cam_cc_csi3phytimer_clk.clkr,
	[CAM_CC_CSI3PHYTIMER_CLK_SRC] = &cam_cc_csi3phytimer_clk_src.clkr,
	[CAM_CC_CSI4PHYTIMER_CLK] = &cam_cc_csi4phytimer_clk.clkr,
	[CAM_CC_CSI4PHYTIMER_CLK_SRC] = &cam_cc_csi4phytimer_clk_src.clkr,
	[CAM_CC_CSI5PHYTIMER_CLK] = &cam_cc_csi5phytimer_clk.clkr,
	[CAM_CC_CSI5PHYTIMER_CLK_SRC] = &cam_cc_csi5phytimer_clk_src.clkr,
	[CAM_CC_CSIPHY0_CLK] = &cam_cc_csiphy0_clk.clkr,
	[CAM_CC_CSIPHY1_CLK] = &cam_cc_csiphy1_clk.clkr,
	[CAM_CC_CSIPHY2_CLK] = &cam_cc_csiphy2_clk.clkr,
	[CAM_CC_CSIPHY3_CLK] = &cam_cc_csiphy3_clk.clkr,
	[CAM_CC_CSIPHY4_CLK] = &cam_cc_csiphy4_clk.clkr,
	[CAM_CC_CSIPHY5_CLK] = &cam_cc_csiphy5_clk.clkr,
	[CAM_CC_FAST_AHB_CLK_SRC] = &cam_cc_fast_ahb_clk_src.clkr,
	[CAM_CC_ICP_AHB_CLK] = &cam_cc_icp_ahb_clk.clkr,
	[CAM_CC_ICP_CLK] = &cam_cc_icp_clk.clkr,
	[CAM_CC_ICP_CLK_SRC] = &cam_cc_icp_clk_src.clkr,
	[CAM_CC_IFE_0_AHB_CLK] = &cam_cc_ife_0_ahb_clk.clkr,
	[CAM_CC_IFE_0_AREG_CLK] = &cam_cc_ife_0_areg_clk.clkr,
	[CAM_CC_IFE_0_AXI_CLK] = &cam_cc_ife_0_axi_clk.clkr,
	[CAM_CC_IFE_0_CLK] = &cam_cc_ife_0_clk.clkr,
	[CAM_CC_IFE_0_CLK_SRC] = &cam_cc_ife_0_clk_src.clkr,
	[CAM_CC_IFE_0_CPHY_RX_CLK] = &cam_cc_ife_0_cphy_rx_clk.clkr,
	[CAM_CC_IFE_0_CSID_CLK] = &cam_cc_ife_0_csid_clk.clkr,
	[CAM_CC_IFE_0_CSID_CLK_SRC] = &cam_cc_ife_0_csid_clk_src.clkr,
	[CAM_CC_IFE_0_DSP_CLK] = &cam_cc_ife_0_dsp_clk.clkr,
	[CAM_CC_IFE_1_AHB_CLK] = &cam_cc_ife_1_ahb_clk.clkr,
	[CAM_CC_IFE_1_AREG_CLK] = &cam_cc_ife_1_areg_clk.clkr,
	[CAM_CC_IFE_1_AXI_CLK] = &cam_cc_ife_1_axi_clk.clkr,
	[CAM_CC_IFE_1_CLK] = &cam_cc_ife_1_clk.clkr,
	[CAM_CC_IFE_1_CLK_SRC] = &cam_cc_ife_1_clk_src.clkr,
	[CAM_CC_IFE_1_CPHY_RX_CLK] = &cam_cc_ife_1_cphy_rx_clk.clkr,
	[CAM_CC_IFE_1_CSID_CLK] = &cam_cc_ife_1_csid_clk.clkr,
	[CAM_CC_IFE_1_CSID_CLK_SRC] = &cam_cc_ife_1_csid_clk_src.clkr,
	[CAM_CC_IFE_1_DSP_CLK] = &cam_cc_ife_1_dsp_clk.clkr,
	[CAM_CC_IFE_2_AHB_CLK] = &cam_cc_ife_2_ahb_clk.clkr,
	[CAM_CC_IFE_2_AREG_CLK] = &cam_cc_ife_2_areg_clk.clkr,
	[CAM_CC_IFE_2_AXI_CLK] = &cam_cc_ife_2_axi_clk.clkr,
	[CAM_CC_IFE_2_CLK] = &cam_cc_ife_2_clk.clkr,
	[CAM_CC_IFE_2_CLK_SRC] = &cam_cc_ife_2_clk_src.clkr,
	[CAM_CC_IFE_2_CPHY_RX_CLK] = &cam_cc_ife_2_cphy_rx_clk.clkr,
	[CAM_CC_IFE_2_CSID_CLK] = &cam_cc_ife_2_csid_clk.clkr,
	[CAM_CC_IFE_2_CSID_CLK_SRC] = &cam_cc_ife_2_csid_clk_src.clkr,
	[CAM_CC_IFE_LITE_AHB_CLK] = &cam_cc_ife_lite_ahb_clk.clkr,
	[CAM_CC_IFE_LITE_AXI_CLK] = &cam_cc_ife_lite_axi_clk.clkr,
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
	[CAM_CC_JPEG_CLK] = &cam_cc_jpeg_clk.clkr,
	[CAM_CC_JPEG_CLK_SRC] = &cam_cc_jpeg_clk_src.clkr,
	[CAM_CC_MCLK0_CLK] = &cam_cc_mclk0_clk.clkr,
	[CAM_CC_MCLK0_CLK_SRC] = &cam_cc_mclk0_clk_src.clkr,
	[CAM_CC_MCLK1_CLK] = &cam_cc_mclk1_clk.clkr,
	[CAM_CC_MCLK1_CLK_SRC] = &cam_cc_mclk1_clk_src.clkr,
	[CAM_CC_MCLK2_CLK] = &cam_cc_mclk2_clk.clkr,
	[CAM_CC_MCLK2_CLK_SRC] = &cam_cc_mclk2_clk_src.clkr,
	[CAM_CC_MCLK3_CLK] = &cam_cc_mclk3_clk.clkr,
	[CAM_CC_MCLK3_CLK_SRC] = &cam_cc_mclk3_clk_src.clkr,
	[CAM_CC_MCLK4_CLK] = &cam_cc_mclk4_clk.clkr,
	[CAM_CC_MCLK4_CLK_SRC] = &cam_cc_mclk4_clk_src.clkr,
	[CAM_CC_MCLK5_CLK] = &cam_cc_mclk5_clk.clkr,
	[CAM_CC_MCLK5_CLK_SRC] = &cam_cc_mclk5_clk_src.clkr,
	[CAM_CC_PLL0] = &cam_cc_pll0.clkr,
	[CAM_CC_PLL0_OUT_EVEN] = &cam_cc_pll0_out_even.clkr,
	[CAM_CC_PLL0_OUT_ODD] = &cam_cc_pll0_out_odd.clkr,
	[CAM_CC_PLL1] = &cam_cc_pll1.clkr,
	[CAM_CC_PLL1_OUT_EVEN] = &cam_cc_pll1_out_even.clkr,
	[CAM_CC_PLL2] = &cam_cc_pll2.clkr,
	[CAM_CC_PLL3] = &cam_cc_pll3.clkr,
	[CAM_CC_PLL3_OUT_EVEN] = &cam_cc_pll3_out_even.clkr,
	[CAM_CC_PLL4] = &cam_cc_pll4.clkr,
	[CAM_CC_PLL4_OUT_EVEN] = &cam_cc_pll4_out_even.clkr,
	[CAM_CC_PLL5] = &cam_cc_pll5.clkr,
	[CAM_CC_PLL5_OUT_EVEN] = &cam_cc_pll5_out_even.clkr,
	[CAM_CC_PLL6] = &cam_cc_pll6.clkr,
	[CAM_CC_PLL6_OUT_EVEN] = &cam_cc_pll6_out_even.clkr,
	[CAM_CC_SBI_AHB_CLK] = &cam_cc_sbi_ahb_clk.clkr,
	[CAM_CC_SBI_AXI_CLK] = &cam_cc_sbi_axi_clk.clkr,
	[CAM_CC_SBI_CLK] = &cam_cc_sbi_clk.clkr,
	[CAM_CC_SBI_CPHY_RX_0_CLK] = &cam_cc_sbi_cphy_rx_0_clk.clkr,
	[CAM_CC_SBI_CPHY_RX_1_CLK] = &cam_cc_sbi_cphy_rx_1_clk.clkr,
	[CAM_CC_SBI_CSID_0_CLK] = &cam_cc_sbi_csid_0_clk.clkr,
	[CAM_CC_SBI_CSID_1_CLK] = &cam_cc_sbi_csid_1_clk.clkr,
	[CAM_CC_SBI_CSID_CLK_SRC] = &cam_cc_sbi_csid_clk_src.clkr,
	[CAM_CC_SBI_DIV_CLK_SRC] = &cam_cc_sbi_div_clk_src.clkr,
	[CAM_CC_SBI_IFE_0_CLK] = &cam_cc_sbi_ife_0_clk.clkr,
	[CAM_CC_SBI_IFE_1_CLK] = &cam_cc_sbi_ife_1_clk.clkr,
	[CAM_CC_SBI_IFE_2_CLK] = &cam_cc_sbi_ife_2_clk.clkr,
	[CAM_CC_SLEEP_CLK] = &cam_cc_sleep_clk.clkr,
	[CAM_CC_SLEEP_CLK_SRC] = &cam_cc_sleep_clk_src.clkr,
	[CAM_CC_SLOW_AHB_CLK_SRC] = &cam_cc_slow_ahb_clk_src.clkr,
	[CAM_CC_XO_CLK_SRC] = &cam_cc_xo_clk_src.clkr,
};

static const struct qcom_reset_map cam_cc_lahaina_resets[] = {
	[CAM_CC_BPS_BCR] = { 0x7000 },
	[CAM_CC_ICP_BCR] = { 0xc070 },
	[CAM_CC_IFE_0_BCR] = { 0xa000 },
	[CAM_CC_IFE_1_BCR] = { 0xb000 },
	[CAM_CC_IFE_2_BCR] = { 0xb06c },
	[CAM_CC_IPE_0_BCR] = { 0x8000 },
	[CAM_CC_SBI_BCR] = { 0x9000 },
};

static const struct regmap_config cam_cc_lahaina_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xf008,
	.fast_io = true,
};

static struct qcom_cc_desc cam_cc_lahaina_desc = {
	.config = &cam_cc_lahaina_regmap_config,
	.clks = cam_cc_lahaina_clocks,
	.num_clks = ARRAY_SIZE(cam_cc_lahaina_clocks),
	.resets = cam_cc_lahaina_resets,
	.num_resets = ARRAY_SIZE(cam_cc_lahaina_resets),
	.clk_regulators = cam_cc_lahaina_regulators,
	.num_clk_regulators = ARRAY_SIZE(cam_cc_lahaina_regulators),
};

static const struct of_device_id cam_cc_lahaina_match_table[] = {
	{ .compatible = "qcom,lahaina-camcc" },
	{ .compatible = "qcom,lahaina-camcc-v2" },
	{ }
};
MODULE_DEVICE_TABLE(of, cam_cc_lahaina_match_table);

static void cam_cc_lahaina_fixup_lahainav2(struct regmap *regmap)
{
	clk_zonda_5lpe_pll_configure(&cam_cc_pll2, regmap,
		&cam_cc_pll2_config_sm8350_v2);
}

static int cam_cc_lahaina_fixup(struct platform_device *pdev,
	struct regmap *regmap)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,lahaina-camcc-v2"))
		cam_cc_lahaina_fixup_lahainav2(regmap);

	return 0;
}

static int cam_cc_lahaina_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &cam_cc_lahaina_desc);
	if (IS_ERR(regmap)) {
		dev_err(&pdev->dev, "Failed to map cam CC registers\n");
		return PTR_ERR(regmap);
	}

	ret = qcom_cc_runtime_init(pdev, &cam_cc_lahaina_desc);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret)
		return ret;

	clk_lucid_5lpe_pll_configure(&cam_cc_pll0, regmap, &cam_cc_pll0_config);
	clk_lucid_5lpe_pll_configure(&cam_cc_pll1, regmap, &cam_cc_pll1_config);
	clk_zonda_5lpe_pll_configure(&cam_cc_pll2, regmap, &cam_cc_pll2_config);
	clk_lucid_5lpe_pll_configure(&cam_cc_pll3, regmap, &cam_cc_pll3_config);
	clk_lucid_5lpe_pll_configure(&cam_cc_pll4, regmap, &cam_cc_pll4_config);
	clk_lucid_5lpe_pll_configure(&cam_cc_pll5, regmap, &cam_cc_pll5_config);
	clk_lucid_5lpe_pll_configure(&cam_cc_pll6, regmap, &cam_cc_pll6_config);

	ret = cam_cc_lahaina_fixup(pdev, regmap);
	if (ret)
		return ret;

	/*
	 * Keep clocks always enabled:
	 *	cam_cc_gdsc_clk
	 */
	regmap_update_bits(regmap, 0xc148, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(pdev, &cam_cc_lahaina_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register CAM CC clocks\n");
		return ret;
	}

	pm_runtime_put_sync(&pdev->dev);
	dev_info(&pdev->dev, "Registered CAM CC clocks\n");

	return ret;
}

static void cam_cc_lahaina_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &cam_cc_lahaina_desc);
}

static const struct dev_pm_ops cam_cc_lahaina_pm_ops = {
	SET_RUNTIME_PM_OPS(qcom_cc_runtime_suspend, qcom_cc_runtime_resume, NULL)
};

static struct platform_driver cam_cc_lahaina_driver = {
	.probe = cam_cc_lahaina_probe,
	.driver = {
		.name = "lahaina-cam_cc",
		.of_match_table = cam_cc_lahaina_match_table,
		.sync_state = cam_cc_lahaina_sync_state,
		.pm = &cam_cc_lahaina_pm_ops,
	},
};

static int __init cam_cc_lahaina_init(void)
{
	return platform_driver_register(&cam_cc_lahaina_driver);
}
subsys_initcall(cam_cc_lahaina_init);

static void __exit cam_cc_lahaina_exit(void)
{
	platform_driver_unregister(&cam_cc_lahaina_driver);
}
module_exit(cam_cc_lahaina_exit);

MODULE_DESCRIPTION("QTI CAM_CC LAHAINA Driver");
MODULE_LICENSE("GPL v2");

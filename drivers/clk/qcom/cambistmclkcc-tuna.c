// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

#include <dt-bindings/clock/qcom,cambistmclkcc-sun.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_mx, VDD_LOW + 1, 1, vdd_corner);

static struct clk_vdd_class *cam_bist_mclk_cc_tuna_regulators[] = {
	&vdd_mx,
};

enum {
	P_BI_TCXO,
	P_CAM_BIST_MCLK_CC_PLL0_OUT_EVEN,
	P_CAM_BIST_MCLK_CC_PLL0_OUT_MAIN,
	P_SLEEP_CLK,
};

static const struct pll_vco rivian_ole_vco[] = {
	{ 777000000, 1285000000, 0 },
};

/* 960.0 MHz Configuration */
static const struct alpha_pll_config cam_bist_mclk_cc_pll0_config = {
	.l = 0x32,
	.cal_l = 0x32,
	.alpha = 0x0,
	.config_ctl_val = 0x10000030,
	.config_ctl_hi_val = 0x80890263,
	.config_ctl_hi1_val = 0x00000217,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00000000,
};

static struct clk_alpha_pll cam_bist_mclk_cc_pll0 = {
	.offset = 0x0,
	.vco_table = rivian_ole_vco,
	.num_vco = ARRAY_SIZE(rivian_ole_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_RIVIAN_OLE],
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "cam_bist_mclk_cc_pll0",
			.parent_data = &(const struct clk_parent_data) {
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_rivian_ole_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_LOW] = 1285000000},
		},
	},
};

static const struct parent_map cam_bist_mclk_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_CAM_BIST_MCLK_CC_PLL0_OUT_EVEN, 3 },
	{ P_CAM_BIST_MCLK_CC_PLL0_OUT_MAIN, 5 },
};

static const struct clk_parent_data cam_bist_mclk_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &cam_bist_mclk_cc_pll0.clkr.hw },
	{ .hw = &cam_bist_mclk_cc_pll0.clkr.hw },
};

static const struct parent_map cam_bist_mclk_cc_parent_map_1[] = {
	{ P_SLEEP_CLK, 0 },
};

static const struct clk_parent_data cam_bist_mclk_cc_parent_data_1_ao[] = {
	{ .fw_name = "sleep_clk" },
};

static const struct freq_tbl ftbl_cam_bist_mclk_cc_mclk0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(24000000, P_CAM_BIST_MCLK_CC_PLL0_OUT_EVEN, 10, 1, 4),
	F(68571429, P_CAM_BIST_MCLK_CC_PLL0_OUT_MAIN, 14, 0, 0),
	{ }
};

static struct clk_rcg2 cam_bist_mclk_cc_mclk0_clk_src = {
	.cmd_rcgr = 0x4000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_bist_mclk_cc_parent_map_0,
	.freq_tbl = ftbl_cam_bist_mclk_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "cam_bist_mclk_cc_mclk0_clk_src",
		.parent_data = cam_bist_mclk_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 68571429},
	},
};

static struct clk_rcg2 cam_bist_mclk_cc_mclk1_clk_src = {
	.cmd_rcgr = 0x401c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_bist_mclk_cc_parent_map_0,
	.freq_tbl = ftbl_cam_bist_mclk_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "cam_bist_mclk_cc_mclk1_clk_src",
		.parent_data = cam_bist_mclk_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 68571429},
	},
};

static struct clk_rcg2 cam_bist_mclk_cc_mclk2_clk_src = {
	.cmd_rcgr = 0x4038,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_bist_mclk_cc_parent_map_0,
	.freq_tbl = ftbl_cam_bist_mclk_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "cam_bist_mclk_cc_mclk2_clk_src",
		.parent_data = cam_bist_mclk_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 68571429},
	},
};

static struct clk_rcg2 cam_bist_mclk_cc_mclk3_clk_src = {
	.cmd_rcgr = 0x4054,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_bist_mclk_cc_parent_map_0,
	.freq_tbl = ftbl_cam_bist_mclk_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "cam_bist_mclk_cc_mclk3_clk_src",
		.parent_data = cam_bist_mclk_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 68571429},
	},
};

static struct clk_rcg2 cam_bist_mclk_cc_mclk4_clk_src = {
	.cmd_rcgr = 0x4070,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_bist_mclk_cc_parent_map_0,
	.freq_tbl = ftbl_cam_bist_mclk_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "cam_bist_mclk_cc_mclk4_clk_src",
		.parent_data = cam_bist_mclk_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 68571429},
	},
};

static struct clk_rcg2 cam_bist_mclk_cc_mclk5_clk_src = {
	.cmd_rcgr = 0x408c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_bist_mclk_cc_parent_map_0,
	.freq_tbl = ftbl_cam_bist_mclk_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "cam_bist_mclk_cc_mclk5_clk_src",
		.parent_data = cam_bist_mclk_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 68571429},
	},
};

static struct clk_rcg2 cam_bist_mclk_cc_mclk6_clk_src = {
	.cmd_rcgr = 0x40a8,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_bist_mclk_cc_parent_map_0,
	.freq_tbl = ftbl_cam_bist_mclk_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "cam_bist_mclk_cc_mclk6_clk_src",
		.parent_data = cam_bist_mclk_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 68571429},
	},
};

static struct clk_rcg2 cam_bist_mclk_cc_mclk7_clk_src = {
	.cmd_rcgr = 0x40c4,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = cam_bist_mclk_cc_parent_map_0,
	.freq_tbl = ftbl_cam_bist_mclk_cc_mclk0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "cam_bist_mclk_cc_mclk7_clk_src",
		.parent_data = cam_bist_mclk_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 68571429},
	},
};

static const struct freq_tbl ftbl_cam_bist_mclk_cc_sleep_clk_src[] = {
	F(32000, P_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 cam_bist_mclk_cc_sleep_clk_src = {
	.cmd_rcgr = 0x40e0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = cam_bist_mclk_cc_parent_map_1,
	.freq_tbl = ftbl_cam_bist_mclk_cc_sleep_clk_src,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "cam_bist_mclk_cc_sleep_clk_src",
		.parent_data = cam_bist_mclk_cc_parent_data_1_ao,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_parent_data_1_ao),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_branch cam_bist_mclk_cc_mclk0_clk = {
	.halt_reg = 0x4018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4018,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "cam_bist_mclk_cc_mclk0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&cam_bist_mclk_cc_mclk0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_bist_mclk_cc_mclk1_clk = {
	.halt_reg = 0x4034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4034,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "cam_bist_mclk_cc_mclk1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&cam_bist_mclk_cc_mclk1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_bist_mclk_cc_mclk2_clk = {
	.halt_reg = 0x4050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4050,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "cam_bist_mclk_cc_mclk2_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&cam_bist_mclk_cc_mclk2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_bist_mclk_cc_mclk3_clk = {
	.halt_reg = 0x406c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x406c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "cam_bist_mclk_cc_mclk3_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&cam_bist_mclk_cc_mclk3_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_bist_mclk_cc_mclk4_clk = {
	.halt_reg = 0x4088,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4088,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "cam_bist_mclk_cc_mclk4_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&cam_bist_mclk_cc_mclk4_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_bist_mclk_cc_mclk5_clk = {
	.halt_reg = 0x40a4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x40a4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "cam_bist_mclk_cc_mclk5_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&cam_bist_mclk_cc_mclk5_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_bist_mclk_cc_mclk6_clk = {
	.halt_reg = 0x40c0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x40c0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "cam_bist_mclk_cc_mclk6_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&cam_bist_mclk_cc_mclk6_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch cam_bist_mclk_cc_mclk7_clk = {
	.halt_reg = 0x40dc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x40dc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "cam_bist_mclk_cc_mclk7_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&cam_bist_mclk_cc_mclk7_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *cam_bist_mclk_cc_tuna_clocks[] = {
	[CAM_BIST_MCLK_CC_MCLK0_CLK] = &cam_bist_mclk_cc_mclk0_clk.clkr,
	[CAM_BIST_MCLK_CC_MCLK0_CLK_SRC] = &cam_bist_mclk_cc_mclk0_clk_src.clkr,
	[CAM_BIST_MCLK_CC_MCLK1_CLK] = &cam_bist_mclk_cc_mclk1_clk.clkr,
	[CAM_BIST_MCLK_CC_MCLK1_CLK_SRC] = &cam_bist_mclk_cc_mclk1_clk_src.clkr,
	[CAM_BIST_MCLK_CC_MCLK2_CLK] = &cam_bist_mclk_cc_mclk2_clk.clkr,
	[CAM_BIST_MCLK_CC_MCLK2_CLK_SRC] = &cam_bist_mclk_cc_mclk2_clk_src.clkr,
	[CAM_BIST_MCLK_CC_MCLK3_CLK] = &cam_bist_mclk_cc_mclk3_clk.clkr,
	[CAM_BIST_MCLK_CC_MCLK3_CLK_SRC] = &cam_bist_mclk_cc_mclk3_clk_src.clkr,
	[CAM_BIST_MCLK_CC_MCLK4_CLK] = &cam_bist_mclk_cc_mclk4_clk.clkr,
	[CAM_BIST_MCLK_CC_MCLK4_CLK_SRC] = &cam_bist_mclk_cc_mclk4_clk_src.clkr,
	[CAM_BIST_MCLK_CC_MCLK5_CLK] = &cam_bist_mclk_cc_mclk5_clk.clkr,
	[CAM_BIST_MCLK_CC_MCLK5_CLK_SRC] = &cam_bist_mclk_cc_mclk5_clk_src.clkr,
	[CAM_BIST_MCLK_CC_MCLK6_CLK] = &cam_bist_mclk_cc_mclk6_clk.clkr,
	[CAM_BIST_MCLK_CC_MCLK6_CLK_SRC] = &cam_bist_mclk_cc_mclk6_clk_src.clkr,
	[CAM_BIST_MCLK_CC_MCLK7_CLK] = &cam_bist_mclk_cc_mclk7_clk.clkr,
	[CAM_BIST_MCLK_CC_MCLK7_CLK_SRC] = &cam_bist_mclk_cc_mclk7_clk_src.clkr,
	[CAM_BIST_MCLK_CC_PLL0] = &cam_bist_mclk_cc_pll0.clkr,
	[CAM_BIST_MCLK_CC_SLEEP_CLK_SRC] = &cam_bist_mclk_cc_sleep_clk_src.clkr,
};

static const struct regmap_config cam_bist_mclk_cc_tuna_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x5010,
	.fast_io = true,
};

static struct qcom_cc_desc cam_bist_mclk_cc_tuna_desc = {
	.config = &cam_bist_mclk_cc_tuna_regmap_config,
	.clks = cam_bist_mclk_cc_tuna_clocks,
	.num_clks = ARRAY_SIZE(cam_bist_mclk_cc_tuna_clocks),
	.clk_regulators = cam_bist_mclk_cc_tuna_regulators,
	.num_clk_regulators = ARRAY_SIZE(cam_bist_mclk_cc_tuna_regulators),
};

static const struct of_device_id cam_bist_mclk_cc_tuna_match_table[] = {
	{ .compatible = "qcom,tuna-cambistmclkcc" },
	{ }
};
MODULE_DEVICE_TABLE(of, cam_bist_mclk_cc_tuna_match_table);

static int cam_bist_mclk_cc_tuna_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &cam_bist_mclk_cc_tuna_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = qcom_cc_runtime_init(pdev, &cam_bist_mclk_cc_tuna_desc);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret)
		return ret;

	clk_rivian_ole_pll_configure(&cam_bist_mclk_cc_pll0, regmap, &cam_bist_mclk_cc_pll0_config);

	/*
	 * Keep clocks always enabled:
	 *	cam_bist_mclk_cc_sleep_clk
	 */
	regmap_update_bits(regmap, 0x40f8, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(pdev, &cam_bist_mclk_cc_tuna_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register CAM BIST MCLK CC clocks\n");
		return ret;
	}

	pm_runtime_put_sync(&pdev->dev);
	dev_info(&pdev->dev, "Registered CAM BIST MCLK CC clocks\n");

	return ret;
}

static void cam_bist_mclk_cc_tuna_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &cam_bist_mclk_cc_tuna_desc);
}

static const struct dev_pm_ops cam_bist_mclk_cc_tuna_pm_ops = {
	SET_RUNTIME_PM_OPS(qcom_cc_runtime_suspend, qcom_cc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static struct platform_driver cam_bist_mclk_cc_tuna_driver = {
	.probe = cam_bist_mclk_cc_tuna_probe,
	.driver = {
		.name = "cam_bist_mclk_cc-tuna",
		.of_match_table = cam_bist_mclk_cc_tuna_match_table,
		.sync_state = cam_bist_mclk_cc_tuna_sync_state,
		.pm = &cam_bist_mclk_cc_tuna_pm_ops,
	},
};

static int __init cam_bist_mclk_cc_tuna_init(void)
{
	return platform_driver_register(&cam_bist_mclk_cc_tuna_driver);
}
subsys_initcall(cam_bist_mclk_cc_tuna_init);

static void __exit cam_bist_mclk_cc_tuna_exit(void)
{
	platform_driver_unregister(&cam_bist_mclk_cc_tuna_driver);
}
module_exit(cam_bist_mclk_cc_tuna_exit);

MODULE_DESCRIPTION("QTI CAM_BIST_MCLK_CC TUNA Driver");
MODULE_LICENSE("GPL");

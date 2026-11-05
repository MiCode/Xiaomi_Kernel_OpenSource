// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,gpucc-tuna.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "common.h"
#include "gdsc.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_LOW_L1 + 1, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_LOW_L1 + 1, 1, vdd_corner);

static struct clk_vdd_class *gpu_cc_tuna_regulators[] = {
	&vdd_cx,
	&vdd_mx,
};

enum {
	P_BI_TCXO,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_GPU_CC_PLL0_OUT_EVEN,
	P_GPU_CC_PLL0_OUT_MAIN,
	P_GPU_CC_PLL0_OUT_ODD,
};

static const struct pll_vco lucid_ole_vco[] = {
	{ 249600000, 2300000000, 0 },
};

static const struct alpha_pll_config gpu_cc_pll0_config = {
	.l = 0x34,
	.cal_l = 0x44,
	.cal_l_ringosc = 0x44,
	.alpha = 0x1555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00182261,
	.config_ctl_hi1_val = 0x82aa299c,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000003,
	.test_ctl_hi1_val = 0x00009000,
	.test_ctl_hi2_val = 0x00000034,
	.user_ctl_val = 0x00000400,
	.user_ctl_hi_val = 0x00000005,
};

static struct clk_alpha_pll gpu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = lucid_ole_vco,
	.num_vco = ARRAY_SIZE(lucid_ole_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_OLE],
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_pll0",
			.parent_data = &(const struct clk_parent_data) {
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ole_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_LOWER_D1] = 615000000,
				[VDD_LOW] = 1100000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000,
				[VDD_HIGH_L1] = 2300000000},
		},
	},
};

static const struct clk_div_table post_div_table_gpu_cc_pll0_out_even[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv gpu_cc_pll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 10,
	.post_div_table = post_div_table_gpu_cc_pll0_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_gpu_cc_pll0_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_OLE],
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "gpu_cc_pll0_out_even",
		.parent_hws = (const struct clk_hw*[]) {
			&gpu_cc_pll0.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_lucid_ole_ops,
	},
};

static const struct parent_map gpu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_MAIN, 1 },
	{ P_GPU_CC_PLL0_OUT_EVEN, 2 },
	{ P_GPU_CC_PLL0_OUT_ODD, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
};

static const struct clk_parent_data gpu_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .hw = &gpu_cc_pll0_out_even.clkr.hw },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .fw_name = "gpll0_out_main" },
	{ .fw_name = "gpll0_out_main_div" },
};

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(500000000, P_GPU_CC_PLL0_OUT_EVEN, 1, 0, 0),
	F(650000000, P_GPU_CC_PLL0_OUT_EVEN, 1, 0, 0),
	F(687500000, P_GPU_CC_PLL0_OUT_EVEN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_gmu_clk_src = {
	.cmd_rcgr = 0x9318,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_0,
	.freq_tbl = ftbl_gpu_cc_gmu_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "gpu_cc_gmu_clk_src",
		.parent_data = gpu_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_classes = gpu_cc_tuna_regulators,
		.num_vdd_classes = ARRAY_SIZE(gpu_cc_tuna_regulators),
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 500000000,
			[VDD_LOW] = 650000000,
			[VDD_LOW_L1] = 687500000},
	},
};

static const struct freq_tbl ftbl_gpu_cc_hub_clk_src[] = {
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	F(400000000, P_GPLL0_OUT_MAIN, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_hub_clk_src = {
	.cmd_rcgr = 0x93ec,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_0,
	.freq_tbl = ftbl_gpu_cc_hub_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "gpu_cc_hub_clk_src",
		.parent_data = gpu_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 200000000,
			[VDD_LOW] = 300000000,
			[VDD_LOW_L1] = 400000000},
	},
};

static struct clk_regmap_div gpu_cc_hub_div_clk_src = {
	.reg = 0x942c,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "gpu_cc_hub_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&gpu_cc_hub_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch gpu_cc_ahb_clk = {
	.halt_reg = 0x90bc,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x90bc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_ahb_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&gpu_cc_hub_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_accu_shift_clk = {
	.halt_reg = 0x910c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x910c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_cx_accu_shift_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_gmu_clk = {
	.halt_reg = 0x90d4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x90d4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_cx_gmu_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&gpu_cc_gmu_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_DONT_HOLD_STATE | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_cxo_clk = {
	.halt_reg = 0x90e4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x90e4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_cxo_clk",
			.flags = CLK_DONT_HOLD_STATE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_demet_clk = {
	.halt_reg = 0x9010,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x9010,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_demet_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_dpm_clk = {
	.halt_reg = 0x9110,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9110,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_dpm_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_freq_measure_clk = {
	.halt_reg = 0x900c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x900c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_freq_measure_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gx_accu_shift_clk = {
	.halt_reg = 0x9070,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x9070,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_gx_accu_shift_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gx_gmu_clk = {
	.halt_reg = 0x9060,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9060,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_gx_gmu_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&gpu_cc_gmu_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_hlos1_vote_gpu_smmu_clk = {
	.halt_reg = 0x7000,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x7000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_hlos1_vote_gpu_smmu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_hub_aon_clk = {
	.halt_reg = 0x93e8,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x93e8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_hub_aon_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&gpu_cc_hub_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_hub_cx_int_clk = {
	.halt_reg = 0x90e8,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x90e8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_hub_cx_int_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&gpu_cc_hub_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_DONT_HOLD_STATE | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_memnoc_gfx_clk = {
	.halt_reg = 0x90f4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x90f4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "gpu_cc_memnoc_gfx_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct gdsc gpu_cc_cx_gdsc = {
	.gdscr = 0x9080,
	.gds_hw_ctrl = 0x9094,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x8,
	.pd = {
		.name = "gpu_cc_cx_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = RETAIN_FF_ENABLE | VOTABLE,
	.supply = "vdd_cx",
};

static int gdsc_cx_do_nothing(struct generic_pm_domain *domain)
{
	return 0;
}

static struct gdsc gpu_cc_cx_smmu_gdsc = {
	.gdscr = 0x9080,
	.gds_hw_ctrl = 0x9094,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x8,
	.pd = {
		.name = "gpu_cc_cx_smmu_gdsc",
		.power_on = gdsc_cx_do_nothing,
		.power_off = gdsc_cx_do_nothing,
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = RETAIN_FF_ENABLE | VOTABLE,
	.parent = &gpu_cc_cx_gdsc.pd,
};

static struct gdsc gpu_cc_cx_gmu_gdsc = {
	.gdscr = 0x9080,
	.gds_hw_ctrl = 0x9094,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x8,
	.pd = {
		.name = "gpu_cc_cx_gmu_gdsc",
		.power_on = gdsc_cx_do_nothing,
		.power_off = gdsc_cx_do_nothing,
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = RETAIN_FF_ENABLE | VOTABLE,
	.parent = &gpu_cc_cx_gdsc.pd,
};

static struct gdsc gx_clkctl_gx_gdsc = {
	.gdscr = 0x0,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "gx_clkctl_gx_gdsc",
		.power_on = gdsc_gx_do_nothing_enable,
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
	.supply = "vdd_gx",
};

static struct clk_regmap *gpu_cc_tuna_clocks[] = {
	[GPU_CC_AHB_CLK] = &gpu_cc_ahb_clk.clkr,
	[GPU_CC_CX_ACCU_SHIFT_CLK] = &gpu_cc_cx_accu_shift_clk.clkr,
	[GPU_CC_CX_GMU_CLK] = &gpu_cc_cx_gmu_clk.clkr,
	[GPU_CC_CXO_CLK] = &gpu_cc_cxo_clk.clkr,
	[GPU_CC_DEMET_CLK] = &gpu_cc_demet_clk.clkr,
	[GPU_CC_DPM_CLK] = &gpu_cc_dpm_clk.clkr,
	[GPU_CC_FREQ_MEASURE_CLK] = &gpu_cc_freq_measure_clk.clkr,
	[GPU_CC_GMU_CLK_SRC] = &gpu_cc_gmu_clk_src.clkr,
	[GPU_CC_GX_ACCU_SHIFT_CLK] = &gpu_cc_gx_accu_shift_clk.clkr,
	[GPU_CC_GX_GMU_CLK] = &gpu_cc_gx_gmu_clk.clkr,
	[GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK] = &gpu_cc_hlos1_vote_gpu_smmu_clk.clkr,
	[GPU_CC_HUB_AON_CLK] = &gpu_cc_hub_aon_clk.clkr,
	[GPU_CC_HUB_CLK_SRC] = &gpu_cc_hub_clk_src.clkr,
	[GPU_CC_HUB_CX_INT_CLK] = &gpu_cc_hub_cx_int_clk.clkr,
	[GPU_CC_HUB_DIV_CLK_SRC] = &gpu_cc_hub_div_clk_src.clkr,
	[GPU_CC_MEMNOC_GFX_CLK] = &gpu_cc_memnoc_gfx_clk.clkr,
	[GPU_CC_PLL0] = &gpu_cc_pll0.clkr,
	[GPU_CC_PLL0_OUT_EVEN] = &gpu_cc_pll0_out_even.clkr,
};

static struct gdsc *gpu_cc_tuna_gdscs[] = {
	[GPU_CC_CX_GDSC] = &gpu_cc_cx_gdsc,
	[GPU_CC_CX_SMMU_GDSC] = &gpu_cc_cx_smmu_gdsc,
	[GPU_CC_CX_GMU_GDSC] = &gpu_cc_cx_gmu_gdsc,
};

static struct gdsc *gx_clkctl_tuna_gdscs[] = {
	[GX_CLKCTL_GX_GDSC] = &gx_clkctl_gx_gdsc,
};

static const struct qcom_reset_map gpu_cc_tuna_resets[] = {
	[GPU_CC_CB_BCR] = { 0x93a0 },
	[GPU_CC_CX_BCR] = { 0x907c },
	[GPU_CC_FAST_HUB_BCR] = { 0x93e4 },
	[GPU_CC_FF_BCR] = { 0x9470 },
	[GPU_CC_GMU_BCR] = { 0x9314 },
	[GPU_CC_GX_BCR] = { 0x905c },
	[GPU_CC_XO_BCR] = { 0x9000 },
};

static const struct regmap_config gpu_cc_tuna_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x964c,
	.fast_io = true,
};

static const struct regmap_config gx_clkctl_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x8,
	.fast_io = true,
};

static const struct qcom_cc_desc gpu_cc_tuna_desc = {
	.config = &gpu_cc_tuna_regmap_config,
	.clks = gpu_cc_tuna_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_tuna_clocks),
	.resets = gpu_cc_tuna_resets,
	.num_resets = ARRAY_SIZE(gpu_cc_tuna_resets),
	.clk_regulators = gpu_cc_tuna_regulators,
	.num_clk_regulators = ARRAY_SIZE(gpu_cc_tuna_regulators),
	.gdscs = gpu_cc_tuna_gdscs,
	.num_gdscs = ARRAY_SIZE(gpu_cc_tuna_gdscs),
};

static const struct qcom_cc_desc gx_clkctl_tuna_desc = {
	.config = &gx_clkctl_regmap_config,
	.gdscs = gx_clkctl_tuna_gdscs,
	.num_gdscs = ARRAY_SIZE(gx_clkctl_tuna_gdscs),
};

static const struct of_device_id gpu_cc_tuna_match_table[] = {
	{ .compatible = "qcom,tuna-gpucc" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_tuna_match_table);

static int gpu_cc_tuna_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &gpu_cc_tuna_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	clk_lucid_ole_pll_configure(&gpu_cc_pll0, regmap, &gpu_cc_pll0_config);

	/*
	 * Keep clocks always enabled:
	 *	gpu_cc_cb_clk
	 *	gpu_cc_cxo_aon_clk
	 *	gpu_cc_rscc_hub_aon_clk
	 *	gpu_cc_rscc_xo_aon_clk
	 *	gpu_cc_sleep_clk
	 */
	regmap_update_bits(regmap, 0x93a4, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x9008, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x93a8, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x9004, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x90cc, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(pdev, &gpu_cc_tuna_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered GPU CC clocks\n");

	return ret;
}

static void gpu_cc_tuna_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &gpu_cc_tuna_desc);
}

static struct platform_driver gpu_cc_tuna_driver = {
	.probe = gpu_cc_tuna_probe,
	.driver = {
		.name = "gpu_cc-tuna",
		.of_match_table = gpu_cc_tuna_match_table,
		.sync_state = gpu_cc_tuna_sync_state,
	},
};


static const struct of_device_id gx_clkctl_tuna_match_table[] = {
	{ .compatible = "qcom,tuna-gx_clkctl" },
	{ }
};
MODULE_DEVICE_TABLE(of, gx_clkctl_tuna_match_table);

static int gx_clkctl_tuna_probe(struct platform_device *pdev)
{

	struct regmap *regmap;
	int ret;

	pm_runtime_enable(&pdev->dev);

	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret)
		return ret;

	regmap = qcom_cc_map(pdev, &gx_clkctl_tuna_desc);
	if (IS_ERR(regmap)) {
		pm_runtime_put(&pdev->dev);
		return PTR_ERR(regmap);
	}

	ret = qcom_cc_really_probe(pdev, &gx_clkctl_tuna_desc, regmap);

	pm_runtime_put(&pdev->dev);

	return ret;
}

static struct platform_driver gx_clkctl_tuna_driver = {
	.probe = gx_clkctl_tuna_probe,
	.driver = {
		.name = "gx_clkctl-tuna",
		.of_match_table = gx_clkctl_tuna_match_table,
	},
};

static int __init gpu_cc_tuna_init(void)
{
	int ret;

	ret = platform_driver_register(&gpu_cc_tuna_driver);
	if (ret)
		return ret;

	return platform_driver_register(&gx_clkctl_tuna_driver);
}
subsys_initcall(gpu_cc_tuna_init);

static void __exit gpu_cc_tuna_exit(void)
{
	platform_driver_unregister(&gx_clkctl_tuna_driver);
	platform_driver_unregister(&gpu_cc_tuna_driver);
}
module_exit(gpu_cc_tuna_exit);

MODULE_DESCRIPTION("QTI GPU_CC TUNA Driver");
MODULE_LICENSE("GPL");

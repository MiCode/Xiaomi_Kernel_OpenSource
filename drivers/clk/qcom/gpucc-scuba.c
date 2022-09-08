// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,gpucc-scuba.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "common.h"
#include "reset.h"
#include "vdd-level-scuba.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_HIGH_L1 + 1, 1, vdd_corner);

static struct clk_vdd_class *gpu_cc_scuba_regulators[] = {
	&vdd_cx,
};

enum {
	P_BI_TCXO,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_GPU_CC_PLL0_2X_DIV_CLK_SRC,
	P_GPU_CC_PLL0_OUT_AUX,
	P_GPU_CC_PLL0_OUT_AUX2,
	P_GPU_CC_PLL0_OUT_MAIN,
};

static const struct pll_vco huayra_vco[] = {
	{ 600000000, 3300000000, 0 },
	{ 600000000, 2200000000, 1 },
};


/* 710.4 MHz Configuration */
static const struct alpha_pll_config gpu_cc_pll0_config = {
	.l = 0x25,
	.config_ctl_val = 0x200D4828,
	.config_ctl_hi_val = 0x6,
	.test_ctl_val = 0x1C000000,
	.test_ctl_hi_val = 0x00004000,
	.user_ctl_val = 0xF,
};

static struct clk_alpha_pll gpu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = huayra_vco,
	.num_vco = ARRAY_SIZE(huayra_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_HUAYRA],
	.clkr = {
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_huayra_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 1200000000,
				[VDD_LOWER] = 2400000000,
				[VDD_LOW] = 3000000000,
				[VDD_NOMINAL] = 3300000000},
		},
	},
};

static const struct parent_map gpu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_MAIN, 1 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
};

static const struct clk_parent_data gpu_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .fw_name = "gpll0_out_main" },
	{ .fw_name = "gpll0_out_main_div" },
};

static const struct parent_map gpu_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_2X_DIV_CLK_SRC, 1 },
	{ P_GPU_CC_PLL0_OUT_AUX2, 2 },
	{ P_GPU_CC_PLL0_OUT_AUX, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
};

static const struct clk_parent_data gpu_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .fw_name = "gpll0_out_main" },
};

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src[] = {
	F(200000000, P_GPLL0_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_gmu_clk_src = {
	.cmd_rcgr = 0x1120,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_0,
	.freq_tbl = ftbl_gpu_cc_gmu_clk_src,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "gpu_cc_gmu_clk_src",
		.parent_data = gpu_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_0),
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 200000000},
	},
};

static const struct freq_tbl ftbl_gpu_cc_gx_gfx3d_clk_src[] = {
	F(355200000, P_GPU_CC_PLL0_OUT_AUX, 2, 0, 0),
	F(537600000, P_GPU_CC_PLL0_OUT_AUX2, 2, 0, 0),
	F(672000000, P_GPU_CC_PLL0_OUT_AUX2, 2, 0, 0),
	F(844800000, P_GPU_CC_PLL0_OUT_AUX2, 2, 0, 0),
	F(921600000, P_GPU_CC_PLL0_OUT_AUX2, 2, 0, 0),
	F(1017600000, P_GPU_CC_PLL0_OUT_AUX2, 2, 0, 0),
	F(1123200000, P_GPU_CC_PLL0_OUT_AUX2, 2, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_gx_gfx3d_clk_src = {
	.cmd_rcgr = 0x101c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_1,
	.freq_tbl = ftbl_gpu_cc_gx_gfx3d_clk_src,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "gpu_cc_gx_gfx3d_clk_src",
		.parent_data = gpu_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 355200000,
			[VDD_LOW] = 537600000,
			[VDD_LOW_L1] = 672000000,
			[VDD_NOMINAL] = 844800000,
			[VDD_NOMINAL_L1] = 921600000,
			[VDD_HIGH] = 1017600000,
			[VDD_HIGH_L1] = 1123200000},
	},
};

static struct clk_branch gpu_cc_crc_ahb_clk = {
	.halt_reg = 0x107c,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x107c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_crc_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_gfx3d_clk = {
	.halt_reg = 0x10a4,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x10a4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_cx_gfx3d_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_gx_gfx3d_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_gmu_clk = {
	.halt_reg = 0x1098,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1098,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_cx_gmu_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_gmu_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_snoc_dvm_clk = {
	.halt_reg = 0x108c,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x108c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_cx_snoc_dvm_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cxo_aon_clk = {
	.halt_reg = 0x1004,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1004,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_cxo_aon_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cxo_clk = {
	.halt_reg = 0x109c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x109c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_cxo_clk",
			.flags = CLK_DONT_HOLD_STATE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gx_gfx3d_clk = {
	.halt_reg = 0x1054,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1054,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_gx_gfx3d_clk",
			.parent_hws = (const struct clk_hw*[]){
				&gpu_cc_gx_gfx3d_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_hlos1_vote_gpu_smmu_clk = {
	.halt_reg = 0x5000,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x5000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			 .name = "gpu_cc_hlos1_vote_gpu_smmu_clk",
			 .ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_sleep_clk = {
	.halt_reg = 0x1090,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x1090,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *gpu_cc_scuba_clocks[] = {
	[GPU_CC_CRC_AHB_CLK] = &gpu_cc_crc_ahb_clk.clkr,
	[GPU_CC_CX_GFX3D_CLK] = &gpu_cc_cx_gfx3d_clk.clkr,
	[GPU_CC_CX_GMU_CLK] = &gpu_cc_cx_gmu_clk.clkr,
	[GPU_CC_CX_SNOC_DVM_CLK] = &gpu_cc_cx_snoc_dvm_clk.clkr,
	[GPU_CC_CXO_AON_CLK] = &gpu_cc_cxo_aon_clk.clkr,
	[GPU_CC_CXO_CLK] = &gpu_cc_cxo_clk.clkr,
	[GPU_CC_GMU_CLK_SRC] = &gpu_cc_gmu_clk_src.clkr,
	[GPU_CC_GX_GFX3D_CLK] = &gpu_cc_gx_gfx3d_clk.clkr,
	[GPU_CC_GX_GFX3D_CLK_SRC] = &gpu_cc_gx_gfx3d_clk_src.clkr,
	[GPU_CC_PLL0] = &gpu_cc_pll0.clkr,
	[GPU_CC_SLEEP_CLK] = &gpu_cc_sleep_clk.clkr,
	[GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK] = &gpu_cc_hlos1_vote_gpu_smmu_clk.clkr,
};

static const struct regmap_config gpu_cc_scuba_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x7008,
	.fast_io = true,
};

static const struct qcom_cc_desc gpu_cc_scuba_desc = {
	.config = &gpu_cc_scuba_regmap_config,
	.clks = gpu_cc_scuba_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_scuba_clocks),
	.clk_regulators = gpu_cc_scuba_regulators,
	.num_clk_regulators = ARRAY_SIZE(gpu_cc_scuba_regulators),
};

static const struct of_device_id gpu_cc_scuba_match_table[] = {
	{ .compatible = "qcom,scuba-gpucc" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_scuba_match_table);

static int gpu_cc_scuba_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &gpu_cc_scuba_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	/*
	 * Keep the clocks always-ON
	 * GPU_CC_GX_CXO_CLK, GPU_CC_AHB_CLK
	 */
	regmap_update_bits(regmap, 0x1060, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x1078, BIT(0), BIT(0));

	clk_huayra_pll_configure(&gpu_cc_pll0, regmap, &gpu_cc_pll0_config);

	ret = qcom_cc_really_probe(pdev, &gpu_cc_scuba_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered GPU CC clocks\n");

	return ret;
}

static void gpu_cc_scuba_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &gpu_cc_scuba_desc);
}

static struct platform_driver gpu_cc_scuba_driver = {
	.probe = gpu_cc_scuba_probe,
	.driver = {
		.name = "gpu_cc-scuba",
		.of_match_table = gpu_cc_scuba_match_table,
		.sync_state = gpu_cc_scuba_sync_state,
	},
};

static int __init gpu_cc_scuba_init(void)
{
	return platform_driver_register(&gpu_cc_scuba_driver);
}
subsys_initcall(gpu_cc_scuba_init);

static void __exit gpu_cc_scuba_exit(void)
{
	platform_driver_unregister(&gpu_cc_scuba_driver);
}
module_exit(gpu_cc_scuba_exit);

MODULE_DESCRIPTION("QTI GPU_CC SCUBA Driver");
MODULE_LICENSE("GPL v2");

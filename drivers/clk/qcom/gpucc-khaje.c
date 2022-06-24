// SPDX-License-Identifier: GPL-2.0-only
/*
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

#include <dt-bindings/clock/qcom,gpucc-khaje.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "common.h"
#include "reset.h"
#include "vdd-level-bengal.h"

#define CX_GMU_CBCR_SLEEP_MASK		0xf
#define CX_GMU_CBCR_SLEEP_SHIFT		4
#define CX_GMU_CBCR_WAKE_MASK		0xf
#define CX_GMU_CBCR_WAKE_SHIFT		8

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_L2_HIGH_L2 + 1, 1, vdd_l2_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_HIGH_L1 + 1, 1, vdd_corner);

static struct clk_vdd_class *gpu_cc_khaje_regulators[] = {
	&vdd_cx,
	&vdd_mx,
};

enum {
	P_BI_TCXO,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_GPU_CC_PLL0_2X_DIV_CLK_SRC,
	P_GPU_CC_PLL0_OUT_MAIN,
	P_GPU_CC_PLL1_OUT_EVEN,
	P_GPU_CC_PLL1_OUT_MAIN,
	P_GPU_CC_PLL1_OUT_ODD,
};

static const struct pll_vco lucid_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static const struct pll_vco zonda_vco[] = {
	{ 595200000, 3600000000, 0 },
};

/* 640MHz configuration */
static const struct alpha_pll_config gpu_cc_pll0_config = {
	.l = 0x21,
	.alpha = 0x5555,
	.config_ctl_val = 0x08200800,
	.config_ctl_hi_val = 0x05022001,
	.config_ctl_hi1_val = 0x00000010,
	.user_ctl_val = 0x01000101,
};

static struct clk_alpha_pll gpu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = zonda_vco,
	.num_vco = ARRAY_SIZE(zonda_vco),
	.flags = SUPPORTS_DYNAMIC_UPDATE,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_ZONDA],
	.clkr = {
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_zonda_ops,
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

static const struct clk_div_table post_div_table_gpu_cc_pll0_out_main[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv gpu_cc_pll0_out_main = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_gpu_cc_pll0_out_main,
	.num_post_div = ARRAY_SIZE(post_div_table_gpu_cc_pll0_out_main),
	.width = 2,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_ZONDA],
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "gpu_cc_pll0_out_main",
		.parent_hws = (const struct clk_hw*[]){
			&gpu_cc_pll0.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_alpha_pll_postdiv_ro_ops,
	},
};

static const struct alpha_pll_config gpu_cc_pll1_config = {
	.l = 0x23,
	.cal_l = 0x44,
	.alpha = 0xF000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A299C,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll gpu_cc_pll1 = {
	.offset = 0x100,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.flags = SUPPORTS_DYNAMIC_UPDATE,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr = {
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_pll1",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mx,
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

static const struct parent_map gpu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_MAIN, 1 },
	{ P_GPU_CC_PLL1_OUT_MAIN, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
};

static const struct clk_parent_data gpu_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &gpu_cc_pll0_out_main.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .fw_name = "gpll0_out_main" },
	{ .fw_name = "gpll0_out_main_div" },
};

static const struct parent_map gpu_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_MAIN, 1 },
	{ P_GPU_CC_PLL0_2X_DIV_CLK_SRC, 2 },
	{ P_GPU_CC_PLL1_OUT_EVEN, 3 },
	{ P_GPU_CC_PLL1_OUT_ODD, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
};

static const struct clk_parent_data gpu_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &gpu_cc_pll0_out_main.clkr.hw },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
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
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_L2_NUM,
		.rate_max = (unsigned long[VDD_L2_NUM]) {
			[VDD_L2_LOWER] = 200000000},
	},
};

static const struct freq_tbl ftbl_gpu_cc_gx_gfx3d_clk_src[] = {
	F(320000000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(465000000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(600000000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(785000000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(820000000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(980000000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(1025000000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(1100000000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(1114800000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(1260000000, P_GPU_CC_PLL0_OUT_MAIN, 1, 0, 0),
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
		.num_rate_max = VDD_L2_NUM,
		.rate_max = (unsigned long[VDD_L2_NUM]) {
			[VDD_L2_LOWER] = 320000000,
			[VDD_L2_LOW] = 465000000,
			[VDD_L2_LOW_L1] = 600000000,
			[VDD_L2_NOMINAL] = 785000000,
			[VDD_L2_HIGH] = 1025000000,
			[VDD_L2_HIGH_L1] = 1114800000,
			[VDD_L2_HIGH_L2] = 1260000000},
	},
};

static struct clk_branch gpu_cc_ahb_clk = {
	.halt_reg = 0x1078,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1078,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_ahb_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
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
	.halt_check = BRANCH_HALT_SKIP,
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
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_hlos1_vote_gpu_smmu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_sleep_clk = {
	.halt_reg = 0x1090,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1090,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "gpu_cc_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *gpu_cc_khaje_clocks[] = {
	[GPU_CC_AHB_CLK] = &gpu_cc_ahb_clk.clkr,
	[GPU_CC_CRC_AHB_CLK] = &gpu_cc_crc_ahb_clk.clkr,
	[GPU_CC_CX_GFX3D_CLK] = &gpu_cc_cx_gfx3d_clk.clkr,
	[GPU_CC_CX_GMU_CLK] = &gpu_cc_cx_gmu_clk.clkr,
	[GPU_CC_CX_SNOC_DVM_CLK] = &gpu_cc_cx_snoc_dvm_clk.clkr,
	[GPU_CC_CXO_AON_CLK] = &gpu_cc_cxo_aon_clk.clkr,
	[GPU_CC_CXO_CLK] = &gpu_cc_cxo_clk.clkr,
	[GPU_CC_GMU_CLK_SRC] = &gpu_cc_gmu_clk_src.clkr,
	[GPU_CC_GX_GFX3D_CLK] = &gpu_cc_gx_gfx3d_clk.clkr,
	[GPU_CC_GX_GFX3D_CLK_SRC] = &gpu_cc_gx_gfx3d_clk_src.clkr,
	[GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK] = &gpu_cc_hlos1_vote_gpu_smmu_clk.clkr,
	[GPU_CC_PLL0] = &gpu_cc_pll0.clkr,
	[GPU_CC_PLL0_OUT_MAIN] = &gpu_cc_pll0_out_main.clkr,
	[GPU_CC_PLL1] = &gpu_cc_pll1.clkr,
	[GPU_CC_SLEEP_CLK] = &gpu_cc_sleep_clk.clkr,
};

static const struct qcom_reset_map gpu_cc_khaje_resets[] = {
	[GPUCC_GPU_CC_CX_BCR] = { 0x1068 },
	[GPUCC_GPU_CC_GFX3D_AON_BCR] = { 0x10a0 },
	[GPUCC_GPU_CC_GMU_BCR] = { 0x111c },
	[GPUCC_GPU_CC_GX_BCR] = { 0x1008 },
	[GPUCC_GPU_CC_XO_BCR] = { 0x1000 },
};

static const struct regmap_config gpu_cc_khaje_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x7008,
	.fast_io = true,
};

static const struct qcom_cc_desc gpu_cc_khaje_desc = {
	.config = &gpu_cc_khaje_regmap_config,
	.clks = gpu_cc_khaje_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_khaje_clocks),
	.resets = gpu_cc_khaje_resets,
	.num_resets = ARRAY_SIZE(gpu_cc_khaje_resets),
	.clk_regulators = gpu_cc_khaje_regulators,
	.num_clk_regulators = ARRAY_SIZE(gpu_cc_khaje_regulators),
};

static const struct of_device_id gpu_cc_khaje_match_table[] = {
	{ .compatible = "qcom,khaje-gpucc" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_khaje_match_table);

static int gpu_cc_khaje_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	unsigned int value, mask;
	int ret;

	regmap = qcom_cc_map(pdev, &gpu_cc_khaje_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	/*
	 * Keep the clock always-ON
	 * GPU_CC_GX_CXO_CLK
	 */
	regmap_update_bits(regmap, 0x1060, BIT(0), BIT(0));

	clk_zonda_pll_configure(&gpu_cc_pll0, regmap, &gpu_cc_pll0_config);
	clk_lucid_pll_configure(&gpu_cc_pll1, regmap, &gpu_cc_pll1_config);

	/* Recommended WAKEUP/SLEEP settings for the gpu_cc_cx_gmu_clk */
	mask = CX_GMU_CBCR_WAKE_MASK << CX_GMU_CBCR_WAKE_SHIFT;
	mask |= CX_GMU_CBCR_SLEEP_MASK << CX_GMU_CBCR_SLEEP_SHIFT;
	value = 0xf << CX_GMU_CBCR_WAKE_SHIFT | 0xf << CX_GMU_CBCR_SLEEP_SHIFT;
	regmap_update_bits(regmap, gpu_cc_cx_gmu_clk.clkr.enable_reg,
								mask, value);

	ret = qcom_cc_really_probe(pdev, &gpu_cc_khaje_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered GPU CC clocks\n");

	return ret;
}

static void gpu_cc_khaje_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &gpu_cc_khaje_desc);
}

static struct platform_driver gpu_cc_khaje_driver = {
	.probe = gpu_cc_khaje_probe,
	.driver = {
		.name = "gpu_cc-khaje",
		.of_match_table = gpu_cc_khaje_match_table,
		.sync_state = gpu_cc_khaje_sync_state,
	},
};

static int __init gpu_cc_khaje_init(void)
{
	return platform_driver_register(&gpu_cc_khaje_driver);
}
subsys_initcall(gpu_cc_khaje_init);

static void __exit gpu_cc_khaje_exit(void)
{
	platform_driver_unregister(&gpu_cc_khaje_driver);
}
module_exit(gpu_cc_khaje_exit);

MODULE_DESCRIPTION("QTI GPU_CC KHAJE Driver");
MODULE_LICENSE("GPL v2");

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
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include <dt-bindings/clock/qcom,gpucc-lito.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "common.h"
#include "reset.h"
#include "vdd-level-lito.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_NUM, 1, vdd_corner);

#define CX_GMU_CBCR_SLEEP_MASK		0xF
#define CX_GMU_CBCR_SLEEP_SHIFT		4
#define CX_GMU_CBCR_WAKE_MASK		0xF
#define CX_GMU_CBCR_WAKE_SHIFT		8

enum {
	P_BI_TCXO,
	P_CORE_BI_PLL_TEST_SE,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_GPU_CC_PLL0_OUT_MAIN,
	P_GPU_CC_PLL1_OUT_MAIN,
};

static const struct parent_map gpu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_MAIN, 1 },
	{ P_GPU_CC_PLL1_OUT_MAIN, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gpu_cc_parent_names_0[] = {
	"bi_tcxo",
	"gpu_cc_pll0",
	"gpu_cc_pll1",
	"gcc_gpu_gpll0_clk_src",
	"gcc_gpu_gpll0_div_clk_src",
	"core_bi_pll_test_se",
};

static struct pll_vco lucid_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static const struct alpha_pll_config gpu_cc_pll1_config = {
	.l = 0x1A,
	.cal_l = 0x44,
	.alpha = 0xAAA,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
};

static struct clk_alpha_pll gpu_cc_pll1 = {
	.offset = 0x100,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll1",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ops,
			.vdd_class = &vdd_mx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000},
		},
	},
};

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN_DIV, 1.5, 0, 0),
	F(500000000, P_GPU_CC_PLL1_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_gmu_clk_src = {
	.cmd_rcgr = 0x1120,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_0,
	.freq_tbl = ftbl_gpu_cc_gmu_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_gmu_clk_src",
		.parent_names = gpu_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 200000000,
			[VDD_LOW] = 500000000},
	},
};

static struct clk_branch gpu_cc_ahb_clk = {
	.halt_reg = 0x1078,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1078,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
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
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_crc_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_apb_clk = {
	.halt_reg = 0x1088,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1088,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_apb_clk",
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
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_gmu_clk",
			.parent_names = (const char *[]){
				"gpu_cc_gmu_clk_src",
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
		.hw.init = &(struct clk_init_data){
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
		.hw.init = &(struct clk_init_data){
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
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cxo_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gx_gmu_clk = {
	.halt_reg = 0x1064,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1064,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gx_gmu_clk",
			.parent_names = (const char *[]){
				"gpu_cc_gmu_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gx_vsense_clk = {
	.halt_reg = 0x1058,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x1058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gx_vsense_clk",
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
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_sleep_clk",
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

static struct clk_regmap *gpu_cc_lito_clocks[] = {
	[GPU_CC_AHB_CLK] = &gpu_cc_ahb_clk.clkr,
	[GPU_CC_CRC_AHB_CLK] = &gpu_cc_crc_ahb_clk.clkr,
	[GPU_CC_CX_APB_CLK] = &gpu_cc_cx_apb_clk.clkr,
	[GPU_CC_CX_GMU_CLK] = &gpu_cc_cx_gmu_clk.clkr,
	[GPU_CC_CX_SNOC_DVM_CLK] = &gpu_cc_cx_snoc_dvm_clk.clkr,
	[GPU_CC_CXO_AON_CLK] = &gpu_cc_cxo_aon_clk.clkr,
	[GPU_CC_CXO_CLK] = &gpu_cc_cxo_clk.clkr,
	[GPU_CC_GMU_CLK_SRC] = &gpu_cc_gmu_clk_src.clkr,
	[GPU_CC_GX_GMU_CLK] = &gpu_cc_gx_gmu_clk.clkr,
	[GPU_CC_GX_VSENSE_CLK] = &gpu_cc_gx_vsense_clk.clkr,
	[GPU_CC_PLL1] = &gpu_cc_pll1.clkr,
	[GPU_CC_SLEEP_CLK] = &gpu_cc_sleep_clk.clkr,
	[GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK] = &gpu_cc_hlos1_vote_gpu_smmu_clk.clkr,
};

static const struct regmap_config gpu_cc_lito_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x8008,
	.fast_io = true,
};

static const struct qcom_cc_desc gpu_cc_lito_desc = {
	.config = &gpu_cc_lito_regmap_config,
	.clks = gpu_cc_lito_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_lito_clocks),
};

static const struct of_device_id gpu_cc_lito_match_table[] = {
	{ .compatible = "qcom,lito-gpucc" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_lito_match_table);

static int gpu_cc_lito_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	struct clk *clk;
	unsigned int value, mask;
	int ret;

	clk = clk_get(&pdev->dev, "xo");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get ahb clock handle\n");
		return PTR_ERR(clk);
	}
	clk_put(clk);

	regmap = qcom_cc_map(pdev, &gpu_cc_lito_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (PTR_ERR(vdd_cx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	vdd_mx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(vdd_mx.regulator[0])) {
		if (PTR_ERR(vdd_mx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_mx regulator\n");
		return PTR_ERR(vdd_mx.regulator[0]);
	}

	clk_lucid_pll_configure(&gpu_cc_pll1, regmap, &gpu_cc_pll1_config);

	/* Recommended WAKEUP/SLEEP settings for the gpu_cc_cx_gmu_clk */
	mask = CX_GMU_CBCR_WAKE_MASK << CX_GMU_CBCR_WAKE_SHIFT;
	mask |= CX_GMU_CBCR_SLEEP_MASK << CX_GMU_CBCR_SLEEP_SHIFT;
	value = 0xF << CX_GMU_CBCR_WAKE_SHIFT | 0xF << CX_GMU_CBCR_SLEEP_SHIFT;
	regmap_update_bits(regmap, gpu_cc_cx_gmu_clk.clkr.enable_reg,
								mask, value);

	ret = qcom_cc_really_probe(pdev, &gpu_cc_lito_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered GPU CC clocks\n");
	return 0;
}

static struct platform_driver gpu_cc_lito_driver = {
	.probe = gpu_cc_lito_probe,
	.driver = {
		.name = "gpu_cc-lito",
		.of_match_table = gpu_cc_lito_match_table,
	},
};

static int __init gpu_cc_lito_init(void)
{
	return platform_driver_register(&gpu_cc_lito_driver);
}
subsys_initcall(gpu_cc_lito_init);

static void __exit gpu_cc_lito_exit(void)
{
	platform_driver_unregister(&gpu_cc_lito_driver);
}
module_exit(gpu_cc_lito_exit);

MODULE_DESCRIPTION("QTI GPU_CC LITO Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gpu_cc-lito");

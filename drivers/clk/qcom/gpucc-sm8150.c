/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <dt-bindings/clock/qcom,gpucc-sm8150.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "reset.h"
#include "clk-alpha-pll.h"
#include "vdd-level.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_NUM, 1, vdd_corner);

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

static struct pll_vco trion_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static struct alpha_pll_config gpu_cc_pll1_config = {
	.l = 0x1A,
	.alpha = 0xAAA,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002267,
	.config_ctl_hi1_val = 0x00000024,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000002,
	.test_ctl_hi1_val = 0x00000000,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x000000D0,
};

static struct clk_alpha_pll gpu_cc_pll1 = {
	.offset = 0x100,
	.vco_table = trion_vco,
	.num_vco = ARRAY_SIZE(trion_vco),
	.config = &gpu_cc_pll1_config,
	.type = TRION_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll1",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_trion_pll_ops,
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

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src_sdmshrike[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN_DIV, 1.5, 0, 0),
	F(400000000, P_GPLL0_OUT_MAIN, 1.5, 0, 0),
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
			[VDD_MIN] = 200000000,
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
	.halt_check = BRANCH_HALT,
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
	.halt_check = BRANCH_HALT,
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

static struct clk_branch gpu_cc_cx_qdss_at_clk = {
	.halt_reg = 0x1080,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1080,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_qdss_at_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_qdss_trig_clk = {
	.halt_reg = 0x1094,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1094,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_qdss_trig_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_qdss_tsctr_clk = {
	.halt_reg = 0x1084,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1084,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_qdss_tsctr_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_snoc_dvm_clk = {
	.halt_reg = 0x108c,
	.halt_check = BRANCH_HALT,
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
	.halt_check = BRANCH_HALT,
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

static struct clk_branch gpu_cc_gx_qdss_tsctr_clk = {
	.halt_reg = 0x105c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x105c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gx_qdss_tsctr_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gx_vsense_clk = {
	.halt_reg = 0x1058,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gx_vsense_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_pll_test_clk = {
	.halt_reg = 0x110c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x110c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll_test_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_sleep_clk = {
	.halt_reg = 0x1090,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1090,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

/* Measure-only clock for gpu_cc_cx_gfx3d_clk. */
static struct clk_dummy measure_only_gpu_cc_cx_gfx3d_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gpu_cc_cx_gfx3d_clk",
		.ops = &clk_dummy_ops,
	},
};

/* Measure-only clock for gpu_cc_cx_gfx3d_slv_clk. */
static struct clk_dummy measure_only_gpu_cc_cx_gfx3d_slv_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gpu_cc_cx_gfx3d_slv_clk",
		.ops = &clk_dummy_ops,
	},
};

/* Measure-only clock for gpu_cc_gx_gfx3d_clk. */
static struct clk_dummy measure_only_gpu_cc_gx_gfx3d_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gpu_cc_gx_gfx3d_clk",
		.ops = &clk_dummy_ops,
	},
};

struct clk_hw *gpu_cc_sm8150_hws[] = {
	[MEASURE_ONLY_GPU_CC_CX_GFX3D_CLK] =
		&measure_only_gpu_cc_cx_gfx3d_clk.hw,
	[MEASURE_ONLY_GPU_CC_CX_GFX3D_SLV_CLK] =
		&measure_only_gpu_cc_cx_gfx3d_slv_clk.hw,
	[MEASURE_ONLY_GPU_CC_GX_GFX3D_CLK] =
		&measure_only_gpu_cc_gx_gfx3d_clk.hw,
};

static struct clk_regmap *gpu_cc_sm8150_clocks[] = {
	[GPU_CC_AHB_CLK] = &gpu_cc_ahb_clk.clkr,
	[GPU_CC_CRC_AHB_CLK] = &gpu_cc_crc_ahb_clk.clkr,
	[GPU_CC_CX_APB_CLK] = &gpu_cc_cx_apb_clk.clkr,
	[GPU_CC_CX_GMU_CLK] = &gpu_cc_cx_gmu_clk.clkr,
	[GPU_CC_CX_QDSS_AT_CLK] = &gpu_cc_cx_qdss_at_clk.clkr,
	[GPU_CC_CX_QDSS_TRIG_CLK] = &gpu_cc_cx_qdss_trig_clk.clkr,
	[GPU_CC_CX_QDSS_TSCTR_CLK] = &gpu_cc_cx_qdss_tsctr_clk.clkr,
	[GPU_CC_CX_SNOC_DVM_CLK] = &gpu_cc_cx_snoc_dvm_clk.clkr,
	[GPU_CC_CXO_AON_CLK] = &gpu_cc_cxo_aon_clk.clkr,
	[GPU_CC_CXO_CLK] = &gpu_cc_cxo_clk.clkr,
	[GPU_CC_GMU_CLK_SRC] = &gpu_cc_gmu_clk_src.clkr,
	[GPU_CC_GX_GMU_CLK] = &gpu_cc_gx_gmu_clk.clkr,
	[GPU_CC_GX_QDSS_TSCTR_CLK] = &gpu_cc_gx_qdss_tsctr_clk.clkr,
	[GPU_CC_GX_VSENSE_CLK] = &gpu_cc_gx_vsense_clk.clkr,
	[GPU_CC_PLL1] = &gpu_cc_pll1.clkr,
	[GPU_CC_PLL_TEST_CLK] = &gpu_cc_pll_test_clk.clkr,
	[GPU_CC_SLEEP_CLK] = &gpu_cc_sleep_clk.clkr,
};

static const struct qcom_reset_map gpu_cc_sm8150_resets[] = {
	[GPUCC_GPU_CC_CX_BCR] = { 0x1068 },
	[GPUCC_GPU_CC_GMU_BCR] = { 0x111c },
	[GPUCC_GPU_CC_GX_BCR] = { 0x1008 },
	[GPUCC_GPU_CC_SPDM_BCR] = { 0x1110 },
	[GPUCC_GPU_CC_XO_BCR] = { 0x1000 },
};

static const struct regmap_config gpu_cc_sm8150_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x8008,
	.fast_io	= true,
};

static const struct qcom_cc_desc gpu_cc_sm8150_desc = {
	.config = &gpu_cc_sm8150_regmap_config,
	.clks = gpu_cc_sm8150_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_sm8150_clocks),
	.resets = gpu_cc_sm8150_resets,
	.num_resets = ARRAY_SIZE(gpu_cc_sm8150_resets),
};

static struct clk_regmap *gpucc_sm8150_critical_clocks[] = {
	&gpu_cc_ahb_clk.clkr,
};

static const struct qcom_cc_critical_desc gpucc_sm8150_critical_desc = {
	.clks = gpucc_sm8150_critical_clocks,
	.num_clks = ARRAY_SIZE(gpucc_sm8150_critical_clocks),
};

static const struct of_device_id gpu_cc_sm8150_match_table[] = {
	{ .compatible = "qcom,gpucc-sm8150" },
	{ .compatible = "qcom,gpucc-sdmshrike" },
	{ .compatible = "qcom,gpucc-sa8155" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_sm8150_match_table);

static int gpucc_sa8150_resume(struct device *dev)
{
	return qcom_cc_enable_critical_clks(&gpucc_sm8150_critical_desc);
}

static const struct dev_pm_ops gpucc_sa8150_pm_ops = {
	.restore_early = gpucc_sa8150_resume,
};

static void gpu_cc_sm8150_fixup_sdmshrike(void)
{
	gpu_cc_gmu_clk_src.freq_tbl = ftbl_gpu_cc_gmu_clk_src_sdmshrike;
	gpu_cc_gmu_clk_src.clkr.hw.init->rate_max[VDD_LOW] = 400000000;
	gpu_cc_gmu_clk_src.clkr.hw.init->rate_max[VDD_LOW_L1] = 500000000;
}

static int gpu_cc_sm8150_fixup(struct platform_device *pdev)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,gpucc-sdmshrike"))
		gpu_cc_sm8150_fixup_sdmshrike();

	if (!strcmp(compat, "qcom,gcc-sa8155"))
		pdev->dev.driver->pm = &gpucc_sa8150_pm_ops;

	return 0;
}

static int gpu_cc_sm8150_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	struct clk *clk;
	int i, ret = 0;

	regmap = qcom_cc_map(pdev, &gpu_cc_sm8150_desc);
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

	gpu_cc_sm8150_fixup(pdev);

	for (i = 0; i < ARRAY_SIZE(gpu_cc_sm8150_hws); i++) {
		clk = devm_clk_register(&pdev->dev, gpu_cc_sm8150_hws[i]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
	}

	clk_trion_pll_configure(&gpu_cc_pll1, regmap, gpu_cc_pll1.config);

	ret = qcom_cc_really_probe(pdev, &gpu_cc_sm8150_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered GPU CC clocks\n");
	return ret;
}

static struct platform_driver gpu_cc_sm8150_driver = {
	.probe		= gpu_cc_sm8150_probe,
	.driver		= {
		.name	= "gpu_cc-sm8150",
		.of_match_table = gpu_cc_sm8150_match_table,
	},
};

static int __init gpu_cc_sm8150_init(void)
{
	return platform_driver_register(&gpu_cc_sm8150_driver);
}
subsys_initcall(gpu_cc_sm8150_init);

static void __exit gpu_cc_sm8150_exit(void)
{
	platform_driver_unregister(&gpu_cc_sm8150_driver);
}
module_exit(gpu_cc_sm8150_exit);

MODULE_DESCRIPTION("QTI GPU_CC SM8150 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gpu_cc-sm8150");

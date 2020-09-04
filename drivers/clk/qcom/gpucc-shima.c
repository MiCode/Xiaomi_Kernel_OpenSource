// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/pm_clock.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,gpucc-shima.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "clk-regmap-divider.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

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

static struct pll_vco lucid_5lpe_vco[] = {
	{ 249600000, 1800000000, 0 },
};

/* 468MHz Configuration */
static const struct alpha_pll_config gpu_cc_pll0_config = {
	.l = 0x18,
	.alpha = 0x6000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x2A9A699C,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll gpu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = lucid_5lpe_vco,
	.num_vco = ARRAY_SIZE(lucid_5lpe_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
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
				[VDD_NOMINAL] = 1750000000,
				[VDD_HIGH] = 1800000000},
		},
	},
};

/* 500MHz Configuration */
static const struct alpha_pll_config gpu_cc_pll1_config = {
	.l = 0x1A,
	.alpha = 0xAAA,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x2A9A699C,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000000,
	.test_ctl_hi1_val = 0x01800000,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll gpu_cc_pll1 = {
	.offset = 0x100,
	.vco_table = lucid_5lpe_vco,
	.num_vco = ARRAY_SIZE(lucid_5lpe_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_5LPE],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll1",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
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
				[VDD_NOMINAL] = 1750000000,
				[VDD_HIGH] = 1800000000},
		},
	},
};

static const struct parent_map gpu_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_MAIN, 1 },
	{ P_GPU_CC_PLL1_OUT_MAIN, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data gpu_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo", },
	{ .hw = &gpu_cc_pll0.clkr.hw },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .fw_name = "gcc_gpu_gpll0_clk_src", },
	{ .fw_name = "gcc_gpu_gpll0_div_clk_src", },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
};

static const struct parent_map gpu_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL1_OUT_MAIN, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const struct clk_parent_data gpu_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo", },
	{ .hw = &gpu_cc_pll1.clkr.hw },
	{ .fw_name = "gcc_gpu_gpll0_clk_src", },
	{ .fw_name = "gcc_gpu_gpll0_div_clk_src", },
	{ .fw_name = "core_bi_pll_test_se", .name = "core_bi_pll_test_se" },
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
		.parent_data = gpu_cc_parent_data_0,
		.num_parents = 6,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 200000000,
			[VDD_LOW] = 500000000},
	},
};

static const struct freq_tbl ftbl_gpu_cc_hub_clk_src[] = {
	F(150000000, P_GPLL0_OUT_MAIN_DIV, 2, 0, 0),
	F(240000000, P_GPLL0_OUT_MAIN, 2.5, 0, 0),
	F(300000000, P_GPLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_hub_clk_src = {
	.cmd_rcgr = 0x117c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_1,
	.freq_tbl = ftbl_gpu_cc_hub_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_hub_clk_src",
		.parent_data = gpu_cc_parent_data_1,
		.num_parents = 5,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 150000000,
			[VDD_LOW] = 240000000,
			[VDD_NOMINAL] = 300000000},
	},
};

static struct clk_regmap_div gpu_cc_hub_ahb_div_clk_src = {
	.reg = 0x11c0,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpu_cc_hub_ahb_div_clk_src",
		.parent_data = &(const struct clk_parent_data){
			.hw = &gpu_cc_hub_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div gpu_cc_hub_cx_int_div_clk_src = {
	.reg = 0x11bc,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpu_cc_hub_cx_int_div_clk_src",
		.parent_data = &(const struct clk_parent_data){
			.hw = &gpu_cc_hub_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
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
			.parent_data = &(const struct clk_parent_data){
				.hw = &gpu_cc_hub_ahb_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cb_clk = {
	.halt_reg = 0x1170,
	.halt_check = BRANCH_HALT_SKIP,
	.clkr = {
		.enable_reg = 0x1170,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_crc_ahb_clk = {
	.halt_reg = 0x107c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x107c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_crc_ahb_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &gpu_cc_hub_ahb_div_clk_src.clkr.hw,
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
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_gmu_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &gpu_cc_gmu_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_DONT_HOLD_STATE,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_snoc_dvm_clk = {
	.halt_reg = 0x108c,
	.halt_check = BRANCH_HALT_VOTED,
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
	.halt_check = BRANCH_HALT_VOTED,
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
			.flags = CLK_DONT_HOLD_STATE,
			.ops = &clk_branch2_aon_ops,
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
			.parent_data = &(const struct clk_parent_data){
				.hw = &gpu_cc_gmu_clk_src.clkr.hw,
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

static struct clk_branch gpu_cc_hub_aon_clk = {
	.halt_reg = 0x1178,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1178,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_hub_aon_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &gpu_cc_hub_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_hub_cx_int_clk = {
	.halt_reg = 0x1204,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1204,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_hub_cx_int_clk",
			.parent_data = &(const struct clk_parent_data){
				.hw = &gpu_cc_hub_cx_int_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_DONT_HOLD_STATE,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch gpu_cc_mnd1x_0_gfx3d_clk = {
	.halt_reg = 0x802c,
	.halt_check = BRANCH_HALT_SKIP,
	.clkr = {
		.enable_reg = 0x802c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_mnd1x_0_gfx3d_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_mnd1x_1_gfx3d_clk = {
	.halt_reg = 0x8030,
	.halt_check = BRANCH_HALT_SKIP,
	.clkr = {
		.enable_reg = 0x8030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_mnd1x_1_gfx3d_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_sleep_clk = {
	.halt_reg = 0x1090,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x1090,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *gpu_cc_shima_clocks[] = {
	[GPU_CC_AHB_CLK] = &gpu_cc_ahb_clk.clkr,
	[GPU_CC_CB_CLK] = &gpu_cc_cb_clk.clkr,
	[GPU_CC_CRC_AHB_CLK] = &gpu_cc_crc_ahb_clk.clkr,
	[GPU_CC_CX_GMU_CLK] = &gpu_cc_cx_gmu_clk.clkr,
	[GPU_CC_CX_SNOC_DVM_CLK] = &gpu_cc_cx_snoc_dvm_clk.clkr,
	[GPU_CC_CXO_AON_CLK] = &gpu_cc_cxo_aon_clk.clkr,
	[GPU_CC_CXO_CLK] = &gpu_cc_cxo_clk.clkr,
	[GPU_CC_GMU_CLK_SRC] = &gpu_cc_gmu_clk_src.clkr,
	[GPU_CC_GX_GMU_CLK] = &gpu_cc_gx_gmu_clk.clkr,
	[GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK] = &gpu_cc_hlos1_vote_gpu_smmu_clk.clkr,
	[GPU_CC_HUB_AHB_DIV_CLK_SRC] = &gpu_cc_hub_ahb_div_clk_src.clkr,
	[GPU_CC_HUB_AON_CLK] = &gpu_cc_hub_aon_clk.clkr,
	[GPU_CC_HUB_CLK_SRC] = &gpu_cc_hub_clk_src.clkr,
	[GPU_CC_HUB_CX_INT_CLK] = &gpu_cc_hub_cx_int_clk.clkr,
	[GPU_CC_HUB_CX_INT_DIV_CLK_SRC] = &gpu_cc_hub_cx_int_div_clk_src.clkr,
	[GPU_CC_MND1X_0_GFX3D_CLK] = &gpu_cc_mnd1x_0_gfx3d_clk.clkr,
	[GPU_CC_MND1X_1_GFX3D_CLK] = &gpu_cc_mnd1x_1_gfx3d_clk.clkr,
	[GPU_CC_PLL0] = &gpu_cc_pll0.clkr,
	[GPU_CC_PLL1] = &gpu_cc_pll1.clkr,
	[GPU_CC_SLEEP_CLK] = &gpu_cc_sleep_clk.clkr,
};

static const struct regmap_config gpu_cc_shima_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x8030,
	.fast_io = true,
};

static const struct qcom_cc_desc gpu_cc_shima_desc = {
	.config = &gpu_cc_shima_regmap_config,
	.clks = gpu_cc_shima_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_shima_clocks),
};

static const struct of_device_id gpu_cc_shima_match_table[] = {
	{ .compatible = "qcom,shima-gpucc" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_shima_match_table);

static int gpu_cc_shima_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (PTR_ERR(vdd_cx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	vdd_mx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(vdd_mx.regulator[0])) {
		if (PTR_ERR(vdd_mx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_mx regulator\n");
		return PTR_ERR(vdd_mx.regulator[0]);
	}

	regmap = qcom_cc_map(pdev, &gpu_cc_shima_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	pm_runtime_enable(&pdev->dev);
	ret = pm_clk_create(&pdev->dev);
	if (ret)
		goto disable_pm_runtime;

	ret = pm_clk_add(&pdev->dev, "cfg_ahb");
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to get ahb clock handle\n");
		goto destroy_pm_clk;
	}

	clk_lucid_5lpe_pll_configure(&gpu_cc_pll0, regmap, &gpu_cc_pll0_config);
	clk_lucid_5lpe_pll_configure(&gpu_cc_pll1, regmap, &gpu_cc_pll1_config);

	ret = qcom_cc_really_probe(pdev, &gpu_cc_shima_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU CC clocks\n");
		goto destroy_pm_clk;
	}

	dev_info(&pdev->dev, "Registered GPU CC clocks\n");

	return 0;

destroy_pm_clk:
	pm_clk_destroy(&pdev->dev);

disable_pm_runtime:
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static const struct dev_pm_ops gpu_cc_shima_pm_ops = {
	SET_RUNTIME_PM_OPS(pm_clk_suspend, pm_clk_resume, NULL)
};

static void gpu_cc_shima_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &gpu_cc_shima_desc);
}

static struct platform_driver gpu_cc_shima_driver = {
	.probe = gpu_cc_shima_probe,
	.driver = {
		.name = "gpu_cc-shima",
		.of_match_table = gpu_cc_shima_match_table,
		.sync_state = gpu_cc_shima_sync_state,
		.pm = &gpu_cc_shima_pm_ops,
	},
};

static int __init gpu_cc_shima_init(void)
{
	return platform_driver_register(&gpu_cc_shima_driver);
}
subsys_initcall(gpu_cc_shima_init);

static void __exit gpu_cc_shima_exit(void)
{
	platform_driver_unregister(&gpu_cc_shima_driver);
}
module_exit(gpu_cc_shima_exit);

MODULE_DESCRIPTION("QTI GPU_CC SHIMA Driver");
MODULE_LICENSE("GPL v2");

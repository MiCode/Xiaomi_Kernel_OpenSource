// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

#include <dt-bindings/clock/qcom,videocc-parrot.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_HIGH + 1, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mxa, VDD_HIGH + 1, 1, vdd_corner);

static struct clk_vdd_class *video_cc_parrot_regulators[] = {
	&vdd_cx,
	&vdd_mxa,
};

enum {
	P_BI_TCXO,
	P_SLEEP_CLK,
	P_VIDEO_PLL0_OUT_EVEN,
	P_VIDEO_PLL0_OUT_MAIN,
	P_VIDEO_PLL0_OUT_ODD,
};

static const struct pll_vco lucid_evo_vco[] = {
	{ 249600000, 2000000000, 0 },
};

/* 400MHz Configuration */
static const struct alpha_pll_config video_pll0_config = {
	.l = 0x14,
	.cal_l = 0x44,
	.alpha = 0xD555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00182261,
	.config_ctl_hi1_val = 0x32AA299C,
	.user_ctl_val = 0x00000001,
	.user_ctl_hi_val = 0x00000805,
};

static struct clk_alpha_pll video_pll0 = {
	.offset = 0x0,
	.vco_table = lucid_evo_vco,
	.num_vco = ARRAY_SIZE(lucid_evo_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_EVO],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "video_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_evo_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_LOWER_D1] = 500000000,
				[VDD_LOWER] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1500000000,
				[VDD_NOMINAL] = 1750000000,
				[VDD_HIGH] = 2000000000},
		},
	},
};

static const struct parent_map video_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_VIDEO_PLL0_OUT_MAIN, 1 },
	{ P_VIDEO_PLL0_OUT_EVEN, 2 },
	{ P_VIDEO_PLL0_OUT_ODD, 3 },
};

static const struct clk_parent_data video_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &video_pll0.clkr.hw },
	{ .hw = &video_pll0.clkr.hw },
	{ .hw = &video_pll0.clkr.hw },
};

static const struct parent_map video_cc_parent_map_1[] = {
	{ P_SLEEP_CLK, 0 },
};

static const struct clk_parent_data video_cc_parent_data_1[] = {
	{ .fw_name = "sleep_clk" },
};

static const struct freq_tbl ftbl_video_cc_iris_clk_src[] = {
	F(133333333, P_VIDEO_PLL0_OUT_EVEN, 3, 0, 0),
	F(240000000, P_VIDEO_PLL0_OUT_EVEN, 2, 0, 0),
	F(335000000, P_VIDEO_PLL0_OUT_EVEN, 2, 0, 0),
	F(424000000, P_VIDEO_PLL0_OUT_EVEN, 2, 0, 0),
	F(460000000, P_VIDEO_PLL0_OUT_EVEN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_iris_clk_src = {
	.cmd_rcgr = 0x4000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_0,
	.freq_tbl = ftbl_video_cc_iris_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_iris_clk_src",
		.parent_data = video_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(video_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_classes = video_cc_parrot_regulators,
		.num_vdd_classes = ARRAY_SIZE(video_cc_parrot_regulators),
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 133333333,
			[VDD_LOW] = 240000000,
			[VDD_LOW_L1] = 335000000,
			[VDD_NOMINAL] = 424000000,
			[VDD_HIGH] = 460000000},
	},
};

static const struct freq_tbl ftbl_video_cc_sleep_clk_src[] = {
	F(32000, P_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_sleep_clk_src = {
	.cmd_rcgr = 0xa000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_1,
	.freq_tbl = ftbl_video_cc_sleep_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_sleep_clk_src",
		.parent_data = video_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(video_cc_parent_data_1),
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 32000},
	},
};

static struct clk_branch video_cc_iris_ahb_clk = {
	.halt_reg = 0x7004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x7004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_iris_ahb_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_iris_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs0_axi_clk = {
	.halt_reg = 0xb00c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb00c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs0_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs0_core_clk = {
	.halt_reg = 0x6018,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x6018,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x6018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs0_core_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_iris_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvsc_core_clk = {
	.halt_reg = 0x501c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x501c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvsc_core_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_iris_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvsc_ctl_axi_clk = {
	.halt_reg = 0xb004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvsc_ctl_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_sleep_clk = {
	.halt_reg = 0xa018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_sleep_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_sleep_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_trig_clk = {
	.halt_reg = 0xb014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_trig_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_venus_ahb_clk = {
	.halt_reg = 0xb01c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xb01c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_venus_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *video_cc_parrot_clocks[] = {
	[VIDEO_CC_IRIS_AHB_CLK] = &video_cc_iris_ahb_clk.clkr,
	[VIDEO_CC_IRIS_CLK_SRC] = &video_cc_iris_clk_src.clkr,
	[VIDEO_CC_MVS0_AXI_CLK] = &video_cc_mvs0_axi_clk.clkr,
	[VIDEO_CC_MVS0_CORE_CLK] = &video_cc_mvs0_core_clk.clkr,
	[VIDEO_CC_MVSC_CORE_CLK] = &video_cc_mvsc_core_clk.clkr,
	[VIDEO_CC_MVSC_CTL_AXI_CLK] = &video_cc_mvsc_ctl_axi_clk.clkr,
	[VIDEO_CC_SLEEP_CLK] = &video_cc_sleep_clk.clkr,
	[VIDEO_CC_SLEEP_CLK_SRC] = &video_cc_sleep_clk_src.clkr,
	[VIDEO_CC_TRIG_CLK] = &video_cc_trig_clk.clkr,
	[VIDEO_CC_VENUS_AHB_CLK] = &video_cc_venus_ahb_clk.clkr,
	[VIDEO_PLL0] = &video_pll0.clkr,
};

static const struct qcom_reset_map video_cc_parrot_resets[] = {
	[VCODEC_VIDEO_CC_INTERFACE_AHB_BCR] = { 0x7000 },
	[VCODEC_VIDEO_CC_INTERFACE_BCR] = { 0xb000 },
	[VCODEC_VIDEO_CC_MVS0_BCR] = { 0x6000 },
	[VCODEC_VIDEO_CC_MVSC_BCR] = { 0x5000 },
};

static const struct regmap_config video_cc_parrot_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xcf4c,
	.fast_io = true,
};

static struct qcom_cc_desc video_cc_parrot_desc = {
	.config = &video_cc_parrot_regmap_config,
	.clks = video_cc_parrot_clocks,
	.num_clks = ARRAY_SIZE(video_cc_parrot_clocks),
	.resets = video_cc_parrot_resets,
	.num_resets = ARRAY_SIZE(video_cc_parrot_resets),
	.clk_regulators = video_cc_parrot_regulators,
	.num_clk_regulators = ARRAY_SIZE(video_cc_parrot_regulators),
};

static const struct of_device_id video_cc_parrot_match_table[] = {
	{ .compatible = "qcom,parrot-videocc" },
	{ }
};
MODULE_DEVICE_TABLE(of, video_cc_parrot_match_table);

static int video_cc_parrot_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &video_cc_parrot_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = qcom_cc_runtime_init(pdev, &video_cc_parrot_desc);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret)
		return ret;

	clk_lucid_evo_pll_configure(&video_pll0, regmap, &video_pll0_config);

	/*
	 * Keep clocks always enabled:
	 *	video_cc_xo_clk
	 */
	regmap_update_bits(regmap, 0x9018, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(pdev, &video_cc_parrot_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register VIDEO CC clocks\n");
		return ret;
	}

	pm_runtime_put_sync(&pdev->dev);
	dev_info(&pdev->dev, "Registered VIDEO CC clocks\n");

	return ret;
}

static void video_cc_parrot_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &video_cc_parrot_desc);
}

static const struct dev_pm_ops video_cc_parrot_pm_ops = {
	SET_RUNTIME_PM_OPS(qcom_cc_runtime_suspend, qcom_cc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static struct platform_driver video_cc_parrot_driver = {
	.probe = video_cc_parrot_probe,
	.driver = {
		.name = "video_cc-parrot",
		.of_match_table = video_cc_parrot_match_table,
		.sync_state = video_cc_parrot_sync_state,
		.pm = &video_cc_parrot_pm_ops,
	},
};

static int __init video_cc_parrot_init(void)
{
	return platform_driver_register(&video_cc_parrot_driver);
}
subsys_initcall(video_cc_parrot_init);

static void __exit video_cc_parrot_exit(void)
{
	platform_driver_unregister(&video_cc_parrot_driver);
}
module_exit(video_cc_parrot_exit);

MODULE_DESCRIPTION("QTI VIDEO_CC PARROT Driver");
MODULE_LICENSE("GPL v2");

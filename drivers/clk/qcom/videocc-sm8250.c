// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

#include <dt-bindings/clock/qcom,videocc-sm8250.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "common.h"
#include "reset.h"
#include "gdsc.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_mm, VDD_NOMINAL + 1, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_HIGH + 1, 1, vdd_corner);

static struct clk_vdd_class *video_cc_sm8250_regulators[] = {
	&vdd_mm,
	&vdd_mx,
};

enum {
	P_BI_TCXO,
	P_SLEEP_CLK,
	P_VIDEO_PLL0_OUT_MAIN,
	P_VIDEO_PLL1_OUT_MAIN,
};

static const struct pll_vco lucid_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static const struct alpha_pll_config video_pll0_config = {
	.l = 0x25,
	.cal_l = 0x44,
	.alpha = 0x8000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll video_pll0 = {
	.offset = 0x42c,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr = {
		.hw.init = &(const struct clk_init_data){
			.name = "video_pll0",
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

static const struct alpha_pll_config video_pll1_config = {
	.l = 0x2B,
	.cal_l = 0x44,
	.alpha = 0xC000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static struct clk_alpha_pll video_pll1 = {
	.offset = 0x7d0,
	.vco_table = lucid_vco,
	.num_vco = ARRAY_SIZE(lucid_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID],
	.clkr = {
		.hw.init = &(const struct clk_init_data){
			.name = "video_pll1",
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

static const struct parent_map video_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
};

static const struct clk_parent_data video_cc_parent_data_0_ao[] = {
	{ .fw_name = "bi_tcxo_ao" },
};

static const struct parent_map video_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_VIDEO_PLL0_OUT_MAIN, 1 },
};

static const struct clk_parent_data video_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &video_pll0.clkr.hw },
};

static const struct parent_map video_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_VIDEO_PLL1_OUT_MAIN, 1 },
};

static const struct clk_parent_data video_cc_parent_data_2[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &video_pll1.clkr.hw },
};

static const struct parent_map video_cc_parent_map_3[] = {
	{ P_SLEEP_CLK, 0 },
};

static const struct clk_parent_data video_cc_parent_data_3[] = {
	{ .fw_name = "sleep_clk" },
};

static const struct freq_tbl ftbl_video_cc_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_ahb_clk_src = {
	.cmd_rcgr = 0xbd4,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_0,
	.freq_tbl = ftbl_video_cc_ahb_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "video_cc_ahb_clk_src",
		.parent_data = video_cc_parent_data_0_ao,
		.num_parents = ARRAY_SIZE(video_cc_parent_data_0_ao),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_video_cc_mvs0_clk_src[] = {
	F(720000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(1014000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(1098000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	F(1332000000, P_VIDEO_PLL0_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_mvs0_clk_src = {
	.cmd_rcgr = 0xb94,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_1,
	.freq_tbl = ftbl_video_cc_mvs0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "video_cc_mvs0_clk_src",
		.parent_data = video_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(video_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 720000000,
			[VDD_LOW] = 1014000000,
			[VDD_LOW_L1] = 1098000000,
			[VDD_NOMINAL] = 1332000000},
	},
};

static const struct freq_tbl ftbl_video_cc_mvs1_clk_src[] = {
	F(840000000, P_VIDEO_PLL1_OUT_MAIN, 1, 0, 0),
	F(1098000000, P_VIDEO_PLL1_OUT_MAIN, 1, 0, 0),
	F(1332000000, P_VIDEO_PLL1_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_mvs1_clk_src = {
	.cmd_rcgr = 0xbb4,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_2,
	.freq_tbl = ftbl_video_cc_mvs1_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "video_cc_mvs1_clk_src",
		.parent_data = video_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(video_cc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 840000000,
			[VDD_LOW] = 1098000000,
			[VDD_LOW_L1] = 1098000000,
			[VDD_NOMINAL] = 1332000000},
	},
};

static const struct freq_tbl ftbl_video_cc_sleep_clk_src[] = {
	F(32000, P_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_sleep_clk_src = {
	.cmd_rcgr = 0xef0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_3,
	.freq_tbl = ftbl_video_cc_sleep_clk_src,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "video_cc_sleep_clk_src",
		.parent_data = video_cc_parent_data_3,
		.num_parents = ARRAY_SIZE(video_cc_parent_data_3),
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 32000},
	},
};

static struct clk_rcg2 video_cc_xo_clk_src = {
	.cmd_rcgr = 0xecc,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_0,
	.freq_tbl = ftbl_video_cc_ahb_clk_src,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "video_cc_xo_clk_src",
		.parent_data = video_cc_parent_data_0_ao,
		.num_parents = ARRAY_SIZE(video_cc_parent_data_0_ao),
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_regmap_div video_cc_mvs0_div_clk_src = {
	.reg = 0xd54,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "video_cc_mvs0_div_clk_src",
		.parent_hws = (const struct clk_hw*[]){
			&video_cc_mvs0_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div video_cc_mvs0c_div2_div_clk_src = {
	.reg = 0xc54,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "video_cc_mvs0c_div2_div_clk_src",
		.parent_hws = (const struct clk_hw*[]){
			&video_cc_mvs0_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div video_cc_mvs1_div_clk_src = {
	.reg = 0xdd4,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "video_cc_mvs1_div_clk_src",
		.parent_hws = (const struct clk_hw*[]){
			&video_cc_mvs1_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div video_cc_mvs1c_div2_div_clk_src = {
	.reg = 0xcf4,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "video_cc_mvs1c_div2_div_clk_src",
		.parent_hws = (const struct clk_hw*[]){
			&video_cc_mvs1_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch video_cc_mvs0_clk = {
	.halt_reg = 0xd34,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0xd34,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_mvs0_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_mvs0_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs0c_clk = {
	.halt_reg = 0xc34,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xc34,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_mvs0c_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_mvs0c_div2_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs1_clk = {
	.halt_reg = 0xdb4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0xdb4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_mvs1_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_mvs1_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs1_div2_clk = {
	.halt_reg = 0xdf4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0xdf4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_mvs1_div2_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_mvs1c_div2_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs1c_clk = {
	.halt_reg = 0xcd4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xcd4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_mvs1c_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_mvs1c_div2_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_sleep_clk = {
	.halt_reg = 0xf10,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xf10,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
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

static struct gdsc mvs0c_gdsc = {
	.gdscr = 0xbf8,
	.pd = {
		.name = "mvs0c_gdsc",
	},
	.flags = 0,
	.pwrsts = PWRSTS_OFF_ON,
	.supply = "mmcx",
};

static struct gdsc mvs1c_gdsc = {
	.gdscr = 0xc98,
	.pd = {
		.name = "mvs1c_gdsc",
	},
	.flags = 0,
	.pwrsts = PWRSTS_OFF_ON,
	.supply = "mmcx",
};

static struct gdsc mvs0_gdsc = {
	.gdscr = 0xd18,
	.pd = {
		.name = "mvs0_gdsc",
	},
	.flags = HW_CTRL,
	.pwrsts = PWRSTS_OFF_ON,
	.supply = "mmcx",
};

static struct gdsc mvs1_gdsc = {
	.gdscr = 0xd98,
	.pd = {
		.name = "mvs1_gdsc",
	},
	.flags = HW_CTRL,
	.pwrsts = PWRSTS_OFF_ON,
	.supply = "mmcx",
};

static struct clk_regmap *video_cc_sm8250_clocks[] = {
	[VIDEO_CC_AHB_CLK_SRC] = &video_cc_ahb_clk_src.clkr,
	[VIDEO_CC_MVS0_CLK] = &video_cc_mvs0_clk.clkr,
	[VIDEO_CC_MVS0_CLK_SRC] = &video_cc_mvs0_clk_src.clkr,
	[VIDEO_CC_MVS0_DIV_CLK_SRC] = &video_cc_mvs0_div_clk_src.clkr,
	[VIDEO_CC_MVS0C_CLK] = &video_cc_mvs0c_clk.clkr,
	[VIDEO_CC_MVS0C_DIV2_DIV_CLK_SRC] = &video_cc_mvs0c_div2_div_clk_src.clkr,
	[VIDEO_CC_MVS1_CLK] = &video_cc_mvs1_clk.clkr,
	[VIDEO_CC_MVS1_CLK_SRC] = &video_cc_mvs1_clk_src.clkr,
	[VIDEO_CC_MVS1_DIV2_CLK] = &video_cc_mvs1_div2_clk.clkr,
	[VIDEO_CC_MVS1_DIV_CLK_SRC] = &video_cc_mvs1_div_clk_src.clkr,
	[VIDEO_CC_MVS1C_CLK] = &video_cc_mvs1c_clk.clkr,
	[VIDEO_CC_MVS1C_DIV2_DIV_CLK_SRC] = &video_cc_mvs1c_div2_div_clk_src.clkr,
	[VIDEO_CC_SLEEP_CLK] = &video_cc_sleep_clk.clkr,
	[VIDEO_CC_SLEEP_CLK_SRC] = &video_cc_sleep_clk_src.clkr,
	[VIDEO_CC_XO_CLK_SRC] = &video_cc_xo_clk_src.clkr,
	[VIDEO_PLL0] = &video_pll0.clkr,
	[VIDEO_PLL1] = &video_pll1.clkr,
};

static const struct qcom_reset_map video_cc_sm8250_resets[] = {
	[CVP_VIDEO_CC_INTERFACE_BCR] = { 0xe54 },
	[CVP_VIDEO_CC_MVS0_BCR] = { 0xd14 },
	[VIDEO_CC_MVS0C_CLK_ARES] = { 0xc34, 2 },
	[CVP_VIDEO_CC_MVS0C_BCR] = { 0xbf4 },
	[CVP_VIDEO_CC_MVS1_BCR] = { 0xd94 },
	[VIDEO_CC_MVS1C_CLK_ARES] = { 0xcd4, 2 },
	[CVP_VIDEO_CC_MVS1C_BCR] = { 0xc94 },
};

static struct gdsc *video_cc_sm8250_gdscs[] = {
	[MVS0C_GDSC] = &mvs0c_gdsc,
	[MVS1C_GDSC] = &mvs1c_gdsc,
	[MVS0_GDSC] = &mvs0_gdsc,
	[MVS1_GDSC] = &mvs1_gdsc,
};

static const struct regmap_config video_cc_sm8250_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xf4c,
	.fast_io = true,
};

static struct qcom_cc_desc video_cc_sm8250_desc = {
	.config = &video_cc_sm8250_regmap_config,
	.clks = video_cc_sm8250_clocks,
	.num_clks = ARRAY_SIZE(video_cc_sm8250_clocks),
	.resets = video_cc_sm8250_resets,
	.num_resets = ARRAY_SIZE(video_cc_sm8250_resets),
	.gdscs = video_cc_sm8250_gdscs,
	.num_gdscs = ARRAY_SIZE(video_cc_sm8250_gdscs),
	.clk_regulators = video_cc_sm8250_regulators,
	.num_clk_regulators = ARRAY_SIZE(video_cc_sm8250_regulators),
};

static const struct of_device_id video_cc_sm8250_match_table[] = {
	{ .compatible = "qcom,sm8250-videocc" },
	{ }
};
MODULE_DEVICE_TABLE(of, video_cc_sm8250_match_table);

static int video_cc_sm8250_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &video_cc_sm8250_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = qcom_cc_runtime_init(pdev, &video_cc_sm8250_desc);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret)
		return ret;

	clk_lucid_pll_configure(&video_pll0, regmap, &video_pll0_config);
	clk_lucid_pll_configure(&video_pll1, regmap, &video_pll1_config);

	/*
	 * Keep clocks always enabled:
	 *	video_cc_ahb_clk
	 *	video_cc_xo_clk
	 */
	regmap_update_bits(regmap, 0xe58, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0xeec, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(pdev, &video_cc_sm8250_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register VIDEO CC clocks\n");
		return ret;
	}

	pm_runtime_put_sync(&pdev->dev);
	dev_info(&pdev->dev, "Registered VIDEO CC clocks\n");

	return ret;
}

static void video_cc_sm8250_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &video_cc_sm8250_desc);
}

static const struct dev_pm_ops video_cc_sm8250_pm_ops = {
	SET_RUNTIME_PM_OPS(qcom_cc_runtime_suspend, qcom_cc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static struct platform_driver video_cc_sm8250_driver = {
	.probe	= video_cc_sm8250_probe,
	.driver	= {
		.name = "sm8250-videocc",
		.of_match_table = video_cc_sm8250_match_table,
		.sync_state = video_cc_sm8250_sync_state,
		.pm = &video_cc_sm8250_pm_ops,
	},
};

static int __init video_cc_sm8250_init(void)
{
	return platform_driver_register(&video_cc_sm8250_driver);
}
subsys_initcall(video_cc_sm8250_init);

static void __exit video_cc_sm8250_exit(void)
{
	platform_driver_unregister(&video_cc_sm8250_driver);
}
module_exit(video_cc_sm8250_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QTI VIDEOCC SM8250 Driver");

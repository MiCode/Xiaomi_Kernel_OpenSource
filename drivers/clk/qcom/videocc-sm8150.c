/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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
#include <linux/msm-bus.h>

#include <dt-bindings/clock/qcom,videocc-sm8150.h>
#include <dt-bindings/msm/msm-bus-ids.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "reset.h"
#include "clk-alpha-pll.h"
#include "vdd-level.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

#define MSM_BUS_VECTOR(_src, _dst, _ab, _ib)	\
{						\
	.src = _src,				\
	.dst = _dst,				\
	.ab = _ab,				\
	.ib = _ib,				\
}

static DEFINE_VDD_REGULATORS(vdd_mm, VDD_MM_NUM, 1, vdd_corner);

static struct msm_bus_vectors clk_debugfs_vectors[] = {
	MSM_BUS_VECTOR(MSM_BUS_MASTER_AMPSS_M0,
			MSM_BUS_SLAVE_VENUS_CFG, 0, 0),
	MSM_BUS_VECTOR(MSM_BUS_MASTER_AMPSS_M0,
			MSM_BUS_SLAVE_VENUS_CFG, 0, 1),
};

static struct msm_bus_paths clk_debugfs_usecases[] = {
	{
		.num_paths = 1,
		.vectors = &clk_debugfs_vectors[0],
	},
	{
		.num_paths = 1,
		.vectors = &clk_debugfs_vectors[1],
	}
};

static struct msm_bus_scale_pdata clk_debugfs_scale_table = {
	.usecase = clk_debugfs_usecases,
	.num_usecases = ARRAY_SIZE(clk_debugfs_usecases),
	.name = "clk_videocc_debugfs",
};

enum {
	P_BI_TCXO,
	P_CHIP_SLEEP_CLK,
	P_CORE_BI_PLL_TEST_SE,
	P_VIDEO_PLL0_OUT_EVEN,
	P_VIDEO_PLL0_OUT_MAIN,
	P_VIDEO_PLL0_OUT_ODD,
};

static const struct parent_map video_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_VIDEO_PLL0_OUT_MAIN, 1 },
	{ P_VIDEO_PLL0_OUT_EVEN, 2 },
	{ P_VIDEO_PLL0_OUT_ODD, 3 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const video_cc_parent_names_0[] = {
	"bi_tcxo",
	"video_pll0",
	"video_pll0_out_even",
	"video_pll0_out_odd",
	"core_bi_pll_test_se",
};

static struct pll_vco trion_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static struct alpha_pll_config video_pll0_config = {
	.l = 0x14,
	.alpha = 0xD555,
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

static struct alpha_pll_config video_pll0_config_sm8150_v2 = {
	.l = 0x14,
	.alpha = 0xD555,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002267,
	.config_ctl_hi1_val = 0x00000024,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x000000D0,
};

static struct clk_alpha_pll video_pll0 = {
	.offset = 0x42c,
	.vco_table = trion_vco,
	.num_vco = ARRAY_SIZE(trion_vco),
	.type = TRION_PLL,
	.config = &video_pll0_config,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "video_pll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_trion_pll_ops,
			.vdd_class = &vdd_mm,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 615000000,
				[VDD_LOW] = 1066000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000},
		},
	},
};

static const struct freq_tbl ftbl_video_cc_iris_clk_src[] = {
	F(200000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(225000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(300000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(365000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(432000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(480000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_video_cc_iris_clk_src_sm8150_v2[] = {
	F(200000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(240000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(338000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(365000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(444000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(533000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_iris_clk_src = {
	.cmd_rcgr = 0x7f0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_0,
	.freq_tbl = ftbl_video_cc_iris_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_iris_clk_src",
		.parent_names = video_cc_parent_names_0,
		.num_parents = 5,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_MIN] = 200000000,
			[VDD_LOWER] = 225000000,
			[VDD_LOW] = 300000000,
			[VDD_LOW_L1] = 365000000,
			[VDD_NOMINAL] = 432000000,
			[VDD_HIGH] = 480000000},
	},
};

static struct clk_branch video_cc_iris_ahb_clk = {
	.halt_reg = 0x8f4,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x8f4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_iris_ahb_clk",
			.parent_names = (const char *[]){
				"video_cc_iris_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs0_core_clk = {
	.halt_reg = 0x890,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x890,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs0_core_clk",
			.parent_names = (const char *[]){
				"video_cc_iris_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs1_core_clk = {
	.halt_reg = 0x8d0,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x8d0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs1_core_clk",
			.parent_names = (const char *[]){
				"video_cc_iris_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvsc_core_clk = {
	.halt_reg = 0x850,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x850,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvsc_core_clk",
			.parent_names = (const char *[]){
				"video_cc_iris_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_xo_clk = {
	.halt_reg = 0x984,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x984,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_xo_clk",
			.flags = CLK_IS_CRITICAL,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *video_cc_sm8150_clocks[] = {
	[VIDEO_CC_IRIS_AHB_CLK] = &video_cc_iris_ahb_clk.clkr,
	[VIDEO_CC_IRIS_CLK_SRC] = &video_cc_iris_clk_src.clkr,
	[VIDEO_CC_MVS0_CORE_CLK] = &video_cc_mvs0_core_clk.clkr,
	[VIDEO_CC_MVS1_CORE_CLK] = &video_cc_mvs1_core_clk.clkr,
	[VIDEO_CC_MVSC_CORE_CLK] = &video_cc_mvsc_core_clk.clkr,
	[VIDEO_CC_XO_CLK] = &video_cc_xo_clk.clkr,
	[VIDEO_PLL0] = &video_pll0.clkr,
};

static const struct regmap_config video_cc_sm8150_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0xb94,
	.fast_io	= true,
};

static const struct qcom_cc_desc video_cc_sm8150_desc = {
	.config = &video_cc_sm8150_regmap_config,
	.clks = video_cc_sm8150_clocks,
	.num_clks = ARRAY_SIZE(video_cc_sm8150_clocks),
};

static const struct of_device_id video_cc_sm8150_match_table[] = {
	{ .compatible = "qcom,videocc-sm8150" },
	{ .compatible = "qcom,videocc-sm8150-v2" },
	{ }
};
MODULE_DEVICE_TABLE(of, video_cc_sm8150_match_table);

static void video_cc_sm8150_fixup_sm8150v2(struct regmap *regmap)
{
	video_pll0.config = &video_pll0_config_sm8150_v2;

	video_cc_iris_clk_src.freq_tbl = ftbl_video_cc_iris_clk_src_sm8150_v2;
	video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_LOWER] = 240000000;
	video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_LOW] = 338000000;
	video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_NOMINAL] = 444000000;
	video_cc_iris_clk_src.clkr.hw.init->rate_max[VDD_HIGH] = 533000000;
}

static int video_cc_sm8150_fixup(struct platform_device *pdev,
	struct regmap *regmap)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,videocc-sm8150-v2"))
		video_cc_sm8150_fixup_sm8150v2(regmap);

	return 0;
}

static int video_cc_sm8150_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	struct clk *clk;
	int ret;
	int i;
	unsigned int videocc_bus_id;

	regmap = qcom_cc_map(pdev, &video_cc_sm8150_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the Video CC registers\n");
		return PTR_ERR(regmap);
	}

	clk = devm_clk_get(&pdev->dev, "cfg_ahb_clk");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get ahb clock handle\n");
		return PTR_ERR(clk);
	}
	devm_clk_put(&pdev->dev, clk);

	vdd_mm.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mm");
	if (IS_ERR(vdd_mm.regulator[0])) {
		if (!(PTR_ERR(vdd_mm.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get vdd_mm regulator\n");
		return PTR_ERR(vdd_mm.regulator[0]);
	}
	vdd_mm.use_max_uV = true;

	videocc_bus_id =
		msm_bus_scale_register_client(&clk_debugfs_scale_table);
	if (!videocc_bus_id) {
		dev_err(&pdev->dev, "Unable to register for bw voting\n");
		return -EPROBE_DEFER;
	}

	for (i = 0; i < ARRAY_SIZE(video_cc_sm8150_clocks); i++)
		if (video_cc_sm8150_clocks[i])
			*(unsigned int *)(void *)
			&video_cc_sm8150_clocks[i]->hw.init->bus_cl_id =
							videocc_bus_id;

	ret = video_cc_sm8150_fixup(pdev, regmap);
	if (ret)
		return ret;

	clk_trion_pll_configure(&video_pll0, regmap, video_pll0.config);

	ret = qcom_cc_really_probe(pdev, &video_cc_sm8150_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register Video CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Video CC clocks\n");

	return ret;
}

static struct platform_driver video_cc_sm8150_driver = {
	.probe		= video_cc_sm8150_probe,
	.driver		= {
		.name	= "video_cc-sm8150",
		.of_match_table = video_cc_sm8150_match_table,
	},
};

static int __init video_cc_sm8150_init(void)
{
	return platform_driver_register(&video_cc_sm8150_driver);
}
subsys_initcall(video_cc_sm8150_init);

static void __exit video_cc_sm8150_exit(void)
{
	platform_driver_unregister(&video_cc_sm8150_driver);
}
module_exit(video_cc_sm8150_exit);

MODULE_DESCRIPTION("QTI VIDEO_CC SM8150 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:video_cc-sm8150");

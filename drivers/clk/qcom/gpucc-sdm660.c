/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>
#include <dt-bindings/clock/qcom,gpu-sdm660.h>

#include "clk-alpha-pll.h"
#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "vdd-level-660.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }
#define F_GFX(f, s, h, m, n, sf) { (f), (s), (2 * (h) - 1), (m), (n), (sf) }

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_DIG_NUM, 1, vdd_corner);
static DEFINE_VDD_REGS_INIT(vdd_gfx, 1);

enum {
	P_CORE_BI_PLL_TEST_SE,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_GPU_PLL0_PLL_OUT_MAIN,
	P_GPU_PLL1_PLL_OUT_MAIN,
	P_XO,
};

static const struct parent_map gpucc_parent_map_0[] = {
	{ P_XO, 0 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_GPLL0_OUT_MAIN_DIV, 6 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gpucc_parent_names_0[] = {
	"cxo_a",
	"gcc_gpu_gpll0_clk",
	"gcc_gpu_gpll0_div_clk",
	"core_bi_pll_test_se",
};

static const struct parent_map gpucc_parent_map_1[] = {
	{ P_XO, 0 },
	{ P_GPU_PLL0_PLL_OUT_MAIN, 1 },
	{ P_GPU_PLL1_PLL_OUT_MAIN, 3 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gpucc_parent_names_1[] = {
	"xo",
	"gpu_pll0_pll_out_main",
	"gpu_pll1_pll_out_main",
	"gcc_gpu_gpll0_clk",
	"core_bi_pll_test_se",
};

static struct pll_vco gpu_vco[] = {
	{ 1000000000, 2000000000, 0 },
	{ 500000000,  1000000000, 2 },
	{ 250000000,   500000000, 3 },
};

/* 800MHz configuration */
static const struct pll_config gpu_pll0_config = {
	.l = 0x29,
	.config_ctl_val = 0x4001055b,
	.alpha = 0xaaaaab00,
	.alpha_u = 0xaa,
	.alpha_en_mask = BIT(24),
	.vco_val = 0x2 << 20,
	.vco_mask = 0x3 << 20,
	.main_output_mask = 0x1,
};

static struct pll_vco_data pll_data[] = {
	/* Frequency  post-div */
	{ 640000000,  0x1 },
};

static struct clk_alpha_pll gpu_pll0_pll_out_main = {
	.offset = 0x0,
	.vco_table = gpu_vco,
	.num_vco = ARRAY_SIZE(gpu_vco),
	.vco_data = pll_data,
	.num_vco_data = ARRAY_SIZE(pll_data),
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_pll0_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			VDD_GPU_PLL_FMAX_MAP1(LOW_L1, 1500000000),
		},
	},
};

static struct clk_alpha_pll gpu_pll1_pll_out_main = {
	.offset = 0x40,
	.vco_table = gpu_vco,
	.num_vco = ARRAY_SIZE(gpu_vco),
	.vco_data = pll_data,
	.num_vco_data = ARRAY_SIZE(pll_data),
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_pll1_pll_out_main",
			.parent_names = (const char *[]){ "xo" },
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
			VDD_GPU_PLL_FMAX_MAP1(LOW_L1, 1500000000),
		},
	},
};

/* GFX clock init data */
static struct clk_init_data gpu_clks_init[] = {
	[0] = {
		.name = "gfx3d_clk_src",
		.parent_names = gpucc_parent_names_1,
		.num_parents = 3,
		.ops = &clk_gfx3d_src_ops,
		.flags = CLK_SET_RATE_PARENT,
	},
	[1] = {
		.name = "gpucc_gfx3d_clk",
		.parent_names = (const char *[]){
			"gfx3d_clk_src",
		},
		.num_parents = 1,
		.ops = &clk_branch2_ops,
		.flags = CLK_SET_RATE_PARENT,
		.vdd_class = &vdd_gfx,
	},
};

/*
 * Frequencies and PLL configuration
 * The PLL source would be to ping-pong between GPU-PLL0
 * and GPU-PLL1.
 *  ====================================================
 *  | F         | PLL SRC Freq | PLL postdiv | RCG Div |
 *  ====================================================
 *  | 160000000 | 640000000    |    2        |    2    |
 *  | 266000000 | 532000000    |    1        |    2    |
 *  | 370000000 | 740000000    |    1        |    2    |
 *  | 465000000 | 930000000    |    1        |    2    |
 *  | 588000000 | 1176000000   |    1        |    2    |
 *  | 647000000 | 1294000000   |    1        |    2    |
 *  | 700000000 | 1400000000   |    1        |    2    |
 *  | 750000000 | 1500000000   |    1        |    2    |
 *  ====================================================
*/

static const struct freq_tbl ftbl_gfx3d_clk_src[] = {
	F_GFX( 19200000, 0,  1, 0, 0,         0),
	F_GFX(160000000, 0,  2, 0, 0,  640000000),
	F_GFX(266000000, 0,  2, 0, 0,  532000000),
	F_GFX(370000000, 0,  2, 0, 0,  740000000),
	F_GFX(465000000, 0,  2, 0, 0,  930000000),
	F_GFX(588000000, 0,  2, 0, 0, 1176000000),
	F_GFX(647000000, 0,  2, 0, 0, 1294000000),
	F_GFX(700000000, 0,  2, 0, 0, 1400000000),
	F_GFX(750000000, 0,  2, 0, 0, 1500000000),
	{ }
};

static const struct freq_tbl ftbl_gfx3d_clk_src_630[] = {
	F_GFX( 19200000, 0,  1, 0, 0,         0),
	F_GFX(160000000, 0,  2, 0, 0,  640000000),
	F_GFX(240000000, 0,  2, 0, 0,  480000000),
	F_GFX(370000000, 0,  2, 0, 0,  740000000),
	F_GFX(465000000, 0,  2, 0, 0,  930000000),
	F_GFX(588000000, 0,  2, 0, 0, 1176000000),
	F_GFX(647000000, 0,  2, 0, 0, 1294000000),
	F_GFX(700000000, 0,  2, 0, 0, 1400000000),
	F_GFX(775000000, 0,  2, 0, 0, 1550000000),
	{ }
};

static struct clk_rcg2 gfx3d_clk_src = {
	.cmd_rcgr = 0x1070,
	.mnd_width = 0,
	.hid_width = 5,
	.freq_tbl = ftbl_gfx3d_clk_src,
	.parent_map = gpucc_parent_map_1,
	.flags = FORCE_ENABLE_RCGR,
	.clkr.hw.init = &gpu_clks_init[0],
};

static const struct freq_tbl ftbl_rbbmtimer_clk_src[] = {
	F(19200000, P_XO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 rbbmtimer_clk_src = {
	.cmd_rcgr = 0x10b0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpucc_parent_map_0,
	.freq_tbl = ftbl_rbbmtimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "rbbmtimer_clk_src",
		.parent_names = gpucc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(MIN, 19200000),
	},
};

static const struct freq_tbl ftbl_rbcpr_clk_src[] = {
	F(19200000, P_XO, 1, 0, 0),
	F(50000000, P_GPLL0_OUT_MAIN_DIV, 6, 0, 0),
	{ }
};

static struct clk_rcg2 rbcpr_clk_src = {
	.cmd_rcgr = 0x1030,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpucc_parent_map_0,
	.freq_tbl = ftbl_rbcpr_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "rbcpr_clk_src",
		.parent_names = gpucc_parent_names_0,
		.num_parents = 4,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_branch gpucc_cxo_clk = {
	.halt_reg = 0x1020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpucc_cxo_clk",
			.parent_names = (const char *[]) {
				"cxo_a",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpucc_gfx3d_clk = {
	.halt_reg = 0x1098,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1098,
		.enable_mask = BIT(0),
		.hw.init = &gpu_clks_init[1],
	},
};

static struct clk_branch gpucc_rbbmtimer_clk = {
	.halt_reg = 0x10d0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10d0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpucc_rbbmtimer_clk",
			.parent_names = (const char *[]){
				"rbbmtimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpucc_rbcpr_clk = {
	.halt_reg = 0x1054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpucc_rbcpr_clk",
			.parent_names = (const char *[]){
				"rbcpr_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *gpucc_660_clocks[] = {
	[GPU_PLL0_PLL] = &gpu_pll0_pll_out_main.clkr,
	[GPU_PLL1_PLL] = &gpu_pll1_pll_out_main.clkr,
	[GFX3D_CLK_SRC] = &gfx3d_clk_src.clkr,
	[GPUCC_GFX3D_CLK] = &gpucc_gfx3d_clk.clkr,
	[GPUCC_RBBMTIMER_CLK] = &gpucc_rbbmtimer_clk.clkr,
	[RBBMTIMER_CLK_SRC] = &rbbmtimer_clk_src.clkr,
	[GPUCC_CXO_CLK] = &gpucc_cxo_clk.clkr,
};

static const struct regmap_config gpucc_660_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x9034,
	.fast_io	= true,
};

static const struct qcom_cc_desc gpucc_660_desc = {
	.config = &gpucc_660_regmap_config,
	.clks = gpucc_660_clocks,
	.num_clks = ARRAY_SIZE(gpucc_660_clocks),
};

static const struct of_device_id gpucc_660_match_table[] = {
	{ .compatible = "qcom,gpucc-sdm660" },
	{ .compatible = "qcom,gpucc-sdm630" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpucc_660_match_table);

static int of_get_fmax_vdd_class(struct platform_device *pdev,
		struct clk_hw *hw, char *prop_name, u32 index)
{
	struct device_node *of = pdev->dev.of_node;
	int prop_len, i, j;
	struct clk_vdd_class *vdd = hw->init->vdd_class;
	int num = vdd->num_regulators + 1;
	u32 *array;

	if (!of_find_property(of, prop_name, &prop_len)) {
		dev_err(&pdev->dev, "missing %s\n", prop_name);
		return -EINVAL;
	}

	prop_len /= sizeof(u32);
	if (prop_len % num) {
		dev_err(&pdev->dev, "bad length %d\n", prop_len);
		return -EINVAL;
	}

	prop_len /= num;
	vdd->level_votes = devm_kzalloc(&pdev->dev, prop_len * sizeof(int),
					GFP_KERNEL);
	if (!vdd->level_votes)
		return -ENOMEM;

	vdd->vdd_uv = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(int) * (num - 1), GFP_KERNEL);
	if (!vdd->vdd_uv)
		return -ENOMEM;

	gpu_clks_init[index].rate_max = devm_kzalloc(&pdev->dev, prop_len *
					sizeof(unsigned long), GFP_KERNEL);
	if (!gpu_clks_init[index].rate_max)
		return -ENOMEM;

	array = devm_kzalloc(&pdev->dev, prop_len * sizeof(u32) * num,
				GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	of_property_read_u32_array(of, prop_name, array, prop_len * num);
	for (i = 0; i < prop_len; i++) {
		gpu_clks_init[index].rate_max[i] = array[num * i];
		for (j = 1; j < num; j++) {
			vdd->vdd_uv[(num - 1) * i + (j - 1)] =
						array[num * i + j];
		}
	}

	devm_kfree(&pdev->dev, array);
	vdd->num_levels = prop_len;
	vdd->cur_level = prop_len;
	gpu_clks_init[index].num_rate_max = prop_len;

	return 0;
}

static int gpucc_660_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct regmap *regmap;
	struct resource *res;
	void __iomem *base;
	bool is_630 = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get resources\n");
		return -EINVAL;
	}

	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(base))
		return PTR_ERR(base);

	regmap = devm_regmap_init_mmio(&pdev->dev, base, gpucc_660_desc.config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	/* CX Regulator for RBBMTimer clock */
	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig_gfx");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_dig regulator\n");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	/* Mx Regulator for GPU-PLLs */
	vdd_mx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mx_gfx");
	if (IS_ERR(vdd_mx.regulator[0])) {
		if (!(PTR_ERR(vdd_mx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_mx regulator\n");
		return PTR_ERR(vdd_mx.regulator[0]);
	}

	/* GFX Rail Regulator for GFX3D clock */
	vdd_gfx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_gfx");
	if (IS_ERR(vdd_gfx.regulator[0])) {
		if (!(PTR_ERR(vdd_gfx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_gfx regulator\n");
		return PTR_ERR(vdd_gfx.regulator[0]);
	}

	is_630 = of_device_is_compatible(pdev->dev.of_node,
					"qcom,gpucc-sdm630");
	if (is_630) {
		gpu_pll0_pll_out_main.clkr.hw.init->rate_max[VDD_DIG_LOW_L1]
						= 1550000000;
		gpu_pll1_pll_out_main.clkr.hw.init->rate_max[VDD_DIG_LOW_L1]
						= 1550000000;
		/* Add new frequency table */
		gfx3d_clk_src.freq_tbl = ftbl_gfx3d_clk_src_630;
	}

	/* GFX rail fmax data linked to branch clock */
	of_get_fmax_vdd_class(pdev, &gpucc_gfx3d_clk.clkr.hw,
						"qcom,gfxfreq-corner", 1);

	clk_alpha_pll_configure(&gpu_pll0_pll_out_main, regmap,
							&gpu_pll0_config);
	clk_alpha_pll_configure(&gpu_pll1_pll_out_main, regmap,
							&gpu_pll0_config);

	ret = qcom_cc_really_probe(pdev, &gpucc_660_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPUCC clocks\n");
		return ret;
	}

	clk_prepare_enable(gpucc_cxo_clk.clkr.hw.clk);

	dev_info(&pdev->dev, "Registered GPUCC clocks\n");

	return ret;
}

static struct platform_driver gpucc_660_driver = {
	.probe		= gpucc_660_probe,
	.driver		= {
		.name	= "gpucc-sdm660",
		.of_match_table = gpucc_660_match_table,
	},
};

static int __init gpucc_660_init(void)
{
	return platform_driver_register(&gpucc_660_driver);
}
arch_initcall(gpucc_660_init);

static void __exit gpucc_660_exit(void)
{
	platform_driver_unregister(&gpucc_660_driver);
}
module_exit(gpucc_660_exit);

/* GPU RBCPR Clocks */
static struct clk_regmap *gpucc_rbcpr_660_clocks[] = {
	[RBCPR_CLK_SRC] = &rbcpr_clk_src.clkr,
	[GPUCC_RBCPR_CLK] = &gpucc_rbcpr_clk.clkr,
};

static const struct qcom_cc_desc gpu_660_desc = {
	.config = &gpucc_660_regmap_config,
	.clks = gpucc_rbcpr_660_clocks,
	.num_clks = ARRAY_SIZE(gpucc_rbcpr_660_clocks),
};

static const struct of_device_id gpucc_rbcpr_660_match_table[] = {
	{ .compatible = "qcom,gpu-sdm660" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpucc_rbcpr_660_match_table);

static int gpu_660_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct regmap *regmap;

	regmap = qcom_cc_map(pdev, &gpu_660_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = qcom_cc_really_probe(pdev, &gpu_660_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU RBCPR clocks\n");
		return ret;
	}


	dev_info(&pdev->dev, "Registered GPU RBCPR clocks\n");

	return ret;
}

static struct platform_driver gpu_660_driver = {
	.probe		= gpu_660_probe,
	.driver		= {
		.name	= "gpu-sdm660",
		.of_match_table = gpucc_rbcpr_660_match_table,
	},
};

static int __init gpu_660_init(void)
{
	return platform_driver_register(&gpu_660_driver);
}
core_initcall(gpu_660_init);

static void __exit gpu_660_exit(void)
{
	platform_driver_unregister(&gpu_660_driver);
}
module_exit(gpu_660_exit);


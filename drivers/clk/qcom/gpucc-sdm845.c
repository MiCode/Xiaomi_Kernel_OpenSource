/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>
#include <linux/clk.h>
#include <linux/clk/qcom.h>
#include <dt-bindings/clock/qcom,gpucc-sdm845.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "reset.h"
#include "clk-alpha-pll.h"
#include "vdd-level-sdm845.h"

#define CX_GMU_CBCR_SLEEP_MASK		0xF
#define CX_GMU_CBCR_SLEEP_SHIFT		4
#define CX_GMU_CBCR_WAKE_MASK		0xF
#define CX_GMU_CBCR_WAKE_SHIFT		8

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static int vdd_gx_corner[] = {
	RPMH_REGULATOR_LEVEL_OFF,		/* VDD_GX_NONE */
	RPMH_REGULATOR_LEVEL_MIN_SVS,		/* VDD_GX_MIN */
	RPMH_REGULATOR_LEVEL_LOW_SVS,		/* VDD_GX_LOWER */
	RPMH_REGULATOR_LEVEL_SVS,		/* VDD_GX_LOW */
	RPMH_REGULATOR_LEVEL_SVS_L1,		/* VDD_GX_LOW_L1 */
	RPMH_REGULATOR_LEVEL_NOM,		/* VDD_GX_NOMINAL */
	RPMH_REGULATOR_LEVEL_NOM_L1,		/* VDD_GX_NOMINAL_L1 */
	RPMH_REGULATOR_LEVEL_TURBO,		/* VDD_GX_HIGH */
	RPMH_REGULATOR_LEVEL_TURBO_L1,		/* VDD_GX_HIGH_L1 */
	RPMH_REGULATOR_LEVEL_MAX,		/* VDD_GX_MAX */
};

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_CX_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_CX_NUM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_gfx, VDD_GX_NUM, 1, vdd_gx_corner);

enum {
	P_BI_TCXO,
	P_CORE_BI_PLL_TEST_SE,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_MAIN_DIV,
	P_GPU_CC_PLL0_OUT_EVEN,
	P_GPU_CC_PLL0_OUT_MAIN,
	P_GPU_CC_PLL0_OUT_ODD,
	P_GPU_CC_PLL1_OUT_EVEN,
	P_GPU_CC_PLL1_OUT_MAIN,
	P_GPU_CC_PLL1_OUT_ODD,
	P_CRC_DIV,
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

static const struct parent_map gpu_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_GPU_CC_PLL0_OUT_EVEN, 1 },
	{ P_GPU_CC_PLL0_OUT_ODD, 2 },
	{ P_GPU_CC_PLL1_OUT_EVEN, 3 },
	{ P_GPU_CC_PLL1_OUT_ODD, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gpu_cc_parent_names_1[] = {
	"bi_tcxo",
	"gpu_cc_pll0_out_even",
	"gpu_cc_pll0_out_odd",
	"gpu_cc_pll1_out_even",
	"gpu_cc_pll1_out_odd",
	"gcc_gpu_gpll0_clk_src",
	"core_bi_pll_test_se",
};

static const struct parent_map gpu_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_CRC_DIV,  1 },
	{ P_GPU_CC_PLL0_OUT_ODD, 2 },
	{ P_GPU_CC_PLL1_OUT_EVEN, 3 },
	{ P_GPU_CC_PLL1_OUT_ODD, 4 },
	{ P_GPLL0_OUT_MAIN, 5 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const gpu_cc_parent_names_2[] = {
	"bi_tcxo",
	"crc_div",
	"gpu_cc_pll0_out_odd",
	"gpu_cc_pll1_out_even",
	"gpu_cc_pll1_out_odd",
	"gcc_gpu_gpll0_clk_src",
	"core_bi_pll_test_se",
};

static struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
	{ 125000000, 1000000000, 1 },
};

static const struct pll_config gpu_cc_pll0_config = {
	.l = 0x1d,
	.frac = 0x2aaa,
};

static const struct pll_config gpu_cc_pll1_config = {
	.l = 0x1a,
	.frac = 0xaaaa,
};

static struct clk_alpha_pll gpu_cc_pll0 = {
	.offset = 0x0,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll0",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_fabia_pll_ops,
			VDD_MX_FMAX_MAP4(
				MIN, 615000000,
				LOW, 1066000000,
				LOW_L1, 1600000000,
				NOMINAL, 2000000000),
		},
	},
};

static const struct clk_div_table post_div_table_fabia_even[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{},
};

static struct clk_alpha_pll_postdiv gpu_cc_pll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 8,
	.post_div_table = post_div_table_fabia_even,
	.num_post_div = ARRAY_SIZE(post_div_table_fabia_even),
	.width = 4,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_pll0_out_even",
		.parent_names = (const char *[]){ "gpu_cc_pll0" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_generic_pll_postdiv_ops,
	},
};

static struct clk_alpha_pll gpu_cc_pll1 = {
	.offset = 0x100,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.type = FABIA_PLL,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_pll1",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_fabia_pll_ops,
			VDD_MX_FMAX_MAP4(
				MIN, 615000000,
				LOW, 1066000000,
				LOW_L1, 1600000000,
				NOMINAL, 2000000000),
		},
	},
};

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN_DIV, 1.5, 0, 0),
	F(400000000, P_GPLL0_OUT_MAIN, 1.5, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src_sdm845_v2[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN_DIV, 1.5, 0, 0),
	F(500000000, P_GPU_CC_PLL1_OUT_MAIN, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gpu_cc_gmu_clk_src_sdm670[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(200000000, P_GPLL0_OUT_MAIN_DIV, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_gmu_clk_src = {
	.cmd_rcgr = 0x1120,
	.mnd_width = 0,
	.hid_width = 5,
	.enable_safe_config = true,
	.parent_map = gpu_cc_parent_map_0,
	.freq_tbl = ftbl_gpu_cc_gmu_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_gmu_clk_src",
		.parent_names = gpu_cc_parent_names_0,
		.num_parents = 6,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		VDD_CX_FMAX_MAP2(
			MIN, 200000000,
			LOW, 400000000),
	},
};

static struct clk_fixed_factor crc_div = {
	.mult = 1,
	.div = 1,
	.hw.init = &(struct clk_init_data){
		.name = "crc_div",
		.parent_names = (const char *[]){ "gpu_cc_pll0_out_even" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static const struct freq_tbl ftbl_gpu_cc_gx_gfx3d_clk_src[] = {
	F(147000000, P_CRC_DIV,  1, 0, 0),
	F(210000000, P_CRC_DIV,  1, 0, 0),
	F(280000000, P_CRC_DIV,  1, 0, 0),
	F(338000000, P_CRC_DIV,  1, 0, 0),
	F(425000000, P_CRC_DIV,  1, 0, 0),
	F(487000000, P_CRC_DIV,  1, 0, 0),
	F(548000000, P_CRC_DIV,  1, 0, 0),
	F(600000000, P_CRC_DIV,  1, 0, 0),
	{ }
};

static const struct freq_tbl  ftbl_gpu_cc_gx_gfx3d_clk_src_sdm845_v2[] = {
	F(180000000, P_CRC_DIV,  1, 0, 0),
	F(257000000, P_CRC_DIV,  1, 0, 0),
	F(342000000, P_CRC_DIV,  1, 0, 0),
	F(414000000, P_CRC_DIV,  1, 0, 0),
	F(520000000, P_CRC_DIV,  1, 0, 0),
	F(596000000, P_CRC_DIV,  1, 0, 0),
	F(675000000, P_CRC_DIV,  1, 0, 0),
	F(710000000, P_CRC_DIV,  1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gpu_cc_gx_gfx3d_clk_src_sdm670[] = {
	F(180000000, P_CRC_DIV,  1, 0, 0),
	F(267000000, P_CRC_DIV,  1, 0, 0),
	F(355000000, P_CRC_DIV,  1, 0, 0),
	F(430000000, P_CRC_DIV,  1, 0, 0),
	F(565000000, P_CRC_DIV,  1, 0, 0),
	F(650000000, P_CRC_DIV,  1, 0, 0),
	F(750000000, P_CRC_DIV,  1, 0, 0),
	F(780000000, P_CRC_DIV,  1, 0, 0),
	{ }
};

static struct clk_rcg2 gpu_cc_gx_gfx3d_clk_src = {
	.cmd_rcgr = 0x101c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_2,
	.freq_tbl = ftbl_gpu_cc_gx_gfx3d_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_gx_gfx3d_clk_src",
		.parent_names = gpu_cc_parent_names_2,
		.num_parents = 7,
		.flags = CLK_SET_RATE_PARENT,
		.ops =  &clk_rcg2_ops,
		VDD_GX_FMAX_MAP8(
			MIN, 147000000,
			LOWER, 210000000,
			LOW, 280000000,
			LOW_L1, 338000000,
			NOMINAL, 425000000,
			NOMINAL_L1, 487000000,
			HIGH, 548000000,
			HIGH_L1, 600000000),
	},
};

static struct clk_branch gpu_cc_acd_ahb_clk = {
	.halt_reg = 0x1168,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1168,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_acd_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_acd_cxo_clk = {
	.halt_reg = 0x1164,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1164,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_acd_cxo_clk",
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

static struct clk_branch gpu_cc_cx_gfx3d_clk = {
	.halt_reg = 0x10a4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10a4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_gfx3d_clk",
			.parent_names = (const char *[]){
				"gpu_cc_gx_gfx3d_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_cx_gfx3d_slv_clk = {
	.halt_reg = 0x10a8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x10a8,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_cx_gfx3d_slv_clk",
			.parent_names = (const char *[]){
				"gpu_cc_gx_gfx3d_clk_src",
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

static struct clk_branch gpu_cc_gx_gfx3d_clk = {
	.halt_reg = 0x1054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gx_gfx3d_clk",
			.parent_names = (const char *[]){
				"gpu_cc_gx_gfx3d_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
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

static struct clk_regmap *gpu_cc_sdm845_clocks[] = {
	[GPU_CC_ACD_AHB_CLK] = &gpu_cc_acd_ahb_clk.clkr,
	[GPU_CC_ACD_CXO_CLK] = &gpu_cc_acd_cxo_clk.clkr,
	[GPU_CC_CRC_AHB_CLK] = &gpu_cc_crc_ahb_clk.clkr,
	[GPU_CC_CX_APB_CLK] = &gpu_cc_cx_apb_clk.clkr,
	[GPU_CC_CX_GFX3D_CLK] = &gpu_cc_cx_gfx3d_clk.clkr,
	[GPU_CC_CX_GFX3D_SLV_CLK] = &gpu_cc_cx_gfx3d_slv_clk.clkr,
	[GPU_CC_CX_GMU_CLK] = &gpu_cc_cx_gmu_clk.clkr,
	[GPU_CC_CX_SNOC_DVM_CLK] = &gpu_cc_cx_snoc_dvm_clk.clkr,
	[GPU_CC_CXO_AON_CLK] = &gpu_cc_cxo_aon_clk.clkr,
	[GPU_CC_CXO_CLK] = &gpu_cc_cxo_clk.clkr,
	[GPU_CC_GMU_CLK_SRC] = &gpu_cc_gmu_clk_src.clkr,
	[GPU_CC_GX_GMU_CLK] = &gpu_cc_gx_gmu_clk.clkr,
	[GPU_CC_GX_VSENSE_CLK] = &gpu_cc_gx_vsense_clk.clkr,
	[GPU_CC_PLL_TEST_CLK] = &gpu_cc_pll_test_clk.clkr,
	[GPU_CC_PLL0] = &gpu_cc_pll0.clkr,
	[GPU_CC_PLL1] = NULL,
};

static struct clk_regmap *gpu_cc_gfx_sdm845_clocks[] = {
	[GPU_CC_PLL0_OUT_EVEN] = &gpu_cc_pll0_out_even.clkr,
	[GPU_CC_GX_GFX3D_CLK_SRC] = &gpu_cc_gx_gfx3d_clk_src.clkr,
	[GPU_CC_GX_GFX3D_CLK] = &gpu_cc_gx_gfx3d_clk.clkr,
};

static const struct qcom_reset_map gpu_cc_sdm845_resets[] = {
	[GPUCC_GPU_CC_ACD_BCR] = { 0x1160 },
	[GPUCC_GPU_CC_CX_BCR] = { 0x1068 },
	[GPUCC_GPU_CC_GFX3D_AON_BCR] = { 0x10a0 },
	[GPUCC_GPU_CC_GMU_BCR] = { 0x111c },
	[GPUCC_GPU_CC_GX_BCR] = { 0x1008 },
	[GPUCC_GPU_CC_SPDM_BCR] = { 0x1110 },
	[GPUCC_GPU_CC_XO_BCR] = { 0x1000 },
};

static const struct regmap_config gpu_cc_sdm845_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x8008,
	.fast_io	= true,
};

static const struct qcom_cc_desc gpu_cc_sdm845_desc = {
	.config = &gpu_cc_sdm845_regmap_config,
	.clks = gpu_cc_sdm845_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_sdm845_clocks),
	.resets = gpu_cc_sdm845_resets,
	.num_resets = ARRAY_SIZE(gpu_cc_sdm845_resets),
};

static const struct qcom_cc_desc gpu_cc_gfx_sdm845_desc = {
	.config = &gpu_cc_sdm845_regmap_config,
	.clks = gpu_cc_gfx_sdm845_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_gfx_sdm845_clocks),
};

static const struct of_device_id gpu_cc_sdm845_match_table[] = {
	{ .compatible = "qcom,gpucc-sdm845" },
	{ .compatible = "qcom,gpucc-sdm845-v2" },
	{ .compatible = "qcom,gpucc-sdm670" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_sdm845_match_table);

static const struct of_device_id gpu_cc_gfx_sdm845_match_table[] = {
	{ .compatible = "qcom,gfxcc-sdm845" },
	{ .compatible = "qcom,gfxcc-sdm845-v2" },
	{ .compatible = "qcom,gfxcc-sdm670" },
	{},
};
MODULE_DEVICE_TABLE(of, gpu_cc_gfx_sdm845_match_table);

static void gpu_cc_sdm845_fixup_sdm845v2(struct regmap *regmap)
{
	gpu_cc_sdm845_clocks[GPU_CC_PLL1] = &gpu_cc_pll1.clkr;
	clk_fabia_pll_configure(&gpu_cc_pll1, regmap, &gpu_cc_pll1_config);

	gpu_cc_gmu_clk_src.freq_tbl = ftbl_gpu_cc_gmu_clk_src_sdm845_v2;
	gpu_cc_gmu_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 500000000;
}

static void gpu_cc_sdm845_fixup_sdm670(struct regmap *regmap)
{
	gpu_cc_sdm845_clocks[GPU_CC_PLL1] = &gpu_cc_pll1.clkr;
	clk_fabia_pll_configure(&gpu_cc_pll1, regmap, &gpu_cc_pll1_config);

	gpu_cc_gmu_clk_src.freq_tbl = ftbl_gpu_cc_gmu_clk_src_sdm670;
	gpu_cc_gmu_clk_src.clkr.hw.init->rate_max[VDD_CX_LOW] = 200000000;
}

static void gpu_cc_gfx_sdm845_fixup_sdm845v2(void)
{
	gpu_cc_gx_gfx3d_clk_src.freq_tbl =
				ftbl_gpu_cc_gx_gfx3d_clk_src_sdm845_v2;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_MIN] = 180000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_LOWER] =
		257000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_LOW] = 342000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_LOW_L1] =
		414000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_NOMINAL] =
		520000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_NOMINAL_L1] =
		596000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_HIGH] = 675000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_HIGH_L1] =
		710000000;
}

static void gpu_cc_gfx_sdm845_fixup_sdm670(void)
{
	gpu_cc_gx_gfx3d_clk_src.freq_tbl =
				ftbl_gpu_cc_gx_gfx3d_clk_src_sdm670;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_MIN] = 180000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_LOWER] =
		267000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_LOW] = 355000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_LOW_L1] =
		430000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_NOMINAL] =
		565000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_NOMINAL_L1] =
		650000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_HIGH] = 750000000;
	gpu_cc_gx_gfx3d_clk_src.clkr.hw.init->rate_max[VDD_GX_HIGH_L1] =
		780000000;
}

static int gpu_cc_gfx_sdm845_fixup(struct platform_device *pdev)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,gfxcc-sdm845-v2"))
		gpu_cc_gfx_sdm845_fixup_sdm845v2();
	else if (!strcmp(compat, "qcom,gfxcc-sdm670"))
		gpu_cc_gfx_sdm845_fixup_sdm670();

	return 0;
}

static int gpu_cc_sdm845_fixup(struct platform_device *pdev,
					struct regmap *regmap)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || (compatlen <= 0))
		return -EINVAL;

	if (!strcmp(compat, "qcom,gpucc-sdm845-v2"))
		gpu_cc_sdm845_fixup_sdm845v2(regmap);
	else if (!strcmp(compat, "qcom,gpucc-sdm670"))
		gpu_cc_sdm845_fixup_sdm670(regmap);

	return 0;
}

static int gpu_cc_gfx_sdm845_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	struct resource *res;
	void __iomem *base;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get resources for clock_gfxcc\n");
		return -EINVAL;
	}

	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed to ioremap the GFX CC base\n");
		return PTR_ERR(base);
	}

	/* Register clock fixed factor for CRC divide. */
	ret = devm_clk_hw_register(&pdev->dev, &crc_div.hw);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register hardware clock\n");
		return ret;
	}

	regmap = devm_regmap_init_mmio(&pdev->dev, base,
				gpu_cc_gfx_sdm845_desc.config);
	if (IS_ERR(regmap)) {
		dev_err(&pdev->dev, "Failed to init regmap\n");
		return PTR_ERR(regmap);
	}

	/* GFX voltage regulators for GFX3D  graphic clock. */
	vdd_gfx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_gfx");
	if (IS_ERR(vdd_gfx.regulator[0])) {
		if (PTR_ERR(vdd_gfx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_gfx regulator\n");
		return PTR_ERR(vdd_gfx.regulator[0]);
	}

	/* Avoid turning on the rail during clock registration */
	vdd_gfx.skip_handoff = true;

	ret = gpu_cc_gfx_sdm845_fixup(pdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to do GFX clock fixup\n");
		return ret;
	}

	clk_fabia_pll_configure(&gpu_cc_pll0, regmap, &gpu_cc_pll0_config);

	ret = qcom_cc_really_probe(pdev, &gpu_cc_gfx_sdm845_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GFX CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered GFX CC clocks\n");

	return ret;
}

static struct platform_driver gpu_cc_gfx_sdm845_driver = {
	.probe = gpu_cc_gfx_sdm845_probe,
	.driver = {
		.name = "gfxcc-sdm845",
		.of_match_table = gpu_cc_gfx_sdm845_match_table,
	},
};

static int __init gpu_cc_gfx_sdm845_init(void)
{
	return platform_driver_register(&gpu_cc_gfx_sdm845_driver);
}
subsys_initcall(gpu_cc_gfx_sdm845_init);

static void __exit gpu_cc_gfx_sdm845_exit(void)
{
	platform_driver_unregister(&gpu_cc_gfx_sdm845_driver);
}
module_exit(gpu_cc_gfx_sdm845_exit);

static int gpu_cc_sdm845_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret = 0;
	unsigned int value, mask;

	regmap = qcom_cc_map(pdev, &gpu_cc_sdm845_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	/* Get CX voltage regulator for CX and GMU clocks. */
	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_cx regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	/* Get MX voltage regulator for GPU PLL graphic clock. */
	vdd_mx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(vdd_mx.regulator[0])) {
		if (!(PTR_ERR(vdd_mx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_mx regulator\n");
		return PTR_ERR(vdd_mx.regulator[0]);
	}

	ret = gpu_cc_sdm845_fixup(pdev, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to do GPU CC clock fixup\n");
		return ret;
	}

	ret = qcom_cc_really_probe(pdev, &gpu_cc_sdm845_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPU CC clocks\n");
		return ret;
	}

	mask = CX_GMU_CBCR_WAKE_MASK << CX_GMU_CBCR_WAKE_SHIFT;
	mask |= CX_GMU_CBCR_SLEEP_MASK << CX_GMU_CBCR_SLEEP_SHIFT;
	value = 0xF << CX_GMU_CBCR_WAKE_SHIFT | 0xF << CX_GMU_CBCR_SLEEP_SHIFT;
	regmap_update_bits(regmap, gpu_cc_cx_gmu_clk.clkr.enable_reg,
								mask, value);

	dev_info(&pdev->dev, "Registered GPU CC clocks\n");

	return ret;
}

static struct platform_driver gpu_cc_sdm845_driver = {
	.probe = gpu_cc_sdm845_probe,
	.driver = {
		.name = "gpu_cc-sdm845",
		.of_match_table = gpu_cc_sdm845_match_table,
	},
};

static int __init gpu_cc_sdm845_init(void)
{
	return platform_driver_register(&gpu_cc_sdm845_driver);
}
subsys_initcall(gpu_cc_sdm845_init);

static void __exit gpu_cc_sdm845_exit(void)
{
	platform_driver_unregister(&gpu_cc_sdm845_driver);
}
module_exit(gpu_cc_sdm845_exit);

MODULE_DESCRIPTION("QTI GPU_CC SDM845 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gpu_cc-sdm845");

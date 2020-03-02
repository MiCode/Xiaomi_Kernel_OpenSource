// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.*/

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msm-bus.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include <dt-bindings/clock/qcom,videocc-kona.h>
#include <dt-bindings/msm/msm-bus-ids.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

#define MSM_BUS_VECTOR(_src, _dst, _ab, _ib)	\
{						\
	.src = _src,				\
	.dst = _dst,				\
	.ab = _ab,				\
	.ib = _ib,				\
}

static DEFINE_VDD_REGULATORS(vdd_mm, VDD_NUM_MM, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mx, VDD_NUM, 1, vdd_corner);

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
	P_VIDEO_PLL0_OUT_MAIN,
	P_VIDEO_PLL1_OUT_MAIN,
};

static const struct parent_map video_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const video_cc_parent_names_0_ao[] = {
	"bi_tcxo_ao",
	"core_bi_pll_test_se",
};

static const struct parent_map video_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_VIDEO_PLL0_OUT_MAIN, 1 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const video_cc_parent_names_1[] = {
	"bi_tcxo",
	"video_pll0",
	"core_bi_pll_test_se",
};

static const struct parent_map video_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_VIDEO_PLL1_OUT_MAIN, 1 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const video_cc_parent_names_2[] = {
	"bi_tcxo",
	"video_pll1",
	"core_bi_pll_test_se",
};

static const struct parent_map video_cc_parent_map_3[] = {
	{ P_CHIP_SLEEP_CLK, 0 },
	{ P_CORE_BI_PLL_TEST_SE, 7 },
};

static const char * const video_cc_parent_names_3[] = {
	"chip_sleep_clk",
	"core_bi_pll_test_se",
};

static struct pll_vco lucid_vco[] = {
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
		.hw.init = &(struct clk_init_data){
			.name = "video_pll0",
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

static const struct alpha_pll_config video_pll1_config = {
	.l = 0x29,
	.cal_l = 0x44,
	.alpha = 0xFAAA,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0x329A699C,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000805,
	.user_ctl_hi1_val = 0x00000000,
};

static const struct alpha_pll_config video_pll1_config_sm8250_v2 = {
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
		.hw.init = &(struct clk_init_data){
			.name = "video_pll1",
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

static struct clk_regmap_div video_cc_mvs0_div_clk_src = {
	.reg = 0xd54,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "video_cc_mvs0_div_clk_src",
		.parent_names = (const char *[]){ "video_cc_mvs0_clk_src" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div video_cc_mvs0c_div2_div_clk_src = {
	.reg = 0xc54,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "video_cc_mvs0c_div2_div_clk_src",
		.parent_names = (const char *[]){ "video_cc_mvs0_clk_src" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div video_cc_mvs1_div_clk_src = {
	.reg = 0xdd4,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "video_cc_mvs1_div_clk_src",
		.parent_names = (const char *[]){ "video_cc_mvs1_clk_src" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div video_cc_mvs1c_div2_div_clk_src = {
	.reg = 0xcf4,
	.shift = 0,
	.width = 2,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "video_cc_mvs1c_div2_div_clk_src",
		.parent_names = (const char *[]){ "video_cc_mvs1_clk_src" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
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
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_ahb_clk_src",
		.parent_names = video_cc_parent_names_0_ao,
		.num_parents = 2,
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
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_mvs0_clk_src",
		.parent_names = video_cc_parent_names_1,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
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
	F(806000000, P_VIDEO_PLL1_OUT_MAIN, 1, 0, 0),
	F(1040000000, P_VIDEO_PLL1_OUT_MAIN, 1, 0, 0),
	F(1098000000, P_VIDEO_PLL1_OUT_MAIN, 1, 0, 0),
	F(1332000000, P_VIDEO_PLL1_OUT_MAIN, 1, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_video_cc_mvs1_clk_src_kona_v2[] = {
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
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_mvs1_clk_src",
		.parent_names = video_cc_parent_names_2,
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 806000000,
			[VDD_LOW] = 1040000000,
			[VDD_LOW_L1] = 1098000000,
			[VDD_NOMINAL] = 1332000000},
	},
};

static const struct freq_tbl ftbl_video_cc_sleep_clk_src[] = {
	F(32000, P_CHIP_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_sleep_clk_src = {
	.cmd_rcgr = 0xef0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_3,
	.freq_tbl = ftbl_video_cc_sleep_clk_src,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_sleep_clk_src",
		.parent_names = video_cc_parent_names_3,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
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
	.clkr.hw.init = &(struct clk_init_data){
		.name = "video_cc_xo_clk_src",
		.parent_names = video_cc_parent_names_0_ao,
		.num_parents = 2,
		.flags = CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_branch video_cc_ahb_clk = {
	.halt_reg = 0xe58,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xe58,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_ahb_clk",
			.parent_names = (const char *[]){
				"video_cc_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs0_clk = {
	.halt_reg = 0xd34,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0xd34,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs0_clk",
			.parent_names = (const char *[]){
				"video_cc_mvs0_div_clk_src",
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
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs0c_clk",
			.parent_names = (const char *[]){
				"video_cc_mvs0c_div2_div_clk_src",
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
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs1_clk",
			.parent_names = (const char *[]){
				"video_cc_mvs1_div_clk_src",
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
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs1_div2_clk",
			.parent_names = (const char *[]){
				"video_cc_mvs1c_div2_div_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_mvs1c_clk = {
	.halt_reg = 0xcd4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0xcd4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_mvs1c_clk",
			.parent_names = (const char *[]){
				"video_cc_mvs1c_div2_div_clk_src",
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
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_sleep_clk",
			.parent_names = (const char *[]){
				"video_cc_sleep_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_xo_clk = {
	.halt_reg = 0xeec,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xeec,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "video_cc_xo_clk",
			.parent_names = (const char *[]){
				"video_cc_xo_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *video_cc_kona_clocks[] = {
	[VIDEO_CC_AHB_CLK] = &video_cc_ahb_clk.clkr,
	[VIDEO_CC_AHB_CLK_SRC] = &video_cc_ahb_clk_src.clkr,
	[VIDEO_CC_MVS0_CLK] = &video_cc_mvs0_clk.clkr,
	[VIDEO_CC_MVS0_CLK_SRC] = &video_cc_mvs0_clk_src.clkr,
	[VIDEO_CC_MVS0_DIV_CLK_SRC] = &video_cc_mvs0_div_clk_src.clkr,
	[VIDEO_CC_MVS0C_CLK] = &video_cc_mvs0c_clk.clkr,
	[VIDEO_CC_MVS0C_DIV2_DIV_CLK_SRC] =
		&video_cc_mvs0c_div2_div_clk_src.clkr,
	[VIDEO_CC_MVS1_CLK] = &video_cc_mvs1_clk.clkr,
	[VIDEO_CC_MVS1_CLK_SRC] = &video_cc_mvs1_clk_src.clkr,
	[VIDEO_CC_MVS1_DIV2_CLK] = &video_cc_mvs1_div2_clk.clkr,
	[VIDEO_CC_MVS1_DIV_CLK_SRC] = &video_cc_mvs1_div_clk_src.clkr,
	[VIDEO_CC_MVS1C_CLK] = &video_cc_mvs1c_clk.clkr,
	[VIDEO_CC_MVS1C_DIV2_DIV_CLK_SRC] =
		&video_cc_mvs1c_div2_div_clk_src.clkr,
	[VIDEO_CC_SLEEP_CLK] = &video_cc_sleep_clk.clkr,
	[VIDEO_CC_SLEEP_CLK_SRC] = &video_cc_sleep_clk_src.clkr,
	[VIDEO_CC_XO_CLK] = &video_cc_xo_clk.clkr,
	[VIDEO_CC_XO_CLK_SRC] = &video_cc_xo_clk_src.clkr,
	[VIDEO_PLL0] = &video_pll0.clkr,
	[VIDEO_PLL1] = &video_pll1.clkr,
};

static const struct qcom_reset_map video_cc_kona_resets[] = {
	[CVP_VIDEO_CC_INTERFACE_BCR] = { 0xe54 },
	[CVP_VIDEO_CC_MVS0_BCR] = { 0xd14 },
	[VIDEO_CC_MVS0C_CLK_ARES] = { 0xc34, 2 },
	[CVP_VIDEO_CC_MVS0C_BCR] = { 0xbf4 },
	[CVP_VIDEO_CC_MVS1_BCR] = { 0xd94 },
	[VIDEO_CC_MVS1C_CLK_ARES] = { 0xcd4, 2 },
	[CVP_VIDEO_CC_MVS1C_BCR] = { 0xc94 },
};

static const struct regmap_config video_cc_kona_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xf4c,
	.fast_io = true,
};

static const struct qcom_cc_desc video_cc_kona_desc = {
	.config = &video_cc_kona_regmap_config,
	.clks = video_cc_kona_clocks,
	.num_clks = ARRAY_SIZE(video_cc_kona_clocks),
	.resets = video_cc_kona_resets,
	.num_resets = ARRAY_SIZE(video_cc_kona_resets),
};

static const struct of_device_id video_cc_kona_match_table[] = {
	{ .compatible = "qcom,videocc-kona" },
	{ .compatible = "qcom,videocc-kona-v2" },
	{ }
};
MODULE_DEVICE_TABLE(of, video_cc_kona_match_table);

static void video_cc_kona_fixup_konav2(struct regmap *regmap)
{
	clk_lucid_pll_configure(&video_pll1, regmap,
		&video_pll1_config_sm8250_v2);
	video_cc_mvs1_clk_src.freq_tbl = ftbl_video_cc_mvs1_clk_src_kona_v2;
	video_cc_mvs1_clk_src.clkr.hw.init->rate_max[VDD_LOWER] = 840000000;
	video_cc_mvs1_clk_src.clkr.hw.init->rate_max[VDD_LOW] = 1098000000;
}

static int video_cc_kona_probe(struct platform_device *pdev)
{
	unsigned int videocc_bus_id;
	struct regmap *regmap;
	struct clk *clk;
	int ret, i;

	regmap = qcom_cc_map(pdev, &video_cc_kona_desc);
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

	vdd_mx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(vdd_mx.regulator[0])) {
		if (PTR_ERR(vdd_mx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_mx regulator\n");
		return PTR_ERR(vdd_mx.regulator[0]);
	}

	vdd_mm.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_mm");
	if (IS_ERR(vdd_mm.regulator[0])) {
		if (PTR_ERR(vdd_mm.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_mm regulator\n");
		return PTR_ERR(vdd_mm.regulator[0]);
	}

	videocc_bus_id =
		msm_bus_scale_register_client(&clk_debugfs_scale_table);
	if (!videocc_bus_id) {
		dev_err(&pdev->dev, "Unable to register for bw voting\n");
		return -EPROBE_DEFER;
	}

	for (i = 0; i < ARRAY_SIZE(video_cc_kona_clocks); i++)
		if (video_cc_kona_clocks[i])
			*(unsigned int *)(void *)
			&video_cc_kona_clocks[i]->hw.init->bus_cl_id =
							videocc_bus_id;

	clk_lucid_pll_configure(&video_pll0, regmap, &video_pll0_config);
	clk_lucid_pll_configure(&video_pll1, regmap, &video_pll1_config);

	if (of_device_is_compatible(pdev->dev.of_node,
				"qcom,videocc-kona-v2"))
		video_cc_kona_fixup_konav2(regmap);

	ret = qcom_cc_really_probe(pdev, &video_cc_kona_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register Video CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Video CC clocks\n");

	return ret;
}

static struct platform_driver video_cc_kona_driver = {
	.probe	= video_cc_kona_probe,
	.driver	= {
		.name = "video_cc-kona",
		.of_match_table = video_cc_kona_match_table,
	},
};

static int __init video_cc_kona_init(void)
{
	return platform_driver_register(&video_cc_kona_driver);
}
subsys_initcall(video_cc_kona_init);

static void __exit video_cc_kona_exit(void)
{
	platform_driver_unregister(&video_cc_kona_driver);
}
module_exit(video_cc_kona_exit);

MODULE_DESCRIPTION("QTI VIDEO_CC KONA Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:video_cc-kona");

/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clock-generic.h>
#include <linux/of_platform.h>

#include <soc/qcom/clock-alpha-pll.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/clock-rpm.h>

#include <dt-bindings/clock/msm-clocks-8996.h>
#include <dt-bindings/clock/msm-clocks-hwio-8996.h>

#include "vdd-level-8994.h"
#include "clock.h"

static void __iomem *virt_base;
static void __iomem *virt_base_gpu;

#define mmpll0_out_main_mm_source_val		1
#define mmpll1_out_main_mm_source_val		2
#define mmpll2_out_main_mm_source_val		3
#define mmpll3_out_main_mm_source_val		3
#define mmpll4_out_main_mm_source_val		3
#define mmpll5_out_main_mm_source_val		2
#define mmpll8_out_main_mm_source_val		4
#define mmpll9_out_main_mm_source_val		2

#define mmsscc_xo_mm_source_val			0
#define mmsscc_gpll0_mm_source_val		5
#define mmsscc_gpll0_div_mm_source_val		6
#define dsi0phypll_mm_source_val		1
#define dsi1phypll_mm_source_val		2
#define ext_extpclk_clk_src_mm_source_val	1

#define FIXDIV(div) (div ? (2 * (div) - 1) : (0))

#define F_MM(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)FIXDIV(div)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

#define GFX_MIN_SVS_LEVEL	2
#define GPU_REQ_ID		0x3

#define EFUSE_SHIFT_v3	29
#define EFUSE_MASK_v3	0x7
#define EFUSE_SHIFT_PRO	28
#define EFUSE_MASK_PRO	0x3

static struct clk_ops clk_ops_gpu;

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

static int vdd_mmpll4_levels[] = {
	RPM_REGULATOR_CORNER_NONE,		0,
	RPM_REGULATOR_CORNER_SVS_KRAIT,		1800000,
	RPM_REGULATOR_CORNER_SVS_SOC,		1800000,
	RPM_REGULATOR_CORNER_NORMAL,		1800000,
	RPM_REGULATOR_CORNER_SUPER_TURBO,	1800000,
};

static DEFINE_VDD_REGULATORS(vdd_mmpll4, VDD_DIG_NUM, 2, vdd_mmpll4_levels,
									NULL);
DEFINE_VDD_REGS_INIT(vdd_gfx, 2);
DEFINE_VDD_REGS_INIT(vdd_gpu_mx, 1);

static struct alpha_pll_masks pll_masks_p = {
	.lock_mask = BIT(31),
	.active_mask = BIT(30),
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
};

static struct alpha_pll_vco_tbl mmpll_p_vco[] = {
	VCO(3,  250000000,  500000000),
	VCO(2,  500000000, 1000000000),
	VCO(1, 1000000000, 1500000000),
	VCO(0, 1500000000, 2000000000),
};

static struct alpha_pll_vco_tbl mmpll_gfx_vco[] = {
	VCO(2,  400000000, 1000000000),
	VCO(1, 1000000000, 1500000000),
	VCO(0, 1500000000, 2000000000),
};

static struct alpha_pll_masks pll_masks_t = {
	.lock_mask = BIT(31),
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
};

static struct alpha_pll_masks pll_masks_b = {
	.lock_mask = BIT(31),
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
	.post_div_mask = BM(11, 8),
	.update_mask = BIT(22),
};

static struct alpha_pll_vco_tbl mmpll_t_vco[] = {
	VCO(0, 500000000, 1500000000),
};

DEFINE_EXT_CLK(mmsscc_xo, NULL);
DEFINE_EXT_CLK(mmsscc_gpll0, NULL);
DEFINE_EXT_CLK(mmsscc_gpll0_div, NULL);

static struct alpha_pll_clk mmpll0 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMSS_MMPLL0_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.fsm_reg_offset = MMSS_MMSS_PLL_VOTE_APCS,
	.fsm_en_mask = BIT(0),
	.enable_config = 0x1,
	.c = {
		.rate = 800000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll0",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP2(LOWER, 400000000, NOMINAL, 800000000),
		CLK_INIT(mmpll0.c),
	},
};
DEFINE_EXT_CLK(mmpll0_out_main, &mmpll0.c);

static struct alpha_pll_clk mmpll1 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMSS_MMPLL1_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.fsm_reg_offset = MMSS_MMSS_PLL_VOTE_APCS,
	.fsm_en_mask = BIT(1),
	.enable_config = 0x1,
	.c = {
		.rate = 740000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll1",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP2(LOWER, 370000000, NOMINAL, 740000000),
		CLK_INIT(mmpll1.c),
	},
};
DEFINE_EXT_CLK(mmpll1_out_main, &mmpll1.c);

static struct alpha_pll_clk mmpll4 = {
	.masks = &pll_masks_t,
	.base = &virt_base,
	.offset = MMSS_MMPLL4_MODE,
	.vco_tbl = mmpll_t_vco,
	.num_vco = ARRAY_SIZE(mmpll_t_vco),
	.enable_config = 0x1,
	.c = {
		.parent = &mmsscc_xo.c,
		.rate = 960000000,
		.dbg_name = "mmpll4",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_MMPLL4_FMAX_MAP2(LOWER, 480000000, NOMINAL, 960000000),
		CLK_INIT(mmpll4.c),
	},
};
DEFINE_EXT_CLK(mmpll4_out_main, &mmpll4.c);

static struct alpha_pll_clk mmpll3 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMSS_MMPLL3_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.enable_config = 0x1,
	.c = {
		.parent = &mmsscc_xo.c,
		.rate = 900000000,
		.dbg_name = "mmpll3",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP2(LOWER, 450000000, NOMINAL, 900000000),
		CLK_INIT(mmpll3.c),
	},
};
DEFINE_EXT_CLK(mmpll3_out_main, &mmpll3.c);

static struct clk_freq_tbl ftbl_ahb_clk_src[] = {
	F_MM(  19200000,        mmsscc_xo,     1,    0,     0),
	F_MM(  40000000,  mmsscc_gpll0_div,  7.5,    0,     0),
	F_MM(  80000000,   mmpll0_out_main,   10,    0,     0),
	F_END
};

static struct rcg_clk ahb_clk_src = {
	.cmd_rcgr_reg = MMSS_AHB_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ahb_clk_src,
	.current_freq = &rcg_dummy_freq,
	.non_local_children = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "ahb_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 40000000, NOMINAL, 80000000),
		CLK_INIT(ahb_clk_src.c),
	},
};

static struct alpha_pll_clk mmpll2 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMSS_MMPLL2_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.enable_config = 0x1,
	.c = {
		.parent = &mmsscc_xo.c,
		.rate = 410000000,
		.dbg_name = "mmpll2",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP1(LOWER, 410000000),
		CLK_INIT(mmpll2.c),
	},
};
DEFINE_EXT_CLK(mmpll2_out_main, &mmpll2.c);

static struct div_clk mmpll2_postdiv_clk = {
	.base = &virt_base,
	.offset = MMSS_MMPLL2_USER_CTL_MODE,
	.mask = 0xf,
	.shift = 8,
	.data = {
		.max_div = 4,
		.min_div = 1,
		.skip_odd_div = true,
		.allow_div_one = true,
	},
	.ops = &postdiv_reg_ops,
	.c = {
		.parent = &mmpll2_out_main.c,
		.dbg_name = "mmpll2_postdiv_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mmpll2_postdiv_clk.c),
	},
};

static struct alpha_pll_clk mmpll8 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMSS_MMPLL8_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.enable_config = 0x1,
	.c = {
		.parent = &mmsscc_xo.c,
		.rate = 360000000,
		.dbg_name = "mmpll8",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP1(LOWER, 360000000),
		CLK_INIT(mmpll8.c),
	},
};
DEFINE_EXT_CLK(mmpll8_out_main, &mmpll8.c);

static struct div_clk mmpll8_postdiv_clk = {
	.base = &virt_base,
	.offset = MMSS_MMPLL8_USER_CTL_MODE,
	.mask = 0xf,
	.shift = 8,
	.data = {
		.max_div = 4,
		.min_div = 1,
		.skip_odd_div = true,
		.allow_div_one = true,
	},
	.ops = &postdiv_reg_ops,
	.c = {
		.parent = &mmpll8_out_main.c,
		.dbg_name = "mmpll8_postdiv_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mmpll8_postdiv_clk.c),
	},
};

static struct alpha_pll_clk mmpll9 = {
	.masks = &pll_masks_b,
	.base = &virt_base,
	.offset = MMSS_MMPLL9_MODE,
	.vco_tbl = mmpll_t_vco,
	.num_vco = ARRAY_SIZE(mmpll_t_vco),
	.post_div_config = 0x100,
	.enable_config = 0x1,
	.dynamic_update = true,
	.c = {
		.parent = &mmsscc_xo.c,
		.rate = 960000000,
		.dbg_name = "mmpll9",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_MMPLL4_FMAX_MAP2(LOWER, 480000000, NOMINAL, 960000000),
		CLK_INIT(mmpll9.c),
	},
};
DEFINE_EXT_CLK(mmpll9_out_main, &mmpll9.c);

static struct div_clk mmpll9_postdiv_clk = {
	.base = &virt_base,
	.offset = MMSS_MMPLL9_USER_CTL_MODE,
	.mask = 0xf,
	.shift = 8,
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.ops = &postdiv_reg_ops,
	.c = {
		.parent = &mmpll9_out_main.c,
		.dbg_name = "mmpll9_postdiv_clk",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mmpll9_postdiv_clk.c),
	},
};

static struct alpha_pll_clk mmpll5 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMSS_MMPLL5_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.enable_config = 0x1,
	.c = {
		.parent = &mmsscc_xo.c,
		.rate = 720000000,
		.dbg_name = "mmpll5",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP2(LOWER, 360000000, LOW, 720000000),
		CLK_INIT(mmpll5.c),
	},
};
DEFINE_EXT_CLK(mmpll5_out_main, &mmpll5.c);

static struct clk_freq_tbl ftbl_gfx3d_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_MM(  60000000, mmpll8_out_main,    6,    0,     0),
	F_MM( 120000000, mmpll8_out_main,    3,    0,     0),
	F_MM( 205000000, mmpll2_out_main,    2,    0,     0),
	F_MM( 360000000, mmpll8_out_main,    1,    0,     0),
	F_MM( 480000000, mmpll9_out_main,    1,    0,     0),
	F_END
};

static struct rcg_clk gfx3d_clk_src = {
	.cmd_rcgr_reg = MMSS_GFX3D_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gfx3d_clk_src,
	.current_freq = &rcg_dummy_freq,
	.force_enable_rcgr = true,
	.base = &virt_base_gpu,
	.c = {
		.dbg_name = "gfx3d_clk_src",
		.ops = &clk_ops_gpu,
		.vdd_class = &vdd_gfx,
		CLK_INIT(gfx3d_clk_src.c),
	},
};

static struct mux_div_clk gfx3d_clk_src_v2 = {
	.div_offset = MMSS_GFX3D_CMD_RCGR,
	.div_mask = BM(4, 0),
	.src_mask = BM(10, 8) >> 8,
	.src_shift = 8,
	.ops = &rcg_mux_div_ops,
	.try_get_rate = true,
	.force_enable_md = true,
	.data = {
		.min_div = 1,
		.max_div = 1,
	},
	.parents = (struct clk_src[]) {
		{&mmpll9_postdiv_clk.c, 2},
		{&mmpll2_postdiv_clk.c, 3},
		{&mmpll8_postdiv_clk.c, 4},
	},
	.num_parents = 3,
	.c = {
		.dbg_name = "gfx3d_clk_src_v2",
		.ops = &clk_ops_gpu,
		.vdd_class = &vdd_gfx,
		CLK_INIT(gfx3d_clk_src_v2.c),
	},
};

static struct clk_freq_tbl ftbl_csi0_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 274290000,  mmpll4_out_main,  3.5,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi0_clk_src_v2[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi0_clk_src_v3[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct rcg_clk csi0_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 480000000, HIGH, 600000000),
		CLK_INIT(csi0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vfe0_clk_src[] = {
	F_MM(  75000000,   mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 100000000,   mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,       mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,    mmpll0_out_main,  2.5,    0,     0),
	F_MM( 400000000,    mmpll0_out_main,    2,    0,     0),
	F_MM( 480000000,    mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,       mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_vfe0_clk_src_v2[] = {
	F_MM(  75000000,   mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 100000000,   mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,       mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,    mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,    mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,       mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_vfe0_clk_src_v3[] = {
	F_MM(  75000000,   mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 100000000,   mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 300000000,       mmsscc_gpll0,    2,    0,     0),
	F_MM( 320000000,    mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,    mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,       mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct rcg_clk vfe0_clk_src = {
	.cmd_rcgr_reg = MMSS_VFE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vfe0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "vfe0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 200000000,
					NOMINAL, 480000000, HIGH, 600000000),
		CLK_INIT(vfe0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vfe1_clk_src[] = {
	F_MM(  75000000,   mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 100000000,   mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,       mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,    mmpll0_out_main,  2.5,    0,     0),
	F_MM( 400000000,    mmpll0_out_main,    2,    0,     0),
	F_MM( 480000000,    mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,       mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_vfe1_clk_src_v2[] = {
	F_MM(  75000000,   mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 100000000,   mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,       mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,    mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,    mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,       mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_vfe1_clk_src_v3[] = {
	F_MM(  75000000,   mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 100000000,   mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 300000000,       mmsscc_gpll0,    2,    0,     0),
	F_MM( 320000000,    mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,    mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,       mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct rcg_clk vfe1_clk_src = {
	.cmd_rcgr_reg = MMSS_VFE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vfe1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "vfe1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 480000000, HIGH, 600000000),
		CLK_INIT(vfe1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi1_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 274290000,  mmpll4_out_main,  3.5,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi1_clk_src_v2[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi1_clk_src_v3[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct rcg_clk csi1_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 480000000, HIGH, 600000000),
		CLK_INIT(csi1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi2_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 274290000,  mmpll4_out_main,  3.5,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi2_clk_src_v2[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi2_clk_src_v3[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct rcg_clk csi2_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 480000000, HIGH, 600000000),
		CLK_INIT(csi2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi3_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 274290000,  mmpll4_out_main,  3.5,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi3_clk_src_v2[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi3_clk_src_v3[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct rcg_clk csi3_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi3_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi3_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 480000000, HIGH, 600000000),
		CLK_INIT(csi3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_maxi_clk_src[] = {
	F_MM(  19200000,       mmsscc_xo,     1,    0,     0),
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 370000000,  mmpll1_out_main,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_maxi_clk_src_v2[] = {
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 171430000,     mmsscc_gpll0,  3.5,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 405000000,  mmpll1_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk maxi_clk_src = {
	.cmd_rcgr_reg = MMSS_MAXI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_maxi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "maxi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 171430000,
					NOMINAL, 320000000, HIGH, 370000000),
		CLK_INIT(maxi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_cpp_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 640000000,  mmpll4_out_main,  1.5,    0,     0),
	F_END
};

static struct rcg_clk cpp_clk_src = {
	.cmd_rcgr_reg = MMSS_CPP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_cpp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "cpp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 480000000, HIGH, 640000000),
		CLK_INIT(cpp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_jpeg0_clk_src[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 228571429,  mmpll0_out_main,  3.5,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk jpeg0_clk_src = {
	.cmd_rcgr_reg = MMSS_JPEG0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_jpeg0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "jpeg0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 150000000,
					NOMINAL, 320000000, HIGH, 480000000),
		CLK_INIT(jpeg0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_jpeg2_clk_src[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 228571429,  mmpll0_out_main,  3.5,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_END
};

static struct rcg_clk jpeg2_clk_src = {
	.cmd_rcgr_reg = MMSS_JPEG2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_jpeg2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "jpeg2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 150000000,
					NOMINAL, 266670000, HIGH, 320000000),
		CLK_INIT(jpeg2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_jpeg_dma_clk_src[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 228571429,  mmpll0_out_main,  3.5,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk jpeg_dma_clk_src = {
	.cmd_rcgr_reg = MMSS_JPEG_DMA_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_jpeg_dma_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "jpeg_dma_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 150000000,
					NOMINAL, 320000000, HIGH, 480000000),
		CLK_INIT(jpeg_dma_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdp_clk_src[] = {
	F_MM(  85714286,   mmsscc_gpll0,    7,    0,     0),
	F_MM( 100000000,   mmsscc_gpll0,    6,    0,     0),
	F_MM( 150000000,   mmsscc_gpll0,    4,    0,     0),
	F_MM( 171428571,   mmsscc_gpll0,  3.5,    0,     0),
	F_MM( 240000000, mmpll5_out_main,    3,    0,     0),
	F_MM( 320000000, mmpll0_out_main,  2.5,    0,     0),
	F_MM( 360000000, mmpll5_out_main,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_mdp_clk_src_v2[] = {
	F_MM(  85714286,    mmsscc_gpll0,    7,    0,     0),
	F_MM( 100000000,    mmsscc_gpll0,    6,    0,     0),
	F_MM( 150000000,    mmsscc_gpll0,    4,    0,     0),
	F_MM( 171428571,    mmsscc_gpll0,  3.5,    0,     0),
	F_MM( 200000000,    mmsscc_gpll0,    3,    0,     0),
	F_MM( 275000000, mmpll5_out_main,    3,    0,     0),
	F_MM( 300000000,    mmsscc_gpll0,    2,    0,     0),
	F_MM( 330000000, mmpll5_out_main,  2.5,    0,     0),
	F_MM( 412500000, mmpll5_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk mdp_clk_src = {
	.cmd_rcgr_reg = MMSS_MDP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 171430000, LOW, 240000000,
					NOMINAL, 320000000, HIGH, 360000000),
		CLK_INIT(mdp_clk_src.c),
	},
};

DEFINE_EXT_CLK(ext_pclk0_clk_src, NULL);
DEFINE_EXT_CLK(ext_pclk1_clk_src, NULL);
static struct clk_freq_tbl ftbl_pclk0_clk_src[] = {
	{
		.div_src_val = BVAL(10, 8, mmsscc_xo_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &mmsscc_xo.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi0phypll_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &ext_pclk0_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi1phypll_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &ext_pclk1_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct rcg_clk pclk0_clk_src = {
	.cmd_rcgr_reg = MMSS_PCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = ftbl_pclk0_clk_src,
	.freq_tbl = ftbl_pclk0_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "pclk0_clk_src",
		.parent = &ext_pclk0_clk_src.c,
		.ops = &clk_ops_pixel_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 175000000, LOW, 280000000,
							NOMINAL, 350000000),
		CLK_INIT(pclk0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pclk1_clk_src[] = {
	{
		.div_src_val = BVAL(10, 8, mmsscc_xo_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &mmsscc_xo.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi0phypll_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &ext_pclk0_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi1phypll_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &ext_pclk1_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct rcg_clk pclk1_clk_src = {
	.cmd_rcgr_reg = MMSS_PCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = ftbl_pclk1_clk_src,
	.freq_tbl = ftbl_pclk1_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "pclk1_clk_src",
		.parent = &ext_pclk1_clk_src.c,
		.ops = &clk_ops_pixel_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 175000000, LOW, 280000000,
							NOMINAL, 350000000),
		CLK_INIT(pclk1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_video_core_clk_src[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 450000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_video_core_clk_src_v2[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 490000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_video_core_clk_src_v3[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 346666667,  mmpll3_out_main,    3,    0,     0),
	F_MM( 520000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk video_core_clk_src = {
	.cmd_rcgr_reg = MMSS_VIDEO_CORE_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_video_core_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_core_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 150000000,
					NOMINAL, 320000000, HIGH, 450000000),
		CLK_INIT(video_core_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_fd_core_clk_src[] = {
	F_MM( 100000000,  mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,      mmsscc_gpll0,    3,    0,     0),
	F_MM( 400000000,   mmpll0_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk fd_core_clk_src = {
	.cmd_rcgr_reg = MMSS_FD_CORE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_fd_core_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "fd_core_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
							NOMINAL, 400000000),
		CLK_INIT(fd_core_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_cci_clk_src[] = {
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_MM(  37500000,     mmsscc_gpll0,   16,    0,     0),
	F_MM(  50000000,     mmsscc_gpll0,   12,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_END
};

static struct rcg_clk cci_clk_src = {
	.cmd_rcgr_reg = MMSS_CCI_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_cci_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "cci_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 50000000,
							NOMINAL, 100000000),
		CLK_INIT(cci_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csiphy0_3p_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csiphy0_3p_clk_src_v2[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 384000000,  mmpll4_out_main,  2.5,    0,     0),
	F_END
};

static struct rcg_clk csiphy0_3p_clk_src = {
	.cmd_rcgr_reg = MMSS_CSIPHY0_3P_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csiphy0_3p_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csiphy0_3p_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
							NOMINAL, 320000000),
		CLK_INIT(csiphy0_3p_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csiphy1_3p_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csiphy1_3p_clk_src_v2[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 384000000,  mmpll4_out_main,  2.5,    0,     0),
	F_END
};

static struct rcg_clk csiphy1_3p_clk_src = {
	.cmd_rcgr_reg = MMSS_CSIPHY1_3P_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csiphy1_3p_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csiphy1_3p_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
							NOMINAL, 320000000),
		CLK_INIT(csiphy1_3p_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csiphy2_3p_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csiphy2_3p_clk_src_v2[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll4_out_main,    3,    0,     0),
	F_MM( 384000000,  mmpll4_out_main,  2.5,    0,     0),
	F_END
};

static struct rcg_clk csiphy2_3p_clk_src = {
	.cmd_rcgr_reg = MMSS_CSIPHY2_3P_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csiphy2_3p_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csiphy2_3p_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
							NOMINAL, 320000000),
		CLK_INIT(csiphy2_3p_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_gp0_clk_src[] = {
	F_MM(     10000,        mmsscc_xo,   16,    1,   120),
	F_MM(     24000,        mmsscc_xo,   16,    1,    50),
	F_MM(   6000000, mmsscc_gpll0_div,   10,    1,     5),
	F_MM(  12000000, mmsscc_gpll0_div,    1,    1,    25),
	F_MM(  13000000, mmsscc_gpll0_div,    2,   13,   150),
	F_MM(  24000000, mmsscc_gpll0_div,    1,    2,    25),
	F_END
};

static struct rcg_clk camss_gp0_clk_src = {
	.cmd_rcgr_reg = MMSS_CAMSS_GP0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_gp0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
							NOMINAL, 200000000),
		CLK_INIT(camss_gp0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_gp1_clk_src[] = {
	F_MM(     10000,        mmsscc_xo,   16,    1,   120),
	F_MM(     24000,        mmsscc_xo,   16,    1,    50),
	F_MM(   6000000, mmsscc_gpll0_div,   10,    1,     5),
	F_MM(  12000000, mmsscc_gpll0_div,    1,    1,    25),
	F_MM(  13000000, mmsscc_gpll0_div,    2,   13,   150),
	F_MM(  24000000, mmsscc_gpll0_div,    1,    2,    25),
	F_END
};

static struct rcg_clk camss_gp1_clk_src = {
	.cmd_rcgr_reg = MMSS_CAMSS_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
							NOMINAL, 200000000),
		CLK_INIT(camss_gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mclk0_clk_src[] = {
	F_MM(   4800000,         mmsscc_xo,   4,    0,     0),
	F_MM(   6000000,  mmsscc_gpll0_div,  10,    1,     5),
	F_MM(   8000000,  mmsscc_gpll0_div,   1,    2,    75),
	F_MM(   9600000,         mmsscc_xo,   2,    0,     0),
	F_MM(  16666667,  mmsscc_gpll0_div,   2,    1,     9),
	F_MM(  19200000,         mmsscc_xo,   1,    0,     0),
	F_MM(  24000000,  mmsscc_gpll0_div,   1,    2,    25),
	F_MM(  33333333,  mmsscc_gpll0_div,   1,    1,     9),
	F_MM(  48000000,      mmsscc_gpll0,   1,    2,    25),
	F_MM(  66666667,      mmsscc_gpll0,   1,    1,     9),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg = MMSS_MCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 33330000, LOW, 66670000,
							NOMINAL, 68570000),
		CLK_INIT(mclk0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mclk1_clk_src[] = {
	F_MM(   4800000,         mmsscc_xo,   4,    0,     0),
	F_MM(   6000000,  mmsscc_gpll0_div,  10,    1,     5),
	F_MM(   8000000,  mmsscc_gpll0_div,   1,    2,    75),
	F_MM(   9600000,         mmsscc_xo,   2,    0,     0),
	F_MM(  16666667,  mmsscc_gpll0_div,   2,    1,     9),
	F_MM(  19200000,         mmsscc_xo,   1,    0,     0),
	F_MM(  24000000,  mmsscc_gpll0_div,   1,    2,    25),
	F_MM(  33333333,  mmsscc_gpll0_div,   1,    1,     9),
	F_MM(  48000000,      mmsscc_gpll0,   1,    2,    25),
	F_MM(  66666667,      mmsscc_gpll0,   1,    1,     9),
	F_END
};

static struct rcg_clk mclk1_clk_src = {
	.cmd_rcgr_reg = MMSS_MCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 33330000, LOW, 66670000,
							NOMINAL, 68570000),
		CLK_INIT(mclk1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mclk2_clk_src[] = {
	F_MM(   4800000,         mmsscc_xo,   4,    0,     0),
	F_MM(   6000000,  mmsscc_gpll0_div,  10,    1,     5),
	F_MM(   8000000,  mmsscc_gpll0_div,   1,    2,    75),
	F_MM(   9600000,         mmsscc_xo,   2,    0,     0),
	F_MM(  16666667,  mmsscc_gpll0_div,   2,    1,     9),
	F_MM(  19200000,         mmsscc_xo,   1,    0,     0),
	F_MM(  24000000,  mmsscc_gpll0_div,   1,    2,    25),
	F_MM(  33333333,  mmsscc_gpll0_div,   1,    1,     9),
	F_MM(  48000000,      mmsscc_gpll0,   1,    2,    25),
	F_MM(  66666667,      mmsscc_gpll0,   1,    1,     9),
	F_END
};

static struct rcg_clk mclk2_clk_src = {
	.cmd_rcgr_reg = MMSS_MCLK2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 33330000, LOW, 66670000,
							NOMINAL, 68570000),
		CLK_INIT(mclk2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mclk3_clk_src[] = {
	F_MM(   4800000,         mmsscc_xo,   4,    0,     0),
	F_MM(   6000000,  mmsscc_gpll0_div,  10,    1,     5),
	F_MM(   8000000,  mmsscc_gpll0_div,   1,    2,    75),
	F_MM(   9600000,         mmsscc_xo,   2,    0,     0),
	F_MM(  16666667,  mmsscc_gpll0_div,   2,    1,     9),
	F_MM(  19200000,         mmsscc_xo,   1,    0,     0),
	F_MM(  24000000,  mmsscc_gpll0_div,   1,    2,    25),
	F_MM(  33333333,  mmsscc_gpll0_div,   1,    1,     9),
	F_MM(  48000000,      mmsscc_gpll0,   1,    2,    25),
	F_MM(  66666667,      mmsscc_gpll0,   1,    1,     9),
	F_END
};

static struct rcg_clk mclk3_clk_src = {
	.cmd_rcgr_reg = MMSS_MCLK3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk3_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 33330000, LOW, 66670000,
							NOMINAL, 68570000),
		CLK_INIT(mclk3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi0phytimer_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,  mmpll0_out_main,    4,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi0phytimer_clk_src_v3[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk csi0phytimer_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI0PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi0phytimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi0phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 266670000),
		CLK_INIT(csi0phytimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi1phytimer_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,  mmpll0_out_main,    4,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi1phytimer_clk_src_v3[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk csi1phytimer_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI1PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi1phytimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi1phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 266670000),
		CLK_INIT(csi1phytimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi2phytimer_clk_src[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,  mmpll0_out_main,    4,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi2phytimer_clk_src_v3[] = {
	F_MM( 100000000, mmsscc_gpll0_div,    3,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 266666667,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk csi2phytimer_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI2PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi2phytimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi2phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 266670000),
		CLK_INIT(csi2phytimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_rbbmtimer_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk rbbmtimer_clk_src = {
	.cmd_rcgr_reg = MMSS_RBBMTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_rbbmtimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base_gpu,
	.c = {
		.dbg_name = "rbbmtimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(rbbmtimer_clk_src.c),
	},
};

DEFINE_EXT_CLK(ext_byte0_clk_src, NULL);
DEFINE_EXT_CLK(ext_byte1_clk_src, NULL);
static struct clk_freq_tbl ftbl_byte0_clk_src[] = {
	{
		.div_src_val = BVAL(10, 8, mmsscc_xo_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &mmsscc_xo.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi0phypll_mm_source_val),
		.src_clk = &ext_byte0_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi1phypll_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &ext_byte1_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct rcg_clk byte0_clk_src = {
	.cmd_rcgr_reg = MMSS_BYTE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.current_freq = ftbl_byte0_clk_src,
	.freq_tbl = ftbl_byte0_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "byte0_clk_src",
		.parent = &ext_byte0_clk_src.c,
		.ops = &clk_ops_byte_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 131250000, LOW, 210000000,
							NOMINAL, 262500000),
		CLK_INIT(byte0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_byte1_clk_src[] = {
	{
		.div_src_val = BVAL(10, 8, mmsscc_xo_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &mmsscc_xo.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi0phypll_mm_source_val),
		.src_clk = &ext_byte0_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi1phypll_mm_source_val)
				| BVAL(4, 0, 0),
		.src_clk = &ext_byte1_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct rcg_clk byte1_clk_src = {
	.cmd_rcgr_reg = MMSS_BYTE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.current_freq = ftbl_byte1_clk_src,
	.freq_tbl = ftbl_byte1_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "byte1_clk_src",
		.parent = &ext_byte1_clk_src.c,
		.ops = &clk_ops_byte_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 131250000, LOW, 210000000,
							NOMINAL, 262500000),
		CLK_INIT(byte1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_esc0_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk esc0_clk_src = {
	.cmd_rcgr_reg = MMSS_ESC0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_esc0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "esc0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(esc0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_esc1_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk esc1_clk_src = {
	.cmd_rcgr_reg = MMSS_ESC1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_esc1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "esc1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(esc1_clk_src.c),
	},
};

DEFINE_EXT_CLK(ext_extpclk_clk_src, NULL);
static struct clk_freq_tbl ftbl_extpclk_clk_src[] = {
	{
		.div_src_val = BVAL(10, 8, ext_extpclk_clk_src_mm_source_val),
		.src_clk = &ext_extpclk_clk_src.c,
	},
	F_END
};

static struct rcg_clk extpclk_clk_src = {
	.cmd_rcgr_reg = MMSS_EXTPCLK_CMD_RCGR,
	.current_freq = ftbl_extpclk_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "extpclk_clk_src",
		.parent = &ext_extpclk_clk_src.c,
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP3(LOWER, 150000000, LOW, 300000000,
							NOMINAL, 600000000),
		CLK_INIT(extpclk_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_hdmi_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk hdmi_clk_src = {
	.cmd_rcgr_reg = MMSS_HDMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_hdmi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "hdmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(hdmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vsync_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk vsync_clk_src = {
	.cmd_rcgr_reg = MMSS_VSYNC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vsync_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "vsync_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(vsync_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_rbcpr_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_rbcpr_clk_src_v2[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_MM(  50000000,   mmsscc_gpll0,   12,    0,     0),
	F_END
};

static struct rcg_clk rbcpr_clk_src = {
	.cmd_rcgr_reg = MMSS_RBCPR_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_rbcpr_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "rbcpr_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(rbcpr_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_video_subcore0_clk_src[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 450000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_video_subcore0_clk_src_v2[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 490000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_video_subcore0_clk_src_v3[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 346666667,  mmpll3_out_main,    3,    0,     0),
	F_MM( 520000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk video_subcore0_clk_src = {
	.cmd_rcgr_reg = MMSS_VIDEO_SUBCORE0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_video_subcore0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.non_local_control_timeout = 1000,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_subcore0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 150000000,
					NOMINAL, 320000000, HIGH, 450000000),
		CLK_INIT(video_subcore0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_video_subcore1_clk_src[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 450000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_video_subcore1_clk_src_v2[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 490000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_video_subcore1_clk_src_v3[] = {
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 346666667,  mmpll3_out_main,    3,    0,     0),
	F_MM( 520000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk video_subcore1_clk_src = {
	.cmd_rcgr_reg = MMSS_VIDEO_SUBCORE1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_video_subcore1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.non_local_control_timeout = 1000,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_subcore1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 150000000,
					NOMINAL, 320000000, HIGH, 450000000),
		CLK_INIT(video_subcore1_clk_src.c),
	},
};

static struct branch_clk mmss_mmagic_ahb_clk = {
	.cbcr_reg = MMSS_MMSS_MMAGIC_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "mmss_mmagic_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mmagic_ahb_clk.c),
	},
};

static struct branch_clk camss_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(camss_ahb_clk.c),
	},
};

static struct branch_clk camss_cci_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CCI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_cci_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cci_ahb_clk.c),
	},
};

static struct branch_clk camss_cci_clk = {
	.cbcr_reg = MMSS_CAMSS_CCI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_cci_clk",
		.parent = &cci_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cci_clk.c),
	},
};

static struct branch_clk camss_cpp_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CPP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_cpp_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cpp_ahb_clk.c),
	},
};

static struct branch_clk camss_cpp_clk = {
	.cbcr_reg = MMSS_CAMSS_CPP_CBCR,
	.bcr_reg = MMSS_CAMSS_CPP_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_cpp_clk",
		.parent = &cpp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cpp_clk.c),
	},
};

static struct branch_clk camss_cpp_axi_clk = {
	.cbcr_reg = MMSS_CAMSS_CPP_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_cpp_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cpp_axi_clk.c),
	},
};

static struct branch_clk camss_cpp_vbif_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CPP_VBIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_cpp_vbif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cpp_vbif_ahb_clk.c),
	},
};

static struct branch_clk camss_csi0_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0_ahb_clk.c),
	},
};

static struct branch_clk camss_csi0_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi0_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0_clk.c),
	},
};

static struct branch_clk camss_csi0phy_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi0phy_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0phy_clk.c),
	},
};

static struct branch_clk camss_csi0pix_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi0pix_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0pix_clk.c),
	},
};

static struct branch_clk camss_csi0rdi_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi0rdi_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0rdi_clk.c),
	},
};

static struct branch_clk camss_csi1_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1_ahb_clk.c),
	},
};

static struct branch_clk camss_csi1_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi1_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1_clk.c),
	},
};

static struct branch_clk camss_csi1phy_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi1phy_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1phy_clk.c),
	},
};

static struct branch_clk camss_csi1pix_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi1pix_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1pix_clk.c),
	},
};

static struct branch_clk camss_csi1rdi_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi1rdi_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1rdi_clk.c),
	},
};

static struct branch_clk camss_csi2_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2_ahb_clk.c),
	},
};

static struct branch_clk camss_csi2_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi2_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2_clk.c),
	},
};

static struct branch_clk camss_csi2phy_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi2phy_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2phy_clk.c),
	},
};

static struct branch_clk camss_csi2pix_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi2pix_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2pix_clk.c),
	},
};

static struct branch_clk camss_csi2rdi_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi2rdi_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2rdi_clk.c),
	},
};

static struct branch_clk camss_csi3_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3_ahb_clk.c),
	},
};

static struct branch_clk camss_csi3_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI3_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi3_clk",
		.parent = &csi3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3_clk.c),
	},
};

static struct branch_clk camss_csi3phy_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI3PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi3phy_clk",
		.parent = &csi3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3phy_clk.c),
	},
};

static struct branch_clk camss_csi3pix_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI3PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi3pix_clk",
		.parent = &csi3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3pix_clk.c),
	},
};

static struct branch_clk camss_csi3rdi_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI3RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi3rdi_clk",
		.parent = &csi3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3rdi_clk.c),
	},
};

static struct branch_clk camss_csi_vfe0_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI_VFE0_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi_vfe0_clk",
		.parent = &vfe0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi_vfe0_clk.c),
	},
};

static struct branch_clk camss_csi_vfe1_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI_VFE1_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi_vfe1_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi_vfe1_clk.c),
	},
};

static struct branch_clk camss_csiphy0_3p_clk = {
	.cbcr_reg = MMSS_CAMSS_CSIPHY0_3P_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csiphy0_3p_clk",
		.parent = &csiphy0_3p_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csiphy0_3p_clk.c),
	},
};

static struct branch_clk camss_csiphy1_3p_clk = {
	.cbcr_reg = MMSS_CAMSS_CSIPHY1_3P_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csiphy1_3p_clk",
		.parent = &csiphy1_3p_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csiphy1_3p_clk.c),
	},
};

static struct branch_clk camss_csiphy2_3p_clk = {
	.cbcr_reg = MMSS_CAMSS_CSIPHY2_3P_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csiphy2_3p_clk",
		.parent = &csiphy2_3p_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csiphy2_3p_clk.c),
	},
};

static struct branch_clk camss_gp0_clk = {
	.cbcr_reg = MMSS_CAMSS_GP0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_gp0_clk",
		.parent = &camss_gp0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_gp0_clk.c),
	},
};

static struct branch_clk camss_gp1_clk = {
	.cbcr_reg = MMSS_CAMSS_GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_gp1_clk",
		.parent = &camss_gp1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_gp1_clk.c),
	},
};

static struct branch_clk camss_ispif_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_ISPIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_ispif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_ispif_ahb_clk.c),
	},
};

static struct branch_clk camss_jpeg0_clk = {
	.cbcr_reg = MMSS_CAMSS_JPEG0_CBCR,
	.bcr_reg = MMSS_CAMSS_JPEG_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_jpeg0_clk",
		.parent = &jpeg0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg0_clk.c),
	},
};

static struct branch_clk camss_jpeg2_clk = {
	.cbcr_reg = MMSS_CAMSS_JPEG2_CBCR,
	.bcr_reg = MMSS_CAMSS_JPEG_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_jpeg2_clk",
		.parent = &jpeg2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg2_clk.c),
	},
};

static struct branch_clk camss_jpeg_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_JPEG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_jpeg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_ahb_clk.c),
	},
};

static struct branch_clk camss_jpeg_axi_clk = {
	.cbcr_reg = MMSS_CAMSS_JPEG_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_jpeg_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_axi_clk.c),
	},
};

static struct branch_clk camss_jpeg_dma_clk = {
	.cbcr_reg = MMSS_CAMSS_JPEG_DMA_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_jpeg_dma_clk",
		.parent = &jpeg_dma_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_dma_clk.c),
	},
};

static struct branch_clk camss_mclk0_clk = {
	.cbcr_reg = MMSS_CAMSS_MCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_mclk0_clk",
		.parent = &mclk0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk0_clk.c),
	},
};

static struct branch_clk camss_mclk1_clk = {
	.cbcr_reg = MMSS_CAMSS_MCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_mclk1_clk",
		.parent = &mclk1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk1_clk.c),
	},
};

static struct branch_clk camss_mclk2_clk = {
	.cbcr_reg = MMSS_CAMSS_MCLK2_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_mclk2_clk",
		.parent = &mclk2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk2_clk.c),
	},
};

static struct branch_clk camss_mclk3_clk = {
	.cbcr_reg = MMSS_CAMSS_MCLK3_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_mclk3_clk",
		.parent = &mclk3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk3_clk.c),
	},
};

static struct branch_clk camss_micro_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_MICRO_AHB_CBCR,
	.bcr_reg = MMSS_CAMSS_MICRO_BCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_micro_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_micro_ahb_clk.c),
	},
};

static struct branch_clk camss_csi0phytimer_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi0phytimer_clk",
		.parent = &csi0phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0phytimer_clk.c),
	},
};

static struct branch_clk camss_csi1phytimer_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi1phytimer_clk",
		.parent = &csi1phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1phytimer_clk.c),
	},
};

static struct branch_clk camss_csi2phytimer_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi2phytimer_clk",
		.parent = &csi2phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2phytimer_clk.c),
	},
};

static struct branch_clk camss_top_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_TOP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_top_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_top_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_axi_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_axi_clk.c),
	},
};

static struct branch_clk camss_vfe0_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe0_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe0_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE0_CBCR,
	.bcr_reg = MMSS_CAMSS_VFE0_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe0_clk",
		.parent = &vfe0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe0_clk.c),
	},
};

static struct branch_clk camss_vfe0_stream_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE0_STREAM_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe0_stream_clk",
		.parent = &vfe0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe0_stream_clk.c),
	},
};

static struct branch_clk camss_vfe1_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe1_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe1_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE1_CBCR,
	.bcr_reg = MMSS_CAMSS_VFE1_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe1_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe1_clk.c),
	},
};

static struct branch_clk camss_vfe1_stream_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE1_STREAM_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe1_stream_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe1_stream_clk.c),
	},
};

static struct branch_clk fd_ahb_clk = {
	.cbcr_reg = MMSS_FD_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "fd_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(fd_ahb_clk.c),
	},
};

static struct branch_clk fd_core_clk = {
	.cbcr_reg = MMSS_FD_CORE_CBCR,
	.bcr_reg = MMSS_FD_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "fd_core_clk",
		.parent = &fd_core_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(fd_core_clk.c),
	},
};

static struct branch_clk fd_core_uar_clk = {
	.cbcr_reg = MMSS_FD_CORE_UAR_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "fd_core_uar_clk",
		.parent = &fd_core_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(fd_core_uar_clk.c),
	},
};

static struct branch_clk gpu_ahb_clk = {
	.cbcr_reg = MMSS_GPU_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base_gpu,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "gpu_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(gpu_ahb_clk.c),
	},
};

static struct branch_clk gpu_aon_isense_clk = {
	.cbcr_reg = MMSS_GPU_AON_ISENSE_CBCR,
	.has_sibling = 1,
	.base = &virt_base_gpu,
	.c = {
		.dbg_name = "gpu_aon_isense_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gpu_aon_isense_clk.c),
	},
};

static struct branch_clk gpu_gx_gfx3d_clk = {
	.cbcr_reg = MMSS_GPU_GX_GFX3D_CBCR,
	.bcr_reg = MMSS_GPU_GX_BCR,
	.has_sibling = 0,
	.base = &virt_base_gpu,
	.c = {
		.dbg_name = "gpu_gx_gfx3d_clk",
		.parent = &gfx3d_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gpu_gx_gfx3d_clk.c),
	},
};

static struct fixed_clk gpu_mx_clk = {
	.c = {
		.dbg_name = "gpu_mx_clk",
		.vdd_class = &vdd_gpu_mx,
		.ops = &clk_ops_dummy,
		CLK_INIT(gpu_mx_clk.c),
	},
};

static struct branch_clk gpu_gx_rbbmtimer_clk = {
	.cbcr_reg = MMSS_GPU_GX_RBBMTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base_gpu,
	.c = {
		.dbg_name = "gpu_gx_rbbmtimer_clk",
		.parent = &rbbmtimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gpu_gx_rbbmtimer_clk.c),
	},
};

static struct branch_clk mdss_ahb_clk = {
	.cbcr_reg = MMSS_MDSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(mdss_ahb_clk.c),
	},
};

static struct branch_clk mdss_axi_clk = {
	.cbcr_reg = MMSS_MDSS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_axi_clk.c),
	},
};

static struct branch_clk mdss_byte0_clk = {
	.cbcr_reg = MMSS_MDSS_BYTE0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_byte0_clk",
		.parent = &byte0_clk_src.c,
		.ops = &clk_ops_branch,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mdss_byte0_clk.c),
	},
};

static struct branch_clk mdss_byte1_clk = {
	.cbcr_reg = MMSS_MDSS_BYTE1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_byte1_clk",
		.parent = &byte1_clk_src.c,
		.ops = &clk_ops_branch,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mdss_byte1_clk.c),
	},
};

static struct branch_clk mdss_esc0_clk = {
	.cbcr_reg = MMSS_MDSS_ESC0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_esc0_clk",
		.parent = &esc0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_esc0_clk.c),
	},
};

static struct branch_clk mdss_esc1_clk = {
	.cbcr_reg = MMSS_MDSS_ESC1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_esc1_clk",
		.parent = &esc1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_esc1_clk.c),
	},
};

static struct branch_clk mdss_extpclk_clk = {
	.cbcr_reg = MMSS_MDSS_EXTPCLK_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_extpclk_clk",
		.parent = &extpclk_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_extpclk_clk.c),
	},
};

static struct branch_clk mdss_hdmi_ahb_clk = {
	.cbcr_reg = MMSS_MDSS_HDMI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_hdmi_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_hdmi_ahb_clk.c),
	},
};

static struct branch_clk mdss_hdmi_clk = {
	.cbcr_reg = MMSS_MDSS_HDMI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_hdmi_clk",
		.parent = &hdmi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_hdmi_clk.c),
	},
};

static struct branch_clk mdss_mdp_clk = {
	.cbcr_reg = MMSS_MDSS_MDP_CBCR,
	.bcr_reg = MMSS_MDSS_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_mdp_clk",
		.parent = &mdp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_clk.c),
	},
};

static DEFINE_CLK_VOTER(mdss_mdp_vote_clk, &mdss_mdp_clk.c, 0);
static DEFINE_CLK_VOTER(mdss_rotator_vote_clk, &mdss_mdp_clk.c, 0);

static struct branch_clk mdss_pclk0_clk = {
	.cbcr_reg = MMSS_MDSS_PCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_pclk0_clk",
		.parent = &pclk0_clk_src.c,
		.ops = &clk_ops_branch,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mdss_pclk0_clk.c),
	},
};

static struct branch_clk mdss_pclk1_clk = {
	.cbcr_reg = MMSS_MDSS_PCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_pclk1_clk",
		.parent = &pclk1_clk_src.c,
		.ops = &clk_ops_branch,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mdss_pclk1_clk.c),
	},
};

static struct branch_clk mdss_vsync_clk = {
	.cbcr_reg = MMSS_MDSS_VSYNC_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_vsync_clk",
		.parent = &vsync_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_vsync_clk.c),
	},
};

static struct branch_clk mmss_misc_ahb_clk = {
	.cbcr_reg = MMSS_MMSS_MISC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_misc_ahb_clk",
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(mmss_misc_ahb_clk.c),
	},
};

static struct branch_clk mmss_misc_cxo_clk = {
	.cbcr_reg = MMSS_MMSS_MISC_CXO_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_misc_cxo_clk",
		.parent = &mmsscc_xo.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_misc_cxo_clk.c),
	},
};

static struct branch_clk mmagic_bimc_noc_cfg_ahb_clk = {
	.cbcr_reg = MMSS_MMAGIC_BIMC_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmagic_bimc_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmagic_bimc_noc_cfg_ahb_clk.c),
	},
};

static struct branch_clk mmagic_camss_axi_clk = {
	.cbcr_reg = MMSS_MMAGIC_CAMSS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmagic_camss_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmagic_camss_axi_clk.c),
	},
};

static struct branch_clk mmagic_camss_noc_cfg_ahb_clk = {
	.cbcr_reg = MMSS_MMAGIC_CAMSS_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmagic_camss_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmagic_camss_noc_cfg_ahb_clk.c),
	},
};

static struct branch_clk mmss_mmagic_cfg_ahb_clk = {
	.cbcr_reg = MMSS_MMSS_MMAGIC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "mmss_mmagic_cfg_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(mmss_mmagic_cfg_ahb_clk.c),
	},
};

static struct branch_clk mmagic_mdss_axi_clk = {
	.cbcr_reg = MMSS_MMAGIC_MDSS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmagic_mdss_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmagic_mdss_axi_clk.c),
	},
};

static struct branch_clk mmagic_mdss_noc_cfg_ahb_clk = {
	.cbcr_reg = MMSS_MMAGIC_MDSS_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmagic_mdss_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmagic_mdss_noc_cfg_ahb_clk.c),
	},
};

static struct branch_clk mmagic_video_axi_clk = {
	.cbcr_reg = MMSS_MMAGIC_VIDEO_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmagic_video_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmagic_video_axi_clk.c),
	},
};

static struct branch_clk mmagic_video_noc_cfg_ahb_clk = {
	.cbcr_reg = MMSS_MMAGIC_VIDEO_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmagic_video_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmagic_video_noc_cfg_ahb_clk.c),
	},
};

static struct branch_clk mmss_mmagic_maxi_clk = {
	.cbcr_reg = MMSS_MMSS_MMAGIC_MAXI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mmagic_maxi_clk",
		.parent = &maxi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mmagic_maxi_clk.c),
	},
};

static struct branch_clk mmss_rbcpr_ahb_clk = {
	.cbcr_reg = MMSS_MMSS_RBCPR_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_rbcpr_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(mmss_rbcpr_ahb_clk.c),
	},
};

static struct branch_clk mmss_rbcpr_clk = {
	.cbcr_reg = MMSS_MMSS_RBCPR_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_rbcpr_clk",
		.parent = &rbcpr_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_rbcpr_clk.c),
	},
};

static struct branch_clk smmu_cpp_ahb_clk = {
	.cbcr_reg = MMSS_SMMU_CPP_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_cpp_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(smmu_cpp_ahb_clk.c),
	},
};

static struct branch_clk smmu_cpp_axi_clk = {
	.cbcr_reg = MMSS_SMMU_CPP_AXI_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_cpp_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(smmu_cpp_axi_clk.c),
	},
};

static struct branch_clk smmu_jpeg_ahb_clk = {
	.cbcr_reg = MMSS_SMMU_JPEG_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_jpeg_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(smmu_jpeg_ahb_clk.c),
	},
};

static struct branch_clk smmu_jpeg_axi_clk = {
	.cbcr_reg = MMSS_SMMU_JPEG_AXI_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_jpeg_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(smmu_jpeg_axi_clk.c),
	},
};

static struct branch_clk smmu_mdp_ahb_clk = {
	.cbcr_reg = MMSS_SMMU_MDP_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_mdp_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(smmu_mdp_ahb_clk.c),
	},
};

static struct branch_clk smmu_mdp_axi_clk = {
	.cbcr_reg = MMSS_SMMU_MDP_AXI_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_mdp_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(smmu_mdp_axi_clk.c),
	},
};

static struct branch_clk smmu_rot_ahb_clk = {
	.cbcr_reg = MMSS_SMMU_ROT_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_rot_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(smmu_rot_ahb_clk.c),
	},
};

static struct branch_clk smmu_rot_axi_clk = {
	.cbcr_reg = MMSS_SMMU_ROT_AXI_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_rot_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(smmu_rot_axi_clk.c),
	},
};

static struct branch_clk smmu_vfe_ahb_clk = {
	.cbcr_reg = MMSS_SMMU_VFE_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_vfe_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(smmu_vfe_ahb_clk.c),
	},
};

static struct branch_clk smmu_vfe_axi_clk = {
	.cbcr_reg = MMSS_SMMU_VFE_AXI_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_vfe_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(smmu_vfe_axi_clk.c),
	},
};

static struct branch_clk smmu_video_ahb_clk = {
	.cbcr_reg = MMSS_SMMU_VIDEO_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_video_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(smmu_video_ahb_clk.c),
	},
};

static struct branch_clk smmu_video_axi_clk = {
	.cbcr_reg = MMSS_SMMU_VIDEO_AXI_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.base = &virt_base,
	.no_halt_check_on_disable = true,
	.c = {
		.dbg_name = "smmu_video_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(smmu_video_axi_clk.c),
	},
};

static struct branch_clk video_ahb_clk = {
	.cbcr_reg = MMSS_VIDEO_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(video_ahb_clk.c),
	},
};

static struct branch_clk video_axi_clk = {
	.cbcr_reg = MMSS_VIDEO_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(video_axi_clk.c),
	},
};

static struct branch_clk video_core_clk = {
	.cbcr_reg = MMSS_VIDEO_CORE_CBCR,
	.bcr_reg = MMSS_VIDEO_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_core_clk",
		.parent = &video_core_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(video_core_clk.c),
	},
};

static struct branch_clk video_maxi_clk = {
	.cbcr_reg = MMSS_VIDEO_MAXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_maxi_clk",
		.parent = &maxi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(video_maxi_clk.c),
	},
};

static struct branch_clk video_subcore0_clk = {
	.cbcr_reg = MMSS_VIDEO_SUBCORE0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_subcore0_clk",
		.parent = &video_subcore0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(video_subcore0_clk.c),
	},
};

static struct branch_clk video_subcore1_clk = {
	.cbcr_reg = MMSS_VIDEO_SUBCORE1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_subcore1_clk",
		.parent = &video_subcore1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(video_subcore1_clk.c),
	},
};

static struct branch_clk vmem_ahb_clk = {
	.cbcr_reg = MMSS_VMEM_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "vmem_ahb_clk",
		.parent = &ahb_clk_src.c,
		.ops = &clk_ops_branch,
		.depends = &mmss_mmagic_ahb_clk.c,
		CLK_INIT(vmem_ahb_clk.c),
	},
};

static struct branch_clk vmem_maxi_clk = {
	.cbcr_reg = MMSS_VMEM_MAXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "vmem_maxi_clk",
		.parent = &maxi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vmem_maxi_clk.c),
	},
};

static struct mux_clk mmss_gcc_dbg_clk = {
	.ops = &mux_reg_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.offset = MMSS_MMSS_DEBUG_CLK_CTL,
	.en_offset = MMSS_MMSS_DEBUG_CLK_CTL,
	.base = &virt_base,
	MUX_SRC_LIST(
		{ &mmss_mmagic_ahb_clk.c, 0x0001 },
		{ &mmss_misc_ahb_clk.c, 0x0003 },
		{ &vmem_maxi_clk.c, 0x0009 },
		{ &vmem_ahb_clk.c, 0x000a },
		{ &video_core_clk.c, 0x000e },
		{ &video_axi_clk.c, 0x000f },
		{ &video_maxi_clk.c, 0x0010 },
		{ &video_ahb_clk.c, 0x0011 },
		{ &mmss_rbcpr_clk.c, 0x0012 },
		{ &mmss_rbcpr_ahb_clk.c, 0x0013 },
		{ &mdss_mdp_clk.c, 0x0014 },
		{ &mdss_pclk0_clk.c, 0x0016 },
		{ &mdss_pclk1_clk.c, 0x0017 },
		{ &mdss_extpclk_clk.c, 0x0018 },
		{ &video_subcore0_clk.c, 0x001a },
		{ &video_subcore1_clk.c, 0x001b },
		{ &mdss_vsync_clk.c, 0x001c },
		{ &mdss_hdmi_clk.c, 0x001d },
		{ &mdss_byte0_clk.c, 0x001e },
		{ &mdss_byte1_clk.c, 0x001f },
		{ &mdss_esc0_clk.c, 0x0020 },
		{ &mdss_esc1_clk.c, 0x0021 },
		{ &mdss_ahb_clk.c, 0x0022 },
		{ &mdss_hdmi_ahb_clk.c, 0x0023 },
		{ &mdss_axi_clk.c, 0x0024 },
		{ &camss_top_ahb_clk.c, 0x0025 },
		{ &camss_micro_ahb_clk.c, 0x0026 },
		{ &camss_gp0_clk.c, 0x0027 },
		{ &camss_gp1_clk.c, 0x0028 },
		{ &camss_mclk0_clk.c, 0x0029 },
		{ &camss_mclk1_clk.c, 0x002a },
		{ &camss_mclk2_clk.c, 0x002b },
		{ &camss_mclk3_clk.c, 0x002c },
		{ &camss_cci_clk.c, 0x002d },
		{ &camss_cci_ahb_clk.c, 0x002e },
		{ &camss_csi0phytimer_clk.c, 0x002f },
		{ &camss_csi1phytimer_clk.c, 0x0030 },
		{ &camss_csi2phytimer_clk.c, 0x0031 },
		{ &camss_jpeg0_clk.c, 0x0032 },
		{ &camss_ispif_ahb_clk.c, 0x0033 },
		{ &camss_jpeg2_clk.c, 0x0034 },
		{ &camss_jpeg_ahb_clk.c, 0x0035 },
		{ &camss_jpeg_axi_clk.c, 0x0036 },
		{ &camss_ahb_clk.c, 0x0037 },
		{ &camss_vfe0_clk.c, 0x0038 },
		{ &camss_vfe1_clk.c, 0x0039 },
		{ &camss_cpp_clk.c, 0x003a },
		{ &camss_cpp_ahb_clk.c, 0x003b },
		{ &camss_vfe_ahb_clk.c, 0x003c },
		{ &camss_vfe_axi_clk.c, 0x003d },
		{ &camss_csi_vfe0_clk.c, 0x003f },
		{ &camss_csi_vfe1_clk.c, 0x0040 },
		{ &camss_csi0_clk.c, 0x0041 },
		{ &camss_csi0_ahb_clk.c, 0x0042 },
		{ &camss_csi0phy_clk.c, 0x0043 },
		{ &camss_csi0rdi_clk.c, 0x0044 },
		{ &camss_csi0pix_clk.c, 0x0045 },
		{ &camss_csi1_clk.c, 0x0046 },
		{ &camss_csi1_ahb_clk.c, 0x0047 },
		{ &camss_csi1phy_clk.c, 0x0048 },
		{ &camss_csi1rdi_clk.c, 0x0049 },
		{ &camss_csi1pix_clk.c, 0x004a },
		{ &camss_csi2_clk.c, 0x004b },
		{ &camss_csi2_ahb_clk.c, 0x004c },
		{ &camss_csi2phy_clk.c, 0x004d },
		{ &camss_csi2rdi_clk.c, 0x004e },
		{ &camss_csi2pix_clk.c, 0x004f },
		{ &camss_csi3_clk.c, 0x0050 },
		{ &camss_csi3_ahb_clk.c, 0x0051 },
		{ &camss_csi3phy_clk.c, 0x0052 },
		{ &camss_csi3rdi_clk.c, 0x0053 },
		{ &camss_csi3pix_clk.c, 0x0054 },
		{ &mmss_mmagic_maxi_clk.c, 0x0070 },
		{ &camss_vfe0_stream_clk.c, 0x0071 },
		{ &camss_vfe1_stream_clk.c, 0x0072 },
		{ &camss_cpp_vbif_ahb_clk.c, 0x0073 },
		{ &mmss_mmagic_cfg_ahb_clk.c, 0x0074 },
		{ &mmss_misc_cxo_clk.c, 0x0077 },
		{ &camss_cpp_axi_clk.c, 0x007a },
		{ &camss_jpeg_dma_clk.c, 0x007b },
		{ &camss_vfe0_ahb_clk.c, 0x0086 },
		{ &camss_vfe1_ahb_clk.c, 0x0087 },
		{ &fd_core_clk.c, 0x0089 },
		{ &fd_core_uar_clk.c, 0x008a },
		{ &fd_ahb_clk.c, 0x008c },
		{ &camss_csiphy0_3p_clk.c, 0x0091 },
		{ &camss_csiphy1_3p_clk.c, 0x0092 },
		{ &camss_csiphy2_3p_clk.c, 0x0093 },
		{ &smmu_vfe_ahb_clk.c, 0x0094 },
		{ &smmu_vfe_axi_clk.c, 0x0095 },
		{ &smmu_cpp_ahb_clk.c, 0x0096 },
		{ &smmu_cpp_axi_clk.c, 0x0097 },
		{ &smmu_jpeg_ahb_clk.c, 0x0098 },
		{ &smmu_jpeg_axi_clk.c, 0x0099 },
		{ &mmagic_camss_axi_clk.c, 0x009a },
		{ &smmu_rot_ahb_clk.c, 0x009b },
		{ &smmu_rot_axi_clk.c, 0x009c },
		{ &smmu_mdp_ahb_clk.c, 0x009d },
		{ &smmu_mdp_axi_clk.c, 0x009e },
		{ &mmagic_mdss_axi_clk.c, 0x009f },
		{ &smmu_video_ahb_clk.c, 0x00a0 },
		{ &smmu_video_axi_clk.c, 0x00a1 },
		{ &mmagic_video_axi_clk.c, 0x00a2 },
		{ &mmagic_camss_noc_cfg_ahb_clk.c, 0x00ad },
		{ &mmagic_mdss_noc_cfg_ahb_clk.c, 0x00ae },
		{ &mmagic_video_noc_cfg_ahb_clk.c, 0x00af },
		{ &mmagic_bimc_noc_cfg_ahb_clk.c, 0x00b0 },
	),
	.c = {
		.dbg_name = "mmss_gcc_dbg_clk",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mmss_gcc_dbg_clk.c),
	},
};

static struct clk_lookup msm_clocks_mmss_8996[] = {
	CLK_LIST(mmsscc_xo),
	CLK_LIST(mmsscc_gpll0),
	CLK_LIST(mmsscc_gpll0_div),
	CLK_LIST(mmpll0),
	CLK_LIST(mmpll0_out_main),
	CLK_LIST(mmpll1),
	CLK_LIST(mmpll1_out_main),
	CLK_LIST(mmpll4),
	CLK_LIST(mmpll4_out_main),
	CLK_LIST(mmpll3),
	CLK_LIST(mmpll3_out_main),
	CLK_LIST(ahb_clk_src),
	CLK_LIST(mmpll2),
	CLK_LIST(mmpll2_out_main),
	CLK_LIST(mmpll8),
	CLK_LIST(mmpll8_out_main),
	CLK_LIST(mmpll9),
	CLK_LIST(mmpll9_out_main),
	CLK_LIST(mmpll5),
	CLK_LIST(mmpll5_out_main),
	CLK_LIST(csi0_clk_src),
	CLK_LIST(vfe0_clk_src),
	CLK_LIST(vfe1_clk_src),
	CLK_LIST(csi1_clk_src),
	CLK_LIST(csi2_clk_src),
	CLK_LIST(csi3_clk_src),
	CLK_LIST(maxi_clk_src),
	CLK_LIST(cpp_clk_src),
	CLK_LIST(jpeg0_clk_src),
	CLK_LIST(jpeg2_clk_src),
	CLK_LIST(jpeg_dma_clk_src),
	CLK_LIST(mdp_clk_src),
	CLK_LIST(video_core_clk_src),
	CLK_LIST(fd_core_clk_src),
	CLK_LIST(cci_clk_src),
	CLK_LIST(csiphy0_3p_clk_src),
	CLK_LIST(csiphy1_3p_clk_src),
	CLK_LIST(csiphy2_3p_clk_src),
	CLK_LIST(camss_gp0_clk_src),
	CLK_LIST(camss_gp1_clk_src),
	CLK_LIST(mclk0_clk_src),
	CLK_LIST(mclk1_clk_src),
	CLK_LIST(mclk2_clk_src),
	CLK_LIST(mclk3_clk_src),
	CLK_LIST(csi0phytimer_clk_src),
	CLK_LIST(csi1phytimer_clk_src),
	CLK_LIST(csi2phytimer_clk_src),
	CLK_LIST(esc0_clk_src),
	CLK_LIST(esc1_clk_src),
	CLK_LIST(hdmi_clk_src),
	CLK_LIST(vsync_clk_src),
	CLK_LIST(rbcpr_clk_src),
	CLK_LIST(video_subcore0_clk_src),
	CLK_LIST(video_subcore1_clk_src),
	CLK_LIST(camss_ahb_clk),
	CLK_LIST(camss_cci_ahb_clk),
	CLK_LIST(camss_cci_clk),
	CLK_LIST(camss_cpp_ahb_clk),
	CLK_LIST(camss_cpp_clk),
	CLK_LIST(camss_cpp_axi_clk),
	CLK_LIST(camss_cpp_vbif_ahb_clk),
	CLK_LIST(camss_csi0_ahb_clk),
	CLK_LIST(camss_csi0_clk),
	CLK_LIST(camss_csi0phy_clk),
	CLK_LIST(camss_csi0pix_clk),
	CLK_LIST(camss_csi0rdi_clk),
	CLK_LIST(camss_csi1_ahb_clk),
	CLK_LIST(camss_csi1_clk),
	CLK_LIST(camss_csi1phy_clk),
	CLK_LIST(camss_csi1pix_clk),
	CLK_LIST(camss_csi1rdi_clk),
	CLK_LIST(camss_csi2_ahb_clk),
	CLK_LIST(camss_csi2_clk),
	CLK_LIST(camss_csi2phy_clk),
	CLK_LIST(camss_csi2pix_clk),
	CLK_LIST(camss_csi2rdi_clk),
	CLK_LIST(camss_csi3_ahb_clk),
	CLK_LIST(camss_csi3_clk),
	CLK_LIST(camss_csi3phy_clk),
	CLK_LIST(camss_csi3pix_clk),
	CLK_LIST(camss_csi3rdi_clk),
	CLK_LIST(camss_csi_vfe0_clk),
	CLK_LIST(camss_csi_vfe1_clk),
	CLK_LIST(camss_csiphy0_3p_clk),
	CLK_LIST(camss_csiphy1_3p_clk),
	CLK_LIST(camss_csiphy2_3p_clk),
	CLK_LIST(camss_gp0_clk),
	CLK_LIST(camss_gp1_clk),
	CLK_LIST(camss_ispif_ahb_clk),
	CLK_LIST(camss_jpeg0_clk),
	CLK_LIST(camss_jpeg2_clk),
	CLK_LIST(camss_jpeg_ahb_clk),
	CLK_LIST(camss_jpeg_axi_clk),
	CLK_LIST(camss_jpeg_dma_clk),
	CLK_LIST(camss_mclk0_clk),
	CLK_LIST(camss_mclk1_clk),
	CLK_LIST(camss_mclk2_clk),
	CLK_LIST(camss_mclk3_clk),
	CLK_LIST(camss_micro_ahb_clk),
	CLK_LIST(camss_csi0phytimer_clk),
	CLK_LIST(camss_csi1phytimer_clk),
	CLK_LIST(camss_csi2phytimer_clk),
	CLK_LIST(camss_top_ahb_clk),
	CLK_LIST(camss_vfe_ahb_clk),
	CLK_LIST(camss_vfe_axi_clk),
	CLK_LIST(camss_vfe0_ahb_clk),
	CLK_LIST(camss_vfe0_clk),
	CLK_LIST(camss_vfe0_stream_clk),
	CLK_LIST(camss_vfe1_ahb_clk),
	CLK_LIST(camss_vfe1_clk),
	CLK_LIST(camss_vfe1_stream_clk),
	CLK_LIST(fd_ahb_clk),
	CLK_LIST(fd_core_clk),
	CLK_LIST(fd_core_uar_clk),
	CLK_LIST(mdss_ahb_clk),
	CLK_LIST(mdss_axi_clk),
	CLK_LIST(mdss_byte0_clk),
	CLK_LIST(mdss_byte1_clk),
	CLK_LIST(byte0_clk_src),
	CLK_LIST(byte1_clk_src),
	CLK_LIST(ext_byte0_clk_src),
	CLK_LIST(ext_byte1_clk_src),
	CLK_LIST(mdss_esc0_clk),
	CLK_LIST(mdss_esc1_clk),
	CLK_LIST(mdss_extpclk_clk),
	CLK_LIST(mdss_hdmi_ahb_clk),
	CLK_LIST(mdss_hdmi_clk),
	CLK_LIST(mdss_mdp_clk),
	CLK_LIST(mdss_pclk0_clk),
	CLK_LIST(mdss_pclk1_clk),
	CLK_LIST(pclk0_clk_src),
	CLK_LIST(pclk1_clk_src),
	CLK_LIST(ext_pclk0_clk_src),
	CLK_LIST(ext_pclk1_clk_src),
	CLK_LIST(mdss_vsync_clk),
	CLK_LIST(mmss_misc_ahb_clk),
	CLK_LIST(mmss_misc_cxo_clk),
	CLK_LIST(mmagic_bimc_noc_cfg_ahb_clk),
	CLK_LIST(mmagic_camss_axi_clk),
	CLK_LIST(mmagic_camss_noc_cfg_ahb_clk),
	CLK_LIST(mmss_mmagic_cfg_ahb_clk),
	CLK_LIST(mmagic_mdss_axi_clk),
	CLK_LIST(mmagic_mdss_noc_cfg_ahb_clk),
	CLK_LIST(mmagic_video_axi_clk),
	CLK_LIST(mmagic_video_noc_cfg_ahb_clk),
	CLK_LIST(mmss_mmagic_ahb_clk),
	CLK_LIST(mmss_mmagic_maxi_clk),
	CLK_LIST(mmss_rbcpr_ahb_clk),
	CLK_LIST(mmss_rbcpr_clk),
	CLK_LIST(smmu_cpp_ahb_clk),
	CLK_LIST(smmu_cpp_axi_clk),
	CLK_LIST(smmu_jpeg_ahb_clk),
	CLK_LIST(smmu_jpeg_axi_clk),
	CLK_LIST(smmu_mdp_ahb_clk),
	CLK_LIST(smmu_mdp_axi_clk),
	CLK_LIST(smmu_rot_ahb_clk),
	CLK_LIST(smmu_rot_axi_clk),
	CLK_LIST(smmu_vfe_ahb_clk),
	CLK_LIST(smmu_vfe_axi_clk),
	CLK_LIST(smmu_video_ahb_clk),
	CLK_LIST(smmu_video_axi_clk),
	CLK_LIST(video_ahb_clk),
	CLK_LIST(video_axi_clk),
	CLK_LIST(video_core_clk),
	CLK_LIST(video_maxi_clk),
	CLK_LIST(video_subcore0_clk),
	CLK_LIST(video_subcore1_clk),
	CLK_LIST(vmem_ahb_clk),
	CLK_LIST(vmem_maxi_clk),
	CLK_LIST(mmss_gcc_dbg_clk),
	CLK_LIST(mdss_mdp_vote_clk),
	CLK_LIST(mdss_rotator_vote_clk),
};

static struct clk_lookup msm_clocks_mmsscc_8996_v2[] = {
	CLK_LIST(mmpll2_postdiv_clk),
	CLK_LIST(mmpll8_postdiv_clk),
	CLK_LIST(mmpll9_postdiv_clk),
};

static void msm_mmsscc_8996_v2_fixup(void)
{
	mmpll1.c.rate = 810000000;
	mmpll1.c.fmax[VDD_DIG_LOWER] = 405000000;
	mmpll1.c.fmax[VDD_DIG_LOW] = 405000000;
	mmpll1.c.fmax[VDD_DIG_NOMINAL] = 1300000000;
	mmpll1.c.fmax[VDD_DIG_HIGH] = 1300000000;

	mmpll2.vco_tbl = mmpll_gfx_vco;
	mmpll2.num_vco = ARRAY_SIZE(mmpll_gfx_vco),
	mmpll2.c.rate = 0;
	mmpll2.c.fmax[VDD_DIG_LOWER] = 1000000000;
	mmpll2.c.fmax[VDD_DIG_LOW] = 1000000000;
	mmpll2.c.fmax[VDD_DIG_NOMINAL] = 1000000000;
	mmpll2.c.fmax[VDD_DIG_HIGH] = 1000000000;
	mmpll2.no_prepared_reconfig = true;
	mmpll2.c.ops = &clk_ops_alpha_pll;

	mmpll3.c.rate = 980000000;
	mmpll3.c.fmax[VDD_DIG_LOWER] = 650000000;
	mmpll3.c.fmax[VDD_DIG_LOW] = 650000000;
	mmpll3.c.fmax[VDD_DIG_NOMINAL] = 1300000000;
	mmpll3.c.fmax[VDD_DIG_HIGH] = 1300000000;

	mmpll4.c.fmax[VDD_DIG_LOWER] = 650000000;
	mmpll4.c.fmax[VDD_DIG_LOW] = 650000000;
	mmpll4.c.fmax[VDD_DIG_NOMINAL] = 1300000000;
	mmpll4.c.fmax[VDD_DIG_HIGH] = 1300000000;

	mmpll5.c.rate = 825000000;
	mmpll5.c.fmax[VDD_DIG_LOWER] = 650000000;
	mmpll5.c.fmax[VDD_DIG_LOW] = 650000000;
	mmpll5.c.fmax[VDD_DIG_NOMINAL] = 1300000000;
	mmpll5.c.fmax[VDD_DIG_HIGH] = 1300000000;

	mmpll8.vco_tbl = mmpll_gfx_vco;
	mmpll8.num_vco = ARRAY_SIZE(mmpll_gfx_vco),
	mmpll8.c.rate = 0;
	mmpll8.c.fmax[VDD_DIG_LOWER] = 1000000000;
	mmpll8.c.fmax[VDD_DIG_LOW] = 1000000000;
	mmpll8.c.fmax[VDD_DIG_NOMINAL] = 1000000000;
	mmpll8.c.fmax[VDD_DIG_HIGH] = 1000000000;
	mmpll8.no_prepared_reconfig = true;
	mmpll8.c.ops = &clk_ops_alpha_pll;

	mmpll9.c.rate = 1209600000;
	mmpll9.c.fmax[VDD_DIG_LOWER] = 650000000;
	mmpll9.c.fmax[VDD_DIG_LOW] = 650000000;
	mmpll9.c.fmax[VDD_DIG_NOMINAL] = 1300000000;
	mmpll9.c.fmax[VDD_DIG_HIGH] = 1300000000;

	csi0_clk_src.freq_tbl = ftbl_csi0_clk_src_v2;
	csi1_clk_src.freq_tbl = ftbl_csi1_clk_src_v2;
	csi2_clk_src.freq_tbl = ftbl_csi2_clk_src_v2;
	csi3_clk_src.freq_tbl = ftbl_csi3_clk_src_v2;

	csiphy0_3p_clk_src.freq_tbl = ftbl_csiphy0_3p_clk_src_v2;
	csiphy0_3p_clk_src.c.fmax[VDD_DIG_HIGH] = 384000000;
	csiphy1_3p_clk_src.freq_tbl = ftbl_csiphy1_3p_clk_src_v2;
	csiphy1_3p_clk_src.c.fmax[VDD_DIG_HIGH] = 384000000;
	csiphy2_3p_clk_src.freq_tbl = ftbl_csiphy2_3p_clk_src_v2;
	csiphy2_3p_clk_src.c.fmax[VDD_DIG_HIGH] = 384000000;

	vfe0_clk_src.freq_tbl = ftbl_vfe0_clk_src_v2;
	vfe0_clk_src.c.fmax[VDD_DIG_LOWER] = 100000000;
	vfe1_clk_src.freq_tbl = ftbl_vfe1_clk_src_v2;

	mdp_clk_src.freq_tbl = ftbl_mdp_clk_src_v2;
	mdp_clk_src.c.fmax[VDD_DIG_LOW] = 275000000;
	mdp_clk_src.c.fmax[VDD_DIG_NOMINAL] = 330000000;
	mdp_clk_src.c.fmax[VDD_DIG_HIGH] = 412500000;

	maxi_clk_src.freq_tbl = ftbl_maxi_clk_src_v2;
	maxi_clk_src.c.fmax[VDD_DIG_HIGH] = 405000000;

	rbcpr_clk_src.freq_tbl = ftbl_rbcpr_clk_src_v2;
	rbcpr_clk_src.c.fmax[VDD_DIG_NOMINAL] = 50000000;
	rbcpr_clk_src.c.fmax[VDD_DIG_HIGH] = 50000000;

	video_core_clk_src.freq_tbl = ftbl_video_core_clk_src_v2;
	video_core_clk_src.c.fmax[VDD_DIG_HIGH] = 516000000;
	video_subcore0_clk_src.freq_tbl = ftbl_video_subcore0_clk_src_v2;
	video_subcore0_clk_src.c.fmax[VDD_DIG_HIGH] = 490000000;
	video_subcore1_clk_src.freq_tbl = ftbl_video_subcore1_clk_src_v2;
	video_subcore1_clk_src.c.fmax[VDD_DIG_HIGH] = 516000000;
}

static void msm_mmsscc_8996_v3_fixup(void)
{
	mmpll1.c.rate = 810000000;
	mmpll1.c.fmax[VDD_DIG_LOWER] = 405000000;
	mmpll1.c.fmax[VDD_DIG_LOW] = 405000000;
	mmpll1.c.fmax[VDD_DIG_NOMINAL] = 810000000;
	mmpll1.c.fmax[VDD_DIG_HIGH] = 810000000;

	mmpll2.vco_tbl = mmpll_gfx_vco;
	mmpll2.num_vco = ARRAY_SIZE(mmpll_gfx_vco),
	mmpll2.c.rate = 0;
	mmpll2.c.fmax[VDD_DIG_LOWER] = 1000000000;
	mmpll2.c.fmax[VDD_DIG_LOW] = 1000000000;
	mmpll2.c.fmax[VDD_DIG_NOMINAL] = 1000000000;
	mmpll2.c.fmax[VDD_DIG_HIGH] = 1000000000;
	mmpll2.no_prepared_reconfig = true;
	mmpll2.c.ops = &clk_ops_alpha_pll;

	mmpll3.c.rate = 1040000000;
	mmpll3.c.fmax[VDD_DIG_LOWER] = 520000000;
	mmpll3.c.fmax[VDD_DIG_LOW] = 520000000;
	mmpll3.c.fmax[VDD_DIG_NOMINAL] = 1040000000;
	mmpll3.c.fmax[VDD_DIG_HIGH] = 1040000000;

	mmpll5.c.rate = 825000000;
	mmpll5.c.fmax[VDD_DIG_LOWER] = 412500000;
	mmpll5.c.fmax[VDD_DIG_LOW] = 825000000;
	mmpll5.c.fmax[VDD_DIG_NOMINAL] = 825000000;
	mmpll5.c.fmax[VDD_DIG_HIGH] = 825000000;

	mmpll8.vco_tbl = mmpll_gfx_vco;
	mmpll8.num_vco = ARRAY_SIZE(mmpll_gfx_vco),
	mmpll8.c.rate = 0;
	mmpll8.c.fmax[VDD_DIG_LOWER] = 1000000000;
	mmpll8.c.fmax[VDD_DIG_LOW] = 1000000000;
	mmpll8.c.fmax[VDD_DIG_NOMINAL] = 1000000000;
	mmpll8.c.fmax[VDD_DIG_HIGH] = 1000000000;
	mmpll8.no_prepared_reconfig = true;
	mmpll8.c.ops = &clk_ops_alpha_pll;

	mmpll9.c.rate = 1248000000;
	mmpll9.c.fmax[VDD_DIG_LOWER] = 624000000;
	mmpll9.c.fmax[VDD_DIG_LOW] = 624000000;
	mmpll9.c.fmax[VDD_DIG_NOMINAL] = 1248000000;
	mmpll9.c.fmax[VDD_DIG_HIGH] = 1248000000;

	csi0_clk_src.freq_tbl = ftbl_csi0_clk_src_v3;
	csi1_clk_src.freq_tbl = ftbl_csi1_clk_src_v3;
	csi2_clk_src.freq_tbl = ftbl_csi2_clk_src_v3;
	csi3_clk_src.freq_tbl = ftbl_csi3_clk_src_v3;

	csiphy0_3p_clk_src.freq_tbl = ftbl_csiphy0_3p_clk_src_v2;
	csiphy0_3p_clk_src.c.fmax[VDD_DIG_HIGH] = 384000000;
	csiphy1_3p_clk_src.freq_tbl = ftbl_csiphy1_3p_clk_src_v2;
	csiphy1_3p_clk_src.c.fmax[VDD_DIG_HIGH] = 384000000;
	csiphy2_3p_clk_src.freq_tbl = ftbl_csiphy2_3p_clk_src_v2;
	csiphy2_3p_clk_src.c.fmax[VDD_DIG_HIGH] = 384000000;

	csi0phytimer_clk_src.freq_tbl = ftbl_csi0phytimer_clk_src_v3;
	csi0phytimer_clk_src.c.fmax[VDD_DIG_LOW] = 200000000;
	csi1phytimer_clk_src.freq_tbl = ftbl_csi1phytimer_clk_src_v3;
	csi1phytimer_clk_src.c.fmax[VDD_DIG_LOW] = 200000000;
	csi2phytimer_clk_src.freq_tbl = ftbl_csi2phytimer_clk_src_v3;
	csi2phytimer_clk_src.c.fmax[VDD_DIG_LOW] = 200000000;

	vfe0_clk_src.freq_tbl = ftbl_vfe0_clk_src_v3;
	vfe0_clk_src.c.fmax[VDD_DIG_LOWER] = 100000000;
	vfe0_clk_src.c.fmax[VDD_DIG_LOW] = 300000000;
	vfe1_clk_src.freq_tbl = ftbl_vfe1_clk_src_v3;
	vfe1_clk_src.c.fmax[VDD_DIG_LOW] = 300000000;

	mdp_clk_src.freq_tbl = ftbl_mdp_clk_src_v2;
	mdp_clk_src.c.fmax[VDD_DIG_LOW] = 275000000;
	mdp_clk_src.c.fmax[VDD_DIG_NOMINAL] = 330000000;
	mdp_clk_src.c.fmax[VDD_DIG_HIGH] = 412500000;

	maxi_clk_src.freq_tbl = ftbl_maxi_clk_src_v2;
	maxi_clk_src.c.fmax[VDD_DIG_HIGH] = 405000000;

	rbcpr_clk_src.freq_tbl = ftbl_rbcpr_clk_src_v2;
	rbcpr_clk_src.c.fmax[VDD_DIG_NOMINAL] = 50000000;
	rbcpr_clk_src.c.fmax[VDD_DIG_HIGH] = 50000000;

	video_core_clk_src.freq_tbl = ftbl_video_core_clk_src_v3;
	video_core_clk_src.c.fmax[VDD_DIG_NOMINAL] = 346666667;
	video_core_clk_src.c.fmax[VDD_DIG_HIGH] = 520000000;
	video_subcore0_clk_src.freq_tbl = ftbl_video_subcore0_clk_src_v3;
	video_subcore0_clk_src.c.fmax[VDD_DIG_NOMINAL] = 346666667;
	video_subcore0_clk_src.c.fmax[VDD_DIG_HIGH] = 520000000;
	video_subcore1_clk_src.freq_tbl = ftbl_video_subcore1_clk_src_v3;
	video_subcore1_clk_src.c.fmax[VDD_DIG_NOMINAL] = 346666667;
	video_subcore1_clk_src.c.fmax[VDD_DIG_HIGH] = 520000000;
}

static void msm_mmsscc_8996_pro_fixup(void)
{
	mmpll9.c.rate = 0;
	mmpll9.c.fmax[VDD_DIG_LOWER] = 652800000;
	mmpll9.c.fmax[VDD_DIG_LOW] = 652800000;
	mmpll9.c.fmax[VDD_DIG_NOMINAL] = 1305600000;
	mmpll9.c.fmax[VDD_DIG_HIGH] = 1305600000;
	mmpll9.c.ops = &clk_ops_alpha_pll;
	mmpll9.min_supported_freq = 1248000000;

	mmpll9_postdiv_clk.c.ops = &clk_ops_div;
}

static int is_v3_gpu;
static int gpu_pre_set_rate(struct clk *clk, unsigned long new_rate)
{
	struct msm_rpm_kvp kvp;
	struct clk_vdd_class *vdd = clk->vdd_class;
	int old_level, new_level, old_uv, new_uv;
	int n_regs = vdd->num_regulators;
	uint32_t value;
	int ret = 0;

	if (!is_v3_gpu)
		return ret;

	old_level = find_vdd_level(clk, clk->rate);
	if (old_level < 0)
		return old_level;
	old_uv = vdd->vdd_uv[old_level * n_regs];

	new_level = find_vdd_level(clk, new_rate);
	if (new_level < 0)
		return new_level;
	new_uv = vdd->vdd_uv[new_level * n_regs];

	if (new_uv == old_uv)
		return ret;

	value = (new_uv == GFX_MIN_SVS_LEVEL);

	kvp.key = RPM_SMD_KEY_STATE;
	kvp.data = (void *)&value;
	kvp.length = sizeof(value);

	ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET, RPM_MISC_CLK_TYPE,
					GPU_REQ_ID, &kvp, 1);
	if (ret)
		WARN_ONCE(1, "%s: Sending the RPM message failed (value - %u)\n",
					__func__, value);
	return 0;
}

static int of_get_fmax_vdd_class(struct platform_device *pdev, struct clk *c,
								char *prop_name)
{
	struct device_node *of = pdev->dev.of_node;
	int prop_len, i, j;
	struct clk_vdd_class *vdd = c->vdd_class;
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

	c->fmax = devm_kzalloc(&pdev->dev, prop_len * sizeof(unsigned long),
					GFP_KERNEL);
	if (!c->fmax)
		return -ENOMEM;

	array = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(u32) * num, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	of_property_read_u32_array(of, prop_name, array, prop_len * num);
	for (i = 0; i < prop_len; i++) {
		c->fmax[i] = array[num * i];
		for (j = 1; j < num; j++) {
			vdd->vdd_uv[(num - 1) * i + (j - 1)] =
						array[num * i + j];
		}
	}

	devm_kfree(&pdev->dev, array);
	vdd->num_levels = prop_len;
	vdd->cur_level = prop_len;
	c->num_fmax = prop_len;
	return 0;
}

static struct platform_driver msm_clock_gpu_driver;
struct resource *efuse_res;
void __iomem *gpu_base;
u64 efuse;
int gpu_speed_bin;

int msm_mmsscc_8996_probe(struct platform_device *pdev)
{
	struct resource *res;
	int rc;
	struct clk *tmp;
	struct regulator *reg;
	u32 regval;
	int is_pro, is_v2, is_v3 = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Unable to retrieve register base.\n");
		return -ENOMEM;
	}

	efuse_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (!efuse_res) {
		dev_err(&pdev->dev, "Unable to retrieve efuse register base.\n");
		return -ENOMEM;
	}

	virt_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_base) {
		dev_err(&pdev->dev, "Failed to map CC registers\n");
		return -ENOMEM;
	}

	gpu_base = devm_ioremap(&pdev->dev, efuse_res->start,
					resource_size(efuse_res));
	if (!gpu_base) {
		dev_err(&pdev->dev, "Unable to map in efuse base\n");
		return -ENOMEM;
	}

	/* Clear the DBG_CLK_DIV bits of the MMSS debug register */
	regval = readl_relaxed(virt_base + mmss_gcc_dbg_clk.offset);
	regval &= ~BM(18, 17);
	writel_relaxed(regval, virt_base + mmss_gcc_dbg_clk.offset);

	/* Disable the AHB DCD */
	regval = readl_relaxed(virt_base + MMSS_MNOC_DCD_CONFIG_AHB);
	regval &= ~BIT(31);
	writel_relaxed(regval, virt_base + MMSS_MNOC_DCD_CONFIG_AHB);

	/* Disable the NoC FSM for mmss_mmagic_cfg_ahb_clk */
	regval = readl_relaxed(virt_base + mmss_mmagic_cfg_ahb_clk.cbcr_reg);
	regval &= ~BIT(15);
	writel_relaxed(regval, virt_base + mmss_mmagic_cfg_ahb_clk.cbcr_reg);

	vdd_dig.vdd_uv[1] = RPM_REGULATOR_CORNER_SVS_KRAIT;
	reg = vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_dig regulator!");
		return PTR_ERR(reg);
	}

	reg = vdd_mmpll4.regulator[0] = devm_regulator_get(&pdev->dev,
								"mmpll4_dig");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get mmpll4_dig regulator!");
		return PTR_ERR(reg);
	}

	reg = vdd_mmpll4.regulator[1] = devm_regulator_get(&pdev->dev,
								"mmpll4_analog");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get mmpll4_analog regulator!");
		return PTR_ERR(reg);
	}

	tmp = mmsscc_xo.c.parent = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(tmp)) {
		if (PTR_ERR(tmp) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo clock!\n");
		return PTR_ERR(tmp);
	}

	tmp = mmsscc_gpll0.c.parent = devm_clk_get(&pdev->dev, "gpll0");
	if (IS_ERR(tmp)) {
		if (PTR_ERR(tmp) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get gpll0 clock!\n");
		return PTR_ERR(tmp);
	}

	tmp = mmsscc_gpll0_div.c.parent = devm_clk_get(&pdev->dev, "gpll0_div");
	if (IS_ERR(tmp)) {
		if (PTR_ERR(tmp) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get gpll0_div clock!\n");
		return PTR_ERR(tmp);
	}

	ext_pclk0_clk_src.dev = &pdev->dev;
	ext_pclk0_clk_src.clk_id = "pclk0_src";
	ext_pclk0_clk_src.c.flags = CLKFLAG_NO_RATE_CACHE;
	ext_pclk1_clk_src.dev = &pdev->dev;
	ext_pclk1_clk_src.clk_id = "pclk1_src";
	ext_pclk1_clk_src.c.flags = CLKFLAG_NO_RATE_CACHE;
	ext_byte0_clk_src.dev = &pdev->dev;
	ext_byte0_clk_src.clk_id = "byte0_src";
	ext_byte0_clk_src.c.flags = CLKFLAG_NO_RATE_CACHE;
	ext_byte1_clk_src.dev = &pdev->dev;
	ext_byte1_clk_src.clk_id = "byte1_src";
	ext_byte1_clk_src.c.flags = CLKFLAG_NO_RATE_CACHE;
	ext_extpclk_clk_src.dev = &pdev->dev;
	ext_extpclk_clk_src.clk_id = "extpclk_src";

	efuse = readl_relaxed(gpu_base);
	gpu_speed_bin = ((efuse >> EFUSE_SHIFT_v3) & EFUSE_MASK_v3);

	is_v2 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,mmsscc-8996-v2");
	if (is_v2)
		msm_mmsscc_8996_v2_fixup();

	is_v3 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,mmsscc-8996-v3");
	if (is_v3)
		msm_mmsscc_8996_v3_fixup();

	is_pro = of_device_is_compatible(pdev->dev.of_node,
						"qcom,mmsscc-8996-pro");
	if (is_pro) {
		gpu_speed_bin = ((efuse >> EFUSE_SHIFT_PRO) & EFUSE_MASK_PRO);
		msm_mmsscc_8996_v3_fixup();
		if (!gpu_speed_bin)
			msm_mmsscc_8996_pro_fixup();
	}

	rc = of_msm_clock_register(pdev->dev.of_node, msm_clocks_mmss_8996,
				   ARRAY_SIZE(msm_clocks_mmss_8996));
	if (rc)
		return rc;

	/* Register v2/v3/pro specific clocks */
	if (is_v2 || is_v3 || is_pro) {
		rc = of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_mmsscc_8996_v2,
				ARRAY_SIZE(msm_clocks_mmsscc_8996_v2));
		if (rc)
			return rc;
	}
	dev_info(&pdev->dev, "Registered MMSS clocks.\n");

	return platform_driver_register(&msm_clock_gpu_driver);
}

static struct of_device_id msm_clock_mmss_match_table[] = {
	{ .compatible = "qcom,mmsscc-8996" },
	{ .compatible = "qcom,mmsscc-8996-v2" },
	{ .compatible = "qcom,mmsscc-8996-v3" },
	{ .compatible = "qcom,mmsscc-8996-pro" },
	{},
};

static struct platform_driver msm_clock_mmss_driver = {
	.probe = msm_mmsscc_8996_probe,
	.driver = {
		.name = "qcom,mmsscc-8996",
		.of_match_table = msm_clock_mmss_match_table,
		.owner = THIS_MODULE,
	},
};

/* ======== Graphics Clock Controller ======== */

static struct mux_clk gpu_gcc_dbg_clk = {
	.ops = &mux_reg_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.offset = MMSS_MMSS_DEBUG_CLK_CTL,
	.en_offset = MMSS_MMSS_DEBUG_CLK_CTL,
	.base = &virt_base_gpu,
	MUX_SRC_LIST(
		{ &gpu_ahb_clk.c, 0x000c },
		{ &gpu_gx_gfx3d_clk.c, 0x000d },
		{ &gpu_gx_rbbmtimer_clk.c, 0x003e },
		{ &gpu_aon_isense_clk.c, 0x0088 },
	),
	.c = {
		.dbg_name = "gpu_gcc_dbg_clk",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gpu_gcc_dbg_clk.c),
	},
};

static struct clk_lookup msm_clocks_gpu_8996[] = {
	CLK_LIST(gfx3d_clk_src),
	CLK_LIST(rbbmtimer_clk_src),
	CLK_LIST(gpu_ahb_clk),
	CLK_LIST(gpu_aon_isense_clk),
	CLK_LIST(gpu_gx_gfx3d_clk),
	CLK_LIST(gpu_mx_clk),
	CLK_LIST(gpu_gx_rbbmtimer_clk),
	CLK_LIST(gpu_gcc_dbg_clk),
};

static struct clk_lookup msm_clocks_gpu_8996_v2[] = {
	CLK_LIST(gfx3d_clk_src_v2),
	CLK_LIST(rbbmtimer_clk_src),
	CLK_LIST(gpu_ahb_clk),
	CLK_LIST(gpu_aon_isense_clk),
	CLK_LIST(gpu_gx_gfx3d_clk),
	CLK_LIST(gpu_mx_clk),
	CLK_LIST(gpu_gx_rbbmtimer_clk),
	CLK_LIST(gpu_gcc_dbg_clk),
};

static void msm_gpucc_8996_v2_fixup(void)
{
	gpu_gx_gfx3d_clk.c.parent = &gfx3d_clk_src_v2.c;
}

int msm_gpucc_8996_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *of_node = pdev->dev.of_node;
	int rc;
	struct regulator *reg;
	int is_v2_gpu, is_v3_0_gpu, is_pro_gpu;
	char speedbin_str[] = "qcom,gfxfreq-speedbin0";
	char mx_speedbin_str[] = "qcom,gfxfreq-mx-speedbin0";

	if (!of_node)
		return -EINVAL;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Unable to retrieve register base.\n");
		return -ENOMEM;
	}

	gfx3d_clk_src_v2.base = virt_base_gpu =  devm_ioremap(&pdev->dev,
					res->start, resource_size(res));
	if (!virt_base_gpu) {
		dev_err(&pdev->dev, "Failed to map CC registers\n");
		return -ENOMEM;
	}

	reg = vdd_gfx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_gfx");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_gfx regulator!");
		return PTR_ERR(reg);
	}

	reg = vdd_gfx.regulator[1] = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_mx regulator!");
		return PTR_ERR(reg);
	}
	vdd_gfx.use_max_uV = true;

	reg = vdd_gpu_mx.regulator[0] = devm_regulator_get(&pdev->dev,
								"vdd_gpu_mx");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_gpu_mx regulator!");
		return PTR_ERR(reg);
	}
	vdd_gpu_mx.use_max_uV = true;

	is_v2_gpu = of_device_is_compatible(of_node, "qcom,gpucc-8996-v2");
	is_v3_gpu = of_device_is_compatible(of_node, "qcom,gpucc-8996-v3");
	is_v3_0_gpu = of_device_is_compatible(of_node, "qcom,gpucc-8996-v3.0");
	is_pro_gpu = of_device_is_compatible(of_node, "qcom,gpucc-8996-pro");

	dev_info(&pdev->dev, "using speed bin %u\n", gpu_speed_bin);
	snprintf(speedbin_str, ARRAY_SIZE(speedbin_str),
				"qcom,gfxfreq-speedbin%d", gpu_speed_bin);
	snprintf(mx_speedbin_str, ARRAY_SIZE(mx_speedbin_str),
				"qcom,gfxfreq-mx-speedbin%d", gpu_speed_bin);

	rc = of_get_fmax_vdd_class(pdev, &gpu_mx_clk.c, mx_speedbin_str);
	if (rc) {
		dev_err(&pdev->dev, "Can't get speed bin for gpu_mx_clk. Falling back to zero.\n");
		rc = of_get_fmax_vdd_class(pdev, &gpu_mx_clk.c,
						"qcom,gfxfreq-mx-speedbin0");
		if (rc) {
			dev_err(&pdev->dev, "Unable to get gpu mx freq-corner mapping info\n");
			return rc;
		}
	}

	if (!is_v2_gpu && !is_v3_gpu && !is_v3_0_gpu && !is_pro_gpu) {
		rc = of_get_fmax_vdd_class(pdev, &gfx3d_clk_src.c,
							speedbin_str);
		if (rc) {
			dev_err(&pdev->dev, "Can't get speed bin for gfx3d_clk_src. Falling back to zero.\n");
			rc = of_get_fmax_vdd_class(pdev, &gfx3d_clk_src.c,
					    "qcom,gfxfreq-speedbin0");
			if (rc) {
				dev_err(&pdev->dev, "Unable to get gfx freq-corner info for gfx3d_clk!\n");
				return rc;
			}
		}

		clk_ops_gpu = clk_ops_rcg;
		clk_ops_gpu.pre_set_rate = gpu_pre_set_rate;

		rc = of_msm_clock_register(of_node, msm_clocks_gpu_8996,
					ARRAY_SIZE(msm_clocks_gpu_8996));
		if (rc)
			return rc;
	} else {
		msm_gpucc_8996_v2_fixup();
		rc = of_get_fmax_vdd_class(pdev, &gfx3d_clk_src_v2.c,
							speedbin_str);
		if (rc) {
			dev_err(&pdev->dev, "Can't get speed bin for gfx3d_clk_src. Falling back to zero.\n");
			rc = of_get_fmax_vdd_class(pdev, &gfx3d_clk_src_v2.c,
					    "qcom,gfxfreq-speedbin0");
			if (rc) {
				dev_err(&pdev->dev, "Unable to get gfx freq-corner info for gfx3d_clk!\n");
				return rc;
			}
		}

		clk_ops_gpu = clk_ops_mux_div_clk;
		clk_ops_gpu.pre_set_rate = gpu_pre_set_rate;

		rc = of_msm_clock_register(of_node, msm_clocks_gpu_8996_v2,
					ARRAY_SIZE(msm_clocks_gpu_8996_v2));
		if (rc)
			return rc;
	}

	dev_info(&pdev->dev, "Registered GPU clocks.\n");
	return 0;
}

static struct of_device_id msm_clock_gpu_match_table[] = {
	{ .compatible = "qcom,gpucc-8996" },
	{ .compatible = "qcom,gpucc-8996-v2" },
	{ .compatible = "qcom,gpucc-8996-v3" },
	{ .compatible = "qcom,gpucc-8996-v3.0" },
	{ .compatible = "qcom,gpucc-8996-pro" },
	{},
};

static struct platform_driver msm_clock_gpu_driver = {
	.probe = msm_gpucc_8996_probe,
	.driver = {
		.name = "qcom,gpucc-8996",
		.of_match_table = msm_clock_gpu_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_mmsscc_8996_init(void)
{
	return platform_driver_register(&msm_clock_mmss_driver);
}
arch_initcall(msm_mmsscc_8996_init);

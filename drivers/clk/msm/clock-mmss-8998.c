/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk/msm-clock-generic.h>

#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-alpha-pll.h>

#include <dt-bindings/clock/msm-clocks-8998.h>
#include <dt-bindings/clock/msm-clocks-hwio-8998.h>

#include "vdd-level-8998.h"
#include "reset.h"

static void __iomem *virt_base;

#define mmsscc_xo_mm_source_val			0
#define mmsscc_gpll0_mm_source_val		5
#define mmsscc_gpll0_div_mm_source_val		6
#define mmpll0_pll_out_mm_source_val		1
#define mmpll1_pll_out_mm_source_val		2
#define mmpll3_pll_out_mm_source_val		3
#define mmpll4_pll_out_mm_source_val		2
#define mmpll5_pll_out_mm_source_val		2
#define mmpll6_pll_out_mm_source_val		4
#define mmpll7_pll_out_mm_source_val		3
#define mmpll10_pll_out_mm_source_val		4
#define dsi0phypll_mm_source_val		1
#define dsi1phypll_mm_source_val		2
#define hdmiphypll_mm_source_val		1
#define ext_dp_phy_pll_link_mm_source_val	1
#define ext_dp_phy_pll_vco_mm_source_val	2

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

#define F_SLEW(f, s_f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_freq = (s_f), \
		.src_clk = &s.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

DEFINE_EXT_CLK(mmsscc_xo, NULL);
DEFINE_EXT_CLK(mmsscc_gpll0, NULL);
DEFINE_EXT_CLK(mmsscc_gpll0_div, NULL);
DEFINE_EXT_CLK(ext_dp_phy_pll_vco, NULL);
DEFINE_EXT_CLK(ext_dp_phy_pll_link, NULL);

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);
static DEFINE_VDD_REGULATORS(vdd_mmsscc_mx, VDD_DIG_NUM, 1, vdd_corner, NULL);

static struct alpha_pll_masks pll_masks_p = {
	.lock_mask = BIT(31),
	.active_mask = BIT(30),
	.update_mask = BIT(22),
	.output_mask = 0xf,
};

static struct pll_vote_clk mmpll0_pll = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)MMSS_MMPLL0_PLL_MODE,
	.status_mask = BIT(31),
	.base = &virt_base,
	.c = {
		.rate = 808000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll0",
		.ops = &clk_ops_pll_vote,
		VDD_MM_PLL_FMAX_MAP2(LOWER, 404000000, NOMINAL, 808000000),
		CLK_INIT(mmpll0_pll.c),
	},
};
DEFINE_EXT_CLK(mmpll0_pll_out, &mmpll0_pll.c);

static struct pll_vote_clk mmpll1_pll = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)MMSS_MMPLL1_PLL_MODE,
	.status_mask = BIT(31),
	.base = &virt_base,
	.c = {
		.rate = 812000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll1_pll",
		.ops = &clk_ops_pll_vote,
		VDD_MM_PLL_FMAX_MAP2(LOWER, 406000000, NOMINAL, 812000000),
		CLK_INIT(mmpll1_pll.c),
	},
};
DEFINE_EXT_CLK(mmpll1_pll_out, &mmpll1_pll.c);

static struct alpha_pll_clk mmpll3_pll = {
	.offset = MMSS_MMPLL3_PLL_MODE,
	.masks = &pll_masks_p,
	.enable_config = 0x1,
	.base = &virt_base,
	.is_fabia = true,
	.c = {
		.rate = 930000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll3_pll",
		.ops = &clk_ops_fixed_fabia_alpha_pll,
		VDD_MM_PLL_FMAX_MAP2(LOWER, 465000000, LOW, 930000000),
		CLK_INIT(mmpll3_pll.c),
	},
};
DEFINE_EXT_CLK(mmpll3_pll_out, &mmpll3_pll.c);

static struct alpha_pll_clk mmpll4_pll = {
	.offset = MMSS_MMPLL4_PLL_MODE,
	.masks = &pll_masks_p,
	.enable_config = 0x1,
	.base = &virt_base,
	.is_fabia = true,
	.c = {
		.rate = 768000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll4_pll",
		.ops = &clk_ops_fixed_fabia_alpha_pll,
		VDD_MM_PLL_FMAX_MAP2(LOWER, 384000000, LOW, 768000000),
		CLK_INIT(mmpll4_pll.c),
	},
};
DEFINE_EXT_CLK(mmpll4_pll_out, &mmpll4_pll.c);

static struct alpha_pll_clk mmpll5_pll = {
	.offset = MMSS_MMPLL5_PLL_MODE,
	.masks = &pll_masks_p,
	.enable_config = 0x1,
	.base = &virt_base,
	.is_fabia = true,
	.c = {
		.rate = 825000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll5_pll",
		.ops = &clk_ops_fixed_fabia_alpha_pll,
		VDD_MM_PLL_FMAX_MAP2(LOWER, 412500000, LOW, 825000000),
		CLK_INIT(mmpll5_pll.c),
	},
};
DEFINE_EXT_CLK(mmpll5_pll_out, &mmpll5_pll.c);

static struct alpha_pll_clk mmpll6_pll = {
	.offset = MMSS_MMPLL6_PLL_MODE,
	.masks = &pll_masks_p,
	.enable_config = 0x1,
	.base = &virt_base,
	.is_fabia = true,
	.c = {
		.rate = 720000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll6_pll",
		.ops = &clk_ops_fixed_fabia_alpha_pll,
		VDD_MM_PLL_FMAX_MAP2(LOWER, 360000000, NOMINAL, 720000000),
		CLK_INIT(mmpll6_pll.c),
	},
};
DEFINE_EXT_CLK(mmpll6_pll_out, &mmpll6_pll.c);

static struct alpha_pll_clk mmpll7_pll = {
	.offset = MMSS_MMPLL7_PLL_MODE,
	.masks = &pll_masks_p,
	.enable_config = 0x1,
	.base = &virt_base,
	.is_fabia = true,
	.c = {
		.rate = 960000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll7_pll",
		.ops = &clk_ops_fixed_fabia_alpha_pll,
		VDD_MM_PLL_FMAX_MAP1(LOW, 960000000),
		CLK_INIT(mmpll7_pll.c),
	},
};
DEFINE_EXT_CLK(mmpll7_pll_out, &mmpll7_pll.c);

static struct alpha_pll_clk mmpll10_pll = {
	.offset = MMSS_MMPLL10_PLL_MODE,
	.masks = &pll_masks_p,
	.enable_config = 0x1,
	.base = &virt_base,
	.is_fabia = true,
	.c = {
		.rate = 576000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll10_pll",
		.ops = &clk_ops_fixed_fabia_alpha_pll,
		VDD_MM_PLL_FMAX_MAP2(LOWER, 288000000, NOMINAL, 576000000),
		CLK_INIT(mmpll10_pll.c),
	},
};
DEFINE_EXT_CLK(mmpll10_pll_out, &mmpll10_pll.c);

static struct clk_freq_tbl ftbl_ahb_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_MM(  40000000,   mmsscc_gpll0,   15,    0,     0),
	F_MM(  80800000, mmpll0_pll_out,   10,    0,     0),
	F_END
};

static struct rcg_clk ahb_clk_src = {
	.cmd_rcgr_reg = MMSS_AHB_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ahb_clk_src,
	.current_freq = &rcg_dummy_freq,
	.non_local_control_timeout = 1000,
	.base = &virt_base,
	.c = {
		.dbg_name = "ahb_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 40000000,
					NOMINAL, 80800000),
		CLK_INIT(ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi_clk_src[] = {
	F_MM( 164571429, mmpll10_pll_out,  3.5,    0,     0),
	F_MM( 256000000,  mmpll4_pll_out,    3,    0,     0),
	F_MM( 384000000,  mmpll4_pll_out,    2,    0,     0),
	F_MM( 576000000, mmpll10_pll_out,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csi_clk_src_vq[] = {
	F_MM( 164571429, mmpll10_pll_out,  3.5,    0,     0),
	F_MM( 256000000,  mmpll4_pll_out,    3,    0,     0),
	F_MM( 274290000,  mmpll7_pll_out,  3.5,    0,     0),
	F_MM( 300000000,    mmsscc_gpll0,    2,    0,     0),
	F_MM( 384000000,  mmpll4_pll_out,    2,    0,     0),
	F_MM( 576000000, mmpll10_pll_out,    1,    0,     0),
	F_END
};

static struct rcg_clk csi0_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi0_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 164571429, LOW, 256000000,
					NOMINAL, 384000000, HIGH, 576000000),
		CLK_INIT(csi0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vfe_clk_src[] = {
	F_MM( 200000000,    mmsscc_gpll0,    3,    0,     0),
	F_MM( 300000000,    mmsscc_gpll0,    2,    0,     0),
	F_MM( 320000000,  mmpll7_pll_out,    3,    0,     0),
	F_MM( 384000000,  mmpll4_pll_out,    2,    0,     0),
	F_MM( 576000000, mmpll10_pll_out,    1,    0,     0),
	F_MM( 600000000,    mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_vfe_clk_src_vq[] = {
	F_MM( 200000000,    mmsscc_gpll0,    3,    0,     0),
	F_MM( 300000000,    mmsscc_gpll0,    2,    0,     0),
	F_MM( 320000000,  mmpll7_pll_out,    3,    0,     0),
	F_MM( 384000000,  mmpll4_pll_out,    2,    0,     0),
	F_MM( 404000000,  mmpll0_pll_out,    2,    0,     0),
	F_MM( 480000000,  mmpll7_pll_out,    2,    0,     0),
	F_MM( 576000000, mmpll10_pll_out,    1,    0,     0),
	F_MM( 600000000,    mmsscc_gpll0,    1,    0,     0),
	F_END
};


static struct rcg_clk vfe0_clk_src = {
	.cmd_rcgr_reg = MMSS_VFE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vfe_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "vfe0_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 200000000, LOW, 384000000,
					NOMINAL, 576000000, HIGH, 600000000),
		CLK_INIT(vfe0_clk_src.c),
	},
};

static struct rcg_clk vfe1_clk_src = {
	.cmd_rcgr_reg = MMSS_VFE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vfe_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "vfe1_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 200000000, LOW, 384000000,
					NOMINAL, 576000000, HIGH, 600000000),
		CLK_INIT(vfe1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdp_clk_src[] = {
	F_MM(  85714286,   mmsscc_gpll0,    7,    0,     0),
	F_MM( 100000000,   mmsscc_gpll0,    6,    0,     0),
	F_MM( 150000000,   mmsscc_gpll0,    4,    0,     0),
	F_MM( 171428571,   mmsscc_gpll0,  3.5,    0,     0),
	F_MM( 200000000,   mmsscc_gpll0,    3,    0,     0),
	F_MM( 275000000, mmpll5_pll_out,    3,    0,     0),
	F_MM( 300000000,   mmsscc_gpll0,    2,    0,     0),
	F_MM( 330000000, mmpll5_pll_out,  2.5,    0,     0),
	F_MM( 412500000, mmpll5_pll_out,    2,    0,     0),
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 171430000, LOW, 275000000,
					NOMINAL, 330000000, HIGH, 412500000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_maxi_clk_src[] = {
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_MM(  75000000, mmsscc_gpll0_div,    4,    0,     0),
	F_MM( 171428571,     mmsscc_gpll0,  3.5,    0,     0),
	F_MM( 323200000,   mmpll0_pll_out,  2.5,    0,     0),
	F_MM( 406000000,   mmpll1_pll_out,    2,    0,     0),
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 171428571,
					NOMINAL, 323200000, HIGH, 406000000),
		CLK_INIT(maxi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_cpp_clk_src[] = {
	F_MM( 100000000,    mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,    mmsscc_gpll0,    3,    0,     0),
	F_MM( 576000000, mmpll10_pll_out,    1,    0,     0),
	F_MM( 600000000,    mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_cpp_clk_src_vq[] = {
	F_MM( 100000000,    mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,    mmsscc_gpll0,    3,    0,     0),
	F_MM( 384000000,  mmpll4_pll_out,    2,    0,     0),
	F_MM( 404000000,  mmpll0_pll_out,    2,    0,     0),
	F_MM( 480000000,  mmpll7_pll_out,    2,    0,     0),
	F_MM( 576000000, mmpll10_pll_out,    1,    0,     0),
	F_MM( 600000000,    mmsscc_gpll0,    1,    0,     0),
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 576000000, HIGH, 600000000),
		CLK_INIT(cpp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_jpeg0_clk_src[] = {
	F_MM(  75000000,   mmsscc_gpll0,    8,    0,     0),
	F_MM( 150000000,   mmsscc_gpll0,    4,    0,     0),
	F_MM( 480000000, mmpll7_pll_out,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_jpeg0_clk_src_vq[] = {
	F_MM(  75000000,   mmsscc_gpll0,    8,    0,     0),
	F_MM( 150000000,   mmsscc_gpll0,    4,    0,     0),
	F_MM( 320000000, mmpll7_pll_out,    3,    0,     0),
	F_MM( 480000000, mmpll7_pll_out,    2,    0,     0),
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 75000000, LOW, 150000000,
					NOMINAL, 480000000),
		CLK_INIT(jpeg0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_rot_clk_src[] = {
	F_MM( 171428571,   mmsscc_gpll0,  3.5,    0,     0),
	F_MM( 275000000, mmpll5_pll_out,    3,    0,     0),
	F_MM( 330000000, mmpll5_pll_out,  2.5,    0,     0),
	F_MM( 412500000, mmpll5_pll_out,    2,    0,     0),
	F_END
};

static struct rcg_clk rot_clk_src = {
	.cmd_rcgr_reg = MMSS_ROT_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_rot_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "rot_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 171430000, LOW, 275000000,
					NOMINAL, 330000000, HIGH, 412500000),
		CLK_INIT(rot_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_video_core_clk_src[] = {
	F_MM( 100000000,   mmsscc_gpll0,    6,    0,     0),
	F_MM( 186000000, mmpll3_pll_out,    5,    0,     0),
	F_MM( 360000000, mmpll6_pll_out,    2,    0,     0),
	F_MM( 465000000, mmpll3_pll_out,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_video_core_clk_src_vq[] = {
	F_MM( 200000000,   mmsscc_gpll0,    3,    0,     0),
	F_MM( 269330000, mmpll0_pll_out,    3,    0,     0),
	F_MM( 355200000, mmpll6_pll_out,  2.5,    0,     0),
	F_MM( 444000000, mmpll6_pll_out,    2,    0,     0),
	F_MM( 533000000, mmpll3_pll_out,    2,    0,     0),
	F_END
};

static struct rcg_clk video_core_clk_src = {
	.cmd_rcgr_reg = MMSS_VIDEO_CORE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_video_core_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_core_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 186000000,
					NOMINAL, 360000000, HIGH, 465000000),
		CLK_INIT(video_core_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csiphy_clk_src[] = {
	F_MM(  164570000,  mmpll10_pll_out, 3.5,    0,     0),
	F_MM(  256000000,   mmpll4_pll_out,   3,    0,     0),
	F_MM(  384000000,   mmpll4_pll_out,   2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_csiphy_clk_src_vq[] = {
	F_MM(  164570000,  mmpll10_pll_out, 3.5,    0,     0),
	F_MM(  256000000,   mmpll4_pll_out,   3,    0,     0),
	F_MM(  274290000,   mmpll7_pll_out, 3.5,    0,     0),
	F_MM(  300000000,     mmsscc_gpll0,   2,    0,     0),
	F_MM(  384000000,   mmpll4_pll_out,   2,    0,     0),
	F_END
};

static struct rcg_clk csiphy_clk_src = {
	.cmd_rcgr_reg = MMSS_CSIPHY_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csiphy_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csiphy_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 164570000, LOW, 256000000,
					NOMINAL, 384000000),
		CLK_INIT(csiphy_clk_src.c),
	},
};

static struct rcg_clk csi1_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi1_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 164570000, LOW, 256000000,
					NOMINAL, 384000000, HIGH, 576000000),
		CLK_INIT(csi1_clk_src.c),
	},
};

static struct rcg_clk csi2_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi2_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 164570000, LOW, 256000000,
					NOMINAL, 384000000, HIGH, 576000000),
		CLK_INIT(csi2_clk_src.c),
	},
};

static struct rcg_clk csi3_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi3_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 164570000, LOW, 256000000,
					NOMINAL, 384000000, HIGH, 576000000),
		CLK_INIT(csi3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_fd_core_clk_src[] = {
	F_MM( 100000000,    mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,    mmsscc_gpll0,    3,    0,     0),
	F_MM( 576000000, mmpll10_pll_out,    1,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_fd_core_clk_src_vq[] = {
	F_MM( 100000000,    mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,    mmsscc_gpll0,    3,    0,     0),
	F_MM( 404000000,  mmpll0_pll_out,    2,    0,     0),
	F_MM( 480000000,  mmpll7_pll_out,    2,    0,     0),
	F_MM( 576000000, mmpll10_pll_out,    1,    0,     0),
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 576000000),
		CLK_INIT(fd_core_clk_src.c),
	},
};

DEFINE_EXT_CLK(ext_byte0_clk_src, NULL);
DEFINE_EXT_CLK(ext_byte1_clk_src, NULL);
static struct clk_freq_tbl ftbl_byte_clk_src[] = {
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
	.current_freq = ftbl_byte_clk_src,
	.freq_tbl = ftbl_byte_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "byte0_clk_src",
		.parent = &ext_byte0_clk_src.c,
		.ops = &clk_ops_byte_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 150000000, LOW, 240000000,
						NOMINAL, 357140000),
		CLK_INIT(byte0_clk_src.c),
	},
};

static struct rcg_clk byte1_clk_src = {
	.cmd_rcgr_reg = MMSS_BYTE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.current_freq = ftbl_byte_clk_src,
	.freq_tbl = ftbl_byte_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "byte1_clk_src",
		.parent = &ext_byte1_clk_src.c,
		.ops = &clk_ops_byte_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 150000000, LOW, 240000000,
						NOMINAL, 357140000),
		CLK_INIT(byte1_clk_src.c),
	},
};

DEFINE_EXT_CLK(ext_pclk0_clk_src, NULL);
DEFINE_EXT_CLK(ext_pclk1_clk_src, NULL);
static struct clk_freq_tbl ftbl_pclk_clk_src[] = {
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
	.current_freq = ftbl_pclk_clk_src,
	.freq_tbl = ftbl_pclk_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "pclk0_clk_src",
		.parent = &ext_pclk0_clk_src.c,
		.ops = &clk_ops_pixel_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 184000000, LOW, 295000000,
						NOMINAL, 610000000),
		CLK_INIT(pclk0_clk_src.c),
	},
};

static struct rcg_clk pclk1_clk_src = {
	.cmd_rcgr_reg = MMSS_PCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = ftbl_pclk_clk_src,
	.freq_tbl = ftbl_pclk_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "pclk1_clk_src",
		.parent = &ext_pclk1_clk_src.c,
		.ops = &clk_ops_pixel_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 184000000, LOW, 295000000,
						NOMINAL, 610000000),
		CLK_INIT(pclk1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_video_subcore_clk_src[] = {
	F_MM( 100000000,   mmsscc_gpll0,    6,    0,     0),
	F_MM( 186000000, mmpll3_pll_out,    5,    0,     0),
	F_MM( 360000000, mmpll6_pll_out,    2,    0,     0),
	F_MM( 465000000, mmpll3_pll_out,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_video_subcore_clk_src_vq[] = {
	F_MM( 200000000,   mmsscc_gpll0,    3,    0,     0),
	F_MM( 269330000, mmpll0_pll_out,    3,    0,     0),
	F_MM( 355200000, mmpll6_pll_out,  2.5,    0,     0),
	F_MM( 444000000, mmpll6_pll_out,    2,    0,     0),
	F_MM( 533000000, mmpll3_pll_out,    2,    0,     0),
	F_END
};

static struct rcg_clk video_subcore0_clk_src = {
	.cmd_rcgr_reg = MMSS_VIDEO_SUBCORE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_video_subcore_clk_src,
	.current_freq = &rcg_dummy_freq,
	.non_local_control_timeout = 1000,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_subcore0_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 186000000,
					NOMINAL, 360000000, HIGH, 465000000),
		CLK_INIT(video_subcore0_clk_src.c),
	},
};

static struct rcg_clk video_subcore1_clk_src = {
	.cmd_rcgr_reg = MMSS_VIDEO_SUBCORE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_video_subcore_clk_src,
	.current_freq = &rcg_dummy_freq,
	.non_local_control_timeout = 1000,
	.base = &virt_base,
	.c = {
		.dbg_name = "video_subcore1_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 186000000,
					NOMINAL, 360000000, HIGH, 465000000),
		CLK_INIT(video_subcore1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_cci_clk_src[] = {
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 37500000, LOW, 50000000,
					NOMINAL, 100000000),
		CLK_INIT(cci_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_gp_clk_src[] = {
	F_MM(     10000,	mmsscc_xo,   16,    1,   120),
	F_MM(     24000,	mmsscc_xo,   16,    1,    50),
	F_MM(   6000000,     mmsscc_gpll0,   10,    1,    10),
	F_MM(  12000000,     mmsscc_gpll0,   10,    1,     5),
	F_MM(  13000000,     mmsscc_gpll0,    4,   13,   150),
	F_MM(  24000000,     mmsscc_gpll0,    5,    1,     5),
	F_END
};

static struct rcg_clk camss_gp0_clk_src = {
	.cmd_rcgr_reg = MMSS_CAMSS_GP0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_gp0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
					NOMINAL, 200000000),
		CLK_INIT(camss_gp0_clk_src.c),
	},
};

static struct rcg_clk camss_gp1_clk_src = {
	.cmd_rcgr_reg = MMSS_CAMSS_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
					NOMINAL, 200000000),
		CLK_INIT(camss_gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mclk_clk_src[] = {
	F_MM(   4800000,	mmsscc_xo,    4,    0,     0),
	F_MM(   6000000, mmsscc_gpll0_div,   10,    1,     5),
	F_MM(   8000000, mmsscc_gpll0_div,    1,    2,    75),
	F_MM(   9600000,	mmsscc_xo,    2,    0,     0),
	F_MM(  16666667, mmsscc_gpll0_div,    2,    1,     9),
	F_MM(  19200000,	mmsscc_xo,    1,    0,     0),
	F_MM(  24000000, mmsscc_gpll0_div,    1,    2,    25),
	F_MM(  33333333, mmsscc_gpll0_div,    1,    1,     9),
	F_MM(  48000000,     mmsscc_gpll0,    1,    2,    25),
	F_MM(  66666667,     mmsscc_gpll0,    1,    1,     9),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg = MMSS_MCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 33333333, LOW, 66666667,
					NOMINAL, 68571429),
		CLK_INIT(mclk0_clk_src.c),
	},
};

static struct rcg_clk mclk1_clk_src = {
	.cmd_rcgr_reg = MMSS_MCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 33333333, LOW, 66666667,
					NOMINAL, 68571429),
		CLK_INIT(mclk1_clk_src.c),
	},
};

static struct rcg_clk mclk2_clk_src = {
	.cmd_rcgr_reg = MMSS_MCLK2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 33333333, LOW, 66666667,
					NOMINAL, 68571429),
		CLK_INIT(mclk2_clk_src.c),
	},
};

static struct rcg_clk mclk3_clk_src = {
	.cmd_rcgr_reg = MMSS_MCLK3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 33333333, LOW, 66666667,
					NOMINAL, 68571429),
		CLK_INIT(mclk3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csiphytimer_clk_src[] = {
	F_MM( 200000000,   mmsscc_gpll0,    3,    0,     0),
	F_MM( 269333333, mmpll0_pll_out,    3,    0,     0),
	F_END
};

static struct rcg_clk csi0phytimer_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI0PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csiphytimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi0phytimer_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 269333333),
		CLK_INIT(csi0phytimer_clk_src.c),
	},
};

static struct rcg_clk csi1phytimer_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI1PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csiphytimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi1phytimer_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 269333333),
		CLK_INIT(csi1phytimer_clk_src.c),
	},
};

static struct rcg_clk csi2phytimer_clk_src = {
	.cmd_rcgr_reg = MMSS_CSI2PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csiphytimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi2phytimer_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
					NOMINAL, 269333333),
		CLK_INIT(csi2phytimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_dp_gtc_clk_src[] = {
	F_MM( 300000000,   mmsscc_gpll0,    2,    0,     0),
	F_END
};

static struct rcg_clk dp_gtc_clk_src = {
	.cmd_rcgr_reg = MMSS_DP_GTC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_dp_gtc_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "dp_gtc_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP2(LOWER, 40000000, LOW, 300000000),
		CLK_INIT(dp_gtc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_esc_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk esc0_clk_src = {
	.cmd_rcgr_reg = MMSS_ESC0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_esc_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "esc0_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, NOMINAL, 19200000),
		CLK_INIT(esc0_clk_src.c),
	},
};

static struct rcg_clk esc1_clk_src = {
	.cmd_rcgr_reg = MMSS_ESC1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_esc_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "esc1_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, NOMINAL, 19200000),
		CLK_INIT(esc1_clk_src.c),
	},
};

DEFINE_EXT_CLK(ext_extpclk_clk_src, NULL);
static struct clk_freq_tbl ftbl_extpclk_clk_src[] = {
	{
		.div_src_val = BVAL(10, 8, hdmiphypll_mm_source_val),
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
		.flags = CLKFLAG_NO_RATE_CACHE,
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, NOMINAL, 19200000),
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, NOMINAL, 19200000),
		CLK_INIT(vsync_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_dp_aux_clk_src[] = {
	F_MM(  19200000,      mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk dp_aux_clk_src = {
	.cmd_rcgr_reg = MMSS_DP_AUX_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_dp_aux_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "dp_aux_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, NOMINAL, 19200000),
		CLK_INIT(dp_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_dp_pixel_clk_src[] = {
	{
		.div_src_val = BVAL(10, 8, ext_dp_phy_pll_vco_mm_source_val),
		.src_clk = &ext_dp_phy_pll_vco.c,
	},
	F_END
};

static struct rcg_clk dp_pixel_clk_src = {
	.cmd_rcgr_reg = MMSS_DP_PIXEL_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = ftbl_dp_pixel_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "dp_pixel_clk_src",
		.parent = &ext_dp_phy_pll_vco.c,
		.ops = &clk_ops_rcg_dp,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 154000000, LOW, 337500000,
					NOMINAL, 675000000),
		CLK_INIT(dp_pixel_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_dp_link_clk_src[] = {
	F_SLEW( 162000,  324000, ext_dp_phy_pll_link,   2,   0,   0),
	F_SLEW( 270000,  540000, ext_dp_phy_pll_link,   2,   0,   0),
	F_SLEW( 540000, 1080000, ext_dp_phy_pll_link,   2,   0,   0),
	F_END
};

static struct rcg_clk dp_link_clk_src = {
	.cmd_rcgr_reg = MMSS_DP_LINK_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_dp_link_clk_src,
	.current_freq = ftbl_dp_link_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "dp_link_clk_src",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 162000, LOW, 270000,
					NOMINAL, 540000),
		CLK_INIT(dp_link_clk_src.c),
	},
};

/*
 * Current understanding is that the DP PLL is going to be configured by using
 * the set_rate ops for the dp_link_clk_src and dp_pixel_clk_src. When set_rate
 * is called on this RCG, the rate call never makes it to the external DP
 * clocks.
 */
static struct clk_freq_tbl ftbl_dp_crypto_clk_src[] = {
	F_MM( 101250, ext_dp_phy_pll_link,   1,   5,   16),
	F_MM( 168750, ext_dp_phy_pll_link,   1,   5,   16),
	F_MM( 337500, ext_dp_phy_pll_link,   1,   5,   16),
	F_END
};

static struct rcg_clk dp_crypto_clk_src = {
	.cmd_rcgr_reg = MMSS_DP_CRYPTO_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_dp_crypto_clk_src,
	.current_freq = ftbl_dp_crypto_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "dp_crypto_clk_src",
		.ops = &clk_ops_rcg_mnd,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 101250, LOW, 168750,
					NOMINAL, 337500),
		CLK_INIT(dp_crypto_clk_src.c),
	},
};

static struct branch_clk mmss_bimc_smmu_ahb_clk = {
	.cbcr_reg = MMSS_BIMC_SMMU_AHB_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.no_halt_check_on_disable = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_bimc_smmu_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_bimc_smmu_ahb_clk.c),
	},
};

static struct branch_clk mmss_bimc_smmu_axi_clk = {
	.cbcr_reg = MMSS_BIMC_SMMU_AXI_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.no_halt_check_on_disable = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_bimc_smmu_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_bimc_smmu_axi_clk.c),
	},
};

static struct branch_clk mmss_snoc_dvm_axi_clk = {
	.cbcr_reg = MMSS_SNOC_DVM_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_snoc_dvm_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_snoc_dvm_axi_clk.c),
	},
};

static struct branch_clk mmss_camss_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_cci_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CCI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cci_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cci_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_cci_clk = {
	.cbcr_reg = MMSS_CAMSS_CCI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cci_clk",
		.parent = &cci_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cci_clk.c),
	},
};

static struct branch_clk mmss_camss_cpp_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CPP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cpp_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cpp_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_cpp_clk = {
	.cbcr_reg = MMSS_CAMSS_CPP_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cpp_clk",
		.parent = &cpp_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cpp_clk.c),
	},
};

static struct branch_clk mmss_camss_cpp_axi_clk = {
	.cbcr_reg = MMSS_CAMSS_CPP_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cpp_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cpp_axi_clk.c),
	},
};

static struct branch_clk mmss_camss_cpp_vbif_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CPP_VBIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cpp_vbif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cpp_vbif_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_cphy_csid0_clk = {
	.cbcr_reg = MMSS_CAMSS_CPHY_CSID0_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cphy_csid0_clk",
		.parent = &csiphy_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cphy_csid0_clk.c),
	},
};

static struct branch_clk mmss_camss_csi0_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi0_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_csi0_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi0_clk",
		.parent = &csi0_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi0_clk.c),
	},
};

static struct branch_clk mmss_camss_csi0pix_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi0pix_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi0pix_clk.c),
	},
};

static struct branch_clk mmss_camss_csi0rdi_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi0rdi_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi0rdi_clk.c),
	},
};

static struct branch_clk mmss_camss_cphy_csid1_clk = {
	.cbcr_reg = MMSS_CAMSS_CPHY_CSID1_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cphy_csid1_clk",
		.parent = &csiphy_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cphy_csid1_clk.c),
	},
};

static struct branch_clk mmss_camss_csi1_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi1_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_csi1_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi1_clk",
		.parent = &csi1_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi1_clk.c),
	},
};

static struct branch_clk mmss_camss_csi1pix_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi1pix_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi1pix_clk.c),
	},
};

static struct branch_clk mmss_camss_csi1rdi_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi1rdi_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi1rdi_clk.c),
	},
};

static struct branch_clk mmss_camss_cphy_csid2_clk = {
	.cbcr_reg = MMSS_CAMSS_CPHY_CSID2_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cphy_csid2_clk",
		.parent = &csiphy_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cphy_csid2_clk.c),
	},
};

static struct branch_clk mmss_camss_csi2_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi2_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_csi2_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi2_clk",
		.parent = &csi2_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi2_clk.c),
	},
};

static struct branch_clk mmss_camss_csi2pix_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi2pix_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi2pix_clk.c),
	},
};

static struct branch_clk mmss_camss_csi2rdi_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi2rdi_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi2rdi_clk.c),
	},
};

static struct branch_clk mmss_camss_cphy_csid3_clk = {
	.cbcr_reg = MMSS_CAMSS_CPHY_CSID3_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_cphy_csid3_clk",
		.parent = &csiphy_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_cphy_csid3_clk.c),
	},
};

static struct branch_clk mmss_camss_csi3_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi3_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_csi3_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI3_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi3_clk",
		.parent = &csi3_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi3_clk.c),
	},
};

static struct branch_clk mmss_camss_csi3pix_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI3PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi3pix_clk",
		.parent = &csi3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi3pix_clk.c),
	},
};

static struct branch_clk mmss_camss_csi3rdi_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI3RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi3rdi_clk",
		.parent = &csi3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi3rdi_clk.c),
	},
};

static struct branch_clk mmss_camss_csi_vfe0_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI_VFE0_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi_vfe0_clk",
		.parent = &vfe0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi_vfe0_clk.c),
	},
};

static struct branch_clk mmss_camss_csi_vfe1_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI_VFE1_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi_vfe1_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi_vfe1_clk.c),
	},
};

static struct branch_clk mmss_camss_csiphy0_clk = {
	.cbcr_reg = MMSS_CAMSS_CSIPHY0_CBCR,
	.has_sibling = 0,
	.aggr_sibling_rates = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csiphy0_clk",
		.parent = &csiphy_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csiphy0_clk.c),
	},
};

static struct branch_clk mmss_camss_csiphy1_clk = {
	.cbcr_reg = MMSS_CAMSS_CSIPHY1_CBCR,
	.has_sibling = 0,
	.aggr_sibling_rates = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csiphy1_clk",
		.parent = &csiphy_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csiphy1_clk.c),
	},
};

static struct branch_clk mmss_camss_csiphy2_clk = {
	.cbcr_reg = MMSS_CAMSS_CSIPHY2_CBCR,
	.has_sibling = 0,
	.aggr_sibling_rates = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csiphy2_clk",
		.parent = &csiphy_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csiphy2_clk.c),
	},
};

static struct branch_clk mmss_fd_ahb_clk = {
	.cbcr_reg = MMSS_FD_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_fd_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_fd_ahb_clk.c),
	},
};

static struct branch_clk mmss_fd_core_clk = {
	.cbcr_reg = MMSS_FD_CORE_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_fd_core_clk",
		.parent = &fd_core_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_fd_core_clk.c),
	},
};

static struct branch_clk mmss_fd_core_uar_clk = {
	.cbcr_reg = MMSS_FD_CORE_UAR_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_fd_core_uar_clk",
		.parent = &fd_core_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_fd_core_uar_clk.c),
	},
};

static struct branch_clk mmss_camss_gp0_clk = {
	.cbcr_reg = MMSS_CAMSS_GP0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_gp0_clk",
		.parent = &camss_gp0_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_gp0_clk.c),
	},
};

static struct branch_clk mmss_camss_gp1_clk = {
	.cbcr_reg = MMSS_CAMSS_GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_gp1_clk",
		.parent = &camss_gp1_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_gp1_clk.c),
	},
};

static struct branch_clk mmss_camss_ispif_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_ISPIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_ispif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_ispif_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_jpeg0_clk = {
	.cbcr_reg = MMSS_CAMSS_JPEG0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_jpeg0_clk",
		.parent = &jpeg0_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_jpeg0_clk.c),
	},
};

static DEFINE_CLK_VOTER(mmss_camss_jpeg0_vote_clk, &mmss_camss_jpeg0_clk.c, 0);
static DEFINE_CLK_VOTER(mmss_camss_jpeg0_dma_vote_clk,
					&mmss_camss_jpeg0_clk.c, 0);

static struct branch_clk mmss_camss_jpeg_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_JPEG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_jpeg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_jpeg_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_jpeg_axi_clk = {
	.cbcr_reg = MMSS_CAMSS_JPEG_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_jpeg_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_jpeg_axi_clk.c),
	},
};

static struct branch_clk mmss_camss_mclk0_clk = {
	.cbcr_reg = MMSS_CAMSS_MCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_mclk0_clk",
		.parent = &mclk0_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_mclk0_clk.c),
	},
};

static struct branch_clk mmss_camss_mclk1_clk = {
	.cbcr_reg = MMSS_CAMSS_MCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_mclk1_clk",
		.parent = &mclk1_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_mclk1_clk.c),
	},
};

static struct branch_clk mmss_camss_mclk2_clk = {
	.cbcr_reg = MMSS_CAMSS_MCLK2_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_mclk2_clk",
		.parent = &mclk2_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_mclk2_clk.c),
	},
};

static struct branch_clk mmss_camss_mclk3_clk = {
	.cbcr_reg = MMSS_CAMSS_MCLK3_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_mclk3_clk",
		.parent = &mclk3_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_mclk3_clk.c),
	},
};

static struct branch_clk mmss_camss_micro_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_MICRO_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_micro_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_micro_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_csi0phytimer_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI0PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi0phytimer_clk",
		.parent = &csi0phytimer_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi0phytimer_clk.c),
	},
};

static struct branch_clk mmss_camss_csi1phytimer_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI1PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi1phytimer_clk",
		.parent = &csi1phytimer_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi1phytimer_clk.c),
	},
};

static struct branch_clk mmss_camss_csi2phytimer_clk = {
	.cbcr_reg = MMSS_CAMSS_CSI2PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_csi2phytimer_clk",
		.parent = &csi2phytimer_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_csi2phytimer_clk.c),
	},
};

static struct branch_clk mmss_camss_top_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_TOP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_top_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_top_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_vfe0_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_vfe0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_vfe0_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_vfe0_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_vfe0_clk",
		.parent = &vfe0_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_vfe0_clk.c),
	},
};

static struct branch_clk mmss_camss_vfe0_stream_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE0_STREAM_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_vfe0_stream_clk",
		.parent = &vfe0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_vfe0_stream_clk.c),
	},
};

static struct branch_clk mmss_camss_vfe1_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_vfe1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_vfe1_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_vfe1_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_vfe1_clk",
		.parent = &vfe1_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_vfe1_clk.c),
	},
};

static struct branch_clk mmss_camss_vfe1_stream_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE1_STREAM_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_vfe1_stream_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_vfe1_stream_clk.c),
	},
};

static struct branch_clk mmss_camss_vfe_vbif_ahb_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE_VBIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_vfe_vbif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_vfe_vbif_ahb_clk.c),
	},
};

static struct branch_clk mmss_camss_vfe_vbif_axi_clk = {
	.cbcr_reg = MMSS_CAMSS_VFE_VBIF_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_camss_vfe_vbif_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_camss_vfe_vbif_axi_clk.c),
	},
};

static struct branch_clk mmss_mdss_ahb_clk = {
	.cbcr_reg = MMSS_MDSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_ahb_clk.c),
	},
};

static struct branch_clk mmss_mdss_axi_clk = {
	.cbcr_reg = MMSS_MDSS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_axi_clk.c),
	},
};

static struct branch_clk mmss_mdss_byte0_clk = {
	.cbcr_reg = MMSS_MDSS_BYTE0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_byte0_clk",
		.parent = &byte0_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_byte0_clk.c),
	},
};

static struct div_clk mmss_mdss_byte0_intf_div_clk = {
	.offset = MMSS_MDSS_BYTE0_INTF_DIV,
	.mask = 0x3,
	.shift = 0,
	.data = {
		.min_div = 1,
		.max_div = 4,
	},
	.base = &virt_base,
	/*
	 * NOTE: Op does not work for div-3. Current assumption is that div-3
	 * is not a recommended setting for this divider.
	 */
	.ops = &postdiv_reg_ops,
	.c = {
		.dbg_name = "mmss_mdss_byte0_intf_div_clk",
		.parent = &byte0_clk_src.c,
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mmss_mdss_byte0_intf_div_clk.c),
	},
};

static struct branch_clk mmss_mdss_byte0_intf_clk = {
	.cbcr_reg = MMSS_MDSS_BYTE0_INTF_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_byte0_intf_clk",
		.parent = &mmss_mdss_byte0_intf_div_clk.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_byte0_intf_clk.c),
	},
};

static struct branch_clk mmss_mdss_byte1_clk = {
	.cbcr_reg = MMSS_MDSS_BYTE1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_byte1_clk",
		.parent = &byte1_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_byte1_clk.c),
	},
};

static struct div_clk mmss_mdss_byte1_intf_div_clk = {
	.offset = MMSS_MDSS_BYTE1_INTF_DIV,
	.mask = 0x3,
	.shift = 0,
	.data = {
		.min_div = 1,
		.max_div = 4,
	},
	.base = &virt_base,
	/*
	 * NOTE: Op does not work for div-3. Current assumption is that div-3
	 * is not a recommended setting for this divider.
	 */
	.ops = &postdiv_reg_ops,
	.c = {
		.dbg_name = "mmss_mdss_byte1_intf_div_clk",
		.parent = &byte1_clk_src.c,
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mmss_mdss_byte1_intf_div_clk.c),
	},
};

static struct branch_clk mmss_mdss_byte1_intf_clk = {
	.cbcr_reg = MMSS_MDSS_BYTE1_INTF_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_byte1_intf_clk",
		.parent = &mmss_mdss_byte1_intf_div_clk.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_byte1_intf_clk.c),
	},
};

static struct branch_clk mmss_mdss_dp_aux_clk = {
	.cbcr_reg = MMSS_MDSS_DP_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_dp_aux_clk",
		.parent = &dp_aux_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_dp_aux_clk.c),
	},
};

static struct branch_clk mmss_mdss_dp_pixel_clk = {
	.cbcr_reg = MMSS_MDSS_DP_PIXEL_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_dp_pixel_clk",
		.parent = &dp_pixel_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_dp_pixel_clk.c),
	},
};

static struct branch_clk mmss_mdss_dp_link_clk = {
	.cbcr_reg = MMSS_MDSS_DP_LINK_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_dp_link_clk",
		.parent = &dp_link_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_dp_link_clk.c),
	},
};

/* Reset state of MMSS_MDSS_DP_LINK_INTF_DIV is 0x3 (div-4) */
static struct branch_clk mmss_mdss_dp_link_intf_clk = {
	.cbcr_reg = MMSS_MDSS_DP_LINK_INTF_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_dp_link_intf_clk",
		.parent = &dp_link_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_dp_link_intf_clk.c),
	},
};

static struct branch_clk mmss_mdss_dp_crypto_clk = {
	.cbcr_reg = MMSS_MDSS_DP_CRYPTO_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_dp_crypto_clk",
		.parent = &dp_crypto_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_dp_crypto_clk.c),
	},
};

static struct branch_clk mmss_mdss_dp_gtc_clk = {
	.cbcr_reg = MMSS_MDSS_DP_GTC_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_dp_gtc_clk",
		.parent = &dp_gtc_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_dp_gtc_clk.c),
	},
};

static struct branch_clk mmss_mdss_esc0_clk = {
	.cbcr_reg = MMSS_MDSS_ESC0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_esc0_clk",
		.parent = &esc0_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_esc0_clk.c),
	},
};

static struct branch_clk mmss_mdss_esc1_clk = {
	.cbcr_reg = MMSS_MDSS_ESC1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_esc1_clk",
		.parent = &esc1_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_esc1_clk.c),
	},
};

static struct branch_clk mmss_mdss_extpclk_clk = {
	.cbcr_reg = MMSS_MDSS_EXTPCLK_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_extpclk_clk",
		.parent = &extpclk_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_extpclk_clk.c),
	},
};

static struct branch_clk mmss_mdss_hdmi_clk = {
	.cbcr_reg = MMSS_MDSS_HDMI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_hdmi_clk",
		.parent = &hdmi_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_hdmi_clk.c),
	},
};

static struct branch_clk mmss_mdss_hdmi_dp_ahb_clk = {
	.cbcr_reg = MMSS_MDSS_HDMI_DP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_hdmi_dp_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_hdmi_dp_ahb_clk.c),
	},
};

static struct branch_clk mmss_mdss_mdp_clk = {
	.cbcr_reg = MMSS_MDSS_MDP_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_mdp_clk",
		.parent = &mdp_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_mdp_clk.c),
	},
};

static struct branch_clk mmss_mdss_mdp_lut_clk = {
	.cbcr_reg = MMSS_MDSS_MDP_LUT_CBCR,
	.has_sibling = 1,
	.check_enable_bit = true,
	.halt_check = DELAY,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_mdp_lut_clk",
		.parent = &mdp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_mdp_lut_clk.c),
	},
};

static struct branch_clk mmss_mdss_pclk0_clk = {
	.cbcr_reg = MMSS_MDSS_PCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_pclk0_clk",
		.parent = &pclk0_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_pclk0_clk.c),
	},
};

static struct branch_clk mmss_mdss_pclk1_clk = {
	.cbcr_reg = MMSS_MDSS_PCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_pclk1_clk",
		.parent = &pclk1_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_pclk1_clk.c),
	},
};

static struct branch_clk mmss_mdss_rot_clk = {
	.cbcr_reg = MMSS_MDSS_ROT_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_rot_clk",
		.parent = &rot_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_rot_clk.c),
	},
};

static struct branch_clk mmss_mdss_vsync_clk = {
	.cbcr_reg = MMSS_MDSS_VSYNC_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mdss_vsync_clk",
		.parent = &vsync_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mdss_vsync_clk.c),
	},
};

static struct branch_clk mmss_mnoc_ahb_clk = {
	.cbcr_reg = MMSS_MNOC_AHB_CBCR,
	.has_sibling = 0,
	.check_enable_bit = true,
	.no_halt_check_on_disable = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mnoc_ahb_clk",
		.parent = &ahb_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mnoc_ahb_clk.c),
	},
};

static struct branch_clk mmss_misc_ahb_clk = {
	.cbcr_reg = MMSS_MISC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_misc_ahb_clk",
		.ops = &clk_ops_branch,
		.depends = &mmss_mnoc_ahb_clk.c,
		CLK_INIT(mmss_misc_ahb_clk.c),
	},
};

static struct branch_clk mmss_misc_cxo_clk = {
	.cbcr_reg = MMSS_MISC_CXO_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_misc_cxo_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_misc_cxo_clk.c),
	},
};

static struct branch_clk mmss_mnoc_maxi_clk = {
	.cbcr_reg = MMSS_MNOC_MAXI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mnoc_maxi_clk",
		.parent = &maxi_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mnoc_maxi_clk.c),
	},
};

static struct branch_clk mmss_video_subcore0_clk = {
	.cbcr_reg = MMSS_VIDEO_SUBCORE0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_video_subcore0_clk",
		.parent = &video_subcore0_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_video_subcore0_clk.c),
	},
};

static struct branch_clk mmss_video_subcore1_clk = {
	.cbcr_reg = MMSS_VIDEO_SUBCORE1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_video_subcore1_clk",
		.parent = &video_subcore1_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_video_subcore1_clk.c),
	},
};

static struct branch_clk mmss_video_ahb_clk = {
	.cbcr_reg = MMSS_VIDEO_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_video_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_video_ahb_clk.c),
	},
};

static struct branch_clk mmss_video_axi_clk = {
	.cbcr_reg = MMSS_VIDEO_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_video_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_video_axi_clk.c),
	},
};

static struct branch_clk mmss_video_core_clk = {
	.cbcr_reg = MMSS_VIDEO_CORE_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_video_core_clk",
		.parent = &video_core_clk_src.c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_video_core_clk.c),
	},
};

static struct branch_clk mmss_video_maxi_clk = {
	.cbcr_reg = MMSS_VIDEO_MAXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_video_maxi_clk",
		.parent = &maxi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_video_maxi_clk.c),
	},
};

static struct branch_clk mmss_vmem_ahb_clk = {
	.cbcr_reg = MMSS_VMEM_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_vmem_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_vmem_ahb_clk.c),
	},
};

static struct branch_clk mmss_vmem_maxi_clk = {
	.cbcr_reg = MMSS_VMEM_MAXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_vmem_maxi_clk",
		.parent = &maxi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_vmem_maxi_clk.c),
	},
};

static struct mux_clk mmss_debug_mux = {
	.ops = &mux_reg_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.offset = MMSS_DEBUG_CLK_CTL,
	.en_offset = MMSS_DEBUG_CLK_CTL,
	.base = &virt_base,
	MUX_SRC_LIST(
		{ &mmss_mnoc_ahb_clk.c, 0x0001 },
		{ &mmss_misc_ahb_clk.c, 0x0003 },
		{ &mmss_vmem_maxi_clk.c, 0x0009 },
		{ &mmss_vmem_ahb_clk.c, 0x000a },
		{ &mmss_bimc_smmu_ahb_clk.c, 0x000c },
		{ &mmss_bimc_smmu_axi_clk.c, 0x000d },
		{ &mmss_video_core_clk.c, 0x000e },
		{ &mmss_video_axi_clk.c, 0x000f },
		{ &mmss_video_maxi_clk.c, 0x0010 },
		{ &mmss_video_ahb_clk.c, 0x0011 },
		{ &mmss_mdss_rot_clk.c, 0x0012 },
		{ &mmss_snoc_dvm_axi_clk.c, 0x0013 },
		{ &mmss_mdss_mdp_clk.c, 0x0014 },
		{ &mmss_mdss_mdp_lut_clk.c, 0x0015 },
		{ &mmss_mdss_pclk0_clk.c, 0x0016 },
		{ &mmss_mdss_pclk1_clk.c, 0x0017 },
		{ &mmss_mdss_extpclk_clk.c, 0x0018 },
		{ &mmss_video_subcore0_clk.c, 0x001a },
		{ &mmss_video_subcore1_clk.c, 0x001b },
		{ &mmss_mdss_vsync_clk.c, 0x001c },
		{ &mmss_mdss_hdmi_clk.c, 0x001d },
		{ &mmss_mdss_byte0_clk.c, 0x001e },
		{ &mmss_mdss_byte1_clk.c, 0x001f },
		{ &mmss_mdss_esc0_clk.c, 0x0020 },
		{ &mmss_mdss_esc1_clk.c, 0x0021 },
		{ &mmss_mdss_ahb_clk.c, 0x0022 },
		{ &mmss_mdss_hdmi_dp_ahb_clk.c, 0x0023 },
		{ &mmss_mdss_axi_clk.c, 0x0024 },
		{ &mmss_camss_top_ahb_clk.c, 0x0025 },
		{ &mmss_camss_micro_ahb_clk.c, 0x0026 },
		{ &mmss_camss_gp0_clk.c, 0x0027 },
		{ &mmss_camss_gp1_clk.c, 0x0028 },
		{ &mmss_camss_mclk0_clk.c, 0x0029 },
		{ &mmss_camss_mclk1_clk.c, 0x002a },
		{ &mmss_camss_mclk2_clk.c, 0x002b },
		{ &mmss_camss_mclk3_clk.c, 0x002c },
		{ &mmss_camss_cci_clk.c, 0x002d },
		{ &mmss_camss_cci_ahb_clk.c, 0x002e },
		{ &mmss_camss_csi0phytimer_clk.c, 0x002f },
		{ &mmss_camss_csi1phytimer_clk.c, 0x0030 },
		{ &mmss_camss_csi2phytimer_clk.c, 0x0031 },
		{ &mmss_camss_jpeg0_clk.c, 0x0032 },
		{ &mmss_camss_ispif_ahb_clk.c, 0x0033 },
		{ &mmss_camss_jpeg_ahb_clk.c, 0x0035 },
		{ &mmss_camss_jpeg_axi_clk.c, 0x0036 },
		{ &mmss_camss_ahb_clk.c, 0x0037 },
		{ &mmss_camss_vfe0_clk.c, 0x0038 },
		{ &mmss_camss_vfe1_clk.c, 0x0039 },
		{ &mmss_camss_cpp_clk.c, 0x003a },
		{ &mmss_camss_cpp_ahb_clk.c, 0x003b },
		{ &mmss_camss_csi_vfe0_clk.c, 0x003f },
		{ &mmss_camss_csi_vfe1_clk.c, 0x0040 },
		{ &mmss_camss_csi0_clk.c, 0x0041 },
		{ &mmss_camss_csi0_ahb_clk.c, 0x0042 },
		{ &mmss_camss_csiphy0_clk.c, 0x0043 },
		{ &mmss_camss_csi0rdi_clk.c, 0x0044 },
		{ &mmss_camss_csi0pix_clk.c, 0x0045 },
		{ &mmss_camss_csi1_clk.c, 0x0046 },
		{ &mmss_camss_csi1_ahb_clk.c, 0x0047 },
		{ &mmss_camss_csi1rdi_clk.c, 0x0049 },
		{ &mmss_camss_csi1pix_clk.c, 0x004a },
		{ &mmss_camss_csi2_clk.c, 0x004b },
		{ &mmss_camss_csi2_ahb_clk.c, 0x004c },
		{ &mmss_camss_csi2rdi_clk.c, 0x004e },
		{ &mmss_camss_csi2pix_clk.c, 0x004f },
		{ &mmss_camss_csi3_clk.c, 0x0050 },
		{ &mmss_camss_csi3_ahb_clk.c, 0x0051 },
		{ &mmss_camss_csi3rdi_clk.c, 0x0053 },
		{ &mmss_camss_csi3pix_clk.c, 0x0054 },
		{ &mmss_mnoc_maxi_clk.c, 0x0070 },
		{ &mmss_camss_vfe0_stream_clk.c, 0x0071 },
		{ &mmss_camss_vfe1_stream_clk.c, 0x0072 },
		{ &mmss_camss_cpp_vbif_ahb_clk.c, 0x0073 },
		{ &mmss_misc_cxo_clk.c, 0x0077 },
		{ &mmss_camss_cpp_axi_clk.c, 0x007a },
		{ &mmss_camss_csiphy1_clk.c, 0x0085 },
		{ &mmss_camss_vfe0_ahb_clk.c, 0x0086 },
		{ &mmss_camss_vfe1_ahb_clk.c, 0x0087 },
		{ &mmss_camss_csiphy2_clk.c, 0x0088 },
		{ &mmss_fd_core_clk.c, 0x0089 },
		{ &mmss_fd_core_uar_clk.c, 0x008a },
		{ &mmss_fd_ahb_clk.c, 0x008c },
		{ &mmss_camss_cphy_csid0_clk.c, 0x008d },
		{ &mmss_camss_cphy_csid1_clk.c, 0x008e },
		{ &mmss_camss_cphy_csid2_clk.c, 0x008f },
		{ &mmss_camss_cphy_csid3_clk.c, 0x0090 },
		{ &mmss_mdss_dp_link_clk.c, 0x0098 },
		{ &mmss_mdss_dp_link_intf_clk.c, 0x0099 },
		{ &mmss_mdss_dp_crypto_clk.c, 0x009a },
		{ &mmss_mdss_dp_pixel_clk.c, 0x009b },
		{ &mmss_mdss_dp_aux_clk.c, 0x009c },
		{ &mmss_mdss_dp_gtc_clk.c, 0x009d },
		{ &mmss_mdss_byte0_intf_clk.c, 0x00ad },
		{ &mmss_mdss_byte1_intf_clk.c, 0x00ae },
	),
	.c = {
		.dbg_name = "mmss_debug_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mmss_debug_mux.c),
	},
};

static struct clk_lookup msm_clocks_mmss_8998[] = {
	CLK_LIST(mmsscc_xo),
	CLK_LIST(mmsscc_gpll0),
	CLK_LIST(mmsscc_gpll0_div),
	CLK_LIST(mmpll0_pll),
	CLK_LIST(mmpll0_pll_out),
	CLK_LIST(mmpll1_pll),
	CLK_LIST(mmpll1_pll_out),
	CLK_LIST(mmpll3_pll),
	CLK_LIST(mmpll3_pll_out),
	CLK_LIST(mmpll4_pll),
	CLK_LIST(mmpll4_pll_out),
	CLK_LIST(mmpll5_pll),
	CLK_LIST(mmpll5_pll_out),
	CLK_LIST(mmpll6_pll),
	CLK_LIST(mmpll6_pll_out),
	CLK_LIST(mmpll7_pll),
	CLK_LIST(mmpll7_pll_out),
	CLK_LIST(mmpll10_pll),
	CLK_LIST(mmpll10_pll_out),
	CLK_LIST(ahb_clk_src),
	CLK_LIST(csi0_clk_src),
	CLK_LIST(vfe0_clk_src),
	CLK_LIST(vfe1_clk_src),
	CLK_LIST(mdp_clk_src),
	CLK_LIST(maxi_clk_src),
	CLK_LIST(cpp_clk_src),
	CLK_LIST(jpeg0_clk_src),
	CLK_LIST(rot_clk_src),
	CLK_LIST(video_core_clk_src),
	CLK_LIST(csi1_clk_src),
	CLK_LIST(csi2_clk_src),
	CLK_LIST(csi3_clk_src),
	CLK_LIST(fd_core_clk_src),
	CLK_LIST(video_subcore0_clk_src),
	CLK_LIST(video_subcore1_clk_src),
	CLK_LIST(cci_clk_src),
	CLK_LIST(csiphy_clk_src),
	CLK_LIST(camss_gp0_clk_src),
	CLK_LIST(camss_gp1_clk_src),
	CLK_LIST(mclk0_clk_src),
	CLK_LIST(mclk1_clk_src),
	CLK_LIST(mclk2_clk_src),
	CLK_LIST(mclk3_clk_src),
	CLK_LIST(ext_byte0_clk_src),
	CLK_LIST(ext_byte1_clk_src),
	CLK_LIST(byte0_clk_src),
	CLK_LIST(byte1_clk_src),
	CLK_LIST(ext_pclk0_clk_src),
	CLK_LIST(ext_pclk1_clk_src),
	CLK_LIST(pclk0_clk_src),
	CLK_LIST(pclk1_clk_src),
	CLK_LIST(ext_extpclk_clk_src),
	CLK_LIST(extpclk_clk_src),
	CLK_LIST(ext_dp_phy_pll_vco),
	CLK_LIST(ext_dp_phy_pll_link),
	CLK_LIST(dp_pixel_clk_src),
	CLK_LIST(dp_link_clk_src),
	CLK_LIST(dp_crypto_clk_src),
	CLK_LIST(csi0phytimer_clk_src),
	CLK_LIST(csi1phytimer_clk_src),
	CLK_LIST(csi2phytimer_clk_src),
	CLK_LIST(dp_aux_clk_src),
	CLK_LIST(dp_gtc_clk_src),
	CLK_LIST(esc0_clk_src),
	CLK_LIST(esc1_clk_src),
	CLK_LIST(hdmi_clk_src),
	CLK_LIST(vsync_clk_src),
	CLK_LIST(mmss_bimc_smmu_ahb_clk),
	CLK_LIST(mmss_bimc_smmu_axi_clk),
	CLK_LIST(mmss_snoc_dvm_axi_clk),
	CLK_LIST(mmss_camss_ahb_clk),
	CLK_LIST(mmss_camss_cci_ahb_clk),
	CLK_LIST(mmss_camss_cci_clk),
	CLK_LIST(mmss_camss_cpp_ahb_clk),
	CLK_LIST(mmss_camss_cpp_clk),
	CLK_LIST(mmss_camss_cpp_axi_clk),
	CLK_LIST(mmss_camss_cpp_vbif_ahb_clk),
	CLK_LIST(mmss_camss_cphy_csid0_clk),
	CLK_LIST(mmss_camss_csi0_ahb_clk),
	CLK_LIST(mmss_camss_csi0_clk),
	CLK_LIST(mmss_camss_csi0pix_clk),
	CLK_LIST(mmss_camss_csi0rdi_clk),
	CLK_LIST(mmss_camss_cphy_csid1_clk),
	CLK_LIST(mmss_camss_csi1_ahb_clk),
	CLK_LIST(mmss_camss_csi1_clk),
	CLK_LIST(mmss_camss_csi1pix_clk),
	CLK_LIST(mmss_camss_csi1rdi_clk),
	CLK_LIST(mmss_camss_cphy_csid2_clk),
	CLK_LIST(mmss_camss_csi2_ahb_clk),
	CLK_LIST(mmss_camss_csi2_clk),
	CLK_LIST(mmss_camss_csi2pix_clk),
	CLK_LIST(mmss_camss_csi2rdi_clk),
	CLK_LIST(mmss_camss_cphy_csid3_clk),
	CLK_LIST(mmss_camss_csi3_ahb_clk),
	CLK_LIST(mmss_camss_csi3_clk),
	CLK_LIST(mmss_camss_csi3pix_clk),
	CLK_LIST(mmss_camss_csi3rdi_clk),
	CLK_LIST(mmss_camss_csi_vfe0_clk),
	CLK_LIST(mmss_camss_csi_vfe1_clk),
	CLK_LIST(mmss_camss_csiphy0_clk),
	CLK_LIST(mmss_camss_csiphy1_clk),
	CLK_LIST(mmss_camss_csiphy2_clk),
	CLK_LIST(mmss_fd_ahb_clk),
	CLK_LIST(mmss_fd_core_clk),
	CLK_LIST(mmss_fd_core_uar_clk),
	CLK_LIST(mmss_camss_gp0_clk),
	CLK_LIST(mmss_camss_gp1_clk),
	CLK_LIST(mmss_camss_ispif_ahb_clk),
	CLK_LIST(mmss_camss_jpeg0_clk),
	CLK_LIST(mmss_camss_jpeg0_vote_clk),
	CLK_LIST(mmss_camss_jpeg0_dma_vote_clk),
	CLK_LIST(mmss_camss_jpeg_ahb_clk),
	CLK_LIST(mmss_camss_jpeg_axi_clk),
	CLK_LIST(mmss_camss_mclk0_clk),
	CLK_LIST(mmss_camss_mclk1_clk),
	CLK_LIST(mmss_camss_mclk2_clk),
	CLK_LIST(mmss_camss_mclk3_clk),
	CLK_LIST(mmss_camss_micro_ahb_clk),
	CLK_LIST(mmss_camss_csi0phytimer_clk),
	CLK_LIST(mmss_camss_csi1phytimer_clk),
	CLK_LIST(mmss_camss_csi2phytimer_clk),
	CLK_LIST(mmss_camss_top_ahb_clk),
	CLK_LIST(mmss_camss_vfe0_ahb_clk),
	CLK_LIST(mmss_camss_vfe0_clk),
	CLK_LIST(mmss_camss_vfe0_stream_clk),
	CLK_LIST(mmss_camss_vfe1_ahb_clk),
	CLK_LIST(mmss_camss_vfe1_clk),
	CLK_LIST(mmss_camss_vfe1_stream_clk),
	CLK_LIST(mmss_camss_vfe_vbif_ahb_clk),
	CLK_LIST(mmss_camss_vfe_vbif_axi_clk),
	CLK_LIST(mmss_mdss_ahb_clk),
	CLK_LIST(mmss_mdss_axi_clk),
	CLK_LIST(mmss_mdss_byte0_clk),
	CLK_LIST(mmss_mdss_byte0_intf_div_clk),
	CLK_LIST(mmss_mdss_byte0_intf_clk),
	CLK_LIST(mmss_mdss_byte1_clk),
	CLK_LIST(mmss_mdss_byte0_intf_div_clk),
	CLK_LIST(mmss_mdss_byte1_intf_clk),
	CLK_LIST(mmss_mdss_dp_aux_clk),
	CLK_LIST(mmss_mdss_dp_crypto_clk),
	CLK_LIST(mmss_mdss_dp_pixel_clk),
	CLK_LIST(mmss_mdss_dp_link_clk),
	CLK_LIST(mmss_mdss_dp_link_intf_clk),
	CLK_LIST(mmss_mdss_dp_gtc_clk),
	CLK_LIST(mmss_mdss_esc0_clk),
	CLK_LIST(mmss_mdss_esc1_clk),
	CLK_LIST(mmss_mdss_extpclk_clk),
	CLK_LIST(mmss_mdss_hdmi_clk),
	CLK_LIST(mmss_mdss_hdmi_dp_ahb_clk),
	CLK_LIST(mmss_mdss_mdp_clk),
	CLK_LIST(mmss_mdss_mdp_lut_clk),
	CLK_LIST(mmss_mdss_pclk0_clk),
	CLK_LIST(mmss_mdss_pclk1_clk),
	CLK_LIST(mmss_mdss_rot_clk),
	CLK_LIST(mmss_mdss_vsync_clk),
	CLK_LIST(mmss_misc_ahb_clk),
	CLK_LIST(mmss_misc_cxo_clk),
	CLK_LIST(mmss_mnoc_ahb_clk),
	CLK_LIST(mmss_video_subcore0_clk),
	CLK_LIST(mmss_video_subcore1_clk),
	CLK_LIST(mmss_video_ahb_clk),
	CLK_LIST(mmss_video_axi_clk),
	CLK_LIST(mmss_video_core_clk),
	CLK_LIST(mmss_video_maxi_clk),
	CLK_LIST(mmss_vmem_ahb_clk),
	CLK_LIST(mmss_vmem_maxi_clk),
	CLK_LIST(mmss_mnoc_maxi_clk),
	CLK_LIST(mmss_debug_mux),
};

static const struct msm_reset_map mmss_8998_resets[] = {
	[CAMSS_MICRO_BCR] = { 0x3490 },
};

static void msm_mmsscc_hamster_fixup(void)
{
	mmpll3_pll.c.rate = 1066000000;
	mmpll3_pll.c.fmax[VDD_DIG_LOWER] = 533000000;
	mmpll3_pll.c.fmax[VDD_DIG_LOW] = 533000000;
	mmpll3_pll.c.fmax[VDD_DIG_LOW_L1] = 533000000;
	mmpll3_pll.c.fmax[VDD_DIG_NOMINAL] = 1066000000;
	mmpll3_pll.c.fmax[VDD_DIG_HIGH] = 1066000000;

	mmpll4_pll.c.fmax[VDD_DIG_LOW] = 384000000;
	mmpll4_pll.c.fmax[VDD_DIG_LOW_L1] = 384000000;
	mmpll4_pll.c.fmax[VDD_DIG_NOMINAL] = 768000000;

	mmpll5_pll.c.fmax[VDD_DIG_LOW] = 412500000;
	mmpll5_pll.c.fmax[VDD_DIG_LOW_L1] = 412500000;
	mmpll5_pll.c.fmax[VDD_DIG_NOMINAL] = 825000000;

	mmpll6_pll.c.rate = 888000000;
	mmpll6_pll.c.fmax[VDD_DIG_LOWER] = 444000000;
	mmpll6_pll.c.fmax[VDD_DIG_LOW] = 444000000;
	mmpll6_pll.c.fmax[VDD_DIG_LOW_L1] = 444000000;
	mmpll6_pll.c.fmax[VDD_DIG_NOMINAL] = 888000000;
	mmpll6_pll.c.fmax[VDD_DIG_HIGH] = 888000000;

	vfe0_clk_src.freq_tbl = ftbl_vfe_clk_src_vq;
	vfe0_clk_src.c.fmax[VDD_DIG_LOW] = 404000000;
	vfe0_clk_src.c.fmax[VDD_DIG_LOW_L1] = 480000000;
	vfe1_clk_src.freq_tbl = ftbl_vfe_clk_src_vq;
	vfe1_clk_src.c.fmax[VDD_DIG_LOW] = 404000000;
	vfe1_clk_src.c.fmax[VDD_DIG_LOW_L1] = 480000000;

	csi0_clk_src.freq_tbl = ftbl_csi_clk_src_vq;
	csi0_clk_src.c.fmax[VDD_DIG_LOW] = 274290000;
	csi0_clk_src.c.fmax[VDD_DIG_LOW_L1] = 320000000;
	csi1_clk_src.freq_tbl = ftbl_csi_clk_src_vq;
	csi1_clk_src.c.fmax[VDD_DIG_LOW] = 274290000;
	csi1_clk_src.c.fmax[VDD_DIG_LOW_L1] = 320000000;
	csi2_clk_src.freq_tbl = ftbl_csi_clk_src_vq;
	csi2_clk_src.c.fmax[VDD_DIG_LOW] = 274290000;
	csi2_clk_src.c.fmax[VDD_DIG_LOW_L1] = 320000000;
	csi3_clk_src.freq_tbl = ftbl_csi_clk_src_vq;
	csi3_clk_src.c.fmax[VDD_DIG_LOW] = 274290000;
	csi3_clk_src.c.fmax[VDD_DIG_LOW_L1] = 320000000;

	cpp_clk_src.freq_tbl = ftbl_cpp_clk_src_vq;
	cpp_clk_src.c.fmax[VDD_DIG_LOW] = 384000000;
	cpp_clk_src.c.fmax[VDD_DIG_LOW_L1] = 404000000;
	jpeg0_clk_src.freq_tbl = ftbl_jpeg0_clk_src_vq;
	jpeg0_clk_src.c.fmax[VDD_DIG_LOW_L1] = 320000000;
	csiphy_clk_src.freq_tbl = ftbl_csiphy_clk_src_vq;
	csiphy_clk_src.c.fmax[VDD_DIG_LOW] = 274290000;
	csiphy_clk_src.c.fmax[VDD_DIG_LOW_L1] = 300000000;
	fd_core_clk_src.freq_tbl = ftbl_fd_core_clk_src_vq;
	fd_core_clk_src.c.fmax[VDD_DIG_LOW] = 404000000;
	fd_core_clk_src.c.fmax[VDD_DIG_LOW_L1] = 480000000;

	csi0phytimer_clk_src.c.fmax[VDD_DIG_LOW_L1] = 269333333;
	csi1phytimer_clk_src.c.fmax[VDD_DIG_LOW_L1] = 269333333;
	csi2phytimer_clk_src.c.fmax[VDD_DIG_LOW_L1] = 269333333;

	mdp_clk_src.c.fmax[VDD_DIG_LOW_L1] = 330000000;
	extpclk_clk_src.c.fmax[VDD_DIG_LOW] = 312500000;
	extpclk_clk_src.c.fmax[VDD_DIG_LOW_L1] = 375000000;
	rot_clk_src.c.fmax[VDD_DIG_LOW_L1] = 330000000;

	video_core_clk_src.freq_tbl = ftbl_video_core_clk_src_vq;
	video_core_clk_src.c.fmax[VDD_DIG_LOWER] = 200000000;
	video_core_clk_src.c.fmax[VDD_DIG_LOW] = 269330000;
	video_core_clk_src.c.fmax[VDD_DIG_LOW_L1] = 355200000;
	video_core_clk_src.c.fmax[VDD_DIG_NOMINAL] = 444000000;
	video_core_clk_src.c.fmax[VDD_DIG_HIGH] = 533000000;

	video_subcore0_clk_src.freq_tbl = ftbl_video_subcore_clk_src_vq;
	video_subcore0_clk_src.c.fmax[VDD_DIG_LOWER] = 200000000;
	video_subcore0_clk_src.c.fmax[VDD_DIG_LOW] = 269330000;
	video_subcore0_clk_src.c.fmax[VDD_DIG_LOW_L1] = 355200000;
	video_subcore0_clk_src.c.fmax[VDD_DIG_NOMINAL] = 444000000;
	video_subcore0_clk_src.c.fmax[VDD_DIG_HIGH] = 533000000;

	video_subcore1_clk_src.freq_tbl = ftbl_video_subcore_clk_src_vq;
	video_subcore1_clk_src.c.fmax[VDD_DIG_LOWER] = 200000000;
	video_subcore1_clk_src.c.fmax[VDD_DIG_LOW] = 269330000;
	video_subcore1_clk_src.c.fmax[VDD_DIG_LOW_L1] = 355200000;
	video_subcore1_clk_src.c.fmax[VDD_DIG_NOMINAL] = 444000000;
	video_subcore1_clk_src.c.fmax[VDD_DIG_HIGH] = 533000000;
};

static void msm_mmsscc_v2_fixup(void)
{
	csi0_clk_src.c.fmax[VDD_DIG_NOMINAL] = 480000000;
	csi1_clk_src.c.fmax[VDD_DIG_NOMINAL] = 480000000;
	csi2_clk_src.c.fmax[VDD_DIG_NOMINAL] = 480000000;
	csi3_clk_src.c.fmax[VDD_DIG_NOMINAL] = 480000000;
}

int msm_mmsscc_8998_probe(struct platform_device *pdev)
{
	struct resource *res;
	int rc;
	struct clk *tmp;
	struct regulator *reg;
	u32 regval;
	bool is_v2 = 0, is_vq = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Unable to retrieve register base.\n");
		return -ENOMEM;
	}
	virt_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_base) {
		dev_err(&pdev->dev, "Failed to map CC registers\n");
		return -ENOMEM;
	}

	/* Clear the DBG_CLK_DIV bits of the MMSS debug register */
	regval = readl_relaxed(virt_base + mmss_debug_mux.offset);
	regval &= ~BM(18, 17);
	writel_relaxed(regval, virt_base + mmss_debug_mux.offset);

	reg = vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_dig regulator!");
		return PTR_ERR(reg);
	}

	reg = vdd_mmsscc_mx.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd_mmsscc_mx");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_mmsscc_mx regulator!");
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

	ext_dp_phy_pll_link.dev = &pdev->dev;
	ext_dp_phy_pll_link.clk_id = "dp_link_src";
	ext_dp_phy_pll_link.c.flags = CLKFLAG_NO_RATE_CACHE;
	ext_dp_phy_pll_vco.dev = &pdev->dev;
	ext_dp_phy_pll_vco.clk_id = "dp_vco_div";
	ext_dp_phy_pll_vco.c.flags = CLKFLAG_NO_RATE_CACHE;

	mmss_camss_jpeg0_vote_clk.c.flags = CLKFLAG_NO_RATE_CACHE;
	mmss_camss_jpeg0_dma_vote_clk.c.flags = CLKFLAG_NO_RATE_CACHE;

	is_vq = of_device_is_compatible(pdev->dev.of_node,
					"qcom,mmsscc-hamster");
	if (is_vq)
		msm_mmsscc_hamster_fixup();

	is_v2 = of_device_is_compatible(pdev->dev.of_node,
					"qcom,mmsscc-8998-v2");
	if (is_v2) {
		msm_mmsscc_hamster_fixup();
		msm_mmsscc_v2_fixup();
	}

	rc = of_msm_clock_register(pdev->dev.of_node, msm_clocks_mmss_8998,
				   ARRAY_SIZE(msm_clocks_mmss_8998));
	if (rc)
		return rc;

	/* Register block resets */
	msm_reset_controller_register(pdev, mmss_8998_resets,
			ARRAY_SIZE(mmss_8998_resets), virt_base);

	dev_info(&pdev->dev, "Registered MMSS clocks.\n");
	return 0;
}

static struct of_device_id msm_clock_mmss_match_table[] = {
	{ .compatible = "qcom,mmsscc-8998" },
	{ .compatible = "qcom,mmsscc-8998-v2" },
	{ .compatible = "qcom,mmsscc-hamster" },
	{},
};

static struct platform_driver msm_clock_mmss_driver = {
	.probe = msm_mmsscc_8998_probe,
	.driver = {
		.name = "qcom,mmsscc-8998",
		.of_match_table = msm_clock_mmss_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_mmsscc_8998_init(void)
{
	return platform_driver_register(&msm_clock_mmss_driver);
}
arch_initcall(msm_mmsscc_8998_init);

/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <dt-bindings/clock/msm-clocks-8992.h>
#include <soc/qcom/clock-alpha-pll.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-voter.h>

#include "vdd-level-8994.h"

static void __iomem *virt_base;

#define MMSS_REG_BASE(x) (void __iomem *)(virt_base + (x))

#define mmsscc_xo_mm_source_val 0
#define mmpll0_out_main_mm_source_val 1
#define mmpll1_out_main_mm_source_val 2
#define mmpll3_out_main_mm_source_val 3
#define mmpll4_out_main_mm_source_val 3
#define mmpll5_out_main_mm_source_val 6
#define mmsscc_gpll0_mm_source_val 5
#define dsi0phypll_mm_source_val 1
#define dsi1phypll_mm_source_val 2
#define hdmiphypll_mm_source_val 3

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

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

static int vdd_mmpll4_levels[] = {
	RPM_REGULATOR_CORNER_NONE,		      0,
	RPM_REGULATOR_CORNER_SVS_SOC,		1800000,
	RPM_REGULATOR_CORNER_SVS_SOC,		1800000,
	RPM_REGULATOR_CORNER_NORMAL,		1800000,
	RPM_REGULATOR_CORNER_SUPER_TURBO,	1800000,
};

static DEFINE_VDD_REGULATORS(vdd_mmpll4, VDD_DIG_NUM, 2, vdd_mmpll4_levels,
			     NULL);

#define GP1_CBCR                                         (0x1900)
#define GP1_CMD_RCGR                                     (0x1904)
#define MMPLL0_MODE                                      (0x0000)
#define MMPLL1_MODE                                      (0x0030)
#define MMPLL3_MODE                                      (0x0060)
#define MMPLL4_MODE                                      (0x0090)
#define MMPLL5_MODE                                      (0x00C0)
#define MMSS_PLL_VOTE_APCS                               (0x0100)
#define VCODEC0_CMD_RCGR                                 (0x1000)
#define VENUS0_VCODEC0_CBCR                              (0x1028)
#define VENUS0_CORE0_VCODEC_CBCR                         (0x1048)
#define VENUS0_CORE1_VCODEC_CBCR                         (0x104C)
#define VENUS0_AHB_CBCR                                  (0x1030)
#define VENUS0_AXI_CBCR                                  (0x1034)
#define VENUS0_OCMEMNOC_CBCR                             (0x1038)
#define PCLK0_CMD_RCGR                                   (0x2000)
#define PCLK1_CMD_RCGR                                   (0x2020)
#define MDP_CMD_RCGR                                     (0x2040)
#define EXTPCLK_CMD_RCGR                                 (0x2060)
#define VSYNC_CMD_RCGR                                   (0x2080)
#define HDMI_CMD_RCGR                                    (0x2100)
#define BYTE0_CMD_RCGR                                   (0x2120)
#define BYTE1_CMD_RCGR                                   (0x2140)
#define ESC0_CMD_RCGR                                    (0x2160)
#define ESC1_CMD_RCGR                                    (0x2180)
#define MDSS_AHB_CBCR                                    (0x2308)
#define MDSS_HDMI_AHB_CBCR                               (0x230C)
#define MDSS_AXI_CBCR                                    (0x2310)
#define MDSS_PCLK0_CBCR                                  (0x2314)
#define MDSS_PCLK1_CBCR                                  (0x2318)
#define MDSS_MDP_CBCR                                    (0x231C)
#define MDSS_EXTPCLK_CBCR                                (0x2324)
#define MDSS_VSYNC_CBCR                                  (0x2328)
#define MDSS_HDMI_CBCR                                   (0x2338)
#define MDSS_BYTE0_CBCR                                  (0x233C)
#define MDSS_BYTE1_CBCR                                  (0x2340)
#define MDSS_ESC0_CBCR                                   (0x2344)
#define MDSS_ESC1_CBCR                                   (0x2348)
#define CSI0PHYTIMER_CMD_RCGR                            (0x3000)
#define CAMSS_PHY0_CSI0PHYTIMER_CBCR                     (0x3024)
#define CSI1PHYTIMER_CMD_RCGR                            (0x3030)
#define CAMSS_PHY1_CSI1PHYTIMER_CBCR                     (0x3054)
#define CSI2PHYTIMER_CMD_RCGR                            (0x3060)
#define CAMSS_PHY2_CSI2PHYTIMER_CBCR                     (0x3084)
#define CSI0_CMD_RCGR                                    (0x3090)
#define CAMSS_CSI0_CBCR                                  (0x30B4)
#define CAMSS_CSI0_AHB_CBCR                              (0x30BC)
#define CAMSS_CSI0PHY_CBCR                               (0x30C4)
#define CAMSS_CSI0RDI_CBCR                               (0x30D4)
#define CAMSS_CSI0PIX_CBCR                               (0x30E4)
#define CSI1_CMD_RCGR                                    (0x3100)
#define CAMSS_CSI1_CBCR                                  (0x3124)
#define CAMSS_CSI1_AHB_CBCR                              (0x3128)
#define CAMSS_CSI1PHY_CBCR                               (0x3134)
#define CAMSS_CSI1RDI_CBCR                               (0x3144)
#define CAMSS_CSI1PIX_CBCR                               (0x3154)
#define CSI2_CMD_RCGR                                    (0x3160)
#define CAMSS_CSI2_CBCR                                  (0x3184)
#define CAMSS_CSI2_AHB_CBCR                              (0x3188)
#define CAMSS_CSI2PHY_CBCR                               (0x3194)
#define CAMSS_CSI2RDI_CBCR                               (0x31A4)
#define CAMSS_CSI2PIX_CBCR                               (0x31B4)
#define CSI3_CMD_RCGR                                    (0x31C0)
#define CAMSS_CSI3_CBCR                                  (0x31E4)
#define CAMSS_CSI3_AHB_CBCR                              (0x31E8)
#define CAMSS_CSI3PHY_CBCR                               (0x31F4)
#define CAMSS_CSI3RDI_CBCR                               (0x3204)
#define CAMSS_CSI3PIX_CBCR                               (0x3214)
#define CAMSS_ISPIF_AHB_CBCR                             (0x3224)
#define CCI_CMD_RCGR                                     (0x3300)
#define CAMSS_CCI_CCI_CBCR                               (0x3344)
#define CAMSS_CCI_CCI_AHB_CBCR                           (0x3348)
#define MCLK0_CMD_RCGR                                   (0x3360)
#define CAMSS_MCLK0_CBCR                                 (0x3384)
#define MCLK1_CMD_RCGR                                   (0x3390)
#define CAMSS_MCLK1_CBCR                                 (0x33B4)
#define MCLK2_CMD_RCGR                                   (0x33C0)
#define CAMSS_MCLK2_CBCR                                 (0x33E4)
#define MCLK3_CMD_RCGR                                   (0x33F0)
#define CAMSS_MCLK3_CBCR                                 (0x3414)
#define MMSS_GP0_CMD_RCGR                                (0x3420)
#define CAMSS_GP0_CBCR                                   (0x3444)
#define MMSS_GP1_CMD_RCGR                                (0x3450)
#define CAMSS_GP1_CBCR                                   (0x3474)
#define CAMSS_TOP_AHB_CBCR                               (0x3484)
#define CAMSS_AHB_CBCR                                   (0x348C)
#define CAMSS_MICRO_BCR                                  (0x3490)
#define CAMSS_MICRO_AHB_CBCR                             (0x3494)
#define JPEG0_CMD_RCGR                                   (0x3500)
#define JPEG_DMA_CMD_RCGR                                (0x3560)
#define CAMSS_JPEG_JPEG0_CBCR                            (0x35A8)
#define CAMSS_JPEG_DMA_CBCR                              (0x35C0)
#define CAMSS_JPEG_JPEG_AHB_CBCR                         (0x35B4)
#define CAMSS_JPEG_JPEG_AXI_CBCR                         (0x35B8)
#define VFE0_CMD_RCGR                                    (0x3600)
#define VFE1_CMD_RCGR                                    (0x3620)
#define CPP_CMD_RCGR                                     (0x3640)
#define CAMSS_VFE_VFE0_CBCR                              (0x36A8)
#define CAMSS_VFE_VFE1_CBCR                              (0x36AC)
#define CAMSS_VFE_CPP_CBCR                               (0x36B0)
#define CAMSS_VFE_CPP_AHB_CBCR                           (0x36B4)
#define CAMSS_VFE_VFE_AHB_CBCR                           (0x36B8)
#define CAMSS_VFE_VFE_AXI_CBCR                           (0x36BC)
#define CAMSS_VFE_CPP_AXI_CBCR                           (0x36C4)
#define CAMSS_CSI_VFE0_CBCR                              (0x3704)
#define CAMSS_CSI_VFE1_CBCR                              (0x3714)
#define OXILI_GFX3D_CBCR                                 (0x4028)
#define RBBMTIMER_CMD_RCGR                               (0x4090)
#define OXILI_RBBMTIMER_CBCR                             (0x40B0)
#define OXILICX_AHB_CBCR                                 (0x403C)
#define OCMEMCX_OCMEMNOC_CBCR                            (0x4058)
#define MMSS_MISC_AHB_CBCR                               (0x502C)
#define AXI_CMD_RCGR                                     (0x5040)
#define MMSS_S0_AXI_CBCR                                 (0x5064)
#define MMSS_MMSSNOC_AXI_CBCR                            (0x506C)
#define OCMEMNOC_CMD_RCGR                                (0x5090)
#define MMSS_DEBUG_CLK_CTL                               (0x0900)

static struct alpha_pll_masks pll_masks_p = {
	.lock_mask = BIT(31),
	.active_mask = BIT(30),
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
};

static struct alpha_pll_masks pll_masks_t = {
	.lock_mask = BIT(31),
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
};

static struct alpha_pll_vco_tbl mmpll_t_vco[] = {
	VCO(0, 500000000, 1500000000),
};

static struct alpha_pll_vco_tbl mmpll_p_vco[] = {
	VCO(3,  250000000,  500000000),
	VCO(2,  500000000, 1000000000),
	VCO(1, 1000000000, 1500000000),
	VCO(0, 1500000000, 2000000000),
};

DEFINE_EXT_CLK(mmsscc_xo, NULL);
DEFINE_EXT_CLK(mmsscc_gpll0, NULL);
DEFINE_EXT_CLK(mmsscc_mmssnoc_ahb, NULL);

static struct alpha_pll_clk mmpll0 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMPLL0_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.fsm_reg_offset = MMSS_PLL_VOTE_APCS,
	.fsm_en_mask = BIT(0),
	.enable_config = 0x1,
	.c = {
		.rate = 800000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll0",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP3(LOWER, 400000000, LOW, 400000000,
				  NOMINAL, 800000000),
		CLK_INIT(mmpll0.c),
	},
};
DEFINE_EXT_CLK(mmpll0_out_main, &mmpll0.c);

static struct alpha_pll_clk mmpll4 = {
	.masks = &pll_masks_t,
	.base = &virt_base,
	.offset = MMPLL4_MODE,
	.vco_tbl = mmpll_t_vco,
	.num_vco = ARRAY_SIZE(mmpll_t_vco),
	.enable_config = 0x1,
	.c = {
		.parent = &mmsscc_xo.c,
		.rate = 960000000,
		.dbg_name = "mmpll4",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_MMPLL4_FMAX_MAP3(LOWER, 480000000, LOW, 480000000,
				     NOMINAL, 960000000),
		CLK_INIT(mmpll4.c),
	},
};
DEFINE_EXT_CLK(mmpll4_out_main, &mmpll4.c);

static struct alpha_pll_clk mmpll1 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMPLL1_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.fsm_reg_offset = MMSS_PLL_VOTE_APCS,
	.fsm_en_mask = BIT(1),
	.enable_config = 0x1,
	.c = {
		.rate = 808000000,
		.parent = &mmsscc_xo.c,
		.dbg_name = "mmpll1",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP3(LOWER, 404000000, LOW, 404000000,
				  NOMINAL, 808000000),
		CLK_INIT(mmpll1.c),
	},
};
DEFINE_EXT_CLK(mmpll1_out_main, &mmpll1.c);

static struct alpha_pll_clk mmpll3 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMPLL3_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.enable_config = 0x1,
	.c = {
		.parent = &mmsscc_xo.c,
		.rate = 1020000000,
		.dbg_name = "mmpll3",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP3(LOWER, 510000000, LOW, 510000000,
				  NOMINAL, 1020000000),
		CLK_INIT(mmpll3.c),
	},
};
DEFINE_EXT_CLK(mmpll3_out_main, &mmpll3.c);

static struct clk_freq_tbl ftbl_axi_clk_src[] = {
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 300000000,     mmsscc_gpll0,    2,    0,     0),
	F_MM( 404000000,  mmpll1_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk axi_clk_src = {
	.cmd_rcgr_reg = AXI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_axi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "axi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 150000000,
				  NOMINAL, 300000000, HIGH, 404000000),
		CLK_INIT(axi_clk_src.c),
	},
};

static struct alpha_pll_clk mmpll5 = {
	.masks = &pll_masks_p,
	.base = &virt_base,
	.offset = MMPLL5_MODE,
	.vco_tbl = mmpll_p_vco,
	.num_vco = ARRAY_SIZE(mmpll_p_vco),
	.enable_config = 0x1,
	.c = {
		.parent = &mmsscc_xo.c,
		.rate = 800000000,
		.dbg_name = "mmpll5",
		.ops = &clk_ops_fixed_alpha_pll,
		VDD_DIG_FMAX_MAP2(LOWER, 400000000, LOW, 800000000),
		CLK_INIT(mmpll5.c),
	},
};
DEFINE_EXT_CLK(mmpll5_out_main, &mmpll5.c);

static struct clk_freq_tbl ftbl_csi0_clk_src[] = {
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 266670000,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk csi0_clk_src = {
	.cmd_rcgr_reg = CSI0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 266670000),
		CLK_INIT(csi0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vcodec0_clk_src[] = {
	F_MM(  66670000,     mmsscc_gpll0,    9,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 133330000,     mmsscc_gpll0,  4.5,    0,     0),
	F_MM( 200000000,  mmpll0_out_main,    4,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 510000000,  mmpll3_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk vcodec0_clk_src = {
	.cmd_rcgr_reg = VCODEC0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_vcodec0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "vcodec0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP4(LOWER, 66670000, LOW, 133330000,
				  NOMINAL, 320000000, HIGH, 510000000),
		CLK_INIT(vcodec0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi1_clk_src[] = {
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 266670000,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk csi1_clk_src = {
	.cmd_rcgr_reg = CSI1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 266670000),
		CLK_INIT(csi1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi2_clk_src[] = {
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 266670000,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk csi2_clk_src = {
	.cmd_rcgr_reg = CSI2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 266670000),
		CLK_INIT(csi2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi3_clk_src[] = {
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 266670000,  mmpll0_out_main,    3,    0,     0),
	F_END
};

static struct rcg_clk csi3_clk_src = {
	.cmd_rcgr_reg = CSI3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi3_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi3_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 266670000),
		CLK_INIT(csi3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vfe0_clk_src[] = {
	F_MM(  80000000,     mmsscc_gpll0,  7.5,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct rcg_clk vfe0_clk_src = {
	.cmd_rcgr_reg = VFE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vfe0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "vfe0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 100000000, LOW, 200000000,
				  NOMINAL, 480000000, HIGH, 600000000),
		CLK_INIT(vfe0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vfe1_clk_src[] = {
	F_MM(  80000000,     mmsscc_gpll0,  7.5,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 600000000,     mmsscc_gpll0,    1,    0,     0),
	F_END
};

static struct rcg_clk vfe1_clk_src = {
	.cmd_rcgr_reg = VFE1_CMD_RCGR,
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

static struct clk_freq_tbl ftbl_jpeg0_clk_src[] = {
	F_MM(  75000000,     mmsscc_gpll0,    8,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 228570000,  mmpll0_out_main,  3.5,    0,     0),
	F_MM( 266670000,  mmpll0_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk jpeg0_clk_src = {
	.cmd_rcgr_reg = JPEG0_CMD_RCGR,
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

static struct clk_freq_tbl ftbl_mdp_clk_src[] = {
	F_MM(  85710000,     mmsscc_gpll0,    7,    0,     0),
	F_MM( 171430000,     mmsscc_gpll0,  3.5,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 240000000,     mmsscc_gpll0,  2.5,    0,     0),
	F_MM( 266670000,  mmpll5_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 400000000,  mmpll0_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk mdp_clk_src = {
	.cmd_rcgr_reg = MDP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 85710000, LOW, 266670000,
				  NOMINAL, 320000000, HIGH, 400000000),
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
	.cmd_rcgr_reg = PCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = ftbl_pclk0_clk_src,
	.freq_tbl = ftbl_pclk0_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "pclk0_clk_src",
		.ops = &clk_ops_pixel_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 75000000, LOW, 150000000,
				  NOMINAL, 250000000),
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
	.cmd_rcgr_reg = PCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = ftbl_pclk1_clk_src,
	.freq_tbl = ftbl_pclk1_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "pclk1_clk_src",
		.ops = &clk_ops_pixel_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 75000000, LOW, 150000000,
				  NOMINAL, 250000000),
		CLK_INIT(pclk1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ocmemnoc_clk_src[] = {
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_MM(  75000000,     mmsscc_gpll0,    8,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 400000000,  mmpll0_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk ocmemnoc_clk_src = {
	.cmd_rcgr_reg = OCMEMNOC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ocmemnoc_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "ocmemnoc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(LOWER, 75000000, LOW, 150000000,
				  NOMINAL, 320000000, HIGH, 400000000),
		CLK_INIT(ocmemnoc_clk_src.c),
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
	.cmd_rcgr_reg = CCI_CMD_RCGR,
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

static struct clk_freq_tbl ftbl_cpp_clk_src[] = {
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,     mmsscc_gpll0,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_MM( 640000000,  mmpll4_out_main,  1.5,    0,     0),
	F_END
};

static struct rcg_clk cpp_clk_src = {
	.cmd_rcgr_reg = CPP_CMD_RCGR,
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

static struct clk_freq_tbl ftbl_mmss_gp0_clk_src[] = {
	F_MM(     10000,        mmsscc_xo,   16,    1,   120),
	F_MM(     24000,        mmsscc_xo,   16,    1,    50),
	F_MM(   6000000,     mmsscc_gpll0,   10,    1,    10),
	F_MM(  12000000,     mmsscc_gpll0,   10,    1,     5),
	F_MM(  13000000,     mmsscc_gpll0,    4,   13,   150),
	F_MM(  24000000,     mmsscc_gpll0,    5,    1,     5),
	F_END
};

static struct rcg_clk mmss_gp0_clk_src = {
	.cmd_rcgr_reg = MMSS_GP0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mmss_gp0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_gp0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(mmss_gp0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mmss_gp1_clk_src[] = {
	F_MM(     10000,        mmsscc_xo,   16,    1,   120),
	F_MM(     24000,        mmsscc_xo,   16,    1,    50),
	F_MM(   6000000,     mmsscc_gpll0,   10,    1,    10),
	F_MM(  12000000,     mmsscc_gpll0,   10,    1,     5),
	F_MM(  13000000,     mmsscc_gpll0,    4,   13,   150),
	F_MM(  24000000,     mmsscc_gpll0,    5,    1,     5),
	F_END
};

static struct rcg_clk mmss_gp1_clk_src = {
	.cmd_rcgr_reg = MMSS_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mmss_gp1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(mmss_gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_jpeg_dma_clk_src[] = {
	F_MM(  75000000,     mmsscc_gpll0,    8,    0,     0),
	F_MM( 150000000,     mmsscc_gpll0,    4,    0,     0),
	F_MM( 228570000,  mmpll0_out_main,  3.5,    0,     0),
	F_MM( 266670000,  mmpll0_out_main,    3,    0,     0),
	F_MM( 320000000,  mmpll0_out_main,  2.5,    0,     0),
	F_MM( 480000000,  mmpll4_out_main,    2,    0,     0),
	F_END
};

static struct rcg_clk jpeg_dma_clk_src = {
	.cmd_rcgr_reg = JPEG_DMA_CMD_RCGR,
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

static struct clk_freq_tbl ftbl_mclk0_clk_src[] = {
	F_MM(   4800000,        mmsscc_xo,    4,    0,     0),
	F_MM(   6000000,  mmpll4_out_main,   10,    1,    16),
	F_MM(   8000000,  mmpll4_out_main,   10,    1,    12),
	F_MM(   9600000,        mmsscc_xo,    2,    0,     0),
	F_MM(  12000000,  mmpll4_out_main,   10,    1,     8),
	F_MM(  16000000,  mmpll4_out_main,   10,    1,     6),
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_MM(  24000000,  mmpll4_out_main,   10,    1,     4),
	F_MM(  32000000,  mmpll4_out_main,   10,    1,     3),
	F_MM(  48000000,  mmpll4_out_main,   10,    1,     2),
	F_MM(  64000000,  mmpll4_out_main,   15,    0,     0),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg = MCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk0_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 33330000, LOW, 66670000),
		CLK_INIT(mclk0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mclk1_clk_src[] = {
	F_MM(   4800000,        mmsscc_xo,    4,    0,     0),
	F_MM(   6000000,  mmpll4_out_main,   10,    1,    16),
	F_MM(   8000000,  mmpll4_out_main,   10,    1,    12),
	F_MM(   9600000,        mmsscc_xo,    2,    0,     0),
	F_MM(  16000000,  mmpll4_out_main,   10,    1,     6),
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_MM(  24000000,  mmpll4_out_main,   10,    1,     4),
	F_MM(  32000000,  mmpll4_out_main,   10,    1,     3),
	F_MM(  48000000,  mmpll4_out_main,   10,    1,     2),
	F_MM(  64000000,  mmpll4_out_main,   15,    0,     0),
	F_END
};

static struct rcg_clk mclk1_clk_src = {
	.cmd_rcgr_reg = MCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 33330000, LOW, 66670000),
		CLK_INIT(mclk1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mclk2_clk_src[] = {
	F_MM(   4800000,        mmsscc_xo,    4,    0,     0),
	F_MM(   6000000,  mmpll4_out_main,   10,    1,    16),
	F_MM(   8000000,  mmpll4_out_main,   10,    1,    12),
	F_MM(   9600000,        mmsscc_xo,    2,    0,     0),
	F_MM(  16000000,  mmpll4_out_main,   10,    1,     6),
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_MM(  24000000,  mmpll4_out_main,   10,    1,     4),
	F_MM(  32000000,  mmpll4_out_main,   10,    1,     3),
	F_MM(  48000000,  mmpll4_out_main,   10,    1,     2),
	F_MM(  64000000,  mmpll4_out_main,   15,    0,     0),
	F_END
};

static struct rcg_clk mclk2_clk_src = {
	.cmd_rcgr_reg = MCLK2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 33330000, LOW, 66670000),
		CLK_INIT(mclk2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mclk3_clk_src[] = {
	F_MM(   4800000,        mmsscc_xo,    4,    0,     0),
	F_MM(   6000000,  mmpll4_out_main,   10,    1,    16),
	F_MM(   8000000,  mmpll4_out_main,   10,    1,    12),
	F_MM(   9600000,        mmsscc_xo,    2,    0,     0),
	F_MM(  16000000,  mmpll4_out_main,   10,    1,     6),
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_MM(  24000000,  mmpll4_out_main,   10,    1,     4),
	F_MM(  32000000,  mmpll4_out_main,   10,    1,     3),
	F_MM(  48000000,  mmpll4_out_main,   10,    1,     2),
	F_MM(  64000000,  mmpll4_out_main,   15,    0,     0),
	F_END
};

static struct rcg_clk mclk3_clk_src = {
	.cmd_rcgr_reg = MCLK3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_mclk3_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "mclk3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 33330000, LOW, 66670000),
		CLK_INIT(mclk3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi0phytimer_clk_src[] = {
	F_MM(  50000000,     mmsscc_gpll0,   12,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,  mmpll0_out_main,    4,    0,     0),
	F_END
};

static struct rcg_clk csi0phytimer_clk_src = {
	.cmd_rcgr_reg = CSI0PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi0phytimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi0phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(csi0phytimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi1phytimer_clk_src[] = {
	F_MM(  50000000,     mmsscc_gpll0,   12,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,  mmpll0_out_main,    4,    0,     0),
	F_END
};

static struct rcg_clk csi1phytimer_clk_src = {
	.cmd_rcgr_reg = CSI1PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi1phytimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi1phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(csi1phytimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_csi2phytimer_clk_src[] = {
	F_MM(  50000000,     mmsscc_gpll0,   12,    0,     0),
	F_MM( 100000000,     mmsscc_gpll0,    6,    0,     0),
	F_MM( 200000000,  mmpll0_out_main,    4,    0,     0),
	F_END
};

static struct rcg_clk csi2phytimer_clk_src = {
	.cmd_rcgr_reg = CSI2PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_csi2phytimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "csi2phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(csi2phytimer_clk_src.c),
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
	.cmd_rcgr_reg = BYTE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.current_freq = ftbl_byte0_clk_src,
	.freq_tbl = ftbl_byte0_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "byte0_clk_src",
		.ops = &clk_ops_byte_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 60000000, LOW, 112500000,
				  NOMINAL, 187500000),
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
	.cmd_rcgr_reg = BYTE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.current_freq = ftbl_byte1_clk_src,
	.freq_tbl = ftbl_byte1_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "byte1_clk_src",
		.ops = &clk_ops_byte_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 60000000, LOW, 112500000,
				  NOMINAL, 187500000),
		CLK_INIT(byte1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_esc0_clk_src[] = {
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk esc0_clk_src = {
	.cmd_rcgr_reg = ESC0_CMD_RCGR,
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
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk esc1_clk_src = {
	.cmd_rcgr_reg = ESC1_CMD_RCGR,
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
		.div_src_val = BVAL(10, 8, hdmiphypll_mm_source_val),
		.src_clk = &ext_extpclk_clk_src.c,
	},
	F_END
};

static struct rcg_clk extpclk_clk_src = {
	.cmd_rcgr_reg = EXTPCLK_CMD_RCGR,
	.current_freq = ftbl_extpclk_clk_src,
	.base = &virt_base,
	.c = {
		.dbg_name = "extpclk_clk_src",
		.parent = &ext_extpclk_clk_src.c,
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP3(LOWER, 85000000, LOW, 170000000,
				  NOMINAL, 340000000),
		CLK_INIT(extpclk_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_hdmi_clk_src[] = {
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk hdmi_clk_src = {
	.cmd_rcgr_reg = HDMI_CMD_RCGR,
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
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk vsync_clk_src = {
	.cmd_rcgr_reg = VSYNC_CMD_RCGR,
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

static struct clk_freq_tbl ftbl_rbbmtimer_clk_src[] = {
	F_MM(  19200000,        mmsscc_xo,    1,    0,     0),
	F_END
};

static struct rcg_clk rbbmtimer_clk_src = {
	.cmd_rcgr_reg = RBBMTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_rbbmtimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "rbbmtimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(rbbmtimer_clk_src.c),
	},
};

static struct branch_clk camss_ahb_clk = {
	.cbcr_reg = CAMSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_ahb_clk.c),
	},
};

static struct branch_clk camss_cci_cci_ahb_clk = {
	.cbcr_reg = CAMSS_CCI_CCI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_cci_cci_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cci_cci_ahb_clk.c),
	},
};

static struct branch_clk camss_cci_cci_clk = {
	.cbcr_reg = CAMSS_CCI_CCI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_cci_cci_clk",
		.parent = &cci_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cci_cci_clk.c),
	},
};

static struct branch_clk camss_vfe_cpp_ahb_clk = {
	.cbcr_reg = CAMSS_VFE_CPP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe_cpp_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_cpp_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_cpp_axi_clk = {
	.cbcr_reg = CAMSS_VFE_CPP_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe_cpp_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_cpp_axi_clk.c),
	},
};

static struct branch_clk camss_vfe_cpp_clk = {
	.cbcr_reg = CAMSS_VFE_CPP_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe_cpp_clk",
		.parent = &cpp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_cpp_clk.c),
	},
};

static struct branch_clk camss_csi0_ahb_clk = {
	.cbcr_reg = CAMSS_CSI0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0_ahb_clk.c),
	},
};

static struct branch_clk camss_csi0_clk = {
	.cbcr_reg = CAMSS_CSI0_CBCR,
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
	.cbcr_reg = CAMSS_CSI0PHY_CBCR,
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
	.cbcr_reg = CAMSS_CSI0PIX_CBCR,
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
	.cbcr_reg = CAMSS_CSI0RDI_CBCR,
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
	.cbcr_reg = CAMSS_CSI1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1_ahb_clk.c),
	},
};

static struct branch_clk camss_csi1_clk = {
	.cbcr_reg = CAMSS_CSI1_CBCR,
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
	.cbcr_reg = CAMSS_CSI1PHY_CBCR,
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
	.cbcr_reg = CAMSS_CSI1PIX_CBCR,
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
	.cbcr_reg = CAMSS_CSI1RDI_CBCR,
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
	.cbcr_reg = CAMSS_CSI2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2_ahb_clk.c),
	},
};

static struct branch_clk camss_csi2_clk = {
	.cbcr_reg = CAMSS_CSI2_CBCR,
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
	.cbcr_reg = CAMSS_CSI2PHY_CBCR,
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
	.cbcr_reg = CAMSS_CSI2PIX_CBCR,
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
	.cbcr_reg = CAMSS_CSI2RDI_CBCR,
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
	.cbcr_reg = CAMSS_CSI3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3_ahb_clk.c),
	},
};

static struct branch_clk camss_csi3_clk = {
	.cbcr_reg = CAMSS_CSI3_CBCR,
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
	.cbcr_reg = CAMSS_CSI3PHY_CBCR,
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
	.cbcr_reg = CAMSS_CSI3PIX_CBCR,
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
	.cbcr_reg = CAMSS_CSI3RDI_CBCR,
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
	.cbcr_reg = CAMSS_CSI_VFE0_CBCR,
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
	.cbcr_reg = CAMSS_CSI_VFE1_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_csi_vfe1_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi_vfe1_clk.c),
	},
};

static struct branch_clk camss_gp0_clk = {
	.cbcr_reg = CAMSS_GP0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_gp0_clk",
		.parent = &mmss_gp0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_gp0_clk.c),
	},
};

static struct branch_clk camss_gp1_clk = {
	.cbcr_reg = CAMSS_GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_gp1_clk",
		.parent = &mmss_gp1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_gp1_clk.c),
	},
};

static struct branch_clk camss_ispif_ahb_clk = {
	.cbcr_reg = CAMSS_ISPIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_ispif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_ispif_ahb_clk.c),
	},
};

static struct branch_clk camss_jpeg_dma_clk = {
	.cbcr_reg = CAMSS_JPEG_DMA_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_jpeg_dma_clk",
		.parent = &jpeg_dma_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_dma_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg0_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_jpeg_jpeg0_clk",
		.parent = &jpeg0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg0_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg_ahb_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_jpeg_jpeg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_ahb_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg_axi_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_jpeg_jpeg_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_axi_clk.c),
	},
};

static struct branch_clk camss_mclk0_clk = {
	.cbcr_reg = CAMSS_MCLK0_CBCR,
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
	.cbcr_reg = CAMSS_MCLK1_CBCR,
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
	.cbcr_reg = CAMSS_MCLK2_CBCR,
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
	.cbcr_reg = CAMSS_MCLK3_CBCR,
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
	.cbcr_reg = CAMSS_MICRO_AHB_CBCR,
	.bcr_reg = CAMSS_MICRO_BCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_micro_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_micro_ahb_clk.c),
	},
};

static struct branch_clk camss_phy0_csi0phytimer_clk = {
	.cbcr_reg = CAMSS_PHY0_CSI0PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_phy0_csi0phytimer_clk",
		.parent = &csi0phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy0_csi0phytimer_clk.c),
	},
};

static struct branch_clk camss_phy1_csi1phytimer_clk = {
	.cbcr_reg = CAMSS_PHY1_CSI1PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_phy1_csi1phytimer_clk",
		.parent = &csi1phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy1_csi1phytimer_clk.c),
	},
};

static struct branch_clk camss_phy2_csi2phytimer_clk = {
	.cbcr_reg = CAMSS_PHY2_CSI2PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_phy2_csi2phytimer_clk",
		.parent = &csi2phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy2_csi2phytimer_clk.c),
	},
};

static struct branch_clk camss_top_ahb_clk = {
	.cbcr_reg = CAMSS_TOP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_top_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_top_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe0_clk = {
	.cbcr_reg = CAMSS_VFE_VFE0_CBCR,
	.has_sibling = 0,
	.toggle_memory = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe_vfe0_clk",
		.parent = &vfe0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe0_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe1_clk = {
	.cbcr_reg = CAMSS_VFE_VFE1_CBCR,
	.has_sibling = 0,
	.toggle_memory = true,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe_vfe1_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe1_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe_ahb_clk = {
	.cbcr_reg = CAMSS_VFE_VFE_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe_vfe_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe_axi_clk = {
	.cbcr_reg = CAMSS_VFE_VFE_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "camss_vfe_vfe_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe_axi_clk.c),
	},
};

static struct branch_clk mdss_ahb_clk = {
	.cbcr_reg = MDSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_ahb_clk.c),
	},
};

static struct branch_clk mdss_axi_clk = {
	.cbcr_reg = MDSS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_axi_clk.c),
	},
};

static struct branch_clk mdss_byte0_clk = {
	.cbcr_reg = MDSS_BYTE0_CBCR,
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
	.cbcr_reg = MDSS_BYTE1_CBCR,
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
	.cbcr_reg = MDSS_ESC0_CBCR,
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
	.cbcr_reg = MDSS_ESC1_CBCR,
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
	.cbcr_reg = MDSS_EXTPCLK_CBCR,
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
	.cbcr_reg = MDSS_HDMI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_hdmi_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_hdmi_ahb_clk.c),
	},
};

static struct branch_clk mdss_hdmi_clk = {
	.cbcr_reg = MDSS_HDMI_CBCR,
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
	.cbcr_reg = MDSS_MDP_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mdss_mdp_clk",
		.parent = &mdp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_clk.c),
	},
};

static struct branch_clk mdss_pclk0_clk = {
	.cbcr_reg = MDSS_PCLK0_CBCR,
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
	.cbcr_reg = MDSS_PCLK1_CBCR,
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
	.cbcr_reg = MDSS_VSYNC_CBCR,
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
	.cbcr_reg = MMSS_MISC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_misc_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_misc_ahb_clk.c),
	},
};

static struct branch_clk mmss_mmssnoc_axi_clk = {
	.cbcr_reg = MMSS_MMSSNOC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_mmssnoc_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mmssnoc_axi_clk.c),
	},
};

static struct branch_clk mmss_s0_axi_clk = {
	.cbcr_reg = MMSS_S0_AXI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "mmss_s0_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_s0_axi_clk.c),
		.depends = &mmss_mmssnoc_axi_clk.c,
	},
};

static struct branch_clk ocmemcx_ocmemnoc_clk = {
	.cbcr_reg = OCMEMCX_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "ocmemcx_ocmemnoc_clk",
		.parent = &ocmemnoc_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(ocmemcx_ocmemnoc_clk.c),
	},
};

static struct branch_clk oxili_gfx3d_clk = {
	.cbcr_reg = OXILI_GFX3D_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "oxili_gfx3d_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(oxili_gfx3d_clk.c),
	},
};

static struct branch_clk oxili_rbbmtimer_clk = {
	.cbcr_reg = OXILI_RBBMTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "oxili_rbbmtimer_clk",
		.parent = &rbbmtimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(oxili_rbbmtimer_clk.c),
	},
};

static struct branch_clk oxilicx_ahb_clk = {
	.cbcr_reg = OXILICX_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "oxilicx_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(oxilicx_ahb_clk.c),
	},
};

static struct branch_clk venus0_ahb_clk = {
	.cbcr_reg = VENUS0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "venus0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_ahb_clk.c),
	},
};

static struct branch_clk venus0_axi_clk = {
	.cbcr_reg = VENUS0_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "venus0_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_axi_clk.c),
	},
};

static struct branch_clk venus0_ocmemnoc_clk = {
	.cbcr_reg = VENUS0_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "venus0_ocmemnoc_clk",
		.parent = &ocmemnoc_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_ocmemnoc_clk.c),
	},
};

static struct branch_clk venus0_vcodec0_clk = {
	.cbcr_reg = VENUS0_VCODEC0_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "venus0_vcodec0_clk",
		.parent = &vcodec0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_vcodec0_clk.c),
	},
};

static struct branch_clk venus0_core0_vcodec_clk = {
	.cbcr_reg = VENUS0_CORE0_VCODEC_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "venus0_core0_vcodec_clk",
		.parent = &vcodec0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_core0_vcodec_clk.c),
	},
};

static struct branch_clk venus0_core1_vcodec_clk = {
	.cbcr_reg = VENUS0_CORE1_VCODEC_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "venus0_core1_vcodec_clk",
		.parent = &vcodec0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_core1_vcodec_clk.c),
	},
};

static int mmss_dbg_set_mux_sel(struct mux_clk *clk, int sel)
{
	unsigned long flags;
	u32 regval;

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Set debug mux clock index */
	regval = BVAL(11, 0, sel);
	writel_relaxed(regval, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

static int mmss_dbg_get_mux_sel(struct mux_clk *clk)
{
	return readl_relaxed(MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL)) & BM(11, 0);
}

static int mmss_dbg_mux_enable(struct mux_clk *clk)
{
	u32 regval;

	regval = readl_relaxed(MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));
	writel_relaxed(regval | BIT(16), MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));
	/* Make sure the register write is complete */
	mb();

	return 0;
}

static void mmss_dbg_mux_disable(struct mux_clk *clk)
{
	u32 regval;

	regval = readl_relaxed(MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));
	writel_relaxed(regval & ~BIT(16),
		       MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));
	/* Make sure the register write is complete */
	mb();
}

static struct clk_mux_ops debug_mux_ops = {
	.enable = mmss_dbg_mux_enable,
	.disable = mmss_dbg_mux_disable,
	.set_mux_sel = mmss_dbg_set_mux_sel,
	.get_mux_sel = mmss_dbg_get_mux_sel,
};

static struct mux_clk mmss_debug_mux = {
	.ops = &debug_mux_ops,
	MUX_SRC_LIST(
		{ &mmsscc_mmssnoc_ahb.c, 0x0001 },
		{ &oxili_gfx3d_clk.c, 0x000d },
		{ &mmss_misc_ahb_clk.c, 0x0003 },
		{ &mmss_mmssnoc_axi_clk.c, 0x0004 },
		{ &mmss_s0_axi_clk.c, 0x0005 },
		{ &ocmemcx_ocmemnoc_clk.c, 0x0009 },
		{ &oxilicx_ahb_clk.c, 0x000c },
		{ &venus0_vcodec0_clk.c, 0x000e },
		{ &venus0_axi_clk.c, 0x000f },
		{ &venus0_ocmemnoc_clk.c, 0x0010 },
		{ &venus0_ahb_clk.c, 0x0011 },
		{ &mdss_mdp_clk.c, 0x0014 },
		{ &mdss_pclk0_clk.c, 0x0016 },
		{ &mdss_pclk1_clk.c, 0x0017 },
		{ &mdss_extpclk_clk.c, 0x0018 },
		{ &venus0_core0_vcodec_clk.c, 0x001a },
		{ &venus0_core1_vcodec_clk.c, 0x001b },
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
		{ &camss_cci_cci_clk.c, 0x002d },
		{ &camss_cci_cci_ahb_clk.c, 0x002e },
		{ &camss_phy0_csi0phytimer_clk.c, 0x002f },
		{ &camss_phy1_csi1phytimer_clk.c, 0x0030 },
		{ &camss_phy2_csi2phytimer_clk.c, 0x0031 },
		{ &camss_jpeg_jpeg0_clk.c, 0x0032 },
		{ &camss_jpeg_dma_clk.c, 0x0033 },
		{ &camss_vfe_cpp_axi_clk.c, 0x0034 },
		{ &camss_jpeg_jpeg_ahb_clk.c, 0x0035 },
		{ &camss_jpeg_jpeg_axi_clk.c, 0x0036 },
		{ &camss_ahb_clk.c, 0x0037 },
		{ &camss_vfe_vfe0_clk.c, 0x0038 },
		{ &camss_vfe_vfe1_clk.c, 0x0039 },
		{ &camss_vfe_cpp_clk.c, 0x003a },
		{ &camss_vfe_cpp_ahb_clk.c, 0x003b },
		{ &camss_vfe_vfe_ahb_clk.c, 0x003c },
		{ &camss_vfe_vfe_axi_clk.c, 0x003d },
		{ &oxili_rbbmtimer_clk.c, 0x003e },
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
		{ &camss_ispif_ahb_clk.c, 0x0055 },
	),
	.c = {
		.dbg_name = "mmss_debug_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mmss_debug_mux.c),
	},
};

static struct clk_lookup msm_clocks_mmss_8992[] = {
	CLK_LIST(mmsscc_xo),
	CLK_LIST(mmsscc_gpll0),
	CLK_LIST(mmsscc_mmssnoc_ahb),
	CLK_LIST(mmpll0),
	CLK_LIST(mmpll0_out_main),
	CLK_LIST(mmpll4),
	CLK_LIST(mmpll4_out_main),
	CLK_LIST(mmpll1),
	CLK_LIST(mmpll1_out_main),
	CLK_LIST(mmpll3),
	CLK_LIST(mmpll3_out_main),
	CLK_LIST(axi_clk_src),
	CLK_LIST(mmpll5),
	CLK_LIST(mmpll5_out_main),
	CLK_LIST(csi0_clk_src),
	CLK_LIST(vcodec0_clk_src),
	CLK_LIST(csi1_clk_src),
	CLK_LIST(csi2_clk_src),
	CLK_LIST(csi3_clk_src),
	CLK_LIST(vfe0_clk_src),
	CLK_LIST(vfe1_clk_src),
	CLK_LIST(jpeg0_clk_src),
	CLK_LIST(mdp_clk_src),
	CLK_LIST(pclk0_clk_src),
	CLK_LIST(pclk1_clk_src),
	CLK_LIST(ext_pclk0_clk_src),
	CLK_LIST(ext_pclk1_clk_src),
	CLK_LIST(ocmemnoc_clk_src),
	CLK_LIST(cci_clk_src),
	CLK_LIST(cpp_clk_src),
	CLK_LIST(mmss_gp0_clk_src),
	CLK_LIST(mmss_gp1_clk_src),
	CLK_LIST(jpeg_dma_clk_src),
	CLK_LIST(mclk0_clk_src),
	CLK_LIST(mclk1_clk_src),
	CLK_LIST(mclk2_clk_src),
	CLK_LIST(mclk3_clk_src),
	CLK_LIST(csi0phytimer_clk_src),
	CLK_LIST(csi1phytimer_clk_src),
	CLK_LIST(csi2phytimer_clk_src),
	CLK_LIST(byte0_clk_src),
	CLK_LIST(byte1_clk_src),
	CLK_LIST(ext_byte0_clk_src),
	CLK_LIST(ext_byte1_clk_src),
	CLK_LIST(esc0_clk_src),
	CLK_LIST(esc1_clk_src),
	CLK_LIST(extpclk_clk_src),
	CLK_LIST(hdmi_clk_src),
	CLK_LIST(vsync_clk_src),
	CLK_LIST(rbbmtimer_clk_src),
	CLK_LIST(camss_ahb_clk),
	CLK_LIST(camss_cci_cci_ahb_clk),
	CLK_LIST(camss_cci_cci_clk),
	CLK_LIST(camss_vfe_cpp_ahb_clk),
	CLK_LIST(camss_vfe_cpp_axi_clk),
	CLK_LIST(camss_vfe_cpp_clk),
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
	CLK_LIST(camss_gp0_clk),
	CLK_LIST(camss_gp1_clk),
	CLK_LIST(camss_ispif_ahb_clk),
	CLK_LIST(camss_jpeg_dma_clk),
	CLK_LIST(camss_jpeg_jpeg0_clk),
	CLK_LIST(camss_jpeg_jpeg_ahb_clk),
	CLK_LIST(camss_jpeg_jpeg_axi_clk),
	CLK_LIST(camss_mclk0_clk),
	CLK_LIST(camss_mclk1_clk),
	CLK_LIST(camss_mclk2_clk),
	CLK_LIST(camss_mclk3_clk),
	CLK_LIST(camss_micro_ahb_clk),
	CLK_LIST(camss_phy0_csi0phytimer_clk),
	CLK_LIST(camss_phy1_csi1phytimer_clk),
	CLK_LIST(camss_phy2_csi2phytimer_clk),
	CLK_LIST(camss_top_ahb_clk),
	CLK_LIST(camss_vfe_vfe0_clk),
	CLK_LIST(camss_vfe_vfe1_clk),
	CLK_LIST(camss_vfe_vfe_ahb_clk),
	CLK_LIST(camss_vfe_vfe_axi_clk),
	CLK_LIST(mdss_ahb_clk),
	CLK_LIST(mdss_axi_clk),
	CLK_LIST(mdss_byte0_clk),
	CLK_LIST(mdss_byte1_clk),
	CLK_LIST(mdss_esc0_clk),
	CLK_LIST(mdss_esc1_clk),
	CLK_LIST(mdss_extpclk_clk),
	CLK_LIST(mdss_hdmi_ahb_clk),
	CLK_LIST(mdss_hdmi_clk),
	CLK_LIST(mdss_mdp_clk),
	CLK_LIST(mdss_pclk0_clk),
	CLK_LIST(mdss_pclk1_clk),
	CLK_LIST(mdss_vsync_clk),
	CLK_LIST(mmss_misc_ahb_clk),
	CLK_LIST(mmss_mmssnoc_axi_clk),
	CLK_LIST(mmss_s0_axi_clk),
	CLK_LIST(ocmemcx_ocmemnoc_clk),
	CLK_LIST(oxili_gfx3d_clk),
	CLK_LIST(oxili_rbbmtimer_clk),
	CLK_LIST(oxilicx_ahb_clk),
	CLK_LIST(venus0_ahb_clk),
	CLK_LIST(venus0_axi_clk),
	CLK_LIST(venus0_ocmemnoc_clk),
	CLK_LIST(venus0_vcodec0_clk),
	CLK_LIST(venus0_core0_vcodec_clk),
	CLK_LIST(venus0_core1_vcodec_clk),
	CLK_LIST(mmss_debug_mux),
	CLK_LIST(ext_extpclk_clk_src),
};

int msm_mmsscc_8992_probe(struct platform_device *pdev)
{
	struct resource *res;
	int rc;
	struct clk *tmp;
	struct regulator *reg;

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
			dev_err(&pdev->dev,
				"Unable to get mmpll4_dig regulator!");
		return PTR_ERR(reg);
	}

	reg = vdd_mmpll4.regulator[1] = devm_regulator_get(&pdev->dev,
							   "mmpll4_analog");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get mmpll4_analog regulator!");
		return PTR_ERR(reg);
	}

	tmp = mmsscc_xo.c.parent = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(tmp)) {
		if (PTR_ERR(tmp) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo clock!");
		return PTR_ERR(tmp);
	}

	tmp = mmsscc_gpll0.c.parent = devm_clk_get(&pdev->dev, "gpll0");
	if (IS_ERR(tmp)) {
		if (PTR_ERR(tmp) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get gpll0 clock!");
		return PTR_ERR(tmp);
	}

	tmp = mmsscc_mmssnoc_ahb.c.parent =
				devm_clk_get(&pdev->dev, "mmssnoc_ahb");
	if (IS_ERR(tmp)) {
		if (PTR_ERR(tmp) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get MMSSNOC AHB clock!");
		return PTR_ERR(tmp);
	}

	tmp = oxili_gfx3d_clk.c.parent =
				devm_clk_get(&pdev->dev, "oxili_gfx3d_clk");
	if (IS_ERR(tmp)) {
		if (PTR_ERR(tmp) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get oxili_gfx3d clock!");
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

	rc = of_msm_clock_register(pdev->dev.of_node, msm_clocks_mmss_8992,
				   ARRAY_SIZE(msm_clocks_mmss_8992));
	if (rc)
		return rc;

	dev_info(&pdev->dev, "Registered MMSS clocks.\n");

	return 0;
}

static struct of_device_id msm_clock_mmss_match_table[] = {
	{ .compatible = "qcom,mmsscc-8992" },
	{}
};

static struct platform_driver msm_clock_mmss_driver = {
	.probe = msm_mmsscc_8992_probe,
	.driver = {
		.name = "qcom,mmsscc-8992",
		.of_match_table = msm_clock_mmss_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_mmsscc_8992_init(void)
{
	return platform_driver_register(&msm_clock_mmss_driver);
}
arch_initcall(msm_mmsscc_8992_init);


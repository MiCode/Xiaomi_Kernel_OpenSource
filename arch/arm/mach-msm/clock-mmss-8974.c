/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk/msm-clock-generic.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-rpm.h>
#include <soc/qcom/clock-voter.h>

#include <dt-bindings/clock/msm-clocks-8974.h>

#include "clock-mdss-8974.h"
#include "clock.h"

enum {
	MMSS_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define MMSS_REG_BASE(x) (void __iomem *)(virt_bases[MMSS_BASE] + (x))

#define MMPLL0_MODE_REG                0x0000
#define MMPLL0_L_REG                   0x0004
#define MMPLL0_M_REG                   0x0008
#define MMPLL0_N_REG                   0x000C
#define MMPLL0_USER_CTL_REG            0x0010
#define MMPLL0_CONFIG_CTL_REG          0x0014
#define MMPLL0_TEST_CTL_REG            0x0018
#define MMPLL0_STATUS_REG              0x001C

#define MMPLL1_MODE_REG                0x0040
#define MMPLL1_L_REG                   0x0044
#define MMPLL1_M_REG                   0x0048
#define MMPLL1_N_REG                   0x004C
#define MMPLL1_USER_CTL_REG            0x0050
#define MMPLL1_CONFIG_CTL_REG          0x0054
#define MMPLL1_TEST_CTL_REG            0x0058
#define MMPLL1_STATUS_REG              0x005C

#define MMPLL3_MODE_REG                0x0080
#define MMPLL3_L_REG                   0x0084
#define MMPLL3_M_REG                   0x0088
#define MMPLL3_N_REG                   0x008C
#define MMPLL3_USER_CTL_REG            0x0090
#define MMPLL3_CONFIG_CTL_REG          0x0094
#define MMPLL3_TEST_CTL_REG            0x0098
#define MMPLL3_STATUS_REG              0x009C

#define GCC_DEBUG_CLK_CTL_REG          0x1880
#define CLOCK_FRQ_MEASURE_CTL_REG      0x1884
#define CLOCK_FRQ_MEASURE_STATUS_REG   0x1888
#define GCC_XO_DIV4_CBCR_REG           0x10C8
#define GCC_PLLTEST_PAD_CFG_REG        0x188C
#define APCS_GPLL_ENA_VOTE_REG         0x1480
#define MMSS_PLL_VOTE_APCS_REG         0x0100
#define MMSS_DEBUG_CLK_CTL_REG         0x0900

#define VCODEC0_CMD_RCGR               0x1000
#define PCLK0_CMD_RCGR                 0x2000
#define PCLK1_CMD_RCGR                 0x2020
#define MDP_CMD_RCGR                   0x2040
#define EXTPCLK_CMD_RCGR               0x2060
#define VSYNC_CMD_RCGR                 0x2080
#define EDPPIXEL_CMD_RCGR              0x20A0
#define EDPLINK_CMD_RCGR               0x20C0
#define EDPAUX_CMD_RCGR                0x20E0
#define HDMI_CMD_RCGR                  0x2100
#define BYTE0_CMD_RCGR                 0x2120
#define BYTE1_CMD_RCGR                 0x2140
#define ESC0_CMD_RCGR                  0x2160
#define ESC1_CMD_RCGR                  0x2180
#define CSI0PHYTIMER_CMD_RCGR          0x3000
#define CSI1PHYTIMER_CMD_RCGR          0x3030
#define CSI2PHYTIMER_CMD_RCGR          0x3060
#define CSI0_CMD_RCGR                  0x3090
#define CSI1_CMD_RCGR                  0x3100
#define CSI2_CMD_RCGR                  0x3160
#define CSI3_CMD_RCGR                  0x31C0
#define CCI_CMD_RCGR                   0x3300
#define MCLK0_CMD_RCGR                 0x3360
#define MCLK1_CMD_RCGR                 0x3390
#define MCLK2_CMD_RCGR                 0x33C0
#define MCLK3_CMD_RCGR                 0x33F0
#define MMSS_GP0_CMD_RCGR              0x3420
#define MMSS_GP1_CMD_RCGR              0x3450
#define JPEG0_CMD_RCGR                 0x3500
#define JPEG1_CMD_RCGR                 0x3520
#define JPEG2_CMD_RCGR                 0x3540
#define VFE0_CMD_RCGR                  0x3600
#define VFE1_CMD_RCGR                  0x3620
#define CPP_CMD_RCGR                   0x3640
#define GFX3D_CMD_RCGR                 0x4000
#define RBCPR_CMD_RCGR                 0x4060
#define AHB_CMD_RCGR                   0x5000
#define AXI_CMD_RCGR                   0x5040
#define OCMEMNOC_CMD_RCGR              0x5090
#define OCMEMCX_OCMEMNOC_CBCR          0x4058

#define VENUS0_BCR                0x1020
#define MDSS_BCR                  0x2300
#define CAMSS_PHY0_BCR            0x3020
#define CAMSS_PHY1_BCR            0x3050
#define CAMSS_PHY2_BCR            0x3080
#define CAMSS_CSI0_BCR            0x30B0
#define CAMSS_CSI0PHY_BCR         0x30C0
#define CAMSS_CSI0RDI_BCR         0x30D0
#define CAMSS_CSI0PIX_BCR         0x30E0
#define CAMSS_CSI1_BCR            0x3120
#define CAMSS_CSI1PHY_BCR         0x3130
#define CAMSS_CSI1RDI_BCR         0x3140
#define CAMSS_CSI1PIX_BCR         0x3150
#define CAMSS_CSI2_BCR            0x3180
#define CAMSS_CSI2PHY_BCR         0x3190
#define CAMSS_CSI2RDI_BCR         0x31A0
#define CAMSS_CSI2PIX_BCR         0x31B0
#define CAMSS_CSI3_BCR            0x31E0
#define CAMSS_CSI3PHY_BCR         0x31F0
#define CAMSS_CSI3RDI_BCR         0x3200
#define CAMSS_CSI3PIX_BCR         0x3210
#define CAMSS_ISPIF_BCR           0x3220
#define CAMSS_CCI_BCR             0x3340
#define CAMSS_MCLK0_BCR           0x3380
#define CAMSS_MCLK1_BCR           0x33B0
#define CAMSS_MCLK2_BCR           0x33E0
#define CAMSS_MCLK3_BCR           0x3410
#define CAMSS_GP0_BCR             0x3440
#define CAMSS_GP1_BCR             0x3470
#define CAMSS_TOP_BCR             0x3480
#define CAMSS_MICRO_BCR           0x3490
#define CAMSS_JPEG_BCR            0x35A0
#define CAMSS_VFE_BCR             0x36A0
#define CAMSS_CSI_VFE0_BCR        0x3700
#define CAMSS_CSI_VFE1_BCR        0x3710
#define OCMEMNOC_BCR              0x50B0
#define MMSSNOCAHB_BCR            0x5020
#define MMSSNOCAXI_BCR            0x5060
#define OXILI_GFX3D_CBCR          0x4028
#define OXILICX_AHB_CBCR          0x403C
#define OXILICX_AXI_CBCR          0x4038
#define OXILI_BCR                 0x4020
#define OXILICX_BCR               0x4030

#define OCMEM_SYS_NOC_AXI_CBCR                   0x0244
#define OCMEM_NOC_CFG_AHB_CBCR                   0x0248
#define MMSS_NOC_CFG_AHB_CBCR                    0x024C

#define VENUS0_VCODEC0_CBCR                      0x1028
#define VENUS0_AHB_CBCR                          0x1030
#define VENUS0_AXI_CBCR                          0x1034
#define VENUS0_OCMEMNOC_CBCR                     0x1038
#define MDSS_AHB_CBCR                            0x2308
#define MDSS_HDMI_AHB_CBCR                       0x230C
#define MDSS_AXI_CBCR                            0x2310
#define MDSS_PCLK0_CBCR                          0x2314
#define MDSS_PCLK1_CBCR                          0x2318
#define MDSS_MDP_CBCR                            0x231C
#define MDSS_MDP_LUT_CBCR                        0x2320
#define MDSS_EXTPCLK_CBCR                        0x2324
#define MDSS_VSYNC_CBCR                          0x2328
#define MDSS_EDPPIXEL_CBCR                       0x232C
#define MDSS_EDPLINK_CBCR                        0x2330
#define MDSS_EDPAUX_CBCR                         0x2334
#define MDSS_HDMI_CBCR                           0x2338
#define MDSS_BYTE0_CBCR                          0x233C
#define MDSS_BYTE1_CBCR                          0x2340
#define MDSS_ESC0_CBCR                           0x2344
#define MDSS_ESC1_CBCR                           0x2348
#define CAMSS_PHY0_CSI0PHYTIMER_CBCR             0x3024
#define CAMSS_PHY1_CSI1PHYTIMER_CBCR             0x3054
#define CAMSS_PHY2_CSI2PHYTIMER_CBCR             0x3084
#define CAMSS_CSI0_CBCR                          0x30B4
#define CAMSS_CSI0_AHB_CBCR                      0x30BC
#define CAMSS_CSI0PHY_CBCR                       0x30C4
#define CAMSS_CSI0RDI_CBCR                       0x30D4
#define CAMSS_CSI0PIX_CBCR                       0x30E4
#define CAMSS_CSI1_CBCR                          0x3124
#define CAMSS_CSI1_AHB_CBCR                      0x3128
#define CAMSS_CSI1PHY_CBCR                       0x3134
#define CAMSS_CSI1RDI_CBCR                       0x3144
#define CAMSS_CSI1PIX_CBCR                       0x3154
#define CAMSS_CSI2_CBCR                          0x3184
#define CAMSS_CSI2_AHB_CBCR                      0x3188
#define CAMSS_CSI2PHY_CBCR                       0x3194
#define CAMSS_CSI2RDI_CBCR                       0x31A4
#define CAMSS_CSI2PIX_CBCR                       0x31B4
#define CAMSS_CSI3_CBCR                          0x31E4
#define CAMSS_CSI3_AHB_CBCR                      0x31E8
#define CAMSS_CSI3PHY_CBCR                       0x31F4
#define CAMSS_CSI3RDI_CBCR                       0x3204
#define CAMSS_CSI3PIX_CBCR                       0x3214
#define CAMSS_ISPIF_AHB_CBCR                     0x3224
#define CAMSS_CCI_CCI_CBCR                       0x3344
#define CAMSS_CCI_CCI_AHB_CBCR                   0x3348
#define CAMSS_MCLK0_CBCR                         0x3384
#define CAMSS_MCLK1_CBCR                         0x33B4
#define CAMSS_MCLK2_CBCR                         0x33E4
#define CAMSS_MCLK3_CBCR                         0x3414
#define CAMSS_GP0_CBCR                           0x3444
#define CAMSS_GP1_CBCR                           0x3474
#define CAMSS_TOP_AHB_CBCR                       0x3484
#define CAMSS_MICRO_AHB_CBCR                     0x3494
#define CAMSS_JPEG_JPEG0_CBCR                    0x35A8
#define CAMSS_JPEG_JPEG1_CBCR                    0x35AC
#define CAMSS_JPEG_JPEG2_CBCR                    0x35B0
#define CAMSS_JPEG_JPEG_AHB_CBCR                 0x35B4
#define CAMSS_JPEG_JPEG_AXI_CBCR                 0x35B8
#define CAMSS_JPEG_JPEG_OCMEMNOC_CBCR            0x35BC
#define CAMSS_VFE_VFE0_CBCR                      0x36A8
#define CAMSS_VFE_VFE1_CBCR                      0x36AC
#define CAMSS_VFE_CPP_CBCR                       0x36B0
#define CAMSS_VFE_CPP_AHB_CBCR                   0x36B4
#define CAMSS_VFE_VFE_AHB_CBCR                   0x36B8
#define CAMSS_VFE_VFE_AXI_CBCR                   0x36BC
#define CAMSS_VFE_VFE_OCMEMNOC_CBCR              0x36C0
#define CAMSS_CSI_VFE0_CBCR                      0x3704
#define CAMSS_CSI_VFE1_CBCR                      0x3714
#define MMSS_MMSSNOC_AXI_CBCR                    0x506C
#define MMSS_MMSSNOC_AHB_CBCR                    0x5024
#define MMSS_MMSSNOC_BTO_AHB_CBCR                0x5028
#define MMSS_MISC_AHB_CBCR                       0x502C
#define MMSS_S0_AXI_CBCR                         0x5064
#define OCMEMNOC_CBCR                            0x50B4

#define MMSS_DEBUG_CLK_CTL_REG                   0x0900

#define mmpll0_mm_source_val 1
#define mmpll1_mm_source_val 2
#define mmpll3_mm_source_val 3
#define gpll0_mm_source_val 5
#define cxo_mm_source_val 0
#define mm_gnd_source_val 6
#define edp_mainlink_mm_source_val 4
#define edp_pixel_mm_source_val 5
#define edppll_350_mm_source_val 4
#define dsipll_750_mm_source_val 1
#define dsipll0_byte_mm_source_val 1
#define dsipll0_pixel_mm_source_val 1
#define hdmipll_mm_source_val 3

#define F_MM(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

#define F_EDP(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

#define F_MDSS(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
	},					\
	.num_fmax = VDD_DIG_NUM
#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_DIG_NUM
#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
	},					\
	.num_fmax = VDD_DIG_NUM

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_HIGH,
	VDD_DIG_NUM
};

static int vdd_corner[] = {
	RPM_REGULATOR_CORNER_NONE,		/* VDD_DIG_NONE */
	RPM_REGULATOR_CORNER_SVS_SOC,		/* VDD_DIG_LOW */
	RPM_REGULATOR_CORNER_NORMAL,		/* VDD_DIG_NOMINAL */
	RPM_REGULATOR_CORNER_SUPER_TURBO,	/* VDD_DIG_HIGH */
};

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

DEFINE_EXT_CLK(cxo_clk_src, NULL);
DEFINE_EXT_CLK(gpll0_clk_src, NULL);
DEFINE_EXT_CLK(mmssnoc_ahb, NULL);

static struct pll_vote_clk mmpll0_clk_src = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS_REG,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)MMPLL0_STATUS_REG,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "mmpll0_clk_src",
		.rate = 800000000,
		.ops = &clk_ops_pll_vote,
		CLK_INIT(mmpll0_clk_src.c),
	},
};

static struct pll_vote_clk mmpll1_clk_src = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS_REG,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)MMPLL1_STATUS_REG,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "mmpll1_clk_src",
		.rate = 846000000,
		.ops = &clk_ops_pll_vote,
		/* May be reassigned at runtime; alloc memory at compile time */
		VDD_DIG_FMAX_MAP1(LOW, 846000000),
		CLK_INIT(mmpll1_clk_src.c),
	},
};

static struct pll_clk mmpll3_clk_src = {
	.mode_reg = (void __iomem *)MMPLL3_MODE_REG,
	.status_reg = (void __iomem *)MMPLL3_STATUS_REG,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "mmpll3_clk_src",
		.rate = 820000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(mmpll3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mmss_axi_clk[] = {
	F_MM( 19200000,    cxo,     1,   0,   0),
	F_MM( 37500000,  gpll0,    16,   0,   0),
	F_MM( 50000000,  gpll0,    12,   0,   0),
	F_MM( 75000000,  gpll0,     8,   0,   0),
	F_MM(100000000,  gpll0,     6,   0,   0),
	F_MM(150000000,  gpll0,     4,   0,   0),
	F_MM(282000000, mmpll1,     3,   0,   0),
	F_MM(400000000, mmpll0,     2,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_mmss_axi_v2_clk[] = {
	F_MM( 19200000,    cxo,     1,   0,   0),
	F_MM( 37500000,  gpll0,    16,   0,   0),
	F_MM( 50000000,  gpll0,    12,   0,   0),
	F_MM( 75000000,  gpll0,     8,   0,   0),
	F_MM(100000000,  gpll0,     6,   0,   0),
	F_MM(150000000,  gpll0,     4,   0,   0),
	F_MM(291750000, mmpll1,     4,   0,   0),
	F_MM(400000000, mmpll0,     2,   0,   0),
	F_MM(466800000, mmpll1,   2.5,   0,   0),
	F_END
};

static struct rcg_clk axi_clk_src = {
	.cmd_rcgr_reg = 0x5040,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mmss_axi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "axi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 282000000,
				  HIGH, 400000000),
		CLK_INIT(axi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ocmemnoc_clk[] = {
	F_MM( 19200000,    cxo,   1,   0,   0),
	F_MM( 37500000,  gpll0,  16,   0,   0),
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM( 75000000,  gpll0,   8,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(282000000, mmpll1,   3,   0,   0),
	F_MM(400000000, mmpll0,   2,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_ocmemnoc_v2_clk[] = {
	F_MM( 19200000,    cxo,   1,   0,   0),
	F_MM( 37500000,  gpll0,  16,   0,   0),
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM( 75000000,  gpll0,   8,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(291750000, mmpll1,   4,   0,   0),
	F_MM(400000000, mmpll0,   2,   0,   0),
	F_END
};

struct rcg_clk ocmemnoc_clk_src = {
	.cmd_rcgr_reg = OCMEMNOC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ocmemnoc_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "ocmemnoc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 282000000,
				  HIGH, 400000000),
		CLK_INIT(ocmemnoc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_csi0_3_clk[] = {
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
	F_END
};

static struct rcg_clk csi0_clk_src = {
	.cmd_rcgr_reg = CSI0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_csi0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi0_clk_src.c),
	},
};

static struct rcg_clk csi1_clk_src = {
	.cmd_rcgr_reg = CSI1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_csi0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi1_clk_src.c),
	},
};

static struct rcg_clk csi2_clk_src = {
	.cmd_rcgr_reg = CSI2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_csi0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi2_clk_src.c),
	},
};

static struct rcg_clk csi3_clk_src = {
	.cmd_rcgr_reg = CSI3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_csi0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi3_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_vfe_vfe0_1_clk[] = {
	F_MM( 37500000,  gpll0,  16,   0,   0),
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM( 60000000,  gpll0,  10,   0,   0),
	F_MM( 80000000,  gpll0, 7.5,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(109090000,  gpll0, 5.5,   0,   0),
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(200000000,  gpll0,   3,   0,   0),
	F_MM(228570000, mmpll0, 3.5,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(320000000, mmpll0, 2.5,   0,   0),
	F_MM(465000000, mmpll3,   2,   0,   0),
	F_END
};

static struct rcg_clk vfe0_clk_src = {
	.cmd_rcgr_reg = VFE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_vfe_vfe0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vfe0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(vfe0_clk_src.c),
	},
};

static struct rcg_clk vfe1_clk_src = {
	.cmd_rcgr_reg = VFE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_vfe_vfe0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vfe1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(vfe1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_mdp_clk[] = {
	F_MM( 37500000,  gpll0,  16,   0,   0),
	F_MM( 60000000,  gpll0,  10,   0,   0),
	F_MM( 75000000,  gpll0,   8,   0,   0),
	F_MM( 85710000,  gpll0,   7,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(133330000, mmpll0,   6,   0,   0),
	F_MM(160000000, mmpll0,   5,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
	F_MM(240000000,  gpll0, 2.5,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(320000000, mmpll0, 2.5,   0,   0),
	F_END
};

static struct rcg_clk mdp_clk_src = {
	.cmd_rcgr_reg = MDP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_mdp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_cci_cci_clk[] = {
	F_MM(19200000,    cxo,   1,   0,   0),
	F_END
};

static struct rcg_clk cci_clk_src = {
	.cmd_rcgr_reg = CCI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_cci_cci_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "cci_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(cci_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_gp0_1_clk[] = {
	F_MM(   10000,    cxo,  16,   1, 120),
	F_MM(   20000,    cxo,  16,   1,  50),
	F_MM( 6000000,  gpll0,  10,   1,  10),
	F_MM(12000000,  gpll0,  10,   1,   5),
	F_MM(13000000,  gpll0,  10,  13,  60),
	F_MM(24000000,  gpll0,   5,   1,   5),
	F_END
};

static struct rcg_clk mmss_gp0_clk_src = {
	.cmd_rcgr_reg = MMSS_GP0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmss_gp0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(mmss_gp0_clk_src.c),
	},
};

static struct rcg_clk mmss_gp1_clk_src = {
	.cmd_rcgr_reg = MMSS_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmss_gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(mmss_gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_jpeg_jpeg0_2_clk[] = {
	F_MM( 75000000,  gpll0,   8,   0,   0),
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(200000000,  gpll0,   3,   0,   0),
	F_MM(228570000, mmpll0, 3.5,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(320000000, mmpll0, 2.5,   0,   0),
	F_END
};

static struct rcg_clk jpeg0_clk_src = {
	.cmd_rcgr_reg = JPEG0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_jpeg_jpeg0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "jpeg0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(jpeg0_clk_src.c),
	},
};

static struct rcg_clk jpeg1_clk_src = {
	.cmd_rcgr_reg = JPEG1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_jpeg_jpeg0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "jpeg1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(jpeg1_clk_src.c),
	},
};

static struct rcg_clk jpeg2_clk_src = {
	.cmd_rcgr_reg = JPEG2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_jpeg_jpeg0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "jpeg2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(jpeg2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_mclk0_3_clk[] = {
	F_MM(19200000,    cxo,   1,   0,   0),
	F_MM(66670000,  gpll0,   9,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_camss_mclk0_3_pro_clk[] = {
	F_MM( 4800000,    cxo,    4,   0,   0),
	F_MM( 6000000,  gpll0,   10,   1,  10),
	F_MM( 8000000,  gpll0,   15,   1,   5),
	F_MM( 9600000,    cxo,    2,   0,   0),
	F_MM(16000000,  gpll0, 12.5,   1,   3),
	F_MM(19200000,    cxo,    1,   0,   0),
	F_MM(24000000,  gpll0,    5,   1,   5),
	F_MM(32000000, mmpll0,    5,   1,   5),
	F_MM(48000000,  gpll0, 12.5,   0,   0),
	F_MM(64000000, mmpll0, 12.5,   0,   0),
	F_MM(66670000,  gpll0,    9,   0,   0),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg = MCLK0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk0_clk_src.c),
	},
};

static struct rcg_clk mclk1_clk_src = {
	.cmd_rcgr_reg = MCLK1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk1_clk_src.c),
	},
};

static struct rcg_clk mclk2_clk_src = {
	.cmd_rcgr_reg = MCLK2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk2_clk_src.c),
	},
};

static struct rcg_clk mclk3_clk_src = {
	.cmd_rcgr_reg = MCLK3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk3_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_phy0_2_csi0_2phytimer_clk[] = {
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
	F_END
};

static struct rcg_clk csi0phytimer_clk_src = {
	.cmd_rcgr_reg = CSI0PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_phy0_2_csi0_2phytimer_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi0phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi0phytimer_clk_src.c),
	},
};

static struct rcg_clk csi1phytimer_clk_src = {
	.cmd_rcgr_reg = CSI1PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_phy0_2_csi0_2phytimer_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi1phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi1phytimer_clk_src.c),
	},
};

static struct rcg_clk csi2phytimer_clk_src = {
	.cmd_rcgr_reg = CSI2PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_phy0_2_csi0_2phytimer_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi2phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi2phytimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_vfe_cpp_clk[] = {
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(320000000, mmpll0, 2.5,   0,   0),
	F_MM(465000000, mmpll3,   2,   0,   0),
	F_END
};

static struct rcg_clk cpp_clk_src = {
	.cmd_rcgr_reg = CPP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_vfe_cpp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "cpp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(cpp_clk_src.c),
	},
};

static struct clk_freq_tbl byte_freq_tbl[] = {
	{
		.src_clk = &byte_clk_src_8974.c,
		.div_src_val = BVAL(10, 8, dsipll0_byte_mm_source_val),
	},
	F_END
};

static struct rcg_clk byte0_clk_src = {
	.cmd_rcgr_reg = BYTE0_CMD_RCGR,
	.current_freq = byte_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte_clk_src_8974.c,
		.dbg_name = "byte0_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP3(LOW, 93800000, NOMINAL, 187500000,
				  HIGH, 188000000),
		CLK_INIT(byte0_clk_src.c),
	},
};

static struct rcg_clk byte1_clk_src = {
	.cmd_rcgr_reg = BYTE1_CMD_RCGR,
	.current_freq = byte_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte_clk_src_8974.c,
		.dbg_name = "byte1_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP3(LOW, 93800000, NOMINAL, 187500000,
				  HIGH, 188000000),
		CLK_INIT(byte1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_edpaux_clk[] = {
	F_MM(19200000,    cxo,   1,   0,   0),
	F_END
};

static struct rcg_clk edpaux_clk_src = {
	.cmd_rcgr_reg = EDPAUX_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_edpaux_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "edpaux_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(edpaux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_edplink_clk[] = {
	F_EDP(162000000, edp_mainlink,  1,   0,   0),
	F_EDP(270000000, edp_mainlink,  1,   0,   0),
	F_END
};

static struct rcg_clk edplink_clk_src = {
	.cmd_rcgr_reg = EDPLINK_CMD_RCGR,
	.freq_tbl = ftbl_mdss_edplink_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "edplink_clk_src",
		.ops = &clk_ops_rcg_edp,
		VDD_DIG_FMAX_MAP2(LOW, 135000000, NOMINAL, 270000000),
		CLK_INIT(edplink_clk_src.c),
	},
};

static struct clk_freq_tbl edp_pixel_freq_tbl[] = {
	{
		.src_clk = &edp_pixel_clk_src.c,
		.div_src_val = BVAL(10, 8, edp_pixel_mm_source_val)
				| BVAL(4, 0, 0),
	},
	F_END
};

static struct rcg_clk edppixel_clk_src = {
	.cmd_rcgr_reg = EDPPIXEL_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = edp_pixel_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &edp_pixel_clk_src.c,
		.dbg_name = "edppixel_clk_src",
		.ops = &clk_ops_edppixel,
		VDD_DIG_FMAX_MAP2(LOW, 175000000, NOMINAL, 350000000),
		CLK_INIT(edppixel_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_esc0_1_clk[] = {
	F_MM(19200000,    cxo,   1,   0,   0),
	F_END
};

static struct rcg_clk esc0_clk_src = {
	.cmd_rcgr_reg = ESC0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_esc0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "esc0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(esc0_clk_src.c),
	},
};

static struct rcg_clk esc1_clk_src = {
	.cmd_rcgr_reg = ESC1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_esc0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "esc1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(esc1_clk_src.c),
	},
};

static struct clk_freq_tbl exptclk_freq_tbl[] = {
	{
		.src_clk = &hdmipll_clk_src.c,
		.div_src_val = BVAL(10, 8, hdmipll_mm_source_val),
	},
	F_END
};

static struct rcg_clk extpclk_clk_src = {
	.cmd_rcgr_reg = EXTPCLK_CMD_RCGR,
	.current_freq = exptclk_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "extpclk_clk_src",
		.parent = &hdmipll_clk_src.c,
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP2(LOW, 148500000, NOMINAL, 297000000),
		CLK_INIT(extpclk_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_hdmi_clk[] = {
	F_MDSS(19200000,    cxo,   1,   0,   0),
	F_END
};

static struct rcg_clk hdmi_clk_src = {
	.cmd_rcgr_reg = HDMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_hdmi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "hdmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(hdmi_clk_src.c),
	},
};

static struct clk_freq_tbl pixel_freq_tbl[] = {
	{
		.src_clk = &pixel_clk_src_8974.c,
		.div_src_val = BVAL(10, 8, dsipll0_pixel_mm_source_val)
				| BVAL(4, 0, 0),
	},
	F_END
};

static struct rcg_clk pclk0_clk_src = {
	.cmd_rcgr_reg = PCLK0_CMD_RCGR,
	.current_freq = pixel_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pixel_clk_src_8974.c,
		.dbg_name = "pclk0_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 125000000, NOMINAL, 250000000),
		CLK_INIT(pclk0_clk_src.c),
	},
};

static struct rcg_clk pclk1_clk_src = {
	.cmd_rcgr_reg = PCLK1_CMD_RCGR,
	.current_freq = pixel_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pixel_clk_src_8974.c,
		.dbg_name = "pclk1_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 125000000, NOMINAL, 250000000),
		CLK_INIT(pclk1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_vsync_clk[] = {
	F_MDSS(19200000,    cxo,   1,   0,   0),
	F_END
};

static struct rcg_clk vsync_clk_src = {
	.cmd_rcgr_reg = VSYNC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_vsync_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vsync_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(vsync_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_venus0_vcodec0_clk[] = {
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(133330000, mmpll0,   6,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(410000000, mmpll3,   2,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_venus0_vcodec0_v2_clk[] = {
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(133330000, mmpll0,   6,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(465000000, mmpll3,   2,   0,   0),
	F_END
};

static struct rcg_clk vcodec0_clk_src = {
	.cmd_rcgr_reg = VCODEC0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_venus0_vcodec0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vcodec0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 410000000),
		CLK_INIT(vcodec0_clk_src.c),
	},
};

static struct branch_clk camss_cci_cci_ahb_clk = {
	.cbcr_reg = CAMSS_CCI_CCI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_cci_cci_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cci_cci_ahb_clk.c),
	},
};

static struct branch_clk camss_cci_cci_clk = {
	.cbcr_reg = CAMSS_CCI_CCI_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cci_clk_src.c,
		.dbg_name = "camss_cci_cci_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cci_cci_clk.c),
	},
};

static struct branch_clk camss_csi0_ahb_clk = {
	.cbcr_reg = CAMSS_CSI0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0_ahb_clk.c),
	},
};

static struct branch_clk camss_csi0_clk = {
	.cbcr_reg = CAMSS_CSI0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0_clk_src.c,
		.dbg_name = "camss_csi0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0_clk.c),
	},
};

static struct branch_clk camss_csi0phy_clk = {
	.cbcr_reg = CAMSS_CSI0PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0_clk_src.c,
		.dbg_name = "camss_csi0phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0phy_clk.c),
	},
};

static struct branch_clk camss_csi0pix_clk = {
	.cbcr_reg = CAMSS_CSI0PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0_clk_src.c,
		.dbg_name = "camss_csi0pix_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0pix_clk.c),
	},
};

static struct branch_clk camss_csi0rdi_clk = {
	.cbcr_reg = CAMSS_CSI0RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0_clk_src.c,
		.dbg_name = "camss_csi0rdi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0rdi_clk.c),
	},
};

static struct branch_clk camss_csi1_ahb_clk = {
	.cbcr_reg = CAMSS_CSI1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1_ahb_clk.c),
	},
};

static struct branch_clk camss_csi1_clk = {
	.cbcr_reg = CAMSS_CSI1_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1_clk_src.c,
		.dbg_name = "camss_csi1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1_clk.c),
	},
};

static struct branch_clk camss_csi1phy_clk = {
	.cbcr_reg = CAMSS_CSI1PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1_clk_src.c,
		.dbg_name = "camss_csi1phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1phy_clk.c),
	},
};

static struct branch_clk camss_csi1pix_clk = {
	.cbcr_reg = CAMSS_CSI1PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1_clk_src.c,
		.dbg_name = "camss_csi1pix_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1pix_clk.c),
	},
};

static struct branch_clk camss_csi1rdi_clk = {
	.cbcr_reg = CAMSS_CSI1RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1_clk_src.c,
		.dbg_name = "camss_csi1rdi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1rdi_clk.c),
	},
};

static struct branch_clk camss_csi2_ahb_clk = {
	.cbcr_reg = CAMSS_CSI2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2_ahb_clk.c),
	},
};

static struct branch_clk camss_csi2_clk = {
	.cbcr_reg = CAMSS_CSI2_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2_clk_src.c,
		.dbg_name = "camss_csi2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2_clk.c),
	},
};

static struct branch_clk camss_csi2phy_clk = {
	.cbcr_reg = CAMSS_CSI2PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2_clk_src.c,
		.dbg_name = "camss_csi2phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2phy_clk.c),
	},
};

static struct branch_clk camss_csi2pix_clk = {
	.cbcr_reg = CAMSS_CSI2PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2_clk_src.c,
		.dbg_name = "camss_csi2pix_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2pix_clk.c),
	},
};

static struct branch_clk camss_csi2rdi_clk = {
	.cbcr_reg = CAMSS_CSI2RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2_clk_src.c,
		.dbg_name = "camss_csi2rdi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2rdi_clk.c),
	},
};

static struct branch_clk camss_csi3_ahb_clk = {
	.cbcr_reg = CAMSS_CSI3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3_ahb_clk.c),
	},
};

static struct branch_clk camss_csi3_clk = {
	.cbcr_reg = CAMSS_CSI3_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi3_clk_src.c,
		.dbg_name = "camss_csi3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3_clk.c),
	},
};

static struct branch_clk camss_csi3phy_clk = {
	.cbcr_reg = CAMSS_CSI3PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi3_clk_src.c,
		.dbg_name = "camss_csi3phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3phy_clk.c),
	},
};

static struct branch_clk camss_csi3pix_clk = {
	.cbcr_reg = CAMSS_CSI3PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi3_clk_src.c,
		.dbg_name = "camss_csi3pix_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3pix_clk.c),
	},
};

static struct branch_clk camss_csi3rdi_clk = {
	.cbcr_reg = CAMSS_CSI3RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi3_clk_src.c,
		.dbg_name = "camss_csi3rdi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3rdi_clk.c),
	},
};

static struct branch_clk camss_csi_vfe0_clk = {
	.cbcr_reg = CAMSS_CSI_VFE0_CBCR,
	.bcr_reg = CAMSS_CSI_VFE0_BCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vfe0_clk_src.c,
		.dbg_name = "camss_csi_vfe0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi_vfe0_clk.c),
	},
};

static struct branch_clk camss_csi_vfe1_clk = {
	.cbcr_reg = CAMSS_CSI_VFE1_CBCR,
	.bcr_reg = CAMSS_CSI_VFE1_BCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vfe1_clk_src.c,
		.dbg_name = "camss_csi_vfe1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi_vfe1_clk.c),
	},
};

static struct branch_clk camss_gp0_clk = {
	.cbcr_reg = CAMSS_GP0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mmss_gp0_clk_src.c,
		.dbg_name = "camss_gp0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_gp0_clk.c),
	},
};

static struct branch_clk camss_gp1_clk = {
	.cbcr_reg = CAMSS_GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mmss_gp1_clk_src.c,
		.dbg_name = "camss_gp1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_gp1_clk.c),
	},
};

static struct branch_clk camss_ispif_ahb_clk = {
	.cbcr_reg = CAMSS_ISPIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_ispif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_ispif_ahb_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg0_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG0_CBCR,
	.bcr_reg = CAMSS_JPEG_BCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &jpeg0_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg0_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg1_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &jpeg1_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg1_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg2_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &jpeg2_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg2_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg_ahb_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_jpeg_jpeg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_ahb_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg_axi_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_axi_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg_ocmemnoc_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg_ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_ocmemnoc_clk.c),
	},
};

static struct branch_clk camss_mclk0_clk = {
	.cbcr_reg = CAMSS_MCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mclk0_clk_src.c,
		.dbg_name = "camss_mclk0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk0_clk.c),
	},
};

static struct branch_clk camss_mclk1_clk = {
	.cbcr_reg = CAMSS_MCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mclk1_clk_src.c,
		.dbg_name = "camss_mclk1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk1_clk.c),
	},
};

static struct branch_clk camss_mclk2_clk = {
	.cbcr_reg = CAMSS_MCLK2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mclk2_clk_src.c,
		.dbg_name = "camss_mclk2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk2_clk.c),
	},
};

static struct branch_clk camss_mclk3_clk = {
	.cbcr_reg = CAMSS_MCLK3_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mclk3_clk_src.c,
		.dbg_name = "camss_mclk3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk3_clk.c),
	},
};

static struct branch_clk camss_micro_ahb_clk = {
	.cbcr_reg = CAMSS_MICRO_AHB_CBCR,
	.has_sibling = 1,
	.bcr_reg = CAMSS_MICRO_BCR,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_micro_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_micro_ahb_clk.c),
	},
};

static struct branch_clk camss_phy0_csi0phytimer_clk = {
	.cbcr_reg = CAMSS_PHY0_CSI0PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0phytimer_clk_src.c,
		.dbg_name = "camss_phy0_csi0phytimer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy0_csi0phytimer_clk.c),
	},
};

static struct branch_clk camss_phy1_csi1phytimer_clk = {
	.cbcr_reg = CAMSS_PHY1_CSI1PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1phytimer_clk_src.c,
		.dbg_name = "camss_phy1_csi1phytimer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy1_csi1phytimer_clk.c),
	},
};

static struct branch_clk camss_phy2_csi2phytimer_clk = {
	.cbcr_reg = CAMSS_PHY2_CSI2PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2phytimer_clk_src.c,
		.dbg_name = "camss_phy2_csi2phytimer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy2_csi2phytimer_clk.c),
	},
};

static struct branch_clk camss_top_ahb_clk = {
	.cbcr_reg = CAMSS_TOP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_top_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_top_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_cpp_ahb_clk = {
	.cbcr_reg = CAMSS_VFE_CPP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_vfe_cpp_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_cpp_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_cpp_clk = {
	.cbcr_reg = CAMSS_VFE_CPP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cpp_clk_src.c,
		.dbg_name = "camss_vfe_cpp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_cpp_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe0_clk = {
	.cbcr_reg = CAMSS_VFE_VFE0_CBCR,
	.bcr_reg = CAMSS_VFE_BCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vfe0_clk_src.c,
		.dbg_name = "camss_vfe_vfe0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe0_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe1_clk = {
	.cbcr_reg = CAMSS_VFE_VFE1_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vfe1_clk_src.c,
		.dbg_name = "camss_vfe_vfe1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe1_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe_ahb_clk = {
	.cbcr_reg = CAMSS_VFE_VFE_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_vfe_vfe_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe_axi_clk = {
	.cbcr_reg = CAMSS_VFE_VFE_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "camss_vfe_vfe_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe_axi_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe_ocmemnoc_clk = {
	.cbcr_reg = CAMSS_VFE_VFE_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "camss_vfe_vfe_ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe_ocmemnoc_clk.c),
	},
};

static struct branch_clk mdss_ahb_clk = {
	.cbcr_reg = MDSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_ahb_clk.c),
	},
};

static struct branch_clk mdss_axi_clk = {
	.cbcr_reg = MDSS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "mdss_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_axi_clk.c),
	},
};

static struct branch_clk mdss_byte0_clk = {
	.cbcr_reg = MDSS_BYTE0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte0_clk_src.c,
		.dbg_name = "mdss_byte0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_byte0_clk.c),
	},
};

static struct branch_clk mdss_byte1_clk = {
	.cbcr_reg = MDSS_BYTE1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte1_clk_src.c,
		.dbg_name = "mdss_byte1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_byte1_clk.c),
	},
};

static struct branch_clk mdss_edpaux_clk = {
	.cbcr_reg = MDSS_EDPAUX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &edpaux_clk_src.c,
		.dbg_name = "mdss_edpaux_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_edpaux_clk.c),
	},
};

static struct branch_clk mdss_edplink_clk = {
	.cbcr_reg = MDSS_EDPLINK_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &edplink_clk_src.c,
		.dbg_name = "mdss_edplink_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_edplink_clk.c),
	},
};

static struct branch_clk mdss_edppixel_clk = {
	.cbcr_reg = MDSS_EDPPIXEL_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &edppixel_clk_src.c,
		.dbg_name = "mdss_edppixel_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_edppixel_clk.c),
	},
};

static struct branch_clk mdss_esc0_clk = {
	.cbcr_reg = MDSS_ESC0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &esc0_clk_src.c,
		.dbg_name = "mdss_esc0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_esc0_clk.c),
	},
};

static struct branch_clk mdss_esc1_clk = {
	.cbcr_reg = MDSS_ESC1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &esc1_clk_src.c,
		.dbg_name = "mdss_esc1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_esc1_clk.c),
	},
};

static struct branch_clk mdss_extpclk_clk = {
	.cbcr_reg = MDSS_EXTPCLK_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &extpclk_clk_src.c,
		.dbg_name = "mdss_extpclk_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_extpclk_clk.c),
	},
};

static struct branch_clk mdss_hdmi_ahb_clk = {
	.cbcr_reg = MDSS_HDMI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_hdmi_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_hdmi_ahb_clk.c),
	},
};

static struct branch_clk mdss_hdmi_clk = {
	.cbcr_reg = MDSS_HDMI_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &hdmi_clk_src.c,
		.dbg_name = "mdss_hdmi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_hdmi_clk.c),
	},
};

static struct branch_clk mdss_mdp_clk = {
	.cbcr_reg = MDSS_MDP_CBCR,
	.bcr_reg = MDSS_BCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mdp_clk_src.c,
		.dbg_name = "mdss_mdp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_clk.c),
	},
};

static struct branch_clk mdss_mdp_lut_clk = {
	.cbcr_reg = MDSS_MDP_LUT_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mdp_clk_src.c,
		.dbg_name = "mdss_mdp_lut_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_lut_clk.c),
	},
};

static struct branch_clk mdss_pclk0_clk = {
	.cbcr_reg = MDSS_PCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pclk0_clk_src.c,
		.dbg_name = "mdss_pclk0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_pclk0_clk.c),
	},
};

static struct branch_clk mdss_pclk1_clk = {
	.cbcr_reg = MDSS_PCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pclk1_clk_src.c,
		.dbg_name = "mdss_pclk1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_pclk1_clk.c),
	},
};

static struct branch_clk mdss_vsync_clk = {
	.cbcr_reg = MDSS_VSYNC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vsync_clk_src.c,
		.dbg_name = "mdss_vsync_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_vsync_clk.c),
	},
};

static struct branch_clk mmss_misc_ahb_clk = {
	.cbcr_reg = MMSS_MISC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmss_misc_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_misc_ahb_clk.c),
	},
};

static struct branch_clk mmss_mmssnoc_axi_clk = {
	.cbcr_reg = MMSS_MMSSNOC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "mmss_mmssnoc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mmssnoc_axi_clk.c),
	},
};

static struct branch_clk mmss_s0_axi_clk = {
	.cbcr_reg = MMSS_S0_AXI_CBCR,
	/* The bus driver needs set_rate to go through to the parent */
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "mmss_s0_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_s0_axi_clk.c),
		.depends = &mmss_mmssnoc_axi_clk.c,
	},
};

struct branch_clk ocmemnoc_clk = {
	.cbcr_reg = OCMEMNOC_CBCR,
	.has_sibling = 0,
	.bcr_reg = 0x50b0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ocmemnoc_clk.c),
	},
};

struct branch_clk ocmemcx_ocmemnoc_clk = {
	.cbcr_reg = OCMEMCX_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "ocmemcx_ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ocmemcx_ocmemnoc_clk.c),
	},
};

static struct branch_clk venus0_ahb_clk = {
	.cbcr_reg = VENUS0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "venus0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_ahb_clk.c),
	},
};

static struct branch_clk venus0_axi_clk = {
	.cbcr_reg = VENUS0_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "venus0_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_axi_clk.c),
	},
};

static struct branch_clk venus0_ocmemnoc_clk = {
	.cbcr_reg = VENUS0_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "venus0_ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_ocmemnoc_clk.c),
	},
};

static struct branch_clk venus0_vcodec0_clk = {
	.cbcr_reg = VENUS0_VCODEC0_CBCR,
	.bcr_reg = VENUS0_BCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vcodec0_clk_src.c,
		.dbg_name = "venus0_vcodec0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_vcodec0_clk.c),
	},
};

static struct branch_clk oxilicx_axi_clk = {
	.cbcr_reg = OXILICX_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "oxilicx_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(oxilicx_axi_clk.c),
	},
};

static struct branch_clk oxili_gfx3d_clk = {
	.cbcr_reg = OXILI_GFX3D_CBCR,
	.bcr_reg = OXILI_BCR,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "oxili_gfx3d_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(oxili_gfx3d_clk.c),
		.depends = &oxilicx_axi_clk.c,
	},
};

static struct branch_clk oxilicx_ahb_clk = {
	.cbcr_reg = OXILICX_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "oxilicx_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(oxilicx_ahb_clk.c),
	},
};

static struct pll_config_regs mmpll0_regs = {
	.l_reg = (void __iomem *)MMPLL0_L_REG,
	.m_reg = (void __iomem *)MMPLL0_M_REG,
	.n_reg = (void __iomem *)MMPLL0_N_REG,
	.config_reg = (void __iomem *)MMPLL0_USER_CTL_REG,
	.mode_reg = (void __iomem *)MMPLL0_MODE_REG,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL0 at 800 MHz, main output enabled. */
static struct pll_config mmpll0_config = {
	.l = 0x29,
	.m = 0x2,
	.n = 0x3,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs mmpll1_regs = {
	.l_reg = (void __iomem *)MMPLL1_L_REG,
	.m_reg = (void __iomem *)MMPLL1_M_REG,
	.n_reg = (void __iomem *)MMPLL1_N_REG,
	.config_reg = (void __iomem *)MMPLL1_USER_CTL_REG,
	.mode_reg = (void __iomem *)MMPLL1_MODE_REG,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL1 at 846 MHz, main output enabled. */
static struct pll_config mmpll1_config = {
	.l = 0x2C,
	.m = 0x1,
	.n = 0x10,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

/* MMPLL1 at 1167 MHz, main output enabled. */
static struct pll_config mmpll1_v2_config = {
	.l = 60,
	.m = 25,
	.n = 32,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs mmpll3_regs = {
	.l_reg = (void __iomem *)MMPLL3_L_REG,
	.m_reg = (void __iomem *)MMPLL3_M_REG,
	.n_reg = (void __iomem *)MMPLL3_N_REG,
	.config_reg = (void __iomem *)MMPLL3_USER_CTL_REG,
	.mode_reg = (void __iomem *)MMPLL3_MODE_REG,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL3 at 820 MHz, main output enabled. */
static struct pll_config mmpll3_config = {
	.l = 0x2A,
	.m = 0x11,
	.n = 0x18,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

/* MMPLL3 at 930 MHz, main output enabled. */
static struct pll_config mmpll3_v2_config = {
	.l = 48,
	.m = 7,
	.n = 16,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static int mmss_dbg_set_mux_sel(struct mux_clk *clk, int sel)
{
	unsigned long flags;
	u32 regval;

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Set debug mux clock index */
	regval = BVAL(11, 0, sel);
	writel_relaxed(regval, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL_REG));

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

static int mmss_dbg_get_mux_sel(struct mux_clk *clk)
{
	return readl_relaxed(MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL_REG)) & BM(11, 0);
}

static int mmss_dbg_mux_enable(struct mux_clk *clk)
{
	u32 regval;

	regval = readl_relaxed(MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL_REG));
	writel_relaxed(regval | BIT(16), MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL_REG));
	mb();

	return 0;
}

static void mmss_dbg_mux_disable(struct mux_clk *clk)
{
	u32 regval;

	regval = readl_relaxed(MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL_REG));
	writel_relaxed(regval & ~BIT(16),
		       MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL_REG));
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
		{&mmssnoc_ahb.c,		   0x0001},
		{&mmss_mmssnoc_axi_clk.c,	   0x0004},
		{&ocmemnoc_clk.c,		   0x0007},
		{&ocmemcx_ocmemnoc_clk.c,	   0x0009},
		{&camss_cci_cci_ahb_clk.c,	   0x002e},
		{&camss_cci_cci_clk.c,		   0x002d},
		{&camss_csi0_ahb_clk.c,		   0x0042},
		{&camss_csi0_clk.c,		   0x0041},
		{&camss_csi0phy_clk.c,		   0x0043},
		{&camss_csi0pix_clk.c,		   0x0045},
		{&camss_csi0rdi_clk.c,		   0x0044},
		{&camss_csi1_ahb_clk.c,		   0x0047},
		{&camss_csi1_clk.c,		   0x0046},
		{&camss_csi1phy_clk.c,		   0x0048},
		{&camss_csi1pix_clk.c,		   0x004a},
		{&camss_csi1rdi_clk.c,		   0x0049},
		{&camss_csi2_ahb_clk.c,		   0x004c},
		{&camss_csi2_clk.c,		   0x004b},
		{&camss_csi2phy_clk.c,		   0x004d},
		{&camss_csi2pix_clk.c,		   0x004f},
		{&camss_csi2rdi_clk.c,		   0x004e},
		{&camss_csi3_ahb_clk.c,		   0x0051},
		{&camss_csi3_clk.c,		   0x0050},
		{&camss_csi3phy_clk.c,		   0x0052},
		{&camss_csi3pix_clk.c,		   0x0054},
		{&camss_csi3rdi_clk.c,		   0x0053},
		{&camss_csi_vfe0_clk.c,		   0x003f},
		{&camss_csi_vfe1_clk.c,		   0x0040},
		{&camss_gp0_clk.c,		   0x0027},
		{&camss_gp1_clk.c,		   0x0028},
		{&camss_ispif_ahb_clk.c,	   0x0055},
		{&camss_jpeg_jpeg0_clk.c,	   0x0032},
		{&camss_jpeg_jpeg1_clk.c,	   0x0033},
		{&camss_jpeg_jpeg2_clk.c,	   0x0034},
		{&camss_jpeg_jpeg_ahb_clk.c,	   0x0035},
		{&camss_jpeg_jpeg_axi_clk.c,	   0x0036},
		{&camss_jpeg_jpeg_ocmemnoc_clk.c,  0x0037},
		{&camss_mclk0_clk.c,		   0x0029},
		{&camss_mclk1_clk.c,		   0x002a},
		{&camss_mclk2_clk.c,		   0x002b},
		{&camss_mclk3_clk.c,		   0x002c},
		{&camss_micro_ahb_clk.c,	   0x0026},
		{&camss_phy0_csi0phytimer_clk.c,   0x002f},
		{&camss_phy1_csi1phytimer_clk.c,   0x0030},
		{&camss_phy2_csi2phytimer_clk.c,   0x0031},
		{&camss_top_ahb_clk.c,		   0x0025},
		{&camss_vfe_cpp_ahb_clk.c,	   0x003b},
		{&camss_vfe_cpp_clk.c,		   0x003a},
		{&camss_vfe_vfe0_clk.c,		   0x0038},
		{&camss_vfe_vfe1_clk.c,		   0x0039},
		{&camss_vfe_vfe_ahb_clk.c,	   0x003c},
		{&camss_vfe_vfe_axi_clk.c,	   0x003d},
		{&camss_vfe_vfe_ocmemnoc_clk.c,	   0x003e},
		{&oxilicx_axi_clk.c,		   0x000b},
		{&oxilicx_ahb_clk.c,		   0x000c},
		{&ocmemcx_ocmemnoc_clk.c,	   0x0009},
		{&oxili_gfx3d_clk.c,		   0x000d},
		{&venus0_axi_clk.c,		   0x000f},
		{&venus0_ocmemnoc_clk.c,	   0x0010},
		{&venus0_ahb_clk.c,		   0x0011},
		{&venus0_vcodec0_clk.c,		   0x000e},
		{&mmss_s0_axi_clk.c,		   0x0005},
		{&mdss_ahb_clk.c,		   0x0022},
		{&mdss_hdmi_clk.c,		   0x001d},
		{&mdss_mdp_clk.c,		   0x0014},
		{&mdss_mdp_lut_clk.c,		   0x0015},
		{&mdss_axi_clk.c,		   0x0024},
		{&mdss_vsync_clk.c,		   0x001c},
		{&mdss_esc0_clk.c,		   0x0020},
		{&mdss_esc1_clk.c,		   0x0021},
		{&mdss_edpaux_clk.c,		   0x001b},
		{&mdss_byte0_clk.c,		   0x001e},
		{&mdss_byte1_clk.c,		   0x001f},
		{&mdss_edplink_clk.c,		   0x001a},
		{&mdss_edppixel_clk.c,		   0x0019},
		{&mdss_extpclk_clk.c,		   0x0018},
		{&mdss_hdmi_ahb_clk.c,		   0x0023},
		{&mdss_pclk0_clk.c,		   0x0016},
		{&mdss_pclk1_clk.c,		   0x0017},
	),
	.c = {
		.dbg_name = "mmss_debug_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mmss_debug_mux.c),
	},
};

static struct clk_lookup msm_camera_clocks_8974pro_only[] = {
	CLK_LOOKUP_OF("cam_src_clk", mclk1_clk_src, "90.qcom,camera"),
	CLK_LOOKUP_OF("cam_clk", camss_mclk1_clk, "90.qcom,camera"),
	CLK_LOOKUP_OF("cam_src_clk", mclk0_clk_src, "0.qcom,camera"),
	CLK_LOOKUP_OF("cam_src_clk", mclk1_clk_src, "1.qcom,camera"),
	CLK_LOOKUP_OF("cam_src_clk", mclk2_clk_src, "2.qcom,camera"),
	CLK_LOOKUP_OF("cam_clk", camss_mclk0_clk, "0.qcom,camera"),
	CLK_LOOKUP_OF("cam_clk", camss_mclk1_clk, "1.qcom,camera"),
	CLK_LOOKUP_OF("cam_clk", camss_mclk2_clk, "2.qcom,camera"),
};

static struct clk_lookup msm_camera_clocks_8974_only[] = {
	CLK_LOOKUP_OF("cam_src_clk", mmss_gp1_clk_src, "90.qcom,camera"),
	CLK_LOOKUP_OF("cam_clk", camss_gp1_clk, "90.qcom,camera"),
	CLK_LOOKUP_OF("cam_src_clk", mmss_gp0_clk_src, "0.qcom,camera"),
	CLK_LOOKUP_OF("cam_src_clk", mmss_gp1_clk_src, "1.qcom,camera"),
	CLK_LOOKUP_OF("cam_clk", camss_gp0_clk, "0.qcom,camera"),
	CLK_LOOKUP_OF("cam_clk", camss_gp1_clk, "1.qcom,camera"),
};

static struct clk_lookup msm_clocks_mmss_8974[] = {
	CLK_LOOKUP_OF("mmss_debug_mux", mmss_debug_mux,
			"fc401880.qcom,cc-debug"),
	CLK_LOOKUP_OF("bus_clk_src", axi_clk_src, ""),
	CLK_LOOKUP_OF("bus_clk", mmss_mmssnoc_axi_clk, ""),
	CLK_LOOKUP_OF("core_clk", mdss_edpaux_clk, "fd923400.qcom,mdss_edp"),
	CLK_LOOKUP_OF("pixel_clk", mdss_edppixel_clk, "fd923400.qcom,mdss_edp"),
	CLK_LOOKUP_OF("link_clk", mdss_edplink_clk, "fd923400.qcom,mdss_edp"),
	CLK_LOOKUP_OF("mdp_core_clk", mdss_mdp_clk, "fd923400.qcom,mdss_edp"),
	CLK_LOOKUP_OF("byte_clk", mdss_byte0_clk, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("byte_clk", mdss_byte1_clk, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("core_clk", mdss_esc0_clk, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("core_clk", mdss_esc1_clk, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("iface_clk", mdss_ahb_clk, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("iface_clk", mdss_ahb_clk, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("bus_clk", mdss_axi_clk, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("bus_clk", mdss_axi_clk, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("pixel_clk", mdss_pclk0_clk, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("pixel_clk", mdss_pclk1_clk, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("mdp_core_clk", mdss_mdp_clk, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("mdp_core_clk", mdss_mdp_clk, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("core_mmss_clk", mmss_misc_ahb_clk.c,
		"fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("core_mmss_clk", mmss_misc_ahb_clk.c,
		"fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP_OF("iface_clk", mdss_ahb_clk, "fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP_OF("alt_iface_clk", mdss_hdmi_ahb_clk,
		"fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP_OF("core_clk", mdss_hdmi_clk, "fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP_OF("mdp_core_clk", mdss_mdp_clk, "fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP_OF("extp_clk", mdss_extpclk_clk, "fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP_OF("core_clk", mdss_mdp_clk, "mdp.0"),
	CLK_LOOKUP_OF("lut_clk", mdss_mdp_lut_clk, "mdp.0"),
	CLK_LOOKUP_OF("core_clk_src", mdp_clk_src, "mdp.0"),
	CLK_LOOKUP_OF("vsync_clk", mdss_vsync_clk, "mdp.0"),

	/* MM sensor clocks placeholder */
	CLK_LOOKUP_OF("", camss_mclk0_clk, ""),
	CLK_LOOKUP_OF("", camss_mclk1_clk, ""),
	CLK_LOOKUP_OF("", camss_mclk2_clk, ""),
	CLK_LOOKUP_OF("", camss_mclk3_clk, ""),
	CLK_LOOKUP_OF("", mmss_gp0_clk_src, ""),
	CLK_LOOKUP_OF("", mmss_gp1_clk_src, ""),
	CLK_LOOKUP_OF("", camss_gp0_clk, ""),
	CLK_LOOKUP_OF("", camss_gp1_clk, ""),

	/* CCI clocks */
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
		"fda0c000.qcom,cci"),
	CLK_LOOKUP_OF("cci_ahb_clk", camss_cci_cci_ahb_clk,
						  "fda0c000.qcom,cci"),
	CLK_LOOKUP_OF("cci_src_clk", cci_clk_src, "fda0c000.qcom,cci"),
	CLK_LOOKUP_OF("cci_clk", camss_cci_cci_clk, "fda0c000.qcom,cci"),
	/* CSIPHY clocks */
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
		"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP_OF("ispif_ahb_clk", camss_ispif_ahb_clk,
		"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP_OF("csiphy_timer_src_clk", csi0phytimer_clk_src,
		"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP_OF("csiphy_timer_clk", camss_phy0_csi0phytimer_clk,
		"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
		"fda0b000.qcom,csiphy"),
	CLK_LOOKUP_OF("ispif_ahb_clk", camss_ispif_ahb_clk,
		"fda0b000.qcom,csiphy"),
	CLK_LOOKUP_OF("csiphy_timer_src_clk", csi1phytimer_clk_src,
		"fda0b000.qcom,csiphy"),
	CLK_LOOKUP_OF("csiphy_timer_clk", camss_phy1_csi1phytimer_clk,
		"fda0b000.qcom,csiphy"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
		"fda0b400.qcom,csiphy"),
	CLK_LOOKUP_OF("ispif_ahb_clk", camss_ispif_ahb_clk,
		"fda0b400.qcom,csiphy"),
	CLK_LOOKUP_OF("csiphy_timer_src_clk", csi2phytimer_clk_src,
		"fda0b400.qcom,csiphy"),
	CLK_LOOKUP_OF("csiphy_timer_clk", camss_phy2_csi2phytimer_clk,
		"fda0b400.qcom,csiphy"),

	/* CSID clocks */
	CLK_LOOKUP_OF("ispif_ahb_clk", camss_ispif_ahb_clk,
					"fda08000.qcom,csid"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
					"fda08000.qcom,csid"),
	CLK_LOOKUP_OF("csi_ahb_clk", camss_csi0_ahb_clk,
					"fda08000.qcom,csid"),
	CLK_LOOKUP_OF("csi_src_clk", csi0_clk_src,
					"fda08000.qcom,csid"),
	CLK_LOOKUP_OF("csi_phy_clk", camss_csi0phy_clk,
					"fda08000.qcom,csid"),
	CLK_LOOKUP_OF("csi_clk", camss_csi0_clk,
					"fda08000.qcom,csid"),
	CLK_LOOKUP_OF("csi_pix_clk", camss_csi0pix_clk,
					"fda08000.qcom,csid"),
	CLK_LOOKUP_OF("csi_rdi_clk", camss_csi0rdi_clk,
					"fda08000.qcom,csid"),

	CLK_LOOKUP_OF("ispif_ahb_clk", camss_ispif_ahb_clk,
					"fda08400.qcom,csid"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
					"fda08400.qcom,csid"),
	CLK_LOOKUP_OF("csi_ahb_clk", camss_csi1_ahb_clk,
					"fda08400.qcom,csid"),
	CLK_LOOKUP_OF("csi_src_clk", csi1_clk_src,
					"fda08400.qcom,csid"),
	CLK_LOOKUP_OF("csi_phy_clk", camss_csi1phy_clk,
					"fda08400.qcom,csid"),
	CLK_LOOKUP_OF("csi_clk", camss_csi1_clk,
					"fda08400.qcom,csid"),
	CLK_LOOKUP_OF("csi_pix_clk", camss_csi1pix_clk,
					"fda08400.qcom,csid"),
	CLK_LOOKUP_OF("csi_rdi_clk", camss_csi1rdi_clk,
					"fda08400.qcom,csid"),

	CLK_LOOKUP_OF("ispif_ahb_clk", camss_ispif_ahb_clk,
					"fda08800.qcom,csid"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
					"fda08800.qcom,csid"),
	CLK_LOOKUP_OF("csi_ahb_clk", camss_csi2_ahb_clk,
					"fda08800.qcom,csid"),
	CLK_LOOKUP_OF("csi_src_clk", csi2_clk_src,
					"fda08800.qcom,csid"),
	CLK_LOOKUP_OF("csi_phy_clk", camss_csi2phy_clk,
					"fda08800.qcom,csid"),
	CLK_LOOKUP_OF("csi_clk", camss_csi2_clk,
					"fda08800.qcom,csid"),
	CLK_LOOKUP_OF("csi_pix_clk", camss_csi2pix_clk,
					"fda08800.qcom,csid"),
	CLK_LOOKUP_OF("csi_rdi_clk", camss_csi2rdi_clk,
					"fda08800.qcom,csid"),

	CLK_LOOKUP_OF("ispif_ahb_clk", camss_ispif_ahb_clk,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP_OF("csi_ahb_clk", camss_csi3_ahb_clk,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP_OF("csi_src_clk", csi3_clk_src,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP_OF("csi_phy_clk", camss_csi3phy_clk,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP_OF("csi_clk", camss_csi3_clk,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP_OF("csi_pix_clk", camss_csi3pix_clk,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP_OF("csi_rdi_clk", camss_csi3rdi_clk,
					"fda08c00.qcom,csid"),

	/* ISPIF clocks */
	CLK_LOOKUP_OF("ispif_ahb_clk", camss_ispif_ahb_clk,
		"fda0a000.qcom,ispif"),

	CLK_LOOKUP_OF("vfe0_clk_src", vfe0_clk_src, "fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("camss_vfe_vfe0_clk", camss_vfe_vfe0_clk,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("camss_csi_vfe0_clk", camss_csi_vfe0_clk,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("vfe1_clk_src", vfe1_clk_src, "fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("camss_vfe_vfe1_clk", camss_vfe_vfe1_clk,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("camss_csi_vfe1_clk", camss_csi_vfe1_clk,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi0_src_clk", csi0_clk_src,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi0_clk", camss_csi0_clk,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi0_pix_clk", camss_csi0pix_clk,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi0_rdi_clk", camss_csi0rdi_clk,
			   "fda0a000.qcom,ispif"),

	CLK_LOOKUP_OF("csi1_src_clk", csi1_clk_src,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi1_clk", camss_csi1_clk,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi1_pix_clk", camss_csi1pix_clk,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi1_rdi_clk", camss_csi1rdi_clk,
					"fda0a000.qcom,ispif"),

	CLK_LOOKUP_OF("csi2_src_clk", csi2_clk_src,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi2_clk", camss_csi2_clk,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi2_pix_clk", camss_csi2pix_clk,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi2_rdi_clk", camss_csi2rdi_clk,
					"fda0a000.qcom,ispif"),

	CLK_LOOKUP_OF("csi3_src_clk", csi3_clk_src,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi3_clk", camss_csi3_clk,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi3_pix_clk", camss_csi3pix_clk,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP_OF("csi3_rdi_clk", camss_csi3rdi_clk,
					"fda0a000.qcom,ispif"),

	/*VFE clocks*/
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
					"fda10000.qcom,vfe"),
	CLK_LOOKUP_OF("vfe_clk_src", vfe0_clk_src,	 "fda10000.qcom,vfe"),
	CLK_LOOKUP_OF("camss_vfe_vfe_clk", camss_vfe_vfe0_clk,
					"fda10000.qcom,vfe"),
	CLK_LOOKUP_OF("camss_csi_vfe_clk", camss_csi_vfe0_clk,
					"fda10000.qcom,vfe"),
	CLK_LOOKUP_OF("iface_clk", camss_vfe_vfe_ahb_clk, "fda10000.qcom,vfe"),
	CLK_LOOKUP_OF("bus_clk", camss_vfe_vfe_axi_clk,	 "fda10000.qcom,vfe"),
	CLK_LOOKUP_OF("alt_bus_clk", camss_vfe_vfe_ocmemnoc_clk,
					"fda10000.qcom,vfe"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
					"fda14000.qcom,vfe"),
	CLK_LOOKUP_OF("vfe_clk_src", vfe1_clk_src,	 "fda14000.qcom,vfe"),
	CLK_LOOKUP_OF("camss_vfe_vfe_clk", camss_vfe_vfe1_clk,
					"fda14000.qcom,vfe"),
	CLK_LOOKUP_OF("camss_csi_vfe_clk", camss_csi_vfe1_clk,
					"fda14000.qcom,vfe"),
	CLK_LOOKUP_OF("iface_clk", camss_vfe_vfe_ahb_clk, "fda14000.qcom,vfe"),
	CLK_LOOKUP_OF("bus_clk", camss_vfe_vfe_axi_clk,	 "fda14000.qcom,vfe"),
	CLK_LOOKUP_OF("alt_bus_clk", camss_vfe_vfe_ocmemnoc_clk,
					"fda14000.qcom,vfe"),
	/*Jpeg Clocks*/
	CLK_LOOKUP_OF("core_clk", camss_jpeg_jpeg0_clk, "fda1c000.qcom,jpeg"),
	CLK_LOOKUP_OF("core_clk", camss_jpeg_jpeg1_clk, "fda20000.qcom,jpeg"),
	CLK_LOOKUP_OF("core_clk", camss_jpeg_jpeg2_clk, "fda24000.qcom,jpeg"),
	CLK_LOOKUP_OF("iface_clk", camss_jpeg_jpeg_ahb_clk,
						"fda1c000.qcom,jpeg"),
	CLK_LOOKUP_OF("iface_clk", camss_jpeg_jpeg_ahb_clk,
						"fda20000.qcom,jpeg"),
	CLK_LOOKUP_OF("iface_clk", camss_jpeg_jpeg_ahb_clk,
						"fda24000.qcom,jpeg"),
	CLK_LOOKUP_OF("iface_clk", camss_jpeg_jpeg_ahb_clk,
						"fda64000.qcom,iommu"),
	CLK_LOOKUP_OF("core_clk", camss_jpeg_jpeg_axi_clk,
						"fda64000.qcom,iommu"),
	CLK_LOOKUP_OF("alt_core_clk", camss_top_ahb_clk, "fda64000.qcom,iommu"),
	CLK_LOOKUP_OF("bus_clk0", camss_jpeg_jpeg_axi_clk,
							"fda1c000.qcom,jpeg"),
	CLK_LOOKUP_OF("bus_clk0", camss_jpeg_jpeg_axi_clk,
							"fda20000.qcom,jpeg"),
	CLK_LOOKUP_OF("bus_clk0", camss_jpeg_jpeg_axi_clk,
							"fda24000.qcom,jpeg"),
	CLK_LOOKUP_OF("alt_bus_clk", camss_jpeg_jpeg_ocmemnoc_clk,
						"fda1c000.qcom,jpeg"),
	CLK_LOOKUP_OF("alt_bus_clk", camss_jpeg_jpeg_ocmemnoc_clk,
						"fda20000.qcom,jpeg"),
	CLK_LOOKUP_OF("alt_bus_clk", camss_jpeg_jpeg_ocmemnoc_clk,
						"fda24000.qcom,jpeg"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
						"fda1c000.qcom,jpeg"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
						"fda20000.qcom,jpeg"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
						"fda24000.qcom,jpeg"),
	CLK_LOOKUP_OF("micro_iface_clk", camss_micro_ahb_clk,
		"fda04000.qcom,cpp"),
	CLK_LOOKUP_OF("camss_top_ahb_clk", camss_top_ahb_clk,
		"fda04000.qcom,cpp"),
	CLK_LOOKUP_OF("cpp_iface_clk", camss_vfe_cpp_ahb_clk,
		"fda04000.qcom,cpp"),
	CLK_LOOKUP_OF("cpp_core_clk", camss_vfe_cpp_clk, "fda04000.qcom,cpp"),
	CLK_LOOKUP_OF("cpp_bus_clk", camss_vfe_vfe_axi_clk,
							"fda04000.qcom,cpp"),
	CLK_LOOKUP_OF("vfe_clk_src", vfe0_clk_src,	"fda04000.qcom,cpp"),
	CLK_LOOKUP_OF("camss_vfe_vfe_clk", camss_vfe_vfe0_clk,
					"fda04000.qcom,cpp"),
	CLK_LOOKUP_OF("iface_clk", camss_vfe_vfe_ahb_clk, "fda04000.qcom,cpp"),


	CLK_LOOKUP_OF("iface_clk", camss_micro_ahb_clk, ""),
	CLK_LOOKUP_OF("iface_clk", camss_vfe_vfe_ahb_clk,
							"fda44000.qcom,iommu"),
	CLK_LOOKUP_OF("core_clk", camss_vfe_vfe_axi_clk, "fda44000.qcom,iommu"),
	CLK_LOOKUP_OF("alt_core_clk", camss_top_ahb_clk, "fda44000.qcom,iommu"),
	CLK_LOOKUP_OF("iface_clk", mdss_ahb_clk, "mdp.0"),
	CLK_LOOKUP_OF("iface_clk", mdss_ahb_clk, "fd923400.qcom,mdss_edp"),
	CLK_LOOKUP_OF("iface_clk", mdss_ahb_clk, "fd928000.qcom,iommu"),
	CLK_LOOKUP_OF("core_clk", mdss_axi_clk, "fd928000.qcom,iommu"),
	CLK_LOOKUP_OF("bus_clk", mdss_axi_clk, "mdp.0"),
	CLK_LOOKUP_OF("core_clk", oxili_gfx3d_clk, "fdb00000.qcom,kgsl-3d0"),
	CLK_LOOKUP_OF("iface_clk", oxilicx_ahb_clk, "fdb00000.qcom,kgsl-3d0"),
	CLK_LOOKUP_OF("mem_iface_clk", ocmemcx_ocmemnoc_clk,
						"fdb00000.qcom,kgsl-3d0"),
	CLK_LOOKUP_OF("core_clk", oxilicx_axi_clk, "fdb10000.qcom,iommu"),
	CLK_LOOKUP_OF("iface_clk", oxilicx_ahb_clk, "fdb10000.qcom,iommu"),
	CLK_LOOKUP_OF("alt_core_clk", oxili_gfx3d_clk, "fdb10000.qcom,iommu"),
	CLK_LOOKUP_OF("iface_clk", ocmemcx_ocmemnoc_clk, "fdd00000.qcom,ocmem"),
	CLK_LOOKUP_OF("iface_clk", venus0_ahb_clk, "fdc84000.qcom,iommu"),
	CLK_LOOKUP_OF("alt_core_clk", venus0_vcodec0_clk,
						  "fdc84000.qcom,iommu"),
	CLK_LOOKUP_OF("core_clk", venus0_axi_clk, "fdc84000.qcom,iommu"),
	CLK_LOOKUP_OF("bus_clk", venus0_axi_clk, ""),
	CLK_LOOKUP_OF("src_clk",  vcodec0_clk_src, "fdce0000.qcom,venus"),
	CLK_LOOKUP_OF("core_clk", venus0_vcodec0_clk, "fdce0000.qcom,venus"),
	CLK_LOOKUP_OF("iface_clk",  venus0_ahb_clk, "fdce0000.qcom,venus"),
	CLK_LOOKUP_OF("bus_clk",  venus0_axi_clk, "fdce0000.qcom,venus"),
	CLK_LOOKUP_OF("mem_clk",  venus0_ocmemnoc_clk, "fdce0000.qcom,venus"),
	CLK_LOOKUP_OF("core_clk", venus0_vcodec0_clk, "fdc00000.qcom,vidc"),
	CLK_LOOKUP_OF("iface_clk",  venus0_ahb_clk, "fdc00000.qcom,vidc"),
	CLK_LOOKUP_OF("bus_clk",  venus0_axi_clk, "fdc00000.qcom,vidc"),
	CLK_LOOKUP_OF("mem_clk",  venus0_ocmemnoc_clk, "fdc00000.qcom,vidc"),

	CLK_LOOKUP_OF("core_mmss_clk", mmss_misc_ahb_clk, "fdf30018.hwevent"),

	CLK_LOOKUP_OF("bus_clk", ocmemnoc_clk, "msm_ocmem_noc"),
	CLK_LOOKUP_OF("bus_a_clk", ocmemnoc_clk, "msm_ocmem_noc"),
	CLK_LOOKUP_OF("bus_clk", mmss_s0_axi_clk, "msm_mmss_noc"),
	CLK_LOOKUP_OF("bus_a_clk", mmss_s0_axi_clk, "msm_mmss_noc"),

	CLK_LOOKUP_OF("", mmssnoc_ahb, ""),

	/* DSI PLL clocks */
	CLK_LOOKUP_OF("",		dsi_vco_clk_8974,                  ""),
	CLK_LOOKUP_OF("",		analog_postdiv_clk_8974,         ""),
	CLK_LOOKUP_OF("",		indirect_path_div2_clk_8974,     ""),
	CLK_LOOKUP_OF("",		pixel_clk_src_8974,              ""),
	CLK_LOOKUP_OF("",		byte_mux_8974,                   ""),
	CLK_LOOKUP_OF("",		byte_clk_src_8974,               ""),
};

/* v1 to v2 clock changes */
void msm8974_mmss_v2_clock_override(void)
{
	mmpll3_clk_src.c.rate =  930000000;
	mmpll1_clk_src.c.rate = 1167000000;
	mmpll1_clk_src.c.fmax[VDD_DIG_NOMINAL] = 1167000000;

	ocmemnoc_clk_src.freq_tbl = ftbl_ocmemnoc_v2_clk;
	ocmemnoc_clk_src.c.fmax[VDD_DIG_NOMINAL] = 291750000;

	axi_clk_src.freq_tbl = ftbl_mmss_axi_v2_clk;
	axi_clk_src.c.fmax[VDD_DIG_NOMINAL] = 291750000;
	axi_clk_src.c.fmax[VDD_DIG_HIGH] = 466800000;

	vcodec0_clk_src.freq_tbl = ftbl_venus0_vcodec0_v2_clk;
	vcodec0_clk_src.c.fmax[VDD_DIG_HIGH] = 465000000;

	mdp_clk_src.c.fmax[VDD_DIG_NOMINAL] = 240000000;
}

/* v2 to pro clock changes */
void msm8974_mmss_pro_clock_override(bool aa, bool ab, bool ac)
{
	vfe0_clk_src.c.fmax[VDD_DIG_LOW] = 150000000;
	vfe0_clk_src.c.fmax[VDD_DIG_NOMINAL] = 320000000;
	vfe1_clk_src.c.fmax[VDD_DIG_LOW] = 150000000;
	vfe1_clk_src.c.fmax[VDD_DIG_NOMINAL] = 320000000;
	cpp_clk_src.c.fmax[VDD_DIG_LOW] = 150000000;
	cpp_clk_src.c.fmax[VDD_DIG_NOMINAL] = 320000000;

	if (ab || ac) {
		vfe0_clk_src.c.fmax[VDD_DIG_HIGH] = 465000000;
		vfe1_clk_src.c.fmax[VDD_DIG_HIGH] = 465000000;
		cpp_clk_src.c.fmax[VDD_DIG_HIGH] = 465000000;
	} else if (aa) {
		vfe0_clk_src.c.fmax[VDD_DIG_HIGH] = 320000000;
		vfe1_clk_src.c.fmax[VDD_DIG_HIGH] = 320000000;
		cpp_clk_src.c.fmax[VDD_DIG_HIGH] = 320000000;
	}

	mdp_clk_src.c.fmax[VDD_DIG_NOMINAL] = 266670000;

	mclk0_clk_src.freq_tbl = ftbl_camss_mclk0_3_pro_clk;
	mclk1_clk_src.freq_tbl = ftbl_camss_mclk0_3_pro_clk;
	mclk2_clk_src.freq_tbl = ftbl_camss_mclk0_3_pro_clk;
	mclk3_clk_src.freq_tbl = ftbl_camss_mclk0_3_pro_clk;
	mclk0_clk_src.set_rate = set_rate_mnd;
	mclk1_clk_src.set_rate = set_rate_mnd;
	mclk2_clk_src.set_rate = set_rate_mnd;
	mclk3_clk_src.set_rate = set_rate_mnd;
	mclk0_clk_src.c.ops = &clk_ops_rcg_mnd;
	mclk1_clk_src.c.ops = &clk_ops_rcg_mnd;
	mclk2_clk_src.c.ops = &clk_ops_rcg_mnd;
	mclk3_clk_src.c.ops = &clk_ops_rcg_mnd;
}

static struct of_device_id msm_clock_mmsscc_match_table[] = {
	{ .compatible = "qcom,mmsscc-8974" },
	{ .compatible = "qcom,mmsscc-8974v2" },
	{ .compatible = "qcom,mmsscc-8974pro" },
	{ .compatible = "qcom,mmsscc-8974pro-ac" },
	{}
};

static int msm_mmsscc_8974_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *cxo_mmss, *gpll0_mmss;
	int ret;
	int compatlen;
	const char *compat = NULL;
	bool v2 = false, pro = false;
	bool pro_aa = false, pro_ab = false, pro_ac = false;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat)
		return -EINVAL;

	pro_ac = !strcmp(compat, "qcom,mmsscc-8974pro-ac");
	pro_ab = !strcmp(compat, "qcom,mmsscc-8974pro-ab");
	pro_aa = !strcmp(compat, "qcom,mmsscc-8974pro-aa");
	pro = pro_ac || pro_ab || pro_aa ||
		     !strcmp(compat, "qcom,mmsscc-8974pro");
	v2 = pro || !strcmp(compat, "qcom,mmsscc-8974v2");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Unable to retrieve register base.\n");
		return -ENOMEM;
	}
	virt_bases[MMSS_BASE] = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!virt_bases[MMSS_BASE]) {
		dev_err(&pdev->dev, "Failed to map in CC registers.\n");
		return -ENOMEM;
	}

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get the vdd_dig regulator!");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	cxo_mmss = cxo_clk_src.c.parent = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(cxo_mmss)) {
		if (!(PTR_ERR(cxo_mmss) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get the the cxo clock!");
		return PTR_ERR(cxo_mmss);
	}

	gpll0_mmss = gpll0_clk_src.c.parent = devm_clk_get(&pdev->dev, "gpll0");
	if (IS_ERR(gpll0_mmss)) {
		if (!(PTR_ERR(gpll0_mmss) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get the the gpll0 clock!");
		return PTR_ERR(gpll0_mmss);
	}

	oxili_gfx3d_clk.c.parent = devm_clk_get(&pdev->dev, "gfx3d_src_clk");
	if (IS_ERR(oxili_gfx3d_clk.c.parent)) {
		if (!(PTR_ERR(oxili_gfx3d_clk.c.parent) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get the the gfx3d source clock!");
		return PTR_ERR(oxili_gfx3d_clk.c.parent);
	}

	mmssnoc_ahb.c.parent = devm_clk_get(&pdev->dev, "mmssnoc_ahb");
	if (IS_ERR(mmssnoc_ahb.c.parent)) {
		if (!(PTR_ERR(mmssnoc_ahb.c.parent) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get the the MMSSNOC AHB clock!");
		return PTR_ERR(mmssnoc_ahb.c.parent);
	}

	configure_sr_hpm_lp_pll(&mmpll0_config, &mmpll0_regs, 1);

	if (v2) {
		configure_sr_hpm_lp_pll(&mmpll1_v2_config, &mmpll1_regs, 1);
		configure_sr_hpm_lp_pll(&mmpll3_v2_config, &mmpll3_regs, 0);
		msm8974_mmss_v2_clock_override();
		if (pro)
			msm8974_mmss_pro_clock_override(pro_aa, pro_ab, pro_ac);
	} else {
		configure_sr_hpm_lp_pll(&mmpll1_config, &mmpll1_regs, 1);
		configure_sr_hpm_lp_pll(&mmpll3_config, &mmpll3_regs, 0);
	}

	/*
	 * MDSS needs the ahb clock and needs to init before we register the
	 * lookup table.
	 */
	mdss_clk_ctrl_pre_init(&mdss_ahb_clk.c);

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_mmss_8974,
				 ARRAY_SIZE(msm_clocks_mmss_8974));
	if (ret)
		return ret;

	if (pro) {
		ret = of_msm_clock_register(pdev->dev.of_node,
				 msm_camera_clocks_8974pro_only,
				 ARRAY_SIZE(msm_camera_clocks_8974pro_only));
		if (ret)
			return ret;
	} else {
		ret = of_msm_clock_register(pdev->dev.of_node,
				 msm_camera_clocks_8974_only,
				 ARRAY_SIZE(msm_camera_clocks_8974_only));
		if (ret)
			return ret;
	}

	if (v2) {
		ret = clk_set_rate(&axi_clk_src.c, 291750000);
		ret = clk_set_rate(&ocmemnoc_clk_src.c, 291750000);
	} else {
		clk_set_rate(&axi_clk_src.c, 282000000);
		clk_set_rate(&ocmemnoc_clk_src.c, 282000000);
	}

	dev_info(&pdev->dev, "Registered MMSS clocks.\n");

	return 0;
}

static struct platform_driver msm_clock_mmsscc_driver = {
	.probe = msm_mmsscc_8974_probe,
	.driver = {
		.name = "qcom,mmsscc-8974",
		.of_match_table = msm_clock_mmsscc_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_mmsscc_8974_init(void)
{
	return platform_driver_register(&msm_clock_mmsscc_driver);
}
arch_initcall(msm_mmsscc_8974_init);


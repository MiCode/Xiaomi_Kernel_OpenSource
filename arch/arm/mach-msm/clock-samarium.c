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
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/clk/msm-clock-generic.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-rpm.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/clock-krait.h>

#include "clock.h"
#include "clock-mdss-8974.h"

enum {
	GCC_BASE,
	MMSS_BASE,
	LPASS_BASE,
	APCS_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))
#define MMSS_REG_BASE(x) (void __iomem *)(virt_bases[MMSS_BASE] + (x))
#define LPASS_REG_BASE(x) (void __iomem *)(virt_bases[LPASS_BASE] + (x))
#define APCS_REG_BASE(x) (void __iomem *)(virt_bases[APCS_BASE] + (x))

#define xo_source_val 0
#define gpll0_source_val 1
#define gpll4_source_val 5
#define xo_mm_source_val 0
#define mmpll0_mm_source_val 1
#define mmpll1_mm_source_val 2
#define mmpll3_mm_source_val 3
#define mmpll4_mm_source_val 3
#define gpll0_mm_source_val 5
#define dsipll0_pixel_mm_source_val 1
#define dsipll0_byte_mm_source_val 1

#define FIXDIV(div) (div ? (2 * (div) - 1) : (0))

#define F(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
	}

#define F_MM(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s.c, \
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

#define RPM_MISC_CLK_TYPE	0x306b6c63
#define RPM_BUS_CLK_TYPE	0x316b6c63
#define RPM_MEM_CLK_TYPE	0x326b6c63

#define CXO_ID		0x0
#define QDSS_ID		0x1
#define PNOC_ID		0x0
#define SNOC_ID		0x1
#define CNOC_ID		0x2
#define BIMC_ID		0x0
#define OCMEM_ID	0x2
#define OXILI_ID	0x1
#define MMSSNOC_AHB_ID  0x3

#define BB_CLK1_ID	 1
#define BB_CLK2_ID	 2
#define RF_CLK1_ID	 4
#define RF_CLK2_ID	 5
#define RF_CLK3_ID	 6
#define DIFF_CLK1_ID	 7
#define DIV_CLK1_ID	11
#define DIV_CLK2_ID	12
#define DIV_CLK3_ID	13

#define GPLL0_STATUS                                       (0x001C)
#define GPLL4_STATUS                                       (0x1DDC)
#define MSS_CFG_AHB_CBCR                                   (0x0280)
#define MSS_Q6_BIMC_AXI_CBCR                               (0x0284)
#define USB_HS_BCR                                         (0x0480)
#define USB_HS_SYSTEM_CBCR                                 (0x0484)
#define USB_HS_AHB_CBCR                                    (0x0488)
#define USB_HS_SYSTEM_CMD_RCGR                             (0x0490)
#define USB2A_PHY_SLEEP_CBCR                               (0x04AC)
#define SDCC1_APPS_CMD_RCGR                                (0x04D0)
#define SDCC1_APPS_CBCR                                    (0x04C4)
#define SDCC1_AHB_CBCR                                     (0x04C8)
#define SDCC1_CDCCAL_SLEEP_CBCR                            (0x04E4)
#define SDCC1_CDCCAL_FF_CBCR                               (0x04E8)
#define SDCC2_APPS_CMD_RCGR                                (0x0510)
#define SDCC2_APPS_CBCR                                    (0x0504)
#define SDCC2_AHB_CBCR                                     (0x0508)
#define SDCC3_APPS_CMD_RCGR                                (0x0550)
#define SDCC3_APPS_CBCR                                    (0x0544)
#define SDCC3_AHB_CBCR                                     (0x0548)
#define SDCC4_APPS_CMD_RCGR                                (0x0590)
#define SDCC4_APPS_CBCR                                    (0x0584)
#define SDCC4_AHB_CBCR                                     (0x0588)
#define BLSP1_AHB_CBCR                                     (0x05C4)
#define BLSP1_QUP1_SPI_APPS_CBCR                           (0x0644)
#define BLSP1_QUP1_I2C_APPS_CBCR                           (0x0648)
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR                       (0x0660)
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR                       (0x06E0)
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR                       (0x0760)
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR                       (0x07E0)
#define BLSP2_QUP1_I2C_APPS_CMD_RCGR                       (0x09A0)
#define BLSP2_QUP2_I2C_APPS_CMD_RCGR                       (0x0A20)
#define BLSP2_QUP3_I2C_APPS_CMD_RCGR                       (0x0AA0)
#define BLSP2_QUP4_I2C_APPS_CMD_RCGR                       (0x0B20)
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR                       (0x064C)
#define BLSP1_UART1_APPS_CBCR                              (0x0684)
#define BLSP1_UART1_APPS_CMD_RCGR                          (0x068C)
#define BLSP1_QUP2_SPI_APPS_CBCR                           (0x06C4)
#define BLSP1_QUP2_I2C_APPS_CBCR                           (0x06C8)
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR                       (0x06CC)
#define BLSP1_UART2_APPS_CBCR                              (0x0704)
#define BLSP1_UART2_APPS_CMD_RCGR                          (0x070C)
#define BLSP1_QUP3_SPI_APPS_CBCR                           (0x0744)
#define BLSP1_QUP3_I2C_APPS_CBCR                           (0x0748)
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR                       (0x074C)
#define BLSP1_UART3_APPS_CBCR                              (0x0784)
#define BLSP1_UART3_APPS_CMD_RCGR                          (0x078C)
#define BLSP1_QUP4_SPI_APPS_CBCR                           (0x07C4)
#define BLSP1_QUP4_I2C_APPS_CBCR                           (0x07C8)
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR                       (0x07CC)
#define BLSP1_UART4_APPS_CBCR                              (0x0804)
#define BLSP1_UART4_APPS_CMD_RCGR                          (0x080C)
#define BLSP2_AHB_CBCR                                     (0x0944)
#define BLSP2_QUP1_SPI_APPS_CBCR                           (0x0984)
#define BLSP2_QUP1_I2C_APPS_CBCR                           (0x0988)
#define BLSP2_QUP1_SPI_APPS_CMD_RCGR                       (0x098C)
#define BLSP2_UART1_APPS_CBCR                              (0x09C4)
#define BLSP2_UART1_APPS_CMD_RCGR                          (0x09CC)
#define BLSP2_QUP2_SPI_APPS_CBCR                           (0x0A04)
#define BLSP2_QUP2_I2C_APPS_CBCR                           (0x0A08)
#define BLSP2_QUP2_SPI_APPS_CMD_RCGR                       (0x0A0C)
#define BLSP2_UART2_APPS_CBCR                              (0x0A44)
#define BLSP2_UART2_APPS_CMD_RCGR                          (0x0A4C)
#define BLSP2_QUP3_SPI_APPS_CBCR                           (0x0A84)
#define BLSP2_QUP3_I2C_APPS_CBCR                           (0x0A88)
#define BLSP2_QUP3_SPI_APPS_CMD_RCGR                       (0x0A8C)
#define BLSP2_UART3_APPS_CBCR                              (0x0AC4)
#define BLSP2_UART3_APPS_CMD_RCGR                          (0x0ACC)
#define BLSP2_QUP4_SPI_APPS_CBCR                           (0x0B04)
#define BLSP2_QUP4_I2C_APPS_CBCR                           (0x0B08)
#define BLSP2_QUP4_SPI_APPS_CMD_RCGR                       (0x0B0C)
#define BLSP2_UART4_APPS_CBCR                              (0x0B44)
#define BLSP2_UART4_APPS_CMD_RCGR                          (0x0B4C)
#define PDM_AHB_CBCR                                       (0x0CC4)
#define PDM2_CBCR                                          (0x0CCC)
#define PDM2_CMD_RCGR                                      (0x0CD0)
#define PRNG_AHB_CBCR                                      (0x0D04)
#define BAM_DMA_AHB_CBCR                                   (0x0D44)
#define TSIF_AHB_CBCR                                      (0x0D84)
#define TSIF_REF_CBCR                                      (0x0D88)
#define TSIF_REF_CMD_RCGR                                  (0x0D90)
#define BOOT_ROM_AHB_CBCR                                  (0x0E04)
#define RPM_MISC                                           (0x0F24)
#define CE1_CMD_RCGR                                       (0x1050)
#define CE1_CBCR                                           (0x1044)
#define CE1_AXI_CBCR                                       (0x1048)
#define CE1_AHB_CBCR                                       (0x104C)
#define GCC_XO_DIV4_CBCR                                   (0x10C8)
#define LPASS_Q6_AXI_CBCR                                  (0x11C0)
#define LPASS_SYS_NOC_MPORT_CBCR                           (0x11C4)
#define LPASS_SYS_NOC_SWAY_CBCR                            (0x11C8)
#define APCS_GPLL_ENA_VOTE                                 (0x1480)
#define APCS_CLOCK_BRANCH_ENA_VOTE                         (0x1484)
#define GCC_DEBUG_CLK_CTL                                  (0x1880)
#define CLOCK_FRQ_MEASURE_CTL                              (0x1884)
#define CLOCK_FRQ_MEASURE_STATUS                           (0x1888)
#define PLLTEST_PAD_CFG                                    (0x188C)
#define GP1_CBCR                                           (0x1900)
#define GP1_CMD_RCGR                                       (0x1904)
#define GLB_CLK_DIAG                                       (0x001C)
#define SLEEP_CBCR                                         (0x0038)
#define L2_CBCR                                            (0x004C)
#define MMPLL0_MODE                                        (0x0000)
#define MMPLL0_L_VAL                                       (0x0004)
#define MMPLL0_M_VAL                                       (0x0008)
#define MMPLL0_N_VAL                                       (0x000C)
#define MMPLL0_USER_CTL                                    (0x0010)
#define MMPLL0_STATUS                                      (0x001C)
#define MMPLL1_MODE                                        (0x0040)
#define MMPLL1_L_VAL                                       (0x0044)
#define MMPLL1_M_VAL                                       (0x0048)
#define MMPLL1_N_VAL                                       (0x004C)
#define MMPLL1_USER_CTL                                    (0x0050)
#define MMPLL1_STATUS                                      (0x005C)
#define MMPLL3_MODE                                        (0x0080)
#define MMPLL3_L_VAL                                       (0x0084)
#define MMPLL3_M_VAL                                       (0x0088)
#define MMPLL3_N_VAL                                       (0x008C)
#define MMPLL3_USER_CTL                                    (0x0090)
#define MMPLL3_STATUS                                      (0x009C)
#define MMPLL4_MODE                                        (0x00A0)
#define MMPLL4_L_VAL                                       (0x00A4)
#define MMPLL4_M_VAL                                       (0x00A8)
#define MMPLL4_N_VAL                                       (0x00AC)
#define MMPLL4_USER_CTL                                    (0x00B0)
#define MMPLL4_STATUS                                      (0x00BC)
#define MMSS_PLL_VOTE_APCS                                 (0x0100)
#define VCODEC0_CMD_RCGR                                   (0x1000)
#define VENUS0_VCODEC0_CBCR                                (0x1028)
#define VENUS0_AHB_CBCR                                    (0x1030)
#define VENUS0_AXI_CBCR                                    (0x1034)
#define VENUS0_OCMEMNOC_CBCR                               (0x1038)
#define PCLK0_CMD_RCGR                                     (0x2000)
#define MDP_CMD_RCGR                                       (0x2040)
#define VSYNC_CMD_RCGR                                     (0x2080)
#define BYTE0_CMD_RCGR                                     (0x2120)
#define ESC0_CMD_RCGR                                      (0x2160)
#define MDSS_AHB_CBCR                                      (0x2308)
#define MDSS_AXI_CBCR                                      (0x2310)
#define MDSS_PCLK0_CBCR                                    (0x2314)
#define MDSS_MDP_CBCR                                      (0x231C)
#define MDSS_MDP_LUT_CBCR                                  (0x2320)
#define MDSS_VSYNC_CBCR                                    (0x2328)
#define MDSS_BYTE0_CBCR                                    (0x233C)
#define MDSS_ESC0_CBCR                                     (0x2344)
#define CSI0PHYTIMER_CMD_RCGR                              (0x3000)
#define CAMSS_PHY0_CSI0PHYTIMER_CBCR                       (0x3024)
#define CSI1PHYTIMER_CMD_RCGR                              (0x3030)
#define CAMSS_PHY1_CSI1PHYTIMER_CBCR                       (0x3054)
#define CSI0_CMD_RCGR                                      (0x3090)
#define CAMSS_CSI0_CBCR                                    (0x30B4)
#define CAMSS_CSI0_AHB_CBCR                                (0x30BC)
#define CAMSS_CSI0PHY_CBCR                                 (0x30C4)
#define CAMSS_CSI0RDI_CBCR                                 (0x30D4)
#define CAMSS_CSI0PIX_CBCR                                 (0x30E4)
#define CSI1_CMD_RCGR                                      (0x3100)
#define CAMSS_CSI1_CBCR                                    (0x3124)
#define CAMSS_CSI1_AHB_CBCR                                (0x3128)
#define CAMSS_CSI1PHY_CBCR                                 (0x3134)
#define CAMSS_CSI1RDI_CBCR                                 (0x3144)
#define CAMSS_CSI1PIX_CBCR                                 (0x3154)
#define CSI2_CMD_RCGR                                      (0x3160)
#define CAMSS_CSI2_CBCR                                    (0x3184)
#define CAMSS_CSI2_AHB_CBCR                                (0x3188)
#define CAMSS_CSI2PHY_CBCR                                 (0x3194)
#define CAMSS_CSI2RDI_CBCR                                 (0x31A4)
#define CAMSS_CSI2PIX_CBCR                                 (0x31B4)
#define CAMSS_ISPIF_AHB_CBCR                               (0x3224)
#define CCI_CMD_RCGR                                       (0x3300)
#define CAMSS_CCI_CCI_CBCR                                 (0x3344)
#define CAMSS_CCI_CCI_AHB_CBCR                             (0x3348)
#define MCLK0_CMD_RCGR                                     (0x3360)
#define CAMSS_MCLK0_CBCR                                   (0x3384)
#define MCLK1_CMD_RCGR                                     (0x3390)
#define CAMSS_MCLK1_CBCR                                   (0x33B4)
#define MCLK2_CMD_RCGR                                     (0x33C0)
#define CAMSS_MCLK2_CBCR                                   (0x33E4)
#define MMSS_GP0_CMD_RCGR                                  (0x3420)
#define CAMSS_GP0_CBCR                                     (0x3444)
#define MMSS_GP1_CMD_RCGR                                  (0x3450)
#define CAMSS_GP1_CBCR                                     (0x3474)
#define CAMSS_TOP_AHB_CBCR                                 (0x3484)
#define CAMSS_AHB_CBCR                                     (0x348C)
#define CAMSS_MICRO_BCR                                    (0x3490)
#define CAMSS_MICRO_AHB_CBCR                               (0x3494)
#define JPEG0_CMD_RCGR                                     (0x3500)
#define CAMSS_JPEG_JPEG0_CBCR                              (0x35A8)
#define CAMSS_JPEG_JPEG_AHB_CBCR                           (0x35B4)
#define CAMSS_JPEG_JPEG_AXI_CBCR                           (0x35B8)
#define VFE0_CMD_RCGR                                      (0x3600)
#define VFE1_CMD_RCGR                                      (0x3620)
#define CPP_CMD_RCGR                                       (0x3640)
#define CAMSS_VFE_VFE0_CBCR                                (0x36A8)
#define CAMSS_VFE_VFE1_CBCR                                (0x36AC)
#define CAMSS_VFE_CPP_CBCR                                 (0x36B0)
#define CAMSS_VFE_CPP_AHB_CBCR                             (0x36B4)
#define CAMSS_VFE_VFE_AHB_CBCR                             (0x36B8)
#define CAMSS_VFE_VFE_AXI_CBCR                             (0x36BC)
#define CAMSS_CSI_VFE0_CBCR                                (0x3704)
#define CAMSS_CSI_VFE1_CBCR                                (0x3714)
#define OXILI_GFX3D_CBCR                                   (0x4028)
#define OXILICX_AHB_CBCR                                   (0x403C)
#define OCMEMCX_OCMEMNOC_CBCR                              (0x4058)
#define MMSS_MISC_AHB_CBCR                                 (0x502C)
#define AXI_CMD_RCGR                                       (0x5040)
#define MMSS_S0_AXI_CBCR                                   (0x5064)
#define MMSS_MMSSNOC_AXI_CBCR                              (0x506C)
#define OCMEMNOC_CMD_RCGR                                  (0x5090)
#define MMSS_DEBUG_CLK_CTL                                 (0x0900)
#define LPASS_DBG_CLK                                      (0x32000)

DEFINE_CLK_RPM_SMD_BRANCH(xo, xo_a_clk, RPM_MISC_CLK_TYPE, CXO_ID, 19200000);
DEFINE_CLK_RPM_SMD(cnoc, cnoc_a_clk, RPM_BUS_CLK_TYPE, CNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(pnoc, pnoc_a_clk, RPM_BUS_CLK_TYPE, PNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(bimc, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_ID, NULL);
DEFINE_CLK_RPM_SMD_QDSS(qdss, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_ID);
DEFINE_CLK_RPM_SMD(gfx3d, gfx3d_a_clk, RPM_MEM_CLK_TYPE, OXILI_ID, NULL);
DEFINE_CLK_RPM_SMD(mmssnoc_ahb, mmssnoc_ahb_a_clk, RPM_BUS_CLK_TYPE,
	MMSSNOC_AHB_ID, NULL);
DEFINE_CLK_RPM_SMD(ocmemgx, ocmemgx_a_clk, RPM_MEM_CLK_TYPE, OCMEM_ID, NULL);

DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk1, bb_clk1_a, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk2, bb_clk2_a, BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk1, rf_clk1_a, RF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk2, rf_clk2_a, RF_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk3, rf_clk3_a, RF_CLK3_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(diff_clk1, diff_clk1_a, DIFF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk1, div_clk1_a, DIV_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk2, div_clk2_a, DIV_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk3, div_clk3_a, DIV_CLK3_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk1_pin, bb_clk1_a_pin, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk2_pin, bb_clk2_a_pin, BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk1_pin, rf_clk1_a_pin, RF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk2_pin, rf_clk2_a_pin, RF_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk3_pin, rf_clk3_a_pin, RF_CLK3_ID);

static DEFINE_CLK_VOTER(pnoc_msmbus_clk, &pnoc.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_clk, &cnoc.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_msmbus_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_a_clk, &cnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(oxili_gfx3d_clk_src, &gfx3d.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_msmbus_clk, &ocmemgx.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_msmbus_a_clk, &ocmemgx_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_core_clk, &ocmemgx.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_keepalive_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_sps_clk, &pnoc.c, 0);

static DEFINE_CLK_BRANCH_VOTER(xo_otg_clk, &xo.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_lpass_clk, &xo.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_mss_clk, &xo.c);
static DEFINE_CLK_BRANCH_VOTER(xo_wlan_clk, &xo.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_pronto_clk, &xo.c);
static DEFINE_CLK_BRANCH_VOTER(xo_ehci_host_clk, &xo.c);
static DEFINE_CLK_BRANCH_VOTER(xo_lpm_clk, &xo.c);

/*
 * RPM manages gcc_bimc_gpu_clk automatically. This clock is created
 * for measurement only.
 */
DEFINE_CLK_DUMMY(bimc_gpu, 0);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.rate = 600000000,
		.parent = &xo.c,
		.dbg_name = "gpll0",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0.c),
	},
};

static struct pll_vote_clk gpll0_ao = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.rate = 600000000,
		.parent = &xo_a_clk.c,
		.dbg_name = "gpll0_ao",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao.c),
	},
};

static struct pll_vote_clk gpll4 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(4),
	.status_reg = (void __iomem *)GPLL4_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.rate = 768000000,
		.parent = &xo.c,
		.dbg_name = "gpll4",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll4.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk[] = {
	F(  19200000,         xo,    1,    0,     0),
	F(  37500000,      gpll0,   16,    0,     0),
	F(  50000000,      gpll0,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk[] = {
	F(    960000,         xo,   10,    1,     2),
	F(   4800000,         xo,    4,    0,     0),
	F(   9600000,         xo,    2,    0,     0),
	F(  15000000,      gpll0,   10,    1,     4),
	F(  19200000,         xo,    1,    0,     0),
	F(  25000000,      gpll0,   12,    1,     2),
	F(  50000000,      gpll0,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_uart1_4_apps_clk[] = {
	F(   3686400,      gpll0,    1,   96, 15625),
	F(   7372800,      gpll0,    1,  192, 15625),
	F(  14745600,      gpll0,    1,  384, 15625),
	F(  16000000,      gpll0,    5,    2,    15),
	F(  19200000,         xo,    1,    0,     0),
	F(  24000000,      gpll0,    5,    1,     5),
	F(  32000000,      gpll0,    1,    4,    75),
	F(  40000000,      gpll0,   15,    0,     0),
	F(  46400000,      gpll0,    1,   29,   375),
	F(  48000000,      gpll0, 12.5,    0,     0),
	F(  51200000,      gpll0,    1,   32,   375),
	F(  56000000,      gpll0,    1,    7,    75),
	F(  58982400,      gpll0,    1, 1536, 15625),
	F(  60000000,      gpll0,   10,    0,     0),
	F(  63160000,      gpll0,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart4_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup1_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce1_clk[] = {
	F(  75000000,      gpll0,    8,    0,     0),
	F( 171430000,      gpll0,  3.5,    0,     0),
	F_END
};

static struct rcg_clk ce1_clk_src = {
	.cmd_rcgr_reg = CE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_ce1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "ce1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 75000000, NOMINAL, 171430000),
		CLK_INIT(ce1_clk_src.c),
	},
};

static DEFINE_CLK_VOTER(scm_ce1_clk_src, &ce1_clk_src.c, 171430000);

static struct clk_freq_tbl ftbl_gcc_pdm2_clk[] = {
	F(  60000000,      gpll0,   10,    0,     0),
	F_END
};

static struct rcg_clk pdm2_clk_src = {
	.cmd_rcgr_reg = PDM2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_pdm2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pdm2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc1_apps_clk[] = {
	F(    144000,         xo,   16,    3,    25),
	F(    400000,         xo,   12,    1,     4),
	F(  20000000,      gpll0,   15,    1,     2),
	F(  25000000,      gpll0,   12,    1,     2),
	F(  50000000,      gpll0,   12,    0,     0),
	F( 100000000,      gpll0,    6,    0,     0),
	F( 192000000,      gpll4,    4,    0,     0),
	F( 384000000,      gpll4,    2,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_sdcc2_4_apps_clk[] = {
	F(    144000,         xo,   16,    3,    25),
	F(    400000,         xo,   12,    1,     4),
	F(  20000000,      gpll0,   15,    1,     2),
	F(  25000000,      gpll0,   12,    1,     2),
	F(  50000000,      gpll0,   12,    0,     0),
	F( 100000000,      gpll0,    6,    0,     0),
	F( 200000000,      gpll0,    3,    0,     0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg = SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 400000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg = SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc2_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc3_apps_clk_src = {
	.cmd_rcgr_reg = SDCC3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc2_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sdcc3_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc4_apps_clk_src = {
	.cmd_rcgr_reg = SDCC4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc2_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sdcc4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_tsif_ref_clk[] = {
	F(    105000,         xo,    2,    1,    91),
	F_END
};

static struct rcg_clk tsif_ref_clk_src = {
	.cmd_rcgr_reg = TSIF_REF_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_tsif_ref_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "tsif_ref_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 105500),
		CLK_INIT(tsif_ref_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F(  75000000,      gpll0,    8,    0,     0),
	F_END
};

static struct rcg_clk usb_hs_system_clk_src = {
	.cmd_rcgr_reg = USB_HS_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hs_system_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hs_system_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 37500000, NOMINAL, 75000000),
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct local_vote_clk gcc_bam_dma_ahb_clk = {
	.cbcr_reg = BAM_DMA_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(12),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_bam_dma_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_bam_dma_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg = BLSP1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp1_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.parent = &blsp1_qup1_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup1_spi_apps_clk",
		.parent = &blsp1_qup1_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.parent = &blsp1_qup2_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup2_spi_apps_clk",
		.parent = &blsp1_qup2_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.parent = &blsp1_qup3_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup3_spi_apps_clk",
		.parent = &blsp1_qup3_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.parent = &blsp1_qup4_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup4_spi_apps_clk",
		.parent = &blsp1_qup4_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart1_apps_clk = {
	.cbcr_reg = BLSP1_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart1_apps_clk",
		.parent = &blsp1_uart1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart2_apps_clk = {
	.cbcr_reg = BLSP1_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart2_apps_clk",
		.parent = &blsp1_uart2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart3_apps_clk = {
	.cbcr_reg = BLSP1_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart3_apps_clk",
		.parent = &blsp1_uart3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart4_apps_clk = {
	.cbcr_reg = BLSP1_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.parent = &blsp1_uart4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct local_vote_clk gcc_blsp2_ahb_clk = {
	.cbcr_reg = BLSP2_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(15),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp2_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup1_i2c_apps_clk",
		.parent = &blsp2_qup1_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup1_spi_apps_clk",
		.parent = &blsp2_qup1_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup2_i2c_apps_clk",
		.parent = &blsp2_qup2_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup2_spi_apps_clk",
		.parent = &blsp2_qup2_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup3_i2c_apps_clk",
		.parent = &blsp2_qup3_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup3_spi_apps_clk",
		.parent = &blsp2_qup3_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup4_i2c_apps_clk",
		.parent = &blsp2_qup4_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup4_spi_apps_clk",
		.parent = &blsp2_qup4_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart1_apps_clk = {
	.cbcr_reg = BLSP2_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart1_apps_clk",
		.parent = &blsp2_uart1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart2_apps_clk = {
	.cbcr_reg = BLSP2_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart2_apps_clk",
		.parent = &blsp2_uart2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart3_apps_clk = {
	.cbcr_reg = BLSP2_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart3_apps_clk",
		.parent = &blsp2_uart3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart4_apps_clk = {
	.cbcr_reg = BLSP2_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart4_apps_clk",
		.parent = &blsp2_uart4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart4_apps_clk.c),
	},
};

static struct local_vote_clk gcc_boot_rom_ahb_clk = {
	.cbcr_reg = BOOT_ROM_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(10),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_boot_rom_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_boot_rom_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_ahb_clk = {
	.cbcr_reg = CE1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(3),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_axi_clk = {
	.cbcr_reg = CE1_AXI_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(4),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce1_axi_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_axi_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_clk = {
	.cbcr_reg = CE1_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(5),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce1_clk",
		.parent = &ce1_clk_src.c,
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_clk.c),
	},
};

static struct branch_clk gcc_lpass_q6_axi_clk = {
	.cbcr_reg = LPASS_Q6_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_lpass_q6_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_lpass_q6_axi_clk.c),
	},
};

static struct branch_clk gcc_lpass_sys_noc_mport_clk = {
	.cbcr_reg = LPASS_SYS_NOC_MPORT_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_lpass_sys_noc_mport_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_lpass_sys_noc_mport_clk.c),
	},
};

static struct branch_clk gcc_lpass_sys_noc_sway_clk = {
	.cbcr_reg = LPASS_SYS_NOC_SWAY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_lpass_sys_noc_sway_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_lpass_sys_noc_sway_clk.c),
	},
};

static struct branch_clk gcc_mss_cfg_ahb_clk = {
	.cbcr_reg = MSS_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mss_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_mss_q6_bimc_axi_clk = {
	.cbcr_reg = MSS_Q6_BIMC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mss_q6_bimc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_q6_bimc_axi_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = PDM2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pdm2_clk",
		.parent = &pdm2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm2_clk.c),
	},
};

static struct branch_clk gcc_pdm_ahb_clk = {
	.cbcr_reg = PDM_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pdm_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_prng_ahb_clk = {
	.cbcr_reg = PRNG_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(13),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_prng_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_prng_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_ahb_clk = {
	.cbcr_reg = SDCC1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_apps_clk = {
	.cbcr_reg = SDCC1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_apps_clk",
		.parent = &sdcc1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_cdccal_ff_clk = {
	.cbcr_reg = SDCC1_CDCCAL_FF_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_cdccal_ff_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_cdccal_ff_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_cdccal_sleep_clk = {
	.cbcr_reg = SDCC1_CDCCAL_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_cdccal_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_cdccal_sleep_clk.c),
	},
};

static struct branch_clk gcc_sdcc2_ahb_clk = {
	.cbcr_reg = SDCC2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc2_apps_clk = {
	.cbcr_reg = SDCC2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc2_apps_clk",
		.parent = &sdcc2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc3_ahb_clk = {
	.cbcr_reg = SDCC3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc3_apps_clk = {
	.cbcr_reg = SDCC3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc3_apps_clk",
		.parent = &sdcc3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc4_ahb_clk = {
	.cbcr_reg = SDCC4_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc4_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc4_apps_clk = {
	.cbcr_reg = SDCC4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc4_apps_clk",
		.parent = &sdcc4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_apps_clk.c),
	},
};

static struct branch_clk gcc_tsif_ahb_clk = {
	.cbcr_reg = TSIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_tsif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ahb_clk.c),
	},
};

static struct branch_clk gcc_tsif_ref_clk = {
	.cbcr_reg = TSIF_REF_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_tsif_ref_clk",
		.parent = &tsif_ref_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ref_clk.c),
	},
};

static struct branch_clk gcc_usb2a_phy_sleep_clk = {
	.cbcr_reg = USB2A_PHY_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2a_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2a_phy_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb_hs_ahb_clk = {
	.cbcr_reg = USB_HS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_hs_system_clk = {
	.cbcr_reg = USB_HS_SYSTEM_CBCR,
	.bcr_reg = USB_HS_BCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs_system_clk",
		.parent = &usb_hs_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_system_clk.c),
	},
};

static struct pll_vote_clk mmpll0 = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)MMPLL0_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.rate = 800000000,
		.parent = &xo.c,
		.dbg_name = "mmpll0",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(mmpll0.c),
	},
};

static struct pll_vote_clk mmpll1 = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)MMPLL1_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.rate = 900000000,
		.parent = &xo.c,
		.dbg_name = "mmpll1",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(mmpll1.c),
	},
};

static struct pll_clk mmpll4 = {
	.mode_reg = (void __iomem *)MMPLL4_MODE,
	.status_reg = (void __iomem *)MMPLL4_STATUS,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmpll4",
		.parent = &xo.c,
		.rate = 930000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(mmpll4.c),
	},
};

static struct pll_clk mmpll3 = {
	.mode_reg = (void __iomem *)MMPLL3_MODE,
	.status_reg = (void __iomem *)MMPLL3_STATUS,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmpll3",
		.parent = &xo.c,
		.rate = 930000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(mmpll3.c),
	},
};

static struct clk_freq_tbl ftbl_mmss_mmssnoc_axi_clk[] = {
	F_MM(  19200000,         xo,    1,    0,     0),
	F_MM(  37500000,      gpll0,   16,    0,     0),
	F_MM(  50000000,      gpll0,   12,    0,     0),
	F_MM(  75000000,      gpll0,    8,    0,     0),
	F_MM( 100000000,      gpll0,    6,    0,     0),
	F_MM( 133330000,      gpll0,  4.5,    0,     0),
	F_MM( 266670000,     mmpll0,    3,    0,     0),
	F_MM( 300000000,     mmpll1,    3,    0,     0),
	F_END
};

static struct rcg_clk axi_clk_src = {
	.cmd_rcgr_reg = AXI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mmss_mmssnoc_axi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "axi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000, HIGH,
			300000000),
		CLK_INIT(axi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_csi0_2_clk[] = {
	F_MM( 100000000,      gpll0,    6,    0,     0),
	F_MM( 200000000,     mmpll0,    4,    0,     0),
	F_END
};

static struct rcg_clk csi0_clk_src = {
	.cmd_rcgr_reg = CSI0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_csi0_2_clk,
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
	.freq_tbl = ftbl_camss_csi0_2_clk,
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
	.freq_tbl = ftbl_camss_csi0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_vfe_vfe0_1_clk[] = {
	F_MM(  37500000,      gpll0,   16,    0,     0),
	F_MM(  50000000,      gpll0,   12,    0,     0),
	F_MM(  60000000,      gpll0,   10,    0,     0),
	F_MM(  80000000,      gpll0,  7.5,    0,     0),
	F_MM( 100000000,      gpll0,    6,    0,     0),
	F_MM( 109090000,      gpll0,  5.5,    0,     0),
	F_MM( 133330000,      gpll0,  4.5,    0,     0),
	F_MM( 150000000,      gpll0,    4,    0,     0),
	F_MM( 200000000,      gpll0,    3,    0,     0),
	F_MM( 228570000,     mmpll0,  3.5,    0,     0),
	F_MM( 266670000,     mmpll0,    3,    0,     0),
	F_MM( 320000000,     mmpll0,  2.5,    0,     0),
	F_MM( 400000000,     mmpll0,    2,    0,     0),
	F_MM( 465000000,     mmpll4,    2,    0,     0),
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
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 320000000, HIGH,
			465000000),
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
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000, HIGH,
			320000000),
		CLK_INIT(vfe1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_mdp_clk[] = {
	F_MM(  37500000,      gpll0,   16,    0,     0),
	F_MM(  60000000,      gpll0,   10,    0,     0),
	F_MM(  75000000,      gpll0,    8,    0,     0),
	F_MM(  85710000,      gpll0,    7,    0,     0),
	F_MM( 100000000,      gpll0,    6,    0,     0),
	F_MM( 150000000,      gpll0,    4,    0,     0),
	F_MM( 160000000,     mmpll0,    5,    0,     0),
	F_MM( 200000000,     mmpll0,    4,    0,     0),
	F_MM( 228570000,     mmpll0,  3.5,    0,     0),
	F_MM( 266670000,     mmpll0,    3,    0,     0),
	F_MM( 320000000,     mmpll0,  2.5,    0,     0),
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
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 266670000, HIGH,
			320000000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_jpeg_jpeg0_clk[] = {
	F_MM(  75000000,      gpll0,    8,    0,     0),
	F_MM( 133330000,      gpll0,  4.5,    0,     0),
	F_MM( 200000000,      gpll0,    3,    0,     0),
	F_MM( 228570000,     mmpll0,  3.5,    0,     0),
	F_MM( 266670000,     mmpll0,    3,    0,     0),
	F_MM( 320000000,     mmpll0,  2.5,    0,     0),
	F_END
};

static struct rcg_clk jpeg0_clk_src = {
	.cmd_rcgr_reg = JPEG0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_jpeg_jpeg0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "jpeg0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000, HIGH,
			320000000),
		CLK_INIT(jpeg0_clk_src.c),
	},
};

static struct clk_freq_tbl pixel_freq_tbl[] = {
	{
		.src_clk = &pixel_clk_src_samarium.c,
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
		.parent = &pixel_clk_src_samarium.c,
		.dbg_name = "pclk0_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 148500000, NOMINAL, 250000000),
		CLK_INIT(pclk0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ocmemcx_ocmemnoc_clk[] = {
	F_MM(  19200000,         xo,    1,    0,     0),
	F_MM(  37500000,      gpll0,   16,    0,     0),
	F_MM(  50000000,      gpll0,   12,    0,     0),
	F_MM(  75000000,      gpll0,    8,    0,     0),
	F_MM( 109090000,      gpll0,  5.5,    0,     0),
	F_MM( 150000000,      gpll0,    4,    0,     0),
	F_MM( 266670000,     mmpll0,    3,    0,     0),
	F_MM( 300000000,     mmpll1,    3,    0,     0),
	F_END
};

static struct rcg_clk ocmemnoc_clk_src = {
	.cmd_rcgr_reg = OCMEMNOC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ocmemcx_ocmemnoc_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "ocmemnoc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000, HIGH,
			300000000),
		CLK_INIT(ocmemnoc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_cci_cci_clk[] = {
	F_MM(  19200000,         xo,    1,    0,     0),
	F_END
};

static struct rcg_clk cci_clk_src = {
	.cmd_rcgr_reg = CCI_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_cci_cci_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "cci_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(cci_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_gp0_1_clk[] = {
	F_MM(     10000,         xo,   16,    1,   120),
	F_MM(     24000,         xo,   16,    1,    50),
	F_MM(   6000000,      gpll0,   10,    1,    10),
	F_MM(  12000000,      gpll0,   10,    1,     5),
	F_MM(  13000000,      gpll0,    4,   13,   150),
	F_MM(  24000000,      gpll0,    5,    1,     5),
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

static struct clk_freq_tbl ftbl_camss_mclk0_2_clk[] = {
	F_MM(   4800000,         xo,    4,    0,     0),
	F_MM(   6000000,      gpll0,   10,    1,    10),
	F_MM(   8000000,      gpll0,   15,    1,     5),
	F_MM(   9600000,         xo,    2,    0,     0),
	F_MM(  16000000,      gpll0, 12.5,    1,     3),
	F_MM(  19200000,         xo,    1,    0,     0),
	F_MM(  24000000,      gpll0,    5,    1,     5),
	F_MM(  32000000,     mmpll0,    5,    1,     5),
	F_MM(  48000000,      gpll0, 12.5,    0,     0),
	F_MM(  64000000,     mmpll0, 12.5,    0,     0),
	F_MM(  66670000,      gpll0,    9,    0,     0),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg = MCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_mclk0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk0_clk_src.c),
	},
};

static struct rcg_clk mclk1_clk_src = {
	.cmd_rcgr_reg = MCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_mclk0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk1_clk_src.c),
	},
};

static struct rcg_clk mclk2_clk_src = {
	.cmd_rcgr_reg = MCLK2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_mclk0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_phy0_1_csi0_1phytimer_clk[] = {
	F_MM( 100000000,      gpll0,    6,    0,     0),
	F_MM( 200000000,     mmpll0,    4,    0,     0),
	F_END
};

static struct rcg_clk csi0phytimer_clk_src = {
	.cmd_rcgr_reg = CSI0PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_phy0_1_csi0_1phytimer_clk,
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
	.freq_tbl = ftbl_camss_phy0_1_csi0_1phytimer_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi1phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi1phytimer_clk_src.c),
	},
};

static struct branch_clk camss_vfe_vfe0_clk = {
	.cbcr_reg = CAMSS_VFE_VFE0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_vfe_vfe1_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe1_clk.c),
	},
};

static struct clk_freq_tbl ftbl_camss_vfe_cpp_clk[] = {
	F_MM( 150000000,      gpll0,    4,    0,     0),
	F_MM( 266670000,     mmpll0,    3,    0,     0),
	F_MM( 320000000,     mmpll0,  2.5,    0,     0),
	F_MM( 400000000,     mmpll0,    2,    0,     0),
	F_MM( 465000000,     mmpll4,    2,    0,     0),
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
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 320000000, HIGH,
			465000000),
		CLK_INIT(cpp_clk_src.c),
	},
};

static struct clk_freq_tbl byte_freq_tbl[] = {
	{
		.src_clk = &byte_clk_src_samarium.c,
		.div_src_val = BVAL(10, 8, dsipll0_byte_mm_source_val),
	},
	F_END
};

static struct rcg_clk byte0_clk_src = {
	.cmd_rcgr_reg = BYTE0_CMD_RCGR,
	.current_freq = byte_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte_clk_src_samarium.c,
		.dbg_name = "byte0_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP2(LOW, 111370000, NOMINAL, 187500000),
		CLK_INIT(byte0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_esc0_clk[] = {
	F_MM(  19200000,         xo,    1,    0,     0),
	F_END
};

static struct rcg_clk esc0_clk_src = {
	.cmd_rcgr_reg = ESC0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_esc0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "esc0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(esc0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_vsync_clk[] = {
	F_MM(  19200000,         xo,    1,    0,     0),
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
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(vsync_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_venus0_vcodec0_clk[] = {
	F_MM(  50000000,      gpll0,   12,    0,     0),
	F_MM( 100000000,      gpll0,    6,    0,     0),
	F_MM( 133330000,     mmpll0,    6,    0,     0),
	F_MM( 200000000,     mmpll0,    4,    0,     0),
	F_MM( 266670000,     mmpll0,    3,    0,     0),
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
		VDD_DIG_FMAX_MAP2(LOW, 133330000, NOMINAL, 266670000),
		CLK_INIT(vcodec0_clk_src.c),
	},
};

static struct branch_clk camss_ahb_clk = {
	.cbcr_reg = CAMSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_ahb_clk.c),
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
		.dbg_name = "camss_cci_cci_clk",
		.parent = &cci_clk_src.c,
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
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1_ahb_clk.c),
	},
};

static struct branch_clk camss_csi1_clk = {
	.cbcr_reg = CAMSS_CSI1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2_ahb_clk.c),
	},
};

static struct branch_clk camss_csi2_clk = {
	.cbcr_reg = CAMSS_CSI2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi2rdi_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2rdi_clk.c),
	},
};

static struct branch_clk camss_csi_vfe0_clk = {
	.cbcr_reg = CAMSS_CSI_VFE0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_ispif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_ispif_ahb_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg0_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
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
		.dbg_name = "camss_jpeg_jpeg_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_axi_clk.c),
	},
};

static struct branch_clk camss_mclk0_clk = {
	.cbcr_reg = CAMSS_MCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_mclk2_clk",
		.parent = &mclk2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk2_clk.c),
	},
};

static struct branch_clk camss_micro_ahb_clk = {
	.cbcr_reg = CAMSS_MICRO_AHB_CBCR,
	.bcr_reg = CAMSS_MICRO_BCR,
	.has_sibling = 1,
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
		.dbg_name = "camss_phy0_csi0phytimer_clk",
		.parent = &csi0phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy0_csi0phytimer_clk.c),
	},
};

static struct branch_clk camss_phy1_csi1phytimer_clk = {
	.cbcr_reg = CAMSS_PHY1_CSI1PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_phy1_csi1phytimer_clk",
		.parent = &csi1phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy1_csi1phytimer_clk.c),
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
		.dbg_name = "camss_vfe_cpp_clk",
		.parent = &cpp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_cpp_clk.c),
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
		.dbg_name = "camss_vfe_vfe_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe_axi_clk.c),
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
		.dbg_name = "mdss_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_axi_clk.c),
	},
};

static struct branch_clk mdss_byte0_clk = {
	.cbcr_reg = MDSS_BYTE0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_byte0_clk",
		.parent = &byte0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_byte0_clk.c),
	},
};

static struct branch_clk mdss_esc0_clk = {
	.cbcr_reg = MDSS_ESC0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_esc0_clk",
		.parent = &esc0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_esc0_clk.c),
	},
};

static struct branch_clk mdss_mdp_clk = {
	.cbcr_reg = MDSS_MDP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_mdp_clk",
		.parent = &mdp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_clk.c),
	},
};

static struct branch_clk mdss_mdp_lut_clk = {
	.cbcr_reg = MDSS_MDP_LUT_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_mdp_lut_clk",
		.parent = &mdp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_lut_clk.c),
	},
};

static struct branch_clk mdss_pclk0_clk = {
	.cbcr_reg = MDSS_PCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_pclk0_clk",
		.parent = &pclk0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_pclk0_clk.c),
	},
};

static struct branch_clk mdss_vsync_clk = {
	.cbcr_reg = MDSS_VSYNC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
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
		.dbg_name = "mmss_mmssnoc_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mmssnoc_axi_clk.c),
	},
};

static struct branch_clk mmss_s0_axi_clk = {
	.cbcr_reg = MMSS_S0_AXI_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "oxili_gfx3d_clk",
		.parent = &oxili_gfx3d_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(oxili_gfx3d_clk.c),
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
		.dbg_name = "venus0_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_axi_clk.c),
	},
};

static struct branch_clk venus0_ocmemnoc_clk = {
	.cbcr_reg = VENUS0_OCMEMNOC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
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
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "venus0_vcodec0_clk",
		.parent = &vcodec0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_vcodec0_clk.c),
	},
};

#ifdef CONFIG_DEBUG_FS
enum {
	M_ACPU0 = 0,
	M_ACPU1,
	M_ACPU2,
	M_ACPU3,
	M_L2,
};

struct measure_mux_entry {
	struct clk *c;
	int base;
	u32 debug_mux;
};

static struct measure_mux_entry measure_mux[] = {
	{&snoc.c,	GCC_BASE,  0x0000},
	{&cnoc.c,	GCC_BASE,  0x0008},
	{&pnoc.c,	GCC_BASE,  0x0010},
	{&bimc.c,	GCC_BASE,  0x0155},
	{&bimc_gpu.c,	GCC_BASE,  0x015c},
	{&mmssnoc_ahb.c,	MMSS_BASE,  0x0001},

	{&gcc_mss_cfg_ahb_clk.c,	GCC_BASE, 0x0030},
	{&gcc_mss_q6_bimc_axi_clk.c,	GCC_BASE, 0x0031},
	{&gcc_usb_hs_system_clk.c,	GCC_BASE, 0x0060},
	{&gcc_usb_hs_ahb_clk.c,	GCC_BASE, 0x0061},
	{&gcc_usb2a_phy_sleep_clk.c,	GCC_BASE, 0x0063},
	{&gcc_sdcc1_apps_clk.c,	GCC_BASE, 0x0068},
	{&gcc_sdcc1_ahb_clk.c,	GCC_BASE, 0x0069},
	{&gcc_sdcc1_cdccal_sleep_clk.c,	GCC_BASE, 0x006a},
	{&gcc_sdcc1_cdccal_ff_clk.c,	GCC_BASE, 0x006b},
	{&gcc_sdcc2_apps_clk.c,	GCC_BASE, 0x0070},
	{&gcc_sdcc2_ahb_clk.c,	GCC_BASE, 0x0071},
	{&gcc_sdcc3_apps_clk.c,	GCC_BASE, 0x0078},
	{&gcc_sdcc3_ahb_clk.c,	GCC_BASE, 0x0079},
	{&gcc_sdcc4_apps_clk.c,	GCC_BASE, 0x0080},
	{&gcc_sdcc4_ahb_clk.c,	GCC_BASE, 0x0081},
	{&gcc_blsp1_ahb_clk.c,	GCC_BASE, 0x0088},
	{&gcc_blsp1_qup1_spi_apps_clk.c,	GCC_BASE, 0x008a},
	{&gcc_blsp1_qup1_i2c_apps_clk.c,	GCC_BASE, 0x008b},
	{&gcc_blsp1_uart1_apps_clk.c,	GCC_BASE, 0x008c},
	{&gcc_blsp1_qup2_spi_apps_clk.c,	GCC_BASE, 0x008e},
	{&gcc_blsp1_qup2_i2c_apps_clk.c,	GCC_BASE, 0x0090},
	{&gcc_blsp1_uart2_apps_clk.c,	GCC_BASE, 0x0091},
	{&gcc_blsp1_qup3_spi_apps_clk.c,	GCC_BASE, 0x0093},
	{&gcc_blsp1_qup3_i2c_apps_clk.c,	GCC_BASE, 0x0094},
	{&gcc_blsp1_uart3_apps_clk.c,	GCC_BASE, 0x0095},
	{&gcc_blsp1_qup4_spi_apps_clk.c,	GCC_BASE, 0x0098},
	{&gcc_blsp1_qup4_i2c_apps_clk.c,	GCC_BASE, 0x0099},
	{&gcc_blsp1_uart4_apps_clk.c,	GCC_BASE, 0x009a},
	{&gcc_blsp2_ahb_clk.c,	GCC_BASE, 0x00a8},
	{&gcc_blsp2_qup1_spi_apps_clk.c,	GCC_BASE, 0x00aa},
	{&gcc_blsp2_qup1_i2c_apps_clk.c,	GCC_BASE, 0x00ab},
	{&gcc_blsp2_uart1_apps_clk.c,	GCC_BASE, 0x00ac},
	{&gcc_blsp2_qup2_spi_apps_clk.c,	GCC_BASE, 0x00ae},
	{&gcc_blsp2_qup2_i2c_apps_clk.c,	GCC_BASE, 0x00b0},
	{&gcc_blsp2_uart2_apps_clk.c,	GCC_BASE, 0x00b1},
	{&gcc_blsp2_qup3_spi_apps_clk.c,	GCC_BASE, 0x00b3},
	{&gcc_blsp2_qup3_i2c_apps_clk.c,	GCC_BASE, 0x00b4},
	{&gcc_blsp2_uart3_apps_clk.c,	GCC_BASE, 0x00b5},
	{&gcc_blsp2_qup4_spi_apps_clk.c,	GCC_BASE, 0x00b8},
	{&gcc_blsp2_qup4_i2c_apps_clk.c,	GCC_BASE, 0x00b9},
	{&gcc_blsp2_uart4_apps_clk.c,	GCC_BASE, 0x00ba},
	{&gcc_pdm_ahb_clk.c,	GCC_BASE, 0x00d0},
	{&gcc_pdm2_clk.c,	GCC_BASE, 0x00d2},
	{&gcc_prng_ahb_clk.c,	GCC_BASE, 0x00d8},
	{&gcc_bam_dma_ahb_clk.c,	GCC_BASE, 0x00e0},
	{&gcc_tsif_ahb_clk.c,	GCC_BASE, 0x00e8},
	{&gcc_tsif_ref_clk.c,	GCC_BASE, 0x00e9},
	{&gcc_boot_rom_ahb_clk.c,	GCC_BASE, 0x00f8},
	{&gcc_ce1_clk.c,	GCC_BASE, 0x0138},
	{&gcc_ce1_axi_clk.c,	GCC_BASE, 0x0139},
	{&gcc_ce1_ahb_clk.c,	GCC_BASE, 0x013a},
	{&gcc_lpass_q6_axi_clk.c,	GCC_BASE, 0x0160},
	{&gcc_lpass_sys_noc_mport_clk.c,	GCC_BASE, 0x0162},
	{&gcc_lpass_sys_noc_sway_clk.c,	GCC_BASE, 0x0163},

	{&mmss_misc_ahb_clk.c,	MMSS_BASE, 0x0003},
	{&mmss_mmssnoc_axi_clk.c,	MMSS_BASE, 0x0004},
	{&mmss_s0_axi_clk.c,	MMSS_BASE, 0x0005},
	{&ocmemcx_ocmemnoc_clk.c,	MMSS_BASE, 0x0007},
	{&oxilicx_ahb_clk.c,	MMSS_BASE, 0x000a},
	{&oxili_gfx3d_clk.c,	MMSS_BASE, 0x000b},
	{&venus0_vcodec0_clk.c,	MMSS_BASE, 0x000c},
	{&venus0_axi_clk.c,	MMSS_BASE, 0x000d},
	{&venus0_ocmemnoc_clk.c,	MMSS_BASE, 0x000e},
	{&venus0_ahb_clk.c,	MMSS_BASE, 0x000f},
	{&mdss_mdp_clk.c,	MMSS_BASE, 0x0012},
	{&mdss_mdp_lut_clk.c,	MMSS_BASE, 0x0013},
	{&mdss_pclk0_clk.c,	MMSS_BASE, 0x0014},
	{&mdss_vsync_clk.c,	MMSS_BASE, 0x0015},
	{&mdss_byte0_clk.c,	MMSS_BASE, 0x0016},
	{&mdss_esc0_clk.c,	MMSS_BASE, 0x0017},
	{&mdss_ahb_clk.c,	MMSS_BASE, 0x0018},
	{&mdss_axi_clk.c,	MMSS_BASE, 0x0019},
	{&camss_top_ahb_clk.c,	MMSS_BASE, 0x001a},
	{&camss_micro_ahb_clk.c,	MMSS_BASE, 0x001b},
	{&camss_gp0_clk.c,	MMSS_BASE, 0x001c},
	{&camss_gp1_clk.c,	MMSS_BASE, 0x001d},
	{&camss_mclk0_clk.c,	MMSS_BASE, 0x001e},
	{&camss_mclk1_clk.c,	MMSS_BASE, 0x001f},
	{&camss_mclk2_clk.c,	MMSS_BASE, 0x0020},
	{&camss_cci_cci_clk.c,	MMSS_BASE, 0x0021},
	{&camss_cci_cci_ahb_clk.c,	MMSS_BASE, 0x0022},
	{&camss_phy0_csi0phytimer_clk.c,	MMSS_BASE, 0x0023},
	{&camss_phy1_csi1phytimer_clk.c,	MMSS_BASE, 0x0024},
	{&camss_jpeg_jpeg0_clk.c,	MMSS_BASE, 0x0025},
	{&camss_jpeg_jpeg_ahb_clk.c,	MMSS_BASE, 0x0026},
	{&camss_jpeg_jpeg_axi_clk.c,	MMSS_BASE, 0x0027},
	{&camss_vfe_vfe0_clk.c,	MMSS_BASE, 0x0028},
	{&camss_vfe_cpp_clk.c,	MMSS_BASE, 0x0029},
	{&camss_vfe_cpp_ahb_clk.c,	MMSS_BASE, 0x002a},
	{&camss_vfe_vfe_ahb_clk.c,	MMSS_BASE, 0x002b},
	{&camss_vfe_vfe_axi_clk.c,	MMSS_BASE, 0x002c},
	{&camss_ispif_ahb_clk.c,	MMSS_BASE, 0x002d},
	{&camss_csi_vfe0_clk.c,	MMSS_BASE, 0x002e},
	{&camss_csi0_clk.c,	MMSS_BASE, 0x002f},
	{&camss_csi0_ahb_clk.c,	MMSS_BASE, 0x0030},
	{&camss_csi0phy_clk.c,	MMSS_BASE, 0x0031},
	{&camss_csi0rdi_clk.c,	MMSS_BASE, 0x0032},
	{&camss_csi0pix_clk.c,	MMSS_BASE, 0x0033},
	{&camss_csi1_clk.c,	MMSS_BASE, 0x0034},
	{&camss_csi1_ahb_clk.c,	MMSS_BASE, 0x0035},
	{&camss_csi1phy_clk.c,	MMSS_BASE, 0x0036},
	{&camss_csi1rdi_clk.c,	MMSS_BASE, 0x0037},
	{&camss_csi1pix_clk.c,	MMSS_BASE, 0x0038},
	{&camss_csi2_clk.c,	MMSS_BASE, 0x0039},
	{&camss_csi2_ahb_clk.c,	MMSS_BASE, 0x003a},
	{&camss_csi2phy_clk.c,	MMSS_BASE, 0x003b},
	{&camss_csi2rdi_clk.c,	MMSS_BASE, 0x003c},
	{&camss_csi2pix_clk.c,	MMSS_BASE, 0x003d},
	{&camss_csi_vfe1_clk.c,	MMSS_BASE, 0x0053},
	{&camss_vfe_vfe1_clk.c,	MMSS_BASE, 0x0055},
	{&camss_ahb_clk.c,	MMSS_BASE, 0x0056},

	{&krait0_clk.c,		APCS_BASE, M_ACPU0},
	{&krait1_clk.c,		APCS_BASE, M_ACPU1},
	{&krait2_clk.c,		APCS_BASE, M_ACPU2},
	{&krait3_clk.c,		APCS_BASE, M_ACPU3},
	{&l2_clk.c,		APCS_BASE, M_L2},

	{&dummy_clk,	N_BASES, 0x0000},
};

static int measure_clk_set_parent(struct clk *c, struct clk *parent)
{
	struct measure_clk *clk = to_measure_clk(c);
	unsigned long flags;
	u32 regval, clk_sel, i;

	if (!parent)
		return -EINVAL;

	for (i = 0; i < (ARRAY_SIZE(measure_mux) - 1); i++)
		if (measure_mux[i].c == parent)
			break;

	if (measure_mux[i].c == &dummy_clk)
		return -EINVAL;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	/*
	 * Program the test vector, measurement period (sample_ticks)
	 * and scaling multiplier.
	 */
	clk->sample_ticks = 0x10000;
	clk->multiplier = 1;

	switch (measure_mux[i].base) {

	case GCC_BASE:
		writel_relaxed(0, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));
		clk_sel = measure_mux[i].debug_mux;
		break;

	case MMSS_BASE:
		writel_relaxed(0, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));
		clk_sel = 0x02C;
		regval = BVAL(11, 0, measure_mux[i].debug_mux);
		writel_relaxed(regval, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));

		/* Activate debug clock output */
		regval |= BIT(16);
		writel_relaxed(regval, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));
		break;

	case LPASS_BASE:
		writel_relaxed(0, LPASS_REG_BASE(LPASS_DBG_CLK));
		clk_sel = 0x161;
		regval = BVAL(11, 0, measure_mux[i].debug_mux);
		writel_relaxed(regval, LPASS_REG_BASE(LPASS_DBG_CLK));

		/* Activate debug clock output */
		regval |= BIT(20);
		writel_relaxed(regval, LPASS_REG_BASE(LPASS_DBG_CLK));
		break;

	case APCS_BASE:
		clk->multiplier = 4;
		clk_sel = 0x16A;

		if (measure_mux[i].debug_mux == M_L2)
			regval = BIT(12);
		else
			regval = measure_mux[i].debug_mux << 8;
		writel_relaxed(BIT(0), APCS_REG_BASE(L2_CBCR));
		writel_relaxed(regval, APCS_REG_BASE(GLB_CLK_DIAG));
		break;

	default:
		return -EINVAL;
	}

	/* Set debug mux clock index */
	regval = BVAL(8, 0, clk_sel);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	/* Activate debug clock output */
	regval |= BIT(16);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	/* Make sure test vector is set before starting measurements. */
	mb();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned ticks)
{
	/* Stop counters and set the XO4 counter start value. */
	writel_relaxed(ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL));

	/* Wait for timer to become ready. */
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS)) &
			BIT(25)) != 0)
		cpu_relax();

	/* Run measurement and wait for completion. */
	writel_relaxed(BIT(20)|ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL));
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS)) &
			BIT(25)) == 0)
		cpu_relax();

	/* Return measured ticks. */
	return readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS)) &
				BM(24, 0);
}

/*
 * Perform a hardware rate measurement for a given clock.
 * FOR DEBUG USE ONLY: Measurements take ~15 ms!
 */
static unsigned long measure_clk_get_rate(struct clk *c)
{
	unsigned long flags;
	u32 gcc_xo4_reg_backup;
	u64 raw_count_short, raw_count_full;
	struct measure_clk *clk = to_measure_clk(c);
	unsigned ret;

	ret = clk_prepare_enable(&xo.c);
	if (ret) {
		pr_warn("CXO clock failed to enable. Can't measure\n");
		return 0;
	}

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Enable CXO/4 and RINGOSC branch. */
	gcc_xo4_reg_backup = readl_relaxed(GCC_REG_BASE(GCC_XO_DIV4_CBCR));
	writel_relaxed(0x1, GCC_REG_BASE(GCC_XO_DIV4_CBCR));

	/*
	 * The ring oscillator counter will not reset if the measured clock
	 * is not running.  To detect this, run a short measurement before
	 * the full measurement.  If the raw results of the two are the same
	 * then the clock must be off.
	 */

	/* Run a short measurement. (~1 ms) */
	raw_count_short = run_measurement(0x1000);
	/* Run a full measurement. (~14 ms) */
	raw_count_full = run_measurement(clk->sample_ticks);

	writel_relaxed(gcc_xo4_reg_backup, GCC_REG_BASE(GCC_XO_DIV4_CBCR));

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short) {
		ret = 0;
	} else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((clk->sample_ticks * 10) + 35));
		ret = (raw_count_full * clk->multiplier);
	}

	writel_relaxed(0x51A00, GCC_REG_BASE(PLLTEST_PAD_CFG));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	clk_disable_unprepare(&xo.c);

	return ret;
}
#else /* !CONFIG_DEBUG_FS */
static int measure_clk_set_parent(struct clk *clk, struct clk *parent)
{
	return -EINVAL;
}

static unsigned long measure_clk_get_rate(struct clk *clk)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static struct clk_ops clk_ops_measure = {
	.set_parent = measure_clk_set_parent,
	.get_rate = measure_clk_get_rate,
};

static struct measure_clk measure_clk = {
	.c = {
		.dbg_name = "measure_clk",
		.ops = &clk_ops_measure,
		CLK_INIT(measure_clk.c),
	},
	.multiplier = 1,
};

static struct clk_lookup msm_clocks_samarium_rumi[] = {
	CLK_DUMMY("xo",          cxo_pil_lpass_clk, "fe200000.qcom,lpass", OFF),
	CLK_DUMMY("core_clk",          q6ss_xo_clk, "fe200000.qcom,lpass", OFF),
	CLK_DUMMY("bus_clk",  gcc_lpass_q6_axi_clk, "fe200000.qcom,lpass", OFF),
	CLK_DUMMY("iface_clk", q6ss_ahb_lfabif_clk, "fe200000.qcom,lpass", OFF),
	CLK_DUMMY("reg_clk",         q6ss_ahbm_clk, "fe200000.qcom,lpass", OFF),

	CLK_DUMMY("core_clk",  venus_vcodec0_clk,  "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("iface_clk", venus_ahb_clk,      "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("bus_clk",   venus_axi_clk,      "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("mem_clk",   venus_ocmemnoc_clk, "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("core_clk",  venus_vcodec0_clk,  "fd8c1024.qcom,gdsc",  OFF),

	CLK_DUMMY("xo",                CXO_CLK, "fc880000.qcom,mss", OFF),
	CLK_DUMMY("bus_clk",   MSS_BIMC_Q6_CLK, "fc880000.qcom,mss", OFF),
	CLK_DUMMY("iface_clk", MSS_CFG_AHB_CLK, "fc880000.qcom,mss", OFF),
	CLK_DUMMY("mem_clk",  BOOT_ROM_AHB_CLK, "fc880000.qcom,mss", OFF),
	CLK_DUMMY("xo",		XO_CLK,		"fb21b000.qcom,pronto", OFF),
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991e000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991e000.serial", OFF),
	CLK_DUMMY("core_clk",	SDC1_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("iface_clk",	SDC1_P_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("core_clk",	SDC2_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("iface_clk",	SDC2_P_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("core_clk",	USB_HS_SYSTEM_CLK, "msm_otg", OFF),
	CLK_DUMMY("iface_clk",	USB_HS_AHB_CLK,    "msm_otg", OFF),
	CLK_DUMMY("xo",         CXO_OTG_CLK,       "msm_otg", OFF),
	CLK_DUMMY("dfab_clk",	DFAB_CLK,	"msm_sps", OFF),
	CLK_DUMMY("dma_bam_pclk",	DMA_BAM_P_CLK,	"msm_sps", OFF),
	CLK_DUMMY("core_clk",   SPI_CLK,        "spi_qsd.1",  OFF),
	CLK_DUMMY("iface_clk",  SPI_P_CLK,      "spi_qsd.1",  OFF),
	CLK_DUMMY("core_clk", gcc_prng_ahb_clk.c, "f9bff000.qcom,msm-rng", OFF),
	CLK_DUMMY("core_clk",	I2C_CLK,	"f9924000.i2c", OFF),
	CLK_DUMMY("iface_clk",	I2C_P_CLK,	"f9924000.i2c", OFF),

	/* CoreSight clocks */
	CLK_DUMMY("core_clk", qdss_clk.c, "fc326000.tmc", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc320000.tpiu", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc324000.replicator", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc325000.tmc", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc323000.funnel", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc321000.funnel", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc322000.funnel", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc355000.funnel", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc36c000.funnel", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc302000.stm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc34c000.etm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc34d000.etm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc34e000.etm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc34f000.etm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc310000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc311000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc312000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc313000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc314000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc315000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc316000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc317000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc318000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc351000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc352000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc353000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc354000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc350000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc330000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc33c000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc360000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc330000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc33c000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc360000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fd828018.hwevent", OFF),

	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc326000.tmc", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc320000.tpiu", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc324000.replicator", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc325000.tmc", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc323000.funnel", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc321000.funnel", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc322000.funnel", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc355000.funnel", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc36c000.funnel", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc302000.stm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc34c000.etm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc34d000.etm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc34e000.etm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc34f000.etm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc310000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc311000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc312000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc313000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc314000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc315000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc316000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc317000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc318000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc351000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc352000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc353000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc354000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc350000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc330000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc33c000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc360000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc330000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc33c000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc360000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fd828018.hwevent", OFF),

	CLK_DUMMY("core_mmss_clk", mmss_misc_ahb_clk.c, "fd828018.hwevent",
		  OFF),
	CLK_DUMMY("core_clk", gcc_ce1_clk.c, "qseecom", OFF),
	CLK_DUMMY("iface_clk", gcc_ce1_ahb_clk.c, "qseecom", OFF),
	CLK_DUMMY("bus_clk",  gcc_ce1_axi_clk.c, "qseecom", OFF),
	CLK_DUMMY("core_clk_src", qseecom_ce1_clk_src.c, "qseecom", OFF),
};

static struct clk_lookup msm_clocks_samarium[] = {
	/* XO and PLL */
	CLK_LOOKUP("", xo.c, ""),
	CLK_LOOKUP("hfpll_src", xo_a_clk.c, "f9016000.qcom,clock-krait"),
	CLK_LOOKUP("", gpll0.c, ""),
	CLK_LOOKUP("aux_clk", gpll0_ao.c, "f9016000.qcom,clock-krait"),
	CLK_LOOKUP("", gpll4.c, ""),
	CLK_LOOKUP("", mmpll0.c, ""),
	CLK_LOOKUP("", mmpll1.c, ""),
	CLK_LOOKUP("", mmpll3.c, ""),
	CLK_LOOKUP("", mmpll4.c, ""),

	/* measure */
	CLK_LOOKUP("measure", measure_clk.c, "debug"),

	/* RPM and voter */
	CLK_LOOKUP("xo", xo_otg_clk.c, "msm_otg"),
	CLK_LOOKUP("xo", xo_pil_lpass_clk.c, "fe200000.qcom,lpass"),
	CLK_LOOKUP("xo", xo_pil_mss_clk.c, "fc880000.qcom,mss"),
	CLK_LOOKUP("xo", xo_wlan_clk.c, "fb000000.qcom,wcnss-wlan"),
	CLK_LOOKUP("rf_clk", rf_clk3.c, "fb000000.qcom,wcnss-wlan"),
	CLK_LOOKUP("xo", xo_pil_pronto_clk.c, "fb21b000.qcom,pronto"),
	CLK_LOOKUP("xo", xo_ehci_host_clk.c, "msm_ehci_host"),
	CLK_LOOKUP("xo", xo_lpm_clk.c, "fc4281d0.qcom,mpm"),

	CLK_LOOKUP("", bb_clk1.c, ""),
	CLK_LOOKUP("", bb_clk1_a.c, ""),
	CLK_LOOKUP("", bb_clk2.c, ""),
	CLK_LOOKUP("", bb_clk2_a.c, ""),
	CLK_LOOKUP("", rf_clk1.c, ""),
	CLK_LOOKUP("", rf_clk1_a.c, ""),
	CLK_LOOKUP("", rf_clk2.c, ""),
	CLK_LOOKUP("", rf_clk2_a.c, ""),
	CLK_LOOKUP("", rf_clk3_a.c, ""),
	CLK_LOOKUP("", div_clk1.c, ""),
	CLK_LOOKUP("", div_clk1_a.c, ""),
	CLK_LOOKUP("", div_clk2.c, ""),
	CLK_LOOKUP("", div_clk2_a.c, ""),
	CLK_LOOKUP("", div_clk3.c, ""),
	CLK_LOOKUP("", div_clk3_a.c, ""),
	CLK_LOOKUP("", diff_clk1.c, ""),
	CLK_LOOKUP("", diff_clk1_a.c, ""),
	CLK_LOOKUP("", bb_clk1_pin.c, ""),
	CLK_LOOKUP("", bb_clk1_a_pin.c, ""),
	CLK_LOOKUP("", bb_clk2_pin.c, ""),
	CLK_LOOKUP("ref_clk", bb_clk2_a_pin.c, "3-000e"),
	CLK_LOOKUP("", rf_clk1_pin.c, ""),
	CLK_LOOKUP("", rf_clk1_a_pin.c, ""),
	CLK_LOOKUP("", rf_clk2_pin.c, ""),
	CLK_LOOKUP("", rf_clk2_a_pin.c, ""),
	CLK_LOOKUP("", rf_clk3_pin.c, ""),
	CLK_LOOKUP("", rf_clk3_a_pin.c, ""),
	CLK_LOOKUP("", cnoc.c, ""),
	CLK_LOOKUP("", cnoc_a_clk.c, ""),
	CLK_LOOKUP("", pnoc.c, ""),
	CLK_LOOKUP("", pnoc_a_clk.c, ""),
	CLK_LOOKUP("dfab_clk", pnoc_sps_clk.c, "msm_sps"),
	CLK_LOOKUP("", snoc.c, ""),
	CLK_LOOKUP("", snoc_a_clk.c, ""),
	CLK_LOOKUP("", bimc.c, ""),
	CLK_LOOKUP("", bimc_a_clk.c, ""),
	CLK_LOOKUP("", bimc_gpu.c, ""),
	CLK_LOOKUP("", pnoc_keepalive_a_clk.c, ""),
	CLK_LOOKUP("", mmssnoc_ahb.c, ""),
	CLK_LOOKUP("", mmssnoc_ahb_a_clk.c, ""),

	/* Bus driver */
	CLK_LOOKUP("bus_clk", cnoc_msmbus_clk.c, "msm_config_noc"),
	CLK_LOOKUP("bus_a_clk", cnoc_msmbus_a_clk.c, "msm_config_noc"),
	CLK_LOOKUP("bus_clk", snoc_msmbus_clk.c, "msm_sys_noc"),
	CLK_LOOKUP("bus_a_clk", snoc_msmbus_a_clk.c, "msm_sys_noc"),
	CLK_LOOKUP("bus_clk", pnoc_msmbus_clk.c, "msm_periph_noc"),
	CLK_LOOKUP("bus_a_clk", pnoc_msmbus_a_clk.c, "msm_periph_noc"),
	CLK_LOOKUP("mem_clk", bimc_msmbus_clk.c, "msm_bimc"),
	CLK_LOOKUP("mem_a_clk", bimc_msmbus_a_clk.c, "msm_bimc"),
	CLK_LOOKUP("ocmem_clk", ocmemgx_msmbus_clk.c, "msm_bus"),
	CLK_LOOKUP("ocmem_a_clk", ocmemgx_msmbus_a_clk.c, "msm_bus"),
	CLK_LOOKUP("bus_clk", mmss_s0_axi_clk.c, "msm_mmss_noc"),
	CLK_LOOKUP("bus_a_clk", mmss_s0_axi_clk.c, "msm_mmss_noc"),

	/* CoreSight clocks */
	CLK_LOOKUP("core_clk", qdss.c, "fc326000.tmc"),
	CLK_LOOKUP("core_clk", qdss.c, "fc320000.tpiu"),
	CLK_LOOKUP("core_clk", qdss.c, "fc324000.replicator"),
	CLK_LOOKUP("core_clk", qdss.c, "fc325000.tmc"),
	CLK_LOOKUP("core_clk", qdss.c, "fc323000.funnel"),
	CLK_LOOKUP("core_clk", qdss.c, "fc321000.funnel"),
	CLK_LOOKUP("core_clk", qdss.c, "fc322000.funnel"),
	CLK_LOOKUP("core_clk", qdss.c, "fc355000.funnel"),
	CLK_LOOKUP("core_clk", qdss.c, "fc36c000.funnel"),
	CLK_LOOKUP("core_clk", qdss.c, "fc302000.stm"),
	CLK_LOOKUP("core_clk", qdss.c, "fc34c000.etm"),
	CLK_LOOKUP("core_clk", qdss.c, "fc34d000.etm"),
	CLK_LOOKUP("core_clk", qdss.c, "fc34e000.etm"),
	CLK_LOOKUP("core_clk", qdss.c, "fc34f000.etm"),
	CLK_LOOKUP("core_clk", qdss.c, "fc310000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc311000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc312000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc313000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc314000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc315000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc316000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc317000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc318000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc351000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc352000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc353000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc354000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc350000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc330000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc33c000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fc360000.cti"),
	CLK_LOOKUP("core_clk", qdss.c, "fd828018.hwevent"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc326000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc320000.tpiu"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc324000.replicator"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc325000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc323000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc321000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc322000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc355000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc36c000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc302000.stm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc34c000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc34d000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc34e000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc34f000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc310000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc311000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc312000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc313000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc314000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc315000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc316000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc317000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc318000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc351000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc352000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc353000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc354000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc350000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc330000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc33c000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc360000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fd828018.hwevent"),

	CLK_LOOKUP("osr_clk", div_clk1.c, "msm-dai-q6-dev.16384"),

	/* BLSP */
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f991f000.serial"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9924000.i2c"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f991e000.serial"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9923000.i2c"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup1_i2c_apps_clk.c, "f9923000.i2c"),
	CLK_LOOKUP("iface_clk", gcc_blsp2_ahb_clk.c, "f9963000.i2c"),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup1_i2c_apps_clk.c, "f9963000.i2c"),
	CLK_LOOKUP("iface_clk", gcc_blsp2_ahb_clk.c, "f9964000.i2c"),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup2_i2c_apps_clk.c, "f9964000.i2c"),
	CLK_LOOKUP("", gcc_blsp1_qup1_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup2_i2c_apps_clk.c, "f9924000.i2c"),
	CLK_LOOKUP("", gcc_blsp1_qup2_spi_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp1_qup3_i2c_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp1_qup3_spi_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp1_qup4_i2c_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp1_qup4_spi_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp1_uart1_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart2_apps_clk.c, "f991e000.serial"),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart3_apps_clk.c, "f991f000.serial"),
	CLK_LOOKUP("", gcc_blsp1_uart4_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_ahb_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_qup1_i2c_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_qup1_spi_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_qup2_i2c_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_qup2_spi_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_qup3_i2c_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_qup3_spi_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_qup4_i2c_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_qup4_spi_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_uart1_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_uart2_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_uart3_apps_clk.c, ""),
	CLK_LOOKUP("", gcc_blsp2_uart4_apps_clk.c, ""),

	/* SDCC */
	CLK_LOOKUP("iface_clk", gcc_sdcc1_ahb_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("core_clk", gcc_sdcc1_apps_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("cal_clk", gcc_sdcc1_cdccal_ff_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("sleep_clk", gcc_sdcc1_cdccal_sleep_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("iface_clk", gcc_sdcc2_ahb_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("core_clk", gcc_sdcc2_apps_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("iface_clk", gcc_sdcc3_ahb_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("core_clk", gcc_sdcc3_apps_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("iface_clk", gcc_sdcc4_ahb_clk.c, "msm_sdcc.4"),
	CLK_LOOKUP("core_clk", gcc_sdcc4_apps_clk.c, "msm_sdcc.4"),

	/* SCM PAS */
	CLK_LOOKUP("core_clk",     gcc_ce1_clk.c,         "scm"),
	CLK_LOOKUP("iface_clk",    gcc_ce1_ahb_clk.c,     "scm"),
	CLK_LOOKUP("bus_clk",      gcc_ce1_axi_clk.c,     "scm"),
	CLK_LOOKUP("core_clk_src", scm_ce1_clk_src.c,     "scm"),

	/* Misc GCC branch */
	CLK_LOOKUP("", ce1_clk_src.c, ""),
	CLK_LOOKUP("dma_bam_pclk", gcc_bam_dma_ahb_clk.c, "msm_sps"),
	CLK_LOOKUP("mem_clk", gcc_boot_rom_ahb_clk.c, "fc880000.qcom,mss"),
	CLK_LOOKUP("", gcc_ce1_ahb_clk.c, ""),
	CLK_LOOKUP("", gcc_ce1_axi_clk.c, ""),
	CLK_LOOKUP("", gcc_ce1_clk.c, ""),
	CLK_LOOKUP("bus_clk", gcc_lpass_q6_axi_clk.c, "fe200000.qcom,lpass"),
	CLK_LOOKUP("mport_clk", gcc_lpass_sys_noc_mport_clk.c,
					   "fe200000.qcom,lpass"),
	CLK_LOOKUP("sway_clk", gcc_lpass_sys_noc_sway_clk.c,
					   "fe200000.qcom,lpass"),
	CLK_LOOKUP("core_clk", dummy_clk, "fe200000.qcom,lpass"),
	CLK_LOOKUP("iface_clk", dummy_clk, "fe200000.qcom,lpass"),
	CLK_LOOKUP("reg_clk", dummy_clk, "fe200000.qcom,lpass"),
	CLK_LOOKUP("iface_clk", gcc_mss_cfg_ahb_clk.c, "fc880000.qcom,mss"),
	CLK_LOOKUP("bus_clk", gcc_mss_q6_bimc_axi_clk.c, "fc880000.qcom,mss"),
	CLK_LOOKUP("", gcc_pdm2_clk.c, ""),
	CLK_LOOKUP("", gcc_pdm_ahb_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_prng_ahb_clk.c, "f9bff000.qcom,msm-rng"),
	CLK_LOOKUP("", gcc_tsif_ref_clk.c, ""),
	CLK_LOOKUP("", gcc_tsif_ahb_clk.c, ""),
	CLK_LOOKUP("", gcc_usb2a_phy_sleep_clk.c, ""),
	CLK_LOOKUP("iface_clk", gcc_usb_hs_ahb_clk.c, "msm_otg"),
	CLK_LOOKUP("core_clk", gcc_usb_hs_system_clk.c, "msm_otg"),

	/* MM sensor clocks */
	CLK_LOOKUP("cam_src_clk", mclk0_clk_src.c, "6e.qcom,camera"),
	CLK_LOOKUP("cam_src_clk", mclk0_clk_src.c, "20.qcom,camera"),
	CLK_LOOKUP("cam_src_clk", mclk0_clk_src.c, "0.qcom,camera"),
	CLK_LOOKUP("cam_src_clk", mclk1_clk_src.c, "90.qcom,camera"),
	CLK_LOOKUP("cam_src_clk", mclk1_clk_src.c, "6d.qcom,camera"),
	CLK_LOOKUP("cam_src_clk", mclk1_clk_src.c, "1.qcom,camera"),
	CLK_LOOKUP("cam_src_clk", mclk2_clk_src.c, "6c.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_mclk0_clk.c, "6e.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_mclk0_clk.c, "20.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_mclk0_clk.c, "0.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_mclk1_clk.c, "90.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_mclk1_clk.c, "6d.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_mclk1_clk.c, "1.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_mclk2_clk.c, "6c.qcom,camera"),

	/* CCI clocks */
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
						"fda0c000.qcom,cci"),
	CLK_LOOKUP("cci_ahb_clk", camss_cci_cci_ahb_clk.c, "fda0c000.qcom,cci"),
	CLK_LOOKUP("cci_src_clk", cci_clk_src.c, "fda0c000.qcom,cci"),
	CLK_LOOKUP("cci_clk", camss_cci_cci_clk.c, "fda0c000.qcom,cci"),

	/* CSIPHY clocks */
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
						"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
						"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_src_clk", csi0phytimer_clk_src.c,
						"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_clk", camss_phy0_csi0phytimer_clk.c,
						"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
						"fda0b000.qcom,csiphy"),
	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
						"fda0b000.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_src_clk", csi1phytimer_clk_src.c,
						"fda0b000.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_clk", camss_phy1_csi1phytimer_clk.c,
						"fda0b000.qcom,csiphy"),

	/* CSID clocks */
	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
					"fda08000.qcom,csid"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda08000.qcom,csid"),
	CLK_LOOKUP("csi_ahb_clk", camss_csi0_ahb_clk.c, "fda08000.qcom,csid"),
	CLK_LOOKUP("csi_src_clk", csi0_clk_src.c, "fda08000.qcom,csid"),
	CLK_LOOKUP("csi_phy_clk", camss_csi0phy_clk.c, "fda08000.qcom,csid"),
	CLK_LOOKUP("csi_clk", camss_csi0_clk.c, "fda08000.qcom,csid"),
	CLK_LOOKUP("csi_pix_clk", camss_csi0pix_clk.c, "fda08000.qcom,csid"),
	CLK_LOOKUP("csi_rdi_clk", camss_csi0rdi_clk.c, "fda08000.qcom,csid"),

	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
					"fda08400.qcom,csid"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda08400.qcom,csid"),
	CLK_LOOKUP("csi_ahb_clk", camss_csi1_ahb_clk.c, "fda08400.qcom,csid"),
	CLK_LOOKUP("csi_src_clk", csi1_clk_src.c, "fda08400.qcom,csid"),
	CLK_LOOKUP("csi_phy_clk", camss_csi1phy_clk.c, "fda08400.qcom,csid"),
	CLK_LOOKUP("csi_clk", camss_csi1_clk.c, "fda08400.qcom,csid"),
	CLK_LOOKUP("csi_pix_clk", camss_csi1pix_clk.c, "fda08400.qcom,csid"),
	CLK_LOOKUP("csi_rdi_clk", camss_csi1rdi_clk.c, "fda08400.qcom,csid"),

	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
					"fda08800.qcom,csid"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda08800.qcom,csid"),
	CLK_LOOKUP("csi_ahb_clk", camss_csi2_ahb_clk.c, "fda08800.qcom,csid"),
	CLK_LOOKUP("csi_src_clk", csi2_clk_src.c, "fda08800.qcom,csid"),
	CLK_LOOKUP("csi_phy_clk", camss_csi2phy_clk.c, "fda08800.qcom,csid"),
	CLK_LOOKUP("csi_clk", camss_csi2_clk.c, "fda08800.qcom,csid"),
	CLK_LOOKUP("csi_pix_clk", camss_csi2pix_clk.c, "fda08800.qcom,csid"),
	CLK_LOOKUP("csi_rdi_clk", camss_csi2rdi_clk.c, "fda08800.qcom,csid"),

	/* ISPIF clocks */
	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP("vfe0_clk_src", vfe0_clk_src.c, "fda0a000.qcom,ispif"),
	CLK_LOOKUP("camss_vfe_vfe0_clk", camss_vfe_vfe0_clk.c,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP("camss_csi_vfe0_clk", camss_csi_vfe0_clk.c,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP("vfe1_clk_src", vfe1_clk_src.c, "fda0a000.qcom,ispif"),
	CLK_LOOKUP("camss_vfe_vfe1_clk", camss_vfe_vfe1_clk.c,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP("camss_csi_vfe1_clk", camss_csi_vfe1_clk.c,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP("csi0_src_clk", csi0_clk_src.c, "fda0a000.qcom,ispif"),
	CLK_LOOKUP("csi0_clk", camss_csi0_clk.c, "fda0a000.qcom,ispif"),
	CLK_LOOKUP("csi0_pix_clk", camss_csi0pix_clk.c,
					"fda0a000.qcom,ispif"),
	CLK_LOOKUP("csi0_rdi_clk", camss_csi0rdi_clk.c,
					"fda0a000.qcom,ispif"),

	/* CPP clocks */
	CLK_LOOKUP("micro_iface_clk", camss_micro_ahb_clk.c,
					"fda04000.qcom,cpp"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda04000.qcom,cpp"),
	CLK_LOOKUP("cpp_iface_clk", camss_vfe_cpp_ahb_clk.c,
					"fda04000.qcom,cpp"),
	CLK_LOOKUP("cpp_core_clk", camss_vfe_cpp_clk.c, "fda04000.qcom,cpp"),
	CLK_LOOKUP("cpp_bus_clk", camss_vfe_vfe_axi_clk.c, "fda04000.qcom,cpp"),
	CLK_LOOKUP("vfe_clk_src", vfe0_clk_src.c, "fda04000.qcom,cpp"),
	CLK_LOOKUP("camss_vfe_vfe_clk", camss_vfe_vfe0_clk.c,
					"fda04000.qcom,cpp"),
	CLK_LOOKUP("iface_clk", camss_vfe_vfe_ahb_clk.c, "fda04000.qcom,cpp"),

	/* GDSC */
	CLK_LOOKUP("core_clk", venus0_vcodec0_clk.c, "fd8c1024.qcom,gdsc"),
	CLK_LOOKUP("core0_clk", camss_vfe_vfe0_clk.c, "fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("cpp_clk", camss_vfe_cpp_clk.c, "fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("core_clk", oxili_gfx3d_clk.c, "fd8c4024.qcom,gdsc"),
	CLK_LOOKUP("core_clk", mdss_mdp_clk.c, "fd8c2304.qcom,gdsc"),
	CLK_LOOKUP("lut_clk", mdss_mdp_lut_clk.c, "fd8c2304.qcom,gdsc"),

	/* DSI PLL clocks */
	CLK_LOOKUP("", dsi_vco_clk_samarium.c, ""),
	CLK_LOOKUP("", analog_postdiv_clk_samarium.c, ""),
	CLK_LOOKUP("", indirect_path_div2_clk_samarium.c, ""),
	CLK_LOOKUP("", pixel_clk_src_samarium.c, ""),
	CLK_LOOKUP("", byte_mux_samarium.c, ""),
	CLK_LOOKUP("", byte_clk_src_samarium.c, ""),

	/* MMSS */
	CLK_LOOKUP("", axi_clk_src.c, ""),
	CLK_LOOKUP("", camss_ahb_clk.c, ""),
	CLK_LOOKUP("", camss_gp0_clk.c, ""),
	CLK_LOOKUP("", camss_gp1_clk.c, ""),
	CLK_LOOKUP("", camss_jpeg_jpeg0_clk.c, ""),
	CLK_LOOKUP("", camss_jpeg_jpeg_ahb_clk.c, ""),
	CLK_LOOKUP("", camss_jpeg_jpeg_axi_clk.c, ""),
	CLK_LOOKUP("", gfx3d.c, ""),
	CLK_LOOKUP("", gfx3d_a_clk.c, ""),
	CLK_LOOKUP("", jpeg0_clk_src.c, ""),
	CLK_LOOKUP("core_clk_src", mdp_clk_src.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd928000.qcom,iommu"),
	CLK_LOOKUP("bus_clk", mdss_axi_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("core_clk", mdss_axi_clk.c, "fd928000.qcom,iommu"),
	CLK_LOOKUP("bus_clk", mdss_axi_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("", byte0_clk_src.c, ""),
	CLK_LOOKUP("", pclk0_clk_src.c, ""),
	CLK_LOOKUP("byte_clk", mdss_byte0_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("core_clk", mdss_esc0_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("core_clk", mdss_mdp_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("mdp_core_clk", mdss_mdp_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("core_clk", mdss_mdp_clk.c, "fd8c2304.qcom,gdsc"),
	CLK_LOOKUP("lut_clk", mdss_mdp_lut_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("lut_clk", mdss_mdp_lut_clk.c, "fd8c2304.qcom,gdsc"),
	CLK_LOOKUP("pixel_clk", mdss_pclk0_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("vsync_clk", mdss_vsync_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("core_mmss_clk", mmss_misc_ahb_clk.c, "fd828018.hwevent"),
	CLK_LOOKUP("", mmss_mmssnoc_axi_clk.c, ""),
	CLK_LOOKUP("", ocmemgx.c, ""),
	CLK_LOOKUP("", ocmemgx_a_clk.c, ""),
	CLK_LOOKUP("core_clk", ocmemgx_core_clk.c, "fdd00000.qcom,ocmem"),
	CLK_LOOKUP("iface_clk", ocmemcx_ocmemnoc_clk.c, "fdd00000.qcom,ocmem"),
	CLK_LOOKUP("", ocmemnoc_clk_src.c, ""),
	CLK_LOOKUP("core_clk", oxili_gfx3d_clk.c, "fdb00000.qcom,kgsl-3d0"),
	CLK_LOOKUP("iface_clk", oxilicx_ahb_clk.c, "fdb00000.qcom,kgsl-3d0"),
	CLK_LOOKUP("iface_clk", venus0_ahb_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("bus_clk", venus0_axi_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("mem_clk", venus0_ocmemnoc_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("core_clk", venus0_vcodec0_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("core_clk", venus0_vcodec0_clk.c, "fdc00000.qcom,vidc"),
	CLK_LOOKUP("iface_clk",  venus0_ahb_clk.c, "fdc00000.qcom,vidc"),
	CLK_LOOKUP("bus_clk",  venus0_axi_clk.c, "fdc00000.qcom,vidc"),
	CLK_LOOKUP("mem_clk",  venus0_ocmemnoc_clk.c, "fdc00000.qcom,vidc"),

	CLK_LOOKUP("iface_clk", oxilicx_ahb_clk.c, "fdb10000.qcom,iommu"),
	CLK_LOOKUP("core_clk", oxili_gfx3d_clk.c, "fdb10000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd928000.qcom,iommu"),
	CLK_LOOKUP("core_clk", mdss_axi_clk.c, "fd928000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", camss_vfe_vfe_ahb_clk.c, "fda44000.qcom,iommu"),
	CLK_LOOKUP("core_clk", camss_vfe_vfe_axi_clk.c, "fda44000.qcom,iommu"),
	CLK_LOOKUP("alt_iface_clk", camss_ahb_clk.c,  "fda44000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", camss_top_ahb_clk.c, "fda44000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", venus0_ahb_clk.c, "fdc84000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", venus0_vcodec0_clk.c, "fdc84000.qcom,iommu"),
	CLK_LOOKUP("core_clk", venus0_axi_clk.c, "fdc84000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", camss_jpeg_jpeg_ahb_clk.c,
					 "fda64000.qcom,iommu"),
	CLK_LOOKUP("core_clk", camss_jpeg_jpeg_axi_clk.c,
					 "fda64000.qcom,iommu"),
	CLK_LOOKUP("alt_iface_clk", camss_ahb_clk.c,  "fda64000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", camss_top_ahb_clk.c, "fda64000.qcom,iommu"),
};

static struct pll_config_regs mmpll0_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL0_L_VAL,
	.m_reg = (void __iomem *)MMPLL0_M_VAL,
	.n_reg = (void __iomem *)MMPLL0_N_VAL,
	.config_reg = (void __iomem *)MMPLL0_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL0_MODE,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL0 at 800 MHz, main output enabled. */
static struct pll_config mmpll0_config __initdata = {
	.l = 41,
	.m = 2,
	.n = 3,
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

static struct pll_config_regs mmpll1_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL1_L_VAL,
	.m_reg = (void __iomem *)MMPLL1_M_VAL,
	.n_reg = (void __iomem *)MMPLL1_N_VAL,
	.config_reg = (void __iomem *)MMPLL1_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL1_MODE,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL1 at 900MHz, main output enabled. */
static struct pll_config mmpll1_config __initdata = {
	.l = 46,
	.m = 7,
	.n = 8,
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

static struct pll_config_regs mmpll3_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL3_L_VAL,
	.m_reg = (void __iomem *)MMPLL3_M_VAL,
	.n_reg = (void __iomem *)MMPLL3_N_VAL,
	.config_reg = (void __iomem *)MMPLL3_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL3_MODE,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL3 at 930 MHz, main output enabled. */
static struct pll_config mmpll3_config __initdata = {
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

static struct pll_config_regs mmpll4_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL4_L_VAL,
	.m_reg = (void __iomem *)MMPLL4_M_VAL,
	.n_reg = (void __iomem *)MMPLL4_N_VAL,
	.config_reg = (void __iomem *)MMPLL4_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL4_MODE,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL4 at 930 MHz, main output enabled. */
static struct pll_config mmpll4_config __initdata = {
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

static void __init reg_init(void)
{
	u32 regval;

	/* MMPLL init */
	configure_sr_hpm_lp_pll(&mmpll0_config, &mmpll0_regs, 1);
	configure_sr_hpm_lp_pll(&mmpll1_config, &mmpll1_regs, 1);
	configure_sr_hpm_lp_pll(&mmpll3_config, &mmpll3_regs, 0);
	configure_sr_hpm_lp_pll(&mmpll4_config, &mmpll4_regs, 0);

	/* Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));

	/* Vote for LPASS and MMSS controller to use GPLL0 */
	regval = readl_relaxed(GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
	writel_relaxed(regval | BIT(26) | BIT(25),
			GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
}

static void __init msmsamarium_clock_post_init(void)
{
	/*
	 * Hold an active set vote at a rate of 40MHz for the MMSS NOC AHB
	 * source. Sleep set vote is 0.
	 */
	/* enable for MMSS */
	clk_set_rate(&mmssnoc_ahb_a_clk.c, 40000000);
	clk_prepare_enable(&mmssnoc_ahb_a_clk.c);

	/*
	 * Hold an active set vote for the PNOC AHB source. Sleep set vote is 0.
	 */
	clk_set_rate(&pnoc_keepalive_a_clk.c, 19200000);
	clk_prepare_enable(&pnoc_keepalive_a_clk.c);

	/*
	 * Hold an active set vote for CXO; this is because CXO is expected
	 * to remain on whenever CPUs aren't power collapsed.
	 */
	clk_prepare_enable(&xo_a_clk.c);
}

#define GCC_CC_PHYS		0xFC400000
#define GCC_CC_SIZE		SZ_8K

#define MMSS_CC_PHYS		0xFD8C0000
#define MMSS_CC_SIZE		SZ_32K

#define APCS_GCC_CC_PHYS	0xF9011000
#define APCS_GCC_CC_SIZE	SZ_4K

static void __init msmsamarium_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-samarium: Unable to ioremap GCC memory!");

	virt_bases[MMSS_BASE] = ioremap(MMSS_CC_PHYS, MMSS_CC_SIZE);
	if (!virt_bases[MMSS_BASE])
		panic("clock-samarium: Unable to ioremap MMSS_CC memory!");

	virt_bases[APCS_BASE] = ioremap(APCS_GCC_CC_PHYS, APCS_GCC_CC_SIZE);
	if (!virt_bases[APCS_BASE])
		panic("clock-samarium: Unable to ioremap APCS_GCC_CC memory!");

	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0]))
		panic("clock-samarium: Unable to get the vdd_dig regulator!");

	enable_rpm_scaling();

	reg_init();

	/*
	 * MDSS needs the ahb clock and needs to init before we register the
	 * lookup table.
	 */
	mdss_clk_ctrl_pre_init(&mdss_ahb_clk.c);
}

static void __init msmsamarium_rumi_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-samarium: Unable to ioremap GCC memory!");

	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0]))
		panic("clock-samarium: Unable to get the vdd_dig regulator!");
}

struct clock_init_data msmsamarium_rumi_clock_init_data __initdata = {
	.table = msm_clocks_samarium_rumi,
	.size = ARRAY_SIZE(msm_clocks_samarium_rumi),
	.pre_init = msmsamarium_rumi_clock_pre_init,
};

struct clock_init_data msmsamarium_clock_init_data __initdata = {
	.table = msm_clocks_samarium,
	.size = ARRAY_SIZE(msm_clocks_samarium),
	.pre_init = msmsamarium_clock_pre_init,
	.post_init = msmsamarium_clock_post_init,
};

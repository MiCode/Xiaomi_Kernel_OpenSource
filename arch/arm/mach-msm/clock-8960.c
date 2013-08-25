/* Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
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
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <asm/clkdev.h>
#include <asm/mach-types.h>

#include <mach/msm_iomap.h>
#include <mach/rpm-regulator.h>
#include <mach/socinfo.h>

#include "clock.h"
#include "clock-local.h"
#include "clock-rpm.h"
#include "clock-voter.h"
#include "clock-dss-8960.h"
#include "devices.h"
#include "clock-pll.h"

#define REG(off)	(MSM_CLK_CTL_BASE + (off))
#define REG_MM(off)	(MSM_MMSS_CLK_CTL_BASE + (off))
#define REG_LPA(off)	(MSM_LPASS_CLK_CTL_BASE + (off))
#define REG_GCC(off)	(MSM_APCS_GCC_BASE + (off))

/* Peripheral clock registers. */
#define ADM0_PBUS_CLK_CTL_REG			REG(0x2208)
#define SFAB_SATA_S_HCLK_CTL_REG		REG(0x2480)
#define CE1_HCLK_CTL_REG			REG(0x2720)
#define CE1_CORE_CLK_CTL_REG			REG(0x2724)
#define PRNG_CLK_NS_REG				REG(0x2E80)
#define CE3_HCLK_CTL_REG			REG(0x36C4)
#define CE3_CORE_CLK_CTL_REG			REG(0x36CC)
#define CE3_CLK_SRC_NS_REG			REG(0x36C0)
#define DMA_BAM_HCLK_CTL			REG(0x25C0)
#define CLK_HALT_AFAB_SFAB_STATEA_REG		REG(0x2FC0)
#define CLK_HALT_AFAB_SFAB_STATEB_REG		REG(0x2FC4)
#define CLK_HALT_CFPB_STATEA_REG		REG(0x2FCC)
#define CLK_HALT_CFPB_STATEB_REG		REG(0x2FD0)
#define CLK_HALT_CFPB_STATEC_REG		REG(0x2FD4)
#define CLK_HALT_DFAB_STATE_REG			REG(0x2FC8)
/* 8064 name CLK_HALT_GSS_KPSS_MISC_STATE_REG */
#define CLK_HALT_MSS_SMPSS_MISC_STATE_REG	REG(0x2FDC)
#define CLK_HALT_SFPB_MISC_STATE_REG		REG(0x2FD8)
#define CLK_HALT_AFAB_SFAB_STATEB_REG		REG(0x2FC4)
#define CLK_TEST_REG				REG(0x2FA0)
#define GPn_MD_REG(n)				REG(0x2D00+(0x20*(n)))
#define GPn_NS_REG(n)				REG(0x2D24+(0x20*(n)))
#define GSBIn_HCLK_CTL_REG(n)			REG(0x29C0+(0x20*((n)-1)))
#define GSBIn_QUP_APPS_MD_REG(n)		REG(0x29C8+(0x20*((n)-1)))
#define GSBIn_QUP_APPS_NS_REG(n)		REG(0x29CC+(0x20*((n)-1)))
#define GSBIn_RESET_REG(n)			REG(0x29DC+(0x20*((n)-1)))
#define GSBIn_UART_APPS_MD_REG(n)		REG(0x29D0+(0x20*((n)-1)))
#define GSBIn_UART_APPS_NS_REG(n)		REG(0x29D4+(0x20*((n)-1)))
#define PDM_CLK_NS_REG				REG(0x2CC0)
/* 8064 name BB_PLL_ENA_APCS_REG */
#define BB_PLL_ENA_SC0_REG			REG(0x34C0)
#define BB_PLL_ENA_RPM_REG			REG(0x34A0)
#define BB_PLL0_STATUS_REG			REG(0x30D8)
#define BB_PLL5_STATUS_REG			REG(0x30F8)
#define BB_PLL6_STATUS_REG			REG(0x3118)
#define BB_PLL7_STATUS_REG			REG(0x3138)
#define BB_PLL8_L_VAL_REG			REG(0x3144)
#define BB_PLL8_M_VAL_REG			REG(0x3148)
#define BB_PLL8_MODE_REG			REG(0x3140)
#define BB_PLL8_N_VAL_REG			REG(0x314C)
#define BB_PLL8_STATUS_REG			REG(0x3158)
#define BB_PLL8_CONFIG_REG			REG(0x3154)
#define BB_PLL8_TEST_CTL_REG			REG(0x3150)
#define BB_MMCC_PLL2_MODE_REG			REG(0x3160)
#define BB_MMCC_PLL2_L_REG			REG(0x3164)
#define BB_MMCC_PLL2_M_REG			REG(0x3168)
#define BB_MMCC_PLL2_N_REG			REG(0x316C)
#define BB_MMCC_PLL2_TEST_CTL_REG		REG(0x3170)
#define BB_MMCC_PLL2_CONFIG_REG			REG(0x3174)
#define BB_MMCC_PLL2_STATUS_REG			REG(0x3178)
#define BB_PLL14_MODE_REG			REG(0x31C0)
#define BB_PLL14_L_VAL_REG			REG(0x31C4)
#define BB_PLL14_M_VAL_REG			REG(0x31C8)
#define BB_PLL14_N_VAL_REG			REG(0x31CC)
#define BB_PLL14_TEST_CTL_REG			REG(0x31D0)
#define BB_PLL14_CONFIG_REG			REG(0x31D4)
#define BB_PLL14_STATUS_REG			REG(0x31D8)
#define PLLTEST_PAD_CFG_REG			REG(0x2FA4)
#define PMEM_ACLK_CTL_REG			REG(0x25A0)
#define RINGOSC_NS_REG				REG(0x2DC0)
#define RINGOSC_STATUS_REG			REG(0x2DCC)
#define RINGOSC_TCXO_CTL_REG			REG(0x2DC4)
#define RPM_MSG_RAM_HCLK_CTL_REG		REG(0x27E0)
#define SC0_U_CLK_BRANCH_ENA_VOTE_REG		REG(0x3080)
#define SDCn_APPS_CLK_MD_REG(n)			REG(0x2828+(0x20*((n)-1)))
#define SDCn_APPS_CLK_NS_REG(n)			REG(0x282C+(0x20*((n)-1)))
#define SDCn_HCLK_CTL_REG(n)			REG(0x2820+(0x20*((n)-1)))
#define SDCn_RESET_REG(n)			REG(0x2830+(0x20*((n)-1)))
#define SLIMBUS_XO_SRC_CLK_CTL_REG		REG(0x2628)
#define TSIF_HCLK_CTL_REG			REG(0x2700)
#define TSIF_REF_CLK_MD_REG			REG(0x270C)
#define TSIF_REF_CLK_NS_REG			REG(0x2710)
#define TSSC_CLK_CTL_REG			REG(0x2CA0)
#define SATA_HCLK_CTL_REG			REG(0x2C00)
#define SATA_CLK_SRC_NS_REG			REG(0x2C08)
#define SATA_RXOOB_CLK_CTL_REG			REG(0x2C0C)
#define SATA_PMALIVE_CLK_CTL_REG		REG(0x2C10)
#define SATA_PHY_REF_CLK_CTL_REG		REG(0x2C14)
#define SATA_RESET				REG(0x2C1C)
#define SATA_ACLK_CTL_REG			REG(0x2C20)
#define SATA_PHY_CFG_CLK_CTL_REG		REG(0x2C40)
#define USB_FSn_HCLK_CTL_REG(n)			REG(0x2960+(0x20*((n)-1)))
#define USB_FSn_RESET_REG(n)			REG(0x2974+(0x20*((n)-1)))
#define USB_FSn_SYSTEM_CLK_CTL_REG(n)		REG(0x296C+(0x20*((n)-1)))
#define USB_FSn_XCVR_FS_CLK_MD_REG(n)		REG(0x2964+(0x20*((n)-1)))
#define USB_FSn_XCVR_FS_CLK_NS_REG(n)		REG(0x2968+(0x20*((n)-1)))
#define USB_HS1_HCLK_CTL_REG			REG(0x2900)
#define USB_HS1_HCLK_FS_REG			REG(0x2904)
#define USB_HS1_RESET_REG			REG(0x2910)
#define USB_HS1_XCVR_FS_CLK_MD_REG		REG(0x2908)
#define USB_HS1_XCVR_FS_CLK_NS_REG		REG(0x290C)
#define USB_HS3_HCLK_CTL_REG			REG(0x3700)
#define USB_HS3_HCLK_FS_REG			REG(0x3704)
#define USB_HS3_RESET_REG			REG(0x3710)
#define USB_HS3_XCVR_FS_CLK_MD_REG		REG(0X3708)
#define USB_HS3_XCVR_FS_CLK_NS_REG		REG(0X370C)
#define USB_HS4_HCLK_CTL_REG			REG(0x3720)
#define USB_HS4_HCLK_FS_REG			REG(0x3724)
#define USB_HS4_RESET_REG			REG(0x3730)
#define USB_HS4_XCVR_FS_CLK_MD_REG		REG(0X3728)
#define USB_HS4_XCVR_FS_CLK_NS_REG		REG(0X372C)
#define USB_HSIC_HCLK_CTL_REG			REG(0x2920)
#define USB_HSIC_HSIC_CLK_CTL_REG		REG(0x2B44)
#define USB_HSIC_HSIC_CLK_SRC_CTL_REG		REG(0x2B40)
#define USB_HSIC_HSIO_CAL_CLK_CTL_REG		REG(0x2B48)
#define USB_HSIC_RESET_REG			REG(0x2934)
#define USB_HSIC_SYSTEM_CLK_CTL_REG		REG(0x292C)
#define USB_HSIC_XCVR_FS_CLK_MD_REG		REG(0x2924)
#define USB_HSIC_XCVR_FS_CLK_NS_REG		REG(0x2928)
#define USB_PHY0_RESET_REG			REG(0x2E20)
#define PCIE_ALT_REF_CLK_NS_REG			REG(0x3860)
#define PCIE_ACLK_CTL_REG			REG(0x22C0)
#define PCIE_HCLK_CTL_REG			REG(0x22CC)
#define PCIE_PCLK_CTL_REG			REG(0x22D0)
#define GPLL1_MODE_REG				REG(0x3160)
#define GPLL1_L_VAL_REG				REG(0x3164)
#define GPLL1_M_VAL_REG				REG(0x3168)
#define GPLL1_N_VAL_REG				REG(0x316C)
#define GPLL1_CONFIG_REG			REG(0x3174)
#define GPLL1_STATUS_REG			REG(0x3178)
#define PXO_SRC_CLK_CTL_REG			REG(0x2EA0)

/* Multimedia clock registers. */
#define AHB_EN_REG				REG_MM(0x0008)
#define AHB_EN2_REG				REG_MM(0x0038)
#define AHB_EN3_REG				REG_MM(0x0248)
#define AHB_NS_REG				REG_MM(0x0004)
#define AXI_NS_REG				REG_MM(0x0014)
#define CAMCLK0_NS_REG				REG_MM(0x0148)
#define CAMCLK0_CC_REG				REG_MM(0x0140)
#define CAMCLK0_MD_REG				REG_MM(0x0144)
#define CAMCLK1_NS_REG				REG_MM(0x015C)
#define CAMCLK1_CC_REG				REG_MM(0x0154)
#define CAMCLK1_MD_REG				REG_MM(0x0158)
#define CAMCLK2_NS_REG				REG_MM(0x0228)
#define CAMCLK2_CC_REG				REG_MM(0x0220)
#define CAMCLK2_MD_REG				REG_MM(0x0224)
#define CSI0_NS_REG				REG_MM(0x0048)
#define CSI0_CC_REG				REG_MM(0x0040)
#define CSI0_MD_REG				REG_MM(0x0044)
#define CSI1_NS_REG				REG_MM(0x0010)
#define CSI1_CC_REG				REG_MM(0x0024)
#define CSI1_MD_REG				REG_MM(0x0028)
#define CSI2_NS_REG				REG_MM(0x0234)
#define CSI2_CC_REG				REG_MM(0x022C)
#define CSI2_MD_REG				REG_MM(0x0230)
#define CSIPHYTIMER_CC_REG			REG_MM(0x0160)
#define CSIPHYTIMER_MD_REG			REG_MM(0x0164)
#define CSIPHYTIMER_NS_REG			REG_MM(0x0168)
#define DSI1_BYTE_NS_REG			REG_MM(0x00B0)
#define DSI1_BYTE_CC_REG			REG_MM(0x0090)
#define DSI2_BYTE_NS_REG			REG_MM(0x00BC)
#define DSI2_BYTE_CC_REG			REG_MM(0x00B4)
#define DSI1_ESC_NS_REG				REG_MM(0x011C)
#define DSI1_ESC_CC_REG				REG_MM(0x00CC)
#define DSI2_ESC_NS_REG				REG_MM(0x0150)
#define DSI2_ESC_CC_REG				REG_MM(0x013C)
#define DSI_PIXEL_CC_REG			REG_MM(0x0130)
#define DSI2_PIXEL_CC_REG			REG_MM(0x0094)
#define DSI2_PIXEL_CC2_REG			REG_MM(0x0264)
#define DBG_BUS_VEC_A_REG			REG_MM(0x01C8)
#define DBG_BUS_VEC_B_REG			REG_MM(0x01CC)
#define DBG_BUS_VEC_C_REG			REG_MM(0x01D0)
#define DBG_BUS_VEC_D_REG			REG_MM(0x01D4)
#define DBG_BUS_VEC_E_REG			REG_MM(0x01D8)
#define DBG_BUS_VEC_F_REG			REG_MM(0x01DC)
#define DBG_BUS_VEC_G_REG			REG_MM(0x01E0)
#define DBG_BUS_VEC_H_REG			REG_MM(0x01E4)
#define DBG_BUS_VEC_I_REG			REG_MM(0x01E8)
#define DBG_BUS_VEC_J_REG			REG_MM(0x0240)
#define DBG_CFG_REG_HS_REG			REG_MM(0x01B4)
#define DBG_CFG_REG_LS_REG			REG_MM(0x01B8)
#define GFX2D0_CC_REG				REG_MM(0x0060)
#define GFX2D0_MD0_REG				REG_MM(0x0064)
#define GFX2D0_MD1_REG				REG_MM(0x0068)
#define GFX2D0_NS_REG				REG_MM(0x0070)
#define GFX2D1_CC_REG				REG_MM(0x0074)
#define GFX2D1_MD0_REG				REG_MM(0x0078)
#define GFX2D1_MD1_REG				REG_MM(0x006C)
#define GFX2D1_NS_REG				REG_MM(0x007C)
#define GFX3D_CC_REG				REG_MM(0x0080)
#define GFX3D_MD0_REG				REG_MM(0x0084)
#define GFX3D_MD1_REG				REG_MM(0x0088)
#define GFX3D_NS_REG				REG_MM(0x008C)
#define IJPEG_CC_REG				REG_MM(0x0098)
#define IJPEG_MD_REG				REG_MM(0x009C)
#define IJPEG_NS_REG				REG_MM(0x00A0)
#define JPEGD_CC_REG				REG_MM(0x00A4)
#define JPEGD_NS_REG				REG_MM(0x00AC)
#define VCAP_CC_REG				REG_MM(0x0178)
#define VCAP_NS_REG				REG_MM(0x021C)
#define VCAP_MD0_REG				REG_MM(0x01EC)
#define VCAP_MD1_REG				REG_MM(0x0218)
#define MAXI_EN_REG				REG_MM(0x0018)
#define MAXI_EN2_REG				REG_MM(0x0020)
#define MAXI_EN3_REG				REG_MM(0x002C)
#define MAXI_EN4_REG				REG_MM(0x0114)
#define MAXI_EN5_REG				REG_MM(0x0244)
#define MDP_CC_REG				REG_MM(0x00C0)
#define MDP_LUT_CC_REG				REG_MM(0x016C)
#define MDP_MD0_REG				REG_MM(0x00C4)
#define MDP_MD1_REG				REG_MM(0x00C8)
#define MDP_NS_REG				REG_MM(0x00D0)
#define MISC_CC_REG				REG_MM(0x0058)
#define MISC_CC2_REG				REG_MM(0x005C)
#define MISC_CC3_REG				REG_MM(0x0238)
#define MM_PLL1_MODE_REG			REG_MM(0x031C)
#define MM_PLL1_L_VAL_REG			REG_MM(0x0320)
#define MM_PLL1_M_VAL_REG			REG_MM(0x0324)
#define MM_PLL1_N_VAL_REG			REG_MM(0x0328)
#define MM_PLL1_CONFIG_REG			REG_MM(0x032C)
#define MM_PLL1_TEST_CTL_REG			REG_MM(0x0330)
#define MM_PLL1_STATUS_REG			REG_MM(0x0334)
#define MM_PLL3_MODE_REG			REG_MM(0x0338)
#define MM_PLL3_L_VAL_REG			REG_MM(0x033C)
#define MM_PLL3_M_VAL_REG			REG_MM(0x0340)
#define MM_PLL3_N_VAL_REG			REG_MM(0x0344)
#define MM_PLL3_CONFIG_REG			REG_MM(0x0348)
#define MM_PLL3_TEST_CTL_REG			REG_MM(0x034C)
#define MM_PLL3_STATUS_REG			REG_MM(0x0350)
#define ROT_CC_REG				REG_MM(0x00E0)
#define ROT_NS_REG				REG_MM(0x00E8)
#define SAXI_EN_REG				REG_MM(0x0030)
#define SW_RESET_AHB_REG			REG_MM(0x020C)
#define SW_RESET_AHB2_REG			REG_MM(0x0200)
#define SW_RESET_ALL_REG			REG_MM(0x0204)
#define SW_RESET_AXI_REG			REG_MM(0x0208)
#define SW_RESET_CORE_REG			REG_MM(0x0210)
#define SW_RESET_CORE2_REG			REG_MM(0x0214)
#define TV_CC_REG				REG_MM(0x00EC)
#define TV_CC2_REG				REG_MM(0x0124)
#define TV_MD_REG				REG_MM(0x00F0)
#define TV_NS_REG				REG_MM(0x00F4)
#define VCODEC_CC_REG				REG_MM(0x00F8)
#define VCODEC_MD0_REG				REG_MM(0x00FC)
#define VCODEC_MD1_REG				REG_MM(0x0128)
#define VCODEC_NS_REG				REG_MM(0x0100)
#define VFE_CC_REG				REG_MM(0x0104)
#define VFE_MD_REG				REG_MM(0x0108)
#define VFE_NS_REG				REG_MM(0x010C)
#define VFE_CC2_REG				REG_MM(0x023C)
#define VPE_CC_REG				REG_MM(0x0110)
#define VPE_NS_REG				REG_MM(0x0118)

/* Low-power Audio clock registers. */
#define LCC_CLK_HS_DEBUG_CFG_REG		REG_LPA(0x00A4)
#define LCC_CLK_LS_DEBUG_CFG_REG		REG_LPA(0x00A8)
#define LCC_CODEC_I2S_MIC_MD_REG		REG_LPA(0x0064)
#define LCC_CODEC_I2S_MIC_NS_REG		REG_LPA(0x0060)
#define LCC_CODEC_I2S_MIC_STATUS_REG		REG_LPA(0x0068)
#define LCC_CODEC_I2S_SPKR_MD_REG		REG_LPA(0x0070)
#define LCC_CODEC_I2S_SPKR_NS_REG		REG_LPA(0x006C)
#define LCC_CODEC_I2S_SPKR_STATUS_REG		REG_LPA(0x0074)
#define LCC_MI2S_MD_REG				REG_LPA(0x004C)
#define LCC_MI2S_NS_REG				REG_LPA(0x0048)
#define LCC_MI2S_STATUS_REG			REG_LPA(0x0050)
#define LCC_PCM_MD_REG				REG_LPA(0x0058)
#define LCC_PCM_NS_REG				REG_LPA(0x0054)
#define LCC_PCM_STATUS_REG			REG_LPA(0x005C)
#define LCC_PLL0_MODE_REG			REG_LPA(0x0000)
#define LCC_PLL0_L_VAL_REG			REG_LPA(0x0004)
#define LCC_PLL0_M_VAL_REG			REG_LPA(0x0008)
#define LCC_PLL0_N_VAL_REG			REG_LPA(0x000C)
#define LCC_PLL0_CONFIG_REG			REG_LPA(0x0014)
#define LCC_PLL0_STATUS_REG			REG_LPA(0x0018)
#define LCC_SPARE_I2S_MIC_MD_REG		REG_LPA(0x007C)
#define LCC_SPARE_I2S_MIC_NS_REG		REG_LPA(0x0078)
#define LCC_SPARE_I2S_MIC_STATUS_REG		REG_LPA(0x0080)
#define LCC_SPARE_I2S_SPKR_MD_REG		REG_LPA(0x0088)
#define LCC_SPARE_I2S_SPKR_NS_REG		REG_LPA(0x0084)
#define LCC_SPARE_I2S_SPKR_STATUS_REG		REG_LPA(0x008C)
#define LCC_SLIMBUS_NS_REG			REG_LPA(0x00CC)
#define LCC_SLIMBUS_MD_REG			REG_LPA(0x00D0)
#define LCC_SLIMBUS_STATUS_REG			REG_LPA(0x00D4)
#define LCC_AHBEX_BRANCH_CTL_REG		REG_LPA(0x00E4)
#define LCC_PRI_PLL_CLK_CTL_REG			REG_LPA(0x00C4)

#define GCC_APCS_CLK_DIAG			REG_GCC(0x001C)

/* MUX source input identifiers. */
#define pxo_to_bb_mux		0
#define cxo_to_bb_mux		5
#define pll0_to_bb_mux		2
#define pll8_to_bb_mux		3
#define pll6_to_bb_mux		4
#define gnd_to_bb_mux		5
#define pll3_to_bb_mux		6
#define pxo_to_mm_mux		0
#define pll1_to_mm_mux		1
#define pll2_to_mm_mux		1	/* or MMCC_PLL1 */
#define pll8_to_mm_mux		2	/* or GCC_PERF */
#define pll0_to_mm_mux		3
#define pll15_to_mm_mux		3	/* or MM_PLL3 */
#define gnd_to_mm_mux		4
#define pll3_to_mm_mux		3	/* or MMCC_PLL2 */
#define hdmi_pll_to_mm_mux	3
#define cxo_to_xo_mux		0
#define pxo_to_xo_mux		1
#define gnd_to_xo_mux		3
#define pxo_to_lpa_mux		0
#define cxo_to_lpa_mux		1
#define pll4_to_lpa_mux		2
#define gnd_to_lpa_mux		6
#define pxo_to_pcie_mux		0
#define pll3_to_pcie_mux	1

/* Test Vector Macros */
#define TEST_TYPE_PER_LS	1
#define TEST_TYPE_PER_HS	2
#define TEST_TYPE_MM_LS		3
#define TEST_TYPE_MM_HS		4
#define TEST_TYPE_LPA		5
#define TEST_TYPE_CPUL2		6
#define TEST_TYPE_LPA_HS	7
#define TEST_TYPE_SHIFT		24
#define TEST_CLK_SEL_MASK	BM(23, 0)
#define TEST_VECTOR(s, t)	(((t) << TEST_TYPE_SHIFT) | BVAL(23, 0, (s)))
#define TEST_PER_LS(s)		TEST_VECTOR((s), TEST_TYPE_PER_LS)
#define TEST_PER_HS(s)		TEST_VECTOR((s), TEST_TYPE_PER_HS)
#define TEST_MM_LS(s)		TEST_VECTOR((s), TEST_TYPE_MM_LS)
#define TEST_MM_HS(s)		TEST_VECTOR((s), TEST_TYPE_MM_HS)
#define TEST_LPA(s)		TEST_VECTOR((s), TEST_TYPE_LPA)
#define TEST_LPA_HS(s)		TEST_VECTOR((s), TEST_TYPE_LPA_HS)
#define TEST_CPUL2(s)		TEST_VECTOR((s), TEST_TYPE_CPUL2)

#define MN_MODE_DUAL_EDGE 0x2

struct pll_rate {
	const uint32_t	l_val;
	const uint32_t	m_val;
	const uint32_t	n_val;
	const uint32_t	vco;
	const uint32_t	post_div;
	const uint32_t	i_bits;
};
#define PLL_RATE(l, m, n, v, d, i) { l, m, n, v, (d>>1), i }

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_HIGH,
	VDD_DIG_NUM
};

static int set_vdd_dig_8960(struct clk_vdd_class *vdd_class, int level)
{
	static const int vdd_uv[] = {
		[VDD_DIG_NONE]    =       0,
		[VDD_DIG_LOW]     =  945000,
		[VDD_DIG_NOMINAL] = 1050000,
		[VDD_DIG_HIGH]    = 1150000
	};
	return rpm_vreg_set_voltage(RPM_VREG_ID_PM8921_S3, RPM_VREG_VOTER3,
				    vdd_uv[level], 1150000, 1);
}

static DEFINE_VDD_CLASS(vdd_dig, set_vdd_dig_8960, VDD_DIG_NUM);

static int rpm_vreg_dig_8930 = RPM_VREG_ID_PM8038_VDD_DIG_CORNER;
static int set_vdd_dig_8930(struct clk_vdd_class *vdd_class, int level)
{
	static const int vdd_corner[] = {
		[VDD_DIG_NONE]    = RPM_VREG_CORNER_NONE,
		[VDD_DIG_LOW]     = RPM_VREG_CORNER_LOW,
		[VDD_DIG_NOMINAL] = RPM_VREG_CORNER_NOMINAL,
		[VDD_DIG_HIGH]    = RPM_VREG_CORNER_HIGH,
	};
	return rpm_vreg_set_voltage(rpm_vreg_dig_8930,
					RPM_VREG_VOTER3,
					vdd_corner[level],
					RPM_VREG_CORNER_HIGH, 1);
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

enum vdd_sr2_hdmi_pll_levels {
	VDD_SR2_HDMI_PLL_OFF,
	VDD_SR2_HDMI_PLL_ON,
	VDD_SR2_HDMI_PLL_NUM
};

static int set_vdd_sr2_hdmi_pll_8960(struct clk_vdd_class *vdd_class, int level)
{
	int rc = 0;

	if (level == VDD_SR2_HDMI_PLL_OFF) {
		rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8921_L23,
				RPM_VREG_VOTER3, 0, 0, 1);
		if (rc)
			return rc;
		rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8921_S8,
				RPM_VREG_VOTER3, 0, 0, 1);
		if (rc)
			rpm_vreg_set_voltage(RPM_VREG_ID_PM8921_L23,
				RPM_VREG_VOTER3, 1800000, 1800000, 1);
	} else {
		rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8921_S8,
				RPM_VREG_VOTER3, 2050000, 2100000, 1);
		if (rc)
			return rc;
		rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8921_L23,
				RPM_VREG_VOTER3, 1800000, 1800000, 1);
		if (rc)
			rpm_vreg_set_voltage(RPM_VREG_ID_PM8921_S8,
					RPM_VREG_VOTER3, 0, 0, 1);
	}

	return rc;
}

static DEFINE_VDD_CLASS(vdd_sr2_hdmi_pll, set_vdd_sr2_hdmi_pll_8960,
			VDD_SR2_HDMI_PLL_NUM);

static int sr2_lreg_uv[] = {
	[VDD_SR2_HDMI_PLL_OFF] = 0,
	[VDD_SR2_HDMI_PLL_ON] = 1800000,
};

static int set_vdd_sr2_hdmi_pll_8064(struct clk_vdd_class *vdd_class, int level)
{
	return rpm_vreg_set_voltage(RPM_VREG_ID_PM8921_LVS7, RPM_VREG_VOTER3,
				    sr2_lreg_uv[level], sr2_lreg_uv[level], 1);
}

static int set_vdd_sr2_hdmi_pll_8930_pm8917(struct clk_vdd_class *vdd_class,
	int level)
{
	int rc = 0;

	if (level == VDD_SR2_HDMI_PLL_OFF) {
		rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8917_L23,
				RPM_VREG_VOTER3, 0, 0, 1);
		if (rc)
			return rc;
		rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8917_S8,
				RPM_VREG_VOTER3, 0, 0, 1);
		if (rc)
			rpm_vreg_set_voltage(RPM_VREG_ID_PM8917_L23,
				RPM_VREG_VOTER3, 1800000, 1800000, 1);
	} else {
		rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8917_S8,
				RPM_VREG_VOTER3, 2050000, 2100000, 1);
		if (rc)
			return rc;
		rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8917_L23,
				RPM_VREG_VOTER3, 1800000, 1800000, 1);
		if (rc)
			rpm_vreg_set_voltage(RPM_VREG_ID_PM8917_S8,
					RPM_VREG_VOTER3, 0, 0, 1);
	}

	return rc;
}

static int set_vdd_sr2_hdmi_pll_8930(struct clk_vdd_class *vdd_class, int level)
{
	return rpm_vreg_set_voltage(RPM_VREG_ID_PM8038_L23, RPM_VREG_VOTER3,
				    sr2_lreg_uv[level], sr2_lreg_uv[level], 1);
}

/*
 * Clock Descriptions
 */

DEFINE_CLK_RPM_BRANCH(pxo_clk, pxo_a_clk, PXO, 27000000);
DEFINE_CLK_RPM_BRANCH(cxo_clk, cxo_a_clk, CXO, 19200000);

static struct pll_clk pll2_clk = {
	.mode_reg = MM_PLL1_MODE_REG,
	.c = {
		.parent = &pxo_clk.c,
		.dbg_name = "pll2_clk",
		.rate = 800000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(pll2_clk.c),
	},
};

static struct pll_clk pll3_clk = {
	.mode_reg = BB_MMCC_PLL2_MODE_REG,
	.c = {
		.parent = &pxo_clk.c,
		.dbg_name = "pll3_clk",
		.rate = 1200000000,
		.ops = &clk_ops_local_pll,
		.vdd_class = &vdd_sr2_hdmi_pll,
		.fmax = (unsigned long[VDD_SR2_HDMI_PLL_NUM]) {
			[VDD_SR2_HDMI_PLL_ON] = ULONG_MAX
		},
		.num_fmax = VDD_SR2_HDMI_PLL_NUM,
		CLK_INIT(pll3_clk.c),
	},
};

static struct pll_vote_clk pll4_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(4),
	.status_reg = LCC_PLL0_STATUS_REG,
	.status_mask = BIT(16),
	.c = {
		.parent = &pxo_clk.c,
		.dbg_name = "pll4_clk",
		.rate = 393216000,
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll4_clk.c),
	},
};

static struct pll_vote_clk pll8_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(8),
	.status_reg = BB_PLL8_STATUS_REG,
	.status_mask = BIT(16),
	.c = {
		.parent = &pxo_clk.c,
		.dbg_name = "pll8_clk",
		.rate = 384000000,
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll8_clk.c),
	},
};

static struct pll_vote_clk pll14_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(14),
	.status_reg = BB_PLL14_STATUS_REG,
	.status_mask = BIT(16),
	.c = {
		.parent = &pxo_clk.c,
		.dbg_name = "pll14_clk",
		.rate = 480000000,
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll14_clk.c),
	},
};

static struct pll_clk pll15_clk = {
	.mode_reg = MM_PLL3_MODE_REG,
	.c = {
		.parent = &pxo_clk.c,
		.dbg_name = "pll15_clk",
		.rate = 975000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(pll15_clk.c),
	},
};

/* AXI Interfaces */
static struct branch_clk gmem_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(24),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 6,
		.retain_reg = MAXI_EN2_REG,
		.retain_mask = BIT(21),
	},
	.c = {
		.dbg_name = "gmem_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gmem_axi_clk.c),
	},
};

static struct branch_clk ijpeg_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(21),
		.hwcg_reg = MAXI_EN_REG,
		.hwcg_mask = BIT(11),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(14),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 4,
	},
	.c = {
		.dbg_name = "ijpeg_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ijpeg_axi_clk.c),
	},
};

static struct branch_clk imem_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(22),
		.hwcg_reg = MAXI_EN_REG,
		.hwcg_mask = BIT(15),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(10),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 7,
		.retain_reg = MAXI_EN2_REG,
		.retain_mask = BIT(10),
	},
	.c = {
		.dbg_name = "imem_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(imem_axi_clk.c),
	},
};

static struct branch_clk jpegd_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(25),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 5,
	},
	.c = {
		.dbg_name = "jpegd_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(jpegd_axi_clk.c),
	},
};

static struct branch_clk vcodec_axi_b_clk = {
	.b = {
		.ctl_reg = MAXI_EN4_REG,
		.en_mask = BIT(23),
		.hwcg_reg = MAXI_EN4_REG,
		.hwcg_mask = BIT(22),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 25,
		.retain_reg = MAXI_EN4_REG,
		.retain_mask = BIT(21),
	},
	.c = {
		.dbg_name = "vcodec_axi_b_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcodec_axi_b_clk.c),
	},
};

static struct branch_clk vcodec_axi_a_clk = {
	.b = {
		.ctl_reg = MAXI_EN4_REG,
		.en_mask = BIT(25),
		.hwcg_reg = MAXI_EN4_REG,
		.hwcg_mask = BIT(24),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 26,
		.retain_reg = MAXI_EN4_REG,
		.retain_mask = BIT(10),
	},
	.c = {
		.dbg_name = "vcodec_axi_a_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcodec_axi_a_clk.c),
		.depends = &vcodec_axi_b_clk.c,
	},
};

static struct branch_clk vcodec_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(19),
		.hwcg_reg = MAXI_EN_REG,
		.hwcg_mask = BIT(13),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(4)|BIT(5)|BIT(7),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 3,
		.retain_reg = MAXI_EN2_REG,
		.retain_mask = BIT(28),
	},
	.c = {
		.dbg_name = "vcodec_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcodec_axi_clk.c),
		.depends = &vcodec_axi_a_clk.c,
	},
};

static struct branch_clk vfe_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(18),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(9),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 0,
	},
	.c = {
		.dbg_name = "vfe_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vfe_axi_clk.c),
	},
};

static struct branch_clk mdp_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(23),
		.hwcg_reg = MAXI_EN_REG,
		.hwcg_mask = BIT(16),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(13),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 8,
		.retain_reg = MAXI_EN_REG,
		.retain_mask = BIT(0),
	},
	.c = {
		.dbg_name = "mdp_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdp_axi_clk.c),
	},
};

static struct branch_clk rot_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN2_REG,
		.en_mask = BIT(24),
		.hwcg_reg = MAXI_EN2_REG,
		.hwcg_mask = BIT(25),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(6),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 2,
		.retain_reg = MAXI_EN3_REG,
		.retain_mask = BIT(10),
	},
	.c = {
		.dbg_name = "rot_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(rot_axi_clk.c),
	},
};

static struct branch_clk vpe_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN2_REG,
		.en_mask = BIT(26),
		.hwcg_reg = MAXI_EN2_REG,
		.hwcg_mask = BIT(27),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(15),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 1,
		.retain_reg = MAXI_EN3_REG,
		.retain_mask = BIT(21),

	},
	.c = {
		.dbg_name = "vpe_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpe_axi_clk.c),
	},
};

static struct branch_clk vcap_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN5_REG,
		.en_mask = BIT(12),
		.hwcg_reg = MAXI_EN5_REG,
		.hwcg_mask = BIT(11),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(16),
		.halt_reg = DBG_BUS_VEC_J_REG,
		.halt_bit = 20,
	},
	.c = {
		.dbg_name = "vcap_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcap_axi_clk.c),
	},
};

/* gfx3d_axi_clk is set as a dependency of gmem_axi_clk at runtime */
static struct branch_clk gfx3d_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN5_REG,
		.en_mask = BIT(25),
		.hwcg_reg = MAXI_EN5_REG,
		.hwcg_mask = BIT(24),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(17),
		.halt_reg = DBG_BUS_VEC_J_REG,
		.halt_bit = 30,
	},
	.c = {
		.dbg_name = "gfx3d_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gfx3d_axi_clk.c),
	},
};

static struct branch_clk gfx3d_axi_clk_8930 = {
	.b = {
		.ctl_reg = MAXI_EN5_REG,
		.en_mask = BIT(12),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(16),
		.halt_reg = DBG_BUS_VEC_J_REG,
		.halt_bit = 12,
	},
	.c = {
		.dbg_name = "gfx3d_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gfx3d_axi_clk_8930.c),
	},
};

/* AHB Interfaces */
static struct branch_clk amp_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(24),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(20),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 18,
	},
	.c = {
		.dbg_name = "amp_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(amp_p_clk.c),
	},
};

static struct branch_clk csi_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(7),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(17),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 16,
	},
	.c = {
		.dbg_name = "csi_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi_p_clk.c),
	},
};

static struct branch_clk dsi1_m_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(9),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(6),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 19,
	},
	.c = {
		.dbg_name = "dsi1_m_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dsi1_m_p_clk.c),
	},
};

static struct branch_clk dsi1_s_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(18),
		.hwcg_reg = AHB_EN2_REG,
		.hwcg_mask = BIT(20),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(5),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 21,
	},
	.c = {
		.dbg_name = "dsi1_s_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dsi1_s_p_clk.c),
	},
};

static struct branch_clk dsi2_m_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(17),
		.reset_reg = SW_RESET_AHB2_REG,
		.reset_mask = BIT(1),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 18,
	},
	.c = {
		.dbg_name = "dsi2_m_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dsi2_m_p_clk.c),
	},
};

static struct branch_clk dsi2_s_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(22),
		.hwcg_reg = AHB_EN2_REG,
		.hwcg_mask = BIT(15),
		.reset_reg = SW_RESET_AHB2_REG,
		.reset_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 20,
	},
	.c = {
		.dbg_name = "dsi2_s_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dsi2_s_p_clk.c),
	},
};

static struct branch_clk gfx2d0_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(19),
		.hwcg_reg = AHB_EN2_REG,
		.hwcg_mask = BIT(28),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(12),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 2,
	},
	.c = {
		.dbg_name = "gfx2d0_p_clk",
		.ops = &clk_ops_branch,
		.flags = CLKFLAG_SKIP_HANDOFF,
		CLK_INIT(gfx2d0_p_clk.c),
	},
};

static struct branch_clk gfx2d1_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(2),
		.hwcg_reg = AHB_EN2_REG,
		.hwcg_mask = BIT(29),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(11),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 3,
	},
	.c = {
		.dbg_name = "gfx2d1_p_clk",
		.ops = &clk_ops_branch,
		.flags = CLKFLAG_SKIP_HANDOFF,
		CLK_INIT(gfx2d1_p_clk.c),
	},
};

static struct branch_clk gfx3d_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(3),
		.hwcg_reg = AHB_EN2_REG,
		.hwcg_mask = BIT(27),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(10),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 4,
	},
	.c = {
		.dbg_name = "gfx3d_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gfx3d_p_clk.c),
	},
};

static struct branch_clk hdmi_m_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(14),
		.hwcg_reg = AHB_EN2_REG,
		.hwcg_mask = BIT(21),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(9),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 5,
	},
	.c = {
		.dbg_name = "hdmi_m_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(hdmi_m_p_clk.c),
	},
};

static struct branch_clk hdmi_s_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(4),
		.hwcg_reg = AHB_EN2_REG,
		.hwcg_mask = BIT(22),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(9),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 6,
	},
	.c = {
		.dbg_name = "hdmi_s_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(hdmi_s_p_clk.c),
	},
};

static struct branch_clk ijpeg_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(5),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(7),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 9,
	},
	.c = {
		.dbg_name = "ijpeg_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ijpeg_p_clk.c),
	},
};

static struct branch_clk imem_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(6),
		.hwcg_reg = AHB_EN2_REG,
		.hwcg_mask = BIT(12),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(8),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 10,
	},
	.c = {
		.dbg_name = "imem_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(imem_p_clk.c),
	},
};

static struct branch_clk jpegd_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(21),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(4),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 7,
	},
	.c = {
		.dbg_name = "jpegd_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(jpegd_p_clk.c),
	},
};

static struct branch_clk mdp_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(10),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(3),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 11,
	},
	.c = {
		.dbg_name = "mdp_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdp_p_clk.c),
	},
};

static struct branch_clk rot_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(12),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(2),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 13,
	},
	.c = {
		.dbg_name = "rot_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(rot_p_clk.c),
	},
};

static struct branch_clk smmu_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(15),
		.hwcg_reg = AHB_EN_REG,
		.hwcg_mask = BIT(26),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 22,
	},
	.c = {
		.dbg_name = "smmu_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(smmu_p_clk.c),
	},
};

static struct branch_clk tv_enc_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(25),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(15),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 23,
	},
	.c = {
		.dbg_name = "tv_enc_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(tv_enc_p_clk.c),
	},
};

static struct branch_clk vcodec_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(11),
		.hwcg_reg = AHB_EN2_REG,
		.hwcg_mask = BIT(26),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(1),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 12,
	},
	.c = {
		.dbg_name = "vcodec_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcodec_p_clk.c),
	},
};

static struct branch_clk vfe_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(13),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 14,
		.retain_reg = AHB_EN2_REG,
		.retain_mask = BIT(0),
	},
	.c = {
		.dbg_name = "vfe_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vfe_p_clk.c),
	},
};

static struct branch_clk vpe_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(16),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(14),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 15,
	},
	.c = {
		.dbg_name = "vpe_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpe_p_clk.c),
	},
};

static struct branch_clk vcap_p_clk = {
	.b = {
		.ctl_reg = AHB_EN3_REG,
		.en_mask = BIT(1),
		.reset_reg = SW_RESET_AHB2_REG,
		.reset_mask = BIT(2),
		.halt_reg = DBG_BUS_VEC_J_REG,
		.halt_bit = 23,
	},
	.c = {
		.dbg_name = "vcap_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcap_p_clk.c),
	},
};

/*
 * Peripheral Clocks
 */
#define CLK_GP(i, n, h_r, h_b) \
	struct rcg_clk i##_clk = { \
		.b = { \
			.ctl_reg = GPn_NS_REG(n), \
			.en_mask = BIT(9), \
			.halt_reg = h_r, \
			.halt_bit = h_b, \
		}, \
		.ns_reg = GPn_NS_REG(n), \
		.md_reg = GPn_MD_REG(n), \
		.root_en_mask = BIT(11), \
		.ns_mask = (BM(23, 16) | BM(6, 0)), \
		.mnd_en_mask = BIT(8), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_gp, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000), \
			CLK_INIT(i##_clk.c), \
		}, \
	}
#define F_GP(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(16, m, 0, n), \
		.ns_val = NS(23, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
	}
static struct clk_freq_tbl clk_tbl_gp[] = {
	F_GP(        0, gnd,  1, 0, 0),
	F_GP(  9600000, cxo,  2, 0, 0),
	F_GP( 13500000, pxo,  2, 0, 0),
	F_GP( 19200000, cxo,  1, 0, 0),
	F_GP( 27000000, pxo,  1, 0, 0),
	F_GP( 64000000, pll8, 2, 1, 3),
	F_GP( 76800000, pll8, 1, 1, 5),
	F_GP( 96000000, pll8, 4, 0, 0),
	F_GP(128000000, pll8, 3, 0, 0),
	F_GP(192000000, pll8, 2, 0, 0),
	F_END
};

static CLK_GP(gp0, 0, CLK_HALT_SFPB_MISC_STATE_REG, 7);
static CLK_GP(gp1, 1, CLK_HALT_SFPB_MISC_STATE_REG, 6);
static CLK_GP(gp2, 2, CLK_HALT_SFPB_MISC_STATE_REG, 5);

#define CLK_GSBI_UART(i, n, h_r, h_b) \
	struct rcg_clk i##_clk = { \
		.b = { \
			.ctl_reg = GSBIn_UART_APPS_NS_REG(n), \
			.en_mask = BIT(9), \
			.reset_reg = GSBIn_RESET_REG(n), \
			.reset_mask = BIT(0), \
			.halt_reg = h_r, \
			.halt_bit = h_b, \
		}, \
		.ns_reg = GSBIn_UART_APPS_NS_REG(n), \
		.md_reg = GSBIn_UART_APPS_MD_REG(n), \
		.root_en_mask = BIT(11), \
		.ns_mask = (BM(31, 16) | BM(6, 0)), \
		.mnd_en_mask = BIT(8), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_gsbi_uart, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP2(LOW, 32000000, NOMINAL, 64000000), \
			CLK_INIT(i##_clk.c), \
		}, \
	}
#define F_GSBI_UART(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD16(m, n), \
		.ns_val = NS(31, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
	}
static struct clk_freq_tbl clk_tbl_gsbi_uart[] = {
	F_GSBI_UART(       0, gnd,  1,  0,   0),
	F_GSBI_UART( 1843200, pll8, 2,  6, 625),
	F_GSBI_UART( 3686400, pll8, 2, 12, 625),
	F_GSBI_UART( 7372800, pll8, 2, 24, 625),
	F_GSBI_UART(14745600, pll8, 2, 48, 625),
	F_GSBI_UART(16000000, pll8, 4,  1,   6),
	F_GSBI_UART(24000000, pll8, 4,  1,   4),
	F_GSBI_UART(32000000, pll8, 4,  1,   3),
	F_GSBI_UART(40000000, pll8, 1,  5,  48),
	F_GSBI_UART(46400000, pll8, 1, 29, 240),
	F_GSBI_UART(48000000, pll8, 4,  1,   2),
	F_GSBI_UART(51200000, pll8, 1,  2,  15),
	F_GSBI_UART(56000000, pll8, 1,  7,  48),
	F_GSBI_UART(58982400, pll8, 1, 96, 625),
	F_GSBI_UART(64000000, pll8, 2,  1,   3),
	F_END
};

static CLK_GSBI_UART(gsbi1_uart,   1, CLK_HALT_CFPB_STATEA_REG, 10);
static CLK_GSBI_UART(gsbi2_uart,   2, CLK_HALT_CFPB_STATEA_REG,  6);
static CLK_GSBI_UART(gsbi3_uart,   3, CLK_HALT_CFPB_STATEA_REG,  2);
static CLK_GSBI_UART(gsbi4_uart,   4, CLK_HALT_CFPB_STATEB_REG, 26);
static CLK_GSBI_UART(gsbi5_uart,   5, CLK_HALT_CFPB_STATEB_REG, 22);
static CLK_GSBI_UART(gsbi6_uart,   6, CLK_HALT_CFPB_STATEB_REG, 18);
static CLK_GSBI_UART(gsbi7_uart,   7, CLK_HALT_CFPB_STATEB_REG, 14);
static CLK_GSBI_UART(gsbi8_uart,   8, CLK_HALT_CFPB_STATEB_REG, 10);
static CLK_GSBI_UART(gsbi9_uart,   9, CLK_HALT_CFPB_STATEB_REG,  6);
static CLK_GSBI_UART(gsbi10_uart, 10, CLK_HALT_CFPB_STATEB_REG,  2);
static CLK_GSBI_UART(gsbi11_uart, 11, CLK_HALT_CFPB_STATEC_REG, 17);
static CLK_GSBI_UART(gsbi12_uart, 12, CLK_HALT_CFPB_STATEC_REG, 13);

#define CLK_GSBI_QUP(i, n, h_r, h_b) \
	struct rcg_clk i##_clk = { \
		.b = { \
			.ctl_reg = GSBIn_QUP_APPS_NS_REG(n), \
			.en_mask = BIT(9), \
			.reset_reg = GSBIn_RESET_REG(n), \
			.reset_mask = BIT(0), \
			.halt_reg = h_r, \
			.halt_bit = h_b, \
		}, \
		.ns_reg = GSBIn_QUP_APPS_NS_REG(n), \
		.md_reg = GSBIn_QUP_APPS_MD_REG(n), \
		.root_en_mask = BIT(11), \
		.ns_mask = (BM(23, 16) | BM(6, 0)), \
		.mnd_en_mask = BIT(8), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_gsbi_qup, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP2(LOW, 24000000, NOMINAL, 52000000), \
			CLK_INIT(i##_clk.c), \
		}, \
	}
#define F_GSBI_QUP(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(16, m, 0, n), \
		.ns_val = NS(23, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
	}
static struct clk_freq_tbl clk_tbl_gsbi_qup[] = {
	F_GSBI_QUP(       0, gnd,  1, 0,  0),
	F_GSBI_QUP( 1100000, pxo,  1, 2, 49),
	F_GSBI_QUP( 5400000, pxo,  1, 1,  5),
	F_GSBI_QUP(10800000, pxo,  1, 2,  5),
	F_GSBI_QUP(15060000, pll8, 1, 2, 51),
	F_GSBI_QUP(24000000, pll8, 4, 1,  4),
	F_GSBI_QUP(25600000, pll8, 1, 1, 15),
	F_GSBI_QUP(27000000, pxo,  1, 0,  0),
	F_GSBI_QUP(48000000, pll8, 4, 1,  2),
	F_GSBI_QUP(51200000, pll8, 1, 2, 15),
	F_END
};

static CLK_GSBI_QUP(gsbi1_qup,   1, CLK_HALT_CFPB_STATEA_REG,  9);
static CLK_GSBI_QUP(gsbi2_qup,   2, CLK_HALT_CFPB_STATEA_REG,  4);
static CLK_GSBI_QUP(gsbi3_qup,   3, CLK_HALT_CFPB_STATEA_REG,  0);
static CLK_GSBI_QUP(gsbi4_qup,   4, CLK_HALT_CFPB_STATEB_REG, 24);
static CLK_GSBI_QUP(gsbi5_qup,   5, CLK_HALT_CFPB_STATEB_REG, 20);
static CLK_GSBI_QUP(gsbi6_qup,   6, CLK_HALT_CFPB_STATEB_REG, 16);
static CLK_GSBI_QUP(gsbi7_qup,   7, CLK_HALT_CFPB_STATEB_REG, 12);
static CLK_GSBI_QUP(gsbi8_qup,   8, CLK_HALT_CFPB_STATEB_REG,  8);
static CLK_GSBI_QUP(gsbi9_qup,   9, CLK_HALT_CFPB_STATEB_REG,  4);
static CLK_GSBI_QUP(gsbi10_qup, 10, CLK_HALT_CFPB_STATEB_REG,  0);
static CLK_GSBI_QUP(gsbi11_qup, 11, CLK_HALT_CFPB_STATEC_REG, 15);
static CLK_GSBI_QUP(gsbi12_qup, 12, CLK_HALT_CFPB_STATEC_REG, 11);

#define F_PDM(f, s, d) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_SRC_SEL(1, 0, s##_to_xo_mux), \
	}
static struct clk_freq_tbl clk_tbl_pdm[] = {
	F_PDM(       0, gnd, 1),
	F_PDM(27000000, pxo, 1),
	F_END
};

static struct rcg_clk pdm_clk = {
	.b = {
		.ctl_reg = PDM_CLK_NS_REG,
		.en_mask = BIT(9),
		.reset_reg = PDM_CLK_NS_REG,
		.reset_mask = BIT(12),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 3,
	},
	.ns_reg = PDM_CLK_NS_REG,
	.root_en_mask = BIT(11),
	.ns_mask = BM(1, 0),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_pdm,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "pdm_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 27000000),
		CLK_INIT(pdm_clk.c),
	},
};

static struct branch_clk pmem_clk = {
	.b = {
		.ctl_reg = PMEM_ACLK_CTL_REG,
		.en_mask = BIT(4),
		.hwcg_reg = PMEM_ACLK_CTL_REG,
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 20,
	},
	.c = {
		.dbg_name = "pmem_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pmem_clk.c),
	},
};

#define F_PRNG(f, s) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
	}
static struct clk_freq_tbl clk_tbl_prng_32[] = {
	F_PRNG(32000000, pll8),
	F_END
};

static struct clk_freq_tbl clk_tbl_prng_64[] = {
	F_PRNG(64000000, pll8),
	F_END
};

static struct rcg_clk prng_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(10),
		.halt_reg = CLK_HALT_SFPB_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 10,
	},
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_prng_32,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "prng_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 32000000, NOMINAL, 64000000),
		CLK_INIT(prng_clk.c),
	},
};

#define CLK_SDC(name, n, h_b, fmax_low, fmax_nom) \
	struct rcg_clk name = { \
		.b = { \
			.ctl_reg = SDCn_APPS_CLK_NS_REG(n), \
			.en_mask = BIT(9), \
			.reset_reg = SDCn_RESET_REG(n), \
			.reset_mask = BIT(0), \
			.halt_reg = CLK_HALT_DFAB_STATE_REG, \
			.halt_bit = h_b, \
		}, \
		.ns_reg = SDCn_APPS_CLK_NS_REG(n), \
		.md_reg = SDCn_APPS_CLK_MD_REG(n), \
		.root_en_mask = BIT(11), \
		.ns_mask = (BM(23, 16) | BM(6, 0)), \
		.mnd_en_mask = BIT(8), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_sdc, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #name, \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP2(LOW, fmax_low, NOMINAL, fmax_nom), \
			CLK_INIT(name.c), \
		}, \
	}
#define F_SDC(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(16, m, 0, n), \
		.ns_val = NS(23, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
	}
static struct clk_freq_tbl clk_tbl_sdc[] = {
	F_SDC(        0, gnd,   1, 0,   0),
	F_SDC(   144000, pxo,   3, 2, 125),
	F_SDC(   400000, pll8,  4, 1, 240),
	F_SDC( 16000000, pll8,  4, 1,   6),
	F_SDC( 17070000, pll8,  1, 2,  45),
	F_SDC( 20210000, pll8,  1, 1,  19),
	F_SDC( 24000000, pll8,  4, 1,   4),
	F_SDC( 48000000, pll8,  4, 1,   2),
	F_SDC( 64000000, pll8,  3, 1,   2),
	F_SDC( 96000000, pll8,  4, 0,   0),
	F_SDC(192000000, pll8,  2, 0,   0),
	F_END
};

static CLK_SDC(sdc1_clk, 1, 6,  52000000, 208000000);
static CLK_SDC(sdc2_clk, 2, 5,  52000000, 104000000);
static CLK_SDC(sdc3_clk, 3, 4, 104000000, 208000000);
static CLK_SDC(sdc4_clk, 4, 3,  33000000,  67000000);
static CLK_SDC(sdc5_clk, 5, 2,  33000000,  67000000);

static unsigned long fmax_sdc1_8064v2[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 100000000,
	[VDD_DIG_NOMINAL] = 200000000,
};

#define F_TSIF_REF(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD16(m, n), \
		.ns_val = NS(31, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
	}
static struct clk_freq_tbl clk_tbl_tsif_ref[] = {
	F_TSIF_REF(     0, gnd,  1, 0,   0),
	F_TSIF_REF(105000, pxo,  1, 1, 256),
	F_END
};

static struct rcg_clk tsif_ref_clk = {
	.b = {
		.ctl_reg = TSIF_REF_CLK_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 5,
	},
	.ns_reg = TSIF_REF_CLK_NS_REG,
	.md_reg = TSIF_REF_CLK_MD_REG,
	.root_en_mask = BIT(11),
	.ns_mask = (BM(31, 16) | BM(6, 0)),
	.mnd_en_mask = BIT(8),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_tsif_ref,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "tsif_ref_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 27000000, NOMINAL, 54000000),
		CLK_INIT(tsif_ref_clk.c),
	},
};

#define F_TSSC(f, s) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_SRC_SEL(1, 0, s##_to_xo_mux), \
	}
static struct clk_freq_tbl clk_tbl_tssc[] = {
	F_TSSC(       0, gnd),
	F_TSSC(27000000, pxo),
	F_END
};

static struct rcg_clk tssc_clk = {
	.b = {
		.ctl_reg = TSSC_CLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 4,
	},
	.ns_reg = TSSC_CLK_CTL_REG,
	.ns_mask = BM(1, 0),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_tssc,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "tssc_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 27000000),
		CLK_INIT(tssc_clk.c),
	},
};

#define CLK_USB_HS(name, n, h_b) \
	static struct rcg_clk name = { \
	.b = { \
		.ctl_reg = USB_HS##n##_XCVR_FS_CLK_NS_REG, \
		.en_mask = BIT(9), \
		.reset_reg = USB_HS##n##_RESET_REG, \
		.reset_mask = BIT(0), \
		.halt_reg = CLK_HALT_DFAB_STATE_REG, \
		.halt_bit = h_b, \
	}, \
	.ns_reg = USB_HS##n##_XCVR_FS_CLK_NS_REG, \
	.md_reg = USB_HS##n##_XCVR_FS_CLK_MD_REG, \
	.root_en_mask = BIT(11), \
	.ns_mask = (BM(23, 16) | BM(6, 0)), \
	.mnd_en_mask = BIT(8), \
	.set_rate = set_rate_mnd, \
	.freq_tbl = clk_tbl_usb, \
	.current_freq = &rcg_dummy_freq, \
	.c = { \
		.dbg_name = #name, \
		.ops = &clk_ops_rcg, \
		VDD_DIG_FMAX_MAP1(NOMINAL, 64000000), \
		CLK_INIT(name.c), \
	}, \
}

#define F_USB(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(16, m, 0, n), \
		.ns_val = NS(23, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
	}
static struct clk_freq_tbl clk_tbl_usb[] = {
	F_USB(       0, gnd,  1, 0,  0),
	F_USB(60000000, pll8, 1, 5, 32),
	F_END
};

CLK_USB_HS(usb_hs1_xcvr_clk, 1, 0);
CLK_USB_HS(usb_hs3_xcvr_clk, 3, 30);
CLK_USB_HS(usb_hs4_xcvr_clk, 4, 2);

static struct clk_freq_tbl clk_tbl_usb_hsic[] = {
	F_USB(       0, gnd,  1, 0,  0),
	F_USB(60000000, pll8, 1, 5, 32),
	F_END
};

static struct rcg_clk usb_hsic_xcvr_fs_clk = {
	.b = {
		.ctl_reg = USB_HSIC_XCVR_FS_CLK_NS_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 26,
	},
	.ns_reg = USB_HSIC_XCVR_FS_CLK_NS_REG,
	.md_reg = USB_HSIC_XCVR_FS_CLK_MD_REG,
	.root_en_mask = BIT(11),
	.ns_mask = (BM(23, 16) | BM(6, 0)),
	.mnd_en_mask = BIT(8),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_usb_hsic,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "usb_hsic_xcvr_fs_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(usb_hsic_xcvr_fs_clk.c),
	},
};

static struct branch_clk usb_hsic_system_clk = {
	.b = {
		.ctl_reg = USB_HSIC_SYSTEM_CLK_CTL_REG,
		.en_mask = BIT(4),
		.reset_reg = USB_HSIC_RESET_REG,
		.reset_mask = BIT(0),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 24,
	},
	.c = {
		.parent = &usb_hsic_xcvr_fs_clk.c,
		.dbg_name = "usb_hsic_system_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hsic_system_clk.c),
	},
};

#define F_USB_HSIC(f, s) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
	}
static struct clk_freq_tbl clk_tbl_usb2_hsic[] = {
	F_USB_HSIC(480000000, pll14),
	F_END
};

static struct rcg_clk usb_hsic_hsic_src_clk = {
	.b = {
		.ctl_reg = USB_HSIC_HSIC_CLK_SRC_CTL_REG,
		.halt_check = NOCHECK,
	},
	.root_en_mask = BIT(0),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_usb2_hsic,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "usb_hsic_hsic_src_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 480000000),
		CLK_INIT(usb_hsic_hsic_src_clk.c),
	},
};

static struct branch_clk usb_hsic_hsic_clk = {
	.b = {
		.ctl_reg = USB_HSIC_HSIC_CLK_CTL_REG,
		.en_mask = BIT(0),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 19,
	},
	.c = {
		.parent = &usb_hsic_hsic_src_clk.c,
		.dbg_name = "usb_hsic_hsic_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hsic_hsic_clk.c),
	},
};

#define F_USB_HSIO_CAL(f, s) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
	}
static struct clk_freq_tbl clk_tbl_usb_hsio_cal[] = {
	F_USB_HSIO_CAL(9000000, pxo),
	F_END
};

static struct rcg_clk usb_hsic_hsio_cal_clk = {
	.b = {
		.ctl_reg = USB_HSIC_HSIO_CAL_CLK_CTL_REG,
		.en_mask = BIT(0),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 23,
	},
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_usb_hsio_cal,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "usb_hsic_hsio_cal_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 10000000),
		CLK_INIT(usb_hsic_hsio_cal_clk.c),
	},
};

static struct branch_clk usb_phy0_clk = {
	.b = {
		.reset_reg = USB_PHY0_RESET_REG,
		.reset_mask = BIT(0),
	},
	.c = {
		.dbg_name = "usb_phy0_clk",
		.ops = &clk_ops_reset,
		CLK_INIT(usb_phy0_clk.c),
	},
};

#define CLK_USB_FS(i, n, fmax_nom) \
	struct rcg_clk i##_clk = { \
		.ns_reg = USB_FSn_XCVR_FS_CLK_NS_REG(n), \
		.b = { \
			.ctl_reg = USB_FSn_XCVR_FS_CLK_NS_REG(n), \
			.halt_check = NOCHECK, \
		}, \
		.md_reg = USB_FSn_XCVR_FS_CLK_MD_REG(n), \
		.root_en_mask = BIT(11), \
		.ns_mask = (BM(23, 16) | BM(6, 0)), \
		.mnd_en_mask = BIT(8), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_usb, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP1(NOMINAL, fmax_nom), \
			CLK_INIT(i##_clk.c), \
		}, \
	}

static CLK_USB_FS(usb_fs1_src, 1, 64000000);
static struct branch_clk usb_fs1_xcvr_clk = {
	.b = {
		.ctl_reg = USB_FSn_XCVR_FS_CLK_NS_REG(1),
		.en_mask = BIT(9),
		.reset_reg = USB_FSn_RESET_REG(1),
		.reset_mask = BIT(1),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 15,
	},
	.c = {
		.parent = &usb_fs1_src_clk.c,
		.dbg_name = "usb_fs1_xcvr_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_fs1_xcvr_clk.c),
	},
};

static struct branch_clk usb_fs1_sys_clk = {
	.b = {
		.ctl_reg = USB_FSn_SYSTEM_CLK_CTL_REG(1),
		.en_mask = BIT(4),
		.reset_reg = USB_FSn_RESET_REG(1),
		.reset_mask = BIT(0),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 16,
	},
	.c = {
		.parent = &usb_fs1_src_clk.c,
		.dbg_name = "usb_fs1_sys_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_fs1_sys_clk.c),
	},
};

static CLK_USB_FS(usb_fs2_src, 2, 60000000);
static struct branch_clk usb_fs2_xcvr_clk = {
	.b = {
		.ctl_reg = USB_FSn_XCVR_FS_CLK_NS_REG(2),
		.en_mask = BIT(9),
		.reset_reg = USB_FSn_RESET_REG(2),
		.reset_mask = BIT(1),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 12,
	},
	.c = {
		.parent = &usb_fs2_src_clk.c,
		.dbg_name = "usb_fs2_xcvr_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_fs2_xcvr_clk.c),
	},
};

static struct branch_clk usb_fs2_sys_clk = {
	.b = {
		.ctl_reg = USB_FSn_SYSTEM_CLK_CTL_REG(2),
		.en_mask = BIT(4),
		.reset_reg = USB_FSn_RESET_REG(2),
		.reset_mask = BIT(0),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 13,
	},
	.c = {
		.parent = &usb_fs2_src_clk.c,
		.dbg_name = "usb_fs2_sys_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_fs2_sys_clk.c),
	},
};

/* Fast Peripheral Bus Clocks */
static struct branch_clk ce1_core_clk = {
	.b = {
		.ctl_reg = CE1_CORE_CLK_CTL_REG,
		.en_mask = BIT(4),
		.hwcg_reg = CE1_CORE_CLK_CTL_REG,
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 27,
	},
	.c = {
		.dbg_name = "ce1_core_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ce1_core_clk.c),
	},
};

static struct branch_clk ce1_p_clk = {
	.b = {
		.ctl_reg = CE1_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 1,
	},
	.c = {
		.dbg_name = "ce1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ce1_p_clk.c),
	},
};

#define F_CE3(f, s, d) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_DIVSRC(6, 3, d, 2, 0, s##_to_bb_mux), \
	}

static struct clk_freq_tbl clk_tbl_ce3[] = {
	F_CE3(        0, gnd,   1),
	F_CE3( 48000000, pll8,  8),
	F_CE3(100000000, pll3, 12),
	F_CE3(120000000, pll3, 10),
	F_END
};

static struct rcg_clk ce3_src_clk = {
	.b = {
		.ctl_reg = CE3_CLK_SRC_NS_REG,
		.halt_check = NOCHECK,
	},
	.ns_reg = CE3_CLK_SRC_NS_REG,
	.root_en_mask = BIT(7),
	.ns_mask = BM(6, 0),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_ce3,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "ce3_src_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(ce3_src_clk.c),
	},
};

static unsigned long fmax_ce3_8064v2[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     =  57000000,
	[VDD_DIG_NOMINAL] = 120000000,
};

static struct branch_clk ce3_core_clk = {
	.b = {
		.ctl_reg = CE3_CORE_CLK_CTL_REG,
		.en_mask = BIT(4),
		.reset_reg = CE3_CORE_CLK_CTL_REG,
		.reset_mask = BIT(7),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_bit = 5,
	},
	.c = {
		.parent = &ce3_src_clk.c,
		.dbg_name = "ce3_core_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ce3_core_clk.c),
	}
};

static struct branch_clk ce3_p_clk = {
	.b = {
		.ctl_reg = CE3_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.reset_reg = CE3_HCLK_CTL_REG,
		.reset_mask = BIT(7),
		.halt_reg = CLK_HALT_AFAB_SFAB_STATEB_REG,
		.halt_bit = 16,
	},
	.c = {
		.parent = &ce3_src_clk.c,
		.dbg_name = "ce3_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ce3_p_clk.c),
	}
};

#define F_SATA(f, s, d) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_DIVSRC(6, 3, d, 2, 0, s##_to_bb_mux), \
	}

static struct clk_freq_tbl clk_tbl_sata[] = {
	F_SATA(        0,  gnd, 1),
	F_SATA( 48000000, pll8, 8),
	F_SATA(100000000, pll3, 12),
	F_END
};

static struct rcg_clk sata_src_clk = {
	.b = {
		.ctl_reg = SATA_CLK_SRC_NS_REG,
		.halt_check = NOCHECK,
	},
	.ns_reg = SATA_CLK_SRC_NS_REG,
	.root_en_mask = BIT(7),
	.ns_mask = BM(6, 0),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_sata,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "sata_src_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sata_src_clk.c),
	},
};

static struct branch_clk sata_rxoob_clk = {
	.b = {
		.ctl_reg = SATA_RXOOB_CLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_bit = 26,
	},
	.c = {
		.parent = &sata_src_clk.c,
		.dbg_name = "sata_rxoob_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sata_rxoob_clk.c),
	},
};

static struct branch_clk sata_pmalive_clk = {
	.b = {
		.ctl_reg = SATA_PMALIVE_CLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_bit = 25,
	},
	.c = {
		.parent = &sata_src_clk.c,
		.dbg_name = "sata_pmalive_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sata_pmalive_clk.c),
	},
};

static struct branch_clk sata_phy_ref_clk = {
	.b = {
		.ctl_reg = SATA_PHY_REF_CLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_bit = 24,
	},
	.c = {
		.parent = &pxo_clk.c,
		.dbg_name = "sata_phy_ref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sata_phy_ref_clk.c),
	},
};

static struct branch_clk sata_a_clk = {
	.b = {
		.ctl_reg = SATA_ACLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_AFAB_SFAB_STATEA_REG,
		.halt_bit = 12,
	},
	.c = {
		.dbg_name = "sata_a_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sata_a_clk.c),
	},
};

static struct branch_clk sata_p_clk = {
	.b = {
		.ctl_reg = SATA_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_bit = 27,
		.reset_reg = SATA_RESET,
		.reset_mask = BIT(0),
	},
	.c = {
		.dbg_name = "sata_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sata_p_clk.c),
	},
};

static struct branch_clk sfab_sata_s_p_clk = {
	.b = {
		.ctl_reg = SFAB_SATA_S_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_AFAB_SFAB_STATEB_REG,
		.halt_bit = 14,
	},
	.c = {
		.dbg_name = "sfab_sata_s_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sfab_sata_s_p_clk.c),
	},
};
static struct branch_clk pcie_p_clk = {
	.b = {
		.ctl_reg = PCIE_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 8,
	},
	.c = {
		.dbg_name = "pcie_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pcie_p_clk.c),
	},
};

static struct branch_clk pcie_phy_ref_clk = {
	.b = {
		.ctl_reg = PCIE_PCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_bit = 29,
	},
	.c = {
		.dbg_name = "pcie_phy_ref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pcie_phy_ref_clk.c),
	},
};

static struct branch_clk pcie_a_clk = {
	.b = {
		.ctl_reg = PCIE_ACLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_AFAB_SFAB_STATEA_REG,
		.halt_bit = 13,
	},
	.c = {
		.dbg_name = "pcie_a_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pcie_a_clk.c),
	},
};

static struct branch_clk dma_bam_p_clk = {
	.b = {
		.ctl_reg = DMA_BAM_HCLK_CTL,
		.en_mask = BIT(4),
		.hwcg_reg = DMA_BAM_HCLK_CTL,
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 12,
	},
	.c = {
		.dbg_name = "dma_bam_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dma_bam_p_clk.c),
	},
};

static struct branch_clk gsbi1_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(1),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(1),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 11,
	},
	.c = {
		.dbg_name = "gsbi1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi1_p_clk.c),
	},
};

static struct branch_clk gsbi2_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(2),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(2),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 7,
	},
	.c = {
		.dbg_name = "gsbi2_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi2_p_clk.c),
	},
};

static struct branch_clk gsbi3_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(3),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(3),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 3,
	},
	.c = {
		.dbg_name = "gsbi3_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi3_p_clk.c),
	},
};

static struct branch_clk gsbi4_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(4),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(4),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEB_REG,
		.halt_bit = 27,
	},
	.c = {
		.dbg_name = "gsbi4_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi4_p_clk.c),
	},
};

static struct branch_clk gsbi5_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(5),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(5),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEB_REG,
		.halt_bit = 23,
	},
	.c = {
		.dbg_name = "gsbi5_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi5_p_clk.c),
	},
};

static struct branch_clk gsbi6_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(6),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(6),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEB_REG,
		.halt_bit = 19,
	},
	.c = {
		.dbg_name = "gsbi6_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi6_p_clk.c),
	},
};

static struct branch_clk gsbi7_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(7),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(7),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEB_REG,
		.halt_bit = 15,
	},
	.c = {
		.dbg_name = "gsbi7_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi7_p_clk.c),
	},
};

static struct branch_clk gsbi8_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(8),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(8),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEB_REG,
		.halt_bit = 11,
	},
	.c = {
		.dbg_name = "gsbi8_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi8_p_clk.c),
	},
};

static struct branch_clk gsbi9_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(9),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(9),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEB_REG,
		.halt_bit = 7,
	},
	.c = {
		.dbg_name = "gsbi9_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi9_p_clk.c),
	},
};

static struct branch_clk gsbi10_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(10),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(10),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEB_REG,
		.halt_bit = 3,
	},
	.c = {
		.dbg_name = "gsbi10_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi10_p_clk.c),
	},
};

static struct branch_clk gsbi11_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(11),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(11),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 18,
	},
	.c = {
		.dbg_name = "gsbi11_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi11_p_clk.c),
	},
};

static struct branch_clk gsbi12_p_clk = {
	.b = {
		.ctl_reg = GSBIn_HCLK_CTL_REG(12),
		.en_mask = BIT(4),
		.hwcg_reg = GSBIn_HCLK_CTL_REG(12),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 14,
	},
	.c = {
		.dbg_name = "gsbi12_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi12_p_clk.c),
	},
};

static struct branch_clk sata_phy_cfg_clk = {
	.b = {
		.ctl_reg = SATA_PHY_CFG_CLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 12,
	},
	.c = {
		.dbg_name = "sata_phy_cfg_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sata_phy_cfg_clk.c),
	},
};

static struct branch_clk tsif_p_clk = {
	.b = {
		.ctl_reg = TSIF_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.hwcg_reg = TSIF_HCLK_CTL_REG,
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 7,
	},
	.c = {
		.dbg_name = "tsif_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(tsif_p_clk.c),
	},
};

static struct branch_clk usb_fs1_p_clk = {
	.b = {
		.ctl_reg = USB_FSn_HCLK_CTL_REG(1),
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 17,
	},
	.c = {
		.dbg_name = "usb_fs1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_fs1_p_clk.c),
	},
};

static struct branch_clk usb_fs2_p_clk = {
	.b = {
		.ctl_reg = USB_FSn_HCLK_CTL_REG(2),
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 14,
	},
	.c = {
		.dbg_name = "usb_fs2_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_fs2_p_clk.c),
	},
};

static struct branch_clk usb_hs1_p_clk = {
	.b = {
		.ctl_reg = USB_HS1_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.hwcg_reg = USB_HS1_HCLK_CTL_REG,
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 1,
	},
	.c = {
		.dbg_name = "usb_hs1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs1_p_clk.c),
	},
};

static struct branch_clk usb_hs3_p_clk = {
	.b = {
		.ctl_reg = USB_HS3_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 31,
	},
	.c = {
		.dbg_name = "usb_hs3_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs3_p_clk.c),
	},
};

static struct branch_clk usb_hs4_p_clk = {
	.b = {
		.ctl_reg = USB_HS4_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 7,
	},
	.c = {
		.dbg_name = "usb_hs4_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs4_p_clk.c),
	},
};

static struct branch_clk usb_hsic_p_clk = {
	.b = {
		.ctl_reg = USB_HSIC_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 28,
	},
	.c = {
		.dbg_name = "usb_hsic_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hsic_p_clk.c),
	},
};

static struct branch_clk sdc1_p_clk = {
	.b = {
		.ctl_reg = SDCn_HCLK_CTL_REG(1),
		.en_mask = BIT(4),
		.hwcg_reg = SDCn_HCLK_CTL_REG(1),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 11,
	},
	.c = {
		.dbg_name = "sdc1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdc1_p_clk.c),
	},
};

static struct branch_clk sdc2_p_clk = {
	.b = {
		.ctl_reg = SDCn_HCLK_CTL_REG(2),
		.en_mask = BIT(4),
		.hwcg_reg = SDCn_HCLK_CTL_REG(2),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 10,
	},
	.c = {
		.dbg_name = "sdc2_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdc2_p_clk.c),
	},
};

static struct branch_clk sdc3_p_clk = {
	.b = {
		.ctl_reg = SDCn_HCLK_CTL_REG(3),
		.en_mask = BIT(4),
		.hwcg_reg = SDCn_HCLK_CTL_REG(3),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 9,
	},
	.c = {
		.dbg_name = "sdc3_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdc3_p_clk.c),
	},
};

static struct branch_clk sdc4_p_clk = {
	.b = {
		.ctl_reg = SDCn_HCLK_CTL_REG(4),
		.en_mask = BIT(4),
		.hwcg_reg = SDCn_HCLK_CTL_REG(4),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 8,
	},
	.c = {
		.dbg_name = "sdc4_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdc4_p_clk.c),
	},
};

static struct branch_clk sdc5_p_clk = {
	.b = {
		.ctl_reg = SDCn_HCLK_CTL_REG(5),
		.en_mask = BIT(4),
		.hwcg_reg = SDCn_HCLK_CTL_REG(5),
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 7,
	},
	.c = {
		.dbg_name = "sdc5_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sdc5_p_clk.c),
	},
};

/* HW-Voteable Clocks */
static struct branch_clk adm0_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(2),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 14,
	},
	.c = {
		.dbg_name = "adm0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(adm0_clk.c),
	},
};

static struct branch_clk adm0_p_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(3),
		.hwcg_reg = ADM0_PBUS_CLK_CTL_REG,
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 13,
	},
	.c = {
		.dbg_name = "adm0_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(adm0_p_clk.c),
	},
};

static struct branch_clk pmic_arb0_p_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(8),
		.halt_reg = CLK_HALT_SFPB_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 22,
	},
	.c = {
		.dbg_name = "pmic_arb0_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pmic_arb0_p_clk.c),
	},
};

static struct branch_clk pmic_arb1_p_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(9),
		.halt_reg = CLK_HALT_SFPB_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 21,
	},
	.c = {
		.dbg_name = "pmic_arb1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pmic_arb1_p_clk.c),
	},
};

static struct branch_clk pmic_ssbi2_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(7),
		.halt_reg = CLK_HALT_SFPB_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 23,
	},
	.c = {
		.dbg_name = "pmic_ssbi2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pmic_ssbi2_clk.c),
	},
};

static struct branch_clk rpm_msg_ram_p_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(6),
		.hwcg_reg = RPM_MSG_RAM_HCLK_CTL_REG,
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_SFPB_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 12,
	},
	.c = {
		.dbg_name = "rpm_msg_ram_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(rpm_msg_ram_p_clk.c),
	},
};

/*
 * Multimedia Clocks
 */

#define CLK_CAM(name, n, hb) \
	struct rcg_clk name = { \
		.b = { \
			.ctl_reg = CAMCLK##n##_CC_REG, \
			.en_mask = BIT(0), \
			.halt_reg = DBG_BUS_VEC_I_REG, \
			.halt_bit = hb, \
		}, \
		.ns_reg = CAMCLK##n##_NS_REG, \
		.md_reg = CAMCLK##n##_MD_REG, \
		.root_en_mask = BIT(2), \
		.ns_mask = BM(31, 24) | BM(15, 14) | BM(2, 0), \
		.mnd_en_mask = BIT(5), \
		.ctl_mask = BM(7, 6), \
		.set_rate = set_rate_mnd_8, \
		.freq_tbl = clk_tbl_cam, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #name, \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP2(LOW, 64000000, NOMINAL, 128000000), \
			CLK_INIT(name.c), \
		}, \
	}
#define F_CAM(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(31, 24, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
	}
static struct clk_freq_tbl clk_tbl_cam[] = {
	F_CAM(        0, gnd,  1, 0,  0),
	F_CAM(  6000000, pll8, 4, 1, 16),
	F_CAM(  8000000, pll8, 4, 1, 12),
	F_CAM( 12000000, pll8, 4, 1,  8),
	F_CAM( 16000000, pll8, 4, 1,  6),
	F_CAM( 19200000, pll8, 4, 1,  5),
	F_CAM( 24000000, pll8, 4, 1,  4),
	F_CAM( 32000000, pll8, 4, 1,  3),
	F_CAM( 48000000, pll8, 4, 1,  2),
	F_CAM( 64000000, pll8, 3, 1,  2),
	F_CAM( 96000000, pll8, 4, 0,  0),
	F_CAM(128000000, pll8, 3, 0,  0),
	F_END
};

static CLK_CAM(cam0_clk, 0, 15);
static CLK_CAM(cam1_clk, 1, 16);
static CLK_CAM(cam2_clk, 2, 31);

#define F_CSI(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(31, 24, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
	}
static struct clk_freq_tbl clk_tbl_csi[] = {
	F_CSI(        0, gnd,  1, 0, 0),
	F_CSI( 27000000, pxo,  1, 0, 0),
	F_CSI( 85330000, pll8, 1, 2, 9),
	F_CSI(177780000, pll2, 1, 2, 9),
	F_END
};

static struct rcg_clk csi0_src_clk = {
	.ns_reg = CSI0_NS_REG,
	.b = {
		.ctl_reg = CSI0_CC_REG,
		.halt_check = NOCHECK,
	},
	.md_reg	= CSI0_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(31, 24) | BM(15, 14) | BM(2, 0),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_csi,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "csi0_src_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 86000000, NOMINAL, 178000000),
		CLK_INIT(csi0_src_clk.c),
	},
};

static struct branch_clk csi0_clk = {
	.b = {
		.ctl_reg = CSI0_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(8),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 13,
	},
	.c = {
		.parent = &csi0_src_clk.c,
		.dbg_name = "csi0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi0_clk.c),
	},
};

static struct branch_clk csi0_phy_clk = {
	.b = {
		.ctl_reg = CSI0_CC_REG,
		.en_mask = BIT(8),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(29),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 9,
	},
	.c = {
		.parent = &csi0_src_clk.c,
		.dbg_name = "csi0_phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi0_phy_clk.c),
	},
};

static struct rcg_clk csi1_src_clk = {
	.ns_reg = CSI1_NS_REG,
	.b = {
		.ctl_reg = CSI1_CC_REG,
		.halt_check = NOCHECK,
	},
	.md_reg	= CSI1_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(31, 24) | BM(15, 14) | BM(2, 0),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_csi,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "csi1_src_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 86000000, NOMINAL, 178000000),
		CLK_INIT(csi1_src_clk.c),
	},
};

static struct branch_clk csi1_clk = {
	.b = {
		.ctl_reg = CSI1_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(18),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 14,
	},
	.c = {
		.parent = &csi1_src_clk.c,
		.dbg_name = "csi1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi1_clk.c),
	},
};

static struct branch_clk csi1_phy_clk = {
	.b = {
		.ctl_reg = CSI1_CC_REG,
		.en_mask = BIT(8),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(28),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 10,
	},
	.c = {
		.parent = &csi1_src_clk.c,
		.dbg_name = "csi1_phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi1_phy_clk.c),
	},
};

static struct rcg_clk csi2_src_clk = {
	.ns_reg = CSI2_NS_REG,
	.b = {
		.ctl_reg = CSI2_CC_REG,
		.halt_check = NOCHECK,
	},
	.md_reg = CSI2_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(31, 24) | BM(15, 14) | BM(2, 0),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_csi,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "csi2_src_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 86000000, NOMINAL, 178000000),
		CLK_INIT(csi2_src_clk.c),
	},
};

static struct branch_clk csi2_clk = {
	.b = {
		.ctl_reg = CSI2_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE2_REG,
		.reset_mask = BIT(2),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 29,
	},
	.c = {
		.parent = &csi2_src_clk.c,
		.dbg_name = "csi2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi2_clk.c),
	},
};

static struct branch_clk csi2_phy_clk = {
	.b = {
		.ctl_reg = CSI2_CC_REG,
		.en_mask = BIT(8),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(31),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 29,
	},
	.c = {
		.parent = &csi2_src_clk.c,
		.dbg_name = "csi2_phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi2_phy_clk.c),
	},
};

static struct clk *pix_rdi_mux_map[] = {
	[0] = &csi0_clk.c,
	[1] = &csi1_clk.c,
	[2] = &csi2_clk.c,
	NULL,
};

struct pix_rdi_clk {
	bool prepared;
	bool enabled;
	unsigned long cur_rate;

	void __iomem *const s_reg;
	u32 s_mask;

	void __iomem *const s2_reg;
	u32 s2_mask;

	struct branch b;
	struct clk c;
};

static inline struct pix_rdi_clk *to_pix_rdi_clk(struct clk *c)
{
	return container_of(c, struct pix_rdi_clk, c);
}

static int pix_rdi_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret, i;
	u32 reg;
	unsigned long flags;
	struct pix_rdi_clk *rdi = to_pix_rdi_clk(c);
	struct clk **mux_map = pix_rdi_mux_map;
	unsigned long old_rate = rdi->cur_rate;

	/*
	 * These clocks select three inputs via two muxes. One mux selects
	 * between csi0 and csi1 and the second mux selects between that mux's
	 * output and csi2. The source and destination selections for each
	 * mux must be clocking for the switch to succeed so just turn on
	 * all three sources because it's easier than figuring out what source
	 * needs to be on at what time.
	 */
	for (i = 0; mux_map[i]; i++) {
		ret = clk_prepare_enable(mux_map[i]);
		if (ret)
			goto err;
	}
	if (rate >= i) {
		ret = -EINVAL;
		goto err;
	}
	/* Keep the new source on when switching inputs of an enabled clock */
	if (rdi->prepared) {
		ret = clk_prepare(mux_map[rate]);
		if (ret)
			goto err;
	}
	spin_lock_irqsave(&c->lock, flags);
	if (rdi->enabled) {
		ret = clk_enable(mux_map[rate]);
		if (ret) {
			spin_unlock_irqrestore(&c->lock, flags);
			clk_unprepare(mux_map[rate]);
			goto err;
		}
	}
	spin_lock(&local_clock_reg_lock);
	reg = readl_relaxed(rdi->s2_reg);
	reg &= ~rdi->s2_mask;
	reg |= rate == 2 ? rdi->s2_mask : 0;
	writel_relaxed(reg, rdi->s2_reg);
	/*
	 * Wait at least 6 cycles of slowest clock
	 * for the glitch-free MUX to fully switch sources.
	 */
	mb();
	udelay(1);
	reg = readl_relaxed(rdi->s_reg);
	reg &= ~rdi->s_mask;
	reg |= rate == 1 ? rdi->s_mask : 0;
	writel_relaxed(reg, rdi->s_reg);
	/*
	 * Wait at least 6 cycles of slowest clock
	 * for the glitch-free MUX to fully switch sources.
	 */
	mb();
	udelay(1);
	rdi->cur_rate = rate;
	c->parent = mux_map[rate];
	spin_unlock(&local_clock_reg_lock);

	if (rdi->enabled)
		clk_disable(mux_map[old_rate]);
	spin_unlock_irqrestore(&c->lock, flags);
	if (rdi->prepared)
		clk_unprepare(mux_map[old_rate]);
err:
	for (i--; i >= 0; i--)
		clk_disable_unprepare(mux_map[i]);

	return 0;
}

static unsigned long pix_rdi_clk_get_rate(struct clk *c)
{
	return to_pix_rdi_clk(c)->cur_rate;
}

static int pix_rdi_clk_prepare(struct clk *c)
{
	struct pix_rdi_clk *rdi = to_pix_rdi_clk(c);
	rdi->prepared = true;
	return 0;
}

static int pix_rdi_clk_enable(struct clk *c)
{
	unsigned long flags;
	struct pix_rdi_clk *rdi = to_pix_rdi_clk(c);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	__branch_enable_reg(&rdi->b, rdi->c.dbg_name);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
	rdi->enabled = true;

	return 0;
}

static void pix_rdi_clk_disable(struct clk *c)
{
	unsigned long flags;
	struct pix_rdi_clk *rdi = to_pix_rdi_clk(c);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	__branch_disable_reg(&rdi->b, rdi->c.dbg_name);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
	rdi->enabled = false;
}

static void pix_rdi_clk_unprepare(struct clk *c)
{
	struct pix_rdi_clk *rdi = to_pix_rdi_clk(c);
	rdi->prepared = false;
}

static int pix_rdi_clk_reset(struct clk *c, enum clk_reset_action action)
{
	return branch_reset(&to_pix_rdi_clk(c)->b, action);
}

static long pix_rdi_clk_list_rate(struct clk *c, unsigned n)
{
	if (pix_rdi_mux_map[n])
		return n;
	return -ENXIO;
}

static struct clk *pix_rdi_clk_get_parent(struct clk *c)
{
	u32 reg;
	struct pix_rdi_clk *rdi = to_pix_rdi_clk(c);

	reg = readl_relaxed(rdi->s_reg);
	rdi->cur_rate = reg & rdi->s_mask ? 1 : 0;
	reg = readl_relaxed(rdi->s2_reg);
	rdi->cur_rate = reg & rdi->s2_mask ? 2 : rdi->cur_rate;

	return pix_rdi_mux_map[rdi->cur_rate];
}

static enum handoff pix_rdi_clk_handoff(struct clk *c)
{
	struct pix_rdi_clk *rdi = to_pix_rdi_clk(c);
	enum handoff ret;

	ret = branch_handoff(&rdi->b, &rdi->c);
	if (ret == HANDOFF_DISABLED_CLK)
		return ret;

	rdi->prepared = true;
	rdi->enabled = true;
	return HANDOFF_ENABLED_CLK;
}

static struct clk_ops clk_ops_pix_rdi_8960 = {
	.prepare = pix_rdi_clk_prepare,
	.enable = pix_rdi_clk_enable,
	.disable = pix_rdi_clk_disable,
	.unprepare = pix_rdi_clk_unprepare,
	.handoff = pix_rdi_clk_handoff,
	.set_rate = pix_rdi_clk_set_rate,
	.get_rate = pix_rdi_clk_get_rate,
	.list_rate = pix_rdi_clk_list_rate,
	.reset = pix_rdi_clk_reset,
	.get_parent = pix_rdi_clk_get_parent,
};

static struct pix_rdi_clk csi_pix_clk = {
	.b = {
		.ctl_reg = MISC_CC_REG,
		.en_mask = BIT(26),
		.halt_check = DELAY,
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(26),
	},
	.s_reg = MISC_CC_REG,
	.s_mask = BIT(25),
	.s2_reg = MISC_CC3_REG,
	.s2_mask = BIT(13),
	.c = {
		.dbg_name = "csi_pix_clk",
		.ops = &clk_ops_pix_rdi_8960,
		CLK_INIT(csi_pix_clk.c),
	},
};

static struct pix_rdi_clk csi_pix1_clk = {
	.b = {
		.ctl_reg = MISC_CC3_REG,
		.en_mask = BIT(10),
		.halt_check = DELAY,
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(30),
	},
	.s_reg = MISC_CC3_REG,
	.s_mask = BIT(8),
	.s2_reg = MISC_CC3_REG,
	.s2_mask = BIT(9),
	.c = {
		.dbg_name = "csi_pix1_clk",
		.ops = &clk_ops_pix_rdi_8960,
		CLK_INIT(csi_pix1_clk.c),
	},
};

static struct pix_rdi_clk csi_rdi_clk = {
	.b = {
		.ctl_reg = MISC_CC_REG,
		.en_mask = BIT(13),
		.halt_check = DELAY,
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(27),
	},
	.s_reg = MISC_CC_REG,
	.s_mask = BIT(12),
	.s2_reg = MISC_CC3_REG,
	.s2_mask = BIT(12),
	.c = {
		.dbg_name = "csi_rdi_clk",
		.ops = &clk_ops_pix_rdi_8960,
		CLK_INIT(csi_rdi_clk.c),
	},
};

static struct pix_rdi_clk csi_rdi1_clk = {
	.b = {
		.ctl_reg = MISC_CC3_REG,
		.en_mask = BIT(2),
		.halt_check = DELAY,
		.reset_reg = SW_RESET_CORE2_REG,
		.reset_mask = BIT(1),
	},
	.s_reg = MISC_CC3_REG,
	.s_mask = BIT(0),
	.s2_reg = MISC_CC3_REG,
	.s2_mask = BIT(1),
	.c = {
		.dbg_name = "csi_rdi1_clk",
		.ops = &clk_ops_pix_rdi_8960,
		CLK_INIT(csi_rdi1_clk.c),
	},
};

static struct pix_rdi_clk csi_rdi2_clk = {
	.b = {
		.ctl_reg = MISC_CC3_REG,
		.en_mask = BIT(6),
		.halt_check = DELAY,
		.reset_reg = SW_RESET_CORE2_REG,
		.reset_mask = BIT(0),
	},
	.s_reg = MISC_CC3_REG,
	.s_mask = BIT(4),
	.s2_reg = MISC_CC3_REG,
	.s2_mask = BIT(5),
	.c = {
		.dbg_name = "csi_rdi2_clk",
		.ops = &clk_ops_pix_rdi_8960,
		CLK_INIT(csi_rdi2_clk.c),
	},
};

#define F_CSI_PHYTIMER(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(31, 24, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
	}
static struct clk_freq_tbl clk_tbl_csi_phytimer[] = {
	F_CSI_PHYTIMER(        0, gnd,  1, 0, 0),
	F_CSI_PHYTIMER( 85330000, pll8, 1, 2, 9),
	F_CSI_PHYTIMER(177780000, pll2, 1, 2, 9),
	F_END
};

static struct rcg_clk csiphy_timer_src_clk = {
	.ns_reg = CSIPHYTIMER_NS_REG,
	.b = {
		.ctl_reg = CSIPHYTIMER_CC_REG,
		.halt_check = NOCHECK,
	},
	.md_reg = CSIPHYTIMER_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(31, 24) | BM(15, 14) | BM(2, 0)),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd_8,
	.freq_tbl = clk_tbl_csi_phytimer,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "csiphy_timer_src_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 86000000, NOMINAL, 178000000),
		CLK_INIT(csiphy_timer_src_clk.c),
	},
};

static struct branch_clk csi0phy_timer_clk = {
	.b = {
		.ctl_reg = CSIPHYTIMER_CC_REG,
		.en_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 17,
	},
	.c = {
		.parent = &csiphy_timer_src_clk.c,
		.dbg_name = "csi0phy_timer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi0phy_timer_clk.c),
	},
};

static struct branch_clk csi1phy_timer_clk = {
	.b = {
		.ctl_reg = CSIPHYTIMER_CC_REG,
		.en_mask = BIT(9),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 18,
	},
	.c = {
		.parent = &csiphy_timer_src_clk.c,
		.dbg_name = "csi1phy_timer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi1phy_timer_clk.c),
	},
};

static struct branch_clk csi2phy_timer_clk = {
	.b = {
		.ctl_reg = CSIPHYTIMER_CC_REG,
		.en_mask = BIT(11),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 30,
	},
	.c = {
		.parent = &csiphy_timer_src_clk.c,
		.dbg_name = "csi2phy_timer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi2phy_timer_clk.c),
	},
};

#define F_DSI(d) \
	{ \
		.freq_hz = d, \
		.ns_val = BVAL(15, 12, (d-1)), \
	}
/*
 * The DSI_BYTE/ESC clock is sourced from the DSI PHY PLL, which may change rate
 * without this clock driver knowing.  So, overload the clk_set_rate() to set
 * the divider (1 to 16) of the clock with respect to the PLL rate.
 */
static struct clk_freq_tbl clk_tbl_dsi_byte[] = {
	F_DSI(1),  F_DSI(2),  F_DSI(3),  F_DSI(4),
	F_DSI(5),  F_DSI(6),  F_DSI(7),  F_DSI(8),
	F_DSI(9),  F_DSI(10), F_DSI(11), F_DSI(12),
	F_DSI(13), F_DSI(14), F_DSI(15), F_DSI(16),
	F_END
};

static struct branch_clk dsi1_reset_clk = {
	.b = {
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(7),
		.halt_check = NOCHECK,
	},
	.c = {
		.dbg_name = "dsi1_reset_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dsi1_reset_clk.c),
	},
};

static struct branch_clk dsi2_reset_clk = {
	.b = {
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(25),
		.halt_check = NOCHECK,
	},
	.c = {
		.dbg_name = "dsi2_reset_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dsi2_reset_clk.c),
	},
};

static struct rcg_clk dsi1_byte_clk = {
	.b = {
		.ctl_reg = DSI1_BYTE_CC_REG,
		.en_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 21,
		.retain_reg = DSI1_BYTE_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = DSI1_BYTE_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(15, 12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_dsi_byte,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "dsi1_byte_clk",
		.ops = &clk_ops_rcg,
		CLK_INIT(dsi1_byte_clk.c),
	},
};

static struct rcg_clk dsi2_byte_clk = {
	.b = {
		.ctl_reg = DSI2_BYTE_CC_REG,
		.en_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 20,
		.retain_reg = DSI2_BYTE_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = DSI2_BYTE_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(15, 12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_dsi_byte,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "dsi2_byte_clk",
		.ops = &clk_ops_rcg,
		CLK_INIT(dsi2_byte_clk.c),
	},
};

static struct rcg_clk dsi1_esc_clk = {
	.b = {
		.ctl_reg = DSI1_ESC_CC_REG,
		.en_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 1,
	},
	.ns_reg = DSI1_ESC_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(15, 12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_dsi_byte,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "dsi1_esc_clk",
		.ops = &clk_ops_rcg,
		CLK_INIT(dsi1_esc_clk.c),
	},
};

static struct rcg_clk dsi2_esc_clk = {
	.b = {
		.ctl_reg = DSI2_ESC_CC_REG,
		.en_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 3,
	},
	.ns_reg = DSI2_ESC_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(15, 12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_dsi_byte,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "dsi2_esc_clk",
		.ops = &clk_ops_rcg,
		CLK_INIT(dsi2_esc_clk.c),
	},
};

#define F_GFX2D(f, s, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD4(4, m, 0, n), \
		.ns_val = NS_MND_BANKED4(20, 16, n, m, 3, 0, s##_to_mm_mux), \
		.ctl_val = CC_BANKED(9, 6, n), \
	}
static struct clk_freq_tbl clk_tbl_gfx2d[] = {
	F_GFX2D(        0, gnd,  0,  0),
	F_GFX2D( 27000000, pxo,  0,  0),
	F_GFX2D( 48000000, pll8, 1,  8),
	F_GFX2D( 54857000, pll8, 1,  7),
	F_GFX2D( 64000000, pll8, 1,  6),
	F_GFX2D( 76800000, pll8, 1,  5),
	F_GFX2D( 96000000, pll8, 1,  4),
	F_GFX2D(128000000, pll8, 1,  3),
	F_GFX2D(145455000, pll2, 2, 11),
	F_GFX2D(160000000, pll2, 1,  5),
	F_GFX2D(177778000, pll2, 2,  9),
	F_GFX2D(200000000, pll2, 1,  4),
	F_GFX2D(228571000, pll2, 2,  7),
	F_END
};

static struct bank_masks bmnd_info_gfx2d0 = {
	.bank_sel_mask =			BIT(11),
	.bank0_mask = {
			.md_reg =		GFX2D0_MD0_REG,
			.ns_mask =		BM(23, 20) | BM(5, 3),
			.rst_mask =		BIT(25),
			.mnd_en_mask =		BIT(8),
			.mode_mask =		BM(10, 9),
	},
	.bank1_mask = {
			.md_reg =		GFX2D0_MD1_REG,
			.ns_mask =		BM(19, 16) | BM(2, 0),
			.rst_mask =		BIT(24),
			.mnd_en_mask =		BIT(5),
			.mode_mask =		BM(7, 6),
	},
};

static struct rcg_clk gfx2d0_clk = {
	.b = {
		.ctl_reg = GFX2D0_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(14),
		.halt_reg = DBG_BUS_VEC_A_REG,
		.halt_bit = 9,
		.retain_reg = GFX2D0_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = GFX2D0_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.freq_tbl = clk_tbl_gfx2d,
	.bank_info = &bmnd_info_gfx2d0,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "gfx2d0_clk",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_SKIP_HANDOFF,
		VDD_DIG_FMAX_MAP3(LOW,  100000000, NOMINAL, 200000000,
				  HIGH, 228571000),
		CLK_INIT(gfx2d0_clk.c),
	},
};

static struct bank_masks bmnd_info_gfx2d1 = {
	.bank_sel_mask =		BIT(11),
	.bank0_mask = {
			.md_reg =		GFX2D1_MD0_REG,
			.ns_mask =		BM(23, 20) | BM(5, 3),
			.rst_mask =		BIT(25),
			.mnd_en_mask =		BIT(8),
			.mode_mask =		BM(10, 9),
	},
	.bank1_mask = {
			.md_reg =		GFX2D1_MD1_REG,
			.ns_mask =		BM(19, 16) | BM(2, 0),
			.rst_mask =		BIT(24),
			.mnd_en_mask =		BIT(5),
			.mode_mask =		BM(7, 6),
	},
};

static struct rcg_clk gfx2d1_clk = {
	.b = {
		.ctl_reg = GFX2D1_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(13),
		.halt_reg = DBG_BUS_VEC_A_REG,
		.halt_bit = 14,
		.retain_reg = GFX2D1_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = GFX2D1_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.freq_tbl = clk_tbl_gfx2d,
	.bank_info = &bmnd_info_gfx2d1,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "gfx2d1_clk",
		.ops = &clk_ops_rcg,
		.flags = CLKFLAG_SKIP_HANDOFF,
		VDD_DIG_FMAX_MAP3(LOW,  100000000, NOMINAL, 200000000,
				  HIGH, 228571000),
		CLK_INIT(gfx2d1_clk.c),
	},
};

#define F_GFX3D(f, s, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD4(4, m, 0, n), \
		.ns_val = NS_MND_BANKED4(18, 14, n, m, 3, 0, s##_to_mm_mux), \
		.ctl_val = CC_BANKED(9, 6, n), \
	}

/*Shared by 8064, and 8930*/
static struct clk_freq_tbl clk_tbl_gfx3d[] = {
	F_GFX3D(        0, gnd,   0,  0),
	F_GFX3D(  1800000, pxo,   1, 15),
	F_GFX3D( 27000000, pxo,   0,  0),
	F_GFX3D( 48000000, pll8,  1,  8),
	F_GFX3D( 54857000, pll8,  1,  7),
	F_GFX3D( 64000000, pll8,  1,  6),
	F_GFX3D( 76800000, pll8,  1,  5),
	F_GFX3D( 96000000, pll8,  1,  4),
	F_GFX3D(128000000, pll8,  1,  3),
	F_GFX3D(145455000, pll2,  2, 11),
	F_GFX3D(160000000, pll2,  1,  5),
	F_GFX3D(177778000, pll2,  2,  9),
	F_GFX3D(192000000, pll8,  1,  2),
	F_GFX3D(200000000, pll2,  1,  4),
	F_GFX3D(228571000, pll2,  2,  7),
	F_GFX3D(266667000, pll2,  1,  3),
	F_GFX3D(320000000, pll2,  2,  5),
	F_GFX3D(400000000, pll2,  1,  2),
	F_GFX3D(450000000, pll15, 1,  2),
	F_END
};

static struct clk_freq_tbl clk_tbl_gfx3d_8960ab[] = {
	F_GFX3D(        0, gnd,   0,  0),
	F_GFX3D( 27000000, pxo,   0,  0),
	F_GFX3D( 48000000, pll8,  1,  8),
	F_GFX3D( 54857000, pll8,  1,  7),
	F_GFX3D( 64000000, pll8,  1,  6),
	F_GFX3D( 76800000, pll8,  1,  5),
	F_GFX3D( 96000000, pll8,  1,  4),
	F_GFX3D(128000000, pll8,  1,  3),
	F_GFX3D(145455000, pll2,  2, 11),
	F_GFX3D(160000000, pll2,  1,  5),
	F_GFX3D(177778000, pll2,  2,  9),
	F_GFX3D(192000000, pll8,  1,  2),
	F_GFX3D(200000000, pll2,  1,  4),
	F_GFX3D(228571000, pll2,  2,  7),
	F_GFX3D(266667000, pll2,  1,  3),
	F_GFX3D(320000000, pll2,  2,  5),
	F_GFX3D(400000000, pll2,  1,  2),
	F_GFX3D(440000000, pll3,  1,  2),
	F_END
};

static struct clk_freq_tbl clk_tbl_gfx3d_8960[] = {
	F_GFX3D(        0, gnd,  0,  0),
	F_GFX3D( 27000000, pxo,  0,  0),
	F_GFX3D( 48000000, pll8, 1,  8),
	F_GFX3D( 54857000, pll8, 1,  7),
	F_GFX3D( 64000000, pll8, 1,  6),
	F_GFX3D( 76800000, pll8, 1,  5),
	F_GFX3D( 96000000, pll8, 1,  4),
	F_GFX3D(128000000, pll8, 1,  3),
	F_GFX3D(145455000, pll2, 2, 11),
	F_GFX3D(160000000, pll2, 1,  5),
	F_GFX3D(177778000, pll2, 2,  9),
	F_GFX3D(200000000, pll2, 1,  4),
	F_GFX3D(228571000, pll2, 2,  7),
	F_GFX3D(266667000, pll2, 1,  3),
	F_GFX3D(300000000, pll3, 1,  4),
	F_GFX3D(320000000, pll2, 2,  5),
	F_GFX3D(400000000, pll2, 1,  2),
	F_END
};

static struct clk_freq_tbl clk_tbl_gfx3d_8930ab[] = {
	F_GFX3D(        0, gnd,   0,  0),
	F_GFX3D( 27000000, pxo,   0,  0),
	F_GFX3D( 48000000, pll8,  1,  8),
	F_GFX3D( 54857000, pll8,  1,  7),
	F_GFX3D( 64000000, pll8,  1,  6),
	F_GFX3D( 76800000, pll8,  1,  5),
	F_GFX3D( 96000000, pll8,  1,  4),
	F_GFX3D(128000000, pll8,  1,  3),
	F_GFX3D(145455000, pll2,  2, 11),
	F_GFX3D(160000000, pll2,  1,  5),
	F_GFX3D(177778000, pll2,  2,  9),
	F_GFX3D(192000000, pll8,  1,  2),
	F_GFX3D(200000000, pll2,  1,  4),
	F_GFX3D(228571000, pll2,  2,  7),
	F_GFX3D(266667000, pll2,  1,  3),
	F_GFX3D(320000000, pll2,  2,  5),
	F_GFX3D(400000000, pll2,  1,  2),
	F_GFX3D(500000000, pll15, 1,  2),
	F_END
};

static unsigned long fmax_gfx3d_8064ab[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 128000000,
	[VDD_DIG_NOMINAL] = 325000000,
	[VDD_DIG_HIGH]    = 450000000
};

static unsigned long fmax_gfx3d_8064[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 128000000,
	[VDD_DIG_NOMINAL] = 325000000,
	[VDD_DIG_HIGH]    = 400000000
};

static unsigned long fmax_gfx3d_8930[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 192000000,
	[VDD_DIG_NOMINAL] = 320000000,
	[VDD_DIG_HIGH]    = 400000000
};

static unsigned long fmax_gfx3d_8930aa[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 192000000,
	[VDD_DIG_NOMINAL] = 320000000,
	[VDD_DIG_HIGH]    = 450000000
};

static unsigned long fmax_gfx3d_8930ab[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 192000000,
	[VDD_DIG_NOMINAL] = 320000000,
	[VDD_DIG_HIGH]    = 500000000
};

static unsigned long fmax_gfx3d_8960ab_400[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 192000000,
	[VDD_DIG_NOMINAL] = 325000000,
	[VDD_DIG_HIGH]    = 400000000
};

static unsigned long fmax_gfx3d_8960ab_440[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 192000000,
	[VDD_DIG_NOMINAL] = 325000000,
	[VDD_DIG_HIGH]    = 440000000
};

static unsigned long *fmax_gfx3d_8960ab[] = {
	[0] = fmax_gfx3d_8960ab_400,
	[1] = fmax_gfx3d_8960ab_440,
};

static struct bank_masks bmnd_info_gfx3d = {
	.bank_sel_mask =		BIT(11),
	.bank0_mask = {
			.md_reg =		GFX3D_MD0_REG,
			.ns_mask =		BM(21, 18) | BM(5, 3),
			.rst_mask =		BIT(23),
			.mnd_en_mask =		BIT(8),
			.mode_mask =		BM(10, 9),
	},
	.bank1_mask = {
			.md_reg =		GFX3D_MD1_REG,
			.ns_mask =		BM(17, 14) | BM(2, 0),
			.rst_mask =		BIT(22),
			.mnd_en_mask =		BIT(5),
			.mode_mask =		BM(7, 6),
	},
};

static struct rcg_clk gfx3d_clk = {
	.b = {
		.ctl_reg = GFX3D_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(12),
		.halt_reg = DBG_BUS_VEC_A_REG,
		.halt_bit = 4,
		.retain_reg = GFX3D_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = GFX3D_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.freq_tbl = clk_tbl_gfx3d,
	.bank_info = &bmnd_info_gfx3d,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "gfx3d_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW,  128000000, NOMINAL, 300000000,
				  HIGH, 400000000),
		CLK_INIT(gfx3d_clk.c),
		.depends = &gmem_axi_clk.c,
	},
};

#define F_VCAP(f, s, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD4(4, m, 0, n), \
		.ns_val = NS_MND_BANKED4(18, 14, n, m, 3, 0, s##_to_mm_mux), \
		.ctl_val = CC_BANKED(9, 6, n), \
	}

static struct clk_freq_tbl clk_tbl_vcap[] = {
	F_VCAP(        0, gnd,  0,  0),
	F_VCAP( 27000000, pxo,  0,  0),
	F_VCAP( 54860000, pll8, 1,  7),
	F_VCAP( 64000000, pll8, 1,  6),
	F_VCAP( 76800000, pll8, 1,  5),
	F_VCAP(128000000, pll8, 1,  3),
	F_VCAP(160000000, pll2, 1,  5),
	F_VCAP(200000000, pll2, 1,  4),
	F_END
};

static struct bank_masks bmnd_info_vcap = {
	.bank_sel_mask =                BIT(11),
	.bank0_mask = {
		.md_reg =               VCAP_MD0_REG,
		.ns_mask =              BM(21, 18) | BM(5, 3),
		.rst_mask =             BIT(23),
		.mnd_en_mask =          BIT(8),
		.mode_mask =            BM(10, 9),
	},
	.bank1_mask = {
		.md_reg =               VCAP_MD1_REG,
		.ns_mask =              BM(17, 14) | BM(2, 0),
		.rst_mask =             BIT(22),
		.mnd_en_mask =          BIT(5),
		.mode_mask =            BM(7, 6),
	},
};

static struct rcg_clk vcap_clk = {
	.b = {
		.ctl_reg = VCAP_CC_REG,
		.en_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_J_REG,
		.halt_bit = 15,
	},
	.ns_reg = VCAP_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.freq_tbl = clk_tbl_vcap,
	.bank_info = &bmnd_info_vcap,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "vcap_clk",
		.ops = &clk_ops_rcg,
		.depends = &vcap_axi_clk.c,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(vcap_clk.c),
	},
};

static struct branch_clk vcap_npl_clk = {
	.b = {
		.ctl_reg = VCAP_CC_REG,
		.en_mask = BIT(13),
		.halt_reg = DBG_BUS_VEC_J_REG,
		.halt_bit = 25,
	},
	.c = {
		.parent = &vcap_clk.c,
		.dbg_name = "vcap_npl_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcap_npl_clk.c),
	},
};

#define F_IJPEG(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(23, 16, n, m, 15, 12, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
	}

static struct clk_freq_tbl clk_tbl_ijpeg[] = {
	F_IJPEG(        0, gnd,  1, 0,  0),
	F_IJPEG( 27000000, pxo,  1, 0,  0),
	F_IJPEG( 36570000, pll8, 1, 2, 21),
	F_IJPEG( 54860000, pll8, 7, 0,  0),
	F_IJPEG( 96000000, pll8, 4, 0,  0),
	F_IJPEG(109710000, pll8, 1, 2,  7),
	F_IJPEG(128000000, pll8, 3, 0,  0),
	F_IJPEG(153600000, pll8, 1, 2,  5),
	F_IJPEG(200000000, pll2, 4, 0,  0),
	F_IJPEG(228571000, pll2, 1, 2,  7),
	F_IJPEG(266667000, pll2, 1, 1,  3),
	F_IJPEG(320000000, pll2, 1, 2,  5),
	F_END
};

static unsigned long fmax_ijpeg_8064[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 128000000,
	[VDD_DIG_NOMINAL] = 266667000,
	[VDD_DIG_HIGH]    = 320000000
};

static struct rcg_clk ijpeg_clk = {
	.b = {
		.ctl_reg = IJPEG_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(9),
		.halt_reg = DBG_BUS_VEC_A_REG,
		.halt_bit = 24,
		.retain_reg = IJPEG_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = IJPEG_NS_REG,
	.md_reg = IJPEG_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(23, 16) | BM(15, 12) | BM(2, 0)),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_ijpeg,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "ijpeg_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 110000000, NOMINAL, 266667000,
				  HIGH, 320000000),
		CLK_INIT(ijpeg_clk.c),
		.depends = &ijpeg_axi_clk.c,
	},
};

#define F_JPEGD(f, s, d) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_DIVSRC(15, 12, d, 2, 0, s##_to_mm_mux), \
	}
static struct clk_freq_tbl clk_tbl_jpegd[] = {
	F_JPEGD(        0, gnd,  1),
	F_JPEGD( 64000000, pll8, 6),
	F_JPEGD( 76800000, pll8, 5),
	F_JPEGD( 96000000, pll8, 4),
	F_JPEGD(160000000, pll2, 5),
	F_JPEGD(200000000, pll2, 4),
	F_END
};

static struct rcg_clk jpegd_clk = {
	.b = {
		.ctl_reg = JPEGD_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(19),
		.halt_reg = DBG_BUS_VEC_A_REG,
		.halt_bit = 19,
		.retain_reg = JPEGD_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = JPEGD_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask =  (BM(15, 12) | BM(2, 0)),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_jpegd,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "jpegd_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 96000000, NOMINAL, 200000000),
		CLK_INIT(jpegd_clk.c),
		.depends = &jpegd_axi_clk.c,
	},
};

#define F_MDP(f, s, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MND_BANKED8(22, 14, n, m, 3, 0, s##_to_mm_mux), \
		.ctl_val = CC_BANKED(9, 6, n), \
	}

static struct clk_freq_tbl clk_tbl_mdp[] = {
	F_MDP(        0, gnd,  0,  0),
	F_MDP(  9600000, pll8, 1, 40),
	F_MDP( 13710000, pll8, 1, 28),
	F_MDP( 27000000, pxo,  0,  0),
	F_MDP( 29540000, pll8, 1, 13),
	F_MDP( 34910000, pll8, 1, 11),
	F_MDP( 38400000, pll8, 1, 10),
	F_MDP( 59080000, pll8, 2, 13),
	F_MDP( 76800000, pll8, 1,  5),
	F_MDP( 85330000, pll8, 2,  9),
	F_MDP( 96000000, pll8, 1,  4),
	F_MDP(128000000, pll8, 1,  3),
	F_MDP(160000000, pll2, 1,  5),
	F_MDP(177780000, pll2, 2,  9),
	F_MDP(200000000, pll2, 1,  4),
	F_MDP(228571000, pll2, 2,  7),
	F_MDP(266667000, pll2, 1,  3),
	F_END
};

static unsigned long fmax_mdp_8064[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 128000000,
	[VDD_DIG_NOMINAL] = 266667000
};

static struct bank_masks bmnd_info_mdp = {
	.bank_sel_mask =		BIT(11),
	.bank0_mask = {
			.md_reg =		MDP_MD0_REG,
			.ns_mask =		BM(29, 22) | BM(5, 3),
			.rst_mask =		BIT(31),
			.mnd_en_mask =		BIT(8),
			.mode_mask =		BM(10, 9),
	},
	.bank1_mask = {
			.md_reg =		MDP_MD1_REG,
			.ns_mask =		BM(21, 14) | BM(2, 0),
			.rst_mask =		BIT(30),
			.mnd_en_mask =		BIT(5),
			.mode_mask =		BM(7, 6),
	},
};

static struct rcg_clk mdp_clk = {
	.b = {
		.ctl_reg = MDP_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(21),
		.halt_reg = DBG_BUS_VEC_C_REG,
		.halt_bit = 10,
		.retain_reg = MDP_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = MDP_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.freq_tbl = clk_tbl_mdp,
	.bank_info = &bmnd_info_mdp,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "mdp_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 96000000, NOMINAL, 200000000),
		CLK_INIT(mdp_clk.c),
		.depends = &mdp_axi_clk.c,
	},
};

static unsigned long fmax_mdp_8960ab[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 128000000,
	[VDD_DIG_NOMINAL] = 266667000
};

static struct branch_clk lut_mdp_clk = {
	.b = {
		.ctl_reg = MDP_LUT_CC_REG,
		.en_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 13,
		.retain_reg = MDP_LUT_CC_REG,
		.retain_mask = BIT(31),
	},
	.c = {
		.parent = &mdp_clk.c,
		.dbg_name = "lut_mdp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(lut_mdp_clk.c),
	},
};

#define F_MDP_VSYNC(f, s) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_SRC_SEL(13, 13, s##_to_bb_mux), \
	}
static struct clk_freq_tbl clk_tbl_mdp_vsync[] = {
	F_MDP_VSYNC(27000000, pxo),
	F_END
};

static struct rcg_clk mdp_vsync_clk = {
	.b = {
		.ctl_reg = MISC_CC_REG,
		.en_mask = BIT(6),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(3),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 22,
	},
	.ns_reg = MISC_CC2_REG,
	.ns_mask = BIT(13),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_mdp_vsync,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "mdp_vsync_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 27000000),
		CLK_INIT(mdp_vsync_clk.c),
	},
};

#define F_ROT(f, s, d) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_DIVSRC_BANKED(29, 26, 25, 22, d, \
				21, 19, 18, 16, s##_to_mm_mux), \
	}
static struct clk_freq_tbl clk_tbl_rot[] = {
	F_ROT(        0, gnd,   1),
	F_ROT( 27000000, pxo,   1),
	F_ROT( 29540000, pll8, 13),
	F_ROT( 32000000, pll8, 12),
	F_ROT( 38400000, pll8, 10),
	F_ROT( 48000000, pll8,  8),
	F_ROT( 54860000, pll8,  7),
	F_ROT( 64000000, pll8,  6),
	F_ROT( 76800000, pll8,  5),
	F_ROT( 96000000, pll8,  4),
	F_ROT(100000000, pll2,  8),
	F_ROT(114290000, pll2,  7),
	F_ROT(133330000, pll2,  6),
	F_ROT(160000000, pll2,  5),
	F_ROT(200000000, pll2,  4),
	F_END
};

static struct bank_masks bdiv_info_rot = {
	.bank_sel_mask = BIT(30),
	.bank0_mask = {
		.ns_mask =	BM(25, 22) | BM(18, 16),
	},
	.bank1_mask = {
		.ns_mask =	BM(29, 26) | BM(21, 19),
	},
};

static struct rcg_clk rot_clk = {
	.b = {
		.ctl_reg = ROT_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(2),
		.halt_reg = DBG_BUS_VEC_C_REG,
		.halt_bit = 15,
		.retain_reg = ROT_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = ROT_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_div_banked,
	.freq_tbl = clk_tbl_rot,
	.bank_info = &bdiv_info_rot,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "rot_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 96000000, NOMINAL, 200000000),
		CLK_INIT(rot_clk.c),
		.depends = &rot_axi_clk.c,
	},
};

static int hdmi_pll_clk_enable(struct clk *c)
{
	int ret;
	unsigned long flags;
	spin_lock_irqsave(&local_clock_reg_lock, flags);
	ret = hdmi_pll_enable();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
	return ret;
}

static void hdmi_pll_clk_disable(struct clk *c)
{
	unsigned long flags;
	spin_lock_irqsave(&local_clock_reg_lock, flags);
	hdmi_pll_disable();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

static struct clk_ops clk_ops_hdmi_pll = {
	.enable = hdmi_pll_clk_enable,
	.disable = hdmi_pll_clk_disable,
};

static struct clk hdmi_pll_clk = {
	.parent = &pxo_clk.c,
	.dbg_name = "hdmi_pll_clk",
	.ops = &clk_ops_hdmi_pll,
	.vdd_class = &vdd_sr2_hdmi_pll,
	.fmax = (unsigned long [VDD_SR2_HDMI_PLL_NUM]) {
		[VDD_SR2_HDMI_PLL_ON] = ULONG_MAX,
	},
	.num_fmax = VDD_SR2_HDMI_PLL_NUM,
	CLK_INIT(hdmi_pll_clk),
};

#define F_TV_GND(f, s, p_r, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(23, 16, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
	}
#define F_TV(f, s, p_r, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(23, 16, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
		.extra_freq_data = (void *)p_r, \
	}
/* Switching TV freqs requires PLL reconfiguration. */
static struct clk_freq_tbl clk_tbl_tv[] = {
	F_TV_GND(    0,      gnd,         0, 1, 0, 0),
	F_TV( 25200000, hdmi_pll,  25200000, 1, 0, 0),
	F_TV( 27000000, hdmi_pll,  27000000, 1, 0, 0),
	F_TV( 27030000, hdmi_pll,  27030000, 1, 0, 0),
	F_TV( 74250000, hdmi_pll,  74250000, 1, 0, 0),
	F_TV(108000000, hdmi_pll, 108000000, 1, 0, 0),
	F_TV(148500000, hdmi_pll, 148500000, 1, 0, 0),
	F_END
};

static unsigned long fmax_tv_src_8064[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     =  74250000,
	[VDD_DIG_NOMINAL] = 149000000
};

/*
 * Unlike other clocks, the TV rate is adjusted through PLL
 * re-programming. It is also routed through an MND divider.
 */
void set_rate_tv(struct rcg_clk *rcg, struct clk_freq_tbl *nf)
{
	unsigned long pll_rate = (unsigned long)nf->extra_freq_data;
	if (pll_rate) {
		hdmi_pll_set_rate(pll_rate);
		hdmi_pll_clk.rate = pll_rate;
	}
	set_rate_mnd(rcg, nf);
}

static struct rcg_clk tv_src_clk = {
	.ns_reg = TV_NS_REG,
	.b = {
		.ctl_reg = TV_CC_REG,
		.halt_check = NOCHECK,
		.retain_reg = TV_CC_REG,
		.retain_mask = BIT(31),
	},
	.md_reg = TV_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(23, 16) | BM(15, 14) | BM(2, 0)),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_tv,
	.freq_tbl = clk_tbl_tv,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "tv_src_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 27030000, NOMINAL, 149000000),
		CLK_INIT(tv_src_clk.c),
	},
};

static struct cdiv_clk tv_src_div_clk = {
	.b = {
		.ctl_reg = TV_NS_REG,
		.halt_check = NOCHECK,
	},
	.ns_reg = TV_NS_REG,
	.div_offset = 6,
	.max_div = 2,
	.c = {
		.dbg_name = "tv_src_div_clk",
		.ops = &clk_ops_cdiv,
		CLK_INIT(tv_src_div_clk.c),
		.rate = ULONG_MAX,
	},
};

static struct branch_clk tv_enc_clk = {
	.b = {
		.ctl_reg = TV_CC_REG,
		.en_mask = BIT(8),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_D_REG,
		.halt_bit = 9,
	},
	.c = {
		.parent = &tv_src_clk.c,
		.dbg_name = "tv_enc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(tv_enc_clk.c),
	},
};

static struct branch_clk tv_dac_clk = {
	.b = {
		.ctl_reg = TV_CC_REG,
		.en_mask = BIT(10),
		.halt_reg = DBG_BUS_VEC_D_REG,
		.halt_bit = 10,
	},
	.c = {
		.parent = &tv_src_clk.c,
		.dbg_name = "tv_dac_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(tv_dac_clk.c),
	},
};

static struct branch_clk mdp_tv_clk = {
	.b = {
		.ctl_reg = TV_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(4),
		.halt_reg = DBG_BUS_VEC_D_REG,
		.halt_bit = 12,
		.retain_reg = TV_CC2_REG,
		.retain_mask = BIT(10),
	},
	.c = {
		.parent = &tv_src_clk.c,
		.dbg_name = "mdp_tv_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdp_tv_clk.c),
	},
};

static struct branch_clk hdmi_tv_clk = {
	.b = {
		.ctl_reg = TV_CC_REG,
		.en_mask = BIT(12),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(1),
		.halt_reg = DBG_BUS_VEC_D_REG,
		.halt_bit = 11,
	},
	.c = {
		.parent = &tv_src_clk.c,
		.dbg_name = "hdmi_tv_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(hdmi_tv_clk.c),
	},
};

static struct branch_clk rgb_tv_clk = {
	.b = {
		.ctl_reg = TV_CC2_REG,
		.en_mask = BIT(14),
		.halt_reg = DBG_BUS_VEC_J_REG,
		.halt_bit = 27,
	},
	.c = {
		.parent = &tv_src_clk.c,
		.dbg_name = "rgb_tv_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(rgb_tv_clk.c),
	},
};

static struct branch_clk npl_tv_clk = {
	.b = {
		.ctl_reg = TV_CC2_REG,
		.en_mask = BIT(16),
		.halt_reg = DBG_BUS_VEC_J_REG,
		.halt_bit = 26,
	},
	.c = {
		.parent = &tv_src_clk.c,
		.dbg_name = "npl_tv_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(npl_tv_clk.c),
	},
};

static struct branch_clk hdmi_app_clk = {
	.b = {
		.ctl_reg = MISC_CC2_REG,
		.en_mask = BIT(11),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(11),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 25,
	},
	.c = {
		.dbg_name = "hdmi_app_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(hdmi_app_clk.c),
	},
};

static struct bank_masks bmnd_info_vcodec = {
	.bank_sel_mask =		BIT(13),
	.bank0_mask = {
			.md_reg =		VCODEC_MD0_REG,
			.ns_mask =		BM(18, 11) | BM(2, 0),
			.rst_mask =		BIT(31),
			.mnd_en_mask =		BIT(5),
			.mode_mask =		BM(7, 6),
	},
	.bank1_mask = {
			.md_reg =		VCODEC_MD1_REG,
			.ns_mask =		BM(26, 19) | BM(29, 27),
			.rst_mask =		BIT(30),
			.mnd_en_mask =		BIT(10),
			.mode_mask =		BM(12, 11),
	},
};
#define F_VCODEC(f, s, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MND_BANKED8(11, 19, n, m, 0, 27, s##_to_mm_mux), \
		.ctl_val = CC_BANKED(6, 11, n), \
	}
static struct clk_freq_tbl clk_tbl_vcodec[] = {
	F_VCODEC(        0, gnd,  0,  0),
	F_VCODEC( 27000000, pxo,  0,  0),
	F_VCODEC( 32000000, pll8, 1, 12),
	F_VCODEC( 48000000, pll8, 1,  8),
	F_VCODEC( 54860000, pll8, 1,  7),
	F_VCODEC( 96000000, pll8, 1,  4),
	F_VCODEC(133330000, pll2, 1,  6),
	F_VCODEC(200000000, pll2, 1,  4),
	F_VCODEC(228570000, pll2, 2,  7),
	F_VCODEC(266670000, pll2, 1,  3),
	F_END
};

static struct rcg_clk vcodec_clk = {
	.b = {
		.ctl_reg = VCODEC_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(6),
		.halt_reg = DBG_BUS_VEC_C_REG,
		.halt_bit = 29,
		.retain_reg = VCODEC_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = VCODEC_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.bank_info = &bmnd_info_vcodec,
	.freq_tbl = clk_tbl_vcodec,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "vcodec_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW,  100000000, NOMINAL, 200000000,
				  HIGH, 228571000),
		CLK_INIT(vcodec_clk.c),
		.depends = &vcodec_axi_clk.c,
	},
};

static unsigned long fmax_vcodec_8064v2[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 100000000,
	[VDD_DIG_NOMINAL] = 200000000,
	[VDD_DIG_HIGH]    = 266670000,
};

static unsigned long fmax_vcodec_8930ab[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 100000000,
	[VDD_DIG_NOMINAL] = 200000000,
	[VDD_DIG_HIGH]    = 266670000
};

#define F_VPE(f, s, d) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_DIVSRC(15, 12, d, 2, 0, s##_to_mm_mux), \
	}
static struct clk_freq_tbl clk_tbl_vpe[] = {
	F_VPE(        0, gnd,   1),
	F_VPE( 27000000, pxo,   1),
	F_VPE( 34909000, pll8, 11),
	F_VPE( 38400000, pll8, 10),
	F_VPE( 64000000, pll8,  6),
	F_VPE( 76800000, pll8,  5),
	F_VPE( 96000000, pll8,  4),
	F_VPE(100000000, pll2,  8),
	F_VPE(160000000, pll2,  5),
	F_END
};

static struct rcg_clk vpe_clk = {
	.b = {
		.ctl_reg = VPE_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(17),
		.halt_reg = DBG_BUS_VEC_A_REG,
		.halt_bit = 28,
		.retain_reg = VPE_CC_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = VPE_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(15, 12) | BM(2, 0)),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_vpe,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "vpe_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 76800000, NOMINAL, 160000000),
		CLK_INIT(vpe_clk.c),
		.depends = &vpe_axi_clk.c,
	},
};

#define F_VFE(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(23, 16, n, m, 11, 10, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
	}

static struct clk_freq_tbl clk_tbl_vfe[] = {
	F_VFE(        0, gnd,   1, 0,  0),
	F_VFE( 13960000, pll8,  1, 2, 55),
	F_VFE( 27000000, pxo,   1, 0,  0),
	F_VFE( 36570000, pll8,  1, 2, 21),
	F_VFE( 38400000, pll8,  2, 1,  5),
	F_VFE( 45180000, pll8,  1, 2, 17),
	F_VFE( 48000000, pll8,  2, 1,  4),
	F_VFE( 54860000, pll8,  1, 1,  7),
	F_VFE( 64000000, pll8,  2, 1,  3),
	F_VFE( 76800000, pll8,  1, 1,  5),
	F_VFE( 96000000, pll8,  2, 1,  2),
	F_VFE(109710000, pll8,  1, 2,  7),
	F_VFE(128000000, pll8,  1, 1,  3),
	F_VFE(153600000, pll8,  1, 2,  5),
	F_VFE(200000000, pll2,  2, 1,  2),
	F_VFE(228570000, pll2,  1, 2,  7),
	F_VFE(266667000, pll2,  1, 1,  3),
	F_VFE(320000000, pll2,  1, 2,  5),
	F_END
};

static unsigned long fmax_vfe_8064[VDD_DIG_NUM] = {
	[VDD_DIG_LOW]     = 128000000,
	[VDD_DIG_NOMINAL] = 266667000,
	[VDD_DIG_HIGH]    = 320000000
};

static struct rcg_clk vfe_clk = {
	.b = {
		.ctl_reg = VFE_CC_REG,
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(15),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 6,
		.en_mask = BIT(0),
		.retain_reg = VFE_CC2_REG,
		.retain_mask = BIT(31),
	},
	.ns_reg = VFE_NS_REG,
	.md_reg = VFE_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(23, 16) | BM(11, 10) | BM(2, 0)),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_vfe,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "vfe_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 110000000, NOMINAL, 266667000,
				  HIGH, 320000000),
		CLK_INIT(vfe_clk.c),
		.depends = &vfe_axi_clk.c,
	},
};

static struct branch_clk csi_vfe_clk = {
	.b = {
		.ctl_reg = VFE_CC_REG,
		.en_mask = BIT(12),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(24),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 8,
	},
	.c = {
		.parent = &vfe_clk.c,
		.dbg_name = "csi_vfe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi_vfe_clk.c),
	},
};

/*
 * Low Power Audio Clocks
 */
#define F_AIF_OSR(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS(31, 24, n, m, 5, 4, 3, d, 2, 0, s##_to_lpa_mux), \
	}
static struct clk_freq_tbl clk_tbl_aif_osr_492[] = {
	F_AIF_OSR(       0, gnd,  1, 0,   0),
	F_AIF_OSR(  512000, pll4, 4, 1, 240),
	F_AIF_OSR(  768000, pll4, 4, 1, 160),
	F_AIF_OSR( 1024000, pll4, 4, 1, 120),
	F_AIF_OSR( 1536000, pll4, 4, 1,  80),
	F_AIF_OSR( 2048000, pll4, 4, 1,  60),
	F_AIF_OSR( 3072000, pll4, 4, 1,  40),
	F_AIF_OSR( 4096000, pll4, 4, 1,  30),
	F_AIF_OSR( 6144000, pll4, 4, 1,  20),
	F_AIF_OSR( 8192000, pll4, 4, 1,  15),
	F_AIF_OSR(12288000, pll4, 4, 1,  10),
	F_AIF_OSR(24576000, pll4, 4, 1,   5),
	F_AIF_OSR(27000000, pxo,  1, 0,   0),
	F_END
};

static struct clk_freq_tbl clk_tbl_aif_osr_393[] = {
	F_AIF_OSR(       0, gnd,  1, 0,   0),
	F_AIF_OSR(  512000, pll4, 4, 1, 192),
	F_AIF_OSR(  768000, pll4, 4, 1, 128),
	F_AIF_OSR( 1024000, pll4, 4, 1,  96),
	F_AIF_OSR( 1536000, pll4, 4, 1,  64),
	F_AIF_OSR( 2048000, pll4, 4, 1,  48),
	F_AIF_OSR( 3072000, pll4, 4, 1,  32),
	F_AIF_OSR( 4096000, pll4, 4, 1,  24),
	F_AIF_OSR( 6144000, pll4, 4, 1,  16),
	F_AIF_OSR( 8192000, pll4, 4, 1,  12),
	F_AIF_OSR(12288000, pll4, 4, 1,   8),
	F_AIF_OSR(24576000, pll4, 4, 1,   4),
	F_AIF_OSR(27000000, pxo,  1, 0,   0),
	F_END
};

#define CLK_AIF_OSR(i, ns, md, h_r) \
	struct rcg_clk i##_clk = { \
		.b = { \
			.ctl_reg = ns, \
			.en_mask = BIT(17), \
			.reset_reg = ns, \
			.reset_mask = BIT(19), \
			.halt_reg = h_r, \
			.halt_check = ENABLE, \
			.halt_bit = 1, \
		}, \
		.ns_reg = ns, \
		.md_reg = md, \
		.root_en_mask = BIT(9), \
		.ns_mask = (BM(31, 24) | BM(6, 0)), \
		.mnd_en_mask = BIT(8), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_aif_osr_393, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP1(LOW, 27000000), \
			CLK_INIT(i##_clk.c), \
		}, \
	}
#define CLK_AIF_OSR_DIV(i, ns, md, h_r) \
	struct rcg_clk i##_clk = { \
		.b = { \
			.ctl_reg = ns, \
			.en_mask = BIT(21), \
			.reset_reg = ns, \
			.reset_mask = BIT(23), \
			.halt_reg = h_r, \
			.halt_check = ENABLE, \
			.halt_bit = 1, \
		}, \
		.ns_reg = ns, \
		.md_reg = md, \
		.root_en_mask = BIT(9), \
		.ns_mask = (BM(31, 24) | BM(6, 0)), \
		.mnd_en_mask = BIT(8), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_aif_osr_393, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP1(LOW, 27000000), \
			CLK_INIT(i##_clk.c), \
		}, \
	}

#define CLK_AIF_BIT(i, ns, h_r) \
	struct cdiv_clk i##_clk = { \
		.b = { \
			.ctl_reg = ns, \
			.en_mask = BIT(15), \
			.halt_reg = h_r, \
			.halt_check = DELAY, \
		}, \
		.ns_reg = ns, \
		.ext_mask = BIT(14), \
		.div_offset = 10, \
		.max_div = 16, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_cdiv, \
			CLK_INIT(i##_clk.c), \
			.rate = ULONG_MAX, \
		}, \
	}

#define CLK_AIF_BIT_DIV(i, ns, h_r) \
	struct cdiv_clk i##_clk = { \
		.b = { \
			.ctl_reg = ns, \
			.en_mask = BIT(19), \
			.halt_reg = h_r, \
			.halt_check = DELAY, \
		}, \
		.ns_reg = ns, \
		.ext_mask = BIT(18), \
		.div_offset = 10, \
		.max_div = 256, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_cdiv, \
			CLK_INIT(i##_clk.c), \
			.rate = ULONG_MAX, \
		}, \
	}

static CLK_AIF_OSR(mi2s_osr, LCC_MI2S_NS_REG, LCC_MI2S_MD_REG,
		LCC_MI2S_STATUS_REG);
static CLK_AIF_BIT(mi2s_bit, LCC_MI2S_NS_REG, LCC_MI2S_STATUS_REG);

static CLK_AIF_OSR_DIV(codec_i2s_mic_osr, LCC_CODEC_I2S_MIC_NS_REG,
		LCC_CODEC_I2S_MIC_MD_REG, LCC_CODEC_I2S_MIC_STATUS_REG);
static CLK_AIF_BIT_DIV(codec_i2s_mic_bit, LCC_CODEC_I2S_MIC_NS_REG,
		LCC_CODEC_I2S_MIC_STATUS_REG);

static CLK_AIF_OSR_DIV(spare_i2s_mic_osr, LCC_SPARE_I2S_MIC_NS_REG,
		LCC_SPARE_I2S_MIC_MD_REG, LCC_SPARE_I2S_MIC_STATUS_REG);
static CLK_AIF_BIT_DIV(spare_i2s_mic_bit, LCC_SPARE_I2S_MIC_NS_REG,
		LCC_SPARE_I2S_MIC_STATUS_REG);

static CLK_AIF_OSR_DIV(codec_i2s_spkr_osr, LCC_CODEC_I2S_SPKR_NS_REG,
		LCC_CODEC_I2S_SPKR_MD_REG, LCC_CODEC_I2S_SPKR_STATUS_REG);
static CLK_AIF_BIT_DIV(codec_i2s_spkr_bit, LCC_CODEC_I2S_SPKR_NS_REG,
		LCC_CODEC_I2S_SPKR_STATUS_REG);

static CLK_AIF_OSR_DIV(spare_i2s_spkr_osr, LCC_SPARE_I2S_SPKR_NS_REG,
		LCC_SPARE_I2S_SPKR_MD_REG, LCC_SPARE_I2S_SPKR_STATUS_REG);
static CLK_AIF_BIT_DIV(spare_i2s_spkr_bit, LCC_SPARE_I2S_SPKR_NS_REG,
		LCC_SPARE_I2S_SPKR_STATUS_REG);

#define F_PCM(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD16(m, n), \
		.ns_val = NS(31, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_lpa_mux), \
	}
static struct clk_freq_tbl clk_tbl_pcm_492[] = {
	{ .ns_val = BIT(10) /* external input */ },
	F_PCM(  256000, pll4, 4, 1, 480),
	F_PCM(  512000, pll4, 4, 1, 240),
	F_PCM(  768000, pll4, 4, 1, 160),
	F_PCM( 1024000, pll4, 4, 1, 120),
	F_PCM( 1536000, pll4, 4, 1,  80),
	F_PCM( 2048000, pll4, 4, 1,  60),
	F_PCM( 3072000, pll4, 4, 1,  40),
	F_PCM( 4096000, pll4, 4, 1,  30),
	F_PCM( 6144000, pll4, 4, 1,  20),
	F_PCM( 8192000, pll4, 4, 1,  15),
	F_PCM(12288000, pll4, 4, 1,  10),
	F_PCM(24576000, pll4, 4, 1,   5),
	F_PCM(27000000, pxo,  1, 0,   0),
	F_END
};

static struct clk_freq_tbl clk_tbl_pcm_393[] = {
	{ .ns_val = BIT(10) /* external input */ },
	F_PCM(  256000, pll4, 4, 1, 384),
	F_PCM(  512000, pll4, 4, 1, 192),
	F_PCM(  768000, pll4, 4, 1, 128),
	F_PCM( 1024000, pll4, 4, 1,  96),
	F_PCM( 1536000, pll4, 4, 1,  64),
	F_PCM( 2048000, pll4, 4, 1,  48),
	F_PCM( 3072000, pll4, 4, 1,  32),
	F_PCM( 4096000, pll4, 4, 1,  24),
	F_PCM( 6144000, pll4, 4, 1,  16),
	F_PCM( 8192000, pll4, 4, 1,  12),
	F_PCM(12288000, pll4, 4, 1,   8),
	F_PCM(24576000, pll4, 4, 1,   4),
	F_PCM(27000000, pxo,  1, 0,   0),
	F_END
};

static struct rcg_clk pcm_clk = {
	.b = {
		.ctl_reg = LCC_PCM_NS_REG,
		.en_mask = BIT(11),
		.reset_reg = LCC_PCM_NS_REG,
		.reset_mask = BIT(13),
		.halt_reg = LCC_PCM_STATUS_REG,
		.halt_check = ENABLE,
		.halt_bit = 0,
	},
	.ns_reg = LCC_PCM_NS_REG,
	.md_reg = LCC_PCM_MD_REG,
	.root_en_mask = BIT(9),
	.ns_mask = BM(31, 16) | BIT(10) | BM(6, 0),
	.mnd_en_mask = BIT(8),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_pcm_393,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "pcm_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 27000000),
		CLK_INIT(pcm_clk.c),
		.rate = ULONG_MAX,
	},
};

static struct rcg_clk audio_slimbus_clk = {
	.b = {
		.ctl_reg = LCC_SLIMBUS_NS_REG,
		.en_mask = BIT(10),
		.reset_reg = LCC_AHBEX_BRANCH_CTL_REG,
		.reset_mask = BIT(5),
		.halt_reg = LCC_SLIMBUS_STATUS_REG,
		.halt_check = ENABLE,
		.halt_bit = 0,
	},
	.ns_reg = LCC_SLIMBUS_NS_REG,
	.md_reg = LCC_SLIMBUS_MD_REG,
	.root_en_mask = BIT(9),
	.ns_mask = (BM(31, 24) | BM(6, 0)),
	.mnd_en_mask = BIT(8),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_aif_osr_393,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "audio_slimbus_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 27000000),
		CLK_INIT(audio_slimbus_clk.c),
	},
};

static struct branch_clk sps_slimbus_clk = {
	.b = {
		.ctl_reg = LCC_SLIMBUS_NS_REG,
		.en_mask = BIT(12),
		.halt_reg = LCC_SLIMBUS_STATUS_REG,
		.halt_check = ENABLE,
		.halt_bit = 1,
	},
	.c = {
		.parent = &audio_slimbus_clk.c,
		.dbg_name = "sps_slimbus_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(sps_slimbus_clk.c),
	},
};

static struct branch_clk slimbus_xo_src_clk = {
	.b = {
		.ctl_reg = SLIMBUS_XO_SRC_CLK_CTL_REG,
		.en_mask = BIT(2),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 28,
	},
	.c = {
		.parent = &sps_slimbus_clk.c,
		.dbg_name = "slimbus_xo_src_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(slimbus_xo_src_clk.c),
	},
};

DEFINE_CLK_RPM(afab_clk, afab_a_clk, APPS_FABRIC, NULL);
DEFINE_CLK_RPM(cfpb_clk, cfpb_a_clk, CFPB, NULL);
DEFINE_CLK_RPM(dfab_clk, dfab_a_clk, DAYTONA_FABRIC, NULL);
DEFINE_CLK_RPM(ebi1_clk, ebi1_a_clk, EBI1, NULL);
DEFINE_CLK_RPM(mmfab_clk, mmfab_a_clk, MM_FABRIC, NULL);
DEFINE_CLK_RPM(mmfpb_clk, mmfpb_a_clk, MMFPB, NULL);
DEFINE_CLK_RPM(sfab_clk, sfab_a_clk, SYSTEM_FABRIC, NULL);
DEFINE_CLK_RPM(sfpb_clk, sfpb_a_clk, SFPB, NULL);
DEFINE_CLK_RPM_QDSS(qdss_clk, qdss_a_clk);

static DEFINE_CLK_VOTER(sfab_msmbus_a_clk, &sfab_a_clk.c, 0);
static DEFINE_CLK_VOTER(sfab_tmr_a_clk, &sfab_a_clk.c, 0);

static DEFINE_CLK_VOTER(dfab_dsps_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_usb_hs_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_usb_hs3_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_usb_hs4_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc1_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc2_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc3_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc4_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc5_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sps_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_bam_dmux_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_scm_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_msmbus_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_msmbus_a_clk, &dfab_a_clk.c, 0);

static DEFINE_CLK_VOTER(ebi1_msmbus_clk, &ebi1_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ebi1_adm_clk, &ebi1_clk.c, 0);

static DEFINE_CLK_VOTER(ebi1_acpu_a_clk, &ebi1_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ebi1_msmbus_a_clk, &ebi1_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(afab_acpu_a_clk, &afab_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(afab_msmbus_a_clk, &afab_a_clk.c, LONG_MAX);

#ifdef CONFIG_DEBUG_FS
struct measure_sel {
	u32 test_vector;
	struct clk *c;
};

static DEFINE_CLK_MEASURE(l2_m_clk);
static DEFINE_CLK_MEASURE(krait0_m_clk);
static DEFINE_CLK_MEASURE(krait1_m_clk);
static DEFINE_CLK_MEASURE(krait2_m_clk);
static DEFINE_CLK_MEASURE(krait3_m_clk);
static DEFINE_CLK_MEASURE(q6sw_clk);
static DEFINE_CLK_MEASURE(q6fw_clk);
static DEFINE_CLK_MEASURE(q6_func_clk);

static struct measure_sel measure_mux[] = {
	{ TEST_PER_LS(0x08), &slimbus_xo_src_clk.c },
	{ TEST_PER_LS(0x12), &sdc1_p_clk.c },
	{ TEST_PER_LS(0x13), &sdc1_clk.c },
	{ TEST_PER_LS(0x14), &sdc2_p_clk.c },
	{ TEST_PER_LS(0x15), &sdc2_clk.c },
	{ TEST_PER_LS(0x16), &sdc3_p_clk.c },
	{ TEST_PER_LS(0x17), &sdc3_clk.c },
	{ TEST_PER_LS(0x18), &sdc4_p_clk.c },
	{ TEST_PER_LS(0x19), &sdc4_clk.c },
	{ TEST_PER_LS(0x1A), &sdc5_p_clk.c },
	{ TEST_PER_LS(0x1B), &sdc5_clk.c },
	{ TEST_PER_LS(0x1F), &gp0_clk.c },
	{ TEST_PER_LS(0x20), &gp1_clk.c },
	{ TEST_PER_LS(0x21), &gp2_clk.c },
	{ TEST_PER_LS(0x25), &dfab_clk.c },
	{ TEST_PER_LS(0x25), &dfab_a_clk.c },
	{ TEST_PER_LS(0x26), &pmem_clk.c },
	{ TEST_PER_LS(0x32), &dma_bam_p_clk.c },
	{ TEST_PER_LS(0x33), &cfpb_clk.c },
	{ TEST_PER_LS(0x33), &cfpb_a_clk.c },
	{ TEST_PER_LS(0x3D), &gsbi1_p_clk.c },
	{ TEST_PER_LS(0x3E), &gsbi1_uart_clk.c },
	{ TEST_PER_LS(0x3F), &gsbi1_qup_clk.c },
	{ TEST_PER_LS(0x41), &gsbi2_p_clk.c },
	{ TEST_PER_LS(0x42), &gsbi2_uart_clk.c },
	{ TEST_PER_LS(0x44), &gsbi2_qup_clk.c },
	{ TEST_PER_LS(0x45), &gsbi3_p_clk.c },
	{ TEST_PER_LS(0x46), &gsbi3_uart_clk.c },
	{ TEST_PER_LS(0x48), &gsbi3_qup_clk.c },
	{ TEST_PER_LS(0x49), &gsbi4_p_clk.c },
	{ TEST_PER_LS(0x4A), &gsbi4_uart_clk.c },
	{ TEST_PER_LS(0x4C), &gsbi4_qup_clk.c },
	{ TEST_PER_LS(0x4D), &gsbi5_p_clk.c },
	{ TEST_PER_LS(0x4E), &gsbi5_uart_clk.c },
	{ TEST_PER_LS(0x50), &gsbi5_qup_clk.c },
	{ TEST_PER_LS(0x51), &gsbi6_p_clk.c },
	{ TEST_PER_LS(0x52), &gsbi6_uart_clk.c },
	{ TEST_PER_LS(0x54), &gsbi6_qup_clk.c },
	{ TEST_PER_LS(0x55), &gsbi7_p_clk.c },
	{ TEST_PER_LS(0x56), &gsbi7_uart_clk.c },
	{ TEST_PER_LS(0x58), &gsbi7_qup_clk.c },
	{ TEST_PER_LS(0x59), &gsbi8_p_clk.c },
	{ TEST_PER_LS(0x59), &sfab_sata_s_p_clk.c },
	{ TEST_PER_LS(0x5A), &gsbi8_uart_clk.c },
	{ TEST_PER_LS(0x5A), &sata_p_clk.c },
	{ TEST_PER_LS(0x5B), &sata_rxoob_clk.c },
	{ TEST_PER_LS(0x5C), &sata_pmalive_clk.c },
	{ TEST_PER_LS(0x5C), &gsbi8_qup_clk.c },
	{ TEST_PER_LS(0x5D), &gsbi9_p_clk.c },
	{ TEST_PER_LS(0x5E), &gsbi9_uart_clk.c },
	{ TEST_PER_LS(0x60), &gsbi9_qup_clk.c },
	{ TEST_PER_LS(0x61), &gsbi10_p_clk.c },
	{ TEST_PER_LS(0x62), &gsbi10_uart_clk.c },
	{ TEST_PER_LS(0x64), &gsbi10_qup_clk.c },
	{ TEST_PER_LS(0x65), &gsbi11_p_clk.c },
	{ TEST_PER_LS(0x66), &gsbi11_uart_clk.c },
	{ TEST_PER_LS(0x68), &gsbi11_qup_clk.c },
	{ TEST_PER_LS(0x69), &gsbi12_p_clk.c },
	{ TEST_PER_LS(0x6A), &gsbi12_uart_clk.c },
	{ TEST_PER_LS(0x6C), &gsbi12_qup_clk.c },
	{ TEST_PER_LS(0x5E), &pcie_p_clk.c },
	{ TEST_PER_LS(0x5F), &ce3_p_clk.c },
	{ TEST_PER_LS(0x60), &ce3_core_clk.c },
	{ TEST_PER_LS(0x63), &usb_hs3_p_clk.c },
	{ TEST_PER_LS(0x64), &usb_hs3_xcvr_clk.c },
	{ TEST_PER_LS(0x65), &usb_hs4_p_clk.c },
	{ TEST_PER_LS(0x66), &usb_hs4_xcvr_clk.c },
	{ TEST_PER_LS(0x6B), &sata_phy_ref_clk.c },
	{ TEST_PER_LS(0x6C), &sata_phy_cfg_clk.c },
	{ TEST_PER_LS(0x78), &sfpb_clk.c },
	{ TEST_PER_LS(0x78), &sfpb_a_clk.c },
	{ TEST_PER_LS(0x7A), &pmic_ssbi2_clk.c },
	{ TEST_PER_LS(0x7B), &pmic_arb0_p_clk.c },
	{ TEST_PER_LS(0x7C), &pmic_arb1_p_clk.c },
	{ TEST_PER_LS(0x7D), &prng_clk.c },
	{ TEST_PER_LS(0x7F), &rpm_msg_ram_p_clk.c },
	{ TEST_PER_LS(0x80), &adm0_p_clk.c },
	{ TEST_PER_LS(0x84), &usb_hs1_p_clk.c },
	{ TEST_PER_LS(0x85), &usb_hs1_xcvr_clk.c },
	{ TEST_PER_LS(0x86), &usb_hsic_p_clk.c },
	{ TEST_PER_LS(0x87), &usb_hsic_system_clk.c },
	{ TEST_PER_LS(0x88), &usb_hsic_xcvr_fs_clk.c },
	{ TEST_PER_LS(0x89), &usb_fs1_p_clk.c },
	{ TEST_PER_LS(0x8A), &usb_fs1_sys_clk.c },
	{ TEST_PER_LS(0x8B), &usb_fs1_xcvr_clk.c },
	{ TEST_PER_LS(0x8C), &usb_fs2_p_clk.c },
	{ TEST_PER_LS(0x8D), &usb_fs2_sys_clk.c },
	{ TEST_PER_LS(0x8E), &usb_fs2_xcvr_clk.c },
	{ TEST_PER_LS(0x8F), &tsif_p_clk.c },
	{ TEST_PER_LS(0x91), &tsif_ref_clk.c },
	{ TEST_PER_LS(0x92), &ce1_p_clk.c },
	{ TEST_PER_LS(0x94), &tssc_clk.c },
	{ TEST_PER_LS(0x9D), &usb_hsic_hsio_cal_clk.c },
	{ TEST_PER_LS(0xA4), &ce1_core_clk.c },

	{ TEST_PER_HS(0x07), &afab_clk.c },
	{ TEST_PER_HS(0x07), &afab_a_clk.c },
	{ TEST_PER_HS(0x18), &sfab_clk.c },
	{ TEST_PER_HS(0x18), &sfab_a_clk.c },
	{ TEST_PER_HS(0x26), &q6sw_clk },
	{ TEST_PER_HS(0x27), &q6fw_clk },
	{ TEST_PER_HS(0x2A), &adm0_clk.c },
	{ TEST_PER_HS(0x31), &sata_a_clk.c },
	{ TEST_PER_HS(0x2D), &pcie_phy_ref_clk.c },
	{ TEST_PER_HS(0x32), &pcie_a_clk.c },
	{ TEST_PER_HS(0x34), &ebi1_clk.c },
	{ TEST_PER_HS(0x34), &ebi1_a_clk.c },
	{ TEST_PER_HS(0x50), &usb_hsic_hsic_clk.c },

	{ TEST_MM_LS(0x00), &dsi1_byte_clk.c },
	{ TEST_MM_LS(0x01), &dsi2_byte_clk.c },
	{ TEST_MM_LS(0x02), &cam1_clk.c },
	{ TEST_MM_LS(0x06), &amp_p_clk.c },
	{ TEST_MM_LS(0x07), &csi_p_clk.c },
	{ TEST_MM_LS(0x08), &dsi2_s_p_clk.c },
	{ TEST_MM_LS(0x09), &dsi1_m_p_clk.c },
	{ TEST_MM_LS(0x0A), &dsi1_s_p_clk.c },
	{ TEST_MM_LS(0x0C), &gfx2d0_p_clk.c },
	{ TEST_MM_LS(0x0D), &gfx2d1_p_clk.c },
	{ TEST_MM_LS(0x0E), &gfx3d_p_clk.c },
	{ TEST_MM_LS(0x0F), &hdmi_m_p_clk.c },
	{ TEST_MM_LS(0x10), &hdmi_s_p_clk.c },
	{ TEST_MM_LS(0x11), &ijpeg_p_clk.c },
	{ TEST_MM_LS(0x12), &imem_p_clk.c },
	{ TEST_MM_LS(0x13), &jpegd_p_clk.c },
	{ TEST_MM_LS(0x14), &mdp_p_clk.c },
	{ TEST_MM_LS(0x16), &rot_p_clk.c },
	{ TEST_MM_LS(0x17), &dsi1_esc_clk.c },
	{ TEST_MM_LS(0x18), &smmu_p_clk.c },
	{ TEST_MM_LS(0x19), &tv_enc_p_clk.c },
	{ TEST_MM_LS(0x1A), &vcodec_p_clk.c },
	{ TEST_MM_LS(0x1B), &vfe_p_clk.c },
	{ TEST_MM_LS(0x1C), &vpe_p_clk.c },
	{ TEST_MM_LS(0x1D), &cam0_clk.c },
	{ TEST_MM_LS(0x1F), &hdmi_app_clk.c },
	{ TEST_MM_LS(0x20), &mdp_vsync_clk.c },
	{ TEST_MM_LS(0x21), &tv_dac_clk.c },
	{ TEST_MM_LS(0x22), &tv_enc_clk.c },
	{ TEST_MM_LS(0x23), &dsi2_esc_clk.c },
	{ TEST_MM_LS(0x25), &mmfpb_clk.c },
	{ TEST_MM_LS(0x25), &mmfpb_a_clk.c },
	{ TEST_MM_LS(0x26), &dsi2_m_p_clk.c },
	{ TEST_MM_LS(0x27), &cam2_clk.c },
	{ TEST_MM_LS(0x28), &vcap_p_clk.c },

	{ TEST_MM_HS(0x00), &csi0_clk.c },
	{ TEST_MM_HS(0x01), &csi1_clk.c },
	{ TEST_MM_HS(0x04), &csi_vfe_clk.c },
	{ TEST_MM_HS(0x05), &ijpeg_clk.c },
	{ TEST_MM_HS(0x06), &vfe_clk.c },
	{ TEST_MM_HS(0x07), &gfx2d0_clk.c },
	{ TEST_MM_HS(0x08), &gfx2d1_clk.c },
	{ TEST_MM_HS(0x09), &gfx3d_clk.c },
	{ TEST_MM_HS(0x0A), &jpegd_clk.c },
	{ TEST_MM_HS(0x0B), &vcodec_clk.c },
	{ TEST_MM_HS(0x0F), &mmfab_clk.c },
	{ TEST_MM_HS(0x0F), &mmfab_a_clk.c },
	{ TEST_MM_HS(0x11), &gmem_axi_clk.c },
	{ TEST_MM_HS(0x12), &ijpeg_axi_clk.c },
	{ TEST_MM_HS(0x13), &imem_axi_clk.c },
	{ TEST_MM_HS(0x14), &jpegd_axi_clk.c },
	{ TEST_MM_HS(0x15), &mdp_axi_clk.c },
	{ TEST_MM_HS(0x16), &rot_axi_clk.c },
	{ TEST_MM_HS(0x17), &vcodec_axi_clk.c },
	{ TEST_MM_HS(0x18), &vfe_axi_clk.c },
	{ TEST_MM_HS(0x19), &vpe_axi_clk.c },
	{ TEST_MM_HS(0x1A), &mdp_clk.c },
	{ TEST_MM_HS(0x1B), &rot_clk.c },
	{ TEST_MM_HS(0x1C), &vpe_clk.c },
	{ TEST_MM_HS(0x1E), &hdmi_tv_clk.c },
	{ TEST_MM_HS(0x1F), &mdp_tv_clk.c },
	{ TEST_MM_HS(0x24), &csi0_phy_clk.c },
	{ TEST_MM_HS(0x25), &csi1_phy_clk.c },
	{ TEST_MM_HS(0x26), &csi_pix_clk.c },
	{ TEST_MM_HS(0x27), &csi_rdi_clk.c },
	{ TEST_MM_HS(0x28), &lut_mdp_clk.c },
	{ TEST_MM_HS(0x29), &vcodec_axi_a_clk.c },
	{ TEST_MM_HS(0x2A), &vcodec_axi_b_clk.c },
	{ TEST_MM_HS(0x2B), &csi1phy_timer_clk.c },
	{ TEST_MM_HS(0x2C), &csi0phy_timer_clk.c },
	{ TEST_MM_HS(0x2D), &csi2_clk.c },
	{ TEST_MM_HS(0x2E), &csi2_phy_clk.c },
	{ TEST_MM_HS(0x2F), &csi2phy_timer_clk.c },
	{ TEST_MM_HS(0x30), &csi_pix1_clk.c },
	{ TEST_MM_HS(0x31), &csi_rdi1_clk.c },
	{ TEST_MM_HS(0x32), &csi_rdi2_clk.c },
	{ TEST_MM_HS(0x33), &vcap_clk.c },
	{ TEST_MM_HS(0x34), &vcap_npl_clk.c },
	{ TEST_MM_HS(0x34), &gfx3d_axi_clk_8930.c },
	{ TEST_MM_HS(0x35), &vcap_axi_clk.c },
	{ TEST_MM_HS(0x36), &rgb_tv_clk.c },
	{ TEST_MM_HS(0x37), &npl_tv_clk.c },
	{ TEST_MM_HS(0x38), &gfx3d_axi_clk.c },

	{ TEST_LPA(0x0F), &mi2s_bit_clk.c },
	{ TEST_LPA(0x10), &codec_i2s_mic_bit_clk.c },
	{ TEST_LPA(0x11), &codec_i2s_spkr_bit_clk.c },
	{ TEST_LPA(0x12), &spare_i2s_mic_bit_clk.c },
	{ TEST_LPA(0x13), &spare_i2s_spkr_bit_clk.c },
	{ TEST_LPA(0x14), &pcm_clk.c },
	{ TEST_LPA(0x1D), &audio_slimbus_clk.c },

	{ TEST_LPA_HS(0x00), &q6_func_clk },

	{ TEST_CPUL2(0x2), &l2_m_clk },
	{ TEST_CPUL2(0x0), &krait0_m_clk },
	{ TEST_CPUL2(0x1), &krait1_m_clk },
	{ TEST_CPUL2(0x4), &krait2_m_clk },
	{ TEST_CPUL2(0x5), &krait3_m_clk },
};

static struct measure_sel *find_measure_sel(struct clk *c)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(measure_mux); i++)
		if (measure_mux[i].c == c)
			return &measure_mux[i];
	return NULL;
}

static int measure_clk_set_parent(struct clk *c, struct clk *parent)
{
	int ret = 0;
	u32 clk_sel;
	struct measure_sel *p;
	struct measure_clk *measure = to_measure_clk(c);
	unsigned long flags;

	if (!parent)
		return -EINVAL;

	p = find_measure_sel(parent);
	if (!p)
		return -EINVAL;

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/*
	 * Program the test vector, measurement period (sample_ticks)
	 * and scaling multiplier.
	 */
	measure->sample_ticks = 0x10000;
	clk_sel = p->test_vector & TEST_CLK_SEL_MASK;
	measure->multiplier = 1;
	switch (p->test_vector >> TEST_TYPE_SHIFT) {
	case TEST_TYPE_PER_LS:
		writel_relaxed(0x4030D00|BVAL(7, 0, clk_sel), CLK_TEST_REG);
		break;
	case TEST_TYPE_PER_HS:
		writel_relaxed(0x4020000|BVAL(16, 10, clk_sel), CLK_TEST_REG);
		break;
	case TEST_TYPE_MM_LS:
		writel_relaxed(0x4030D97, CLK_TEST_REG);
		writel_relaxed(BVAL(6, 1, clk_sel)|BIT(0), DBG_CFG_REG_LS_REG);
		break;
	case TEST_TYPE_MM_HS:
		writel_relaxed(0x402B800, CLK_TEST_REG);
		writel_relaxed(BVAL(6, 1, clk_sel)|BIT(0), DBG_CFG_REG_HS_REG);
		break;
	case TEST_TYPE_LPA:
		writel_relaxed(0x4030D98, CLK_TEST_REG);
		writel_relaxed(BVAL(6, 1, clk_sel)|BIT(0),
				LCC_CLK_LS_DEBUG_CFG_REG);
		break;
	case TEST_TYPE_LPA_HS:
		writel_relaxed(0x402BC00, CLK_TEST_REG);
		writel_relaxed(BVAL(2, 1, clk_sel)|BIT(0),
				LCC_CLK_HS_DEBUG_CFG_REG);
		break;
	case TEST_TYPE_CPUL2:
		writel_relaxed(0x4030400, CLK_TEST_REG);
		writel_relaxed(0x80|BVAL(5, 3, clk_sel), GCC_APCS_CLK_DIAG);
		measure->sample_ticks = 0x4000;
		measure->multiplier = 2;
		if (cpu_is_krait_v3())
			measure->multiplier = 8;
		break;
	default:
		ret = -EPERM;
	}
	/* Make sure test vector is set before starting measurements. */
	mb();

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return ret;
}

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned ticks)
{
	/* Stop counters and set the XO4 counter start value. */
	writel_relaxed(ticks, RINGOSC_TCXO_CTL_REG);

	/* Wait for timer to become ready. */
	while ((readl_relaxed(RINGOSC_STATUS_REG) & BIT(25)) != 0)
		cpu_relax();

	/* Run measurement and wait for completion. */
	writel_relaxed(BIT(20)|ticks, RINGOSC_TCXO_CTL_REG);
	while ((readl_relaxed(RINGOSC_STATUS_REG) & BIT(25)) == 0)
		cpu_relax();

	/* Stop counters. */
	writel_relaxed(0x0, RINGOSC_TCXO_CTL_REG);

	/* Return measured ticks. */
	return readl_relaxed(RINGOSC_STATUS_REG) & BM(24, 0);
}


/* Perform a hardware rate measurement for a given clock.
   FOR DEBUG USE ONLY: Measurements take ~15 ms! */
static unsigned long measure_clk_get_rate(struct clk *c)
{
	unsigned long flags;
	u32 pdm_reg_backup, ringosc_reg_backup;
	u64 raw_count_short, raw_count_full;
	struct measure_clk *measure = to_measure_clk(c);
	unsigned ret;

	ret = clk_prepare_enable(&cxo_clk.c);
	if (ret) {
		pr_warning("CXO clock failed to enable. Can't measure\n");
		return 0;
	}

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Enable CXO/4 and RINGOSC branch and root. */
	pdm_reg_backup = readl_relaxed(PDM_CLK_NS_REG);
	ringosc_reg_backup = readl_relaxed(RINGOSC_NS_REG);
	writel_relaxed(0x2898, PDM_CLK_NS_REG);
	writel_relaxed(0xA00, RINGOSC_NS_REG);

	/*
	 * The ring oscillator counter will not reset if the measured clock
	 * is not running.  To detect this, run a short measurement before
	 * the full measurement.  If the raw results of the two are the same
	 * then the clock must be off.
	 */

	/* Run a short measurement. (~1 ms) */
	raw_count_short = run_measurement(0x1000);
	/* Run a full measurement. (~14 ms) */
	raw_count_full = run_measurement(measure->sample_ticks);

	writel_relaxed(ringosc_reg_backup, RINGOSC_NS_REG);
	writel_relaxed(pdm_reg_backup, PDM_CLK_NS_REG);

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short)
		ret = 0;
	else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((measure->sample_ticks * 10) + 35));
		ret = (raw_count_full * measure->multiplier);
	}

	/* Route dbg_hs_clk to PLLTEST.  300mV single-ended amplitude. */
	writel_relaxed(0x38F8, PLLTEST_PAD_CFG_REG);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	clk_disable_unprepare(&cxo_clk.c);

	return ret;
}
#else /* !CONFIG_DEBUG_FS */
static int measure_clk_set_parent(struct clk *c, struct clk *parent)
{
	return -EINVAL;
}

static unsigned long measure_clk_get_rate(struct clk *c)
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

static struct clk_lookup msm_clocks_8064[] = {
	CLK_LOOKUP("xo",		cxo_a_clk.c,	""),
	CLK_LOOKUP("xo",		pxo_a_clk.c,	""),
	CLK_LOOKUP("pwm_clk",		cxo_clk.c,	"0-0048"),
	CLK_LOOKUP("cxo",		cxo_clk.c,	"wcnss_wlan.0"),
	CLK_LOOKUP("cxo",		cxo_clk.c,	"pil_riva"),
	CLK_LOOKUP("xo",		pxo_clk.c,	"pil-q6v4-lpass"),
	CLK_LOOKUP("xo",		cxo_clk.c,	"pil_gss"),
	CLK_LOOKUP("xo",		cxo_clk.c,	"BAM_RMNT"),
	CLK_LOOKUP("xo",		cxo_clk.c,	"msm_xo"),
	CLK_LOOKUP("vref_buff",		cxo_clk.c,	"rpm-regulator"),
	CLK_LOOKUP("pll2",		pll2_clk.c,	NULL),
	CLK_LOOKUP("pll8",		pll8_clk.c,	NULL),
	CLK_LOOKUP("pll4",		pll4_clk.c,	NULL),
	CLK_LOOKUP("measure",		measure_clk.c,	"debug"),

	CLK_LOOKUP("bus_clk",		afab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		afab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		cfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		cfpb_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		dfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		dfab_a_clk.c,	""),
	CLK_LOOKUP("mem_clk",		ebi1_clk.c,	""),
	CLK_LOOKUP("mem_clk",		ebi1_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfpb_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfpb_a_clk.c,	""),

	CLK_LOOKUP("bus_clk",		afab_clk.c,		"msm_apps_fab"),
	CLK_LOOKUP("bus_a_clk",		afab_msmbus_a_clk.c,	"msm_apps_fab"),
	CLK_LOOKUP("bus_clk",		cfpb_clk.c,		"msm_cpss_fpb"),
	CLK_LOOKUP("bus_a_clk",		cfpb_a_clk.c,		"msm_cpss_fpb"),
	CLK_LOOKUP("bus_clk",		sfab_clk.c,		"msm_sys_fab"),
	CLK_LOOKUP("bus_a_clk",		sfab_msmbus_a_clk.c,	"msm_sys_fab"),
	CLK_LOOKUP("bus_clk",		sfpb_clk.c,		"msm_sys_fpb"),
	CLK_LOOKUP("bus_a_clk",		sfpb_a_clk.c,		"msm_sys_fpb"),
	CLK_LOOKUP("bus_clk",		mmfab_clk.c,		"msm_mm_fab"),
	CLK_LOOKUP("bus_a_clk",		mmfab_a_clk.c,		"msm_mm_fab"),
	CLK_LOOKUP("mem_clk",		ebi1_msmbus_clk.c,	"msm_bus"),
	CLK_LOOKUP("mem_a_clk",		ebi1_msmbus_a_clk.c,	"msm_bus"),
	CLK_LOOKUP("dfab_clk",		dfab_msmbus_clk.c,	"msm_bus"),
	CLK_LOOKUP("dfab_a_clk",	dfab_msmbus_a_clk.c,	"msm_bus"),
	CLK_LOOKUP("core_a_clk",	qdss_a_clk.c,		""),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-tpiu.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etb.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-funnel.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.1"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.2"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.3"),

	CLK_LOOKUP("ebi1_clk",		ebi1_clk.c,		""),
	CLK_LOOKUP("mmfpb_clk",		mmfpb_clk.c,		""),
	CLK_LOOKUP("mmfpb_a_clk",	mmfpb_a_clk.c,		"clock-8960"),
	CLK_LOOKUP("cfpb_a_clk",	cfpb_a_clk.c,		"clock-8960"),

	CLK_LOOKUP("core_clk",		gp0_clk.c,		""),
	CLK_LOOKUP("core_clk",		gp1_clk.c,		""),
	CLK_LOOKUP("core_clk",		gp2_clk.c,		""),
	CLK_LOOKUP("core_clk",		gsbi1_uart_clk.c, "msm_serial_hsl.1"),
	CLK_LOOKUP("core_clk",		gsbi2_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi2_uart_clk.c, "msm_serial_hsl.3"),
	CLK_LOOKUP("core_clk",		gsbi3_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi4_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi4_uart_clk.c, "msm_serial_hsl.4"),
	CLK_LOOKUP("core_clk",		gsbi5_uart_clk.c, "msm_serial_hsl.2"),
	CLK_LOOKUP("core_clk",		gsbi6_uart_clk.c, "msm_serial_hs.0"),
	CLK_LOOKUP("core_clk",		gsbi7_uart_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("core_clk",		gsbi1_qup_clk.c,	"qup_i2c.0"),
	CLK_LOOKUP("core_clk",		gsbi2_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi3_qup_clk.c,	"qup_i2c.3"),
	CLK_LOOKUP("core_clk",		gsbi4_qup_clk.c,	"qup_i2c.4"),
	CLK_LOOKUP("core_clk",		gsbi5_qup_clk.c,	"spi_qsd.0"),
	CLK_LOOKUP("core_clk",		gsbi5_qup_clk.c,	"qup_i2c.5"),
	CLK_LOOKUP("core_clk",		gsbi6_qup_clk.c,	"spi_qsd.1"),
	CLK_LOOKUP("core_clk",		gsbi7_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		pdm_clk.c,		""),
	CLK_LOOKUP("mem_clk",		pmem_clk.c,		"msm_sps"),
	CLK_LOOKUP("core_clk",          prng_clk.c,		"msm_rng.0"),
	CLK_LOOKUP("core_clk",		sdc1_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("core_clk",		sdc2_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("core_clk",		sdc3_clk.c,		"msm_sdcc.3"),
	CLK_LOOKUP("core_clk",		sdc4_clk.c,		"msm_sdcc.4"),
	CLK_LOOKUP("ref_clk",		tsif_ref_clk.c,		"msm_tsif.0"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,		"msm_tsif.0"),
	CLK_LOOKUP("ref_clk",		tsif_ref_clk.c,		"msm_tsif.1"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,		"msm_tsif.1"),
	CLK_LOOKUP("core_clk",		tssc_clk.c,		""),
	CLK_LOOKUP("alt_core_clk",	usb_hs1_xcvr_clk.c,	"msm_otg"),
	CLK_LOOKUP("alt_core_clk",      usb_hs3_xcvr_clk.c,  "msm_ehci_host.0"),
	CLK_LOOKUP("alt_core_clk",      usb_hs4_xcvr_clk.c,  "msm_ehci_host.1"),
	CLK_LOOKUP("src_clk",		usb_fs1_src_clk.c,	""),
	CLK_LOOKUP("alt_core_clk",	usb_fs1_xcvr_clk.c,	""),
	CLK_LOOKUP("sys_clk",		usb_fs1_sys_clk.c,	""),
	CLK_LOOKUP("ref_clk",		sata_phy_ref_clk.c,	"msm_sata.0"),
	CLK_LOOKUP("cfg_clk",		sata_phy_cfg_clk.c,	"msm_sata.0"),
	CLK_LOOKUP("src_clk",		sata_src_clk.c,		"msm_sata.0"),
	CLK_LOOKUP("core_rxoob_clk",	sata_rxoob_clk.c,	"msm_sata.0"),
	CLK_LOOKUP("core_pmalive_clk",	sata_pmalive_clk.c,	"msm_sata.0"),
	CLK_LOOKUP("bus_clk",		sata_a_clk.c,		"msm_sata.0"),
	CLK_LOOKUP("iface_clk",		sata_p_clk.c,		"msm_sata.0"),
	CLK_LOOKUP("slave_iface_clk",	sfab_sata_s_p_clk.c,	"msm_sata.0"),
	CLK_LOOKUP("iface_clk",		ce3_p_clk.c,		"qce.0"),
	CLK_LOOKUP("iface_clk",		ce3_p_clk.c,		"qcrypto.0"),
	CLK_LOOKUP("core_clk",		ce3_core_clk.c,		"qce.0"),
	CLK_LOOKUP("core_clk",		ce3_core_clk.c,		"qcrypto.0"),
	CLK_LOOKUP("ce3_core_src_clk",	ce3_src_clk.c,		"qce.0"),
	CLK_LOOKUP("ce3_core_src_clk",	ce3_src_clk.c,		"qcrypto.0"),
	CLK_LOOKUP("dma_bam_pclk",	dma_bam_p_clk.c,	NULL),
	CLK_LOOKUP("iface_clk",		gsbi1_p_clk.c,	"msm_serial_hsl.1"),
	CLK_LOOKUP("iface_clk",		gsbi1_p_clk.c,	"qup_i2c.0"),
	CLK_LOOKUP("iface_clk",		gsbi2_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi2_p_clk.c,	"msm_serial_hsl.3"),
	CLK_LOOKUP("iface_clk",		gsbi3_p_clk.c,		"qup_i2c.3"),
	CLK_LOOKUP("iface_clk",		gsbi4_p_clk.c,		"qup_i2c.4"),
	CLK_LOOKUP("iface_clk",		gsbi5_p_clk.c,	"msm_serial_hsl.2"),
	CLK_LOOKUP("iface_clk",		gsbi5_p_clk.c,		"spi_qsd.0"),
	CLK_LOOKUP("iface_clk",		gsbi5_p_clk.c,		"qup_i2c.5"),
	CLK_LOOKUP("iface_clk",		gsbi6_p_clk.c,	"msm_serial_hs.0"),
	CLK_LOOKUP("iface_clk",		gsbi6_p_clk.c,		"spi_qsd.1"),
	CLK_LOOKUP("iface_clk",		gsbi7_p_clk.c,	"msm_serial_hsl.0"),
	CLK_LOOKUP("iface_clk",		gsbi7_p_clk.c,	"msm_serial_hsl.4"),
	CLK_LOOKUP("ref_clk",	tsif_ref_clk.c,	"msm_tspp.0"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,		"msm_tspp.0"),
	CLK_LOOKUP("iface_clk",		usb_fs1_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		usb_hs1_p_clk.c,	"msm_otg"),
	CLK_LOOKUP("iface_clk",         usb_hs3_p_clk.c,     "msm_ehci_host.0"),
	CLK_LOOKUP("iface_clk",         usb_hs4_p_clk.c,     "msm_ehci_host.1"),
	CLK_LOOKUP("iface_clk",		sdc1_p_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",		sdc2_p_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("iface_clk",		sdc3_p_clk.c,		"msm_sdcc.3"),
	CLK_LOOKUP("iface_clk",		sdc4_p_clk.c,		"msm_sdcc.4"),
	CLK_LOOKUP("iface_clk",		pcie_p_clk.c,		"msm_pcie"),
	CLK_LOOKUP("ref_clk",		pcie_phy_ref_clk.c,	"msm_pcie"),
	CLK_LOOKUP("bus_clk",		pcie_a_clk.c,		"msm_pcie"),
	CLK_LOOKUP("core_clk",		adm0_clk.c,		"msm_dmov"),
	CLK_LOOKUP("iface_clk",		adm0_p_clk.c,		"msm_dmov"),
	CLK_LOOKUP("iface_clk",		pmic_arb0_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		pmic_arb1_p_clk.c,	""),
	CLK_LOOKUP("core_clk",		pmic_ssbi2_clk.c,	""),
	CLK_LOOKUP("mem_clk",		rpm_msg_ram_p_clk.c,	""),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-001a"),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-0010"),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-0034"),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-0020"),
	CLK_LOOKUP("cam_clk",		cam1_clk.c,	"4-0048"),
	CLK_LOOKUP("cam_clk",		cam1_clk.c,	"4-006c"),
	CLK_LOOKUP("cam_clk",		cam2_clk.c,		""),
	CLK_LOOKUP("csi_src_clk",	csi0_src_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_src_clk",	csi1_src_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_src_clk",	csi2_src_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("csi_clk",		csi0_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_clk",		csi1_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_clk",		csi2_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("csi_phy_clk",	csi0_phy_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_phy_clk",	csi1_phy_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_phy_clk",	csi2_phy_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("csiphy_timer_src_clk",
			   csiphy_timer_src_clk.c, "msm_csiphy.0"),
	CLK_LOOKUP("csiphy_timer_src_clk",
			   csiphy_timer_src_clk.c, "msm_csiphy.1"),
	CLK_LOOKUP("csiphy_timer_src_clk",
			   csiphy_timer_src_clk.c, "msm_csiphy.2"),
	CLK_LOOKUP("csiphy_timer_clk",	csi0phy_timer_clk.c,	"msm_csiphy.0"),
	CLK_LOOKUP("csiphy_timer_clk",	csi1phy_timer_clk.c,	"msm_csiphy.1"),
	CLK_LOOKUP("csiphy_timer_clk",	csi2phy_timer_clk.c,	"msm_csiphy.2"),
	CLK_LOOKUP("byte_clk",	dsi1_byte_clk.c,	"mipi_dsi.1"),
	CLK_LOOKUP("byte_clk",	dsi2_byte_clk.c,	"mipi_dsi.2"),
	CLK_LOOKUP("esc_clk",	dsi1_esc_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("esc_clk",	dsi2_esc_clk.c,		"mipi_dsi.2"),
	CLK_LOOKUP("rgb_clk",		rgb_tv_clk.c,		""),
	CLK_LOOKUP("npl_clk",		npl_tv_clk.c,		""),

	CLK_LOOKUP("core_clk",		gfx3d_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		gfx3d_clk.c,	"footswitch-8x60.11"),
	CLK_LOOKUP("bus_clk",
			    gfx3d_axi_clk.c, "footswitch-8x60.11"),
	CLK_LOOKUP("iface_clk",         vcap_p_clk.c,           ""),
	CLK_LOOKUP("iface_clk",         vcap_p_clk.c,           "msm_vcap.0"),
	CLK_LOOKUP("iface_clk",         vcap_p_clk.c,	"footswitch-8x60.10"),
	CLK_LOOKUP("bus_clk",		vcap_axi_clk.c,	"footswitch-8x60.10"),
	CLK_LOOKUP("core_clk",          vcap_clk.c,             ""),
	CLK_LOOKUP("core_clk",          vcap_clk.c,             "msm_vcap.0"),
	CLK_LOOKUP("core_clk",          vcap_clk.c,	"footswitch-8x60.10"),
	CLK_LOOKUP("vcap_npl_clk",      vcap_npl_clk.c,         ""),
	CLK_LOOKUP("vcap_npl_clk",      vcap_npl_clk.c,         "msm_vcap.0"),
	CLK_LOOKUP("bus_clk",		ijpeg_axi_clk.c, "footswitch-8x60.3"),
	CLK_LOOKUP("mem_clk",		imem_axi_clk.c,	"msm_gemini.0"),
	CLK_LOOKUP("core_clk",          ijpeg_clk.c,    "msm_gemini.0"),
	CLK_LOOKUP("core_clk",		ijpeg_clk.c,	"footswitch-8x60.3"),
	CLK_LOOKUP("core_clk",		jpegd_clk.c,		""),
	CLK_LOOKUP("core_clk",		mdp_clk.c,		"mdp.0"),
	CLK_LOOKUP("core_clk",		mdp_clk.c,	 "footswitch-8x60.4"),
	CLK_LOOKUP("vsync_clk",	mdp_vsync_clk.c,	"mdp.0"),
	CLK_LOOKUP("vsync_clk",		mdp_vsync_clk.c, "footswitch-8x60.4"),
	CLK_LOOKUP("lut_clk",		lut_mdp_clk.c,		"mdp.0"),
	CLK_LOOKUP("lut_clk",		lut_mdp_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("core_clk",		rot_clk.c,	"msm_rotator.0"),
	CLK_LOOKUP("core_clk",		rot_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("tv_src_clk",	tv_src_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("src_clk",	tv_src_clk.c,		"dtv.0"),
	CLK_LOOKUP("div_clk",	tv_src_div_clk.c,	""),
	CLK_LOOKUP("core_clk",		vcodec_clk.c,		"msm_vidc.0"),
	CLK_LOOKUP("core_clk",		vcodec_clk.c,	"footswitch-8x60.7"),
	CLK_LOOKUP("mdp_clk",	mdp_tv_clk.c,		"dtv.0"),
	CLK_LOOKUP("tv_clk",		mdp_tv_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("hdmi_clk",		hdmi_tv_clk.c,		"dtv.0"),
	CLK_LOOKUP("core_clk",		hdmi_app_clk.c,		"hdmi_msm.1"),
	CLK_LOOKUP("vpe_clk",		vpe_clk.c,		"msm_vpe.0"),
	CLK_LOOKUP("core_clk",		vpe_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,		"msm_vfe.0"),
	CLK_LOOKUP("core_clk",		vfe_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("csi_vfe_clk",	csi_vfe_clk.c,		"msm_vfe.0"),
	CLK_LOOKUP("bus_clk",		vfe_axi_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("bus_clk",		mdp_axi_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("bus_clk",		rot_axi_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("bus_clk",		vcodec_axi_clk.c,  "footswitch-8x60.7"),
	CLK_LOOKUP("bus_a_clk",        vcodec_axi_a_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("bus_b_clk",        vcodec_axi_b_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("bus_clk",		vpe_axi_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("arb_clk",		amp_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("arb_clk",		amp_p_clk.c,		"mipi_dsi.2"),
	CLK_LOOKUP("csi_pclk",          csi_p_clk.c,            "msm_csid.0"),
	CLK_LOOKUP("csi_pclk",          csi_p_clk.c,            "msm_csid.1"),
	CLK_LOOKUP("csi_pclk",          csi_p_clk.c,            "msm_csid.2"),
	CLK_LOOKUP("master_iface_clk",	dsi1_m_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("slave_iface_clk",	dsi1_s_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("master_iface_clk",	dsi2_m_p_clk.c,		"mipi_dsi.2"),
	CLK_LOOKUP("slave_iface_clk",	dsi2_s_p_clk.c,		"mipi_dsi.2"),
	CLK_LOOKUP("iface_clk",		gfx3d_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("iface_clk",		gfx3d_p_clk.c,	"footswitch-8x60.11"),
	CLK_LOOKUP("master_iface_clk",	hdmi_m_p_clk.c,		"hdmi_msm.1"),
	CLK_LOOKUP("slave_iface_clk",	hdmi_s_p_clk.c,		"hdmi_msm.1"),
	CLK_LOOKUP("iface_clk",		ijpeg_p_clk.c,		"msm_gemini.0"),
	CLK_LOOKUP("iface_clk",		ijpeg_p_clk.c,	"footswitch-8x60.3"),
	CLK_LOOKUP("iface_clk",		jpegd_p_clk.c,		""),
	CLK_LOOKUP("mem_iface_clk",	imem_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("iface_clk",		mdp_p_clk.c,		"mdp.0"),
	CLK_LOOKUP("iface_clk",		mdp_p_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.0"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.1"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.2"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.3"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.4"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.5"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.6"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.7"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.8"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.9"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.10"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.11"),
	CLK_LOOKUP("iface_clk",		rot_p_clk.c,	"msm_rotator.0"),
	CLK_LOOKUP("iface_clk",		rot_p_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("iface_clk",		vcodec_p_clk.c,		"msm_vidc.0"),
	CLK_LOOKUP("iface_clk",		vcodec_p_clk.c,	"footswitch-8x60.7"),
	CLK_LOOKUP("vfe_pclk",		vfe_p_clk.c,		"msm_vfe.0"),
	CLK_LOOKUP("iface_clk",		vfe_p_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("vpe_pclk",		vpe_p_clk.c,		"msm_vpe.0"),
	CLK_LOOKUP("iface_clk",		vpe_p_clk.c,	"footswitch-8x60.9"),

	CLK_LOOKUP("bit_clk",		mi2s_bit_clk.c,
			    "msm-dai-q6-mi2s"),
	CLK_LOOKUP("osr_clk",		mi2s_osr_clk.c,
			    "msm-dai-q6-mi2s"),
	CLK_LOOKUP("bit_clk",		codec_i2s_mic_bit_clk.c,
			   "msm-dai-q6.1"),
	CLK_LOOKUP("osr_clk",		codec_i2s_mic_osr_clk.c,
			   "msm-dai-q6.1"),
	CLK_LOOKUP("bit_clk",		spare_i2s_mic_bit_clk.c,
			   "msm-dai-q6.5"),
	CLK_LOOKUP("osr_clk",		spare_i2s_mic_osr_clk.c,
			   "msm-dai-q6.5"),
	CLK_LOOKUP("bit_clk",		codec_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.0"),
	CLK_LOOKUP("osr_clk",		codec_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.0"),
	CLK_LOOKUP("bit_clk",		codec_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.16384"),
	CLK_LOOKUP("osr_clk",		codec_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.16384"),
	CLK_LOOKUP("bit_clk",		spare_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.4"),
	CLK_LOOKUP("osr_clk",		spare_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.4"),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,		"msm-dai-q6.2"),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,		"msm-dai-q6.3"),
	CLK_LOOKUP("sps_slimbus_clk",	sps_slimbus_clk.c,	""),
	CLK_LOOKUP("core_clk",		audio_slimbus_clk.c, "msm_slim_ctrl.1"),
	CLK_LOOKUP("core_clk",		jpegd_axi_clk.c,	""),
	CLK_LOOKUP("core_clk",		vpe_axi_clk.c,		""),
	CLK_LOOKUP("core_clk",		mdp_axi_clk.c,		""),
	CLK_LOOKUP("core_clk",		vcap_axi_clk.c,		""),
	CLK_LOOKUP("core_clk",		rot_axi_clk.c,		""),
	CLK_LOOKUP("core_clk",		ijpeg_axi_clk.c,	""),
	CLK_LOOKUP("core_clk",		vfe_axi_clk.c,		""),
	CLK_LOOKUP("core_clk",		vcodec_axi_a_clk.c,	""),
	CLK_LOOKUP("core_clk",		vcodec_axi_b_clk.c,	""),
	CLK_LOOKUP("core_clk",		gfx3d_axi_clk.c,	""),

	CLK_LOOKUP("dfab_dsps_clk",	dfab_dsps_clk.c, NULL),
	CLK_LOOKUP("core_clk",		dfab_usb_hs_clk.c,	"msm_otg"),
	CLK_LOOKUP("core_clk",		dfab_usb_hs3_clk.c, "msm_ehci_host.0"),
	CLK_LOOKUP("core_clk",		dfab_usb_hs3_clk.c, "msm_ehci_host.1"),
	CLK_LOOKUP("bus_clk",		dfab_sdc1_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("bus_clk",		dfab_sdc2_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("bus_clk",		dfab_sdc3_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("bus_clk",		dfab_sdc4_clk.c, "msm_sdcc.4"),
	CLK_LOOKUP("dfab_clk",		dfab_sps_clk.c,	"msm_sps"),
	CLK_LOOKUP("bus_clk",		dfab_bam_dmux_clk.c,	"BAM_RMNT"),
	CLK_LOOKUP("bus_clk",		dfab_scm_clk.c,	"scm"),

	CLK_LOOKUP("alt_core_clk",    usb_hsic_xcvr_fs_clk.c,  "msm_hsic_host"),
	CLK_LOOKUP("phy_clk",	      usb_hsic_hsic_clk.c,     "msm_hsic_host"),
	CLK_LOOKUP("cal_clk",	      usb_hsic_hsio_cal_clk.c, "msm_hsic_host"),
	CLK_LOOKUP("core_clk",	      usb_hsic_system_clk.c,   "msm_hsic_host"),
	CLK_LOOKUP("iface_clk",	      usb_hsic_p_clk.c,        "msm_hsic_host"),

	CLK_LOOKUP("core_clk",		jpegd_axi_clk.c,    "msm_iommu-v0.0"),
	CLK_LOOKUP("core_clk",		vpe_axi_clk.c,	    "msm_iommu-v0.1"),
	CLK_LOOKUP("core_clk",		mdp_axi_clk.c,	    "msm_iommu-v0.2"),
	CLK_LOOKUP("core_clk",		mdp_axi_clk.c,	    "msm_iommu-v0.3"),
	CLK_LOOKUP("core_clk",		rot_axi_clk.c,	    "msm_iommu-v0.4"),
	CLK_LOOKUP("core_clk",		ijpeg_axi_clk.c,    "msm_iommu-v0.5"),
	CLK_LOOKUP("core_clk",		vfe_axi_clk.c,	    "msm_iommu-v0.6"),
	CLK_LOOKUP("core_clk",		vcodec_axi_a_clk.c, "msm_iommu-v0.7"),
	CLK_LOOKUP("core_clk",		vcodec_axi_b_clk.c, "msm_iommu-v0.8"),
	CLK_LOOKUP("core_clk",		gfx3d_axi_clk.c,    "msm_iommu-v0.9"),
	CLK_LOOKUP("core_clk",		gfx3d_axi_clk.c,    "msm_iommu-v0.10"),

	CLK_LOOKUP("core_clk",		vcap_axi_clk.c,	    "msm_iommu-v0.11"),

	CLK_LOOKUP("mdp_iommu_clk", mdp_axi_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("rot_iommu_clk",	rot_axi_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("vcodec_iommu0_clk", vcodec_axi_a_clk.c, "msm_vidc.0"),
	CLK_LOOKUP("vcodec_iommu1_clk", vcodec_axi_b_clk.c, "msm_vidc.0"),
	CLK_LOOKUP("smmu_iface_clk", smmu_p_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("core_clk",		vcodec_axi_clk.c,  "pil_vidc"),
	CLK_LOOKUP("smmu_iface_clk",	smmu_p_clk.c,  "pil_vidc"),

	CLK_LOOKUP("mem_clk",		ebi1_adm_clk.c, "msm_dmov"),
	CLK_LOOKUP("mem_clk",		ebi1_acpu_a_clk.c, ""),
	CLK_LOOKUP("bus_clk",		afab_acpu_a_clk.c, ""),

	CLK_LOOKUP("reset1_clk",	dsi1_reset_clk.c, "footswitch-8x60.4"),
	CLK_LOOKUP("reset2_clk",	dsi2_reset_clk.c, "footswitch-8x60.4"),

	CLK_LOOKUP("l2_mclk",		l2_m_clk,     ""),
	CLK_LOOKUP("krait0_mclk",	krait0_m_clk, ""),
	CLK_LOOKUP("krait1_mclk",	krait1_m_clk, ""),
	CLK_LOOKUP("krait2_mclk",	krait2_m_clk, ""),
	CLK_LOOKUP("krait3_mclk",	krait3_m_clk, ""),
};

static struct clk_lookup msm_clocks_8960_common[] __initdata = {
	CLK_LOOKUP("xo",		cxo_a_clk.c,	""),
	CLK_LOOKUP("xo",		pxo_a_clk.c,	""),
	CLK_LOOKUP("cxo",		cxo_clk.c,	"wcnss_wlan.0"),
	CLK_LOOKUP("cxo",		cxo_clk.c,	"pil_riva"),
	CLK_LOOKUP("xo",		pxo_clk.c,	"pil-q6v4-lpass"),
	CLK_LOOKUP("xo",		cxo_clk.c,	"pil-q6v4-modem"),
	CLK_LOOKUP("xo",		cxo_clk.c,	"BAM_RMNT"),
	CLK_LOOKUP("xo",		cxo_clk.c,	"msm_xo"),
	CLK_LOOKUP("vref_buff",		cxo_clk.c,	"rpm-regulator"),
	CLK_LOOKUP("pll2",		pll2_clk.c,	NULL),
	CLK_LOOKUP("pll8",		pll8_clk.c,	NULL),
	CLK_LOOKUP("pll4",		pll4_clk.c,	NULL),
	CLK_LOOKUP("measure",		measure_clk.c,	"debug"),

	CLK_LOOKUP("bus_clk",		afab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		afab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		cfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		cfpb_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		dfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		dfab_a_clk.c,	""),
	CLK_LOOKUP("mem_clk",		ebi1_clk.c,	""),
	CLK_LOOKUP("mem_clk",		ebi1_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfpb_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfpb_a_clk.c,	""),

	CLK_LOOKUP("bus_clk",		afab_clk.c,		"msm_apps_fab"),
	CLK_LOOKUP("bus_a_clk",		afab_msmbus_a_clk.c,	"msm_apps_fab"),
	CLK_LOOKUP("bus_clk",		cfpb_clk.c,		"msm_cpss_fpb"),
	CLK_LOOKUP("bus_a_clk",		cfpb_a_clk.c,		"msm_cpss_fpb"),
	CLK_LOOKUP("bus_clk",		sfab_clk.c,		"msm_sys_fab"),
	CLK_LOOKUP("bus_a_clk",		sfab_msmbus_a_clk.c,	"msm_sys_fab"),
	CLK_LOOKUP("bus_clk",		sfpb_clk.c,		"msm_sys_fpb"),
	CLK_LOOKUP("bus_a_clk",		sfpb_a_clk.c,		"msm_sys_fpb"),
	CLK_LOOKUP("bus_clk",		mmfab_clk.c,		"msm_mm_fab"),
	CLK_LOOKUP("bus_a_clk",		mmfab_a_clk.c,		"msm_mm_fab"),
	CLK_LOOKUP("mem_clk",		ebi1_msmbus_clk.c,	"msm_bus"),
	CLK_LOOKUP("mem_a_clk",		ebi1_msmbus_a_clk.c,	"msm_bus"),
	CLK_LOOKUP("dfab_clk",		dfab_msmbus_clk.c,	"msm_bus"),
	CLK_LOOKUP("dfab_a_clk",	dfab_msmbus_a_clk.c,	"msm_bus"),
	CLK_LOOKUP("core_a_clk",	qdss_a_clk.c,		""),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-tpiu.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etb.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-funnel.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.1"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.2"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.3"),

	CLK_LOOKUP("ebi1_clk",		ebi1_clk.c,		NULL),
	CLK_LOOKUP("mmfpb_clk",		mmfpb_clk.c,		NULL),
	CLK_LOOKUP("mmfpb_a_clk",	mmfpb_a_clk.c,		"clock-8960"),
	CLK_LOOKUP("cfpb_a_clk",	cfpb_a_clk.c,		"clock-8960"),

	CLK_LOOKUP("core_clk",		gp0_clk.c,		""),
	CLK_LOOKUP("core_clk",		gp1_clk.c,		""),
	CLK_LOOKUP("core_clk",		gp2_clk.c,		""),
	CLK_LOOKUP("core_clk",		gsbi1_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi2_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi3_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi4_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi5_uart_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("core_clk",		gsbi6_uart_clk.c, "msm_serial_hs.0"),
	CLK_LOOKUP("core_clk",		gsbi7_uart_clk.c,	""),
	/* used on 8960 SGLTE for console */
	CLK_LOOKUP("core_clk",		gsbi8_uart_clk.c, "msm_serial_hsl.1"),
	/* used on 8960 standalone with Atheros Bluetooth */
	CLK_LOOKUP("core_clk",		gsbi8_uart_clk.c, "msm_serial_hs.2"),
	CLK_LOOKUP("core_clk",		gsbi9_uart_clk.c, "msm_serial_hs.1"),
	CLK_LOOKUP("core_clk",		gsbi10_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi11_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi12_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi1_qup_clk.c,	"spi_qsd.0"),
	CLK_LOOKUP("core_clk",		gsbi2_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi3_qup_clk.c,	"qup_i2c.3"),
	CLK_LOOKUP("core_clk",		gsbi4_qup_clk.c,	"qup_i2c.4"),
	CLK_LOOKUP("core_clk",		gsbi5_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi6_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi7_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi8_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi9_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi10_qup_clk.c,	"qup_i2c.10"),
	CLK_LOOKUP("core_clk",		gsbi11_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi12_qup_clk.c,	"qup_i2c.12"),
	CLK_LOOKUP("core_clk",		pdm_clk.c,		""),
	CLK_LOOKUP("mem_clk",		pmem_clk.c,		"msm_sps"),
	CLK_LOOKUP("core_clk",		prng_clk.c,		"msm_rng.0"),
	CLK_LOOKUP("core_clk",		sdc1_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("core_clk",		sdc2_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("core_clk",		sdc3_clk.c,		"msm_sdcc.3"),
	CLK_LOOKUP("core_clk",		sdc4_clk.c,		"msm_sdcc.4"),
	CLK_LOOKUP("core_clk",		sdc5_clk.c,		"msm_sdcc.5"),
	CLK_LOOKUP("slimbus_xo_src_clk", slimbus_xo_src_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		tssc_clk.c,		""),
	CLK_LOOKUP("alt_core_clk",	usb_hs1_xcvr_clk.c,	"msm_otg"),
	CLK_LOOKUP("phy_clk",		usb_phy0_clk.c,		"msm_otg"),
	CLK_LOOKUP("alt_core_clk",	usb_fs1_xcvr_clk.c,	""),
	CLK_LOOKUP("sys_clk",		usb_fs1_sys_clk.c,	""),
	CLK_LOOKUP("src_clk",		usb_fs1_src_clk.c,	""),
	CLK_LOOKUP("alt_core_clk",	usb_fs2_xcvr_clk.c,	""),
	CLK_LOOKUP("sys_clk",		usb_fs2_sys_clk.c,	""),
	CLK_LOOKUP("src_clk",		usb_fs2_src_clk.c,	""),
	CLK_LOOKUP("alt_core_clk",    usb_hsic_xcvr_fs_clk.c,  "msm_hsic_host"),
	CLK_LOOKUP("phy_clk",	      usb_hsic_hsic_clk.c,     "msm_hsic_host"),
	CLK_LOOKUP("cal_clk",	      usb_hsic_hsio_cal_clk.c, "msm_hsic_host"),
	CLK_LOOKUP("core_clk",	      usb_hsic_system_clk.c,   "msm_hsic_host"),
	CLK_LOOKUP("iface_clk",	      usb_hsic_p_clk.c,        "msm_hsic_host"),
	CLK_LOOKUP("iface_clk",		ce1_p_clk.c,		"qce.0"),
	CLK_LOOKUP("iface_clk",		ce1_p_clk.c,		"qcrypto.0"),
	CLK_LOOKUP("core_clk",		ce1_core_clk.c,		"qce.0"),
	CLK_LOOKUP("core_clk",		ce1_core_clk.c,		"qcrypto.0"),
	CLK_LOOKUP("dma_bam_pclk",	dma_bam_p_clk.c,	NULL),
	CLK_LOOKUP("iface_clk",		gsbi1_p_clk.c,		"spi_qsd.0"),
	CLK_LOOKUP("iface_clk",		gsbi2_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi3_p_clk.c,		"qup_i2c.3"),
	CLK_LOOKUP("iface_clk",		gsbi4_p_clk.c,		"qup_i2c.4"),
	CLK_LOOKUP("iface_clk",		gsbi5_p_clk.c,	"msm_serial_hsl.0"),
	CLK_LOOKUP("iface_clk",		gsbi6_p_clk.c,  "msm_serial_hs.0"),
	CLK_LOOKUP("iface_clk",		gsbi7_p_clk.c,		""),
	/* used on 8960 SGLTE for serial console */
	CLK_LOOKUP("iface_clk",		gsbi8_p_clk.c,	"msm_serial_hsl.1"),
	/* used on 8960 standalone with Atheros Bluetooth */
	CLK_LOOKUP("iface_clk",		gsbi8_p_clk.c,	"msm_serial_hs.2"),
	CLK_LOOKUP("iface_clk",		gsbi9_p_clk.c,  "msm_serial_hs.1"),
	CLK_LOOKUP("iface_clk",		gsbi10_p_clk.c,		"qup_i2c.10"),
	CLK_LOOKUP("iface_clk",		gsbi11_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi12_p_clk.c,		"qup_i2c.12"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,		""),
	CLK_LOOKUP("ref_clk",	tsif_ref_clk.c,	"msm_tspp.0"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,		"msm_tspp.0"),
	CLK_LOOKUP("iface_clk",		usb_fs1_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		usb_fs2_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		usb_hs1_p_clk.c,	"msm_otg"),
	CLK_LOOKUP("iface_clk",		sdc1_p_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",		sdc2_p_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("iface_clk",		sdc3_p_clk.c,		"msm_sdcc.3"),
	CLK_LOOKUP("iface_clk",		sdc4_p_clk.c,		"msm_sdcc.4"),
	CLK_LOOKUP("iface_clk",		sdc5_p_clk.c,		"msm_sdcc.5"),
	CLK_LOOKUP("core_clk",		adm0_clk.c,		"msm_dmov"),
	CLK_LOOKUP("iface_clk",		adm0_p_clk.c,		"msm_dmov"),
	CLK_LOOKUP("iface_clk",		pmic_arb0_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		pmic_arb1_p_clk.c,	""),
	CLK_LOOKUP("core_clk",		pmic_ssbi2_clk.c,	""),
	CLK_LOOKUP("mem_clk",		rpm_msg_ram_p_clk.c,	""),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-001a"),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-006c"),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-0048"),
	CLK_LOOKUP("cam_clk",		cam2_clk.c,		NULL),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-0020"),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-0034"),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-0010"),
	CLK_LOOKUP("csi_src_clk",	csi0_src_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_src_clk",	csi1_src_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_src_clk",	csi2_src_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("csi_clk",		csi0_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_clk",		csi1_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_clk",		csi2_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("csi_phy_clk",	csi0_phy_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_phy_clk",	csi1_phy_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_phy_clk",	csi2_phy_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("csi_pix_clk",	csi_pix_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_rdi_clk",	csi_rdi_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_src_clk",	csi2_src_clk.c,		NULL),
	CLK_LOOKUP("csi_clk",		csi2_clk.c,		NULL),
	CLK_LOOKUP("csi_pix1_clk",	csi_pix1_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_rdi1_clk",	csi_rdi1_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_rdi2_clk",	csi_rdi2_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_phy_clk",	csi2_phy_clk.c,		NULL),
	CLK_LOOKUP("csi2phy_timer_clk",	csi2phy_timer_clk.c,	NULL),
	CLK_LOOKUP("csiphy_timer_src_clk",
			   csiphy_timer_src_clk.c, "msm_csiphy.0"),
	CLK_LOOKUP("csiphy_timer_src_clk",
			   csiphy_timer_src_clk.c, "msm_csiphy.1"),
	CLK_LOOKUP("csiphy_timer_src_clk",
			   csiphy_timer_src_clk.c, "msm_csiphy.2"),
	CLK_LOOKUP("csiphy_timer_clk",	csi0phy_timer_clk.c,	"msm_csiphy.0"),
	CLK_LOOKUP("csiphy_timer_clk",	csi1phy_timer_clk.c,	"msm_csiphy.1"),
	CLK_LOOKUP("csiphy_timer_clk",	csi2phy_timer_clk.c,	"msm_csiphy.2"),
	CLK_LOOKUP("byte_clk",	dsi1_byte_clk.c,	"mipi_dsi.1"),
	CLK_LOOKUP("byte_clk",	dsi2_byte_clk.c,	"mipi_dsi.2"),
	CLK_LOOKUP("esc_clk",	dsi1_esc_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("esc_clk",	dsi2_esc_clk.c,		"mipi_dsi.2"),
	CLK_LOOKUP("core_clk",		gfx3d_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		gfx3d_clk.c,	"footswitch-8x60.2"),
	CLK_LOOKUP("bus_clk",		ijpeg_axi_clk.c, "footswitch-8x60.3"),
	CLK_LOOKUP("mem_clk",		imem_axi_clk.c,	"msm_gemini.0"),
	CLK_LOOKUP("core_clk",          ijpeg_clk.c,    "msm_gemini.0"),
	CLK_LOOKUP("core_clk",		ijpeg_clk.c,	"footswitch-8x60.3"),
	CLK_LOOKUP("core_clk",		jpegd_clk.c, "msm_mercury.0"),
	CLK_LOOKUP("iface_clk",		jpegd_p_clk.c, "msm_mercury.0"),
	CLK_LOOKUP("core_clk",		mdp_clk.c,		"mdp.0"),
	CLK_LOOKUP("core_clk",		mdp_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("vsync_clk",	mdp_vsync_clk.c,	"mdp.0"),
	CLK_LOOKUP("vsync_clk",		mdp_vsync_clk.c, "footswitch-8x60.4"),
	CLK_LOOKUP("lut_clk",		lut_mdp_clk.c,		"mdp.0"),
	CLK_LOOKUP("lut_clk",		lut_mdp_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("core_clk",		rot_clk.c,	"msm_rotator.0"),
	CLK_LOOKUP("core_clk",		rot_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("src_clk",	tv_src_clk.c,		"dtv.0"),
	CLK_LOOKUP("src_clk",	tv_src_clk.c,		"tvenc.0"),
	CLK_LOOKUP("tv_src_clk",	tv_src_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("core_clk",		vcodec_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("core_clk",		vcodec_clk.c,	"footswitch-8x60.7"),
	CLK_LOOKUP("mdp_clk",	mdp_tv_clk.c,		"dtv.0"),
	CLK_LOOKUP("mdp_clk",	mdp_tv_clk.c,		"tvenc.0"),
	CLK_LOOKUP("tv_clk",		mdp_tv_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("hdmi_clk",		hdmi_tv_clk.c,		"dtv.0"),
	CLK_LOOKUP("core_clk",		hdmi_app_clk.c,	"hdmi_msm.1"),
	CLK_LOOKUP("vpe_clk",		vpe_clk.c,		"msm_vpe.0"),
	CLK_LOOKUP("core_clk",		vpe_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,		"msm_vfe.0"),
	CLK_LOOKUP("core_clk",		vfe_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("csi_vfe_clk",	csi_vfe_clk.c,		"msm_vfe.0"),
	CLK_LOOKUP("bus_clk",		vfe_axi_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("bus_clk",		mdp_axi_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("bus_clk",		rot_axi_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("bus_clk",		vcodec_axi_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("bus_a_clk",	       vcodec_axi_a_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("bus_b_clk",        vcodec_axi_b_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("bus_clk",		vpe_axi_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("arb_clk",		amp_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("arb_clk",		amp_p_clk.c,		"mipi_dsi.2"),
	CLK_LOOKUP("csi_pclk",		csi_p_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_pclk",		csi_p_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_pclk",		csi_p_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("master_iface_clk",	dsi1_m_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("slave_iface_clk",	dsi1_s_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("master_iface_clk",	dsi2_m_p_clk.c,		"mipi_dsi.2"),
	CLK_LOOKUP("slave_iface_clk",	dsi2_s_p_clk.c,		"mipi_dsi.2"),
	CLK_LOOKUP("iface_clk",		gfx3d_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("iface_clk",		gfx3d_p_clk.c,	"footswitch-8x60.2"),
	CLK_LOOKUP("master_iface_clk",	hdmi_m_p_clk.c,	"hdmi_msm.1"),
	CLK_LOOKUP("slave_iface_clk",	hdmi_s_p_clk.c,	"hdmi_msm.1"),
	CLK_LOOKUP("iface_clk",		ijpeg_p_clk.c,	"msm_gemini.0"),
	CLK_LOOKUP("iface_clk",		ijpeg_p_clk.c,	"footswitch-8x60.3"),
	CLK_LOOKUP("iface_clk",		jpegd_p_clk.c,		""),
	CLK_LOOKUP("mem_iface_clk",	imem_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("iface_clk",		mdp_p_clk.c,		"mdp.0"),
	CLK_LOOKUP("iface_clk",		mdp_p_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.0"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.1"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.2"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.3"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.4"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.5"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.6"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.7"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.8"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.9"),
	CLK_LOOKUP("iface_clk",		rot_p_clk.c,	"msm_rotator.0"),
	CLK_LOOKUP("iface_clk",		rot_p_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("iface_clk",		vcodec_p_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("iface_clk",		vcodec_p_clk.c,	"footswitch-8x60.7"),
	CLK_LOOKUP("vfe_pclk",		vfe_p_clk.c,		"msm_vfe.0"),
	CLK_LOOKUP("iface_clk",		vfe_p_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("vpe_pclk",		vpe_p_clk.c,		"msm_vpe.0"),
	CLK_LOOKUP("iface_clk",		vpe_p_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("bit_clk",		mi2s_bit_clk.c,
			    "msm-dai-q6-mi2s"),
	CLK_LOOKUP("osr_clk",		mi2s_osr_clk.c,
			    "msm-dai-q6-mi2s"),
	CLK_LOOKUP("bit_clk",		codec_i2s_mic_bit_clk.c,
			   "msm-dai-q6.1"),
	CLK_LOOKUP("osr_clk",		codec_i2s_mic_osr_clk.c,
			   "msm-dai-q6.1"),
	CLK_LOOKUP("bit_clk",		spare_i2s_mic_bit_clk.c,
			   "msm-dai-q6.5"),
	CLK_LOOKUP("osr_clk",		spare_i2s_mic_osr_clk.c,
			   "msm-dai-q6.5"),
	CLK_LOOKUP("bit_clk",		codec_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.0"),
	CLK_LOOKUP("osr_clk",		codec_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.0"),
	CLK_LOOKUP("bit_clk",		codec_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.16384"),
	CLK_LOOKUP("osr_clk",		codec_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.16384"),
	CLK_LOOKUP("bit_clk",		spare_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.4"),
	CLK_LOOKUP("osr_clk",		spare_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.4"),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,		"msm-dai-q6.2"),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,		"msm-dai-q6.3"),
	CLK_LOOKUP("sps_slimbus_clk",	sps_slimbus_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		audio_slimbus_clk.c, "msm_slim_ctrl.1"),
	CLK_LOOKUP("core_clk",		jpegd_axi_clk.c,    "msm_iommu-v0.0"),
	CLK_LOOKUP("core_clk",		vpe_axi_clk.c,	    "msm_iommu-v0.1"),
	CLK_LOOKUP("core_clk",		mdp_axi_clk.c,	    "msm_iommu-v0.2"),
	CLK_LOOKUP("core_clk",		mdp_axi_clk.c,	    "msm_iommu-v0.3"),
	CLK_LOOKUP("core_clk",		rot_axi_clk.c,	    "msm_iommu-v0.4"),
	CLK_LOOKUP("core_clk",		ijpeg_axi_clk.c,    "msm_iommu-v0.5"),
	CLK_LOOKUP("core_clk",		vfe_axi_clk.c,	    "msm_iommu-v0.6"),
	CLK_LOOKUP("core_clk",		vcodec_axi_a_clk.c, "msm_iommu-v0.7"),
	CLK_LOOKUP("core_clk",		vcodec_axi_b_clk.c, "msm_iommu-v0.8"),
	CLK_LOOKUP("core_clk",		gfx3d_clk.c,	    "msm_iommu-v0.9"),

	CLK_LOOKUP("mdp_iommu_clk", mdp_axi_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("rot_iommu_clk",	rot_axi_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("vcodec_iommu0_clk", vcodec_axi_a_clk.c, "msm_vidc.0"),
	CLK_LOOKUP("vcodec_iommu1_clk", vcodec_axi_b_clk.c, "msm_vidc.0"),
	CLK_LOOKUP("smmu_iface_clk", smmu_p_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("core_clk",		vcodec_axi_clk.c,  "pil_vidc"),
	CLK_LOOKUP("smmu_iface_clk",	smmu_p_clk.c,  "pil_vidc"),

	CLK_LOOKUP("dfab_dsps_clk",	dfab_dsps_clk.c, NULL),
	CLK_LOOKUP("core_clk",		dfab_usb_hs_clk.c,	"msm_otg"),
	CLK_LOOKUP("bus_clk",		dfab_sdc1_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("bus_clk",		dfab_sdc2_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("bus_clk",		dfab_sdc3_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("bus_clk",		dfab_sdc4_clk.c, "msm_sdcc.4"),
	CLK_LOOKUP("bus_clk",		dfab_sdc5_clk.c, "msm_sdcc.5"),
	CLK_LOOKUP("dfab_clk",		dfab_sps_clk.c,	"msm_sps"),
	CLK_LOOKUP("bus_clk",		dfab_bam_dmux_clk.c,	"BAM_RMNT"),
	CLK_LOOKUP("bus_clk",		dfab_scm_clk.c,	"scm"),

	CLK_LOOKUP("mem_clk",		ebi1_adm_clk.c, "msm_dmov"),
	CLK_LOOKUP("mem_clk",		ebi1_acpu_a_clk.c, ""),
	CLK_LOOKUP("bus_clk",		afab_acpu_a_clk.c, ""),

	CLK_LOOKUP("reset1_clk",	dsi1_reset_clk.c, "footswitch-8x60.4"),
	CLK_LOOKUP("reset2_clk",	dsi2_reset_clk.c, "footswitch-8x60.4"),

	CLK_LOOKUP("l2_mclk",		l2_m_clk,     ""),
	CLK_LOOKUP("krait0_mclk",	krait0_m_clk, ""),
	CLK_LOOKUP("krait1_mclk",	krait1_m_clk, ""),
	CLK_LOOKUP("q6sw_clk",		q6sw_clk,     ""),
	CLK_LOOKUP("q6fw_clk",		q6fw_clk,     ""),
	CLK_LOOKUP("q6_func_clk",	q6_func_clk,  ""),
};

static struct clk_lookup msm_clocks_8960_only[] __initdata = {
	CLK_LOOKUP("enc_clk",	tv_enc_clk.c,		"tvenc.0"),
	CLK_LOOKUP("dac_clk",	tv_dac_clk.c,		"tvenc.0"),
	CLK_LOOKUP("iface_clk",	tv_enc_p_clk.c,		"tvenc.0"),

	CLK_LOOKUP("core_clk",		gfx2d0_clk.c,	"kgsl-2d0.0"),
	CLK_LOOKUP("core_clk",		gfx2d0_clk.c,	"footswitch-8x60.0"),
	CLK_LOOKUP("core_clk",		gfx2d1_clk.c,	"kgsl-2d1.1"),
	CLK_LOOKUP("core_clk",		gfx2d1_clk.c,	"footswitch-8x60.1"),
	CLK_LOOKUP("iface_clk",		gfx2d0_p_clk.c,	"kgsl-2d0.0"),
	CLK_LOOKUP("iface_clk",		gfx2d0_p_clk.c,	"footswitch-8x60.0"),
	CLK_LOOKUP("iface_clk",		gfx2d1_p_clk.c,	"kgsl-2d1.1"),
	CLK_LOOKUP("iface_clk",		gfx2d1_p_clk.c,	"footswitch-8x60.1"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.10"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.11"),
	CLK_LOOKUP("core_clk",		gfx2d0_clk.c,	"msm_iommu-v0.10"),
	CLK_LOOKUP("core_clk",		gfx2d1_clk.c,	"msm_iommu-v0.11"),
};

static struct clk_lookup msm_clocks_8960ab_only[] __initdata = {
	CLK_LOOKUP("bus_clk", gfx3d_axi_clk.c, "footswitch-8x60.2"),
	CLK_LOOKUP("iface_clk",	smmu_p_clk.c,	"msm_iommu-v0.10"),
	CLK_LOOKUP("core_clk", gfx3d_axi_clk.c, "msm_iommu-v0.10"),
	CLK_LOOKUP("div_clk",	tv_src_div_clk.c,	""),
};

static struct clk_lookup msm_clocks_8960[ARRAY_SIZE(msm_clocks_8960_common)
	+ ARRAY_SIZE(msm_clocks_8960_only)
	+ ARRAY_SIZE(msm_clocks_8960ab_only)];

static struct clk_lookup msm_clocks_8930[] = {
	CLK_LOOKUP("xo",		cxo_clk.c,	"msm_xo"),
	CLK_LOOKUP("cxo",		cxo_clk.c,	"wcnss_wlan.0"),
	CLK_LOOKUP("cxo",		cxo_clk.c,	"pil_riva"),
	CLK_LOOKUP("xo",		pxo_clk.c,	"pil-q6v4-lpass"),
	CLK_LOOKUP("xo",		cxo_clk.c,	"pil-q6v4-modem"),
	CLK_LOOKUP("xo",		cxo_clk.c,	"BAM_RMNT"),
	CLK_LOOKUP("vref_buff",		cxo_clk.c,	"rpm-regulator"),
	CLK_LOOKUP("pll2",		pll2_clk.c,	NULL),
	CLK_LOOKUP("pll8",		pll8_clk.c,	NULL),
	CLK_LOOKUP("pll4",		pll4_clk.c,	NULL),
	CLK_LOOKUP("measure",		measure_clk.c,	"debug"),

	CLK_LOOKUP("bus_clk",		afab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		afab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		cfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		cfpb_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		dfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		dfab_a_clk.c,	""),
	CLK_LOOKUP("mem_clk",		ebi1_clk.c,	""),
	CLK_LOOKUP("mem_clk",		ebi1_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		mmfpb_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfpb_a_clk.c,	""),

	CLK_LOOKUP("bus_clk",		afab_clk.c,		"msm_apps_fab"),
	CLK_LOOKUP("bus_a_clk",		afab_msmbus_a_clk.c,	"msm_apps_fab"),
	CLK_LOOKUP("bus_clk",		cfpb_clk.c,		"msm_cpss_fpb"),
	CLK_LOOKUP("bus_a_clk",		cfpb_a_clk.c,		"msm_cpss_fpb"),
	CLK_LOOKUP("bus_clk",		sfab_clk.c,		"msm_sys_fab"),
	CLK_LOOKUP("bus_a_clk",		sfab_msmbus_a_clk.c,	"msm_sys_fab"),
	CLK_LOOKUP("bus_clk",		sfpb_clk.c,		"msm_sys_fpb"),
	CLK_LOOKUP("bus_a_clk",		sfpb_a_clk.c,		"msm_sys_fpb"),
	CLK_LOOKUP("bus_clk",		mmfab_clk.c,		"msm_mm_fab"),
	CLK_LOOKUP("bus_a_clk",		mmfab_a_clk.c,		"msm_mm_fab"),
	CLK_LOOKUP("mem_clk",		ebi1_msmbus_clk.c,	"msm_bus"),
	CLK_LOOKUP("mem_a_clk",		ebi1_msmbus_a_clk.c,	"msm_bus"),
	CLK_LOOKUP("dfab_clk",		dfab_msmbus_clk.c,	"msm_bus"),
	CLK_LOOKUP("dfab_a_clk",	dfab_msmbus_a_clk.c,	"msm_bus"),
	CLK_LOOKUP("core_a_clk",	qdss_a_clk.c,		""),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-tpiu.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etb.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-funnel.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.0"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.1"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.2"),
	CLK_LOOKUP("core_clk",		qdss_clk.c,	"coresight-etm.3"),

	CLK_LOOKUP("ebi1_clk",		ebi1_clk.c,		NULL),
	CLK_LOOKUP("mmfpb_clk",		mmfpb_clk.c,		NULL),
	CLK_LOOKUP("mmfpb_a_clk",	mmfpb_a_clk.c,		"clock-8960"),
	CLK_LOOKUP("cfpb_a_clk",	cfpb_a_clk.c,		"clock-8960"),

	CLK_LOOKUP("core_clk",		gp0_clk.c,		""),
	CLK_LOOKUP("core_clk",		gp1_clk.c,		""),
	CLK_LOOKUP("core_clk",		gp2_clk.c,		""),
	CLK_LOOKUP("core_clk",		gsbi1_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi2_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi3_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi4_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi5_uart_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("core_clk",		gsbi6_uart_clk.c, "msm_serial_hs.0"),
	CLK_LOOKUP("core_clk",		gsbi7_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi8_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi9_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi10_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi11_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi12_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi1_qup_clk.c,	"spi_qsd.0"),
	CLK_LOOKUP("core_clk",		gsbi2_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi3_qup_clk.c,	"qup_i2c.3"),
	CLK_LOOKUP("core_clk",		gsbi4_qup_clk.c,	"qup_i2c.4"),
	CLK_LOOKUP("core_clk",		gsbi5_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi6_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi7_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi8_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi9_qup_clk.c,	"qup_i2c.0"),
	CLK_LOOKUP("core_clk",		gsbi10_qup_clk.c,	"qup_i2c.10"),
	CLK_LOOKUP("core_clk",		gsbi11_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi12_qup_clk.c,	"qup_i2c.12"),
	CLK_LOOKUP("core_clk",		pdm_clk.c,		""),
	CLK_LOOKUP("mem_clk",		pmem_clk.c,		"msm_sps"),
	CLK_LOOKUP("core_clk",		prng_clk.c,		"msm_rng.0"),
	CLK_LOOKUP("core_clk",		sdc1_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("core_clk",		sdc2_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("core_clk",		sdc3_clk.c,		"msm_sdcc.3"),
	CLK_LOOKUP("core_clk",		sdc4_clk.c,		"msm_sdcc.4"),
	CLK_LOOKUP("core_clk",		sdc5_clk.c,		"msm_sdcc.5"),
	CLK_LOOKUP("ref_clk",		tsif_ref_clk.c,		""),
	CLK_LOOKUP("core_clk",		tssc_clk.c,		""),
	CLK_LOOKUP("alt_core_clk",	usb_hs1_xcvr_clk.c,	"msm_otg"),
	CLK_LOOKUP("phy_clk",		usb_phy0_clk.c,		"msm_otg"),
	CLK_LOOKUP("alt_core_clk",	usb_fs1_xcvr_clk.c,	""),
	CLK_LOOKUP("sys_clk",		usb_fs1_sys_clk.c,	""),
	CLK_LOOKUP("src_clk",		usb_fs1_src_clk.c,	""),
	CLK_LOOKUP("alt_core_clk",	usb_fs2_xcvr_clk.c,	""),
	CLK_LOOKUP("sys_clk",		usb_fs2_sys_clk.c,	""),
	CLK_LOOKUP("src_clk",		usb_fs2_src_clk.c,	""),
	CLK_LOOKUP("alt_core_clk",    usb_hsic_xcvr_fs_clk.c,  "msm_hsic_host"),
	CLK_LOOKUP("phy_clk",	      usb_hsic_hsic_clk.c,     "msm_hsic_host"),
	CLK_LOOKUP("cal_clk",	      usb_hsic_hsio_cal_clk.c, "msm_hsic_host"),
	CLK_LOOKUP("core_clk",	      usb_hsic_system_clk.c,   "msm_hsic_host"),
	CLK_LOOKUP("iface_clk",	      usb_hsic_p_clk.c,        "msm_hsic_host"),
	CLK_LOOKUP("iface_clk",		ce1_p_clk.c,		"qce.0"),
	CLK_LOOKUP("iface_clk",		ce1_p_clk.c,		"qcrypto.0"),
	CLK_LOOKUP("core_clk",		ce1_core_clk.c,		"qce.0"),
	CLK_LOOKUP("core_clk",		ce1_core_clk.c,		"qcrypto.0"),
	CLK_LOOKUP("dma_bam_pclk",	dma_bam_p_clk.c,	NULL),
	CLK_LOOKUP("iface_clk",		gsbi1_p_clk.c,		"spi_qsd.0"),
	CLK_LOOKUP("iface_clk",		gsbi2_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi3_p_clk.c,		"qup_i2c.3"),
	CLK_LOOKUP("iface_clk",		gsbi4_p_clk.c,		"qup_i2c.4"),
	CLK_LOOKUP("iface_clk",		gsbi5_p_clk.c,	"msm_serial_hsl.0"),
	CLK_LOOKUP("iface_clk",		gsbi6_p_clk.c,  "msm_serial_hs.0"),
	CLK_LOOKUP("iface_clk",		gsbi7_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi8_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi9_p_clk.c,		"qup_i2c.0"),
	CLK_LOOKUP("iface_clk",		gsbi10_p_clk.c,		"qup_i2c.10"),
	CLK_LOOKUP("iface_clk",		gsbi11_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi12_p_clk.c,		"qup_i2c.12"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		usb_fs1_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		usb_fs2_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		usb_hs1_p_clk.c,	"msm_otg"),
	CLK_LOOKUP("iface_clk",		sdc1_p_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",		sdc2_p_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("iface_clk",		sdc3_p_clk.c,		"msm_sdcc.3"),
	CLK_LOOKUP("iface_clk",		sdc4_p_clk.c,		"msm_sdcc.4"),
	CLK_LOOKUP("iface_clk",		sdc5_p_clk.c,		"msm_sdcc.5"),
	CLK_LOOKUP("core_clk",		adm0_clk.c,		"msm_dmov"),
	CLK_LOOKUP("iface_clk",		adm0_p_clk.c,		"msm_dmov"),
	CLK_LOOKUP("iface_clk",		pmic_arb0_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		pmic_arb1_p_clk.c,	""),
	CLK_LOOKUP("core_clk",		pmic_ssbi2_clk.c,	""),
	CLK_LOOKUP("mem_clk",		rpm_msg_ram_p_clk.c,	""),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-001a"),
	CLK_LOOKUP("cam_clk",		cam1_clk.c,	"4-006c"),
	CLK_LOOKUP("cam_clk",		cam1_clk.c,	"4-0048"),
	CLK_LOOKUP("cam_clk",		cam2_clk.c,		NULL),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"4-0020"),
	CLK_LOOKUP("csi_src_clk",	csi0_src_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_src_clk",	csi1_src_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_src_clk",	csi2_src_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("csi_clk",		csi0_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_clk",		csi1_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_clk",		csi2_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("csi_phy_clk",	csi0_phy_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_phy_clk",	csi1_phy_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_phy_clk",	csi2_phy_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("csi_pix_clk",	csi_pix_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_rdi_clk",	csi_rdi_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_src_clk",	csi2_src_clk.c,		NULL),
	CLK_LOOKUP("csi_clk",		csi2_clk.c,		NULL),
	CLK_LOOKUP("csi_pix1_clk",	csi_pix1_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_rdi1_clk",	csi_rdi1_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_rdi2_clk",	csi_rdi2_clk.c,		"msm_ispif.0"),
	CLK_LOOKUP("csi_phy_clk",	csi2_phy_clk.c,		NULL),
	CLK_LOOKUP("csi2phy_timer_clk",	csi2phy_timer_clk.c,	NULL),
	CLK_LOOKUP("csiphy_timer_src_clk",
			   csiphy_timer_src_clk.c, "msm_csiphy.0"),
	CLK_LOOKUP("csiphy_timer_src_clk",
			   csiphy_timer_src_clk.c, "msm_csiphy.1"),
	CLK_LOOKUP("csiphy_timer_src_clk",
			   csiphy_timer_src_clk.c, "msm_csiphy.2"),
	CLK_LOOKUP("csiphy_timer_clk",	csi0phy_timer_clk.c,	"msm_csiphy.0"),
	CLK_LOOKUP("csiphy_timer_clk",	csi1phy_timer_clk.c,	"msm_csiphy.1"),
	CLK_LOOKUP("csiphy_timer_clk",	csi2phy_timer_clk.c,	"msm_csiphy.2"),
	CLK_LOOKUP("byte_clk",	dsi1_byte_clk.c,	"mipi_dsi.1"),
	CLK_LOOKUP("esc_clk",	dsi1_esc_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("core_clk",		gfx3d_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		gfx3d_clk.c,	"footswitch-8x60.2"),
	CLK_LOOKUP("bus_clk",
			    gfx3d_axi_clk_8930.c, "footswitch-8x60.2"),
	CLK_LOOKUP("bus_clk",		ijpeg_axi_clk.c, "footswitch-8x60.3"),
	CLK_LOOKUP("mem_clk",		imem_axi_clk.c, "msm_gemini.0"),
	CLK_LOOKUP("core_clk",          ijpeg_clk.c,    "msm_gemini.0"),
	CLK_LOOKUP("core_clk",		ijpeg_clk.c,	"footswitch-8x60.3"),
	CLK_LOOKUP("core_clk",		mdp_clk.c,		"mdp.0"),
	CLK_LOOKUP("core_clk",		mdp_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("vsync_clk",	mdp_vsync_clk.c,	"mdp.0"),
	CLK_LOOKUP("vsync_clk",		mdp_vsync_clk.c, "footswitch-8x60.4"),
	CLK_LOOKUP("lut_clk",		lut_mdp_clk.c,		"mdp.0"),
	CLK_LOOKUP("lut_clk",		lut_mdp_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("core_clk",		rot_clk.c,	"msm_rotator.0"),
	CLK_LOOKUP("core_clk",		rot_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("src_clk",	tv_src_clk.c,		"dtv.0"),
	CLK_LOOKUP("src_clk",	tv_src_clk.c,		"tvenc.0"),
	CLK_LOOKUP("tv_src_clk",	tv_src_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("dac_clk",	tv_dac_clk.c,		"tvenc.0"),
	CLK_LOOKUP("core_clk",		vcodec_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("core_clk",		vcodec_clk.c,	"footswitch-8x60.7"),
	CLK_LOOKUP("mdp_clk",	mdp_tv_clk.c,		"dtv.0"),
	CLK_LOOKUP("mdp_clk",	mdp_tv_clk.c,		"tvenc.0"),
	CLK_LOOKUP("tv_clk",		mdp_tv_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("hdmi_clk",		hdmi_tv_clk.c,		"dtv.0"),
	CLK_LOOKUP("core_clk",		hdmi_app_clk.c,	"hdmi_msm.1"),
	CLK_LOOKUP("vpe_clk",		vpe_clk.c,		"msm_vpe.0"),
	CLK_LOOKUP("core_clk",		vpe_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,		"msm_vfe.0"),
	CLK_LOOKUP("core_clk",		vfe_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("csi_vfe_clk",	csi_vfe_clk.c,		"msm_vfe.0"),
	CLK_LOOKUP("bus_clk",		vfe_axi_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("bus_clk",		mdp_axi_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("bus_clk",		rot_axi_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("bus_clk",		vcodec_axi_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("bus_a_clk",	       vcodec_axi_a_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("bus_b_clk",        vcodec_axi_b_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("bus_clk",		vpe_axi_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("arb_clk",		amp_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("csi_pclk",		csi_p_clk.c,		"msm_csid.0"),
	CLK_LOOKUP("csi_pclk",		csi_p_clk.c,		"msm_csid.1"),
	CLK_LOOKUP("csi_pclk",		csi_p_clk.c,		"msm_csid.2"),
	CLK_LOOKUP("master_iface_clk",	dsi1_m_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("slave_iface_clk",	dsi1_s_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("iface_clk",		gfx3d_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("iface_clk",		gfx3d_p_clk.c,	"footswitch-8x60.2"),
	CLK_LOOKUP("master_iface_clk",	hdmi_m_p_clk.c,	"hdmi_msm.1"),
	CLK_LOOKUP("slave_iface_clk",	hdmi_s_p_clk.c,	"hdmi_msm.1"),
	CLK_LOOKUP("iface_clk",		ijpeg_p_clk.c,	"msm_gemini.0"),
	CLK_LOOKUP("iface_clk",		ijpeg_p_clk.c,	"footswitch-8x60.3"),
	CLK_LOOKUP("mem_iface_clk",	imem_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("iface_clk",		mdp_p_clk.c,		"mdp.0"),
	CLK_LOOKUP("iface_clk",		mdp_p_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.0"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.1"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.2"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.3"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.4"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.5"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.6"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.7"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.8"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.9"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu-v0.10"),
	CLK_LOOKUP("iface_clk",		rot_p_clk.c,	"msm_rotator.0"),
	CLK_LOOKUP("iface_clk",		rot_p_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("iface_clk",		vcodec_p_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("iface_clk",		vcodec_p_clk.c,	"footswitch-8x60.7"),
	CLK_LOOKUP("vfe_pclk",		vfe_p_clk.c,		"msm_vfe.0"),
	CLK_LOOKUP("iface_clk",		vfe_p_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("vpe_pclk",		vpe_p_clk.c,		"msm_vpe.0"),
	CLK_LOOKUP("iface_clk",		vpe_p_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("bit_clk",		mi2s_bit_clk.c,		"msm-dai-q6.6"),
	CLK_LOOKUP("osr_clk",		mi2s_osr_clk.c,		"msm-dai-q6.6"),
	CLK_LOOKUP("bit_clk",		codec_i2s_mic_bit_clk.c,
			   "msm-dai-q6.1"),
	CLK_LOOKUP("osr_clk",		codec_i2s_mic_osr_clk.c,
			   "msm-dai-q6.1"),
	CLK_LOOKUP("bit_clk",		spare_i2s_mic_bit_clk.c,
			   "msm-dai-q6.5"),
	CLK_LOOKUP("osr_clk",		spare_i2s_mic_osr_clk.c,
			   "msm-dai-q6.5"),
	CLK_LOOKUP("bit_clk",		codec_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.0"),
	CLK_LOOKUP("osr_clk",		codec_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.0"),
	CLK_LOOKUP("bit_clk",		codec_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.16384"),
	CLK_LOOKUP("osr_clk",		codec_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.16384"),
	CLK_LOOKUP("bit_clk",		spare_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.4"),
	CLK_LOOKUP("osr_clk",		spare_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.4"),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,		"msm-dai-q6.2"),
	CLK_LOOKUP("sps_slimbus_clk",	sps_slimbus_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		audio_slimbus_clk.c, "msm_slim_ctrl.1"),
	CLK_LOOKUP("core_clk",		vpe_axi_clk.c,	  "msm_iommu-v0.1"),
	CLK_LOOKUP("core_clk",		mdp_axi_clk.c,	  "msm_iommu-v0.2"),
	CLK_LOOKUP("core_clk",		mdp_axi_clk.c,	  "msm_iommu-v0.3"),
	CLK_LOOKUP("core_clk",		rot_axi_clk.c,	  "msm_iommu-v0.4"),
	CLK_LOOKUP("core_clk",		ijpeg_axi_clk.c,  "msm_iommu-v0.5"),
	CLK_LOOKUP("core_clk",		vfe_axi_clk.c,	  "msm_iommu-v0.6"),
	CLK_LOOKUP("core_clk",	   vcodec_axi_a_clk.c,    "msm_iommu-v0.7"),
	CLK_LOOKUP("core_clk",	   vcodec_axi_b_clk.c,    "msm_iommu-v0.8"),
	CLK_LOOKUP("core_clk",	   gfx3d_axi_clk_8930.c,  "msm_iommu-v0.9"),
	CLK_LOOKUP("core_clk",	   gfx3d_axi_clk_8930.c,  "msm_iommu-v0.10"),

	CLK_LOOKUP("mdp_iommu_clk", mdp_axi_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("rot_iommu_clk",	rot_axi_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("vcodec_iommu0_clk", vcodec_axi_a_clk.c, "msm_vidc.0"),
	CLK_LOOKUP("vcodec_iommu1_clk", vcodec_axi_b_clk.c, "msm_vidc.0"),
	CLK_LOOKUP("smmu_iface_clk", smmu_p_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("core_clk",		vcodec_axi_clk.c,  "pil_vidc"),
	CLK_LOOKUP("smmu_iface_clk",	smmu_p_clk.c,  "pil_vidc"),

	CLK_LOOKUP("dfab_dsps_clk",	dfab_dsps_clk.c, NULL),
	CLK_LOOKUP("core_clk",		dfab_usb_hs_clk.c,	"msm_otg"),
	CLK_LOOKUP("bus_clk",		dfab_sdc1_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("bus_clk",		dfab_sdc2_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("bus_clk",		dfab_sdc3_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("bus_clk",		dfab_sdc4_clk.c, "msm_sdcc.4"),
	CLK_LOOKUP("bus_clk",		dfab_sdc5_clk.c, "msm_sdcc.5"),
	CLK_LOOKUP("dfab_clk",		dfab_sps_clk.c,	"msm_sps"),
	CLK_LOOKUP("bus_clk",		dfab_bam_dmux_clk.c,	"BAM_RMNT"),
	CLK_LOOKUP("bus_clk",		dfab_scm_clk.c,	"scm"),

	CLK_LOOKUP("mem_clk",		ebi1_adm_clk.c, "msm_dmov"),
	CLK_LOOKUP("mem_clk",		ebi1_acpu_a_clk.c, ""),
	CLK_LOOKUP("bus_clk",		afab_acpu_a_clk.c, ""),

	CLK_LOOKUP("reset1_clk",	dsi1_reset_clk.c, "footswitch-8x60.4"),

	CLK_LOOKUP("l2_mclk",		l2_m_clk,     ""),
	CLK_LOOKUP("krait0_mclk",	krait0_m_clk, ""),
	CLK_LOOKUP("krait1_mclk",	krait1_m_clk, ""),
	CLK_LOOKUP("q6sw_clk",		q6sw_clk,     ""),
	CLK_LOOKUP("q6fw_clk",		q6fw_clk,     ""),
	CLK_LOOKUP("q6_func_clk",	q6_func_clk,  ""),
};
/*
 * Miscellaneous clock register initializations
 */

/* Read, modify, then write-back a register. */
static void __init rmwreg(uint32_t val, void *reg, uint32_t mask)
{
	uint32_t regval = readl_relaxed(reg);
	regval &= ~mask;
	regval |= val;
	writel_relaxed(regval, reg);
}

static struct pll_config_regs pll3_regs __initdata = {
	.l_reg = BB_MMCC_PLL2_L_REG,
	.m_reg = BB_MMCC_PLL2_M_REG,
	.n_reg = BB_MMCC_PLL2_N_REG,
	.config_reg = BB_MMCC_PLL2_CONFIG_REG,
	.mode_reg = BB_MMCC_PLL2_MODE_REG,
};

/* Program PLL3 to 880MHZ */
static struct pll_config pll3_config __initdata = {
	.l = (32 | BVAL(31, 7, 0x8)),
	.m = 16,
	.n = 27,
	.vco_val = 0x0,
	.vco_mask = BM(8, 7),
	.pre_div_val = 0x0,
	.pre_div_mask = BIT(15),
	.post_div_val = 0x0,
	.post_div_mask = BIT(16),
	.mn_ena_val = 0,
	.mn_ena_mask = 0,
	.main_output_val = 0,
	.main_output_mask = 0,
};

static struct pll_config_regs pll4_regs __initdata = {
	.l_reg = LCC_PLL0_L_VAL_REG,
	.m_reg = LCC_PLL0_M_VAL_REG,
	.n_reg = LCC_PLL0_N_VAL_REG,
	.config_reg = LCC_PLL0_CONFIG_REG,
	.mode_reg = LCC_PLL0_MODE_REG,
};

static struct pll_config pll4_config_393 __initdata = {
	.l = 0xE,
	.m = 0x27A,
	.n = 0x465,
	.vco_val = 0x0,
	.vco_mask = BM(17, 16),
	.pre_div_val = 0x0,
	.pre_div_mask = BIT(19),
	.post_div_val = 0x0,
	.post_div_mask = BM(21, 20),
	.mn_ena_val = BIT(22),
	.mn_ena_mask = BIT(22),
	.main_output_val = BIT(23),
	.main_output_mask = BIT(23),
};

static struct pll_config_regs pll15_regs __initdata = {
	.l_reg = MM_PLL3_L_VAL_REG,
	.m_reg = MM_PLL3_M_VAL_REG,
	.n_reg = MM_PLL3_N_VAL_REG,
	.config_reg = MM_PLL3_CONFIG_REG,
	.mode_reg = MM_PLL3_MODE_REG,
};

static struct pll_config pll15_config __initdata = {
	.l = (0x24 | BVAL(31, 7, 0x620)),
	.m = 0x1,
	.n = 0x9,
	.vco_val = BVAL(17, 16, 0x2),
	.vco_mask = BM(17, 16),
	.pre_div_val = 0x0,
	.pre_div_mask = BIT(19),
	.post_div_val = 0x0,
	.post_div_mask = BM(21, 20),
	.mn_ena_val = BIT(22),
	.mn_ena_mask = BIT(22),
	.main_output_val = BIT(23),
	.main_output_mask = BIT(23),
};

static struct pll_config_regs pll14_regs __initdata = {
	.l_reg = BB_PLL14_L_VAL_REG,
	.m_reg = BB_PLL14_M_VAL_REG,
	.n_reg = BB_PLL14_N_VAL_REG,
	.config_reg = BB_PLL14_CONFIG_REG,
	.mode_reg = BB_PLL14_MODE_REG,
};

static struct pll_config pll14_config __initdata = {
	.l = (0x11 | BVAL(31, 7, 0x620)),
	.m = 0x7,
	.n = 0x9,
	.vco_val = 0x0,
	.vco_mask = BM(17, 16),
	.pre_div_val = 0x0,
	.pre_div_mask = BIT(19),
	.post_div_val = 0x0,
	.post_div_mask = BM(21, 20),
	.mn_ena_val = BIT(22),
	.mn_ena_mask = BIT(22),
	.main_output_val = BIT(23),
	.main_output_mask = BIT(23),
};


static void __init init_mm_cc(void)
{
	/*
	 * Initialize MM CC registers: Set MM FORCE_CORE_ON bits so that core
	 * memories retain state even when not clocked. Also, set sleep and
	 * wake-up delays to safe values.
	 */
	rmwreg(0x00000000, CSI0_CC_REG,       0x00000410);
	rmwreg(0x00000000, CSI1_CC_REG,       0x00000410);
	rmwreg(0x80FF0000, DSI1_BYTE_CC_REG,  0xE0FF0010);
	rmwreg(0x80FF0000, DSI_PIXEL_CC_REG,  0xE0FF0010);
	rmwreg(0xC0FF0000, GFX3D_CC_REG,      0xE0FF0010);
	rmwreg(0x80FF0000, IJPEG_CC_REG,      0xE0FF0010);
	rmwreg(0x80FF0000, MDP_CC_REG,        0xE1FF0010);
	rmwreg(0x80FF0000, MDP_LUT_CC_REG,    0xE0FF0010);
	rmwreg(0x80FF0000, ROT_CC_REG,        0xE0FF0010);
	rmwreg(0x000004FF, TV_CC2_REG,        0x000007FF);
	rmwreg(0xC0FF0000, VCODEC_CC_REG,     0xE0FF0010);
	rmwreg(0x80FF0000, VFE_CC_REG,        0xE0FF4010);
	rmwreg(0x800000FF, VFE_CC2_REG,       0xE00000FF);
	rmwreg(0x80FF0000, VPE_CC_REG,        0xE0FF0010);
}

static void __init enable_imem_clk(unsigned long phys)
{
	void __iomem *imem_reg;

	/* Enable IMEM's clk_on signal */
	imem_reg = ioremap(phys, SZ_4);
	if (imem_reg) {
		writel_relaxed(0x3, imem_reg);
		iounmap(imem_reg);
	}
}

static void __init reg_init_8930(void)
{
	/* MM-AHB default values */
	u32 en_reg = 0x40000000, en2_reg = 0x3C7097F9;
	/* MM-AXI default values */
	u32 aen_reg = 0x0003AFF9, aen2_reg = 0x3A27FCFF,
		 saen_reg = 0x00003C38;


	/* Deassert MM SW_RESET_ALL signal. */
	writel_relaxed(0, SW_RESET_ALL_REG);

	/*
	 * Initialize MM AHB registers: Enable the FPB clock and disable HW
	 * gating on 8627 and 8930ab for all clocks. Also set VFE_AHB's
	 * FORCE_CORE_ON bit to prevent its memory from being collapsed when
	 * the clock is halted. The sleep and wake-up delays are set to safe
	 * values.
	 */
	if (cpu_is_msm8627() || cpu_is_msm8930ab()) {
		en_reg  = 0x00000003;
		en2_reg = 0x000007F9;
	}
	rmwreg(en_reg, AHB_EN_REG, 0x6C000103);
	writel_relaxed(en2_reg, AHB_EN2_REG);

	/* Deassert all locally-owned MM AHB resets. */
	rmwreg(0, SW_RESET_AHB_REG, 0xFFF7DFFF);
	rmwreg(0, SW_RESET_AHB2_REG, 0x0000000F);

	/*
	 * Initialize MM AXI registers: Enable HW gating for all clocks that
	 * support it. Also set FORCE_CORE_ON bits, and any sleep and wake-up
	 * delays to safe values.
	 */
	if (cpu_is_msm8627() || cpu_is_msm8930ab()) {
		aen_reg = 0x000007F9;
		aen2_reg = 0x3027FCFF;
	}
	rmwreg(aen_reg, MAXI_EN_REG,  0x0803FFFF);
	rmwreg(aen2_reg, MAXI_EN2_REG, 0x3A3FFFFF);
	rmwreg(0x0027FCFF, MAXI_EN3_REG, 0x003FFFFF);
	rmwreg(0x0027FCFF, MAXI_EN4_REG, 0x017FFFFF);
	rmwreg(0x000004FF, MAXI_EN5_REG, 0x00000FFF);
	if (cpu_is_msm8627() || cpu_is_msm8930ab())
		saen_reg = 0x000003C7;
	rmwreg(saen_reg, SAXI_EN_REG,  0x00003FFF);

	enable_imem_clk(0x04b00040);

	init_mm_cc();
	rmwreg(0x80FF0000, TV_CC_REG,        0xE1FFC010);

	/*
	 * Initialize USB_HS_HCLK_FS registers: Set FORCE_C_ON bits so that
	 * core remain active during halt state of the clk. Also, set sleep
	 * and wake-up value to max.
	 */
	rmwreg(0x0000004F, USB_HS1_HCLK_FS_REG, 0x0000007F);

	/* De-assert MM AXI resets to all hardware blocks. */
	writel_relaxed(0, SW_RESET_AXI_REG);

	/* Deassert all MM core resets. */
	writel_relaxed(0, SW_RESET_CORE_REG);
	writel_relaxed(0, SW_RESET_CORE2_REG);

	/* Enable TSSC and PDM PXO sources. */
	writel_relaxed(BIT(11), TSSC_CLK_CTL_REG);
	writel_relaxed(BIT(15), PDM_CLK_NS_REG);

	/* Source the dsi1_byte_clks/dsi1_esc_clk from the DSI PHY PLLs */
	rmwreg(0x1, DSI1_BYTE_NS_REG, 0x7);
	rmwreg(0x1, DSI1_ESC_NS_REG, 0x7);

	/*
	 * Change PLL15 configuration based on the SoC we're running on.
	 *
	 * Default pll15 l, m, n values for 8930/8930aa/8627()
	 */
	pll15_config.l = 0x21 | BVAL(31, 7, 0x600);
	pll15_config.m = 0x1;
	pll15_config.n = 0x3;

	if (cpu_is_msm8930ab()) {
		pll15_config.l = 0x25 | BVAL(31, 7, 0x600);
		pll15_config.m = 0x25;
		pll15_config.n = 0x3E7;
	}
	configure_sr_pll(&pll15_config, &pll15_regs, 0);

	/* Disable AUX and BIST outputs */
	writel_relaxed(0, MM_PLL3_TEST_CTL_REG);
}

static void __init reg_init_8064(void)
{
	u32 is_pll_enabled;

	/* Deassert MM SW_RESET_ALL signal. */
	writel_relaxed(0, SW_RESET_ALL_REG);

	/*
	 * Initialize MM AHB registers:
	 * Also set VFE_AHB's FORCE_CORE_ON bit to prevent its memory
	 * from being collapsed when the clock is halted. The sleep and
	 * wake-up delays are set to safe values.
	 */
	rmwreg(0x40000000, AHB_EN_REG, 0x6C000103);
	writel_relaxed(0x3C7097F9, AHB_EN2_REG);
	rmwreg(0x00000000, AHB_EN3_REG, 0x00000001);

	/* Deassert all locally-owned MM AHB resets. */
	rmwreg(0, SW_RESET_AHB_REG, 0xFFF7DFFF);
	rmwreg(0, SW_RESET_AHB2_REG, 0x0000000F);

	/*
	 * Initialize MM AXI registers: Enable HW gating for all clocks that
	 * support it. Also set FORCE_CORE_ON bits, and any sleep and wake-up
	 * delays to safe values.
	 */
	rmwreg(0x0003AFF9, MAXI_EN_REG,  0x0803FFFF);
	rmwreg(0x3A27FCFF, MAXI_EN2_REG, 0x3A3FFFFF);
	rmwreg(0x0027FCFF, MAXI_EN3_REG, 0x003FFFFF);
	rmwreg(0x0027FCFF, MAXI_EN4_REG, 0x017FFFFF);
	rmwreg(0x019FECFF, MAXI_EN5_REG, 0x01FFEFFF);
	rmwreg(0x00003C38, SAXI_EN_REG,  0x00003FFF);

	enable_imem_clk(0x04b00040);

	init_mm_cc();
	rmwreg(0x80FF0000, DSI2_BYTE_CC_REG,  0xE0FF0010);
	rmwreg(0x80FF0000, DSI2_PIXEL_CC_REG, 0xE0FF0010);
	rmwreg(0x80FF0000, JPEGD_CC_REG,      0xE0FF0010);
	rmwreg(0x00000000, TV_CC_REG,         0x00004010);
	rmwreg(0x80FF0000, VCAP_CC_REG,       0xE0FF1010);

	/*
	 * Initialize USB_HS_HCLK_FS registers: Set FORCE_C_ON bits so that
	 * core remain active during halt state of the clk. Also, set sleep
	 * and wake-up value to max.
	 */
	rmwreg(0x0000004F, USB_HS1_HCLK_FS_REG, 0x0000007F);
	rmwreg(0x0000004F, USB_HS3_HCLK_FS_REG, 0x0000007F);
	rmwreg(0x0000004F, USB_HS4_HCLK_FS_REG, 0x0000007F);

	/* De-assert MM AXI resets to all hardware blocks. */
	writel_relaxed(0, SW_RESET_AXI_REG);

	/* Deassert all MM core resets. */
	writel_relaxed(0, SW_RESET_CORE_REG);
	writel_relaxed(0, SW_RESET_CORE2_REG);

	/* Enable TSSC and PDM PXO sources. */
	writel_relaxed(BIT(11), TSSC_CLK_CTL_REG);
	writel_relaxed(BIT(15), PDM_CLK_NS_REG);

	/* Source the dsi1_byte_clks/dsi1_esc_clk from the DSI PHY PLLs */
	rmwreg(0x1, DSI1_BYTE_NS_REG, 0x7);
	rmwreg(0x1, DSI1_ESC_NS_REG, 0x7);

	/* Source the dsi2_byte_clks from the DSI PHY PLLs */
	rmwreg(0x2, DSI2_BYTE_NS_REG, 0x7);

	/*
	 * Source the sata_phy_ref_clk from PXO and set predivider of
	 * sata_pmalive_clk to 1.
	 */
	rmwreg(0, SATA_PHY_REF_CLK_CTL_REG, 0x1);
	rmwreg(0, SATA_PMALIVE_CLK_CTL_REG, 0x3);

	/*
	 * TODO: Programming below PLLs and prng_clk is temporary and
	 *	 needs to be removed after bootloaders program them.
	 */

	/* Program pxo_src_clk to source from PXO */
	rmwreg(0x1, PXO_SRC_CLK_CTL_REG, 0x7);

	/* Check if PLL14 is active */
	is_pll_enabled = readl_relaxed(BB_PLL14_STATUS_REG) & BIT(16);
	if (!is_pll_enabled)
		/* Ref clk = 27MHz and program pll14 to 480MHz */
		configure_sr_pll(&pll14_config, &pll14_regs, 1);

	/* Check if PLL4 is active */
	is_pll_enabled = readl_relaxed(LCC_PLL0_STATUS_REG) & BIT(16);
	if (!is_pll_enabled)
		/* Ref clk = 27MHz and program pll4 to 393.2160MHz */
		configure_sr_pll(&pll4_config_393, &pll4_regs, 1);

	/* Enable PLL4 source on the LPASS Primary PLL Mux */
	writel_relaxed(0x1, LCC_PRI_PLL_CLK_CTL_REG);

	/* Program prng_clk to 64MHz if it isn't configured */
	if (!readl_relaxed(PRNG_CLK_NS_REG))
		writel_relaxed(0x2B, PRNG_CLK_NS_REG);

	if (cpu_is_apq8064ab()) {
		/* Program PLL15 to 900MHZ */
		pll15_config.l = 0x21 | BVAL(31, 7, 0x620);
		pll15_config.m = 0x1;
		pll15_config.n = 0x3;
	}
	/*
	 * Default Program PLL15 to 975MHz with ref clk = 27MHz
	 * In case of apq8064ab PLL15 is set to 900MHZ
	 */
	configure_sr_pll(&pll15_config, &pll15_regs, 0);
}

static void __init reg_init_8960(void)
{
	u32 aen_reg = 0x0003AFF9, aen2_reg = 0x3A27FCFF;

	/* Deassert MM SW_RESET_ALL signal. */
	writel_relaxed(0, SW_RESET_ALL_REG);

	/*
	 * Initialize MM AHB registers:
	 * Also set VFE_AHB's FORCE_CORE_ON bit to prevent its memory
	 * from being collapsed when the clock is halted. The sleep and
	 * wake-up delays are set to safe values.
	 */
	rmwreg(0x40000000, AHB_EN_REG, 0x6C000103);
	writel_relaxed(0x3C7097F9, AHB_EN2_REG);

	/* Deassert all locally-owned MM AHB resets. */
	rmwreg(0, SW_RESET_AHB_REG, 0xFFF7DFFF);
	rmwreg(0, SW_RESET_AHB2_REG, 0x0000000F);

	/*
	 * Initialize MM AXI registers: Enable HW gating for all clocks that
	 * support it. Also set FORCE_CORE_ON bits, and any sleep and wake-up
	 * delays to safe values.
	 */
	if (cpu_is_msm8960() &&
		SOCINFO_VERSION_MAJOR(socinfo_get_version()) < 3) {
		aen_reg = 0x000007F9;
		aen2_reg = 0x3027FCFF;
	}
	rmwreg(aen_reg, MAXI_EN_REG,  0x0803FFFF);
	rmwreg(aen2_reg, MAXI_EN2_REG, 0x3A3FFFFF);
	rmwreg(0x0027FCFF, MAXI_EN3_REG, 0x003FFFFF);
	rmwreg(0x0027FCFF, MAXI_EN4_REG, 0x017FFFFF);
	if (cpu_is_msm8960ab())
		rmwreg(0x009FE000, MAXI_EN5_REG, 0x01FFE000);
	rmwreg(0x00003C38, SAXI_EN_REG,  0x00003FFF);

	enable_imem_clk(0x04b00040);

	init_mm_cc();
	rmwreg(0x80FF0000, DSI2_BYTE_CC_REG,  0xE0FF0010);
	rmwreg(0x80FF0000, DSI2_PIXEL_CC_REG, 0xE0FF0010);
	rmwreg(0x80FF0000, JPEGD_CC_REG,      0xE0FF0010);
	if (cpu_is_msm8960ab()) {
		rmwreg(0x00000001, DSI2_PIXEL_CC2_REG, 0x00000001);
		rmwreg(0x00000000, TV_CC_REG,        0x00004010);
	}
	if (cpu_is_msm8960()) {
		rmwreg(0x80FF0000, TV_CC_REG,        0xE1FFC010);
		rmwreg(0x80FF0000, GFX2D0_CC_REG,     0xE0FF0010);
		rmwreg(0x80FF0000, GFX2D1_CC_REG,     0xE0FF0010);
	}

	/*
	 * Initialize USB_HS_HCLK_FS registers: Set FORCE_C_ON bits so that
	 * core remain active during halt state of the clk. Also, set sleep
	 * and wake-up value to max.
	 */
	rmwreg(0x0000004F, USB_HS1_HCLK_FS_REG, 0x0000007F);

	/* De-assert MM AXI resets to all hardware blocks. */
	writel_relaxed(0, SW_RESET_AXI_REG);

	/* Deassert all MM core resets. */
	writel_relaxed(0, SW_RESET_CORE_REG);
	writel_relaxed(0, SW_RESET_CORE2_REG);

	/* Enable TSSC and PDM PXO sources. */
	writel_relaxed(BIT(11), TSSC_CLK_CTL_REG);
	writel_relaxed(BIT(15), PDM_CLK_NS_REG);

	/* Source the dsi1_byte_clks/dsi1_esc_clk from the DSI PHY PLLs */
	rmwreg(0x1, DSI1_BYTE_NS_REG, 0x7);
	rmwreg(0x1, DSI1_ESC_NS_REG, 0x7);

	/* Source SLIMBus xo src from slimbus reference clock */
	writel_relaxed(0x3, SLIMBUS_XO_SRC_CLK_CTL_REG);

	/* Source the dsi2_byte_clks from the DSI PHY PLLs */
	rmwreg(0x2, DSI2_BYTE_NS_REG, 0x7);

	 if (cpu_is_msm8960ab()) {
		pll3_clk.c.rate = 880000000;
		configure_sr_pll(&pll3_config, &pll3_regs, 0);
	}
}

#define PTE_EFUSE_GFX_PHYS (0x007000BC)

static unsigned long *select_gfx_fmax_plan(unsigned long **gfx_fmax, int size)
{
	void __iomem *pte_efuse;
	u32 gfx_speed_bin;

	pte_efuse = ioremap(PTE_EFUSE_GFX_PHYS, 4);
	gfx_speed_bin = readl_relaxed(pte_efuse);
	gfx_speed_bin = (gfx_speed_bin & BM(25, 24)) >> 24;
	iounmap(pte_efuse);

	if (gfx_speed_bin >= size) {
		pr_err("GFX_SPEED_BIN: defaulting to 0\n");
		gfx_speed_bin = 0;
	}

	pr_info("GFX_SPEED_BIN: %d\n", gfx_speed_bin);
	return gfx_fmax[gfx_speed_bin];
}

struct clock_init_data msm8960_clock_init_data __initdata;

static void __init msm8960_clock_pre_init(void)
{
	struct clk_lookup *clk_lkup;
	size_t clk_size;
	struct clk_freq_tbl *tbl;

	/* Initialize clock registers. */
	reg_init_8960();

	/* Detect PLL4 programmed for alternate 491.52MHz clock plan. */
	if (readl_relaxed(LCC_PLL0_L_VAL_REG) == 0x12) {
		pll4_clk.c.rate = 491520000;
		audio_slimbus_clk.freq_tbl = clk_tbl_aif_osr_492;
		mi2s_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		codec_i2s_mic_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		spare_i2s_mic_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		codec_i2s_spkr_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		spare_i2s_spkr_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		pcm_clk.freq_tbl = clk_tbl_pcm_492;
	}

	if (cpu_is_msm8960()) {
		tbl = clk_tbl_gfx3d_8960;
		clk_lkup = msm_clocks_8960_only;
		clk_size = sizeof(msm_clocks_8960_only);
		msm8960_clock_init_data.size -=
			ARRAY_SIZE(msm_clocks_8960ab_only);
	}

	if (cpu_is_msm8960ab()) {
		mdp_clk.c.fmax = fmax_mdp_8960ab;
		gmem_axi_clk.c.depends = &gfx3d_axi_clk.c;
		tbl = clk_tbl_gfx3d_8960ab;
		gfx3d_clk.c.fmax = select_gfx_fmax_plan(fmax_gfx3d_8960ab,
				ARRAY_SIZE(fmax_gfx3d_8960ab));

		clk_lkup = msm_clocks_8960ab_only;
		clk_size = sizeof(msm_clocks_8960ab_only);
		msm8960_clock_init_data.size -=
			 ARRAY_SIZE(msm_clocks_8960_only);
	}

	gfx3d_clk.freq_tbl = tbl;

	memcpy(msm_clocks_8960, msm_clocks_8960_common,
			sizeof(msm_clocks_8960_common));
	memcpy(msm_clocks_8960 + ARRAY_SIZE(msm_clocks_8960_common),
			clk_lkup, clk_size);

	if ((readl_relaxed(PRNG_CLK_NS_REG) & 0x7F) == 0x2B)
		prng_clk.freq_tbl = clk_tbl_prng_64;

	clk_ops_local_pll.enable = sr_pll_clk_enable;
}

static void __init msm8064_clock_pre_init(void)
{
	unsigned long *fmax = fmax_gfx3d_8064;

	/* Initialize clock registers. */
	reg_init_8064();

	vdd_sr2_hdmi_pll.set_vdd = set_vdd_sr2_hdmi_pll_8064;

	/* Detect PLL4 programmed for alternate 491.52MHz clock plan. */
	if (readl_relaxed(LCC_PLL0_L_VAL_REG) == 0x12) {
		pll4_clk.c.rate = 491520000;
		audio_slimbus_clk.freq_tbl = clk_tbl_aif_osr_492;
		mi2s_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		codec_i2s_mic_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		spare_i2s_mic_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		codec_i2s_spkr_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		spare_i2s_spkr_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		pcm_clk.freq_tbl = clk_tbl_pcm_492;
	}

	if (cpu_is_apq8064ab())
		fmax = fmax_gfx3d_8064ab;

	if ((cpu_is_apq8064() &&
		SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2) ||
		cpu_is_apq8064ab() || cpu_is_apq8064aa()) {
		vcodec_clk.c.fmax = fmax_vcodec_8064v2;
		ce3_src_clk.c.fmax = fmax_ce3_8064v2;
		sdc1_clk.c.fmax = fmax_sdc1_8064v2;
	}

	gfx3d_clk.c.fmax = fmax;
	ijpeg_clk.c.fmax = fmax_ijpeg_8064;
	mdp_clk.c.fmax = fmax_mdp_8064;
	tv_src_clk.c.fmax = fmax_tv_src_8064;
	vfe_clk.c.fmax = fmax_vfe_8064;

	gmem_axi_clk.c.depends = &gfx3d_axi_clk.c;

	if ((readl_relaxed(PRNG_CLK_NS_REG) & 0x7F) == 0x2B)
		prng_clk.freq_tbl = clk_tbl_prng_64;

	clk_ops_local_pll.enable = sr_pll_clk_enable;
}

static void __init __msm8930_clock_pre_init(void)
{
	unsigned long rate = 900000000;
	unsigned long *fmax = fmax_gfx3d_8930;

	/* Initialize clock registers. */
	reg_init_8930();

	/* Detect PLL4 programmed for alternate 491.52MHz clock plan. */
	if (readl_relaxed(LCC_PLL0_L_VAL_REG) == 0x12) {
		pll4_clk.c.rate = 491520000;
		audio_slimbus_clk.freq_tbl = clk_tbl_aif_osr_492;
		mi2s_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		codec_i2s_mic_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		spare_i2s_mic_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		codec_i2s_spkr_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		spare_i2s_spkr_osr_clk.freq_tbl = clk_tbl_aif_osr_492;
		pcm_clk.freq_tbl = clk_tbl_pcm_492;
	}

	if (cpu_is_msm8930aa())
		fmax = fmax_gfx3d_8930aa;

	if (cpu_is_msm8930ab()) {
		rate = 1000000000;
		fmax = fmax_gfx3d_8930ab;
		gfx3d_clk.freq_tbl = clk_tbl_gfx3d_8930ab;
		vcodec_clk.c.fmax = fmax_vcodec_8930ab;
	}

	pll15_clk.c.rate = rate;
	gfx3d_clk.c.fmax = fmax;
	gmem_axi_clk.c.depends = &gfx3d_axi_clk_8930.c;

	if ((readl_relaxed(PRNG_CLK_NS_REG) & 0x7F) == 0x2B)
		prng_clk.freq_tbl = clk_tbl_prng_64;

	clk_ops_local_pll.enable = sr_pll_clk_enable;
}

static void __init msm8930_pm8917_clock_pre_init(void)
{
	/* detect pmic8917 from board file, and call this init function */

	vdd_dig.set_vdd = set_vdd_dig_8930;
	rpm_vreg_dig_8930 = RPM_VREG_ID_PM8917_VDD_DIG_CORNER;
	vdd_sr2_hdmi_pll.set_vdd = set_vdd_sr2_hdmi_pll_8930_pm8917;

	__msm8930_clock_pre_init();
}

static void __init msm8930_clock_pre_init(void)
{
	vdd_dig.set_vdd = set_vdd_dig_8930;
	vdd_sr2_hdmi_pll.set_vdd = set_vdd_sr2_hdmi_pll_8930;

	__msm8930_clock_pre_init();
}

static void __init common_clock_post_init(void)
{
	/* Keep PXO on whenever APPS cpu is active */
	clk_prepare_enable(&pxo_a_clk.c);

	/* Reset 3D core while clocked to ensure it resets completely. */
	clk_set_rate(&gfx3d_clk.c, 27000000);
	clk_prepare_enable(&gfx3d_clk.c);
	clk_reset(&gfx3d_clk.c, CLK_RESET_ASSERT);
	udelay(5);
	clk_reset(&gfx3d_clk.c, CLK_RESET_DEASSERT);
	clk_disable_unprepare(&gfx3d_clk.c);

	/* Initialize rates for clocks that only support one. */
	clk_set_rate(&pdm_clk.c, 27000000);
	clk_set_rate(&prng_clk.c, prng_clk.freq_tbl->freq_hz);
	clk_set_rate(&mdp_vsync_clk.c, 27000000);
	clk_set_rate(&tsif_ref_clk.c, 105000);
	clk_set_rate(&tssc_clk.c, 27000000);
	clk_set_rate(&usb_hs1_xcvr_clk.c, 60000000);
	clk_set_rate(&usb_fs1_src_clk.c, 60000000);
	clk_set_rate(&usb_hsic_xcvr_fs_clk.c, 60000000);
	clk_set_rate(&usb_hsic_hsic_src_clk.c, 480000000);
	clk_set_rate(&usb_hsic_hsio_cal_clk.c, 9000000);
	clk_set_rate(&usb_hsic_system_clk.c, 60000000);

	/*
	 * Set the CSI rates to a safe default to avoid warnings when
	 * switching csi pix and rdi clocks.
	 */
	clk_set_rate(&csi0_src_clk.c, 27000000);
	clk_set_rate(&csi1_src_clk.c, 27000000);
	clk_set_rate(&csi2_src_clk.c, 27000000);

	/*
	 * The halt status bits for these clocks may be incorrect at boot.
	 * Toggle these clocks on and off to refresh them.
	 */
	clk_prepare_enable(&pdm_clk.c);
	clk_disable_unprepare(&pdm_clk.c);
	clk_prepare_enable(&tssc_clk.c);
	clk_disable_unprepare(&tssc_clk.c);
	clk_prepare_enable(&usb_hsic_hsic_clk.c);
	clk_disable_unprepare(&usb_hsic_hsic_clk.c);

	/*
	 * Keep sfab floor @ 54MHz so that Krait AHB is at least 27MHz at all
	 * times when Apps CPU is active. This ensures the timer's requirement
	 * of Krait AHB running 4 times as fast as the timer itself.
	 */
	clk_set_rate(&sfab_tmr_a_clk.c, 54000000);
	clk_prepare_enable(&sfab_tmr_a_clk.c);
}

static void __init msm8960_clock_post_init(void)
{
	common_clock_post_init();
	clk_set_rate(&usb_fs2_src_clk.c, 60000000);
}

static void __init msm8064_clock_post_init(void)
{
	common_clock_post_init();
	clk_set_rate(&usb_hs3_xcvr_clk.c, 60000000);
	clk_set_rate(&usb_hs4_xcvr_clk.c, 60000000);
}

static int __init msm8960_clock_late_init(void)
{
	int rc;
	struct clk *mmfpb_a_clk = clk_get_sys("clock-8960", "mmfpb_a_clk");
	struct clk *cfpb_a_clk = clk_get_sys("clock-8960", "cfpb_a_clk");

	/* Vote for MMFPB to be on when Apps is active. */
	if (WARN(IS_ERR(mmfpb_a_clk), "mmfpb_a_clk not found (%ld)\n",
			PTR_ERR(mmfpb_a_clk)))
		return PTR_ERR(mmfpb_a_clk);
	rc = clk_set_rate(mmfpb_a_clk, 38400000);
	if (WARN(rc, "mmfpb_a_clk rate was not set (%d)\n", rc))
		return rc;
	rc = clk_prepare_enable(mmfpb_a_clk);
	if (WARN(rc, "mmfpb_a_clk not enabled (%d)\n", rc))
		return rc;

	/* Vote for CFPB to be on when Apps is active. */
	if (WARN(IS_ERR(cfpb_a_clk), "cfpb_a_clk not found (%ld)\n",
			PTR_ERR(cfpb_a_clk)))
		return PTR_ERR(cfpb_a_clk);
	rc = clk_set_rate(cfpb_a_clk, 32000000);
	if (WARN(rc, "cfpb_a_clk rate was not set (%d)\n", rc))
		return rc;
	rc = clk_prepare_enable(cfpb_a_clk);
	if (WARN(rc, "cfpb_a_clk not enabled (%d)\n", rc))
		return rc;

	return 0;
}

struct clock_init_data msm8960_clock_init_data __initdata = {
	.table = msm_clocks_8960,
	.size = ARRAY_SIZE(msm_clocks_8960),
	.pre_init = msm8960_clock_pre_init,
	.post_init = msm8960_clock_post_init,
	.late_init = msm8960_clock_late_init,
};

struct clock_init_data apq8064_clock_init_data __initdata = {
	.table = msm_clocks_8064,
	.size = ARRAY_SIZE(msm_clocks_8064),
	.pre_init = msm8064_clock_pre_init,
	.post_init = msm8064_clock_post_init,
	.late_init = msm8960_clock_late_init,
};

struct clock_init_data msm8930_clock_init_data __initdata = {
	.table = msm_clocks_8930,
	.size = ARRAY_SIZE(msm_clocks_8930),
	.pre_init = msm8930_clock_pre_init,
	.post_init = msm8960_clock_post_init,
	.late_init = msm8960_clock_late_init,
};

struct clock_init_data msm8930_pm8917_clock_init_data __initdata = {
	.table = msm_clocks_8930,
	.size = ARRAY_SIZE(msm_clocks_8930),
	.pre_init = msm8930_pm8917_clock_pre_init,
	.post_init = msm8960_clock_post_init,
	.late_init = msm8960_clock_late_init,
};

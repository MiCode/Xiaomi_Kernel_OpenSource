/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
#include <mach/clk.h>
#include <mach/rpm-regulator.h>
#include <mach/msm_xo.h>

#include "clock-local.h"
#include "clock-rpm.h"
#include "clock-voter.h"
#include "clock-dss-8960.h"
#include "devices.h"

#define REG(off)	(MSM_CLK_CTL_BASE + (off))
#define REG_MM(off)	(MSM_MMSS_CLK_CTL_BASE + (off))
#define REG_LPA(off)	(MSM_LPASS_CLK_CTL_BASE + (off))

/* Peripheral clock registers. */
#define CE1_HCLK_CTL_REG			REG(0x2720)
#define CE1_CORE_CLK_CTL_REG			REG(0x2724)
#define DMA_BAM_HCLK_CTL			REG(0x25C0)
#define CLK_HALT_CFPB_STATEA_REG		REG(0x2FCC)
#define CLK_HALT_CFPB_STATEB_REG		REG(0x2FD0)
#define CLK_HALT_CFPB_STATEC_REG		REG(0x2FD4)
#define CLK_HALT_DFAB_STATE_REG			REG(0x2FC8)
#define CLK_HALT_MSS_SMPSS_MISC_STATE_REG	REG(0x2FDC)
#define CLK_HALT_SFPB_MISC_STATE_REG		REG(0x2FD8)
#define CLK_TEST_REG				REG(0x2FA0)
#define GSBIn_HCLK_CTL_REG(n)			REG(0x29C0+(0x20*((n)-1)))
#define GSBIn_QUP_APPS_MD_REG(n)		REG(0x29C8+(0x20*((n)-1)))
#define GSBIn_QUP_APPS_NS_REG(n)		REG(0x29CC+(0x20*((n)-1)))
#define GSBIn_RESET_REG(n)			REG(0x29DC+(0x20*((n)-1)))
#define GSBIn_UART_APPS_MD_REG(n)		REG(0x29D0+(0x20*((n)-1)))
#define GSBIn_UART_APPS_NS_REG(n)		REG(0x29D4+(0x20*((n)-1)))
#define LPASS_XO_SRC_CLK_CTL_REG		REG(0x2EC0)
#define PDM_CLK_NS_REG				REG(0x2CC0)
#define BB_PLL_ENA_Q6_SW_REG			REG(0x3500)
#define BB_PLL_ENA_SC0_REG			REG(0x34C0)
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
#define PLLTEST_PAD_CFG_REG			REG(0x2FA4)
#define PMEM_ACLK_CTL_REG			REG(0x25A0)
#define RINGOSC_NS_REG				REG(0x2DC0)
#define RINGOSC_STATUS_REG			REG(0x2DCC)
#define RINGOSC_TCXO_CTL_REG			REG(0x2DC4)
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
#define USB_FSn_HCLK_CTL_REG(n)			REG(0x2960+(0x20*((n)-1)))
#define USB_FSn_RESET_REG(n)			REG(0x2974+(0x20*((n)-1)))
#define USB_FSn_SYSTEM_CLK_CTL_REG(n)		REG(0x296C+(0x20*((n)-1)))
#define USB_FSn_XCVR_FS_CLK_MD_REG(n)		REG(0x2964+(0x20*((n)-1)))
#define USB_FSn_XCVR_FS_CLK_NS_REG(n)		REG(0x2968+(0x20*((n)-1)))
#define USB_HS1_HCLK_CTL_REG			REG(0x2900)
#define USB_HS1_RESET_REG			REG(0x2910)
#define USB_HS1_XCVR_FS_CLK_MD_REG		REG(0x2908)
#define USB_HS1_XCVR_FS_CLK_NS_REG		REG(0x290C)
#define USB_PHY0_RESET_REG			REG(0x2E20)

/* Multimedia clock registers. */
#define AHB_EN_REG				REG_MM(0x0008)
#define AHB_EN2_REG				REG_MM(0x0038)
#define AHB_NS_REG				REG_MM(0x0004)
#define AXI_NS_REG				REG_MM(0x0014)
#define CAMCLKn_NS_REG(n)			REG_MM(0x0148+(0x14*(n)))
#define CAMCLKn_CC_REG(n)			REG_MM(0x0140+(0x14*(n)))
#define CAMCLKn_MD_REG(n)			REG_MM(0x0144+(0x14*(n)))
#define CSI0_NS_REG				REG_MM(0x0048)
#define CSI0_CC_REG				REG_MM(0x0040)
#define CSI0_MD_REG				REG_MM(0x0044)
#define CSI1_NS_REG				REG_MM(0x0010)
#define CSI1_CC_REG				REG_MM(0x0024)
#define CSI1_MD_REG				REG_MM(0x0028)
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
#define DBG_BUS_VEC_A_REG			REG_MM(0x01C8)
#define DBG_BUS_VEC_B_REG			REG_MM(0x01CC)
#define DBG_BUS_VEC_C_REG			REG_MM(0x01D0)
#define DBG_BUS_VEC_D_REG			REG_MM(0x01D4)
#define DBG_BUS_VEC_E_REG			REG_MM(0x01D8)
#define DBG_BUS_VEC_F_REG			REG_MM(0x01DC)
#define DBG_BUS_VEC_G_REG			REG_MM(0x01E0)
#define DBG_BUS_VEC_H_REG			REG_MM(0x01E4)
#define DBG_BUS_VEC_I_REG			REG_MM(0x01E8)
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
#define MAXI_EN_REG				REG_MM(0x0018)
#define MAXI_EN2_REG				REG_MM(0x0020)
#define MAXI_EN3_REG				REG_MM(0x002C)
#define MAXI_EN4_REG				REG_MM(0x0114)
#define MDP_CC_REG				REG_MM(0x00C0)
#define MDP_LUT_CC_REG				REG_MM(0x016C)
#define MDP_MD0_REG				REG_MM(0x00C4)
#define MDP_MD1_REG				REG_MM(0x00C8)
#define MDP_NS_REG				REG_MM(0x00D0)
#define MISC_CC_REG				REG_MM(0x0058)
#define MISC_CC2_REG				REG_MM(0x005C)
#define MM_PLL1_MODE_REG			REG_MM(0x031C)
#define ROT_CC_REG				REG_MM(0x00E0)
#define ROT_NS_REG				REG_MM(0x00E8)
#define SAXI_EN_REG				REG_MM(0x0030)
#define SW_RESET_AHB_REG			REG_MM(0x020C)
#define SW_RESET_AHB2_REG			REG_MM(0x0200)
#define SW_RESET_ALL_REG			REG_MM(0x0204)
#define SW_RESET_AXI_REG			REG_MM(0x0208)
#define SW_RESET_CORE_REG			REG_MM(0x0210)
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
#define VPE_CC_REG				REG_MM(0x0110)
#define VPE_NS_REG				REG_MM(0x0118)

/* Low-power Audio clock registers. */
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
#define LCC_PLL0_STATUS_REG			REG_LPA(0x0018)
#define LCC_PRI_PLL_CLK_CTL_REG			REG_LPA(0x00C4)
#define LCC_PXO_SRC_CLK_CTL_REG			REG_LPA(0x00B4)
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

/* MUX source input identifiers. */
#define pxo_to_bb_mux		0
#define cxo_to_bb_mux		pxo_to_bb_mux
#define pll0_to_bb_mux		2
#define pll8_to_bb_mux		3
#define pll6_to_bb_mux		4
#define gnd_to_bb_mux		5
#define pxo_to_mm_mux		0
#define pll1_to_mm_mux		1
#define pll2_to_mm_mux		1
#define pll8_to_mm_mux		2
#define pll0_to_mm_mux		3
#define gnd_to_mm_mux		4
#define hdmi_pll_to_mm_mux	3
#define cxo_to_xo_mux		0
#define pxo_to_xo_mux		1
#define gnd_to_xo_mux		3
#define pxo_to_lpa_mux		0
#define cxo_to_lpa_mux		1
#define pll4_to_lpa_mux		2
#define gnd_to_lpa_mux		6

/* Test Vector Macros */
#define TEST_TYPE_PER_LS	1
#define TEST_TYPE_PER_HS	2
#define TEST_TYPE_MM_LS		3
#define TEST_TYPE_MM_HS		4
#define TEST_TYPE_LPA		5
#define TEST_TYPE_SHIFT		24
#define TEST_CLK_SEL_MASK	BM(23, 0)
#define TEST_VECTOR(s, t)	(((t) << TEST_TYPE_SHIFT) | BVAL(23, 0, (s)))
#define TEST_PER_LS(s)		TEST_VECTOR((s), TEST_TYPE_PER_LS)
#define TEST_PER_HS(s)		TEST_VECTOR((s), TEST_TYPE_PER_HS)
#define TEST_MM_LS(s)		TEST_VECTOR((s), TEST_TYPE_MM_LS)
#define TEST_MM_HS(s)		TEST_VECTOR((s), TEST_TYPE_MM_HS)
#define TEST_LPA(s)		TEST_VECTOR((s), TEST_TYPE_LPA)

#define MN_MODE_DUAL_EDGE 0x2

/* MD Registers */
#define MD4(m_lsb, m, n_lsb, n) \
		(BVAL((m_lsb+3), m_lsb, m) | BVAL((n_lsb+3), n_lsb, ~(n)))
#define MD8(m_lsb, m, n_lsb, n) \
		(BVAL((m_lsb+7), m_lsb, m) | BVAL((n_lsb+7), n_lsb, ~(n)))
#define MD16(m, n) (BVAL(31, 16, m) | BVAL(15, 0, ~(n)))

/* NS Registers */
#define NS(n_msb, n_lsb, n, m, mde_lsb, d_msb, d_lsb, d, s_msb, s_lsb, s) \
		(BVAL(n_msb, n_lsb, ~(n-m)) \
		| (BVAL((mde_lsb+1), mde_lsb, MN_MODE_DUAL_EDGE) * !!(n)) \
		| BVAL(d_msb, d_lsb, (d-1)) | BVAL(s_msb, s_lsb, s))

#define NS_MM(n_msb, n_lsb, n, m, d_msb, d_lsb, d, s_msb, s_lsb, s) \
		(BVAL(n_msb, n_lsb, ~(n-m)) | BVAL(d_msb, d_lsb, (d-1)) \
		| BVAL(s_msb, s_lsb, s))

#define NS_DIVSRC(d_msb , d_lsb, d, s_msb, s_lsb, s) \
		(BVAL(d_msb, d_lsb, (d-1)) | BVAL(s_msb, s_lsb, s))

#define NS_DIV(d_msb , d_lsb, d) \
		BVAL(d_msb, d_lsb, (d-1))

#define NS_SRC_SEL(s_msb, s_lsb, s) \
		BVAL(s_msb, s_lsb, s)

#define NS_MND_BANKED4(n0_lsb, n1_lsb, n, m, s0_lsb, s1_lsb, s) \
		 (BVAL((n0_lsb+3), n0_lsb, ~(n-m)) \
		| BVAL((n1_lsb+3), n1_lsb, ~(n-m)) \
		| BVAL((s0_lsb+2), s0_lsb, s) \
		| BVAL((s1_lsb+2), s1_lsb, s))

#define NS_MND_BANKED8(n0_lsb, n1_lsb, n, m, s0_lsb, s1_lsb, s) \
		 (BVAL((n0_lsb+7), n0_lsb, ~(n-m)) \
		| BVAL((n1_lsb+7), n1_lsb, ~(n-m)) \
		| BVAL((s0_lsb+2), s0_lsb, s) \
		| BVAL((s1_lsb+2), s1_lsb, s))

#define NS_DIVSRC_BANKED(d0_msb, d0_lsb, d1_msb, d1_lsb, d, \
	s0_msb, s0_lsb, s1_msb, s1_lsb, s) \
		 (BVAL(d0_msb, d0_lsb, (d-1)) | BVAL(d1_msb, d1_lsb, (d-1)) \
		| BVAL(s0_msb, s0_lsb, s) \
		| BVAL(s1_msb, s1_lsb, s))

/* CC Registers */
#define CC(mde_lsb, n) (BVAL((mde_lsb+1), mde_lsb, MN_MODE_DUAL_EDGE) * !!(n))
#define CC_BANKED(mde0_lsb, mde1_lsb, n) \
		((BVAL((mde0_lsb+1), mde0_lsb, MN_MODE_DUAL_EDGE) \
		| BVAL((mde1_lsb+1), mde1_lsb, MN_MODE_DUAL_EDGE)) \
		* !!(n))

struct pll_rate {
	const uint32_t	l_val;
	const uint32_t	m_val;
	const uint32_t	n_val;
	const uint32_t	vco;
	const uint32_t	post_div;
	const uint32_t	i_bits;
};
#define PLL_RATE(l, m, n, v, d, i) { l, m, n, v, (d>>1), i }

/*
 * Clock Descriptions
 */

static struct msm_xo_voter *xo_pxo, *xo_cxo;

static int pxo_clk_enable(struct clk *clk)
{
	return msm_xo_mode_vote(xo_pxo, MSM_XO_MODE_ON);
}

static void pxo_clk_disable(struct clk *clk)
{
	 msm_xo_mode_vote(xo_pxo, MSM_XO_MODE_OFF);
}

static struct clk_ops clk_ops_pxo = {
	.enable = pxo_clk_enable,
	.disable = pxo_clk_disable,
	.get_rate = fixed_clk_get_rate,
	.is_local = local_clk_is_local,
};

static struct fixed_clk pxo_clk = {
	.rate = 27000000,
	.c = {
		.dbg_name = "pxo_clk",
		.ops = &clk_ops_pxo,
		CLK_INIT(pxo_clk.c),
	},
};

static int cxo_clk_enable(struct clk *clk)
{
	return msm_xo_mode_vote(xo_cxo, MSM_XO_MODE_ON);
}

static void cxo_clk_disable(struct clk *clk)
{
	msm_xo_mode_vote(xo_cxo, MSM_XO_MODE_OFF);
}

static struct clk_ops clk_ops_cxo = {
	.enable = cxo_clk_enable,
	.disable = cxo_clk_disable,
	.get_rate = fixed_clk_get_rate,
	.is_local = local_clk_is_local,
};

static struct fixed_clk cxo_clk = {
	.rate = 19200000,
	.c = {
		.dbg_name = "cxo_clk",
		.ops = &clk_ops_cxo,
		CLK_INIT(cxo_clk.c),
	},
};

static struct pll_clk pll2_clk = {
	.rate = 800000000,
	.mode_reg = MM_PLL1_MODE_REG,
	.parent = &pxo_clk.c,
	.c = {
		.dbg_name = "pll2_clk",
		.ops = &clk_ops_pll,
		CLK_INIT(pll2_clk.c),
	},
};

static struct pll_vote_clk pll4_clk = {
	.rate = 393216000,
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(4),
	.status_reg = LCC_PLL0_STATUS_REG,
	.parent = &pxo_clk.c,
	.c = {
		.dbg_name = "pll4_clk",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll4_clk.c),
	},
};

static struct pll_vote_clk pll8_clk = {
	.rate = 384000000,
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(8),
	.status_reg = BB_PLL8_STATUS_REG,
	.parent = &pxo_clk.c,
	.c = {
		.dbg_name = "pll8_clk",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll8_clk.c),
	},
};

/*
 * SoC-specific functions required by clock-local driver
 */

/* Update the sys_vdd voltage given a level. */
static int msm8960_update_sys_vdd(enum sys_vdd_level level)
{
	static const int vdd_uv[] = {
		[NONE...LOW] =  945000,
		[NOMINAL] = 1050000,
		[HIGH]    = 1150000,
	};

	return rpm_vreg_set_voltage(RPM_VREG_ID_PM8921_S3, RPM_VREG_VOTER3,
				    vdd_uv[level], vdd_uv[HIGH], 1);
}

static int soc_clk_reset(struct clk *clk, enum clk_reset_action action)
{
	return branch_reset(&to_rcg_clk(clk)->b, action);
}

static struct clk_ops soc_clk_ops_8960 = {
	.enable = rcg_clk_enable,
	.disable = rcg_clk_disable,
	.auto_off = rcg_clk_auto_off,
	.set_rate = rcg_clk_set_rate,
	.set_min_rate = rcg_clk_set_min_rate,
	.get_rate = rcg_clk_get_rate,
	.list_rate = rcg_clk_list_rate,
	.is_enabled = rcg_clk_is_enabled,
	.round_rate = rcg_clk_round_rate,
	.reset = soc_clk_reset,
	.is_local = local_clk_is_local,
	.get_parent = rcg_clk_get_parent,
};

static struct clk_ops clk_ops_branch = {
	.enable = branch_clk_enable,
	.disable = branch_clk_disable,
	.auto_off = branch_clk_auto_off,
	.is_enabled = branch_clk_is_enabled,
	.reset = branch_clk_reset,
	.is_local = local_clk_is_local,
	.get_parent = branch_clk_get_parent,
	.set_parent = branch_clk_set_parent,
};

static struct clk_ops clk_ops_reset = {
	.reset = branch_clk_reset,
	.is_local = local_clk_is_local,
};

/* AXI Interfaces */
static struct branch_clk gmem_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(24),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 6,
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
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(10),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 7,
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
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 25,
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
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 26,
	},
	.depends = &vcodec_axi_b_clk.c,
	.c = {
		.dbg_name = "vcodec_axi_a_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcodec_axi_a_clk.c),
	},
};

static struct branch_clk vcodec_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(19),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(4)|BIT(5)|BIT(7),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 3,
	},
	.depends = &vcodec_axi_a_clk.c,
	.c = {
		.dbg_name = "vcodec_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcodec_axi_clk.c),
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
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(13),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 8,
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
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(6),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 2,
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
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(15),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 1,
	},
	.c = {
		.dbg_name = "vpe_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpe_axi_clk.c),
	},
};

/* AHB Interfaces */
static struct branch_clk amp_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(24),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 18,
	},
	.c = {
		.dbg_name = "amp_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(amp_p_clk.c),
	},
};

static struct branch_clk csi0_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(7),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(17),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 16,
	},
	.c = {
		.dbg_name = "csi0_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi0_p_clk.c),
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
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(12),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 2,
	},
	.c = {
		.dbg_name = "gfx2d0_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gfx2d0_p_clk.c),
	},
};

static struct branch_clk gfx2d1_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(2),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(11),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 3,
	},
	.c = {
		.dbg_name = "gfx2d1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gfx2d1_p_clk.c),
	},
};

static struct branch_clk gfx3d_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(3),
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

/*
 * Peripheral Clocks
 */
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
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_gsbi_uart, \
		.current_freq = &local_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &soc_clk_ops_8960, \
			CLK_INIT(i##_clk.c), \
		}, \
	}
#define F_GSBI_UART(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD16(m, n), \
		.ns_val = NS(31, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_gsbi_uart[] = {
	F_GSBI_UART(       0, gnd,  1,  0,   0, NONE),
	F_GSBI_UART( 1843200, pll8, 1,  3, 625, LOW),
	F_GSBI_UART( 3686400, pll8, 1,  6, 625, LOW),
	F_GSBI_UART( 7372800, pll8, 1, 12, 625, LOW),
	F_GSBI_UART(14745600, pll8, 1, 24, 625, LOW),
	F_GSBI_UART(16000000, pll8, 4,  1,   6, LOW),
	F_GSBI_UART(24000000, pll8, 4,  1,   4, LOW),
	F_GSBI_UART(32000000, pll8, 4,  1,   3, LOW),
	F_GSBI_UART(40000000, pll8, 1,  5,  48, NOMINAL),
	F_GSBI_UART(46400000, pll8, 1, 29, 240, NOMINAL),
	F_GSBI_UART(48000000, pll8, 4,  1,   2, NOMINAL),
	F_GSBI_UART(51200000, pll8, 1,  2,  15, NOMINAL),
	F_GSBI_UART(56000000, pll8, 1,  7,  48, NOMINAL),
	F_GSBI_UART(58982400, pll8, 1, 96, 625, NOMINAL),
	F_GSBI_UART(64000000, pll8, 2,  1,   3, NOMINAL),
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
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_gsbi_qup, \
		.current_freq = &local_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &soc_clk_ops_8960, \
			CLK_INIT(i##_clk.c), \
		}, \
	}
#define F_GSBI_QUP(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(16, m, 0, n), \
		.ns_val = NS(23, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_gsbi_qup[] = {
	F_GSBI_QUP(       0, gnd,  1, 0,  0, NONE),
	F_GSBI_QUP( 1100000, pxo,  1, 2, 49, LOW),
	F_GSBI_QUP( 5400000, pxo,  1, 1,  5, LOW),
	F_GSBI_QUP(10800000, pxo,  1, 2,  5, LOW),
	F_GSBI_QUP(15060000, pll8, 1, 2, 51, LOW),
	F_GSBI_QUP(24000000, pll8, 4, 1,  4, LOW),
	F_GSBI_QUP(25600000, pll8, 1, 1, 15, NOMINAL),
	F_GSBI_QUP(27000000, pxo,  1, 0,  0, NOMINAL),
	F_GSBI_QUP(48000000, pll8, 4, 1,  2, NOMINAL),
	F_GSBI_QUP(51200000, pll8, 1, 2, 15, NOMINAL),
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

#define F_PDM(f, s, d, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_SRC_SEL(1, 0, s##_to_xo_mux), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_pdm[] = {
	F_PDM(       0, gnd, 1, NONE),
	F_PDM(27000000, pxo, 1, LOW),
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
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "pdm_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(pdm_clk.c),
	},
};

static struct branch_clk pmem_clk = {
	.b = {
		.ctl_reg = PMEM_ACLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 20,
	},
	.c = {
		.dbg_name = "pmem_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pmem_clk.c),
	},
};

#define F_PRNG(f, s, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_prng[] = {
	F_PRNG(64000000, pll8, NOMINAL),
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
	.freq_tbl = clk_tbl_prng,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "prng_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(prng_clk.c),
	},
};

#define CLK_SDC(i, n, h_r, h_b) \
	struct rcg_clk i##_clk = { \
		.b = { \
			.ctl_reg = SDCn_APPS_CLK_NS_REG(n), \
			.en_mask = BIT(9), \
			.reset_reg = SDCn_RESET_REG(n), \
			.reset_mask = BIT(0), \
			.halt_reg = h_r, \
			.halt_bit = h_b, \
		}, \
		.ns_reg = SDCn_APPS_CLK_NS_REG(n), \
		.md_reg = SDCn_APPS_CLK_MD_REG(n), \
		.root_en_mask = BIT(11), \
		.ns_mask = (BM(23, 16) | BM(6, 0)), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_sdc, \
		.current_freq = &local_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &soc_clk_ops_8960, \
			CLK_INIT(i##_clk.c), \
		}, \
	}
#define F_SDC(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(16, m, 0, n), \
		.ns_val = NS(23, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_sdc[] = {
	F_SDC(        0, gnd,   1, 0,   0, NONE),
	F_SDC(   144000, pxo,   3, 2, 125, LOW),
	F_SDC(   400000, pll8,  4, 1, 240, LOW),
	F_SDC( 16000000, pll8,  4, 1,   6, LOW),
	F_SDC( 17070000, pll8,  1, 2,  45, LOW),
	F_SDC( 20210000, pll8,  1, 1,  19, LOW),
	F_SDC( 24000000, pll8,  4, 1,   4, LOW),
	F_SDC( 48000000, pll8,  4, 1,   2, NOMINAL),
	F_SDC( 64000000, pll8,  3, 1,   2, NOMINAL),
	F_SDC( 96000000, pll8,  4, 0,   0, NOMINAL),
	F_SDC(192000000, pll8,  2, 0,   0, NOMINAL),
	F_END
};

static CLK_SDC(sdc1, 1, CLK_HALT_DFAB_STATE_REG, 6);
static CLK_SDC(sdc2, 2, CLK_HALT_DFAB_STATE_REG, 5);
static CLK_SDC(sdc3, 3, CLK_HALT_DFAB_STATE_REG, 4);
static CLK_SDC(sdc4, 4, CLK_HALT_DFAB_STATE_REG, 3);
static CLK_SDC(sdc5, 5, CLK_HALT_DFAB_STATE_REG, 2);

#define F_TSIF_REF(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD16(m, n), \
		.ns_val = NS(31, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_tsif_ref[] = {
	F_TSIF_REF(     0, gnd,  1, 0,   0, NONE),
	F_TSIF_REF(105000, pxo,  1, 1, 256, LOW),
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
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_tsif_ref,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "tsif_ref_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(tsif_ref_clk.c),
	},
};

#define F_TSSC(f, s, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_SRC_SEL(1, 0, s##_to_xo_mux), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_tssc[] = {
	F_TSSC(       0, gnd, NONE),
	F_TSSC(27000000, pxo, LOW),
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
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "tssc_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(tssc_clk.c),
	},
};

#define F_USB(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(16, m, 0, n), \
		.ns_val = NS(23, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_usb[] = {
	F_USB(       0, gnd,  1, 0,  0, NONE),
	F_USB(60000000, pll8, 1, 5, 32, NOMINAL),
	F_END
};

static struct rcg_clk usb_hs1_xcvr_clk = {
	.b = {
		.ctl_reg = USB_HS1_XCVR_FS_CLK_NS_REG,
		.en_mask = BIT(9),
		.reset_reg = USB_HS1_RESET_REG,
		.reset_mask = BIT(0),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 0,
	},
	.ns_reg = USB_HS1_XCVR_FS_CLK_NS_REG,
	.md_reg = USB_HS1_XCVR_FS_CLK_MD_REG,
	.root_en_mask = BIT(11),
	.ns_mask = (BM(23, 16) | BM(6, 0)),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_usb,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "usb_hs1_xcvr_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(usb_hs1_xcvr_clk.c),
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

#define CLK_USB_FS(i, n) \
	struct rcg_clk i##_clk = { \
		.ns_reg = USB_FSn_XCVR_FS_CLK_NS_REG(n), \
		.b = { \
			.ctl_reg = USB_FSn_XCVR_FS_CLK_NS_REG(n), \
			.halt_check = NOCHECK, \
		}, \
		.md_reg = USB_FSn_XCVR_FS_CLK_MD_REG(n), \
		.root_en_mask = BIT(11), \
		.ns_mask = (BM(23, 16) | BM(6, 0)), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_usb, \
		.current_freq = &local_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &soc_clk_ops_8960, \
			CLK_INIT(i##_clk.c), \
		}, \
	}

static CLK_USB_FS(usb_fs1_src, 1);
static struct branch_clk usb_fs1_xcvr_clk = {
	.b = {
		.ctl_reg = USB_FSn_XCVR_FS_CLK_NS_REG(1),
		.en_mask = BIT(9),
		.reset_reg = USB_FSn_RESET_REG(1),
		.reset_mask = BIT(1),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 15,
	},
	.parent = &usb_fs1_src_clk.c,
	.c = {
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
	.parent = &usb_fs1_src_clk.c,
	.c = {
		.dbg_name = "usb_fs1_sys_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_fs1_sys_clk.c),
	},
};

static CLK_USB_FS(usb_fs2_src, 2);
static struct branch_clk usb_fs2_xcvr_clk = {
	.b = {
		.ctl_reg = USB_FSn_XCVR_FS_CLK_NS_REG(2),
		.en_mask = BIT(9),
		.reset_reg = USB_FSn_RESET_REG(2),
		.reset_mask = BIT(1),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 12,
	},
	.parent = &usb_fs2_src_clk.c,
	.c = {
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
	.parent = &usb_fs2_src_clk.c,
	.c = {
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

static struct branch_clk dma_bam_p_clk = {
	.b = {
		.ctl_reg = DMA_BAM_HCLK_CTL,
		.en_mask = BIT(4),
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
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 14,
	},
	.c = {
		.dbg_name = "gsbi12_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gsbi12_p_clk.c),
	},
};

static struct branch_clk tsif_p_clk = {
	.b = {
		.ctl_reg = TSIF_HCLK_CTL_REG,
		.en_mask = BIT(4),
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
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 1,
	},
	.c = {
		.dbg_name = "usb_hs1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hs1_p_clk.c),
	},
};

static struct branch_clk sdc1_p_clk = {
	.b = {
		.ctl_reg = SDCn_HCLK_CTL_REG(1),
		.en_mask = BIT(4),
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

static struct branch_clk amp_clk = {
	.b = {
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(20),
	},
	.c = {
		.dbg_name = "amp_clk",
		.ops = &clk_ops_reset,
		CLK_INIT(amp_clk.c),
	},
};

#define CLK_CAM(i, n, hb) \
	struct rcg_clk i##_clk = { \
		.b = { \
			.ctl_reg = CAMCLKn_CC_REG(n), \
			.en_mask = BIT(0), \
			.halt_reg = DBG_BUS_VEC_I_REG, \
			.halt_bit = hb, \
		}, \
		.ns_reg = CAMCLKn_NS_REG(n), \
		.md_reg = CAMCLKn_MD_REG(n), \
		.root_en_mask = BIT(2), \
		.ns_mask = (BM(31, 24) | BM(15, 14) | BM(2, 0)), \
		.ctl_mask = BM(7, 6), \
		.set_rate = set_rate_mnd_8, \
		.freq_tbl = clk_tbl_cam, \
		.current_freq = &local_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &soc_clk_ops_8960, \
			CLK_INIT(i##_clk.c), \
		}, \
	}
#define F_CAM(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(31, 24, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
		.mnd_en_mask = BIT(5) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_cam[] = {
	F_CAM(        0, gnd,  1, 0,  0, NONE),
	F_CAM(  6000000, pll8, 4, 1, 16, LOW),
	F_CAM(  8000000, pll8, 4, 1, 12, LOW),
	F_CAM( 12000000, pll8, 4, 1,  8, LOW),
	F_CAM( 16000000, pll8, 4, 1,  6, LOW),
	F_CAM( 19200000, pll8, 4, 1,  5, LOW),
	F_CAM( 24000000, pll8, 4, 1,  4, LOW),
	F_CAM( 32000000, pll8, 4, 1,  3, LOW),
	F_CAM( 48000000, pll8, 4, 1,  2, LOW),
	F_CAM( 64000000, pll8, 3, 1,  2, LOW),
	F_CAM( 96000000, pll8, 4, 0,  0, NOMINAL),
	F_CAM(128000000, pll8, 3, 0,  0, NOMINAL),
	F_END
};

static CLK_CAM(cam0, 0, 15);
static CLK_CAM(cam1, 1, 16);

#define F_CSI(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(31, 24, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
		.mnd_en_mask = BIT(5) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_csi[] = {
	F_CSI(        0, gnd,  1, 0, 0, NONE),
	F_CSI( 85330000, pll8, 1, 2, 9, LOW),
	F_CSI(177780000, pll2, 1, 2, 9, NOMINAL),
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
	.ns_mask = BM(31, 24) | BM(15, 12) | BM(2, 0),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_csi,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "csi0_src_clk",
		.ops = &soc_clk_ops_8960,
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
	.parent = &csi0_src_clk.c,
	.c = {
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
	.parent = &csi0_src_clk.c,
	.c = {
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
	.ns_mask = BM(31, 24) | BM(15, 12) | BM(2, 0),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_csi,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "csi1_src_clk",
		.ops = &soc_clk_ops_8960,
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
	.parent = &csi1_src_clk.c,
	.c = {
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
	.parent = &csi1_src_clk.c,
	.c = {
		.dbg_name = "csi1_phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi1_phy_clk.c),
	},
};

#define F_CSI_PIX(s) \
	{ \
		.src_clk = &csi##s##_clk.c, \
		.freq_hz = s, \
		.ns_val = BVAL(25, 25, s), \
	}
static struct clk_freq_tbl clk_tbl_csi_pix[] = {
	F_CSI_PIX(0), /* CSI0 source */
	F_CSI_PIX(1), /* CSI1 source */
	F_END
};

#define F_CSI_RDI(s) \
	{ \
		.src_clk = &csi##s##_clk.c, \
		.freq_hz = s, \
		.ns_val = BVAL(12, 12, s), \
	}
static struct clk_freq_tbl clk_tbl_csi_rdi[] = {
	F_CSI_RDI(0), /* CSI0 source */
	F_CSI_RDI(1), /* CSI1 source */
	F_END
};

static struct rcg_clk csi_pix_clk = {
	.b = {
		.ctl_reg = MISC_CC_REG,
		.en_mask = BIT(26),
		.halt_check = DELAY,
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(26),
	},
	.ns_reg = MISC_CC_REG,
	.ns_mask = BIT(25),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_csi_pix,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "csi_pix_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(csi_pix_clk.c),
	},
};

static struct rcg_clk csi_rdi_clk = {
	.b = {
		.ctl_reg = MISC_CC_REG,
		.en_mask = BIT(13),
		.halt_check = DELAY,
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(27),
	},
	.ns_reg = MISC_CC_REG,
	.ns_mask = BIT(12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_csi_rdi,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "csi_rdi_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(csi_rdi_clk.c),
	},
};

#define F_CSI_PHYTIMER(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(31, 24, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
		.mnd_en_mask = BIT(5) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_csi_phytimer[] = {
	F_CSI_PHYTIMER(        0, gnd,  1, 0, 0, NONE),
	F_CSI_PHYTIMER( 85330000, pll8, 1, 2, 9, LOW),
	F_CSI_PHYTIMER(177780000, pll2, 1, 2, 9, NOMINAL),
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
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd_8,
	.freq_tbl = clk_tbl_csi_phytimer,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "csiphy_timer_src_clk",
		.ops = &soc_clk_ops_8960,
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
	.parent = &csiphy_timer_src_clk.c,
	.c = {
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
	.parent = &csiphy_timer_src_clk.c,
	.c = {
		.dbg_name = "csi1phy_timer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi1phy_timer_clk.c),
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

static struct rcg_clk dsi1_byte_clk = {
	.b = {
		.ctl_reg = DSI1_BYTE_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(7),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 21,
	},
	.ns_reg = DSI1_BYTE_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(15, 12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_dsi_byte,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "dsi1_byte_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(dsi1_byte_clk.c),
	},
};

static struct rcg_clk dsi2_byte_clk = {
	.b = {
		.ctl_reg = DSI2_BYTE_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(25),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 20,
	},
	.ns_reg = DSI2_BYTE_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(15, 12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_dsi_byte,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "dsi2_byte_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(dsi2_byte_clk.c),
	},
};

static struct rcg_clk dsi1_esc_clk = {
	.b = {
		.ctl_reg = DSI1_ESC_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 1,
	},
	.ns_reg = DSI1_ESC_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(15, 12),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_dsi_byte,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "dsi1_esc_clk",
		.ops = &soc_clk_ops_8960,
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
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "dsi2_esc_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(dsi2_esc_clk.c),
	},
};

#define F_GFX2D(f, s, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD4(4, m, 0, n), \
		.ns_val = NS_MND_BANKED4(20, 16, n, m, 3, 0, s##_to_mm_mux), \
		.ctl_val = CC_BANKED(9, 6, n), \
		.mnd_en_mask = (BIT(8) | BIT(5)) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_gfx2d[] = {
	F_GFX2D(        0, gnd,  0,  0, NONE),
	F_GFX2D( 27000000, pxo,  0,  0, LOW),
	F_GFX2D( 48000000, pll8, 1,  8, LOW),
	F_GFX2D( 54857000, pll8, 1,  7, LOW),
	F_GFX2D( 64000000, pll8, 1,  6, LOW),
	F_GFX2D( 76800000, pll8, 1,  5, LOW),
	F_GFX2D( 96000000, pll8, 1,  4, LOW),
	F_GFX2D(128000000, pll8, 1,  3, NOMINAL),
	F_GFX2D(145455000, pll2, 2, 11, NOMINAL),
	F_GFX2D(160000000, pll2, 1,  5, NOMINAL),
	F_GFX2D(177778000, pll2, 2,  9, NOMINAL),
	F_GFX2D(200000000, pll2, 1,  4, NOMINAL),
	F_GFX2D(228571000, pll2, 2,  7, HIGH),
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
	},
	.ns_reg = GFX2D0_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.freq_tbl = clk_tbl_gfx2d,
	.bank_masks = &bmnd_info_gfx2d0,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "gfx2d0_clk",
		.ops = &soc_clk_ops_8960,
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
	},
	.ns_reg = GFX2D1_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.freq_tbl = clk_tbl_gfx2d,
	.bank_masks = &bmnd_info_gfx2d1,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "gfx2d1_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(gfx2d1_clk.c),
	},
};

#define F_GFX3D(f, s, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD4(4, m, 0, n), \
		.ns_val = NS_MND_BANKED4(18, 14, n, m, 3, 0, s##_to_mm_mux), \
		.ctl_val = CC_BANKED(9, 6, n), \
		.mnd_en_mask = (BIT(8) | BIT(5)) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_gfx3d[] = {
	F_GFX3D(        0, gnd,  0,  0, NONE),
	F_GFX3D( 27000000, pxo,  0,  0, LOW),
	F_GFX3D( 48000000, pll8, 1,  8, LOW),
	F_GFX3D( 54857000, pll8, 1,  7, LOW),
	F_GFX3D( 64000000, pll8, 1,  6, LOW),
	F_GFX3D( 76800000, pll8, 1,  5, LOW),
	F_GFX3D( 96000000, pll8, 1,  4, LOW),
	F_GFX3D(128000000, pll8, 1,  3, NOMINAL),
	F_GFX3D(145455000, pll2, 2, 11, NOMINAL),
	F_GFX3D(160000000, pll2, 1,  5, NOMINAL),
	F_GFX3D(177778000, pll2, 2,  9, NOMINAL),
	F_GFX3D(200000000, pll2, 1,  4, NOMINAL),
	F_GFX3D(228571000, pll2, 2,  7, NOMINAL),
	F_GFX3D(266667000, pll2, 1,  3, NOMINAL),
	F_GFX3D(320000000, pll2, 2,  5, HIGH),
	F_END
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
	},
	.ns_reg = GFX3D_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.freq_tbl = clk_tbl_gfx3d,
	.bank_masks = &bmnd_info_gfx3d,
	.depends = &gmem_axi_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "gfx3d_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(gfx3d_clk.c),
	},
};

#define F_IJPEG(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(23, 16, n, m, 15, 12, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
		.mnd_en_mask = BIT(5) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_ijpeg[] = {
	F_IJPEG(        0, gnd,  1, 0,  0, NONE),
	F_IJPEG( 27000000, pxo,  1, 0,  0, LOW),
	F_IJPEG( 36570000, pll8, 1, 2, 21, LOW),
	F_IJPEG( 54860000, pll8, 7, 0,  0, LOW),
	F_IJPEG( 96000000, pll8, 4, 0,  0, LOW),
	F_IJPEG(109710000, pll8, 1, 2,  7, LOW),
	F_IJPEG(128000000, pll8, 3, 0,  0, NOMINAL),
	F_IJPEG(153600000, pll8, 1, 2,  5, NOMINAL),
	F_IJPEG(200000000, pll2, 4, 0,  0, NOMINAL),
	F_IJPEG(228571000, pll2, 1, 2,  7, NOMINAL),
	F_END
};

static struct rcg_clk ijpeg_clk = {
	.b = {
		.ctl_reg = IJPEG_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(9),
		.halt_reg = DBG_BUS_VEC_A_REG,
		.halt_bit = 24,
	},
	.ns_reg = IJPEG_NS_REG,
	.md_reg = IJPEG_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(23, 16) | BM(15, 12) | BM(2, 0)),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_ijpeg,
	.depends = &ijpeg_axi_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "ijpeg_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(ijpeg_clk.c),
	},
};

#define F_JPEGD(f, s, d, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_DIVSRC(15, 12, d, 2, 0, s##_to_mm_mux), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_jpegd[] = {
	F_JPEGD(        0, gnd,  1, NONE),
	F_JPEGD( 64000000, pll8, 6, LOW),
	F_JPEGD( 76800000, pll8, 5, LOW),
	F_JPEGD( 96000000, pll8, 4, LOW),
	F_JPEGD(160000000, pll2, 5, NOMINAL),
	F_JPEGD(200000000, pll2, 4, NOMINAL),
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
	},
	.ns_reg = JPEGD_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask =  (BM(15, 12) | BM(2, 0)),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_jpegd,
	.depends = &jpegd_axi_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "jpegd_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(jpegd_clk.c),
	},
};

#define F_MDP(f, s, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MND_BANKED8(22, 14, n, m, 3, 0, s##_to_mm_mux), \
		.ctl_val = CC_BANKED(9, 6, n), \
		.mnd_en_mask = (BIT(8) | BIT(5)) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_mdp[] = {
	F_MDP(        0, gnd,  0,  0, NONE),
	F_MDP(  9600000, pll8, 1, 40, LOW),
	F_MDP( 13710000, pll8, 1, 28, LOW),
	F_MDP( 27000000, pxo,  0,  0, LOW),
	F_MDP( 29540000, pll8, 1, 13, LOW),
	F_MDP( 34910000, pll8, 1, 11, LOW),
	F_MDP( 38400000, pll8, 1, 10, LOW),
	F_MDP( 59080000, pll8, 2, 13, LOW),
	F_MDP( 76800000, pll8, 1,  5, LOW),
	F_MDP( 85330000, pll8, 2,  9, LOW),
	F_MDP( 96000000, pll8, 1,  4, NOMINAL),
	F_MDP(128000000, pll8, 1,  3, NOMINAL),
	F_MDP(160000000, pll2, 1,  5, NOMINAL),
	F_MDP(177780000, pll2, 2,  9, NOMINAL),
	F_MDP(200000000, pll2, 1,  4, NOMINAL),
	F_END
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
	},
	.ns_reg = MDP_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.freq_tbl = clk_tbl_mdp,
	.bank_masks = &bmnd_info_mdp,
	.depends = &mdp_axi_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "mdp_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(mdp_clk.c),
	},
};

static struct branch_clk lut_mdp_clk = {
	.b = {
		.ctl_reg = MDP_LUT_CC_REG,
		.en_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 13,
	},
	.parent = &mdp_clk.c,
	.c = {
		.dbg_name = "lut_mdp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(lut_mdp_clk.c),
	},
};

#define F_MDP_VSYNC(f, s, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_SRC_SEL(13, 13, s##_to_bb_mux), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_mdp_vsync[] = {
	F_MDP_VSYNC(27000000, pxo, LOW),
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
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "mdp_vsync_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(mdp_vsync_clk.c),
	},
};

#define F_ROT(f, s, d, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_DIVSRC_BANKED(29, 26, 25, 22, d, \
				21, 19, 18, 16, s##_to_mm_mux), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_rot[] = {
	F_ROT(        0, gnd,   1, NONE),
	F_ROT( 27000000, pxo,   1, LOW),
	F_ROT( 29540000, pll8, 13, LOW),
	F_ROT( 32000000, pll8, 12, LOW),
	F_ROT( 38400000, pll8, 10, LOW),
	F_ROT( 48000000, pll8,  8, LOW),
	F_ROT( 54860000, pll8,  7, LOW),
	F_ROT( 64000000, pll8,  6, LOW),
	F_ROT( 76800000, pll8,  5, LOW),
	F_ROT( 96000000, pll8,  4, NOMINAL),
	F_ROT(100000000, pll2,  8, NOMINAL),
	F_ROT(114290000, pll2,  7, NOMINAL),
	F_ROT(133330000, pll2,  6, NOMINAL),
	F_ROT(160000000, pll2,  5, NOMINAL),
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
	},
	.ns_reg = ROT_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_div_banked,
	.freq_tbl = clk_tbl_rot,
	.bank_masks = &bdiv_info_rot,
	.current_freq = &local_dummy_freq,
	.depends = &rot_axi_clk.c,
	.c = {
		.dbg_name = "rot_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(rot_clk.c),
	},
};

static int hdmi_pll_clk_enable(struct clk *clk)
{
	int ret;
	unsigned long flags;
	spin_lock_irqsave(&local_clock_reg_lock, flags);
	ret = hdmi_pll_enable();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
	return ret;
}

static void hdmi_pll_clk_disable(struct clk *clk)
{
	unsigned long flags;
	spin_lock_irqsave(&local_clock_reg_lock, flags);
	hdmi_pll_disable();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

static unsigned hdmi_pll_clk_get_rate(struct clk *clk)
{
	return hdmi_pll_get_rate();
}

static struct clk_ops clk_ops_hdmi_pll = {
	.enable = hdmi_pll_clk_enable,
	.disable = hdmi_pll_clk_disable,
	.get_rate = hdmi_pll_clk_get_rate,
	.is_local = local_clk_is_local,
};

static struct clk hdmi_pll_clk = {
	.dbg_name = "hdmi_pll_clk",
	.ops = &clk_ops_hdmi_pll,
	CLK_INIT(hdmi_pll_clk),
};

#define F_TV_GND(f, s, p_r, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(23, 16, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
		.mnd_en_mask = BIT(5) * !!(n), \
		.sys_vdd = v, \
	}
#define F_TV(f, s, p_r, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(23, 16, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
		.mnd_en_mask = BIT(5) * !!(n), \
		.sys_vdd = v, \
		.extra_freq_data = (void *)p_r, \
	}
/* Switching TV freqs requires PLL reconfiguration. */
static struct clk_freq_tbl clk_tbl_tv[] = {
	F_TV_GND(    0,      gnd,          0, 1, 0, 0, NONE),
	F_TV( 25200000, hdmi_pll,   25200000, 1, 0, 0, LOW),
	F_TV( 27000000, hdmi_pll,   27000000, 1, 0, 0, LOW),
	F_TV( 27030000, hdmi_pll,   27030000, 1, 0, 0, LOW),
	F_TV( 74250000, hdmi_pll,   74250000, 1, 0, 0, NOMINAL),
	F_TV(148500000, hdmi_pll,  148500000, 1, 0, 0, NOMINAL),
	F_END
};

/*
 * Unlike other clocks, the TV rate is adjusted through PLL
 * re-programming. It is also routed through an MND divider.
 */
void set_rate_tv(struct rcg_clk *clk, struct clk_freq_tbl *nf)
{
	unsigned long pll_rate = (unsigned long)nf->extra_freq_data;
	if (pll_rate)
		hdmi_pll_set_rate(pll_rate);
	set_rate_mnd(clk, nf);
}

static struct rcg_clk tv_src_clk = {
	.ns_reg = TV_NS_REG,
	.b = {
		.ctl_reg = TV_CC_REG,
		.halt_check = NOCHECK,
	},
	.md_reg = TV_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(23, 16) | BM(15, 14) | BM(2, 0)),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_tv,
	.freq_tbl = clk_tbl_tv,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "tv_src_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(tv_src_clk.c),
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
	.parent = &tv_src_clk.c,
	.c = {
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
	.parent = &tv_src_clk.c,
	.c = {
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
	},
	.parent = &tv_src_clk.c,
	.c = {
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
	.parent = &tv_src_clk.c,
	.c = {
		.dbg_name = "hdmi_tv_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(hdmi_tv_clk.c),
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
#define F_VCODEC(f, s, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MND_BANKED8(11, 19, n, m, 0, 27, s##_to_mm_mux), \
		.ctl_val = CC_BANKED(6, 11, n), \
		.mnd_en_mask = (BIT(10) | BIT(5)) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_vcodec[] = {
	F_VCODEC(        0, gnd,  0,  0, NONE),
	F_VCODEC( 27000000, pxo,  0,  0, LOW),
	F_VCODEC( 32000000, pll8, 1, 12, LOW),
	F_VCODEC( 48000000, pll8, 1,  8, LOW),
	F_VCODEC( 54860000, pll8, 1,  7, LOW),
	F_VCODEC( 96000000, pll8, 1,  4, LOW),
	F_VCODEC(133330000, pll2, 1,  6, NOMINAL),
	F_VCODEC(200000000, pll2, 1,  4, NOMINAL),
	F_VCODEC(228570000, pll2, 2,  7, HIGH),
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
	},
	.ns_reg = VCODEC_NS_REG,
	.root_en_mask = BIT(2),
	.set_rate = set_rate_mnd_banked,
	.bank_masks = &bmnd_info_vcodec,
	.freq_tbl = clk_tbl_vcodec,
	.depends = &vcodec_axi_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "vcodec_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(vcodec_clk.c),
	},
};

#define F_VPE(f, s, d, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_DIVSRC(15, 12, d, 2, 0, s##_to_mm_mux), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_vpe[] = {
	F_VPE(        0, gnd,   1, NONE),
	F_VPE( 27000000, pxo,   1, LOW),
	F_VPE( 34909000, pll8, 11, LOW),
	F_VPE( 38400000, pll8, 10, LOW),
	F_VPE( 64000000, pll8,  6, LOW),
	F_VPE( 76800000, pll8,  5, LOW),
	F_VPE( 96000000, pll8,  4, NOMINAL),
	F_VPE(100000000, pll2,  8, NOMINAL),
	F_VPE(160000000, pll2,  5, NOMINAL),
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
	},
	.ns_reg = VPE_NS_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(15, 12) | BM(2, 0)),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_vpe,
	.current_freq = &local_dummy_freq,
	.depends = &vpe_axi_clk.c,
	.c = {
		.dbg_name = "vpe_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(vpe_clk.c),
	},
};

#define F_VFE(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(23, 16, n, m, 11, 10, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
		.mnd_en_mask = BIT(5) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_vfe[] = {
	F_VFE(        0, gnd,   1, 0,  0, NONE),
	F_VFE( 13960000, pll8,  1, 2, 55, LOW),
	F_VFE( 27000000, pxo,   1, 0,  0, LOW),
	F_VFE( 36570000, pll8,  1, 2, 21, LOW),
	F_VFE( 38400000, pll8,  2, 1,  5, LOW),
	F_VFE( 45180000, pll8,  1, 2, 17, LOW),
	F_VFE( 48000000, pll8,  2, 1,  4, LOW),
	F_VFE( 54860000, pll8,  1, 1,  7, LOW),
	F_VFE( 64000000, pll8,  2, 1,  3, LOW),
	F_VFE( 76800000, pll8,  1, 1,  5, LOW),
	F_VFE( 96000000, pll8,  2, 1,  2, LOW),
	F_VFE(109710000, pll8,  1, 2,  7, LOW),
	F_VFE(128000000, pll8,  1, 1,  3, NOMINAL),
	F_VFE(153600000, pll8,  1, 2,  5, NOMINAL),
	F_VFE(200000000, pll2,  2, 1,  2, NOMINAL),
	F_VFE(228570000, pll2,  1, 2,  7, NOMINAL),
	F_VFE(266667000, pll2,  1, 1,  3, NOMINAL),
	F_END
};


static struct rcg_clk vfe_clk = {
	.b = {
		.ctl_reg = VFE_CC_REG,
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(15),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 6,
		.en_mask = BIT(0),
	},
	.ns_reg = VFE_NS_REG,
	.md_reg = VFE_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(23, 16) | BM(11, 10) | BM(2, 0)),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_vfe,
	.depends = &vfe_axi_clk.c,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "vfe_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(vfe_clk.c),
	},
};

static struct branch_clk csi0_vfe_clk = {
	.b = {
		.ctl_reg = VFE_CC_REG,
		.en_mask = BIT(12),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(24),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 8,
	},
	.parent = &vfe_clk.c,
	.c = {
		.dbg_name = "csi0_vfe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi0_vfe_clk.c),
	},
};

/*
 * Low Power Audio Clocks
 */
#define F_AIF_OSR(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS(31, 24, n, m, 5, 4, 3, d, 2, 0, s##_to_lpa_mux), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_aif_osr[] = {
	F_AIF_OSR(       0, gnd,  1, 0,   0, NONE),
	F_AIF_OSR(  768000, pll4, 4, 1, 128, LOW),
	F_AIF_OSR( 1024000, pll4, 4, 1,  96, LOW),
	F_AIF_OSR( 1536000, pll4, 4, 1,  64, LOW),
	F_AIF_OSR( 2048000, pll4, 4, 1,  48, LOW),
	F_AIF_OSR( 3072000, pll4, 4, 1,  32, LOW),
	F_AIF_OSR( 4096000, pll4, 4, 1,  24, LOW),
	F_AIF_OSR( 6144000, pll4, 4, 1,  16, LOW),
	F_AIF_OSR( 8192000, pll4, 4, 1,  12, LOW),
	F_AIF_OSR(12288000, pll4, 4, 1,   8, LOW),
	F_AIF_OSR(24576000, pll4, 4, 1,   4, LOW),
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
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_aif_osr, \
		.current_freq = &local_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &soc_clk_ops_8960, \
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
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_aif_osr, \
		.current_freq = &local_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &soc_clk_ops_8960, \
			CLK_INIT(i##_clk.c), \
		}, \
	}

#define F_AIF_BIT(d, s) \
	{ \
		.freq_hz = d, \
		.ns_val = (BVAL(14, 14, s) | BVAL(13, 10, (d-1))) \
	}
static struct clk_freq_tbl clk_tbl_aif_bit[] = {
	F_AIF_BIT(0, 1),  /* Use external clock. */
	F_AIF_BIT(1, 0),  F_AIF_BIT(2, 0),  F_AIF_BIT(3, 0),  F_AIF_BIT(4, 0),
	F_AIF_BIT(5, 0),  F_AIF_BIT(6, 0),  F_AIF_BIT(7, 0),  F_AIF_BIT(8, 0),
	F_AIF_BIT(9, 0),  F_AIF_BIT(10, 0), F_AIF_BIT(11, 0), F_AIF_BIT(12, 0),
	F_AIF_BIT(13, 0), F_AIF_BIT(14, 0), F_AIF_BIT(15, 0), F_AIF_BIT(16, 0),
	F_END
};

#define CLK_AIF_BIT(i, ns, h_r) \
	struct rcg_clk i##_clk = { \
		.b = { \
			.ctl_reg = ns, \
			.en_mask = BIT(15), \
			.halt_reg = h_r, \
			.halt_check = DELAY, \
		}, \
		.ns_reg = ns, \
		.ns_mask = BM(14, 10), \
		.set_rate = set_rate_nop, \
		.freq_tbl = clk_tbl_aif_bit, \
		.current_freq = &local_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &soc_clk_ops_8960, \
			CLK_INIT(i##_clk.c), \
		}, \
	}

#define F_AIF_BIT_D(d, s) \
	{ \
		.freq_hz = d, \
		.ns_val = (BVAL(18, 18, s) | BVAL(17, 10, (d-1))) \
	}
static struct clk_freq_tbl clk_tbl_aif_bit_div[] = {
	F_AIF_BIT_D(0, 1),  /* Use external clock. */
	F_AIF_BIT_D(1, 0), F_AIF_BIT_D(2, 0), F_AIF_BIT_D(3, 0),
	F_AIF_BIT_D(4, 0), F_AIF_BIT_D(5, 0), F_AIF_BIT_D(6, 0),
	F_AIF_BIT_D(7, 0), F_AIF_BIT_D(8, 0), F_AIF_BIT_D(9, 0),
	F_AIF_BIT_D(10, 0), F_AIF_BIT_D(11, 0), F_AIF_BIT_D(12, 0),
	F_AIF_BIT_D(13, 0), F_AIF_BIT_D(14, 0), F_AIF_BIT_D(15, 0),
	F_AIF_BIT_D(16, 0),
	F_END
};

#define CLK_AIF_BIT_DIV(i, ns, h_r) \
	struct rcg_clk i##_clk = { \
		.b = { \
			.ctl_reg = ns, \
			.en_mask = BIT(19), \
			.halt_reg = h_r, \
			.halt_check = ENABLE, \
		}, \
		.ns_reg = ns, \
		.ns_mask = BM(18, 10), \
		.set_rate = set_rate_nop, \
		.freq_tbl = clk_tbl_aif_bit_div, \
		.current_freq = &local_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &soc_clk_ops_8960, \
			CLK_INIT(i##_clk.c), \
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

#define F_PCM(f, s, d, m, n, v) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD16(m, n), \
		.ns_val = NS(31, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_lpa_mux), \
		.mnd_en_mask = BIT(8) * !!(n), \
		.sys_vdd = v, \
	}
static struct clk_freq_tbl clk_tbl_pcm[] = {
	F_PCM(       0, gnd,  1, 0,   0, NONE),
	F_PCM(  512000, pll4, 4, 1, 192, LOW),
	F_PCM(  768000, pll4, 4, 1, 128, LOW),
	F_PCM( 1024000, pll4, 4, 1,  96, LOW),
	F_PCM( 1536000, pll4, 4, 1,  64, LOW),
	F_PCM( 2048000, pll4, 4, 1,  48, LOW),
	F_PCM( 3072000, pll4, 4, 1,  32, LOW),
	F_PCM( 4096000, pll4, 4, 1,  24, LOW),
	F_PCM( 6144000, pll4, 4, 1,  16, LOW),
	F_PCM( 8192000, pll4, 4, 1,  12, LOW),
	F_PCM(12288000, pll4, 4, 1,   8, LOW),
	F_PCM(24576000, pll4, 4, 1,   4, LOW),
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
	.ns_mask = (BM(31, 16) | BM(6, 0)),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_pcm,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "pcm_clk",
		.ops = &soc_clk_ops_8960,
		CLK_INIT(pcm_clk.c),
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
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_aif_osr,
	.current_freq = &local_dummy_freq,
	.c = {
		.dbg_name = "audio_slimbus_clk",
		.ops = &soc_clk_ops_8960,
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
	.parent = &audio_slimbus_clk.c,
	.c = {
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
	.parent = &sps_slimbus_clk.c,
	.c = {
		.dbg_name = "slimbus_xo_src_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(slimbus_xo_src_clk.c),
	},
};

DEFINE_CLK_RPM(afab_clk, afab_a_clk, APPS_FABRIC);
DEFINE_CLK_RPM(cfpb_clk, cfpb_a_clk, CFPB);
DEFINE_CLK_RPM(dfab_clk, dfab_a_clk, DAYTONA_FABRIC);
DEFINE_CLK_RPM(ebi1_clk, ebi1_a_clk, EBI1);
DEFINE_CLK_RPM(mmfab_clk, mmfab_a_clk, MM_FABRIC);
DEFINE_CLK_RPM(mmfpb_clk, mmfpb_a_clk, MMFPB);
DEFINE_CLK_RPM(sfab_clk, sfab_a_clk, SYSTEM_FABRIC);
DEFINE_CLK_RPM(sfpb_clk, sfpb_a_clk, SFPB);

static DEFINE_CLK_VOTER(dfab_dsps_clk, &dfab_clk.c);
static DEFINE_CLK_VOTER(dfab_usb_hs_clk, &dfab_clk.c);
static DEFINE_CLK_VOTER(dfab_sdc1_clk, &dfab_clk.c);
static DEFINE_CLK_VOTER(dfab_sdc2_clk, &dfab_clk.c);
static DEFINE_CLK_VOTER(dfab_sdc3_clk, &dfab_clk.c);
static DEFINE_CLK_VOTER(dfab_sdc4_clk, &dfab_clk.c);
static DEFINE_CLK_VOTER(dfab_sdc5_clk, &dfab_clk.c);
static DEFINE_CLK_VOTER(dfab_sps_clk, &dfab_clk.c);

static DEFINE_CLK_VOTER(ebi1_msmbus_clk, &ebi1_clk.c);
/*
 * TODO: replace dummy_clk below with ebi1_clk.c once the
 * bus driver starts voting on ebi1 rates.
 */
static DEFINE_CLK_VOTER(ebi1_adm_clk,    &dummy_clk);

#ifdef CONFIG_DEBUG_FS
struct measure_sel {
	u32 test_vector;
	struct clk *clk;
};

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
	{ TEST_PER_LS(0x5A), &gsbi8_uart_clk.c },
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
	{ TEST_PER_LS(0xA4), &ce1_core_clk.c },

	{ TEST_PER_HS(0x07), &afab_clk.c },
	{ TEST_PER_HS(0x07), &afab_a_clk.c },
	{ TEST_PER_HS(0x18), &sfab_clk.c },
	{ TEST_PER_HS(0x18), &sfab_a_clk.c },
	{ TEST_PER_HS(0x2A), &adm0_clk.c },
	{ TEST_PER_HS(0x34), &ebi1_clk.c },
	{ TEST_PER_HS(0x34), &ebi1_a_clk.c },

	{ TEST_MM_LS(0x00), &dsi1_byte_clk.c },
	{ TEST_MM_LS(0x01), &dsi2_byte_clk.c },
	{ TEST_MM_LS(0x02), &cam1_clk.c },
	{ TEST_MM_LS(0x06), &amp_p_clk.c },
	{ TEST_MM_LS(0x07), &csi0_p_clk.c },
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

	{ TEST_MM_HS(0x00), &csi0_clk.c },
	{ TEST_MM_HS(0x01), &csi1_clk.c },
	{ TEST_MM_HS(0x04), &csi0_vfe_clk.c },
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

	{ TEST_LPA(0x0F), &mi2s_bit_clk.c },
	{ TEST_LPA(0x10), &codec_i2s_mic_bit_clk.c },
	{ TEST_LPA(0x11), &codec_i2s_spkr_bit_clk.c },
	{ TEST_LPA(0x12), &spare_i2s_mic_bit_clk.c },
	{ TEST_LPA(0x13), &spare_i2s_spkr_bit_clk.c },
	{ TEST_LPA(0x14), &pcm_clk.c },
	{ TEST_LPA(0x1D), &audio_slimbus_clk.c },
};

static struct measure_sel *find_measure_sel(struct clk *clk)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(measure_mux); i++)
		if (measure_mux[i].clk == clk)
			return &measure_mux[i];
	return NULL;
}

static int measure_clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = 0;
	u32 clk_sel;
	struct measure_sel *p;
	unsigned long flags;

	if (!parent)
		return -EINVAL;

	p = find_measure_sel(parent);
	if (!p)
		return -EINVAL;

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Program the test vector. */
	clk_sel = p->test_vector & TEST_CLK_SEL_MASK;
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
	writel_relaxed(0x0, RINGOSC_TCXO_CTL_REG);
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
static unsigned measure_clk_get_rate(struct clk *clk)
{
	unsigned long flags;
	u32 pdm_reg_backup, ringosc_reg_backup;
	u64 raw_count_short, raw_count_full;
	unsigned ret;

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
	raw_count_full = run_measurement(0x10000);

	writel_relaxed(ringosc_reg_backup, RINGOSC_NS_REG);
	writel_relaxed(pdm_reg_backup, PDM_CLK_NS_REG);

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short)
		ret = 0;
	else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((0x10000 * 10) + 35));
		ret = raw_count_full;
	}

	/* Route dbg_hs_clk to PLLTEST.  300mV single-ended amplitude. */
	writel_relaxed(0x38F8, PLLTEST_PAD_CFG_REG);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return ret;
}
#else /* !CONFIG_DEBUG_FS */
static int measure_clk_set_parent(struct clk *clk, struct clk *parent)
{
	return -EINVAL;
}

static unsigned measure_clk_get_rate(struct clk *clk)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static struct clk_ops measure_clk_ops = {
	.set_parent = measure_clk_set_parent,
	.get_rate = measure_clk_get_rate,
	.is_local = local_clk_is_local,
};

static struct clk measure_clk = {
	.dbg_name = "measure_clk",
	.ops = &measure_clk_ops,
	CLK_INIT(measure_clk),
};

static struct clk_lookup msm_clocks_8960[] = {
	CLK_LOOKUP("cxo",		cxo_clk.c,		NULL),
	CLK_LOOKUP("pll2",		pll2_clk.c,		NULL),
	CLK_LOOKUP("pll8",		pll8_clk.c,		NULL),
	CLK_LOOKUP("pll4",		pll4_clk.c,		NULL),
	CLK_LOOKUP("measure",		measure_clk,		"debug"),

	CLK_LOOKUP("afab_clk",		afab_clk.c,	NULL),
	CLK_LOOKUP("afab_a_clk",	afab_a_clk.c,	NULL),
	CLK_LOOKUP("cfpb_clk",		cfpb_clk.c,	NULL),
	CLK_LOOKUP("cfpb_a_clk",	cfpb_a_clk.c,	NULL),
	CLK_LOOKUP("dfab_clk",		dfab_clk.c,	NULL),
	CLK_LOOKUP("dfab_a_clk",	dfab_a_clk.c,	NULL),
	CLK_LOOKUP("ebi1_clk",		ebi1_clk.c,	NULL),
	CLK_LOOKUP("ebi1_a_clk",	ebi1_a_clk.c,	NULL),
	CLK_LOOKUP("mmfab_clk",		mmfab_clk.c,	NULL),
	CLK_LOOKUP("mmfab_a_clk",	mmfab_a_clk.c,	NULL),
	CLK_LOOKUP("mmfpb_clk",		mmfpb_clk.c,	NULL),
	CLK_LOOKUP("mmfpb_a_clk",	mmfpb_a_clk.c,	NULL),
	CLK_LOOKUP("sfab_clk",		sfab_clk.c,	NULL),
	CLK_LOOKUP("sfab_a_clk",	sfab_a_clk.c,	NULL),
	CLK_LOOKUP("sfpb_clk",		sfpb_clk.c,	NULL),
	CLK_LOOKUP("sfpb_a_clk",	sfpb_a_clk.c,	NULL),

	CLK_LOOKUP("gsbi_uart_clk",	gsbi1_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi2_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi3_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi4_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi5_uart_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("uartdm_clk",	gsbi6_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi7_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi8_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi9_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi10_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi11_uart_clk.c,	NULL),
	CLK_LOOKUP("gsbi_uart_clk",	gsbi12_uart_clk.c,	NULL),
	CLK_LOOKUP("spi_clk",		gsbi1_qup_clk.c,	"spi_qsd.0"),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi2_qup_clk.c,	NULL),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi3_qup_clk.c,	"qup_i2c.3"),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi4_qup_clk.c,	"qup_i2c.4"),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi5_qup_clk.c,	NULL),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi6_qup_clk.c,	NULL),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi7_qup_clk.c,	NULL),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi8_qup_clk.c,	NULL),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi9_qup_clk.c,	NULL),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi10_qup_clk.c,	"qup_i2c.10"),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi11_qup_clk.c,	NULL),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi12_qup_clk.c,	"qup_i2c.12"),
	CLK_LOOKUP("pdm_clk",		pdm_clk.c,		NULL),
	CLK_LOOKUP("pmem_clk",		pmem_clk.c,		NULL),
	CLK_LOOKUP("prng_clk",		prng_clk.c,		NULL),
	CLK_LOOKUP("sdc_clk",		sdc1_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("sdc_clk",		sdc2_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("sdc_clk",		sdc3_clk.c,		"msm_sdcc.3"),
	CLK_LOOKUP("sdc_clk",		sdc4_clk.c,		"msm_sdcc.4"),
	CLK_LOOKUP("sdc_clk",		sdc5_clk.c,		"msm_sdcc.5"),
	CLK_LOOKUP("slimbus_xo_src_clk", slimbus_xo_src_clk.c,	NULL),
	CLK_LOOKUP("tsif_ref_clk",	tsif_ref_clk.c,		NULL),
	CLK_LOOKUP("tssc_clk",		tssc_clk.c,		NULL),
	CLK_LOOKUP("usb_hs_clk",	usb_hs1_xcvr_clk.c,	NULL),
	CLK_LOOKUP("usb_phy_clk",	usb_phy0_clk.c,		NULL),
	CLK_LOOKUP("usb_fs_clk",	usb_fs1_xcvr_clk.c,	NULL),
	CLK_LOOKUP("usb_fs_sys_clk",	usb_fs1_sys_clk.c,	NULL),
	CLK_LOOKUP("usb_fs_src_clk",	usb_fs1_src_clk.c,	NULL),
	CLK_LOOKUP("usb_fs_clk",	usb_fs2_xcvr_clk.c,	NULL),
	CLK_LOOKUP("usb_fs_sys_clk",	usb_fs2_sys_clk.c,	NULL),
	CLK_LOOKUP("usb_fs_src_clk",	usb_fs2_src_clk.c,	NULL),
	CLK_LOOKUP("ce_pclk",		ce1_p_clk.c,		NULL),
	CLK_LOOKUP("ce_clk",		ce1_core_clk.c,		NULL),
	CLK_LOOKUP("dma_bam_pclk",	dma_bam_p_clk.c,	NULL),
	CLK_LOOKUP("spi_pclk",		gsbi1_p_clk.c,		"spi_qsd.0"),
	CLK_LOOKUP("gsbi_pclk",		gsbi2_p_clk.c,		NULL),
	CLK_LOOKUP("gsbi_pclk",		gsbi3_p_clk.c,		"qup_i2c.3"),
	CLK_LOOKUP("gsbi_pclk",		gsbi4_p_clk.c, "qup_i2c.4"),
	CLK_LOOKUP("gsbi_pclk",		gsbi5_p_clk.c,	"msm_serial_hsl.0"),
	CLK_LOOKUP("uartdm_pclk",	gsbi6_p_clk.c,		NULL),
	CLK_LOOKUP("gsbi_pclk",		gsbi7_p_clk.c,		NULL),
	CLK_LOOKUP("gsbi_pclk",		gsbi8_p_clk.c,		NULL),
	CLK_LOOKUP("gsbi_pclk",		gsbi9_p_clk.c,		NULL),
	CLK_LOOKUP("gsbi_pclk",		gsbi10_p_clk.c,		"qup_i2c.10"),
	CLK_LOOKUP("gsbi_pclk",		gsbi11_p_clk.c,		NULL),
	CLK_LOOKUP("gsbi_pclk",		gsbi12_p_clk.c,		"qup_i2c.12"),
	CLK_LOOKUP("tsif_pclk",		tsif_p_clk.c,		NULL),
	CLK_LOOKUP("usb_fs_pclk",	usb_fs1_p_clk.c,	NULL),
	CLK_LOOKUP("usb_fs_pclk",	usb_fs2_p_clk.c,	NULL),
	CLK_LOOKUP("usb_hs_pclk",	usb_hs1_p_clk.c,	NULL),
	CLK_LOOKUP("sdc_pclk",		sdc1_p_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("sdc_pclk",		sdc2_p_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("sdc_pclk",		sdc3_p_clk.c,		"msm_sdcc.3"),
	CLK_LOOKUP("sdc_pclk",		sdc4_p_clk.c,		"msm_sdcc.4"),
	CLK_LOOKUP("sdc_pclk",		sdc5_p_clk.c,		"msm_sdcc.5"),
	CLK_LOOKUP("adm_clk",		adm0_clk.c,		NULL),
	CLK_LOOKUP("adm_pclk",		adm0_p_clk.c,		NULL),
	CLK_LOOKUP("pmic_arb_pclk",	pmic_arb0_p_clk.c,	NULL),
	CLK_LOOKUP("pmic_arb_pclk",	pmic_arb1_p_clk.c,	NULL),
	CLK_LOOKUP("pmic_ssbi2",	pmic_ssbi2_clk.c,	NULL),
	CLK_LOOKUP("rpm_msg_ram_pclk",	rpm_msg_ram_p_clk.c,	NULL),
	CLK_LOOKUP("amp_clk",		amp_clk.c,		NULL),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,		NULL),
	CLK_LOOKUP("cam_clk",		cam1_clk.c,		NULL),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"msm_camera_imx074.0"),
	CLK_LOOKUP("cam_clk",		cam0_clk.c,	"msm_camera_ov2720.0"),
	CLK_LOOKUP("cam_clk",	cam0_clk.c,	"msm_camera_qs_mt9p017.0"),
	CLK_LOOKUP("csi_src_clk",	csi0_src_clk.c,		NULL),
	CLK_LOOKUP("csi_src_clk",	csi1_src_clk.c,		NULL),
	CLK_LOOKUP("csi_src_clk",	csi0_src_clk.c,	"msm_camera_imx074.0"),
	CLK_LOOKUP("csi_src_clk", csi0_src_clk.c, "msm_camera_qs_mt9p017.0"),
	CLK_LOOKUP("csi_src_clk",	csi1_src_clk.c,	"msm_camera_ov2720.0"),
	CLK_LOOKUP("csi_clk",		csi0_clk.c,		NULL),
	CLK_LOOKUP("csi_clk",		csi1_clk.c,		NULL),
	CLK_LOOKUP("csi_clk",		csi0_clk.c,	"msm_camera_imx074.0"),
	CLK_LOOKUP("csi_clk",	csi0_clk.c,	"msm_camera_qs_mt9p017.0"),
	CLK_LOOKUP("csi_clk",		csi1_clk.c,	"msm_camera_ov2720.0"),
	CLK_LOOKUP("csi_phy_clk",	csi0_phy_clk.c,		NULL),
	CLK_LOOKUP("csi_phy_clk",	csi1_phy_clk.c,		NULL),
	CLK_LOOKUP("csi_phy_clk",	csi0_phy_clk.c,	"msm_camera_imx074.0"),
	CLK_LOOKUP("csi_phy_clk", csi0_phy_clk.c, "msm_camera_qs_mt9p017.0"),
	CLK_LOOKUP("csi_phy_clk",	csi1_phy_clk.c,	"msm_camera_ov2720.0"),
	CLK_LOOKUP("csi_pix_clk",	csi_pix_clk.c,		NULL),
	CLK_LOOKUP("csi_rdi_clk",	csi_rdi_clk.c,		NULL),
	CLK_LOOKUP("csiphy_timer_src_clk", csiphy_timer_src_clk.c, NULL),
	CLK_LOOKUP("csi0phy_timer_clk",	csi0phy_timer_clk.c,	NULL),
	CLK_LOOKUP("csi1phy_timer_clk",	csi1phy_timer_clk.c,	NULL),
	CLK_LOOKUP("dsi_byte_div_clk",	dsi1_byte_clk.c,	NULL),
	CLK_LOOKUP("dsi_byte_div_clk",	dsi2_byte_clk.c,	NULL),
	CLK_LOOKUP("dsi_esc_clk",	dsi1_esc_clk.c,		NULL),
	CLK_LOOKUP("dsi_esc_clk",	dsi2_esc_clk.c,		NULL),
	CLK_LOOKUP("gfx2d0_clk",	gfx2d0_clk.c,		NULL),
	CLK_LOOKUP("gfx2d1_clk",	gfx2d1_clk.c,		NULL),
	CLK_LOOKUP("gfx3d_clk",		gfx3d_clk.c,		NULL),
	CLK_LOOKUP("ijpeg_axi_clk",	ijpeg_axi_clk.c,	NULL),
	CLK_LOOKUP("imem_axi_clk",	imem_axi_clk.c,		NULL),
	CLK_LOOKUP("ijpeg_clk",         ijpeg_clk.c,            NULL),
	CLK_LOOKUP("jpegd_clk",		jpegd_clk.c,		NULL),
	CLK_LOOKUP("mdp_clk",		mdp_clk.c,		NULL),
	CLK_LOOKUP("mdp_vsync_clk",	mdp_vsync_clk.c,	NULL),
	CLK_LOOKUP("lut_mdp",		lut_mdp_clk.c,		NULL),
	CLK_LOOKUP("rot_clk",		rot_clk.c,		NULL),
	CLK_LOOKUP("tv_src_clk",	tv_src_clk.c,		NULL),
	CLK_LOOKUP("tv_enc_clk",	tv_enc_clk.c,		NULL),
	CLK_LOOKUP("tv_dac_clk",	tv_dac_clk.c,		NULL),
	CLK_LOOKUP("vcodec_clk",	vcodec_clk.c,		NULL),
	CLK_LOOKUP("mdp_tv_clk",	mdp_tv_clk.c,		NULL),
	CLK_LOOKUP("hdmi_clk",		hdmi_tv_clk.c,		NULL),
	CLK_LOOKUP("hdmi_app_clk",	hdmi_app_clk.c,		NULL),
	CLK_LOOKUP("vpe_clk",		vpe_clk.c,		NULL),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,		NULL),
	CLK_LOOKUP("csi_vfe_clk",	csi0_vfe_clk.c,		NULL),
	CLK_LOOKUP("vfe_axi_clk",	vfe_axi_clk.c,		NULL),
	CLK_LOOKUP("mdp_axi_clk",	mdp_axi_clk.c,		NULL),
	CLK_LOOKUP("rot_axi_clk",	rot_axi_clk.c,		NULL),
	CLK_LOOKUP("vcodec_axi_clk",	vcodec_axi_clk.c,	NULL),
	CLK_LOOKUP("vcodec_axi_a_clk",	vcodec_axi_a_clk.c,	NULL),
	CLK_LOOKUP("vcodec_axi_b_clk",	vcodec_axi_b_clk.c,	NULL),
	CLK_LOOKUP("vpe_axi_clk",	vpe_axi_clk.c,		NULL),
	CLK_LOOKUP("amp_pclk",		amp_p_clk.c,		NULL),
	CLK_LOOKUP("csi_pclk",		csi0_p_clk.c,		NULL),
	CLK_LOOKUP("dsi_m_pclk",	dsi1_m_p_clk.c,		NULL),
	CLK_LOOKUP("dsi_s_pclk",	dsi1_s_p_clk.c,		NULL),
	CLK_LOOKUP("dsi_m_pclk",	dsi2_m_p_clk.c,		NULL),
	CLK_LOOKUP("dsi_s_pclk",	dsi2_s_p_clk.c,		NULL),
	CLK_LOOKUP("gfx2d0_pclk",	gfx2d0_p_clk.c,		NULL),
	CLK_LOOKUP("gfx2d1_pclk",	gfx2d1_p_clk.c,		NULL),
	CLK_LOOKUP("gfx3d_pclk",	gfx3d_p_clk.c,		NULL),
	CLK_LOOKUP("hdmi_m_pclk",	hdmi_m_p_clk.c,		NULL),
	CLK_LOOKUP("hdmi_s_pclk",	hdmi_s_p_clk.c,		NULL),
	CLK_LOOKUP("ijpeg_pclk",	ijpeg_p_clk.c,		NULL),
	CLK_LOOKUP("jpegd_pclk",	jpegd_p_clk.c,		NULL),
	CLK_LOOKUP("imem_pclk",		imem_p_clk.c,		NULL),
	CLK_LOOKUP("mdp_pclk",		mdp_p_clk.c,		NULL),
	CLK_LOOKUP("smmu_pclk",		smmu_p_clk.c,		NULL),
	CLK_LOOKUP("rotator_pclk",	rot_p_clk.c,		NULL),
	CLK_LOOKUP("tv_enc_pclk",	tv_enc_p_clk.c,		NULL),
	CLK_LOOKUP("vcodec_pclk",	vcodec_p_clk.c,		NULL),
	CLK_LOOKUP("vfe_pclk",		vfe_p_clk.c,		NULL),
	CLK_LOOKUP("vpe_pclk",		vpe_p_clk.c,		NULL),
	CLK_LOOKUP("mi2s_bit_clk",	mi2s_bit_clk.c,		NULL),
	CLK_LOOKUP("mi2s_osr_clk",	mi2s_osr_clk.c,		NULL),
	CLK_LOOKUP("i2s_mic_bit_clk",	codec_i2s_mic_bit_clk.c,	NULL),
	CLK_LOOKUP("i2s_mic_osr_clk",	codec_i2s_mic_osr_clk.c,	NULL),
	CLK_LOOKUP("i2s_mic_bit_clk",	spare_i2s_mic_bit_clk.c,	NULL),
	CLK_LOOKUP("i2s_mic_osr_clk",	spare_i2s_mic_osr_clk.c,	NULL),
	CLK_LOOKUP("i2s_spkr_bit_clk",	codec_i2s_spkr_bit_clk.c,	NULL),
	CLK_LOOKUP("i2s_spkr_osr_clk",	codec_i2s_spkr_osr_clk.c,	NULL),
	CLK_LOOKUP("i2s_spkr_bit_clk",	spare_i2s_spkr_bit_clk.c,	NULL),
	CLK_LOOKUP("i2s_spkr_osr_clk",	spare_i2s_spkr_osr_clk.c,	NULL),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,		NULL),
	CLK_LOOKUP("sps_slimbus_clk",	sps_slimbus_clk.c,	NULL),
	CLK_LOOKUP("audio_slimbus_clk",	audio_slimbus_clk.c,	NULL),
	CLK_LOOKUP("iommu_clk",		jpegd_axi_clk.c,	"msm_iommu.0"),
	CLK_LOOKUP("iommu_clk",		vpe_axi_clk.c,		"msm_iommu.1"),
	CLK_LOOKUP("iommu_clk",		mdp_axi_clk.c,		"msm_iommu.2"),
	CLK_LOOKUP("iommu_clk",		mdp_axi_clk.c,		"msm_iommu.3"),
	CLK_LOOKUP("iommu_clk",		rot_axi_clk.c,		"msm_iommu.4"),
	CLK_LOOKUP("iommu_clk",		ijpeg_axi_clk.c,	"msm_iommu.5"),
	CLK_LOOKUP("iommu_clk",		vfe_axi_clk.c,		"msm_iommu.6"),
	CLK_LOOKUP("iommu_clk",		vcodec_axi_a_clk.c,	"msm_iommu.7"),
	CLK_LOOKUP("iommu_clk",		vcodec_axi_b_clk.c,	"msm_iommu.8"),
	CLK_LOOKUP("iommu_clk",		gfx3d_clk.c,		"msm_iommu.9"),
	CLK_LOOKUP("iommu_clk",		gfx2d0_clk.c,		"msm_iommu.10"),
	CLK_LOOKUP("iommu_clk",		gfx2d1_clk.c,		"msm_iommu.11"),
	CLK_LOOKUP("dfab_dsps_clk",	dfab_dsps_clk.c, NULL),
	CLK_LOOKUP("dfab_usb_hs_clk",	dfab_usb_hs_clk.c, NULL),
	CLK_LOOKUP("dfab_sdc_clk",	dfab_sdc1_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("dfab_sdc_clk",	dfab_sdc2_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("dfab_sdc_clk",	dfab_sdc3_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("dfab_sdc_clk",	dfab_sdc4_clk.c, "msm_sdcc.4"),
	CLK_LOOKUP("dfab_sdc_clk",	dfab_sdc5_clk.c, "msm_sdcc.5"),
	CLK_LOOKUP("dfab_clk",		dfab_sps_clk.c,	NULL /* sps */),

	CLK_LOOKUP("ebi1_msmbus_clk",	ebi1_msmbus_clk.c, NULL),
	CLK_LOOKUP("ebi1_clk",		ebi1_adm_clk.c, "msm_dmov"),
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

static void __init reg_init(void)
{
	/* TODO: Remove once LPASS starts voting */
	u32 reg;
	reg = readl_relaxed(BB_PLL_ENA_Q6_SW_REG);
	reg |= BIT(4);
	writel_relaxed(reg, BB_PLL_ENA_Q6_SW_REG);

	/* Setup LPASS toplevel muxes */
	writel_relaxed(0x15, LPASS_XO_SRC_CLK_CTL_REG); /* Select PXO */
	writel_relaxed(0x1, LCC_PXO_SRC_CLK_CTL_REG); /* Select PXO */
	writel_relaxed(0x1, LCC_PRI_PLL_CLK_CTL_REG); /* Select PLL4 */

	/* Deassert MM SW_RESET_ALL signal. */
	writel_relaxed(0, SW_RESET_ALL_REG);

	/* Initialize MM AHB registers: Enable the FPB clock and disable HW
	 * gating for all clocks. Also set VFE_AHB's FORCE_CORE_ON bit to
	 * prevent its memory from being collapsed when the clock is halted.
	 * The sleep and wake-up delays are set to safe values. */
	rmwreg(0x00000003, AHB_EN_REG,  0x0F7FFFFF);
	rmwreg(0x000007F9, AHB_EN2_REG, 0xFFFFBFFF);

	/* Deassert all locally-owned MM AHB resets. */
	rmwreg(0, SW_RESET_AHB_REG, 0xFFF7DFFF);

	/* Initialize MM AXI registers: Enable HW gating for all clocks that
	 * support it. Also set FORCE_CORE_ON bits, and any sleep and wake-up
	 * delays to safe values. */
	/* TODO: Enable HW Gating */
	rmwreg(0x000007F9, MAXI_EN_REG,  0x0FFFFFFF);
	rmwreg(0x1027FCFF, MAXI_EN2_REG, 0x1FFFFFFF);
	writel_relaxed(0x0027FCFF, MAXI_EN3_REG);
	writel_relaxed(0x0027FCFF, MAXI_EN4_REG);
	writel_relaxed(0x000003C7, SAXI_EN_REG);

	/* Initialize MM CC registers: Set MM FORCE_CORE_ON bits so that core
	 * memories retain state even when not clocked. Also, set sleep and
	 * wake-up delays to safe values. */
	writel_relaxed(0x00000000, CSI0_CC_REG);
	writel_relaxed(0x00000000, CSI1_CC_REG);
	rmwreg(0x80FF0000, DSI1_BYTE_CC_REG, BM(31, 29) | BM(23, 16));
	rmwreg(0x80FF0000, DSI2_BYTE_CC_REG, BM(31, 29) | BM(23, 16));
	rmwreg(0x80FF0000, DSI_PIXEL_CC_REG, BM(31, 29) | BM(23, 16));
	rmwreg(0x80FF0000, DSI2_PIXEL_CC_REG, BM(31, 29) | BM(23, 16));
	writel_relaxed(0x80FF0000, GFX2D0_CC_REG);
	writel_relaxed(0x80FF0000, GFX2D1_CC_REG);
	writel_relaxed(0x80FF0000, GFX3D_CC_REG);
	writel_relaxed(0x80FF0000, IJPEG_CC_REG);
	writel_relaxed(0x80FF0000, JPEGD_CC_REG);
	/* MDP clocks may be running at boot, don't turn them off. */
	rmwreg(0x80FF0000, MDP_CC_REG,   BM(31, 29) | BM(23, 16));
	rmwreg(0x80FF0000, MDP_LUT_CC_REG,  BM(31, 29) | BM(23, 16));
	writel_relaxed(0x80FF0000, ROT_CC_REG);
	writel_relaxed(0x80FF0000, TV_CC_REG);
	writel_relaxed(0x000004FF, TV_CC2_REG);
	writel_relaxed(0xC0FF0000, VCODEC_CC_REG);
	writel_relaxed(0x80FF0000, VFE_CC_REG);
	writel_relaxed(0x80FF0000, VPE_CC_REG);

	/* De-assert MM AXI resets to all hardware blocks. */
	writel_relaxed(0, SW_RESET_AXI_REG);

	/* Deassert all MM core resets. */
	writel_relaxed(0, SW_RESET_CORE_REG);

	/* Reset 3D core once more, with its clock enabled. This can
	 * eventually be done as part of the GDFS footswitch driver. */
	clk_set_rate(&gfx3d_clk.c, 27000000);
	clk_enable(&gfx3d_clk.c);
	writel_relaxed(BIT(12), SW_RESET_CORE_REG);
	mb();
	udelay(5);
	writel_relaxed(0, SW_RESET_CORE_REG);
	/* Make sure reset is de-asserted before clock is disabled. */
	mb();
	clk_disable(&gfx3d_clk.c);

	/* Enable TSSC and PDM PXO sources. */
	writel_relaxed(BIT(11), TSSC_CLK_CTL_REG);
	writel_relaxed(BIT(15), PDM_CLK_NS_REG);

	/* Source SLIMBus xo src from slimbus reference clock */
	writel_relaxed(0x3, SLIMBUS_XO_SRC_CLK_CTL_REG);

	/* Source the dsi_byte_clks from the DSI PHY PLLs */
	rmwreg(0x1, DSI1_BYTE_NS_REG, 0x7);
	rmwreg(0x2, DSI2_BYTE_NS_REG, 0x7);
}

static int wr_pll_clk_enable(struct clk *clk)
{
	u32 mode;
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(clk);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	mode = readl_relaxed(pll->mode_reg);
	/* De-assert active-low PLL reset. */
	mode |= BIT(2);
	writel_relaxed(mode, pll->mode_reg);

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset. Delay 10us just to be safe.
	 */
	mb();
	udelay(10);

	/* Disable PLL bypass mode. */
	mode |= BIT(1);
	writel_relaxed(mode, pll->mode_reg);

	/* Wait until PLL is locked. */
	mb();
	udelay(60);

	/* Enable PLL output. */
	mode |= BIT(0);
	writel_relaxed(mode, pll->mode_reg);

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
	return 0;
}

void __init msm8960_clock_init_dummy(void)
{
	soc_update_sys_vdd = msm8960_update_sys_vdd;
	local_vote_sys_vdd(HIGH);
	msm_clock_init(msm_clocks_8960_dummy, msm_num_clocks_8960_dummy);
}

/* Local clock driver initialization. */
void __init msm8960_clock_init(void)
{
	xo_pxo = msm_xo_get(MSM_XO_PXO, "clock-8960");
	if (IS_ERR(xo_pxo)) {
		pr_err("%s: msm_xo_get(PXO) failed.\n", __func__);
		BUG();
	}
	xo_cxo = msm_xo_get(MSM_XO_TCXO_D0, "clock-8960");
	if (IS_ERR(xo_cxo)) {
		pr_err("%s: msm_xo_get(CXO) failed.\n", __func__);
		BUG();
	}

	soc_update_sys_vdd = msm8960_update_sys_vdd;
	local_vote_sys_vdd(HIGH);

	clk_ops_pll.enable = wr_pll_clk_enable;

	/* Initialize clock registers. */
	reg_init();

	/* Initialize rates for clocks that only support one. */
	clk_set_rate(&pdm_clk.c, 27000000);
	clk_set_rate(&prng_clk.c, 64000000);
	clk_set_rate(&mdp_vsync_clk.c, 27000000);
	clk_set_rate(&tsif_ref_clk.c, 105000);
	clk_set_rate(&tssc_clk.c, 27000000);
	clk_set_rate(&usb_hs1_xcvr_clk.c, 60000000);
	clk_set_rate(&usb_fs1_src_clk.c, 60000000);
	clk_set_rate(&usb_fs2_src_clk.c, 60000000);

	/*
	 * The halt status bits for PDM and TSSC may be incorrect at boot.
	 * Toggle these clocks on and off to refresh them.
	 */
	rcg_clk_enable(&pdm_clk.c);
	rcg_clk_disable(&pdm_clk.c);
	rcg_clk_enable(&tssc_clk.c);
	rcg_clk_disable(&tssc_clk.c);

	if (machine_is_msm8960_sim()) {
		clk_set_rate(&sdc1_clk.c, 48000000);
		clk_enable(&sdc1_clk.c);
		clk_enable(&sdc1_p_clk.c);
		clk_set_rate(&sdc3_clk.c, 48000000);
		clk_enable(&sdc3_clk.c);
		clk_enable(&sdc3_p_clk.c);
	}

	msm_clock_init(msm_clocks_8960, ARRAY_SIZE(msm_clocks_8960));
}

static int __init msm_clk_soc_late_init(void)
{
	return local_unvote_sys_vdd(HIGH);
}
late_initcall(msm_clk_soc_late_init);

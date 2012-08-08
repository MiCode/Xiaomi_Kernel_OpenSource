/* Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/clkdev.h>

#include <asm/mach-types.h>

#include <mach/msm_iomap.h>
#include <mach/clk.h>
#include <mach/rpm-9615.h>
#include <mach/rpm-regulator.h>

#include "clock-local.h"
#include "clock-voter.h"
#include "clock-rpm.h"
#include "devices.h"
#include "clock-pll.h"

#define REG(off)	(MSM_CLK_CTL_BASE + (off))
#define REG_LPA(off)	(MSM_LPASS_CLK_CTL_BASE + (off))
#define REG_GCC(off)	(MSM_APCS_GCC_BASE + (off))

/* Peripheral clock registers. */
#define CE1_HCLK_CTL_REG			REG(0x2720)
#define CE1_CORE_CLK_CTL_REG			REG(0x2724)
#define DMA_BAM_HCLK_CTL			REG(0x25C0)
#define CLK_HALT_CFPB_STATEA_REG		REG(0x2FCC)
#define CLK_HALT_CFPB_STATEB_REG		REG(0x2FD0)
#define CLK_HALT_CFPB_STATEC_REG		REG(0x2FD4)
#define CLK_HALT_DFAB_STATE_REG			REG(0x2FC8)

#define CLK_HALT_MSS_KPSS_MISC_STATE_REG	REG(0x2FDC)
#define CLK_HALT_SFPB_MISC_STATE_REG		REG(0x2FD8)
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
#define BB_PLL_ENA_SC0_REG			REG(0x34C0)

#define BB_PLL0_L_VAL_REG			REG(0x30C4)
#define BB_PLL0_M_VAL_REG			REG(0x30C8)
#define BB_PLL0_MODE_REG			REG(0x30C0)
#define BB_PLL0_N_VAL_REG			REG(0x30CC)
#define BB_PLL0_STATUS_REG			REG(0x30D8)
#define BB_PLL0_CONFIG_REG			REG(0x30D4)
#define BB_PLL0_TEST_CTL_REG			REG(0x30D0)

#define BB_PLL8_L_VAL_REG			REG(0x3144)
#define BB_PLL8_M_VAL_REG			REG(0x3148)
#define BB_PLL8_MODE_REG			REG(0x3140)
#define BB_PLL8_N_VAL_REG			REG(0x314C)
#define BB_PLL8_STATUS_REG			REG(0x3158)
#define BB_PLL8_CONFIG_REG			REG(0x3154)
#define BB_PLL8_TEST_CTL_REG			REG(0x3150)

#define BB_PLL14_L_VAL_REG			REG(0x31C4)
#define BB_PLL14_M_VAL_REG			REG(0x31C8)
#define BB_PLL14_MODE_REG			REG(0x31C0)
#define BB_PLL14_N_VAL_REG			REG(0x31CC)
#define BB_PLL14_STATUS_REG			REG(0x31D8)
#define BB_PLL14_CONFIG_REG			REG(0x31D4)
#define BB_PLL14_TEST_CTL_REG			REG(0x31D0)

#define SC_PLL0_L_VAL_REG			REG(0x3208)
#define SC_PLL0_M_VAL_REG			REG(0x320C)
#define SC_PLL0_MODE_REG			REG(0x3200)
#define SC_PLL0_N_VAL_REG			REG(0x3210)
#define SC_PLL0_STATUS_REG			REG(0x321C)
#define SC_PLL0_CONFIG_REG			REG(0x3204)
#define SC_PLL0_TEST_CTL_REG			REG(0x3218)

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
#define USB_HS1_HCLK_CTL_REG			REG(0x2900)
#define USB_HS1_RESET_REG			REG(0x2910)
#define USB_HS1_XCVR_FS_CLK_MD_REG		REG(0x2908)
#define USB_HS1_XCVR_FS_CLK_NS_REG		REG(0x290C)
#define USB_HS1_SYS_CLK_MD_REG			REG(0x36A0)
#define USB_HS1_SYS_CLK_NS_REG			REG(0x36A4)
#define USB_HSIC_HCLK_CTL_REG			REG(0x2920)
#define USB_HSIC_XCVR_FS_CLK_MD_REG		REG(0x2924)
#define USB_HSIC_XCVR_FS_CLK_NS_REG		REG(0x2928)
#define USB_HSIC_RESET_REG			REG(0x2934)
#define USB_HSIC_HSIO_CAL_CLK_CTL_REG		REG(0x2B48)
#define USB_HSIC_CLK_MD_REG			REG(0x2B4C)
#define USB_HSIC_CLK_NS_REG			REG(0x2B50)
#define USB_HSIC_SYSTEM_CLK_MD_REG		REG(0x2B54)
#define USB_HSIC_SYSTEM_CLK_NS_REG		REG(0x2B58)
#define SLIMBUS_XO_SRC_CLK_CTL_REG		REG(0x2628)

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
#define LCC_SEC_PCM_MD_REG			REG_LPA(0x00F4)
#define LCC_SEC_PCM_NS_REG			REG_LPA(0x00F0)
#define LCC_SEC_PCM_STATUS_REG			REG_LPA(0x00F8)
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
#define cxo_to_bb_mux		  0
#define pll8_to_bb_mux		  3
#define pll8_activeonly_to_bb_mux 3
#define pll14_to_bb_mux		  4
#define gnd_to_bb_mux		  6
#define cxo_to_xo_mux		  0
#define gnd_to_xo_mux		  3
#define cxo_to_lpa_mux		  1
#define pll4_to_lpa_mux		  2
#define gnd_to_lpa_mux		  6

/* Test Vector Macros */
#define TEST_TYPE_PER_LS	1
#define TEST_TYPE_PER_HS	2
#define TEST_TYPE_LPA		5
#define TEST_TYPE_LPA_HS	6
#define TEST_TYPE_SHIFT		24
#define TEST_CLK_SEL_MASK	BM(23, 0)
#define TEST_VECTOR(s, t)	(((t) << TEST_TYPE_SHIFT) | BVAL(23, 0, (s)))
#define TEST_PER_LS(s)		TEST_VECTOR((s), TEST_TYPE_PER_LS)
#define TEST_PER_HS(s)		TEST_VECTOR((s), TEST_TYPE_PER_HS)
#define TEST_LPA(s)		TEST_VECTOR((s), TEST_TYPE_LPA)
#define TEST_LPA_HS(s)		TEST_VECTOR((s), TEST_TYPE_LPA_HS)

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_HIGH
};

static int set_vdd_dig(struct clk_vdd_class *vdd_class, int level)
{
	static const int vdd_corner[] = {
		[VDD_DIG_NONE]    = RPM_VREG_CORNER_NONE,
		[VDD_DIG_LOW]     = RPM_VREG_CORNER_LOW,
		[VDD_DIG_NOMINAL] = RPM_VREG_CORNER_NOMINAL,
		[VDD_DIG_HIGH]    = RPM_VREG_CORNER_HIGH,
	};

	return rpm_vreg_set_voltage(RPM_VREG_ID_PM8018_VDD_DIG_CORNER,
		RPM_VREG_VOTER3, vdd_corner[level], RPM_VREG_CORNER_HIGH, 1);
}

static DEFINE_VDD_CLASS(vdd_dig, set_vdd_dig);

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig, \
	.fmax[VDD_DIG_##l1] = (f1)
#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig, \
	.fmax[VDD_DIG_##l1] = (f1), \
	.fmax[VDD_DIG_##l2] = (f2)

/*
 * Clock Descriptions
 */

DEFINE_CLK_RPM_BRANCH(cxo_clk, cxo_a_clk, CXO, 19200000);

static DEFINE_SPINLOCK(soft_vote_lock);

static int pll_acpu_vote_clk_enable(struct clk *c)
{
	int ret = 0;
	unsigned long flags;
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);

	spin_lock_irqsave(&soft_vote_lock, flags);

	if (!*pllv->soft_vote)
		ret = pll_vote_clk_enable(c);
	if (ret == 0)
		*pllv->soft_vote |= (pllv->soft_vote_mask);

	spin_unlock_irqrestore(&soft_vote_lock, flags);
	return ret;
}

static void pll_acpu_vote_clk_disable(struct clk *c)
{
	unsigned long flags;
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);

	spin_lock_irqsave(&soft_vote_lock, flags);

	*pllv->soft_vote &= ~(pllv->soft_vote_mask);
	if (!*pllv->soft_vote)
		pll_vote_clk_disable(c);

	spin_unlock_irqrestore(&soft_vote_lock, flags);
}

static struct clk_ops clk_ops_pll_acpu_vote = {
	.enable = pll_acpu_vote_clk_enable,
	.disable = pll_acpu_vote_clk_disable,
	.is_enabled = pll_vote_clk_is_enabled,
	.get_parent = pll_vote_clk_get_parent,
};

#define PLL_SOFT_VOTE_PRIMARY	BIT(0)
#define PLL_SOFT_VOTE_ACPU	BIT(1)

static unsigned int soft_vote_pll0;

static struct pll_vote_clk pll0_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(0),
	.status_reg = BB_PLL0_STATUS_REG,
	.status_mask = BIT(16),
	.parent = &cxo_clk.c,
	.soft_vote = &soft_vote_pll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.c = {
		.dbg_name = "pll0_clk",
		.rate = 276000000,
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(pll0_clk.c),
		.warned = true,
	},
};

static struct pll_vote_clk pll0_activeonly_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(0),
	.status_reg = BB_PLL0_STATUS_REG,
	.status_mask = BIT(16),
	.soft_vote = &soft_vote_pll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.c = {
		.dbg_name = "pll0_activeonly_clk",
		.rate = 276000000,
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(pll0_activeonly_clk.c),
		.warned = true,
	},
};

static struct pll_vote_clk pll4_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(4),
	.status_reg = LCC_PLL0_STATUS_REG,
	.status_mask = BIT(16),
	.parent = &cxo_clk.c,
	.c = {
		.dbg_name = "pll4_clk",
		.rate = 393216000,
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll4_clk.c),
		.warned = true,
	},
};

static unsigned int soft_vote_pll8;

static struct pll_vote_clk pll8_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(8),
	.status_reg = BB_PLL8_STATUS_REG,
	.status_mask = BIT(16),
	.parent = &cxo_clk.c,
	.soft_vote = &soft_vote_pll8,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.c = {
		.dbg_name = "pll8_clk",
		.rate = 384000000,
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(pll8_clk.c),
		.warned = true,
	},
};

static struct pll_vote_clk pll8_activeonly_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(8),
	.status_reg = BB_PLL8_STATUS_REG,
	.status_mask = BIT(16),
	.soft_vote = &soft_vote_pll8,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.c = {
		.dbg_name = "pll8_activeonly_clk",
		.rate = 384000000,
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(pll8_activeonly_clk.c),
		.warned = true,
	},
};

static struct pll_clk pll9_activeonly_clk = {
	.mode_reg = SC_PLL0_MODE_REG,
	.c = {
		.dbg_name = "pll9_activeonly_clk",
		.rate = 440000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(pll9_activeonly_clk.c),
		.warned = true,
	},
};

static struct pll_vote_clk pll14_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(11),
	.status_reg = BB_PLL14_STATUS_REG,
	.status_mask = BIT(16),
	.parent = &cxo_clk.c,
	.c = {
		.dbg_name = "pll14_clk",
		.rate = 480000000,
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll14_clk.c),
		.warned = true,
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
			VDD_DIG_FMAX_MAP1(LOW, 27000000), \
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
	F_GP( 19200000, cxo,  1, 0, 0),
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
	F_GSBI_QUP(  960000, cxo,  4, 1,  5),
	F_GSBI_QUP( 4800000, cxo,  4, 0,  1),
	F_GSBI_QUP( 9600000, cxo,  2, 0,  1),
	F_GSBI_QUP(15058800, pll8, 1, 2, 51),
	F_GSBI_QUP(24000000, pll8, 4, 1,  4),
	F_GSBI_QUP(25600000, pll8, 1, 1, 15),
	F_GSBI_QUP(48000000, pll8, 4, 1,  2),
	F_GSBI_QUP(51200000, pll8, 1, 2, 15),
	F_END
};

static CLK_GSBI_QUP(gsbi1_qup,   1, CLK_HALT_CFPB_STATEA_REG,  9);
static CLK_GSBI_QUP(gsbi2_qup,   2, CLK_HALT_CFPB_STATEA_REG,  4);
static CLK_GSBI_QUP(gsbi3_qup,   3, CLK_HALT_CFPB_STATEA_REG,  0);
static CLK_GSBI_QUP(gsbi4_qup,   4, CLK_HALT_CFPB_STATEB_REG, 24);
static CLK_GSBI_QUP(gsbi5_qup,   5, CLK_HALT_CFPB_STATEB_REG, 20);

#define F_PDM(f, s, d) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_SRC_SEL(1, 0, s##_to_xo_mux), \
	}
static struct clk_freq_tbl clk_tbl_pdm[] = {
	F_PDM(       0, gnd, 1),
	F_PDM(19200000, cxo, 1),
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
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(pdm_clk.c),
	},
};

static struct branch_clk pmem_clk = {
	.b = {
		.ctl_reg = PMEM_ACLK_CTL_REG,
		.en_mask = BIT(4),
		.hwcg_reg =  PMEM_ACLK_CTL_REG,
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
static struct clk_freq_tbl clk_tbl_prng[] = {
	F_PRNG(32000000, pll8),
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
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "prng_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 32000000, NOMINAL, 65000000),
		CLK_INIT(prng_clk.c),
	},
};

#define CLK_SDC(name, n, h_b, f_table) \
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
		.freq_tbl = f_table, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #name, \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP2(LOW, 26000000, NOMINAL, 52000000), \
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
static struct clk_freq_tbl clk_tbl_sdc1_2[] = {
	F_SDC(        0, gnd,   1, 0,   0),
	F_SDC(   144300, cxo,   1, 1, 133),
	F_SDC(   400000, pll8,  4, 1, 240),
	F_SDC( 16000000, pll8,  4, 1,   6),
	F_SDC( 17070000, pll8,  1, 2,  45),
	F_SDC( 20210000, pll8,  1, 1,  19),
	F_SDC( 24000000, pll8,  4, 1,   4),
	F_SDC( 38400000, pll8,  2, 1,   5),
	F_SDC( 48000000, pll8,  4, 1,   2),
	F_SDC( 64000000, pll8,  3, 1,   2),
	F_SDC( 76800000, pll8,  1, 1,   5),
	F_END
};

static CLK_SDC(sdc1_clk, 1, 6, clk_tbl_sdc1_2);
static CLK_SDC(sdc2_clk, 2, 5, clk_tbl_sdc1_2);

#define F_USB(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(16, m, 0, n), \
		.ns_val = NS(23, 16, n, m, 5, 4, 3, d, 2, 0, s##_to_bb_mux), \
	}
static struct clk_freq_tbl clk_tbl_usb[] = {
	F_USB(       0,  gnd, 1, 0,  0),
	F_USB(60000000, pll8, 1, 5, 32),
	F_END
};

static struct clk_freq_tbl clk_tbl_usb_hs1_sys[] = {
	F_USB(       0,		    gnd, 1, 0,  0),
	F_USB(60000000, pll8_activeonly, 1, 5, 32),
	F_END
};

static struct clk_freq_tbl clk_tbl_usb_hsic_sys[] = {
	F_USB(       0,		    gnd, 1, 0, 0),
	F_USB(64000000, pll8_activeonly, 1, 1, 6),
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
	.mnd_en_mask = BIT(8),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_usb,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "usb_hs1_xcvr_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(NOMINAL, 60000000),
		CLK_INIT(usb_hs1_xcvr_clk.c),
	},
};

static struct rcg_clk usb_hs1_sys_clk = {
	.b = {
		.ctl_reg = USB_HS1_SYS_CLK_NS_REG,
		.en_mask = BIT(9),
		.reset_reg = USB_HS1_RESET_REG,
		.reset_mask = BIT(0),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 4,
	},
	.ns_reg = USB_HS1_SYS_CLK_NS_REG,
	.md_reg = USB_HS1_SYS_CLK_MD_REG,
	.root_en_mask = BIT(11),
	.ns_mask = (BM(23, 16) | BM(6, 0)),
	.mnd_en_mask = BIT(8),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_usb_hs1_sys,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "usb_hs1_sys_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(NOMINAL, 60000000),
		CLK_INIT(usb_hs1_sys_clk.c),
	},
};

static struct rcg_clk usb_hsic_xcvr_clk = {
	.b = {
		.ctl_reg = USB_HSIC_XCVR_FS_CLK_NS_REG,
		.en_mask = BIT(9),
		.reset_reg = USB_HSIC_RESET_REG,
		.reset_mask = BIT(0),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 9,
	},
	.ns_reg = USB_HSIC_XCVR_FS_CLK_NS_REG,
	.md_reg = USB_HSIC_XCVR_FS_CLK_MD_REG,
	.root_en_mask = BIT(11),
	.ns_mask = (BM(23, 16) | BM(6, 0)),
	.mnd_en_mask = BIT(8),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_usb,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "usb_hsic_xcvr_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(usb_hsic_xcvr_clk.c),
	},
};

static struct rcg_clk usb_hsic_sys_clk = {
	.b = {
		.ctl_reg = USB_HSIC_SYSTEM_CLK_NS_REG,
		.en_mask = BIT(9),
		.reset_reg = USB_HSIC_RESET_REG,
		.reset_mask = BIT(0),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 7,
	},
	.ns_reg = USB_HSIC_SYSTEM_CLK_NS_REG,
	.md_reg = USB_HSIC_SYSTEM_CLK_MD_REG,
	.root_en_mask = BIT(11),
	.ns_mask = (BM(23, 16) | BM(6, 0)),
	.mnd_en_mask = BIT(8),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_usb_hsic_sys,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "usb_hsic_sys_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 64000000),
		CLK_INIT(usb_hsic_sys_clk.c),
	},
};

static struct clk_freq_tbl clk_tbl_usb_hsic[] = {
	F_USB(        0, gnd,   1, 0, 0),
	F_USB(480000000, pll14, 1, 0, 0),
	F_END
};

static struct rcg_clk usb_hsic_clk = {
	.b = {
		.ctl_reg = USB_HSIC_CLK_NS_REG,
		.en_mask = BIT(9),
		.reset_reg = USB_HSIC_RESET_REG,
		.reset_mask = BIT(0),
		.halt_check = DELAY,
	},
	.ns_reg = USB_HSIC_CLK_NS_REG,
	.md_reg = USB_HSIC_CLK_MD_REG,
	.root_en_mask = BIT(11),
	.ns_mask = (BM(23, 16) | BM(6, 0)),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_usb_hsic,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "usb_hsic_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 480000000),
		CLK_INIT(usb_hsic_clk.c),
	},
};

static struct branch_clk usb_hsic_hsio_cal_clk = {
	.b = {
		.ctl_reg = USB_HSIC_HSIO_CAL_CLK_CTL_REG,
		.en_mask = BIT(0),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 8,
	},
	.parent = &cxo_clk.c,
	.c = {
		.dbg_name = "usb_hsic_hsio_cal_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(usb_hsic_hsio_cal_clk.c),
	},
};

/* Fast Peripheral Bus Clocks */
static struct branch_clk ce1_core_clk = {
	.b = {
		.ctl_reg = CE1_CORE_CLK_CTL_REG,
		.en_mask = BIT(4),
		.hwcg_reg =  CE1_CORE_CLK_CTL_REG,
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

static struct branch_clk usb_hs1_p_clk = {
	.b = {
		.ctl_reg = USB_HS1_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.hwcg_reg =  USB_HS1_HCLK_CTL_REG,
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

static struct branch_clk usb_hsic_p_clk = {
	.b = {
		.ctl_reg = USB_HSIC_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.hwcg_reg =  USB_HSIC_HCLK_CTL_REG,
		.hwcg_mask = BIT(6),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 3,
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
		.hwcg_reg =  SDCn_HCLK_CTL_REG(1),
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
		.hwcg_reg =  SDCn_HCLK_CTL_REG(2),
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

/* HW-Voteable Clocks */
static struct branch_clk adm0_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(2),
		.halt_reg = CLK_HALT_MSS_KPSS_MISC_STATE_REG,
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
		.halt_reg = CLK_HALT_MSS_KPSS_MISC_STATE_REG,
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
 * Low Power Audio Clocks
 */
#define F_AIF_OSR(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS(31, 24, n, m, 5, 4, 3, d, 2, 0, s##_to_lpa_mux), \
	}
static struct clk_freq_tbl clk_tbl_aif_osr[] = {
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
		.freq_tbl = clk_tbl_aif_osr, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
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
		.freq_tbl = clk_tbl_aif_osr, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
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
static struct clk_freq_tbl clk_tbl_pcm[] = {
	{ .ns_val = BIT(10) /* external input */ },
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
	.freq_tbl = clk_tbl_pcm,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "pcm_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 24576000),
		CLK_INIT(pcm_clk.c),
		.rate = ULONG_MAX,
	},
};

static struct rcg_clk sec_pcm_clk = {
	.b = {
		.ctl_reg = LCC_SEC_PCM_NS_REG,
		.en_mask = BIT(11),
		.reset_reg = LCC_SEC_PCM_NS_REG,
		.reset_mask = BIT(13),
		.halt_reg = LCC_SEC_PCM_STATUS_REG,
		.halt_check = ENABLE,
		.halt_bit = 0,
	},
	.ns_reg = LCC_SEC_PCM_NS_REG,
	.md_reg = LCC_SEC_PCM_MD_REG,
	.root_en_mask = BIT(9),
	.ns_mask = BM(31, 16) | BIT(10) | BM(6, 0),
	.mnd_en_mask = BIT(8),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_pcm,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "sec_pcm_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 24576000),
		CLK_INIT(sec_pcm_clk.c),
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
	.freq_tbl = clk_tbl_aif_osr,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "audio_slimbus_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 24576000),
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

DEFINE_CLK_RPM(cfpb_clk, cfpb_a_clk, CFPB, NULL);
DEFINE_CLK_RPM(dfab_clk, dfab_a_clk, DAYTONA_FABRIC, NULL);
DEFINE_CLK_RPM(ebi1_clk, ebi1_a_clk, EBI1, NULL);
DEFINE_CLK_RPM(sfab_clk, sfab_a_clk, SYSTEM_FABRIC, NULL);
DEFINE_CLK_RPM(sfpb_clk, sfpb_a_clk, SFPB, NULL);

static DEFINE_CLK_VOTER(dfab_usb_hs_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc1_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc2_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sps_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_bam_dmux_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_msmbus_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_msmbus_a_clk, &dfab_a_clk.c, 0);
static DEFINE_CLK_VOTER(ebi1_msmbus_clk, &ebi1_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ebi1_msmbus_a_clk, &ebi1_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ebi1_acpu_a_clk, &ebi1_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ebi1_adm_clk, &ebi1_clk.c, 0);
static DEFINE_CLK_VOTER(sfab_msmbus_a_clk, &sfab_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(sfab_acpu_a_clk, &sfab_a_clk.c, LONG_MAX);

#ifdef CONFIG_DEBUG_FS
struct measure_sel {
	u32 test_vector;
	struct clk *c;
};

static DEFINE_CLK_MEASURE(q6sw_clk);
static DEFINE_CLK_MEASURE(q6fw_clk);
static DEFINE_CLK_MEASURE(q6_func_clk);

static struct measure_sel measure_mux[] = {
	{ TEST_PER_LS(0x08), &slimbus_xo_src_clk.c },
	{ TEST_PER_LS(0x12), &sdc1_p_clk.c },
	{ TEST_PER_LS(0x13), &sdc1_clk.c },
	{ TEST_PER_LS(0x14), &sdc2_p_clk.c },
	{ TEST_PER_LS(0x15), &sdc2_clk.c },
	{ TEST_PER_LS(0x1F), &gp0_clk.c },
	{ TEST_PER_LS(0x20), &gp1_clk.c },
	{ TEST_PER_LS(0x21), &gp2_clk.c },
	{ TEST_PER_LS(0x26), &pmem_clk.c },
	{ TEST_PER_LS(0x25), &dfab_clk.c },
	{ TEST_PER_LS(0x25), &dfab_a_clk.c },
	{ TEST_PER_LS(0x32), &dma_bam_p_clk.c },
	{ TEST_PER_LS(0x33), &cfpb_clk.c },
	{ TEST_PER_LS(0x33), &cfpb_a_clk.c },
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
	{ TEST_PER_LS(0x86), &usb_hsic_sys_clk.c },
	{ TEST_PER_LS(0x87), &usb_hsic_p_clk.c },
	{ TEST_PER_LS(0x88), &usb_hsic_xcvr_clk.c },
	{ TEST_PER_LS(0x8B), &usb_hsic_hsio_cal_clk.c },
	{ TEST_PER_LS(0x8D), &usb_hs1_sys_clk.c },
	{ TEST_PER_LS(0x92), &ce1_p_clk.c },
	{ TEST_PER_HS(0x18), &sfab_clk.c },
	{ TEST_PER_HS(0x18), &sfab_a_clk.c },
	{ TEST_PER_HS(0x26), &q6sw_clk },
	{ TEST_PER_HS(0x27), &q6fw_clk },
	{ TEST_PER_LS(0xA4), &ce1_core_clk.c },
	{ TEST_PER_HS(0x2A), &adm0_clk.c },
	{ TEST_PER_HS(0x34), &ebi1_clk.c },
	{ TEST_PER_HS(0x34), &ebi1_a_clk.c },
	{ TEST_PER_HS(0x3E), &usb_hsic_clk.c },
	{ TEST_LPA(0x0F), &mi2s_bit_clk.c },
	{ TEST_LPA(0x10), &codec_i2s_mic_bit_clk.c },
	{ TEST_LPA(0x11), &codec_i2s_spkr_bit_clk.c },
	{ TEST_LPA(0x12), &spare_i2s_mic_bit_clk.c },
	{ TEST_LPA(0x13), &spare_i2s_spkr_bit_clk.c },
	{ TEST_LPA(0x14), &pcm_clk.c },
	{ TEST_LPA(0x1D), &audio_slimbus_clk.c },
	{ TEST_LPA_HS(0x00), &q6_func_clk },
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
	default:
		ret = -EPERM;
	}
	/* Make sure test vector is set before starting measurements. */
	mb();

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return ret;
}

/* Sample clock for 'ticks' reference clock ticks. */
static unsigned long run_measurement(unsigned ticks)
{
	/* Stop counters and set the XO4 counter start value. */
	writel_relaxed(ticks, RINGOSC_TCXO_CTL_REG);

	/* Wait for timer to become ready. */
	while ((readl_relaxed(RINGOSC_STATUS_REG) & BIT(25)) != 0)
		cpu_relax();

	/* Run measurement and wait for completion. */
	writel_relaxed(BIT(28)|ticks, RINGOSC_TCXO_CTL_REG);
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

static struct clk_lookup msm_clocks_9615[] = {
	CLK_LOOKUP("xo",	cxo_a_clk.c,	""),
	CLK_LOOKUP("xo",	cxo_clk.c,	"BAM_RMNT"),
	CLK_LOOKUP("xo",	cxo_clk.c,	"msm_xo"),
	CLK_LOOKUP("vref_buff",	cxo_clk.c,	"rpm-regulator"),
	CLK_LOOKUP("pll0",	pll0_clk.c,	NULL),
	CLK_LOOKUP("pll8",	pll8_clk.c,	NULL),
	CLK_LOOKUP("pll14",	pll14_clk.c,	NULL),

	CLK_LOOKUP("pll0", pll0_activeonly_clk.c, "acpu"),
	CLK_LOOKUP("pll8", pll8_activeonly_clk.c, "acpu"),
	CLK_LOOKUP("pll9", pll9_activeonly_clk.c, "acpu"),

	CLK_LOOKUP("measure",	measure_clk.c,	"debug"),

	CLK_LOOKUP("bus_clk",		cfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		cfpb_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		dfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		dfab_a_clk.c,	""),
	CLK_LOOKUP("mem_clk",		ebi1_clk.c,	""),
	CLK_LOOKUP("mem_clk",		ebi1_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfab_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfab_a_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfpb_clk.c,	""),
	CLK_LOOKUP("bus_clk",		sfpb_a_clk.c,	""),

	CLK_LOOKUP("bus_clk",		sfab_clk.c,		"msm_sys_fab"),
	CLK_LOOKUP("bus_a_clk",		sfab_msmbus_a_clk.c,	"msm_sys_fab"),
	CLK_LOOKUP("mem_clk",		ebi1_msmbus_clk.c,	"msm_bus"),
	CLK_LOOKUP("mem_a_clk",		ebi1_msmbus_a_clk.c,	"msm_bus"),
	CLK_LOOKUP("dfab_clk",		dfab_msmbus_clk.c,	"msm_bus"),
	CLK_LOOKUP("dfab_a_clk",	dfab_msmbus_a_clk.c,	"msm_bus"),

	CLK_LOOKUP("core_clk",		gp0_clk.c,	""),
	CLK_LOOKUP("core_clk",		gp1_clk.c,	""),
	CLK_LOOKUP("core_clk",		gp2_clk.c,	""),

	CLK_LOOKUP("core_clk", gsbi3_uart_clk.c, ""),
	CLK_LOOKUP("core_clk", gsbi4_uart_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("core_clk", gsbi5_uart_clk.c, ""),

	CLK_LOOKUP("core_clk",	gsbi3_qup_clk.c, "spi_qsd.0"),
	CLK_LOOKUP("core_clk",	gsbi4_qup_clk.c, ""),
	CLK_LOOKUP("core_clk",	gsbi5_qup_clk.c, "qup_i2c.0"),

	CLK_LOOKUP("core_clk",		pdm_clk.c,		""),
	CLK_LOOKUP("mem_clk",		pmem_clk.c,		"msm_sps"),
	CLK_LOOKUP("core_clk",		prng_clk.c,		"msm_rng.0"),
	CLK_LOOKUP("core_clk",		sdc1_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("core_clk",		sdc2_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("iface_clk",		ce1_p_clk.c,		""),
	CLK_LOOKUP("core_clk",		ce1_core_clk.c,		""),
	CLK_LOOKUP("dma_bam_pclk",	dma_bam_p_clk.c,	NULL),

	CLK_LOOKUP("iface_clk",	gsbi3_p_clk.c, "spi_qsd.0"),
	CLK_LOOKUP("iface_clk",	gsbi4_p_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("iface_clk",	gsbi5_p_clk.c, "qup_i2c.0"),

	CLK_LOOKUP("iface_clk",	     usb_hs1_p_clk.c,		"msm_otg"),
	CLK_LOOKUP("core_clk",       usb_hs1_sys_clk.c,		"msm_otg"),
	CLK_LOOKUP("alt_core_clk",   usb_hs1_xcvr_clk.c,	"msm_otg"),
	CLK_LOOKUP("alt_core_clk",   usb_hsic_xcvr_clk.c,      "msm_hsic_host"),
	CLK_LOOKUP("cal_clk",	     usb_hsic_hsio_cal_clk.c,  "msm_hsic_host"),
	CLK_LOOKUP("core_clk",	     usb_hsic_sys_clk.c,       "msm_hsic_host"),
	CLK_LOOKUP("iface_clk",	     usb_hsic_p_clk.c,	       "msm_hsic_host"),
	CLK_LOOKUP("phy_clk",        usb_hsic_clk.c,	       "msm_hsic_host"),
	CLK_LOOKUP("alt_core_clk",  usb_hsic_xcvr_clk.c, "msm_hsic_peripheral"),
	CLK_LOOKUP("cal_clk",  usb_hsic_hsio_cal_clk.c,  "msm_hsic_peripheral"),
	CLK_LOOKUP("core_clk",      usb_hsic_sys_clk.c,  "msm_hsic_peripheral"),
	CLK_LOOKUP("iface_clk",     usb_hsic_p_clk.c,    "msm_hsic_peripheral"),
	CLK_LOOKUP("phy_clk",       usb_hsic_clk.c,      "msm_hsic_peripheral"),

	CLK_LOOKUP("iface_clk",		sdc1_p_clk.c,		"msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",		sdc2_p_clk.c,		"msm_sdcc.2"),
	CLK_LOOKUP("core_clk",		adm0_clk.c,		"msm_dmov"),
	CLK_LOOKUP("iface_clk",		adm0_p_clk.c,		"msm_dmov"),
	CLK_LOOKUP("iface_clk",		pmic_arb0_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		pmic_arb1_p_clk.c,	""),
	CLK_LOOKUP("core_clk",		pmic_ssbi2_clk.c,	""),
	CLK_LOOKUP("mem_clk",		rpm_msg_ram_p_clk.c,	""),
	CLK_LOOKUP("bit_clk",		mi2s_bit_clk.c,		"msm-dai-q6.6"),
	CLK_LOOKUP("osr_clk",		mi2s_osr_clk.c,		"msm-dai-q6.6"),

	CLK_LOOKUP("bit_clk",		codec_i2s_mic_bit_clk.c,
			   "msm-dai-q6.1"),
	CLK_LOOKUP("osr_clk",		codec_i2s_mic_osr_clk.c,
			   "msm-dai-q6.1"),
	CLK_LOOKUP("bit_clk",		codec_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.0"),
	CLK_LOOKUP("osr_clk",		codec_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.0"),
	CLK_LOOKUP("bit_clk",		spare_i2s_mic_bit_clk.c,
			   "msm-dai-q6.5"),
	CLK_LOOKUP("osr_clk",		spare_i2s_mic_osr_clk.c,
			   "msm-dai-q6.5"),
	CLK_LOOKUP("bit_clk",		codec_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.16384"),
	CLK_LOOKUP("osr_clk",		codec_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.16384"),
	CLK_LOOKUP("bit_clk",		spare_i2s_spkr_bit_clk.c,
			   "msm-dai-q6.4"),
	CLK_LOOKUP("osr_clk",		spare_i2s_spkr_osr_clk.c,
			   "msm-dai-q6.4"),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,	"msm-dai-q6.2"),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,	"msm-dai-q6.3"),
	CLK_LOOKUP("sec_pcm_clk",	sec_pcm_clk.c,	"msm-dai-q6.12"),
	CLK_LOOKUP("sec_pcm_clk",	sec_pcm_clk.c,	"msm-dai-q6.13"),

	CLK_LOOKUP("sps_slimbus_clk",	sps_slimbus_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		audio_slimbus_clk.c, "msm_slim_ctrl.1"),
	CLK_LOOKUP("core_clk",		dfab_usb_hs_clk.c,	"msm_otg"),
	CLK_LOOKUP("bus_clk",		dfab_sdc1_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("bus_clk",		dfab_sdc2_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("dfab_clk",		dfab_sps_clk.c,		"msm_sps"),
	CLK_LOOKUP("bus_clk",		dfab_bam_dmux_clk.c,	"BAM_RMNT"),
	CLK_LOOKUP("mem_clk",		ebi1_adm_clk.c, "msm_dmov"),
	CLK_LOOKUP("mem_clk",		ebi1_acpu_a_clk.c, ""),
	CLK_LOOKUP("bus_clk",		sfab_acpu_a_clk.c, ""),

	CLK_LOOKUP("iface_clk",		ce1_p_clk.c,		"qce.0"),
	CLK_LOOKUP("iface_clk",		ce1_p_clk.c,		"qcrypto.0"),
	CLK_LOOKUP("core_clk",		ce1_core_clk.c,		"qce.0"),
	CLK_LOOKUP("core_clk",		ce1_core_clk.c,		"qcrypto.0"),

	CLK_LOOKUP("q6sw_clk",		q6sw_clk, NULL),
	CLK_LOOKUP("q6fw_clk",		q6fw_clk, NULL),
	CLK_LOOKUP("q6_func_clk",	q6_func_clk, NULL),
};

static struct pll_config_regs pll0_regs __initdata = {
	.l_reg = BB_PLL0_L_VAL_REG,
	.m_reg = BB_PLL0_M_VAL_REG,
	.n_reg = BB_PLL0_N_VAL_REG,
	.config_reg = BB_PLL0_CONFIG_REG,
	.mode_reg = BB_PLL0_MODE_REG,
};

static struct pll_config pll0_config __initdata = {
	.l = 0xE,
	.m = 0x3,
	.n = 0x8,
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

static struct pll_config_regs pll14_regs __initdata = {
	.l_reg = BB_PLL14_L_VAL_REG,
	.m_reg = BB_PLL14_M_VAL_REG,
	.n_reg = BB_PLL14_N_VAL_REG,
	.config_reg = BB_PLL14_CONFIG_REG,
	.mode_reg = BB_PLL14_MODE_REG,
};

static struct pll_config pll14_config __initdata = {
	.l = 0x19,
	.m = 0x0,
	.n = 0x1,
	.vco_val = 0x0,
	.vco_mask = BM(17, 16),
	.pre_div_val = 0x0,
	.pre_div_mask = BIT(19),
	.post_div_val = 0x0,
	.post_div_mask = BM(21, 20),
	.main_output_val = BIT(23),
	.main_output_mask = BIT(23),
};

/*
 * Miscellaneous clock register initializations
 */
static void __init msm9615_clock_pre_init(void)
{
	u32 regval, is_pll_enabled, pll9_lval;

	vote_vdd_level(&vdd_dig, VDD_DIG_HIGH);

	clk_ops_local_pll.enable = sr_pll_clk_enable;

	/* Enable PDM CXO source. */
	regval = readl_relaxed(PDM_CLK_NS_REG);
	writel_relaxed(BIT(13) | regval, PDM_CLK_NS_REG);

	/* Check if PLL0 is active */
	is_pll_enabled = readl_relaxed(BB_PLL0_STATUS_REG) & BIT(16);

	if (!is_pll_enabled) {
		/* Enable AUX output */
		regval = readl_relaxed(BB_PLL0_TEST_CTL_REG);
		regval |= BIT(12);
		writel_relaxed(regval, BB_PLL0_TEST_CTL_REG);

		configure_pll(&pll0_config, &pll0_regs, 1);
	}

	/* Check if PLL14 is enabled in FSM mode */
	is_pll_enabled  = readl_relaxed(BB_PLL14_STATUS_REG) & BIT(16);

	if (!is_pll_enabled)
		configure_pll(&pll14_config, &pll14_regs, 1);
	else if (!(readl_relaxed(BB_PLL14_MODE_REG) & BIT(20)))
		WARN(1, "PLL14 enabled in non-FSM mode!\n");

	/* Detect PLL9 rate and fixup structure accordingly */
	pll9_lval = readl_relaxed(SC_PLL0_L_VAL_REG);

	if (pll9_lval == 0x1C)
		pll9_activeonly_clk.c.rate = 550000000;

	/* Enable PLL4 source on the LPASS Primary PLL Mux */
	regval = readl_relaxed(LCC_PRI_PLL_CLK_CTL_REG);
	writel_relaxed(regval | BIT(0), LCC_PRI_PLL_CLK_CTL_REG);

	/*
	 * Disable hardware clock gating for pmem_clk. Leaving it enabled
	 * results in the clock staying on.
	 */
	regval = readl_relaxed(PMEM_ACLK_CTL_REG);
	regval &= ~BIT(6);
	writel_relaxed(regval, PMEM_ACLK_CTL_REG);

	/*
	 * Disable hardware clock gating for dma_bam_p_clk, which does
	 * not have working support for the feature.
	 */
	regval = readl_relaxed(DMA_BAM_HCLK_CTL);
	regval &= ~BIT(6);
	writel_relaxed(regval, DMA_BAM_HCLK_CTL);
}

static void __init msm9615_clock_post_init(void)
{
	/* Keep CXO on whenever APPS cpu is active */
	clk_prepare_enable(&cxo_a_clk.c);

	/* Initialize rates for clocks that only support one. */
	clk_set_rate(&pdm_clk.c, 19200000);
	clk_set_rate(&prng_clk.c, 32000000);
	clk_set_rate(&usb_hs1_xcvr_clk.c, 60000000);
	clk_set_rate(&usb_hs1_sys_clk.c, 60000000);
	clk_set_rate(&usb_hsic_xcvr_clk.c, 60000000);
	clk_set_rate(&usb_hsic_sys_clk.c, 64000000);
	clk_set_rate(&usb_hsic_clk.c, 480000000);

	/*
	 * The halt status bits for PDM may be incorrect at boot.
	 * Toggle these clocks on and off to refresh them.
	*/
	clk_prepare_enable(&pdm_clk.c);
	clk_disable_unprepare(&pdm_clk.c);
}

static int __init msm9615_clock_late_init(void)
{
	return unvote_vdd_level(&vdd_dig, VDD_DIG_HIGH);
}

struct clock_init_data msm9615_clock_init_data __initdata = {
	.table = msm_clocks_9615,
	.size = ARRAY_SIZE(msm_clocks_9615),
	.pre_init = msm9615_clock_pre_init,
	.post_init = msm9615_clock_post_init,
	.late_init = msm9615_clock_late_init,
};

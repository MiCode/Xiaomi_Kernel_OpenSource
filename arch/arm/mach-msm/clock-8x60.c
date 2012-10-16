/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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

#include <mach/msm_iomap.h>
#include <mach/scm-io.h>
#include <mach/rpm.h>
#include <mach/rpm-regulator.h>

#include "clock.h"
#include "clock-local.h"
#include "clock-rpm.h"
#include "clock-voter.h"
#include "clock-pll.h"

#ifdef CONFIG_MSM_SECURE_IO
#undef readl_relaxed
#undef writel_relaxed
#define readl_relaxed secure_readl
#define writel_relaxed secure_writel
#endif

#define REG(off)	(MSM_CLK_CTL_BASE + (off))
#define REG_MM(off)	(MSM_MMSS_CLK_CTL_BASE + (off))
#define REG_LPA(off)	(MSM_LPASS_CLK_CTL_BASE + (off))

/* Peripheral clock registers. */
#define CE2_HCLK_CTL_REG			REG(0x2740)
#define CLK_HALT_CFPB_STATEA_REG		REG(0x2FCC)
#define CLK_HALT_CFPB_STATEB_REG		REG(0x2FD0)
#define CLK_HALT_CFPB_STATEC_REG		REG(0x2FD4)
#define CLK_HALT_DFAB_STATE_REG			REG(0x2FC8)
#define CLK_HALT_MSS_SMPSS_MISC_STATE_REG	REG(0x2FDC)
#define CLK_HALT_SFPB_MISC_STATE_REG		REG(0x2FD8)
#define CLK_TEST_REG				REG(0x2FA0)
#define EBI2_2X_CLK_CTL_REG			REG(0x2660)
#define EBI2_CLK_CTL_REG			REG(0x2664)
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
#define BB_PLL0_STATUS_REG			REG(0x30D8)
#define BB_PLL6_STATUS_REG			REG(0x3118)
#define BB_PLL8_L_VAL_REG			REG(0x3144)
#define BB_PLL8_M_VAL_REG			REG(0x3148)
#define BB_PLL8_MODE_REG			REG(0x3140)
#define BB_PLL8_N_VAL_REG			REG(0x314C)
#define BB_PLL8_STATUS_REG			REG(0x3158)
#define PLLTEST_PAD_CFG_REG			REG(0x2FA4)
#define PMEM_ACLK_CTL_REG			REG(0x25A0)
#define PPSS_HCLK_CTL_REG			REG(0x2580)
#define PRNG_CLK_NS_REG				REG(0x2E80)
#define RINGOSC_NS_REG				REG(0x2DC0)
#define RINGOSC_STATUS_REG			REG(0x2DCC)
#define RINGOSC_TCXO_CTL_REG			REG(0x2DC4)
#define SC0_U_CLK_BRANCH_ENA_VOTE_REG		REG(0x3080)
#define SC1_U_CLK_BRANCH_ENA_VOTE_REG		REG(0x30A0)
#define SC0_U_CLK_SLEEP_ENA_VOTE_REG		REG(0x3084)
#define SC1_U_CLK_SLEEP_ENA_VOTE_REG		REG(0x30A4)
#define SDCn_APPS_CLK_MD_REG(n)			REG(0x2828+(0x20*((n)-1)))
#define SDCn_APPS_CLK_NS_REG(n)			REG(0x282C+(0x20*((n)-1)))
#define SDCn_HCLK_CTL_REG(n)			REG(0x2820+(0x20*((n)-1)))
#define SDCn_RESET_REG(n)			REG(0x2830+(0x20*((n)-1)))
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
#define CAMCLK_CC_REG				REG_MM(0x0140)
#define CAMCLK_MD_REG				REG_MM(0x0144)
#define CAMCLK_NS_REG				REG_MM(0x0148)
#define CSI_CC_REG				REG_MM(0x0040)
#define CSI_NS_REG				REG_MM(0x0048)
#define DBG_BUS_VEC_A_REG			REG_MM(0x01C8)
#define DBG_BUS_VEC_B_REG			REG_MM(0x01CC)
#define DBG_BUS_VEC_C_REG			REG_MM(0x01D0)
#define DBG_BUS_VEC_D_REG			REG_MM(0x01D4)
#define DBG_BUS_VEC_E_REG			REG_MM(0x01D8)
#define DBG_BUS_VEC_F_REG			REG_MM(0x01DC)
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
#define MDP_CC_REG				REG_MM(0x00C0)
#define MDP_MD0_REG				REG_MM(0x00C4)
#define MDP_MD1_REG				REG_MM(0x00C8)
#define MDP_NS_REG				REG_MM(0x00D0)
#define MISC_CC_REG				REG_MM(0x0058)
#define MISC_CC2_REG				REG_MM(0x005C)
#define PIXEL_CC_REG				REG_MM(0x00D4)
#define PIXEL_CC2_REG				REG_MM(0x0120)
#define PIXEL_MD_REG				REG_MM(0x00D8)
#define PIXEL_NS_REG				REG_MM(0x00DC)
#define MM_PLL0_MODE_REG			REG_MM(0x0300)
#define MM_PLL1_MODE_REG			REG_MM(0x031C)
#define MM_PLL2_CONFIG_REG			REG_MM(0x0348)
#define MM_PLL2_L_VAL_REG			REG_MM(0x033C)
#define MM_PLL2_M_VAL_REG			REG_MM(0x0340)
#define MM_PLL2_MODE_REG			REG_MM(0x0338)
#define MM_PLL2_N_VAL_REG			REG_MM(0x0344)
#define ROT_CC_REG				REG_MM(0x00E0)
#define ROT_NS_REG				REG_MM(0x00E8)
#define SAXI_EN_REG				REG_MM(0x0030)
#define SW_RESET_AHB_REG			REG_MM(0x020C)
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
#define LCC_PLL0_CONFIG_REG			REG_LPA(0x0014)
#define LCC_PLL0_L_VAL_REG			REG_LPA(0x0004)
#define LCC_PLL0_M_VAL_REG			REG_LPA(0x0008)
#define LCC_PLL0_MODE_REG			REG_LPA(0x0000)
#define LCC_PLL0_N_VAL_REG			REG_LPA(0x000C)
#define LCC_PRI_PLL_CLK_CTL_REG			REG_LPA(0x00C4)
#define LCC_SPARE_I2S_MIC_MD_REG		REG_LPA(0x007C)
#define LCC_SPARE_I2S_MIC_NS_REG		REG_LPA(0x0078)
#define LCC_SPARE_I2S_MIC_STATUS_REG		REG_LPA(0x0080)
#define LCC_SPARE_I2S_SPKR_MD_REG		REG_LPA(0x0088)
#define LCC_SPARE_I2S_SPKR_NS_REG		REG_LPA(0x0084)
#define LCC_SPARE_I2S_SPKR_STATUS_REG		REG_LPA(0x008C)

/* MUX source input identifiers. */
#define pxo_to_bb_mux		0
#define mxo_to_bb_mux		1
#define cxo_to_bb_mux		5
#define pll0_to_bb_mux		2
#define pll8_to_bb_mux		3
#define pll6_to_bb_mux		4
#define gnd_to_bb_mux		6
#define pxo_to_mm_mux		0
#define pll1_to_mm_mux		1	/* or MMSS_PLL0 */
#define pll2_to_mm_mux		1	/* or MMSS_PLL1 */
#define pll3_to_mm_mux		3	/* or MMSS_PLL2 */
#define pll8_to_mm_mux		2	/* or MMSS_GPERF */
#define pll0_to_mm_mux		3	/* or MMSS_GPLL0 */
#define mxo_to_mm_mux		4
#define gnd_to_mm_mux		6
#define cxo_to_xo_mux		0
#define pxo_to_xo_mux		1
#define mxo_to_xo_mux		2
#define gnd_to_xo_mux		3
#define pxo_to_lpa_mux		0
#define cxo_to_lpa_mux		1
#define pll4_to_lpa_mux		2	/* or LPA_PLL0 */
#define gnd_to_lpa_mux		6

/* Test Vector Macros */
#define TEST_TYPE_PER_LS	1
#define TEST_TYPE_PER_HS	2
#define TEST_TYPE_MM_LS		3
#define TEST_TYPE_MM_HS		4
#define TEST_TYPE_LPA		5
#define TEST_TYPE_SC		6
#define TEST_TYPE_MM_HS2X	7
#define TEST_TYPE_SHIFT		24
#define TEST_CLK_SEL_MASK	BM(23, 0)
#define TEST_VECTOR(s, t)	(((t) << TEST_TYPE_SHIFT) | BVAL(23, 0, (s)))
#define TEST_PER_LS(s)		TEST_VECTOR((s), TEST_TYPE_PER_LS)
#define TEST_PER_HS(s)		TEST_VECTOR((s), TEST_TYPE_PER_HS)
#define TEST_MM_LS(s)		TEST_VECTOR((s), TEST_TYPE_MM_LS)
#define TEST_MM_HS(s)		TEST_VECTOR((s), TEST_TYPE_MM_HS)
#define TEST_LPA(s)		TEST_VECTOR((s), TEST_TYPE_LPA)
#define TEST_SC(s)		TEST_VECTOR((s), TEST_TYPE_SC)
#define TEST_MM_HS2X(s)		TEST_VECTOR((s), TEST_TYPE_MM_HS2X)

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
 * Clock frequency definitions and macros
 */

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_HIGH,
	VDD_DIG_NUM
};

static int set_vdd_dig(struct clk_vdd_class *vdd_class, int level)
{
	static const int vdd_uv[] = {
		[VDD_DIG_NONE]    =  500000,
		[VDD_DIG_LOW]     = 1000000,
		[VDD_DIG_NOMINAL] = 1100000,
		[VDD_DIG_HIGH]    = 1200000
	};

	return rpm_vreg_set_voltage(RPM_VREG_ID_PM8058_S1, RPM_VREG_VOTER3,
				    vdd_uv[level], 1200000, 1);
}

static DEFINE_VDD_CLASS(vdd_dig, set_vdd_dig, VDD_DIG_NUM);

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

DEFINE_CLK_RPM_BRANCH(pxo_clk, pxo_a_clk, PXO, 27000000);
DEFINE_CLK_RPM_BRANCH(cxo_clk, cxo_a_clk, CXO, 19200000);

static struct pll_vote_clk pll8_clk = {
	.en_reg = BB_PLL_ENA_SC0_REG,
	.en_mask = BIT(8),
	.status_reg = BB_PLL8_STATUS_REG,
	.status_mask = BIT(16),
	.parent = &pxo_clk.c,
	.c = {
		.dbg_name = "pll8_clk",
		.rate = 384000000,
		.ops = &clk_ops_pll_vote,
		CLK_INIT(pll8_clk.c),
	},
};

static struct pll_clk pll2_clk = {
	.mode_reg = MM_PLL1_MODE_REG,
	.parent = &pxo_clk.c,
	.c = {
		.dbg_name = "pll2_clk",
		.rate = 800000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(pll2_clk.c),
	},
};

static struct pll_clk pll3_clk = {
	.mode_reg = MM_PLL2_MODE_REG,
	.parent = &pxo_clk.c,
	.c = {
		.dbg_name = "pll3_clk",
		.rate = 0, /* TODO: Detect rate dynamically */
		.ops = &clk_ops_local_pll,
		CLK_INIT(pll3_clk.c),
	},
};

static int pll4_clk_enable(struct clk *c)
{
	struct msm_rpm_iv_pair iv = { MSM_RPM_ID_PLL_4, 1 };
	return msm_rpm_set_noirq(MSM_RPM_CTX_SET_0, &iv, 1);
}

static void pll4_clk_disable(struct clk *c)
{
	struct msm_rpm_iv_pair iv = { MSM_RPM_ID_PLL_4, 0 };
	msm_rpm_set_noirq(MSM_RPM_CTX_SET_0, &iv, 1);
}

static struct clk *pll4_clk_get_parent(struct clk *c)
{
	return &pxo_clk.c;
}

static bool pll4_clk_is_local(struct clk *c)
{
	return false;
}

static enum handoff pll4_clk_handoff(struct clk *clk)
{
	struct msm_rpm_iv_pair iv = { MSM_RPM_ID_PLL_4 };
	int rc = msm_rpm_get_status(&iv, 1);
	if (rc < 0 || !iv.value)
		return HANDOFF_DISABLED_CLK;

	return HANDOFF_ENABLED_CLK;
}

static struct clk_ops clk_ops_pll4 = {
	.enable = pll4_clk_enable,
	.disable = pll4_clk_disable,
	.get_parent = pll4_clk_get_parent,
	.is_local = pll4_clk_is_local,
	.handoff = pll4_clk_handoff,
};

static struct fixed_clk pll4_clk = {
	.c = {
		.dbg_name = "pll4_clk",
		.rate = 540672000,
		.ops = &clk_ops_pll4,
		CLK_INIT(pll4_clk.c),
	},
};

/*
 * SoC-specific Set-Rate Functions
 */

/* Unlike other clocks, the TV rate is adjusted through PLL
 * re-programming. It is also routed through an MND divider. */
static void set_rate_tv(struct rcg_clk *rcg, struct clk_freq_tbl *nf)
{
	struct pll_rate *rate = nf->extra_freq_data;
	uint32_t pll_mode, pll_config, misc_cc2;

	/* Disable PLL output. */
	pll_mode = readl_relaxed(MM_PLL2_MODE_REG);
	pll_mode &= ~BIT(0);
	writel_relaxed(pll_mode, MM_PLL2_MODE_REG);

	/* Assert active-low PLL reset. */
	pll_mode &= ~BIT(2);
	writel_relaxed(pll_mode, MM_PLL2_MODE_REG);

	/* Program L, M and N values. */
	writel_relaxed(rate->l_val, MM_PLL2_L_VAL_REG);
	writel_relaxed(rate->m_val, MM_PLL2_M_VAL_REG);
	writel_relaxed(rate->n_val, MM_PLL2_N_VAL_REG);

	/* Configure MN counter, post-divide, VCO, and i-bits. */
	pll_config = readl_relaxed(MM_PLL2_CONFIG_REG);
	pll_config &= ~(BM(22, 20) | BM(18, 0));
	pll_config |= rate->n_val ? BIT(22) : 0;
	pll_config |= BVAL(21, 20, rate->post_div);
	pll_config |= BVAL(17, 16, rate->vco);
	pll_config |= rate->i_bits;
	writel_relaxed(pll_config, MM_PLL2_CONFIG_REG);

	/* Configure MND. */
	set_rate_mnd(rcg, nf);

	/* Configure hdmi_ref_clk to be equal to the TV clock rate. */
	misc_cc2 = readl_relaxed(MISC_CC2_REG);
	misc_cc2 &= ~(BIT(28)|BM(21, 18));
	misc_cc2 |= (BIT(28)|BVAL(21, 18, (nf->ns_val >> 14) & 0x3));
	writel_relaxed(misc_cc2, MISC_CC2_REG);

	/* De-assert active-low PLL reset. */
	pll_mode |= BIT(2);
	writel_relaxed(pll_mode, MM_PLL2_MODE_REG);

	/* Enable PLL output. */
	pll_mode |= BIT(0);
	writel_relaxed(pll_mode, MM_PLL2_MODE_REG);
}

/*
 * Clock Descriptions
 */

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

static struct branch_clk mdp_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(23),
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

static struct branch_clk vcodec_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN_REG,
		.en_mask = BIT(19),
		.reset_reg = SW_RESET_AXI_REG,
		.reset_mask = BIT(4)|BIT(5),
		.halt_reg = DBG_BUS_VEC_E_REG,
		.halt_bit = 3,
	},
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

static struct branch_clk smi_2x_axi_clk = {
	.b = {
		.ctl_reg = MAXI_EN2_REG,
		.en_mask = BIT(30),
		.halt_reg = DBG_BUS_VEC_I_REG,
		.halt_bit = 0,
	},
	.c = {
		.dbg_name = "smi_2x_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(smi_2x_axi_clk.c),
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

static struct branch_clk csi1_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(20),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(16),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 17,
	},
	.c = {
		.dbg_name = "csi1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi1_p_clk.c),
	},
};

static struct branch_clk dsi_m_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(9),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(6),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 19,
	},
	.c = {
		.dbg_name = "dsi_m_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dsi_m_p_clk.c),
	},
};

static struct branch_clk dsi_s_p_clk = {
	.b = {
		.ctl_reg = AHB_EN_REG,
		.en_mask = BIT(18),
		.reset_reg = SW_RESET_AHB_REG,
		.reset_mask = BIT(5),
		.halt_reg = DBG_BUS_VEC_F_REG,
		.halt_bit = 20,
	},
	.c = {
		.dbg_name = "dsi_s_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dsi_s_p_clk.c),
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
		.flags = CLKFLAG_SKIP_HANDOFF,
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
		.flags = CLKFLAG_SKIP_HANDOFF,
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
	F_GP( 13500000, pxo,  2, 0, 0),
	F_GP( 19200000, cxo,  1, 0, 0),
	F_GP( 27000000, pxo,  1, 0, 0),
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
	F_GSBI_UART( 1843200, pll8, 1,  3, 625),
	F_GSBI_UART( 3686400, pll8, 1,  6, 625),
	F_GSBI_UART( 7372800, pll8, 1, 12, 625),
	F_GSBI_UART(14745600, pll8, 1, 24, 625),
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
		.mnd_en_mask = BIT(8), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_sdc, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000), \
			CLK_INIT(i##_clk.c), \
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
	F_SDC(       0, gnd,   1, 0,   0),
	F_SDC(  144000, pxo,   3, 2, 125),
	F_SDC(  400000, pll8,  4, 1, 240),
	F_SDC(16000000, pll8,  4, 1,   6),
	F_SDC(17070000, pll8,  1, 2,  45),
	F_SDC(20210000, pll8,  1, 1,  19),
	F_SDC(24000000, pll8,  4, 1,   4),
	F_SDC(48000000, pll8,  4, 1,   2),
	F_END
};

static CLK_SDC(sdc1, 1, CLK_HALT_DFAB_STATE_REG, 6);
static CLK_SDC(sdc2, 2, CLK_HALT_DFAB_STATE_REG, 5);
static CLK_SDC(sdc3, 3, CLK_HALT_DFAB_STATE_REG, 4);
static CLK_SDC(sdc4, 4, CLK_HALT_DFAB_STATE_REG, 3);
static CLK_SDC(sdc5, 5, CLK_HALT_DFAB_STATE_REG, 2);

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
		.mnd_en_mask = BIT(8), \
		.set_rate = set_rate_mnd, \
		.freq_tbl = clk_tbl_usb, \
		.current_freq = &rcg_dummy_freq, \
		.c = { \
			.dbg_name = #i "_clk", \
			.ops = &clk_ops_rcg, \
			VDD_DIG_FMAX_MAP1(NOMINAL, 60000000), \
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
static struct branch_clk ce2_p_clk = {
	.b = {
		.ctl_reg = CE2_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEC_REG,
		.halt_bit = 0,
	},
	.parent = &pxo_clk.c,
	.c = {
		.dbg_name = "ce2_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ce2_p_clk.c),
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

static struct branch_clk ppss_p_clk = {
	.b = {
		.ctl_reg = PPSS_HCLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_DFAB_STATE_REG,
		.halt_bit = 19,
	},
	.c = {
		.dbg_name = "ppss_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ppss_p_clk.c),
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

static struct branch_clk ebi2_2x_clk = {
	.b = {
		.ctl_reg = EBI2_2X_CLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 18,
	},
	.c = {
		.dbg_name = "ebi2_2x_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ebi2_2x_clk.c),
	},
};

static struct branch_clk ebi2_clk = {
	.b = {
		.ctl_reg = EBI2_CLK_CTL_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_CFPB_STATEA_REG,
		.halt_bit = 19,
	},
	.c = {
		.dbg_name = "ebi2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ebi2_clk.c),
		.depends = &ebi2_2x_clk.c,
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
	.parent = &pxo_clk.c,
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

static struct branch_clk adm1_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(4),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 12,
	},
	.parent = &pxo_clk.c,
	.c = {
		.dbg_name = "adm1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(adm1_clk.c),
	},
};

static struct branch_clk adm1_p_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(5),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 11,
	},
	.c = {
		.dbg_name = "adm1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(adm1_p_clk.c),
	},
};

static struct branch_clk modem_ahb1_p_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(0),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 8,
	},
	.c = {
		.dbg_name = "modem_ahb1_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(modem_ahb1_p_clk.c),
	},
};

static struct branch_clk modem_ahb2_p_clk = {
	.b = {
		.ctl_reg = SC0_U_CLK_BRANCH_ENA_VOTE_REG,
		.en_mask = BIT(1),
		.halt_reg = CLK_HALT_MSS_SMPSS_MISC_STATE_REG,
		.halt_check = HALT_VOTED,
		.halt_bit = 7,
	},
	.c = {
		.dbg_name = "modem_ahb2_p_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(modem_ahb2_p_clk.c),
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

static struct rcg_clk cam_clk = {
	.b = {
		.ctl_reg = CAMCLK_CC_REG,
		.en_mask = BIT(0),
		.halt_check = DELAY,
	},
	.ns_reg = CAMCLK_NS_REG,
	.md_reg = CAMCLK_MD_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(31, 24) | BM(15, 14) | BM(2, 0)),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd_8,
	.freq_tbl = clk_tbl_cam,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "cam_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 64000000, NOMINAL, 128000000),
		CLK_INIT(cam_clk.c),
	},
};

#define F_CSI(f, s, d) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.ns_val = NS_DIVSRC(15, 12, d, 2, 0, s##_to_mm_mux),  \
	}
static struct clk_freq_tbl clk_tbl_csi[] = {
	F_CSI(        0,  gnd, 1),
	F_CSI(192000000, pll8, 2),
	F_CSI(384000000, pll8, 1),
	F_END
};

static struct rcg_clk csi_src_clk = {
	.ns_reg = CSI_NS_REG,
	.b = {
		.ctl_reg = CSI_CC_REG,
		.halt_check = NOCHECK,
	},
	.root_en_mask = BIT(2),
	.ns_mask = (BM(15, 12) | BM(2, 0)),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_csi,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "csi_src_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 192000000, NOMINAL, 384000000),
		CLK_INIT(csi_src_clk.c),
	},
};

static struct branch_clk csi0_clk = {
	.b = {
		.ctl_reg = CSI_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(8),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 13,
	},
	.parent = &csi_src_clk.c,
	.c = {
		.dbg_name = "csi0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi0_clk.c),
	},
};

static struct branch_clk csi1_clk = {
	.b = {
		.ctl_reg = CSI_CC_REG,
		.en_mask = BIT(7),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(18),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 14,
	},
	.parent = &csi_src_clk.c,
	.c = {
		.dbg_name = "csi1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi1_clk.c),
	},
};

#define F_DSI(d) \
	{ \
		.freq_hz = d, \
		.ns_val = BVAL(27, 24, (d-1)), \
	}
/* The DSI_BYTE clock is sourced from the DSI PHY PLL, which may change rate
 * without this clock driver knowing.  So, overload the clk_set_rate() to set
 * the divider (1 to 16) of the clock with respect to the PLL rate. */
static struct clk_freq_tbl clk_tbl_dsi_byte[] = {
	F_DSI(1),  F_DSI(2),  F_DSI(3),  F_DSI(4),
	F_DSI(5),  F_DSI(6),  F_DSI(7),  F_DSI(8),
	F_DSI(9),  F_DSI(10), F_DSI(11), F_DSI(12),
	F_DSI(13), F_DSI(14), F_DSI(15), F_DSI(16),
	F_END
};


static struct rcg_clk dsi_byte_clk = {
	.b = {
		.ctl_reg = MISC_CC_REG,
		.halt_check = DELAY,
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(7),
		.retain_reg = MISC_CC2_REG,
		.retain_mask = BIT(10),
	},
	.ns_reg = MISC_CC2_REG,
	.root_en_mask = BIT(2),
	.ns_mask = BM(27, 24),
	.set_rate = set_rate_nop,
	.freq_tbl = clk_tbl_dsi_byte,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "dsi_byte_clk",
		.ops = &clk_ops_rcg,
		CLK_INIT(dsi_byte_clk.c),
	},
};

static struct branch_clk dsi_esc_clk = {
	.b = {
		.ctl_reg = MISC_CC_REG,
		.en_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 24,
	},
	.c = {
		.dbg_name = "dsi_esc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(dsi_esc_clk.c),
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
static struct clk_freq_tbl clk_tbl_gfx3d[] = {
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
	F_GFX3D(320000000, pll2, 2,  5),
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
		VDD_DIG_FMAX_MAP3(LOW,   96000000, NOMINAL, 200000000,
				  HIGH, 320000000),
		CLK_INIT(gfx3d_clk.c),
		.depends = &gmem_axi_clk.c,
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
		VDD_DIG_FMAX_MAP2(LOW, 110000000, NOMINAL, 228571000),
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
		VDD_DIG_FMAX_MAP3(LOW,   85330000, NOMINAL, 200000000,
				  HIGH, 228571000),
		CLK_INIT(mdp_clk.c),
		.depends = &mdp_axi_clk.c,
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

#define F_PIXEL_MDP(f, s, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD16(m, n), \
		.ns_val = NS_MM(31, 16, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
	}
static struct clk_freq_tbl clk_tbl_pixel_mdp[] = {
	F_PIXEL_MDP(        0, gnd,  1,   0,    0),
	F_PIXEL_MDP( 25600000, pll8, 3,   1,    5),
	F_PIXEL_MDP( 42667000, pll8, 1,   1,    9),
	F_PIXEL_MDP( 43192000, pll8, 1,  64,  569),
	F_PIXEL_MDP( 48000000, pll8, 4,   1,    2),
	F_PIXEL_MDP( 53990000, pll8, 2, 169,  601),
	F_PIXEL_MDP( 64000000, pll8, 2,   1,    3),
	F_PIXEL_MDP( 69300000, pll8, 1, 231, 1280),
	F_PIXEL_MDP( 76800000, pll8, 1,   1,    5),
	F_PIXEL_MDP( 85333000, pll8, 1,   2,    9),
	F_PIXEL_MDP(106500000, pll8, 1,  71,  256),
	F_PIXEL_MDP(109714000, pll8, 1,   2,    7),
	F_END
};

static struct rcg_clk pixel_mdp_clk = {
	.ns_reg = PIXEL_NS_REG,
	.md_reg = PIXEL_MD_REG,
	.b = {
		.ctl_reg = PIXEL_CC_REG,
		.en_mask = BIT(0),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(5),
		.halt_reg = DBG_BUS_VEC_C_REG,
		.halt_bit = 23,
		.retain_reg = PIXEL_CC_REG,
		.retain_mask = BIT(31),
	},
	.root_en_mask = BIT(2),
	.ns_mask = (BM(31, 16) | BM(15, 14) | BM(2, 0)),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
	.freq_tbl = clk_tbl_pixel_mdp,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "pixel_mdp_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 85333000, NOMINAL, 170000000),
		CLK_INIT(pixel_mdp_clk.c),
	},
};

static struct branch_clk pixel_lcdc_clk = {
	.b = {
		.ctl_reg = PIXEL_CC_REG,
		.en_mask = BIT(8),
		.halt_reg = DBG_BUS_VEC_C_REG,
		.halt_bit = 21,
	},
	.parent = &pixel_mdp_clk.c,
	.c = {
		.dbg_name = "pixel_lcdc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(pixel_lcdc_clk.c),
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
		VDD_DIG_FMAX_MAP2(LOW, 80000000, NOMINAL, 160000000),
		CLK_INIT(rot_clk.c),
		.depends = &rot_axi_clk.c,
	},
};

#define F_TV(f, s, p_r, d, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(23, 16, n, m, 15, 14, d, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
		.extra_freq_data = p_r, \
	}
/* Switching TV freqs requires PLL reconfiguration. */
static struct pll_rate mm_pll2_rate[] = {
	[0] = PLL_RATE( 7, 6301, 13500, 0, 4, 0x4248B), /*  50400500 Hz */
	[1] = PLL_RATE( 8,    0,     0, 0, 4, 0x4248B), /*  54000000 Hz */
	[2] = PLL_RATE(16,    2,   125, 0, 4, 0x5248F), /* 108108000 Hz */
	[3] = PLL_RATE(22,    0,     0, 2, 4, 0x6248B), /* 148500000 Hz */
	[4] = PLL_RATE(44,    0,     0, 2, 4, 0x6248F), /* 297000000 Hz */
};
static struct clk_freq_tbl clk_tbl_tv[] = {
	F_TV(        0, gnd,  &mm_pll2_rate[0], 1, 0, 0),
	F_TV( 25200000, pll3, &mm_pll2_rate[0], 2, 0, 0),
	F_TV( 27000000, pll3, &mm_pll2_rate[1], 2, 0, 0),
	F_TV( 27030000, pll3, &mm_pll2_rate[2], 4, 0, 0),
	F_TV( 74250000, pll3, &mm_pll2_rate[3], 2, 0, 0),
	F_TV(148500000, pll3, &mm_pll2_rate[4], 2, 0, 0),
	F_END
};

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

static struct branch_clk tv_enc_clk = {
	.b = {
		.ctl_reg = TV_CC_REG,
		.en_mask = BIT(8),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(0),
		.halt_reg = DBG_BUS_VEC_D_REG,
		.halt_bit = 8,
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
		.halt_bit = 9,
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
		.halt_bit = 11,
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
		.halt_bit = 10,
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

#define F_VCODEC(f, s, m, n) \
	{ \
		.freq_hz = f, \
		.src_clk = &s##_clk.c, \
		.md_val = MD8(8, m, 0, n), \
		.ns_val = NS_MM(18, 11, n, m, 0, 0, 1, 2, 0, s##_to_mm_mux), \
		.ctl_val = CC(6, n), \
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
	.md_reg = VCODEC_MD0_REG,
	.root_en_mask = BIT(2),
	.ns_mask = (BM(18, 11) | BM(2, 0)),
	.mnd_en_mask = BIT(5),
	.ctl_mask = BM(7, 6),
	.set_rate = set_rate_mnd,
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
	F_VPE(200000000, pll2,  4),
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
		.retain_mask =  BIT(31),
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
		VDD_DIG_FMAX_MAP3(LOW,   76800000, NOMINAL, 160000000,
				  HIGH, 200000000),
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
		.retain_reg = VFE_CC_REG,
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
		VDD_DIG_FMAX_MAP3(LOW,  110000000, NOMINAL, 228570000,
				  HIGH, 266667000),
		CLK_INIT(vfe_clk.c),
		.depends = &vfe_axi_clk.c,
	},
};

static struct branch_clk csi0_vfe_clk = {
	.b = {
		.ctl_reg = VFE_CC_REG,
		.en_mask = BIT(12),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(24),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 7,
	},
	.parent = &vfe_clk.c,
	.c = {
		.dbg_name = "csi0_vfe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi0_vfe_clk.c),
	},
};

static struct branch_clk csi1_vfe_clk = {
	.b = {
		.ctl_reg = VFE_CC_REG,
		.en_mask = BIT(10),
		.reset_reg = SW_RESET_CORE_REG,
		.reset_mask = BIT(23),
		.halt_reg = DBG_BUS_VEC_B_REG,
		.halt_bit = 8,
	},
	.parent = &vfe_clk.c,
	.c = {
		.dbg_name = "csi1_vfe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(csi1_vfe_clk.c),
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
	F_AIF_OSR(  768000, pll4, 4, 1, 176),
	F_AIF_OSR( 1024000, pll4, 4, 1, 132),
	F_AIF_OSR( 1536000, pll4, 4, 1,  88),
	F_AIF_OSR( 2048000, pll4, 4, 1,  66),
	F_AIF_OSR( 3072000, pll4, 4, 1,  44),
	F_AIF_OSR( 4096000, pll4, 4, 1,  33),
	F_AIF_OSR( 6144000, pll4, 4, 1,  22),
	F_AIF_OSR( 8192000, pll4, 2, 1,  33),
	F_AIF_OSR(12288000, pll4, 4, 1,  11),
	F_AIF_OSR(24576000, pll4, 2, 1,  11),
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
		.freq_tbl = clk_tbl_aif_osr, \
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

static CLK_AIF_OSR(mi2s_osr, LCC_MI2S_NS_REG, LCC_MI2S_MD_REG,
		LCC_MI2S_STATUS_REG);
static CLK_AIF_BIT(mi2s_bit, LCC_MI2S_NS_REG, LCC_MI2S_STATUS_REG);

static CLK_AIF_OSR(codec_i2s_mic_osr, LCC_CODEC_I2S_MIC_NS_REG,
		LCC_CODEC_I2S_MIC_MD_REG, LCC_CODEC_I2S_MIC_STATUS_REG);
static CLK_AIF_BIT(codec_i2s_mic_bit, LCC_CODEC_I2S_MIC_NS_REG,
		LCC_CODEC_I2S_MIC_STATUS_REG);

static CLK_AIF_OSR(spare_i2s_mic_osr, LCC_SPARE_I2S_MIC_NS_REG,
		LCC_SPARE_I2S_MIC_MD_REG, LCC_SPARE_I2S_MIC_STATUS_REG);
static CLK_AIF_BIT(spare_i2s_mic_bit, LCC_SPARE_I2S_MIC_NS_REG,
		LCC_SPARE_I2S_MIC_STATUS_REG);

static CLK_AIF_OSR(codec_i2s_spkr_osr, LCC_CODEC_I2S_SPKR_NS_REG,
		LCC_CODEC_I2S_SPKR_MD_REG, LCC_CODEC_I2S_SPKR_STATUS_REG);
static CLK_AIF_BIT(codec_i2s_spkr_bit, LCC_CODEC_I2S_SPKR_NS_REG,
		LCC_CODEC_I2S_SPKR_STATUS_REG);

static CLK_AIF_OSR(spare_i2s_spkr_osr, LCC_SPARE_I2S_SPKR_NS_REG,
		LCC_SPARE_I2S_SPKR_MD_REG, LCC_SPARE_I2S_SPKR_STATUS_REG);
static CLK_AIF_BIT(spare_i2s_spkr_bit, LCC_SPARE_I2S_SPKR_NS_REG,
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
	F_PCM(  512000, pll4, 4, 1, 264),
	F_PCM(  768000, pll4, 4, 1, 176),
	F_PCM( 1024000, pll4, 4, 1, 132),
	F_PCM( 1536000, pll4, 4, 1,  88),
	F_PCM( 2048000, pll4, 4, 1,  66),
	F_PCM( 3072000, pll4, 4, 1,  44),
	F_PCM( 4096000, pll4, 4, 1,  33),
	F_PCM( 6144000, pll4, 4, 1,  22),
	F_PCM( 8192000, pll4, 2, 1,  33),
	F_PCM(12288000, pll4, 4, 1,  11),
	F_PCM(24580000, pll4, 2, 1,  11),
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
	.freq_tbl = clk_tbl_pcm,
	.current_freq = &rcg_dummy_freq,
	.c = {
		.dbg_name = "pcm_clk",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 24580000),
		CLK_INIT(pcm_clk.c),
		.rate = ULONG_MAX,
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
DEFINE_CLK_RPM(smi_clk, smi_a_clk, SMI, &smi_2x_axi_clk.c);

static DEFINE_CLK_VOTER(dfab_dsps_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_usb_hs_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc1_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc2_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc3_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc4_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_sdc5_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_scm_clk, &dfab_clk.c, 0);
static DEFINE_CLK_VOTER(dfab_qseecom_clk, &dfab_clk.c, 0);

static DEFINE_CLK_VOTER(ebi1_msmbus_clk, &ebi1_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ebi1_adm0_clk,   &ebi1_clk.c, 0);
static DEFINE_CLK_VOTER(ebi1_adm1_clk,   &ebi1_clk.c, 0);
static DEFINE_CLK_VOTER(ebi1_acpu_a_clk,   &ebi1_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ebi1_msmbus_a_clk, &ebi1_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(afab_acpu_a_clk,   &afab_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(afab_msmbus_a_clk, &afab_a_clk.c, LONG_MAX);

static DEFINE_CLK_MEASURE(sc0_m_clk);
static DEFINE_CLK_MEASURE(sc1_m_clk);
static DEFINE_CLK_MEASURE(l2_m_clk);

#ifdef CONFIG_DEBUG_FS
struct measure_sel {
	u32 test_vector;
	struct clk *c;
};

static struct measure_sel measure_mux[] = {
	{ TEST_PER_LS(0x08), &modem_ahb1_p_clk.c },
	{ TEST_PER_LS(0x09), &modem_ahb2_p_clk.c },
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
	{ TEST_PER_LS(0x1D), &ebi2_2x_clk.c },
	{ TEST_PER_LS(0x1E), &ebi2_clk.c },
	{ TEST_PER_LS(0x1F), &gp0_clk.c },
	{ TEST_PER_LS(0x20), &gp1_clk.c },
	{ TEST_PER_LS(0x21), &gp2_clk.c },
	{ TEST_PER_LS(0x25), &dfab_clk.c },
	{ TEST_PER_LS(0x25), &dfab_a_clk.c },
	{ TEST_PER_LS(0x26), &pmem_clk.c },
	{ TEST_PER_LS(0x2B), &ppss_p_clk.c },
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
	{ TEST_PER_LS(0x81), &adm1_p_clk.c },
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
	{ TEST_PER_LS(0x93), &ce2_p_clk.c },
	{ TEST_PER_LS(0x94), &tssc_clk.c },

	{ TEST_PER_HS(0x07), &afab_clk.c },
	{ TEST_PER_HS(0x07), &afab_a_clk.c },
	{ TEST_PER_HS(0x18), &sfab_clk.c },
	{ TEST_PER_HS(0x18), &sfab_a_clk.c },
	{ TEST_PER_HS(0x2A), &adm0_clk.c },
	{ TEST_PER_HS(0x2B), &adm1_clk.c },
	{ TEST_PER_HS(0x34), &ebi1_clk.c },
	{ TEST_PER_HS(0x34), &ebi1_a_clk.c },

	{ TEST_MM_LS(0x00), &dsi_byte_clk.c },
	{ TEST_MM_LS(0x01), &pixel_lcdc_clk.c },
	{ TEST_MM_LS(0x04), &pixel_mdp_clk.c },
	{ TEST_MM_LS(0x06), &amp_p_clk.c },
	{ TEST_MM_LS(0x07), &csi0_p_clk.c },
	{ TEST_MM_LS(0x08), &csi1_p_clk.c },
	{ TEST_MM_LS(0x09), &dsi_m_p_clk.c },
	{ TEST_MM_LS(0x0A), &dsi_s_p_clk.c },
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
	{ TEST_MM_LS(0x18), &smmu_p_clk.c },
	{ TEST_MM_LS(0x19), &tv_enc_p_clk.c },
	{ TEST_MM_LS(0x1A), &vcodec_p_clk.c },
	{ TEST_MM_LS(0x1B), &vfe_p_clk.c },
	{ TEST_MM_LS(0x1C), &vpe_p_clk.c },
	{ TEST_MM_LS(0x1D), &cam_clk.c },
	{ TEST_MM_LS(0x1F), &hdmi_app_clk.c },
	{ TEST_MM_LS(0x20), &mdp_vsync_clk.c },
	{ TEST_MM_LS(0x21), &tv_dac_clk.c },
	{ TEST_MM_LS(0x22), &tv_enc_clk.c },
	{ TEST_MM_LS(0x23), &dsi_esc_clk.c },
	{ TEST_MM_LS(0x25), &mmfpb_clk.c },
	{ TEST_MM_LS(0x25), &mmfpb_a_clk.c },

	{ TEST_MM_HS(0x00), &csi0_clk.c },
	{ TEST_MM_HS(0x01), &csi1_clk.c },
	{ TEST_MM_HS(0x03), &csi0_vfe_clk.c },
	{ TEST_MM_HS(0x04), &csi1_vfe_clk.c },
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
	{ TEST_MM_HS(0x24), &smi_2x_axi_clk.c },

	{ TEST_MM_HS2X(0x24), &smi_clk.c },
	{ TEST_MM_HS2X(0x24), &smi_a_clk.c },

	{ TEST_LPA(0x0A), &mi2s_osr_clk.c },
	{ TEST_LPA(0x0B), &mi2s_bit_clk.c },
	{ TEST_LPA(0x0C), &codec_i2s_mic_osr_clk.c },
	{ TEST_LPA(0x0D), &codec_i2s_mic_bit_clk.c },
	{ TEST_LPA(0x0E), &codec_i2s_spkr_osr_clk.c },
	{ TEST_LPA(0x0F), &codec_i2s_spkr_bit_clk.c },
	{ TEST_LPA(0x10), &spare_i2s_mic_osr_clk.c },
	{ TEST_LPA(0x11), &spare_i2s_mic_bit_clk.c },
	{ TEST_LPA(0x12), &spare_i2s_spkr_osr_clk.c },
	{ TEST_LPA(0x13), &spare_i2s_spkr_bit_clk.c },
	{ TEST_LPA(0x14), &pcm_clk.c },

	{ TEST_SC(0x40), &sc0_m_clk },
	{ TEST_SC(0x41), &sc1_m_clk },
	{ TEST_SC(0x42), &l2_m_clk },
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
	 * and scaling factors (multiplier, divider).
	 */
	clk_sel = p->test_vector & TEST_CLK_SEL_MASK;
	measure->sample_ticks = 0x10000;
	measure->multiplier = 1;
	measure->divider = 1;
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
	case TEST_TYPE_MM_HS2X:
		measure->divider = 2;
	case TEST_TYPE_MM_HS:
		writel_relaxed(0x402B800, CLK_TEST_REG);
		writel_relaxed(BVAL(6, 1, clk_sel)|BIT(0), DBG_CFG_REG_HS_REG);
		break;
	case TEST_TYPE_LPA:
		writel_relaxed(0x4030D98, CLK_TEST_REG);
		writel_relaxed(BVAL(6, 1, clk_sel)|BIT(0),
				LCC_CLK_LS_DEBUG_CFG_REG);
		break;
	case TEST_TYPE_SC:
		writel_relaxed(0x5020000|BVAL(16, 10, clk_sel), CLK_TEST_REG);
		measure->sample_ticks = 0x4000;
		measure->multiplier = 2;
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
		do_div(raw_count_full, (((measure->sample_ticks * 10) + 35)
			* measure->divider));
		ret = (raw_count_full * measure->multiplier);
	}

	/* Route dbg_hs_clk to PLLTEST.  300mV single-ended amplitude. */
	writel_relaxed(0x3CF8, PLLTEST_PAD_CFG_REG);
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
	.divider = 1,
};

static struct clk_lookup msm_clocks_8x60[] = {
	CLK_LOOKUP("xo",		cxo_clk.c,	""),
	CLK_LOOKUP("xo",		cxo_a_clk.c,	""),
	CLK_LOOKUP("xo",		pxo_a_clk.c,	""),
	CLK_LOOKUP("xo",		pxo_clk.c,	"pil_modem"),
	CLK_LOOKUP("vref_buff",		cxo_clk.c,	"rpm-regulator"),
	CLK_LOOKUP("pll4",		pll4_clk.c,	"pil_qdsp6v3"),
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
	CLK_LOOKUP("mem_clk",		smi_clk.c,	""),
	CLK_LOOKUP("mem_clk",		smi_a_clk.c,	""),

	CLK_LOOKUP("bus_clk",		afab_clk.c,	"msm_apps_fab"),
	CLK_LOOKUP("bus_a_clk",		afab_msmbus_a_clk.c, "msm_apps_fab"),
	CLK_LOOKUP("bus_clk",		sfab_clk.c,	"msm_sys_fab"),
	CLK_LOOKUP("bus_a_clk",		sfab_a_clk.c,	"msm_sys_fab"),
	CLK_LOOKUP("bus_clk",		sfpb_clk.c,	"msm_sys_fpb"),
	CLK_LOOKUP("bus_a_clk",		sfpb_a_clk.c,	"msm_sys_fpb"),
	CLK_LOOKUP("bus_clk",		mmfab_clk.c,	"msm_mm_fab"),
	CLK_LOOKUP("bus_a_clk",		mmfab_a_clk.c,	"msm_mm_fab"),
	CLK_LOOKUP("bus_clk",		cfpb_clk.c,	"msm_cpss_fpb"),
	CLK_LOOKUP("bus_a_clk",		cfpb_a_clk.c,	"msm_cpss_fpb"),
	CLK_LOOKUP("mem_clk",		ebi1_msmbus_clk.c, "msm_bus"),
	CLK_LOOKUP("mem_a_clk",		ebi1_msmbus_a_clk.c, "msm_bus"),
	CLK_LOOKUP("smi_clk",		smi_clk.c,	"msm_bus"),
	CLK_LOOKUP("smi_a_clk",		smi_a_clk.c,	"msm_bus"),
	CLK_LOOKUP("mmfpb_a_clk",	mmfpb_a_clk.c,	"clock-8x60"),

	CLK_LOOKUP("core_clk",		gp0_clk.c,		""),
	CLK_LOOKUP("core_clk",		gp1_clk.c,		""),
	CLK_LOOKUP("core_clk",		gp2_clk.c,		""),
	CLK_LOOKUP("core_clk",		gsbi1_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi2_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi3_uart_clk.c, "msm_serial_hsl.2"),
	CLK_LOOKUP("core_clk",		gsbi4_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi5_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi6_uart_clk.c, "msm_serial_hs.0"),
	CLK_LOOKUP("core_clk",		gsbi7_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi8_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi9_uart_clk.c, "msm_serial_hsl.1"),
	CLK_LOOKUP("core_clk",		gsbi10_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi11_uart_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi12_uart_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("core_clk",		gsbi1_qup_clk.c,	"spi_qsd.0"),
	CLK_LOOKUP("core_clk",		gsbi2_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi3_qup_clk.c,	"qup_i2c.0"),
	CLK_LOOKUP("core_clk",		gsbi4_qup_clk.c,	"qup_i2c.1"),
	CLK_LOOKUP("core_clk",		gsbi5_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi6_qup_clk.c,	""),
	CLK_LOOKUP("core_clk",		gsbi7_qup_clk.c,	"qup_i2c.4"),
	CLK_LOOKUP("core_clk",		gsbi8_qup_clk.c,	"qup_i2c.3"),
	CLK_LOOKUP("core_clk",		gsbi9_qup_clk.c,	"qup_i2c.2"),
	CLK_LOOKUP("core_clk",		gsbi10_qup_clk.c,	"spi_qsd.1"),
	CLK_LOOKUP("core_clk",		gsbi11_qup_clk.c,	""),
	CLK_LOOKUP("gsbi_qup_clk",	gsbi12_qup_clk.c,	"msm_dsps"),
	CLK_LOOKUP("core_clk",		gsbi12_qup_clk.c,	"qup_i2c.5"),
	CLK_LOOKUP("core_clk",		pdm_clk.c,		""),
	CLK_LOOKUP("mem_clk",		pmem_clk.c,		"msm_dsps"),
	CLK_LOOKUP("core_clk",		prng_clk.c,	"msm_rng.0"),
	CLK_LOOKUP("core_clk",		sdc1_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("core_clk",		sdc2_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("core_clk",		sdc3_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("core_clk",		sdc4_clk.c, "msm_sdcc.4"),
	CLK_LOOKUP("core_clk",		sdc5_clk.c, "msm_sdcc.5"),
	CLK_LOOKUP("ref_clk",		tsif_ref_clk.c,		"msm_tsif.0"),
	CLK_LOOKUP("ref_clk",		tsif_ref_clk.c,		"msm_tsif.1"),
	CLK_LOOKUP("core_clk",		tssc_clk.c,		""),
	CLK_LOOKUP("alt_core_clk",	usb_hs1_xcvr_clk.c,	"msm_otg"),
	CLK_LOOKUP("phy_clk",		usb_phy0_clk.c,		"msm_otg"),
	CLK_LOOKUP("alt_core_clk",	usb_fs1_xcvr_clk.c,	""),
	CLK_LOOKUP("sys_clk",		usb_fs1_sys_clk.c,	""),
	CLK_LOOKUP("src_clk",		usb_fs1_src_clk.c,	""),
	CLK_LOOKUP("alt_core_clk",	usb_fs2_xcvr_clk.c,	""),
	CLK_LOOKUP("sys_clk",		usb_fs2_sys_clk.c,	""),
	CLK_LOOKUP("src_clk",		usb_fs2_src_clk.c,	""),
	CLK_LOOKUP("core_clk",		ce2_p_clk.c,		"qce.0"),
	CLK_LOOKUP("core_clk",		ce2_p_clk.c,		"qcrypto.0"),
	CLK_LOOKUP("iface_clk",		gsbi1_p_clk.c,		"spi_qsd.0"),
	CLK_LOOKUP("iface_clk",		gsbi2_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi3_p_clk.c, "msm_serial_hsl.2"),
	CLK_LOOKUP("iface_clk",		gsbi3_p_clk.c,		"qup_i2c.0"),
	CLK_LOOKUP("iface_clk",		gsbi4_p_clk.c,		"qup_i2c.1"),
	CLK_LOOKUP("iface_clk",		gsbi5_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi6_p_clk.c, "msm_serial_hs.0"),
	CLK_LOOKUP("iface_clk",		gsbi7_p_clk.c,		"qup_i2c.4"),
	CLK_LOOKUP("iface_clk",		gsbi8_p_clk.c,		"qup_i2c.3"),
	CLK_LOOKUP("iface_clk",		gsbi9_p_clk.c, "msm_serial_hsl.1"),
	CLK_LOOKUP("iface_clk",		gsbi9_p_clk.c,		"qup_i2c.2"),
	CLK_LOOKUP("iface_clk",		gsbi10_p_clk.c,		"spi_qsd.1"),
	CLK_LOOKUP("iface_clk",		gsbi11_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi12_p_clk.c,		""),
	CLK_LOOKUP("iface_clk",		gsbi12_p_clk.c, "msm_serial_hsl.0"),
	CLK_LOOKUP("iface_clk",		gsbi12_p_clk.c,		"qup_i2c.5"),
	CLK_LOOKUP("iface_clk",		ppss_p_clk.c,		"msm_dsps"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,		"msm_tsif.0"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,		"msm_tsif.1"),
	CLK_LOOKUP("iface_clk",		usb_fs1_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		usb_fs2_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		usb_hs1_p_clk.c,	"msm_otg"),
	CLK_LOOKUP("iface_clk",		sdc1_p_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",		sdc2_p_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("iface_clk",		sdc3_p_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("iface_clk",		sdc4_p_clk.c, "msm_sdcc.4"),
	CLK_LOOKUP("iface_clk",		sdc5_p_clk.c, "msm_sdcc.5"),
	CLK_LOOKUP("mem_clk",		ebi2_2x_clk.c,		""),
	CLK_LOOKUP("mem_clk",		ebi2_clk.c,		"msm_ebi2"),
	CLK_LOOKUP("core_clk",		adm0_clk.c, "msm_dmov.0"),
	CLK_LOOKUP("iface_clk",		adm0_p_clk.c, "msm_dmov.0"),
	CLK_LOOKUP("core_clk",		adm1_clk.c, "msm_dmov.1"),
	CLK_LOOKUP("iface_clk",		adm1_p_clk.c, "msm_dmov.1"),
	CLK_LOOKUP("iface_clk",		modem_ahb1_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		modem_ahb2_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		pmic_arb0_p_clk.c,	""),
	CLK_LOOKUP("iface_clk",		pmic_arb1_p_clk.c,	""),
	CLK_LOOKUP("core_clk",		pmic_ssbi2_clk.c,	""),
	CLK_LOOKUP("mem_clk",		rpm_msg_ram_p_clk.c,	""),
	CLK_LOOKUP("cam_clk",		cam_clk.c,		NULL),
	CLK_LOOKUP("csi_clk",		csi0_clk.c,		NULL),
	CLK_LOOKUP("csi_clk",		csi1_clk.c, "msm_camera_ov7692.0"),
	CLK_LOOKUP("csi_clk",		csi1_clk.c, "msm_camera_ov9726.0"),
	CLK_LOOKUP("csi_clk",		csi1_clk.c, "msm_csic.1"),
	CLK_LOOKUP("csi_src_clk",	csi_src_clk.c,		NULL),
	CLK_LOOKUP("byte_clk",	dsi_byte_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("esc_clk",	dsi_esc_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("core_clk",		gfx2d0_clk.c,	"kgsl-2d0.0"),
	CLK_LOOKUP("core_clk",		gfx2d0_clk.c,	"footswitch-8x60.0"),
	CLK_LOOKUP("core_clk",		gfx2d1_clk.c,	"kgsl-2d1.1"),
	CLK_LOOKUP("core_clk",		gfx2d1_clk.c,	"footswitch-8x60.1"),
	CLK_LOOKUP("core_clk",		gfx3d_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		gfx3d_clk.c,	"footswitch-8x60.2"),
	CLK_LOOKUP("core_clk",		ijpeg_clk.c,	"msm_gemini.0"),
	CLK_LOOKUP("core_clk",		ijpeg_clk.c,	"footswitch-8x60.3"),
	CLK_LOOKUP("core_clk",		jpegd_clk.c,		NULL),
	CLK_LOOKUP("core_clk",		mdp_clk.c,		"mdp.0"),
	CLK_LOOKUP("core_clk",		mdp_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("vsync_clk",	mdp_vsync_clk.c,		"mdp.0"),
	CLK_LOOKUP("vsync_clk",		mdp_vsync_clk.c, "footswitch-8x60.4"),
	CLK_LOOKUP("lcdc_clk",	pixel_lcdc_clk.c,		"lcdc.0"),
	CLK_LOOKUP("pixel_lcdc_clk",	pixel_lcdc_clk.c, "footswitch-8x60.4"),
	CLK_LOOKUP("mdp_clk",	pixel_mdp_clk.c,	"lcdc.0"),
	CLK_LOOKUP("pixel_mdp_clk",	pixel_mdp_clk.c, "footswitch-8x60.4"),
	CLK_LOOKUP("core_clk",		rot_clk.c,	"msm_rotator.0"),
	CLK_LOOKUP("core_clk",		rot_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("tv_enc_clk",	tv_enc_clk.c,		NULL),
	CLK_LOOKUP("tv_dac_clk",	tv_dac_clk.c,		NULL),
	CLK_LOOKUP("core_clk",		vcodec_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("core_clk",		vcodec_clk.c,	"footswitch-8x60.7"),
	CLK_LOOKUP("mdp_clk",	mdp_tv_clk.c,		"dtv.0"),
	CLK_LOOKUP("tv_clk",		mdp_tv_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("hdmi_clk",		hdmi_tv_clk.c,	"dtv.0"),
	CLK_LOOKUP("src_clk",	tv_src_clk.c,	"dtv.0"),
	CLK_LOOKUP("tv_src_clk",	tv_src_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("core_clk",		hdmi_app_clk.c,	"hdmi_msm.1"),
	CLK_LOOKUP("vpe_clk",		vpe_clk.c,		NULL),
	CLK_LOOKUP("core_clk",		vpe_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("csi_vfe_clk",	csi0_vfe_clk.c,		NULL),
	CLK_LOOKUP("csi_vfe_clk",	csi1_vfe_clk.c, "msm_camera_ov7692.0"),
	CLK_LOOKUP("csi_vfe_clk",	csi1_vfe_clk.c, "msm_camera_ov9726.0"),
	CLK_LOOKUP("csi_vfe_clk",	csi1_vfe_clk.c, "msm_csic.1"),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,		NULL),
	CLK_LOOKUP("core_clk",		vfe_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("bus_clk",		vfe_axi_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("bus_clk",		ijpeg_axi_clk.c, "footswitch-8x60.3"),
	CLK_LOOKUP("mem_clk",		imem_axi_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("bus_clk",		mdp_axi_clk.c,	 "footswitch-8x60.4"),
	CLK_LOOKUP("bus_clk",		rot_axi_clk.c,	 "footswitch-8x60.6"),
	CLK_LOOKUP("bus_clk",		vcodec_axi_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("bus_clk",		vpe_axi_clk.c,	 "footswitch-8x60.9"),
	CLK_LOOKUP("arb_clk",		amp_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("csi_pclk",		csi0_p_clk.c,		NULL),
	CLK_LOOKUP("csi_pclk",		csi1_p_clk.c, "msm_camera_ov7692.0"),
	CLK_LOOKUP("csi_pclk",		csi1_p_clk.c, "msm_camera_ov9726.0"),
	CLK_LOOKUP("csi_pclk",		csi1_p_clk.c,		"msm_csic.1"),
	CLK_LOOKUP("master_iface_clk",	dsi_m_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("slave_iface_clk",	dsi_s_p_clk.c,		"mipi_dsi.1"),
	CLK_LOOKUP("iface_clk",		gfx2d0_p_clk.c,	"kgsl-2d0.0"),
	CLK_LOOKUP("iface_clk",		gfx2d0_p_clk.c,	"footswitch-8x60.0"),
	CLK_LOOKUP("iface_clk",		gfx2d1_p_clk.c,	"kgsl-2d1.1"),
	CLK_LOOKUP("iface_clk",		gfx2d1_p_clk.c,	"footswitch-8x60.1"),
	CLK_LOOKUP("iface_clk",		gfx3d_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("iface_clk",		gfx3d_p_clk.c,	"footswitch-8x60.2"),
	CLK_LOOKUP("master_iface_clk",	hdmi_m_p_clk.c,	"hdmi_msm.1"),
	CLK_LOOKUP("slave_iface_clk",	hdmi_s_p_clk.c,	"hdmi_msm.1"),
	CLK_LOOKUP("iface_clk",		ijpeg_p_clk.c,	"msm_gemini.0"),
	CLK_LOOKUP("iface_clk",		ijpeg_p_clk.c,	"footswitch-8x60.3"),
	CLK_LOOKUP("iface_clk",		jpegd_p_clk.c,		NULL),
	CLK_LOOKUP("mem_iface_clk",	imem_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("iface_clk",		mdp_p_clk.c,		"mdp.0"),
	CLK_LOOKUP("iface_clk",		mdp_p_clk.c,	"footswitch-8x60.4"),
	CLK_LOOKUP("iface_clk",		smmu_p_clk.c,	"msm_iommu"),
	CLK_LOOKUP("iface_clk",		rot_p_clk.c,	"msm_rotator.0"),
	CLK_LOOKUP("iface_clk",		rot_p_clk.c,	"footswitch-8x60.6"),
	CLK_LOOKUP("tv_enc_pclk",	tv_enc_p_clk.c,		NULL),
	CLK_LOOKUP("iface_clk",		vcodec_p_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("iface_clk",		vcodec_p_clk.c, "footswitch-8x60.7"),
	CLK_LOOKUP("vfe_pclk",		vfe_p_clk.c,		NULL),
	CLK_LOOKUP("iface_clk",		vfe_p_clk.c,	"footswitch-8x60.8"),
	CLK_LOOKUP("vpe_pclk",		vpe_p_clk.c,		NULL),
	CLK_LOOKUP("iface_clk",		vpe_p_clk.c,	"footswitch-8x60.9"),
	CLK_LOOKUP("mi2s_osr_clk",	mi2s_osr_clk.c,		NULL),
	CLK_LOOKUP("mi2s_bit_clk",	mi2s_bit_clk.c,		NULL),
	CLK_LOOKUP("i2s_mic_osr_clk",	codec_i2s_mic_osr_clk.c,	NULL),
	CLK_LOOKUP("i2s_mic_bit_clk",	codec_i2s_mic_bit_clk.c,	NULL),
	CLK_LOOKUP("i2s_mic_osr_clk",	spare_i2s_mic_osr_clk.c,	NULL),
	CLK_LOOKUP("i2s_mic_bit_clk",	spare_i2s_mic_bit_clk.c,	NULL),
	CLK_LOOKUP("i2s_spkr_osr_clk",	codec_i2s_spkr_osr_clk.c,	NULL),
	CLK_LOOKUP("i2s_spkr_bit_clk",	codec_i2s_spkr_bit_clk.c,	NULL),
	CLK_LOOKUP("i2s_spkr_osr_clk",	spare_i2s_spkr_osr_clk.c,	NULL),
	CLK_LOOKUP("i2s_spkr_bit_clk",	spare_i2s_spkr_bit_clk.c,	NULL),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,		NULL),
	CLK_LOOKUP("core_clk",		jpegd_axi_clk.c,	"msm_iommu.0"),
	CLK_LOOKUP("core_clk",		vpe_axi_clk.c,		"msm_iommu.1"),
	CLK_LOOKUP("core_clk",		mdp_axi_clk.c,		"msm_iommu.2"),
	CLK_LOOKUP("core_clk",		mdp_axi_clk.c,		"msm_iommu.3"),
	CLK_LOOKUP("core_clk",		rot_axi_clk.c,		"msm_iommu.4"),
	CLK_LOOKUP("core_clk",		ijpeg_axi_clk.c,	"msm_iommu.5"),
	CLK_LOOKUP("core_clk",		vfe_axi_clk.c,		"msm_iommu.6"),
	CLK_LOOKUP("core_clk",		vcodec_axi_clk.c,	"msm_iommu.7"),
	CLK_LOOKUP("core_clk",		vcodec_axi_clk.c,	"msm_iommu.8"),
	CLK_LOOKUP("core_clk",		gfx3d_clk.c,		"msm_iommu.9"),
	CLK_LOOKUP("core_clk",		gfx2d0_clk.c,		"msm_iommu.10"),
	CLK_LOOKUP("core_clk",		gfx2d1_clk.c,		"msm_iommu.11"),

	CLK_LOOKUP("mdp_iommu_clk", mdp_axi_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("rot_iommu_clk",	rot_axi_clk.c,	"msm_vidc.0"),
	CLK_LOOKUP("vcodec_iommu0_clk", vcodec_axi_clk.c, "msm_vidc.0"),
	CLK_LOOKUP("vcodec_iommu1_clk", vcodec_axi_clk.c, "msm_vidc.0"),
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
	CLK_LOOKUP("bus_clk",		dfab_scm_clk.c,	"scm"),
	CLK_LOOKUP("bus_clk",		dfab_qseecom_clk.c,	"qseecom"),

	CLK_LOOKUP("mem_clk",		ebi1_adm0_clk.c, "msm_dmov.0"),
	CLK_LOOKUP("mem_clk",		ebi1_adm1_clk.c, "msm_dmov.1"),
	CLK_LOOKUP("mem_clk",		ebi1_acpu_a_clk.c, ""),
	CLK_LOOKUP("bus_clk",		afab_acpu_a_clk.c, ""),

	CLK_LOOKUP("sc0_mclk",		sc0_m_clk, ""),
	CLK_LOOKUP("sc1_mclk",		sc1_m_clk, ""),
	CLK_LOOKUP("l2_mclk",		l2_m_clk,  ""),
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

static void __init msm8660_clock_pre_init(void)
{
	vote_vdd_level(&vdd_dig, VDD_DIG_HIGH);

	/* Setup MM_PLL2 (PLL3), but turn it off. Rate set by set_rate_tv(). */
	rmwreg(0, MM_PLL2_MODE_REG, BIT(0)); /* Disable output */
	/* Set ref, bypass, assert reset, disable output, disable test mode */
	writel_relaxed(0, MM_PLL2_MODE_REG); /* PXO */
	writel_relaxed(0x00800000, MM_PLL2_CONFIG_REG); /* Enable main out. */

	/* The clock driver doesn't use SC1's voting register to control
	 * HW-voteable clocks.  Clear its bits so that disabling bits in the
	 * SC0 register will cause the corresponding clocks to be disabled. */
	rmwreg(BIT(12)|BIT(11), SC0_U_CLK_BRANCH_ENA_VOTE_REG, BM(12, 11));
	writel_relaxed(BIT(12)|BIT(11), SC1_U_CLK_BRANCH_ENA_VOTE_REG);
	/* Let sc_aclk and sc_clk halt when both Scorpions are collapsed. */
	writel_relaxed(BIT(12)|BIT(11), SC0_U_CLK_SLEEP_ENA_VOTE_REG);
	writel_relaxed(BIT(12)|BIT(11), SC1_U_CLK_SLEEP_ENA_VOTE_REG);

	/* Deassert MM SW_RESET_ALL signal. */
	writel_relaxed(0, SW_RESET_ALL_REG);

	/* Initialize MM AHB registers: Enable the FPB clock and disable HW
	 * gating for all clocks. Also set VFE_AHB's FORCE_CORE_ON bit to
	 * prevent its memory from being collapsed when the clock is halted.
	 * The sleep and wake-up delays are set to safe values. */
	rmwreg(0x00000003, AHB_EN_REG,  0x6C000003);
	writel_relaxed(0x000007F9, AHB_EN2_REG);

	/* Deassert all locally-owned MM AHB resets. */
	rmwreg(0, SW_RESET_AHB_REG, 0xFFF7DFFF);

	/* Initialize MM AXI registers: Enable HW gating for all clocks that
	 * support it. Also set FORCE_CORE_ON bits, and any sleep and wake-up
	 * delays to safe values. */
	rmwreg(0x100207F9, MAXI_EN_REG,  0x1803FFFF);
	rmwreg(0x7027FCFF, MAXI_EN2_REG, 0x7A3FFFFF);
	writel_relaxed(0x3FE7FCFF, MAXI_EN3_REG);
	writel_relaxed(0x000001D8, SAXI_EN_REG);

	/* Initialize MM CC registers: Set MM FORCE_CORE_ON bits so that core
	 * memories retain state even when not clocked. Also, set sleep and
	 * wake-up delays to safe values. */
	rmwreg(0x00000000, CSI_CC_REG,    0x00000018);
	rmwreg(0x00000400, MISC_CC_REG,   0x017C0400);
	rmwreg(0x000007FD, MISC_CC2_REG,  0x70C2E7FF);
	rmwreg(0x80FF0000, GFX2D0_CC_REG, 0xE0FF0010);
	rmwreg(0x80FF0000, GFX2D1_CC_REG, 0xE0FF0010);
	rmwreg(0x80FF0000, GFX3D_CC_REG,  0xE0FF0010);
	rmwreg(0x80FF0000, IJPEG_CC_REG,  0xE0FF0018);
	rmwreg(0x80FF0000, JPEGD_CC_REG,  0xE0FF0018);
	rmwreg(0x80FF0000, MDP_CC_REG,    0xE1FF0010);
	rmwreg(0x80FF0000, PIXEL_CC_REG,  0xE1FF0010);
	rmwreg(0x000004FF, PIXEL_CC2_REG, 0x000007FF);
	rmwreg(0x80FF0000, ROT_CC_REG,    0xE0FF0010);
	rmwreg(0x80FF0000, TV_CC_REG,     0xE1FFC010);
	rmwreg(0x000004FF, TV_CC2_REG,    0x000027FF);
	rmwreg(0xC0FF0000, VCODEC_CC_REG, 0xE0FF0010);
	rmwreg(0x80FF0000, VFE_CC_REG,    0xE0FFC010);
	rmwreg(0x80FF0000, VPE_CC_REG,    0xE0FF0010);

	/* De-assert MM AXI resets to all hardware blocks. */
	writel_relaxed(0, SW_RESET_AXI_REG);

	/* Deassert all MM core resets. */
	writel_relaxed(0, SW_RESET_CORE_REG);

	/* Enable TSSC and PDM PXO sources. */
	writel_relaxed(BIT(11), TSSC_CLK_CTL_REG);
	writel_relaxed(BIT(15), PDM_CLK_NS_REG);
	/* Set the dsi_byte_clk src to the DSI PHY PLL,
	 * dsi_esc_clk to PXO/2, and the hdmi_app_clk src to PXO */
	rmwreg(0x400001, MISC_CC2_REG, 0x424003);

	if ((readl_relaxed(PRNG_CLK_NS_REG) & 0x7F) == 0x2B)
		prng_clk.freq_tbl = clk_tbl_prng_64;
}

static void __init msm8660_clock_post_init(void)
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
	clk_set_rate(&usb_fs2_src_clk.c, 60000000);

	/* The halt status bits for PDM and TSSC may be incorrect at boot.
	 * Toggle these clocks on and off to refresh them. */
	clk_prepare_enable(&pdm_clk.c);
	clk_disable_unprepare(&pdm_clk.c);
	clk_prepare_enable(&tssc_clk.c);
	clk_disable_unprepare(&tssc_clk.c);
}

static int __init msm8660_clock_late_init(void)
{
	int rc;

	/* Vote for MMFPB to be at least 64MHz when an Apps CPU is active. */
	struct clk *mmfpb_a_clk = clk_get_sys("clock-8x60", "mmfpb_a_clk");
	if (WARN(IS_ERR(mmfpb_a_clk), "mmfpb_a_clk not found (%ld)\n",
			PTR_ERR(mmfpb_a_clk)))
		return PTR_ERR(mmfpb_a_clk);
	rc = clk_set_rate(mmfpb_a_clk, 64000000);
	if (WARN(rc, "mmfpb_a_clk rate was not set (%d)\n", rc))
		return rc;
	rc = clk_prepare_enable(mmfpb_a_clk);
	if (WARN(rc, "mmfpb_a_clk not enabled (%d)\n", rc))
		return rc;

	return unvote_vdd_level(&vdd_dig, VDD_DIG_HIGH);
}

struct clock_init_data msm8x60_clock_init_data __initdata = {
	.table = msm_clocks_8x60,
	.size = ARRAY_SIZE(msm_clocks_8x60),
	.pre_init = msm8660_clock_pre_init,
	.post_init = msm8660_clock_post_init,
	.late_init = msm8660_clock_late_init,
};

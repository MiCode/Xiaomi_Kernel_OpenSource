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
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-alpha-pll.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/clock-rpm.h>

#include <linux/clk/msm-clock-generic.h>
#include <linux/regulator/rpm-smd-regulator.h>

#include <dt-bindings/clock/msm-clocks-tellurium.h>
#include <dt-bindings/clock/msm-clocks-hwio-tellurium.h>

#include "clock.h"

enum {
	GCC_BASE,
	APCS_C0_PLL_BASE,
	APCS_C1_PLL_BASE,
	APCS_CCI_PLL_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];
#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))

/* Mux source select values */
#define xo_source_val			0
#define xo_a_source_val			0
#define gpll0_source_val		1
#define gpll3_source_val		2

#define gpll0_out_aux_source_val	2   /* cci_clk_src and
					     * usb_fs_system_clk_src */
#define gpll6_source_val		2   /* mclk0_2_clk_src */
#define gpll6_out_aux_source_val	3   /* gfx3d_clk_src */
#define gpll6_out_main_source_val	1   /* usb_fs_ic_clk_src */
#define dsi0_phypll_source_val		1   /* byte0_clk & pclk0_clk */

#define F(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
	}

#define F_MM(s) \
	{ \
		.div_src_val = BVAL(10, 8, s##_source_val),\
	}

#define F_APCS_PLL(f, l, m, n, pre_div, post_div, vco) \
	{ \
		.freq_hz = (f), \
		.l_val = (l), \
		.m_val = (m), \
		.n_val = (n), \
		.pre_div_val = BVAL(12, 12, (pre_div)), \
		.post_div_val = BVAL(9, 8, (post_div)), \
		.vco_val = BVAL(29, 28, (vco)), \
	}

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
		[VDD_DIG_##l3] = (f3),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_HIGH,
	VDD_DIG_NUM
};

static int vdd_corner[] = {
	RPM_REGULATOR_CORNER_NONE,              /* VDD_DIG_NONE */
	RPM_REGULATOR_CORNER_SVS_SOC,           /* VDD_DIG_LOW */
	RPM_REGULATOR_CORNER_NORMAL,            /* VDD_DIG_NOMINAL */
	RPM_REGULATOR_CORNER_SUPER_TURBO,       /* VDD_DIG_HIGH */
};

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

/* SMD clocks */
DEFINE_CLK_RPM_SMD_BRANCH(xo_clk_src, xo_a_clk_src, RPM_MISC_CLK_TYPE,
				CXO_CLK_SRC_ID, 19200000);
DEFINE_CLK_RPM_SMD(pnoc_clk, pnoc_a_clk, RPM_BUS_CLK_TYPE, PNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(ipa_clk, ipa_a_clk, RPM_IPA_CLK_TYPE, IPA_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_CLK_ID);

/* SMD_XO_BUFFER */
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk1, bb_clk1_a, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk2, bb_clk2_a, BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk1, rf_clk1_a, RF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk2, rf_clk2_a, RF_CLK2_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk1_pin, bb_clk1_a_pin, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk2_pin, bb_clk2_a_pin, BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk1_pin, rf_clk1_a_pin, RF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk2_pin, rf_clk2_a_pin, RF_CLK2_ID);

/* Voter clocks */
static DEFINE_CLK_VOTER(pnoc_msmbus_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_msmbus_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_keepalive_a_clk, &pnoc_a_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_usb_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_usb_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_usb_a_clk, &bimc_a_clk.c, LONG_MAX);

/* Branch Voter clocks */
static DEFINE_CLK_BRANCH_VOTER(xo_gcc, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_otg_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_lpm_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_pronto_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_mss_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_wlan_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_lpass_clk, &xo_clk_src.c);

enum vdd_sr2_pll_levels {
	VDD_SR2_PLL_OFF,
	VDD_SR2_PLL_SVS,
	VDD_SR2_PLL_NOM,
	VDD_SR2_PLL_TUR,
	VDD_SR2_PLL_NUM,
};

static int vdd_sr2_levels[] = {
	0,	 RPM_REGULATOR_CORNER_NONE,		/* VDD_SR2_PLL_OFF */
	1800000, RPM_REGULATOR_CORNER_SVS_SOC,		/* VDD_SR2_PLL_SVS */
	1800000, RPM_REGULATOR_CORNER_NORMAL,		/* VDD_SR2_PLL_NOM */
	1800000, RPM_REGULATOR_CORNER_SUPER_TURBO,	/* VDD_SR2_PLL_TUR */
};

static DEFINE_VDD_REGULATORS(vdd_sr2_pll, VDD_SR2_PLL_NUM, 2,
				vdd_sr2_levels, NULL);

enum vdd_hf_pll_levels {
	VDD_HF_PLL_OFF,
	VDD_HF_PLL_SVS,
	VDD_HF_PLL_NOM,
	VDD_HF_PLL_TUR,
	VDD_HF_PLL_NUM,
};

static int vdd_hf_levels[] = {
	0,	 RPM_REGULATOR_CORNER_NONE,		/* VDD_HF_PLL_OFF */
	1800000, RPM_REGULATOR_CORNER_SVS_SOC,		/* VDD_HF_PLL_SVS */
	1800000, RPM_REGULATOR_CORNER_NORMAL,		/* VDD_HF_PLL_NOM */
	1800000, RPM_REGULATOR_CORNER_SUPER_TURBO,	/* VDD_HF_PLL_TUR */
};
static DEFINE_VDD_REGULATORS(vdd_hf_pll, VDD_HF_PLL_NUM, 2,
				vdd_hf_levels, NULL);

static struct pll_freq_tbl apcs_cci_pll_freq[] = {
	F_APCS_PLL(198400000, 31, 0x0, 0x1, 0x0, 0x2, 0x0),
	F_APCS_PLL(230400000, 24, 0x0, 0x1, 0x0, 0x1, 0x0),
	F_APCS_PLL(297600000, 31, 0x0, 0x1, 0x0, 0x1, 0x0),
	F_APCS_PLL(307200000, 16, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(345600000, 18, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(348000000, 20, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(460800000, 24, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(499200000, 26, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(518400000, 27, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(556800000, 29, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(576000000, 30, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(595200000, 31, 0x0, 0x1, 0x0, 0x0, 0x0),
};

static struct pll_clk a53ss_cci_pll = {
	.mode_reg = (void __iomem *)APCS_CCI_PLL_MODE,
	.l_reg = (void __iomem *)APCS_CCI_PLL_L_VAL,
	.m_reg = (void __iomem *)APCS_CCI_PLL_M_VAL,
	.n_reg = (void __iomem *)APCS_CCI_PLL_N_VAL,
	.config_reg = (void __iomem *)APCS_CCI_PLL_USER_CTL,
	.status_reg = (void __iomem *)APCS_CCI_PLL_STATUS,
	.freq_tbl = apcs_cci_pll_freq,
	.masks = {
		.vco_mask = BM(29, 28),
		.pre_div_mask = BIT(12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
	},
	.base = &virt_bases[APCS_CCI_PLL_BASE],
	.c = {
		.parent = &xo_a_clk_src.c,
		.dbg_name = "a53ss_cci_pll",
		.ops = &clk_ops_sr2_pll,
		.vdd_class = &vdd_sr2_pll,
		.fmax = (unsigned long [VDD_SR2_PLL_NUM]) {
			[VDD_SR2_PLL_SVS] = 1000000000,
			[VDD_SR2_PLL_NOM] = 1900000000,
		},
		.num_fmax = VDD_SR2_PLL_NUM,
		CLK_INIT(a53ss_cci_pll.c),
	},
};

static struct pll_freq_tbl apcs_c0_pll_freq[] = {
	F_APCS_PLL( 249600000,  26, 0x0, 0x1, 0x0, 0x1, 0x0),
	F_APCS_PLL( 307200000,  16, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 345600000,  18, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 384000000,  20, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 460800000,  24, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 499200000,  52, 0x0, 0x1, 0x0, 0x1, 0x0),
	F_APCS_PLL( 518400000,  27, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 844800000,  44, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 883200000,  46, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 921600000,  48, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 998400000,  52, 0x0, 0x1, 0x0, 0x0, 0x0),
};

static struct pll_clk a53ss_c0_pll = {
	.mode_reg = (void __iomem *)APCS_C0_PLL_MODE,
	.l_reg = (void __iomem *)APCS_C0_PLL_L_VAL,
	.m_reg = (void __iomem *)APCS_C0_PLL_M_VAL,
	.n_reg = (void __iomem *)APCS_C0_PLL_N_VAL,
	.config_reg = (void __iomem *)APCS_C0_PLL_USER_CTL,
	.status_reg = (void __iomem *)APCS_C0_PLL_STATUS,
	.freq_tbl = apcs_c0_pll_freq,
	.masks = {
		.vco_mask = BM(29, 28),
		.pre_div_mask = BIT(12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
	},
	.base = &virt_bases[APCS_C0_PLL_BASE],
	.c = {
		.parent = &xo_a_clk_src.c,
		.dbg_name = "a53ss_c0_pll",
		.ops = &clk_ops_sr2_pll,
		.vdd_class = &vdd_sr2_pll,
		.fmax = (unsigned long [VDD_SR2_PLL_NUM]) {
			[VDD_SR2_PLL_SVS] = 1000000000,
			[VDD_SR2_PLL_NOM] = 1900000000,
		},
		.num_fmax = VDD_SR2_PLL_NUM,
		CLK_INIT(a53ss_c0_pll.c),
	},
};

static struct pll_freq_tbl apcs_c1_pll_freq[] = {
	F_APCS_PLL( 345600000, 36, 0x0, 0x1, 0x0, 0x1, 0x0),
	F_APCS_PLL( 422400000, 44, 0x0, 0x1, 0x0, 0x1, 0x0),
	F_APCS_PLL( 499200000, 52, 0x0, 0x1, 0x0, 0x1, 0x0),
	F_APCS_PLL( 652800000, 34, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 729600000, 38, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 806400000, 42, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 844800000, 44, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 883200000, 46, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 960000000, 50, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1036800000, 54, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1094400000, 57, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1113600000, 58, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1190400000, 62, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1267200000, 66, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1344000000, 70, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1420800000, 74, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1497600000, 78, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1536000000, 80, 0x0, 0x1, 0x0, 0x0, 0x0),
};

static struct pll_clk a53ss_c1_pll = {
	.mode_reg = (void __iomem *)APCS_C1_PLL_MODE,
	.l_reg = (void __iomem *)APCS_C1_PLL_L_VAL,
	.m_reg = (void __iomem *)APCS_C1_PLL_M_VAL,
	.n_reg = (void __iomem *)APCS_C1_PLL_N_VAL,
	.config_reg = (void __iomem *)APCS_C1_PLL_USER_CTL,
	.status_reg = (void __iomem *)APCS_C1_PLL_STATUS,
	.freq_tbl = apcs_c1_pll_freq,
	.masks = {
		.vco_mask = BM(29, 28),
		.pre_div_mask = BIT(12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
	},
	.base = &virt_bases[APCS_C1_PLL_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.dbg_name = "a53ss_c1_pll",
		.ops = &clk_ops_sr2_pll,
		.vdd_class = &vdd_hf_pll,
		.fmax = (unsigned long [VDD_HF_PLL_NUM]) {
			[VDD_HF_PLL_SVS] = 1000000000,
			[VDD_HF_PLL_NOM] = 2000000000,
		},
		.num_fmax = VDD_HF_PLL_NUM,
		CLK_INIT(a53ss_c1_pll.c),
	},
};

static unsigned int soft_vote_gpll0;

/* PLL_ACTIVE_FLAG bit of GCC_GPLL0_MODE register
 * gets set from PLL voting FSM.It indicates when
 * FSM has enabled the PLL and PLL should be locked.
 */
static struct pll_vote_clk gpll0_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 800000000,
		.dbg_name = "gpll0_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_clk_src.c),
	},
};

DEFINE_EXT_CLK(gpll0_out_aux_clk_src, &gpll0_clk_src.c);

/* Don't vote for xo if using this clock to allow xo shutdown */
static struct pll_vote_clk gpll0_ao_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_a_clk_src.c,
		.rate = 800000000,
		.dbg_name = "gpll0_ao_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao_clk_src.c),
	},
};

static struct pll_vote_clk gpll6_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(7),
	.status_reg = (void __iomem *)GPLL6_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 1080000000,
		.dbg_name = "gpll6_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll6_clk_src.c),
	},
};

DEFINE_EXT_CLK(gpll6_out_aux_clk_src, &gpll6_clk_src.c);
DEFINE_EXT_CLK(gpll6_out_main_clk_src, &gpll6_clk_src.c);

static struct alpha_pll_masks pll_masks_p = {
	.lock_mask = BIT(31),
	.active_mask = BIT(30),
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
};

static struct alpha_pll_vco_tbl p_vco[] = {
	VCO(3,  250000000,  500000000),
	VCO(2,  500000000, 1000000000),
	VCO(1, 1000000000, 1500000000),
	VCO(0, 1500000000, 2000000000),
};

static struct alpha_pll_clk gpll3_clk_src = {
	.masks = &pll_masks_p,
	.base = &virt_bases[GCC_BASE],
	.offset = GPLL3_MODE,
	.vco_tbl = p_vco,
	.num_vco = ARRAY_SIZE(p_vco),
	.fsm_reg_offset = APCS_GPLL_ENA_VOTE,
	.fsm_en_mask =  BIT(4),
	.enable_config = 1,
	.c = {
		.rate =  1160000000,
		.parent = &xo_clk_src.c,
		.dbg_name = "gpll3_clk_src",
		.ops = &clk_ops_fixed_alpha_pll,
		CLK_INIT(gpll3_clk_src.c),
	},
};

static struct alpha_pll_clk gpll4_clk_src = {
	.masks = &pll_masks_p,
	.base = &virt_bases[GCC_BASE],
	.offset = GPLL4_MODE,
	.vco_tbl = p_vco,
	.num_vco = ARRAY_SIZE(p_vco),
	.fsm_reg_offset = APCS_GPLL_ENA_VOTE,
	.fsm_en_mask =  BIT(5),
	.enable_config = 1,
	.c = {
		.rate =  1200000000,
		.parent = &xo_clk_src.c,
		.dbg_name = "gpll4_clk_src",
		.ops = &clk_ops_fixed_alpha_pll,
		CLK_INIT(gpll4_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_top_ahb_clk[] = {
	F( 40000000,	gpll0,	10,	1,	2),
	F( 80000000,	gpll0,	10,	0,	0),
	F_END
};

static struct rcg_clk camss_top_ahb_clk_src = {
	.cmd_rcgr_reg =  CAMSS_TOP_AHB_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_top_ahb_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "camss_top_ahb_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 40000000, NOMINAL, 80000000),
		CLK_INIT(camss_top_ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_apss_ahb_clk[] = {
	F( 19200000,	xo,	1,	0,	0),
	F( 50000000,	gpll0,	16,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 133330000,	gpll0,	6,	0,	0),
	F_END
};

static struct rcg_clk apss_ahb_clk_src = {
	.cmd_rcgr_reg = APSS_AHB_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_apss_ahb_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "apss_ahb_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(apss_ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_csi0_2_clk[] = {
	F( 100000000,	gpll0,	8,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F_END
};

static struct rcg_clk csi0_clk_src = {
	.cmd_rcgr_reg = CSI0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_camss_csi0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
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
	.freq_tbl = ftbl_gcc_camss_csi0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
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
	.freq_tbl = ftbl_gcc_camss_csi0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "csi2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_venus0_vcodec0_clk[] = {
	F( 133330000,	gpll0,	6,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F_END
};

static struct rcg_clk vcodec0_clk_src = {
	.cmd_rcgr_reg =  VCODEC0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_venus0_vcodec0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "vcodec0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 200000000,
						HIGH, 266670000),
		CLK_INIT(vcodec0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_vfe0_1_clk[] = {
	F( 50000000,	gpll0,	16,	0,	0),
	F( 80000000,	gpll0,	10,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 133330000,	gpll0,	6,	0,	0),
	F( 160000000,	gpll0,	5,	0,	0),
	F( 177780000,	gpll0,	4.5,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 320000000,	gpll0,	2.5,	0,	0),
	F_END
};

static struct rcg_clk vfe0_clk_src = {
	.cmd_rcgr_reg = VFE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_camss_vfe0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
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
	.freq_tbl = ftbl_gcc_camss_vfe0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "vfe1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
					HIGH, 320000000),
		CLK_INIT(vfe1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_oxili_gfx3d_clk[] = {
	F( 19200000,	xo,		1,	0,	0),
	F( 50000000,	gpll0,		16,	0,	0),
	F( 80000000,	gpll0,		10,	0,	0),
	F( 100000000,	gpll0,		8,	0,	0),
	F( 160000000,	gpll0,		5,	0,	0),
	F( 200000000,	gpll0,		4,	0,	0),
	F( 228570000,	gpll0,		3.5,	0,	0),
	F( 232000000,	gpll3,		5,	0,	0),
	F( 257770000,	gpll3,		4.5,	0,	0),
	F( 266670000,	gpll0,		3,	0,	0),
	F( 290000000,	gpll3,		4,	0,	0),
	F( 331400000,	gpll3,		3.5,	0,	0),
	F( 386670000,	gpll3,		3,	0,	0),
	F( 400000000,	gpll0,		2,	0,	0),
	F( 432000000,	gpll6_out_aux,	2.5,	0,	0),
	F( 464000000,	gpll3,		2.5,	0,	0),
	F( 540000000,	gpll6_out_aux,	2,	0,	0),
	F( 580000000,	gpll3,		2,	0,	0),
	F_END
};

static struct rcg_clk gfx3d_clk_src = {
	.cmd_rcgr_reg = GFX3D_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_oxili_gfx3d_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gfx3d_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 228570000, NOMINAL, 400000000,
						HIGH, 580000000),
		CLK_INIT(gfx3d_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_4_i2c_apps_clk[] = {
	F( 19200000,	xo,	1,	0,	0),
	F( 50000000,	gpll0,	16,	0,	0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_i2c_apps_clk,
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
	F( 960000,	xo,	10,	1,	2),
	F( 4800000,	xo,	4,	0,	0),
	F( 9600000,	xo,	2,	0,	0),
	F( 16000000,	gpll0,	10,	1,	5),
	F( 19200000,	xo,	1,	0,	0),
	F( 25000000,	gpll0,	16,	1,	2),
	F( 50000000,	gpll0,	16,	0,	0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_QUP1_SPI_APPS_CMD_RCGR,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_i2c_apps_clk,
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
	.cmd_rcgr_reg =  BLSP1_QUP2_SPI_APPS_CMD_RCGR,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_i2c_apps_clk,
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
	.cmd_rcgr_reg =  BLSP1_QUP3_SPI_APPS_CMD_RCGR,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_i2c_apps_clk,
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
	.cmd_rcgr_reg =  BLSP1_QUP4_SPI_APPS_CMD_RCGR,
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

static struct clk_freq_tbl ftbl_gcc_blsp1_2_uart1_2_apps_clk[] = {
	F( 3686400,	gpll0,	1,	72,	15625),
	F( 7372800,	gpll0,	1,	144,	15625),
	F( 14745600,	gpll0,	1,	288,	15625),
	F( 16000000,	gpll0,	10,	1,	5),
	F( 19200000,	xo,	1,	0,	0),
	F( 24000000,	gpll0,	1,	3,	100),
	F( 25000000,	gpll0,	16,	1,	2),
	F( 32000000,	gpll0,	1,	1,	25),
	F( 40000000,	gpll0,	1,	1,	20),
	F( 46400000,	gpll0,	1,	29,	500),
	F( 48000000,	gpll0,	1,	3,	50),
	F( 51200000,	gpll0,	1,	8,	125),
	F( 56000000,	gpll0,	1,	7,	100),
	F( 58982400,	gpll0,	1,	1152,	15625),
	F( 60000000,	gpll0,	1,	3,	40),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 32000000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 32000000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_i2c_apps_clk,
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
	.cmd_rcgr_reg =  BLSP2_QUP1_SPI_APPS_CMD_RCGR,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_i2c_apps_clk,
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
	.cmd_rcgr_reg =  BLSP2_QUP2_SPI_APPS_CMD_RCGR,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_i2c_apps_clk,
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
	.cmd_rcgr_reg =  BLSP2_QUP3_SPI_APPS_CMD_RCGR,
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
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_4_i2c_apps_clk,
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
	.cmd_rcgr_reg =  BLSP2_QUP4_SPI_APPS_CMD_RCGR,
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
	.cmd_rcgr_reg =  BLSP2_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 32000000, NOMINAL, 64000000),
		CLK_INIT(blsp2_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart2_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP2_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 32000000, NOMINAL, 64000000),
		CLK_INIT(blsp2_uart2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_cci_clk[] = {
	F( 19200000,	xo,		1,	0,	0),
	F( 37500000,	gpll0_out_aux,	1,	3,	64),
	F_END
};

static struct rcg_clk cci_clk_src = {
	.cmd_rcgr_reg =  CCI_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_cci_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "cci_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 19200000, NOMINAL, 37500000),
		CLK_INIT(cci_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_cpp_clk[] = {
	F( 133330000,	gpll0,	6,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 320000000,	gpll0,	2.5,	0,	0),
	F_END
};

static struct rcg_clk cpp_clk_src = {
	.cmd_rcgr_reg = CPP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_camss_cpp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "cpp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
						HIGH, 320000000),
		CLK_INIT(cpp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_gp0_1_clk[] = {
	F( 100000000,	gpll0,	8,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F_END
};

static struct rcg_clk camss_gp0_clk_src = {
	.cmd_rcgr_reg =  MM_GP0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "camss_gp0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(camss_gp0_clk_src.c),
	},
};

static struct rcg_clk camss_gp1_clk_src = {
	.cmd_rcgr_reg =  MM_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "camss_gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(camss_gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_jpeg0_clk[] = {
	F( 133330000,	gpll0,	6,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 320000000,	gpll0,	2.5,	0,	0),
	F_END
};

static struct rcg_clk jpeg0_clk_src = {
	.cmd_rcgr_reg = JPEG0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_camss_jpeg0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "jpeg0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
						HIGH, 320000000),
		CLK_INIT(jpeg0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_mclk0_2_clk[] = {
	F( 24000000,	gpll6,	1,	1,	45),
	F( 66670000,	gpll0,	12,	0,	0),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg =  MCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_mclk0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "mclk0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk0_clk_src.c),
	},
};

static struct rcg_clk mclk1_clk_src = {
	.cmd_rcgr_reg =  MCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_mclk0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk1_clk_src.c),
	},
};

static struct rcg_clk mclk2_clk_src = {
	.cmd_rcgr_reg =  MCLK2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_mclk0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "mclk2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_csi0_1phytimer_clk[] = {
	F( 100000000,	gpll0,	8,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F_END
};

static struct rcg_clk csi0phytimer_clk_src = {
	.cmd_rcgr_reg = CSI0PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_camss_csi0_1phytimer_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
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
	.freq_tbl = ftbl_gcc_camss_csi0_1phytimer_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "csi1phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi1phytimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_crypto_clk[] = {
	F( 50000000,	gpll0,	16,	0,	0),
	F( 80000000,	gpll0,	10,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 160000000,	gpll0,	5,	0,	0),
	F_END
};

static struct rcg_clk crypto_clk_src = {
	.cmd_rcgr_reg = CRYPTO_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_crypto_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "crypto_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 80000000, NOMINAL, 160000000),
		CLK_INIT(crypto_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gp1_3_clk[] = {
	F( 19200000,	xo,	1,	0,	0),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg =  GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(gp1_clk_src.c),
	},
};

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg =  GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(gp2_clk_src.c),
	},
};

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg =  GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_byte0_clk[] = {
	F_MM(dsi0_phypll),
};

static struct rcg_clk byte0_clk_src = {
	.cmd_rcgr_reg = BYTE0_CMD_RCGR,
	.current_freq = ftbl_gcc_mdss_byte0_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "byte0_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP2(LOW, 112500000, NOMINAL, 187500000),
		CLK_INIT(byte0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_esc0_clk[] = {
	F( 19200000,	xo,	1,	0,	0),
	F_END
};

static struct rcg_clk esc0_clk_src = {
	.cmd_rcgr_reg = ESC0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_mdss_esc0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "esc0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(esc0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_mdp_clk[] = {
	F( 50000000,	gpll0,	16,	0,	0),
	F( 80000000,	gpll0,	10,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 145450000,	gpll0,	5.5,	0,	0),
	F( 160000000,	gpll0,	5,	0,	0),
	F( 177780000,	gpll0,	4.5,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 320000000,	gpll0,	2.5,	0,	0),
	F_END
};

static struct rcg_clk mdp_clk_src = {
	.cmd_rcgr_reg = MDP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_mdss_mdp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "mdp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 160000000, NOMINAL, 266670000,
							HIGH, 320000000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_pclk0_clk[] = {
	F_MM(dsi0_phypll)
};

static struct rcg_clk pclk0_clk_src = {
	.cmd_rcgr_reg =  PCLK0_CMD_RCGR,
	.current_freq = ftbl_gcc_mdss_pclk0_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pclk0_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 250000000),
		CLK_INIT(pclk0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_vsync_clk[] = {
	F( 19200000,	xo,	1,	0,	0),
	F_END
};

static struct rcg_clk vsync_clk_src = {
	.cmd_rcgr_reg = VSYNC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_mdss_vsync_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "vsync_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(vsync_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pdm2_clk[] = {
	F( 64000000,	gpll0,	12.5,	0,	0),
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
		VDD_DIG_FMAX_MAP1(LOW, 64000000),
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc1_2_apps_clk[] = {
	F( 144000,	xo,	16,	3,	25),
	F( 400000,	xo,	12,	1,	4),
	F( 20000000,	gpll0,	10,	1,	4),
	F( 25000000,	gpll0,	16,	1,	2),
	F( 50000000,	gpll0,	16,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 177770000,	gpll0,	4.5,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg =  SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 400000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg =  SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_fs_ic_clk[] = {
	F( 60000000,	gpll6_out_main,	9,	1,	2),
	F_END
};

static struct rcg_clk usb_fs_ic_clk_src = {
	.cmd_rcgr_reg =  USB_FS_IC_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_usb_fs_ic_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_fs_ic_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(usb_fs_ic_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_fs_system_clk[] = {
	F( 64000000,	gpll0_out_aux,	12.5,	0,	0),
	F_END
};

static struct rcg_clk usb_fs_system_clk_src = {
	.cmd_rcgr_reg =  USB_FS_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_usb_fs_system_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_fs_system_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 64000000),
		CLK_INIT(usb_fs_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F( 57140000,	gpll0,	14,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 133330000,	gpll0,	6,	0,	0),
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
		VDD_DIG_FMAX_MAP3(LOW, 57140000, NOMINAL, 100000000,
						HIGH, 133330000),
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sys_mm_noc_axi_clk[] = {
	F( 19200000,	xo,	1,	0,	0),
	F( 50000000,	gpll0,	16,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 133330000,	gpll0,	6,	0,	0),
	F( 160000000,	gpll0,	5,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 320000000,	gpll0,	2.5,	0,	0),
	F( 400000000,	gpll0,	2,	0,	0),
	F_END
};

static struct rcg_clk sys_mm_noc_axi_clk_src = {
	.cmd_rcgr_reg = SYSTEM_MM_NOC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_sys_mm_noc_axi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sys_mm_noc_axi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 160000000, NOMINAL, 320000000,
					HIGH, 400000000),
		CLK_INIT(sys_mm_noc_axi_clk_src.c),
	},
};

static struct branch_clk gcc_bimc_gpu_clk = {
	.cbcr_reg = BIMC_GPU_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_bimc_gpu_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_bimc_gpu_clk.c),
	},
};

static struct local_vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg = BLSP1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(10),
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

static struct local_vote_clk gcc_blsp2_ahb_clk = {
	.cbcr_reg = BLSP2_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(20),
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

static struct local_vote_clk gcc_boot_rom_ahb_clk = {
	.cbcr_reg = BOOT_ROM_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(7),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_boot_rom_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_boot_rom_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_cci_ahb_clk = {
	.cbcr_reg = CAMSS_CCI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_cci_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_cci_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_cci_clk = {
	.cbcr_reg = CAMSS_CCI_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_cci_clk",
		.parent = &cci_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_cci_clk.c),
	},
};

static struct branch_clk gcc_camss_cpp_ahb_clk = {
	.cbcr_reg = CAMSS_CPP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_cpp_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_cpp_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_cpp_axi_clk = {
	.cbcr_reg = CAMSS_CPP_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_cpp_axi_clk",
		.parent = &sys_mm_noc_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_cpp_axi_clk.c),
	},
};

static struct branch_clk gcc_camss_cpp_clk = {
	.cbcr_reg = CAMSS_CPP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_cpp_clk",
		.parent = &cpp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_cpp_clk.c),
	},
};

static struct branch_clk gcc_camss_csi0_ahb_clk = {
	.cbcr_reg = CAMSS_CSI0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi0_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi0_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_csi0_clk = {
	.cbcr_reg = CAMSS_CSI0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi0_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi0_clk.c),
	},
};

static struct branch_clk gcc_camss_csi0phy_clk = {
	.cbcr_reg = CAMSS_CSI0PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi0phy_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi0phy_clk.c),
	},
};

static struct branch_clk gcc_camss_csi0pix_clk = {
	.cbcr_reg = CAMSS_CSI0PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi0pix_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi0pix_clk.c),
	},
};

static struct branch_clk gcc_camss_csi0rdi_clk = {
	.cbcr_reg = CAMSS_CSI0RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi0rdi_clk",
		.parent = &csi0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi0rdi_clk.c),
	},
};

static struct branch_clk gcc_camss_csi1_ahb_clk = {
	.cbcr_reg = CAMSS_CSI1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi1_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi1_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_csi1_clk = {
	.cbcr_reg = CAMSS_CSI1_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi1_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi1_clk.c),
	},
};

static struct branch_clk gcc_camss_csi1phy_clk = {
	.cbcr_reg = CAMSS_CSI1PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi1phy_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi1phy_clk.c),
	},
};

static struct branch_clk gcc_camss_csi1pix_clk = {
	.cbcr_reg = CAMSS_CSI1PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi1pix_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi1pix_clk.c),
	},
};

static struct branch_clk gcc_camss_csi1rdi_clk = {
	.cbcr_reg = CAMSS_CSI1RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi1rdi_clk",
		.parent = &csi1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi1rdi_clk.c),
	},
};

static struct branch_clk gcc_camss_csi2_ahb_clk = {
	.cbcr_reg = CAMSS_CSI2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi2_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi2_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_csi2_clk = {
	.cbcr_reg = CAMSS_CSI2_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi2_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi2_clk.c),
	},
};

static struct branch_clk gcc_camss_csi2phy_clk = {
	.cbcr_reg = CAMSS_CSI2PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi2phy_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi2phy_clk.c),
	},
};

static struct branch_clk gcc_camss_csi2pix_clk = {
	.cbcr_reg = CAMSS_CSI2PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi2pix_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi2pix_clk.c),
	},
};

static struct branch_clk gcc_camss_csi2rdi_clk = {
	.cbcr_reg = CAMSS_CSI2RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi2rdi_clk",
		.parent = &csi2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi2rdi_clk.c),
	},
};

static struct branch_clk gcc_camss_csi_vfe0_clk = {
	.cbcr_reg = CAMSS_CSI_VFE0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi_vfe0_clk",
		.parent = &vfe0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi_vfe0_clk.c),
	},
};

static struct branch_clk gcc_camss_csi_vfe1_clk = {
	.cbcr_reg = CAMSS_CSI_VFE1_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi_vfe1_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi_vfe1_clk.c),
	},
};

static struct branch_clk gcc_camss_gp0_clk = {
	.cbcr_reg = CAMSS_GP0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_gp0_clk",
		.parent = &camss_gp0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_gp0_clk.c),
	},
};

static struct branch_clk gcc_camss_gp1_clk = {
	.cbcr_reg = CAMSS_GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_gp1_clk",
		.parent = &camss_gp1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_gp1_clk.c),
	},
};

static struct branch_clk gcc_camss_ispif_ahb_clk = {
	.cbcr_reg = CAMSS_ISPIF_AHB_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_ispif_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_ispif_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_jpeg0_clk = {
	.cbcr_reg = CAMSS_JPEG0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_jpeg0_clk",
		.parent = &jpeg0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_jpeg0_clk.c),
	},
};

static struct branch_clk gcc_camss_jpeg_ahb_clk = {
	.cbcr_reg = CAMSS_JPEG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_jpeg_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_jpeg_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_jpeg_axi_clk = {
	.cbcr_reg = CAMSS_JPEG_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_jpeg_axi_clk",
		.parent = &sys_mm_noc_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_jpeg_axi_clk.c),
	},
};

static struct branch_clk gcc_camss_mclk0_clk = {
	.cbcr_reg = CAMSS_MCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_mclk0_clk",
		.parent = &mclk0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_mclk0_clk.c),
	},
};

static struct branch_clk gcc_camss_mclk1_clk = {
	.cbcr_reg = CAMSS_MCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_mclk1_clk",
		.parent = &mclk1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_mclk1_clk.c),
	},
};

static struct branch_clk gcc_camss_mclk2_clk = {
	.cbcr_reg = CAMSS_MCLK2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_mclk2_clk",
		.parent = &mclk2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_mclk2_clk.c),
	},
};

static struct branch_clk gcc_camss_micro_ahb_clk = {
	.cbcr_reg = CAMSS_MICRO_AHB_CBCR,
	.bcr_reg =  CAMSS_MICRO_BCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_micro_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_micro_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_csi0phytimer_clk = {
	.cbcr_reg = CAMSS_CSI0PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi0phytimer_clk",
		.parent = &csi0phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi0phytimer_clk.c),
	},
};

static struct branch_clk gcc_camss_csi1phytimer_clk = {
	.cbcr_reg = CAMSS_CSI1PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_csi1phytimer_clk",
		.parent = &csi1phytimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_csi1phytimer_clk.c),
	},
};

static struct branch_clk gcc_camss_ahb_clk = {
	.cbcr_reg = CAMSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_top_ahb_clk = {
	.cbcr_reg = CAMSS_TOP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_top_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_top_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_vfe0_clk = {
	.cbcr_reg = CAMSS_VFE0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_vfe0_clk",
		.parent = &vfe0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_vfe0_clk.c),
	},
};

static struct branch_clk gcc_camss_vfe_ahb_clk = {
	.cbcr_reg = CAMSS_VFE_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_vfe_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_vfe_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_vfe_axi_clk = {
	.cbcr_reg = CAMSS_VFE_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_vfe_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_vfe_axi_clk.c),
	},
};

static struct branch_clk gcc_camss_vfe1_ahb_clk = {
	.cbcr_reg = CAMSS_VFE1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_vfe1_ahb_clk",
		.parent = &camss_top_ahb_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_vfe1_ahb_clk.c),
	},
};

static struct branch_clk gcc_camss_vfe1_axi_clk = {
	.cbcr_reg = CAMSS_VFE1_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_vfe1_axi_clk",
		.parent = &sys_mm_noc_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_vfe1_axi_clk.c),
	},
};

static struct branch_clk gcc_camss_vfe1_clk = {
	.cbcr_reg = CAMSS_VFE1_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_camss_vfe1_clk",
		.parent = &vfe1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_camss_vfe1_clk.c),
	},
};

static struct local_vote_clk gcc_crypto_ahb_clk = {
	.cbcr_reg = CRYPTO_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_crypto_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_crypto_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_crypto_axi_clk = {
	.cbcr_reg = CRYPTO_AXI_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(1),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_crypto_axi_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_crypto_axi_clk.c),
	},
};

static struct local_vote_clk gcc_crypto_clk = {
	.cbcr_reg = CRYPTO_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(2),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_crypto_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_crypto_clk.c),
	},
};

static struct branch_clk gcc_oxili_gmem_clk = {
	.cbcr_reg = OXILI_GMEM_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_oxili_gmem_clk",
		.parent = &gfx3d_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_oxili_gmem_clk.c),
	},
};

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp1_clk",
		.parent = &gp1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp1_clk.c),
	},
};

static struct branch_clk gcc_gp2_clk = {
	.cbcr_reg = GP2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp2_clk",
		.parent = &gp2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp2_clk.c),
	},
};

static struct branch_clk gcc_gp3_clk = {
	.cbcr_reg = GP3_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp3_clk",
		.parent = &gp3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_mdss_ahb_clk = {
	.cbcr_reg = MDSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mdss_ahb_clk.c),
	},
};

static struct branch_clk gcc_mdss_axi_clk = {
	.cbcr_reg = MDSS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mdss_axi_clk.c),
	},
};

static struct branch_clk gcc_mdss_byte0_clk = {
	.cbcr_reg = MDSS_BYTE0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_byte0_clk",
		.parent = &byte0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mdss_byte0_clk.c),
	},
};

static struct branch_clk gcc_mdss_esc0_clk = {
	.cbcr_reg = MDSS_ESC0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_esc0_clk",
		.parent = &esc0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mdss_esc0_clk.c),
	},
};

static struct branch_clk gcc_mdss_mdp_clk = {
	.cbcr_reg = MDSS_MDP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_mdp_clk",
		.parent = &mdp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mdss_mdp_clk.c),
	},
};

static struct branch_clk gcc_mdss_pclk0_clk = {
	.cbcr_reg = MDSS_PCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_pclk0_clk",
		.parent = &pclk0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mdss_pclk0_clk.c),
	},
};

static struct branch_clk gcc_mdss_vsync_clk = {
	.cbcr_reg = MDSS_VSYNC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_vsync_clk",
		.parent = &vsync_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mdss_vsync_clk.c),
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

static struct branch_clk gcc_bimc_gfx_clk = {
	.cbcr_reg = BIMC_GFX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_bimc_gfx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_bimc_gfx_clk.c),
	},
};

static struct branch_clk gcc_oxili_ahb_clk = {
	.cbcr_reg = OXILI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_oxili_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_oxili_ahb_clk.c),
	},
};

static struct branch_clk gcc_oxili_gfx3d_clk = {
	.cbcr_reg = OXILI_GFX3D_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_oxili_gfx3d_clk",
		.parent = &gfx3d_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_oxili_gfx3d_clk.c),
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
	.en_mask = BIT(8),
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

static struct local_vote_clk gcc_cpp_tbu_clk = {
	.cbcr_reg = CPP_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(14),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_cpp_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_cpp_tbu_clk.c),
	},
};

static struct local_vote_clk gcc_gfx_tbu_clk = {
	.cbcr_reg = GFX_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(3),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gfx_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_gfx_tbu_clk.c),
	},
};

static struct local_vote_clk gcc_gfx_tcu_clk = {
	.cbcr_reg = GFX_TCU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(2),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gfx_tcu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_gfx_tcu_clk.c),
	},
};

static struct local_vote_clk gcc_gtcu_ahb_clk = {
	.cbcr_reg = GTCU_AHB_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(13),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gtcu_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_gtcu_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ipa_tbu_clk = {
	.cbcr_reg = IPA_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(16),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ipa_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ipa_tbu_clk.c),
	},
};

static struct local_vote_clk gcc_jpeg_tbu_clk = {
	.cbcr_reg = JPEG_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(10),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_jpeg_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_jpeg_tbu_clk.c),
	},
};

static struct local_vote_clk gcc_mdp_tbu_clk = {
	.cbcr_reg = MDP_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(4),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdp_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_mdp_tbu_clk.c),
	},
};

static struct local_vote_clk gcc_smmu_cfg_clk = {
	.cbcr_reg = SMMU_CFG_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(12),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_smmu_cfg_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_smmu_cfg_clk.c),
	},
};

static struct local_vote_clk gcc_venus_tbu_clk = {
	.cbcr_reg = VENUS_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(5),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_venus_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_venus_tbu_clk.c),
	},
};

static struct local_vote_clk gcc_vfe1_tbu_clk = {
	.cbcr_reg = VFE1_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_vfe1_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_vfe1_tbu_clk.c),
	},
};

static struct local_vote_clk gcc_vfe_tbu_clk = {
	.cbcr_reg = VFE_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(9),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_vfe_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_vfe_tbu_clk.c),
	},
};

static struct branch_clk gcc_sys_mm_noc_axi_clk = {
	.cbcr_reg = SYS_MM_NOC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sys_mm_noc_axi_clk",
		.parent = &sys_mm_noc_axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_mm_noc_axi_clk.c),
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

static struct branch_clk gcc_usb_hs_phy_cfg_ahb_clk = {
	.cbcr_reg = USB_HS_PHY_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs_phy_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_phy_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_fs_ahb_clk = {
	.cbcr_reg = USB_FS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_fs_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_fs_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_fs_ic_clk = {
	.cbcr_reg = USB_FS_IC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_fs_ic_clk",
		.parent = &usb_fs_ic_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_fs_ic_clk.c),
	},
};

static struct branch_clk gcc_usb_fs_system_clk = {
	.cbcr_reg = USB_FS_SYSTEM_CBCR,
	.bcr_reg  = USB_FS_BCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_fs_system_clk",
		.parent = &usb_fs_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_fs_system_clk.c),
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

static struct reset_clk gcc_usb2_hs_phy_only_clk = {
	.reset_reg = USB2_HS_PHY_ONLY_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2_hs_phy_only_clk",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb2_hs_phy_only_clk.c),
	},
};

static struct reset_clk gcc_qusb2_phy_clk = {
	.reset_reg = QUSB2_PHY_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_qusb2_phy_clk",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_qusb2_phy_clk.c),
	},
};

static struct branch_clk gcc_venus0_ahb_clk = {
	.cbcr_reg = VENUS0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_venus0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_venus0_ahb_clk.c),
	},
};

static struct branch_clk gcc_venus0_axi_clk = {
	.cbcr_reg = VENUS0_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_venus0_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_venus0_axi_clk.c),
	},
};

static struct branch_clk gcc_venus0_core0_vcodec0_clk = {
	.cbcr_reg = VENUS0_CORE0_VCODEC0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_venus0_core0_vcodec0_clk",
		.parent = &vcodec0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_venus0_core0_vcodec0_clk.c),
	},
};

static struct branch_clk gcc_venus0_core1_vcodec0_clk = {
	.cbcr_reg = VENUS0_CORE1_VCODEC0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_venus0_core1_vcodec0_clk",
		.parent = &vcodec0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_venus0_core1_vcodec0_clk.c),
	},
};

static struct branch_clk gcc_venus0_vcodec0_clk = {
	.cbcr_reg = VENUS0_VCODEC0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_venus0_vcodec0_clk",
		.parent = &vcodec0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_venus0_vcodec0_clk.c),
	},
};

static struct clk_ops clk_ops_debug_mux;

static struct measure_clk_data debug_mux_priv = {
	.cxo = &xo_clk_src.c,
	.plltest_reg = GCC_PLLTEST_PAD_CFG,
	.plltest_val = 0x51A00,
	.xo_div4_cbcr = GCC_XO_DIV4_CBCR,
	.ctl_reg = CLOCK_FRQ_MEASURE_CTL,
	.status_reg = CLOCK_FRQ_MEASURE_STATUS,
	.base = &virt_bases[GCC_BASE],
};

static struct mux_clk gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.ops = &mux_reg_ops,
	.offset = GCC_DEBUG_CLK_CTL,
	.mask = 0x1FF,
	.en_offset = GCC_DEBUG_CLK_CTL,
	.en_mask = BIT(16),
	.base = &virt_bases[GCC_BASE],
	MUX_SRC_LIST(
		{ &snoc_clk.c,  0x0000 },
		{ &gcc_sys_mm_noc_axi_clk.c, 0x0001 },
		{ &pnoc_clk.c, 0x0008 },
		{ &bimc_clk.c,  0x0154 },
		{ &gcc_gp1_clk.c, 0x0010 },
		{ &gcc_gp2_clk.c, 0x0011 },
		{ &gcc_gp3_clk.c, 0x0012 },
		{ &gcc_bimc_gfx_clk.c, 0x002d },
		{ &gcc_mss_cfg_ahb_clk.c, 0x0030 },
		{ &gcc_mss_q6_bimc_axi_clk.c, 0x0031 },
		{ &gcc_mdp_tbu_clk.c, 0x0051 },
		{ &gcc_gfx_tbu_clk.c, 0x0052 },
		{ &gcc_gfx_tcu_clk.c, 0x0053 },
		{ &gcc_venus_tbu_clk.c, 0x0054 },
		{ &gcc_gtcu_ahb_clk.c, 0x0058 },
		{ &gcc_vfe_tbu_clk.c, 0x005a },
		{ &gcc_smmu_cfg_clk.c, 0x005b },
		{ &gcc_jpeg_tbu_clk.c, 0x005c },
		{ &gcc_usb_hs_system_clk.c, 0x0060 },
		{ &gcc_usb_hs_ahb_clk.c, 0x0061 },
		{ &gcc_usb2a_phy_sleep_clk.c, 0x0063 },
		{ &gcc_usb_hs_phy_cfg_ahb_clk.c, 0x0064 },
		{ &gcc_sdcc1_apps_clk.c, 0x0068 },
		{ &gcc_sdcc1_ahb_clk.c, 0x0069 },
		{ &gcc_sdcc2_apps_clk.c, 0x0070 },
		{ &gcc_sdcc2_ahb_clk.c, 0x0071 },
		{ &gcc_blsp1_ahb_clk.c, 0x0088 },
		{ &gcc_blsp1_qup1_spi_apps_clk.c, 0x008a },
		{ &gcc_blsp1_qup1_i2c_apps_clk.c, 0x008b },
		{ &gcc_blsp1_uart1_apps_clk.c, 0x008c },
		{ &gcc_blsp1_qup2_spi_apps_clk.c, 0x008e },
		{ &gcc_blsp1_qup2_i2c_apps_clk.c, 0x0090 },
		{ &gcc_blsp1_uart2_apps_clk.c, 0x0091 },
		{ &gcc_blsp1_qup3_spi_apps_clk.c, 0x0093 },
		{ &gcc_blsp1_qup3_i2c_apps_clk.c, 0x0094 },
		{ &gcc_blsp1_qup4_spi_apps_clk.c, 0x0095 },
		{ &gcc_blsp1_qup4_i2c_apps_clk.c, 0x0096 },
		{ &gcc_blsp2_ahb_clk.c, 0x0098 },
		{ &gcc_blsp2_qup1_spi_apps_clk.c, 0x009a },
		{ &gcc_blsp2_qup1_i2c_apps_clk.c, 0x009b },
		{ &gcc_blsp2_uart1_apps_clk.c, 0x009c },
		{ &gcc_blsp2_qup2_spi_apps_clk.c, 0x009e },
		{ &gcc_blsp2_qup2_i2c_apps_clk.c, 0x00a0 },
		{ &gcc_blsp2_uart2_apps_clk.c, 0x00a1 },
		{ &gcc_blsp2_qup3_spi_apps_clk.c, 0x00a3 },
		{ &gcc_blsp2_qup3_i2c_apps_clk.c, 0x00a4 },
		{ &gcc_blsp2_qup4_spi_apps_clk.c, 0x00a5 },
		{ &gcc_blsp2_qup4_i2c_apps_clk.c, 0x00a6 },
		{ &gcc_camss_ahb_clk.c, 0x00a8 },
		{ &gcc_camss_top_ahb_clk.c, 0x00a9 },
		{ &gcc_camss_micro_ahb_clk.c, 0x00aa },
		{ &gcc_camss_gp0_clk.c, 0x00ab },
		{ &gcc_camss_gp1_clk.c, 0x00ac },
		{ &gcc_camss_mclk0_clk.c, 0x00ad },
		{ &gcc_camss_mclk1_clk.c, 0x00ae },
		{ &gcc_camss_cci_clk.c, 0x00af },
		{ &gcc_camss_cci_ahb_clk.c, 0x00b0 },
		{ &gcc_camss_csi0phytimer_clk.c, 0x00b1 },
		{ &gcc_camss_csi1phytimer_clk.c, 0x00b2 },
		{ &gcc_camss_jpeg0_clk.c, 0x00b3 },
		{ &gcc_camss_jpeg_ahb_clk.c, 0x00b4 },
		{ &gcc_camss_jpeg_axi_clk.c, 0x00b5 },
		{ &gcc_camss_vfe0_clk.c, 0x00b8 },
		{ &gcc_camss_cpp_clk.c, 0x00b9 },
		{ &gcc_camss_cpp_ahb_clk.c, 0x00ba },
		{ &gcc_camss_vfe_ahb_clk.c, 0x00bb },
		{ &gcc_camss_vfe_axi_clk.c, 0x00bc },
		{ &gcc_camss_csi_vfe0_clk.c, 0x00bf },
		{ &gcc_camss_csi0_clk.c, 0x00c0 },
		{ &gcc_camss_csi0_ahb_clk.c, 0x00c1 },
		{ &gcc_camss_csi0phy_clk.c, 0x00c2 },
		{ &gcc_camss_csi0rdi_clk.c, 0x00c3 },
		{ &gcc_camss_csi0pix_clk.c, 0x00c4 },
		{ &gcc_camss_csi1_clk.c, 0x00c5 },
		{ &gcc_camss_csi1_ahb_clk.c, 0x00c6 },
		{ &gcc_camss_csi1phy_clk.c, 0x00c7 },
		{ &gcc_pdm_ahb_clk.c, 0x00d0 },
		{ &gcc_pdm2_clk.c, 0x00d2 },
		{ &gcc_prng_ahb_clk.c, 0x00d8 },
		{ &gcc_camss_csi1rdi_clk.c, 0x00e0 },
		{ &gcc_camss_csi1pix_clk.c, 0x00e1 },
		{ &gcc_camss_ispif_ahb_clk.c, 0x00e2 },
		{ &gcc_camss_csi2_clk.c, 0x00e3 },
		{ &gcc_camss_csi2_ahb_clk.c, 0x00e4 },
		{ &gcc_camss_csi2phy_clk.c, 0x00e5 },
		{ &gcc_camss_csi2rdi_clk.c, 0x00e6 },
		{ &gcc_camss_csi2pix_clk.c, 0x00e7 },
		{ &gcc_cpp_tbu_clk.c, 0x00e9 },
		{ &gcc_boot_rom_ahb_clk.c, 0x00f8 },
		{ &gcc_usb_fs_ahb_clk.c, 0x00f1 },
		{ &gcc_usb_fs_ic_clk.c, 0x00f4 },
		{ &gcc_crypto_clk.c, 0x0138 },
		{ &gcc_crypto_axi_clk.c, 0x0139 },
		{ &gcc_crypto_ahb_clk.c, 0x013a },
		{ &gcc_bimc_gpu_clk.c, 0x0157 },
		{ &gcc_ipa_tbu_clk.c, 0x0198 },
		{ &gcc_vfe1_tbu_clk.c, 0x0199 },
		{ &gcc_camss_csi_vfe1_clk.c, 0x01a0 },
		{ &gcc_camss_vfe1_clk.c, 0x01a1 },
		{ &gcc_camss_vfe1_ahb_clk.c, 0x01a2 },
		{ &gcc_camss_vfe1_axi_clk.c, 0x01a3 },
		{ &gcc_venus0_core0_vcodec0_clk.c, 0x01b8 },
		{ &gcc_venus0_core1_vcodec0_clk.c, 0x01b9 },
		{ &gcc_camss_mclk2_clk.c, 0x01bd },
		{ &gcc_oxili_gfx3d_clk.c, 0x01ea },
		{ &gcc_oxili_ahb_clk.c, 0x01eb },
		{ &gcc_oxili_gmem_clk.c, 0x01f0 },
		{ &gcc_venus0_vcodec0_clk.c, 0x01f1 },
		{ &gcc_venus0_axi_clk.c, 0x01f2 },
		{ &gcc_venus0_ahb_clk.c, 0x01f3 },
		{ &gcc_mdss_ahb_clk.c, 0x01f6 },
		{ &gcc_mdss_axi_clk.c, 0x01f7 },
		{ &gcc_mdss_pclk0_clk.c, 0x01f8 },
		{ &gcc_mdss_mdp_clk.c, 0x01f9 },
		{ &gcc_mdss_vsync_clk.c, 0x01fb },
		{ &gcc_mdss_byte0_clk.c, 0x01fc },
		{ &gcc_mdss_esc0_clk.c, 0x01fd },
	),
	.c = {
		.dbg_name = "gcc_debug_mux",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux.c),
	},
};

/* Clock lookup */
static struct clk_lookup msm_clocks_lookup[] = {
	/* RPM clocks */
	CLK_LIST(xo_clk_src),
	CLK_LIST(xo_a_clk_src),
	CLK_LIST(xo_otg_clk),
	CLK_LIST(xo_lpm_clk),
	CLK_LIST(xo_pil_mss_clk),
	CLK_LIST(xo_pil_pronto_clk),
	CLK_LIST(xo_wlan_clk),
	CLK_LIST(xo_pil_lpass_clk),

	CLK_LIST(snoc_msmbus_clk),
	CLK_LIST(snoc_msmbus_a_clk),
	CLK_LIST(pnoc_msmbus_clk),
	CLK_LIST(pnoc_msmbus_a_clk),
	CLK_LIST(bimc_msmbus_clk),
	CLK_LIST(bimc_msmbus_a_clk),
	CLK_LIST(pnoc_keepalive_a_clk),

	CLK_LIST(pnoc_usb_a_clk),
	CLK_LIST(snoc_usb_a_clk),
	CLK_LIST(bimc_usb_a_clk),

	CLK_LIST(ipa_clk),
	CLK_LIST(ipa_a_clk),

	CLK_LIST(qdss_clk),
	CLK_LIST(qdss_a_clk),

	CLK_LIST(snoc_clk),
	CLK_LIST(pnoc_clk),
	CLK_LIST(bimc_clk),
	CLK_LIST(snoc_a_clk),
	CLK_LIST(pnoc_a_clk),
	CLK_LIST(bimc_a_clk),

	CLK_LIST(bb_clk1),
	CLK_LIST(bb_clk1_a),
	CLK_LIST(bb_clk2),
	CLK_LIST(bb_clk2_a),
	CLK_LIST(rf_clk1),
	CLK_LIST(rf_clk1_a),
	CLK_LIST(rf_clk2),
	CLK_LIST(rf_clk2_a),

	CLK_LIST(bb_clk1_pin),
	CLK_LIST(bb_clk1_a_pin),
	CLK_LIST(bb_clk2_pin),
	CLK_LIST(bb_clk2_a_pin),
	CLK_LIST(rf_clk1_pin),
	CLK_LIST(rf_clk1_a_pin),
	CLK_LIST(rf_clk2_pin),
	CLK_LIST(rf_clk2_a_pin),

	CLK_LIST(gpll0_clk_src),
	CLK_LIST(gpll0_ao_clk_src),
	CLK_LIST(gpll6_clk_src),
	CLK_LIST(gpll4_clk_src),
	CLK_LIST(gpll3_clk_src),
	CLK_LIST(a53ss_c0_pll),
	CLK_LIST(a53ss_c1_pll),
	CLK_LIST(a53ss_cci_pll),
	CLK_LIST(gcc_blsp1_ahb_clk),
	CLK_LIST(gcc_blsp2_ahb_clk),
	CLK_LIST(gcc_boot_rom_ahb_clk),
	CLK_LIST(gcc_crypto_ahb_clk),
	CLK_LIST(gcc_crypto_axi_clk),
	CLK_LIST(gcc_crypto_clk),
	CLK_LIST(gcc_prng_ahb_clk),
	CLK_LIST(gcc_cpp_tbu_clk),
	CLK_LIST(gcc_gfx_tbu_clk),
	CLK_LIST(gcc_gfx_tcu_clk),
	CLK_LIST(gcc_gtcu_ahb_clk),
	CLK_LIST(gcc_ipa_tbu_clk),
	CLK_LIST(gcc_jpeg_tbu_clk),
	CLK_LIST(gcc_mdp_tbu_clk),
	CLK_LIST(gcc_smmu_cfg_clk),
	CLK_LIST(gcc_venus_tbu_clk),
	CLK_LIST(gcc_vfe1_tbu_clk),
	CLK_LIST(gcc_vfe_tbu_clk),
	CLK_LIST(camss_top_ahb_clk_src),
	CLK_LIST(apss_ahb_clk_src),
	CLK_LIST(csi0_clk_src),
	CLK_LIST(csi1_clk_src),
	CLK_LIST(csi2_clk_src),
	CLK_LIST(vcodec0_clk_src),
	CLK_LIST(vfe0_clk_src),
	CLK_LIST(vfe1_clk_src),
	CLK_LIST(gfx3d_clk_src),
	CLK_LIST(blsp1_qup1_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup1_spi_apps_clk_src),
	CLK_LIST(blsp1_qup2_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup2_spi_apps_clk_src),
	CLK_LIST(blsp1_qup3_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup3_spi_apps_clk_src),
	CLK_LIST(blsp1_qup4_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup4_spi_apps_clk_src),
	CLK_LIST(blsp1_uart1_apps_clk_src),
	CLK_LIST(blsp1_uart2_apps_clk_src),
	CLK_LIST(blsp2_qup1_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup1_spi_apps_clk_src),
	CLK_LIST(blsp2_qup2_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup2_spi_apps_clk_src),
	CLK_LIST(blsp2_qup3_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup3_spi_apps_clk_src),
	CLK_LIST(blsp2_qup4_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup4_spi_apps_clk_src),
	CLK_LIST(blsp2_uart1_apps_clk_src),
	CLK_LIST(blsp2_uart2_apps_clk_src),
	CLK_LIST(cci_clk_src),
	CLK_LIST(cpp_clk_src),
	CLK_LIST(camss_gp0_clk_src),
	CLK_LIST(camss_gp1_clk_src),
	CLK_LIST(jpeg0_clk_src),
	CLK_LIST(mclk0_clk_src),
	CLK_LIST(mclk1_clk_src),
	CLK_LIST(mclk2_clk_src),
	CLK_LIST(csi0phytimer_clk_src),
	CLK_LIST(csi1phytimer_clk_src),
	CLK_LIST(crypto_clk_src),
	CLK_LIST(gp1_clk_src),
	CLK_LIST(gp2_clk_src),
	CLK_LIST(gp3_clk_src),
	CLK_LIST(esc0_clk_src),
	CLK_LIST(mdp_clk_src),
	CLK_LIST(vsync_clk_src),
	CLK_LIST(pdm2_clk_src),
	CLK_LIST(sdcc1_apps_clk_src),
	CLK_LIST(sdcc2_apps_clk_src),
	CLK_LIST(usb_fs_ic_clk_src),
	CLK_LIST(usb_fs_system_clk_src),
	CLK_LIST(usb_hs_system_clk_src),
	CLK_LIST(gcc_bimc_gpu_clk),
	CLK_LIST(gcc_blsp1_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup1_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_spi_apps_clk),
	CLK_LIST(gcc_blsp1_uart1_apps_clk),
	CLK_LIST(gcc_blsp1_uart2_apps_clk),
	CLK_LIST(gcc_blsp2_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup1_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup2_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup3_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup3_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_spi_apps_clk),
	CLK_LIST(gcc_blsp2_uart1_apps_clk),
	CLK_LIST(gcc_blsp2_uart2_apps_clk),
	CLK_LIST(gcc_camss_cci_ahb_clk),
	CLK_LIST(gcc_camss_cci_clk),
	CLK_LIST(gcc_camss_cpp_ahb_clk),
	CLK_LIST(gcc_camss_cpp_axi_clk),
	CLK_LIST(gcc_camss_cpp_clk),
	CLK_LIST(gcc_camss_csi0_ahb_clk),
	CLK_LIST(gcc_camss_csi0_clk),
	CLK_LIST(gcc_camss_csi0phy_clk),
	CLK_LIST(gcc_camss_csi0pix_clk),
	CLK_LIST(gcc_camss_csi0rdi_clk),
	CLK_LIST(gcc_camss_csi1_ahb_clk),
	CLK_LIST(gcc_camss_csi1_clk),
	CLK_LIST(gcc_camss_csi1phy_clk),
	CLK_LIST(gcc_camss_csi1pix_clk),
	CLK_LIST(gcc_camss_csi1rdi_clk),
	CLK_LIST(gcc_camss_csi2_ahb_clk),
	CLK_LIST(gcc_camss_csi2_clk),
	CLK_LIST(gcc_camss_csi2phy_clk),
	CLK_LIST(gcc_camss_csi2pix_clk),
	CLK_LIST(gcc_camss_csi2rdi_clk),
	CLK_LIST(gcc_camss_csi_vfe0_clk),
	CLK_LIST(gcc_camss_csi_vfe1_clk),
	CLK_LIST(gcc_camss_gp0_clk),
	CLK_LIST(gcc_camss_gp1_clk),
	CLK_LIST(gcc_camss_ispif_ahb_clk),
	CLK_LIST(gcc_camss_jpeg0_clk),
	CLK_LIST(gcc_camss_jpeg_ahb_clk),
	CLK_LIST(gcc_camss_jpeg_axi_clk),
	CLK_LIST(gcc_camss_mclk0_clk),
	CLK_LIST(gcc_camss_mclk1_clk),
	CLK_LIST(gcc_camss_mclk2_clk),
	CLK_LIST(gcc_camss_micro_ahb_clk),
	CLK_LIST(gcc_camss_csi0phytimer_clk),
	CLK_LIST(gcc_camss_csi1phytimer_clk),
	CLK_LIST(gcc_camss_ahb_clk),
	CLK_LIST(gcc_camss_top_ahb_clk),
	CLK_LIST(gcc_camss_vfe0_clk),
	CLK_LIST(gcc_camss_vfe_ahb_clk),
	CLK_LIST(gcc_camss_vfe_axi_clk),
	CLK_LIST(gcc_camss_vfe1_ahb_clk),
	CLK_LIST(gcc_camss_vfe1_axi_clk),
	CLK_LIST(gcc_camss_vfe1_clk),
	CLK_LIST(gcc_oxili_gmem_clk),
	CLK_LIST(gcc_gp1_clk),
	CLK_LIST(gcc_gp2_clk),
	CLK_LIST(gcc_gp3_clk),
	CLK_LIST(gcc_mdss_ahb_clk),
	CLK_LIST(gcc_mdss_axi_clk),
	CLK_LIST(gcc_mdss_esc0_clk),
	CLK_LIST(gcc_mdss_mdp_clk),
	CLK_LIST(gcc_mdss_vsync_clk),
	CLK_LIST(gcc_mss_cfg_ahb_clk),
	CLK_LIST(gcc_mss_q6_bimc_axi_clk),
	CLK_LIST(gcc_bimc_gfx_clk),
	CLK_LIST(gcc_oxili_ahb_clk),
	CLK_LIST(gcc_oxili_gfx3d_clk),
	CLK_LIST(gcc_pdm2_clk),
	CLK_LIST(gcc_pdm_ahb_clk),
	CLK_LIST(gcc_sdcc1_ahb_clk),
	CLK_LIST(gcc_sdcc1_apps_clk),
	CLK_LIST(gcc_sdcc2_ahb_clk),
	CLK_LIST(gcc_sdcc2_apps_clk),
	CLK_LIST(gcc_sys_mm_noc_axi_clk),
	CLK_LIST(gcc_usb2a_phy_sleep_clk),
	CLK_LIST(gcc_usb_hs_phy_cfg_ahb_clk),
	CLK_LIST(gcc_usb_fs_ahb_clk),
	CLK_LIST(gcc_usb_fs_ic_clk),
	CLK_LIST(gcc_usb_fs_system_clk),
	CLK_LIST(gcc_usb_hs_ahb_clk),
	CLK_LIST(gcc_usb_hs_system_clk),
	CLK_LIST(gcc_venus0_ahb_clk),
	CLK_LIST(gcc_venus0_axi_clk),
	CLK_LIST(gcc_venus0_core0_vcodec0_clk),
	CLK_LIST(gcc_venus0_core1_vcodec0_clk),
	CLK_LIST(gcc_venus0_vcodec0_clk),

	/* Reset clks */
	CLK_LIST(gcc_usb2_hs_phy_only_clk),
	CLK_LIST(gcc_qusb2_phy_clk),
};

/* Please note that the order of reg-names is important */
static int get_mmio_addr(struct platform_device *pdev)
{
	int i, count;
	const char *str;
	struct resource *res;
	struct device *dev = &pdev->dev;

	count = of_property_count_strings(dev->of_node, "reg-names");
	if (count != N_BASES) {
		dev_err(dev, "missing reg-names property, expected %d strings\n",
				N_BASES);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		of_property_read_string_index(dev->of_node, "reg-names", i,
						&str);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, str);
		if (!res) {
			dev_err(dev, "Unable to retrieve register base.\n");
			return -ENOMEM;
		}

		virt_bases[i] = devm_ioremap(dev, res->start,
							resource_size(res));
		if (!virt_bases[i]) {
			dev_err(dev, "Failed to map in CC registers.\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static int msm_gcc_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;
	u32 regval;

	ret = enable_rpm_scaling();
	if (ret < 0)
		return ret;

	ret = get_mmio_addr(pdev);
	if (ret)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Register base not defined\n");
		return -ENOMEM;
	}

	virt_bases[GCC_BASE] = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!virt_bases[GCC_BASE]) {
		dev_err(&pdev->dev, "Failed to ioremap CC registers\n");
		return -ENOMEM;
	}

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_dig regulator!!!\n");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	vdd_sr2_pll.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd_sr2_pll");
	if (IS_ERR(vdd_sr2_pll.regulator[0])) {
		if (PTR_ERR(vdd_sr2_pll.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_sr2_pll regulator!!!\n");
		return PTR_ERR(vdd_sr2_pll.regulator[0]);
	}

	vdd_sr2_pll.regulator[1] = devm_regulator_get(&pdev->dev,
							"vdd_sr2_dig");
	if (IS_ERR(vdd_sr2_pll.regulator[1])) {
		if (PTR_ERR(vdd_sr2_pll.regulator[1]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_sr2_dig regulator!!!\n");
		return PTR_ERR(vdd_sr2_pll.regulator[1]);
	}

	vdd_hf_pll.regulator[0] = devm_regulator_get(&pdev->dev,
							"vdd_hf_pll");
	if (IS_ERR(vdd_hf_pll.regulator[0])) {
		if (PTR_ERR(vdd_hf_pll.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_sr2_pll regulator!!!\n");
		return PTR_ERR(vdd_hf_pll.regulator[0]);
	}

	vdd_hf_pll.regulator[1] = devm_regulator_get(&pdev->dev,
							"vdd_hf_dig");
	if (IS_ERR(vdd_hf_pll.regulator[1])) {
		if (PTR_ERR(vdd_hf_pll.regulator[1]) != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Unable to get vdd_hf_dig regulator!!!\n");
		return PTR_ERR(vdd_hf_pll.regulator[1]);
	}

	/*Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));

	ret = of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_lookup,
				ARRAY_SIZE(msm_clocks_lookup));
	if (ret)
		return ret;

	clk_set_rate(&apss_ahb_clk_src.c, 19200000);
	clk_prepare_enable(&apss_ahb_clk_src.c);

	/*
	 *  Hold an active set vote for PCNOC AHB source. Sleep set vote is 0.
	 */
	clk_set_rate(&pnoc_keepalive_a_clk.c, 19200000);
	clk_prepare_enable(&pnoc_keepalive_a_clk.c);

	clk_prepare_enable(&xo_a_clk_src.c);

	dev_info(&pdev->dev, "Registered GCC clocks\n");

	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-tellurium" },
	{},
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_probe,
	.driver = {
		.name = "qcom,gcc-tellurium",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_gcc_init(void)
{
	return platform_driver_register(&msm_clock_gcc_driver);
}

static struct clk_lookup msm_clocks_measure[] = {
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
};

static int msm_clock_debug_probe(struct platform_device *pdev)
{
	int ret;

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

	ret =  of_msm_clock_register(pdev->dev.of_node, msm_clocks_measure,
					ARRAY_SIZE(msm_clocks_measure));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register debug Mux\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Debug Mux successfully\n");
	return ret;
}

static struct of_device_id msm_clock_debug_match_table[] = {
	{ .compatible = "qcom,cc-debug-tellurium" },
	{}
};

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_probe,
	.driver = {
		.name = "qcom,cc-debug-tellurium",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_clock_debug_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}

/* MDSS DSI_PHY_PLL */
static struct clk_lookup msm_clocks_gcc_mdss[] = {
	CLK_LIST(byte0_clk_src),
	CLK_LIST(pclk0_clk_src),
	CLK_LIST(gcc_mdss_pclk0_clk),
	CLK_LIST(gcc_mdss_byte0_clk),
};

static int msm_gcc_mdss_probe(struct platform_device *pdev)
{
	int counter, ret;

	pclk0_clk_src.c.parent = devm_clk_get(&pdev->dev, "pixel_src");
	if (IS_ERR(pclk0_clk_src.c.parent)) {
		dev_err(&pdev->dev, "Failed to get pixel source.\n");
		return PTR_ERR(pclk0_clk_src.c.parent);
	}

	for (counter = 0; counter < (sizeof(ftbl_gcc_mdss_pclk0_clk)/
				sizeof(struct clk_freq_tbl)); counter++)
		ftbl_gcc_mdss_pclk0_clk[counter].src_clk =
					pclk0_clk_src.c.parent;

	byte0_clk_src.c.parent = devm_clk_get(&pdev->dev, "byte_src");
	if (IS_ERR(byte0_clk_src.c.parent)) {
		dev_err(&pdev->dev, "Failed to get byte0 source.\n");
		devm_clk_put(&pdev->dev, pclk0_clk_src.c.parent);
		return PTR_ERR(byte0_clk_src.c.parent);
	}

	for (counter = 0; counter < (sizeof(ftbl_gcc_mdss_byte0_clk)/
				sizeof(struct clk_freq_tbl)); counter++)
		ftbl_gcc_mdss_byte0_clk[counter].src_clk =
					byte0_clk_src.c.parent;

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_gcc_mdss,
					ARRAY_SIZE(msm_clocks_gcc_mdss));
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered GCC MDSS clocks.\n");

	return ret;
}

static struct of_device_id msm_clock_mdss_match_table[] = {
	{ .compatible = "qcom,gcc-mdss-tellurium" },
	{}
};

static struct platform_driver msm_clock_gcc_mdss_driver = {
	.probe = msm_gcc_mdss_probe,
	.driver = {
		.name = "gcc-mdss-tellurium",
		.of_match_table = msm_clock_mdss_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_gcc_mdss_init(void)
{
	return platform_driver_register(&msm_clock_gcc_mdss_driver);
}
arch_initcall(msm_gcc_init);
fs_initcall_sync(msm_gcc_mdss_init);
late_initcall(msm_clock_debug_init);

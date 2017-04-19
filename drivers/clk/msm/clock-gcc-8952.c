/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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
#include <linux/of_platform.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-alpha-pll.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/clock-rpm.h>

#include <linux/clk/msm-clock-generic.h>
#include <linux/regulator/rpm-smd-regulator.h>

#include <dt-bindings/clock/msm-clocks-8952.h>
#include <dt-bindings/clock/msm-clocks-hwio-8952.h>

#include "clock.h"

enum {
	GCC_BASE,
	APCS_C1_PLL_BASE,
	APCS_C0_PLL_BASE,
	APCS_CCI_PLL_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];
#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

/* SMD clocks */
DEFINE_CLK_RPM_SMD_BRANCH(xo_clk_src, xo_a_clk_src, RPM_MISC_CLK_TYPE,
				CXO_CLK_SRC_ID, 19200000);
DEFINE_CLK_RPM_SMD(pnoc_clk, pnoc_a_clk, RPM_BUS_CLK_TYPE, PNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(bimc_gpu_clk, bimc_gpu_a_clk, RPM_MEM_CLK_TYPE,
						BIMC_GPU_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(sysmmnoc_clk, sysmmnoc_a_clk, RPM_BUS_CLK_TYPE,
						SYSMMNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(ipa_clk, ipa_a_clk, RPM_IPA_CLK_TYPE, IPA_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_CLK_ID);

/* SMD_XO_BUFFER */
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk1, bb_clk1_a, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk2, bb_clk2_a, BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk2, rf_clk2_a, RF_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk2, div_clk2_a, DIV_CLK2_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk1_pin, bb_clk1_a_pin, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk2_pin, bb_clk2_a_pin, BB_CLK2_ID);

/* Voter clocks */
static DEFINE_CLK_VOTER(pnoc_msmbus_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(sysmmnoc_msmbus_clk,  &sysmmnoc_clk.c,  LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_msmbus_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(sysmmnoc_msmbus_a_clk,  &sysmmnoc_a_clk.c,  LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_keepalive_a_clk, &pnoc_a_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_usb_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_usb_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_usb_a_clk, &bimc_a_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_usb_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_usb_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_usb_clk, &bimc_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(snoc_wcnss_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_wcnss_a_clk, &bimc_a_clk.c, LONG_MAX);

/* Branch Voter clocks */
static DEFINE_CLK_BRANCH_VOTER(xo_gcc, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_otg_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_lpm_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_pronto_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_mss_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_wlan_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(xo_pil_lpass_clk, &xo_clk_src.c);

DEFINE_CLK_DUMMY(wcnss_m_clk, 0);

enum vdd_sr2_pll_levels {
	VDD_SR2_PLL_OFF,
	VDD_SR2_PLL_SVS,
	VDD_SR2_PLL_NOM,
	VDD_SR2_PLL_TUR,
	VDD_SR2_PLL_SUPER_TUR,
	VDD_SR2_PLL_NUM,
};

static int vdd_sr2_levels[] = {
	0,	 RPM_REGULATOR_LEVEL_NONE,	/* VDD_SR2_PLL_OFF */
	1800000, RPM_REGULATOR_LEVEL_SVS,	/* VDD_SR2_PLL_SVS */
	1800000, RPM_REGULATOR_LEVEL_NOM,	/* VDD_SR2_PLL_NOM */
	1800000, RPM_REGULATOR_LEVEL_TURBO,	/* VDD_SR2_PLL_TUR */
	1800000, RPM_REGULATOR_LEVEL_BINNING,	/* VDD_SR2_PLL_SUPER_TUR */
};

static DEFINE_VDD_REGULATORS(vdd_sr2_pll, VDD_SR2_PLL_NUM, 2,
				vdd_sr2_levels, NULL);

enum vdd_hf_pll_levels {
	VDD_HF_PLL_OFF,
	VDD_HF_PLL_SVS,
	VDD_HF_PLL_NOM,
	VDD_HF_PLL_TUR,
	VDD_HF_PLL_SUPER_TUR,
	VDD_HF_PLL_NUM,
};

static int vdd_hf_levels[] = {
	0,	 RPM_REGULATOR_LEVEL_NONE,	/* VDD_HF_PLL_OFF */
	1800000, RPM_REGULATOR_LEVEL_SVS,	/* VDD_HF_PLL_SVS */
	1800000, RPM_REGULATOR_LEVEL_NOM,	/* VDD_HF_PLL_NOM */
	1800000, RPM_REGULATOR_LEVEL_TURBO,	/* VDD_HF_PLL_TUR */
	1800000, RPM_REGULATOR_LEVEL_BINNING,	/* VDD_HF_PLL_SUPER_TUR */
};
static DEFINE_VDD_REGULATORS(vdd_hf_pll, VDD_HF_PLL_NUM, 2,
				vdd_hf_levels, NULL);

static struct pll_freq_tbl apcs_cci_pll_freq[] = {
	F_APCS_PLL(307200000, 16, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(600000000, 31, 0x1, 0x4, 0x0, 0x0, 0x0),
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
	.spm_ctrl = {
		.offset = 0x40,
		.event_bit = 0x0,
	},
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
	F_APCS_PLL( 249600000,  13, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 307200000,  16, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 345600000,  18, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 384000000,  20, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 460800000,  24, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 499200000,  26, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 518400000,  27, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 768000000,  40, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 806400000,  42, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 844800000,  44, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 883200000,  46, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 902400000,  47, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 921600000,  48, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 998400000,  52, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1094400000,  57, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1209600000,  63, 0x0, 0x1, 0x0, 0x0, 0x0),
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
	.spm_ctrl = {
		.offset = 0x50,
		.event_bit = 0x4,
	},
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
	F_APCS_PLL( 345600000, 18, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 422400000, 22, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 499200000, 26, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 652800000, 34, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 729600000, 38, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 806400000, 42, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 844800000, 44, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 883200000, 46, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 960000000, 50, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 998400000, 52, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1036800000, 54, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1094400000, 57, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1113600000, 58, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1190400000, 62, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1209600000, 63, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1248000000, 65, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1267200000, 66, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1344000000, 70, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1401000000, 73, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1420800000, 74, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1440000000, 75, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1459200000, 76, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1497600000, 78, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1516800000, 79, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1536000000, 80, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1651200000, 86, 0x0, 0x1, 0x0, 0x0, 0x0),
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
	.spm_ctrl = {
		.offset = 0x50,
		.event_bit = 0x4,
	},
	.c = {
		.parent = &xo_a_clk_src.c,
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

static struct pll_vote_clk gpll0_sleep_clk_src = {
	.en_reg = (void __iomem *)APCS_CLOCK_SLEEP_ENA_VOTE,
	.en_mask = BIT(23),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 800000000,
		.dbg_name = "gpll0_sleep_clk_src",
		.ops = &clk_ops_pll_sleep_vote,
		CLK_INIT(gpll0_sleep_clk_src.c),
	},
};

static unsigned int soft_vote_gpll0;

/* PLL_ACTIVE_FLAG bit of GCC_GPLL0_MODE register
 * gets set from PLL voting FSM.It indicates when
 * FSM has enabled the PLL and PLL should be locked.
 */
static struct pll_vote_clk gpll0_clk_src_8952 = {
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
		.dbg_name = "gpll0_clk_src_8952",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_clk_src_8952.c),
	},
};

static struct pll_vote_clk gpll0_clk_src_8937 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gpll0_sleep_clk_src.c,
		.rate = 800000000,
		.dbg_name = "gpll0_clk_src_8937",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_clk_src_8937.c),
	},
};

DEFINE_EXT_CLK(gpll0_clk_src, NULL);
DEFINE_EXT_CLK(gpll0_ao_clk_src, NULL);
DEFINE_EXT_CLK(gpll0_out_aux_clk_src, &gpll0_clk_src.c);
DEFINE_EXT_CLK(gpll0_out_main_clk_src, &gpll0_clk_src.c);
DEFINE_EXT_CLK(ext_pclk0_clk_src, NULL);
DEFINE_EXT_CLK(ext_byte0_clk_src, NULL);
DEFINE_EXT_CLK(ext_pclk1_clk_src, NULL);
DEFINE_EXT_CLK(ext_byte1_clk_src, NULL);

/* Don't vote for xo if using this clock to allow xo shutdown */
static struct pll_vote_clk gpll0_ao_clk_src_8952 = {
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
		.dbg_name = "gpll0_ao_clk_src_8952",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao_clk_src_8952.c),
	},
};

static struct pll_vote_clk gpll0_ao_clk_src_8937 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_a_clk_src.c,
		.rate = 800000000,
		.dbg_name = "gpll0_ao_clk_src_8937",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao_clk_src_8937.c),
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

DEFINE_EXT_CLK(gpll6_aux_clk_src, &gpll6_clk_src.c);
DEFINE_EXT_CLK(gpll6_out_main_clk_src, &gpll6_clk_src.c);

static struct alpha_pll_masks pll_masks_p = {
	.lock_mask = BIT(31),
	.active_mask = BIT(30),
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
	.update_mask = BIT(22),
	.post_div_mask = BM(11, 8),
	.test_ctl_lo_mask = BM(31, 0),
	.test_ctl_hi_mask = BM(31, 0),
};

/* Slewing plls won't allow to change vco_sel.
 * Hence will have only one vco table entry */
static struct alpha_pll_vco_tbl p_vco[] = {
	VCO(0,  700000000, 1400000000),
};

/* Slewing plls won't allow to change vco_sel.
 * Hence will have only one vco table entry */
static struct alpha_pll_vco_tbl p_vco_8937[] = {
	VCO(1,  525000000, 1066000000),
};

static struct alpha_pll_clk gpll3_clk_src = {
	.masks = &pll_masks_p,
	.base = &virt_bases[GCC_BASE],
	.offset = GPLL3_MODE,
	.vco_tbl = p_vco,
	.num_vco = ARRAY_SIZE(p_vco),
	.enable_config = 1,
	/*
	 * gpll3 is dedicated to oxili and has a fuse implementation for
	 * post divider to limit frequency. HW with fuse blown has a divider
	 * value set to 2. So lets stick to divide by 2 in software to avoid
	 * conflicts.
	 */
	.post_div_config = 1 << 8,
	.slew = true,
	.config_ctl_val = 0x4001055b,
	.test_ctl_hi_val = 0x40000600,
	.c = {
		.rate = 1050000000,
		.parent = &xo_clk_src.c,
		.dbg_name = "gpll3_clk_src",
		.ops = &clk_ops_dyna_alpha_pll,
		VDD_DIG_FMAX_MAP1(NOMINAL, 1400000000),
		CLK_INIT(gpll3_clk_src.c),
	},
};

static struct pll_vote_clk gpll4_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(5),
	.status_reg = (void __iomem *)GPLL4_MODE,
	.status_mask = BIT(30),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.rate =  1152000000,
		.parent = &xo_clk_src.c,
		.dbg_name = "gpll4_clk_src",
		.ops = &clk_ops_pll_vote,
		VDD_DIG_FMAX_MAP1(NOMINAL, 1400000000),
		CLK_INIT(gpll4_clk_src.c),
	},
};
DEFINE_EXT_CLK(gpll4_out_clk_src, &gpll4_clk_src.c);

static struct clk_freq_tbl ftbl_gcc_camss_top_ahb_clk[] = {
	F( 40000000,	gpll0,	10,	1,	2),
	F( 61540000,	gpll0,	13,	0,	0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 40000000, LOW, 61540000,
				  NOMINAL, 80000000),
		CLK_INIT(camss_top_ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_apss_ahb_clk[] = {
	F( 19200000,	xo_a,	1,	0,	0),
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
	F( 160000000,	gpll0,	5,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_camss_csi0_2_clk_8937[] = {
	F( 100000000,          gpll0,    8,    0,     0),
	F( 160000000,          gpll0,    5,    0,     0),
	F( 200000000,          gpll0,    4,    0,     0),
	F( 266670000,          gpll0,    3,    0,     0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 160000000,
				  NOMINAL, 200000000),
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
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 160000000,
				  NOMINAL, 200000000),
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
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 160000000,
				  NOMINAL, 200000000),
		CLK_INIT(csi2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_venus0_vcodec0_clk[] = {
	F( 133330000,	gpll0,	6,	0,	0),
	F( 180000000,	gpll6,	6,	0,	0),
	F( 228570000,	gpll0,	3.5,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 308570000,	gpll6,	3.5,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_venus0_vcodec0_clk_8937[] = {
	F( 166150000,          gpll6,  6.5,    0,     0),
	F( 240000000,          gpll6,  4.5,    0,     0),
	F( 308570000,          gpll6,  3.5,    0,     0),
	F( 320000000,          gpll0,  2.5,    0,     0),
	F( 360000000,          gpll6,    3,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_venus0_vcodec0_clk_8917[] = {
	F( 160000000,          gpll0,    5,    0,     0),
	F( 200000000,          gpll0,    4,    0,     0),
	F( 270000000,          gpll6,    4,    0,     0),
	F( 308570000,          gpll6,  3.5,    0,     0),
	F( 329140000,          gpll4_out,  3.5,    0,     0),
	F( 360000000,          gpll6,    3,    0,     0),
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
		VDD_DIG_FMAX_MAP5(LOWER, 133330000, LOW, 180000000,
				  NOMINAL, 228570000, NOM_PLUS, 266670000,
				  HIGH, 308570000),
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
	F( 308570000,	gpll6,	3.5,	0,	0),
	F( 320000000,	gpll0,	2.5,	0,	0),
	F( 360000000,	gpll6,	3,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_camss_vfe0_1_clk_8937[] = {
	F(  50000000,          gpll0,   16,    0,     0),
	F(  80000000,          gpll0,   10,    0,     0),
	F( 100000000,          gpll0,    8,    0,     0),
	F( 133333333,          gpll0,    6,    0,     0),
	F( 160000000,          gpll0,    5,    0,     0),
	F( 177780000,          gpll0,  4.5,    0,     0),
	F( 200000000,          gpll0,    4,    0,     0),
	F( 266670000,          gpll0,    3,    0,     0),
	F( 308570000,          gpll6,  3.5,    0,     0),
	F( 320000000,          gpll0,  2.5,    0,     0),
	F( 360000000,          gpll6,    3,    0,     0),
	F( 400000000,          gpll0,    2,    0,     0),
	F( 432000000,          gpll6,  2.5,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_camss_vfe0_1_clk_8917[] = {
	F(  50000000,          gpll0,   16,    0,     0),
	F(  80000000,          gpll0,   10,    0,     0),
	F( 100000000,          gpll0,    8,    0,     0),
	F( 133333333,          gpll0,    6,    0,     0),
	F( 160000000,          gpll0,    5,    0,     0),
	F( 177780000,          gpll0,  4.5,    0,     0),
	F( 200000000,          gpll0,    4,    0,     0),
	F( 266670000,          gpll0,    3,    0,     0),
	F( 308570000,          gpll6,  3.5,    0,     0),
	F( 320000000,          gpll0,  2.5,    0,     0),
	F( 329140000,          gpll4_out,  3.5,    0,     0),
	F( 360000000,          gpll6,    3,    0,     0),
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
		VDD_DIG_FMAX_MAP5(LOWER, 133330000, LOW, 266670000,
				  NOMINAL, 308570000, NOM_PLUS, 320000000,
				  HIGH, 360000000),
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
		VDD_DIG_FMAX_MAP5(LOWER, 133330000, LOW, 266670000,
				  NOMINAL, 308570000, NOM_PLUS, 320000000,
				  HIGH, 360000000),
		CLK_INIT(vfe1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_oxili_gfx3d_clk[] = {
	F_SLEW( 19200000,  FIXED_CLK_SRC, xo,		1,	0,	0),
	F_SLEW( 50000000,  FIXED_CLK_SRC, gpll0,	16,	0,	0),
	F_SLEW( 80000000,  FIXED_CLK_SRC, gpll0,	10,	0,	0),
	F_SLEW( 100000000, FIXED_CLK_SRC, gpll0,	8,	0,	0),
	F_SLEW( 160000000, FIXED_CLK_SRC, gpll0,	5,	0,	0),
	F_SLEW( 200000000, FIXED_CLK_SRC, gpll0,	4,	0,	0),
	F_SLEW( 228570000, FIXED_CLK_SRC, gpll0,	3.5,	0,	0),
	F_SLEW( 240000000, FIXED_CLK_SRC, gpll6_aux,	4.5,	0,	0),
	F_SLEW( 266670000, FIXED_CLK_SRC, gpll0,	3,	0,	0),
	F_SLEW( 400000000, FIXED_CLK_SRC, gpll0,	2,	0,	0),
	F_SLEW( 465000000, 930000000,	  gpll3,	1,	0,	0),
	F_SLEW( 500000000, 1000000000,	  gpll3,	1,	0,	0),
	F_SLEW( 550000000, 1100000000,	  gpll3,	1,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_oxili_gfx3d_clk_8937[] = {
	F_SLEW( 19200000,  FIXED_CLK_SRC, xo,		1,	0,	0),
	F_SLEW( 50000000,  FIXED_CLK_SRC, gpll0,	16,	0,	0),
	F_SLEW( 80000000,  FIXED_CLK_SRC, gpll0,	10,	0,	0),
	F_SLEW( 100000000, FIXED_CLK_SRC, gpll0,	8,	0,	0),
	F_SLEW( 160000000, FIXED_CLK_SRC, gpll0,	5,	0,	0),
	F_SLEW( 200000000, FIXED_CLK_SRC, gpll0,	4,	0,	0),
	F_SLEW( 216000000, FIXED_CLK_SRC, gpll6_aux,	5,	0,	0),
	F_SLEW( 228570000, FIXED_CLK_SRC, gpll0,	3.5,	0,	0),
	F_SLEW( 240000000, FIXED_CLK_SRC, gpll6_aux,	4.5,	0,	0),
	F_SLEW( 266670000, FIXED_CLK_SRC, gpll0,	3,	0,	0),
	F_SLEW( 300000000, 600000000,	  gpll3,	1,	0,      0),
	F_SLEW( 320000000, FIXED_CLK_SRC, gpll0,	2.5,	0,	0),
	F_SLEW( 375000000, 750000000,	  gpll3,	1,	0,	0),
	F_SLEW( 400000000, FIXED_CLK_SRC, gpll0,	2,	0,	0),
	F_SLEW( 450000000, 900000000,	  gpll3,	1,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_oxili_gfx3d_clk_8937_475MHz[] = {
	F_SLEW( 19200000,  FIXED_CLK_SRC, xo,		1,	0,	0),
	F_SLEW( 50000000,  FIXED_CLK_SRC, gpll0,	16,	0,	0),
	F_SLEW( 80000000,  FIXED_CLK_SRC, gpll0,	10,	0,	0),
	F_SLEW( 100000000, FIXED_CLK_SRC, gpll0,	8,	0,	0),
	F_SLEW( 160000000, FIXED_CLK_SRC, gpll0,	5,	0,	0),
	F_SLEW( 200000000, FIXED_CLK_SRC, gpll0,	4,	0,	0),
	F_SLEW( 216000000, FIXED_CLK_SRC, gpll6_aux,	5,	0,	0),
	F_SLEW( 228570000, FIXED_CLK_SRC, gpll0,	3.5,	0,	0),
	F_SLEW( 240000000, FIXED_CLK_SRC, gpll6_aux,	4.5,	0,	0),
	F_SLEW( 266670000, FIXED_CLK_SRC, gpll0,	3,	0,	0),
	F_SLEW( 300000000, 600000000,	  gpll3,	1,	0,	0),
	F_SLEW( 320000000, FIXED_CLK_SRC, gpll0,	2.5,	0,	0),
	F_SLEW( 375000000, 750000000,	  gpll3,	1,	0,	0),
	F_SLEW( 400000000, FIXED_CLK_SRC, gpll0,	2,	0,	0),
	F_SLEW( 450000000, 900000000,	  gpll3,	1,	0,	0),
	F_SLEW( 475000000, 950000000,	  gpll3,	1,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_oxili_gfx3d_clk_8940_500MHz[] = {
	F_SLEW( 19200000,  FIXED_CLK_SRC, xo,		1,	0,	0),
	F_SLEW( 50000000,  FIXED_CLK_SRC, gpll0,	16,	0,	0),
	F_SLEW( 80000000,  FIXED_CLK_SRC, gpll0,	10,	0,	0),
	F_SLEW( 100000000, FIXED_CLK_SRC, gpll0,	8,	0,	0),
	F_SLEW( 160000000, FIXED_CLK_SRC, gpll0,	5,	0,	0),
	F_SLEW( 200000000, FIXED_CLK_SRC, gpll0,	4,	0,	0),
	F_SLEW( 216000000, FIXED_CLK_SRC, gpll6_aux,	5,	0,	0),
	F_SLEW( 228570000, FIXED_CLK_SRC, gpll0,	3.5,	0,	0),
	F_SLEW( 240000000, FIXED_CLK_SRC, gpll6_aux,	4.5,	0,	0),
	F_SLEW( 266670000, FIXED_CLK_SRC, gpll0,	3,	0,	0),
	F_SLEW( 300000000, 600000000,	  gpll3,	1,	0,	0),
	F_SLEW( 320000000, FIXED_CLK_SRC, gpll0,	2.5,	0,	0),
	F_SLEW( 375000000, 750000000,	  gpll3,	1,	0,	0),
	F_SLEW( 400000000, FIXED_CLK_SRC, gpll0,	2,	0,	0),
	F_SLEW( 450000000, 900000000,	  gpll3,	1,	0,	0),
	F_SLEW( 475000000, 950000000,	  gpll3,	1,	0,	0),
	F_SLEW( 500000000, 1000000000,	  gpll3,	1,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_oxili_gfx3d_clk_8917[] = {
	F_SLEW( 19200000,  FIXED_CLK_SRC, xo,		1,	0,	0),
	F_SLEW( 50000000,  FIXED_CLK_SRC, gpll0,	16,	0,	0),
	F_SLEW( 80000000,  FIXED_CLK_SRC, gpll0,	10,	0,	0),
	F_SLEW( 100000000, FIXED_CLK_SRC, gpll0,	8,	0,	0),
	F_SLEW( 160000000, FIXED_CLK_SRC, gpll0,	5,	0,	0),
	F_SLEW( 200000000, FIXED_CLK_SRC, gpll0,	4,	0,	0),
	F_SLEW( 228570000, FIXED_CLK_SRC, gpll0,	3.5,	0,	0),
	F_SLEW( 240000000, FIXED_CLK_SRC, gpll6_aux,	4.5,	0,	0),
	F_SLEW( 266670000, FIXED_CLK_SRC, gpll0,	3,	0,	0),
	F_SLEW( 270000000, FIXED_CLK_SRC, gpll6_aux,	4,	0,	0),
	F_SLEW( 320000000, FIXED_CLK_SRC, gpll0,	2.5,	0,	0),
	F_SLEW( 400000000, FIXED_CLK_SRC, gpll0,	2,	0,	0),
	F_SLEW( 484800000, 969600000,	  gpll3,	1,	0,	0),
	F_SLEW( 523200000, 1046400000,	  gpll3,	1,	0,	0),
	F_SLEW( 550000000, 1100000000,	  gpll3,	1,	0,	0),
	F_SLEW( 598000000, 1196000000,	  gpll3,	1,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_oxili_gfx3d_clk_8917_650MHz[] = {
	F_SLEW( 19200000,  FIXED_CLK_SRC, xo,		1,	0,	0),
	F_SLEW( 50000000,  FIXED_CLK_SRC, gpll0,	16,	0,	0),
	F_SLEW( 80000000,  FIXED_CLK_SRC, gpll0,	10,	0,	0),
	F_SLEW( 100000000, FIXED_CLK_SRC, gpll0,	8,	0,	0),
	F_SLEW( 160000000, FIXED_CLK_SRC, gpll0,	5,	0,	0),
	F_SLEW( 200000000, FIXED_CLK_SRC, gpll0,	4,	0,	0),
	F_SLEW( 228570000, FIXED_CLK_SRC, gpll0,	3.5,	0,	0),
	F_SLEW( 240000000, FIXED_CLK_SRC, gpll6_aux,	4.5,	0,	0),
	F_SLEW( 266670000, FIXED_CLK_SRC, gpll0,	3,	0,	0),
	F_SLEW( 270000000, FIXED_CLK_SRC, gpll6_aux,	4,	0,	0),
	F_SLEW( 320000000, FIXED_CLK_SRC, gpll0,	2.5,	0,	0),
	F_SLEW( 400000000, FIXED_CLK_SRC, gpll0,	2,	0,	0),
	F_SLEW( 484800000, 969600000,	  gpll3,	1,	0,	0),
	F_SLEW( 523200000, 1046400000,	  gpll3,	1,	0,	0),
	F_SLEW( 550000000, 1100000000,	  gpll3,	1,	0,	0),
	F_SLEW( 598000000, 1196000000,	  gpll3,	1,	0,	0),
	F_SLEW( 650000000, 1300000000,	  gpll3,	1,	0,	0),
	F_END
};

static struct  rcg_clk gfx3d_clk_src = {
	.cmd_rcgr_reg = GFX3D_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_oxili_gfx3d_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gfx3d_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP5(LOWER, 240000000, LOW, 400000000,
				  NOMINAL, 465000000, NOM_PLUS, 500000000,
				  HIGH, 550000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
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

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk_8917[] = {
	F( 960000,	xo,	10,	1,	2),
	F( 4800000,	xo,	4,	0,	0),
	F( 9600000,	xo,	2,	0,	0),
	F( 16000000,	gpll0,	10,	1,	5),
	F( 19200000,	xo,	1,	0,	0),
	F( 25000000,	gpll0,	16,	1,	2),
	F( 40000000,	gpll0,	10,	1,	2),
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
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
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
	F( 64000000,	gpll0,  1,	2,	25),
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
		VDD_DIG_FMAX_MAP2(LOWER, 32000000, NOMINAL, 64000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 32000000, NOMINAL, 64000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 32000000, NOMINAL, 64000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 32000000, NOMINAL, 64000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 37500000),
		CLK_INIT(cci_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_cpp_clk[] = {
	F( 133330000,	gpll0,	6,	0,	0),
	F( 180000000,	gpll6,	6,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 308570000,	gpll6,	3.5,	0,	0),
	F( 320000000,	gpll0,	2.5,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_camss_cpp_clk_8937[] = {
	F( 133333333,          gpll0,    6,    0,     0),
	F( 160000000,          gpll0,    5,    0,     0),
	F( 200000000,          gpll0,    4,    0,     0),
	F( 266666667,          gpll0,    3,    0,     0),
	F( 308570000,          gpll6,  3.5,    0,     0),
	F( 320000000,          gpll0,  2.5,    0,     0),
	F( 360000000,          gpll6,    3,    0,     0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_camss_cpp_clk_8917[] = {
	F( 133330000,          gpll0,    6,    0,     0),
	F( 160000000,          gpll0,    5,    0,     0),
	F( 266670000,          gpll0,    3,    0,     0),
	F( 308570000,          gpll6,  3.5,    0,     0),
	F( 320000000,          gpll0,  2.5,    0,     0),
	F( 360000000,          gpll6,    3,    0,     0),
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
		VDD_DIG_FMAX_MAP5(LOWER, 133330000, LOW, 180000000,
				  NOMINAL, 266670000, NOM_PLUS, 308570000,
				  HIGH, 320000000),
		CLK_INIT(cpp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_gp0_1_clk[] = {
	F( 100000000,	gpll0,	8,	0,	0),
	F( 160000000,	gpll0,	5,	0,	0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 160000000,
				  NOMINAL, 200000000),
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
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 160000000,
				  NOMINAL, 200000000),
		CLK_INIT(camss_gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_jpeg0_clk[] = {
	F( 133330000,	gpll0,	6,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 320000000,	gpll0,	2.5,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_camss_jpeg0_clk_8937[] = {
	F( 133333333,          gpll0,    6,    0,     0),
	F( 200000000,          gpll0,    4,    0,     0),
	F( 266666667,          gpll0,    3,    0,     0),
	F( 308570000,          gpll6,  3.5,    0,     0),
	F( 320000000,          gpll0,  2.5,    0,     0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(jpeg0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_mclk0_2_clk[] = {
	F( 19200000,	xo,	1,	0,	0),
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
		VDD_DIG_FMAX_MAP1(LOWER, 66670000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 66670000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 66670000),
		CLK_INIT(mclk2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_csi0_1phytimer_clk[] = {
	F( 100000000,	gpll0,	8,	0,	0),
	F( 160000000,	gpll0,	5,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_camss_csi0_1phytimer_clk_8917[] = {
	F( 100000000,	gpll0,	8,	0,	0),
	F( 160000000,	gpll0,	5,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 160000000,
				  NOMINAL, 200000000),
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
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 160000000,
				  NOMINAL, 200000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 80000000, NOMINAL, 160000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
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
	{
		.div_src_val = BVAL(10, 8, dsi0_phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_byte0_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct clk_freq_tbl ftbl_gcc_mdss_byte0_clk_8937[] = {
	{
		.div_src_val = BVAL(10, 8, xo_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &xo_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi0_0phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_byte0_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi1_0phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_byte1_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct rcg_clk byte0_clk_src = {
	.cmd_rcgr_reg = BYTE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.current_freq = ftbl_gcc_mdss_byte0_clk,
	.freq_tbl = ftbl_gcc_mdss_byte0_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "byte0_clk_src",
		.ops = &clk_ops_byte_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP2(LOWER, 120000000, NOMINAL, 187500000),
		CLK_INIT(byte0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_byte1_clk[] = {
	{
		.div_src_val = BVAL(10, 8, xo_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &xo_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi1_1phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_byte1_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi0_1phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_byte0_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct rcg_clk byte1_clk_src = {
		.cmd_rcgr_reg = BYTE1_CMD_RCGR,
		.set_rate = set_rate_hid,
		.current_freq = ftbl_gcc_mdss_byte1_clk,
		.freq_tbl = ftbl_gcc_mdss_byte1_clk,
		.base = &virt_bases[GCC_BASE],
		.c = {
			.dbg_name = "byte1_clk_src",
			.ops = &clk_ops_byte_multiparent,
			.flags = CLKFLAG_NO_RATE_CACHE,
			VDD_DIG_FMAX_MAP2(LOWER, 125000000, NOMINAL, 187500000),
			CLK_INIT(byte1_clk_src.c),
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
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(esc0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_esc1_clk_src[] = {
	F(  19200000,             xo,    1,    0,     0),
	F_END
};

static struct rcg_clk esc1_clk_src = {
	.cmd_rcgr_reg = ESC1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_esc1_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "esc1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(esc1_clk_src.c),
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
		VDD_DIG_FMAX_MAP3(LOWER, 160000000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_pclk0_clk[] = {
	{
		.div_src_val = BVAL(10, 8, dsi0_phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_pclk0_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct clk_freq_tbl ftbl_gcc_mdss_pclk0_clk_8937[] = {
	{
		.div_src_val = BVAL(10, 8, xo_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &xo_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi0_0phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_pclk0_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi1_0phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_pclk1_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct rcg_clk pclk0_clk_src = {
	.cmd_rcgr_reg =  PCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = ftbl_gcc_mdss_pclk0_clk,
	.freq_tbl = ftbl_gcc_mdss_pclk0_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pclk0_clk_src",
		.ops = &clk_ops_pixel_multiparent,
		VDD_DIG_FMAX_MAP2(LOWER, 160000000, NOMINAL, 250000000),
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(pclk0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_pclk1_clk[] = {
	{
		.div_src_val = BVAL(10, 8, xo_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &xo_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi1_1phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_pclk1_clk_src.c,
		.freq_hz = 0,
	},
	{
		.div_src_val = BVAL(10, 8, dsi0_1phypll_source_val)
					| BVAL(4, 0, 0),
		.src_clk = &ext_pclk0_clk_src.c,
		.freq_hz = 0,
	},
	F_END
};

static struct rcg_clk pclk1_clk_src = {
	.cmd_rcgr_reg = PCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = ftbl_gcc_mdss_pclk1_clk,
	.freq_tbl = ftbl_gcc_mdss_pclk1_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pclk1_clk_src",
		.ops = &clk_ops_pixel_multiparent,
		.flags = CLKFLAG_NO_RATE_CACHE,
		VDD_DIG_FMAX_MAP2(LOWER, 166670000, NOMINAL, 250000000),
		CLK_INIT(pclk1_clk_src.c),
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
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 64000000),
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc1_apps_clk[] = {
	F( 144000,	xo,	16,	3,	25),
	F( 400000,	xo,	12,	1,	4),
	F( 20000000,	gpll0,	10,	1,	4),
	F( 25000000,	gpll0,	16,	1,	2),
	F( 50000000,	gpll0,	16,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 177770000,	gpll0,	4.5,	0,	0),
	F( 192000000,	gpll4,	6,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F( 384000000,	gpll4,	3,	0,	0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg =  SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 50000000, NOMINAL, 384000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc1_ice_core_clk[] = {
	F( 100000000,	gpll0_out_main,	8,	0,	0),
	F( 200000000,	gpll0_out_main,	4,	0,	0),
	F_END
};

static struct rcg_clk sdcc1_ice_core_clk_src = {
	.cmd_rcgr_reg =  SDCC1_ICE_CORE_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_ice_core_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc1_ice_core_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(sdcc1_ice_core_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc2_apps_clk[] = {
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

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg =  SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 60000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 64000000),
		CLK_INIT(usb_fs_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F( 57140000,	gpll0,	14,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 133330000,	gpll0,	6,	0,	0),
	F( 177780000,	gpll0,	4.5,	0,	0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk_8917[] = {
	F( 80000000,	gpll0,	 10,	0,	0),
	F( 100000000,	gpll0,	  8,	0,	0),
	F( 133330000,	gpll0,	  6,	0,	0),
	F( 177780000,	gpll0,	4.5,	0,	0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 57140000, NOMINAL, 133330000,
				  HIGH, 177780000),
		CLK_INIT(usb_hs_system_clk_src.c),
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

static struct branch_clk gcc_dcc_clk = {
	.cbcr_reg = DCC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_dcc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_dcc_clk.c),
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
		.parent = &crypto_clk_src.c,
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
		.parent = &crypto_clk_src.c,
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_crypto_clk.c),
	},
};

static struct gate_clk gcc_oxili_gmem_clk = {
	.en_reg = OXILI_GMEM_CBCR,
	.en_mask = BIT(0),
	.delay_us = 50,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_oxili_gmem_clk",
		.parent = &gfx3d_clk_src.c,
		.ops = &clk_ops_gate,
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gcc_mdss_byte0_clk.c),
	},
};

static struct branch_clk gcc_mdss_byte1_clk = {
	.cbcr_reg = MDSS_BYTE1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_byte1_clk",
		.parent = &byte1_clk_src.c,
		.ops = &clk_ops_branch,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gcc_mdss_byte1_clk.c),
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

static struct branch_clk gcc_mdss_esc1_clk = {
	.cbcr_reg = MDSS_ESC1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_esc1_clk",
		.parent = &esc1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mdss_esc1_clk.c),
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

static DEFINE_CLK_VOTER(mdss_mdp_vote_clk, &gcc_mdss_mdp_clk.c, 0);
static DEFINE_CLK_VOTER(mdss_rotator_vote_clk, &gcc_mdss_mdp_clk.c, 0);

static struct branch_clk gcc_mdss_pclk0_clk = {
	.cbcr_reg = MDSS_PCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_pclk0_clk",
		.parent = &pclk0_clk_src.c,
		.ops = &clk_ops_branch,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gcc_mdss_pclk0_clk.c),
	},
};

static struct branch_clk gcc_mdss_pclk1_clk = {
	.cbcr_reg = MDSS_PCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_pclk1_clk",
		.parent = &pclk1_clk_src.c,
		.ops = &clk_ops_branch,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gcc_mdss_pclk1_clk.c),
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

static struct local_vote_clk gcc_apss_tcu_clk;
static struct branch_clk gcc_bimc_gfx_clk = {
	.cbcr_reg = BIMC_GFX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_bimc_gfx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_bimc_gfx_clk.c),
		.depends = &gcc_apss_tcu_clk.c,
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

static struct branch_clk gcc_oxili_timer_clk = {
	.cbcr_reg = OXILI_TIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_oxili_timer_clk",
		.parent = &xo_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_oxili_timer_clk.c),
	},
};

static struct branch_clk gcc_oxili_aon_clk = {
	.cbcr_reg = OXILI_AON_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_oxili_aon_clk",
		.parent = &gfx3d_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_oxili_aon_clk.c),
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

static struct local_vote_clk gcc_qdss_dap_clk = {
	.cbcr_reg = QDSS_DAP_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(21),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_qdss_dap_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_qdss_dap_clk.c),
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

static struct branch_clk gcc_sdcc1_ice_core_clk = {
	.cbcr_reg = SDCC1_ICE_CORE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_ice_core_clk",
		.parent = &sdcc1_ice_core_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_ice_core_clk.c),
	}
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

static struct local_vote_clk gcc_apss_tcu_clk = {
	.cbcr_reg = APSS_TCU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(1),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_apss_tcu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_apss_tcu_clk.c),
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

static void __iomem *meas_base;

static struct measure_clk apc0_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "apc0_m_clk",
		CLK_INIT(apc0_m_clk.c),
	},
};

static struct measure_clk apc1_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "apc1_m_clk",
		CLK_INIT(apc1_m_clk.c),
	},
};

static struct measure_clk cci_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "cci_m_clk",
		CLK_INIT(cci_m_clk.c),
	},
};

static struct mux_clk apss_debug_ter_mux = {
	.ops = &mux_reg_ops,
	.mask = 0x3,
	.shift = 8,
	MUX_SRC_LIST(
		{&apc0_m_clk.c, 0},
		{&apc1_m_clk.c, 1},
		{&cci_m_clk.c,	2},
	),
	.base = &meas_base,
	.c = {
		.dbg_name = "apss_debug_ter_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(apss_debug_ter_mux.c),
	},
};

static struct mux_clk apss_debug_sec_mux = {
	.ops = &mux_reg_ops,
	.mask = 0x7,
	.shift = 12,
	MUX_SRC_LIST(
		{&apss_debug_ter_mux.c, 0},
	),
	MUX_REC_SRC_LIST(
		&apss_debug_ter_mux.c,
	),
	.base = &meas_base,
	.c = {
		.dbg_name = "apss_debug_sec_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(apss_debug_sec_mux.c),
	},
};

static struct mux_clk apss_debug_pri_mux = {
	.ops = &mux_reg_ops,
	.mask = 0x3,
	.shift = 16,
	MUX_SRC_LIST(
		{&apss_debug_sec_mux.c, 0},
	),
	MUX_REC_SRC_LIST(
		&apss_debug_sec_mux.c,
	),
	.base = &meas_base,
	.c = {
		.dbg_name = "apss_debug_pri_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(apss_debug_pri_mux.c),
	},
};

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
	MUX_REC_SRC_LIST(
		&apss_debug_pri_mux.c,
	),
	MUX_SRC_LIST(
		{ &apss_debug_pri_mux.c, 0x016A},
		{ &snoc_clk.c,  0x0000 },
		{ &sysmmnoc_clk.c,  0x0001 },
		{ &pnoc_clk.c, 0x0008 },
		{ &bimc_clk.c,  0x0154 },
		{ &gcc_gp1_clk.c, 0x0010 },
		{ &gcc_gp2_clk.c, 0x0011 },
		{ &gcc_gp3_clk.c, 0x0012 },
		{ &gcc_bimc_gfx_clk.c, 0x002d },
		{ &gcc_mss_cfg_ahb_clk.c, 0x0030 },
		{ &gcc_mss_q6_bimc_axi_clk.c, 0x0031 },
		{ &gcc_apss_tcu_clk.c, 0x0050 },
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
		{ &gcc_sdcc1_ice_core_clk.c, 0x006a },
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
		{ &gcc_oxili_timer_clk.c, 0x01e9 },
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
		{ &wcnss_m_clk.c, 0x0ec },
	),
	.c = {
		.dbg_name = "gcc_debug_mux",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux.c),
	},
};

static struct mux_clk gcc_debug_mux_8937 = {
	.priv = &debug_mux_priv,
	.ops = &mux_reg_ops,
	.offset = GCC_DEBUG_CLK_CTL,
	.mask = 0x1FF,
	.en_offset = GCC_DEBUG_CLK_CTL,
	.en_mask = BIT(16),
	.base = &virt_bases[GCC_BASE],
	MUX_REC_SRC_LIST(
		&apss_debug_pri_mux.c,
	),
	MUX_SRC_LIST(
		{ &apss_debug_pri_mux.c, 0x016A},
		{ &snoc_clk.c,  0x0000 },
		{ &sysmmnoc_clk.c,  0x0001 },
		{ &pnoc_clk.c, 0x0008 },
		{ &bimc_clk.c,  0x015A },
		{ &ipa_clk.c, 0x1B0 },
		{ &gcc_gp1_clk.c, 0x0010 },
		{ &gcc_gp2_clk.c, 0x0011 },
		{ &gcc_gp3_clk.c, 0x0012 },
		{ &gcc_bimc_gfx_clk.c, 0x002d },
		{ &gcc_mss_cfg_ahb_clk.c, 0x0030 },
		{ &gcc_mss_q6_bimc_axi_clk.c, 0x0031 },
		{ &gcc_qdss_dap_clk.c, 0x0049 },
		{ &gcc_apss_tcu_clk.c, 0x0050 },
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
		{ &gcc_sdcc1_ice_core_clk.c, 0x006a },
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
		{ &gcc_camss_cpp_axi_clk.c, 0x01a4 },
		{ &gcc_venus0_core0_vcodec0_clk.c, 0x01b8 },
		{ &gcc_dcc_clk.c, 0x01b9 },
		{ &gcc_camss_mclk2_clk.c, 0x01bd },
		{ &gcc_oxili_timer_clk.c, 0x01e9 },
		{ &gcc_oxili_gfx3d_clk.c, 0x01ea },
		{ &gcc_oxili_aon_clk.c, 0x00ee },
		{ &gcc_oxili_ahb_clk.c, 0x01eb },
		{ &gcc_venus0_vcodec0_clk.c, 0x01f1 },
		{ &gcc_venus0_axi_clk.c, 0x01f2 },
		{ &gcc_venus0_ahb_clk.c, 0x01f3 },
		{ &gcc_mdss_ahb_clk.c, 0x01f6 },
		{ &gcc_mdss_axi_clk.c, 0x01f7 },
		{ &gcc_mdss_pclk0_clk.c, 0x01f8 },
		{ &gcc_mdss_pclk1_clk.c, 0x01e3 },
		{ &gcc_mdss_mdp_clk.c, 0x01f9 },
		{ &gcc_mdss_vsync_clk.c, 0x01fb },
		{ &gcc_mdss_byte0_clk.c, 0x01fc },
		{ &gcc_mdss_byte1_clk.c, 0x01e4 },
		{ &gcc_mdss_esc0_clk.c, 0x01fd },
		{ &gcc_mdss_esc1_clk.c, 0x01e5 },
		{ &wcnss_m_clk.c, 0x0ec },
	),
	.c = {
		.dbg_name = "gcc_debug_mux_8937",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux_8937.c),
	},
};

/* Clock lookup */
static struct clk_lookup msm_clocks_lookup_common[] = {
	/* RPM clocks */
	CLK_LIST(xo_clk_src),
	CLK_LIST(xo_a_clk_src),
	CLK_LIST(xo_otg_clk),
	CLK_LIST(xo_lpm_clk),
	CLK_LIST(xo_pil_mss_clk),
	CLK_LIST(xo_pil_pronto_clk),
	CLK_LIST(xo_wlan_clk),
	CLK_LIST(xo_pil_lpass_clk),

	CLK_LIST(sysmmnoc_msmbus_clk),
	CLK_LIST(snoc_msmbus_clk),
	CLK_LIST(snoc_msmbus_a_clk),
	CLK_LIST(sysmmnoc_msmbus_a_clk),
	CLK_LIST(pnoc_msmbus_clk),
	CLK_LIST(pnoc_msmbus_a_clk),
	CLK_LIST(bimc_msmbus_clk),
	CLK_LIST(bimc_msmbus_a_clk),
	CLK_LIST(pnoc_keepalive_a_clk),

	CLK_LIST(pnoc_usb_a_clk),
	CLK_LIST(snoc_usb_a_clk),
	CLK_LIST(bimc_usb_a_clk),
	CLK_LIST(pnoc_usb_clk),
	CLK_LIST(snoc_usb_clk),
	CLK_LIST(bimc_usb_clk),

	CLK_LIST(snoc_wcnss_a_clk),
	CLK_LIST(bimc_wcnss_a_clk),

	CLK_LIST(qdss_clk),
	CLK_LIST(qdss_a_clk),

	CLK_LIST(snoc_clk),
	CLK_LIST(sysmmnoc_clk),
	CLK_LIST(pnoc_clk),
	CLK_LIST(bimc_clk),
	CLK_LIST(snoc_a_clk),
	CLK_LIST(sysmmnoc_a_clk),
	CLK_LIST(pnoc_a_clk),
	CLK_LIST(bimc_a_clk),

	CLK_LIST(bb_clk1),
	CLK_LIST(bb_clk1_a),
	CLK_LIST(bb_clk2),
	CLK_LIST(bb_clk2_a),
	CLK_LIST(rf_clk2),
	CLK_LIST(rf_clk2_a),
	CLK_LIST(div_clk2),
	CLK_LIST(div_clk2_a),

	CLK_LIST(bb_clk1_pin),
	CLK_LIST(bb_clk1_a_pin),
	CLK_LIST(bb_clk2_pin),
	CLK_LIST(bb_clk2_a_pin),

	CLK_LIST(gpll0_clk_src),
	CLK_LIST(gpll0_ao_clk_src),
	CLK_LIST(gpll6_clk_src),
	CLK_LIST(gpll4_clk_src),
	CLK_LIST(gpll3_clk_src),
	CLK_LIST(a53ss_c1_pll),
	CLK_LIST(gcc_blsp1_ahb_clk),
	CLK_LIST(gcc_blsp2_ahb_clk),
	CLK_LIST(gcc_boot_rom_ahb_clk),
	CLK_LIST(gcc_crypto_ahb_clk),
	CLK_LIST(gcc_crypto_axi_clk),
	CLK_LIST(gcc_crypto_clk),
	CLK_LIST(gcc_prng_ahb_clk),
	CLK_LIST(gcc_cpp_tbu_clk),
	CLK_LIST(gcc_apss_tcu_clk),
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
	CLK_LIST(sdcc1_ice_core_clk_src),
	CLK_LIST(sdcc2_apps_clk_src),
	CLK_LIST(usb_hs_system_clk_src),
	CLK_LIST(gcc_bimc_gpu_clk),
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
	CLK_LIST(gcc_sdcc1_ice_core_clk),
	CLK_LIST(gcc_sdcc2_ahb_clk),
	CLK_LIST(gcc_sdcc2_apps_clk),
	CLK_LIST(gcc_usb2a_phy_sleep_clk),
	CLK_LIST(gcc_usb_hs_phy_cfg_ahb_clk),
	CLK_LIST(gcc_usb_hs_ahb_clk),
	CLK_LIST(gcc_usb_hs_system_clk),
	CLK_LIST(gcc_venus0_ahb_clk),
	CLK_LIST(gcc_venus0_axi_clk),
	CLK_LIST(gcc_venus0_core0_vcodec0_clk),
	CLK_LIST(gcc_venus0_vcodec0_clk),

	/* Reset clks */
	CLK_LIST(gcc_usb2_hs_phy_only_clk),
	CLK_LIST(gcc_qusb2_phy_clk),

	/* WCNSS Debug */
	CLK_LIST(wcnss_m_clk),
};

static struct clk_lookup msm_clocks_lookup_8952[] = {
	CLK_LIST(a53ss_c0_pll),
	CLK_LIST(a53ss_cci_pll),
	CLK_LIST(ipa_clk),
	CLK_LIST(ipa_a_clk),
	CLK_LIST(gpll0_clk_src_8952),
	CLK_LIST(gpll0_ao_clk_src_8952),
	CLK_LIST(gcc_oxili_gmem_clk),
	CLK_LIST(usb_fs_ic_clk_src),
	CLK_LIST(gcc_usb_fs_ic_clk),
	CLK_LIST(usb_fs_system_clk_src),
	CLK_LIST(gcc_usb_fs_system_clk),
	CLK_LIST(gcc_gfx_tcu_clk),
	CLK_LIST(gcc_gfx_tbu_clk),
	CLK_LIST(gcc_gtcu_ahb_clk),
	CLK_LIST(gcc_ipa_tbu_clk),
	CLK_LIST(gcc_usb_fs_ahb_clk),
	CLK_LIST(gcc_venus0_core1_vcodec0_clk),
	CLK_LIST(blsp1_qup1_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup1_spi_apps_clk_src),
	CLK_LIST(gcc_blsp1_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup1_spi_apps_clk),
	CLK_LIST(blsp2_qup4_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup4_spi_apps_clk_src),
	CLK_LIST(gcc_blsp2_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_spi_apps_clk),
	CLK_LIST(gcc_oxili_timer_clk),
};

static struct clk_lookup msm_clocks_lookup_8937[] = {
	CLK_LIST(gpll0_clk_src_8937),
	CLK_LIST(gpll0_ao_clk_src_8937),
	CLK_LIST(gpll0_sleep_clk_src),
	CLK_LIST(a53ss_c0_pll),
	CLK_LIST(esc1_clk_src),
	CLK_LIST(gcc_mdss_esc1_clk),
	CLK_LIST(gcc_dcc_clk),
	CLK_LIST(gcc_oxili_aon_clk),
	CLK_LIST(gcc_qdss_dap_clk),
	CLK_LIST(blsp1_qup1_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup1_spi_apps_clk_src),
	CLK_LIST(gcc_blsp1_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup1_spi_apps_clk),
	CLK_LIST(blsp2_qup4_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup4_spi_apps_clk_src),
	CLK_LIST(gcc_blsp2_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_spi_apps_clk),
	CLK_LIST(gcc_oxili_timer_clk),
};

static struct clk_lookup msm_clocks_lookup_8917[] = {
	CLK_LIST(gpll0_clk_src_8937),
	CLK_LIST(gpll0_ao_clk_src_8937),
	CLK_LIST(gpll0_sleep_clk_src),
	CLK_LIST(bimc_gpu_clk),
	CLK_LIST(bimc_gpu_a_clk),
	CLK_LIST(gcc_dcc_clk),
	CLK_LIST(gcc_qdss_dap_clk),
	CLK_LIST(gcc_gfx_tcu_clk),
	CLK_LIST(gcc_gfx_tbu_clk),
	CLK_LIST(gcc_gtcu_ahb_clk),
};

static struct clk_lookup msm_clocks_lookup_8920[] = {
	CLK_LIST(gpll0_clk_src_8937),
	CLK_LIST(gpll0_ao_clk_src_8937),
	CLK_LIST(gpll0_sleep_clk_src),
	CLK_LIST(bimc_gpu_clk),
	CLK_LIST(bimc_gpu_a_clk),
	CLK_LIST(gcc_dcc_clk),
	CLK_LIST(gcc_qdss_dap_clk),
	CLK_LIST(gcc_gfx_tcu_clk),
	CLK_LIST(gcc_gfx_tbu_clk),
	CLK_LIST(gcc_gtcu_ahb_clk),
	CLK_LIST(ipa_clk),
	CLK_LIST(ipa_a_clk),
	CLK_LIST(gcc_ipa_tbu_clk),
};

static struct clk_lookup msm_clocks_lookup_8940[] = {
	CLK_LIST(gpll0_clk_src_8937),
	CLK_LIST(gpll0_ao_clk_src_8937),
	CLK_LIST(gpll0_sleep_clk_src),
	CLK_LIST(a53ss_c0_pll),
	CLK_LIST(esc1_clk_src),
	CLK_LIST(gcc_mdss_esc1_clk),
	CLK_LIST(gcc_dcc_clk),
	CLK_LIST(gcc_oxili_aon_clk),
	CLK_LIST(gcc_qdss_dap_clk),
	CLK_LIST(blsp1_qup1_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup1_spi_apps_clk_src),
	CLK_LIST(gcc_blsp1_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup1_spi_apps_clk),
	CLK_LIST(blsp2_qup4_i2c_apps_clk_src),
	CLK_LIST(blsp2_qup4_spi_apps_clk_src),
	CLK_LIST(gcc_blsp2_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_spi_apps_clk),
	CLK_LIST(gcc_oxili_timer_clk),
	CLK_LIST(ipa_clk),
	CLK_LIST(ipa_a_clk),
	CLK_LIST(gcc_ipa_tbu_clk),
};

/* Please note that the order of reg-names is important */
static int get_mmio_addr(struct platform_device *pdev, u32 nbases)
{
	int i, count;
	const char *str;
	struct resource *res;
	struct device *dev = &pdev->dev;

	count = of_property_count_strings(dev->of_node, "reg-names");
	if (count < nbases) {
		dev_err(dev, "missing reg-names property, expected %d strings\n",
				nbases);
		return -EINVAL;
	}

	for (i = 0; i < nbases; i++) {
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

static void override_for_8917(int speed_bin)
{
	gpll3_clk_src.c.rate = 930000000;

	OVERRIDE_FMAX3(csi0,
		LOWER, 100000000, LOW, 200000000, NOMINAL, 266670000);
	/* Frequency Table same as 8937 */
	OVERRIDE_FTABLE(csi0, ftbl_gcc_camss_csi0_2_clk, 8937);
	OVERRIDE_FMAX3(csi1,
		LOWER, 100000000, LOW, 200000000, NOMINAL, 266670000);
	/* Frequency Table same as 8937 */
	OVERRIDE_FTABLE(csi1, ftbl_gcc_camss_csi0_2_clk, 8937);
	OVERRIDE_FMAX3(csi2,
		LOWER, 100000000, LOW, 200000000, NOMINAL, 266670000);
	/* Frequency Table same as 8937 */
	OVERRIDE_FTABLE(csi2, ftbl_gcc_camss_csi0_2_clk, 8937);

	OVERRIDE_FMAX5(vfe0,
		LOWER, 160000000, LOW, 266670000, NOMINAL, 320000000,
		NOM_PLUS, 329140000, HIGH, 360000000);
	OVERRIDE_FTABLE(vfe0, ftbl_gcc_camss_vfe0_1_clk, 8917);
	OVERRIDE_FMAX5(vfe1,
		LOWER, 160000000, LOW, 266670000, NOMINAL, 320000000,
		NOM_PLUS, 329140000, HIGH, 360000000);
	OVERRIDE_FTABLE(vfe1, ftbl_gcc_camss_vfe0_1_clk, 8917);
	OVERRIDE_FTABLE(vcodec0, ftbl_gcc_venus0_vcodec0_clk, 8917);
	OVERRIDE_FMAX5(vcodec0,
		LOWER, 200000000, LOW, 270000000, NOMINAL, 308570000,
		NOM_PLUS, 329140000, HIGH, 360000000);
	OVERRIDE_FMAX4(cpp,
		LOWER, 160000000, LOW, 266670000, NOMINAL, 320000000,
		NOM_PLUS, 360000000);
	OVERRIDE_FTABLE(cpp, ftbl_gcc_camss_cpp_clk, 8917);
	OVERRIDE_FMAX5(jpeg0,
		LOWER, 133330000, LOW, 200000000, NOMINAL, 266670000,
		NOM_PLUS, 308570000, HIGH, 320000000);
	/* Frequency Table same as 8937 */
	OVERRIDE_FTABLE(jpeg0, ftbl_gcc_camss_jpeg0_clk, 8937);

	if (speed_bin) {
		OVERRIDE_FMAX5(gfx3d,
			LOWER, 270000000, LOW, 400000000, NOMINAL, 484800000,
			NOM_PLUS, 523200000, HIGH, 650000000);
		OVERRIDE_FTABLE(gfx3d, ftbl_gcc_oxili_gfx3d_clk, 8917_650MHz);
	} else {
		OVERRIDE_FMAX5(gfx3d,
			LOWER, 270000000, LOW, 400000000, NOMINAL, 484800000,
			NOM_PLUS, 523200000, HIGH, 598000000);
		OVERRIDE_FTABLE(gfx3d, ftbl_gcc_oxili_gfx3d_clk, 8917);
	}

	OVERRIDE_FMAX1(cci, LOWER, 37500000);

	OVERRIDE_FTABLE(csi0phytimer, ftbl_gcc_camss_csi0_1phytimer_clk, 8917);
	OVERRIDE_FMAX3(csi0phytimer, LOWER, 100000000, LOW, 200000000,
			NOMINAL, 266670000);
	OVERRIDE_FTABLE(csi1phytimer, ftbl_gcc_camss_csi0_1phytimer_clk, 8917);
	OVERRIDE_FMAX3(csi1phytimer, LOWER, 100000000, LOW, 200000000,
			NOMINAL, 266670000);
	OVERRIDE_FMAX2(gp1, LOWER, 100000000, NOMINAL, 200000000);
	OVERRIDE_FMAX2(gp2, LOWER, 100000000, NOMINAL, 200000000);
	OVERRIDE_FMAX2(gp3, LOWER, 100000000, NOMINAL, 200000000);

	OVERRIDE_FMAX2(byte0, LOWER, 125000000, NOMINAL, 187500000);
	/* Frequency Table same as 8937 */
	OVERRIDE_FTABLE(byte0, ftbl_gcc_mdss_byte0_clk, 8937);
	byte0_clk_src.current_freq = ftbl_gcc_mdss_pclk0_clk_8937;

	OVERRIDE_FMAX2(pclk0, LOWER, 166670000, NOMINAL, 250000000);
	/* Frequency Table same as 8937 */
	OVERRIDE_FTABLE(pclk0, ftbl_gcc_mdss_pclk0_clk, 8937);
	pclk0_clk_src.current_freq = ftbl_gcc_mdss_pclk0_clk_8937;

	OVERRIDE_FMAX2(sdcc1_apps, LOWER, 200000000, NOMINAL, 400000000);
	OVERRIDE_FTABLE(usb_hs_system, ftbl_gcc_usb_hs_system_clk, 8917);
	OVERRIDE_FMAX3(usb_hs_system, LOWER, 800000000, NOMINAL, 133333000,
			HIGH, 177780000);
}

static void override_for_8937(int speed_bin)
{
	gpll3_clk_src.c.rate = 900000000;
	gpll3_clk_src.vco_tbl = p_vco_8937;
	gpll3_clk_src.num_vco = ARRAY_SIZE(p_vco_8937);
	OVERRIDE_FMAX2(gpll3, LOW, 800000000, NOMINAL, 1066000000);

	OVERRIDE_FMAX1(cci, LOWER, 37500000);
	OVERRIDE_FMAX3(csi0,
		LOWER, 100000000, LOW, 200000000, NOMINAL, 266670000);
	OVERRIDE_FTABLE(csi0, ftbl_gcc_camss_csi0_2_clk, 8937);
	OVERRIDE_FMAX3(csi1,
		LOWER, 100000000, LOW, 200000000, NOMINAL, 266670000);
	OVERRIDE_FTABLE(csi1, ftbl_gcc_camss_csi0_2_clk, 8937);
	OVERRIDE_FMAX3(csi2,
		LOWER, 100000000, LOW, 200000000, NOMINAL, 266670000);
	OVERRIDE_FTABLE(csi2, ftbl_gcc_camss_csi0_2_clk, 8937);
	OVERRIDE_FMAX4(vfe0,
		LOWER, 160000000, LOW, 308570000, NOMINAL, 400000000,
		NOM_PLUS, 432000000);
	OVERRIDE_FTABLE(vfe0, ftbl_gcc_camss_vfe0_1_clk, 8937);
	OVERRIDE_FMAX4(vfe1,
		LOWER, 160000000, LOW, 308570000, NOMINAL, 400000000,
		NOM_PLUS, 432000000);
	OVERRIDE_FTABLE(vfe1, ftbl_gcc_camss_vfe0_1_clk, 8937);

	if (speed_bin) {
		OVERRIDE_FMAX6(gfx3d,
			LOWER, 216000000, LOW, 300000000,
			NOMINAL, 375000000, NOM_PLUS, 400000000,
			HIGH, 450000000, SUPER_TUR, 475000000);
		OVERRIDE_FTABLE(gfx3d, ftbl_gcc_oxili_gfx3d_clk, 8937_475MHz);
	} else {
		OVERRIDE_FMAX5(gfx3d,
			LOWER, 216000000, LOW, 300000000,
			NOMINAL, 375000000, NOM_PLUS, 400000000,
			HIGH, 450000000);
		OVERRIDE_FTABLE(gfx3d, ftbl_gcc_oxili_gfx3d_clk, 8937);
	}

	OVERRIDE_FMAX5(cpp,
		LOWER, 160000000, LOW, 266670000, NOMINAL, 320000000,
		NOM_PLUS, 342860000, HIGH, 360000000);
	OVERRIDE_FTABLE(cpp, ftbl_gcc_camss_cpp_clk, 8937);
	OVERRIDE_FMAX5(jpeg0,
		LOWER, 133330000, LOW, 200000000, NOMINAL, 266670000,
		NOM_PLUS, 308570000, HIGH, 320000000);
	OVERRIDE_FTABLE(jpeg0, ftbl_gcc_camss_jpeg0_clk, 8937);
	OVERRIDE_FMAX2(csi0phytimer, LOWER, 100000000, LOW, 200000000);
	OVERRIDE_FMAX2(csi1phytimer, LOWER, 100000000, LOW, 200000000);
	OVERRIDE_FMAX2(gp1, LOWER, 100000000, NOMINAL, 200000000);
	OVERRIDE_FMAX2(gp2, LOWER, 100000000, NOMINAL, 200000000);
	OVERRIDE_FMAX2(gp3, LOWER, 100000000, NOMINAL, 200000000);
	OVERRIDE_FMAX2(byte0, LOWER, 125000000, NOMINAL, 187500000);
	OVERRIDE_FTABLE(byte0, ftbl_gcc_mdss_byte0_clk, 8937);
	OVERRIDE_FMAX2(byte1, LOWER, 125000000, NOMINAL, 187500000);
	byte0_clk_src.current_freq = ftbl_gcc_mdss_byte0_clk_8937;
	OVERRIDE_FMAX2(pclk0, LOWER, 166670000, NOMINAL, 250000000);
	OVERRIDE_FTABLE(pclk0, ftbl_gcc_mdss_pclk0_clk, 8937);
	OVERRIDE_FMAX2(pclk1, LOWER, 166670000, NOMINAL, 250000000);
	pclk0_clk_src.current_freq = ftbl_gcc_mdss_pclk0_clk_8937;
	OVERRIDE_FTABLE(vcodec0, ftbl_gcc_venus0_vcodec0_clk, 8937);
	OVERRIDE_FMAX5(vcodec0,
		LOWER, 166150000, LOW, 240000000, NOMINAL, 308570000,
		NOM_PLUS, 320000000, HIGH, 360000000);
	OVERRIDE_FMAX2(sdcc1_apps, LOWER, 100000000,
		NOMINAL, 400000000);
}

static void get_speed_bin(struct platform_device *pdev, int *bin)
{
	struct resource *res;
	void __iomem *base;
	u32 config_efuse;

	*bin = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (!res) {
		dev_info(&pdev->dev,
			"No GPU speed binning available. Defaulting to 0.\n");
		return;
	}

	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!base) {
		dev_warn(&pdev->dev,
			"Unable to ioremap efuse reg address. Defaulting to 0.\n");
		return;
	}

	config_efuse = readl_relaxed(base);
	devm_iounmap(&pdev->dev, base);
	*bin = (config_efuse >> 31) & 0x1;

	dev_info(&pdev->dev, "GPU speed bin: %d\n", *bin);
}

static int msm_gcc_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;
	int speed_bin;
	u32 regval, nbases = N_BASES;
	bool compat_bin = false;
	bool compat_bin2 = false;
	bool compat_bin3 = false;
	bool compat_bin4 = false;

	compat_bin = of_device_is_compatible(pdev->dev.of_node,
						"qcom,gcc-8937");

	compat_bin2 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,gcc-8917");

	compat_bin3 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,gcc-8940");

	compat_bin4 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,gcc-8920");

	ret = vote_bimc(&bimc_clk, INT_MAX);
	if (ret < 0)
		return ret;

	if (compat_bin2 || compat_bin4)
		nbases = APCS_C0_PLL_BASE;

	ret = get_mmio_addr(pdev, nbases);
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

	if (!compat_bin2 && !compat_bin4) {
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

	if (compat_bin || compat_bin3) {
		gpll0_clk_src.c.parent = &gpll0_clk_src_8937.c;
		gpll0_ao_clk_src.c.parent = &gpll0_ao_clk_src_8937.c;
		/* Oxili Ocmem in GX rail: OXILI_GMEM_CLAMP_IO */
		regval = readl_relaxed(GCC_REG_BASE(GX_DOMAIN_MISC));
		regval &= ~BIT(0);
		writel_relaxed(regval, GCC_REG_BASE(GX_DOMAIN_MISC));
		get_speed_bin(pdev, &speed_bin);
		override_for_8937(speed_bin);

		if (compat_bin3) {
			if (speed_bin) {
				gfx3d_clk_src.freq_tbl =
					ftbl_gcc_oxili_gfx3d_clk_8940_500MHz;
				gfx3d_clk_src.c.fmax[VDD_DIG_SUPER_TUR] =
								500000000;
			} else {
				gfx3d_clk_src.freq_tbl =
					ftbl_gcc_oxili_gfx3d_clk_8937_475MHz;
				gfx3d_clk_src.c.fmax[VDD_DIG_SUPER_TUR] =
								475000000;
			}
		}
	} else if (compat_bin2 || compat_bin4) {
		gpll0_clk_src.c.parent = &gpll0_clk_src_8937.c;
		gpll0_ao_clk_src.c.parent = &gpll0_ao_clk_src_8937.c;
		vdd_dig.num_levels = VDD_DIG_NUM_8917;
		vdd_dig.cur_level = VDD_DIG_NUM_8917;
		vdd_hf_pll.num_levels = VDD_HF_PLL_NUM_8917;
		vdd_hf_pll.cur_level = VDD_HF_PLL_NUM_8917;
		get_speed_bin(pdev, &speed_bin);
		override_for_8917(speed_bin);

		if (compat_bin2) {
			blsp1_qup2_spi_apps_clk_src.freq_tbl =
				ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk_8917;
			blsp1_qup3_spi_apps_clk_src.freq_tbl =
				ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk_8917;
			blsp1_qup4_spi_apps_clk_src.freq_tbl =
				ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk_8917;
			blsp2_qup1_spi_apps_clk_src.freq_tbl =
				ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk_8917;
			blsp2_qup2_spi_apps_clk_src.freq_tbl =
				ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk_8917;
			blsp2_qup3_spi_apps_clk_src.freq_tbl =
				ftbl_gcc_blsp1_2_qup1_4_spi_apps_clk_8917;
		}
	} else {
		gpll0_clk_src.c.parent = &gpll0_clk_src_8952.c;
		gpll0_ao_clk_src.c.parent = &gpll0_ao_clk_src_8952.c;
	}

	ret = of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_lookup_common,
				ARRAY_SIZE(msm_clocks_lookup_common));
	if (ret)
		return ret;

	if (compat_bin)
		ret = of_msm_clock_register(pdev->dev.of_node,
					msm_clocks_lookup_8937,
					ARRAY_SIZE(msm_clocks_lookup_8937));
	else if (compat_bin2)
		ret = of_msm_clock_register(pdev->dev.of_node,
					msm_clocks_lookup_8917,
					ARRAY_SIZE(msm_clocks_lookup_8917));
	else if (compat_bin3)
		ret = of_msm_clock_register(pdev->dev.of_node,
					msm_clocks_lookup_8940,
					ARRAY_SIZE(msm_clocks_lookup_8940));
	else if (compat_bin4)
		ret = of_msm_clock_register(pdev->dev.of_node,
					msm_clocks_lookup_8920,
					ARRAY_SIZE(msm_clocks_lookup_8920));
	else
		ret = of_msm_clock_register(pdev->dev.of_node,
					msm_clocks_lookup_8952,
					ARRAY_SIZE(msm_clocks_lookup_8952));
	if (ret)
		return ret;

	ret = enable_rpm_scaling();
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

	if (!compat_bin && !compat_bin3) {
		/* Configure Sleep and Wakeup cycles for GMEM clock */
		regval = readl_relaxed(GCC_REG_BASE(OXILI_GMEM_CBCR));
		regval ^= 0xFF0;
		regval |= CLKFLAG_WAKEUP_CYCLES << 8;
		regval |= CLKFLAG_SLEEP_CYCLES << 4;
		writel_relaxed(regval, GCC_REG_BASE(OXILI_GMEM_CBCR));
	} else {
		/* Configure Sleep and Wakeup cycles for OXILI clock */
		regval = readl_relaxed(GCC_REG_BASE(OXILI_GFX3D_CBCR));
		regval &= ~0xF0;
		regval |= CLKFLAG_SLEEP_CYCLES << 4;
		writel_relaxed(regval, GCC_REG_BASE(OXILI_GFX3D_CBCR));
	}

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered GCC clocks\n");

	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-8952" },
	{ .compatible = "qcom,gcc-8937" },
	{ .compatible = "qcom,gcc-8917" },
	{ .compatible = "qcom,gcc-8940" },
	{ .compatible = "qcom,gcc-8920" },
	{}
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_probe,
	.driver = {
		.name = "qcom,gcc-8952",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

static int msm_gcc_spm_probe(struct platform_device *pdev)
{
	struct resource *res = NULL;
	bool compat_bin = false;

	compat_bin = of_device_is_compatible(pdev->dev.of_node,
					"qcom,spm-8952");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spm_c0_base");
	if (!res) {
		dev_err(&pdev->dev, "SPM register base not defined for c0\n");
		return -ENOMEM;
	}

	a53ss_c0_pll.spm_ctrl.spm_base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!a53ss_c0_pll.spm_ctrl.spm_base) {
		dev_err(&pdev->dev, "Failed to ioremap c0 spm registers\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spm_c1_base");
	if (!res) {
		dev_err(&pdev->dev, "SPM register base not defined for c1\n");
		return -ENOMEM;
	}

	a53ss_c1_pll.spm_ctrl.spm_base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!a53ss_c1_pll.spm_ctrl.spm_base) {
		dev_err(&pdev->dev, "Failed to ioremap c1 spm registers\n");
		return -ENOMEM;
	}

	if (compat_bin) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"spm_cci_base");
		if (!res) {
			dev_err(&pdev->dev, "SPM register base not defined for cci\n");
			return -ENOMEM;
		}

		a53ss_cci_pll.spm_ctrl.spm_base = devm_ioremap(&pdev->dev,
						res->start, resource_size(res));
		if (!a53ss_cci_pll.spm_ctrl.spm_base) {
			dev_err(&pdev->dev, "Failed to ioremap cci spm registers\n");
			return -ENOMEM;
		}
	}

	dev_info(&pdev->dev, "Registered GCC SPM clocks\n");

	return 0;
}

static struct of_device_id msm_clock_spm_match_table[] = {
	{ .compatible = "qcom,gcc-spm-8952" },
	{ .compatible = "qcom,gcc-spm-8937" },
	{}
};

static struct platform_driver msm_clock_spm_driver = {
	.probe = msm_gcc_spm_probe,
	.driver = {
		.name = "qcom,gcc-spm-8952",
		.of_match_table = msm_clock_spm_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_gcc_init(void)
{
	int ret;

	ret = platform_driver_register(&msm_clock_gcc_driver);
	if (!ret)
		ret = platform_driver_register(&msm_clock_spm_driver);

	return ret;
}

static struct clk_lookup msm_clocks_measure[] = {
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
	CLK_LIST(apss_debug_pri_mux),
	CLK_LIST(apc0_m_clk),
	CLK_LIST(apc1_m_clk),
	CLK_LIST(cci_m_clk),
};

static struct clk_lookup msm_clocks_measure_8937[] = {
	CLK_LOOKUP_OF("measure", gcc_debug_mux_8937, "debug"),
	CLK_LIST(apss_debug_pri_mux),
	CLK_LIST(apc0_m_clk),
	CLK_LIST(apc1_m_clk),
	CLK_LIST(cci_m_clk),
};

static int msm_clock_debug_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	bool compat_bin = false, compat_bin2 = false;
	bool compat_bin3 = false;
	bool compat_bin4 = false;

	compat_bin = of_device_is_compatible(pdev->dev.of_node,
					"qcom,cc-debug-8937");

	compat_bin2 = of_device_is_compatible(pdev->dev.of_node,
					"qcom,cc-debug-8917");

	compat_bin3 = of_device_is_compatible(pdev->dev.of_node,
					"qcom,cc-debug-8940");

	compat_bin4 = of_device_is_compatible(pdev->dev.of_node,
					"qcom,cc-debug-8920");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "meas");
	if (!res) {
		dev_err(&pdev->dev, "GLB clock diag base not defined.\n");
		return -EINVAL;
	}

	meas_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!meas_base) {
		dev_err(&pdev->dev, "Unable to map GLB clock diag base.\n");
		return -ENOMEM;
	}

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

	if (compat_bin2)
		gcc_debug_mux_8937.post_div = 0x3;

	if (!compat_bin && !compat_bin2 && !compat_bin3 && !compat_bin4)
		ret =  of_msm_clock_register(pdev->dev.of_node,
			msm_clocks_measure, ARRAY_SIZE(msm_clocks_measure));
	else
		ret =  of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_measure_8937,
				ARRAY_SIZE(msm_clocks_measure_8937));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register debug Mux\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Debug Mux successfully\n");
	return ret;
}

static struct of_device_id msm_clock_debug_match_table[] = {
	{ .compatible = "qcom,cc-debug-8952" },
	{ .compatible = "qcom,cc-debug-8937" },
	{ .compatible = "qcom,cc-debug-8917" },
	{ .compatible = "qcom,cc-debug-8940" },
	{ .compatible = "qcom,cc-debug-8920" },
	{}
};

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_probe,
	.driver = {
		.name = "qcom,cc-debug-8952",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_clock_debug_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}

/* MDSS DSI_PHY_PLL */
static struct clk_lookup msm_clocks_gcc_mdss_common[] = {
	CLK_LIST(ext_pclk0_clk_src),
	CLK_LIST(ext_byte0_clk_src),
	CLK_LIST(byte0_clk_src),
	CLK_LIST(pclk0_clk_src),
	CLK_LIST(gcc_mdss_pclk0_clk),
	CLK_LIST(gcc_mdss_byte0_clk),
	CLK_LIST(mdss_mdp_vote_clk),
	CLK_LIST(mdss_rotator_vote_clk),
};

static struct clk_lookup msm_clocks_gcc_mdss_8937[] = {
	CLK_LIST(ext_pclk1_clk_src),
	CLK_LIST(ext_byte1_clk_src),
	CLK_LIST(byte1_clk_src),
	CLK_LIST(pclk1_clk_src),
	CLK_LIST(gcc_mdss_pclk1_clk),
	CLK_LIST(gcc_mdss_byte1_clk),
};

static int msm_gcc_mdss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct clk *curr_p;
	bool compat_bin = false;

	compat_bin = of_device_is_compatible(pdev->dev.of_node,
				"qcom,gcc-mdss-8937");

	if (!compat_bin)
		compat_bin = of_device_is_compatible(pdev->dev.of_node,
				"qcom,gcc-mdss-8940");

	curr_p = ext_pclk0_clk_src.c.parent = devm_clk_get(&pdev->dev,
								"pixel_src");
	if (IS_ERR(curr_p)) {
		dev_err(&pdev->dev, "Failed to get pixel source.\n");
		return PTR_ERR(curr_p);
	}

	curr_p = ext_byte0_clk_src.c.parent = devm_clk_get(&pdev->dev,
								"byte_src");
	if (IS_ERR(curr_p)) {
		dev_err(&pdev->dev, "Failed to get byte source.\n");
		ret = PTR_ERR(curr_p);
		goto byte0_fail;
	}

	ext_pclk0_clk_src.c.flags = CLKFLAG_NO_RATE_CACHE;
	ext_byte0_clk_src.c.flags = CLKFLAG_NO_RATE_CACHE;

	if (compat_bin) {
		curr_p = ext_pclk1_clk_src.c.parent = devm_clk_get(&pdev->dev,
									"pclk1_src");
		if (IS_ERR(curr_p)) {
			dev_err(&pdev->dev, "Failed to get pclk1 source.\n");
			ret = PTR_ERR(curr_p);
			goto fail;
		}

		curr_p = ext_byte1_clk_src.c.parent = devm_clk_get(&pdev->dev,
									"byte1_src");
		if (IS_ERR(curr_p)) {
			dev_err(&pdev->dev, "Failed to get byte1 source.\n");
			ret = PTR_ERR(curr_p);
			goto byte1_fail;
		}

		ext_pclk1_clk_src.c.flags = CLKFLAG_NO_RATE_CACHE;
		ext_byte1_clk_src.c.flags = CLKFLAG_NO_RATE_CACHE;
	}

	ret = of_msm_clock_register(pdev->dev.of_node,
			msm_clocks_gcc_mdss_common,
			ARRAY_SIZE(msm_clocks_gcc_mdss_common));
	if (ret)
		goto fail;
	if (compat_bin) {
		ret = of_msm_clock_register(pdev->dev.of_node,
		msm_clocks_gcc_mdss_8937,
		ARRAY_SIZE(msm_clocks_gcc_mdss_8937));
		if (ret)
			goto fail_8937;
	}

	dev_info(&pdev->dev, "Registered GCC MDSS clocks.\n");

	return ret;
fail_8937:
	devm_clk_put(&pdev->dev, ext_byte1_clk_src.c.parent);
byte1_fail:
	devm_clk_put(&pdev->dev, ext_pclk1_clk_src.c.parent);
fail:
	devm_clk_put(&pdev->dev, ext_byte0_clk_src.c.parent);
byte0_fail:
	devm_clk_put(&pdev->dev, ext_pclk0_clk_src.c.parent);
	return ret;

}

static struct of_device_id msm_clock_mdss_match_table[] = {
	{ .compatible = "qcom,gcc-mdss-8952" },
	{ .compatible = "qcom,gcc-mdss-8937" },
	{ .compatible = "qcom,gcc-mdss-8917" },
	{ .compatible = "qcom,gcc-mdss-8940" },
	{ .compatible = "qcom,gcc-mdss-8920" },
	{}
};

static struct platform_driver msm_clock_gcc_mdss_driver = {
	.probe = msm_gcc_mdss_probe,
	.driver = {
		.name = "gcc-mdss-8952",
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

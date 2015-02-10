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
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-voter.h>

#include <linux/clk/msm-clock-generic.h>
#include <linux/regulator/rpm-smd-regulator.h>

#include <dt-bindings/clock/msm-clocks-8909.h>

#include "clock.h"

enum {
	GCC_BASE,
	APCS_PLL_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))

#define GPLL0_MODE					0x21000
#define GPLL0_L_VAL					0x21004
#define GPLL0_ALPHA_VAL					0x21008
#define GPLL0_ALPHA_VAL_U				0x2100C
#define GPLL0_USER_CTL					0x21010
#define GPLL0_CONFIG_CTL				0x21018
#define GPLL0_STATUS					0x21024
#define GPLL1_MODE					0x20000
#define GPLL1_L_VAL					0x20004
#define GPLL1_M_VAL					0x20008
#define GPLL1_N_VAL					0x2000C
#define GPLL1_USER_CTL					0x20010
#define GPLL1_CONFIG_CTL				0x20014
#define GPLL1_STATUS					0x2001C
#define GPLL2_MODE					0x25000
#define GPLL2_L_VAL					0x25004
#define GPLL2_ALPHA_VAL					0x25008
#define GPLL2_ALPHA_VAL_U				0x2500C
#define GPLL2_USER_CTL					0x25010
#define GPLL2_CONFIG_CTL				0x25018
#define GPLL2_STATUS					0x25024
#define SNOC_QOSGEN					0x2601C
#define MSS_CFG_AHB_CBCR				0x49000
#define MSS_Q6_BIMC_AXI_CBCR				0x49004
#define QPIC_AHB_CBCR					0x3F01C
#define USB_HS_BCR					0x41000
#define USB_HS_SYSTEM_CBCR				0x41004
#define USB_HS_AHB_CBCR					0x41008
#define USB_HS_SYSTEM_CMD_RCGR				0x41010
#define USB2A_PHY_SLEEP_CBCR				0x4102C
#define USB_HS_PHY_CFG_AHB_CBCR				0x41030
#define USB2_HS_PHY_ONLY_BCR				0x41034
#define QUSB2_PHY_BCR					0x4103C
#define SDCC1_APPS_CMD_RCGR				0x42004
#define SDCC1_APPS_CBCR					0x42018
#define SDCC1_AHB_CBCR					0x4201C
#define SDCC2_APPS_CMD_RCGR				0x43004
#define SDCC2_APPS_CBCR					0x43018
#define SDCC2_AHB_CBCR					0x4301C
#define BLSP1_AHB_CBCR					0x01008
#define BLSP1_QUP1_SPI_APPS_CBCR			0x02004
#define BLSP1_QUP1_I2C_APPS_CBCR			0x02008
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR			0x0200C
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR			0x03000
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR			0x04000
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR			0x05000
#define BLSP1_QUP5_I2C_APPS_CMD_RCGR			0x06000
#define BLSP1_QUP6_I2C_APPS_CMD_RCGR			0x07000
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR			0x02024
#define BLSP1_UART1_APPS_CBCR				0x0203C
#define BLSP1_UART1_APPS_CMD_RCGR			0x02044
#define BLSP1_QUP2_SPI_APPS_CBCR			0x0300C
#define BLSP1_QUP2_I2C_APPS_CBCR			0x03010
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR			0x03014
#define BLSP1_UART2_APPS_CBCR				0x0302C
#define BLSP1_UART2_APPS_CMD_RCGR			0x03034
#define BLSP1_QUP3_SPI_APPS_CBCR			0x0401C
#define BLSP1_QUP3_I2C_APPS_CBCR			0x04020
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR			0x04024
#define BLSP1_QUP4_SPI_APPS_CBCR			0x0501C
#define BLSP1_QUP4_I2C_APPS_CBCR			0x05020
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR			0x05024
#define BLSP1_QUP5_SPI_APPS_CBCR			0x0601C
#define BLSP1_QUP5_I2C_APPS_CBCR			0x06020
#define BLSP1_QUP5_SPI_APPS_CMD_RCGR			0x06024
#define BLSP1_QUP6_SPI_APPS_CBCR			0x0701C
#define BLSP1_QUP6_I2C_APPS_CBCR			0x07020
#define BLSP1_QUP6_SPI_APPS_CMD_RCGR			0x07024
#define PDM_AHB_CBCR					0x44004
#define PDM2_CBCR					0x4400C
#define PDM2_CMD_RCGR					0x44010
#define PRNG_AHB_CBCR					0x13004
#define BOOT_ROM_AHB_CBCR				0x1300C
#define CRYPTO_CMD_RCGR					0x16004
#define CRYPTO_CBCR					0x1601C
#define CRYPTO_AXI_CBCR					0x16020
#define CRYPTO_AHB_CBCR					0x16024
#define GCC_XO_DIV4_CBCR				0x30034
#define GFX_TBU_CBCR					0x12010
#define VENUS_TBU_CBCR					0x12014
#define MDP_TBU_CBCR					0x1201C
#define APSS_TCU_CBCR					0x12018
#define GFX_TCU_CBCR					0x12020
#define MSS_TBU_AXI_CBCR				0x12024
#define MSS_TBU_GSS_AXI_CBCR				0x12028
#define MSS_TBU_Q6_AXI_CBCR				0x1202C
#define SMMU_CFG_CBCR					0x12038
#define VFE_TBU_CBCR					0x1203C
#define GTCU_AHB_CBCR					0x12044
#define GTCU_AHB_BRIDGE_CBCR				0x12094
#define APCS_GPLL_ENA_VOTE				0x45000
#define APCS_CLOCK_BRANCH_ENA_VOTE			0x45004
#define APCS_CLOCK_SLEEP_ENA_VOTE			0x45008
#define APCS_SMMU_CLOCK_BRANCH_ENA_VOTE			0x4500C
#define APSS_AHB_CMD_RCGR				0x46000
#define GCC_DEBUG_CLK_CTL				0x74000
#define CLOCK_FRQ_MEASURE_CTL				0x74004
#define CLOCK_FRQ_MEASURE_STATUS			0x74008
#define GCC_PLLTEST_PAD_CFG				0x7400C
#define GP1_CBCR					0x08000
#define GP1_CMD_RCGR					0x08004
#define GP2_CBCR					0x09000
#define GP2_CMD_RCGR					0x09004
#define GP3_CBCR					0x0A000
#define GP3_CMD_RCGR					0x0A004
#define VCODEC0_CMD_RCGR				0x4C000
#define VENUS0_VCODEC0_CBCR				0x4C01C
#define VENUS0_CORE0_VCODEC0_CBCR			0x4C02C
#define VENUS0_AHB_CBCR					0x4C020
#define VENUS0_AXI_CBCR					0x4C024
#define PCLK0_CMD_RCGR					0x4D000
#define MDP_CMD_RCGR					0x4D014
#define VSYNC_CMD_RCGR					0x4D02C
#define BYTE0_CMD_RCGR					0x4D044
#define ESC0_CMD_RCGR					0x4D05C
#define MDSS_BCR					0x4D074
#define MDSS_AHB_CBCR					0x4D07C
#define MDSS_AXI_CBCR					0x4D080
#define MDSS_PCLK0_CBCR					0x4D084
#define MDSS_MDP_CBCR					0x4D088
#define MDSS_VSYNC_CBCR					0x4D090
#define MDSS_BYTE0_CBCR					0x4D094
#define MDSS_ESC0_CBCR					0x4D098
#define CSI0PHYTIMER_CMD_RCGR				0x4E000
#define CAMSS_CSI0PHYTIMER_CBCR				0x4E01C
#define CSI1PHYTIMER_CMD_RCGR				0x4F000
#define CAMSS_CSI1PHYTIMER_CBCR				0x4F01C
#define CSI0_CMD_RCGR					0x4E020
#define CAMSS_CSI0_CBCR					0x4E03C
#define CAMSS_CSI0_AHB_CBCR				0x4E040
#define CAMSS_CSI0PHY_CBCR				0x4E048
#define CAMSS_CSI0RDI_CBCR				0x4E050
#define CAMSS_CSI0PIX_CBCR				0x4E058
#define CSI1_CMD_RCGR					0x4F020
#define CAMSS_CSI1_CBCR					0x4F03C
#define CAMSS_CSI1_AHB_CBCR				0x4F040
#define CAMSS_CSI1PHY_CBCR				0x4F048
#define CAMSS_CSI1RDI_CBCR				0x4F050
#define CAMSS_CSI1PIX_CBCR				0x4F058
#define CAMSS_ISPIF_AHB_CBCR				0x50004
#define CCI_CMD_RCGR					0x51000
#define CAMSS_CCI_CBCR					0x51018
#define CAMSS_CCI_AHB_CBCR				0x5101C
#define MCLK0_CMD_RCGR					0x52000
#define CAMSS_MCLK0_CBCR				0x52018
#define MCLK1_CMD_RCGR					0x53000
#define CAMSS_MCLK1_CBCR				0x53018
#define CAMSS_GP0_CMD_RCGR				0x54000
#define CAMSS_GP0_CBCR					0x54018
#define CAMSS_GP1_CMD_RCGR				0x55000
#define CAMSS_GP1_CBCR					0x55018
#define CAMSS_AHB_CBCR					0x5A014
#define CAMSS_TOP_AHB_CBCR				0x56004
#define VFE0_CMD_RCGR					0x58000
#define CAMSS_VFE_BCR					0x58030
#define CAMSS_VFE0_CBCR					0x58038
#define CAMSS_VFE_AHB_CBCR				0x58044
#define CAMSS_VFE_AXI_CBCR				0x58048
#define CAMSS_CSI_VFE0_CBCR				0x58050
#define GFX3D_CMD_RCGR					0x59000
#define OXILI_GFX3D_CBCR				0x59020
#define OXILI_AHB_CBCR					0x59028
#define CAMSS_TOP_AHB_CMD_RCGR				0x5A000
#define BIMC_GFX_CBCR					0x31024
#define BIMC_GPU_CBCR					0x31040

#define APCS_SH_PLL_MODE				0x00000
#define APCS_SH_PLL_L_VAL				0x00004
#define APCS_SH_PLL_M_VAL				0x00008
#define APCS_SH_PLL_N_VAL				0x0000C
#define APCS_SH_PLL_USER_CTL				0x00010
#define APCS_SH_PLL_CONFIG_CTL				0x00014
#define APCS_SH_PLL_STATUS				0x0001C

/* Mux source select values */
#define xo_source_val			0
#define xo_a_source_val			0
#define gpll0_source_val		1
#define gpll0_aux_source_val		3
#define gpll1_e_source_val              3
#define gpll1_e_gfx3d_source_val	2
#define gpll2_source_val		3
#define dsi0_phypll_mm_source_val	1

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

#define F_MDSS(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_mm_source_val), \
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
	VDD_DIG_LOWER,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_HIGH,
	VDD_DIG_NUM
};

static int vdd_corner[] = {
	RPM_REGULATOR_CORNER_NONE,              /* VDD_DIG_NONE */
	RPM_REGULATOR_CORNER_SVS_KRAIT,         /* VDD_DIG_LOWER SVS */
	RPM_REGULATOR_CORNER_SVS_SOC,           /* VDD_DIG_LOW SVS */
	RPM_REGULATOR_CORNER_NORMAL,            /* VDD_DIG_NOMINAL */
	RPM_REGULATOR_CORNER_SUPER_TURBO,       /* VDD_DIG_HIGH */
};

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

DEFINE_EXT_CLK(xo_clk_src, NULL);
DEFINE_EXT_CLK(xo_a_clk_src, NULL);
DEFINE_EXT_CLK(rpm_debug_clk, NULL);
DEFINE_EXT_CLK(apss_debug_clk, NULL);

DEFINE_CLK_DUMMY(wcnss_m_clk, 0);

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

static struct pll_freq_tbl apcs_pll_freq[] = {
	F_APCS_PLL( 998400000, 52, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1094400000, 57, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1190400000, 62, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1248000000, 65, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1305600000, 68, 0x0, 0x1, 0x0, 0x0, 0x0),
	PLL_F_END
};

static struct pll_clk a7sspll = {
	.mode_reg = (void __iomem *)APCS_SH_PLL_MODE,
	.l_reg = (void __iomem *)APCS_SH_PLL_L_VAL,
	.m_reg = (void __iomem *)APCS_SH_PLL_M_VAL,
	.n_reg = (void __iomem *)APCS_SH_PLL_N_VAL,
	.config_reg = (void __iomem *)APCS_SH_PLL_USER_CTL,
	.status_reg = (void __iomem *)APCS_SH_PLL_STATUS,
	.freq_tbl = apcs_pll_freq,
	.masks = {
		.vco_mask = BM(29, 28),
		.pre_div_mask = BIT(12),
		.post_div_mask = BM(9, 8),
		.mn_en_mask = BIT(24),
		.main_output_mask = BIT(0),
	},
	.base = &virt_bases[APCS_PLL_BASE],
	.c = {
		.parent = &xo_a_clk_src.c,
		.dbg_name = "a7sspll",
		.ops = &clk_ops_sr2_pll,
		.vdd_class = &vdd_sr2_pll,
		.fmax = (unsigned long [VDD_SR2_PLL_NUM]) {
			[VDD_SR2_PLL_SVS] = 1000000000,
			[VDD_SR2_PLL_NOM] = 1900000000,
		},
		.num_fmax = VDD_SR2_PLL_NUM,
		CLK_INIT(a7sspll.c),
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
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
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

/* Don't vote for xo if using this clock to allow xo shutdown */
static struct pll_vote_clk gpll0_ao_clk_src = {
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
		.dbg_name = "gpll0_ao_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao_clk_src.c),
	},
};

static struct pll_vote_clk gpll1_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)GPLL1_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 614400000,
		.dbg_name = "gpll1_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll1_clk_src.c),
	},
};

DEFINE_EXT_CLK(gpll1_e_clk_src, &gpll1_clk_src.c);
DEFINE_EXT_CLK(gpll1_e_gfx3d_clk_src, &gpll1_clk_src.c);

/* PLL_ACTIVE_FLAG bit of GCC_GPLL2_MODE register
 * gets set from PLL voting FSM.It indicates when
 * FSM has enabled the PLL and PLL should be locked.
 */
static struct pll_vote_clk gpll2_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(3),
	.status_reg = (void __iomem *)GPLL2_MODE,
	.status_mask = BIT(30),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 792000000,
		.dbg_name = "gpll2_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_apss_ahb_clk[] = {
	F( 19200000,	xo_a,	1,	0,	0),
	F( 50000000,	gpll0,	16,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
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

static struct clk_freq_tbl ftbl_gcc_camss_top_ahb_clk[] = {
	F(  40000000,	   gpll0,  10,	  1,	2),
	F(  80000000,	   gpll0,  10,	  0,	0),
	F_END
};

static struct rcg_clk camss_top_ahb_clk_src = {
	.cmd_rcgr_reg = CAMSS_TOP_AHB_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_top_ahb_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "camss_top_ahb_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 40000000, NOMINAL, 80000000),
		CLK_INIT(camss_top_ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_csi0_1_clk[] = {
	F( 100000000,	gpll0,	8,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F_END
};

static struct rcg_clk csi0_clk_src = {
	.cmd_rcgr_reg = CSI0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_camss_csi0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "csi0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi0_clk_src.c),
	},
};

static struct rcg_clk csi1_clk_src = {
	.cmd_rcgr_reg = CSI1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_camss_csi0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "csi1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_vfe0_clk[] = {
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
	.freq_tbl = ftbl_gcc_camss_vfe0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "vfe0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOWER, 133330000, NOMINAL, 266670000, HIGH,
					320000000),
		CLK_INIT(vfe0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_venus0_vcodec0_clk[] = {
	F( 133330000,	gpll0,	6,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 307200000, gpll1_e,	4,	0,	0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 133330000, NOMINAL, 266670000, HIGH,
					307200000),
		CLK_INIT(vcodec0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_6_i2c_apps_clk[] = {
	F( 19200000,	xo,	1,	0,	0),
	F( 50000000,	gpll0,	16,	0,	0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_6_spi_apps_clk[] = {
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup5_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg =  BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_uart1_2_apps_clk[] = {
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
	.freq_tbl = ftbl_gcc_blsp1_uart1_2_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_uart1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 32000000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_gp0_1_clk[] = {
	F( 100000000,	gpll0,	8,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F_END
};

static struct rcg_clk camss_gp0_clk_src = {
	.cmd_rcgr_reg =  CAMSS_GP0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "camss_gp0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(camss_gp0_clk_src.c),
	},
};

static struct rcg_clk camss_gp1_clk_src = {
	.cmd_rcgr_reg =  CAMSS_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "camss_gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(camss_gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_mclk0_1_clk[] = {
	F( 24000000,	gpll2,	1,	1,	33),
	F( 66670000,	gpll0,	12,	0,	0),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg =  MCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_mclk0_1_clk,
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
	.freq_tbl = ftbl_gcc_camss_mclk0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOWER, 66670000),
		CLK_INIT(mclk1_clk_src.c),
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
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi0phytimer_clk_src.c),
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
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_byte0_clk[] = {
	{
		.div_src_val = BVAL(10, 8, dsi0_phypll_mm_source_val),
	},
};

static struct rcg_clk byte0_clk_src = {
	.cmd_rcgr_reg = BYTE0_CMD_RCGR,
	.current_freq = ftbl_gcc_mdss_byte0_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "byte0_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP1(LOWER, 125000000),
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
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(esc0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_mdp_clk[] = {
	F( 50000000,	gpll0,	16,	0,	0),
	F( 80000000,	gpll0,	10,	0,	0),
	F( 100000000,	gpll0,	8,	0,	0),
	F( 160000000,	gpll0,	5,	0,	0),
	F( 177780000,	gpll0,	4.5,	0,	0),
	F( 200000000,	gpll0,	4,	0,	0),
	F( 266670000,	gpll0,	3,	0,	0),
	F( 307200000,	gpll1_e,	4,	0,	0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 160000000, LOW, 200000000, NOMINAL,
			      307200000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_pclk0_clk[] = {
	{
		.div_src_val = BVAL(10, 8, dsi0_phypll_mm_source_val)
					| BVAL(4, 0, 0),
	},
};

static struct rcg_clk pclk0_clk_src = {
	.cmd_rcgr_reg = PCLK0_CMD_RCGR,
	.current_freq = ftbl_gcc_mdss_pclk0_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pclk0_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP1(LOWER, 83333333.33),
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
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(vsync_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_oxili_gfx3d_clk[] = {
	F( 19200000,	xo,		1,	0,	0),
	F( 50000000,	gpll0,		16,	0,	0),
	F( 80000000,	gpll0,		10,	0,	0),
	F( 100000000,	gpll0,		8,	0,	0),
	F( 160000000,	gpll0,		5,	0,	0),
	F( 177780000,	gpll0,		4.5,	0,	0),
	F( 200000000,	gpll0,		4,	0,	0),
	F( 266670000,	gpll0,		3,	0,	0),
	F( 307200000,	gpll1_e_gfx3d,		4,	0,	0),
	F( 409600000,	gpll1_e_gfx3d,		3,	0,	0),
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
		VDD_DIG_FMAX_MAP3(LOWER, 200000000, NOMINAL, 307200000, HIGH,
					409600000),
		CLK_INIT(gfx3d_clk_src.c),
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
		VDD_DIG_FMAX_MAP2(LOWER, 50000000, NOMINAL, 200000000),
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
		VDD_DIG_FMAX_MAP2(LOWER, 50000000, NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F( 57140000,	gpll0,	14,	0,	0),
	F( 80000000,	gpll0,	10,	0,	0),
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
		VDD_DIG_FMAX_MAP2(LOWER, 57142857.14, NOMINAL, 80000000),
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

static struct branch_clk gcc_blsp1_qup5_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup5_i2c_apps_clk",
		.parent = &blsp1_qup5_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup5_spi_apps_clk",
		.parent = &blsp1_qup5_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup6_i2c_apps_clk",
		.parent = &blsp1_qup6_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup6_spi_apps_clk",
		.parent = &blsp1_qup6_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_spi_apps_clk.c),
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
	.has_sibling = 0,
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
		.parent = &camss_top_ahb_clk_src.c,
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

static struct gate_clk gcc_snoc_qosgen_clk = {
	.en_mask = BIT(0),
	.en_reg = SNOC_QOSGEN,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_snoc_qosgen_clk",
		.ops = &clk_ops_gate,
		.flags = CLKFLAG_SKIP_HANDOFF,
		CLK_INIT(gcc_snoc_qosgen_clk.c),
	},
};

static struct mux_clk gcc_debug_mux;
static struct clk_ops clk_ops_debug_mux;

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

static struct measure_clk apc2_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "apc2_m_clk",
		CLK_INIT(apc2_m_clk.c),
	},
};

static struct measure_clk apc3_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "apc3_m_clk",
		CLK_INIT(apc3_m_clk.c),
	},
};

static struct measure_clk l2_m_clk = {
	.c = {
		.ops = &clk_ops_empty,
		.dbg_name = "l2_m_clk",
		CLK_INIT(l2_m_clk.c),
	},
};

static void __iomem *meas_base;

static struct mux_clk apss_debug_ter_mux = {
	.ops = &mux_reg_ops,
	.mask = 0x3,
	.shift = 8,
	MUX_SRC_LIST(
		{&apc0_m_clk.c, 0},
		{&apc1_m_clk.c, 1},
		{&apc2_m_clk.c, 2},
		{&apc3_m_clk.c, 3},
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
		{&l2_m_clk.c, 1},
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

static int  gcc_set_mux_sel(struct mux_clk *clk, int sel)
{
	u32 regval;

	regval = readl_relaxed(GCC_REG_BASE(GCC_DEBUG_CLK_CTL));
	regval &= 0x1FF;
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	if (sel == 0xFFFF)
		return 0;

	mux_reg_ops.set_mux_sel(clk, sel);

	return 0;
};

static struct clk_mux_ops gcc_debug_mux_ops;
static struct mux_clk gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.ops = &gcc_debug_mux_ops,
	.offset = GCC_DEBUG_CLK_CTL,
	.mask = 0x1FF,
	.en_offset = GCC_DEBUG_CLK_CTL,
	.en_mask = BIT(16),
	.base = &virt_bases[GCC_BASE],
	MUX_REC_SRC_LIST(
		&rpm_debug_clk.c,
		&apss_debug_pri_mux.c,
	),
	MUX_SRC_LIST(
		{ &rpm_debug_clk.c,	0xFFFF},
		{ &apss_debug_pri_mux.c, 0x016A},
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
		{ &gcc_blsp1_qup4_spi_apps_clk.c, 0x0098 },
		{ &gcc_blsp1_qup4_i2c_apps_clk.c, 0x0099 },
		{ &gcc_blsp1_qup5_spi_apps_clk.c, 0x009c },
		{ &gcc_blsp1_qup5_i2c_apps_clk.c, 0x009d },
		{ &gcc_blsp1_qup6_spi_apps_clk.c, 0x00a1 },
		{ &gcc_blsp1_qup6_i2c_apps_clk.c, 0x00a2 },
		{ &gcc_camss_top_ahb_clk.c, 0x00a8 },
		{ &gcc_camss_ahb_clk.c, 0x00a9 },
		{ &gcc_camss_gp0_clk.c, 0x00ab },
		{ &gcc_camss_gp1_clk.c, 0x00ac },
		{ &gcc_camss_mclk0_clk.c, 0x00ad },
		{ &gcc_camss_mclk1_clk.c, 0x00ae },
		{ &gcc_camss_csi0phytimer_clk.c, 0x00b1 },
		{ &gcc_camss_vfe0_clk.c, 0x00b8 },
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
		{ &gcc_venus0_core0_vcodec0_clk.c, 0x00ee },
		{ &gcc_boot_rom_ahb_clk.c, 0x00f8 },
		{ &gcc_crypto_clk.c, 0x0138 },
		{ &gcc_crypto_axi_clk.c, 0x0139 },
		{ &gcc_crypto_ahb_clk.c, 0x013a },
		{ &gcc_bimc_gpu_clk.c, 0x0157 },
		{ &gcc_oxili_gfx3d_clk.c, 0x01ea },
		{ &gcc_oxili_ahb_clk.c, 0x01eb },
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
		{ &wcnss_m_clk.c, 0x0198},
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
	/* PLLs */
	CLK_LIST(gpll0_clk_src),
	CLK_LIST(gpll0_ao_clk_src),
	CLK_LIST(gpll1_clk_src),
	CLK_LIST(gpll2_clk_src),
	CLK_LIST(a7sspll),

	/* RCGs */
	CLK_LIST(apss_ahb_clk_src),
	CLK_LIST(camss_top_ahb_clk_src),
	CLK_LIST(csi0_clk_src),
	CLK_LIST(csi1_clk_src),
	CLK_LIST(vfe0_clk_src),
	CLK_LIST(mdp_clk_src),
	CLK_LIST(gfx3d_clk_src),
	CLK_LIST(blsp1_qup1_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup1_spi_apps_clk_src),
	CLK_LIST(blsp1_qup2_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup2_spi_apps_clk_src),
	CLK_LIST(blsp1_qup3_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup3_spi_apps_clk_src),
	CLK_LIST(blsp1_qup4_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup4_spi_apps_clk_src),
	CLK_LIST(blsp1_qup5_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup5_spi_apps_clk_src),
	CLK_LIST(blsp1_qup6_i2c_apps_clk_src),
	CLK_LIST(blsp1_qup6_spi_apps_clk_src),
	CLK_LIST(blsp1_uart1_apps_clk_src),
	CLK_LIST(blsp1_uart2_apps_clk_src),
	CLK_LIST(camss_gp0_clk_src),
	CLK_LIST(camss_gp1_clk_src),
	CLK_LIST(mclk0_clk_src),
	CLK_LIST(mclk1_clk_src),
	CLK_LIST(csi0phytimer_clk_src),
	CLK_LIST(gp1_clk_src),
	CLK_LIST(gp2_clk_src),
	CLK_LIST(gp3_clk_src),
	CLK_LIST(esc0_clk_src),
	CLK_LIST(vsync_clk_src),
	CLK_LIST(pdm2_clk_src),
	CLK_LIST(sdcc1_apps_clk_src),
	CLK_LIST(sdcc2_apps_clk_src),
	CLK_LIST(usb_hs_system_clk_src),
	CLK_LIST(vcodec0_clk_src),

	/* Voteable Clocks */
	CLK_LIST(gcc_blsp1_ahb_clk),
	CLK_LIST(gcc_boot_rom_ahb_clk),
	CLK_LIST(gcc_prng_ahb_clk),
	CLK_LIST(gcc_apss_tcu_clk),
	CLK_LIST(gcc_gfx_tbu_clk),
	CLK_LIST(gcc_gfx_tcu_clk),
	CLK_LIST(gcc_mdp_tbu_clk),
	CLK_LIST(gcc_gtcu_ahb_clk),
	CLK_LIST(gcc_smmu_cfg_clk),
	CLK_LIST(gcc_venus_tbu_clk),
	CLK_LIST(gcc_vfe_tbu_clk),

	/* Branches */
	CLK_LIST(gcc_blsp1_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup1_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup5_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup5_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup6_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_qup6_spi_apps_clk),
	CLK_LIST(gcc_blsp1_uart1_apps_clk),
	CLK_LIST(gcc_blsp1_uart2_apps_clk),
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
	CLK_LIST(gcc_camss_csi_vfe0_clk),
	CLK_LIST(gcc_camss_gp0_clk),
	CLK_LIST(gcc_camss_gp1_clk),
	CLK_LIST(gcc_camss_ispif_ahb_clk),
	CLK_LIST(gcc_camss_mclk0_clk),
	CLK_LIST(gcc_camss_mclk1_clk),
	CLK_LIST(gcc_camss_csi0phytimer_clk),
	CLK_LIST(gcc_camss_ahb_clk),
	CLK_LIST(gcc_camss_top_ahb_clk),
	CLK_LIST(gcc_camss_vfe0_clk),
	CLK_LIST(gcc_camss_vfe_ahb_clk),
	CLK_LIST(gcc_camss_vfe_axi_clk),
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
	CLK_LIST(gcc_oxili_ahb_clk),
	CLK_LIST(gcc_oxili_gfx3d_clk),
	CLK_LIST(gcc_pdm2_clk),
	CLK_LIST(gcc_pdm_ahb_clk),
	CLK_LIST(gcc_sdcc1_ahb_clk),
	CLK_LIST(gcc_sdcc1_apps_clk),
	CLK_LIST(gcc_sdcc2_ahb_clk),
	CLK_LIST(gcc_sdcc2_apps_clk),
	CLK_LIST(gcc_usb2a_phy_sleep_clk),
	CLK_LIST(gcc_usb_hs_ahb_clk),
	CLK_LIST(gcc_usb_hs_system_clk),
	CLK_LIST(gcc_venus0_ahb_clk),
	CLK_LIST(gcc_venus0_axi_clk),
	CLK_LIST(gcc_venus0_vcodec0_clk),
	CLK_LIST(gcc_bimc_gfx_clk),
	CLK_LIST(gcc_bimc_gpu_clk),
	CLK_LIST(gcc_venus0_core0_vcodec0_clk),
	CLK_LIST(gcc_usb_hs_phy_cfg_ahb_clk),
	CLK_LIST(wcnss_m_clk),

	/* Crypto clocks */
	CLK_LIST(gcc_crypto_clk),
	CLK_LIST(gcc_crypto_ahb_clk),
	CLK_LIST(gcc_crypto_axi_clk),
	CLK_LIST(crypto_clk_src),

	/* Reset clocks */
	CLK_LIST(gcc_usb2_hs_phy_only_clk),
	CLK_LIST(gcc_qusb2_phy_clk),

	/* QoS Reference clock */
	CLK_LIST(gcc_snoc_qosgen_clk),
};

static int add_dev_opp(struct clk *c, struct device *dev,
				unsigned long max_rate)
{
	unsigned long rate = 0;
	int level;
	long ret;

	while (1) {
		ret = clk_round_rate(c, rate + 1);
		if (ret < 0) {
			pr_warn("round_rate failed at %lu\n", rate);
			return ret;
		}
		rate = ret;
		level = find_vdd_level(c, rate);
		if (level <= 0) {
			pr_warn("no uv for %lu.\n", rate);
			return -EINVAL;
		}
		ret = dev_pm_opp_add(dev, rate, c->vdd_class->vdd_uv[level]);
		if (ret) {
			pr_warn("failed to add OPP for %lu\n", rate);
			return ret;
		}
		if (rate >= max_rate)
			break;
	}
	return 0;
}

static void register_opp_for_dev(struct platform_device *pdev)
{
	struct clk *opp_clk, *opp_clk_src;
	unsigned long dev_fmax;

	opp_clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(opp_clk)) {
		pr_err("Error getting core clk: %lu\n", PTR_ERR(opp_clk));
		return;
	}
	opp_clk_src = opp_clk;
	if (opp_clk->num_fmax <= 0) {
		if (opp_clk->parent && opp_clk->parent->num_fmax > 0)
			opp_clk_src = opp_clk->parent;
		else
			return;
	}

	dev_fmax = opp_clk_src->fmax[opp_clk_src->num_fmax - 1];
	WARN(add_dev_opp(opp_clk_src, &pdev->dev, dev_fmax),
		"Failed to add OPP levels for dev\n");
}

static int msm_gcc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *xo_gcc;
	int ret, node = 0;
	u32 regval;
	struct device_node *opp_dev_node = NULL;
	struct platform_device *opp_dev = NULL;

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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs_base");
	if (!res) {
		dev_err(&pdev->dev, "APCS PLL Register base not defined\n");
		return -ENOMEM;
	}

	virt_bases[APCS_PLL_BASE] = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!virt_bases[APCS_PLL_BASE]) {
		dev_err(&pdev->dev, "Failed to ioremap APCS PLL registers\n");
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
		if (!(PTR_ERR(vdd_sr2_pll.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_sr2_pll regulator!!!\n");
		return PTR_ERR(vdd_sr2_pll.regulator[0]);
	}

	vdd_sr2_pll.regulator[1] = devm_regulator_get(&pdev->dev,
							"vdd_sr2_dig");
	if (IS_ERR(vdd_sr2_pll.regulator[1])) {
		if (!(PTR_ERR(vdd_sr2_pll.regulator[1]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_sr2_dig regulator!!!\n");
		return PTR_ERR(vdd_sr2_pll.regulator[1]);
	}

	xo_gcc = xo_clk_src.c.parent = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(xo_gcc)) {
		if (!(PTR_ERR(xo_gcc) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get XO clock!!!\n");
		return PTR_ERR(xo_gcc);
	}

	 /*Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));

	xo_a_clk_src.c.parent = clk_get(&pdev->dev, "xo_a");
	if (IS_ERR(xo_a_clk_src.c.parent)) {
		if (!(PTR_ERR(xo_a_clk_src.c.parent) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get xo_a clock!!!\n");
		return PTR_ERR(xo_a_clk_src.c.parent);
	}

	ret = of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_lookup,
				ARRAY_SIZE(msm_clocks_lookup));
	if (ret)
		return ret;

	clk_set_rate(&apss_ahb_clk_src.c, 19200000);
	clk_prepare_enable(&apss_ahb_clk_src.c);

	opp_dev_node = of_parse_phandle(pdev->dev.of_node, "qcom,dev-opp-list",
					node);
	while (opp_dev_node) {
		opp_dev = of_find_device_by_node(opp_dev_node);
		if (!opp_dev) {
			pr_err("cant find device for node\n");
			return -EINVAL;
		}
		register_opp_for_dev(opp_dev);
		node++;
		opp_dev_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,dev-opp-list", node);
	}

	dev_info(&pdev->dev, "Registered GCC clocks\n");

	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-8909" },
	{},
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_probe,
	.driver = {
		.name = "qcom,gcc-8909",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_gcc_init(void)
{
	return platform_driver_register(&msm_clock_gcc_driver);
}
arch_initcall(msm_gcc_init);

static struct clk_lookup msm_clocks_measure[] = {
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
	CLK_LIST(apss_debug_pri_mux),
	CLK_LIST(apc0_m_clk),
	CLK_LIST(apc1_m_clk),
	CLK_LIST(apc2_m_clk),
	CLK_LIST(apc3_m_clk),
	CLK_LIST(l2_m_clk),
};

static int msm_clock_debug_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

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

	gcc_debug_mux_ops = mux_reg_ops;
	gcc_debug_mux_ops.set_mux_sel = gcc_set_mux_sel;

	rpm_debug_clk.c.parent = clk_get(&pdev->dev, "rpm_debug_mux");
	if (IS_ERR(rpm_debug_clk.c.parent)) {
		dev_err(&pdev->dev, "Failed to get RPM debug Mux\n");
		return PTR_ERR(rpm_debug_clk.c.parent);
	}

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
	{ .compatible = "qcom,cc-debug-8909" },
	{}
};

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_probe,
	.driver = {
		.name = "qcom,cc-debug-8909",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_clock_debug_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}
late_initcall(msm_clock_debug_init);

/* MDSS DSI_PHY_PLL */
static struct clk_lookup msm_clocks_gcc_mdss[] = {
	CLK_LIST(byte0_clk_src),
	CLK_LIST(pclk0_clk_src),
	CLK_LIST(gcc_mdss_pclk0_clk),
	CLK_LIST(gcc_mdss_byte0_clk),
};

static int msm_gcc_mdss_probe(struct platform_device *pdev)
{
	int counter = 0, ret = 0;

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
	{ .compatible = "qcom,gcc-mdss-8909" },
	{}
};

static struct platform_driver msm_clock_gcc_mdss_driver = {
	.probe = msm_gcc_mdss_probe,
	.driver = {
		.name = "gcc-mdss-8909",
		.of_match_table = msm_clock_mdss_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_gcc_mdss_init(void)
{
	return platform_driver_register(&msm_clock_gcc_mdss_driver);
}
fs_initcall_sync(msm_gcc_mdss_init);

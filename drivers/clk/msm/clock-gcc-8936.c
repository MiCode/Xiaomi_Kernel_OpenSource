/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/clock-voter.h>

#include <linux/clk/msm-clock-generic.h>
#include <linux/regulator/rpm-smd-regulator.h>

#include <dt-bindings/clock/msm-clocks-8936.h>

#include "clock.h"

enum {
	GCC_BASE,
	APCS_C0_PLL_BASE,
	APCS_C1_PLL_BASE,
	APCS_CCI_PLL_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];
static void __iomem *virt_dbgbase;

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))

#define GPLL0_MODE					0x21000
#define GPLL0_L_VAL					0x21004
#define GPLL0_M_VAL					0x21008
#define GPLL0_N_VAL					0x2100C
#define GPLL0_USER_CTL					0x21010
#define GPLL0_CONFIG_CTL				0x21014
#define GPLL0_STATUS					0x2101C
#define GPLL1_MODE					0x20000
#define GPLL1_L_VAL					0x20004
#define GPLL1_M_VAL					0x20008
#define GPLL1_N_VAL					0x2000C
#define GPLL1_USER_CTL					0x20010
#define GPLL1_CONFIG_CTL				0x20014
#define GPLL1_STATUS					0x2001C
#define GPLL2_MODE					0x4A000
#define GPLL2_L_VAL					0x4A004
#define GPLL2_M_VAL					0x4A008
#define GPLL2_N_VAL					0x4A00C
#define GPLL2_USER_CTL					0x4A010
#define GPLL2_CONFIG_CTL				0x4A014
#define GPLL2_STATUS					0x4A01C
#define GPLL3_MODE					0x22000
#define GPLL3_L_VAL					0x22004
#define GPLL3_M_VAL					0x22008
#define GPLL3_N_VAL					0x2200C
#define GPLL3_USER_CTL					0x22010
#define GPLL3_CONFIG_CTL				0x22014
#define GPLL3_STATUS					0x2201C
#define GPLL4_MODE					0x24000
#define GPLL4_L_VAL					0x24004
#define GPLL4_M_VAL					0x24008
#define GPLL4_N_VAL					0x2400C
#define GPLL4_USER_CTL					0x24010
#define GPLL4_CONFIG_CTL				0x24014
#define GPLL4_STATUS					0x2401C
#define GPLL6_MODE					0x37000
#define GPLL6_L_VAL					0x37004
#define GPLL6_M_VAL					0x37008
#define GPLL6_N_VAL					0x3700C
#define GPLL6_USER_CTL					0x37010
#define GPLL6_CONFIG_CTL				0x37014
#define GPLL6_STATUS					0x3701C
#define MSS_CFG_AHB_CBCR				0x49000
#define MSS_Q6_BIMC_AXI_CBCR				0x49004
#define USB_HS_BCR					0x41000
#define USB_HS_SYSTEM_CBCR				0x41004
#define USB_HS_AHB_CBCR					0x41008
#define USB_HS_SYSTEM_CMD_RCGR				0x41010
#define USB_FS_BCR					0x3F000
#define USB_FS_SYSTEM_CMD_RCGR                          0x3F010
#define USB_FS_IC_CMD_RCGR				0x3F034
#define USB_FS_AHB_CBCR                                 0x3F008
#define USB_FS_IC_CBCR					0x3F030
#define USB_FS_SYSTEM_CBCR				0x3F004
#define USB2A_PHY_SLEEP_CBCR				0x4102C
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
#define JPEG_TBU_CBCR					0x12034
#define SMMU_CFG_CBCR					0x12038
#define VFE_TBU_CBCR					0x1203C
#define CPP_TBU_CBCR					0x12040
#define MDP_RT_TBU_CBCR					0x1204C
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
#define SPDM_JPEG0_CBCR					0x2F028
#define SPDM_MDP_CBCR					0x2F02C
#define SPDM_VCODEC0_CBCR				0x2F034
#define SPDM_VFE0_CBCR					0x2F038
#define SPDM_GFX3D_CBCR					0x2F03C
#define SPDM_PCLK0_CBCR					0x2F044
#define SPDM_CSI0_CBCR					0x2F048
#define VCODEC0_CMD_RCGR				0x4C000
#define VENUS0_BCR					0x4C014
#define VENUS0_VCODEC0_CBCR				0x4C01C
#define VENUS0_AHB_CBCR					0x4C020
#define VENUS0_AXI_CBCR					0x4C024
#define VENUS0_CORE0_VCODEC0_CBCR			0x4C02C
#define VENUS0_CORE1_VCODEC0_CBCR			0x4C034
#define PCLK0_CMD_RCGR					0x4D000
#define PCLK1_CMD_RCGR                                  0x4D0B8
#define MDP_CMD_RCGR					0x4D014
#define VSYNC_CMD_RCGR					0x4D02C
#define BYTE0_CMD_RCGR					0x4D044
#define BYTE1_CMD_RCGR                                  0x4D0B0
#define ESC0_CMD_RCGR					0x4D05C
#define ESC1_CMD_RCGR                                   0x4D0A8
#define MDSS_BCR					0x4D074
#define MDSS_AHB_CBCR					0x4D07C
#define MDSS_AXI_CBCR					0x4D080
#define MDSS_PCLK0_CBCR					0x4D084
#define MDSS_PCLK1_CBCR                                 0x4D0A4
#define MDSS_MDP_CBCR					0x4D088
#define MDSS_VSYNC_CBCR					0x4D090
#define MDSS_BYTE0_CBCR					0x4D094
#define MDSS_BYTE1_CBCR                                 0x4D0A0
#define MDSS_ESC0_CBCR					0x4D098
#define MDSS_ESC1_CBCR                                  0x4D09C
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
#define CSI2_CMD_RCGR					0x3C020
#define CAMSS_CSI2_CBCR					0x3C03C
#define CAMSS_CSI2_AHB_CBCR				0x3C040
#define CAMSS_CSI2PHY_CBCR				0x3C048
#define CAMSS_CSI2RDI_CBCR				0x3C050
#define CAMSS_CSI2PIX_CBCR				0x3C058
#define CAMSS_ISPIF_AHB_CBCR				0x50004
#define CCI_CMD_RCGR					0x51000
#define CAMSS_CCI_CBCR					0x51018
#define CAMSS_CCI_AHB_CBCR				0x5101C
#define MCLK0_CMD_RCGR					0x52000
#define CAMSS_MCLK0_CBCR				0x52018
#define MCLK1_CMD_RCGR					0x53000
#define CAMSS_MCLK1_CBCR				0x53018
#define MCLK2_CMD_RCGR                                  0x5C000
#define CAMSS_MCLK2_CBCR                                0x5C018
#define CAMSS_GP0_CMD_RCGR				0x54000
#define CAMSS_GP0_CBCR					0x54018
#define CAMSS_GP1_CMD_RCGR				0x55000
#define CAMSS_GP1_CBCR					0x55018
#define CAMSS_TOP_AHB_CBCR				0x5A014
#define CAMSS_AHB_CBCR					0x56004
#define CAMSS_MICRO_AHB_CBCR				0x5600C
#define CAMSS_MICRO_BCR					0x56008
#define JPEG0_CMD_RCGR					0x57000
#define CAMSS_JPEG0_BCR					0x57018
#define CAMSS_JPEG0_CBCR				0x57020
#define CAMSS_JPEG_AHB_CBCR				0x57024
#define CAMSS_JPEG_AXI_CBCR				0x57028
#define VFE0_CMD_RCGR					0x58000
#define CPP_CMD_RCGR					0x58018
#define CAMSS_VFE_BCR					0x58030
#define CAMSS_VFE0_CBCR					0x58038
#define CAMSS_CPP_CBCR					0x5803C
#define CAMSS_CPP_AHB_CBCR				0x58040
#define CAMSS_VFE_AHB_CBCR				0x58044
#define CAMSS_VFE_AXI_CBCR				0x58048
#define CAMSS_CSI_VFE0_BCR				0x5804C
#define CAMSS_CSI_VFE0_CBCR				0x58050
#define GFX3D_CMD_RCGR					0x59000
#define OXILI_GFX3D_CBCR				0x59020
#define OXILI_GMEM_CBCR					0x59024
#define OXILI_AHB_CBCR					0x59028
#define CAMSS_TOP_AHB_CMD_RCGR				0x5A000
#define BIMC_GFX_CBCR					0x31024
#define BIMC_GPU_CBCR					0x31040

#define APCS_CCI_PLL_MODE				0x00000
#define APCS_CCI_PLL_L_VAL				0x00004
#define APCS_CCI_PLL_M_VAL				0x00008
#define APCS_CCI_PLL_N_VAL				0x0000C
#define APCS_CCI_PLL_USER_CTL				0x00010
#define APCS_CCI_PLL_CONFIG_CTL				0x00014
#define APCS_CCI_PLL_STATUS				0x0001C

#define APCS_C0_PLL_MODE				0x00000
#define APCS_C0_PLL_L_VAL				0x00004
#define APCS_C0_PLL_M_VAL				0x00008
#define APCS_C0_PLL_N_VAL				0x0000C
#define APCS_C0_PLL_USER_CTL				0x00010
#define APCS_C0_PLL_CONFIG_CTL				0x00014
#define APCS_C0_PLL_STATUS				0x0001C

#define APCS_C1_PLL_MODE				0x00000
#define APCS_C1_PLL_L_VAL				0x00004
#define APCS_C1_PLL_M_VAL				0x00008
#define APCS_C1_PLL_N_VAL				0x0000C
#define APCS_C1_PLL_USER_CTL				0x00010
#define APCS_C1_PLL_CONFIG_CTL				0x00014
#define APCS_C1_PLL_STATUS				0x0001C

/* Mux source select values */
#define gcc_xo_source_val		0
#define xo_a_clk_source_val		0
#define gpll0_out_main_source_val	1
#define gpll0_out_aux_source_val	5
#define gpll0_misc_source_val		2
#define gpll1_out_main_source_val	1
#define gpll2_out_main_source_val	2
#define gpll2_out_aux_source_val	3
#define gpll2_gfx3d_source_val		4
#define gpll3_out_main_source_val	2
#define gpll3_out_aux_source_val	4
#define gpll4_out_main_source_val	2
#define gpll6_out_main_source_val	1
#define gpll6_mclk_source_val		3
#define dsi0_phypll_mm_source_val	1

#define FIXDIV(div) (div ? (2 * (div) - 1) : (0))

#define F(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)FIXDIV(div)) \
			| BVAL(10, 8, s##_source_val), \
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

#define F_MDSS(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)FIXDIV(div)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
	},					\
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig, \
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

DEFINE_EXT_CLK(gcc_xo, NULL);
DEFINE_EXT_CLK(xo_a_clk, NULL);
DEFINE_EXT_CLK(rpm_debug_clk, NULL);

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
	F_APCS_PLL(403200000, 21, 0x0, 0x1, 0x0, 0x0, 0x0),
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
		.parent = &xo_a_clk.c,
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
	F_APCS_PLL( 998400000,  52, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1113600000,  58, 0x0, 0x1, 0x0, 0x0, 0x0),
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
		.parent = &xo_a_clk.c,
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
	F_APCS_PLL( 652800000, 34, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 691200000, 36, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 729600000, 38, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 806400000, 42, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 844800000, 44, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 883200000, 46, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 960000000, 50, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL( 998400000, 52, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1036800000, 54, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1113600000, 58, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1190400000, 62, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1267200000, 66, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1344000000, 70, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1420800000, 74, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1497600000, 78, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1536000000, 80, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1574400000, 82, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1612800000, 84, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1632000000, 85, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1651200000, 86, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1689600000, 88, 0x0, 0x1, 0x0, 0x0, 0x0),
	F_APCS_PLL(1708800000, 89, 0x0, 0x1, 0x0, 0x0, 0x0),
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
		.parent = &xo_a_clk.c,
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

static struct pll_vote_clk gpll0 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_xo.c,
		.rate = 800000000,
		.dbg_name = "gpll0",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0.c),
	},
};

/* Don't vote for xo if using this clock to allow xo shutdown */
static struct pll_vote_clk gpll0_ao = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_a_clk.c,
		.rate = 800000000,
		.dbg_name = "gpll0_ao",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao.c),
	},
};

DEFINE_EXT_CLK(gpll0_out_main, &gpll0.c);
DEFINE_EXT_CLK(gpll0_out_aux, &gpll0.c);
DEFINE_EXT_CLK(gpll0_misc, &gpll0.c);

static struct pll_vote_clk gpll1 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)GPLL1_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_xo.c,
		.rate = 614400000,
		.dbg_name = "gpll1",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll1.c),
	},
};

DEFINE_EXT_CLK(gpll1_out_main, &gpll1.c);

static struct pll_vote_clk gpll2 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(2),
	.status_reg = (void __iomem *)GPLL2_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_xo.c,
		.rate = 930000000,
		.dbg_name = "gpll2",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll2.c),
	},
};

DEFINE_EXT_CLK(gpll2_out_main, &gpll2.c);
DEFINE_EXT_CLK(gpll2_out_aux, &gpll2.c);
DEFINE_EXT_CLK(gpll2_gfx3d, &gpll2.c);

static struct pll_vote_clk gpll6 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(7),
	.status_reg = (void __iomem *)GPLL6_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_xo.c,
		.rate = 1080000000,
		.dbg_name = "gpll6",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll6.c),
	},
};

DEFINE_EXT_CLK(gpll6_out_main, &gpll6.c);
DEFINE_EXT_CLK(gpll6_mclk, &gpll6.c);

static struct pll_vote_clk gpll3 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(4),
	.status_reg = (void __iomem *)GPLL3_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_xo.c,
		.rate = 1100000000,
		.dbg_name = "gpll3",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll3.c),
	},
};

DEFINE_EXT_CLK(gpll3_out_main, &gpll3.c);
DEFINE_EXT_CLK(gpll3_out_aux, &gpll3.c);

static struct pll_vote_clk gpll4 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(5),
	.status_reg = (void __iomem *)GPLL4_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_xo.c,
		.rate = 1200000000,
		.dbg_name = "gpll4",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll4.c),
	},
};

DEFINE_EXT_CLK(gpll4_out_main, &gpll4.c);

static struct clk_freq_tbl ftbl_apss_ahb_clk[] = {
	F(  19200000,	      xo_a_clk,   1,	  0,	0),
	F(  50000000,	   gpll0_out_main,  16,	  0,	0),
	F(  100000000,	   gpll0_out_main,   8,	  0,	0),
	F(  133330000,	   gpll0_out_main,   6,	  0,	0),
	F_END
};

static struct rcg_clk apss_ahb_clk_src = {
	.cmd_rcgr_reg = APSS_AHB_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_apss_ahb_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "apss_ahb_clk_src",
		.ops = &clk_ops_rcg,
		CLK_INIT(apss_ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_top_ahb_clk[] = {
	F(  40000000,	   gpll0_out_main,  10,	  1,	2),
	F(  80000000,	   gpll0_out_main,  10,	  0,	0),
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
		VDD_DIG_FMAX_MAP2(LOW, 40000000, NOMINAL, 80000000),
		CLK_INIT(camss_top_ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_csi0_1_2_clk[] = {
	F( 100000000,	   gpll0_out_main,   8,	  0,	0),
	F( 200000000,	   gpll0_out_main,   4,	  0,	0),
	F_END
};

static struct rcg_clk csi0_clk_src = {
	.cmd_rcgr_reg = CSI0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_camss_csi0_1_2_clk,
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
	.freq_tbl = ftbl_gcc_camss_csi0_1_2_clk,
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
	.freq_tbl = ftbl_gcc_camss_csi0_1_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "csi2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_vfe0_clk[] = {
	F(  50000000,	   gpll0_out_main,  16,	  0,	0),
	F(  80000000,	   gpll0_out_main,  10,	  0,	0),
	F( 100000000,	   gpll0_out_main,   8,	  0,	0),
	F( 160000000,	   gpll0_out_main,   5,	  0,	0),
	F( 177780000,	   gpll0_out_main, 4.5,	  0,	0),
	F( 200000000,	   gpll0_out_main,   4,	  0,	0),
	F( 266670000,	   gpll0_out_main,   3,	  0,	0),
	F( 320000000,	   gpll0_out_main, 2.5,	  0,	0),
	F( 400000000,	   gpll0_out_main,   2,	  0,	0),
	F( 465000000,	   gpll2_out_aux,   2,	  0,	0),
	F( 480000000,      gpll4_out_main, 2.5,    0,    0),
	F( 600000000,      gpll4_out_main,   2,    0,    0),
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
		VDD_DIG_FMAX_MAP3(LOW, 160000000, NOMINAL, 320000000, HIGH,
			600000000),
		CLK_INIT(vfe0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_oxili_gfx3d_clk[] = {
	F(  19200000,	      gcc_xo,   1,	  0,	0),
	F(  50000000,	   gpll0_out_main,  16,	  0,	0),
	F(  80000000,      gpll0_out_main,  10,	  0,	0),
	F( 100000000,      gpll0_out_main,   8,	  0,	0),
	F( 160000000,      gpll0_out_main,   5,	  0,	0),
	F( 200000000,      gpll0_out_main,   4,	  0,	0),
	F( 220000000,      gpll3_out_main,   5,	  0,	0),
	F( 266670000,      gpll0_out_main,   3,	  0,	0),
	F( 310000000,	gpll2_gfx3d,	3,	  0,	0),
	F( 400000000,      gpll0_out_main,   2,	  0,	0),
	F( 550000000,      gpll3_out_main,   2,    0,    0),
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
		VDD_DIG_FMAX_MAP3(LOW, 220000000, NOMINAL, 400000000, HIGH,
			550000000),
		CLK_INIT(gfx3d_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_6_i2c_apps_clk[] = {
	F(  19200000,	      gcc_xo,   1,	  0,	0),
	F(  50000000,	   gpll0_out_main,  16,	  0,	0),
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
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_qup1_6_spi_apps_clk[] = {
	F(    960000,	      gcc_xo,  10,	  1,	2),
	F(   4800000,	      gcc_xo,   4,	  0,	0),
	F(   9600000,	      gcc_xo,   2,	  0,	0),
	F(  16000000,	   gpll0_out_main,  10,	  1,	5),
	F(  19200000,	      gcc_xo,   1,	  0,	0),
	F(  25000000,	   gpll0_out_main,  16,	  1,	2),
	F(  50000000,	   gpll0_out_main,  16,	  0,	0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_i2c_apps_clk,
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
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
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
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
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
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_uart1_6_apps_clk[] = {
	F(   3686400,	   gpll0_out_main,   1,	 72, 15625),
	F(   7372800,	   gpll0_out_main,   1,	144, 15625),
	F(  14745600,	   gpll0_out_main,   1,	288, 15625),
	F(  16000000,	   gpll0_out_main,  10,	  1,	5),
	F(  19200000,	      gcc_xo,   1,	  0,	0),
	F(  24000000,	   gpll0_out_main,   1,	  3,  100),
	F(  25000000,	   gpll0_out_main,  16,	  1,	2),
	F(  32000000,	   gpll0_out_main,   1,	  1,   25),
	F(  40000000,	   gpll0_out_main,   1,	  1,   20),
	F(  46400000,	   gpll0_out_main,   1,	 29,  500),
	F(  48000000,	   gpll0_out_main,   1,	  3,   50),
	F(  51200000,	   gpll0_out_main,   1,	  8,  125),
	F(  56000000,	   gpll0_out_main,   1,	  7,  100),
	F(  58982400,	   gpll0_out_main,   1, 1152, 15625),
	F(  60000000,	   gpll0_out_main,   1,	  3,   40),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
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
	.cmd_rcgr_reg = BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 32000000, NOMINAL, 64000000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_cci_clk[] = {
	F(  19200000,	      gcc_xo,   1,	  0,	0),
	F(  37500000,         gpll0_misc,   1,    3,    64),
	F_END
};

static struct rcg_clk cci_clk_src = {
	.cmd_rcgr_reg = CCI_CMD_RCGR,
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

static struct clk_freq_tbl ftbl_gcc_camss_gp0_1_clk[] = {
	F( 100000000,	   gpll0_out_main,   8,	  0,	0),
	F( 200000000,	   gpll0_out_main,   4,	  0,	0),
	F_END
};

static struct rcg_clk camss_gp0_clk_src = {
	.cmd_rcgr_reg = CAMSS_GP0_CMD_RCGR,
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
	.cmd_rcgr_reg = CAMSS_GP1_CMD_RCGR,
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
	F( 133330000,	   gpll0_out_main,   6,	  0,	0),
	F( 266670000,	   gpll0_out_main,   3,	  0,	0),
	F( 320000000,	   gpll0_out_main, 2.5,	  0,	0),
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
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000, HIGH,
			320000000),
		CLK_INIT(jpeg0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_mclk0_1_2_clk[] = {
	F(  24000000,      gpll6_mclk,  1,   1,    45),
	F(  66670000,	   gpll0_out_main,  12,	  0,	0),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg = MCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_mclk0_1_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "mclk0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 24000000, NOMINAL, 66670000),
		CLK_INIT(mclk0_clk_src.c),
	},
};

static struct rcg_clk mclk1_clk_src = {
	.cmd_rcgr_reg = MCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_mclk0_1_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_camss_mclk2_clk[] = {
	F(  66670000,	   gpll0_out_main,  12,	  0,	0),
	F_END
};

static struct rcg_clk mclk2_clk_src = {
	.cmd_rcgr_reg = MCLK2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_camss_mclk2_clk,
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
	F( 100000000,	   gpll0_out_main,   8,	  0,	0),
	F( 200000000,	   gpll0_out_main,   4,	  0,	0),
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

static struct clk_freq_tbl ftbl_gcc_camss_cpp_clk[] = {
	F( 160000000,	   gpll0_out_main,   5,	  0,	0),
	F( 200000000,      gpll0_out_main,   4,   0,    0),
	F( 228570000,      gpll0_out_main, 3.5,   0,    0),
	F( 266670000,      gpll0_out_main,   3,   0,    0),
	F( 320000000,	   gpll0_out_main, 2.5,	  0,	0),
	F( 465000000,	   gpll2_out_main,   2,	  0,	0),
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
		VDD_DIG_FMAX_MAP3(LOW, 160000000, NOMINAL, 320000000, HIGH,
			465000000),
		CLK_INIT(cpp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gp1_3_clk[] = {
	F(  19200000,	      gcc_xo,   1,	  0,	0),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp1_clk_src.c),
	},
};

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg = GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp2_clk_src.c),
	},
};

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg = GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_byte0_clk[] = {
	{
		.div_src_val = BVAL(10, 8, dsi0_phypll_mm_source_val),
	},
	F_END
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

static struct clk_freq_tbl ftbl_gcc_mdss_byte1_clk[] = {
	{
		.div_src_val = BVAL(10, 8, dsi0_phypll_mm_source_val),
	},
	F_END
};

static struct rcg_clk byte1_clk_src = {
	.cmd_rcgr_reg = BYTE1_CMD_RCGR,
	.current_freq = ftbl_gcc_mdss_byte1_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "byte1_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP2(LOW, 112500000, NOMINAL, 187500000),
		CLK_INIT(byte1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_esc0_1_clk[] = {
	F(  19200000,	      gcc_xo,   1,	  0,	0),
	F_END
};

static struct rcg_clk esc0_clk_src = {
	.cmd_rcgr_reg = ESC0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_mdss_esc0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "esc0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(esc0_clk_src.c),
	},
};

static struct rcg_clk esc1_clk_src = {
	.cmd_rcgr_reg = ESC1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_mdss_esc0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "esc1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(esc1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_mdp_clk[] = {
	F(  50000000,	   gpll0_out_aux,  16,	  0,	0),
	F(  80000000,	   gpll0_out_aux,  10,	  0,	0),
	F( 100000000,	   gpll0_out_aux,   8,	  0,	0),
	F( 145500000,	   gpll0_out_aux,  5.5,   0,    0),
	F( 153600000,	   gpll1_out_main,	4,	0,	0),
	F( 160000000,	   gpll0_out_aux,   5,	  0,	0),
	F( 177780000,	   gpll0_out_aux, 4.5,	  0,	0),
	F( 200000000,	   gpll0_out_aux,   4,	  0,	0),
	F( 266670000,	   gpll0_out_aux,   3,	  0,	0),
	F( 307200000,	   gpll1_out_main,	2,	0,	0),
	F( 366670000,      gpll3_out_aux,   3,        0,    0),
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
		VDD_DIG_FMAX_MAP3(LOW, 153600000, NOMINAL, 307200000, HIGH,
			366670000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_pclk0_clk[] = {
	{
		.div_src_val = BVAL(10, 8, dsi0_phypll_mm_source_val)
					| BVAL(4, 0, 0),
	},
	F_END
};

static struct rcg_clk pclk0_clk_src = {
	.cmd_rcgr_reg = PCLK0_CMD_RCGR,
	.current_freq = ftbl_gcc_mdss_pclk0_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pclk0_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 250000000),
		CLK_INIT(pclk0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_pclk1_clk[] = {
	{
		.div_src_val = BVAL(10, 8, dsi0_phypll_mm_source_val)
						| BVAL(4, 0, 0),
	},
	F_END
};
static struct rcg_clk pclk1_clk_src = {
	.cmd_rcgr_reg = PCLK1_CMD_RCGR,
	.current_freq = ftbl_gcc_mdss_pclk1_clk,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pclk1_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 250000000),
		CLK_INIT(pclk1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_mdss_vsync_clk[] = {
	F(  19200000,	      gcc_xo,   1,	  0,	0),
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
	F(  64000000,	   gpll0_out_main, 12.5,    0,	 0),
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
	F(  144000,	gcc_xo,	16,	3,	25),
	F(    400000,	      gcc_xo,       12,	  1,	4),
	F(  20000000,	   gpll0_out_main,  10,	  1,	4),
	F(  25000000,	   gpll0_out_main,  16,	  1,	2),
	F(  50000000,	   gpll0_out_main,  16,	  0,	0),
	F( 100000000,	   gpll0_out_main,   8,	  0,	0),
	F( 177770000,      gpll0_out_main, 4.5,    0,    0),
	F( 200000000,	   gpll0_out_main,   4,	  0,	0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg = SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 200000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg = SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F(  57140000,      gpll0_out_main,  14,    0,    0),
	F(  80000000,	   gpll0_out_main,  10,	  0,	0),
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
		VDD_DIG_FMAX_MAP2(LOW, 57140000, NOMINAL, 80000000),
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_ic_clk[] = {
	F(  60000000,	   gpll6_out_main,  1,	  1,	18),
	F_END
};

static struct rcg_clk usb_fs_ic_clk_src = {
	.cmd_rcgr_reg = USB_FS_IC_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_usb_ic_clk,
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
	F(  64000000,	   gpll0_misc,  12.5,	  0,	0),
	F_END
};

static struct rcg_clk usb_fs_system_clk_src = {
	.cmd_rcgr_reg = USB_FS_SYSTEM_CMD_RCGR,
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

static struct clk_freq_tbl ftbl_gcc_venus0_vcodec0_clk[] = {
	F( 133330000,	   gpll0_out_main,   6,	  0,	0),
	F( 200000000,	   gpll0_out_main,   4,	  0,	0),
	F( 266670000,	   gpll0_out_main,   3,	  0,	0),
	F_END
};

static struct rcg_clk vcodec0_clk_src = {
	.cmd_rcgr_reg = VCODEC0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_venus0_vcodec0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "vcodec0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 200000000, HIGH,
			266670000),
		CLK_INIT(vcodec0_clk_src.c),
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
	.bcr_reg  = CAMSS_CSI_VFE0_BCR,
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

static struct branch_clk gcc_camss_jpeg0_clk = {
	.cbcr_reg = CAMSS_JPEG0_CBCR,
	.bcr_reg = CAMSS_JPEG0_BCR,
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

static struct branch_clk gcc_camss_vfe0_clk = {
	.cbcr_reg = CAMSS_VFE0_CBCR,
	.bcr_reg = CAMSS_VFE_BCR,
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

static struct clk_freq_tbl ftbl_gcc_crypto_clk[] = {
	F(  50000000,	   gpll0_out_main,   16,	   0,	 0),
	F(  80000000,	   gpll0_out_main,   10,	   0,	 0),
	F( 100000000,	   gpll0_out_main,    8,	   0,	 0),
	F( 160000000,	   gpll0_out_main,    5,	   0,	 0),
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
		.parent = &crypto_clk_src.c,
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

static struct branch_clk gcc_mdss_byte1_clk = {
	.cbcr_reg = MDSS_BYTE1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_byte1_clk",
		.parent = &byte1_clk_src.c,
		.ops = &clk_ops_branch,
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
	.bcr_reg = MDSS_BCR,
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

static struct branch_clk gcc_mdss_pclk1_clk = {
	.cbcr_reg = MDSS_PCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdss_pclk1_clk",
		.parent = &pclk1_clk_src.c,
		.ops = &clk_ops_branch,
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

static struct local_vote_clk gcc_mdp_rt_tbu_clk = {
	.cbcr_reg = MDP_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(15),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mdp_rt_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_mdp_rt_tbu_clk.c),
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs_system_clk",
		.parent = &usb_hs_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_system_clk.c),
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

/* GPLL3 at 1100 MHz, main output enabled. */
static struct pll_config gpll3_config = {
	.l = 57,
	.m =  7,
	.n = 24,
	.vco_val = 0x0,
	.vco_mask = BIT(20),
	.pre_div_val = 0x0,
	.pre_div_mask = BIT(12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
	.aux_output_val = BIT(1),
	.aux_output_mask = BIT(1),
};

static struct pll_config_regs gpll3_regs = {
	.l_reg = (void __iomem *)GPLL3_L_VAL,
	.m_reg = (void __iomem *)GPLL3_M_VAL,
	.n_reg = (void __iomem *)GPLL3_N_VAL,
	.config_reg = (void __iomem *)GPLL3_USER_CTL,
	.mode_reg = (void __iomem *)GPLL3_MODE,
	.base = &virt_bases[GCC_BASE],
};

/* GPLL4 at 1200 MHz, main output enabled. */
static struct pll_config gpll4_config = {
	.l = 62,
	.m =  1,
	.n =  2,
	.vco_val = 0x0,
	.vco_mask = BIT(20),
	.pre_div_val = 0x0,
	.pre_div_mask = BIT(12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs gpll4_regs = {
	.l_reg = (void __iomem *)GPLL4_L_VAL,
	.m_reg = (void __iomem *)GPLL4_M_VAL,
	.n_reg = (void __iomem *)GPLL4_N_VAL,
	.config_reg = (void __iomem *)GPLL4_USER_CTL,
	.mode_reg = (void __iomem *)GPLL4_MODE,
	.base = &virt_bases[GCC_BASE],
};

static struct mux_clk gcc_debug_mux;
static struct clk_ops clk_ops_debug_mux;

static struct measure_clk_data debug_mux_priv = {
	.cxo = &gcc_xo.c,
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
	.en_mask = BIT(16),
	.mask = 0x1FF,
	.base = &virt_dbgbase,
	MUX_REC_SRC_LIST(
		&rpm_debug_clk.c,
	),
	MUX_SRC_LIST(
		{&rpm_debug_clk.c,			0xFFFF},
		{&gcc_gp1_clk.c,			0x0010},
		{&gcc_gp2_clk.c,			0x0011},
		{&gcc_gp3_clk.c,			0x0012},
		{&gcc_bimc_gfx_clk.c,			0x002d},
		{&gcc_mss_cfg_ahb_clk.c,		0x0030},
		{&gcc_mss_q6_bimc_axi_clk.c,		0x0031},
		{&gcc_apss_tcu_clk.c,			0x0050},
		{&gcc_mdp_tbu_clk.c,			0x0051},
		{&gcc_gfx_tbu_clk.c,			0x0052},
		{&gcc_gfx_tcu_clk.c,			0x0053},
		{&gcc_venus_tbu_clk.c,			0x0054},
		{&gcc_gtcu_ahb_clk.c,			0x0058},
		{&gcc_vfe_tbu_clk.c,			0x005a},
		{&gcc_smmu_cfg_clk.c,			0x005b},
		{&gcc_jpeg_tbu_clk.c,			0x005c},
		{&gcc_usb_hs_system_clk.c,		0x0060},
		{&gcc_usb_hs_ahb_clk.c,			0x0061},
		{&gcc_usb_fs_ahb_clk.c,			0x00f1},
		{&gcc_usb_fs_ic_clk.c,			0x00f4},
		{&gcc_usb2a_phy_sleep_clk.c,		0x0063},
		{&gcc_sdcc1_apps_clk.c,			0x0068},
		{&gcc_sdcc1_ahb_clk.c,			0x0069},
		{&gcc_sdcc2_apps_clk.c,			0x0070},
		{&gcc_sdcc2_ahb_clk.c,			0x0071},
		{&gcc_blsp1_ahb_clk.c,			0x0088},
		{&gcc_blsp1_qup1_spi_apps_clk.c,	0x008a},
		{&gcc_blsp1_qup1_i2c_apps_clk.c,	0x008b},
		{&gcc_blsp1_uart1_apps_clk.c,		0x008c},
		{&gcc_blsp1_qup2_spi_apps_clk.c,	0x008e},
		{&gcc_blsp1_qup2_i2c_apps_clk.c,	0x0090},
		{&gcc_blsp1_uart2_apps_clk.c,		0x0091},
		{&gcc_blsp1_qup3_spi_apps_clk.c,	0x0093},
		{&gcc_blsp1_qup3_i2c_apps_clk.c,	0x0094},
		{&gcc_blsp1_qup4_spi_apps_clk.c,	0x0098},
		{&gcc_blsp1_qup4_i2c_apps_clk.c,	0x0099},
		{&gcc_blsp1_qup5_spi_apps_clk.c,	0x009c},
		{&gcc_blsp1_qup5_i2c_apps_clk.c,	0x009d},
		{&gcc_blsp1_qup6_spi_apps_clk.c,	0x00a1},
		{&gcc_blsp1_qup6_i2c_apps_clk.c,	0x00a2},
		{&gcc_camss_ahb_clk.c,			0x00a8},
		{&gcc_camss_top_ahb_clk.c,		0x00a9},
		{&gcc_camss_micro_ahb_clk.c,		0x00aa},
		{&gcc_camss_gp0_clk.c,			0x00ab},
		{&gcc_camss_gp1_clk.c,			0x00ac},
		{&gcc_camss_mclk0_clk.c,		0x00ad},
		{&gcc_camss_mclk1_clk.c,		0x00ae},
		{&gcc_camss_mclk2_clk.c,		0x01bd},
		{&gcc_camss_cci_clk.c,			0x00af},
		{&gcc_camss_cci_ahb_clk.c,		0x00b0},
		{&gcc_camss_csi0phytimer_clk.c,		0x00b1},
		{&gcc_camss_csi1phytimer_clk.c,		0x00b2},
		{&gcc_camss_jpeg0_clk.c,		0x00b3},
		{&gcc_camss_jpeg_ahb_clk.c,		0x00b4},
		{&gcc_camss_jpeg_axi_clk.c,		0x00b5},
		{&gcc_camss_vfe0_clk.c,			0x00b8},
		{&gcc_camss_cpp_clk.c,			0x00b9},
		{&gcc_camss_cpp_ahb_clk.c,		0x00ba},
		{&gcc_camss_vfe_ahb_clk.c,		0x00bb},
		{&gcc_camss_vfe_axi_clk.c,		0x00bc},
		{&gcc_camss_csi_vfe0_clk.c,		0x00bf},
		{&gcc_camss_csi0_clk.c,			0x00c0},
		{&gcc_camss_csi0_ahb_clk.c,		0x00c1},
		{&gcc_camss_csi0phy_clk.c,		0x00c2},
		{&gcc_camss_csi0rdi_clk.c,		0x00c3},
		{&gcc_camss_csi0pix_clk.c,		0x00c4},
		{&gcc_camss_csi1_clk.c,			0x00c5},
		{&gcc_camss_csi1_ahb_clk.c,		0x00c6},
		{&gcc_camss_csi1phy_clk.c,		0x00c7},
		{&gcc_camss_csi2_clk.c,			0x00e3},
		{&gcc_camss_csi2_ahb_clk.c,		0x00e4},
		{&gcc_camss_csi2phy_clk.c,		0x00e5},
		{&gcc_camss_csi2rdi_clk.c,		0x00e6},
		{&gcc_camss_csi2pix_clk.c,		0x00e7},
		{&gcc_pdm_ahb_clk.c,			0x00d0},
		{&gcc_pdm2_clk.c,			0x00d2},
		{&gcc_prng_ahb_clk.c,			0x00d8},
		{&gcc_camss_csi1rdi_clk.c,		0x00e0},
		{&gcc_camss_csi1pix_clk.c,		0x00e1},
		{&gcc_camss_ispif_ahb_clk.c,		0x00e2},
		{&gcc_boot_rom_ahb_clk.c,		0x00f8},
		{&gcc_crypto_clk.c,			0x0138},
		{&gcc_crypto_axi_clk.c,			0x0139},
		{&gcc_crypto_ahb_clk.c,			0x013a},
		{&gcc_oxili_gfx3d_clk.c,		0x01ea},
		{&gcc_oxili_ahb_clk.c,			0x01eb},
		{&gcc_oxili_gmem_clk.c,			0x01f0},
		{&gcc_venus0_vcodec0_clk.c,		0x01f1},
		{&gcc_venus0_core0_vcodec0_clk.c,	0x01b8},
		{&gcc_venus0_core1_vcodec0_clk.c,	0x01b9},
		{&gcc_venus0_axi_clk.c,			0x01f2},
		{&gcc_venus0_ahb_clk.c,			0x01f3},
		{&gcc_mdss_ahb_clk.c,			0x01f6},
		{&gcc_mdss_axi_clk.c,			0x01f7},
		{&gcc_mdss_pclk0_clk.c,			0x01f8},
		{&gcc_mdss_pclk1_clk.c,			0x01ba},
		{&gcc_mdss_mdp_clk.c,			0x01f9},
		{&gcc_mdss_vsync_clk.c,			0x01fb},
		{&gcc_mdss_byte0_clk.c,			0x01fc},
		{&gcc_mdss_byte1_clk.c,			0x01bb},
		{&gcc_mdss_esc0_clk.c,			0x01fd},
		{&gcc_mdss_esc1_clk.c,			0x01bc},
		{&gcc_bimc_gpu_clk.c,			0x0157},
		{&gcc_cpp_tbu_clk.c,			0x00e9},
		{&gcc_mdp_rt_tbu_clk.c,			0x00ee},
		{&wcnss_m_clk.c,			0x0198},
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
	CLK_LIST(gcc_xo),
	CLK_LIST(xo_a_clk),
	CLK_LIST(gpll0),
	CLK_LIST(gpll0_ao),
	CLK_LIST(gpll0_out_main),
	CLK_LIST(gpll0_out_aux),
	CLK_LIST(gpll0_misc),
	CLK_LIST(gpll1),
	CLK_LIST(gpll1_out_main),
	CLK_LIST(gpll2),
	CLK_LIST(gpll2_out_main),
	CLK_LIST(gpll2_out_aux),
	CLK_LIST(gpll3),
	CLK_LIST(gpll3_out_main),
	CLK_LIST(gpll3_out_aux),
	CLK_LIST(gpll4),
	CLK_LIST(gpll4_out_main),
	CLK_LIST(gpll6),
	CLK_LIST(gpll6_out_main),
	CLK_LIST(a53ss_c0_pll),
	CLK_LIST(a53ss_c1_pll),
	CLK_LIST(a53ss_cci_pll),

	/* RCGs */
	CLK_LIST(apss_ahb_clk_src),
	CLK_LIST(camss_top_ahb_clk_src),
	CLK_LIST(csi0_clk_src),
	CLK_LIST(csi1_clk_src),
	CLK_LIST(csi2_clk_src),
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
	CLK_LIST(cci_clk_src),
	CLK_LIST(camss_gp0_clk_src),
	CLK_LIST(camss_gp1_clk_src),
	CLK_LIST(jpeg0_clk_src),
	CLK_LIST(mclk0_clk_src),
	CLK_LIST(mclk1_clk_src),
	CLK_LIST(mclk2_clk_src),
	CLK_LIST(csi0phytimer_clk_src),
	CLK_LIST(csi1phytimer_clk_src),
	CLK_LIST(cpp_clk_src),
	CLK_LIST(gp1_clk_src),
	CLK_LIST(gp2_clk_src),
	CLK_LIST(gp3_clk_src),
	CLK_LIST(esc0_clk_src),
	CLK_LIST(esc1_clk_src),
	CLK_LIST(vsync_clk_src),
	CLK_LIST(pdm2_clk_src),
	CLK_LIST(sdcc1_apps_clk_src),
	CLK_LIST(sdcc2_apps_clk_src),
	CLK_LIST(usb_hs_system_clk_src),
	CLK_LIST(usb_fs_system_clk_src),
	CLK_LIST(usb_fs_ic_clk_src),
	CLK_LIST(vcodec0_clk_src),

	/* Voteable Clocks */
	CLK_LIST(gcc_blsp1_ahb_clk),
	CLK_LIST(gcc_boot_rom_ahb_clk),
	CLK_LIST(gcc_prng_ahb_clk),
	CLK_LIST(gcc_apss_tcu_clk),
	CLK_LIST(gcc_gfx_tbu_clk),
	CLK_LIST(gcc_gfx_tcu_clk),
	CLK_LIST(gcc_gtcu_ahb_clk),
	CLK_LIST(gcc_jpeg_tbu_clk),
	CLK_LIST(gcc_mdp_tbu_clk),
	CLK_LIST(gcc_smmu_cfg_clk),
	CLK_LIST(gcc_venus_tbu_clk),
	CLK_LIST(gcc_vfe_tbu_clk),
	CLK_LIST(gcc_cpp_tbu_clk),
	CLK_LIST(gcc_mdp_rt_tbu_clk),

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
	CLK_LIST(gcc_camss_cci_ahb_clk),
	CLK_LIST(gcc_camss_cci_clk),
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
	CLK_LIST(gcc_camss_cpp_ahb_clk),
	CLK_LIST(gcc_camss_cpp_clk),
	CLK_LIST(gcc_camss_vfe0_clk),
	CLK_LIST(gcc_camss_vfe_ahb_clk),
	CLK_LIST(gcc_camss_vfe_axi_clk),
	CLK_LIST(gcc_oxili_gmem_clk),
	CLK_LIST(gcc_gp1_clk),
	CLK_LIST(gcc_gp2_clk),
	CLK_LIST(gcc_gp3_clk),
	CLK_LIST(gcc_mdss_ahb_clk),
	CLK_LIST(gcc_mdss_axi_clk),
	CLK_LIST(gcc_mdss_esc0_clk),
	CLK_LIST(gcc_mdss_esc1_clk),
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
	CLK_LIST(gcc_usb_fs_ahb_clk),
	CLK_LIST(gcc_usb_fs_ic_clk),
	CLK_LIST(gcc_usb_fs_system_clk),
	CLK_LIST(gcc_venus0_ahb_clk),
	CLK_LIST(gcc_venus0_axi_clk),
	CLK_LIST(gcc_venus0_vcodec0_clk),
	CLK_LIST(gcc_venus0_core0_vcodec0_clk),
	CLK_LIST(gcc_venus0_core1_vcodec0_clk),
	CLK_LIST(gcc_bimc_gfx_clk),
	CLK_LIST(gcc_bimc_gpu_clk),
	CLK_LIST(wcnss_m_clk),

	/* Crypto clocks */
	CLK_LIST(gcc_crypto_clk),
	CLK_LIST(gcc_crypto_ahb_clk),
	CLK_LIST(gcc_crypto_axi_clk),
	CLK_LIST(crypto_clk_src),
};

/* Please note that the order of reg-names is important */
static int get_memory(struct platform_device *pdev)
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
	struct clk *tmp_clk;
	int ret;
	u32 regval;

	ret = get_memory(pdev);
	if (ret)
		return ret;

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (PTR_ERR(vdd_dig.regulator[0]) != -EPROBE_DEFER)
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

	tmp_clk = gcc_xo.c.parent = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(tmp_clk)) {
		if (PTR_ERR(tmp_clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo clock!!!\n");
		return PTR_ERR(tmp_clk);
	}

	tmp_clk = xo_a_clk.c.parent = devm_clk_get(&pdev->dev, "xo_a");
	if (IS_ERR(tmp_clk)) {
		if (PTR_ERR(tmp_clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo_a clock!!!\n");
		return PTR_ERR(tmp_clk);
	}

	/* Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));

	configure_sr_hpm_lp_pll(&gpll3_config, &gpll3_regs, 1);
	configure_sr_hpm_lp_pll(&gpll4_config, &gpll4_regs, 1);

	ret = of_msm_clock_register(pdev->dev.of_node,
				msm_clocks_lookup,
				ARRAY_SIZE(msm_clocks_lookup));
	if (ret)
		return ret;

	clk_set_rate(&apss_ahb_clk_src.c, 19200000);
	clk_prepare_enable(&apss_ahb_clk_src.c);

	dev_info(&pdev->dev, "Registered GCC clocks\n");

	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-8936" },
	{}
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_probe,
	.driver = {
		.name = "qcom,gcc-8936",
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
};

static int msm_clock_debug_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

	gcc_debug_mux_ops = mux_reg_ops;
	gcc_debug_mux_ops.set_mux_sel = gcc_set_mux_sel;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get CC base.\n");
		return -EINVAL;
	}

	virt_dbgbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_dbgbase) {
		dev_err(&pdev->dev, "Failed to map in CC registers.\n");
		return -ENOMEM;
	}

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
	{ .compatible = "qcom,cc-debug-8936" },
	{}
};

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_probe,
	.driver = {
		.name = "qcom,cc-debug-8936",
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
	CLK_LIST(byte1_clk_src),
	CLK_LIST(pclk0_clk_src),
	CLK_LIST(pclk1_clk_src),
	CLK_LIST(gcc_mdss_pclk0_clk),
	CLK_LIST(gcc_mdss_pclk1_clk),
	CLK_LIST(gcc_mdss_byte0_clk),
	CLK_LIST(gcc_mdss_byte1_clk),
};

static int msm_gcc_mdss_probe(struct platform_device *pdev)
{
	int counter = 0, ret = 0;
	struct clk *curr_p;

	curr_p = pclk0_clk_src.c.parent = devm_clk_get(&pdev->dev, "pclk0_src");
	if (IS_ERR(curr_p)) {
		dev_err(&pdev->dev, "Failed to get pclk0 source.\n");
		return PTR_ERR(curr_p);
	}

	for (counter = 0; counter < (sizeof(ftbl_gcc_mdss_pclk0_clk)/
				sizeof(struct clk_freq_tbl)); counter++)
		ftbl_gcc_mdss_pclk0_clk[counter].src_clk = curr_p;

	curr_p = pclk1_clk_src.c.parent = devm_clk_get(&pdev->dev, "pclk1_src");
	if (IS_ERR(curr_p)) {
		dev_err(&pdev->dev, "Failed to get pclk1 source.\n");
		ret = PTR_ERR(curr_p);
		goto pclk1_fail;
	}

	for (counter = 0; counter < (sizeof(ftbl_gcc_mdss_pclk1_clk)/
				sizeof(struct clk_freq_tbl)); counter++)
		ftbl_gcc_mdss_pclk1_clk[counter].src_clk = curr_p;

	curr_p = byte0_clk_src.c.parent = devm_clk_get(&pdev->dev, "byte0_src");
	if (IS_ERR(curr_p)) {
		dev_err(&pdev->dev, "Failed to get byte0 source.\n");
		ret = PTR_ERR(curr_p);
		goto byte0_fail;
	}

	for (counter = 0; counter < (sizeof(ftbl_gcc_mdss_byte0_clk)/
				sizeof(struct clk_freq_tbl)); counter++)
		ftbl_gcc_mdss_byte0_clk[counter].src_clk = curr_p;

	curr_p = byte1_clk_src.c.parent = devm_clk_get(&pdev->dev, "byte1_src");
	if (IS_ERR(curr_p)) {
		dev_err(&pdev->dev, "Failed to get byte1 source.\n");
		ret = PTR_ERR(curr_p);
		goto byte1_fail;
	}

	for (counter = 0; counter < (sizeof(ftbl_gcc_mdss_byte1_clk)/
				sizeof(struct clk_freq_tbl)); counter++)
		ftbl_gcc_mdss_byte1_clk[counter].src_clk = curr_p;

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_gcc_mdss,
					ARRAY_SIZE(msm_clocks_gcc_mdss));
	if (ret)
		goto fail;

	dev_info(&pdev->dev, "Registered GCC MDSS clocks.\n");

	return ret;
fail:
	devm_clk_put(&pdev->dev, byte1_clk_src.c.parent);
byte1_fail:
	devm_clk_put(&pdev->dev, byte0_clk_src.c.parent);
byte0_fail:
	devm_clk_put(&pdev->dev, pclk1_clk_src.c.parent);
pclk1_fail:
	devm_clk_put(&pdev->dev, pclk0_clk_src.c.parent);
	return ret;
}

static struct of_device_id msm_clock_mdss_match_table[] = {
	{ .compatible = "qcom,gcc-mdss-8936" },
	{}
};

static struct platform_driver msm_clock_gcc_mdss_driver = {
	.probe = msm_gcc_mdss_probe,
	.driver = {
		.name = "gcc-mdss-8936",
		.of_match_table = msm_clock_mdss_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init msm_gcc_mdss_init(void)
{
	return platform_driver_register(&msm_clock_gcc_mdss_driver);
}
fs_initcall_sync(msm_gcc_mdss_init);

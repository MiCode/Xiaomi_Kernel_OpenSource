/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk/msm-clock-generic.h>
#include <dt-bindings/clock/msm-clocks-californium.h>
#include <soc/qcom/clock-rpm.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-alpha-pll.h>
#include <soc/qcom/rpm-smd.h>

#include "vdd-level-californium.h"

#define RPM_MISC_CLK_TYPE	0x306b6c63
#define RPM_BUS_CLK_TYPE	0x316b6c63
#define RPM_MEM_CLK_TYPE	0x326b6c63
#define RPM_IPA_CLK_TYPE	0x617069
#define RPM_CE_CLK_TYPE		0x6563
#define RPM_QPIC_CLK_TYPE	0x63697071

#define RPM_SMD_KEY_ENABLE	0x62616E45

#define CXO_CLK_SRC_ID		0x0
#define QDSS_CLK_ID		0x1
#define PCNOC_CLK_ID		0x0
#define SNOC_CLK_ID		0x1
#define BIMC_CLK_ID		0x0
#define IPA_CLK_ID		0x0
#define QPIC_CLK_ID		0x0
#define CE_CLK_ID		0x0
#define XO_ID			0x0
#define MSS_CFG_AHB_CLK_ID	0x0

#define RF_CLK1_ID		0x4
#define RF_CLK2_ID		0x5
#define RF_CLK3_ID		0x6
#define LN_BB_CLK_ID		0x8
#define DIV_CLK1_ID		0xb

#define RF_CLK1_PIN_ID		0x4
#define RF_CLK2_PIN_ID		0x5
#define RF_CLK3_PIN_ID		0x6

static void __iomem *virt_base;
static void __iomem *virt_dbgbase;
static void __iomem *virt_apcsbase;

#define GCC_REG_BASE(x) (void __iomem *)(virt_base + (x))

#define xo_source_val 0
#define gpll0_out_main_cgc_source_val 1
#define gpll0_out_main_div2_cgc_source_val 2

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

#define F_EXT(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)FIXDIV(div)) \
			| BVAL(10, 8, s##_source_val), \
	}

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);
static DEFINE_VDD_REGULATORS(vdd_dig_ao, VDD_DIG_NUM, 1, vdd_corner, NULL);

#define GPLL0_MODE                                       (0x21000)
#define MSS_Q6_BIMC_AXI_CBCR                             (0x49004)
#define QUSB2A_PHY_BCR                                   (0x41028)
#define SDCC1_APPS_CMD_RCGR                              (0x42004)
#define SDCC1_APPS_CBCR                                  (0x42018)
#define SDCC1_AHB_CBCR                                   (0x4201C)
#define BLSP1_AHB_CBCR                                   (0x1008)
#define BLSP1_QUP1_SPI_APPS_CBCR                         (0x2004)
#define BLSP1_QUP1_I2C_APPS_CBCR                         (0x2008)
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR                     (0x200C)
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR                     (0x3000)
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR                     (0x4000)
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR                     (0x5000)
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR                     (0x2024)
#define BLSP1_UART1_APPS_CBCR                            (0x203C)
#define BLSP1_UART1_APPS_CMD_RCGR                        (0x2044)
#define BLSP1_QUP2_SPI_APPS_CBCR                         (0x300C)
#define BLSP1_QUP2_I2C_APPS_CBCR                         (0x3010)
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR                     (0x3014)
#define BLSP1_UART2_APPS_CBCR                            (0x302C)
#define BLSP1_UART2_APPS_CMD_RCGR                        (0x3034)
#define BLSP1_QUP3_SPI_APPS_CBCR                         (0x401C)
#define BLSP1_QUP3_I2C_APPS_CBCR                         (0x4020)
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR                     (0x4024)
#define BLSP1_UART3_APPS_CBCR                            (0x403C)
#define BLSP1_UART3_APPS_CMD_RCGR                        (0x4044)
#define BLSP1_QUP4_SPI_APPS_CBCR                         (0x501C)
#define BLSP1_QUP4_I2C_APPS_CBCR                         (0x5020)
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR                     (0x5024)
#define BLSP1_UART4_APPS_CBCR                            (0x503C)
#define BLSP1_UART4_APPS_CMD_RCGR                        (0x5044)
#define PDM_AHB_CBCR                                     (0x44004)
#define PDM2_CBCR                                        (0x4400C)
#define PDM2_CMD_RCGR                                    (0x44010)
#define PRNG_AHB_CBCR                                    (0x13004)
#define BOOT_ROM_AHB_CBCR                                (0x1300C)
#define RPM_MISC                                         (0x2D028)
#define GCC_XO_CMD_RCGR                                  (0x30018)
#define GCC_XO_DIV4_CBCR                                 (0x30034)
#define APSS_TCU_CBCR                                    (0x12018)
#define SMMU_CFG_CBCR                                    (0x12038)
#define APCS_GPLL_ENA_VOTE                               (0x45000)
#define APCS_CLOCK_BRANCH_ENA_VOTE                       (0x45004)
#define APCS_SMMU_CLOCK_BRANCH_ENA_VOTE                  (0x4500C)
#define GCC_DEBUG_CLK_CTL                                (0x74000)
#define CLOCK_FRQ_MEASURE_CTL                            (0x74004)
#define CLOCK_FRQ_MEASURE_STATUS                         (0x74008)
#define PLLTEST_PAD_CFG                                  (0x7400C)
#define GP1_CBCR                                         (0x8000)
#define GP1_CMD_RCGR                                     (0x8004)
#define GP2_CBCR                                         (0x9000)
#define GP2_CMD_RCGR                                     (0x9004)
#define GP3_CBCR                                         (0xA000)
#define GP3_CMD_RCGR                                     (0xA004)
#define PCIE_CFG_AHB_CBCR                                (0x5D008)
#define PCIE_PIPE_CBCR                                   (0x5D00C)
#define PCIE_AXI_CBCR                                    (0x5D010)
#define PCIE_SLEEP_CBCR                                  (0x5D014)
#define PCIE_AXI_MSTR_CBCR                               (0x5D018)
#define PCIE_AUX_CMD_RCGR                                (0x5D030)
#define PCIEPHY_PHY_BCR                                  (0x5D048)
#define PCIE_REF_CLK_EN                                  (0x5D04C)
#define PCIE_PHY_BCR                                     (0x5D050)
#define USB_SS_REF_CLK_EN                                (0x5E07C)
#define USB_PHY_CFG_AHB_CBCR                             (0x5E080)
#define SYS_NOC_USB3_AXI_CBCR                            (0x5E084)
#define USB_30_BCR                                       (0x5E070)
#define USB30_MASTER_CBCR                                (0x5E000)
#define USB30_SLEEP_CBCR                                 (0x5E004)
#define USB30_MOCK_UTMI_CBCR                             (0x5E008)
#define USB30_MASTER_CMD_RCGR                            (0x5E00C)
#define USB30_MOCK_UTMI_CMD_RCGR                         (0x5E020)
#define USB3_PHY_BCR                                     (0x5E034)
#define USB3PHY_PHY_BCR                                  (0x5E03C)
#define USB3_PIPE_CBCR                                   (0x5E040)
#define USB3_AUX_CBCR                                    (0x5E044)
#define USB3_AUX_CMD_RCGR                                (0x5E05C)
#define USB3_AXI_TBU_CBCR                                (0x12060)
#define PCIE_AXI_TBU_CBCR                                (0x12064)
#define QUSB_REF_CLK_EN                                  (0x41030)
#define DCC_CBCR                                         (0x77004)
#define MSS_CFG_AHB_CBCR				 (0x49000)

DEFINE_CLK_RPM_SMD_BRANCH(xo, xo_a_clk, RPM_MISC_CLK_TYPE,
			  XO_ID, 19200000);

DEFINE_CLK_RPM_SMD(ce_clk, ce_a_clk, RPM_CE_CLK_TYPE,
		   CE_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(pcnoc_clk, pcnoc_a_clk, RPM_BUS_CLK_TYPE,
		   PCNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE,
		   BIMC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE,
		   SNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(ipa_clk, ipa_a_clk, RPM_IPA_CLK_TYPE,
		   IPA_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(qpic_clk, qpic_a_clk, RPM_QPIC_CLK_TYPE,
		   QPIC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE,
			QDSS_CLK_ID);

static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(mcd_ce_clk, &ce_clk.c, 85710000);
static DEFINE_CLK_VOTER(pcnoc_keepalive_a_clk, &pcnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pcnoc_msmbus_clk, &pcnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pcnoc_msmbus_a_clk, &pcnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pcnoc_pm_clk, &pcnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pcnoc_sps_clk, &pcnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(qcedev_ce_clk, &ce_clk.c, 85710000);
static DEFINE_CLK_VOTER(qcrypto_ce_clk, &ce_clk.c, 85710000);
static DEFINE_CLK_VOTER(qseecom_ce_clk, &ce_clk.c, 85710000);
static DEFINE_CLK_VOTER(scm_ce_clk, &ce_clk.c, 85710000);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, &xo.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, &xo.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, &xo.c);

DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk1, div_clk1_ao, DIV_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(ln_bb_clk, ln_bb_a_clk, LN_BB_CLK_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk1, rf_clk1_ao, RF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk2, rf_clk2_ao, RF_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk3, rf_clk3_ao, RF_CLK3_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk1_pin, rf_clk1_pin_ao,
				     RF_CLK1_PIN_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk2_pin, rf_clk2_pin_ao,
				     RF_CLK2_PIN_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk3_pin, rf_clk3_pin_ao,
				     RF_CLK3_PIN_ID);

static struct alpha_pll_masks alpha_pll_masks_20nm_p = {
	.lock_mask = BIT(31),
	.update_mask = BIT(22),
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
};

static struct alpha_pll_vco_tbl alpha_pll_vco_20nm_p[] = {
	VCO(3,  250000000,  500000000),
	VCO(2,  500000000, 1000000000),
	VCO(1, 1000000000, 1500000000),
	VCO(0, 1500000000, 2000000000),
};

static struct alpha_pll_clk a7pll_clk = {
	.masks = &alpha_pll_masks_20nm_p,
	.base = &virt_apcsbase,
	.vco_tbl = alpha_pll_vco_20nm_p,
	.num_vco = ARRAY_SIZE(alpha_pll_vco_20nm_p),
	.c = {
		.parent = &xo_a_clk.c,
		.dbg_name = "a7pll_clk",
		.ops = &clk_ops_alpha_pll,
		VDD_DIG_FMAX_MAP2_AO(LOW, 1000000000, NOMINAL, 2000000000),
		CLK_INIT(a7pll_clk.c),
	},
};

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0 = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_base,
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
	.status_reg = (void __iomem *)GPLL0_MODE,
	.status_mask = BIT(30),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_base,
	.c = {
		.rate = 600000000,
		.parent = &xo_a_clk.c,
		.dbg_name = "gpll0_ao",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao.c),
	},
};

DEFINE_EXT_CLK(gpll0_out_main_cgc, &gpll0.c);

DEFINE_FIXED_DIV_CLK(gpll0_out_main_div2_cgc, 2, &gpll0.c);

static struct gate_clk gpll0_out_msscc = {
	.en_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(18),
	.delay_us = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gpll0_out_msscc",
		.ops = &clk_ops_gate,
		CLK_INIT(gpll0_out_msscc.c),
	},
};

static struct clk_freq_tbl ftbl_usb30_master_clk_src[] = {
	F( 125000000, gpll0_out_main_cgc,    1,    5,    24),
	F_END
};

static struct rcg_clk usb30_master_clk_src = {
	.cmd_rcgr_reg = USB30_MASTER_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_usb30_master_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb30_master_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP4(LOWER, 60000000, LOW, 120000000,
				  NOMINAL, 171430000, HIGH, 200000000),
		CLK_INIT(usb30_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp_i2c_apps_clk_src[] = {
	F(  19200000,         xo,    1,    0,     0),
	F(  50000000, gpll0_out_main_cgc,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup1_spi_apps_clk_src[] = {
	F(    960000,         xo,   10,    1,     2),
	F(   4800000,         xo,    4,    0,     0),
	F(   9600000,         xo,    2,    0,     0),
	F(  15000000, gpll0_out_main_cgc,   10,    1,     4),
	F(  19200000,         xo,    1,    0,     0),
	F(  24000000, gpll0_out_main_cgc, 12.5,    1,     2),
	F(  25000000, gpll0_out_main_cgc,   12,    1,     2),
	F(  50000000, gpll0_out_main_cgc,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup1_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup2_spi_apps_clk_src[] = {
	F(    960000,         xo,   10,    1,     2),
	F(   4800000,         xo,    4,    0,     0),
	F(   9600000,         xo,    2,    0,     0),
	F(  15000000, gpll0_out_main_cgc,   10,    1,     4),
	F(  19200000,         xo,    1,    0,     0),
	F(  24000000, gpll0_out_main_cgc, 12.5,    1,     2),
	F(  25000000, gpll0_out_main_cgc,   12,    1,     2),
	F(  50000000, gpll0_out_main_cgc,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup2_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup3_spi_apps_clk_src[] = {
	F(    960000,         xo,   10,    1,     2),
	F(   4800000,         xo,    4,    0,     0),
	F(   9600000,         xo,    2,    0,     0),
	F(  15000000, gpll0_out_main_cgc,   10,    1,     4),
	F(  19200000,         xo,    1,    0,     0),
	F(  24000000, gpll0_out_main_cgc, 12.5,    1,     2),
	F(  25000000, gpll0_out_main_cgc,   12,    1,     2),
	F(  50000000, gpll0_out_main_cgc,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup3_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp1_qup4_spi_apps_clk_src[] = {
	F(    960000,         xo,   10,    1,     2),
	F(   4800000,         xo,    4,    0,     0),
	F(   9600000,         xo,    2,    0,     0),
	F(  15000000, gpll0_out_main_cgc,   10,    1,     4),
	F(  19200000,         xo,    1,    0,     0),
	F(  24000000, gpll0_out_main_cgc, 12.5,    1,     2),
	F(  25000000, gpll0_out_main_cgc,   12,    1,     2),
	F(  50000000, gpll0_out_main_cgc,   12,    0,     0),
	F_END
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp1_qup4_spi_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 12500000, LOW, 25000000,
				  NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_blsp_uart_apps_clk_src[] = {
	F(   3686400, gpll0_out_main_div2_cgc,    1,  192, 15625),
	F(   7372800, gpll0_out_main_div2_cgc,    1,  384, 15625),
	F(  14745600, gpll0_out_main_div2_cgc,    1,  768, 15625),
	F(  16000000, gpll0_out_main_div2_cgc,    1,    4,    75),
	F(  19200000,         xo,    1,    0,     0),
	F(  24000000, gpll0_out_main_cgc,    5,    1,     5),
	F(  32000000, gpll0_out_main_cgc,    1,    4,    75),
	F(  40000000, gpll0_out_main_cgc,   15,    0,     0),
	F(  46400000, gpll0_out_main_cgc,    1,   29,   375),
	F(  48000000, gpll0_out_main_cgc, 12.5,    0,     0),
	F(  51200000, gpll0_out_main_cgc,    1,   32,   375),
	F(  56000000, gpll0_out_main_cgc,    1,    7,    75),
	F(  58982400, gpll0_out_main_cgc,    1, 1536, 15625),
	F(  60000000, gpll0_out_main_cgc,   10,    0,     0),
	F(  63157895, gpll0_out_main_cgc,  9.5,    0,     0),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 48000000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 48000000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 48000000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 19200000, LOW, 48000000,
				  NOMINAL, 63160000),
		CLK_INIT(blsp1_uart4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gp_clk_src[] = {
	F(  19200000,         xo,    1,    0,     0),
	F(  50000000, gpll0_out_main_div2_cgc,    6,    0,     0),
	F( 100000000, gpll0_out_main_cgc,    6,    0,     0),
	F( 200000000, gpll0_out_main_cgc,    3,    0,     0),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(gp1_clk_src.c),
	},
};

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg = GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(gp2_clk_src.c),
	},
};

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg = GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gp_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pcie_aux_clk_src[] = {
	F(   1000000,         xo,    1,    5,    96),
	F_END
};

static struct rcg_clk pcie_aux_clk_src = {
	.cmd_rcgr_reg = PCIE_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_pcie_aux_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "pcie_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(pcie_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_pdm2_clk_src[] = {
	F(  19200000,         xo,    1,    0,     0),
	F(  60000000, gpll0_out_main_cgc,   10,    0,     0),
	F_END
};

static struct rcg_clk pdm2_clk_src = {
	.cmd_rcgr_reg = PDM2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_pdm2_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "pdm2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, LOW, 60000000),
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_sdcc1_apps_clk_src[] = {
	F(    144000,         xo,   16,    3,    25),
	F(    400000,         xo,   12,    1,     4),
	F(  20000000, gpll0_out_main_div2_cgc,   15,    0,     0),
	F(  25000000, gpll0_out_main_div2_cgc,   12,    0,     0),
	F(  50000000, gpll0_out_main_div2_cgc,    6,    0,     0),
	F( 100000000, gpll0_out_main_cgc,    6,    0,     0),
	F( 200000000, gpll0_out_main_cgc,    3,    0,     0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg = SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_sdcc1_apps_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOWER, 50000000, LOW, 100000000,
				  NOMINAL, 200000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb30_mock_utmi_clk_src[] = {
	F(  60000000, gpll0_out_main_cgc,   10,    0,     0),
	F_END
};

static struct rcg_clk usb30_mock_utmi_clk_src = {
	.cmd_rcgr_reg = USB30_MOCK_UTMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_usb30_mock_utmi_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb30_mock_utmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOWER, 30000000, LOW, 60000000),
		CLK_INIT(usb30_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_usb3_aux_clk_src[] = {
	F(   1000000,         xo,    1,    5,    96),
	F_END
};

static struct rcg_clk usb3_aux_clk_src = {
	.cmd_rcgr_reg = USB3_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_usb3_aux_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "usb3_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
		CLK_INIT(usb3_aux_clk_src.c),
	},
};

static struct reset_clk gcc_pcie_phy_reset = {
	.reset_reg = PCIE_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_pcie_phy_reset.c),
	},
};

static struct reset_clk gcc_qusb2a_phy_reset = {
	.reset_reg = QUSB2A_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_qusb2a_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_qusb2a_phy_reset.c),
	},
};

static struct reset_clk gcc_usb3phy_phy_reset = {
	.reset_reg = USB3PHY_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3phy_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb3phy_phy_reset.c),
	},
};

static struct reset_clk gcc_usb3_phy_reset = {
	.reset_reg = USB3_PHY_BCR,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_phy_reset",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb3_phy_reset.c),
	},
};

static struct local_vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg = BLSP1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(10),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp1_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.parent = &blsp1_uart4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct local_vote_clk gcc_boot_rom_ahb_clk = {
	.cbcr_reg = BOOT_ROM_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(7),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_boot_rom_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_boot_rom_ahb_clk.c),
	},
};

static struct branch_clk gcc_dcc_clk = {
	.cbcr_reg = DCC_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_dcc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_dcc_clk.c),
	},
};

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_gp3_clk",
		.parent = &gp3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_mss_q6_bimc_axi_clk = {
	.cbcr_reg = MSS_Q6_BIMC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mss_q6_bimc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_q6_bimc_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_axi_clk = {
	.cbcr_reg = PCIE_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_axi_mstr_clk = {
	.cbcr_reg = PCIE_AXI_MSTR_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_axi_mstr_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_axi_mstr_clk.c),
	},
};

static struct branch_clk gcc_pcie_cfg_ahb_clk = {
	.cbcr_reg = PCIE_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_pipe_clk = {
	.cbcr_reg = PCIE_PIPE_CBCR,
	.bcr_reg = PCIEPHY_PHY_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.halt_check = DELAY,
	.c = {
		.dbg_name = "gcc_pcie_pipe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_sleep_clk = {
	.cbcr_reg = PCIE_SLEEP_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_sleep_clk",
		.parent = &pcie_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_sleep_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = PDM2_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
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
	.base = &virt_base,
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
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_prng_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_prng_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_ahb_clk = {
	.cbcr_reg = SDCC1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_apps_clk = {
	.cbcr_reg = SDCC1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sdcc1_apps_clk",
		.parent = &sdcc1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_apps_clk.c),
	},
};

static struct local_vote_clk gcc_apss_tcu_clk = {
	.cbcr_reg = APSS_TCU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(1),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_apss_tcu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_apss_tcu_clk.c),
	},
};

static struct local_vote_clk gcc_pcie_axi_tbu_clk = {
	.cbcr_reg = PCIE_AXI_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(16),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_axi_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_pcie_axi_tbu_clk.c),
	},
};

static struct gate_clk gcc_pcie_ref_clk = {
	.en_reg = PCIE_REF_CLK_EN,
	.en_mask = BIT(0),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_pcie_ref_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_pcie_ref_clk.c),
	},
};

static struct gate_clk gcc_usb_ss_ref_clk = {
	.en_reg = USB_SS_REF_CLK_EN,
	.en_mask = BIT(0),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb_ss_ref_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_usb_ss_ref_clk.c),
	},
};

static struct gate_clk gcc_qusb_ref_clk = {
	.en_reg = QUSB_REF_CLK_EN,
	.en_mask = BIT(0),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_qusb_ref_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_qusb_ref_clk.c),
	},
};

static struct local_vote_clk gcc_smmu_cfg_clk = {
	.cbcr_reg = SMMU_CFG_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(12),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_smmu_cfg_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_smmu_cfg_clk.c),
	},
};

static struct local_vote_clk gcc_usb3_axi_tbu_clk = {
	.cbcr_reg = USB3_AXI_TBU_CBCR,
	.vote_reg = APCS_SMMU_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(15),
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_axi_tbu_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_usb3_axi_tbu_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_usb3_axi_clk = {
	.cbcr_reg = SYS_NOC_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_sys_noc_usb3_axi_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_usb30_master_clk = {
	.cbcr_reg = USB30_MASTER_CBCR,
	.bcr_reg = USB_30_BCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb30_master_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_master_clk.c),
		.depends = &gcc_sys_noc_usb3_axi_clk.c,
	},
};

static struct branch_clk gcc_usb30_mock_utmi_clk = {
	.cbcr_reg = USB30_MOCK_UTMI_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb30_mock_utmi_clk",
		.parent = &usb30_mock_utmi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_mock_utmi_clk.c),
	},
};

static struct branch_clk gcc_usb30_sleep_clk = {
	.cbcr_reg = USB30_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb30_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb3_aux_clk = {
	.cbcr_reg = USB3_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_aux_clk",
		.parent = &usb3_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb3_aux_clk.c),
	},
};

static struct gate_clk gcc_usb3_pipe_clk = {
	.en_reg = USB3_PIPE_CBCR,
	.en_mask = BIT(0),
	.delay_us = 50,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb3_pipe_clk",
		.ops = &clk_ops_gate,
		CLK_INIT(gcc_usb3_pipe_clk.c),
	},
};

static struct branch_clk gcc_usb_phy_cfg_ahb_clk = {
	.cbcr_reg = USB_PHY_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_usb_phy_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_phy_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_mss_cfg_ahb_clk = {
	.cbcr_reg = MSS_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gcc_mss_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_cfg_ahb_clk.c),
	},
};

static struct mux_clk gcc_debug_mux;
static struct clk_ops clk_ops_debug_mux;
static struct clk_mux_ops gcc_debug_mux_ops;

static struct measure_clk_data debug_mux_priv = {
	.cxo = &xo.c,
	.plltest_reg = PLLTEST_PAD_CFG,
	.plltest_val = 0x51A00,
	.xo_div4_cbcr = GCC_XO_DIV4_CBCR,
	.ctl_reg = CLOCK_FRQ_MEASURE_CTL,
	.status_reg = CLOCK_FRQ_MEASURE_STATUS,
	.base = &virt_base,
};

static struct mux_clk gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.ops = &gcc_debug_mux_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.base = &virt_dbgbase,
	MUX_REC_SRC_LIST(
	),
	MUX_SRC_LIST(
		{ &snoc_clk.c, 0x0000 },
		{ &gcc_sys_noc_usb3_axi_clk.c, 0x0001 },
		{ &pcnoc_clk.c, 0x0008 },
		{ &gcc_mss_cfg_ahb_clk.c, 0x0030 },
		{ &qdss_clk.c, 0x0042 },
		{ &gcc_apss_tcu_clk.c, 0x0050 },
		{ &gcc_smmu_cfg_clk.c, 0x005b },
		{ &gcc_sdcc1_apps_clk.c, 0x0068 },
		{ &gcc_sdcc1_ahb_clk.c, 0x0069 },
		{ &gcc_blsp1_ahb_clk.c, 0x0088 },
		{ &gcc_blsp1_qup1_spi_apps_clk.c, 0x008a },
		{ &gcc_blsp1_qup1_i2c_apps_clk.c, 0x008b },
		{ &gcc_blsp1_uart1_apps_clk.c, 0x008c },
		{ &gcc_blsp1_qup2_spi_apps_clk.c, 0x008e },
		{ &gcc_blsp1_qup2_i2c_apps_clk.c, 0x0090 },
		{ &gcc_blsp1_uart2_apps_clk.c, 0x0091 },
		{ &gcc_blsp1_qup3_spi_apps_clk.c, 0x0093 },
		{ &gcc_blsp1_qup3_i2c_apps_clk.c, 0x0094 },
		{ &gcc_blsp1_uart3_apps_clk.c, 0x0095 },
		{ &gcc_blsp1_qup4_spi_apps_clk.c, 0x0098 },
		{ &gcc_blsp1_qup4_i2c_apps_clk.c, 0x0099 },
		{ &gcc_blsp1_uart4_apps_clk.c, 0x009a },
		{ &gcc_pdm_ahb_clk.c, 0x00d0 },
		{ &gcc_pdm2_clk.c, 0x00d2 },
		{ &gcc_prng_ahb_clk.c, 0x00d8 },
		{ &gcc_boot_rom_ahb_clk.c, 0x00f8 },
		{ &ce_clk.c, 0x0138 },
		{ &bimc_clk.c, 0x0155 },
		{ &gcc_usb3_axi_tbu_clk.c, 0x0203 },
		{ &gcc_pcie_axi_tbu_clk.c, 0x0204 },
		{ &ipa_clk.c, 0x0218 },
		{ &qpic_clk.c, 0x0220 },
		{ &gcc_usb30_master_clk.c, 0x0230 },
		{ &gcc_usb30_sleep_clk.c, 0x0231 },
		{ &gcc_usb30_mock_utmi_clk.c, 0x0232 },
		{ &gcc_usb_phy_cfg_ahb_clk.c, 0x0233 },
		{ &gcc_usb3_pipe_clk.c, 0x0234 },
		{ &gcc_usb3_aux_clk.c, 0x0235 },
		{ &gcc_pcie_cfg_ahb_clk.c, 0x0238 },
		{ &gcc_pcie_pipe_clk.c, 0x0239 },
		{ &gcc_pcie_axi_clk.c, 0x023a },
		{ &gcc_pcie_sleep_clk.c, 0x023b },
		{ &gcc_pcie_axi_mstr_clk.c, 0x023c },
		{ &gcc_dcc_clk.c, 0x0278 },
	),
	.c = {
		.dbg_name = "gcc_debug_mux",
		.ops = &clk_ops_debug_mux,
		.flags = CLKFLAG_NO_RATE_CACHE | CLKFLAG_MEASURE,
		CLK_INIT(gcc_debug_mux.c),
	},
};

static struct clk_lookup msm_clocks_rpm_californium[] = {
	CLK_LIST(a7pll_clk),
	CLK_LIST(xo),
	CLK_LIST(xo_a_clk),
	CLK_LIST(ce_clk),
	CLK_LIST(ce_a_clk),
	CLK_LIST(pcnoc_clk),
	CLK_LIST(pcnoc_a_clk),
	CLK_LIST(bimc_clk),
	CLK_LIST(bimc_a_clk),
	CLK_LIST(snoc_clk),
	CLK_LIST(snoc_a_clk),
	CLK_LIST(ipa_clk),
	CLK_LIST(ipa_a_clk),
	CLK_LIST(qpic_clk),
	CLK_LIST(qpic_a_clk),
	CLK_LIST(qdss_clk),
	CLK_LIST(qdss_a_clk),
	CLK_LIST(bimc_msmbus_clk),
	CLK_LIST(bimc_msmbus_a_clk),
	CLK_LIST(mcd_ce_clk),
	CLK_LIST(pcnoc_keepalive_a_clk),
	CLK_LIST(pcnoc_msmbus_clk),
	CLK_LIST(pcnoc_msmbus_a_clk),
	CLK_LIST(pcnoc_pm_clk),
	CLK_LIST(pcnoc_sps_clk),
	CLK_LIST(qcedev_ce_clk),
	CLK_LIST(qcrypto_ce_clk),
	CLK_LIST(qseecom_ce_clk),
	CLK_LIST(scm_ce_clk),
	CLK_LIST(snoc_msmbus_clk),
	CLK_LIST(snoc_msmbus_a_clk),
	CLK_LIST(cxo_dwc3_clk),
	CLK_LIST(cxo_lpm_clk),
	CLK_LIST(cxo_otg_clk),
	CLK_LIST(div_clk1),
	CLK_LIST(div_clk1_ao),
	CLK_LIST(ln_bb_clk),
	CLK_LIST(ln_bb_a_clk),
	CLK_LIST(rf_clk1),
	CLK_LIST(rf_clk1_ao),
	CLK_LIST(rf_clk2),
	CLK_LIST(rf_clk2_ao),
	CLK_LIST(rf_clk3),
	CLK_LIST(rf_clk3_ao),
	CLK_LIST(rf_clk1_pin),
	CLK_LIST(rf_clk1_pin_ao),
	CLK_LIST(rf_clk2_pin),
	CLK_LIST(rf_clk2_pin_ao),
	CLK_LIST(rf_clk3_pin),
	CLK_LIST(rf_clk3_pin_ao),
};

static struct clk_lookup msm_clocks_gcc_californium[] = {
	CLK_LIST(gpll0),
	CLK_LIST(gpll0_ao),
	CLK_LIST(gpll0_out_main_cgc),
	CLK_LIST(gpll0_out_main_div2_cgc),
	CLK_LIST(gpll0_out_msscc),
	CLK_LIST(usb30_master_clk_src),
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
	CLK_LIST(blsp1_uart3_apps_clk_src),
	CLK_LIST(blsp1_uart4_apps_clk_src),
	CLK_LIST(gp1_clk_src),
	CLK_LIST(gp2_clk_src),
	CLK_LIST(gp3_clk_src),
	CLK_LIST(pcie_aux_clk_src),
	CLK_LIST(pdm2_clk_src),
	CLK_LIST(sdcc1_apps_clk_src),
	CLK_LIST(usb30_mock_utmi_clk_src),
	CLK_LIST(usb3_aux_clk_src),
	CLK_LIST(gcc_pcie_phy_reset),
	CLK_LIST(gcc_qusb2a_phy_reset),
	CLK_LIST(gcc_usb3phy_phy_reset),
	CLK_LIST(gcc_usb3_phy_reset),
	CLK_LIST(gcc_blsp1_ahb_clk),
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
	CLK_LIST(gcc_blsp1_uart3_apps_clk),
	CLK_LIST(gcc_blsp1_uart4_apps_clk),
	CLK_LIST(gcc_boot_rom_ahb_clk),
	CLK_LIST(gcc_dcc_clk),
	CLK_LIST(gcc_gp1_clk),
	CLK_LIST(gcc_gp2_clk),
	CLK_LIST(gcc_gp3_clk),
	CLK_LIST(gcc_mss_q6_bimc_axi_clk),
	CLK_LIST(gcc_pcie_axi_clk),
	CLK_LIST(gcc_pcie_axi_mstr_clk),
	CLK_LIST(gcc_pcie_cfg_ahb_clk),
	CLK_LIST(gcc_pcie_pipe_clk),
	CLK_LIST(gcc_pcie_sleep_clk),
	CLK_LIST(gcc_pdm2_clk),
	CLK_LIST(gcc_pdm_ahb_clk),
	CLK_LIST(gcc_prng_ahb_clk),
	CLK_LIST(gcc_sdcc1_ahb_clk),
	CLK_LIST(gcc_sdcc1_apps_clk),
	CLK_LIST(gcc_apss_tcu_clk),
	CLK_LIST(gcc_pcie_axi_tbu_clk),
	CLK_LIST(gcc_pcie_ref_clk),
	CLK_LIST(gcc_usb_ss_ref_clk),
	CLK_LIST(gcc_qusb_ref_clk),
	CLK_LIST(gcc_smmu_cfg_clk),
	CLK_LIST(gcc_usb3_axi_tbu_clk),
	CLK_LIST(gcc_sys_noc_usb3_axi_clk),
	CLK_LIST(gcc_usb30_master_clk),
	CLK_LIST(gcc_usb30_mock_utmi_clk),
	CLK_LIST(gcc_usb30_sleep_clk),
	CLK_LIST(gcc_usb3_aux_clk),
	CLK_LIST(gcc_usb3_pipe_clk),
	CLK_LIST(gcc_usb_phy_cfg_ahb_clk),
	CLK_LIST(gcc_mss_cfg_ahb_clk),
};

static int msm_gcc_californium_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	ret = vote_bimc(&bimc_clk, INT_MAX);
	if (ret < 0)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get CC base.\n");
		return -EINVAL;
	}
	virt_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_base) {
		dev_err(&pdev->dev, "Failed to map in CC registers.\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs_base");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get APCS base.\n");
		return -EINVAL;
	}
	virt_apcsbase = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!virt_apcsbase) {
		dev_err(&pdev->dev, "Failed to map in APCS registers.\n");
		return -ENOMEM;
	}

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get vdd_dig regulator!");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	vdd_dig_ao.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig_ao");
	if (IS_ERR(vdd_dig_ao.regulator[0])) {
		if (!(PTR_ERR(vdd_dig_ao.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev, "Unable to get vdd_dig_ao regulator!");
		return PTR_ERR(vdd_dig_ao.regulator[0]);
	}

	ret = of_msm_clock_register(pdev->dev.of_node,
				    msm_clocks_rpm_californium,
				    ARRAY_SIZE(msm_clocks_rpm_californium));
	if (ret)
		return ret;

	ret = enable_rpm_scaling();
	if (ret < 0)
		return ret;

	dev_info(&pdev->dev, "Registered RPM clocks.\n");

	ret = of_msm_clock_register(pdev->dev.of_node,
				    msm_clocks_gcc_californium,
				    ARRAY_SIZE(msm_clocks_gcc_californium));
	if (ret)
		return ret;

	/*
	 * Hold an active set vote for the PCNOC AHB source.
	 * Sleep set vote is 0.
	 */
	clk_set_rate(&pcnoc_keepalive_a_clk.c, 19200000);
	clk_prepare_enable(&pcnoc_keepalive_a_clk.c);

	clk_prepare_enable(&xo_a_clk.c);

	dev_info(&pdev->dev, "Registered GCC clocks.\n");

	return 0;
}

static struct of_device_id msm_clock_gcc_match_table[] = {
	{ .compatible = "qcom,gcc-californium" },
	{}
};

static struct platform_driver msm_clock_gcc_driver = {
	.probe = msm_gcc_californium_probe,
	.driver = {
		.name = "qcom,gcc-californium",
		.of_match_table = msm_clock_gcc_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_gcc_californium_init(void)
{
	return platform_driver_register(&msm_clock_gcc_driver);
}
arch_initcall(msm_gcc_californium_init);

/* ======== Clock Debug Controller ======== */
static struct clk_lookup msm_clocks_measure_californium[] = {
	CLK_LOOKUP_OF("measure", gcc_debug_mux, "debug"),
};

static struct of_device_id msm_clock_debug_match_table[] = {
	{ .compatible = "qcom,cc-debug-californium" },
	{}
};

static int msm_clock_debug_californium_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	clk_ops_debug_mux = clk_ops_gen_mux;
	clk_ops_debug_mux.get_rate = measure_get_rate;

	gcc_debug_mux_ops = mux_reg_ops;

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

	ret = of_msm_clock_register(pdev->dev.of_node,
				    msm_clocks_measure_californium,
				    ARRAY_SIZE(msm_clocks_measure_californium));
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered debug mux.\n");
	return ret;
}

static struct platform_driver msm_clock_debug_driver = {
	.probe = msm_clock_debug_californium_probe,
	.driver = {
		.name = "qcom,cc-debug-californium",
		.of_match_table = msm_clock_debug_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_clock_debug_californium_init(void)
{
	return platform_driver_register(&msm_clock_debug_driver);
}
late_initcall(msm_clock_debug_californium_init);

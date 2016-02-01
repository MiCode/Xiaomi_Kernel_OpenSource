/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MDM_CLOCKS_9607_HWIO_H
#define __MDM_CLOCKS_9607_HWIO_H

#define GPLL0_MODE				0x21000
#define GPLL0_STATUS				0x21024
#define GPLL1_MODE				0x20000
#define GPLL1_STATUS				0x2001C
#define GPLL2_MODE				0x25000
#define GPLL2_STATUS				0x25024
#define APCS_GPLL_ENA_VOTE			0x45000
#define APCS_MODE				0x00018
#define APSS_AHB_CMD_RCGR			0x46000
#define PRNG_AHB_CBCR				0x13004
#define EMAC_0_125M_CMD_RCGR			0x4E028
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR		 0x200C
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR		 0x2024
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR		 0x3000
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR		 0x3014
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR		 0x4000
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR		 0x4024
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR		 0x5000
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR		 0x5024
#define BLSP1_QUP5_I2C_APPS_CMD_RCGR		 0x6000
#define BLSP1_QUP5_SPI_APPS_CMD_RCGR		 0x6024
#define BLSP1_QUP6_I2C_APPS_CMD_RCGR		 0x7000
#define BLSP1_QUP6_SPI_APPS_CMD_RCGR		 0x7024
#define BLSP1_UART1_APPS_CMD_RCGR		 0x2044
#define BLSP1_UART2_APPS_CMD_RCGR		 0x3034
#define BLSP1_UART3_APPS_CMD_RCGR		 0x4044
#define BLSP1_UART4_APPS_CMD_RCGR		 0x5044
#define BLSP1_UART5_APPS_CMD_RCGR		 0x6044
#define BLSP1_UART6_APPS_CMD_RCGR		 0x7044
#define CRYPTO_CMD_RCGR			0x16004
#define GP1_CMD_RCGR				 0x8004
#define GP2_CMD_RCGR				 0x9004
#define GP3_CMD_RCGR				 0xA004
#define PDM2_CMD_RCGR				0x44010
#define QPIC_CMD_RCGR				0x3F004
#define SDCC1_APPS_CMD_RCGR			0x42004
#define SDCC2_APPS_CMD_RCGR			0x43004
#define EMAC_0_SYS_25M_CMD_RCGR		0x4E03C
#define EMAC_0_TX_CMD_RCGR			0x4E014
#define USB_HS_SYSTEM_CMD_RCGR			0x41010
#define USB_HSIC_CMD_RCGR			0x3D018
#define USB_HSIC_IO_CAL_CMD_RCGR		0x3D030
#define USB_HSIC_SYSTEM_CMD_RCGR		0x3D000
#define BIMC_PCNOC_AXI_CBCR			0x31024
#define BLSP1_AHB_CBCR				 0x1008
#define APCS_CLOCK_BRANCH_ENA_VOTE		0x45004
#define BLSP1_QUP1_I2C_APPS_CBCR		 0x2008
#define BLSP1_QUP1_SPI_APPS_CBCR		 0x2004
#define BLSP1_QUP2_I2C_APPS_CBCR		 0x3010
#define BLSP1_QUP2_SPI_APPS_CBCR		 0x300C
#define BLSP1_QUP3_I2C_APPS_CBCR		 0x4020
#define BLSP1_QUP3_SPI_APPS_CBCR		 0x401C
#define BLSP1_QUP4_I2C_APPS_CBCR		 0x5020
#define BLSP1_QUP4_SPI_APPS_CBCR		 0x501C
#define BLSP1_QUP5_I2C_APPS_CBCR		 0x6020
#define BLSP1_QUP5_SPI_APPS_CBCR		 0x601C
#define BLSP1_QUP6_I2C_APPS_CBCR		 0x7020
#define BLSP1_QUP6_SPI_APPS_CBCR		 0x701C
#define BLSP1_UART1_APPS_CBCR			 0x203C
#define BLSP1_UART2_APPS_CBCR			 0x302C
#define BLSP1_UART3_APPS_CBCR			 0x403C
#define BLSP1_UART4_APPS_CBCR			 0x503C
#define BLSP1_UART5_APPS_CBCR			 0x603C
#define BLSP1_UART6_APPS_CBCR			 0x703C
#define APSS_AHB_CBCR				0x4601C
#define APSS_AXI_CBCR				0x46020
#define BOOT_ROM_AHB_CBCR			0x1300C
#define CRYPTO_AHB_CBCR			0x16024
#define CRYPTO_AXI_CBCR			0x16020
#define CRYPTO_CBCR				0x1601C
#define GP1_CBCR				 0x8000
#define GP2_CBCR				 0x9000
#define GP3_CBCR				 0xA000
#define MSS_CFG_AHB_CBCR			0x49000
#define MSS_Q6_BIMC_AXI_CBCR			0x49004
#define PCNOC_APSS_AHB_CBCR			0x27030
#define PDM2_CBCR				0x4400C
#define PDM_AHB_CBCR				0x44004
#define QPIC_AHB_CBCR				0x3F01C
#define QPIC_CBCR				0x3F018
#define QPIC_SYSTEM_CBCR			0x3F020
#define SDCC1_AHB_CBCR				0x4201C
#define SDCC1_APPS_CBCR			0x42018
#define SDCC2_AHB_CBCR				0x4301C
#define SDCC2_APPS_CBCR			0x43018
#define EMAC_0_125M_CBCR			0x4E010
#define EMAC_0_AHB_CBCR			0x4E000
#define EMAC_0_AXI_CBCR			0x4E008
#define EMAC_0_RX_CBCR				0x4E030
#define EMAC_0_SYS_25M_CBCR			0x4E038
#define EMAC_0_SYS_CBCR				0x4E034
#define EMAC_0_TX_CBCR				0x4E00C
#define APSS_TCU_CBCR				0x12018
#define SMMU_CFG_CBCR				0x12038
#define QDSS_DAP_CBCR				0x29084
#define APCS_SMMU_CLOCK_BRANCH_ENA_VOTE		0x4500C
#define USB2A_PHY_SLEEP_CBCR			0x4102C
#define USB_HS_PHY_CFG_AHB_CBCR			0x41030
#define USB_HS_AHB_CBCR				0x41008
#define USB_HS_SYSTEM_CBCR			0x41004
#define USB_HS_BCR				0x41000
#define USB_HSIC_AHB_CBCR			0x3D04C
#define USB_HSIC_CBCR				0x3D050
#define USB_HSIC_IO_CAL_CBCR			0x3D054
#define USB_HSIC_IO_CAL_SLEEP_CBCR		0x3D058
#define USB_HSIC_SYSTEM_CBCR			0x3D048
#define USB_HS_HSIC_BCR				0x3D05C
#define USB2_HS_PHY_ONLY_BCR			0x41034
#define QUSB2_PHY_BCR				0x4103C
#define GCC_DEBUG_CLK_CTL			0x74000
#define CLOCK_FRQ_MEASURE_CTL			0x74004
#define CLOCK_FRQ_MEASURE_STATUS		0x74008
#define PLLTEST_PAD_CFG			0x7400C
#define GCC_XO_DIV4_CBCR			0x30034

#define xo_source_val				0
#define xo_a_source_val			0
#define gpll0_source_val			1
#define gpll2_source_val			1
#define emac_0_125m_clk_source_val		1
#define emac_0_tx_clk_source_val		2

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

#define F_EXT(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
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
	RPM_REGULATOR_LEVEL_NONE,              /* VDD_DIG_NONE */
	RPM_REGULATOR_LEVEL_SVS,		/* VDD_DIG_LOWER */
	RPM_REGULATOR_LEVEL_SVS_PLUS,		/*VDD_DIG_LOW*/
	RPM_REGULATOR_LEVEL_NOM,            /* VDD_DIG_NOMINAL */
	RPM_REGULATOR_LEVEL_TURBO,		/* VDD_DIG_HIGH */
};

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);


#define VDD_STROMER_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_stromer_pll, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM


#define RPM_MISC_CLK_TYPE			0x306b6c63
#define RPM_BUS_CLK_TYPE			0x316b6c63
#define RPM_MEM_CLK_TYPE			0x326b6c63
#define RPM_SMD_KEY_ENABLE			0x62616E45
#define RPM_QPIC_CLK_TYPE			0x63697071

#define XO_ID					0x0
#define QDSS_ID				0x1
#define PCNOC_ID				0x0
#define BIMC_ID				0x0
#define QPIC_ID				0x0

/* XO clock */
#define BB_CLK1_ID				1
#define RF_CLK2_ID				5

#endif

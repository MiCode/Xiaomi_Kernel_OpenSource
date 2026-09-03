/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc UIS8520 platform clocks
 *
 * Copyright (C) 2023, Unisoc Inc.
 */

#ifndef _DT_BINDINGS_CLK_UIS8520_H_
#define _DT_BINDINGS_CLK_UIS8520_H_

#define CLK_26M_AUD		0
#define CLK_13M			1
#define CLK_6M5			2
#define CLK_4M3			3
#define CLK_4M			4
#define CLK_2M			5
#define CLK_1M			6
#define CLK_250K		7
#define CLK_200K		8
#define CLK_160K		9
#define CLK_16K			10
#define CLK_RCO_100M_25M	11
#define CLK_RCO_100m_20M	12
#define CLK_RCO_100m_4M		13
#define CLK_RCO_100m_2M		14
#define CLK_RCO_60m_20M		15
#define CLK_RCO_60m_4M		16
#define CLK_RCO_60m_2M		17
#define CLK_PHYR8PLL_GATE	18
#define CLK_PSR8PLL_GATE	19
#define CLK_V4NRPLL_GATE	20
#define CLK_TGPLL_GATE		21
#define CLK_MPLLL_GATE		22
#define CLK_MPLLS_GATE		23
#define CLK_DPLL0_GATE		24
#define CLK_DPLL1_GATE		25
#define CLK_DPLL2_GATE		26
#define CLK_AUDPLL_GATE		27
#define CLK_PCIE0PLLV_GATE	28
#define CLK_PCIE1PLLV_GATE	29
#define CLK_TSNPLL_GATE		30
#define CLK_ETHPLL_GATE		31
#define CLK_PMU_GATE_NUM	(CLK_ETHPLL_GATE + 1)

#define CLK_RPLL		0
#define CLK_RPLL_390M		1
#define CLK_RPLL_260M		2
#define CLK_RPLL_26M		3
#define CLK_ANLG_PHY_G1_NUM	(CLK_RPLL_26M + 1)

#define CLK_TSNPLL		0
#define CLK_TSNPLL_1300M	1
#define CLK_TSNPLL_1040M	2
#define CLK_TSNPLL_100M		3
#define CLK_DFTPLL		4
#define CLK_PSR8PLL		5
#define CLK_PHYR8PLL		6
#define CLK_V4NRPLL		7
#define CLK_V4NRPLL_38M4	8
#define CLK_V4NRPLL_245M76	9
#define CLK_V4NRPLL_409M6	10
#define CLK_V4NRPLL_614M4	11
#define CLK_V4NRPLL_819M2	12
#define CLK_V4NRPLL_1228M8	13
#define CLK_AUDPLL		14
#define CLK_AUDPLL_38M4		15
#define CLK_AUDPLL_24M57	16
#define CLK_AUDPLL_19M2		17
#define CLK_AUDPLL_12M28	18
#define CLK_TGPLL		19
#define CLK_TGPLL_12M		20
#define CLK_TGPLL_24M		21
#define CLK_TGPLL_38M4		22
#define CLK_TGPLL_48M		23
#define CLK_TGPLL_51M2		24
#define CLK_TGPLL_64M		25
#define CLK_TGPLL_76M8		26
#define CLK_TGPLL_96M		27
#define CLK_TGPLL_128M		28
#define CLK_TGPLL_153M6		29
#define CLK_TGPLL_170M6		30
#define CLK_TGPLL_192M		31
#define CLK_TGPLL_219M4		32
#define CLK_TGPLL_256M		33
#define CLK_TGPLL_307M2		34
#define CLK_TGPLL_384M		35
#define CLK_TGPLL_512M		36
#define CLK_TGPLL_614M4		37
#define CLK_TGPLL_768M		38
#define CLK_ANLG_PHY_G2_NUM	(CLK_TGPLL_768M + 1)

#define CLK_MPLLL		0
#define CLK_MPLLS		1
#define CLK_ANLG_PHY_G10_NUM	(CLK_MPLLS + 1)

#define CLK_DPLL0		0
#define CLK_DPLL1		1
#define CLK_DPLL2		2
#define CLK_ANLG_PHY_G11_NUM	(CLK_DPLL2 + 1)

#define CLK_IIS0_EB		0
#define CLK_IIS1_EB		1
#define CLK_IIS2_EB		2
#define CLK_SPI0_EB		3
#define CLK_SPI1_EB		4
#define CLK_SPI2_EB		5
#define CLK_UART3_EB		6
#define CLK_UART0_EB		7
#define CLK_UART1_EB		8
#define CLK_UART2_EB		9
#define CLK_SPI0_LFIN_EB	10
#define CLK_SPI1_LFIN_EB	11
#define CLK_SPI2_LFIN_EB	12
#define CLK_SPI3_LFIN_EB	13
#define CLK_SPI4_LFIN_EB	14
#define CLK_SPI5_LFIN_EB	15
#define CLK_CE_SEC_EB		16
#define CLK_CE_PUB_EB		17
#define CLK_NIC400_EB		18
#define CLK_UART4_EB		19
#define CLK_UART5_EB		20
#define CLK_UART6_EB		21
#define CLK_UART7_EB		22
#define CLK_UART8_EB		23
#define CLK_UART9_EB		24
#define CLK_UART10_EB		25
#define CLK_SPI3_EB		26
#define CLK_SPI4_EB		27
#define CLK_SPI5_EB		28
#define CLK_AP_APB_GATE_NUM	(CLK_SPI5_EB + 1)

#define CLK_SDIO0_EB		0
#define CLK_SDIO1_EB		1
#define CLK_SDIO2_EB		2
#define CLK_EMMC_EB		3
#define CLK_DMA_PUB_EB		4
#define CLK_UFS_EB		5
#define CLK_CKG_EB		6
#define CLK_BUSMON_CLK_EB	7
#define CLK_AP2EMC_EB		8
#define CLK_I2C0_EB		9
#define CLK_I2C1_EB		10
#define CLK_I2C2_EB		11
#define CLK_I2C3_EB		12
#define CLK_I2C4_EB		13
#define CLK_I2C5_EB		14
#define CLK_I2C6_EB		15
#define CLK_I2C7_EB		16
#define CLK_I2C8_EB		17
#define CLK_I2C9_EB		18
#define CLK_I2C0_EB_SEC		19
#define CLK_I2C1_EB_SEC		20
#define CLK_I2C2_EB_SEC		21
#define CLK_I2C3_EB_SEC		22
#define CLK_I2C4_EB_SEC		23
#define CLK_I2C5_EB_SEC		24
#define CLK_I2C6_EB_SEC		25
#define CLK_I2C7_EB_SEC		26
#define CLK_I2C8_EB_SEC		27
#define CLK_I2C9_EB_SEC		28
#define CLK_NANDC_EB		29
#define CLK_I2C_S0_EB_SEC	30
#define CLK_I2C_S1_EB_SEC	31
#define CLK_I2C_S0_EB_NON_SEC	32
#define CLK_I2C_S1_EB_NON_SEC	33
#define CLK_AP_AHB_GATE_NUM	(CLK_I2C_S1_EB_NON_SEC + 1)

#define CLK_AP_APB		0
#define CLK_AP_AXI		1
#define CLK_NANDC_ECC		2
#define CLK_AP_UART0		3
#define CLK_AP_UART1		4
#define CLK_AP_UART2		5
#define CLK_AP_UART3		6
#define CLK_AP_UART4		7
#define CLK_AP_UART5		8
#define CLK_AP_UART6		9
#define CLK_AP_UART7		10
#define CLK_AP_UART8		11
#define CLK_AP_UART9		12
#define CLK_AP_UART10		13
#define CLK_AP_I2C0		14
#define CLK_AP_I2C1		15
#define CLK_AP_I2C2		16
#define CLK_AP_I2C3		17
#define CLK_AP_I2C4		18
#define CLK_AP_I2C5		19
#define CLK_AP_I2C6		20
#define CLK_AP_I2C7		21
#define CLK_AP_I2C8		22
#define CLK_AP_I2C9		23
#define CLK_I2C_SLV1		24
#define CLK_I2C_SLV2		25
#define CLK_AP_IIS0		26
#define CLK_AP_IIS1		27
#define CLK_AP_IIS2		28
#define CLK_AP_CE		29
#define CLK_EMMC_2X		30
#define CLK_EMMC_1X		31
#define CLK_NANDC_2X		32
#define CLK_NANDC_1X		33
#define CLK_DISP		34
#define CLK_AP_CLK_NUM		(CLK_DISP + 1)

#define CLK_RC100M_CAL_EB	0
#define CLK_RFTI_EB		1
#define CLK_DJTAG_EB		2
#define CLK_AUX0_EB		3
#define CLK_AUX1_EB		4
#define CLK_AUX2_EB		5
#define CLK_PROBE_EB		6
#define CLK_APCPU_DAP_EB	7
#define CLK_AON_CSSYS_EB	8
#define CLK_CSSYS_APB_EB	9
#define CLK_CSSYS_PUB_EB	10
#define CLK_AUX4_EB		11
#define CLK_AUX5_EB		12
#define CLK_AUX6_EB		13
#define CLK_EFUSE_EB		14
#define CLK_GPIO_EB		15
#define CLK_MBOX_EB		16
#define CLK_KPD_EB		17
#define CLK_AON_SYST_EB		18
#define CLK_AP_SYST_EB		19
#define CLK_AON_TMR_EB		20
#define CLK_AON_DVFS_TOP_EB	21
#define CLK_AON_USB2_TOP_EB	22
#define CLK_OTG_PHY_EB		23
#define CLK_SPLK_EB		24
#define CLK_PIN_EB		25
#define CLK_ANA_EB		26
#define CLK_APCPU_BUSMON_EB	27
#define CLK_APCPU_TS0_EB	28
#define CLK_APB_BUSMON_EB	29
#define CLK_AON_IIS_EB		30
#define CLK_SCC_EB		31
#define CLK_AUX3_EB		32
#define CLK_THM0_EB		33
#define CLK_THM1_EB		34
#define CLK_AUDCP_INTC_EB	35
#define CLK_PMU_EB		36
#define CLK_ADI_EB		37
#define CLK_EIC_EB		38
#define CLK_AP_INTC0_EB		39
#define CLK_AP_INTC1_EB		40
#define CLK_AP_INTC2_EB		41
#define CLK_AP_INTC3_EB		42
#define CLK_AP_INTC4_EB		43
#define CLK_AP_INTC5_EB		44
#define CLK_AP_INTC6_EB		45
#define CLK_AP_INTC7_EB		46
#define CLK_PSCP_INTC_EB	47
#define CLK_PHYCP_INTC_EB	48
#define CLK_AP_TMR0_EB		49
#define CLK_AP_TMR1_EB		50
#define CLK_AP_TMR2_EB		51
#define CLK_AP_UART_EB		52
#define CLK_AP_WDG_EB		53
#define CLK_APCPU_WDG_EB	54
#define CLK_ARCH_RTC_EB		55
#define CLK_KPD_RTC_EB		56
#define CLK_AON_SYST_RTC_EB	57
#define CLK_AP_SYST_RTC_EB	58
#define CLK_AON_TMR_RTC_EB	59
#define CLK_EIC_RTC_EB		60
#define CLK_EIC_RTCDV5_EB	61
#define CLK_AP_WDG_RTC_EB	62
#define CLK_AC_WDG_RTC_EB	63
#define CLK_AP_TMR0_RTC_EB	64
#define CLK_AP_TMR1_RTC_EB	65
#define CLK_AP_TMR2_RTC_EB	66
#define CLK_DCXO_LC_RTC_EB	67
#define CLK_BB_CAL_RTC_EB	68
#define CLK_DJTAG_TCK_EN	69
#define CLK_DMC_REF_EB		70
#define CLK_OTG_REF_EB		71
#define CLK_RC100M_REF_EB	72
#define CLK_RC100M_FDK_EB	73
#define CLK_DEBOUNCE_EB		74
#define CLK_DET_32K_EB		75
#define CLK_DEBUG_TS_EN		76
#define CLK_ACCESS_AUD_EN	77
#define CLK_AP_ACCESS_XGE_EN	78
#define CLK_AUX0		79
#define CLK_AUX1		80
#define CLK_AUX2		81
#define CLK_PROBE		82
#define CLK_AUX3		83
#define CLK_AUX4		84
#define CLK_AUX5		85
#define CLK_AUX6		86
#define CLK_AON_APB_GATE_NUM	(CLK_AUX6 + 1)

#define CLK_AON_APB		0
#define CLK_ADI			1
#define CLK_PWM_TIMER		2
#define CLK_EFUSE		3
#define CLK_UART0		4
#define CLK_UART1		5
#define CLK_AP_UART		6
#define CLK_THM0		7
#define CLK_THM1		8
#define CLK_SP_I3C0		9
#define CLK_SP_I3C1		10
#define CLK_SP_I2C0_SLV		11
#define CLK_SP_I2C1_SLV		12
#define CLK_SP_I2C0		13
#define CLK_SP_I2C1		14
#define CLK_SP_SPI0		15
#define CLK_SP_SPI1		16
#define CLK_SCC			17
#define CLK_APCPU_DAP		18
#define CLK_APCPU_TS		19
#define CLK_DEBUG_TS		20
#define CLK_RFFE		21
#define CLK_PRI_SBI		22
#define CLK_XO_SEL		23
#define CLK_RFTI_LTH		24
#define CLK_AFC_LTH		25
#define CLK_RCO100M_FDK		26
#define CLK_DJTAG_TCK		27
#define CLK_AON_TMR		28
#define CLK_AON_PMU		29
#define CLK_DEBOUNCE		30
#define CLK_APCPU_PMU		31
#define CLK_TOP_DVFS		32
#define CLK_PMU_26M		33
#define CLK_TZPC		34
#define CLK_OTG_REF		35
#define CLK_CSSYS		36
#define CLK_CSSYS_APB		37
#define CLK_SDIO0_2X		38
#define CLK_SDIO0_1X		39
#define CLK_SDIO2_2X		40
#define CLK_SDIO2_1X		41
#define CLK_SPI0		42
#define CLK_SPI1		43
#define CLK_SPI2		44
#define CLK_SPI3		45
#define CLK_SPI4		46
#define CLK_SPI5		47
#define CLK_SPI_PSCP		48
#define CLK_ANALOG_IO_APB	49
#define CLK_DMC_REF		50
#define CLK_USB			51
#define CLK_USB_SUSPEND		52
#define CLK_RCO60M_FDK		53
#define CLK_RCO6M_REF		54
#define CLK_RCO6M_FDK		55
#define CLK_AON_IIS		56
#define CLK_TSN_TIMER		57
#define CLK_AON_APB_NUM		(CLK_TSN_TIMER + 1)

#define CLK_CORE0		0
#define CLK_CORE1		1
#define CLK_CORE2		2
#define CLK_CORE3		3
#define CLK_SCU			4
#define CLK_ACE			5
#define CLK_ATB			6
#define CLK_DEBUG_APB		7
#define CLK_CPS			8
#define CLK_GIC			9
#define CLK_PERIPH		10
#define CLK_TOPDVFS_CLK_NUM	(CLK_PERIPH + 1)

#define CLK_USB_EB		0
#define CLK_USB_SUSPEND_EB	1
#define CLK_USB_REF_EB		2
#define CLK_IPA_EB		3
#define CLK_PAM_USB_EB		4
#define CLK_TFT_EB		5
#define CLK_PAM_WIFI_EB		6
#define CLK_IPA_TCA_EB		7
#define CLK_IPA_USB31PLL_EB	8
#define CLK_IPA_CKG_EB		9
#define CLK_IPA_ACCESS_PCIE0_EN	10
#define CLK_IPA_ACCESS_PCIE1_EN	11
#define CLK_IPA_ACCESS_ETH_EN	12
#define CLK_IPAAPB_GATE_NUM	(CLK_IPA_ACCESS_ETH_EN + 1)

#define CLK_IPA_AXI		0
#define CLK_IPA_APB		1
#define CLK_USB_REF		2
#define CLK_IPA_OTG_REF		3
#define CLK_IPA_CLK_NUM		(CLK_IPA_OTG_REF + 1)

#define CLK_PCIE3_AUX_0_EB	0
#define CLK_PCIE3_0_EB		1
#define CLK_NIC400_TRANMON_0_EB	2
#define CLK_NIC400_CFG_0_EB	3
#define CLK_PCIE3_PHY_0_EB	4
#define CLK_PCIE0APB_GATE_NUM	(CLK_PCIE3_PHY_0_EB + 1)

#define CLK_PCIE0_AXI		0
#define CLK_PCIE0_APB		1
#define CLK_PCIE0_AUX		2
#define CLK_PCIE0_CLK_NUM	(CLK_PCIE0_AUX + 1)

#define CLK_PCIE3_AUX_1_EB	0
#define CLK_PCIE3_1_EB		1
#define CLK_NIC400_TRANMON_1_EB	2
#define CLK_NIC400_CFG_1_EB	3
#define CLK_PCIE3_PHY_1_EB	4
#define CLK_PCIE1APB_GATE_NUM	(CLK_PCIE3_PHY_1_EB + 1)

#define CLK_PCIE1_AXI		0
#define CLK_PCIE1_APB		1
#define CLK_PCIE1_AUX		2
#define CLK_PCIE1_CLK_NUM	(CLK_PCIE1_AUX + 1)

#define CLK_XGMAC_EB			0
#define CLK_XGE_CLK_RF_EB		1
#define CLK_NIC400_TRANSMON_EB		2
#define CLK_NIC400_CFG_EB		3
#define CLK_PHY_EB			4
#define CLK_TSN_TIMER_EB		5
#define CLK_ETH_GATE_NUM		(CLK_TSN_TIMER_EB + 1)

#define CLK_XGE_AXI			0
#define CLK_XGE_APB			1
#define CLK_PHY_APB			2
#define CLK_ETH_CLK_NUM			(CLK_PHY_APB + 1)

#define CLK_AUDCP_IIS0_EB		0
#define CLK_AUDCP_IIS1_EB		1
#define CLK_AUDCP_IIS2_EB		2
#define CLK_AUDCP_UART_EB		3
#define CLK_AUDCP_DMA_CP_EB		4
#define CLK_AUDCP_DMA_AP_EB		5
#define CLK_AUDCP_GLB_VAD_EB		6
#define CLK_AUDCP_GLB_PDM_EB		7
#define CLK_AUDCP_GLB_PDM_IIS_EB	8
#define CLK_AUDCP_SRC48K_EB		9
#define CLK_AUDCP_MCDT_EB		10
#define CLK_AUDCP_VBC_EB		11
#define CLK_AUDCP_SPLK_EB		12
#define CLK_AUDCP_ICU_EB		13
#define CLK_AUDCP_DMA_AP_ASHB_EB	14
#define CLK_AUDCP_DMA_CP_ASHB_EB	15
#define CLK_AUDCP_AUD_EB		16
#define CLK_AUDIF_CKG_AUTO_EN		17
#define CLK_AUDCP_VBC_24M_EB		18
#define CLK_AUDCP_TMR_26M_EB		19
#define CLK_AUDCP_DVFS_ASHB_EB		20
#define CLK_AUDCP_MATRIX_CFG_EN		21
#define CLK_AUDCP_TDM_HF_EB		22
#define CLK_AUDCP_TDM_EB		23
#define CLK_AUDCP_GLB_AUDIF_EB		24
#define CLK_AUDCP_GLB_PDM_AP_EB		25
#define CLK_AUDCP_VBC_AP_EB		26
#define CLK_AUDCP_MCDT_AP_EB		27
#define CLK_AUDCP_AUD_AP_EB		28
#define CLK_AUDCP_VAD_PCLK_EB		29
#define CLK_AUDCP_GLB_GATE_NUM		(CLK_AUDCP_VAD_PCLK_EB + 1)

#define CLK_AUDCP_VAD_EB		0
#define CLK_AUDCP_PDM_EB		1
#define CLK_AUDCP_AUDIF_EB		2
#define CLK_AUDCP_PDM_IIS_EB		3
#define CLK_AUDCP_VAD_APB_EB		4
#define CLK_AUDCP_PDM_AP_EB		5
#define CLK_AUDCP_APB_GATE_NUM		(CLK_AUDCP_PDM_AP_EB + 1)

#endif/* _DT_BINDINGS_CLK_UIS8520_H_ */


/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc UDX710 platform clocks
 *
 * Copyright (C) 2023, Unisoc Inc.
 */

#ifndef __DT_BINDINGS_CLK_UDX710_H__
#define __DT_BINDINGS_CLK_UDX710_H__

#define CLK_MPLL0_GATE			0
#define CLK_MPLL1_GATE			1
#define CLK_PMU_GATE_NUM		(CLK_MPLL1_GATE + 1)

#define CLK_V3RPLL			0
#define CLK_V3RPLL_195M			1
#define CLK_V3PLL			2
#define CLK_TWPLL_768M			3
#define CLK_TWPLL_384M			4
#define CLK_TWPLL_192M			5
#define CLK_TWPLL_96M			6
#define CLK_TWPLL_48M			7
#define CLK_TWPLL_24M			8
#define CLK_TWPLL_12M			9
#define CLK_TWPLL_512M			10
#define CLK_TWPLL_256M			11
#define CLK_TWPLL_128M			12
#define CLK_TWPLL_64M			13
#define CLK_TWPLL_307M2			14
#define CLK_TWPLL_219M4			15
#define CLK_TWPLL_170M6			16
#define CLK_TWPLL_153M6			17
#define CLK_TWPLL_76M8			18
#define CLK_TWPLL_51M2			19
#define CLK_TWPLL_38M4			20
#define CLK_TWPLL_19M2			21
#define CLK_MPLL0			22
#define CLK_MPLL1			23
#define CLK_ANLG_PHY_G3_NUM		(CLK_MPLL1 + 1)

#define CLK_CORE0_CLK			0
#define CLK_CORE1_CLK			1
#define CLK_SCU_CLK			2
#define CLK_ACE_CLK			3
#define CLK_GIC_CLK			4
#define CLK_PERIPH_CLK			5
#define CLK_APCPU_CLK_NUM		(CLK_PERIPH_CLK + 1)

#define CLK_APAHB_CKG_EB		0
#define CLK_NANDC_EB			1
#define CLK_NANDC_ECC_EB		2
#define CLK_NANDC_26M_EB		3
#define CLK_DMA_EB			4
#define CLK_DMA_EB2			5
#define CLK_USB0_EB			6
#define CLK_USB0_SUSPEND_EB		7
#define CLK_USB0_REF_EB			8
#define CLK_SDIO_MST_EB			9
#define CLK_SDIO_MST_32K_EB		10
#define CLK_EMMC_EB			11
#define CLK_EMMC_32K_EB			12
#define CLK_AP_AHB_GATE_NUM		(CLK_EMMC_32K_EB + 1)

#define CLK_AP_AXI			0
#define CLK_PERI_APB			1
#define CLK_NANDC_ECC			2
#define CLK_USB0_REF			3
#define CLK_USB1_REF			4
#define CLK_PCIE_AUX			5
#define CLK_AP_UART0			6
#define CLK_AP_I2C0			7
#define CLK_AP_I2C1			8
#define CLK_AP_I2C2			9
#define CLK_AP_I2C3			10
#define CLK_AP_I2C4			11
#define CLK_AP_SIM			12
#define CLK_PWM0			13
#define CLK_PWM1			14
#define CLK_PWM2			15
#define CLK_PWM3			16
#define CLK_USB0_PIPE			17
#define CLK_USB0_UTMI			18
#define CLK_USB1_PIPE			19
#define CLK_USB1_UTMI			20
#define CLK_PCIE_PIPE			21
#define CLK_AP_CLK_NUM			(CLK_PCIE_PIPE + 1)

#define CLK_APAPB_REG_EB		0
#define CLK_AP_UART0_EB			1
#define CLK_AP_I2C0_EB			2
#define CLK_AP_I2C1_EB			3
#define CLK_AP_I2C2_EB			4
#define CLK_AP_I2C3_EB			5
#define CLK_AP_I2C4_EB			6
#define CLK_AP_APB_SPI0_EB		7
#define CLK_SPI0_LF_IN_EB		8
#define CLK_AP_APB_SPI1_EB		9
#define CLK_SPI1_IF_IN_EB		10
#define CLK_AP_APB_SPI2_EB		11
#define CLK_SPI2_IF_IN_EB		12
#define CLK_PWM0_EB			13
#define CLK_PWM1_EB			14
#define CLK_PWM2_EB			15
#define CLK_PWM3_EB			16
#define CLK_SIM0_EB			17
#define CLK_SIM0_32K_EB			18
#define CLK_AP_APB_GATE_NUM		(CLK_SIM0_32K_EB + 1)

#define CLK_IPA_USB1_EB			0
#define CLK_USB1_SUSPEND_EB		1
#define CLK_IPA_USB1_REF_EB		2
#define CLK_SDIO_SLV_EB			3
#define CLK_SD_SLV_FRUN_EB		4
#define CLK_PCIE_EB			5
#define CLK_PCIE_AUX_EB			6
#define CLK_IPA_EB			7
#define CLK_USB_PAM_EB			8
#define CLK_PCIE_SEL			9
#define CLK_IPA_GATE_NUM		(CLK_PCIE_SEL + 1)

#define CLK_RC100_CAL_EB		0
#define CLK_AON_SPI_EB			1
#define CLK_DJTAG_TCK_EB		2
#define CLK_DJTAG_EB			3
#define CLK_AUX0_EB			4
#define CLK_AUX1_EB			5
#define CLK_AUX2_EB			6
#define CLK_PROBE_EB			7
#define CLK_BSM_TMR_EB			8
#define CLK_AON_APB_BM_EB		9
#define CLK_PMU_APB_BM_EB		10
#define CLK_APCPU_CSSYS_EB		11
#define CLK_DEBUG_FILTER_EB		12
#define CLK_APCPU_DAP_EB		13
#define CLK_CSSYS_EB			14
#define CLK_CSSYS_APB_EB		15
#define CLK_CSSYS_PUB_EB		16
#define CLK_SD0_CFG_EB			17
#define CLK_SD0_REF_EB			18
#define CLK_SD1_CFG_EB			19
#define CLK_SD1_REF_EB			20
#define CLK_SD2_CFG_EB			21
#define CLK_SD2_REF_EB			22
#define CLK_SERDES0_EB			23
#define CLK_SERDES1_EB			24
#define CLK_SERDES2_EB			25
#define CLK_RTM_EB			26
#define CLK_RTM_ATB_EB			27
#define CLK_AON_NR_SPI_EB		28
#define CLK_AON_BM_S5_EB		29
#define CLK_EFUSE_EB			30
#define CLK_GPIO_EB			31
#define CLK_MBOX_EB			32
#define CLK_KPD_EB			33
#define CLK_AON_SYST_EB			34
#define CLK_AP_SYST_EB			35
#define CLK_AON_TMR_EB			36
#define CLK_DVFS_TOP_EB			37
#define CLK_APCPU_CLK_EB		38
#define CLK_SPLK_EB			39
#define CLK_PIN_EB			40
#define CLK_ANA_EB			41
#define CLK_AON_CKG_EB			42
#define CLK_DJTAG_CTRL_EB		43
#define CLK_APCPU_TS0_EB		44
#define CLK_NIC400_AON_EB		45
#define CLK_SCC_EB			46
#define CLK_AP_SPI0_EB			47
#define CLK_AP_SPI1_EB			48
#define CLK_AP_SPI2_EB			49
#define CLK_AON_BM_S3_EB		50
#define CLK_SC_CC_EB			51
#define CLK_THM0_EB			52
#define CLK_THM1_EB			53
#define CLK_AP_SIM_EB			54
#define CLK_AON_I2C_EB			55
#define CLK_PMU_EB			56
#define CLK_ADI_EB			57
#define CLK_EIC_EB			58
#define CLK_AP_INTC0_EB			59
#define CLK_AP_INTC1_EB			60
#define CLK_AP_INTC2_EB			61
#define CLK_AP_INTC3_EB			62
#define CLK_AP_INTC4_EB			63
#define CLK_AP_INTC5_EB			64
#define CLK_AUDCP_INTC_EB		65
#define CLK_AP_TMR0_EB			66
#define CLK_AP_TMR1_EB			67
#define CLK_AP_TMR2_EB			68
#define CLK_AP_WDG_EB			69
#define CLK_APCPU_WDG_EB		70
#define CLK_THM2_EB			71
#define CLK_ARCH_RTC_EB			72
#define CLK_KPD_RTC_EB			73
#define CLK_AON_SYST_RTC_EB		74
#define CLK_AP_SYST_RTC_EB		75
#define CLK_AON_TMR_RTC_EB		76
#define CLK_EIC_RTC_EB			77
#define CLK_EIC_RTCDV5_EB		78
#define CLK_AP_WDG_RTC_EB		79
#define CLK_AC_WDG_RTC_EB		80
#define CLK_AP_TMR0_RTC_EB		81
#define CLK_AP_TMR1_RTC_EB		82
#define CLK_AP_TMR2_RTC_EB		83
#define CLK_DCXO_LC_RTC_EB		84
#define CLK_BB_CAL_RTC_EB		85
#define CLK_DSI0_TEST_EB		86
#define CLK_DSI1_TEST_EB		87
#define CLK_DSI2_TEST_EB		88
#define CLK_DMC_REF_EN			89
#define CLK_TSEN_EN			90
#define CLK_TMR_EN			91
#define CLK_RC100_REF_EN		92
#define CLK_RC100_FDK_EN		93
#define CLK_DEBOUNCE_EN			94
#define CLK_DET_32K_EB			95
#define CLK_CSSYS_EN			96
#define CLK_SDIO0_2X_EN			97
#define CLK_SDIO0_1X_EN			98
#define CLK_SDIO1_2X_EN			99
#define CLK_SDIO1_1X_EN			100
#define CLK_SDIO2_2X_EN			101
#define CLK_SDIO2_1X_EN			102
#define CLK_EMMC_1X_EN			103
#define CLK_EMMC_2X_EN			104
#define CLK_NANDC_1X_EN			105
#define CLK_NANDC_2X_EN			106
#define CLK_ALL_PLL_TEST_EB		107
#define CLK_AAPC_TEST_EB		108
#define CLK_DEBUG_TS_EB			109
#define CLK_U2_0_REF_EN			110
#define CLK_U2_1_REF_EN			111
#define CLK_U3_0_REF_EN			112
#define CLK_U3_0_SUSPEND_EN		113
#define CLK_U3_1_REF_EN			114
#define CLK_U3_1_SUSPEND_EN		115
#define CLK_DSI0_REF_EN			116
#define CLK_DSI1_REF_EN			117
#define CLK_DSI2_REF_EN			118
#define CLK_PCIE_REF_EN			119
#define CLK_ACCESS_AUD_EN		120
#define CLK_AON_GATE_NUM		(CLK_ACCESS_AUD_EN + 1)

#define CLK_13M				0
#define CLK_6M5				1
#define CLK_4M				2
#define CLK_2M				3
#define CLK_1M				4
#define CLK_250K			5
#define CLK_RCO25M			6
#define CLK_RCO20M			7
#define CLK_RCO4M			8
#define CLK_RCO2M			9
#define CLK_AON_APB			10
#define CLK_ADI				11
#define CLK_AON_UART0			12
#define CLK_AON_I2C			13
#define CLK_EFUSE			14
#define CLK_TMR				15
#define CLK_THM0			16
#define CLK_THM1			17
#define CLK_THM2			18
#define CLK_PMU				19
#define CLK_APCPU_PMU			20
#define CLK_AUX0			21
#define CLK_AUX1			22
#define CLK_AUX2			23
#define CLK_PROBE			24
#define CLK_APCPU_DAP			25
#define CLK_APCPU_DAP_MTCK		26
#define CLK_DEBUG_TS			27
#define CLK_DSI0_TEST			28
#define CLK_DSI1_TEST			29
#define CLK_DSI2_TEST			30
#define CLK_DJTAG_TCK			31
#define CLK_DJTAG_TCK_HW		32
#define CLK_DEBOUNCE			33
#define CLK_SCC				34
#define CLK_TOP_DVFS			35
#define CLK_SDIO2_2X			36
#define CLK_SDIO2_1X			37
#define CLK_CSSYS			38
#define CLK_CSSYS_APB			39
#define CLK_APCPU_AXI			40
#define CLK_SDIO1_2X			41
#define CLK_SDIO1_1X			42
#define CLK_SDIO0_SLV			43
#define CLK_EMMC_2X			44
#define CLK_EMMC_1X			45
#define CLK_NANDC_2X			46
#define CLK_NANDC_1X			47
#define CLK_AP_SPI0			48
#define CLK_AP_SPI1			49
#define CLK_AP_SPI2			50
#define CLK_OTG2A_REF			51
#define CLK_U3A_SUSPEND_REF		52
#define CLK_OTG2B_REF			53
#define CLK_U3B_SUSPEND_REF		54
#define CLK_ANALOG_IO			55
#define CLK_DMC_REF			56
#define CLK_EMC				57
#define CLK_SC_CC			58
#define CLK_PMU_26M			59
#define CLK_AON_CLK_NUM			(CLK_PMU_26M + 1)

#define CLK_AUDCP_WDG_EB		0
#define CLK_AUDCP_RTC_WDG_EB		1
#define CLK_AUDCP_TMR0_EB		2
#define CLK_AUDCP_TMR1_EB		3
#define CLK_AUDCP_APB_GATE_NUM		(CLK_AUDCP_TMR1_EB + 1)

#define CLK_AUDCP_IIS0_EB		0
#define CLK_AUDCP_IIS1_EB		1
#define CLK_AUDCP_IIS2_EB		2
#define CLK_AUDCP_IIS3_EB		3
#define CLK_AUDCP_UART_EB		4
#define CLK_AUDCP_DMA_CP_EB		5
#define CLK_AUDCP_DMA_AP_EB		6
#define CLK_AUDCP_SRC48K_EB		7
#define CLK_AUDCP_MCDT_EB		8
#define CLK_AUDCP_VBCIFD_EB		9
#define CLK_AUDCP_VBC_EB		10
#define CLK_AUDCP_SPLK_EB		11
#define CLK_AUDCP_ICU_EB		12
#define CLK_AUDCP_DMA_AP_ASHB_EB	13
#define CLK_AUDCP_DMA_CP_ASHB_EB	14
#define CLK_AUDCP_AUD_EB		15
#define CLK_AUDCP_VBC_24M_EB		16
#define CLK_AUDCP_TMR_26M_EB		17
#define CLK_AUDCP_DVFS_ASHB_EB		18
#define CLK_AUDCP_AHB_GATE_NUM		(CLK_AUDCP_DVFS_ASHB_EB + 1)

#define CLK_SP_UART0_EB                 0
#define CLK_SP_AHB_GATE_NUM             (CLK_SP_UART0_EB + 1)

#endif

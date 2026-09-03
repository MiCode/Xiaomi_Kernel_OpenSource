/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc SC7731E platform clocks
 *
 * Copyright (C) 2020, Unisoc Inc.
 */

#ifndef _DT_BINDINGS_CLK_SC7731E_H_
#define _DT_BINDINGS_CLK_SC7731E_H_

#define CLK_26M_AUD		0
#define CLK_13M			1
#define CLK_6M5			2
#define CLK_4M3			3
#define CLK_2M			4
#define CLK_1M			5
#define CLK_250K		6
#define	CLK_CPLL_GATE		7
#define	CLK_GPLL_GATE		8
#define	CLK_MPLL_GATE		9
#define	CLK_DPLL_GATE		10
#define	CLK_BBPLL_GATE		11
#define	CLK_PMU_APB_NUM		(CLK_BBPLL_GATE + 1)

#define CLK_TWPLL		0
#define CLK_TWPLL_768M		1
#define CLK_TWPLL_384M		2
#define CLK_TWPLL_192M		3
#define CLK_TWPLL_96M		4
#define CLK_TWPLL_48M		5
#define CLK_TWPLL_24M		6
#define CLK_TWPLL_12M		7
#define CLK_TWPLL_512M		8
#define CLK_TWPLL_256M		9
#define CLK_TWPLL_128M		10
#define CLK_TWPLL_64M		11
#define CLK_TWPLL_307M2		12
#define CLK_TWPLL_219M4		13
#define CLK_TWPLL_170M6		14
#define CLK_TWPLL_153M6		15
#define CLK_TWPLL_76M8		16
#define CLK_TWPLL_51M2		17
#define CLK_TWPLL_38M4		18
#define CLK_TWPLL_19M2		19
#define CLK_CPLL		20
#define CLK_CPPLL_800M		21
#define CLK_CPPLL_533M		22
#define CLK_CPPLL_400M		23
#define CLK_CPPLL_320M		24
#define CLK_CPPLL_266M		25
#define CLK_CPPLL_228M		26
#define CLK_CPPLL_200M		27
#define CLK_CPPLL_160M		28
#define CLK_CPPLL_133M		29
#define CLK_CPPLL_100M		30
#define CLK_CPPLL_50M		31
#define CLK_CPPLL_40M		32
#define CLK_MPLL		33
#define CLK_DPLL		34
#define CLK_GPLL		35
#define	CLK_BBPLL_416M		36
#define CLK_AON_PLL_NUM		(CLK_BBPLL_416M + 1)

#define CLK_DSI_EB		0
#define CLK_DISPC_EB		1
#define CLK_GSP_EB		2
#define CLK_OTG_EB		3
#define CLK_DMA_EB		4
#define CLK_CE_EB		5
#define CLK_SDIO0_EB		6
#define CLK_NANDC_EB		7
#define CLK_EMMC_EB		8
#define CLK_CE_SEC_EB		9
#define CLK_EMMC_32K_EB		10
#define CLK_SDIO0_32K_EB	11
#define CLK_NANDC_ECC_EB	12
#define CLK_MCU			13
#define CLK_CA7_AXI		14
#define CLK_CA7_DBG		15
#define CLK_AP_AHB_GATE_NUM	(CLK_CA7_DBG + 1)

#define CLK_AP_AXI		0
#define CLK_AP_AHB		1
#define CLK_AP_APB		2
#define CLK_GSP			3
#define CLK_DISPC0		4
#define CLK_DISPC0_DPI		5
#define CLK_DSI_RXSEC		6
#define CLK_DLANEBYTE		7
#define CLK_OTG_UTMI		8
#define CLK_AP_UART0		9
#define CLK_AP_UART1		10
#define CLK_AP_I2C0		11
#define CLK_AP_I2C1		12
#define CLK_AP_I2C2		13
#define CLK_AP_SPI0		14
#define CLK_AP_IIS0		15
#define CLK_AP_CE		16
#define CLK_NANDC_ECC		17
#define CLK_AP_CLK_NUM		(CLK_NANDC_ECC + 1)

#define CLK_EMC			0
#define CLK_AON_APB		1
#define CLK_ADI			2
#define CLK_PWM0		3
#define CLK_PWM1		4
#define CLK_PWM2		5
#define CLK_PWM3		6
#define CLK_AON_THM		7
#define CLK_AON_I2C0		8
#define CLK_AVS			9
#define CLK_AUDIF		10
#define CLK_IIS_DA0		11
#define CLK_IIS0_AD0		12
#define CLK_IIS1_AD0		13
#define CLK_CPU_DAP		14
#define CLK_CDAP_MTCK		15
#define CLK_CPU_TS		16
#define CLK_DJTAG_TCK		17
#define CLK_EMC_REF		18
#define CLK_CSSYS		19
#define CLK_CSSYS_CA7		20
#define CLK_SDIO0_2X		21
#define CLK_NANDC_2X		22
#define CLK_EMMC_2X		23
#define CLK_AP_HS_SPI		24
#define CLK_SDPHY_APB		25
#define CLK_ANALOG_APB		26
#define CLK_IO_APB		27
#define CLK_DTCK_HW		28
#define CLK_AON_CLK_NUM		(CLK_DTCK_HW + 1)

#define CLK_GPIO_EB		0
#define CLK_PWM0_EB		1
#define CLK_PWM1_EB		2
#define CLK_PWM2_EB		3
#define CLK_PWM3_EB		4
#define CLK_KPD_EB		5
#define CLK_AON_SYST_EB		6
#define CLK_AP_SYST_EB		7
#define CLK_AON_TMR_EB		8
#define CLK_AP_TMR0_EB		9
#define CLK_EFUSE_EB		10
#define CLK_EIC_EB		11
#define CLK_INTC_EB		12
#define CLK_ADI_EB		13
#define CLK_AUDIF_EB		14
#define CLK_AUD_EB		15
#define CLK_VBC_EB		16
#define CLK_PIN_EB		17
#define CLK_SPLK_EB		18
#define CLK_AP_WDG_EB		19
#define CLK_MM_EB		20
#define CLK_AON_APB_CKG_EB	21
#define CLK_GPU_EB		22
#define CLK_CA53_TS0_EB		23
#define CLK_CA53_TS1_EB		24
#define CLK_CA53_DAP_EB		25
#define CLK_I2C_EB		26
#define CLK_PMU_EB		27
#define CLK_THM_EB		28
#define CLK_AUX0_EB		29
#define CLK_AUX1_EB		30
#define CLK_AUX2_EB		31
#define CLK_PROBE_EB		32
#define CLK_AVS_EB		33
#define CLK_EMC_REF_EB		34
#define CLK_CA53_WDG_EB		35
#define CLK_AP_TMR1_EB		36
#define CLK_AP_TMR2_EB		37
#define CLK_DISP_EMC_EB		38
#define CLK_GSP_EMC_EB		39
#define CLK_MM_VSP_EB		40
#define CLK_MDAR_EB		41
#define CLK_RTC4M0_CAL_EB	42
#define CLK_DJTAG_EB		43
#define CLK_MBOX_EB		44
#define CLK_AON_DMA_EB		45
#define CLK_CM4_DJTAG_EB	46
#define CLK_WCN_EB		47
#define CLK_AON_APB_DEF_EB	48
#define CLK_DBG_EB		49
#define CLK_DBG_EMC_EB		50
#define CLK_CROSS_TRIG_EB	51
#define CLK_SERDES_DPHY_EB	52
#define CLK_ARCH_RTC_EB		53
#define CLK_KPD_RTC_EB		54
#define CLK_AON_SYST_RTC_EB	55
#define CLK_AP_SYST_RTC_EB	56
#define CLK_AON_TMR_RTC_EB	57
#define CLK_AP_TMR0_RTC_EB	58
#define CLK_EIC_RTC_EB		59
#define CLK_EIC_RTCDV5_EB	60
#define CLK_AP_WDG_RTC_EB	61
#define CLK_CA53_WDG_RTC_EB	62
#define CLK_THM_RTC_EB		63
#define CLK_ATHMA_RTC_EB	64
#define CLK_GTHMA_RTC_EB	65
#define CLK_ATHMA_RTC_A_EB	66
#define CLK_GTHMA_RTC_A_EB	67
#define CLK_AP_TMR1_RTC_EB	68
#define CLK_AP_TMR2_RTC_EB	69
#define CLK_DXCO_LC_RTC_EB	70
#define CLK_BB_CAL_RTC_EB	71
#define CLK_AUDIO_GATE		72
#define CLK_AUX0		73
#define CLK_CSSYS_EB		74
#define CLK_DMC_EB		75
#define CLK_PUB_REG_EB		76
#define CLK_ROSC_EB		77
#define CLK_S_D_CFG_EB		78
#define CLK_S_D_REF_EB		79
#define CLK_B_DMA_EB		80
#define CLK_ANLG_EB		81
#define CLK_PIN_APB_EB		82
#define CLK_ANLG_APB_EB		83
#define CLK_BSMTMR_EB		84
#define CLK_AP_DAP_EB		85
#define CLK_EMMC_1X_EB		86
#define CLK_EMMC_2X_EB		87
#define CLK_SDIO0_1X_EB		88
#define CLK_SDIO0_2X_EB		89
#define CLK_SDIO1_1X_EB		90
#define CLK_SDIO1_2X_EB		91
#define CLK_NANDC_1X_EB		92
#define CLK_NANDC_2X_EB		93
#define CLK_CSSYS_CA7_EB	94
#define CLK_AP_HS_SPI_EB	95
#define CLK_DET_32K_EB		96
#define CLK_TMR_EB		97
#define CLK_APLL_TEST_EB	98
#define CLK_AON_APB_GATE_NUM	(CLK_APLL_TEST_EB + 1)

#define CLK_GPU			0
#define CLK_GPU_CLK_NUM		(CLK_GPU + 1)

#define CLK_DCAM_EB		0
#define CLK_ISP_EB		1
#define CLK_VSP_EB		2
#define CLK_CSI_EB		3
#define CLK_JPG_EB		4
#define CLK_MM_CKG_EB		5
#define CLK_VSP_MQ_EB		6
#define CLK_MCPHY_CFG_EB	7
#define CLK_MSENSOR0_EB		8
#define CLK_MISP_AXI_EB		9
#define CLK_MDCAM_AXI_EB	10
#define CLK_MMIPI_CSI_EB	11
#define CLK_MM_GATE_NUM		(CLK_MMIPI_CSI_EB + 1)

#define CLK_MM_AHB		0
#define CLK_SENSOR0		1
#define CLK_DCAM_IF		2
#define CLK_VSP			3
#define CLK_ISP			4
#define CLK_JPG			5
#define CLK_MIPI_CSI		6
#define CLK_DCAM_AXI		7
#define CLK_ISP_AXI		8
#define CLK_MM_CLK_NUM		(CLK_ISP_AXI + 1)

#define CLK_SIM0_EB		0
#define CLK_IIS0_EB		1
#define CLK_SPI0_EB		2
#define CLK_I2C0_EB		3
#define CLK_I2C1_EB		4
#define CLK_I2C2_EB		5
#define CLK_UART0_EB		6
#define CLK_UART1_EB		7
#define CLK_SIM0_32K_EB		8
#define CLK_INTC0_EB		9
#define CLK_INTC1_EB		10
#define CLK_INTC2_EB		11
#define CLK_INTC3_EB		12
#define CLK_AP_APB_GATE_NUM	(CLK_INTC3_EB + 1)

#endif /* _DT_BINDINGS_CLK_SC7731E_H_ */

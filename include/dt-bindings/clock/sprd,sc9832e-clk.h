/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc SC9832E platform clocks
 *
 * Copyright (C) 2021, Unisoc Inc.
 */

#ifndef _DT_BINDINGS_CLK_SC9832_H_
#define _DT_BINDINGS_CLK_SC9832_H_

#define CLK_26M_AUD		0
#define CLK_13M			1
#define CLK_6M5			2
#define CLK_4M3			3
#define CLK_2M			4
#define CLK_1M			5
#define CLK_250K		6
#define CLK_ISPPLL_GATE		7
#define	CLK_MPLL_GATE		8
#define	CLK_DPLL_GATE		9
#define	CLK_LPLL_GATE		10
#define	CLK_GPLL_GATE		11
#define	CLK_PMU_GATE_NUM	(CLK_GPLL_GATE + 1)

#define	CLK_AUDIO_GATE		0
#define CLK_RPLL		1
#define CLK_RPLL_D1_EN		2
#define CLK_RPLL_390M		3
#define CLK_RPLL_260M		4
#define CLK_RPLL_195M		5
#define CLK_RPLL_26M		6
#define CLK_RPLL_NUM		(CLK_RPLL_26M + 1)

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
#define CLK_LPLL		20
#define CLK_LPLL_409M6		21
#define CLK_LPLL_245M76		22
#define CLK_GPLL		23
#define CLK_ISPPLL		24
#define CLK_ISPPLL_468M		25
#define CLK_PLL_NUM		(CLK_ISPPLL_468M + 1)

#define CLK_DPLL		0
#define CLK_DPLL_NUM		(CLK_DPLL + 1)

#define CLK_MPLL		0
#define CLK_MPLL_NUM		(CLK_MPLL + 1)

#define	CLK_AP_APB		0
#define	CLK_NANDC_ECC		1
#define	CLK_OTG_REF		2
#define CLK_OTG_UTMI		3
#define	CLK_UART1		4
#define	CLK_I2C0		5
#define	CLK_I2C1		6
#define	CLK_I2C2		7
#define	CLK_I2C3		8
#define	CLK_I2C4		9
#define	CLK_SPI0		10
#define	CLK_SPI2		11
#define	CLK_HS_SPI		12
#define	CLK_IIS0		13
#define	CLK_CE			14
#define CLK_NANDC_2X		15
#define CLK_SDIO0_2X		16
#define CLK_SDIO1_2X		17
#define CLK_EMMC_2X		18
#define CLK_VSP			19
#define CLK_GSP			20
#define CLK_DISPC0		21
#define CLK_DISPC0_DPI		22
#define CLK_DSI_RXESC		23
#define CLK_DSI_LANEBYTE	24
#define CLK_AP_CLK_NUM		(CLK_DSI_LANEBYTE + 1)

#define	CLK_AON_APB		0
#define	CLK_ADI			1
#define	CLK_PWM0		2
#define	CLK_PWM1		3
#define	CLK_PWM2		4
#define	CLK_PWM3		5
#define CLK_CM4_UART            6
#define CLK_THM0		7
#define CLK_THM1		8
#define	CLK_AUDIF		9
#define CLK_AUD_IIS_DA0		10
#define CLK_AUD_IIS_AD0		11
#define	CLK_CA53_DAP		12
#define CLK_CA53_DMTCK		13
#define	CLK_CA53_TS		14
#define	CLK_DJTAG_TCK		15
#define	CLK_EMC_REF		16
#define CLK_CSSYS		17
#define	CLK_TMR			18
#define	CLK_DSI_TEST		19
#define	CLK_SDPHY_APB		20
#define	CLK_AIO_APB		21
#define	CLK_DTCK_HW		22
#define	CLK_AP_MM		23
#define	CLK_AP_AXI		24
#define	CLK_NIC_GPU		25
#define	CLK_MM_ISP		26
#define CLK_AON_PREDIV_NUM	(CLK_MM_ISP + 1)

#define CLK_CM4_UART_EB		0
#define CLK_SPAHB_GATE_NUM	(CLK_CM4_UART_EB + 1)

#define CLK_ADC_EB		0
#define CLK_FM_EB		1
#define CLK_TPC_EB		2
#define CLK_GPIO_EB		3
#define CLK_PWM0_EB		4
#define CLK_PWM1_EB		5
#define CLK_PWM2_EB		6
#define CLK_PWM3_EB		7
#define CLK_KPD_EB		8
#define CLK_AON_SYST_EB		9
#define CLK_AP_SYST_EB		10
#define CLK_AON_TMR_EB		11
#define CLK_AP_TMR0_EB		12
#define CLK_EFUSE_EB		13
#define CLK_EIC_EB		14
#define CLK_INTC_EB		15
#define CLK_ADI_EB		16
#define CLK_AUDIF_EB		17
#define CLK_AUD_EB		18
#define CLK_VBC_EB		19
#define CLK_PIN_EB		20
#define CLK_IPI_EB		21
#define CLK_SPLK_EB		22
#define CLK_AP_WDG_EB		23
#define CLK_MM_EB		24
#define CLK_AON_APB_CKG_EB	25
#define CLK_GPU_EB		26
#define CLK_CA53_TS0_EB		27
#define CLK_WTLCP_INTC_EB	28
#define CLK_PUBCP_INTC_EB	29
#define CLK_CA53_DAP_EB		30
#define CLK_PMU_EB		31
#define CLK_THM0_EB		32
#define CLK_AUX0_EB		33
#define CLK_AUX1_EB		34
#define CLK_AUX2_EB		35
#define CLK_PROBE_EB		36
#define CLK_EMC_REF_EB		37
#define CLK_CA53_WDG_EB		38
#define CLK_AP_TMR1_EB		39
#define CLK_AP_TMR2_EB		40
#define CLK_DISP_EMC_EB		41
#define CLK_ZIP_EMC_EB		42
#define CLK_GSP_EMC_EB		43
#define CLK_MM_VSP_EB		44
#define CLK_MDAR_EB		45
#define CLK_DJTAG_EB		46
#define CLK_AON_INTC_EB		47
#define CLK_THM1_EB		48
#define CLK_MBOX_EB		49
#define CLK_AON_DMA_EB		50
#define CLK_L_PLL_D_EB		51
#define CLK_ORP_JTAG_EB		52
#define CLK_DBG_EB		53
#define CLK_DBG_EMC_EB		54
#define CLK_CROSS_TRIG_EB	55
#define CLK_SERDES_DPHY_EB	56
#define CLK_ARCH_RTC_EB		57
#define CLK_KPD_RTC_EB		58
#define CLK_AON_SYST_RTC_EB	59
#define CLK_AP_SYST_RTC_EB	60
#define CLK_AON_TMR_RTC_EB	61
#define CLK_AP_TMR0_RTC_EB	62
#define CLK_EIC_RTC_EB		63
#define CLK_EIC_RTCDV5_EB	64
#define CLK_AP_WDG_RTC_EB	65
#define CLK_CA53_WDG_RTC_EB	66
#define CLK_THM_RTC_EB		67
#define CLK_ATHMA_RTC_EB	68
#define CLK_GTHMA_RTC_EB	69
#define CLK_ATHMA_RTC_A_EB	70
#define CLK_GTHMA_RTC_A_EB	71
#define CLK_AP_TMR1_RTC_EB	72
#define CLK_AP_TMR2_RTC_EB	73
#define CLK_DXCO_LC_RTC_EB	74
#define CLK_BB_CAL_RTC_EB	75
#define CLK_AUX0		76
#define CLK_AUX1		77
#define CLK_AUX2		78
#define CLK_CSSYS_EB		79
#define CLK_DMC_EB		80
#define CLK_ROSC_EB		81
#define CLK_S_D_CFG_EB		82
#define CLK_S_D_REF_EB		83
#define CLK_B_DMA_EB		84
#define CLK_ANLG_EB		85
#define CLK_PIN_APB_EB		86
#define CLK_ANLG_APB_EB		87
#define CLK_BSMTMR_EB		88
#define CLK_AP_DAP_EB		89
#define CLK_APSIM_AONTOP_EB     90
#define CLK_TSEN_EB		91
#define CLK_CSSYS_CA53_EB	92
#define CLK_AP_HS_SPI_EB	93
#define CLK_DET_32K_EB		94
#define CLK_TMR_EB		95
#define CLK_APLL_TEST_EB	96
#define CLK_DJTAG_TCK_EB	97
#define CLK_AON_APB_GATE_NUM	(CLK_DJTAG_TCK_EB + 1)

#define CLK_GPU			0
#define CLK_GPU_CLK_NUM		(CLK_GPU + 1)

#define CLK_DCAM_EB		0
#define CLK_ISP_EB		1
#define CLK_CPP_EB		2
#define CLK_CSI_EB		3
#define CLK_CSI_S_EB		4
#define CLK_JPG_EB		5
#define CLK_MAHB_CKG_EB		6
#define CLK_CPHY_CFG_EB	        7
#define CLK_SENSOR0_EB		8
#define CLK_SENSOR1_EB		9
#define CLK_ISP_AXI_EB		10
#define CLK_MIPI_CSI_EB	        11
#define CLK_MIPI_CSI_S_EB	12
#define CLK_MM_GATE_NUM		(CLK_MIPI_CSI_S_EB + 1)

#define CLK_MM_AHB		0
#define CLK_SENSOR0		1
#define CLK_SENSOR1		2
#define CLK_DCAM_IF		3
#define CLK_JPG			4
#define CLK_MIPI_CSI		5
#define CLK_MCSI_S		6
#define CLK_MM_CLK_NUM		(CLK_MCSI_S + 1)

#define CLK_DSI_EB		0
#define CLK_DISPC_EB		1
#define CLK_VSP_EB		2
#define CLK_GSP_EB		3
#define CLK_OTG_EB		4
#define CLK_DMA_PUB_EB		5
#define CLK_CE_PUB_EB		6
#define CLK_AHB_CKG_EB		7
#define CLK_SDIO0_EB		8
#define CLK_SDIO1_EB		9
#define CLK_NANDC_EB		10
#define CLK_EMMC_EB		11
#define CLK_SPINLOCK_EB		12
#define CLK_CE_EFUSE_EB		13
#define CLK_EMMC_32K_EB		14
#define CLK_SDIO0_32K_EB	15
#define CLK_SDIO1_32K_EB	16
#define CLK_MCU			17
#define CLK_APAHB_GATE_NUM	(CLK_MCU + 1)

#define CLK_SIM0_EB		0
#define CLK_IIS0_EB		1
#define CLK_APB_REG_EB          2
#define CLK_SPI0_EB		3
#define CLK_SPI2_EB		4
#define CLK_I2C0_EB		5
#define CLK_I2C1_EB		6
#define CLK_I2C2_EB		7
#define CLK_I2C3_EB		8
#define CLK_I2C4_EB		9
#define CLK_UART1_EB		10
#define CLK_SIM0_32K_EB		11
#define CLK_INTC0_EB		12
#define CLK_INTC1_EB		13
#define CLK_INTC2_EB		14
#define CLK_INTC3_EB		15
#define CLK_AP_APB_GATE_NUM	(CLK_INTC3_EB + 1)

#endif /* _DT_BINDINGS_CLK_SC9832E_H_ */

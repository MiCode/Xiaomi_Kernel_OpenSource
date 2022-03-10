/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef _CLK_MT6789_FMETER_H
#define _CLK_MT6789_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define FM_AXI_CK				1
#define FM_SPM_CK				2
#define FM_SCP_CK				3
#define FM_B					4
#define FM_DISP_CK				5
#define FM_MDP_CK				6
#define FM_IMG1_CK				7
#define FM_IPE_CK				9
#define FM_CAM_CK				11
#define FM_MFG_REF_CK				19
#define FM_CAMTG_CK				20
#define FM_CAMTG2_CK				21
#define FM_CAMTG3_CK				22
#define FM_CAMTG4_CK				23
#define FM_CAMTG5_CK				24
#define FM_CAMTG6_CK				25
#define FM_UART_CK				26
#define FM_SPI_CK				27
#define FM_MSDC5HCLK_CK				28
#define FM_MSDC50_0_CK				29
#define FM_MSDC30_1_CK				30
#define FM_AUDIO_CK				32
#define FM_AUD_INTBUS_CK			33
#define FM_PWRAP_ULPOSC_CK			34
#define FM_ATB_CK				35
#define FM_SSPM_CK				36
#define FM_SCAM_CK				38
#define FM_DISP_PWM_CK				39
#define FM_USB_CK				40
#define FM_I2C_CK				42
#define FM_SENINF_CK				43
#define FM_SENINF1_CK				44
#define FM_SENINF2_CK				45
#define FM_SENINF3_CK				46
#define FM_DXCC_CK				48
#define FM_AUD_ENGEN1_CK			49
#define FM_AUD_ENGEN2_CK			50
#define FM_AES_UFSFDE_CK			51
#define FM_U_CK					52
#define FM_AUD_1_CK				53
#define FM_AUD_2_CK				54
#define FM_DPMAIF_MAIN_CK			56
#define FM_VENC_CK				57
#define FM_VDEC_CK				58
#define FM_CAMTM_CK				59
#define FM_PWM_CK				60
#define FM_AUDIO_H_CK				61
#define FM_SPMI_MST_CK				62
#define FM_DVFSRC_CK				63
#define FM_CKGEN_NUM				64
/* ABIST Part */
#define FM_MDBRPPLL_CK				1
#define FM_APLL1_CK				2
#define FM_APLL2_CK				3
#define FM_APPLLGP_MON_FM_CK			4
#define FM_MDBPIPLL_CK				5
#define FM_ARMPLL_BL_CK				6
#define FM_NPUPLL_CK				7
#define FM_USBPLL_CK				8
#define FM_ARMPLL_LL_CK				10
#define FM_CCIPLL_CK				11
#define FM_CSI1A_DPHY_DELAYCAL_CK		14
#define FM_CSI1B_DPHY_DELAYCAL_CK		15
#define FM_CSI2A_DPHY_DELAYCAL_CK		16
#define FM_CSI2B_DPHY_DELAYCAL_CK		17
#define FM_MDVDSPPLL_CK				18
#define FM_MDMCUPLL_CK				19
#define FM_DSI0_LNTC_DSICLK			20
#define FM_DSI0_MPPLL_TST_CK			21
#define FM_MFG_CK				23
#define FM_MAINPLL_CK				24
#define FM_MDPLL_FS26M_CK			25
#define FM_MFGPLL_CK				26
#define FM_MMPLL_CK				27
#define FM_MMPLL_D3_CK				28
#define FM_MPLL_CK				29
#define FM_MSDCPLL_CK				30
#define FM_RCLRPLL_DIV4_CK			31
#define FM_RPHYPLL_DIV4_CK			33
#define FM_TVDPLL_CK				35
#define FM_ULPOSC2_CK				36
#define FM_ULPOSC_CK				37
#define FM_UNIVPLL_CK				38
#define FM_USB20_192M_CK			39
#define FM_USBPLL_192M_CK			40
#define FM_U_CLK2FREQ				41
#define FM_WBG_DIG_BPLL_CK			42
#define FM_WBG_DIG_WPLL_CK960			43
#define FMEM_AFT_CH0				44
#define FMEM_AFT_CH1				45
#define FM_MSDC31_IN_CK				46
#define FM_MSDC32_IN_CK				47
#define FMEM_BFE_CH0				48
#define FMEM_BFE_CH1				49
#define FM_466M_FMEM_INFRASYS			50
#define FM_MCUSYS_ARM_OUT_ALL			51
#define FM_MSDC21_IN_CK				52
#define FM_MSDC22_IN_CK				53
#define FM_MSDC11_IN_CK				54
#define FM_MSDC12_IN_CK				55
#define FM_OSC_SYNC_2				56
#define FM_OSC_SYNC_CK				57
#define FM_RTC32K_I				58
#define FM_CKMON1_CK				60
#define FM_CKMON2_CK				61
#define FM_CKMON3_CK				62
#define FM_CKMON4_CK				63
#define FM_ABIST_NUM				64
/* ABIST2 Part */
#define FM_APLL_I2S0_M_CK			1
#define FM_APLL_I2S1_M_CK			2
#define FM_APLL_I2S2_M_CK			3
#define FM_APLL_I2S3_M_CK			4
#define FM_APLL_I2S4_M_CK			5
#define FM_APLL_I2S4_B_CK			6
#define FM_APLL_I2S5_M_CK			7
#define FM_APLL_I2S6_M_CK			8
#define FM_APLL_I2S7_M_CK			9
#define FM_APLL_I2S8_M_CK			10
#define FM_APLL_I2S9_M_CK			11
#define FM_AES_MSDCFDE_CK			12
#define FM_MCUPM_CK				13
#define FM_UNIPLL_SES_CK			15
#define FM_ULPOSC_CK_2				16
#define FM_ULPOSC_CORE_CK			17
#define FM_SRCK_CK				18
#define FM_MAIN_H728M_CK			19
#define FM_MAIN_H546M_CK			20
#define FM_MAIN_H436P8M_CK			21
#define FM_MAIN_H364M_CK			22
#define FM_MAIN_H312M_CK			23
#define FM_UNIV_1248M_CK			24
#define FM_UNIV_832M_CK				25
#define FM_UNIV_624M_CK				26
#define FM_UNIV_499M_CK				27
#define FM_UNIV_416M_CK				28
#define FM_UNIV_356P6M_CK			29
#define FM_MMPLL_D3_CK_2			30
#define FM_MMPLL_D4_CK				31
#define FM_MMPLL_D5_CK				32
#define FM_MMPLL_D6_CK				33
#define FM_MMPLL_D7_CK				34
#define FM_MMPLL_D9_CK				35
#define FM_ALVTS_TO_PLLGP_MON_L1		52
#define FM_ALVTS_TO_PLLGP_MON_L2		53
#define FM_ALVTS_TO_PLLGP_MON_L3		54
#define FM_ALVTS_TO_PLLGP_MON_L4		55
#define FM_ALVTS_TO_PLLGP_MON_L5		56
#define FM_ABIST2_NUM				57

#endif /* _CLK_MT6789_FMETER_H */

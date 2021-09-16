/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef _CLK_MT6879_FMETER_H
#define _CLK_MT6879_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define FM_AXI_CK				1
#define FM_AXIP_CK				2
#define FM_AXI_U_CK				3
#define FM_B					4
#define FM_DISP0_CK				5
#define FM_MDP0_CK				6
#define FM_MMINFRA_CK				7
#define FM_MMUP_CK				8
#define FM_DSP_CK				9
#define FM_DSP1_CK				10
#define FM_DSP2_CK				11
#define FM_DSP3_CK				12
#define FM_DSP4_CK				13
#define FM_DSP5_CK				14
#define FM_DSP6_CK				15
#define FM_DSP7_CK				16
#define FM_IPU_IF_CK				17
#define FM_CAMTG_CK				18
#define FM_CAMTG2_CK				19
#define FM_CAMTG3_CK				20
#define FM_CAMTG4_CK				21
#define FM_CAMTG5_CK				22
#define FM_CAMTG6_CK				23
#define FM_SPI_CK				25
#define FM_MSDC_MACRO_CK			26
#define FM_MSDC30_1_CK				27
#define FM_AUDIO_CK				28
#define FM_AUD_INTBUS_CK			29
#define FM_ATB_CK				31
#define FM_DISP_PWM_CK				32
#define FM_USB_CK				33
#define FM_USB_XHCI_CK				34
#define FM_I2C_CK				35
#define FM_SENINF_CK				36
#define FM_SENINF1_CK				37
#define FM_SENINF2_CK				38
#define FM_SENINF3_CK				39
#define FM_DXCC_CK				40
#define FM_AUD_ENGEN1_CK			41
#define FM_AUD_ENGEN2_CK			42
#define FM_AES_UFSFDE_CK			43
#define FM_U_CK					44
#define FM_U_MBIST_CK				45
#define FM_AUD_1_CK				46
#define FM_AUD_2_CK				47
#define FM_ADSP_CK				48
#define FM_DPMAIF_MAIN_CK			49
#define FM_VENC_CK				50
#define FM_VDEC_CK				51
#define FM_PWM_CK				52
#define FM_AUDIO_H_CK				53
#define FM_MCUPM_CK				54
#define FM_MEM_SUB_CK				57
#define FM_MEM_SUBP_CK				58
#define FM_MEM_SUB_U_CK				59
#define FM_EMI_N_CK				60
#define FM_DSI_OCC_CK				62
#define FM_CCU_AHB_CK				63
#define FM_AP2CONN_HOST_CK			65
#define FM_MCU_ACP_CK				66
#define FM_DPI_CK				67
#define FM_IMG1_CK				68
#define FM_IPE_CK				69
#define FM_CAM_CK				70
#define FM_CCUSYS_CK				71
#define FM_CAMTM_CK				72
/* ABIST Part */
#define FM_ADSPPLL_CK				1
#define FM_APLL1_CK				2
#define FM_APLL2_CK				3
#define FM_APPLLGP_MON_FM_CK			4
#define FM_ARMPLL_BL_CKDIV_CK			7
#define FM_ARMPLL_LL_CKDIV_CK			9
#define FM_CCIPLL_CKDIV_CK			11
#define FM_CSI0A_DELAYCAL_CK			12
#define FM_CSI0B_DELAYCAL_CK			13
#define FM_CSI1A_DELAYCAL_CK			14
#define FM_CSI1B_DELAYCAL_CK			15
#define FM_CSI2A_DELAYCAL_CK			16
#define FM_CSI2B_DELAYCAL_CK			17
#define FM_CSI3A_DELAYCAL_CK			18
#define FM_CSI3B_DELAYCAL_CK			19
#define FM_DSI0_LNTC_DSICLK			20
#define FM_DSI0_MPPLL_TST_CK			21
#define FM_MAINPLL_CKDIV_CK			23
#define FM_MAINPLL_CK				24
#define FM_MDPLL1_FS26M_GUIDE			25
#define FM_MMPLL_CKDIV_CK			26
#define FM_MMPLL_CK				27
#define FM_MMPLL_D3_CK				28
#define FM_MPLL_CK				29
#define FM_MSDCPLL_CK				30
#define FM_RCLRPLL_DIV4_CH01			31
#define FM_IMGPLL_CK				32
#define FM_RPHYPLL_DIV4_CH01			33
#define FM_TVDPLL_CK				35
#define FM_ULPOSC2_MON_V_VCORE_CK		36
#define FM_ULPOSC_MON_VCORE_CK			37
#define FM_UNIVPLL_CK				38
#define FM_UNIVPLL_CKDIV_CK			40
#define FM_U_CLK2FREQ				41
#define FM_WBG_DIG_BPLL_CK			42
#define FM_WBG_DIG_WPLL_CK960			43
#define FMEM_AFT_CH0				44
#define FMEM_AFT_CH1				45
#define FMEM_BFE_CH0				48
#define FMEM_BFE_CH1				49
#define FM_466M_FMEM_INFRASYS			50
#define FM_MCUSYS_ARM_OUT_ALL			51
#define FM_MSDC1_IN_CK				54
#define FM_MSDC2_IN_CK				55
#define FM_F32K_VCORE_CK			58
#define FM_APU_OCC_TO_SOC			60
#define FM_ALVTS_T0_PLLGP_MON_L6		61
#define FM_ALVTS_T0_PLLGP_MON_L5		62
#define FM_ALVTS_T0_PLLGP_MON_L4		63
#define FM_ALVTS_T0_PLLGP_MON_L3		64
#define FM_ALVTS_T0_PLLGP_MON_L2		65
#define FM_ALVTS_T0_PLLGP_MON_L1		66
#define FM_ALVTS_T0_PLLGP_MON_LM		67
/* ABIST2 Part */
#define FM_CKMON4_CK				0
#define FM_CKMON3_CK				1
#define FM_CKMON2_CK				2
#define FM_CKMON1_CK				3
#define FM_PMSRCK_CK				4
#define FM_R0_OUT_FM				5
#define FM_ROSC_OUT_FREQ			6
#define FM_MMPLL_D4_CK				7
#define FM_MMPLL_D3_CK_2			8
#define FM_UNIV_624M_CK				9
#define FM_UNIV_832M_CK				10
#define FM_UNIV_1248M_CK			11
#define FM_MAIN_H436P8M_CK			12
#define FM_MAIN_H546M_CK			13
#define FM_MAIN_H728M_CK			14
#define FM_SPMI_MST_32K_CK			15
#define FM_AP2CONN_HOST_CK_2			16
#define FM_SRCK_CK				17
#define FM_ULPOSC_CORE_CK			18
#define FM_ULPOSC_CK				19
#define FM_UNIPLL_SES_CK			20
#define FM_APLL_I2S9_M_CK			21
#define FM_APLL_I2S8_M_CK			22
#define FM_AUD_ETDM_OUT1_M_CK			23
#define FM_AUD_ETDM_IN1_M_CK			24
#define FM_APLL_I2S5_M_CK			25
#define FM_APLL_I2S4_B_CK			26
#define FM_APLL_I2S4_M_CK			27
#define FM_APLL_I2S3_M_CK			28
#define FM_APLL_I2S2_M_CK			29
#define FM_APLL_I2S1_M_CK			30
#define FM_APLL_I2S0_M_CK			31
/* VLPCK Part */
#define FM_SCP_CK				0
#define FM_SPM_CK				1
#define FM_PWRAP_ULPOSC_CK			2
#define FM_DXCC_VLP_CK				4
#define FM_SPMI_P_MST_CK			5
#define FM_SPMI_M_MST_CK			6
#define FM_DVFSRC_CK				7
#define FM_PWM_VLP_CK				8
#define FM_AXI_VLP_CK				9
#define FM_DBGAO_26M_CK				10
#define FM_SYSTIMER_26M_CK			11
#define FM_SSPM_CK				12
#define FM_SSPM_F26M_CK				13
#define FM_SRCK_CK_2				15
#define FM_SRAMRC_CK				16
#define FM_SEJ_26M_CK				17
#define FM_MD_BUCK_CTRL_OSC26M_CK		18
#define FM_SSPM_ULPOSC_CK			19
#define FM_RTC_CK				21
#define FM_ULPOSC_CORE_CK_2			22
#define FM_ULPOSC_CK_2				23

enum fm_sys_id {
	FM_GPU_PLL_CTRL = 0,
	FM_APU_PLL_CTRL = 1,
	FM_SYS_NUM = 2,
};

#endif /* _CLK_MT6879_FMETER_H */

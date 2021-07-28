/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Wendell Lin <wendell.lin@mediatek.com>
 */

#ifndef _CLK_MT6983_FMETER_H
#define _CLK_MT6983_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define	hd_faxi_ck	1
#define	f_fperi_hd_faxi_ck	2
#define	f_fufs_hd_haxi_ck	3
#define	hd_fbus_aximem_ck	4
#define	hf_fdisp0_ck	5
#define	hf_fdisp1_ck	6
#define	hf_fmdp0_ck	7
#define	hf_fmdp1_ck	8
#define	f_fmminfra_ck	9
#define	f_fmmup_ck	10
#define	hf_fdsp_ck	11
#define	hf_fdsp1_ck	12
#define	hf_fdsp2_ck	13
#define	hf_fdsp3_ck	14
#define	hf_fdsp4_ck	15
#define	hf_fdsp5_ck	16
#define	hf_fdsp6_ck	17
#define	hf_fdsp7_ck	18
#define	hf_fipu_if_ck	19
#define	hf_fmfg_ref_ck	20
#define	hf_fmfgsc_ref_ck	21
#define	f_fcamtg_ck	22
#define	f_fcamtg2_ck	23
#define	f_fcamtg3_ck	24
#define	f_fcamtg4_ck	25
#define	f_fcamtg5_ck	26
#define	f_fcamtg6_ck	27
#define	f_fcamtg7_ck	28
#define	f_fcamtg8_ck	29
#define	f_fuart_ck	30
#define	hf_fspi_ck	31
#define	hf_fmsdc50_0_hclk_ck	32
#define	hf_fmsdc_macro_ck	33
#define	hf_fmsdc30_1_ck	34
#define	hf_fmsdc30_2_ck	35
#define	hf_faudio_ck	36
#define	hf_faud_intbus_ck	37
#define	f_fpwrap_ulposc_ck	38
#define	hf_fatb_ck	39
#define	hf_fdp_ck	40
#define	f_fdisp_pwm_ck	41
#define	f_fusb_top_ck	42
#define	f_fssusb_xhci_ck	43
#define	f_fusb_top_1p_ck	44
#define	f_fssusb_xhci_1p_ck	45
#define	f_fi2c_ck	46
#define	f_fseninf_ck	47
#define	f_fseninf1_ck	48
#define	f_fseninf2_ck	49
#define	f_fseninf3_ck	50
#define	f_fseninf4_ck	51
#define	f_fseninf5_ck	52
#define	hf_fdxcc_ck	53
#define	hf_faud_engen1_ck	54
#define	hf_faud_engen2_ck	55
#define	hf_faes_ufsfde_ck	56
#define	hf_fufs_ck	57
#define	f_fufs_mbist_ck	58
#define	f_fpextp_mbist_ck	59
#define	hf_faud_1_ck	60
#define	hf_faud_2_ck	61
#define	hf_fadsp_ck	62
#define	hf_fdpmaif_main_ck	63
#define	hf_fvenc_ck	64
#define	hf_fvdec_ck	65
#define	hf_fpwm_ck	66
#define	hf_faudio_h_ck	67
#define	hg_fmcupm_ck	68
#define	hf_fspmi_p_mst_ck	69
#define	hf_fspmi_m_mst_ck	70
#define	hf_ftl_ck	71
#define	hf_fmem_sub_ck	72
#define	f_fperi_hf_fmem_ck	73
#define	f_fufs_hf_fmem_ck	74
#define	hf_faes_msdcfde_ck	75
#define	hf_femi_n_ck	76
#define	hf_femi_s_ck	77
#define	hf_fdsi_occ_ck	78
#define	f_fdptx_ck	79
#define	hf_fccu_ahb_ck	80
#define	f_fap2conn_host_ck	81
#define	hf_fimg1_ck	82
#define	hf_fipe_ck	83
#define	hf_fcam_ck	84
#define	hf_fccusys_ck	85
#define	f_fcamtm_ck	86
#define	hf_fsflash_ck	87
#define	hf_fmcu_acp_ck	88

/* ABIST Part */
#define FM_ADSPPLL_CK				1
#define FM_APLL1_CK				2
#define FM_APLL2_CK				3
#define FM_APPLLGP_MON_FM_CK			4
#define FM_ARMPLL_BL_CKDIV_CK			7
#define FM_ARMPLL_LL_CKDIV_CK			9
#define FM_USBPLL_OPP_CK			10
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
#define FM_EMIPLL_CK				34
#define FM_TVDPLL_CK				35
#define FM_ULPOSC2_MON_V_VCORE_CK		36
#define FM_ULPOSC_MON_VCORE_CK			37
#define FM_UNIVPLL_CK				38
#define FM_USBPLL_CKDIV_CK			39
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
#define FM_APEINT_66M_CK			14
#define FM_SRCK_CK				15
#define FM_SRAMRC_CK				16
#define FM_SEJ_26M_CK				17
#define FM_MD_BUCK_CTRL_OSC26M_CK		18
#define FM_SSPM_ULPOSC_CK			19
#define FM_RTC_CK				21
#define FM_ULPOSC_CORE_CK			22
#define FM_ULPOSC_CK				23

enum fm_sys_id {
	FM_GPU_PLL_CTRL = 0,
	FM_APU_PLL_CTRL = 1,
	FM_SYS_NUM = 2,
};

#endif /* _CLK_MT6983_FMETER_H */

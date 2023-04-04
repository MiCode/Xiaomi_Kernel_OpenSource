/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef _CLK_MT6739_FMETER_H
#define _CLK_MT6739_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define hd_faxi_ck				1
#define hf_fddrphycfg_ck		2
#define hf_fmm_ck				3
#define hf_fmfg_ck				7
#define f_fcamtg_ck				8
#define f_fuart_ck				9
#define hf_fspi_ck				10
#define hf_fmsdc50_0_hclk_ck	12
#define hf_fmsdc50_0_ck			13
#define hf_fmsdc30_1_ck			14
#define hf_faudio_ck			17
#define hf_faud_intbus_ck		18
#define hf_fdbi0_ck				23
#define hf_fscam_ck				24
#define hf_faud_1_ck			25
#define f_fdisp_pwm_ck			27
#define hf_fnfi2x_ck			28
#define hf_fnfiecc_ck			29
#define f_fusb_top_ck			30
#define hg_fspm_ck				31
#define f_fi2c_ck				33
#define f_fseninf_ck			35
#define f_fdxcc_ck				36
#define hf_faud_engin1_ck		37
#define hf_faud_engin2_ck		38
#define f_fcamtg2_ck			41
#define hf_fnfi1x_ck			47
#define f_ufs_mp_sap_cfg_ck		48
#define f_ufs_tick1us_ck		49
#define hd_faxi_east_ck			50
#define hd_faxi_west_ck			51
#define hd_faxi_north_ck		52
#define hd_faxi_south_ck		53
#define hg_fmipicfg_tx_ck		54
#define fmem_ck_bfe_dcm_ch0		55
#define fmem_ck_aft_dcm_ch0		56
#define dramc_pll104m_ck		59
//abist
#define AD_CSI0_DELAY_TSTCLK	1
#define AD_CSI1_DELAY_TSTCLK	2
#define AD_MDBPIPLL_DIV3_CK		4
#define AD_MDBPIPLL_DIV7_CK		5
#define AD_MDBRPPLL_DIV6_CK		6
#define AD_UNIV_624M_CK			7
#define AD_MAIN_H546_CK			8
#define AD_MEMPLL_MONCLK		9
#define AD_MEMPLL2_MONCLK		10
#define AD_MEMPLL3_MONCLK		11
#define AD_MEMPLL4_MONCLK		12
#define AD_MDPLL_FS26M_CK		16
#define AD_ARMPLL_L_CK			20
#define AD_ARMPLL_LL_CK			22
#define AD_MAINPLL_CK			23
#define AD_UNIVPLL_CK			24
#define AD_MSDCPLL_CK			26
#define AD_MMPLL_CK				27
#define AD_APLL1_CK				28
#define AD_APPLLGP_TST_CK		30
#define AD_USB20_192M_CK		31
#define AD_UNIV_192M_CK			32
#define AD_VENCPLL_CK			34
#define AD_DSI0_MPPLL_TST_CK	35
#define AD_DSI0_LNTC_DSICLK		36
#define rtc32k_ck_i				39
#define mcusys_arm_clk_out_all	40
#define msdc01_in_ck			43
#define msdc02_in_ck			44
#define msdc11_in_ck			45
#define msdc12_in_ck			46
#define AD_MPLL_208M_CK			50
#define DA_USB20_48M_DIV_CK		54
#define DA_UNIV_48M_DIV_CK		55


enum fm_sys_id {
	FM_GPU_PLL_CTRL = 0,
	FM_APU_PLL_CTRL = 1,
	FM_SYS_NUM = 2,
};

#endif /* _CLK_MT6739_FMETER_H */

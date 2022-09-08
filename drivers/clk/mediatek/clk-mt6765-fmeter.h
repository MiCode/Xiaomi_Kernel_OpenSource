/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef _CLK_MT6765_FMETER_H
#define _CLK_MT6765_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define hd_faxi_ck				1
#define hf_fmem_ck				2
#define hf_fmm_ck				3
#define hf_fscp_ck				4
#define hf_fmfg_ref_ck			5
#define hf_fatb_ck				6
#define f_fcamtg_ck				7
#define f_fcamtg1_ck			8
#define f_fcamtg2_ck			9
#define f_fcamtg3_ck			10
#define f_fuart_ck              11
#define hf_fspi_ck              12
#define hf_fmsdc50_0_hclk_ck    13
#define hf_fmsdc50_0_ck			14
#define hf_fmsdc30_1_ck         15
#define hf_faudio_ck            16
#define hf_faud_intbus_ck       17
#define hf_faud_1_ck            18
#define hf_faud_engen1_ck       19
#define f_fdisp_pwm_ck          20
#define hf_fsspm_ck             21
#define hf_fdxcc_ck             22
#define f_fusb_top_ck           23
#define hf_fspm_ck				24
#define hf_fi2c_ck				25
#define f_fpwm_ck               26
#define f_fseninf_ck            27
#define hf_faes_fde_ck          28
#define f_fpwrap_ulposc_ck      29
#define f_fcamtm_ck             30
#define f_ufs_mp_sap_cfg_ck     48
#define f_ufs_tick1us_ck        49
#define hd_faxi_east_ck         50
#define hd_faxi_west_ck						51
#define hd_faxi_north_ck					52
#define hd_faxi_south_ck					53
#define hg_fmipicfg_tx_ck					54
#define fmem_ck_bfe_dcm_ch0					55
#define fmem_ck_aft_dcm_ch0					56
#define fmem_ck_bfe_dcm_ch1					57
#define fmem_ck_aft_dcm_ch1					58
//abist
#define AD_CSI0A_CDPHY_DELAYCAL_CK			1
#define AD_CSI0B_CDPHY_DELAYCAL_CK			2
#define UFS_MP_CLK2FREQ						3
#define AD_MDBPIPLL_DIV3_CK					4
#define AD_MDBPIPLL_DIV7_CK                 5
#define AD_MDBRPPLL_DIV6_CK                 6
#define AD_UNIV_624M_CK                     7
#define AD_MAIN_H546M_CK                    8
#define AD_MAIN_H364M_CK                    9
#define AD_MAIN_H218P4M_CK                  10
#define AD_MAIN_H156M_CK                    11
#define AD_UNIV_624M_CK_DUMMY               12
#define AD_UNIV_416M_CK                     13
#define AD_UNIV_249P6M_CK                   14
#define AD_UNIV_178P3M_CK					15
#define AD_MDPLL1_FS26M_CK                  16
#define AD_CSI1A_CDPHY_DELAYCAL_CK          17
#define AD_CSI1B_CDPHY_DELAYCAL_CK          18
#define AD_CSI2A_CDPHY_DELAYCAL_CK          19
#define AD_CSI2B_CDPHY_DELAYCAL_CK          20
#define AD_ARMPLL_L_CK                      21
#define AD_ARMPLL_CK						22
#define AD_MAINPLL_1092M_CK                 23
#define AD_UNIVPLL_1248M_CK                 24
#define AD_MFGPLL_CK                        25
#define AD_MSDCPLL_CK                       26
#define AD_MMPLL_CK                         27
#define AD_APLL1_196P608M_CK                28
#define AD_APPLLGP_TST_CK                   30
#define AD_USB20_192M_CK                    31
#define AD_VENCPLL_CK                       34
#define AD_DSI0_MPPLL_TST_CK                35
#define AD_DSI0_LNTC_DSICLK                 36
#define ad_ulposc1_ck                       37
#define ad_ulposc2_ck                       38
#define rtc32k_ck_i                         39
#define mcusys_arm_clk_out_all              40
#define AD_ULPOSC1_SYNC_CK                  41
#define AD_ULPOSC2_SYNC_CK                  42
#define msdc01_in_ck                        43
#define msdc02_in_ck                        44
#define msdc11_in_ck                        45
#define msdc12_in_ck                        46
#define AD_CCIPLL_CK                        49
#define AD_MPLL_208M_CK                     50
#define AD_WBG_DIG_CK_416M                  51
#define AD_WBG_B_DIG_CK_64M                 52
#define AD_WBG_W_DIG_CK_160M                53
#define DA_USB20_48M_DIV_CK                 54
#define DA_UNIV_48M_DIV_CK                  55
#define DA_MPLL_104M_DIV_CK					56
#define DA_MPLL_52M_DIV_CK                  57
#define DA_ARMCPU_MON_CK                    58
#define ckmon1_ck                           60
#define ckmon2_ck                           61
#define ckmon3_ck                           62
#define ckmon4_ck                           63


enum fm_sys_id {
	FM_GPU_PLL_CTRL = 0,
	FM_APU_PLL_CTRL = 1,
	FM_SYS_NUM = 2,
};

#endif /* _CLK_MT6765_FMETER_H */
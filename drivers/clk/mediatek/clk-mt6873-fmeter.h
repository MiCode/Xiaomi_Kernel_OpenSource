// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CLK_MT6873_FMETER_H
#define __CLK_MT6873_FMETER_H

/* generate from abist_fqmtr.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define hd_faxi_ck	1
#define hg_fspm_ck	2
#define hf_fscp_ck	3
#define hd_fbus_aximem_ck	4
#define hf_fdisp_ck	5
#define hf_fmdp_ck	6
#define hf_fimg1_ck	7
#define hf_fimg2_ck	8
#define hf_fipe_ck	9
#define hf_fdpe_ck	10
#define hf_fcam_ck	11
#define hf_fccu_ck	12
#define hf_fdsp_ck	13
#define hf_fdsp1_ck	14
#define hf_fdsp2_ck	15
#define hf_fdsp3_ck	16
#define hf_fdsp4_ck	17
#define hf_fdsp5_ck	18
#define hf_fdsp6_ck	19
#define hf_fdsp7_ck	20
#define hf_fipu_if_ck	21
#define hf_fmfg_ck	22
#define f_fcamtg_ck	23
#define f_fcamtg2_ck	24
#define f_fcamtg3_ck	25
#define f_fcamtg4_ck	26
#define f_fuart_ck	27
#define hf_fspi_ck	28
#define hf_fmsdc50_0_hclk_ck	29
#define hf_fmsdc50_0_ck	30
#define hf_fmsdc30_1_ck	31
#define hf_faudio_ck	32
#define hf_faud_intbus_ck	33
#define f_fpwrap_ulposc_ck	34
#define hf_fatb_ck	35
#define hf_fsspm_ck	36
#define hf_fdp_ck	37
#define hf_fscam_ck	38
#define f_fdisp_pwm_ck	39
#define f_fusb_top_ck	40
#define f_fssusb_xhci_ck	41
#define f_fi2c_ck	42
#define f_fseninf_ck	43
#define hg_mcupm_ck	44
#define hf_fspmi_mst_ck	45
#define hg_fdvfsrc_ck	46
#define hf_fdxcc_ck	47
#define hf_faud_engen1_ck	48
#define hf_faud_engen2_ck	49
#define hf_faes_ufsfde_ck	50
#define hf_fufs_ck	51
#define hf_faud_1_ck	52
#define hf_faud_2_ck	53
#define hf_fadsp_ck	54
#define hf_fdpmaif_main_ck	55
#define hf_fvenc_ck	56
#define hf_fvdec_ck	57
#define hf_fvdec_lat_ck	58
#define hf_fcamtm_ck	59
#define hf_fpwm_ck	60
#define hf_faudio_h_ck	61
#define f_fcamtg5_ck	62
#define f_fcamtg6_ck	63


/* Abist Part */
#define AD_ADSPPLL_CK	1
#define AD_APLL1_CK	2
#define AD_APLL2_CK	3
#define AD_APPLLGP_MON_FM_CK	4
#define AD_APUPLL_CK	5
#define AD_ARMPLL_BL0_CK	6
#define AD_ARMPLL_BL1_CK	7
#define AD_ARMPLL_BL2_CK	8
#define AD_ARMPLL_BL3_CK	9
#define AD_ARMPLL_LL_CK	10
#define AD_CCIPLL_CK	11
#define AD_CSI0A_CDPHY_DELAYCAL_CK	12
#define AD_CSI0B_CDPHY_DELAYCAL_CK	13
#define AD_CSI1A_DPHY_DELAYCAL_CK	14
#define AD_CSI1B_DPHY_DELAYCAL_CK	15
#define AD_CSI2A_DPHY_DELAYCAL_CK	16
#define AD_CSI2B_DPHY_DELAYCAL_CK	17
#define AD_CSI3A_DPHY_DELAYCAL_CK	18
#define AD_CSI3B_DPHY_DELAYCAL_CK	19
#define AD_DSI0_LNTC_DSICLK	20
#define AD_DSI0_MPPLL_TST_CK	21
#define AD_DSI1_LNTC_DSICLK	22
#define AD_DSI1_MPPLL_TST_CK	23
#define AD_MAINPLL_CK	24
#define AD_MDPLL1_FS26M_CK_guide	25
#define AD_MFGPLL_CK	26
#define AD_MMPLL_CK	27
#define AD_MMPLL_D3_CK	28
#define AD_MPLL_CK	29
#define AD_MSDCPLL_CK	30
#define AD_RCLRPLL_DIV4_CK_ch02	31
#define AD_RCLRPLL_DIV4_CK_ch13	32
#define AD_RPHYPLL_DIV4_CK_ch02	33
#define AD_RPHYPLL_DIV4_CK_ch13	34
#define AD_TVDPLL_CK	35
#define AD_ULPOSC2_CK	36
#define AD_ULPOSC_CK	37
#define AD_UNIVPLL_CK	38
#define AD_USB20_192M_CK	39
#define DA_MPLL_52M_DIV_CK	40
#define UFS_MP_CLK2FREQ	41
#define ad_wbg_dig_bpll_ck	42
#define ad_wbg_dig_wpll_ck960	43
#define fmem_ck_aft_dcm_ch0	44
#define fmem_ck_aft_dcm_ch1	45
#define fmem_ck_aft_dcm_ch2	46
#define fmem_ck_aft_dcm_ch3	47
#define fmem_ck_bfe_dcm_ch0	48
#define fmem_ck_bfe_dcm_ch1	49
#define hd_466m_fmem_ck_infrasys	50
#define mcusys_arm_clk_out_all	51
#define msdc01_in_ck	52
#define msdc02_in_ck	53
#define msdc11_in_ck	54
#define msdc12_in_ck	55
#define msdc21_in_ck	56
#define msdc22_in_ck	57
#define rtc32k_ck_i_vao	58
/* #define 1'b0	59 */
#define ckomo1_ck	60
#define ckmon2_ck	61
#define ckmon3_ck	62
#define ckmon4_ck	63

extern unsigned int mt_get_ckgen_freq(unsigned int ID);
extern unsigned int mt_get_abist_freq(unsigned int ID);
extern const struct fmeter_clk *get_fmeter_clks(void);

#endif

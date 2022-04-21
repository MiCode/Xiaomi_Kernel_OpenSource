// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <dt-bindings/power/mt6983-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6983.h"

#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0

/*
 * The clk names in Mediatek CCF.
 */

/* afe */
struct pd_check_swcg afe_swcgs[] = {
	SWCG("afe_afe"),
	SWCG("afe_22m"),
	SWCG("afe_24m"),
	SWCG("afe_apll2_tuner"),
	SWCG("afe_apll_tuner"),
	SWCG("afe_tdm_ck"),
	SWCG("afe_adc"),
	SWCG("afe_dac"),
	SWCG("afe_dac_predis"),
	SWCG("afe_tml"),
	SWCG("afe_nle"),
	SWCG("afe_general3_asrc"),
	SWCG("afe_connsys_i2s_asrc"),
	SWCG("afe_general1_asrc"),
	SWCG("afe_general2_asrc"),
	SWCG("afe_dac_hires"),
	SWCG("afe_adc_hires"),
	SWCG("afe_adc_hires_tml"),
	SWCG("afe_adda6_adc"),
	SWCG("afe_adda6_adc_hires"),
	SWCG("afe_adda7_adc"),
	SWCG("afe_adda7_adc_hires"),
	SWCG("afe_3rd_dac"),
	SWCG("afe_3rd_dac_predis"),
	SWCG("afe_3rd_dac_tml"),
	SWCG("afe_3rd_dac_hires"),
	SWCG("afe_i2s5_bclk"),
	SWCG("afe_i2s6_bclk"),
	SWCG("afe_i2s7_bclk"),
	SWCG("afe_i2s8_bclk"),
	SWCG("afe_i2s9_bclk"),
	SWCG("afe_etdm_in0_bclk"),
	SWCG("afe_etdm_out0_bclk"),
	SWCG("afe_i2s1_bclk"),
	SWCG("afe_i2s2_bclk"),
	SWCG("afe_i2s3_bclk"),
	SWCG("afe_i2s4_bclk"),
	SWCG("afe_etdm_in1_bclk"),
	SWCG("afe_etdm_out1_bclk"),
	SWCG(NULL),
};
/* camsys_mraw */
struct pd_check_swcg camsys_mraw_swcgs[] = {
	SWCG("cam_mr_larbx"),
	SWCG("cam_mr_camtg"),
	SWCG("cam_mr_mraw0"),
	SWCG("cam_mr_mraw1"),
	SWCG("cam_mr_mraw2"),
	SWCG("cam_mr_mraw3"),
	SWCG("cam_mr_pda0"),
	SWCG("cam_mr_pda1"),
	SWCG(NULL),
};
/* camsys_rawa */
struct pd_check_swcg camsys_rawa_swcgs[] = {
	SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG(NULL),
};
/* camsys_rawb */
struct pd_check_swcg camsys_rawb_swcgs[] = {
	SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG(NULL),
};
/* camsys_rawc */
struct pd_check_swcg camsys_rawc_swcgs[] = {
	SWCG("cam_rc_larbx"),
	SWCG("cam_rc_cam"),
	SWCG("cam_rc_camtg"),
	SWCG(NULL),
};
/* camsys_yuva */
struct pd_check_swcg camsys_yuva_swcgs[] = {
	SWCG("cam_ya_larbx"),
	SWCG("cam_ya_cam"),
	SWCG("cam_ya_camtg"),
	SWCG(NULL),
};
/* camsys_yuvb */
struct pd_check_swcg camsys_yuvb_swcgs[] = {
	SWCG("cam_yb_larbx"),
	SWCG("cam_yb_cam"),
	SWCG("cam_yb_camtg"),
	SWCG(NULL),
};
/* camsys_yuvc */
struct pd_check_swcg camsys_yuvc_swcgs[] = {
	SWCG("cam_yc_larbx"),
	SWCG("cam_yc_cam"),
	SWCG("cam_yc_camtg"),
	SWCG(NULL),
};
/* cam_main_r1a */
struct pd_check_swcg cam_main_r1a_swcgs[] = {
	SWCG("c_larb13_con"),
	SWCG("c_larb14_con"),
	SWCG("c_cam_con"),
	SWCG("c_cam_suba_con"),
	SWCG("c_cam_subb_con"),
	SWCG("c_cam_subc_con"),
	SWCG("c_cam_mraw_con"),
	SWCG("c_camtg_con"),
	SWCG("c_seninf_con"),
	SWCG("c_gcamsva_con"),
	SWCG("c_gcamsvb_con"),
	SWCG("c_gcamsvc_con"),
	SWCG("c_gcamsvd_con"),
	SWCG("c_gcamsve_con"),
	SWCG("c_gcamsvf_con"),
	SWCG("c_gcamsvg_con"),
	SWCG("c_gcamsvh_con"),
	SWCG("c_gcamsvi_con"),
	SWCG("c_gcamsvj_con"),
	SWCG("c_camsv_con"),
	SWCG("c_camsv_cq_a_con"),
	SWCG("c_camsv_cq_b_con"),
	SWCG("c_camsv_cq_c_con"),
	SWCG("c_adl_con"),
	SWCG("c_asg_con"),
	SWCG("c_pda0_con"),
	SWCG("c_pda1_con"),
	SWCG("c_pda2_con"),
	SWCG("c_fake_eng_con"),
	SWCG("c_cam2mm0_gals_con"),
	SWCG("c_cam2mm1_gals_con"),
	SWCG("c_cam2sys_gals_con"),
	SWCG(NULL),
};
/* ccu_main */
struct pd_check_swcg ccu_main_swcgs[] = {
	SWCG("ccu_larb19"),
	SWCG("ccu_ahb"),
	SWCG("ccusys_ccu0"),
	SWCG("ccusys_ccu1"),
	SWCG(NULL),
};
/* dip_nr_dip1 */
struct pd_check_swcg dip_nr_dip1_swcgs[] = {
	SWCG("dip_nr_dip1_larb15"),
	SWCG("dip_nr_dip1_dip_nr"),
	SWCG(NULL),
};
/* dip_top_dip1 */
struct pd_check_swcg dip_top_dip1_swcgs[] = {
	SWCG("dip_dip1_larb10"),
	SWCG("dip_dip1_dip_top"),
	SWCG(NULL),
};
/* dispsys_config */
struct pd_check_swcg dispsys_config_swcgs[] = {
	SWCG("dispsys_config_disp_mutex"),
	SWCG("dispsys_config_disp_ovl0"),
	SWCG("dispsys_config_disp_merge0"),
	SWCG("dispsys_config_disp_fake_eng0"),
	SWCG("dispsys_config_inlinerot"),
	SWCG("dispsys_config_disp_wdma0"),
	SWCG("dispsys_config_disp_fake_eng1"),
	SWCG("dispsys_config_disp_dpi0"),
	SWCG("dispsys_config_disp_ovl0_2l_nwcg"),
	SWCG("dispsys_config_disp_rdma0"),
	SWCG("dispsys_config_disp_rdma1"),
	SWCG("dispsys_config_disp_dli_async0"),
	SWCG("dispsys_config_disp_dli_async1"),
	SWCG("dispsys_config_disp_dli_async2"),
	SWCG("dispsys_config_disp_dlo_async0"),
	SWCG("dispsys_config_disp_dlo_async1"),
	SWCG("dispsys_config_disp_dlo_async2"),
	SWCG("dispsys_config_disp_rsz0"),
	SWCG("dispsys_config_disp_color0"),
	SWCG("dispsys_config_disp_ccorr0"),
	SWCG("dispsys_config_disp_ccorr1"),
	SWCG("dispsys_config_disp_aal0"),
	SWCG("dispsys_config_disp_gamma0"),
	SWCG("dispsys_config_disp_postmask0"),
	SWCG("dispsys_config_disp_dither0"),
	SWCG("dispsys_config_disp_cm0"),
	SWCG("dispsys_config_disp_spr0"),
	SWCG("dispsys_config_disp_dsc_wrap0"),
	SWCG("dispsys_config_disp_dsi0"),
	SWCG("dispsys_config_disp_ufbc_wdma0"),
	SWCG("dispsys_config_disp_wdma1"),
	SWCG("dispsys_config_disp_dp_intf0"),
	SWCG("dispsys_config_apb_bus"),
	SWCG("dispsys_config_disp_tdshp0"),
	SWCG("dispsys_config_disp_c3d0"),
	SWCG("dispsys_config_disp_y2r0"),
	SWCG("dispsys_config_disp_mdp_aal0"),
	SWCG("dispsys_config_disp_chist0"),
	SWCG("dispsys_config_disp_chist1"),
	SWCG("dispsys_config_disp_ovl0_2l"),
	SWCG("dispsys_config_disp_dli_async3"),
	SWCG("dispsys_config_disp_dlo_async3"),
	SWCG("dispsys_config_disp_ovl1_2l"),
	SWCG("dispsys_config_disp_ovl1_2l_nwcg"),
	SWCG("dispsys_config_smi_sub_comm"),
	SWCG("dispsys_config_dsi_clk"),
	SWCG("dispsys_config_dp_clk"),
	SWCG(NULL),
};
/* dispsys1_config */
struct pd_check_swcg dispsys1_config_swcgs[] = {
	SWCG("dispsys1_config_disp_mutex"),
	SWCG("dispsys1_config_disp_ovl0"),
	SWCG("dispsys1_config_disp_merge0"),
	SWCG("dispsys1_config_disp_fake_eng0"),
	SWCG("dispsys1_config_inlinerot"),
	SWCG("dispsys1_config_disp_wdma0"),
	SWCG("dispsys1_config_disp_fake_eng1"),
	SWCG("dispsys1_config_disp_dpi0"),
	SWCG("dispsys1_config_disp_ovl0_2l_nwcg"),
	SWCG("dispsys1_config_disp_rdma0"),
	SWCG("dispsys1_config_disp_rdma1"),
	SWCG("dispsys1_config_disp_dli_async0"),
	SWCG("dispsys1_config_disp_dli_async1"),
	SWCG("dispsys1_config_disp_dli_async2"),
	SWCG("dispsys1_config_disp_dlo_async0"),
	SWCG("dispsys1_config_disp_dlo_async1"),
	SWCG("dispsys1_config_disp_dlo_async2"),
	SWCG("dispsys1_config_disp_rsz0"),
	SWCG("dispsys1_config_disp_color0"),
	SWCG("dispsys1_config_disp_ccorr0"),
	SWCG("dispsys1_config_disp_ccorr1"),
	SWCG("dispsys1_config_disp_aal0"),
	SWCG("dispsys1_config_disp_gamma0"),
	SWCG("dispsys1_config_disp_postmask0"),
	SWCG("dispsys1_config_disp_dither0"),
	SWCG("dispsys1_config_disp_cm0"),
	SWCG("dispsys1_config_disp_spr0"),
	SWCG("dispsys1_config_disp_dsc_wrap0"),
	SWCG("dispsys1_config_disp_dsi0"),
	SWCG("dispsys1_config_disp_ufbc_wdma0"),
	SWCG("dispsys1_config_disp_wdma1"),
	SWCG("dispsys1_config_disp_dp_intf0"),
	SWCG("dispsys1_config_apb_bus"),
	SWCG("dispsys1_config_disp_tdshp0"),
	SWCG("dispsys1_config_disp_c3d0"),
	SWCG("dispsys1_config_disp_y2r0"),
	SWCG("dispsys1_config_disp_mdp_aal0"),
	SWCG("dispsys1_config_disp_chist0"),
	SWCG("dispsys1_config_disp_chist1"),
	SWCG("dispsys1_config_disp_ovl0_2l"),
	SWCG("dispsys1_config_disp_dli_async3"),
	SWCG("dispsys1_config_disp_dlo_async3"),
	SWCG("dispsys1_config_disp_ovl1_2l"),
	SWCG("dispsys1_config_disp_ovl1_2l_nwcg"),
	SWCG("dispsys1_config_smi_sub_comm"),
	SWCG("dispsys1_config_dsi_clk"),
	SWCG("dispsys1_config_dp_clk"),
	SWCG(NULL),
};
/* imgsys_main */
struct pd_check_swcg imgsys_main_swcgs[] = {
	SWCG("imgsys_main_larb9"),
	SWCG("imgsys_main_traw0"),
	SWCG("imgsys_main_traw1"),
	SWCG("imgsys_main_vcore_gals"),
	SWCG("imgsys_main_dip0"),
	SWCG("imgsys_main_wpe0"),
	SWCG("imgsys_main_ipe"),
	SWCG("imgsys_main_wpe1"),
	SWCG("imgsys_main_wpe2"),
	SWCG("imgsys_main_adl_larb"),
	SWCG("imgsys_main_adl_top0"),
	SWCG("imgsys_main_adl_top1"),
	SWCG("imgsys_main_gals"),
	SWCG(NULL),
};
/* ipesys */
struct pd_check_swcg ipesys_swcgs[] = {
	SWCG("ipesys_dpe"),
	SWCG("ipesys_fdvt"),
	SWCG("ipesys_me"),
	SWCG("ipesys_ipesys_top"),
	SWCG("ipesys_smi_larb12"),
	SWCG("ipesys_fdvt1"),
	SWCG(NULL),
};
/* mdpsys_config */
struct pd_check_swcg mdpsys_config_swcgs[] = {
	SWCG("mdp_mdp_mutex0"),
	SWCG("mdp_apb_bus"),
	SWCG("mdp_smi0"),
	SWCG("mdp_mdp_rdma0"),
	SWCG("mdp_mdp_fg0"),
	SWCG("mdp_mdp_hdr0"),
	SWCG("mdp_mdp_aal0"),
	SWCG("mdp_mdp_rsz0"),
	SWCG("mdp_mdp_tdshp0"),
	SWCG("mdp_mdp_color0"),
	SWCG("mdp_mdp_wrot0"),
	SWCG("mdp_mdp_fake_eng0"),
	SWCG("mdp_img_dl_relay0"),
	SWCG("mdp_img_dl_relay1"),
	SWCG("mdp_mdp_rdma1"),
	SWCG("mdp_mdp_fg1"),
	SWCG("mdp_mdp_hdr1"),
	SWCG("mdp_mdp_aal1"),
	SWCG("mdp_mdp_rsz1"),
	SWCG("mdp_mdp_tdshp1"),
	SWCG("mdp_mdp_color1"),
	SWCG("mdp_mdp_wrot1"),
	SWCG("mdp_mdp_rsz2"),
	SWCG("mdp_mdp_wrot2"),
	SWCG("mdp_mdp_dlo_async0"),
	SWCG("mdp_mdp_rsz3"),
	SWCG("mdp_mdp_wrot3"),
	SWCG("mdp_mdp_dlo_async1"),
	SWCG("mdp_hre_top_mdpsys"),
	SWCG(NULL),
};
/* mdpsys1_config */
struct pd_check_swcg mdpsys1_config_swcgs[] = {
	SWCG("mdpsys1_config_mdp_mutex0"),
	SWCG("mdpsys1_config_apb_bus"),
	SWCG("mdpsys1_config_smi0"),
	SWCG("mdpsys1_config_mdp_rdma0"),
	SWCG("mdpsys1_config_mdp_fg0"),
	SWCG("mdpsys1_config_mdp_hdr0"),
	SWCG("mdpsys1_config_mdp_aal0"),
	SWCG("mdpsys1_config_mdp_rsz0"),
	SWCG("mdpsys1_config_mdp_tdshp0"),
	SWCG("mdpsys1_config_mdp_color0"),
	SWCG("mdpsys1_config_mdp_wrot0"),
	SWCG("mdpsys1_config_mdp_fake_eng0"),
	SWCG("mdpsys1_config_img_dl_relay0"),
	SWCG("mdpsys1_config_img_dl_relay1"),
	SWCG("mdpsys1_config_mdp_rdma1"),
	SWCG("mdpsys1_config_mdp_fg1"),
	SWCG("mdpsys1_config_mdp_hdr1"),
	SWCG("mdpsys1_config_mdp_aal1"),
	SWCG("mdpsys1_config_mdp_rsz1"),
	SWCG("mdpsys1_config_mdp_tdshp1"),
	SWCG("mdpsys1_config_mdp_color1"),
	SWCG("mdpsys1_config_mdp_wrot1"),
	SWCG("mdpsys1_config_mdp_rsz2"),
	SWCG("mdpsys1_config_mdp_wrot2"),
	SWCG("mdpsys1_config_mdp_dlo_async0"),
	SWCG("mdpsys1_config_mdp_rsz3"),
	SWCG("mdpsys1_config_mdp_wrot3"),
	SWCG("mdpsys1_config_mdp_dlo_async1"),
	SWCG("mdpsys1_config_hre_top_mdpsys"),
	SWCG(NULL),
};
/* mminfra_config */
struct pd_check_swcg mminfra_config_swcgs[] = {
	SWCG("mminfra_config_gce_d"),
	SWCG("mminfra_config_gce_m"),
	SWCG("mminfra_config_smi"),
	SWCG("mminfra_config_gce_26m"),
	SWCG(NULL),
};
/* vdec_soc_gcon_base */
struct pd_check_swcg vdec_soc_gcon_base_swcgs[] = {
	SWCG("vde1_base_vdec_cken"),
	SWCG("vde1_base_vdec_active"),
	SWCG("vde1_base_vdec_cken_eng"),
	SWCG("vde1_base_mini_mdp_cken"),
	SWCG("vde1_base_lat_cken"),
	SWCG("vde1_base_lat_active"),
	SWCG("vde1_base_lat_cken_eng"),
	SWCG("vde1_base_larb1_cken"),
	SWCG(NULL),
};
/* vdec_gcon_base */
struct pd_check_swcg vdec_gcon_base_swcgs[] = {
	SWCG("vde2_base_vdec_cken"),
	SWCG("vde2_base_vdec_active"),
	SWCG("vde2_base_vdec_cken_eng"),
	SWCG("vde2_base_lat_cken"),
	SWCG("vde2_base_lat_active"),
	SWCG("vde2_base_lat_cken_eng"),
	SWCG("vde2_base_larb1_cken"),
	SWCG(NULL),
};
/* venc_gcon */
struct pd_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_cke0_larb"),
	SWCG("ven1_cke1_venc"),
	SWCG("ven1_cke2_jpgenc"),
	SWCG("ven1_cke3_jpgdec"),
	SWCG("ven1_cke4_jpgdec_c1"),
	SWCG("ven1_cke5_gals"),
	SWCG("ven1_cke6_gals_sram"),
	SWCG("ven1_core1_cke0_larb"),
	SWCG("ven1_core1_cke1_venc"),
	SWCG("ven1_core1_cke2_jpgenc"),
	SWCG("ven1_core1_cke3_jpgdec"),
	SWCG("ven1_core1_cke4_jpgdec_c1"),
	SWCG("ven1_core1_cke5_gals"),
	SWCG("ven1_core1_cke6_gals_sram"),
	SWCG(NULL),
};
/* venc_gcon_core1 */
struct pd_check_swcg venc_gcon_core1_swcgs[] = {
	SWCG("ven1_core1_cke0_larb"),
	SWCG("ven1_core1_cke1_venc"),
	SWCG("ven1_core1_cke2_jpgenc"),
	SWCG("ven1_core1_cke3_jpgdec"),
	SWCG("ven1_core1_cke4_jpgdec_c1"),
	SWCG("ven1_core1_cke5_gals"),
	SWCG("ven1_core1_cke6_gals_sram"),
	SWCG(NULL),
};
/* wpe1_dip1 */
struct pd_check_swcg wpe1_dip1_swcgs[] = {
	SWCG("wpe1_dip1_larb11"),
	SWCG("wpe1_dip1_wpe"),
	SWCG(NULL),
};
/* wpe2_dip1 */
struct pd_check_swcg wpe2_dip1_swcgs[] = {
	SWCG("wpe2_dip1_larb11"),
	SWCG("wpe2_dip1_wpe"),
	SWCG(NULL),
};
/* wpe3_dip1 */
struct pd_check_swcg wpe3_dip1_swcgs[] = {
	SWCG("wpe3_dip1_larb11"),
	SWCG("wpe3_dip1_wpe"),
	SWCG(NULL),
};

struct subsys_cgs_check {
	unsigned int pd_id;		/* power domain id */
	struct pd_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * chk_id is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

struct subsys_cgs_check mtk_subsys_check[] = {
	{MT6983_POWER_DOMAIN_AUDIO, afe_swcgs, afe},
	{MT6983_POWER_DOMAIN_CAM_MRAW, camsys_mraw_swcgs, cam_mr},
	{MT6983_POWER_DOMAIN_CAM_SUBA, camsys_rawa_swcgs, cam_ra},
	{MT6983_POWER_DOMAIN_CAM_SUBB, camsys_rawb_swcgs, cam_rb},
	{MT6983_POWER_DOMAIN_CAM_SUBC, camsys_rawc_swcgs, cam_rc},
	{MT6983_POWER_DOMAIN_CAM_SUBA, camsys_yuva_swcgs, cam_ya},
	{MT6983_POWER_DOMAIN_CAM_SUBB, camsys_yuvb_swcgs, cam_yb},
	{MT6983_POWER_DOMAIN_CAM_SUBC, camsys_yuvc_swcgs, cam_yc},
	{MT6983_POWER_DOMAIN_CAM_MAIN, cam_main_r1a_swcgs, cam_m},
	{MT6983_POWER_DOMAIN_CAM_MAIN, ccu_main_swcgs, ccu},
	{MT6983_POWER_DOMAIN_ISP_DIP1, dip_nr_dip1_swcgs, dip_nr_dip1},
	{MT6983_POWER_DOMAIN_ISP_DIP1, dip_top_dip1_swcgs, dip_top_dip1},
	{MT6983_POWER_DOMAIN_DIS0, dispsys_config_swcgs, disp},
	{MT6983_POWER_DOMAIN_DIS1, dispsys1_config_swcgs, disp1},
	{MT6983_POWER_DOMAIN_MDP0, mdpsys_config_swcgs, mdp},
	{MT6983_POWER_DOMAIN_MDP1, mdpsys1_config_swcgs, mdp1},
	{MT6983_POWER_DOMAIN_ISP_MAIN, imgsys_main_swcgs, img},
	{MT6983_POWER_DOMAIN_ISP_IPE, ipesys_swcgs, ipe},
	{MT6983_POWER_DOMAIN_MM_INFRA, mminfra_config_swcgs, mminfra_config},
	{MT6983_POWER_DOMAIN_VDE0, vdec_gcon_base_swcgs, vde2},
	{MT6983_POWER_DOMAIN_VDE1, vdec_soc_gcon_base_swcgs, vde1},
	{MT6983_POWER_DOMAIN_VEN0, venc_gcon_swcgs, ven1},
	{MT6983_POWER_DOMAIN_VEN1, venc_gcon_core1_swcgs, ven2},
	{MT6983_POWER_DOMAIN_ISP_DIP1, wpe1_dip1_swcgs, wpe1_dip1},
	{MT6983_POWER_DOMAIN_ISP_DIP1, wpe2_dip1_swcgs, wpe2_dip1},
	{MT6983_POWER_DOMAIN_ISP_DIP1, wpe3_dip1_swcgs, wpe3_dip1},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6983_POWER_DOMAIN_NR)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			return mtk_subsys_check[i].swcgs;
	}

	return NULL;
}

static void dump_subsys_reg(unsigned int id)
{
	int i;

	if (id >= MT6983_POWER_DOMAIN_NR)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6983(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6983_POWER_DOMAIN_MD1,
	MT6983_POWER_DOMAIN_CONN,
	//MT6983_POWER_DOMAIN_UFS0,
	MT6983_POWER_DOMAIN_MM_INFRA,
	MT6983_POWER_DOMAIN_MFG0_DORMANT,
	MT6983_POWER_DOMAIN_ADSP_INFRA,
	MT6983_POWER_DOMAIN_MM_PROC_DORMANT,
	MT6983_POWER_DOMAIN_ISP_VCORE,
	MT6983_POWER_DOMAIN_DIS0,
	MT6983_POWER_DOMAIN_DIS1,
	MT6983_POWER_DOMAIN_CAM_VCORE,
	MT6983_POWER_DOMAIN_MFG1,
	MT6983_POWER_DOMAIN_ADSP_TOP_DORMANT,
	MT6983_POWER_DOMAIN_AUDIO,
	MT6983_POWER_DOMAIN_ISP_MAIN,
	MT6983_POWER_DOMAIN_VDE0,
	MT6983_POWER_DOMAIN_VEN0,
	MT6983_POWER_DOMAIN_MDP0,
	MT6983_POWER_DOMAIN_VDE1,
	MT6983_POWER_DOMAIN_VEN1,
	MT6983_POWER_DOMAIN_MDP1,
	MT6983_POWER_DOMAIN_CAM_MAIN,
	MT6983_POWER_DOMAIN_MFG2,
	MT6983_POWER_DOMAIN_MFG3,
	MT6983_POWER_DOMAIN_MFG4,
	MT6983_POWER_DOMAIN_MFG5,
	MT6983_POWER_DOMAIN_MFG6,
	MT6983_POWER_DOMAIN_MFG7,
	MT6983_POWER_DOMAIN_MFG8,
	MT6983_POWER_DOMAIN_MFG9,
	MT6983_POWER_DOMAIN_MFG10,
	MT6983_POWER_DOMAIN_MFG11,
	MT6983_POWER_DOMAIN_MFG12,
	MT6983_POWER_DOMAIN_MFG13,
	MT6983_POWER_DOMAIN_MFG14,
	MT6983_POWER_DOMAIN_MFG15,
	MT6983_POWER_DOMAIN_MFG16,
	MT6983_POWER_DOMAIN_MFG17,
	MT6983_POWER_DOMAIN_MFG18,
	MT6983_POWER_DOMAIN_ISP_DIP1,
	MT6983_POWER_DOMAIN_ISP_IPE,
	MT6983_POWER_DOMAIN_CAM_MRAW,
	MT6983_POWER_DOMAIN_CAM_SUBA,
	MT6983_POWER_DOMAIN_CAM_SUBB,
	MT6983_POWER_DOMAIN_CAM_SUBC,
	MT6983_POWER_DOMAIN_APU,
	MT6983_POWER_DOMAIN_DP_TX,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6983_POWER_DOMAIN_NR)
		return false;

	for (i = 0; i < ARRAY_SIZE(pd_list); i++) {
		if (id == pd_list[i])
			return true;
	}

	return false;
}

static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	int i;

	print_subsys_reg_mt6983(spm);
	print_subsys_reg_mt6983(top);
	print_subsys_reg_mt6983(ifrao);
	print_subsys_reg_mt6983(apmixed);
	print_subsys_reg_mt6983(vlpcfg);
	print_subsys_reg_mt6983(vlp_ck);

	if (id >= MT6983_POWER_DOMAIN_NR)
		return;

	if (pwr_sta == PD_PWR_ON) {
		for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
			if (mtk_subsys_check[i].pd_id == id)
				print_subsys_reg_mt6983(mtk_subsys_check[i].chk_id);
		}
	}

	if ((id >= MT6983_POWER_DOMAIN_CAM_MRAW) &&
		(id <= MT6983_POWER_DOMAIN_CAM_SUBC))
		print_subsys_reg_mt6983(cam_m);

	BUG_ON(1);
}

static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6983_POWER_DOMAIN_NR)
		return;

	if (id == MT6983_POWER_DOMAIN_MD1) {
		print_subsys_reg_mt6983(ifrao);
		print_subsys_reg_mt6983(spm);
		print_subsys_reg_mt6983(vlpcfg);
	}
}

static struct pd_sta pd_pwr_msk[] = {
	{MT6983_POWER_DOMAIN_MD1, PWR_STA, 0x00000001},
	{MT6983_POWER_DOMAIN_CONN, PWR_STA, 0x00000002},
	{MT6983_POWER_DOMAIN_UFS0, PWR_STA, 0x00000010},
	{MT6983_POWER_DOMAIN_MM_INFRA, PWR_STA, 0x08000000},
	{MT6983_POWER_DOMAIN_MFG0_DORMANT, XPU_PWR_STA, 0x00000002},
	{MT6983_POWER_DOMAIN_ADSP_INFRA, PWR_STA, 0x00000080},
	{MT6983_POWER_DOMAIN_MM_PROC_DORMANT, PWR_STA, 0x10000000},
	{MT6983_POWER_DOMAIN_ISP_VCORE, PWR_STA, 0x00001000},
	{MT6983_POWER_DOMAIN_DIS0, PWR_STA, 0x02000000},
	{MT6983_POWER_DOMAIN_DIS1, PWR_STA, 0x04000000},
	{MT6983_POWER_DOMAIN_CAM_VCORE, PWR_STA, 0x00400000},
	{MT6983_POWER_DOMAIN_MFG1, XPU_PWR_STA, 0x00000004},
	{MT6983_POWER_DOMAIN_ADSP_TOP_DORMANT, PWR_STA, 0x00000040},
	{MT6983_POWER_DOMAIN_AUDIO, PWR_STA, 0x00000020},
	{MT6983_POWER_DOMAIN_ISP_MAIN, PWR_STA, 0x00000200},
	{MT6983_POWER_DOMAIN_VDE0, PWR_STA, 0x00002000},
	{MT6983_POWER_DOMAIN_VEN0, PWR_STA, 0x00008000},
	{MT6983_POWER_DOMAIN_MDP0, PWR_STA, 0x00800000},
	{MT6983_POWER_DOMAIN_VDE1, PWR_STA, 0x00004000},
	{MT6983_POWER_DOMAIN_VEN1, PWR_STA, 0x00010000},
	{MT6983_POWER_DOMAIN_MDP1, PWR_STA, 0x01000000},
	{MT6983_POWER_DOMAIN_CAM_MAIN, PWR_STA, 0x00020000},
	{MT6983_POWER_DOMAIN_MFG2, XPU_PWR_STA, 0x00000008},
	{MT6983_POWER_DOMAIN_MFG3, XPU_PWR_STA, 0x00000010},
	{MT6983_POWER_DOMAIN_MFG4, XPU_PWR_STA, 0x00000020},
	{MT6983_POWER_DOMAIN_MFG5, XPU_PWR_STA, 0x00000040},
	{MT6983_POWER_DOMAIN_MFG6, XPU_PWR_STA, 0x00000080},
	{MT6983_POWER_DOMAIN_MFG7, XPU_PWR_STA, 0x00000100},
	{MT6983_POWER_DOMAIN_MFG8, XPU_PWR_STA, 0x00000200},
	{MT6983_POWER_DOMAIN_MFG9, XPU_PWR_STA, 0x00000400},
	{MT6983_POWER_DOMAIN_MFG10, XPU_PWR_STA, 0x00000800},
	{MT6983_POWER_DOMAIN_MFG11, XPU_PWR_STA, 0x00001000},
	{MT6983_POWER_DOMAIN_MFG12, XPU_PWR_STA, 0x00002000},
	{MT6983_POWER_DOMAIN_MFG13, XPU_PWR_STA, 0x00004000},
	{MT6983_POWER_DOMAIN_MFG14, XPU_PWR_STA, 0x00008000},
	{MT6983_POWER_DOMAIN_MFG15, XPU_PWR_STA, 0x00010000},
	{MT6983_POWER_DOMAIN_MFG16, XPU_PWR_STA, 0x00020000},
	{MT6983_POWER_DOMAIN_MFG17, XPU_PWR_STA, 0x00040000},
	{MT6983_POWER_DOMAIN_MFG18, XPU_PWR_STA, 0x00080000},
	{MT6983_POWER_DOMAIN_ISP_DIP1, PWR_STA, 0x00000400},
	{MT6983_POWER_DOMAIN_ISP_IPE, PWR_STA, 0x00000800},
	{MT6983_POWER_DOMAIN_CAM_MRAW, PWR_STA, 0x00040000},
	{MT6983_POWER_DOMAIN_CAM_SUBA, PWR_STA, 0x00080000},
	{MT6983_POWER_DOMAIN_CAM_SUBB, PWR_STA, 0x00100000},
	{MT6983_POWER_DOMAIN_CAM_SUBC, PWR_STA, 0x00200000},
	{MT6983_POWER_DOMAIN_APU, OTHER_STA, 0x00000200},
	{MT6983_POWER_DOMAIN_DP_TX, PWR_STA, 0x20000000},
};

static struct pd_sta *get_pd_pwr_msk(int pd_id)
{
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_msk))
		return NULL;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_msk); i++) {
		if (pd_id == pd_pwr_msk[i].pd_id)
			return &pd_pwr_msk[pd_id];
	}

	return NULL;
}

static int off_mtcmos_id[] = {
	MT6983_POWER_DOMAIN_UFS0,
	MT6983_POWER_DOMAIN_MM_INFRA,
	MT6983_POWER_DOMAIN_MFG0_DORMANT,
	MT6983_POWER_DOMAIN_MM_PROC_DORMANT,
	MT6983_POWER_DOMAIN_ISP_VCORE,
	MT6983_POWER_DOMAIN_DIS0,
	MT6983_POWER_DOMAIN_DIS1,
	MT6983_POWER_DOMAIN_CAM_VCORE,
	MT6983_POWER_DOMAIN_MFG1,
	MT6983_POWER_DOMAIN_ISP_MAIN,
	MT6983_POWER_DOMAIN_VDE0,
	MT6983_POWER_DOMAIN_VEN0,
	MT6983_POWER_DOMAIN_MDP0,
	MT6983_POWER_DOMAIN_VDE1,
	MT6983_POWER_DOMAIN_VEN1,
	MT6983_POWER_DOMAIN_MDP1,
	MT6983_POWER_DOMAIN_CAM_MAIN,
	MT6983_POWER_DOMAIN_MFG2,
	MT6983_POWER_DOMAIN_MFG3,
	MT6983_POWER_DOMAIN_MFG4,
	MT6983_POWER_DOMAIN_MFG5,
	MT6983_POWER_DOMAIN_MFG6,
	MT6983_POWER_DOMAIN_MFG7,
	MT6983_POWER_DOMAIN_MFG8,
	MT6983_POWER_DOMAIN_MFG9,
	MT6983_POWER_DOMAIN_MFG10,
	MT6983_POWER_DOMAIN_MFG11,
	MT6983_POWER_DOMAIN_MFG12,
	MT6983_POWER_DOMAIN_MFG13,
	MT6983_POWER_DOMAIN_MFG14,
	MT6983_POWER_DOMAIN_MFG15,
	MT6983_POWER_DOMAIN_MFG16,
	MT6983_POWER_DOMAIN_MFG17,
	MT6983_POWER_DOMAIN_MFG18,
	MT6983_POWER_DOMAIN_ISP_DIP1,
	MT6983_POWER_DOMAIN_ISP_IPE,
	MT6983_POWER_DOMAIN_CAM_MRAW,
	MT6983_POWER_DOMAIN_CAM_SUBA,
	MT6983_POWER_DOMAIN_CAM_SUBB,
	MT6983_POWER_DOMAIN_CAM_SUBC,
	MT6983_POWER_DOMAIN_DP_TX,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6983_POWER_DOMAIN_MD1,
	MT6983_POWER_DOMAIN_CONN,
	MT6983_POWER_DOMAIN_ADSP_TOP_DORMANT,
	MT6983_POWER_DOMAIN_AUDIO,
	MT6983_POWER_DOMAIN_ADSP_INFRA,
	MT6983_POWER_DOMAIN_APU,
	PD_NULL,
};

static int *get_off_mtcmos_id(void)
{
	return off_mtcmos_id;
}

static int *get_notice_mtcmos_id(void)
{
	return notice_mtcmos_id;
}

static bool is_mtcmos_chk_bug_on(void)
{
#if BUG_ON_CHK_ENABLE
	return true;
#endif
	return false;
}

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6983_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.log_dump = log_dump,
	.get_pd_pwr_msk = get_pd_pwr_msk,
	.get_off_mtcmos_id = get_off_mtcmos_id,
	.get_notice_mtcmos_id = get_notice_mtcmos_id,
	.is_mtcmos_chk_bug_on = is_mtcmos_chk_bug_on,
};

static int pd_chk_mt6983_probe(struct platform_device *pdev)
{
	pdchk_common_init(&pdchk_mt6983_ops);

	return 0;
}

static struct platform_driver pd_chk_mt6983_drv = {
	.probe = pd_chk_mt6983_probe,
	.driver = {
		.name = "pd-chk-mt6983",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	static struct platform_device *pd_chk_dev;

	pd_chk_dev = platform_device_register_simple("pd-chk-mt6983", -1, NULL, 0);
	if (IS_ERR(pd_chk_dev))
		pr_notice("unable to register pd-chk device");

	return platform_driver_register(&pd_chk_mt6983_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6983_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");

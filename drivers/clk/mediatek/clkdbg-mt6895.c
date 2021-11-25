// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Ren-Ting Wang <ren-ting.wang@mediatek.com>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <clk-mux.h>
#include "clkdbg.h"
#include "clkchk.h"
#include "clk-fmeter.h"

const char * const *get_mt6895_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"axi_sel",
		"peri_axi_sel",
		"ufs_haxi_sel",
		"bus_aximem_sel",
		"disp0_sel",
		"disp1_sel",
		"mdp0_sel",
		"mdp1_sel",
		"mminfra_sel",
		"mmup_sel",
		"dsp_sel",
		"dsp1_sel",
		"dsp2_sel",
		"dsp3_sel",
		"dsp4_sel",
		"dsp5_sel",
		"dsp6_sel",
		"dsp7_sel",
		"mfg_ref_sel",
		"mfgsc_ref_sel",
		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"camtg6_sel",
		"camtg7_sel",
		"spi_sel",
		"msdc_macro_sel",
		"msdc30_1_sel",
		"msdc30_2_sel",
		"audio_sel",
		"aud_intbus_sel",
		"atb_sel",
		"dp_sel",
		"disp_pwm_sel",
		"usb_sel",
		"ssusb_xhci_sel",
		"usb_1p_sel",
		"ssusb_xhci_1p_sel",
		"i2c_sel",
		"seninf_sel",
		"seninf1_sel",
		"seninf2_sel",
		"seninf3_sel",
		"seninf4_sel",
		"dxcc_sel",
		"aud_engen1_sel",
		"aud_engen2_sel",
		"aes_ufsfde_sel",
		"ufs_sel",
		"ufs_mbist_sel",
		"pextp_mbist_sel",
		"aud_1_sel",
		"aud_2_sel",
		"adsp_sel",
		"dpmaif_main_sel",
		"venc_sel",
		"vdec_sel",
		"pwm_sel",
		"audio_h_sel",
		"mcupm_sel",
		"mem_sub_sel",
		"peri_mem_sel",
		"ufs_mem_sel",
		"emi_n_sel",
		"emi_s_sel",
		"dsi_occ_sel",
		"ccu_ahb_sel",
		"ap2conn_host_sel",
		"img1_sel",
		"ipe_sel",
		"cam_sel",
		"ccusys_sel",
		"camtm_sel",
		"mcu_acp_sel",
		"mfg_sel_0_sel",
		"mfg_sel_1_sel",
		"apll_i2s0_mck_sel",
		"apll_i2s1_mck_sel",
		"apll_i2s2_mck_sel",
		"apll_i2s3_mck_sel",
		"apll_i2s4_mck_sel",
		"apll_i2s5_mck_sel",
		"apll_i2s6_mck_sel",
		"apll_i2s7_mck_sel",
		"apll_i2s8_mck_sel",
		"apll_i2s9_mck_sel",

		/* topckgen */
		"apll12_div0",
		"apll12_div1",
		"apll12_div2",
		"apll12_div3",
		"apll12_div4",
		"apll12_divb",
		"apll12_div5",
		"apll12_div6",
		"apll12_div7",
		"apll12_div8",
		"apll12_div9",

		/* topckgen */
		"armpll_divider_pll1",
		"armpll_divider_pll2",
		"ufs_sap",
		"ufs_tick1us",
		"_md_32k",

		/* infracfg_ao */
		"ifrao_bus_hre",
		"ifrao_infra_force",
		"ifrao_therm",
		"ifrao_trng",
		"ifrao_cpum",
		"ifrao_ccif1_ap",
		"ifrao_ccif1_md",
		"ifrao_ccif_ap",
		"ifrao_debugsys",
		"ifrao_ccif_md",
		"ifrao_secore",
		"ifrao_dxcc_ao",
		"ifrao_dbg_trace",
		"ifrao_cldmabclk",
		"ifrao_cq_dma",
		"ifrao_ccif5_ap",
		"ifrao_ccif5_md",
		"ifrao_ccif2_ap",
		"ifrao_ccif2_md",
		"ifrao_ccif3_ap",
		"ifrao_ccif3_md",
		"ifrao_fbist2fpc",
		"ifrao_dapc_sync",
		"ifrao_dpmaif_main",
		"ifrao_ccif4_ap",
		"ifrao_ccif4_md",
		"ifrao_dpmaif_26m",
		"ifrao_mem_sub_ck",

		/* apmixedsys */
		"armpll_ll",
		"armpll_bl",
		"armpll_b",
		"ccipll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"adsppll",
		"tvdpll",
		"apll1",
		"apll2",
		"mpll",
		"imgpll",

		/* nemi_reg */
		"nemi_bus_mon_mode",

		/* semi_reg */
		"semi_bus_mon_mode",

		/* pericfg_ao */
		"peraop_0_uart0",
		"peraop_0_uart1",
		"peraop_0_uart2",
		"peraop_0_pwm_hclk",
		"peraop_0_pwm_bclk",
		"peraop_0_pwm_fbclk1",
		"peraop_0_pwm_fbclk2",
		"peraop_0_pwm_fbclk3",
		"peraop_0_pwm_fbclk4",
		"peraop_0_btif",
		"peraop_0_disp",
		"peraop_0_disp_h",
		"peraop_0_spi0_bclk",
		"peraop_0_spi1_bclk",
		"peraop_0_spi2_bclk",
		"peraop_0_spi3_bclk",
		"peraop_0_spi4_bclk",
		"peraop_0_spi5_bclk",
		"peraop_0_spi6_bclk",
		"peraop_0_spi7_bclk",
		"peraop_0_spi0_hclk",
		"peraop_0_spi1_hclk",
		"peraop_0_spi2_hclk",
		"peraop_0_spi3_hclk",
		"peraop_0_spi4_hclk",
		"peraop_0_spi5_hclk",
		"peraop_0_spi6_hclk",
		"peraop_1_spi7_hclk",
		"peraop_1_bus_hclk",
		"peraop_1_dma_bclk",
		"peraop_1_usb_pipe",
		"peraop_1_usb_ref",
		"peraop_1_usb_frmc",
		"peraop_1_usb_utmi",
		"peraop_1_usb_sys",
		"peraop_1_usb_xhci",
		"peraop_1_usb_133",
		"peraop_1_usb_66",
		"peraop_1_usb1p_ref",
		"peraop_1_usb1p_frmc",
		"peraop_1_usb1p_utmi",
		"peraop_1_usb1p_sys",
		"peraop_1_usb1p_xhci",
		"peraop_1_usb1p_133",
		"peraop_1_usb1p_66",
		"peraop_1_msdc1",
		"peraop_1_msdc1_hclk",
		"peraop_1_msdc2",
		"peraop_1_msdc2_hclk",
		"perao_p_2_conbus_mck",

		/* ssusb_device */
		"usb_d_dma_b",

		/* ssusb_sifslv_ippc */
		"usb_sif_usb_dma",
		"usb_sif_usb_u2_p",
		"usb_sif_usb_u3_p",

		/* ssusb_sifslv_ippc_p1 */
		"usb_sif_p1_usb_dma",
		"usb_sif_p1_usb_u2_p",
		"usb_sif_p1_usb_u3_p",

		/* imp_iic_wrap_c */
		"impc_ap_clock_i2c5",
		"impc_ap_clock_i2c6",

		/* ufs_ao_config */
		"ufsao_u_ao_0_tx_clk",
		"ufsao_u_ao_0_rx_clk0",
		"ufsao_u_ao_0_rx_clk1",
		"ufsao_u_ao_0_sysclk",
		"ufsao_u_ao_0_umpsap",
		"ufsao_u_ao_0_mmpsap",
		"ufsao_u_ao_0_mmpahb",
		"ufsao_u_ao_0_p2ufs",

		/* ufs_pdn_cfg */
		"ufspdn_u_0_hciufs",
		"ufspdn_u_0_hciaes",
		"ufspdn_u_0_hciahb",
		"ufspdn_u_0_hciaxi",

		/* imp_iic_wrap_s */
		"imps_ap_clock_i2c1",
		"imps_ap_clock_i2c2",
		"imps_ap_clock_i2c3",
		"imps_ap_clock_i2c4",
		"imps_ap_clock_i2c7",
		"imps_ap_clock_i2c8",
		"imps_ap_clock_i2c9",

		/* imp_iic_wrap_w */
		"impw_ap_clock_i2c0",

		/* mfgpll_pll_ctrl */
		"mfg_ao_mfgpll",

		/* mfgscpll_pll_ctrl */
		"mfgsc_ao_mfgscpll",

		/* mfg_top_config */
		"mfgcfg_bg3d",

		/* mmsys0_config */
		"mm0_disp_mutex0",
		"mm0_disp_ovl0",
		"mm0_disp_merge0",
		"mm0_disp_fake_eng0",
		"mm0_inlinerot0",
		"mm0_disp_wdma0",
		"mm0_disp_fake_eng1",
		"mm0_disp_dpi0",
		"mm0_disp_ovl0_2l_nw",
		"mm0_disp_rdma0",
		"mm0_disp_rdma1",
		"mm0_disp_dli_async0",
		"mm0_disp_dli_async1",
		"mm0_disp_dli_async2",
		"mm0_disp_dlo_async0",
		"mm0_disp_dlo_async1",
		"mm0_disp_dlo_async2",
		"mm0_disp_rsz0",
		"mm0_disp_color0",
		"mm0_disp_ccorr0",
		"mm0_disp_ccorr1",
		"mm0_disp_aal0",
		"mm0_disp_gamma0",
		"mm0_disp_postmask0",
		"mm0_disp_dither0",
		"mm0_disp_cm0",
		"mm0_disp_spr0",
		"mm0_disp_dsc_wrap0",
		"mm0_fmm_clk0",
		"mm0_disp_ufbc_wdma0",
		"mm0_disp_wdma1",
		"mm0_fmm_dp_clk",
		"mm0_apb_bus",
		"mm0_disp_tdshp0",
		"mm0_disp_c3d0",
		"mm0_disp_y2r0",
		"mm0_mdp_aal0",
		"mm0_disp_chist0",
		"mm0_disp_chist1",
		"mm0_disp_ovl0_2l",
		"mm0_disp_dli_async3",
		"mm0_disp_dlo_async3",
		"mm0_disp_ovl1_2l",
		"mm0_disp_ovl1_2l_nw",
		"mm0_smi_common",
		"mm0_clk",
		"mm0_dp_clk",
		"mm0_sig_emi",

		/* mmsys1_config */
		"mm1_disp_mutex0",
		"mm1_disp_ovl0",
		"mm1_disp_merge0",
		"mm1_disp_fake_eng0",
		"mm1_inlinerot0",
		"mm1_disp_wdma0",
		"mm1_disp_fake_eng1",
		"mm1_disp_dpi0",
		"mm1_disp_ovl0_2l_nw",
		"mm1_disp_rdma0",
		"mm1_disp_rdma1",
		"mm1_disp_dli_async0",
		"mm1_disp_dli_async1",
		"mm1_disp_dli_async2",
		"mm1_disp_dlo_async0",
		"mm1_disp_dlo_async1",
		"mm1_disp_dlo_async2",
		"mm1_disp_rsz0",
		"mm1_disp_color0",
		"mm1_disp_ccorr0",
		"mm1_disp_ccorr1",
		"mm1_disp_aal0",
		"mm1_disp_gamma0",
		"mm1_disp_postmask0",
		"mm1_disp_dither0",
		"mm1_disp_cm0",
		"mm1_disp_spr0",
		"mm1_disp_dsc_wrap0",
		"mm1_fmm_clk0",
		"mm1_disp_ufbc_wdma0",
		"mm1_disp_wdma1",
		"mm1_fmm_dp_clk",
		"mm1_apb_bus",
		"mm1_disp_tdshp0",
		"mm1_disp_c3d0",
		"mm1_disp_y2r0",
		"mm1_mdp_aal0",
		"mm1_disp_chist0",
		"mm1_disp_chist1",
		"mm1_disp_ovl0_2l",
		"mm1_disp_dli_async3",
		"mm1_disp_dlo_async3",
		"mm1_disp_ovl1_2l",
		"mm1_disp_ovl1_2l_nw",
		"mm1_smi_common",
		"mm1_clk",
		"mm1_dp_clk",
		"mm1_sig_emi",

		/* imgsys_main */
		"img_larb9",
		"img_traw0",
		"img_traw1",
		"img_vcore_gals",
		"img_dip0",
		"img_wpe0",
		"img_ipe",
		"img_wpe1",
		"img_wpe2",
		"img_adl_larb",
		"img_adl_top0",
		"img_adl_top1",
		"img_gals",
		"img_dip0_dummy",
		"img_wpe0_dummy",
		"img_ipe_dummy",
		"img_wpe1_dummy",
		"img_wpe2_dummy",

		/* dip_top_dip1 */
		"dip_dip1_larb10",
		"dip_dip1_dip_top",

		/* dip_nr_dip1 */
		"dip_nr_dip1_larb15",
		"dip_nr_dip1_dip_nr",

		/* wpe1_dip1 */
		"wpe1_dip1_larb11",
		"wpe1_dip1_wpe",

		/* ipesys */
		"ipe_dpe",
		"ipe_fdvt",
		"ipe_me",
		"ipesys_top",
		"ipe_smi_larb12",

		/* wpe2_dip1 */
		"wpe2_dip1_larb11",
		"wpe2_dip1_wpe",

		/* wpe3_dip1 */
		"wpe3_dip1_larb11",
		"wpe3_dip1_wpe",

		/* vdec_soc_gcon_base */
		"vde1_larb1_cken",
		"vde1_lat_cken",
		"vde1_lat_active",
		"vde1_lat_cken_eng",
		"vde1_mini_mdp_cken",
		"vde1_vdec_cken",
		"vde1_vdec_active",
		"vde1_vdec_cken_eng",

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_lat_cken",
		"vde2_lat_active",
		"vde2_lat_cken_eng",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven1_cke0_larb",
		"ven1_cke1_venc",
		"ven1_cke2_jpgenc",
		"ven1_cke3_jpgdec",
		"ven1_cke4_jpgdec_c1",
		"ven1_cke5_gals",
		"ven1_cke6_gals_sram",

		/* venc_gcon_core1 */
		"ven2_cke0_larb",
		"ven2_cke1_venc",
		"ven2_cke2_jpgenc",
		"ven2_cke3_jpgdec",
		"ven2_cke4_jpgdec_c1",
		"ven2_cke5_gals",
		"ven2_cke6_gals_sram",

		/* apupll_pll_ctrl */
		"apu0_ao_apupll",

		/* npupll_pll_ctrl */
		"npu_ao_npupll",

		/* apupll1_pll_ctrl */
		"apu1_ao_apupll1",

		/* vlp_cksys */
		"vlp_scp_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_dxcc_vlp_sel",
		"vlp_spmi_p_sel",
		"vlp_spmi_m_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_dbgao_26m_sel",
		"vlp_systimer_26m_sel",
		"vlp_sspm_sel",
		"vlp_sspm_f26m_sel",
		"vlp_srck_sel",
		"vlp_sramrc_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",

		/* cam_main_r1a */
		"cam_m_larb13_con",
		"cam_m_larb14_con",
		"cam_m_cam_con",
		"cam_m_cam_suba_con",
		"cam_m_cam_subb_con",
		"cam_m_cam_subc_con",
		"cam_m_cam_mraw_con",
		"cam_m_camtg_con",
		"cam_m_seninf_con",
		"cam_m_gcamsva_con",
		"cam_m_gcamsvb_con",
		"cam_m_gcamsvc_con",
		"cam_m_gcamsvd_con",
		"cam_m_gcamsve_con",
		"cam_m_gcamsvf_con",
		"cam_m_gcamsvg_con",
		"cam_m_gcamsvh_con",
		"cam_m_gcamsvi_con",
		"cam_m_gcamsvj_con",
		"cam_m_camsv_con",
		"cam_m_camsv_cq_a_con",
		"cam_m_camsv_cq_b_con",
		"cam_m_camsv_cq_c_con",
		"cam_m_adl_con",
		"cam_m_asg_con",
		"cam_m_pda0_con",
		"cam_m_pda1_con",
		"cam_m_pda2_con",
		"cam_m_fake_eng_con",
		"cam_m_cam2mm0_gcon",
		"cam_m_cam2mm1_gcon",
		"cam_m_cam2sys_gcon",
		"cam_m_cam_suba_con_dummy",
		"cam_m_cam_subb_con_dummy",
		"cam_m_cam_subc_con_dummy",
		"cam_m_cam_mraw_con_dummy",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",
		"cam_ra_larbx_dummy",

		/* camsys_yuva */
		"cam_ya_larbx",
		"cam_ya_cam",
		"cam_ya_camtg",
		"cam_ya_larbx_dummy",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",
		"cam_rb_larbx_dummy",

		/* camsys_yuvb */
		"cam_yb_larbx",
		"cam_yb_cam",
		"cam_yb_camtg",
		"cam_yb_larbx_dummy",

		/* camsys_rawc */
		"cam_rc_larbx",
		"cam_rc_cam",
		"cam_rc_camtg",
		"cam_rc_larbx_dummy",

		/* camsys_yuvc */
		"cam_yc_larbx",
		"cam_yc_cam",
		"cam_yc_camtg",
		"cam_yc_larbx_dummy",

		/* camsys_mraw */
		"cam_mr_larbx",
		"cam_mr_camtg",
		"cam_mr_mraw0",
		"cam_mr_mraw1",
		"cam_mr_mraw2",
		"cam_mr_mraw3",
		"cam_mr_pda0",
		"cam_mr_pda1",
		"cam_mr_larbx_dummy",

		/* ccu_main */
		"ccu_larb19",
		"ccu_ahb",
		"ccusys_ccu0",
		"ccusys_ccu1",

		/* afe */
		"afe_afe",
		"afe_22m",
		"afe_24m",
		"afe_apll2_tuner",
		"afe_apll_tuner",
		"afe_tdm_ck",
		"afe_adc",
		"afe_dac",
		"afe_dac_predis",
		"afe_tml",
		"afe_nle",
		"afe_general3_asrc",
		"afe_connsys_i2s_asrc",
		"afe_general1_asrc",
		"afe_general2_asrc",
		"afe_dac_hires",
		"afe_adc_hires",
		"afe_adc_hires_tml",
		"afe_adda6_adc",
		"afe_adda6_adc_hires",
		"afe_3rd_dac",
		"afe_3rd_dac_predis",
		"afe_3rd_dac_tml",
		"afe_3rd_dac_hires",
		"afe_i2s5_bclk",
		"afe_i2s6_bclk",
		"afe_i2s7_bclk",
		"afe_i2s8_bclk",
		"afe_i2s9_bclk",
		"afe_etdm_in0_bclk",
		"afe_etdm_out0_bclk",
		"afe_i2s1_bclk",
		"afe_i2s2_bclk",
		"afe_i2s3_bclk",
		"afe_i2s4_bclk",
		"afe_etdm_in1_bclk",
		"afe_etdm_out1_bclk",

		/* mminfra_config */
		"mminfra_gce_d",
		"mminfra_gce_m",
		"mminfra_smi",
		"mminfra_gce_26m",

		/* mdpsys_config */
		"mdp_mutex0",
		"mdp_apb_bus",
		"mdp_smi0",
		"mdp_rdma0",
		"mdp_fg0",
		"mdp_hdr0",
		"mdp_aal0",
		"mdp_rsz0",
		"mdp_tdshp0",
		"mdp_color0",
		"mdp_wrot0",
		"mdp_fake_eng0",
		"mdp_dli_async0",
		"mdp_dli_async1",
		"mdp_rdma1",
		"mdp_fg1",
		"mdp_hdr1",
		"mdp_aal1",
		"mdp_rsz1",
		"mdp_tdshp1",
		"mdp_color1",
		"mdp_wrot1",
		"mdp_rsz2",
		"mdp_wrot2",
		"mdp_dlo_async0",
		"mdp_rsz3",
		"mdp_wrot3",
		"mdp_dlo_async1",
		"mdp_hre_mdpsys",

		/* mdpsys1_config */
		"mdp1_mdp_mutex0",
		"mdp1_apb_bus",
		"mdp1_smi0",
		"mdp1_mdp_rdma0",
		"mdp1_mdp_fg0",
		"mdp1_mdp_hdr0",
		"mdp1_mdp_aal0",
		"mdp1_mdp_rsz0",
		"mdp1_mdp_tdshp0",
		"mdp1_mdp_color0",
		"mdp1_mdp_wrot0",
		"mdp1_mdp_fake_eng0",
		"mdp1_mdp_dli_async0",
		"mdp1_mdp_dli_async1",
		"mdp1_mdp_rdma1",
		"mdp1_mdp_fg1",
		"mdp1_mdp_hdr1",
		"mdp1_mdp_aal1",
		"mdp1_mdp_rsz1",
		"mdp1_mdp_tdshp1",
		"mdp1_mdp_color1",
		"mdp1_mdp_wrot1",
		"mdp1_mdp_rsz2",
		"mdp1_mdp_wrot2",
		"mdp1_mdp_dlo_async0",
		"mdp1_mdp_rsz3",
		"mdp1_mdp_wrot3",
		"mdp1_mdp_dlo_async1",
		"mdp1_hre_mdpsys",


	};

	return clks;
}


/*
 * clkdbg dump all fmeter clks
 */
static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return mt_get_fmeter_clks();
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	return mt_get_fmeter_freq(fclk->id, fclk->type);
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6895_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6895_all_clk_names,
};

static int clk_dbg_mt6895_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6895_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6895_drv = {
	.probe = clk_dbg_mt6895_probe,
	.driver = {
		.name = "clk-dbg-mt6895",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6895_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6895_drv, "clk-dbg-mt6895");
}

static void __exit clkdbg_mt6895_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6895_drv);
}

subsys_initcall(clkdbg_mt6895_init);
module_exit(clkdbg_mt6895_exit);
MODULE_LICENSE("GPL");

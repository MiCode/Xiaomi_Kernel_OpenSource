// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

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

const char * const *get_mt6985_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"axi_sel",
		"peri_faxi_sel",
		"ufs_faxi_sel",
		"bus_aximem_sel",
		"disp0_sel",
		"disp1_sel",
		"ovl0_sel",
		"ovl1_sel",
		"mdp0_sel",
		"mdp1_sel",
		"mminfra_sel",
		"mmup_sel",
		"dsp_sel",
		"mfg_ref_sel",
		"mfgsc_ref_sel",
		"uart_sel",
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
		"vdec_cksys1_sel",
		"pwm_sel",
		"audio_h_sel",
		"mcupm_sel",
		"mem_sub_sel",
		"peri_fmem_sub_sel",
		"ufs_fmem_sub_sel",
		"emi_n_sel",
		"emi_s_sel",
		"ap2conn_host_sel",
		"mcu_acp_sel",
		"sflash_sel",
		"mcu_l3gic_sel",
		"ipseast_sel",
		"ipssouth_sel",
		"ipswest_sel",
		"tl_sel",
		"pextp_faxi_sel",
		"pextp_fmem_sub_sel",
		"audio_local_bus_sel",
		"md_emi_sel",
		"mfg_int0_sel",
		"mfg1_int1_sel",
		"apll_i2s0_m_sel",
		"apll_i2s1_m_sel",
		"apll_i2s2_m_sel",
		"apll_i2s3_m_sel",
		"apll_i2s4_m_sel",
		"apll_i2s5_m_sel",
		"apll_i2s6_m_sel",
		"apll_i2s7_m_sel",
		"apll_i2s8_m_sel",
		"apll_i2s9_m_sel",
		"apll_etdm_in1_m_sel",
		"apll_etdm_out1_m_sel",
		"apll_i2s10_m_sel",
		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"camtg6_sel",
		"camtg7_sel",
		"camtg8_sel",
		"seninf_sel",
		"seninf1_sel",
		"seninf2_sel",
		"seninf3_sel",
		"seninf4_sel",
		"seninf5_sel",
		"venc_sel",
		"vdec_sel",
		"ccu_ahb_sel",
		"img1_sel",
		"ipe_sel",
		"cam_sel",
		"ccusys_sel",
		"camtm_sel",

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
		"apll12_div10",

		/* infracfg_ao */
		"ifrao_cldmabclk",
		"ifrao_fbist2fpc",
		"ifrao_dpmaif_main",
		"ifrao_dpmaif_26m_set",
		"ifrao_ap_i3c",

		/* apmixedsys */
		"mainpll2",
		"univpll2",
		"mmpll2",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"adsppll",
		"tvdpll",
		"apll1",
		"apll2",
		"mpll",
		"emipll",
		"imgpll",

		/* ssr_top */
		"ssr_rw_ssr_rng",
		"ssr_rq_rw_ssr_dma",
		"ssr_rq_rw_ssr_kdf",
		"ssr_rq_rw_ssr_pka",

		/* pericfg_ao */
		"perao_uart0",
		"perao_uart1",
		"perao_uart2",
		"perao_uart3",
		"perao_pwm_h",
		"perao_pwm_b",
		"perao_pwm_fb1",
		"perao_pwm_fb2",
		"perao_pwm_fb3",
		"perao_pwm_fb4",
		"perao_disp_pwm0",
		"perao_disp_pwm1",
		"perao_spi0_b",
		"perao_spi1_b",
		"perao_spi2_b",
		"perao_spi3_b",
		"perao_spi4_b",
		"perao_spi5_b",
		"perao_spi6_b",
		"perao_spi7_b",
		"perao_spi0_h",
		"perao_spi1_h",
		"perao_spi2_h",
		"perao_spi3_h",
		"perao_spi4_h",
		"perao_spi5_h",
		"perao_spi6_h",
		"perao_spi7_h",
		"perao_sflash",
		"perao_sflash_f",
		"perao_sflash_h",
		"perao_sflash_p",
		"perao_i2c",
		"perao_dma_b",
		"perao_ssusb0_frmcnt",
		"perao_ssusb0_sys",
		"perao_ssusb0_xhci",
		"perao_ssusb0_f",
		"perao_ssusb0_h",
		"perao_ssusb1_frmcnt",
		"perao_ssusb1_sys",
		"perao_ssusb1_xhci",
		"perao_ssusb1_f",
		"perao_ssusb1_h",
		"perao_msdc1",
		"perao_msdc1_f",
		"perao_msdc1_h",
		"perao_msdc2",
		"perao_msdc2_f",
		"perao_msdc2_h",
		"perao_uarthub",
		"perao_uarthub_h",
		"perao_uarthub_p",
		"perao_audio_slv",
		"perao_audio_mst",
		"perao_audio_intbus",

		/* afe */
		"afe_aud_pad_clock_en",
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
		"afe_adda7_adc",
		"afe_adda7_adc_hires",
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
		"afe_i2s10_bclk",
		"afe_etdm_in1_bclk",
		"afe_etdm_out1_bclk",

		/* ssusb_sifslv_ippc */
		"usb_sif_usb_u2_p",
		"usb_sif_usb_u3_p",

		/* ssusb_sifslv_ippc_p1 */
		"usb_sif_p1_usb_u2_p",
		"usb_sif_p1_usb_u3_p",

		/* imp_iic_wrap_c */
		"impc_i2c5",

		/* ufscfg_ao */
		"ufsao_unipro_tx_sym",
		"ufsao_unipro_rx_sym0",
		"ufsao_unipro_rx_sym1",
		"ufsao_unipro_sys",

		/* ufscfg_pdn */
		"ufspdn_ufshci_ufs",
		"ufspdn_ufshci_aes",

		/* pextpcfg_ao */
		"pext_mac0_26m",
		"pext_mac0_p1_p_250m",
		"pext_mac0_gfmux_tl",
		"pext_mac0_fmem",
		"pext_mac0",
		"pext_phy0_ref",
		"pext_mac1_26m",
		"pext_mac1_p1_p_250m",
		"pext_mac1_gfmux_tl",
		"pext_mac1_fmem",
		"pext_mac1",
		"pext_phy1_ref",

		/* imp_iic_wrap_s */
		"imps_i2c1",
		"imps_i2c2",
		"imps_i2c3",
		"imps_i2c4",
		"imps_i2c7",
		"imps_i2c8",
		"imps_i2c9",

		/* imp_iic_wrap_n */
		"impn_i2c0",
		"impn_i2c6",
		"impn_i2c10",
		"impn_i2c11",
		"impn_i2c12",
		"impn_i2c13",

		/* mfgpll_pll_ctrl */
		"mfg_ao_mfgpll",

		/* gpuebpll_pll_ctrl */
		"gpueb_ao_gpuebpll",

		/* mfgscpll_pll_ctrl */
		"mfgsc_ao_mfgscpll",

		/* dispsys_config */
		"mm_config",
		"mm_disp_mutex0",
		"mm_disp_aal0",
		"mm_disp_c3d0",
		"mm_disp_ccorr0",
		"mm_disp_ccorr1",
		"mm_disp_chist0",
		"mm_disp_chist1",
		"mm_disp_color0",
		"mm_disp_dither0",
		"mm_disp_dither1",
		"mm_disp_dli_async0",
		"mm_disp_dli_async1",
		"mm_disp_dli_async2",
		"mm_disp_dli_async3",
		"mm_disp_dli_async4",
		"mm_disp_dli_async5",
		"mm_disp_dlo_async0",
		"mm_disp_dlo_async1",
		"mm_disp_dp_intf0",
		"mm_disp_dsc_wrap0",
		"mm_clk0",
		"mm_disp_gamma0",
		"mm_mdp_aal0",
		"mm_mdp_rdma0",
		"mm_disp_merge0",
		"mm_disp_merge1",
		"mm_disp_oddmr0",
		"mm_disp_postalign0",
		"mm_disp_postmask0",
		"mm_disp_relay0",
		"mm_disp_rsz0",
		"mm_disp_spr0",
		"mm_disp_tdshp0",
		"mm_disp_tdshp1",
		"mm_disp_ufbc_wdma1",
		"mm_disp_vdcm0",
		"mm_disp_wdma1",
		"mm_smi_sub_comm0",
		"mm_disp_y2r0",
		"mm_dsi_clk",
		"mm_dp_clk",
		"mm_26m_clk",

		/* dispsys1_config */
		"mm1_config",
		"mm1_disp_mutex0",
		"mm1_disp_aal0",
		"mm1_disp_c3d0",
		"mm1_disp_ccorr0",
		"mm1_disp_ccorr1",
		"mm1_disp_chist0",
		"mm1_disp_chist1",
		"mm1_disp_color0",
		"mm1_disp_dither0",
		"mm1_disp_dither1",
		"mm1_disp_dli_async0",
		"mm1_disp_dli_async1",
		"mm1_disp_dli_async2",
		"mm1_disp_dli_async3",
		"mm1_disp_dli_async4",
		"mm1_disp_dli_async5",
		"mm1_disp_dlo_async0",
		"mm1_disp_dlo_async1",
		"mm1_disp_dp_intf0",
		"mm1_disp_dsc_wrap0",
		"mm1_clk0",
		"mm1_disp_gamma0",
		"mm1_mdp_aal0",
		"mm1_mdp_rdma0",
		"mm1_disp_merge0",
		"mm1_disp_merge1",
		"mm1_disp_oddmr0",
		"mm1_disp_postalign0",
		"mm1_disp_postmask0",
		"mm1_disp_relay0",
		"mm1_disp_rsz0",
		"mm1_disp_spr0",
		"mm1_disp_tdshp0",
		"mm1_disp_tdshp1",
		"mm1_disp_ufbc_wdma1",
		"mm1_disp_vdcm0",
		"mm1_disp_wdma1",
		"mm1_smi_sub_comm0",
		"mm1_disp_y2r0",
		"mm1_dsi_clk",
		"mm1_dp_clk",
		"mm1_26m_clk",

		/* ovlsys_config */
		"ovl_config",
		"ovl_disp_fake_eng0",
		"ovl_disp_fake_eng1",
		"ovl_disp_mutex0",
		"ovl_ovl0_2l",
		"ovl_ovl1_2l",
		"ovl_ovl2_2l",
		"ovl_ovl3_2l",
		"ovl_disp_rsz1",
		"ovl_mdp_rsz0",
		"ovl_disp_wdma0",
		"ovl_disp_ufbc_wdma0",
		"ovl_disp_wdma2",
		"ovl_disp_dli_async0",
		"ovl_disp_dli_async1",
		"ovl_disp_dli_async2",
		"ovl_disp_dlo_async0",
		"ovl_disp_dlo_async1",
		"ovl_disp_dlo_async2",
		"ovl_disp_dlo_async3",
		"ovl_disp_dlo_async4",
		"ovl_disp_dlo_async5",
		"ovl_disp_dlo_async6",
		"ovl_inlinerot",
		"ovl_smi_sub_common0",
		"ovl_disp_y2r0",
		"ovl_disp_y2r1",

		/* ovlsys1_config */
		"ovl1_config",
		"ovl1_disp_fake_eng0",
		"ovl1_disp_fake_eng1",
		"ovl1_disp_mutex0",
		"ovl1_ovl0_2l",
		"ovl1_ovl1_2l",
		"ovl1_ovl2_2l",
		"ovl1_ovl3_2l",
		"ovl1_disp_rsz1",
		"ovl1_mdp_rsz0",
		"ovl1_disp_wdma0",
		"ovl1_disp_ufbc_wdma0",
		"ovl1_disp_wdma2",
		"ovl1_disp_dli_async0",
		"ovl1_disp_dli_async1",
		"ovl1_disp_dli_async2",
		"ovl1_disp_dlo_async0",
		"ovl1_disp_dlo_async1",
		"ovl1_disp_dlo_async2",
		"ovl1_disp_dlo_async3",
		"ovl1_disp_dlo_async4",
		"ovl1_disp_dlo_async5",
		"ovl1_disp_dlo_async6",
		"ovl1_inlinerot",
		"ovl1_smi_sub_common0",
		"ovl1_disp_y2r0",
		"ovl1_disp_y2r1",

		/* imgsys_main */
		"img_fdvt",
		"img_me",
		"img_mmg",
		"img_larb12",
		"img_larb9",
		"img_traw0",
		"img_traw1",
		"img_vcore_gals",
		"img_dip0",
		"img_wpe0",
		"img_ipe",
		"img_wpe1",
		"img_wpe2",
		"img_smi_adl_larb0",
		"img_adl0",
		"img_avs",
		"img_gals",

		/* dip_top_dip1 */
		"dip_dip1_larb10",
		"dip_dip1_dip_top",

		/* dip_nr1_dip1 */
		"dip_nr1_dip1_larb",
		"dip_nr1_dip1_dip_nr1",

		/* dip_nr2_dip1 */
		"dip_nr2_dip1_larb15",
		"dip_nr2_dip1_dip_nr",

		/* wpe1_dip1 */
		"wpe1_dip1_larb11",
		"wpe1_dip1_wpe",

		/* wpe2_dip1 */
		"wpe2_dip1_larb11",
		"wpe2_dip1_wpe",

		/* wpe3_dip1 */
		"wpe3_dip1_larb11",
		"wpe3_dip1_wpe",

		/* traw_dip1 */
		"traw_dip1_larb28",
		"traw_dip1_traw",

		/* vdec_soc_gcon_base */
		"vde1_larb1_cken",
		"vde1_lat_cken",
		"vde1_lat_active",
		"vde1_lat_cken_eng",
		"vde1_mini_mdp_en",
		"vde1_vdec_cken",
		"vde1_vdec_active",
		"vde1_vdec_cken_eng",
		"vde1_vdec_soc_ips_en",

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_lat_cken",
		"vde2_lat_active",
		"vde2_lat_cken_eng",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven_larb",
		"ven_venc",
		"ven_jpgenc",
		"ven_jpgdec",
		"ven_jpgdec_c1",
		"ven_gals",
		"ven_gals_sram",

		/* venc_gcon_core1 */
		"ven_c1_larb",
		"ven_c1_venc",
		"ven_c1_jpgenc",
		"ven_c1_jpgdec",
		"ven_c1_jpgdec_c1",
		"ven_c1_gals",
		"ven_c1_gals_sram",

		/* venc_gcon_core2 */
		"ven_c2_larb",
		"ven_c2_venc",
		"ven_c2_jpgenc",
		"ven_c2_jpgdec",
		"ven_c2_jpgdec_c1",
		"ven_c2_gals",
		"ven_c2_gals_sram",

		/* vlp_cksys */
		"vlp_scp_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_aptgpt_sel",
		"vlp_dxcc_vlp_sel",
		"vlp_dpsw_sel",
		"vlp_spmi_m_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_dbgao_26m_sel",
		"vlp_systimer_26m_sel",
		"vlp_sspm_sel",
		"vlp_srck_sel",
		"vlp_sramrc_sel",
		"vlp_camtg_vlp_sel",
		"vlp_ips_sel",
		"vlp_26m_sspm_sel",
		"vlp_ulposc_sspm_sel",

		/* scp */
		"scp_set_spi0",
		"scp_set_spi1",
		"scp_set_spi2",
		"scp_set_spi3",

		/* scp_iic */
		"scp_iic_i2c0",
		"scp_iic_i2c1",
		"scp_iic_i2c2",
		"scp_iic_i2c3",
		"scp_iic_i2c4",
		"scp_iic_i2c5",
		"scp_iic_i2c6",
		"scp_iic_i2c7",

		/* camsys_main */
		"cam_m_larb13_con_0",
		"cam_m_larb14_con_0",
		"cam_m_larb27_con_0",
		"cam_m_larb29_con_0",
		"cam_m_cam_con_0",
		"cam_m_cam_suba_con_0",
		"cam_m_cam_subb_con_0",
		"cam_m_cam_subc_con_0",
		"cam_m_cam_mraw_con_0",
		"cam_m_camtg_con_0",
		"cam_m_seninf_con_0",
		"cam_m_camsv_con_0",
		"cam_m_adlrd_con_0",
		"cam_m_adlwr_con_0",
		"cam_m_uisp_con_0",
		"cam_m_fake_eng_con_0",
		"cam_m_cam2mm0_gcon_0",
		"cam_m_cam2mm1_gcon_0",
		"cam_m_cam2sys_gcon_0",
		"cam_m_cam2mm2_gcon_0",
		"cam_m_ccusys_con_0",
		"cam_m_ips_con_0",
		"cam_m_camsv_a_con_1",
		"cam_m_camsv_b_con_1",
		"cam_m_camsv_c_con_1",
		"cam_m_camsv_d_con_1",
		"cam_m_camsv_e_con_1",
		"cam_m_camsv_con_1",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",

		/* camsys_yuva */
		"cam_ya_larbx",
		"cam_ya_cam",
		"cam_ya_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",

		/* camsys_yuvb */
		"cam_yb_larbx",
		"cam_yb_cam",
		"cam_yb_camtg",

		/* camsys_rawc */
		"cam_rc_larbx",
		"cam_rc_cam",
		"cam_rc_camtg",

		/* camsys_yuvc */
		"cam_yc_larbx",
		"cam_yc_cam",
		"cam_yc_camtg",

		/* camsys_mraw */
		"cam_mr_larbx",
		"cam_mr_camtg",
		"cam_mr_mraw0",
		"cam_mr_mraw1",
		"cam_mr_mraw2",
		"cam_mr_mraw3",
		"cam_mr_pda0",
		"cam_mr_pda1",

		/* ccu_main */
		"ccu_larb19",
		"ccu_ahb",
		"ccusys_ccu0",
		"ccusys_ccu1",
		"ccusys_dpe",
		"ccusys_dhze",

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
		"mdp_rdma2",
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
		"mdp_rdma3",
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
		"mdp_dli_async2",
		"mdp_dli_async3",
		"mdp_dlo_async2",
		"mdp_dlo_async3",
		"mdp_birsz0",
		"mdp_birsz1",
		"mdp_img_dl_async0",
		"mdp_img_dl_async1",
		"mdp_hre_mdpsys",

		/* mdpsys1_config */
		"mdp1_mdp_mutex0",
		"mdp1_apb_bus",
		"mdp1_smi0",
		"mdp1_mdp_rdma0",
		"mdp1_mdp_rdma2",
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
		"mdp1_mdp_rdma3",
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
		"mdp1_mdp_dli_async2",
		"mdp1_mdp_dli_async3",
		"mdp1_mdp_dlo_async2",
		"mdp1_mdp_dlo_async3",
		"mdp1_mdp_birsz0",
		"mdp1_mdp_birsz1",
		"mdp1_img_dl_async0",
		"mdp1_img_dl_async1",
		"mdp1_hre_mdpsys",

		/* ccipll_pll_ctrl */
		"ccipll",

		/* armpll_ll_pll_ctrl */
		"cpu_ll_armpll_ll",

		/* armpll_bl_pll_ctrl */
		"cpu_bl_armpll_bl",

		/* armpll_b_pll_ctrl */
		"cpu_b_armpll_b",

		/* ptppll_pll_ctrl */
		"ptppll",


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

static struct clkdbg_ops clkdbg_mt6985_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6985_all_clk_names,
};

static int clk_dbg_mt6985_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6985_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6985_drv = {
	.probe = clk_dbg_mt6985_probe,
	.driver = {
		.name = "clk-dbg-mt6985",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6985_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6985_drv, "clk-dbg-mt6985");
}

static void __exit clkdbg_mt6985_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6985_drv);
}

subsys_initcall(clkdbg_mt6985_init);
module_exit(clkdbg_mt6985_exit);
MODULE_LICENSE("GPL");

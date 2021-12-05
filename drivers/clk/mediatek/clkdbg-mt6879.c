// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

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

const char * const *get_mt6879_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"axi_sel",
		"axip_sel",
		"axi_u_sel",
		"bus_aximem_sel",
		"disp0_sel",
		"mdp0_sel",
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
		"ipu_if_sel",
		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"camtg6_sel",
		"spi_sel",
		"msdc_macro_sel",
		"msdc30_1_sel",
		"audio_sel",
		"aud_intbus_sel",
		"atb_sel",
		"disp_pwm_sel",
		"usb_sel",
		"ssusb_xhci_sel",
		"i2c_sel",
		"seninf_sel",
		"seninf1_sel",
		"seninf2_sel",
		"seninf3_sel",
		"dxcc_sel",
		"aud_engen1_sel",
		"aud_engen2_sel",
		"aes_ufsfde_sel",
		"ufs_sel",
		"ufs_mbist_sel",
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
		"mem_subp_sel",
		"mem_sub_u_sel",
		"emi_n_sel",
		"dsi_occ_sel",
		"ccu_ahb_sel",
		"ap2conn_host_sel",
		"mcu_acp_sel",
		"dpi_sel",
		"img1_sel",
		"ipe_sel",
		"cam_sel",
		"ccusys_sel",
		"camtm_sel",
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
		"_md_32k",
		"_md_26m",

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

		/* infracfg */
		"ifr_bus_mon_enable",

		/* nemi_reg */
		"nemi_bus_mon_mode",

		/* pericfg_ao */
		"peraop_uart0",
		"peraop_uart1",
		"peraop_uart2",
		"peraop_pwm_hclk",
		"peraop_pwm_bclk",
		"peraop_pwm_fbclk1",
		"peraop_pwm_fbclk2",
		"peraop_pwm_fbclk3",
		"peraop_pwm_fbclk4",
		"peraop_btif_bclk",
		"peraop_disp_pwm0",
		"peraop_spi0_bclk",
		"peraop_spi1_bclk",
		"peraop_spi2_bclk",
		"peraop_spi3_bclk",
		"peraop_spi4_bclk",
		"peraop_spi5_bclk",
		"peraop_spi6_bclk",
		"peraop_spi7_bclk",
		"peraop_spi0_hclk",
		"peraop_spi1_hclk",
		"peraop_spi2_hclk",
		"peraop_spi3_hclk",
		"peraop_spi4_hclk",
		"peraop_spi5_hclk",
		"peraop_spi6_hclk",
		"peraop_spi7_hclk",
		"peraop_iic",
		"peraop_apdma",
		"peraop_usb_pclk",
		"peraop_usb_ref",
		"peraop_usb_frmcnt",
		"peraop_usb_phy",
		"peraop_usb_sys",
		"peraop_usb_xhci",
		"peraop_usb_dma_bus",
		"peraop_usb_mcu_bus",
		"peraop_msdc1",
		"peraop_msdc1_hclk",

		/* imp_iic_wrap_c */
		"impc_ap_clock_i2c10",
		"impc_ap_clock_i2c11",

		/* ufs_ao_config */
		"ufsao_peri2ufs_0",
		"ufsao_u_phy_sap_0",
		"ufsao_u_phy_ahb_s_0",
		"ufsao_u_tx_symbol_0",
		"ufsao_u_rx_symbol_0",
		"ufsao_u_rx_sym1_0",

		/* ufs_pdn_cfg */
		"ufspdn_ufs2peri_ck",
		"ufspdn_upro_sck_ck",
		"ufspdn_upro_tick_ck",
		"ufspdn_u_ck",
		"ufspdn_aes_u_ck",
		"ufspdn_u_tick_ck",
		"ufspdn_ufshci_ck",
		"ufspdn_mem_sub_ck",

		/* imp_iic_wrap_e */
		"impe_ap_clock_i2c2",
		"impe_ap_clock_i2c4",
		"impe_ap_clock_i2c9",

		/* imp_iic_wrap_w */
		"impw_ap_clock_i2c0",
		"impw_ap_clock_i2c1",
		"impw_ap_clock_i2c6",

		/* imp_iic_wrap_en */
		"impen_ap_clock_i2c3",
		"impen_ap_clock_i2c5",
		"impen_ap_clock_i2c7",
		"impen_ap_clock_i2c8",

		/* mfg_pll_ctrl */
		"mfg_ao_mfgpll",
		"mfg_ao_mfgscpll",

		/* mfg_top_config */
		"mfgcfg_bg3d",

		/* dispsys_config */
		"mm_disp_mutex0",
		"mm_disp_ovl0",
		"mm_disp_merge0",
		"mm_disp_fake_eng0",
		"mm_disp_inlinerot0",
		"mm_disp_wdma0",
		"mm_disp_fake_eng1",
		"mm_disp_dpi0",
		"mm_disp_ovl0_2l_nwcg",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_rsz0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_ccorr1",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_postmask0",
		"mm_disp_dither0",
		"mm_disp_cm0",
		"mm_disp_spr0",
		"mm_disp_dsc_wrap0",
		"mm_disp_dsi0",
		"mm_disp_ufbc_wdma0",
		"mm_disp_wdma1",
		"mm_dispsys_config",
		"mm_disp_tdshp0",
		"mm_disp_c3d0",
		"mm_disp_y2r0",
		"mm_disp_chist0",
		"mm_disp_ovl0_2l",
		"mm_disp_dli_async3",
		"mm_disp_dl0_async3",
		"mm_smi_larb",
		"mm_dsi_clk",
		"mm_dpi_clk",
		"mm_sig_emi",

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

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_mini_mdp_cken",
		"vde2_vdec_cken",
		"vde2_vdec_active",

		/* venc_gcon */
		"ven1_cke0_larb",
		"ven1_cke1_venc",
		"ven1_cke2_jpgenc",
		"ven1_cke5_gals",

		/* apu_pll_ctrl */
		"apu_ao_apupll",
		"apu_ao_npupll",
		"apu_ao_apupll1",
		"apu_ao_apupll2",

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
		"mdp_hdr0",
		"mdp_aal0",
		"mdp_rsz0",
		"mdp_tdshp0",
		"mdp_color0",
		"mdp_wrot0",
		"mdp_fake_eng0",
		"mdp_dli_async0",
		"mdp_rdma1",
		"mdp_hdr1",
		"mdp_aal1",
		"mdp_rsz1",
		"mdp_tdshp1",
		"mdp_color1",
		"mdp_wrot1",
		"mdp_dlo_async0",
		"mdp_hre_mdpsys",


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

static struct clkdbg_ops clkdbg_mt6879_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6879_all_clk_names,
};

static int clk_dbg_mt6879_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6879_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6879_drv = {
	.probe = clk_dbg_mt6879_probe,
	.driver = {
		.name = "clk-dbg-mt6879",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6879_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6879_drv, "clk-dbg-mt6879");
}

static void __exit clkdbg_mt6879_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6879_drv);
}

subsys_initcall(clkdbg_mt6879_init);
module_exit(clkdbg_mt6879_exit);
MODULE_LICENSE("GPL");

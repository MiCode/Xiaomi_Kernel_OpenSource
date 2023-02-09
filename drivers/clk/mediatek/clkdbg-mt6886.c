// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
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

const char * const *get_mt6886_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"axi_sel",
		"peri_axi_sel",
		"ufs_haxi_sel",
		"bus_aximem_sel",
		"disp0_sel",
		"mdp0_sel",
		"mminfra_sel",
		"mmup_sel",
		"dsp_sel",
		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"camtg6_sel",
		"uart_sel",
		"spi_sel",
		"msdc_macro_sel",
		"msdc30_1_sel",
		"msdc30_2_sel",
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
		"peri_mem_sel",
		"ufs_mem_sel",
		"emi_n_sel",
		"dsi_occ_sel",
		"ccu_ahb_sel",
		"ap2conn_host_sel",
		"img1_sel",
		"ipe_sel",
		"cam_sel",
		"ccusys_sel",
		"camtm_sel",
		"mcu_acp_sel",
		"csi_occ_scan_sel",
		"ipswest_sel",
		"ipsnorth_sel",
		"axi_l3gic_sel",
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
		"apll12_div_etdm_in1",
		"apll12_div_etdm_out1",

		/* infracfg_ao */
		"ifrao_i2c_dummy",
		"ifrao_therm",
		"ifrao_dma",
		"ifrao_ccif1_ap",
		"ifrao_ccif1_md",
		"ifrao_ccif_ap",
		"ifrao_ccif_md",
		"ifrao_cldmabclk",
		"ifrao_cq_dma",
		"ifrao_ccif5_md",
		"ifrao_ccif2_ap",
		"ifrao_ccif2_md",
		"ifrao_dpmaif_main",
		"ifrao_ccif4_md",
		"ifrao_dpmaif_26m",

		/* apmixedsys */
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"adsppll",
		"ufspll",
		"apll1",
		"apll2",
		"mpll",
		"emipll",
		"imgpll",

		/* pericfg_ao */
		"perao_uart0",
		"perao_uart1",
		"perao_pwm_h",
		"perao_pwm_b",
		"perao_pwm_fb1",
		"perao_pwm_fb2",
		"perao_pwm_fb3",
		"perao_pwm_fb4",
		"perao_btif_b",
		"perao_disp_pwm0",
		"perao_spi0_b",
		"perao_spi1_b",
		"perao_spi2_b",
		"perao_spi3_b",
		"perao_spi4_b",
		"perao_spi5_b",
		"perao_spi6_b",
		"perao_spi7_b",
		"perao_apdma",
		"perao_usb_sys",
		"perao_usb_xhci",
		"perao_usb_bus",
		"perao_msdc1",
		"perao_msdc1_h",
		"perao_audio_slv_ckp",
		"perao_audio_mst_ckp",
		"perao_intbus_ckp",
		"perao_aud_mst_idl_en",

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

		/* imp_iic_wrap_c */
		"impc_i2c5",
		"impc_i2c6",
		"impc_i2c10",
		"impc_i2c11",

		/* ufscfg_ao */
		"ufsao_unipro_tx_sym",
		"ufsao_unipro_rx_sym0",
		"ufsao_unipro_rx_sym1",
		"ufsao_unipro_sys",
		"ufsao_unipro_phy_sap",

		/* ufscfg_pdn */
		"ufspdn_ufshci_ufs",
		"ufspdn_ufshci_aes",

		/* imp_iic_wrap_es */
		"impes_i2c2",
		"impes_i2c4",
		"impes_i2c9",

		/* imp_iic_wrap_w */
		"impw_i2c0",
		"impw_i2c1",

		/* imp_iic_wrap_e */
		"impe_i2c3",
		"impe_i2c7",
		"impe_i2c8",

		/* mfgpll_pll_ctrl */
		"mfg_ao_mfgpll",

		/* mfgscpll_pll_ctrl */
		"mfgsc_ao_mfgscpll",

		/* dispsys_config */
		"mm_disp_mutex0",
		"mm_disp_ovl0",
		"mm_disp_merge0",
		"mm_disp_fake_eng0",
		"mm_inlinerot0",
		"mm_disp_wdma0",
		"mm_disp_fake_eng1",
		"mm_disp_ovl0_2l_nw",
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
		"mm_fmm_clk0",
		"mm_disp_ufbc_wdma0",
		"mm_disp_wdma1",
		"mm_apb_bus",
		"mm_disp_c3d0",
		"mm_disp_y2r0",
		"mm_disp_chist0",
		"mm_disp_chist1",
		"mm_disp_ovl0_2l",
		"mm_disp_dli_async3",
		"mm_disp_dlo_async3",
		"mm_disp_ovl1_2l",
		"mm_disp_ovl1_2l_nw",
		"mm_smi_common",
		"mm_clk0",
		"mm_sig_emi",

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

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_lat_cken",
		"vde2_lat_active",
		"vde2_mini_mdp_en",
		"vde2_vdec_cken",
		"vde2_vdec_active",

		/* venc_gcon */
		"ven_larb",
		"ven_venc",
		"ven_jpgenc",
		"ven_gals",
		"ven_gals_sram",

		/* vlp_cksys */
		"vlp_scp_vlp_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_26m_gpt_vlp_sel",
		"vlp_dxcc_vlp_sel",
		"vlp_spmi_p_sel",
		"vlp_spmi_m_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_dbg_26m_vlp_sel",
		"vlp_stmr26m_vlp_sel",
		"vlp_sspm_vlp_sel",
		"vlp_sspm_f26m_sel",
		"vlp_srck_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_psvlp_sel",

		/* scp */
		"scp_spi0",
		"scp_spi1",
		"scp_spi2",
		"scp_spi3",
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
		"ccusys_dpe",

		/* mminfra_config */
		"mminfra_gce_d",
		"mminfra_gce_m",
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
		"mdp_dl_relay0",
		"mdp_dl_relay1",
		"mdp_rdma1",
		"mdp_hdr1",
		"mdp_aal1",
		"mdp_rsz1",
		"mdp_tdshp1",
		"mdp_color1",
		"mdp_wrot1",
		"mdp_dlo_async0",
		"mdp_dlo_async1",
		"mdp_hre_mdpsys",

		/* ccipll_pll_ctrl */
		"ccipll",

		/* armpll_ll_pll_ctrl */
		"cpu_ll_armpll_ll",

		/* armpll_bl_pll_ctrl */
		"cpu_bl_armpll_bl",

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

static struct clkdbg_ops clkdbg_mt6886_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6886_all_clk_names,
};

static int clk_dbg_mt6886_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6886_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6886_drv = {
	.probe = clk_dbg_mt6886_probe,
	.driver = {
		.name = "clk-dbg-mt6886",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6886_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6886_drv, "clk-dbg-mt6886");
}

static void __exit clkdbg_mt6886_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6886_drv);
}

subsys_initcall(clkdbg_mt6886_init);
module_exit(clkdbg_mt6886_exit);
MODULE_LICENSE("GPL");

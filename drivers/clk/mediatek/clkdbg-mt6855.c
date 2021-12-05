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

const char * const *get_mt6855_all_clk_names(void)
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
		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"camtg6_sel",
		"uart_sel",
		"spi_sel",
		"msdc_0p_macro_sel",
		"msdc5hclk_sel",
		"msdc50_0_sel",
		"aes_msdcfde_sel",
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
		"ap2conn_host_sel",
		"mcu_acp_sel",
		"img1_sel",
		"ipe_sel",
		"cam_sel",
		"camtm_sel",
		"msdc_1p_rx_sel",
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

		/* infracfg_ao */
		"ifrao_therm",
		"ifrao_dma",
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
		"ifrao_dpmaif_26m_set",
		"ifrao_mem_sub_ck",

		/* apmixedsys */
		"armpll_ll",
		"armpll_bl",
		"ccipll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"tvdpll",
		"apll1",
		"apll2",
		"mpll",
		"emipll",
		"imgpll",

		/* imp_iic_wrap */
		"imp_ap_clock_i2c11",
		"imp_ap_clock_i2c0",
		"imp_ap_clock_i2c1",
		"imp_ap_clock_i2c2",
		"imp_ap_clock_i2c4",
		"imp_ap_clock_i2c9",
		"imp_ap_clock_i2c3",
		"imp_ap_clock_i2c6",
		"imp_ap_clock_i2c7",
		"imp_ap_clock_i2c8",
		"imp_ap_clock_i2c10",
		"imp_ap_clock_i2c5",

		/* pericfg_ao */
		"peraop_uart0",
		"peraop_uart1",
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
		"peraop_usb_phy",
		"peraop_usb_sys",
		"peraop_usb_dma_bus",
		"peraop_usb_mcu_bus",
		"peraop_msdc1",
		"peraop_msdc1_hclk",
		"peraop_fmsdc50",
		"peraop_fmsdc50_hclk",
		"peraop_faes_msdcfde",
		"peraop_msdc50_xclk",
		"peraop_msdc50_hclk",
		"perao_audio_slv_peri",
		"perao_audio_mst_peri",
		"perao_intbus_peri",
		"perao_hre_sys_peri",
		"perao_hre_cbip_peri",

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

		/* mfg_pll_ctrl */
		"mfg_ao_mfgpll",
		"mfg_ao_mfgscpll",

		/* dispsys_config */
		"mm_disp_mutex0",
		"mm_disp_ovl0",
		"mm_disp_merge0",
		"mm_disp_fake_eng0",
		"mm_disp_inlinerot0",
		"mm_disp_wdma0",
		"mm_disp_fake_eng1",
		"mm_disp_dpi0",
		"mm_disp_ovl0_2l_nw",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_dli_async0",
		"mm_disp_dli_async1",
		"mm_disp_dli_async2",
		"mm_disp_dlo_async0",
		"mm_disp_dlo_async1",
		"mm_disp_dlo_async2",
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
		"mm_CLK0",
		"mm_disp_ufbc_wdma0",
		"mm_disp_wdma1",
		"mm_DP_CLK",
		"mm_disp_apb_bus",
		"mm_disp_tdshp0",
		"mm_disp_c3d0",
		"mm_disp_y2r0",
		"mm_mdp_aal0",
		"mm_disp_chist0",
		"mm_disp_chist1",
		"mm_disp_ovl0_2l",
		"mm_disp_dli_async3",
		"mm_disp_dl0_async3",
		"mm_disp_ovl1_2l",
		"mm_disp_ovl1_2l_nw",
		"mm_smi_common",
		"mm_dsi_ck",
		"mm_dpi_ck",

		/* imgsys1 */
		"imgsys1_larb9",
		"imgsys1_larb10",
		"imgsys1_dip",
		"imgsys1_gals",

		/* imgsys2 */
		"imgsys2_larb9",
		"imgsys2_larb10",
		"imgsys2_mfb",
		"imgsys2_wpe",
		"imgsys2_mss",
		"imgsys2_gals",

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_mini_mdp",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven1_cke0_larb",
		"ven1_cke1_venc",
		"ven1_cke2_jpgenc",
		"ven1_cke5_gals",

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

		/* camsys_main */
		"cam_m_larb13",
		"cam_m_dfp_vad",
		"cam_m_larb14",
		"cam_m_cam",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv0",
		"cam_m_camsv1",
		"cam_m_camsv2",
		"cam_m_camsv3",
		"cam_m_ccu0",
		"cam_m_ccu1",
		"cam_m_mraw0",
		"cam_m_fake_eng",
		"cam_m_ccu_gals",
		"cam_m_cam2mm_gals",
		"cam_m_camsv4",
		"cam_m_pda",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",

		/* ipesys */
		"ipe_larb19",
		"ipe_larb20",
		"ipe_smi_subcom",
		"ipe_fd",
		"ipe_fe",
		"ipe_rsc",
		"ipe_gals",

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
		"mdp_mm_img_dl_as0",
		"mdp_mm_img_dl_as1",
		"mdp_img_dl_as0",
		"mdp_img_dl_as1",

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
		"mdp1_mm_img_dl_as0",
		"mdp1_mm_img_dl_as1",
		"mdp1_img_dl_as0",
		"mdp1_img_dl_as1",


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

static struct clkdbg_ops clkdbg_mt6855_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6855_all_clk_names,
};

static int clk_dbg_mt6855_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6855_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6855_drv = {
	.probe = clk_dbg_mt6855_probe,
	.driver = {
		.name = "clk-dbg-mt6855",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6855_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6855_drv, "clk-dbg-mt6855");
}

static void __exit clkdbg_mt6855_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6855_drv);
}

subsys_initcall(clkdbg_mt6855_init);
module_exit(clkdbg_mt6855_exit);
MODULE_LICENSE("GPL");

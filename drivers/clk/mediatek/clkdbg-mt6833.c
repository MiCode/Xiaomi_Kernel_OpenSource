// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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

const char * const *get_mt6833_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"axi_sel",
		"spm_sel",
		"scp_sel",
		"bus_aximem_sel",
		"disp_sel",
		"mdp_sel",
		"img1_sel",
		"img2_sel",
		"ipe_sel",
		"dpe_sel",
		"cam_sel",
		"ccu_sel",
		"mfg_ref_sel",
		"mfg_pll_sel",
		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"uart_sel",
		"spi_sel",
		"msdc5hclk_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"audio_sel",
		"aud_intbus_sel",
		"pwrap_ulposc_sel",
		"atb_sel",
		"sspm_sel",
		"scam_sel",
		"disp_pwm_sel",
		"usb_sel",
		"ssusb_xhci_sel",
		"i2c_sel",
		"seninf_sel",
		"seninf1_sel",
		"seninf2_sel",
		"dxcc_sel",
		"aud_engen1_sel",
		"aud_engen2_sel",
		"aes_ufsfde_sel",
		"ufs_sel",
		"aud_1_sel",
		"aud_2_sel",
		"adsp_sel",
		"dpmaif_main_sel",
		"venc_sel",
		"vdec_sel",
		"camtm_sel",
		"pwm_sel",
		"audio_h_sel",
		"spmi_mst_sel",
		"dvfsrc_sel",
		"aes_msdcfde_sel",
		"mcupm_sel",
		"sflash_sel",
		"dsi_occ_sel",
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
		"ifrao_infra_force",
		"ifrao_pmic_tmr",
		"ifrao_pmic_ap",
		"ifrao_sej",
		"ifrao_apxgpt",
		"ifrao_mcupm",
		"ifrao_gce",
		"ifrao_gce2",
		"ifrao_therm",
		"ifrao_i2c_pseudo",
		"ifrao_i2c1_pseudo",
		"ifrao_pwm_hclk",
		"ifrao_pwm1",
		"ifrao_pwm2",
		"ifrao_pwm3",
		"ifrao_pwm4",
		"ifrao_pwm",
		"ifrao_uart0",
		"ifrao_uart1",
		"ifrao_uart2",
		"ifrao_uart3",
		"ifrao_gce_26m",
		"ifrao_btif",
		"ifrao_spi0",
		"ifrao_msdc0",
		"ifrao_msdc1",
		"ifrao_msdc0_clk",
		"ifrao_dvfsrc",
		"ifrao_gcpu",
		"ifrao_trng",
		"ifrao_auxadc",
		"ifrao_cpum",
		"ifrao_ccif1_ap",
		"ifrao_ccif1_md",
		"ifrao_auxadc_md",
		"ifrao_msdc1_clk",
		"ifrao_msdc0_aes_clk",
		"ifrao_dapc",
		"ifrao_ccif_ap",
		"ifrao_debugsys",
		"ifrao_audio",
		"ifrao_ccif_md",
		"ifrao_dxcc_ao",
		"ifrao_dbg_trace",
		"ifrao_devmpu_bclk",
		"ifrao_dramc26",
		"ifrao_irtx",
		"ifrao_ssusb",
		"ifrao_disp_pwm",
		"ifrao_cldmabclk",
		"ifrao_audio26m",
		"ifrao_mdtemp",
		"ifrao_spi1",
		"ifrao_spi2",
		"ifrao_spi3",
		"ifrao_unipro_sysclk",
		"ifrao_unipro_tick",
		"ifrao_u_bclk",
		"ifrao_md32_bclk",
		"ifrao_fsspm",
		"ifrao_unipro_mbist",
		"ifrao_sspm_hclk",
		"ifrao_spi4",
		"ifrao_spi5",
		"ifrao_cq_dma",
		"ifrao_ufs",
		"ifrao_u_aes",
		"ifrao_u_tick",
		"ifrao_usb_xhci",
		"ifrao_sspm_26m",
		"ifrao_sspm_32k",
		"ifrao_u_axi",
		"ifrao_ap_msdc0",
		"ifrao_md_msdc0",
		"ifrao_ccif5_md",
		"ifrao_ccif2_ap",
		"ifrao_ccif2_md",
		"ifrao_ccif3_ap",
		"ifrao_ccif3_md",
		"ifrao_sej_f13m",
		"ifrao_aes",
		"ifrao_fbist2fpc",
		"ifrao_dapc_sync",
		"ifrao_dpmaif_main",
		"ifrao_ccif4_md",
		"ifrao_spi6_ck",
		"ifrao_spi7_ck",
		"ifrao_66mp_mclk_p",
		"ifrao_infra_133m",
		"ifrao_infra_66m",
		"ifrao_peru_bus_133m",
		"ifrao_peru_bus_66m",
		"ifrao_flash_26m",
		"ifrao_sflash_ck",
		"ifrao_ap_dma",
		"ifrao_dcmforce",

		/* pericfg */
		"periaxi_disable",

		/* apmixedsys */
		"armpll_ll",
		"armpll_bl0",
		"ccipll",
		"mpll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"adsppll",
		"mfgpll",
		"tvdpll",
		"apll1",
		"apll2",
		"npupll",
		"usbpll",

		/* apmixedsys */
		"mipic0",
		"mipic1",
		"pll_mipid26m_0_en",
		"pll_mipid26m_1_en",

		/* infracfg */
		"ifr_bus_mon_enable",
		"ifr_bus_mon_1",
		"ifr_bus_mon_2",

		/* imp_iic_wrap_c */
		"impc_i2c10_ap_clock",
		"impc_i2c11_ap_clock",

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
		"afe_i2s1_bclk",
		"afe_i2s2_bclk",
		"afe_i2s3_bclk",
		"afe_i2s4_bclk",
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

		/* msdc0 */
		"msdc0_axi_wrap_cken",

		/* imp_iic_wrap_e */
		"impe_i2c3_ap_clock",

		/* imp_iic_wrap_s */
		"imps_i3c9_ap_clock",
		"imps_i3c8_ap_clock",

		/* imp_iic_wrap_ws */
		"impws_i2c1_ap_clock",
		"impws_i3c2_ap_clock",
		"impws_i3c4_ap_clock",

		/* imp_iic_wrap_w */
		"impw_i2c0_ap_clock",
		"impw_i2c5_ap_clock",
		"impw_i3c7_ap_clock",

		/* imp_iic_wrap_n */
		"impn_i2c6_ap_clock",

		/* mfg_top_config */
		"mfgcfg_bg3d",

		/* dispsys_config */
		"mm_disp_mutex0",
		"mm_apb_bus",
		"mm_disp_ovl0",
		"mm_disp_rdma0",
		"mm_disp_ovl0_2l",
		"mm_disp_wdma0",
		"mm_disp_rsz0",
		"mm_disp_aal0",
		"mm_disp_ccorr0",
		"mm_disp_color0",
		"mm_smi_infra",
		"mm_disp_gamma0",
		"mm_disp_postmask0",
		"mm_disp_dither0",
		"mm_smi_common",
		"mm_dsi0",
		"mm_disp_fake_eng0",
		"mm_disp_fake_eng1",
		"mm_smi_gals",
		"mm_smi_iommu",
		"mm_dsi0_dsi_domain",
		"mm_disp_26m_ck",

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
		"vde2_vdec_cken",
		"vde2_vdec_active",

		/* venc_gcon */
		"ven1_cke0_larb",
		"ven1_cke1_venc",
		"ven1_cke2_jpgenc",
		"ven1_cke5_gals",

		/* camsys_main */
		"cam_m_larb13",
		"cam_m_dfp_vad",
		"cam_m_larb14",
		"cam_m_cam",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv1",
		"cam_m_camsv2",
		"cam_m_camsv3",
		"cam_m_ccu0",
		"cam_m_ccu1",
		"cam_m_mraw0",
		"cam_m_fake_eng",
		"cam_m_ccu_gals",
		"cam_m_cam2mm_gals",

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
		"ipe_dpe",
		"ipe_gals",

		/* mdpsys_config */
		"mdp_rdma0",
		"mdp_tdshp0",
		"mdp_img_dl_async0",
		"mdp_img_dl_async1",
		"mdp_smi0",
		"mdp_apb_bus",
		"mdp_wrot0",
		"mdp_rsz0",
		"mdp_hdr0",
		"mdp_mutex0",
		"mdp_wrot1",
		"mdp_rsz1",
		"mdp_fake_eng0",
		"mdp_aal0",
		"mdp_img_dl_rel0_as0",
		"mdp_img_dl_rel1_as1",


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

static struct clkdbg_ops clkdbg_mt6833_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6833_all_clk_names,
};

static int clk_dbg_mt6833_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6833_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6833_drv = {
	.probe = clk_dbg_mt6833_probe,
	.driver = {
		.name = "clk-dbg-mt6833",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6833_init(void)
{
	pr_notice("%s start\n", __func__);
	return clk_dbg_driver_register(&clk_dbg_mt6833_drv, "clk-dbg-mt6833");
}

static void __exit clkdbg_mt6833_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6833_drv);
}

subsys_initcall(clkdbg_mt6833_init);
module_exit(clkdbg_mt6833_exit);
MODULE_LICENSE("GPL");

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

const char * const *get_mt6789_all_clk_names(void)
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
		"ipe_sel",
		"cam_sel",
		"mfg_ref_sel",
		"mfg_pll_sel",
		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"camtg6_sel",
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
		"aud_1_sel",
		"aud_2_sel",
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
		"ifrao_pmic_tmr",
		"ifrao_pmic_ap",
		"ifrao_gce",
		"ifrao_gce2",
		"ifrao_therm",
		"ifrao_i2c_pseudo",
		"ifrao_apdma_pseudo",
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
		"ifrao_auxadc",
		"ifrao_cpum",
		"ifrao_ccif1_ap",
		"ifrao_ccif1_md",
		"ifrao_auxadc_md",
		"ifrao_msdc1_clk",
		"ifrao_msdc0_aes_clk",
		"ifrao_ccif_ap",
		"ifrao_debugsys",
		"ifrao_audio",
		"ifrao_ccif_md",
		"ifrao_ssusb",
		"ifrao_disp_pwm",
		"ifrao_cldmabclk",
		"ifrao_audio26m",
		"ifrao_spi1",
		"ifrao_spi2",
		"ifrao_spi3",
		"ifrao_unipro_sysclk",
		"ifrao_unipro_tick",
		"ifrao_u_bclk",
		"ifrao_spi4",
		"ifrao_spi5",
		"ifrao_cq_dma",
		"ifrao_ufs",
		"ifrao_u_aes",
		"ifrao_ap_msdc0",
		"ifrao_md_msdc0",
		"ifrao_ccif5_md",
		"ifrao_ccif2_ap",
		"ifrao_ccif2_md",
		"ifrao_fbist2fpc",
		"ifrao_dpmaif_main",
		"ifrao_ccif4_ap",
		"ifrao_ccif4_md",
		"ifrao_spi6_ck",
		"ifrao_spi7_ck",
		"ifrao_66mp_mclkp",
		"ifrao_ap_dma",

		/* apmixedsys */
		"armpll_ll",
		"armpll_bl0",
		"ccipll",
		"mpll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"npupll",
		"mfgpll",
		"tvdpll",
		"apll1",
		"apll2",
		"usbpll",

		/* imp_iic_wrap_c */
		"impc_ap_clock_i2c3",
		"impc_ap_clock_i2c5",
		"impc_ap_clock_i2c6",

		/* afe */
		"afe_afe",
		"afe_22m",
		"afe_24m",
		"afe_apll2_tuner",
		"afe_apll_tuner",
		"afe_adc",
		"afe_dac",
		"afe_dac_predis",
		"afe_tml",
		"afe_nle",
		"afe_i2s1_bclk",
		"afe_i2s2_bclk",
		"afe_i2s3_bclk",
		"afe_i2s4_bclk",
		"afe_general3_asrc",
		"afe_connsys_i2s_asrc",
		"afe_general1_asrc",
		"afe_general2_asrc",
		"afe_dac_hires",
		"afe_adc_hires",
		"afe_adc_hires_tml",

		/* imp_iic_wrap_w */
		"impw_ap_clock_i2c0",
		"impw_ap_clock_i2c1",

		/* imp_iic_wrap_en */
		"impen_ap_clock_i2c2",
		"impen_ap_clock_i2c4",
		"impen_ap_clock_i2c8",
		"impen_ap_clock_i2c9",

		/* imp_iic_wrap_n */
		"impn_ap_clock_i2c7",

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
		"mm_disp_dsc_wrap0",
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

		/* disp_dsc */
		"disp_dsc_dsc_en",

		/* imgsys1 */
		"imgsys1_larb9",
		"imgsys1_larb10",
		"imgsys1_dip",
		"imgsys1_gals",

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_lat_cken",
		"vde2_lat_active",
		"vde2_lat_cken_eng",
		"vde2_mini_mdp",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven1_cke0_larb",
		"ven1_cke1_venc",
		"ven1_cke2_jpgenc",
		"ven1_cke5_gals",

		/* camsys_main */
		"cam_m_larb13",
		"cam_m_larb14",
		"cam_m_cam",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv1",
		"cam_m_camsv2",
		"cam_m_camsv3",
		"cam_m_mraw0",
		"cam_m_fake_eng",
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

static struct clkdbg_ops clkdbg_mt6789_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6789_all_clk_names,
};

static int clk_dbg_mt6789_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6789_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6789_drv = {
	.probe = clk_dbg_mt6789_probe,
	.driver = {
		.name = "clk-dbg-mt6789",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6789_init(void)
{
	pr_notice("%s start\n", __func__);
	return clk_dbg_driver_register(&clk_dbg_mt6789_drv, "clk-dbg-mt6789");
}

static void __exit clkdbg_mt6789_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6789_drv);
}

subsys_initcall(clkdbg_mt6789_init);
module_exit(clkdbg_mt6789_exit);
MODULE_LICENSE("GPL");

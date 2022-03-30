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

#define DUMP_INIT_STATE		0

const char * const *get_mt6983_all_clk_names(void)
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
		"dsp_sel",
		"dsp1_sel",
		"dsp2_sel",
		"dsp3_sel",
		"dsp4_sel",
		"dsp5_sel",
		"dsp6_sel",
		"dsp7_sel",
		"ipu_if_sel",
		"mfg_sel",
		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"uart_sel",
		"spi_sel",
		"msdc50_0_h_sel",
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
		"seninf3_sel",
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
		"vdec_lat_sel",
		"camtm_sel",
		"pwm_sel",
		"audio_h_sel",
		"camtg5_sel",
		"camtg6_sel",
		"mcupm_sel",
		"spmi_mst_sel",
		"dvfsrc_sel",
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
		"apll1_div0",
		"apll2_div0",
		"apll12_div0",
		"apll12_div1",
		"apll12_div2",
		"apll12_div3",
		"apll12_div4",
		"apll12_divb",
		"apll12_div5_lsb",
		"apll12_div5_msb",
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
		"ifrao_i2c0",
		"ifrao_i2c1",
		"ifrao_i2c2",
		"ifrao_i2c3",
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
		"ifrao_dma",
		"ifrao_btif",
		"ifrao_spi0",
		"ifrao_msdc0",
		"ifrao_msdc1",
		"ifrao_msdc0_clk",
		"ifrao_auxadc",
		"ifrao_cpum",
		"ifrao_ccif1_ap",
		"ifrao_ccif1_md",
		"ifrao_msdc1_clk",
		"ifrao_ap_dma_ps",
		"ifrao_dapc",
		"ifrao_ccif_ap",
		"ifrao_audio",
		"ifrao_ccif_md",
		"ifrao_secore",
		"ifrao_ssusb",
		"ifrao_disp_pwm",
		"ifrao_dpmaif_ck",
		"ifrao_audio26m",
		"ifrao_spi1",
		"ifrao_i2c4",
		"ifrao_spi2",
		"ifrao_spi3",
		"ifrao_unipro_sysclk",
		"ifrao_unipro_tick",
		"ifrao_ufs_bclk",
		"ifrao_unipro_mbist",
		"ifrao_i2c5",
		"ifrao_i2c5a",
		"ifrao_i2c5_imm",
		"ifrao_i2c1a",
		"ifrao_i2c1_imm",
		"ifrao_i2c2a",
		"ifrao_i2c2_imm",
		"ifrao_spi4",
		"ifrao_spi5",
		"ifrao_cq_dma",
		"ifrao_ufs",
		"ifrao_aes",
		"ifrao_ufs_tick",
		"ifrao_ssusb_xhci",
		"ifrao_msdc0sf",
		"ifrao_msdc1sf",
		"ifrao_msdc2sf",
		"ifrao_ufs_axi",
		"ifrao_i2c6",
		"ifrao_ap_msdc0",
		"ifrao_md_msdc0",
		"ifrao_ccif5_ap",
		"ifrao_ccif5_md",
		"ifrao_ccif2_ap",
		"ifrao_ccif2_md",
		"ifrao_i2c7",
		"ifrao_i2c8",
		"ifrao_fbist2fpc",
		"ifrao_dapc_sync",
		"ifrao_dpmaif_main",
		"ifrao_ccif4_ap",
		"ifrao_ccif4_md",
		"ifrao_spi6_ck",
		"ifrao_spi7_ck",
		"ifrao_apdma",

		/* apmixedsys */
		"armpll_ll",
		"armpll_bl0",
		"armpll_bl1",
		"armpll_bl2",
		"armpll_bl3",
		"ccipll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"adsppll",
		"mfgpll",
		"tvdpll",
		"apll1",
		"apll2",
		"mpll",
		"apupll",

		/* scp_adsp */
		"scp_adsp_audiodsp",

		/* imp_iic_wrap_c */
		"impc_ap_i2c0_ro",
		"impc_ap_i2c10_ro",
		"impc_ap_i2c11_ro",
		"impc_ap_i2c12_ro",
		"impc_ap_i2c13_ro",

		/* audiosys */
		"aud_afe",
		"aud_22m",
		"aud_24m",
		"aud_apll2_tuner",
		"aud_apll_tuner",
		"aud_tdm_ck",
		"aud_adc",
		"aud_dac",
		"aud_dac_predis",
		"aud_tml",
		"aud_nle",
		"aud_i2s1_bclk",
		"aud_i2s2_bclk",
		"aud_i2s3_bclk",
		"aud_i2s4_bclk",
		"aud_connsys_i2s_asrc",
		"aud_general1_asrc",
		"aud_general2_asrc",
		"aud_dac_hires",
		"aud_adc_hires",
		"aud_adc_hires_tml",
		"aud_adda6_adc",
		"aud_adda6_adc_hires",
		"aud_3rd_dac",
		"aud_3rd_dac_predis",
		"aud_3rd_dac_tml",
		"aud_3rd_dac_hires",
		"aud_i2s5_bclk",
		"aud_i2s6_bclk",
		"aud_i2s7_bclk",
		"aud_i2s8_bclk",
		"aud_i2s9_bclk",

		/* imp_iic_wrap_e */
		"impe_ap_i2c3_ro",
		"impe_ap_i2c9_ro",

		/* imp_iic_wrap_s */
		"imps_ap_i2c1_ro",
		"imps_ap_i2c2_ro",
		"imps_ap_i2c4_ro",
		"imps_ap_i2c7_ro",
		"imps_ap_i2c8_ro",

		/* imp_iic_wrap_n */
		"impn_ap_i2c5_ro",
		"impn_ap_i2c6_ro",

		/* mfgcfg */
		"mfgcfg_bg3d",

		/* mmsys_config */
		"mm_disp_rsz0",
		"mm_disp_rsz1",
		"mm_disp_ovl0",
		"mm_inline",
		"mm_mdp_tdshp4",
		"mm_mdp_tdshp5",
		"mm_mdp_aal4",
		"mm_mdp_aal5",
		"mm_mdp_hdr4",
		"mm_mdp_hdr5",
		"mm_mdp_rsz4",
		"mm_mdp_rsz5",
		"mm_mdp_rdma4",
		"mm_mdp_rdma5",
		"mm_disp_fake_eng0",
		"mm_disp_fake_eng1",
		"mm_disp_ovl0_2l",
		"mm_disp_ovl1_2l",
		"mm_disp_ovl2_2l",
		"mm_disp_mutex",
		"mm_disp_ovl1",
		"mm_disp_ovl3_2l",
		"mm_disp_ccorr0",
		"mm_disp_ccorr1",
		"mm_disp_color0",
		"mm_disp_color1",
		"mm_disp_postmask0",
		"mm_disp_postmask1",
		"mm_disp_dither0",
		"mm_disp_dither1",
		"mm_dsi0_mm_clk",
		"mm_dsi1_mm_clk",
		"mm_disp_gamma0",
		"mm_disp_gamma1",
		"mm_disp_aal0",
		"mm_disp_aal1",
		"mm_disp_wdma0",
		"mm_disp_wdma1",
		"mm_disp_ufbc_wdma0",
		"mm_disp_ufbc_wdma1",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_rdma4",
		"mm_disp_rdma5",
		"mm_disp_dsc_wrap",
		"mm_dp_intf_mm_clk",
		"mm_disp_merge0",
		"mm_disp_merge1",
		"mm_smi_common",
		"mm_smi_gals",
		"mm_smi_infra",
		"mm_smi_iommu",
		"mm_dsi0_intf_clk",
		"mm_dsi1_intf_clk",
		"mm_dp_intf_intf_clk",
		"mm_26_mhz",
		"mm_32_khz",

		/* imgsys1 */
		"imgsys1_larb9",
		"imgsys1_larb10",
		"imgsys1_dip",
		"imgsys1_mfb",
		"imgsys1_wpe",
		"imgsys1_mss",

		/* imgsys2 */
		"imgsys2_larb9",
		"imgsys2_larb10",
		"imgsys2_dip",
		"imgsys2_wpe",

		/* vdec_soc_gcon */
		"vde1_larb1_cken",
		"vde1_lat_cken",
		"vde1_lat_active",
		"vde1_lat_cken_eng",
		"vde1_vdec_cken",
		"vde1_vdec_active",
		"vde1_vdec_cken_eng",

		/* vdec_gcon */
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

		/* venc_c1_gcon */
		"ven2_cke0_larb",
		"ven2_cke1_venc",
		"ven2_cke2_jpgenc",
		"ven2_cke3_jpgdec",
		"ven2_cke4_jpgdec_c1",
		"ven2_cke5_gals",

		/* camsys_main */
		"cam_m_larb13",
		"cam_m_dfp_vad",
		"cam_m_larb14",
		"cam_m_larb15",
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
		"cam_m_mraw1",
		"cam_m_fake_eng",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",

		/* camsys_rawc */
		"cam_rc_larbx",
		"cam_rc_cam",
		"cam_rc_camtg",

		/* ipesys */
		"ipe_larb19",
		"ipe_larb20",
		"ipe_smi_subcom",
		"ipe_fd",
		"ipe_fe",
		"ipe_rsc",
		"ipe_dpe",

		/* mdpsys_config */
		"mdp_rdma0",
		"mdp_fg0",
		"mdp_hdr0",
		"mdp_aal0",
		"mdp_rsz0",
		"mdp_tdshp0",
		"mdp_tcc0",
		"mdp_wrot0",
		"mdp_rdma2",
		"mdp_aal2",
		"mdp_rsz2",
		"mdp_color0",
		"mdp_tdshp2",
		"mdp_tcc2",
		"mdp_wrot2",
		"mdp_mutex0",
		"mdp_rdma1",
		"mdp_fg1",
		"mdp_hdr1",
		"mdp_aal1",
		"mdp_rsz1",
		"mdp_tdshp1",
		"mdp_tcc1",
		"mdp_wrot1",
		"mdp_rdma3",
		"mdp_aal3",
		"mdp_rsz3",
		"mdp_color1",
		"mdp_tdshp3",
		"mdp_tcc3",
		"mdp_wrot3",
		"mdp_apb_bus",
		"mdp_mmsysram",
		"mdp_apmcu_gals",
		"mdp_fake_eng0",
		"mdp_fake_eng1",
		"mdp_smi0",
		"mdp_img_dl_async0",
		"mdp_img_dl_async1",
		"mdp_img_dl_async2",
		"mdp_smi1",
		"mdp_img_dl_async3",
		"mdp_reserved42",
		"mdp_reserved43",
		"mdp_smi2",
		"mdp_reserved45",
		"mdp_reserved46",
		"mdp_reserved47",
		"mdp_img0_dl_as0",
		"mdp_img0_dl_as1",
		"mdp_img1_dl_as2",
		"mdp_img1_dl_as3",


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

static struct clkdbg_ops clkdbg_mt6983_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6983_all_clk_names,
};

static int clk_dbg_mt6983_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6983_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6983_drv = {
	.probe = clk_dbg_mt6983_probe,
	.driver = {
		.name = "clk-dbg-mt6983",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6983_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6983_drv, "clk-dbg-mt6983");
}

static void __exit clkdbg_mt6983_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6983_drv);
}

subsys_initcall(clkdbg_mt6983_init);
module_exit(clkdbg_mt6983_exit);
MODULE_LICENSE("GPL");


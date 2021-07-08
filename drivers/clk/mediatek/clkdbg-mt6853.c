// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <dt-bindings/power/mt6853-power.h>

#ifdef CONFIG_MTK_DEVAPC
#include <mt-plat/devapc_public.h>
#endif

#include "clkdbg.h"
#include "clkchk.h"

/*
 * clkdbg dump_state
 */

static const char * const clk_names[] = {
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
	"dsp1_npupll_sel",
	"dsp2_sel",
	"dsp2_npupll_sel",
	"ipu_if_sel",
	"mfg_ref_sel",
	"mfg_pll_sel",
	"camtg_sel",
	"camtg2_sel",
	"camtg3_sel",
	"camtg4_sel",
	"camtg5_sel",
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
	"ifrao_pcie_tl_26m",
	"ifrao_msdc1_clk",
	"ifrao_msdc0_aes_clk",
	"ifrao_pcie_tl_96m",
	"ifrao_pcie_pl_p_250m",
	"ifrao_dapc",
	"ifrao_ccif_ap",
	"ifrao_audio",
	"ifrao_ccif_md",
	"ifrao_secore",
	"ifrao_ssusb",
	"ifrao_disp_pwm",
	"ifrao_cldmabclk",
	"ifrao_audio26m",
	"ifrao_spi1",
	"ifrao_i2c4",
	"ifrao_spi2",
	"ifrao_spi3",
	"ifrao_unipro_sysclk",
	"ifrao_ufs_bclk",
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
	"ifrao_ufs_aes",
	"ifrao_ssusb_xhci",
	"ifrao_msdc0sf",
	"ifrao_msdc1sf",
	"ifrao_msdc2sf",
	"ifrao_i2c6",
	"ifrao_ap_msdc0",
	"ifrao_md_msdc0",
	"ifrao_ccif5_ap",
	"ifrao_ccif5_md",
	"ifrao_flashif_h_133m",
	"ifrao_ccif2_ap",
	"ifrao_ccif2_md",
	"ifrao_i2c7",
	"ifrao_i2c8",
	"ifrao_fbist2fpc",
	"ifrao_dapc_sync",
	"ifrao_ccif4_ap",
	"ifrao_ccif4_md",
	"ifrao_spi6_ck",
	"ifrao_spi7_ck",
	"ifrao_66m_peri_mclk",
	"ifrao_infra_133m",
	"ifrao_infra_66m",
	"ifrao_peri_bus_133m",
	"ifrao_peri_bus_66m",
	"ifrao_flash_26m",
	"ifrao_sflash_ck",
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

	/* scp_adsp */
	"scp_par_audiodsp",

	/* imp_iic_wrap_c */
	"impc_ap_i2c10",
	"impc_ap_i2c11",

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
	"impe_ap_i2c3",

	/* imp_iic_wrap_s */
	"imps_ap_i2c5",
	"imps_ap_i2c7",
	"imps_ap_i2c8",
	"imps_ap_i2c9",

	/* imp_iic_wrap_ws */
	"impws_ap_i2c1",
	"impws_ap_i2c2",
	"impws_ap_i2c4",

	/* imp_iic_wrap_w */
	"impw_ap_i2c0",

	/* imp_iic_wrap_n */
	"impn_ap_i2c6",

	/* mfgcfg */
	"mfg_bg3d",

	/* mmsys_config */
	"mm_disp_mutex0",
	"mm_apb_bus",
	"mm_disp_ovl0",
	"mm_disp_rdma0",
	"mm_disp_ovl0_2l",
	"mm_disp_wdma0",
	"mm_disp_ccorr1",
	"mm_disp_rsz0",
	"mm_disp_aal0",
	"mm_disp_ccorr0",
	"mm_disp_color0",
	"mm_smi_infra",
	"mm_disp_dsc_wrap",
	"mm_disp_gamma0",
	"mm_disp_postmask0",
	"mm_disp_spr0",
	"mm_disp_dither0",
	"mm_smi_common",
	"mm_disp_cm0",
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

	/* vdec_gcon */
	"vdec_larb1_cken",
	"vdec_cken",
	"vdec_active",

	/* venc_gcon */
	"venc_cke0_larb",
	"venc_cke1_venc",
	"venc_cke2_jpgenc",
	"venc_cke5_gals",

	/* apu_conn */
	"apuc_apu",
	"apuc_ahb",
	"apuc_axi",
	"apuc_isp",
	"apuc_cam_adl",
	"apuc_img_adl",
	"apuc_emi_26m",
	"apuc_vpu_udi",
	"apuc_edma_0",
	"apuc_edma_1",
	"apuc_edmal_0",
	"apuc_edmal_1",
	"apuc_mnoc",
	"apuc_tcm",
	"apuc_md32",
	"apuc_iommu_0",
	"apuc_md32_32k",

	/* apu_vcore */
	"apuv_ahb",
	"apuv_axi",
	"apuv_adl",
	"apuv_qos",

	/* apu0 */
	"apu0_apu",
	"apu0_axi_m",
	"apu0_jtag",
	"apu0_pclk",

	/* apu1 */
	"apu1_apu",
	"apu1_axi_m",
	"apu1_jtag",
	"apu1_pclk",

	/* camsys_main */
	"cam_m_larb13",
	"cam_m_larb14",
	"cam_m_reserved0",
	"cam_m_cam",
	"cam_m_camtg",
	"cam_m_seninf",
	"cam_m_camsv1",
	"cam_m_camsv2",
	"cam_m_camsv3",
	"cam_m_ccu0",
	"cam_m_ccu1",
	"cam_m_mraw0",
	"cam_m_reserved2",
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
	"mdp_rdma1",
	"mdp_tdshp1",
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
	"mdp_aal1",
	"mdp_color0",
	"mdp_img_dl_rel0_as0",
	"mdp_img_dl_rel1_as1",
};

static const char * const *get_all_clk_names(void)
{
	return clk_names;
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6853_ops = {
	.get_all_fmeter_clks = NULL,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = NULL,
	.get_all_clk_names = get_all_clk_names,
};

static int clk_dbg_mt6853_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6853_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6853_drv = {
	.probe = clk_dbg_mt6853_probe,
	.driver = {
		.name = "clk-dbg-mt6853",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6853_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6853_drv, "clk-dbg-mt6853");
}

static void __exit clkdbg_mt6853_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6853_drv);
}

subsys_initcall(clkdbg_mt6853_init);
module_exit(clkdbg_mt6853_exit);
MODULE_LICENSE("GPL");


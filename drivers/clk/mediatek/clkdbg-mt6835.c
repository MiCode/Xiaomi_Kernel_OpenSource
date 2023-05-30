// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chuan-Wen Chen <chuan-wen.chen@mediatek.com>
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

const char * const *get_mt6835_all_clk_names(void)
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
		"dpmaif_main_sel",
		"venc_sel",
		"vdec_sel",
		"pwm_sel",
		"audio_h_sel",
		"mcupm_sel",
		"mem_sub_sel",
		"mem_subp_sel",
		"mem_sub_u_sel",
		"ap2conn_host_sel",
		"mcu_acp_sel",
		"img1_sel",
		"ipe_sel",
		"cam_sel",
		"camtm_sel",
		"msdc_1p_rx_sel",
		"nfi1x_sel",
		"dbi_sel",
		"mfg_ref_sel",
		"emi_546_sel",
		"emi_624_sel",
		"mfg_pll_sel",
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
		"ifrao_cpum",
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
		"ifrao_ccif4_ap",
		"ifrao_ccif4_md",
		"ifrao_dpmaif_26m",
		"ifrao_mem_sub_ck",
		"ifrao_aes_top0",
		"ifrao_i2c_dummy_0",
		"ifrao_i2c_dummy_1",
		"ifrao_i2c_dummy_2",
		"ifrao_i2c_dummy_3",
		"ifrao_i2c_dummy_4",
		"ifrao_i2c_dummy_5",
		"ifrao_i2c_dummy_6",
		"ifrao_i2c_dummy_7",
		"ifrao_i2c_dummy_8",
		"ifrao_i2c_dummy_9",
		"ifrao_i2c_dummy_10",
		"ifrao_i2c_dummy_11",
		"ifrao_dcmforce",

		/* apmixedsys */
		"armpll_ll",
		"armpll_bl",
		"ccipll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"mfgpll",
		"tvdpll",
		"apll1",
		"apll2",
		"mpll",
		"imgpll",

		/* nemi_reg */
		"nemi_bus_mon_mode",

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
		"peraop_apdma",
		"peraop_usb_frmcnt",
		"peraop_usb_sys",
		"peraop_usb_xhci",
		"peraop_msdc1_src",
		"peraop_msdc1_hclk",
		"peraop_msdc0_src",
		"peraop_msdc0_hclk",
		"peraop_msdc0_aes",
		"peraop_msdc0_xclk",
		"peraop_msdc0_h_wrap",
		"peraop_nfiecc_bclk",
		"peraop_nfi_bclk",
		"peraop_nfi_hclk",
		"auxadc_bclk_ap",
		"auxadc_bclk_md",
		"perao_audio_slv_ckp",
		"perao_audio_mst_ckp",
		"perao_intbus_ckp",

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
		"afe_general3_asrc",
		"afe_connsys_i2s_asrc",
		"afe_general1_asrc",
		"afe_general2_asrc",
		"afe_dac_hires",
		"afe_adc_hires",
		"afe_adc_hires_tml",
		"afe_i2s5_bclk",
		"afe_i2s1_bclk",
		"afe_i2s2_bclk",
		"afe_i2s3_bclk",
		"afe_i2s4_bclk",

		/* imp_iic_wrap_c */
		"impc_ap_clock_i2c10",
		"impc_ap_clock_i2c11",

		/* imp_iic_wrap_ws */
		"impws_ap_clock_i2c3",
		"impws_ap_clock_i2c5",

		/* imp_iic_wrap_s */
		"imps_ap_clock_i2c1",
		"imps_ap_clock_i2c6",
		"imps_ap_clock_i2c7",
		"imps_ap_clock_i2c8",

		/* imp_iic_wrap_en */
		"impen_ap_clock_i2c0",
		"impen_ap_clock_i2c2",
		"impen_ap_clock_i2c4",
		"impen_ap_clock_i2c9",

		/* mfg_top_config */
		"mfgcfg_bg3d",

		/* dispsys_config */
		"mm_disp_mutex0",
		"mm_disp_ovl0",
		"mm_disp_fake_eng0",
		"mm_inlinerot0",
		"mm_disp_wdma0",
		"mm_disp_fake_eng1",
		"mm_disp_dbi0",
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
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_postmask0",
		"mm_disp_dither0",
		"mm_disp_dummy_mod_b0",
		"mm_clk0",
		"mm_dp_clk",
		"mm_apb_bus",
		"mm_disp_tdshp0",
		"mm_disp_c3d0",
		"mm_disp_y2r0",
		"mm_mdp_aal0",
		"mm_disp_chist0",
		"mm_disp_chist1",
		"mm_disp_ovl0_2l",
		"mm_dli_async3",
		"mm_dlo_async3",
		"mm_dummy_mod_b1",
		"mm_disp_ovl1_2l",
		"mm_dummy_mod_b2",
		"mm_dummy_mod_b3",
		"mm_dummy_mod_b4",
		"mm_disp_ovl1_2l_nw",
		"mm_dummy_mod_b5",
		"mm_dummy_mod_b6",
		"mm_dummy_mod_b7",
		"mm_smi_iommu",
		"mm_clk",
		"mm_disp_dbpi",
		"mm_disp_hrt_urgent",

		/* imgsys1 */
		"imgsys1_larb9",
		"imgsys1_dip",
		"imgsys1_gals",

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven1_cke0_larb",
		"ven1_cke1_venc",
		"ven1_cke2_jpgenc",

		/* vlp_cksys */
		"vlp_scp_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_spmi_p_sel",
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

		/* scp_iic */
		"scp_iic_i2c0",
		"scp_iic_i2c1",
		"scp_iic_i2c2",
		"scp_iic_i2c3",
		"scp_iic_i2c4",
		"scp_iic_i2c5",
		"scp_iic_i2c6",

		/* camsys_main */
		"cam_m_larb13",
		"cam_m_larb14",
		"cam_m_cam",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv1",
		"cam_m_camsv2",
		"cam_m_camsv3",
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
		"ipe_gals",

		/* sramrc_apb */
		"sramrc_apb_sramrc_en",

		/* mminfra_config */
		"mminfra_gce_d",
		"mminfra_gce_m",
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
		"mdp_rsz2",
		"mdp_wrot2",
		"mdp_fmm_dl_async0",
		"mdp_fmm_dl_async1",
		"mdp_fimg_dl_async0",
		"mdp_fimg_dl_async1",


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

static struct clkdbg_ops clkdbg_mt6835_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6835_all_clk_names,
};

static int clk_dbg_mt6835_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6835_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6835_drv = {
	.probe = clk_dbg_mt6835_probe,
	.driver = {
		.name = "clk-dbg-mt6835",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6835_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6835_drv, "clk-dbg-mt6835");
}

static void __exit clkdbg_mt6835_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6835_drv);
}

subsys_initcall(clkdbg_mt6835_init);
module_exit(clkdbg_mt6835_exit);
MODULE_LICENSE("GPL");

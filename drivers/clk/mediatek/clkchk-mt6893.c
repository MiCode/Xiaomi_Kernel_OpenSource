// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/syscore_ops.h>
#include <linux/version.h>

#include <mt-plat/aee.h>
#include "clk-mt6893-pg.h"
#include "clkdbg-mt6893.h"

#define TAG	"[clkchk] "
#define	BUG_ON_CHK_ENABLE	1

const char * const *get_mt6893_all_clk_names(void)
{
	static const char * const clks[] = {
		/* plls */
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"adsppll",
		"mfgpll",
		"tvdpll",
		"apupll",
		"apll1",
		"apll2",

		/* apmixedsys */
		"apmixed_mipi0_26m",

		/* TOP */
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

		"msdc50_0_hclk_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"audio_sel",

		"aud_intbus_sel",
		"pwrap_ulposc_sel",
		"atb_sel",
		"sspm_sel",

		"dp_sel",
		"scam_sel",
		"disp_pwm_sel",
		"usb_top_sel",

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

		/* INFRA */
		"infracfg_ao_pmic_cg_tmr",
		"infracfg_ao_pmic_cg_ap",
		"infracfg_ao_pmic_cg_md",
		"infracfg_ao_pmic_cg_conn",
		"infracfg_ao_scpsys_cg",
		"infracfg_ao_sej_cg",
		"infracfg_ao_apxgpt_cg",
		"infracfg_ao_mcupm_cg",
		"infracfg_ao_gce_cg",
		"infracfg_ao_gce2_cg",
		"infracfg_ao_therm_cg",
		"infracfg_ao_i2c0_cg",
		"infracfg_ao_i2c1_cg",
		"infracfg_ao_i2c2_cg",
		"infracfg_ao_i2c3_cg",
		"infracfg_ao_pwm_hclk_cg",
		"infracfg_ao_pwm1_cg",
		"infracfg_ao_pwm2_cg",
		"infracfg_ao_pwm3_cg",
		"infracfg_ao_pwm4_cg",
		"infracfg_ao_pwm_cg",
		"infracfg_ao_uart0_cg",
		"infracfg_ao_uart1_cg",
		"infracfg_ao_uart2_cg",
		"infracfg_ao_uart3_cg",
		"infracfg_ao_gce_26m",
		"infracfg_ao_cq_dma_fpc",
		"infracfg_ao_btif_cg",
		"infracfg_ao_spi0_cg",
		"infracfg_ao_msdc0_cg",
		"infracfg_ao_msdc1_cg",
		"infracfg_ao_msdc0_src_clk_cg",
		"infracfg_ao_dvfsrc_cg",
		"infracfg_ao_gcpu_cg",
		"infracfg_ao_trng_cg",
		"infracfg_ao_cpum_cg",
		"infracfg_ao_ccif1_ap_cg",
		"infracfg_ao_ccif1_md_cg",
		"infracfg_ao_msdc1_src_clk_cg",
		"infracfg_ao_ap_dma_pseudo_cg",
		"infracfg_ao_device_apc_cg",
		"infracfg_ao_ccif_ap_cg",
		"infracfg_ao_debugsys_cg",
		"infracfg_ao_audio_cg",
		"infracfg_ao_ccif_md_cg",
		"infracfg_ao_dxcc_sec_core_cg",
		"infracfg_ao_dxcc_ao_cg",
		"infracfg_ao_dbg_trace_cg",
		"infracfg_ao_devmpu_bclk_cg",
		"infracfg_ao_dramc_f26m_cg",
		"infracfg_ao_irtx_cg",
		"infracfg_ao_ssusb_cg",
		"infracfg_ao_disp_pwm_cg",
		"infracfg_ao_cldma_bclk_ck",
		"infracfg_ao_audio_26m_bclk_ck",
		"infracfg_ao_modem_temp_share_cg",
		"infracfg_ao_spi1_cg",
		"infracfg_ao_i2c4_cg",
		"infracfg_ao_spi2_cg",
		"infracfg_ao_spi3_cg",
		"infracfg_ao_unipro_sysclk_cg",
		"infracfg_ao_unipro_tick_cg",
		"infracfg_ao_ufs_mp_sap_bclk_cg",
		"infracfg_ao_md32_bclk_cg",
		"infracfg_ao_sspm_cg",
		"infracfg_ao_unipro_mbist_cg",
		"infracfg_ao_sspm_bus_hclk_cg",
		"infracfg_ao_i2c5_cg",
		"infracfg_ao_i2c5_arbiter_cg",
		"infracfg_ao_i2c5_imm_cg",
		"infracfg_ao_i2c1_arbiter_cg",
		"infracfg_ao_i2c1_imm_cg",
		"infracfg_ao_i2c2_arbiter_cg",
		"infracfg_ao_i2c2_imm_cg",
		"infracfg_ao_spi4_cg",
		"infracfg_ao_spi5_cg",
		"infracfg_ao_cq_dma_cg",
		"infracfg_ao_ufs_cg",
		"infracfg_ao_aes_cg",
		"infracfg_ao_ufs_tick_cg",
		"infracfg_ao_ssusb_xhci_cg",
		"infracfg_ao_msdc0_self_cg",
		"infracfg_ao_msdc1_self_cg",
		"infracfg_ao_msdc2_self_cg",
		"infracfg_ao_sspm_26m_self_cg",
		"infracfg_ao_sspm_32k_self_cg",
		"infracfg_ao_ufs_axi_cg",
		"infracfg_ao_i2c6_cg",
		"infracfg_ao_ap_msdc0_cg",
		"infracfg_ao_md_msdc0_cg",
		"infracfg_ao_ccif5_ap_cg",
		"infracfg_ao_ccif5_md_cg",
		"infracfg_ao_ccif2_ap_cg",
		"infracfg_ao_ccif2_md_cg",
		"infracfg_ao_ccif3_ap_cg",
		"infracfg_ao_ccif3_md_cg",
		"infracfg_ao_sej_f13m_cg",
		"infracfg_ao_aes_cg",
		"infracfg_ao_i2c7_cg",
		"infracfg_ao_i2c8_cg",
		"infracfg_ao_fbist2fpc_cg",
		"infracfg_ao_device_apc_sync_cg",
		"infracfg_ao_dpmaif_main_cg",
		"infracfg_ao_ccif4_ap_cg",
		"infracfg_ao_ccif4_md_cg",
		"infracfg_ao_spi6_ck_cg",
		"infracfg_ao_spi7_ck_cg",
		"infracfg_ao_ap_dma_cg",

		/* MM */
		"MM_DISP_RSZ0",
		"MM_DISP_RSZ1",
		"MM_DISP_OVL0",
		"MM_INLINEROT",
		"MM_MDP_TDSHP4",
		"MM_MDP_TDSHP5",
		"MM_MDP_AAL4",
		"MM_MDP_AAL5",
		"MM_MDP_HDR4",
		"MM_MDP_HDR5",
		"MM_MDP_RSZ4",
		"MM_MDP_RSZ5",
		"MM_MDP_RDMA4",
		"MM_MDP_RDMA5",
		"MM_DISP_FAKE_ENG0",
		"MM_DISP_FAKE_ENG1",
		"MM_DISP_OVL0_2L",
		"MM_DISP_OVL1_2L",
		"MM_DISP_OVL2_2L",
		"MM_DISP_MUTEX0",
		"MM_DISP_OVL1",
		"MM_DISP_OVL3_2L",
		"MM_DISP_CCORR0",
		"MM_DISP_CCORR1",
		"MM_DISP_COLOR0",
		"MM_DISP_COLOR1",
		"MM_DISP_POSTMASK0",
		"MM_DISP_POSTMASK1",
		"MM_DISP_DITHER0",
		"MM_DISP_DITHER1",
		"MM_DISP_DSI0",
		"MM_DISP_DSI1",
		"MM_DISP_GAMMA0",
		"MM_DISP_GAMMA1",
		"MM_DISP_AAL0",
		"MM_DISP_AAL1",
		"MM_DISP_WDMA0",
		"MM_DISP_WDMA1",
		"MM_DISP_RDMA0",
		"MM_DISP_RDMA1",
		"MM_DISP_RDMA4",
		"MM_DISP_RDMA5",
		"MM_DISP_DSC_WRAP",
		"MM_DISP_DP_INTF",
		"MM_DISP_MERGE0",
		"MM_DISP_MERGE1",
		"MM_SMI_COMMON",
		"MM_SMI_GALS",
		"MM_SMI_INFRA",
		"MM_SMI_IOMMU",
		"MM_DSI_DSI0",
		"MM_DSI_DSI1",
		"MM_DP_INTF",
		"MM_26MHZ",
		"MM_32KHZ",

		/* MDP */
		"MDP_MDP_RDMA0",
		"MDP_MDP_FG0",
		"MDP_MDP_HDR0",
		"MDP_MDP_AAL0",
		"MDP_MDP_RSZ0",
		"MDP_MDP_TDSHP0",
		"MDP_MDP_TCC0",
		"MDP_MDP_WROT0",
		"MDP_MDP_RDMA2",
		"MDP_MDP_AAL2",
		"MDP_MDP_RSZ2",
		"MDP_MDP_COLOR0",
		"MDP_MDP_TDSHP2",
		"MDP_MDP_TCC2",
		"MDP_MDP_WROT2",
		"MDP_MDP_MUTEX0",
		"MDP_MDP_RDMA1",
		"MDP_MDP_FG1",
		"MDP_MDP_HDR1",
		"MDP_MDP_AAL1",
		"MDP_MDP_RSZ1",
		"MDP_MDP_TDSHP1",
		"MDP_MDP_TCC1",
		"MDP_MDP_WROT1",
		"MDP_MDP_RDMA3",
		"MDP_MDP_AAL3",
		"MDP_MDP_RSZ3",
		"MDP_MDP_COLOR1",
		"MDP_MDP_TDSHP3",
		"MDP_MDP_TCC3",
		"MDP_MDP_WROT3",
		"MDP_APB_BUS",
		"MDP_MMSYSRAM",
		"MDP_APMCU_GALS",
		"MDP_MDP_FAKE_ENG0",
		"MDP_MDP_FAKE_ENG1",
		"MDP_SMI0",
		"MDP_IMG_DL_ASYNC0",
		"MDP_IMG_DL_ASYNC1",
		"MDP_IMG_DL_ASYNC2",
		"MDP_SMI1",
		"MDP_IMG_DL_ASYNC3",
		"MDP_SMI2",
		"MDP_IMG0_IMG_DL_ASYNC0",
		"MDP_IMG0_IMG_DL_ASYNC1",
		"MDP_IMG1_IMG_DL_ASYNC2",
		"MDP_IMG1_IMG_DL_ASYNC3",


		/* CAM */
		"camsys_main_larb13_cgpdn",
		"camsys_main_dfp_vad_cgpdn",
		"camsys_main_larb14_cgpdn",
		"camsys_main_larb15_cgpdn",
		"camsys_main_cam_cgpdn",
		"camsys_main_camtg_cgpdn",
		"camsys_main_seninf_cgpdn",
		"camsys_main_camsv0_cgpdn",
		"camsys_main_camsv1_cgpdn",
		"camsys_main_camsv2_cgpdn",
		"camsys_main_camsv3_cgpdn",
		"camsys_main_ccu0_cgpdn",
		"camsys_main_ccu1_cgpdn",
		"camsys_main_mraw0_cgpdn",
		"camsys_main_mraw1_cgpdn",
		"camsys_main_fake_eng_cgpdn",
		"camsys_rawa_larbx_cgpdn",
		"camsys_rawa_cam_cgpdn",
		"camsys_rawa_camtg_cgpdn",
		"camsys_rawb_larbx_cgpdn",
		"camsys_rawb_cam_cgpdn",
		"camsys_rawb_camtg_cgpdn",
		"camsys_rawc_larbx_cgpdn",
		"camsys_rawc_cam_cgpdn",
		"camsys_rawc_camtg_cgpdn",

		/* IMG */
		"imgsys1_larb9_cgpdn",
		"imgsys1_larb10_cgpdn",
		"imgsys1_dip_cgpdn",
		"imgsys1_mfb_cgpdn",
		"imgsys1_wpe_cgpdn",
		"imgsys1_mss_cgpdn",
		"imgsys2_larb11_cgpdn",
		"imgsys2_larb12_cgpdn",
		"imgsys2_dip_cgpdn",
		"imgsys2_mfb_cgpdn",
		"imgsys2_wpe_cgpdn",
		"imgsys2_mss_cgpdn",

		/* IPE */
		"ipesys_larb19_cgpdn",
		"ipesys_larb20_cgpdn",
		"ipesys_ipe_smi_subcom_cgpdn",
		"ipesys_fd_cgpdn",
		"ipesys_fe_cgpdn",
		"ipesys_rsc_cgpdn",
		"ipesys_dpe_cgpdn",

		/* MFG */
		"mfgcfg_bg3d",

		/* IIC */
		"imp_iic_wrap_c_ap_i2c0_cg_ro",
		"imp_iic_wrap_c_ap_i2c10_cg_ro",
		"imp_iic_wrap_c_ap_i2c11_cg_ro",
		"imp_iic_wrap_c_ap_i2c12_cg_ro",
		"imp_iic_wrap_c_ap_i2c13_cg_ro",
		"imp_iic_wrap_e_ap_i2c3_cg_ro",
		"imp_iic_wrap_e_ap_i2c9_cg_ro",
		"imp_iic_wrap_n_ap_i2c5_cg_ro",
		"imp_iic_wrap_n_ap_i2c6_cg_ro",
		"imp_iic_wrap_s_ap_i2c1_cg_ro",
		"imp_iic_wrap_s_ap_i2c2_cg_ro",
		"imp_iic_wrap_s_ap_i2c4_cg_ro",
		"imp_iic_wrap_s_ap_i2c7_cg_ro",
		"imp_iic_wrap_s_ap_i2c8_cg_ro",

		/* APU */
		"apu0_apu_cg",
		"apu0_axi_m_cg",
		"apu0_jtag_cg",
		"apu1_apu_cg",
		"apu1_axi_m_cg",
		"apu1_jtag_cg",
		"apu2_apu_cg",
		"apu2_axi_m_cg",
		"apu2_jtag_cg",
		"apusys_vcore_ahb_cg",
		"apusys_vcore_axi_cg",
		"apusys_vcore_adl_cg",
		"apusys_vcore_qos_cg",
		"apu_conn_ahb_cg",
		"apu_conn_axi_cg",
		"apu_conn_isp_cg",
		"apu_conn_cam_adl_cg",
		"apu_conn_img_adl_cg",
		"apu_conn_emi_26m_cg",
		"apu_conn_vpu_udi_cg",
		"apu_conn_edma_0_cg",
		"apu_conn_edma_1_cg",
		"apu_conn_edmal_0_cg",
		"apu_conn_edmal_1_cg",
		"apu_conn_mnoc_cg",
		"apu_conn_tcm_cg",
		"apu_conn_md32_cg",
		"apu_conn_iommu_0_cg",
		"apu_conn_iommu_1_cg",
		"apu_conn_md32_32k_cg",
		"apu_mdla0_mdla_cg0",
		"apu_mdla0_mdla_cg1",
		"apu_mdla0_mdla_cg2",
		"apu_mdla0_mdla_cg3",
		"apu_mdla0_mdla_cg4",
		"apu_mdla0_mdla_cg5",
		"apu_mdla0_mdla_cg6",
		"apu_mdla0_mdla_cg7",
		"apu_mdla0_mdla_cg8",
		"apu_mdla0_mdla_cg9",
		"apu_mdla0_mdla_cg10",
		"apu_mdla0_mdla_cg11",
		"apu_mdla0_mdla_cg12",
		"apu_mdla0_apb_cg",
		"apu_mdla1_mdla_cg0",
		"apu_mdla1_mdla_cg1",
		"apu_mdla1_mdla_cg2",
		"apu_mdla1_mdla_cg3",
		"apu_mdla1_mdla_cg4",
		"apu_mdla1_mdla_cg5",
		"apu_mdla1_mdla_cg6",
		"apu_mdla1_mdla_cg7",
		"apu_mdla1_mdla_cg8",
		"apu_mdla1_mdla_cg9",
		"apu_mdla1_mdla_cg10",
		"apu_mdla1_mdla_cg11",
		"apu_mdla1_mdla_cg12",
		"apu_mdla1_apb_cg",

		/* AUDIO */
		"audio_pdn_apll2_tuner",
		"audio_pdn_24m",
		"audio_i2s9_bclk_sw_cg",
		"audio_i2s8_bclk_sw_cg",
		"audio_i2s7_bclk_sw_cg",
		"audio_i2s6_bclk_sw_cg",
		"audio_i2s5_bclk_sw_cg",
		"audio_pdn_3rd_dac_tml",
		"audio_pdn_3rd_dac_predis",
		"audio_pdn_3rd_dac",
		"audio_pdn_adda6_adc",
		"audio_pdn_general2_asrc",
		"audio_pdn_general1_asrc",
		"audio_pdn_connsys_i2s_asrc",
		"audio_i2s4_bclk_sw_cg",
		"audio_i2s3_bclk_sw_cg",
		"audio_i2s2_bclk_sw_cg",
		"audio_i2s1_bclk_sw_cg",
		"audio_pdn_nle",
		"audio_pdn_tml",
		"audio_pdn_dac_predis",
		"audio_pdn_dac",
		"audio_pdn_adc",
		"audio_pdn_afe",
		"audio_pdn_3rd_dac_hires",
		"audio_pdn_adda6_adc_hires",
		"audio_pdn_adc_hires_tml",
		"audio_pdn_adc_hires",
		"audio_pdn_dac_hires",
		"audio_pdn_tdm_ck",
		"audio_pdn_apll_tuner",
		"audio_pdn_22m",

		/* VDEC */
		"vdec_soc_gcon_vdec_cken_eng",
		"vdec_soc_gcon_vdec_active",
		"vdec_soc_gcon_vdec_cken",
		"vdec_soc_gcon_lat_cken_eng",
		"vdec_soc_gcon_lat_active",
		"vdec_soc_gcon_lat_cken",
		"vdec_soc_gcon_larb1_cken",
		"vdec_gcon_vdec_cken_eng",
		"vdec_gcon_vdec_active",
		"vdec_gcon_vdec_cken",
		"vdec_gcon_lat_cken_eng",
		"vdec_gcon_lat_active",
		"vdec_gcon_lat_cken",
		"vdec_gcon_larb1_cken",

		/* VENC */
		"venc_c1_gcon_set5_gals",
		"venc_c1_gcon_set4_jpgdec_c1",
		"venc_c1_gcon_set3_jpgdec",
		"venc_c1_gcon_set2_jpgenc",
		"venc_c1_gcon_set1_venc",
		"venc_c1_gcon_set0_larb",
		"venc_gcon_set5_gals",
		"venc_gcon_set4_jpgdec_c1",
		"venc_gcon_set3_jpgdec",
		"venc_gcon_set2_jpgenc",
		"venc_gcon_set1_venc",
		"venc_gcon_set0_larb",

		/* SCPSYS */
		"PG_MD1",
		"PG_CONN",
		"PG_MDP",
		"PG_DIS",
		"PG_MFG0",
		"PG_MFG1",
		"PG_MFG2",
		"PG_MFG3",
		"PG_MFG4",
		"PG_MFG5",
		"PG_MFG6",
		"PG_ISP",
		"PG_ISP2",
		"PG_IPE",
		"PG_VDEC",
		"PG_VDEC2",
		"PG_VENC",
		"PG_VENC_C1",
		"PG_AUDIO",
		"PG_ADSP",
		"PG_CAM",
		"PG_CAM_RAWA",
		"PG_CAM_RAWB",
		"PG_CAM_RAWC",
		"PG_DP_TX",
		"PG_VPU",
		/* end */
		NULL
	};

	return clks;
}

static const char * const off_pll_names[] = {
	"univpll",
	"mfgpll",
	"msdcpll",
	"tvdpll",
	"mmpll",
	"apupll",
	NULL
};

static const char * const notice_pll_names[] = {
	"adsppll",
	"apll1",
	"apll2",
	NULL
};

static const char * const off_mtcmos_names[] = {
	"PG_MDP",
	"PG_DIS",
	"PG_MFG0",
	"PG_MFG1",
	"PG_MFG2",
	"PG_MFG3",
	"PG_MFG4",
	"PG_MFG5",
	"PG_MFG6",
	"PG_ISP",
	"PG_ISP2",
	"PG_IPE",
	"PG_VDEC",
	"PG_VDEC2",
	"PG_VENC",
	"PG_VENC_C1",
	"PG_CAM",
	"PG_CAM_RAWA",
	"PG_CAM_RAWB",
	"PG_CAM_RAWC",
	"PG_DP_TX",
	"PG_VPU",
	NULL
};

static const char * const notice_mtcmos_names[] = {
	"PG_MD1",
	"PG_CONN",
	"PG_ADSP",
	"PG_AUDIO",
	NULL
};

static const char *ccf_state(struct clk_hw *hw)
{
	if (__clk_get_enable_count(hw->clk))
		return "enabled";

	if (clk_hw_is_prepared(hw))
		return "prepared";

	return "disabled";
}

static void print_enabled_clks(void)
{
	const char * const *cn = get_mt6893_all_clk_names();
	const char *fix_clk = "clk26m";

	pr_notice("enabled clks:\n");

	for (; *cn; cn++) {
		int valid = 0;
		struct clk *c = __clk_lookup(*cn);
		struct clk_hw *c_hw = __clk_get_hw(c);
		struct clk_hw *p_hw;
		const char *c_name;
		const char *p_name;
		const char * const *pn;

		if (IS_ERR_OR_NULL(c) || !c_hw)
			continue;

		if (!__clk_get_enable_count(c))
			continue;

		p_hw = clk_hw_get_parent(c_hw);
		c_name = clk_hw_get_name(c_hw);
		p_name = p_hw ? clk_hw_get_name(p_hw) : 0;
		while (p_name && strcmp(p_name, fix_clk)) {
			struct clk_hw *p_hw_temp;

			p_hw_temp = clk_hw_get_parent(p_hw);
			p_name = p_hw_temp ? clk_hw_get_name(p_hw_temp) : 0;
			if (p_name && strcmp(p_name, fix_clk))
				p_hw = p_hw_temp;
			else if (p_name && !strcmp(p_name, fix_clk)) {
				c_name = clk_hw_get_name(p_hw);
				break;
			}
		}
		for (pn = off_pll_names; *pn && c_name; pn++)
			if (!strncmp(c_name, *pn, 10)) {
				valid++;
				break;
			}

		if (!valid)
			continue;

		p_hw = clk_hw_get_parent(c_hw);
		pr_notice("[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
			clk_hw_get_name(c_hw),
			ccf_state(c_hw),
			clk_hw_is_prepared(c_hw),
			__clk_get_enable_count(c),
			clk_hw_get_rate(c_hw),
			p_hw ? clk_hw_get_name(p_hw) : "- ");
	}
}

static void check_pll_off(void)
{
	static struct clk *off_plls[ARRAY_SIZE(off_pll_names)];
	struct clk **c;
	int invalid = 0;

	if (!off_plls[0]) {
		const char * const *pn;

		for (pn = off_pll_names, c = off_plls; *pn; pn++, c++)
			*c = __clk_lookup(*pn);
	}

	for (c = off_plls; *c; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (!c_hw)
			continue;

		if (!clk_hw_is_enabled(c_hw))
			continue;

		pr_notice("suspend warning[0m: %s is on\n",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid) {
		print_enabled_clks();


#ifdef CONFIG_MTK_ENG_BUILD
#if BUG_ON_CHK_ENABLE
		BUG_ON(1);
#else
		aee_kernel_warning("CCF MT6893",
			"@%s():%d, PLLs are not off\n", __func__, __LINE__);
		WARN_ON(1);
#endif
#else
		aee_kernel_warning("CCF MT6893",
			"@%s():%d, PLLs are not off\n", __func__, __LINE__);
		WARN_ON(1);
#endif
	}
}

static void check_pll_notice(void)
{
	static struct clk *off_plls[ARRAY_SIZE(notice_pll_names)];
	struct clk **c;
	int invalid = 0;

	if (!off_plls[0]) {
		const char * const *pn;

		for (pn = notice_pll_names, c = off_plls; *pn; pn++, c++)
			*c = __clk_lookup(*pn);
	}

	for (c = off_plls; *c; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (!c_hw)
			continue;

		if (!clk_hw_is_enabled(c_hw))
			continue;

		pr_notice("suspend warning[0m: %s is on\n",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid)
		print_enabled_clks();
}

static void check_mtcmos_off(void)
{
	static struct clk *off_mtcmos[ARRAY_SIZE(off_mtcmos_names)];
	struct clk **c;
	int invalid = 0;

	if (!off_mtcmos[0]) {
		const char * const *pn;

		for (pn = off_mtcmos_names, c = off_mtcmos; *pn; pn++, c++)
			*c = __clk_lookup(*pn);
	}

	for (c = off_mtcmos; *c; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (!c_hw)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))
			continue;

		pr_notice("suspend warning[0m: %s is on\n",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid) {
#ifdef CONFIG_MTK_ENG_BUILD
#if BUG_ON_CHK_ENABLE
		BUG_ON(1);
#else
		aee_kernel_warning("CCF MT6893",
			"@%s():%d, MTCMOSs are not off\n", __func__, __LINE__);
		WARN_ON(1);
#endif
#else
		aee_kernel_warning("CCF MT6893",
			"@%s():%d, MTCMOSs are not off\n", __func__, __LINE__);
		WARN_ON(1);
#endif
	}
}

static void check_mtcmos_notice(void)
{
	static struct clk *notice_mtcmos[ARRAY_SIZE(notice_mtcmos_names)];
	struct clk **c;

	if (!notice_mtcmos[0]) {
		const char * const *pn;

		for (pn = notice_mtcmos_names, c = notice_mtcmos;
				*pn; pn++, c++)
			*c = __clk_lookup(*pn);
	}

	for (c = notice_mtcmos; *c; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (!c_hw)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))
			continue;

		pr_notice("suspend warning[0m: %s\n", clk_hw_get_name(c_hw));
	}
}

void print_enabled_clks_once(void)
{
	static bool first_flag = true;

	if (first_flag) {
		first_flag = false;
		print_enabled_clks();
	}
}

static int clkchk_syscore_suspend(void)
{
	check_pll_notice();
	check_pll_off();
	check_mtcmos_notice();
	check_mtcmos_off();

	return 0;
}

static void clkchk_syscore_resume(void)
{
}

static struct syscore_ops clkchk_syscore_ops = {
	.suspend = clkchk_syscore_suspend,
	.resume = clkchk_syscore_resume,
};

/*
 *	Before MTCMOS off procedure, perform the Subsys CGs sanity check.
 */
struct pg_check_swcg {
	struct clk *c;
	const char *name;
};

#define SWCG(_name) {						\
		.name = _name,					\
	}

struct subsys_cgs_check {
	enum subsys_id id;		/* the Subsys id */
	struct pg_check_swcg *swcgs;	/* those CGs that would be checked */
	enum dbg_sys_id dbg_id;		/*
					 * subsys_name is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

/*
 * The clk names in Mediatek CCF.
 */
struct pg_check_swcg mm_swcgs[] = {
	SWCG("MM_DISP_RSZ0"),
	SWCG("MM_DISP_RSZ1"),
	SWCG("MM_DISP_OVL0"),
	SWCG("MM_INLINEROT"),
	SWCG("MM_MDP_TDSHP4"),
	SWCG("MM_MDP_TDSHP5"),
	SWCG("MM_MDP_AAL4"),
	SWCG("MM_MDP_AAL5"),
	SWCG("MM_MDP_HDR4"),
	SWCG("MM_MDP_HDR5"),
	SWCG("MM_MDP_RSZ4"),
	SWCG("MM_MDP_RSZ5"),
	SWCG("MM_MDP_RDMA4"),
	SWCG("MM_MDP_RDMA5"),
	SWCG("MM_DISP_FAKE_ENG0"),
	SWCG("MM_DISP_FAKE_ENG1"),
	SWCG("MM_DISP_OVL0_2L"),
	SWCG("MM_DISP_OVL1_2L"),
	SWCG("MM_DISP_OVL2_2L"),
	SWCG("MM_DISP_MUTEX0"),
	SWCG("MM_DISP_OVL1"),
	SWCG("MM_DISP_OVL3_2L"),
	SWCG("MM_DISP_CCORR0"),
	SWCG("MM_DISP_CCORR1"),
	SWCG("MM_DISP_COLOR0"),
	SWCG("MM_DISP_COLOR1"),
	SWCG("MM_DISP_POSTMASK0"),
	SWCG("MM_DISP_POSTMASK1"),
	SWCG("MM_DISP_DITHER0"),
	SWCG("MM_DISP_DITHER1"),
	SWCG("MM_DISP_DSI0"),
	SWCG("MM_DISP_DSI1"),
	SWCG("MM_DISP_GAMMA0"),
	SWCG("MM_DISP_GAMMA1"),
	SWCG("MM_DISP_AAL0"),
	SWCG("MM_DISP_AAL1"),
	SWCG("MM_DISP_WDMA0"),
	SWCG("MM_DISP_WDMA1"),
	SWCG("MM_DISP_RDMA0"),
	SWCG("MM_DISP_RDMA1"),
	SWCG("MM_DISP_RDMA4"),
	SWCG("MM_DISP_RDMA5"),
	SWCG("MM_DISP_DSC_WRAP"),
	SWCG("MM_DISP_DP_INTF"),
	SWCG("MM_DISP_MERGE0"),
	SWCG("MM_DISP_MERGE1"),
	SWCG("MM_SMI_COMMON"),
	SWCG("MM_SMI_GALS"),
	SWCG("MM_SMI_INFRA"),
	SWCG("MM_SMI_IOMMU"),
	SWCG("MM_DSI_DSI0"),
	SWCG("MM_DSI_DSI1"),
	SWCG("MM_DP_INTF"),
	SWCG("MM_26MHZ"),
	SWCG("MM_32KHZ"),
	SWCG(NULL),
};

struct pg_check_swcg mdp_swcgs[] = {
	SWCG("MDP_MDP_RDMA0"),
	SWCG("MDP_MDP_FG0"),
	SWCG("MDP_MDP_HDR0"),
	SWCG("MDP_MDP_AAL0"),
	SWCG("MDP_MDP_RSZ0"),
	SWCG("MDP_MDP_TDSHP0"),
	SWCG("MDP_MDP_TCC0"),
	SWCG("MDP_MDP_WROT0"),
	SWCG("MDP_MDP_RDMA2"),
	SWCG("MDP_MDP_AAL2"),
	SWCG("MDP_MDP_RSZ2"),
	SWCG("MDP_MDP_COLOR0"),
	SWCG("MDP_MDP_TDSHP2"),
	SWCG("MDP_MDP_TCC2"),
	SWCG("MDP_MDP_WROT2"),
	SWCG("MDP_MDP_MUTEX0"),
	SWCG("MDP_MDP_RDMA1"),
	SWCG("MDP_MDP_FG1"),
	SWCG("MDP_MDP_HDR1"),
	SWCG("MDP_MDP_AAL1"),
	SWCG("MDP_MDP_RSZ1"),
	SWCG("MDP_MDP_TDSHP1"),
	SWCG("MDP_MDP_TCC1"),
	SWCG("MDP_MDP_WROT1"),
	SWCG("MDP_MDP_RDMA3"),
	SWCG("MDP_MDP_AAL3"),
	SWCG("MDP_MDP_RSZ3"),
	SWCG("MDP_MDP_COLOR1"),
	SWCG("MDP_MDP_TDSHP3"),
	SWCG("MDP_MDP_TCC3"),
	SWCG("MDP_MDP_WROT3"),
	SWCG("MDP_APB_BUS"),
	SWCG("MDP_MMSYSRAM"),
	SWCG("MDP_APMCU_GALS"),
	SWCG("MDP_MDP_FAKE_ENG0"),
	SWCG("MDP_MDP_FAKE_ENG1"),
	SWCG("MDP_SMI0"),
	SWCG("MDP_IMG_DL_ASYNC0"),
	SWCG("MDP_IMG_DL_ASYNC1"),
	SWCG("MDP_IMG_DL_ASYNC2"),
	SWCG("MDP_SMI1"),
	SWCG("MDP_IMG_DL_ASYNC3"),
	SWCG("MDP_SMI2"),
	SWCG("MDP_IMG0_IMG_DL_ASYNC0"),
	SWCG("MDP_IMG0_IMG_DL_ASYNC1"),
	SWCG("MDP_IMG1_IMG_DL_ASYNC2"),
	SWCG("MDP_IMG1_IMG_DL_ASYNC3"),
	SWCG(NULL),
};
struct pg_check_swcg vde_swcgs[] = {
	SWCG("vdec_soc_gcon_larb1_cken"),
	SWCG("vdec_soc_gcon_lat_cken"),
	SWCG("vdec_soc_gcon_lat_active"),
	SWCG("vdec_soc_gcon_lat_cken_eng"),
	SWCG("vdec_soc_gcon_vdec_cken"),
	SWCG("vdec_soc_gcon_vdec_active"),
	SWCG("vdec_soc_gcon_vdec_cken_eng"),
	SWCG(NULL),
};
struct pg_check_swcg vde2_swcgs[] = {
	SWCG("vdec_gcon_larb1_cken"),
	SWCG("vdec_gcon_lat_cken"),
	SWCG("vdec_gcon_lat_active"),
	SWCG("vdec_gcon_lat_cken_eng"),
	SWCG("vdec_gcon_vdec_cken"),
	SWCG("vdec_gcon_vdec_active"),
	SWCG("vdec_gcon_vdec_cken_eng"),
	SWCG(NULL),
};
struct pg_check_swcg venc_swcgs[] = {
	SWCG("venc_gcon_set0_larb"),
	SWCG("venc_gcon_set1_venc"),
	SWCG("venc_gcon_set2_jpgenc"),
	SWCG("venc_gcon_set3_jpgdec"),
	SWCG("venc_gcon_set4_jpgdec_c1"),
	SWCG("venc_gcon_set5_gals"),
	SWCG(NULL),
};
struct pg_check_swcg venc_c1_swcgs[] = {
	SWCG("venc_c1_gcon_set0_larb"),
	SWCG("venc_c1_gcon_set1_venc"),
	SWCG("venc_c1_gcon_set2_jpgenc"),
	SWCG("venc_c1_gcon_set3_jpgdec"),
	SWCG("venc_c1_gcon_set4_jpgdec_c1"),
	SWCG("venc_c1_gcon_set5_gals"),
	SWCG(NULL),
};
struct pg_check_swcg img1_swcgs[] = {
	SWCG("imgsys1_larb9_cgpdn"),
	SWCG("imgsys1_larb10_cgpdn"),
	SWCG("imgsys1_dip_cgpdn"),
	SWCG("imgsys1_mfb_cgpdn"),
	SWCG("imgsys1_wpe_cgpdn"),
	SWCG("imgsys1_mss_cgpdn"),
	SWCG(NULL),
};
struct pg_check_swcg img2_swcgs[] = {
	SWCG("imgsys2_larb11_cgpdn"),
	SWCG("imgsys2_larb12_cgpdn"),
	SWCG("imgsys2_dip_cgpdn"),
	SWCG("imgsys2_mfb_cgpdn"),
	SWCG("imgsys2_wpe_cgpdn"),
	SWCG("imgsys2_mss_cgpdn"),
	SWCG(NULL),
};
struct pg_check_swcg ipe_swcgs[] = {
	SWCG("ipesys_larb19_cgpdn"),
	SWCG("ipesys_larb20_cgpdn"),
	SWCG("ipesys_ipe_smi_subcom_cgpdn"),
	SWCG("ipesys_fd_cgpdn"),
	SWCG("ipesys_fe_cgpdn"),
	SWCG("ipesys_rsc_cgpdn"),
	SWCG("ipesys_dpe_cgpdn"),
	SWCG(NULL),
};
struct pg_check_swcg cam_swcgs[] = {
	SWCG("camsys_main_larb13_cgpdn"),
	SWCG("camsys_main_dfp_vad_cgpdn"),
	SWCG("camsys_main_cam_cgpdn"),
	SWCG("camsys_main_camtg_cgpdn"),
	SWCG("camsys_main_larb14_cgpdn"),
	SWCG("camsys_main_larb15_cgpdn"),
	SWCG("camsys_main_seninf_cgpdn"),
	SWCG("camsys_main_camsv0_cgpdn"),
	SWCG("camsys_main_camsv1_cgpdn"),
	SWCG("camsys_main_camsv2_cgpdn"),
	SWCG("camsys_main_camsv3_cgpdn"),
	SWCG("camsys_main_ccu0_cgpdn"),
	SWCG("camsys_main_ccu1_cgpdn"),
	SWCG("camsys_main_mraw0_cgpdn"),
	SWCG("camsys_main_mraw1_cgpdn"),
	SWCG("camsys_main_fake_eng_cgpdn"),
	SWCG(NULL),
};
struct pg_check_swcg cam_rawa_swcgs[] = {
	SWCG("camsys_rawa_larbx_cgpdn"),
	SWCG("camsys_rawa_cam_cgpdn"),
	SWCG("camsys_rawa_camtg_cgpdn"),
	SWCG(NULL),
};
struct pg_check_swcg cam_rawb_swcgs[] = {
	SWCG("camsys_rawb_larbx_cgpdn"),
	SWCG("camsys_rawb_cam_cgpdn"),
	SWCG("camsys_rawb_camtg_cgpdn"),
	SWCG(NULL),
};
struct pg_check_swcg cam_rawc_swcgs[] = {
	SWCG("camsys_rawc_larbx_cgpdn"),
	SWCG("camsys_rawc_cam_cgpdn"),
	SWCG("camsys_rawc_camtg_cgpdn"),
	SWCG(NULL),
};

struct subsys_cgs_check mtk_subsys_check[] = {
	/*{SYS_DIS, mm_swcgs, NULL}, */
	{SYS_DIS, mm_swcgs, mmsys},
	{SYS_MDP, mdp_swcgs, mdpsys},
	{SYS_VDE, vde_swcgs, vdec_soc_sys},
	{SYS_VDE2, vde2_swcgs, vdecsys},
	{SYS_VEN, venc_swcgs, vencsys},
	{SYS_VEN_CORE1, venc_c1_swcgs, venc_c1_sys},
	{SYS_ISP, img1_swcgs, img1sys},
	{SYS_ISP2, img2_swcgs, img2sys},
	{SYS_IPE, ipe_swcgs, ipesys},
	{SYS_CAM, cam_swcgs, camsys},
	{SYS_CAM_RAWA, cam_rawa_swcgs, cam_rawa_sys},
	{SYS_CAM_RAWB, cam_rawb_swcgs, cam_rawb_sys},
	{SYS_CAM_RAWC, cam_rawc_swcgs, cam_rawc_sys},
};

static unsigned int check_cg_state(struct pg_check_swcg *swcg)
{
	int enable_count = 0;

	if (!swcg)
		return 0;

	while (swcg->name) {
		if (!IS_ERR_OR_NULL(swcg->c)) {
			if (__clk_get_enable_count(swcg->c) > 0) {
				pr_notice("%s[%-17s: %3d]\n",
				__func__,
				__clk_get_name(swcg->c),
				__clk_get_enable_count(swcg->c));
				enable_count++;
			}
		}
		swcg++;
	}

	return enable_count;
}

void mtk_check_subsys_swcg(enum subsys_id id)
{
	int i;
	unsigned int ret = 0;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].id != id)
			continue;

		/* check if Subsys CGs are still on */
		ret = check_cg_state(mtk_subsys_check[i].swcgs);
		if (ret) {
			pr_notice("%s:(%d) warning!\n", __func__, id);

			/* print registers dump */
			print_subsys_reg(mtk_subsys_check[i].dbg_id);
		}
	}

	if (ret) {
		pr_err("%s(%d): %d\n", __func__, id, ret);
		BUG_ON(1);
	}
}

static void __init pg_check_swcg_init_common(struct pg_check_swcg *swcg)
{
	if (!swcg)
		return;

	while (swcg->name) {
		struct clk *c = __clk_lookup(swcg->name);

		if (IS_ERR_OR_NULL(c))
			pr_notice("[%17s: NULL]\n", swcg->name);
		else
			swcg->c = c;
		swcg++;
	}
}

static int __init clkchk_init(void)
{
	/* fill the 'struct clk *' ptr of every CGs*/
	int i;

	register_syscore_ops(&clkchk_syscore_ops);

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++)
		pg_check_swcg_init_common(mtk_subsys_check[i].swcgs);

	return 0;
}
subsys_initcall(clkchk_init);

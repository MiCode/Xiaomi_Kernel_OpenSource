// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/syscore_ops.h>
#include <linux/version.h>

#include <mt-plat/aee.h>
#include "clk-mt6885-pg.h"
#include "clkchk.h"
#include "clkchk-mt6885.h"
#include "clkdbg.h"

#define TAG			"[clkchk] "
#define BUG_ON_CHK_ENABLE	1

int __attribute__((weak)) get_sw_req_vcore_opp(void)
{
	return -1;
}

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

const char * const *get_mt6893_all_clk_names(void)
{
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

/*
 * clkchk vf table
 */

struct mtk_vf {
	const char *name;
	int freq_table[4];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3},	\
	}

/*
 * Opp0 : 0.725v
 * Opp1 : 0.65v
 * Opp2 : 0.60v
 * Opp3 : 0.575v
 */
static struct mtk_vf vf_table[] = {
	/* name, opp0, opp1, opp2, opp3 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("spm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("scp_sel", 416000, 312000, 273000, 218400),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("disp_sel", 546000, 416000, 312000, 249600),
	MTK_VF_TABLE("mdp_sel", 594000, 416000, 312000, 273000),
	MTK_VF_TABLE("img1_sel", 624000, 416000, 343750, 273000),
	MTK_VF_TABLE("img2_sel", 624000, 416000, 343750, 273000),
	MTK_VF_TABLE("ipe_sel", 546000, 416000, 312000, 273000),
	MTK_VF_TABLE("dpe_sel", 546000, 458333, 364000, 312000),
	MTK_VF_TABLE("cam_sel", 624000, 499200, 392857, 312000),
	MTK_VF_TABLE("ccu_sel", 499200, 392857, 364000, 312000),
	/* APU CORE Power: 0.575v, 0.725v, 0.825v - vcore-less */
	/* GPU DVFS - vcore-less */
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("uart_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("spi_sel", 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("msdc50_0_hclk_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("msdc50_0_sel", 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("msdc30_1_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("pwrap_ulposc_sel", 65000, 65000, 65000, 65000),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("sspm_sel", 364000, 312000, 273000, 273000),
	MTK_VF_TABLE("dp_sel", 148500, 148500, 148500, 148500),
	MTK_VF_TABLE("scam_sel", 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_top_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 499200, 416000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 499200, 416000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 499200, 416000),
	MTK_VF_TABLE("seninf3_sel", 499200, 499200, 499200, 416000),
	MTK_VF_TABLE("dxcc_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("aud_engen1_sel", 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("aud_engen2_sel", 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("aes_ufsfde_sel", 546000, 546000, 546000, 416000),
	MTK_VF_TABLE("ufs_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("aud_1_sel", 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("adsp_sel", 700000, 700000, 700000, 700000),
	MTK_VF_TABLE("dpmaif_main_sel", 364000, 364000, 364000, 273000),
	MTK_VF_TABLE("venc_sel", 624000, 416000, 312000, 273000),
	MTK_VF_TABLE("vdec_sel", 546000, 416000, 312000, 249600),
	MTK_VF_TABLE("vdec_lat_sel", 546000, 458333, 356571, 312000),
	MTK_VF_TABLE("camtm_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("audio_h_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("camtg5_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg6_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("mcupm_sel", 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("spmi_mst_sel", 32500, 32500, 32500, 32500),
	MTK_VF_TABLE("dvfsrc_sel", 26000, 26000, 26000, 26000),
	{},
};

static const char *get_vf_name(int id)
{
	return vf_table[id].name;
}

static int get_vf_opp(int id, int opp)
{
	return vf_table[id].freq_table[opp];
}

static u32 get_vf_num(void)
{
	return ARRAY_SIZE(vf_table) - 1;
}

static int get_vcore_opp(void)
{
	int opp;

	opp = get_sw_req_vcore_opp();
#if defined(CONFIG_MTK_DVFSRC_MT6893_PRETEST) || defined(CONFIG_MACH_MT6893)
	if (opp >= 1)
		opp = opp - 1;
#endif

	return opp;
}

/*
 *	Before MTCMOS off procedure, perform the Subsys CGs sanity check.
 */
struct subsys_cgs_check {
	enum subsys_id id;		/* the Subsys id */
	struct pg_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * chk_id is used in
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
	{},
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
			print_subsys_reg(scpsys);
			print_subsys_reg(mtk_subsys_check[i].chk_id);
		}
	}

	if (ret) {
		pr_debug("%s(%d): %d\n", __func__, id, ret);
#if BUG_ON_CHK_ENABLE
		WARN_ON(true);
#endif
	}
}

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg) { .phys = _phys,	\
		.name = #_id_name, .pg = _pg}

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen] = REGBASE_V(0x10000000, topckgen, NULL),
	[infracfg] = REGBASE_V(0x10001000, infracfg, NULL),
	[infracfg_dbg]  = REGBASE_V(0x10001000, infracfg_dbg, NULL),
	[scpsys]   = REGBASE_V(0x10006000, scpsys, NULL),
	[apmixed]  = REGBASE_V(0x1000c000, apmixed, NULL),
	[audio]    = REGBASE_V(0x11210000, audio, "PG_AUDIO"),
	[mfgsys]   = REGBASE_V(0x13fbf000, mfgsys, "PG_MFG6"),
	[mmsys]    = REGBASE_V(0x14116000, mmsys, "PG_DIS"),
	[mdpsys]    = REGBASE_V(0x1F000000, mdpsys, "PG_MDP"),
	[img1sys]   = REGBASE_V(0x15020000, img1sys, "PG_ISP"),
	[img2sys]   = REGBASE_V(0x15820000, img2sys, "PG_ISP2"),
	[ipesys]   = REGBASE_V(0x1b000000, ipesys, "PG_IPE"),
	[camsys]   = REGBASE_V(0x1a000000, camsys, "PG_CAM"),
	[cam_rawa_sys]   = REGBASE_V(0x1a04f000, cam_rawa_sys, "PG_CAM_RAWA"),
	[cam_rawb_sys]   = REGBASE_V(0x1a06f000, cam_rawb_sys, "PG_CAM_RAWB"),
	[cam_rawc_sys]   = REGBASE_V(0x1a08f000, cam_rawc_sys, "PG_CAM_RAWC"),
	[vencsys]  = REGBASE_V(0x17000000, vencsys, "PG_VENC"),
	[venc_c1_sys]  = REGBASE_V(0x17800000, venc_c1_sys, "PG_VENC_C1"),
	[vdecsys]  = REGBASE_V(0x1602f000, vdecsys, "PG_VDEC2"),
	[vdec_soc_sys]  = REGBASE_V(0x1600f000, vdec_soc_sys, "PG_VDEC1"),
	[ipu_vcore]  = REGBASE_V(0x19029000, ipu_vcore, "PG_VPU"),
	[ipu_conn]  = REGBASE_V(0x19020000, ipu_conn, "PG_VPU"),
	[ipu0]  = REGBASE_V(0x19030000, ipu0, "PG_VPU"),
	[ipu1]  = REGBASE_V(0x19031000, ipu1, "PG_VPU"),
	[ipu2]  = REGBASE_V(0x19032000, ipu2, "PG_VPU"),
	[scp_par] = REGBASE_V(0x10720000, scp_par, NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen,  0x000, CLK_MODE),
	REGNAME(topckgen,  0x010, CLK_CFG_0),
	REGNAME(topckgen,  0x020, CLK_CFG_1),
	REGNAME(topckgen,  0x030, CLK_CFG_2),
	REGNAME(topckgen,  0x040, CLK_CFG_3),
	REGNAME(topckgen,  0x050, CLK_CFG_4),
	REGNAME(topckgen,  0x060, CLK_CFG_5),
	REGNAME(topckgen,  0x070, CLK_CFG_6),
	REGNAME(topckgen,  0x080, CLK_CFG_7),
	REGNAME(topckgen,  0x090, CLK_CFG_8),
	REGNAME(topckgen,  0x0A0, CLK_CFG_9),
	REGNAME(topckgen,  0x0B0, CLK_CFG_10),
	REGNAME(topckgen,  0x0C0, CLK_CFG_11),
	REGNAME(topckgen,  0x0D0, CLK_CFG_12),
	REGNAME(topckgen,  0x0E0, CLK_CFG_13),
	REGNAME(topckgen,  0x0F0, CLK_CFG_14),
	REGNAME(topckgen,  0x100, CLK_CFG_15),
	REGNAME(topckgen,  0x110, CLK_CFG_16),
	REGNAME(topckgen,  0x320, CLK_AUDDIV_0),
	REGNAME(topckgen,  0x328, CLK_AUDDIV_2),
	REGNAME(topckgen,  0x334, CLK_AUDDIV_3),
	REGNAME(topckgen,  0x338, CLK_AUDDIV_4),

	REGNAME(apmixed, 0x00C, AP_PLL_CON3),
	REGNAME(apmixed, 0x014, AP_PLL_CON5),
	REGNAME(apmixed, 0x040, APLL1_TUNER_CON0),
	REGNAME(apmixed, 0x044, APLL2_TUNER_CON0),
	REGNAME(apmixed, 0x050, PLLON_CON0),
	REGNAME(apmixed, 0x054, PLLON_CON1),
	REGNAME(apmixed, 0x058, PLLON_CON2),
	REGNAME(apmixed, 0x05C, PLLON_CON3),
	REGNAME(apmixed, 0x208, ARMPLL_LL_CON0),
	REGNAME(apmixed, 0x20C, ARMPLL_LL_CON1),
	REGNAME(apmixed, 0x210, ARMPLL_LL_CON2),
	REGNAME(apmixed, 0x214, ARMPLL_LL_CON3),
	REGNAME(apmixed, 0x218, ARMPLL_BL0_CON0),
	REGNAME(apmixed, 0x21C, ARMPLL_BL0_CON1),
	REGNAME(apmixed, 0x220, ARMPLL_BL0_CON2),
	REGNAME(apmixed, 0x224, ARMPLL_BL0_CON3),
	REGNAME(apmixed, 0x228, ARMPLL_BL1_CON0),
	REGNAME(apmixed, 0x22C, ARMPLL_BL1_CON1),
	REGNAME(apmixed, 0x230, ARMPLL_BL1_CON2),
	REGNAME(apmixed, 0x234, ARMPLL_BL1_CON3),
	REGNAME(apmixed, 0x238, ARMPLL_BL2_CON0),
	REGNAME(apmixed, 0x23C, ARMPLL_BL2_CON1),
	REGNAME(apmixed, 0x240, ARMPLL_BL2_CON2),
	REGNAME(apmixed, 0x244, ARMPLL_BL2_CON3),
	REGNAME(apmixed, 0x248, ARMPLL_BL3_CON0),
	REGNAME(apmixed, 0x24C, ARMPLL_BL3_CON1),
	REGNAME(apmixed, 0x250, ARMPLL_BL3_CON2),
	REGNAME(apmixed, 0x254, ARMPLL_BL3_CON3),
	REGNAME(apmixed, 0x258, CCIPLL_CON0),
	REGNAME(apmixed, 0x25C, CCIPLL_CON1),
	REGNAME(apmixed, 0x260, CCIPLL_CON2),
	REGNAME(apmixed, 0x264, CCIPLL_CON3),
	REGNAME(apmixed, 0x268, MFGPLL_CON0),
	REGNAME(apmixed, 0x26C, MFGPLL_CON1),
	REGNAME(apmixed, 0x274, MFGPLL_CON3),
	REGNAME(apmixed, 0x308, UNIVPLL_CON0),
	REGNAME(apmixed, 0x30C, UNIVPLL_CON1),
	REGNAME(apmixed, 0x314, UNIVPLL_CON3),
	REGNAME(apmixed, 0x318, APLL1_CON0),
	REGNAME(apmixed, 0x31C, APLL1_CON1),
	REGNAME(apmixed, 0x320, APLL1_CON2),
	REGNAME(apmixed, 0x324, APLL1_CON3),
	REGNAME(apmixed, 0x328, APLL1_CON4),
	REGNAME(apmixed, 0x32C, APLL2_CON0),
	REGNAME(apmixed, 0x330, APLL2_CON1),
	REGNAME(apmixed, 0x334, APLL2_CON2),
	REGNAME(apmixed, 0x338, APLL2_CON3),
	REGNAME(apmixed, 0x33C, APLL2_CON4),
	REGNAME(apmixed, 0x340, MAINPLL_CON0),
	REGNAME(apmixed, 0x344, MAINPLL_CON1),
	REGNAME(apmixed, 0x34C, MAINPLL_CON3),
	REGNAME(apmixed, 0x350, MSDCPLL_CON0),
	REGNAME(apmixed, 0x354, MSDCPLL_CON1),
	REGNAME(apmixed, 0x35C, MSDCPLL_CON3),
	REGNAME(apmixed, 0x360, MMPLL_CON0),
	REGNAME(apmixed, 0x364, MMPLL_CON1),
	REGNAME(apmixed, 0x36C, MMPLL_CON3),
	REGNAME(apmixed, 0x370, ADSPPLL_CON0),
	REGNAME(apmixed, 0x374, ADSPPLL_CON1),
	REGNAME(apmixed, 0x37C, ADSPPLL_CON3),
	REGNAME(apmixed, 0x380, TVDPLL_CON0),
	REGNAME(apmixed, 0x384, TVDPLL_CON1),
	REGNAME(apmixed, 0x38C, TVDPLL_CON3),
	REGNAME(apmixed, 0x390, MPLL_CON0),
	REGNAME(apmixed, 0x394, MPLL_CON1),
	REGNAME(apmixed, 0x39C, MPLL_CON3),
	REGNAME(apmixed, 0x3A0, APUPLL_CON0),
	REGNAME(apmixed, 0x3A4, APUPLL_CON1),
	REGNAME(apmixed, 0x3AC, APUPLL_CON3),

	REGNAME(scpsys, 0x0000, POWERON_CONFIG_EN),
	REGNAME(scpsys, 0x016C, PWR_STATUS),
	REGNAME(scpsys, 0x0170, PWR_STATUS_2ND),
	REGNAME(scpsys, 0x0178, OTHER_PWR_STATUS),
	REGNAME(scpsys, 0x300, MD1_PWR_CON),
	REGNAME(scpsys, 0x304, CONN_PWR_CON),
	REGNAME(scpsys, 0x308, MFG0_PWR_CON),
	REGNAME(scpsys, 0x30C, MFG1_PWR_CON),
	REGNAME(scpsys, 0x310, MFG2_PWR_CON),
	REGNAME(scpsys, 0x314, MFG3_PWR_CON),
	REGNAME(scpsys, 0x318, MFG4_PWR_CON),
	REGNAME(scpsys, 0x31C, MFG5_PWR_CON),
	REGNAME(scpsys, 0x320, MFG6_PWR_CON),
	REGNAME(scpsys, 0x324, IFR_PWR_CON),
	REGNAME(scpsys, 0x328, IFR_SUB_PWR_CON),
	REGNAME(scpsys, 0x32C, DPY_PWR_CON),
	REGNAME(scpsys, 0x330, ISP_PWR_CON),
	REGNAME(scpsys, 0x334, ISP2_PWR_CON),
	REGNAME(scpsys, 0x338, IPE_PWR_CON),
	REGNAME(scpsys, 0x33C, VDE_PWR_CON),
	REGNAME(scpsys, 0x340, VDE2_PWR_CON),
	REGNAME(scpsys, 0x344, VEN_PWR_CON),
	REGNAME(scpsys, 0x348, VEN_CORE1_PWR_CON),
	REGNAME(scpsys, 0x34C, MDP_PWR_CON),
	REGNAME(scpsys, 0x350, DIS_PWR_CON),
	REGNAME(scpsys, 0x354, AUDIO_PWR_CON),
	REGNAME(scpsys, 0x358, ADSP_PWR_CON),
	REGNAME(scpsys, 0x35C, CAM_PWR_CON),
	REGNAME(scpsys, 0x360, CAM_RAWA_PWR_CON),
	REGNAME(scpsys, 0x364, CAM_RAWB_PWR_CON),
	REGNAME(scpsys, 0x368, CAM_RAWC_PWR_CON),
	REGNAME(scpsys, 0x3AC, DP_TX_PWR_CON),
	REGNAME(scpsys, 0x3C4, DPY2_PWR_CON),
	REGNAME(scpsys, 0x670, SPM_CROSS_WAKE_M01_REQ),
	REGNAME(scpsys, 0x398, MD_EXT_BUCK_ISO_CON),
	REGNAME(scpsys, 0x39C, EXT_BUCK_ISO),

	REGNAME(audio, 0x0000, AUDIO_TOP_CON0),
	REGNAME(audio, 0x0004, AUDIO_TOP_CON1),
	REGNAME(audio, 0x0008, AUDIO_TOP_CON2),

	REGNAME(scp_par, 0x0180, ADSP_SW_CG),

	REGNAME(camsys, 0x0000, CAMSYS_CG_CON),
	REGNAME(cam_rawa_sys, 0x0000, CAMSYS_RAWA_CG_CON),
	REGNAME(cam_rawb_sys, 0x0000, CAMSYS_RAWB_CG_CON),
	REGNAME(cam_rawc_sys, 0x0000, CAMSYS_RAWC_CG_CON),

	REGNAME(img1sys, 0x0000, IMG1_CG_CON),
	REGNAME(img2sys, 0x0000, IMG2_CG_CON),
	REGNAME(ipesys, 0x0000, IPE_CG_CON),

	REGNAME(infracfg,  0x090, MODULE_SW_CG_0),
	REGNAME(infracfg,  0x094, MODULE_SW_CG_1),
	REGNAME(infracfg,  0x0ac, MODULE_SW_CG_2),
	REGNAME(infracfg,  0x0c8, MODULE_SW_CG_3),
	REGNAME(infracfg,  0x0d8, MODULE_SW_CG_4),
	REGNAME(infracfg,  0xe98, MM_HANG_FREE),

	/* BUS STATUS */
	REGNAME(infracfg_dbg,  0x0220, INFRA_TOPAXI_PROTECTEN),
	REGNAME(infracfg_dbg,  0x0224, INFRA_TOPAXI_PROTECTEN_STA0),
	REGNAME(infracfg_dbg,  0x0228, INFRA_TOPAXI_PROTECTEN_STA1),
	REGNAME(infracfg_dbg,  0x0250, INFRA_TOPAXI_PROTECTEN_1),
	REGNAME(infracfg_dbg,  0x0254, INFRA_TOPAXI_PROTECTEN_STA0_1),
	REGNAME(infracfg_dbg,  0x0258, INFRA_TOPAXI_PROTECTEN_STA1_1),
	REGNAME(infracfg_dbg,  0x02C0, INFRA_TOPAXI_PROTECTEN_MCU),
	REGNAME(infracfg_dbg,  0x02C4, INFRA_TOPAXI_PROTECTEN_MCU_STA0),
	REGNAME(infracfg_dbg,  0x02C8, INFRA_TOPAXI_PROTECTEN_MCU_STA1),
	REGNAME(infracfg_dbg,  0x02D0, INFRA_TOPAXI_PROTECTEN_MM),
	REGNAME(infracfg_dbg,  0x02E8, INFRA_TOPAXI_PROTECTEN_MM_STA0),
	REGNAME(infracfg_dbg,  0x02EC, INFRA_TOPAXI_PROTECTEN_MM_STA1),
	REGNAME(infracfg_dbg,  0x0710, INFRA_TOPAXI_PROTECTEN_2),
	REGNAME(infracfg_dbg,  0x0720, INFRA_TOPAXI_PROTECTEN_STA0_2),
	REGNAME(infracfg_dbg,  0x0724, INFRA_TOPAXI_PROTECTEN_STA1_2),
	REGNAME(infracfg_dbg,  0x0DC8, INFRA_TOPAXI_PROTECTEN_MM_2),
	REGNAME(infracfg_dbg,  0x0DD4, INFRA_TOPAXI_PROTECTEN_MM_2_STA0),
	REGNAME(infracfg_dbg,  0x0DD8, INFRA_TOPAXI_PROTECTEN_MM_2_STA1),
	REGNAME(infracfg_dbg,  0x0B80, INFRA_TOPAXI_PROTECTEN_VDNR),
	REGNAME(infracfg_dbg,  0x0B8C, INFRA_TOPAXI_PROTECTEN_VDNR_STA0),
	REGNAME(infracfg_dbg,  0x0B90, INFRA_TOPAXI_PROTECTEN_VDNR_STA1),
	REGNAME(infracfg_dbg,  0x0BA0, INFRA_TOPAXI_PROTECTEN_VDNR1),
	REGNAME(infracfg_dbg,  0x0BAC, INFRA_TOPAXI_PROTECTEN_VDNR_1_STA0),
	REGNAME(infracfg_dbg,  0x0BB0, INFRA_TOPAXI_PROTECTEN_VDNR_1_STA1),
	REGNAME(infracfg_dbg,  0x0BB4, INFRA_TOPAXI_PROTECTEN_SUB_VDNR),
	REGNAME(infracfg_dbg,  0x0BC0, INFRA_TOPAXI_PROTECTEN_SUB_VDNR_STA0),
	REGNAME(infracfg_dbg,  0x0BC4, INFRA_TOPAXI_PROTECTEN_SUB_VDNR_STA1),

	REGNAME(ipu0,  0x000, IPU0_CORE_CG),
	REGNAME(ipu1,  0x000, IPU1_CORE_CG),
	REGNAME(ipu2,  0x000, IPU2_CORE_CG),
	REGNAME(ipu_conn,  0x000, IPU_CONN_CG),
	REGNAME(ipu_vcore,  0x000, IPU_VCORE_CG),

	REGNAME(mfgsys, 0x0000, MFG_CG_CON),

	REGNAME(mmsys, 0x100, MM_CG_CON0),
	REGNAME(mmsys, 0x110, MM_CG_CON1),
	REGNAME(mmsys, 0x1a0, MM_CG_CON2),

	REGNAME(mdpsys, 0x100, MDP_CG_CON0),
	REGNAME(mdpsys, 0x104, MDP_CG_SET0),
	REGNAME(mdpsys, 0x114, MDP_CG_SET1),
	REGNAME(mdpsys, 0x124, MDP_CG_SET2),


	REGNAME(vdecsys, 0x0000, VDEC_CKEN_SET),
	REGNAME(vdecsys, 0x0008, VDEC_LARB1_CKEN_SET),
	REGNAME(vdecsys, 0x0200, VDEC_LAT_CKEN_SET),

	REGNAME(vdec_soc_sys, 0x0000, VDEC_SOC_CKEN_SET),
	REGNAME(vdec_soc_sys, 0x0008, VDEC_SOC_LARB1_CKEN_SET),
	REGNAME(vdec_soc_sys, 0x0200, VDEC_SOC_LAT_CKEN_SET),

	REGNAME(venc_c1_sys, 0x0000, VENC_C1_CG_CON),

	REGNAME(vencsys, 0x0000, VENC_CG_CON),
	{},
};

struct regbase *get_mt6893_all_reg_bases(void)
{
	return rb;
}

struct regname *get_mt6893_all_reg_names(void)
{
	return rn;
}

void print_subsys_reg(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int i;

	if (rns == NULL)
		return;

	if (id >= chk_sys_num || id < 0) {
		pr_info("wrong id:%d\n", id);
		return;
	}

	rb_dump = &rb[id];

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}

static void devapc_dump(void)
{
	print_subsys_reg(scpsys);
	print_subsys_reg(topckgen);
	print_subsys_reg(infracfg);
	print_subsys_reg(infracfg_dbg);
	print_subsys_reg(apmixed);
}

static void __init init_regbase(void)
{
	struct regbase *rb = get_mt6893_all_reg_bases();

	for (; rb->name; rb++) {
		if (!rb->phys)
			continue;

		rb->virt = ioremap_nocache(rb->phys, 0x1000);
	}
}

static const char * const compatible[] = {"mediatek,mt6893", "mediatek,mt6885", NULL};

static struct clkchk_cfg_t cfg = {
	.aee_excp_on_fail = false,
#ifdef CONFIG_MTK_ENG_BUILD
#if BUG_ON_CHK_ENABLE
	.bug_on_fail = true,
#else
	.bug_on_fail = false,
#endif
	.bug_on_fail = false,
#endif
	.warn_on_fail = true,
	.compatible = compatible,
	.off_pll_names = off_pll_names,
	.notice_pll_names = notice_pll_names,
	.off_mtcmos_names = off_mtcmos_names,
	.notice_mtcmos_names = notice_mtcmos_names,
	.all_clk_names = clks,
	.get_vf_name = get_vf_name,
	.get_vf_opp = get_vf_opp,
	.get_vf_num = get_vf_num,
	.get_vcore_opp = get_vcore_opp,
	.get_devapc_dump = devapc_dump,
};

static int __init clkchk_mt6893_init(void)
{
	int i;

	init_regbase();

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++)
		clkchk_swcg_init(mtk_subsys_check[i].swcgs);

	return clkchk_init(&cfg);
}
subsys_initcall(clkchk_mt6893_init);

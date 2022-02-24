// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk-provider.h>
#include <linux/syscore_ops.h>
#include <linux/version.h>

#include <mt-plat/aee.h>
#include "clk-mt6853-pg.h"
#include "clkchk.h"
#include "clkdbg.h"
#include "clkchk-mt6853.h"

#define TAG			"[clkchk] "
#define BUG_ON_CHK_ENABLE	1

int __attribute__((weak)) get_sw_req_vcore_opp(void)
{
	return -1;
}

static const char * const clks[] = {
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
	"mipid0",

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
	"ifrao_infra_force",
	"ifrao_pmic_tmr",
	"ifrao_pmic_ap",
	"ifrao_pmic_md",
	"ifrao_pmic_conn",
	"ifrao_sej",
	"ifrao_apxgpt",
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
	"ifrao_dvfsrc",
	"ifrao_trng",
	"ifrao_auxadc",
	"ifrao_cpum",
	"ifrao_ccif1_ap",
	"ifrao_ccif1_md",
	"ifrao_auxadc_md",
	"ifrao_pcie_tl_26m",
	"ifrao_msdc1_clk",
	"ifrao_msdc0_aes_clk",
	"ifrao_pcie_tl_96m",
	"ifrao_pcie_pl_p_250m",
	"ifrao_dapc",
	"ifrao_ccif_ap",
	"ifrao_debugsys",
	"ifrao_audio",
	"ifrao_ccif_md",
	"ifrao_secore",
	"ifrao_dxcc_ao",
	"ifrao_dbg_trace",
	"ifrao_dramc26",
	"ifrao_ssusb",
	"ifrao_disp_pwm",
	"ifrao_cldmabclk",
	"ifrao_audio26m",
	"ifrao_mdtemp",
	"ifrao_spi1",
	"ifrao_i2c4",
	"ifrao_spi2",
	"ifrao_spi3",
	"ifrao_unipro_sysclk",
	"ifrao_unipro_tick",
	"ifrao_ufs_bclk",
	"ifrao_fsspm",
	"ifrao_sspm_hclk",
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
	"ifrao_sspm_26m",
	"ifrao_sspm_32k",
	"ifrao_i2c6",
	"ifrao_ap_msdc0",
	"ifrao_md_msdc0",
	"ifrao_ccif5_ap",
	"ifrao_ccif5_md",
	"ifrao_flashif_h_133m",
	"ifrao_ccif2_ap",
	"ifrao_ccif2_md",
	"ifrao_ccif3_ap",
	"ifrao_ccif3_md",
	"ifrao_sej_f13m",
	"ifrao_i2c7",
	"ifrao_i2c8",
	"ifrao_fbist2fpc",
	"ifrao_dapc_sync",
	"ifrao_dpmaif_main",
	"ifrao_ccif4_ap",
	"ifrao_ccif4_md",
	"ifrao_spi6_ck",
	"ifrao_spi7_ck",
	"ifrao_133m_mclk_ck",
	"ifrao_66m_mclk_ck",
	"ifrao_66m_peri_mclk",
	"ifrao_infra_133m",
	"ifrao_infra_66m",
	"ifrao_peru_bus_133m",
	"ifrao_peru_bus_66m",
	"ifrao_flash_26m",
	"ifrao_sflash_ck",
	"ifrao_ap_dma",
	"ifrao_peri_force",

	/* pericfg */
	"periaxi_disable",

	/* scp */
	"scp_par_adsp_pll",

	/* imp_iic_wrap_c */
	"impc_ap_i2c10",
	"impc_ap_i2c11",

	/* audio */
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

	/* msdc0sys */
	"msdc0_axi_wrap_cken",

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
	"impw_ap_i2c6",

	/* imp_iic_wrap_n */
	"impn_ap_i2c0",

	/* mfgsys */
	"mfgcfg_bg3d",

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
	"venc_set0_larb",
	"venc_set1_venc",
	"jpgenc",
	"venc_set5_gals",

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


	/* SCPSYS */
	"PG_MD1",
	"PG_CONN",
	"PG_MDP",
	"PG_DIS",
	"PG_MFG0",
	"PG_MFG1",
	"PG_MFG2",
	"PG_MFG3",
	"PG_MFG5",
	"PG_ISP",
	"PG_ISP2",
	"PG_IPE",
	"PG_VDEC",
	"PG_VENC",
	"PG_AUDIO",
	"PG_ADSP",
	"PG_CAM",
	"PG_CAM_RAWA",
	"PG_CAM_RAWB",
	"PG_VPU",
	/* end */
	NULL
};

const char * const *get_mt6853_all_clk_names(void)
{
	return clks;
}

static const char * const off_pll_names[] = {
	"univpll",
	"msdcpll",
	"mmpll",
	"tvdpll",
	"npupll",
	"usbpll",
	NULL
};

static const char * const notice_pll_names[] = {
	"adsppll",
	"apll1",
	"apll2",
	NULL
};

static const char * const off_mtcmos_names[] = {
	"PG_DIS",
	"PG_MFG0",
	"PG_MFG1",
	"PG_MFG2",
	"PG_MFG3",
	"PG_MFG5",
	"PG_ISP",
	"PG_ISP2",
	"PG_IPE",
	"PG_VDEC",
	"PG_VENC",
	"PG_CAM",
	"PG_CAM_RAWA",
	"PG_CAM_RAWB",
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

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3},	\
	}

struct mtk_vf {
	const char *name;
	int freq_table[4];
};
/*
 * Opp0 : 0.725v
 * Opp1 : 0.65v
 * Opp2 : 0.60v
 * Opp3 : 0.55v
 */
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("spm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("scp_sel", 624000, 416000, 364000, 273000),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("disp_sel", 546000, 416000, 312000, 208000),
	MTK_VF_TABLE("mdp_sel", 594000, 436800, 343750, 275000),
	MTK_VF_TABLE("img1_sel", 624000, 458333, 343750, 275000),
	MTK_VF_TABLE("img2_sel", 624000, 458333, 343750, 275000),
	MTK_VF_TABLE("ipe_sel", 546000, 416000, 312000, 275000),
	MTK_VF_TABLE("dpe_sel", 546000, 458333, 364000, 249600),
	MTK_VF_TABLE("cam_sel", 624000, 499200, 392857, 273000),
	MTK_VF_TABLE("ccu_sel", 499200, 392857, 364000, 275000),
	/* MTK_VF_TABLE("dsp_sel", 728000, 728000, 499200, 242666), */
	/* MTK_VF_TABLE("dsp1_sel", 624000, 624000, 546000, 273000), */
	/* MTK_VF_TABLE("dsp2_sel", 624000, 624000, 546000, 273000), */
	MTK_VF_TABLE("ipu_if_sel", 546000, 416000, 312000, 208000),
	/* MTK_VF_TABLE("mfg_ref_sel", 416000, 416000, 416000, 416000), */
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg5_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("uart_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("spi_sel", 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("msdc50_0_hclk_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("msdc50_0_sel", 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("msdc30_1_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("pwrap_ulposc_sel", 65000, 65000, 65000, 65000),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("sspm_sel", 364000, 312000, 273000, 242666),
	MTK_VF_TABLE("scam_sel", 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_top_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 392857, 273000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 392857, 273000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 392857, 273000),
	MTK_VF_TABLE("dxcc_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("aud_engen1_sel", 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("aud_engen2_sel", 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("aes_ufsfde_sel", 546000, 546000, 546000, 416000),
	MTK_VF_TABLE("ufs_sel", 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("aud_1_sel", 180633, 180633, 180633, 180633),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("adsp_sel", 750000, 750000, 750000, 750000),
	MTK_VF_TABLE("dpmaif_main_sel", 364000, 364000, 364000, 273000),
	MTK_VF_TABLE("venc_sel", 624000, 458333, 364000, 249600),
	MTK_VF_TABLE("vdec_sel", 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("camtm_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("audio_h_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("spmi_mst_sel", 32500, 32500, 32500, 32500),
	MTK_VF_TABLE("dvfsrc_sel", 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("aes_msdcfde_sel", 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("mcupm_sel", 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("sflash_sel", 62400, 62400, 62400, 62400),
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
#if defined(CONFIG_MTK_DVFSRC_MT6877_PRETEST)
	if (opp >= 1)
		opp = opp - 1;
#endif

	return opp;
}

/*
 * The clk names in Mediatek CCF.
 */

struct subsys_cgs_check {
	enum subsys_id id;		/* the Subsys id */
	struct pg_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * subsys_name is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

struct pg_check_swcg mm_swcgs[] = {
	SWCG("mm_disp_mutex0"),
	SWCG("mm_apb_bus"),
	SWCG("mm_disp_ovl0"),
	SWCG("mm_disp_rdma0"),
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_disp_wdma0"),
	SWCG("mm_disp_ccorr1"),
	SWCG("mm_disp_rsz0"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_ccorr0"),
	SWCG("mm_disp_color0"),
	SWCG("mm_smi_infra"),
	SWCG("mm_disp_dsc_wrap"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_spr0"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_smi_common"),
	SWCG("mm_disp_cm0"),
	SWCG("mm_dsi0"),
	SWCG("mm_disp_fake_eng0"),
	SWCG("mm_disp_fake_eng1"),
	SWCG("mm_smi_gals"),
	SWCG("mm_smi_iommu"),
	SWCG(NULL),
};

struct pg_check_swcg mdp_swcgs[] = {
	SWCG("mdp_rdma0"),
	SWCG("mdp_tdshp0"),
	SWCG("mdp_img_dl_async0"),
	SWCG("mdp_img_dl_async1"),
	SWCG("mdp_rdma1"),
	SWCG("mdp_tdshp1"),
	SWCG("mdp_smi0"),
	SWCG("mdp_apb_bus"),
	SWCG("mdp_wrot0"),
	SWCG("mdp_rsz0"),
	SWCG("mdp_hdr0"),
	SWCG("mdp_mutex0"),
	SWCG("mdp_wrot1"),
	SWCG("mdp_rsz1"),
	SWCG("mdp_fake_eng0"),
	SWCG("mdp_aal0"),
	SWCG("mdp_aal1"),
	SWCG("mdp_color0"),
	SWCG("mdp_img_dl_rel0_as0"),
	SWCG("mdp_img_dl_rel1_as1"),
	SWCG(NULL),
};

struct pg_check_swcg vdec_swcgs[] = {
	SWCG("vdec_larb1_cken"),
	SWCG("vdec_cken"),
	SWCG("vdec_active"),
	SWCG(NULL),
};

struct pg_check_swcg venc_swcgs[] = {
	SWCG("venc_set0_larb"),
	SWCG("venc_set1_venc"),
	SWCG("jpgenc"),
	SWCG("venc_set5_gals"),
	SWCG(NULL),
};

struct pg_check_swcg img1_swcgs[] = {
	SWCG("imgsys1_larb9"),
	SWCG("imgsys1_larb10"),
	SWCG("imgsys1_dip"),
	SWCG("imgsys1_gals"),
	SWCG(NULL),
};

struct pg_check_swcg img2_swcgs[] = {
	SWCG("imgsys2_larb9"),
	SWCG("imgsys2_larb10"),
	SWCG("imgsys2_mfb"),
	SWCG("imgsys2_wpe"),
	SWCG("imgsys2_mss"),
	SWCG("imgsys2_gals"),
	SWCG(NULL),
};

struct pg_check_swcg ipe_swcgs[] = {
	SWCG("ipe_larb19"),
	SWCG("ipe_larb20"),
	SWCG("ipe_smi_subcom"),
	SWCG("ipe_fd"),
	SWCG("ipe_fe"),
	SWCG("ipe_rsc"),
	SWCG("ipe_dpe"),
	SWCG("ipe_gals"),
	SWCG(NULL),
};

struct pg_check_swcg cam_swcgs[] = {
	SWCG("cam_m_larb13"),
	SWCG("cam_m_larb14"),
	SWCG("cam_m_reserved0"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv1"),
	SWCG("cam_m_camsv2"),
	SWCG("cam_m_camsv3"),
	SWCG("cam_m_ccu0"),
	SWCG("cam_m_ccu1"),
	SWCG("cam_m_mraw0"),
	SWCG("cam_m_reserved2"),
	SWCG("cam_m_fake_eng"),
	SWCG("cam_m_ccu_gals"),
	SWCG("cam_m_cam2mm_gals"),
	SWCG(NULL),
};

struct pg_check_swcg cam_rawa_swcgs[] = {
	SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG(NULL),
};

struct pg_check_swcg cam_rawb_swcgs[] = {
	SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG(NULL),
};

struct subsys_cgs_check mtk_subsys_check[] = {
	/*{SYS_DIS, mm_swcgs, NULL}, */
	{SYS_DIS, mm_swcgs, mmsys},
	{SYS_DIS, mdp_swcgs, mdpsys},
	{SYS_VDE, vdec_swcgs, vdecsys},
	{SYS_VEN, venc_swcgs, vencsys},
	{SYS_ISP, img1_swcgs, img1sys},
	{SYS_ISP2, img2_swcgs, img2sys},
	{SYS_IPE, ipe_swcgs, ipesys},
	{SYS_CAM, cam_swcgs, camsys},
	{SYS_CAM_RAWA, cam_rawa_swcgs, cam_rawa_sys},
	{SYS_CAM_RAWB, cam_rawb_swcgs, cam_rawb_sys},
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
		pr_notice("%s(%d): %d\n", __func__, id, ret);
		BUG_ON(1);
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
	[infracfg_ao] = REGBASE_V(0x10001000, infracfg_ao, NULL),
	[scpsys]   = REGBASE_V(0x10006000, scpsys, NULL),
	[apmixed]  = REGBASE_V(0x1000c000, apmixed, NULL),
	[apu0]    = REGBASE_V(0x19030000, apu0, "PG_VPU"),
	[apu1]    = REGBASE_V(0x19031000, apu1, "PG_VPU"),
	[apuvc]    = REGBASE_V(0x19029000, apuvc, "PG_VPU"),
	[apuc]    = REGBASE_V(0x19020000, apuc, "PG_VPU"),
	[audio]    = REGBASE_V(0x11210000, audio, "PG_AUDIO"),
	[mfgsys]   = REGBASE_V(0x13fbf000, mfgsys, "PG_MFG5"),
	[mmsys]    = REGBASE_V(0x14000000, mmsys, "PG_DIS"),
	[mdpsys]    = REGBASE_V(0x1F000000, mdpsys, "PG_DIS"),
	[img1sys]   = REGBASE_V(0x15020000, img1sys, "PG_ISP"),
	[img2sys]   = REGBASE_V(0x15820000, img2sys, "PG_ISP2"),
	[i2c_c] = REGBASE_V(0x11007000, i2c_c, "i2c_sel"),
	[i2c_e] = REGBASE_V(0x11cb1000, i2c_e, "i2c_sel"),
	[i2c_n] = REGBASE_V(0x11f01000, i2c_n, "i2c_sel"),
	[i2c_s] = REGBASE_V(0x11d04000, i2c_s, "i2c_sel"),
	[i2c_w] = REGBASE_V(0x11e01000, i2c_w, "i2c_sel"),
	[i2c_ws] = REGBASE_V(0x11d23000, i2c_ws, "i2c_sel"),
	[infracfg] = REGBASE_V(0x1020E000, infracfg, NULL),
	[ipesys]   = REGBASE_V(0x1b000000, ipesys, "PG_IPE"),
	[camsys]   = REGBASE_V(0x1a000000, camsys, "PG_CAM"),
	[cam_rawa_sys]   = REGBASE_V(0x1a04f000, cam_rawa_sys, "PG_CAM_RAWA"),
	[cam_rawb_sys]   = REGBASE_V(0x1a06f000, cam_rawb_sys, "PG_CAM_RAWB"),
	[pericfg] = REGBASE_V(0x10003000, pericfg, NULL),
	[scp_par] = REGBASE_V(0x10720000, scp_par, NULL),
	[vencsys]  = REGBASE_V(0x17000000, vencsys, "PG_VENC"),
	[vdecsys]  = REGBASE_V(0x1602f000, vdecsys, "PG_VDEC"),
	[infracfg_dbg]  = REGBASE_V(0x10001000, infracfg_dbg, NULL),
	[infrapdn_dbg]  = REGBASE_V(0x10215000, infrapdn_dbg, NULL),
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
	REGNAME(apmixed, 0x258, CCIPLL_CON0),
	REGNAME(apmixed, 0x25C, CCIPLL_CON1),
	REGNAME(apmixed, 0x260, CCIPLL_CON2),
	REGNAME(apmixed, 0x264, CCIPLL_CON3),
	REGNAME(apmixed, 0x268, MFGPLL_CON0),
	REGNAME(apmixed, 0x26C, MFGPLL_CON1),
	REGNAME(apmixed, 0x270, MFGPLL_CON2),
	REGNAME(apmixed, 0x274, MFGPLL_CON3),
	REGNAME(apmixed, 0x308, UNIVPLL_CON0),
	REGNAME(apmixed, 0x30C, UNIVPLL_CON1),
	REGNAME(apmixed, 0x310, UNIVPLL_CON2),
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
	REGNAME(apmixed, 0x348, MAINPLL_CON2),
	REGNAME(apmixed, 0x34C, MAINPLL_CON3),
	REGNAME(apmixed, 0x350, MSDCPLL_CON0),
	REGNAME(apmixed, 0x354, MSDCPLL_CON1),
	REGNAME(apmixed, 0x358, MSDCPLL_CON2),
	REGNAME(apmixed, 0x35C, MSDCPLL_CON3),
	REGNAME(apmixed, 0x360, MMPLL_CON0),
	REGNAME(apmixed, 0x364, MMPLL_CON1),
	REGNAME(apmixed, 0x368, MMPLL_CON2),
	REGNAME(apmixed, 0x36C, MMPLL_CON3),
	REGNAME(apmixed, 0x370, ADSPPLL_CON0),
	REGNAME(apmixed, 0x374, ADSPPLL_CON1),
	REGNAME(apmixed, 0x378, ADSPPLL_CON2),
	REGNAME(apmixed, 0x37C, ADSPPLL_CON3),
	REGNAME(apmixed, 0x380, TVDPLL_CON0),
	REGNAME(apmixed, 0x384, TVDPLL_CON1),
	REGNAME(apmixed, 0x388, TVDPLL_CON2),
	REGNAME(apmixed, 0x38C, TVDPLL_CON3),
	REGNAME(apmixed, 0x390, MPLL_CON0),
	REGNAME(apmixed, 0x394, MPLL_CON1),
	REGNAME(apmixed, 0x398, MPLL_CON2),
	REGNAME(apmixed, 0x39C, MPLL_CON3),
	REGNAME(apmixed, 0x3B4, NPUPLL_CON0),
	REGNAME(apmixed, 0x3B8, NPUPLL_CON1),
	REGNAME(apmixed, 0x3BC, NPUPLL_CON2),
	REGNAME(apmixed, 0x3C0, NPUPLL_CON3),
	REGNAME(apmixed, 0x3C4, USBPLL_CON0),
	REGNAME(apmixed, 0x3C8, USBPLL_CON1),
	REGNAME(apmixed, 0x3CC, USBPLL_CON2),

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

	REGNAME(apu0, 0x0100, CORE_CG),
	REGNAME(apu0, 0x0910, CORE_CTRL),

	REGNAME(apu1, 0x0100, CORE_CG),
	REGNAME(apu1, 0x0910, CORE_CTRL),

	REGNAME(apuvc, 0x0000, APUSYS_VCORE_CG),

	REGNAME(apuc, 0x0000, APU_CONN_CG),

	REGNAME(audio, 0x0000, AUDIO_TOP_CON0),
	REGNAME(audio, 0x0004, AUDIO_TOP_CON1),
	REGNAME(audio, 0x0008, AUDIO_TOP_CON2),

	REGNAME(scp_par, 0x0180, ADSP_SW_CG),

	REGNAME(camsys, 0x0000, CAMSYS_CG_CON),
	REGNAME(cam_rawa_sys, 0x0000, CAMSYS_RAWA_CG_CON),
	REGNAME(cam_rawb_sys, 0x0000, CAMSYS_RAWB_CG_CON),

	REGNAME(img1sys, 0x0000, IMG1_CG_CON),
	REGNAME(img2sys, 0x0000, IMG2_CG_CON),
	REGNAME(ipesys, 0x0000, IPE_CG_CON),

	REGNAME(i2c_e,  0xe00, AP_CLOCK_CG_RO_EST),
	REGNAME(i2c_n,  0xe00, AP_CLOCK_CG_RO_NOR),
	REGNAME(i2c_s,  0xe00, AP_CLOCK_CG_RO_SOU),
	REGNAME(i2c_w,  0xe00, AP_CLOCK_CG_RO_WST),
	REGNAME(i2c_ws,  0xe00, AP_CLOCK_CG_RO_WEST_SOU),

	REGNAME(infracfg,  0xb00, BUS_MON_CKEN),

	REGNAME(infracfg_ao,  0x090, MODULE_SW_CG_0),
	REGNAME(infracfg_ao,  0x094, MODULE_SW_CG_1),
	REGNAME(infracfg_ao,  0x0ac, MODULE_SW_CG_2),
	REGNAME(infracfg_ao,  0x0c8, MODULE_SW_CG_3),
	REGNAME(infracfg_ao,  0x0e8, MODULE_SW_CG_4),
	REGNAME(infracfg_ao,  0x0d8, MODULE_SW_CG_5),
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
	REGNAME(infrapdn_dbg,  0x004C, INFRA_PDN_MFG1_WAY_EN),

	REGNAME(mfgsys, 0x0000, MFG_CG_CON),

	REGNAME(mmsys, 0x100, MM_CG_CON0),
	REGNAME(mmsys, 0x110, MM_CG_CON1),
	REGNAME(mmsys, 0x1a0, MM_CG_CON2),

	REGNAME(pericfg, 0x20C, PERIAXI_SI0_CTL),

	REGNAME(mdpsys, 0x100, MDP_CG_CON0),
	REGNAME(mdpsys, 0x120, MDP_CG_CON2),

	REGNAME(vdecsys, 0x0000, VDEC_CKEN_SET),
	REGNAME(vdecsys, 0x0008, VDEC_LARB1_CKEN_SET),

	REGNAME(vencsys, 0x0000, VENC_CG_CON),
	{},
};

struct regbase *get_mt6853_all_reg_bases(void)
{
	return rb;
}

struct regname *get_mt6853_all_reg_names(void)
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
	print_subsys_reg(infracfg_ao);
	print_subsys_reg(infracfg);
	print_subsys_reg(infracfg_dbg);
	print_subsys_reg(infrapdn_dbg);
	print_subsys_reg(apmixed);
}

static const char * const compatible[] = {"mediatek,mt6853", "mediatek,mt6877", NULL};

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

static int __init clkchk_platform_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++)
		clkchk_swcg_init(mtk_subsys_check[i].swcgs);

	return clkchk_init(&cfg);
}
subsys_initcall(clkchk_platform_init);

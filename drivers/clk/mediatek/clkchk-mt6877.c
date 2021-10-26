/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/syscore_ops.h>
#include <linux/version.h>

#include <mt-plat/aee.h>
#include "clk-mt6877-pg.h"
#include "clkchk.h"
#include "clkchk-mt6877.h"

#define TAG			"[clkchk] "
#define	BUG_ON_CHK_ENABLE	1

int __attribute__((weak)) get_sw_req_vcore_opp(void)
{
	return -1;
}

static const char * const clks[] = {
	/* topckgen */
	"axi_sel",
	"spm_sel",
	"scp_sel",
	"bus_aximem_sel",
	"disp0_sel",
	"mdp0_sel",
	"img1_sel",
	"ipe_sel",
	"dpe_sel",
	"cam_sel",
	"ccu_sel",
	"dsp_sel",
	"dsp1_sel",
	"dsp2_sel",
	"dsp4_sel",
	"dsp7_sel",
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
	"camtm_sel",
	"pwm_sel",
	"audio_h_sel",
	"mcupm_sel",
	"spmi_m_mst_sel",
	"dvfsrc_sel",
	"mem_sub_sel",
	"aes_msdcfde_sel",
	"ufs_mbist_sel",
	"mfg_internal2_sel",
	"mfg_internal1_sel",
	"ap2conn_host_sel",
	"msdc_new_rx_sel",
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
	"ifrao_pmic_md",
	"ifrao_pmic_conn",
	"ifrao_apxgpt",
	"ifrao_gce",
	"ifrao_gce2",
	"ifrao_therm",
	"ifrao_i2c_pseudo",
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
	"ifrao_ufs_bclk",
	"ifrao_apdma",
	"ifrao_spi4",
	"ifrao_spi5",
	"ifrao_cq_dma",
	"ifrao_ufs",
	"ifrao_aes_ufsfde",
	"ifrao_ssusb_xhci",
	"ifrao_ap_msdc0",
	"ifrao_md_msdc0",
	"ifrao_ccif5_md",
	"ifrao_ccif2_ap",
	"ifrao_ccif2_md",
	"ifrao_fbist2fpc",
	"ifrao_dpmaif_main",
	"ifrao_ccif4_md",
	"ifrao_spi6_ck",
	"ifrao_spi7_ck",
	"ifrao_aes_0p_ck",

	/* apmixedsys */
	"armpll_ll",
	"armpll_bl",
	"ccipll",
	"mainpll",
	"univpll",
	"msdcpll",
	"mmpll",
	"adsppll",
	"tvdpll",
	"apll1",
	"apll2",
	"mpll",
	"usbpll",

	/* scp_par_top */
	"scp_par_audiodsp",

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

	/* msdc0 */
	"msdc0_msdc_rx",

	/* imp_iic_wrap_c */
	"impc_ap_clock_i2c10",
	"impc_ap_clock_i2c11",

	/* imp_iic_wrap_e */
	"impe_ap_clock_i2c3",

	/* imp_iic_wrap_s */
	"imps_ap_clock_i2c5",
	"imps_ap_clock_i2c7",
	"imps_ap_clock_i2c8",
	"imps_ap_clock_i2c9",

	/* imp_iic_wrap_ws */
	"impws_ap_clock_i2c1",
	"impws_ap_clock_i2c2",
	"impws_ap_clock_i2c4",

	/* imp_iic_wrap_w */
	"impw_ap_clock_i2c0",

	/* imp_iic_wrap_n */
	"impn_ap_clock_i2c6",

	/* gpu_pll_ctrl */
	"mfg_ao_mfgpll1",
	"mfg_ao_mfgpll4",

	/* mfgcfg */
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
	"mm_disp_gamma0",
	"mm_disp_postmask0",
	"mm_disp_spr0",
	"mm_disp_dither0",
	"mm_smi_common",
	"mm_disp_cm0",
	"mm_dsi0",
	"mm_smi_gals",
	"mm_disp_dsc_wrap",
	"mm_smi_iommu",
	"mm_disp_ovl1_2l",
	"mm_disp_ufbc_wdma0",
	"mm_dsi0_dsi_domain",
	"mm_disp_26m_ck",

	/* imgsys1 */
	"imgsys1_larb9",
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
	"vde2_vdec_cken",

	/* venc_gcon */
	"ven1_cke0_larb",
	"ven1_cke1_venc",
	"ven1_cke2_jpgenc",
	"ven1_cke5_gals",

	/* apu_conn2 */
	"apu_conn2_ahb",
	"apu_conn2_axi",
	"apu_conn2_isp",
	"apu_conn2_cam_adl",
	"apu_conn2_img_adl",
	"apu_conn2_emi_26m",
	"apu_conn2_vpu_udi",
	"apu_conn2_edma_0",
	"apu_conn2_edma_1",
	"apu_conn2_edmal_0",
	"apu_conn2_edmal_1",
	"apu_conn2_mnoc",
	"apu_conn2_tcm",
	"apu_conn2_md32",
	"apu_conn2_iommu_0",
	"apu_conn2_iommu_1",
	"apu_conn2_md32_32k",
	"apu_conn2_cpe",

	/* apu_conn1 */
	"apu_conn1_axi",
	"apu_conn1_edma_0",
	"apu_conn1_edma_1",
	"apu_conn1_iommu_0",
	"apu_conn1_iommu_1",

	/* apusys_vcore */
	"apuv_ahb",
	"apuv_axi",
	"apuv_adl",
	"apuv_qos",

	/* apu0 */
	"apu0_apu",
	"apu0_axi_m",
	"apu0_jtag",

	/* apu1 */
	"apu1_apu",
	"apu1_axi_m",
	"apu1_jtag",

	/* apu_mdla0 */
	"apum0_mdla_cg0",
	"apum0_mdla_cg1",
	"apum0_mdla_cg2",
	"apum0_mdla_cg3",
	"apum0_mdla_cg4",
	"apum0_mdla_cg5",
	"apum0_mdla_cg6",
	"apum0_mdla_cg7",
	"apum0_mdla_cg8",
	"apum0_mdla_cg9",
	"apum0_mdla_cg10",
	"apum0_mdla_cg11",
	"apum0_mdla_cg12",
	"apum0_apb",
	"apum0_axi_m",

	/* camsys_main */
	"cam_m_larb13",
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
	"mdp_color0",
	"mdp_aal0",
	"mdp_aal1",
	"mdp_rsz1",
	"mdp_img_dl_rel0_as0",
	"mdp_img_dl_rel1_as1",

	/* SCPSYS */
	"PG_MFG0",
	"PG_MFG1",
	"PG_MFG2",
	"PG_MFG3",
	"PG_MFG4",
	"PG_MFG5",
	"PG_MD",
	"PG_CONN",
	"PG_ISP0",
	"PG_ISP1",
	"PG_IPE",
	"PG_VDEC",
	"PG_VENC",
	"PG_DISP",
	"PG_AUDIO",
	"PG_ADSP_DORMANT",
	"PG_CAM",
	"PG_CAM_RAWA",
	"PG_CAM_RAWB",
	"PG_CSI",
	"PG_APU",

	/* end */
	NULL
};

const char * const *get_mt6877_all_clk_names(void)
{
	return clks;
}

static const char * const off_pll_names[] = {
	"univpll",
	"msdcpll",
	"mmpll",
	"tvdpll",
	"usbpll",
	"mfg_ao_mfgpll1",
	"mfg_ao_mfgpll4",
	NULL
};

static const char * const notice_pll_names[] = {
	"adsppll",
	"apll1",
	"apll2",
	NULL
};

static const char * const off_mtcmos_names[] = {
	"PG_MFG0",
	"PG_MFG1",
	"PG_MFG2",
	"PG_MFG3",
	"PG_MFG4",
	"PG_MFG5",
	"PG_ISP0",
	"PG_ISP1",
	"PG_IPE",
	"PG_VDEC",
	"PG_VENC",
	"PG_DISP",
	"PG_CAM",
	"PG_CAM_RAWA",
	"PG_CAM_RAWB",
	"PG_CSI",
	"PG_APU",
	NULL
};

static const char * const notice_mtcmos_names[] = {
	"PG_MD",
	"PG_CONN",
	"PG_AUDIO",
	"PG_ADSP_DORMANT",
	NULL
};

/*
 * clkchk vf table
 */

struct mtk_vf {
	const char *name;
	int freq_table[5];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3, _freq4) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3, _freq4},	\
	}

/*
 * Opp0 : 0p75v
 * Opp1 : 0p725v
 * Opp2 : 0p65v
 * Opp3 : 0p60v
 * Opp4 : 0p55v
 */
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3, Opp4 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("spm_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("scp_sel", 624000, 624000, 416000, 364000, 312000),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("disp0_sel", 546000, 546000, 416000, 312000, 208000),
	MTK_VF_TABLE("mdp0_sel", 594000, 594000, 436800, 343750, 275000),
	MTK_VF_TABLE("img1_sel", 624000, 624000, 458333, 343750, 275000),
	MTK_VF_TABLE("ipe_sel", 546000, 546000, 416000, 312000, 275000),
	MTK_VF_TABLE("dpe_sel", 546000, 546000, 458333, 364000, 249600),
	MTK_VF_TABLE("cam_sel", 624000, 624000, 546000, 392857, 297000),
	MTK_VF_TABLE("ccu_sel", 499200, 499200, 392857, 364000, 275000),
	MTK_VF_TABLE("dsp_sel", 687500, 687500, 687500, 687500, 687500),
	MTK_VF_TABLE("dsp1_sel", 624000, 624000, 624000, 624000, 624000),
	MTK_VF_TABLE("dsp2_sel", 624000, 624000, 624000, 624000, 624000),
	MTK_VF_TABLE("dsp4_sel", 687500, 687500, 687500, 687500, 687500),
	MTK_VF_TABLE("dsp7_sel", 687500, 687500, 687500, 687500, 687500),
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg5_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("uart_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("spi_sel", 109200, 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("msdc50_0_hclk_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("msdc50_0_sel", 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("msdc30_1_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("pwrap_ulposc_sel", 65000, 65000, 65000, 65000, 65000),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("sspm_sel", 312000, 312000, 273000, 242667, 218400),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_top_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("seninf3_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("dxcc_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("aud_engen1_sel", 45158, 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("aud_engen2_sel", 49152, 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("aes_ufsfde_sel", 546000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("ufs_sel", 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("aud_1_sel", 180634, 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("adsp_sel", 750000, 750000, 750000, 750000, 750000),
	MTK_VF_TABLE("dpmaif_main_sel", 364000, 364000, 364000, 364000, 273000),
	MTK_VF_TABLE("venc_sel", 624000, 624000, 458333, 343750, 249600),
	MTK_VF_TABLE("vdec_sel", 546000, 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("camtm_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("audio_h_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("mcupm_sel", 182000, 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("spmi_m_mst_sel", 39000, 39000, 39000, 39000, 39000),
	MTK_VF_TABLE("dvfsrc_sel", 26000, 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("mem_sub_sel", 436800, 436800, 364000, 273000, 182000),
	MTK_VF_TABLE("aes_msdcfde_sel", 416000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("ufs_mbist_sel", 297000, 297000, 297000, 297000, 297000),
	MTK_VF_TABLE("ap2conn_host_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("msdc_new_rx_sel", 384000, 384000, 384000, 384000, 384000),
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
					 * chk_id is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

/*
 * The clk names in Mediatek CCF.
 */
/* audio */
struct pg_check_swcg audio_swcgs[] = {
	SWCG("aud_afe"),
	SWCG("aud_22m"),
	SWCG("aud_24m"),
	SWCG("aud_apll2_tuner"),
	SWCG("aud_apll_tuner"),
	SWCG("aud_tdm_ck"),
	SWCG("aud_adc"),
	SWCG("aud_dac"),
	SWCG("aud_dac_predis"),
	SWCG("aud_tml"),
	SWCG("aud_nle"),
	SWCG("aud_connsys_i2s_asrc"),
	SWCG("aud_general1_asrc"),
	SWCG("aud_general2_asrc"),
	SWCG("aud_dac_hires"),
	SWCG("aud_adc_hires"),
	SWCG("aud_adc_hires_tml"),
	SWCG("aud_adda6_adc"),
	SWCG("aud_adda6_adc_hires"),
	SWCG("aud_3rd_dac"),
	SWCG("aud_3rd_dac_predis"),
	SWCG("aud_3rd_dac_tml"),
	SWCG("aud_3rd_dac_hires"),
	SWCG(NULL),
};
/* camsys_main */
struct pg_check_swcg camsys_main_swcgs[] = {
	SWCG("cam_m_larb13"),
	SWCG("cam_m_larb14"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv0"),
	SWCG("cam_m_camsv1"),
	SWCG("cam_m_camsv2"),
	SWCG("cam_m_camsv3"),
	SWCG("cam_m_ccu0"),
	SWCG("cam_m_ccu1"),
	SWCG("cam_m_mraw0"),
	SWCG("cam_m_ccu_gals"),
	SWCG("cam_m_cam2mm_gals"),
	SWCG("cam_m_camsv4"),
	SWCG("cam_m_pda"),
	SWCG(NULL),
};
/* camsys_rawa */
struct pg_check_swcg camsys_rawa_swcgs[] = {
	SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG(NULL),
};
/* camsys_rawb */
struct pg_check_swcg camsys_rawb_swcgs[] = {
	SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG(NULL),
};
/* imgsys1 */
struct pg_check_swcg imgsys1_swcgs[] = {
	SWCG("imgsys1_larb9"),
	SWCG("imgsys1_dip"),
	SWCG("imgsys1_gals"),
	SWCG(NULL),
};
/* imgsys2 */
struct pg_check_swcg imgsys2_swcgs[] = {
	SWCG("imgsys2_larb9"),
	SWCG("imgsys2_larb10"),
	SWCG("imgsys2_mfb"),
	SWCG("imgsys2_wpe"),
	SWCG("imgsys2_mss"),
	SWCG("imgsys2_gals"),
	SWCG(NULL),
};
/* ipesys */
struct pg_check_swcg ipesys_swcgs[] = {
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
/* mdpsys_config */
struct pg_check_swcg mdpsys_config_swcgs[] = {
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
	SWCG("mdp_color0"),
	SWCG("mdp_aal0"),
	SWCG("mdp_aal1"),
	SWCG("mdp_rsz1"),
	SWCG("mdp_img_dl_rel0_as0"),
	SWCG("mdp_img_dl_rel1_as1"),
	SWCG(NULL),
};
/* mfgcfg */
struct pg_check_swcg mfgcfg_swcgs[] = {
	SWCG("mfgcfg_bg3d"),
	SWCG(NULL),
};
/* mmsys_config */
struct pg_check_swcg mmsys_config_swcgs[] = {
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
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_spr0"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_smi_common"),
	SWCG("mm_disp_cm0"),
	SWCG("mm_dsi0"),
	SWCG("mm_smi_gals"),
	SWCG("mm_disp_dsc_wrap"),
	SWCG("mm_smi_iommu"),
	SWCG("mm_disp_ovl1_2l"),
	SWCG("mm_disp_ufbc_wdma0"),
	SWCG("mm_dsi0_dsi_domain"),
	SWCG("mm_disp_26m_ck"),
	SWCG(NULL),
};
/* vdec_gcon */
struct pg_check_swcg vdec_gcon_swcgs[] = {
	SWCG("vde2_vdec_cken"),
	SWCG(NULL),
};
/* venc_gcon */
struct pg_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_cke0_larb"),
	SWCG("ven1_cke1_venc"),
	SWCG("ven1_cke2_jpgenc"),
	SWCG("ven1_cke5_gals"),
	SWCG(NULL),
};

struct subsys_cgs_check mtk_subsys_check[] = {
	{SYS_AUDIO, audio_swcgs, audsys},
	{SYS_CAM, camsys_main_swcgs, cam_m},
	{SYS_CAM_RAWA, camsys_rawa_swcgs, cam_ra},
	{SYS_CAM_RAWB, camsys_rawb_swcgs, cam_rb},
	{SYS_ISP0, imgsys1_swcgs, imgsys1},
	{SYS_ISP1, imgsys2_swcgs, imgsys2},
	{SYS_IPE, ipesys_swcgs, ipe},
	{SYS_DISP, mdpsys_config_swcgs, mdp},
	{SYS_MFG0, mfgcfg_swcgs, mfgcfg},
	{SYS_DISP, mmsys_config_swcgs, mm},
	{SYS_VDEC, vdec_gcon_swcgs, vde2},
	{SYS_VENC, venc_gcon_swcgs, ven1},
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
			print_subsys_reg(spm);
			print_subsys_reg(mtk_subsys_check[i].chk_id);
		}
	}

	if (ret) {
		pr_err("%s(%d): %d\n", __func__, id, ret);
#if BUG_ON_CHK_ENABLE
		BUG_ON(1);
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
	[top] = REGBASE_V(0x10000000, top, NULL),
	[ifrao] = REGBASE_V(0x10001000, ifrao, NULL),
	[infracfg_ao_bus] = REGBASE_V(0x10001000, infracfg_ao_bus, NULL),
	[spm] = REGBASE_V(0x10006000, spm, NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, NULL),
	[scp_par] = REGBASE_V(0x10720000, scp_par, NULL),
	[audsys] = REGBASE_V(0x11210000, audsys, "PG_AUDIO"),
	[msdc0] = REGBASE_V(0x11230000, msdc0, NULL),
	[impc] = REGBASE_V(0x11282000, impc, "fi2c_pseudo_ck"),
	[impe] = REGBASE_V(0x11cb1000, impe, "fi2c_pseudo_ck"),
	[imps] = REGBASE_V(0x11d04000, imps, "fi2c_pseudo_ck"),
	[impws] = REGBASE_V(0x11d23000, impws, "fi2c_pseudo_ck"),
	[impw] = REGBASE_V(0x11e01000, impw, "fi2c_pseudo_ck"),
	[impn] = REGBASE_V(0x11f01000, impn, "fi2c_pseudo_ck"),
	[mfg_ao] = REGBASE_V(0x13fa0000, mfg_ao, "PG_MFG5"),
	[mfgcfg] = REGBASE_V(0x13fbf000, mfgcfg, "PG_MFG5"),
	[mm] = REGBASE_V(0x14000000, mm, "PG_DISP"),
	[imgsys1] = REGBASE_V(0x15020000, imgsys1, "PG_ISP0"),
	[imgsys2] = REGBASE_V(0x15820000, imgsys2, "PG_ISP1"),
	[vde2] = REGBASE_V(0x1602f000, vde2, "PG_VDEC"),
	[ven1] = REGBASE_V(0x17000000, ven1, "PG_VENC"),
	[apu_conn2] = REGBASE_V(0x19020000, apu_conn2, "PG_APU"),
	[apu_conn1] = REGBASE_V(0x19024000, apu_conn1, "PG_APU"),
	[apuv] = REGBASE_V(0x19029000, apuv, "PG_APU"),
	[apu0] = REGBASE_V(0x19030000, apu0, "PG_APU"),
	[apu1] = REGBASE_V(0x19031000, apu1, "PG_APU"),
	[apum0] = REGBASE_V(0x19034000, apum0, "PG_APU"),
	[apu_ao] = REGBASE_V(0x190f3000, apu_ao, NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, "PG_CAM"),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, "PG_CAM_RAWA"),
	[cam_rb] = REGBASE_V(0x1a06f000, cam_rb, "PG_CAM_RAWB"),
	[ipe] = REGBASE_V(0x1b000000, ipe, "PG_IPE"),
	[mdp] = REGBASE_V(0x1f000000, mdp, "PG_DISP"),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* TOPCKGEN register */
	REGNAME(top,  0x0010, CLK_CFG_0),
	REGNAME(top,  0x0020, CLK_CFG_1),
	REGNAME(top,  0x0030, CLK_CFG_2),
	REGNAME(top,  0x0040, CLK_CFG_3),
	REGNAME(top,  0x0050, CLK_CFG_4),
	REGNAME(top,  0x0060, CLK_CFG_5),
	REGNAME(top,  0x0070, CLK_CFG_6),
	REGNAME(top,  0x0080, CLK_CFG_7),
	REGNAME(top,  0x0090, CLK_CFG_8),
	REGNAME(top,  0x00A0, CLK_CFG_9),
	REGNAME(top,  0x00B0, CLK_CFG_10),
	REGNAME(top,  0x00C0, CLK_CFG_11),
	REGNAME(top,  0x00D0, CLK_CFG_12),
	REGNAME(top,  0x00E0, CLK_CFG_13),
	REGNAME(top,  0x00F0, CLK_CFG_14),
	REGNAME(top,  0x0100, CLK_CFG_15),
	REGNAME(top,  0x0110, CLK_CFG_16),
	REGNAME(top,  0x0120, CLK_CFG_20),
	REGNAME(top,  0x0180, CLK_CFG_17),
	REGNAME(top,  0x0320, CLK_AUDDIV_0),
	REGNAME(top,  0x0328, CLK_AUDDIV_2),
	REGNAME(top,  0x0334, CLK_AUDDIV_3),
	REGNAME(top,  0x0338, CLK_AUDDIV_4),
	/* INFRACFG_AO register */
	REGNAME(ifrao,  0x90, MODULE_SW_CG_0),
	REGNAME(ifrao,  0x94, MODULE_SW_CG_1),
	REGNAME(ifrao,  0xac, MODULE_SW_CG_2),
	REGNAME(ifrao,  0xc8, MODULE_SW_CG_3),
	REGNAME(ifrao,  0xe8, MODULE_SW_CG_4),
	/* INFRACFG_AO_BUS register */
	REGNAME(infracfg_ao_bus,  0x0710, INFRA_TOPAXI_PROTECTEN_2),
	REGNAME(infracfg_ao_bus,  0x0720, INFRA_TOPAXI_PROTECTEN_STA0_2),
	REGNAME(infracfg_ao_bus,  0x0724, INFRA_TOPAXI_PROTECTEN_STA1_2),
	REGNAME(infracfg_ao_bus,  0x0220, INFRA_TOPAXI_PROTECTEN),
	REGNAME(infracfg_ao_bus,  0x0224, INFRA_TOPAXI_PROTECTEN_STA0),
	REGNAME(infracfg_ao_bus,  0x0228, INFRA_TOPAXI_PROTECTEN_STA1),
	REGNAME(infracfg_ao_bus,  0x0B80, INFRA_TOPAXI_PROTECTEN_VDNR),
	REGNAME(infracfg_ao_bus,  0x0B8C, INFRA_TOPAXI_PROTECTEN_VDNR_STA0),
	REGNAME(infracfg_ao_bus,  0x0B90, INFRA_TOPAXI_PROTECTEN_VDNR_STA1),
	REGNAME(infracfg_ao_bus,  0x0250, INFRA_TOPAXI_PROTECTEN_1),
	REGNAME(infracfg_ao_bus,  0x0254, INFRA_TOPAXI_PROTECTEN_STA0_1),
	REGNAME(infracfg_ao_bus,  0x0258, INFRA_TOPAXI_PROTECTEN_STA1_1),
	REGNAME(infracfg_ao_bus,  0x02D0, INFRA_TOPAXI_PROTECTEN_MM),
	REGNAME(infracfg_ao_bus,  0x02E8, INFRA_TOPAXI_PROTECTEN_MM_STA0),
	REGNAME(infracfg_ao_bus,  0x02EC, INFRA_TOPAXI_PROTECTEN_MM_STA1),
	/* SPM register */
	REGNAME(spm,  0xE80, MFG0_PWR_CON),
	REGNAME(spm,  0xEF8, XPU_PWR_STATUS),
	REGNAME(spm,  0xEFC, XPU_PWR_STATUS_2ND),
	REGNAME(spm,  0xE84, MFG1_PWR_CON),
	REGNAME(spm,  0xE88, MFG2_PWR_CON),
	REGNAME(spm,  0xE8C, MFG3_PWR_CON),
	REGNAME(spm,  0xE90, MFG4_PWR_CON),
	REGNAME(spm,  0xE94, MFG5_PWR_CON),
	REGNAME(spm,  0xE00, MD1_PWR_CON),
	REGNAME(spm,  0xEF0, PWR_STATUS),
	REGNAME(spm,  0xEF4, PWR_STATUS_2ND),
	REGNAME(spm,  0xEE8, MD_BUCK_ISO_CON),
	REGNAME(spm,  0xE04, CONN_PWR_CON),
	REGNAME(spm,  0xE24, ISP0_PWR_CON),
	REGNAME(spm,  0xE28, ISP1_PWR_CON),
	REGNAME(spm,  0xE2C, IPE_PWR_CON),
	REGNAME(spm,  0xE30, VDE0_PWR_CON),
	REGNAME(spm,  0xE38, VEN_PWR_CON),
	REGNAME(spm,  0xE48, DIS0_PWR_CON),
	REGNAME(spm,  0xE54, AUDIO_PWR_CON),
	REGNAME(spm,  0xE58, ADSP_PWR_CON),
	REGNAME(spm,  0xEEC, SOC_BUCK_ISO_CON),
	REGNAME(spm,  0xE5C, CAM_PWR_CON),
	REGNAME(spm,  0xE60, CAM_RAWA_PWR_CON),
	REGNAME(spm,  0xE64, CAM_RAWB_PWR_CON),
	REGNAME(spm,  0xE78, CSI_PWR_CON),
	REGNAME(spm,  0x670, SPM_CROSS_WAKE_M01_REQ),
	REGNAME(spm,  0x178, OTHER_PWR_STATUS),
	/* APMIXEDSYS register */
	REGNAME(apmixed,  0x208, ARMPLL_LL_CON0),
	REGNAME(apmixed,  0x20c, ARMPLL_LL_CON1),
	REGNAME(apmixed,  0x210, ARMPLL_LL_CON2),
	REGNAME(apmixed,  0x214, ARMPLL_LL_CON3),
	REGNAME(apmixed,  0x218, ARMPLL_BL_CON0),
	REGNAME(apmixed,  0x21c, ARMPLL_BL_CON1),
	REGNAME(apmixed,  0x220, ARMPLL_BL_CON2),
	REGNAME(apmixed,  0x224, ARMPLL_BL_CON3),
	REGNAME(apmixed,  0x238, CCIPLL_CON0),
	REGNAME(apmixed,  0x23c, CCIPLL_CON1),
	REGNAME(apmixed,  0x240, CCIPLL_CON2),
	REGNAME(apmixed,  0x244, CCIPLL_CON3),
	REGNAME(apmixed,  0x350, MAINPLL_CON0),
	REGNAME(apmixed,  0x354, MAINPLL_CON1),
	REGNAME(apmixed,  0x358, MAINPLL_CON2),
	REGNAME(apmixed,  0x35c, MAINPLL_CON3),
	REGNAME(apmixed,  0x308, UNIVPLL_CON0),
	REGNAME(apmixed,  0x30c, UNIVPLL_CON1),
	REGNAME(apmixed,  0x310, UNIVPLL_CON2),
	REGNAME(apmixed,  0x314, UNIVPLL_CON3),
	REGNAME(apmixed,  0x360, MSDCPLL_CON0),
	REGNAME(apmixed,  0x364, MSDCPLL_CON1),
	REGNAME(apmixed,  0x368, MSDCPLL_CON2),
	REGNAME(apmixed,  0x36c, MSDCPLL_CON3),
	REGNAME(apmixed,  0x3a0, MMPLL_CON0),
	REGNAME(apmixed,  0x3a4, MMPLL_CON1),
	REGNAME(apmixed,  0x3a8, MMPLL_CON2),
	REGNAME(apmixed,  0x3ac, MMPLL_CON3),
	REGNAME(apmixed,  0x380, ADSPPLL_CON0),
	REGNAME(apmixed,  0x384, ADSPPLL_CON1),
	REGNAME(apmixed,  0x388, ADSPPLL_CON2),
	REGNAME(apmixed,  0x38c, ADSPPLL_CON3),
	REGNAME(apmixed,  0x248, TVDPLL_CON0),
	REGNAME(apmixed,  0x24c, TVDPLL_CON1),
	REGNAME(apmixed,  0x250, TVDPLL_CON2),
	REGNAME(apmixed,  0x254, TVDPLL_CON3),
	REGNAME(apmixed,  0x328, APLL1_CON0),
	REGNAME(apmixed,  0x32c, APLL1_CON1),
	REGNAME(apmixed,  0x330, APLL1_CON2),
	REGNAME(apmixed,  0x334, APLL1_CON3),
	REGNAME(apmixed,  0x338, APLL1_CON4),
	REGNAME(apmixed,  0x33c, APLL2_CON0),
	REGNAME(apmixed,  0x340, APLL2_CON1),
	REGNAME(apmixed,  0x344, APLL2_CON2),
	REGNAME(apmixed,  0x348, APLL2_CON3),
	REGNAME(apmixed,  0x34c, APLL2_CON4),
	REGNAME(apmixed,  0x390, MPLL_CON0),
	REGNAME(apmixed,  0x394, MPLL_CON1),
	REGNAME(apmixed,  0x398, MPLL_CON2),
	REGNAME(apmixed,  0x39c, MPLL_CON3),
	REGNAME(apmixed,  0x318, USBPLL_CON0),
	REGNAME(apmixed,  0x31c, USBPLL_CON1),
	REGNAME(apmixed,  0x320, USBPLL_CON2),
	REGNAME(apmixed,  0x324, USBPLL_CON3),
	/* SCP_PAR_TOP register */
	REGNAME(scp_par,  0x180, AUDIODSP_CK_CG),
	/* AUDIO register */
	REGNAME(audsys,  0x0, AUDIO_TOP_0),
	REGNAME(audsys,  0x4, AUDIO_TOP_1),
	/* MSDC0 register */
	REGNAME(msdc0,  0x68, MSDC_NEW_RX_CFG),
	/* IMP_IIC_WRAP_C register */
	REGNAME(impc,  0xe00, AP_CLOCK_CG_CEN),
	/* IMP_IIC_WRAP_E register */
	REGNAME(impe,  0xe00, AP_CLOCK_CG_EST),
	/* IMP_IIC_WRAP_S register */
	REGNAME(imps,  0xe00, AP_CLOCK_CG_SOU),
	/* IMP_IIC_WRAP_WS register */
	REGNAME(impws,  0xe00, AP_CLOCK_CG_WEST_SOU),
	/* IMP_IIC_WRAP_W register */
	REGNAME(impw,  0xe00, AP_CLOCK_CG_WST),
	/* IMP_IIC_WRAP_N register */
	REGNAME(impn,  0xe00, AP_CLOCK_CG_NOR),
	/* GPU_PLL_CTRL register */
	REGNAME(mfg_ao,  0x8, MFGPLL1_CON0),
	REGNAME(mfg_ao,  0xc, MFGPLL1_CON1),
	REGNAME(mfg_ao,  0x10, MFGPLL1_CON2),
	REGNAME(mfg_ao,  0x14, MFGPLL1_CON3),
	REGNAME(mfg_ao,  0x38, MFGPLL4_CON0),
	REGNAME(mfg_ao,  0x3c, MFGPLL4_CON1),
	REGNAME(mfg_ao,  0x40, MFGPLL4_CON2),
	REGNAME(mfg_ao,  0x44, MFGPLL4_CON3),
	/* MFGCFG register */
	REGNAME(mfgcfg,  0x0, MFG_CG),
	/* MMSYS_CONFIG register */
	REGNAME(mm,  0x100, MMSYS_CG_0),
	REGNAME(mm,  0x1a0, MMSYS_CG_2),
	/* IMGSYS1 register */
	REGNAME(imgsys1,  0x0, IMG_CG),
	/* IMGSYS2 register */
	REGNAME(imgsys2,  0x0, IMG_CG),
	/* VDEC_GCON register */
	REGNAME(vde2,  0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1,  0x0, VENCSYS_CG),
	/* APU_CONN2 register */
	REGNAME(apu_conn2,  0x0, APU_CONN_CG),
	/* APU_CONN1 register */
	REGNAME(apu_conn1,  0x0, APU_CONN1_CG),
	/* APUSYS_VCORE register */
	REGNAME(apuv,  0x0, APUSYS_VCORE_CG),
	/* APU0 register */
	REGNAME(apu0,  0x100, CORE_CG),
	/* APU1 register */
	REGNAME(apu1,  0x100, CORE_CG),
	/* APU_MDLA0 register */
	REGNAME(apum0,  0x0, MDLA_CG),
	/* APU_PLL_CTRL register */
	REGNAME(apu_ao,  0x8, APUPLL_CON0),
	REGNAME(apu_ao,  0xc, APUPLL_CON1),
	REGNAME(apu_ao,  0x10, APUPLL_CON2),
	REGNAME(apu_ao,  0x14, APUPLL_CON3),
	REGNAME(apu_ao,  0x18, NPUPLL_CON0),
	REGNAME(apu_ao,  0x1c, NPUPLL_CON1),
	REGNAME(apu_ao,  0x20, NPUPLL_CON2),
	REGNAME(apu_ao,  0x24, NPUPLL_CON3),
	REGNAME(apu_ao,  0x28, APUPLL1_CON0),
	REGNAME(apu_ao,  0x2c, APUPLL1_CON1),
	REGNAME(apu_ao,  0x30, APUPLL1_CON2),
	REGNAME(apu_ao,  0x34, APUPLL1_CON3),
	REGNAME(apu_ao,  0x38, APUPLL2_CON0),
	REGNAME(apu_ao,  0x3c, APUPLL2_CON1),
	REGNAME(apu_ao,  0x40, APUPLL2_CON2),
	REGNAME(apu_ao,  0x44, APUPLL2_CON3),
	/* CAMSYS_MAIN register */
	REGNAME(cam_m,  0x0, CAMSYS_CG),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra,  0x0, CAMSYS_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb,  0x0, CAMSYS_CG),
	/* IPESYS register */
	REGNAME(ipe,  0x0, IMG_CG),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp,  0x100, MDPSYS_CG_0),
	REGNAME(mdp,  0x120, MDPSYS_CG_2),
	{},
};

struct regbase *get_mt6877_all_reg_bases(void)
{
	return rb;
}

struct regname *get_mt6877_all_reg_names(void)
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
	print_subsys_reg(spm);
	print_subsys_reg(top);
	print_subsys_reg(ifrao);
	print_subsys_reg(infracfg_ao_bus);
	print_subsys_reg(apmixed);
}

static void __init init_regbase(void)
{
	struct regbase *rb = get_mt6877_all_reg_bases();

	for (; rb->name; rb++) {
		if (!rb->phys)
			continue;

		rb->virt = ioremap_nocache(rb->phys, 0x1000);
	}
}

static const char * const compatible[] = {"mediatek,mt6877", NULL};

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

	init_regbase();

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++)
		clkchk_swcg_init(mtk_subsys_check[i].swcgs);

	return clkchk_init(&cfg);
}
subsys_initcall(clkchk_platform_init);

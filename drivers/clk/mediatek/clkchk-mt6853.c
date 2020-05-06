/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "clk-mt6853-pg.h"
#include "clkdbg-mt6853.h"

#define TAG			"[clkchk] "
#define	BUG_ON_CHK_ENABLE	1

const char * const *get_mt6853_all_clk_names(void)
{
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
	const char * const *cn = get_mt6853_all_clk_names();
	const char *fix_clk = "clk26m";

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
	char buf[128] = {0};
	int n = 0;

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
		aee_kernel_warning("CCF MT6853",
			"@%s():%d, PLLs are not off\n", __func__, __LINE__);
		WARN_ON(1);
#endif
#else
		aee_kernel_warning("CCF MT6853",
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
	char buf[128] = {0};
	int n = 0;

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
	char buf[128] = {0};
	int n = 0;

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
		aee_kernel_warning("CCF MT6853",
			"@%s():%d, MTCMOSs are not off\n", __func__, __LINE__);
		WARN_ON(1);
#endif
#else
		aee_kernel_warning("CCF MT6853",
			"@%s():%d, MTCMOSs are not off\n", __func__, __LINE__);
		WARN_ON(1);
#endif
	}
}

static void check_mtcmos_notice(void)
{
	static struct clk *notice_mtcmos[ARRAY_SIZE(notice_mtcmos_names)];

	struct clk **c;
	int invalid = 0;
	char buf[128] = {0};
	int n = 0;

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

	if (!of_machine_is_compatible("mediatek,MT6853"))
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++)
		pg_check_swcg_init_common(mtk_subsys_check[i].swcgs);

	return 0;
}
subsys_initcall(clkchk_init);

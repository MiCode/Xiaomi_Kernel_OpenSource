/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Wendell Lin <wendell.lin@mediatek.com>
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

#define WARN_ON_CHECK_PLL_FAIL		0
#define CLKDBG_CCF_API_4_4	1
#include "clk-mt6781-pg.h"

#define TAG	"[clkchk] "

#if !CLKDBG_CCF_API_4_4

/* backward compatible */

static const char *clk_hw_get_name(const struct clk_hw *hw)
{
	return __clk_get_name(hw->clk);
}

static bool clk_hw_is_prepared(const struct clk_hw *hw)
{
	return __clk_is_prepared(hw->clk);
}

static bool clk_hw_is_enabled(const struct clk_hw *hw)
{
	return __clk_is_enabled(hw->clk);
}

#endif /* !CLKDBG_CCF_API_4_4 */

static const char * const *get_all_clk_names(void)
{
	static const char * const clks[] = {
		/* plls */
		"mainpll",
		"univ2pll",
		"mfgpll",
		"msdcpll",
		"adsppll",
		"mmpll",
		"apll1",
		"apll2",
		/* apmixedsys */
		"apmixed_ssusb26m",
		"apmixed_appll26m",
		"apmixed_mipic026m",
		"apmixed_mdpll26m",
		"apmixed_mmsys26m",
		"apmixed_ufs26m",
		"apmixed_mipic126m",
		"apmixed_mempll26m",
		"apmixed_lvpll26m",
		"apmixed_mipid026m",
		"apmixed_mipid126m",
		/* TOP */
		"fpwrap_ulposc_sel",
		"aud_intbus_sel",
		"audio_h_sel",
		"aud_2_sel",
		"aud_eng2_sel",
		"aud_1_sel",
		"aud_eng1_sel",
		"mdp_sel",
		"disp_sel",
		"venc_sel",
		"adsp_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"mfg_sel",
		"i2c_sel",
		"usb_top_sel",
		"aes_fde_sel",
		"aes_msdcfde_sel",
		"seninf3_sel",
		"seninf2_sel",
		"seninf1_sel",
		"seninf_sel",
		"camtg6_sel",
		"camtg5_sel",
		"camtg4_sel",
		"camtg3_sel",
		"camtg2_sel",
		"camtg1_sel",
		"camtg_sel",
		"vdec_sel",
		"ipe_sel",
		"img1_sel",
		"cam_sel",
		"ufs_sel",
		"dxcc_sel",
		"msdc50_0_hclk_sel",
		"dpmaif_sel",
		"spi_sel",
		"axi_sel",
		"spm_sel",
		"spmi_mst_sel",
		"dsi_occ_sel",
		"dvfsrc_sel",
		"camtm_sel",
		"pwm_sel",
		"sspm_sel",
		"disppwm_sel",
		"audio_sel",
		"uart_sel",
		"scp_sel",
		"srck_sel",
		/* INFRACFG */
		"infra_pmic_tmr",
		"infra_pmic_ap",
		"infra_pmic_md",
		"infra_pmic_conn",
		"infra_scp",
		"infra_sej",
		"infra_apxgpt",
		"infra_icusb",
		"infra_gce",
		"infra_therm",
		"infra_i2c_ap",
		"infra_i2c_ccu",
		"infra_i2c_sspm",
		"infra_i2c_rsv",
		"infra_pwm_hclk",
		"infra_pwm1",
		"infra_pwm2",
		"infra_pwm3",
		"infra_pwm4",
		"infra_pwm5",
		"infra_pwm",
		"infra_uart0",
		"infra_uart1",
		"infra_uart2",
		"infra_uart3",
		"infra_gce_26m",
		"infra_cqdma_fpc",
		"infra_btif",
		"infra_spi0",
		"infra_msdc0",
		"infra_msdcfde",
		"infra_msdc1",
		"infra_msdc2",
		"infra_msdc0_sck",
		"infra_dvfsrc",
		"infra_gcpu",
		"infra_trng",
		"infra_auxadc",
		"infra_cpum",
		"infra_ccif1_ap",
		"infra_ccif1_md",
		"infra_auxadc_md",
		"infra_msdc1_sck",
		"infra_msdc2_sck",
		"infra_apdma",
		"infra_xiu",
		"infra_devapc",
		"infra_ccif_ap",
		"infra_debugsys",
		"infra_audio",
		"infra_ccif_md",
		"infra_dxcc_sec_core",
		"infra_dxcc_ao",
		"infra_imp_iic",
		"infra_devmpu_bclk",
		"infra_dramc_f26m",
		"infra_pwm_bclk6",
		"infra_usb",
		"infra_disppwm",
		"infra_cldma_bclk",
		"infra_spi1",
		"infra_i2c4",
		"infra_spi2",
		"infra_spi3",
		"infra_unipro_sck",
		"infra_unipro_tick",
		"infra_md32_bclk",
		"infra_sspm",
		"infra_unipro_mbist",
		"infra_sspm_bus_hclk",
		"infra_i2c5",
		"infra_i2c5_arbiter",
		"infra_i2c5_imm",
		"infra_i2c1_arbiter",
		"infra_i2c1_imm",
		"infra_i2c2_arbiter",
		"infra_i2c2_imm",
		"infra_spi4",
		"infra_spi5",
		"infra_cqdma",
		"infra_bist2fpc",
		"infra_aes_ufs",
		"infra_ufs",
		"infra_ufs_tick",
		"infra_msdc0_self",
		"infra_msdc1_self",
		"infra_msdc2_self",
		"infra_sspm_26m_self",
		"infra_sspm_32k_self",
		"infra_ufs_axi",
		"infra_i2c6",
		"infra_ap_msdc0",
		"infra_md_msdc0",
		"infra_msdc0_srclk",
		"infra_msdc1_srclk",
		"infra_pwrap_tmr_fo",
		"infra_pwrap_spi_fo",
		"infra_pwrap_sys_fo",
		"infra_sej_f13m",
		"infra_aes_top0_bclk",
		"infra_mcupm_bclk",
		"infra_ccif2_ap",
		"infra_ccif2_md",
		"infra_ccif3_ap",
		"infra_ccif3_md",
		"infra_fadsp_26m",
		"infra_fadsp_32k",
		"infra_ccif4_ap",
		"infra_ccif4_md",
		"infra_dpmaif",
		"infra_fadsp",
		/* AUDIO */
		"aud_afe",
		"aud_22m",
		"aud_24m",
		"aud_apll2_tuner",
		"aud_apll_tuner",
		"aud_tdm",
		"aud_adc",
		"aud_dac",
		"aud_dac_predis",
		"aud_tml",
		"aud_nle",
		"aud_i2s1_bclk",
		"aud_i2s2_bclk",
		"aud_i2s3_bclk",
		"aud_i2s4_bclk",
		"aud_i2s5_bclk",
		"aud_conn_i2s",
		"aud_general1",
		"aud_general2",
		"aud_dac_hires",
		"aud_adc_hires",
		"aud_adc_hires_tml",
		"aud_pdn_adda6_adc",
		"aud_adda6_adc_hires",
		"aud_3rd_dac",
		"aud_3rd_dac_predis",
		"aud_3rd_dac_tml",
		"aud_3rd_dac_hires",
		"aud_etdm_out1_bclk",
		"aud_etdm_in1_bclk",
		/* CAM */
		"cam_m_larb13",
		"cam_m_dfpvad",
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
		/* CAM_RAWA */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",
		/* CAM_RAWB */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",
		/* IMG */
		"imgsys1_larb9",
		"imgsys1_larb10",
		"imgsys1_dip",
		"imgsys1_gals",
		/* IMG2 */
		"imgsys2_larb9",
		"imgsys2_larb10",
		"imgsys2_mfb",
		"imgsys2_wpe",
		"imgsys2_mss",
		"imgsys2_gals",
		/* IPE */
		"ipe_larb19",
		"ipe_larb20",
		"ipe_smi_subcom",
		"ipe_fd",
		"ipe_fe",
		"ipe_rsc",
		"ipe_dpe",
		"ipe_gals",
		/* MFG */
		"mfg_cfg_bg3d",
		/* MM0 */
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
		/* MM1 */
		"mm_dsi0_dsi_domain",
		"mm_disp_26m_ck",
		/* MDP0 */
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
		/* MDP1 */
		"mdp_img_dl_rel0_as0",
		"mdp_img_dl_rel1_as1",
		/* VDEC */
		"vdec_cken",
		"vdec_larb1_cken",
		"vdec_lat_cken",
		/* VENC */
		"venc_larb",
		"venc_venc",
		"venc_jpgenc",
		"venc_gals",
		/* SCPSYS */
		"pg_md1",
		"pg_conn",
		"pg_dis",
		"pg_cam",
		"pg_cam_rawa",
		"pg_cam_rawb",
		"pg_isp",
		"pg_isp2",
		"pg_ipe",
		"pg_ven",
		"pg_vde",
		"pg_mfg0",
		"pg_mfg1",
		"pg_mfg2",
		"pg_mfg3",
		"pg_csi",
		/* end */
		NULL
	};

	return clks;
}

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
	const char * const *cn = get_all_clk_names();

	pr_notice("enabled clks:\n");

	for (; *cn; cn++) {
		struct clk *c = __clk_lookup(*cn);
		struct clk_hw *c_hw = __clk_get_hw(c);
		struct clk_hw *p_hw;

		if (IS_ERR_OR_NULL(c) || !c_hw)
			continue;

		p_hw = clk_hw_get_parent(c_hw);

		if (!p_hw)
			continue;

		/*if (!clk_hw_is_prepared(c_hw) && !__clk_get_enable_count(c))*/
		if (!__clk_get_enable_count(c))
			continue;

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
	static const char * const off_pll_names[] = {
		"univ2pll",
		"mfgpll",
		"msdcpll",
		"tvdpll",
		"adsppll",
		"mmpll",
		"apll1",
		"apll2",
		NULL
	};

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

		/*if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))*/
		if (!clk_hw_is_enabled(c_hw))
			continue;

		n += snprintf(buf + n, sizeof(buf) - n, "%s ",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid) {
		pr_notice("unexpected unclosed PLL: %s\n", buf);
		print_enabled_clks();

#if WARN_ON_CHECK_PLL_FAIL
		WARN_ON(1);
#endif
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

	return 0;
}

static void clkchk_syscore_resume(void)
{
}

static struct syscore_ops clkchk_syscore_ops = {
	.suspend = clkchk_syscore_suspend,
	.resume = clkchk_syscore_resume,
};

static int __init clkchk_init(void)
{
	if (!of_machine_is_compatible("mediatek,mt6781"))
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
subsys_initcall(clkchk_init);


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
	char *subsys_name;		/*
					 * subsys_name is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

/*
 * The clk names in Mediatek CCF.
 */
struct pg_check_swcg mm_mdp_swcgs[] = {
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
	SWCG("mm_dsi0_dsi_domain"),
	SWCG("mm_disp_26m_ck"),
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
	SWCG("vdec_cken"),
	SWCG("vdec_larb1_cken"),
	SWCG("vdec_lat_cken"),
	SWCG(NULL),
};
struct pg_check_swcg venc_swcgs[] = {
	SWCG("venc_larb"),
	SWCG("venc_venc"),
	SWCG("venc_jpgenc"),
	SWCG("venc_gals"),
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
	SWCG("cam_m_dfpvad"),
	SWCG("cam_m_larb14"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv1"),
	SWCG("cam_m_camsv2"),
	SWCG("cam_m_camsv3"),
	SWCG("cam_m_ccu0"),
	SWCG("cam_m_ccu1"),
	SWCG("cam_m_mraw0"),
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
	{SYS_DIS, mm_mdp_swcgs, "mmsys"},
	{SYS_VDE, vdec_swcgs, "vdecsys"},
	{SYS_VEN, venc_swcgs, "vencsys"},
	{SYS_ISP, img1_swcgs, "img1sys"},
	{SYS_ISP2, img2_swcgs, "img2sys"},
	{SYS_IPE, ipe_swcgs, "ipesys"},
	{SYS_CAM, cam_swcgs, "camsys"},
	{SYS_CAM_RAWA, cam_rawa_swcgs, "cam_rawa_sys"},
	{SYS_CAM_RAWB, cam_rawb_swcgs, "cam_rawb_sys"},
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
			if (mtk_subsys_check[i].subsys_name)
				print_subsys_reg(
					mtk_subsys_check[i].subsys_name);
		}
		break;
	}

	if (ret) {
		pr_notice("%s(%d): %d\n", __func__, id, ret);
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

/*
 * Init procedure for CG checking before MTCMOS off.
 */
static int __init pg_check_swcg_init_mt6781(void)
{
	/* fill the 'struct clk *' ptr of every CGs*/
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++)
		pg_check_swcg_init_common(mtk_subsys_check[i].swcgs);

	return 0;
}
subsys_initcall(pg_check_swcg_init_mt6781);


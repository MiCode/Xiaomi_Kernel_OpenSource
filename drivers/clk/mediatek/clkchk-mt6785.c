/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/clk-provider.h>
#include <linux/syscore_ops.h>
#include <linux/version.h>

#define WARN_ON_CHECK_PLL_FAIL		0
#define CLKDBG_CCF_API_4_4	1

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
		"tvdpll",
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
		"axi_sel",
		"mm_sel",
		"scp_sel",

		"img_sel",
		"ipe_sel",
		"dpe_sel",
		"cam_sel",

		"ccu_sel",
		"dsp_sel",
		"dsp1_sel",
		"dsp2_sel",

		"dsp3_sel",
		"ipu_if_sel",
		"mfg_sel",
		"f52m_mfg_sel",

		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",

		"uart_sel",
		"spi_sel",
		"msdc50_hclk_sel",
		"msdc50_0_sel",

		"msdc30_1_sel",
		"audio_sel",
		"aud_intbus_sel",
		"fpwrap_ulposc_sel",

		"atb_sel",
		"sspm_sel",
		"dpi0_sel",
		"scam_sel",

		"disppwm_sel",
		"usb_top_sel",
		"ssusb_top_xhci_sel",
		"spm_sel",

		"i2c_sel",
		"seninf_sel",
		"seninf1_sel",
		"seninf2_sel",

		"dxcc_sel",
		"aud_eng1_sel",
		"aud_eng2_sel",
		"faes_ufsfde_sel",

		"fufs_sel",
		"aud_1_sel",
		"aud_2_sel",
		"adsp_sel",

		"dpmaif_parents",
		"venc_sel",
		"vdec_sel",
		"camtm_sel",

		"pwm_sel",
		"audio_h_sel",
		"camtg5_sel",


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
		"infra_i2c0",
		"infra_i2c1",
		"infra_i2c2",
		"infra_i2c3",
		"infra_pwm_hclk",
		"infra_pwm1",
		"infra_pwm2",
		"infra_pwm3",
		"infra_pwm4",
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
		"infra_device_apc",
		"infra_ccif_ap",
		"infra_debugsys",
		"infra_audio",
		"infra_ccif_md",
		"infra_dxcc_sec_core",
		"infra_dxcc_ao",
		"infra_dramc_f26m",

		"infra_irtx",
		"infra_disppwm",
		"infra_cldma_bclk",
		"infracfg_ao_audio_26m_bclk_ck",
		"infra_spi1",
		"infra_i2c4",
		"infra_md_tmp_share",
		"infra_spi2",
		"infra_spi3",
		"infra_unipro_sck",
		"infra_unipro_tick",
		"infra_ufs_mp_sap_bck",
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
		"infra_ufs",
		"infra_aes",
		"infra_ufs_tick",
		"infra_ssusb_xhci",

		"infra_msdc0_self",
		"infra_msdc1_self",
		"infra_msdc2_self",
		"infra_sspm_26m_self",
		"infra_sspm_32k_self",
		"infra_ufs_axi",
		"infra_i2c6",
		"infra_ap_msdc0",
		"infra_md_msdc0",
		"infra_ccif2_ap",
		"infra_ccif2_md",
		"infra_ccif3_ap",
		"infra_ccif3_md",
		"infra_sej_f13m",
		"infra_aes_bclk",
		"infra_i2c7",
		"infra_i2c8",
		"infra_fbist2fpc",
		"infra_dpmaif",
		"infra_fadsp",
		"infra_ccif4_ap",
		"infra_ccif4_md",
		"infra_spi6",
		"infra_spi7",

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

		/* CAM */
		"camsys_larb10",
		"camsys_dfp_vad",
		"camsys_larb11",
		"camsys_larb9",
		"camsys_cam",
		"camsys_camtg",
		"camsys_seninf",
		"camsys_camsv0",
		"camsys_camsv1",
		"camsys_camsv2",
		"camsys_camsv3",
		"camsys_ccu",
		"camsys_fake_eng",

		/* IMG */
		"imgsys_larb5",
		"imgsys_larb6",
		"imgsys_dip",
		"imgsys_mfb",
		"imgsys_wpe_a",

		/* IPE */
		"ipe_larb7",
		"ipe_larb8",
		"ipe_smi_subcom",
		"ipe_fd",
		"ipe_fe",
		"ipe_rsc",
		"ipe_dpe",

		/* MFG */
		"mfg_cfg_bg3d",

		/* MM */
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_smi_larb1",
		"mm_gals_comm0",
		"mm_gals_comm1",
		"mm_gals_ccu2mm",
		"mm_gals_ipu12mm",
		"mm_gals_img2mm",
		"mm_gals_cam2mm",
		"mm_gals_ipu2mm",
		"mm_mdp_dl_txck",
		"mm_ipu_dl_txck",
		"mm_mdp_rdma0",
		"mm_mdp_rdma1",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_tdshp",
		"mm_mdp_wrot0",
		"mm_mdp_wrot1",
		"mm_fake_eng",
		"mm_disp_ovl0",
		"mm_disp_ovl0_2l",
		"mm_disp_ovl1_2l",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_wdma0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_dither0",
		"mm_disp_split",
		/* MM1 */
		"mm_dsi0_mmck",
		"mm_dsi0_ifck",
		"mm_dpi_mmck",
		"mm_dpi_ifck",
		"mm_fake_eng2",
		"mm_mdp_dl_rxck",
		"mm_ipu_dl_rxck",
		"mm_26m",
		"mm_mmsys_r2y",
		"mm_disp_rsz",
		"mm_mdp_aal",
		"mm_mdp_hdr",
		"mm_dbi_mmck",
		"mm_dbi_ifck",
		"mm_disp_pm0",
		"mm_disp_hrt_bw",
		"mm_disp_ovl_fbdc",

		/* VDEC */
		"vdec_cken",
		/* VDEC1 */
		"vdec_larb1_cken",

		/* VENC */
		"venc_larb",
		"venc_venc",
		"venc_jpgenc",
		"venc_gals",

		/* APUSYS CONN */
		"apu_conn_apu",
		"apu_conn_ahb",
		"apu_conn_axi",
		"apu_conn_isp",
		"apu_conn_cam_adl",
		"apu_conn_img_adl",
		"apu_conn_emi_26m",
		"apu_conn_vpu_udi",

		/* APUSYS APU0 */
		"apu0_apu",
		"apu0_axi",
		"apu0_jtag",

		/* APUSYS APU1 */
		"apu1_apu",
		"apu1_axi",
		"apu1_jtag",

		/* APUSYS VCORE */
		"apu_vcore_ahb",
		"apu_vcore_axi",
		"apu_vcore_adl",
		"apu_vcore_qos",

		/* APUSYS MDLA */
		"mdla_b0",
		"mdla_b1",
		"mdla_b2",
		"mdla_b3",
		"mdla_b4",
		"mdla_b5",
		"mdla_b6",
		"mdla_b7",
		"mdla_b8",
		"mdla_b9",
		"mdla_b10",
		"mdla_b11",
		"mdla_b12",
		"mdla_apb",

		/* SCPSYS */
		"pg_md1",
		"pg_conn",
		"pg_dis",
		"pg_cam",
		"pg_isp",
		"pg_ipe",
		"pg_ven",
		"pg_vde",
		"pg_audio",
		"pg_mfg0",
		"pg_mfg1",
		"pg_mfg2",
		"pg_mfg3",
		"pg_mfg4",
		"pg_vpu_vcore_d",
		"pg_vpu_vcore_s",
		"pg_vpu_conn_d",
		"pg_vpu_conn_s",
		"pg_vpu_core0_d",
		"pg_vpu_core0_s",
		"pg_vpu_core1_d",
		"pg_vpu_core1_s",
		"pg_vpu_core2_d",
		"pg_vpu_core2_s",
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
	if (!of_machine_is_compatible("mediatek,mt6785"))
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
subsys_initcall(clkchk_init);

/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Weiyi Lu <weiyi.lu@mediatek.com>
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

#define WARN_ON_CHECK_FAIL		0
#define CLKDBG_CCF_API_4_4		1

#define TAG	"[clkchk] "

#define clk_warn(fmt, args...)	pr_notice(TAG fmt, ##args)

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
		/* PLLs */
		"armpll",
		"armpll_l",
		"ccipll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mfgpll",
		"mmpll",
		"mpll",
		"apll1",

		/* TOP */
		"syspll_ck",
		"syspll_d2",
		"syspll1_d2",
		"syspll1_d4",
		"syspll1_d8",
		"syspll1_d16",
		"syspll_d3",
		"syspll2_d2",
		"syspll2_d4",
		"syspll2_d8",
		"syspll_d5",
		"syspll3_d2",
		"syspll3_d4",
		"syspll_d7",
		"syspll4_d2",
		"syspll4_d4",
		"usb20_192m_ck",
		"usb20_192m_d4",
		"usb20_192m_d8",
		"usb20_192m_d16",
		"usb20_192m_d32",
		"univpll_d2",
		"univpll1_d2",
		"univpll1_d4",
		"univpll_d3",
		"univpll2_d2",
		"univpll2_d4",
		"univpll2_d8",
		"univpll2_d32",
		"univpll_d5",
		"univpll3_d2",
		"univpll3_d4",
		"mmpll_ck",
		"mmpll_d2",
		"mpll_ck",
		"mpll_104m_div_ck",
		"mpll_52m_div_ck",
		"mfgpll_ck",
		"msdcpll_ck",
		"msdcpll_d2",
		"apll1_ck",
		"apll1_d2",
		"apll1_d4",
		"apll1_d8",
		"ulposc1_ck",
		"ulposc1_d2",
		"ulposc1_d4",
		"ulposc1_d8",
		"ulposc1_d16",
		"ulposc1_d32",
		"f_frtc_ck",
		"clk_26m_ck",
		"dmpll_ck",

		"axi_sel",
		"mem_sel",
		"mm_sel",
		"scp_sel",

		"mfg_sel",
		"atb_sel",
		"camtg_sel",
		"camtg1_sel",

		"camtg2_sel",
		"camtg3_sel",
		"uart_sel",
		"spi_sel",

		"msdc50_hclk_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"audio_sel",

		"aud_intbus_sel",
		"aud_1_sel",
		"aud_engen1_sel",
		"disp_pwm_sel",

		"sspm_sel",
		"dxcc_sel",
		"usb_top_sel",
		"spm_sel",

		"i2c_sel",
		"pwm_sel",
		"seninf_sel",
		"aes_fde_sel",

		"pwrap_ulposc_sel",
		"camtm_sel",

		/* INFRACFG */
		"ifr_axi_dis",
		"ifr_pmic_tmr",
		"ifr_pmic_ap",
		"ifr_pmic_md",
		"ifr_pmic_conn",
		"ifr_scp_core",
		"ifr_sej",
		"ifr_apxgpt",
		"ifr_icusb",
		"ifr_gce",
		"ifr_therm",
		"ifr_i2c_ap",
		"ifr_i2c_ccu",
		"ifr_i2c_sspm",
		"ifr_i2c_rsv",
		"ifr_pwm_hclk",
		"ifr_pwm1",
		"ifr_pwm2",
		"ifr_pwm3",
		"ifr_pwm4",
		"ifr_pwm5",
		"ifr_pwm",
		"ifr_uart0",
		"ifr_uart1",
		"ifr_gce_26m",
		"ifr_dma",
		"ifr_btif",
		"ifr_spi0",
		"ifr_msdc0",
		"ifr_msdc1",
		"ifr_dvfsrc",
		"ifr_gcpu",
		"ifr_trng",
		"ifr_auxadc",
		"ifr_cpum",
		"ifr_ccif1_ap",
		"ifr_ccif1_md",
		"ifr_auxadc_md",
		"ifr_ap_dma",
		"ifr_xiu",
		"ifr_dapc",
		"ifr_ccif_ap",
		"ifr_debugtop",
		"ifr_audio",
		"ifr_ccif_md",
		"ifr_secore",
		"ifr_dxcc_ao",
		"ifr_dramc26",
		"ifr_pwmfb",
		"ifr_disp_pwm",
		"ifr_cldmabclk",
		"ifr_audio26m",
		"ifr_spi1",
		"ifr_i2c4",
		"ifr_mdtemp",
		"ifr_spi2",
		"ifr_spi3",
		"ifr_hf_fsspm",
		"ifr_i2c5",
		"ifr_i2c5a",
		"ifr_i2c5_imm",
		"ifr_i2c1a",
		"ifr_i2c1_imm",
		"ifr_i2c2a",
		"ifr_i2c2_imm",
		"ifr_spi4",
		"ifr_spi5",
		"ifr_cq_dma",
		"ifr_faes_fde_ck",
		"ifr_msdc0f",
		"ifr_msdc1sf",
		"ifr_sspm_26m",
		"ifr_sspm_32k",
		"ifr_i2c6",
		"ifr_ap_msdc0",
		"ifr_md_msdc0",
		"ifr_msdc0_clk",
		"ifr_msdc1_clk",
		"ifr_sej_f13m",
		"ifr_aes",
		"ifr_mcu_pm_bclk",
		"ifr_ccif2_ap",
		"ifr_ccif2_md",
		"ifr_ccif3_ap",
		"ifr_ccif3_md",

		/* PERICFG */
		"periaxi_disable",

		/* AUDIO */
		"aud_afe",
		"aud_22m",
		"aud_apll_tuner",
		"aud_adc",
		"aud_dac",
		"aud_dac_predis",
		"aud_tml",
		"aud_i2s1_bclk",
		"aud_i2s2_bclk",
		"aud_i2s3_bclk",
		"aud_i2s4_bclk",

		/* CAM */
		"cam_larb3",
		"cam_dfp_vad",
		"cam",
		"camtg",
		"cam_seninf",
		"camsv0",
		"camsv1",
		"camsv2",
		"cam_ccu",

		/* IMG */
		"img_larb2",
		"img_dip",
		"img_fdvt",
		"img_dpe",
		"img_rsc",

		/* MFG */
		"mfgcfg_baxi",
		"mfgcfg_bmem",
		"mfgcfg_bg3d",
		"mfgcfg_b26m",

		/* MM */
		"mm_mdp_rdma0",
		"mm_mdp_ccorr0",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_tdshp0",
		"mm_mdp_wrot0",
		"mm_mdp_wdma0",
		"mm_disp_ovl0",
		"mm_disp_ovl0_2l",
		"mm_disp_rsz",
		"mm_disp_rdma0",
		"mm_disp_wdma0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_dither0",
		"mm_dsi0",
		"mm_fake_eng",
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_smi_comm0",
		"mm_smi_comm1",
		"mm_cam_mdp_ck",
		"mm_smi_img_ck",
		"mm_smi_cam_ck",
		"mm_img_dl_relay",
		"mm_imgdl_async",
		"mm_dig_dsi_ck",
		"mm_hrtwt",

		/* VENC */
		"venc_set0_larb",
		"venc_set1_venc",
		"jpgenc",
		"venc_set3_vdec",

		/* MIPI */
		"mipi0a_csr_0a",
		"mipi0b_csr_0b",
		"mipi1a_csr_1a",
		"mipi1b_csr_1b",
		"mipi2a_csr_2a",
		"mipi2b_csr_2b",

		/* GCE */
		"gce",

		/* APMIXED 26MCK */
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

	clk_warn("enabled clks:\n");

	for (; *cn; cn++) {
		struct clk *c = __clk_lookup(*cn);
		struct clk_hw *c_hw = __clk_get_hw(c);
		struct clk_hw *p_hw;

		if (IS_ERR_OR_NULL(c) || !c_hw)
			continue;

		p_hw = clk_hw_get_parent(c_hw);

		if (!p_hw)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !__clk_get_enable_count(c))
			continue;

		clk_warn("[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
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
		"univpll",
		"apll1",
		"mfgpll",
		"msdcpll",
		"mmpll",
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

		if (!clk_hw_is_enabled(c_hw))
			continue;

		n += snprintf(buf + n, sizeof(buf) - n, "%s ",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid) {
		clk_warn("unexpected unclosed PLL: %s\n", buf);
		print_enabled_clks();

#if WARN_ON_CHECK_FAIL
		WARN_ON(1);
#endif
	}
}

static void check_mtcmos_off(void)
{
	static const char * const off_mtcmos_names[] = {
		"pg_conn",
		"pg_dis",
		"pg_mfg",
		"pg_isp",
		"pg_mfg_core0",
		"pg_mfg_async",
		"pg_cam",
		"pg_vcodec",
		NULL
	};

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

		n += snprintf(buf + n, sizeof(buf) - n, "%s ",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid) {
		clk_warn("unexpected unclosed MTCMOS: %s\n", buf);

#if WARN_ON_CHECK_FAIL
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

static int __init clkchk_init(void)
{
	if (!of_machine_is_compatible("mediatek,MT6765"))
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
subsys_initcall(clkchk_init);

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

#define WARN_ON_CHECK_PLL_FAIL		0
#define CLKDBG_CCF_API_4_4	1

#define TAG	"[clkchk] "

#define clk_warn(fmt, args...)	pr_warn(TAG fmt, ##args)

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
		"univpll",
		"vcodecpll",
		"vencpll",
		"apll1",
		"apll2",
		"lvdspll",
		"lvdspll2",
		"msdcpll",
		"msdcpll2",
		"tvdpll",
		"mmpll",
		"armca35pll",
		"armca72pll",
		"etherpll",
		"cvbspll",
		/* apmixedsys */
		"ref2usb_tx",
		/* topckgen */
		"armca35pll_ck",
		"armca35pll_600m",
		"armca35pll_400m",
		"armca72pll_ck",
		"syspll_ck",
		"syspll_d2",
		"syspll1_d2",
		"syspll1_d4",
		"syspll1_d8",
		"syspll1_d16",
		"syspll_d3",
		"syspll2_d2",
		"syspll2_d4",
		"syspll_d5",
		"syspll3_d2",
		"syspll3_d4",
		"syspll_d7",
		"syspll4_d2",
		"syspll4_d4",
		"univpll_ck",
		"univpll_d7",
		"univpll_d26",
		"univpll_d52",
		"univpll_d104",
		"univpll_d208",
		"univpll_d2",
		"univpll1_d2",
		"univpll1_d4",
		"univpll1_d8",
		"univpll_d3",
		"univpll2_d2",
		"univpll2_d4",
		"univpll2_d8",
		"univpll_d5",
		"univpll3_d2",
		"univpll3_d4",
		"univpll3_d8",
		"f_mp0_pll1_ck",
		"f_mp0_pll2_ck",
		"f_big_pll1_ck",
		"f_big_pll2_ck",
		"f_bus_pll1_ck",
		"f_bus_pll2_ck",
		"apll1_ck",
		"apll1_d2",
		"apll1_d4",
		"apll1_d8",
		"apll1_d16",
		"apll2_ck",
		"apll2_d2",
		"apll2_d4",
		"apll2_d8",
		"apll2_d16",
		"lvdspll_ck",
		"lvdspll_d2",
		"lvdspll_d4",
		"lvdspll_d8",
		"lvdspll2_ck",
		"lvdspll2_d2",
		"lvdspll2_d4",
		"lvdspll2_d8",
		"etherpll_125m",
		"etherpll_50m",
		"cvbs",
		"cvbs_d2",
		"sys_26m",
		"mmpll_ck",
		"mmpll_d2",
		"vencpll_ck",
		"vencpll_d2",
		"vcodecpll_ck",
		"vcodecpll_d2",
		"tvdpll_ck",
		"tvdpll_d2",
		"tvdpll_d4",
		"tvdpll_d8",
		"tvdpll_429m",
		"tvdpll_429m_d2",
		"tvdpll_429m_d4",
		"msdcpll_ck",
		"msdcpll_d2",
		"msdcpll_d4",
		"msdcpll2_ck",
		"msdcpll2_d2",
		"msdcpll2_d4",
		"clk26m_d2",
		"d2a_ulclk_6p5m",
		"vpll3_dpix",
		"vpll_dpix",
		"ltepll_fs26m",
		"dmpll_ck",
		"dsi0_lntc",
		"dsi1_lntc",
		"lvdstx3",
		"lvdstx",
		"clkrtc_ext",
		"clkrtc_int",
		"csi0",
		"apll_div0",
		"apll_div1",
		"apll_div2",
		"apll_div3",
		"apll_div4",
		"apll_div5",
		"apll_div6",
		"apll_div7",
		"apll_div_pdn0",
		"apll_div_pdn1",
		"apll_div_pdn2",
		"apll_div_pdn3",
		"apll_div_pdn4",
		"apll_div_pdn5",
		"apll_div_pdn6",
		"apll_div_pdn7",
		"axi_sel",
		"mem_sel",
		"mm_sel",
		"pwm_sel",
		"vdec_sel",
		"venc_sel",
		"mfg_sel",
		"camtg_sel",
		"uart_sel",
		"spi_sel",
		"usb20_sel",
		"usb30_sel",
		"msdc50_0_h_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"msdc30_2_sel",
		"msdc30_3_sel",
		"audio_sel",
		"aud_intbus_sel",
		"pmicspi_sel",
		"dpilvds1_sel",
		"atb_sel",
		"nr_sel",
		"nfi2x_sel",
		"irda_sel",
		"cci400_sel",
		"aud_1_sel",
		"aud_2_sel",
		"mem_mfg_sel",
		"axi_mfg_sel",
		"scam_sel",
		"nfiecc_sel",
		"pe2_mac_p0_sel",
		"pe2_mac_p1_sel",
		"dpilvds_sel",
		"msdc50_3_h_sel",
		"hdcp_sel",
		"hdcp_24m_sel",
		"rtc_sel",
		"spinor_sel",
		"apll_sel",
		"apll2_sel",
		"a1sys_hp_sel",
		"a2sys_hp_sel",
		"asm_l_sel",
		"asm_m_sel",
		"asm_h_sel",
		"i2so1_sel",
		"i2so2_sel",
		"i2so3_sel",
		"tdmo0_sel",
		"tdmo1_sel",
		"i2si1_sel",
		"i2si2_sel",
		"i2si3_sel",
		"ether_125m_sel",
		"ether_50m_sel",
		"jpgdec_sel",
		"spislv_sel",
		"ether_sel",
		"cam2tg_sel",
		"di_sel",
		"tvd_sel",
		"i2c_sel",
		"pwm_infra_sel",
		"msdc0p_aes_sel",
		"cmsys_sel",
		"gcpu_sel",
		"aud_apll1_sel",
		"aud_apll2_sel",
		"audull_vtx_sel",
		/* mcucfg */
		"mcu_mp0_sel",
		"mcu_mp2_sel",
		"mcu_bus_sel",
		/* bdpsys */
		"bdp_bridge_b",
		"bdp_bridge_d",
		"bdp_larb_d",
		"bdp_vdi_pxl",
		"bdp_vdi_d",
		"bdp_vdi_b",
		"bdp_fmt_b",
		"bdp_27m",
		"bdp_27m_vdout",
		"bdp_27_74_74",
		"bdp_2fs",
		"bdp_2fs74_148",
		"bdp_b",
		"bdp_vdo_d",
		"bdp_vdo_2fs",
		"bdp_vdo_b",
		"bdp_di_pxl",
		"bdp_di_d",
		"bdp_di_b",
		"bdp_nr_agent",
		"bdp_nr_d",
		"bdp_nr_b",
		"bdp_bridge_rt_b",
		"bdp_bridge_rt_d",
		"bdp_larb_rt_d",
		"bdp_tvd_tdc",
		"bdp_tvd_clk_54",
		"bdp_tvd_cbus",
		/* infracfg */
		"infra_dbgclk",
		"infra_gce",
		"infra_m4u",
		"infra_kp",
		"infra_ao_spi0",
		"infra_ao_spi1",
		"infra_ao_uart5",
		/* imgsys */
		"img_smi_larb2",
		"img_scam_en",
		"img_cam_en",
		"img_cam_sv_en",
		"img_cam_sv1_en",
		"img_cam_sv2_en",
		/* jpgdecsys */
		"jpgdec_jpgdec1",
		"jpgdec_jpgdec",
		/* mfgcfg */
		"mfg_bg3d",
		/* mmsys */
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_cam_mdp",
		"mm_mdp_rdma0",
		"mm_mdp_rdma1",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_rsz2",
		"mm_mdp_tdshp0",
		"mm_mdp_tdshp1",
		"mm_mdp_crop",
		"mm_mdp_wdma",
		"mm_mdp_wrot0",
		"mm_mdp_wrot1",
		"mm_fake_eng",
		"mm_mutex_32k",
		"mm_disp_ovl0",
		"mm_disp_ovl1",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_rdma2",
		"mm_disp_wdma0",
		"mm_disp_wdma1",
		"mm_disp_color0",
		"mm_disp_color1",
		"mm_disp_aal",
		"mm_disp_gamma",
		"mm_disp_ufoe",
		"mm_disp_split0",
		"mm_disp_od",
		"mm_pwm0_mm",
		"mm_pwm0_26m",
		"mm_pwm1_mm",
		"mm_pwm1_26m",
		"mm_dsi0_engine",
		"mm_dsi0_digital",
		"mm_dsi1_engine",
		"mm_dsi1_digital",
		"mm_dpi_pixel",
		"mm_dpi_engine",
		"mm_dpi1_pixel",
		"mm_dpi1_engine",
		"mm_lvds_pixel",
		"mm_lvds_cts",
		"mm_smi_larb4",
		"mm_smi_common1",
		"mm_smi_larb5",
		"mm_mdp_rdma2",
		"mm_mdp_tdshp2",
		"mm_disp_ovl2",
		"mm_disp_wdma2",
		"mm_disp_color2",
		"mm_disp_aal1",
		"mm_disp_od1",
		"mm_lvds1_pixel",
		"mm_lvds1_cts",
		"mm_smi_larb7",
		"mm_mdp_rdma3",
		"mm_mdp_wrot2",
		"mm_dsi2",
		"mm_dsi2_digital",
		"mm_dsi3",
		"mm_dsi3_digital",
		/* pericfg */
		"per_nfi",
		"per_therm",
		"per_pwm0",
		"per_pwm1",
		"per_pwm2",
		"per_pwm3",
		"per_pwm4",
		"per_pwm5",
		"per_pwm6",
		"per_pwm7",
		"per_pwm",
		"per_ap_dma",
		"per_msdc30_0",
		"per_msdc30_1",
		"per_msdc30_2",
		"per_msdc30_3",
		"per_uart0",
		"per_uart1",
		"per_uart2",
		"per_uart3",
		"per_i2c0",
		"per_i2c1",
		"per_i2c2",
		"per_i2c3",
		"per_i2c4",
		"per_auxadc",
		"per_spi0",
		"per_spi",
		"per_i2c5",
		"per_spi2",
		"per_spi3",
		"per_spi5",
		"per_uart4",
		"per_sflash",
		"per_gmac",
		"per_pcie0",
		"per_pcie1",
		"per_gmac_pclk",
		"per_msdc50_0_en",
		"per_msdc30_1_en",
		"per_msdc30_2_en",
		"per_msdc30_3_en",
		"per_msdc50_0_h",
		"per_msdc50_3_h",
		/* vdecsys */
		"vdec_cken",
		"vdec_larb1_cken",
		"vdec_imgrz_cken",
		/* vencsys */
		"venc_smi",
		"venc_venc",
		"venc_smi_larb6",
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
		"vcodecpll",
		"vencpll",
		"apll1",
		"apll2",
		"lvdspll",
		"lvdspll2",
		"msdcpll",
		"msdcpll2",
		"tvdpll",
		"mmpll",
		"armca72pll",
		"etherpll",
		"cvbspll",
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

		if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))
			continue;

		n += snprintf(buf + n, sizeof(buf) - n, "%s ",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid) {
		clk_warn("unexpected unclosed PLL: %s\n", buf);
		print_enabled_clks();

#if WARN_ON_CHECK_PLL_FAIL
		WARN_ON(1);
#endif
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
	if (!of_machine_is_compatible("mediatek,mt2712"))
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
subsys_initcall(clkchk_init);

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

#define WARN_ON_CHECK_PLL_FAIL	0
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

const char * const *get_mt6873_all_clk_names(void)
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
		"imgsys2_larb9_cgpdn",
		"imgsys2_larb10_cgpdn",
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

static const char *ccf_state(struct clk_hw *hw)
{
	if (__clk_get_enable_count(hw->clk))
		return "enabled";

	if (clk_hw_is_prepared(hw))
		return "prepared";

	return "disabled";
}

static void print_enabled_clks(int is_deferred)
{
	const char * const *cn = get_mt6873_all_clk_names();

	if (is_deferred)
		pr_notice("enabled clks:\n");
	else
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

		if (is_deferred)
			pr_notice(
				"[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
				clk_hw_get_name(c_hw),
				ccf_state(c_hw),
				clk_hw_is_prepared(c_hw),
				__clk_get_enable_count(c),
				clk_hw_get_rate(c_hw),
				p_hw ? clk_hw_get_name(p_hw) : "- ");
		else
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
		"univpll",
		"mfgpll",
		"msdcpll",
		"tvdpll",
		"adsppll",
		"mmpll",
		"apll1",
		"apll2",
		"apupll",
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

		print_enabled_clks(1);

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
		print_enabled_clks(0);
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
	if (!of_machine_is_compatible("mediatek,MT6873"))
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
subsys_initcall(clkchk_init);

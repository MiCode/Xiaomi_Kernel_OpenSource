// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "clk-debug.h"
#include "common.h"

static struct measure_clk_data debug_mux_priv = {
	.ctl_reg = 0x62038,
	.status_reg = 0x6203C,
	.xo_div4_cbcr = 0x2800C,
};

static const char *const apss_cc_debug_mux_parent_names[] = {
	"measure_only_apcs_silver_post_acd_clk",
};

static int apss_cc_debug_mux_sels[] = {
	0x0,		/* measure_only_apcs_silver_post_acd_clk */
};

static int apss_cc_debug_mux_pre_divs[] = {
	0x8,		/* measure_only_apcs_silver_post_acd_clk */
};

static struct clk_debug_mux apss_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x0,
	.post_div_offset = 0x0,
	.cbcr_offset = 0x0,
	.src_sel_mask = 0x3FF00,
	.src_sel_shift = 8,
	.post_div_mask = 0xF0000000,
	.post_div_shift = 28,
	.post_div_val = 1,
	.mux_sels = apss_cc_debug_mux_sels,
	.pre_div_vals = apss_cc_debug_mux_pre_divs,
	.hw.init = &(struct clk_init_data){
		.name = "apss_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apss_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apss_cc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static const char *const disp_cc_debug_mux_parent_names[] = {
	"disp_cc_mdss_ahb_clk",
	"disp_cc_mdss_byte0_clk",
	"disp_cc_mdss_byte0_intf_clk",
	"disp_cc_mdss_esc0_clk",
	"disp_cc_mdss_mdp_clk",
	"disp_cc_mdss_mdp_lut_clk",
	"disp_cc_mdss_non_gdsc_ahb_clk",
	"disp_cc_mdss_pclk0_clk",
	"disp_cc_mdss_vsync_clk",
	"disp_cc_sleep_clk",
	"disp_cc_xo_clk",
};

static int disp_cc_debug_mux_sels[] = {
	0x1A,		/* disp_cc_mdss_ahb_clk */
	0x11,		/* disp_cc_mdss_byte0_clk */
	0x12,		/* disp_cc_mdss_byte0_intf_clk */
	0x13,		/* disp_cc_mdss_esc0_clk */
	0xE,		/* disp_cc_mdss_mdp_clk */
	0xF,		/* disp_cc_mdss_mdp_lut_clk */
	0x1B,		/* disp_cc_mdss_non_gdsc_ahb_clk */
	0xD,		/* disp_cc_mdss_pclk0_clk */
	0x10,		/* disp_cc_mdss_vsync_clk */
	0x24,		/* disp_cc_sleep_clk */
	0x23,		/* disp_cc_xo_clk */
};

static struct clk_debug_mux disp_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x7000,
	.post_div_offset = 0x5008,
	.cbcr_offset = 0x500C,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0x3,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = disp_cc_debug_mux_sels,
	.hw.init = &(struct clk_init_data){
		.name = "disp_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = disp_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(disp_cc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static const char *const gcc_debug_mux_parent_names[] = {
	"apss_cc_debug_mux",
	"disp_cc_debug_mux",
	"gcc_ahb2phy_csi_clk",
	"gcc_ahb2phy_usb_clk",
	"gcc_bimc_gpu_axi_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_cam_throttle_nrt_clk",
	"gcc_cam_throttle_rt_clk",
	"gcc_camera_ahb_clk",
	"gcc_camera_xo_clk",
	"gcc_camss_axi_clk",
	"gcc_camss_camnoc_atb_clk",
	"gcc_camss_camnoc_nts_xo_clk",
	"gcc_camss_cci_0_clk",
	"gcc_camss_cphy_0_clk",
	"gcc_camss_cphy_1_clk",
	"gcc_camss_csi0phytimer_clk",
	"gcc_camss_csi1phytimer_clk",
	"gcc_camss_mclk0_clk",
	"gcc_camss_mclk1_clk",
	"gcc_camss_mclk2_clk",
	"gcc_camss_mclk3_clk",
	"gcc_camss_nrt_axi_clk",
	"gcc_camss_ope_ahb_clk",
	"gcc_camss_ope_clk",
	"gcc_camss_rt_axi_clk",
	"gcc_camss_tfe_0_clk",
	"gcc_camss_tfe_0_cphy_rx_clk",
	"gcc_camss_tfe_0_csid_clk",
	"gcc_camss_tfe_1_clk",
	"gcc_camss_tfe_1_cphy_rx_clk",
	"gcc_camss_tfe_1_csid_clk",
	"gcc_camss_top_ahb_clk",
	"gcc_cfg_noc_usb2_prim_axi_clk",
	"gcc_cpuss_gnoc_clk",
	"gcc_cpuss_trig_clk",
	"gcc_disp_ahb_clk",
	"gcc_disp_gpll0_clk_src",
	"gcc_disp_hf_axi_clk",
	"gcc_disp_throttle_core_clk",
	"gcc_disp_xo_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_gpu_cfg_ahb_clk",
	"gcc_gpu_gpll0_clk_src",
	"gcc_gpu_gpll0_div_clk_src",
	"gcc_gpu_memnoc_gfx_clk",
	"gcc_gpu_snoc_dvm_gfx_clk",
	"gcc_gpu_throttle_core_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_pwm0_xo512_clk",
	"gcc_qmip_camera_nrt_ahb_clk",
	"gcc_qmip_camera_rt_ahb_clk",
	"gcc_qmip_disp_ahb_clk",
	"gcc_qmip_gpu_cfg_ahb_clk",
	"gcc_qmip_video_vcodec_ahb_clk",
	"gcc_qupv3_wrap0_core_2x_clk",
	"gcc_qupv3_wrap0_core_clk",
	"gcc_qupv3_wrap0_s0_clk",
	"gcc_qupv3_wrap0_s1_clk",
	"gcc_qupv3_wrap0_s2_clk",
	"gcc_qupv3_wrap0_s3_clk",
	"gcc_qupv3_wrap0_s4_clk",
	"gcc_qupv3_wrap0_s5_clk",
	"gcc_qupv3_wrap0_s6_clk",
	"gcc_qupv3_wrap0_s7_clk",
	"gcc_qupv3_wrap_0_m_ahb_clk",
	"gcc_qupv3_wrap_0_s_ahb_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_sys_noc_cpuss_ahb_clk",
	"gcc_sys_noc_usb2_prim_axi_clk",
	"gcc_usb20_master_clk",
	"gcc_usb20_mock_utmi_clk",
	"gcc_usb20_sleep_clk",
	"gcc_vcodec0_axi_clk",
	"gcc_venus_ahb_clk",
	"gcc_venus_ctl_axi_clk",
	"gcc_video_ahb_clk",
	"gcc_video_throttle_core_clk",
	"gcc_video_vcodec0_sys_clk",
	"gcc_video_venus_ctl_clk",
	"gcc_video_xo_clk",
	"gpu_cc_debug_mux",
	"mc_cc_debug_mux",
	"measure_only_cnoc_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_snoc_clk",
	"measure_only_hwkm_km_core_clk",
	"measure_only_hwkm_ahb_clk",
	"measure_only_pka_core_clk",
	"measure_only_pka_ahb_clk",
	"measure_only_cpuss_gnoc_clk",
};

static int gcc_debug_mux_sels[] = {
	0xAB,		/* apss_cc_debug_mux */
	0x40,		/* disp_cc_debug_mux */
	0x5E,		/* gcc_ahb2phy_csi_clk */
	0x5F,		/* gcc_ahb2phy_usb_clk */
	0x8E,		/* gcc_bimc_gpu_axi_clk */
	0x73,		/* gcc_boot_rom_ahb_clk */
	0x4A,		/* gcc_cam_throttle_nrt_clk */
	0x49,		/* gcc_cam_throttle_rt_clk */
	0x36,		/* gcc_camera_ahb_clk */
	0x3D,		/* gcc_camera_xo_clk */
	0x120,		/* gcc_camss_axi_clk */
	0x122,		/* gcc_camss_camnoc_atb_clk */
	0x124,		/* gcc_camss_camnoc_nts_xo_clk */
	0x11E,		/* gcc_camss_cci_0_clk */
	0x115,		/* gcc_camss_cphy_0_clk */
	0x116,		/* gcc_camss_cphy_1_clk */
	0x10B,		/* gcc_camss_csi0phytimer_clk */
	0x10C,		/* gcc_camss_csi1phytimer_clk */
	0x10D,		/* gcc_camss_mclk0_clk */
	0x10E,		/* gcc_camss_mclk1_clk */
	0x10F,		/* gcc_camss_mclk2_clk */
	0x110,		/* gcc_camss_mclk3_clk */
	0x125,		/* gcc_camss_nrt_axi_clk */
	0x11D,		/* gcc_camss_ope_ahb_clk */
	0x11B,		/* gcc_camss_ope_clk */
	0x127,		/* gcc_camss_rt_axi_clk */
	0x111,		/* gcc_camss_tfe_0_clk */
	0x113,		/* gcc_camss_tfe_0_cphy_rx_clk */
	0x117,		/* gcc_camss_tfe_0_csid_clk */
	0x112,		/* gcc_camss_tfe_1_clk */
	0x114,		/* gcc_camss_tfe_1_cphy_rx_clk */
	0x119,		/* gcc_camss_tfe_1_csid_clk */
	0x11F,		/* gcc_camss_top_ahb_clk */
	0x1B,		/* gcc_cfg_noc_usb2_prim_axi_clk */
	0xA6,		/* gcc_cpuss_gnoc_clk */
	0xA7,		/* gcc_cpuss_trig_clk */
	0x37,		/* gcc_disp_ahb_clk */
	0x45,		/* gcc_disp_gpll0_clk_src */
	0x3B,		/* gcc_disp_hf_axi_clk */
	0x47,		/* gcc_disp_throttle_core_clk */
	0x3E,		/* gcc_disp_xo_clk */
	0xB8,		/* gcc_gp1_clk */
	0xB9,		/* gcc_gp2_clk */
	0xBA,		/* gcc_gp3_clk */
	0xE1,		/* gcc_gpu_cfg_ahb_clk */
	0xE7,		/* gcc_gpu_gpll0_clk_src */
	0xE8,		/* gcc_gpu_gpll0_div_clk_src */
	0xE4,		/* gcc_gpu_memnoc_gfx_clk */
	0xE6,		/* gcc_gpu_snoc_dvm_gfx_clk */
	0xEB,		/* gcc_gpu_throttle_core_clk */
	0x70,		/* gcc_pdm2_clk */
	0x6E,		/* gcc_pdm_ahb_clk */
	0x6F,		/* gcc_pdm_xo4_clk */
	0x71,		/* gcc_pwm0_xo512_clk */
	0x39,		/* gcc_qmip_camera_nrt_ahb_clk */
	0x46,		/* gcc_qmip_camera_rt_ahb_clk */
	0x3A,		/* gcc_qmip_disp_ahb_clk */
	0xE9,		/* gcc_qmip_gpu_cfg_ahb_clk */
	0x38,		/* gcc_qmip_video_vcodec_ahb_clk */
	0x65,		/* gcc_qupv3_wrap0_core_2x_clk */
	0x64,		/* gcc_qupv3_wrap0_core_clk */
	0x66,		/* gcc_qupv3_wrap0_s0_clk */
	0x67,		/* gcc_qupv3_wrap0_s1_clk */
	0x68,		/* gcc_qupv3_wrap0_s2_clk */
	0x69,		/* gcc_qupv3_wrap0_s3_clk */
	0x6A,		/* gcc_qupv3_wrap0_s4_clk */
	0x6B,		/* gcc_qupv3_wrap0_s5_clk */
	0x6C,		/* gcc_qupv3_wrap0_s6_clk */
	0x6D,		/* gcc_qupv3_wrap0_s7_clk */
	0x62,		/* gcc_qupv3_wrap_0_m_ahb_clk */
	0x63,		/* gcc_qupv3_wrap_0_s_ahb_clk */
	0xEF,		/* gcc_sdcc1_ahb_clk */
	0xEE,		/* gcc_sdcc1_apps_clk */
	0xF0,		/* gcc_sdcc1_ice_core_clk */
	0x61,		/* gcc_sdcc2_ahb_clk */
	0x60,		/* gcc_sdcc2_apps_clk */
	0x9,		/* gcc_sys_noc_cpuss_ahb_clk */
	0x16,		/* gcc_sys_noc_usb2_prim_axi_clk */
	0x5A,		/* gcc_usb20_master_clk */
	0x5C,		/* gcc_usb20_mock_utmi_clk */
	0x5B,		/* gcc_usb20_sleep_clk */
	0x12F,		/* gcc_vcodec0_axi_clk */
	0x130,		/* gcc_venus_ahb_clk */
	0x12E,		/* gcc_venus_ctl_axi_clk */
	0x35,		/* gcc_video_ahb_clk */
	0x48,		/* gcc_video_throttle_core_clk */
	0x12C,		/* gcc_video_vcodec0_sys_clk */
	0x12A,		/* gcc_video_venus_ctl_clk */
	0x3C,		/* gcc_video_xo_clk */
	0xE3,		/* gpu_cc_debug_mux */
	0x9F,		/* mc_cc_debug_mux */
	0x18,		/* measure_only_cnoc_clk */
	0xC5,		/* measure_only_ipa_2x_clk */
	0x7,		/* measure_only_snoc_clk */
	0xA0,		/* measure_only_hwkm_km_core_clk */
	0xA2,		/* measure_only_hwkm_ahb_clk */
	0xA3,		/* measure_only_pka_core_clk */
	0xA4,		/* measure_only_pka_ahb_clk */
	0xA6,		/* measure_only_cpuss_gnoc_clk */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x62000,
	.post_div_offset = 0x30000,
	.cbcr_offset = 0x30004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 1,
	.mux_sels = gcc_debug_mux_sels,
	.hw.init = &(struct clk_init_data){
		.name = "gcc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gcc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gcc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static const char *const gpu_cc_debug_mux_parent_names[] = {
	"gpu_cc_ahb_clk",
	"gpu_cc_crc_ahb_clk",
	"gpu_cc_cx_apb_clk",
	"gpu_cc_cx_gfx3d_clk",
	"gpu_cc_cx_gfx3d_slv_clk",
	"gpu_cc_cx_gmu_clk",
	"gpu_cc_cx_snoc_dvm_clk",
	"gpu_cc_cxo_aon_clk",
	"gpu_cc_cxo_clk",
	"gpu_cc_gx_cxo_clk",
	"gpu_cc_gx_gfx3d_clk",
	"gpu_cc_sleep_clk",
};

static int gpu_cc_debug_mux_sels[] = {
	0x10,		/* gpu_cc_ahb_clk */
	0x11,		/* gpu_cc_crc_ahb_clk */
	0x14,		/* gpu_cc_cx_apb_clk */
	0x1A,		/* gpu_cc_cx_gfx3d_clk */
	0x1B,		/* gpu_cc_cx_gfx3d_slv_clk */
	0x18,		/* gpu_cc_cx_gmu_clk */
	0x15,		/* gpu_cc_cx_snoc_dvm_clk */
	0xA,		/* gpu_cc_cxo_aon_clk */
	0x19,		/* gpu_cc_cxo_clk */
	0xE,		/* gpu_cc_gx_cxo_clk */
	0xB,		/* gpu_cc_gx_gfx3d_clk */
	0x16,		/* gpu_cc_sleep_clk */
};

static struct clk_debug_mux gpu_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x4568,
	.post_div_offset = 0x40FC,
	.cbcr_offset = 0x4100,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0x3,
	.post_div_shift = 0,
	.post_div_val = 2,
	.mux_sels = gpu_cc_debug_mux_sels,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gpu_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gpu_cc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static const char *const mc_cc_debug_mux_parent_names[] = {
	"measure_only_mccc_clk",
};

static struct clk_debug_mux mc_cc_debug_mux = {
	.period_offset = 0x20,
	.hw.init = &(struct clk_init_data){
		.name = "mc_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = mc_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(mc_cc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static struct mux_regmap_names mux_list[] = {
	{ .mux = &apss_cc_debug_mux, .regmap_name = "qcom,apsscc" },
	{ .mux = &disp_cc_debug_mux, .regmap_name = "qcom,dispcc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
	{ .mux = &gpu_cc_debug_mux, .regmap_name = "qcom,gpucc" },
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
};

static struct clk_dummy measure_only_mccc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_mccc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_silver_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_silver_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cnoc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_cnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ipa_2x_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_ipa_2x_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_snoc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_snoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_hwkm_km_core_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_hwkm_km_core_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_hwkm_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_hwkm_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pka_core_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pka_core_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pka_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pka_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cpuss_gnoc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_cpuss_gnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_monaco_hws[] = {
	&measure_only_apcs_silver_post_acd_clk.hw,
	&measure_only_cnoc_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_snoc_clk.hw,
	&measure_only_hwkm_km_core_clk.hw,
	&measure_only_hwkm_ahb_clk.hw,
	&measure_only_pka_core_clk.hw,
	&measure_only_pka_ahb_clk.hw,
	&measure_only_cpuss_gnoc_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,monaco-debugcc" },
	{ }
};

static int clk_debug_monaco_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret, i;

	BUILD_BUG_ON(ARRAY_SIZE(apss_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(apss_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(disp_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(disp_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) !=
		ARRAY_SIZE(gcc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gpu_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(gpu_cc_debug_mux_sels));

	clk = devm_clk_get(&pdev->dev, "xo_clk_src");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo clock\n");
		return PTR_ERR(clk);
	}

	debug_mux_priv.cxo = clk;

	for (i = 0; i < ARRAY_SIZE(mux_list); i++) {
		ret = map_debug_bases(pdev, mux_list[i].regmap_name,
				      mux_list[i].mux);
		if (ret == -EBADR)
			continue;
		else if (ret)
			return ret;

		clk = devm_clk_register(&pdev->dev, &mux_list[i].mux->hw);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(&mux_list[i].mux->hw),
				PTR_ERR(clk));
			return PTR_ERR(clk);
		}
	}

	for (i = 0; i < ARRAY_SIZE(debugcc_monaco_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_monaco_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_monaco_hws[i]),
				PTR_ERR(clk));
			return PTR_ERR(clk);
		}
	}

	ret = clk_debug_measure_register(&gcc_debug_mux.hw);
	if (ret) {
		dev_err(&pdev->dev, "Could not register Measure clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered debug measure clocks\n");

	return ret;
}

static struct platform_driver clk_debug_driver = {
	.probe = clk_debug_monaco_probe,
	.driver = {
		.name = "monaco-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_monaco_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_monaco_init);

MODULE_DESCRIPTION("QTI DEBUG CC MONACO Driver");
MODULE_LICENSE("GPL v2");

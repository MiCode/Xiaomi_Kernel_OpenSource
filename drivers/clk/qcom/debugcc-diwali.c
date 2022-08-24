// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022, Qualcomm Innovation Center, Inc. All rights reserved.
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
	.ctl_reg = 0x72038,
	.status_reg = 0x7203C,
	.xo_div4_cbcr = 0x7200C,
};

static const char *const apss_cc_debug_mux_parent_names[] = {
	"measure_only_apcs_gold_post_acd_clk",
	"measure_only_apcs_gold_pre_acd_clk",
	"measure_only_apcs_goldplus_post_acd_clk",
	"measure_only_apcs_goldplus_pre_acd_clk",
	"measure_only_apcs_l3_post_acd_clk",
	"measure_only_apcs_l3_pre_acd_clk",
	"measure_only_apcs_silver_post_acd_clk",
	"measure_only_apcs_silver_pre_acd_clk",
};

static int apss_cc_debug_mux_sels[] = {
	0x4,		/* measure_only_apcs_gold_post_acd_clk */
	0x3,		/* measure_only_apcs_gold_pre_acd_clk */
	0x8,		/* measure_only_apcs_goldplus_post_acd_clk */
	0x7,		/* measure_only_apcs_goldplus_pre_acd_clk */
	0x6,		/* measure_only_apcs_l3_post_acd_clk */
	0x5,		/* measure_only_apcs_l3_pre_acd_clk */
	0x2,		/* measure_only_apcs_silver_post_acd_clk */
	0x1,		/* measure_only_apcs_silver_pre_acd_clk */
};

static int apss_cc_debug_mux_pre_divs[] = {
	0x8,		/* measure_only_apcs_gold_post_acd_clk */
	0x8,		/* measure_only_apcs_gold_pre_acd_clk */
	0x8,		/* measure_only_apcs_goldplus_post_acd_clk */
	0x8,		/* measure_only_apcs_goldplus_pre_acd_clk */
	0x4,		/* measure_only_apcs_l3_post_acd_clk */
	0x4,		/* measure_only_apcs_l3_pre_acd_clk */
	0x4,		/* measure_only_apcs_silver_post_acd_clk */
	0x4,		/* measure_only_apcs_silver_pre_acd_clk */
};

static struct clk_debug_mux apss_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x20100,
	.post_div_offset = 0x20104,
	.cbcr_offset = 0x20108,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = apss_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(apss_cc_debug_mux_sels),
	.pre_div_vals = apss_cc_debug_mux_pre_divs,
	.hw.init = &(struct clk_init_data){
		.name = "apss_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apss_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apss_cc_debug_mux_parent_names),
	},
};

static const char *const cam_cc_debug_mux_parent_names[] = {
	"cam_cc_bps_ahb_clk",
	"cam_cc_bps_areg_clk",
	"cam_cc_bps_axi_clk",
	"cam_cc_bps_clk",
	"cam_cc_camnoc_axi_clk",
	"cam_cc_camnoc_dcd_xo_clk",
	"cam_cc_cci_0_clk",
	"cam_cc_cci_1_clk",
	"cam_cc_core_ahb_clk",
	"cam_cc_cpas_ahb_clk",
	"cam_cc_csi0phytimer_clk",
	"cam_cc_csi1phytimer_clk",
	"cam_cc_csi2phytimer_clk",
	"cam_cc_csi3phytimer_clk",
	"cam_cc_csi4phytimer_clk",
	"cam_cc_csiphy0_clk",
	"cam_cc_csiphy1_clk",
	"cam_cc_csiphy2_clk",
	"cam_cc_csiphy3_clk",
	"cam_cc_csiphy4_clk",
	"cam_cc_icp_ahb_clk",
	"cam_cc_icp_clk",
	"cam_cc_ife_0_ahb_clk",
	"cam_cc_ife_0_areg_clk",
	"cam_cc_ife_0_axi_clk",
	"cam_cc_ife_0_clk",
	"cam_cc_ife_0_cphy_rx_clk",
	"cam_cc_ife_0_csid_clk",
	"cam_cc_ife_0_dsp_clk",
	"cam_cc_ife_1_ahb_clk",
	"cam_cc_ife_1_areg_clk",
	"cam_cc_ife_1_axi_clk",
	"cam_cc_ife_1_clk",
	"cam_cc_ife_1_cphy_rx_clk",
	"cam_cc_ife_1_csid_clk",
	"cam_cc_ife_1_dsp_clk",
	"cam_cc_ife_2_ahb_clk",
	"cam_cc_ife_2_areg_clk",
	"cam_cc_ife_2_axi_clk",
	"cam_cc_ife_2_clk",
	"cam_cc_ife_2_cphy_rx_clk",
	"cam_cc_ife_2_csid_clk",
	"cam_cc_ife_lite_ahb_clk",
	"cam_cc_ife_lite_axi_clk",
	"cam_cc_ife_lite_clk",
	"cam_cc_ife_lite_cphy_rx_clk",
	"cam_cc_ife_lite_csid_clk",
	"cam_cc_ipe_0_ahb_clk",
	"cam_cc_ipe_0_areg_clk",
	"cam_cc_ipe_0_axi_clk",
	"cam_cc_ipe_0_clk",
	"cam_cc_jpeg_clk",
	"cam_cc_mclk0_clk",
	"cam_cc_mclk1_clk",
	"cam_cc_mclk2_clk",
	"cam_cc_mclk3_clk",
	"cam_cc_mclk4_clk",
	"cam_cc_mclk5_clk",
	"cam_cc_qdss_debug_clk",
	"cam_cc_qdss_debug_xo_clk",
	"cam_cc_sleep_clk",
	"measure_only_cam_cc_gdsc_clk",
};

static int cam_cc_debug_mux_sels[] = {
	0x18,		/* cam_cc_bps_ahb_clk */
	0x17,		/* cam_cc_bps_areg_clk */
	0x16,		/* cam_cc_bps_axi_clk */
	0x14,		/* cam_cc_bps_clk */
	0x3C,		/* cam_cc_camnoc_axi_clk */
	0x3D,		/* cam_cc_camnoc_dcd_xo_clk */
	0x39,		/* cam_cc_cci_0_clk */
	0x3A,		/* cam_cc_cci_1_clk */
	0x40,		/* cam_cc_core_ahb_clk */
	0x3B,		/* cam_cc_cpas_ahb_clk */
	0x8,		/* cam_cc_csi0phytimer_clk */
	0xA,		/* cam_cc_csi1phytimer_clk */
	0xC,		/* cam_cc_csi2phytimer_clk */
	0xE,		/* cam_cc_csi3phytimer_clk */
	0x10,		/* cam_cc_csi4phytimer_clk */
	0x9,		/* cam_cc_csiphy0_clk */
	0xB,		/* cam_cc_csiphy1_clk */
	0xD,		/* cam_cc_csiphy2_clk */
	0xF,		/* cam_cc_csiphy3_clk */
	0x11,		/* cam_cc_csiphy4_clk */
	0x36,		/* cam_cc_icp_ahb_clk */
	0x35,		/* cam_cc_icp_clk */
	0x26,		/* cam_cc_ife_0_ahb_clk */
	0x1F,		/* cam_cc_ife_0_areg_clk */
	0x25,		/* cam_cc_ife_0_axi_clk */
	0x1E,		/* cam_cc_ife_0_clk */
	0x24,		/* cam_cc_ife_0_cphy_rx_clk */
	0x22,		/* cam_cc_ife_0_csid_clk */
	0x21,		/* cam_cc_ife_0_dsp_clk */
	0x2E,		/* cam_cc_ife_1_ahb_clk */
	0x29,		/* cam_cc_ife_1_areg_clk */
	0x2D,		/* cam_cc_ife_1_axi_clk */
	0x27,		/* cam_cc_ife_1_clk */
	0x2C,		/* cam_cc_ife_1_cphy_rx_clk */
	0x2B,		/* cam_cc_ife_1_csid_clk */
	0x2A,		/* cam_cc_ife_1_dsp_clk */
	0x54,		/* cam_cc_ife_2_ahb_clk */
	0x37,		/* cam_cc_ife_2_areg_clk */
	0x53,		/* cam_cc_ife_2_axi_clk */
	0x7,		/* cam_cc_ife_2_clk */
	0x52,		/* cam_cc_ife_2_cphy_rx_clk */
	0x51,		/* cam_cc_ife_2_csid_clk */
	0x32,		/* cam_cc_ife_lite_ahb_clk */
	0x49,		/* cam_cc_ife_lite_axi_clk */
	0x2F,		/* cam_cc_ife_lite_clk */
	0x31,		/* cam_cc_ife_lite_cphy_rx_clk */
	0x30,		/* cam_cc_ife_lite_csid_clk */
	0x1D,		/* cam_cc_ipe_0_ahb_clk */
	0x1C,		/* cam_cc_ipe_0_areg_clk */
	0x1B,		/* cam_cc_ipe_0_axi_clk */
	0x19,		/* cam_cc_ipe_0_clk */
	0x33,		/* cam_cc_jpeg_clk */
	0x1,		/* cam_cc_mclk0_clk */
	0x2,		/* cam_cc_mclk1_clk */
	0x3,		/* cam_cc_mclk2_clk */
	0x4,		/* cam_cc_mclk3_clk */
	0x5,		/* cam_cc_mclk4_clk */
	0x6,		/* cam_cc_mclk5_clk */
	0x3E,		/* cam_cc_qdss_debug_clk */
	0x3F,		/* cam_cc_qdss_debug_xo_clk */
	0x42,		/* cam_cc_sleep_clk */
	0x41,		/* measure_only_cam_cc_gdsc_clk */
};

static struct clk_debug_mux cam_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x16000,
	.post_div_offset = 0x16004,
	.cbcr_offset = 0x16008,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = cam_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(cam_cc_debug_mux_sels),
	.hw.init = &(struct clk_init_data){
		.name = "cam_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = cam_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(cam_cc_debug_mux_parent_names),
	},
};

static const char *const disp_cc_debug_mux_parent_names[] = {
	"disp_cc_mdss_ahb1_clk",
	"disp_cc_mdss_ahb_clk",
	"disp_cc_mdss_byte0_clk",
	"disp_cc_mdss_byte0_intf_clk",
	"disp_cc_mdss_byte1_clk",
	"disp_cc_mdss_byte1_intf_clk",
	"disp_cc_mdss_dptx0_aux_clk",
	"disp_cc_mdss_dptx0_crypto_clk",
	"disp_cc_mdss_dptx0_link_clk",
	"disp_cc_mdss_dptx0_link_intf_clk",
	"disp_cc_mdss_dptx0_pixel0_clk",
	"disp_cc_mdss_dptx0_pixel1_clk",
	"disp_cc_mdss_dptx0_usb_router_link_intf_clk",
	"disp_cc_mdss_dptx1_aux_clk",
	"disp_cc_mdss_dptx1_crypto_clk",
	"disp_cc_mdss_dptx1_link_clk",
	"disp_cc_mdss_dptx1_link_intf_clk",
	"disp_cc_mdss_dptx1_pixel0_clk",
	"disp_cc_mdss_dptx1_pixel1_clk",
	"disp_cc_mdss_dptx1_usb_router_link_intf_clk",
	"disp_cc_mdss_dptx2_aux_clk",
	"disp_cc_mdss_dptx2_crypto_clk",
	"disp_cc_mdss_dptx2_link_clk",
	"disp_cc_mdss_dptx2_link_intf_clk",
	"disp_cc_mdss_dptx2_pixel0_clk",
	"disp_cc_mdss_dptx2_pixel1_clk",
	"disp_cc_mdss_dptx3_aux_clk",
	"disp_cc_mdss_dptx3_crypto_clk",
	"disp_cc_mdss_dptx3_link_clk",
	"disp_cc_mdss_dptx3_link_intf_clk",
	"disp_cc_mdss_dptx3_pixel0_clk",
	"disp_cc_mdss_esc0_clk",
	"disp_cc_mdss_esc1_clk",
	"disp_cc_mdss_mdp1_clk",
	"disp_cc_mdss_mdp_clk",
	"disp_cc_mdss_mdp_lut1_clk",
	"disp_cc_mdss_mdp_lut_clk",
	"disp_cc_mdss_non_gdsc_ahb_clk",
	"disp_cc_mdss_pclk0_clk",
	"disp_cc_mdss_pclk1_clk",
	"disp_cc_mdss_rot1_clk",
	"disp_cc_mdss_rot_clk",
	"disp_cc_mdss_rscc_ahb_clk",
	"disp_cc_mdss_rscc_vsync_clk",
	"disp_cc_mdss_vsync1_clk",
	"disp_cc_mdss_vsync_clk",
	"disp_cc_sleep_clk",
	"measure_only_disp_cc_xo_clk",
};

static int disp_cc_debug_mux_sels[] = {
	0x39,		/* disp_cc_mdss_ahb1_clk */
	0x34,		/* disp_cc_mdss_ahb_clk */
	0x15,		/* disp_cc_mdss_byte0_clk */
	0x16,		/* disp_cc_mdss_byte0_intf_clk */
	0x17,		/* disp_cc_mdss_byte1_clk */
	0x18,		/* disp_cc_mdss_byte1_intf_clk */
	0x21,		/* disp_cc_mdss_dptx0_aux_clk */
	0x1E,		/* disp_cc_mdss_dptx0_crypto_clk */
	0x1B,		/* disp_cc_mdss_dptx0_link_clk */
	0x1D,		/* disp_cc_mdss_dptx0_link_intf_clk */
	0x1F,		/* disp_cc_mdss_dptx0_pixel0_clk */
	0x20,		/* disp_cc_mdss_dptx0_pixel1_clk */
	0x1C,		/* disp_cc_mdss_dptx0_usb_router_link_intf_clk */
	0x28,		/* disp_cc_mdss_dptx1_aux_clk */
	0x27,		/* disp_cc_mdss_dptx1_crypto_clk */
	0x24,		/* disp_cc_mdss_dptx1_link_clk */
	0x26,		/* disp_cc_mdss_dptx1_link_intf_clk */
	0x22,		/* disp_cc_mdss_dptx1_pixel0_clk */
	0x23,		/* disp_cc_mdss_dptx1_pixel1_clk */
	0x25,		/* disp_cc_mdss_dptx1_usb_router_link_intf_clk */
	0x2E,		/* disp_cc_mdss_dptx2_aux_clk */
	0x2D,		/* disp_cc_mdss_dptx2_crypto_clk */
	0x2B,		/* disp_cc_mdss_dptx2_link_clk */
	0x2C,		/* disp_cc_mdss_dptx2_link_intf_clk */
	0x29,		/* disp_cc_mdss_dptx2_pixel0_clk */
	0x2A,		/* disp_cc_mdss_dptx2_pixel1_clk */
	0x32,		/* disp_cc_mdss_dptx3_aux_clk */
	0x33,		/* disp_cc_mdss_dptx3_crypto_clk */
	0x30,		/* disp_cc_mdss_dptx3_link_clk */
	0x31,		/* disp_cc_mdss_dptx3_link_intf_clk */
	0x2F,		/* disp_cc_mdss_dptx3_pixel0_clk */
	0x19,		/* disp_cc_mdss_esc0_clk */
	0x1A,		/* disp_cc_mdss_esc1_clk */
	0x35,		/* disp_cc_mdss_mdp1_clk */
	0x11,		/* disp_cc_mdss_mdp_clk */
	0x37,		/* disp_cc_mdss_mdp_lut1_clk */
	0x13,		/* disp_cc_mdss_mdp_lut_clk */
	0x3A,		/* disp_cc_mdss_non_gdsc_ahb_clk */
	0xF,		/* disp_cc_mdss_pclk0_clk */
	0x10,		/* disp_cc_mdss_pclk1_clk */
	0x36,		/* disp_cc_mdss_rot1_clk */
	0x12,		/* disp_cc_mdss_rot_clk */
	0x3C,		/* disp_cc_mdss_rscc_ahb_clk */
	0x3B,		/* disp_cc_mdss_rscc_vsync_clk */
	0x38,		/* disp_cc_mdss_vsync1_clk */
	0x14,		/* disp_cc_mdss_vsync_clk */
	0x45,		/* disp_cc_sleep_clk */
	0x44,		/* measure_only_disp_cc_xo_clk */
};

static struct clk_debug_mux disp_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x11000,
	.post_div_offset = 0xD008,
	.cbcr_offset = 0xD00C,
	.src_sel_mask = 0x1FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = disp_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(disp_cc_debug_mux_sels),
	.hw.init = &(struct clk_init_data){
		.name = "disp_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = disp_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(disp_cc_debug_mux_parent_names),
	},
};

static const char *const gcc_debug_mux_parent_names[] = {
	"apss_cc_debug_mux",
	"cam_cc_debug_mux",
	"disp_cc_debug_mux",
	"gcc_aggre_ufs_phy_axi_clk",
	"gcc_aggre_usb3_prim_axi_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_camera_hf_axi_clk",
	"gcc_camera_sf_axi_clk",
	"gcc_cfg_noc_usb3_prim_axi_clk",
	"gcc_ddrss_gpu_axi_clk",
	"gcc_disp_hf_axi_clk",
	"gcc_disp_sf_axi_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_gpu_gpll0_clk_src",
	"gcc_gpu_gpll0_div_clk_src",
	"gcc_gpu_memnoc_gfx_clk",
	"gcc_gpu_snoc_dvm_gfx_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_qmip_camera_nrt_ahb_clk",
	"gcc_qmip_camera_rt_ahb_clk",
	"gcc_qmip_disp_ahb_clk",
	"gcc_qmip_gpu_ahb_clk",
	"gcc_qmip_video_cv_cpu_ahb_clk",
	"gcc_qmip_video_cvp_ahb_clk",
	"gcc_qmip_video_v_cpu_ahb_clk",
	"gcc_qmip_video_vcodec_ahb_clk",
	"gcc_qupv3_wrap0_core_2x_clk",
	"gcc_qupv3_wrap0_core_clk",
	"gcc_qupv3_wrap0_s0_clk",
	"gcc_qupv3_wrap0_s1_clk",
	"gcc_qupv3_wrap0_s3_clk",
	"gcc_qupv3_wrap0_s4_clk",
	"gcc_qupv3_wrap0_s5_clk",
	"gcc_qupv3_wrap0_s6_clk",
	"gcc_qupv3_wrap0_s7_clk",
	"gcc_qupv3_wrap1_core_2x_clk",
	"gcc_qupv3_wrap1_core_clk",
	"gcc_qupv3_wrap1_s0_clk",
	"gcc_qupv3_wrap1_s1_clk",
	"gcc_qupv3_wrap1_s2_clk",
	"gcc_qupv3_wrap1_s3_clk",
	"gcc_qupv3_wrap1_s4_clk",
	"gcc_qupv3_wrap1_s5_clk",
	"gcc_qupv3_wrap1_s6_clk",
	"gcc_qupv3_wrap1_s7_clk",
	"gcc_qupv3_wrap_1_m_ahb_clk",
	"gcc_qupv3_wrap_1_s_ahb_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_ufs_phy_ahb_clk",
	"gcc_ufs_phy_axi_clk",
	"gcc_ufs_phy_ice_core_clk",
	"gcc_ufs_phy_phy_aux_clk",
	"gcc_ufs_phy_rx_symbol_0_clk",
	"gcc_ufs_phy_rx_symbol_1_clk",
	"gcc_ufs_phy_tx_symbol_0_clk",
	"gcc_ufs_phy_unipro_core_clk",
	"gcc_usb30_prim_master_clk",
	"gcc_usb30_prim_mock_utmi_clk",
	"gcc_usb30_prim_sleep_clk",
	"gcc_usb3_prim_phy_aux_clk",
	"gcc_usb3_prim_phy_com_aux_clk",
	"gcc_usb3_prim_phy_pipe_clk",
	"gcc_video_axi0_clk",
	"gcc_video_axi1_clk",
	"gpu_cc_debug_mux",
	"mc_cc_debug_mux",
	"measure_only_cnoc_clk",
	"measure_only_gcc_aggre_noc_pcie_0_axi_clk",
	"measure_only_gcc_anoc_pcie_north_at_clk",
	"measure_only_gcc_aoss_at_clk",
	"measure_only_gcc_apss_qdss_apb_clk",
	"measure_only_gcc_apss_qdss_tsctr_clk",
	"measure_only_gcc_at_clk",
	"measure_only_gcc_camera_ahb_clk",
	"measure_only_gcc_camera_xo_clk",
	"measure_only_gcc_cfg_noc_pcie_anoc_ahb_clk",
	"measure_only_gcc_cnoc_qdss_stm_clk",
	"measure_only_gcc_config_noc_at_clk",
	"measure_only_gcc_cpuss_at_clk",
	"measure_only_gcc_cpuss_trig_clk",
	"measure_only_gcc_ddrss_at_clk",
	"measure_only_gcc_ddrss_pcie_sf_tbu_clk",
	"measure_only_gcc_disp_ahb_clk",
	"measure_only_gcc_disp_xo_clk",
	"measure_only_gcc_gpu_at_clk",
	"measure_only_gcc_gpu_cfg_ahb_clk",
	"measure_only_gcc_gpu_trig_clk",
	"measure_only_gcc_ipa_tpdm_at_clk",
	"measure_only_gcc_lpass_at_clk",
	"measure_only_gcc_lpass_trig_clk",
	"measure_only_gcc_mmnoc_hf_at_clk",
	"measure_only_gcc_mmnoc_sf_at_clk",
	"measure_only_gcc_mmss_center_at_clk",
	"measure_only_gcc_mmss_lpass_at_clk",
	"measure_only_gcc_mmss_trig_clk",
	"measure_only_gcc_mss_at_clk",
	"measure_only_gcc_mss_trig_clk",
	"measure_only_gcc_north_at_clk",
	"measure_only_gcc_north_trig_clk",
	"measure_only_gcc_pcie_0_aux_clk",
	"measure_only_gcc_pcie_0_cfg_ahb_clk",
	"measure_only_gcc_pcie_0_mstr_axi_clk",
	"measure_only_gcc_pcie_0_phy_rchng_clk",
	"measure_only_gcc_pcie_0_pipe_clk",
	"measure_only_gcc_pcie_0_slv_axi_clk",
	"measure_only_gcc_pcie_0_slv_q2a_axi_clk",
	"measure_only_gcc_phy_at_clk",
	"measure_only_gcc_pimem_at_clk",
	"measure_only_gcc_qdss_center_at_clk",
	"measure_only_gcc_qdss_cfg_ahb_clk",
	"measure_only_gcc_qdss_dap_ahb_clk",
	"measure_only_gcc_qdss_dap_clk",
	"measure_only_gcc_qdss_etr_ddr_clk",
	"measure_only_gcc_qdss_etr_usb_clk",
	"measure_only_gcc_qdss_stm_clk",
	"measure_only_gcc_qdss_traceclkin_clk",
	"measure_only_gcc_qdss_trig_clk",
	"measure_only_gcc_qdss_tsctr_clk",
	"measure_only_gcc_qdss_usb_prim_clk",
	"measure_only_gcc_qdss_xo_clk",
	"measure_only_gcc_qmip_pcie_ahb_clk",
	"measure_only_gcc_qupv3_wrap0_s2_clk",
	"measure_only_gcc_qupv3_wrap_0_m_ahb_clk",
	"measure_only_gcc_qupv3_wrap_0_s_ahb_clk",
	"measure_only_gcc_sdcc2_at_clk",
	"measure_only_gcc_south_at_clk",
	"measure_only_gcc_sys_noc_at_clk",
	"measure_only_gcc_tme_at_clk",
	"measure_only_gcc_tme_trig_clk",
	"measure_only_gcc_turing_at_clk",
	"measure_only_gcc_turing_trig_clk",
	"measure_only_gcc_video_ahb_clk",
	"measure_only_gcc_video_xo_clk",
	"measure_only_gcc_west_at_clk",
	"measure_only_gcc_wpss_at_clk",
	"measure_only_gcc_wpss_m_at_clk",
	"measure_only_gcc_wpss_trig_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_memnoc_clk",
	"measure_only_pcie_0_pipe_clk",
	"measure_only_snoc_clk",
	"measure_only_ufs_phy_rx_symbol_0_clk",
	"measure_only_ufs_phy_rx_symbol_1_clk",
	"measure_only_ufs_phy_tx_symbol_0_clk",
	"measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
	"video_cc_debug_mux",
};

static int gcc_debug_mux_sels[] = {
	0x117,		/* apss_cc_debug_mux */
	0x6B,		/* cam_cc_debug_mux */
	0x71,		/* disp_cc_debug_mux */
	0x40,		/* gcc_aggre_ufs_phy_axi_clk */
	0x3F,		/* gcc_aggre_usb3_prim_axi_clk */
	0xD9,		/* gcc_boot_rom_ahb_clk */
	0x66,		/* gcc_camera_hf_axi_clk */
	0x68,		/* gcc_camera_sf_axi_clk */
	0x20,		/* gcc_cfg_noc_usb3_prim_axi_clk */
	0xF5,		/* gcc_ddrss_gpu_axi_clk */
	0x6E,		/* gcc_disp_hf_axi_clk */
	0x6F,		/* gcc_disp_sf_axi_clk */
	0x122,		/* gcc_gp1_clk */
	0x123,		/* gcc_gp2_clk */
	0x124,		/* gcc_gp3_clk */
	0x172,		/* gcc_gpu_gpll0_clk_src */
	0x173,		/* gcc_gpu_gpll0_div_clk_src */
	0x16F,		/* gcc_gpu_memnoc_gfx_clk */
	0x171,		/* gcc_gpu_snoc_dvm_gfx_clk */
	0xCB,		/* gcc_pdm2_clk */
	0xC9,		/* gcc_pdm_ahb_clk */
	0xCA,		/* gcc_pdm_xo4_clk */
	0x64,		/* gcc_qmip_camera_nrt_ahb_clk */
	0x65,		/* gcc_qmip_camera_rt_ahb_clk */
	0x6D,		/* gcc_qmip_disp_ahb_clk */
	0x16C,		/* gcc_qmip_gpu_ahb_clk */
	0x76,		/* gcc_qmip_video_cv_cpu_ahb_clk */
	0x73,		/* gcc_qmip_video_cvp_ahb_clk */
	0x75,		/* gcc_qmip_video_v_cpu_ahb_clk */
	0x74,		/* gcc_qmip_video_vcodec_ahb_clk */
	0xAA,		/* gcc_qupv3_wrap0_core_2x_clk */
	0xA9,		/* gcc_qupv3_wrap0_core_clk */
	0xAB,		/* gcc_qupv3_wrap0_s0_clk */
	0xAC,		/* gcc_qupv3_wrap0_s1_clk */
	0xAE,		/* gcc_qupv3_wrap0_s3_clk */
	0xAF,		/* gcc_qupv3_wrap0_s4_clk */
	0xB0,		/* gcc_qupv3_wrap0_s5_clk */
	0xB1,		/* gcc_qupv3_wrap0_s6_clk */
	0xB2,		/* gcc_qupv3_wrap0_s7_clk */
	0xB6,		/* gcc_qupv3_wrap1_core_2x_clk */
	0xB5,		/* gcc_qupv3_wrap1_core_clk */
	0xB7,		/* gcc_qupv3_wrap1_s0_clk */
	0xB8,		/* gcc_qupv3_wrap1_s1_clk */
	0xB9,		/* gcc_qupv3_wrap1_s2_clk */
	0xBA,		/* gcc_qupv3_wrap1_s3_clk */
	0xBB,		/* gcc_qupv3_wrap1_s4_clk */
	0xBC,		/* gcc_qupv3_wrap1_s5_clk */
	0xBD,		/* gcc_qupv3_wrap1_s6_clk */
	0x1A3,		/* gcc_qupv3_wrap1_s7_clk */
	0xB3,		/* gcc_qupv3_wrap_1_m_ahb_clk */
	0xB4,		/* gcc_qupv3_wrap_1_s_ahb_clk */
	0xA2,		/* gcc_sdcc2_ahb_clk */
	0xA1,		/* gcc_sdcc2_apps_clk */
	0x13B,		/* gcc_ufs_phy_ahb_clk */
	0x13A,		/* gcc_ufs_phy_axi_clk */
	0x141,		/* gcc_ufs_phy_ice_core_clk */
	0x142,		/* gcc_ufs_phy_phy_aux_clk */
	0x13D,		/* gcc_ufs_phy_rx_symbol_0_clk */
	0x143,		/* gcc_ufs_phy_rx_symbol_1_clk */
	0x13C,		/* gcc_ufs_phy_tx_symbol_0_clk */
	0x140,		/* gcc_ufs_phy_unipro_core_clk */
	0x94,		/* gcc_usb30_prim_master_clk */
	0x96,		/* gcc_usb30_prim_mock_utmi_clk */
	0x95,		/* gcc_usb30_prim_sleep_clk */
	0x97,		/* gcc_usb3_prim_phy_aux_clk */
	0x98,		/* gcc_usb3_prim_phy_com_aux_clk */
	0x99,		/* gcc_usb3_prim_phy_pipe_clk */
	0x77,		/* gcc_video_axi0_clk */
	0x78,		/* gcc_video_axi1_clk */
	0x16E,		/* gpu_cc_debug_mux */
	0x100,		/* mc_cc_debug_mux or ddrss_gcc_debug_clk */
	0x19,		/* measure_only_cnoc_clk */
	0x3D,		/* measure_only_gcc_aggre_noc_pcie_0_axi_clk */
	0x4A,		/* measure_only_gcc_anoc_pcie_north_at_clk */
	0xDE,		/* measure_only_gcc_aoss_at_clk */
	0x116,		/* measure_only_gcc_apss_qdss_apb_clk */
	0x115,		/* measure_only_gcc_apss_qdss_tsctr_clk */
	0xE9,		/* measure_only_gcc_at_clk */
	0x63,		/* measure_only_gcc_camera_ahb_clk */
	0x6A,		/* measure_only_gcc_camera_xo_clk */
	0x2D,		/* measure_only_gcc_cfg_noc_pcie_anoc_ahb_clk */
	0x1F,		/* measure_only_gcc_cnoc_qdss_stm_clk */
	0x2C,		/* measure_only_gcc_config_noc_at_clk */
	0x113,		/* measure_only_gcc_cpuss_at_clk */
	0x112,		/* measure_only_gcc_cpuss_trig_clk */
	0xFC,		/* measure_only_gcc_ddrss_at_clk */
	0xF6,		/* measure_only_gcc_ddrss_pcie_sf_tbu_clk */
	0x6C,		/* measure_only_gcc_disp_ahb_clk */
	0x70,		/* measure_only_gcc_disp_xo_clk */
	0x16D,		/* measure_only_gcc_gpu_at_clk */
	0x16B,		/* measure_only_gcc_gpu_cfg_ahb_clk */
	0x170,		/* measure_only_gcc_gpu_trig_clk */
	0x1A8,		/* measure_only_gcc_ipa_tpdm_at_clk */
	0x106,		/* measure_only_gcc_lpass_at_clk */
	0x105,		/* measure_only_gcc_lpass_trig_clk */
	0x57,		/* measure_only_gcc_mmnoc_hf_at_clk */
	0x1BB,		/* measure_only_gcc_mmnoc_sf_at_clk */
	0x5F,		/* measure_only_gcc_mmss_center_at_clk */
	0x1BA,		/* measure_only_gcc_mmss_lpass_at_clk */
	0x61,		/* measure_only_gcc_mmss_trig_clk */
	0x162,		/* measure_only_gcc_mss_at_clk */
	0x161,		/* measure_only_gcc_mss_trig_clk */
	0x83,		/* measure_only_gcc_north_at_clk */
	0x1A2,		/* measure_only_gcc_north_trig_clk */
	0x12A,		/* measure_only_gcc_pcie_0_aux_clk */
	0x129,		/* measure_only_gcc_pcie_0_cfg_ahb_clk */
	0x128,		/* measure_only_gcc_pcie_0_mstr_axi_clk */
	0x12C,		/* measure_only_gcc_pcie_0_phy_rchng_clk */
	0x12B,		/* measure_only_gcc_pcie_0_pipe_clk */
	0x127,		/* measure_only_gcc_pcie_0_slv_axi_clk */
	0x126,		/* measure_only_gcc_pcie_0_slv_q2a_axi_clk */
	0x84,		/* measure_only_gcc_phy_at_clk */
	0x7D,		/* measure_only_gcc_pimem_at_clk */
	0x80,		/* measure_only_gcc_qdss_center_at_clk */
	0x7F,		/* measure_only_gcc_qdss_cfg_ahb_clk */
	0x7E,		/* measure_only_gcc_qdss_dap_ahb_clk */
	0x8B,		/* measure_only_gcc_qdss_dap_clk */
	0x86,		/* measure_only_gcc_qdss_etr_ddr_clk */
	0x85,		/* measure_only_gcc_qdss_etr_usb_clk */
	0x87,		/* measure_only_gcc_qdss_stm_clk */
	0x88,		/* measure_only_gcc_qdss_traceclkin_clk */
	0x8A,		/* measure_only_gcc_qdss_trig_clk */
	0x89,		/* measure_only_gcc_qdss_tsctr_clk */
	0x93,		/* measure_only_gcc_qdss_usb_prim_clk */
	0x92,		/* measure_only_gcc_qdss_xo_clk */
	0x125,		/* measure_only_gcc_qmip_pcie_ahb_clk */
	0xAD,		/* measure_only_gcc_qupv3_wrap0_s2_clk */
	0xA7,		/* measure_only_gcc_qupv3_wrap_0_m_ahb_clk */
	0xA8,		/* measure_only_gcc_qupv3_wrap_0_s_ahb_clk */
	0xA3,		/* measure_only_gcc_sdcc2_at_clk */
	0x81,		/* measure_only_gcc_south_at_clk */
	0x10,		/* measure_only_gcc_sys_noc_at_clk */
	0xD4,		/* measure_only_gcc_tme_at_clk */
	0xD3,		/* measure_only_gcc_tme_trig_clk */
	0x10E,		/* measure_only_gcc_turing_at_clk */
	0x10F,		/* measure_only_gcc_turing_trig_clk */
	0x72,		/* measure_only_gcc_video_ahb_clk */
	0x79,		/* measure_only_gcc_video_xo_clk */
	0x82,		/* measure_only_gcc_west_at_clk */
	0x1AE,		/* measure_only_gcc_wpss_at_clk */
	0x1AD,		/* measure_only_gcc_wpss_m_at_clk */
	0x1AF,		/* measure_only_gcc_wpss_trig_clk */
	0x158,		/* measure_only_ipa_2x_clk */
	0xFB,		/* measure_only_memnoc_clk */
	0x12D,		/* measure_only_pcie_0_pipe_clk */
	0xC,		/* measure_only_snoc_clk */
	0x13F,		/* measure_only_ufs_phy_rx_symbol_0_clk */
	0x144,		/* measure_only_ufs_phy_rx_symbol_1_clk */
	0x13E,		/* measure_only_ufs_phy_tx_symbol_0_clk */
	0x9D,		/* measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk */
	0x7A,		/* video_cc_debug_mux */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x72000,
	.post_div_offset = 0x72004,
	.cbcr_offset = 0x72008,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 2,
	.mux_sels = gcc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(gcc_debug_mux_sels),
	.hw.init = &(struct clk_init_data){
		.name = "gcc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gcc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gcc_debug_mux_parent_names),
	},
};

static const char *const gpu_cc_debug_mux_parent_names[] = {
	"gpu_cc_ahb_clk",
	"gpu_cc_crc_ahb_clk",
	"gpu_cc_cx_ff_clk",
	"gpu_cc_cx_gmu_clk",
	"gpu_cc_cx_snoc_dvm_clk",
	"gpu_cc_cxo_aon_clk",
	"gpu_cc_cxo_clk",
	"gpu_cc_demet_clk",
	"gpu_cc_freq_measure_clk",
	"gpu_cc_hub_aon_clk",
	"gpu_cc_hub_cx_int_clk",
	"gpu_cc_memnoc_gfx_clk",
	"gpu_cc_mnd1x_0_gfx3d_clk",
	"gpu_cc_mnd1x_1_gfx3d_clk",
	"gpu_cc_sleep_clk",
	"measure_only_gpu_cc_cx_gfx3d_clk",
	"measure_only_gpu_cc_cx_gfx3d_slv_clk",
	"measure_only_gpu_cc_gx_gfx3d_clk",
};

static int gpu_cc_debug_mux_sels[] = {
	0x16,		/* gpu_cc_ahb_clk */
	0x17,		/* gpu_cc_crc_ahb_clk */
	0x21,		/* gpu_cc_cx_ff_clk */
	0x1E,		/* gpu_cc_cx_gmu_clk */
	0x1B,		/* gpu_cc_cx_snoc_dvm_clk */
	0xB,		/* gpu_cc_cxo_aon_clk */
	0x1F,		/* gpu_cc_cxo_clk */
	0xD,		/* gpu_cc_demet_clk */
	0xC,		/* gpu_cc_freq_measure_clk */
	0x2F,		/* gpu_cc_hub_aon_clk */
	0x20,		/* gpu_cc_hub_cx_int_clk */
	0x22,		/* gpu_cc_memnoc_gfx_clk */
	0x29,		/* gpu_cc_mnd1x_0_gfx3d_clk */
	0x2A,		/* gpu_cc_mnd1x_1_gfx3d_clk */
	0x1C,		/* gpu_cc_sleep_clk */
	0x25,		/* measure_only_gpu_cc_cx_gfx3d_clk */
	0x26,		/* measure_only_gpu_cc_cx_gfx3d_slv_clk */
	0xE,		/* measure_only_gpu_cc_gx_gfx3d_clk */
};

static struct clk_debug_mux gpu_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x9564,
	.post_div_offset = 0x9270,
	.cbcr_offset = 0x9274,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 2,
	.mux_sels = gpu_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(gpu_cc_debug_mux_sels),
	.hw.init = &(struct clk_init_data){
		.name = "gpu_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gpu_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gpu_cc_debug_mux_parent_names),
	},
};

static const char *const video_cc_debug_mux_parent_names[] = {
	"measure_only_video_cc_ahb_clk",
	"measure_only_video_cc_xo_clk",
	"video_cc_mvs0_clk",
	"video_cc_mvs0c_clk",
	"video_cc_mvs1_clk",
	"video_cc_mvs1_div2_clk",
	"video_cc_mvs1c_clk",
	"video_cc_sleep_clk",
};

static int video_cc_debug_mux_sels[] = {
	0x7,		/* measure_only_video_cc_ahb_clk */
	0xB,		/* measure_only_video_cc_xo_clk */
	0x3,		/* video_cc_mvs0_clk */
	0x1,		/* video_cc_mvs0c_clk */
	0x5,		/* video_cc_mvs1_clk */
	0x8,		/* video_cc_mvs1_div2_clk */
	0x9,		/* video_cc_mvs1c_clk */
	0xC,		/* video_cc_sleep_clk */
};

static struct clk_debug_mux video_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x9A4C,
	.post_div_offset = 0x8364,
	.cbcr_offset = 0x8384,
	.src_sel_mask = 0x3F,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 3,
	.mux_sels = video_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(video_cc_debug_mux_sels),
	.hw.init = &(struct clk_init_data){
		.name = "video_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = video_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(video_cc_debug_mux_parent_names),
	},
};

static const char *const mc_cc_debug_mux_parent_names[] = {
	"measure_only_mccc_clk",
};

static struct clk_debug_mux mc_cc_debug_mux = {
	.period_offset = 0x50,
	.hw.init = &(struct clk_init_data){
		.name = "mc_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = mc_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(mc_cc_debug_mux_parent_names),
	},
};

static struct mux_regmap_names mux_list[] = {
	{ .mux = &apss_cc_debug_mux, .regmap_name = "qcom,apsscc" },
	{ .mux = &cam_cc_debug_mux, .regmap_name = "qcom,camcc" },
	{ .mux = &disp_cc_debug_mux, .regmap_name = "qcom,dispcc" },
	{ .mux = &gpu_cc_debug_mux, .regmap_name = "qcom,gpucc" },
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
	{ .mux = &video_cc_debug_mux, .regmap_name = "qcom,videocc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
};

static struct clk_dummy measure_only_apcs_gold_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_gold_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_gold_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_gold_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_goldplus_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_goldplus_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_goldplus_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_goldplus_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_l3_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_l3_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_l3_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_l3_pre_acd_clk",
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

static struct clk_dummy measure_only_apcs_silver_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_silver_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cam_cc_gdsc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_cam_cc_gdsc_clk",
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

static struct clk_dummy measure_only_disp_cc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_disp_cc_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_aggre_noc_pcie_0_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_aggre_noc_pcie_0_axi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_anoc_pcie_north_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_anoc_pcie_north_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_aoss_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_aoss_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_apss_qdss_apb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_apss_qdss_apb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_apss_qdss_tsctr_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_apss_qdss_tsctr_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_camera_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_camera_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_camera_xo_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_camera_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_cfg_noc_pcie_anoc_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_cfg_noc_pcie_anoc_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_cnoc_qdss_stm_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_cnoc_qdss_stm_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_config_noc_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_config_noc_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_cpuss_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_cpuss_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_cpuss_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_cpuss_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_ddrss_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_ddrss_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_ddrss_pcie_sf_tbu_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_ddrss_pcie_sf_tbu_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_disp_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_disp_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_disp_xo_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_disp_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_gpu_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_gpu_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_gpu_cfg_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_gpu_cfg_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_gpu_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_gpu_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_ipa_tpdm_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_ipa_tpdm_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_lpass_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_lpass_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_lpass_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_lpass_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_mmnoc_hf_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mmnoc_hf_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_mmnoc_sf_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mmnoc_sf_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_mmss_center_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mmss_center_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_mmss_lpass_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mmss_lpass_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_mmss_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mmss_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_mss_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mss_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_mss_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mss_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_north_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_north_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_north_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_north_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_0_aux_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_0_aux_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_0_cfg_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_0_cfg_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_0_mstr_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_0_mstr_axi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_0_phy_rchng_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_0_phy_rchng_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_0_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_0_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_0_slv_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_0_slv_axi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_0_slv_q2a_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_0_slv_q2a_axi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_phy_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_phy_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pimem_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pimem_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_center_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_center_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_cfg_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_cfg_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_dap_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_dap_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_dap_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_dap_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_etr_ddr_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_etr_ddr_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_etr_usb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_etr_usb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_stm_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_stm_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_traceclkin_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_traceclkin_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_tsctr_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_tsctr_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_usb_prim_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_usb_prim_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qdss_xo_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qmip_pcie_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qmip_pcie_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qupv3_wrap0_s2_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qupv3_wrap0_s2_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qupv3_wrap_0_m_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qupv3_wrap_0_m_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_qupv3_wrap_0_s_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qupv3_wrap_0_s_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_sdcc2_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_sdcc2_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_south_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_south_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_sys_noc_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_sys_noc_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_tme_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_tme_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_tme_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_tme_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_turing_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_turing_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_turing_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_turing_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_video_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_video_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_video_xo_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_video_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_west_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_west_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_wpss_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_wpss_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_wpss_m_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_wpss_m_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_wpss_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_wpss_trig_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_cx_gfx3d_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gpu_cc_cx_gfx3d_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_cx_gfx3d_slv_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gpu_cc_cx_gfx3d_slv_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_gx_gfx3d_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gpu_cc_gx_gfx3d_clk",
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

static struct clk_dummy measure_only_mccc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_mccc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_memnoc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_memnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcie_0_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pcie_0_pipe_clk",
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

static struct clk_dummy measure_only_ufs_phy_rx_symbol_0_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_ufs_phy_rx_symbol_0_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ufs_phy_rx_symbol_1_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_ufs_phy_rx_symbol_1_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ufs_phy_tx_symbol_0_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_ufs_phy_tx_symbol_0_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_video_cc_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_video_cc_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_video_cc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_video_cc_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_diwali_hws[] = {
	&measure_only_apcs_gold_post_acd_clk.hw,
	&measure_only_apcs_gold_pre_acd_clk.hw,
	&measure_only_apcs_goldplus_post_acd_clk.hw,
	&measure_only_apcs_goldplus_pre_acd_clk.hw,
	&measure_only_apcs_l3_post_acd_clk.hw,
	&measure_only_apcs_l3_pre_acd_clk.hw,
	&measure_only_apcs_silver_post_acd_clk.hw,
	&measure_only_apcs_silver_pre_acd_clk.hw,
	&measure_only_cam_cc_gdsc_clk.hw,
	&measure_only_cnoc_clk.hw,
	&measure_only_disp_cc_xo_clk.hw,
	&measure_only_gcc_aggre_noc_pcie_0_axi_clk.hw,
	&measure_only_gcc_anoc_pcie_north_at_clk.hw,
	&measure_only_gcc_aoss_at_clk.hw,
	&measure_only_gcc_apss_qdss_apb_clk.hw,
	&measure_only_gcc_apss_qdss_tsctr_clk.hw,
	&measure_only_gcc_at_clk.hw,
	&measure_only_gcc_camera_ahb_clk.hw,
	&measure_only_gcc_camera_xo_clk.hw,
	&measure_only_gcc_cfg_noc_pcie_anoc_ahb_clk.hw,
	&measure_only_gcc_cnoc_qdss_stm_clk.hw,
	&measure_only_gcc_config_noc_at_clk.hw,
	&measure_only_gcc_cpuss_at_clk.hw,
	&measure_only_gcc_cpuss_trig_clk.hw,
	&measure_only_gcc_ddrss_at_clk.hw,
	&measure_only_gcc_ddrss_pcie_sf_tbu_clk.hw,
	&measure_only_gcc_disp_ahb_clk.hw,
	&measure_only_gcc_disp_xo_clk.hw,
	&measure_only_gcc_gpu_at_clk.hw,
	&measure_only_gcc_gpu_cfg_ahb_clk.hw,
	&measure_only_gcc_gpu_trig_clk.hw,
	&measure_only_gcc_ipa_tpdm_at_clk.hw,
	&measure_only_gcc_lpass_at_clk.hw,
	&measure_only_gcc_lpass_trig_clk.hw,
	&measure_only_gcc_mmnoc_hf_at_clk.hw,
	&measure_only_gcc_mmnoc_sf_at_clk.hw,
	&measure_only_gcc_mmss_center_at_clk.hw,
	&measure_only_gcc_mmss_lpass_at_clk.hw,
	&measure_only_gcc_mmss_trig_clk.hw,
	&measure_only_gcc_mss_at_clk.hw,
	&measure_only_gcc_mss_trig_clk.hw,
	&measure_only_gcc_north_at_clk.hw,
	&measure_only_gcc_north_trig_clk.hw,
	&measure_only_gcc_pcie_0_aux_clk.hw,
	&measure_only_gcc_pcie_0_cfg_ahb_clk.hw,
	&measure_only_gcc_pcie_0_mstr_axi_clk.hw,
	&measure_only_gcc_pcie_0_phy_rchng_clk.hw,
	&measure_only_gcc_pcie_0_pipe_clk.hw,
	&measure_only_gcc_pcie_0_slv_axi_clk.hw,
	&measure_only_gcc_pcie_0_slv_q2a_axi_clk.hw,
	&measure_only_gcc_phy_at_clk.hw,
	&measure_only_gcc_pimem_at_clk.hw,
	&measure_only_gcc_qdss_center_at_clk.hw,
	&measure_only_gcc_qdss_cfg_ahb_clk.hw,
	&measure_only_gcc_qdss_dap_ahb_clk.hw,
	&measure_only_gcc_qdss_dap_clk.hw,
	&measure_only_gcc_qdss_etr_ddr_clk.hw,
	&measure_only_gcc_qdss_etr_usb_clk.hw,
	&measure_only_gcc_qdss_stm_clk.hw,
	&measure_only_gcc_qdss_traceclkin_clk.hw,
	&measure_only_gcc_qdss_trig_clk.hw,
	&measure_only_gcc_qdss_tsctr_clk.hw,
	&measure_only_gcc_qdss_usb_prim_clk.hw,
	&measure_only_gcc_qdss_xo_clk.hw,
	&measure_only_gcc_qmip_pcie_ahb_clk.hw,
	&measure_only_gcc_qupv3_wrap0_s2_clk.hw,
	&measure_only_gcc_qupv3_wrap_0_m_ahb_clk.hw,
	&measure_only_gcc_qupv3_wrap_0_s_ahb_clk.hw,
	&measure_only_gcc_sdcc2_at_clk.hw,
	&measure_only_gcc_south_at_clk.hw,
	&measure_only_gcc_sys_noc_at_clk.hw,
	&measure_only_gcc_tme_at_clk.hw,
	&measure_only_gcc_tme_trig_clk.hw,
	&measure_only_gcc_turing_at_clk.hw,
	&measure_only_gcc_turing_trig_clk.hw,
	&measure_only_gcc_video_ahb_clk.hw,
	&measure_only_gcc_video_xo_clk.hw,
	&measure_only_gcc_west_at_clk.hw,
	&measure_only_gcc_wpss_at_clk.hw,
	&measure_only_gcc_wpss_m_at_clk.hw,
	&measure_only_gcc_wpss_trig_clk.hw,
	&measure_only_gpu_cc_cx_gfx3d_clk.hw,
	&measure_only_gpu_cc_cx_gfx3d_slv_clk.hw,
	&measure_only_gpu_cc_gx_gfx3d_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_memnoc_clk.hw,
	&measure_only_pcie_0_pipe_clk.hw,
	&measure_only_snoc_clk.hw,
	&measure_only_ufs_phy_rx_symbol_0_clk.hw,
	&measure_only_ufs_phy_rx_symbol_1_clk.hw,
	&measure_only_ufs_phy_tx_symbol_0_clk.hw,
	&measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk.hw,
	&measure_only_video_cc_ahb_clk.hw,
	&measure_only_video_cc_xo_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,diwali-debugcc" },
	{ }
};

static int clk_debug_diwali_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret, i;

	BUILD_BUG_ON(ARRAY_SIZE(apss_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(apss_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(cam_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(cam_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(disp_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(disp_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) != ARRAY_SIZE(gcc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gpu_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(gpu_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(video_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(video_cc_debug_mux_sels));

	clk = devm_clk_get(&pdev->dev, "xo_clk_src");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo clock\n");
		return PTR_ERR(clk);
	}

	debug_mux_priv.cxo = clk;

	for (i = 0; i < ARRAY_SIZE(mux_list); i++) {
		if (IS_ERR_OR_NULL(mux_list[i].mux->regmap)) {
			ret = map_debug_bases(pdev,
				mux_list[i].regmap_name, mux_list[i].mux);
			if (ret == -EBADR)
				continue;
			else if (ret)
				return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(debugcc_diwali_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_diwali_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_diwali_hws[i]),
				PTR_ERR(clk));
			return PTR_ERR(clk);
		}
	}

	for (i = 0; i < ARRAY_SIZE(mux_list); i++) {
		ret = devm_clk_register_debug_mux(&pdev->dev, mux_list[i].mux);
		if (ret) {
			dev_err(&pdev->dev, "Unable to register mux clk %s, err:(%d)\n",
				qcom_clk_hw_get_name(&mux_list[i].mux->hw),
				ret);
			return ret;
		}
	}

	ret = clk_debug_measure_register(&gcc_debug_mux.hw);
	if (ret) {
		dev_err(&pdev->dev, "Could not register Measure clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered debug measure clocks\n");

	return 0;
}

static struct platform_driver clk_debug_driver = {
	.probe = clk_debug_diwali_probe,
	.driver = {
		.name = "diwali-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_diwali_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_diwali_init);

MODULE_DESCRIPTION("QTI DEBUG CC DIWALI Driver");
MODULE_LICENSE("GPL v2");

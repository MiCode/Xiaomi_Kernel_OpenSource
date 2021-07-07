// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

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
	.xo_div4_cbcr = 0x6200C,
};

static const char *const apss_cc_debug_mux_parent_names[] = {
	"measure_only_apcs_gold_post_acd_clk",
	"measure_only_apcs_goldplus_post_acd_clk",
	"measure_only_apcs_l3_post_acd_clk",
	"measure_only_apcs_silver_post_acd_clk",
};

static int apss_cc_debug_mux_sels[] = {
	0x25,		/* measure_only_apcs_gold_post_acd_clk */
	0x61,		/* measure_only_apcs_goldplus_post_acd_clk */
	0x41,		/* measure_only_apcs_l3_post_acd_clk */
	0x21,		/* measure_only_apcs_silver_post_acd_clk */
};

static int apss_cc_debug_mux_pre_divs[] = {
	0x8,		/* measure_only_apcs_gold_post_acd_clk */
	0x8,		/* measure_only_apcs_goldplus_post_acd_clk */
	0x4,		/* measure_only_apcs_l3_post_acd_clk */
	0x4,		/* measure_only_apcs_silver_post_acd_clk */
};

static struct clk_debug_mux apss_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x18,
	.post_div_offset = 0x18,
	.cbcr_offset = 0x0,
	.src_sel_mask = 0x7F0,
	.src_sel_shift = 4,
	.post_div_mask = 0x7800,
	.post_div_shift = 11,
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
	"cam_cc_gdsc_clk",
	"cam_cc_icp_ahb_clk",
	"cam_cc_icp_clk",
	"cam_cc_ife_0_axi_clk",
	"cam_cc_ife_0_clk",
	"cam_cc_ife_0_cphy_rx_clk",
	"cam_cc_ife_0_csid_clk",
	"cam_cc_ife_0_dsp_clk",
	"cam_cc_ife_1_axi_clk",
	"cam_cc_ife_1_clk",
	"cam_cc_ife_1_cphy_rx_clk",
	"cam_cc_ife_1_csid_clk",
	"cam_cc_ife_1_dsp_clk",
	"cam_cc_ife_2_axi_clk",
	"cam_cc_ife_2_clk",
	"cam_cc_ife_2_cphy_rx_clk",
	"cam_cc_ife_2_csid_clk",
	"cam_cc_ife_2_dsp_clk",
	"cam_cc_ife_lite_0_clk",
	"cam_cc_ife_lite_0_cphy_rx_clk",
	"cam_cc_ife_lite_0_csid_clk",
	"cam_cc_ife_lite_1_clk",
	"cam_cc_ife_lite_1_cphy_rx_clk",
	"cam_cc_ife_lite_1_csid_clk",
	"cam_cc_ipe_0_ahb_clk",
	"cam_cc_ipe_0_areg_clk",
	"cam_cc_ipe_0_axi_clk",
	"cam_cc_ipe_0_clk",
	"cam_cc_jpeg_clk",
	"cam_cc_lrme_clk",
	"cam_cc_mclk0_clk",
	"cam_cc_mclk1_clk",
	"cam_cc_mclk2_clk",
	"cam_cc_mclk3_clk",
	"cam_cc_mclk4_clk",
	"cam_cc_mclk5_clk",
	"cam_cc_sleep_clk",
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
	0x41,		/* cam_cc_gdsc_clk */
	0x36,		/* cam_cc_icp_ahb_clk */
	0x35,		/* cam_cc_icp_clk */
	0x25,		/* cam_cc_ife_0_axi_clk */
	0x1E,		/* cam_cc_ife_0_clk */
	0x24,		/* cam_cc_ife_0_cphy_rx_clk */
	0x22,		/* cam_cc_ife_0_csid_clk */
	0x21,		/* cam_cc_ife_0_dsp_clk */
	0x2D,		/* cam_cc_ife_1_axi_clk */
	0x27,		/* cam_cc_ife_1_clk */
	0x2C,		/* cam_cc_ife_1_cphy_rx_clk */
	0x2B,		/* cam_cc_ife_1_csid_clk */
	0x2A,		/* cam_cc_ife_1_dsp_clk */
	0x53,		/* cam_cc_ife_2_axi_clk */
	0x7,		/* cam_cc_ife_2_clk */
	0x52,		/* cam_cc_ife_2_cphy_rx_clk */
	0x51,		/* cam_cc_ife_2_csid_clk */
	0x26,		/* cam_cc_ife_2_dsp_clk */
	0x2F,		/* cam_cc_ife_lite_0_clk */
	0x31,		/* cam_cc_ife_lite_0_cphy_rx_clk */
	0x30,		/* cam_cc_ife_lite_0_csid_clk */
	0x2E,		/* cam_cc_ife_lite_1_clk */
	0x37,		/* cam_cc_ife_lite_1_cphy_rx_clk */
	0x32,		/* cam_cc_ife_lite_1_csid_clk */
	0x1D,		/* cam_cc_ipe_0_ahb_clk */
	0x1C,		/* cam_cc_ipe_0_areg_clk */
	0x1B,		/* cam_cc_ipe_0_axi_clk */
	0x19,		/* cam_cc_ipe_0_clk */
	0x33,		/* cam_cc_jpeg_clk */
	0x13,		/* cam_cc_lrme_clk */
	0x1,		/* cam_cc_mclk0_clk */
	0x2,		/* cam_cc_mclk1_clk */
	0x3,		/* cam_cc_mclk2_clk */
	0x4,		/* cam_cc_mclk3_clk */
	0x5,		/* cam_cc_mclk4_clk */
	0x6,		/* cam_cc_mclk5_clk */
	0x42,		/* cam_cc_sleep_clk */
};

static struct clk_debug_mux cam_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0xD000,
	.post_div_offset = 0xD004,
	.cbcr_offset = 0xD008,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = cam_cc_debug_mux_sels,
	.hw.init = &(struct clk_init_data){
		.name = "cam_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = cam_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(cam_cc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static const char *const disp_cc_debug_mux_parent_names[] = {
	"disp_cc_mdss_ahb_clk",
	"disp_cc_mdss_byte0_clk",
	"disp_cc_mdss_byte0_intf_clk",
	"disp_cc_mdss_dp_aux_clk",
	"disp_cc_mdss_dp_crypto_clk",
	"disp_cc_mdss_dp_link_clk",
	"disp_cc_mdss_dp_link_intf_clk",
	"disp_cc_mdss_dp_pixel_clk",
	"disp_cc_mdss_edp_aux_clk",
	"disp_cc_mdss_edp_link_clk",
	"disp_cc_mdss_edp_link_intf_clk",
	"disp_cc_mdss_edp_pixel_clk",
	"disp_cc_mdss_esc0_clk",
	"disp_cc_mdss_mdp_clk",
	"disp_cc_mdss_mdp_lut_clk",
	"disp_cc_mdss_non_gdsc_ahb_clk",
	"disp_cc_mdss_pclk0_clk",
	"disp_cc_mdss_rot_clk",
	"disp_cc_mdss_rscc_ahb_clk",
	"disp_cc_mdss_rscc_vsync_clk",
	"disp_cc_mdss_vsync_clk",
	"disp_cc_sleep_clk",
	"measure_only_disp_cc_xo_clk",
};

static int disp_cc_debug_mux_sels[] = {
	0x16,		/* disp_cc_mdss_ahb_clk */
	0xE,		/* disp_cc_mdss_byte0_clk */
	0xF,		/* disp_cc_mdss_byte0_intf_clk */
	0x15,		/* disp_cc_mdss_dp_aux_clk */
	0x13,		/* disp_cc_mdss_dp_crypto_clk */
	0x11,		/* disp_cc_mdss_dp_link_clk */
	0x12,		/* disp_cc_mdss_dp_link_intf_clk */
	0x14,		/* disp_cc_mdss_dp_pixel_clk */
	0x1A,		/* disp_cc_mdss_edp_aux_clk */
	0x18,		/* disp_cc_mdss_edp_link_clk */
	0x19,		/* disp_cc_mdss_edp_link_intf_clk */
	0x17,		/* disp_cc_mdss_edp_pixel_clk */
	0x10,		/* disp_cc_mdss_esc0_clk */
	0xA,		/* disp_cc_mdss_mdp_clk */
	0xC,		/* disp_cc_mdss_mdp_lut_clk */
	0x1B,		/* disp_cc_mdss_non_gdsc_ahb_clk */
	0x9,		/* disp_cc_mdss_pclk0_clk */
	0xB,		/* disp_cc_mdss_rot_clk */
	0x1D,		/* disp_cc_mdss_rscc_ahb_clk */
	0x1C,		/* disp_cc_mdss_rscc_vsync_clk */
	0xD,		/* disp_cc_mdss_vsync_clk */
	0x23,		/* disp_cc_sleep_clk */
	0x24,		/* measure_only_disp_cc_xo_clk */
};

static struct clk_debug_mux disp_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x7000,
	.post_div_offset = 0x3000,
	.cbcr_offset = 0x3004,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
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
	"cam_cc_debug_mux",
	"disp_cc_debug_mux",
	"gcc_aggre_ufs_phy_axi_clk",
	"gcc_aggre_usb3_prim_axi_clk",
	"gcc_camera_hf_axi_clk",
	"gcc_camera_sf_axi_clk",
	"gcc_cfg_noc_usb3_prim_axi_clk",
	"gcc_ddrss_gpu_axi_clk",
	"gcc_disp_gpll0_clk_src",
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
	"gcc_qmip_video_vcodec_ahb_clk",
	"gcc_qupv3_wrap0_core_2x_clk",
	"gcc_qupv3_wrap0_core_clk",
	"gcc_qupv3_wrap0_s0_clk",
	"gcc_qupv3_wrap0_s2_clk",
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
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_sdcc4_ahb_clk",
	"gcc_sdcc4_apps_clk",
	"gcc_titan_nrt_throttle_core_clk",
	"gcc_titan_rt_throttle_core_clk",
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
	"gcc_video_mvp_throttle_core_clk",
	"gpu_cc_debug_mux",
	"mc_cc_debug_mux",
	"measure_only_cnoc_clk",
	"measure_only_gcc_aggre_noc_pcie_0_axi_clk",
	"measure_only_gcc_aggre_noc_pcie_1_axi_clk",
	"measure_only_gcc_aggre_noc_pcie_center_sf_axi_clk",
	"measure_only_gcc_aggre_usb3_sec_axi_clk",
	"measure_only_gcc_aoss_at_clk",
	"measure_only_gcc_apss_qdss_apb_clk",
	"measure_only_gcc_apss_qdss_tsctr_clk",
	"measure_only_gcc_cfg_noc_usb3_sec_axi_clk",
	"measure_only_gcc_camera_ahb_clk",
	"measure_only_gcc_camera_xo_clk",
	"measure_only_gcc_cnoc_qdss_stm_clk",
	"measure_only_gcc_config_noc_at_clk",
	"measure_only_gcc_cpuss_at_clk",
	"measure_only_gcc_cpuss_trig_clk",
	"measure_only_gcc_ddrss_at_clk",
	"measure_only_gcc_ddrss_pcie_sf_clk",
	"measure_only_gcc_disp_ahb_clk",
	"measure_only_gcc_disp_xo_clk",
	"measure_only_gcc_east_at_clk",
	"measure_only_gcc_gpu_at_clk",
	"measure_only_gcc_gpu_cfg_ahb_clk",
	"measure_only_gcc_gpu_trig_clk",
	"measure_only_gcc_lpass_trig_clk",
	"measure_only_gcc_mmnoc_at_clk",
	"measure_only_gcc_mmss_at_clk",
	"measure_only_gcc_mmss_trig_clk",
	"measure_only_gcc_mss_at_clk",
	"measure_only_gcc_pcie0_phy_rchng_clk",
	"measure_only_gcc_pcie1_phy_rchng_clk",
	"measure_only_gcc_pcie_0_aux_clk",
	"measure_only_gcc_pcie_0_cfg_ahb_clk",
	"measure_only_gcc_pcie_0_mstr_axi_clk",
	"measure_only_gcc_pcie_0_pipe_clk",
	"measure_only_gcc_pcie_0_slv_axi_clk",
	"measure_only_gcc_pcie_0_slv_q2a_axi_clk",
	"measure_only_gcc_pcie_1_aux_clk",
	"measure_only_gcc_pcie_1_cfg_ahb_clk",
	"measure_only_gcc_pcie_1_mstr_axi_clk",
	"measure_only_gcc_pcie_1_pipe_clk",
	"measure_only_gcc_pcie_1_slv_axi_clk",
	"measure_only_gcc_pcie_1_slv_q2a_axi_clk",
	"measure_only_gcc_pcie_throttle_core_clk",
	"measure_only_gcc_phy_at_clk",
	"measure_only_gcc_pimem_at_clk",
	"measure_only_gcc_qdss_center_at_clk",
	"measure_only_gcc_qdss_cfg_ahb_clk",
	"measure_only_gcc_qdss_dap_ahb_clk",
	"measure_only_gcc_qdss_dap_clk",
	"measure_only_gcc_qdss_etr_usb_clk",
	"measure_only_gcc_qdss_stm_clk",
	"measure_only_gcc_qdss_traceclkin_clk",
	"measure_only_gcc_qdss_tsctr_clk",
	"measure_only_gcc_qdss_xo_clk",
	"measure_only_gcc_qupv3_wrap0_s1_clk",
	"measure_only_gcc_qupv3_wrap_0_m_ahb_clk",
	"measure_only_gcc_qupv3_wrap_0_s_ahb_clk",
	"measure_only_gcc_south_at_clk",
	"measure_only_gcc_sys_noc_at_clk",
	"measure_only_gcc_throttle_pcie_ahb_clk",
	"measure_only_gcc_turing_at_clk",
	"measure_only_gcc_turing_trig_clk",
	"measure_only_gcc_usb30_sec_master_clk",
	"measure_only_gcc_usb30_sec_mock_utmi_clk",
	"measure_only_gcc_usb30_sec_sleep_clk",
	"measure_only_gcc_usb3_sec_phy_aux_clk",
	"measure_only_gcc_usb3_sec_phy_com_aux_clk",
	"measure_only_gcc_usb3_sec_phy_pipe_clk",
	"measure_only_gcc_video_ahb_clk",
	"measure_only_gcc_video_xo_clk",
	"measure_only_gcc_west_at_clk",
	"measure_only_gcc_wpss_at_clk",
	"measure_only_gcc_wpss_m_at_clk",
	"measure_only_gcc_wpss_trig_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_memnoc_clk",
	"measure_only_pcie_0_pipe_clk",
	"measure_only_pcie_1_pipe_clk",
	"measure_only_snoc_clk",
	"measure_only_ufs_phy_rx_symbol_0_clk",
	"measure_only_ufs_phy_rx_symbol_1_clk",
	"measure_only_ufs_phy_tx_symbol_0_clk",
	"measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
	"video_cc_debug_mux",
	"measure_only_gcc_at_clk",
	"measure_only_gcc_lpass_at_clk",
	"measure_only_gcc_mss_trig_clk",
	"measure_only_gcc_qdss_trig_clk",
};

static int gcc_debug_mux_sels[] = {
	0xED,		/* apss_cc_debug_mux */
	0x4E,		/* cam_cc_debug_mux */
	0x54,		/* disp_cc_debug_mux */
	0x12F,		/* gcc_aggre_ufs_phy_axi_clk */
	0x12E,		/* gcc_aggre_usb3_prim_axi_clk */
	0x49,		/* gcc_camera_hf_axi_clk */
	0x4B,		/* gcc_camera_sf_axi_clk */
	0x1B,		/* gcc_cfg_noc_usb3_prim_axi_clk */
	0xCD,		/* gcc_ddrss_gpu_axi_clk */
	0x45,		/* gcc_disp_gpll0_clk_src */
	0x51,		/* gcc_disp_hf_axi_clk */
	0x52,		/* gcc_disp_sf_axi_clk */
	0xF8,		/* gcc_gp1_clk */
	0xF9,		/* gcc_gp2_clk */
	0xFA,		/* gcc_gp3_clk */
	0x14B,		/* gcc_gpu_gpll0_clk_src */
	0x14C,		/* gcc_gpu_gpll0_div_clk_src */
	0x147,		/* gcc_gpu_memnoc_gfx_clk */
	0x14A,		/* gcc_gpu_snoc_dvm_gfx_clk */
	0xA4,		/* gcc_pdm2_clk */
	0xA2,		/* gcc_pdm_ahb_clk */
	0xA3,		/* gcc_pdm_xo4_clk */
	0x47,		/* gcc_qmip_camera_nrt_ahb_clk */
	0x48,		/* gcc_qmip_camera_rt_ahb_clk */
	0x50,		/* gcc_qmip_disp_ahb_clk */
	0x57,		/* gcc_qmip_video_vcodec_ahb_clk */
	0x8D,		/* gcc_qupv3_wrap0_core_2x_clk */
	0x8C,		/* gcc_qupv3_wrap0_core_clk */
	0x8E,		/* gcc_qupv3_wrap0_s0_clk */
	0x90,		/* gcc_qupv3_wrap0_s2_clk */
	0x91,		/* gcc_qupv3_wrap0_s3_clk */
	0x92,		/* gcc_qupv3_wrap0_s4_clk */
	0x93,		/* gcc_qupv3_wrap0_s5_clk */
	0x94,		/* gcc_qupv3_wrap0_s6_clk */
	0x95,		/* gcc_qupv3_wrap0_s7_clk */
	0x99,		/* gcc_qupv3_wrap1_core_2x_clk */
	0x98,		/* gcc_qupv3_wrap1_core_clk */
	0x9A,		/* gcc_qupv3_wrap1_s0_clk */
	0x9B,		/* gcc_qupv3_wrap1_s1_clk */
	0x9C,		/* gcc_qupv3_wrap1_s2_clk */
	0x9D,		/* gcc_qupv3_wrap1_s3_clk */
	0x9E,		/* gcc_qupv3_wrap1_s4_clk */
	0x9F,		/* gcc_qupv3_wrap1_s5_clk */
	0xA0,		/* gcc_qupv3_wrap1_s6_clk */
	0xA1,		/* gcc_qupv3_wrap1_s7_clk */
	0x96,		/* gcc_qupv3_wrap_1_m_ahb_clk */
	0x97,		/* gcc_qupv3_wrap_1_s_ahb_clk */
	0x15D,		/* gcc_sdcc1_ahb_clk */
	0x15E,		/* gcc_sdcc1_apps_clk */
	0x15F,		/* gcc_sdcc1_ice_core_clk */
	0x87,		/* gcc_sdcc2_ahb_clk */
	0x86,		/* gcc_sdcc2_apps_clk */
	0x89,		/* gcc_sdcc4_ahb_clk */
	0x88,		/* gcc_sdcc4_apps_clk */
	0x4C,		/* gcc_titan_nrt_throttle_core_clk */
	0x4A,		/* gcc_titan_rt_throttle_core_clk */
	0x10E,		/* gcc_ufs_phy_ahb_clk */
	0x10D,		/* gcc_ufs_phy_axi_clk */
	0x114,		/* gcc_ufs_phy_ice_core_clk */
	0x115,		/* gcc_ufs_phy_phy_aux_clk */
	0x110,		/* gcc_ufs_phy_rx_symbol_0_clk */
	0x116,		/* gcc_ufs_phy_rx_symbol_1_clk */
	0x10F,		/* gcc_ufs_phy_tx_symbol_0_clk */
	0x113,		/* gcc_ufs_phy_unipro_core_clk */
	0x70,		/* gcc_usb30_prim_master_clk */
	0x72,		/* gcc_usb30_prim_mock_utmi_clk */
	0x71,		/* gcc_usb30_prim_sleep_clk */
	0x73,		/* gcc_usb3_prim_phy_aux_clk */
	0x74,		/* gcc_usb3_prim_phy_com_aux_clk */
	0x75,		/* gcc_usb3_prim_phy_pipe_clk */
	0x58,		/* gcc_video_axi0_clk */
	0x59,		/* gcc_video_mvp_throttle_core_clk */
	0x146,		/* gpu_cc_debug_mux */
	0xD7,		/* mc_cc_debug_mux or ddrss_gcc_debug_clk */
	0x17,		/* measure_only_cnoc_clk */
	0x12C,		/* measure_only_gcc_aggre_noc_pcie_0_axi_clk */
	0x12D,		/* measure_only_gcc_aggre_noc_pcie_1_axi_clk */
	0x172,		/* measure_only_gcc_aggre_noc_pcie_center_sf_axi_clk */
	0x170,		/* measure_only_gcc_aggre_usb3_sec_axi_clk */
	0xB3,		/* measure_only_gcc_aoss_at_clk */
	0xEC,		/* measure_only_gcc_apss_qdss_apb_clk */
	0xEB,		/* measure_only_gcc_apss_qdss_tsctr_clk */
	0x1C,		/* measure_only_gcc_cfg_noc_usb3_sec_axi_clk */
	0x46,		/* measure_only_gcc_camera_ahb_clk */
	0x4D,		/* measure_only_gcc_camera_xo_clk */
	0x1A,		/* measure_only_gcc_cnoc_qdss_stm_clk */
	0x25,		/* measure_only_gcc_config_noc_at_clk */
	0xEA,		/* measure_only_gcc_cpuss_at_clk */
	0xE9,		/* measure_only_gcc_cpuss_trig_clk */
	0xD4,		/* measure_only_gcc_ddrss_at_clk */
	0xCE,		/* measure_only_gcc_ddrss_pcie_sf_clk */
	0x4F,		/* measure_only_gcc_disp_ahb_clk */
	0x53,		/* measure_only_gcc_disp_xo_clk */
	0x65,		/* measure_only_gcc_east_at_clk */
	0x145,		/* measure_only_gcc_gpu_at_clk */
	0x144,		/* measure_only_gcc_gpu_cfg_ahb_clk */
	0x149,		/* measure_only_gcc_gpu_trig_clk */
	0xDA,		/* measure_only_gcc_lpass_trig_clk */
	0x35,		/* measure_only_gcc_mmnoc_at_clk */
	0x41,		/* measure_only_gcc_mmss_at_clk */
	0x43,		/* measure_only_gcc_mmss_trig_clk */
	0x13B,		/* measure_only_gcc_mss_at_clk */
	0x101,		/* measure_only_gcc_pcie0_phy_rchng_clk */
	0x10A,		/* measure_only_gcc_pcie1_phy_rchng_clk */
	0xFF,		/* measure_only_gcc_pcie_0_aux_clk */
	0xFE,		/* measure_only_gcc_pcie_0_cfg_ahb_clk */
	0xFD,		/* measure_only_gcc_pcie_0_mstr_axi_clk */
	0x100,		/* measure_only_gcc_pcie_0_pipe_clk */
	0xFC,		/* measure_only_gcc_pcie_0_slv_axi_clk */
	0xFB,		/* measure_only_gcc_pcie_0_slv_q2a_axi_clk */
	0x108,		/* measure_only_gcc_pcie_1_aux_clk */
	0x107,		/* measure_only_gcc_pcie_1_cfg_ahb_clk */
	0x106,		/* measure_only_gcc_pcie_1_mstr_axi_clk */
	0x109,		/* measure_only_gcc_pcie_1_pipe_clk */
	0x105,		/* measure_only_gcc_pcie_1_slv_axi_clk */
	0x104,		/* measure_only_gcc_pcie_1_slv_q2a_axi_clk */
	0x30,		/* measure_only_gcc_pcie_throttle_core_clk */
	0x67,		/* measure_only_gcc_phy_at_clk */
	0x60,		/* measure_only_gcc_pimem_at_clk */
	0x63,		/* measure_only_gcc_qdss_center_at_clk */
	0x62,		/* measure_only_gcc_qdss_cfg_ahb_clk */
	0x61,		/* measure_only_gcc_qdss_dap_ahb_clk */
	0x6D,		/* measure_only_gcc_qdss_dap_clk */
	0x68,		/* measure_only_gcc_qdss_etr_usb_clk */
	0x69,		/* measure_only_gcc_qdss_stm_clk */
	0x6A,		/* measure_only_gcc_qdss_traceclkin_clk */
	0x6B,		/* measure_only_gcc_qdss_tsctr_clk */
	0x6F,		/* measure_only_gcc_qdss_xo_clk */
	0x8F,		/* measure_only_gcc_qupv3_wrap0_s1_clk */
	0x8A,		/* measure_only_gcc_qupv3_wrap_0_m_ahb_clk */
	0x8B,		/* measure_only_gcc_qupv3_wrap_0_s_ahb_clk */
	0x64,		/* measure_only_gcc_south_at_clk */
	0xE,		/* measure_only_gcc_sys_noc_at_clk */
	0x3A,		/* measure_only_gcc_throttle_pcie_ahb_clk */
	0xE5,		/* measure_only_gcc_turing_at_clk */
	0xE6,		/* measure_only_gcc_turing_trig_clk */
	0x7C,		/* measure_only_gcc_usb30_sec_master_clk */
	0x7E,		/* measure_only_gcc_usb30_sec_mock_utmi_clk */
	0x7D,		/* measure_only_gcc_usb30_sec_sleep_clk */
	0x7F,		/* measure_only_gcc_usb3_sec_phy_aux_clk */
	0x80,		/* measure_only_gcc_usb3_sec_phy_com_aux_clk */
	0x81,		/* measure_only_gcc_usb3_sec_phy_pipe_clk */
	0x55,		/* measure_only_gcc_video_ahb_clk */
	0x5C,		/* measure_only_gcc_video_xo_clk */
	0x66,		/* measure_only_gcc_west_at_clk */
	0x163,		/* measure_only_gcc_wpss_at_clk */
	0x179,		/* measure_only_gcc_wpss_m_at_clk */
	0x164,		/* measure_only_gcc_wpss_trig_clk */
	0x132,		/* measure_only_ipa_2x_clk */
	0xD3,		/* measure_only_memnoc_clk */
	0x102,		/* measure_only_pcie_0_pipe_clk */
	0x10B,		/* measure_only_pcie_1_pipe_clk */
	0x9,		/* measure_only_snoc_clk */
	0x112,		/* measure_only_ufs_phy_rx_symbol_0_clk */
	0x117,		/* measure_only_ufs_phy_rx_symbol_1_clk */
	0x111,		/* measure_only_ufs_phy_tx_symbol_0_clk */
	0x79,		/* measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk */
	0x5D,		/* video_cc_debug_mux */
	0xC2,		/* measure_only_gcc_at_clk */
	0xDB,		/* measure_only_gcc_lpass_at_clk */
	0x13A,		/* measure_only_gcc_mss_trig_clk */
	0x6C,		/* measure_only_gcc_qdss_trig_clk */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x62000,
	.post_div_offset = 0x62004,
	.cbcr_offset = 0x62008,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 2,
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
	"gpu_cc_cx_gmu_clk",
	"gpu_cc_cx_snoc_dvm_clk",
	"gpu_cc_cxo_aon_clk",
	"gpu_cc_cxo_clk",
	"gpu_cc_gx_gmu_clk",
	"gpu_cc_hub_aon_clk",
	"gpu_cc_hub_cx_int_clk",
	"gpu_cc_mnd1x_0_gfx3d_clk",
	"gpu_cc_mnd1x_1_gfx3d_clk",
	"gpu_cc_sleep_clk",
	"measure_only_gpu_cc_cb_clk",
	"measure_only_gpu_cc_cx_gfx3d_clk",
	"measure_only_gpu_cc_cx_gfx3d_slv_clk",
	"measure_only_gpu_cc_gx_gfx3d_clk",
};

static int gpu_cc_debug_mux_sels[] = {
	0x12,		/* gpu_cc_ahb_clk */
	0x13,		/* gpu_cc_crc_ahb_clk */
	0x1A,		/* gpu_cc_cx_gmu_clk */
	0x17,		/* gpu_cc_cx_snoc_dvm_clk */
	0xB,		/* gpu_cc_cxo_aon_clk */
	0x1B,		/* gpu_cc_cxo_clk */
	0x11,		/* gpu_cc_gx_gmu_clk */
	0x27,		/* gpu_cc_hub_aon_clk */
	0x1C,		/* gpu_cc_hub_cx_int_clk */
	0x21,		/* gpu_cc_mnd1x_0_gfx3d_clk */
	0x22,		/* gpu_cc_mnd1x_1_gfx3d_clk */
	0x18,		/* gpu_cc_sleep_clk */
	0x26,		/* measure_only_gpu_cc_cb_clk */
	0x1D,		/* measure_only_gpu_cc_cx_gfx3d_clk */
	0x1E,		/* measure_only_gpu_cc_cx_gfx3d_slv_clk */
	0xD,		/* measure_only_gpu_cc_gx_gfx3d_clk */
};

static struct clk_debug_mux gpu_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x1568,
	.post_div_offset = 0x10FC,
	.cbcr_offset = 0x1100,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
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

static const char *const video_cc_debug_mux_parent_names[] = {
	"video_cc_iris_ahb_clk",
	"video_cc_mvs0_axi_clk",
	"video_cc_mvs0_core_clk",
	"video_cc_mvsc_core_clk",
	"video_cc_mvsc_ctl_axi_clk",
	"video_cc_sleep_clk",
	"video_cc_venus_ahb_clk",
	"video_cc_xo_clk",
};

static int video_cc_debug_mux_sels[] = {
	0x5,		/* video_cc_iris_ahb_clk */
	0x9,		/* video_cc_mvs0_axi_clk */
	0x3,		/* video_cc_mvs0_core_clk */
	0x1,		/* video_cc_mvsc_core_clk */
	0x8,		/* video_cc_mvsc_ctl_axi_clk */
	0x7,		/* video_cc_sleep_clk */
	0xC,		/* video_cc_venus_ahb_clk */
	0x6,		/* video_cc_xo_clk */
};

static struct clk_debug_mux video_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x9000,
	.post_div_offset = 0x6000,
	.cbcr_offset = 0x6004,
	.src_sel_mask = 0x3F,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 2,
	.mux_sels = video_cc_debug_mux_sels,
	.hw.init = &(struct clk_init_data){
		.name = "video_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = video_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(video_cc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
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
		.flags = CLK_IS_MEASURE,
	},
};

static struct mux_regmap_names mux_list[] = {
	{ .mux = &apss_cc_debug_mux, .regmap_name = "qcom,apsscc" },
	{ .mux = &cam_cc_debug_mux, .regmap_name = "qcom,camcc" },
	{ .mux = &disp_cc_debug_mux, .regmap_name = "qcom,dispcc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
	{ .mux = &gpu_cc_debug_mux, .regmap_name = "qcom,gpucc" },
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
	{ .mux = &video_cc_debug_mux, .regmap_name = "qcom,videocc" },
};

static struct clk_dummy measure_only_apcs_gold_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_gold_post_acd_clk",
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

static struct clk_dummy measure_only_apcs_l3_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_l3_post_acd_clk",
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

static struct clk_dummy measure_only_gcc_aggre_noc_pcie_1_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_aggre_noc_pcie_1_axi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_aggre_noc_pcie_center_sf_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_aggre_noc_pcie_center_sf_axi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_aggre_usb3_sec_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_aggre_usb3_sec_axi_clk",
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

static struct clk_dummy measure_only_gcc_cfg_noc_usb3_sec_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_cfg_noc_usb3_sec_axi_clk",
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

static struct clk_dummy measure_only_gcc_east_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_east_at_clk",
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

static struct clk_dummy measure_only_gcc_gpu_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_gpu_trig_clk",
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

static struct clk_dummy measure_only_gcc_mmnoc_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mmnoc_at_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_mmss_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mmss_at_clk",
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

static struct clk_dummy measure_only_gcc_ddrss_pcie_sf_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_ddrss_pcie_sf_clk",
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

static struct clk_dummy measure_only_gcc_gpu_cfg_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_gpu_cfg_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie0_phy_rchng_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie0_phy_rchng_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie1_phy_rchng_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie1_phy_rchng_clk",
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

static struct clk_dummy measure_only_gcc_pcie_1_aux_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_1_aux_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_1_cfg_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_1_cfg_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_1_mstr_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_1_mstr_axi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_1_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_1_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_1_slv_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_1_slv_axi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_1_slv_q2a_axi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_1_slv_q2a_axi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_throttle_core_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_pcie_throttle_core_clk",
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

static struct clk_dummy measure_only_gcc_qdss_tsctr_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qdss_tsctr_clk",
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

static struct clk_dummy measure_only_gcc_qupv3_wrap0_s1_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_qupv3_wrap0_s1_clk",
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

static struct clk_dummy measure_only_gcc_throttle_pcie_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_throttle_pcie_ahb_clk",
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

static struct clk_dummy measure_only_gcc_usb30_sec_master_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_usb30_sec_master_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_usb30_sec_mock_utmi_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_usb30_sec_mock_utmi_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_usb30_sec_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_usb30_sec_sleep_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_usb3_sec_phy_aux_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_usb3_sec_phy_aux_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_usb3_sec_phy_com_aux_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_usb3_sec_phy_com_aux_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_usb3_sec_phy_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_usb3_sec_phy_pipe_clk",
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

static struct clk_dummy measure_only_gpu_cc_cb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gpu_cc_cb_clk",
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

static struct clk_dummy measure_only_mccc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_mccc_clk",
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

static struct clk_dummy measure_only_pcie_1_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pcie_1_pipe_clk",
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

static struct clk_dummy measure_only_gcc_at_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_at_clk",
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

static struct clk_dummy measure_only_gcc_mss_trig_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_mss_trig_clk",
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

static struct clk_hw *debugcc_yupik_hws[] = {
	&measure_only_apcs_gold_post_acd_clk.hw,
	&measure_only_apcs_goldplus_post_acd_clk.hw,
	&measure_only_apcs_l3_post_acd_clk.hw,
	&measure_only_apcs_silver_post_acd_clk.hw,
	&measure_only_cnoc_clk.hw,
	&measure_only_disp_cc_xo_clk.hw,
	&measure_only_gcc_aggre_noc_pcie_0_axi_clk.hw,
	&measure_only_gcc_aggre_noc_pcie_1_axi_clk.hw,
	&measure_only_gcc_aggre_noc_pcie_center_sf_axi_clk.hw,
	&measure_only_gcc_aggre_usb3_sec_axi_clk.hw,
	&measure_only_gcc_aoss_at_clk.hw,
	&measure_only_gcc_apss_qdss_apb_clk.hw,
	&measure_only_gcc_apss_qdss_tsctr_clk.hw,
	&measure_only_gcc_cfg_noc_usb3_sec_axi_clk.hw,
	&measure_only_gcc_camera_ahb_clk.hw,
	&measure_only_gcc_camera_xo_clk.hw,
	&measure_only_gcc_cnoc_qdss_stm_clk.hw,
	&measure_only_gcc_config_noc_at_clk.hw,
	&measure_only_gcc_cpuss_at_clk.hw,
	&measure_only_gcc_cpuss_trig_clk.hw,
	&measure_only_gcc_ddrss_at_clk.hw,
	&measure_only_gcc_ddrss_pcie_sf_clk.hw,
	&measure_only_gcc_disp_ahb_clk.hw,
	&measure_only_gcc_disp_xo_clk.hw,
	&measure_only_gcc_east_at_clk.hw,
	&measure_only_gcc_gpu_at_clk.hw,
	&measure_only_gcc_gpu_cfg_ahb_clk.hw,
	&measure_only_gcc_gpu_trig_clk.hw,
	&measure_only_gcc_lpass_trig_clk.hw,
	&measure_only_gcc_mmnoc_at_clk.hw,
	&measure_only_gcc_mmss_at_clk.hw,
	&measure_only_gcc_mmss_trig_clk.hw,
	&measure_only_gcc_mss_at_clk.hw,
	&measure_only_gcc_pcie0_phy_rchng_clk.hw,
	&measure_only_gcc_pcie1_phy_rchng_clk.hw,
	&measure_only_gcc_pcie_0_aux_clk.hw,
	&measure_only_gcc_pcie_0_cfg_ahb_clk.hw,
	&measure_only_gcc_pcie_0_mstr_axi_clk.hw,
	&measure_only_gcc_pcie_0_pipe_clk.hw,
	&measure_only_gcc_pcie_0_slv_axi_clk.hw,
	&measure_only_gcc_pcie_0_slv_q2a_axi_clk.hw,
	&measure_only_gcc_pcie_1_aux_clk.hw,
	&measure_only_gcc_pcie_1_cfg_ahb_clk.hw,
	&measure_only_gcc_pcie_1_mstr_axi_clk.hw,
	&measure_only_gcc_pcie_1_pipe_clk.hw,
	&measure_only_gcc_pcie_1_slv_axi_clk.hw,
	&measure_only_gcc_pcie_1_slv_q2a_axi_clk.hw,
	&measure_only_gcc_pcie_throttle_core_clk.hw,
	&measure_only_gcc_phy_at_clk.hw,
	&measure_only_gcc_pimem_at_clk.hw,
	&measure_only_gcc_qdss_center_at_clk.hw,
	&measure_only_gcc_qdss_cfg_ahb_clk.hw,
	&measure_only_gcc_qdss_dap_ahb_clk.hw,
	&measure_only_gcc_qdss_dap_clk.hw,
	&measure_only_gcc_qdss_etr_usb_clk.hw,
	&measure_only_gcc_qdss_stm_clk.hw,
	&measure_only_gcc_qdss_traceclkin_clk.hw,
	&measure_only_gcc_qdss_tsctr_clk.hw,
	&measure_only_gcc_qdss_xo_clk.hw,
	&measure_only_gcc_qupv3_wrap0_s1_clk.hw,
	&measure_only_gcc_qupv3_wrap_0_m_ahb_clk.hw,
	&measure_only_gcc_qupv3_wrap_0_s_ahb_clk.hw,
	&measure_only_gcc_south_at_clk.hw,
	&measure_only_gcc_sys_noc_at_clk.hw,
	&measure_only_gcc_throttle_pcie_ahb_clk.hw,
	&measure_only_gcc_turing_at_clk.hw,
	&measure_only_gcc_turing_trig_clk.hw,
	&measure_only_gcc_usb30_sec_master_clk.hw,
	&measure_only_gcc_usb30_sec_mock_utmi_clk.hw,
	&measure_only_gcc_usb30_sec_sleep_clk.hw,
	&measure_only_gcc_usb3_sec_phy_aux_clk.hw,
	&measure_only_gcc_usb3_sec_phy_com_aux_clk.hw,
	&measure_only_gcc_usb3_sec_phy_pipe_clk.hw,
	&measure_only_gcc_video_ahb_clk.hw,
	&measure_only_gcc_video_xo_clk.hw,
	&measure_only_gcc_west_at_clk.hw,
	&measure_only_gcc_wpss_at_clk.hw,
	&measure_only_gcc_wpss_m_at_clk.hw,
	&measure_only_gcc_wpss_trig_clk.hw,
	&measure_only_gpu_cc_cb_clk.hw,
	&measure_only_gpu_cc_cx_gfx3d_clk.hw,
	&measure_only_gpu_cc_cx_gfx3d_slv_clk.hw,
	&measure_only_gpu_cc_gx_gfx3d_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_memnoc_clk.hw,
	&measure_only_pcie_0_pipe_clk.hw,
	&measure_only_pcie_1_pipe_clk.hw,
	&measure_only_snoc_clk.hw,
	&measure_only_ufs_phy_rx_symbol_0_clk.hw,
	&measure_only_ufs_phy_rx_symbol_1_clk.hw,
	&measure_only_ufs_phy_tx_symbol_0_clk.hw,
	&measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk.hw,
	&measure_only_gcc_at_clk.hw,
	&measure_only_gcc_lpass_at_clk.hw,
	&measure_only_gcc_mss_trig_clk.hw,
	&measure_only_gcc_qdss_trig_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,yupik-debugcc" },
	{ }
};

static int clk_debug_yupik_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0, i;

	BUILD_BUG_ON(ARRAY_SIZE(apss_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(apss_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(cam_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(cam_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(disp_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(disp_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) !=
		ARRAY_SIZE(gcc_debug_mux_sels));
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

	for (i = 0; i < ARRAY_SIZE(debugcc_yupik_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_yupik_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_yupik_hws[i]),
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
	.probe = clk_debug_yupik_probe,
	.driver = {
		.name = "yupik-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_yupik_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_yupik_init);

MODULE_DESCRIPTION("QTI DEBUG CC YUPIK Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:debugcc-yupik");

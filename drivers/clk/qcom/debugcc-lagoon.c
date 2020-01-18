// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
	.ctl_reg = 0x35F24,
	.status_reg = 0x35F28,
	.xo_div4_cbcr = 0x2C008,
};

static const char *const cpu_cc_debug_mux_parent_names[] = {
	"l3_clk",
	"perfcl_clk",
	"pwrcl_clk",
};

static int cpu_cc_debug_mux_sels[] = {
	0x41,		/* l3_clk */
	0x21,		/* perf_clk */
	0x25,		/* pwrcl_clk */
};

static int apss_cc_debug_mux_pre_divs[] = {
	0x4,		/* l3_clk */
	0x4,		/* perfcl_clk */
	0x8,		/* pwrcl_clk */
};

static struct clk_debug_mux cpu_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x18,
	.post_div_offset = 0x18,
	.cbcr_offset = 0x0,
	.src_sel_mask = 0x7F0,
	.src_sel_shift = 4,
	.post_div_mask = 0x7800,
	.post_div_shift = 11,
	.post_div_val = 1,
	.mux_sels = cpu_cc_debug_mux_sels,
	.pre_div_vals = apss_cc_debug_mux_pre_divs,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = cpu_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(cpu_cc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static const char *const cam_cc_debug_mux_parent_names[] = {
	"cam_cc_bps_ahb_clk",
	"cam_cc_bps_areg_clk",
	"cam_cc_bps_axi_clk",
	"cam_cc_bps_clk",
	"cam_cc_camnoc_axi_clk",
	"cam_cc_cci_0_clk",
	"cam_cc_cci_1_clk",
	"cam_cc_core_ahb_clk",
	"cam_cc_cpas_ahb_clk",
	"cam_cc_csi0phytimer_clk",
	"cam_cc_csi1phytimer_clk",
	"cam_cc_csi2phytimer_clk",
	"cam_cc_csi3phytimer_clk",
	"cam_cc_csiphy0_clk",
	"cam_cc_csiphy1_clk",
	"cam_cc_csiphy2_clk",
	"cam_cc_csiphy3_clk",
	"cam_cc_icp_clk",
	"cam_cc_icp_ts_clk",
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
	"cam_cc_ife_lite_clk",
	"cam_cc_ife_lite_cphy_rx_clk",
	"cam_cc_ife_lite_csid_clk",
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
	"cam_cc_soc_ahb_clk",
	"cam_cc_sys_tmr_clk",
};

static int cam_cc_debug_mux_sels[] = {
	0x12,		/* cam_cc_bps_ahb_clk */
	0x11,		/* cam_cc_bps_areg_clk */
	0x10,		/* cam_cc_bps_axi_clk */
	0xE,		/* cam_cc_bps_clk */
	0x38,		/* cam_cc_camnoc_axi_clk */
	0x34,		/* cam_cc_cci_0_clk */
	0x35,		/* cam_cc_cci_1_clk */
	0x3B,		/* cam_cc_core_ahb_clk */
	0x37,		/* cam_cc_cpas_ahb_clk */
	0x6,		/* cam_cc_csi0phytimer_clk */
	0x8,		/* cam_cc_csi1phytimer_clk */
	0xA,		/* cam_cc_csi2phytimer_clk */
	0xC,		/* cam_cc_csi3phytimer_clk */
	0x7,		/* cam_cc_csiphy0_clk */
	0x9,		/* cam_cc_csiphy1_clk */
	0xB,		/* cam_cc_csiphy2_clk */
	0xD,		/* cam_cc_csiphy3_clk */
	0x32,		/* cam_cc_icp_clk */
	0x30,		/* cam_cc_icp_ts_clk */
	0x1E,		/* cam_cc_ife_0_axi_clk */
	0x18,		/* cam_cc_ife_0_clk */
	0x1D,		/* cam_cc_ife_0_cphy_rx_clk */
	0x1B,		/* cam_cc_ife_0_csid_clk */
	0x1A,		/* cam_cc_ife_0_dsp_clk */
	0x23,		/* cam_cc_ife_1_axi_clk */
	0x1F,		/* cam_cc_ife_1_clk */
	0x22,		/* cam_cc_ife_1_cphy_rx_clk */
	0x21,		/* cam_cc_ife_1_csid_clk */
	0x20,		/* cam_cc_ife_1_dsp_clk */
	0x28,		/* cam_cc_ife_2_axi_clk */
	0x24,		/* cam_cc_ife_2_clk */
	0x27,		/* cam_cc_ife_2_cphy_rx_clk */
	0x26,		/* cam_cc_ife_2_csid_clk */
	0x25,		/* cam_cc_ife_2_dsp_clk */
	0x29,		/* cam_cc_ife_lite_clk */
	0x2B,		/* cam_cc_ife_lite_cphy_rx_clk */
	0x2A,		/* cam_cc_ife_lite_csid_clk */
	0x17,		/* cam_cc_ipe_0_ahb_clk */
	0x16,		/* cam_cc_ipe_0_areg_clk */
	0x15,		/* cam_cc_ipe_0_axi_clk */
	0x13,		/* cam_cc_ipe_0_clk */
	0x2C,		/* cam_cc_jpeg_clk */
	0x36,		/* cam_cc_lrme_clk */
	0x1,		/* cam_cc_mclk0_clk */
	0x2,		/* cam_cc_mclk1_clk */
	0x3,		/* cam_cc_mclk2_clk */
	0x4,		/* cam_cc_mclk3_clk */
	0x5,		/* cam_cc_mclk4_clk */
	0x3A,		/* cam_cc_soc_ahb_clk */
	0x33,		/* cam_cc_sys_tmr_clk */
};

static struct clk_debug_mux cam_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x15100,
	.post_div_offset = 0x15004,
	.cbcr_offset = 0x15008,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0x3,
	.post_div_shift = 0,
	.post_div_val = 2,
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
	"disp_cc_xo_clk",
};

static int disp_cc_debug_mux_sels[] = {
	0x14,		/* disp_cc_mdss_ahb_clk */
	0xC,		/* disp_cc_mdss_byte0_clk */
	0xD,		/* disp_cc_mdss_byte0_intf_clk */
	0x13,		/* disp_cc_mdss_dp_aux_clk */
	0x11,		/* disp_cc_mdss_dp_crypto_clk */
	0xF,		/* disp_cc_mdss_dp_link_clk */
	0x10,		/* disp_cc_mdss_dp_link_intf_clk */
	0x12,		/* disp_cc_mdss_dp_pixel_clk */
	0xE,		/* disp_cc_mdss_esc0_clk */
	0x8,		/* disp_cc_mdss_mdp_clk */
	0xA,		/* disp_cc_mdss_mdp_lut_clk */
	0x15,		/* disp_cc_mdss_non_gdsc_ahb_clk */
	0x7,		/* disp_cc_mdss_pclk0_clk */
	0x9,		/* disp_cc_mdss_rot_clk */
	0x17,		/* disp_cc_mdss_rscc_ahb_clk */
	0x16,		/* disp_cc_mdss_rscc_vsync_clk */
	0xB,		/* disp_cc_mdss_vsync_clk */
	0x1D,		/* disp_cc_sleep_clk */
	0x1E,		/* disp_cc_xo_clk */
};

static struct clk_debug_mux disp_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x7000,
	.post_div_offset = 0x3000,
	.cbcr_offset = 0x3004,
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
	"cam_cc_debug_mux",
	"cpu_cc_debug_mux",
	"disp_cc_debug_mux",
	"gcc_aggre_ufs_phy_axi_clk",
	"gcc_aggre_usb3_prim_axi_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_camera_ahb_clk",
	"gcc_camera_axi_clk",
	"gcc_camera_throttle_nrt_axi_clk",
	"gcc_camera_throttle_rt_axi_clk",
	"gcc_camera_xo_clk",
	"gcc_ce1_ahb_clk",
	"gcc_ce1_axi_clk",
	"gcc_ce1_clk",
	"gcc_cfg_noc_usb3_prim_axi_clk",
	"gcc_cpuss_ahb_clk",
	"gcc_cpuss_gnoc_clk",
	"gcc_cpuss_rbcpr_clk",
	"gcc_ddrss_gpu_axi_clk",
	"gcc_disp_ahb_clk",
	"gcc_disp_axi_clk",
	"gcc_disp_cc_sleep_clk",
	"gcc_disp_cc_xo_clk",
	"gcc_disp_gpll0_clk",
	"gcc_disp_throttle_axi_clk",
	"gcc_disp_xo_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_gpu_cfg_ahb_clk",
	"gcc_gpu_gpll0_clk",
	"gcc_gpu_gpll0_div_clk",
	"gcc_gpu_memnoc_gfx_clk",
	"gcc_gpu_snoc_dvm_gfx_clk",
	"gcc_npu_axi_clk",
	"gcc_npu_bwmon_axi_clk",
	"gcc_npu_bwmon_dma_cfg_ahb_clk",
	"gcc_npu_bwmon_dsp_cfg_ahb_clk",
	"gcc_npu_cfg_ahb_clk",
	"gcc_npu_dma_clk",
	"gcc_npu_gpll0_clk",
	"gcc_npu_gpll0_div_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_prng_ahb_clk",
	"gcc_qupv3_wrap0_core_2x_clk",
	"gcc_qupv3_wrap0_core_clk",
	"gcc_qupv3_wrap0_s0_clk",
	"gcc_qupv3_wrap0_s1_clk",
	"gcc_qupv3_wrap0_s2_clk",
	"gcc_qupv3_wrap0_s3_clk",
	"gcc_qupv3_wrap0_s4_clk",
	"gcc_qupv3_wrap0_s5_clk",
	"gcc_qupv3_wrap1_core_2x_clk",
	"gcc_qupv3_wrap1_core_clk",
	"gcc_qupv3_wrap1_s0_clk",
	"gcc_qupv3_wrap1_s1_clk",
	"gcc_qupv3_wrap1_s2_clk",
	"gcc_qupv3_wrap1_s3_clk",
	"gcc_qupv3_wrap1_s4_clk",
	"gcc_qupv3_wrap1_s5_clk",
	"gcc_qupv3_wrap_0_m_ahb_clk",
	"gcc_qupv3_wrap_0_s_ahb_clk",
	"gcc_qupv3_wrap_1_m_ahb_clk",
	"gcc_qupv3_wrap_1_s_ahb_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_sys_noc_cpuss_ahb_clk",
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
	"gcc_video_ahb_clk",
	"gcc_video_axi_clk",
	"gcc_video_throttle_axi_clk",
	"gcc_video_xo_clk",
	"gpu_cc_debug_mux",
	"mc_cc_debug_mux",
	"measure_only_cnoc_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_snoc_clk",
	"npu_cc_debug_mux",
	"video_cc_debug_mux",
};

static int gcc_debug_mux_sels[] = {
	0x3F,		/* cam_cc_debug_mux */
	0xBE,		/* cpu_cc_debug_mux */
	0x40,		/* disp_cc_debug_mux */
	0xE2,		/* gcc_aggre_ufs_phy_axi_clk */
	0xE1,		/* gcc_aggre_usb3_prim_axi_clk */
	0x80,		/* gcc_boot_rom_ahb_clk */
	0x32,		/* gcc_camera_ahb_clk */
	0x36,		/* gcc_camera_axi_clk */
	0x4A,		/* gcc_camera_throttle_nrt_axi_clk */
	0x39,		/* gcc_camera_throttle_rt_axi_clk */
	0x3C,		/* gcc_camera_xo_clk */
	0x92,		/* gcc_ce1_ahb_clk */
	0x91,		/* gcc_ce1_axi_clk */
	0x90,		/* gcc_ce1_clk */
	0x18,		/* gcc_cfg_noc_usb3_prim_axi_clk */
	0xB7,		/* gcc_cpuss_ahb_clk */
	0xB8,		/* gcc_cpuss_gnoc_clk */
	0xB9,		/* gcc_cpuss_rbcpr_clk */
	0xA5,		/* gcc_ddrss_gpu_axi_clk */
	0x33,		/* gcc_disp_ahb_clk */
	0x37,		/* gcc_disp_axi_clk */
	0x49,		/* gcc_disp_cc_sleep_clk */
	0x48,		/* gcc_disp_cc_xo_clk */
	0x44,		/* gcc_disp_gpll0_clk */
	0x3A,		/* gcc_disp_throttle_axi_clk */
	0x3D,		/* gcc_disp_xo_clk */
	0xC5,		/* gcc_gp1_clk */
	0xC6,		/* gcc_gp2_clk */
	0xC7,		/* gcc_gp3_clk */
	0x105,		/* gcc_gpu_cfg_ahb_clk */
	0x10B,		/* gcc_gpu_gpll0_clk */
	0x10C,		/* gcc_gpu_gpll0_div_clk */
	0x108,		/* gcc_gpu_memnoc_gfx_clk */
	0x109,		/* gcc_gpu_snoc_dvm_gfx_clk */
	0x116,		/* gcc_npu_axi_clk */
	0x11C,		/* gcc_npu_bwmon_axi_clk */
	0x11D,		/* gcc_npu_bwmon_dma_cfg_ahb_clk */
	0x11E,		/* gcc_npu_bwmon_dsp_cfg_ahb_clk */
	0x115,		/* gcc_npu_cfg_ahb_clk */
	0x11B,		/* gcc_npu_dma_clk */
	0x118,		/* gcc_npu_gpll0_clk */
	0x119,		/* gcc_npu_gpll0_div_clk */
	0x7D,		/* gcc_pdm2_clk */
	0x7B,		/* gcc_pdm_ahb_clk */
	0x7C,		/* gcc_pdm_xo4_clk */
	0x7E,		/* gcc_prng_ahb_clk */
	0x6A,		/* gcc_qupv3_wrap0_core_2x_clk */
	0x69,		/* gcc_qupv3_wrap0_core_clk */
	0x6B,		/* gcc_qupv3_wrap0_s0_clk */
	0x6C,		/* gcc_qupv3_wrap0_s1_clk */
	0x6D,		/* gcc_qupv3_wrap0_s2_clk */
	0x6E,		/* gcc_qupv3_wrap0_s3_clk */
	0x6F,		/* gcc_qupv3_wrap0_s4_clk */
	0x70,		/* gcc_qupv3_wrap0_s5_clk */
	0x71,		/* gcc_qupv3_wrap1_core_2x_clk */
	0x72,		/* gcc_qupv3_wrap1_core_clk */
	0x75,		/* gcc_qupv3_wrap1_s0_clk */
	0x76,		/* gcc_qupv3_wrap1_s1_clk */
	0x77,		/* gcc_qupv3_wrap1_s2_clk */
	0x78,		/* gcc_qupv3_wrap1_s3_clk */
	0x79,		/* gcc_qupv3_wrap1_s4_clk */
	0x7A,		/* gcc_qupv3_wrap1_s5_clk */
	0x67,		/* gcc_qupv3_wrap_0_m_ahb_clk */
	0x68,		/* gcc_qupv3_wrap_0_s_ahb_clk */
	0x73,		/* gcc_qupv3_wrap_1_m_ahb_clk */
	0x74,		/* gcc_qupv3_wrap_1_s_ahb_clk */
	0x112,		/* gcc_sdcc1_ahb_clk */
	0x113,		/* gcc_sdcc1_apps_clk */
	0x114,		/* gcc_sdcc1_ice_core_clk */
	0x66,		/* gcc_sdcc2_ahb_clk */
	0x65,		/* gcc_sdcc2_apps_clk */
	0x9,		/* gcc_sys_noc_cpuss_ahb_clk */
	0xC8,		/* gcc_ufs_phy_ahb_clk */
	0xCC,		/* gcc_ufs_phy_axi_clk */
	0xD1,		/* gcc_ufs_phy_ice_core_clk */
	0xD2,		/* gcc_ufs_phy_phy_aux_clk */
	0xCA,		/* gcc_ufs_phy_rx_symbol_0_clk */
	0xCB,		/* gcc_ufs_phy_rx_symbol_1_clk */
	0xC9,		/* gcc_ufs_phy_tx_symbol_0_clk */
	0xD0,		/* gcc_ufs_phy_unipro_core_clk */
	0x5B,		/* gcc_usb30_prim_master_clk */
	0x5D,		/* gcc_usb30_prim_mock_utmi_clk */
	0x5C,		/* gcc_usb30_prim_sleep_clk */
	0x5E,		/* gcc_usb3_prim_phy_aux_clk */
	0x5F,		/* gcc_usb3_prim_phy_com_aux_clk */
	0x60,		/* gcc_usb3_prim_phy_pipe_clk */
	0x31,		/* gcc_video_ahb_clk */
	0x35,		/* gcc_video_axi_clk */
	0x38,		/* gcc_video_throttle_axi_clk */
	0x3B,		/* gcc_video_xo_clk */
	0x107,		/* gpu_cc_debug_mux */
	0xAB,		/* mc_cc_debug_mux */
	0x14,		/* measure_only_cnoc_clk */
	0xEC,		/* measure_only_ipa_2x_clk */
	0x07,		/* measure_only_snoc_clk */
	0x11A,		/* npu_cc_debug_mux */
	0x41,		/* video_cc_debug_mux */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x35F08,
	.post_div_offset = 0x35008,
	.cbcr_offset = 0x3500C,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0x7,
	.post_div_shift = 0,
	.post_div_val = 4,
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
	"gpu_cc_acd_ahb_clk",
	"gpu_cc_acd_cxo_clk",
	"gpu_cc_ahb_clk",
	"gpu_cc_crc_ahb_clk",
	"gpu_cc_cx_gfx3d_clk",
	"gpu_cc_cx_gfx3d_slv_clk",
	"gpu_cc_cx_gmu_clk",
	"gpu_cc_cx_snoc_dvm_clk",
	"gpu_cc_cxo_aon_clk",
	"gpu_cc_cxo_clk",
	"gpu_cc_gx_cxo_clk",
	"gpu_cc_gx_gfx3d_clk",
	"gpu_cc_gx_gmu_clk",
	"gpu_cc_gx_vsense_clk",
};

static int gpu_cc_debug_mux_sels[] = {
	0x20,		/* gpu_cc_acd_ahb_clk */
	0x1F,		/* gpu_cc_acd_cxo_clk */
	0x11,		/* gpu_cc_ahb_clk */
	0x12,		/* gpu_cc_crc_ahb_clk */
	0x1A,		/* gpu_cc_cx_gfx3d_clk */
	0x1B,		/* gpu_cc_cx_gfx3d_slv_clk */
	0x19,		/* gpu_cc_cx_gmu_clk */
	0x16,		/* gpu_cc_cx_snoc_dvm_clk */
	0xB,		/* gpu_cc_cxo_aon_clk */
	0xA,		/* gpu_cc_cxo_clk */
	0xF,		/* gpu_cc_gx_cxo_clk */
	0xC,		/* gpu_cc_gx_gfx3d_clk */
	0x10,		/* gpu_cc_gx_gmu_clk */
	0xD,		/* gpu_cc_gx_vsense_clk */
};

static struct clk_debug_mux gpu_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x1568,
	.post_div_offset = 0x10FC,
	.cbcr_offset = 0x1100,
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

static const char *const npu_cc_debug_mux_parent_names[] = {
	"npu_cc_bto_core_clk",
	"npu_cc_bwmon_clk",
	"npu_cc_cal_hm0_cdc_clk",
	"npu_cc_cal_hm0_clk",
	"npu_cc_cal_hm0_perf_cnt_clk",
	"npu_cc_core_clk",
	"npu_cc_dsp_ahbm_clk",
	"npu_cc_dsp_ahbs_clk",
	"npu_cc_dsp_axi_clk",
	"npu_cc_noc_ahb_clk",
	"npu_cc_noc_axi_clk",
	"npu_cc_noc_dma_clk",
	"npu_cc_rsc_xo_clk",
	"npu_cc_s2p_clk",
	"npu_cc_xo_clk",
};

static int npu_cc_debug_mux_sels[] = {
	0x19,		/* npu_cc_bto_core_clk */
	0x18,		/* npu_cc_bwmon_clk */
	0xB,		/* npu_cc_cal_hm0_cdc_clk */
	0x2,		/* npu_cc_cal_hm0_clk */
	0xD,		/* npu_cc_cal_hm0_perf_cnt_clk */
	0x4,		/* npu_cc_core_clk */
	0x1C,		/* npu_cc_dsp_ahbm_clk */
	0x1B,		/* npu_cc_dsp_ahbs_clk */
	0x1E,		/* npu_cc_dsp_axi_clk */
	0x13,		/* npu_cc_noc_ahb_clk */
	0x12,		/* npu_cc_noc_axi_clk */
	0x11,		/* npu_cc_noc_dma_clk */
	0x1A,		/* npu_cc_rsc_xo_clk */
	0x16,		/* npu_cc_s2p_clk */
	0x1,		/* npu_cc_xo_clk */
};

static struct clk_debug_mux npu_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x3000,
	.post_div_offset = 0x3004,
	.cbcr_offset = 0x3008,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0x3,
	.post_div_shift = 0,
	.post_div_val = 2,
	.mux_sels = npu_cc_debug_mux_sels,
	.hw.init = &(struct clk_init_data){
		.name = "npu_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = npu_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(npu_cc_debug_mux_parent_names),
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
	.post_div_mask = 0x7,
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
	{ .mux = &cpu_cc_debug_mux, .regmap_name = "qcom,cpucc" },
	{ .mux = &cam_cc_debug_mux, .regmap_name = "qcom,camcc" },
	{ .mux = &disp_cc_debug_mux, .regmap_name = "qcom,dispcc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
	{ .mux = &gpu_cc_debug_mux, .regmap_name = "qcom,gpucc" },
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
	{ .mux = &npu_cc_debug_mux, .regmap_name = "qcom,npucc" },
	{ .mux = &video_cc_debug_mux, .regmap_name = "qcom,videocc" },
};

static struct clk_dummy l3_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "l3_clk",
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

static struct clk_dummy perfcl_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "perfcl_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy pwrcl_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "pwrcl_clk",
		.ops = &clk_dummy_ops,
	},
};

struct clk_hw *debugcc_lagoon_hws[] = {
	&l3_clk.hw,
	&measure_only_cnoc_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_snoc_clk.hw,
	&perfcl_clk.hw,
	&pwrcl_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,lagoon-debugcc" },
	{ }
};

static int clk_debug_lagoon_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret, i;

	BUILD_BUG_ON(ARRAY_SIZE(cpu_cc_debug_mux_parent_names) !=
		     ARRAY_SIZE(cpu_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(cam_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(cam_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(disp_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(disp_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) !=
		ARRAY_SIZE(gcc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gpu_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(gpu_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(npu_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(npu_cc_debug_mux_sels));
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
				mux_list[i].mux->hw.init->name, PTR_ERR(clk));
			return PTR_ERR(clk);
		}
	}

	for (i = 0; i < ARRAY_SIZE(debugcc_lagoon_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_lagoon_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				debugcc_lagoon_hws[i]->init->name,
				PTR_ERR(clk));
			return PTR_ERR(clk);
		}
	}

	ret = clk_debug_measure_register(&gcc_debug_mux.hw);
	if (ret)
		dev_err(&pdev->dev, "Could not register Measure clock\n");

	return ret;
}

static struct platform_driver clk_debug_driver = {
	.probe = clk_debug_lagoon_probe,
	.driver = {
		.name = "debugcc-lagoon",
		.of_match_table = clk_debug_match_table,
	},
};

int __init clk_debug_lagoon_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_lagoon_init);

MODULE_DESCRIPTION("QTI DEBUG CC LAGOON Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:debugcc-lagoon");

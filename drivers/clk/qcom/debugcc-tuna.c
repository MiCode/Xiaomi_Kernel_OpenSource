// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
	.ctl_reg = 0x62048,
	.status_reg = 0x6204C,
	.xo_div4_cbcr = 0x62008,
};

static const char *const apss_cc_debug_mux_parent_names[] = {
	"measure_only_apcs_gold_post_acd_clk",
	"measure_only_apcs_gold_pre_acd_clk",
	"measure_only_apcs_goldapc2_post_acd_clk",
	"measure_only_apcs_goldapc2_pre_acd_clk",
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
	0x22,		/* measure_only_apcs_goldapc2_post_acd_clk */
	0x21,		/* measure_only_apcs_goldapc2_pre_acd_clk */
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
	0x8,		/* measure_only_apcs_goldapc2_post_acd_clk */
	0x8,		/* measure_only_apcs_goldapc2_pre_acd_clk */
	0x8,		/* measure_only_apcs_goldplus_post_acd_clk */
	0x8,		/* measure_only_apcs_goldplus_pre_acd_clk */
	0x4,		/* measure_only_apcs_l3_post_acd_clk */
	0x4,		/* measure_only_apcs_l3_pre_acd_clk */
	0x4,		/* measure_only_apcs_silver_post_acd_clk */
	0x4,		/* measure_only_apcs_silver_pre_acd_clk */
};

static struct clk_debug_mux apss_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x30100,
	.post_div_offset = 0x30104,
	.cbcr_offset = 0x30108,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = apss_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(apss_cc_debug_mux_sels),
	.pre_div_vals = apss_cc_debug_mux_pre_divs,
	.hw.init = &(const struct clk_init_data){
		.name = "apss_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apss_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apss_cc_debug_mux_parent_names),
	},
};

static const char *const cam_bist_mclk_cc_debug_mux_parent_names[] = {
	"cam_bist_mclk_cc_mclk0_clk",
	"cam_bist_mclk_cc_mclk1_clk",
	"cam_bist_mclk_cc_mclk2_clk",
	"cam_bist_mclk_cc_mclk3_clk",
	"cam_bist_mclk_cc_mclk4_clk",
	"cam_bist_mclk_cc_mclk5_clk",
	"cam_bist_mclk_cc_mclk6_clk",
	"cam_bist_mclk_cc_mclk7_clk",
	"measure_only_cam_bist_mclk_cc_sleep_clk",
};

static int cam_bist_mclk_cc_debug_mux_sels[] = {
	0x1,		/* cam_bist_mclk_cc_mclk0_clk */
	0x2,		/* cam_bist_mclk_cc_mclk1_clk */
	0x3,		/* cam_bist_mclk_cc_mclk2_clk */
	0x4,		/* cam_bist_mclk_cc_mclk3_clk */
	0x5,		/* cam_bist_mclk_cc_mclk4_clk */
	0x6,		/* cam_bist_mclk_cc_mclk5_clk */
	0x7,		/* cam_bist_mclk_cc_mclk6_clk */
	0x8,		/* cam_bist_mclk_cc_mclk7_clk */
	0x9,		/* measure_only_cam_bist_mclk_cc_sleep_clk */
};

static struct clk_debug_mux cam_bist_mclk_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x5000,
	.post_div_offset = 0x4100,
	.cbcr_offset = 0x4104,
	.src_sel_mask = 0x3F,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 16,
	.mux_sels = cam_bist_mclk_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(cam_bist_mclk_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "cam_bist_mclk_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = cam_bist_mclk_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(cam_bist_mclk_cc_debug_mux_parent_names),
	},
};

static const char *const cam_cc_debug_mux_parent_names[] = {
	"cam_cc_cam_top_ahb_clk",
	"cam_cc_cam_top_fast_ahb_clk",
	"cam_cc_camnoc_dcd_xo_clk",
	"cam_cc_camnoc_nrt_axi_clk",
	"cam_cc_camnoc_nrt_cre_clk",
	"cam_cc_camnoc_nrt_ipe_nps_clk",
	"cam_cc_camnoc_nrt_ofe_anchor_clk",
	"cam_cc_camnoc_nrt_ofe_hdr_clk",
	"cam_cc_camnoc_nrt_ofe_main_clk",
	"cam_cc_camnoc_rt_axi_clk",
	"cam_cc_camnoc_rt_ife_lite_clk",
	"cam_cc_camnoc_rt_tfe_0_bayer_clk",
	"cam_cc_camnoc_rt_tfe_0_main_clk",
	"cam_cc_camnoc_rt_tfe_1_bayer_clk",
	"cam_cc_camnoc_rt_tfe_1_main_clk",
	"cam_cc_camnoc_rt_tfe_2_bayer_clk",
	"cam_cc_camnoc_rt_tfe_2_main_clk",
	"cam_cc_camnoc_xo_clk",
	"cam_cc_cci_0_clk",
	"cam_cc_cci_1_clk",
	"cam_cc_cci_2_clk",
	"cam_cc_core_ahb_clk",
	"cam_cc_cre_ahb_clk",
	"cam_cc_cre_clk",
	"cam_cc_csi0phytimer_clk",
	"cam_cc_csi1phytimer_clk",
	"cam_cc_csi2phytimer_clk",
	"cam_cc_csi3phytimer_clk",
	"cam_cc_csi4phytimer_clk",
	"cam_cc_csi5phytimer_clk",
	"cam_cc_csid_clk",
	"cam_cc_csid_csiphy_rx_clk",
	"cam_cc_csiphy0_clk",
	"cam_cc_csiphy1_clk",
	"cam_cc_csiphy2_clk",
	"cam_cc_csiphy3_clk",
	"cam_cc_csiphy4_clk",
	"cam_cc_csiphy5_clk",
	"cam_cc_icp_0_ahb_clk",
	"cam_cc_icp_0_clk",
	"cam_cc_icp_1_ahb_clk",
	"cam_cc_icp_1_clk",
	"cam_cc_ife_lite_ahb_clk",
	"cam_cc_ife_lite_clk",
	"cam_cc_ife_lite_cphy_rx_clk",
	"cam_cc_ife_lite_csid_clk",
	"cam_cc_ipe_nps_ahb_clk",
	"cam_cc_ipe_nps_clk",
	"cam_cc_ipe_nps_fast_ahb_clk",
	"cam_cc_ipe_pps_clk",
	"cam_cc_ipe_pps_fast_ahb_clk",
	"cam_cc_jpeg_0_clk",
	"cam_cc_jpeg_1_clk",
	"cam_cc_ofe_ahb_clk",
	"cam_cc_ofe_anchor_clk",
	"cam_cc_ofe_anchor_fast_ahb_clk",
	"cam_cc_ofe_hdr_clk",
	"cam_cc_ofe_hdr_fast_ahb_clk",
	"cam_cc_ofe_main_clk",
	"cam_cc_ofe_main_fast_ahb_clk",
	"cam_cc_qdss_debug_clk",
	"cam_cc_qdss_debug_xo_clk",
	"cam_cc_tfe_0_bayer_clk",
	"cam_cc_tfe_0_bayer_fast_ahb_clk",
	"cam_cc_tfe_0_main_clk",
	"cam_cc_tfe_0_main_fast_ahb_clk",
	"cam_cc_tfe_1_bayer_clk",
	"cam_cc_tfe_1_bayer_fast_ahb_clk",
	"cam_cc_tfe_1_main_clk",
	"cam_cc_tfe_1_main_fast_ahb_clk",
	"cam_cc_tfe_2_bayer_clk",
	"cam_cc_tfe_2_bayer_fast_ahb_clk",
	"cam_cc_tfe_2_main_clk",
	"cam_cc_tfe_2_main_fast_ahb_clk",
	"measure_only_cam_cc_drv_ahb_clk",
	"measure_only_cam_cc_drv_xo_clk",
	"measure_only_cam_cc_gdsc_clk",
	"measure_only_cam_cc_sleep_clk",
};

static int cam_cc_debug_mux_sels[] = {
	0x67,		/* cam_cc_cam_top_ahb_clk */
	0x65,		/* cam_cc_cam_top_fast_ahb_clk */
	0x5E,		/* cam_cc_camnoc_dcd_xo_clk */
	0x5C,		/* cam_cc_camnoc_nrt_axi_clk */
	0x48,		/* cam_cc_camnoc_nrt_cre_clk */
	0x1E,		/* cam_cc_camnoc_nrt_ipe_nps_clk */
	0x16,		/* cam_cc_camnoc_nrt_ofe_anchor_clk */
	0x19,		/* cam_cc_camnoc_nrt_ofe_hdr_clk */
	0x13,		/* cam_cc_camnoc_nrt_ofe_main_clk */
	0x5A,		/* cam_cc_camnoc_rt_axi_clk */
	0x42,		/* cam_cc_camnoc_rt_ife_lite_clk */
	0x29,		/* cam_cc_camnoc_rt_tfe_0_bayer_clk */
	0x26,		/* cam_cc_camnoc_rt_tfe_0_main_clk */
	0x32,		/* cam_cc_camnoc_rt_tfe_1_bayer_clk */
	0x2F,		/* cam_cc_camnoc_rt_tfe_1_main_clk */
	0x3B,		/* cam_cc_camnoc_rt_tfe_2_bayer_clk */
	0x38,		/* cam_cc_camnoc_rt_tfe_2_main_clk */
	0x5F,		/* cam_cc_camnoc_xo_clk */
	0x55,		/* cam_cc_cci_0_clk */
	0x56,		/* cam_cc_cci_1_clk */
	0x57,		/* cam_cc_cci_2_clk */
	0x62,		/* cam_cc_core_ahb_clk */
	0x49,		/* cam_cc_cre_ahb_clk */
	0x47,		/* cam_cc_cre_clk */
	0x1,		/* cam_cc_csi0phytimer_clk */
	0x4,		/* cam_cc_csi1phytimer_clk */
	0x6,		/* cam_cc_csi2phytimer_clk */
	0x8,		/* cam_cc_csi3phytimer_clk */
	0xA,		/* cam_cc_csi4phytimer_clk */
	0xC,		/* cam_cc_csi5phytimer_clk */
	0x58,		/* cam_cc_csid_clk */
	0x3,		/* cam_cc_csid_csiphy_rx_clk */
	0x2,		/* cam_cc_csiphy0_clk */
	0x5,		/* cam_cc_csiphy1_clk */
	0x7,		/* cam_cc_csiphy2_clk */
	0x9,		/* cam_cc_csiphy3_clk */
	0xB,		/* cam_cc_csiphy4_clk */
	0xD,		/* cam_cc_csiphy5_clk */
	0x53,		/* cam_cc_icp_0_ahb_clk */
	0x4F,		/* cam_cc_icp_0_clk */
	0x54,		/* cam_cc_icp_1_ahb_clk */
	0x51,		/* cam_cc_icp_1_clk */
	0x46,		/* cam_cc_ife_lite_ahb_clk */
	0x40,		/* cam_cc_ife_lite_clk */
	0x45,		/* cam_cc_ife_lite_cphy_rx_clk */
	0x43,		/* cam_cc_ife_lite_csid_clk */
	0x22,		/* cam_cc_ipe_nps_ahb_clk */
	0x1C,		/* cam_cc_ipe_nps_clk */
	0x23,		/* cam_cc_ipe_nps_fast_ahb_clk */
	0x1F,		/* cam_cc_ipe_pps_clk */
	0x24,		/* cam_cc_ipe_pps_fast_ahb_clk */
	0x4A,		/* cam_cc_jpeg_0_clk */
	0x4B,		/* cam_cc_jpeg_1_clk */
	0x11,		/* cam_cc_ofe_ahb_clk */
	0x15,		/* cam_cc_ofe_anchor_clk */
	0xF,		/* cam_cc_ofe_anchor_fast_ahb_clk */
	0x18,		/* cam_cc_ofe_hdr_clk */
	0x10,		/* cam_cc_ofe_hdr_fast_ahb_clk */
	0x12,		/* cam_cc_ofe_main_clk */
	0xE,		/* cam_cc_ofe_main_fast_ahb_clk */
	0x60,		/* cam_cc_qdss_debug_clk */
	0x61,		/* cam_cc_qdss_debug_xo_clk */
	0x28,		/* cam_cc_tfe_0_bayer_clk */
	0x2D,		/* cam_cc_tfe_0_bayer_fast_ahb_clk */
	0x25,		/* cam_cc_tfe_0_main_clk */
	0x2C,		/* cam_cc_tfe_0_main_fast_ahb_clk */
	0x31,		/* cam_cc_tfe_1_bayer_clk */
	0x36,		/* cam_cc_tfe_1_bayer_fast_ahb_clk */
	0x2E,		/* cam_cc_tfe_1_main_clk */
	0x35,		/* cam_cc_tfe_1_main_fast_ahb_clk */
	0x3A,		/* cam_cc_tfe_2_bayer_clk */
	0x3F,		/* cam_cc_tfe_2_bayer_fast_ahb_clk */
	0x37,		/* cam_cc_tfe_2_main_clk */
	0x3E,		/* cam_cc_tfe_2_main_fast_ahb_clk */
	0x6A,		/* measure_only_cam_cc_drv_ahb_clk */
	0x69,		/* measure_only_cam_cc_drv_xo_clk */
	0x63,		/* measure_only_cam_cc_gdsc_clk */
	0x64,		/* measure_only_cam_cc_sleep_clk */
};

static struct clk_debug_mux cam_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x16000,
	.post_div_offset = 0x12004,
	.cbcr_offset = 0x12008,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = cam_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(cam_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "cam_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = cam_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(cam_cc_debug_mux_parent_names),
	},
};

static const char *const disp_cc_debug_mux_parent_names[] = {
	"disp_cc_esync0_clk",
	"disp_cc_esync1_clk",
	"disp_cc_mdss_accu_shift_clk",
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
	"disp_cc_mdss_hdmi_ahbm_clk",
	"disp_cc_mdss_hdmi_app_clk",
	"disp_cc_mdss_hdmi_crypto_clk",
	"disp_cc_mdss_hdmi_intf_clk",
	"disp_cc_mdss_hdmi_pclk_clk",
	"disp_cc_mdss_mdp1_clk",
	"disp_cc_mdss_mdp_clk",
	"disp_cc_mdss_mdp_lut1_clk",
	"disp_cc_mdss_mdp_lut_clk",
	"disp_cc_mdss_non_gdsc_ahb_clk",
	"disp_cc_mdss_pclk0_clk",
	"disp_cc_mdss_pclk1_clk",
	"disp_cc_mdss_pclk2_clk",
	"disp_cc_mdss_vsync1_clk",
	"disp_cc_mdss_vsync_clk",
	"disp_cc_osc_clk",
	"measure_only_disp_cc_mdss_rscc_ahb_clk",
	"measure_only_disp_cc_mdss_rscc_vsync_clk",
	"measure_only_disp_cc_sleep_clk",
	"measure_only_disp_cc_xo_clk",
};

static int disp_cc_debug_mux_sels[] = {
	0x36,		/* disp_cc_esync0_clk */
	0x37,		/* disp_cc_esync1_clk */
	0x4B,		/* disp_cc_mdss_accu_shift_clk */
	0x3B,		/* disp_cc_mdss_ahb1_clk */
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
	0x53,		/* disp_cc_mdss_hdmi_ahbm_clk */
	0x57,		/* disp_cc_mdss_hdmi_app_clk */
	0x56,		/* disp_cc_mdss_hdmi_crypto_clk */
	0x55,		/* disp_cc_mdss_hdmi_intf_clk */
	0x54,		/* disp_cc_mdss_hdmi_pclk_clk */
	0x38,		/* disp_cc_mdss_mdp1_clk */
	0x12,		/* disp_cc_mdss_mdp_clk */
	0x39,		/* disp_cc_mdss_mdp_lut1_clk */
	0x13,		/* disp_cc_mdss_mdp_lut_clk */
	0x3C,		/* disp_cc_mdss_non_gdsc_ahb_clk */
	0xF,		/* disp_cc_mdss_pclk0_clk */
	0x10,		/* disp_cc_mdss_pclk1_clk */
	0x11,		/* disp_cc_mdss_pclk2_clk */
	0x3A,		/* disp_cc_mdss_vsync1_clk */
	0x14,		/* disp_cc_mdss_vsync_clk */
	0x35,		/* disp_cc_osc_clk */
	0x3E,		/* measure_only_disp_cc_mdss_rscc_ahb_clk */
	0x3D,		/* measure_only_disp_cc_mdss_rscc_vsync_clk */
	0x4C,		/* measure_only_disp_cc_sleep_clk */
	0x4A,		/* measure_only_disp_cc_xo_clk */
};

static struct clk_debug_mux disp_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x11000,
	.post_div_offset = 0xD000,
	.cbcr_offset = 0xD004,
	.src_sel_mask = 0x1FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = disp_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(disp_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "disp_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = disp_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(disp_cc_debug_mux_parent_names),
	},
};

static const char *const eva_cc_debug_mux_parent_names[] = {
	"eva_cc_mvs0_clk",
	"eva_cc_mvs0_freerun_clk",
	"eva_cc_mvs0_shift_clk",
	"eva_cc_mvs0c_clk",
	"eva_cc_mvs0c_freerun_clk",
	"eva_cc_mvs0c_shift_clk",
	"measure_only_eva_cc_ahb_clk",
	"measure_only_eva_cc_sleep_clk",
	"measure_only_eva_cc_xo_clk",
};

static int eva_cc_debug_mux_sels[] = {
	0x4,		/* eva_cc_mvs0_clk */
	0x5,		/* eva_cc_mvs0_freerun_clk */
	0xA,		/* eva_cc_mvs0_shift_clk */
	0x1,		/* eva_cc_mvs0c_clk */
	0x2,		/* eva_cc_mvs0c_freerun_clk */
	0xB,		/* eva_cc_mvs0c_shift_clk */
	0x8,		/* measure_only_eva_cc_ahb_clk */
	0xC,		/* measure_only_eva_cc_sleep_clk */
	0x9,		/* measure_only_eva_cc_xo_clk */
};

static struct clk_debug_mux eva_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x9A4C,
	.post_div_offset = 0x80A8,
	.cbcr_offset = 0x80AC,
	.src_sel_mask = 0x3F,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 3,
	.mux_sels = eva_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(eva_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "eva_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = eva_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(eva_cc_debug_mux_parent_names),
	},
};

static const char *const gcc_debug_mux_parent_names[] = {
	"apss_cc_debug_mux",
	"cam_bist_mclk_cc_debug_mux",
	"cam_cc_debug_mux",
	"disp_cc_debug_mux",
	"eva_cc_debug_mux",
	"gcc_aggre_noc_pcie_axi_clk",
	"gcc_aggre_ufs_phy_axi_clk",
	"gcc_aggre_usb3_prim_axi_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_camera_hf_axi_clk",
	"gcc_camera_sf_axi_clk",
	"gcc_cfg_noc_pcie_anoc_ahb_clk",
	"gcc_cfg_noc_usb3_prim_axi_clk",
	"gcc_cnoc_pcie_sf_axi_clk",
	"gcc_ddrss_gpu_axi_clk",
	"gcc_ddrss_pcie_sf_qtb_clk",
	"gcc_disp_hf_axi_clk",
	"gcc_eva_axi0_clk",
	"gcc_eva_axi0c_clk",
	"gcc_eva_video_axi_dcht_pipe_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_gpu_gemnoc_gfx_clk",
	"gcc_gpu_gpll0_cph_clk_src",
	"gcc_gpu_gpll0_div_cph_clk_src",
	"gcc_pcie_0_aux_clk",
	"gcc_pcie_0_cfg_ahb_clk",
	"gcc_pcie_0_mstr_axi_clk",
	"gcc_pcie_0_phy_rchng_clk",
	"gcc_pcie_0_pipe_clk",
	"gcc_pcie_0_pipe_div2_clk",
	"gcc_pcie_0_slv_axi_clk",
	"gcc_pcie_0_slv_q2a_axi_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_qmip_camera_cmd_ahb_clk",
	"gcc_qmip_camera_nrt_ahb_clk",
	"gcc_qmip_camera_rt_ahb_clk",
	"gcc_qmip_gpu_ahb_clk",
	"gcc_qmip_pcie_ahb_clk",
	"gcc_qmip_video_cv_cpu_ahb_clk",
	"gcc_qmip_video_cvp_ahb_clk",
	"gcc_qmip_video_v_cpu_ahb_clk",
	"gcc_qmip_video_vcodec_ahb_clk",
	"gcc_qupv3_wrap1_core_2x_clk",
	"gcc_qupv3_wrap1_core_clk",
	"gcc_qupv3_wrap1_qspi_ref_clk",
	"gcc_qupv3_wrap1_s0_clk",
	"gcc_qupv3_wrap1_s1_clk",
	"gcc_qupv3_wrap1_s2_clk",
	"gcc_qupv3_wrap1_s3_clk",
	"gcc_qupv3_wrap1_s4_clk",
	"gcc_qupv3_wrap1_s5_clk",
	"gcc_qupv3_wrap1_s6_clk",
	"gcc_qupv3_wrap1_s7_clk",
	"gcc_qupv3_wrap2_core_2x_clk",
	"gcc_qupv3_wrap2_core_clk",
	"gcc_qupv3_wrap2_ibi_ctrl_2_clk",
	"gcc_qupv3_wrap2_ibi_ctrl_3_clk",
	"gcc_qupv3_wrap2_s0_clk",
	"gcc_qupv3_wrap2_s1_clk",
	"gcc_qupv3_wrap2_s2_clk",
	"gcc_qupv3_wrap2_s3_clk",
	"gcc_qupv3_wrap2_s4_clk",
	"gcc_qupv3_wrap2_s5_clk",
	"gcc_qupv3_wrap2_s6_clk",
	"gcc_qupv3_wrap2_s7_clk",
	"gcc_qupv3_wrap_1_m_ahb_clk",
	"gcc_qupv3_wrap_1_s_ahb_clk",
	"gcc_qupv3_wrap_2_ibi_2_ahb_clk",
	"gcc_qupv3_wrap_2_ibi_3_ahb_clk",
	"gcc_qupv3_wrap_2_m_ahb_clk",
	"gcc_qupv3_wrap_2_s_ahb_clk",
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
	"gcc_usb30_prim_atb_clk",
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
	"measure_only_gcc_cam_bist_mclk_ahb_clk",
	"measure_only_gcc_camera_ahb_clk",
	"measure_only_gcc_camera_xo_clk",
	"measure_only_gcc_disp_ahb_clk",
	"measure_only_gcc_eva_ahb_clk",
	"measure_only_gcc_eva_xo_clk",
	"measure_only_gcc_gpu_cfg_ahb_clk",
	"measure_only_gcc_pcie_rscc_cfg_ahb_clk",
	"measure_only_gcc_pcie_rscc_xo_clk",
	"measure_only_gcc_video_ahb_clk",
	"measure_only_gcc_video_xo_clk",
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
	0x15C,		/* apss_cc_debug_mux */
	0x8B,		/* cam_bist_mclk_cc_debug_mux */
	0x89,		/* cam_cc_debug_mux */
	0x8E,		/* disp_cc_debug_mux */
	0x9C,		/* eva_cc_debug_mux */
	0x4E,		/* gcc_aggre_noc_pcie_axi_clk */
	0x50,		/* gcc_aggre_ufs_phy_axi_clk */
	0x4F,		/* gcc_aggre_usb3_prim_axi_clk */
	0x102,		/* gcc_boot_rom_ahb_clk */
	0x84,		/* gcc_camera_hf_axi_clk */
	0x86,		/* gcc_camera_sf_axi_clk */
	0x3B,		/* gcc_cfg_noc_pcie_anoc_ahb_clk */
	0x20,		/* gcc_cfg_noc_usb3_prim_axi_clk */
	0x1A,		/* gcc_cnoc_pcie_sf_axi_clk */
	0x11F,		/* gcc_ddrss_gpu_axi_clk */
	0x120,		/* gcc_ddrss_pcie_sf_qtb_clk */
	0x8D,		/* gcc_disp_hf_axi_clk */
	0x99,		/* gcc_eva_axi0_clk */
	0x9A,		/* gcc_eva_axi0c_clk */
	0x9D,		/* gcc_eva_video_axi_dcht_pipe_clk */
	0x16D,		/* gcc_gp1_clk */
	0x16E,		/* gcc_gp2_clk */
	0x16F,		/* gcc_gp3_clk */
	0x1AE,		/* gcc_gpu_gemnoc_gfx_clk */
	0x1B0,		/* gcc_gpu_gpll0_cph_clk_src */
	0x1B1,		/* gcc_gpu_gpll0_div_cph_clk_src */
	0x175,		/* gcc_pcie_0_aux_clk */
	0x174,		/* gcc_pcie_0_cfg_ahb_clk */
	0x173,		/* gcc_pcie_0_mstr_axi_clk */
	0x177,		/* gcc_pcie_0_phy_rchng_clk */
	0x176,		/* gcc_pcie_0_pipe_clk */
	0x178,		/* gcc_pcie_0_pipe_div2_clk */
	0x172,		/* gcc_pcie_0_slv_axi_clk */
	0x171,		/* gcc_pcie_0_slv_q2a_axi_clk */
	0xF3,		/* gcc_pdm2_clk */
	0xF1,		/* gcc_pdm_ahb_clk */
	0xF2,		/* gcc_pdm_xo4_clk */
	0x83,		/* gcc_qmip_camera_cmd_ahb_clk */
	0x81,		/* gcc_qmip_camera_nrt_ahb_clk */
	0x82,		/* gcc_qmip_camera_rt_ahb_clk */
	0x1AB,		/* gcc_qmip_gpu_ahb_clk */
	0x170,		/* gcc_qmip_pcie_ahb_clk */
	0x93,		/* gcc_qmip_video_cv_cpu_ahb_clk */
	0x90,		/* gcc_qmip_video_cvp_ahb_clk */
	0x92,		/* gcc_qmip_video_v_cpu_ahb_clk */
	0x91,		/* gcc_qmip_video_vcodec_ahb_clk */
	0xD7,		/* gcc_qupv3_wrap1_core_2x_clk */
	0xD6,		/* gcc_qupv3_wrap1_core_clk */
	0xE0,		/* gcc_qupv3_wrap1_qspi_ref_clk */
	0xD8,		/* gcc_qupv3_wrap1_s0_clk */
	0xD9,		/* gcc_qupv3_wrap1_s1_clk */
	0xDA,		/* gcc_qupv3_wrap1_s2_clk */
	0xDB,		/* gcc_qupv3_wrap1_s3_clk */
	0xDC,		/* gcc_qupv3_wrap1_s4_clk */
	0xDD,		/* gcc_qupv3_wrap1_s5_clk */
	0xDE,		/* gcc_qupv3_wrap1_s6_clk */
	0xDF,		/* gcc_qupv3_wrap1_s7_clk */
	0xE4,		/* gcc_qupv3_wrap2_core_2x_clk */
	0xE3,		/* gcc_qupv3_wrap2_core_clk */
	0xEF,		/* gcc_qupv3_wrap2_ibi_ctrl_2_clk */
	0xF0,		/* gcc_qupv3_wrap2_ibi_ctrl_3_clk */
	0xE5,		/* gcc_qupv3_wrap2_s0_clk */
	0xE6,		/* gcc_qupv3_wrap2_s1_clk */
	0xE7,		/* gcc_qupv3_wrap2_s2_clk */
	0xE8,		/* gcc_qupv3_wrap2_s3_clk */
	0xE9,		/* gcc_qupv3_wrap2_s4_clk */
	0xEA,		/* gcc_qupv3_wrap2_s5_clk */
	0xEB,		/* gcc_qupv3_wrap2_s6_clk */
	0xEC,		/* gcc_qupv3_wrap2_s7_clk */
	0xD4,		/* gcc_qupv3_wrap_1_m_ahb_clk */
	0xD5,		/* gcc_qupv3_wrap_1_s_ahb_clk */
	0xED,		/* gcc_qupv3_wrap_2_ibi_2_ahb_clk */
	0xEE,		/* gcc_qupv3_wrap_2_ibi_3_ahb_clk */
	0xE1,		/* gcc_qupv3_wrap_2_m_ahb_clk */
	0xE2,		/* gcc_qupv3_wrap_2_s_ahb_clk */
	0xD2,		/* gcc_sdcc2_ahb_clk */
	0xD1,		/* gcc_sdcc2_apps_clk */
	0x17C,		/* gcc_ufs_phy_ahb_clk */
	0x17B,		/* gcc_ufs_phy_axi_clk */
	0x182,		/* gcc_ufs_phy_ice_core_clk */
	0x183,		/* gcc_ufs_phy_phy_aux_clk */
	0x17E,		/* gcc_ufs_phy_rx_symbol_0_clk */
	0x184,		/* gcc_ufs_phy_rx_symbol_1_clk */
	0x17D,		/* gcc_ufs_phy_tx_symbol_0_clk */
	0x181,		/* gcc_ufs_phy_unipro_core_clk */
	0xCD,		/* gcc_usb30_prim_atb_clk */
	0xC4,		/* gcc_usb30_prim_master_clk */
	0xC6,		/* gcc_usb30_prim_mock_utmi_clk */
	0xC5,		/* gcc_usb30_prim_sleep_clk */
	0xC7,		/* gcc_usb3_prim_phy_aux_clk */
	0xC8,		/* gcc_usb3_prim_phy_com_aux_clk */
	0xC9,		/* gcc_usb3_prim_phy_pipe_clk */
	0x94,		/* gcc_video_axi0_clk */
	0x95,		/* gcc_video_axi1_clk */
	0x1AD,		/* gpu_cc_debug_mux */
	0x12E,		/* mc_cc_debug_mux */
	0x17,		/* measure_only_cnoc_clk */
	0x8A,		/* measure_only_gcc_cam_bist_mclk_ahb_clk */
	0x80,		/* measure_only_gcc_camera_ahb_clk */
	0x88,		/* measure_only_gcc_camera_xo_clk */
	0x8C,		/* measure_only_gcc_disp_ahb_clk */
	0x98,		/* measure_only_gcc_eva_ahb_clk */
	0x9B,		/* measure_only_gcc_eva_xo_clk */
	0x1AA,		/* measure_only_gcc_gpu_cfg_ahb_clk */
	0x1C5,		/* measure_only_gcc_pcie_rscc_cfg_ahb_clk */
	0x1C6,		/* measure_only_gcc_pcie_rscc_xo_clk */
	0x8F,		/* measure_only_gcc_video_ahb_clk */
	0x96,		/* measure_only_gcc_video_xo_clk */
	0x198,		/* measure_only_ipa_2x_clk */
	0x124,		/* measure_only_memnoc_clk */
	0x179,		/* measure_only_pcie_0_pipe_clk */
	0xC,		/* measure_only_snoc_clk */
	0x180,		/* measure_only_ufs_phy_rx_symbol_0_clk */
	0x186,		/* measure_only_ufs_phy_rx_symbol_1_clk */
	0x17F,		/* measure_only_ufs_phy_tx_symbol_0_clk */
	0xCE,		/* measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk */
	0x97,		/* video_cc_debug_mux */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x62024,
	.post_div_offset = 0x62000,
	.cbcr_offset = 0x62004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 2,
	.mux_sels = gcc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(gcc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "gcc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gcc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gcc_debug_mux_parent_names),
	},
};

static const char *const gpu_cc_debug_mux_parent_names[] = {
	"gpu_cc_ahb_clk",
	"gpu_cc_cx_accu_shift_clk",
	"gpu_cc_cx_gmu_clk",
	"gpu_cc_cxo_clk",
	"gpu_cc_demet_clk",
	"gpu_cc_dpm_clk",
	"gpu_cc_freq_measure_clk",
	"gpu_cc_gx_accu_shift_clk",
	"gpu_cc_gx_gmu_clk",
	"gpu_cc_hub_aon_clk",
	"gpu_cc_hub_cx_int_clk",
	"gpu_cc_memnoc_gfx_clk",
	"gpu_cc_sleep_clk",
	"gx_clkctl_debug_mux",
	"measure_only_gpu_cc_cb_clk",
	"measure_only_gpu_cc_cxo_aon_clk",
	"measure_only_gpu_cc_rscc_hub_aon_clk",
	"measure_only_gpu_cc_rscc_xo_aon_clk",
};

static int gpu_cc_debug_mux_sels[] = {
	0x14,		/* gpu_cc_ahb_clk */
	0x20,		/* gpu_cc_cx_accu_shift_clk */
	0x1A,		/* gpu_cc_cx_gmu_clk */
	0x1B,		/* gpu_cc_cxo_clk */
	0x10,		/* gpu_cc_demet_clk */
	0x21,		/* gpu_cc_dpm_clk */
	0xF,		/* gpu_cc_freq_measure_clk */
	0x12,		/* gpu_cc_gx_accu_shift_clk */
	0x11,		/* gpu_cc_gx_gmu_clk */
	0x26,		/* gpu_cc_hub_aon_clk */
	0x1C,		/* gpu_cc_hub_cx_int_clk */
	0x1D,		/* gpu_cc_memnoc_gfx_clk */
	0x18,		/* gpu_cc_sleep_clk */
	0xB,		/* gx_clkctl_debug_mux */
	0x24,		/* measure_only_gpu_cc_cb_clk */
	0xE,		/* measure_only_gpu_cc_cxo_aon_clk */
	0x25,		/* measure_only_gpu_cc_rscc_hub_aon_clk */
	0xD,		/* measure_only_gpu_cc_rscc_xo_aon_clk */
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
	.hw.init = &(const struct clk_init_data){
		.name = "gpu_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gpu_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gpu_cc_debug_mux_parent_names),
	},
};

static const char *const gx_clkctl_debug_mux_parent_names[] = {
	"measure_only_gx_clkctl_acd_ahb_ff_clk",
	"measure_only_gx_clkctl_acd_gfx3d_clk",
	"measure_only_gx_clkctl_ahb_ff_clk",
	"measure_only_gx_clkctl_demet_clk",
	"measure_only_gx_clkctl_gx_accu_clk",
	"measure_only_gx_clkctl_gx_gfx3d_clk",
	"measure_only_gx_clkctl_gx_gfx3d_rdvm_clk",
	"measure_only_gx_clkctl_mnd1x_gfx3d_clk",
	"measure_only_gx_clkctl_rcg_ahb_ff_clk",
};

static int gx_clkctl_debug_mux_sels[] = {
	0xD,		/* measure_only_gx_clkctl_acd_ahb_ff_clk */
	0x8,		/* measure_only_gx_clkctl_acd_gfx3d_clk */
	0xC,		/* measure_only_gx_clkctl_ahb_ff_clk */
	0x2,		/* measure_only_gx_clkctl_demet_clk */
	0xA,		/* measure_only_gx_clkctl_gx_accu_clk */
	0x3,		/* measure_only_gx_clkctl_gx_gfx3d_clk */
	0x6,		/* measure_only_gx_clkctl_gx_gfx3d_rdvm_clk */
	0x7,		/* measure_only_gx_clkctl_mnd1x_gfx3d_clk */
	0xE,		/* measure_only_gx_clkctl_rcg_ahb_ff_clk */
};

static struct clk_debug_mux gx_clkctl_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x4144,
	.post_div_offset = U32_MAX,
	.cbcr_offset = 0x4088,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0x1,
	.post_div_shift = 0,
	.post_div_val = 1,
	.mux_sels = gx_clkctl_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(gx_clkctl_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "gx_clkctl_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gx_clkctl_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gx_clkctl_debug_mux_parent_names),
	},
};

static const char *const video_cc_debug_mux_parent_names[] = {
	"measure_only_video_cc_ahb_clk",
	"measure_only_video_cc_sleep_clk",
	"measure_only_video_cc_xo_clk",
	"video_cc_mvs0_clk",
	"video_cc_mvs0_shift_clk",
	"video_cc_mvs0c_clk",
	"video_cc_mvs0c_shift_clk",
};

static int video_cc_debug_mux_sels[] = {
	0x17,		/* measure_only_video_cc_ahb_clk */
	0xC,		/* measure_only_video_cc_sleep_clk */
	0xB,		/* measure_only_video_cc_xo_clk */
	0x3,		/* video_cc_mvs0_clk */
	0xD,		/* video_cc_mvs0_shift_clk */
	0x1,		/* video_cc_mvs0c_clk */
	0xE,		/* video_cc_mvs0c_shift_clk */
};

static struct clk_debug_mux video_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x9A4C,
	.post_div_offset = 0x80E0,
	.cbcr_offset = 0x80E4,
	.src_sel_mask = 0x3F,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 3,
	.mux_sels = video_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(video_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
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
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
	{ .mux = &video_cc_debug_mux, .regmap_name = "qcom,videocc" },
	{ .mux = &gx_clkctl_debug_mux, .regmap_name = "qcom,gxclkctl" },
	{ .mux = &gpu_cc_debug_mux, .regmap_name = "qcom,gpucc" },
	{ .mux = &eva_cc_debug_mux, .regmap_name = "qcom,evacc" },
	{ .mux = &disp_cc_debug_mux, .regmap_name = "qcom,dispcc" },
	{ .mux = &cam_cc_debug_mux, .regmap_name = "qcom,camcc" },
	{ .mux = &cam_bist_mclk_cc_debug_mux, .regmap_name = "qcom,cambistmclkcc" },
	{ .mux = &apss_cc_debug_mux, .regmap_name = "qcom,apsscc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
};

static struct clk_dummy measure_only_apcs_gold_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_gold_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_gold_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_gold_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_goldapc2_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_goldapc2_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_goldapc2_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_goldapc2_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_goldplus_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_goldplus_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_goldplus_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_goldplus_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_l3_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_l3_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_l3_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_l3_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_silver_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_silver_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_silver_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_silver_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cam_bist_mclk_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cam_bist_mclk_cc_sleep_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cam_cc_drv_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cam_cc_drv_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cam_cc_drv_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cam_cc_drv_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cam_cc_gdsc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cam_cc_gdsc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cam_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cam_cc_sleep_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cnoc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_disp_cc_mdss_rscc_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_disp_cc_mdss_rscc_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_disp_cc_mdss_rscc_vsync_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_disp_cc_mdss_rscc_vsync_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_disp_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_disp_cc_sleep_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_disp_cc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_disp_cc_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_eva_cc_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_eva_cc_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_eva_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_eva_cc_sleep_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_eva_cc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_eva_cc_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_cam_bist_mclk_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_cam_bist_mclk_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_camera_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_camera_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_camera_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_camera_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_disp_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_disp_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_eva_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_eva_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_eva_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_eva_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_gpu_cfg_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_gpu_cfg_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_rscc_cfg_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_pcie_rscc_cfg_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_rscc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_pcie_rscc_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_video_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_video_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_video_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_video_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_cb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_cb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_cxo_aon_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_cxo_aon_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_rscc_hub_aon_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_rscc_hub_aon_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_rscc_xo_aon_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_rscc_xo_aon_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gx_clkctl_acd_ahb_ff_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gx_clkctl_acd_ahb_ff_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gx_clkctl_acd_gfx3d_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gx_clkctl_acd_gfx3d_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gx_clkctl_ahb_ff_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gx_clkctl_ahb_ff_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gx_clkctl_demet_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gx_clkctl_demet_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gx_clkctl_gx_accu_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gx_clkctl_gx_accu_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gx_clkctl_gx_gfx3d_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gx_clkctl_gx_gfx3d_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gx_clkctl_gx_gfx3d_rdvm_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gx_clkctl_gx_gfx3d_rdvm_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gx_clkctl_mnd1x_gfx3d_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gx_clkctl_mnd1x_gfx3d_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gx_clkctl_rcg_ahb_ff_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gx_clkctl_rcg_ahb_ff_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ipa_2x_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_ipa_2x_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_mccc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_mccc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_memnoc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_memnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcie_0_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie_0_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_snoc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_snoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ufs_phy_rx_symbol_0_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_ufs_phy_rx_symbol_0_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ufs_phy_rx_symbol_1_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_ufs_phy_rx_symbol_1_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ufs_phy_tx_symbol_0_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_ufs_phy_tx_symbol_0_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_video_cc_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_video_cc_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_video_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_video_cc_sleep_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_video_cc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_video_cc_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_tuna_hws[] = {
	&measure_only_apcs_gold_post_acd_clk.hw,
	&measure_only_apcs_gold_pre_acd_clk.hw,
	&measure_only_apcs_goldapc2_post_acd_clk.hw,
	&measure_only_apcs_goldapc2_pre_acd_clk.hw,
	&measure_only_apcs_goldplus_post_acd_clk.hw,
	&measure_only_apcs_goldplus_pre_acd_clk.hw,
	&measure_only_apcs_l3_post_acd_clk.hw,
	&measure_only_apcs_l3_pre_acd_clk.hw,
	&measure_only_apcs_silver_post_acd_clk.hw,
	&measure_only_apcs_silver_pre_acd_clk.hw,
	&measure_only_cam_bist_mclk_cc_sleep_clk.hw,
	&measure_only_cam_cc_drv_ahb_clk.hw,
	&measure_only_cam_cc_drv_xo_clk.hw,
	&measure_only_cam_cc_gdsc_clk.hw,
	&measure_only_cam_cc_sleep_clk.hw,
	&measure_only_cnoc_clk.hw,
	&measure_only_disp_cc_mdss_rscc_ahb_clk.hw,
	&measure_only_disp_cc_mdss_rscc_vsync_clk.hw,
	&measure_only_disp_cc_sleep_clk.hw,
	&measure_only_disp_cc_xo_clk.hw,
	&measure_only_eva_cc_ahb_clk.hw,
	&measure_only_eva_cc_sleep_clk.hw,
	&measure_only_eva_cc_xo_clk.hw,
	&measure_only_gcc_cam_bist_mclk_ahb_clk.hw,
	&measure_only_gcc_camera_ahb_clk.hw,
	&measure_only_gcc_camera_xo_clk.hw,
	&measure_only_gcc_disp_ahb_clk.hw,
	&measure_only_gcc_eva_ahb_clk.hw,
	&measure_only_gcc_eva_xo_clk.hw,
	&measure_only_gcc_gpu_cfg_ahb_clk.hw,
	&measure_only_gcc_pcie_rscc_cfg_ahb_clk.hw,
	&measure_only_gcc_pcie_rscc_xo_clk.hw,
	&measure_only_gcc_video_ahb_clk.hw,
	&measure_only_gcc_video_xo_clk.hw,
	&measure_only_gpu_cc_cb_clk.hw,
	&measure_only_gpu_cc_cxo_aon_clk.hw,
	&measure_only_gpu_cc_rscc_hub_aon_clk.hw,
	&measure_only_gpu_cc_rscc_xo_aon_clk.hw,
	&measure_only_gx_clkctl_acd_ahb_ff_clk.hw,
	&measure_only_gx_clkctl_acd_gfx3d_clk.hw,
	&measure_only_gx_clkctl_ahb_ff_clk.hw,
	&measure_only_gx_clkctl_demet_clk.hw,
	&measure_only_gx_clkctl_gx_accu_clk.hw,
	&measure_only_gx_clkctl_gx_gfx3d_clk.hw,
	&measure_only_gx_clkctl_gx_gfx3d_rdvm_clk.hw,
	&measure_only_gx_clkctl_mnd1x_gfx3d_clk.hw,
	&measure_only_gx_clkctl_rcg_ahb_ff_clk.hw,
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
	&measure_only_video_cc_sleep_clk.hw,
	&measure_only_video_cc_xo_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,tuna-debugcc" },
	{ }
};

static int clk_debug_tuna_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0, i;

	BUILD_BUG_ON(ARRAY_SIZE(apss_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(apss_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(cam_bist_mclk_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(cam_bist_mclk_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(cam_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(cam_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(disp_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(disp_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(eva_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(eva_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) != ARRAY_SIZE(gcc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gpu_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(gpu_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gx_clkctl_debug_mux_parent_names) !=
		ARRAY_SIZE(gx_clkctl_debug_mux_sels));
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
			ret = map_debug_bases(pdev, mux_list[i].regmap_name,
					      mux_list[i].mux);
			if (ret == -EBADR)
				continue;
			else if (ret)
				return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(debugcc_tuna_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_tuna_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%ld)\n",
				qcom_clk_hw_get_name(debugcc_tuna_hws[i]),
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

	return ret;
}

static struct platform_driver clk_debug_driver = {
	.probe = clk_debug_tuna_probe,
	.driver = {
		.name = "tuna-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_tuna_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_tuna_init);

MODULE_DESCRIPTION("QTI DEBUG CC TUNA Driver");
MODULE_LICENSE("GPL");

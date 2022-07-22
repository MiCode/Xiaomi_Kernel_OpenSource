// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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
	"measure_only_apcs_l3_post_acd_clk",
	"measure_only_apcs_l3_pre_acd_clk",
	"measure_only_apcs_silver_post_acd_clk",
	"measure_only_apcs_silver_pre_acd_clk",
};

static int apss_cc_debug_mux_sels[] = {
	0x41,		/* measure_only_apcs_l3_post_acd_clk */
	0x45,		/* measure_only_apcs_l3_pre_acd_clk */
	0x21,		/* measure_only_apcs_silver_post_acd_clk */
	0x44,		/* measure_only_apcs_silver_pre_acd_clk */
};

static int apss_cc_debug_mux_pre_divs[] = {
	0x4,		/* measure_only_apcs_l3_post_acd_clk */
	0x10,		/* measure_only_apcs_l3_pre_acd_clk */
	0x4,		/* measure_only_apcs_silver_post_acd_clk */
	0x10,		/* measure_only_apcs_silver_pre_acd_clk */
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
	.hw.init = &(const struct clk_init_data){
		.name = "apss_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apss_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apss_cc_debug_mux_parent_names),
	},
};

static const char *const ecpri_cc_debug_mux_parent_names[] = {
	"ecpri_cc_ecpri_cg_clk",
	"ecpri_cc_ecpri_dma_clk",
	"ecpri_cc_ecpri_dma_noc_clk",
	"ecpri_cc_ecpri_fast_clk",
	"ecpri_cc_ecpri_fast_div2_clk",
	"ecpri_cc_ecpri_fast_div2_noc_clk",
	"ecpri_cc_ecpri_fr_clk",
	"ecpri_cc_ecpri_oran_div2_clk",
	"ecpri_cc_eth_100g_c2c0_udp_fifo_clk",
	"ecpri_cc_eth_100g_c2c1_udp_fifo_clk",
	"ecpri_cc_eth_100g_c2c_0_hm_ff_0_clk",
	"ecpri_cc_eth_100g_c2c_0_hm_ff_1_clk",
	"ecpri_cc_eth_100g_c2c_hm_macsec_clk",
	"ecpri_cc_eth_100g_dbg_c2c_hm_ff_0_clk",
	"ecpri_cc_eth_100g_dbg_c2c_hm_ff_1_clk",
	"ecpri_cc_eth_100g_dbg_c2c_udp_fifo_clk",
	"ecpri_cc_eth_100g_fh_0_hm_ff_0_clk",
	"ecpri_cc_eth_100g_fh_0_hm_ff_1_clk",
	"ecpri_cc_eth_100g_fh_0_hm_ff_2_clk",
	"ecpri_cc_eth_100g_fh_0_hm_ff_3_clk",
	"ecpri_cc_eth_100g_fh_0_udp_fifo_clk",
	"ecpri_cc_eth_100g_fh_1_hm_ff_0_clk",
	"ecpri_cc_eth_100g_fh_1_hm_ff_1_clk",
	"ecpri_cc_eth_100g_fh_1_hm_ff_2_clk",
	"ecpri_cc_eth_100g_fh_1_hm_ff_3_clk",
	"ecpri_cc_eth_100g_fh_1_udp_fifo_clk",
	"ecpri_cc_eth_100g_fh_2_hm_ff_0_clk",
	"ecpri_cc_eth_100g_fh_2_hm_ff_1_clk",
	"ecpri_cc_eth_100g_fh_2_hm_ff_2_clk",
	"ecpri_cc_eth_100g_fh_2_hm_ff_3_clk",
	"ecpri_cc_eth_100g_fh_2_udp_fifo_clk",
	"ecpri_cc_eth_100g_fh_macsec_0_clk",
	"ecpri_cc_eth_100g_fh_macsec_1_clk",
	"ecpri_cc_eth_100g_fh_macsec_2_clk",
	"ecpri_cc_eth_100g_mac_c2c_hm_ref_clk",
	"ecpri_cc_eth_100g_mac_dbg_c2c_hm_ref_clk",
	"ecpri_cc_eth_100g_mac_fh0_hm_ref_clk",
	"ecpri_cc_eth_100g_mac_fh1_hm_ref_clk",
	"ecpri_cc_eth_100g_mac_fh2_hm_ref_clk",
	"ecpri_cc_eth_dbg_nfapi_axi_clk",
	"ecpri_cc_eth_dbg_noc_axi_clk",
	"ecpri_cc_eth_phy_0_ock_sram_clk",
	"ecpri_cc_eth_phy_1_ock_sram_clk",
	"ecpri_cc_eth_phy_2_ock_sram_clk",
	"ecpri_cc_eth_phy_3_ock_sram_clk",
	"ecpri_cc_eth_phy_4_ock_sram_clk",
	"ecpri_cc_mss_emac_clk",
	"ecpri_cc_mss_oran_clk",
	"ecpri_cc_phy0_lane0_rx_clk",
	"ecpri_cc_phy0_lane0_tx_clk",
	"ecpri_cc_phy0_lane1_rx_clk",
	"ecpri_cc_phy0_lane1_tx_clk",
	"ecpri_cc_phy0_lane2_rx_clk",
	"ecpri_cc_phy0_lane2_tx_clk",
	"ecpri_cc_phy0_lane3_rx_clk",
	"ecpri_cc_phy0_lane3_tx_clk",
	"ecpri_cc_phy1_lane0_rx_clk",
	"ecpri_cc_phy1_lane0_tx_clk",
	"ecpri_cc_phy1_lane1_rx_clk",
	"ecpri_cc_phy1_lane1_tx_clk",
	"ecpri_cc_phy1_lane2_rx_clk",
	"ecpri_cc_phy1_lane2_tx_clk",
	"ecpri_cc_phy1_lane3_rx_clk",
	"ecpri_cc_phy1_lane3_tx_clk",
	"ecpri_cc_phy2_lane0_rx_clk",
	"ecpri_cc_phy2_lane0_tx_clk",
	"ecpri_cc_phy2_lane1_rx_clk",
	"ecpri_cc_phy2_lane1_tx_clk",
	"ecpri_cc_phy2_lane2_rx_clk",
	"ecpri_cc_phy2_lane2_tx_clk",
	"ecpri_cc_phy2_lane3_rx_clk",
	"ecpri_cc_phy2_lane3_tx_clk",
	"ecpri_cc_phy3_lane0_rx_clk",
	"ecpri_cc_phy3_lane0_tx_clk",
	"ecpri_cc_phy3_lane1_rx_clk",
	"ecpri_cc_phy3_lane1_tx_clk",
	"ecpri_cc_phy3_lane2_rx_clk",
	"ecpri_cc_phy3_lane2_tx_clk",
	"ecpri_cc_phy3_lane3_rx_clk",
	"ecpri_cc_phy3_lane3_tx_clk",
	"ecpri_cc_phy4_lane0_rx_clk",
	"ecpri_cc_phy4_lane0_tx_clk",
	"ecpri_cc_phy4_lane1_rx_clk",
	"ecpri_cc_phy4_lane1_tx_clk",
	"ecpri_cc_phy4_lane2_rx_clk",
	"ecpri_cc_phy4_lane2_tx_clk",
	"ecpri_cc_phy4_lane3_rx_clk",
	"ecpri_cc_phy4_lane3_tx_clk",
};

static int ecpri_cc_debug_mux_sels[] = {
	0x38,		/* ecpri_cc_ecpri_cg_clk */
	0x3E,		/* ecpri_cc_ecpri_dma_clk */
	0x42,		/* ecpri_cc_ecpri_dma_noc_clk */
	0x3A,		/* ecpri_cc_ecpri_fast_clk */
	0x3B,		/* ecpri_cc_ecpri_fast_div2_clk */
	0x43,		/* ecpri_cc_ecpri_fast_div2_noc_clk */
	0x36,		/* ecpri_cc_ecpri_fr_clk */
	0x3C,		/* ecpri_cc_ecpri_oran_div2_clk */
	0x2C,		/* ecpri_cc_eth_100g_c2c0_udp_fifo_clk */
	0x2D,		/* ecpri_cc_eth_100g_c2c1_udp_fifo_clk */
	0x28,		/* ecpri_cc_eth_100g_c2c_0_hm_ff_0_clk */
	0x29,		/* ecpri_cc_eth_100g_c2c_0_hm_ff_1_clk */
	0x26,		/* ecpri_cc_eth_100g_c2c_hm_macsec_clk */
	0x2F,		/* ecpri_cc_eth_100g_dbg_c2c_hm_ff_0_clk */
	0x30,		/* ecpri_cc_eth_100g_dbg_c2c_hm_ff_1_clk */
	0x33,		/* ecpri_cc_eth_100g_dbg_c2c_udp_fifo_clk */
	0xA,		/* ecpri_cc_eth_100g_fh_0_hm_ff_0_clk */
	0xB,		/* ecpri_cc_eth_100g_fh_0_hm_ff_1_clk */
	0xC,		/* ecpri_cc_eth_100g_fh_0_hm_ff_2_clk */
	0xD,		/* ecpri_cc_eth_100g_fh_0_hm_ff_3_clk */
	0x10,		/* ecpri_cc_eth_100g_fh_0_udp_fifo_clk */
	0x14,		/* ecpri_cc_eth_100g_fh_1_hm_ff_0_clk */
	0x15,		/* ecpri_cc_eth_100g_fh_1_hm_ff_1_clk */
	0x16,		/* ecpri_cc_eth_100g_fh_1_hm_ff_2_clk */
	0x17,		/* ecpri_cc_eth_100g_fh_1_hm_ff_3_clk */
	0x1A,		/* ecpri_cc_eth_100g_fh_1_udp_fifo_clk */
	0x1E,		/* ecpri_cc_eth_100g_fh_2_hm_ff_0_clk */
	0x1F,		/* ecpri_cc_eth_100g_fh_2_hm_ff_1_clk */
	0x20,		/* ecpri_cc_eth_100g_fh_2_hm_ff_2_clk */
	0x21,		/* ecpri_cc_eth_100g_fh_2_hm_ff_3_clk */
	0x24,		/* ecpri_cc_eth_100g_fh_2_udp_fifo_clk */
	0x8,		/* ecpri_cc_eth_100g_fh_macsec_0_clk */
	0x12,		/* ecpri_cc_eth_100g_fh_macsec_1_clk */
	0x1C,		/* ecpri_cc_eth_100g_fh_macsec_2_clk */
	0x2A,		/* ecpri_cc_eth_100g_mac_c2c_hm_ref_clk */
	0x31,		/* ecpri_cc_eth_100g_mac_dbg_c2c_hm_ref_clk */
	0xE,		/* ecpri_cc_eth_100g_mac_fh0_hm_ref_clk */
	0x18,		/* ecpri_cc_eth_100g_mac_fh1_hm_ref_clk */
	0x22,		/* ecpri_cc_eth_100g_mac_fh2_hm_ref_clk */
	0x34,		/* ecpri_cc_eth_dbg_nfapi_axi_clk */
	0x35,		/* ecpri_cc_eth_dbg_noc_axi_clk */
	0x95,		/* ecpri_cc_eth_phy_0_ock_sram_clk */
	0x96,		/* ecpri_cc_eth_phy_1_ock_sram_clk */
	0x97,		/* ecpri_cc_eth_phy_2_ock_sram_clk */
	0x98,		/* ecpri_cc_eth_phy_3_ock_sram_clk */
	0x99,		/* ecpri_cc_eth_phy_4_ock_sram_clk */
	0x41,		/* ecpri_cc_mss_emac_clk */
	0x40,		/* ecpri_cc_mss_oran_clk */
	0x45,		/* ecpri_cc_phy0_lane0_rx_clk */
	0x59,		/* ecpri_cc_phy0_lane0_tx_clk */
	0x46,		/* ecpri_cc_phy0_lane1_rx_clk */
	0x5A,		/* ecpri_cc_phy0_lane1_tx_clk */
	0x47,		/* ecpri_cc_phy0_lane2_rx_clk */
	0x5B,		/* ecpri_cc_phy0_lane2_tx_clk */
	0x48,		/* ecpri_cc_phy0_lane3_rx_clk */
	0x5C,		/* ecpri_cc_phy0_lane3_tx_clk */
	0x49,		/* ecpri_cc_phy1_lane0_rx_clk */
	0x5D,		/* ecpri_cc_phy1_lane0_tx_clk */
	0x4A,		/* ecpri_cc_phy1_lane1_rx_clk */
	0x5E,		/* ecpri_cc_phy1_lane1_tx_clk */
	0x4B,		/* ecpri_cc_phy1_lane2_rx_clk */
	0x5F,		/* ecpri_cc_phy1_lane2_tx_clk */
	0x4C,		/* ecpri_cc_phy1_lane3_rx_clk */
	0x60,		/* ecpri_cc_phy1_lane3_tx_clk */
	0x4D,		/* ecpri_cc_phy2_lane0_rx_clk */
	0x61,		/* ecpri_cc_phy2_lane0_tx_clk */
	0x4E,		/* ecpri_cc_phy2_lane1_rx_clk */
	0x62,		/* ecpri_cc_phy2_lane1_tx_clk */
	0x4F,		/* ecpri_cc_phy2_lane2_rx_clk */
	0x63,		/* ecpri_cc_phy2_lane2_tx_clk */
	0x50,		/* ecpri_cc_phy2_lane3_rx_clk */
	0x64,		/* ecpri_cc_phy2_lane3_tx_clk */
	0x51,		/* ecpri_cc_phy3_lane0_rx_clk */
	0x65,		/* ecpri_cc_phy3_lane0_tx_clk */
	0x52,		/* ecpri_cc_phy3_lane1_rx_clk */
	0x66,		/* ecpri_cc_phy3_lane1_tx_clk */
	0x53,		/* ecpri_cc_phy3_lane2_rx_clk */
	0x67,		/* ecpri_cc_phy3_lane2_tx_clk */
	0x54,		/* ecpri_cc_phy3_lane3_rx_clk */
	0x68,		/* ecpri_cc_phy3_lane3_tx_clk */
	0x55,		/* ecpri_cc_phy4_lane0_rx_clk */
	0x69,		/* ecpri_cc_phy4_lane0_tx_clk */
	0x56,		/* ecpri_cc_phy4_lane1_rx_clk */
	0x6A,		/* ecpri_cc_phy4_lane1_tx_clk */
	0x57,		/* ecpri_cc_phy4_lane2_rx_clk */
	0x6B,		/* ecpri_cc_phy4_lane2_tx_clk */
	0x58,		/* ecpri_cc_phy4_lane3_rx_clk */
	0x6C,		/* ecpri_cc_phy4_lane3_tx_clk */
};

static struct clk_debug_mux ecpri_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x1B108,
	.post_div_offset = 0xB000,
	.cbcr_offset = 0xB004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = ecpri_cc_debug_mux_sels,
	.hw.init = &(const struct clk_init_data){
		.name = "ecpri_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = ecpri_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(ecpri_cc_debug_mux_parent_names),
	},
};

static const char *const gcc_debug_mux_parent_names[] = {
	"apss_cc_debug_mux",
	"ecpri_cc_debug_mux",
	"gcc_aggre_noc_ecpri_dma_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_cfg_noc_ecpri_cc_ahb_clk",
	"gcc_cfg_noc_usb3_prim_axi_clk",
	"gcc_ddrss_ecpri_dma_clk",
	"gcc_aggre_noc_ecpri_gsi_clk",
	"gcc_ecpri_ahb_clk",
	"gcc_ecpri_cc_gpll0_clk_src",
	"gcc_ecpri_cc_gpll1_even_clk_src",
	"gcc_ecpri_cc_gpll2_even_clk_src",
	"gcc_ecpri_cc_gpll3_clk_src",
	"gcc_ecpri_cc_gpll4_clk_src",
	"gcc_ecpri_cc_gpll5_even_clk_src",
	"gcc_ecpri_xo_clk",
	"gcc_eth_dbg_snoc_axi_clk",
	"gcc_gemnoc_pcie_qx_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_pcie_0_aux_clk",
	"gcc_pcie_0_cfg_ahb_clk",
	"gcc_pcie_0_mstr_axi_clk",
	"gcc_pcie_0_phy_aux_clk",
	"gcc_pcie_0_phy_rchng_clk",
	"gcc_pcie_0_pipe_clk",
	"gcc_pcie_0_slv_axi_clk",
	"gcc_pcie_0_slv_q2a_axi_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_qmip_anoc_pcie_clk",
	"gcc_qmip_ecpri_dma0_clk",
	"gcc_qmip_ecpri_dma1_clk",
	"gcc_qmip_ecpri_gsi_clk",
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
	"gcc_qupv3_wrap_0_m_ahb_clk",
	"gcc_qupv3_wrap_0_s_ahb_clk",
	"gcc_qupv3_wrap_1_m_ahb_clk",
	"gcc_qupv3_wrap_1_s_ahb_clk",
	"gcc_sdcc5_ahb_clk",
	"gcc_sdcc5_apps_clk",
	"gcc_sdcc5_ice_core_clk",
	"gcc_sm_bus_ahb_clk",
	"gcc_sm_bus_xo_clk",
	"gcc_snoc_cnoc_gemnoc_pcie_qx_clk",
	"gcc_snoc_cnoc_gemnoc_pcie_south_qx_clk",
	"gcc_snoc_cnoc_pcie_qx_clk",
	"gcc_snoc_pcie_sf_center_qx_clk",
	"gcc_snoc_pcie_sf_south_qx_clk",
	"gcc_tsc_cfg_ahb_clk",
	"gcc_tsc_cntr_clk",
	"gcc_tsc_etu_clk",
	"gcc_usb30_prim_master_clk",
	"gcc_usb30_prim_mock_utmi_clk",
	"gcc_usb30_prim_sleep_clk",
	"gcc_usb3_prim_phy_aux_clk",
	"gcc_usb3_prim_phy_com_aux_clk",
	"gcc_usb3_prim_phy_pipe_clk",
	"mc_cc_debug_mux",
	"measure_only_pcie_0_phy_aux_clk",
	"measure_only_pcie_0_pipe_clk",
	"measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
	"gcc_eth_100g_c2c_hm_apb_clk",
	"gcc_eth_100g_fh_hm_apb_0_clk",
	"gcc_eth_100g_fh_hm_apb_1_clk",
	"gcc_eth_100g_fh_hm_apb_2_clk",
	"gcc_eth_dbg_c2c_hm_apb_clk",
};

static int gcc_debug_mux_sels[] = {
	0xBF,		/* apss_cc_debug_mux */
	0x115,		/* ecpri_cc_debug_mux */
	0x2C,		/* gcc_aggre_noc_ecpri_dma_clk */
	0x92,		/* gcc_boot_rom_ahb_clk */
	0x10E,		/* gcc_cfg_noc_ecpri_cc_ahb_clk */
	0x18,		/* gcc_cfg_noc_usb3_prim_axi_clk */
	0xB5,		/* gcc_ddrss_ecpri_dma_clk */
	0x31,		/* gcc_aggre_noc_ecpri_gsi_clk */
	0x105,		/* gcc_ecpri_ahb_clk */
	0x10F,		/* gcc_ecpri_cc_gpll0_clk_src */
	0x110,		/* gcc_ecpri_cc_gpll1_even_clk_src */
	0x111,		/* gcc_ecpri_cc_gpll2_even_clk_src */
	0x112,		/* gcc_ecpri_cc_gpll3_clk_src */
	0x113,		/* gcc_ecpri_cc_gpll4_clk_src */
	0x114,		/* gcc_ecpri_cc_gpll5_even_clk_src */
	0x104,		/* gcc_ecpri_xo_clk */
	0x102,		/* gcc_eth_dbg_snoc_axi_clk */
	0xB4,		/* gcc_gemnoc_pcie_qx_clk */
	0xC8,		/* gcc_gp1_clk */
	0xC9,		/* gcc_gp2_clk */
	0xCA,		/* gcc_gp3_clk */
	0xCF,		/* gcc_pcie_0_aux_clk */
	0xCE,		/* gcc_pcie_0_cfg_ahb_clk */
	0xCD,		/* gcc_pcie_0_mstr_axi_clk */
	0xD0,		/* gcc_pcie_0_phy_aux_clk */
	0xD3,		/* gcc_pcie_0_phy_rchng_clk */
	0xD1,		/* gcc_pcie_0_pipe_clk */
	0xCC,		/* gcc_pcie_0_slv_axi_clk */
	0xCB,		/* gcc_pcie_0_slv_q2a_axi_clk */
	0x84,		/* gcc_pdm2_clk */
	0x82,		/* gcc_pdm_ahb_clk */
	0x83,		/* gcc_pdm_xo4_clk */
	0x23,		/* gcc_qmip_anoc_pcie_clk */
	0x20,		/* gcc_qmip_ecpri_dma0_clk */
	0x21,		/* gcc_qmip_ecpri_dma1_clk */
	0x22,		/* gcc_qmip_ecpri_gsi_clk */
	0x6D,		/* gcc_qupv3_wrap0_core_2x_clk */
	0x6C,		/* gcc_qupv3_wrap0_core_clk */
	0x6E,		/* gcc_qupv3_wrap0_s0_clk */
	0x6F,		/* gcc_qupv3_wrap0_s1_clk */
	0x70,		/* gcc_qupv3_wrap0_s2_clk */
	0x71,		/* gcc_qupv3_wrap0_s3_clk */
	0x72,		/* gcc_qupv3_wrap0_s4_clk */
	0x73,		/* gcc_qupv3_wrap0_s5_clk */
	0x74,		/* gcc_qupv3_wrap0_s6_clk */
	0x75,		/* gcc_qupv3_wrap0_s7_clk */
	0x79,		/* gcc_qupv3_wrap1_core_2x_clk */
	0x78,		/* gcc_qupv3_wrap1_core_clk */
	0x7A,		/* gcc_qupv3_wrap1_s0_clk */
	0x7B,		/* gcc_qupv3_wrap1_s1_clk */
	0x7C,		/* gcc_qupv3_wrap1_s2_clk */
	0x7D,		/* gcc_qupv3_wrap1_s3_clk */
	0x7E,		/* gcc_qupv3_wrap1_s4_clk */
	0x7F,		/* gcc_qupv3_wrap1_s5_clk */
	0x80,		/* gcc_qupv3_wrap1_s6_clk */
	0x81,		/* gcc_qupv3_wrap1_s7_clk */
	0x6A,		/* gcc_qupv3_wrap_0_m_ahb_clk */
	0x6B,		/* gcc_qupv3_wrap_0_s_ahb_clk */
	0x76,		/* gcc_qupv3_wrap_1_m_ahb_clk */
	0x77,		/* gcc_qupv3_wrap_1_s_ahb_clk */
	0x108,		/* gcc_sdcc5_ahb_clk */
	0x107,		/* gcc_sdcc5_apps_clk */
	0x109,		/* gcc_sdcc5_ice_core_clk */
	0x11A,		/* gcc_sm_bus_ahb_clk */
	0x11B,		/* gcc_sm_bus_xo_clk */
	0x2D,		/* gcc_snoc_cnoc_gemnoc_pcie_qx_clk */
	0x2E,		/* gcc_snoc_cnoc_gemnoc_pcie_south_qx_clk */
	0x1D,		/* gcc_snoc_cnoc_pcie_qx_clk */
	0x2F,		/* gcc_snoc_pcie_sf_center_qx_clk */
	0x30,		/* gcc_snoc_pcie_sf_south_qx_clk */
	0x119,		/* gcc_tsc_cfg_ahb_clk */
	0x117,		/* gcc_tsc_cntr_clk */
	0x118,		/* gcc_tsc_etu_clk */
	0x5D,		/* gcc_usb30_prim_master_clk */
	0x5F,		/* gcc_usb30_prim_mock_utmi_clk */
	0x5E,		/* gcc_usb30_prim_sleep_clk */
	0x60,		/* gcc_usb3_prim_phy_aux_clk */
	0x61,		/* gcc_usb3_prim_phy_com_aux_clk */
	0x62,		/* gcc_usb3_prim_phy_pipe_clk */
	0xB8,		/* mc_cc_debug_mux */
	0xD5,		/* measure_only_pcie_0_phy_aux_clk */
	0xD4,		/* measure_only_pcie_0_pipe_clk */
	0x66,		/* measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk */
	0xFF,		/* gcc_eth_100g_c2c_hm_apb_clk */
	0xFC,		/* gcc_eth_100g_fh_hm_apb_0_clk */
	0xFD,		/* gcc_eth_100g_fh_hm_apb_1_clk */
	0xFE,		/* gcc_eth_100g_fh_hm_apb_2_clk */
	0x100,		/* gcc_eth_dbg_c2c_hm_apb_clk */
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
	.hw.init = &(const struct clk_init_data){
		.name = "gcc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gcc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gcc_debug_mux_parent_names),
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
	{ .mux = &ecpri_cc_debug_mux, .regmap_name = "qcom,ecpricc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
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

static struct clk_dummy measure_only_mccc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_mccc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcie_0_phy_aux_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie_0_phy_aux_clk",
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

static struct clk_dummy measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_cinder_hws[] = {
	&measure_only_apcs_l3_post_acd_clk.hw,
	&measure_only_apcs_l3_pre_acd_clk.hw,
	&measure_only_apcs_silver_post_acd_clk.hw,
	&measure_only_apcs_silver_pre_acd_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_pcie_0_phy_aux_clk.hw,
	&measure_only_pcie_0_pipe_clk.hw,
	&measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,cinder-debugcc" },
	{ }
};

static int clk_debug_cinder_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0, i;

	BUILD_BUG_ON(ARRAY_SIZE(apss_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(apss_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(ecpri_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(ecpri_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) != ARRAY_SIZE(gcc_debug_mux_sels));

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

	for (i = 0; i < ARRAY_SIZE(mux_list); i++) {
		ret = devm_clk_register_debug_mux(&pdev->dev, mux_list[i].mux);
		if (ret) {
			dev_err(&pdev->dev, "Unable to register mux clk %s, err:(%d)\n",
				qcom_clk_hw_get_name(&mux_list[i].mux->hw),
				ret);
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(debugcc_cinder_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_cinder_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_cinder_hws[i]),
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
	.probe = clk_debug_cinder_probe,
	.driver = {
		.name = "cinder-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_cinder_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_cinder_init);

MODULE_DESCRIPTION("QTI DEBUG CC CINDER Driver");
MODULE_LICENSE("GPL v2");

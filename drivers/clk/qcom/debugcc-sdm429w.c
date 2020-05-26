/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "clk-debug.h"

static struct measure_clk_data debug_mux_priv = {
	.ctl_reg = 0x74004,
	.status_reg = 0x74008,
	.xo_div4_cbcr = 0x30034,
};

static const char *const debug_mux_parent_names[] = {
	"snoc_clk",
	"sysmmnoc_clk",
	"bimc_clk",
	"pnoc_clk",
	"gcc_bimc_gfx_clk",
	"gcc_bimc_gpu_clk",
	"gcc_blsp1_ahb_clk",
	"gcc_blsp1_qup1_i2c_apps_clk",
	"gcc_blsp1_qup1_spi_apps_clk",
	"gcc_blsp1_qup2_i2c_apps_clk",
	"gcc_blsp1_qup2_spi_apps_clk",
	"gcc_blsp1_qup3_i2c_apps_clk",
	"gcc_blsp1_qup3_spi_apps_clk",
	"gcc_blsp1_qup4_i2c_apps_clk",
	"gcc_blsp1_qup4_spi_apps_clk",
	"gcc_blsp1_uart1_apps_clk",
	"gcc_blsp1_uart2_apps_clk",
	"gcc_blsp2_ahb_clk",
	"gcc_blsp2_qup1_i2c_apps_clk",
	"gcc_blsp2_qup1_spi_apps_clk",
	"gcc_blsp2_qup2_i2c_apps_clk",
	"gcc_blsp2_qup2_spi_apps_clk",
	"gcc_blsp2_qup3_i2c_apps_clk",
	"gcc_blsp2_qup3_spi_apps_clk",
	"gcc_blsp2_qup4_i2c_apps_clk",
	"gcc_blsp2_qup4_spi_apps_clk",
	"gcc_blsp2_sleep_clk",
	"gcc_blsp2_uart1_apps_clk",
	"gcc_blsp2_uart2_apps_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_camss_ahb_clk",
	"gcc_camss_cci_ahb_clk",
	"gcc_camss_cci_clk",
	"gcc_camss_cpp_ahb_clk",
	"gcc_camss_cpp_axi_clk",
	"gcc_camss_cpp_clk",
	"gcc_camss_csi0_ahb_clk",
	"gcc_camss_csi0_clk",
	"gcc_camss_csi0phy_clk",
	"gcc_camss_csi0phytimer_clk",
	"gcc_camss_csi0pix_clk",
	"gcc_camss_csi0rdi_clk",
	"gcc_camss_csi1_ahb_clk",
	"gcc_camss_csi1_clk",
	"gcc_camss_csi1phy_clk",
	"gcc_camss_csi1phytimer_clk",
	"gcc_camss_csi1pix_clk",
	"gcc_camss_csi1rdi_clk",
	"gcc_camss_csi2_ahb_clk",
	"gcc_camss_csi2_clk",
	"gcc_camss_csi2phy_clk",
	"gcc_camss_csi2pix_clk",
	"gcc_camss_csi2rdi_clk",
	"gcc_camss_csi_vfe0_clk",
	"gcc_camss_csi_vfe1_clk",
	"gcc_camss_gp0_clk",
	"gcc_camss_gp1_clk",
	"gcc_camss_ispif_ahb_clk",
	"gcc_camss_jpeg0_clk",
	"gcc_camss_jpeg_ahb_clk",
	"gcc_camss_jpeg_axi_clk",
	"gcc_camss_mclk0_clk",
	"gcc_camss_mclk1_clk",
	"gcc_camss_mclk2_clk",
	"gcc_camss_micro_ahb_clk",
	"gcc_camss_top_ahb_clk",
	"gcc_camss_vfe0_clk",
	"gcc_camss_vfe1_ahb_clk",
	"gcc_camss_vfe1_axi_clk",
	"gcc_camss_vfe1_clk",
	"gcc_camss_vfe_ahb_clk",
	"gcc_camss_vfe_axi_clk",
	"gcc_crypto_ahb_clk",
	"gcc_crypto_axi_clk",
	"gcc_crypto_clk",
	"gcc_dcc_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_mdss_ahb_clk",
	"gcc_mdss_axi_clk",
	"gcc_mdss_byte0_clk",
	"gcc_mdss_byte1_clk",
	"gcc_mdss_esc0_clk",
	"gcc_mdss_esc1_clk",
	"gcc_mdss_mdp_clk",
	"gcc_mdss_pclk0_clk",
	"gcc_mdss_pclk1_clk",
	"gcc_mdss_vsync_clk",
	"gcc_mss_cfg_ahb_clk",
	"gcc_mss_q6_bimc_axi_clk",
	"gcc_oxili_ahb_clk",
	"gcc_oxili_aon_clk",
	"gcc_oxili_gfx3d_clk",
	"gcc_oxili_timer_clk",
	"gcc_pcnoc_mpu_cfg_ahb_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_prng_ahb_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_usb2a_phy_sleep_clk",
	"gcc_usb_hs_ahb_clk",
	"gcc_usb_hs_phy_cfg_ahb_clk",
	"gcc_usb_hs_system_clk",
	"gcc_venus0_ahb_clk",
	"gcc_venus0_axi_clk",
	"gcc_venus0_core0_vcodec0_clk",
	"gcc_venus0_vcodec0_clk",
	"gcc_apss_tcu_clk",
	"gcc_cpp_tbu_clk",
	"gcc_jpeg_tbu_clk",
	"gcc_mdp_tbu_clk",
	"gcc_smmu_cfg_clk",
	"gcc_venus_tbu_clk",
	"gcc_vfe_tbu_clk",
	"gcc_vfe1_tbu_clk",
	"gcc_qdss_dap_clk",
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x74000,
	.post_div_offset = 0x74000,
	.cbcr_offset = 0x74000,
	.src_sel_mask = 0x1FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 12,
	.en_mask = BIT(16),
	MUX_SRC_LIST(
		{ "snoc_clk", 0x000, 1, GCC, 0x000, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "sysmmnoc_clk", 0x001, 1, GCC, 0x001, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "bimc_clk", 0x15A, 4, GCC, 0x15A, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "pnoc_clk", 0x008, 1, GCC, 0x008, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_bimc_gfx_clk", 0x2D, 1, GCC, 0x2D, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_bimc_gpu_clk", 0x157, 1, GCC, 0x157, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_ahb_clk", 0x88, 1, GCC, 0x88, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup1_i2c_apps_clk", 0x8B, 1, GCC, 0x8B, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup1_spi_apps_clk", 0x8A, 1, GCC, 0x8A, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup2_i2c_apps_clk", 0x90, 1, GCC, 0x90, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup2_spi_apps_clk", 0x8E, 1, GCC, 0x8E, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup3_i2c_apps_clk", 0x94, 1, GCC, 0x94, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup3_spi_apps_clk", 0x93, 1, GCC, 0x93, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup4_i2c_apps_clk", 0x96, 1, GCC, 0x96, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup4_spi_apps_clk", 0x95, 1, GCC, 0x95, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_uart1_apps_clk", 0x8C, 1, GCC, 0x8C, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_uart2_apps_clk", 0x91, 1, GCC, 0x91, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_ahb_clk", 0x98, 1, GCC, 0x98, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup1_i2c_apps_clk", 0x9B, 1, GCC, 0x9B, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup1_spi_apps_clk", 0x9A, 1, GCC, 0x9A, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup2_i2c_apps_clk", 0xA0, 1, GCC, 0xA0, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup2_spi_apps_clk", 0x9E, 1, GCC, 0x9E, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup3_i2c_apps_clk", 0xA4, 1, GCC, 0xA4, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup3_spi_apps_clk", 0xA3, 1, GCC, 0xA3, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup4_i2c_apps_clk", 0xA6, 1, GCC, 0xA6, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup4_spi_apps_clk", 0xA5, 1, GCC, 0xA5, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_sleep_clk", 0x99, 1, GCC, 0x99, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_uart1_apps_clk", 0x9C, 1, GCC, 0x9C, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_uart2_apps_clk", 0xA1, 1, GCC, 0xA1, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_boot_rom_ahb_clk", 0xF8, 1, GCC, 0xF8, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_ahb_clk", 0xA8, 1, GCC, 0xA8, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_cci_ahb_clk", 0xB0, 1, GCC, 0xB0, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_cci_clk", 0xAF, 1, GCC, 0xAF, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_cpp_ahb_clk", 0xBA, 1, GCC, 0xBA, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_cpp_axi_clk", 0x1A3, 1, GCC, 0x1A3, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_cpp_clk", 0xB9, 1, GCC, 0xB9, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi0_ahb_clk", 0xC1, 1, GCC, 0xC1, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi0_clk", 0xC0, 1, GCC, 0xC0, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi0phy_clk", 0xC2, 1, GCC, 0xC2, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi0phytimer_clk", 0xB1, 1, GCC, 0xB1, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi0pix_clk", 0xC4, 1, GCC, 0xC4, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi0rdi_clk", 0xC3, 1, GCC, 0xC3, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi1_ahb_clk", 0xC6, 1, GCC, 0xC6, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi1_clk", 0xC5, 1, GCC, 0xC5, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi1phy_clk", 0xC7, 1, GCC, 0xC7, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi1phytimer_clk", 0xB2, 1, GCC, 0xB2, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi1pix_clk", 0xE1, 1, GCC, 0xE1, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi1rdi_clk", 0xE0, 1, GCC, 0xE0, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi2_ahb_clk", 0xE4, 1, GCC, 0xE4, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi2_clk", 0xE3, 1, GCC, 0xE3, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi2phy_clk", 0xE5, 1, GCC, 0xE5, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi2pix_clk", 0xE7, 1, GCC, 0xE7, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi2rdi_clk", 0xE6, 1, GCC, 0xE6, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi_vfe0_clk", 0xBF, 1, GCC, 0xBF, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_csi_vfe1_clk", 0x1A0, 1, GCC, 0x1A0, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_gp0_clk", 0xAB, 1, GCC, 0xAB, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_gp1_clk", 0xAC, 1, GCC, 0xAC, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_ispif_ahb_clk", 0xE2, 1, GCC, 0xE2, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_jpeg0_clk", 0xB3, 1, GCC, 0xB3, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_jpeg_ahb_clk", 0xB4, 1, GCC, 0xB4, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_jpeg_axi_clk", 0xB5, 1, GCC, 0xB5, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_mclk0_clk", 0xAD, 1, GCC, 0xAD, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_mclk1_clk", 0xAE, 1, GCC, 0xAE, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_mclk2_clk", 0x1BD, 1, GCC, 0x1BD, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_micro_ahb_clk", 0xAA, 1, GCC, 0xAA, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_top_ahb_clk", 0xA9, 1, GCC, 0xA9, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_vfe0_clk", 0xB8, 1, GCC, 0xB8, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_vfe1_ahb_clk", 0x1A2, 1, GCC, 0x1A2, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_vfe1_axi_clk", 0x1A4, 1, GCC, 0x1A4, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_vfe1_clk", 0x1A1, 1, GCC, 0x1A1, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_vfe_ahb_clk", 0xBB, 1, GCC, 0xBB, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_camss_vfe_axi_clk", 0xBC, 1, GCC, 0xBC, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_crypto_ahb_clk", 0x13A, 1, GCC, 0x13A, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_crypto_axi_clk", 0x139, 1, GCC, 0x139, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_crypto_clk", 0x138, 1, GCC, 0x138, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_dcc_clk", 0x1B9, 1, GCC, 0x1B9, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_gp1_clk", 0x10, 1, GCC, 0x10, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_gp2_clk", 0x11, 1, GCC, 0x11, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_gp3_clk", 0x12, 1, GCC, 0x12, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_ahb_clk", 0x1F6, 1, GCC, 0x1F6, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_axi_clk", 0x1F7, 1, GCC, 0x1F7, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_byte0_clk", 0x1E4, 1, GCC, 0x1E4, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_byte1_clk", 0x1FC, 1, GCC, 0x1FC, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_esc0_clk", 0x1FD, 1, GCC, 0x1FD, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_esc1_clk", 0x1E5, 1, GCC, 0x1E5, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_mdp_clk", 0x1F9, 1, GCC, 0x1F9, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_pclk0_clk", 0x1E3, 1, GCC, 0x1E3, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_pclk1_clk", 0x1F8, 1, GCC, 0x1F8, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_vsync_clk", 0x1FB, 1, GCC, 0x1FB, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mss_cfg_ahb_clk", 0x030, 1, GCC, 0x030, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mss_q6_bimc_axi_clk", 0x031, 1, GCC, 0x031, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_oxili_ahb_clk", 0x1EB, 1, GCC, 0x1EB, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_oxili_aon_clk", 0xEE, 1, GCC, 0xEE, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_oxili_gfx3d_clk", 0x1EA, 1, GCC, 0x1EA, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_oxili_timer_clk", 0x1E9, 1, GCC, 0x1E9, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pcnoc_mpu_cfg_ahb_clk", 0xC9, 1, GCC, 0xC9, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pdm2_clk", 0xD2, 1, GCC, 0xD2, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_pdm_ahb_clk", 0xD0, 1, GCC, 0xD0, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_prng_ahb_clk", 0xD8, 1, GCC, 0xD8, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc1_ahb_clk", 0x69, 1, GCC, 0x69, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc1_apps_clk", 0x68, 1, GCC, 0x68, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc1_ice_core_clk", 0x6A, 1, GCC, 0x6A, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc2_ahb_clk", 0x71, 1, GCC, 0x71, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc2_apps_clk", 0x70, 1, GCC, 0x70, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb2a_phy_sleep_clk", 0x63, 1, GCC, 0x63, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb_hs_ahb_clk", 0x61, 1, GCC, 0x61, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb_hs_phy_cfg_ahb_clk", 0x64, 1, GCC, 0x64, 0x1FF, 0,
			 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb_hs_system_clk", 0x60, 1, GCC, 0x60, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_venus0_ahb_clk", 0x1F3, 1, GCC, 0x1F3, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_venus0_axi_clk", 0x1F2, 1, GCC, 0x1F2, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_venus0_core0_vcodec0_clk", 0x1B8, 1, GCC, 0x1B8, 0x1FF,
			 0, 0xF, 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_venus0_vcodec0_clk", 0x1F1, 1, GCC, 0x1F1, 0x1FF, 0, 0xF,
			 12, 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_apss_tcu_clk", 0x159, 1, GCC, 0x159, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_cpp_tbu_clk", 0xE9, 1, GCC, 0xE9, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_jpeg_tbu_clk", 0x5C, 1, GCC, 0x5C, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdp_tbu_clk", 0x51, 1, GCC, 0x51, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_smmu_cfg_clk", 0x5B, 1, GCC, 0x5B, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_venus_tbu_clk", 0x54, 1, GCC, 0x54, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_vfe_tbu_clk", 0x5A, 1, GCC, 0x5A, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
		{ "gcc_vfe1_tbu_clk", 0x199, 1, GCC, 0x199, 0x1FF, 0, 0xF, 12,
			 1, 0x74000, 0x74000, 0x74000 },
		{ "gcc_qdss_dap_clk", 0x49, 1, GCC, 0x49, 0x1FF, 0, 0xF, 12, 1,
			 0x74000, 0x74000, 0x74000 },
	),
	.hw.init = &(struct clk_init_data){
		.name = "gcc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,debugcc-sdm429w" },
	{ }
};

static int map_debug_bases(struct platform_device *pdev, char *base, int cc)
{
	if (!of_get_property(pdev->dev.of_node, base, NULL))
		return -ENODEV;

	gcc_debug_mux.regmap[cc] =
			syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
					base);
	if (IS_ERR(gcc_debug_mux.regmap[cc])) {
		pr_err("Failed to map %s (ret=%ld)\n", base,
				PTR_ERR(gcc_debug_mux.regmap[cc]));
		return PTR_ERR(gcc_debug_mux.regmap[cc]);
	}
	return 0;
}

static int clk_debug_sdm429w_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret;

	clk = devm_clk_get(&pdev->dev, "xo_clk_src");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo clock\n");
		return PTR_ERR(clk);
	}

	debug_mux_priv.cxo = clk;

	gcc_debug_mux.regmap = devm_kcalloc(&pdev->dev, MAX_NUM_CC,
				sizeof(*gcc_debug_mux.regmap), GFP_KERNEL);
	if (!gcc_debug_mux.regmap)
		return -ENOMEM;

	ret = map_debug_bases(pdev, "qcom,gcc", GCC);
	if (ret)
		return ret;

	clk = devm_clk_register(&pdev->dev, &gcc_debug_mux.hw);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Unable to register GCC debug mux\n");
		return PTR_ERR(clk);
	}

	ret = clk_debug_measure_register(&gcc_debug_mux.hw);
	if (ret)
		dev_err(&pdev->dev, "Could not register Measure clock\n");

	return ret;
}

static struct platform_driver clk_debug_driver = {
	.probe = clk_debug_sdm429w_probe,
	.driver = {
		.name = "debugcc-sdm429w",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_sdm429w_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_sdm429w_init);

MODULE_DESCRIPTION("QTI DEBUG CC SDM429W Driver");
MODULE_LICENSE("GPL v2");

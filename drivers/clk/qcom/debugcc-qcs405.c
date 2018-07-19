/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
	"pnoc_clk",
	"bimc_clk",
	"qpic_clk",
	"ce1_clk",
	"wcnss_m_clk",
	"gcc_apss_ahb_clk",
	"gcc_bimc_gfx_clk",
	"gcc_blsp1_ahb_clk",
	"gcc_blsp1_qup0_i2c_apps_clk",
	"gcc_blsp1_qup0_spi_apps_clk",
	"gcc_blsp1_qup1_i2c_apps_clk",
	"gcc_blsp1_qup1_spi_apps_clk",
	"gcc_blsp1_qup2_i2c_apps_clk",
	"gcc_blsp1_qup2_spi_apps_clk",
	"gcc_blsp1_qup3_i2c_apps_clk",
	"gcc_blsp1_qup3_spi_apps_clk",
	"gcc_blsp1_qup4_i2c_apps_clk",
	"gcc_blsp1_qup4_spi_apps_clk",
	"gcc_blsp1_uart0_apps_clk",
	"gcc_blsp1_uart1_apps_clk",
	"gcc_blsp1_uart2_apps_clk",
	"gcc_blsp1_uart3_apps_clk",
	"gcc_blsp2_ahb_clk",
	"gcc_blsp2_qup0_i2c_apps_clk",
	"gcc_blsp2_qup0_spi_apps_clk",
	"gcc_blsp2_uart0_apps_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_dcc_clk",
	"gcc_eth_axi_clk",
	"gcc_eth_ptp_clk",
	"gcc_eth_rgmii_clk",
	"gcc_eth_slave_ahb_clk",
	"gcc_geni_ir_s_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_mdss_ahb_clk",
	"gcc_mdss_axi_clk",
	"gcc_mdss_byte0_clk",
	"gcc_mdss_esc0_clk",
	"gcc_mdss_hdmi_app_clk",
	"gcc_mdss_hdmi_pclk_clk",
	"gcc_mdss_mdp_clk",
	"gcc_mdss_pclk0_clk",
	"gcc_mdss_vsync_clk",
	"gcc_oxili_ahb_clk",
	"gcc_oxili_gfx3d_clk",
	"gcc_pcie_0_aux_clk",
	"gcc_pcie_0_cfg_ahb_clk",
	"gcc_pcie_0_mstr_axi_clk",
	"gcc_pcie_0_pipe_clk",
	"gcc_pcie_0_slv_axi_clk",
	"gcc_pcnoc_usb2_clk",
	"gcc_pcnoc_usb3_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_prng_ahb_clk",
	"gcc_pwm0_xo512_clk",
	"gcc_pwm1_xo512_clk",
	"gcc_pwm2_xo512_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_sys_noc_usb3_clk",
	"gcc_usb20_mock_utmi_clk",
	"gcc_usb2a_phy_sleep_clk",
	"gcc_usb30_master_clk",
	"gcc_usb30_mock_utmi_clk",
	"gcc_usb30_sleep_clk",
	"gcc_usb3_phy_aux_clk",
	"gcc_usb3_phy_pipe_clk",
	"gcc_usb_hs_inactivity_timers_clk",
	"gcc_usb_hs_phy_cfg_ahb_clk",
	"gcc_usb_hs_system_clk",
	"gcc_dcc_clk",
	"apcs_mux_clk",
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x74000,
	.post_div_offset = 0x74000,
	.cbcr_offset = 0x74000,
	.src_sel_mask = 0x1FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF000,
	.post_div_shift = 12,
	.en_mask = BIT(16),
	MUX_SRC_LIST(
		{ "snoc_clk", 0x000, 4, GCC,
		0x000, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "pnoc_clk", 0x008, 4, GCC,
		0x008, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "bimc_clk", 0x15A, 4, GCC,
		0x15A, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "qpic_clk", 0x0C0, 4, GCC,
		0x0C0, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "ce1_clk", 0x138, 4, GCC,
		0x138, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "wcnss_m_clk", 0x08A, 4, GCC,
		0x08A, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_apss_ahb_clk", 0x168, 4, GCC,
		0x168, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_bimc_gfx_clk", 0x2D, 4, GCC,
		0x2D, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_ahb_clk", 0x88, 4, GCC,
		0x88, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup0_i2c_apps_clk", 0x99, 4, GCC,
		0x99, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup0_spi_apps_clk", 0x98, 4, GCC,
		0x98, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup1_i2c_apps_clk", 0x8B, 4, GCC,
		0x8B, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup1_spi_apps_clk", 0x8A, 4, GCC,
		0x8A, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup2_i2c_apps_clk", 0x8F, 4, GCC,
		0x8F, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup2_spi_apps_clk", 0x8E, 4, GCC,
		0x8E, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup3_i2c_apps_clk", 0x93, 4, GCC,
		0x93, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup3_spi_apps_clk", 0x92, 4, GCC,
		0x92, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup4_i2c_apps_clk", 0x95, 4, GCC,
		0x95, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_qup4_spi_apps_clk", 0x94, 4, GCC,
		0x94, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_uart0_apps_clk", 0x9A, 4, GCC,
		0x9A, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_uart1_apps_clk", 0x8C, 4, GCC,
		0x8C, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_uart2_apps_clk", 0x90, 4, GCC,
		0x90, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp1_uart3_apps_clk", 0x96, 4, GCC,
		0x96, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_ahb_clk", 0xA0, 4, GCC,
		0xA0, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup0_i2c_apps_clk", 0xA2, 4, GCC,
		0xA2, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_qup0_spi_apps_clk", 0xA3, 4, GCC,
		0xA3, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_blsp2_uart0_apps_clk", 0xA4, 4, GCC,
		0xA4, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_boot_rom_ahb_clk", 0xF8, 4, GCC,
		0xF8, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_dcc_clk", 0x1B9, 4, GCC,
		0x1B9, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_eth_axi_clk", 0x80, 4, GCC,
		0x80, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_eth_ptp_clk", 0x83, 4, GCC,
		0x83, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_eth_rgmii_clk", 0x81, 4, GCC,
		0x81, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_eth_slave_ahb_clk", 0x82, 4, GCC,
		0x82, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_geni_ir_s_clk", 0xEE, 4, GCC,
		0xEE, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_gp1_clk", 0x10, 4, GCC,
		0x10, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_gp2_clk", 0x11, 4, GCC,
		0x11, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_gp3_clk", 0x12, 4, GCC,
		0x12, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_ahb_clk", 0x1F6, 4, GCC,
		0x1F6, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_axi_clk", 0x1F7, 4, GCC,
		0x1F7, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_byte0_clk", 0x1FC, 4, GCC,
		0x1FC, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_esc0_clk", 0x1FD, 4, GCC,
		0x1FD, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_hdmi_app_clk", 0x1F2, 4, GCC,
		0x1F2, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_hdmi_pclk_clk", 0x1F1, 4, GCC,
		0x1F1, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_mdp_clk", 0x1F9, 4, GCC,
		0x1F9, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_pclk0_clk", 0x1F8, 4, GCC,
		0x1F8, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_mdss_vsync_clk", 0x1FB, 4, GCC,
		0x1FB, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_oxili_ahb_clk", 0x1EB, 4, GCC,
		0x1EB, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_oxili_gfx3d_clk", 0x1EA, 4, GCC,
		0x1EA, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pcie_0_aux_clk", 0xAB, 4, GCC,
		0xAB, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pcie_0_cfg_ahb_clk", 0xAA, 4, GCC,
		0xAA, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pcie_0_mstr_axi_clk", 0xA9, 4, GCC,
		0xA9, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pcie_0_pipe_clk", 0xAC, 4, GCC,
		0xAC, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pcie_0_slv_axi_clk", 0xA8, 4, GCC,
		0xA8, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pcnoc_usb2_clk", 0x9, 4, GCC,
		0x9, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pcnoc_usb3_clk", 0xA, 4, GCC,
		0xA, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pdm2_clk", 0xD2, 4, GCC,
		0xD2, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pdm_ahb_clk", 0xD0, 4, GCC,
		0xD0, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_prng_ahb_clk", 0xD8, 4, GCC,
		0xD8, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pwm0_xo512_clk", 0xD3, 4, GCC,
		0xD3, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pwm1_xo512_clk", 0xD4, 4, GCC,
		0xD4, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_pwm2_xo512_clk", 0xD5, 4, GCC,
		0xD5, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc1_ahb_clk", 0x69, 4, GCC,
		0x69, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc1_apps_clk", 0x68, 4, GCC,
		0x68, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc1_ice_core_clk", 0x6A, 4, GCC,
		0x6A, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc2_ahb_clk", 0x71, 4, GCC,
		0x71, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_sdcc2_apps_clk", 0x70, 4, GCC,
		0x70, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_sys_noc_usb3_clk", 0x1, 4, GCC,
		0x1, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb20_mock_utmi_clk", 0x65, 4, GCC,
		0x65, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb2a_phy_sleep_clk", 0x63, 4, GCC,
		0x63, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb30_master_clk", 0x78, 4, GCC,
		0x78, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb30_mock_utmi_clk", 0x7A, 4, GCC,
		0x7A, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb30_sleep_clk", 0x79, 4, GCC,
		0x79, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb3_phy_aux_clk", 0x7C, 4, GCC,
		0x7C, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb3_phy_pipe_clk", 0x7B, 4, GCC,
		0x7B, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb_hs_inactivity_timers_clk", 0x62, 4, GCC,
		0x62, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb_hs_phy_cfg_ahb_clk", 0x64, 4, GCC,
		0x64, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_usb_hs_system_clk", 0x60, 4, GCC,
		0x60, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "gcc_dcc_clk", 0x1B9, 4, GCC,
		0x1B9, 0x1FF, 0, 0xF000, 12, 4, 0x74000, 0x74000, 0x74000 },
		{ "apcs_mux_clk", 0x16A, CPU_CC, 0x000, 0x3, 8, 0x0FF },
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
	{ .compatible = "qcom,debugcc-qcs405" },
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

static int clk_debug_qcs405_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0;

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

	ret = map_debug_bases(pdev, "qcom,cpucc", CPU_CC);
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
	.probe = clk_debug_qcs405_probe,
	.driver = {
		.name = "debugcc-qcs405",
		.of_match_table = clk_debug_match_table,
		.owner = THIS_MODULE,
	},
};

int __init clk_debug_qcs405_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_qcs405_init);

MODULE_DESCRIPTION("QTI DEBUG CC QCS405 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:debugcc-qcs405");

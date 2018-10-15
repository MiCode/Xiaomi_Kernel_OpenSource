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
	.ctl_reg = 0x79004,
	.status_reg = 0x79008,
	.xo_div4_cbcr = 0x22010,
};

static const char *const debug_mux_parent_names[] = {
	"gcc_ahb_pcie_link_clk",
	"gcc_blsp1_ahb_clk",
	"gcc_blsp1_qup1_i2c_apps_clk",
	"gcc_blsp1_qup1_spi_apps_clk",
	"gcc_blsp1_qup2_i2c_apps_clk",
	"gcc_blsp1_qup2_spi_apps_clk",
	"gcc_blsp1_qup3_i2c_apps_clk",
	"gcc_blsp1_qup3_spi_apps_clk",
	"gcc_blsp1_qup4_i2c_apps_clk",
	"gcc_blsp1_qup4_spi_apps_clk",
	"gcc_blsp1_sleep_clk",
	"gcc_blsp1_uart1_apps_clk",
	"gcc_blsp1_uart2_apps_clk",
	"gcc_blsp1_uart3_apps_clk",
	"gcc_blsp1_uart4_apps_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_ce1_ahb_clk",
	"gcc_ce1_axi_clk",
	"gcc_ce1_clk",
	"gcc_cpuss_ahb_clk",
	"gcc_cpuss_gnoc_clk",
	"gcc_cpuss_rbcpr_clk",
	"gcc_eth_axi_clk",
	"gcc_eth_ptp_clk",
	"gcc_eth_rgmii_clk",
	"gcc_eth_slave_ahb_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_pcie_aux_clk",
	"gcc_pcie_cfg_ahb_clk",
	"gcc_pcie_mstr_axi_clk",
	"gcc_pcie_pipe_clk",
	"gcc_pcie_rchng_phy_clk",
	"gcc_pcie_sleep_clk",
	"gcc_pcie_slv_axi_clk",
	"gcc_pcie_slv_q2a_axi_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_spmi_fetcher_ahb_clk",
	"gcc_spmi_fetcher_clk",
	"gcc_sys_noc_cpuss_ahb_clk",
	"gcc_usb30_master_clk",
	"gcc_usb30_mock_utmi_clk",
	"gcc_usb30_mstr_axi_clk",
	"gcc_usb30_sleep_clk",
	"gcc_usb30_slv_ahb_clk",
	"gcc_usb3_phy_aux_clk",
	"gcc_usb3_phy_pipe_clk",
	"gcc_usb_phy_cfg_ahb2phy_clk",
	"gcc_xo_div4_clk",
	"gcc_xo_pcie_link_clk",
	"measure_only_bimc_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_snoc_clk",
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x79000,
	.post_div_offset = 0x29000,
	.cbcr_offset = 0x29004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	MUX_SRC_LIST(
		{ "gcc_ahb_pcie_link_clk", 0xCF, 4, GCC,
			0xCF, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_ahb_clk", 0x34, 4, GCC,
			0x34, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_qup1_i2c_apps_clk", 0x37, 4, GCC,
			0x37, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_qup1_spi_apps_clk", 0x36, 4, GCC,
			0x36, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_qup2_i2c_apps_clk", 0x3B, 4, GCC,
			0x3B, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_qup2_spi_apps_clk", 0x3A, 4, GCC,
			0x3A, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_qup3_i2c_apps_clk", 0x3F, 4, GCC,
			0x3F, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_qup3_spi_apps_clk", 0x3E, 4, GCC,
			0x3E, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_qup4_i2c_apps_clk", 0x43, 4, GCC,
			0x43, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_qup4_spi_apps_clk", 0x42, 4, GCC,
			0x42, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_sleep_clk", 0x35, 4, GCC,
			0x35, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_uart1_apps_clk", 0x38, 4, GCC,
			0x38, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_uart2_apps_clk", 0x3C, 4, GCC,
			0x3C, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_uart3_apps_clk", 0x40, 4, GCC,
			0x40, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_blsp1_uart4_apps_clk", 0x44, 4, GCC,
			0x44, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_boot_rom_ahb_clk", 0x4B, 4, GCC,
			0x4B, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_ce1_ahb_clk", 0x60, 4, GCC,
			0x60, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_ce1_axi_clk", 0x5F, 4, GCC,
			0x5F, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_ce1_clk", 0x5E, 4, GCC,
			0x5E, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_cpuss_ahb_clk", 0x74, 4, GCC,
			0x74, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_cpuss_gnoc_clk", 0x75, 4, GCC,
			0x75, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_cpuss_rbcpr_clk", 0x76, 4, GCC,
			0x76, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_eth_axi_clk", 0xCB, 4, GCC,
			0xCB, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_eth_ptp_clk", 0xFD, 4, GCC,
			0xFD, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_eth_rgmii_clk", 0xC9, 4, GCC,
			0xC9, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_eth_slave_ahb_clk", 0xCA, 4, GCC,
			0xCA, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_gp1_clk", 0x85, 4, GCC,
			0x85, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_gp2_clk", 0x86, 4, GCC,
			0x86, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_gp3_clk", 0x87, 4, GCC,
			0x87, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pcie_aux_clk", 0x99, 4, GCC,
			0x99, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pcie_cfg_ahb_clk", 0x98, 4, GCC,
			0x98, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pcie_mstr_axi_clk", 0x97, 4, GCC,
			0x97, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pcie_pipe_clk", 0x9A, 4, GCC,
			0x9A, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pcie_rchng_phy_clk", 0xB9, 4, GCC,
			0xB9, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pcie_sleep_clk", 0x9C, 4, GCC,
			0x9C, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pcie_slv_axi_clk", 0x96, 4, GCC,
			0x96, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pcie_slv_q2a_axi_clk", 0x95, 4, GCC,
			0x95, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pdm2_clk", 0x48, 4, GCC,
			0x48, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pdm_ahb_clk", 0x46, 4, GCC,
			0x46, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_pdm_xo4_clk", 0x47, 4, GCC,
			0x47, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_sdcc1_ahb_clk", 0x33, 4, GCC,
			0x33, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_sdcc1_apps_clk", 0x32, 4, GCC,
			0x32, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_spmi_fetcher_ahb_clk", 0xB5, 4, GCC,
			0xB5, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_spmi_fetcher_clk", 0xB4, 4, GCC,
			0xB4, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_sys_noc_cpuss_ahb_clk", 0x10B, 4, GCC,
			0x10B, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_usb30_master_clk", 0x28, 4, GCC,
			0x28, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_usb30_mock_utmi_clk", 0x2A, 4, GCC,
			0x2A, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_usb30_mstr_axi_clk", 0x4F, 4, GCC,
			0x4F, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_usb30_sleep_clk", 0x29, 4, GCC,
			0x29, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_usb30_slv_ahb_clk", 0x6B, 4, GCC,
			0x6B, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_usb3_phy_aux_clk", 0x2B, 4, GCC,
			0x2B, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_usb3_phy_pipe_clk", 0x2D, 4, GCC,
			0x2D, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_usb_phy_cfg_ahb2phy_clk", 0x31, 4, GCC,
			0x31, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_xo_div4_clk", 0x63, 4, GCC,
			0x63, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "gcc_xo_pcie_link_clk", 0x77, 4, GCC,
			0x77, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "measure_only_bimc_clk", 0x73, 4, GCC,
			0x73, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "measure_only_ipa_2x_clk", 0xAC, 4, GCC,
			0xAC, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
		{ "measure_only_snoc_clk", 0x109, 4, GCC,
			0x109, 0x3FF, 0, 0xF, 0, 4, 0x79000, 0x29000, 0x29004 },
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
	{ .compatible = "qcom,debugcc-sdxprairie" },
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

static int clk_debug_sdxprairie_probe(struct platform_device *pdev)
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
	.probe = clk_debug_sdxprairie_probe,
	.driver = {
		.name = "debugcc-sdxprairie",
		.of_match_table = clk_debug_match_table,
		.owner = THIS_MODULE,
	},
};

int __init clk_debug_sdxprairie_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_sdxprairie_init);

MODULE_DESCRIPTION("QTI DEBUG CC SDXPRAIRIE Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:debugcc-sdxprairie");

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
	.ctl_reg = 0x79004,
	.status_reg = 0x79008,
	.xo_div4_cbcr = 0x2E010,
};

static const char *const apss_cc_debug_mux_parent_names[] = {
	"measure_only_apcs_clk",
};

static int apss_cc_debug_mux_sels[] = {
	0x3,		/* measure_only_apcs_clk */
};

static int apss_cc_debug_mux_pre_divs[] = {
	0x1,		/* measure_only_apcs_clk */
};

static struct clk_debug_mux apss_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0xc,
	.post_div_offset = 0x0,
	.cbcr_offset = 0x0,
	.src_sel_mask = 0x38,
	.src_sel_shift = 3,
	.post_div_mask = 0x0,
	.post_div_shift = 0x0,
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

static const char *const gcc_debug_mux_parent_names[] = {
	"apss_cc_debug_mux",
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
	"gcc_cpuss_ahb_clk",
	"gcc_cpuss_gnoc_clk",
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
	"mc_cc_debug_mux",
	"measure_only_ipa_2x_clk",
	"measure_only_snoc_clk",
};

static int gcc_debug_mux_sels[] = {
	0x7C,		/* apss_cc_debug_mux */
	0xCF,		/* gcc_ahb_pcie_link_clk */
	0x34,		/* gcc_blsp1_ahb_clk */
	0x37,		/* gcc_blsp1_qup1_i2c_apps_clk */
	0x36,		/* gcc_blsp1_qup1_spi_apps_clk */
	0x3B,		/* gcc_blsp1_qup2_i2c_apps_clk */
	0x3A,		/* gcc_blsp1_qup2_spi_apps_clk */
	0x3F,		/* gcc_blsp1_qup3_i2c_apps_clk */
	0x3E,		/* gcc_blsp1_qup3_spi_apps_clk */
	0x43,		/* gcc_blsp1_qup4_i2c_apps_clk */
	0x42,		/* gcc_blsp1_qup4_spi_apps_clk */
	0x35,		/* gcc_blsp1_sleep_clk */
	0x38,		/* gcc_blsp1_uart1_apps_clk */
	0x3C,		/* gcc_blsp1_uart2_apps_clk */
	0x40,		/* gcc_blsp1_uart3_apps_clk */
	0x44,		/* gcc_blsp1_uart4_apps_clk */
	0x4B,		/* gcc_boot_rom_ahb_clk */
	0x74,		/* gcc_cpuss_ahb_clk */
	0x75,		/* gcc_cpuss_gnoc_clk */
	0x85,		/* gcc_gp1_clk */
	0x86,		/* gcc_gp2_clk */
	0x87,		/* gcc_gp3_clk */
	0x99,		/* gcc_pcie_aux_clk */
	0x98,		/* gcc_pcie_cfg_ahb_clk */
	0x97,		/* gcc_pcie_mstr_axi_clk */
	0x9A,		/* gcc_pcie_pipe_clk */
	0xB9,		/* gcc_pcie_rchng_phy_clk */
	0x9C,		/* gcc_pcie_sleep_clk */
	0x96,		/* gcc_pcie_slv_axi_clk */
	0x95,		/* gcc_pcie_slv_q2a_axi_clk */
	0x48,		/* gcc_pdm2_clk */
	0x46,		/* gcc_pdm_ahb_clk */
	0x47,		/* gcc_pdm_xo4_clk */
	0x33,		/* gcc_sdcc1_ahb_clk */
	0x32,		/* gcc_sdcc1_apps_clk */
	0x10B,		/* gcc_sys_noc_cpuss_ahb_clk */
	0x28,		/* gcc_usb30_master_clk */
	0x2A,		/* gcc_usb30_mock_utmi_clk */
	0x4F,		/* gcc_usb30_mstr_axi_clk */
	0x29,		/* gcc_usb30_sleep_clk */
	0x6B,		/* gcc_usb30_slv_ahb_clk */
	0x2B,		/* gcc_usb3_phy_aux_clk */
	0x2D,		/* gcc_usb3_phy_pipe_clk */
	0x31,		/* gcc_usb_phy_cfg_ahb2phy_clk */
	0x63,		/* gcc_xo_div4_clk */
	0x77,		/* gcc_xo_pcie_link_clk */
	0x73,		/* ddrss_gcc_debug_clk (mc_cc_debug_mux) */
	0xAC,		/* measure_only_ipa_2x_clk */
	0x109,		/* measure_only_snoc_clk */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x79000,
	.post_div_offset = 0x35000,
	.cbcr_offset = 0x35004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
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
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
};

static struct clk_dummy measure_only_apcs_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_clk",
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

static struct clk_dummy measure_only_mccc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_mccc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_sdxlemur_hws[] = {
	&measure_only_ipa_2x_clk.hw,
	&measure_only_snoc_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_apcs_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,sdxlemur-debugcc" },
	{ }
};

static int clk_debug_sdxlemur_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0, i;

	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) !=
		ARRAY_SIZE(gcc_debug_mux_sels));

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

	for (i = 0; i < ARRAY_SIZE(debugcc_sdxlemur_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_sdxlemur_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_sdxlemur_hws[i]),
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
	.probe = clk_debug_sdxlemur_probe,
	.driver = {
		.name = "sdxlemur-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_sdxlemur_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_sdxlemur_init);

MODULE_DESCRIPTION("QTI DEBUG CC SDXLEMUR Driver");
MODULE_LICENSE("GPL v2");

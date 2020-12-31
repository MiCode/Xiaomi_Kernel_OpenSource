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
	.ctl_reg = 0x74004,
	.status_reg = 0x74008,
	.xo_div4_cbcr = 0x30034,
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
	.debug_offset = 0x0,
	.post_div_offset = 0x0,
	.cbcr_offset = U32_MAX,
	.src_sel_mask = 0x38,
	.src_sel_shift = 0x3,
	.post_div_mask = 0x0,
	.post_div_shift = 0x0,
	.post_div_val = 0x1,
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
	"gcc_apss_tcu_clk",
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
	"gcc_dcc_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_pcie_axi_clk",
	"gcc_pcie_axi_mstr_clk",
	"gcc_pcie_axi_tbu_clk",
	"gcc_pcie_cfg_ahb_clk",
	"gcc_pcie_pipe_clk",
	"gcc_pcie_sleep_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_prng_ahb_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_smmu_cfg_clk",
	"gcc_sys_noc_usb3_axi_clk",
	"gcc_usb30_master_clk",
	"gcc_usb30_mock_utmi_clk",
	"gcc_usb30_sleep_clk",
	"gcc_usb3_aux_clk",
	"gcc_usb3_axi_tbu_clk",
	"gcc_usb3_pipe_clk",
	"gcc_usb_phy_cfg_ahb_clk",
	"gcc_xo_div4_clk",
	"gcc_mss_q6_bimc_axi_clk",
	"gcc_mss_cfg_ahb_clk",
	"measure_only_bimc_clk",
	"measure_only_ce_clk",
	"measure_only_ipa_clk",
	"measure_only_pcnoc_clk",
	"measure_only_qdss_clk",
	"measure_only_qpic_clk",
	"measure_only_snoc_clk",
};

static int gcc_debug_mux_sels[] = {
	0x16A,		/* apss_cc_debug_mux */
	0x50,		/* gcc_apss_tcu_clk */
	0x88,		/* gcc_blsp1_ahb_clk */
	0x8B,		/* gcc_blsp1_qup1_i2c_apps_clk */
	0x8A,		/* gcc_blsp1_qup1_spi_apps_clk */
	0x90,		/* gcc_blsp1_qup2_i2c_apps_clk */
	0x8E,		/* gcc_blsp1_qup2_spi_apps_clk */
	0x94,		/* gcc_blsp1_qup3_i2c_apps_clk */
	0x93,		/* gcc_blsp1_qup3_spi_apps_clk */
	0x99,		/* gcc_blsp1_qup4_i2c_apps_clk */
	0x98,		/* gcc_blsp1_qup4_spi_apps_clk */
	0x89,		/* gcc_blsp1_sleep_clk */
	0x8C,		/* gcc_blsp1_uart1_apps_clk */
	0x91,		/* gcc_blsp1_uart2_apps_clk */
	0x95,		/* gcc_blsp1_uart3_apps_clk */
	0x9A,		/* gcc_blsp1_uart4_apps_clk */
	0xF8,		/* gcc_boot_rom_ahb_clk */
	0x278,		/* gcc_dcc_clk */
	0x10,		/* gcc_gp1_clk */
	0x11,		/* gcc_gp2_clk */
	0x12,		/* gcc_gp3_clk */
	0x23A,		/* gcc_pcie_axi_clk */
	0x23C,		/* gcc_pcie_axi_mstr_clk */
	0x204,		/* gcc_pcie_axi_tbu_clk */
	0x238,		/* gcc_pcie_cfg_ahb_clk */
	0x239,		/* gcc_pcie_pipe_clk */
	0x23B,		/* gcc_pcie_sleep_clk */
	0xD2,		/* gcc_pdm2_clk */
	0xD0,		/* gcc_pdm_ahb_clk */
	0xD1,		/* gcc_pdm_xo4_clk */
	0xD8,		/* gcc_prng_ahb_clk */
	0x69,		/* gcc_sdcc1_ahb_clk */
	0x68,		/* gcc_sdcc1_apps_clk */
	0x5B,		/* gcc_smmu_cfg_clk */
	0x1,		/* gcc_sys_noc_usb3_axi_clk */
	0x230,		/* gcc_usb30_master_clk */
	0x232,		/* gcc_usb30_mock_utmi_clk */
	0x231,		/* gcc_usb30_sleep_clk */
	0x235,		/* gcc_usb3_aux_clk */
	0x203,		/* gcc_usb3_axi_tbu_clk */
	0x234,		/* gcc_usb3_pipe_clk */
	0x233,		/* gcc_usb_phy_cfg_ahb_clk */
	0x14A,		/* gcc_xo_div4_clk */
	0x31,		/* gcc_mss_q6_bimc_axi_clk */
	0x30,		/* gcc_mss_cfg_ahb_clk */
	0x155,		/* measure_only_bimc_clk */
	0x138,		/* measure_only_ce_clk */
	0x218,		/* measure_only_ipa_clk */
	0x8,		/* measure_only_pcnoc_clk */
	0x42,		/* measure_only_qdss_clk */
	0x220,		/* measure_only_qpic_clk */
	0x0,		/* measure_only_snoc_clk */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x74000,
	.post_div_offset = 0x74000,
	.cbcr_offset = 0x74000,
	.en_mask = BIT(16),
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0x0,
	.post_div_mask = 0xF000,
	.post_div_shift = 12,
	.post_div_val = 0x4,		/*post_dev_val 0x3: DIV4*/
	.mux_sels = gcc_debug_mux_sels,
	.hw.init = &(struct clk_init_data){
		.name = "gcc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gcc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gcc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static struct mux_regmap_names mux_list[] = {
	{ .mux = &apss_cc_debug_mux, .regmap_name = "qcom,cpucc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
};

static struct clk_dummy measure_only_bimc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_bimc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ce_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_ce_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ipa_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_ipa_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcnoc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pcnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_qdss_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_qdss_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_qpic_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_qpic_clk",
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

static struct clk_dummy measure_only_apcs_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_sdxnightjar_hws[] = {
	&measure_only_bimc_clk.hw,
	&measure_only_ce_clk.hw,
	&measure_only_ipa_clk.hw,
	&measure_only_pcnoc_clk.hw,
	&measure_only_qdss_clk.hw,
	&measure_only_qpic_clk.hw,
	&measure_only_snoc_clk.hw,
	&measure_only_apcs_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,sdxnightjar-debugcc" },
	{ }
};

static int clk_debug_sdxnightjar_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret, i;

	BUILD_BUG_ON(ARRAY_SIZE(apss_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(apss_cc_debug_mux_sels));
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

	for (i = 0; i < ARRAY_SIZE(debugcc_sdxnightjar_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_sdxnightjar_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_sdxnightjar_hws[i]),
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
	.probe = clk_debug_sdxnightjar_probe,
	.driver = {
		.name = "sdxnightjar-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_sdxnightjar_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_sdxnightjar_init);

MODULE_DESCRIPTION("QTI DEBUG CC SDXNIGHTJAR Driver");
MODULE_LICENSE("GPL v2");

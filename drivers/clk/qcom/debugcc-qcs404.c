// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

static const char *const apcs_debug_mux_parent_names[] = {
	"measure_only_apcs_clk",
};

static int apcs_debug_mux_sels[] = {
	0x0,		/* measure_only_apcs_clk */
};

static int apcs_debug_mux_pre_divs[] = {
	0x1,		/* measure_only_apcs_clk */
};


static struct clk_debug_mux apcs_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x0,
	.post_div_offset = 0x0,
	.cbcr_offset = U32_MAX,
	.src_sel_mask = 0x300,
	.src_sel_shift = 8,
	.post_div_mask = 0x1E0000,
	.post_div_shift = 11,
	.post_div_val = 1,
	.mux_sels = apcs_debug_mux_sels,
	.pre_div_vals = apcs_debug_mux_pre_divs,
	.hw.init = &(struct clk_init_data){
		.name = "apcs_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apcs_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apcs_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};
static const char *const gcc_debug_mux_parent_names[] = {
	"gcc_apss_ahb_clk",
	"gcc_bimc_gfx_clk",
	"gcc_bimc_gpu_clk",
	"gcc_bimc_mdss_clk",
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
	"gcc_gfx_tbu_clk",
	"gcc_gfx_tcu_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_gtcu_ahb_clk",
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
	"gcc_smmu_cfg_clk",
	"gcc_sys_noc_usb3_clk",
	"gcc_usb20_mock_utmi_clk",
	"gcc_usb2a_phy_sleep_clk",
	"gcc_usb30_master_clk",
	"gcc_usb30_mock_utmi_clk",
	"gcc_usb30_phy_pipe_clk",
	"gcc_usb30_sleep_clk",
	"gcc_usb3_phy_aux_clk",
	"gcc_usb_hs_inactivity_timers_clk",
	"gcc_usb_hs_phy_cfg_ahb_clk",
	"gcc_usb_hs_system_clk",
	"gcc_apss_tcu_clk",
	"gcc_crypto_ahb_clk",
	"gcc_crypto_axi_clk",
	"gcc_mdp_tbu_clk",
	"gcc_qdss_dap_clk",
	"gcc_geni_ir_h_clk",
	"gcc_dcc_xo_clk",
	"pnoc_clk",
	"qpic_clk",
	"snoc_clk",
	"bimc_clk",
	"ce1_clk",
	"apcs_debug_mux",
};

static int gcc_debug_mux_sels[] = {
	0x168,		/* gcc_apss_ahb_clk */
	0x2D,		/* gcc_bimc_gfx_clk */
	0x157,		/* gcc_bimc_gpu_clk */
	0x15F,		/* gcc_bimc_mdss_clk */
	0x88,		/* gcc_blsp1_ahb_clk */
	0x99,		/* gcc_blsp1_qup0_i2c_apps_clk */
	0x98,		/* gcc_blsp1_qup0_spi_apps_clk */
	0x8B,		/* gcc_blsp1_qup1_i2c_apps_clk */
	0x8A,		/* gcc_blsp1_qup1_spi_apps_clk */
	0x8F,		/* gcc_blsp1_qup2_i2c_apps_clk */
	0x8E,		/* gcc_blsp1_qup2_spi_apps_clk */
	0x93,		/* gcc_blsp1_qup3_i2c_apps_clk */
	0x92,		/* gcc_blsp1_qup3_spi_apps_clk */
	0x95,		/* gcc_blsp1_qup4_i2c_apps_clk */
	0x94,		/* gcc_blsp1_qup4_spi_apps_clk */
	0x9A,		/* gcc_blsp1_uart0_apps_clk */
	0x8C,		/* gcc_blsp1_uart1_apps_clk */
	0x90,		/* gcc_blsp1_uart2_apps_clk */
	0x96,		/* gcc_blsp1_uart3_apps_clk */
	0xA0,		/* gcc_blsp2_ahb_clk */
	0xA3,		/* gcc_blsp2_qup0_i2c_apps_clk */
	0xA2,		/* gcc_blsp2_qup0_spi_apps_clk */
	0xA4,		/* gcc_blsp2_uart0_apps_clk */
	0xF8,		/* gcc_boot_rom_ahb_clk */
	0x1B9,		/* gcc_dcc_clk */
	0x80,		/* gcc_eth_axi_clk */
	0x83,		/* gcc_eth_ptp_clk */
	0x81,		/* gcc_eth_rgmii_clk */
	0x82,		/* gcc_eth_slave_ahb_clk */
	0xEE,		/* gcc_geni_ir_s_clk */
	0x52,		/* gcc_gfx_tbu_clk */
	0x53,		/* gcc_gfx_tcu_clk */
	0x10,		/* gcc_gp1_clk */
	0x11,		/* gcc_gp2_clk */
	0x12,		/* gcc_gp3_clk */
	0x58,		/* gcc_gtcu_ahb_clk */
	0x1F6,		/* gcc_mdss_ahb_clk */
	0x1F7,		/* gcc_mdss_axi_clk */
	0x1FC,		/* gcc_mdss_byte0_clk */
	0x1FD,		/* gcc_mdss_esc0_clk */
	0x1F2,		/* gcc_mdss_hdmi_app_clk */
	0x1F1,		/* gcc_mdss_hdmi_pclk_clk */
	0x1F9,		/* gcc_mdss_mdp_clk */
	0x1F8,		/* gcc_mdss_pclk0_clk */
	0x1FB,		/* gcc_mdss_vsync_clk */
	0x1EB,		/* gcc_oxili_ahb_clk */
	0x1EA,		/* gcc_oxili_gfx3d_clk */
	0xAB,		/* gcc_pcie_0_aux_clk */
	0xAA,		/* gcc_pcie_0_cfg_ahb_clk */
	0xA9,		/* gcc_pcie_0_mstr_axi_clk */
	0xAC,		/* gcc_pcie_0_pipe_clk */
	0xA8,		/* gcc_pcie_0_slv_axi_clk */
	0x9,		/* gcc_pcnoc_usb2_clk */
	0xA,		/* gcc_pcnoc_usb3_clk */
	0xD2,		/* gcc_pdm2_clk */
	0xD0,		/* gcc_pdm_ahb_clk */
	0xD8,		/* gcc_prng_ahb_clk */
	0xD3,		/* gcc_pwm0_xo512_clk */
	0xD4,		/* gcc_pwm1_xo512_clk */
	0xD5,		/* gcc_pwm2_xo512_clk */
	0x69,		/* gcc_sdcc1_ahb_clk */
	0x68,		/* gcc_sdcc1_apps_clk */
	0x6A,		/* gcc_sdcc1_ice_core_clk */
	0x71,		/* gcc_sdcc2_ahb_clk */
	0x70,		/* gcc_sdcc2_apps_clk */
	0x5B,		/* gcc_smmu_cfg_clk */
	0x1,		/* gcc_sys_noc_usb3_clk */
	0x65,		/* gcc_usb20_mock_utmi_clk */
	0x63,		/* gcc_usb2a_phy_sleep_clk */
	0x78,		/* gcc_usb30_master_clk */
	0x7A,		/* gcc_usb30_mock_utmi_clk */
	0x7B,		/* gcc_usb30_phy_pipe_clk */
	0x79,		/* gcc_usb30_sleep_clk */
	0x7C,		/* gcc_usb3_phy_aux_clk */
	0x62,		/* gcc_usb_hs_inactivity_timers_clk */
	0x64,		/* gcc_usb_hs_phy_cfg_ahb_clk */
	0x60,		/* gcc_usb_hs_system_clk */
	0x159,		/* gcc_apss_tcu_clk */
	0x13A,		/* gcc_crypto_ahb_clk */
	0x139,		/* gcc_crypto_axi_clk */
	0x051,		/* gcc_mdp_tbu_clk */
	0x049,		/* gcc_qdss_dap_clk */
	0x0EC,		/* gcc_geni_ir_h_clk */
	0x1B8,		/* gcc_dcc_xo_clk */
	0x008,		/* pnoc_clk */
	0xC0,		/* qpic_clk */
	0x0,		/* snoc_clk */
	0x15A,		/* bimc_clk */
	0x138,		/* ce1_clk */
	0x16A,		/* apcs_debug_mux */
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
	.post_div_val = 1,
	.en_mask = BIT(16),
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
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
	{ .mux = &apcs_debug_mux, .regmap_name = "qcom,cpucc" },
};

static struct clk_dummy measure_only_apcs_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_apcs_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_qcs404_hws[] = {
	&measure_only_apcs_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,qcs404-debugcc" },
	{ }
};

static int clk_debug_qcs404_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret, i;

	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) != ARRAY_SIZE(gcc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(apcs_debug_mux_parent_names) != ARRAY_SIZE(apcs_debug_mux_sels));

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
		if (!mux_list[i].mux->regmap)
			continue;

		clk = devm_clk_register(&pdev->dev, &mux_list[i].mux->hw);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register mux clk %s, err:(%d)\n",
				clk_hw_get_name(&mux_list[i].mux->hw),
				PTR_ERR(clk));
			return PTR_ERR(clk);
		}
	}

	for (i = 0; i < ARRAY_SIZE(debugcc_qcs404_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_qcs404_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_qcs404_hws[i]),
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
	.probe = clk_debug_qcs404_probe,
	.driver = {
		.name = "qcs404-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_qcs404_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_qcs404_init);

MODULE_DESCRIPTION("QTI DEBUG CC QCS404 Driver");
MODULE_LICENSE("GPL v2");

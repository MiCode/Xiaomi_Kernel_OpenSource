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
	.ctl_reg = 0x89004,
	.status_reg = 0x89008,
	.xo_div4_cbcr = 0x45008,
};

static const char *const apss_cc_debug_mux_parent_names[] = {
	"measure_only_apcs_l3_post_acd_clk",
	"measure_only_apcs_silver_post_acd_clk",
};

static int apss_cc_debug_mux_sels[] = {
	0x41,		/* measure_only_apcs_l3_post_acd_clk */
	0x21,		/* measure_only_apcs_silver_post_acd_clk */
};

static int apss_cc_debug_mux_pre_divs[] = {
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
	.num_mux_sels = ARRAY_SIZE(apss_cc_debug_mux_sels),
	.pre_div_vals = apss_cc_debug_mux_pre_divs,
	.hw.init = &(const struct clk_init_data){
		.name = "apss_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apss_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apss_cc_debug_mux_parent_names),
	},
};

static const char *const gcc_debug_mux_parent_names[] = {
	"apss_cc_debug_mux",
	"gcc_boot_rom_ahb_clk",
	"gcc_eee_emac0_clk",
	"gcc_eee_emac1_clk",
	"gcc_emac0_axi_clk",
	"gcc_emac0_cc_sgmiiphy_rx_clk",
	"gcc_emac0_cc_sgmiiphy_tx_clk",
	"gcc_emac0_phy_aux_clk",
	"gcc_emac0_ptp_clk",
	"gcc_emac0_rgmii_clk",
	"gcc_emac0_rpcs_rx_clk",
	"gcc_emac0_rpcs_tx_clk",
	"gcc_emac0_slv_ahb_clk",
	"gcc_emac0_xgxs_rx_clk",
	"gcc_emac0_xgxs_tx_clk",
	"gcc_emac1_axi_clk",
	"gcc_emac1_cc_sgmiiphy_rx_clk",
	"gcc_emac1_cc_sgmiiphy_tx_clk",
	"gcc_emac1_phy_aux_clk",
	"gcc_emac1_ptp_clk",
	"gcc_emac1_rgmii_clk",
	"gcc_emac1_rpcs_rx_clk",
	"gcc_emac1_rpcs_tx_clk",
	"gcc_emac1_slv_ahb_clk",
	"gcc_emac1_xgxs_rx_clk",
	"gcc_emac1_xgxs_tx_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_mvm_ahb_clk",
	"gcc_mvm_master_axi_clk",
	"gcc_mvmss_nts_clk",
	"gcc_pcie_1_aux_clk",
	"gcc_pcie_1_cfg_ahb_clk",
	"gcc_pcie_1_mstr_axi_clk",
	"gcc_pcie_1_phy_rchng_clk",
	"gcc_pcie_1_pipe_clk",
	"gcc_pcie_1_pipe_div2_clk",
	"gcc_pcie_1_slv_axi_clk",
	"gcc_pcie_1_slv_q2a_axi_clk",
	"gcc_pcie_2_aux_clk",
	"gcc_pcie_2_cfg_ahb_clk",
	"gcc_pcie_2_mstr_axi_clk",
	"gcc_pcie_2_phy_rchng_clk",
	"gcc_pcie_2_pipe_clk",
	"gcc_pcie_2_pipe_div2_clk",
	"gcc_pcie_2_slv_axi_clk",
	"gcc_pcie_2_slv_q2a_axi_clk",
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
	"gcc_qupv3_wrap0_s8_clk",
	"gcc_qupv3_wrap_0_m_ahb_clk",
	"gcc_qupv3_wrap_0_s_ahb_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_sys_noc_mvmss_clk",
	"gcc_usb30_master_clk",
	"gcc_usb30_mock_utmi_clk",
	"gcc_usb30_mstr_axi_clk",
	"gcc_usb30_sleep_clk",
	"gcc_usb30_slv_ahb_clk",
	"gcc_usb3_phy_aux_clk",
	"gcc_usb3_phy_pipe_clk",
	"gcc_usb_phy_cfg_ahb2phy_clk",
	"gcc_xo_div4_clk",
	"mc_cc_debug_mux",
	"measure_only_gcc_ahb_pcie_link_clk",
	"measure_only_gcc_xo_pcie_link_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_pcie20_phy_aux_clk",
	"measure_only_pcie_1_pipe_clk",
	"measure_only_pcie_2_pipe_clk",
	"measure_only_pcie_pipe_clk",
	"measure_only_snoc_clk",
	"measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
};

static int gcc_debug_mux_sels[] = {
	0x89,		/* apss_cc_debug_mux */
	0x59,		/* gcc_boot_rom_ahb_clk */
	0x135,		/* gcc_eee_emac0_clk */
	0x140,		/* gcc_eee_emac1_clk */
	0xE2,		/* gcc_emac0_axi_clk */
	0x131,		/* gcc_emac0_cc_sgmiiphy_rx_clk */
	0x130,		/* gcc_emac0_cc_sgmiiphy_tx_clk */
	0xE4,		/* gcc_emac0_phy_aux_clk */
	0xE5,		/* gcc_emac0_ptp_clk */
	0xE6,		/* gcc_emac0_rgmii_clk */
	0x13B,		/* gcc_emac0_rpcs_rx_clk */
	0x13A,		/* gcc_emac0_rpcs_tx_clk */
	0xE3,		/* gcc_emac0_slv_ahb_clk */
	0x136,		/* gcc_emac0_xgxs_rx_clk */
	0x138,		/* gcc_emac0_xgxs_tx_clk */
	0xEB,		/* gcc_emac1_axi_clk */
	0x133,		/* gcc_emac1_cc_sgmiiphy_rx_clk */
	0x132,		/* gcc_emac1_cc_sgmiiphy_tx_clk */
	0xED,		/* gcc_emac1_phy_aux_clk */
	0xEE,		/* gcc_emac1_ptp_clk */
	0xEF,		/* gcc_emac1_rgmii_clk */
	0x13D,		/* gcc_emac1_rpcs_rx_clk */
	0x13C,		/* gcc_emac1_rpcs_tx_clk */
	0xEC,		/* gcc_emac1_slv_ahb_clk */
	0x13F,		/* gcc_emac1_xgxs_rx_clk */
	0x13E,		/* gcc_emac1_xgxs_tx_clk */
	0x98,		/* gcc_gp1_clk */
	0x99,		/* gcc_gp2_clk */
	0x9A,		/* gcc_gp3_clk */
	0xD0,		/* gcc_mvm_ahb_clk */
	0xCE,		/* gcc_mvm_master_axi_clk */
	0xBB,		/* gcc_mvmss_nts_clk */
	0xAD,		/* gcc_pcie_1_aux_clk */
	0xAC,		/* gcc_pcie_1_cfg_ahb_clk */
	0xAB,		/* gcc_pcie_1_mstr_axi_clk */
	0xAF,		/* gcc_pcie_1_phy_rchng_clk */
	0xAE,		/* gcc_pcie_1_pipe_clk */
	0xBC,		/* gcc_pcie_1_pipe_div2_clk */
	0xAA,		/* gcc_pcie_1_slv_axi_clk */
	0xA9,		/* gcc_pcie_1_slv_q2a_axi_clk */
	0xB7,		/* gcc_pcie_2_aux_clk */
	0xB6,		/* gcc_pcie_2_cfg_ahb_clk */
	0xB5,		/* gcc_pcie_2_mstr_axi_clk */
	0xB9,		/* gcc_pcie_2_phy_rchng_clk */
	0xB8,		/* gcc_pcie_2_pipe_clk */
	0xD3,		/* gcc_pcie_2_pipe_div2_clk */
	0xB4,		/* gcc_pcie_2_slv_axi_clk */
	0xB3,		/* gcc_pcie_2_slv_q2a_axi_clk */
	0xA2,		/* gcc_pcie_aux_clk */
	0xA0,		/* gcc_pcie_cfg_ahb_clk */
	0x9F,		/* gcc_pcie_mstr_axi_clk */
	0xA4,		/* gcc_pcie_pipe_clk */
	0xA1,		/* gcc_pcie_rchng_phy_clk */
	0xA3,		/* gcc_pcie_sleep_clk */
	0x9E,		/* gcc_pcie_slv_axi_clk */
	0x9D,		/* gcc_pcie_slv_q2a_axi_clk */
	0x55,		/* gcc_pdm2_clk */
	0x53,		/* gcc_pdm_ahb_clk */
	0x54,		/* gcc_pdm_xo4_clk */
	0x4A,		/* gcc_qupv3_wrap0_core_2x_clk */
	0x49,		/* gcc_qupv3_wrap0_core_clk */
	0x4B,		/* gcc_qupv3_wrap0_s0_clk */
	0x4C,		/* gcc_qupv3_wrap0_s1_clk */
	0x4D,		/* gcc_qupv3_wrap0_s2_clk */
	0x4E,		/* gcc_qupv3_wrap0_s3_clk */
	0x4F,		/* gcc_qupv3_wrap0_s4_clk */
	0x50,		/* gcc_qupv3_wrap0_s5_clk */
	0x51,		/* gcc_qupv3_wrap0_s6_clk */
	0x52,		/* gcc_qupv3_wrap0_s7_clk */
	0x14C,		/* gcc_qupv3_wrap0_s8_clk */
	0x47,		/* gcc_qupv3_wrap_0_m_ahb_clk */
	0x48,		/* gcc_qupv3_wrap_0_s_ahb_clk */
	0x44,		/* gcc_sdcc1_ahb_clk */
	0x45,		/* gcc_sdcc1_apps_clk */
	0x42,		/* gcc_sdcc2_ahb_clk */
	0x41,		/* gcc_sdcc2_apps_clk */
	0x19,		/* gcc_sys_noc_mvmss_clk */
	0x35,		/* gcc_usb30_master_clk */
	0x39,		/* gcc_usb30_mock_utmi_clk */
	0x36,		/* gcc_usb30_mstr_axi_clk */
	0x38,		/* gcc_usb30_sleep_clk */
	0x37,		/* gcc_usb30_slv_ahb_clk */
	0x3A,		/* gcc_usb3_phy_aux_clk */
	0x3B,		/* gcc_usb3_phy_pipe_clk */
	0x40,		/* gcc_usb_phy_cfg_ahb2phy_clk */
	0x74,		/* gcc_xo_div4_clk */
	0x77,		/* mc_cc_debug_mux */
	0x71,		/* measure_only_gcc_ahb_pcie_link_clk */
	0x72,		/* measure_only_gcc_xo_pcie_link_clk */
	0xC8,		/* measure_only_ipa_2x_clk */
	0xA8,		/* measure_only_pcie20_phy_aux_clk */
	0xB0,		/* measure_only_pcie_1_pipe_clk */
	0xBA,		/* measure_only_pcie_2_pipe_clk */
	0xA7,		/* measure_only_pcie_pipe_clk */
	0xA,		/* measure_only_snoc_clk */
	0x3C,		/* measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x89000,
	.post_div_offset = 0x45000,
	.cbcr_offset = 0x45004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = gcc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(gcc_debug_mux_sels),
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
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
	{ .mux = &apss_cc_debug_mux, .regmap_name = "qcom,apsscc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
};

static struct clk_dummy measure_only_apcs_l3_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_l3_post_acd_clk",
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

static struct clk_dummy measure_only_gcc_ahb_pcie_link_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_ahb_pcie_link_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_xo_pcie_link_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_xo_pcie_link_clk",
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

static struct clk_dummy measure_only_pcie20_phy_aux_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie20_phy_aux_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcie_1_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie_1_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcie_2_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie_2_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcie_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie_pipe_clk",
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

static struct clk_dummy measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_sdxpinn_hws[] = {
	&measure_only_apcs_l3_post_acd_clk.hw,
	&measure_only_apcs_silver_post_acd_clk.hw,
	&measure_only_gcc_ahb_pcie_link_clk.hw,
	&measure_only_gcc_xo_pcie_link_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_pcie20_phy_aux_clk.hw,
	&measure_only_pcie_1_pipe_clk.hw,
	&measure_only_pcie_2_pipe_clk.hw,
	&measure_only_pcie_pipe_clk.hw,
	&measure_only_snoc_clk.hw,
	&measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,sdxpinn-debugcc" },
	{ }
};

static int clk_debug_sdxpinn_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0, i;

	BUILD_BUG_ON(ARRAY_SIZE(apss_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(apss_cc_debug_mux_sels));
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
			ret = map_debug_bases(pdev, mux_list[i].regmap_name,
					      mux_list[i].mux);
			if (ret == -EBADR)
				continue;
			else if (ret)
				return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(debugcc_sdxpinn_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_sdxpinn_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_sdxpinn_hws[i]),
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
	.probe = clk_debug_sdxpinn_probe,
	.driver = {
		.name = "sdxpinn-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_sdxpinn_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_sdxpinn_init);

MODULE_DESCRIPTION("QTI DEBUG CC SDXPINN Driver");
MODULE_LICENSE("GPL v2");

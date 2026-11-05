// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
	.xo_div4_cbcr = 0x3E010,
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
	.debug_offset = 0x1c,
	.post_div_offset = 0x0,
	.cbcr_offset = 0x0,
	.src_sel_mask = 0x38,
	.src_sel_shift = 3,
	.post_div_mask = 0x0,
	.post_div_shift = 0x0,
	.post_div_val = 1,
	.mux_sels = apss_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(apss_cc_debug_mux_sels),
	.pre_div_vals = apss_cc_debug_mux_pre_divs,
	.hw.init = &(struct clk_init_data){
		.name = "apss_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apss_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apss_cc_debug_mux_parent_names),
	},
};

static const char *const gcc_debug_mux_parent_names[] = {
	"apss_cc_debug_mux",
	"gcc_ahb_pcie_link_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_cpuss_ahb_clk",
	"gcc_emac0_axi_clk",
	"gcc_emac0_phy_aux_clk",
	"gcc_emac0_ptp_clk",
	"gcc_emac0_rgmii_clk",
	"gcc_emac0_slv_ahb_clk",
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
	"gcc_qupv3_wrap0_core_2x_clk",
	"gcc_qupv3_wrap0_core_clk",
	"gcc_qupv3_wrap0_s0_clk",
	"gcc_qupv3_wrap0_s1_clk",
	"gcc_qupv3_wrap0_s2_clk",
	"gcc_qupv3_wrap0_s3_clk",
	"gcc_qupv3_wrap0_s4_clk",
	"gcc_qupv3_wrap_0_m_ahb_clk",
	"gcc_qupv3_wrap_0_s_ahb_clk",
	"gcc_sdcc4_ahb_clk",
	"gcc_sdcc4_apps_clk",
	"gcc_snoc_cnoc_usb3_clk",
	"gcc_sys_noc_usb_sf_axi_clk",
	"gcc_usb20_master_clk",
	"gcc_usb20_mock_utmi_clk",
	"gcc_usb20_sleep_clk",
	"gcc_usb_phy_cfg_ahb2phy_clk",
	"gcc_xo_div4_clk",
	"gcc_xo_pcie_link_clk",
	"mc_cc_debug_mux",
	"measure_only_ipa_2x_clk",
	"measure_only_pcie_pipe_clk",
	"measure_only_qpic_ahb_clk",
	"measure_only_qpic_clk",
	"measure_only_qpic_system_clk",
	"measure_only_snoc_clk",
};

static int gcc_debug_mux_sels[] = {
	0x89,		/* apss_cc_debug_mux */
	0x71,		/* gcc_ahb_pcie_link_clk */
	0x59,		/* gcc_boot_rom_ahb_clk */
	0x86,		/* gcc_cpuss_ahb_clk */
	0xE2,		/* gcc_emac0_axi_clk */
	0xE4,		/* gcc_emac0_phy_aux_clk */
	0xE5,		/* gcc_emac0_ptp_clk */
	0xE6,		/* gcc_emac0_rgmii_clk */
	0xE3,		/* gcc_emac0_slv_ahb_clk */
	0x98,		/* gcc_gp1_clk */
	0x99,		/* gcc_gp2_clk */
	0x9A,		/* gcc_gp3_clk */
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
	0x47,		/* gcc_qupv3_wrap_0_m_ahb_clk */
	0x48,		/* gcc_qupv3_wrap_0_s_ahb_clk */
	0x42,		/* gcc_sdcc4_ahb_clk */
	0x41,		/* gcc_sdcc4_apps_clk */
	0x12,		/* gcc_snoc_cnoc_usb3_clk */
	0x127,		/* gcc_sys_noc_usb_sf_axi_clk */
	0x35,		/* gcc_usb20_master_clk */
	0x39,		/* gcc_usb20_mock_utmi_clk */
	0x38,		/* gcc_usb20_sleep_clk */
	0x40,		/* gcc_usb_phy_cfg_ahb2phy_clk */
	0x74,		/* gcc_xo_div4_clk */
	0x72,		/* gcc_xo_pcie_link_clk */
	0x77,		/* mc_cc_debug_mux */
	0xC8,		/* measure_only_ipa_2x_clk */
	0xA7,		/* measure_only_pcie_pipe_clk */
	0xFB,		/* measure_only_qpic_ahb_clk */
	0x109,		/* measure_only_qpic_clk */
	0xFC,		/* measure_only_qpic_system_clk */
	0xA,		/* measure_only_snoc_clk */
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
	{ .mux = &apss_cc_debug_mux, .regmap_name = "qcom,apsscc" },
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
};

static struct clk_dummy measure_only_apcs_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_clk",
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

static struct clk_dummy measure_only_pcie_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_qpic_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_qpic_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_qpic_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_qpic_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_qpic_system_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_qpic_system_clk",
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

static struct clk_hw *debugcc_sdxbaagha_hws[] = {
	&measure_only_apcs_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_pcie_pipe_clk.hw,
	&measure_only_qpic_ahb_clk.hw,
	&measure_only_qpic_clk.hw,
	&measure_only_qpic_system_clk.hw,
	&measure_only_snoc_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,sdxbaagha-debugcc" },
	{ }
};

static int clk_debug_sdxbaagha_probe(struct platform_device *pdev)
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

	/* Update the mux_sel value of apss_cc_debug_mux  */
	regmap_write(mux_list[0].mux->regmap, 0x1c, 0x18);

	for (i = 0; i < ARRAY_SIZE(debugcc_sdxbaagha_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_sdxbaagha_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%ld)\n",
				clk_hw_get_name(debugcc_sdxbaagha_hws[i]),
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
	.probe = clk_debug_sdxbaagha_probe,
	.driver = {
		.name = "sdxbaagha-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_sdxbaagha_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_sdxbaagha_init);

MODULE_DESCRIPTION("QTI DEBUG CC SDXBAAGHA Driver");
MODULE_LICENSE("GPL");

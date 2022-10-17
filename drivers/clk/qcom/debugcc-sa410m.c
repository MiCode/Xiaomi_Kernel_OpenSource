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
	.ctl_reg = 0x62038,
	.status_reg = 0x6203C,
	.xo_div4_cbcr = 0x28008,
};

static const char *const apcs_debug_mux_parent_names[] = {
	"measure_only_pwrcl_clk",
};

static int apcs_debug_mux_sels[] = {
	0x0,
};

static int apcs_debug_mux_pre_divs[] = {
	0x8,
};

static struct clk_debug_mux apcs_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x0,
	.post_div_offset = 0x0,
	.cbcr_offset = 0x0,
	.src_sel_mask = 0x3FF00,
	.src_sel_shift = 8,
	.post_div_mask = 0xF0000000,
	.post_div_shift = 28,
	.post_div_val = 1,
	.mux_sels = apcs_debug_mux_sels,
	.pre_div_vals = apcs_debug_mux_pre_divs,
	.hw.init = &(struct clk_init_data){
		.name = "apcs_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apcs_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apcs_debug_mux_parent_names),
	 },
};

static const char *const gcc_debug_mux_parent_names[] = {
	"apcs_debug_mux",
	"gcc_ahb2phy_csi_clk",
	"gcc_ahb2phy_usb_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_cam_throttle_rt_clk",
	"gcc_cfg_noc_usb3_prim_axi_clk",
	"gcc_cpuss_gnoc_clk",
	"gcc_disp_throttle_core_clk",
	"gcc_emac0_axi_clk",
	"gcc_emac0_phy_aux_clk",
	"gcc_emac0_ptp_clk",
	"gcc_emac0_rgmii_clk",
	"gcc_emac0_slv_ahb_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_pcie_0_aux_clk",
	"gcc_pcie_0_cfg_ahb_clk",
	"gcc_pcie_0_mstr_axi_clk",
	"gcc_pcie_0_pipe_clk",
	"gcc_pcie_0_slv_axi_clk",
	"gcc_pcie_0_slv_q2a_axi_clk",
	"gcc_pcie_throttle_nrt_clk",
	"gcc_pcie_throttle_xo_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_pwm0_xo512_clk",
	"gcc_qmip_pcie_ahb_clk",
	"gcc_qupv3_wrap0_core_2x_clk",
	"gcc_qupv3_wrap0_core_clk",
	"gcc_qupv3_wrap0_s0_clk",
	"gcc_qupv3_wrap0_s1_clk",
	"gcc_qupv3_wrap0_s2_clk",
	"gcc_qupv3_wrap0_s3_clk",
	"gcc_qupv3_wrap0_s4_clk",
	"gcc_qupv3_wrap0_s5_clk",
	"gcc_qupv3_wrap_0_m_ahb_clk",
	"gcc_qupv3_wrap_0_s_ahb_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_sys_noc_usb3_prim_axi_clk",
	"gcc_usb30_prim_master_clk",
	"gcc_usb30_prim_mock_utmi_clk",
	"gcc_usb30_prim_sleep_clk",
	"gcc_usb3_prim_phy_com_aux_clk",
	"gcc_usb3_prim_phy_pipe_clk",
	"pcie_0_pipe_clk",
	"usb3_phy_wrapper_gcc_usb30_pipe_clk",
	"measure_only_cnoc_clk",
	"measure_only_gcc_sys_noc_cpuss_ahb_clk",
	"measure_only_hwkm_ahb_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_mccc_clk",
	"measure_only_pka_ahb_clk",
	"measure_only_pka_core_clk",
	"measure_only_qpic_clk",
	"measure_only_qpic_ahb_clk",
	"measure_only_snoc_clk",
};

static int gcc_debug_mux_sels[] = {
	0xAB,		/* apcs_debug_mux */
	0x12D,		/* gcc_ahb2phy_csi_clk */
	0x12E,		/* gcc_ahb2phy_usb_clk */
	0x140,		/* gcc_boot_rom_ahb_clk */
	0x115,		/* gcc_cam_throttle_rt_clk */
	0x1A,		/* gcc_cfg_noc_usb3_prim_axi_clk */
	0x170,		/* gcc_cpuss_gnoc_clk */
	0x114,		/* gcc_disp_throttle_core_clk */
	0x1C5,		/* gcc_emac0_axi_clk */
	0x1C9,		/* gcc_emac0_phy_aux_clk */
	0x1C7,		/* gcc_emac0_ptp_clk */
	0x1C8,		/* gcc_emac0_rgmii_clk */
	0x1C6,		/* gcc_emac0_slv_ahb_clk */
	0x180,		/* gcc_gp1_clk */
	0x181,		/* gcc_gp2_clk */
	0x182,		/* gcc_gp3_clk */
	0x1D2,		/* gcc_pcie_0_aux_clk */
	0x1D1,		/* gcc_pcie_0_cfg_ahb_clk */
	0x1D0,		/* gcc_pcie_0_mstr_axi_clk */
	0x1D4,		/* gcc_pcie_0_pipe_clk */
	0x1CF,		/* gcc_pcie_0_slv_axi_clk */
	0x1CE,		/* gcc_pcie_0_slv_q2a_axi_clk */
	0x116,		/* gcc_pcie_throttle_nrt_clk */
	0x1D3,		/* gcc_pcie_throttle_xo_clk */
	0x13D,		/* gcc_pdm2_clk */
	0x13B,		/* gcc_pdm_ahb_clk */
	0x13C,		/* gcc_pdm_xo4_clk */
	0x13E,		/* gcc_pwm0_xo512_clk */
	0x1D5,		/* gcc_qmip_pcie_ahb_clk */
	0x134,		/* gcc_qupv3_wrap0_core_2x_clk */
	0x133,		/* gcc_qupv3_wrap0_core_clk */
	0x135,		/* gcc_qupv3_wrap0_s0_clk */
	0x136,		/* gcc_qupv3_wrap0_s1_clk */
	0x137,		/* gcc_qupv3_wrap0_s2_clk */
	0x138,		/* gcc_qupv3_wrap0_s3_clk */
	0x139,		/* gcc_qupv3_wrap0_s4_clk */
	0x13A,		/* gcc_qupv3_wrap0_s5_clk */
	0x131,		/* gcc_qupv3_wrap_0_m_ahb_clk */
	0x132,		/* gcc_qupv3_wrap_0_s_ahb_clk */
	0x1AE,		/* gcc_sdcc1_ahb_clk */
	0x1AD,		/* gcc_sdcc1_apps_clk */
	0x1AF,		/* gcc_sdcc1_ice_core_clk */
	0x130,		/* gcc_sdcc2_ahb_clk */
	0x12F,		/* gcc_sdcc2_apps_clk */
	0x16,		/* gcc_sys_noc_usb3_prim_axi_clk */
	0x126,		/* gcc_usb30_prim_master_clk */
	0x128,		/* gcc_usb30_prim_mock_utmi_clk */
	0x127,		/* gcc_usb30_prim_sleep_clk */
	0x129,		/* gcc_usb3_prim_phy_com_aux_clk */
	0x12A,		/* gcc_usb3_prim_phy_pipe_clk */
	0x1D6,		/* pcie_0_pipe_clk */
	0x12B,		/* usb3_phy_wrapper_gcc_usb30_pipe_clk */
	0x17,		/* measure_only_cnoc_clk */
	0x9,		/* measure_only_gcc_sys_noc_cpuss_ahb_clk */
	0x16C,		/* measure_only_hwkm_ahb_clk */
	0x18C,		/* measure_only_ipa_2x_clk */
	0x165,		/* measure_only_mccc_clk */
	0x16E,		/* measure_only_pka_ahb_clk */
	0x16D,		/* measure_only_pka_core_clk */
	0x166,		/* measure_only_qpic_clk */
	0x168,		/* measure_only_qpic_ahb_clk */
	0x7,		/* measure_only_snoc_clk */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x62000,
	.post_div_offset = 0x30000,
	.cbcr_offset = 0x30004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 1,
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
	{ .mux = &apcs_debug_mux, .regmap_name = "qcom,cpucc" },
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
};

static struct clk_dummy measure_only_cnoc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_cnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_sys_noc_cpuss_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_gcc_sys_noc_cpuss_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_hwkm_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_hwkm_ahb_clk",
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

static struct clk_dummy measure_only_qpic_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_qpic_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_qpic_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_qpic_ahb_clk",
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

static struct clk_dummy measure_only_pka_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pka_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pka_core_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pka_core_clk",
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

static struct clk_dummy measure_only_pwrcl_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pwrcl_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_sa410m_hws[] = {
	&measure_only_pwrcl_clk.hw,
	&measure_only_cnoc_clk.hw,
	&measure_only_gcc_sys_noc_cpuss_ahb_clk.hw,
	&measure_only_hwkm_ahb_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_pka_ahb_clk.hw,
	&measure_only_pka_core_clk.hw,
	&measure_only_qpic_clk.hw,
	&measure_only_qpic_ahb_clk.hw,
	&measure_only_snoc_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,sa410m-debugcc" },
	{ }
};

static int clk_debug_sa410m_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0, i;

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

	for (i = 0; i < ARRAY_SIZE(debugcc_sa410m_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_sa410m_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_sa410m_hws[i]),
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
	.probe = clk_debug_sa410m_probe,
	.driver = {
		.name = "sa410m-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_sa410m_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_sa410m_init);

MODULE_DESCRIPTION("QTI DEBUG CC SA410M Driver");
MODULE_LICENSE("GPL v2");

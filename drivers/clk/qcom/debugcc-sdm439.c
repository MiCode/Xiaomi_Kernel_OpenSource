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

static const char *const apss_cc_debug_mux_parent_names[] = {
	"pwrcl_clk",
	"perfcl_clk",
};

static int apss_cc_debug_mux_sels[] = {
	0x0,		/* pwrcl_clk */
	0x1,		/* perfcl_clk */
};

static int apss_cc_debug_mux_pre_divs[] = {
	0x1,		/* pwrcl_clk */
	0x1,		/* perfcl_clk */
};

static struct clk_debug_mux apss_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x0,
	.post_div_offset = 0x0,
	.cbcr_offset = U32_MAX,
	.src_sel_mask = 0x3FF00,
	.src_sel_shift = 8,
	.post_div_mask = 0xF0000000,
	.post_div_shift = 28,
	.post_div_val = 2,
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
	"gcc_ahb_clk",
	"gcc_apss_ahb_clk",
	"gcc_apss_axi_clk",
	"gcc_bimc_gfx_clk",
	"gcc_bimc_gpu_clk",
	"gcc_blsp1_ahb_clk",
	"gcc_blsp1_qup2_i2c_apps_clk",
	"gcc_blsp1_qup2_spi_apps_clk",
	"gcc_blsp1_qup3_i2c_apps_clk",
	"gcc_blsp1_qup3_spi_apps_clk",
	"gcc_blsp1_qup4_i2c_apps_clk",
	"gcc_blsp1_qup4_spi_apps_clk",
	"gcc_blsp1_sleep_clk",
	"gcc_blsp1_uart1_apps_clk",
	"gcc_blsp1_uart1_sim_clk",
	"gcc_blsp1_uart2_apps_clk",
	"gcc_blsp1_uart2_sim_clk",
	"gcc_blsp2_ahb_clk",
	"gcc_blsp2_qup1_i2c_apps_clk",
	"gcc_blsp2_qup1_spi_apps_clk",
	"gcc_blsp2_qup2_i2c_apps_clk",
	"gcc_blsp2_qup2_spi_apps_clk",
	"gcc_blsp2_qup3_i2c_apps_clk",
	"gcc_blsp2_qup3_spi_apps_clk",
	"gcc_blsp2_sleep_clk",
	"gcc_blsp2_uart1_apps_clk",
	"gcc_blsp2_uart1_sim_clk",
	"gcc_blsp2_uart2_apps_clk",
	"gcc_blsp2_uart2_sim_clk",
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
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_im_sleep_clk",
	"gcc_lpass_mport_axi_clk",
	"gcc_lpass_q6_axi_clk",
	"gcc_lpass_sway_clk",
	"gcc_mdss_ahb_clk",
	"gcc_mdss_axi_clk",
	"gcc_mdss_byte0_clk",
	"gcc_mdss_esc0_clk",
	"gcc_mdss_mdp_clk",
	"gcc_mdss_pclk0_clk",
	"gcc_mdss_vsync_clk",
	"gcc_mpm_ahb_clk",
	"gcc_msg_ram_ahb_clk",
	"gcc_oxili_ahb_clk",
	"gcc_oxili_aon_clk",
	"gcc_oxili_gfx3d_clk",
	"gcc_pcnoc_mpu_cfg_ahb_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_prng_ahb_clk",
	"gcc_q6_mpu_cfg_ahb_clk",
	"gcc_rpm_cfg_xpu_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_sec_ctrl_acc_clk",
	"gcc_sec_ctrl_ahb_clk",
	"gcc_sec_ctrl_boot_rom_patch_clk",
	"gcc_sec_ctrl_clk",
	"gcc_sec_ctrl_sense_clk",
	"gcc_tcsr_ahb_clk",
	"gcc_tlmm_ahb_clk",
	"gcc_tlmm_clk",
	"gcc_usb2a_phy_sleep_clk",
	"gcc_usb_hs_ahb_clk",
	"gcc_usb_hs_inactivity_timers_clk",
	"gcc_usb_hs_phy_cfg_ahb_clk",
	"gcc_usb_hs_system_clk",
	"gcc_venus0_ahb_clk",
	"gcc_venus0_axi_clk",
	"gcc_venus0_core0_vcodec0_clk",
	"gcc_venus0_vcodec0_clk",
	"gcc_xo_clk",
	"gcc_xo_div4_clk",
	"gcc_gfx_tbu_clk",
	"gcc_gfx_tcu_clk",
	"gcc_gtcu_ahb_clk",
	"bimc_clk",
	"gcc_smmu_cfg_clk",
	"apss_cc_debug_mux",
	"gcc_mdss_pclk1_clk",
	"gcc_mdss_byte1_clk",
	"gcc_mdss_esc1_clk",
	"gcc_oxili_timer_clk",
	"gcc_blsp1_qup1_spi_apps_clk",
	"gcc_blsp1_qup1_i2c_apps_clk",
	"gcc_blsp2_qup4_spi_apps_clk",
	"gcc_blsp2_qup4_i2c_apps_clk",
};

static int gcc_debug_mux_sels[] = {
	0x148,		/* gcc_ahb_clk */
	0x168,		/* gcc_apss_ahb_clk */
	0x169,		/* gcc_apss_axi_clk */
	0x2D,		/* gcc_bimc_gfx_clk */
	0x157,		/* gcc_bimc_gpu_clk */
	0x88,		/* gcc_blsp1_ahb_clk */
	0x90,		/* gcc_blsp1_qup2_i2c_apps_clk */
	0x8E,		/* gcc_blsp1_qup2_spi_apps_clk */
	0x94,		/* gcc_blsp1_qup3_i2c_apps_clk */
	0x93,		/* gcc_blsp1_qup3_spi_apps_clk */
	0x96,		/* gcc_blsp1_qup4_i2c_apps_clk */
	0x95,		/* gcc_blsp1_qup4_spi_apps_clk */
	0x89,		/* gcc_blsp1_sleep_clk */
	0x8C,		/* gcc_blsp1_uart1_apps_clk */
	0x8D,		/* gcc_blsp1_uart1_sim_clk */
	0x91,		/* gcc_blsp1_uart2_apps_clk */
	0x92,		/* gcc_blsp1_uart2_sim_clk */
	0x98,		/* gcc_blsp2_ahb_clk */
	0x9B,		/* gcc_blsp2_qup1_i2c_apps_clk */
	0x9A,		/* gcc_blsp2_qup1_spi_apps_clk */
	0xA0,		/* gcc_blsp2_qup2_i2c_apps_clk */
	0x9E,		/* gcc_blsp2_qup2_spi_apps_clk */
	0xA4,		/* gcc_blsp2_qup3_i2c_apps_clk */
	0xA3,		/* gcc_blsp2_qup3_spi_apps_clk */
	0x99,		/* gcc_blsp2_sleep_clk */
	0x9C,		/* gcc_blsp2_uart1_apps_clk */
	0x9D,		/* gcc_blsp2_uart1_sim_clk */
	0x9A,		/* gcc_blsp2_uart2_apps_clk */
	0xA2,		/* gcc_blsp2_uart2_sim_clk */
	0xF8,		/* gcc_boot_rom_ahb_clk */
	0xA8,		/* gcc_camss_ahb_clk */
	0xB0,		/* gcc_camss_cci_ahb_clk */
	0xAF,		/* gcc_camss_cci_clk */
	0xBA,		/* gcc_camss_cpp_ahb_clk */
	0x1A3,		/* gcc_camss_cpp_axi_clk */
	0xB9,		/* gcc_camss_cpp_clk */
	0xC1,		/* gcc_camss_csi0_ahb_clk */
	0xC0,		/* gcc_camss_csi0_clk */
	0xC2,		/* gcc_camss_csi0phy_clk */
	0xB1,		/* gcc_camss_csi0phytimer_clk */
	0xC4,		/* gcc_camss_csi0pix_clk */
	0xC3,		/* gcc_camss_csi0rdi_clk */
	0xC6,		/* gcc_camss_csi1_ahb_clk */
	0xC5,		/* gcc_camss_csi1_clk */
	0xC7,		/* gcc_camss_csi1phy_clk */
	0xB2,		/* gcc_camss_csi1phytimer_clk */
	0xE1,		/* gcc_camss_csi1pix_clk */
	0xE0,		/* gcc_camss_csi1rdi_clk */
	0xE4,		/* gcc_camss_csi2_ahb_clk */
	0xE3,		/* gcc_camss_csi2_clk */
	0xE5,		/* gcc_camss_csi2phy_clk */
	0xE7,		/* gcc_camss_csi2pix_clk */
	0xE6,		/* gcc_camss_csi2rdi_clk */
	0xBF,		/* gcc_camss_csi_vfe0_clk */
	0x1A0,		/* gcc_camss_csi_vfe1_clk */
	0xAB,		/* gcc_camss_gp0_clk */
	0xAC,		/* gcc_camss_gp1_clk */
	0xE2,		/* gcc_camss_ispif_ahb_clk */
	0xB3,		/* gcc_camss_jpeg0_clk */
	0xB4,		/* gcc_camss_jpeg_ahb_clk */
	0xB5,		/* gcc_camss_jpeg_axi_clk */
	0xAD,		/* gcc_camss_mclk0_clk */
	0xAE,		/* gcc_camss_mclk1_clk */
	0x1BD,		/* gcc_camss_mclk2_clk */
	0xAA,		/* gcc_camss_micro_ahb_clk */
	0xA9,		/* gcc_camss_top_ahb_clk */
	0xB8,		/* gcc_camss_vfe0_clk */
	0x1A2,		/* gcc_camss_vfe1_ahb_clk */
	0x1A4,		/* gcc_camss_vfe1_axi_clk */
	0x1A1,		/* gcc_camss_vfe1_clk */
	0xBB,		/* gcc_camss_vfe_ahb_clk */
	0xBC,		/* gcc_camss_vfe_axi_clk */
	0x13A,		/* gcc_crypto_ahb_clk */
	0x139,		/* gcc_crypto_axi_clk */
	0x138,		/* gcc_crypto_clk */
	0x10,		/* gcc_gp1_clk */
	0x11,		/* gcc_gp2_clk */
	0x12,		/* gcc_gp3_clk */
	0x14B,		/* gcc_im_sleep_clk */
	0x162,		/* gcc_lpass_mport_axi_clk */
	0x160,		/* gcc_lpass_q6_axi_clk */
	0x163,		/* gcc_lpass_sway_clk */
	0x1F6,		/* gcc_mdss_ahb_clk */
	0x1F7,		/* gcc_mdss_axi_clk */
	0x1FC,		/* gcc_mdss_byte0_clk */
	0x1FD,		/* gcc_mdss_esc0_clk */
	0x1F9,		/* gcc_mdss_mdp_clk */
	0x1F8,		/* gcc_mdss_pclk0_clk */
	0x1FB,		/* gcc_mdss_vsync_clk */
	0x110,		/* gcc_mpm_ahb_clk */
	0x100,		/* gcc_msg_ram_ahb_clk */
	0x1EB,		/* gcc_oxili_ahb_clk */
	0xEE,		/* gcc_oxili_aon_clk */
	0x1EA,		/* gcc_oxili_gfx3d_clk */
	0xC9,		/* gcc_pcnoc_mpu_cfg_ahb_clk */
	0xD2,		/* gcc_pdm2_clk */
	0xD0,		/* gcc_pdm_ahb_clk */
	0xD1,		/* gcc_pdm_xo4_clk */
	0xD8,		/* gcc_prng_ahb_clk */
	0xC8,		/* gcc_q6_mpu_cfg_ahb_clk */
	0x38,		/* gcc_rpm_cfg_xpu_clk */
	0x69,		/* gcc_sdcc1_ahb_clk */
	0x68,		/* gcc_sdcc1_apps_clk */
	0x6A,		/* gcc_sdcc1_ice_core_clk */
	0x71,		/* gcc_sdcc2_ahb_clk */
	0x70,		/* gcc_sdcc2_apps_clk */
	0x120,		/* gcc_sec_ctrl_acc_clk */
	0x121,		/* gcc_sec_ctrl_ahb_clk */
	0x124,		/* gcc_sec_ctrl_boot_rom_patch_clk */
	0x122,		/* gcc_sec_ctrl_clk */
	0x123,		/* gcc_sec_ctrl_sense_clk */
	0xE8,		/* gcc_tcsr_ahb_clk */
	0x108,		/* gcc_tlmm_ahb_clk */
	0x109,		/* gcc_tlmm_clk */
	0x63,		/* gcc_usb2a_phy_sleep_clk */
	0x61,		/* gcc_usb_hs_ahb_clk */
	0x62,		/* gcc_usb_hs_inactivity_timers_clk */
	0x64,		/* gcc_usb_hs_phy_cfg_ahb_clk */
	0x60,		/* gcc_usb_hs_system_clk */
	0x1F3,		/* gcc_venus0_ahb_clk */
	0x1F2,		/* gcc_venus0_axi_clk */
	0x1B8,		/* gcc_venus0_core0_vcodec0_clk */
	0x1F1,		/* gcc_venus0_vcodec0_clk */
	0x149,		/* gcc_xo_clk */
	0x14A,		/* gcc_xo_div4_clk */
	0x52,		/* gcc_gfx_tbu_clk */
	0x53,		/* gcc_gfx_tcu_clk */
	0x58,		/* gcc_gtcu_ahb_clk */
	0x15A,		/* bimc_clk */
	0x5B,		/* gcc_smmu_cfg_clk */
	0x16A,		/* apss_cc_debug_mux */
	0x1e3,		/* gcc_mdss_pclk1_clk */
	0x1e4,		/* gcc_mdss_byte1_clk */
	0x1e5,		/* gcc_mdss_esc1_clk */
	0x1e9,		/* gcc_oxili_timer_clk */
	0x8a,		/* gcc_blsp1_qup1_spi_apps_clk */
	0x8b,		/* gcc_blsp1_qup1_i2c_apps_clk */
	0xa5,		/* gcc_blsp2_qup4_spi_apps_clk */
	0xa6,		/* gcc_blsp2_qup4_i2c_apps_clk */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.en_mask = BIT(16),
	.debug_offset = 0x74000,
	.post_div_offset = 0x74000,
	.cbcr_offset = 0x74000,
	.src_sel_mask = 0x1FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF000,
	.post_div_shift = 12,
	.post_div_val = 1,
	.mux_sels = gcc_debug_mux_sels,
	.hw.init = &(struct clk_init_data){
		.name = "gcc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gcc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gcc_debug_mux_parent_names),
		.flags = CLK_IS_MEASURE,
	},
};

static struct clk_dummy pwrcl_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "pwrcl_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy perfcl_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "perfcl_clk",
		.ops = &clk_dummy_ops,
	},
};

struct clk_hw *debugcc_sdm439_hws[] = {
	&pwrcl_clk.hw,
	&perfcl_clk.hw,
};

static struct mux_regmap_names mux_list[] = {
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
	{ .mux = &apss_cc_debug_mux, .regmap_name = "qcom,cpu" },
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,sdm439-debugcc" },
	{ }
};

static int clk_debug_sdm429w_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret, i;

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

	for (i = 0; i < ARRAY_SIZE(debugcc_sdm439_hws); i++) {
		clk = devm_clk_register(&pdev->dev,
			debugcc_sdm439_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
			debugcc_sdm439_hws[i]->init->name,
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
	.probe = clk_debug_sdm429w_probe,
	.driver = {
		.name = "sdm439-debugcc",
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

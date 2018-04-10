/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/clk/msm-clock-generic.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <dt-bindings/clock/msm-clocks-8996.h>
#include "virtclk-front.h"
#include "virt-reset-front.h"

static struct virtclk_front gcc_blsp1_ahb_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_ahb_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup1_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup1_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup1_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup1_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart1_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart1_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart1_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup2_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup2_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup2_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup2_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart2_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart2_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart2_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup3_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup3_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup3_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup3_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart3_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart3_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart3_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup4_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup4_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup4_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup4_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart4_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup5_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup5_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup5_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup5_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup5_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup5_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart5_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart5_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart5_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup6_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup6_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup6_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_qup6_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_qup6_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_qup6_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp1_uart6_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp1_uart6_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp1_uart6_apps_clk.c),
	},
};


static struct virtclk_front gcc_blsp2_ahb_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_ahb_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_ahb_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup1_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup1_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup1_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup1_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup1_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup1_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart1_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart1_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart1_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup2_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup2_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup2_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup2_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup2_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup2_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart2_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart2_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart2_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup3_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup3_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup3_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup3_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup3_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup3_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart3_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart3_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart3_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup4_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup4_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup4_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup4_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup4_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup4_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart4_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart4_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart4_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup5_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup5_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup5_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup5_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup5_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup5_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart5_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart5_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart5_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup6_spi_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup6_spi_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup6_spi_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_qup6_i2c_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_qup6_i2c_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_qup6_i2c_apps_clk.c),
	},
};

static struct virtclk_front gcc_blsp2_uart6_apps_clk = {
	.c = {
		.dbg_name = "gcc_blsp2_uart6_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_blsp2_uart6_apps_clk.c),
	},
};

static struct virtclk_front gcc_sdcc2_ahb_clk = {
	.c = {
		.dbg_name = "gcc_sdcc2_ahb_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_sdcc2_ahb_clk.c),
	},
};

static struct virtclk_front gcc_sdcc2_apps_clk = {
	.c = {
		.dbg_name = "gcc_sdcc2_apps_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_sdcc2_apps_clk.c),
	},
};

static struct virtclk_front gcc_usb3_phy_pipe_clk = {
	.c = {
		.dbg_name = "gcc_usb3_phy_pipe_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb3_phy_pipe_clk.c),
	},
};

static struct virtclk_front gcc_usb3_phy_aux_clk = {
	.c = {
		.dbg_name = "gcc_usb3_phy_aux_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb3_phy_aux_clk.c),
	},
};

static struct virtclk_front gcc_usb30_mock_utmi_clk = {
	.c = {
		.dbg_name = "gcc_usb30_mock_utmi_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb30_mock_utmi_clk.c),
	},
};

static struct virtclk_front gcc_aggre2_usb3_axi_clk = {
	.c = {
		.dbg_name = "gcc_aggre2_usb3_axi_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_aggre2_usb3_axi_clk.c),
	},
};

static struct virtclk_front gcc_sys_noc_usb3_axi_clk = {
	.c = {
		.dbg_name = "gcc_sys_noc_usb3_axi_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_sys_noc_usb3_axi_clk.c),
	},
};

static struct virtclk_front gcc_usb30_master_clk = {
	.c = {
		.dbg_name = "gcc_usb30_master_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb30_master_clk.c),
	},
};

static struct virtclk_front gcc_usb30_sleep_clk = {
	.c = {
		.dbg_name = "gcc_usb30_sleep_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb30_sleep_clk.c),
	},
};

static struct virtclk_front gcc_usb_phy_cfg_ahb2phy_clk = {
	.c = {
		.dbg_name = "gcc_usb_phy_cfg_ahb2phy_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb_phy_cfg_ahb2phy_clk.c),
	},
};

static struct virtclk_front gcc_usb3_clkref_clk = {
	.c = {
		.dbg_name = "gcc_usb3_clkref_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb3_clkref_clk.c),
	},
};

static struct virtclk_front gcc_usb20_master_clk = {
	.c = {
		.dbg_name = "gcc_usb20_master_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb20_master_clk.c),
	},
};

static struct virtclk_front gcc_periph_noc_usb20_ahb_clk = {
	.c = {
		.dbg_name = "gcc_periph_noc_usb20_ahb_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_periph_noc_usb20_ahb_clk.c),
	},
};

static struct virtclk_front gcc_usb20_mock_utmi_clk = {
	.c = {
		.dbg_name = "gcc_usb20_mock_utmi_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb20_mock_utmi_clk.c),
	},
};

static struct virtclk_front gcc_usb20_sleep_clk = {
	.c = {
		.dbg_name = "gcc_usb20_sleep_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_usb20_sleep_clk.c),
	},
};

static struct virtclk_front hlos1_vote_lpass_adsp_smmu_clk = {
	.c = {
		.dbg_name = "gcc_lpass_adsp_smmu_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(hlos1_vote_lpass_adsp_smmu_clk.c),
	},
};

static struct virtclk_front gcc_mss_cfg_ahb_clk = {
	.c = {
		.dbg_name = "gcc_mss_cfg_ahb_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_mss_cfg_ahb_clk.c),
	},
};

static struct virtclk_front gcc_mss_q6_bimc_axi_clk = {
	.c = {
		.dbg_name = "gcc_mss_q6_bimc_axi_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_mss_q6_bimc_axi_clk.c),
	},
};

static struct virtclk_front gcc_boot_rom_ahb_clk = {
	.c = {
		.dbg_name = "gcc_boot_rom_ahb_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_boot_rom_ahb_clk.c),
	},
};

static struct virtclk_front gpll0_out_msscc = {
	.c = {
		.dbg_name = "gcc_mss_gpll0_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gpll0_out_msscc.c),
	},
};

static struct virtclk_front gcc_mss_snoc_axi_clk = {
	.c = {
		.dbg_name = "gcc_mss_snoc_axi_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_mss_snoc_axi_clk.c),
	},
};

static struct virtclk_front gcc_mss_mnoc_bimc_axi_clk = {
	.c = {
		.dbg_name = "gcc_mss_mnoc_bimc_axi_clk",
		.ops = &virtclk_front_ops,
		CLK_INIT(gcc_mss_mnoc_bimc_axi_clk.c),
	},
};

static struct virtclk_front ipa_clk = {
	.c = {
		.dbg_name = "ipa",
		.ops = &virtclk_front_ops,
		CLK_INIT(ipa_clk.c),
	},
	.flag = CLOCK_FLAG_NODE_TYPE_REMOTE,
};

static struct virtclk_front pnoc_clk = {
	.c = {
		.dbg_name = "pnoc",
		.ops = &virtclk_front_ops,
		CLK_INIT(pnoc_clk.c),
	},
	.flag = CLOCK_FLAG_NODE_TYPE_REMOTE,
};

static struct virtclk_front qdss_clk = {
	.c = {
		.dbg_name = "qdss",
		.ops = &virtclk_front_ops,
		CLK_INIT(qdss_clk.c),
	},
	.flag = CLOCK_FLAG_NODE_TYPE_REMOTE,
};

static struct clk_lookup msm_clocks_8996[] = {
	CLK_LIST(gcc_blsp1_ahb_clk),
	CLK_LIST(gcc_blsp1_qup1_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart1_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart2_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup3_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart3_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart4_apps_clk),
	CLK_LIST(gcc_blsp1_qup5_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup5_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart5_apps_clk),
	CLK_LIST(gcc_blsp1_qup6_spi_apps_clk),
	CLK_LIST(gcc_blsp1_qup6_i2c_apps_clk),
	CLK_LIST(gcc_blsp1_uart6_apps_clk),
	CLK_LIST(gcc_blsp2_ahb_clk),
	CLK_LIST(gcc_blsp2_qup1_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup1_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart1_apps_clk),
	CLK_LIST(gcc_blsp2_qup2_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup2_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart2_apps_clk),
	CLK_LIST(gcc_blsp2_qup3_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup3_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart3_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup4_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart4_apps_clk),
	CLK_LIST(gcc_blsp2_qup5_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup5_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart5_apps_clk),
	CLK_LIST(gcc_blsp2_qup6_spi_apps_clk),
	CLK_LIST(gcc_blsp2_qup6_i2c_apps_clk),
	CLK_LIST(gcc_blsp2_uart6_apps_clk),
	CLK_LIST(gcc_sdcc2_ahb_clk),
	CLK_LIST(gcc_sdcc2_apps_clk),
	CLK_LIST(gcc_usb3_phy_pipe_clk),
	CLK_LIST(gcc_usb3_phy_aux_clk),
	CLK_LIST(gcc_usb30_mock_utmi_clk),
	CLK_LIST(gcc_aggre2_usb3_axi_clk),
	CLK_LIST(gcc_sys_noc_usb3_axi_clk),
	CLK_LIST(gcc_usb30_master_clk),
	CLK_LIST(gcc_usb30_sleep_clk),
	CLK_LIST(gcc_usb_phy_cfg_ahb2phy_clk),
	CLK_LIST(gcc_usb3_clkref_clk),
	CLK_LIST(gcc_usb20_master_clk),
	CLK_LIST(gcc_periph_noc_usb20_ahb_clk),
	CLK_LIST(gcc_usb20_mock_utmi_clk),
	CLK_LIST(gcc_usb20_sleep_clk),
	CLK_LIST(hlos1_vote_lpass_adsp_smmu_clk),
	CLK_LIST(gcc_mss_cfg_ahb_clk),
	CLK_LIST(gcc_mss_q6_bimc_axi_clk),
	CLK_LIST(gcc_boot_rom_ahb_clk),
	CLK_LIST(gpll0_out_msscc),
	CLK_LIST(gcc_mss_snoc_axi_clk),
	CLK_LIST(gcc_mss_mnoc_bimc_axi_clk),
	CLK_LIST(ipa_clk),
	CLK_LIST(pnoc_clk),
	CLK_LIST(qdss_clk),
};

static struct virt_reset_map msm_resets_8996[] = {
	[QUSB2PHY_PRIM_BCR] = { "gcc_qusb2phy_prim_clk" },
	[QUSB2PHY_SEC_BCR] = { "gcc_qusb2phy_sec_clk" },
	[USB_20_BCR] = { "gcc_usb20_master_clk" },
	[USB_30_BCR] = { "gcc_usb3_phy_pipe_clk" },
	[USB3_PHY_BCR] = { "gcc_usb3_phy_clk" },
	[USB3PHY_PHY_BCR] = { "gcc_usb3phy_phy_clk" },
};

static const struct of_device_id msm8996_virtclk_front_match_table[] = {
	{ .compatible = "qcom,virtclk-frontend-8996" },
	{}
};

static int msm8996_virtclk_front_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = msm_virtclk_front_probe(pdev, msm_clocks_8996,
			ARRAY_SIZE(msm_clocks_8996));
	if (ret)
		return ret;

	ret = msm_virtrc_front_register(pdev, msm_resets_8996,
			ARRAY_SIZE(msm_resets_8996));

	return ret;
}

static struct platform_driver msm8996_virtclk_front_driver = {
	.probe = msm8996_virtclk_front_probe,
	.driver = {
		.name = "virtclk-front-8996",
		.of_match_table = msm8996_virtclk_front_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm8996_virtclk_front_init(void)
{
	return platform_driver_register(&msm8996_virtclk_front_driver);
}
arch_initcall(msm8996_virtclk_front_init);

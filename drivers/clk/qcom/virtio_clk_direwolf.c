// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <dt-bindings/clock/qcom,gcc-direwolf.h>
#include "virtio_clk_common.h"

static const char * const direwolf_gcc_virtio_clocks[] = {
	[GCC_QUPV3_WRAP0_S0_CLK] = "gcc_qupv3_wrap0_s0_clk",
	[GCC_QUPV3_WRAP0_S1_CLK] = "gcc_qupv3_wrap0_s1_clk",
	[GCC_QUPV3_WRAP0_S2_CLK] = "gcc_qupv3_wrap0_s2_clk",
	[GCC_QUPV3_WRAP0_S3_CLK] = "gcc_qupv3_wrap0_s3_clk",
	[GCC_QUPV3_WRAP0_S4_CLK] = "gcc_qupv3_wrap0_s4_clk",
	[GCC_QUPV3_WRAP0_S5_CLK] = "gcc_qupv3_wrap0_s5_clk",
	[GCC_QUPV3_WRAP0_S6_CLK] = "gcc_qupv3_wrap0_s6_clk",
	[GCC_QUPV3_WRAP0_S7_CLK] = "gcc_qupv3_wrap0_s7_clk",
	[GCC_QUPV3_WRAP1_S0_CLK] = "gcc_qupv3_wrap1_s0_clk",
	[GCC_QUPV3_WRAP1_S1_CLK] = "gcc_qupv3_wrap1_s1_clk",
	[GCC_QUPV3_WRAP1_S2_CLK] = "gcc_qupv3_wrap1_s2_clk",
	[GCC_QUPV3_WRAP1_S3_CLK] = "gcc_qupv3_wrap1_s3_clk",
	[GCC_QUPV3_WRAP1_S4_CLK] = "gcc_qupv3_wrap1_s4_clk",
	[GCC_QUPV3_WRAP1_S5_CLK] = "gcc_qupv3_wrap1_s5_clk",
	[GCC_QUPV3_WRAP1_S6_CLK] = "gcc_qupv3_wrap1_s6_clk",
	[GCC_QUPV3_WRAP1_S7_CLK] = "gcc_qupv3_wrap1_s7_clk",
	[GCC_QUPV3_WRAP2_S0_CLK] = "gcc_qupv3_wrap2_s0_clk",
	[GCC_QUPV3_WRAP2_S1_CLK] = "gcc_qupv3_wrap2_s1_clk",
	[GCC_QUPV3_WRAP2_S2_CLK] = "gcc_qupv3_wrap2_s2_clk",
	[GCC_QUPV3_WRAP2_S3_CLK] = "gcc_qupv3_wrap2_s3_clk",
	[GCC_QUPV3_WRAP2_S4_CLK] = "gcc_qupv3_wrap2_s4_clk",
	[GCC_QUPV3_WRAP2_S5_CLK] = "gcc_qupv3_wrap2_s5_clk",
	[GCC_QUPV3_WRAP2_S6_CLK] = "gcc_qupv3_wrap2_s6_clk",
	[GCC_QUPV3_WRAP2_S7_CLK] = "gcc_qupv3_wrap2_s7_clk",
	[GCC_QUPV3_WRAP_0_M_AHB_CLK] = "gcc_qupv3_wrap_0_m_ahb_clk",
	[GCC_QUPV3_WRAP_0_S_AHB_CLK] = "gcc_qupv3_wrap_0_s_ahb_clk",
	[GCC_QUPV3_WRAP_1_M_AHB_CLK] = "gcc_qupv3_wrap_1_m_ahb_clk",
	[GCC_QUPV3_WRAP_1_S_AHB_CLK] = "gcc_qupv3_wrap_1_s_ahb_clk",
	[GCC_QUPV3_WRAP_2_M_AHB_CLK] = "gcc_qupv3_wrap_2_m_ahb_clk",
	[GCC_QUPV3_WRAP_2_S_AHB_CLK] = "gcc_qupv3_wrap_2_s_ahb_clk",
	[GCC_USB30_PRIM_MASTER_CLK] = "gcc_usb30_prim_master_clk",
	[GCC_CFG_NOC_USB3_PRIM_AXI_CLK] = "gcc_cfg_noc_usb3_prim_axi_clk",
	[GCC_AGGRE_USB3_PRIM_AXI_CLK] = "gcc_aggre_usb3_prim_axi_clk",
	[GCC_USB30_PRIM_MOCK_UTMI_CLK] = "gcc_usb30_prim_mock_utmi_clk",
	[GCC_USB30_PRIM_SLEEP_CLK] = "gcc_usb30_prim_sleep_clk",
	[GCC_USB3_PRIM_PHY_AUX_CLK] = "gcc_usb3_prim_phy_aux_clk",
	[GCC_USB3_PRIM_PHY_PIPE_CLK] = "gcc_usb3_prim_phy_pipe_clk",
	[GCC_USB3_PRIM_PHY_COM_AUX_CLK] = "gcc_usb3_prim_phy_com_aux_clk",
	[GCC_USB30_SEC_MASTER_CLK] = "gcc_usb30_sec_master_clk",
	[GCC_CFG_NOC_USB3_SEC_AXI_CLK] = "gcc_cfg_noc_usb3_sec_axi_clk",
	[GCC_AGGRE_USB3_SEC_AXI_CLK] = "gcc_aggre_usb3_sec_axi_clk",
	[GCC_USB30_SEC_MOCK_UTMI_CLK] = "gcc_usb30_sec_mock_utmi_clk",
	[GCC_USB30_SEC_SLEEP_CLK] = "gcc_usb30_sec_sleep_clk",
	[GCC_USB3_SEC_PHY_AUX_CLK] = "gcc_usb3_sec_phy_aux_clk",
	[GCC_USB3_SEC_PHY_PIPE_CLK] = "gcc_usb3_sec_phy_pipe_clk",
	[GCC_USB3_SEC_PHY_COM_AUX_CLK] = "gcc_usb3_sec_phy_com_aux_clk",
	[GCC_SDCC2_AHB_CLK] = "gcc_sdcc2_ahb_clk",
	[GCC_SDCC2_APPS_CLK] = "gcc_sdcc2_apps_clk",
};

static const char * const direwolf_gcc_virtio_resets[] = {
	[GCC_QUSB2PHY_PRIM_BCR] = "gcc_qusb2phy_prim_bcr",
	[GCC_QUSB2PHY_SEC_BCR] = "gcc_qusb2phy_sec_bcr",
	[GCC_USB30_PRIM_BCR] = "gcc_usb30_prim_master_clk",
	[GCC_USB30_SEC_BCR] = "gcc_usb30_sec_master_clk",
};

const struct clk_virtio_desc clk_virtio_direwolf_gcc = {
	.clk_names = direwolf_gcc_virtio_clocks,
	.num_clks = ARRAY_SIZE(direwolf_gcc_virtio_clocks),
	.reset_names = direwolf_gcc_virtio_resets,
	.num_resets = ARRAY_SIZE(direwolf_gcc_virtio_resets),
};
EXPORT_SYMBOL(clk_virtio_direwolf_gcc);

MODULE_LICENSE("GPL v2");

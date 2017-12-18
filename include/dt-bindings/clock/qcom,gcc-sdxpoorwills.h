/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_MSM_GCC_SDX24_H
#define _DT_BINDINGS_CLK_MSM_GCC_SDX24_H

/* GCC clock registers */
#define GCC_BLSP1_AHB_CLK					0
#define GCC_BLSP1_QUP1_I2C_APPS_CLK				1
#define GCC_BLSP1_QUP1_I2C_APPS_CLK_SRC				2
#define GCC_BLSP1_QUP1_SPI_APPS_CLK				3
#define GCC_BLSP1_QUP1_SPI_APPS_CLK_SRC				4
#define GCC_BLSP1_QUP2_I2C_APPS_CLK				5
#define GCC_BLSP1_QUP2_I2C_APPS_CLK_SRC				6
#define GCC_BLSP1_QUP2_SPI_APPS_CLK				7
#define GCC_BLSP1_QUP2_SPI_APPS_CLK_SRC				8
#define GCC_BLSP1_QUP3_I2C_APPS_CLK				9
#define GCC_BLSP1_QUP3_I2C_APPS_CLK_SRC				10
#define GCC_BLSP1_QUP3_SPI_APPS_CLK				11
#define GCC_BLSP1_QUP3_SPI_APPS_CLK_SRC				12
#define GCC_BLSP1_QUP4_I2C_APPS_CLK				13
#define GCC_BLSP1_QUP4_I2C_APPS_CLK_SRC				14
#define GCC_BLSP1_QUP4_SPI_APPS_CLK				15
#define GCC_BLSP1_QUP4_SPI_APPS_CLK_SRC				16
#define GCC_BLSP1_SLEEP_CLK					17
#define GCC_BLSP1_UART1_APPS_CLK				18
#define GCC_BLSP1_UART1_APPS_CLK_SRC				19
#define GCC_BLSP1_UART2_APPS_CLK				20
#define GCC_BLSP1_UART2_APPS_CLK_SRC				21
#define GCC_BLSP1_UART3_APPS_CLK				22
#define GCC_BLSP1_UART3_APPS_CLK_SRC				23
#define GCC_BLSP1_UART4_APPS_CLK				24
#define GCC_BLSP1_UART4_APPS_CLK_SRC				25
#define GCC_BOOT_ROM_AHB_CLK					26
#define GCC_CE1_AHB_CLK						27
#define GCC_CE1_AXI_CLK						28
#define GCC_CE1_CLK						29
#define GCC_CPUSS_AHB_CLK					30
#define GCC_CPUSS_AHB_CLK_SRC					31
#define GCC_CPUSS_GNOC_CLK					32
#define GCC_CPUSS_RBCPR_CLK					33
#define GCC_CPUSS_RBCPR_CLK_SRC					34
#define GCC_EMAC_CLK_SRC					35
#define GCC_EMAC_PTP_CLK_SRC					36
#define GCC_ETH_AXI_CLK						37
#define GCC_ETH_PTP_CLK						38
#define GCC_ETH_RGMII_CLK					39
#define GCC_ETH_SLAVE_AHB_CLK					40
#define GCC_GP1_CLK						41
#define GCC_GP1_CLK_SRC						42
#define GCC_GP2_CLK						43
#define GCC_GP2_CLK_SRC						44
#define GCC_GP3_CLK						45
#define GCC_GP3_CLK_SRC						46
#define GCC_MSS_CFG_AHB_CLK					47
#define GCC_MSS_GPLL0_DIV_CLK_SRC				48
#define GCC_MSS_SNOC_AXI_CLK					49
#define GCC_PCIE_0_CLKREF_CLK					50
#define GCC_PCIE_AUX_CLK					51
#define GCC_PCIE_AUX_PHY_CLK_SRC				52
#define GCC_PCIE_CFG_AHB_CLK					53
#define GCC_PCIE_MSTR_AXI_CLK					54
#define GCC_PCIE_PHY_REFGEN_CLK					55
#define GCC_PCIE_PHY_REFGEN_CLK_SRC				56
#define GCC_PCIE_PIPE_CLK					57
#define GCC_PCIE_SLEEP_CLK					58
#define GCC_PCIE_SLV_AXI_CLK					59
#define GCC_PCIE_SLV_Q2A_AXI_CLK				60
#define GCC_PDM2_CLK						61
#define GCC_PDM2_CLK_SRC					62
#define GCC_PDM_AHB_CLK						63
#define GCC_PDM_XO4_CLK						64
#define GCC_PRNG_AHB_CLK					65
#define GCC_SDCC1_AHB_CLK					66
#define GCC_SDCC1_APPS_CLK					67
#define GCC_SDCC1_APPS_CLK_SRC					68
#define GCC_SPMI_FETCHER_AHB_CLK				69
#define GCC_SPMI_FETCHER_CLK					70
#define GCC_SPMI_FETCHER_CLK_SRC				71
#define GCC_SYS_NOC_CPUSS_AHB_CLK				72
#define GCC_SYS_NOC_USB3_CLK					73
#define GCC_USB30_MASTER_CLK					74
#define GCC_USB30_MASTER_CLK_SRC				75
#define GCC_USB30_MOCK_UTMI_CLK					76
#define GCC_USB30_MOCK_UTMI_CLK_SRC				77
#define GCC_USB30_SLEEP_CLK					78
#define GCC_USB3_PHY_AUX_CLK					79
#define GCC_USB3_PHY_AUX_CLK_SRC				80
#define GCC_USB3_PHY_PIPE_CLK					81
#define GCC_USB3_PRIM_CLKREF_CLK				82
#define GCC_USB_PHY_CFG_AHB2PHY_CLK				83
#define GPLL0							84
#define GPLL0_OUT_EVEN						85
#define GPLL4							86
#define GPLL4_OUT_EVEN						87

/* CPU clocks */
#define CLOCK_A7SS						0

/* GCC reset clocks */
#define GCC_BLSP1_QUP1_BCR					0
#define GCC_BLSP1_QUP2_BCR					1
#define GCC_BLSP1_QUP3_BCR					2
#define GCC_BLSP1_QUP4_BCR					3
#define GCC_BLSP1_UART2_BCR					4
#define GCC_BLSP1_UART3_BCR					5
#define GCC_BLSP1_UART4_BCR					6
#define GCC_CE1_BCR						7
#define GCC_PCIE_BCR						8
#define GCC_PCIE_PHY_BCR					9
#define GCC_PDM_BCR						10
#define GCC_PRNG_BCR						11
#define GCC_SDCC1_BCR						12
#define GCC_SPMI_FETCHER_BCR					13
#define GCC_USB30_BCR						14
#define GCC_USB3_PHY_BCR					15
#define GCC_USB3PHY_PHY_BCR					16
#define GCC_QUSB2PHY_BCR					17
#define GCC_USB_PHY_CFG_AHB2PHY_BCR				18
#define GCC_EMAC_BCR						19

#endif

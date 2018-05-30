/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_QCOM_GCC_QCS405_H
#define _DT_BINDINGS_CLK_QCOM_GCC_QCS405_H

#define APSS_AHB_CLK_SRC				0
#define BLSP1_QUP0_I2C_APPS_CLK_SRC			1
#define BLSP1_QUP0_SPI_APPS_CLK_SRC			2
#define BLSP1_QUP1_I2C_APPS_CLK_SRC			3
#define BLSP1_QUP1_SPI_APPS_CLK_SRC			4
#define BLSP1_QUP2_I2C_APPS_CLK_SRC			5
#define BLSP1_QUP2_SPI_APPS_CLK_SRC			6
#define BLSP1_QUP3_I2C_APPS_CLK_SRC			7
#define BLSP1_QUP3_SPI_APPS_CLK_SRC			8
#define BLSP1_QUP4_I2C_APPS_CLK_SRC			9
#define BLSP1_QUP4_SPI_APPS_CLK_SRC			10
#define BLSP1_UART0_APPS_CLK_SRC			11
#define BLSP1_UART1_APPS_CLK_SRC			12
#define BLSP1_UART2_APPS_CLK_SRC			13
#define BLSP1_UART3_APPS_CLK_SRC			14
#define BLSP2_QUP0_I2C_APPS_CLK_SRC			15
#define BLSP2_QUP0_SPI_APPS_CLK_SRC			16
#define BLSP2_UART0_APPS_CLK_SRC			17
#define BYTE0_CLK_SRC					18
#define EMAC_CLK_SRC					19
#define EMAC_PTP_CLK_SRC				20
#define ESC0_CLK_SRC					21
#define GCC_APSS_AHB_CLK				22
#define GCC_APSS_AXI_CLK				23
#define GCC_BIMC_APSS_AXI_CLK				24
#define GCC_BIMC_GFX_CLK				25
#define GCC_BIMC_MDSS_CLK				26
#define GCC_BLSP1_AHB_CLK				27
#define GCC_BLSP1_QUP0_I2C_APPS_CLK			28
#define GCC_BLSP1_QUP0_SPI_APPS_CLK			29
#define GCC_BLSP1_QUP1_I2C_APPS_CLK			30
#define GCC_BLSP1_QUP1_SPI_APPS_CLK			31
#define GCC_BLSP1_QUP2_I2C_APPS_CLK			32
#define GCC_BLSP1_QUP2_SPI_APPS_CLK			33
#define GCC_BLSP1_QUP3_I2C_APPS_CLK			34
#define GCC_BLSP1_QUP3_SPI_APPS_CLK			35
#define GCC_BLSP1_QUP4_I2C_APPS_CLK			36
#define GCC_BLSP1_QUP4_SPI_APPS_CLK			37
#define GCC_BLSP1_UART0_APPS_CLK			38
#define GCC_BLSP1_UART1_APPS_CLK			39
#define GCC_BLSP1_UART2_APPS_CLK			40
#define GCC_BLSP1_UART3_APPS_CLK			41
#define GCC_BLSP2_AHB_CLK				42
#define GCC_BLSP2_QUP0_I2C_APPS_CLK			43
#define GCC_BLSP2_QUP0_SPI_APPS_CLK			44
#define GCC_BLSP2_UART0_APPS_CLK			45
#define GCC_BOOT_ROM_AHB_CLK				46
#define GCC_DCC_CLK					47
#define GCC_GENI_IR_H_CLK				48
#define GCC_ETH_AXI_CLK					49
#define GCC_ETH_PTP_CLK					50
#define GCC_ETH_RGMII_CLK				51
#define GCC_ETH_SLAVE_AHB_CLK				52
#define GCC_GENI_IR_S_CLK				53
#define GCC_GP1_CLK					54
#define GCC_GP2_CLK					55
#define GCC_GP3_CLK					56
#define GCC_MDSS_AHB_CLK				57
#define GCC_MDSS_AXI_CLK				58
#define GCC_MDSS_BYTE0_CLK				59
#define GCC_MDSS_ESC0_CLK				60
#define GCC_MDSS_HDMI_APP_CLK				61
#define GCC_MDSS_HDMI_PCLK_CLK				62
#define GCC_MDSS_MDP_CLK				63
#define GCC_MDSS_PCLK0_CLK				64
#define GCC_MDSS_VSYNC_CLK				65
#define GCC_OXILI_AHB_CLK				66
#define GCC_OXILI_GFX3D_CLK				67
#define GCC_PCIE_0_AUX_CLK				68
#define GCC_PCIE_0_CFG_AHB_CLK				69
#define GCC_PCIE_0_MSTR_AXI_CLK				70
#define GCC_PCIE_0_PIPE_CLK				71
#define GCC_PCIE_0_SLV_AXI_CLK				72
#define GCC_PCNOC_USB2_CLK				73
#define GCC_PCNOC_USB3_CLK				74
#define GCC_PDM2_CLK					75
#define GCC_PDM_AHB_CLK					76
#define VSYNC_CLK_SRC					77
#define GCC_PRNG_AHB_CLK				78
#define GCC_PWM0_XO512_CLK				79
#define GCC_PWM1_XO512_CLK				80
#define GCC_PWM2_XO512_CLK				81
#define GCC_SDCC1_AHB_CLK				82
#define GCC_SDCC1_APPS_CLK				83
#define GCC_SDCC1_ICE_CORE_CLK				84
#define GCC_SDCC2_AHB_CLK				85
#define GCC_SDCC2_APPS_CLK				86
#define GCC_SYS_NOC_USB3_CLK				87
#define GCC_USB20_MOCK_UTMI_CLK				88
#define GCC_USB2A_PHY_SLEEP_CLK				89
#define GCC_USB30_MASTER_CLK				90
#define GCC_USB30_MOCK_UTMI_CLK				91
#define GCC_USB30_SLEEP_CLK				92
#define GCC_USB3_PHY_AUX_CLK				93
#define GCC_USB3_PHY_PIPE_CLK				94
#define GCC_USB_HS_PHY_CFG_AHB_CLK			95
#define GCC_USB_HS_SYSTEM_CLK				96
#define GFX3D_CLK_SRC					97
#define GP1_CLK_SRC					98
#define GP2_CLK_SRC					99
#define GP3_CLK_SRC					100
#define GPLL0_OUT_MAIN					101
#define GPLL1_OUT_MAIN					102
#define GPLL3_OUT_MAIN					103
#define GPLL4_OUT_MAIN					104
#define HDMI_APP_CLK_SRC				105
#define HDMI_PCLK_CLK_SRC				106
#define MDP_CLK_SRC					107
#define PCIE_0_AUX_CLK_SRC				108
#define PCIE_0_PIPE_CLK_SRC				109
#define PCLK0_CLK_SRC					110
#define PDM2_CLK_SRC					111
#define SDCC1_APPS_CLK_SRC				112
#define SDCC1_ICE_CORE_CLK_SRC				113
#define SDCC2_APPS_CLK_SRC				114
#define USB20_MOCK_UTMI_CLK_SRC				115
#define USB30_MASTER_CLK_SRC				116
#define USB30_MOCK_UTMI_CLK_SRC				117
#define USB3_PHY_AUX_CLK_SRC				118
#define USB_HS_SYSTEM_CLK_SRC				119
#define GPLL0_AO_CLK_SRC				120
#define WCNSS_M_CLK					121
#define GCC_USB_HS_INACTIVITY_TIMERS_CLK		122
#define GPLL0_AO_OUT_MAIN				123
#define GPLL0_SLEEP_CLK_SRC				124
#define GPLL6						125
#define GPLL6_OUT_AUX					126
#define MDSS_MDP_VOTE_CLK				127
#define MDSS_ROTATOR_VOTE_CLK				128

#define GCC_GENI_IR_BCR					0
#define GCC_USB_HS_BCR					1
#define GCC_USB2_HS_PHY_ONLY_BCR			2
#define GCC_QUSB2_PHY_BCR				3
#define GCC_USB_HS_PHY_CFG_AHB_BCR			4
#define GCC_USB2A_PHY_BCR				5
#define GCC_USB3_PHY_BCR				6
#define GCC_USB_30_BCR					7
#define GCC_USB3PHY_PHY_BCR				8
#define GCC_PCIE_0_BCR					9
#define GCC_PCIE_0_PHY_BCR				10
#define GCC_PCIE_0_LINK_DOWN_BCR			11
#define GCC_PCIEPHY_0_PHY_BCR				12
#define GCC_EMAC_BCR					13

#endif

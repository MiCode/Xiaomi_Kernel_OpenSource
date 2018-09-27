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

#define GPLL0_OUT_MAIN					0
#define GPLL0_AO_CLK_SRC				1
#define GPLL1_OUT_MAIN					2
#define GPLL3_OUT_MAIN					3
#define GPLL4_OUT_MAIN					4
#define GPLL0_AO_OUT_MAIN				5
#define GPLL0_SLEEP_CLK_SRC				6
#define GPLL6						7
#define GPLL6_OUT_AUX					8
#define APSS_AHB_CLK_SRC				9
#define BLSP1_QUP0_I2C_APPS_CLK_SRC			10
#define BLSP1_QUP0_SPI_APPS_CLK_SRC			11
#define BLSP1_QUP1_I2C_APPS_CLK_SRC			12
#define BLSP1_QUP1_SPI_APPS_CLK_SRC			13
#define BLSP1_QUP2_I2C_APPS_CLK_SRC			14
#define BLSP1_QUP2_SPI_APPS_CLK_SRC			15
#define BLSP1_QUP3_I2C_APPS_CLK_SRC			16
#define BLSP1_QUP3_SPI_APPS_CLK_SRC			17
#define BLSP1_QUP4_I2C_APPS_CLK_SRC			18
#define BLSP1_QUP4_SPI_APPS_CLK_SRC			19
#define BLSP1_UART0_APPS_CLK_SRC			20
#define BLSP1_UART1_APPS_CLK_SRC			21
#define BLSP1_UART2_APPS_CLK_SRC			22
#define BLSP1_UART3_APPS_CLK_SRC			23
#define BLSP2_QUP0_I2C_APPS_CLK_SRC			24
#define BLSP2_QUP0_SPI_APPS_CLK_SRC			25
#define BLSP2_UART0_APPS_CLK_SRC			26
#define BYTE0_CLK_SRC					27
#define EMAC_CLK_SRC					28
#define EMAC_PTP_CLK_SRC				29
#define ESC0_CLK_SRC					30
#define GCC_APSS_AHB_CLK				31
#define GCC_APSS_AXI_CLK				32
#define GCC_BIMC_APSS_AXI_CLK				33
#define GCC_BIMC_GFX_CLK				34
#define GCC_BIMC_MDSS_CLK				35
#define GCC_BLSP1_AHB_CLK				36
#define GCC_BLSP1_QUP0_I2C_APPS_CLK			37
#define GCC_BLSP1_QUP0_SPI_APPS_CLK			38
#define GCC_BLSP1_QUP1_I2C_APPS_CLK			39
#define GCC_BLSP1_QUP1_SPI_APPS_CLK			40
#define GCC_BLSP1_QUP2_I2C_APPS_CLK			41
#define GCC_BLSP1_QUP2_SPI_APPS_CLK			42
#define GCC_BLSP1_QUP3_I2C_APPS_CLK			43
#define GCC_BLSP1_QUP3_SPI_APPS_CLK			44
#define GCC_BLSP1_QUP4_I2C_APPS_CLK			45
#define GCC_BLSP1_QUP4_SPI_APPS_CLK			46
#define GCC_BLSP1_UART0_APPS_CLK			47
#define GCC_BLSP1_UART1_APPS_CLK			48
#define GCC_BLSP1_UART2_APPS_CLK			49
#define GCC_BLSP1_UART3_APPS_CLK			50
#define GCC_BLSP2_AHB_CLK				51
#define GCC_BLSP2_QUP0_I2C_APPS_CLK			52
#define GCC_BLSP2_QUP0_SPI_APPS_CLK			53
#define GCC_BLSP2_UART0_APPS_CLK			54
#define GCC_BOOT_ROM_AHB_CLK				55
#define GCC_DCC_CLK					56
#define GCC_GENI_IR_H_CLK				57
#define GCC_ETH_AXI_CLK					58
#define GCC_ETH_PTP_CLK					59
#define GCC_ETH_RGMII_CLK				60
#define GCC_ETH_SLAVE_AHB_CLK				61
#define GCC_GENI_IR_S_CLK				62
#define GCC_GP1_CLK					63
#define GCC_GP2_CLK					64
#define GCC_GP3_CLK					65
#define GCC_MDSS_AHB_CLK				66
#define GCC_MDSS_AXI_CLK				67
#define GCC_MDSS_BYTE0_CLK				68
#define GCC_MDSS_ESC0_CLK				69
#define GCC_MDSS_HDMI_APP_CLK				70
#define GCC_MDSS_HDMI_PCLK_CLK				71
#define GCC_MDSS_MDP_CLK				72
#define GCC_MDSS_PCLK0_CLK				73
#define GCC_MDSS_VSYNC_CLK				74
#define GCC_OXILI_AHB_CLK				75
#define GFX3D_CLK_SRC					76
#define GCC_PCIE_0_AUX_CLK				77
#define GCC_PCIE_0_CFG_AHB_CLK				78
#define GCC_PCIE_0_MSTR_AXI_CLK				79
#define GCC_PCIE_0_PIPE_CLK				80
#define GCC_PCIE_0_SLV_AXI_CLK				81
#define GCC_PCNOC_USB2_CLK				82
#define GCC_PCNOC_USB3_CLK				83
#define GCC_PDM2_CLK					84
#define GCC_PDM_AHB_CLK					85
#define VSYNC_CLK_SRC					86
#define GCC_PRNG_AHB_CLK				87
#define GCC_PWM0_XO512_CLK				88
#define GCC_PWM1_XO512_CLK				89
#define GCC_PWM2_XO512_CLK				90
#define GCC_SDCC1_AHB_CLK				91
#define GCC_SDCC1_APPS_CLK				92
#define GCC_SDCC1_ICE_CORE_CLK				93
#define GCC_SDCC2_AHB_CLK				94
#define GCC_SDCC2_APPS_CLK				95
#define GCC_SYS_NOC_USB3_CLK				96
#define GCC_USB20_MOCK_UTMI_CLK				97
#define GCC_USB2A_PHY_SLEEP_CLK				98
#define GCC_USB30_MASTER_CLK				99
#define GCC_USB30_MOCK_UTMI_CLK				100
#define GCC_USB30_SLEEP_CLK				101
#define GCC_USB3_PHY_AUX_CLK				102
#define GCC_USB3_PHY_PIPE_CLK				103
#define GCC_USB_HS_PHY_CFG_AHB_CLK			104
#define GCC_USB_HS_SYSTEM_CLK				105
#define GCC_OXILI_GFX3D_CLK				106
#define GP1_CLK_SRC					107
#define GP2_CLK_SRC					108
#define GP3_CLK_SRC					109
#define HDMI_APP_CLK_SRC				110
#define HDMI_PCLK_CLK_SRC				111
#define MDP_CLK_SRC					112
#define PCIE_0_AUX_CLK_SRC				113
#define PCIE_0_PIPE_CLK_SRC				114
#define PCLK0_CLK_SRC					115
#define PDM2_CLK_SRC					116
#define SDCC1_APPS_CLK_SRC				117
#define SDCC1_ICE_CORE_CLK_SRC				118
#define SDCC2_APPS_CLK_SRC				119
#define USB20_MOCK_UTMI_CLK_SRC				120
#define USB30_MASTER_CLK_SRC				121
#define USB30_MOCK_UTMI_CLK_SRC				122
#define USB3_PHY_AUX_CLK_SRC				123
#define USB_HS_SYSTEM_CLK_SRC				124
#define WCNSS_M_CLK					125
#define GCC_USB_HS_INACTIVITY_TIMERS_CLK		126
#define MDSS_MDP_VOTE_CLK				127
#define MDSS_ROTATOR_VOTE_CLK				128
#define GCC_BIMC_GPU_CLK				129
#define GCC_GTCU_AHB_CLK				130
#define GCC_GFX_TCU_CLK					131
#define GCC_GFX_TBU_CLK					132
#define GCC_SMMU_CFG_CLK				133
#define GCC_APSS_TCU_CLK				134
#define GCC_CRYPTO_AHB_CLK				135
#define GCC_CRYPTO_AXI_CLK				136
#define GCC_CRYPTO_CLK					137
#define GCC_MDP_TBU_CLK					138
#define GCC_QDSS_DAP_CLK				139
#define GCC_DCC_XO_CLK					140

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

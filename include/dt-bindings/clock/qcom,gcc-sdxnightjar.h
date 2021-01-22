/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_GCC_SDXNIGHTJAR_H
#define _DT_BINDINGS_CLK_QCOM_GCC_SDXNIGHTJAR_H

/* GCC clocks */
#define GPLL0							0
#define GPLL0_AO						1
#define GPLL0_OUT_MAIN_DIV2					2
#define BLSP1_QUP1_I2C_APPS_CLK_SRC				3
#define BLSP1_QUP1_SPI_APPS_CLK_SRC				4
#define BLSP1_QUP2_I2C_APPS_CLK_SRC				5
#define BLSP1_QUP2_SPI_APPS_CLK_SRC				6
#define BLSP1_QUP3_I2C_APPS_CLK_SRC				7
#define BLSP1_QUP3_SPI_APPS_CLK_SRC				8
#define BLSP1_QUP4_I2C_APPS_CLK_SRC				9
#define BLSP1_QUP4_SPI_APPS_CLK_SRC				10
#define BLSP1_UART1_APPS_CLK_SRC				11
#define BLSP1_UART2_APPS_CLK_SRC				12
#define BLSP1_UART3_APPS_CLK_SRC				13
#define BLSP1_UART4_APPS_CLK_SRC				14
#define GCC_APSS_TCU_CLK					15
#define GCC_BLSP1_AHB_CLK					16
#define GCC_BLSP1_QUP1_I2C_APPS_CLK				17
#define GCC_BLSP1_QUP1_SPI_APPS_CLK				18
#define GCC_BLSP1_QUP2_I2C_APPS_CLK				19
#define GCC_BLSP1_QUP2_SPI_APPS_CLK				20
#define GCC_BLSP1_QUP3_I2C_APPS_CLK				21
#define GCC_BLSP1_QUP3_SPI_APPS_CLK				22
#define GCC_BLSP1_QUP4_I2C_APPS_CLK				23
#define GCC_BLSP1_QUP4_SPI_APPS_CLK				24
#define GCC_BLSP1_SLEEP_CLK					25
#define GCC_BLSP1_UART1_APPS_CLK				26
#define GCC_BLSP1_UART2_APPS_CLK				27
#define GCC_BLSP1_UART3_APPS_CLK				28
#define GCC_BLSP1_UART4_APPS_CLK				29
#define GCC_BOOT_ROM_AHB_CLK					30
#define GCC_DCC_CLK						31
#define GCC_GP1_CLK						32
#define GCC_GP2_CLK						33
#define GCC_GP3_CLK						34
#define GCC_MSS_GPLL0_CLK_SRC					35
#define GCC_PCIE_AXI_CLK					36
#define GCC_PCIE_AXI_MSTR_CLK					37
#define GCC_PCIE_AXI_TBU_CLK					38
#define GCC_PCIE_CFG_AHB_CLK					39
#define GCC_PCIE_REF_CLK					40
#define GCC_PCIE_PIPE_CLK					41
#define GCC_PCIE_SLEEP_CLK					42
#define GCC_PDM2_CLK						43
#define GCC_PDM_AHB_CLK						44
#define GCC_PDM_XO4_CLK						45
#define GCC_PRNG_AHB_CLK					46
#define GCC_QUSB_REF_CLK					47
#define GCC_SDCC1_AHB_CLK					48
#define GCC_SDCC1_APPS_CLK					49
#define GCC_SMMU_CFG_CLK					50
#define GCC_SYS_NOC_USB3_AXI_CLK				51
#define GCC_USB30_MASTER_CLK					52
#define GCC_USB30_MOCK_UTMI_CLK					53
#define GCC_USB30_SLEEP_CLK					54
#define GCC_USB3_AUX_CLK					55
#define GCC_USB3_AXI_TBU_CLK					56
#define GCC_USB3_PIPE_CLK					57
#define GCC_USB_PHY_CFG_AHB_CLK					58
#define GCC_USB_SS_REF_CLK					59
#define GCC_XO_DIV4_CLK						60
#define GP1_CLK_SRC						61
#define GP2_CLK_SRC						62
#define GP3_CLK_SRC						63
#define PCIE_AUX_CLK_SRC					64
#define PDM2_CLK_SRC						65
#define SDCC1_APPS_CLK_SRC					66
#define USB30_MASTER_CLK_SRC					67
#define USB30_MOCK_UTMI_CLK_SRC					68
#define USB3_AUX_CLK_SRC					69
#define APSS_AHB_CLK_SRC					70
#define GCC_MSS_Q6_BIMC_AXI_CLK					71
#define GCC_MSS_CFG_AHB_CLK					72

/* GCC resets */
#define GCC_PCIEPHY_PHY_BCR					0
#define GCC_PCIE_PHY_BCR					1
#define GCC_USB_30_BCR						2
#define GCC_USB3_PHY_BCR					3
#define GCC_USB3PHY_PHY_BCR					4
#define GCC_QUSB2A_PHY_BCR					5

#endif

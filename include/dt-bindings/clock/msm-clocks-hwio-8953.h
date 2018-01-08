/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_CLOCKS_8953_HWIO_H
#define __MSM_CLOCKS_8953_HWIO_H

#define GPLL0_MODE					0x21000
#define GPLL0_L_VAL					0x21004
#define GPLL0_ALPHA_VAL					0x21008
#define GPLL0_ALPHA_VAL_U				0x2100C
#define GPLL0_USER_CTL					0x21010
#define GPLL0_USER_CTL_U				0x21014
#define GPLL0_CONFIG_CTL				0x21018
#define GPLL0_TEST_CTL					0x2101C
#define GPLL0_TEST_CTL_U				0x21020
#define GPLL0_FREQ_CTL					0x21028
#define GPLL0_CLK_CGC_EN				0x2102C
#define GPLL0_SSC_CTL					0x21030
#define GPLL2_MODE					0x4A000
#define GPLL2_L_VAL					0x4A004
#define GPLL2_ALPHA_VAL					0x4A008
#define GPLL2_ALPHA_VAL_U				0x4A00C
#define GPLL2_USER_CTL					0x4A010
#define GPLL2_USER_CTL_U				0x4A014
#define GPLL2_CONFIG_CTL				0x4A018
#define GPLL2_TEST_CTL					0x4A01C
#define GPLL2_TEST_CTL_U				0x4A020
#define GPLL2_FREQ_CTL					0x4A028
#define GPLL2_CLK_CGC_EN				0x4A02C
#define GPLL2_SSC_CTL					0x4A030
#define GPLL3_MODE					0x22000
#define GPLL3_L_VAL					0x22004
#define GPLL3_ALPHA_VAL					0x22008
#define GPLL3_ALPHA_VAL_U				0x2200C
#define GPLL3_USER_CTL					0x22010
#define GPLL3_USER_CTL_U				0x22014
#define GPLL3_CONFIG_CTL				0x22018
#define GPLL3_TEST_CTL					0x2201C
#define GPLL3_TEST_CTL_U				0x22020
#define GPLL3_FREQ_CTL					0x22028
#define GPLL3_CLK_CGC_EN				0x2202C
#define GPLL3_SSC_CTL					0x22030
#define GPLL4_MODE					0x24000
#define GPLL4_L_VAL					0x24004
#define GPLL4_ALPHA_VAL					0x24008
#define GPLL4_ALPHA_VAL_U				0x2400C
#define GPLL4_USER_CTL					0x24010
#define GPLL4_USER_CTL_U				0x24014
#define GPLL4_CONFIG_CTL				0x24018
#define GPLL4_TEST_CTL					0x2401C
#define GPLL4_TEST_CTL_U				0x24020
#define GPLL4_FREQ_CTL					0x24028
#define GPLL4_CLK_CGC_EN				0x2402C
#define GPLL4_SSC_CTL					0x24030
#define GPLL5_MODE					0x25000
#define GPLL5_L_VAL					0x25004
#define GPLL5_ALPHA_VAL					0x25008
#define GPLL5_ALPHA_VAL_U				0x2500C
#define GPLL5_USER_CTL					0x25010
#define GPLL5_CONFIG_CTL				0x25018
#define GPLL5_TEST_CTL					0x2501C
#define GPLL5_CLK_CGC_EN				0x2502C
#define QDSS_DAP_CBCR					0x29084
#define GPLL6_MODE					0x37000
#define GPLL6_L_VAL					0x37004
#define GPLL6_ALPHA_VAL					0x37008
#define GPLL6_ALPHA_VAL_U				0x3700C
#define GPLL6_USER_CTL					0x37010
#define GPLL6_CONFIG_CTL				0x37018
#define GPLL6_TEST_CTL					0x3701C
#define GPLL6_STATUS					0x37024
#define GPLL6_CLK_CGC_EN				0x3702C
#define DCC_CBCR					0x77004
#define BIMC_GFX_CBCR					0x59034
#define OXILI_AON_CBCR					0x59044
#define MSS_CFG_AHB_CBCR				0x49000
#define MSS_Q6_BIMC_AXI_CBCR				0x49004
#define GCC_SLEEP_CMD_RCGR				0x30000
#define QUSB2_PHY_BCR					0x4103C
#define USB_30_BCR					0x3F070
#define PCNOC_USB3_AXI_CBCR				0x3F038
#define USB_30_MISC					0x3F074
#define USB30_MASTER_CBCR				0x3F000
#define USB30_SLEEP_CBCR				0x3F004
#define USB30_MOCK_UTMI_CBCR				0x3F008
#define USB30_MASTER_CMD_RCGR				0x3F00C
#define USB30_MASTER_CFG_RCGR				0x3F010
#define USB30_MASTER_M					0x3F014
#define USB30_MASTER_N					0x3F018
#define USB30_MASTER_D					0x3F01C
#define USB30_MOCK_UTMI_CMD_RCGR			0x3F020
#define USB30_MOCK_UTMI_CFG_RCGR			0x3F024
#define USB30_MOCK_UTMI_M				0x3F028
#define USB30_MOCK_UTMI_N				0x3F02C
#define USB30_MOCK_UTMI_D				0x3F030
#define USB_PHY_CFG_AHB_CBCR				0x3F080
#define USB3_PHY_BCR					0x3F034
#define USB3PHY_PHY_BCR					0x3F03C
#define USB3_PIPE_CBCR					0x3F040
#define USB3_PHY_PIPE_MISC				0x3F048
#define USB3_AUX_CBCR					0x3F044
#define USB3_AUX_CMD_RCGR				0x3F05C
#define USB3_AUX_CFG_RCGR				0x3F060
#define USB3_AUX_M					0x3F064
#define USB3_AUX_N					0x3F068
#define USB3_AUX_D					0x3F06C
#define SDCC1_APPS_CMD_RCGR				0x42004
#define SDCC1_APPS_CFG_RCGR				0x42008
#define SDCC1_APPS_M					0x4200C
#define SDCC1_APPS_N					0x42010
#define SDCC1_APPS_D					0x42014
#define SDCC1_APPS_CBCR					0x42018
#define SDCC1_AHB_CBCR					0x4201C
#define SDCC1_MISC					0x42020
#define SDCC2_APPS_CMD_RCGR				0x43004
#define SDCC2_APPS_CFG_RCGR				0x43008
#define SDCC2_APPS_M					0x4300C
#define SDCC2_APPS_N					0x43010
#define SDCC2_APPS_D					0x43014
#define SDCC2_APPS_CBCR					0x43018
#define SDCC2_AHB_CBCR					0x4301C
#define SDCC1_ICE_CORE_CMD_RCGR				0x5D000
#define SDCC1_ICE_CORE_CFG_RCGR				0x5D004
#define SDCC1_ICE_CORE_M				0x5D008
#define SDCC1_ICE_CORE_N				0x5D00C
#define SDCC1_ICE_CORE_D				0x5D010
#define SDCC1_ICE_CORE_CBCR				0x5D014
#define BLSP1_AHB_CBCR					0x01008
#define BLSP1_QUP1_SPI_APPS_CBCR			0x02004
#define BLSP1_QUP1_I2C_APPS_CBCR			0x02008
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR			0x0200C
#define BLSP1_QUP1_I2C_APPS_CFG_RCGR			0x02010
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR			0x03000
#define BLSP1_QUP2_I2C_APPS_CFG_RCGR			0x03004
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR			0x04000
#define BLSP1_QUP3_I2C_APPS_CFG_RCGR			0x04004
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR			0x05000
#define BLSP1_QUP4_I2C_APPS_CFG_RCGR			0x05004
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR			0x02024
#define BLSP1_QUP1_SPI_APPS_CFG_RCGR			0x02028
#define BLSP1_QUP1_SPI_APPS_M				0x0202C
#define BLSP1_QUP1_SPI_APPS_N				0x02030
#define BLSP1_QUP1_SPI_APPS_D				0x02034
#define BLSP1_UART1_APPS_CBCR				0x0203C
#define BLSP1_UART1_SIM_CBCR				0x02040
#define BLSP1_UART1_APPS_CMD_RCGR			0x02044
#define BLSP1_UART1_APPS_CFG_RCGR			0x02048
#define BLSP1_UART1_APPS_M				0x0204C
#define BLSP1_UART1_APPS_N				0x02050
#define BLSP1_UART1_APPS_D				0x02054
#define BLSP1_QUP2_SPI_APPS_CBCR			0x0300C
#define BLSP1_QUP2_I2C_APPS_CBCR			0x03010
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR			0x03014
#define BLSP1_QUP2_SPI_APPS_CFG_RCGR			0x03018
#define BLSP1_QUP2_SPI_APPS_M				0x0301C
#define BLSP1_QUP2_SPI_APPS_N				0x03020
#define BLSP1_QUP2_SPI_APPS_D				0x03024
#define BLSP1_UART2_APPS_CBCR				0x0302C
#define BLSP1_UART2_SIM_CBCR				0x03030
#define BLSP1_UART2_APPS_CMD_RCGR			0x03034
#define BLSP1_UART2_APPS_CFG_RCGR			0x03038
#define BLSP1_UART2_APPS_M				0x0303C
#define BLSP1_UART2_APPS_N				0x03040
#define BLSP1_UART2_APPS_D				0x03044
#define BLSP1_QUP3_SPI_APPS_CBCR			0x0401C
#define BLSP1_QUP3_I2C_APPS_CBCR			0x04020
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR			0x04024
#define BLSP1_QUP3_SPI_APPS_CFG_RCGR			0x04028
#define BLSP1_QUP3_SPI_APPS_M				0x0402C
#define BLSP1_QUP3_SPI_APPS_N				0x04030
#define BLSP1_QUP3_SPI_APPS_D				0x04034
#define BLSP1_QUP4_SPI_APPS_CBCR			0x0501C
#define BLSP1_QUP4_I2C_APPS_CBCR			0x05020
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR			0x05024
#define BLSP1_QUP4_SPI_APPS_CFG_RCGR			0x05028
#define BLSP1_QUP4_SPI_APPS_M				0x0502C
#define BLSP1_QUP4_SPI_APPS_N				0x05030
#define BLSP1_QUP4_SPI_APPS_D				0x05034
#define BLSP2_AHB_CBCR					0x0B008
#define BLSP2_QUP1_SPI_APPS_CBCR			0x0C004
#define BLSP2_QUP1_I2C_APPS_CBCR			0x0C008
#define BLSP2_QUP1_I2C_APPS_CMD_RCGR			0x0C00C
#define BLSP2_QUP1_I2C_APPS_CFG_RCGR			0x0C010
#define BLSP2_QUP2_I2C_APPS_CMD_RCGR			0x0D000
#define BLSP2_QUP2_I2C_APPS_CFG_RCGR			0x0D004
#define BLSP2_QUP3_I2C_APPS_CMD_RCGR			0x0F000
#define BLSP2_QUP3_I2C_APPS_CFG_RCGR			0x0F004
#define BLSP2_QUP4_I2C_APPS_CMD_RCGR			0x18000
#define BLSP2_QUP4_I2C_APPS_CFG_RCGR			0x18004
#define BLSP2_QUP1_SPI_APPS_CMD_RCGR			0x0C024
#define BLSP2_QUP1_SPI_APPS_CFG_RCGR			0x0C028
#define BLSP2_QUP1_SPI_APPS_M				0x0C02C
#define BLSP2_QUP1_SPI_APPS_N				0x0C030
#define BLSP2_QUP1_SPI_APPS_D				0x0C034
#define BLSP2_UART1_APPS_CBCR				0x0C03C
#define BLSP2_UART1_SIM_CBCR				0x0C040
#define BLSP2_UART1_APPS_CMD_RCGR			0x0C044
#define BLSP2_UART1_APPS_CFG_RCGR			0x0C048
#define BLSP2_UART1_APPS_M				0x0C04C
#define BLSP2_UART1_APPS_N				0x0C050
#define BLSP2_UART1_APPS_D				0x0C054
#define BLSP2_QUP2_SPI_APPS_CBCR			0x0D00C
#define BLSP2_QUP2_I2C_APPS_CBCR			0x0D010
#define BLSP2_QUP2_SPI_APPS_CMD_RCGR			0x0D014
#define BLSP2_QUP2_SPI_APPS_CFG_RCGR			0x0D018
#define BLSP2_QUP2_SPI_APPS_M				0x0D01C
#define BLSP2_QUP2_SPI_APPS_N				0x0D020
#define BLSP2_QUP2_SPI_APPS_D				0x0D024
#define BLSP2_UART2_APPS_CBCR				0x0D02C
#define BLSP2_UART2_SIM_CBCR				0x0D030
#define BLSP2_UART2_APPS_CMD_RCGR			0x0D034
#define BLSP2_UART2_APPS_CFG_RCGR			0x0D038
#define BLSP2_UART2_APPS_M				0x0D03C
#define BLSP2_UART2_APPS_N				0x0D040
#define BLSP2_UART2_APPS_D				0x0D044
#define BLSP2_QUP3_SPI_APPS_CBCR			0x0F01C
#define BLSP2_QUP3_I2C_APPS_CBCR			0x0F020
#define BLSP2_QUP3_SPI_APPS_CMD_RCGR			0x0F024
#define BLSP2_QUP3_SPI_APPS_CFG_RCGR			0x0F028
#define BLSP2_QUP3_SPI_APPS_M				0x0F02C
#define BLSP2_QUP3_SPI_APPS_N				0x0F030
#define BLSP2_QUP3_SPI_APPS_D				0x0F034
#define BLSP2_QUP4_SPI_APPS_CBCR			0x1801C
#define BLSP2_QUP4_I2C_APPS_CBCR			0x18020
#define BLSP2_QUP4_SPI_APPS_CMD_RCGR			0x18024
#define BLSP2_QUP4_SPI_APPS_CFG_RCGR			0x18028
#define BLSP2_QUP4_SPI_APPS_M				0x1802C
#define BLSP2_QUP4_SPI_APPS_N				0x18030
#define BLSP2_QUP4_SPI_APPS_D				0x18034
#define BLSP_UART_SIM_CMD_RCGR				0x0100C
#define BLSP_UART_SIM_CFG_RCGR				0x01010
#define PRNG_XPU_CFG_AHB_CBCR				0x17008
#define PDM_AHB_CBCR					0x44004
#define PDM_XO4_CBCR					0x44008
#define PDM2_CBCR					0x4400C
#define PDM2_CMD_RCGR					0x44010
#define PDM2_CFG_RCGR					0x44014
#define PRNG_AHB_CBCR					0x13004
#define BOOT_ROM_AHB_CBCR				0x1300C
#define CRYPTO_CMD_RCGR					0x16004
#define CRYPTO_CFG_RCGR					0x16008
#define CRYPTO_CBCR					0x1601C
#define CRYPTO_AXI_CBCR					0x16020
#define CRYPTO_AHB_CBCR					0x16024
#define GCC_XO_DIV4_CBCR				0x30034
#define APSS_TCU_CMD_RCGR				0x38000
#define APSS_TCU_CFG_RCGR				0x38004
#define APSS_AXI_CMD_RCGR				0x38048
#define APSS_AXI_CFG_RCGR				0x3804C
#define APSS_AHB_CMD_RCGR				0x46000
#define APSS_AHB_CFG_RCGR				0x46004
#define APSS_AHB_MISC					0x46018
#define APSS_AHB_CBCR					0x4601C
#define APSS_AXI_CBCR					0x46020
#define VENUS_TBU_CBCR					0x12014
#define APSS_TCU_ASYNC_CBCR				0x12018
#define MDP_TBU_CBCR					0x1201C
#define JPEG_TBU_CBCR					0x12034
#define SMMU_CFG_CBCR					0x12038
#define VFE_TBU_CBCR					0x1203C
#define VFE1_TBU_CBCR					0x12090
#define CPP_TBU_CBCR					0x12040
#define RBCPR_GFX_CBCR					0x3A004
#define RBCPR_GFX_CMD_RCGR				0x3A00C
#define RBCPR_GFX_CFG_RCGR				0x3A010
#define APCS_GPLL_ENA_VOTE				0x45000
#define APCS_CLOCK_BRANCH_ENA_VOTE			0x45004
#define APCS_SMMU_CLOCK_BRANCH_ENA_VOTE			0x4500C
#define APCS_CLOCK_SLEEP_ENA_VOTE			0x45008
#define APCS_SMMU_CLOCK_SLEEP_ENA_VOTE			0x45010
#define GCC_DEBUG_CLK_CTL				0x74000
#define CLOCK_FRQ_MEASURE_CTL				0x74004
#define CLOCK_FRQ_MEASURE_STATUS			0x74008
#define PLLTEST_PAD_CFG					0x7400C
#define GP1_CBCR					0x08000
#define GP1_CMD_RCGR					0x08004
#define GP1_CFG_RCGR					0x08008
#define GP1_M						0x0800C
#define GP1_N						0x08010
#define GP1_D						0x08014
#define GP2_CBCR					0x09000
#define GP2_CMD_RCGR					0x09004
#define GP2_CFG_RCGR					0x09008
#define GP2_M						0x0900C
#define GP2_N						0x09010
#define GP2_D						0x09014
#define GP3_CBCR					0x0A000
#define GP3_CMD_RCGR					0x0A004
#define GP3_CFG_RCGR					0x0A008
#define GP3_M						0x0A00C
#define GP3_N						0x0A010
#define GP3_D						0x0A014
#define APSS_MISC					0x60000
#define VCODEC0_CMD_RCGR				0x4C000
#define VCODEC0_CFG_RCGR				0x4C004
#define VCODEC0_M					0x4C008
#define VCODEC0_N					0x4C00C
#define VCODEC0_D					0x4C010
#define VENUS0_VCODEC0_CBCR				0x4C01C
#define VENUS0_CORE0_VCODEC0_CBCR			0x4C02C
#define VENUS0_AHB_CBCR					0x4C020
#define VENUS0_AXI_CBCR					0x4C024
#define PCLK0_CMD_RCGR					0x4D000
#define PCLK0_CFG_RCGR					0x4D004
#define PCLK0_M						0x4D008
#define PCLK0_N						0x4D00C
#define PCLK0_D						0x4D010
#define PCLK1_CMD_RCGR					0x4D0B8
#define PCLK1_CFG_RCGR					0x4D0BC
#define PCLK1_M						0x4D0C0
#define PCLK1_N						0x4D0C4
#define PCLK1_D						0x4D0C8
#define MDP_CMD_RCGR					0x4D014
#define MDP_CFG_RCGR					0x4D018
#define VSYNC_CMD_RCGR					0x4D02C
#define VSYNC_CFG_RCGR					0x4D030
#define BYTE0_CMD_RCGR					0x4D044
#define BYTE0_CFG_RCGR					0x4D048
#define BYTE1_CMD_RCGR					0x4D0B0
#define BYTE1_CFG_RCGR					0x4D0B4
#define ESC0_CMD_RCGR					0x4D05C
#define ESC0_CFG_RCGR					0x4D060
#define ESC1_CMD_RCGR					0x4D0A8
#define ESC1_CFG_RCGR					0x4D0AC
#define MDSS_AHB_CBCR					0x4D07C
#define MDSS_AXI_CBCR					0x4D080
#define MDSS_PCLK0_CBCR					0x4D084
#define MDSS_PCLK1_CBCR					0x4D0A4
#define MDSS_MDP_CBCR					0x4D088
#define MDSS_VSYNC_CBCR					0x4D090
#define MDSS_BYTE0_CBCR					0x4D094
#define MDSS_BYTE1_CBCR					0x4D0A0
#define MDSS_ESC0_CBCR					0x4D098
#define MDSS_ESC1_CBCR					0x4D09C
#define CSI0PHYTIMER_CMD_RCGR				0x4E000
#define CSI0PHYTIMER_CFG_RCGR				0x4E004
#define CAMSS_CSI0PHYTIMER_CBCR				0x4E01C
#define CSI0P_CMD_RCGR					0x58084
#define CSI0P_CFG_RCGR					0x58088
#define CAMSS_CSI0_CSIPHY_3P_CBCR			0x58090
#define CSI1P_CMD_RCGR					0x58094
#define CSI1P_CFG_RCGR					0x58098
#define CAMSS_CSI1_CSIPHY_3P_CBCR			0x580A0
#define CSI2P_CMD_RCGR					0x580A4
#define CSI2P_CFG_RCGR					0x580A8
#define CAMSS_CSI2_CSIPHY_3P_CBCR			0x580B0
#define CSI1PHYTIMER_CMD_RCGR				0x4F000
#define CSI1PHYTIMER_CFG_RCGR				0x4F004
#define CAMSS_CSI1PHYTIMER_CBCR				0x4F01C
#define CSI0_CMD_RCGR					0x4E020
#define CSI2PHYTIMER_CMD_RCGR				0x4F05C
#define CSI2PHYTIMER_CFG_RCGR				0x4F060
#define CAMSS_CSI2PHYTIMER_CBCR				0x4F068
#define CSI0_CFG_RCGR					0x4E024
#define CAMSS_CSI0_CBCR					0x4E03C
#define CAMSS_CSI0_AHB_CBCR				0x4E040
#define CAMSS_CSI0PHY_CBCR				0x4E048
#define CAMSS_CSI0RDI_CBCR				0x4E050
#define CAMSS_CSI0PIX_CBCR				0x4E058
#define CSI1_CMD_RCGR					0x4F020
#define CSI1_CFG_RCGR					0x4F024
#define CAMSS_CSI1_CBCR					0x4F03C
#define CAMSS_CSI1_AHB_CBCR				0x4F040
#define CAMSS_CSI1PHY_CBCR				0x4F048
#define CAMSS_CSI1RDI_CBCR				0x4F050
#define CAMSS_CSI1PIX_CBCR				0x4F058
#define CSI2_CMD_RCGR					0x3C020
#define CSI2_CFG_RCGR					0x3C024
#define CAMSS_CSI2_CBCR					0x3C03C
#define CAMSS_CSI2_AHB_CBCR				0x3C040
#define CAMSS_CSI2PHY_CBCR				0x3C048
#define CAMSS_CSI2RDI_CBCR				0x3C050
#define CAMSS_CSI2PIX_CBCR				0x3C058
#define CAMSS_ISPIF_AHB_CBCR				0x50004
#define CCI_CMD_RCGR					0x51000
#define CCI_CFG_RCGR					0x51004
#define CCI_M						0x51008
#define CCI_N						0x5100C
#define CCI_D						0x51010
#define CAMSS_CCI_CBCR					0x51018
#define CAMSS_CCI_AHB_CBCR				0x5101C
#define MCLK0_CMD_RCGR					0x52000
#define MCLK0_CFG_RCGR					0x52004
#define MCLK0_M						0x52008
#define MCLK0_N						0x5200C
#define MCLK0_D						0x52010
#define CAMSS_MCLK0_CBCR				0x52018
#define MCLK1_CMD_RCGR					0x53000
#define MCLK1_CFG_RCGR					0x53004
#define MCLK1_M						0x53008
#define MCLK1_N						0x5300C
#define MCLK1_D						0x53010
#define CAMSS_MCLK1_CBCR				0x53018
#define MCLK2_CMD_RCGR					0x5C000
#define MCLK2_CFG_RCGR					0x5C004
#define MCLK2_M						0x5C008
#define MCLK2_N						0x5C00C
#define MCLK2_D						0x5C010
#define CAMSS_MCLK2_CBCR				0x5C018
#define MCLK3_CMD_RCGR					0x5E000
#define MCLK3_CFG_RCGR					0x5E004
#define MCLK3_M						0x5E008
#define MCLK3_N						0x5E00C
#define MCLK3_D						0x5E010
#define CAMSS_MCLK3_CBCR				0x5E018
#define CAMSS_GP0_CMD_RCGR				0x54000
#define CAMSS_GP0_CFG_RCGR				0x54004
#define CAMSS_GP0_M					0x54008
#define CAMSS_GP0_N					0x5400C
#define CAMSS_GP0_D					0x54010
#define CAMSS_GP0_CBCR					0x54018
#define CAMSS_GP1_CMD_RCGR				0x55000
#define CAMSS_GP1_CFG_RCGR				0x55004
#define CAMSS_GP1_M					0x55008
#define CAMSS_GP1_N					0x5500C
#define CAMSS_GP1_D					0x55010
#define CAMSS_GP1_CBCR					0x55018
#define CAMSS_TOP_AHB_CBCR				0x5A014
#define CAMSS_AHB_CBCR					0x56004
#define CAMSS_MICRO_BCR					0x56008
#define CAMSS_MICRO_AHB_CBCR				0x5600C
#define JPEG0_CMD_RCGR					0x57000
#define JPEG0_CFG_RCGR					0x57004
#define CAMSS_JPEG0_CBCR				0x57020
#define CAMSS_JPEG_AHB_CBCR				0x57024
#define CAMSS_JPEG_AXI_CBCR				0x57028
#define VFE0_CMD_RCGR					0x58000
#define VFE0_CFG_RCGR					0x58004
#define CPP_CMD_RCGR					0x58018
#define CPP_CFG_RCGR					0x5801C
#define CAMSS_VFE0_CBCR					0x58038
#define CAMSS_CPP_CBCR					0x5803C
#define CAMSS_CPP_AHB_CBCR				0x58040
#define CAMSS_VFE_AHB_CBCR				0x58044
#define CAMSS_VFE_AXI_CBCR				0x58048
#define CAMSS_CSI_VFE0_CBCR				0x58050
#define VFE1_CMD_RCGR					0x58054
#define VFE1_CFG_RCGR					0x58058
#define CAMSS_VFE1_CBCR					0x5805C
#define CAMSS_VFE1_AHB_CBCR				0x58060
#define CAMSS_CPP_AXI_CBCR				0x58064
#define CAMSS_VFE1_AXI_CBCR				0x58068
#define CAMSS_CSI_VFE1_CBCR				0x58074
#define GFX3D_CMD_RCGR					0x59000
#define GFX3D_CFG_RCGR					0x59004
#define OXILI_GFX3D_CBCR				0x59020
#define OXILI_AHB_CBCR					0x59028
#define BIMC_GPU_CBCR					0x59030
#define OXILI_TIMER_CBCR				0x59040
#define CAMSS_TOP_AHB_CMD_RCGR				0x5A000
#define CAMSS_TOP_AHB_CFG_RCGR				0x5A004
#define CAMSS_TOP_AHB_M					0x5A008
#define CAMSS_TOP_AHB_N					0x5A00C
#define CAMSS_TOP_AHB_D					0x5A010
#define GX_DOMAIN_MISC					0x5B00C
#define APC0_VOLTAGE_DROOP_DETECTOR_GPLL0_CBCR		0x78004
#define APC0_VOLTAGE_DROOP_DETECTOR_CMD_RCGR		0x78008
#define APC0_VOLTAGE_DROOP_DETECTOR_CFG_RCGR		0x7800C
#define APC1_VOLTAGE_DROOP_DETECTOR_GPLL0_CBCR		0x79004
#define APC1_VOLTAGE_DROOP_DETECTOR_CMD_RCGR		0x79008
#define APC1_VOLTAGE_DROOP_DETECTOR_CFG_RCGR		0x7900C
#define QUSB_REF_CLK_EN					0x41030
#define USB_SS_REF_CLK_EN				0x3F07C

/* Mux source select values */
#define xo_src_val			0
#define xo_a_src_val			0
#define xo_pipe_src_val			1

#define gpll0_src_val			1
#define gpll0_main_src_val		2   /* cci_clk_src */
#define gpll0_main_mock_src_val		3   /* usb30_mock_utmi_clk_src */

#define gpll0_main_div2_usb3_src_val	2   /* usb30_master_clk_src
					     * rbcpr_gfx_clk_src
					     */
#define gpll0_main_div2_src_val		4
#define gpll0_main_div2_cci_src_val	3   /* cci_clk_src */
#define gpll0_main_div2_mm_src_val	5   /* gfx3d_clk_src vfe0_clk_src
					     * vfe1_clk_src cpp_clk_src
					     * csi0_clk_src csi0p_clk_src
					     * csi1p_clk_src csi2p_clk_src
					     */
#define gpll0_main_div2_axi_src_val	6   /* apss_axi_clk_src */

#define gpll2_src_val			4   /* vfe0_clk_src  vfe1_clk_src
					     * cpp_clk_src   csi0_clk_src
					     * csi0p_clk_src csi1p_clk_src
					     * csi2p_clk_src
					     */

#define gpll2_out_main_src_val		5   /* jpeg0_clk_src csi1_clk_src
					     * csi2_clk_src
					     */
#define gpll2_vcodec_src_val		3   /* vcodec0_clk_src */

#define gpll3_src_val			2   /* gfx3d_clk_src */

#define gpll4_src_val			2   /* sdcc1_apss_clk_src v_droop */
#define gpll4_aux_src_val		2   /* sdcc2_apss_clk_src */
#define gpll4_out_aux_src_val		4   /* gfx3d_clk_src */

#define gpll6_main_src_val		1   /* usb30_mock_utmi_clk_src */
#define gpll6_src_val			2
#define gpll6_main_gfx_src_val		3   /* gfx3d_clk_src */

#define gpll6_main_div2_mock_src_val    2   /* usb30_mock_utmi_clk_src */

#define gpll6_main_div2_src_val		5   /* mclk0_clk_src mclk1_clk_src
					     * mclk2_clk_src mclk3_clk_src
					     */
#define gpll6_main_div2_gfx_src_val	6   /* gfx3d_clk_src */

#define gpll6_aux_src_val		2  /* gp1_clk_src gp2_clk_src
					    * gp3_clk_src camss_gp0_clk_src
					    * camss_gp1_clk_src
					    */

#define gpll6_out_aux_src_val		3   /* mdp_clk_src cpp_clk_src */

#define usb3_pipe_src_val		0

#define dsi0_phypll_mm_src_val		1   /* byte0_clk & pclk0_clk */
#define dsi1_phypll_mm_src_val		3   /* byte0_clk & pclk0_clk */

#define dsi0_phypll_clk_mm_src_val	3   /* byte1_clk & pclk1_clk */
#define dsi1_phypll_clk_mm_src_val	1   /* byte1_clk & pclk1_clk */

#define F(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_src_val), \
	}

#define F_MM(f, s_f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_freq = (s_f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_src_val), \
	}

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
		[VDD_DIG_##l3] = (f3),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP4(l1, f1, l2, f2, l3, f3, l4, f4) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
		[VDD_DIG_##l3] = (f3),          \
		[VDD_DIG_##l4] = (f4),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP5(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
		[VDD_DIG_##l3] = (f3),          \
		[VDD_DIG_##l4] = (f4),          \
		[VDD_DIG_##l5] = (f5),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP6(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5, l6, f6) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
		[VDD_DIG_##l3] = (f3),          \
		[VDD_DIG_##l4] = (f4),          \
		[VDD_DIG_##l5] = (f5),          \
		[VDD_DIG_##l6] = (f6),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP7(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5, l6, \
							   f6, l7, f7) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {  \
		[VDD_DIG_##l1] = (f1),          \
		[VDD_DIG_##l2] = (f2),          \
		[VDD_DIG_##l3] = (f3),          \
		[VDD_DIG_##l4] = (f4),          \
		[VDD_DIG_##l5] = (f5),          \
		[VDD_DIG_##l6] = (f6),          \
		[VDD_DIG_##l7] = (f7),          \
	},                                      \
	.num_fmax = VDD_DIG_NUM

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_MIN_SVS,
	VDD_DIG_LOW_SVS,
	VDD_DIG_SVS,
	VDD_DIG_SVS_PLUS,
	VDD_DIG_NOM,
	VDD_DIG_NOM_PLUS,
	VDD_DIG_HIGH,
	VDD_DIG_NUM
};

static int vdd_level[] = {
	RPM_REGULATOR_LEVEL_NONE,		/* VDD_DIG_NONE */
	RPM_REGULATOR_LEVEL_MIN_SVS,		/* VDD_DIG_MIN_SVS */
	RPM_REGULATOR_LEVEL_LOW_SVS,		/* VDD_DIG_LOW_SVS*/
	RPM_REGULATOR_LEVEL_SVS,		/* VDD_DIG_SVS */
	RPM_REGULATOR_LEVEL_SVS_PLUS,		/* VDD_DIG_SVS_PLUS */
	RPM_REGULATOR_LEVEL_NOM,		/* VDD_DIG_NOM */
	RPM_REGULATOR_LEVEL_NOM_PLUS,		/* VDD_DIG_NOM_PLUS */
	RPM_REGULATOR_LEVEL_TURBO,		/* VDD_DIG_TURBO */
};

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_level, NULL);
static DEFINE_VDD_REGS_INIT(vdd_gfx, 1);

#define RPM_MISC_CLK_TYPE       0x306b6c63
#define RPM_BUS_CLK_TYPE        0x316b6c63
#define RPM_MEM_CLK_TYPE        0x326b6c63
#define RPM_IPA_CLK_TYPE        0x00617069
#define RPM_SMD_KEY_ENABLE      0x62616E45

#define XO_ID                   0x0
#define QDSS_ID                 0x1
#define BUS_SCALING             0x2

#define PCNOC_ID                0x0
#define SNOC_ID                 0x1
#define SYSMMNOC_ID             0x2
#define BIMC_ID                 0x0
#define IPA_ID                  0x0

#define BB_CLK1_ID              0x1
#define BB_CLK2_ID              0x2
#define RF_CLK2_ID              0x5
#define RF_CLK3_ID              0x8
#define DIV_CLK1_ID             0xB
#define DIV_CLK2_ID		0xC

#endif


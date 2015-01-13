/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Shobhit Kumar <shobhit.kumar@intel.com>
 *
 */

#include <linux/slab.h>
#include <video/mipi_display.h>
#include <asm/intel-mid.h>
#include <drm/i915_drm.h>
#include <drm/i915_adf.h>
#include <core/common/dsi/dsi_config.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/dsi/dsi_panel.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_dc_gpio.h>
#include <core/vlv/vlv_dc_config.h>
#include <linux/i2c.h>
#include <linux/mfd/intel_soc_pmic.h>
#include "intel_dsi.h"
#include "intel_dsi_cmd.h"
#include "dsi_vbt.h"

#define MIPI_TRANSFER_MODE_SHIFT	0
#define MIPI_VIRTUAL_CHANNEL_SHIFT	1
#define MIPI_PORT_SHIFT			3

#define PREPARE_CNT_MAX		0x3F
#define EXIT_ZERO_CNT_MAX	0x3F
#define CLK_ZERO_CNT_MAX	0xFF
#define TRAIL_CNT_MAX		0x1F

#define NS_KHZ_RATIO 1000000

struct gpio_table {
	u16 function_reg;
	u16 pad_reg;
	u8 init;
};

static struct gpio_table gtable[] = {
	{HV_DDI0_HPD_GPIONC_0_PCONF0, HV_DDI0_HPD_GPIONC_0_PAD, 0},
	{HV_DDI0_DDC_SDA_GPIONC_1_PCONF0, HV_DDI0_DDC_SDA_GPIONC_1_PAD, 0},
	{HV_DDI0_DDC_SCL_GPIONC_2_PCONF0, HV_DDI0_DDC_SCL_GPIONC_2_PAD, 0},
	{PANEL0_VDDEN_GPIONC_3_PCONF0, PANEL0_VDDEN_GPIONC_3_PAD, 0},
	{PANEL0_BKLTEN_GPIONC_4_PCONF0, PANEL0_BKLTEN_GPIONC_4_PAD, 0},
	{PANEL0_BKLTCTL_GPIONC_5_PCONF0, PANEL0_BKLTCTL_GPIONC_5_PAD, 0},
	{HV_DDI1_HPD_GPIONC_6_PCONF0, HV_DDI1_HPD_GPIONC_6_PAD, 0},
	{HV_DDI1_DDC_SDA_GPIONC_7_PCONF0, HV_DDI1_DDC_SDA_GPIONC_7_PAD, 0},
	{HV_DDI1_DDC_SCL_GPIONC_8_PCONF0, HV_DDI1_DDC_SCL_GPIONC_8_PAD, 0},
	{PANEL1_VDDEN_GPIONC_9_PCONF0, PANEL1_VDDEN_GPIONC_9_PAD, 0},
	{PANEL1_BKLTEN_GPIONC_10_PCONF0, PANEL1_BKLTEN_GPIONC_10_PAD, 0},
	{PANEL1_BKLTCTL_GPIONC_11_PCONF0, PANEL1_BKLTCTL_GPIONC_11_PAD, 0},
	{GP_INTD_DSI_TE1_GPIONC_12_PCONF0, GP_INTD_DSI_TE1_GPIONC_12_PAD, 0},
	{HV_DDI2_DDC_SDA_GPIONC_13_PCONF0, HV_DDI2_DDC_SDA_GPIONC_13_PAD, 0},
	{HV_DDI2_DDC_SCL_GPIONC_14_PCONF0, HV_DDI2_DDC_SCL_GPIONC_14_PAD, 0},
	{GP_CAMERASB00_GPIONC_15_PCONF0, GP_CAMERASB00_GPIONC_15_PAD, 0},
	{GP_CAMERASB01_GPIONC_16_PCONF0, GP_CAMERASB01_GPIONC_16_PAD, 0},
	{GP_CAMERASB02_GPIONC_17_PCONF0, GP_CAMERASB02_GPIONC_17_PAD, 0},
	{GP_CAMERASB03_GPIONC_18_PCONF0, GP_CAMERASB03_GPIONC_18_PAD, 0},
	{GP_CAMERASB04_GPIONC_19_PCONF0, GP_CAMERASB04_GPIONC_19_PAD, 0},
	{GP_CAMERASB05_GPIONC_20_PCONF0, GP_CAMERASB05_GPIONC_20_PAD, 0},
	{GP_CAMERASB06_GPIONC_21_PCONF0, GP_CAMERASB06_GPIONC_21_PAD, 0},
	{GP_CAMERASB07_GPIONC_22_PCONF0, GP_CAMERASB07_GPIONC_22_PAD, 0},
	{GP_CAMERASB08_GPIONC_23_PCONF0, GP_CAMERASB08_GPIONC_23_PAD, 0},
	{GP_CAMERASB09_GPIONC_24_PCONF0, GP_CAMERASB09_GPIONC_24_PAD, 0},
	{GP_CAMERASB10_GPIONC_25_PCONF0, GP_CAMERASB10_GPIONC_25_PAD, 0},
	{GP_CAMERASB11_GPIONC_26_PCONF0, GP_CAMERASB11_GPIONC_26_PAD, 0},

	{SATA_GP0_GPIOC_0_PCONF0, SATA_GP0_GPIOC_0_PAD, 0},
	{SATA_GP1_GPIOC_1_PCONF0, SATA_GP1_GPIOC_1_PAD, 0},
	{SATA_LEDN_GPIOC_2_PCONF0, SATA_LEDN_GPIOC_2_PAD, 0},
	{PCIE_CLKREQ0B_GPIOC_3_PCONF0, PCIE_CLKREQ0B_GPIOC_3_PAD, 0},
	{PCIE_CLKREQ1B_GPIOC_4_PCONF0, PCIE_CLKREQ1B_GPIOC_4_PAD, 0},
	{PCIE_CLKREQ2B_GPIOC_5_PCONF0, PCIE_CLKREQ2B_GPIOC_5_PAD, 0},
	{PCIE_CLKREQ3B_GPIOC_6_PCONF0, PCIE_CLKREQ3B_GPIOC_6_PAD, 0},
	{PCIE_CLKREQ4B_GPIOC_7_PCONF0, PCIE_CLKREQ4B_GPIOC_7_PAD, 0},
	{HDA_RSTB_GPIOC_8_PCONF0, HDA_RSTB_GPIOC_8_PAD, 0},
	{HDA_SYNC_GPIOC_9_PCONF0, HDA_SYNC_GPIOC_9_PAD, 0},
	{HDA_CLK_GPIOC_10_PCONF0, HDA_CLK_GPIOC_10_PAD, 0},
	{HDA_SDO_GPIOC_11_PCONF0, HDA_SDO_GPIOC_11_PAD, 0},
	{HDA_SDI0_GPIOC_12_PCONF0, HDA_SDI0_GPIOC_12_PAD, 0},
	{HDA_SDI1_GPIOC_13_PCONF0, HDA_SDI1_GPIOC_13_PAD, 0},
	{HDA_DOCKRSTB_GPIOC_14_PCONF0, HDA_DOCKRSTB_GPIOC_14_PAD, 0},
	{HDA_DOCKENB_GPIOC_15_PCONF0, HDA_DOCKENB_GPIOC_15_PAD, 0},
	{SDMMC1_CLK_GPIOC_16_PCONF0, SDMMC1_CLK_GPIOC_16_PAD, 0},
	{SDMMC1_D0_GPIOC_17_PCONF0, SDMMC1_D0_GPIOC_17_PAD, 0},
	{SDMMC1_D1_GPIOC_18_PCONF0, SDMMC1_D1_GPIOC_18_PAD, 0},
	{SDMMC1_D2_GPIOC_19_PCONF0, SDMMC1_D2_GPIOC_19_PAD, 0},
	{SDMMC1_D3_CD_B_GPIOC_20_PCONF0, SDMMC1_D3_CD_B_GPIOC_20_PAD, 0},
	{MMC1_D4_SD_WE_GPIOC_21_PCONF0, MMC1_D4_SD_WE_GPIOC_21_PAD, 0},
	{MMC1_D5_GPIOC_22_PCONF0, MMC1_D5_GPIOC_22_PAD, 0},
	{MMC1_D6_GPIOC_23_PCONF0, MMC1_D6_GPIOC_23_PAD, 0},
	{MMC1_D7_GPIOC_24_PCONF0, MMC1_D7_GPIOC_24_PAD, 0},
	{SDMMC1_CMD_GPIOC_25_PCONF0, SDMMC1_CMD_GPIOC_25_PAD, 0},
	{MMC1_RESET_B_GPIOC_26_PCONF0, MMC1_RESET_B_GPIOC_26_PAD, 0},
	{SDMMC2_CLK_GPIOC_27_PCONF0, SDMMC2_CLK_GPIOC_27_PAD, 0},
	{SDMMC2_D0_GPIOC_28_PCONF0, SDMMC2_D0_GPIOC_28_PAD, 0},
	{SDMMC2_D1_GPIOC_29_PCONF0, SDMMC2_D1_GPIOC_29_PAD, 0},
	{SDMMC2_D2_GPIOC_30_PCONF0, SDMMC2_D2_GPIOC_30_PAD, 0},
	{SDMMC2_D3_CD_B_GPIOC_31_PCONF0, SDMMC2_D3_CD_B_GPIOC_31_PAD, 0},
	{SDMMC2_CMD_GPIOC_32_PCONF0, SDMMC2_CMD_GPIOC_32_PAD, 0},
	{SDMMC3_CLK_GPIOC_33_PCONF0, SDMMC3_CLK_GPIOC_33_PAD, 0},
	{SDMMC3_D0_GPIOC_34_PCONF0, SDMMC3_D0_GPIOC_34_PAD, 0},
	{SDMMC3_D1_GPIOC_35_PCONF0, SDMMC3_D1_GPIOC_35_PAD, 0},
	{SDMMC3_D2_GPIOC_36_PCONF0, SDMMC3_D2_GPIOC_36_PAD, 0},
	{SDMMC3_D3_GPIOC_37_PCONF0, SDMMC3_D3_GPIOC_37_PAD, 0},
	{SDMMC3_CD_B_GPIOC_38_PCONF0, SDMMC3_CD_B_GPIOC_38_PAD, 0},
	{SDMMC3_CMD_GPIOC_39_PCONF0, SDMMC3_CMD_GPIOC_39_PAD, 0},
	{SDMMC3_1P8_EN_GPIOC_40_PCONF0, SDMMC3_1P8_EN_GPIOC_40_PAD, 0},
	{SDMMC3_PWR_EN_B_GPIOC_41_PCONF0, SDMMC3_PWR_EN_B_GPIOC_41_PAD, 0},
	{LPC_AD0_GPIOC_42_PCONF0, LPC_AD0_GPIOC_42_PAD, 0},
	{LPC_AD1_GPIOC_43_PCONF0, LPC_AD1_GPIOC_43_PAD, 0},
	{LPC_AD2_GPIOC_44_PCONF0, LPC_AD2_GPIOC_44_PAD, 0},
	{LPC_AD3_GPIOC_45_PCONF0, LPC_AD3_GPIOC_45_PAD, 0},
	{LPC_FRAMEB_GPIOC_46_PCONF0, LPC_FRAMEB_GPIOC_46_PAD, 0},
	{LPC_CLKOUT0_GPIOC_47_PCONF0, LPC_CLKOUT0_GPIOC_47_PAD, 0},
	{LPC_CLKOUT1_GPIOC_48_PCONF0, LPC_CLKOUT1_GPIOC_48_PAD, 0},
	{LPC_CLKRUNB_GPIOC_49_PCONF0, LPC_CLKRUNB_GPIOC_49_PAD, 0},
	{ILB_SERIRQ_GPIOC_50_PCONF0, ILB_SERIRQ_GPIOC_50_PAD, 0},
	{SMB_DATA_GPIOC_51_PCONF0, SMB_DATA_GPIOC_51_PAD, 0},
	{SMB_CLK_GPIOC_52_PCONF0, SMB_CLK_GPIOC_52_PAD, 0},
	{SMB_ALERTB_GPIOC_53_PCONF0, SMB_ALERTB_GPIOC_53_PAD, 0},
	{SPKR_GPIOC_54_PCONF0, SPKR_GPIOC_54_PAD, 0},
	{MHSI_ACDATA_GPIOC_55_PCONF0, MHSI_ACDATA_GPIOC_55_PAD, 0},
	{MHSI_ACFLAG_GPIOC_56_PCONF0, MHSI_ACFLAG_GPIOC_56_PAD, 0},
	{MHSI_ACREADY_GPIOC_57_PCONF0, MHSI_ACREADY_GPIOC_57_PAD, 0},
	{MHSI_ACWAKE_GPIOC_58_PCONF0, MHSI_ACWAKE_GPIOC_58_PAD, 0},
	{MHSI_CADATA_GPIOC_59_PCONF0, MHSI_CADATA_GPIOC_59_PAD, 0},
	{MHSI_CAFLAG_GPIOC_60_PCONF0, MHSI_CAFLAG_GPIOC_60_PAD, 0},
	{MHSI_CAREADY_GPIOC_61_PCONF0, MHSI_CAREADY_GPIOC_61_PAD, 0},
	{GP_SSP_2_CLK_GPIOC_62_PCONF0, GP_SSP_2_CLK_GPIOC_62_PAD, 0},
	{GP_SSP_2_FS_GPIOC_63_PCONF0, GP_SSP_2_FS_GPIOC_63_PAD, 0},
	{GP_SSP_2_RXD_GPIOC_64_PCONF0, GP_SSP_2_RXD_GPIOC_64_PAD, 0},
	{GP_SSP_2_TXD_GPIOC_65_PCONF0, GP_SSP_2_TXD_GPIOC_65_PAD, 0},
	{SPI1_CS0_B_GPIOC_66_PCONF0, SPI1_CS0_B_GPIOC_66_PAD, 0},
	{SPI1_MISO_GPIOC_67_PCONF0, SPI1_MISO_GPIOC_67_PAD, 0},
	{SPI1_MOSI_GPIOC_68_PCONF0, SPI1_MOSI_GPIOC_68_PAD, 0},
	{SPI1_CLK_GPIOC_69_PCONF0, SPI1_CLK_GPIOC_69_PAD, 0},
	{UART1_RXD_GPIOC_70_PCONF0, UART1_RXD_GPIOC_70_PAD, 0},
	{UART1_TXD_GPIOC_71_PCONF0, UART1_TXD_GPIOC_71_PAD, 0},
	{UART1_RTS_B_GPIOC_72_PCONF0, UART1_RTS_B_GPIOC_72_PAD, 0},
	{UART1_CTS_B_GPIOC_73_PCONF0, UART1_CTS_B_GPIOC_73_PAD, 0},
	{UART2_RXD_GPIOC_74_PCONF0, UART2_RXD_GPIOC_74_PAD, 0},
	{UART2_TXD_GPIOC_75_PCONF0, UART2_TXD_GPIOC_75_PAD, 0},
	{UART2_RTS_B_GPIOC_76_PCONF0, UART2_RTS_B_GPIOC_76_PAD, 0},
	{UART2_CTS_B_GPIOC_77_PCONF0, UART2_CTS_B_GPIOC_77_PAD, 0},
	{I2C0_SDA_GPIOC_78_PCONF0, I2C0_SDA_GPIOC_78_PAD, 0},
	{I2C0_SCL_GPIOC_79_PCONF0, I2C0_SCL_GPIOC_79_PAD, 0},
	{I2C1_SDA_GPIOC_80_PCONF0, I2C1_SDA_GPIOC_80_PAD, 0},
	{I2C1_SCL_GPIOC_81_PCONF0, I2C1_SCL_GPIOC_81_PAD, 0},
	{I2C2_SDA_GPIOC_82_PCONF0, I2C2_SDA_GPIOC_82_PAD, 0},
	{I2C2_SCL_GPIOC_83_PCONF0, I2C2_SCL_GPIOC_83_PAD, 0},
	{I2C3_SDA_GPIOC_84_PCONF0, I2C3_SDA_GPIOC_84_PAD, 0},
	{I2C3_SCL_GPIOC_85_PCONF0, I2C3_SCL_GPIOC_85_PAD, 0},
	{I2C4_SDA_GPIOC_86_PCONF0, I2C4_SDA_GPIOC_86_PAD, 0},
	{I2C4_SCL_GPIOC_87_PCONF0, I2C4_SCL_GPIOC_87_PAD, 0},
	{I2C5_SDA_GPIOC_88_PCONF0, I2C5_SDA_GPIOC_88_PAD, 0},
	{I2C5_SCL_GPIOC_89_PCONF0, I2C5_SCL_GPIOC_89_PAD, 0},
	{I2C6_SDA_GPIOC_90_PCONF0, I2C6_SDA_GPIOC_90_PAD, 0},
	{I2C6_SCL_GPIOC_91_PCONF0, I2C6_SCL_GPIOC_91_PAD, 0},
	{I2C_NFC_SDA_GPIOC_92_PCONF0, I2C_NFC_SDA_GPIOC_92_PAD, 0},
	{I2C_NFC_SCL_GPIOC_93_PCONF0, I2C_NFC_SCL_GPIOC_93_PAD, 0},
	{PWM0_GPIOC_94_PCONF0, PWM0_GPIOC_94_PAD, 0},
	{PWM1_GPIOC_95_PCONF0, PWM1_GPIOC_95_PAD, 0},
	{PLT_CLK0_GPIOC_96_PCONF0, PLT_CLK0_GPIOC_96_PAD, 0},
	{PLT_CLK1_GPIOC_97_PCONF0, PLT_CLK1_GPIOC_97_PAD, 0},
	{PLT_CLK2_GPIOC_98_PCONF0, PLT_CLK2_GPIOC_98_PAD, 0},
	{PLT_CLK3_GPIOC_99_PCONF0, PLT_CLK3_GPIOC_99_PAD, 0},
	{PLT_CLK4_GPIOC_100_PCONF0, PLT_CLK4_GPIOC_100_PAD, 0},
	{PLT_CLK5_GPIOC_101_PCONF0, PLT_CLK5_GPIOC_101_PAD, 0},

	{GPIO_SUS0_GPIO_SUS0_PCONF0, GPIO_SUS0_GPIO_SUS0_PAD, 0},
	{GPIO_SUS1_GPIO_SUS1_PCONF0, GPIO_SUS1_GPIO_SUS1_PAD, 0},
	{GPIO_SUS2_GPIO_SUS2_PCONF0, GPIO_SUS2_GPIO_SUS2_PAD, 0},
	{GPIO_SUS3_GPIO_SUS3_PCONF0, GPIO_SUS3_GPIO_SUS3_PAD, 0},
	{GPIO_SUS4_GPIO_SUS4_PCONF0, GPIO_SUS4_GPIO_SUS4_PAD, 0},
	{GPIO_SUS5_GPIO_SUS5_PCONF0, GPIO_SUS5_GPIO_SUS5_PAD, 0},
	{GPIO_SUS6_GPIO_SUS6_PCONF0, GPIO_SUS6_GPIO_SUS6_PAD, 0},
	{GPIO_SUS7_GPIO_SUS7_PCONF0, GPIO_SUS7_GPIO_SUS7_PAD, 0},
	{SEC_GPIO_SUS8_GPIO_SUS8_PCONF0, SEC_GPIO_SUS8_GPIO_SUS8_PAD, 0},
	{SEC_GPIO_SUS9_GPIO_SUS9_PCONF0, SEC_GPIO_SUS9_GPIO_SUS9_PAD, 0},
	{SEC_GPIO_SUS10_GPIO_SUS10_PCONF0, SEC_GPIO_SUS10_GPIO_SUS10_PAD, 0},
	{SUSPWRDNACK_GPIOS_11_PCONF0, SUSPWRDNACK_GPIOS_11_PAD, 0},
	{PMU_SUSCLK_GPIOS_12_PCONF0, PMU_SUSCLK_GPIOS_12_PAD, 0},
	{PMU_SLP_S0IX_B_GPIOS_13_PCONF0, PMU_SLP_S0IX_B_GPIOS_13_PAD, 0},
	{PMU_SLP_LAN_B_GPIOS_14_PCONF0, PMU_SLP_LAN_B_GPIOS_14_PAD, 0},
	{PMU_WAKE_B_GPIOS_15_PCONF0, PMU_WAKE_B_GPIOS_15_PAD, 0},
	{PMU_PWRBTN_B_GPIOS_16_PCONF0, PMU_PWRBTN_B_GPIOS_16_PAD, 0},
	{PMU_WAKE_LAN_B_GPIOS_17_PCONF0, PMU_WAKE_LAN_B_GPIOS_17_PAD, 0},
	{SUS_STAT_B_GPIOS_18_PCONF0, SUS_STAT_B_GPIOS_18_PAD, 0},
	{USB_OC0_B_GPIOS_19_PCONF0, USB_OC0_B_GPIOS_19_PAD, 0},
	{USB_OC1_B_GPIOS_20_PCONF0, USB_OC1_B_GPIOS_20_PAD, 0},
	{SPI_CS1_B_GPIOS_21_PCONF0, SPI_CS1_B_GPIOS_21_PAD, 0},
	{GPIO_DFX0_GPIOS_22_PCONF0, GPIO_DFX0_GPIOS_22_PAD, 0},
	{GPIO_DFX1_GPIOS_23_PCONF0, GPIO_DFX1_GPIOS_23_PAD, 0},
	{GPIO_DFX2_GPIOS_24_PCONF0, GPIO_DFX2_GPIOS_24_PAD, 0},
	{GPIO_DFX3_GPIOS_25_PCONF0, GPIO_DFX3_GPIOS_25_PAD, 0},
	{GPIO_DFX4_GPIOS_26_PCONF0, GPIO_DFX4_GPIOS_26_PAD, 0},
	{GPIO_DFX5_GPIOS_27_PCONF0, GPIO_DFX5_GPIOS_27_PAD, 0},
	{GPIO_DFX6_GPIOS_28_PCONF0, GPIO_DFX6_GPIOS_28_PAD, 0},
	{GPIO_DFX7_GPIOS_29_PCONF0, GPIO_DFX7_GPIOS_29_PAD, 0},
	{GPIO_DFX8_GPIOS_30_PCONF0, GPIO_DFX8_GPIOS_30_PAD, 0},
	{USB_ULPI_0_CLK_GPIOS_31_PCONF0, USB_ULPI_0_CLK_GPIOS_31_PAD, 0},
	{USB_ULPI_0_DATA0_GPIOS_32_PCONF0, USB_ULPI_0_DATA0_GPIOS_32_PAD, 0},
	{USB_ULPI_0_DATA1_GPIOS_33_PCONF0, USB_ULPI_0_DATA1_GPIOS_33_PAD, 0},
	{USB_ULPI_0_DATA2_GPIOS_34_PCONF0, USB_ULPI_0_DATA2_GPIOS_34_PAD, 0},
	{USB_ULPI_0_DATA3_GPIOS_35_PCONF0, USB_ULPI_0_DATA3_GPIOS_35_PAD, 0},
	{USB_ULPI_0_DATA4_GPIOS_36_PCONF0, USB_ULPI_0_DATA4_GPIOS_36_PAD, 0},
	{USB_ULPI_0_DATA5_GPIOS_37_PCONF0, USB_ULPI_0_DATA5_GPIOS_37_PAD, 0},
	{USB_ULPI_0_DATA6_GPIOS_38_PCONF0, USB_ULPI_0_DATA6_GPIOS_38_PAD, 0},
	{USB_ULPI_0_DATA7_GPIOS_39_PCONF0, USB_ULPI_0_DATA7_GPIOS_39_PAD, 0},
	{USB_ULPI_0_DIR_GPIOS_40_PCONF0, USB_ULPI_0_DIR_GPIOS_40_PAD, 0},
	{USB_ULPI_0_NXT_GPIOS_41_PCONF0, USB_ULPI_0_NXT_GPIOS_41_PAD, 0},
	{USB_ULPI_0_STP_GPIOS_42_PCONF0, USB_ULPI_0_STP_GPIOS_42_PAD, 0},
	{USB_ULPI_0_REFCLK_GPIOS_43_PCONF0, USB_ULPI_0_REFCLK_GPIOS_43_PAD, 0}
};

static u8 *mipi_exec_send_packet(struct dsi_pipe *dsi_pipe, u8 *data)
{
	struct dsi_context *intel_dsi = &dsi_pipe->config.ctx;
	u8 type, byte, mode, vc, port;
	u16 len;

	byte = *data++;
	mode = (byte >> MIPI_TRANSFER_MODE_SHIFT) & 0x1;
	vc = (byte >> MIPI_VIRTUAL_CHANNEL_SHIFT) & 0x3;
	port = (byte >> MIPI_PORT_SHIFT) & 0x3;

	/* LP or HS mode */
	intel_dsi->hs = mode;

	/* get packet type and increment the pointer */
	type = *data++;

	len = *((u16 *) data);
	data += 2;

	switch (type) {
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
		adf_dsi_vc_generic_write_0(dsi_pipe, vc);
		break;
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
		adf_dsi_vc_generic_write_1(dsi_pipe, vc, *data);
		break;
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
		adf_dsi_vc_generic_write_2(dsi_pipe, vc, *data, *(data + 1));
		break;
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
		pr_debug("Generic Read not yet implemented or used\n");
		break;
	case MIPI_DSI_GENERIC_LONG_WRITE:
		adf_dsi_vc_generic_write(dsi_pipe, vc, data, len);
		break;
	case MIPI_DSI_DCS_SHORT_WRITE:
		adf_dsi_vc_dcs_write_0(dsi_pipe, vc, *data);
		break;
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
		adf_dsi_vc_dcs_write_1(dsi_pipe, vc, *data, *(data + 1));
		break;
	case MIPI_DSI_DCS_READ:
		pr_debug("DCS Read not yet implemented or used\n");
		break;
	case MIPI_DSI_DCS_LONG_WRITE:
		adf_dsi_vc_dcs_write(dsi_pipe, vc, data, len);
		break;
	}

	data += len;

	return data;
}

static u8 *mipi_exec_delay(struct dsi_pipe *dsi_pipe, u8 *data)
{
	u32 delay = *((u32 *) data);

	usleep_range(delay, delay + 10);
	data += 4;

	return data;
}

static u8 *chv_mipi_exec_gpio(struct dsi_pipe *dsi_pipe, u8 *data)
{
	u8 gpio, action;
	u32 function, pad;
	u32 port;
	u16 familyno, padno;

	/*
	 * Skipping the first byte as it is of no
	 * interest for android in new version
	 */
	if (dsi_pipe->config.dsi->seq_version >= 3)
		data++;

	gpio = *data++;

	/* pull up/down */
	action = *data++;

	if (gpio > (TOTAL_NO_OF_GPIOS - 1)) {
		pr_err("%s: Unsupported gpio index for CHV\n", __func__);
		return NULL;
	}
	if (gpio >= 0 && gpio <= (NO_OF_NORTH_CORE_GPIOS - 1))
		port = IOSF_PORT_GPIO_NC;
	else if (gpio >= NO_OF_NORTH_CORE_GPIOS && gpio <=
		((NO_OF_NORTH_CORE_GPIOS + NO_OF_EAST_CORE_GPIOS) - 1)) {
		port = IOSF_PORT_GPIO_EC;
		gpio = gpio - NO_OF_NORTH_CORE_GPIOS;
	} else if (gpio >= (NO_OF_NORTH_CORE_GPIOS + NO_OF_EAST_CORE_GPIOS) &&
		gpio <= ((NO_OF_NORTH_CORE_GPIOS + NO_OF_EAST_CORE_GPIOS +
		NO_OF_SOUTH_WEST_CORE_GPIOS) - 1)) {
		port = IOSF_PORT_GPIO_SWC;
		gpio = gpio - (NO_OF_NORTH_CORE_GPIOS + NO_OF_EAST_CORE_GPIOS);
	} else if (gpio >= (NO_OF_NORTH_CORE_GPIOS + NO_OF_EAST_CORE_GPIOS +
		NO_OF_SOUTH_WEST_CORE_GPIOS &&
		gpio <= (TOTAL_NO_OF_GPIOS - 1))) {
		port = IOSF_PORT_GPIO_SEC;
		gpio = gpio - (NO_OF_NORTH_CORE_GPIOS + NO_OF_EAST_CORE_GPIOS +
		NO_OF_SOUTH_WEST_CORE_GPIOS);
	}

	familyno = gpio / MAX_FAMILY_PAD_GPIO_NO;
	padno = gpio % MAX_FAMILY_PAD_GPIO_NO;
	pad = FAMILY0_PAD_REGS_BASE + (FAMILY_PAD_REGS_SIZE * familyno) +
			(GPIO_REGS_SIZE * padno) + PAD_CONF0_OFFSET;
	function = pad + PAD_CONF1_OFFSET;
	vlv_gpio_write(port, function, 0x00000000);
	vlv_gpio_write(port, pad, 0x00008100 |
				(action << 1));

	return data;
}

static u8 *mipi_exec_gpio(struct dsi_pipe *dsi_pipe, u8 *data)
{
	u8 gpio, action;
	u32 function, pad;
	u32 port;
	u32 val;

	if (IS_CHERRYVIEW()) {
		data = chv_mipi_exec_gpio(dsi_pipe, data);
		goto out;
	}

	/*
	 * Skipping the first byte as it is of no
	 * interest for android in new version
	 */
	if (dsi_pipe->config.dsi->seq_version >= 3)
		data++;

	gpio = *data++;

	/* pull up/down */
	action = *data++;

	if (dsi_pipe->config.dsi->seq_version >= 3) {
		if (gpio <= MAX_GPIO_NUM_NC)
			port = IOSF_PORT_GPIO_NC;
		else if (gpio > MAX_GPIO_NUM_NC && gpio <= MAX_GPIO_NUM_SC)
			port = IOSF_PORT_GPIO_SC;
		else if (gpio > MAX_GPIO_NUM_SC && gpio <= MAX_GPIO_NUM)
			port = IOSF_PORT_GPIO_SUS;
		else {
			DRM_ERROR("GPIO number is not present in the table\n");
			return NULL;
		}
	} else {
		port = IOSF_PORT_GPIO_NC;
	}

	function = gtable[gpio].function_reg;
	pad = gtable[gpio].pad_reg;

	if (!gtable[gpio].init) {
		/* program the function */
		/* FIXME: remove constant below */
		vlv_gpio_write(port, function, 0x2000CC00);
		gtable[gpio].init = 1;
	}

	val = 0x4 | action;

	/* pull up/down */
	vlv_gpio_write(port, pad, val);
out:
	return data;
}

static u8 *mipi_exec_i2c(struct dsi_pipe *dsi_pipe, u8 *data)
{
	struct i2c_adapter *adapter;
	int ret;
	u8 reg_offset, payload_size, retries = 5;
	struct i2c_msg msg;
	u8 *transmit_buffer = NULL;

	u8 flag = *data++;
	u8 index = *data++;
	u8 bus_number = *data++;
	u16 slave_add = *(u16 *)(data);
	data = data + 2;
	reg_offset = *data++;
	payload_size = *data++;

	adapter = i2c_get_adapter(bus_number);

	if (!adapter) {
		DRM_ERROR("i2c_get_adapter(%u) failed, index:%u flag: %u\n",
				(bus_number + 1), index, flag);
		goto out;
	}

	transmit_buffer = kmalloc(1 + payload_size, GFP_TEMPORARY);

	if (!transmit_buffer)
		goto out;

	transmit_buffer[0] = reg_offset;
	memcpy(&transmit_buffer[1], data, (size_t)payload_size);

	msg.addr   = slave_add;
	msg.flags  = 0;
	msg.len    = 2;
	msg.buf    = &transmit_buffer[0];

	do {
		ret =  i2c_transfer(adapter, &msg, 1);
		if (ret == -EAGAIN)
			usleep_range(1000, 2500);
		else if (ret != 1) {
			DRM_ERROR("i2c transfer failed %d\n", ret);
			break;
		}
	} while (retries--);

	if (retries == 0)
		DRM_ERROR("i2c transfer failed");

out:
	kfree(transmit_buffer);

	data = data + payload_size;
	return data;
}

static u8 *mipi_exec_spi(struct dsi_pipe *dsi_pipe, u8 *data)
{
	u8 payload_size;

	/*
	 * SPI block is not used in linux, but if at all the
	 * VBT contains the SPI block we have to skip to the
	 * next block, hence reading the size of the SPI block
	 * and skipping the same.
	 */
	data = data + 5;
	payload_size = *data;
	data = data + payload_size + 1;

	return data;
}

static u8 *mipi_exec_pmic(struct dsi_pipe *dsi_pipe, u8 *data)
{
	u8 pmic_page;
	u32 register_address, register_data;
	u32 data_mask, tmp;

	data++;
	pmic_page = *data++;
	register_address = *((u32 *)data);
	data += 4;
	register_data = *((u32 *)data);
	data += 4;
	data_mask = *((u32 *)data);
	data += 4;

	tmp = intel_soc_pmic_readb(register_address);
	tmp &= ~data_mask;
	register_data &= data_mask;
	register_data |= tmp;
	intel_soc_pmic_writeb(register_address, register_data);

	return data;
}

typedef u8 * (*fn_mipi_elem_exec)(struct dsi_pipe *dsi_pipe, u8 *data);
static const fn_mipi_elem_exec exec_elem[] = {
	NULL, /* reserved */
	mipi_exec_send_packet,
	mipi_exec_delay,
	mipi_exec_gpio,
	mipi_exec_i2c,
	mipi_exec_spi,
	mipi_exec_pmic,
	NULL, /* status read; later */
};

/*
 * MIPI Sequence from VBT #53 parsing logic
 * We have already separated each seqence during bios parsing
 * Following is generic execution function for any sequence
 */

static const char * const seq_name[] = {
	"UNDEFINED",
	"MIPI_SEQ_ASSERT_RESET",
	"MIPI_SEQ_INIT_OTP",
	"MIPI_SEQ_DISPLAY_ON",
	"MIPI_SEQ_DISPLAY_OFF",
	"MIPI_SEQ_DEASSERT_RESET",
	"MIPI_SEQ_BACKLIGHT_ON",
	"MIPI_SEQ_BACKLIGHT_OFF",
	"MIPI_SEQ_TEAR_ON",
	"MIPI_SEQ_TEAR_OFF",
	"MIPI_SEQ_PANEL_ON",
	"MIPI_SEQ_PANEL_OFF"
};

static void panel_generic_exec_sequence(struct dsi_pipe *dsi_pipe,
					char *sequence)
{
	u8 *data = sequence;
	fn_mipi_elem_exec mipi_elem_exec;
	int index;

	if (!sequence)
		return;

	pr_debug("%s: Starting MIPI sequence - %s\n",
		__func__, seq_name[*data]);

	/* go to the first element of the sequence */
	data++;

	if (dsi_pipe->config.dsi->seq_version >= 3)
		data = data + 4;

	/* parse each byte till we reach end of sequence byte - 0x00 */
	while (1) {
		index = *data;
		pr_debug("ADF: %s: Element Type = %d\n", __func__, index);
		mipi_elem_exec = exec_elem[index];
		if (!mipi_elem_exec) {
			pr_err("ADF: %s: Unsupported MIPI element, skipping\n",
			__func__);
			return;
		}

		/* goto element payload */
		data++;

		if (dsi_pipe->config.dsi->seq_version >= 3)
			data++;

		/* execute the element specific rotines */
		data = mipi_elem_exec(dsi_pipe, data);

		/*
		 * After processing the element, data should point to
		 * next element or end of sequence
		 * check if have we reached end of sequence
		 */
		if (*data == 0x00)
			break;
	}
}

static int panel_generic_init(struct dsi_pipe *pipe)
{
	struct dsi_config *dsi_config = &pipe->config;
	struct dsi_context *intel_dsi = &dsi_config->ctx;
	struct dsi_vbt *dsi_vbt = NULL;
	struct mipi_config *mipi_config;
	struct drm_display_mode *mode = NULL;
	struct mipi_pps_data *pps;

	u32 bits_per_pixel = 24;
	u32 tlpx_ns, extra_byte_count, bitrate, tlpx_ui;
	u32 ui_num, ui_den;
	u32 prepare_cnt, exit_zero_cnt, clk_zero_cnt, trail_cnt;
	u32 ths_prepare_ns, tclk_trail_ns;
	u32 tclk_prepare_clkzero, ths_prepare_hszero;
	u32 lp_to_hs_switch, hs_to_lp_switch;
	u32 pclk, computed_ddr;
	u16 burst_mode_ratio;

	pr_debug("ADF: %s\n", __func__);

	/* get the VBT parsed MIPI data and support mode from i915 wrapper */
	intel_adf_get_dsi_vbt_data((void **)&dsi_vbt, &mode);
	if (!dsi_vbt || !mode) {
		pr_err("ADF: %s: No VBT data from i915\n", __func__);
		return -1;
	}

	dsi_config->dsi = dsi_vbt;
	memcpy(&dsi_config->vbt_mode, mode, sizeof(struct drm_display_mode));
	mipi_config = dsi_config->dsi->config;
	pps = dsi_config->dsi->pps;

	intel_dsi->eotp_pkt = mipi_config->eot_pkt_disabled ? 0 : 1;
	intel_dsi->clock_stop = mipi_config->enable_clk_stop ? 1 : 0;
	intel_dsi->lane_count = mipi_config->lane_cnt + 1;
	intel_dsi->pixel_format = mipi_config->videomode_color_format << 7;

	if (intel_dsi->pixel_format == VID_MODE_FORMAT_RGB666)
		bits_per_pixel = 18;
	else if (intel_dsi->pixel_format == VID_MODE_FORMAT_RGB565)
		bits_per_pixel = 16;

	intel_dsi->operation_mode = mipi_config->is_cmd_mode;
	intel_dsi->video_mode_format = mipi_config->video_transfer_mode;
	intel_dsi->escape_clk_div = mipi_config->byte_clk_sel;
	intel_dsi->lp_rx_timeout = mipi_config->lp_rx_timeout;
	intel_dsi->turn_arnd_val = mipi_config->turn_around_timeout;
	intel_dsi->rst_timer_val = mipi_config->device_reset_timer;
	intel_dsi->init_count = mipi_config->master_init_timer;
	intel_dsi->bw_timer = mipi_config->dbi_bw_timer;
	intel_dsi->video_frmt_cfg_bits =
		mipi_config->bta_enabled ? DISABLE_VIDEO_BTA : 0;
	intel_dsi->dual_link = mipi_config->dual_link;
	intel_dsi->pixel_overlap = mipi_config->pixel_overlap;

	if (intel_dsi->dual_link)
		intel_dsi->ports = ((1 << PORT_A) | (1 << PORT_C));

	pclk = mode->clock;

	/* In dual link mode each port needs half of pixel clock */
	if (intel_dsi->dual_link) {
		pclk = pclk / 2;

		/*
		 * we can enable pixel_overlap if needed by panel. In this
		 * case we need to increase the pixel clock for extra pixels
		 */
		if (intel_dsi->dual_link == DSI_DUAL_LINK_FRONT_BACK) {
			pclk += DIV_ROUND_UP(mode->vtotal *
						intel_dsi->pixel_overlap *
						60, 1000);
		}
	}

	/* Burst Mode Ratio
	 * Target ddr frequency from VBT / non burst ddr freq
	 * multiply by 100 to preserve remainder
	 */
	if (intel_dsi->video_mode_format == VIDEO_MODE_BURST) {
		if (mipi_config->target_burst_mode_freq) {
			computed_ddr =
				(pclk * bits_per_pixel) / intel_dsi->lane_count;

			if (mipi_config->target_burst_mode_freq <
								computed_ddr) {
				pr_err("Burst mode freq is less than computed\n");
				return -1;
			}

			burst_mode_ratio = DIV_ROUND_UP(
				mipi_config->target_burst_mode_freq * 100,
				computed_ddr);

			pclk = DIV_ROUND_UP(pclk * burst_mode_ratio, 100);
		} else {
			pr_err("Burst mode target is not set\n");
			return -1;
		}
	} else
		burst_mode_ratio = 100;

	intel_dsi->burst_mode_ratio = burst_mode_ratio;
	intel_dsi->pclk = pclk;

	bitrate = (pclk * bits_per_pixel) / intel_dsi->lane_count;

	switch (intel_dsi->escape_clk_div) {
	case 0:
		tlpx_ns = 50;
		break;
	case 1:
		tlpx_ns = 100;
		break;

	case 2:
		tlpx_ns = 200;
		break;
	default:
		tlpx_ns = 50;
		break;
	}

	switch (intel_dsi->lane_count) {
	case 1:
	case 2:
		extra_byte_count = 2;
		break;
	case 3:
		extra_byte_count = 4;
		break;
	case 4:
	default:
		extra_byte_count = 3;
		break;
	}

	/*
	 * ui(s) = 1/f [f in hz]
	 * ui(ns) = 10^9 / (f*10^6) [f in Mhz] -> 10^3/f(Mhz)
	 */

	/* in Kbps */
	ui_num = NS_KHZ_RATIO;
	ui_den = bitrate;

	tclk_prepare_clkzero = mipi_config->tclk_prepare_clkzero;
	ths_prepare_hszero = mipi_config->ths_prepare_hszero;

	/*
	 * B060
	 * LP byte clock = TLPX/ (8UI)
	 */
	intel_dsi->lp_byte_clk = DIV_ROUND_UP(tlpx_ns * ui_den, 8 * ui_num);

	/* count values in UI = (ns value) * (bitrate / (2 * 10^6))
	 *
	 * Since txddrclkhs_i is 2xUI, all the count values programmed in
	 * DPHY param register are divided by 2
	 *
	 * prepare count
	 */
	ths_prepare_ns = max(mipi_config->ths_prepare,
			     mipi_config->tclk_prepare);
	prepare_cnt = DIV_ROUND_UP(ths_prepare_ns * ui_den, ui_num * 2);

	/* exit zero count */
	exit_zero_cnt = DIV_ROUND_UP(
				(ths_prepare_hszero - ths_prepare_ns) * ui_den,
				ui_num * 2
				);

	/*
	 * Exit zero  is unified val ths_zero and ths_exit
	 * minimum value for ths_exit = 110ns
	 * min (exit_zero_cnt * 2) = 110/UI
	 * exit_zero_cnt = 55/UI
	 */
	 if (exit_zero_cnt < (55 * ui_den / ui_num))
		if ((55 * ui_den) % ui_num)
			exit_zero_cnt += 1;

	/* clk zero count */
	clk_zero_cnt = DIV_ROUND_UP(
			(tclk_prepare_clkzero -	ths_prepare_ns)
			* ui_den, 2 * ui_num);

	/* trail count */
	tclk_trail_ns = max(mipi_config->tclk_trail, mipi_config->ths_trail);
	trail_cnt = DIV_ROUND_UP(tclk_trail_ns * ui_den, 2 * ui_num);

	if (prepare_cnt > PREPARE_CNT_MAX ||
		exit_zero_cnt > EXIT_ZERO_CNT_MAX ||
		clk_zero_cnt > CLK_ZERO_CNT_MAX ||
		trail_cnt > TRAIL_CNT_MAX)
		pr_debug("Values crossing maximum limits, restricting to max values\n");

	if (prepare_cnt > PREPARE_CNT_MAX)
		prepare_cnt = PREPARE_CNT_MAX;

	if (exit_zero_cnt > EXIT_ZERO_CNT_MAX)
		exit_zero_cnt = EXIT_ZERO_CNT_MAX;

	if (clk_zero_cnt > CLK_ZERO_CNT_MAX)
		clk_zero_cnt = CLK_ZERO_CNT_MAX;

	if (trail_cnt > TRAIL_CNT_MAX)
		trail_cnt = TRAIL_CNT_MAX;

	/* B080 */
	intel_dsi->dphy_reg = exit_zero_cnt << 24 | trail_cnt << 16 |
						clk_zero_cnt << 8 | prepare_cnt;

	/*
	 * LP to HS switch count = 4TLPX + PREP_COUNT * 2 + EXIT_ZERO_COUNT * 2
	 *					+ 10UI + Extra Byte Count
	 *
	 * HS to LP switch count = THS-TRAIL + 2TLPX + Extra Byte Count
	 * Extra Byte Count is calculated according to number of lanes.
	 * High Low Switch Count is the Max of LP to HS and
	 * HS to LP switch count
	 *
	 */
	tlpx_ui = DIV_ROUND_UP(tlpx_ns * ui_den, ui_num);

	/* B044 */
	/* FIXME:
	 * The comment above does not match with the code */
	lp_to_hs_switch = DIV_ROUND_UP(4 * tlpx_ui + prepare_cnt * 2 +
						exit_zero_cnt * 2 + 10, 8);

	hs_to_lp_switch = DIV_ROUND_UP(mipi_config->ths_trail + 2 * tlpx_ui, 8);

	intel_dsi->hs_to_lp_count = max(lp_to_hs_switch, hs_to_lp_switch);
	intel_dsi->hs_to_lp_count += extra_byte_count;

	/* B088 */
	/* LP -> HS for clock lanes
	 * LP clk sync + LP11 + LP01 + tclk_prepare + tclk_zero +
	 *						extra byte count
	 * 2TPLX + 1TLPX + 1 TPLX(in ns) + prepare_cnt * 2 + clk_zero_cnt *
	 *					2(in UI) + extra byte count
	 * In byteclks = (4TLPX + prepare_cnt * 2 + clk_zero_cnt *2 (in UI)) /
	 *					8 + extra byte count
	 */
	intel_dsi->clk_lp_to_hs_count =
		DIV_ROUND_UP(
			4 * tlpx_ui + prepare_cnt * 2 +
			clk_zero_cnt * 2,
			8);

	intel_dsi->clk_lp_to_hs_count += extra_byte_count;

	/* HS->LP for Clock Lanes
	 * Low Power clock synchronisations + 1Tx byteclk + tclk_trail +
	 *						Extra byte count
	 * 2TLPX + 8UI + (trail_count*2)(in UI) + Extra byte count
	 * In byteclks = (2*TLpx(in UI) + trail_count*2 +8)(in UI)/8 +
	 *						Extra byte count
	 */
	intel_dsi->clk_hs_to_lp_count =
		DIV_ROUND_UP(2 * tlpx_ui + trail_cnt * 2 + 8,
			8);
	intel_dsi->clk_hs_to_lp_count += extra_byte_count;

	pr_info("ADF: %s: Eot %s\n", __func__,
		intel_dsi->eotp_pkt ? "enabled" : "disabled");
	pr_info("ADF: %s: Clockstop %s\n", __func__, intel_dsi->clock_stop ?
						"disabled" : "enabled");
	pr_info("ADF: %s: Mode %s\n", __func__,
		intel_dsi->operation_mode ? "command" : "video");

	if (intel_dsi->dual_link == DSI_DUAL_LINK_FRONT_BACK)
		pr_info("ADF %s: Dual link: DSI_DUAL_LINK_FRONT_BACK\n",
								__func__);
	else if (intel_dsi->dual_link == DSI_DUAL_LINK_PIXEL_ALTN)
		pr_info("ADF %s: Dual link: DSI_DUAL_LINK_PIXEL_ALTN\n",
								__func__);
	else
		pr_info("ADF %s: Dual link: NONE\n", __func__);

	pr_info("ADF: %s: Pixel Format %d\n", __func__,
		intel_dsi->pixel_format);
	pr_info("ADF: %s: TLPX %d\n", __func__, intel_dsi->escape_clk_div);
	pr_info("ADF: %s: LP RX Timeout 0x%x\n",
		__func__, intel_dsi->lp_rx_timeout);
	pr_info("ADF: %s: Turnaround Timeout 0x%x\n",
		__func__, intel_dsi->turn_arnd_val);
	pr_info("ADF: %s: Init Count 0x%x\n", __func__, intel_dsi->init_count);
	pr_info("ADF: %s: HS to LP Count 0x%x\n",
		__func__, intel_dsi->hs_to_lp_count);
	pr_info("ADF: %s: LP Byte Clock %d\n",
		__func__, intel_dsi->lp_byte_clk);
	pr_info("ADF: %s: DBI BW Timer 0x%x\n", __func__, intel_dsi->bw_timer);
	pr_info("ADF: %s: LP to HS Clock Count 0x%x\n",
		__func__, intel_dsi->clk_lp_to_hs_count);
	pr_info("ADF: %s: HS to LP Clock Count 0x%x\n",
		__func__, intel_dsi->clk_hs_to_lp_count);
	pr_info("ADF: %s: BTA %s\n", __func__,
			intel_dsi->video_frmt_cfg_bits & DISABLE_VIDEO_BTA ?
			"disabled" : "enabled");

	/* delays in VBT are in unit of 100us, so need to convert
	 * here in ms
	 * Delay (100us) * 100 /1000 = Delay / 10 (ms) */
	intel_dsi->backlight_off_delay = pps->bl_disable_delay / 10;
	intel_dsi->backlight_on_delay = pps->bl_enable_delay / 10;
	intel_dsi->panel_on_delay = pps->panel_on_delay / 10;
	intel_dsi->panel_off_delay = pps->panel_off_delay / 10;
	intel_dsi->panel_pwr_cycle_delay = pps->panel_power_cycle_delay / 10;

	return 0;
}

#if 0
static int panel_generic_mode_valid(struct intel_dsi_device *dsi,
		   struct drm_display_mode *mode)
{
	return MODE_OK;
}

static bool panel_generic_mode_fixup(struct intel_dsi_device *dsi,
		    const struct drm_display_mode *mode,
		    struct drm_display_mode *adjusted_mode) {
	return true;
}
#endif

static int panel_generic_panel_reset(struct dsi_pipe *interface)
{
	struct dsi_vbt *dsi = interface->config.dsi;
	char *sequence = dsi->sequence[MIPI_SEQ_ASSERT_RESET];
	pr_debug("ADF: %s\n", __func__);

	panel_generic_exec_sequence(interface, sequence);

	return 0;
}

static int panel_generic_disable_panel_power(struct dsi_pipe *interface)
{
	struct dsi_vbt *dsi = interface->config.dsi;
	char *sequence = dsi->sequence[MIPI_SEQ_DEASSERT_RESET];
	pr_debug("ADF: %s\n", __func__);

	panel_generic_exec_sequence(interface, sequence);
	return 0;
}

static int panel_generic_send_otp_cmds(struct dsi_pipe *interface)
{
	struct dsi_vbt *dsi = interface->config.dsi;
	char *sequence = dsi->sequence[MIPI_SEQ_INIT_OTP];
	pr_debug("ADF: %s\n", __func__);

	panel_generic_exec_sequence(interface, sequence);

	return 0;
}

static int panel_generic_enable(struct dsi_pipe *interface)
{
	struct dsi_vbt *dsi = interface->config.dsi;
	char *sequence = dsi->sequence[MIPI_SEQ_DISPLAY_ON];
	pr_debug("ADF: %s\n", __func__);

	panel_generic_exec_sequence(interface, sequence);

	return 0;
}

static int panel_generic_disable(struct dsi_pipe *interface)
{
	struct dsi_vbt *dsi = interface->config.dsi;
	char *sequence = dsi->sequence[MIPI_SEQ_DISPLAY_OFF];
	pr_debug("ADF: %s\n", __func__);

	panel_generic_exec_sequence(interface, sequence);

	return 0;
}

int panel_generic_enable_bklt(struct dsi_pipe *interface)
{
	struct dsi_vbt *dsi = interface->config.dsi;
	char *sequence = dsi->sequence[MIPI_SEQ_BACKLIGHT_ON];
	pr_debug("ADF: %s\n", __func__);

	panel_generic_exec_sequence(interface, sequence);
	return 0;
}

int panel_generic_disable_bklt(struct dsi_pipe *interface)
{
	struct dsi_vbt *dsi = interface->config.dsi;
	char *sequence = dsi->sequence[MIPI_SEQ_BACKLIGHT_OFF];
	pr_debug("ADF: %s\n", __func__);

	panel_generic_exec_sequence(interface, sequence);
	return 0;
}

int panel_generic_power_on(struct dsi_pipe *interface)
{
	struct dsi_vbt *dsi = interface->config.dsi;
	char *sequence = dsi->sequence[MIPI_SEQ_PANEL_ON];
	pr_debug("ADF: %s\n", __func__);

	panel_generic_exec_sequence(interface, sequence);
	return 0;
}

int panel_generic_power_off(struct dsi_pipe *interface)
{
	struct dsi_vbt *dsi = interface->config.dsi;
	char *sequence = dsi->sequence[MIPI_SEQ_PANEL_OFF];
	pr_debug("ADF: %s\n", __func__);

	panel_generic_exec_sequence(interface, sequence);
	return 0;
}

static int panel_generic_detect(struct dsi_pipe *interface)
{
	pr_debug("ADF: %s\n", __func__);
	return 1;
}

#if 0
static bool panel_generic_get_hw_state(struct intel_dsi_device *dev)
{
	return true;
}
#endif

static int panel_generic_get_modes(struct dsi_config *config,
			     struct drm_mode_modeinfo *modeinfo)
{
	struct drm_display_mode *mode = &config->vbt_mode;

	pr_debug("ADF: %s\n", __func__);

	modeinfo->clock = mode->clock;
	modeinfo->hdisplay = (u16) mode->hdisplay;
	modeinfo->hsync_start = (u16) mode->hsync_start;
	modeinfo->hsync_end = (u16) mode->hsync_end;
	modeinfo->htotal = (u16) mode->htotal;
	modeinfo->vdisplay = (u16) mode->vdisplay;
	modeinfo->vsync_start = (u16) mode->vsync_start;
	modeinfo->vsync_end = (u16) mode->vsync_end;
	modeinfo->vtotal = (u16) mode->vtotal;
	modeinfo->hskew = (u16) mode->hskew;
	modeinfo->vscan = (u16) mode->vscan;
	modeinfo->vrefresh = (u32) mode->vrefresh;
	modeinfo->flags = mode->flags;
	modeinfo->type |= mode->type | DRM_MODE_TYPE_PREFERRED;
	strncpy(modeinfo->name, mode->name, DRM_DISPLAY_MODE_LEN);

	return 0;
}

#if 0
static void panel_generic_destroy(struct intel_dsi_device *dsi) { }
#endif

int panel_generic_get_panel_info(struct dsi_config *config,
				 struct panel_info *info)
{
	struct drm_display_mode *mode = &config->vbt_mode;
	struct dsi_context *ctx = &config->ctx;
	int bpp = 24;

	pr_debug("ADF: %s\n", __func__);

	info->width_mm = mode->width_mm;
	info->height_mm = mode->height_mm;
	info->dsi_type = ctx->operation_mode;
	info->lane_num = ctx->lane_count;
	info->dual_link = ctx->dual_link;

	if (ctx->pixel_format == VID_MODE_FORMAT_RGB666)
		bpp = 18;
	else if (ctx->pixel_format == VID_MODE_FORMAT_RGB565)
		bpp = 16;

	info->bpp = bpp;

	return 0;
}

static int panel_generic_exit_standby(struct dsi_pipe *interface)
{
	pr_debug("ADF: %s\n", __func__);
	return 0;
}

static int panel_generic_set_brightness(struct dsi_pipe *interface, int level)
{
	pr_debug("ADF: %s\n", __func__);
	return 0;
}

static int panel_generic_set_mode(struct dsi_pipe *interface)
{
	pr_debug("ADF: %s\n", __func__);
	return 0;
}

struct panel_ops panel_generic_ops = {
		.get_config_mode = panel_generic_get_modes,
		.dsi_controller_init = panel_generic_init,
		.get_panel_info = panel_generic_get_panel_info,
		.reset = panel_generic_panel_reset,
		.exit_deep_standby = panel_generic_exit_standby,
		.detect = panel_generic_detect,
		.power_on = panel_generic_enable,
		.power_off = panel_generic_disable,
		.enable_backlight = panel_generic_enable_bklt,
		.disable_backlight = panel_generic_disable_bklt,
		.set_brightness = panel_generic_set_brightness,
		.drv_ic_init = panel_generic_send_otp_cmds,
		.drv_set_panel_mode = panel_generic_set_mode,
		.disable_panel_power = panel_generic_disable_panel_power,
		.panel_power_on = panel_generic_power_on,
		.panel_power_off = panel_generic_power_off,
/*
 * Might need to add these hooks in panel_ops
		.mode_valid = panel_generic_mode_valid,
		.mode_fixup = panel_generic_mode_fixup,
		.get_hw_state = panel_generic_get_hw_state,
		.destroy = panel_generic_destroy,
*/
};

struct dsi_panel generic_panel = {
	.panel_id = MIPI_DSI_GENERIC_PANEL_ID,
	.ops = &panel_generic_ops,
};

const struct dsi_panel *get_generic_panel(void)
{
	pr_debug("ADF: %s\n", __func__);
	return &generic_panel;
}

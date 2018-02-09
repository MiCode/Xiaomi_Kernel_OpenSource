/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2018 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2559.h
**
** Description:
**     definitions and data structures for TAS2559 Android Linux driver
**
** =============================================================================
*/

#ifndef _TAS2559_H
#define _TAS2559_H

#include <linux/regmap.h>

/* Page Control Register */
#define	TAS2559_PAGECTL_REG			0

/* Book Control Register (available in page0 of each book) */
#define	TAS2559_BOOKCTL_PAGE			0
#define	TAS2559_BOOKCTL_REG			127

#define TAS2559_REG(book, page, reg)		((((unsigned int)book * 256 * 128) + \
											((unsigned int)page * 128)) + reg)

#define	TAS2559_BOOK_ID(reg)			((unsigned char)(reg / (256 * 128)))
#define	TAS2559_PAGE_ID(reg)			((unsigned char)((reg % (256 * 128)) / 128))
#define	TAS2559_BOOK_REG(reg)			((unsigned char)(reg % (256 * 128)))
#define	TAS2559_PAGE_REG(reg)			((unsigned char)((reg % (256 * 128)) % 128))

/* Book0, Page0 registers */
#define	TAS2559_SW_RESET_REG			TAS2559_REG(0, 0, 1)

#define	TAS2559_REV_PGID_REG			TAS2559_REG(0, 0, 3)
#define	TAS2559_PG_VERSION_1P0			0x00
#define	TAS2559_PG_VERSION_2P0			0x10
#define	TAS2559_PG_VERSION_2P1			0x20

#define	TAS2559_POWER_CTRL1_REG			TAS2559_REG(0, 0, 4)
#define	TAS2559_POWER_CTRL2_REG			TAS2559_REG(0, 0, 5)

#define	TAS2559_SPK_CTRL_REG			TAS2559_REG(0, 0, 6)
/* B0P0R6 - TAS2559_SPK_CTRL_REG */
#define	TAS2559_DAC_GAIN_MASK			(0xf << 3)
#define	TAS2559_DAC_GAIN_SHIFT			0x03

#define	TAS2559_MUTE_REG			TAS2559_REG(0, 0, 7)
#define	TAS2559_SNS_CTRL_REG			TAS2559_REG(0, 0, 8)
#define	TAS2559_ADC_INPUT_SEL_REG		TAS2559_REG(0, 0, 9)
#define	TAS2559_DBOOST_CTL_REG			TAS2559_REG(0, 0, 10)
#define	TAS2559_NONAME11_REG			TAS2559_REG(0, 0, 11)
#define	TAS2559_NONAME12_REG			TAS2559_REG(0, 0, 12)
#define	TAS2559_NONAME13_REG			TAS2559_REG(0, 0, 13)
#define	TAS2559_NONAME14_REG			TAS2559_REG(0, 0, 14)
#define	TAS2559_NONAME15_REG			TAS2559_REG(0, 0, 15)
#define	TAS2559_NONAME16_REG			TAS2559_REG(0, 0, 16)
#define	TAS2559_NONAME17_REG			TAS2559_REG(0, 0, 17)
#define	TAS2559_NONAME18_REG			TAS2559_REG(0, 0, 18)
#define	TAS2559_SAR_SAMPLING_TIME_REG		TAS2559_REG(0, 0, 19)
#define	TAS2559_SAR_ADC1_REG			TAS2559_REG(0, 0, 20)
#define	TAS2559_SAR_ADC2_REG			TAS2559_REG(0, 0, 21)	/* B0_P0_R0x15 */
#define	TAS2559_CRC_CHECKSUM_REG		TAS2559_REG(0, 0, 32)	/* B0_P0_R0x20 */
#define	TAS2559_CRC_RESET_REG			TAS2559_REG(0, 0, 33)	/* B0_P0_R0x21 */
#define	TAS2559_DSP_MODE_SELECT_REG		TAS2559_REG(0, 0, 34)
#define	TAS2559_SAFE_GUARD_REG			TAS2559_REG(0, 0, 37)	/* B0_P0_R0x25 */
#define	TAS2559_ASI_CTRL_REG			TAS2559_REG(0, 0, 42)
#define	TAS2559_CLK_ERR_CTRL			TAS2559_REG(0, 0, 44)	/* B0_P0_R0x2c */
#define	TAS2559_CLK_ERR_CTRL2			TAS2559_REG(0, 0, 45)	/* B0_P0_R0x2d*/
#define	TAS2559_CLK_ERR_CTRL3			TAS2559_REG(0, 0, 46)	/* B0_P0_R0x2e*/
#define	TAS2559_DBOOST_CFG_REG			TAS2559_REG(0, 0, 52)
#define	TAS2559_POWER_UP_FLAG_REG		TAS2559_REG(0, 0, 100)	/* B0_P0_R0x64 */
#define	TAS2559_FLAGS_1				TAS2559_REG(0, 0, 104)	/* B0_P0_R0x68 */
#define	TAS2559_FLAGS_2				TAS2559_REG(0, 0, 108)	/* B0_P0_R0x6c */

/* Book0, Page1 registers */
#define	TAS2559_ASI1_DAC_FORMAT_REG		TAS2559_REG(0, 1, 1)
#define	TAS2559_ASI1_ADC_FORMAT_REG		TAS2559_REG(0, 1, 2)
#define	TAS2559_ASI1_OFFSET1_REG		TAS2559_REG(0, 1, 3)
#define	TAS2559_ASI1_ADC_PATH_REG		TAS2559_REG(0, 1, 7)
#define	TAS2559_ASI1_DAC_BCLK_REG		TAS2559_REG(0, 1, 8)
#define	TAS2559_ASI1_DAC_WCLK_REG		TAS2559_REG(0, 1, 9)
#define	TAS2559_ASI1_ADC_BCLK_REG		TAS2559_REG(0, 1, 10)
#define	TAS2559_ASI1_ADC_WCLK_REG		TAS2559_REG(0, 1, 11)
#define	TAS2559_ASI1_DIN_DOUT_MUX_REG		TAS2559_REG(0, 1, 12)
#define	TAS2559_ASI1_BDIV_CLK_SEL_REG		TAS2559_REG(0, 1, 13)
#define	TAS2559_ASI1_BDIV_CLK_RATIO_REG		TAS2559_REG(0, 1, 14)
#define	TAS2559_ASI1_WDIV_CLK_RATIO_REG		TAS2559_REG(0, 1, 15)
#define	TAS2559_ASI1_DAC_CLKOUT_REG		TAS2559_REG(0, 1, 16)
#define	TAS2559_ASI1_ADC_CLKOUT_REG		TAS2559_REG(0, 1, 17)

#define	TAS2559_ASI2_DAC_FORMAT_REG		TAS2559_REG(0, 1, 21)
#define	TAS2559_ASI2_ADC_FORMAT_REG		TAS2559_REG(0, 1, 22)
#define	TAS2559_ASI2_OFFSET1_REG		TAS2559_REG(0, 1, 23)
#define	TAS2559_ASI2_ADC_PATH_REG		TAS2559_REG(0, 1, 27)
#define	TAS2559_ASI2_DAC_BCLK_REG		TAS2559_REG(0, 1, 28)
#define	TAS2559_ASI2_DAC_WCLK_REG		TAS2559_REG(0, 1, 29)
#define	TAS2559_ASI2_ADC_BCLK_REG		TAS2559_REG(0, 1, 30)
#define	TAS2559_ASI2_ADC_WCLK_REG		TAS2559_REG(0, 1, 31)
#define	TAS2559_ASI2_DIN_DOUT_MUX_REG		TAS2559_REG(0, 1, 32)
#define	TAS2559_ASI2_BDIV_CLK_SEL_REG		TAS2559_REG(0, 1, 33)
#define	TAS2559_ASI2_BDIV_CLK_RATIO_REG		TAS2559_REG(0, 1, 34)
#define	TAS2559_ASI2_WDIV_CLK_RATIO_REG		TAS2559_REG(0, 1, 35)
#define	TAS2559_ASI2_DAC_CLKOUT_REG		TAS2559_REG(0, 1, 36)
#define	TAS2559_ASI2_ADC_CLKOUT_REG		TAS2559_REG(0, 1, 37)

#define	TAS2559_GPIO1_PIN_REG			TAS2559_REG(0, 1, 61)	/* B0_P1_R0x3d */
#define	TAS2559_GPIO2_PIN_REG			TAS2559_REG(0, 1, 62)	/* B0_P1_R0x3e */
#define	TAS2559_GPIO3_PIN_REG			TAS2559_REG(0, 1, 63)
#define	TAS2559_GPIO4_PIN_REG			TAS2559_REG(0, 1, 64)
#define	TAS2559_GPIO5_PIN_REG			TAS2559_REG(0, 1, 65)
#define	TAS2559_GPIO6_PIN_REG			TAS2559_REG(0, 1, 66)
#define	TAS2559_GPIO7_PIN_REG			TAS2559_REG(0, 1, 67)
#define	TAS2559_GPIO8_PIN_REG			TAS2559_REG(0, 1, 68)
#define	TAS2559_GPIO9_PIN_REG			TAS2559_REG(0, 1, 69)
#define	TAS2559_GPIO10_PIN_REG			TAS2559_REG(0, 1, 70)

#define	TAS2559_GPI_PIN_REG				TAS2559_REG(0, 1, 77)		/* B0_P1_R0x4d */
#define	TAS2559_GPIO_HIZ_CTRL1_REG		TAS2559_REG(0, 1, 79)
#define	TAS2559_GPIO_HIZ_CTRL2_REG		TAS2559_REG(0, 1, 80)		/* B0_P1_R0x50 */
#define	TAS2559_GPIO_HIZ_CTRL3_REG		TAS2559_REG(0, 1, 81)
#define	TAS2559_GPIO_HIZ_CTRL4_REG		TAS2559_REG(0, 1, 82)
#define	TAS2559_GPIO_HIZ_CTRL5_REG		TAS2559_REG(0, 1, 83)

#define	TAS2559_BIT_BANG_CTRL_REG		TAS2559_REG(0, 1, 87)
#define	TAS2559_BIT_BANG_OUT1_REG		TAS2559_REG(0, 1, 88)
#define	TAS2559_BIT_BANG_OUT2_REG		TAS2559_REG(0, 1, 89)
#define	TAS2559_BIT_BANG_IN1_REG		TAS2559_REG(0, 1, 90)
#define	TAS2559_BIT_BANG_IN2_REG		TAS2559_REG(0, 1, 91)
#define	TAS2559_BIT_BANG_IN3_REG		TAS2559_REG(0, 1, 92)

#define	TAS2559_PDM_IN_CLK_REG			TAS2559_REG(0, 1, 94)
#define	TAS2559_PDM_IN_PIN_REG			TAS2559_REG(0, 1, 95)

#define	TAS2559_ASIM_IFACE1_REG			TAS2559_REG(0, 1, 98)
#define	TAS2559_ASIM_FORMAT_REG			TAS2559_REG(0, 1, 99)
#define	TAS2559_ASIM_IFACE3_REG			TAS2559_REG(0, 1, 100)
#define	TAS2559_ASIM_IFACE4_REG			TAS2559_REG(0, 1, 101)
#define	TAS2559_ASIM_IFACE5_REG			TAS2559_REG(0, 1, 102)
#define	TAS2559_ASIM_IFACE6_REG			TAS2559_REG(0, 1, 103)
#define	TAS2559_ASIM_IFACE7_REG			TAS2559_REG(0, 1, 104)
#define	TAS2559_ASIM_IFACE8_REG			TAS2559_REG(0, 1, 105)
#define	TAS2559_CLK_HALT_REG			TAS2559_REG(0, 1, 106)	/* B0_P1_R0x6a */
#define	TAS2559_INT_GEN1_REG			TAS2559_REG(0, 1, 108)	/* B0_P1_R0x6c */
#define	TAS2559_INT_GEN2_REG			TAS2559_REG(0, 1, 109)	/* B0_P1_R0x6d */
#define	TAS2559_INT_GEN3_REG			TAS2559_REG(0, 1, 110)	/* B0_P1_R0x6e */
#define	TAS2559_INT_GEN4_REG			TAS2559_REG(0, 1, 111)	/* B0_P1_R0x6f */
#define TAS2559_INT_MODE_REG			TAS2559_REG(0, 1, 114)	/* B0_P1_R0x72 */
#define	TAS2559_MAIN_CLKIN_REG			TAS2559_REG(0, 1, 115)
#define	TAS2559_PLL_CLKIN_REG			TAS2559_REG(0, 1, 116)
#define	TAS2559_CLKOUT_MUX_REG			TAS2559_REG(0, 1, 117)
#define	TAS2559_CLKOUT_CDIV_REG			TAS2559_REG(0, 1, 118)
#define	TAS2559_HACK_GP01_REG			TAS2559_REG(0, 1, 122)

#define	TAS2559_SLEEPMODE_CTL_REG		TAS2559_REG(0, 2, 7)
#define	TAS2559_HACK01_REG			TAS2559_REG(0, 2, 10)

#define	TAS2559_ISENSE_THRESHOLD		TAS2559_REG(0, 50, 104)
#define	TAS2559_BOOSTON_EFFICIENCY		TAS2559_REG(0, 51, 16)
#define	TAS2559_BOOSTOFF_EFFICIENCY		TAS2559_REG(0, 51, 20)
#define	TAS2559_BOOST_HEADROOM			TAS2559_REG(0, 51, 24)
#define	TAS2559_THERMAL_FOLDBACK_REG	TAS2559_REG(0, 51, 100)

#define TAS2559_SA_CHL_CTRL_REG			TAS2559_REG(0, 53, 20)	/* B0_P0x35_R0x14 */
#define	TAS2559_VPRED_COMP_REG			TAS2559_REG(0, 53, 24)
#define	TAS2559_SA_COEFF_SWAP_REG		TAS2559_REG(0, 53, 44)	/* B0_P0x35_R0x2c */

#define	TAS2559_TEST_MODE_REG			TAS2559_REG(0, 253, 13)
#define	TAS2559_BROADCAST_REG			TAS2559_REG(0, 253, 54)
#define	TAS2559_VBST_VOLT_REG			TAS2559_REG(0, 253, 58)/* B0_P0xfd_R0x3a */
#define	TAS2559_CRYPTIC_REG			TAS2559_REG(0, 253, 71)

#define	TAS2559_DAC_INTERPOL_REG		TAS2559_REG(100, 0, 1)
#define	TAS2559_SOFT_MUTE_REG			TAS2559_REG(100, 0, 7)
#define	TAS2559_PLL_P_VAL_REG			TAS2559_REG(100, 0, 27)
#define	TAS2559_PLL_J_VAL_REG			TAS2559_REG(100, 0, 28)
#define	TAS2559_PLL_D_VAL_MSB_REG		TAS2559_REG(100, 0, 29)
#define	TAS2559_PLL_D_VAL_LSB_REG		TAS2559_REG(100, 0, 30)
#define	TAS2559_CLK_MISC_REG			TAS2559_REG(100, 0, 31)
#define	TAS2559_PLL_N_VAL_REG			TAS2559_REG(100, 0, 32)
#define	TAS2559_DAC_MADC_VAL_REG		TAS2559_REG(100, 0, 33)
#define	TAS2559_ISENSE_DIV_REG			TAS2559_REG(100, 0, 42)
#define	TAS2559_RAMP_CLK_DIV_MSB_REG	TAS2559_REG(100, 0, 43)
#define	TAS2559_RAMP_CLK_DIV_LSB_REG	TAS2559_REG(100, 0, 44)
#define	TAS2559_VBOOST_CTL_REG			TAS2559_REG(100, 0, 64)	/* B0x64_P0x00_R0x40 */

#define	TAS2559_DIE_TEMP_REG			TAS2559_REG(130, 2, 124)	/* B0x82_P0x02_R0x7C */
#define	TAS2559_DEVA_CALI_R0_REG		TAS2559_REG(140, 47, 40)	/* B0x8c_P0x2f_R0x28 */
#define	TAS2559_DEVB_CALI_R0_REG		TAS2559_REG(140, 54, 12)	/* B0x8c_P0x36_R0x0c */

/* Bits */
/* B0P0R4 - TAS2559_POWER_CTRL1_REG */
#define	TAS2559_SW_SHUTDOWN			(0x1 << 0)
#define	TAS2559_MADC_POWER_UP			(0x1 << 3)
#define	TAS2559_MDAC_POWER_UP			(0x1 << 4)
#define	TAS2559_NDIV_POWER_UP			(0x1 << 5)
#define	TAS2559_PLL_POWER_UP			(0x1 << 6)
#define	TAS2559_DSP_POWER_UP			(0x1 << 7)

/* B0P0R5 - TAS2559_POWER_CTRL2_REG */
#define	TAS2559_VSENSE_ENABLE			(0x1 << 0)
#define	TAS2559_ISENSE_ENABLE			(0x1 << 1)
#define	TAS2559_BOOST_ENABLE			(0x1 << 5)
#define	TAS2559_CLASSD_ENABLE			(0x1 << 7)

/* B0P0R7 - TAS2559_MUTE_REG */
#define	TAS2559_CLASSD_MUTE			(0x1 << 0)
#define	TAS2559_ISENSE_MUTE			(0x1 << 1)

/* B0P253R13 - TAS2559_TEST_MODE_REG */
#define	TAS2559_TEST_MODE_ENABLE		(13)
#define	TAS2559_TEST_MODE_MASK			(0xf << 0)

/* B0P253R71 - TAS2559_CRYPTIC_REG */
#define	TAS2559_OSC_TRIM_CAP(x)			((x & 0x3f) << 0)
#define	TAS2559_DISABLE_ENCRYPTION		(0x1 << 6)
#define	TAS2559_SL_COMP				(0x1 << 7)

/* B0P1R115/6 - TAS2559_MAIN/PLL_CLKIN_REG */
#define	TAS2559_XXX_CLKIN_GPIO1			(0)
#define	TAS2559_XXX_CLKIN_GPIO2			(1)
#define	TAS2559_XXX_CLKIN_GPIO3			(2)
#define	TAS2559_XXX_CLKIN_GPIO4			(3)
#define	TAS2559_XXX_CLKIN_GPIO5			(4)
#define	TAS2559_XXX_CLKIN_GPIO6			(5)
#define	TAS2559_XXX_CLKIN_GPIO7			(6)
#define	TAS2559_XXX_CLKIN_GPIO8			(7)
#define	TAS2559_XXX_CLKIN_GPIO9			(8)
#define	TAS2559_XXX_CLKIN_GPIO10		(9)
#define	TAS2559_XXX_CLKIN_GPI1			(12)
#define	TAS2559_XXX_CLKIN_GPI2			(13)
#define	TAS2559_XXX_CLKIN_GPI3			(14)
#define	TAS2559_NDIV_CLKIN_PLL			(15)
#define	TAS2559_PLL_CLKIN_INT_OSC		(15)

#define	TAS2559_MCLK_CLKIN_SRC_GPIO1       (0)
#define	TAS2559_MCLK_CLKIN_SRC_GPIO2       (1)
#define	TAS2559_MCLK_CLKIN_SRC_GPIO3       (2)
#define	TAS2559_MCLK_CLKIN_SRC_GPIO4       (3)
#define	TAS2559_MCLK_CLKIN_SRC_GPIO5       (4)
#define	TAS2559_MCLK_CLKIN_SRC_GPIO6       (5)
#define	TAS2559_MCLK_CLKIN_SRC_GPIO7       (6)
#define	TAS2559_MCLK_CLKIN_SRC_GPIO8       (7)
#define	TAS2559_MCLK_CLKIN_SRC_GPIO9       (8)
#define	TAS2559_MCLK_CLKIN_SRC_GPIO10      (9)
#define	TAS2559_MCLK_CLKIN_SRC_GPI1        (12)
#define	TAS2559_MCLK_CLKIN_SRC_GPI2        (13)
#define	TAS2559_MCLK_CLKIN_SRC_GPI3        (14)

#define	TAS2559_FORMAT_I2S			(0x0 << 5)
#define	TAS2559_FORMAT_DSP			(0x1 << 5)
#define	TAS2559_FORMAT_RIGHT_J			(0x2 << 5)
#define	TAS2559_FORMAT_LEFT_J			(0x3 << 5)
#define	TAS2559_FORMAT_MONO_PCM			(0x4 << 5)
#define	TAS2559_FORMAT_MASK			(0x7 << 5)

#define	TAS2559_WORDLENGTH_16BIT		(0x0 << 3)
#define	TAS2559_WORDLENGTH_20BIT		(0x1 << 3)
#define	TAS2559_WORDLENGTH_24BIT		(0x2 << 3)
#define	TAS2559_WORDLENGTH_32BIT		(0x3 << 3)
#define	TAS2559_WORDLENGTH_MASK			TAS2559_WORDLENGTH_32BIT

/* B100P0R7 - TAS2559_SOFT_MUTE_REG */
#define	TAS2559_PDM_SOFT_MUTE			(0x1 << 0)
#define	TAS2559_VSENSE_SOFT_MUTE		(0x1 << 1)
#define	TAS2559_ISENSE_SOFT_MUTE		(0x1 << 2)
#define	TAS2559_CLASSD_SOFT_MUTE		(0x1 << 3)

/* B100P0R27 - TAS2559_PLL_P_VAL_REG */
#define	TAS2559_PLL_P_VAL_MASK			(0x3f << 0)

/* B100P0R28 - TAS2559_PLL_J_VAL_REG */
#define	TAS2559_PLL_J_VAL_MASK			((unsigned int) (0x7f << 0))
#define	TAS2559_PLL_J_VAL_MASKX	0x00

/* B100P0R29-30 - TAS2559_PLL_D_VAL_MSB/LSB_REG */
#define	TAS2559_PLL_D_MSB_VAL(x)		((x >> 8) & 0x3f)
#define	TAS2559_PLL_D_LSB_VAL(x)		(x & 0xff)

/* B100P0R31 - TAS2559_CLK_MISC_REG */
#define	TAS2559_DSP_CLK_FROM_PLL		(0x1 << 5)

#define	TAS2559_FW_NAME     "tas2559_uCDSP.bin"
#define	TAS2559_S_FW_NAME   "tas2559_s_uCDSP.bin"

#define	CHANNEL_LEFT				(0)
#define	CHANNEL_RIGHT				(1)

#define	TAS2559_APP_ROM1MODE	0
#define	TAS2559_APP_ROM2MODE	1
#define	TAS2559_APP_TUNINGMODE	2
#define	TAS2559_APP_ROM1_96KHZ	3
#define	TAS2559_APP_ROM2_96KHZ	4
#define	TAS2559_APP_RAMMODE		5

#define	TAS2559_BOOST_OFF		0
#define	TAS2559_BOOST_DEVA		1
#define	TAS2559_BOOST_DEVB		2
#define	TAS2559_BOOST_BOTH		3

#define	TAS2559_AD_BD		0	/* DevA default, DevB default */
#define	TAS2559_AM_BM		1	/* DevA mute, DevB mute */
#define	TAS2559_AL_BR		2	/* DevA left channel, DevB right channel */
#define	TAS2559_AR_BL		3	/* DevA right channel, DevB left channel */
#define	TAS2559_AH_BH		4	/* DevA (L+R)/2, DevB (L+R)/2 */

#define	TAS2559_VBST_DEFAULT		0	/* firmware default */
#define	TAS2559_VBST_A_ON			1	/* DevA always 8.5V, DevB default */
#define	TAS2559_VBST_B_ON			2	/* DevA default, DevB always 8.5V */
#define	TAS2559_VBST_A_ON_B_ON		(TAS2559_VBST_A_ON | TAS2559_VBST_B_ON)	/* both DevA and DevB always 8.5V */
#define	TAS2559_VBST_NEED_DEFAULT	0xff	/* need default value */

#define	TAS2559_VBST_8P5V	0	/* coresponding PPG 0dB */
#define	TAS2559_VBST_8P1V	1	/* coresponding PPG -1dB */
#define	TAS2559_VBST_7P6V	2	/* coresponding PPG -2dB */
#define	TAS2559_VBST_6P6V	3	/* coresponding PPG -3dB */
#define	TAS2559_VBST_5P6V	4	/* coresponding PPG -4dB */

#define	ERROR_NONE			0x00000000
#define	ERROR_PLL_ABSENT	0x00000001
#define	ERROR_DEVA_I2C_COMM	0x00000002
#define	ERROR_DEVB_I2C_COMM	0x00000004
#define	ERROR_PRAM_CRCCHK	0x00000008
#define	ERROR_YRAM_CRCCHK	0x00000010
#define	ERROR_CLK_DET2		0x00000020
#define	ERROR_CLK_DET1		0x00000040
#define	ERROR_CLK_LOST		0x00000080
#define	ERROR_BROWNOUT		0x00000100
#define	ERROR_DIE_OVERTEMP	0x00000200
#define	ERROR_CLK_HALT		0x00000400
#define	ERROR_UNDER_VOLTAGE	0x00000800
#define	ERROR_OVER_CURRENT	0x00001000
#define	ERROR_CLASSD_PWR	0x00002000
#define	ERROR_SAFE_GUARD	0x00004000
#define	ERROR_FAILSAFE		0x40000000

#define	LOW_TEMPERATURE_GAIN	6
#define	LOW_TEMPERATURE_COUNTER	12

struct TBlock {
	unsigned int mnType;
	unsigned char mbPChkSumPresent;
	unsigned char mnPChkSum;
	unsigned char mbYChkSumPresent;
	unsigned char mnYChkSum;
	unsigned int mnCommands;
	unsigned char *mpData;
};

struct TData {
	char mpName[64];
	char *mpDescription;
	unsigned int mnBlocks;
	struct TBlock *mpBlocks;
};

struct TProgram {
	char mpName[64];
	char *mpDescription;
	unsigned char mnAppMode;
	unsigned short mnBoost;
	struct TData mData;
};

struct TPLL {
	char mpName[64];
	char *mpDescription;
	struct TBlock mBlock;
};

struct TConfiguration {
	char mpName[64];
	char *mpDescription;
	unsigned int mnDevices;
	unsigned int mnProgram;
	unsigned int mnPLL;
	unsigned int mnSamplingRate;
	struct TData mData;
};

struct TCalibration {
	char mpName[64];
	char *mpDescription;
	unsigned int mnProgram;
	unsigned int mnConfiguration;
	struct TData mData;
};

struct TFirmware {
	unsigned int mnFWSize;
	unsigned int mnChecksum;
	unsigned int mnPPCVersion;
	unsigned int mnFWVersion;
	unsigned int mnDriverVersion;
	unsigned int mnTimeStamp;
	char mpDDCName[64];
	char *mpDescription;
	unsigned int mnDeviceFamily;
	unsigned int mnDevice;
	unsigned int mnPLLs;
	struct TPLL *mpPLLs;
	unsigned int mnPrograms;
	struct TProgram *mpPrograms;
	unsigned int mnConfigurations;
	struct TConfiguration *mpConfigurations;
	unsigned int mnCalibrations;
	struct TCalibration *mpCalibrations;
};

struct tas2559_register {
	int book;
	int page;
	int reg;
};

enum channel {
	DevA = 0x01,
	DevB = 0x02,
	DevBoth = (DevA | DevB),
};

struct tas2559_priv {
	struct device *dev;
	struct regmap *mpRegmap;
	struct i2c_client *client;
	struct mutex dev_lock;
	struct TFirmware *mpFirmware;
	struct TFirmware *mpCalFirmware;
	unsigned int mnCurrentProgram;
	unsigned int mnCurrentSampleRate;
	unsigned int mnCurrentConfiguration;
	unsigned int mnNewConfiguration;
	unsigned int mnCurrentCalibration;
	enum channel mnCurrentChannel;
	unsigned int mnBitRate;
	bool mbTILoadActive;
	bool mbPowerUp;
	bool mbLoadConfigurationPrePowerUp;
	struct delayed_work irq_work;
	unsigned int mnEchoRef;
	bool mbYCRCEnable;
	bool mbIRQEnable;
	bool mbCalibrationLoaded;

	/* parameters for TAS2559 */
	int mnDevAPGID;
	int mnDevAGPIORST;
	int mnDevAGPIOIRQ;
	int mnDevAIRQ;
	unsigned char mnDevAAddr;
	unsigned char mnDevAChl;
	unsigned char mnDevACurrentBook;
	unsigned char mnDevACurrentPage;

	/* parameters for TAS2560 */
	int mnDevBPGID;
	int mnDevBGPIORST;
	int mnDevBGPIOIRQ;
	int mnDevBIRQ;
	unsigned char mnDevBAddr;
	unsigned char mnDevBChl;
	unsigned char mnDevBLoad;
	unsigned char mnDevBCurrentBook;
	unsigned char mnDevBCurrentPage;

	int (*read)(struct tas2559_priv *pTAS2559,
		    enum channel chn, unsigned int reg, unsigned int *pValue);
	int (*write)(struct tas2559_priv *pTAS2559,
		     enum channel chn, unsigned int reg, unsigned int Value);
	int (*bulk_read)(struct tas2559_priv *pTAS2559,
			 enum channel chn, unsigned int reg, unsigned char *pData, unsigned int len);
	int (*bulk_write)(struct tas2559_priv *pTAS2559,
			  enum channel chn, unsigned int reg, unsigned char *pData, unsigned int len);
	int (*update_bits)(struct tas2559_priv *pTAS2559,
			   enum channel chn, unsigned int reg, unsigned int mask, unsigned int value);
	int (*set_config)(struct tas2559_priv *pTAS2559, int config);
	int (*set_calibration)(struct tas2559_priv *pTAS2559, int calibration);
	void (*clearIRQ)(struct tas2559_priv *pTAS2559);
	void (*enableIRQ)(struct tas2559_priv *pTAS2559, enum channel chl, bool enable);
	void (*hw_reset)(struct tas2559_priv *pTAS2559);
	/* device is working, but system is suspended */
	int (*runtime_suspend)(struct tas2559_priv *pTAS2559);
	int (*runtime_resume)(struct tas2559_priv *pTAS2559);

	unsigned int mnVBoostState;
	bool mbLoadVBoostPrePowerUp;
	unsigned int mnVBoostVoltage;
	unsigned int mnVBoostNewState;
	unsigned int mnVBoostDefaultCfg[6];

	/* for low temperature check */
	unsigned int mnDevGain;
	unsigned int mnDevCurrentGain;
	unsigned int mnDieTvReadCounter;
	struct hrtimer mtimer;
	struct work_struct mtimerwork;

	unsigned int mnChannelState;
	unsigned char mnDefaultChlData[16];

	/* device is working, but system is suspended */
	bool mbRuntimeSuspend;

	unsigned int mnErrCode;

	unsigned int mnRestart;
#ifdef CONFIG_TAS2559_CODEC
	struct mutex codec_lock;
#endif

#ifdef CONFIG_TAS2559_MISC
	int mnDBGCmd;
	int mnCurrentReg;
	struct mutex file_lock;
#endif

};

#endif /* _TAS2559_H */

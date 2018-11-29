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
**     tas2560.h
**
** Description:
**     definitions and data structures for TAS2559 Android Linux driver
**
** =============================================================================
*/

#ifndef _TAS2560_H
#define _TAS2560_H

/* Page Control Register */
#define	TAS2560_PAGECTL_REG			0

/* Book Control Register (available in page0 of each book) */
#define	TAS2560_BOOKCTL_PAGE		0
#define	TAS2560_BOOKCTL_REG			127

#define	TAS2560_REG(book, page, reg)		(((book * 256 * 128) + \
						 (page * 128)) + reg)

#define	TAS2560_BOOK_ID(reg)		(reg / (256 * 128))
#define	TAS2560_PAGE_ID(reg)		((reg % (256 * 128)) / 128)
#define	TAS2560_BOOK_REG(reg)		(reg % (256 * 128))
#define	TAS2560_PAGE_REG(reg)		((reg % (256 * 128)) % 128)

/* Book0, Page0 registers */
#define	TAS2560_SW_RESET_REG		TAS2560_REG(0, 0, 1)
#define	TAS2560_DEV_MODE_REG		TAS2560_REG(0, 0, 2)
#define	TAS2560_SPK_CTRL_REG		TAS2560_REG(0, 0, 4)
#define	TAS2560_MUTE_REG			TAS2560_REG(0, 0, 7)
#define	TAS2560_PWR_REG				TAS2560_REG(0, 0, 7)
#define	TAS2560_PWR_BIT_MASK		(0x3 << 6)
#define	TAS2560_MUTE_MASK			(0x7)

#define	TAS2560_SR_CTRL1			TAS2560_REG(0, 0, 8)
#define	TAS2560_LOAD				TAS2560_REG(0, 0, 9)
#define	TAS2560_SR_CTRL2			TAS2560_REG(0, 0, 13)	/*B0_P0_R0x0d*/
#define	TAS2560_SR_CTRL3			TAS2560_REG(0, 0, 14)	/*B0_P0_R0x0e*/

#define	TAS2560_CLK_SEL				TAS2560_REG(0, 0, 15)
#define	TAS2560_PLL_SRC_MASK		(0xc0)
#define	TAS2560_PLL_CLKIN_BCLK		(0)
#define	TAS2560_PLL_CLKIN_MCLK		(1)
#define	TAS2560_PLL_CLKIN_PDMCLK	(2)
#define	TAS2560_PLL_P_MASK			(0x3f)

#define	TAS2560_SET_FREQ			TAS2560_REG(0, 0, 16)
#define	TAS2560_PLL_J_MASK			(0x7f)

#define	TAS2560_PLL_D_MSB			TAS2560_REG(0, 0, 17)	/*B0_P0_R0x11*/
#define	TAS2560_PLL_D_LSB			TAS2560_REG(0, 0, 18)	/*B0_P0_R0x12*/

#define	TAS2560_DAI_FMT				TAS2560_REG(0, 0, 20)	/* B0_P0_R0x14 */
#define	TAS2560_ASI_CTRL			TAS2560_REG(0, 0, 21)	/* B0_P0_R0x15 */
#define	TAS2560_ASI_OFFSET_1		TAS2560_REG(0, 0, 22)	/*B0_P0_R0x16*/
#define	TAS2560_ASI_CFG_1			TAS2560_REG(0, 0, 24)	/* B0_P0_R0x18 */
#define	TAS2560_DIRINV_MASK			0x3c
#define	TAS2560_BCLKINV				(1 << 2)
#define	TAS2560_WCLKINV				(1 << 3)
#define	TAS2560_BCLKDIR				(1 << 4)
#define	TAS2560_WCLKDIR				(1 << 5)

#define	TAS2560_CLK_ERR_CTRL		TAS2560_REG(0, 0, 33)	/* B0_P0_R0x21 */
#define	TAS2560_IRQ_PIN_REG			TAS2560_REG(0, 0, 35)	/* B0_P0_R0x23 */
#define	TAS2560_INT_MODE_REG		TAS2560_REG(0, 0, 36)	/* B0_P0_R0x24 */
#define	TAS2560_INT_GEN_REG			TAS2560_REG(0, 0, 37)	/* B0_P0_R0x25 */
#define	TAS2560_FLAGS_1				TAS2560_REG(0, 0, 38)	/* B0_P0_R0x26 */
#define	TAS2560_FLAGS_2				TAS2560_REG(0, 0, 39)	/* B0_P0_R0x27 */
#define	TAS2560_POWER_UP_FLAG_REG	TAS2560_REG(0, 0, 42)	/* B0_P0_R0x2a */

#define	TAS2560_DR_BOOST_REG_2		TAS2560_REG(0, 0, 60)	/* B0_P0_R0x3c */
#define	TAS2560_DR_BOOST_REG_1		TAS2560_REG(0, 0, 73)	/* B0_P0_R0x49 */
#define	TAS2560_VBOOST_CTL_REG		TAS2560_REG(0, 0, 79)	/* B0_P0_R0x4f */
#define	TAS2560_CLK_ERR_CTRL2		TAS2560_REG(0, 0, 80)	/* B0_P0_R0x50 */
#define	TAS2560_SLEEPMODE_CTL_REG	TAS2560_REG(0, 0, 84)	/* B0_P0_R0x54 */
#define	TAS2560_ID_REG				TAS2560_REG(0, 0, 125)
#define	TAS2560_CRC_CHK_REG			TAS2560_REG(0, 0, 126)	/* B0_P0_R0x7e */

/* Book0, Page50 registers */
#define	TAS2560_HPF_CUTOFF_CTL1		TAS2560_REG(0, 50, 28)	/* B0_P0x32_R0x1c */
#define	TAS2560_HPF_CUTOFF_CTL2		TAS2560_REG(0, 50, 32)	/* B0_P0x32_R0x20 */
#define	TAS2560_HPF_CUTOFF_CTL3		TAS2560_REG(0, 50, 36)	/* B0_P0x32_R0x24 */

#define	TAS2560_ISENSE_PATH_CTL1	TAS2560_REG(0, 50, 40)	/* B0_P0x32_R0x28 */
#define	TAS2560_ISENSE_PATH_CTL2	TAS2560_REG(0, 50, 44)	/* B0_P0x32_R0x2c */
#define	TAS2560_ISENSE_PATH_CTL3	TAS2560_REG(0, 50, 48)	/* B0_P0x32_R0x30 */

#define	TAS2560_VLIMIT_THRESHOLD	TAS2560_REG(0, 50, 60)
#define TAS2560_IDLE_CHNL_DETECT	TAS2560_REG(0, 50, 108)	/* B0_P0x32_R0x6c */

/* Book0, Page51 registers */
#define	TAS2560_BOOST_HEAD			TAS2560_REG(0, 51, 24)	/* B0_P0x33_R0x18 */
#define	TAS2560_BOOST_ON			TAS2560_REG(0, 51, 16)	/* B0_P0x33_R0x10 */
#define	TAS2560_BOOST_OFF			TAS2560_REG(0, 51, 20)	/* B0_P0x33_R0x14 */
#define	TAS2560_BOOST_TABLE_CTRL1	TAS2560_REG(0, 51, 32)	/* B0_P0x33_R0x20 */
#define	TAS2560_BOOST_TABLE_CTRL2	TAS2560_REG(0, 51, 36)	/* B0_P0x33_R0x24 */
#define	TAS2560_BOOST_TABLE_CTRL3	TAS2560_REG(0, 51, 40)	/* B0_P0x33_R0x28 */
#define	TAS2560_BOOST_TABLE_CTRL4	TAS2560_REG(0, 51, 44)	/* B0_P0x33_R0x2c */
#define	TAS2560_BOOST_TABLE_CTRL5	TAS2560_REG(0, 51, 48)	/* B0_P0x33_R0x30 */
#define	TAS2560_BOOST_TABLE_CTRL6	TAS2560_REG(0, 51, 52)	/* B0_P0x33_R0x34 */
#define	TAS2560_BOOST_TABLE_CTRL7	TAS2560_REG(0, 51, 56)	/* B0_P0x33_R0x38 */
#define	TAS2560_BOOST_TABLE_CTRL8	TAS2560_REG(0, 51, 60)	/* B0_P0x33_R0x3c */
#define	TAS2560_THERMAL_FOLDBACK	TAS2560_REG(0, 51, 100)	/* B0_P0x33_R0x64 */

/* Book0, Page52 registers */
#define	TAS2560_VSENSE_DEL_CTL1		TAS2560_REG(0, 52, 52)	/* B0_P0x34_R0x34 */
#define	TAS2560_VSENSE_DEL_CTL2		TAS2560_REG(0, 52, 56)	/* B0_P0x34_R0x38 */
#define	TAS2560_VSENSE_DEL_CTL3		TAS2560_REG(0, 52, 60)	/* B0_P0x34_R0x3c */
#define	TAS2560_VSENSE_DEL_CTL4		TAS2560_REG(0, 52, 64)	/* B0_P0x34_R0x40 */
#define	TAS2560_VSENSE_DEL_CTL5		TAS2560_REG(0, 52, 68)	/* B0_P0x34_R0x44 */

#define	TAS2560_VBST_VOLT_REG		TAS2559_REG(0, 253, 54)	/* B0_P0xfd_R0x36 */

#define	TAS2560_DATAFORMAT_I2S		(0x0 << 2)
#define	TAS2560_DATAFORMAT_DSP		(0x1 << 2)
#define	TAS2560_DATAFORMAT_RIGHT_J	(0x2 << 2)
#define	TAS2560_DATAFORMAT_LEFT_J	(0x3 << 2)

#define	TAS2560_DAI_FMT_MASK		(0x7 << 2)
#define	LOAD_MASK					0x18

#define	TAS2560_YRAM_BOOK			0
#define	TAS2560_YRAM_START_PAGE		50
#define	TAS2560_YRAM_END_PAGE		52
#define	TAS2560_YRAM_START_REG		8
#define	TAS2560_YRAM_END_REG		127

#endif

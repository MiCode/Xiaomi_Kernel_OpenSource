/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2017 XiaoMi, Inc.
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
**     definitions and data structures for TAS2560 Android Linux driver
**
** =============================================================================
*/

#ifndef _TAS2560_H
#define _TAS2560_H

/* Page Control Register */
#define TAS2560_PAGECTL_REG			0

/* Book Control Register (available in page0 of each book) */
#define TAS2560_BOOKCTL_PAGE			0
#define TAS2560_BOOKCTL_REG			127

#define TAS2560_REG(book, page, reg)		(((book * 256 * 128) + \
						 (page * 128)) + reg)

#define TAS2560_BOOK_ID(reg)			(reg / (256 * 128))
#define TAS2560_PAGE_ID(reg)			((reg % (256 * 128)) / 128)
#define TAS2560_BOOK_REG(reg)			(reg % (256 * 128))
#define TAS2560_PAGE_REG(reg)			((reg % (256 * 128)) % 128)

/* Book0, Page0 registers */
#define TAS2560_SW_RESET_REG                    TAS2560_REG(0, 0, 1)
#define TAS2560_DEV_MODE_REG                    TAS2560_REG(0, 0, 2)
#define TAS2560_SPK_CTRL_REG                    TAS2560_REG(0, 0, 4)
#define TAS2560_MUTE_REG			TAS2560_REG(0, 0, 7)
#define TAS2560_PWR_REG				TAS2560_REG(0, 0, 7)
#define TAS2560_PWR_BIT_MASK			(0x3 << 6)
#define TAS2560_MUTE_MASK				(0x7)

#define TAS2560_SR_CTRL1			TAS2560_REG(0, 0, 8)
#define TAS2560_LOAD	                        TAS2560_REG(0, 0, 9)
#define TAS2560_SR_CTRL2			TAS2560_REG(0, 0, 13)
#define TAS2560_SR_CTRL3			TAS2560_REG(0, 0, 14)

#define TAS2560_CLK_SEL	                        TAS2560_REG(0, 0, 15)
#define TAS2560_PLL_SRC_MASK					(0xc0)
#define TAS2560_PLL_CLKIN_BCLK			(0)
#define TAS2560_PLL_CLKIN_MCLK			(1)
#define TAS2560_PLL_CLKIN_PDMCLK		(2)
#define TAS2560_PLL_P_MASK					(0x3f)

#define TAS2560_SET_FREQ			TAS2560_REG(0, 0, 16)
#define TAS2560_PLL_J_MASK			(0x7f)

#define TAS2560_PLL_D_LSB			TAS2560_REG(0, 0, 17)
#define TAS2560_PLL_D_MSB			TAS2560_REG(0, 0, 18)

#define TAS2560_DAI_FMT				TAS2560_REG(0, 0, 20)

#define TAS2560_ASI_CHANNEL			TAS2560_REG(0, 0, 21)

#define TAS2560_ASI_OFFSET_1			TAS2560_REG(0, 0, 22)

#define TAS2560_ASI_CFG_1			TAS2560_REG(0, 0, 24)
#define	TAS2560_CLK_ERR_CTRL		TAS2560_REG(0, 0, 33)	/* B0_P0_R0x21 */
#define	TAS2560_IRQ_PIN_REG			TAS2560_REG(0, 0, 35)	/* B0_P0_R0x23 */
#define	TAS2560_INT_MODE_REG		TAS2560_REG(0, 0, 36)	/* B0_P0_R0x24 */
#define	TAS2560_INT_GEN_REG			TAS2560_REG(0, 0, 37)	/* B0_P0_R0x25 */
#define	TAS2560_FLAGS_1				TAS2560_REG(0, 0, 38)	/* B0_P0_R0x26 */
#define	TAS2560_FLAGS_2				TAS2560_REG(0, 0, 39)	/* B0_P0_R0x27 */
#define	TAS2560_POWER_UP_FLAG_REG	TAS2560_REG(0, 0, 42)	/* B0_P0_R0x2a */

#define TAS2560_DR_BOOST_REG_2			TAS2560_REG(0, 0, 60)
#define TAS2560_DR_BOOST_REG_1			TAS2560_REG(0, 0, 73)
#define	TAS2560_ASICFG_MASK				0x3f
#define TAS2560_BUSKEEP					 1
#define TAS2560_TRISTATE				(1 << 1)
#define TAS2560_BCLKINV					(1 << 2)
#define TAS2560_WCLKINV					(1 << 3)
#define TAS2560_BCLKDIR					(1 << 4)
#define TAS2560_WCLKDIR					(1 << 5)

#define TAS2560_DR_BOOST_REG                    TAS2560_REG(0, 0, 73)
#define TAS2560_ID_REG				TAS2560_REG(0, 0, 125)

/* Book0, Page50 registers */
#define TAS2560_HPF_CUTOFF_CTL1			TAS2560_REG(0, 50, 28)
#define TAS2560_HPF_CUTOFF_CTL2			TAS2560_REG(0, 50, 32)
#define TAS2560_HPF_CUTOFF_CTL3			TAS2560_REG(0, 50, 36)

#define TAS2560_ISENSE_PATH_CTL1		TAS2560_REG(0, 50, 40)
#define TAS2560_ISENSE_PATH_CTL2		TAS2560_REG(0, 50, 44)
#define TAS2560_ISENSE_PATH_CTL3		TAS2560_REG(0, 50, 48)

#define TAS2560_VLIMIT_THRESHOLD		TAS2560_REG(0, 50, 60)

/* Book0, Page51 registers */
#define TAS2560_BOOST_HEAD			TAS2560_REG(0, 51, 24)
#define TAS2560_BOOST_ON			TAS2560_REG(0, 51, 16)
#define TAS2560_BOOST_OFF			TAS2560_REG(0, 51, 20)
#define TAS2560_BOOST_TABLE_CTRL1		TAS2560_REG(0, 51, 32)
#define TAS2560_BOOST_TABLE_CTRL2		TAS2560_REG(0, 51, 36)
#define TAS2560_BOOST_TABLE_CTRL3		TAS2560_REG(0, 51, 40)
#define TAS2560_BOOST_TABLE_CTRL4		TAS2560_REG(0, 51, 44)
#define TAS2560_BOOST_TABLE_CTRL5		TAS2560_REG(0, 51, 48)
#define TAS2560_BOOST_TABLE_CTRL6		TAS2560_REG(0, 51, 52)
#define TAS2560_BOOST_TABLE_CTRL7		TAS2560_REG(0, 51, 56)
#define TAS2560_BOOST_TABLE_CTRL8		TAS2560_REG(0, 51, 60)
#define TAS2560_THERMAL_FOLDBACK		TAS2560_REG(0, 51, 100)

/* Book0, Page52 registers */
#define TAS2560_VSENSE_DEL_CTL1			TAS2560_REG(0, 52, 52)
#define TAS2560_VSENSE_DEL_CTL2			TAS2560_REG(0, 52, 56)
#define TAS2560_VSENSE_DEL_CTL3			TAS2560_REG(0, 52, 60)
#define TAS2560_VSENSE_DEL_CTL4			TAS2560_REG(0, 52, 64)
#define TAS2560_VSENSE_DEL_CTL5			TAS2560_REG(0, 52, 68)


#define TAS2560_DATAFORMAT_I2S			(0x0 << 2)
#define TAS2560_DATAFORMAT_DSP			(0x1 << 2)
#define TAS2560_DATAFORMAT_RIGHT_J		(0x2 << 2)
#define TAS2560_DATAFORMAT_LEFT_J		(0x3 << 2)

#define TAS2560_DAI_FMT_MASK			(0x7 << 2)


#define TAS2560_ASI_CHANNEL_LEFT		0x0
#define TAS2560_ASI_CHANNEL_RIGHT		0x1
#define TAS2560_ASI_CHANNEL_LEFT_RIGHT		0x2
#define TAS2560_ASI_CHANNEL_MONO		0x3

#define TAS2560_ASI_CHANNEL_MASK                0X3

#define LOAD_MASK				0x18
#define LOAD_8OHM				(0)
#define LOAD_6OHM				(1)
#define LOAD_4OHM				(2)

#define CHECK_PERIOD	5000	/* 5 second */

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
#define	ERROR_FAILSAFE		0x40000000


struct tas2560_register {
	int book;
	int page;
	int reg;
};

struct tas2560_dai_cfg {
	unsigned int dai_fmt;
	unsigned int tdm_delay;
};

enum channel{
	channel_left = 0x01,
	channel_right = 0x02,
	channel_max = 0x02,
	channel_both = (channel_left|channel_right),
} ;

struct tas2560_priv {
	u8 i2c_regs_status;
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;

	unsigned char mnLAddr;
	unsigned char mnRAddr;
	int mnLCurrentBook;
	int mnLCurrentPage;
	int mnRCurrentBook;
	int mnRCurrentPage;
	enum channel mnCurrentChannel;

	struct mutex dev_lock;
	/*struct tas2560_dai_cfg dai_cfg[3];*/
	struct delayed_work irq_work;
	int mnClkin;
	int mnClkid;
	bool mbPowerUp[channel_max];
	int mnLeftLoad ;
	int mnRightLoad ;
	int mnLeftResetGPIO;
	int mnRightResetGPIO;
	int mnLeftIRQGPIO;
	int mnRightIRQGPIO;
	int mnLeftIRQ;
	int mnRightIRQ;
	bool mbIRQEnable;
	int mnSamplingRate;
	int mnBitRate;
	int mnFrameSize;
	int mnSpkType;
	struct device_node *spk_id_gpio_p;

	int (*read) (struct tas2560_priv *pTAS2555,
		enum channel chn,
		unsigned int reg,
		unsigned int *pValue);
	int (*write) (struct tas2560_priv *pTAS2555,
		enum channel chn,
		unsigned int reg,
		unsigned int Value);
	int (*bulk_read) (struct tas2560_priv *pTAS2555,
		enum channel chn,
		unsigned int reg,
		unsigned char *pData, unsigned int len);
	int (*bulk_write) (struct tas2560_priv *pTAS2555,
		enum channel chn,
		unsigned int reg,
		unsigned char *pData, unsigned int len);
	int (*update_bits) (struct tas2560_priv *pTAS2555,
		enum channel chn,
		unsigned int reg,
		unsigned int mask, unsigned int value);
	void (*hw_reset)(struct tas2560_priv *pTAS2560);
	int (*sw_reset)(struct tas2560_priv *pTAS2560);
	void (*clearIRQ)(struct tas2560_priv *pTAS2560);
	void (*enableIRQ)(struct tas2560_priv *pTAS2560, bool enable);
	/* device is working, but system is suspended */
	int (*runtime_suspend)(struct tas2560_priv *pTAS2560);
	int (*runtime_resume)(struct tas2560_priv *pTAS2560);
	bool mbRuntimeSuspend;

	unsigned int mnErrCode;
#ifdef CONFIG_TAS2560_CODEC
	struct mutex codec_lock;
#endif

#ifdef CONFIG_TAS2560_MISC_STEREO
	int mnDBGCmd;
	int mnCurrentReg;
	struct mutex file_lock;
#endif
};
#endif

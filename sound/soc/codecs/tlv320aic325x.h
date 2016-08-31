/*
 * linux/sound/soc/codecs/tlv320aic325x.h
 *
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 *
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * History:
 * Rev 1.1	Added More ENUM Macros			18-01-2011
 *
 */

#ifndef _TLV320AIC325x_H
#define _TLV320AIC325x_H

#include "./aic3xxx/aic3xxx_cfw.h"
#include "./aic3xxx/aic3xxx_cfw_ops.h"
#define AUDIO_NAME "aic325x"
#define AIC325x_VERSION "1.1"

/* #define AIC3256_CODEC_SUPPORT 1 */

/* Enable slave / master mode for codec */
#define AIC325x_MCBSP_SLAVE
/*#undef AIC325x_MCBSP_SLAVE*/
/* Macro enables or disables support for miniDSP in the driver */

/*#undef CONFIG_MINI_DSP*/

/* Enable headset detection */
/*#define HEADSET_DETECTION*/
#undef HEADSET_DETECTION

/* Macro enables or disables  AIC3xxx TiLoad Support */
#define AIC3256_TiLoad
/* #undef AIC3xxx_TiLoad */
/* Enable register caching on write */
#define EN_REG_CACHE

/* Flag to Select OMAP PANDA Board */
#define CONFIG_SND_SOC_OMAP_AIC3256

/* AIC325x supported sample rate are 8k to 192k */
#define AIC325x_RATES	SNDRV_PCM_RATE_8000_192000

/* AIC325x supports the word formats 16bits, 20bits, 24bits and 32 bits */
#define AIC325x_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define AIC325x_FREQ_12000000 12000000
#define	AIC325x_FREQ_12288000 12288000
#define AIC325x_FREQ_24000000 24000000
#define AIC325x_FREQ_19200000 19200000
#define AIC325x_FREQ_38400000 38400000

/* Audio data word length = 16-bits (default setting) */
#define AIC325x_WORD_LEN_16BITS		0x00
#define AIC325x_WORD_LEN_20BITS		0x01
#define AIC325x_WORD_LEN_24BITS		0x02
#define AIC325x_WORD_LEN_32BITS		0x03

/* #define AIC325x_8BITS_MASK		0XFF */
/* sink: name of target widget */
#define AIC325x_WIDGET_NAME             0
/* control: mixer control name */
#define AIC325x_CONTROL_NAME		1
/* source: name of source name */
#define AIC325x_SOURCE_NAME		2

/* D15..D8 aic325x register offset */
#define AIC325x_REG_OFFSET_INDEX        0
/* D7...D0 register data */
#define AIC325x_REG_DATA_INDEX          1

/* Serial data bus uses I2S mode (Default mode) */
#define AIC325x_I2S_MODE		0x00
#define AIC325x_DSP_MODE		0x01
#define AIC325x_RIGHT_JUSTIFIED_MODE	0x02
#define AIC325x_LEFT_JUSTIFIED_MODE	0x03

/* 8 bit mask value */
#define AIC325x_8BITS_MASK              0xFF

/* shift value for CLK_REG_3 register */
#define CLK_REG_3_SHIFT			6
/* shift value for DAC_OSR_MSB register */
#define DAC_OSR_MSB_SHIFT		4

/* Mask for Headset detection status */
#define FLAG_HS_MASKBITS		0x10

/* number of codec specific register for configuration */
#define NO_FEATURE_REGS			2

/* AIC325x register space */
#define	AIC325x_CACHEREGNUM		256

#endif


/* Moved the registers to tlv320aic3256-registers.h file
	as part of the MFD changes */

/****************************************************************************/
#define BIT7				(0x01 << 7)
#define CODEC_CLKIN_MASK		0x03
#define MCLK_2_CODEC_CLKIN		0x00
#define PLLCLK_2_CODEC_CLKIN	        0x03
/*Bclk_in selection*/
#define BDIV_CLKIN_MASK			0x03
#define	DAC_MOD_CLK_2_BDIV_CLKIN        0x01
#define SOFT_RESET			0x01
#define PAGE0				0x00
#define PAGE1				0x01
#define BIT_CLK_MASTER			0x08
#define WORD_CLK_MASTER			0x04
#define BCLK_INV_MASK			0x08
#define	HIGH_PLL			(0x01 << 6)
#define ENABLE_PLL			BIT7
#define ENABLE_NDAC			BIT7
#define ENABLE_MDAC			BIT7
#define ENABLE_NADC			BIT7
#define ENABLE_MADC			BIT7
#define ENABLE_BCLK			BIT7
#define ENABLE_DAC		        (0x03 << 6)
#define LDAC_2_LCHN			(0x01 << 4)
#define RDAC_2_RCHN			(0x01 << 2)
#define LDAC_CHNL_2_HPL			(0x01 << 3)
#define RDAC_CHNL_2_HPR			(0x01 << 3)
#define SOFT_STEP_2WCLK			(0x01)
#define DAC_MUTE_ON			0x0C
#define ADC_MUTE_ON			0x88
#define DEFAULT_VOL			0x0
#define DISABLE_ANALOG			(0x01 << 3)
#define LDAC_2_HPL_ROUTEON		0x08
#define RDAC_2_HPR_ROUTEON		0x08

#define HP_DRIVER_BUSY_MASK		0x04
/* Headphone driver Configuration Register Page 1, Register 125 */
#define GCHP_ENABLE			0x10
#define DC_OC_FOR_ALL_COMB		0x03
#define DC_OC_FOR_PROG_COMB		0x02

/* Reference Power-Up configuration register */
#define REF_PWR_UP_MASK			0x4
#define AUTO_REF_PWR_UP			0x0
#define FORCED_REF_PWR_UP		0x4

/* Power Configuration register 1 */
#define WEAK_AVDD_TO_DVDD_DIS		0x8

/* Power Configuration register 1 */
#define ANALOG_BLOCK_POWER_CONTROL_MASK	0x08
#define ENABLE_ANALOG_BLOCK		0x0
#define DISABLE_ANALOG_BLOCK		0x8

/* Floating input Configuration register P1_R58 */
#define WEAK_BIAS_INPUTS_MASK		0xFC

/* Common Mode Control Register */
#define GCHP_HPL_STATUS			0x4

/* Audio Interface Register 3 P0_R29 */
#define BCLK_WCLK_BUFFER_POWER_CONTROL_MASK	0x4
#define BCLK_WCLK_BUFFER_ON			0x4

/* Power Configuration Register */
#define AVDD_CONNECTED_TO_DVDD_MASK	0x8
#define DISABLE_AVDD_TO_DVDD		0x8
#define ENABLE_AVDD_TO_DVDD		0x0


/* Masks used for updating register bits */
#define PLL_P_DIV_MASK			0x7F
#define PLL_J_DIV_MASK			0x7F
#define PLL_NDAC_DIV_MASK		0x7F
#define PLL_MDAC_DIV_MASK		0x7F
#define PLL_NADC_DIV_MASK		0x7F
#define PLL_MADC_DIV_MASK		0x7F
#define PLL_BCLK_DIV_MASK		0x7F
#define INTERFACE_REG_MASK		0x7F
#define INTERFACE_REG1_DATA_TYPE_MASK	0xC0
#define INTERFACE_REG1_MASTER_MASK	0x0C
#define INTERFACE_REG2_MASK		0xFF
#define INTERFACE_REG3_MASK		0x08

#define PLL_D_MSB_DIV_MASK		0x3F
#define PLL_D_LSB_DIV_MASK		0xFF
#define PLL_DOSR_MSB_MASK		0x03
#define PLL_DOSR_LSB_MASK		0xFF
#define PLL_AOSR_DIV_MASK		0xFF

#define BCLK_DIV_POWER_MASK		0x80
#define DAC_MUTE_MASK			0x0C
#define ADC_MUTE_MASK			0x88
#define CODEC_RESET_MASK		0x01

#define TIME_DELAY			5
#define DELAY_COUNTER			100
/*
 *****************************************************************************
 * Structures Definitions
 *****************************************************************************
 */
/*
 *----------------------------------------------------------------------------
 * @struct  aic325x_setup_data |
 *          i2c specific data setup for AIC325x.
 * @field   unsigned short |i2c_address |
 *          Unsigned short for i2c address.
 *----------------------------------------------------------------------------
 */
struct aic325x_setup_data {
	unsigned short i2c_address;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic325x_priv |
 *          AIC325x priviate data structure to set the system clock, mode and
 *          page number.
 * @field   u32 | sysclk |
 *          system clock
 * @field   s32 | master |
 *          master/slave mode setting for AIC325x
 * @field   u8 | page_no |
 *          page number. Here, page 0 and page 1 are used.
 *----------------------------------------------------------------------------
 */
struct aic3256_jack_data {
	struct snd_soc_jack *jack;
	int report;
	struct switch_dev sdev;
};

struct aic325x_priv {
	u32 sysclk;
	s32 master;
	u8 page_no;
	struct aic3256_jack_data hs_jack;
	struct workqueue_struct *workqueue;
	struct delayed_work delayed_work;
	int hs_half_insert_count;
	struct snd_soc_codec *codec;
	struct i2c_client *control_data;
	int irq;
	struct snd_soc_jack *headset_jack;
	struct mutex io_lock;
	int playback_stream;
	int record_stream;

	void *pdata;
	struct mutex mutex;
	struct mutex *cfw_mutex;
	struct cfw_state cfw_ps;
	struct cfw_state *cfw_p;
	struct firmware *cur_fw;
	int dsp_runstate;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic325x_configs |
 *          AIC325x initialization data which has register offset and register
 *          value.
 * @field   u16 | reg_offset |
 *          AIC325x Register offsets required for initialization..
 * @field   u8 | reg_val |
 *          value to set the AIC325x register to initialize the AIC325x.
 *----------------------------------------------------------------------------
 */
struct aic325x_configs {
	u8 reg_offset;
	u8 reg_val;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic325x_rate_divs |
 *          Setting up the values to get different freqencies
 *
 * @field   u32 | mclk |
 *          Master clock
 * @field   u32 | rate |
 *          sample rate
 * @field   u8 | p_val |
 *          value of p in PLL
 * @field   u32 | pll_j |
 *          value for pll_j
 * @field   u32 | pll_d |
 *          value for pll_d
 * @field   u32 | dosr |
 *          value to store dosr
 * @field   u32 | ndac |
 *          value for ndac
 * @field   u32 | mdac |
 *          value for mdac
 * @field   u32 | aosr |
 *          value for aosr
 * @field   u32 | nadc |
 *          value for nadc
 * @field   u32 | madc |
 *          value for madc
 * @field   u32 | blck_N |
 *          value for block N
 * @field   u32 | aic325x_configs |
 *          configurations for aic325x register value
 *----------------------------------------------------------------------------
 */
struct aic325x_rate_divs {
	u32 mclk;
	u32 rate;
	u8 p_val;
	u8 pll_j;
	u16 pll_d;
	u16 dosr;
	u8 ndac;
	u8 mdac;
	u8 aosr;
	u8 nadc;
	u8 madc;
	u8 blck_N;
	struct aic325x_configs codec_specific_regs[NO_FEATURE_REGS];
};


void aic3256_hs_jack_detect(struct snd_soc_codec *codec,
				struct snd_soc_jack *jack, int report);



/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *          It is SoC Codec DAI structure which has DAI capabilities viz.,
 *          playback and capture, DAI runtime information viz. state of DAI
 *			and pop wait state, and DAI private data.
 *----------------------------------------------------------------------------
 */
extern struct snd_soc_dai tlv320aic325x_dai;

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic325x_probe(), aic325x_remove(),
 *			aic325x_suspend() and aic325x_resume()
 *
 */
extern struct snd_soc_codec_device soc_codec_dev_aic325x;

				/* _TLV320AIC325x_H */

/*
 * definitions for PCM1792A
 *
 * Copyright 2013 Amarula Solutions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MT2712_CODEC_H__
#define __MT2712_CODEC_H__

struct mt2712_codec_priv {
	void __iomem *base_addr;
	struct regmap *regmap_dig;
	struct regmap *regmap_ana;
	struct snd_soc_codec *codec;
	int pga_gain;
};

enum {
	ANALOG_UL_PGA_GAIN_negtive_6dB = 0,
	ANALOG_UL_PGA_GAIN_0dB         = 1,
	ANALOG_UL_PGA_GAIN_6dB         = 2,
	ANALOG_UL_PGA_GAIN_12dB        = 3,
	ANALOG_UL_PGA_GAIN_18dB        = 4,
	ANALOG_UL_PGA_GAIN_24dB        = 5,
	ANALOG_UL_PGA_GAIN_NA          = 6,
};

#define ABB_AFE_CON0 0x00U
#define ABB_AFE_CON1 0x04U
#define ABB_AFE_CON11 0x2cU
#define AFE_ADDA_UL_SRC_CON0 0x3cU
#define AFE_ADDA_UL_SRC_CON1 0x40U
#define AFE_ADDA_UL_DL_CON0 0x44U

#define AFIFO_RATE (0x1fU << 3U)
#define AFIFO_RATE_SET(x) ((x) << 3U)
#define AFIFO_SRPT (0x7U)
#define AFIFO_SRPT_SET(x) (x)
#define ABB_UL_RATE (0x1U << 4U)
#define ABB_UL_RATE_SET(x) ((x) << 4U)
#define ABB_PDN_I2SO1 (0x1U << 3U)
#define ABB_PDN_I2SO1_SET(x) ((x) << 3U)
#define ABB_PDN_I2SI1 (0x1U << 2U)
#define ABB_PDN_I2SI1_SET(x) ((x) << 2U)
#define ABB_UL_EN (0x1U << 1U)
#define ABB_UL_EN_SET(x) ((x) << 1U)
#define ABB_AFE_EN (0x1U)
#define ABB_AFE_EN_SET(x) (x)
#define ULSRC_VOICE_MODE (0x3U << 17U)
#define ULSRC_VOICE_MODE_SET(x) ((x) << 17U)
#define ULSRC_ON (0x1U)
#define ULSRC_ON_SET(x) (x)

#define ADDA_END_ADDR 0x88U

/* AFE_ADDA_UL_SRC_CON1: ul src sgen setting */
#define UL_SRC_SGEN_EN_POS		27U
#define UL_SRC_SGEN_EN_MASK		(1U << UL_SRC_SGEN_EN_POS)
#define UL_SRC_MUTE_POS			26U
#define UL_SRC_MUTE_MASK		(1U << UL_SRC_MUTE_POS)
#define UL_SRC_CH2_AMP_POS		21U
#define UL_SRC_CH2_AMP_MASK		(0x7U << UL_SRC_CH2_AMP_POS)
#define UL_SRC_CH2_FREQ_POS		16U
#define UL_SRC_CH2_FREQ_MASK		(0x1fU << UL_SRC_CH2_FREQ_POS)
#define UL_SRC_CH2_MODE_POS		12U
#define UL_SRC_CH2_MODE_MASK		(0xfU << UL_SRC_CH2_MODE_POS)
#define UL_SRC_CH1_AMP_POS		9U
#define UL_SRC_CH1_AMP_MASK		(0x7U << UL_SRC_CH1_AMP_POS)
#define UL_SRC_CH1_FREQ_POS		4U
#define UL_SRC_CH1_FREQ_MASK		(0x1fU << UL_SRC_CH1_FREQ_POS)
#define UL_SRC_CH1_MODE_POS		0U
#define UL_SRC_CH1_MODE_MASK		(0xfU << UL_SRC_CH1_MODE_POS)

/* AFE_ADDA_UL_DL_CON0 */
#define ADDA_adda_afe_on_POS		0U
#define ADDA_adda_afe_on_MASK		(1U << ADDA_adda_afe_on_POS)

/* apmixedsys regmap - analog part reg */
#define APMIXED_OFFSET           (0U)	/* 0x10209000 */
#define APMIXED_REG(reg)         ((reg) | APMIXED_OFFSET)

#define AADC_CON0        APMIXED_REG(0x0910U)
#define AADC_CON1        APMIXED_REG(0x0914U)
#define AADC_CON2        APMIXED_REG(0x0918U)
#define AADC_CON3        APMIXED_REG(0x091CU)
#define AADC_CON4        APMIXED_REG(0x0920U)

/* AADC_CON0 */
#define RG_AUDULR_VCM14_EN_POS			1U
#define RG_AUDULR_VCM14_EN_MASK			(1U << RG_AUDULR_VCM14_EN_POS)
#define RG_AUDULR_VREF24_EN_POS			2U
#define RG_AUDULR_VREF24_EN_MASK		(1U << RG_AUDULR_VREF24_EN_POS)
#define RG_AUDULR_VADC_DVREF_CAL_POS		3U
#define RG_AUDULR_VADC_DVREF_CAL_MASK		(1U << RG_AUDULR_VADC_DVREF_CAL_POS)
#define RG_AUDULR_VUPG_POS			4U
#define RG_AUDULR_VUPG_MASK			(7U << RG_AUDULR_VUPG_POS)
#define RG_AUDULR_VADC_DENB_POS			7U
#define RG_AUDULR_VADC_DENB_MASK		(1U << RG_AUDULR_VADC_DENB_POS)
#define RG_AUDULR_VPWDB_ADC_POS			15U
#define RG_AUDULR_VPWDB_ADC_MASK		(1U << RG_AUDULR_VPWDB_ADC_POS)
#define RG_AUDULR_VPWDB_PGA_POS			16U
#define RG_AUDULR_VPWDB_PGA_MASK		(1U << RG_AUDULR_VPWDB_PGA_POS)
#define RG_AUDULL_VCM14_EN_POS			19U
#define RG_AUDULL_VCM14_EN_MASK			(1U << RG_AUDULL_VCM14_EN_POS)
#define RG_AUDULL_VREF24_EN_POS			20U
#define RG_AUDULL_VREF24_EN_MASK		(1U << RG_AUDULL_VREF24_EN_POS)
#define RG_AUDULL_VADC_DVREF_CAL_POS		21U
#define RG_AUDULL_VADC_DVREF_CAL_MASK		(1U << RG_AUDULL_VADC_DVREF_CAL_POS)
#define RG_AUDULL_VADC_DENB_POS			22U
#define RG_AUDULL_VADC_DENB_MASK		(1U << RG_AUDULL_VADC_DENB_POS)
#define RG_AUDULL_VPWDB_ADC_POS			23U
#define RG_AUDULL_VPWDB_ADC_MASK		(1U << RG_AUDULL_VPWDB_ADC_POS)
#define RG_AUDULL_VPWDB_PGA_POS			24U
#define RG_AUDULL_VPWDB_PGA_MASK		(1U << RG_AUDULL_VPWDB_PGA_POS)
#define RG_AUDULL_VUPG_POS			25U
#define RG_AUDULL_VUPG_MASK			(7U << RG_AUDULL_VUPG_POS)
#define RG_AUDULL_VCFG_POS			28U

/* AADC_CON3 */
#define RG_AUDUL_TEST_TIELOW_POS	1U
#define RG_AUDUL_TEST_TIELOW_MASK	(1U << RG_AUDUL_TEST_TIELOW_POS)
#define RG_AUDMICBIASVREF_POS		12U
#define RG_AUDMICBIASVREF_MASK		(3U << RG_AUDMICBIASVREF_POS)
#define RG_AUDPWDBMICBIAS_POS		14U
#define RG_AUDPWDBMICBIAS_MASK		(1U << RG_AUDPWDBMICBIAS_POS)
#define RG_CLK_SEL_POS			16U
#define RG_CLK_SEL_MASK			(1U << RG_CLK_SEL_POS)
#define RG_CLK_EN_POS			17U
#define RG_CLK_EN_MASK			(1U << RG_CLK_EN_POS)
#define RG_ANAREG_DELAY_SEL_POS		29U
#define RG_ANAREG_DELAY_SEL_MASK	(3U << RG_ANAREG_DELAY_SEL_POS)

#endif

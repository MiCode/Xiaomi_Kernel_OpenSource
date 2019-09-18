/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _AQT1000_INTERNAL_H
#define _AQT1000_INTERNAL_H

#include <linux/types.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#define AQT1000_RATES_MASK (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			    SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			    SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 |\
			    SNDRV_PCM_RATE_384000)
/* Fractional Rates */
#define AQT1000_FRAC_RATES_MASK (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 |\
				 SNDRV_PCM_RATE_176400)

#define AQT1000_FORMATS_S16_S24_LE (SNDRV_PCM_FMTBIT_S16_LE | \
				    SNDRV_PCM_FMTBIT_S24_LE)

#define AQT1000_FORMATS_S16_S24_S32_LE (SNDRV_PCM_FMTBIT_S16_LE | \
					SNDRV_PCM_FMTBIT_S24_LE | \
					SNDRV_PCM_FMTBIT_S32_LE)

#define AQT1000_FORMATS_S16_LE (SNDRV_PCM_FMTBIT_S16_LE)

/* Macros for packing register writes into a U32 */
#define AQT1000_PACKED_REG_SIZE sizeof(u32)
#define AQT1000_CODEC_UNPACK_ENTRY(packed, reg, mask, val) \
	do { \
		((reg) = ((packed >> 16) & (0xffff))); \
		((mask) = ((packed >> 8) & (0xff))); \
		((val) = ((packed) & (0xff))); \
	} while (0)

#define STRING(name) #name
#define AQT_DAPM_ENUM(name, reg, offset, text) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM(STRING(name), name##_enum)

#define AQT_DAPM_ENUM_EXT(name, reg, offset, text, getname, putname) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM_EXT(STRING(name), name##_enum, getname, putname)

#define AQT_DAPM_MUX(name, shift, kctl) \
		SND_SOC_DAPM_MUX(name, SND_SOC_NOPM, shift, 0, &kctl##_mux)


#define AQT1000_INTERP_MUX_NUM_INPUTS 3
#define AQT1000_RX_PATH_CTL_OFFSET 20

#define BYTE_BIT_MASK(nr) (1 << ((nr) % BITS_PER_BYTE))

#define AQT1000_REG_BITS 8
#define AQT1000_MAX_VALID_ADC_MUX  3

#define AQT1000_AMIC_PWR_LEVEL_LP 0
#define AQT1000_AMIC_PWR_LEVEL_DEFAULT 1
#define AQT1000_AMIC_PWR_LEVEL_HP 2
#define AQT1000_AMIC_PWR_LVL_MASK 0x60
#define AQT1000_AMIC_PWR_LVL_SHIFT 0x5

#define AQT1000_DEC_PWR_LVL_MASK 0x06
#define AQT1000_DEC_PWR_LVL_DF 0x00
#define AQT1000_DEC_PWR_LVL_LP 0x02
#define AQT1000_DEC_PWR_LVL_HP 0x04
#define AQT1000_STRING_LEN 100

#define AQT1000_CDC_SIDETONE_IIR_COEFF_MAX 5

#define AQT1000_MAX_MICBIAS 1
#define DAPM_MICBIAS1_STANDALONE "MIC BIAS1 Standalone"

#define  TX_HPF_CUT_OFF_FREQ_MASK    0x60
#define  CF_MIN_3DB_4HZ              0x0
#define  CF_MIN_3DB_75HZ             0x1
#define  CF_MIN_3DB_150HZ            0x2

enum {
	AUDIO_NOMINAL,
	HPH_PA_DELAY,
	CLSH_Z_CONFIG,
	ANC_MIC_AMIC1,
	ANC_MIC_AMIC2,
	ANC_MIC_AMIC3,
};

enum {
	INTn_1_INP_SEL_ZERO = 0,
	INTn_1_INP_SEL_DEC0,
	INTn_1_INP_SEL_DEC1,
	INTn_1_INP_SEL_IIR0,
	INTn_1_INP_SEL_IIR1,
	INTn_1_INP_SEL_RX0,
	INTn_1_INP_SEL_RX1,
};

enum {
	INTn_2_INP_SEL_ZERO = 0,
	INTn_2_INP_SEL_RX0,
	INTn_2_INP_SEL_RX1,
	INTn_2_INP_SEL_PROXIMITY,
};

/* Codec supports 2 IIR filters */
enum {
	IIR0 = 0,
	IIR1,
	IIR_MAX,
};

enum {
	ASRC_IN_HPHL,
	ASRC_IN_HPHR,
	ASRC_INVALID,
};

enum {
	CONV_88P2K_TO_384K,
	CONV_96K_TO_352P8K,
	CONV_352P8K_TO_384K,
	CONV_384K_TO_352P8K,
	CONV_384K_TO_384K,
	CONV_96K_TO_384K,
};

enum aqt_notify_event {
	AQT_EVENT_INVALID,
	/* events for micbias ON and OFF */
	AQT_EVENT_PRE_MICBIAS_1_OFF,
	AQT_EVENT_POST_MICBIAS_1_OFF,
	AQT_EVENT_PRE_MICBIAS_1_ON,
	AQT_EVENT_POST_MICBIAS_1_ON,
	AQT_EVENT_PRE_DAPM_MICBIAS_1_OFF,
	AQT_EVENT_POST_DAPM_MICBIAS_1_OFF,
	AQT_EVENT_PRE_DAPM_MICBIAS_1_ON,
	AQT_EVENT_POST_DAPM_MICBIAS_1_ON,
	/* events for PA ON and OFF */
	AQT_EVENT_PRE_HPHL_PA_ON,
	AQT_EVENT_POST_HPHL_PA_OFF,
	AQT_EVENT_PRE_HPHR_PA_ON,
	AQT_EVENT_POST_HPHR_PA_OFF,
	AQT_EVENT_PRE_HPHL_PA_OFF,
	AQT_EVENT_PRE_HPHR_PA_OFF,
	AQT_EVENT_OCP_OFF,
	AQT_EVENT_OCP_ON,
	AQT_EVENT_LAST,
};

struct interp_sample_rate {
	int sample_rate;
	int rate_val;
};

extern struct regmap_config aqt1000_regmap_config;
extern int aqt_register_codec(struct device *dev);

#endif /* _AQT1000_INTERNAL_H */

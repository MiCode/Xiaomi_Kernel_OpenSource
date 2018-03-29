/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <sound/soc.h>
#include "mt6391.h"
#include <linux/mfd/mt6397/registers.h>

#define MT6397_CODEC_NAME "mt6397-codec"
#define PMIC6397_E1_CID_CODE 0x1097
#define ENUM_TO_STR(enum) #enum

#define USE_MT6397_REGMAP
/* #define USE_PMIC_WRAP_DRIVER */

#if defined(USE_MT6397_REGMAP)
#include <linux/mfd/mt6397/core.h>
#elif defined(USE_PMIC_WRAP_DRIVER)
#include <mt_pmic_wrap.h>
#include <mt-plat/upmu_common.h>
#endif

#define MT6397_TRIM_ADDRESS1    (MT6397_EFUSE_DOUT_192_207)
#define MT6397_TRIM_ADDRESS2    (MT6397_EFUSE_DOUT_208_223)


/* enum definition */
enum mt6391_device_volume {
	MT6391_VOL_HSOUTL = 0,
	MT6391_VOL_HSOUTR,
	MT6391_VOL_HPOUTL,
	MT6391_VOL_HPOUTR,
	MT6391_VOL_SPKL,
	MT6391_VOL_SPKR,
	MT6391_VOL_SPEAKER_HEADSET_R,
	MT6391_VOL_SPEAKER_HEADSET_L,
	MT6391_VOL_IV_BUFFER,
	MT6391_VOL_LINEOUTL,
	MT6391_VOL_LINEOUTR,
	MT6391_VOL_LINEINL,
	MT6391_VOL_LINEINR,
	MT6391_VOL_MICAMPL,
	MT6391_VOL_MICAMPR,
	MT6391_VOL_LEVELSHIFTL,
	MT6391_VOL_LEVELSHIFTR,
	MT6391_VOL_TYPE_MAX
};

enum mt6391_device_mux {
	MT6391_MUX_VOICE = 0,
	MT6391_MUX_AUDIO,
	MT6391_MUX_IV_BUFFER,
	MT6391_MUX_LINEIN_STEREO,
	MT6391_MUX_LINEIN_L,
	MT6391_MUX_LINEIN_R,
	MT6391_MUX_LINEIN_AUDIO_MONO,
	MT6391_MUX_LINEIN_AUDIO_STEREO,
	MT6391_MUX_IN_MIC1,
	MT6391_MUX_IN_MIC2,
	MT6391_MUX_IN_MIC3,
	MT6391_MUX_IN_PREAMP_1,
	MT6391_MUX_IN_PREAMP_2,
	MT6391_MUX_IN_LEVEL_SHIFT_BUFFER,
	MT6391_MUX_MUTE,
	MT6391_MUX_OPEN,
	MT6391_MUX_MAX_TYPE
};

enum mt6391_device_type {
	MT6391_DEV_OUT_EARPIECER = 0,
	MT6391_DEV_OUT_EARPIECEL,
	MT6391_DEV_OUT_HEADSETR,
	MT6391_DEV_OUT_HEADSETL,
	MT6391_DEV_OUT_SPEAKERR,
	MT6391_DEV_OUT_SPEAKERL,
	MT6391_DEV_OUT_SPEAKER_HEADSET_R,
	MT6391_DEV_OUT_SPEAKER_HEADSET_L,
	MT6391_DEV_IN_ADC1,
	MT6391_DEV_IN_ADC2,
	MT6391_DEV_IN_PREAMP_L,
	MT6391_DEV_IN_PREAMP_R,
	MT6391_DEV_IN_DIGITAL_MIC,
	MT6391_DEV_MAX,
	MT6391_DEV_OUT_MAX = MT6391_DEV_OUT_SPEAKER_HEADSET_L + 1,
	MT6391_DEV_ADC_MAX = MT6391_DEV_IN_ADC2 + 1,
	MT6391_DEV_IN_MAX = MT6391_DEV_IN_DIGITAL_MIC + 1,
};

enum mt6391_adda_type {
	MT6391_ADDA_DAC,
	MT6391_ADDA_ADC,
	MT6391_ADDA_MAX
};

enum mt6391_speaker_mode {
	MT6391_CLASS_D = 0,
	MT6391_CLASS_AB,
};

enum mt6391_speaker_channel_sel {
	MT6391_CHANNEL_SEL_STEREO = 0,
	MT6391_CHANNEL_SEL_MONO_LEFT,
	MT6391_CHANNEL_SEL_MONO_RIGHT,
};

enum mt6391_loopback {
	CODEC_LOOPBACK_NONE = 0,
	CODEC_LOOPBACK_AMIC_TO_SPK,
	CODEC_LOOPBACK_AMIC_TO_HP,
	CODEC_LOOPBACK_DMIC_TO_SPK,
	CODEC_LOOPBACK_DMIC_TO_HP,
	CODEC_LOOPBACK_HEADSET_MIC_TO_SPK,
	CODEC_LOOPBACK_HEADSET_MIC_TO_HP,
	CODEC_LOOPBACK_AMIC_TO_EXTDAC,
	CODEC_LOOPBACK_HEADSET_MIC_TO_EXTDAC,
};

enum mt6391_dac_frequency {
	DAC_FREQ_8000 = 0,
	DAC_FREQ_11025,
	DAC_FREQ_12000,
	DAC_FREQ_16000,
	DAC_FREQ_22050,
	DAC_FREQ_24000,
	DAC_FREQ_32000,
	DAC_FREQ_44100,
	DAC_FREQ_48000,
};

enum mt6391_adc_frequency {
	ADC_FREQ_8000 = 0,
	ADC_FREQ_16000,
	ADC_FREQ_32000,
	ADC_FREQ_48000,
};

enum mt6391_soc_enum_type {
	ENUM_AUDIO_AMP = 0,
	ENUM_VOICE_AMP,
	ENUM_SPK_AMP,
	ENUM_HS_SPK_AMP,
	ENUM_HEADSETL_GAIN,
	ENUM_HEADSETR_GAIN,
	ENUM_HANDSET_GAIN,
	ENUM_SPKL_GAIN,
	ENUM_SPKR_GAIN,
	ENUM_SPK_SEL,
	ENUM_SPK_OC_FLAG,
	ENUM_DAC_SCK,
	ENUM_DMIC_SWITCH,
	ENUM_ADC1_SWITCH,
	ENUM_ADC2_SWITCH,
	ENUM_PREAMP1_MUX,
	ENUM_PREAMP2_MUX,
	ENUM_PREAMP1_GAIN,
	ENUM_PREAMP2_GAIN,
	ENUM_LOOPBACK_SEL,
	ENUM_DAC_SGEN,
	ENUM_ADC_SGEN,
	ENUM_DAC_FREQ,
	ENUM_ADC_FREQ,
};


/* codec private data */
struct mt6391_priv {
	int device_volume[MT6391_VOL_TYPE_MAX];
	int device_mux[MT6391_MUX_MAX_TYPE];
	bool device_power[MT6391_DEV_MAX];
	uint32_t sample_rate[MT6391_ADDA_MAX];
	uint32_t speaker_channel_sel;
	uint32_t speaker_mode;
	uint32_t adc_warmup_time_us;
	uint32_t dmic_warmup_time_us;
	uint8_t hpl_trim;
	uint8_t hpl_fine_trim;
	uint8_t hpr_trim;
	uint8_t hpr_fine_trim;
	uint8_t iv_hpl_trim;
	uint8_t iv_hpl_fine_trim;
	uint8_t iv_hpr_trim;
	uint8_t iv_hpr_fine_trim;
	uint8_t spkl_polarity;
	uint8_t ispkl_trim;
	uint8_t spkr_polarity;
	uint8_t ispkr_trim;
	uint32_t codec_loopback_type;
	uint32_t dac_sgen_switch;
	uint32_t adc_sgen_switch;
	int ana_clk_counter;
	struct mutex ctrl_mutex;
	struct mutex clk_mutex;
	struct snd_soc_codec *codec;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
};


/* Function implementation */

static uint32_t mt6391_get_reg(struct mt6391_priv *codec_data, uint32_t offset)
{
	uint32_t data = 0;

#if defined(USE_MT6397_REGMAP)
	data = snd_soc_read(codec_data->codec, offset);
#elif defined(USE_PMIC_WRAP_DRIVER)
	pwrap_read(offset, &data);
#endif
	return data;
}

static void mt6391_set_reg(struct mt6391_priv *codec_data, uint32_t offset,
		uint32_t value, uint32_t mask)
{
#if defined(USE_MT6397_REGMAP)
	snd_soc_update_bits(codec_data->codec, offset, mask, value);
#elif defined(USE_PMIC_WRAP_DRIVER)
	int ret = 0;
	uint32_t reg_value = mt6391_get_reg(codec_data, offset);

	reg_value &= (~mask);
	reg_value |= (value & mask);
	ret = pwrap_write(offset, reg_value);
	reg_value = mt6391_get_reg(codec_data, offset);
	if ((reg_value & mask) != (value & mask)) {
		pr_debug("%s 0x%x-0x%x(0x%x) ret = %d reg_value = 0x%x\n",
			 __func__, offset, value, mask, ret, reg_value);
	}
#endif
}

static void mt6391_control_top_clk(struct mt6391_priv *codec_data,
		uint32_t mask, bool enable)
{
	/* set pmic register or analog CONTROL_IFACE_PATH */
	uint32_t val;
	uint32_t reg = enable ? MT6397_TOP_CKPDN_CLR : MT6397_TOP_CKPDN_SET;

#if defined(USE_MT6397_REGMAP)
	snd_soc_update_bits(codec_data->codec, reg, mask, mask);
	val = snd_soc_read(codec_data->codec, MT6397_TOP_CKPDN);
#elif defined(USE_PMIC_WRAP_DRIVER)
	pwrap_write(reg, mask);
	pwrap_read(MT6397_TOP_CKPDN, &val);
#endif

	if ((val & mask) != (enable ? 0 : mask))
		pr_err("%s: data mismatch: mask=%04X, val=%04X, enable=%d\n",
			__func__, mask, val, enable);
}

static void mt6391_ana_clk_on(struct mt6391_priv *codec_data)
{
	mutex_lock(&codec_data->clk_mutex);
	if (codec_data->ana_clk_counter == 0) {
		pr_debug("+%s ana_clk_counter:%d\n", __func__, codec_data->ana_clk_counter);
#if defined(USE_MT6397_REGMAP)
		mt6391_set_reg(codec_data, MT6397_TOP_CKCON1, 0x0010, 0x0010);
#elif defined(USE_PMIC_WRAP_DRIVER)
		upmu_set_rg_clksq_en(1);
#endif
		mt6391_control_top_clk(codec_data, 0x0003, true);
	}
	codec_data->ana_clk_counter++;
	mutex_unlock(&codec_data->clk_mutex);
	pr_debug("-%s ana_clk_counter:%d\n", __func__, codec_data->ana_clk_counter);
}

static void mt6391_ana_clk_off(struct mt6391_priv *codec_data)
{
	mutex_lock(&codec_data->clk_mutex);
	codec_data->ana_clk_counter--;
	if (codec_data->ana_clk_counter == 0) {
		pr_debug("+%s ana_clk_counter:%d\n", __func__, codec_data->ana_clk_counter);
#if defined(USE_MT6397_REGMAP)
		mt6391_set_reg(codec_data, MT6397_TOP_CKCON1, 0x0000, 0x0010);
#elif defined(USE_PMIC_WRAP_DRIVER)
		upmu_set_rg_clksq_en(0);
#endif
		mt6391_control_top_clk(codec_data, 0x0003, false);
	} else if (codec_data->ana_clk_counter < 0) {
		pr_err("%s ana_clk_counter:%d<0\n", __func__, codec_data->ana_clk_counter);
		codec_data->ana_clk_counter = 0;
	}
	mutex_unlock(&codec_data->clk_mutex);
	pr_debug("-%s ana_clk_counter:%d\n", __func__, codec_data->ana_clk_counter);
}

static void mt6391_suspend_clk_on(struct mt6391_priv *codec_data)
{
	if (codec_data->ana_clk_counter > 0) {
		pr_debug("%s ana_clk_counter:%d\n", __func__, codec_data->ana_clk_counter);
#if defined(USE_MT6397_REGMAP)
		mt6391_set_reg(codec_data, MT6397_TOP_CKCON1, 0x0010, 0x0010);
#elif defined(USE_PMIC_WRAP_DRIVER)
		upmu_set_rg_clksq_en(1);
#endif
	}
}

static void mt6391_suspend_clk_off(struct mt6391_priv *codec_data)
{
	if (codec_data->ana_clk_counter > 0) {
		pr_debug("%s ana_clk_counter:%d\n", __func__, codec_data->ana_clk_counter);
#if defined(USE_MT6397_REGMAP)
		mt6391_set_reg(codec_data, MT6397_TOP_CKCON1, 0x0000, 0x0010);
#elif defined(USE_PMIC_WRAP_DRIVER)
		upmu_set_rg_clksq_en(0);
#endif
	}
}

static uint32_t mt6391_get_dl_input_mode(unsigned int frequency)
{
	uint32_t reg_value = 0;

	pr_debug("%s frequency = %d\n", __func__, frequency);
	switch (frequency) {
	case 8000:
		reg_value = 0;
		break;
	case 11025:
		reg_value = 1;
		break;
	case 12000:
		reg_value = 2;
		break;
	case 16000:
		reg_value = 3;
		break;
	case 22050:
		reg_value = 4;
		break;
	case 24000:
		reg_value = 5;
		break;
	case 32000:
		reg_value = 6;
		break;
	case 44100:
		reg_value = 7;
		break;
	case 48000:
		reg_value = 8;
		break;
	default:
		pr_warn("%s unexpected frequency = %d\n", __func__, frequency);
		break;
	}
	return reg_value;
}

uint32_t mt6391_get_ul_voice_mode(uint32_t frequency)
{
	uint32_t reg_value = 0;

	pr_debug("%s frequency = %d\n", __func__, frequency);

	switch (frequency) {
	case 8000:
		reg_value = 0x0 << 1;
		break;
	case 16000:
		reg_value = 0x5 << 1;
		break;
	case 32000:
		reg_value = 0xa << 1;
		break;
	case 48000:
		reg_value = 0xf << 1;
		break;
	default:
		pr_warn("%s unsupported frequency = %d\n", __func__, frequency);
	}
	pr_debug("%s reg_value = %d\n", __func__, reg_value);
	return reg_value;
}

static bool mt6391_get_dl_status(struct mt6391_priv *codec_data)
{
	int i = 0;

	for (i = 0; i < MT6391_DEV_OUT_MAX; i++) {
		if (codec_data->device_power[i])
			return true;
	}
	return false;
}

static bool mt6391_get_ul_status(struct mt6391_priv *codec_data)
{
	int i = 0;

	for (i = MT6391_DEV_IN_ADC1; i < MT6391_DEV_IN_MAX; i++) {
		if (codec_data->device_power[i])
			return true;
	}
	return false;
}

static bool mt6391_get_adc_status(struct mt6391_priv *codec_data)
{
	int i = 0;

	for (i = MT6391_DEV_IN_ADC1; i < MT6391_DEV_ADC_MAX; i++) {
		if (codec_data->device_power[i])
			return true;
	}
	return false;
}

static void mt6391_set_mux(struct mt6391_priv *codec_data,
	enum mt6391_device_type device_type, enum mt6391_device_mux mux_type)
{
	uint32_t reg_value = 0;

	switch (device_type) {
	case MT6391_DEV_OUT_EARPIECEL:
	case MT6391_DEV_OUT_EARPIECER:
		if (mux_type == MT6391_MUX_OPEN) {
			reg_value = 0;
		} else if (mux_type == MT6391_MUX_MUTE) {
			reg_value = 1 << 3;
		} else if (mux_type == MT6391_MUX_VOICE) {
			reg_value = 2 << 3;
		} else {
			reg_value = 2 << 3;
			pr_warn("%s %d %d\n", __func__, device_type, mux_type);
		}
		mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, reg_value, 0x000000018);
		break;
	case MT6391_DEV_OUT_HEADSETL:
		if (mux_type == MT6391_MUX_OPEN) {
			reg_value = 0;
		} else if (mux_type == MT6391_MUX_LINEIN_L) {
			reg_value = 1 << 5;
		} else if (mux_type == MT6391_MUX_LINEIN_R) {
			reg_value = 2 << 5;
		} else if (mux_type == MT6391_MUX_LINEIN_STEREO) {
			reg_value = 3 << 5;
		} else if (mux_type == MT6391_MUX_AUDIO) {
			reg_value = 4 << 5;
		} else if (mux_type == MT6391_MUX_LINEIN_AUDIO_MONO) {
			reg_value = 5 << 5;
		} else if (mux_type == MT6391_MUX_IV_BUFFER) {
			reg_value = 8 << 5;
		} else {
			reg_value = 4 << 5;
			pr_warn("%s %d %d\n", __func__, device_type, mux_type);
		}
		mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, reg_value, 0x000001e0);
		break;
	case MT6391_DEV_OUT_HEADSETR:
		if (mux_type == MT6391_MUX_OPEN) {
			reg_value = 0;
		} else if (mux_type == MT6391_MUX_LINEIN_L) {
			reg_value = 1 << 9;
		} else if (mux_type == MT6391_MUX_LINEIN_R) {
			reg_value = 2 << 9;
		} else if (mux_type == MT6391_MUX_LINEIN_STEREO) {
			reg_value = 3 << 9;
		} else if (mux_type == MT6391_MUX_AUDIO) {
			reg_value = 4 << 9;
		} else if (mux_type == MT6391_MUX_LINEIN_AUDIO_MONO) {
			reg_value = 5 << 9;
		} else if (mux_type == MT6391_MUX_IV_BUFFER) {
			reg_value = 8 << 9;
		} else {
			reg_value = 4 << 9;
			pr_warn("%s %d %d\n", __func__, device_type, mux_type);
		}
		mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, reg_value, 0x00001e00);
		break;
	case MT6391_DEV_OUT_SPEAKERR:
	case MT6391_DEV_OUT_SPEAKERL:
	case MT6391_DEV_OUT_SPEAKER_HEADSET_R:
	case MT6391_DEV_OUT_SPEAKER_HEADSET_L:
		if (mux_type == MT6391_MUX_OPEN) {
			reg_value = 0;
		} else if ((mux_type == MT6391_MUX_LINEIN_L)
			   || (mux_type == MT6391_MUX_LINEIN_R)) {
			reg_value = 1 << 2;
		} else if (mux_type == MT6391_MUX_LINEIN_STEREO) {
			reg_value = 2 << 2;
		} else if (mux_type == MT6391_MUX_OPEN) {
			reg_value = 3 << 2;
		} else if (mux_type == MT6391_MUX_AUDIO) {
			reg_value = 4 << 2;
		} else if (mux_type == MT6391_MUX_LINEIN_AUDIO_MONO) {
			reg_value = 5 << 2;
		} else if (mux_type == MT6391_MUX_LINEIN_AUDIO_STEREO) {
			reg_value = 6 << 2;
		} else {
			reg_value = 4 << 2;
			pr_warn("%s %d %d\n", __func__, device_type, mux_type);
		}
		mt6391_set_reg(codec_data, MT6397_AUD_IV_CFG0,
			       reg_value | (reg_value << 8), 0x00001c1c);
		break;
	case MT6391_DEV_IN_PREAMP_L:
		if (mux_type == MT6391_MUX_IN_MIC1) {
			reg_value = 1 << 2;
		} else if (mux_type == MT6391_MUX_IN_MIC2) {
			reg_value = 2 << 2;
		} else if (mux_type == MT6391_MUX_IN_MIC3) {
			reg_value = 3 << 2;
		} else {
			reg_value = 1 << 2;
			pr_warn("%s %d %d\n", __func__, device_type, mux_type);
		}
		mt6391_set_reg(codec_data, MT6397_AUDPREAMP_CON0, reg_value, 0x0000001c);
		break;
	case MT6391_DEV_IN_PREAMP_R:
		if (mux_type == MT6391_MUX_IN_MIC1) {
			reg_value = 1 << 5;
		} else if (mux_type == MT6391_MUX_IN_MIC2) {
			reg_value = 2 << 5;
		} else if (mux_type == MT6391_MUX_IN_MIC3) {
			reg_value = 3 << 5;
		} else {
			reg_value = 1 << 5;
			pr_warn("%s %d %d\n", __func__, device_type, mux_type);
		}
		mt6391_set_reg(codec_data, MT6397_AUDPREAMP_CON0, reg_value, 0x000000e0);
		break;
	case MT6391_DEV_IN_ADC1:
		if (mux_type == MT6391_MUX_IN_MIC1) {
			reg_value = 1 << 2;
		} else if (mux_type == MT6391_MUX_IN_PREAMP_1) {
			reg_value = 4 << 2;
		} else if (mux_type == MT6391_MUX_IN_LEVEL_SHIFT_BUFFER) {
			reg_value = 5 << 2;
		} else {
			reg_value = 1 << 2;
			pr_warn("%s %d %d\n", __func__, device_type, mux_type);
		}
		mt6391_set_reg(codec_data, MT6397_AUDADC_CON0, reg_value, 0x0000001c);
		break;
	case MT6391_DEV_IN_ADC2:
		if (mux_type == MT6391_MUX_IN_MIC1) {
			reg_value = 1 << 5;
		} else if (mux_type == MT6391_MUX_IN_PREAMP_2) {
			reg_value = 4 << 5;
		} else if (mux_type == MT6391_MUX_IN_LEVEL_SHIFT_BUFFER) {
			reg_value = 5 << 5;
		} else {
			reg_value = 1 << 5;
			pr_warn("%s %d %d\n", __func__, device_type, mux_type);
		}
		mt6391_set_reg(codec_data, MT6397_AUDADC_CON0, reg_value, 0x000000e0);
		break;
	default:
		break;
	}
}

static void mt6391_turn_on_dac(struct mt6391_priv *codec_data)
{
	uint32_t rate = codec_data->sample_rate[MT6391_ADDA_DAC];

	pr_debug("%s dac_sample_rate = %d\n", __func__, rate);
	mt6391_set_reg(codec_data, MT6397_AFE_PMIC_NEWIF_CFG0,
		       (mt6391_get_dl_input_mode(rate) << 12),
		       0xf000);
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0006, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON0, 0xc3a1, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0003, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x000b, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFE_DL_SDM_CON1, 0x001e, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFE_DL_SRC2_CON0_H,
		       0x0300 | (mt6391_get_dl_input_mode(rate) << 12),
		       0x0ffff);
	mt6391_set_reg(codec_data, MT6397_AFE_UL_DL_CON0, 0x007f, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFE_DL_SRC2_CON0_L, 0x1801, 0xffff);
}

static void mt6391_turn_off_dac(struct mt6391_priv *codec_data)
{
	pr_debug("%s\n", __func__);

	mt6391_set_reg(codec_data, MT6397_AFE_DL_SRC2_CON0_L, 0x1800, 0xffff);
	if (!mt6391_get_ul_status(codec_data))
		mt6391_set_reg(codec_data, MT6397_AFE_UL_DL_CON0, 0x0000, 0xffff);
}

static void mt6391_spk_auto_trim_offset(struct mt6391_priv *codec_data)
{
	uint32_t wait_for_ready = 0;
	uint32_t reg = 0;
	uint32_t chip_version = 0;
	int retry_count = 50;

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	/* enable VA28 , VA 33 VBAT ref , set dc */
	mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
	/* set ACC mode  enable NVREF */
	mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0xE000, 0xE000);
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff);	/* RG DEV ck on */
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);	/* NCP on */
	udelay(200);
	/* ZCD setting gain step gain and enable */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON0, 0x0301, 0xffff);
	/* audio bias adjustment */
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
	/* set DUDIV gain ,iv buffer gain */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON4, 0x0505, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUD_IV_CFG0, 0x1111, 0xffff);	/* set IV buffer on */
	udelay(100);
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001);	/* reset docoder */
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x000f, 0xffff);	/* power on DAC */
	udelay(100);
	mt6391_set_mux(codec_data, MT6391_DEV_OUT_SPEAKERR, MT6391_MUX_AUDIO);
	mt6391_set_mux(codec_data, MT6391_DEV_OUT_SPEAKERL, MT6391_MUX_AUDIO);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0000, 0x0007);	/* set Mux */
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);

	/* disable the software register mode */
	mt6391_set_reg(codec_data, MT6397_SPK_CON1, 0, 0x7f00);
	/* disable the software register mode */
	mt6391_set_reg(codec_data, MT6397_SPK_CON4, 0, 0x7f00);
	/* Choose new mode for trim (E2 Trim) */
	mt6391_set_reg(codec_data, MT6397_SPK_CON9, 0x0018, 0xffff);
	mt6391_set_reg(codec_data, MT6397_SPK_CON0, 0x0008, 0xffff);	/* Enable auto trim */
	mt6391_set_reg(codec_data, MT6397_SPK_CON3, 0x0008, 0xffff);	/* Enable auto trim R */
	mt6391_set_reg(codec_data, MT6397_SPK_CON0, 0x3000, 0xf000);	/* set gain */
	mt6391_set_reg(codec_data, MT6397_SPK_CON3, 0x3000, 0xf000);	/* set gain R */
	mt6391_set_reg(codec_data, MT6397_SPK_CON9, 0x0100, 0x0f00);	/* set gain L */
	mt6391_set_reg(codec_data, MT6397_SPK_CON5, (0x1 << 11), 0x7800);	/* set gain R */
	/* Enable amplifier & auto trim */
	mt6391_set_reg(codec_data, MT6397_SPK_CON0, 0x0001, 0x0001);
	/* Enable amplifier & auto trim R */
	mt6391_set_reg(codec_data, MT6397_SPK_CON3, 0x0001, 0x0001);

	/* empirical data shows it usually takes 13ms to be ready */
	usleep_range(15000, 16000);

	do {
		wait_for_ready = mt6391_get_reg(codec_data, MT6397_SPK_CON1);
		wait_for_ready = ((wait_for_ready & 0x8000) >> 15);

		if (wait_for_ready) {
			wait_for_ready = mt6391_get_reg(codec_data, MT6397_SPK_CON4);
			wait_for_ready = ((wait_for_ready & 0x8000) >> 15);
			if (wait_for_ready)
				break;
		}

		pr_debug("%s sleep\n", __func__);
		udelay(100);
	} while (retry_count--);

	if (wait_for_ready)
		pr_info("%s done retry_count = %d\n", __func__, retry_count);
	else
		pr_warn("%s fail\n", __func__);

	mt6391_set_reg(codec_data, MT6397_SPK_CON9, 0x0, 0xffff);
	mt6391_set_reg(codec_data, MT6397_SPK_CON5, 0, 0x7800);	/* set gain R */
	mt6391_set_reg(codec_data, MT6397_SPK_CON0, 0x0000, 0x0001);
	mt6391_set_reg(codec_data, MT6397_SPK_CON3, 0x0000, 0x0001);

	/* get trim offset result */
	pr_debug("%s GetSPKAutoTrimOffset\n", __func__);
	mt6391_set_reg(codec_data, MT6397_TEST_CON0, 0x0805, 0xffff);
	reg = mt6391_get_reg(codec_data, MT6397_TEST_OUT_L);
	codec_data->ispkl_trim = ((reg >> 0) & 0xf);
	mt6391_set_reg(codec_data, MT6397_TEST_CON0, 0x0806, 0xffff);
	reg = mt6391_get_reg(codec_data, MT6397_TEST_OUT_L);
	codec_data->ispkl_trim |= (((reg >> 0) & 0x1) << 4);
	codec_data->spkl_polarity = ((reg >> 1) & 0x1);
	mt6391_set_reg(codec_data, MT6397_TEST_CON0, 0x080E, 0xffff);
	reg = mt6391_get_reg(codec_data, MT6397_TEST_OUT_L);
	codec_data->ispkr_trim = ((reg >> 0) & 0xf);
	mt6391_set_reg(codec_data, MT6397_TEST_CON0, 0x080F, 0xffff);
	reg = mt6391_get_reg(codec_data, MT6397_TEST_OUT_L);
	codec_data->ispkr_trim |= (((reg >> 0) & 0x1) << 4);
	codec_data->spkr_polarity = ((reg >> 1) & 0x1);

#if defined(USE_MT6397_REGMAP)
	chip_version = mt6391_get_reg(codec_data, MT6397_CID);
#elif defined(USE_PMIC_WRAP_DRIVER)
	chip_version = upmu_get_cid();
#endif

	if (chip_version == PMIC6397_E1_CID_CODE) {
		pr_debug("%s PMIC is MT6397 E1, set speaker R trim code to 0\n", __func__);
		codec_data->ispkr_trim = 0;
		codec_data->spkr_polarity = 0;
	}

	pr_debug("%s spkl_polarity = %d ispkl_trim = 0x%x\n",
		 __func__, codec_data->spkl_polarity, codec_data->ispkl_trim);
	pr_debug("%s spkr_polarity = %d ispkr_trim = 0x%x\n",
		 __func__, codec_data->spkr_polarity, codec_data->ispkr_trim);

	/* turn off speaker after trim */
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6391_set_reg(codec_data, MT6397_SPK_CON0, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_SPK_CON3, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_SPK_CON11, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_ZCD_CON0, 0x0101, 0xffff);

	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);	/* RG DEV ck on */
	mt6391_set_reg(codec_data, MT6397_AUD_IV_CFG0, 0x0000, 0xffff);	/* NCP on */
	/* Audio headset power on */
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
	/* mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG1, 0x0000, 0x0100); */

	mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);	/* fix me */
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0x0000, 0x6000);
	mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
}

static void mt6391_get_hp_trim_offset(struct mt6391_priv *codec_data)
{
	uint32_t reg1 = 0, reg2 = 0;
	bool trim_enable = 0;

	/* get to check if trim happen */
	reg1 = mt6391_get_reg(codec_data, MT6397_TRIM_ADDRESS1);
	reg2 = mt6391_get_reg(codec_data, MT6397_TRIM_ADDRESS2);
	pr_debug("%s reg1 = 0x%x reg2 = 0x%x\n", __func__, reg1, reg2);

	trim_enable = (reg1 >> 11) & 1;
	if (trim_enable == 0) {
		codec_data->hpl_trim = 2;
		codec_data->hpl_fine_trim = 0;
		codec_data->hpr_trim = 2;
		codec_data->hpr_fine_trim = 0;
		codec_data->iv_hpl_trim = 3;
		codec_data->iv_hpl_fine_trim = 0;
		codec_data->iv_hpr_trim = 3;
		codec_data->iv_hpr_fine_trim = 0;
	} else {
		codec_data->hpl_trim = ((reg1 >> 3) & 0xf);
		codec_data->hpr_trim = ((reg1 >> 7) & 0xf);
		codec_data->hpl_fine_trim = ((reg1 >> 12) & 0x3);
		codec_data->hpr_fine_trim = ((reg1 >> 14) & 0x3);
		codec_data->iv_hpl_trim = ((reg2 >> 0) & 0xf);
		codec_data->iv_hpr_trim = ((reg2 >> 4) & 0xf);
		codec_data->iv_hpl_fine_trim = ((reg2 >> 8) & 0x3);
		codec_data->iv_hpr_fine_trim = ((reg2 >> 10) & 0x3);
	}

	pr_debug("%s trim_enable = %d reg1 = 0x%x reg2 = 0x%x\n", __func__, trim_enable, reg1,
		 reg2);
	pr_debug("%s hpl_trim = 0x%x hpl_fine_trim = 0x%x hpr_trim = 0x%x hpr_fine_trim = 0x%x\n",
		 __func__, codec_data->hpl_trim, codec_data->hpl_fine_trim, codec_data->hpr_trim,
		 codec_data->hpr_fine_trim);
	pr_debug("%s iv_hpl_trim = 0x%x iv_hpl_fine_trim = 0x%x\n", __func__,
		 codec_data->iv_hpl_trim, codec_data->iv_hpl_fine_trim);
	pr_debug("%s iv_hpr_trim = 0x%x iv_hpr_fine_trim = 0x%x\n", __func__,
		 codec_data->iv_hpr_trim, codec_data->iv_hpr_fine_trim);
}

static void mt6391_set_hp_trim_offset(struct mt6391_priv *codec_data)
{
	uint32_t reg_value = 0;

	pr_debug("%s", __func__);
	reg_value |= 1 << 8;	/* enable trim function */
	reg_value |= codec_data->hpr_fine_trim << 11;
	reg_value |= codec_data->hpl_fine_trim << 9;
	reg_value |= codec_data->hpr_trim << 4;
	reg_value |= codec_data->hpl_trim;
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG3, reg_value, 0x1fff);
}

static void mt6391_set_spk_trim_offset(struct mt6391_priv *codec_data)
{
	uint32_t reg_value = 0;

	reg_value |= 1 << 14;	/* enable trim function */
	reg_value |= codec_data->spkl_polarity << 13;	/* polarity */
	reg_value |= codec_data->ispkl_trim << 8;	/* polarity */
	pr_debug("%s reg_value = 0x%x\n", __func__, reg_value);
	mt6391_set_reg(codec_data, MT6397_SPK_CON1, reg_value, 0x7f00);
	reg_value = 0;
	reg_value |= 1 << 14;	/* enable trim function */
	reg_value |= codec_data->spkr_polarity << 13;	/* polarity */
	reg_value |= codec_data->ispkr_trim << 8;	/* polarity */
	pr_debug("%s reg_value = 0x%x\n", __func__, reg_value);
	mt6391_set_reg(codec_data, MT6397_SPK_CON4, reg_value, 0x7f00);
}

static void mt6391_set_iv_hp_trim_offset(struct mt6391_priv *codec_data)
{
	uint32_t reg_value = 0;

	reg_value |= 1 << 8;	/* enable trim function */

	if ((codec_data->hpr_fine_trim == 0) || (codec_data->hpl_fine_trim == 0))
		codec_data->iv_hpr_fine_trim = 0;
	else
		codec_data->iv_hpr_fine_trim = 2;

	reg_value |= codec_data->iv_hpr_fine_trim << 11;

	if ((codec_data->hpr_fine_trim == 0) || (codec_data->hpl_fine_trim == 0))
		codec_data->iv_hpl_fine_trim = 0;
	else
		codec_data->iv_hpl_fine_trim = 2;

	reg_value |= codec_data->iv_hpl_fine_trim << 9;

	reg_value |= codec_data->iv_hpr_trim << 4;
	reg_value |= codec_data->iv_hpl_trim;
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG3, reg_value, 0x1fff);
}

static void mt6391_turn_on_headphone_amp(struct mt6391_priv *codec_data)
{
	int gain_l = codec_data->device_volume[MT6391_VOL_HPOUTL];
	int gain_r = codec_data->device_volume[MT6391_VOL_HPOUTR];

	pr_debug("%s\n", __func__);

	if (codec_data->device_power[MT6391_DEV_OUT_HEADSETL]) {
		pr_debug("%s turn on already\n", __func__);
		return;
	}

	if (!mt6391_get_dl_status(codec_data))
		mt6391_turn_on_dac(codec_data);

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6391_set_hp_trim_offset(codec_data);
	/* enable VA28 , VA 33 VBAT ref , set dc */
	mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
	/* set ACC mode      enable NVREF */
	mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0xE000, 0xE000);
	/* RG DEV ck on */
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON0, 0x102b, 0xffff);
	/* NCP on */
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);

	udelay(200);

	mt6391_set_reg(codec_data, MT6397_ZCD_CON0, 0x0101, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDACCDEPOP_CFG0, 0x0030, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0008, 0xffff);
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
	mt6391_set_reg(codec_data, MT6397_ZCD_CON2, 0xc0c, 0xf0f);
	mt6391_set_reg(codec_data, MT6397_ZCD_CON3, 0xf, 0xf);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG1, 0x0900, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG2, 0x0082, 0xffff);

	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0009, 0xffff);

	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG1, 0x0940, 0xffff);
	udelay(200);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x000F, 0xffff);

	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG1, 0x0100, 0xffff);
	udelay(100);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG2, 0x0022, 0xffff);
	mt6391_set_reg(codec_data, MT6397_ZCD_CON2, ((gain_r << 8) | gain_l), 0xf0f);
	udelay(100);

	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001);
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x000F, 0xffff);
	udelay(100);

	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0006, 0x0007);
	mt6391_set_mux(codec_data, MT6391_DEV_OUT_HEADSETR, MT6391_MUX_AUDIO);
	mt6391_set_mux(codec_data, MT6391_DEV_OUT_HEADSETL, MT6391_MUX_AUDIO);

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);

	pr_debug("%s done\n", __func__);
}

static void mt6391_turn_off_headphone_amp(struct mt6391_priv *codec_data)
{
	pr_debug("%s\n", __func__);

	if (codec_data->device_power[MT6391_DEV_OUT_HEADSETL]) {
		pr_debug("%s still on\n", __func__);
		return;
	}

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6391_set_reg(codec_data, MT6397_ZCD_CON2, 0x0c0c, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0000, 0x1fe7);
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff); /* RG DEV ck off; */
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);	/* NCP off */
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);

	if (mt6391_get_ul_status(codec_data) == false)
		mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);

	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0x0000, 0x6000);

	if (mt6391_get_ul_status(codec_data) == false)
		mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);

	if (mt6391_get_dl_status(codec_data) == false)
		mt6391_turn_off_dac(codec_data);

	pr_debug("%s done\n", __func__);
}

static void mt6391_turn_on_voice_amp(struct mt6391_priv *codec_data)
{
	int gain = codec_data->device_volume[MT6391_VOL_HSOUTL];

	if (codec_data->device_power[MT6391_DEV_OUT_EARPIECEL]) {
		pr_debug("%s turn on already\n", __func__);
		return;
	}

	if (!mt6391_get_dl_status(codec_data))
		mt6391_turn_on_dac(codec_data);

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	/* enable VA28 , VA 33 VBAT ref , set dc */
	mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
	/* set ACC mode  enable NVREF */
	mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
	/* enable LDO ; separate for UL  DL LDO */
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0xE000, 0xE000);
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff); /* RG DEV ck on */
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff); /* NCP on */
	/* usleep(1 * 1000); */

	/* ZCD setting gain step gain and enable */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON0, 0x0201, 0xffff);
	/* select charge current l; fix me */
	mt6391_set_reg(codec_data, MT6397_AUDACCDEPOP_CFG0, 0x0030, 0xffff);
	/* set voice playback with headset */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0008, 0xffff);
	/* audio bias adjustment */
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);

	/* handset gain , minimun gain */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON3, 0xf, 0xf);
	/* short HS to vcm and HS output stability enhance */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG2, 0x00A2, 0xffff);
	/* handset gain , minimun gain */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0001, 0xffff);
	/* short HS to vcm and HS output stability enhance */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG2, 0x0022, 0xffff);

	/* handset gain , normal gain */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON3, gain, 0xf);
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001); /* reset decoder */
	/* power on audio DAC right channels */
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x0009, 0xffff);
	/* usleep(1000); */
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);

	mt6391_set_mux(codec_data, MT6391_DEV_OUT_EARPIECEL, MT6391_MUX_VOICE);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0001, 0x0001);	/* mux selection */
	/* usleep(1000); */
}

static void mt6391_turn_off_voice_amp(struct mt6391_priv *codec_data)
{
	if (codec_data->device_power[MT6391_DEV_OUT_EARPIECEL]) {
		pr_debug("%s still on\n", __func__);
		return;
	}

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	/* short HS to vcm and HS output stability enhance */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG2, 0x0022, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0880, 0xffff);
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff); /* RG DEV ck off */
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);	/* NCP off */
	/* Audio headset power off */
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	/* short HS to vcm and HS output stability EnhanceParasNum */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG2, 0x0022, 0xffff);

	if (mt6391_get_ul_status(codec_data) == false)
		mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);

	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0x0000, 0x6000);

	if (mt6391_get_ul_status(codec_data) == false)
		mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);

	if (!mt6391_get_dl_status(codec_data))
		mt6391_turn_off_dac(codec_data);
}

static void mt6391_turn_on_speaker_amp(struct mt6391_priv *codec_data)
{
	int gain_l = codec_data->device_volume[MT6391_VOL_SPKL];
	int gain_r = codec_data->device_volume[MT6391_VOL_SPKR];

	pr_debug("%s\n", __func__);

	if (codec_data->device_power[MT6391_DEV_OUT_SPEAKERL]) {
		pr_debug("%s turn on already\n", __func__);
		return;
	}

	if (mt6391_get_dl_status(codec_data) == false)
		mt6391_turn_on_dac(codec_data);

	/* here pmic analog control */
	mt6391_control_top_clk(codec_data, 0x0604, true);	/* enable SPK related CLK */
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6391_set_spk_trim_offset(codec_data);
	/* enable VA28 , VA 33 VBAT ref , set dc */
	mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
	/* set ACC mode  enable NVREF */
	mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0xE000, 0xE000);
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff);	/* RG DEV ck on */
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);	/* NCP on */
	udelay(200);

	/* ZCD setting gain step gain and enable */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON0, 0x0301, 0xffff);
	/* audio bias adjustment */
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
	/* set DUDIV gain ,iv buffer gain */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON4, 0x0505, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUD_IV_CFG0, 0x1111, 0xffff);	/* set IV buffer on */
	udelay(100);
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001); /* reset docoder */
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x000f, 0xffff);	/* power on DAC */
	udelay(100);

	mt6391_set_mux(codec_data, MT6391_DEV_OUT_SPEAKERR, MT6391_MUX_AUDIO);
	mt6391_set_mux(codec_data, MT6391_DEV_OUT_SPEAKERL, MT6391_MUX_AUDIO);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0000, 0x0007);	/* set Mux */

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);

	mt6391_set_reg(codec_data, MT6397_SPK_CON9, 0x0100, 0x0f00);	/* set gain L */
	mt6391_set_reg(codec_data, MT6397_SPK_CON5, (0x1 << 11), 0x7800);	/* set gain R */

	if (codec_data->speaker_channel_sel == MT6391_CHANNEL_SEL_STEREO) {
		mt6391_set_reg(codec_data, MT6397_SPK_CON0,
			       0x3001 | (codec_data->speaker_mode << 2),
			       0xffff);
		mt6391_set_reg(codec_data, MT6397_SPK_CON3,
			       0x3001 | (codec_data->speaker_mode << 2),
			       0xffff);
		mt6391_set_reg(codec_data, MT6397_SPK_CON2, 0x0014, 0xffff);
		mt6391_set_reg(codec_data, MT6397_SPK_CON5, 0x0014, 0x07ff);
		/* SPK gain setting */
		mt6391_set_reg(codec_data, MT6397_SPK_CON9, gain_l << 8, 0xf00);
		mt6391_set_reg(codec_data, MT6397_SPK_CON5, gain_r << 11, 0x7800);
	} else if (codec_data->speaker_channel_sel == MT6391_CHANNEL_SEL_MONO_LEFT) {
		mt6391_set_reg(codec_data, MT6397_SPK_CON0,
			       0x3001 | (codec_data->speaker_mode << 2),
			       0xffff);
		mt6391_set_reg(codec_data, MT6397_SPK_CON2, 0x0014, 0xffff);
		/* SPK gain setting */
		mt6391_set_reg(codec_data, MT6397_SPK_CON9, gain_l << 8, 0xf00);
	} else if (codec_data->speaker_channel_sel == MT6391_CHANNEL_SEL_MONO_RIGHT) {
		mt6391_set_reg(codec_data, MT6397_SPK_CON3,
			       0x3001 | (codec_data->speaker_mode << 2),
			       0xffff);
		mt6391_set_reg(codec_data, MT6397_SPK_CON5, 0x0014, 0x07ff);
		/* SPK gain setting */
		mt6391_set_reg(codec_data, MT6397_SPK_CON5, gain_r << 11, 0x7800);
	} else {
		pr_err("%s unexpected condition\n", __func__);
	}

	/* spk output stage enabke and enable */
	mt6391_set_reg(codec_data, MT6397_SPK_CON11, 0x0f00, 0xffff);
	usleep_range(4000, 5000);

	pr_debug("%s done\n", __func__);
}

static void mt6391_turn_off_speaker_amp(struct mt6391_priv *codec_data)
{
	pr_debug("%s\n", __func__);

	if (codec_data->device_power[MT6391_DEV_OUT_SPEAKERL]) {
		pr_debug("%s still on\n", __func__);
		return;
	}

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6391_set_reg(codec_data, MT6397_SPK_CON0, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_SPK_CON3, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_SPK_CON11, 0x0000, 0xffff);
	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);	/* RG DEV ck on */
	mt6391_set_reg(codec_data, MT6397_AUD_IV_CFG0, 0x0000, 0xffff);	/* NCP on */
	/* Audio headset power on */
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
	/* mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG1, 0x0000, 0x0100); */
	if (mt6391_get_ul_status(codec_data) == false)
		mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);

	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);	/* fix me */
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0x0000, 0x6000);
	if (mt6391_get_ul_status(codec_data) == false)
		mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
	mt6391_control_top_clk(codec_data, 0x0604, false);	/* disable SPK related CLK */
	if (mt6391_get_dl_status(codec_data) == false)
		mt6391_turn_off_dac(codec_data);

	/* temp solution, set MT6397_ZCD_CON0 to 0x101 for pop noise */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON0, 0x0101, 0xffff);

	pr_debug("%s done\n", __func__);
}

static void mt6391_turn_on_headset_speaker_amp(struct mt6391_priv *codec_data)
{
	int gain_hpl = codec_data->device_volume[MT6391_VOL_HPOUTL];
	int gain_hpr = codec_data->device_volume[MT6391_VOL_HPOUTR];
	int gain_spkl = codec_data->device_volume[MT6391_VOL_SPKL];
	int gain_spkr = codec_data->device_volume[MT6391_VOL_SPKR];

	pr_debug("%s\n", __func__);

	if (codec_data->device_power[MT6391_DEV_OUT_SPEAKER_HEADSET_L]) {
		pr_debug("%s turn on already\n", __func__);
		return;
	}

	if (!mt6391_get_dl_status(codec_data))
		mt6391_turn_on_dac(codec_data);

	/* here pmic analog control */
	mt6391_control_top_clk(codec_data, 0x0604, true);	/* enable SPK related CLK */
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6391_set_hp_trim_offset(codec_data);
	mt6391_set_iv_hp_trim_offset(codec_data);
	mt6391_set_spk_trim_offset(codec_data);

	/* enable VA28 , VA 33 VBAT ref , set dc */
	mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
	/* set ACC mode  enable NVREF */
	mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x000C, 0xffff);
	/* enable LDO ; fix me , separate for UL  DL LDO */
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0xE000, 0xE000);
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON0, 0x102B, 0xffff);	/* RG DEV ck on */
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);	/* NCP on */
	udelay(200);

	/* ZCD setting gain step gain and enable */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON0, 0x0301, 0xffff);
	/* select charge current ; fix me */
	mt6391_set_reg(codec_data, MT6397_AUDACCDEPOP_CFG0, 0x0030, 0xffff);
	/* set voice playback with headset */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0008, 0xffff);
	/* audio bias adjustment */
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x0552, 0xffff);
	mt6391_set_reg(codec_data, MT6397_ZCD_CON2, 0xc0c, 0xf0f);	/* HP PGA gain */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON3, 0xf, 0xf);	/* HS PGA gain */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG1, 0x0900, 0xffff);	/* HP enhance */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG2, 0x0082, 0xffff);	/* HS enahnce */
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0009, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG1, 0x0940, 0xffff);	/* HP vcm short */
	udelay(200);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x000F, 0xffff);	/* HP power on */

	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG1, 0x0100, 0xffff);	/* HP vcm not short */
	udelay(100);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG2, 0x0022, 0xffff);	/* HS VCM not short */

	/* HP PGA gain */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON2, ((gain_hpr << 8) | gain_hpl), 0xf0f);
	udelay(100);
	mt6391_set_reg(codec_data, MT6397_ZCD_CON4, 0x0505, 0xffff);	/* HP PGA gain */

	mt6391_set_reg(codec_data, MT6397_AUD_IV_CFG0, 0x1111, 0xffff);	/* set IV buffer on */
	udelay(100);
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0001, 0x0001); /* reset docoder */
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x000F, 0xffff);	/* power on DAC */
	udelay(100);
	mt6391_set_mux(codec_data, MT6391_DEV_OUT_SPEAKERR, MT6391_MUX_AUDIO);
	mt6391_set_mux(codec_data, MT6391_DEV_OUT_SPEAKERL, MT6391_MUX_AUDIO);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x1106, 0x1106);	/* set headhpone mux */

	mt6391_set_reg(codec_data, MT6397_SPK_CON9, 1 << 13, 1 << 13);
	mt6391_set_reg(codec_data, MT6397_SPK_CON9, 0x0100, 0x0f00);	/* set gain L */
	mt6391_set_reg(codec_data, MT6397_SPK_CON5, (0x1 << 11), 0x7800);	/* set gain R */

	/* speaker gain setting , trim enable , spk enable , class AB or D */
	mt6391_set_reg(codec_data, MT6397_SPK_CON0,
		       0x3001 | (codec_data->speaker_mode << 2),
		       0xffff);
	/* speaker gain setting , trim enable , spk enable , class AB or D */
	mt6391_set_reg(codec_data, MT6397_SPK_CON3,
		       0x3001 | (codec_data->speaker_mode << 2),
		       0xffff);
	/* speaker gain setting , trim enable , spk enable , class AB or D */
	mt6391_set_reg(codec_data, MT6397_SPK_CON2, 0x0014, 0xffff);
	mt6391_set_reg(codec_data, MT6397_SPK_CON5, 0x0014, 0x07ff);

	/* SPK-L gain setting */
	mt6391_set_reg(codec_data, MT6397_SPK_CON9, gain_spkl << 8, 0xf00);
	/* SPK-R gain setting */
	mt6391_set_reg(codec_data, MT6397_SPK_CON5, gain_spkr << 11, 0x7800);
	/* spk output stage enabke and enableAudioClockPortDST */
	mt6391_set_reg(codec_data, MT6397_SPK_CON11, 0x0f00, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
	usleep_range(4000, 5000);
	mt6391_set_reg(codec_data, MT6397_SPK_CON9, 0 << 13, 1 << 13);

	pr_debug("%s done\n", __func__);
}

static void mt6391_turn_off_headset_speaker_amp(struct mt6391_priv *codec_data)
{
	pr_debug("%s\n", __func__);

	if (codec_data->device_power[MT6391_DEV_OUT_SPEAKER_HEADSET_L]) {
		pr_debug("%s still on\n", __func__);
		return;
	}

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6391_set_reg(codec_data, MT6397_SPK_CON0, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_SPK_CON3, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_SPK_CON11, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_ZCD_CON2, 0x0C0C, 0x0f0f);

	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0000, 0x0007);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0000, 0x1fe0);
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	mt6391_set_reg(codec_data, MT6397_AUD_IV_CFG0, 0x0010, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG1, 0x0000, 0x0100);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG2, 0x0000, 0x0080);

	if (!mt6391_get_ul_status(codec_data))
		mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);

	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0x0000, 0x6000);
	if (!mt6391_get_ul_status(codec_data))
		mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
	mt6391_control_top_clk(codec_data, 0x0604, false);	/* disable SPK related CLK */
	if (!mt6391_get_dl_status(codec_data))
		mt6391_turn_off_dac(codec_data);

	/* ZCD setting gain step gain and enable */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON0, 0x0101, 0xffff);

	pr_debug("%s done\n", __func__);
}

static void mt6391_check_and_turn_off_all_amps(struct mt6391_priv *codec_data)
{
	if (codec_data->device_power[MT6391_DEV_OUT_EARPIECEL]) {
		mt6391_turn_off_voice_amp(codec_data);
		mt6391_ana_clk_off(codec_data);
		codec_data->device_power[MT6391_DEV_OUT_EARPIECEL] = false;
	}

	if (codec_data->device_power[MT6391_DEV_OUT_SPEAKERL]) {
		mt6391_turn_off_speaker_amp(codec_data);
		mt6391_ana_clk_off(codec_data);
		codec_data->device_power[MT6391_DEV_OUT_SPEAKERL] = false;
	}

	if (codec_data->device_power[MT6391_DEV_OUT_HEADSETL]) {
		mt6391_turn_off_headphone_amp(codec_data);
		mt6391_ana_clk_off(codec_data);
		codec_data->device_power[MT6391_DEV_OUT_HEADSETL] = false;
	}

	if (codec_data->device_power[MT6391_DEV_OUT_SPEAKER_HEADSET_L]) {
		mt6391_turn_off_headset_speaker_amp(codec_data);
		mt6391_ana_clk_off(codec_data);
		codec_data->device_power[MT6391_DEV_OUT_SPEAKER_HEADSET_L] = false;
	}
}

static void mt6391_turn_on_dmic(struct mt6391_priv *codec_data)
{
	uint32_t rate = codec_data->sample_rate[MT6391_ADDA_ADC];
	uint32_t warmup_time = codec_data->dmic_warmup_time_us;
	/* pmic digital part */
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0002);
	mt6391_set_reg(codec_data, MT6397_AFE_UL_SRC_CON0_L, 0x0000, 0xffff);

	mt6391_control_top_clk(codec_data, 0x0003, true);
	mt6391_set_reg(codec_data, MT6397_ANA_AUDIO_TOP_CON0, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFE_UL_SRC_CON0_H,
		       0x00e0 | mt6391_get_ul_voice_mode(rate),
		       0xffff);
	mt6391_set_reg(codec_data, MT6397_AFE_UL_DL_CON0, 0x007f, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFE_UL_SRC_CON0_L, 0x0023, 0xffff);

	/* AudioMachineDevice */
	mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x0000, 0x0002);
	mt6391_set_reg(codec_data, MT6397_AUDDIGMI_CON0, 0x0181, 0xffff);

	if (warmup_time > 0)
		usleep_range(warmup_time, warmup_time + 1);
}

static void mt6391_turn_off_dmic(struct mt6391_priv *codec_data)
{
	/* AudioMachineDevice */
	mt6391_set_reg(codec_data, MT6397_AUDDIGMI_CON0, 0x0080, 0xffff);
	/* pmic digital part */
	mt6391_set_reg(codec_data, MT6397_AFE_UL_SRC_CON0_H, 0x0000, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
	if (mt6391_get_dl_status(codec_data) == false) {
		mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x0002, 0x0002);
		mt6391_set_reg(codec_data, MT6397_AFE_UL_DL_CON0, 0x0000, 0xffff);
	}
}

static void mt6391_turn_on_adc(struct mt6391_priv *codec_data, int adc_type)
{
	if (!mt6391_get_adc_status(codec_data)) {
		uint32_t rate = codec_data->sample_rate[MT6391_ADDA_ADC];
		uint32_t warmup_time = codec_data->adc_warmup_time_us;
		/* pmic digital part */
		mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0002);
		mt6391_set_reg(codec_data, MT6397_AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
		mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0002, 0x0002);
		mt6391_control_top_clk(codec_data, 0x0003, true);
		mt6391_set_reg(codec_data, MT6397_ANA_AUDIO_TOP_CON0, 0x0000, 0xffff);
		mt6391_set_reg(codec_data, MT6397_AFE_UL_SRC_CON0_H,
			    0x0000 | mt6391_get_ul_voice_mode(rate), 0xffff);
		mt6391_set_reg(codec_data, MT6397_AFE_UL_DL_CON0, 0x007f, 0xffff);
		mt6391_set_reg(codec_data, MT6397_AFE_UL_SRC_CON0_L, 0x0001, 0xffff);

		/* pmic analog part */
		mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x000c, 0xffff);
		mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0D92, 0xffff);
		mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0x9000, 0x9000);

		mt6391_set_mux(codec_data, MT6391_DEV_IN_ADC1, MT6391_MUX_IN_PREAMP_1);
		mt6391_set_mux(codec_data, MT6391_DEV_IN_ADC2, MT6391_MUX_IN_PREAMP_2);

		/* open power */
		mt6391_set_reg(codec_data, MT6397_AUDPREAMP_CON0, 0x0003, 0x0003);
		mt6391_set_reg(codec_data, MT6397_AUDADC_CON0, 0x0093, 0xffff);
		mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON0, 0x102B, 0x102B);
		mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0000, 0xffff);
		mt6391_set_reg(codec_data, MT6397_AUDDIGMI_CON0, 0x0180, 0x0180);

		if (warmup_time > 0)
			usleep_range(warmup_time, warmup_time + 1);
	}
}

static void mt6391_turn_off_adc(struct mt6391_priv *codec_data, int adc_type)
{
	if (!mt6391_get_adc_status(codec_data)) {
		/* pmic analog part */
		mt6391_set_reg(codec_data, MT6397_AUDPREAMP_CON0, 0x0000, 0x0003);
		mt6391_set_reg(codec_data, MT6397_AUDADC_CON0, 0x00B4, 0xffff);
		mt6391_set_reg(codec_data, MT6397_AUDDIGMI_CON0, 0x0080, 0xffff);
		mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0x0000, 0x1000);
		mt6391_set_reg(codec_data, MT6397_AUDLSBUF_CON0, 0x0000, 0x0003);
		if (mt6391_get_dl_status(codec_data) == false)
			mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);

		if (mt6391_get_dl_status(codec_data) == false)
			mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);

		mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0002);
		/* pmic digital part */

		mt6391_set_reg(codec_data, MT6397_AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
		if (!mt6391_get_dl_status(codec_data))
			mt6391_set_reg(codec_data, MT6397_AFE_UL_DL_CON0, 0x0000, 0xffff);
	}
}


/* snd kcontrol implementation */
static int mt6391_audio_amp_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    codec_data->device_power[MT6391_DEV_OUT_HEADSETL] ? 1 : 0;
	return 0;
}

static int mt6391_audio_amp_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	mutex_lock(&codec_data->ctrl_mutex);

	pr_debug("%s %ld\n ", __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] &&
	    !codec_data->device_power[MT6391_DEV_OUT_HEADSETL]) {
		mt6391_check_and_turn_off_all_amps(codec_data);
		mt6391_ana_clk_on(codec_data);
		mt6391_turn_on_headphone_amp(codec_data);
		codec_data->device_power[MT6391_DEV_OUT_HEADSETL] = true;
	} else if (!ucontrol->value.integer.value[0] &&
		   codec_data->device_power[MT6391_DEV_OUT_HEADSETL]) {
		codec_data->device_power[MT6391_DEV_OUT_HEADSETL] = false;
		mt6391_turn_off_headphone_amp(codec_data);
		mt6391_ana_clk_off(codec_data);
	}

	mutex_unlock(&codec_data->ctrl_mutex);
	return 0;
}

static int mt6391_voice_amp_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    codec_data->device_power[MT6391_DEV_OUT_EARPIECEL] ? 1 : 0;
	return 0;
}

static int mt6391_voice_amp_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	mutex_lock(&codec_data->ctrl_mutex);

	pr_debug("%s %ld\n ", __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] &&
	    !codec_data->device_power[MT6391_DEV_OUT_EARPIECEL]) {
		mt6391_check_and_turn_off_all_amps(codec_data);
		mt6391_ana_clk_on(codec_data);
		mt6391_turn_on_voice_amp(codec_data);
		codec_data->device_power[MT6391_DEV_OUT_EARPIECEL] = true;
	} else if (!ucontrol->value.integer.value[0] &&
		   codec_data->device_power[MT6391_DEV_OUT_EARPIECEL]) {
		codec_data->device_power[MT6391_DEV_OUT_EARPIECEL] = false;
		mt6391_turn_off_voice_amp(codec_data);
		mt6391_ana_clk_off(codec_data);
	}

	mutex_unlock(&codec_data->ctrl_mutex);
	return 0;
}

static int mt6391_speaker_amp_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    codec_data->device_power[MT6391_DEV_OUT_SPEAKERL] ? 1 : 0;
	return 0;
}

static int mt6391_speaker_amp_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	mutex_lock(&codec_data->ctrl_mutex);

	pr_debug("%s %ld\n ", __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] &&
	    !codec_data->device_power[MT6391_DEV_OUT_SPEAKERL]) {
		mt6391_check_and_turn_off_all_amps(codec_data);
		mt6391_ana_clk_on(codec_data);
		mt6391_turn_on_speaker_amp(codec_data);
		codec_data->device_power[MT6391_DEV_OUT_SPEAKERL] = true;
	} else if (!ucontrol->value.integer.value[0] &&
		   codec_data->device_power[MT6391_DEV_OUT_SPEAKERL]) {
		codec_data->device_power[MT6391_DEV_OUT_SPEAKERL] = false;
		mt6391_turn_off_speaker_amp(codec_data);
		mt6391_ana_clk_off(codec_data);
	}

	mutex_unlock(&codec_data->ctrl_mutex);
	return 0;
}

static int mt6391_headset_speaker_amp_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    codec_data->device_power[MT6391_DEV_OUT_SPEAKER_HEADSET_L] ? 1 : 0;
	return 0;
}

static int mt6391_headset_speaker_amp_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	mutex_lock(&codec_data->ctrl_mutex);

	pr_debug("%s %ld\n ", __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] &&
	    !codec_data->device_power[MT6391_DEV_OUT_SPEAKER_HEADSET_L]) {
		mt6391_check_and_turn_off_all_amps(codec_data);
		mt6391_ana_clk_on(codec_data);
		mt6391_turn_on_headset_speaker_amp(codec_data);
		codec_data->device_power[MT6391_DEV_OUT_SPEAKER_HEADSET_L] = true;
	} else if (!ucontrol->value.integer.value[0] &&
		   codec_data->device_power[MT6391_DEV_OUT_SPEAKER_HEADSET_L]) {
		codec_data->device_power[MT6391_DEV_OUT_SPEAKER_HEADSET_L] = false;
		mt6391_turn_off_headset_speaker_amp(codec_data);
		mt6391_ana_clk_off(codec_data);
	}

	mutex_unlock(&codec_data->ctrl_mutex);
	return 0;
}

static int mt6391_headset_pgal_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->device_volume[MT6391_VOL_HPOUTL];
	return 0;
}

static int mt6391_headset_pgal_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	int index = 0;

	pr_debug("%s\n", __func__);
	if (ucontrol->value.enumerated.item[0] > e->items) {
		pr_err("%s out of bound\n", __func__);
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6391_set_reg(codec_data, MT6397_ZCD_CON2, index, 0x0000000F);
	codec_data->device_volume[MT6391_VOL_HPOUTL] = index;
	return 0;
}

static int mt6391_headset_pgar_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->device_volume[MT6391_VOL_HPOUTR];
	return 0;
}

static int mt6391_headset_pgar_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	int index = 0;

	pr_debug("%s\n", __func__);

	if (ucontrol->value.enumerated.item[0] > e->items) {
		pr_err("%s out of bound\n", __func__);
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6391_set_reg(codec_data, MT6397_ZCD_CON2, index << 8, 0x000000F00);
	codec_data->device_volume[MT6391_VOL_HPOUTR] = index;
	return 0;
}

static int mt6391_handset_pga_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->device_volume[MT6391_VOL_HSOUTL];
	return 0;
}

static int mt6391_handset_pga_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	int index = 0;

	pr_debug("%s %ld\n", __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.enumerated.item[0] > e->items) {
		pr_err("%s out of bound\n", __func__);
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6391_set_reg(codec_data, MT6397_ZCD_CON3, index, 0xF);
	codec_data->device_volume[MT6391_VOL_HSOUTL] = index;
	return 0;
}

static int mt6391_speaker_pgal_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->device_volume[MT6391_VOL_SPKL];
	return 0;
}

static int mt6391_speaker_pgal_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	int index = 0;

	pr_debug("%s\n", __func__);

	if (ucontrol->value.enumerated.item[0] > e->items) {
		pr_err("%s out of bound\n", __func__);
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6391_set_reg(codec_data, MT6397_SPK_CON9, index << 8, 0x00000f00);
	codec_data->device_volume[MT6391_VOL_SPKL] = index;
	return 0;
}

static int mt6391_speaker_pgar_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->device_volume[MT6391_VOL_SPKR];
	return 0;
}

static int mt6391_speaker_pgar_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	int index = 0;

	pr_debug("%s\n", __func__);
	if (ucontrol->value.enumerated.item[0] > e->items) {
		pr_err("%s out of bound\n", __func__);
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6391_set_reg(codec_data, MT6397_SPK_CON5, index << 11, 0x00007800);
	codec_data->device_volume[MT6391_VOL_SPKR] = index;
	return 0;
}

static int mt6391_speaker_channel_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	pr_debug("%s\n", __func__);
	codec_data->speaker_channel_sel = ucontrol->value.integer.value[0];
	return 0;
}

static int mt6391_speaker_channel_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	pr_debug("%s = %d\n", __func__, codec_data->speaker_channel_sel);
	ucontrol->value.integer.value[0] = codec_data->speaker_channel_sel;
	return 0;
}

static int mt6391_speaker_oc_flag_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	uint32_t reg_value = mt6391_get_reg(codec_data, MT6397_SPK_CON6);

	if (codec_data->speaker_mode == MT6391_CLASS_AB)
		ucontrol->value.integer.value[0] = (reg_value & 0xA000) ? 1 : 0;
	else
		ucontrol->value.integer.value[0] = (reg_value & 0x5000) ? 1 : 0;

	return 0;
}

static int mt6391_speaker_oc_flag_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt6391_dac_newif_sck_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	uint32_t reg_value = mt6391_get_reg(codec_data, MT6397_AFE_PMIC_NEWIF_CFG2);

	ucontrol->value.integer.value[0] = (reg_value & 0x8000) ? 1 : 0;
	return 0;
}

static int mt6391_dac_newif_sck_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] == 0)
		mt6391_set_reg(codec_data, MT6397_AFE_PMIC_NEWIF_CFG2, 0 << 15, 1 << 15);
	else if (ucontrol->value.integer.value[0] == 1)
		mt6391_set_reg(codec_data, MT6397_AFE_PMIC_NEWIF_CFG2, 1 << 15, 1 << 15);

	return 0;
}

static int mt6391_dmic_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    codec_data->device_power[MT6391_DEV_IN_DIGITAL_MIC] ? 1 : 0;
	return 0;
}

static int mt6391_dmic_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	pr_debug("%s\n", __func__);
	if (ucontrol->value.integer.value[0] &&
	    !codec_data->device_power[MT6391_DEV_IN_DIGITAL_MIC]) {
		mt6391_ana_clk_on(codec_data);
		mt6391_turn_on_dmic(codec_data);
		codec_data->device_power[MT6391_DEV_IN_DIGITAL_MIC] = true;
	} else if (!ucontrol->value.integer.value[0] &&
		   codec_data->device_power[MT6391_DEV_IN_DIGITAL_MIC]) {
		codec_data->device_power[MT6391_DEV_IN_DIGITAL_MIC] = false;
		mt6391_turn_off_dmic(codec_data);
		mt6391_ana_clk_off(codec_data);
	}
	return 0;
}

static int mt6391_adc1_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    codec_data->device_power[MT6391_DEV_IN_ADC1] ? 1 : 0;
	return 0;
}

static int mt6391_adc1_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	pr_debug("%s\n", __func__);
	if (ucontrol->value.integer.value[0]
	    && !codec_data->device_power[MT6391_DEV_IN_ADC1]) {
		mt6391_ana_clk_on(codec_data);
		mt6391_turn_on_adc(codec_data, MT6391_DEV_IN_ADC1);
		codec_data->device_power[MT6391_DEV_IN_ADC1] = true;
	} else if (!ucontrol->value.integer.value[0] &&
		   codec_data->device_power[MT6391_DEV_IN_ADC1]) {
		codec_data->device_power[MT6391_DEV_IN_ADC1] = false;
		mt6391_turn_off_adc(codec_data, MT6391_DEV_IN_ADC1);
		mt6391_ana_clk_off(codec_data);
	}
	return 0;
}

static int mt6391_adc2_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    codec_data->device_power[MT6391_DEV_IN_ADC2] ? 1 : 0;
	return 0;
}

static int mt6391_adc2_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	pr_debug("%s\n", __func__);
	if (ucontrol->value.integer.value[0]
	    && !codec_data->device_power[MT6391_DEV_IN_ADC2]) {
		mt6391_ana_clk_on(codec_data);
		mt6391_turn_on_adc(codec_data, MT6391_DEV_IN_ADC2);
		codec_data->device_power[MT6391_DEV_IN_ADC2] = true;
	} else if (!ucontrol->value.integer.value[0] &&
		   codec_data->device_power[MT6391_DEV_IN_ADC2]) {
		codec_data->device_power[MT6391_DEV_IN_ADC2] = false;
		mt6391_turn_off_adc(codec_data, MT6391_DEV_IN_ADC2);
		mt6391_ana_clk_off(codec_data);
	}
	return 0;
}

static int mt6391_preamp1_mux_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->device_mux[MT6391_MUX_IN_PREAMP_1];
	return 0;
}

static int mt6391_preamp1_mux_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	pr_debug("%s\n", __func__);

	if (ucontrol->value.enumerated.item[0] > e->items) {
		pr_err("%s out of bound\n", __func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 1)
		mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_L, MT6391_MUX_IN_MIC1);
	else if (ucontrol->value.integer.value[0] == 2)
		mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_L, MT6391_MUX_IN_MIC2);
	else if (ucontrol->value.integer.value[0] == 3)
		mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_L, MT6391_MUX_IN_MIC3);
	else if (ucontrol->value.integer.value[0] != 0)
		pr_warn("%s unexpected value %ld", __func__, ucontrol->value.integer.value[0]);

	pr_debug("%s done\n", __func__);
	codec_data->device_mux[MT6391_MUX_IN_PREAMP_1] = ucontrol->value.integer.value[0];
	return 0;
}

static int mt6391_preamp2_mux_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->device_mux[MT6391_MUX_IN_PREAMP_2];
	return 0;
}

static int mt6391_preamp2_mux_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	pr_debug("%s\n", __func__);

	if (ucontrol->value.enumerated.item[0] > e->items) {
		pr_err("%s out of bound\n", __func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 1)
		mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_R, MT6391_MUX_IN_MIC1);
	else if (ucontrol->value.integer.value[0] == 2)
		mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_R, MT6391_MUX_IN_MIC2);
	else if (ucontrol->value.integer.value[0] == 3)
		mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_R, MT6391_MUX_IN_MIC3);
	else if (ucontrol->value.integer.value[0] != 0)
		pr_warn("%s unexpected value %ld", __func__, ucontrol->value.integer.value[0]);

	pr_debug("%s done\n", __func__);
	codec_data->device_mux[MT6391_MUX_IN_PREAMP_2] = ucontrol->value.integer.value[0];
	return 0;
}

static int mt6391_preamp1_gain_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    codec_data->device_volume[MT6391_VOL_MICAMPL];
	return 0;
}

static int mt6391_preamp1_gain_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	int index = 0;

	pr_debug("%s\n", __func__);
	if (ucontrol->value.enumerated.item[0] > e->items) {
		pr_err("%s out of bound\n", __func__);
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6391_set_reg(codec_data, MT6397_AUDPREAMPGAIN_CON0, index << 0, 0x00000007);
	codec_data->device_volume[MT6391_VOL_MICAMPL] = index;
	return 0;
}

static int mt6391_preamp2_gain_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    codec_data->device_volume[MT6391_VOL_MICAMPR];
	return 0;
}

static int mt6391_preamp2_gain_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	int index = 0;

	pr_debug("%s\n", __func__);

	if (ucontrol->value.enumerated.item[0] > e->items) {
		pr_err("%s out of bound\n", __func__);
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	mt6391_set_reg(codec_data, MT6397_AUDPREAMPGAIN_CON0, index << 4, 0x00000070);
	codec_data->device_volume[MT6391_VOL_MICAMPR] = index;
	return 0;
}

static int mt6391_loopback_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->codec_loopback_type;
	return 0;
}

static int mt6391_loopback_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);
	uint32_t previous_type = codec_data->codec_loopback_type;
	long set_value = ucontrol->value.integer.value[0];

	pr_debug("%s %ld\n", __func__, set_value);

	if (previous_type == set_value) {
		pr_debug("%s dummy operation for %u", __func__, codec_data->codec_loopback_type);
		return 0;
	}

	if (previous_type != CODEC_LOOPBACK_NONE) {
		/* disable uplink */
		if (previous_type == CODEC_LOOPBACK_AMIC_TO_SPK ||
		    previous_type == CODEC_LOOPBACK_AMIC_TO_HP ||
		    previous_type == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
		    previous_type == CODEC_LOOPBACK_HEADSET_MIC_TO_HP) {
			if (codec_data->device_power[MT6391_DEV_IN_ADC1]) {
				codec_data->device_power[MT6391_DEV_IN_ADC1] = false;
				mt6391_turn_off_adc(codec_data, MT6391_DEV_IN_ADC1);
			}

			if (codec_data->device_power[MT6391_DEV_IN_ADC2]) {
				codec_data->device_power[MT6391_DEV_IN_ADC2] = false;
				mt6391_turn_off_adc(codec_data, MT6391_DEV_IN_ADC2);
			}
			codec_data->device_mux[MT6391_MUX_IN_PREAMP_1] = 0;
			codec_data->device_mux[MT6391_MUX_IN_PREAMP_2] = 0;
		} else if (previous_type == CODEC_LOOPBACK_DMIC_TO_SPK ||
			   previous_type == CODEC_LOOPBACK_DMIC_TO_HP) {
			if (codec_data->device_power[MT6391_DEV_IN_DIGITAL_MIC]) {
				codec_data->device_power[MT6391_DEV_IN_DIGITAL_MIC] = false;
				mt6391_turn_off_dmic(codec_data);
			}
		}
		/* disable downlink */
		if (previous_type == CODEC_LOOPBACK_AMIC_TO_SPK ||
		    previous_type == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
		    previous_type == CODEC_LOOPBACK_DMIC_TO_SPK) {
			if (codec_data->device_power[MT6391_DEV_OUT_SPEAKERL]) {
				codec_data->device_power[MT6391_DEV_OUT_SPEAKERL] = false;
				mt6391_turn_off_speaker_amp(codec_data);
			}
			mt6391_ana_clk_off(codec_data);
		} else if (previous_type == CODEC_LOOPBACK_AMIC_TO_HP ||
			   previous_type == CODEC_LOOPBACK_DMIC_TO_HP ||
			   previous_type == CODEC_LOOPBACK_HEADSET_MIC_TO_HP) {
			if (codec_data->device_power[MT6391_DEV_OUT_HEADSETL]) {
				codec_data->device_power[MT6391_DEV_OUT_HEADSETL] = false;
				mt6391_turn_off_headphone_amp(codec_data);
			}
			mt6391_ana_clk_off(codec_data);
		}
	}

	/* enable uplink */
	if (set_value == CODEC_LOOPBACK_AMIC_TO_SPK ||
	    set_value == CODEC_LOOPBACK_AMIC_TO_HP ||
	    set_value == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
	    set_value == CODEC_LOOPBACK_HEADSET_MIC_TO_HP ||
	    set_value == CODEC_LOOPBACK_AMIC_TO_EXTDAC ||
	    set_value == CODEC_LOOPBACK_HEADSET_MIC_TO_EXTDAC) {
		mt6391_ana_clk_on(codec_data);
		codec_data->sample_rate[MT6391_ADDA_ADC] = 48000;

		if (!codec_data->device_power[MT6391_DEV_IN_ADC1]) {
			mt6391_turn_on_adc(codec_data, MT6391_DEV_IN_ADC1);
			codec_data->device_power[MT6391_DEV_IN_ADC1] = true;
		}

		if (!codec_data->device_power[MT6391_DEV_IN_ADC2]) {
			mt6391_turn_on_adc(codec_data, MT6391_DEV_IN_ADC2);
			codec_data->device_power[MT6391_DEV_IN_ADC2] = true;
		}
		/* mux selection */
		if (set_value == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
		    set_value == CODEC_LOOPBACK_HEADSET_MIC_TO_HP ||
		    set_value == CODEC_LOOPBACK_HEADSET_MIC_TO_EXTDAC) {
			mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_L, MT6391_MUX_IN_MIC2);
			mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_R, MT6391_MUX_IN_MIC2);
			codec_data->device_mux[MT6391_MUX_IN_PREAMP_1] = 2;
			codec_data->device_mux[MT6391_MUX_IN_PREAMP_2] = 2;
		} else {
			mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_L, MT6391_MUX_IN_MIC1);
			mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_R, MT6391_MUX_IN_MIC3);
			codec_data->device_mux[MT6391_MUX_IN_PREAMP_1] = 1;
			codec_data->device_mux[MT6391_MUX_IN_PREAMP_2] = 3;
		}
	} else if (set_value == CODEC_LOOPBACK_DMIC_TO_SPK ||
		   set_value == CODEC_LOOPBACK_DMIC_TO_HP) {
		mt6391_ana_clk_on(codec_data);
		codec_data->sample_rate[MT6391_ADDA_ADC] = 32000;

		if (!codec_data->device_power[MT6391_DEV_IN_DIGITAL_MIC]) {
			mt6391_turn_on_dmic(codec_data);
			codec_data->device_power[MT6391_DEV_IN_DIGITAL_MIC] = true;
		}
	}

	/* enable downlink */
	if (set_value == CODEC_LOOPBACK_AMIC_TO_SPK ||
	    set_value == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
	    set_value == CODEC_LOOPBACK_DMIC_TO_SPK) {
		codec_data->sample_rate[MT6391_ADDA_DAC] =
				(set_value == CODEC_LOOPBACK_DMIC_TO_SPK) ? 32000 : 48000;
		if (!codec_data->device_power[MT6391_DEV_OUT_SPEAKERL]) {
			mt6391_turn_on_speaker_amp(codec_data);
			codec_data->device_power[MT6391_DEV_OUT_SPEAKERL] = true;
		}
	} else if (set_value == CODEC_LOOPBACK_AMIC_TO_HP ||
		   set_value == CODEC_LOOPBACK_DMIC_TO_HP ||
		   set_value == CODEC_LOOPBACK_HEADSET_MIC_TO_HP) {
		codec_data->sample_rate[MT6391_ADDA_DAC] =
				(set_value == CODEC_LOOPBACK_DMIC_TO_HP) ? 32000 : 48000;
		if (!codec_data->device_power[MT6391_DEV_OUT_HEADSETL]) {
			mt6391_turn_on_headphone_amp(codec_data);
			codec_data->device_power[MT6391_DEV_OUT_HEADSETL] = true;
		}
	}

	codec_data->codec_loopback_type = set_value;
	return 0;
}

static int mt6391_dac_sgen_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->dac_sgen_switch;
	return 0;
}

static int mt6391_dac_sgen_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	if (codec_data->dac_sgen_switch == ucontrol->value.integer.value[0]) {
		pr_debug("%s dummy operation for %u", __func__, codec_data->dac_sgen_switch);
		return 0;
	}

	if (ucontrol->value.integer.value[0]) {
		mt6391_set_reg(codec_data, MT6397_ANA_AFE_TOP_CON0, 0x1, 0x1);
		/* mt6391_set_reg(codec_data, MT6397_AFE_SGEN_CFG0, 0x80, 0x80); */
		mt6391_set_reg(codec_data, MT6397_AFE_SGEN_CFG0, 0x4480, 0xff80);
		mt6391_set_reg(codec_data, MT6397_AFE_SGEN_CFG1, 0x0101, 0xffff);
	} else {
		mt6391_set_reg(codec_data, MT6397_ANA_AFE_TOP_CON0, 0x0, 0x1);
		mt6391_set_reg(codec_data, MT6397_AFE_SGEN_CFG0, 0x0, 0x80);
	}

	codec_data->dac_sgen_switch = ucontrol->value.integer.value[0];
	return 0;
}

static int mt6391_adc_sgen_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->adc_sgen_switch;
	return 0;
}

static int mt6391_adc_sgen_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	if (codec_data->adc_sgen_switch == ucontrol->value.integer.value[0]) {
		pr_debug("%s dummy operation for %u", __func__, codec_data->adc_sgen_switch);
		return 0;
	}

	if (ucontrol->value.integer.value[0]) {
		mt6391_set_reg(codec_data, MT6397_ANA_AFE_TOP_CON0, 0x2, 0x2);
		mt6391_set_reg(codec_data, MT6397_AFE_SGEN_CFG0, 0x80, 0x80);
		mt6391_set_reg(codec_data, MT6397_AFE_SGEN_CFG1, 0x0101, 0xffff);
	} else {
		mt6391_set_reg(codec_data, MT6397_ANA_AFE_TOP_CON0, 0x0, 0x2);
		mt6391_set_reg(codec_data, MT6397_AFE_SGEN_CFG0, 0x0, 0x80);
	}

	codec_data->adc_sgen_switch = ucontrol->value.integer.value[0];
	return 0;
}

static int mt6391_dac_freq_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	if (codec_data->sample_rate[MT6391_ADDA_DAC] == 48000)
		ucontrol->value.integer.value[0] = DAC_FREQ_48000;
	else if (codec_data->sample_rate[MT6391_ADDA_DAC] == 44100)
		ucontrol->value.integer.value[0] = DAC_FREQ_44100;
	else if (codec_data->sample_rate[MT6391_ADDA_DAC] == 32000)
		ucontrol->value.integer.value[0] = DAC_FREQ_32000;
	else if (codec_data->sample_rate[MT6391_ADDA_DAC] == 24000)
		ucontrol->value.integer.value[0] = DAC_FREQ_24000;
	else if (codec_data->sample_rate[MT6391_ADDA_DAC] == 22050)
		ucontrol->value.integer.value[0] = DAC_FREQ_22050;
	else if (codec_data->sample_rate[MT6391_ADDA_DAC] == 16000)
		ucontrol->value.integer.value[0] = DAC_FREQ_16000;
	else if (codec_data->sample_rate[MT6391_ADDA_DAC] == 12000)
		ucontrol->value.integer.value[0] = DAC_FREQ_12000;
	else if (codec_data->sample_rate[MT6391_ADDA_DAC] == 11025)
		ucontrol->value.integer.value[0] = DAC_FREQ_11025;
	else if (codec_data->sample_rate[MT6391_ADDA_DAC] == 8000)
		ucontrol->value.integer.value[0] = DAC_FREQ_8000;
	else
		ucontrol->value.integer.value[0] = DAC_FREQ_48000;
	return 0;
}

static int mt6391_dac_freq_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] == DAC_FREQ_48000)
		codec_data->sample_rate[MT6391_ADDA_DAC] = 48000;
	else if (ucontrol->value.integer.value[0] == DAC_FREQ_44100)
		codec_data->sample_rate[MT6391_ADDA_DAC] = 44100;
	else if (ucontrol->value.integer.value[0] == DAC_FREQ_32000)
		codec_data->sample_rate[MT6391_ADDA_DAC] = 32000;
	else if (ucontrol->value.integer.value[0] == DAC_FREQ_24000)
		codec_data->sample_rate[MT6391_ADDA_DAC] = 24000;
	else if (ucontrol->value.integer.value[0] == DAC_FREQ_22050)
		codec_data->sample_rate[MT6391_ADDA_DAC] = 22050;
	else if (ucontrol->value.integer.value[0] == DAC_FREQ_16000)
		codec_data->sample_rate[MT6391_ADDA_DAC] = 16000;
	else if (ucontrol->value.integer.value[0] == DAC_FREQ_12000)
		codec_data->sample_rate[MT6391_ADDA_DAC] = 12000;
	else if (ucontrol->value.integer.value[0] == DAC_FREQ_11025)
		codec_data->sample_rate[MT6391_ADDA_DAC] = 11025;
	else if (ucontrol->value.integer.value[0] == DAC_FREQ_8000)
		codec_data->sample_rate[MT6391_ADDA_DAC] = 8000;

	return 0;
}

static int mt6391_adc_freq_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	if (codec_data->sample_rate[MT6391_ADDA_ADC] == 48000)
		ucontrol->value.integer.value[0] = ADC_FREQ_48000;
	else if (codec_data->sample_rate[MT6391_ADDA_ADC] == 32000)
		ucontrol->value.integer.value[0] = ADC_FREQ_32000;
	else if (codec_data->sample_rate[MT6391_ADDA_ADC] == 16000)
		ucontrol->value.integer.value[0] = ADC_FREQ_16000;
	else if (codec_data->sample_rate[MT6391_ADDA_ADC] == 8000)
		ucontrol->value.integer.value[0] = ADC_FREQ_8000;
	else
		ucontrol->value.integer.value[0] = ADC_FREQ_48000;
	return 0;
}

static int mt6391_adc_freq_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt6391_priv *codec_data = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] == ADC_FREQ_48000)
		codec_data->sample_rate[MT6391_ADDA_ADC] = 48000;
	else if (ucontrol->value.integer.value[0] == ADC_FREQ_32000)
		codec_data->sample_rate[MT6391_ADDA_ADC] = 32000;
	else if (ucontrol->value.integer.value[0] == ADC_FREQ_16000)
		codec_data->sample_rate[MT6391_ADDA_ADC] = 16000;
	else if (ucontrol->value.integer.value[0] == ADC_FREQ_8000)
		codec_data->sample_rate[MT6391_ADDA_ADC] = 8000;

	return 0;
}


/* snd kcontrol function definition */
static const char *const mt6391_amp_func[] = { "Off", "On" };

static const char *const mt6391_headset_gain[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db", "-1Db",
	"-2Db", "-3Db",	"-4Db", "RES1", "RES2", "-40Db"
};

static const char *const mt6391_handset_gain[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db", "-1Db",
	"-2Db", "-3Db",	"-4Db", "RES1", "RES2", "-40Db"
};

static const char *const mt6391_speaker_gain[] = {
	"Mute", "0Db", "4Db", "5Db", "6Db", "7Db", "8Db", "9Db", "10Db",
	"11Db", "12Db", "13Db", "14Db", "15Db", "16Db", "17Db"
};

static const char *const mt6391_speaker_select_func[] = { "Stereo", "MonoLeft", "MonoRight" };

static const char *const mt6391_speaker_oc_func[] = { "NoOverCurrent", "OverCurrent" };

static const char *const mt6391_dac_newif_sck_func[] = { "NotInverse", "Inverse" };

static const char *const mt6391_dmic_func[] = { "Off", "On" };

static const char *const mt6391_adc_func[] = { "Off", "On" };

static const char *const mt6391_preamp_mux_func[] = { "OPEN", "AIN1", "AIN2", "AIN3" };

static const char *const mt6391_preamp_gain[] = { "2Db", "8Db", "14Db", "20Db", "26Db", "32Db" };

static const char *const mt6391_codec_loopback_func[] = {
	ENUM_TO_STR(CODEC_LOOPBACK_NONE),
	ENUM_TO_STR(CODEC_LOOPBACK_AMIC_TO_SPK),
	ENUM_TO_STR(CODEC_LOOPBACK_AMIC_TO_HP),
	ENUM_TO_STR(CODEC_LOOPBACK_DMIC_TO_SPK),
	ENUM_TO_STR(CODEC_LOOPBACK_DMIC_TO_HP),
	ENUM_TO_STR(CODEC_LOOPBACK_HEADSET_MIC_TO_SPK),
	ENUM_TO_STR(CODEC_LOOPBACK_HEADSET_MIC_TO_HP),
	ENUM_TO_STR(CODEC_LOOPBACK_AMIC_TO_EXTDAC),
	ENUM_TO_STR(CODEC_LOOPBACK_HEADSET_MIC_TO_EXTDAC),
};

static const char *const mt6391_sgen_func[] = { "Off", "On" };

static const char *const mt6391_dac_freq_func[] = {
	ENUM_TO_STR(DAC_FREQ_8000),
	ENUM_TO_STR(DAC_FREQ_11025),
	ENUM_TO_STR(DAC_FREQ_12000),
	ENUM_TO_STR(DAC_FREQ_16000),
	ENUM_TO_STR(DAC_FREQ_22050),
	ENUM_TO_STR(DAC_FREQ_24000),
	ENUM_TO_STR(DAC_FREQ_32000),
	ENUM_TO_STR(DAC_FREQ_44100),
	ENUM_TO_STR(DAC_FREQ_48000),
};

static const char *const mt6391_adc_freq_func[] = {
	ENUM_TO_STR(ADC_FREQ_8000),
	ENUM_TO_STR(ADC_FREQ_16000),
	ENUM_TO_STR(ADC_FREQ_32000),
	ENUM_TO_STR(ADC_FREQ_48000),
};

/* soc_enum list */
static const struct soc_enum mt6391_soc_enums[] = {
	/* downlink */
	[ENUM_AUDIO_AMP] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_amp_func),
				mt6391_amp_func),
	[ENUM_VOICE_AMP] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_amp_func),
				mt6391_amp_func),
	[ENUM_SPK_AMP] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_amp_func),
				mt6391_amp_func),
	[ENUM_HS_SPK_AMP] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_amp_func),
				mt6391_amp_func),
	[ENUM_HEADSETL_GAIN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_headset_gain),
				mt6391_headset_gain),
	[ENUM_HEADSETR_GAIN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_headset_gain),
				mt6391_headset_gain),
	[ENUM_HANDSET_GAIN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_handset_gain),
				mt6391_handset_gain),
	[ENUM_SPKL_GAIN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_speaker_gain),
				mt6391_speaker_gain),
	[ENUM_SPKR_GAIN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_speaker_gain),
				mt6391_speaker_gain),
	[ENUM_SPK_SEL] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_speaker_select_func),
				mt6391_speaker_select_func),
	[ENUM_SPK_OC_FLAG] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_speaker_oc_func),
				mt6391_speaker_oc_func),
	[ENUM_DAC_SCK] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_dac_newif_sck_func),
				mt6391_dac_newif_sck_func),
	/* uplink */
	[ENUM_DMIC_SWITCH] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_dmic_func),
				mt6391_dmic_func),
	[ENUM_ADC1_SWITCH] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_adc_func),
				mt6391_adc_func),
	[ENUM_ADC2_SWITCH] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_adc_func),
				mt6391_adc_func),
	[ENUM_PREAMP1_MUX] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_preamp_mux_func),
				mt6391_preamp_mux_func),
	[ENUM_PREAMP2_MUX] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_preamp_mux_func),
				mt6391_preamp_mux_func),
	[ENUM_PREAMP1_GAIN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_preamp_gain),
				mt6391_preamp_gain),
	[ENUM_PREAMP2_GAIN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_preamp_gain),
				mt6391_preamp_gain),
	/* factory */
	[ENUM_LOOPBACK_SEL] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_codec_loopback_func),
				mt6391_codec_loopback_func),
	[ENUM_DAC_SGEN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_sgen_func),
				mt6391_sgen_func),
	[ENUM_ADC_SGEN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_sgen_func),
				mt6391_sgen_func),
	[ENUM_DAC_FREQ] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_dac_freq_func),
				mt6391_dac_freq_func),
	[ENUM_ADC_FREQ] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6391_adc_freq_func),
				mt6391_adc_freq_func),
};

static const struct snd_kcontrol_new mt6391_dl_codec_controls[] = {
	SOC_ENUM_EXT("Audio_Amp_Switch", mt6391_soc_enums[ENUM_AUDIO_AMP],
		mt6391_audio_amp_get, mt6391_audio_amp_set),
	SOC_ENUM_EXT("Voice_Amp_Switch", mt6391_soc_enums[ENUM_VOICE_AMP],
		mt6391_voice_amp_get, mt6391_voice_amp_set),
	SOC_ENUM_EXT("Speaker_Amp_Switch", mt6391_soc_enums[ENUM_SPK_AMP],
		mt6391_speaker_amp_get, mt6391_speaker_amp_set),
	SOC_ENUM_EXT("Headset_Speaker_Amp_Switch", mt6391_soc_enums[ENUM_HS_SPK_AMP],
		mt6391_headset_speaker_amp_get, mt6391_headset_speaker_amp_set),
	SOC_ENUM_EXT("Headset_PGAL_GAIN", mt6391_soc_enums[ENUM_HEADSETL_GAIN],
		mt6391_headset_pgal_get, mt6391_headset_pgal_set),
	SOC_ENUM_EXT("Headset_PGAR_GAIN", mt6391_soc_enums[ENUM_HEADSETR_GAIN],
		mt6391_headset_pgar_get, mt6391_headset_pgar_set),
	SOC_ENUM_EXT("Hanset_PGA_GAIN", mt6391_soc_enums[ENUM_HANDSET_GAIN],
		mt6391_handset_pga_get, mt6391_handset_pga_set),
	SOC_ENUM_EXT("Speaker_PGAL_GAIN", mt6391_soc_enums[ENUM_SPKL_GAIN],
		mt6391_speaker_pgal_get, mt6391_speaker_pgal_set),
	SOC_ENUM_EXT("Speaker_PGAR_GAIN", mt6391_soc_enums[ENUM_SPKR_GAIN],
		mt6391_speaker_pgar_get, mt6391_speaker_pgar_set),
	SOC_ENUM_EXT("Speaker_Channel_Select", mt6391_soc_enums[ENUM_SPK_SEL],
		mt6391_speaker_channel_get, mt6391_speaker_channel_set),
	SOC_ENUM_EXT("Speaker_OC_Flag", mt6391_soc_enums[ENUM_SPK_OC_FLAG],
		mt6391_speaker_oc_flag_get, mt6391_speaker_oc_flag_set),
	SOC_ENUM_EXT("DAC_Newif_Sck_Switch", mt6391_soc_enums[ENUM_DAC_SCK],
		mt6391_dac_newif_sck_get, mt6391_dac_newif_sck_set),
};

static const struct snd_kcontrol_new mt6391_ul_codec_controls[] = {
	SOC_ENUM_EXT("Audio_Digital_Mic_Switch", mt6391_soc_enums[ENUM_DMIC_SWITCH],
		mt6391_dmic_get, mt6391_dmic_set),
	SOC_ENUM_EXT("Audio_ADC_1_Switch", mt6391_soc_enums[ENUM_ADC1_SWITCH],
		mt6391_adc1_get, mt6391_adc1_set),
	SOC_ENUM_EXT("Audio_ADC_2_Switch", mt6391_soc_enums[ENUM_ADC2_SWITCH],
		mt6391_adc2_get, mt6391_adc2_set),
	SOC_ENUM_EXT("Audio_Preamp1_Switch", mt6391_soc_enums[ENUM_PREAMP1_MUX],
		mt6391_preamp1_mux_get, mt6391_preamp1_mux_set),
	SOC_ENUM_EXT("Audio_Preamp2_Switch", mt6391_soc_enums[ENUM_PREAMP2_MUX],
		mt6391_preamp2_mux_get, mt6391_preamp2_mux_set),
	SOC_ENUM_EXT("Audio_PGA1_Setting", mt6391_soc_enums[ENUM_PREAMP1_GAIN],
		mt6391_preamp1_gain_get, mt6391_preamp1_gain_set),
	SOC_ENUM_EXT("Audio_PGA2_Setting", mt6391_soc_enums[ENUM_PREAMP2_GAIN],
		mt6391_preamp2_gain_get, mt6391_preamp2_gain_set),
};

static const struct snd_kcontrol_new mt6391_factory_controls[] = {
	SOC_ENUM_EXT("Codec_Loopback_Select", mt6391_soc_enums[ENUM_LOOPBACK_SEL],
		     mt6391_loopback_get, mt6391_loopback_set),
	SOC_ENUM_EXT("DAC_SGen_Switch", mt6391_soc_enums[ENUM_DAC_SGEN],
		     mt6391_dac_sgen_get, mt6391_dac_sgen_set),
	SOC_ENUM_EXT("ADC_SGen_Switch", mt6391_soc_enums[ENUM_ADC_SGEN],
		     mt6391_adc_sgen_get, mt6391_adc_sgen_set),
	SOC_ENUM_EXT("DAC_Freq_Switch", mt6391_soc_enums[ENUM_DAC_FREQ],
		     mt6391_dac_freq_get, mt6391_dac_freq_set),
	SOC_ENUM_EXT("ADC_Freq_Switch", mt6391_soc_enums[ENUM_ADC_FREQ],
		     mt6391_adc_freq_get, mt6391_adc_freq_set),
};

#ifdef CONFIG_DEBUG_FS
struct mt6391_reg_attr {
	uint32_t offset;
	char *name;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct mt6391_reg_attr dump_reg_list[] = {
	DUMP_REG_ENTRY(MT6397_AFE_UL_DL_CON0),
	DUMP_REG_ENTRY(MT6397_AFE_DL_SRC2_CON0_H),
	DUMP_REG_ENTRY(MT6397_AFE_DL_SRC2_CON0_L),
	DUMP_REG_ENTRY(MT6397_AFE_DL_SDM_CON0),
	DUMP_REG_ENTRY(MT6397_AFE_DL_SDM_CON1),
	DUMP_REG_ENTRY(MT6397_AFE_UL_SRC_CON0_H),
	DUMP_REG_ENTRY(MT6397_AFE_UL_SRC_CON0_L),
	DUMP_REG_ENTRY(MT6397_AFE_UL_SRC_CON1_H),
	DUMP_REG_ENTRY(MT6397_AFE_UL_SRC_CON1_L),
	DUMP_REG_ENTRY(MT6397_ANA_AFE_TOP_CON0),
	DUMP_REG_ENTRY(MT6397_AFUNC_AUD_CON0),
	DUMP_REG_ENTRY(MT6397_AFUNC_AUD_CON1),
	DUMP_REG_ENTRY(MT6397_AFUNC_AUD_CON2),
	DUMP_REG_ENTRY(MT6397_AFUNC_AUD_CON3),
	DUMP_REG_ENTRY(MT6397_AFUNC_AUD_CON4),
	DUMP_REG_ENTRY(MT6397_AFUNC_AUD_MON0),
	DUMP_REG_ENTRY(MT6397_AFUNC_AUD_MON1),
	DUMP_REG_ENTRY(MT6397_AUDRC_TUNE_MON0),
	DUMP_REG_ENTRY(MT6397_AFE_UP8X_FIFO_CFG0),
	DUMP_REG_ENTRY(MT6397_AFE_UP8X_FIFO_LOG_MON0),
	DUMP_REG_ENTRY(MT6397_AFE_UP8X_FIFO_LOG_MON1),
	DUMP_REG_ENTRY(MT6397_AFE_DL_DC_COMP_CFG0),
	DUMP_REG_ENTRY(MT6397_AFE_DL_DC_COMP_CFG1),
	DUMP_REG_ENTRY(MT6397_AFE_DL_DC_COMP_CFG2),
	DUMP_REG_ENTRY(MT6397_AFE_PMIC_NEWIF_CFG0),
	DUMP_REG_ENTRY(MT6397_AFE_PMIC_NEWIF_CFG1),
	DUMP_REG_ENTRY(MT6397_AFE_PMIC_NEWIF_CFG2),
	DUMP_REG_ENTRY(MT6397_AFE_PMIC_NEWIF_CFG3),
	DUMP_REG_ENTRY(MT6397_AFE_SGEN_CFG0),
	DUMP_REG_ENTRY(MT6397_AFE_SGEN_CFG1),
	DUMP_REG_ENTRY(MT6397_TOP_CKPDN),
	DUMP_REG_ENTRY(MT6397_TOP_CKPDN2),
	DUMP_REG_ENTRY(MT6397_TOP_CKCON1),
	DUMP_REG_ENTRY(MT6397_TOP_CKCON3),
	DUMP_REG_ENTRY(MT6397_SPK_CON0),
	DUMP_REG_ENTRY(MT6397_SPK_CON1),
	DUMP_REG_ENTRY(MT6397_SPK_CON2),
	DUMP_REG_ENTRY(MT6397_SPK_CON3),
	DUMP_REG_ENTRY(MT6397_SPK_CON4),
	DUMP_REG_ENTRY(MT6397_SPK_CON5),
	DUMP_REG_ENTRY(MT6397_SPK_CON6),
	DUMP_REG_ENTRY(MT6397_SPK_CON7),
	DUMP_REG_ENTRY(MT6397_SPK_CON8),
	DUMP_REG_ENTRY(MT6397_SPK_CON9),
	DUMP_REG_ENTRY(MT6397_SPK_CON10),
	DUMP_REG_ENTRY(MT6397_SPK_CON11),
	DUMP_REG_ENTRY(MT6397_AUDDAC_CON0),
	DUMP_REG_ENTRY(MT6397_AUDBUF_CFG0),
	DUMP_REG_ENTRY(MT6397_AUDBUF_CFG1),
	DUMP_REG_ENTRY(MT6397_AUDBUF_CFG2),
	DUMP_REG_ENTRY(MT6397_AUDBUF_CFG3),
	DUMP_REG_ENTRY(MT6397_AUDBUF_CFG4),
	DUMP_REG_ENTRY(MT6397_IBIASDIST_CFG0),
	DUMP_REG_ENTRY(MT6397_AUDACCDEPOP_CFG0),
	DUMP_REG_ENTRY(MT6397_AUD_IV_CFG0),
	DUMP_REG_ENTRY(MT6397_AUDCLKGEN_CFG0),
	DUMP_REG_ENTRY(MT6397_AUDLDO_CFG0),
	DUMP_REG_ENTRY(MT6397_AUDLDO_CFG1),
	DUMP_REG_ENTRY(MT6397_AUDNVREGGLB_CFG0),
	DUMP_REG_ENTRY(MT6397_AUD_NCP0),
	DUMP_REG_ENTRY(MT6397_AUDPREAMP_CON0),
	DUMP_REG_ENTRY(MT6397_AUDADC_CON0),
	DUMP_REG_ENTRY(MT6397_AUDADC_CON1),
	DUMP_REG_ENTRY(MT6397_AUDADC_CON2),
	DUMP_REG_ENTRY(MT6397_AUDADC_CON3),
	DUMP_REG_ENTRY(MT6397_AUDADC_CON4),
	DUMP_REG_ENTRY(MT6397_AUDADC_CON5),
	DUMP_REG_ENTRY(MT6397_AUDADC_CON6),
	DUMP_REG_ENTRY(MT6397_AUDDIGMI_CON0),
	DUMP_REG_ENTRY(MT6397_AUDLSBUF_CON0),
	DUMP_REG_ENTRY(MT6397_AUDLSBUF_CON1),
	DUMP_REG_ENTRY(MT6397_AUDENCSPARE_CON0),
	DUMP_REG_ENTRY(MT6397_AUDENCCLKSQ_CON0),
	DUMP_REG_ENTRY(MT6397_AUDPREAMPGAIN_CON0),
	DUMP_REG_ENTRY(MT6397_ZCD_CON0),
	DUMP_REG_ENTRY(MT6397_ZCD_CON1),
	DUMP_REG_ENTRY(MT6397_ZCD_CON2),
	DUMP_REG_ENTRY(MT6397_ZCD_CON3),
	DUMP_REG_ENTRY(MT6397_ZCD_CON4),
	DUMP_REG_ENTRY(MT6397_ZCD_CON5),
	DUMP_REG_ENTRY(MT6397_NCP_CLKDIV_CON0),
	DUMP_REG_ENTRY(MT6397_NCP_CLKDIV_CON1),
};


static ssize_t mt6391_debug_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *pos)
{
	struct mt6391_priv *codec_data = file->private_data;
	ssize_t ret, i;
	char *buf;
	int n = 0;

	if (*pos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(dump_reg_list); i++) {
		n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
			dump_reg_list[i].name,
			mt6391_get_reg(codec_data, dump_reg_list[i].offset));
	}

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);

	kfree(buf);

	return ret;
}

static const struct file_operations mt6391_debug_ops = {
	.open = simple_open,
	.read = mt6391_debug_read,
	.llseek = default_llseek,
};
#endif

static int mt6391_codec_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *codec_dai)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pr_debug("%s SNDRV_PCM_STREAM_CAPTURE\n", __func__);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("%s SNDRV_PCM_STREAM_PLAYBACK\n", __func__);

	return 0;
}

static int mt6391_codec_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *codec_dai)
{
	struct mt6391_priv *codec_data = snd_soc_codec_get_drvdata(codec_dai->codec);
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_debug("%s capture rate = %d\n", __func__, runtime->rate);
		codec_data->sample_rate[MT6391_ADDA_ADC] = runtime->rate;
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s playback rate = %d\n", __func__, runtime->rate);
		codec_data->sample_rate[MT6391_ADDA_DAC] = runtime->rate;
	}
	return 0;
}

static int mt6391_codec_trigger(struct snd_pcm_substream *substream, int command,
				struct snd_soc_dai *codec_dai)
{
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}

	pr_debug("%s command = %d\n ", __func__, command);
	return 0;
}

static const struct snd_soc_dai_ops mt6391_aif1_dai_ops = {
	.startup = mt6391_codec_startup,
	.prepare = mt6391_codec_prepare,
	.trigger = mt6391_codec_trigger,
};

static struct snd_soc_dai_driver mt6391_codec_dai_drvs[] = {
	{
	 .name = "mt6397-codec-tx-dai",
	 .ops = &mt6391_aif1_dai_ops,
	 .playback = {
		      .stream_name = "Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 },
	{
	 .name = "mt6397-codec-rx-dai",
	 .ops = &mt6391_aif1_dai_ops,
	 .capture = {
		     .stream_name = "Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 },
};

static void mt6391_codec_init_reg(struct mt6391_priv *codec_data)
{
	pr_debug("%s\n", __func__);

	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0080, 0x0080);
	mt6391_set_reg(codec_data, MT6397_ZCD_CON2, 0x0c0c, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AUDBUF_CFG0, 0x0000, 0x1fe7);
	mt6391_set_reg(codec_data, MT6397_IBIASDIST_CFG0, 0x1552, 0xffff);	/* RG DEV ck off; */
	mt6391_set_reg(codec_data, MT6397_AUDDAC_CON0, 0x0000, 0xffff);	/* NCP off */
	mt6391_set_reg(codec_data, MT6397_AUDCLKGEN_CFG0, 0x0000, 0x0001);
	mt6391_set_reg(codec_data, MT6397_AUDNVREGGLB_CFG0, 0x0006, 0xffff);	/* need check */
	mt6391_set_reg(codec_data, MT6397_NCP_CLKDIV_CON1, 0x0001, 0xffff);	/* fix me */
	mt6391_set_reg(codec_data, MT6397_AUD_NCP0, 0x0000, 0x6000);
	mt6391_set_reg(codec_data, MT6397_AUDLDO_CFG0, 0x0192, 0xffff);
	mt6391_set_reg(codec_data, MT6397_AFUNC_AUD_CON2, 0x0000, 0x0080);
	/* ZCD setting gain step gain and enable */
	mt6391_set_reg(codec_data, MT6397_ZCD_CON0, 0x0101, 0xffff);
	/* sck inverse */
	mt6391_set_reg(codec_data, MT6397_AFE_PMIC_NEWIF_CFG2, 1 << 15, 1 << 15);
	/* default preamp mux */
	mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_L, MT6391_MUX_IN_MIC1);
	mt6391_set_mux(codec_data, MT6391_DEV_IN_PREAMP_R, MT6391_MUX_IN_MIC2);
}

static int mt6391_codec_probe(struct snd_soc_codec *codec)
{
	struct mt6391_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	codec_data->codec = codec;

#if defined(USE_MT6397_REGMAP)
	mt6391_set_reg(codec_data, MT6397_TOP_CKCON1, 0x0010, 0x0010);
#elif defined(USE_PMIC_WRAP_DRIVER)
	upmu_set_rg_clksq_en(1);
#endif

	mt6391_control_top_clk(codec_data, 0x0607, true);

	mt6391_codec_init_reg(codec_data);
	mt6391_get_hp_trim_offset(codec_data);
	mt6391_spk_auto_trim_offset(codec_data);

#if defined(USE_MT6397_REGMAP)
	mt6391_set_reg(codec_data, MT6397_TOP_CKCON1, 0x0000, 0x0010);
#elif defined(USE_PMIC_WRAP_DRIVER)
	upmu_set_rg_clksq_en(0);
#endif

	mt6391_control_top_clk(codec_data, 0x0607, false);

	snd_soc_add_codec_controls(codec, mt6391_dl_codec_controls,
				ARRAY_SIZE(mt6391_dl_codec_controls));
	snd_soc_add_codec_controls(codec, mt6391_ul_codec_controls,
				ARRAY_SIZE(mt6391_ul_codec_controls));
	snd_soc_add_codec_controls(codec, mt6391_factory_controls,
				ARRAY_SIZE(mt6391_factory_controls));

#ifdef CONFIG_DEBUG_FS
	codec_data->debugfs = debugfs_create_file("mt6391reg", S_IFREG | S_IRUGO,
					NULL, codec_data, &mt6391_debug_ops);
#endif
	return 0;
}

static int mt6391_codec_remove(struct snd_soc_codec *codec)
{
#ifdef CONFIG_DEBUG_FS
	struct mt6391_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	debugfs_remove(codec_data->debugfs);
#endif
	return 0;
}

static int mt6391_codec_suspend(struct snd_soc_codec *codec)
{
	struct mt6391_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	mt6391_suspend_clk_off(codec_data);
	return 0;
}

static int mt6391_codec_resume(struct snd_soc_codec *codec)
{
	struct mt6391_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	mt6391_suspend_clk_on(codec_data);
	return 0;
}

#if defined(USE_MT6397_REGMAP)
static struct regmap *mt6391_codec_get_regmap(struct device *dev)
{
	struct mt6397_chip *mt6397;

	mt6397 = dev_get_drvdata(dev->parent);

	return mt6397->regmap;
}
#else
static unsigned int mt6391_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct mt6391_priv *codec_data = snd_soc_codec_get_drvdata(codec);
	unsigned int val = mt6391_get_reg(codec_data, reg);

	pr_debug("%s reg = 0x%x val = 0x%x", __func__, reg, val);
	return val;
}

static int mt6391_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	struct mt6391_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s reg = 0x%x value= 0x%x\n", __func__, reg, value);
	mt6391_set_reg(codec_data, reg, value, 0xffffffff);
	return 0;
}
#endif

static struct snd_soc_codec_driver mt6391_codec_driver = {
	.probe = mt6391_codec_probe,
	.remove = mt6391_codec_remove,
	.suspend = mt6391_codec_suspend,
	.resume = mt6391_codec_resume,
#if defined(USE_MT6397_REGMAP)
	.get_regmap = mt6391_codec_get_regmap,
#else
	.read = mt6391_read,
	.write = mt6391_write,
#endif
};

static int mt6391_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt6391_priv *codec_data;
	int ret;

	pr_debug("%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT6397_CODEC_NAME);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

	codec_data = devm_kzalloc(dev, sizeof(struct mt6391_priv), GFP_KERNEL);
	if (unlikely(!codec_data)) {
		pr_err("%s failed to allocate private data\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&codec_data->ctrl_mutex);
	mutex_init(&codec_data->clk_mutex);

	codec_data->sample_rate[MT6391_ADDA_DAC] = 44100;
	codec_data->sample_rate[MT6391_ADDA_ADC] = 32000;

	ret = of_property_read_u32(dev->of_node, "mediatek,speaker-mode",
				&codec_data->speaker_mode);
	if (ret) {
		pr_warn("%s fail to read speaker-mode in node %s\n", __func__,
			dev->of_node->full_name);
		codec_data->speaker_mode = MT6391_CLASS_D;
	} else if (codec_data->speaker_mode != MT6391_CLASS_D &&
		codec_data->speaker_mode != MT6391_CLASS_AB) {
		codec_data->speaker_mode = MT6391_CLASS_D;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,adc-warmup-time-us",
			&codec_data->adc_warmup_time_us);
	if (ret)
		codec_data->adc_warmup_time_us = 0;

	ret = of_property_read_u32(dev->of_node, "mediatek,dmic-warmup-time-us",
			&codec_data->dmic_warmup_time_us);
	if (ret)
		codec_data->dmic_warmup_time_us = 0;

	dev_set_drvdata(dev, codec_data);

	return snd_soc_register_codec(dev, &mt6391_codec_driver, mt6391_codec_dai_drvs,
				ARRAY_SIZE(mt6391_codec_dai_drvs));
}

static int mt6391_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id mt6391_codec_dt_match[] = {
	{.compatible = "mediatek," MT6397_CODEC_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt6391_codec_dt_match);

static struct platform_driver mt6391_codec_device_driver = {
	.driver = {
		   .name = MT6397_CODEC_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt6391_codec_dt_match,
		   },
	.probe = mt6391_dev_probe,
	.remove = mt6391_dev_remove,
};

module_platform_driver(mt6391_codec_device_driver);

/* Module information */
MODULE_DESCRIPTION("MT6391 codec driver");
MODULE_LICENSE("GPL v2");

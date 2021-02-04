/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2021 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE.See the GNU General Public License for more details.
**
** File:
**     tas2562-codec.c
**
** Description:
**     ALSA SoC driver for Texas Instruments TAS2562 High Performance 4W Smart
**     Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2562_CODEC
#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tas2562.h"


#define TAS2562_MDELAY 0xFFFFFFFE
/* #define KCONTROL_CODEC */
static char const *iv_enable_text[] = {"Off", "On"};
static int tas2562iv_enable;
static const struct soc_enum tas2562_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(iv_enable_text), iv_enable_text),
};

static unsigned int tas2562_codec_read(struct snd_soc_codec *codec,
		unsigned int reg)
{
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);
	int nResult = 0;
	unsigned int value = 0;

	mutex_lock(&pTAS2562->dev_lock);

	nResult = regmap_read(pTAS2562->regmap, reg, &value);

	if (nResult < 0)
		dev_err(pTAS2562->dev, "%s, ERROR, reg=0x%x, E=%d\n",
			__func__, reg, nResult);
	else
		dev_err(pTAS2562->dev, "%s, reg: 0x%x, value: 0x%x\n",
				__func__, reg, value);

	mutex_unlock(&pTAS2562->dev_lock);

	if (nResult >= 0)
		return value;
	else
		return nResult;
}

static int tas2562iv_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

    if (codec == NULL) {
		pr_err("%s: codec is NULL \n",  __func__);
		return 0;
    }

    tas2562iv_enable = ucontrol->value.integer.value[0];

	if (tas2562iv_enable) {
		pr_debug("%s: tas2562iv_enable \n", __func__);
		snd_soc_update_bits(codec, TAS2562_PowerControl,
			TAS2562_PowerControl_OperationalMode10_Mask |
		    TAS2562_PowerControl_ISNSPower_Mask |
		    TAS2562_PowerControl_VSNSPower_Mask,
		    TAS2562_PowerControl_OperationalMode10_Active |
		    TAS2562_PowerControl_VSNSPower_Active |
		    TAS2562_PowerControl_ISNSPower_Active);
	} else {
		pr_debug("%s: tas2562iv_disable \n", __func__);
		snd_soc_update_bits(codec, TAS2562_PowerControl,
			TAS2562_PowerControl_OperationalMode10_Mask |
			TAS2562_PowerControl_ISNSPower_Mask |
			TAS2562_PowerControl_VSNSPower_Mask,
			TAS2562_PowerControl_OperationalMode10_Active |
			TAS2562_PowerControl_VSNSPower_PoweredDown |
			TAS2562_PowerControl_ISNSPower_PoweredDown);
	}

	pr_debug("%s: tas2562iv_enable = %d\n", __func__, tas2562iv_enable);

	return 0;
}

static int tas2562iv_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
   int value;
   ucontrol->value.integer.value[0] = tas2562iv_enable;
   value=gpio_get_value(37);
   pr_debug("%s: tas2562iv_enable = %d\n", __func__, tas2562iv_enable);
   pr_debug("%s: gpio37 value = %d\n", __func__, value);
   return 0;
}
static const struct snd_kcontrol_new tas2562_controls[] = {
SOC_ENUM_EXT("TAS2562 IVSENSE ENABLE", tas2562_enum[0],
		    tas2562iv_get, tas2562iv_put),
};

static int tas2562_codec_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);

	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);

	nResult = regmap_write(pTAS2562->regmap, reg, value);
	if (nResult < 0)
		dev_err(pTAS2562->dev, "%s, ERROR, reg=0x%x, E=%d\n",
			__func__, reg, nResult);
	else
		dev_err(pTAS2562->dev, "%s, reg: 0x%x, 0x%x\n",
			__func__, reg, value);

	mutex_unlock(&pTAS2562->dev_lock);

	return nResult;

}


static int tas2562_codec_suspend(struct snd_soc_codec *codec)
{
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&pTAS2562->codec_lock);

	dev_err(pTAS2562->dev, "%s\n", __func__);
	pTAS2562->runtime_suspend(pTAS2562);

	mutex_unlock(&pTAS2562->codec_lock);
	return ret;
}

static int tas2562_codec_resume(struct snd_soc_codec *codec)
{
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&pTAS2562->codec_lock);

	dev_err(pTAS2562->dev, "%s\n", __func__);
	pTAS2562->runtime_resume(pTAS2562);

	mutex_unlock(&pTAS2562->codec_lock);
	return ret;
}

static const struct snd_kcontrol_new tas2562_asi_controls[] = {
	SOC_DAPM_SINGLE("Left", TAS2562_TDMConfigurationReg2,
		4, 1, 0),
	SOC_DAPM_SINGLE("Right", TAS2562_TDMConfigurationReg2,
		4, 2, 0),
	SOC_DAPM_SINGLE("LeftRightDiv2", TAS2562_TDMConfigurationReg2,
		4, 3, 0),
};


static int tas2562_dac_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, TAS2562_PowerControl,
			TAS2562_PowerControl_OperationalMode10_Mask |
			TAS2562_PowerControl_ISNSPower_Mask |
			TAS2562_PowerControl_VSNSPower_Mask,
			TAS2562_PowerControl_OperationalMode10_Active |
			TAS2562_PowerControl_VSNSPower_Active |
			TAS2562_PowerControl_ISNSPower_Active);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, TAS2562_PowerControl,
			TAS2562_PowerControl_OperationalMode10_Mask,
			TAS2562_PowerControl_OperationalMode10_Shutdown);
		break;

	}
	return 0;

}

static const struct snd_soc_dapm_widget tas2562_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("ASI1", "ASI1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("Voltage Sense", "ASI1 Capture",  1, TAS2562_PowerControl, 2, 1),
	SND_SOC_DAPM_AIF_OUT("Current Sense", "ASI1 Capture",  0, TAS2562_PowerControl, 3, 1),
	SND_SOC_DAPM_MIXER("ASI1 Sel",
		TAS2562_TDMConfigurationReg2, 4, 0,
		&tas2562_asi_controls[0],
		ARRAY_SIZE(tas2562_asi_controls)),
	SND_SOC_DAPM_DAC_E("DAC", NULL, SND_SOC_NOPM, 0, 0, tas2562_dac_event,
	SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT"),
	SND_SOC_DAPM_SIGGEN("VMON"),
	SND_SOC_DAPM_SIGGEN("IMON")
};

static const struct snd_soc_dapm_route tas2562_audio_map[] = {
	{"ASI1 Sel", "Left", "ASI1"},
	{"ASI1 Sel", "Right", "ASI1"},
	{"ASI1 Sel", "LeftRightDiv2", "ASI1"},
	{"DAC", NULL, "ASI1 Sel"},
	{"OUT", NULL, "DAC"},
	/*{"VMON", NULL, "Voltage Sense"},
	{"IMON", NULL, "Current Sense"},*/
	{"Voltage Sense", NULL, "VMON"},
	{"Current Sense", NULL, "IMON"},
};


static int tas2562_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2562->codec_lock);
	if (mute) {
		snd_soc_update_bits(codec, TAS2562_PowerControl,
			TAS2562_PowerControl_OperationalMode10_Mask,
			TAS2562_PowerControl_OperationalMode10_Mute);
	} else {
		snd_soc_update_bits(codec, TAS2562_PowerControl,
			TAS2562_PowerControl_OperationalMode10_Mask,
			TAS2562_PowerControl_OperationalMode10_Active);
	}
	mutex_unlock(&pTAS2562->codec_lock);
	return 0;
}

static int tas2562_slot_config(struct snd_soc_codec *codec, struct tas2562_priv *pTAS2562, int blr_clk_ratio)
{
	int ret = 0;
		snd_soc_update_bits(codec,
			TAS2562_TDMConfigurationReg5, 0xff, 0x42);

		snd_soc_update_bits(codec,
			TAS2562_TDMConfigurationReg6, 0xff, 0x40);

	return ret;
}

//Added/Mofified 060356-PP
//To avoid implicit decleration 

static int tas2562_set_slot(struct snd_soc_codec *codec, int slot_width)
{
	int ret = 0;
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);

	switch (slot_width) {
	case 16:
	ret = snd_soc_update_bits(codec,
		TAS2562_TDMConfigurationReg2,
		TAS2562_TDMConfigurationReg2_RXSLEN10_Mask,
		TAS2562_TDMConfigurationReg2_RXSLEN10_16Bits);
	break;

	case 24:
	ret = snd_soc_update_bits(codec,
		TAS2562_TDMConfigurationReg2,
		TAS2562_TDMConfigurationReg2_RXSLEN10_Mask,
		TAS2562_TDMConfigurationReg2_RXSLEN10_24Bits);
	break;

	case 32:
	ret = snd_soc_update_bits(codec,
		TAS2562_TDMConfigurationReg2,
		TAS2562_TDMConfigurationReg2_RXSLEN10_Mask,
		TAS2562_TDMConfigurationReg2_RXSLEN10_32Bits);
	break;

	case 0:
	/* Do not change slot width */
	break;

	default:
		dev_err(pTAS2562->dev, "slot width not supported");
		ret = -EINVAL;
	}

	if (ret >= 0)
		pTAS2562->mnSlot_width = slot_width;

	return ret;
}

static int tas2562_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);
	int blr_clk_ratio;
	int ret = 0;
	int slot_width_tmp = 16;

	dev_err(pTAS2562->dev, "%s, format: %d\n", __func__,
		params_format(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(codec,
			TAS2562_TDMConfigurationReg2,
			TAS2562_TDMConfigurationReg2_RXWLEN32_Mask,
			TAS2562_TDMConfigurationReg2_RXWLEN32_16Bits);
			pTAS2562->ch_size = 16;
			if (pTAS2562->mnSlot_width == 0)
				slot_width_tmp = 16;
		break;
	case SNDRV_PCM_FMTBIT_S24_LE:
			snd_soc_update_bits(codec,
			TAS2562_TDMConfigurationReg2,
			TAS2562_TDMConfigurationReg2_RXWLEN32_Mask,
			TAS2562_TDMConfigurationReg2_RXWLEN32_24Bits);
			pTAS2562->ch_size = 24;
			if (pTAS2562->mnSlot_width == 0)
				slot_width_tmp = 32;
		break;
	case SNDRV_PCM_FMTBIT_S32_LE:
			snd_soc_update_bits(codec,
			TAS2562_TDMConfigurationReg2,
			TAS2562_TDMConfigurationReg2_RXWLEN32_Mask,
			TAS2562_TDMConfigurationReg2_RXWLEN32_32Bits);
			pTAS2562->ch_size = 32;
			if (pTAS2562->mnSlot_width == 0)
				slot_width_tmp = 32;
		break;

	}

	/* If machine driver did not call set slot width */
	if (pTAS2562->mnSlot_width == 0)
		tas2562_set_slot(codec, slot_width_tmp);

	blr_clk_ratio = params_channels(params) * pTAS2562->ch_size;
	dev_err(pTAS2562->dev, "blr_clk_ratio: %d\n", blr_clk_ratio);
	tas2562_slot_config(codec, pTAS2562, blr_clk_ratio);

	dev_err(pTAS2562->dev, "%s, sample rate: %d\n", __func__,
		params_rate(params));
	switch (params_rate(params)) {
	case 48000:
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATERAMP_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATERAMP_48KHz);
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATE31_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATE31_44_1_48kHz);
			break;
	case 44100:
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATERAMP_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATERAMP_44_1KHz);
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATE31_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATE31_44_1_48kHz);
			break;
	case 96000:
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATERAMP_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATERAMP_48KHz);
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATE31_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATE31_88_2_96kHz);
			break;
	case 88200:
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATERAMP_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATERAMP_44_1KHz);
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATE31_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATE31_88_2_96kHz);
			break;
	case 19200:
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATERAMP_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATERAMP_48KHz);
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATE31_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATE31_176_4_192kHz);
			break;
	case 17640:
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATERAMP_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATERAMP_44_1KHz);
			snd_soc_update_bits(codec,
				TAS2562_TDMConfigurationReg0,
				TAS2562_TDMConfigurationReg0_SAMPRATE31_Mask,
			TAS2562_TDMConfigurationReg0_SAMPRATE31_176_4_192kHz);
			break;
	default:
	dev_err(pTAS2562->dev, "%s, unsupported sample rate\n", __func__);

	}
	return ret;
}

static int tas2562_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	u8 tdm_rx_start_slot = 0, asi_cfg_1 = 0;
	struct snd_soc_codec *codec = dai->codec;
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_err(pTAS2562->dev, "%s, format=0x%x\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		asi_cfg_1 = 0x00;
		break;
	default:
		dev_err(pTAS2562->dev, "ASI format master is not found\n");
		ret = -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		dev_err(pTAS2562->dev, "INV format: NBNF\n");
		asi_cfg_1 |= TAS2562_TDMConfigurationReg1_RXEDGE_Rising;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		dev_err(pTAS2562->dev, "INV format: IBNF\n");
		asi_cfg_1 |= TAS2562_TDMConfigurationReg1_RXEDGE_Falling;
		break;
	default:
		dev_err(pTAS2562->dev, "ASI format Inverse is not found\n");
		ret = -EINVAL;
	}

	snd_soc_update_bits(codec, TAS2562_TDMConfigurationReg1,
		TAS2562_TDMConfigurationReg1_RXEDGE_Mask,
		asi_cfg_1);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case (SND_SOC_DAIFMT_I2S):
		tdm_rx_start_slot = 1;
		break;
	case (SND_SOC_DAIFMT_DSP_A):
	case (SND_SOC_DAIFMT_DSP_B):
		tdm_rx_start_slot = 1;
		break;
	case (SND_SOC_DAIFMT_LEFT_J):
		tdm_rx_start_slot = 0;
		break;
	default:
	dev_err(pTAS2562->dev, "DAI Format is not found, fmt=0x%x\n", fmt);
	ret = -EINVAL;
		break;
	}

	snd_soc_update_bits(codec, TAS2562_TDMConfigurationReg1,
		TAS2562_TDMConfigurationReg1_RXOFFSET51_Mask,
	(tdm_rx_start_slot << TAS2562_TDMConfigurationReg1_RXOFFSET51_Shift));
	return ret;
}

static int tas2562_set_dai_tdm_slot(struct snd_soc_dai *dai,
		unsigned int tx_mask, unsigned int rx_mask,
		int slots, int slot_width)
{
	int ret = 0;
	struct snd_soc_codec *codec = dai->codec;
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);

	dev_err(pTAS2562->dev, "%s, tx_mask:%d, rx_mask:%d, slots:%d, slot_width:%d",
			__func__, tx_mask, rx_mask, slots, slot_width);

	ret = tas2562_set_slot(codec, slot_width);

	return ret;
}

static struct snd_soc_dai_ops tas2562_dai_ops = {
	.digital_mute = tas2562_mute,
	.hw_params  = tas2562_hw_params,
	.set_fmt    = tas2562_set_dai_fmt,
	.set_tdm_slot = tas2562_set_dai_tdm_slot,
};

#define TAS2562_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define TAS2562_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 \
						SNDRV_PCM_RATE_88200 |\
						SNDRV_PCM_RATE_96000 |\
						SNDRV_PCM_RATE_176400 |\
						SNDRV_PCM_RATE_192000\
						)

static struct snd_soc_dai_driver tas2562_dai_driver[] = {
	{
		.name = "tas2562 ASI1",
		.id = 0,
		.playback = {
			.stream_name    = "ASI1 Playback",
			.channels_min   = 2,
			.channels_max   = 2,
			.rates      = SNDRV_PCM_RATE_8000_192000,
			.formats    = TAS2562_FORMATS,
		},
		.capture = {
			.stream_name    = "ASI1 Capture",
			.channels_min   = 0,
			.channels_max   = 2,
			.rates          = SNDRV_PCM_RATE_8000_192000,
			.formats    = TAS2562_FORMATS,
		},
		.ops = &tas2562_dai_ops,
		.symmetric_rates = 1,
	},
};

static int tas2562_codec_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct tas2562_priv *pTAS2562 = snd_soc_codec_get_drvdata(codec);

	ret = snd_soc_add_codec_controls(codec, tas2562_controls,
					 ARRAY_SIZE(tas2562_controls));
	if (ret < 0) {
		pr_err("%s: add_codec_controls failed, err %d\n",
			__func__, ret);
		return ret;
	}


	dev_err(pTAS2562->dev, "%s\n", __func__);

	return 0;
}

static int tas2562_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

/*static DECLARE_TLV_DB_SCALE(dac_tlv, 0, 100, 0);*/
static DECLARE_TLV_DB_SCALE(tas2562_digital_tlv, 1100, 50, 0);
static DECLARE_TLV_DB_SCALE(tas2562_playback_volume, -12750, 50, 0);

static const struct snd_kcontrol_new tas2562_snd_controls[] = {
	SOC_SINGLE_TLV("Amp Output Level", TAS2562_PlaybackConfigurationReg0,
		0, 0x16, 0,
		tas2562_digital_tlv),
	SOC_SINGLE_TLV("Playback Volume", TAS2562_PlaybackConfigurationReg2,
		0, TAS2562_PlaybackConfigurationReg2_DVCPCM70_Mask, 1,
		tas2562_playback_volume),
};

static struct snd_soc_codec_driver soc_codec_driver_tas2562 = {
	.probe			= tas2562_codec_probe,
	.remove			= tas2562_codec_remove,
	.read			= tas2562_codec_read,
	.write			= tas2562_codec_write,
	.suspend		= tas2562_codec_suspend,
	.resume			= tas2562_codec_resume,
	.component_driver = {
	.controls		= tas2562_snd_controls,
	.num_controls		= ARRAY_SIZE(tas2562_snd_controls),
		.dapm_widgets		= tas2562_dapm_widgets,
		.num_dapm_widgets	= ARRAY_SIZE(tas2562_dapm_widgets),
		.dapm_routes		= tas2562_audio_map,
		.num_dapm_routes	= ARRAY_SIZE(tas2562_audio_map),
	},
};

int tas2562_register_codec(struct tas2562_priv *pTAS2562)
{
	int nResult = 0;

	dev_info(pTAS2562->dev, "%s, enter\n", __func__);
	nResult = snd_soc_register_codec(pTAS2562->dev,
		&soc_codec_driver_tas2562,
		tas2562_dai_driver, ARRAY_SIZE(tas2562_dai_driver));
	return nResult;
}

int tas2562_deregister_codec(struct tas2562_priv *pTAS2562)
{
	snd_soc_unregister_codec(pTAS2562->dev);

	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2562 ALSA SOC Smart Amplifier driver");
MODULE_LICENSE("GPL v2");
#endif /* CONFIG_TAS2562_CODEC */

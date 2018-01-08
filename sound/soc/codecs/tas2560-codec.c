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
**     tas2560-codec.c
**
** Description:
**     ALSA SoC driver for Texas Instruments TAS2560 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2560_CODEC_STEREO

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
#include <linux/mfd/spk-id.h>

#include "tas2560.h"
#include "tas2560-core.h"

#define TAS2560_MDELAY 0xFFFFFFFE
#define KCONTROL_CODEC

#define MAX_CLIENTS 8
static struct i2c_client *g_client[MAX_CLIENTS];

static unsigned int tas2560_read(struct snd_soc_codec *codec,  unsigned int reg)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	unsigned int value = 0;
	int ret = 0;

	dev_err(pTAS2560->dev, "%s, should not get here\n", __func__);
	/*ret = pTAS2560->read(pTAS2560, reg, &value);*/
	if (ret >= 0)
		return value;
	else
		return ret;
}

static int tas2560_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_err(pTAS2560->dev, "%s, should not get here\n", __func__);

	return 1;/*pTAS2560->write(pTAS2560, reg, value);*/
}

static int tas2560_mix_post_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	enum channel mchannel;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	if (!strcmp(w->name, "SPK Mixer")) {
		mchannel = channel_right;
	} else if (!strcmp(w->name, "RCV Mixer")) {
		mchannel = channel_left;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(pTAS2560->dev, "SND_SOC_DAPM_POST_PMU");
		tas2560_enable(pTAS2560, true, mchannel);
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(pTAS2560->dev, "SND_SOC_DAPM_POST_PMD");
		tas2560_enable(pTAS2560, false, mchannel);
		break;
	}

	return 0;
}

static int tas2560_routing_get_audio_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;

	if (mc->shift)
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	pr_debug("%s: reg %x shift %x val %ld\n", __func__, mc->reg, mc->shift,
	ucontrol->value.integer.value[0]);

	return 0;
}

static int tas2560_routing_put_audio_mixer(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];

	if (ucontrol->value.integer.value[0]) {
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1, NULL);
	} else if (!ucontrol->value.integer.value[0]) {
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0, NULL);
	}

	return 1;
}

static const struct snd_kcontrol_new spk_mixer_controls[] = {
	SOC_SINGLE_EXT("SPK", SND_SOC_NOPM ,
	0, 1, 0, tas2560_routing_get_audio_mixer,
	tas2560_routing_put_audio_mixer),
};

static const struct snd_kcontrol_new rec_mixer_controls[] = {
	SOC_SINGLE_EXT("RCV", SND_SOC_NOPM,
	0, 1, 0, tas2560_routing_get_audio_mixer,
	tas2560_routing_put_audio_mixer),
};

static const struct snd_soc_dapm_widget tas2560_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("ASI1", "ASI1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUT_DRV("ClassD", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("SPK Mixer", SND_SOC_NOPM, 0, 0,
					spk_mixer_controls,
					ARRAY_SIZE(spk_mixer_controls),
					tas2560_mix_post_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RCV Mixer", SND_SOC_NOPM, 0, 0,
					rec_mixer_controls,
					ARRAY_SIZE(rec_mixer_controls),
					tas2560_mix_post_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_dapm_route tas2560_audio_map[] = {
	{"SPK Mixer", "SPK", "ASI1"},
	{"RCV Mixer", "RCV", "ASI1"},
	{"DAC", NULL, "SPK Mixer"},
	{"DAC", NULL, "RCV Mixer"},
	{"ClassD", NULL, "DAC"},
	{"OUT", NULL, "ClassD"},
	{"DAC", NULL, "PLL"},
};

static int tas2560_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	pTAS2560->mnClkid = -1;
	pTAS2560->mnClkin = -1;

	return 0;
}

static void tas2560_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
}

static int tas2560_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s, %d\n", __func__, mute);

	return 0;
}

static int tas2560_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				  unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	ret = tas2560_set_pll_clkin(pTAS2560, clk_id, freq);

	return ret;
}

static int tas2560_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int nResult = -1;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	nResult = tas2560_irq_detect(pTAS2560);

	tas2560_set_SampleRate(pTAS2560, params_rate(params));
	tas2560_set_bit_rate(pTAS2560, snd_pcm_format_width(params_format(params)));
	tas2560_set_ASI_fmt(pTAS2560, SND_SOC_DAIFMT_CBS_CFS|SND_SOC_DAIFMT_I2S|SND_SOC_DAIFMT_NB_NF);
	pTAS2560->mnFrameSize = snd_soc_params_to_frame_size(params);

	if (pTAS2560->mnClkid == -1) {
		tas2560_set_dai_sysclk(dai, TAS2560_PLL_CLKIN_BCLK, 0, 1);
	}

	tas2560_setupPLL(pTAS2560, pTAS2560->mnClkin);

	tas2560_setLoad(pTAS2560, channel_left, pTAS2560->mnLeftLoad);
	tas2560_setLoad(pTAS2560, channel_right, pTAS2560->mnRightLoad);
	return 0;
}

static int tas2560_prepare(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	return 0;
}

static int tas2560_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	dev_info(codec->dev, "%s, fmt=0x%x\n", __func__, fmt);
	ret = tas2560_set_ASI_fmt(pTAS2560, fmt);
	return ret;
}

static struct snd_soc_dai_ops tas2560_dai_ops = {
	.startup = tas2560_startup,
	.shutdown = tas2560_shutdown,
	.digital_mute = tas2560_mute,
	.hw_params  = tas2560_hw_params,
	.prepare    = tas2560_prepare,
	.set_sysclk = tas2560_set_dai_sysclk,
	.set_fmt    = tas2560_set_dai_fmt,
};

#define TAS2560_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)
static struct snd_soc_dai_driver tas2560_dai_driver[] = {
	{
		.name = "tas2560 Stereo ASI1",
		.id = 0,
		.playback = {
			.stream_name    = "ASI1 Playback",
			.channels_min   = 2,
			.channels_max   = 2,
			.rates      = SNDRV_PCM_RATE_8000_192000,
			.formats    = TAS2560_FORMATS,
		},
		.ops = &tas2560_dai_ops,
		.symmetric_rates = 1,
	},
};

static int tas2560_codec_probe(struct snd_soc_codec *codec)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct i2c_client *pClient;
	static int nClient;
	nClient = 0;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	snd_soc_dapm_ignore_suspend(dapm, "OUT");
	/*pTAS2560->codec = codec;*/

	/* DR boost */
	tas2560_dr_boost(pTAS2560);

	codec->control_data = g_client[nClient];
	if (nClient < MAX_CLIENTS - 1)
		nClient++;

	pClient = codec->control_data;

	return 0;
}

static int tas2560_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static int tas2560_get_load(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	if (!strcmp(pKcontrol->id.name, "TAS2560 Receiver Boostload"))
		pUcontrol->value.integer.value[0] = pTAS2560->mnLeftLoad;
	else
		pUcontrol->value.integer.value[0] = pTAS2560->mnRightLoad;

	return 0;
}

static int tas2560_set_load(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	dev_dbg(pCodec->dev, "%s:\n", __func__);

	if (!strcmp(pKcontrol->id.name, "TAS2560 Receiver Boostload")) {
		pTAS2560->mnLeftLoad = pUcontrol->value.integer.value[0];
		tas2560_setLoad(pTAS2560, channel_left, pTAS2560->mnLeftLoad);
	} else {
		pTAS2560->mnRightLoad = pUcontrol->value.integer.value[0];
		tas2560_setLoad(pTAS2560, channel_right, pTAS2560->mnRightLoad);
	}
	return 0;
}

static int tas2560_set_DAC_vol(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);
	int gain = pUcontrol->value.integer.value[0];
	dev_dbg(pCodec->dev, "%s:\n", __func__);

	if (!strcmp(pKcontrol->id.name, "TAS2560 Receiver DAC Volume"))
		pTAS2560->update_bits(pTAS2560, channel_left,
				TAS2560_SPK_CTRL_REG, 0x0f, gain);
	else
		pTAS2560->update_bits(pTAS2560, channel_right,
				TAS2560_SPK_CTRL_REG, 0x0f, gain);
	return 0;
}

static int tas2560_get_DAC_vol(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);
	int gain;

	dev_dbg(pCodec->dev, "%s:\n", __func__);

	if (!strcmp(pKcontrol->id.name, "TAS2560 Receiver DAC Volume"))
		pTAS2560->read(pTAS2560, channel_left, TAS2560_SPK_CTRL_REG,
				&gain);
	else
		pTAS2560->read(pTAS2560, channel_right, TAS2560_SPK_CTRL_REG,
				&gain);
	pUcontrol->value.integer.value[0] = gain&0x0f;
	return 0;
}

static int tas2560_set_mode(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);
	int mode = pUcontrol->value.integer.value[0];
	dev_dbg(pCodec->dev, "%s:\n", __func__);

	if (!strcmp(pKcontrol->id.name, "TAS2560 Receiver Mode"))
		pTAS2560->update_bits(pTAS2560, channel_left,
				TAS2560_DEV_MODE_REG, 0x03, mode);
	else
		pTAS2560->update_bits(pTAS2560, channel_right,
				TAS2560_DEV_MODE_REG, 0x03, mode);
	return 0;
}

static int tas2560_get_mode(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);
	int mode;

	dev_dbg(pCodec->dev, "%s:\n", __func__);

	if (!strcmp(pKcontrol->id.name, "TAS2560 Receiver Mode"))
		pTAS2560->read(pTAS2560, channel_left, TAS2560_DEV_MODE_REG,
				&mode);
	else
		pTAS2560->read(pTAS2560, channel_right, TAS2560_DEV_MODE_REG,
				&mode);
	pUcontrol->value.integer.value[0] = mode&0x03;
	return 0;
}

static int tas2560_get_Sampling_Rate(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	pUcontrol->value.integer.value[0] = pTAS2560->mnSamplingRate;
	dev_dbg(pCodec->dev, "%s: %d\n", __func__,
			pTAS2560->mnSamplingRate);
	return 0;
}

static int tas2560_set_Sampling_Rate(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	int sampleRate = pUcontrol->value.integer.value[0];
	dev_dbg(pCodec->dev, "%s: %d\n", __func__, sampleRate);
	tas2560_set_SampleRate(pTAS2560, sampleRate);

	return 0;
}

static int tas2560_power_ctrl_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	pValue->value.integer.value[0] = pTAS2560->mbPowerUp[0];
	dev_dbg(codec->dev, "tas2560_power_ctrl_get = 0x%x\n",
					pTAS2560->mbPowerUp[0]);

	return 0;
}

static int tas2560_power_ctrl_put(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	int bPowerUp = pValue->value.integer.value[0];
	tas2560_enable(pTAS2560, bPowerUp, channel_both);

	return 0;
}

static int vendor_id_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
		struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
		struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

		ucontrol->value.integer.value[0] = 0;

		if (pTAS2560->spk_id_gpio_p)
			ucontrol->value.integer.value[0] = spk_id_get(pTAS2560->spk_id_gpio_p);
		return 0;
}

static const char *load_text[] = {"8_Ohm", "6_Ohm", "4_Ohm"};

static const struct soc_enum load_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(load_text), load_text),
};

static const char *mode_text[] = {"Mode0", "Mode1", "Mode2"};

static const struct soc_enum mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mode_text), mode_text),
};

static const char *dac_vol_text[] = { "0db",  "1db",  "2db", \
				      "3db",  "4db",  "5db", \
				      "6db",  "7db",  "8db", \
				      "9db", "10db", "11db", \
				     "12db", "13db", "14db", "15db" \
				    };

static const struct soc_enum dac_vol_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dac_vol_text), dac_vol_text),
};

static const char *Sampling_Rate_text[] = {"48_khz", "44.1_khz",\
					   "16_khz", "8_khz"};

static const struct soc_enum Sampling_Rate_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Sampling_Rate_text), Sampling_Rate_text),
};

static const char *const vendor_id_text[] = {"None", "AAC", "SSI", "Unknown"};
static const struct soc_enum vendor_id[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(vendor_id_text), vendor_id_text),
};

/*
 * DAC digital volumes. From 0 to 15 dB in 1 dB steps
 */
static DECLARE_TLV_DB_SCALE(dac_tlv, 0, 100, 0);

static const struct snd_kcontrol_new tas2560_snd_controls[] = {
	SOC_SINGLE_TLV("DAC Playback Volume", TAS2560_SPK_CTRL_REG, 0, 0x0f, 0,
		dac_tlv),
	SOC_ENUM_EXT("TAS2560 Speaker Boostload", load_enum[0],
		      tas2560_get_load, tas2560_set_load),
	SOC_ENUM_EXT("TAS2560 Receiver Boostload", load_enum[0],
		      tas2560_get_load, tas2560_set_load),
	SOC_ENUM_EXT("TAS2560 Sampling Rate", Sampling_Rate_enum[0],
		      tas2560_get_Sampling_Rate, tas2560_set_Sampling_Rate),
	SOC_ENUM_EXT("TAS2560 Speaker Mode", mode_enum[0],
		      tas2560_get_mode, tas2560_set_mode),
	SOC_ENUM_EXT("TAS2560 Receiver Mode", mode_enum[0],
		      tas2560_get_mode, tas2560_set_mode),
	SOC_ENUM_EXT("TAS2560 Speaker DAC Volume", dac_vol_enum[0],
		      tas2560_get_DAC_vol, tas2560_set_DAC_vol),
	SOC_ENUM_EXT("TAS2560 Receiver DAC Volume", dac_vol_enum[0],
		      tas2560_get_DAC_vol, tas2560_set_DAC_vol),
	SOC_SINGLE_EXT("TAS2560 PowerCtrl", SND_SOC_NOPM, 0, 0x0001, 0,
			tas2560_power_ctrl_get, tas2560_power_ctrl_put),
	SOC_ENUM_EXT("SPK ID", vendor_id, vendor_id_get, NULL),

};


static struct snd_soc_codec_driver soc_codec_driver_tas2560 = {
	.probe			= tas2560_codec_probe,
	.remove			= tas2560_codec_remove,
	.read			= tas2560_read,
	.write			= tas2560_write,
	.controls		= tas2560_snd_controls,
	.num_controls		= ARRAY_SIZE(tas2560_snd_controls),
	.dapm_widgets		= tas2560_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(tas2560_dapm_widgets),
	.dapm_routes		= tas2560_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(tas2560_audio_map),
};

int tas2560_register_codec(struct tas2560_priv *pTAS2560)
{
	int nResult = 0;

	dev_info(pTAS2560->dev, "%s, enter\n", __func__);

	nResult = snd_soc_register_codec(pTAS2560->dev,
		&soc_codec_driver_tas2560,
		tas2560_dai_driver, ARRAY_SIZE(tas2560_dai_driver));

	return nResult;
}

int tas2560_deregister_codec(struct tas2560_priv *pTAS2560)
{
	snd_soc_unregister_codec(pTAS2560->dev);

	return 0;
}


MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2560 ALSA SOC Smart Amplifier driver");
MODULE_LICENSE("GPLv2");

#endif /*CONFIG_TAS2560_CODEC*/

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
**     tas2557-codec.c
**
** Description:
**     ALSA SoC driver for Texas Instruments TAS2557 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2557_CODEC_STEREO

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
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tas2557-core.h"
#include "tas2557-codec.h"

#define KCONTROL_CODEC

static unsigned int tas2557_codec_read(struct snd_soc_codec *pCodec,
	unsigned int nRegister)
{
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(pCodec);
	int ret = 0;
	unsigned int Value = 0;

	mutex_lock(&pTAS2557->codec_lock);

	ret = pTAS2557->read(pTAS2557,
		pTAS2557->mnCurrentChannel, nRegister, &Value);
	if (ret < 0)
		dev_err(pTAS2557->dev, "%s, %d, ERROR happen=%d\n", __func__,
			__LINE__, ret);
	else
		ret = Value;

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static int tas2557_codec_write(struct snd_soc_codec *pCodec, unsigned int nRegister,
	unsigned int nValue)
{
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(pCodec);
	int ret = 0;

	mutex_lock(&pTAS2557->codec_lock);

	ret = pTAS2557->write(pTAS2557,
		pTAS2557->mnCurrentChannel, nRegister, nValue);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static int tas2557_codec_suspend(struct snd_soc_codec *pCodec)
{
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(pCodec);
	int ret = 0;

	mutex_lock(&pTAS2557->codec_lock);

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
	pTAS2557->runtime_suspend(pTAS2557);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static int tas2557_codec_resume(struct snd_soc_codec *pCodec)
{
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(pCodec);
	int ret = 0;

	mutex_lock(&pTAS2557->codec_lock);

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
	pTAS2557->runtime_resume(pTAS2557);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static const struct snd_soc_dapm_widget tas2557_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("Stereo ASI1", "Stereo ASI1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("Stereo ASI2", "Stereo ASI2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("Stereo ASIM", "Stereo ASIM Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("Stereo DAC", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUT_DRV("Stereo ClassD", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("Stereo PLL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Stereo NDivider", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("Stereo OUT")
};

static const struct snd_soc_dapm_route tas2557_audio_map[] = {
	{"Stereo DAC", NULL, "Stereo ASI1"},
	{"Stereo DAC", NULL, "Stereo ASI2"},
	{"Stereo DAC", NULL, "Stereo ASIM"},
	{"Stereo ClassD", NULL, "Stereo DAC"},
	{"Stereo OUT", NULL, "Stereo ClassD"},
	{"Stereo DAC", NULL, "Stereo PLL"},
	{"Stereo DAC", NULL, "Stereo NDivider"},
};

static int tas2557_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
	return 0;
}

static void tas2557_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
}

static int tas2557_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2557->codec_lock);

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
	tas2557_enable(pTAS2557, !mute);

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_set_dai_sysclk(struct snd_soc_dai *pDAI,
	int nClkID, unsigned int nFreqency, int nDir)
{
	struct snd_soc_codec *pCodec = pDAI->codec;
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(pCodec);

	dev_dbg(pTAS2557->dev, "tas2557_set_dai_sysclk: freq = %u\n", nFreqency);

	return 0;
}

static int tas2557_hw_params(struct snd_pcm_substream *pSubstream,
	struct snd_pcm_hw_params *pParams, struct snd_soc_dai *pDAI)
{
	struct snd_soc_codec *pCodec = pDAI->codec;
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(pCodec);

	mutex_lock(&pTAS2557->codec_lock);

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
/* do bit rate setting during platform data */
/* tas2557_set_bit_rate(pTAS2557, channel_both, snd_pcm_format_width(params_format(pParams))); */
	tas2557_set_sampling_rate(pTAS2557, params_rate(pParams));

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_set_dai_fmt(struct snd_soc_dai *pDAI, unsigned int nFormat)
{
	struct snd_soc_codec *codec = pDAI->codec;
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
	return 0;
}

static int tas2557_prepare(struct snd_pcm_substream *pSubstream,
	struct snd_soc_dai *pDAI)
{
	struct snd_soc_codec *codec = pDAI->codec;
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
	return 0;
}

static int tas2557_set_bias_level(struct snd_soc_codec *pCodec,
	enum snd_soc_bias_level eLevel)
{
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(pCodec);

	dev_dbg(pTAS2557->dev, "%s: %d\n", __func__, eLevel);
	return 0;
}

static int tas2557_codec_probe(struct snd_soc_codec *pCodec)
{
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(pCodec);

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
	return 0;
}

static int tas2557_codec_remove(struct snd_soc_codec *pCodec)
{
	return 0;
}

static int tas2557_power_ctrl_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2557->codec_lock);

	pValue->value.integer.value[0] = pTAS2557->mbPowerUp;
	dev_dbg(pTAS2557->dev, "tas2557_power_ctrl_get = %d\n",
		pTAS2557->mbPowerUp);

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_power_ctrl_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	int nPowerOn = pValue->value.integer.value[0];

	mutex_lock(&pTAS2557->codec_lock);

	dev_dbg(pTAS2557->dev, "tas2557_power_ctrl_put = %d\n", nPowerOn);
	tas2557_enable(pTAS2557, (nPowerOn != 0));

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_fs_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	int nFS = 48000;

	mutex_lock(&pTAS2557->codec_lock);

	if (pTAS2557->mpFirmware->mnConfigurations)
		nFS = pTAS2557->mpFirmware->mpConfigurations[pTAS2557->mnCurrentConfiguration].mnSamplingRate;
	pValue->value.integer.value[0] = nFS;
	dev_dbg(pTAS2557->dev, "tas2557_fs_get = %d\n", nFS);

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_fs_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int nFS = pValue->value.integer.value[0];

	mutex_lock(&pTAS2557->codec_lock);

	dev_info(pTAS2557->dev, "tas2557_fs_put = %d\n", nFS);
	ret = tas2557_set_sampling_rate(pTAS2557, nFS);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static int tas2557_DevA_Cali_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	bool ret = 0;
	int prm_r0 = 0;

	mutex_lock(&pTAS2557->codec_lock);

	ret = tas2557_get_Cali_prm_r0(pTAS2557, channel_left, &prm_r0);
	if (ret)
		pValue->value.integer.value[0] = prm_r0;

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_DevB_Cali_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	bool ret = 0;
	int prm_r0 = 0;

	mutex_lock(&pTAS2557->codec_lock);

	ret = tas2557_get_Cali_prm_r0(pTAS2557, channel_right, &prm_r0);
	if (ret)
		pValue->value.integer.value[0] = prm_r0;

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_program_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2557->codec_lock);

	pValue->value.integer.value[0] = pTAS2557->mnCurrentProgram;
	dev_dbg(pTAS2557->dev, "tas2557_program_get = %d\n",
		pTAS2557->mnCurrentProgram);

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_program_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	unsigned int nProgram = pValue->value.integer.value[0];
	int ret = 0, nConfiguration = -1;

	mutex_lock(&pTAS2557->codec_lock);

	if (nProgram == pTAS2557->mnCurrentProgram)
		nConfiguration = pTAS2557->mnCurrentConfiguration;
	ret = tas2557_set_program(pTAS2557, nProgram, nConfiguration);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static int tas2557_configuration_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2557->codec_lock);

	pValue->value.integer.value[0] = pTAS2557->mnCurrentConfiguration;
	dev_dbg(pTAS2557->dev, "tas2557_configuration_get = %d\n",
		pTAS2557->mnCurrentConfiguration);

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_configuration_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	unsigned int nConfiguration = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2557->codec_lock);

	dev_info(pTAS2557->dev, "%s = %d\n", __func__, nConfiguration);
	ret = tas2557_set_config(pTAS2557, nConfiguration);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static int tas2557_calibration_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2557->codec_lock);

	pValue->value.integer.value[0] = pTAS2557->mnCurrentCalibration;
	dev_info(pTAS2557->dev,
		"tas2557_calibration_get = %d\n",
		pTAS2557->mnCurrentCalibration);

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_calibration_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	unsigned int nCalibration = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2557->codec_lock);

	ret = tas2557_set_calibration(pTAS2557, nCalibration);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static int tas2557_ldac_gain_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	unsigned char nGain = 0;
	int ret = -1;

	mutex_lock(&pTAS2557->codec_lock);

	ret = tas2557_get_DAC_gain(pTAS2557, channel_left, &nGain);
	if (ret >= 0)
		pValue->value.integer.value[0] = nGain;
	dev_dbg(pTAS2557->dev, "%s, ret = %d, %d\n", __func__, ret, nGain);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static int tas2557_ldac_gain_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	unsigned int nGain = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2557->codec_lock);

	ret = tas2557_set_DAC_gain(pTAS2557, channel_left, nGain);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static int tas2557_rdac_gain_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	unsigned char nGain = 0;
	int ret = -1;

	mutex_lock(&pTAS2557->codec_lock);

	ret = tas2557_get_DAC_gain(pTAS2557, channel_right, &nGain);
	if (ret >= 0)
		pValue->value.integer.value[0] = nGain;
	dev_dbg(pTAS2557->dev, "%s, ret = %d, %d\n", __func__, ret, nGain);

	mutex_unlock(&pTAS2557->codec_lock);

	return ret;
}

static int tas2557_rdac_gain_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	unsigned int nGain = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2557->codec_lock);

	ret = tas2557_set_DAC_gain(pTAS2557, channel_right, nGain);

	mutex_unlock(&pTAS2557->codec_lock);
	return ret;
}

static const char * const chl_setup_text[] = {
	"default",
	"DevA-Mute-DevB-Mute",
	"DevA-Left-DevB-Right",
	"DevA-Right-DevB-Left",
	"DevA-MonoMix-DevB-MonoMix"
};
static const struct soc_enum chl_setup_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(chl_setup_text), chl_setup_text),
};

static int tas2557_dsp_chl_setup_get(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2557->codec_lock);

	pValue->value.integer.value[0] = pTAS2557->mnChannelState;

	mutex_unlock(&pTAS2557->codec_lock);

	return 0;
}

static int tas2557_dsp_chl_setup_put(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	int channel_state = pValue->value.integer.value[0];

	mutex_lock(&pTAS2557->codec_lock);

	tas2557_SA_DevChnSetup(pTAS2557, channel_state);

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static const char * const vboost_ctl_text[] = {
	"default",
	"Device(s) AlwaysOn"
};

static const struct soc_enum vboost_ctl_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(vboost_ctl_text), vboost_ctl_text),
};

static int tas2557_vboost_ctl_get(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	int nResult = 0, nVBoost = 0;

	mutex_lock(&pTAS2557->codec_lock);

	nResult = tas2557_get_VBoost(pTAS2557, &nVBoost);
	if (nResult >= 0)
		pValue->value.integer.value[0] = nVBoost;

	mutex_unlock(&pTAS2557->codec_lock);

	return 0;
}

static int tas2557_vboost_ctl_put(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	int vboost_state = pValue->value.integer.value[0];

	mutex_lock(&pTAS2557->codec_lock);

	tas2557_set_VBoost(pTAS2557, vboost_state, pTAS2557->mbPowerUp);

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static const char * const vboost_volt_text[] = {
	"8.6V",
	"8.1V",
	"7.6V",
	"6.6V",
	"5.6V"
};

static const struct soc_enum vboost_volt_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(vboost_volt_text), vboost_volt_text),
};

static int tas2557_vboost_volt_get(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	int nVBstVolt = 0;

	mutex_lock(&pTAS2557->codec_lock);

	dev_dbg(pTAS2557->dev, "%s, VBoost volt %d\n", __func__, pTAS2557->mnVBoostVoltage);
	switch (pTAS2557->mnVBoostVoltage) {
	case TAS2557_VBST_8P5V:
		nVBstVolt = 0;
	break;

	case TAS2557_VBST_8P1V:
		nVBstVolt = 1;
	break;

	case TAS2557_VBST_7P6V:
		nVBstVolt = 2;
	break;

	case TAS2557_VBST_6P6V:
		nVBstVolt = 3;
	break;

	case TAS2557_VBST_5P6V:
		nVBstVolt = 4;
	break;

	default:
		dev_err(pTAS2557->dev, "%s, error volt %d\n", __func__, pTAS2557->mnVBoostVoltage);
	break;
	}

	pValue->value.integer.value[0] = nVBstVolt;

	mutex_unlock(&pTAS2557->codec_lock);

	return 0;
}

static int tas2557_vboost_volt_put(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	int vbstvolt = pValue->value.integer.value[0];

	mutex_lock(&pTAS2557->codec_lock);

	dev_dbg(pTAS2557->dev, "%s, volt %d\n", __func__, vbstvolt);
	switch (vbstvolt) {
	case 0:
		pTAS2557->mnVBoostVoltage = TAS2557_VBST_8P5V;
	break;

	case 1:
		pTAS2557->mnVBoostVoltage = TAS2557_VBST_8P1V;
	break;

	case 2:
		pTAS2557->mnVBoostVoltage = TAS2557_VBST_7P6V;
	break;

	case 3:
		pTAS2557->mnVBoostVoltage = TAS2557_VBST_6P6V;
	break;

	case 4:
		pTAS2557->mnVBoostVoltage = TAS2557_VBST_5P6V;
	break;

	default:
		dev_err(pTAS2557->dev, "%s, error volt %d\n", __func__, vbstvolt);
	break;
	}

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static const char * const echoref_ctl_text[] = {"left channel", "right channel", "both channel"};
static const struct soc_enum echoref_ctl_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(echoref_ctl_text), echoref_ctl_text),
};

static int tas2557_echoref_ctl_get(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2557->codec_lock);

	pValue->value.integer.value[0] = pTAS2557->mnEchoRef;

	mutex_unlock(&pTAS2557->codec_lock);

	return 0;
}

static int tas2557_echoref_ctl_put(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);
	int echoref = pValue->value.integer.value[0]&0x01;	/* only take care of left/right channel switch */

	mutex_lock(&pTAS2557->codec_lock);

	if (echoref != pTAS2557->mnEchoRef) {
		pTAS2557->mnEchoRef = echoref;
		tas2557_SA_ctl_echoRef(pTAS2557);
	}

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static const struct snd_kcontrol_new tas2557_snd_controls[] = {
	SOC_SINGLE_EXT("Stereo LDAC Playback Volume", SND_SOC_NOPM, 0, 0x0f, 0,
		tas2557_ldac_gain_get, tas2557_ldac_gain_put),
	SOC_SINGLE_EXT("Stereo RDAC Playback Volume", SND_SOC_NOPM, 0, 0x0f, 0,
		tas2557_rdac_gain_get, tas2557_rdac_gain_put),
	SOC_SINGLE_EXT("Stereo PowerCtrl", SND_SOC_NOPM, 0, 0x0001, 0,
		tas2557_power_ctrl_get, tas2557_power_ctrl_put),
	SOC_SINGLE_EXT("Stereo Program", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2557_program_get, tas2557_program_put),
	SOC_SINGLE_EXT("Stereo Configuration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2557_configuration_get, tas2557_configuration_put),
	SOC_SINGLE_EXT("Stereo FS", SND_SOC_NOPM, 8000, 48000, 0,
		tas2557_fs_get, tas2557_fs_put),
	SOC_SINGLE_EXT("Get DevA Cali_Re", SND_SOC_NOPM, 0, 0x7f000000, 0,
		tas2557_DevA_Cali_get, NULL),
	SOC_SINGLE_EXT("Get DevB Cali_Re", SND_SOC_NOPM, 0, 0x7f000000, 0,
		tas2557_DevB_Cali_get, NULL),
	SOC_SINGLE_EXT("Stereo Calibration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2557_calibration_get, tas2557_calibration_put),
	SOC_ENUM_EXT("Stereo DSPChl Setup", chl_setup_enum[0],
		tas2557_dsp_chl_setup_get, tas2557_dsp_chl_setup_put),
	SOC_ENUM_EXT("VBoost Ctrl", vboost_ctl_enum[0],
		tas2557_vboost_ctl_get, tas2557_vboost_ctl_put),
	SOC_ENUM_EXT("VBoost Volt", vboost_volt_enum[0],
		tas2557_vboost_volt_get, tas2557_vboost_volt_put),
	SOC_ENUM_EXT("Stereo EchoRef Ctrl", echoref_ctl_enum[0],
		tas2557_echoref_ctl_get, tas2557_echoref_ctl_put),
};

static struct snd_soc_codec_driver soc_codec_driver_tas2557 = {
	.probe = tas2557_codec_probe,
	.remove = tas2557_codec_remove,
	.read = tas2557_codec_read,
	.write = tas2557_codec_write,
	.suspend = tas2557_codec_suspend,
	.resume = tas2557_codec_resume,
	.set_bias_level = tas2557_set_bias_level,
	.idle_bias_off = true,
	.controls = tas2557_snd_controls,
	.num_controls = ARRAY_SIZE(tas2557_snd_controls),
	.dapm_widgets = tas2557_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tas2557_dapm_widgets),
	.dapm_routes = tas2557_audio_map,
	.num_dapm_routes = ARRAY_SIZE(tas2557_audio_map),
};

static struct snd_soc_dai_ops tas2557_dai_ops = {
	.startup = tas2557_startup,
	.shutdown = tas2557_shutdown,
	.digital_mute = tas2557_mute,
	.hw_params = tas2557_hw_params,
	.prepare = tas2557_prepare,
	.set_sysclk = tas2557_set_dai_sysclk,
	.set_fmt = tas2557_set_dai_fmt,
};

#define TAS2557_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)
static struct snd_soc_dai_driver tas2557_dai_driver[] = {
	{
		.name = "tas2557 Stereo ASI1",
		.id = 0,
		.playback = {
				.stream_name = "Stereo ASI1 Playback",
				.channels_min = 2,
				.channels_max = 2,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = TAS2557_FORMATS,
			},
		.ops = &tas2557_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tas2557 Stereo ASI2",
		.id = 1,
		.playback = {
				.stream_name = "Stereo ASI2 Playback",
				.channels_min = 2,
				.channels_max = 2,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = TAS2557_FORMATS,
			},
		.ops = &tas2557_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tas2557 Stereo ASIM",
		.id = 2,
		.playback = {
				.stream_name = "Stereo ASIM Playback",
				.channels_min = 2,
				.channels_max = 2,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = TAS2557_FORMATS,
			},
		.ops = &tas2557_dai_ops,
		.symmetric_rates = 1,
	},
};

int tas2557_register_codec(struct tas2557_priv *pTAS2557)
{
	int nResult = 0;

	dev_info(pTAS2557->dev, "%s, enter\n", __func__);
	nResult = snd_soc_register_codec(pTAS2557->dev,
		&soc_codec_driver_tas2557,
		tas2557_dai_driver, ARRAY_SIZE(tas2557_dai_driver));
	return nResult;
}

int tas2557_deregister_codec(struct tas2557_priv *pTAS2557)
{
	snd_soc_unregister_codec(pTAS2557->dev);
	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2557 ALSA SOC Smart Amplifier Stereo driver");
MODULE_LICENSE("GPL v2");
#endif

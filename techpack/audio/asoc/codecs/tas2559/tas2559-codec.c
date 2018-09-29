/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
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
**     tas2559-codec.c
**
** Description:
**     ALSA SoC driver for Texas Instruments TAS2559 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2559_CODEC

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

#include "tas2559-core.h"
#include "tas2559-codec.h"

#define KCONTROL_CODEC

static unsigned int tas2559_codec_read(struct snd_soc_codec *pCodec,
				       unsigned int nRegister)
{
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(pCodec);

	mutex_lock(&pTAS2559->codec_lock);
	dev_err(pTAS2559->dev, "%s, ERROR, shouldn't be here\n", __func__);
	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_codec_write(struct snd_soc_codec *pCodec, unsigned int nRegister,
			       unsigned int nValue)
{
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(pCodec);

	mutex_lock(&pTAS2559->codec_lock);
	dev_err(pTAS2559->dev, "%s, ERROR, shouldn't be here\n", __func__);
	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_codec_suspend(struct snd_soc_codec *pCodec)
{
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(pCodec);
	int ret = 0;

	mutex_lock(&pTAS2559->codec_lock);

	dev_dbg(pTAS2559->dev, "%s\n", __func__);
	pTAS2559->runtime_suspend(pTAS2559);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_codec_resume(struct snd_soc_codec *pCodec)
{
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(pCodec);
	int ret = 0;

	mutex_lock(&pTAS2559->codec_lock);

	dev_dbg(pTAS2559->dev, "%s\n", __func__);
	pTAS2559->runtime_resume(pTAS2559);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static const struct snd_soc_dapm_widget tas2559_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("ASI1", "ASI1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("ASI2", "ASI2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("ASIM", "ASIM Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUT_DRV("ClassD", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("NDivider", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_dapm_route tas2559_audio_map[] = {
	{"DAC", NULL, "ASI1"},
	{"DAC", NULL, "ASI2"},
	{"DAC", NULL, "ASIM"},
	{"ClassD", NULL, "DAC"},
	{"OUT", NULL, "ClassD"},
	{"DAC", NULL, "PLL"},
	{"DAC", NULL, "NDivider"},
};

static int tas2559_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);
	dev_dbg(pTAS2559->dev, "%s\n", __func__);
	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static void tas2559_shutdown(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);
	dev_dbg(pTAS2559->dev, "%s\n", __func__);
	mutex_unlock(&pTAS2559->codec_lock);
}

static int tas2559_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);

	dev_dbg(pTAS2559->dev, "%s\n", __func__);
	tas2559_enable(pTAS2559, !mute);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_set_dai_sysclk(struct snd_soc_dai *pDAI,
				  int nClkID, unsigned int nFreqency, int nDir)
{
	struct snd_soc_codec *pCodec = pDAI->codec;
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(pCodec);

	mutex_lock(&pTAS2559->codec_lock);
	dev_dbg(pTAS2559->dev, "%s: freq = %u\n", __func__, nFreqency);
	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_hw_params(struct snd_pcm_substream *pSubstream,
			     struct snd_pcm_hw_params *pParams, struct snd_soc_dai *pDAI)
{
	struct snd_soc_codec *pCodec = pDAI->codec;
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(pCodec);

	mutex_lock(&pTAS2559->codec_lock);

	dev_dbg(pTAS2559->dev, "%s\n", __func__);
	/* do bit rate setting during platform data */
	/* tas2559_set_bit_rate(pTAS2559, DevBoth, snd_pcm_format_width(params_format(pParams))); */
	tas2559_set_sampling_rate(pTAS2559, params_rate(pParams));

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_set_dai_fmt(struct snd_soc_dai *pDAI, unsigned int nFormat)
{
	struct snd_soc_codec *codec = pDAI->codec;
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);
	dev_dbg(pTAS2559->dev, "%s\n", __func__);
	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_prepare(struct snd_pcm_substream *pSubstream,
			   struct snd_soc_dai *pDAI)
{
	struct snd_soc_codec *codec = pDAI->codec;
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);
	dev_dbg(pTAS2559->dev, "%s\n", __func__);
	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_set_bias_level(struct snd_soc_codec *pCodec,
				  enum snd_soc_bias_level eLevel)
{
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(pCodec);

	mutex_lock(&pTAS2559->codec_lock);
	dev_dbg(pTAS2559->dev, "%s: %d\n", __func__, eLevel);
	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_codec_probe(struct snd_soc_codec *pCodec)
{
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(pCodec);

	dev_err(pTAS2559->dev, "%s\n", __func__);
	return 0;
}

static int tas2559_codec_remove(struct snd_soc_codec *pCodec)
{
	return 0;
}

static int tas2559_power_ctrl_get(struct snd_kcontrol *pKcontrol,
				  struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);

	pValue->value.integer.value[0] = pTAS2559->mbPowerUp;
	dev_dbg(pTAS2559->dev, "%s = %d\n", __func__, pTAS2559->mbPowerUp);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_power_ctrl_put(struct snd_kcontrol *pKcontrol,
				  struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	int nPowerOn = pValue->value.integer.value[0];

	mutex_lock(&pTAS2559->codec_lock);

	dev_dbg(pTAS2559->dev, "%s = %d\n", __func__, nPowerOn);
	tas2559_enable(pTAS2559, (nPowerOn != 0));

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_fs_get(struct snd_kcontrol *pKcontrol,
			  struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int nFS = 48000;

	mutex_lock(&pTAS2559->codec_lock);

	if (pTAS2559->mpFirmware->mnConfigurations)
		nFS = pTAS2559->mpFirmware->mpConfigurations[pTAS2559->mnCurrentConfiguration].mnSamplingRate;

	pValue->value.integer.value[0] = nFS;
	dev_dbg(pTAS2559->dev, "%s = %d\n", __func__, nFS);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_fs_put(struct snd_kcontrol *pKcontrol,
			  struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int nFS = pValue->value.integer.value[0];

	mutex_lock(&pTAS2559->codec_lock);

	dev_info(pTAS2559->dev, "%s = %d\n", __func__, nFS);
	ret = tas2559_set_sampling_rate(pTAS2559, nFS);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_DevA_Cali_get(struct snd_kcontrol *pKcontrol,
				 struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int prm_r0 = 0;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_get_Cali_prm_r0(pTAS2559, DevA, &prm_r0);
	pValue->value.integer.value[0] = prm_r0;
	dev_dbg(pTAS2559->dev, "%s = 0x%x\n", __func__, prm_r0);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_DevB_Cali_get(struct snd_kcontrol *pKcontrol,
				 struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int prm_r0 = 0;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_get_Cali_prm_r0(pTAS2559, DevB, &prm_r0);
	pValue->value.integer.value[0] = prm_r0;
	dev_dbg(pTAS2559->dev, "%s = 0x%x\n", __func__, prm_r0);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_program_get(struct snd_kcontrol *pKcontrol,
			       struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);

	pValue->value.integer.value[0] = pTAS2559->mnCurrentProgram;
	dev_dbg(pTAS2559->dev, "%s = %d\n", __func__,
		pTAS2559->mnCurrentProgram);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_program_put(struct snd_kcontrol *pKcontrol,
			       struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	unsigned int nProgram = pValue->value.integer.value[0];
	int ret = 0, nConfiguration = -1;

	mutex_lock(&pTAS2559->codec_lock);

	if (nProgram == pTAS2559->mnCurrentProgram)
		nConfiguration = pTAS2559->mnCurrentConfiguration;

	ret = tas2559_set_program(pTAS2559, nProgram, nConfiguration);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_configuration_get(struct snd_kcontrol *pKcontrol,
				     struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);

	pValue->value.integer.value[0] = pTAS2559->mnCurrentConfiguration;
	dev_dbg(pTAS2559->dev, "%s = %d\n", __func__,
		pTAS2559->mnCurrentConfiguration);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_configuration_put(struct snd_kcontrol *pKcontrol,
				     struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	unsigned int nConfiguration = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2559->codec_lock);

	dev_info(pTAS2559->dev, "%s = %d\n", __func__, nConfiguration);
	ret = tas2559_set_config(pTAS2559, nConfiguration);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_calibration_get(struct snd_kcontrol *pKcontrol,
				   struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);

	pValue->value.integer.value[0] = pTAS2559->mnCurrentCalibration;
	dev_info(pTAS2559->dev, "%s = %d\n", __func__,
		 pTAS2559->mnCurrentCalibration);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_calibration_put(struct snd_kcontrol *pKcontrol,
				   struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	unsigned int nCalibration = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_set_calibration(pTAS2559, nCalibration);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_ldac_gain_get(struct snd_kcontrol *pKcontrol,
				 struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	unsigned char nGain = 0;
	int ret = -1;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_get_DAC_gain(pTAS2559, DevA, &nGain);

	if (ret >= 0)
		pValue->value.integer.value[0] = nGain;

	dev_dbg(pTAS2559->dev, "%s, ret = %d, %d\n", __func__, ret, nGain);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_ldac_gain_put(struct snd_kcontrol *pKcontrol,
				 struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	unsigned int nGain = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_set_DAC_gain(pTAS2559, DevA, nGain);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_rdac_gain_get(struct snd_kcontrol *pKcontrol,
				 struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	unsigned char nGain = 0;
	int ret = -1;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_get_DAC_gain(pTAS2559, DevB, &nGain);

	if (ret >= 0)
		pValue->value.integer.value[0] = nGain;

	dev_dbg(pTAS2559->dev, "%s, ret = %d, %d\n", __func__, ret, nGain);

	mutex_unlock(&pTAS2559->codec_lock);

	return ret;
}

static int tas2559_rdac_gain_put(struct snd_kcontrol *pKcontrol,
				 struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	unsigned int nGain = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_set_DAC_gain(pTAS2559, DevB, nGain);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static const char * const dev_mute_text[] = {
	"Mute",
	"Unmute"
};

static const struct soc_enum dev_mute_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dev_mute_text), dev_mute_text),
};

static int tas2559_dev_a_mute_get(struct snd_kcontrol *pKcontrol,
		struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	bool nMute = 0;
	int ret = -1;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_DevMuteStatus(pTAS2559, DevA, &nMute);
	if (ret >= 0)
		pValue->value.integer.value[0] = nMute;
	dev_dbg(pTAS2559->dev, "%s, ret = %d, %d\n", __func__, ret, nMute);

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_dev_a_mute_put(struct snd_kcontrol *pKcontrol,
		struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	unsigned int nMute = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_DevMute(pTAS2559, DevA, (nMute == 0));

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static int tas2559_dev_b_mute_get(struct snd_kcontrol *pKcontrol,
		struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	bool nMute = 0;
	int ret = -1;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_DevMuteStatus(pTAS2559, DevB, &nMute);
	if (ret >= 0)
		pValue->value.integer.value[0] = nMute;
	dev_dbg(pTAS2559->dev, "%s, ret = %d, %d\n", __func__, ret, nMute);

	mutex_unlock(&pTAS2559->codec_lock);

	return ret;
}

static int tas2559_dev_b_mute_put(struct snd_kcontrol *pKcontrol,
		struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	unsigned int nMute = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2559->codec_lock);

	ret = tas2559_DevMute(pTAS2559, DevB, (nMute == 0));

	mutex_unlock(&pTAS2559->codec_lock);
	return ret;
}

static const char *const chl_setup_text[] = {
	"default",
	"DevA-Mute-DevB-Mute",
	"DevA-Left-DevB-Right",
	"DevA-Right-DevB-Left",
	"DevA-MonoMix-DevB-MonoMix"
};

static const struct soc_enum chl_setup_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(chl_setup_text), chl_setup_text),
};

static int tas2559_dsp_chl_setup_get(struct snd_kcontrol *pKcontrol,
				     struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);

	pValue->value.integer.value[0] = pTAS2559->mnChannelState;

	mutex_unlock(&pTAS2559->codec_lock);

	return 0;
}

static int tas2559_dsp_chl_setup_put(struct snd_kcontrol *pKcontrol,
				     struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int channel_state = pValue->value.integer.value[0];

	mutex_lock(&pTAS2559->codec_lock);

	tas2559_SA_DevChnSetup(pTAS2559, channel_state);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static const char * const vboost_ctl_text[] = {
	"Default",
	"AlwaysOn"
};

static const struct soc_enum vboost_ctl_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(vboost_ctl_text), vboost_ctl_text),
};

static int tas2559_vboost_ctl_get(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int nResult = 0, nVBoost = 0;

	mutex_lock(&pTAS2559->codec_lock);

	nResult = tas2559_get_VBoost(pTAS2559, &nVBoost);
	if (nResult >= 0)
		pValue->value.integer.value[0] = nVBoost;

	mutex_unlock(&pTAS2559->codec_lock);

	return 0;
}

static int tas2559_vboost_ctl_put(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int vboost_state = pValue->value.integer.value[0];

	mutex_lock(&pTAS2559->codec_lock);

	tas2559_set_VBoost(pTAS2559, vboost_state, pTAS2559->mbPowerUp);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static const char * const vboost_volt_text[] = {
	"Default",
	"8.6V", /* (PPG 0dB) */
	"8.1V", /* (PPG -1dB) */
	"7.6V", /* (PPG -2dB) */
	"6.6V", /* (PPG -3dB) */
	"5.6V"  /* (PPG -4dB) */
};

static const struct soc_enum vboost_volt_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(vboost_volt_text), vboost_volt_text),
};

static int tas2559_vboost_volt_get(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int nVBstVolt = 0;

	mutex_lock(&pTAS2559->codec_lock);

	switch (pTAS2559->mnVBoostVoltage) {
	case TAS2559_VBST_8P5V:
		nVBstVolt = 1;
	break;

	case TAS2559_VBST_8P1V:
		nVBstVolt = 2;
	break;

	case TAS2559_VBST_7P6V:
		nVBstVolt = 3;
	break;

	case TAS2559_VBST_6P6V:
		nVBstVolt = 4;
	break;

	case TAS2559_VBST_5P6V:
		nVBstVolt = 5;
	break;
	}

	pValue->value.integer.value[0] = nVBstVolt;

	mutex_unlock(&pTAS2559->codec_lock);

	return 0;
}

static int tas2559_vboost_volt_put(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int vbstvolt = pValue->value.integer.value[0];

	mutex_lock(&pTAS2559->codec_lock);

	dev_dbg(pTAS2559->dev, "%s, volt %d\n", __func__, vbstvolt);
	tas2559_set_VBstVolt(pTAS2559, vbstvolt);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static const char *const echoref_ctl_text[] = {"DevA", "DevB", "DevBoth"};
static const struct soc_enum echoref_ctl_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(echoref_ctl_text), echoref_ctl_text),
};

static int tas2559_echoref_ctl_get(struct snd_kcontrol *pKcontrol,
				   struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);

	pValue->value.integer.value[0] = pTAS2559->mnEchoRef;

	mutex_unlock(&pTAS2559->codec_lock);

	return 0;
}

static int tas2559_echoref_ctl_put(struct snd_kcontrol *pKcontrol,
				   struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);
	int echoref = pValue->value.integer.value[0] & 0x01;	/* only take care of left/right channel switch */

	mutex_lock(&pTAS2559->codec_lock);

	if (echoref != pTAS2559->mnEchoRef) {
		pTAS2559->mnEchoRef = echoref;
		tas2559_SA_ctl_echoRef(pTAS2559);
	}

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_mute_ctrl_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2559->codec_lock);

	pValue->value.integer.value[0] = pTAS2559->mbMute;
	dev_dbg(pTAS2559->dev, "tas2559_mute_ctrl_get = %d\n",
		pTAS2559->mbMute);

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static int tas2559_mute_ctrl_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2559_priv *pTAS2559 = snd_soc_codec_get_drvdata(codec);

	int mbMute = pValue->value.integer.value[0];

	mutex_lock(&pTAS2559->codec_lock);

	dev_dbg(pTAS2559->dev, "tas2559_mute_ctrl_put = %d\n", mbMute);

	pTAS2559->mbMute = !!mbMute;

	mutex_unlock(&pTAS2559->codec_lock);
	return 0;
}

static const char *const vendor_id_text[] = {"None", "AAC", "SSI", "GOER", "Unknown"};
static const struct soc_enum vendor_id[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(vendor_id_text), vendor_id_text),
};

static int vendor_id_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	(void)kcontrol;
	ucontrol->value.integer.value[0] = 1;
	return 0;
}

static const struct snd_kcontrol_new tas2559_snd_controls[] = {
	SOC_SINGLE_EXT("TAS2559 DAC Playback Volume", SND_SOC_NOPM, 0, 0x0f, 0,
		tas2559_ldac_gain_get, tas2559_ldac_gain_put),
	SOC_SINGLE_EXT("TAS2560 DAC Playback Volume", SND_SOC_NOPM, 0, 0x0f, 0,
		tas2559_rdac_gain_get, tas2559_rdac_gain_put),
	SOC_SINGLE_EXT("PowerCtrl", SND_SOC_NOPM, 0, 0x0001, 0,
		tas2559_power_ctrl_get, tas2559_power_ctrl_put),
	SOC_SINGLE_EXT("Program", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2559_program_get, tas2559_program_put),
	SOC_SINGLE_EXT("Configuration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2559_configuration_get, tas2559_configuration_put),
	SOC_SINGLE_EXT("FS", SND_SOC_NOPM, 8000, 48000, 0,
		tas2559_fs_get, tas2559_fs_put),
	SOC_SINGLE_EXT("Get DevA Cali_Re", SND_SOC_NOPM, 0, 0x7f000000, 0,
		tas2559_DevA_Cali_get, NULL),
	SOC_SINGLE_EXT("Get DevB Cali_Re", SND_SOC_NOPM, 0, 0x7f000000, 0,
		tas2559_DevB_Cali_get, NULL),
	SOC_SINGLE_EXT("Calibration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2559_calibration_get, tas2559_calibration_put),
	SOC_ENUM_EXT("Stereo DSPChl Setup", chl_setup_enum[0],
		tas2559_dsp_chl_setup_get, tas2559_dsp_chl_setup_put),
	SOC_ENUM_EXT("VBoost Ctrl", vboost_ctl_enum[0],
		tas2559_vboost_ctl_get, tas2559_vboost_ctl_put),
	SOC_ENUM_EXT("VBoost Volt", vboost_volt_enum[0],
		tas2559_vboost_volt_get, tas2559_vboost_volt_put),
	SOC_ENUM_EXT("Stereo EchoRef Ctrl", echoref_ctl_enum[0],
		tas2559_echoref_ctl_get, tas2559_echoref_ctl_put),
	SOC_ENUM_EXT("TAS2559 Mute", dev_mute_enum[0],
		tas2559_dev_a_mute_get, tas2559_dev_a_mute_put),
	SOC_ENUM_EXT("TAS2560 Mute", dev_mute_enum[0],
		tas2559_dev_b_mute_get, tas2559_dev_b_mute_put),
	SOC_SINGLE_EXT("SmartPA Mute", SND_SOC_NOPM, 0, 0x0001, 0,
			tas2559_mute_ctrl_get, tas2559_mute_ctrl_put),
	SOC_ENUM_EXT("SPK ID", vendor_id, vendor_id_get, NULL),
};

static struct snd_soc_codec_driver soc_codec_driver_tas2559 = {
	.probe = tas2559_codec_probe,
	.remove = tas2559_codec_remove,
	.read = tas2559_codec_read,
	.write = tas2559_codec_write,
	.suspend = tas2559_codec_suspend,
	.resume = tas2559_codec_resume,
	.set_bias_level = tas2559_set_bias_level,
	.idle_bias_off = true,

	.component_driver = {
		.controls = tas2559_snd_controls,
		.num_controls = ARRAY_SIZE(tas2559_snd_controls),
		.dapm_widgets = tas2559_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(tas2559_dapm_widgets),
		.dapm_routes = tas2559_audio_map,
		.num_dapm_routes = ARRAY_SIZE(tas2559_audio_map),
	},
};

static struct snd_soc_dai_ops tas2559_dai_ops = {
	.startup = tas2559_startup,
	.shutdown = tas2559_shutdown,
	.digital_mute = tas2559_mute,
	.hw_params = tas2559_hw_params,
	.prepare = tas2559_prepare,
	.set_sysclk = tas2559_set_dai_sysclk,
	.set_fmt = tas2559_set_dai_fmt,
};

#define TAS2559_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)
static struct snd_soc_dai_driver tas2559_dai_driver[] = {
	{
		.name = "tas2559 ASI1",
		.id = 0,
		.playback = {
			.stream_name = "ASI1 Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = TAS2559_FORMATS,
		},
		.ops = &tas2559_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tas2559 ASI2",
		.id = 1,
		.playback = {
			.stream_name = "ASI2 Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = TAS2559_FORMATS,
		},
		.ops = &tas2559_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tas2559 ASIM",
		.id = 2,
		.playback = {
			.stream_name = "ASIM Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = TAS2559_FORMATS,
		},
		.ops = &tas2559_dai_ops,
		.symmetric_rates = 1,
	},
};

int tas2559_register_codec(struct tas2559_priv *pTAS2559)
{
	int nResult = 0;

	dev_info(pTAS2559->dev, "%s, enter\n", __func__);
	nResult = snd_soc_register_codec(pTAS2559->dev,
					 &soc_codec_driver_tas2559,
					 tas2559_dai_driver, ARRAY_SIZE(tas2559_dai_driver));
	return nResult;
}

int tas2559_deregister_codec(struct tas2559_priv *pTAS2559)
{
	snd_soc_unregister_codec(pTAS2559->dev);
	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2559 ALSA SOC Smart Amplifier Stereo driver");
MODULE_LICENSE("GPL v2");
#endif

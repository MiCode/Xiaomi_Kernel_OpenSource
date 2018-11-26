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
**     tas2557-codec.c
**
** Description:
**     ALSA SoC driver for Texas Instruments TAS2557 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2557_CODEC

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
#include <soc/qcom/socinfo.h>

#include "tas2557-core.h"
#include "tas2557-codec.h"
#include <linux/mfd/spk-id.h>

#define KCONTROL_CODEC

static unsigned int tas2557_codec_read(struct snd_soc_codec *pCodec,
	unsigned int nRegister)
{
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(pCodec);
	int ret = 0;
	unsigned int Value = 0;

	mutex_lock(&pTAS2557->codec_lock);

	ret = pTAS2557->read(pTAS2557, nRegister, &Value);
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

	ret = pTAS2557->write(pTAS2557, nRegister, nValue);

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
	SND_SOC_DAPM_AIF_IN("ASI1", "ASI1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("ASI2", "ASI2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("ASIM", "ASIM Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUT_DRV("ClassD", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("PLL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("NDivider", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_dapm_route tas2557_audio_map[] = {
	{"DAC", NULL, "ASI1"},
	{"DAC", NULL, "ASI2"},
	{"DAC", NULL, "ASIM"},
	{"ClassD", NULL, "DAC"},
	{"OUT", NULL, "ClassD"},
	{"DAC", NULL, "PLL"},
	{"DAC", NULL, "NDivider"},
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
static int tas2557_mute_ctrl_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2557->codec_lock);

	pValue->value.integer.value[0] = pTAS2557->mbMute;
	dev_dbg(pTAS2557->dev, "tas2557_mute_ctrl_get = %d\n",
		pTAS2557->mbMute);

	mutex_unlock(&pTAS2557->codec_lock);
	return 0;
}

static int tas2557_mute_ctrl_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

	int mbMute = pValue->value.integer.value[0];

	mutex_lock(&pTAS2557->codec_lock);

	dev_dbg(pTAS2557->dev, "tas2557_mute_ctrl_put = %d\n", mbMute);

	tas2557_permanent_mute(pTAS2557, mbMute);

	mutex_unlock(&pTAS2557->codec_lock);
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

static int vendor_id_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
		struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
		struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

		ucontrol->value.integer.value[0] = VENDOR_ID_NONE;

		if (pTAS2557->spk_id_gpio_p)
			ucontrol->value.integer.value[0] = spk_id_get(pTAS2557->spk_id_gpio_p);

		if (get_hw_version_platform() == HARDWARE_PLATFORM_URSA)
			ucontrol->value.integer.value[0] = VENDOR_ID_GOER;
		return 0;
}

static int pa_version_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
		struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
		struct tas2557_priv *pTAS2557 = snd_soc_codec_get_drvdata(codec);

		ucontrol->value.integer.value[0] = 0;

		if (pTAS2557->mnPGID == TAS2557_PG_VERSION_1P0)
			ucontrol->value.integer.value[0] = 1;
		else if(pTAS2557->mnPGID == TAS2557_PG_VERSION_2P0)
			ucontrol->value.integer.value[0] = 2;
		else if(pTAS2557->mnPGID == TAS2557_PG_VERSION_2P1)
			ucontrol->value.integer.value[0] = 3;
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

static int tas2557_Cali_get(struct snd_kcontrol *pKcontrol,
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

	ret = tas2557_get_Cali_prm_r0(pTAS2557, &prm_r0);
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

static const char *const vendor_id_text[] = {"None", "AAC", "SSI", "GOER", "Unknown"};
static const struct soc_enum vendor_id[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(vendor_id_text), vendor_id_text),
};

static const char *const pa_version_text[] = {"Unknown", "tas2557_v1.0", "tas2557_v2.0", "tas2557_v2.1"};
static const struct soc_enum pa_version[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pa_version_text), pa_version_text),
};

static const struct snd_kcontrol_new tas2557_snd_controls[] = {
	SOC_SINGLE_EXT("PowerCtrl", SND_SOC_NOPM, 0, 0x0001, 0,
		tas2557_power_ctrl_get, tas2557_power_ctrl_put),
	SOC_SINGLE_EXT("Program", SND_SOC_NOPM, 0, 0x00FF, 0, tas2557_program_get,
		tas2557_program_put),
	SOC_SINGLE_EXT("Configuration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2557_configuration_get, tas2557_configuration_put),
	SOC_SINGLE_EXT("FS", SND_SOC_NOPM, 8000, 48000, 0,
		tas2557_fs_get, tas2557_fs_put),
	SOC_SINGLE_EXT("Get Cali_Re", SND_SOC_NOPM, 0, 0x7f000000, 0,
		tas2557_Cali_get, NULL),
	SOC_SINGLE_EXT("Calibration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2557_calibration_get, tas2557_calibration_put),
	SOC_ENUM_EXT("SPK ID", vendor_id, vendor_id_get, NULL),
	SOC_ENUM_EXT("SmartPA Version", pa_version, pa_version_get, NULL),
	SOC_SINGLE_EXT("SmartPA Mute", SND_SOC_NOPM, 0, 0x0001, 0,
		tas2557_mute_ctrl_get, tas2557_mute_ctrl_put),

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
	.component_driver = {
		.controls = tas2557_snd_controls,
		.num_controls = ARRAY_SIZE(tas2557_snd_controls),
		.dapm_widgets = tas2557_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(tas2557_dapm_widgets),
		.dapm_routes = tas2557_audio_map,
		.num_dapm_routes = ARRAY_SIZE(tas2557_audio_map),
	},
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
		.name = "tas2557 ASI1",
		.id = 0,
		.playback = {
				.stream_name = "ASI1 Playback",
				.channels_min = 2,
				.channels_max = 2,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = TAS2557_FORMATS,
			},
		.ops = &tas2557_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tas2557 ASI2",
		.id = 1,
		.playback = {
				.stream_name = "ASI2 Playback",
				.channels_min = 2,
				.channels_max = 2,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = TAS2557_FORMATS,
			},
		.ops = &tas2557_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tas2557 ASIM",
		.id = 2,
		.playback = {
				.stream_name = "ASIM Playback",
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
MODULE_DESCRIPTION("TAS2557 ALSA SOC Smart Amplifier driver");
MODULE_LICENSE("GPL v2");
#endif

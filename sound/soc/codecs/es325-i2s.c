/*
 * es325-i2s.c  --  Audience eS325 I2S interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include <linux/i2c/esxxx.h> /* TODO: common location for i2c and slimbus */
#include "es325.h"

static int es325_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	return rc;
}

static int es325_i2s_set_pll(struct snd_soc_dai *dai, int pll_id,
			     int source, unsigned int freq_in,
			     unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	return rc;
}

static int es325_i2s_set_clkdiv(struct snd_soc_dai *dai, int div_id,
				int div)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	return rc;
}

static int es325_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	return rc;
}

static int es325_i2s_set_tdm_slot(struct snd_soc_dai *dai,
				  unsigned int tx_mask, unsigned int rx_mask,
				  int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	return rc;
}

static int es325_i2s_set_channel_map(struct snd_soc_dai *dai,
				     unsigned int tx_num, unsigned int *tx_slot,
				     unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	return rc;
}

static int es325_i2s_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;
	//unsigned int paramid = 0;
	unsigned int val = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	switch (dai->id) {
	case ES325_I2S_PORTA:
		break;
	case ES325_I2S_PORTB:
		break;
	case ES325_I2S_PORTC:
		break;
	case ES325_I2S_PORTD:
		break;
	default:
		return -EINVAL;
	}

	if (tristate)
		val = 0x0001;
	else
		val = 0x0000;

	//return snd_soc_write(codec, paramid, val);
	return 0;
}

static int es325_i2s_port_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	/*unsigned int paramid = 0;*/
	unsigned int val = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	/* Is this valid since DACs are not statically mapped to DAIs? */
	switch (dai->id) {
	case ES325_I2S_PORTA:
		break;
	case ES325_I2S_PORTB:
		break;
	case ES325_I2S_PORTC:
		break;
	case ES325_I2S_PORTD:
		break;
	default:
		return -EINVAL;
	}

	if (mute)
		val = 0x0000;
	else
		val = 0x0001;

	/*return snd_soc_write(codec, paramid, val);*/
	return 0;
}

static int es325_i2s_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	return rc;
}

static void es325_i2s_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);
}

static int es325_i2s_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	/* struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec); */
	int bits_per_sample = 0;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);
	switch (dai->id) {
	case ES325_I2S_PORTA:
		dev_dbg(codec->dev, "%s(): ES325_PORTA_PARAM_ID\n", __func__);
		break;
	case ES325_I2S_PORTB:
		dev_dbg(codec->dev, "%s(): ES325_PORTB_PARAM_ID\n", __func__);
		break;
	case ES325_I2S_PORTC:
		dev_dbg(codec->dev, "%s(): ES325_PORTC_PARAMID\n", __func__);
		break;
	case ES325_I2S_PORTD:
		dev_dbg(codec->dev, "%s(): ES325_PORTD_PARAMID\n", __func__);
		break;
	default:
		dev_err(codec->dev, "%s(): unknown I2S port\n", __func__);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s(): params_channels(params) = %d\n", __func__,
		params_channels(params));
	switch (params_channels(params)) {
	case 1:
		dev_dbg(codec->dev, "%s(): 1 channel\n", __func__);
		break;
	case 2:
		dev_dbg(codec->dev, "%s(): 2 channels\n", __func__);
		break;
	case 4:
		dev_dbg(codec->dev, "%s(): 4 channels\n", __func__);
		break;
	default:
		dev_dbg(codec->dev, "%s(): unsupported number of channels\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s(): params_rate(params) = %d\n", __func__,
		params_rate(params));
	switch (params_rate(params)) {
	case 8000:
		dev_dbg(codec->dev, "%s(): 8000Hz\n", __func__);
		break;
	case 11025:
		dev_dbg(codec->dev, "%s(): 11025\n", __func__);
		break;
	case 16000:
		dev_dbg(codec->dev, "%s(): 16000\n", __func__);
		break;
	case 22050:
		dev_dbg(codec->dev, "%s(): 22050\n", __func__);
		break;
	case 24000:
		dev_dbg(codec->dev, "%s(): 24000\n", __func__);
		break;
	case 32000:
		dev_dbg(codec->dev, "%s(): 32000\n", __func__);
		break;
	case 44100:
		dev_dbg(codec->dev, "%s(): 44100\n", __func__);
		break;
	case 48000:
		dev_dbg(codec->dev, "%s(): 48000\n", __func__);
		break;
	case 96000:
		dev_dbg(codec->dev, "%s(): 96000\n", __func__);
		break;
	case 192000:
		dev_dbg(codec->dev, "%s(): 192000\n", __func__);
		break;
	default:
		dev_err(codec->dev, "%s(): unsupported rate = %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dev_dbg(codec->dev, "%s(): S16_LE\n", __func__);
		bits_per_sample = 16;
		break;
	case SNDRV_PCM_FORMAT_S16_BE:
		dev_dbg(codec->dev, "%s(): S16_BE\n", __func__);
		bits_per_sample = 16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		dev_dbg(codec->dev, "%s(): S20_3LE\n", __func__);
		bits_per_sample = 20;
		break;
	case SNDRV_PCM_FORMAT_S20_3BE:
		dev_dbg(codec->dev, "%s(): S20_3BE\n", __func__);
		bits_per_sample = 20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		dev_dbg(codec->dev, "%s(): S24_LE\n", __func__);
		bits_per_sample = 24;
		break;
	case SNDRV_PCM_FORMAT_S24_BE:
		dev_dbg(codec->dev, "%s(): S24_BE\n", __func__);
		bits_per_sample = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		dev_dbg(codec->dev, "%s(): S32_LE\n", __func__);
		bits_per_sample = 32;
		break;
	case SNDRV_PCM_FORMAT_S32_BE:
		dev_dbg(codec->dev, "%s(): S32_BE\n", __func__);
		bits_per_sample = 32;
		break;
	default:
		dev_dbg(codec->dev, "%s(): unknown format\n", __func__);
		return -EINVAL;
	}
	if (rc) {
		dev_dbg(codec->dev, "%s(): snd_soc_update_bits() failed\n",
			__func__);
		return rc;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s(): PLAYBACK\n", __func__);
	else
		dev_dbg(codec->dev, "%s(): CAPTURE\n", __func__);

	return rc;
}

static int es325_i2s_hw_free(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s(): PLAYBACK\n", __func__);
	else
		dev_dbg(codec->dev, "%s(): CAPTURE\n", __func__);

	return rc;
}

static int es325_i2s_prepare(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);
	return rc;
}

static int es325_i2s_trigger(struct snd_pcm_substream *substream,
			     int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);
	return rc;
}

struct snd_soc_dai_ops es325_i2s_port_dai_ops = {
	.set_sysclk	= es325_i2s_set_sysclk,
	.set_pll	= es325_i2s_set_pll,
	.set_clkdiv	= es325_i2s_set_clkdiv,
	.set_fmt	= es325_i2s_set_dai_fmt,
	.set_tdm_slot	= es325_i2s_set_tdm_slot,
	.set_channel_map	= es325_i2s_set_channel_map,
	.set_tristate	= es325_i2s_set_tristate,
	.digital_mute	= es325_i2s_port_mute,
	.startup	= es325_i2s_startup,
	.shutdown	= es325_i2s_shutdown,
	.hw_params	= es325_i2s_hw_params,
	.hw_free	= es325_i2s_hw_free,
	.prepare	= es325_i2s_prepare,
	.trigger	= es325_i2s_trigger,
};

MODULE_DESCRIPTION("ASoC ES325 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");

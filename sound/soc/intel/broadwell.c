/*
 * Intel Broadwell Wildcatpoint SST Audio
 *
 * Copyright (C) 2013, Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "sst-dsp.h"
#include "sst-haswell-ipc.h"

#include "../codecs/rt286.h"

static const struct snd_soc_dapm_widget broadwell_widgets[] = {
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_MIC("Mic", NULL),
};

static const struct snd_soc_dapm_route broadwell_rt286_map[] = {

	{"Headphones", NULL, "SPOR"},
	{"Headphones", NULL, "SPOL"},
	{"MIC1", NULL, "Mic"},

	/* CODEC BE connections */
	{"SSP0 CODEC IN", NULL, "AIF1 Capture"},
	{"AIF1 Playback", NULL, "SSP0 CODEC OUT"},
};

static int broadwell_ssp0_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	/* The ADSP will covert the FE rate to 48k, stereo */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	/* set SSP0 to 16 bit */
	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
				    SNDRV_PCM_HW_PARAM_FIRST_MASK],
				    SNDRV_PCM_FORMAT_S16_LE);
	return 0;
}

static int broadwell_rt286_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set codec DAI configuration\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, RT286_SCLK_S_MCLK, 24000000,
		SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(rtd->dev, "can't set codec sysclk configuration\n");
		return ret;
	}

	return ret;
}

static struct snd_soc_ops broadwell_rt286_ops = {
	.hw_params = broadwell_rt286_hw_params,
};

static int broadwell_compr_set_params(struct snd_compr_stream *compr)
{
	return 0;
}

static struct snd_soc_compr_ops broadwell_compr_ops = {
	.set_params = broadwell_compr_set_params,
};

static int broadwell_rtd_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct sst_pdata *pdata = dev_get_platdata(rtd->platform->dev);
	struct sst_hsw *broadwell = pdata->dsp;
	int ret;

	/* Set ADSP SSP port settings */
	ret = sst_hsw_device_set_config(broadwell, SST_HSW_DEVICE_SSP_0,
		SST_HSW_DEVICE_MCLK_FREQ_24_MHZ, SST_HSW_DEVICE_CLOCK_MASTER, 9);
	if (ret < 0) {
		dev_err(rtd->dev, "failed to set device config\n");
		return ret;
	}

	/* always connected */
	snd_soc_dapm_enable_pin(dapm, "Headphones");
	snd_soc_dapm_enable_pin(dapm, "Mic");

	return 0;
}

/* broadwell digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link broadwell_rt286_dais[] = {
	/* Front End DAI links */
	{
		.name = "System",
		.stream_name = "System Playback",
		.cpu_dai_name = "System Pin",
		.platform_name = "haswell-pcm-audio",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = broadwell_rtd_init,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
	},
	{
		.name = "Offload0",
		.stream_name = "Offload0 Playback",
		.cpu_dai_name = "Offload0 Pin",
		.platform_name = "haswell-pcm-audio",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.compr_ops = &broadwell_compr_ops,
	},
	{
		.name = "Offload1",
		.stream_name = "Offload1 Playback",
		.cpu_dai_name = "Offload1 Pin",
		.platform_name = "haswell-pcm-audio",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.compr_ops = &broadwell_compr_ops,
	},
	{
		.name = "Loopback",
		.stream_name = "Loopback",
		.cpu_dai_name = "Loopback Pin",
		.platform_name = "haswell-pcm-audio",
		.dynamic = 0,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
	},
	{
		.name = "Capture",
		.stream_name = "Capture",
		.cpu_dai_name = "Capture Pin",
		.platform_name = "haswell-pcm-audio",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
	},

	/* Back End DAI links */
	{
		/* SSP0 - Codec */
		.name = "Codec",
		.be_id = 0,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.codec_name = "i2c-INT343A:00",
		.codec_dai_name = "rt286-aif1",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = broadwell_ssp0_fixup,
		.ops = &broadwell_rt286_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
};

/* broadwell audio machine driver for WPT + RT286S */
static struct snd_soc_card broadwell_rt286 = {
	.name = "broadwell-rt286",
	.owner = THIS_MODULE,
	.dai_link = broadwell_rt286_dais,
	.num_links = ARRAY_SIZE(broadwell_rt286_dais),
	.dapm_widgets = broadwell_widgets,
	.num_dapm_widgets = ARRAY_SIZE(broadwell_widgets),
	.dapm_routes = broadwell_rt286_map,
	.num_dapm_routes = ARRAY_SIZE(broadwell_rt286_map),
	.fully_routed = true,
};

static int broadwell_audio_probe(struct platform_device *pdev)
{
	broadwell_rt286.dev = &pdev->dev;

	return snd_soc_register_card(&broadwell_rt286);
}

static int broadwell_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&broadwell_rt286);
	return 0;
}

static struct platform_driver broadwell_audio = {
	.probe = broadwell_audio_probe,
	.remove = broadwell_audio_remove,
	.driver = {
		.name = "broadwell-audio",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(broadwell_audio)

/* Module information */
MODULE_AUTHOR("Liam Girdwood, Xingchao Wang");
MODULE_DESCRIPTION("Intel SST Audio for WPT/Broadwell");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:broadwell-audio");

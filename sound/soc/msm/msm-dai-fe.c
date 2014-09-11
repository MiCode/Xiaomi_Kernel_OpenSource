/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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


#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

static struct snd_soc_dai_ops msm_fe_dai_ops = {};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
	88200, 96000, 176400, 192000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static int multimedia_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	snd_pcm_hw_constraint_list(substream->runtime, 0,
		SNDRV_PCM_HW_PARAM_RATE,
		&constraints_sample_rates);
	return 0;
}

static int fe_dai_probe(struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_route intercon;
	if (!dai || !dai->driver) {
		pr_err("%s invalid params\n", __func__);
		return -EINVAL;
	}
	memset(&intercon, 0 , sizeof(intercon));
	if (dai->driver->playback.stream_name &&
		dai->driver->playback.aif_name) {
		dev_dbg(dai->dev, "%s add route for widget %s",
			   __func__, dai->driver->playback.stream_name);
		intercon.source = dai->driver->playback.stream_name;
		intercon.sink = dai->driver->playback.aif_name;
		dev_dbg(dai->dev, "%s src %s sink %s\n",
			   __func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(&dai->dapm, &intercon, 1);
	}
	if (dai->driver->capture.stream_name &&
	   dai->driver->capture.aif_name) {
		dev_dbg(dai->dev, "%s add route for widget %s",
			   __func__, dai->driver->capture.stream_name);
		intercon.sink = dai->driver->capture.stream_name;
		intercon.source = dai->driver->capture.aif_name;
		dev_dbg(dai->dev, "%s src %s sink %s\n",
			   __func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(&dai->dapm, &intercon, 1);
	}
	return 0;
}

static struct snd_soc_dai_ops msm_fe_Multimedia_dai_ops = {
	.startup	= multimedia_startup,
};

static const struct snd_soc_component_driver msm_fe_dai_component = {
	.name		= "msm-dai-fe",
};

static struct snd_soc_dai_driver msm_fe_dais[] = {
	{
		.playback = {
			.stream_name = "MultiMedia1 Playback",
			.aif_name = "MM_DL1",
			.rates = (SNDRV_PCM_RATE_8000_192000|
					SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	192000,
		},
		.capture = {
			.stream_name = "MultiMedia1 Capture",
			.aif_name = "MM_UL1",
			.rates = (SNDRV_PCM_RATE_8000_48000|
					SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 4,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.name = "MultiMedia1",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia2 Playback",
			.aif_name = "MM_DL2",
			.rates = (SNDRV_PCM_RATE_8000_192000|
					SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	192000,
		},
		.capture = {
			.stream_name = "MultiMedia2 Capture",
			.aif_name = "MM_UL2",
			.rates = (SNDRV_PCM_RATE_8000_48000|
					SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.name = "MultiMedia2",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "CS-VOICE Playback",
			.aif_name = "CS-VOICE_DL1",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "CS-VOICE Capture",
			.aif_name = "CS-VOICE_UL1",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "CS-VOICE",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "VoIP Playback",
			.aif_name = "VOIP_DL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_SPECIAL,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =	8000,
			.rate_max = 48000,
		},
		.capture = {
			.stream_name = "VoIP Capture",
			.aif_name = "VOIP_UL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_SPECIAL,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =	8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "VoIP",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia3 Playback",
			.aif_name = "MM_DL3",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
					SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 6,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.name = "MultiMedia3",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia4 Playback",
			.aif_name = "MM_DL4",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
					SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "MultiMedia4 Capture",
			.aif_name = "MM_UL4",
			.rates = (SNDRV_PCM_RATE_8000_48000|
					SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.compress_dai = 1,
		.name = "MultiMedia4",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia5 Playback",
			.aif_name = "MM_DL5",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
					SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "MultiMedia5 Capture",
			.aif_name = "MM_UL5",
			.rates = (SNDRV_PCM_RATE_8000_48000|
					SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.name = "MultiMedia5",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia6 Playback",
			.aif_name = "MM_DL6",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
					SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "MultiMedia6 Capture",
			.aif_name = "MM_UL6",
			.rates = (SNDRV_PCM_RATE_8000_48000|
					SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.name = "MultiMedia6",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia7 Playback",
			.aif_name = "MM_DL7",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
					SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.compress_dai = 1,
		.name = "MultiMedia7",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia8 Playback",
			.aif_name = "MM_DL8",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
					SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "MultiMedia8 Capture",
			.aif_name = "MM_UL8",
			.rates = (SNDRV_PCM_RATE_8000_48000|
					SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.name = "MultiMedia8",
		.probe = fe_dai_probe,
	},
	/* FE DAIs created for hostless operation purpose */
	{
		.playback = {
			.stream_name = "SLIMBUS0_HOSTLESS Playback",
			.aif_name = "SLIM0_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "SLIMBUS0_HOSTLESS Capture",
			.aif_name = "SLIM0_UL_HL",
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "SLIMBUS0_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "SLIMBUS1_HOSTLESS Playback",
			.aif_name = "SLIM1_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "SLIMBUS1_HOSTLESS Capture",
			.aif_name = "SLIM1_UL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "SLIMBUS1_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "SLIMBUS3_HOSTLESS Playback",
			.aif_name = "SLIM3_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "SLIMBUS3_HOSTLESS Capture",
			.aif_name = "SLIM3_UL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "SLIMBUS3_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "SLIMBUS4_HOSTLESS Playback",
			.aif_name = "SLIM4_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     192000,
		},
		.capture = {
			.stream_name = "SLIMBUS4_HOSTLESS Capture",
			.aif_name = "SLIM4_UL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "SLIMBUS4_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "INT_FM_HOSTLESS Playback",
			.aif_name = "INTFM_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "INT_FM_HOSTLESS Capture",
			.aif_name = "INTFM_UL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "INT_FM_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "INT_HFP_BT Hostless Playback",
			.aif_name = "INTHFP_DL_HL",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     16000,
		},
		.capture = {
			.stream_name = "INT_HFP_BT Hostless Capture",
			.aif_name = "INTHFP_UL_HL",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "INT_HFP_BT_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "AFE Playback",
			.aif_name = "PCM_RX",
			.rates = (SNDRV_PCM_RATE_8000 |
				SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "AFE Capture",
			.aif_name = "PCM_TX",
			.rates = (SNDRV_PCM_RATE_8000 |
				SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "AFE-PROXY",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "HDMI_HOSTLESS Playback",
			.aif_name = "HDMI_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "HDMI_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "AUXPCM_HOSTLESS Playback",
			.aif_name = "AUXPCM_DL_HL",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min =     8000,
			.rate_max =     16000,
		},
		.capture = {
			.stream_name = "AUXPCM_HOSTLESS Capture",
			.aif_name = "AUXPCM_UL_HL",
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min =     8000,
			.rate_max =    16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "AUXPCM_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "VOICE_STUB Playback",
			.aif_name = "VOICE_STUB_DL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.capture = {
			.stream_name = "VOICE_STUB Capture",
			.aif_name = "VOICE_STUB_UL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "VOICE_STUB",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "VoLTE Playback",
			.aif_name = "VoLTE_DL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "VoLTE Capture",
			.aif_name = "VoLTE_UL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "VoLTE",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MI2S_RX_HOSTLESS Playback",
			.aif_name = "MI2S_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.capture = {
			.stream_name = "MI2S_TX_HOSTLESS Capture",
			.aif_name = "MI2S_UL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "MI2S_TX_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "SEC_I2S_RX_HOSTLESS Playback",
			.aif_name = "SEC_I2S_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =    48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "SEC_I2S_RX_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Primary MI2S_TX Hostless Capture",
			.aif_name = "PRI_MI2S_UL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "PRI_MI2S_TX_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "Primary MI2S_RX Hostless Playback",
			.aif_name = "PRI_MI2S_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =    48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "PRI_MI2S_RX_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Tertiary MI2S_TX Hostless Capture",
			.aif_name = "TERT_MI2S_UL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "TERT_MI2S_TX_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "Secondary MI2S_RX Hostless Playback",
			.aif_name = "SEC_MI2S_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =	8000,
			.rate_max =    48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "SEC_MI2S_RX_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Quaternary MI2S_TX Hostless Capture",
			.aif_name = "QUAT_MI2S_UL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "QUAT_MI2S_TX_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "Voice2 Playback",
			.aif_name = "VOICE2_DL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.capture = {
			.stream_name = "Voice2 Capture",
			.aif_name = "VOICE2_UL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =     8000,
			.rate_max =     48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "Voice2",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "Pseudo Playback",
			.aif_name = "MM_DL9",
			.rates = (SNDRV_PCM_RATE_8000_48000 |
					SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 48000,
		},
		.capture = {
			.stream_name = "Pseudo Capture",
			.aif_name = "MM_UL9",
			.rates = (SNDRV_PCM_RATE_8000_48000|
					SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.name = "Pseudo",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "DTMF_RX_HOSTLESS Playback",
			.aif_name = "DTMF_DL_HL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min =	8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "DTMF_RX_HOSTLESS",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "CPE Listen Audio capture",
			.aif_name = "CPE_LSM_UL_HL",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 16000,
			.rate_max = 16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "CPE_LSM_NOHOST",
	},
	{
		.playback = {
			.stream_name = "VOLTE_STUB Playback",
			.aif_name = "VOLTE_STUB_DL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.capture = {
			.stream_name = "VOLTE_STUB Capture",
			.aif_name = "VOLTE_STUB_UL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "VOLTE_STUB",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "VOICE2_STUB Playback",
			.aif_name = "VOICE2_STUB_DL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.capture = {
			.stream_name = "VOICE2_STUB Capture",
			.aif_name = "VOICE2_STUB_UL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "VOICE2_STUB",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia9 Playback",
			.aif_name = "MM_DL9",
			.rates = (SNDRV_PCM_RATE_8000_192000|
				  SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	192000,
		},
		.capture = {
			.stream_name = "MultiMedia9 Capture",
			.aif_name = "MM_UL9",
			.rates = (SNDRV_PCM_RATE_8000_48000|
				  SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =     8000,
			.rate_max =	48000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.name = "MultiMedia9",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "QCHAT Playback",
			.aif_name = "QCHAT_DL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.capture = {
			.stream_name = "QCHAT Capture",
			.aif_name = "QCHAT_UL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "QCHAT",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Listen 1 Audio Service Capture",
			.aif_name = "LSM1_UL_HL",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 16000,
			.rate_max = 16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "LSM1",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Listen 2 Audio Service Capture",
			.aif_name = "LSM2_UL_HL",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 16000,
			.rate_max = 16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "LSM2",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Listen 3 Audio Service Capture",
			.aif_name = "LSM3_UL_HL",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 16000,
			.rate_max = 16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "LSM3",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Listen 4 Audio Service Capture",
			.aif_name = "LSM4_UL_HL",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 16000,
			.rate_max = 16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "LSM4",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Listen 5 Audio Service Capture",
			.aif_name = "LSM5_UL_HL",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 16000,
			.rate_max = 16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "LSM5",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Listen 6 Audio Service Capture",
			.aif_name = "LSM6_UL_HL",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 16000,
			.rate_max = 16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "LSM6",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Listen 7 Audio Service Capture",
			.aif_name = "LSM7_UL_HL",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 16000,
			.rate_max = 16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "LSM7",
		.probe = fe_dai_probe,
	},
	{
		.capture = {
			.stream_name = "Listen 8 Audio Service Capture",
			.aif_name = "LSM8_UL_HL",
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 16000,
			.rate_max = 16000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "LSM8",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "VoWLAN Playback",
			.aif_name = "VoWLAN_DL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.capture = {
			.stream_name = "VoWLAN Capture",
			.aif_name = "VoWLAN_UL",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_fe_dai_ops,
		.name = "VoWLAN",
		.probe = fe_dai_probe,
	},
	/* FE DAIs created for multiple instances of offload playback */
	{
		.playback = {
			.stream_name = "MultiMedia10 Playback",
			.aif_name = "MM_DL10",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
				  SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.compress_dai = 1,
		.name = "MultiMedia10",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia11 Playback",
			.aif_name = "MM_DL11",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
				  SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.compress_dai = 1,
		.name = "MultiMedia11",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia12 Playback",
			.aif_name = "MM_DL12",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
				  SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.compress_dai = 1,
		.name = "MultiMedia12",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia13 Playback",
			.aif_name = "MM_DL13",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
				  SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.compress_dai = 1,
		.name = "MultiMedia13",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia14 Playback",
			.aif_name = "MM_DL14",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
				  SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.compress_dai = 1,
		.name = "MultiMedia14",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia15 Playback",
			.aif_name = "MM_DL15",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
				  SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.compress_dai = 1,
		.name = "MultiMedia15",
		.probe = fe_dai_probe,
	},
	{
		.playback = {
			.stream_name = "MultiMedia16 Playback",
			.aif_name = "MM_DL16",
			.rates = (SNDRV_PCM_RATE_8000_192000 |
				  SNDRV_PCM_RATE_KNOT),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 8,
			.rate_min =	8000,
			.rate_max = 192000,
		},
		.ops = &msm_fe_Multimedia_dai_ops,
		.compress_dai = 1,
		.name = "MultiMedia16",
		.probe = fe_dai_probe,
	},
};

static int msm_fe_dai_dev_probe(struct platform_device *pdev)
{

	dev_dbg(&pdev->dev, "%s: dev name %s\n", __func__,
		dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev, &msm_fe_dai_component,
		msm_fe_dais, ARRAY_SIZE(msm_fe_dais));
}

static int msm_fe_dai_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_dai_fe_dt_match[] = {
	{.compatible = "qcom,msm-dai-fe"},
	{}
};

static struct platform_driver msm_fe_dai_driver = {
	.probe  = msm_fe_dai_dev_probe,
	.remove = msm_fe_dai_dev_remove,
	.driver = {
		.name = "msm-dai-fe",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_fe_dt_match,
	},
};

static int __init msm_fe_dai_init(void)
{
	return platform_driver_register(&msm_fe_dai_driver);
}
module_init(msm_fe_dai_init);

static void __exit msm_fe_dai_exit(void)
{
	platform_driver_unregister(&msm_fe_dai_driver);
}
module_exit(msm_fe_dai_exit);

/* Module information */
MODULE_DESCRIPTION("MSM Frontend DAI driver");
MODULE_LICENSE("GPL v2");

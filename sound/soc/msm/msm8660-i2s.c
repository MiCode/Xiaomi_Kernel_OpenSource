/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/dai.h>

static int msm_cpu_dai_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	uint32_t dma_ch = dai->id;
	int ret = 0;

	pr_debug("%s\n", __func__);
	ret = dai_open(dma_ch);
	return ret;

}

static void msm_cpu_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	uint32_t dma_ch = dai->id;

	pr_debug("%s\n", __func__);
	dai_close(dma_ch);
}

static int msm_cpu_dai_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int msm_cpu_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	uint32_t dma_ch = dai->id;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		dai_set_master_mode(dma_ch, 1); /* CPU is master */
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		dai_set_master_mode(dma_ch, 0); /* CPU is slave */
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct snd_soc_dai_ops msm_cpu_dai_ops = {
	.startup	= msm_cpu_dai_startup,
	.shutdown	= msm_cpu_dai_shutdown,
	.trigger	= msm_cpu_dai_trigger,
	.set_fmt	= msm_cpu_dai_fmt,

};


#define MSM_DAI_SPEAKER_BUILDER(link_id)			\
{								\
	.name = "msm-speaker-dai-"#link_id,			\
	.id = (link_id),					\
	.playback = {						\
		.rates = SNDRV_PCM_RATE_8000_96000,		\
		.formats = SNDRV_PCM_FMTBIT_S16_LE,		\
		.channels_min = 1,				\
		.channels_max = 2,				\
		.rate_max =	96000,				\
		.rate_min =	8000,				\
	},							\
	.ops = &msm_cpu_dai_ops,				\
}


#define MSM_DAI_MIC_BUILDER(link_id)				\
{								\
	.name = "msm-mic-dai-"#link_id,				\
	.id = (link_id),					\
	.capture = {						\
		.rates = SNDRV_PCM_RATE_8000_96000,		\
		.formats = SNDRV_PCM_FMTBIT_S16_LE,		\
		.rate_min =	8000,				\
		.rate_max =	96000,				\
		.channels_min = 1,				\
		.channels_max = 2,				\
	},							\
	.ops = &msm_cpu_dai_ops,				\
}


struct snd_soc_dai msm_cpu_dai[] = {
	MSM_DAI_SPEAKER_BUILDER(0),
	MSM_DAI_SPEAKER_BUILDER(1),
	MSM_DAI_SPEAKER_BUILDER(2),
	MSM_DAI_SPEAKER_BUILDER(3),
	MSM_DAI_SPEAKER_BUILDER(4),
	MSM_DAI_MIC_BUILDER(5),
	MSM_DAI_MIC_BUILDER(6),
	MSM_DAI_MIC_BUILDER(7),
};
EXPORT_SYMBOL_GPL(msm_cpu_dai);

static int __init msm_cpu_dai_init(void)
{
	return snd_soc_register_dais(msm_cpu_dai, ARRAY_SIZE(msm_cpu_dai));
}
module_init(msm_cpu_dai_init);

static void __exit msm_cpu_dai_exit(void)
{
	snd_soc_unregister_dais(msm_cpu_dai, ARRAY_SIZE(msm_cpu_dai));
}
module_exit(msm_cpu_dai_exit);

/* Module information */
MODULE_DESCRIPTION("MSM CPU DAI driver");
MODULE_LICENSE("GPL v2");

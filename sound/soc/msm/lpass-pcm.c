/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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


#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <mach/audio_dma_msm8k.h>
#include <sound/dai.h>
#include "lpass-pcm.h"

static const struct snd_pcm_hardware msm_pcm_hardware = {
	.info			=	SNDRV_PCM_INFO_MMAP |
					SNDRV_PCM_INFO_MMAP_VALID |
					SNDRV_PCM_INFO_INTERLEAVED |
					SNDRV_PCM_INFO_PAUSE |
					SNDRV_PCM_INFO_RESUME,
	.rates			=	SNDRV_PCM_RATE_8000_48000,
	.formats		=	SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min =	32,
	.period_bytes_max =	DMASZ/4,
	.buffer_bytes_max =	DMASZ,
	.rate_max =	96000,
	.rate_min =	8000,
	.channels_min =	USE_CHANNELS_MIN,
	.channels_max =	USE_CHANNELS_MAX,
	.periods_min =	4,
	.periods_max =	512,
	.fifo_size =	0,
};

struct msm_pcm_data {
	spinlock_t		lock;
	int			ch;
};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static int msm_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{

	pr_debug("%s\n", __func__);
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static irqreturn_t msm_pcm_irq(int intrsrc, void *data)
{
	struct snd_pcm_substream *substream = data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = (struct msm_audio *)runtime->private_data;
	int dma_ch = 0;
	unsigned int has_xrun, pending;
	int ret = IRQ_NONE;

	if (prtd)
		dma_ch = prtd->dma_ch;
	else
		return ret;

	pr_debug("msm8660-pcm: msm_pcm_irq called\n");
	pending = (intrsrc
		& (UNDER_CH(dma_ch) | PER_CH(dma_ch) | ERR_CH(dma_ch)));
	has_xrun = (pending & UNDER_CH(dma_ch));

	if (unlikely(has_xrun) &&
	    substream->runtime &&
	    snd_pcm_running(substream)) {
		pr_err("xrun\n");
		snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);
		ret = IRQ_HANDLED;
		pending &= ~UNDER_CH(dma_ch);
	}


	if (pending & PER_CH(dma_ch)) {
		ret = IRQ_HANDLED;
		if (likely(substream->runtime &&
			   snd_pcm_running(substream))) {
			/* end of buffer missed? loop back */
			if (++prtd->period_index >= runtime->periods)
				prtd->period_index = 0;
				snd_pcm_period_elapsed(substream);
			pr_debug("period elapsed\n");
		}
		pending &= ~PER_CH(dma_ch);
	}

	if (unlikely(pending
		& (UNDER_CH(dma_ch) & PER_CH(dma_ch) & ERR_CH(dma_ch)))) {
		if (pending & UNDER_CH(dma_ch))
			pr_err("msm8660-pcm: DMA %x Underflow\n",
			       dma_ch);
		if (pending & ERR_CH(dma_ch))
			pr_err("msm8660-pcm: DMA %x Master Error\n",
			       dma_ch);

	}
	return ret;
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = (struct msm_audio *)runtime->private_data;
	struct dai_dma_params dma_params;
	int dma_ch = 0;

	if (prtd)
		dma_ch = prtd->dma_ch;
	else
		return 0;

	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	pr_debug("%s:prtd->pcm_size = %d\n", __func__, prtd->pcm_size);
	pr_debug("%s:prtd->pcm_count = %d\n", __func__, prtd->pcm_count);

	if (prtd->enabled)
		return 0;

	dma_params.src_start = runtime->dma_addr;
	dma_params.buffer = (u8 *)runtime->dma_area;
	dma_params.buffer_size = prtd->pcm_size;
	dma_params.period_size = prtd->pcm_count;
	dma_params.channels = runtime->channels;

	dai_set_params(dma_ch, &dma_params);
	register_dma_irq_handler(dma_ch, msm_pcm_irq, (void *)substream);

	prtd->enabled = 1;
	return 0;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = (struct msm_audio *)runtime->private_data;
	int ret = 0;

	pr_debug("%s\n", __func__);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dai_start(prtd->dma_ch);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dai_stop(prtd->dma_ch);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static snd_pcm_uframes_t msm_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = (struct msm_audio *)runtime->private_data;
	snd_pcm_uframes_t offset = 0;

	pr_debug("%s: period_index =%d\n", __func__, prtd->period_index);
	offset = prtd->period_index * runtime->period_size;
	if (offset >= runtime->buffer_size)
		offset = 0;
	return offset;
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *machine = rtd->dai;
	struct snd_soc_dai *cpu_dai = machine->cpu_dai;
	struct msm_audio *prtd = NULL;
	int ret = 0;

	pr_debug("%s\n", __func__);
	snd_soc_set_runtime_hwparams(substream, &msm_pcm_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
				SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0) {
		pr_err("Error setting hw_constraint\n");
		goto err;
	}
	ret = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE,
				&constraints_sample_rates);
	if (ret < 0)
		pr_err("Error snd_pcm_hw_constraint_list failed\n");

	prtd = kzalloc(sizeof(struct msm_audio), GFP_KERNEL);

	if (prtd == NULL) {
		pr_err("Error allocating prtd\n");
		ret = -ENOMEM;
		goto err;
	}
	prtd->dma_ch = cpu_dai->id;
	prtd->enabled = 0;
	runtime->dma_bytes = msm_pcm_hardware.buffer_bytes_max;
	runtime->private_data = prtd;
err:
	return ret;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = (struct msm_audio *)runtime->private_data;
	int dma_ch = 0;

	if (prtd)
		dma_ch = prtd->dma_ch;
	else
		return 0;

	pr_debug("%s\n", __func__);
	unregister_dma_irq_handler(dma_ch);
	kfree(runtime->private_data);
	return 0;
}

static int msm_pcm_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vms)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	pr_debug("%s: snd_msm_audio_hw_params runtime->dma_addr 0x(%x)\n",
		__func__, (unsigned int)runtime->dma_addr);
	pr_debug("%s: snd_msm_audio_hw_params runtime->dma_area 0x(%x)\n",
		__func__, (unsigned int)runtime->dma_area);
	pr_debug("%s: snd_msm_audio_hw_params runtime->dma_bytes 0x(%x)\n",
		__func__, (unsigned int)runtime->dma_bytes);

	return dma_mmap_coherent(substream->pcm->card->dev, vms,
					runtime->dma_area,
					runtime->dma_addr,
					runtime->dma_bytes);
}


static struct snd_pcm_ops msm_pcm_ops = {
	.open		= msm_pcm_open,
	.close		= msm_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= msm_pcm_hw_params,
	.prepare	= msm_pcm_prepare,
	.trigger	= msm_pcm_trigger,
	.pointer	= msm_pcm_pointer,
	.mmap		= msm_pcm_mmap,
};

static int pcm_preallocate_buffer(struct snd_pcm *pcm,
					int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = msm_pcm_hardware.buffer_bytes_max;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev, size,
					&buf->addr, GFP_KERNEL);

	if (!buf->area)
		return -ENOMEM;

	buf->bytes = size;
	return 0;
}

static void msm_pcm_free_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!stream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_coherent(pcm->card->dev, buf->bytes,
					buf->area, buf->addr);
		buf->area = NULL;
	}
}
static u64 msm_pcm_dmamask = DMA_BIT_MASK(32);

static int msm_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
			struct snd_pcm *pcm)
{
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &msm_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (dai->playback.channels_min) {
		ret = pcm_preallocate_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			return ret;
	}
	if (dai->capture.channels_min) {
		ret = pcm_preallocate_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			return ret;
	}
	return ret;
}

struct snd_soc_platform msm8660_soc_platform = {
	.name		= "msm8660-pcm-audio",
	.pcm_ops	= &msm_pcm_ops,
	.pcm_new	= msm_pcm_new,
	.pcm_free	= msm_pcm_free_buffers,
};
EXPORT_SYMBOL_GPL(msm8660_soc_platform);

static int __init msm_soc_platform_init(void)
{
	return snd_soc_register_platform(&msm8660_soc_platform);
}
static void __exit msm_soc_platform_exit(void)
{
	snd_soc_unregister_platform(&msm8660_soc_platform);
}
module_init(msm_soc_platform_init);
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("MSM PCM module");
MODULE_LICENSE("GPL v2");

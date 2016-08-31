/*
 * tegra_alt_pcm.c - Tegra PCM driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on code copyright/by:
 *
 * Copyright (c) 2009-2010, NVIDIA Corporation.
 * Scott Peterson <speterson@nvidia.com>
 * Vijay Mali <vmali@nvidia.com>
 *
 * Copyright (C) 2010 Google, Inc.
 * Iliyan Malchev <malchev@google.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

#include "tegra_pcm_alt.h"

static const struct snd_pcm_hardware tegra_alt_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S8 |
				  SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_S20_3LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.channels_min		= 1,
	.channels_max		= 2,
	.period_bytes_min	= 128,
	.period_bytes_max	= PAGE_SIZE * 2,
	.periods_min		= 1,
	.periods_max		= 8,
	.buffer_bytes_max	= PAGE_SIZE * 8,
	.fifo_size		= 4,
};

static int tegra_alt_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct tegra_alt_pcm_runtime_data *prtd;
	int ret;

	prtd = kzalloc(sizeof(struct tegra_alt_pcm_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	/* Set HW params now that initialization is complete */
	snd_soc_set_runtime_hwparams(substream, &tegra_alt_pcm_hardware);

	/* Ensure period size is multiple of 8 */
	ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 0x8);
	if (ret) {
		dev_err(dev, "failed to set constraint %d\n", ret);
		kfree(prtd);
		return ret;
	}

	ret = snd_dmaengine_pcm_open_request_chan(substream, NULL, NULL);
	if (ret) {
		dev_err(dev, "dmaengine pcm open failed with err %d\n", ret);
		kfree(prtd);
		return ret;
	}

	return 0;
}

static int tegra_alt_pcm_close(struct snd_pcm_substream *substream)
{
	snd_dmaengine_pcm_close_release_chan(substream);
	return 0;
}

int tegra_alt_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct tegra_alt_pcm_dma_params *dmap;
	struct dma_slave_config slave_config;
	int ret;

	dmap = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	ret = snd_hwparams_to_dma_slave_config(substream, params,
						&slave_config);
	if (ret) {
		dev_err(dev, "hw params config failed with err %d\n", ret);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.dst_addr = dmap->addr;
		slave_config.dst_maxburst = 4;
	} else {
		slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.src_addr = dmap->addr;
		slave_config.src_maxburst = 4;
	}
	slave_config.slave_id = dmap->req_sel;

	ret = dmaengine_slave_config(chan, &slave_config);
	if (ret < 0) {
		dev_err(dev, "dma slave config failed with err %d\n", ret);
		return ret;
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

int tegra_alt_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int tegra_alt_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					runtime->dma_area,
					runtime->dma_addr,
					runtime->dma_bytes);
}

static struct snd_pcm_ops tegra_alt_pcm_ops = {
	.open		= tegra_alt_pcm_open,
	.close		= tegra_alt_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= tegra_alt_pcm_hw_params,
	.hw_free	= tegra_alt_pcm_hw_free,
	.trigger	= snd_dmaengine_pcm_trigger,
	.pointer	= snd_dmaengine_pcm_pointer,
	.mmap		= tegra_alt_pcm_mmap,
};

static int tegra_alt_pcm_preallocate_dma_buffer(struct snd_pcm *pcm,
				int stream , size_t size)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
						&buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->private_data = NULL;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->bytes = size;

	return 0;
}

void tegra_alt_pcm_deallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;

	substream = pcm->streams[stream].substream;
	if (!substream)
		return;

	buf = &substream->dma_buffer;
	if (!buf->area)
		return;

	dma_free_writecombine(pcm->card->dev, buf->bytes,
				buf->area, buf->addr);
	buf->area = NULL;
}

static u64 tegra_dma_mask = DMA_BIT_MASK(32);

int tegra_alt_pcm_dma_allocate(struct snd_soc_pcm_runtime *rtd, size_t size)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &tegra_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = tegra_alt_pcm_preallocate_dma_buffer(pcm,
						SNDRV_PCM_STREAM_PLAYBACK,
						size);
		if (ret)
			goto err;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = tegra_alt_pcm_preallocate_dma_buffer(pcm,
						SNDRV_PCM_STREAM_CAPTURE,
						size);
		if (ret)
			goto err_free_play;
	}

	return 0;

err_free_play:
	tegra_alt_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
err:
	return ret;
}

int tegra_alt_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	return tegra_alt_pcm_dma_allocate(rtd,
				tegra_alt_pcm_hardware.buffer_bytes_max);
}

void tegra_alt_pcm_free(struct snd_pcm *pcm)
{
	tegra_alt_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
	tegra_alt_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
}

static int tegra_alt_pcm_probe(struct snd_soc_platform *platform)
{
	platform->dapm.idle_bias_off = 1;
	return 0;
}

static struct snd_soc_platform_driver tegra_alt_pcm_platform = {
	.ops		= &tegra_alt_pcm_ops,
	.pcm_new	= tegra_alt_pcm_new,
	.pcm_free	= tegra_alt_pcm_free,
	.probe		= tegra_alt_pcm_probe,
};

int tegra_alt_pcm_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &tegra_alt_pcm_platform);
}
EXPORT_SYMBOL_GPL(tegra_alt_pcm_platform_register);

void tegra_alt_pcm_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(tegra_alt_pcm_platform_unregister);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra Alt PCM ASoC driver");
MODULE_LICENSE("GPL");

/*
 * tegra_pcm.c - Tegra PCM driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on code copyright/by:
 *
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 * Scott Peterson <speterson@nvidia.com>
 * Vijay Mali <vmali@nvidia.com>
 * Manoj Gangwal <mgangwal@nvidia.com>
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

#include <asm/mach-types.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

#include "tegra_pcm.h"

static const struct snd_pcm_hardware tegra_pcm_hardware = {
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

static int tegra_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct tegra_runtime_data *prtd;
	int ret;

	prtd = kzalloc(sizeof(struct tegra_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	/* Set HW params now that initialization is complete */
	snd_soc_set_runtime_hwparams(substream, &tegra_pcm_hardware);

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

	snd_dmaengine_pcm_set_data(substream, prtd);

	return 0;
}

static int tegra_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_runtime_data *tegra_prtd;

	if (rtd->dai_link->no_pcm)
		return 0;

	tegra_prtd =
		(struct tegra_runtime_data *)snd_dmaengine_pcm_get_data(
						substream);
	kfree(tegra_prtd);

	snd_dmaengine_pcm_set_data(substream, NULL);
	snd_dmaengine_pcm_close_release_chan(substream);

	return 0;
}

int tegra_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct tegra_pcm_dma_params *dmap;
	struct dma_slave_config slave_config;
	int ret;

	dmap = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	if (!dmap)
		return 0;

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

int tegra_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

int tegra_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_pcm_dma_params * dmap;
	struct tegra_runtime_data *prtd;

	dmap = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);


	prtd = (struct tegra_runtime_data *)
	snd_dmaengine_pcm_get_data(substream);

	if (!dmap)
		return 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		prtd->running = 1;
		if (prtd->disable_intr) {
			substream->runtime->dma_addr = prtd->avp_dma_addr;
			substream->runtime->no_period_wakeup = 1;
		} else {
			substream->runtime->no_period_wakeup = 0;
		}

		return snd_dmaengine_pcm_trigger(substream,
					SNDRV_PCM_TRIGGER_START);

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		prtd->running = 0;

		return snd_dmaengine_pcm_trigger(substream,
					SNDRV_PCM_TRIGGER_STOP);
	default:
		return -EINVAL;
	}
	return 0;
}

static int tegra_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					runtime->dma_area,
					runtime->dma_addr,
					runtime->dma_bytes);
}

static struct snd_pcm_ops tegra_pcm_ops = {
	.open		= tegra_pcm_open,
	.close		= tegra_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= tegra_pcm_hw_params,
	.hw_free	= tegra_pcm_hw_free,
	.trigger	= tegra_pcm_trigger,
	.pointer	= snd_dmaengine_pcm_pointer,
	.mmap		= tegra_pcm_mmap,
};

static int tegra_pcm_preallocate_dma_buffer(struct snd_pcm *pcm,
				int stream , size_t size)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
#if TEGRA30_USE_SMMU
	unsigned char *vaddr;
	phys_addr_t paddr;
	struct tegra_smmu_data *ptsd;

	ptsd = kzalloc(sizeof(struct tegra_smmu_data), GFP_KERNEL);
	ptsd->pcm_nvmap_client = nvmap_create_client(nvmap_dev, "Audio_SMMU");
	ptsd->pcm_nvmap_handle = nvmap_alloc(ptsd->pcm_nvmap_client,
				 size,
				 32,
				 NVMAP_HANDLE_WRITE_COMBINE,
				 NVMAP_HEAP_IOVMM);

	vaddr = (unsigned char *) nvmap_mmap(ptsd->pcm_nvmap_handle);
	paddr = nvmap_pin(ptsd->pcm_nvmap_client, ptsd->pcm_nvmap_handle);
	buf->area = vaddr;
	buf->addr = paddr;
	buf->private_data = ptsd;
#else
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
						&buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->private_data = NULL;
#endif
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->bytes = size;

	return 0;
}

void tegra_pcm_deallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
#if TEGRA30_USE_SMMU
	struct tegra_smmu_data *ptsd;
#endif

	substream = pcm->streams[stream].substream;
	if (!substream)
		return;

	buf = &substream->dma_buffer;
	if (!buf->area)
		return;

#if TEGRA30_USE_SMMU
	if (!buf->private_data)
		return;
	ptsd = (struct tegra_smmu_data *)buf->private_data;
	nvmap_unpin(ptsd->pcm_nvmap_client, ptsd->pcm_nvmap_handle);
	nvmap_munmap(ptsd->pcm_nvmap_handle, buf->area);
	nvmap_free(ptsd->pcm_nvmap_client, ptsd->pcm_nvmap_handle);
	kfree(ptsd);
	buf->private_data = NULL;
#else
	dma_free_writecombine(pcm->card->dev, buf->bytes,
				buf->area, buf->addr);
#endif
	buf->area = NULL;
}

static u64 tegra_dma_mask = DMA_BIT_MASK(32);

int tegra_pcm_dma_allocate(struct snd_soc_pcm_runtime *rtd, size_t size)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &tegra_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = tegra_pcm_preallocate_dma_buffer(pcm,
						SNDRV_PCM_STREAM_PLAYBACK,
						size);
		if (ret)
			goto err;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = tegra_pcm_preallocate_dma_buffer(pcm,
						SNDRV_PCM_STREAM_CAPTURE,
						size);
		if (ret)
			goto err_free_play;
	}

	return 0;

err_free_play:
	tegra_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
err:
	return ret;
}

int tegra_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	return tegra_pcm_dma_allocate(rtd ,
					tegra_pcm_hardware.buffer_bytes_max);
}

void tegra_pcm_free(struct snd_pcm *pcm)
{
	tegra_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
	tegra_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
}

static int tegra_pcm_probe(struct snd_soc_platform *platform)
{
	platform->dapm.idle_bias_off = 1;
	return 0;
}

static struct snd_soc_platform_driver tegra_pcm_platform = {
	.ops		= &tegra_pcm_ops,
	.pcm_new	= tegra_pcm_new,
	.pcm_free	= tegra_pcm_free,
	.probe		= tegra_pcm_probe,
};

int tegra_pcm_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &tegra_pcm_platform);
}
EXPORT_SYMBOL_GPL(tegra_pcm_platform_register);

void tegra_pcm_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(tegra_pcm_platform_unregister);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra PCM ASoC driver");
MODULE_LICENSE("GPL");

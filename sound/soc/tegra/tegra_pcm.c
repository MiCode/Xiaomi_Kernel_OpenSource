/*
 * tegra_pcm.c - Tegra PCM driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (C) 2010-2012 - NVIDIA, Inc.
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

#include <asm/mach-types.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra_pcm.h"

#define DRV_NAME "tegra-pcm-audio"

static const struct snd_pcm_hardware tegra_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S8 |
				  SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
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

static void tegra_pcm_queue_dma(struct tegra_runtime_data *prtd)
{
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct tegra_dma_req *dma_req;
	unsigned long addr;

	dma_req = &prtd->dma_req[prtd->dma_req_idx];
	if (++prtd->dma_req_idx >= prtd->dma_req_count)
		prtd->dma_req_idx -= prtd->dma_req_count;

	if (prtd->avp_dma_addr)
		addr = prtd->avp_dma_addr + prtd->dma_pos;
	else
		addr = buf->addr + prtd->dma_pos;

	prtd->dma_pos += dma_req->size;
	if (prtd->dma_pos >= prtd->dma_pos_end)
		prtd->dma_pos = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_req->source_addr = addr;
	else
		dma_req->dest_addr = addr;

	tegra_dma_enqueue_req(prtd->dma_chan, dma_req);
}

static void dma_complete_callback(struct tegra_dma_req *req)
{
	struct tegra_runtime_data *prtd = (struct tegra_runtime_data *)req->dev;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;

	spin_lock(&prtd->lock);

	if (!prtd->running) {
		spin_unlock(&prtd->lock);
		return;
	}

	if (++prtd->period_index >= runtime->periods)
		prtd->period_index = 0;

	tegra_pcm_queue_dma(prtd);

	spin_unlock(&prtd->lock);

	snd_pcm_period_elapsed(substream);
}

static void setup_dma_tx_request(struct tegra_dma_req *req,
					struct tegra_pcm_dma_params * dmap)
{
	req->complete = dma_complete_callback;
	req->to_memory = false;
	req->dest_addr = dmap->addr;
	req->dest_wrap = dmap->wrap;
	req->source_bus_width = 32;
	req->source_wrap = 0;
	req->dest_bus_width = dmap->width;
	req->req_sel = dmap->req_sel;
	req->use_smmu = false;
#if TEGRA30_USE_SMMU
	req->use_smmu = true;
#endif
}

static void setup_dma_rx_request(struct tegra_dma_req *req,
					struct tegra_pcm_dma_params * dmap)
{
	req->complete = dma_complete_callback;
	req->to_memory = true;
	req->source_addr = dmap->addr;
	req->dest_wrap = 0;
	req->source_bus_width = dmap->width;
	req->source_wrap = dmap->wrap;
	req->dest_bus_width = 32;
	req->req_sel = dmap->req_sel;
	req->use_smmu = false;
#if TEGRA30_USE_SMMU
	req->use_smmu = true;
#endif
}

int tegra_pcm_allocate(struct snd_pcm_substream *substream,
					int dma_mode,
					const struct snd_pcm_hardware *pcm_hardware)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_pcm_dma_params * dmap;
	int ret = 0;
	int i = 0;

	prtd = kzalloc(sizeof(struct tegra_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	runtime->private_data = prtd;
	prtd->substream = substream;

	spin_lock_init(&prtd->lock);

	dmap = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	prtd->dma_req_count = MAX_DMA_REQ_COUNT;

	if (dmap) {
		for (i = 0; i < prtd->dma_req_count; i++)
			prtd->dma_req[i].dev = prtd;

		prtd->dma_chan = tegra_dma_allocate_channel(
					dma_mode,
					"pcm");
		if (prtd->dma_chan == NULL) {
			ret = -ENOMEM;
			goto err;
		}
	}

	/* Set HW params now that initialization is complete */
	snd_soc_set_runtime_hwparams(substream, pcm_hardware);

	/* Ensure period size is multiple of 8 */
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 0x8);
	if (ret < 0)
		goto err;

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto err;

	return 0;

err:
	if (prtd->dma_chan) {
		tegra_dma_free_channel(prtd->dma_chan);
	}

	kfree(prtd);

	return ret;
}

static int tegra_pcm_open(struct snd_pcm_substream *substream)
{
	return tegra_pcm_allocate(substream,
					TEGRA_DMA_MODE_CONTINUOUS_SINGLE,
					&tegra_pcm_hardware);

}

int tegra_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd = runtime->private_data;

	if (prtd->dma_chan)
		tegra_dma_free_channel(prtd->dma_chan);

	kfree(prtd);

	return 0;
}

int tegra_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_pcm_dma_params * dmap;
	int i;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	/* Limit dma_req_count to period count */
	if (prtd->dma_req_count > params_periods(params))
		prtd->dma_req_count = params_periods(params);
	dmap = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	if (dmap) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			for (i = 0; i < prtd->dma_req_count; i++)
				setup_dma_tx_request(&prtd->dma_req[i], dmap);
		} else {
			for (i = 0; i < prtd->dma_req_count; i++)
				setup_dma_rx_request(&prtd->dma_req[i], dmap);
		}
	}
	for (i = 0; i < prtd->dma_req_count; i++)
		prtd->dma_req[i].size = params_period_bytes(params);

	return 0;
}

int tegra_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

int tegra_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_pcm_dma_params * dmap;
	unsigned long flags;
	int i;

	dmap = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	if (!dmap)
		return 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->dma_pos = 0;
		prtd->dma_pos_end = frames_to_bytes(runtime, runtime->periods * runtime->period_size);
		prtd->period_index = 0;
		prtd->dma_req_idx = 0;
		if (prtd->disable_intr) {
			prtd->dma_req_count = 1;
			prtd->dma_req[0].complete = NULL;
		} else if (!prtd->dma_req[0].complete) {
			prtd->dma_req[0].complete = dma_complete_callback;
			prtd->dma_req_count =
				(MAX_DMA_REQ_COUNT <= runtime->periods) ?
				MAX_DMA_REQ_COUNT : runtime->periods;
		}
		/* Fall-through */
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		spin_lock_irqsave(&prtd->lock, flags);
		prtd->running = 1;
		spin_unlock_irqrestore(&prtd->lock, flags);
		for (i = 0; i < prtd->dma_req_count; i++)
			tegra_pcm_queue_dma(prtd);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spin_lock_irqsave(&prtd->lock, flags);
		prtd->running = 0;
		spin_unlock_irqrestore(&prtd->lock, flags);
		tegra_dma_cancel(prtd->dma_chan);
		for (i = 0; i < prtd->dma_req_count; i++) {
			if (prtd->dma_req[i].complete &&
				(prtd->dma_req[i].status ==
				 -TEGRA_DMA_REQ_ERROR_ABORTED))
				prtd->dma_req[i].complete(&prtd->dma_req[i]);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

snd_pcm_uframes_t tegra_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *prtd = runtime->private_data;
	int dma_transfer_count;

	dma_transfer_count = tegra_dma_get_transfer_count(prtd->dma_chan,
					&prtd->dma_req[prtd->dma_req_idx]);

	return prtd->period_index * runtime->period_size +
		bytes_to_frames(runtime, dma_transfer_count);
}

int tegra_pcm_mmap(struct snd_pcm_substream *substream,
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
	.pointer	= tegra_pcm_pointer,
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

static int __devinit tegra_pcm_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &tegra_pcm_platform);
}

static int __devexit tegra_pcm_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver tegra_pcm_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tegra_pcm_platform_probe,
	.remove = __devexit_p(tegra_pcm_platform_remove),
};
module_platform_driver(tegra_pcm_driver);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra PCM ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);

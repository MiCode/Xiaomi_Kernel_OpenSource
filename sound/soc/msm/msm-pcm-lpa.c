/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>
#include <linux/android_pmem.h>

#include "msm-pcm-q6.h"
#include "msm-pcm-routing.h"

static struct audio_locks the_locks;

struct snd_msm {
	struct snd_card *card;
	struct snd_pcm *pcm;
};

static struct snd_pcm_hardware msm_pcm_hardware = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =              SNDRV_PCM_FMTBIT_S16_LE,
	.rates =                SNDRV_PCM_RATE_8000_48000,
	.rate_min =             8000,
	.rate_max =             48000,
	.channels_min =         1,
	.channels_max =         2,
	.buffer_bytes_max =     2 * 1024 * 1024,
/* TODO: Check on the lowest period size we can support */
	.period_bytes_min =	128 * 1024,
	.period_bytes_max =     512 * 1024,
	.periods_min =          4,
	.periods_max =          16,
	.fifo_size =            0,
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

static void event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	struct msm_audio *prtd = priv;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_aio_write_param param;
	struct audio_buffer *buf = prtd->audio_client->port[IN].buf;
	int i = 0;

	pr_debug("%s\n", __func__);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE: {
		uint32_t *ptrmem = (uint32_t *)&param;
		pr_debug("ASM_DATA_EVENT_WRITE_DONE\n");
		pr_debug("Buffer Consumed = 0x%08x\n", *ptrmem);
		prtd->pcm_irq_pos += prtd->pcm_count;
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		atomic_inc(&prtd->out_count);
		wake_up(&the_locks.write_wait);
		if (!atomic_read(&prtd->start)) {
			prtd->pending_buffer = 1;
			break;
		} else
			prtd->pending_buffer = 0;
		pr_debug("%s:writing %d bytes of buffer to dsp 2\n",
				__func__, prtd->pcm_count);

		param.paddr = (unsigned long)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count);
		param.len = prtd->pcm_count;
		param.msw_ts = 0;
		param.lsw_ts = 0;
		param.flags = NO_TIMESTAMP;
		param.uid =  (unsigned long)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count);
		for (i = 0; i < sizeof(struct audio_aio_write_param)/4;
					i++, ++ptrmem)
			pr_debug("cmd[%d]=0x%08x\n", i, *ptrmem);
		if (q6asm_async_write(prtd->audio_client,
					&param) < 0)
			pr_err("%s:q6asm_async_write failed\n",
				__func__);
		else
			prtd->out_head =
				(prtd->out_head + 1) & (runtime->periods - 1);
		break;
	}
	case ASM_DATA_CMDRSP_EOS:
		pr_debug("ASM_DATA_CMDRSP_EOS\n");
		prtd->cmd_ack = 1;
		wake_up(&the_locks.eos_wait);
		break;
	case APR_BASIC_RSP_RESULT: {
		switch (payload[0]) {
		case ASM_SESSION_CMD_RUN: {
			if (!prtd->pending_buffer)
				break;
			pr_debug("%s:writing %d bytes"
				" of buffer to dsp\n",
				__func__, prtd->pcm_count);
			param.paddr = (unsigned long)buf[prtd->out_head].data;
			param.len = prtd->pcm_count;
			param.msw_ts = 0;
			param.lsw_ts = 0;
			param.flags = NO_TIMESTAMP;
			param.uid =  (unsigned long)buf[prtd->out_head].data;
			if (q6asm_async_write(prtd->audio_client,
						&param) < 0)
				pr_err("%s:q6asm_async_write failed\n",
					__func__);
			else
				prtd->out_head =
					(prtd->out_head + 1)
					& (runtime->periods - 1);
		}
			break;
		default:
			break;
		}
		break;
	}
	default:
		pr_debug("Not Supported Event opcode[0x%x]\n", opcode);
		break;
	}
}

static int msm_pcm_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	int ret;

	pr_debug("%s\n", __func__);
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;
	if (prtd->enabled)
		return 0;

	ret = q6asm_media_format_block_pcm(prtd->audio_client, runtime->rate,
				runtime->channels);
	if (ret < 0)
		pr_debug("%s: CMD Format block failed\n", __func__);

	atomic_set(&prtd->out_count, runtime->periods);
	prtd->enabled = 1;
	prtd->cmd_ack = 0;
	return 0;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	pr_debug("%s\n", __func__);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("SNDRV_PCM_TRIGGER_START\n");
		q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
		atomic_set(&prtd->start, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("SNDRV_PCM_TRIGGER_STOP\n");
		atomic_set(&prtd->start, 0);
		if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK)
			break;
		prtd->cmd_ack = 0;
		q6asm_cmd_nowait(prtd->audio_client, CMD_EOS);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("SNDRV_PCM_TRIGGER_PAUSE\n");
		q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
		atomic_set(&prtd->start, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd;
	int ret = 0;

	pr_debug("%s\n", __func__);
	prtd = kzalloc(sizeof(struct msm_audio), GFP_KERNEL);
	if (prtd == NULL) {
		pr_err("Failed to allocate memory for msm_audio\n");
		return -ENOMEM;
	}
	runtime->hw = msm_pcm_hardware;
	prtd->substream = substream;
	prtd->audio_client = q6asm_audio_client_alloc(
				(app_cb)event_handler, prtd);
	if (!prtd->audio_client) {
		pr_debug("%s: Could not allocate memory\n", __func__);
		kfree(prtd);
		return -ENOMEM;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = q6asm_open_write(prtd->audio_client, FORMAT_LINEAR_PCM);
		if (ret < 0) {
			pr_err("%s: pcm out open failed\n", __func__);
			q6asm_audio_client_free(prtd->audio_client);
			kfree(prtd);
			return -ENOMEM;
		}
		ret = q6asm_set_io_mode(prtd->audio_client, ASYNC_IO_MODE);
		if (ret < 0) {
			pr_err("%s: Set IO mode failed\n", __func__);
			q6asm_audio_client_free(prtd->audio_client);
			kfree(prtd);
			return -ENOMEM;
		}
	}
	/* Capture path */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return -EPERM;
	pr_debug("%s: session ID %d\n", __func__, prtd->audio_client->session);
	prtd->session_id = prtd->audio_client->session;
	msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->be_id,
		prtd->session_id, substream->stream);

	ret = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE,
				&constraints_sample_rates);
	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_list failed\n");
	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_integer failed\n");

	prtd->dsp_cnt = 0;
	prtd->pending_buffer = 1;
	runtime->private_data = prtd;

	return 0;
}

static int msm_pcm_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	int dir = 0;
	int ret = 0;

	pr_debug("%s\n", __func__);

	dir = IN;
	ret = wait_event_timeout(the_locks.eos_wait,
				prtd->cmd_ack, 5 * HZ);
	if (ret < 0)
		pr_err("%s: CMD_EOS failed\n", __func__);
	q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);

	pr_debug("%s\n", __func__);
	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->be_id,
		SNDRV_PCM_STREAM_PLAYBACK);
	pr_debug("%s\n", __func__);
	q6asm_cmd(prtd->audio_client, CMD_CLOSE);
	q6asm_audio_client_free(prtd->audio_client);
	kfree(prtd);

	return 0;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_close(substream);
	return ret;
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_prepare(substream);
	return ret;
}

static snd_pcm_uframes_t msm_pcm_pointer(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	if (prtd->pcm_irq_pos >= prtd->pcm_size)
		prtd->pcm_irq_pos = 0;
	pr_debug("%s: pcm_irq_pos = %d\n", __func__, prtd->pcm_irq_pos);
	return bytes_to_frames(runtime, (prtd->pcm_irq_pos));
}

static int msm_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	pr_debug("%s\n", __func__);
	prtd->mmap_flag = 1;
	dma_mmap_coherent(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
	return 0;
}

static int msm_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct audio_buffer *buf;
	int dir, ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		return -EPERM;
	ret = q6asm_audio_client_buf_alloc_contiguous(dir,
			prtd->audio_client,
			runtime->hw.period_bytes_min,
			runtime->hw.periods_max);
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed \
					rc = %d\n", ret);
		return -ENOMEM;
	}
	buf = prtd->audio_client->port[dir].buf;
	if (!buf && !buf[0].data)
		return -ENOMEM;

	pr_debug("%s:buf = %p\n", __func__, buf);
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->area = buf[0].data;
	dma_buf->addr =  buf[0].phys;
	dma_buf->bytes = runtime->hw.buffer_bytes_max;
	if (!dma_buf->area)
		return -ENOMEM;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.hw_params	= msm_pcm_hw_params,
	.close          = msm_pcm_close,
	.ioctl          = snd_pcm_lib_ioctl,
	.prepare        = msm_pcm_prepare,
	.trigger        = msm_pcm_trigger,
	.pointer        = msm_pcm_pointer,
	.mmap		= msm_pcm_mmap,
};

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	return ret;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_pcm_ops,
	.pcm_new	= msm_asoc_pcm_new,
};

static __devinit int msm_pcm_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: dev name %s\n",
			__func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
				&msm_soc_platform);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-pcm-lpa",
		.owner = THIS_MODULE,
	},
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
};

static int __init msm_soc_platform_init(void)
{
	init_waitqueue_head(&the_locks.enable_wait);
	init_waitqueue_head(&the_locks.eos_wait);
	init_waitqueue_head(&the_locks.write_wait);
	init_waitqueue_head(&the_locks.read_wait);

	return platform_driver_register(&msm_pcm_driver);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	platform_driver_unregister(&msm_pcm_driver);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("PCM module platform driver");
MODULE_LICENSE("GPL v2");

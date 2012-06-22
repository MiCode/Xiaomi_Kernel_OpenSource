/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include "msm-compr-q6-v2.h"
#include "msm-pcm-routing-v2.h"

struct snd_msm {
	struct msm_audio *prtd;
	unsigned volume;
};
static struct snd_msm compressed_audio = {NULL, 0x2000} ;

static struct audio_locks the_locks;

static struct snd_pcm_hardware msm_compr_hardware_playback = {
	.info =		 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =	      SNDRV_PCM_FMTBIT_S16_LE,
	.rates =		SNDRV_PCM_RATE_8000_48000,
	.rate_min =	     8000,
	.rate_max =	     48000,
	.channels_min =	 1,
	.channels_max =	 2,
	.buffer_bytes_max =     1200 * 1024 * 2,
	.period_bytes_min =	4800,
	.period_bytes_max =     1200 * 1024,
	.periods_min =	  2,
	.periods_max =	  512,
	.fifo_size =	    0,
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

static void compr_event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	struct compr_audio *compr = priv;
	struct msm_audio *prtd = &compr->prtd;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_aio_write_param param;
	struct audio_buffer *buf = NULL;
	int i = 0;

	pr_debug("%s opcode =%08x\n", __func__, opcode);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2: {
		uint32_t *ptrmem = (uint32_t *)&param;
		pr_debug("ASM_DATA_EVENT_WRITE_DONE\n");
		pr_debug("Buffer Consumed = 0x%08x\n", *ptrmem);
		prtd->pcm_irq_pos += prtd->pcm_count;
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		atomic_inc(&prtd->out_count);
		wake_up(&the_locks.write_wait);
		if (!atomic_read(&prtd->start)) {
			atomic_set(&prtd->pending_buffer, 1);
			break;
		} else
			atomic_set(&prtd->pending_buffer, 0);

		if (runtime->status->hw_ptr >= runtime->control->appl_ptr)
			break;
		buf = prtd->audio_client->port[IN].buf;
		pr_debug("%s:writing %d bytes of buffer[%d] to dsp 2\n",
				__func__, prtd->pcm_count, prtd->out_head);
		pr_debug("%s:writing buffer[%d] from 0x%08x\n",
				__func__, prtd->out_head,
				((unsigned int)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count)));

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
	case ASM_DATA_EVENT_RENDERED_EOS:
		pr_debug("ASM_DATA_CMDRSP_EOS\n");
		prtd->cmd_ack = 1;
		wake_up(&the_locks.eos_wait);
		break;
	case APR_BASIC_RSP_RESULT: {
		switch (payload[0]) {
		case ASM_SESSION_CMD_RUN_V2: {
			if (!atomic_read(&prtd->pending_buffer))
				break;
			pr_debug("%s:writing %d bytes of buffer[%d] to dsp\n",
				__func__, prtd->pcm_count, prtd->out_head);
			buf = prtd->audio_client->port[IN].buf;
			pr_debug("%s:writing buffer[%d] from 0x%08x\n",
				__func__, prtd->out_head,
				((unsigned int)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count)));
			param.paddr = (unsigned long)buf[prtd->out_head].phys;
			param.len = prtd->pcm_count;
			param.msw_ts = 0;
			param.lsw_ts = 0;
			param.flags = NO_TIMESTAMP;
			param.uid =  (unsigned long)buf[prtd->out_head].phys;
			if (q6asm_async_write(prtd->audio_client,
						&param) < 0)
				pr_err("%s:q6asm_async_write failed\n",
					__func__);
			else
				prtd->out_head =
					(prtd->out_head + 1)
					& (runtime->periods - 1);
			atomic_set(&prtd->pending_buffer, 0);
		}
			break;
		case ASM_STREAM_CMD_FLUSH:
			pr_debug("ASM_STREAM_CMD_FLUSH\n");
			prtd->cmd_ack = 1;
			wake_up(&the_locks.eos_wait);
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

static int msm_compr_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	struct asm_aac_cfg aac_cfg;
	int ret;

	pr_debug("%s\n", __func__);
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;
	prtd->out_head = 0;
	atomic_set(&prtd->out_count, runtime->periods);

	if (prtd->enabled)
		return 0;

	switch (compr->info.codec_param.codec.id) {
	case SND_AUDIOCODEC_MP3:
		/* No media format block for mp3 */
		break;
	case SND_AUDIOCODEC_AAC:
		pr_debug("SND_AUDIOCODEC_AAC\n");
		memset(&aac_cfg, 0x0, sizeof(struct asm_aac_cfg));
		aac_cfg.aot = AAC_ENC_MODE_EAAC_P;
		aac_cfg.format = 0x03;
		aac_cfg.ch_cfg = runtime->channels;
		aac_cfg.sample_rate =  runtime->rate;
		ret = q6asm_media_format_block_aac(prtd->audio_client,
					&aac_cfg);
		if (ret < 0)
			pr_err("%s: CMD Format block failed\n", __func__);
		break;
	default:
		return -EINVAL;
	}

	prtd->enabled = 1;
	prtd->cmd_ack = 0;

	return 0;
}

static int msm_compr_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;

	pr_debug("%s\n", __func__);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->pcm_irq_pos = 0;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("%s: Trigger start\n", __func__);
		q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
		atomic_set(&prtd->start, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("SNDRV_PCM_TRIGGER_STOP\n");
		atomic_set(&prtd->start, 0);
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

static void populate_codec_list(struct compr_audio *compr,
		struct snd_pcm_runtime *runtime)
{
	pr_debug("%s\n", __func__);
	/* MP3 Block */
	compr->info.compr_cap.num_codecs = 1;
	compr->info.compr_cap.min_fragment_size = runtime->hw.period_bytes_min;
	compr->info.compr_cap.max_fragment_size = runtime->hw.period_bytes_max;
	compr->info.compr_cap.min_fragments = runtime->hw.periods_min;
	compr->info.compr_cap.max_fragments = runtime->hw.periods_max;
	compr->info.compr_cap.codecs[0] = SND_AUDIOCODEC_MP3;
	compr->info.compr_cap.codecs[1] = SND_AUDIOCODEC_AAC;
	/* Add new codecs here */
}

static int msm_compr_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct compr_audio *compr;
	struct msm_audio *prtd;
	int ret = 0;
	struct asm_softpause_params softpause = {
		.enable = SOFT_PAUSE_ENABLE,
		.period = SOFT_PAUSE_PERIOD,
		.step = SOFT_PAUSE_STEP,
		.rampingcurve = SOFT_PAUSE_CURVE_LINEAR,
	};
	struct asm_softvolume_params softvol = {
		.period = SOFT_VOLUME_PERIOD,
		.step = SOFT_VOLUME_STEP,
		.rampingcurve = SOFT_VOLUME_CURVE_LINEAR,
	};

	/* Capture path */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return -EINVAL;

	pr_debug("%s\n", __func__);
	compr = kzalloc(sizeof(struct compr_audio), GFP_KERNEL);
	if (compr == NULL) {
		pr_err("Failed to allocate memory for msm_audio\n");
		return -ENOMEM;
	}
	prtd = &compr->prtd;
	prtd->substream = substream;
	prtd->audio_client = q6asm_audio_client_alloc(
				(app_cb)compr_event_handler, compr);
	if (!prtd->audio_client) {
		pr_info("%s: Could not allocate memory\n", __func__);
		kfree(prtd);
		return -ENOMEM;
	}
	runtime->hw = msm_compr_hardware_playback;

	pr_info("%s: session ID %d\n", __func__, prtd->audio_client->session);

	prtd->session_id = prtd->audio_client->session;
	msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->be_id,
			prtd->session_id, substream->stream);

	prtd->cmd_ack = 1;

	ret = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&constraints_sample_rates);
	if (ret < 0)
		pr_info("snd_pcm_hw_constraint_list failed\n");
	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
			    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		pr_info("snd_pcm_hw_constraint_integer failed\n");

	prtd->dsp_cnt = 0;
	atomic_set(&prtd->pending_buffer, 1);
	compr->codec = FORMAT_MP3;
	populate_codec_list(compr, runtime);
	runtime->private_data = compr;
	compressed_audio.prtd =  &compr->prtd;
	ret = compressed_set_volume(compressed_audio.volume);
	if (ret < 0)
		pr_err("%s : Set Volume failed : %d", __func__, ret);

	ret = q6asm_set_softpause(compressed_audio.prtd->audio_client,
								&softpause);
	if (ret < 0)
		pr_err("%s: Send SoftPause Param failed ret=%d\n",
			__func__, ret);
	ret = q6asm_set_softvolume(compressed_audio.prtd->audio_client,
								&softvol);
	if (ret < 0)
		pr_err("%s: Send SoftVolume Param failed ret=%d\n",
			__func__, ret);

	return 0;
}

int compressed_set_volume(unsigned volume)
{
	int rc = 0;
	if (compressed_audio.prtd && compressed_audio.prtd->audio_client) {
		rc = q6asm_set_volume(compressed_audio.prtd->audio_client,
								 volume);
		if (rc < 0) {
			pr_err("%s: Send Volume command failed rc=%d\n",
						__func__, rc);
		}
	}
	compressed_audio.volume = volume;
	return rc;
}

static int msm_compr_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	int dir = 0;

	pr_debug("%s\n", __func__);

	dir = IN;
	atomic_set(&prtd->pending_buffer, 0);
	q6asm_cmd(prtd->audio_client, CMD_CLOSE);
	compressed_audio.prtd = NULL;
	q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);

	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->be_id,
	SNDRV_PCM_STREAM_PLAYBACK);
	q6asm_audio_client_free(prtd->audio_client);
	kfree(prtd);
	return 0;
}

static int msm_compr_close(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_compr_playback_close(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = EINVAL;
	return ret;
}
static int msm_compr_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_compr_playback_prepare(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = EINVAL;
	return ret;
}

static snd_pcm_uframes_t msm_compr_pointer(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;

	if (prtd->pcm_irq_pos >= prtd->pcm_size)
		prtd->pcm_irq_pos = 0;

	pr_debug("pcm_irq_pos = %d\n", prtd->pcm_irq_pos);
	return bytes_to_frames(runtime, (prtd->pcm_irq_pos));
}

static int msm_compr_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	int result = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;

	pr_debug("%s\n", __func__);
	prtd->mmap_flag = 1;
	if (runtime->dma_addr && runtime->dma_bytes) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		result = remap_pfn_range(vma, vma->vm_start,
				runtime->dma_addr >> PAGE_SHIFT,
				runtime->dma_bytes,
				vma->vm_page_prot);
	} else {
		pr_err("Physical address or size of buf is NULL");
		return -EINVAL;
	}
	return result;
}

static int msm_compr_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct audio_buffer *buf;
	int dir, ret;

	pr_debug("%s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		return -EINVAL;

	ret = q6asm_open_write(prtd->audio_client, compr->codec);
	if (ret < 0) {
		pr_err("%s: Session out open failed\n", __func__);
		return -ENOMEM;
	}
	ret = q6asm_set_io_mode(prtd->audio_client, ASYNC_IO_MODE);
	if (ret < 0) {
		pr_err("%s: Set IO mode failed\n", __func__);
		return -ENOMEM;
	}

	ret = q6asm_audio_client_buf_alloc_contiguous(dir,
			prtd->audio_client,
			runtime->hw.period_bytes_min,
			runtime->hw.periods_max);
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed rc = %d\n",
						ret);
		return -ENOMEM;
	}
	buf = prtd->audio_client->port[dir].buf;

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

static int msm_compr_ioctl(struct snd_pcm_substream *substream,
		unsigned int cmd, void *arg)
{
	int rc = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	uint64_t timestamp;
	uint64_t temp;

	switch (cmd) {
	case SNDRV_COMPRESS_TSTAMP: {
		struct snd_compr_tstamp tstamp;
		pr_debug("SNDRV_COMPRESS_TSTAMP\n");

		memset(&tstamp, 0x0, sizeof(struct snd_compr_tstamp));
		timestamp = q6asm_get_session_time(prtd->audio_client);
		if (timestamp < 0) {
			pr_err("%s: Get Session Time return value =%lld\n",
				__func__, timestamp);
			return -EAGAIN;
		}
		temp = (timestamp * 2 * runtime->channels);
		temp = temp * (runtime->rate/1000);
		temp = div_u64(temp, 1000);
		tstamp.sampling_rate = runtime->rate;
		tstamp.timestamp = timestamp;
		pr_debug("%s: bytes_consumed:,timestamp = %lld,\n",
						__func__,
			tstamp.timestamp);
		if (copy_to_user((void *) arg, &tstamp,
			sizeof(struct snd_compr_tstamp)))
				return -EFAULT;
		return 0;
	}
	case SNDRV_COMPRESS_GET_CAPS:
		pr_debug("SNDRV_COMPRESS_GET_CAPS\n");
		if (copy_to_user((void *) arg, &compr->info.compr_cap,
			sizeof(struct snd_compr_caps))) {
			rc = -EFAULT;
			pr_err("%s: ERROR: copy to user\n", __func__);
			return rc;
		}
		return 0;
	case SNDRV_COMPRESS_SET_PARAMS:
		pr_debug("SNDRV_COMPRESS_SET_PARAMS: ");
		if (copy_from_user(&compr->info.codec_param, (void *) arg,
			sizeof(struct snd_compr_params))) {
			rc = -EFAULT;
			pr_err("%s: ERROR: copy from user\n", __func__);
			return rc;
		}
		switch (compr->info.codec_param.codec.id) {
		case SND_AUDIOCODEC_MP3:
			/* For MP3 we dont need any other parameter */
			pr_debug("SND_AUDIOCODEC_MP3\n");
			compr->codec = FORMAT_MP3;
			break;
		case SND_AUDIOCODEC_AAC:
			pr_debug("SND_AUDIOCODEC_AAC\n");
			compr->codec = FORMAT_MPEG4_AAC;
			break;
		default:
			pr_debug("FORMAT_LINEAR_PCM\n");
			compr->codec = FORMAT_LINEAR_PCM;
			break;
		}
		return 0;
	case SNDRV_PCM_IOCTL1_RESET:
		prtd->cmd_ack = 0;
		rc = q6asm_cmd(prtd->audio_client, CMD_FLUSH);
		if (rc < 0)
			pr_err("%s: flush cmd failed rc=%d\n", __func__, rc);
		rc = wait_event_timeout(the_locks.eos_wait,
			prtd->cmd_ack, 5 * HZ);
		if (rc < 0)
			pr_err("Flush cmd timeout\n");
		prtd->pcm_irq_pos = 0;
		break;
	default:
		break;
	}
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static struct snd_pcm_ops msm_compr_ops = {
	.open	   = msm_compr_open,
	.hw_params	= msm_compr_hw_params,
	.close	  = msm_compr_close,
	.ioctl	  = msm_compr_ioctl,
	.prepare	= msm_compr_prepare,
	.trigger	= msm_compr_trigger,
	.pointer	= msm_compr_pointer,
	.mmap		= msm_compr_mmap,
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
	.ops		= &msm_compr_ops,
	.pcm_new	= msm_asoc_pcm_new,
};

static __devinit int msm_compr_probe(struct platform_device *pdev)
{
	pr_info("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
				   &msm_soc_platform);
}

static int msm_compr_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver msm_compr_driver = {
	.driver = {
		.name = "msm-compr-dsp",
		.owner = THIS_MODULE,
	},
	.probe = msm_compr_probe,
	.remove = __devexit_p(msm_compr_remove),
};

static int __init msm_soc_platform_init(void)
{
	init_waitqueue_head(&the_locks.enable_wait);
	init_waitqueue_head(&the_locks.eos_wait);
	init_waitqueue_head(&the_locks.write_wait);
	init_waitqueue_head(&the_locks.read_wait);

	return platform_driver_register(&msm_compr_driver);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	platform_driver_unregister(&msm_compr_driver);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("PCM module platform driver");
MODULE_LICENSE("GPL v2");

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

#include "msm-pcm-q6-v2.h"
#include "msm-pcm-routing-v2.h"

static struct audio_locks the_locks;

struct snd_msm {
	struct snd_card *card;
	struct snd_pcm *pcm;
};

#define PLAYBACK_NUM_PERIODS	8
#define PLAYBACK_PERIOD_SIZE	2048
#define CAPTURE_NUM_PERIODS	16
#define CAPTURE_PERIOD_SIZE	512

static struct snd_pcm_hardware msm_pcm_hardware_capture = {
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
	.buffer_bytes_max =     CAPTURE_NUM_PERIODS * CAPTURE_PERIOD_SIZE,
	.period_bytes_min =	CAPTURE_PERIOD_SIZE,
	.period_bytes_max =     CAPTURE_PERIOD_SIZE,
	.periods_min =          CAPTURE_NUM_PERIODS,
	.periods_max =          CAPTURE_NUM_PERIODS,
	.fifo_size =            0,
};

static struct snd_pcm_hardware msm_pcm_hardware_playback = {
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
	.buffer_bytes_max =     PLAYBACK_NUM_PERIODS * PLAYBACK_PERIOD_SIZE,
	.period_bytes_min =	PLAYBACK_PERIOD_SIZE,
	.period_bytes_max =     PLAYBACK_PERIOD_SIZE,
	.periods_min =          PLAYBACK_NUM_PERIODS,
	.periods_max =          PLAYBACK_NUM_PERIODS,
	.fifo_size =            0,
};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static uint32_t in_frame_info[CAPTURE_NUM_PERIODS][2];

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
	uint32_t *ptrmem = (uint32_t *)payload;
	uint32_t idx = 0;
	uint32_t size = 0;

	pr_err("%s\n", __func__);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2: {
		pr_debug("ASM_DATA_EVENT_WRITE_DONE_V2\n");
		pr_debug("Buffer Consumed = 0x%08x\n", *ptrmem);
		prtd->pcm_irq_pos += prtd->pcm_count;
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		atomic_inc(&prtd->out_count);
		wake_up(&the_locks.write_wait);
		if (!atomic_read(&prtd->start))
			break;
		if (!prtd->mmap_flag)
			break;
		if (q6asm_is_cpu_buf_avail_nolock(IN,
				prtd->audio_client,
				&size, &idx)) {
			pr_debug("%s:writing %d bytes of buffer to dsp 2\n",
					__func__, prtd->pcm_count);
			q6asm_write_nolock(prtd->audio_client,
				prtd->pcm_count, 0, 0, NO_TIMESTAMP);
		}
		break;
	}
	case ASM_DATA_EVENT_RENDERED_EOS:
		pr_debug("ASM_DATA_EVENT_RENDERED_EOS\n");
		prtd->cmd_ack = 1;
		wake_up(&the_locks.eos_wait);
		break;
	case ASM_DATA_EVENT_READ_DONE_V2: {
		pr_debug("ASM_DATA_EVENT_READ_DONE_V2\n");
		pr_debug("token = 0x%08x\n", token);
		in_frame_info[token][0] = payload[4];
		in_frame_info[token][1] = payload[5];
		prtd->pcm_irq_pos += in_frame_info[token][0];
		pr_debug("pcm_irq_pos=%d\n", prtd->pcm_irq_pos);
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		if (atomic_read(&prtd->in_count) <= prtd->periods)
			atomic_inc(&prtd->in_count);
		wake_up(&the_locks.read_wait);
		if (prtd->mmap_flag
			&& q6asm_is_cpu_buf_avail_nolock(OUT,
				prtd->audio_client,
				&size, &idx))
			q6asm_read_nolock(prtd->audio_client);
		break;
	}
	case APR_BASIC_RSP_RESULT: {
		switch (payload[0]) {
		case ASM_SESSION_CMD_RUN_V2:
			if (substream->stream
				!= SNDRV_PCM_STREAM_PLAYBACK) {
				atomic_set(&prtd->start, 1);
				break;
			}
			if (prtd->mmap_flag) {
				pr_debug("%s:writing %d bytes"
					" of buffer to dsp\n",
					__func__,
					prtd->pcm_count);
				q6asm_write_nolock(prtd->audio_client,
					prtd->pcm_count,
					0, 0, NO_TIMESTAMP);
			} else {
				while (atomic_read(&prtd->out_needed)) {
					pr_debug("%s:writing %d bytes"
						 " of buffer to dsp\n",
						__func__,
						prtd->pcm_count);
					q6asm_write_nolock(prtd->audio_client,
						prtd->pcm_count,
						0, 0, NO_TIMESTAMP);
					atomic_dec(&prtd->out_needed);
					wake_up(&the_locks.write_wait);
				};
			}
			atomic_set(&prtd->start, 1);
			break;
		default:
			pr_debug("%s:Payload = [0x%x]stat[0x%x]\n",
				__func__, payload[0], payload[1]);
			break;
		}
	}
	break;
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
		pr_info("%s: CMD Format block failed\n", __func__);

	atomic_set(&prtd->out_count, runtime->periods);

	prtd->enabled = 1;
	prtd->cmd_ack = 0;

	return 0;
}

static int msm_pcm_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	int ret = 0;
	int i = 0;
	pr_debug("%s\n", __func__);
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;

	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;

	if (prtd->enabled)
		return 0;

	pr_debug("Samp_rate = %d\n", prtd->samp_rate);
	pr_debug("Channel = %d\n", prtd->channel_mode);
	ret = q6asm_enc_cfg_blk_pcm(prtd->audio_client, prtd->samp_rate,
					prtd->channel_mode);
	if (ret < 0)
		pr_debug("%s: cmd cfg pcm was block failed", __func__);

	for (i = 0; i < runtime->periods; i++)
		q6asm_read(prtd->audio_client);
	prtd->periods = runtime->periods;

	prtd->enabled = 1;

	return ret;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("%s: Trigger start\n", __func__);
		q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
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
	prtd->substream = substream;
	prtd->audio_client = q6asm_audio_client_alloc(
				(app_cb)event_handler, prtd);
	if (!prtd->audio_client) {
		pr_info("%s: Could not allocate memory\n", __func__);
		kfree(prtd);
		return -ENOMEM;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		runtime->hw = msm_pcm_hardware_playback;
		ret = q6asm_open_write(prtd->audio_client, FORMAT_LINEAR_PCM);
		if (ret < 0) {
			pr_err("%s: pcm out open failed\n", __func__);
			q6asm_audio_client_free(prtd->audio_client);
			kfree(prtd);
			return -ENOMEM;
		}
	}
	/* Capture path */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		runtime->hw = msm_pcm_hardware_capture;
		ret = q6asm_open_read(prtd->audio_client, FORMAT_LINEAR_PCM);
		if (ret < 0) {
			pr_err("%s: pcm in open failed\n", __func__);
			q6asm_audio_client_free(prtd->audio_client);
			kfree(prtd);
			return -ENOMEM;
		}
	}

	pr_debug("%s: session ID %d\n", __func__, prtd->audio_client->session);

	prtd->session_id = prtd->audio_client->session;
	msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->be_id,
			prtd->session_id, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
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
	runtime->private_data = prtd;

	return 0;
}

static int msm_pcm_playback_copy(struct snd_pcm_substream *substream, int a,
	snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	int ret = 0;
	int fbytes = 0;
	int xfer = 0;
	char *bufptr = NULL;
	void *data = NULL;
	uint32_t idx = 0;
	uint32_t size = 0;

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	fbytes = frames_to_bytes(runtime, frames);
	pr_debug("%s: prtd->out_count = %d\n",
				__func__, atomic_read(&prtd->out_count));
	ret = wait_event_timeout(the_locks.write_wait,
			(atomic_read(&prtd->out_count)), 5 * HZ);
	if (ret < 0) {
		pr_err("%s: wait_event_timeout failed\n", __func__);
		goto fail;
	}

	if (!atomic_read(&prtd->out_count)) {
		pr_err("%s: pcm stopped out_count 0\n", __func__);
		return 0;
	}

	data = q6asm_is_cpu_buf_avail(IN, prtd->audio_client, &size, &idx);
	bufptr = data;
	if (bufptr) {
		pr_debug("%s:fbytes =%d: xfer=%d size=%d\n",
					__func__, fbytes, xfer, size);
		xfer = fbytes;
		if (copy_from_user(bufptr, buf, xfer)) {
			ret = -EFAULT;
			goto fail;
		}
		buf += xfer;
		fbytes -= xfer;
		pr_debug("%s:fbytes = %d: xfer=%d\n", __func__, fbytes, xfer);
		if (atomic_read(&prtd->start)) {
			pr_debug("%s:writing %d bytes of buffer to dsp\n",
					__func__, xfer);
			ret = q6asm_write(prtd->audio_client, xfer,
						0, 0, NO_TIMESTAMP);
			if (ret < 0) {
				ret = -EFAULT;
				goto fail;
			}
		} else
			atomic_inc(&prtd->out_needed);
		atomic_dec(&prtd->out_count);
	}
fail:
	return  ret;
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
	q6asm_cmd(prtd->audio_client, CMD_CLOSE);
	q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);

	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->be_id,
	SNDRV_PCM_STREAM_PLAYBACK);
	q6asm_audio_client_free(prtd->audio_client);
	kfree(prtd);
	return 0;
}

static int msm_pcm_capture_copy(struct snd_pcm_substream *substream,
		 int channel, snd_pcm_uframes_t hwoff, void __user *buf,
						 snd_pcm_uframes_t frames)
{
	int ret = 0;
	int fbytes = 0;
	int xfer;
	char *bufptr;
	void *data = NULL;
	static uint32_t idx;
	static uint32_t size;
	uint32_t offset = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = substream->runtime->private_data;


	pr_debug("%s\n", __func__);
	fbytes = frames_to_bytes(runtime, frames);

	pr_debug("appl_ptr %d\n", (int)runtime->control->appl_ptr);
	pr_debug("hw_ptr %d\n", (int)runtime->status->hw_ptr);
	pr_debug("avail_min %d\n", (int)runtime->control->avail_min);

	ret = wait_event_timeout(the_locks.read_wait,
			(atomic_read(&prtd->in_count)), 5 * HZ);
	if (ret < 0) {
		pr_debug("%s: wait_event_timeout failed\n", __func__);
		goto fail;
	}
	if (!atomic_read(&prtd->in_count)) {
		pr_debug("%s: pcm stopped in_count 0\n", __func__);
		return 0;
	}
	pr_debug("Checking if valid buffer is available...%08x\n",
						(unsigned int) data);
	data = q6asm_is_cpu_buf_avail(OUT, prtd->audio_client, &size, &idx);
	bufptr = data;
	pr_debug("Size = %d\n", size);
	pr_debug("fbytes = %d\n", fbytes);
	pr_debug("idx = %d\n", idx);
	if (bufptr) {
		xfer = fbytes;
		if (xfer > size)
			xfer = size;
		offset = in_frame_info[idx][1];
		pr_debug("Offset value = %d\n", offset);
		if (copy_to_user(buf, bufptr+offset, xfer)) {
			pr_err("Failed to copy buf to user\n");
			ret = -EFAULT;
			goto fail;
		}
		fbytes -= xfer;
		size -= xfer;
		in_frame_info[idx][1] += xfer;
		pr_debug("%s:fbytes = %d: size=%d: xfer=%d\n",
					__func__, fbytes, size, xfer);
		pr_debug(" Sending next buffer to dsp\n");
		memset(&in_frame_info[idx], 0,
			sizeof(uint32_t) * 2);
		atomic_dec(&prtd->in_count);
		ret = q6asm_read(prtd->audio_client);
		if (ret < 0) {
			pr_err("q6asm read failed\n");
			ret = -EFAULT;
			goto fail;
		}
	} else
		pr_err("No valid buffer\n");

	pr_debug("Returning from capture_copy... %d\n", ret);
fail:
	return ret;
}

static int msm_pcm_capture_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	int dir = OUT;

	pr_debug("%s\n", __func__);
	q6asm_cmd(prtd->audio_client, CMD_CLOSE);
	q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);
	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->be_id,
	SNDRV_PCM_STREAM_CAPTURE);
	q6asm_audio_client_free(prtd->audio_client);
	kfree(prtd);

	return 0;
}

static int msm_pcm_copy(struct snd_pcm_substream *substream, int a,
	 snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_copy(substream, a, hwoff, buf, frames);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_copy(substream, a, hwoff, buf, frames);
	return ret;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_close(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_close(substream);
	return ret;
}
static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_prepare(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_prepare(substream);
	return ret;
}

static snd_pcm_uframes_t msm_pcm_pointer(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	if (prtd->pcm_irq_pos >= prtd->pcm_size)
		prtd->pcm_irq_pos = 0;

	pr_debug("pcm_irq_pos = %d\n", prtd->pcm_irq_pos);
	return bytes_to_frames(runtime, (prtd->pcm_irq_pos));
}

static int msm_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	int result = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

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
		dir = OUT;
pr_err("%s: before buf alloc\n", __func__);
	ret = q6asm_audio_client_buf_alloc_contiguous(dir,
			prtd->audio_client,
			runtime->hw.period_bytes_min,
			runtime->hw.periods_max);
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed "
					"rc = %d\n", ret);
		return -ENOMEM;
	}
pr_err("%s: after buf alloc\n", __func__);
	buf = prtd->audio_client->port[dir].buf;
	if (buf == NULL || buf[0].data == NULL)
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
	.copy		= msm_pcm_copy,
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
	pr_info("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
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
		.name = "msm-pcm-dsp",
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

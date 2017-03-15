/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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
#include <sound/pcm_params.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>

#include <linux/of_device.h>
#include <linux/msm_audio_ion.h>

#include <sound/compress_params.h>
#include <sound/compress_offload.h>
#include <sound/compress_driver.h>
#include <sound/timer.h>
#include <sound/pcm_params.h>

#include "msm-pcm-q6-v2.h"
#include "msm-pcm-routing-v2.h"
#include <sound/pcm.h>
#include <sound/tlv.h>

#define LPA_LR_VOL_MAX_STEPS	0x20002000

const DECLARE_TLV_DB_LINEAR(lpa_rx_vol_gain, 0,
			    LPA_LR_VOL_MAX_STEPS);
static struct audio_locks the_locks;

static struct snd_pcm_hardware msm_pcm_hardware = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =              SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE,
	.rates =                SNDRV_PCM_RATE_8000_192000 |
					SNDRV_PCM_RATE_KNOT,
	.rate_min =             8000,
	.rate_max =             192000,
	.channels_min =         1,
	.channels_max =         2,
	.buffer_bytes_max =     1024 * 1024,
	.period_bytes_min =	128 * 1024,
	.period_bytes_max =     256 * 1024,
	.periods_min =          4,
	.periods_max =          8,
	.fifo_size =            0,
};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
	96000, 192000
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
	struct audio_buffer *buf = NULL;
	struct output_meta_data_st output_meta_data;
	unsigned long flag = 0;
	int i = 0;

	memset(&output_meta_data, 0x0, sizeof(struct output_meta_data_st));
	spin_lock_irqsave(&the_locks.event_lock, flag);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2: {
		uint32_t *ptrmem = (uint32_t *)&param;
		dma_addr_t temp;
		pr_debug("ASM_DATA_EVENT_WRITE_DONE_V2\n");
		pr_debug("Buffer Consumed = 0x%08x\n", *ptrmem);
		prtd->pcm_irq_pos += prtd->pcm_count;
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		else
			if (substream->timer_running)
				snd_timer_interrupt(substream->timer, 1);

		atomic_inc(&prtd->out_count);
		wake_up(&the_locks.write_wait);
		if (!atomic_read(&prtd->start)) {
			atomic_set(&prtd->pending_buffer, 1);
			break;
		} else
			atomic_set(&prtd->pending_buffer, 0);

		buf = prtd->audio_client->port[IN].buf;
		if (runtime->status->hw_ptr >= runtime->control->appl_ptr) {
			runtime->render_flag |= SNDRV_RENDER_STOPPED;
			pr_info("%s:lpa driver underrun\n", __func__);
			break;
		}
		pr_debug("%s:writing %d bytes of buffer[%d] to dsp 2\n",
				__func__, prtd->pcm_count, prtd->out_head);
		temp = buf[0].phys + (prtd->out_head * prtd->pcm_count);
		pr_debug("%s:writing buffer[%d] from 0x%pK\n",
				__func__, prtd->out_head, &temp);
		if (prtd->meta_data_mode) {
			memcpy(&output_meta_data, (char *)(buf->data +
			prtd->out_head * prtd->pcm_count),
			sizeof(struct output_meta_data_st));
			param.len = output_meta_data.frame_size;
		} else {
			param.len = prtd->pcm_count;
		}
		pr_debug("meta_data_length: %d, frame_length: %d\n",
			output_meta_data.meta_data_length,
			output_meta_data.frame_size);
		param.paddr = temp +
			output_meta_data.meta_data_length;
		param.msw_ts = output_meta_data.timestamp_msw;
		param.lsw_ts = output_meta_data.timestamp_lsw;
		param.flags = NO_TIMESTAMP;
		param.uid = prtd->session_id;
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
		atomic_set(&prtd->pending_buffer, 0);
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
			pr_debug("%s:writing %d bytes of buffer to dsp\n",
				__func__, prtd->pcm_count);
			buf = prtd->audio_client->port[IN].buf;
			pr_debug("%s:writing buffer[%d] from 0x%08x\n",
				__func__, prtd->out_head,
				((unsigned int)buf[0].phys +
				(prtd->out_head * prtd->pcm_count)));
			if (prtd->meta_data_mode) {
				memcpy(&output_meta_data, (char *)(buf->data +
					prtd->out_head * prtd->pcm_count),
					sizeof(struct output_meta_data_st));
				param.len = output_meta_data.frame_size;
			} else {
				param.len = prtd->pcm_count;
			}
			param.paddr = buf[prtd->out_head].phys +
				output_meta_data.meta_data_length;
			param.msw_ts = output_meta_data.timestamp_msw;
			param.lsw_ts = output_meta_data.timestamp_lsw;
			param.flags = NO_TIMESTAMP;
			param.uid = prtd->session_id;
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
	case RESET_EVENTS:
		pr_debug("%s RESET_EVENTS\n", __func__);
		prtd->cmd_ack = 1;
		prtd->reset_event = true;
		wake_up(&the_locks.eos_wait);
		break;
	default:
		pr_debug("Not Supported Event opcode[0x%x]\n", opcode);
		break;
	}
	spin_unlock_irqrestore(&the_locks.event_lock, flag);
}

static int msm_pcm_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	int ret;
	uint16_t bits_per_sample = 16;
	u32 io_mode = ASYNC_IO_MODE;

	pr_debug("%s\n", __func__);
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;
	prtd->out_head = 0;
	if (prtd->enabled)
		return 0;

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		bits_per_sample = 16;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		bits_per_sample = 24;
		break;
	}

	if (prtd->meta_data_mode)
		io_mode |= COMPRESSED_IO;

	ret = q6asm_set_io_mode(prtd->audio_client, io_mode);
	if (ret < 0) {
		pr_err("%s: Set IO mode failed\n", __func__);
		q6asm_audio_client_free(prtd->audio_client);
		prtd->audio_client = NULL;
		return -ENOMEM;
	}

	ret = q6asm_media_format_block_pcm_format_support(
				prtd->audio_client, runtime->rate,
				runtime->channels, bits_per_sample);
	if (ret < 0)
		pr_debug("%s: CMD Format block failed\n", __func__);

	atomic_set(&prtd->out_count, runtime->periods);
	prtd->enabled = 1;
	prtd->cmd_ack = 0;
	prtd->cmd_interrupt = 0;

	return 0;
}

static int msm_pcm_restart(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct audio_aio_write_param param;
	struct audio_buffer *buf = NULL;
	struct output_meta_data_st output_meta_data;

	pr_debug("%s: restart\n", __func__);
	memset(&output_meta_data, 0x0, sizeof(struct output_meta_data_st));
	if (runtime->render_flag & SNDRV_RENDER_STOPPED) {
		buf = prtd->audio_client->port[IN].buf;
		pr_debug("%s:writing %d bytes of buffer[%d] to dsp 2\n",
				__func__, prtd->pcm_count, prtd->out_head);
		pr_debug("%s:writing buffer[%d] from 0x%08x\n",
				__func__, prtd->out_head,
				((unsigned int)buf[0].phys +
				(prtd->out_head * prtd->pcm_count)));
		if (prtd->meta_data_mode) {
			memcpy(&output_meta_data, (char *)(buf->data +
				prtd->out_head * prtd->pcm_count),
				sizeof(struct output_meta_data_st));
			param.len = output_meta_data.frame_size;
		} else {
			param.len = prtd->pcm_count;
		}
		pr_debug("meta_data_length: %d, frame_length: %d\n",
				output_meta_data.meta_data_length,
				output_meta_data.frame_size);
		pr_debug("timestamp_msw: %d, timestamp_lsw: %d\n",
				output_meta_data.timestamp_msw,
			output_meta_data.timestamp_lsw);
		param.paddr = (buf[0].phys +
				(prtd->out_head * prtd->pcm_count) +
				output_meta_data.meta_data_length);
		param.msw_ts = output_meta_data.timestamp_msw;
		param.lsw_ts = output_meta_data.timestamp_lsw;
		param.flags = NO_TIMESTAMP;
		param.uid = prtd->session_id;
		if (q6asm_async_write(prtd->audio_client, &param) < 0)
			pr_err("%s:q6asm_async_write failed\n",
							__func__);
		else
			prtd->out_head =
				(prtd->out_head + 1) & (runtime->periods - 1);
		runtime->render_flag &= ~SNDRV_RENDER_STOPPED;
		return 0;
	}
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
		prtd->pcm_irq_pos = 0;
		atomic_set(&prtd->pending_buffer, 1);
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("SNDRV_PCM_TRIGGER_START\n");
		q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
		atomic_set(&prtd->start, 1);
		atomic_set(&prtd->stop, 0);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("SNDRV_PCM_TRIGGER_STOP\n");
		atomic_set(&prtd->start, 0);
		atomic_set(&prtd->stop, 1);
		runtime->render_flag &= ~SNDRV_RENDER_STOPPED;
		if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK)
			break;
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("SNDRV_PCM_TRIGGER_PAUSE\n");
		q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
		atomic_set(&prtd->start, 0);
		runtime->render_flag &= ~SNDRV_RENDER_STOPPED;
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
	prtd->reset_event = false;
	runtime->render_flag = SNDRV_DMA_MODE;
	prtd->audio_client = q6asm_audio_client_alloc(
				(app_cb)event_handler, prtd);
	if (!prtd->audio_client) {
		pr_debug("%s: Could not allocate memory\n", __func__);
		kfree(prtd);
		return -ENOMEM;
	}

	prtd->meta_data_mode = false;
	/* Capture path */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return -EPERM;

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
	atomic_set(&prtd->pending_buffer, 1);
	atomic_set(&prtd->stop, 1);
	runtime->private_data = prtd;
	return 0;
}

static int lpa_set_volume(struct msm_audio *prtd, uint32_t volume)
{
	int rc = 0;
	if (prtd && prtd->audio_client) {
		rc = q6asm_set_lrgain(prtd->audio_client,
				      (volume >> 16) & 0xFFFF, volume & 0xFFFF);
		if (rc < 0) {
			pr_err("%s: Send Volume command failed rc=%d\n",
						__func__, rc);
		} else {
			prtd->volume = volume;
		}
	}
	return rc;
}

static int msm_pcm_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	int dir = 0;
	int rc = 0;

	/*
	If routing is still enabled, we need to issue EOS to
	the DSP
	To issue EOS to dsp, we need to be run state otherwise
	EOS is not honored.
	*/
	if (msm_routing_check_backend_enabled(soc_prtd->dai_link->be_id) &&
		(!atomic_read(&prtd->stop))) {
		rc = q6asm_run(prtd->audio_client, 0, 0, 0);
		atomic_set(&prtd->pending_buffer, 0);
		prtd->cmd_ack = 0;
		q6asm_cmd_nowait(prtd->audio_client, CMD_EOS);
		pr_debug("%s\n", __func__);
		rc = wait_event_timeout(the_locks.eos_wait,
			prtd->cmd_ack, 5 * HZ);
		if (!rc)
			pr_err("EOS cmd timeout\n");
		prtd->pcm_irq_pos = 0;
	}

	if (prtd->audio_client) {
		dir = IN;
		atomic_set(&prtd->pending_buffer, 0);

		q6asm_cmd(prtd->audio_client, CMD_CLOSE);
		q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);

		atomic_set(&prtd->stop, 1);
		q6asm_audio_client_free(prtd->audio_client);
		pr_debug("%s\n", __func__);
	}
	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->be_id,
		SNDRV_PCM_STREAM_PLAYBACK);

	prtd->meta_data_mode = false;

	pr_debug("%s\n", __func__);
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
	struct audio_client *ac = prtd->audio_client;
	struct audio_port_data *apd = ac->port;
	struct audio_buffer *ab;
	int dir = -1;

	prtd->mmap_flag = 1;
	runtime->render_flag = SNDRV_NON_DMA_MODE;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		dir = OUT;
	ab = &(apd[dir].buf[0]);

	return msm_audio_ion_mmap(ab, vma);
}

static int msm_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct audio_buffer *buf;
	uint16_t bits_per_sample = 16;
	int dir, ret;

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

	prtd->audio_client->perf_mode = false;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE)
			bits_per_sample = 24;
		ret = q6asm_open_write_v2(prtd->audio_client,
				FORMAT_LINEAR_PCM, bits_per_sample);
		if (ret < 0) {
			pr_err("%s: pcm out open failed\n", __func__);
			q6asm_audio_client_free(prtd->audio_client);
			prtd->audio_client = NULL;
			return -ENOMEM;
		}
	}

	pr_debug("%s: session ID %d\n", __func__, prtd->audio_client->session);
	prtd->session_id = prtd->audio_client->session;
	msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->be_id,
		prtd->audio_client->perf_mode,
		prtd->session_id, substream->stream);

	ret = q6asm_set_softpause(prtd->audio_client, &softpause);
	if (ret < 0)
		pr_err("%s: Send SoftPause Param failed ret=%d\n",
			__func__, ret);
	ret = q6asm_set_softvolume(prtd->audio_client, &softvol);
	if (ret < 0)
		pr_err("%s: Send SoftVolume Param failed ret=%d\n",
			__func__, ret);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		return -EPERM;
	ret = q6asm_audio_client_buf_alloc_contiguous(dir,
			prtd->audio_client,
			params_period_bytes(params),
			params_periods(params));
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed rc = %d\n",
						ret);
		return -ENOMEM;
	}
	buf = prtd->audio_client->port[dir].buf;

	if (buf == NULL || buf[0].data == NULL)
		return -ENOMEM;

	pr_debug("%s:buf = %pK\n", __func__, buf);
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->area = buf[0].data;
	dma_buf->addr =  buf[0].phys;
	dma_buf->bytes = params_period_bytes(params) * params_periods(params);
	if (!dma_buf->area)
		return -ENOMEM;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static int msm_pcm_ioctl(struct snd_pcm_substream *substream,
		unsigned int cmd, void *arg)
{
	int rc = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	uint64_t timestamp;
	uint64_t temp;

	switch (cmd) {
	case SNDRV_COMPRESS_TSTAMP: {
		struct snd_compr_tstamp tstamp;
		pr_debug("SNDRV_COMPRESS_TSTAMP\n");

		memset(&tstamp, 0x0, sizeof(struct snd_compr_tstamp));
		rc = q6asm_get_session_time(prtd->audio_client, &timestamp);
		if (rc < 0) {
			pr_err("%s: Fail to get session time stamp, rc:%d\n",
							__func__, rc);
			return -EAGAIN;
		}
		temp = (timestamp * 2 * runtime->channels);
		temp = temp * (runtime->rate/1000);
		temp = div_u64(temp, 1000);
		tstamp.sampling_rate = runtime->rate;
		tstamp.timestamp = timestamp;
		pr_debug("%s: bytes_consumed:timestamp = %lld,\n",
					__func__,
			tstamp.timestamp);
		if (copy_to_user((void *) arg, &tstamp,
			sizeof(struct snd_compr_tstamp)))
			return -EFAULT;
		return 0;
	}
	case SNDRV_PCM_IOCTL1_RESET:
		prtd->cmd_ack = 0;
		rc = q6asm_cmd(prtd->audio_client, CMD_FLUSH);
		if (rc < 0)
			pr_err("%s: flush cmd failed rc=%d\n", __func__, rc);
		if (prtd->reset_event == true) {
			prtd->cmd_ack = 1;
			prtd->reset_event = false;
			return -ENETRESET;
		}
		rc = wait_event_timeout(the_locks.eos_wait,
			!prtd->reset_event && prtd->cmd_ack, 5 * HZ);
		if (!rc)
			pr_err("Flush cmd timeout\n");
		prtd->pcm_irq_pos = 0;
		break;
	case SNDRV_COMPRESS_METADATA_MODE:
		if (!atomic_read(&prtd->start)) {
			pr_debug("Metadata mode enabled\n");
			prtd->meta_data_mode = true;
			return 0;
		}
		pr_debug("Metadata mode not enabled\n");
		return -EPERM;
	default:
		break;
	}
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static int msm_lpa_volume_ctl_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct snd_pcm_substream *substream =
			 vol->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct msm_audio *prtd;
	int volume = ucontrol->value.integer.value[0];

	pr_debug("%s: volume : %x\n", __func__, volume);
	if (!substream)
		return -ENODEV;
	if (!substream->runtime)
		return 0;
	prtd = substream->runtime->private_data;
	if (prtd)
		rc = lpa_set_volume(prtd, volume);

	return rc;
}

static int msm_lpa_volume_ctl_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct snd_pcm_substream *substream =
			 vol->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct msm_audio *prtd;

	pr_debug("%s\n", __func__);
	if (!substream)
		return -ENODEV;
	if (!substream->runtime)
		return 0;
	prtd = substream->runtime->private_data;
	if (prtd)
		ucontrol->value.integer.value[0] = prtd->volume;
	return 0;
}

static int msm_lpa_add_controls(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_pcm *pcm = rtd->pcm->streams[0].pcm;
	struct snd_pcm_volume *volume_info;
	struct snd_kcontrol *kctl;

	dev_dbg(rtd->dev, "%s, Volume cntrl add\n", __func__);
	ret = snd_pcm_add_volume_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
				      NULL, 1, rtd->dai_link->be_id,
				      &volume_info);
	if (ret < 0)
		return ret;
	kctl = volume_info->kctl;
	kctl->put = msm_lpa_volume_ctl_put;
	kctl->get = msm_lpa_volume_ctl_get;
	kctl->tlv.p = lpa_rx_vol_gain;
	return 0;
}

static struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.hw_params	= msm_pcm_hw_params,
	.close          = msm_pcm_close,
	.ioctl          = msm_pcm_ioctl,
	.prepare        = msm_pcm_prepare,
	.trigger        = msm_pcm_trigger,
	.pointer        = msm_pcm_pointer,
	.mmap		= msm_pcm_mmap,
	.restart	= msm_pcm_restart,
};

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = msm_lpa_add_controls(rtd);
	if (ret)
		pr_err("%s, kctl add failed\n", __func__);
	return ret;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_pcm_ops,
	.pcm_new	= msm_asoc_pcm_new,
};

static int msm_pcm_probe(struct platform_device *pdev)
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

static const struct of_device_id msm_pcm_lpa_dt_match[] = {
	{.compatible = "qcom,msm-pcm-lpa"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_pcm_lpa_dt_match);

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-pcm-lpa",
		.owner = THIS_MODULE,
		.of_match_table = msm_pcm_lpa_dt_match,
	},
	.probe = msm_pcm_probe,
	.remove = msm_pcm_remove,
};

static int __init msm_soc_platform_init(void)
{
	spin_lock_init(&the_locks.event_lock);
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

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
#include <linux/delay.h>
#include <linux/android_pmem.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>

#include "msm8x60-pcm.h"

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
	.buffer_bytes_max =     960 * 10,
	.period_bytes_min =     960 * 5,
	.period_bytes_max =     960 * 5,
	.periods_min =          2,
	.periods_max =          2,
	.fifo_size =            0,
};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

uint32_t in_frame_info[8][2];

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static void alsa_out_listener(u32 evt_id, union auddev_evt_data *evt_payload,
							void *private_data)
{
	int ret = 0;
	struct msm_audio *prtd = (struct msm_audio *) private_data;
	int dev_rate = 48000;
	pr_debug("evt_id = 0x%8x\n", evt_id);
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		pr_debug("AUDDEV_EVT_DEV_RDY\n");
		prtd->copp_id = evt_payload->routing_id;
		pr_debug("prtd->session_id = %d, copp_id= %d",
			prtd->session_id, prtd->copp_id);
		if (prtd->copp_id == PCM_RX)
			dev_rate = 8000;

		ret = msm_snddev_set_dec(prtd->session_id, prtd->copp_id, 1,
			dev_rate, 1);
		break;
	case AUDDEV_EVT_DEV_RLS:
		pr_debug("AUDDEV_EVT_DEV_RLS\n");
		prtd->copp_id = evt_payload->routing_id;
		pr_debug("prtd->session_id = %d, copp_id= %d",
			prtd->session_id, prtd->copp_id);
		if (prtd->copp_id == PCM_RX)
			dev_rate = 8000;

		ret = msm_snddev_set_dec(prtd->session_id, prtd->copp_id, 0,
			dev_rate, 1);
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		pr_debug("AUDDEV_EVT_STREAM_VOL_CHG\n");
		break;
	default:
		pr_debug("Unknown Event\n");
		break;
	}
}

static void alsa_in_listener(u32 evt_id, union auddev_evt_data *evt_payload,
							void *private_data)
{
	int ret = 0;
	struct msm_audio *prtd = (struct msm_audio *) private_data;
	int dev_rate = 48000;
	pr_debug("evt_id = 0x%8x\n", evt_id);

	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		prtd->copp_id = evt_payload->routing_id;
		if (prtd->copp_id == PCM_TX)
			dev_rate = 8000;

		ret = msm_snddev_set_enc(prtd->session_id, prtd->copp_id, 1,
			dev_rate, 1);
		break;
	case AUDDEV_EVT_DEV_RLS:
		prtd->copp_id = evt_payload->routing_id;
		if (prtd->copp_id == PCM_TX)
			dev_rate = 8000;

		ret = msm_snddev_set_enc(prtd->session_id, prtd->copp_id, 0,
			dev_rate, 1);
		break;
	default:
		pr_debug("Unknown Event\n");
		break;
	}
}

static void event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	struct msm_audio *prtd = priv;
	struct snd_pcm_substream *substream = prtd->substream;
	uint32_t *ptrmem = (uint32_t *)payload;
	int i = 0;

	pr_debug("%s\n", __func__);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE: {
		pr_debug("ASM_DATA_EVENT_WRITE_DONE\n");
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
		pr_debug("%s:writing %d bytes of buffer to dsp 2\n",
				__func__, prtd->pcm_count);
		q6asm_write_nolock(prtd->audio_client,
			prtd->pcm_count, 0, 0, NO_TIMESTAMP);
		break;
	}
	case ASM_DATA_CMDRSP_EOS:
		pr_debug("ASM_DATA_CMDRSP_EOS\n");
		prtd->cmd_ack = 1;
		wake_up(&the_locks.eos_wait);
		break;
	case ASM_DATA_EVENT_READ_DONE: {
		pr_debug("ASM_DATA_EVENT_READ_DONE\n");
		pr_debug("token = 0x%08x\n", token);
		for (i = 0; i < 8; i++, ++ptrmem)
			pr_debug("cmd[%d]=0x%08x\n", i, *ptrmem);
		in_frame_info[token][0] = payload[2];
		in_frame_info[token][1] = payload[3];
		prtd->pcm_irq_pos += in_frame_info[token][0];
		pr_debug("pcm_irq_pos=%d\n", prtd->pcm_irq_pos);
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		if (atomic_read(&prtd->in_count) <= prtd->periods)
			atomic_inc(&prtd->in_count);
		wake_up(&the_locks.read_wait);
		if (prtd->mmap_flag)
			q6asm_read_nolock(prtd->audio_client);
		break;
	}
	case APR_BASIC_RSP_RESULT: {
		if (!prtd->mmap_flag
			&& !atomic_read(&prtd->out_needed))
			break;
		switch (payload[0]) {
		case ASM_SESSION_CMD_RUN:
			if (substream->stream
				!= SNDRV_PCM_STREAM_PLAYBACK)
				break;
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
			break;
		default:
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
	int dev_rate = 48000;
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

	ret = q6asm_media_format_block_pcm(prtd->audio_client, runtime->rate,
				runtime->channels);
	if (ret < 0)
		pr_debug("%s: CMD Format block failed\n", __func__);

	atomic_set(&prtd->out_count, runtime->periods);
	atomic_set(&prtd->in_count, 0);
	for (i = 0; i < MAX_COPP; i++) {
		pr_debug("prtd->session_id = %d, copp_id= %d",
			prtd->session_id, i);
		if (session_route.playback_session[substream->number][i]
				!= DEVICE_IGNORE) {
			pr_err("Device active\n");
			if (i == PCM_RX)
				dev_rate = 8000;
			msm_snddev_set_dec(prtd->session_id,
				       i, 1, dev_rate, runtime->channels);
		}
	}
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
	int dev_rate = 48000;
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
		q6asm_read_nolock(prtd->audio_client);
	prtd->periods = runtime->periods;
	for (i = 0; i < MAX_COPP; i++) {
		pr_debug("prtd->session_id = %d, copp_id= %d",
			prtd->session_id,
			session_route.capture_session[prtd->session_id][i]);
		if (session_route.capture_session[prtd->session_id][i]
				!= DEVICE_IGNORE) {
			if (i == PCM_RX)
				dev_rate = 8000;
			msm_snddev_set_enc(prtd->session_id, i, 1, dev_rate, 1);
		}
	}
	prtd->enabled = 1;

	return ret;
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
	}
	/* Capture path */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		ret = q6asm_open_read(prtd->audio_client, FORMAT_LINEAR_PCM);
		if (ret < 0) {
			pr_err("%s: pcm in open failed\n", __func__);
			q6asm_audio_client_free(prtd->audio_client);
			kfree(prtd);
			return -ENOMEM;
		}
	}
	/* The session id returned by q6asm_open_read above is random and
	 * hence we cannot use the session id to route from user space.
	 * This results in need of a hardcoded session id for both playback
	 * and capture sessions. we can use the subdevice id to identify
	 * the session and use that for routing. Hence using
	 * substream->number as the session id for routing purpose. However
	 * DSP understands the session based on the allocated session id,
	 * hence using the variable prtd->session_id for all dsp commands.
	 */

	prtd->session_id = prtd->audio_client->session;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd->cmd_ack = 1;
		prtd->device_events = AUDDEV_EVT_DEV_RDY |
				AUDDEV_EVT_STREAM_VOL_CHG |
				AUDDEV_EVT_DEV_RLS;
		prtd->source = msm_snddev_route_dec(prtd->session_id);
		pr_debug("Register device event listener for"
				"SNDRV_PCM_STREAM_PLAYBACK session %d\n",
				substream->number);
		ret = auddev_register_evt_listner(prtd->device_events,
				AUDDEV_CLNT_DEC, substream->number,
				alsa_out_listener, (void *) prtd);
		if (ret)
			pr_debug("failed to register device event listener\n");
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		prtd->device_events = AUDDEV_EVT_DEV_RDY | AUDDEV_EVT_DEV_RLS |
				AUDDEV_EVT_FREQ_CHG;
		prtd->source = msm_snddev_route_enc(prtd->session_id);
		pr_debug("Register device event listener for"
				"SNDRV_PCM_STREAM_CAPTURE session %d\n",
				substream->number);
		ret = auddev_register_evt_listner(prtd->device_events,
				AUDDEV_CLNT_ENC, substream->number,
				alsa_in_listener, (void *) prtd);
		if (ret)
			pr_debug("failed to register device event listener\n");
	}
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
		pr_debug("%s: wait_event_timeout failed\n", __func__);
		goto fail;
	}

	if (!atomic_read(&prtd->out_count)) {
		pr_debug("%s: pcm stopped out_count 0\n", __func__);
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
			ret = q6asm_write_nolock(prtd->audio_client, xfer,
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

	pr_debug("%s\n", __func__);
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC,
		substream->number);
	pr_debug("%s\n", __func__);
	msm_clear_session_id(prtd->session_id);
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
		ret = q6asm_read_nolock(prtd->audio_client);
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
	struct msm_audio *prtd = runtime->private_data;
	int dir = OUT;

	pr_debug("%s\n", __func__);
	q6asm_cmd(prtd->audio_client, CMD_CLOSE);
	q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);
	auddev_unregister_evt_listner(AUDDEV_CLNT_ENC,
		substream->number);
	msm_clear_session_id(prtd->session_id);
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
	pr_debug("%s: pcm_irq_pos = %d\n", __func__, prtd->pcm_irq_pos);
	return bytes_to_frames(runtime, (prtd->pcm_irq_pos));
}

int msm_pcm_mmap(struct snd_pcm_substream *substream,
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

int msm_pcm_hw_params(struct snd_pcm_substream *substream,
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

static int msm_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;

	ret = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_PLAYBACK, 2);
	if (ret)
		return ret;
	ret = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_CAPTURE, 1);
	if (ret)
		return ret;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &msm_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &msm_pcm_ops);

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	return ret;
}

struct snd_soc_platform_driver msm_soc_platform = {
	.ops            = &msm_pcm_ops,
	.pcm_new	= msm_pcm_new,
};
EXPORT_SYMBOL(msm_soc_platform);

static __devinit int msm_pcm_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
				&msm_soc_platform);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver msm_pcm_driver = {
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
	.driver = {
		.name = "msm-dsp-audio",
		.owner = THIS_MODULE,
	},
};

static int __init msm_soc_platform_init(void)
{
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

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
#include <sound/q6audio-v2.h>
#include <sound/timer.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>
#include <linux/msm_audio_ion.h>

#include <linux/of_device.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>

#include "msm-pcm-q6-v2.h"
#include "msm-pcm-routing-v2.h"

enum stream_state {
	IDLE = 0,
	STOPPED,
	RUNNING,
};

static struct audio_locks the_locks;

#define PCM_MASTER_VOL_MAX_STEPS	0x2000
static const DECLARE_TLV_DB_LINEAR(msm_pcm_vol_gain, 0,
			PCM_MASTER_VOL_MAX_STEPS);

struct snd_msm {
	struct snd_card *card;
	struct snd_pcm *pcm;
};

#define CMD_EOS_MIN_TIMEOUT_LENGTH  50
#define CMD_EOS_TIMEOUT_MULTIPLIER  (HZ * 50)

static struct snd_pcm_hardware msm_pcm_hardware_capture = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =              (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE),
	.rates =                SNDRV_PCM_RATE_8000_48000,
	.rate_min =             8000,
	.rate_max =             48000,
	.channels_min =         1,
	.channels_max =         4,
	.buffer_bytes_max =     CAPTURE_MAX_NUM_PERIODS *
				CAPTURE_MAX_PERIOD_SIZE,
	.period_bytes_min =	CAPTURE_MIN_PERIOD_SIZE,
	.period_bytes_max =     CAPTURE_MAX_PERIOD_SIZE,
	.periods_min =          CAPTURE_MIN_NUM_PERIODS,
	.periods_max =          CAPTURE_MAX_NUM_PERIODS,
	.fifo_size =            0,
};

static struct snd_pcm_hardware msm_pcm_hardware_playback = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =              (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE),
	.rates =                SNDRV_PCM_RATE_8000_192000,
	.rate_min =             8000,
	.rate_max =             192000,
	.channels_min =         1,
	.channels_max =         8,
	.buffer_bytes_max =     PLAYBACK_MAX_NUM_PERIODS *
				PLAYBACK_MAX_PERIOD_SIZE,
	.period_bytes_min =	PLAYBACK_MIN_PERIOD_SIZE,
	.period_bytes_max =     PLAYBACK_MAX_PERIOD_SIZE,
	.periods_min =          PLAYBACK_MIN_NUM_PERIODS,
	.periods_max =          PLAYBACK_MAX_NUM_PERIODS,
	.fifo_size =            0,
};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
	88200, 96000, 176400, 192000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static void msm_pcm_route_event_handler(enum msm_pcm_routing_event event,
					void *priv_data)
{
	struct msm_audio *prtd = priv_data;

	BUG_ON(!prtd);

	pr_debug("%s: event %x\n", __func__, event);

	switch (event) {
	case MSM_PCM_RT_EVT_BUF_RECFG:
		q6asm_cmd(prtd->audio_client, CMD_PAUSE);
		q6asm_cmd(prtd->audio_client, CMD_FLUSH);
		q6asm_run(prtd->audio_client, 0, 0, 0);
	default:
		break;
	}
}

static void event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	struct msm_audio *prtd = priv;
	struct snd_pcm_substream *substream = prtd->substream;
	uint32_t *ptrmem = (uint32_t *)payload;
	uint32_t idx = 0;
	uint32_t size = 0;

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
		if (!prtd->mmap_flag || prtd->reset_event)
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
		clear_bit(CMD_EOS, &prtd->cmd_pending);
		wake_up(&the_locks.eos_wait);
		break;
	case ASM_DATA_EVENT_READ_DONE_V2: {
		pr_debug("ASM_DATA_EVENT_READ_DONE_V2\n");
		pr_debug("token = 0x%08x\n", token);
		prtd->in_frame_info[token].size = payload[4];
		prtd->in_frame_info[token].offset = payload[5];
		/* assume data size = 0 during flushing */
		if (prtd->in_frame_info[token].size) {
			prtd->pcm_irq_pos += prtd->in_frame_info[token].size;
			pr_debug("pcm_irq_pos=%d\n", prtd->pcm_irq_pos);
			if (atomic_read(&prtd->start))
				snd_pcm_period_elapsed(substream);
			if (atomic_read(&prtd->in_count) <= prtd->periods)
				atomic_inc(&prtd->in_count);
			wake_up(&the_locks.read_wait);
			if (prtd->mmap_flag &&
			    q6asm_is_cpu_buf_avail_nolock(OUT,
				prtd->audio_client,
				&size, &idx) &&
			    (substream->runtime->status->state ==
			     SNDRV_PCM_STATE_RUNNING))
				q6asm_read_nolock(prtd->audio_client);
		} else {
			pr_debug("%s: reclaim flushed buf in_count %x\n",
				__func__, atomic_read(&prtd->in_count));
			prtd->pcm_irq_pos += prtd->pcm_count;
			if (prtd->mmap_flag) {
				if (q6asm_is_cpu_buf_avail_nolock(OUT,
				    prtd->audio_client,
				    &size, &idx) &&
				    (substream->runtime->status->state ==
				    SNDRV_PCM_STATE_RUNNING))
					q6asm_read_nolock(prtd->audio_client);
			} else {
				atomic_inc(&prtd->in_count);
			}
			if (atomic_read(&prtd->in_count) == prtd->periods) {
				pr_info("%s: reclaimed all bufs\n", __func__);
				if (atomic_read(&prtd->start))
					snd_pcm_period_elapsed(substream);
				wake_up(&the_locks.read_wait);
			}
		}
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
				pr_debug("%s:writing %d bytes of buffer to dsp\n",
					__func__,
					prtd->pcm_count);
				q6asm_write_nolock(prtd->audio_client,
					prtd->pcm_count,
					0, 0, NO_TIMESTAMP);
			} else {
				while (atomic_read(&prtd->out_needed)) {
					pr_debug("%s:writing %d bytes of buffer to dsp\n",
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
	case RESET_EVENTS:
		pr_debug("%s RESET_EVENTS\n", __func__);
		prtd->pcm_irq_pos += prtd->pcm_count;
		atomic_inc(&prtd->out_count);
		atomic_inc(&prtd->in_count);
		prtd->reset_event = true;
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		wake_up(&the_locks.eos_wait);
		wake_up(&the_locks.write_wait);
		wake_up(&the_locks.read_wait);
		break;
	default:
		pr_debug("Not Supported Event opcode[0x%x]\n", opcode);
		break;
	}
}

static int msm_pcm_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	struct msm_plat_data *pdata;
	struct snd_pcm_hw_params *params;
	int ret;
	uint16_t bits_per_sample = 16;

	pdata = (struct msm_plat_data *)
		dev_get_drvdata(soc_prtd->platform->dev);
	if (!pdata) {
		pr_err("%s: platform data not populated\n", __func__);
		return -EINVAL;
	}
	if (!prtd || !prtd->audio_client) {
		pr_err("%s: private data null or audio client freed\n",
			__func__);
		return -EINVAL;
	}
	params = &soc_prtd->dpcm[substream->stream].hw_params;

	pr_debug("%s\n", __func__);
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;
	if (prtd->enabled)
		return 0;

	prtd->audio_client->perf_mode = pdata->perf_mode;
	pr_debug("%s: perf: %x\n", __func__, pdata->perf_mode);

	if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE)
		bits_per_sample = 24;

	ret = q6asm_open_write_v2(prtd->audio_client,
			FORMAT_LINEAR_PCM, bits_per_sample);
	if (ret < 0) {
		pr_err("%s: q6asm_open_write_v2 failed\n", __func__);
		q6asm_audio_client_free(prtd->audio_client);
		prtd->audio_client = NULL;
		return -ENOMEM;
	}

	ret = q6asm_send_cal(prtd->audio_client);
	if (ret < 0)
		pr_debug("%s : Send cal failed : %d", __func__, ret);

	pr_debug("%s: session ID %d\n", __func__,
			prtd->audio_client->session);
	prtd->session_id = prtd->audio_client->session;
	ret = msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->be_id,
			prtd->audio_client->perf_mode,
			prtd->session_id, substream->stream);
	if (ret) {
		pr_err("%s: stream reg failed ret:%d\n", __func__, ret);
		return ret;
	}

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		bits_per_sample = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		bits_per_sample = 24;
		break;
	}

	ret = q6asm_media_format_block_multi_ch_pcm_v2(
			prtd->audio_client, runtime->rate,
			runtime->channels, !prtd->set_channel_map,
			prtd->channel_map, bits_per_sample);
	if (ret < 0)
		pr_info("%s: CMD Format block failed\n", __func__);

	atomic_set(&prtd->out_count, runtime->periods);

	prtd->enabled = 1;
	prtd->cmd_pending = 0;
	prtd->cmd_interrupt = 0;

	return 0;
}

static int msm_pcm_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_plat_data *pdata;
	struct snd_pcm_hw_params *params;
	struct msm_pcm_routing_evt event;
	int ret = 0;
	int i = 0;
	uint16_t bits_per_sample = 16;

	pdata = (struct msm_plat_data *)
		dev_get_drvdata(soc_prtd->platform->dev);
	if (!pdata) {
		pr_err("%s: platform data not populated\n", __func__);
		return -EINVAL;
	}
	if (!prtd || !prtd->audio_client) {
		pr_err("%s: private data null or audio client freed\n",
			__func__);
		return -EINVAL;
	}

	if (prtd->enabled == IDLE) {
		pr_debug("%s:perf_mode=%d periods=%d\n", __func__,
			pdata->perf_mode, runtime->periods);
		params = &soc_prtd->dpcm[substream->stream].hw_params;
		if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE)
			bits_per_sample = 24;

		/* ULL mode is not supported in capture path */
		if (pdata->perf_mode == LEGACY_PCM_MODE)
			prtd->audio_client->perf_mode = LEGACY_PCM_MODE;
		else
			prtd->audio_client->perf_mode = LOW_LATENCY_PCM_MODE;

		pr_debug("%s Opening %d-ch PCM read stream, perf_mode %d\n",
				__func__, params_channels(params),
				prtd->audio_client->perf_mode);

		ret = q6asm_open_read_v2(prtd->audio_client, FORMAT_LINEAR_PCM,
				bits_per_sample);
		if (ret < 0) {
			pr_err("%s: q6asm_open_read failed\n", __func__);
			q6asm_audio_client_free(prtd->audio_client);
			prtd->audio_client = NULL;
			return -ENOMEM;
		}

		ret = q6asm_send_cal(prtd->audio_client);
		if (ret < 0)
			pr_debug("%s : Send cal failed : %d", __func__, ret);

		pr_debug("%s: session ID %d\n",
				__func__, prtd->audio_client->session);
		prtd->session_id = prtd->audio_client->session;
		event.event_func = msm_pcm_route_event_handler;
		event.priv_data = (void *) prtd;
		ret = msm_pcm_routing_reg_phy_stream_v2(
				soc_prtd->dai_link->be_id,
				prtd->audio_client->perf_mode,
				prtd->session_id, substream->stream,
				event);
		if (ret) {
			pr_err("%s: stream reg failed ret:%d\n", __func__, ret);
			return ret;
		}
	}

	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;

	if (prtd->enabled == IDLE || prtd->enabled == STOPPED) {
		for (i = 0; i < runtime->periods; i++)
			q6asm_read(prtd->audio_client);
		prtd->periods = runtime->periods;
	}

	if (prtd->enabled != IDLE)
		return 0;

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		bits_per_sample = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		bits_per_sample = 24;
		break;
	}
	pr_debug("Samp_rate = %d\n", prtd->samp_rate);
	pr_debug("Channel = %d\n", prtd->channel_mode);
	ret = q6asm_enc_cfg_blk_pcm_format_support(prtd->audio_client,
					prtd->samp_rate, prtd->channel_mode,
					bits_per_sample);
	if (ret < 0)
		pr_debug("%s: cmd cfg pcm was block failed", __func__);

	prtd->enabled = RUNNING;

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
		ret = q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("SNDRV_PCM_TRIGGER_STOP\n");
		atomic_set(&prtd->start, 0);
		if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
			prtd->enabled = STOPPED;
			ret = q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
			break;
		}
		/* pending CMD_EOS isn't expected */
		WARN_ON_ONCE(test_bit(CMD_EOS, &prtd->cmd_pending));
		set_bit(CMD_EOS, &prtd->cmd_pending);
		ret = q6asm_cmd_nowait(prtd->audio_client, CMD_EOS);
		if (ret)
			clear_bit(CMD_EOS, &prtd->cmd_pending);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("SNDRV_PCM_TRIGGER_PAUSE\n");
		ret = q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
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

	prtd->audio_client->dev = soc_prtd->platform->dev;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		runtime->hw = msm_pcm_hardware_playback;

	/* Capture path */
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		runtime->hw = msm_pcm_hardware_capture;
	else {
		pr_err("Invalid Stream type %d\n", substream->stream);
		return -EINVAL;
	}

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

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_pcm_hw_constraint_minmax(runtime,
			SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
			PLAYBACK_MIN_NUM_PERIODS * PLAYBACK_MIN_PERIOD_SIZE,
			PLAYBACK_MAX_NUM_PERIODS * PLAYBACK_MAX_PERIOD_SIZE);
		if (ret < 0) {
			pr_err("constraint for buffer bytes min max ret = %d\n",
									ret);
		}
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		ret = snd_pcm_hw_constraint_minmax(runtime,
			SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
			CAPTURE_MIN_NUM_PERIODS * CAPTURE_MIN_PERIOD_SIZE,
			CAPTURE_MAX_NUM_PERIODS * CAPTURE_MAX_PERIOD_SIZE);
		if (ret < 0) {
			pr_err("constraint for buffer bytes min max ret = %d\n",
									ret);
		}
	}
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	if (ret < 0) {
		pr_err("constraint for period bytes step ret = %d\n",
								ret);
	}
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	if (ret < 0) {
		pr_err("constraint for buffer bytes step ret = %d\n",
								ret);
	}

	prtd->enabled = IDLE;
	prtd->dsp_cnt = 0;
	prtd->set_channel_map = false;
	prtd->reset_event = false;
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

	while (fbytes > 0) {
		if (prtd->reset_event) {
			pr_err("%s: In SSR return ENETRESET before wait\n",
				__func__);
			return -ENETRESET;
		}

		ret = wait_event_timeout(the_locks.write_wait,
				(atomic_read(&prtd->out_count)), 5 * HZ);
		if (!ret) {
			pr_err("%s: wait_event_timeout failed\n", __func__);
			ret = -ETIMEDOUT;
			goto fail;
		}
		ret = 0;

		if (prtd->reset_event) {
			pr_err("%s: In SSR return ENETRESET after wait\n",
				__func__);
			return -ENETRESET;
		}

		if (!atomic_read(&prtd->out_count)) {
			pr_err("%s: pcm stopped out_count 0\n", __func__);
			return 0;
		}

		data = q6asm_is_cpu_buf_avail(IN, prtd->audio_client, &size,
			&idx);
		if (fbytes > size)
			xfer = size;
		else
			xfer = fbytes;

		bufptr = data;
		if (bufptr) {
			pr_debug("%s:fbytes =%d: xfer=%d size=%d\n",
						__func__, fbytes, xfer, size);
			if (copy_from_user(bufptr, buf, xfer)) {
				ret = -EFAULT;
				goto fail;
			}
			buf += xfer;
			fbytes -= xfer;
			pr_debug("%s:fbytes = %d: xfer=%d\n", __func__, fbytes,
				xfer);
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
	}
fail:
	return  ret;
}

static int msm_pcm_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	uint32_t timeout;
	int dir = 0;
	int ret = 0;

	pr_debug("%s: cmd_pending 0x%lx\n", __func__, prtd->cmd_pending);

	if (prtd->audio_client) {
		dir = IN;

		/* determine timeout length */
		if (runtime->frame_bits == 0 || runtime->rate == 0) {
			timeout = CMD_EOS_MIN_TIMEOUT_LENGTH;
		} else {
			timeout = (runtime->period_size *
					CMD_EOS_TIMEOUT_MULTIPLIER) /
					((runtime->frame_bits / 8) *
					 runtime->rate);
			if (timeout < CMD_EOS_MIN_TIMEOUT_LENGTH)
				timeout = CMD_EOS_MIN_TIMEOUT_LENGTH;
		}
		pr_debug("%s: CMD_EOS timeout is %d\n", __func__, timeout);

		ret = wait_event_timeout(the_locks.eos_wait,
					 !test_bit(CMD_EOS, &prtd->cmd_pending),
					 timeout);
		if (!ret)
			pr_err("%s: CMD_EOS failed, cmd_pending 0x%lx\n",
			       __func__, prtd->cmd_pending);
		q6asm_cmd(prtd->audio_client, CMD_CLOSE);
		q6asm_audio_client_buf_free_contiguous(dir,
					prtd->audio_client);
		q6asm_audio_client_free(prtd->audio_client);
	}
	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->be_id,
						SNDRV_PCM_STREAM_PLAYBACK);
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

	if (prtd->reset_event) {
		pr_err("%s: In SSR return ENETRESET before wait\n", __func__);
		return -ENETRESET;
	}
	ret = wait_event_timeout(the_locks.read_wait,
			(atomic_read(&prtd->in_count)), 5 * HZ);
	if (!ret) {
		pr_debug("%s: wait_event_timeout failed\n", __func__);
		goto fail;
	}
	if (prtd->reset_event) {
		pr_err("%s: In SSR return ENETRESET after wait\n", __func__);
		return -ENETRESET;
	}
	if (!atomic_read(&prtd->in_count)) {
		pr_debug("%s: pcm stopped in_count 0\n", __func__);
		return 0;
	}
	pr_debug("Checking if valid buffer is available...%p\n",
						data);
	data = q6asm_is_cpu_buf_avail(OUT, prtd->audio_client, &size, &idx);
	bufptr = data;
	pr_debug("Size = %d\n", size);
	pr_debug("fbytes = %d\n", fbytes);
	pr_debug("idx = %d\n", idx);
	if (bufptr) {
		xfer = fbytes;
		if (xfer > size)
			xfer = size;
		offset = prtd->in_frame_info[idx].offset;
		pr_debug("Offset value = %d\n", offset);
		if (copy_to_user(buf, bufptr+offset, xfer)) {
			pr_err("Failed to copy buf to user\n");
			ret = -EFAULT;
			goto fail;
		}
		fbytes -= xfer;
		size -= xfer;
		prtd->in_frame_info[idx].offset += xfer;
		pr_debug("%s:fbytes = %d: size=%d: xfer=%d\n",
					__func__, fbytes, size, xfer);
		pr_debug(" Sending next buffer to dsp\n");
		memset(&prtd->in_frame_info[idx], 0,
		       sizeof(struct msm_audio_in_frame_info));
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
	if (prtd->audio_client) {
		q6asm_cmd(prtd->audio_client, CMD_CLOSE);
		q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);
		q6asm_audio_client_free(prtd->audio_client);
	}

	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->be_id,
		SNDRV_PCM_STREAM_CAPTURE);
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
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct audio_client *ac = prtd->audio_client;
	struct audio_port_data *apd = ac->port;
	struct audio_buffer *ab;
	int dir = -1;

	prtd->mmap_flag = 1;

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
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct audio_buffer *buf;
	int dir, ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		dir = OUT;
	ret = q6asm_audio_client_buf_alloc_contiguous(dir,
			prtd->audio_client,
			(params_buffer_bytes(params) / params_periods(params)),
			 params_periods(params));
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed rc = %d\n",
							ret);
		return -ENOMEM;
	}
	buf = prtd->audio_client->port[dir].buf;
	if (buf == NULL || buf[0].data == NULL)
		return -ENOMEM;

	pr_debug("%s:buf = %p\n", __func__, buf);
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->area = buf[0].data;
	dma_buf->addr =  buf[0].phys;
	dma_buf->bytes = params_buffer_bytes(params);
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

static int msm_pcm_set_volume(struct msm_audio *prtd, uint32_t volume)
{
	int rc = 0;

	if (prtd && prtd->audio_client) {
		pr_debug("%s: channels %d volume 0x%x\n", __func__,
				prtd->channel_mode, volume);
		rc = q6asm_set_volume(prtd->audio_client, volume);
		if (rc < 0) {
			pr_err("%s: Send Volume command failed rc=%d\n",
					__func__, rc);
		}
	}
	return rc;
}

static int msm_pcm_volume_ctl_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct snd_pcm_substream *substream =
		vol->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct msm_audio *prtd;

	pr_debug("%s\n", __func__);
	if (!substream) {
		pr_err("%s substream not found\n", __func__);
		return -ENODEV;
	}
	if (!substream->runtime) {
		pr_err("%s substream runtime not found\n", __func__);
		return 0;
	}
	prtd = substream->runtime->private_data;
	if (prtd)
		ucontrol->value.integer.value[0] = prtd->volume;
	return 0;
}

static int msm_pcm_volume_ctl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct snd_pcm_substream *substream =
		vol->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct msm_audio *prtd;
	int volume = ucontrol->value.integer.value[0];

	pr_debug("%s: volume : 0x%x\n", __func__, volume);
	if (!substream) {
		pr_err("%s substream not found\n", __func__);
		return -ENODEV;
	}
	if (!substream->runtime) {
		pr_err("%s substream runtime not found\n", __func__);
		return 0;
	}
	prtd = substream->runtime->private_data;
	if (prtd) {
		rc = msm_pcm_set_volume(prtd, volume);
		prtd->volume = volume;
	}
	return rc;
}

static int msm_pcm_add_volume_control(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_volume *volume_info;
	struct snd_kcontrol *kctl;

	dev_dbg(rtd->dev, "%s, Volume control add\n", __func__);
	ret = snd_pcm_add_volume_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
			NULL, 1, rtd->dai_link->be_id,
			&volume_info);
	if (ret < 0) {
		pr_err("%s volume control failed ret %d\n", __func__, ret);
		return ret;
	}
	kctl = volume_info->kctl;
	kctl->put = msm_pcm_volume_ctl_put;
	kctl->get = msm_pcm_volume_ctl_get;
	kctl->tlv.p = msm_pcm_vol_gain;
	return 0;
}

static int msm_pcm_chmap_ctl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int i;
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	unsigned int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	struct snd_pcm_substream *substream;
	struct msm_audio *prtd;

	pr_debug("%s", __func__);
	substream = snd_pcm_chmap_substream(info, idx);
	if (!substream)
		return -ENODEV;
	if (!substream->runtime)
		return 0;

	prtd = substream->runtime->private_data;
	if (prtd) {
		prtd->set_channel_map = true;
			for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
				prtd->channel_map[i] =
				(char)(ucontrol->value.integer.value[i]);
	}
	return 0;
}

static int msm_pcm_chmap_ctl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int i;
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	unsigned int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	struct snd_pcm_substream *substream;
	struct msm_audio *prtd;

	pr_debug("%s", __func__);
	substream = snd_pcm_chmap_substream(info, idx);
	if (!substream)
		return -ENODEV;
	memset(ucontrol->value.integer.value, 0,
		sizeof(ucontrol->value.integer.value));
	if (!substream->runtime)
		return 0; /* no channels set */

	prtd = substream->runtime->private_data;

	if (prtd && prtd->set_channel_map == true) {
		for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
			ucontrol->value.integer.value[i] =
					(int)prtd->channel_map[i];
	} else {
		for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
			ucontrol->value.integer.value[i] = 0;
	}

	return 0;
}

static int msm_pcm_add_chmap_controls(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_chmap *chmap_info;
	struct snd_kcontrol *kctl;
	char device_num[12];
	int i, ret = 0;

	pr_debug("%s, Channel map cntrl add\n", __func__);
	ret = snd_pcm_add_chmap_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
				     snd_pcm_std_chmaps,
				     PCM_FORMAT_MAX_NUM_CHANNEL, 0,
				     &chmap_info);
	if (ret < 0) {
		pr_err("%s, channel map cntrl add failed\n", __func__);
		return ret;
	}
	kctl = chmap_info->kctl;
	for (i = 0; i < kctl->count; i++)
		kctl->vd[i].access |= SNDRV_CTL_ELEM_ACCESS_WRITE;
	snprintf(device_num, sizeof(device_num), "%d", pcm->device);
	strlcat(kctl->id.name, device_num, sizeof(kctl->id.name));
	pr_debug("%s, Overwriting channel map control name to: %s\n",
		__func__, kctl->id.name);
	kctl->put = msm_pcm_chmap_ctl_put;
	kctl->get = msm_pcm_chmap_ctl_get;
	return 0;
}

static int msm_pcm_app_type_cfg_ctl_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int app_type;
	int acdb_dev_id;
	int sample_rate = 48000;

	pr_debug("%s: fe_id- %llu\n", __func__, fe_id);
	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds fe_id %llu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	app_type = ucontrol->value.integer.value[0];
	acdb_dev_id = ucontrol->value.integer.value[1];
	if (0 != ucontrol->value.integer.value[2])
		sample_rate = ucontrol->value.integer.value[2];
	pr_debug("%s: app_type- %d acdb_dev_id- %d sample_rate- %d\n",
		__func__, app_type, acdb_dev_id, sample_rate);
	msm_pcm_routing_reg_stream_app_type_cfg(fe_id, app_type,
						acdb_dev_id, sample_rate);

	return 0;
}

static int msm_pcm_app_type_cfg_ctl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int ret = 0;
	int app_type;
	int acdb_dev_id;
	int sample_rate;

	pr_debug("%s: fe_id- %llu\n", __func__, fe_id);
	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds fe_id %llu\n",
			__func__, fe_id);
		ret = -EINVAL;
		goto done;
	}

	ret = msm_pcm_routing_get_stream_app_type_cfg(fe_id, &app_type,
		&acdb_dev_id, &sample_rate);
	if (ret < 0) {
		pr_err("%s: msm_pcm_routing_get_stream_app_type_cfg failed returned %d\n",
			__func__, ret);
		goto done;
	}

	ucontrol->value.integer.value[0] = app_type;
	ucontrol->value.integer.value[1] = acdb_dev_id;
	ucontrol->value.integer.value[2] = sample_rate;
	pr_debug("%s: fedai_id %llu, app_type %d, acdb_dev_id %d, sample_rate %d\n",
		__func__, fe_id, app_type, acdb_dev_id, sample_rate);
done:
	return ret;
}

static int msm_pcm_add_app_type_controls(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_usr *app_type_info;
	struct snd_kcontrol *kctl;
	const char *mixer_ctl_name	= "Audio Stream";
	const char *deviceNo		= "NN";
	const char *suffix		= "App Type Cfg";
	int ctl_len, ret = 0;

	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1 +
		  strlen(suffix) + 1;
	pr_debug("%s, App type cntrl add\n", __func__);
	ret = snd_pcm_add_usr_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
				   NULL, 1, ctl_len, rtd->dai_link->be_id,
				   &app_type_info);
	if (ret < 0) {
		pr_err("%s, app type cntrl add failed:%d\n", __func__, ret);
		return ret;
	}
	kctl = app_type_info->kctl;
	snprintf(kctl->id.name, ctl_len, "%s %d %s", mixer_ctl_name,
		 rtd->pcm->device, suffix);
	kctl = app_type_info->kctl;
	kctl->put = msm_pcm_app_type_cfg_ctl_put;
	kctl->get = msm_pcm_app_type_cfg_ctl_get;

	return 0;
}

static int msm_pcm_add_controls(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	pr_debug("%s\n", __func__);
	ret = msm_pcm_add_chmap_controls(rtd);
	if (ret)
		pr_err("%s: pcm add controls failed:%d\n", __func__, ret);
	ret = msm_pcm_add_app_type_controls(rtd);
	if (ret)
		pr_err("%s: pcm add app type controls failed:%d\n",
			__func__, ret);
	return ret;
}

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = msm_pcm_add_controls(rtd);
	if (ret) {
		pr_err("%s, kctl add failed:%d\n", __func__, ret);
		return ret;
	}

	ret = msm_pcm_add_volume_control(rtd);
	if (ret)
		pr_err("%s: Could not add pcm Volume Control %d\n",
			__func__, ret);

	return ret;
}

static snd_pcm_sframes_t msm_pcm_delay_blk(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct audio_client *ac = prtd->audio_client;
	snd_pcm_sframes_t frames;
	int ret;

	ret = q6asm_get_path_delay(prtd->audio_client);
	if (ret) {
		pr_err("%s: get_path_delay failed, ret=%d\n", __func__, ret);
		return 0;
	}

	/* convert microseconds to frames */
	frames = ac->path_delay / 1000 * runtime->rate / 1000;

	/* also convert the remainder from the initial division */
	frames += ac->path_delay % 1000 * runtime->rate / 1000000;

	/* overcompensate for the loss of precision (empirical) */
	frames += 2;

	return frames;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_pcm_ops,
	.pcm_new	= msm_asoc_pcm_new,
	.delay_blk      = msm_pcm_delay_blk,
};

static int msm_pcm_probe(struct platform_device *pdev)
{
	int rc;
	int id;
	struct msm_plat_data *pdata;
	const char *latency_level;

	rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,msm-pcm-dsp-id", &id);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-pcm-dsp-id missing in DT node\n",
					__func__);
		return rc;
	}

	pdata = kzalloc(sizeof(struct msm_plat_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		return -ENOMEM;
	}

	if (of_property_read_bool(pdev->dev.of_node,
				"qcom,msm-pcm-low-latency")) {

		pdata->perf_mode = LOW_LATENCY_PCM_MODE;
		rc = of_property_read_string(pdev->dev.of_node,
			"qcom,latency-level", &latency_level);
		if (!rc) {
			if (!strcmp(latency_level, "ultra"))
				pdata->perf_mode = ULTRA_LOW_LATENCY_PCM_MODE;
			else if (!strcmp(latency_level, "ull-pp"))
				pdata->perf_mode =
					ULL_POST_PROCESSING_PCM_MODE;
		}
	}
	else
		pdata->perf_mode = LEGACY_PCM_MODE;

	dev_set_drvdata(&pdev->dev, pdata);


	dev_dbg(&pdev->dev, "%s: dev name %s\n",
				__func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
				   &msm_soc_platform);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	struct msm_plat_data *pdata;

	pdata = dev_get_drvdata(&pdev->dev);
	kfree(pdata);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}
static const struct of_device_id msm_pcm_dt_match[] = {
	{.compatible = "qcom,msm-pcm-dsp"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_pcm_dt_match);

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-pcm-dsp",
		.owner = THIS_MODULE,
		.of_match_table = msm_pcm_dt_match,
	},
	.probe = msm_pcm_probe,
	.remove = msm_pcm_remove,
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

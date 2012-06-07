/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org.
 */


#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>
#include <linux/android_pmem.h>
#include <linux/slab.h>
#include "msm7kv2-pcm.h"
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/debug_mm.h>

#define HOSTPCM_STREAM_ID 5

struct snd_msm {
	struct snd_card *card;
	struct snd_pcm *pcm;
};

int copy_count;

static struct snd_pcm_hardware msm_pcm_playback_hardware = {
	.info =                 SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED,
	.formats =              USE_FORMATS,
	.rates =                USE_RATE,
	.rate_min =             USE_RATE_MIN,
	.rate_max =             48000,
	.channels_min =         1,
	.channels_max =         2,
	.buffer_bytes_max =     MAX_BUFFER_PLAYBACK_SIZE,
	.period_bytes_min =     BUFSZ,
	.period_bytes_max =     BUFSZ,
	.periods_min =          2,
	.periods_max =          2,
	.fifo_size =            0,
};

static struct snd_pcm_hardware msm_pcm_capture_hardware = {
	.info =                 SNDRV_PCM_INFO_INTERLEAVED,
	.formats =              USE_FORMATS,
	.rates =                USE_RATE,
	.rate_min =             8000,
	.rate_max =             48000,
	.channels_min =         1,
	.channels_max =         2,
	.buffer_bytes_max =     MAX_BUFFER_CAPTURE_SIZE,
	.period_bytes_min =	4096,
	.period_bytes_max =     4096,
	.periods_min =          4,
	.periods_max =          4,
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
static void alsa_out_listener(u32 evt_id, union auddev_evt_data *evt_payload,
							void *private_data)
{
	struct msm_audio *prtd = (struct msm_audio *) private_data;
	MM_DBG("evt_id = 0x%8x\n", evt_id);
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		MM_DBG("AUDDEV_EVT_DEV_RDY\n");
		prtd->source |= (0x1 << evt_payload->routing_id);
		if (prtd->running == 1 && prtd->enabled == 1)
			audpp_route_stream(prtd->session_id, prtd->source);
		break;
	case AUDDEV_EVT_DEV_RLS:
		MM_DBG("AUDDEV_EVT_DEV_RLS\n");
		prtd->source &= ~(0x1 << evt_payload->routing_id);
		if (prtd->running == 1 && prtd->enabled == 1)
			audpp_route_stream(prtd->session_id, prtd->source);
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		prtd->vol_pan.volume = evt_payload->session_vol;
		MM_DBG("AUDDEV_EVT_STREAM_VOL_CHG, stream vol %d\n",
				prtd->vol_pan.volume);
		if (prtd->running)
			audpp_set_volume_and_pan(prtd->session_id,
					prtd->vol_pan.volume,
					0, POPP);
		break;
	default:
		MM_DBG("Unknown Event\n");
		break;
	}
}

static void alsa_in_listener(u32 evt_id, union auddev_evt_data *evt_payload,
							void *private_data)
{
	struct msm_audio *prtd = (struct msm_audio *) private_data;
	MM_DBG("evt_id = 0x%8x\n", evt_id);

	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY: {
		MM_DBG("AUDDEV_EVT_DEV_RDY\n");
		prtd->source |= (0x1 << evt_payload->routing_id);

		if ((prtd->running == 1) && (prtd->enabled == 1))
			alsa_in_record_config(prtd, 1);

		break;
	}
	case AUDDEV_EVT_DEV_RLS: {
		MM_DBG("AUDDEV_EVT_DEV_RLS\n");
		prtd->source &= ~(0x1 << evt_payload->routing_id);

		if (!prtd->running || !prtd->enabled)
			break;

		/* Turn off as per source */
		if (prtd->source)
			alsa_in_record_config(prtd, 1);
		else
			/* Turn off all */
			alsa_in_record_config(prtd, 0);

		break;
	}
	case AUDDEV_EVT_FREQ_CHG: {
		MM_DBG("Encoder Driver got sample rate change event\n");
		MM_DBG("sample rate %d\n", evt_payload->freq_info.sample_rate);
		MM_DBG("dev_type %d\n", evt_payload->freq_info.dev_type);
		MM_DBG("acdb_dev_id %d\n", evt_payload->freq_info.acdb_dev_id);
		if (prtd->running == 1) {
			/* Stop Recording sample rate does not match
			with device sample rate */
			if (evt_payload->freq_info.sample_rate !=
				prtd->samp_rate) {
				alsa_in_record_config(prtd, 0);
				prtd->abort = 1;
				wake_up(&the_locks.read_wait);
			}
		}
		break;
	}
	default:
		MM_DBG("Unknown Event\n");
		break;
	}
}

static void msm_pcm_enqueue_data(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	unsigned int period_size;

	MM_DBG("prtd->out_tail =%d mmap_flag=%d\n",
			prtd->out_tail, prtd->mmap_flag);
	period_size = snd_pcm_lib_period_bytes(substream);
	alsa_dsp_send_buffer(prtd, prtd->out_tail, period_size);
	prtd->out_tail ^= 1;
	++copy_count;
	prtd->period++;
	if (unlikely(prtd->period >= runtime->periods))
		prtd->period = 0;

}

static void event_handler(void *data)
{
	struct msm_audio *prtd = data;
	MM_DBG("\n");
	snd_pcm_period_elapsed(prtd->substream);
	if (prtd->mmap_flag) {
		if (prtd->dir == SNDRV_PCM_STREAM_CAPTURE)
			return;
		if (!prtd->stopped)
			msm_pcm_enqueue_data(prtd->substream);
		else
			prtd->out_needed++;
	}
}

static int msm_pcm_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	MM_DBG("\n");
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	prtd->pcm_buf_pos = 0;
	if (prtd->enabled)
		return 0;

	MM_DBG("\n");
	/* rate and channels are sent to audio driver */
	prtd->out_sample_rate = runtime->rate;
	prtd->out_channel_mode = runtime->channels;
	prtd->data = prtd->substream->dma_buffer.area;
	prtd->phys = prtd->substream->dma_buffer.addr;
	prtd->out[0].data = prtd->data + 0;
	prtd->out[0].addr = prtd->phys + 0;
	prtd->out[0].size = BUFSZ;
	prtd->out[1].data = prtd->data + BUFSZ;
	prtd->out[1].addr = prtd->phys + BUFSZ;
	prtd->out[1].size = BUFSZ;

	if (prtd->enabled | !(prtd->mmap_flag))
		return 0;

	prtd->out[0].used = prtd->pcm_count;
	prtd->out[1].used = prtd->pcm_count;

	mutex_lock(&the_locks.lock);
	alsa_audio_configure(prtd);
	mutex_unlock(&the_locks.lock);

	return 0;
}

static int msm_pcm_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	int ret = 0;
	uint32_t freq;
	MM_DBG("\n");
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	prtd->pcm_buf_pos = 0;

	/* rate and channels are sent to audio driver */
	prtd->type = ENC_TYPE_WAV;
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = (runtime->channels - 1);
	prtd->buffer_size = prtd->channel_mode ? STEREO_DATA_SIZE : \
							MONO_DATA_SIZE;

	if (prtd->enabled)
		return 0;

	freq = prtd->samp_rate;

	prtd->data = prtd->substream->dma_buffer.area;
	prtd->phys = prtd->substream->dma_buffer.addr;
	MM_DBG("prtd->data =%08x\n", (unsigned int)prtd->data);
	MM_DBG("prtd->phys =%08x\n", (unsigned int)prtd->phys);

	mutex_lock(&the_locks.lock);
	ret = alsa_audio_configure(prtd);
	mutex_unlock(&the_locks.lock);
	if (ret)
		return ret;
	ret = wait_event_interruptible(the_locks.enable_wait,
				prtd->running != 0);
	MM_DBG("state prtd->running = %d ret = %d\n", prtd->running, ret);

	if (prtd->running == 0)
		ret = -ENODEV;
	else
		ret = 0;
	prtd->enabled = 1;

	return ret;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	unsigned long flag = 0;
	int ret = 0;

	MM_DBG("\n");
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			|| !prtd->mmap_flag)
			break;
		if (!prtd->out_needed) {
			prtd->stopped = 0;
			break;
		}
		spin_lock_irqsave(&the_locks.write_dsp_lock, flag);
		if (prtd->running == 1) {
			if (prtd->stopped == 1) {
				prtd->stopped = 0;
				prtd->period = 0;
				if (prtd->pcm_irq_pos == 0) {
					prtd->out_tail = 0;
					msm_pcm_enqueue_data(prtd->substream);
					prtd->out_needed--;
				} else {
					prtd->out_tail = 1;
					msm_pcm_enqueue_data(prtd->substream);
					prtd->out_needed--;
				}
				if (prtd->out_needed) {
					prtd->out_tail ^= 1;
					msm_pcm_enqueue_data(prtd->substream);
					prtd->out_needed--;
				}
			}
		}
		spin_unlock_irqrestore(&the_locks.write_dsp_lock, flag);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			|| !prtd->mmap_flag)
			break;
		prtd->stopped = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

struct  msm_audio_event_callbacks snd_msm_audio_ops = {
	.playback = event_handler,
	.capture = event_handler,
};

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd;
	int ret = 0;
	int i = 0;
	int session_attrb, sessionid;

	MM_DBG("\n");
	prtd = kzalloc(sizeof(struct msm_audio), GFP_KERNEL);
	if (prtd == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (prtd->opened) {
			kfree(prtd);
			return -EBUSY;
		}
		runtime->hw = msm_pcm_playback_hardware;
		prtd->dir = SNDRV_PCM_STREAM_PLAYBACK;
		prtd->eos_ack = 0;
		prtd->session_id = HOSTPCM_STREAM_ID;
		prtd->device_events = AUDDEV_EVT_DEV_RDY |
				AUDDEV_EVT_STREAM_VOL_CHG |
				AUDDEV_EVT_DEV_RLS;
		prtd->source = msm_snddev_route_dec(prtd->session_id);
		MM_ERR("Register device event listener\n");
		ret = auddev_register_evt_listner(prtd->device_events,
				AUDDEV_CLNT_DEC, prtd->session_id,
				alsa_out_listener, (void *) prtd);
		if (ret) {
			MM_ERR("failed to register device event listener\n");
			goto evt_error;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		runtime->hw = msm_pcm_capture_hardware;
		prtd->dir = SNDRV_PCM_STREAM_CAPTURE;
		session_attrb = ENC_TYPE_WAV;
		sessionid = audpreproc_aenc_alloc(session_attrb,
				&prtd->module_name, &prtd->queue_id);
		if (sessionid < 0) {
			MM_ERR("AUDREC not available\n");
			kfree(prtd);
			return -ENODEV;
		}
		prtd->session_id = sessionid;
		MM_DBG("%s\n", prtd->module_name);
		ret = msm_adsp_get(prtd->module_name, &prtd->audrec,
				&alsa_audrec_adsp_ops, prtd);
		if (ret < 0) {
			audpreproc_aenc_free(prtd->session_id);
			kfree(prtd);
			return -ENODEV;
		}

		prtd->abort = 0;
		prtd->device_events = AUDDEV_EVT_DEV_RDY | AUDDEV_EVT_DEV_RLS |
				AUDDEV_EVT_FREQ_CHG;
		prtd->source = msm_snddev_route_enc(prtd->session_id);
		MM_ERR("Register device event listener\n");
		ret = auddev_register_evt_listner(prtd->device_events,
				AUDDEV_CLNT_ENC, prtd->session_id,
				alsa_in_listener, (void *) prtd);
		if (ret) {
			MM_ERR("failed to register device event listener\n");
			audpreproc_aenc_free(prtd->session_id);
			msm_adsp_put(prtd->audrec);
			goto evt_error;
		}
	}
	prtd->substream = substream;
	ret = snd_pcm_hw_constraint_list(runtime, 0,
						SNDRV_PCM_HW_PARAM_RATE,
						&constraints_sample_rates);
	if (ret < 0)
		MM_ERR("snd_pcm_hw_constraint_list failed\n");
	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		MM_ERR("snd_pcm_hw_constraint_integer failed\n");

	prtd->ops = &snd_msm_audio_ops;
	prtd->out[0].used = BUF_INVALID_LEN;
	prtd->out[1].used = 0;
	prtd->out_head = 1; /* point to second buffer on startup */
	prtd->out_tail = 0;
	prtd->dsp_cnt = 0;
	prtd->in_head = 0;
	prtd->in_tail = 0;
	prtd->in_count = 0;
	prtd->out_needed = 0;
	for (i = 0; i < FRAME_NUM; i++) {
		prtd->in[i].size = 0;
		prtd->in[i].read = 0;
	}
	prtd->vol_pan.volume = 0x2000;
	prtd->vol_pan.pan = 0x0;
	prtd->opened = 1;
	runtime->private_data = prtd;

	copy_count = 0;
	return 0;
evt_error:
	kfree(prtd);
	return ret;
}

static int msm_pcm_playback_copy(struct snd_pcm_substream *substream, int a,
	snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	int ret = 0;
	int fbytes = 0;

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	fbytes = frames_to_bytes(runtime, frames);
	MM_DBG("%d\n", fbytes);
	ret = alsa_send_buffer(prtd, buf, fbytes, NULL);
	++copy_count;
	prtd->pcm_buf_pos += fbytes;
	if (copy_count == 1) {
		mutex_lock(&the_locks.lock);
		ret = alsa_audio_configure(prtd);
		mutex_unlock(&the_locks.lock);
	}
	return  ret;
}

static int msm_pcm_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	int ret = 0;

	MM_DBG("\n");
	if ((!prtd->mmap_flag) && prtd->enabled) {
		ret = wait_event_interruptible(the_locks.eos_wait,
		(!(prtd->out[0].used) && !(prtd->out[1].used)));

		if (ret < 0)
			goto done;
	}

	/* PCM DMAMISS message is sent only once in
	 * hpcm interface. So, wait for buffer complete
	 * and teos flag.
	 */
	if (prtd->enabled)
		ret = wait_event_interruptible(the_locks.eos_wait,
					prtd->eos_ack);

done:
	alsa_audio_disable(prtd);
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, prtd->session_id);
	kfree(prtd);

	return 0;
}

static int msm_pcm_capture_copy(struct snd_pcm_substream *substream,
		 int channel, snd_pcm_uframes_t hwoff, void __user *buf,
						 snd_pcm_uframes_t frames)
{
	int ret = 0, rc1 = 0, rc2 = 0;
	int fbytes = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = substream->runtime->private_data;

	int monofbytes = 0;
	char *bufferp = NULL;

	if (prtd->abort)
		return -EPERM;

	fbytes = frames_to_bytes(runtime, frames);
	MM_DBG("%d\n", fbytes);
	monofbytes = fbytes / 2;
	if (runtime->channels == 2) {
		ret = alsa_buffer_read(prtd, buf, fbytes, NULL);
	} else {
		bufferp = buf;
		rc1 = alsa_buffer_read(prtd, bufferp, monofbytes, NULL);
		bufferp = buf + monofbytes ;
		rc2 = alsa_buffer_read(prtd, bufferp, monofbytes, NULL);
		ret = rc1 + rc2;
	}
	prtd->pcm_buf_pos += fbytes;
	MM_DBG("prtd->pcm_buf_pos =%d, prtd->mmap_flag =%d\n",
				prtd->pcm_buf_pos, prtd->mmap_flag);
	return ret;
}

static int msm_pcm_capture_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	int ret = 0;

	MM_DBG("\n");
	ret = msm_snddev_withdraw_freq(prtd->session_id,
			SNDDEV_CAP_TX, AUDDEV_CLNT_ENC);
	MM_DBG("msm_snddev_withdraw_freq\n");
	auddev_unregister_evt_listner(AUDDEV_CLNT_ENC, prtd->session_id);
	prtd->abort = 0;
	wake_up(&the_locks.enable_wait);
	alsa_audrec_disable(prtd);
	audpreproc_aenc_free(prtd->session_id);
	msm_adsp_put(prtd->audrec);
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

	MM_DBG("pcm_irq_pos = %d\n", prtd->pcm_irq_pos);
	if (prtd->pcm_irq_pos == prtd->pcm_size)
		prtd->pcm_irq_pos = 0;
	return bytes_to_frames(runtime, (prtd->pcm_irq_pos));
}

int msm_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	prtd->out_head = 0; /* point to First buffer on startup */
	prtd->mmap_flag = 1;
	runtime->dma_bytes = snd_pcm_lib_period_bytes(substream)*2;
	dma_mmap_coherent(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
	return 0;
}

int msm_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
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
	.mmap           = msm_pcm_mmap,
};

static int pcm_preallocate_buffer(struct snd_pcm *pcm,
	int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size;
	if (!stream)
		size = PLAYBACK_DMASZ;
	else
		size = CAPTURE_DMASZ;

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
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_coherent(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}

static int msm_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;

	ret = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_PLAYBACK, 1);
	if (ret)
		return ret;
	ret = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_CAPTURE, 1);
	if (ret)
		return ret;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &msm_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &msm_pcm_ops);

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = pcm_preallocate_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
	if (ret)
		return ret;
	ret = pcm_preallocate_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
	if (ret)
		msm_pcm_free_buffers(pcm);
	return ret;
}

struct snd_soc_platform_driver msm_soc_platform = {
	.ops            = &msm_pcm_ops,
	.pcm_new	= msm_pcm_new,
	.pcm_free	= msm_pcm_free_buffers,
};
EXPORT_SYMBOL(msm_soc_platform);

static __devinit int msm_pcm_probe(struct platform_device *pdev)
{
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
		.name = "msm-dsp-audio",
		.owner = THIS_MODULE,
	},
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
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

/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/dma-mapping.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/q6asm.h>
#include <sound/apr_audio.h>
#include <mach/msm_rpcrouter.h>
#include <mach/qdsp6v2/q6voice.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>
#include "msm_audio_mvs.h"


static struct audio_voip_info_type audio_voip_info;
static void audio_mvs_process_ul_pkt(uint8_t *voc_pkt,
				uint32_t pkt_len,
				void *private_data);
static void audio_mvs_process_dl_pkt(uint8_t *voc_pkt,
				uint32_t *pkt_len,
				void *private_data);

struct msm_audio_mvs_frame {
	uint32_t frame_type;
	uint32_t len;
	uint8_t voc_pkt[MVS_MAX_VOC_PKT_SIZE];
};

struct audio_mvs_buf_node {
	struct list_head list;
	struct msm_audio_mvs_frame frame;
};

static struct snd_pcm_hardware msm_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.rates			= (SNDRV_PCM_RATE_8000),
	.rate_min		= 8000,
	.rate_max		= 8000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= MVS_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN,
	.period_bytes_min	= MVS_MAX_VOC_PKT_SIZE,
	.period_bytes_max	= MVS_MAX_VOC_PKT_SIZE,
	.periods_min		= VOIP_MAX_Q_LEN,
	.periods_max		= VOIP_MAX_Q_LEN,
	.fifo_size		= 0,
};

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{

	struct audio_voip_info_type *audio = &audio_voip_info;
	pr_debug("%s\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (substream->stream ==  SNDRV_PCM_STREAM_PLAYBACK)
			audio->playback_start = 1;
		else
			audio->capture_start = 1;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		if (substream->stream ==  SNDRV_PCM_STREAM_PLAYBACK)
			audio->playback_start = 0;
		else
			audio->capture_start = 0;
		break;
	default:
		break;
	}
	return 0;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	int rc = 0;
	struct audio_voip_info_type *audio = &audio_voip_info;
	struct audio_mvs_release_msg release_msg;

	pr_debug("%s\n", __func__);
	memset(&release_msg, 0, sizeof(release_msg));
	mutex_lock(&audio->lock);

	audio->instance--;
	wake_up(&audio->out_wait);

	if (substream->stream ==  SNDRV_PCM_STREAM_PLAYBACK)
		audio->playback_state = AUDIO_MVS_CLOSED;
	else if (substream->stream ==  SNDRV_PCM_STREAM_CAPTURE)
		audio->capture_state = AUDIO_MVS_CLOSED;
	if (!audio->instance) {
		/* Release MVS. */
		release_msg.client_id = cpu_to_be32(MVS_CLIENT_ID_VOIP);
		/* Derigstering the callbacks with voice driver */
		voice_register_mvs_cb(NULL, NULL, audio);
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		voice_register_mvs_cb(audio_mvs_process_ul_pkt,
			NULL, audio);
	} else {
		voice_register_mvs_cb(NULL, audio_mvs_process_dl_pkt,
				audio);
	}

	mutex_unlock(&audio->lock);

	wake_unlock(&audio->suspend_lock);
	pm_qos_update_request(&audio->pm_qos_req, PM_QOS_DEFAULT_VALUE);
	/* Release the IO buffers. */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		audio->in_write = 0;
		audio->in_read = 0;
		memset(audio->in[0].voc_pkt, 0,
			 MVS_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN);
		audio->playback_substream = NULL;
	} else {
		audio->out_write = 0;
		audio->out_read = 0;
		memset(audio->out[0].voc_pkt, 0,
			 MVS_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN);
		audio->capture_substream = NULL;
	}
	return rc;
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_voip_info_type *audio = &audio_voip_info;

	pr_debug("%s\n", __func__);
	mutex_lock(&audio->lock);

	if (audio->playback_substream == NULL ||
		audio->capture_substream == NULL) {
		if (substream->stream ==
			SNDRV_PCM_STREAM_PLAYBACK) {
			audio->playback_substream = substream;
			runtime->hw = msm_pcm_hardware;
			audio_voip_info.in_read = 0;
			audio_voip_info.in_write = 0;
			if (audio->playback_state < AUDIO_MVS_OPENED)
				audio->playback_state = AUDIO_MVS_OPENED;
		} else if (substream->stream ==
			SNDRV_PCM_STREAM_CAPTURE) {
			audio->capture_substream = substream;
			runtime->hw = msm_pcm_hardware;
			audio_voip_info.out_read = 0;
			audio_voip_info.out_write = 0;
			if (audio->capture_state < AUDIO_MVS_OPENED)
				audio->capture_state = AUDIO_MVS_OPENED;
		}
	} else {
		ret  = -EPERM;
		goto err;
	}
	ret = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		pr_debug("%s:snd_pcm_hw_constraint_integer failed\n", __func__);
		goto err;
	}
	audio->instance++;

err:
	mutex_unlock(&audio->lock);
	return ret;
}

static int msm_pcm_playback_copy(struct snd_pcm_substream *substream, int a,
				 snd_pcm_uframes_t hwoff, void __user *buf,
				 snd_pcm_uframes_t frames)
{
	int rc = 0;
	int count = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_voip_info_type *audio = &audio_voip_info;
	uint32_t index;
	pr_debug("%s\n", __func__);

	rc = wait_event_timeout(audio->in_wait,
		(audio->in_write - audio->in_read <= VOIP_MAX_Q_LEN-1),
		1 * HZ);
	if (rc < 0) {
		pr_debug("%s: write was interrupted\n", __func__);
		return  -ERESTARTSYS;
	}

	if (audio->playback_state == AUDIO_MVS_ENABLED) {
		index = audio->in_write % VOIP_MAX_Q_LEN;
		count = frames_to_bytes(runtime, frames);
		if (count == MVS_MAX_VOC_PKT_SIZE) {
			pr_debug("%s:write index = %d\n", __func__, index);
			rc = copy_from_user(audio->in[index].voc_pkt, buf,
						 count);
			if (!rc) {
				audio->in[index].len = count;
				audio->in_write++;
			} else {
				pr_debug("%s:Copy from user returned %d\n",
						__func__, rc);
				rc = -EFAULT;
			}
		} else
			rc = -ENOMEM;

	} else {
		pr_debug("%s:Write performed in invalid state %d\n",
					__func__, audio->playback_state);
		rc = -EINVAL;
	}
	return rc;
}

static int msm_pcm_capture_copy(struct snd_pcm_substream *substream,
			int channel, snd_pcm_uframes_t hwoff,
			void __user *buf, snd_pcm_uframes_t frames)
{
	int rc = 0;
	int count = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_voip_info_type *audio = &audio_voip_info;
	uint32_t index = 0;

	pr_debug("%s\n", __func__);

	/* Ensure the driver has been enabled. */
	if (audio->capture_state != AUDIO_MVS_ENABLED) {
		pr_debug("%s:Read performed in invalid state %d\n",
				__func__, audio->capture_state);
		return -EPERM;
	}
	rc = wait_event_timeout(audio->out_wait,
		((audio->out_read < audio->out_write) ||
		(audio->capture_state == AUDIO_MVS_CLOSING) ||
		(audio->capture_state == AUDIO_MVS_CLOSED)),
		1 * HZ);

	if (rc < 0) {
		pr_debug("%s: Read was interrupted\n", __func__);
		return  -ERESTARTSYS;
	}

	if (audio->capture_state  == AUDIO_MVS_CLOSING
		|| audio->capture_state == AUDIO_MVS_CLOSED) {
		pr_debug("%s:EBUSY STATE\n", __func__);
		rc = -EBUSY;
	} else {
		count = frames_to_bytes(runtime, frames);
		index = audio->out_read % VOIP_MAX_Q_LEN;
		pr_debug("%s:index=%d\n", __func__, index);
		if (audio->out[index].len <= count) {
				rc = copy_to_user(buf,
				audio->out[index].voc_pkt,
				audio->out[index].len);
				if (rc) {
					pr_debug("%s:Copy to user %d\n",
							__func__, rc);
					rc = -EFAULT;
				} else
					audio->out_read++;
		} else {
			pr_debug("%s:returning ENOMEM\n", __func__);
			rc = -ENOMEM;
		}
	}
	return rc;
}

static int msm_pcm_copy(struct snd_pcm_substream *substream, int a,
			snd_pcm_uframes_t hwoff, void __user *buf,
			snd_pcm_uframes_t frames)
{
	int ret = 0;
	pr_debug("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_copy(substream, a, hwoff, buf, frames);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_copy(substream, a, hwoff, buf, frames);
	return ret;
}

/* Capture path */
static void audio_mvs_process_ul_pkt(uint8_t *voc_pkt,
				uint32_t pkt_len,
				void *private_data)
{
	struct audio_voip_info_type *audio = private_data;
	uint32_t index;
	static int i;
	pr_debug("%s\n", __func__);

	if (audio->capture_substream == NULL)
		return;
	index = audio->out_write % VOIP_MAX_Q_LEN;
	memcpy(audio->out[index].voc_pkt, voc_pkt, pkt_len);
	audio->out[index].len = pkt_len;
	audio->out_write++;
	wake_up(&audio->out_wait);
	i++;
	if (audio->capture_start) {
		audio->pcm_capture_irq_pos += audio->pcm_count;
		if (!(i % 2))
			snd_pcm_period_elapsed(audio->capture_substream);
	}
}

/* Playback path */
static void audio_mvs_process_dl_pkt(uint8_t *voc_pkt,
				uint32_t *pkt_len,
				void *private_data)
{
	struct audio_voip_info_type *audio = private_data;
	uint32_t index;
	static int i;
	pr_debug("%s\n", __func__);

	if (audio->playback_substream == NULL)
		return;
	if ((audio->in_write - audio->in_read >= 0)
		&& (audio->playback_start)) {
		index = audio->in_read % VOIP_MAX_Q_LEN;
		*pkt_len = audio->pcm_count;
		memcpy(voc_pkt, audio->in[index].voc_pkt, *pkt_len);
		audio->in_read++;
		wake_up(&audio->in_wait);
		i++;
		audio->pcm_playback_irq_pos += audio->pcm_count;
		if (!(i%2))
			snd_pcm_period_elapsed(audio->playback_substream);
		pr_debug("%s:read_index=%d\n", __func__, index);
	}
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int rc = 0;
	struct audio_voip_info_type *prtd = &audio_voip_info;
	pr_debug("%s\n", __func__);
	prtd->pcm_playback_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	pr_debug("%s:prtd->pcm_playback_size:%d\n",
			__func__, prtd->pcm_playback_size);
	pr_debug("%s:prtd->pcm_count:%d\n", __func__, prtd->pcm_count);

	mutex_lock(&prtd->prepare_lock);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (prtd->playback_state == AUDIO_MVS_ENABLED)
			goto enabled;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (prtd->capture_state == AUDIO_MVS_ENABLED)
			goto enabled;
	}

	pr_debug("%s:Register cbs with voice driver check audio_mvs_driver\n",
			__func__);
	if (prtd->instance == 2) {
		voice_register_mvs_cb(audio_mvs_process_ul_pkt,
				audio_mvs_process_dl_pkt,
				prtd);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			voice_register_mvs_cb(NULL,
					audio_mvs_process_dl_pkt,
					prtd);
		} else {
			voice_register_mvs_cb(audio_mvs_process_ul_pkt,
					NULL,
					prtd);
		}
	}

enabled:
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd->playback_state = AUDIO_MVS_ENABLED;
		prtd->pcm_playback_irq_pos = 0;
		prtd->pcm_playback_buf_pos = 0;
		/* rate and channels are sent to audio driver */
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		prtd->capture_state = AUDIO_MVS_ENABLED;
		prtd->pcm_capture_size  = snd_pcm_lib_buffer_bytes(substream);
		prtd->pcm_capture_count = snd_pcm_lib_period_bytes(substream);
		prtd->pcm_capture_irq_pos = 0;
		prtd->pcm_capture_buf_pos = 0;
	}
	mutex_unlock(&prtd->prepare_lock);
	return rc;
}

static snd_pcm_uframes_t
msm_pcm_playback_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_voip_info_type *audio = &audio_voip_info;

	if (audio->pcm_playback_irq_pos >= audio->pcm_playback_size)
		audio->pcm_playback_irq_pos = 0;
	return bytes_to_frames(runtime, (audio->pcm_playback_irq_pos));
}

static snd_pcm_uframes_t
msm_pcm_capture_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_voip_info_type *audio = &audio_voip_info;

	if (audio->pcm_capture_irq_pos >= audio->pcm_capture_size)
		audio->pcm_capture_irq_pos = 0;
	return bytes_to_frames(runtime, (audio->pcm_capture_irq_pos));
}

static snd_pcm_uframes_t msm_pcm_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t ret = 0;
	pr_debug("%s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_pointer(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_pointer(substream);
	return ret;
}

static struct snd_pcm_ops msm_mvs_pcm_ops = {
	.open = msm_pcm_open,
	.copy = msm_pcm_copy,
	.close = msm_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.prepare = msm_pcm_prepare,
	.trigger = msm_pcm_trigger,
	.pointer = msm_pcm_pointer,

};

static int msm_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int   i, ret, offset = 0;
	struct snd_pcm_substream *substream = NULL;
	struct snd_dma_buffer *dma_buffer = NULL;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;

	ret = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_PLAYBACK, 1);
	if (ret)
		return ret;
	ret = snd_pcm_new_stream(pcm, SNDRV_PCM_STREAM_CAPTURE, 1);
	if (ret)
		return ret;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &msm_mvs_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &msm_mvs_pcm_ops);

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (!substream)
		return -ENOMEM;

	dma_buffer = &substream->dma_buffer;
	dma_buffer->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buffer->dev.dev = card->dev;
	dma_buffer->private_data = NULL;
	dma_buffer->area = dma_alloc_coherent(card->dev,
				(MVS_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN),
				&dma_buffer->addr, GFP_KERNEL);
	if (!dma_buffer->area) {
		pr_err("%s:MSM VOIP dma_alloc failed\n", __func__);
		return -ENOMEM;
	}
	dma_buffer->bytes = MVS_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN;
	memset(dma_buffer->area, 0, MVS_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN);
	audio_voip_info.in_read = 0;
	audio_voip_info.in_write = 0;
	audio_voip_info.out_read = 0;
	audio_voip_info.out_write = 0;
	for (i = 0; i < VOIP_MAX_Q_LEN; i++) {
		audio_voip_info.in[i].voc_pkt =
		dma_buffer->area + offset;
		offset = offset + MVS_MAX_VOC_PKT_SIZE;
	}
	substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (!substream)
		return -ENOMEM;

	dma_buffer = &substream->dma_buffer;
	dma_buffer->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buffer->dev.dev = card->dev;
	dma_buffer->private_data = NULL;
	dma_buffer->area = dma_alloc_coherent(card->dev,
					(MVS_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN),
					&dma_buffer->addr, GFP_KERNEL);
	if (!dma_buffer->area) {
		pr_err("%s:MSM VOIP dma_alloc failed\n", __func__);
		return -ENOMEM;
	}
	memset(dma_buffer->area, 0, MVS_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN);
	dma_buffer->bytes = MVS_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN;
	for (i = 0; i < VOIP_MAX_Q_LEN; i++) {
		audio_voip_info.out[i].voc_pkt =
		dma_buffer->area + offset;
		offset = offset + MVS_MAX_VOC_PKT_SIZE;
	}
	audio_voip_info.playback_substream = NULL;
	audio_voip_info.capture_substream = NULL;

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

struct snd_soc_platform_driver msm_mvs_soc_platform = {
	.ops		= &msm_mvs_pcm_ops,
	.pcm_new	= msm_pcm_new,
	.pcm_free	= msm_pcm_free_buffers,
};
EXPORT_SYMBOL(msm_mvs_soc_platform);

static __devinit int msm_pcm_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
				&msm_mvs_soc_platform);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-mvs-audio",
		.owner = THIS_MODULE,
	},
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
};

static int __init msm_mvs_soc_platform_init(void)
{
	memset(&audio_voip_info, 0, sizeof(audio_voip_info));
	mutex_init(&audio_voip_info.lock);
	mutex_init(&audio_voip_info.prepare_lock);
	init_waitqueue_head(&audio_voip_info.out_wait);
	init_waitqueue_head(&audio_voip_info.in_wait);
	wake_lock_init(&audio_voip_info.suspend_lock, WAKE_LOCK_SUSPEND,
				"audio_mvs_suspend");
	pm_qos_add_request(&audio_voip_info.pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	return platform_driver_register(&msm_pcm_driver);
}
module_init(msm_mvs_soc_platform_init);

static void __exit msm_mvs_soc_platform_exit(void)
{
	 platform_driver_unregister(&msm_pcm_driver);
}
module_exit(msm_mvs_soc_platform_exit);

MODULE_DESCRIPTION("MVS PCM module platform driver");
MODULE_LICENSE("GPL v2");

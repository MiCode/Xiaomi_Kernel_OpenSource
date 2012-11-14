/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/q6afe.h>

#include "msm-pcm-q6.h"
#include "msm-pcm-routing.h"
#include "qdsp6/q6voice.h"

enum {
	DTMF_IN_RX,
	DTMF_IN_TX,
};

enum format {
	FORMAT_S16_LE = 2
};

struct dtmf_det_info {
	char     session[MAX_SESSION_NAME_LEN];
	uint8_t  dir;
	uint16_t high_freq;
	uint16_t low_freq;
};

struct dtmf_buf_node {
	struct list_head list;
	struct dtmf_det_info dtmf_det_pkt;
};

enum dtmf_state {
	DTMF_GEN_RX_STOPPED,
	DTMF_GEN_RX_STARTED,
};

#define DTMF_MAX_Q_LEN 10
#define DTMF_PKT_SIZE sizeof(struct dtmf_det_info)

struct dtmf_drv_info {
	enum  dtmf_state state;
	struct snd_pcm_substream *capture_substream;

	struct list_head out_queue;
	struct list_head free_out_queue;

	wait_queue_head_t out_wait;

	struct mutex lock;
	spinlock_t dsp_lock;

	uint8_t capture_start;
	uint8_t capture_instance;

	unsigned int pcm_capture_size;
	unsigned int pcm_capture_count;
	unsigned int pcm_capture_irq_pos;
	unsigned int pcm_capture_buf_pos;
};

static struct snd_pcm_hardware msm_pcm_hardware = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID |
				 SNDRV_PCM_INFO_INTERLEAVED),
	.formats =              SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min =         1,
	.channels_max =         1,
	.buffer_bytes_max =	(sizeof(struct dtmf_buf_node) * DTMF_MAX_Q_LEN),
	.period_bytes_min =	DTMF_PKT_SIZE,
	.period_bytes_max =	DTMF_PKT_SIZE,
	.periods_min =		DTMF_MAX_Q_LEN,
	.periods_max =		DTMF_MAX_Q_LEN,
	.fifo_size =            0,
};

static int msm_dtmf_rx_generate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	uint16_t low_freq = ucontrol->value.integer.value[0];
	uint16_t high_freq = ucontrol->value.integer.value[1];
	int64_t duration = ucontrol->value.integer.value[2];
	uint16_t gain = ucontrol->value.integer.value[3];

	pr_debug("%s: low_freq=%d high_freq=%d duration=%d gain=%d\n",
		 __func__, low_freq, high_freq, (int)duration, gain);
	afe_dtmf_generate_rx(duration, high_freq, low_freq, gain);
	return 0;
}

static int msm_dtmf_rx_generate_get(struct  snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s:\n", __func__);
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int msm_dtmf_detect_voice_rx_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	pr_debug("%s: enable=%d\n", __func__, enable);
	voc_enable_dtmf_rx_detection(voc_get_session_id(VOICE_SESSION_NAME),
				     enable);

	return 0;
}

static int msm_dtmf_detect_voice_rx_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int msm_dtmf_detect_volte_rx_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	pr_debug("%s: enable=%d\n", __func__, enable);
	voc_enable_dtmf_rx_detection(voc_get_session_id(VOLTE_SESSION_NAME),
				     enable);

	return 0;
}

static int msm_dtmf_detect_volte_rx_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static struct snd_kcontrol_new msm_dtmf_controls[] = {
	SOC_SINGLE_MULTI_EXT("DTMF_Generate Rx Low High Duration Gain",
			     SND_SOC_NOPM, 0, 5000, 0, 4,
			     msm_dtmf_rx_generate_get,
			     msm_dtmf_rx_generate_put),
	SOC_SINGLE_EXT("DTMF_Detect Rx Voice enable", SND_SOC_NOPM, 0, 1, 0,
				msm_dtmf_detect_voice_rx_get,
				msm_dtmf_detect_voice_rx_put),
	SOC_SINGLE_EXT("DTMF_Detect Rx VoLTE enable", SND_SOC_NOPM, 0, 1, 0,
				msm_dtmf_detect_volte_rx_get,
				msm_dtmf_detect_volte_rx_put),
};

static int msm_pcm_dtmf_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, msm_dtmf_controls,
				      ARRAY_SIZE(msm_dtmf_controls));
	return 0;
}

static void dtmf_rx_detected_cb(uint8_t *pkt,
				char *session,
				void *private_data)
{
	struct dtmf_buf_node *buf_node = NULL;
	struct vss_istream_evt_rx_dtmf_detected *dtmf_det_pkt =
		(struct vss_istream_evt_rx_dtmf_detected *)pkt;
	struct dtmf_drv_info *prtd = private_data;
	unsigned long dsp_flags;

	pr_debug("%s\n", __func__);
	if (prtd->capture_substream == NULL)
		return;

	/* Copy dtmf detected info into out_queue. */
	spin_lock_irqsave(&prtd->dsp_lock, dsp_flags);
	/* discarding dtmf detection info till start is received */
	if (!list_empty(&prtd->free_out_queue) && prtd->capture_start) {
		buf_node = list_first_entry(&prtd->free_out_queue,
					    struct dtmf_buf_node, list);
		list_del(&buf_node->list);
		buf_node->dtmf_det_pkt.high_freq = dtmf_det_pkt->high_freq;
		buf_node->dtmf_det_pkt.low_freq = dtmf_det_pkt->low_freq;
		if (session != NULL)
			strlcpy(buf_node->dtmf_det_pkt.session,
				session, MAX_SESSION_NAME_LEN);

		buf_node->dtmf_det_pkt.dir = DTMF_IN_RX;
		pr_debug("high =%d, low=%d session=%s\n",
			 buf_node->dtmf_det_pkt.high_freq,
			 buf_node->dtmf_det_pkt.low_freq,
			 buf_node->dtmf_det_pkt.session);
		list_add_tail(&buf_node->list, &prtd->out_queue);
		prtd->pcm_capture_irq_pos += prtd->pcm_capture_count;
		spin_unlock_irqrestore(&prtd->dsp_lock, dsp_flags);
		snd_pcm_period_elapsed(prtd->capture_substream);
	} else {
		spin_unlock_irqrestore(&prtd->dsp_lock, dsp_flags);
		pr_err("DTMF detection pkt in Rx  dropped, no free node available\n");
	}

	wake_up(&prtd->out_wait);
}

static int msm_pcm_capture_copy(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t hwoff,
				void __user *buf, snd_pcm_uframes_t frames)
{
	int ret = 0;
	int count = 0;
	struct dtmf_buf_node *buf_node = NULL;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dtmf_drv_info *prtd = runtime->private_data;
	unsigned long dsp_flags;

	count = frames_to_bytes(runtime, frames);

	ret = wait_event_interruptible_timeout(prtd->out_wait,
				(!list_empty(&prtd->out_queue)),
				1 * HZ);

	if (ret > 0) {
		if (count <= DTMF_PKT_SIZE) {
			spin_lock_irqsave(&prtd->dsp_lock, dsp_flags);
			buf_node = list_first_entry(&prtd->out_queue,
					struct dtmf_buf_node, list);
			list_del(&buf_node->list);
			spin_unlock_irqrestore(&prtd->dsp_lock, dsp_flags);
			ret = copy_to_user(buf,
					   &buf_node->dtmf_det_pkt,
					   count);
			if (ret) {
				pr_err("%s: Copy to user retuned %d\n",
					__func__, ret);
				ret = -EFAULT;
			}
			spin_lock_irqsave(&prtd->dsp_lock, dsp_flags);
			list_add_tail(&buf_node->list,
				      &prtd->free_out_queue);
			spin_unlock_irqrestore(&prtd->dsp_lock, dsp_flags);

		} else {
			pr_err("%s: Read count %d > DTMF_PKT_SIZE\n",
				__func__, count);
			ret = -ENOMEM;
		}
	} else if (ret == 0) {
		pr_err("%s: No UL data available\n", __func__);
		ret = -ETIMEDOUT;
	} else {
		pr_err("%s: Read was interrupted\n", __func__);
		ret = -ERESTARTSYS;
	}
	return ret;
}

static int msm_pcm_copy(struct snd_pcm_substream *substream, int a,
	 snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	int ret = 0;
	pr_debug("%s() DTMF\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_copy(substream, a, hwoff, buf, frames);

	return ret;
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dtmf_drv_info *prtd = NULL;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		prtd = kzalloc(sizeof(struct dtmf_drv_info), GFP_KERNEL);

		if (prtd == NULL) {
			pr_err("Failed to allocate memory for msm_audio\n");
			ret = -ENOMEM;
			goto done;
		}

		mutex_init(&prtd->lock);
		spin_lock_init(&prtd->dsp_lock);
		init_waitqueue_head(&prtd->out_wait);
		INIT_LIST_HEAD(&prtd->out_queue);
		INIT_LIST_HEAD(&prtd->free_out_queue);

		runtime->hw = msm_pcm_hardware;

		ret = snd_pcm_hw_constraint_integer(runtime,
						    SNDRV_PCM_HW_PARAM_PERIODS);
		if (ret < 0)
			pr_info("snd_pcm_hw_constraint_integer failed\n");

		prtd->capture_substream = substream;
		prtd->capture_instance++;
		runtime->private_data = prtd;
	}

done:
	return ret;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct list_head *ptr = NULL;
	struct list_head *next = NULL;
	struct dtmf_buf_node *buf_node = NULL;
	struct snd_dma_buffer *c_dma_buf;
	struct snd_pcm_substream *c_substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dtmf_drv_info *prtd = runtime->private_data;
	unsigned long dsp_flags;

	pr_debug("%s() DTMF\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		mutex_lock(&prtd->lock);
		wake_up(&prtd->out_wait);

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			prtd->capture_instance--;

		if (!prtd->capture_instance) {
			if (prtd->state == DTMF_GEN_RX_STARTED) {
				prtd->state = DTMF_GEN_RX_STOPPED;
				voc_disable_dtmf_det_on_active_sessions();
				voc_register_dtmf_rx_detection_cb(NULL, NULL);
			}
			/* release all buffer */
			/* release out_queue and free_out_queue */
			pr_debug("release all buffer\n");
			c_substream = prtd->capture_substream;
			if (c_substream == NULL) {
				pr_debug("c_substream is NULL\n");
				mutex_unlock(&prtd->lock);
				return -EINVAL;
			}

			c_dma_buf = &c_substream->dma_buffer;
			if (c_dma_buf == NULL) {
				pr_debug("c_dma_buf is NULL.\n");
				mutex_unlock(&prtd->lock);
				return -EINVAL;
			}

			if (c_dma_buf->area != NULL) {
				spin_lock_irqsave(&prtd->dsp_lock, dsp_flags);
				list_for_each_safe(ptr, next,
							&prtd->out_queue) {
					buf_node = list_entry(ptr,
						   struct dtmf_buf_node, list);
					list_del(&buf_node->list);
				}

				list_for_each_safe(ptr, next,
						   &prtd->free_out_queue) {
					buf_node = list_entry(ptr,
						   struct dtmf_buf_node, list);
					list_del(&buf_node->list);
				}

				spin_unlock_irqrestore(&prtd->dsp_lock,
						       dsp_flags);
				dma_free_coherent(c_substream->pcm->card->dev,
						  runtime->hw.buffer_bytes_max,
						  c_dma_buf->area,
						  c_dma_buf->addr);
				c_dma_buf->area = NULL;
			}
		}
		prtd->capture_substream = NULL;
		mutex_unlock(&prtd->lock);
	}

	return ret;
}

static int msm_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dtmf_drv_info *prtd = runtime->private_data;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct dtmf_buf_node *buf_node = NULL;
	int i = 0, offset = 0;
	int ret = 0;

	pr_debug("%s: DTMF\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		mutex_lock(&prtd->lock);
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		dma_buf->private_data = NULL;

		dma_buf->area = dma_alloc_coherent(substream->pcm->card->dev,
						runtime->hw.buffer_bytes_max,
						&dma_buf->addr, GFP_KERNEL);
		if (!dma_buf->area) {
			pr_err("%s:MSM DTMF dma_alloc failed\n", __func__);
			mutex_unlock(&prtd->lock);
			return -ENOMEM;
		}

		dma_buf->bytes = runtime->hw.buffer_bytes_max;
		memset(dma_buf->area, 0, runtime->hw.buffer_bytes_max);

		for (i = 0; i < DTMF_MAX_Q_LEN; i++) {
			pr_debug("node =%d\n", i);
			buf_node = (void *) dma_buf->area + offset;
			list_add_tail(&buf_node->list,
				      &prtd->free_out_queue);
			offset = offset + sizeof(struct dtmf_buf_node);
		}

		snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
		mutex_unlock(&prtd->lock);
	}

	return ret;
}

static int msm_pcm_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dtmf_drv_info *prtd = runtime->private_data;

	pr_debug("%s: DTMF\n", __func__);
	prtd->pcm_capture_size  = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_capture_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_capture_irq_pos = 0;
	prtd->pcm_capture_buf_pos = 0;
	return 0;
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dtmf_drv_info *prtd = runtime->private_data;

	pr_debug("%s: DTMF\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		mutex_lock(&prtd->lock);

		msm_pcm_capture_prepare(substream);

		if (runtime->format != FORMAT_S16_LE) {
			pr_err("format:%u doesnt match %d\n",
			       (uint32_t)runtime->format, FORMAT_S16_LE);
			mutex_unlock(&prtd->lock);
			return -EINVAL;
		}

		if (prtd->capture_instance &&
			(prtd->state != DTMF_GEN_RX_STARTED)) {
			voc_register_dtmf_rx_detection_cb(dtmf_rx_detected_cb,
							  prtd);
			prtd->state = DTMF_GEN_RX_STARTED;
		}
		mutex_unlock(&prtd->lock);
	}

	return 0;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dtmf_drv_info *prtd = runtime->private_data;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		pr_debug("%s: Trigger start\n", __func__);
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			prtd->capture_start = 1;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("SNDRV_PCM_TRIGGER_STOP\n");
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			prtd->capture_start = 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static snd_pcm_uframes_t msm_pcm_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dtmf_drv_info *prtd = runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (prtd->pcm_capture_irq_pos >= prtd->pcm_capture_size)
			prtd->pcm_capture_irq_pos = 0;
		ret = bytes_to_frames(runtime, (prtd->pcm_capture_irq_pos));
	}

	return ret;
}

static struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.copy		= msm_pcm_copy,
	.hw_params	= msm_pcm_hw_params,
	.close          = msm_pcm_close,
	.prepare        = msm_pcm_prepare,
	.trigger        = msm_pcm_trigger,
	.pointer        = msm_pcm_pointer,
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
	.probe		= msm_pcm_dtmf_probe,
};

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
		.name = "msm-pcm-dtmf",
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

MODULE_DESCRIPTION("DTMF platform driver");
MODULE_LICENSE("GPL v2");

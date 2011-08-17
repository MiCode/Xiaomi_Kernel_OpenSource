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
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <asm/dma.h>

#include "msm-pcm-q6.h"
#include "msm-pcm-routing.h"
#include "qdsp6/q6voice.h"

#define VOIP_MAX_Q_LEN 10
#define VOIP_MAX_VOC_PKT_SIZE 320

enum voip_state {
	VOIP_STOPPED,
	VOIP_STARTED,
};

struct voip_frame {
	uint32_t len;
	uint8_t voc_pkt[VOIP_MAX_VOC_PKT_SIZE];
};

struct voip_buf_node {
	struct list_head list;
	struct voip_frame frame;
};

struct voip_drv_info {
	enum  voip_state state;

	struct snd_pcm_substream *playback_substream;
	struct snd_pcm_substream *capture_substream;

	struct list_head in_queue;
	struct list_head free_in_queue;

	struct list_head out_queue;
	struct list_head free_out_queue;

	wait_queue_head_t out_wait;

	struct mutex lock;
	struct mutex in_lock;
	struct mutex out_lock;

	spinlock_t dsp_lock;


	uint8_t capture_start;
	uint8_t playback_start;

	uint8_t playback_instance;
	uint8_t capture_instance;

	unsigned int play_samp_rate;
	unsigned int cap_samp_rate;

	unsigned int pcm_size;
	unsigned int pcm_count;
	unsigned int pcm_playback_irq_pos;      /* IRQ position */
	unsigned int pcm_playback_buf_pos;      /* position in buffer */

	unsigned int pcm_capture_size;
	unsigned int pcm_capture_count;
	unsigned int pcm_capture_irq_pos;       /* IRQ position */
	unsigned int pcm_capture_buf_pos;       /* position in buffer */
};

static struct voip_drv_info voip_info;

static struct snd_pcm_hardware msm_pcm_hardware = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED),
	.formats =              SNDRV_PCM_FMTBIT_S16_LE,
	.rates =                SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
	.rate_min =             8000,
	.rate_max =             16000,
	.channels_min =         1,
	.channels_max =         1,
	.buffer_bytes_max =	VOIP_MAX_VOC_PKT_SIZE * VOIP_MAX_Q_LEN,
	.period_bytes_min =	VOIP_MAX_VOC_PKT_SIZE,
	.period_bytes_max =	VOIP_MAX_VOC_PKT_SIZE,
	.periods_min =		VOIP_MAX_Q_LEN,
	.periods_max =		VOIP_MAX_Q_LEN,
	.fifo_size =            0,
};


/* sample rate supported */
static unsigned int supported_sample_rates[] = {8000, 16000};

/* capture path */
static void voip_process_ul_pkt(uint8_t *voc_pkt,
					uint32_t pkt_len,
					void *private_data)
{
	struct voip_buf_node *buf_node = NULL;
	struct voip_drv_info *prtd = private_data;
	unsigned long dsp_flags;

	if (prtd->capture_substream == NULL)
		return;

	/* Copy up-link packet into out_queue. */
	spin_lock_irqsave(&prtd->dsp_lock, dsp_flags);

	/* discarding UL packets till start is received */
	if (!list_empty(&prtd->free_out_queue) && prtd->capture_start) {
		buf_node = list_first_entry(&prtd->free_out_queue,
					struct voip_buf_node, list);
		list_del(&buf_node->list);

		buf_node->frame.len = pkt_len;
		memcpy(&buf_node->frame.voc_pkt[0], voc_pkt,
					buf_node->frame.len);

		list_add_tail(&buf_node->list, &prtd->out_queue);
		prtd->pcm_capture_irq_pos += prtd->pcm_capture_count;
		spin_unlock_irqrestore(&prtd->dsp_lock, dsp_flags);
		snd_pcm_period_elapsed(prtd->capture_substream);
	} else {
		spin_unlock_irqrestore(&prtd->dsp_lock, dsp_flags);
		pr_err("UL data dropped\n");
	}

	wake_up(&prtd->out_wait);
}

/* playback path */
static void voip_process_dl_pkt(uint8_t *voc_pkt,
					uint32_t *pkt_len,
					void *private_data)
{
	struct voip_buf_node *buf_node = NULL;
	struct voip_drv_info *prtd = private_data;
	unsigned long dsp_flags;


	if (prtd->playback_substream == NULL)
		return;

	spin_lock_irqsave(&prtd->dsp_lock, dsp_flags);

	if (!list_empty(&prtd->in_queue) && prtd->playback_start) {
		buf_node = list_first_entry(&prtd->in_queue,
				struct voip_buf_node, list);
		list_del(&buf_node->list);

		*pkt_len = buf_node->frame.len;

		memcpy(voc_pkt,
			&buf_node->frame.voc_pkt[0],
			buf_node->frame.len);

		list_add_tail(&buf_node->list, &prtd->free_in_queue);

		prtd->pcm_playback_irq_pos += prtd->pcm_count;
		spin_unlock_irqrestore(&prtd->dsp_lock, dsp_flags);
		snd_pcm_period_elapsed(prtd->playback_substream);
	} else {
		*pkt_len = 0;
		spin_unlock_irqrestore(&prtd->dsp_lock, dsp_flags);
		pr_err("DL data not available\n");
	}
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static int msm_pcm_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct voip_drv_info *prtd = runtime->private_data;

	prtd->play_samp_rate = runtime->rate;
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_playback_irq_pos = 0;
	prtd->pcm_playback_buf_pos = 0;

	return 0;
}

static int msm_pcm_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct voip_drv_info *prtd = runtime->private_data;
	int ret = 0;

	prtd->cap_samp_rate = runtime->rate;
	prtd->pcm_capture_size  = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_capture_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_capture_irq_pos = 0;
	prtd->pcm_capture_buf_pos = 0;
	return ret;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct voip_drv_info *prtd = runtime->private_data;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		pr_debug("%s: Trigger start\n", __func__);
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			prtd->capture_start = 1;
		else
			prtd->playback_start = 1;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("SNDRV_PCM_TRIGGER_STOP\n");
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			prtd->playback_start = 0;
		else
			prtd->capture_start = 0;
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
	struct voip_drv_info *prtd = &voip_info;
	int ret = 0;

	pr_debug("%s, VoIP\n", __func__);
	mutex_lock(&prtd->lock);

	runtime->hw = msm_pcm_hardware;

	ret = snd_pcm_hw_constraint_list(runtime, 0,
					SNDRV_PCM_HW_PARAM_RATE,
					&constraints_sample_rates);
	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_list failed\n");

	ret = snd_pcm_hw_constraint_integer(runtime,
					SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		pr_debug("snd_pcm_hw_constraint_integer failed\n");
		goto err;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd->playback_substream = substream;
		prtd->playback_instance++;
	} else {
		prtd->capture_substream = substream;
		prtd->capture_instance++;
	}
	runtime->private_data = prtd;
err:
	mutex_unlock(&prtd->lock);

	return ret;
}

static int msm_pcm_playback_copy(struct snd_pcm_substream *substream, int a,
	snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	int ret = 0;
	struct voip_buf_node *buf_node = NULL;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct voip_drv_info *prtd = runtime->private_data;

	int count = frames_to_bytes(runtime, frames);
	pr_debug("%s: count = %d\n", __func__, count);

	mutex_lock(&prtd->in_lock);

	if (count == VOIP_MAX_VOC_PKT_SIZE) {
		if (!list_empty(&prtd->free_in_queue)) {
			buf_node =
				list_first_entry(&prtd->free_in_queue,
						struct voip_buf_node, list);
			list_del(&buf_node->list);

			ret = copy_from_user(&buf_node->frame.voc_pkt,
						buf, count);
			buf_node->frame.len = count;
			list_add_tail(&buf_node->list, &prtd->in_queue);
		} else
			pr_err("%s: No free DL buffs\n", __func__);
	} else {
		pr_err("%s: Write count %d is not  VOIP_MAX_VOC_PKT_SIZE\n",
			__func__, count);
		ret = -ENOMEM;
	}

	mutex_unlock(&prtd->in_lock);

	return  ret;
}
static int msm_pcm_capture_copy(struct snd_pcm_substream *substream,
		int channel, snd_pcm_uframes_t hwoff, void __user *buf,
						snd_pcm_uframes_t frames)
{
	int ret = 0;
	int count = 0;
	struct voip_buf_node *buf_node = NULL;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct voip_drv_info *prtd = runtime->private_data;

	count = frames_to_bytes(runtime, frames);

	pr_debug("%s: count = %d\n", __func__, count);

	ret = wait_event_interruptible_timeout(prtd->out_wait,
				(!list_empty(&prtd->out_queue) ||
				prtd->state == VOIP_STOPPED),
				1 * HZ);

	if (ret > 0) {
		mutex_lock(&prtd->out_lock);

		if (count == VOIP_MAX_VOC_PKT_SIZE) {
			buf_node = list_first_entry(&prtd->out_queue,
					struct voip_buf_node, list);
			list_del(&buf_node->list);

			ret = copy_to_user(buf,
					&buf_node->frame.voc_pkt,
					buf_node->frame.len);

			if (ret) {
				pr_err("%s: Copy to user retuned %d\n",
					__func__, ret);
				ret = -EFAULT;
			}
			list_add_tail(&buf_node->list,
						&prtd->free_out_queue);
		} else {
				pr_err("%s: Read count %d < VOIP_MAX_VOC_PKT_SIZE\n",
						__func__, count);
				ret = -ENOMEM;
		}

		mutex_unlock(&prtd->out_lock);

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

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_copy(substream, a, hwoff, buf, frames);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_copy(substream, a, hwoff, buf, frames);

	return ret;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct list_head *ptr = NULL;
	struct list_head *next = NULL;
	struct voip_buf_node *buf_node = NULL;
	struct snd_dma_buffer *p_dma_buf, *c_dma_buf;
	struct snd_pcm_substream *p_substream, *c_substream;
	struct snd_pcm_runtime *runtime;
	struct voip_drv_info *prtd;

	if (substream == NULL) {
		pr_err("substream is NULL\n");
		return -EINVAL;
	}
	runtime = substream->runtime;
	prtd = runtime->private_data;

	wake_up(&prtd->out_wait);

	mutex_lock(&prtd->lock);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		prtd->playback_instance--;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		prtd->capture_instance--;

	if (!prtd->playback_instance && !prtd->capture_instance) {
		if (prtd->state == VOIP_STARTED) {
			prtd->state = VOIP_STOPPED;
			voc_end_voice_call();
			voc_set_voc_path_full(0);
			voc_register_mvs_cb(NULL, NULL, prtd);
		}
		/* release all buffer */
		/* release in_queue and free_in_queue */
		pr_debug("release all buffer\n");
		p_substream = prtd->playback_substream;
		if (p_substream == NULL) {
			pr_debug("p_substream is NULL\n");
			goto capt;
		}
		p_dma_buf = &p_substream->dma_buffer;
		if (p_dma_buf == NULL) {
			pr_debug("p_dma_buf is NULL\n");
			goto capt;
		}
		if (p_dma_buf->area != NULL) {
			mutex_lock(&prtd->in_lock);
			list_for_each_safe(ptr, next, &prtd->in_queue) {
				buf_node = list_entry(ptr,
						struct voip_buf_node, list);
				list_del(&buf_node->list);
			}
			list_for_each_safe(ptr, next, &prtd->free_in_queue) {
				buf_node = list_entry(ptr,
						struct voip_buf_node, list);
				list_del(&buf_node->list);
			}
			dma_free_coherent(p_substream->pcm->card->dev,
				runtime->hw.buffer_bytes_max, p_dma_buf->area,
				p_dma_buf->addr);
			p_dma_buf->area = NULL;
			mutex_unlock(&prtd->in_lock);
		}
		/* release out_queue and free_out_queue */
capt:		c_substream = prtd->capture_substream;
		if (c_substream == NULL) {
			pr_debug("c_substream is NULL\n");
			goto done;
		}
		c_dma_buf = &c_substream->dma_buffer;
		if (c_substream == NULL) {
			pr_debug("c_dma_buf is NULL.\n");
			goto done;
		}
		if (c_dma_buf->area != NULL) {
			mutex_lock(&prtd->out_lock);
			list_for_each_safe(ptr, next, &prtd->out_queue) {
				buf_node = list_entry(ptr,
						struct voip_buf_node, list);
				list_del(&buf_node->list);
			}
			list_for_each_safe(ptr, next, &prtd->free_out_queue) {
				buf_node = list_entry(ptr,
						struct voip_buf_node, list);
				list_del(&buf_node->list);
			}
			dma_free_coherent(c_substream->pcm->card->dev,
				runtime->hw.buffer_bytes_max, c_dma_buf->area,
				c_dma_buf->addr);
			c_dma_buf->area = NULL;
			mutex_unlock(&prtd->out_lock);
		}
done:
		prtd->capture_substream = NULL;
		prtd->playback_substream = NULL;
	}
	mutex_unlock(&prtd->lock);

	return ret;
}
static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct voip_drv_info *prtd = runtime->private_data;

	mutex_lock(&prtd->lock);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_prepare(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_prepare(substream);

	if (prtd->playback_instance && prtd->capture_instance
				&& (prtd->state != VOIP_STARTED)) {

		voc_set_voc_path_full(1);

		if ((prtd->play_samp_rate == 8000) &&
					(prtd->cap_samp_rate == 8000))
			voc_config_vocoder(VSS_MEDIA_ID_PCM_NB,
						0, VSS_NETWORK_ID_VOIP_NB);
		else if ((prtd->play_samp_rate == 16000) &&
					(prtd->cap_samp_rate == 16000))
			voc_config_vocoder(VSS_MEDIA_ID_PCM_WB,
						0, VSS_NETWORK_ID_VOIP_WB);
		else {
			pr_err(" sample rate are not set properly\n");
			goto err;
		}
		voc_register_mvs_cb(voip_process_ul_pkt,
					voip_process_dl_pkt, prtd);
		voc_start_voice_call();

		prtd->state = VOIP_STARTED;
	}
err:
	mutex_unlock(&prtd->lock);

	return ret;
}

static snd_pcm_uframes_t
msm_pcm_playback_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct voip_drv_info *prtd = runtime->private_data;

	pr_debug("%s\n", __func__);
	if (prtd->pcm_playback_irq_pos >= prtd->pcm_size)
		prtd->pcm_playback_irq_pos = 0;
	return bytes_to_frames(runtime, (prtd->pcm_playback_irq_pos));
}

static snd_pcm_uframes_t
msm_pcm_capture_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct voip_drv_info *prtd = runtime->private_data;

	if (prtd->pcm_capture_irq_pos >= prtd->pcm_capture_size)
		prtd->pcm_capture_irq_pos = 0;
	return bytes_to_frames(runtime, (prtd->pcm_capture_irq_pos));
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

static int msm_pcm_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);
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
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct voip_buf_node *buf_node = NULL;
	int i = 0, offset = 0;

	pr_debug("%s: voip\n", __func__);

	mutex_lock(&voip_info.lock);

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	dma_buf->area = dma_alloc_coherent(substream->pcm->card->dev,
			runtime->hw.buffer_bytes_max,
			&dma_buf->addr, GFP_KERNEL);
	if (!dma_buf->area) {
		pr_err("%s:MSM VOIP dma_alloc failed\n", __func__);
		return -ENOMEM;
	}

	dma_buf->bytes = runtime->hw.buffer_bytes_max;
	memset(dma_buf->area, 0, runtime->hw.buffer_bytes_max);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < VOIP_MAX_Q_LEN; i++) {
			buf_node = (void *)dma_buf->area + offset;

			mutex_lock(&voip_info.in_lock);
			list_add_tail(&buf_node->list,
					&voip_info.free_in_queue);
			mutex_unlock(&voip_info.in_lock);
			offset = offset + sizeof(struct voip_buf_node);
		}
	} else {
		for (i = 0; i < VOIP_MAX_Q_LEN; i++) {
			buf_node = (void *) dma_buf->area + offset;
			mutex_lock(&voip_info.out_lock);
			list_add_tail(&buf_node->list,
					&voip_info.free_out_queue);
			mutex_unlock(&voip_info.out_lock);
			offset = offset + sizeof(struct voip_buf_node);
		}
	}

	mutex_unlock(&voip_info.lock);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.copy		= msm_pcm_copy,
	.hw_params	= msm_pcm_hw_params,
	.close          = msm_pcm_close,
	.prepare        = msm_pcm_prepare,
	.trigger        = msm_pcm_trigger,
	.pointer        = msm_pcm_pointer,
	.mmap		= msm_pcm_mmap,
};

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	pr_debug("msm_asoc_pcm_new\n");
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
		.name = "msm-voip-dsp",
		.owner = THIS_MODULE,
	},
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
};

static int __init msm_soc_platform_init(void)
{
	memset(&voip_info, 0, sizeof(voip_info));

	mutex_init(&voip_info.lock);
	mutex_init(&voip_info.in_lock);
	mutex_init(&voip_info.out_lock);

	spin_lock_init(&voip_info.dsp_lock);

	init_waitqueue_head(&voip_info.out_wait);

	INIT_LIST_HEAD(&voip_info.in_queue);
	INIT_LIST_HEAD(&voip_info.free_in_queue);
	INIT_LIST_HEAD(&voip_info.out_queue);
	INIT_LIST_HEAD(&voip_info.free_out_queue);

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

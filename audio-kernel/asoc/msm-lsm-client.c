/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/freezer.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/timer.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/lsm_params.h>
#include <sound/pcm_params.h>
#include <dsp/msm_audio_ion.h>
#include <dsp/q6lsm.h>
#include "msm-pcm-routing-v2.h"

#define CAPTURE_MIN_NUM_PERIODS     2
#define CAPTURE_MAX_NUM_PERIODS     8
#define CAPTURE_MAX_PERIOD_SIZE     61440
#define CAPTURE_MIN_PERIOD_SIZE     320
#define LISTEN_MAX_STATUS_PAYLOAD_SIZE 256

#define LAB_BUFFER_ALLOC 1
#define LAB_BUFFER_DEALLOC 0

#define LSM_IS_LAST_STAGE(client, stage_idx) \
	(client->num_stages == (stage_idx + 1))

static struct snd_pcm_hardware msm_pcm_hardware_capture = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =              (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE),
	.rates =		(SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_48000),
	.rate_min =             16000,
	.rate_max =             48000,
	.channels_min =         LSM_INPUT_NUM_CHANNELS_MIN,
	.channels_max =         LSM_INPUT_NUM_CHANNELS_MAX,
	.buffer_bytes_max =     CAPTURE_MAX_NUM_PERIODS *
				CAPTURE_MAX_PERIOD_SIZE,
	.period_bytes_min =	CAPTURE_MIN_PERIOD_SIZE,
	.period_bytes_max =     CAPTURE_MAX_PERIOD_SIZE,
	.periods_min =          CAPTURE_MIN_NUM_PERIODS,
	.periods_max =          CAPTURE_MAX_NUM_PERIODS,
	.fifo_size =            0,
};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	16000, 48000,
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

struct lsm_priv {
	struct snd_pcm_substream *substream;
	struct lsm_client *lsm_client;
	struct snd_lsm_event_status_v3 *event_status;
	struct snd_lsm_event_status *det_event;
	spinlock_t event_lock;
	wait_queue_head_t event_wait;
	unsigned long event_avail;
	atomic_t event_wait_stop;
	atomic_t buf_count;
	atomic_t read_abort;
	wait_queue_head_t period_wait;
	struct mutex lsm_api_lock;
	int appl_cnt;
	int dma_write;
	int xrun_count;
	int xrun_index;
	spinlock_t xrun_lock;
};

enum { /* lsm session states */
	IDLE = 0,
	RUNNING,
};

static int msm_lsm_queue_lab_buffer(struct lsm_priv *prtd, int i)
{
	int rc = 0;
	struct lsm_cmd_read cmd_read;
	struct snd_soc_pcm_runtime *rtd;

	if (!prtd || !prtd->lsm_client) {
		pr_err("%s: Invalid params prtd %pK lsm client %pK\n",
			__func__, prtd, ((!prtd) ? NULL : prtd->lsm_client));
		return -EINVAL;
	}
	if (!prtd->substream || !prtd->substream->private_data) {
		pr_err("%s: Invalid %s\n", __func__,
			(!prtd->substream) ? "substream" : "private_data");
		return -EINVAL;
	}
	rtd = prtd->substream->private_data;

	if (!prtd->lsm_client->lab_buffer ||
		i >= prtd->lsm_client->out_hw_params.period_count) {
		dev_err(rtd->dev,
			"%s: Lab buffer not setup %pK incorrect index %d period count %d\n",
			__func__, prtd->lsm_client->lab_buffer, i,
			prtd->lsm_client->out_hw_params.period_count);
		return -EINVAL;
	}
	cmd_read.buf_addr_lsw =
		lower_32_bits(prtd->lsm_client->lab_buffer[i].phys);
	cmd_read.buf_addr_msw =
		msm_audio_populate_upper_32_bits(
				prtd->lsm_client->lab_buffer[i].phys);
	cmd_read.buf_size = prtd->lsm_client->lab_buffer[i].size;
	cmd_read.mem_map_handle =
		prtd->lsm_client->lab_buffer[i].mem_map_handle;
	rc = q6lsm_read(prtd->lsm_client, &cmd_read);
	if (rc)
		dev_err(rtd->dev,
			"%s: error in queuing the lab buffer rc %d\n",
			__func__, rc);
	return rc;
}

static int lsm_lab_buffer_sanity(struct lsm_priv *prtd,
		struct lsm_cmd_read_done *read_done, int *index)
{
	int i = 0, rc = -EINVAL;
	struct snd_soc_pcm_runtime *rtd;

	if (!prtd || !read_done || !index) {
		pr_err("%s: Invalid params prtd %pK read_done %pK index %pK\n",
			__func__, prtd, read_done, index);
		return -EINVAL;
	}

	if (!prtd->substream || !prtd->substream->private_data) {
		pr_err("%s: Invalid %s\n", __func__,
			(!prtd->substream) ? "substream" : "private_data");
		return -EINVAL;
	}
	rtd = prtd->substream->private_data;

	if (!prtd->lsm_client->lab_enable || !prtd->lsm_client->lab_buffer) {
		dev_err(rtd->dev,
			"%s: Lab not enabled %d invalid lab buffer %pK\n",
			__func__, prtd->lsm_client->lab_enable,
			prtd->lsm_client->lab_buffer);
		return -EINVAL;
	}
	for (i = 0; i < prtd->lsm_client->out_hw_params.period_count; i++) {
		if ((lower_32_bits(prtd->lsm_client->lab_buffer[i].phys) ==
			read_done->buf_addr_lsw) &&
			(msm_audio_populate_upper_32_bits
				(prtd->lsm_client->lab_buffer[i].phys) ==
			read_done->buf_addr_msw) &&
			(prtd->lsm_client->lab_buffer[i].mem_map_handle ==
			read_done->mem_map_handle)) {
			dev_dbg(rtd->dev,
				"%s: Buffer found %pK memmap handle %d\n",
				__func__, &prtd->lsm_client->lab_buffer[i].phys,
			prtd->lsm_client->lab_buffer[i].mem_map_handle);
			if (read_done->total_size >
				prtd->lsm_client->lab_buffer[i].size) {
				dev_err(rtd->dev,
					"%s: Size mismatch call back size %d actual size %zd\n",
					__func__, read_done->total_size,
				prtd->lsm_client->lab_buffer[i].size);
				rc = -EINVAL;
				break;
			} else {
				*index = i;
				rc = 0;
				break;
			}
		}
	}
	return rc;
}

static void lsm_event_handler(uint32_t opcode, uint32_t token,
			      uint32_t *payload, uint16_t client_size,
				void *priv)
{
	unsigned long flags;
	struct lsm_priv *prtd = priv;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_lsm_event_status_v3 *temp;
	uint16_t status = 0;
	uint16_t payload_size = 0;
	uint16_t index = 0;
	uint32_t event_ts_lsw = 0;
	uint32_t event_ts_msw = 0;

	if (!substream || !substream->private_data) {
		pr_err("%s: Invalid %s\n", __func__,
			(!substream) ? "substream" : "private_data");
		return;
	}
	rtd = substream->private_data;

	switch (opcode) {
	case LSM_DATA_EVENT_READ_DONE: {
		int rc;
		struct lsm_cmd_read_done *read_done = (struct lsm_cmd_read_done *)payload;
		int buf_index = 0;
		unsigned long flags = 0;

		if (prtd->lsm_client->session != token ||
		    !read_done) {
			dev_err(rtd->dev,
				"%s: EVENT_READ_DONE invalid callback, session %d callback %d payload %pK",
				__func__, prtd->lsm_client->session,
				token, read_done);
			return;
		}
		if (atomic_read(&prtd->read_abort)) {
			dev_dbg(rtd->dev,
				"%s: read abort set skip data\n", __func__);
			return;
		}
		if (!lsm_lab_buffer_sanity(prtd, read_done, &buf_index)) {
			dev_dbg(rtd->dev,
				"%s: process read done index %d\n",
				__func__, buf_index);
			if (buf_index >=
				prtd->lsm_client->out_hw_params.period_count) {
				dev_err(rtd->dev,
					"%s: Invalid index %d buf_index max cnt %d\n",
					__func__, buf_index,
				prtd->lsm_client->out_hw_params.period_count);
				return;
			}
			spin_lock_irqsave(&prtd->xrun_lock, flags);
			prtd->dma_write += read_done->total_size;
			atomic_inc(&prtd->buf_count);
			snd_pcm_period_elapsed(substream);
			wake_up(&prtd->period_wait);
			if (atomic_read(&prtd->buf_count) <
				prtd->lsm_client->out_hw_params.period_count) {
				/* queue the next period buffer */
				buf_index = (buf_index + 1) %
				prtd->lsm_client->out_hw_params.period_count;
				rc = msm_lsm_queue_lab_buffer(prtd, buf_index);
				if (rc)
					dev_err(rtd->dev,
						"%s: error in queuing the lab buffer rc %d\n",
						__func__, rc);
			} else {
				dev_dbg(rtd->dev,
					"%s: xrun: further lab to be queued after read from user\n",
					 __func__);
				if (!prtd->xrun_count)
					prtd->xrun_index = buf_index;
				(prtd->xrun_count)++;
			}
			spin_unlock_irqrestore(&prtd->xrun_lock, flags);
		} else
			dev_err(rtd->dev, "%s: Invalid lab buffer returned by dsp\n",
				__func__);
		break;
	}

	case LSM_SESSION_EVENT_DETECTION_STATUS:
                if (client_size < 3 * sizeof(uint8_t)) {
			dev_err(rtd->dev,
					"%s: client_size has invalid size[%d]\n",
					__func__, client_size);
			return;
		}
		status = (uint16_t)((uint8_t *)payload)[0];
		payload_size = (uint16_t)((uint8_t *)payload)[2];
		index = 4;
		dev_dbg(rtd->dev,
			"%s: event detect status = %d payload size = %d\n",
			__func__, status, payload_size);
		break;

	case LSM_SESSION_EVENT_DETECTION_STATUS_V2:
		if (client_size < 2 * sizeof(uint8_t)) {
			dev_err(rtd->dev,
					"%s: client_size has invalid size[%d]\n",
					__func__, client_size);
			return;
		}
		status = (uint16_t)((uint8_t *)payload)[0];
		payload_size = (uint16_t)((uint8_t *)payload)[1];
		index = 2;
		dev_dbg(rtd->dev,
			"%s: event detect status = %d payload size = %d\n",
			__func__, status, payload_size);
		break;

	case LSM_SESSION_EVENT_DETECTION_STATUS_V3:
		if (client_size < 2 * (sizeof(uint32_t) + sizeof(uint8_t))) {
			dev_err(rtd->dev,
					"%s: client_size has invalid size[%d]\n",
					__func__, client_size);
			return;
		}
		event_ts_lsw = ((uint32_t *)payload)[0];
		event_ts_msw = ((uint32_t *)payload)[1];
		status = (uint16_t)((uint8_t *)payload)[8];
		payload_size = (uint16_t)((uint8_t *)payload)[9];
		index = 10;
		dev_dbg(rtd->dev,
			"%s: ts_msw = %u, ts_lsw = %u, event detect status = %d payload size = %d\n",
			__func__, event_ts_msw, event_ts_lsw, status,
			payload_size);
		break;

	case LSM_SESSION_DETECTION_ENGINE_GENERIC_EVENT: {
		struct snd_lsm_event_status *tmp;
		if (client_size < 2 * sizeof(uint16_t)) {
			dev_err(rtd->dev,
					"%s: client_size has invalid size[%d]\n",
					__func__, client_size);
			return;
		}


		status = ((uint16_t *)payload)[0];
		payload_size = ((uint16_t *)payload)[1];

		spin_lock_irqsave(&prtd->event_lock, flags);
		tmp = krealloc(prtd->det_event,
			       sizeof(struct snd_lsm_event_status) +
			       payload_size, GFP_ATOMIC);
		if (!tmp) {
			spin_unlock_irqrestore(&prtd->event_lock, flags);
			dev_err(rtd->dev,
				"%s: Failed to allocate memory for %s, size = %zu\n",
				__func__,
				"LSM_SESSION_DETECTION_ENGINE_GENERIC_EVENT",
				sizeof(struct snd_lsm_event_status) +
				payload_size);
			return;
		}

		prtd->det_event = tmp;
		prtd->det_event->status = status;
		prtd->det_event->payload_size = payload_size;
		if (client_size >= payload_size + 4) {
			memcpy(prtd->det_event->payload,
				&((uint8_t *)payload)[4], payload_size);
		} else {
			spin_unlock_irqrestore(&prtd->event_lock, flags);
			dev_err(rtd->dev,
				"%s: Failed to copy memory with invalid size = %d\n",
				__func__, payload_size);
			return;
		}
		prtd->event_avail = 1;
		spin_unlock_irqrestore(&prtd->event_lock, flags);
		wake_up(&prtd->event_wait);

		if (substream->timer_running)
			snd_timer_interrupt(substream->timer, 1);

		dev_dbg(rtd->dev,
			"%s: Generic det event status = %d payload size = %d\n",
			__func__, prtd->det_event->status,
			prtd->det_event->payload_size);
		break;
	}

	default:
		break;
	}

	if (opcode == LSM_SESSION_EVENT_DETECTION_STATUS ||
		opcode == LSM_SESSION_EVENT_DETECTION_STATUS_V2 ||
		opcode == LSM_SESSION_EVENT_DETECTION_STATUS_V3) {
		spin_lock_irqsave(&prtd->event_lock, flags);
		temp = krealloc(prtd->event_status,
				sizeof(struct snd_lsm_event_status_v3) +
				payload_size, GFP_ATOMIC);
		if (!temp) {
			dev_err(rtd->dev, "%s: no memory for event status\n",
				__func__);
			return;
		}
		/*
		 * event status timestamp will be non-zero and valid if
		 * opcode is LSM_SESSION_EVENT_DETECTION_STATUS_V3
		 */
		prtd->event_status = temp;
		prtd->event_status->timestamp_lsw = event_ts_lsw;
		prtd->event_status->timestamp_msw = event_ts_msw;
		prtd->event_status->status = status;
		prtd->event_status->payload_size = payload_size;

		if (likely(prtd->event_status)) {
			if (client_size >= (payload_size + index)) {
				memcpy(prtd->event_status->payload,
					&((uint8_t *)payload)[index],
					payload_size);
				prtd->event_avail = 1;
				spin_unlock_irqrestore(&prtd->event_lock, flags);
				wake_up(&prtd->event_wait);
			} else {
				spin_unlock_irqrestore(&prtd->event_lock, flags);
				dev_err(rtd->dev,
						"%s: Failed to copy memory with invalid size = %d\n",
						__func__, payload_size);
				return;
			}
		} else {
			spin_unlock_irqrestore(&prtd->event_lock, flags);
			dev_err(rtd->dev,
				"%s: Couldn't allocate %d bytes of memory\n",
				__func__, payload_size);
		}
		if (substream->timer_running)
			snd_timer_interrupt(substream->timer, 1);
	}
}

static int msm_lsm_lab_buffer_alloc(struct lsm_priv *lsm, int alloc)
{
	int ret = 0;
	struct snd_dma_buffer *dma_buf = NULL;

	if (!lsm) {
		pr_err("%s: Invalid param lsm %pK\n", __func__, lsm);
		return -EINVAL;
	}
	if (alloc) {
		if (!lsm->substream) {
			pr_err("%s: substream is NULL\n", __func__);
			return -EINVAL;
		}
		ret = q6lsm_lab_buffer_alloc(lsm->lsm_client, alloc);
		if (ret) {
			pr_err("%s: alloc lab buffer failed ret %d\n",
				__func__, ret);
			goto exit;
		}
		dma_buf = &lsm->substream->dma_buffer;
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = lsm->substream->pcm->card->dev;
		dma_buf->private_data = NULL;
		dma_buf->area = lsm->lsm_client->lab_buffer[0].data;
		dma_buf->addr = lsm->lsm_client->lab_buffer[0].phys;
		dma_buf->bytes = lsm->lsm_client->out_hw_params.buf_sz *
		lsm->lsm_client->out_hw_params.period_count;
		snd_pcm_set_runtime_buffer(lsm->substream, dma_buf);
	} else {
		ret = q6lsm_lab_buffer_alloc(lsm->lsm_client, alloc);
		if (ret)
			pr_err("%s: free lab buffer failed ret %d\n",
				__func__, ret);
		kfree(lsm->lsm_client->lab_buffer);
		lsm->lsm_client->lab_buffer = NULL;
	}
exit:
	return ret;
}

static int msm_lsm_get_conf_levels(struct lsm_client *client,
				   u8 *conf_levels_ptr)
{
	int rc = 0;

	if (client->num_confidence_levels == 0) {
		pr_debug("%s: no confidence levels provided\n",
			__func__);
		client->confidence_levels = NULL;
		goto done;
	}

	client->confidence_levels =
		kzalloc((sizeof(uint8_t) * client->num_confidence_levels),
			 GFP_KERNEL);
	if (!client->confidence_levels) {
		pr_err("%s: No memory for confidence\n"
			"levels num of level from user = %d\n",
			__func__, client->num_confidence_levels);
			rc = -ENOMEM;
			goto done;
	}

	if (copy_from_user(client->confidence_levels,
			   conf_levels_ptr,
			   client->num_confidence_levels)) {
		pr_err("%s: copy from user failed, size = %d\n",
		       __func__, client->num_confidence_levels);
		rc = -EFAULT;
		goto copy_err;
	}

	return rc;

copy_err:
	kfree(client->confidence_levels);
	client->confidence_levels = NULL;
done:
	return rc;

}

static int msm_lsm_set_epd(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rc = 0;
	struct snd_lsm_ep_det_thres epd_th;

	if (p_info->param_size != sizeof(epd_th)) {
		dev_err(rtd->dev,
			"%s: Invalid param_size %d\n",
			__func__, p_info->param_size);
		rc = -EINVAL;
		goto done;
	}

	if (copy_from_user(&epd_th, p_info->param_data,
			   p_info->param_size)) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed, size = %d\n",
			__func__, p_info->param_size);
		rc = -EFAULT;
		goto done;
	}

	rc = q6lsm_set_one_param(prtd->lsm_client, p_info,
				 &epd_th, LSM_ENDPOINT_DETECT_THRESHOLD);
	if (rc)
		dev_err(rtd->dev,
			"%s: Failed to set epd param, err = %d\n",
			__func__, rc);
done:
	return rc;
}

static int msm_lsm_set_mode(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_lsm_detect_mode mode;
	int rc = 0;

	if (p_info->param_size != sizeof(mode)) {
		dev_err(rtd->dev,
			"%s: Invalid param_size %d\n",
			__func__, p_info->param_size);
		rc = -EINVAL;
		goto done;
	}

	if (copy_from_user(&mode, p_info->param_data,
			   sizeof(mode))) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed, size = %zd\n",
			__func__, sizeof(mode));
		rc = -EFAULT;
		goto done;
	}

	rc = q6lsm_set_one_param(prtd->lsm_client, p_info,
				 &mode, LSM_OPERATION_MODE);
	if (rc)
		dev_err(rtd->dev,
			"%s: Failed to set det_mode param, err = %d\n",
			__func__, rc);
done:
	return rc;
}

static int msm_lsm_set_gain(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_lsm_gain gain;
	int rc = 0;

	if (p_info->param_size != sizeof(gain)) {
		dev_err(rtd->dev,
			"%s: Invalid param_size %d\n",
			__func__, p_info->param_size);
		rc = -EINVAL;
		goto done;
	}

	if (copy_from_user(&gain, p_info->param_data,
			   sizeof(gain))) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed, size = %zd\n",
			__func__, sizeof(gain));
		rc = -EFAULT;
		goto done;
	}

	rc = q6lsm_set_one_param(prtd->lsm_client, p_info,
				 &gain, LSM_GAIN);
	if (rc)
		dev_err(rtd->dev,
			"%s: Failed to set det_mode param, err = %d\n",
			__func__, rc);
done:
	return rc;
}

static int msm_lsm_set_conf(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rc = 0;

	if (p_info->param_size > MAX_NUM_CONFIDENCE) {
		dev_err(rtd->dev,
			"%s: invalid confidence levels %d\n",
			__func__, p_info->param_size);
		return -EINVAL;
	}

	prtd->lsm_client->num_confidence_levels =
			p_info->param_size;
	rc = msm_lsm_get_conf_levels(prtd->lsm_client,
				     p_info->param_data);
	if (rc) {
		dev_err(rtd->dev,
			"%s: get_conf_levels failed, err = %d\n",
			__func__, rc);
		return rc;
	}

	rc = q6lsm_set_one_param(prtd->lsm_client, p_info,
				 prtd->lsm_client->confidence_levels,
				 LSM_MIN_CONFIDENCE_LEVELS);
	if (rc)
		dev_err(rtd->dev,
			"%s: Failed to set min_conf_levels, err = %d\n",
			__func__, rc);

	return rc;
}

static int msm_lsm_reg_model(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rc = 0;
	struct lsm_sound_model *sm = NULL;
	size_t offset = sizeof(union param_hdrs);

	rc = q6lsm_snd_model_buf_alloc(prtd->lsm_client,
				       p_info->param_size, p_info);
	if (rc) {
		dev_err(rtd->dev,
			"%s: snd_model buf alloc failed, size = %d\n",
			__func__, p_info->param_size);
		return rc;
	}

	q6lsm_sm_set_param_data(prtd->lsm_client, p_info, &offset);

	/*
	 * For set_param, advance the sound model data with the
	 * number of bytes required by param_data.
	 */

	sm = &prtd->lsm_client->stage_cfg[p_info->stage_idx].sound_model;
	if (copy_from_user((u8 *)sm->data + offset,
			   p_info->param_data, p_info->param_size)) {
		dev_err(rtd->dev,
			"%s: copy_from_user for snd_model failed, size = %d\n",
			__func__, p_info->param_size);
		rc = -EFAULT;
		goto err_copy;
	}
	rc = q6lsm_set_one_param(prtd->lsm_client, p_info, NULL,
				 LSM_REG_SND_MODEL);
	if (rc) {
		dev_err(rtd->dev,
			"%s: Failed to set sound_model, err = %d\n",
			__func__, rc);
		goto err_copy;
	}
	return rc;

err_copy:
	q6lsm_snd_model_buf_free(prtd->lsm_client, p_info);
	return rc;
}

static int msm_lsm_dereg_model(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rc = 0;

	rc = q6lsm_set_one_param(prtd->lsm_client, p_info,
				 NULL, LSM_DEREG_SND_MODEL);
	if (rc)
		dev_err(rtd->dev,
			"%s: Failed to set det_mode param, err = %d\n",
			__func__, rc);

	q6lsm_snd_model_buf_free(prtd->lsm_client, p_info);

	return rc;
}

static int msm_lsm_set_custom(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	u8 *data;
	int rc = 0;

	data = kzalloc(p_info->param_size, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (copy_from_user(data, p_info->param_data,
			   p_info->param_size)) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed for custom params, size = %d\n",
			__func__, p_info->param_size);
		rc = -EFAULT;
		goto err_ret;
	}

	rc = q6lsm_set_one_param(prtd->lsm_client, p_info,
				 data, LSM_CUSTOM_PARAMS);
	if (rc)
		dev_err(rtd->dev,
			"%s: Failed to set custom param, err = %d\n",
			__func__, rc);

err_ret:
	kfree(data);
	return rc;
}

static int msm_lsm_check_and_set_lab_controls(struct snd_pcm_substream *substream,
			u32 enable, struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct lsm_hw_params *out_hw_params = &prtd->lsm_client->out_hw_params;
	u8 chmap[out_hw_params->num_chs];
	u32 ch_idx;
	int rc = 0, stage_idx = p_info->stage_idx;

	if (prtd->lsm_client->stage_cfg[stage_idx].lab_enable == enable) {
		dev_dbg(rtd->dev, "%s: Lab for session %d, stage %d already %s\n",
				__func__, prtd->lsm_client->session,
				stage_idx, enable ? "enabled" : "disabled");
		return rc;
	}

	rc = q6lsm_lab_control(prtd->lsm_client, enable, p_info);
	if (rc) {
		dev_err(rtd->dev, "%s: Failed to set lab_control param, err = %d\n",
			__func__, rc);
		return rc;
	} else {
		if (LSM_IS_LAST_STAGE(prtd->lsm_client, stage_idx)) {
			rc = msm_lsm_lab_buffer_alloc(prtd,
					enable ? LAB_BUFFER_ALLOC : LAB_BUFFER_DEALLOC);
			if (rc) {
				dev_err(rtd->dev,
					"%s: msm_lsm_lab_buffer_alloc failed rc %d for %s\n",
					__func__, rc, enable ? "ALLOC" : "DEALLOC");
				return rc;
			} else {
				/* set client level flag based on last stage control */
				prtd->lsm_client->lab_enable = enable;
			}
		}
		if (!rc)
			prtd->lsm_client->stage_cfg[stage_idx].lab_enable = enable;
	}

	memset(chmap, 0, out_hw_params->num_chs);
	/*
	 * First channel to be read from lab is always the
	 * best channel (0xff). For second channel onwards,
	 * the channel indices are 0, 1, .. etc
	 */
	chmap[0] = 0xFF;
	for (ch_idx = 1; ch_idx < out_hw_params->num_chs; ch_idx++)
		chmap[ch_idx] = ch_idx - 1;

	rc = q6lsm_lab_out_ch_cfg(prtd->lsm_client, chmap, p_info);
	if (rc)
		dev_err(rtd->dev, "%s: Failed to set lab out ch cfg %d\n",
			__func__, rc);

	return rc;
}

static int msm_lsm_set_lab_control(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_lsm_lab_control lab_ctrl;
	int rc = 0;

	if (p_info->param_size != sizeof(lab_ctrl))
		return -EINVAL;

	if (prtd->lsm_client->started) {
		dev_err(rtd->dev, "%s: lab control sent after start\n", __func__);
		return -EAGAIN;
	}

	if (copy_from_user(&lab_ctrl, p_info->param_data,
			   p_info->param_size)) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed for lab_control params, size = %d\n",
			__func__, p_info->param_size);
		return  -EFAULT;
	}

	rc = msm_lsm_check_and_set_lab_controls(substream, lab_ctrl.enable, p_info);
	return rc;
}

static int msm_lsm_set_poll_enable(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_lsm_poll_enable poll_enable;
	int rc = 0;

	if (p_info->param_size != sizeof(poll_enable)) {
		dev_err(rtd->dev,
			"%s: Invalid param_size %d\n",
			__func__, p_info->param_size);
		rc = -EINVAL;
		goto done;
	}

	if (copy_from_user(&poll_enable, p_info->param_data,
			   sizeof(poll_enable))) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed, size = %zd\n",
			__func__, sizeof(poll_enable));
		rc = -EFAULT;
		goto done;
	}

	if (prtd->lsm_client->poll_enable == poll_enable.poll_en) {
		dev_dbg(rtd->dev,
			"%s: Polling for session %d already %s\n",
			__func__, prtd->lsm_client->session,
			(poll_enable.poll_en ? "enabled" : "disabled"));
		rc = 0;
		goto done;
	}

	rc = q6lsm_set_one_param(prtd->lsm_client, p_info,
				 &poll_enable, LSM_POLLING_ENABLE);
	if (!rc) {
		prtd->lsm_client->poll_enable = poll_enable.poll_en;
	} else {
		dev_err(rtd->dev,
			"%s: Failed to set poll enable, err = %d\n",
			__func__, rc);
	}
done:
	return rc;
}

static int msm_lsm_set_det_event_type(struct snd_pcm_substream *substream,
				      struct lsm_params_info_v2 *p_info)
{
	struct snd_lsm_det_event_type det_event_type;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rc = 0;

	if (p_info->param_size != sizeof(det_event_type)) {
		dev_err(rtd->dev,
			"%s: Invalid param_size %d\n",
			__func__, p_info->param_size);
		rc = -EINVAL;
		goto done;
	}

	if (copy_from_user(&det_event_type, p_info->param_data,
			   sizeof(det_event_type))) {
		dev_err(rtd->dev,
			"%s: copy_from_user failed, size = %zd\n",
			__func__, sizeof(det_event_type));
		rc = -EFAULT;
		goto done;
	}

	rc = q6lsm_set_one_param(prtd->lsm_client, p_info,
				 &det_event_type, LSM_DET_EVENT_TYPE);
	if (!rc)
		prtd->lsm_client->event_type = det_event_type.event_type;
	else
		dev_err(rtd->dev,
			"%s: Failed to set detection event type %s, err = %d\n",
			__func__, (det_event_type.event_type ?
			"LSM_DET_EVENT_TYPE_GENERIC" :
			"LSM_DET_EVENT_TYPE_LEGACY"), rc);
done:
	return rc;
}

static int msm_lsm_process_params(struct snd_pcm_substream *substream,
		struct lsm_params_info_v2 *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rc = 0;

	dev_dbg(rtd->dev,
		"%s: mid=0x%x, pid=0x%x, iid=0x%x, stage_idx=%d, size=0x%x, type=%d\n",
		__func__, p_info->module_id, p_info->param_id, p_info->instance_id,
		p_info->stage_idx, p_info->param_size, p_info->param_type);

	if (!prtd->lsm_client ||
		prtd->lsm_client->num_stages <= p_info->stage_idx) {
		dev_err(rtd->dev,
			"%s: invalid stage_idx(%d) for client(%p) having num_stages(%d)\n",
			__func__, p_info->stage_idx, prtd->lsm_client,
			prtd->lsm_client ? prtd->lsm_client->num_stages : 0);
		return -EINVAL;
	}

	switch (p_info->param_type) {
	case LSM_ENDPOINT_DETECT_THRESHOLD:
		rc = msm_lsm_set_epd(substream, p_info);
		break;
	case LSM_OPERATION_MODE:
		rc = msm_lsm_set_mode(substream, p_info);
		break;
	case LSM_GAIN:
		rc = msm_lsm_set_gain(substream, p_info);
		break;
	case LSM_MIN_CONFIDENCE_LEVELS:
		rc = msm_lsm_set_conf(substream, p_info);
		break;
	case LSM_REG_SND_MODEL:
		rc = msm_lsm_reg_model(substream, p_info);
		break;
	case LSM_DEREG_SND_MODEL:
		rc = msm_lsm_dereg_model(substream, p_info);
		break;
	case LSM_CUSTOM_PARAMS:
		rc = msm_lsm_set_custom(substream, p_info);
		break;
	case LSM_POLLING_ENABLE:
		rc = msm_lsm_set_poll_enable(substream, p_info);
		break;
	case LSM_DET_EVENT_TYPE:
		rc = msm_lsm_set_det_event_type(substream, p_info);
		break;
	case LSM_LAB_CONTROL:
		rc = msm_lsm_set_lab_control(substream, p_info);
		break;
	default:
		dev_err(rtd->dev,
			"%s: Invalid param_type %d\n",
			__func__, p_info->param_type);
		rc = -EINVAL;
		break;
	}
	if (rc) {
		pr_err("%s: set_param fail for param_type %d\n",
			__func__, p_info->param_type);
	}

	return rc;
}

static int msm_lsm_start_lab_buffer(struct lsm_priv *prtd, uint16_t status)
{
	struct lsm_client *lsm_client = prtd->lsm_client;
	int rc = 0;

	if (lsm_client && lsm_client->lab_enable &&
	    !lsm_client->lab_started &&
	    status == LSM_VOICE_WAKEUP_STATUS_DETECTED) {
		atomic_set(&prtd->read_abort, 0);
		atomic_set(&prtd->buf_count, 0);
		prtd->appl_cnt = 0;
		prtd->dma_write = 0;
		prtd->xrun_count = 0;
		prtd->xrun_index = 0;

		rc = msm_lsm_queue_lab_buffer(prtd, 0);
		if (rc)
			pr_err("%s: Queue buffer failed for lab rc = %d\n",
				__func__, rc);
		else
			prtd->lsm_client->lab_started = true;
	}

	return rc;
}

static int msm_lsm_ioctl_shared(struct snd_pcm_substream *substream,
			 unsigned int cmd, void *arg)
{
	struct snd_soc_pcm_runtime *rtd;
	unsigned long flags;
	int ret;
	struct snd_lsm_sound_model_v2 snd_model_v2;
	struct snd_lsm_session_data session_data;
	struct snd_lsm_session_data_v2 ses_data_v2 = {0};
	int rc = 0, stage_idx;
	int xchg = 0;
	struct snd_pcm_runtime *runtime;
	struct lsm_priv *prtd;
	struct snd_lsm_detection_params det_params;
	uint8_t *confidence_level = NULL;
	uint32_t max_detection_stages_supported = LSM_MAX_STAGES_PER_SESSION;

	if (!substream || !substream->private_data) {
		pr_err("%s: Invalid %s\n", __func__,
			(!substream) ? "substream" : "private_data");
		return -EINVAL;
	}

	runtime = substream->runtime;
	prtd = runtime->private_data;
	rtd = substream->private_data;

	switch (cmd) {
	case SNDRV_LSM_SET_SESSION_DATA:
	case SNDRV_LSM_SET_SESSION_DATA_V2:

		if (cmd == SNDRV_LSM_SET_SESSION_DATA) {
			dev_dbg(rtd->dev, "%s: set session data\n", __func__);
			rc = copy_from_user(&session_data, arg, sizeof(session_data));
			if (!rc) {
				ses_data_v2.app_id = session_data.app_id;
				ses_data_v2.num_stages = 1;
			}
		} else {
			dev_dbg(rtd->dev, "%s: set session data_v2\n", __func__);
			rc = copy_from_user(&ses_data_v2, arg, sizeof(ses_data_v2));
		}
		if (rc) {
			dev_err(rtd->dev, "%s: %s: copy_from_user failed\n",
				__func__, "LSM_SET_SESSION_DATA(_V2)");
			return -EFAULT;
		}

		if (ses_data_v2.app_id != LSM_VOICE_WAKEUP_APP_ID_V2) {
			dev_err(rtd->dev,
				"%s:Invalid App id %d for Listen client\n",
			       __func__, ses_data_v2.app_id);
			rc = -EINVAL;
			break;
		}

		/*
		 * Before validating num_stages from user argument.
		 * Check ADSP support for multi-stage session,
		 * and reset max_detection_stages_supported to "1" if required.
		 */
		if (!q6lsm_adsp_supports_multi_stage_detection()) {
			dev_dbg(rtd->dev,
				"%s: multi-stage session not supported by adsp\n", __func__);
			max_detection_stages_supported = 1;
		}

		if (ses_data_v2.num_stages <= 0 ||
			ses_data_v2.num_stages > max_detection_stages_supported) {
			dev_err(rtd->dev,
				"%s: Unsupported number of stages req(%d)/max(%d)\n",
				 __func__, ses_data_v2.num_stages,
				max_detection_stages_supported);
			rc = -EINVAL;
			break;
		}

		prtd->lsm_client->app_id = ses_data_v2.app_id;
		prtd->lsm_client->num_stages = ses_data_v2.num_stages;
		for (stage_idx = LSM_STAGE_INDEX_FIRST;
			stage_idx < ses_data_v2.num_stages; stage_idx++) {
			prtd->lsm_client->stage_cfg[stage_idx].app_type =
				ses_data_v2.stage_info[stage_idx].app_type;
			prtd->lsm_client->stage_cfg[stage_idx].lpi_enable =
				ses_data_v2.stage_info[stage_idx].lpi_enable;
		}

		ret = q6lsm_open(prtd->lsm_client, ses_data_v2.app_id);
		if (ret < 0) {
			dev_err(rtd->dev,
				"%s: lsm open failed, %d\n",
				__func__, ret);
			return ret;
		}
		prtd->lsm_client->opened = true;
		dev_dbg(rtd->dev, "%s: Session_ID = %d, APP ID = %d, Num stages %d\n",
			__func__,
			prtd->lsm_client->session,
			prtd->lsm_client->app_id,
			prtd->lsm_client->num_stages);
		break;
	case SNDRV_LSM_REG_SND_MODEL_V2: {
		/*
		 * With multi-stage support sm buff allocation/free usage param info
		 * to check stage index for which this sound model is being set, and
		 * to check whether sm data is sent using set param command or not.
		 * Hence, set param ids to '0' to indicate allocation is for legacy
		 * reg_sm cmd, where buffer for param header need not be allocated,
		 * also set stage index to LSM_STAGE_INDEX_FIRST.
		 */
		struct lsm_params_info_v2 p_info = {0};
		p_info.stage_idx = LSM_STAGE_INDEX_FIRST;

		dev_dbg(rtd->dev, "%s: Registering sound model V2\n",
			__func__);
		memcpy(&snd_model_v2, arg,
		       sizeof(struct snd_lsm_sound_model_v2));
		if (snd_model_v2.num_confidence_levels >
		    MAX_NUM_CONFIDENCE) {
			dev_err(rtd->dev,
				"%s: Invalid conf_levels = %d, maximum allowed = %d\n",
				__func__, snd_model_v2.num_confidence_levels,
				MAX_NUM_CONFIDENCE);
			rc = -EINVAL;
			break;
		}
		rc = q6lsm_snd_model_buf_alloc(prtd->lsm_client,
					snd_model_v2.data_size, &p_info);
		if (rc) {
			dev_err(rtd->dev,
				"%s: q6lsm buffer alloc failed V2, size %d\n",
			       __func__, snd_model_v2.data_size);
			break;
		}
		if (copy_from_user(
				prtd->lsm_client->stage_cfg[p_info.stage_idx].sound_model.data,
				snd_model_v2.data, snd_model_v2.data_size)) {
			dev_err(rtd->dev,
				"%s: copy from user data failed\n"
			       "data %pK size %d\n", __func__,
			       snd_model_v2.data, snd_model_v2.data_size);
			q6lsm_snd_model_buf_free(prtd->lsm_client, &p_info);
			rc = -EFAULT;
			break;
		}

		dev_dbg(rtd->dev, "SND Model Magic no byte[0] %x,\n"
			 "byte[1] %x, byte[2] %x byte[3] %x\n",
			 snd_model_v2.data[0], snd_model_v2.data[1],
			 snd_model_v2.data[2], snd_model_v2.data[3]);
		prtd->lsm_client->num_confidence_levels =
			snd_model_v2.num_confidence_levels;

		rc = msm_lsm_get_conf_levels(prtd->lsm_client,
				snd_model_v2.confidence_level);
		if (rc) {
			dev_err(rtd->dev,
				"%s: get_conf_levels failed, err = %d\n",
				__func__, rc);
			break;
		}

		rc = q6lsm_register_sound_model(prtd->lsm_client,
					snd_model_v2.detection_mode,
					snd_model_v2.detect_failure);
		if (rc < 0) {
			dev_err(rtd->dev,
				"%s: Register snd Model v2 failed =%d\n",
			       __func__, rc);
			kfree(confidence_level);
			q6lsm_snd_model_buf_free(prtd->lsm_client, &p_info);
		}

		kfree(prtd->lsm_client->confidence_levels);
		prtd->lsm_client->confidence_levels = NULL;
		break;
	}
	case SNDRV_LSM_SET_PARAMS:
		dev_dbg(rtd->dev, "%s: set_params\n", __func__);
		memcpy(&det_params, arg,
			sizeof(det_params));
		if (det_params.num_confidence_levels >
		    MAX_NUM_CONFIDENCE) {
			rc = -EINVAL;
			break;
		}

		prtd->lsm_client->num_confidence_levels =
			det_params.num_confidence_levels;

		rc = msm_lsm_get_conf_levels(prtd->lsm_client,
				det_params.conf_level);
		if (rc) {
			dev_err(rtd->dev,
				"%s: Failed to get conf_levels, err = %d\n",
				__func__, rc);
			break;
		}

		rc = q6lsm_set_data(prtd->lsm_client,
			       det_params.detect_mode,
			       det_params.detect_failure);
		if (rc)
			dev_err(rtd->dev,
				"%s: Failed to set params, err = %d\n",
				__func__, rc);

		kfree(prtd->lsm_client->confidence_levels);
		prtd->lsm_client->confidence_levels = NULL;

		break;

	case SNDRV_LSM_DEREG_SND_MODEL:
		dev_dbg(rtd->dev, "%s: Deregistering sound model\n",
			__func__);
		rc = q6lsm_deregister_sound_model(prtd->lsm_client);
		if (rc)
			dev_err(rtd->dev,
				"%s: Sound model de-register failed, err = %d\n",
				__func__, rc);
		break;

	case SNDRV_LSM_EVENT_STATUS:
	case SNDRV_LSM_EVENT_STATUS_V3: {
		uint32_t ts_lsw, ts_msw;
		uint16_t status = 0, payload_size = 0;

		dev_dbg(rtd->dev, "%s: Get event status\n", __func__);
		atomic_set(&prtd->event_wait_stop, 0);

		/*
		 * Release the api lock before wait to allow
		 * other IOCTLs to be invoked while waiting
		 * for event
		 */
		mutex_unlock(&prtd->lsm_api_lock);
		rc = wait_event_freezable(prtd->event_wait,
				(cmpxchg(&prtd->event_avail, 1, 0) ||
				 (xchg = atomic_cmpxchg(&prtd->event_wait_stop,
							1, 0))));
		mutex_lock(&prtd->lsm_api_lock);
		dev_dbg(rtd->dev, "%s: wait_event_freezable %d event_wait_stop %d\n",
			 __func__, rc, xchg);
		if (!rc && !xchg) {
			dev_dbg(rtd->dev, "%s: New event available %ld\n",
				__func__, prtd->event_avail);
			spin_lock_irqsave(&prtd->event_lock, flags);

			if (prtd->event_status) {
				payload_size = prtd->event_status->payload_size;
				ts_lsw = prtd->event_status->timestamp_lsw;
				ts_msw = prtd->event_status->timestamp_msw;
				status = prtd->event_status->status;
				spin_unlock_irqrestore(&prtd->event_lock,
						       flags);
			} else {
				spin_unlock_irqrestore(&prtd->event_lock,
						       flags);
				rc = -EINVAL;
				dev_err(rtd->dev,
					"%s: prtd->event_status is NULL\n",
					__func__);
				break;
			}

			if (cmd == SNDRV_LSM_EVENT_STATUS) {
				struct snd_lsm_event_status *user = arg;

				if (user->payload_size < payload_size) {
					dev_dbg(rtd->dev,
						"%s: provided %d bytes isn't enough, needs %d bytes\n",
						__func__, user->payload_size,
						payload_size);
					rc = -ENOMEM;
				} else {
					user->status = status;
					user->payload_size = payload_size;
					memcpy(user->payload,
						prtd->event_status->payload,
						payload_size);
				}
			} else {
				struct snd_lsm_event_status_v3 *user_v3 = arg;

				if (user_v3->payload_size < payload_size) {
					dev_dbg(rtd->dev,
						"%s: provided %d bytes isn't enough, needs %d bytes\n",
						__func__, user_v3->payload_size,
						payload_size);
					rc = -ENOMEM;
				} else {
					user_v3->timestamp_lsw = ts_lsw;
					user_v3->timestamp_msw = ts_msw;
					user_v3->status = status;
					user_v3->payload_size = payload_size;
					memcpy(user_v3->payload,
						prtd->event_status->payload,
						payload_size);
				}
			}

			if (!rc)
				rc = msm_lsm_start_lab_buffer(prtd, status);
		} else if (xchg) {
			dev_dbg(rtd->dev, "%s: Wait aborted\n", __func__);
			rc = 0;
		}
		break;
	}

	case SNDRV_LSM_GENERIC_DET_EVENT: {
		struct snd_lsm_event_status *user = arg;
		uint16_t status = 0;
		uint16_t payload_size = 0;

		dev_dbg(rtd->dev,
			"%s: SNDRV_LSM_GENERIC_DET_EVENT\n", __func__);

		atomic_set(&prtd->event_wait_stop, 0);

		/*
		 * Release the api lock before wait to allow
		 * other IOCTLs to be invoked while waiting
		 * for event
		 */
		mutex_unlock(&prtd->lsm_api_lock);
		rc = wait_event_freezable(prtd->event_wait,
				(cmpxchg(&prtd->event_avail, 1, 0) ||
				(xchg = atomic_cmpxchg(&prtd->event_wait_stop,
							1, 0))));
		mutex_lock(&prtd->lsm_api_lock);

		dev_dbg(rtd->dev, "%s: wait_event_freezable %d event_wait_stop %d\n",
			 __func__, rc, xchg);

		if (!rc && !xchg) {
			dev_dbg(rtd->dev, "%s: %s: New event available %ld\n",
				__func__, "SNDRV_LSM_GENERIC_DET_EVENT",
				prtd->event_avail);

			spin_lock_irqsave(&prtd->event_lock, flags);

			if (prtd->det_event) {
				payload_size = prtd->det_event->payload_size;
				status = prtd->det_event->status;
				spin_unlock_irqrestore(&prtd->event_lock,
						       flags);
			} else {
				spin_unlock_irqrestore(&prtd->event_lock,
						       flags);
				dev_err(rtd->dev,
					"%s: %s: prtd->event_status is NULL\n",
					__func__,
					"SNDRV_LSM_GENERIC_DET_EVENT");
				rc = -EINVAL;
				break;
			}

			if (user->payload_size < payload_size) {
				dev_err(rtd->dev,
					"%s: provided %d bytes isn't enough, needs %d bytes\n",
					__func__, user->payload_size,
					payload_size);
				rc = -ENOMEM;
				break;
			}
			user->status = status;
			user->payload_size = payload_size;
			memcpy(user->payload, prtd->det_event->payload,
			       payload_size);

			rc = msm_lsm_start_lab_buffer(prtd, status);
		} else if (xchg) {
			dev_dbg(rtd->dev, "%s: %s: Wait aborted\n",
				__func__, "SNDRV_LSM_GENERIC_DET_EVENT");
			rc = 0;
		}
		break;
	}

	case SNDRV_LSM_ABORT_EVENT:
		dev_dbg(rtd->dev, "%s: Aborting event status wait\n",
			__func__);
		atomic_set(&prtd->event_wait_stop, 1);
		wake_up(&prtd->event_wait);
		break;

	case SNDRV_LSM_START:
		dev_dbg(rtd->dev, "%s: Starting LSM client session\n",
			__func__);
		if (!prtd->lsm_client->started) {
			ret = q6lsm_start(prtd->lsm_client, true);
			if (!ret) {
				prtd->lsm_client->started = true;
				dev_dbg(rtd->dev, "%s: LSM client session started\n",
					 __func__);
			}
		}
		break;

	case SNDRV_LSM_STOP: {
		dev_dbg(rtd->dev,
			"%s: Stopping LSM client session\n",
			__func__);
		if (prtd->lsm_client->started) {
			if (prtd->lsm_client->lab_enable) {
				atomic_set(&prtd->read_abort, 1);
				if (prtd->lsm_client->lab_started) {
					ret = q6lsm_stop_lab(prtd->lsm_client);
					if (ret)
						dev_err(rtd->dev,
							"%s: stop lab failed ret %d\n",
							__func__, ret);
					prtd->lsm_client->lab_started = false;
				}
			}
			ret = q6lsm_stop(prtd->lsm_client, true);
			if (!ret)
				dev_dbg(rtd->dev,
					"%s: LSM client session stopped %d\n",
					__func__, ret);
			prtd->lsm_client->started = false;
		}
		break;
	}
	case SNDRV_LSM_LAB_CONTROL: {
		u32 enable = 0;
		struct lsm_params_info_v2 p_info = {0};

		if (prtd->lsm_client->num_stages > 1) {
			dev_err(rtd->dev, "%s: %s: not supported for multi stage session\n",
				__func__, "LSM_LAB_CONTROL");
			return -EINVAL;
		}

		if (copy_from_user(&enable, arg, sizeof(enable))) {
			dev_err(rtd->dev, "%s: %s: copy_frm_user failed\n",
				__func__, "LSM_LAB_CONTROL");
			return -EFAULT;
		}

		dev_dbg(rtd->dev, "%s: ioctl %s, enable = %d\n",
			 __func__, "SNDRV_LSM_LAB_CONTROL", enable);

		if (prtd->lsm_client->started) {
			dev_err(rtd->dev, "%s: ioctl %s issued after start",
				__func__, "SNDRV_LSM_LAB_CONTROL");
			rc = -EINVAL;
			break;
		}

		/*
		 * With multi-stage support lab control needs to set param info
		 * specifying stage index for which this lab control is issued,
		 * along with values of module/instance ids applicable for the stage.
		 * Hence, set param info with default lab module/instance ids, and
		 * set stage index to LSM_STAGE_INDEX_FIRST.
		 */
		p_info.param_type = LSM_LAB_CONTROL;
		p_info.module_id = LSM_MODULE_ID_LAB;
		p_info.instance_id = INSTANCE_ID_0;
		p_info.stage_idx = LSM_STAGE_INDEX_FIRST;
		p_info.param_size = 0;
		rc = msm_lsm_check_and_set_lab_controls(substream, enable, &p_info);
		break;
	}
	case SNDRV_LSM_STOP_LAB:
		dev_dbg(rtd->dev, "%s: stopping LAB\n", __func__);
		if (prtd->lsm_client->lab_enable &&
			prtd->lsm_client->lab_started) {
			atomic_set(&prtd->read_abort, 1);
			rc = q6lsm_stop_lab(prtd->lsm_client);
			if (rc)
				dev_err(rtd->dev,
					"%s: Lab stop failed for session %d rc %d\n",
					__func__,
					prtd->lsm_client->session, rc);
			prtd->lsm_client->lab_started = false;
		}
	break;

	case SNDRV_LSM_SET_PORT:
		dev_dbg(rtd->dev, "%s: set LSM port\n", __func__);
		rc = q6lsm_set_port_connected(prtd->lsm_client);
		break;

	case SNDRV_LSM_SET_FWK_MODE_CONFIG: {
		u32 mode;

		if (copy_from_user(&mode, arg, sizeof(mode))) {
			dev_err(rtd->dev, "%s: %s: copy_frm_user failed\n",
				__func__, "LSM_SET_FWK_MODE_CONFIG");
			return -EFAULT;
		}

		dev_dbg(rtd->dev, "%s: ioctl %s, enable = %d\n",
			__func__, "SNDRV_LSM_SET_FWK_MODE_CONFIG", mode);
		if (prtd->lsm_client->event_mode == mode) {
			dev_dbg(rtd->dev,
				"%s: mode for %d already set to %d\n",
				__func__, prtd->lsm_client->session, mode);
			rc = 0;
		} else {
			dev_dbg(rtd->dev, "%s: Event mode = %d\n",
				 __func__, mode);
			rc = q6lsm_set_fwk_mode_cfg(prtd->lsm_client, mode);
			if (!rc)
				prtd->lsm_client->event_mode = mode;
			else
				dev_err(rtd->dev,
					"%s: set event mode failed %d\n",
					__func__, rc);
		}
		break;
	}
	case SNDRV_LSM_SET_INPUT_HW_PARAMS: {
		struct lsm_hw_params *in_params;
		struct snd_lsm_input_hw_params params;

		if (copy_from_user(&params, arg, sizeof(params))) {
			dev_err(rtd->dev, "%s: %s: copy_from_user failed\n",
				__func__, "LSM_SET_INPUT_HW_PARAMS");
			return -EFAULT;
		}

		in_params = &prtd->lsm_client->in_hw_params;
		in_params->sample_rate = params.sample_rate;
		in_params->sample_size = params.bit_width;
		in_params->num_chs = params.num_channels;

		break;
	}

	default:
		dev_dbg(rtd->dev,
			"%s: Falling into default snd_lib_ioctl cmd 0x%x\n",
			 __func__, cmd);
		rc = snd_pcm_lib_ioctl(substream, cmd, arg);
		break;
	}

	if (!rc)
		dev_dbg(rtd->dev, "%s: leave (%d)\n",
			__func__, rc);
	else
		dev_err(rtd->dev, "%s: cmd 0x%x failed %d\n",
			__func__, cmd, rc);

	return rc;
}

static int msm_lsm_check_event_type(struct lsm_client *lsm_client,
				    unsigned int cmd)
{
	int err = 0;
	uint32_t event_type = lsm_client->event_type;

	if (cmd == SNDRV_LSM_EVENT_STATUS &&
	    event_type != LSM_DET_EVENT_TYPE_LEGACY) {
		pr_err("%s: %s: Invalid event request\n",
		       __func__, "SNDRV_LSM_EVENT_STATUS");
		err = -EINVAL;
	} else if (cmd == SNDRV_LSM_GENERIC_DET_EVENT &&
		   event_type != LSM_DET_EVENT_TYPE_GENERIC) {
		pr_err("%s: %s: Invalid event request\n",
		       __func__, "SNDRV_LSM_GENERIC_DET_EVENT");
		err = -EINVAL;
	}

	return err;
}

#ifdef CONFIG_COMPAT

struct snd_lsm_event_status32 {
	u16 status;
	u16 payload_size;
	u8 payload[0];
};

struct snd_lsm_event_status_v3_32 {
	u32 timestamp_lsw;
	u32 timestamp_msw;
	u16 status;
	u16 payload_size;
	u8 payload[0];
};

struct snd_lsm_sound_model_v2_32 {
	compat_uptr_t data;
	compat_uptr_t confidence_level;
	u32 data_size;
	enum lsm_detection_mode detection_mode;
	u8 num_confidence_levels;
	bool detect_failure;
};

struct snd_lsm_detection_params_32 {
	compat_uptr_t conf_level;
	enum lsm_detection_mode detect_mode;
	u8 num_confidence_levels;
	bool detect_failure;
};

struct lsm_params_info_32 {
	u32 module_id;
	u32 param_id;
	u32 param_size;
	compat_uptr_t param_data;
	uint32_t param_type;
};

struct lsm_params_info_v2_32 {
	u32 module_id;
	u32 param_id;
	u32 param_size;
	compat_uptr_t param_data;
	uint32_t param_type;
	u16 instance_id;
	u16 stage_idx;
};

struct snd_lsm_module_params_32 {
	compat_uptr_t params;
	u32 num_params;
	u32 data_size;
};

enum {
	SNDRV_LSM_REG_SND_MODEL_V2_32 =
		_IOW('U', 0x07, struct snd_lsm_sound_model_v2_32),
	SNDRV_LSM_SET_PARAMS_32 =
		_IOW('U', 0x0A, struct snd_lsm_detection_params_32),
	SNDRV_LSM_SET_MODULE_PARAMS_32 =
		_IOW('U', 0x0B, struct snd_lsm_module_params_32),
	SNDRV_LSM_EVENT_STATUS_V3_32 =
		_IOW('U', 0x0F, struct snd_lsm_event_status_v3_32),
	SNDRV_LSM_SET_MODULE_PARAMS_V2_32 =
		_IOW('U', 0x13, struct snd_lsm_module_params_32),
};

static int msm_lsm_ioctl_compat(struct snd_pcm_substream *substream,
			  unsigned int cmd, void __user *arg)
{
	struct snd_pcm_runtime *runtime;
	struct lsm_priv *prtd;
	struct snd_soc_pcm_runtime *rtd;
	int err = 0;
	u32 size = 0;

	if (PCM_RUNTIME_CHECK(substream))
		return -ENXIO;

	if (!substream || !substream->private_data) {
		pr_err("%s: Invalid %s\n", __func__,
			(!substream) ? "substream" : "private_data");
		return -EINVAL;
	}
	runtime = substream->runtime;
	rtd = substream->private_data;
	prtd = runtime->private_data;

	mutex_lock(&prtd->lsm_api_lock);

	switch (cmd) {
	case SNDRV_LSM_EVENT_STATUS:
	case SNDRV_LSM_GENERIC_DET_EVENT: {
		struct snd_lsm_event_status userarg32, *user32 = NULL;
		struct snd_lsm_event_status *user = NULL;

		dev_dbg(rtd->dev,
			"%s: %s\n", __func__,
			(cmd == SNDRV_LSM_EVENT_STATUS) ?
			"SNDRV_LSM_EVENT_STATUS" :
			"SNDRV_LSM_GENERIC_DET_EVENT");

		err = msm_lsm_check_event_type(prtd->lsm_client, cmd);
		if (err)
			goto done;

		if (copy_from_user(&userarg32, arg, sizeof(userarg32))) {
			dev_err(rtd->dev, "%s: %s: Failed to copy from user\n",
				__func__, (cmd == SNDRV_LSM_EVENT_STATUS) ?
				"SNDRV_LSM_EVENT_STATUS" :
				"SNDRV_LSM_GENERIC_DET_EVENT");
			err = -EFAULT;
			goto done;
		}

		if (userarg32.payload_size >
		    LISTEN_MAX_STATUS_PAYLOAD_SIZE) {
			dev_err(rtd->dev,
				"%s: payload_size %d is invalid, max allowed = %d\n",
				__func__, userarg32.payload_size,
				LISTEN_MAX_STATUS_PAYLOAD_SIZE);
			err = -EINVAL;
			goto done;
		}

		size = sizeof(*user) + userarg32.payload_size;
		user = kzalloc(size, GFP_KERNEL);
		if (!user) {
			err = -ENOMEM;
			goto done;
		}

		user->payload_size = userarg32.payload_size;
		err = msm_lsm_ioctl_shared(substream, cmd, user);
		if (err) {
			dev_err(rtd->dev,
				"%s: msm_lsm_ioctl_shared() failed, err = %d",
				__func__, err);
			kfree(user);
			goto done;
		}

		/* Update size with actual payload size */
		size = sizeof(userarg32) + user->payload_size;
		if (!access_ok(VERIFY_WRITE, arg, size)) {
			dev_err(rtd->dev,
				"%s: Failed to verify write, size = %d\n",
				__func__, size);
			err = -EFAULT;
			kfree(user);
			goto done;
		}

		user32 = kzalloc(size, GFP_KERNEL);
		if (!user32) {
			err = -ENOMEM;
			kfree(user);
			goto done;
		}
		user32->status = user->status;
		user32->payload_size = user->payload_size;
		memcpy(user32->payload, user->payload,
		       user32->payload_size);

		if (copy_to_user(arg, user32, size)) {
			dev_err(rtd->dev,
				"%s: Failed to copy payload to user, size = %d",
				__func__, size);
			err = -EFAULT;
		}
		kfree(user);
		kfree(user32);
		break;
	}

	case SNDRV_LSM_EVENT_STATUS_V3_32: {
		struct snd_lsm_event_status_v3_32 userarg32, *user32 = NULL;
		struct snd_lsm_event_status_v3 *user = NULL;

		if (prtd->lsm_client->event_type !=
		    LSM_DET_EVENT_TYPE_LEGACY) {
			dev_err(rtd->dev,
				"%s: %s: Invalid event request\n",
				__func__, "SNDRV_LSM_EVENT_STATUS_V3_32");
			err = -EINVAL;
			goto done;
		}

		if (copy_from_user(&userarg32, arg, sizeof(userarg32))) {
			dev_err(rtd->dev, "%s: err copyuser ioctl %s\n",
				__func__, "SNDRV_LSM_EVENT_STATUS_V3_32");
			err = -EFAULT;
			goto done;
		}

		if (userarg32.payload_size >
		    LISTEN_MAX_STATUS_PAYLOAD_SIZE) {
			pr_err("%s: payload_size %d is invalid, max allowed = %d\n",
				__func__, userarg32.payload_size,
				LISTEN_MAX_STATUS_PAYLOAD_SIZE);
			err = -EINVAL;
			goto done;
		}

		size = sizeof(*user) + userarg32.payload_size;
		user = kzalloc(size, GFP_KERNEL);
		if (!user) {
			dev_err(rtd->dev,
				"%s: Allocation failed event status size %d\n",
				__func__, size);
			err = -ENOMEM;
			goto done;
		}
		cmd = SNDRV_LSM_EVENT_STATUS_V3;
		user->payload_size = userarg32.payload_size;
		err = msm_lsm_ioctl_shared(substream, cmd, user);

		/* Update size with actual payload size */
		size = sizeof(userarg32) + user->payload_size;
		if (!err && !access_ok(VERIFY_WRITE, arg, size)) {
			dev_err(rtd->dev,
				"%s: write verify failed size %d\n",
				__func__, size);
			err = -EFAULT;
		}
		if (!err) {
			user32 = kzalloc(size, GFP_KERNEL);
			if (!user32) {
				dev_err(rtd->dev,
					"%s: Allocation event user status size %d\n",
					__func__, size);
				err = -EFAULT;
			} else {
				user32->timestamp_lsw = user->timestamp_lsw;
				user32->timestamp_msw = user->timestamp_msw;
				user32->status = user->status;
				user32->payload_size = user->payload_size;
				memcpy(user32->payload,
				user->payload, user32->payload_size);
			}
		}
		if (!err && (copy_to_user(arg, user32, size))) {
			dev_err(rtd->dev, "%s: failed to copy payload %d",
				__func__, size);
			err = -EFAULT;
		}
		kfree(user);
		kfree(user32);
		if (err)
			dev_err(rtd->dev, "%s: lsmevent failed %d",
				__func__, err);
		break;
	}

	case SNDRV_LSM_REG_SND_MODEL_V2_32: {
		struct snd_lsm_sound_model_v2_32 snd_modelv232;
		struct snd_lsm_sound_model_v2 snd_modelv2;

		if (prtd->lsm_client->use_topology) {
			dev_err(rtd->dev,
				"%s: %s: not supported if using topology\n",
				__func__, "REG_SND_MODEL_V2");
			err = -EINVAL;
			goto done;
		}

		if (copy_from_user(&snd_modelv232, arg,
			sizeof(snd_modelv232))) {
			err = -EFAULT;
			dev_err(rtd->dev,
				"%s: copy user failed, size %zd %s\n",
				__func__,
				sizeof(struct snd_lsm_sound_model_v2_32),
				"SNDRV_LSM_REG_SND_MODEL_V2_32");
		} else {
			snd_modelv2.confidence_level =
			compat_ptr(snd_modelv232.confidence_level);
			snd_modelv2.data = compat_ptr(snd_modelv232.data);
			snd_modelv2.data_size = snd_modelv232.data_size;
			snd_modelv2.detect_failure =
			snd_modelv232.detect_failure;
			snd_modelv2.detection_mode =
			snd_modelv232.detection_mode;
			snd_modelv2.num_confidence_levels =
			snd_modelv232.num_confidence_levels;
			cmd = SNDRV_LSM_REG_SND_MODEL_V2;
			err = msm_lsm_ioctl_shared(substream, cmd,
				&snd_modelv2);
			if (err)
				dev_err(rtd->dev,
					"%s: ioctl %s failed\n", __func__,
					"SNDDRV_LSM_REG_SND_MODEL_V2_32");
		}
		break;
	}

	case SNDRV_LSM_SET_PARAMS_32:{
		struct snd_lsm_detection_params_32 det_params32;
		struct snd_lsm_detection_params det_params;

		if (prtd->lsm_client->use_topology) {
			dev_err(rtd->dev,
				"%s: %s: not supported if using topology\n",
				__func__, "SET_PARAMS_32");
			err = -EINVAL;
		}

		if (copy_from_user(&det_params32, arg,
				   sizeof(det_params32))) {
			err = -EFAULT;
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %zd\n",
				__func__, "SNDRV_LSM_SET_PARAMS_32",
				sizeof(det_params32));
		} else {
			det_params.conf_level =
				compat_ptr(det_params32.conf_level);
			det_params.detect_mode =
				det_params32.detect_mode;
			det_params.num_confidence_levels =
				det_params32.num_confidence_levels;
			det_params.detect_failure =
				det_params32.detect_failure;
			cmd = SNDRV_LSM_SET_PARAMS;
			err = msm_lsm_ioctl_shared(substream, cmd,
					&det_params);
			if (err)
				dev_err(rtd->dev,
					"%s: ioctl %s failed\n", __func__,
					"SNDRV_LSM_SET_PARAMS");
		}
		break;
	}

	case SNDRV_LSM_SET_MODULE_PARAMS_32:
	case SNDRV_LSM_SET_MODULE_PARAMS_V2_32: {
		struct snd_lsm_module_params_32 p_data_32;
		struct snd_lsm_module_params p_data;
		u8 *params32;
		size_t expected_size = 0, count;
		struct lsm_params_info_32 *p_info_32 = NULL;
		struct lsm_params_info_v2_32 *p_info_v2_32 = NULL;
		struct lsm_params_info_v2 p_info;

		if (!prtd->lsm_client->use_topology) {
			dev_err(rtd->dev,
				"%s: %s: not supported if not using topology\n",
				__func__, "SET_MODULE_PARAMS(_V2)_32");
			err = -EINVAL;
			goto done;
		}

		if (copy_from_user(&p_data_32, arg,
				   sizeof(p_data_32))) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %zd\n",
				__func__, "SET_MODULE_PARAMS(_V2)_32",
				sizeof(p_data_32));
			err = -EFAULT;
			goto done;
		}

		p_data.params = compat_ptr(p_data_32.params);
		p_data.num_params = p_data_32.num_params;
		p_data.data_size = p_data_32.data_size;

		if (p_data.num_params > LSM_PARAMS_MAX) {
			dev_err(rtd->dev,
				"%s: %s: Invalid num_params %d\n",
				__func__, "SET_MODULE_PARAMS(_V2)_32",
				p_data.num_params);
			err = -EINVAL;
			goto done;
		}

		expected_size = (cmd == SNDRV_LSM_SET_MODULE_PARAMS_32) ?
					p_data.num_params * sizeof(struct lsm_params_info_32) :
					p_data.num_params * sizeof(struct lsm_params_info_v2_32);

		if (p_data.data_size != expected_size) {
			dev_err(rtd->dev,
				"%s: %s: Invalid size %d\n",
				__func__, "SET_MODULE_PARAMS(_V2)_32",
				p_data.data_size);
			err = -EINVAL;
			goto done;
		}

		params32 = kzalloc(p_data.data_size, GFP_KERNEL);
		if (!params32) {
			err = -ENOMEM;
			goto done;
		}

		if (copy_from_user(params32, p_data.params,
				   p_data.data_size)) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %d\n",
				__func__, "params32", p_data.data_size);
			kfree(params32);
			err = -EFAULT;
			goto done;
		}

		if (cmd == SNDRV_LSM_SET_MODULE_PARAMS_32)
			p_info_32 = (struct lsm_params_info_32 *) params32;
		else
			p_info_v2_32 = (struct lsm_params_info_v2_32 *) params32;

		for (count = 0; count < p_data.num_params; count++) {
			if (cmd == SNDRV_LSM_SET_MODULE_PARAMS_32) {
				p_info.module_id = p_info_32->module_id;
				p_info.param_id = p_info_32->param_id;
				p_info.param_size = p_info_32->param_size;
				p_info.param_data = compat_ptr(p_info_32->param_data);
				p_info.param_type = p_info_32->param_type;

				p_info.instance_id = INSTANCE_ID_0;
				p_info.stage_idx = LSM_STAGE_INDEX_FIRST;

				p_info_32++;
			} else {
				p_info.module_id = p_info_v2_32->module_id;
				p_info.param_id = p_info_v2_32->param_id;
				p_info.param_size = p_info_v2_32->param_size;
				p_info.param_data = compat_ptr(p_info_v2_32->param_data);
				p_info.param_type = p_info_v2_32->param_type;

				p_info.instance_id = p_info_v2_32->instance_id;
				p_info.stage_idx = p_info_v2_32->stage_idx;

				p_info_v2_32++;
			}

			err = msm_lsm_process_params(substream, &p_info);
			if (err)
				dev_err(rtd->dev,
					"%s: Failed to process param, type%d stage=%d err=%d\n",
					__func__, p_info.param_type, p_info.stage_idx, err);
		}

		kfree(params32);
		break;
	}
	case SNDRV_LSM_REG_SND_MODEL_V2:
	case SNDRV_LSM_SET_PARAMS:
	case SNDRV_LSM_SET_MODULE_PARAMS:
	case SNDRV_LSM_SET_MODULE_PARAMS_V2:
		/*
		 * In ideal cases, the compat_ioctl should never be called
		 * with the above unlocked ioctl commands. Print error
		 * and return error if it does.
		 */
		dev_err(rtd->dev,
			"%s: Invalid cmd for compat_ioctl\n",
			__func__);
		err = -EINVAL;
		break;
	default:
		err = msm_lsm_ioctl_shared(substream, cmd, arg);
		break;
	}
done:
	mutex_unlock(&prtd->lsm_api_lock);
	return err;
}
#else
#define msm_lsm_ioctl_compat NULL
#endif

static int msm_lsm_ioctl(struct snd_pcm_substream *substream,
			 unsigned int cmd, void __user *arg)
{
	int err = 0;
	u32 size = 0;
	struct snd_pcm_runtime *runtime;
	struct snd_soc_pcm_runtime *rtd;
	struct lsm_priv *prtd;

	if (!substream || !substream->private_data) {
		pr_err("%s: Invalid %s\n", __func__,
			(!substream) ? "substream" : "private_data");
		return -EINVAL;
	}
	runtime = substream->runtime;
	prtd = runtime->private_data;
	rtd = substream->private_data;

	mutex_lock(&prtd->lsm_api_lock);
	switch (cmd) {
	case SNDRV_LSM_REG_SND_MODEL_V2: {
		struct snd_lsm_sound_model_v2 snd_model_v2;

		if (prtd->lsm_client->use_topology) {
			dev_err(rtd->dev,
				"%s: %s: not supported if using topology\n",
				__func__, "REG_SND_MODEL_V2");
			err = -EINVAL;
			goto done;
		}

		if (copy_from_user(&snd_model_v2, arg, sizeof(snd_model_v2))) {
			err = -EFAULT;
			dev_err(rtd->dev,
				"%s: copy from user failed, size %zd\n",
				__func__,
				sizeof(struct snd_lsm_sound_model_v2));
		}
		if (!err)
			err = msm_lsm_ioctl_shared(substream, cmd,
						   &snd_model_v2);
		if (err)
			dev_err(rtd->dev,
				"%s REG_SND_MODEL failed err %d\n",
				__func__, err);
		goto done;
		}
		break;
	case SNDRV_LSM_SET_PARAMS: {
		struct snd_lsm_detection_params det_params;

		if (prtd->lsm_client->use_topology) {
			dev_err(rtd->dev,
				"%s: %s: not supported if using topology\n",
				__func__, "SET_PARAMS");
			err = -EINVAL;
			goto done;
		}

		pr_debug("%s: SNDRV_LSM_SET_PARAMS\n", __func__);

		if (copy_from_user(&det_params, arg,
				   sizeof(det_params))) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size %zd\n",
				__func__, "SNDRV_LSM_SET_PARAMS",
				sizeof(det_params));
			err = -EFAULT;
		}

		if (!err)
			err = msm_lsm_ioctl_shared(substream, cmd,
						   &det_params);
		else
			dev_err(rtd->dev,
				"%s: LSM_SET_PARAMS failed, err %d\n",
				__func__, err);

		goto done;
	}

	case SNDRV_LSM_SET_MODULE_PARAMS:
	case SNDRV_LSM_SET_MODULE_PARAMS_V2: {
		struct snd_lsm_module_params p_data;
		struct lsm_params_info *temp_ptr_info = NULL;
		struct lsm_params_info_v2 info_v2;
		struct lsm_params_info_v2 *ptr_info_v2 = NULL, *temp_ptr_info_v2 = NULL;
		size_t p_size = 0, count;
		u8 *params;

		if (!prtd->lsm_client->use_topology) {
			dev_err(rtd->dev,
				"%s: %s: not supported if not using topology\n",
				__func__, "SET_MODULE_PARAMS(_V2)");
			err = -EINVAL;
			goto done;
		}

		if (copy_from_user(&p_data, arg,
				   sizeof(p_data))) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %zd\n",
				__func__, "p_data", sizeof(p_data));
			err = -EFAULT;
			goto done;
		}

		if (p_data.num_params > LSM_PARAMS_MAX) {
			dev_err(rtd->dev,
				"%s: %s: Invalid num_params %d\n",
				__func__, "SET_MODULE_PARAMS(_V2)",
				p_data.num_params);
			err = -EINVAL;
			goto done;
		}

		if (cmd == SNDRV_LSM_SET_MODULE_PARAMS)
			p_size = p_data.num_params * sizeof(struct lsm_params_info);
		else
			p_size = p_data.num_params * sizeof(struct lsm_params_info_v2);

		if (p_data.data_size != p_size) {
			dev_err(rtd->dev,
				"%s: %s: Invalid data_size(%u) against expected(%zd)\n",
				__func__, "SET_MODULE_PARAMS(_V2)",
				p_data.data_size, p_size);
			err = -EFAULT;
			goto done;
		}

		params = kzalloc(p_size, GFP_KERNEL);
		if (!params) {
			err = -ENOMEM;
			goto done;
		}

		if (copy_from_user(params, p_data.params,
				  p_data.data_size)) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %d\n",
				__func__, "set module params", p_data.data_size);
			kfree(params);
			err = -EFAULT;
			goto done;
		}

		if (cmd == SNDRV_LSM_SET_MODULE_PARAMS)
			temp_ptr_info = (struct lsm_params_info *)params;
		else
			temp_ptr_info_v2 = (struct lsm_params_info_v2 *)params;

		for (count = 0; count < p_data.num_params; count++) {
			if (cmd == SNDRV_LSM_SET_MODULE_PARAMS) {
				/* convert to V2 param info struct from legacy param info */
				info_v2.module_id = temp_ptr_info->module_id;
				info_v2.param_id = temp_ptr_info->param_id;
				info_v2.param_size = temp_ptr_info->param_size;
				info_v2.param_data = temp_ptr_info->param_data;
				info_v2.param_type = temp_ptr_info->param_type;

				info_v2.instance_id = INSTANCE_ID_0;
				info_v2.stage_idx = LSM_STAGE_INDEX_FIRST;

				ptr_info_v2 = &info_v2;
				temp_ptr_info++;
			} else {
				/* Just copy the pointer as user already provided v2 params */
				ptr_info_v2 = temp_ptr_info_v2;
				temp_ptr_info_v2++;
			}
			err = msm_lsm_process_params(substream, ptr_info_v2);
			if (err)
				dev_err(rtd->dev,
					"%s: Failed to process param, type%d stage=%d err=%d\n",
					__func__, ptr_info_v2->param_type,
					ptr_info_v2->stage_idx, err);
		}
		kfree(params);
		break;
	}

	case SNDRV_LSM_EVENT_STATUS:
	case SNDRV_LSM_GENERIC_DET_EVENT: {
		struct snd_lsm_event_status *user = NULL;
		struct snd_lsm_event_status userarg;

		dev_dbg(rtd->dev,
			"%s: %s\n", __func__,
			(cmd == SNDRV_LSM_EVENT_STATUS) ?
			"SNDRV_LSM_EVENT_STATUS" :
			"SNDRV_LSM_GENERIC_DET_EVENT");

		err = msm_lsm_check_event_type(prtd->lsm_client, cmd);
		if (err)
			goto done;

		if (copy_from_user(&userarg, arg, sizeof(userarg))) {
			dev_err(rtd->dev,
				"%s: %s: Copy from user failed\n", __func__,
				(cmd == SNDRV_LSM_EVENT_STATUS) ?
				"SNDRV_LSM_EVENT_STATUS" :
				"SNDRV_LSM_GENERIC_DET_EVENT");
			err = -EFAULT;
			goto done;
		}

		if (userarg.payload_size >
		    LISTEN_MAX_STATUS_PAYLOAD_SIZE) {
			dev_err(rtd->dev,
				"%s: payload_size %d is invalid, max allowed = %d\n",
				__func__, userarg.payload_size,
				LISTEN_MAX_STATUS_PAYLOAD_SIZE);
			err = -EINVAL;
			goto done;
		}

		size = sizeof(struct snd_lsm_event_status) +
			userarg.payload_size;
		user = kzalloc(size, GFP_KERNEL);
		if (!user) {
			err = -ENOMEM;
			goto done;
		}

		user->payload_size = userarg.payload_size;
		err = msm_lsm_ioctl_shared(substream, cmd, user);
		if (err) {
			dev_err(rtd->dev,
				"%s: msm_lsm_ioctl_shared() failed, err = %d",
				__func__, err);
			kfree(user);
			goto done;
		}

		/* Update size with actual payload size */
		size = sizeof(*user) + user->payload_size;
		if (!access_ok(VERIFY_WRITE, arg, size)) {
			dev_err(rtd->dev,
				"%s: Failed to verify write, size = %d\n",
				__func__, size);
			err = -EFAULT;
		}
		if (!err && copy_to_user(arg, user, size)) {
			dev_err(rtd->dev,
				"%s: Failed to copy payload to user, size = %d\n",
				__func__, size);
			err = -EFAULT;
		}

		kfree(user);
		break;
	}

	case SNDRV_LSM_EVENT_STATUS_V3: {
		struct snd_lsm_event_status_v3 *user = NULL;
		struct snd_lsm_event_status_v3 userarg;

		dev_dbg(rtd->dev,
			"%s: SNDRV_LSM_EVENT_STATUS_V3\n", __func__);

		if (prtd->lsm_client->event_type !=
		    LSM_DET_EVENT_TYPE_LEGACY) {
			dev_err(rtd->dev,
				"%s: %s: Invalid event request\n",
				__func__, "SNDRV_LSM_EVENT_STATUS_V3");
			err = -EINVAL;
			goto done;
		}

		if (!arg) {
			dev_err(rtd->dev,
				"%s: Invalid params event_status_v3\n",
				__func__);
			err = -EINVAL;
			goto done;
		}
		if (copy_from_user(&userarg, arg, sizeof(userarg))) {
			dev_err(rtd->dev,
				"%s: err copyuser event_status_v3\n",
				__func__);
			err = -EFAULT;
			goto done;
		}

		if (userarg.payload_size >
		    LISTEN_MAX_STATUS_PAYLOAD_SIZE) {
			pr_err("%s: payload_size %d is invalid, max allowed = %d\n",
				__func__, userarg.payload_size,
				LISTEN_MAX_STATUS_PAYLOAD_SIZE);
			err = -EINVAL;
			goto done;
		}

		size = sizeof(struct snd_lsm_event_status_v3) +
			userarg.payload_size;
		user = kzalloc(size, GFP_KERNEL);
		if (!user) {
			dev_err(rtd->dev,
				"%s: Allocation failed event status size %d\n",
				__func__, size);
			err = -EFAULT;
			goto done;
		}
		user->payload_size = userarg.payload_size;
		err = msm_lsm_ioctl_shared(substream, cmd, user);

		/* Update size with actual payload size */
		size = sizeof(*user) + user->payload_size;
		if (!err && !access_ok(VERIFY_WRITE, arg, size)) {
			dev_err(rtd->dev,
				"%s: write verify failed size %d\n",
				__func__, size);
			err = -EFAULT;
		}
		if (!err && (copy_to_user(arg, user, size))) {
			dev_err(rtd->dev,
				"%s: failed to copy payload %d",
				__func__, size);
			err = -EFAULT;
		}
		kfree(user);
		if (err)
			dev_err(rtd->dev,
				"%s: lsm_event_v3 failed %d", __func__, err);
		break;
	}

	default:
		err = msm_lsm_ioctl_shared(substream, cmd, arg);
	break;
	}
done:
	mutex_unlock(&prtd->lsm_api_lock);
	return err;
}

static int msm_lsm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd;
	int ret = 0;

	pr_debug("%s\n", __func__);
	prtd = kzalloc(sizeof(struct lsm_priv), GFP_KERNEL);
	if (!prtd) {
		pr_err("%s: Failed to allocate memory for lsm_priv\n",
		       __func__);
		return -ENOMEM;
	}
	mutex_init(&prtd->lsm_api_lock);
	spin_lock_init(&prtd->event_lock);
	spin_lock_init(&prtd->xrun_lock);
	init_waitqueue_head(&prtd->event_wait);
	init_waitqueue_head(&prtd->period_wait);
	prtd->substream = substream;
	runtime->private_data = prtd;
	runtime->hw = msm_pcm_hardware_capture;

	ret = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE,
				&constraints_sample_rates);
	if (ret < 0)
		pr_info("%s: snd_pcm_hw_constraint_list failed ret %d\n",
			 __func__, ret);
	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
			    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		pr_info("%s: snd_pcm_hw_constraint_integer failed ret %d\n",
			__func__, ret);

	ret = snd_pcm_hw_constraint_minmax(runtime,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
		CAPTURE_MIN_NUM_PERIODS * CAPTURE_MIN_PERIOD_SIZE,
		CAPTURE_MAX_NUM_PERIODS * CAPTURE_MAX_PERIOD_SIZE);
	if (ret < 0)
		pr_info("%s: constraint for buffer bytes min max ret = %d\n",
			__func__, ret);
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	if (ret < 0) {
		pr_info("%s: constraint for period bytes step ret = %d\n",
			__func__, ret);
	}
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	if (ret < 0)
		pr_info("%s: constraint for buffer bytes step ret = %d\n",
			__func__, ret);
	prtd->lsm_client = q6lsm_client_alloc(
				lsm_event_handler, prtd);
	if (!prtd->lsm_client) {
		pr_err("%s: Could not allocate memory\n", __func__);
		kfree(prtd);
		runtime->private_data = NULL;
		return -ENOMEM;
	}
	prtd->lsm_client->opened = false;
	prtd->lsm_client->session_state = IDLE;
	prtd->lsm_client->poll_enable = true;
	prtd->lsm_client->perf_mode = 0;
	prtd->lsm_client->event_mode = LSM_EVENT_NON_TIME_STAMP_MODE;
	prtd->lsm_client->event_type = LSM_DET_EVENT_TYPE_LEGACY;

	return 0;
}

static int msm_lsm_send_ch_mix_config(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd;
	struct lsm_hw_params *in_params;
	int pp_ch_cnt;
	int *ch_wght_coeff;
	int ret = 0, i, idx;

	/*
	 * The output channels from channel mixer is the input to LSM (stream)
	 * side and is read from in_params->num_chs.
	 *
	 * The input channels to channel mixer are the output channels from
	 * the device side (routing) and is obtained by reading the
	 * pp_ch_cnt.
	 *
	 * For LSM to be functional, only unity channel mixing is allowed.
	 */

	in_params = &prtd->lsm_client->in_hw_params;
	rtd = prtd->substream->private_data;
	pp_ch_cnt = msm_pcm_routing_get_pp_ch_cnt(rtd->dai_link->id,
						  SESSION_TYPE_TX);
	if (pp_ch_cnt < 0 ||
	    pp_ch_cnt > LSM_V3P0_MAX_NUM_CHANNELS ||
	     in_params->num_chs > LSM_V3P0_MAX_NUM_CHANNELS) {
		dev_err(rtd->dev,
			"%s: invalid ch cnt, pp_ch_cnt %d in_ch_cnt %d\n",
			__func__, pp_ch_cnt, in_params->num_chs);
		return -EINVAL;
	}

	if (!pp_ch_cnt ||
	    (pp_ch_cnt == in_params->num_chs)) {
		dev_dbg(rtd->dev,
			"%s: Skip ch mixing, pp_ch_cnt %d in_ch_cnt %d\n",
			__func__, pp_ch_cnt, in_params->num_chs);
		return 0;
	}

	ch_wght_coeff = kzalloc(in_params->num_chs * pp_ch_cnt * sizeof(int),
				GFP_KERNEL);
	if (!ch_wght_coeff)
		return -ENOMEM;

	/*
	 * channel weight co-efficients is a m X n array, where
	 * m = number of input channels to ch mixer (pp_ch_cnt)
	 * n = number of output channels from ch mixer (in_params->num_chs)
	 */
	for (i = 0; i < in_params->num_chs; i++) {
		idx = (i * pp_ch_cnt) + i;
		ch_wght_coeff[idx] = 1;
	}

	ret = msm_pcm_routing_send_chmix_cfg(rtd->dai_link->id,
					     pp_ch_cnt, in_params->num_chs,
					     ch_wght_coeff,
					     SESSION_TYPE_TX, STREAM_TYPE_LSM);
	if (ret)
		dev_err(rtd->dev,
			"%s: Failed to configure channel mixer err %d\n",
			__func__, ret);

	kfree(ch_wght_coeff);

	return ret;
}

static int msm_lsm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;

	if (!substream->private_data) {
		pr_err("%s: Invalid private_data", __func__);
		return -EINVAL;
	}

	rtd = prtd->substream->private_data;

	if (!prtd->lsm_client) {
		dev_err(rtd->dev,
			"%s: LSM client data ptr is NULL\n", __func__);
		return -EINVAL;
	}

	if (q6lsm_set_media_fmt_v2_params(prtd->lsm_client))
		dev_dbg(rtd->dev,
			"%s: failed to set lsm media fmt params\n", __func__);

	if (prtd->lsm_client->session_state == IDLE) {
		ret = msm_pcm_routing_reg_phy_compr_stream(
				rtd->dai_link->id,
				prtd->lsm_client->perf_mode,
				prtd->lsm_client->session,
				SNDRV_PCM_STREAM_CAPTURE,
				LISTEN);
		if (ret) {
			dev_err(rtd->dev,
				"%s: register phy compr stream failed %d\n",
					__func__, ret);
			return ret;
		}

		ret = msm_lsm_send_ch_mix_config(substream);
		if (ret) {
			msm_pcm_routing_dereg_phy_stream(rtd->dai_link->id,
					SNDRV_PCM_STREAM_CAPTURE);
			return ret;
		}
	}

	prtd->lsm_client->session_state = RUNNING;
	prtd->lsm_client->started = false;
	runtime->private_data = prtd;
	return ret;
}

static int msm_lsm_close(struct snd_pcm_substream *substream)
{
	unsigned long flags;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;

	if (!substream->private_data) {
		pr_err("%s: Invalid private_data", __func__);
		return -EINVAL;
	}
	if (!prtd || !prtd->lsm_client) {
		pr_err("%s: No LSM session active\n", __func__);
		return -EINVAL;
	}
	rtd = substream->private_data;

	dev_dbg(rtd->dev, "%s\n", __func__);
	if (prtd->lsm_client->started) {
		ret = q6lsm_stop(prtd->lsm_client, true);
		if (ret)
			dev_err(rtd->dev,
				"%s: session stop failed, err = %d\n",
				__func__, ret);
		else
			dev_dbg(rtd->dev,
				"%s: LSM client session stopped %d\n",
				 __func__, ret);

		/*
		 * Go Ahead and try de-register sound model,
		 * even if stop failed
		 */
		prtd->lsm_client->started = false;

		ret = q6lsm_deregister_sound_model(prtd->lsm_client);
		if (ret)
			dev_err(rtd->dev,
				"%s: dereg_snd_model failed, err = %d\n",
				__func__, ret);
		else
			dev_dbg(rtd->dev, "%s: dereg_snd_model successful\n",
				 __func__);
	}

	msm_pcm_routing_dereg_phy_stream(rtd->dai_link->id,
					SNDRV_PCM_STREAM_CAPTURE);

	if (prtd->lsm_client->opened) {
		q6lsm_close(prtd->lsm_client);
		prtd->lsm_client->opened = false;
	}
	q6lsm_client_free(prtd->lsm_client);

	spin_lock_irqsave(&prtd->event_lock, flags);
	kfree(prtd->event_status);
	prtd->event_status = NULL;
	kfree(prtd->det_event);
	prtd->det_event = NULL;
	spin_unlock_irqrestore(&prtd->event_lock, flags);
	mutex_destroy(&prtd->lsm_api_lock);
	kfree(prtd);
	runtime->private_data = NULL;

	return 0;
}

static int msm_lsm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct lsm_hw_params *out_hw_params = NULL;
	struct lsm_hw_params *in_hw_params = NULL;
	struct snd_soc_pcm_runtime *rtd;

	if (!substream->private_data) {
		pr_err("%s: Invalid private_data", __func__);
		return -EINVAL;
	}
	rtd = substream->private_data;

	if (!prtd || !params) {
		dev_err(rtd->dev,
			"%s: invalid params prtd %pK params %pK",
			 __func__, prtd, params);
		return -EINVAL;
	}
	in_hw_params = &prtd->lsm_client->in_hw_params;
	out_hw_params = &prtd->lsm_client->out_hw_params;
	out_hw_params->num_chs = params_channels(params);
	out_hw_params->period_count = params_periods(params);
	out_hw_params->sample_rate = params_rate(params);
	if (((out_hw_params->sample_rate != 16000) &&
		(out_hw_params->sample_rate != 48000)) ||
		(out_hw_params->period_count == 0)) {
		dev_err(rtd->dev,
			"%s: Invalid Params sample rate %d period count %d\n",
			__func__, out_hw_params->sample_rate,
			out_hw_params->period_count);
		return -EINVAL;
	}

	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE) {
		out_hw_params->sample_size = 16;
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) {
		out_hw_params->sample_size = 24;
	} else {
		dev_err(rtd->dev, "%s: Invalid Format 0x%x\n",
			__func__, params_format(params));
		return -EINVAL;
	}

	out_hw_params->buf_sz = params_buffer_bytes(params) /
			out_hw_params->period_count;
	dev_dbg(rtd->dev,
		"%s: channels %d sample rate %d sample size %d buffer size %d period count %d\n",
		__func__, out_hw_params->num_chs, out_hw_params->sample_rate,
		out_hw_params->sample_size, out_hw_params->buf_sz,
		out_hw_params->period_count);

	/*
	 * copy the out_hw_params to in_hw_params. in_hw_params will be
	 * over-written with LSM_SET_INPUT_HW_PARAMS ioctl from userspace.
	 * If this ioctl is not set, then it is assumed that input and
	 * output hw params for LSM are the same.
	 * Currently the period_count and buf_sz are unused for input params.
	 */
	memcpy(in_hw_params, out_hw_params,
	       sizeof(struct lsm_hw_params));
	return 0;
}

static snd_pcm_uframes_t msm_lsm_pcm_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd;

	if (!substream->private_data) {
		pr_err("%s: Invalid private_data", __func__);
		return -EINVAL;
	}
	rtd = substream->private_data;

	if (!prtd) {
		dev_err(rtd->dev,
			"%s: Invalid param %pK\n", __func__, prtd);
		return 0;
	}

	if (prtd->dma_write >= snd_pcm_lib_buffer_bytes(substream))
		prtd->dma_write = 0;
	dev_dbg(rtd->dev,
		"%s: dma post = %d\n", __func__, prtd->dma_write);
	return bytes_to_frames(runtime, prtd->dma_write);
}

static int msm_lsm_pcm_copy(struct snd_pcm_substream *substream, int ch,
	unsigned long hwoff, void __user *buf, unsigned long fbytes)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	char *pcm_buf = NULL;
	int rc = 0, buf_index = 0;
	unsigned long flags = 0;
	struct snd_soc_pcm_runtime *rtd;

	if (!substream->private_data) {
		pr_err("%s: Invalid private_data", __func__);
		return -EINVAL;
	}
	rtd = substream->private_data;

	if (!prtd) {
		dev_err(rtd->dev,
			"%s: Invalid param %pK\n", __func__, prtd);
		return -EINVAL;
	}

	if (runtime->status->state == SNDRV_PCM_STATE_XRUN ||
	    runtime->status->state == SNDRV_PCM_STATE_PREPARED) {
		dev_err(rtd->dev,
			"%s: runtime state incorrect %d", __func__,
			runtime->status->state);
		return 0;
	}
	rc = wait_event_timeout(prtd->period_wait,
		(atomic_read(&prtd->buf_count) |
		atomic_read(&prtd->read_abort)), (2 * HZ));
	if (!rc) {
		dev_err(rtd->dev,
			"%s: timeout for read retry\n", __func__);
		return -EAGAIN;
	}
	if (atomic_read(&prtd->read_abort)) {
		dev_err(rtd->dev,
			"%s: Read abort received\n", __func__);
		return -EIO;
	}
	prtd->appl_cnt = prtd->appl_cnt %
		prtd->lsm_client->out_hw_params.period_count;
	pcm_buf = prtd->lsm_client->lab_buffer[prtd->appl_cnt].data;
	dev_dbg(rtd->dev,
		"%s: copy the pcm data size %lu\n",
		__func__, fbytes);
	if (pcm_buf) {
		if (copy_to_user(buf, pcm_buf, fbytes)) {
			dev_err(rtd->dev,
				"%s: failed to copy bytes %lu\n",
				__func__, fbytes);
			return -EINVAL;
		}
	} else {
		dev_err(rtd->dev,
			"%s: Invalid pcm buffer\n", __func__);
		return -EINVAL;
	}
	prtd->appl_cnt = (prtd->appl_cnt + 1) %
		prtd->lsm_client->out_hw_params.period_count;

	spin_lock_irqsave(&prtd->xrun_lock, flags);
	/* Queue lab buffer here if in xrun */
	if (prtd->xrun_count > 0) {
		(prtd->xrun_count)--;
		buf_index = (prtd->xrun_index + 1) %
			prtd->lsm_client->out_hw_params.period_count;
		rc = msm_lsm_queue_lab_buffer(prtd, buf_index);
		if (rc)
			dev_err(rtd->dev,
				"%s: error in queuing the lab buffer rc %d\n",
				__func__, rc);
		prtd->xrun_index = buf_index;
	}
	atomic_dec(&prtd->buf_count);
	spin_unlock_irqrestore(&prtd->xrun_lock, flags);

	return 0;
}

static int msm_lsm_app_type_cfg_ctl_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int session_type = SESSION_TYPE_TX;
	int be_id = ucontrol->value.integer.value[3];
	struct msm_pcm_stream_app_type_cfg cfg_data = {0};
	int ret = 0;

	cfg_data.app_type = ucontrol->value.integer.value[0];
	cfg_data.acdb_dev_id = ucontrol->value.integer.value[1];
	cfg_data.sample_rate = ucontrol->value.integer.value[2];

	pr_debug("%s: fe_id- %llu session_type- %d be_id- %d app_type- %d acdb_dev_id- %d sample_rate- %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate);
	ret = msm_pcm_routing_reg_stream_app_type_cfg(fe_id, session_type,
						      be_id, &cfg_data);
	if (ret < 0)
		pr_err("%s: msm_pcm_routing_reg_stream_app_type_cfg failed returned %d\n",
			__func__, ret);

	return 0;
}

static int msm_lsm_app_type_cfg_ctl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int session_type = SESSION_TYPE_TX;
	int be_id = 0;
	struct msm_pcm_stream_app_type_cfg cfg_data = {0};
	int ret = 0;

	ret = msm_pcm_routing_get_stream_app_type_cfg(fe_id, session_type,
						      &be_id, &cfg_data);
	if (ret < 0) {
		pr_err("%s: msm_pcm_routing_get_stream_app_type_cfg failed returned %d\n",
			__func__, ret);
		goto done;
	}

	ucontrol->value.integer.value[0] = cfg_data.app_type;
	ucontrol->value.integer.value[1] = cfg_data.acdb_dev_id;
	ucontrol->value.integer.value[2] = cfg_data.sample_rate;
	ucontrol->value.integer.value[3] = be_id;
	pr_debug("%s: fedai_id %llu, session_type %d, be_id %d, app_type %d, acdb_dev_id %d, sample_rate %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate);
done:
	return ret;
}

static int msm_lsm_add_app_type_controls(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_usr *app_type_info;
	struct snd_kcontrol *kctl;
	const char *mixer_ctl_name	= "Listen Stream";
	const char *deviceNo		= "NN";
	const char *suffix		= "App Type Cfg";
	int ctl_len, ret = 0;

	ctl_len = strlen(mixer_ctl_name) + 1 +
			strlen(deviceNo) + 1 + strlen(suffix) + 1;
	pr_debug("%s: Listen app type cntrl add\n", __func__);
	ret = snd_pcm_add_usr_ctls(pcm, SNDRV_PCM_STREAM_CAPTURE,
				NULL, 1, ctl_len, rtd->dai_link->id,
				&app_type_info);
	if (ret < 0) {
		pr_err("%s: Listen app type cntrl add failed: %d\n",
			__func__, ret);
		return ret;
	}
	kctl = app_type_info->kctl;
	snprintf(kctl->id.name, ctl_len, "%s %d %s",
		mixer_ctl_name, rtd->pcm->device, suffix);
	kctl->put = msm_lsm_app_type_cfg_ctl_put;
	kctl->get = msm_lsm_app_type_cfg_ctl_get;
	return 0;
}

static int msm_lsm_add_controls(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	ret = msm_lsm_add_app_type_controls(rtd);
	if (ret)
		pr_err("%s, add  app type controls failed:%d\n", __func__, ret);

	return ret;
}

static const struct snd_pcm_ops msm_lsm_ops = {
	.open           = msm_lsm_open,
	.close          = msm_lsm_close,
	.ioctl          = msm_lsm_ioctl,
	.prepare	= msm_lsm_prepare,
	.compat_ioctl   = msm_lsm_ioctl_compat,
	.hw_params      = msm_lsm_hw_params,
	.copy_user      = msm_lsm_pcm_copy,
	.pointer        = msm_lsm_pcm_pointer,
};

static int msm_asoc_lsm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = msm_lsm_add_controls(rtd);
	if (ret)
		pr_err("%s, kctl add failed:%d\n", __func__, ret);

	return ret;
}

static int msm_asoc_lsm_probe(struct snd_soc_platform *platform)
{
	pr_debug("enter %s\n", __func__);

	return 0;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_lsm_ops,
	.pcm_new	= msm_asoc_lsm_new,
	.probe		= msm_asoc_lsm_probe,
};

static int msm_lsm_probe(struct platform_device *pdev)
{

	return snd_soc_register_platform(&pdev->dev, &msm_soc_platform);
}

static int msm_lsm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static const struct of_device_id msm_lsm_client_dt_match[] = {
	{.compatible = "qcom,msm-lsm-client" },
	{ }
};

static struct platform_driver msm_lsm_driver = {
	.driver = {
		.name = "msm-lsm-client",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(msm_lsm_client_dt_match),
	},
	.probe = msm_lsm_probe,
	.remove = msm_lsm_remove,
};

int __init msm_lsm_client_init(void)
{
	return platform_driver_register(&msm_lsm_driver);
}

void msm_lsm_client_exit(void)
{
	platform_driver_unregister(&msm_lsm_driver);
}

MODULE_DESCRIPTION("LSM client platform driver");
MODULE_DEVICE_TABLE(of, msm_lsm_client_dt_match);
MODULE_LICENSE("GPL v2");

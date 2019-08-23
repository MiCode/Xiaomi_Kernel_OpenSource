/*
 * Copyright (c) 2013-2017, 2019 Linux Foundation. All rights reserved.
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
#include <linux/msm_audio_ion.h>
#include <linux/freezer.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/timer.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/q6lsm.h>
#include <sound/lsm_params.h>
#include <sound/pcm_params.h>
#include "msm-pcm-routing-v2.h"

#define CAPTURE_MIN_NUM_PERIODS     2
#define CAPTURE_MAX_NUM_PERIODS     8
#define CAPTURE_MAX_PERIOD_SIZE     61440
#define CAPTURE_MIN_PERIOD_SIZE     320
#define LISTEN_MAX_STATUS_PAYLOAD_SIZE 256

#define LAB_BUFFER_ALLOC 1
#define LAB_BUFFER_DEALLOC 0

/*
 * Driver ioctl will parse only so many params
 * size of LSM_PARAMS_MAX is last LSM_PARAM_TYPE + 1
 */
#define LSM_PARAMS_MAX (LSM_POLLING_ENABLE + 1)

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
		i >= prtd->lsm_client->hw_params.period_count) {
		dev_err(rtd->dev,
			"%s: Lab buffer not setup %pK incorrect index %d period count %d\n",
			__func__, prtd->lsm_client->lab_buffer, i,
			prtd->lsm_client->hw_params.period_count);
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
	for (i = 0; i < prtd->lsm_client->hw_params.period_count; i++) {
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
			      void *payload, uint16_t client_size,
				void *priv)
{
	unsigned long flags;
	struct lsm_priv *prtd = priv;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_soc_pcm_runtime *rtd;
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
		struct lsm_cmd_read_done *read_done = payload;
		int buf_index = 0;
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
				prtd->lsm_client->hw_params.period_count) {
				dev_err(rtd->dev,
					"%s: Invalid index %d buf_index max cnt %d\n",
					__func__, buf_index,
				prtd->lsm_client->hw_params.period_count);
				return;
			}
			prtd->dma_write += read_done->total_size;
			atomic_inc(&prtd->buf_count);
			snd_pcm_period_elapsed(substream);
			wake_up(&prtd->period_wait);
			/* queue the next period buffer */
			buf_index = (buf_index + 1) %
			prtd->lsm_client->hw_params.period_count;
			rc = msm_lsm_queue_lab_buffer(prtd, buf_index);
			if (rc)
				dev_err(rtd->dev,
					"%s: error in queuing the lab buffer rc %d\n",
					__func__, rc);
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
			__func__, status , payload_size);
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
			__func__, status , payload_size);
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

	default:
		break;
	}

	if (opcode == LSM_SESSION_EVENT_DETECTION_STATUS ||
		opcode == LSM_SESSION_EVENT_DETECTION_STATUS_V2 ||
		opcode == LSM_SESSION_EVENT_DETECTION_STATUS_V3) {
		spin_lock_irqsave(&prtd->event_lock, flags);
		prtd->event_status = krealloc(prtd->event_status,
					sizeof(struct snd_lsm_event_status_v3) +
					payload_size, GFP_ATOMIC);
		if (!prtd->event_status) {
			dev_err(rtd->dev, "%s: no memory for event status\n",
				__func__);
			return;
		}
		/*
		 * event status timestamp will be non-zero and valid if
		 * opcode is LSM_SESSION_EVENT_DETECTION_STATUS_V3
		 */
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
				spin_unlock_irqrestore(&prtd->event_lock,
							flags);
				wake_up(&prtd->event_wait);
			} else {
				spin_unlock_irqrestore(&prtd->event_lock,
							flags);
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
		dma_buf->bytes = lsm->lsm_client->hw_params.buf_sz *
		lsm->lsm_client->hw_params.period_count;
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
		struct lsm_params_info *p_info)
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
		struct lsm_params_info *p_info)
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
		struct lsm_params_info *p_info)
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
		struct lsm_params_info *p_info)
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
		struct lsm_params_info *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rc = 0;
	u8 *snd_model_ptr;
	size_t offset;

	rc = q6lsm_snd_model_buf_alloc(prtd->lsm_client,
				       p_info->param_size,
				       true);
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
	snd_model_ptr = ((u8 *) prtd->lsm_client->sound_model.data) + offset;

	if (copy_from_user(snd_model_ptr,
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
	q6lsm_snd_model_buf_free(prtd->lsm_client);
	return rc;
}

static int msm_lsm_dereg_model(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
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

	q6lsm_snd_model_buf_free(prtd->lsm_client);

	return rc;
}

static int msm_lsm_set_custom(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	u8 *data;
	int rc = 0;

	data = kzalloc(p_info->param_size, GFP_KERNEL);
	if (!data) {
		dev_err(rtd->dev,
			"%s: no memory for custom data, size = %d\n",
			__func__, p_info->param_size);
		return -ENOMEM;
	}

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

static int msm_lsm_set_poll_enable(struct snd_pcm_substream *substream,
		struct lsm_params_info *p_info)
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
	if (rc)
		dev_err(rtd->dev,
			"%s: Failed to set poll enable, err = %d\n",
			__func__, rc);
done:
	return rc;
}

static int msm_lsm_process_params(struct snd_pcm_substream *substream,
		struct snd_lsm_module_params *p_data,
		void *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct lsm_params_info *p_info;
	int i;
	int rc = 0;

	p_info = (struct lsm_params_info *) params;

	for (i = 0; i < p_data->num_params; i++) {
		dev_dbg(rtd->dev,
			"%s: param (%d), module_id = 0x%x, param_id = 0x%x, param_size = 0x%x, param_type = 0x%x\n",
			__func__, i, p_info->module_id,
			p_info->param_id, p_info->param_size,
			p_info->param_type);

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
			return rc;
		}

		p_info++;
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
	int rc = 0;
	int xchg = 0;
	struct snd_pcm_runtime *runtime;
	struct lsm_priv *prtd;
	struct snd_lsm_detection_params det_params;
	uint8_t *confidence_level = NULL;
	bool poll_en;

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
		dev_dbg(rtd->dev, "%s: set session data\n", __func__);
		if (copy_from_user(&session_data, arg,
				   sizeof(session_data))) {
			dev_err(rtd->dev, "%s: %s: copy_from_user failed\n",
				__func__, "LSM_SET_SESSION_DATA");
			return -EFAULT;
		}

		if (session_data.app_id != LSM_VOICE_WAKEUP_APP_ID_V2) {
			dev_err(rtd->dev,
				"%s:Invalid App id %d for Listen client\n",
			       __func__, session_data.app_id);
			rc = -EINVAL;
			break;
		}

		prtd->lsm_client->app_id = session_data.app_id;
		ret = q6lsm_open(prtd->lsm_client,
				 prtd->lsm_client->app_id);
		if (ret < 0) {
			dev_err(rtd->dev,
				"%s: lsm open failed, %d\n",
				__func__, ret);
			return ret;
		}
		prtd->lsm_client->opened = true;
		dev_dbg(rtd->dev, "%s: Session_ID = %d, APP ID = %d\n",
			__func__,
			prtd->lsm_client->session,
			prtd->lsm_client->app_id);
		break;
	case SNDRV_LSM_REG_SND_MODEL_V2:
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
					       snd_model_v2.data_size, false);
		if (rc) {
			dev_err(rtd->dev,
				"%s: q6lsm buffer alloc failed V2, size %d\n",
			       __func__, snd_model_v2.data_size);
			break;
		}
		if (copy_from_user(prtd->lsm_client->sound_model.data,
			   snd_model_v2.data, snd_model_v2.data_size)) {
			dev_err(rtd->dev,
				"%s: copy from user data failed\n"
			       "data %pK size %d\n", __func__,
			       snd_model_v2.data, snd_model_v2.data_size);
			q6lsm_snd_model_buf_free(prtd->lsm_client);
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
			q6lsm_snd_model_buf_free(prtd->lsm_client);
		}

		kfree(prtd->lsm_client->confidence_levels);
		prtd->lsm_client->confidence_levels = NULL;
		break;

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
		else {
			poll_en = det_params.poll_enable;
			if (prtd->lsm_client->poll_enable == poll_en) {
				dev_dbg(rtd->dev,
					"%s: Polling for session %d already %s\n",
					__func__, prtd->lsm_client->session,
					(poll_en ? "enabled" : "disabled"));
				rc = 0;
			} else {
				dev_dbg(rtd->dev, "%s: polling enable = %d\n",
					 __func__, poll_en);
				rc = q6lsm_polling_enable(prtd->lsm_client,
								poll_en);
				if (!rc)
					prtd->lsm_client->poll_enable = poll_en;
				else
					dev_err(rtd->dev,
						"%s: poll enable failed %d\n",
						__func__, rc);
			}
		}

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
			if (!rc) {
				if (prtd->lsm_client->lab_enable
					&& !prtd->lsm_client->lab_started
					&& prtd->event_status->status ==
					LSM_VOICE_WAKEUP_STATUS_DETECTED) {
					atomic_set(&prtd->read_abort, 0);
					atomic_set(&prtd->buf_count, 0);
					prtd->appl_cnt = 0;
					prtd->dma_write = 0;
					rc = msm_lsm_queue_lab_buffer(prtd,
						0);
					if (rc)
						dev_err(rtd->dev,
							"%s: Queue buffer failed for lab rc = %d\n",
							__func__, rc);
					else
						prtd->lsm_client->lab_started
						= true;
				}
			}
		} else if (xchg) {
			dev_dbg(rtd->dev, "%s: Wait aborted\n", __func__);
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
		u32 enable;

		if (copy_from_user(&enable, arg, sizeof(enable))) {
			dev_err(rtd->dev, "%s: %s: copy_frm_user failed\n",
				__func__, "LSM_LAB_CONTROL");
			return -EFAULT;
		}

		dev_dbg(rtd->dev, "%s: ioctl %s, enable = %d\n",
			 __func__, "SNDRV_LSM_LAB_CONTROL", enable);
		if (!prtd->lsm_client->started) {
			if (prtd->lsm_client->lab_enable == enable) {
				dev_dbg(rtd->dev,
					"%s: Lab for session %d already %s\n",
					__func__, prtd->lsm_client->session,
					enable ? "enabled" : "disabled");
				rc = 0;
				break;
			}
			rc = q6lsm_lab_control(prtd->lsm_client, enable);
			if (rc) {
				dev_err(rtd->dev,
					"%s: ioctl %s failed rc %d to %s lab for session %d\n",
					__func__, "SNDRV_LAB_CONTROL", rc,
					enable ? "enable" : "disable",
					prtd->lsm_client->session);
			} else {
				rc = msm_lsm_lab_buffer_alloc(prtd,
					enable ? LAB_BUFFER_ALLOC
					: LAB_BUFFER_DEALLOC);
				if (rc)
					dev_err(rtd->dev,
						"%s: msm_lsm_lab_buffer_alloc failed rc %d for %s",
						__func__, rc,
						enable ? "ALLOC" : "DEALLOC");
				if (!rc)
					prtd->lsm_client->lab_enable = enable;
			}
		} else {
			dev_err(rtd->dev, "%s: ioctl %s issued after start",
				__func__, "SNDRV_LSM_LAB_CONTROL");
			rc = -EINVAL;
		}
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
#ifdef CONFIG_COMPAT

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
	bool poll_enable;
};

struct lsm_params_info_32 {
	u32 module_id;
	u32 param_id;
	u32 param_size;
	compat_uptr_t param_data;
	enum LSM_PARAM_TYPE param_type;
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
	case SNDRV_LSM_EVENT_STATUS: {
		struct snd_lsm_event_status *user = NULL, userarg32;
		struct snd_lsm_event_status *user32 = NULL;
		if (copy_from_user(&userarg32, arg, sizeof(userarg32))) {
			dev_err(rtd->dev, "%s: err copyuser ioctl %s\n",
				__func__, "SNDRV_LSM_EVENT_STATUS");
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
			err = -EFAULT;
			goto done;
		} else {
			cmd = SNDRV_LSM_EVENT_STATUS;
			user->payload_size = userarg32.payload_size;
			err = msm_lsm_ioctl_shared(substream, cmd, user);
		}
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

	case SNDRV_LSM_EVENT_STATUS_V3_32: {
		struct snd_lsm_event_status_v3_32 userarg32, *user32 = NULL;
		struct snd_lsm_event_status_v3 *user = NULL;

		if (copy_from_user(&userarg32, arg, sizeof(userarg32))) {
			dev_err(rtd->dev, "%s: err copyuser ioctl %s\n",
				__func__, "SNDRV_LSM_EVENT_STATUS_V3_32");
			return -EFAULT;
		}

		if (userarg32.payload_size >
		    LISTEN_MAX_STATUS_PAYLOAD_SIZE) {
			pr_err("%s: payload_size %d is invalid, max allowed = %d\n",
				__func__, userarg32.payload_size,
				LISTEN_MAX_STATUS_PAYLOAD_SIZE);
			return -EINVAL;
		}

		size = sizeof(*user) + userarg32.payload_size;
		user = kzalloc(size, GFP_KERNEL);
		if (!user) {
			dev_err(rtd->dev,
				"%s: Allocation failed event status size %d\n",
				__func__, size);
			return -EFAULT;
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
			det_params.poll_enable = det_params32.poll_enable;
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

	case SNDRV_LSM_SET_MODULE_PARAMS_32: {
		struct snd_lsm_module_params_32 p_data_32;
		struct snd_lsm_module_params p_data;
		u8 *params, *params32;
		size_t p_size;
		struct lsm_params_info_32 *p_info_32;
		struct lsm_params_info *p_info;
		int i;

		if (!prtd->lsm_client->use_topology) {
			dev_err(rtd->dev,
				"%s: %s: not supported if not using topology\n",
				__func__, "SET_MODULE_PARAMS_32");
			err = -EINVAL;
			goto done;
		}

		if (copy_from_user(&p_data_32, arg,
				   sizeof(p_data_32))) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %zd\n",
				__func__, "SET_MODULE_PARAMS_32",
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
				__func__, "SET_MODULE_PARAMS_32",
				p_data.num_params);
			err = -EINVAL;
			goto done;
		}

		if (p_data.data_size !=
		    (p_data.num_params * sizeof(struct lsm_params_info_32))) {
			dev_err(rtd->dev,
				"%s: %s: Invalid size %d\n",
				__func__, "SET_MODULE_PARAMS_32",
				p_data.data_size);
			err = -EINVAL;
			goto done;
		}

		p_size = sizeof(struct lsm_params_info_32) *
			 p_data.num_params;

		params32 = kzalloc(p_size, GFP_KERNEL);
		if (!params32) {
			dev_err(rtd->dev,
				"%s: no memory for params32, size = %zd\n",
				__func__, p_size);
			err = -ENOMEM;
			goto done;
		}

		p_size = sizeof(struct lsm_params_info) * p_data.num_params;
		params = kzalloc(p_size, GFP_KERNEL);
		if (!params) {
			dev_err(rtd->dev,
				"%s: no memory for params, size = %zd\n",
				__func__, p_size);
			kfree(params32);
			err = -ENOMEM;
			goto done;
		}

		if (copy_from_user(params32, p_data.params,
				   p_data.data_size)) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %d\n",
				__func__, "params32", p_data.data_size);
			kfree(params32);
			kfree(params);
			err = -EFAULT;
			goto done;
		}

		p_info_32 = (struct lsm_params_info_32 *) params32;
		p_info = (struct lsm_params_info *) params;
		for (i = 0; i < p_data.num_params; i++) {
			p_info->module_id = p_info_32->module_id;
			p_info->param_id = p_info_32->param_id;
			p_info->param_size = p_info_32->param_size;
			p_info->param_data = compat_ptr(p_info_32->param_data);
			p_info->param_type = p_info_32->param_type;

			p_info_32++;
			p_info++;
		}

		err = msm_lsm_process_params(substream,
					     &p_data, params);
		if (err)
			dev_err(rtd->dev,
				"%s: Failed to process params, err = %d\n",
				__func__, err);
		kfree(params);
		kfree(params32);
		break;
	}
	case SNDRV_LSM_REG_SND_MODEL_V2:
	case SNDRV_LSM_SET_PARAMS:
	case SNDRV_LSM_SET_MODULE_PARAMS:
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
			 unsigned int cmd, void *arg)
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

	case SNDRV_LSM_SET_MODULE_PARAMS: {
		struct snd_lsm_module_params p_data;
		size_t p_size;
		u8 *params;

		if (!prtd->lsm_client->use_topology) {
			dev_err(rtd->dev,
				"%s: %s: not supported if not using topology\n",
				__func__, "SET_MODULE_PARAMS");
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
				__func__, "SET_MODULE_PARAMS",
				p_data.num_params);
			err = -EINVAL;
			goto done;
		}

		p_size = p_data.num_params *
			 sizeof(struct lsm_params_info);

		if (p_data.data_size != p_size) {
			dev_err(rtd->dev,
				"%s: %s: Invalid size %zd\n",
				__func__, "SET_MODULE_PARAMS", p_size);

			err = -EFAULT;
			goto done;
		}

		params = kzalloc(p_size, GFP_KERNEL);
		if (!params) {
			dev_err(rtd->dev,
				"%s: no memory for params\n",
				__func__);
			err = -ENOMEM;
			goto done;
		}

		if (copy_from_user(params, p_data.params,
				   p_data.data_size)) {
			dev_err(rtd->dev,
				"%s: %s: copy_from_user failed, size = %d\n",
				__func__, "params", p_data.data_size);
			kfree(params);
			err = -EFAULT;
			goto done;
		}

		err = msm_lsm_process_params(substream, &p_data, params);
		if (err)
			dev_err(rtd->dev,
				"%s: %s: Failed to set params, err = %d\n",
				__func__, "SET_MODULE_PARAMS", err);
		kfree(params);
		break;
	}

	case SNDRV_LSM_EVENT_STATUS: {
		struct snd_lsm_event_status *user = NULL, userarg;
		dev_dbg(rtd->dev,
			"%s: SNDRV_LSM_EVENT_STATUS\n", __func__);
		if (copy_from_user(&userarg, arg, sizeof(userarg))) {
			dev_err(rtd->dev,
				"%s: err copyuser event_status\n",
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

		size = sizeof(struct snd_lsm_event_status) +
		userarg.payload_size;
		user = kzalloc(size, GFP_KERNEL);
		if (!user) {
			dev_err(rtd->dev,
				"%s: Allocation failed event status size %d\n",
				__func__, size);
			err = -EFAULT;
			goto done;
		} else {
			user->payload_size = userarg.payload_size;
			err = msm_lsm_ioctl_shared(substream, cmd, user);
		}
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
				"%s: lsmevent failed %d", __func__, err);
		goto done;
	}

	case SNDRV_LSM_EVENT_STATUS_V3: {
		struct snd_lsm_event_status_v3 *user = NULL, userarg;

		dev_dbg(rtd->dev,
			"%s: SNDRV_LSM_EVENT_STATUS_V3\n", __func__);
		if (!arg) {
			dev_err(rtd->dev,
				"%s: Invalid params event_status_v3\n",
				__func__);
			return -EINVAL;
		}
		if (copy_from_user(&userarg, arg, sizeof(userarg))) {
			dev_err(rtd->dev,
				"%s: err copyuser event_status_v3\n",
				__func__);
			return -EFAULT;
		}

		if (userarg.payload_size >
		    LISTEN_MAX_STATUS_PAYLOAD_SIZE) {
			pr_err("%s: payload_size %d is invalid, max allowed = %d\n",
				__func__, userarg.payload_size,
				LISTEN_MAX_STATUS_PAYLOAD_SIZE);
			return -EINVAL;
		}

		size = sizeof(struct snd_lsm_event_status_v3) +
			userarg.payload_size;
		user = kzalloc(size, GFP_KERNEL);
		if (!user) {
			dev_err(rtd->dev,
				"%s: Allocation failed event status size %d\n",
				__func__, size);
			return -EFAULT;
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
				(lsm_app_cb)lsm_event_handler, prtd);
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

	return 0;
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

	if (q6lsm_set_media_fmt_params(prtd->lsm_client))
		dev_dbg(rtd->dev,
			"%s: failed to set lsm media fmt params\n", __func__);

	if (prtd->lsm_client->session_state == IDLE) {
		ret = msm_pcm_routing_reg_phy_compr_stream(
				rtd->dai_link->be_id,
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
			dev_dbg(rtd->dev, "%s: dereg_snd_model succesful\n",
				 __func__);
	}

	msm_pcm_routing_dereg_phy_stream(rtd->dai_link->be_id,
					SNDRV_PCM_STREAM_CAPTURE);

	if (prtd->lsm_client->opened) {
		q6lsm_close(prtd->lsm_client);
		prtd->lsm_client->opened = false;
	}
	q6lsm_client_free(prtd->lsm_client);

	spin_lock_irqsave(&prtd->event_lock, flags);
	kfree(prtd->event_status);
	prtd->event_status = NULL;
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
	struct lsm_hw_params *hw_params = NULL;
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
	hw_params = &prtd->lsm_client->hw_params;
	hw_params->num_chs = params_channels(params);
	hw_params->period_count = params_periods(params);
	hw_params->sample_rate = params_rate(params);
	if (((hw_params->sample_rate != 16000) &&
		(hw_params->sample_rate != 48000)) ||
		(hw_params->period_count == 0)) {
		dev_err(rtd->dev,
			"%s: Invalid Params sample rate %d period count %d\n",
			__func__, hw_params->sample_rate,
			hw_params->period_count);
		return -EINVAL;
	}

	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE) {
		hw_params->sample_size = 16;
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) {
		hw_params->sample_size = 24;
	} else {
		dev_err(rtd->dev, "%s: Invalid Format 0x%x\n",
			__func__, params_format(params));
		return -EINVAL;
	}

	hw_params->buf_sz = params_buffer_bytes(params) /
			hw_params->period_count;
	dev_dbg(rtd->dev,
		"%s: channels %d sample rate %d sample size %d buffer size %d period count %d\n",
		__func__, hw_params->num_chs, hw_params->sample_rate,
		hw_params->sample_size, hw_params->buf_sz,
		hw_params->period_count);
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
	snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lsm_priv *prtd = runtime->private_data;
	char *pcm_buf = NULL;
	int fbytes = 0, rc = 0;
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

	fbytes = frames_to_bytes(runtime, frames);
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
			"%s: Read abort recieved\n", __func__);
		return -EIO;
	}
	prtd->appl_cnt = prtd->appl_cnt %
		prtd->lsm_client->hw_params.period_count;
	pcm_buf = prtd->lsm_client->lab_buffer[prtd->appl_cnt].data;
	dev_dbg(rtd->dev,
		"%s: copy the pcm data size %d\n",
		__func__, fbytes);
	if (pcm_buf) {
		if (copy_to_user(buf, pcm_buf, fbytes)) {
			dev_err(rtd->dev,
				"%s: failed to copy bytes %d\n",
				__func__, fbytes);
			return -EINVAL;
		}
	} else {
		dev_err(rtd->dev,
			"%s: Invalid pcm buffer\n", __func__);
		return -EINVAL;
	}
	prtd->appl_cnt = (prtd->appl_cnt + 1) %
		prtd->lsm_client->hw_params.period_count;
	atomic_dec(&prtd->buf_count);
	return 0;
}

static int msm_lsm_app_type_cfg_ctl_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int app_type;
	int acdb_dev_id;
	int sample_rate;

	pr_debug("%s: fe_id- %llu\n", __func__, fe_id);
	if ((fe_id < MSM_FRONTEND_DAI_LSM1) ||
		(fe_id > MSM_FRONTEND_DAI_LSM8)) {
		pr_err("%s: Received out of bounds fe_id %llu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	app_type = ucontrol->value.integer.value[0];
	acdb_dev_id = ucontrol->value.integer.value[1];
	sample_rate = ucontrol->value.integer.value[2];

	pr_debug("%s: app_type- %d acdb_dev_id- %d sample_rate- %d session_type- %d\n",
		__func__, app_type, acdb_dev_id, sample_rate, SESSION_TYPE_TX);
	msm_pcm_routing_reg_stream_app_type_cfg(fe_id, app_type,
			acdb_dev_id, sample_rate, SESSION_TYPE_TX);

	return 0;
}

static int msm_lsm_app_type_cfg_ctl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int ret = 0;
	int app_type;
	int acdb_dev_id;
	int sample_rate;

	pr_debug("%s: fe_id- %llu\n", __func__, fe_id);
	if ((fe_id < MSM_FRONTEND_DAI_LSM1) ||
		(fe_id > MSM_FRONTEND_DAI_LSM8)) {
		pr_err("%s: Received out of bounds fe_id %llu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	ret = msm_pcm_routing_get_stream_app_type_cfg(fe_id, SESSION_TYPE_TX,
		&app_type, &acdb_dev_id, &sample_rate);
	if (ret < 0) {
		pr_err("%s: msm_pcm_routing_get_stream_app_type_cfg failed returned %d\n",
			__func__, ret);
		goto done;
	}

	ucontrol->value.integer.value[0] = app_type;
	ucontrol->value.integer.value[1] = acdb_dev_id;
	ucontrol->value.integer.value[2] = sample_rate;
	pr_debug("%s: fedai_id %llu, session_type %d, app_type %d, acdb_dev_id %d, sample_rate %d\n",
		__func__, fe_id, SESSION_TYPE_TX,
		app_type, acdb_dev_id, sample_rate);
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
				NULL, 1, ctl_len, rtd->dai_link->be_id,
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

static struct snd_pcm_ops msm_lsm_ops = {
	.open           = msm_lsm_open,
	.close          = msm_lsm_close,
	.ioctl          = msm_lsm_ioctl,
	.prepare	= msm_lsm_prepare,
	.compat_ioctl   = msm_lsm_ioctl_compat,
	.hw_params      = msm_lsm_hw_params,
	.copy           = msm_lsm_pcm_copy,
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

static int __init msm_soc_platform_init(void)
{
	return platform_driver_register(&msm_lsm_driver);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	platform_driver_unregister(&msm_lsm_driver);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("LSM client platform driver");
MODULE_DEVICE_TABLE(of, msm_lsm_client_dt_match);
MODULE_LICENSE("GPL v2");

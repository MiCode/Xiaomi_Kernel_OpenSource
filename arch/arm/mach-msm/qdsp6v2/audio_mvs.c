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
 *
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/msm_audio_mvs.h>
#include <linux/pm_qos.h>

#include <mach/qdsp6v2/q6voice.h>
#include <mach/cpuidle.h>

/* Each buffer is 20 ms, queue holds 200 ms of data. */
#define MVS_MAX_Q_LEN 10

/* Length of the DSP frame info header added to the voc packet. */
#define DSP_FRAME_HDR_LEN 1

enum audio_mvs_state_type {
	AUDIO_MVS_CLOSED,
	AUDIO_MVS_STARTED,
	AUDIO_MVS_STOPPED
};

struct audio_mvs_buf_node {
	struct list_head list;
	struct q6_msm_audio_mvs_frame frame;
};

struct audio_mvs_info_type {
	enum audio_mvs_state_type state;

	uint32_t mvs_mode;
	uint32_t rate_type;
	uint32_t dtx_mode;
	struct q_min_max_rate min_max_rate;

	struct list_head in_queue;
	struct list_head free_in_queue;

	struct list_head out_queue;
	struct list_head free_out_queue;

	wait_queue_head_t in_wait;
	wait_queue_head_t out_wait;

	struct mutex lock;
	struct mutex in_lock;
	struct mutex out_lock;

	spinlock_t dsp_lock;

	struct wake_lock suspend_lock;
	struct pm_qos_request pm_qos_req;

	void *memory_chunk;
};

static struct audio_mvs_info_type audio_mvs_info;

static uint32_t audio_mvs_get_rate(uint32_t mvs_mode, uint32_t rate_type)
{
	uint32_t cvs_rate;

	if (mvs_mode == MVS_MODE_AMR_WB)
		cvs_rate = rate_type - MVS_AMR_MODE_0660;
	else
		cvs_rate = rate_type;

	pr_debug("%s: CVS rate is %d for MVS mode %d\n",
		 __func__, cvs_rate, mvs_mode);

	return cvs_rate;
}

static void audio_mvs_process_ul_pkt(uint8_t *voc_pkt,
				     uint32_t pkt_len,
				     void *private_data)
{
	struct audio_mvs_buf_node *buf_node = NULL;
	struct audio_mvs_info_type *audio = private_data;
	unsigned long dsp_flags;

	/* Copy up-link packet into out_queue. */
	spin_lock_irqsave(&audio->dsp_lock, dsp_flags);

	if (!list_empty(&audio->free_out_queue)) {
		buf_node = list_first_entry(&audio->free_out_queue,
					    struct audio_mvs_buf_node,
					    list);
		list_del(&buf_node->list);

		switch (audio->mvs_mode) {
		case MVS_MODE_AMR:
		case MVS_MODE_AMR_WB: {
			/* Remove the DSP frame info header. Header format:
			 * Bits 0-3: Frame rate
			 * Bits 4-7: Frame type
			 */
			buf_node->frame.header.frame_type =
						((*voc_pkt) & 0xF0) >> 4;
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

			buf_node->frame.len = pkt_len - DSP_FRAME_HDR_LEN;

			memcpy(&buf_node->frame.voc_pkt[0],
			       voc_pkt,
			       buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->out_queue);
			break;
		}

		case MVS_MODE_IS127: {
			buf_node->frame.header.packet_rate = (*voc_pkt) & 0x0F;
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

			buf_node->frame.len = pkt_len - DSP_FRAME_HDR_LEN;

			memcpy(&buf_node->frame.voc_pkt[0],
			       voc_pkt,
			       buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->out_queue);
			break;
		}

		case MVS_MODE_G729A: {
			/* G729 frames are 10ms each, but the DSP works with
			 * 20ms frames and sends two 10ms frames per buffer.
			 * Extract the two frames and put them in separate
			 * buffers.
			 */
			/* Remove the first DSP frame info header.
			 * Header format:
			 * Bits 0-1: Frame type
			 */
			buf_node->frame.header.frame_type = (*voc_pkt) & 0x03;
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

			/* There are two frames in the buffer. Length of the
			 * first frame:
			 */
			buf_node->frame.len = (pkt_len -
					       2 * DSP_FRAME_HDR_LEN) / 2;

			memcpy(&buf_node->frame.voc_pkt[0],
			       voc_pkt,
			       buf_node->frame.len);
			voc_pkt = voc_pkt + buf_node->frame.len;

			list_add_tail(&buf_node->list, &audio->out_queue);

			/* Get another buffer from the free Q and fill in the
			 * second frame.
			 */
			if (!list_empty(&audio->free_out_queue)) {
				buf_node =
					list_first_entry(&audio->free_out_queue,
						      struct audio_mvs_buf_node,
						      list);
				list_del(&buf_node->list);

				/* Remove the second DSP frame info header.
				 * Header format:
				 * Bits 0-1: Frame type
				 */
				buf_node->frame.header.frame_type =
							(*voc_pkt) & 0x03;
				voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

				/* There are two frames in the buffer. Length
				 * of the first frame:
				 */
				buf_node->frame.len = (pkt_len -
						     2 * DSP_FRAME_HDR_LEN) / 2;

				memcpy(&buf_node->frame.voc_pkt[0],
				       voc_pkt,
				       buf_node->frame.len);

				list_add_tail(&buf_node->list,
					      &audio->out_queue);

			} else {
				/* Drop the second frame. */
				pr_err("%s: UL data dropped, read is slow\n",
				       __func__);
			}

			break;
		}

		case MVS_MODE_G711:
		case MVS_MODE_G711A: {
			/* G711 frames are 10ms each, but the DSP works with
			 * 20ms frames and sends two 10ms frames per buffer.
			 * Extract the two frames and put them in separate
			 * buffers.
			 */
			/* Remove the first DSP frame info header.
			 * Header format: G711A
			 * Bits 0-1: Frame type
			 * Bits 2-3: Frame rate
			 *
			 * Header format: G711
			 * Bits 2-3: Frame rate
			 */
			if (audio->mvs_mode == MVS_MODE_G711A)
				buf_node->frame.header.frame_type =
							(*voc_pkt) & 0x03;
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

			/* There are two frames in the buffer. Length of the
			 * first frame:
			 */
			buf_node->frame.len = (pkt_len -
					       2 * DSP_FRAME_HDR_LEN) / 2;

			memcpy(&buf_node->frame.voc_pkt[0],
			       voc_pkt,
			       buf_node->frame.len);
			voc_pkt = voc_pkt + buf_node->frame.len;

			list_add_tail(&buf_node->list, &audio->out_queue);

			/* Get another buffer from the free Q and fill in the
			 * second frame.
			 */
			if (!list_empty(&audio->free_out_queue)) {
				buf_node =
					list_first_entry(&audio->free_out_queue,
						      struct audio_mvs_buf_node,
						      list);
				list_del(&buf_node->list);

				/* Remove the second DSP frame info header.
				 * Header format:
				 * Bits 0-1: Frame type
				 * Bits 2-3: Frame rate
				 */
				if (audio->mvs_mode == MVS_MODE_G711A)
					buf_node->frame.header.frame_type =
							(*voc_pkt) & 0x03;
				voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

				/* There are two frames in the buffer. Length
				 * of the second frame:
				 */
				buf_node->frame.len = (pkt_len -
						     2 * DSP_FRAME_HDR_LEN) / 2;

				memcpy(&buf_node->frame.voc_pkt[0],
				       voc_pkt,
				       buf_node->frame.len);

				list_add_tail(&buf_node->list,
					      &audio->out_queue);
			} else {
				/* Drop the second frame. */
				pr_err("%s: UL data dropped, read is slow\n",
				       __func__);
			}
			break;
		}

		case MVS_MODE_IS733:
		case MVS_MODE_4GV_NB:
		case MVS_MODE_4GV_WB: {
			/* Remove the DSP frame info header.
			 * Header format:
			 * Bits 0-3: frame rate
			 */
			buf_node->frame.header.packet_rate = (*voc_pkt) & 0x0F;
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;
			buf_node->frame.len = pkt_len - DSP_FRAME_HDR_LEN;

			memcpy(&buf_node->frame.voc_pkt[0],
				voc_pkt,
				buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->out_queue);
			break;
		}

		case MVS_MODE_EFR:
		case MVS_MODE_FR:
		case MVS_MODE_HR: {
			/*
			 * Remove the DSP frame info header
			 * Header Format
			 * Bit 0: bfi unused for uplink
			 * Bit 1-2: sid applies to both uplink and downlink
			 * Bit 3: taf unused for uplink
			 * MVS_MODE_HR
			 * Bit 4: ufi unused for uplink
			 */
			buf_node->frame.header.gsm_frame_type.sid =
						((*voc_pkt) & 0x06) >> 1;
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;
			buf_node->frame.len = pkt_len - DSP_FRAME_HDR_LEN;

			memcpy(&buf_node->frame.voc_pkt[0],
			voc_pkt,
			buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->out_queue);
			break;
		}

		default: {
			buf_node->frame.header.frame_type = 0;

			buf_node->frame.len = pkt_len;

			memcpy(&buf_node->frame.voc_pkt[0],
			       voc_pkt,
			       buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->out_queue);
		}
		}
	} else {
		pr_err("%s: UL data dropped, read is slow\n", __func__);
	}

	spin_unlock_irqrestore(&audio->dsp_lock, dsp_flags);

	wake_up(&audio->out_wait);
}

static void audio_mvs_process_dl_pkt(uint8_t *voc_pkt,
				     uint32_t *pkt_len,
				     void *private_data)
{
	struct audio_mvs_buf_node *buf_node = NULL;
	struct audio_mvs_info_type *audio = private_data;
	unsigned long dsp_flags;

	spin_lock_irqsave(&audio->dsp_lock, dsp_flags);

	if (!list_empty(&audio->in_queue)) {
		uint32_t rate_type = audio_mvs_get_rate(audio->mvs_mode,
							audio->rate_type);

		buf_node = list_first_entry(&audio->in_queue,
					    struct audio_mvs_buf_node,
					    list);
		list_del(&buf_node->list);

		switch (audio->mvs_mode) {
		case MVS_MODE_AMR:
		case MVS_MODE_AMR_WB: {
			/* Add the DSP frame info header. Header format:
			 * Bits 0-3: Frame rate
			 * Bits 4-7: Frame type
			 */
			*voc_pkt =
			    ((buf_node->frame.header.frame_type & 0x0F) << 4) |
			    (rate_type & 0x0F);
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

			*pkt_len = buf_node->frame.len + DSP_FRAME_HDR_LEN;

			memcpy(voc_pkt,
			       &buf_node->frame.voc_pkt[0],
			       buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->free_in_queue);
			break;
		}

		case MVS_MODE_IS127: {
			/* Add the DSP frame info header. Header format:
			 * Bits 0-3: Frame rate
			 */
			*voc_pkt = buf_node->frame.header.packet_rate & 0x0F;
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

			*pkt_len = buf_node->frame.len + DSP_FRAME_HDR_LEN;

			memcpy(voc_pkt,
			       &buf_node->frame.voc_pkt[0],
			       buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->free_in_queue);
			break;
		}

		case MVS_MODE_G729A: {
			/* G729 frames are 10ms each but the DSP expects 20ms
			 * worth of data, so send two 10ms frames per buffer.
			 */
			/* Add the first DSP frame info header. Header format:
			 * Bits 0-1: Frame type
			 */
			*voc_pkt = buf_node->frame.header.frame_type & 0x03;
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

			*pkt_len = buf_node->frame.len + DSP_FRAME_HDR_LEN;

			memcpy(voc_pkt,
			       &buf_node->frame.voc_pkt[0],
			       buf_node->frame.len);
			voc_pkt = voc_pkt + buf_node->frame.len;

			list_add_tail(&buf_node->list, &audio->free_in_queue);

			if (!list_empty(&audio->in_queue)) {
				/* Get the second buffer. */
				buf_node = list_first_entry(&audio->in_queue,
						      struct audio_mvs_buf_node,
						      list);
				list_del(&buf_node->list);

				/* Add the second DSP frame info header.
				 * Header format:
				 * Bits 0-1: Frame type
				 */
				*voc_pkt = buf_node->frame.header.frame_type
						& 0x03;
				voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

				*pkt_len = *pkt_len +
					buf_node->frame.len + DSP_FRAME_HDR_LEN;

				memcpy(voc_pkt,
				       &buf_node->frame.voc_pkt[0],
				       buf_node->frame.len);

				list_add_tail(&buf_node->list,
					      &audio->free_in_queue);
			} else {
				/* Only 10ms worth of data is available, signal
				 * erasure frame.
				 */
				*voc_pkt = MVS_G729A_ERASURE & 0x03;

				*pkt_len = *pkt_len + DSP_FRAME_HDR_LEN;
			}

			break;
		}

		case MVS_MODE_G711:
		case MVS_MODE_G711A: {
			/* G711 frames are 10ms each but the DSP expects 20ms
			 * worth of data, so send two 10ms frames per buffer.
			 */
			/* Add the first DSP frame info header. Header format:
			 * Bits 0-1: Frame type
			 * Bits 2-3: Frame rate
			 */
			*voc_pkt = ((rate_type & 0x0F) << 2) |
				   (buf_node->frame.header.frame_type & 0x03);
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

			*pkt_len = buf_node->frame.len + DSP_FRAME_HDR_LEN;

			memcpy(voc_pkt,
			       &buf_node->frame.voc_pkt[0],
			       buf_node->frame.len);
			voc_pkt = voc_pkt + buf_node->frame.len;

			list_add_tail(&buf_node->list, &audio->free_in_queue);

			if (!list_empty(&audio->in_queue)) {
				/* Get the second buffer. */
				buf_node = list_first_entry(&audio->in_queue,
						      struct audio_mvs_buf_node,
						      list);
				list_del(&buf_node->list);

				/* Add the second DSP frame info header.
				 * Header format:
				 * Bits 0-1: Frame type
				 * Bits 2-3: Frame rate
				 */
				*voc_pkt = ((rate_type & 0x0F) << 2) |
				     (buf_node->frame.header.frame_type & 0x03);
				voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;

				*pkt_len = *pkt_len +
					buf_node->frame.len + DSP_FRAME_HDR_LEN;

				memcpy(voc_pkt,
				       &buf_node->frame.voc_pkt[0],
				       buf_node->frame.len);

				list_add_tail(&buf_node->list,
					      &audio->free_in_queue);
			} else {
				/* Only 10ms worth of data is available, signal
				 * erasure frame.
				 */
				*voc_pkt = ((rate_type & 0x0F) << 2) |
					   (MVS_G711A_ERASURE & 0x03);

				*pkt_len = *pkt_len + DSP_FRAME_HDR_LEN;
			}
			break;
		}

		case MVS_MODE_IS733:
		case MVS_MODE_4GV_NB:
		case MVS_MODE_4GV_WB: {
			/* Add the DSP frame info header. Header format:
			 * Bits 0-3 : Frame rate
			*/
			*voc_pkt = buf_node->frame.header.packet_rate & 0x0F;
			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;
			*pkt_len = buf_node->frame.len + DSP_FRAME_HDR_LEN;

			memcpy(voc_pkt,
				&buf_node->frame.voc_pkt[0],
				buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->free_in_queue);
			break;
		}

		case MVS_MODE_EFR:
		case MVS_MODE_FR:
		case MVS_MODE_HR: {
			/*
			 * Remove the DSP frame info header
			 * Header Format
			 * Bit 0: bfi applies only for downlink
			 * Bit 1-2: sid applies for downlink and uplink
			 * Bit 3: taf applies only for downlink
			 * MVS_MODE_HR
			 * Bit 4: ufi applies only for downlink
			 */
			*voc_pkt =
				((buf_node->frame.header.gsm_frame_type.bfi
					& 0x01) |
				((buf_node->frame.header.gsm_frame_type.sid
					& 0x03) << 1) |
				((buf_node->frame.header.gsm_frame_type.taf
					& 0x01) << 3));

			if (audio->mvs_mode == MVS_MODE_HR) {
				*voc_pkt = (*voc_pkt |
				((buf_node->frame.header.gsm_frame_type.ufi
				& 0x01) << 4) |
				((0 & 0x07) << 5));
			} else {
				*voc_pkt = (*voc_pkt |
				((0 & 0x0F) << 4));
			}

			voc_pkt = voc_pkt + DSP_FRAME_HDR_LEN;
			*pkt_len = buf_node->frame.len + DSP_FRAME_HDR_LEN;

			memcpy(voc_pkt,
				&buf_node->frame.voc_pkt[0],
				buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->free_in_queue);

			break;
		}

		default: {
			*pkt_len = buf_node->frame.len;

			memcpy(voc_pkt,
			       &buf_node->frame.voc_pkt[0],
			       buf_node->frame.len);

			list_add_tail(&buf_node->list, &audio->free_in_queue);
		}
		}
	} else {
		*pkt_len = 0;

		pr_info("%s: No DL data available to send to MVS\n", __func__);
	}

	spin_unlock_irqrestore(&audio->dsp_lock, dsp_flags);
	wake_up(&audio->in_wait);
}

static uint32_t audio_mvs_get_media_type(uint32_t mvs_mode, uint32_t rate_type)
{
	uint32_t media_type;

	switch (mvs_mode) {
	case MVS_MODE_IS733:
		media_type = VSS_MEDIA_ID_13K_MODEM;
		break;

	case MVS_MODE_IS127:
		media_type = VSS_MEDIA_ID_EVRC_MODEM;
		break;

	case MVS_MODE_4GV_NB:
		media_type = VSS_MEDIA_ID_4GV_NB_MODEM;
		break;

	case MVS_MODE_4GV_WB:
		media_type = VSS_MEDIA_ID_4GV_WB_MODEM;
		break;

	case MVS_MODE_AMR:
		media_type = VSS_MEDIA_ID_AMR_NB_MODEM;
		break;

	case MVS_MODE_EFR:
		media_type = VSS_MEDIA_ID_EFR_MODEM;
		break;

	case MVS_MODE_FR:
		media_type = VSS_MEDIA_ID_FR_MODEM;
		break;

	case MVS_MODE_HR:
		media_type = VSS_MEDIA_ID_HR_MODEM;
		break;

	case MVS_MODE_LINEAR_PCM:
		media_type = VSS_MEDIA_ID_PCM_NB;
		break;

	case MVS_MODE_PCM:
		media_type = VSS_MEDIA_ID_PCM_NB;
		break;

	case MVS_MODE_AMR_WB:
		media_type = VSS_MEDIA_ID_AMR_WB_MODEM;
		break;

	case MVS_MODE_G729A:
		media_type = VSS_MEDIA_ID_G729;
		break;

	case MVS_MODE_G711:
	case MVS_MODE_G711A:
		if (rate_type == MVS_G711A_MODE_MULAW)
			media_type = VSS_MEDIA_ID_G711_MULAW;
		else
			media_type = VSS_MEDIA_ID_G711_ALAW;
		break;

	case MVS_MODE_PCM_WB:
		media_type = VSS_MEDIA_ID_PCM_WB;
		break;

	default:
		media_type = VSS_MEDIA_ID_PCM_NB;
	}

	pr_debug("%s: media_type is 0x%x\n", __func__, media_type);

	return media_type;
}

static uint32_t audio_mvs_get_network_type(uint32_t mvs_mode)
{
	uint32_t network_type;

	switch (mvs_mode) {
	case MVS_MODE_IS733:
	case MVS_MODE_IS127:
	case MVS_MODE_4GV_NB:
	case MVS_MODE_AMR:
	case MVS_MODE_EFR:
	case MVS_MODE_FR:
	case MVS_MODE_HR:
	case MVS_MODE_LINEAR_PCM:
	case MVS_MODE_G711:
	case MVS_MODE_PCM:
	case MVS_MODE_G729A:
	case MVS_MODE_G711A:
		network_type = VSS_NETWORK_ID_VOIP_NB;
		break;

	case MVS_MODE_4GV_WB:
	case MVS_MODE_AMR_WB:
	case MVS_MODE_PCM_WB:
		network_type = VSS_NETWORK_ID_VOIP_WB;
		break;

	default:
		network_type = VSS_NETWORK_ID_DEFAULT;
	}

	pr_debug("%s: network_type is 0x%x\n", __func__, network_type);

	return network_type;
}

static int audio_mvs_start(struct audio_mvs_info_type *audio)
{
	int rc = 0;

	pr_info("%s\n", __func__);

	/* Prevent sleep. */
	wake_lock(&audio->suspend_lock);
	pm_qos_update_request(&audio->pm_qos_req,
			msm_cpuidle_get_deep_idle_latency());

	rc = voice_set_voc_path_full(1);

	if (rc == 0) {
		voice_register_mvs_cb(audio_mvs_process_ul_pkt,
				      audio_mvs_process_dl_pkt,
				      audio);

		voice_config_vocoder(
		    audio_mvs_get_media_type(audio->mvs_mode, audio->rate_type),
		    audio_mvs_get_rate(audio->mvs_mode, audio->rate_type),
		    audio_mvs_get_network_type(audio->mvs_mode),
		    audio->dtx_mode,
		    audio->min_max_rate);

		audio->state = AUDIO_MVS_STARTED;
	} else {
		pr_err("%s: Error %d setting voc path to full\n", __func__, rc);
	}

	return rc;
}

static int audio_mvs_stop(struct audio_mvs_info_type *audio)
{
	int rc = 0;

	pr_info("%s\n", __func__);

	voice_set_voc_path_full(0);

	audio->state = AUDIO_MVS_STOPPED;

	/* Allow sleep. */
	pm_qos_update_request(&audio->pm_qos_req, PM_QOS_DEFAULT_VALUE);
	wake_unlock(&audio->suspend_lock);

	return rc;
}

static int audio_mvs_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	int i;
	int offset = 0;
	struct audio_mvs_buf_node *buf_node = NULL;

	pr_info("%s\n", __func__);

	mutex_lock(&audio_mvs_info.lock);

	/* Allocate input and output buffers. */
	audio_mvs_info.memory_chunk = kmalloc(2 * MVS_MAX_Q_LEN *
					      sizeof(struct audio_mvs_buf_node),
					      GFP_KERNEL);

	if (audio_mvs_info.memory_chunk != NULL) {
		for (i = 0; i < MVS_MAX_Q_LEN; i++) {
			buf_node = audio_mvs_info.memory_chunk + offset;

			list_add_tail(&buf_node->list,
				      &audio_mvs_info.free_in_queue);

			offset = offset + sizeof(struct audio_mvs_buf_node);
		}

		for (i = 0; i < MVS_MAX_Q_LEN; i++) {
			buf_node = audio_mvs_info.memory_chunk + offset;

			list_add_tail(&buf_node->list,
				      &audio_mvs_info.free_out_queue);

			offset = offset + sizeof(struct audio_mvs_buf_node);
		}

		audio_mvs_info.state = AUDIO_MVS_STOPPED;

		file->private_data = &audio_mvs_info;

	}  else {
		pr_err("%s: No memory for IO buffers\n", __func__);

		rc = -ENOMEM;
	}

	mutex_unlock(&audio_mvs_info.lock);

	return rc;
}

static int audio_mvs_release(struct inode *inode, struct file *file)
{
	struct list_head *ptr = NULL;
	struct list_head *next = NULL;
	struct audio_mvs_buf_node *buf_node = NULL;
	struct audio_mvs_info_type *audio = file->private_data;

	pr_info("%s\n", __func__);

	mutex_lock(&audio->lock);

	if (audio->state == AUDIO_MVS_STARTED)
		audio_mvs_stop(audio);

	/* Free input and output memory. */
	mutex_lock(&audio->in_lock);

	list_for_each_safe(ptr, next, &audio->in_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
	}

	list_for_each_safe(ptr, next, &audio->free_in_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
	}

	mutex_unlock(&audio->in_lock);


	mutex_lock(&audio->out_lock);

	list_for_each_safe(ptr, next, &audio->out_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
	}

	list_for_each_safe(ptr, next, &audio->free_out_queue) {
		buf_node = list_entry(ptr, struct audio_mvs_buf_node, list);
		list_del(&buf_node->list);
	}

	mutex_unlock(&audio->out_lock);

	kfree(audio->memory_chunk);
	audio->memory_chunk = NULL;

	audio->state = AUDIO_MVS_CLOSED;

	mutex_unlock(&audio->lock);

	return 0;
}

static ssize_t audio_mvs_read(struct file *file,
			      char __user *buf,
			      size_t count,
			      loff_t *pos)
{
	int rc = 0;
	struct audio_mvs_buf_node *buf_node = NULL;
	struct audio_mvs_info_type *audio = file->private_data;

	pr_debug("%s:\n", __func__);

	rc = wait_event_interruptible_timeout(audio->out_wait,
					     (!list_empty(&audio->out_queue) ||
					     audio->state == AUDIO_MVS_STOPPED),
					     1 * HZ);

	if (rc > 0) {
		mutex_lock(&audio->out_lock);

		if ((audio->state == AUDIO_MVS_STARTED) &&
		    (!list_empty(&audio->out_queue))) {

			if (count >= sizeof(struct q6_msm_audio_mvs_frame)) {
				buf_node = list_first_entry(&audio->out_queue,
						struct audio_mvs_buf_node,
						list);
				list_del(&buf_node->list);

				rc = copy_to_user(buf,
					&buf_node->frame,
					sizeof(struct q6_msm_audio_mvs_frame));

				if (rc == 0) {
					rc = buf_node->frame.len +
					    sizeof(buf_node->frame.header) +
					    sizeof(buf_node->frame.len);
				} else {
					pr_err("%s: Copy to user retuned %d",
					       __func__, rc);

					rc = -EFAULT;
				}

				list_add_tail(&buf_node->list,
					      &audio->free_out_queue);
			} else {
				pr_err("%s: Read count %d < sizeof(frame) %d",
				       __func__, count,
				       sizeof(struct q6_msm_audio_mvs_frame));

				rc = -ENOMEM;
			}
		} else {
			pr_err("%s: Read performed in state %d\n",
			       __func__, audio->state);

			rc = -EPERM;
		}

		mutex_unlock(&audio->out_lock);

	} else if (rc == 0) {
		pr_err("%s: No UL data available\n", __func__);

		rc = -ETIMEDOUT;
	} else {
		pr_err("%s: Read was interrupted\n", __func__);

		rc = -ERESTARTSYS;
	}

	return rc;
}

static ssize_t audio_mvs_write(struct file *file,
			       const char __user *buf,
			       size_t count,
			       loff_t *pos)
{
	int rc = 0;
	struct audio_mvs_buf_node *buf_node = NULL;
	struct audio_mvs_info_type *audio = file->private_data;

	pr_debug("%s:\n", __func__);

	rc = wait_event_interruptible_timeout(audio->in_wait,
		(!list_empty(&audio->free_in_queue) ||
		audio->state == AUDIO_MVS_STOPPED), 1 * HZ);
	if (rc > 0) {
		mutex_lock(&audio->in_lock);

		if (audio->state == AUDIO_MVS_STARTED) {
			if (count <= sizeof(struct q6_msm_audio_mvs_frame)) {
				if (!list_empty(&audio->free_in_queue)) {
					buf_node =
					list_first_entry(&audio->free_in_queue,
					 struct audio_mvs_buf_node, list);
					list_del(&buf_node->list);
					rc = copy_from_user(&buf_node->frame,
							    buf,
							    count);

					list_add_tail(&buf_node->list,
						      &audio->in_queue);
				} else {
					pr_err("%s: No free DL buffs\n",
						__func__);
				}
			} else {
				pr_err("%s: Write count %d < sizeof(frame) %d",
				       __func__, count,
				       sizeof(struct q6_msm_audio_mvs_frame));

				rc = -ENOMEM;
			}
		} else {
			pr_err("%s: Write performed in invalid state %d\n",
			       __func__, audio->state);

			rc = -EPERM;
		}

		mutex_unlock(&audio->in_lock);
	} else if (rc == 0) {
		pr_err("%s: No free DL buffs\n", __func__);

		rc = -ETIMEDOUT;
	} else {
		pr_err("%s: write was interrupted\n", __func__);

		rc = -ERESTARTSYS;
	}

	return rc;
}

static long audio_mvs_ioctl(struct file *file,
			    unsigned int cmd,
			    unsigned long arg)
{
	int rc = 0;
	struct audio_mvs_info_type *audio = file->private_data;

	pr_info("%s:\n", __func__);

	switch (cmd) {
	case AUDIO_GET_MVS_CONFIG: {
		struct msm_audio_mvs_config config;

		pr_info("%s: IOCTL GET_MVS_CONFIG\n", __func__);

		mutex_lock(&audio->lock);

		config.mvs_mode = audio->mvs_mode;
		config.rate_type = audio->rate_type;
		config.dtx_mode = audio->dtx_mode;
		config.min_max_rate.min_rate = audio->min_max_rate.min_rate;
		config.min_max_rate.max_rate = audio->min_max_rate.max_rate;
		mutex_unlock(&audio->lock);

		rc = copy_to_user((void *)arg, &config, sizeof(config));
		if (rc == 0)
			rc = sizeof(config);
		else
			pr_err("%s: Config copy failed %d\n", __func__, rc);

		break;
	}

	case AUDIO_SET_MVS_CONFIG: {
		struct msm_audio_mvs_config config;

		pr_info("%s: IOCTL SET_MVS_CONFIG\n", __func__);

		rc = copy_from_user(&config, (void *)arg, sizeof(config));
		if (rc == 0) {
			mutex_lock(&audio->lock);

			if (audio->state == AUDIO_MVS_STOPPED) {
				audio->mvs_mode = config.mvs_mode;
				audio->rate_type = config.rate_type;
				audio->dtx_mode = config.dtx_mode;
				audio->min_max_rate.min_rate =
						config.min_max_rate.min_rate;
				audio->min_max_rate.max_rate =
						config.min_max_rate.max_rate;
			} else {
				pr_err("%s: Set confg called in state %d\n",
				       __func__, audio->state);

				rc = -EPERM;
			}

			mutex_unlock(&audio->lock);
		} else {
			pr_err("%s: Config copy failed %d\n", __func__, rc);
		}

		break;
	}

	case AUDIO_START: {
		pr_info("%s: IOCTL START\n", __func__);

		mutex_lock(&audio->lock);

		if (audio->state == AUDIO_MVS_STOPPED) {
			rc = audio_mvs_start(audio);

			if (rc != 0)
				audio_mvs_stop(audio);
		} else {
			pr_err("%s: Start called in invalid state %d\n",
			       __func__, audio->state);

			rc = -EPERM;
		}

		mutex_unlock(&audio->lock);

		break;
	}

	case AUDIO_STOP: {
		pr_info("%s: IOCTL STOP\n", __func__);

		mutex_lock(&audio->lock);

		if (audio->state == AUDIO_MVS_STARTED) {
			rc = audio_mvs_stop(audio);
		} else {
			pr_err("%s: Stop called in invalid state %d\n",
			       __func__, audio->state);

			rc = -EPERM;
		}

		mutex_unlock(&audio->lock);

		break;
	}

	default: {
		pr_err("%s: Unknown IOCTL %d\n", __func__, cmd);
	}
	}

	return rc;
}

static const struct file_operations audio_mvs_fops = {
	.owner = THIS_MODULE,
	.open = audio_mvs_open,
	.release = audio_mvs_release,
	.read = audio_mvs_read,
	.write = audio_mvs_write,
	.unlocked_ioctl = audio_mvs_ioctl
};

struct miscdevice audio_mvs_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_mvs",
	.fops = &audio_mvs_fops
};

static int __init audio_mvs_init(void)
{
	int rc = 0;

	memset(&audio_mvs_info, 0, sizeof(audio_mvs_info));

	init_waitqueue_head(&audio_mvs_info.in_wait);
	init_waitqueue_head(&audio_mvs_info.out_wait);

	mutex_init(&audio_mvs_info.lock);
	mutex_init(&audio_mvs_info.in_lock);
	mutex_init(&audio_mvs_info.out_lock);

	spin_lock_init(&audio_mvs_info.dsp_lock);

	INIT_LIST_HEAD(&audio_mvs_info.in_queue);
	INIT_LIST_HEAD(&audio_mvs_info.free_in_queue);
	INIT_LIST_HEAD(&audio_mvs_info.out_queue);
	INIT_LIST_HEAD(&audio_mvs_info.free_out_queue);

	wake_lock_init(&audio_mvs_info.suspend_lock,
		       WAKE_LOCK_SUSPEND,
		       "audio_mvs_suspend");
	pm_qos_add_request(&audio_mvs_info.pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	rc = misc_register(&audio_mvs_misc);

	return rc;
}

static void __exit audio_mvs_exit(void){
	pr_info("%s:\n", __func__);

	misc_deregister(&audio_mvs_misc);
}

module_init(audio_mvs_init);
module_exit(audio_mvs_exit);

MODULE_DESCRIPTION("MSM MVS driver");
MODULE_LICENSE("GPL v2");

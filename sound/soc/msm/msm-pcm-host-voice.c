/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include "qdsp6/q6voice.h"

#define HPCM_MAX_Q_LEN 10
#define HPCM_MIN_VOC_PKT_SIZE 320
#define HPCM_MAX_VOC_PKT_SIZE 640

#define VOICE_TX_CAPTURE_DAI_ID  "CS-VOICE HOST TX CAPTURE"
#define VOICE_TX_PLAYBACK_DAI_ID "CS-VOICE HOST TX PLAYBACK"
#define VOICE_RX_CAPTURE_DAI_ID  "CS-VOICE HOST RX CAPTURE"
#define VOICE_RX_PLAYBACK_DAI_ID "CS-VOICE HOST RX PLAYBACK"

#define VOLTE_TX_CAPTURE_DAI_ID  "VOLTE HOST TX CAPTURE"
#define VOLTE_TX_PLAYBACK_DAI_ID "VOLTE HOST TX PLAYBACK"
#define VOLTE_RX_CAPTURE_DAI_ID  "VOLTE HOST RX CAPTURE"
#define VOLTE_RX_PLAYBACK_DAI_ID "VOLTE HOST RX PLAYBACK"

enum {
	RX = 1,
	TX,
};

enum {
	VOICE_INDEX = 0,
	VOLTE_INDEX,
	MAX_SESSION
};

enum hpcm_state {
	HPCM_STOPPED = 1,
	HPCM_CLOSED,
	HPCM_PREPARED,
	HPCM_STARTED,
};

struct hpcm_frame {
	uint32_t len;
	uint8_t voc_pkt[HPCM_MAX_VOC_PKT_SIZE];
};

struct hpcm_buf_node {
	struct list_head list;
	struct hpcm_frame frame;
};

struct vocpcm_ion_buffer {
	/* Physical address */
	uint32_t paddr;
	/* Kernel virtual address */
	uint32_t kvaddr;
};

struct dai_data {
	enum  hpcm_state state;
	struct snd_pcm_substream *substream;
	struct list_head filled_queue;
	struct list_head free_queue;
	wait_queue_head_t queue_wait;
	spinlock_t dsp_lock;
	uint32_t pcm_size;
	uint32_t pcm_count;
	/* IRQ position */
	uint32_t pcm_irq_pos;
	/* Position in buffer */
	uint32_t pcm_buf_pos;
	struct vocpcm_ion_buffer vocpcm_ion_buffer;
};

struct tap_point {
	struct dai_data playback_dai_data;
	struct dai_data capture_dai_data;
	struct ion_handle *ion_handle;
	int ion_mem_len;
};

struct session {
	struct tap_point tx_tap_point;
	struct tap_point rx_tap_point;
};

struct tappnt_mxr_data {
	bool enable;
	uint16_t direction;
	uint16_t sample_rate;
};

/* Values from mixer ctl are cached in this structure */
struct mixer_conf {
	uint8_t sess_indx;
	struct tappnt_mxr_data rx;
	struct tappnt_mxr_data tx;
};

struct start_cmd {
	struct vss_ivpcm_tap_point tap_pnt[2];
	uint32_t no_of_tapoints;
};

struct hpcm_drv {
	struct mutex lock;
	struct session session[MAX_SESSION];
	struct mixer_conf mixer_conf;
	struct ion_client *ion_client;
	struct start_cmd start_cmd;
};

static struct hpcm_drv hpcm_drv;

static struct snd_pcm_hardware msm_pcm_hardware = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED),
	.formats =              SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_SPECIAL,
	.rates =                SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
	.rate_min =             8000,
	.rate_max =             16000,
	.channels_min =         1,
	.channels_max =         1,
	.buffer_bytes_max =	sizeof(struct hpcm_buf_node) * HPCM_MAX_Q_LEN,
	.period_bytes_min =	HPCM_MIN_VOC_PKT_SIZE,
	.period_bytes_max =	HPCM_MAX_VOC_PKT_SIZE,
	.periods_min =		HPCM_MAX_Q_LEN,
	.periods_max =		HPCM_MAX_Q_LEN,
	.fifo_size =            0,
};

static char *hpcm_get_sess_name(int sess_indx)
{
	char *sess_name = NULL;

	if (sess_indx == VOICE_INDEX)
		sess_name = VOICE_SESSION_NAME;
	else if (sess_indx == VOLTE_INDEX)
		sess_name = VOLTE_SESSION_NAME;
	else
		pr_err("%s:, Invalid sess_index\n", __func__);

	return sess_name;
}

static void hpcm_reset_mixer_config(struct hpcm_drv *prtd)
{
	prtd->mixer_conf.sess_indx = -1;
	prtd->mixer_conf.rx.enable = false;
	prtd->mixer_conf.rx.direction = -1;
	prtd->mixer_conf.rx.sample_rate = 0;

	prtd->mixer_conf.tx.enable = false;
	prtd->mixer_conf.tx.direction = -1;
	prtd->mixer_conf.tx.sample_rate = 0;
}

/* Check for valid mixer control values */
static bool hpcm_is_valid_config(int sess_indx, int tap_point,
				 uint16_t direction, uint16_t samplerate)
{
	if (sess_indx < VOICE_INDEX || sess_indx > VOLTE_INDEX) {
		pr_err("%s: invalid sess_indx :%d\n", __func__, sess_indx);
		goto error;
	}

	if (samplerate != VSS_IVPCM_SAMPLING_RATE_8K &&
	    samplerate != VSS_IVPCM_SAMPLING_RATE_16K) {
		pr_err("%s: invalid sample rate :%d\n", __func__, samplerate);
		goto error;
	}

	if ((tap_point != RX) && (tap_point != TX)) {
		pr_err("%s: invalid tappoint :%d\n", __func__, tap_point);
		goto error;
	}

	if ((direction != VSS_IVPCM_TAP_POINT_DIR_IN) &&
	    (direction != VSS_IVPCM_TAP_POINT_DIR_OUT) &&
	    (direction != VSS_IVPCM_TAP_POINT_DIR_OUT_IN)) {
		pr_err("%s: invalid direction :%d\n", __func__, direction);
		goto error;
	}

	return true;

error:
	return false;
}


static struct dai_data *hpcm_get_dai_data(char *pcm_id, struct hpcm_drv *prtd)
{
	struct dai_data *dai_data = NULL;
	size_t size = 0;

	if (pcm_id) {
		size = strlen(pcm_id);
		/* Check for Voice DAI */
		if (strnstr(pcm_id, VOICE_TX_CAPTURE_DAI_ID, size)) {
			dai_data =
		&prtd->session[VOICE_INDEX].tx_tap_point.capture_dai_data;
		} else if (strnstr(pcm_id, VOICE_TX_PLAYBACK_DAI_ID, size)) {
			dai_data =
		&prtd->session[VOICE_INDEX].tx_tap_point.playback_dai_data;
		} else if (strnstr(pcm_id, VOICE_RX_CAPTURE_DAI_ID, size)) {
			dai_data =
		&prtd->session[VOICE_INDEX].rx_tap_point.capture_dai_data;
		} else if (strnstr(pcm_id, VOICE_RX_PLAYBACK_DAI_ID, size)) {
			dai_data =
		&prtd->session[VOICE_INDEX].rx_tap_point.playback_dai_data;
		/* Check for VoLTE DAI */
		} else if (strnstr(pcm_id, VOLTE_TX_CAPTURE_DAI_ID, size)) {
			dai_data =
		&prtd->session[VOLTE_INDEX].tx_tap_point.capture_dai_data;
		} else if (strnstr(pcm_id, VOLTE_TX_PLAYBACK_DAI_ID, size)) {
			dai_data =
		&prtd->session[VOLTE_INDEX].tx_tap_point.playback_dai_data;
		} else if (strnstr(pcm_id, VOLTE_RX_CAPTURE_DAI_ID, size)) {
			dai_data =
		&prtd->session[VOLTE_INDEX].rx_tap_point.capture_dai_data;
		} else if (strnstr(pcm_id, VOLTE_RX_PLAYBACK_DAI_ID, size)) {
			dai_data =
		&prtd->session[VOLTE_INDEX].rx_tap_point.playback_dai_data;
		} else {
			pr_err("%s: Wrong dai id\n", __func__);
		}
	}

	return dai_data;
}

static struct tap_point *hpcm_get_tappoint_data(char *pcm_id,
						struct hpcm_drv *prtd)
{
	struct tap_point *tp = NULL;
	size_t size = strlen(pcm_id);

	if (pcm_id) {
		/* Check for Voice DAI */
		if (strnstr(pcm_id, VOICE_TX_CAPTURE_DAI_ID, size)) {
			tp = &prtd->session[VOICE_INDEX].tx_tap_point;
		} else if (strnstr(pcm_id, VOICE_TX_PLAYBACK_DAI_ID, size)) {
			tp = &prtd->session[VOICE_INDEX].tx_tap_point;
		} else if (strnstr(pcm_id, VOICE_RX_CAPTURE_DAI_ID, size)) {
			tp = &prtd->session[VOICE_INDEX].rx_tap_point;
		} else if (strnstr(pcm_id, VOICE_RX_PLAYBACK_DAI_ID, size)) {
			tp = &prtd->session[VOICE_INDEX].rx_tap_point;
		/* Check for VoLTE DAI */
		} else if (strnstr(pcm_id, VOLTE_TX_CAPTURE_DAI_ID, size)) {
			tp = &prtd->session[VOLTE_INDEX].tx_tap_point;
		} else if (strnstr(pcm_id, VOLTE_TX_PLAYBACK_DAI_ID, size)) {
			tp = &prtd->session[VOLTE_INDEX].tx_tap_point;
		} else if (strnstr(pcm_id, VOLTE_RX_CAPTURE_DAI_ID, size)) {
			tp = &prtd->session[VOLTE_INDEX].rx_tap_point;
		} else if (strnstr(pcm_id, VOLTE_RX_PLAYBACK_DAI_ID, size)) {
			tp = &prtd->session[VOLTE_INDEX].rx_tap_point;
		} else {
			pr_err("%s: wrong dai id\n", __func__);
		}
	}

	return tp;
}

static struct tappnt_mxr_data *hpcm_get_tappnt_mixer_data(char *pcm_id,
						struct hpcm_drv *prtd)
{

	if (strnstr(pcm_id, VOICE_TX_CAPTURE_DAI_ID, strlen(pcm_id)) ||
	    strnstr(pcm_id, VOICE_TX_PLAYBACK_DAI_ID, strlen(pcm_id)) ||
	    strnstr(pcm_id, VOLTE_TX_CAPTURE_DAI_ID, strlen(pcm_id)) ||
	    strnstr(pcm_id, VOLTE_TX_PLAYBACK_DAI_ID, strlen(pcm_id))) {
		return &prtd->mixer_conf.tx;
	} else {
		return &prtd->mixer_conf.rx;
	}
}

static int get_tappnt_value(char *pcm_id)
{

	if (strnstr(pcm_id, VOICE_TX_CAPTURE_DAI_ID, strlen(pcm_id)) ||
	    strnstr(pcm_id, VOICE_TX_PLAYBACK_DAI_ID, strlen(pcm_id)) ||
	    strnstr(pcm_id, VOLTE_TX_CAPTURE_DAI_ID, strlen(pcm_id)) ||
	    strnstr(pcm_id, VOLTE_TX_PLAYBACK_DAI_ID, strlen(pcm_id))) {
		return TX;
	} else {
		return RX;
	}
}

static bool hpcm_all_dais_are_ready(uint16_t direction, struct tap_point *tp,
				    enum hpcm_state state)
{
	bool dais_started = false;

	/*
	 * Based on the direction set per tap point in the mixer control,
	 * all the dais per tap point should meet the required state for the
	 * commands such as vpcm_map_memory/vpcm_start to be executed.
	 */
	switch (direction) {
	case VSS_IVPCM_TAP_POINT_DIR_OUT_IN:
		if ((tp->playback_dai_data.state >= state) &&
		    (tp->capture_dai_data.state >= state)) {
			dais_started = true;
		}
		break;

	case VSS_IVPCM_TAP_POINT_DIR_IN:
		if (tp->playback_dai_data.state >= state)
			dais_started = true;
		break;

	case VSS_IVPCM_TAP_POINT_DIR_OUT:
		if (tp->capture_dai_data.state >= state)
			dais_started = true;
		break;

	default:
		pr_err("invalid direction\n");
	}

	return dais_started;
}

static void hpcm_create_free_queue(struct snd_dma_buffer *dma_buf,
				   struct dai_data *dai_data)
{
	struct hpcm_buf_node *buf_node = NULL;
	int i = 0, offset = 0;

	for (i = 0; i < HPCM_MAX_Q_LEN; i++) {
		buf_node = (void *)dma_buf->area + offset;
		list_add_tail(&buf_node->list,
				&dai_data->free_queue);
		offset = offset + sizeof(struct hpcm_buf_node);
	}
}

static void hpcm_unmap_and_free_shared_memory(struct hpcm_drv *prtd,
					      struct tap_point *tp,
					      uint16_t direction)
{
	unsigned long paddr = 0;
	char *sess = hpcm_get_sess_name(prtd->mixer_conf.sess_indx);

	switch (direction) {
	case VSS_IVPCM_TAP_POINT_DIR_OUT_IN:
	case VSS_IVPCM_TAP_POINT_DIR_IN:
		paddr = tp->playback_dai_data.vocpcm_ion_buffer.paddr;
		break;

	case VSS_IVPCM_TAP_POINT_DIR_OUT:
		paddr = tp->capture_dai_data.vocpcm_ion_buffer.paddr;
		break;

	default:
		pr_err("Invalid direction\n");
	}

	if (paddr) {
		voc_send_cvp_unmap_vocpcm_memory(voc_get_session_id(sess),
						 paddr);
		ion_unmap_kernel(prtd->ion_client, tp->ion_handle);
		ion_free(prtd->ion_client, tp->ion_handle);
		tp->ion_mem_len = 0;
		tp->playback_dai_data.vocpcm_ion_buffer.paddr = 0;
		tp->capture_dai_data.vocpcm_ion_buffer.paddr = 0;
		tp->playback_dai_data.vocpcm_ion_buffer.kvaddr = 0;
		tp->capture_dai_data.vocpcm_ion_buffer.kvaddr = 0;
	}

}

static int hpcm_map_vocpcm_memory(struct hpcm_drv *prtd,
				  struct tap_point *tp,
				  struct tappnt_mxr_data *tmd)
{
	unsigned long paddr = 0;
	bool send_cmd = false;
	int ret = 0;
	char *sess = hpcm_get_sess_name(prtd->mixer_conf.sess_indx);

	/*
	 * only one memory map command is sent per tap point, ensure all dais
	 * for a tap point are in HPCM_PREPARED state.
	 */
	send_cmd = hpcm_all_dais_are_ready(tmd->direction, tp, HPCM_PREPARED);

	if (send_cmd == true) {
		switch (tmd->direction) {
		case VSS_IVPCM_TAP_POINT_DIR_OUT_IN:
		case VSS_IVPCM_TAP_POINT_DIR_IN:
			paddr = tp->playback_dai_data.vocpcm_ion_buffer.paddr;
			break;

		case VSS_IVPCM_TAP_POINT_DIR_OUT:
			paddr = tp->capture_dai_data.vocpcm_ion_buffer.paddr;
			break;
		}

		ret = voc_send_cvp_map_vocpcm_memory(voc_get_session_id(sess),
						     paddr, tp->ion_mem_len);
	}

	return ret;

}

static int hpcm_allocate_shared_memory(struct hpcm_drv *prtd,
				       struct tap_point *tp,
				       struct tappnt_mxr_data *tmd)
{
	int result;
	int mem_len;
	dma_addr_t paddr;
	void *kvptr;
	int ion_mem_reqd = 0;
	bool create_mem = false;


	create_mem = hpcm_all_dais_are_ready(tmd->direction, tp,
					     HPCM_PREPARED);

	if (create_mem) {
		if (tmd->direction == VSS_IVPCM_TAP_POINT_DIR_OUT_IN)
			ion_mem_reqd = HPCM_MAX_VOC_PKT_SIZE * 2;
		else
			ion_mem_reqd = HPCM_MAX_VOC_PKT_SIZE;

		tp->ion_handle = ion_alloc(prtd->ion_client,
					ion_mem_reqd,
					SZ_4K, ION_HEAP(ION_AUDIO_HEAP_ID), 0);
		if (IS_ERR_OR_NULL((void *) tp->ion_handle)) {
			pr_err("%s: ION memory allocation failed\n",
				__func__);
			goto error;
		}

		result = ion_phys(prtd->ion_client, tp->ion_handle,
				&paddr, (size_t *)&mem_len);
		if (result) {
			pr_err("%s: ION Get Physical failed, rc = %d\n",
				__func__, result);
			goto error;
		}

		kvptr = ion_map_kernel(prtd->ion_client, tp->ion_handle);
		if (IS_ERR_OR_NULL(kvptr)) {
			pr_err("%s: ION memory mapping failed\n", __func__);
			goto error;
		}

		switch (tmd->direction)	{
		case VSS_IVPCM_TAP_POINT_DIR_OUT_IN:
			tp->playback_dai_data.vocpcm_ion_buffer.paddr =
							       (uint32_t)paddr;
			tp->playback_dai_data.vocpcm_ion_buffer.kvaddr =
						    (uint32_t)(uint8_t *)kvptr;
			tp->capture_dai_data.vocpcm_ion_buffer.paddr =
				(uint32_t)paddr + 1 * HPCM_MAX_VOC_PKT_SIZE;
			tp->capture_dai_data.vocpcm_ion_buffer.kvaddr =
			(uint32_t)(uint8_t *)kvptr + 1 * HPCM_MAX_VOC_PKT_SIZE;
			break;

		case VSS_IVPCM_TAP_POINT_DIR_IN:
			tp->playback_dai_data.vocpcm_ion_buffer.paddr =
							       (uint32_t)paddr;
			tp->playback_dai_data.vocpcm_ion_buffer.kvaddr =
						    (uint32_t)(uint8_t *)kvptr;
			break;

		case VSS_IVPCM_TAP_POINT_DIR_OUT:
			tp->capture_dai_data.vocpcm_ion_buffer.paddr =
							       (uint32_t)paddr;
			tp->capture_dai_data.vocpcm_ion_buffer.kvaddr =
						    (uint32_t)(uint8_t *)kvptr;
			break;
		}

		tp->ion_mem_len = ion_mem_reqd;
	}

	return 0;

error:
	if (tp->ion_handle)
		ion_free(prtd->ion_client, tp->ion_handle);

	return -ENOMEM;
}

static int hpcm_start_vocpcm(char *pcm_id, struct hpcm_drv *prtd,
			     struct tap_point *tp)
{
	int indx = prtd->mixer_conf.sess_indx;
	uint32_t *no_of_tp = &prtd->start_cmd.no_of_tapoints;
	struct vss_ivpcm_tap_point *tap_pnt = &prtd->start_cmd.tap_pnt[0];
	uint32_t no_of_tp_req = 0;
	char *sess = hpcm_get_sess_name(indx);

	if (prtd->mixer_conf.rx.enable)
		no_of_tp_req++;
	if (prtd->mixer_conf.tx.enable)
		no_of_tp_req++;

	if (prtd->mixer_conf.rx.enable && (get_tappnt_value(pcm_id) == RX)) {
		if (hpcm_all_dais_are_ready(prtd->mixer_conf.rx.direction,
					    tp, HPCM_PREPARED)) {
			pr_debug("%s: RX conditions met\n", __func__);
			tap_pnt[*no_of_tp].tap_point =
					VSS_IVPCM_TAP_POINT_RX_DEFAULT;
			tap_pnt[*no_of_tp].direction =
					prtd->mixer_conf.rx.direction;
			tap_pnt[*no_of_tp].sampling_rate =
					prtd->mixer_conf.rx.sample_rate;
			(*no_of_tp)++;
		}
	}

	if (prtd->mixer_conf.tx.enable && (get_tappnt_value(pcm_id) == TX)) {
		if (hpcm_all_dais_are_ready(prtd->mixer_conf.tx.direction,
					    tp, HPCM_PREPARED)) {
			pr_debug("%s: TX conditions met\n", __func__);
			tap_pnt[*no_of_tp].tap_point =
						VSS_IVPCM_TAP_POINT_TX_DEFAULT;
			tap_pnt[*no_of_tp].direction =
						prtd->mixer_conf.tx.direction;
			tap_pnt[*no_of_tp].sampling_rate =
						prtd->mixer_conf.tx.sample_rate;
			(*no_of_tp)++;
		}
	}

	pr_debug("%s: *no_of_tp = %d no_of_tp_req = %d\n",
		 __func__, *no_of_tp, no_of_tp_req);

	if ((prtd->mixer_conf.tx.enable && prtd->mixer_conf.rx.enable) &&
	    *no_of_tp == no_of_tp_req) {
		voc_send_cvp_start_vocpcm(voc_get_session_id(sess),
					  tap_pnt, *no_of_tp);
		memset(&prtd->start_cmd, 0, sizeof(struct start_cmd));
	} else if ((prtd->mixer_conf.tx.enable ||
		    prtd->mixer_conf.rx.enable) && *no_of_tp == no_of_tp_req) {
		voc_send_cvp_start_vocpcm(voc_get_session_id(sess),
					  tap_pnt, *no_of_tp);
		memset(&prtd->start_cmd, 0, sizeof(struct start_cmd));
	} else {
		pr_debug("%s: required pcm handles not opened yet\n", __func__);
	}

	return 0;
}

/* Playback path*/
static void hpcm_copy_playback_data_from_queue(struct dai_data *dai_data,
					       uint32_t *len)
{
	struct hpcm_buf_node *buf_node = NULL;
	unsigned long dsp_flags;

	if (dai_data->substream == NULL)
		return;

	spin_lock_irqsave(&dai_data->dsp_lock, dsp_flags);

	if (!list_empty(&dai_data->filled_queue)) {
		buf_node = list_first_entry(&dai_data->filled_queue,
				struct hpcm_buf_node, list);
		list_del(&buf_node->list);
		*len = buf_node->frame.len;
		memcpy((uint8_t *)dai_data->vocpcm_ion_buffer.kvaddr,
		       &buf_node->frame.voc_pkt[0],
		       buf_node->frame.len);

		list_add_tail(&buf_node->list, &dai_data->free_queue);
		dai_data->pcm_irq_pos += dai_data->pcm_count;
		spin_unlock_irqrestore(&dai_data->dsp_lock, dsp_flags);
		snd_pcm_period_elapsed(dai_data->substream);
	} else {
		*len = 0;
		spin_unlock_irqrestore(&dai_data->dsp_lock, dsp_flags);
		pr_err("IN data not available\n");
	}

	wake_up(&dai_data->queue_wait);
}

/* Capture path*/
static void hpcm_copy_capture_data_to_queue(struct dai_data *dai_data,
					    uint32_t len)
{
	struct hpcm_buf_node *buf_node = NULL;
	unsigned long dsp_flags;

	if (dai_data->substream == NULL)
		return;

	/* Copy out buffer packet into free_queue */
	spin_lock_irqsave(&dai_data->dsp_lock, dsp_flags);

	if (!list_empty(&dai_data->free_queue)) {
		buf_node = list_first_entry(&dai_data->free_queue,
					struct hpcm_buf_node, list);
		list_del(&buf_node->list);
		buf_node->frame.len = len;
		memcpy(&buf_node->frame.voc_pkt[0],
		       (uint8_t *)dai_data->vocpcm_ion_buffer.kvaddr,
		       buf_node->frame.len);
		list_add_tail(&buf_node->list, &dai_data->filled_queue);
		dai_data->pcm_irq_pos += dai_data->pcm_count;
		spin_unlock_irqrestore(&dai_data->dsp_lock, dsp_flags);
		snd_pcm_period_elapsed(dai_data->substream);
	} else {
		spin_unlock_irqrestore(&dai_data->dsp_lock, dsp_flags);
		pr_err("OUTPUT data dropped\n");
	}

	wake_up(&dai_data->queue_wait);
}

void hpcm_notify_evt_processing(uint8_t *data, char *session,
				void *private_data)
{
	struct hpcm_drv *prtd = (struct hpcm_drv *)private_data;
	struct vss_ivpcm_evt_notify *notify_evt =
				(struct vss_ivpcm_evt_notify *)data;
	struct vss_ivpcm_evt_push_buffer push_buff_event;
	struct tap_point *tp = NULL;
	int in_buf_len = 0;
	struct tappnt_mxr_data *tmd = NULL;
	char *sess = hpcm_get_sess_name(prtd->mixer_conf.sess_indx);

	/* If it's not a timetick, it's a error notification, drop the event */
	if ((notify_evt->notify_mask & VSS_IVPCM_NOTIFY_MASK_TIMETICK) == 0) {
		pr_err("%s: Error notification. mask=%d\n", __func__,
			notify_evt->notify_mask);
		return;
	}

	 if (notify_evt->tap_point == VSS_IVPCM_TAP_POINT_TX_DEFAULT) {
		tp = &prtd->session[prtd->mixer_conf.sess_indx].tx_tap_point;
		tmd = &prtd->mixer_conf.tx;
	 } else if (notify_evt->tap_point == VSS_IVPCM_TAP_POINT_RX_DEFAULT) {
		tp = &prtd->session[prtd->mixer_conf.sess_indx].rx_tap_point;
		tmd = &prtd->mixer_conf.rx;
	 }

	 if (notify_evt->notify_mask & VSS_IVPCM_NOTIFY_MASK_OUTPUT_BUFFER) {
		hpcm_copy_capture_data_to_queue(&tp->capture_dai_data,
						notify_evt->filled_out_size);
	 }

	 if (notify_evt->notify_mask & VSS_IVPCM_NOTIFY_MASK_INPUT_BUFFER) {
		hpcm_copy_playback_data_from_queue(&tp->playback_dai_data,
						   &in_buf_len);
	 }

	switch (tmd->direction) {
	/*
	 * When the dir is OUT_IN, for the first notify mask, pushbuf mask
	 * should be set to VSS_IVPCM_PUSH_BUFFER_MASK_OUTPUT_BUFFER since we
	 * atleast need one buffer's worth data before we can send IN buffer.
	 * For the consecutive notify evts, the push buf mask will set for both
	 * VSS_IVPCM_PUSH_BUFFER_MASK_OUTPUT_BUFFER and
	 * VSS_IVPCM_PUSH_BUFFER_MASK_IN_BUFFER.
	 */
	case VSS_IVPCM_TAP_POINT_DIR_OUT_IN:
		if (notify_evt->notify_mask ==
		    VSS_IVPCM_NOTIFY_MASK_TIMETICK) {
			push_buff_event.push_buf_mask =
				VSS_IVPCM_PUSH_BUFFER_MASK_OUTPUT_BUFFER;
		} else {
			push_buff_event.push_buf_mask =
			   VSS_IVPCM_PUSH_BUFFER_MASK_OUTPUT_BUFFER |
			   VSS_IVPCM_PUSH_BUFFER_MASK_INPUT_BUFFER;
		}
		break;

	case VSS_IVPCM_TAP_POINT_DIR_IN:
		push_buff_event.push_buf_mask =
			VSS_IVPCM_PUSH_BUFFER_MASK_INPUT_BUFFER;
		break;

	case VSS_IVPCM_TAP_POINT_DIR_OUT:
		push_buff_event.push_buf_mask =
			 VSS_IVPCM_PUSH_BUFFER_MASK_OUTPUT_BUFFER;
		break;
	}

	push_buff_event.tap_point = notify_evt->tap_point;
	push_buff_event.out_buf_addr =
				tp->capture_dai_data.vocpcm_ion_buffer.paddr;
	push_buff_event.in_buf_addr =
				tp->playback_dai_data.vocpcm_ion_buffer.paddr;
	push_buff_event.out_buf_size = notify_evt->request_buff_size;
	push_buff_event.in_buf_size = in_buf_len;
	push_buff_event.sampling_rate = notify_evt->sampling_rate;
	push_buff_event.num_in_channels = 1;

	voc_send_cvp_vocpcm_push_buf_evt(voc_get_session_id(sess),
					 &push_buff_event);
}

static int msm_hpcm_configure_voice_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{

	int tap_point = ucontrol->value.integer.value[0];
	uint16_t direction = ucontrol->value.integer.value[1];
	uint16_t sample_rate = ucontrol->value.integer.value[2];
	struct tappnt_mxr_data *tmd = NULL;
	int ret = 0;

	mutex_lock(&hpcm_drv.lock);
	pr_debug("%s: tap_point = %d direction = %d sample_rate = %d\n",
		 __func__, tap_point, direction, sample_rate);

	if (!hpcm_is_valid_config(VOICE_INDEX, tap_point, direction,
				  sample_rate)) {
		pr_err("Invalid vpcm mixer control voice values\n");
		ret = -EINVAL;
		goto done;
	}

	if (tap_point == RX)
		tmd = &hpcm_drv.mixer_conf.rx;
	else
		tmd = &hpcm_drv.mixer_conf.tx;

	tmd->enable = true;
	tmd->direction = direction;
	tmd->sample_rate = sample_rate;
	hpcm_drv.mixer_conf.sess_indx = VOICE_INDEX;

done:
	mutex_unlock(&hpcm_drv.lock);
	return ret;
}

static int msm_hpcm_configure_voice_get(struct  snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s:\n", __func__);
	return -EINVAL;
}

static int msm_hpcm_configure_volte_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{

	int tap_point = ucontrol->value.integer.value[0];
	uint16_t direction = ucontrol->value.integer.value[1];
	uint16_t sample_rate = ucontrol->value.integer.value[2];
	struct tappnt_mxr_data *tmd = NULL;
	int ret = 0;

	mutex_lock(&hpcm_drv.lock);
	pr_debug("%s: tap_point=%d direction=%d sample_rate=%d\n",
		 __func__, tap_point, direction, sample_rate);

	if (!hpcm_is_valid_config(VOLTE_INDEX, tap_point, direction,
				  sample_rate)) {
		pr_err("Invalid vpcm mixer control volte values\n");
		ret = -EINVAL;
		goto done;
	}

	if (tap_point == RX)
		tmd = &hpcm_drv.mixer_conf.rx;
	else
		tmd = &hpcm_drv.mixer_conf.tx;

	tmd->enable = true;
	tmd->direction = direction;
	tmd->sample_rate = sample_rate;
	hpcm_drv.mixer_conf.sess_indx = VOLTE_INDEX;

done:
	mutex_unlock(&hpcm_drv.lock);
	return ret;

}

static int msm_hpcm_configure_volte_get(struct  snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s:\n", __func__);
	return -EINVAL;
}

static struct snd_kcontrol_new msm_hpcm_controls[] = {
	SOC_SINGLE_MULTI_EXT("HPCM_Voice tappoint direction samplerate",
			     SND_SOC_NOPM, 0, 16000 , 0, 3,
			     msm_hpcm_configure_voice_get,
			     msm_hpcm_configure_voice_put),
	SOC_SINGLE_MULTI_EXT("HPCM_VoLTE tappoint direction samplerate",
			     SND_SOC_NOPM, 0, 16000 , 0, 3,
			     msm_hpcm_configure_volte_get,
			     msm_hpcm_configure_volte_put),
};

/* Sample rates supported */
static unsigned int supported_sample_rates[] = {8000, 16000};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct list_head *ptr = NULL;
	struct list_head *next = NULL;
	struct hpcm_buf_node *buf_node = NULL;
	struct snd_dma_buffer *dma_buf;
	struct snd_pcm_runtime *runtime;
	struct hpcm_drv *prtd;
	unsigned long dsp_flags;
	struct dai_data *dai_data = NULL;
	struct tap_point *tp = NULL;
	struct tappnt_mxr_data *tmd = NULL;
	char *sess = NULL;

	if (substream == NULL) {
		pr_err("substream is NULL\n");
		return -EINVAL;
	}

	pr_debug("%s, %s\n", __func__, substream->pcm->id);
	runtime = substream->runtime;
	prtd = runtime->private_data;
	sess = hpcm_get_sess_name(prtd->mixer_conf.sess_indx);
	dai_data = hpcm_get_dai_data(substream->pcm->id, prtd);

	wake_up(&dai_data->queue_wait);
	mutex_lock(&prtd->lock);

	tmd = hpcm_get_tappnt_mixer_data(substream->pcm->id, prtd);

	tp = hpcm_get_tappoint_data(substream->pcm->id, prtd);
	/* Send stop command */
	voc_send_cvp_stop_vocpcm(voc_get_session_id(sess));
	/* Unmap will be called twice once for RX and TX each */
	hpcm_unmap_and_free_shared_memory(prtd, tp, tmd->direction);
	/* Reset the cached start cmd */
	memset(&prtd->start_cmd, 0, sizeof(struct start_cmd));
	/* Release all buffer */
	pr_debug("%s: Release all buffer\n", __func__);
	substream = dai_data->substream;
	if (substream == NULL) {
		pr_debug("%s: substream is NULL\n", __func__);
		goto done;
	}
	dma_buf = &substream->dma_buffer;
	if (dma_buf == NULL) {
		pr_debug("%s: dma_buf is NULL\n", __func__);
		goto done;
	}
	if (dma_buf->area != NULL) {
		spin_lock_irqsave(&dai_data->dsp_lock, dsp_flags);
		list_for_each_safe(ptr, next, &dai_data->filled_queue) {
			buf_node = list_entry(ptr,
					struct hpcm_buf_node, list);
			list_del(&buf_node->list);
		}
		list_for_each_safe(ptr, next, &dai_data->free_queue) {
			buf_node = list_entry(ptr,
					struct hpcm_buf_node, list);
			list_del(&buf_node->list);
		}
		spin_unlock_irqrestore(&dai_data->dsp_lock, dsp_flags);
		dma_free_coherent(substream->pcm->card->dev,
			runtime->hw.buffer_bytes_max, dma_buf->area,
			dma_buf->addr);
		dma_buf->area = NULL;
	}
	dai_data->substream = NULL;
	dai_data->pcm_buf_pos = 0;
	dai_data->pcm_count = 0;
	dai_data->pcm_irq_pos = 0;
	dai_data->pcm_size = 0;
	dai_data->state = HPCM_CLOSED;
	hpcm_reset_mixer_config(prtd);

done:
	mutex_unlock(&prtd->lock);
	return ret;
}

static int msm_pcm_playback_copy(struct snd_pcm_substream *substream, int a,
				 snd_pcm_uframes_t hwoff, void __user *buf,
				 snd_pcm_uframes_t frames)
{
	int ret = 0;
	struct hpcm_buf_node *buf_node = NULL;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hpcm_drv *prtd = runtime->private_data;
	struct dai_data *dai_data = hpcm_get_dai_data(substream->pcm->id, prtd);
	unsigned long dsp_flags;

	int count = frames_to_bytes(runtime, frames);

	ret = wait_event_interruptible_timeout(dai_data->queue_wait,
				(!list_empty(&dai_data->free_queue) ||
				dai_data->state == HPCM_STOPPED),
				1 * HZ);
	if (ret > 0) {
		if (count <= HPCM_MAX_VOC_PKT_SIZE) {
			spin_lock_irqsave(&dai_data->dsp_lock, dsp_flags);
			buf_node =
				list_first_entry(&dai_data->free_queue,
						struct hpcm_buf_node, list);
			list_del(&buf_node->list);
			spin_unlock_irqrestore(&dai_data->dsp_lock, dsp_flags);
			ret = copy_from_user(&buf_node->frame.voc_pkt,
					     buf, count);
			buf_node->frame.len = count;
			spin_lock_irqsave(&dai_data->dsp_lock, dsp_flags);
			list_add_tail(&buf_node->list, &dai_data->filled_queue);
			spin_unlock_irqrestore(&dai_data->dsp_lock, dsp_flags);
		} else {
			pr_err("%s: Write cnt %d is > HPCM_MAX_VOC_PKT_SIZE\n",
				__func__, count);
			ret = -ENOMEM;
		}
	} else if (ret == 0) {
		pr_err("%s: No free Playback buffer\n", __func__);
		ret = -ETIMEDOUT;
	} else {
		pr_err("%s: playback copy  was interrupted\n", __func__);
	}

	return  ret;
}
static int msm_pcm_capture_copy(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t hwoff,
				void __user *buf, snd_pcm_uframes_t frames)
{
	int ret = 0;
	int count = 0;
	struct hpcm_buf_node *buf_node = NULL;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hpcm_drv *prtd = runtime->private_data;
	struct dai_data *dai_data = hpcm_get_dai_data(substream->pcm->id, prtd);
	unsigned long dsp_flags;

	count = frames_to_bytes(runtime, frames);

	ret = wait_event_interruptible_timeout(dai_data->queue_wait,
				(!list_empty(&dai_data->filled_queue) ||
				dai_data->state == HPCM_STOPPED),
				1 * HZ);

	if (ret > 0) {
		if (count <= HPCM_MAX_VOC_PKT_SIZE) {
			spin_lock_irqsave(&dai_data->dsp_lock, dsp_flags);
			buf_node = list_first_entry(&dai_data->filled_queue,
					struct hpcm_buf_node, list);
			list_del(&buf_node->list);
			spin_unlock_irqrestore(&dai_data->dsp_lock, dsp_flags);
			ret = copy_to_user(buf,
					   &buf_node->frame.voc_pkt,
					   buf_node->frame.len);
			if (ret) {
				pr_err("%s: Copy to user retuned %d\n",
					__func__, ret);
				ret = -EFAULT;
			}
			spin_lock_irqsave(&dai_data->dsp_lock, dsp_flags);
			list_add_tail(&buf_node->list, &dai_data->free_queue);
			spin_unlock_irqrestore(&dai_data->dsp_lock, dsp_flags);

		} else {
			pr_err("%s: Read count %d > HPCM_MAX_VOC_PKT_SIZE\n",
				__func__, count);
			ret = -ENOMEM;
		}

	} else if (ret == 0) {
		pr_err("%s: No Caputre data available\n", __func__);
		ret = -ETIMEDOUT;
	} else {
		pr_err("%s: Read was interrupted\n", __func__);
		ret = -ERESTARTSYS;
	}

	return ret;
}

static int msm_pcm_copy(struct snd_pcm_substream *substream, int channel,
			snd_pcm_uframes_t hwoff, void __user *buf,
			snd_pcm_uframes_t frames)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_copy(substream, channel,
					    hwoff, buf, frames);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_copy(substream, channel,
					   hwoff, buf, frames);

	return ret;
}

static snd_pcm_uframes_t msm_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct dai_data *dai_data = NULL;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hpcm_drv *prtd = runtime->private_data;

	dai_data = hpcm_get_dai_data(substream->pcm->id, prtd);

	if (dai_data->pcm_irq_pos >= dai_data->pcm_size)
		dai_data->pcm_irq_pos = 0;

	return bytes_to_frames(runtime, (dai_data->pcm_irq_pos));
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hpcm_drv *prtd = runtime->private_data;
	struct dai_data *dai_data =
			hpcm_get_dai_data(substream->pcm->id, prtd);

	pr_debug("%s, %s\n", __func__, substream->pcm->id);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		pr_debug("SNDRV_PCM_TRIGGER_START\n");
		dai_data->state = HPCM_STARTED;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("SNDRV_PCM_TRIGGER_STOP\n");
		dai_data->state = HPCM_STOPPED;
		break;

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hpcm_drv *prtd = runtime->private_data;
	struct dai_data *dai_data = NULL;
	struct tap_point *tp = NULL;
	struct tappnt_mxr_data *tmd = NULL;

	pr_debug("%s, %s\n", __func__, substream->pcm->id);
	mutex_lock(&prtd->lock);

	dai_data = hpcm_get_dai_data(substream->pcm->id, prtd);
	dai_data->pcm_size  = snd_pcm_lib_buffer_bytes(substream);
	dai_data->pcm_count = snd_pcm_lib_period_bytes(substream);
	dai_data->pcm_irq_pos = 0;
	dai_data->pcm_buf_pos = 0;
	dai_data->state = HPCM_PREPARED;

	tp = hpcm_get_tappoint_data(substream->pcm->id, prtd);
	tmd = hpcm_get_tappnt_mixer_data(substream->pcm->id, prtd);

	ret = hpcm_allocate_shared_memory(prtd, tp, tmd);
	if (ret) {
		pr_err("error creating shared memory err=%d\n", ret);
		goto done;
	}
	ret = hpcm_map_vocpcm_memory(prtd, tp, tmd);
	if (ret) {
		pr_err("error mapping shared memory err=%d\n", ret);
		goto done;
	}

	ret = hpcm_start_vocpcm(substream->pcm->id, prtd,
				hpcm_get_tappoint_data(substream->pcm->id,
						       prtd));
	if (ret) {
		pr_err("error sending start cmd err=%d\n", ret);
		goto done;
	}

done:
	mutex_unlock(&prtd->lock);
	return ret;
}

static int msm_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct hpcm_drv *prtd = (struct hpcm_drv *)runtime->private_data;
	int ret = 0;

	pr_debug("%s: %s\n", __func__, substream->pcm->id);

	mutex_lock(&prtd->lock);

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	dma_buf->area = dma_alloc_coherent(substream->pcm->card->dev,
			runtime->hw.buffer_bytes_max,
			&dma_buf->addr, GFP_KERNEL);

	if (!dma_buf->area) {
		pr_err("%s:MSM dma_alloc failed\n", __func__);
		ret = -ENOMEM;
		goto done;
	}

	dma_buf->bytes = runtime->hw.buffer_bytes_max;
	memset(dma_buf->area, 0, runtime->hw.buffer_bytes_max);

	hpcm_create_free_queue(dma_buf,
		hpcm_get_dai_data(substream->pcm->id, prtd));

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

done:
	mutex_unlock(&prtd->lock);
	return ret;
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hpcm_drv *prtd = &hpcm_drv;
	struct tappnt_mxr_data *tmd = NULL;
	struct dai_data *dai_data = NULL;
	int ret = 0;
	int tp_val = 0;

	pr_debug("%s, %s\n", __func__, substream->pcm->id);
	mutex_lock(&prtd->lock);

	runtime->hw = msm_pcm_hardware;

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_list failed\n");

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		pr_debug("snd_pcm_hw_constraint_integer failed\n");
		goto done;
	}

	tp_val = get_tappnt_value(substream->pcm->id);
	tmd = hpcm_get_tappnt_mixer_data(substream->pcm->id, prtd);

	/*Check whehter the kcontrol values set are valid*/
	if (!tmd ||
	    !(tmd->enable) ||
	    !hpcm_is_valid_config(prtd->mixer_conf.sess_indx,
				  tp_val, tmd->direction,
				  tmd->sample_rate)) {
		ret = -EINVAL;
		goto done;
	}

	dai_data = hpcm_get_dai_data(substream->pcm->id, prtd);
	dai_data->substream = substream;
	runtime->private_data = prtd;

done:
	mutex_unlock(&prtd->lock);
	return ret;
}

static struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.hw_params      = msm_pcm_hw_params,
	.prepare        = msm_pcm_prepare,
	.trigger        = msm_pcm_trigger,
	.pointer        = msm_pcm_pointer,
	.copy           = msm_pcm_copy,
	.close          = msm_pcm_close,
};

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;

	pr_debug("%s:\n", __func__);
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	return 0;
}

static int msm_pcm_hpcm_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, msm_hpcm_controls,
				ARRAY_SIZE(msm_hpcm_controls));

	return 0;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_pcm_ops,
	.pcm_new	= msm_asoc_pcm_new,
	.probe		= msm_pcm_hpcm_probe,
};

static int msm_pcm_probe(struct platform_device *pdev)
{
	pr_info("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &msm_soc_platform);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-host-pcm-voice",
		.owner = THIS_MODULE,
	},
	.probe = msm_pcm_probe,
	.remove = msm_pcm_remove,
};

static int __init msm_soc_platform_init(void)
{
	int i = 0;
	struct session *s = NULL;

	memset(&hpcm_drv, 0, sizeof(hpcm_drv));
	mutex_init(&hpcm_drv.lock);

	hpcm_drv.ion_client = msm_ion_client_create(UINT_MAX, "host_voice_pcm");
	if (IS_ERR_OR_NULL((void *)hpcm_drv.ion_client)) {
		pr_err("%s: ION create client failed\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < MAX_SESSION; i++) {
		s = &hpcm_drv.session[i];
		spin_lock_init(&s->rx_tap_point.capture_dai_data.dsp_lock);
		spin_lock_init(&s->rx_tap_point.playback_dai_data.dsp_lock);
		spin_lock_init(&s->tx_tap_point.capture_dai_data.dsp_lock);
		spin_lock_init(&s->tx_tap_point.playback_dai_data.dsp_lock);

		init_waitqueue_head(
			&s->rx_tap_point.capture_dai_data.queue_wait);
		init_waitqueue_head(
			&s->rx_tap_point.playback_dai_data.queue_wait);
		init_waitqueue_head(
			&s->tx_tap_point.capture_dai_data.queue_wait);
		init_waitqueue_head(
			&s->tx_tap_point.playback_dai_data.queue_wait);

		INIT_LIST_HEAD(&s->rx_tap_point.capture_dai_data.filled_queue);
		INIT_LIST_HEAD(&s->rx_tap_point.capture_dai_data.free_queue);
		INIT_LIST_HEAD(&s->rx_tap_point.playback_dai_data.filled_queue);
		INIT_LIST_HEAD(&s->rx_tap_point.playback_dai_data.free_queue);

		INIT_LIST_HEAD(&s->tx_tap_point.capture_dai_data.filled_queue);
		INIT_LIST_HEAD(&s->tx_tap_point.capture_dai_data.free_queue);
		INIT_LIST_HEAD(&s->tx_tap_point.playback_dai_data.filled_queue);
		INIT_LIST_HEAD(&s->tx_tap_point.playback_dai_data.free_queue);
	}

	voc_register_hpcm_evt_cb(hpcm_notify_evt_processing, &hpcm_drv);

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

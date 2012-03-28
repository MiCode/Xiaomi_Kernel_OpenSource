/* low power audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/list.h>
#include <linux/android_pmem.h>
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/msm_adsp.h>
#include <linux/slab.h>
#include <linux/msm_audio.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>

#include <mach/qdsp5v2/qdsp5audppmsg.h>
#include <mach/qdsp5v2/qdsp5audplaycmdi.h>
#include <mach/qdsp5v2/qdsp5audplaymsg.h>
#include <mach/qdsp5v2/audpp.h>
#include <mach/qdsp5v2/codec_utils.h>
#include <mach/qdsp5v2/mp3_funcs.h>
#include <mach/qdsp5v2/pcm_funcs.h>
#include <mach/debug_mm.h>

#define ADRV_STATUS_AIO_INTF 0x00000001
#define ADRV_STATUS_OBUF_GIVEN 0x00000002
#define ADRV_STATUS_IBUF_GIVEN 0x00000004
#define ADRV_STATUS_FSYNC 0x00000008
#define ADRV_STATUS_PAUSE 0x00000010

#define DEVICE_SWITCH_STATE_NONE     0
#define DEVICE_SWITCH_STATE_PENDING  1
#define DEVICE_SWITCH_STATE_READY    2
#define DEVICE_SWITCH_STATE_COMPLETE 3

#define AUDDEC_DEC_PCM 0
#define AUDDEC_DEC_MP3 2

#define PCM_BUFSZ_MIN 4800	/* Hold one stereo MP3 frame */

/* Decoder status received from AUDPPTASK */
#define  AUDPP_DEC_STATUS_SLEEP	0
#define	 AUDPP_DEC_STATUS_INIT  1
#define  AUDPP_DEC_STATUS_CFG   2
#define  AUDPP_DEC_STATUS_PLAY  3

#define AUDMP3_METAFIELD_MASK 0xFFFF0000
#define AUDMP3_EOS_FLG_OFFSET 0x0A /* Offset from beginning of buffer */
#define AUDMP3_EOS_FLG_MASK 0x01
#define AUDMP3_EOS_NONE 0x0 /* No EOS detected */
#define AUDMP3_EOS_SET 0x1 /* EOS set in meta field */

#define AUDLPA_EVENT_NUM 10 /* Default number of pre-allocated event packets */

#define MASK_32BITS     0xFFFFFFFF

#define MAX_BUF 4
#define BUFSZ (524288)

#define __CONTAINS(r, v, l) ({					\
	typeof(r) __r = r;					\
	typeof(v) __v = v;					\
	typeof(v) __e = __v + l;				\
	int res = ((__v >= __r->vaddr) && 			\
		(__e <= __r->vaddr + __r->len));		\
	res;							\
})

#define CONTAINS(r1, r2) ({					\
	typeof(r2) __r2 = r2;					\
	__CONTAINS(r1, __r2->vaddr, __r2->len);			\
})

#define IN_RANGE(r, v) ({					\
	typeof(r) __r = r;					\
	typeof(v) __vv = v;					\
	int res = ((__vv >= __r->vaddr) &&			\
		(__vv < (__r->vaddr + __r->len)));		\
	res;							\
})

#define OVERLAPS(r1, r2) ({					\
	typeof(r1) __r1 = r1;					\
	typeof(r2) __r2 = r2;					\
	typeof(__r2->vaddr) __v = __r2->vaddr;			\
	typeof(__v) __e = __v + __r2->len - 1;			\
	int res = (IN_RANGE(__r1, __v) || IN_RANGE(__r1, __e));	\
	res;							\
})

/* payload[7]; -1 indicates error, 0 indicates no error */
#define CHECK_ERROR(v) (!v[7])

/* calculates avsync_info from payload */
#define CALCULATE_AVSYNC_FROM_PAYLOAD(v) ((uint64_t)((((uint64_t)v[10]) \
					<< 32) | (v[11] & MASK_32BITS)))

/* calculates avsync_info from avsync_info stored in audio */
#define CALCULATE_AVSYNC(v)					   \
			((uint64_t)((((uint64_t)v[4]) << 32) | 	   \
			 (v[5] << 16) | (v[6])))

#ifdef CONFIG_HAS_EARLYSUSPEND
struct audlpa_suspend_ctl {
	struct early_suspend node;
	struct audio *audio;
};
#endif

struct audlpa_event {
	struct list_head list;
	int event_type;
	union msm_audio_event_payload payload;
};

struct audlpa_pmem_region {
	struct list_head list;
	struct file *file;
	int fd;
	void *vaddr;
	unsigned long paddr;
	unsigned long kvaddr;
	unsigned long len;
	unsigned ref_cnt;
};

struct audlpa_buffer_node {
	struct list_head list;
	struct msm_audio_aio_buf buf;
	unsigned long paddr;
};

struct audlpa_dec {
	char *name;
	int dec_attrb;
	long (*ioctl)(struct file *, unsigned int, unsigned long);
	void (*adec_params)(struct audio *);
};

struct audlpa_dec audlpa_decs[] = {
	{"msm_mp3_lp", AUDDEC_DEC_MP3, &mp3_ioctl, &audpp_cmd_cfg_mp3_params},
	{"msm_pcm_lp_dec", AUDDEC_DEC_PCM, &pcm_ioctl,
		&audpp_cmd_cfg_pcm_params},
};

static int auddec_dsp_config(struct audio *audio, int enable);
static void audio_dsp_event(void *private, unsigned id, uint16_t *msg);
static void audlpa_post_event(struct audio *audio, int type,
	union msm_audio_event_payload payload);
static unsigned long audlpa_pmem_fixup(struct audio *audio, void *addr,
				unsigned long len, int ref_up);
static void audlpa_async_send_data(struct audio *audio, unsigned needed,
				uint32_t *payload);

static void lpa_listner(u32 evt_id, union auddev_evt_data *evt_payload,
			void *private_data)
{
	struct audio *audio = (struct audio *) private_data;
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		MM_DBG(":AUDDEV_EVT_DEV_RDY routing id = %d\n",
		evt_payload->routing_id);
		/* Do not select HLB path for icodec, if there is already COPP3
		 * routing exists. DSP can not support concurrency of HLB path
		 * and COPP3 routing as it involves different buffer Path */
		if (((0x1 << evt_payload->routing_id) == AUDPP_MIXER_ICODEC) &&
			!(audio->source & AUDPP_MIXER_3)) {
			audio->source |= AUDPP_MIXER_HLB;
			MM_DBG("mixer_mask modified for low-power audio\n");
		} else
			audio->source |= (0x1 << evt_payload->routing_id);

		MM_DBG("running = %d, enabled = %d, source = 0x%x\n",
			audio->running, audio->enabled, audio->source);
		if (audio->running == 1 && audio->enabled == 1) {
			audpp_route_stream(audio->dec_id, audio->source);
			if (audio->source & AUDPP_MIXER_HLB) {
				audpp_dsp_set_vol_pan(
					AUDPP_CMD_CFG_DEV_MIXER_ID_4,
					&audio->vol_pan,
					COPP);
					/*restore the POPP gain to 0x2000
					this is needed to avoid use cases
					where POPP volume is lowered during
					NON HLB playback, when device moved
					from NON HLB to HLB POPP is not
					disabled but POPP gain will be retained
					as the old one which result
					in lower volume*/
					audio->vol_pan.volume = 0x2000;
					audpp_dsp_set_vol_pan(
						audio->dec_id,
						&audio->vol_pan, POPP);
			} else if (audio->source & AUDPP_MIXER_NONHLB)
				audpp_dsp_set_vol_pan(
					audio->dec_id, &audio->vol_pan, POPP);
			if (audio->device_switch == DEVICE_SWITCH_STATE_READY) {
				audio->wflush = 1;
				audio->device_switch =
					DEVICE_SWITCH_STATE_COMPLETE;
				audpp_flush(audio->dec_id);
				if (wait_event_interruptible(audio->write_wait,
							 !audio->wflush) < 0)
					MM_DBG("AUDIO_FLUSH interrupted\n");

				if (audio->wflush == 0) {
					if (audio->drv_status &
						ADRV_STATUS_PAUSE) {
						if (audpp_pause(audio->dec_id,
							1))
							MM_DBG("audpp_pause"
								"failed\n");
					}
				}
			}
		}
		break;
	case AUDDEV_EVT_REL_PENDING:
		MM_DBG(":AUDDEV_EVT_REL_PENDING\n");
		/* If route to multiple devices like COPP3, not need to
		 * handle device switch */
		if ((audio->running == 1) && (audio->enabled == 1) &&
			!(audio->source & AUDPP_MIXER_3)) {
			if (audio->device_switch == DEVICE_SWITCH_STATE_NONE) {
				if (!(audio->drv_status & ADRV_STATUS_PAUSE)) {
					if (audpp_pause(audio->dec_id, 1))
						MM_DBG("audpp pause failed\n");
				}
				audio->device_switch =
					DEVICE_SWITCH_STATE_PENDING;
				audio->avsync_flag = 0;
				if (audpp_query_avsync(audio->dec_id) < 0)
					MM_DBG("query avsync failed\n");

				if (wait_event_interruptible_timeout
					(audio->avsync_wait, audio->avsync_flag,
				 msecs_to_jiffies(AVSYNC_EVENT_TIMEOUT)) < 0)
					MM_DBG("AV sync timeout failed\n");
				if (audio->avsync_flag == 1) {
					if (audio->device_switch ==
						DEVICE_SWITCH_STATE_PENDING)
						audio->device_switch =
						DEVICE_SWITCH_STATE_READY;
				}
			}
		}
		break;
	case AUDDEV_EVT_DEV_RLS:
		/* If there is already COPP3 routing exists. icodec route
		 * was not having HLB path. */
		MM_DBG(":AUDDEV_EVT_DEV_RLS routing id = %d\n",
			evt_payload->routing_id);
		if (((0x1 << evt_payload->routing_id) == AUDPP_MIXER_ICODEC) &&
			!(audio->source & AUDPP_MIXER_3))
			audio->source &= ~AUDPP_MIXER_HLB;
		else
			audio->source &= ~(0x1 << evt_payload->routing_id);
		MM_DBG("running = %d, enabled = %d, source = 0x%x\n",
			audio->running, audio->enabled, audio->source);

		if (audio->running == 1 && audio->enabled == 1)
			audpp_route_stream(audio->dec_id, audio->source);
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		audio->vol_pan.volume = evt_payload->session_vol;
		MM_DBG("\n:AUDDEV_EVT_STREAM_VOL_CHG, stream vol %d\n"
			"running = %d, enabled = %d, source = 0x%x",
			audio->vol_pan.volume, audio->running,
			audio->enabled, audio->source);
		if (audio->running == 1 && audio->enabled == 1) {
			if (audio->source & AUDPP_MIXER_HLB)
				audpp_dsp_set_vol_pan(
					AUDPP_CMD_CFG_DEV_MIXER_ID_4,
					&audio->vol_pan, COPP);
			else if (audio->source & AUDPP_MIXER_NONHLB)
				audpp_dsp_set_vol_pan(
					audio->dec_id, &audio->vol_pan, POPP);
		}
		break;
	default:
		MM_ERR(":ERROR:wrong event\n");
		break;
	}
}

/* must be called with audio->lock held */
static int audio_enable(struct audio *audio)
{
	MM_DBG("\n"); /* Macro prints the file name and function */

	if (audio->enabled)
		return 0;

	audio->dec_state = MSM_AUD_DECODER_STATE_NONE;
	audio->out_needed = 0;

	if (msm_adsp_enable(audio->audplay)) {
		MM_ERR("msm_adsp_enable(audplay) failed\n");
		return -ENODEV;
	}

	if (audpp_enable(audio->dec_id, audio_dsp_event, audio)) {
		MM_ERR("audpp_enable() failed\n");
		msm_adsp_disable(audio->audplay);
		return -ENODEV;
	}

	audio->enabled = 1;
	return 0;
}

/* must be called with audio->lock held */
static int audio_disable(struct audio *audio)
{
	int rc = 0;
	MM_DBG("\n"); /* Macro prints the file name and function */
	if (audio->enabled) {
		audio->enabled = 0;
		audio->dec_state = MSM_AUD_DECODER_STATE_NONE;
		auddec_dsp_config(audio, 0);
		rc = wait_event_interruptible_timeout(audio->wait,
				audio->dec_state != MSM_AUD_DECODER_STATE_NONE,
				msecs_to_jiffies(MSM_AUD_DECODER_WAIT_MS));
		if (rc == 0)
			rc = -ETIMEDOUT;
		else if (audio->dec_state != MSM_AUD_DECODER_STATE_CLOSE)
			rc = -EFAULT;
		else
			rc = 0;
		wake_up(&audio->write_wait);
		msm_adsp_disable(audio->audplay);
		audpp_disable(audio->dec_id, audio);
		audio->out_needed = 0;
	}
	return rc;
}

/* ------------------- dsp --------------------- */
static void audplay_dsp_event(void *data, unsigned id, size_t len,
			      void (*getevent) (void *ptr, size_t len))
{
	struct audio *audio = data;
	uint32_t msg[28];
	getevent(msg, sizeof(msg));

	MM_DBG("msg_id=%x\n", id);

	switch (id) {
	case AUDPLAY_MSG_DEC_NEEDS_DATA:
		audlpa_async_send_data(audio, 1, msg);
		break;
	case ADSP_MESSAGE_ID:
		MM_DBG("Received ADSP event: module enable(audplaytask)\n");
		break;
	default:
		MM_ERR("unexpected message from decoder\n");
		break;
	}
}

static void audio_dsp_event(void *private, unsigned id, uint16_t *msg)
{
	struct audio *audio = private;

	switch (id) {
	case AUDPP_MSG_STATUS_MSG:{
			unsigned status = msg[1];

			switch (status) {
			case AUDPP_DEC_STATUS_SLEEP: {
				uint16_t reason = msg[2];
				MM_DBG("decoder status: sleep reason=0x%04x\n",
						reason);
				if ((reason == AUDPP_MSG_REASON_MEM)
					|| (reason ==
					AUDPP_MSG_REASON_NODECODER)) {
					audio->dec_state =
						MSM_AUD_DECODER_STATE_FAILURE;
					wake_up(&audio->wait);
				} else if (reason == AUDPP_MSG_REASON_NONE) {
					/* decoder is in disable state */
					audio->dec_state =
						MSM_AUD_DECODER_STATE_CLOSE;
					wake_up(&audio->wait);
				}
				break;
			}
			case AUDPP_DEC_STATUS_INIT:
				MM_DBG("decoder status: init\n");
				audio->codec_ops.adec_params(audio);
				break;
			case AUDPP_DEC_STATUS_CFG:
				MM_DBG("decoder status: cfg\n");
				break;
			case AUDPP_DEC_STATUS_PLAY:
				MM_DBG("decoder status: play\n");
				/* send  mixer command */
				audpp_route_stream(audio->dec_id,
						audio->source);
				audio->dec_state =
					MSM_AUD_DECODER_STATE_SUCCESS;
				wake_up(&audio->wait);
				break;
			case AUDPP_DEC_STATUS_EOS:
				MM_DBG("decoder status: EOS\n");
				audio->teos = 1;
				wake_up(&audio->write_wait);
				break;
			default:
				MM_ERR("unknown decoder status\n");
				break;
			}
			break;
		}
	case AUDPP_MSG_CFG_MSG:
		if (msg[0] == AUDPP_MSG_ENA_ENA) {
			MM_DBG("CFG_MSG ENABLE\n");
			auddec_dsp_config(audio, 1);
			audio->out_needed = 0;
			audio->running = 1;
			MM_DBG("source = 0x%x\n", audio->source);
			if (audio->source & AUDPP_MIXER_HLB)
				audpp_dsp_set_vol_pan(
					AUDPP_CMD_CFG_DEV_MIXER_ID_4,
					&audio->vol_pan,
					COPP);
			else if (audio->source & AUDPP_MIXER_NONHLB)
				audpp_dsp_set_vol_pan(
					audio->dec_id, &audio->vol_pan,
					POPP);
			audpp_dsp_set_eq(audio->dec_id, audio->eq_enable,
					&audio->eq, POPP);
		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			MM_DBG("CFG_MSG DISABLE\n");
			audio->running = 0;
		} else {
			MM_DBG("CFG_MSG %d?\n", msg[0]);
		}
		break;
	case AUDPP_MSG_ROUTING_ACK:
		MM_DBG("ROUTING_ACK mode=%d\n",	msg[1]);
		audio->codec_ops.adec_params(audio);
		break;

	case AUDPP_MSG_FLUSH_ACK:
		MM_DBG("FLUSH_ACK\n");
		audio->wflush = 0;
		wake_up(&audio->write_wait);
		break;

	case AUDPP_MSG_PCMDMAMISSED:
		MM_DBG("PCMDMAMISSED\n");
		wake_up(&audio->write_wait);
		break;

	case AUDPP_MSG_AVSYNC_MSG:
		MM_DBG("AVSYNC_MSG\n");
		memcpy(&audio->avsync[0], msg, sizeof(audio->avsync));
		audio->avsync_flag = 1;
		wake_up(&audio->avsync_wait);
		break;

	default:
		MM_ERR("UNKNOWN (%d)\n", id);
	}

}

struct msm_adsp_ops audplay_adsp_ops_lpa = {
	.event = audplay_dsp_event,
};

#define audplay_send_queue0(audio, cmd, len) \
	msm_adsp_write(audio->audplay, audio->queue_id, \
			cmd, len)

static int auddec_dsp_config(struct audio *audio, int enable)
{
	struct audpp_cmd_cfg_dec_type cfg_dec_cmd;

	memset(&cfg_dec_cmd, 0, sizeof(cfg_dec_cmd));

	cfg_dec_cmd.cmd_id = AUDPP_CMD_CFG_DEC_TYPE;
	if (enable)
		cfg_dec_cmd.dec_cfg = AUDPP_CMD_UPDATDE_CFG_DEC |
			AUDPP_CMD_ENA_DEC_V |
			audlpa_decs[audio->minor_no].dec_attrb;
	else
		cfg_dec_cmd.dec_cfg = AUDPP_CMD_UPDATDE_CFG_DEC |
				AUDPP_CMD_DIS_DEC_V;
	cfg_dec_cmd.dm_mode = 0x0;
	cfg_dec_cmd.stream_id = audio->dec_id;
	return audpp_send_queue1(&cfg_dec_cmd, sizeof(cfg_dec_cmd));
}

static void audlpa_async_send_buffer(struct audio *audio)
{
	int	found = 0;
	uint64_t temp = 0;
	struct audplay_cmd_bitstream_data_avail cmd;
	struct audlpa_buffer_node *next_buf = NULL;

	temp = audio->bytecount_head;
	if (audio->device_switch == DEVICE_SWITCH_STATE_NONE) {
		list_for_each_entry(next_buf, &audio->out_queue, list) {
			if (temp == audio->bytecount_given) {
				found = 1;
				break;
			} else
				temp += next_buf->buf.data_len;
		}
		if (next_buf && found) {
			cmd.cmd_id = AUDPLAY_CMD_BITSTREAM_DATA_AVAIL;
			cmd.decoder_id = audio->dec_id;
			cmd.buf_ptr	= (unsigned) next_buf->paddr;
			cmd.buf_size = next_buf->buf.data_len >> 1;
			cmd.partition_number	= 0;
			audio->bytecount_given += next_buf->buf.data_len;
			wmb();
			audplay_send_queue0(audio, &cmd, sizeof(cmd));
			audio->out_needed = 0;
			audio->drv_status |= ADRV_STATUS_OBUF_GIVEN;
		}
	} else if (audio->device_switch == DEVICE_SWITCH_STATE_COMPLETE) {
		audio->device_switch = DEVICE_SWITCH_STATE_NONE;
		next_buf = list_first_entry(&audio->out_queue,
					struct audlpa_buffer_node, list);
		if (next_buf) {
			cmd.cmd_id = AUDPLAY_CMD_BITSTREAM_DATA_AVAIL;
			cmd.decoder_id = audio->dec_id;
			temp = audio->bytecount_head +
				next_buf->buf.data_len -
				audio->bytecount_consumed;
			if (audpp_restore_avsync(audio->dec_id,
						&audio->avsync[0]))
				MM_DBG("audpp_restore_avsync failed\n");

			if ((signed)(temp >= 0) &&
			((signed)(next_buf->buf.data_len - temp) >= 0)) {
				MM_DBG("audlpa_async_send_buffer - sending the"
					"rest of the buffer bassedon AV sync");
				cmd.buf_ptr	= (unsigned) (next_buf->paddr +
						  (next_buf->buf.data_len -
						   temp));
				cmd.buf_size = temp >> 1;
				cmd.partition_number	= 0;
				audio->bytecount_given =
					audio->bytecount_consumed + temp;
				wmb();
				audplay_send_queue0(audio, &cmd, sizeof(cmd));
				audio->out_needed = 0;
				audio->drv_status |= ADRV_STATUS_OBUF_GIVEN;
			} else if ((signed)(temp >= 0) &&
				((signed)(next_buf->buf.data_len -
							temp) < 0)) {
				MM_DBG("audlpa_async_send_buffer - else case:"
					"sending the rest of the buffer bassedon"
					"AV sync");
				cmd.buf_ptr	= (unsigned) next_buf->paddr;
				cmd.buf_size = next_buf->buf.data_len >> 1;
				cmd.partition_number	= 0;
				audio->bytecount_given = audio->bytecount_head +
					next_buf->buf.data_len;
				wmb();
				audplay_send_queue0(audio, &cmd, sizeof(cmd));
				audio->out_needed = 0;
				audio->drv_status |= ADRV_STATUS_OBUF_GIVEN;
			}
		}
	}
}

static void audlpa_async_send_data(struct audio *audio, unsigned needed,
				uint32_t *payload)
{
	unsigned long flags;
	uint64_t temp = 0;

	spin_lock_irqsave(&audio->dsp_lock, flags);
	if (!audio->running)
		goto done;

	if (needed && !audio->wflush) {
		audio->out_needed = 1;
		if (audio->drv_status & ADRV_STATUS_OBUF_GIVEN) {
			union msm_audio_event_payload evt_payload;
			struct audlpa_buffer_node *used_buf = NULL;

			if (CHECK_ERROR(payload))
				audio->bytecount_consumed =
					CALCULATE_AVSYNC_FROM_PAYLOAD(payload);

			if ((audio->device_switch ==
				DEVICE_SWITCH_STATE_COMPLETE) &&
				(audio->avsync_flag == 1)) {
				audio->avsync_flag = 0;
				audio->bytecount_consumed =
					CALCULATE_AVSYNC(audio->avsync);
			}
			BUG_ON(list_empty(&audio->out_queue));
			temp = audio->bytecount_head;
			used_buf = list_first_entry(&audio->out_queue,
					struct audlpa_buffer_node, list);
			if (audio->device_switch !=
				DEVICE_SWITCH_STATE_COMPLETE) {
				audio->bytecount_head +=
						used_buf->buf.data_len;
				temp = audio->bytecount_head;
				list_del(&used_buf->list);
				evt_payload.aio_buf = used_buf->buf;
				audlpa_post_event(audio,
						AUDIO_EVENT_WRITE_DONE,
						  evt_payload);
				kfree(used_buf);
				audio->drv_status &= ~ADRV_STATUS_OBUF_GIVEN;
			}
		}
	}
	if (audio->out_needed) {
		if (!list_empty(&audio->out_queue))
			audlpa_async_send_buffer(audio);
	}
done:
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
}

/* ------------------- device --------------------- */
static void audlpa_async_flush(struct audio *audio)
{
	struct audlpa_buffer_node *buf_node;
	struct list_head *ptr, *next;
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	list_for_each_safe(ptr, next, &audio->out_queue) {
		buf_node = list_entry(ptr, struct audlpa_buffer_node, list);
		list_del(&buf_node->list);
		payload.aio_buf = buf_node->buf;
		if ((buf_node->paddr != 0xFFFFFFFF) &&
			(buf_node->buf.data_len != 0))
			audlpa_post_event(audio, AUDIO_EVENT_WRITE_DONE,
							  payload);
		kfree(buf_node);
	}
	audio->drv_status &= ~ADRV_STATUS_OBUF_GIVEN;
	audio->out_needed = 0;
	audio->bytecount_consumed = 0;
	audio->bytecount_head = 0;
	audio->bytecount_given = 0;
	audio->device_switch = DEVICE_SWITCH_STATE_NONE;
	atomic_set(&audio->out_bytes, 0);
}

static void audio_ioport_reset(struct audio *audio)
{
	/* If fsync is in progress, make sure
	 * return value of fsync indicates
	 * abort due to flush
	 */
	if (audio->drv_status & ADRV_STATUS_FSYNC) {
		MM_DBG("fsync in progress\n");
		wake_up(&audio->write_wait);
		mutex_lock(&audio->write_lock);
		audlpa_async_flush(audio);
		mutex_unlock(&audio->write_lock);
		audio->avsync_flag = 1;
		wake_up(&audio->avsync_wait);
	} else
		audlpa_async_flush(audio);
}

static int audlpa_events_pending(struct audio *audio)
{
	unsigned long flags;
	int empty;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	empty = !list_empty(&audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	return empty || audio->event_abort;
}

static void audlpa_reset_event_queue(struct audio *audio)
{
	unsigned long flags;
	struct audlpa_event *drv_evt;
	struct list_head *ptr, *next;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	list_for_each_safe(ptr, next, &audio->event_queue) {
		drv_evt = list_first_entry(&audio->event_queue,
			struct audlpa_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	list_for_each_safe(ptr, next, &audio->free_event_queue) {
		drv_evt = list_first_entry(&audio->free_event_queue,
			struct audlpa_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	return;
}

static long audlpa_process_event_req(struct audio *audio, void __user *arg)
{
	long rc;
	struct msm_audio_event usr_evt;
	struct audlpa_event *drv_evt = NULL;
	int timeout;
	unsigned long flags;

	if (copy_from_user(&usr_evt, arg, sizeof(struct msm_audio_event)))
		return -EFAULT;

	timeout = (int) usr_evt.timeout_ms;

	if (timeout > 0) {
		rc = wait_event_interruptible_timeout(
			audio->event_wait, audlpa_events_pending(audio),
			msecs_to_jiffies(timeout));
		if (rc == 0)
			return -ETIMEDOUT;
	} else {
		rc = wait_event_interruptible(
			audio->event_wait, audlpa_events_pending(audio));
	}

	if (rc < 0)
		return rc;

	if (audio->event_abort) {
		audio->event_abort = 0;
		return -ENODEV;
	}

	rc = 0;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	if (!list_empty(&audio->event_queue)) {
		drv_evt = list_first_entry(&audio->event_queue,
			struct audlpa_event, list);
		list_del(&drv_evt->list);
	}
	if (drv_evt) {
		usr_evt.event_type = drv_evt->event_type;
		usr_evt.event_payload = drv_evt->payload;
		list_add_tail(&drv_evt->list, &audio->free_event_queue);
	} else
		rc = -1;
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	if (drv_evt->event_type == AUDIO_EVENT_WRITE_DONE ||
	    drv_evt->event_type == AUDIO_EVENT_READ_DONE) {
		mutex_lock(&audio->lock);
		audlpa_pmem_fixup(audio, drv_evt->payload.aio_buf.buf_addr,
				  drv_evt->payload.aio_buf.buf_len, 0);
		mutex_unlock(&audio->lock);
	}
	if (!rc && copy_to_user(arg, &usr_evt, sizeof(usr_evt)))
		rc = -EFAULT;

	return rc;
}

static int audlpa_pmem_check(struct audio *audio,
		void *vaddr, unsigned long len)
{
	struct audlpa_pmem_region *region_elt;
	struct audlpa_pmem_region t = { .vaddr = vaddr, .len = len };

	list_for_each_entry(region_elt, &audio->pmem_region_queue, list) {
		if (CONTAINS(region_elt, &t) || CONTAINS(&t, region_elt) ||
		    OVERLAPS(region_elt, &t)) {
			MM_ERR("region (vaddr %p len %ld)"
				" clashes with registered region"
				" (vaddr %p paddr %p len %ld)\n",
				vaddr, len,
				region_elt->vaddr,
				(void *)region_elt->paddr,
				region_elt->len);
			return -EINVAL;
		}
	}

	return 0;
}

static int audlpa_pmem_add(struct audio *audio,
	struct msm_audio_pmem_info *info)
{
	unsigned long paddr, kvaddr, len;
	struct file *file;
	struct audlpa_pmem_region *region;
	int rc = -EINVAL;

	MM_DBG("\n"); /* Macro prints the file name and function */
	region = kmalloc(sizeof(*region), GFP_KERNEL);

	if (!region) {
		rc = -ENOMEM;
		goto end;
	}

	if (get_pmem_file(info->fd, &paddr, &kvaddr, &len, &file)) {
		kfree(region);
		goto end;
	}

	rc = audlpa_pmem_check(audio, info->vaddr, len);
	if (rc < 0) {
		put_pmem_file(file);
		kfree(region);
		goto end;
	}

	region->vaddr = info->vaddr;
	region->fd = info->fd;
	region->paddr = paddr;
	region->kvaddr = kvaddr;
	region->len = len;
	region->file = file;
	region->ref_cnt = 0;
	MM_DBG("add region paddr %lx vaddr %p, len %lu\n", region->paddr,
			region->vaddr, region->len);
	list_add_tail(&region->list, &audio->pmem_region_queue);
end:
	return rc;
}

static int audlpa_pmem_remove(struct audio *audio,
	struct msm_audio_pmem_info *info)
{
	struct audlpa_pmem_region *region;
	struct list_head *ptr, *next;
	int rc = -EINVAL;

	MM_DBG("info fd %d vaddr %p\n", info->fd, info->vaddr);

	list_for_each_safe(ptr, next, &audio->pmem_region_queue) {
		region = list_entry(ptr, struct audlpa_pmem_region, list);

		if ((region->fd == info->fd) &&
		    (region->vaddr == info->vaddr)) {
			if (region->ref_cnt) {
				MM_DBG("region %p in use ref_cnt %d\n",
						region, region->ref_cnt);
				break;
			}
			MM_DBG("remove region fd %d vaddr %p\n",
				info->fd, info->vaddr);
			list_del(&region->list);
			put_pmem_file(region->file);
			kfree(region);
			rc = 0;
			break;
		}
	}

	return rc;
}

static int audlpa_pmem_lookup_vaddr(struct audio *audio, void *addr,
		     unsigned long len, struct audlpa_pmem_region **region)
{
	struct audlpa_pmem_region *region_elt;

	int match_count = 0;

	*region = NULL;

	/* returns physical address or zero */
	list_for_each_entry(region_elt, &audio->pmem_region_queue,
		list) {
		if (addr >= region_elt->vaddr &&
		    addr < region_elt->vaddr + region_elt->len &&
		    addr + len <= region_elt->vaddr + region_elt->len) {
			/* offset since we could pass vaddr inside a registerd
			 * pmem buffer
			 */

			match_count++;
			if (!*region)
				*region = region_elt;
		}
	}

	if (match_count > 1) {
		MM_ERR("multiple hits for vaddr %p, len %ld\n", addr, len);
		list_for_each_entry(region_elt,
		  &audio->pmem_region_queue, list) {
			if (addr >= region_elt->vaddr &&
			    addr < region_elt->vaddr + region_elt->len &&
			    addr + len <= region_elt->vaddr + region_elt->len)
				MM_ERR("\t%p, %ld --> %p\n", region_elt->vaddr,
						region_elt->len,
						(void *)region_elt->paddr);
		}
	}

	return *region ? 0 : -1;
}

unsigned long audlpa_pmem_fixup(struct audio *audio, void *addr,
		    unsigned long len, int ref_up)
{
	struct audlpa_pmem_region *region;
	unsigned long paddr;
	int ret;

	ret = audlpa_pmem_lookup_vaddr(audio, addr, len, &region);
	if (ret) {
		MM_ERR("lookup (%p, %ld) failed\n", addr, len);
		return 0;
	}
	if (ref_up)
		region->ref_cnt++;
	else
		region->ref_cnt--;
	MM_DBG("found region %p ref_cnt %d\n", region, region->ref_cnt);
	paddr = region->paddr + (addr - region->vaddr);
	return paddr;
}

/* audio -> lock must be held at this point */
static int audlpa_aio_buf_add(struct audio *audio, unsigned dir,
	void __user *arg)
{
	unsigned long flags;
	struct audlpa_buffer_node *buf_node;

	buf_node = kmalloc(sizeof(*buf_node), GFP_KERNEL);

	if (!buf_node)
		return -ENOMEM;

	if (copy_from_user(&buf_node->buf, arg, sizeof(buf_node->buf))) {
		kfree(buf_node);
		return -EFAULT;
	}

	MM_DBG("node %p dir %x buf_addr %p buf_len %d data_len"
			"%d\n", buf_node, dir,
			buf_node->buf.buf_addr, buf_node->buf.buf_len,
			buf_node->buf.data_len);

	buf_node->paddr = audlpa_pmem_fixup(
		audio, buf_node->buf.buf_addr,
		buf_node->buf.buf_len, 1);

	if (dir) {
		/* write */
		if (!buf_node->paddr ||
		    (buf_node->paddr & 0x1) ||
		    (buf_node->buf.data_len & 0x1)) {
			kfree(buf_node);
			return -EINVAL;
		}
		spin_lock_irqsave(&audio->dsp_lock, flags);
		list_add_tail(&buf_node->list, &audio->out_queue);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		audlpa_async_send_data(audio, 0, 0);
	} else {
		/* read */
	}

	MM_DBG("Add buf_node %p paddr %lx\n", buf_node, buf_node->paddr);

	return 0;
}

static int audio_enable_eq(struct audio *audio, int enable)
{
	if (audio->eq_enable == enable && !audio->eq_needs_commit)
		return 0;

	audio->eq_enable = enable;

	if (audio->running) {
		audpp_dsp_set_eq(audio->dec_id, enable, &audio->eq, POPP);
		audio->eq_needs_commit = 0;
	}
	return 0;
}

static int audio_get_avsync_data(struct audio *audio,
						struct msm_audio_stats *stats)
{
	int rc = -EINVAL;
	unsigned long flags;

	local_irq_save(flags);
	if (audio->dec_id == audio->avsync[0] && audio->avsync_flag) {
		/* av_sync sample count */
		stats->sample_count = (audio->avsync[2] << 16) |
						(audio->avsync[3]);

		/* av_sync byte_count */
		stats->byte_count = (audio->avsync[5] << 16) |
						(audio->avsync[6]);

		audio->avsync_flag = 0;
		rc = 0;
	}
	local_irq_restore(flags);
	return rc;

}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = -EINVAL;
	unsigned long flags = 0;
	uint16_t enable_mask;
	int enable;
	int prev_state;

	MM_DBG("audio_ioctl() cmd = %d\n", cmd);

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;

		audio->avsync_flag = 0;
		memset(&stats, 0, sizeof(stats));
		if (audpp_query_avsync(audio->dec_id) < 0)
			return rc;

		rc = wait_event_interruptible_timeout(audio->avsync_wait,
				(audio->avsync_flag == 1),
				msecs_to_jiffies(AVSYNC_EVENT_TIMEOUT));

		if (rc < 0)
			return rc;
		else if ((rc > 0) || ((rc == 0) && (audio->avsync_flag == 1))) {
			if (audio_get_avsync_data(audio, &stats) < 0)
				return rc;

			if (copy_to_user((void *) arg, &stats, sizeof(stats)))
				return -EFAULT;
			return 0;
		} else
			return -EAGAIN;
	}

	switch (cmd) {
	case AUDIO_ENABLE_AUDPP:
		if (copy_from_user(&enable_mask, (void *) arg,
						sizeof(enable_mask))) {
			rc = -EFAULT;
			break;
		}

		spin_lock_irqsave(&audio->dsp_lock, flags);
		enable = (enable_mask & EQ_ENABLE) ? 1 : 0;
		audio_enable_eq(audio, enable);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		rc = 0;
		break;

	case AUDIO_SET_VOLUME:
		spin_lock_irqsave(&audio->dsp_lock, flags);
		audio->vol_pan.volume = arg;
		if (audio->running)
			audpp_dsp_set_vol_pan(AUDPP_CMD_CFG_DEV_MIXER_ID_4,
						&audio->vol_pan,
						COPP);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		rc = 0;
		break;

	case AUDIO_SET_PAN:
		spin_lock_irqsave(&audio->dsp_lock, flags);
		audio->vol_pan.pan = arg;
		if (audio->running)
			audpp_dsp_set_vol_pan(AUDPP_CMD_CFG_DEV_MIXER_ID_4,
						&audio->vol_pan,
						COPP);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		rc = 0;
		break;

	case AUDIO_SET_EQ:
		prev_state = audio->eq_enable;
		audio->eq_enable = 0;
		if (copy_from_user(&audio->eq.num_bands, (void *) arg,
				sizeof(audio->eq) -
				(AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN + 2))) {
			rc = -EFAULT;
			break;
		}
		audio->eq_enable = prev_state;
		audio->eq_needs_commit = 1;
		rc = 0;
		break;
	}

	if (-EINVAL != rc)
		return rc;

	if (cmd == AUDIO_GET_EVENT) {
		MM_DBG(" AUDIO_GET_EVENT\n");
		if (mutex_trylock(&audio->get_event_lock)) {
			rc = audlpa_process_event_req(audio,
				(void __user *) arg);
			mutex_unlock(&audio->get_event_lock);
		} else
			rc = -EBUSY;
		return rc;
	}

	if (cmd == AUDIO_ABORT_GET_EVENT) {
		audio->event_abort = 1;
		wake_up(&audio->event_wait);
		return 0;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START:
		MM_DBG("AUDIO_START\n");
		rc = audio_enable(audio);
		if (!rc) {
			rc = wait_event_interruptible_timeout(audio->wait,
				audio->dec_state != MSM_AUD_DECODER_STATE_NONE,
				msecs_to_jiffies(MSM_AUD_DECODER_WAIT_MS));
			MM_DBG("dec_state %d rc = %d\n", audio->dec_state, rc);

			if (audio->dec_state != MSM_AUD_DECODER_STATE_SUCCESS)
				rc = -ENODEV;
			else
				rc = 0;
		}
		break;

	case AUDIO_STOP:
		MM_DBG("AUDIO_STOP\n");
		rc = audio_disable(audio);
		audio->stopped = 1;
		audio_ioport_reset(audio);
		audio->stopped = 0;
		audio->drv_status &= ~ADRV_STATUS_PAUSE;
		break;

	case AUDIO_FLUSH:
		MM_DBG("AUDIO_FLUSH\n");
		audio->wflush = 1;
		audio_ioport_reset(audio);
		if (audio->running) {
			if (!(audio->drv_status & ADRV_STATUS_PAUSE)) {
				rc = audpp_pause(audio->dec_id, (int) arg);
				if (rc < 0) {
					MM_ERR("%s: pause cmd failed rc=%d\n",
						__func__, rc);
					rc = -EINTR;
					break;
				}
			}
			audpp_flush(audio->dec_id);
			rc = wait_event_interruptible(audio->write_wait,
				!audio->wflush);
			if (rc < 0) {
				MM_ERR("AUDIO_FLUSH interrupted\n");
				rc = -EINTR;
			}
		} else {
			audio->wflush = 0;
		}
		break;

	case AUDIO_SET_CONFIG:{
		struct msm_audio_config config;
		MM_INFO("AUDIO_SET_CONFIG\n");
		if (copy_from_user(&config, (void *) arg, sizeof(config))) {
			rc = -EFAULT;
			MM_INFO("ERROR: copy from user\n");
			break;
		}
		if (config.channel_count == 1) {
			config.channel_count = AUDPP_CMD_PCM_INTF_MONO_V;
		} else if (config.channel_count == 2) {
			config.channel_count = AUDPP_CMD_PCM_INTF_STEREO_V;
		} else {
			rc = -EINVAL;
			MM_INFO("ERROR: config.channel_count == %d\n",
					config.channel_count);
			break;
		}

		if (config.bits == 8)
			config.bits = AUDPP_CMD_WAV_PCM_WIDTH_8;
		else if (config.bits == 16)
			config.bits = AUDPP_CMD_WAV_PCM_WIDTH_16;
		else if (config.bits == 24)
			config.bits = AUDPP_CMD_WAV_PCM_WIDTH_24;
		else {
			rc = -EINVAL;
			MM_INFO("ERROR: config.bits == %d\n", config.bits);
			break;
		}
		audio->out_sample_rate = config.sample_rate;
		audio->out_channel_mode = config.channel_count;
		audio->out_bits = config.bits;
		audio->buffer_count = config.buffer_count;
		audio->buffer_size = config.buffer_size;
		MM_DBG("AUDIO_SET_CONFIG: config.bits = %d\n", config.bits);
		rc = 0;
		break;
	}

	case AUDIO_GET_CONFIG:{
		struct msm_audio_config config;
		config.buffer_count = audio->buffer_count;
		config.buffer_size = audio->buffer_size;
		config.sample_rate = audio->out_sample_rate;
		if (audio->out_channel_mode == AUDPP_CMD_PCM_INTF_MONO_V)
			config.channel_count = 1;
		else
			config.channel_count = 2;
		if (audio->out_bits == AUDPP_CMD_WAV_PCM_WIDTH_8)
			config.bits = 8;
		else if (audio->out_bits == AUDPP_CMD_WAV_PCM_WIDTH_24)
			config.bits = 24;
		else
			config.bits = 16;
		config.meta_field = 0;
		config.unused[0] = 0;
		config.unused[1] = 0;
		config.unused[2] = 0;
		MM_DBG("AUDIO_GET_CONFIG: config.bits = %d\n", config.bits);
		if (copy_to_user((void *) arg, &config, sizeof(config)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}

	case AUDIO_PAUSE:
		MM_DBG("AUDIO_PAUSE %ld\n", arg);
		rc = audpp_pause(audio->dec_id, (int) arg);
		if (arg == 1)
			audio->drv_status |= ADRV_STATUS_PAUSE;
		else if (arg == 0)
			audio->drv_status &= ~ADRV_STATUS_PAUSE;
		break;

	case AUDIO_REGISTER_PMEM: {
			struct msm_audio_pmem_info info;
			MM_DBG("AUDIO_REGISTER_PMEM\n");
			if (copy_from_user(&info, (void *) arg, sizeof(info)))
				rc = -EFAULT;
			else
				rc = audlpa_pmem_add(audio, &info);
			break;
		}

	case AUDIO_DEREGISTER_PMEM: {
			struct msm_audio_pmem_info info;
			MM_DBG("AUDIO_DEREGISTER_PMEM\n");
			if (copy_from_user(&info, (void *) arg, sizeof(info)))
				rc = -EFAULT;
			else
				rc = audlpa_pmem_remove(audio, &info);
			break;
		}
	case AUDIO_ASYNC_WRITE:
		if (audio->drv_status & ADRV_STATUS_FSYNC)
			rc = -EBUSY;
		else
			rc = audlpa_aio_buf_add(audio, 1, (void __user *) arg);
		break;

	case AUDIO_GET_SESSION_ID:
		if (copy_to_user((void *) arg, &audio->dec_id,
					sizeof(unsigned short)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	default:
		rc = audio->codec_ops.ioctl(file, cmd, arg);
	}
	mutex_unlock(&audio->lock);
	return rc;
}

/* Only useful in tunnel-mode */
int audlpa_async_fsync(struct audio *audio)
{
	int rc = 0, empty = 0;
	struct audlpa_buffer_node *buf_node;

	MM_DBG("\n"); /* Macro prints the file name and function */

	/* Blocking client sends more data */
	mutex_lock(&audio->lock);
	audio->drv_status |= ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	mutex_lock(&audio->write_lock);
	audio->teos = 0;
	empty = list_empty(&audio->out_queue);
	buf_node = kmalloc(sizeof(*buf_node), GFP_KERNEL);
	if (!buf_node)
		goto done;

	buf_node->paddr = 0xFFFFFFFF;
	buf_node->buf.data_len = 0;
	buf_node->buf.buf_addr = NULL;
	buf_node->buf.buf_len = 0;
	buf_node->buf.private_data = NULL;
	list_add_tail(&buf_node->list, &audio->out_queue);
	if ((empty != 0) && (audio->out_needed == 1))
		audlpa_async_send_data(audio, 0, 0);

	rc = wait_event_interruptible(audio->write_wait,
				  audio->teos || audio->wflush ||
				  audio->stopped);

	if (rc < 0)
		goto done;

	if (audio->teos == 1) {
		/* Releasing all the pending buffers to user */
		audio->teos = 0;
		audlpa_async_flush(audio);
	}

	if (audio->stopped || audio->wflush)
		rc = -EBUSY;

done:
	mutex_unlock(&audio->write_lock);
	mutex_lock(&audio->lock);
	audio->drv_status &= ~ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	return rc;
}

int audlpa_fsync(struct file *file, int datasync)
{
	struct audio *audio = file->private_data;

	if (!audio->running)
		return -EINVAL;

	return audlpa_async_fsync(audio);
}

static void audlpa_reset_pmem_region(struct audio *audio)
{
	struct audlpa_pmem_region *region;
	struct list_head *ptr, *next;

	list_for_each_safe(ptr, next, &audio->pmem_region_queue) {
		region = list_entry(ptr, struct audlpa_pmem_region, list);
		list_del(&region->list);
		put_pmem_file(region->file);
		kfree(region);
	}

	return;
}

static int audio_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	MM_DBG("\n"); /* Macro prints the file name and function */

	MM_INFO("audio instance 0x%08x freeing\n", (int)audio);
	mutex_lock(&audio->lock);
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, audio->dec_id);
	audio_disable(audio);
	audlpa_async_flush(audio);
	audlpa_reset_pmem_region(audio);

	msm_adsp_put(audio->audplay);
	audpp_adec_free(audio->dec_id);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&audio->suspend_ctl.node);
#endif
	audio->opened = 0;
	audio->event_abort = 1;
	wake_up(&audio->event_wait);
	audlpa_reset_event_queue(audio);
	iounmap(audio->data);
	pmem_kfree(audio->phys);
	mutex_unlock(&audio->lock);
#ifdef CONFIG_DEBUG_FS
	if (audio->dentry)
		debugfs_remove(audio->dentry);
#endif
	kfree(audio);
	return 0;
}

static void audlpa_post_event(struct audio *audio, int type,
	union msm_audio_event_payload payload)
{
	struct audlpa_event *e_node = NULL;
	unsigned long flags;

	spin_lock_irqsave(&audio->event_queue_lock, flags);

	if (!list_empty(&audio->free_event_queue)) {
		e_node = list_first_entry(&audio->free_event_queue,
			struct audlpa_event, list);
		list_del(&e_node->list);
	} else {
		e_node = kmalloc(sizeof(struct audlpa_event), GFP_ATOMIC);
		if (!e_node) {
			MM_ERR("No mem to post event %d\n", type);
			return;
		}
	}

	e_node->event_type = type;
	e_node->payload = payload;

	list_add_tail(&e_node->list, &audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	wake_up(&audio->event_wait);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void audlpa_suspend(struct early_suspend *h)
{
	struct audlpa_suspend_ctl *ctl =
		container_of(h, struct audlpa_suspend_ctl, node);
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	audlpa_post_event(ctl->audio, AUDIO_EVENT_SUSPEND, payload);
}

static void audlpa_resume(struct early_suspend *h)
{
	struct audlpa_suspend_ctl *ctl =
		container_of(h, struct audlpa_suspend_ctl, node);
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	audlpa_post_event(ctl->audio, AUDIO_EVENT_RESUME, payload);
}
#endif

#ifdef CONFIG_DEBUG_FS
static ssize_t audlpa_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t audlpa_debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	const int debug_bufmax = 4096;
	static char buffer[4096];
	int n = 0;
	struct audio *audio = file->private_data;

	mutex_lock(&audio->lock);
	n = scnprintf(buffer, debug_bufmax, "opened %d\n", audio->opened);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"enabled %d\n", audio->enabled);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"stopped %d\n", audio->stopped);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"volume %x\n", audio->vol_pan.volume);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"sample rate %d\n",
					audio->out_sample_rate);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"channel mode %d\n",
					audio->out_channel_mode);
	mutex_unlock(&audio->lock);
	/* Following variables are only useful for debugging when
	 * when playback halts unexpectedly. Thus, no mutual exclusion
	 * enforced
	 */
	n += scnprintf(buffer + n, debug_bufmax - n,
					"wflush %d\n", audio->wflush);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"running %d\n", audio->running);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"dec state %d\n", audio->dec_state);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"out_needed %d\n", audio->out_needed);
	buffer[n] = 0;
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static const struct file_operations audlpa_debug_fops = {
	.read = audlpa_debug_read,
	.open = audlpa_debug_open,
};
#endif

static int audio_open(struct inode *inode, struct file *file)
{
	struct audio *audio = NULL;
	int rc, i, dec_attrb = 0, decid;
	struct audlpa_event *e_node = NULL;
#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_lpa_" + 5];
#endif

	/* Allocate audio instance, set to zero */
	audio = kzalloc(sizeof(struct audio), GFP_KERNEL);
	if (!audio) {
		MM_ERR("no memory to allocate audio instance\n");
		rc = -ENOMEM;
		goto done;
	}
	MM_INFO("audio instance 0x%08x created\n", (int)audio);

	if ((file->f_mode & FMODE_WRITE) && !(file->f_mode & FMODE_READ)) {
		dec_attrb |= MSM_AUD_MODE_TUNNEL;
	} else {
		kfree(audio);
		rc = -EACCES;
		goto done;
	}

	/* Allocate the decoder based on inode minor number*/
	audio->minor_no = iminor(inode);
	dec_attrb |= audlpa_decs[audio->minor_no].dec_attrb;
	audio->codec_ops.ioctl = audlpa_decs[audio->minor_no].ioctl;
	audio->codec_ops.adec_params = audlpa_decs[audio->minor_no].adec_params;
	audio->buffer_size = BUFSZ;
	audio->buffer_count = MAX_BUF;

	dec_attrb |= MSM_AUD_MODE_LP;

	decid = audpp_adec_alloc(dec_attrb, &audio->module_name,
			&audio->queue_id);
	if (decid < 0) {
		MM_ERR("No free decoder available\n");
		rc = -ENODEV;
		MM_INFO("audio instance 0x%08x freeing\n", (int)audio);
		kfree(audio);
		goto done;
	}
	audio->dec_id = decid & MSM_AUD_DECODER_MASK;

	MM_DBG("set to aio interface\n");
	audio->drv_status |= ADRV_STATUS_AIO_INTF;

	rc = msm_adsp_get(audio->module_name, &audio->audplay,
		&audplay_adsp_ops_lpa, audio);

	if (rc) {
		MM_ERR("failed to get %s module\n", audio->module_name);
		goto err;
	}

	/* Initialize all locks of audio instance */
	mutex_init(&audio->lock);
	mutex_init(&audio->write_lock);
	mutex_init(&audio->get_event_lock);
	spin_lock_init(&audio->dsp_lock);
	init_waitqueue_head(&audio->write_wait);
	INIT_LIST_HEAD(&audio->out_queue);
	INIT_LIST_HEAD(&audio->pmem_region_queue);
	INIT_LIST_HEAD(&audio->free_event_queue);
	INIT_LIST_HEAD(&audio->event_queue);
	init_waitqueue_head(&audio->wait);
	init_waitqueue_head(&audio->event_wait);
	spin_lock_init(&audio->event_queue_lock);
	init_waitqueue_head(&audio->avsync_wait);

	audio->out_sample_rate = 44100;
	audio->out_channel_mode = AUDPP_CMD_PCM_INTF_STEREO_V;
	audio->out_bits = AUDPP_CMD_WAV_PCM_WIDTH_16;
	audio->vol_pan.volume = 0x2000;

	audlpa_async_flush(audio);

	file->private_data = audio;
	audio->opened = 1;

	audio->device_events = AUDDEV_EVT_DEV_RDY
				|AUDDEV_EVT_DEV_RLS | AUDDEV_EVT_REL_PENDING
				|AUDDEV_EVT_STREAM_VOL_CHG;
	audio->device_switch = DEVICE_SWITCH_STATE_NONE;
	audio->drv_status &= ~ADRV_STATUS_PAUSE;
	audio->bytecount_consumed = 0;
	audio->bytecount_head = 0;
	audio->bytecount_given = 0;

	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_DEC,
					audio->dec_id,
					lpa_listner,
					(void *)audio);
	if (rc) {
		MM_ERR("%s: failed to register listnet\n", __func__);
		goto event_err;
	}

#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof name, "msm_lpa_%04x", audio->dec_id);
	audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
			NULL, (void *) audio, &audlpa_debug_fops);

	if (IS_ERR(audio->dentry))
		MM_DBG("debugfs_create_file failed\n");
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	audio->suspend_ctl.node.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	audio->suspend_ctl.node.resume = audlpa_resume;
	audio->suspend_ctl.node.suspend = audlpa_suspend;
	audio->suspend_ctl.audio = audio;
	register_early_suspend(&audio->suspend_ctl.node);
#endif
	for (i = 0; i < AUDLPA_EVENT_NUM; i++) {
		e_node = kmalloc(sizeof(struct audlpa_event), GFP_KERNEL);
		if (e_node)
			list_add_tail(&e_node->list, &audio->free_event_queue);
		else {
			MM_ERR("event pkt alloc failed\n");
			break;
		}
	}
done:
	return rc;
event_err:
	msm_adsp_put(audio->audplay);
err:
	iounmap(audio->data);
	pmem_kfree(audio->phys);
	audpp_adec_free(audio->dec_id);
	MM_INFO("audio instance 0x%08x freeing\n", (int)audio);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_lpa_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_open,
	.release	= audio_release,
	.unlocked_ioctl	= audio_ioctl,
	.fsync		= audlpa_fsync,
};

static dev_t audlpa_devno;
static struct class *audlpa_class;
struct audlpa_device {
	const char *name;
	struct device *device;
	struct cdev cdev;
};

static struct audlpa_device *audlpa_devices;

static void audlpa_create(struct audlpa_device *adev, const char *name,
			struct device *parent, dev_t devt)
{
	struct device *dev;
	int rc;

	dev = device_create(audlpa_class, parent, devt, "%s", name);
	if (IS_ERR(dev))
		return;

	cdev_init(&adev->cdev, &audio_lpa_fops);
	adev->cdev.owner = THIS_MODULE;

	rc = cdev_add(&adev->cdev, devt, 1);
	if (rc < 0) {
		device_destroy(audlpa_class, devt);
	} else {
		adev->device = dev;
		adev->name = name;
	}
}

static int __init audio_init(void)
{
	int rc;
	int n = ARRAY_SIZE(audlpa_decs);

	audlpa_devices = kzalloc(sizeof(struct audlpa_device) * n, GFP_KERNEL);
	if (!audlpa_devices)
		return -ENOMEM;

	audlpa_class = class_create(THIS_MODULE, "audlpa");
	if (IS_ERR(audlpa_class))
		goto fail_create_class;

	rc = alloc_chrdev_region(&audlpa_devno, 0, n, "msm_audio_lpa");
	if (rc < 0)
		goto fail_alloc_region;

	for (n = 0; n < ARRAY_SIZE(audlpa_decs); n++) {
		audlpa_create(audlpa_devices + n,
				audlpa_decs[n].name, NULL,
				MKDEV(MAJOR(audlpa_devno), n));
	}

	return 0;

fail_alloc_region:
	class_unregister(audlpa_class);
	return rc;
fail_create_class:
	kfree(audlpa_devices);
	return -ENOMEM;
}

static void __exit audio_exit(void)
{
	class_unregister(audlpa_class);
	kfree(audlpa_devices);
}

module_init(audio_init);
module_exit(audio_exit);

MODULE_DESCRIPTION("MSM LPA driver");
MODULE_LICENSE("GPL v2");

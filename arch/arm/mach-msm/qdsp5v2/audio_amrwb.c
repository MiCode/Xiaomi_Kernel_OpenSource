/* amrwb audio decoder device
 *
 * Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 *
 * Based on the mp3 native driver in arch/arm/mach-msm/qdsp5v2/audio_mp3.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
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
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <asm/atomic.h>
#include <asm/ioctls.h>

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/earlysuspend.h>
#include <linux/android_pmem.h>
#include <linux/memory_alloc.h>
#include <linux/msm_audio.h>
#include <linux/slab.h>

#include <mach/msm_adsp.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/msm_subsystem_map.h>
#include <mach/qdsp5v2/qdsp5audppmsg.h>
#include <mach/qdsp5v2/qdsp5audplaycmdi.h>
#include <mach/qdsp5v2/qdsp5audplaymsg.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/qdsp5v2/audpp.h>
#include <mach/debug_mm.h>
#include <mach/msm_memtypes.h>

#define BUFSZ 4110 /* Hold minimum 700ms voice data and 14 bytes of meta in*/
#define DMASZ (BUFSZ * 2)

#define AUDPLAY_INVALID_READ_PTR_OFFSET	0xFFFF
#define AUDDEC_DEC_AMRWB 11

#define PCM_BUFSZ_MIN 8216 /* 100ms worth of data and 24 bytes of meta out*/
#define PCM_BUF_MAX_COUNT 5	/* DSP only accepts 5 buffers at most
				   but support 2 buffers currently */
#define ROUTING_MODE_FTRT 1
#define ROUTING_MODE_RT 2
/* Decoder status received from AUDPPTASK */
#define  AUDPP_DEC_STATUS_SLEEP	0
#define	 AUDPP_DEC_STATUS_INIT  1
#define  AUDPP_DEC_STATUS_CFG   2
#define  AUDPP_DEC_STATUS_PLAY  3

#define AUDAMRWB_METAFIELD_MASK 0xFFFF0000
#define AUDAMRWB_EOS_FLG_OFFSET 0x0A /* Offset from beginning of buffer */
#define AUDAMRWB_EOS_FLG_MASK 0x01
#define AUDAMRWB_EOS_NONE 0x0 /* No EOS detected */
#define AUDAMRWB_EOS_SET 0x1 /* EOS set in meta field */

#define AUDAMRWB_EVENT_NUM 10 /* Default number of pre-allocated event pkts */

struct buffer {
	void *data;
	unsigned size;
	unsigned used;		/* Input usage actual DSP produced PCM size  */
	unsigned addr;
	unsigned short mfield_sz; /*only useful for data has meta field */
};

#ifdef CONFIG_HAS_EARLYSUSPEND
struct audamrwb_suspend_ctl {
	struct early_suspend node;
	struct audio *audio;
};
#endif

struct audamrwb_event{
	struct list_head list;
	int event_type;
	union msm_audio_event_payload payload;
};

struct audio {
	struct buffer out[2];

	spinlock_t dsp_lock;

	uint8_t out_head;
	uint8_t out_tail;
	uint8_t out_needed;	/* number of buffers the dsp is waiting for */

	atomic_t out_bytes;

	struct mutex lock;
	struct mutex write_lock;
	wait_queue_head_t write_wait;

	/* Host PCM section */
	struct buffer in[PCM_BUF_MAX_COUNT];
	struct mutex read_lock;
	wait_queue_head_t read_wait;	/* Wait queue for read */
	char *read_data;	/* pointer to reader buffer */
	int32_t read_phys;	/* physical address of reader buffer */
	uint8_t read_next;	/* index to input buffers to be read next */
	uint8_t fill_next;	/* index to buffer that DSP should be filling */
	uint8_t pcm_buf_count;	/* number of pcm buffer allocated */
	/* ---- End of Host PCM section */

	struct msm_adsp_module *audplay;

	/* configuration to use on next enable */
	uint32_t out_sample_rate;
	uint32_t out_channel_mode;

	/* data allocated for various buffers */
	char *data;
	int32_t phys; /* physical address of write buffer */

	struct msm_mapped_buffer *map_v_read;
	struct msm_mapped_buffer *map_v_write;

	int mfield; /* meta field embedded in data */
	int rflush; /* Read  flush */
	int wflush; /* Write flush */
	int opened;
	int enabled;
	int running;
	int stopped;	/* set when stopped, cleared on flush */
	int pcm_feedback;
	int buf_refresh;
	int teos; /* valid only if tunnel mode & no data left for decoder */
	enum msm_aud_decoder_state dec_state;	/* Represents decoder state */
	int reserved; /* A byte is being reserved */
	char rsv_byte; /* Handle odd length user data */

	const char *module_name;
	unsigned queue_id;
	uint16_t dec_id;
	uint32_t read_ptr_offset;
	int16_t source;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct audamrwb_suspend_ctl suspend_ctl;
#endif

#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif

	wait_queue_head_t wait;
	struct list_head free_event_queue;
	struct list_head event_queue;
	wait_queue_head_t event_wait;
	spinlock_t event_queue_lock;
	struct mutex get_event_lock;
	int event_abort;
	/* AV sync Info */
	int avsync_flag;              /* Flag to indicate feedback from DSP */
	wait_queue_head_t avsync_wait;/* Wait queue for AV Sync Message     */
	/* flags, 48 bits sample/bytes counter per channel */
	uint16_t avsync[AUDPP_AVSYNC_CH_COUNT * AUDPP_AVSYNC_NUM_WORDS + 1];

	uint32_t device_events;

	int eq_enable;
	int eq_needs_commit;
	struct audpp_cmd_cfg_object_params_eqalizer eq;
	struct audpp_cmd_cfg_object_params_volume vol_pan;
};

static int auddec_dsp_config(struct audio *audio, int enable);
static void audpp_cmd_cfg_adec_params(struct audio *audio);
static void audpp_cmd_cfg_routing_mode(struct audio *audio);
static void audamrwb_send_data(struct audio *audio, unsigned needed);
static void audamrwb_config_hostpcm(struct audio *audio);
static void audamrwb_buffer_refresh(struct audio *audio);
static void audamrwb_dsp_event(void *private, unsigned id, uint16_t *msg);
static void audamrwb_post_event(struct audio *audio, int type,
		union msm_audio_event_payload payload);

/* must be called with audio->lock held */
static int audamrwb_enable(struct audio *audio)
{

	MM_DBG("\n"); /* Macro prints the file name and function */

	if (audio->enabled)
		return 0;

	audio->dec_state = MSM_AUD_DECODER_STATE_NONE;
	audio->out_tail = 0;
	audio->out_needed = 0;

	if (msm_adsp_enable(audio->audplay)) {
		MM_ERR("msm_adsp_enable(audplay) failed\n");
		return -ENODEV;
	}

	if (audpp_enable(audio->dec_id, audamrwb_dsp_event, audio)) {
		MM_ERR("audpp_enable() failed\n");
		msm_adsp_disable(audio->audplay);
		return -ENODEV;
	}
	audio->enabled = 1;
	return 0;
}

static void amrwb_listner(u32 evt_id, union auddev_evt_data *evt_payload,
			void *private_data)
{
	struct audio *audio = (struct audio *) private_data;
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		MM_DBG("AUDDEV_EVT_DEV_RDY\n");
		audio->source |= (0x1 << evt_payload->routing_id);
		if (audio->running == 1 && audio->enabled == 1)
			audpp_route_stream(audio->dec_id, audio->source);
		break;
	case AUDDEV_EVT_DEV_RLS:
		MM_DBG("AUDDEV_EVT_DEV_RLS\n");
		audio->source &= ~(0x1 << evt_payload->routing_id);
		if (audio->running == 1 && audio->enabled == 1)
			audpp_route_stream(audio->dec_id, audio->source);
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		audio->vol_pan.volume = evt_payload->session_vol;
		MM_DBG("AUDDEV_EVT_STREAM_VOL_CHG, stream vol %d\n",
				audio->vol_pan.volume);
		if (audio->running)
			audpp_dsp_set_vol_pan(audio->dec_id, &audio->vol_pan,
					POPP);
		break;
	default:
		MM_ERR("ERROR:wrong event\n");
		break;
	}
}

/* must be called with audio->lock held */
static int audamrwb_disable(struct audio *audio)
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
		wake_up(&audio->read_wait);
		msm_adsp_disable(audio->audplay);
		audpp_disable(audio->dec_id, audio);
		audio->out_needed = 0;
	}
	return rc;
}

/* ------------------- dsp --------------------- */
static void audamrwb_update_pcm_buf_entry(struct audio *audio,
		uint32_t *payload)
{
	uint8_t index;
	unsigned long flags;

	if (audio->rflush)
		return;

	spin_lock_irqsave(&audio->dsp_lock, flags);
	for (index = 0; index < payload[1]; index++) {
		if (audio->in[audio->fill_next].addr ==
		    payload[2 + index * 2]) {
			MM_DBG("audamrwb_update_pcm_buf_entry: \
				in[%d] ready\n", audio->fill_next);
			audio->in[audio->fill_next].used =
			    payload[3 + index * 2];
			if ((++audio->fill_next) == audio->pcm_buf_count)
				audio->fill_next = 0;

		} else {
			MM_ERR("expected=%x ret=%x\n",
				audio->in[audio->fill_next].addr,
				payload[1 + index * 2]);
			break;
		}
	}
	if (audio->in[audio->fill_next].used == 0) {
		audamrwb_buffer_refresh(audio);
	} else {
		MM_DBG("audamrwb_update_pcm_buf_entry: \
				read cannot keep up\n");
		audio->buf_refresh = 1;
	}
	wake_up(&audio->read_wait);
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
}

static void audplay_dsp_event(void *data, unsigned id, size_t len,
			      void (*getevent) (void *ptr, size_t len))
{
	struct audio *audio = data;
	uint32_t msg[28];
	getevent(msg, sizeof(msg));

	MM_DBG("audplay_dsp_event: msg_id=%x\n", id);

	switch (id) {
	case AUDPLAY_MSG_DEC_NEEDS_DATA:
		audamrwb_send_data(audio, 1);
		break;

	case AUDPLAY_MSG_BUFFER_UPDATE:
		audamrwb_update_pcm_buf_entry(audio, msg);
		break;

	case ADSP_MESSAGE_ID:
		MM_DBG("Received ADSP event:module audplaytask\n");
		break;

	default:
		MM_DBG("unexpected message from decoder\n");
	}
}

static void audamrwb_dsp_event(void *private, unsigned id, uint16_t *msg)
{
	struct audio *audio = private;

	switch (id) {
	case AUDPP_MSG_STATUS_MSG:{
			unsigned status = msg[1];

			switch (status) {
			case AUDPP_DEC_STATUS_SLEEP: {
				uint16_t reason = msg[2];
				MM_DBG("decoder status:sleep reason=0x%04x\n",
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
				if (audio->pcm_feedback)
					audpp_cmd_cfg_routing_mode(audio);
				else
					audpp_cmd_cfg_adec_params(audio);
				break;

			case AUDPP_DEC_STATUS_CFG:
				MM_DBG("decoder status: cfg\n");
				break;
			case AUDPP_DEC_STATUS_PLAY:
				MM_DBG("decoder status: play\n");
				/* send  mixer command */
				audpp_route_stream(audio->dec_id,
						audio->source);
				if (audio->pcm_feedback) {
					audamrwb_config_hostpcm(audio);
					audamrwb_buffer_refresh(audio);
				}
				audio->dec_state =
					MSM_AUD_DECODER_STATE_SUCCESS;
				wake_up(&audio->wait);
				break;
			default:
				MM_DBG("unknown decoder status\n");
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
			audpp_dsp_set_vol_pan(audio->dec_id, &audio->vol_pan,
					POPP);
			audpp_dsp_set_eq(audio->dec_id,	audio->eq_enable,
					&audio->eq, POPP);
		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			MM_DBG("CFG_MSG DISABLE\n");
			audio->running = 0;
		} else {
			MM_DBG("CFG_MSG %d?\n", msg[0]);
		}
		break;
	case AUDPP_MSG_ROUTING_ACK:
		MM_DBG("ROUTING_ACK mode=%d\n", msg[1]);
		audpp_cmd_cfg_adec_params(audio);
		break;
	case AUDPP_MSG_FLUSH_ACK:
		MM_DBG("FLUSH_ACK\n");
		audio->wflush = 0;
		audio->rflush = 0;
		wake_up(&audio->write_wait);
		if (audio->pcm_feedback)
			audamrwb_buffer_refresh(audio);
		break;
	case AUDPP_MSG_PCMDMAMISSED:
		MM_DBG("PCMDMAMISSED\n");
		audio->teos = 1;
		wake_up(&audio->write_wait);
		break;

	case AUDPP_MSG_AVSYNC_MSG:
		MM_DBG("AUDPP_MSG_AVSYNC_MSG\n");
		memcpy(&audio->avsync[0], msg, sizeof(audio->avsync));
		audio->avsync_flag = 1;
		wake_up(&audio->avsync_wait);
		break;

	default:
		MM_DBG("UNKNOWN (%d)\n", id);
	}

}

struct msm_adsp_ops audplay_adsp_ops_amrwb = {
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
				AUDPP_CMD_ENA_DEC_V | AUDDEC_DEC_AMRWB;
	else
		cfg_dec_cmd.dec_cfg = AUDPP_CMD_UPDATDE_CFG_DEC |
				AUDPP_CMD_DIS_DEC_V;
	cfg_dec_cmd.dm_mode = 0x0;
	cfg_dec_cmd.stream_id = audio->dec_id;
	return audpp_send_queue1(&cfg_dec_cmd, sizeof(cfg_dec_cmd));
}

static void audpp_cmd_cfg_adec_params(struct audio *audio)
{
	struct audpp_cmd_cfg_adec_params_amrwb cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_CMD_CFG_ADEC_PARAMS;
	cmd.common.length = AUDPP_CMD_CFG_ADEC_PARAMS_AMRWB_LEN;
	cmd.common.dec_id = audio->dec_id;
	cmd.common.input_sampling_frequency = audio->out_sample_rate;
	cmd.stereo_cfg = audio->out_channel_mode;
	audpp_send_queue2(&cmd, sizeof(cmd));
}

static void audpp_cmd_cfg_routing_mode(struct audio *audio)
{
	struct audpp_cmd_routing_mode cmd;
	MM_DBG("\n"); /* Macro prints the file name and function */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDPP_CMD_ROUTING_MODE;
	cmd.object_number = audio->dec_id;
	if (audio->pcm_feedback)
		cmd.routing_mode = ROUTING_MODE_FTRT;
	else
		cmd.routing_mode = ROUTING_MODE_RT;

	audpp_send_queue1(&cmd, sizeof(cmd));
}

static int audplay_dsp_send_data_avail(struct audio *audio,
				       unsigned idx, unsigned len)
{
	struct audplay_cmd_bitstream_data_avail_nt2 cmd;

	cmd.cmd_id = AUDPLAY_CMD_BITSTREAM_DATA_AVAIL_NT2;
	if (audio->mfield)
		cmd.decoder_id = AUDAMRWB_METAFIELD_MASK |
			(audio->out[idx].mfield_sz >> 1);
	else
		cmd.decoder_id = audio->dec_id;
	cmd.buf_ptr = audio->out[idx].addr;
	cmd.buf_size = len / 2;
	cmd.partition_number = 0;
	return audplay_send_queue0(audio, &cmd, sizeof(cmd));
}

static void audamrwb_buffer_refresh(struct audio *audio)
{
	struct audplay_cmd_buffer_refresh refresh_cmd;

	refresh_cmd.cmd_id = AUDPLAY_CMD_BUFFER_REFRESH;
	refresh_cmd.num_buffers = 1;
	refresh_cmd.buf0_address = audio->in[audio->fill_next].addr;
	refresh_cmd.buf0_length = audio->in[audio->fill_next].size;
	refresh_cmd.buf_read_count = 0;
	MM_DBG("buf0_addr=%x buf0_len=%d\n", refresh_cmd.buf0_address,
			refresh_cmd.buf0_length);
	(void)audplay_send_queue0(audio, &refresh_cmd, sizeof(refresh_cmd));
}

static void audamrwb_config_hostpcm(struct audio *audio)
{
	struct audplay_cmd_hpcm_buf_cfg cfg_cmd;

	MM_DBG("\n"); /* Macro prints the file name and function */
	cfg_cmd.cmd_id = AUDPLAY_CMD_HPCM_BUF_CFG;
	cfg_cmd.max_buffers = audio->pcm_buf_count;
	cfg_cmd.byte_swap = 0;
	cfg_cmd.hostpcm_config = (0x8000) | (0x4000);
	cfg_cmd.feedback_frequency = 1;
	cfg_cmd.partition_number = 0;
	(void)audplay_send_queue0(audio, &cfg_cmd, sizeof(cfg_cmd));

}

static void audamrwb_send_data(struct audio *audio, unsigned needed)
{
	struct buffer *frame;
	unsigned long flags;

	spin_lock_irqsave(&audio->dsp_lock, flags);
	if (!audio->running)
		goto done;

	if (needed && !audio->wflush) {
		/* We were called from the callback because the DSP
		 * requested more data.  Note that the DSP does want
		 * more data, and if a buffer was in-flight, mark it
		 * as available (since the DSP must now be done with
		 * it).
		 */
		audio->out_needed = 1;
		frame = audio->out + audio->out_tail;
		if (frame->used == 0xffffffff) {
			frame->used = 0;
			audio->out_tail ^= 1;
			wake_up(&audio->write_wait);
		}
	}

	if (audio->out_needed) {
		/* If the DSP currently wants data and we have a
		 * buffer available, we will send it and reset
		 * the needed flag.  We'll mark the buffer as in-flight
		 * so that it won't be recycled until the next buffer
		 * is requested
		 */

		frame = audio->out + audio->out_tail;
		if (frame->used) {
			BUG_ON(frame->used == 0xffffffff);
			MM_DBG("frame %d busy\n", audio->out_tail);
			audplay_dsp_send_data_avail(audio, audio->out_tail,
						    frame->used);
			frame->used = 0xffffffff;
			audio->out_needed = 0;
		}
	}
 done:
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
}

/* ------------------- device --------------------- */

static void audamrwb_flush(struct audio *audio)
{
	audio->out[0].used = 0;
	audio->out[1].used = 0;
	audio->out_head = 0;
	audio->out_tail = 0;
	audio->reserved = 0;
	audio->out_needed = 0;
	atomic_set(&audio->out_bytes, 0);
}

static void audamrwb_flush_pcm_buf(struct audio *audio)
{
	uint8_t index;

	for (index = 0; index < PCM_BUF_MAX_COUNT; index++)
		audio->in[index].used = 0;

	audio->buf_refresh = 0;
	audio->read_next = 0;
	audio->fill_next = 0;
}

static void audamrwb_ioport_reset(struct audio *audio)
{
	/* Make sure read/write thread are free from
	 * sleep and knowing that system is not able
	 * to process io request at the moment
	 */
	wake_up(&audio->write_wait);
	mutex_lock(&audio->write_lock);
	audamrwb_flush(audio);
	mutex_unlock(&audio->write_lock);
	wake_up(&audio->read_wait);
	mutex_lock(&audio->read_lock);
	audamrwb_flush_pcm_buf(audio);
	mutex_unlock(&audio->read_lock);
	audio->avsync_flag = 1;
	wake_up(&audio->avsync_wait);
}

static int audamrwb_events_pending(struct audio *audio)
{
	unsigned long flags;
	int empty;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	empty = !list_empty(&audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	return empty || audio->event_abort;
}

static void audamrwb_reset_event_queue(struct audio *audio)
{
	unsigned long flags;
	struct audamrwb_event *drv_evt;
	struct list_head *ptr, *next;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	list_for_each_safe(ptr, next, &audio->event_queue) {
		drv_evt = list_first_entry(&audio->event_queue,
				struct audamrwb_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	list_for_each_safe(ptr, next, &audio->free_event_queue) {
		drv_evt = list_first_entry(&audio->free_event_queue,
				struct audamrwb_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	return;
}

static long audamrwb_process_event_req(struct audio *audio, void __user *arg)
{
	long rc;
	struct msm_audio_event usr_evt;
	struct audamrwb_event *drv_evt = NULL;
	int timeout;
	unsigned long flags;

	if (copy_from_user(&usr_evt, arg, sizeof(struct msm_audio_event)))
		return -EFAULT;

	timeout = (int) usr_evt.timeout_ms;

	if (timeout > 0) {
		rc = wait_event_interruptible_timeout(
			audio->event_wait, audamrwb_events_pending(audio),
			msecs_to_jiffies(timeout));
		if (rc == 0)
			return -ETIMEDOUT;
	} else {
		rc = wait_event_interruptible(
			audio->event_wait, audamrwb_events_pending(audio));
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
				struct audamrwb_event, list);
		list_del(&drv_evt->list);
	}

	if (drv_evt) {
		usr_evt.event_type = drv_evt->event_type;
		usr_evt.event_payload = drv_evt->payload;
		list_add_tail(&drv_evt->list, &audio->free_event_queue);
	} else
		rc = -1;
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	if (!rc && copy_to_user(arg, &usr_evt, sizeof(usr_evt)))
		rc = -EFAULT;

	return rc;
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

static long audamrwb_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = -EINVAL;
	unsigned long flags = 0;
	uint16_t enable_mask;
	int enable;
	int prev_state;

	MM_DBG("cmd = %d\n", cmd);

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;

		audio->avsync_flag = 0;
		memset(&stats, 0, sizeof(stats));
		if (audpp_query_avsync(audio->dec_id) < 0)
			return rc;

		rc = wait_event_interruptible_timeout(audio->avsync_wait,
				(audio->avsync_flag == 1),
				msecs_to_jiffies(AUDPP_AVSYNC_EVENT_TIMEOUT));

		if (rc < 0)
			return rc;
		else if ((rc > 0) || ((rc == 0) && (audio->avsync_flag == 1))) {
			if (audio_get_avsync_data(audio, &stats) < 0)
				return rc;

			if (copy_to_user((void *)arg, &stats, sizeof(stats)))
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
			audpp_dsp_set_vol_pan(audio->dec_id, &audio->vol_pan,
					POPP);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		rc = 0;
		break;

	case AUDIO_SET_PAN:
		spin_lock_irqsave(&audio->dsp_lock, flags);
		audio->vol_pan.pan = arg;
		if (audio->running)
			audpp_dsp_set_vol_pan(audio->dec_id, &audio->vol_pan,
					POPP);
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
		MM_DBG("AUDIO_GET_EVENT\n");
		if (mutex_trylock(&audio->get_event_lock)) {
			rc = audamrwb_process_event_req(audio,
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
		rc = audamrwb_enable(audio);
		if (!rc) {
			rc = wait_event_interruptible_timeout(audio->wait,
				audio->dec_state != MSM_AUD_DECODER_STATE_NONE,
				msecs_to_jiffies(MSM_AUD_DECODER_WAIT_MS));
			MM_INFO("dec_state %d rc = %d\n", audio->dec_state, rc);

			if (audio->dec_state != MSM_AUD_DECODER_STATE_SUCCESS)
				rc = -ENODEV;
			else
				rc = 0;
		}
		break;
	case AUDIO_STOP:
		MM_DBG("AUDIO_STOP\n");
		rc = audamrwb_disable(audio);
		audio->stopped = 1;
		audamrwb_ioport_reset(audio);
		audio->stopped = 0;
		break;
	case AUDIO_FLUSH:
		MM_DBG("AUDIO_FLUSH\n");
		audio->rflush = 1;
		audio->wflush = 1;
		audamrwb_ioport_reset(audio);
		if (audio->running) {
			audpp_flush(audio->dec_id);
			rc = wait_event_interruptible(audio->write_wait,
				!audio->wflush);
			if (rc < 0) {
				MM_ERR("AUDIO_FLUSH interrupted\n");
				rc = -EINTR;
			}
		} else {
			audio->rflush = 0;
			audio->wflush = 0;
		}
		break;
	case AUDIO_SET_CONFIG:{
			struct msm_audio_config config;
			if (copy_from_user
			    (&config, (void *)arg, sizeof(config))) {
				rc = -EFAULT;
				break;
			}
			if (config.channel_count == 1)
				config.channel_count =
					AUDPP_CMD_PCM_INTF_MONO_V;
			else if (config.channel_count == 2)
				config.channel_count =
					AUDPP_CMD_PCM_INTF_STEREO_V;
			else
				rc = -EINVAL;
			audio->out_channel_mode = config.channel_count;
			audio->out_sample_rate = config.sample_rate;
			audio->mfield = config.meta_field;
			rc = 0;
			break;
		}
	case AUDIO_GET_CONFIG:{
			struct msm_audio_config config;
			config.buffer_size = BUFSZ;
			config.buffer_count = 2;
			config.sample_rate = audio->out_sample_rate;
			if (audio->out_channel_mode ==
					AUDPP_CMD_PCM_INTF_MONO_V)
				config.channel_count = 1;
			else
				config.channel_count = 2;
			config.meta_field = 0;
			config.unused[0] = 0;
			config.unused[1] = 0;
			config.unused[2] = 0;
			if (copy_to_user((void *)arg, &config,
					 sizeof(config)))
				rc = -EFAULT;
			else
				rc = 0;

			break;
		}
	case AUDIO_GET_PCM_CONFIG:{
			struct msm_audio_pcm_config config;
			config.pcm_feedback = 0;
			config.buffer_count = PCM_BUF_MAX_COUNT;
			config.buffer_size = PCM_BUFSZ_MIN;
			if (copy_to_user((void *)arg, &config,
					 sizeof(config)))
				rc = -EFAULT;
			else
				rc = 0;
			break;
		}
	case AUDIO_SET_PCM_CONFIG:{
		struct msm_audio_pcm_config config;
		if (copy_from_user
		    (&config, (void *)arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		if ((config.buffer_count > PCM_BUF_MAX_COUNT) ||
		    (config.buffer_count == 1))
			config.buffer_count = PCM_BUF_MAX_COUNT;

		if (config.buffer_size < PCM_BUFSZ_MIN)
			config.buffer_size = PCM_BUFSZ_MIN;

			/* Check if pcm feedback is required */
		if ((config.pcm_feedback) && (!audio->read_data)) {
			MM_DBG("allocate PCM buf %d\n", config.buffer_count *
					config.buffer_size);
			audio->read_phys = allocate_contiguous_ebi_nomap(
						config.buffer_size *
						config.buffer_count,
						SZ_4K);
			if (!audio->read_phys) {
					rc = -ENOMEM;
					break;
			}
			audio->map_v_read = msm_subsystem_map_buffer(
						audio->read_phys,
						config.buffer_size *
						config.buffer_count,
						MSM_SUBSYSTEM_MAP_KADDR,
						NULL, 0);
			if (IS_ERR(audio->map_v_read)) {
				MM_ERR("Error could not map read"
							" phys address\n");
				rc = -ENOMEM;
				free_contiguous_memory_by_paddr(
							audio->read_phys);
			} else {
				uint8_t index;
				uint32_t offset = 0;
				audio->read_data = audio->map_v_read->vaddr;
				audio->pcm_feedback = 1;
				audio->buf_refresh = 0;
				audio->pcm_buf_count =
					config.buffer_count;
				audio->read_next = 0;
				audio->fill_next = 0;

				for (index = 0;
				index < config.buffer_count; index++) {
					audio->in[index].data =
						audio->read_data + offset;
					audio->in[index].addr =
					    audio->read_phys + offset;
					audio->in[index].size =
					    config.buffer_size;
					audio->in[index].used = 0;
					offset += config.buffer_size;
				}
				MM_DBG("read buf: phy addr 0x%08x \
						kernel addr 0x%08x\n",
						audio->read_phys,
						(int)audio->read_data);
				rc = 0;
			}
		} else {
			rc = 0;
		}
		break;
	}
	case AUDIO_GET_SESSION_ID:
		if (copy_to_user((void *) arg, &audio->dec_id,
					sizeof(unsigned short)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

/* Only useful in tunnel-mode */
static int audamrwb_fsync(struct file *file, int datasync)
{
	struct audio *audio = file->private_data;
	struct buffer *frame;
	int rc = 0;

	MM_DBG("\n"); /* Macro prints the file name and function */

	if (!audio->running || audio->pcm_feedback) {
		rc = -EINVAL;
		goto done_nolock;
	}

	mutex_lock(&audio->write_lock);

	rc = wait_event_interruptible(audio->write_wait,
		(!audio->out[0].used &&
		!audio->out[1].used &&
		audio->out_needed) || audio->wflush);

	if (rc < 0)
		goto done;
	else if (audio->wflush) {
		rc = -EBUSY;
		goto done;
	}

	if (audio->reserved) {
		MM_DBG("send reserved byte\n");
		frame = audio->out + audio->out_tail;
		((char *) frame->data)[0] = audio->rsv_byte;
		((char *) frame->data)[1] = 0;
		frame->used = 2;
		audamrwb_send_data(audio, 0);

		rc = wait_event_interruptible(audio->write_wait,
			(!audio->out[0].used &&
			!audio->out[1].used &&
			audio->out_needed) || audio->wflush);

		if (rc < 0)
			goto done;
		else if (audio->wflush) {
			rc = -EBUSY;
			goto done;
		}
	}

	/* pcm dmamiss message is sent continously
	 * when decoder is starved so no race
	 * condition concern
	 */
	audio->teos = 0;

	rc = wait_event_interruptible(audio->write_wait,
		audio->teos || audio->wflush);

	if (audio->wflush)
		rc = -EBUSY;

done:
	mutex_unlock(&audio->write_lock);
done_nolock:
	return rc;
}

static ssize_t audamrwb_read(struct file *file, char __user *buf, size_t count,
			  loff_t *pos)
{
	struct audio *audio = file->private_data;
	const char __user *start = buf;
	int rc = 0;

	if (!audio->pcm_feedback)
		return 0; /* PCM feedback is not enabled. Nothing to read */

	mutex_lock(&audio->read_lock);
	MM_DBG("count %d\n", count);
	while (count > 0) {
		rc = wait_event_interruptible(audio->read_wait,
			(audio->in[audio->read_next].used > 0) ||
			(audio->stopped) || (audio->rflush));

		if (rc < 0)
			break;

		if (audio->stopped || audio->rflush) {
			rc = -EBUSY;
			break;
		}

		if (count < audio->in[audio->read_next].used) {
			/* Read must happen in frame boundary. Since driver does
			 * not know frame size, read count must be greater or
			 * equal to size of PCM samples
			 */
			MM_DBG("read stop - partial frame\n");
			break;
		} else {
			MM_DBG("read from in[%d]\n", audio->read_next);

			if (copy_to_user
			    (buf, audio->in[audio->read_next].data,
			     audio->in[audio->read_next].used)) {
				MM_ERR("invalid addr %x\n", (unsigned int)buf);
				rc = -EFAULT;
				break;
			}
			count -= audio->in[audio->read_next].used;
			buf += audio->in[audio->read_next].used;
			audio->in[audio->read_next].used = 0;
			if ((++audio->read_next) == audio->pcm_buf_count)
				audio->read_next = 0;
			break;
		}
	}

	/* don't feed output buffer to HW decoder during flushing
	 * buffer refresh command will be sent once flush completes
	 * send buf refresh command here can confuse HW decoder
	 */
	if (audio->buf_refresh && !audio->rflush) {
		audio->buf_refresh = 0;
		MM_DBG("kick start pcm feedback again\n");
		audamrwb_buffer_refresh(audio);
	}

	mutex_unlock(&audio->read_lock);

	if (buf > start)
		rc = buf - start;

	MM_DBG("read %d bytes\n", rc);
	return rc;
}

static int audamrwb_process_eos(struct audio *audio,
		const char __user *buf_start, unsigned short mfield_size)
{
	struct buffer *frame;
	char *buf_ptr;
	int rc = 0;

	MM_DBG("signal input EOS reserved=%d\n", audio->reserved);
	if (audio->reserved) {
		MM_DBG("Pass reserve byte\n");
		frame = audio->out + audio->out_head;
		buf_ptr = frame->data;
		rc = wait_event_interruptible(audio->write_wait,
					(frame->used == 0)
					|| (audio->stopped)
					|| (audio->wflush));
	if (rc < 0)
		goto done;
	if (audio->stopped || audio->wflush) {
		rc = -EBUSY;
		goto done;
	}
	buf_ptr[0] = audio->rsv_byte;
	buf_ptr[1] = 0;
	audio->out_head ^= 1;
	frame->mfield_sz = 0;
	audio->reserved = 0;
	frame->used = 2;
	audamrwb_send_data(audio, 0);
	}

	MM_DBG("Now signal input EOS after reserved bytes %d %d %d\n",
		audio->out[0].used, audio->out[1].used, audio->out_needed);

	frame = audio->out + audio->out_head;

	rc = wait_event_interruptible(audio->write_wait,
		(audio->out_needed &&
		audio->out[0].used == 0 &&
		audio->out[1].used == 0)
		|| (audio->stopped)
		|| (audio->wflush));

	if (rc < 0)
		goto done;
	if (audio->stopped || audio->wflush) {
		rc = -EBUSY;
		goto done;
	}

	if (copy_from_user(frame->data, buf_start, mfield_size)) {
		rc = -EFAULT;
		goto done;
	}

	frame->mfield_sz = mfield_size;
	audio->out_head ^= 1;
	frame->used = mfield_size;
	audamrwb_send_data(audio, 0);

done:
	return rc;
}

static ssize_t audamrwb_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *pos)
{
	struct audio *audio = file->private_data;
	const char __user *start = buf;
	struct buffer *frame;
	size_t xfer;
	char *cpy_ptr;
	int rc = 0, eos_condition = AUDAMRWB_EOS_NONE;
	unsigned short mfield_size = 0;
	unsigned dsize;

	MM_DBG("cnt=%d\n", count);

	mutex_lock(&audio->write_lock);
	while (count > 0) {
		frame = audio->out + audio->out_head;
		cpy_ptr = frame->data;
		dsize = 0;
		rc = wait_event_interruptible(audio->write_wait,
					      (frame->used == 0)
						|| (audio->stopped)
						|| (audio->wflush));

		MM_DBG("buffer available\n");
		if (rc < 0)
			break;
		if (audio->stopped || audio->wflush) {
			rc = -EBUSY;
			break;
		}

		if (audio->mfield) {
			if (buf == start) {
				/* Processing beginning of user buffer */
				if (__get_user(mfield_size,
					(unsigned short __user *) buf)) {
					rc = -EFAULT;
					break;
				} else 	if (mfield_size > count) {
					rc = -EINVAL;
					break;
				}
				MM_DBG("mf offset_val %x\n", mfield_size);
				if (copy_from_user(cpy_ptr, buf, mfield_size)) {
					rc = -EFAULT;
					break;
				}
				/* Check if EOS flag is set and buffer
				 * contains just meta field
				 */
				if (cpy_ptr[AUDAMRWB_EOS_FLG_OFFSET] &
						AUDAMRWB_EOS_FLG_MASK) {
					MM_DBG("eos set\n");
					eos_condition = AUDAMRWB_EOS_SET;
					if (mfield_size == count) {
						buf += mfield_size;
						break;
					} else
					cpy_ptr[AUDAMRWB_EOS_FLG_OFFSET] &=
							~AUDAMRWB_EOS_FLG_MASK;
				}
				cpy_ptr += mfield_size;
				count -= mfield_size;
				dsize += mfield_size;
				buf += mfield_size;
			} else {
				mfield_size = 0;
				MM_DBG("continuous buffer\n");
			}
			frame->mfield_sz = mfield_size;
		}

		if (audio->reserved) {
			MM_DBG("append reserved byte %x\n", audio->rsv_byte);
			*cpy_ptr = audio->rsv_byte;
			xfer = (count > ((frame->size - mfield_size) - 1)) ?
				((frame->size - mfield_size) - 1) : count;
			cpy_ptr++;
			dsize += 1;
			audio->reserved = 0;
		} else
			xfer = (count > (frame->size - mfield_size)) ?
				(frame->size - mfield_size) : count;

		if (copy_from_user(cpy_ptr, buf, xfer)) {
			rc = -EFAULT;
			break;
		}

		dsize += xfer;
		if (dsize & 1) {
			audio->rsv_byte = ((char *) frame->data)[dsize - 1];
			MM_DBG("odd length buf reserve last byte %x\n",
					audio->rsv_byte);
			audio->reserved = 1;
			dsize--;
		}
		count -= xfer;
		buf += xfer;

		if (dsize > 0) {
			audio->out_head ^= 1;
			frame->used = dsize;
			audamrwb_send_data(audio, 0);
		}
	}
	MM_DBG("eos_condition %x buf[0x%x] start[0x%x]\n", eos_condition,
			(int) buf, (int) start);
	if (eos_condition == AUDAMRWB_EOS_SET)
		rc = audamrwb_process_eos(audio, start, mfield_size);
	mutex_unlock(&audio->write_lock);
	if (!rc) {
		if (buf > start)
			return buf - start;
	}
	return rc;
}

static int audamrwb_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	MM_INFO("audio instance 0x%08x freeing\n", (int)audio);

	mutex_lock(&audio->lock);
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, audio->dec_id);
	audamrwb_disable(audio);
	audamrwb_flush(audio);
	audamrwb_flush_pcm_buf(audio);
	msm_adsp_put(audio->audplay);
	audpp_adec_free(audio->dec_id);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&audio->suspend_ctl.node);
#endif
	audio->event_abort = 1;
	wake_up(&audio->event_wait);
	audamrwb_reset_event_queue(audio);
	msm_subsystem_unmap_buffer(audio->map_v_write);
	free_contiguous_memory_by_paddr(audio->phys);
	if (audio->read_data) {
		msm_subsystem_unmap_buffer(audio->map_v_read);
		free_contiguous_memory_by_paddr(audio->read_phys);
	}
	mutex_unlock(&audio->lock);
#ifdef CONFIG_DEBUG_FS
	if (audio->dentry)
		debugfs_remove(audio->dentry);
#endif
	kfree(audio);
	return 0;
}

static void audamrwb_post_event(struct audio *audio, int type,
		union msm_audio_event_payload payload)
{
	struct audamrwb_event *e_node = NULL;
	unsigned long flags;

	spin_lock_irqsave(&audio->event_queue_lock, flags);

	if (!list_empty(&audio->free_event_queue)) {
		e_node = list_first_entry(&audio->free_event_queue,
				struct audamrwb_event, list);
		list_del(&e_node->list);
	} else {
		e_node = kmalloc(sizeof(struct audamrwb_event), GFP_ATOMIC);
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
static void audamrwb_suspend(struct early_suspend *h)
{
	struct audamrwb_suspend_ctl *ctl =
		container_of(h, struct audamrwb_suspend_ctl, node);
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	audamrwb_post_event(ctl->audio, AUDIO_EVENT_SUSPEND, payload);
}

static void audamrwb_resume(struct early_suspend *h)
{
	struct audamrwb_suspend_ctl *ctl =
		container_of(h, struct audamrwb_suspend_ctl, node);
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	audamrwb_post_event(ctl->audio, AUDIO_EVENT_RESUME, payload);
}
#endif

#ifdef CONFIG_DEBUG_FS
static ssize_t audamrwb_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t audamrwb_debug_read(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	const int debug_bufmax = 1024;
	static char buffer[1024];
	int n = 0, i;
	struct audio *audio = file->private_data;

	mutex_lock(&audio->lock);
	n = scnprintf(buffer, debug_bufmax, "opened %d\n", audio->opened);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"enabled %d\n", audio->enabled);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"stopped %d\n", audio->stopped);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"pcm_feedback %d\n", audio->pcm_feedback);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"out_buf_sz %d\n", audio->out[0].size);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"pcm_buf_count %d \n", audio->pcm_buf_count);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"pcm_buf_sz %d \n", audio->in[0].size);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"volume %x \n", audio->vol_pan.volume);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"sample rate %d \n", audio->out_sample_rate);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"channel mode %d \n", audio->out_channel_mode);
	mutex_unlock(&audio->lock);
	/* Following variables are only useful for debugging when
	 * when playback halts unexpectedly. Thus, no mutual exclusion
	 * enforced
	 */
	n += scnprintf(buffer + n, debug_bufmax - n,
			"wflush %d\n", audio->wflush);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"rflush %d\n", audio->rflush);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"running %d \n", audio->running);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"dec state %d \n", audio->dec_state);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"out_needed %d \n", audio->out_needed);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"out_head %d \n", audio->out_head);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"out_tail %d \n", audio->out_tail);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"out[0].used %d \n", audio->out[0].used);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"out[1].used %d \n", audio->out[1].used);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"buffer_refresh %d \n", audio->buf_refresh);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"read_next %d \n", audio->read_next);
	n += scnprintf(buffer + n, debug_bufmax - n,
			"fill_next %d \n", audio->fill_next);
	for (i = 0; i < audio->pcm_buf_count; i++)
		n += scnprintf(buffer + n, debug_bufmax - n,
				"in[%d].used %d \n", i, audio->in[i].used);
	buffer[n] = 0;
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static const struct file_operations audamrwb_debug_fops = {
	.read = audamrwb_debug_read,
	.open = audamrwb_debug_open,
};
#endif

static int audamrwb_open(struct inode *inode, struct file *file)
{
	struct audio *audio = NULL;
	int rc, dec_attrb, decid, i;
	struct audamrwb_event *e_node = NULL;
#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_amrwb_" + 5];
#endif

	/* Allocate Mem for audio instance */
	audio = kzalloc(sizeof(struct audio), GFP_KERNEL);
	if (!audio) {
		MM_ERR("no memory to allocate audio instance\n");
		rc = -ENOMEM;
		goto done;
	}
	MM_INFO("audio instance 0x%08x created\n", (int)audio);

	/* Allocate the decoder */
	dec_attrb = AUDDEC_DEC_AMRWB;
	if (file->f_mode & FMODE_READ)
		dec_attrb |= MSM_AUD_MODE_NONTUNNEL;
	else
		dec_attrb |= MSM_AUD_MODE_TUNNEL;

	decid = audpp_adec_alloc(dec_attrb, &audio->module_name,
			&audio->queue_id);

	if (decid < 0) {
		MM_ERR("No free decoder available, freeing instance 0x%08x\n",
				(int)audio);
		rc = -ENODEV;
		kfree(audio);
		goto done;
	}

	audio->dec_id = decid & MSM_AUD_DECODER_MASK;

	audio->phys = allocate_contiguous_ebi_nomap(DMASZ, SZ_4K);
	if (!audio->phys) {
		MM_ERR("could not allocate write buffers, freeing instance \
				0x%08x\n", (int)audio);
		rc = -ENOMEM;
		audpp_adec_free(audio->dec_id);
		kfree(audio);
		goto done;
	} else {
		audio->map_v_write = msm_subsystem_map_buffer(
					audio->phys, DMASZ,
					MSM_SUBSYSTEM_MAP_KADDR, NULL, 0);
		if (IS_ERR(audio->map_v_write)) {
			MM_ERR("could not map write phys buffers, freeing \
					instance 0x%08x\n", (int)audio);
			rc = -ENOMEM;
			free_contiguous_memory_by_paddr(audio->phys);
			audpp_adec_free(audio->dec_id);
			kfree(audio);
			goto done;
		}
		audio->data = audio->map_v_write->vaddr;
		MM_DBG("write buf: phy addr 0x%08x kernel addr 0x%08x\n",
				audio->phys, (int)audio->data);
	}

	rc = msm_adsp_get(audio->module_name, &audio->audplay,
		&audplay_adsp_ops_amrwb, audio);
	if (rc) {
		MM_ERR("failed to get %s module freeing instance 0x%08x\n",
				audio->module_name, (int)audio);
		goto err;
	}

	mutex_init(&audio->lock);
	mutex_init(&audio->write_lock);
	mutex_init(&audio->read_lock);
	mutex_init(&audio->get_event_lock);
	spin_lock_init(&audio->dsp_lock);
	spin_lock_init(&audio->event_queue_lock);
	INIT_LIST_HEAD(&audio->free_event_queue);
	INIT_LIST_HEAD(&audio->event_queue);
	init_waitqueue_head(&audio->write_wait);
	init_waitqueue_head(&audio->read_wait);
	init_waitqueue_head(&audio->wait);
	init_waitqueue_head(&audio->event_wait);
	init_waitqueue_head(&audio->avsync_wait);

	audio->out[0].data = audio->data + 0;
	audio->out[0].addr = audio->phys + 0;
	audio->out[0].size = BUFSZ;

	audio->out[1].data = audio->data + BUFSZ;
	audio->out[1].addr = audio->phys + BUFSZ;
	audio->out[1].size = BUFSZ;

	audio->vol_pan.volume = 0x2000;
	audio->vol_pan.pan = 0x0;
	audio->eq_enable = 0;
	audio->out_sample_rate = 44100;
	audio->out_channel_mode = AUDPP_CMD_PCM_INTF_STEREO_V;

	audamrwb_flush(audio);

	file->private_data = audio;
	audio->opened = 1;
	audio->event_abort = 0;
	audio->device_events = AUDDEV_EVT_DEV_RDY
				|AUDDEV_EVT_DEV_RLS|
				AUDDEV_EVT_STREAM_VOL_CHG;

	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_DEC,
					audio->dec_id,
					amrwb_listner,
					(void *)audio);
	if (rc) {
		MM_ERR("failed to register listner\n");
		goto event_err;
	}

#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof name, "msm_amrwb_%04x", audio->dec_id);
	audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
			NULL, (void *) audio, &audamrwb_debug_fops);

	if (IS_ERR(audio->dentry))
		MM_DBG("debugfs_create_file failed\n");
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	audio->suspend_ctl.node.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	audio->suspend_ctl.node.resume = audamrwb_resume;
	audio->suspend_ctl.node.suspend = audamrwb_suspend;
	audio->suspend_ctl.audio = audio;
	register_early_suspend(&audio->suspend_ctl.node);
#endif
	for (i = 0; i < AUDAMRWB_EVENT_NUM; i++) {
		e_node = kmalloc(sizeof(struct audamrwb_event), GFP_KERNEL);
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
	msm_subsystem_unmap_buffer(audio->map_v_write);
	free_contiguous_memory_by_paddr(audio->phys);
	audpp_adec_free(audio->dec_id);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_amrwb_fops = {
	.owner = THIS_MODULE,
	.open = audamrwb_open,
	.release = audamrwb_release,
	.read = audamrwb_read,
	.write = audamrwb_write,
	.unlocked_ioctl = audamrwb_ioctl,
	.fsync = audamrwb_fsync,
};

struct miscdevice audio_amrwb_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_amrwb",
	.fops = &audio_amrwb_fops,
};

static int __init audamrwb_init(void)
{
	return misc_register(&audio_amrwb_misc);
}

static void __exit audamrwb_exit(void)
{
	misc_deregister(&audio_amrwb_misc);
}

module_init(audamrwb_init);
module_exit(audamrwb_exit);

MODULE_DESCRIPTION("MSM AMR-WB driver");
MODULE_LICENSE("GPL v2");

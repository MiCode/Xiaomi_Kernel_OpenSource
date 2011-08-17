/*
 * aac audio decoder device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
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
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <linux/slab.h>
#include <mach/msm_adsp.h>
#include <linux/msm_audio_aac.h>
#include <mach/qdsp5v2/qdsp5audppmsg.h>
#include <mach/qdsp5v2/qdsp5audplaycmdi.h>
#include <mach/qdsp5v2/qdsp5audplaymsg.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/qdsp5v2/audpp.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/debug_mm.h>

#define BUFSZ 32768
#define DMASZ (BUFSZ * 2)
#define BUFSZ_MIN 4096
#define DMASZ_MIN (BUFSZ_MIN * 2)

#define AUDPLAY_INVALID_READ_PTR_OFFSET	0xFFFF
#define AUDDEC_DEC_AAC 5

#define PCM_BUFSZ_MIN 9600	/* Hold one stereo AAC frame */
#define PCM_BUF_MAX_COUNT 5	/* DSP only accepts 5 buffers at most
				   but support 2 buffers currently */
#define ROUTING_MODE_FTRT 1
#define ROUTING_MODE_RT 2
/* Decoder status received from AUDPPTASK */
#define  AUDPP_DEC_STATUS_SLEEP	0
#define	 AUDPP_DEC_STATUS_INIT  1
#define  AUDPP_DEC_STATUS_CFG   2
#define  AUDPP_DEC_STATUS_PLAY  3

#define AUDAAC_METAFIELD_MASK 0xFFFF0000
#define AUDAAC_EOS_FLG_OFFSET 0x0A /* Offset from beginning of buffer */
#define AUDAAC_EOS_FLG_MASK 0x01
#define AUDAAC_EOS_NONE 0x0 /* No EOS detected */
#define AUDAAC_EOS_SET 0x1 /* EOS set in meta field */

#define AUDAAC_EVENT_NUM 10 /* Default number of pre-allocated event packets */

#define BITSTREAM_ERROR_THRESHOLD_VALUE 0x1 /* DEFAULT THRESHOLD VALUE */

struct buffer {
	void *data;
	unsigned size;
	unsigned used;		/* Input usage actual DSP produced PCM size  */
	unsigned addr;
	unsigned short mfield_sz; /*only useful for data has meta field */
};

#ifdef CONFIG_HAS_EARLYSUSPEND
struct audaac_suspend_ctl {
	struct early_suspend node;
	struct audio *audio;
};
#endif

struct audaac_event{
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
	unsigned out_dma_sz;

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
	struct msm_audio_aac_config aac_config;

	/* AV sync Info */
	int avsync_flag;              /* Flag to indicate feedback from DSP */
	wait_queue_head_t avsync_wait;/* Wait queue for AV Sync Message     */
	/* 48 bits sample/bytes counter per channel */
	uint16_t avsync[AUDPP_AVSYNC_CH_COUNT * AUDPP_AVSYNC_NUM_WORDS + 1];

	/* data allocated for various buffers */
	char *data;
	int32_t phys; /* physical address of write buffer */

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
	struct audaac_suspend_ctl suspend_ctl;
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
	uint32_t device_events;

	struct msm_audio_bitstream_info stream_info;
	struct msm_audio_bitstream_error_info bitstream_error_info;
	uint32_t bitstream_error_threshold_value;

	int eq_enable;
	int eq_needs_commit;
	struct audpp_cmd_cfg_object_params_eqalizer eq;
	struct audpp_cmd_cfg_object_params_volume vol_pan;
};

static int auddec_dsp_config(struct audio *audio, int enable);
static void audpp_cmd_cfg_adec_params(struct audio *audio);
static void audpp_cmd_cfg_routing_mode(struct audio *audio);
static void audplay_send_data(struct audio *audio, unsigned needed);
static void audplay_error_threshold_config(struct audio *audio);
static void audplay_config_hostpcm(struct audio *audio);
static void audplay_buffer_refresh(struct audio *audio);
static void audio_dsp_event(void *private, unsigned id, uint16_t *msg);
static void audaac_post_event(struct audio *audio, int type,
		union msm_audio_event_payload payload);

/* must be called with audio->lock held */
static int audio_enable(struct audio *audio)
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

	if (audpp_enable(audio->dec_id, audio_dsp_event, audio)) {
		MM_ERR("audpp_enable() failed\n");
		msm_adsp_disable(audio->audplay);
		return -ENODEV;
	}
	audio->enabled = 1;
	return 0;
}

static void aac_listner(u32 evt_id, union auddev_evt_data *evt_payload,
			void *private_data)
{
	struct audio *audio = (struct audio *) private_data;
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		MM_DBG(":AUDDEV_EVT_DEV_RDY\n");
		audio->source |= (0x1 << evt_payload->routing_id);
		if (audio->running == 1 && audio->enabled == 1)
			audpp_route_stream(audio->dec_id, audio->source);
		break;
	case AUDDEV_EVT_DEV_RLS:
		MM_DBG(":AUDDEV_EVT_DEV_RLS\n");
		audio->source &= ~(0x1 << evt_payload->routing_id);
		if (audio->running == 1 && audio->enabled == 1)
			audpp_route_stream(audio->dec_id, audio->source);
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		audio->vol_pan.volume = evt_payload->session_vol;
		MM_DBG(":AUDDEV_EVT_STREAM_VOL_CHG, stream vol %d\n",
				audio->vol_pan.volume);
		if (audio->running)
			audpp_dsp_set_vol_pan(audio->dec_id, &audio->vol_pan,
					POPP);
		break;
	default:
		MM_ERR(":ERROR:wrong event\n");
		break;
	}
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
		wake_up(&audio->read_wait);
		msm_adsp_disable(audio->audplay);
		audpp_disable(audio->dec_id, audio);
		audio->out_needed = 0;
	}
	return rc;
}

/* ------------------- dsp --------------------- */
static void audio_update_pcm_buf_entry(struct audio *audio, uint32_t *payload)
{
	uint8_t index;
	unsigned long flags;

	if (audio->rflush)
		return;

	spin_lock_irqsave(&audio->dsp_lock, flags);
	for (index = 0; index < payload[1]; index++) {
		if (audio->in[audio->fill_next].addr ==
		    payload[2 + index * 2]) {
			MM_DBG("in[%d] ready\n", audio->fill_next);
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
		audplay_buffer_refresh(audio);
	} else {
		MM_DBG("read cannot keep up\n");
		audio->buf_refresh = 1;
	}
	wake_up(&audio->read_wait);
	spin_unlock_irqrestore(&audio->dsp_lock, flags);

}

static void audaac_bitstream_error_info(struct audio *audio, uint32_t *payload)
{
	unsigned long flags;
	union msm_audio_event_payload e_payload;

	if (payload[0] != AUDDEC_DEC_AAC) {
		MM_ERR("Unexpected bitstream error info from DSP:\
				Invalid decoder\n");
		return;
	}

	/* get stream info from DSP msg */
	spin_lock_irqsave(&audio->dsp_lock, flags);

	audio->bitstream_error_info.dec_id = payload[0];
	audio->bitstream_error_info.err_msg_indicator = payload[1];
	audio->bitstream_error_info.err_type = payload[2];

	spin_unlock_irqrestore(&audio->dsp_lock, flags);
	MM_ERR("bit_stream_error_type=%d error_count=%d\n",
			audio->bitstream_error_info.err_type, (0x0000FFFF &
			audio->bitstream_error_info.err_msg_indicator));

	/* send event to ARM to notify error info coming */
	e_payload.error_info = audio->bitstream_error_info;
	audaac_post_event(audio, AUDIO_EVENT_BITSTREAM_ERROR_INFO, e_payload);
}

static void audaac_update_stream_info(struct audio *audio, uint32_t *payload)
{
	unsigned long flags;
	union msm_audio_event_payload e_payload;

	/* get stream info from DSP msg */
	spin_lock_irqsave(&audio->dsp_lock, flags);

	audio->stream_info.codec_type = AUDIO_CODEC_TYPE_AAC;
	audio->stream_info.chan_info = (0x0000FFFF & payload[1]);
	audio->stream_info.sample_rate = (0x0000FFFF & payload[2]);
	audio->stream_info.bit_stream_info = (0x0000FFFF & payload[3]);
	audio->stream_info.bit_rate = payload[4];

	spin_unlock_irqrestore(&audio->dsp_lock, flags);
	MM_DBG("chan_info=%d, sample_rate=%d, bit_stream_info=%d\n",
			audio->stream_info.chan_info,
			audio->stream_info.sample_rate,
			audio->stream_info.bit_stream_info);

	/* send event to ARM to notify steam info coming */
	e_payload.stream_info = audio->stream_info;
	audaac_post_event(audio, AUDIO_EVENT_STREAM_INFO, e_payload);
}
static void audplay_dsp_event(void *data, unsigned id, size_t len,
			      void (*getevent) (void *ptr, size_t len))
{
	struct audio *audio = data;
	uint32_t msg[28];
	getevent(msg, sizeof(msg));

	MM_DBG("msg_id=%x\n", id);

	switch (id) {
	case AUDPLAY_MSG_DEC_NEEDS_DATA:
		audplay_send_data(audio, 1);
		break;

	case AUDPLAY_MSG_BUFFER_UPDATE:
		audio_update_pcm_buf_entry(audio, msg);
		break;

	case AUDPLAY_UP_STREAM_INFO:
		if ((msg[1] & AUDPLAY_STREAM_INFO_MSG_MASK) ==
				AUDPLAY_STREAM_INFO_MSG_MASK) {
			audaac_bitstream_error_info(audio, msg);
		} else {
			audaac_update_stream_info(audio, msg);
		}
		break;

	case AUDPLAY_UP_OUTPORT_FLUSH_ACK:
		MM_DBG("OUTPORT_FLUSH_ACK\n");
		audio->rflush = 0;
		wake_up(&audio->read_wait);
		if (audio->pcm_feedback)
			audplay_buffer_refresh(audio);
		break;

	case ADSP_MESSAGE_ID:
		MM_DBG("Received ADSP event: module enable(audplaytask)\n");
		break;

	default:
		MM_ERR("unexpected message from decoder \n");
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
				MM_DBG("decoder status: sleep reason = \
						0x%04x\n", reason);
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
				MM_DBG("decoder status: init \n");
				if (audio->pcm_feedback)
					audpp_cmd_cfg_routing_mode(audio);
				else
					audpp_cmd_cfg_adec_params(audio);
				break;

			case AUDPP_DEC_STATUS_CFG:
				MM_DBG("decoder status: cfg \n");
				break;
			case AUDPP_DEC_STATUS_PLAY:
				MM_DBG("decoder status: play \n");
				/* send  mixer command */
				audpp_route_stream(audio->dec_id,
						audio->source);
				if (audio->pcm_feedback) {
					audplay_error_threshold_config(audio);
					audplay_config_hostpcm(audio);
					audplay_buffer_refresh(audio);
				}
				audio->dec_state =
					MSM_AUD_DECODER_STATE_SUCCESS;
				wake_up(&audio->wait);
				break;
			default:
				MM_ERR("unknown decoder status \n");
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
			audpp_dsp_set_eq(audio->dec_id, audio->eq_enable,
					&audio->eq, POPP);
		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			MM_DBG("CFG_MSG DISABLE\n");
			audio->running = 0;
		} else {
			MM_DBG("CFG_MSG %d?\n",	msg[0]);
		}
		break;
	case AUDPP_MSG_ROUTING_ACK:
		MM_DBG("ROUTING_ACK mode=%d\n",	msg[1]);
		audpp_cmd_cfg_adec_params(audio);
		break;

	case AUDPP_MSG_FLUSH_ACK:
		MM_DBG("FLUSH_ACK\n");
		audio->wflush = 0;
		audio->rflush = 0;
		wake_up(&audio->write_wait);
		if (audio->pcm_feedback)
			audplay_buffer_refresh(audio);
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
		MM_ERR("UNKNOWN (%d)\n", id);
	}

}

struct msm_adsp_ops audplay_adsp_ops_aac = {
	.event = audplay_dsp_event,
};

#define audplay_send_queue0(audio, cmd, len) \
	msm_adsp_write(audio->audplay, audio->queue_id,\
			cmd, len)

static int auddec_dsp_config(struct audio *audio, int enable)
{
	struct audpp_cmd_cfg_dec_type cfg_dec_cmd;

	memset(&cfg_dec_cmd, 0, sizeof(cfg_dec_cmd));

	cfg_dec_cmd.cmd_id = AUDPP_CMD_CFG_DEC_TYPE;
	if (enable)
		cfg_dec_cmd.dec_cfg = AUDPP_CMD_UPDATDE_CFG_DEC |
				AUDPP_CMD_ENA_DEC_V | AUDDEC_DEC_AAC;
	else
		cfg_dec_cmd.dec_cfg = AUDPP_CMD_UPDATDE_CFG_DEC |
				AUDPP_CMD_DIS_DEC_V;
	cfg_dec_cmd.dm_mode = 0x0;
	cfg_dec_cmd.stream_id = audio->dec_id;

	return audpp_send_queue1(&cfg_dec_cmd, sizeof(cfg_dec_cmd));
}

static void audpp_cmd_cfg_adec_params(struct audio *audio)
{
	struct audpp_cmd_cfg_adec_params_aac cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_CMD_CFG_ADEC_PARAMS;
	cmd.common.length = AUDPP_CMD_CFG_ADEC_PARAMS_AAC_LEN;
	cmd.common.dec_id = audio->dec_id;
	cmd.common.input_sampling_frequency = audio->out_sample_rate;
	cmd.format = audio->aac_config.format;
	cmd.audio_object = audio->aac_config.audio_object;
	cmd.ep_config = audio->aac_config.ep_config;
	cmd.aac_section_data_resilience_flag =
		audio->aac_config.aac_section_data_resilience_flag;
	cmd.aac_scalefactor_data_resilience_flag =
		audio->aac_config.aac_scalefactor_data_resilience_flag;
	cmd.aac_spectral_data_resilience_flag =
		audio->aac_config.aac_spectral_data_resilience_flag;
	cmd.sbr_on_flag = audio->aac_config.sbr_on_flag;
	cmd.sbr_ps_on_flag = audio->aac_config.sbr_ps_on_flag;
	cmd.channel_configuration = audio->aac_config.channel_configuration;

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
		cmd.decoder_id = AUDAAC_METAFIELD_MASK |
			(audio->out[idx].mfield_sz >> 1);
	else
	    cmd.decoder_id = audio->dec_id;
	cmd.buf_ptr = audio->out[idx].addr;
	cmd.buf_size = len / 2;
	cmd.partition_number = 0;
	return audplay_send_queue0(audio, &cmd, sizeof(cmd));
}

static void audplay_buffer_refresh(struct audio *audio)
{
	struct audplay_cmd_buffer_refresh refresh_cmd;

	refresh_cmd.cmd_id = AUDPLAY_CMD_BUFFER_REFRESH;
	refresh_cmd.num_buffers = 1;
	refresh_cmd.buf0_address = audio->in[audio->fill_next].addr;
	/* AAC frame size */
	refresh_cmd.buf0_length = audio->in[audio->fill_next].size -
		(audio->in[audio->fill_next].size % 1024)
		+ (audio->mfield ? 24 : 0);
	refresh_cmd.buf_read_count = 0;
	MM_DBG("buf0_addr=%x buf0_len=%d\n", refresh_cmd.buf0_address,
			refresh_cmd.buf0_length);
	(void)audplay_send_queue0(audio, &refresh_cmd, sizeof(refresh_cmd));
}

static void audplay_outport_flush(struct audio *audio)
{
	struct audplay_cmd_outport_flush op_flush_cmd;

	MM_DBG("\n"); /* Macro prints the file name and function */
	op_flush_cmd.cmd_id = AUDPLAY_CMD_OUTPORT_FLUSH;
	(void)audplay_send_queue0(audio, &op_flush_cmd, sizeof(op_flush_cmd));
}

static void audplay_error_threshold_config(struct audio *audio)
{
	union audplay_cmd_channel_info ch_cfg_cmd;

	MM_DBG("\n"); /* Macro prints the file name and function */
	ch_cfg_cmd.thr_update.cmd_id = AUDPLAY_CMD_CHANNEL_INFO;
	ch_cfg_cmd.thr_update.threshold_update = AUDPLAY_ERROR_THRESHOLD_ENABLE;
	ch_cfg_cmd.thr_update.threshold_value =
		audio->bitstream_error_threshold_value;
	(void)audplay_send_queue0(audio, &ch_cfg_cmd, sizeof(ch_cfg_cmd));
}

static void audplay_config_hostpcm(struct audio *audio)
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

static void audplay_send_data(struct audio *audio, unsigned needed)
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
			MM_DBG("frame %d free\n", audio->out_tail);
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

static void audio_flush(struct audio *audio)
{
	audio->out[0].used = 0;
	audio->out[1].used = 0;
	audio->out_head = 0;
	audio->out_tail = 0;
	audio->reserved = 0;
	audio->out_needed = 0;
	atomic_set(&audio->out_bytes, 0);
}

static void audio_flush_pcm_buf(struct audio *audio)
{
	uint8_t index;

	for (index = 0; index < PCM_BUF_MAX_COUNT; index++)
		audio->in[index].used = 0;
	audio->buf_refresh = 0;
	audio->read_next = 0;
	audio->fill_next = 0;
}

static int audaac_validate_usr_config(struct msm_audio_aac_config *config)
{
	int ret_val = -1;

	if (config->format != AUDIO_AAC_FORMAT_ADTS &&
		config->format != AUDIO_AAC_FORMAT_RAW &&
		config->format != AUDIO_AAC_FORMAT_PSUEDO_RAW &&
		config->format != AUDIO_AAC_FORMAT_LOAS)
		goto done;

	if (config->audio_object != AUDIO_AAC_OBJECT_LC &&
		config->audio_object != AUDIO_AAC_OBJECT_LTP &&
		config->audio_object != AUDIO_AAC_OBJECT_BSAC &&
		config->audio_object != AUDIO_AAC_OBJECT_ERLC)
		goto done;

	if (config->audio_object == AUDIO_AAC_OBJECT_ERLC) {
		if (config->ep_config > 3)
			goto done;
		if (config->aac_scalefactor_data_resilience_flag !=
			AUDIO_AAC_SCA_DATA_RES_OFF &&
			config->aac_scalefactor_data_resilience_flag !=
			AUDIO_AAC_SCA_DATA_RES_ON)
			goto done;
		if (config->aac_section_data_resilience_flag !=
			AUDIO_AAC_SEC_DATA_RES_OFF &&
			config->aac_section_data_resilience_flag !=
			AUDIO_AAC_SEC_DATA_RES_ON)
			goto done;
		if (config->aac_spectral_data_resilience_flag !=
			AUDIO_AAC_SPEC_DATA_RES_OFF &&
			config->aac_spectral_data_resilience_flag !=
			AUDIO_AAC_SPEC_DATA_RES_ON)
			goto done;
	} else {
		config->aac_section_data_resilience_flag =
			AUDIO_AAC_SEC_DATA_RES_OFF;
		config->aac_scalefactor_data_resilience_flag =
			AUDIO_AAC_SCA_DATA_RES_OFF;
		config->aac_spectral_data_resilience_flag =
			AUDIO_AAC_SPEC_DATA_RES_OFF;
	}

#ifndef CONFIG_AUDIO_AAC_PLUS
	if (AUDIO_AAC_SBR_ON_FLAG_OFF != config->sbr_on_flag)
		goto done;
#else
	if (config->sbr_on_flag != AUDIO_AAC_SBR_ON_FLAG_OFF &&
		config->sbr_on_flag != AUDIO_AAC_SBR_ON_FLAG_ON)
		goto done;
#endif

#ifndef CONFIG_AUDIO_ENHANCED_AAC_PLUS
	if (AUDIO_AAC_SBR_PS_ON_FLAG_OFF != config->sbr_ps_on_flag)
		goto done;
#else
	if (config->sbr_ps_on_flag != AUDIO_AAC_SBR_PS_ON_FLAG_OFF &&
		config->sbr_ps_on_flag != AUDIO_AAC_SBR_PS_ON_FLAG_ON)
		goto done;
#endif

	if (config->dual_mono_mode > AUDIO_AAC_DUAL_MONO_PL_SR)
		goto done;

	if (config->channel_configuration > 2)
		goto done;

	ret_val = 0;
 done:
	return ret_val;
}

static void audio_ioport_reset(struct audio *audio)
{
	/* Make sure read/write thread are free from
	 * sleep and knowing that system is not able
	 * to process io request at the moment
	 */
	wake_up(&audio->write_wait);
	mutex_lock(&audio->write_lock);
	audio_flush(audio);
	mutex_unlock(&audio->write_lock);
	wake_up(&audio->read_wait);
	mutex_lock(&audio->read_lock);
	audio_flush_pcm_buf(audio);
	mutex_unlock(&audio->read_lock);
	audio->avsync_flag = 1;
	wake_up(&audio->avsync_wait);

}

static int audaac_events_pending(struct audio *audio)
{
	unsigned long flags;
	int empty;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	empty = !list_empty(&audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	return empty || audio->event_abort;
}

static void audaac_reset_event_queue(struct audio *audio)
{
	unsigned long flags;
	struct audaac_event *drv_evt;
	struct list_head *ptr, *next;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	list_for_each_safe(ptr, next, &audio->event_queue) {
		drv_evt = list_first_entry(&audio->event_queue,
				struct audaac_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	list_for_each_safe(ptr, next, &audio->free_event_queue) {
		drv_evt = list_first_entry(&audio->free_event_queue,
				struct audaac_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	return;
}

static long audaac_process_event_req(struct audio *audio, void __user *arg)
{
	long rc;
	struct msm_audio_event usr_evt;
	struct audaac_event *drv_evt = NULL;
	int timeout;
	unsigned long flags;

	if (copy_from_user(&usr_evt, arg, sizeof(struct msm_audio_event)))
		return -EFAULT;

	timeout = (int) usr_evt.timeout_ms;

	if (timeout > 0) {
		rc = wait_event_interruptible_timeout(
			audio->event_wait, audaac_events_pending(audio),
			msecs_to_jiffies(timeout));
		if (rc == 0)
			return -ETIMEDOUT;
	} else {
		rc = wait_event_interruptible(
			audio->event_wait, audaac_events_pending(audio));
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
				struct audaac_event, list);
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

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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
			rc = audaac_process_event_req(audio,
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
			MM_INFO("dec_state %d rc = %d\n", audio->dec_state, rc);

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
		break;
	case AUDIO_FLUSH:
		MM_DBG("AUDIO_FLUSH running=%d\n", audio->running);
		audio->rflush = 1;
		audio->wflush = 1;
		audio_ioport_reset(audio);
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

	case AUDIO_OUTPORT_FLUSH:
		MM_DBG("AUDIO_OUTPORT_FLUSH\n");
		audio->rflush = 1;
		wake_up(&audio->read_wait);
		mutex_lock(&audio->read_lock);
		audio_flush_pcm_buf(audio);
		mutex_unlock(&audio->read_lock);
		audplay_outport_flush(audio);
		rc = wait_event_interruptible(audio->read_wait,
				!audio->rflush);
		if (rc < 0) {
			MM_ERR("AUDPLAY_OUTPORT_FLUSH interrupted\n");
			rc = -EINTR;
		}
		break;

	case AUDIO_SET_CONFIG:{
			struct msm_audio_config config;

			if (copy_from_user
			    (&config, (void *)arg, sizeof(config))) {
				rc = -EFAULT;
				break;
			}

			if (config.channel_count == 1) {
				config.channel_count =
				    AUDPP_CMD_PCM_INTF_MONO_V;
			} else if (config.channel_count == 2) {
				config.channel_count =
				    AUDPP_CMD_PCM_INTF_STEREO_V;
			} else {
				rc = -EINVAL;
				break;
			}

			audio->out_sample_rate = config.sample_rate;
			audio->out_channel_mode = config.channel_count;
			audio->mfield = config.meta_field;
			rc = 0;
			break;
		}
	case AUDIO_GET_CONFIG:{
			struct msm_audio_config config;
			config.buffer_size = (audio->out_dma_sz >> 1);
			config.buffer_count = 2;
			config.sample_rate = audio->out_sample_rate;
			if (audio->out_channel_mode ==
			    AUDPP_CMD_PCM_INTF_MONO_V) {
				config.channel_count = 1;
			} else {
				config.channel_count = 2;
			}
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
	case AUDIO_GET_AAC_CONFIG:{
			if (copy_to_user((void *)arg, &audio->aac_config,
				sizeof(audio->aac_config)))
				rc = -EFAULT;
			else
				rc = 0;
			break;
		}
	case AUDIO_SET_AAC_CONFIG:{
			struct msm_audio_aac_config usr_config;

			if (copy_from_user
				(&usr_config, (void *)arg,
					sizeof(usr_config))) {
				rc = -EFAULT;
				break;
			}

			if (audaac_validate_usr_config(&usr_config) == 0) {
				audio->aac_config = usr_config;
				rc = 0;
			} else
				rc = -EINVAL;

			break;
		}
	case AUDIO_GET_PCM_CONFIG:{
			struct msm_audio_pcm_config config;
			config.pcm_feedback = audio->pcm_feedback;
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
			if (config.pcm_feedback != audio->pcm_feedback) {
				MM_ERR("Not sufficient permission to"
					 "change the playback mode\n");
				rc = -EACCES;
				break;
			}
			if ((config.buffer_count > PCM_BUF_MAX_COUNT) ||
			    (config.buffer_count == 1))
				config.buffer_count = PCM_BUF_MAX_COUNT;

			if (config.buffer_size < PCM_BUFSZ_MIN)
				config.buffer_size = PCM_BUFSZ_MIN;

			/* Check if pcm feedback is required */
			if (config.pcm_feedback) {
					audio->buf_refresh = 0;
					audio->read_next = 0;
					audio->fill_next = 0;
			}
			rc = 0;
			break;
		}
	case AUDIO_PAUSE:
		MM_DBG("AUDIO_PAUSE %ld\n", arg);
		rc = audpp_pause(audio->dec_id, (int) arg);
		break;
	case AUDIO_GET_STREAM_INFO:{
		if (audio->stream_info.sample_rate == 0) {
			/* haven't received DSP stream event,
			the stream info is not updated */
			rc = -EPERM;
			break;
		}
		if (copy_to_user((void *)arg, &audio->stream_info,
			sizeof(struct msm_audio_bitstream_info)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}
	case AUDIO_GET_BITSTREAM_ERROR_INFO:{
		if ((audio->bitstream_error_info.err_msg_indicator &
				AUDPLAY_STREAM_INFO_MSG_MASK) ==
				AUDPLAY_STREAM_INFO_MSG_MASK) {
			/* haven't received bitstream error info event,
			the bitstream error info is not updated */
			rc = -EPERM;
			break;
		}
		if (copy_to_user((void *)arg, &audio->bitstream_error_info,
			sizeof(struct msm_audio_bitstream_error_info)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}
	case AUDIO_GET_SESSION_ID:
		if (copy_to_user((void *) arg, &audio->dec_id,
				sizeof(unsigned short)))
			rc =  -EFAULT;
		else
			rc = 0;
		break;
	case AUDIO_SET_ERR_THRESHOLD_VALUE:
		if (copy_from_user(&audio->bitstream_error_threshold_value,
					(void *)arg, sizeof(uint32_t)))
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
static int audaac_fsync(struct file *file, int datasync)
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
		audplay_send_data(audio, 0);

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

static ssize_t audio_read(struct file *file, char __user *buf, size_t count,
			  loff_t *pos)
{
	struct audio *audio = file->private_data;
	const char __user *start = buf;
	int rc = 0;

	if (!audio->pcm_feedback)
		return 0; /* PCM feedback is not enabled. Nothing to read */

	mutex_lock(&audio->read_lock);
	MM_DBG("to read %d \n", count);
	while (count > 0) {
		rc = wait_event_interruptible_timeout(audio->read_wait,
					      (audio->in[audio->read_next].
						used > 0) || (audio->stopped)
						|| (audio->rflush),
			msecs_to_jiffies(MSM_AUD_BUFFER_UPDATE_WAIT_MS));

		if (rc == 0) {
			rc = -ETIMEDOUT;
			break;
		} else if (rc < 0)
			break;

		if (audio->stopped || audio->rflush) {
			rc = -EBUSY;
			break;
		}

		if (count < audio->in[audio->read_next].used) {
			/* Read must happen in frame boundary. Since driver
			   does not know frame size, read count must be greater
			   or equal to size of PCM samples */
			MM_DBG("no partial frame done reading\n");
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
				/*
				* Force to exit while loop
				* to prevent output thread
				* sleep too long if data is not
				* ready at this moment.
				*/
		}
	}

	/* don't feed output buffer to HW decoder during flushing
	 * buffer refresh command will be sent once flush completes
	 * send buf refresh command here can confuse HW decoder
	 */
	if (audio->buf_refresh && !audio->rflush) {
		audio->buf_refresh = 0;
		MM_DBG("kick start pcm feedback again\n");
		audplay_buffer_refresh(audio);
	}

	mutex_unlock(&audio->read_lock);

	if (buf > start)
		rc = buf - start;

	MM_DBG("read %d bytes\n", rc);
	return rc;
}

static int audaac_process_eos(struct audio *audio,
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
	audplay_send_data(audio, 0);
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
	audplay_send_data(audio, 0);
done:
	return rc;
}
static ssize_t audio_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *pos)
{
	struct audio *audio = file->private_data;
	const char __user *start = buf;
	struct buffer *frame;
	size_t xfer;
	char *cpy_ptr;
	int rc = 0, eos_condition = AUDAAC_EOS_NONE;
	unsigned dsize;

	unsigned short mfield_size = 0;
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
				/* Check if EOS flag is set and buffer has
				* contains just meta field
				*/
				if (cpy_ptr[AUDAAC_EOS_FLG_OFFSET] &
						AUDAAC_EOS_FLG_MASK) {
					MM_DBG("eos set\n");
					eos_condition = AUDAAC_EOS_SET;
					if (mfield_size == count) {
						buf += mfield_size;
						break;
					} else
					cpy_ptr[AUDAAC_EOS_FLG_OFFSET] &=
							~AUDAAC_EOS_FLG_MASK;
				}
				/* Check EOS to see if */
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
			MM_DBG("append reserved byte %x\n",
				audio->rsv_byte);
			*cpy_ptr = audio->rsv_byte;
			xfer = (count > ((frame->size - mfield_size) - 1)) ?
				(frame->size - mfield_size) - 1 : count;
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
			audplay_send_data(audio, 0);
		}
	}
	MM_DBG("eos_condition %x buf[0x%x] start[0x%x]\n", eos_condition,
			(int) buf, (int) start);
	if (eos_condition == AUDAAC_EOS_SET)
		rc = audaac_process_eos(audio, start, mfield_size);
	mutex_unlock(&audio->write_lock);
	if (!rc) {
		if (buf > start)
			return buf - start;
	}
	return rc;
}

static int audio_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	MM_INFO("audio instance 0x%08x freeing\n", (int)audio);

	mutex_lock(&audio->lock);
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, audio->dec_id);
	audio_disable(audio);
	audio_flush(audio);
	audio_flush_pcm_buf(audio);
	msm_adsp_put(audio->audplay);
	audpp_adec_free(audio->dec_id);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&audio->suspend_ctl.node);
#endif
	audio->event_abort = 1;
	wake_up(&audio->event_wait);
	audaac_reset_event_queue(audio);
	iounmap(audio->data);
	pmem_kfree(audio->phys);
	iounmap(audio->read_data);
	pmem_kfree(audio->read_phys);
	mutex_unlock(&audio->lock);
#ifdef CONFIG_DEBUG_FS
	if (audio->dentry)
		debugfs_remove(audio->dentry);
#endif
	kfree(audio);
	return 0;
}

static void audaac_post_event(struct audio *audio, int type,
		union msm_audio_event_payload payload)
{
	struct audaac_event *e_node = NULL;
	unsigned long flags;

	spin_lock_irqsave(&audio->event_queue_lock, flags);

	if (!list_empty(&audio->free_event_queue)) {
		e_node = list_first_entry(&audio->free_event_queue,
				struct audaac_event, list);
		list_del(&e_node->list);
	} else {
		e_node = kmalloc(sizeof(struct audaac_event), GFP_ATOMIC);
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
static void audaac_suspend(struct early_suspend *h)
{
	struct audaac_suspend_ctl *ctl =
		container_of(h, struct audaac_suspend_ctl, node);
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	audaac_post_event(ctl->audio, AUDIO_EVENT_SUSPEND, payload);
}

static void audaac_resume(struct early_suspend *h)
{
	struct audaac_suspend_ctl *ctl =
		container_of(h, struct audaac_suspend_ctl, node);
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	audaac_post_event(ctl->audio, AUDIO_EVENT_RESUME, payload);
}
#endif

#ifdef CONFIG_DEBUG_FS
static ssize_t audaac_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t audaac_debug_read(struct file *file, char __user *buf,
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

static const struct file_operations audaac_debug_fops = {
	.read = audaac_debug_read,
	.open = audaac_debug_open,
};
#endif

static int audio_open(struct inode *inode, struct file *file)
{
	struct audio *audio = NULL;
	int rc, dec_attrb, decid, index, offset = 0;
	unsigned pmem_sz = DMASZ;
	struct audaac_event *e_node = NULL;
#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_aac_" + 5];
#endif

	/* Allocate audio instance, set to zero */
	audio = kzalloc(sizeof(struct audio), GFP_KERNEL);
	if (!audio) {
		MM_ERR("no memory to allocate audio instance \n");
		rc = -ENOMEM;
		goto done;
	}
	MM_INFO("audio instance 0x%08x created\n", (int)audio);

	/* Allocate the decoder */
	dec_attrb = AUDDEC_DEC_AAC;
	if ((file->f_mode & FMODE_WRITE) &&
			(file->f_mode & FMODE_READ)) {
		dec_attrb |= MSM_AUD_MODE_NONTUNNEL;
		audio->pcm_feedback = NON_TUNNEL_MODE_PLAYBACK;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
		dec_attrb |= MSM_AUD_MODE_TUNNEL;
		audio->pcm_feedback = TUNNEL_MODE_PLAYBACK;
	} else {
		kfree(audio);
		rc = -EACCES;
		goto done;
	}
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

	while (pmem_sz >= DMASZ_MIN) {
		MM_DBG("pmemsz = %d \n", pmem_sz);
		audio->phys = pmem_kalloc(pmem_sz, PMEM_MEMTYPE_EBI1|
						PMEM_ALIGNMENT_4K);
		if (!IS_ERR((void *)audio->phys)) {
			audio->data = ioremap(audio->phys, pmem_sz);
			if (!audio->data) {
				MM_ERR("could not allocate write buffers, \
						freeing instance 0x%08x\n",
						(int)audio);
				rc = -ENOMEM;
				pmem_kfree(audio->phys);
				audpp_adec_free(audio->dec_id);
				kfree(audio);
				goto done;
			}
			MM_DBG("write buf: phy addr 0x%08x kernel addr \
				0x%08x\n", audio->phys, (int)audio->data);
			break;
		} else if (pmem_sz == DMASZ_MIN) {
			MM_ERR("could not allocate write buffers, freeing \
					instance 0x%08x\n", (int)audio);
			rc = -ENOMEM;
			audpp_adec_free(audio->dec_id);
			kfree(audio);
			goto done;
		} else
			pmem_sz >>= 1;
	}
	audio->out_dma_sz = pmem_sz;

	audio->read_phys = pmem_kalloc(PCM_BUFSZ_MIN * PCM_BUF_MAX_COUNT,
				PMEM_MEMTYPE_EBI1|PMEM_ALIGNMENT_4K);
	if (IS_ERR((void *)audio->read_phys)) {
		MM_ERR("could not allocate read buffers, freeing instance \
				0x%08x\n", (int)audio);
		rc = -ENOMEM;
		iounmap(audio->data);
		pmem_kfree(audio->phys);
		audpp_adec_free(audio->dec_id);
		kfree(audio);
		goto done;
	}
	audio->read_data = ioremap(audio->read_phys,
				PCM_BUFSZ_MIN * PCM_BUF_MAX_COUNT);
	if (!audio->read_data) {
		MM_ERR("could not allocate read buffers, freeing instance \
				0x%08x\n", (int)audio);
		rc = -ENOMEM;
		iounmap(audio->data);
		pmem_kfree(audio->phys);
		pmem_kfree(audio->read_phys);
		audpp_adec_free(audio->dec_id);
		kfree(audio);
		goto done;
	}
	MM_DBG("read buf: phy addr 0x%08x kernel addr 0x%08x\n",
				audio->read_phys, (int)audio->read_data);

	rc = msm_adsp_get(audio->module_name, &audio->audplay,
			  &audplay_adsp_ops_aac, audio);
	if (rc) {
		MM_ERR("failed to get %s module, freeing instance 0x%08x\n",
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
	audio->out[0].size = audio->out_dma_sz >> 1;

	audio->out[1].data = audio->data + audio->out[0].size;
	audio->out[1].addr = audio->phys + audio->out[0].size;
	audio->out[1].size = audio->out[0].size;

	audio->pcm_buf_count = PCM_BUF_MAX_COUNT;
	for (index = 0; index < PCM_BUF_MAX_COUNT; index++) {
		audio->in[index].data = audio->read_data + offset;
		audio->in[index].addr = audio->read_phys + offset;
		audio->in[index].size = PCM_BUFSZ_MIN;
		audio->in[index].used = 0;
		offset += PCM_BUFSZ_MIN;
	}

	audio->out_sample_rate = 44100;
	audio->out_channel_mode = AUDPP_CMD_PCM_INTF_STEREO_V;
	audio->aac_config.format = AUDIO_AAC_FORMAT_ADTS;
	audio->aac_config.audio_object = AUDIO_AAC_OBJECT_LC;
	audio->aac_config.ep_config = 0;
	audio->aac_config.aac_section_data_resilience_flag =
		AUDIO_AAC_SEC_DATA_RES_OFF;
	audio->aac_config.aac_scalefactor_data_resilience_flag =
		AUDIO_AAC_SCA_DATA_RES_OFF;
	audio->aac_config.aac_spectral_data_resilience_flag =
		AUDIO_AAC_SPEC_DATA_RES_OFF;
#ifdef CONFIG_AUDIO_AAC_PLUS
	audio->aac_config.sbr_on_flag = AUDIO_AAC_SBR_ON_FLAG_ON;
#else
	audio->aac_config.sbr_on_flag = AUDIO_AAC_SBR_ON_FLAG_OFF;
#endif
#ifdef CONFIG_AUDIO_ENHANCED_AAC_PLUS
	audio->aac_config.sbr_ps_on_flag = AUDIO_AAC_SBR_PS_ON_FLAG_ON;
#else
	audio->aac_config.sbr_ps_on_flag = AUDIO_AAC_SBR_PS_ON_FLAG_OFF;
#endif
	audio->aac_config.dual_mono_mode = AUDIO_AAC_DUAL_MONO_PL_SR;
	audio->aac_config.channel_configuration = 2;
	audio->vol_pan.volume = 0x2000;
	audio->bitstream_error_threshold_value =
		BITSTREAM_ERROR_THRESHOLD_VALUE;

	audio_flush(audio);

	file->private_data = audio;
	audio->opened = 1;

	audio->device_events = AUDDEV_EVT_DEV_RDY
				|AUDDEV_EVT_DEV_RLS|
				AUDDEV_EVT_STREAM_VOL_CHG;

	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_DEC,
					audio->dec_id,
					aac_listner,
					(void *)audio);
	if (rc) {
		MM_ERR("%s: failed to register listner\n", __func__);
		goto event_err;
	}

#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof name, "msm_aac_%04x", audio->dec_id);
	audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
				NULL, (void *) audio,
				&audaac_debug_fops);

	if (IS_ERR(audio->dentry))
		MM_DBG("debugfs_create_file failed\n");
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	audio->suspend_ctl.node.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	audio->suspend_ctl.node.resume = audaac_resume;
	audio->suspend_ctl.node.suspend = audaac_suspend;
	audio->suspend_ctl.audio = audio;
	register_early_suspend(&audio->suspend_ctl.node);
#endif
	for (index = 0; index < AUDAAC_EVENT_NUM; index++) {
		e_node = kmalloc(sizeof(struct audaac_event), GFP_KERNEL);
		if (e_node)
			list_add_tail(&e_node->list, &audio->free_event_queue);
		else {
			MM_ERR("event pkt alloc failed\n");
			break;
		}
	}
	memset(&audio->stream_info, 0, sizeof(struct msm_audio_bitstream_info));
	memset(&audio->bitstream_error_info, 0,
			sizeof(struct msm_audio_bitstream_info));
done:
	return rc;
event_err:
	msm_adsp_put(audio->audplay);
err:
	iounmap(audio->data);
	pmem_kfree(audio->phys);
	iounmap(audio->read_data);
	pmem_kfree(audio->read_phys);
	audpp_adec_free(audio->dec_id);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_aac_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_release,
	.read = audio_read,
	.write = audio_write,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audaac_fsync
};

struct miscdevice audio_aac_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_aac",
	.fops = &audio_aac_fops,
};

static int __init audio_init(void)
{
	return misc_register(&audio_aac_misc);
}

static void __exit audio_exit(void)
{
	misc_deregister(&audio_aac_misc);
}

module_init(audio_init);
module_exit(audio_exit);

MODULE_DESCRIPTION("MSM AAC driver");
MODULE_LICENSE("GPL v2");

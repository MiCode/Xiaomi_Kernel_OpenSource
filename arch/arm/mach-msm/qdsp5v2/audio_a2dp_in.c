/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * sbc/pcm audio input driver
 * Based on the pcm input driver in arch/arm/mach-msm/qdsp5v2/audio_pcm_in.c
 *
 * Copyright (C) 2008 HTC Corporation
 * Copyright (C) 2008 Google, Inc.
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
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
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/msm_audio.h>
#include <linux/msm_audio_sbc.h>
#include <linux/android_pmem.h>
#include <linux/memory_alloc.h>

#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/msm_adsp.h>
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include <mach/qdsp5v2/qdsp5audreccmdi.h>
#include <mach/qdsp5v2/qdsp5audrecmsg.h>
#include <mach/qdsp5v2/audpreproc.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/debug_mm.h>

/* FRAME_NUM must be a power of two */
#define FRAME_NUM		(8)
#define FRAME_SIZE		(2052 * 2)
#define FRAME_SIZE_SBC		(768 * 2)
#define MONO_DATA_SIZE		(2048)
#define STEREO_DATA_SIZE	(MONO_DATA_SIZE * 2)
#define DMASZ 			(FRAME_SIZE * FRAME_NUM)

struct buffer {
	void *data;
	uint32_t size;
	uint32_t read;
	uint32_t addr;
	uint32_t frame_num;
	uint32_t frame_len;
};

struct audio_a2dp_in {
	struct buffer in[FRAME_NUM];

	spinlock_t dsp_lock;

	atomic_t in_bytes;
	atomic_t in_samples;

	struct mutex lock;
	struct mutex read_lock;
	wait_queue_head_t wait;
	wait_queue_head_t wait_enable;

	struct msm_adsp_module *audrec;

	struct audrec_session_info session_info; /*audrec session info*/

	/* configuration to use on next enable */
	uint32_t samp_rate;
	uint32_t channel_mode;
	uint32_t buffer_size; /* 2048 for mono, 4096 for stereo */
	uint32_t enc_type;
	struct msm_audio_sbc_enc_config cfg;

	uint32_t dsp_cnt;
	uint32_t in_head; /* next buffer dsp will write */
	uint32_t in_tail; /* next buffer read() will read */
	uint32_t in_count; /* number of buffers available to read() */
	uint32_t mode;

	const char *module_name;
	unsigned queue_ids;
	uint16_t enc_id; /* Session Id */

	uint16_t source; /* Encoding source bit mask */
	uint32_t device_events; /* device events interested in */
	uint32_t dev_cnt;
	spinlock_t dev_lock;

	/* data allocated for various buffers */
	char *data;
	dma_addr_t phys;
	void *msm_map;

	int opened;
	int enabled;
	int running;
	int stopped; /* set when stopped, cleared on flush */
	int abort; /* set when error, like sample rate mismatch */
	char *build_id;
};

static struct audio_a2dp_in the_audio_a2dp_in;

struct wav_frame {
	uint16_t frame_count_lsw;
	uint16_t frame_count_msw;
	uint16_t frame_length;
	uint16_t erased_a2dp;
	unsigned char raw_bitstream[]; /* samples */
};

struct sbc_frame {
	uint16_t bit_rate_msw;
	uint16_t bit_rate_lsw;
	uint16_t frame_length;
	uint16_t frame_num;
	unsigned char raw_bitstream[]; /* samples */
};

struct audio_frame {
	union {
		struct wav_frame wav;
		struct sbc_frame sbc;
	} a2dp;
} __attribute__((packed));

/* Audrec Queue command sent macro's */
#define audrec_send_bitstreamqueue(audio, cmd, len) \
	msm_adsp_write(audio->audrec, ((audio->queue_ids & 0xFFFF0000) >> 16),\
			cmd, len)

#define audrec_send_audrecqueue(audio, cmd, len) \
	msm_adsp_write(audio->audrec, (audio->queue_ids & 0x0000FFFF),\
			cmd, len)

/* DSP command send functions */
static int auda2dp_in_enc_config(struct audio_a2dp_in *audio, int enable);
static int auda2dp_in_param_config(struct audio_a2dp_in *audio);
static int auda2dp_in_mem_config(struct audio_a2dp_in *audio);
static int auda2dp_in_record_config(struct audio_a2dp_in *audio, int enable);
static int auda2dp_dsp_read_buffer(struct audio_a2dp_in *audio,
							uint32_t read_cnt);

static void auda2dp_in_get_dsp_frames(struct audio_a2dp_in *audio);

static void auda2dp_in_flush(struct audio_a2dp_in *audio);

static void a2dp_in_listener(u32 evt_id, union auddev_evt_data *evt_payload,
				void *private_data)
{
	struct audio_a2dp_in *audio = (struct audio_a2dp_in *) private_data;
	unsigned long flags;

	MM_DBG("evt_id = 0x%8x\n", evt_id);
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY: {
		MM_DBG("AUDDEV_EVT_DEV_RDY\n");
		spin_lock_irqsave(&audio->dev_lock, flags);
		audio->dev_cnt++;
		audio->source |= (0x1 << evt_payload->routing_id);
		spin_unlock_irqrestore(&audio->dev_lock, flags);

		if ((audio->running == 1) && (audio->enabled == 1))
			auda2dp_in_record_config(audio, 1);

		break;
	}
	case AUDDEV_EVT_DEV_RLS: {
		MM_DBG("AUDDEV_EVT_DEV_RLS\n");
		spin_lock_irqsave(&audio->dev_lock, flags);
		audio->dev_cnt--;
		audio->source &= ~(0x1 << evt_payload->routing_id);
		spin_unlock_irqrestore(&audio->dev_lock, flags);

		if (!audio->running || !audio->enabled)
			break;

		/* Turn of as per source */
		if (audio->source)
			auda2dp_in_record_config(audio, 1);
		else
			/* Turn off all */
			auda2dp_in_record_config(audio, 0);

		break;
	}
	case AUDDEV_EVT_FREQ_CHG: {
		MM_DBG("Encoder Driver got sample rate change event\n");
		MM_DBG("sample rate %d\n", evt_payload->freq_info.sample_rate);
		MM_DBG("dev_type %d\n", evt_payload->freq_info.dev_type);
		MM_DBG("acdb_dev_id %d\n", evt_payload->freq_info.acdb_dev_id);
		if (audio->running == 1) {
			/* Stop Recording sample rate does not match
			   with device sample rate */
			if (evt_payload->freq_info.sample_rate !=
				audio->samp_rate) {
				auda2dp_in_record_config(audio, 0);
				audio->abort = 1;
				wake_up(&audio->wait);
			}
		}
		break;
	}
	default:
		MM_ERR("wrong event %d\n", evt_id);
		break;
	}
}

/* ------------------- dsp preproc event handler--------------------- */
static void audpreproc_dsp_event(void *data, unsigned id,  void *msg)
{
	struct audio_a2dp_in *audio = data;

	switch (id) {
	case AUDPREPROC_ERROR_MSG: {
		struct audpreproc_err_msg *err_msg = msg;

		MM_ERR("ERROR_MSG: stream id %d err idx %d\n",
		err_msg->stream_id, err_msg->aud_preproc_err_idx);
		/* Error case */
		wake_up(&audio->wait_enable);
		break;
	}
	case AUDPREPROC_CMD_CFG_DONE_MSG: {
		MM_DBG("CMD_CFG_DONE_MSG \n");
		break;
	}
	case AUDPREPROC_CMD_ENC_CFG_DONE_MSG: {
		struct audpreproc_cmd_enc_cfg_done_msg *enc_cfg_msg = msg;

		MM_DBG("CMD_ENC_CFG_DONE_MSG: stream id %d enc type \
			0x%8x\n", enc_cfg_msg->stream_id,
			enc_cfg_msg->rec_enc_type);
		/* Encoder enable success */
		if (enc_cfg_msg->rec_enc_type & ENCODE_ENABLE)
			auda2dp_in_param_config(audio);
		else { /* Encoder disable success */
			audio->running = 0;
			auda2dp_in_record_config(audio, 0);
		}
		break;
	}
	case AUDPREPROC_CMD_ENC_PARAM_CFG_DONE_MSG: {
		MM_DBG("CMD_ENC_PARAM_CFG_DONE_MSG \n");
		auda2dp_in_mem_config(audio);
		break;
	}
	case AUDPREPROC_AFE_CMD_AUDIO_RECORD_CFG_DONE_MSG: {
		MM_DBG("AFE_CMD_AUDIO_RECORD_CFG_DONE_MSG \n");
		wake_up(&audio->wait_enable);
		break;
	}
	default:
		MM_ERR("Unknown Event id %d\n", id);
	}
}

/* ------------------- dsp audrec event handler--------------------- */
static void audrec_dsp_event(void *data, unsigned id, size_t len,
			    void (*getevent)(void *ptr, size_t len))
{
	struct audio_a2dp_in *audio = data;

	switch (id) {
	case AUDREC_CMD_MEM_CFG_DONE_MSG: {
		MM_DBG("CMD_MEM_CFG_DONE MSG DONE\n");
		audio->running = 1;
		if (audio->dev_cnt > 0)
			auda2dp_in_record_config(audio, 1);
		break;
	}
	case AUDREC_FATAL_ERR_MSG: {
		struct audrec_fatal_err_msg fatal_err_msg;

		getevent(&fatal_err_msg, AUDREC_FATAL_ERR_MSG_LEN);
		MM_ERR("FATAL_ERR_MSG: err id %d\n",
				fatal_err_msg.audrec_err_id);
		/* Error stop the encoder */
		audio->stopped = 1;
		wake_up(&audio->wait);
		break;
	}
	case AUDREC_UP_PACKET_READY_MSG: {
		struct audrec_up_pkt_ready_msg pkt_ready_msg;

		getevent(&pkt_ready_msg, AUDREC_UP_PACKET_READY_MSG_LEN);
		MM_DBG("UP_PACKET_READY_MSG: write cnt lsw  %d \
		write cnt msw %d read cnt lsw %d  read cnt msw %d \n",\
		pkt_ready_msg.audrec_packet_write_cnt_lsw, \
		pkt_ready_msg.audrec_packet_write_cnt_msw, \
		pkt_ready_msg.audrec_up_prev_read_cnt_lsw, \
		pkt_ready_msg.audrec_up_prev_read_cnt_msw);

		auda2dp_in_get_dsp_frames(audio);
		break;
	}
	case ADSP_MESSAGE_ID: {
		MM_DBG("Received ADSP event: module audrectask\n");
		break;
	}
	default:
		MM_ERR("Unknown Event id %d\n", id);
	}
}

static void auda2dp_in_get_dsp_frames(struct audio_a2dp_in *audio)
{
	struct audio_frame *frame;
	uint32_t index;
	unsigned long flags;

	index = audio->in_head;

	frame = (void *) (((char *)audio->in[index].data) - \
			 sizeof(*frame));

	spin_lock_irqsave(&audio->dsp_lock, flags);
	if (audio->enc_type == ENC_TYPE_WAV)
		audio->in[index].size = frame->a2dp.wav.frame_length;
	else if (audio->enc_type == ENC_TYPE_SBC) {
		audio->in[index].size = frame->a2dp.sbc.frame_length *
						frame->a2dp.sbc.frame_num;
		audio->in[index].frame_num = frame->a2dp.sbc.frame_num;
		audio->in[index].frame_len = frame->a2dp.sbc.frame_length;
	}

	/* statistics of read */
	atomic_add(audio->in[index].size, &audio->in_bytes);
	atomic_add(1, &audio->in_samples);

	audio->in_head = (audio->in_head + 1) & (FRAME_NUM - 1);

	/* If overflow, move the tail index foward. */
	if (audio->in_head == audio->in_tail)
		audio->in_tail = (audio->in_tail + 1) & (FRAME_NUM - 1);
	else
		audio->in_count++;

	auda2dp_dsp_read_buffer(audio, audio->dsp_cnt++);
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
	wake_up(&audio->wait);
}

static struct msm_adsp_ops audrec_adsp_ops = {
	.event = audrec_dsp_event,
};

static int auda2dp_in_enc_config(struct audio_a2dp_in *audio, int enable)
{
	struct audpreproc_audrec_cmd_enc_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));
	if (audio->build_id[17] == '1') {
		cmd.cmd_id = AUDPREPROC_AUDREC_CMD_ENC_CFG_2;
	} else {
		cmd.cmd_id = AUDPREPROC_AUDREC_CMD_ENC_CFG;
	}
	cmd.stream_id = audio->enc_id;

	if (enable)
		cmd.audrec_enc_type = audio->enc_type | ENCODE_ENABLE;
	else
		cmd.audrec_enc_type &= ~(ENCODE_ENABLE);

	return audpreproc_send_audreccmdqueue(&cmd, sizeof(cmd));
}

static int auda2dp_in_param_config(struct audio_a2dp_in *audio)
{
	if (audio->enc_type == ENC_TYPE_WAV) {
		struct audpreproc_audrec_cmd_parm_cfg_wav cmd;

		memset(&cmd, 0, sizeof(cmd));
		cmd.common.cmd_id = AUDPREPROC_AUDREC_CMD_PARAM_CFG;
		cmd.common.stream_id = audio->enc_id;

		cmd.aud_rec_samplerate_idx = audio->samp_rate;
		cmd.aud_rec_stereo_mode = audio->channel_mode;
		return audpreproc_send_audreccmdqueue(&cmd, sizeof(cmd));
	} else if (audio->enc_type == ENC_TYPE_SBC) {
		struct audpreproc_audrec_cmd_parm_cfg_sbc cmd;

		memset(&cmd, 0, sizeof(cmd));
		cmd.common.cmd_id = AUDPREPROC_AUDREC_CMD_PARAM_CFG;
		cmd.common.stream_id = audio->enc_id;
		cmd.aud_rec_sbc_enc_param =
			(audio->cfg.number_of_blocks <<
			AUDREC_SBC_ENC_PARAM_NUM_SUB_BLOCKS_MASK) |
			(audio->cfg.number_of_subbands <<
			AUDREC_SBC_ENC_PARAM_NUM_SUB_BANDS_MASK) |
			(audio->cfg.mode <<
			AUDREC_SBC_ENC_PARAM_MODE_MASK) |
			(audio->cfg.bit_allocation <<
			AUDREC_SBC_ENC_PARAM_BIT_ALLOC_MASK);
		cmd.aud_rec_sbc_bit_rate_msw =
			(audio->cfg.bit_rate & 0xFFFF0000) >> 16;
		cmd.aud_rec_sbc_bit_rate_lsw =
			(audio->cfg.bit_rate & 0xFFFF);
		return audpreproc_send_audreccmdqueue(&cmd, sizeof(cmd));
	}
	return 0;
}

/* To Do: msm_snddev_route_enc(audio->enc_id); */
static int auda2dp_in_record_config(struct audio_a2dp_in *audio, int enable)
{
	struct audpreproc_afe_cmd_audio_record_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDPREPROC_AFE_CMD_AUDIO_RECORD_CFG;
	cmd.stream_id = audio->enc_id;
	if (enable)
		cmd.destination_activity = AUDIO_RECORDING_TURN_ON;
	else
		cmd.destination_activity = AUDIO_RECORDING_TURN_OFF;

	cmd.source_mix_mask = audio->source;
	if (audio->enc_id == 2) {
		if ((cmd.source_mix_mask &
				INTERNAL_CODEC_TX_SOURCE_MIX_MASK) ||
			(cmd.source_mix_mask & AUX_CODEC_TX_SOURCE_MIX_MASK) ||
			(cmd.source_mix_mask & VOICE_UL_SOURCE_MIX_MASK) ||
			(cmd.source_mix_mask & VOICE_DL_SOURCE_MIX_MASK)) {
			cmd.pipe_id = SOURCE_PIPE_1;
		}
		if (cmd.source_mix_mask &
				AUDPP_A2DP_PIPE_SOURCE_MIX_MASK)
			cmd.pipe_id |= SOURCE_PIPE_0;
	}
	return audpreproc_send_audreccmdqueue(&cmd, sizeof(cmd));
}

static int auda2dp_in_mem_config(struct audio_a2dp_in *audio)
{
	struct audrec_cmd_arecmem_cfg cmd;
	uint16_t *data = (void *) audio->data;
	int n;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_MEM_CFG_CMD;
	cmd.audrec_up_pkt_intm_count = 1;
	cmd.audrec_ext_pkt_start_addr_msw = audio->phys >> 16;
	cmd.audrec_ext_pkt_start_addr_lsw = audio->phys;
	cmd.audrec_ext_pkt_buf_number = FRAME_NUM;

	/* prepare buffer pointers:
	 * Wav:
	 * Mono: 1024 samples + 4 halfword header
	 * Stereo: 2048 samples + 4 halfword header
	 * SBC:
	 * 768 + 4 halfword header
	 */
	if (audio->enc_type == ENC_TYPE_SBC) {
		for (n = 0; n < FRAME_NUM; n++) {
			audio->in[n].data = data + 4;
			data += (4 + (FRAME_SIZE_SBC/2));
			MM_DBG("0x%8x\n", (int)(audio->in[n].data - 8));
		}
	} else if (audio->enc_type == ENC_TYPE_WAV) {
		for (n = 0; n < FRAME_NUM; n++) {
			audio->in[n].data = data + 4;
			data += (4 + (audio->channel_mode ? 2048 : 1024));
			MM_DBG("0x%8x\n", (int)(audio->in[n].data - 8));
		}
	}

	return audrec_send_audrecqueue(audio, &cmd, sizeof(cmd));
}

static int auda2dp_dsp_read_buffer(struct audio_a2dp_in *audio,
							uint32_t read_cnt)
{
	struct up_audrec_packet_ext_ptr cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = UP_AUDREC_PACKET_EXT_PTR;
	cmd.audrec_up_curr_read_count_msw = read_cnt >> 16;
	cmd.audrec_up_curr_read_count_lsw = read_cnt;

	return audrec_send_bitstreamqueue(audio, &cmd, sizeof(cmd));
}

/* must be called with audio->lock held */
static int auda2dp_in_enable(struct audio_a2dp_in *audio)
{
	if (audio->enabled)
		return 0;

	if (audpreproc_enable(audio->enc_id, &audpreproc_dsp_event, audio)) {
		MM_ERR("msm_adsp_enable(audpreproc) failed\n");
		return -ENODEV;
	}

	if (msm_adsp_enable(audio->audrec)) {
		MM_ERR("msm_adsp_enable(audrec) failed\n");
		audpreproc_disable(audio->enc_id, audio);
		return -ENODEV;
	}
	audio->enabled = 1;
	auda2dp_in_enc_config(audio, 1);

	return 0;
}

/* must be called with audio->lock held */
static int auda2dp_in_disable(struct audio_a2dp_in *audio)
{
	if (audio->enabled) {
		audio->enabled = 0;
		auda2dp_in_enc_config(audio, 0);
		wake_up(&audio->wait);
		wait_event_interruptible_timeout(audio->wait_enable,
				audio->running == 0, 1*HZ);
		msm_adsp_disable(audio->audrec);
		audpreproc_disable(audio->enc_id, audio);
	}
	return 0;
}

static void auda2dp_in_flush(struct audio_a2dp_in *audio)
{
	int i;

	audio->dsp_cnt = 0;
	audio->in_head = 0;
	audio->in_tail = 0;
	audio->in_count = 0;
	for (i = 0; i < FRAME_NUM; i++) {
		audio->in[i].size = 0;
		audio->in[i].read = 0;
	}
	MM_DBG("in_bytes %d\n", atomic_read(&audio->in_bytes));
	MM_DBG("in_samples %d\n", atomic_read(&audio->in_samples));
	atomic_set(&audio->in_bytes, 0);
	atomic_set(&audio->in_samples, 0);
}

/* ------------------- device --------------------- */
static long auda2dp_in_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct audio_a2dp_in *audio = file->private_data;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		stats.byte_count = atomic_read(&audio->in_bytes);
		stats.sample_count = atomic_read(&audio->in_samples);
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return rc;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START: {
		uint32_t freq;
		/* Poll at 48KHz always */
		freq = 48000;
		MM_DBG("AUDIO_START\n");
		rc = msm_snddev_request_freq(&freq, audio->enc_id,
					SNDDEV_CAP_TX, AUDDEV_CLNT_ENC);
		MM_DBG("sample rate configured %d sample rate requested %d\n",
				freq, audio->samp_rate);
		if (rc < 0) {
			MM_DBG("sample rate can not be set, return code %d\n",\
							rc);
			msm_snddev_withdraw_freq(audio->enc_id,
						SNDDEV_CAP_TX, AUDDEV_CLNT_ENC);
			MM_DBG("msm_snddev_withdraw_freq\n");
			break;
		}
		/*update aurec session info in audpreproc layer*/
		audio->session_info.session_id = audio->enc_id;
		audio->session_info.sampling_freq = audio->samp_rate;
		audpreproc_update_audrec_info(&audio->session_info);
		rc = auda2dp_in_enable(audio);
		if (!rc) {
			rc =
			wait_event_interruptible_timeout(audio->wait_enable,
				audio->running != 0, 1*HZ);
			MM_DBG("state %d rc = %d\n", audio->running, rc);

			if (audio->running == 0) {
				rc = -ENODEV;
				msm_snddev_withdraw_freq(audio->enc_id,
					SNDDEV_CAP_TX, AUDDEV_CLNT_ENC);
				MM_DBG("msm_snddev_withdraw_freq\n");
			} else
				rc = 0;
		}
		audio->stopped = 0;
		break;
	}
	case AUDIO_STOP: {
		/*reset the sampling frequency information at audpreproc layer*/
		audio->session_info.sampling_freq = 0;
		audpreproc_update_audrec_info(&audio->session_info);
		rc = auda2dp_in_disable(audio);
		rc = msm_snddev_withdraw_freq(audio->enc_id,
					SNDDEV_CAP_TX, AUDDEV_CLNT_ENC);
		MM_DBG("msm_snddev_withdraw_freq\n");
		audio->stopped = 1;
		audio->abort = 0;
		break;
	}
	case AUDIO_FLUSH: {
		if (audio->stopped) {
			/* Make sure we're stopped and we wake any threads
			 * that might be blocked holding the read_lock.
			 * While audio->stopped read threads will always
			 * exit immediately.
			 */
			wake_up(&audio->wait);
			mutex_lock(&audio->read_lock);
			auda2dp_in_flush(audio);
			mutex_unlock(&audio->read_lock);
		}
		break;
	}
	case AUDIO_SET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		if (copy_from_user(&cfg, (void *) arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}
		/* Allow only single frame */
		if ((audio->enc_type == ENC_TYPE_SBC) &&
				(cfg.buffer_size != FRAME_SIZE_SBC))
			rc = -EINVAL;
		else
			audio->buffer_size = cfg.buffer_size;
		break;
	}
	case AUDIO_GET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		memset(&cfg, 0, sizeof(cfg));
		if (audio->enc_type == ENC_TYPE_SBC)
			cfg.buffer_size = FRAME_SIZE_SBC;
		else
			cfg.buffer_size = MONO_DATA_SIZE;
		cfg.buffer_count = FRAME_NUM;
		if (copy_to_user((void *) arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_SET_SBC_ENC_CONFIG: {
		if (copy_from_user(&audio->cfg, (void *) arg,
						sizeof(audio->cfg))) {
			rc = -EFAULT;
			break;
		}
		audio->samp_rate = audio->cfg.sample_rate;
		audio->channel_mode = audio->cfg.channels;
		audio->enc_type = ENC_TYPE_SBC;
		break;
	}
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config cfg;
		if (copy_from_user(&cfg, (void *) arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}
		if (cfg.channel_count == 1) {
			cfg.channel_count = AUDREC_CMD_MODE_MONO;
			audio->buffer_size = MONO_DATA_SIZE;
		} else if (cfg.channel_count == 2) {
			cfg.channel_count = AUDREC_CMD_MODE_STEREO;
			audio->buffer_size = STEREO_DATA_SIZE;
		} else {
			rc = -EINVAL;
			break;
		}
		audio->samp_rate = cfg.sample_rate;
		audio->channel_mode = cfg.channel_count;
		audio->enc_type = ENC_TYPE_WAV;
		break;
	}
	case AUDIO_GET_SBC_ENC_CONFIG: {
		struct msm_audio_sbc_enc_config cfg;
		memset(&cfg, 0, sizeof(cfg));
		cfg.bit_allocation = audio->cfg.bit_allocation;
		cfg.mode =  audio->cfg.mode;
		cfg.number_of_subbands = audio->cfg.number_of_subbands;
		cfg.number_of_blocks = audio->cfg.number_of_blocks;
		cfg.sample_rate = audio->samp_rate;
		cfg.channels = audio->channel_mode;
		cfg.bit_rate = audio->cfg.bit_rate;
		if (copy_to_user((void *) arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config cfg;
		memset(&cfg, 0, sizeof(cfg));
		cfg.buffer_count = FRAME_NUM;
		cfg.sample_rate = audio->samp_rate;
		if (audio->channel_mode == AUDREC_CMD_MODE_MONO) {
			cfg.channel_count = 1;
			cfg.buffer_size = MONO_DATA_SIZE;
		} else {
			cfg.channel_count = 2;
			cfg.buffer_size = STEREO_DATA_SIZE;
		}
		cfg.type = ENC_TYPE_WAV;
		if (copy_to_user((void *) arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_GET_SESSION_ID: {
		if (copy_to_user((void *) arg, &audio->enc_id,
			sizeof(unsigned short))) {
			rc = -EFAULT;
		}
		break;
	}
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

static ssize_t auda2dp_in_read(struct file *file,
				char __user *buf,
				size_t count, loff_t *pos)
{
	struct audio_a2dp_in *audio = file->private_data;
	unsigned long flags;
	const char __user *start = buf;
	void *data;
	uint32_t index;
	uint32_t size;
	int rc = 0;
	uint32_t f_len = 0, f_num = 0;
	int i = 0;

	mutex_lock(&audio->read_lock);
	while (count > 0) {
		rc = wait_event_interruptible(
			audio->wait, (audio->in_count > 0) || audio->stopped ||
			audio->abort);

		if (rc < 0)
			break;

		if (audio->stopped && !audio->in_count) {
			MM_DBG("Driver in stop state, No more buffer to read");
			rc = 0;/* End of File */
			break;
		}

		if (audio->abort) {
			rc = -EPERM; /* Not permitted due to abort */
			break;
		}

		index = audio->in_tail;
		data = (uint8_t *) audio->in[index].data;
		size = audio->in[index].size;
		if (count >= size) {
			if (audio->enc_type == ENC_TYPE_SBC &&
				(audio->in[index].frame_len % 2)) {
				f_len = audio->in[index].frame_len;
				f_num = audio->in[index].frame_num;
				for (i = 0; i < f_num; i++) {
					if (copy_to_user(&buf[i * f_len],
					(uint8_t *) (data + (i * (f_len + 1))),
					f_len)) {
						rc = -EFAULT;
						break;
					}
				}
			} else {
				if (copy_to_user(buf, data, size)) {
					rc = -EFAULT;
					break;
				}
			}
			spin_lock_irqsave(&audio->dsp_lock, flags);
			if (index != audio->in_tail) {
				/* overrun -- data is
				 * invalid and we need to retry */
				spin_unlock_irqrestore(&audio->dsp_lock, flags);
				continue;
			}
			audio->in[index].size = 0;
			audio->in_tail = (audio->in_tail + 1) & (FRAME_NUM - 1);
			audio->in_count--;
			spin_unlock_irqrestore(&audio->dsp_lock, flags);
			count -= size;
			buf += size;
		} else {
			MM_ERR("short read\n");
			break;
		}
	}
	mutex_unlock(&audio->read_lock);
	if (buf > start)
		return buf - start;

	return rc;
}

static ssize_t auda2dp_in_write(struct file *file,
				const char __user *buf,
				size_t count, loff_t *pos)
{
	return -EINVAL;
}

static int auda2dp_in_release(struct inode *inode, struct file *file)
{
	struct audio_a2dp_in *audio = file->private_data;

	mutex_lock(&audio->lock);
	/* with draw frequency for session
	   incase not stopped the driver */
	msm_snddev_withdraw_freq(audio->enc_id, SNDDEV_CAP_TX,
					AUDDEV_CLNT_ENC);
	auddev_unregister_evt_listner(AUDDEV_CLNT_ENC, audio->enc_id);
	/*reset the sampling frequency information at audpreproc layer*/
	audio->session_info.sampling_freq = 0;
	audpreproc_update_audrec_info(&audio->session_info);
	auda2dp_in_disable(audio);
	auda2dp_in_flush(audio);
	msm_adsp_put(audio->audrec);
	audpreproc_aenc_free(audio->enc_id);
	audio->audrec = NULL;
	audio->opened = 0;
	if (audio->data) {
		iounmap(audio->msm_map);
		free_contiguous_memory_by_paddr(audio->phys);
		audio->data = NULL;
	}
	mutex_unlock(&audio->lock);
	return 0;
}

static int auda2dp_in_open(struct inode *inode, struct file *file)
{
	struct audio_a2dp_in *audio = &the_audio_a2dp_in;
	int rc;
	int encid;

	mutex_lock(&audio->lock);
	if (audio->opened) {
		rc = -EBUSY;
		goto done;
	}

	audio->phys = allocate_contiguous_ebi_nomap(DMASZ, SZ_4K);
	if (audio->phys) {
		audio->msm_map = ioremap(audio->phys, DMASZ);
		if (IS_ERR(audio->msm_map)) {
			MM_ERR("could not map the phys address to kernel"
							"space\n");
			rc = -ENOMEM;
			free_contiguous_memory_by_paddr(audio->phys);
			goto done;
		}
		audio->data = (u8 *)audio->msm_map;
	} else {
		MM_ERR("could not allocate DMA buffers\n");
		rc = -ENOMEM;
		goto done;
	}
	MM_DBG("Memory addr = 0x%8x  phy addr = 0x%8x\n",\
		(int) audio->data, (int) audio->phys);

	if ((file->f_mode & FMODE_WRITE) &&
				(file->f_mode & FMODE_READ)) {
		rc = -EACCES;
		MM_ERR("Non tunnel encoding is not supported\n");
		goto done;
	} else if (!(file->f_mode & FMODE_WRITE) &&
					(file->f_mode & FMODE_READ)) {
		audio->mode = MSM_AUD_ENC_MODE_TUNNEL;
		MM_DBG("Opened for Tunnel mode encoding\n");
	} else {
		rc = -EACCES;
		goto done;
	}
	/* Settings will be re-config at AUDIO_SET_CONFIG/SBC_ENC_CONFIG,
	 * but at least we need to have initial config
	 */
	audio->channel_mode = AUDREC_CMD_MODE_MONO;
	audio->buffer_size = FRAME_SIZE_SBC;
	audio->samp_rate = 48000;
	audio->enc_type = ENC_TYPE_SBC | audio->mode;
	audio->cfg.bit_allocation = AUDIO_SBC_BA_SNR;
	audio->cfg.mode = AUDIO_SBC_MODE_JSTEREO;
	audio->cfg.number_of_subbands = AUDIO_SBC_BANDS_8;
	audio->cfg.number_of_blocks = AUDIO_SBC_BLOCKS_16;
	audio->cfg.bit_rate = 320000; /* max 512kbps(mono), 320kbs(others) */

	encid = audpreproc_aenc_alloc(audio->enc_type, &audio->module_name,
			&audio->queue_ids);
	if (encid < 0) {
		MM_ERR("No free encoder available\n");
		rc = -ENODEV;
		goto done;
	}
	audio->enc_id = encid;

	rc = msm_adsp_get(audio->module_name, &audio->audrec,
			   &audrec_adsp_ops, audio);

	if (rc) {
		audpreproc_aenc_free(audio->enc_id);
		goto done;
	}

	audio->stopped = 0;
	audio->source = 0;
	audio->abort = 0;
	auda2dp_in_flush(audio);
	audio->device_events = AUDDEV_EVT_DEV_RDY | AUDDEV_EVT_DEV_RLS |
				AUDDEV_EVT_FREQ_CHG;

	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_ENC, audio->enc_id,
					a2dp_in_listener, (void *) audio);
	if (rc) {
		MM_ERR("failed to register device event listener\n");
		goto evt_error;
	}
	audio->build_id = socinfo_get_build_id();
	MM_DBG("Modem build id = %s\n", audio->build_id);
	file->private_data = audio;
	audio->opened = 1;
	rc = 0;
done:
	mutex_unlock(&audio->lock);
	return rc;
evt_error:
	msm_adsp_put(audio->audrec);
	audpreproc_aenc_free(audio->enc_id);
	mutex_unlock(&audio->lock);
	return rc;
}

static const struct file_operations audio_a2dp_in_fops = {
	.owner		= THIS_MODULE,
	.open		= auda2dp_in_open,
	.release	= auda2dp_in_release,
	.read		= auda2dp_in_read,
	.write		= auda2dp_in_write,
	.unlocked_ioctl	= auda2dp_in_ioctl,
};

struct miscdevice audio_a2dp_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_a2dp_in",
	.fops	= &audio_a2dp_in_fops,
};

static int __init auda2dp_in_init(void)
{
	mutex_init(&the_audio_a2dp_in.lock);
	mutex_init(&the_audio_a2dp_in.read_lock);
	spin_lock_init(&the_audio_a2dp_in.dsp_lock);
	spin_lock_init(&the_audio_a2dp_in.dev_lock);
	init_waitqueue_head(&the_audio_a2dp_in.wait);
	init_waitqueue_head(&the_audio_a2dp_in.wait_enable);
	return misc_register(&audio_a2dp_in_misc);
}

device_initcall(auda2dp_in_init);

MODULE_DESCRIPTION("MSM SBC encode driver");
MODULE_LICENSE("GPL v2");

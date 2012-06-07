/* arch/arm/mach-msm/qdsp5/audio_in.c
 *
 * pcm audio input device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>

#include <linux/msm_audio_aac.h>

#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/msm_adsp.h>
#include <mach/msm_rpcrouter.h>

#include "audmgr.h"

#include <mach/qdsp5/qdsp5audpreproccmdi.h>
#include <mach/qdsp5/qdsp5audpreprocmsg.h>
#include <mach/qdsp5/qdsp5audreccmdi.h>
#include <mach/qdsp5/qdsp5audrecmsg.h>
#include <mach/debug_mm.h>

/* FRAME_NUM must be a power of two */
#define FRAME_NUM		(8)
#define FRAME_SIZE		(2052 * 2)
#define MONO_DATA_SIZE		(2048)
#define STEREO_DATA_SIZE	(MONO_DATA_SIZE * 2)
#define DMASZ 			(FRAME_SIZE * FRAME_NUM)

struct buffer {
	void *data;
	uint32_t size;
	uint32_t read;
	uint32_t addr;
};

struct audio_in {
	struct buffer in[FRAME_NUM];

	spinlock_t dsp_lock;

	atomic_t in_bytes;

	struct mutex lock;
	struct mutex read_lock;
	wait_queue_head_t wait;

	struct msm_adsp_module *audpre;
	struct msm_adsp_module *audrec;

	/* configuration to use on next enable */
	uint32_t samp_rate;
	uint32_t channel_mode;
	uint32_t buffer_size; /* 2048 for mono, 4096 for stereo */
	uint32_t type; /* 0 for PCM ,1 for AAC */
	uint32_t bit_rate; /* bit rate for AAC */
	uint32_t record_quality; /* record quality (bits/sample/channel)
				    for AAC*/
	uint32_t buffer_cfg_ioctl; /* to allow any one of buffer set ioctl */
	uint32_t dsp_cnt;
	uint32_t in_head; /* next buffer dsp will write */
	uint32_t in_tail; /* next buffer read() will read */
	uint32_t in_count; /* number of buffers available to read() */

	unsigned short samp_rate_index;

	struct audmgr audmgr;

	/* data allocated for various buffers */
	char *data;
	dma_addr_t phys;

	int opened;
	int enabled;
	int running;
	int stopped; /* set when stopped, cleared on flush */

	/* audpre settings */
	int tx_agc_enable;
	audpreproc_cmd_cfg_agc_params tx_agc_cfg;
	int ns_enable;
	audpreproc_cmd_cfg_ns_params ns_cfg;
	/* For different sample rate, the coeff might be different. *
	 * All the coeff should be passed from user space	    */
	int iir_enable;
	audpreproc_cmd_cfg_iir_tuning_filter_params iir_cfg;
};

static int audio_in_dsp_enable(struct audio_in *audio, int enable);
static int audio_in_encoder_config(struct audio_in *audio);
static int audio_dsp_read_buffer(struct audio_in *audio, uint32_t read_cnt);
static void audio_flush(struct audio_in *audio);
static int audio_dsp_set_tx_agc(struct audio_in *audio);
static int audio_dsp_set_ns(struct audio_in *audio);
static int audio_dsp_set_iir(struct audio_in *audio);

static unsigned convert_dsp_samp_index(unsigned index)
{
	switch (index) {
	case 48000:	return AUDREC_CMD_SAMP_RATE_INDX_48000;
	case 44100:	return AUDREC_CMD_SAMP_RATE_INDX_44100;
	case 32000:	return AUDREC_CMD_SAMP_RATE_INDX_32000;
	case 24000:	return AUDREC_CMD_SAMP_RATE_INDX_24000;
	case 22050:	return AUDREC_CMD_SAMP_RATE_INDX_22050;
	case 16000:	return AUDREC_CMD_SAMP_RATE_INDX_16000;
	case 12000:	return AUDREC_CMD_SAMP_RATE_INDX_12000;
	case 11025:	return AUDREC_CMD_SAMP_RATE_INDX_11025;
	case 8000:	return AUDREC_CMD_SAMP_RATE_INDX_8000;
	default: 	return AUDREC_CMD_SAMP_RATE_INDX_11025;
	}
}

static unsigned convert_samp_rate(unsigned hz)
{
	switch (hz) {
	case 48000: return RPC_AUD_DEF_SAMPLE_RATE_48000;
	case 44100: return RPC_AUD_DEF_SAMPLE_RATE_44100;
	case 32000: return RPC_AUD_DEF_SAMPLE_RATE_32000;
	case 24000: return RPC_AUD_DEF_SAMPLE_RATE_24000;
	case 22050: return RPC_AUD_DEF_SAMPLE_RATE_22050;
	case 16000: return RPC_AUD_DEF_SAMPLE_RATE_16000;
	case 12000: return RPC_AUD_DEF_SAMPLE_RATE_12000;
	case 11025: return RPC_AUD_DEF_SAMPLE_RATE_11025;
	case 8000:  return RPC_AUD_DEF_SAMPLE_RATE_8000;
	default:    return RPC_AUD_DEF_SAMPLE_RATE_11025;
	}
}

static unsigned convert_samp_index(unsigned index)
{
	switch (index) {
	case RPC_AUD_DEF_SAMPLE_RATE_48000:	return 48000;
	case RPC_AUD_DEF_SAMPLE_RATE_44100:	return 44100;
	case RPC_AUD_DEF_SAMPLE_RATE_32000:	return 32000;
	case RPC_AUD_DEF_SAMPLE_RATE_24000:	return 24000;
	case RPC_AUD_DEF_SAMPLE_RATE_22050:	return 22050;
	case RPC_AUD_DEF_SAMPLE_RATE_16000:	return 16000;
	case RPC_AUD_DEF_SAMPLE_RATE_12000:	return 12000;
	case RPC_AUD_DEF_SAMPLE_RATE_11025:	return 11025;
	case RPC_AUD_DEF_SAMPLE_RATE_8000:	return 8000;
	default: 				return 11025;
	}
}

/* must be called with audio->lock held */
static int audio_in_enable(struct audio_in *audio)
{
	struct audmgr_config cfg;
	int rc;

	if (audio->enabled)
		return 0;

	cfg.tx_rate = audio->samp_rate;
	cfg.rx_rate = RPC_AUD_DEF_SAMPLE_RATE_NONE;
	cfg.def_method = RPC_AUD_DEF_METHOD_RECORD;
	if (audio->type == AUDREC_CMD_TYPE_0_INDEX_WAV)
		cfg.codec = RPC_AUD_DEF_CODEC_PCM;
	else
		cfg.codec = RPC_AUD_DEF_CODEC_AAC;
	cfg.snd_method = RPC_SND_METHOD_MIDI;

	rc = audmgr_enable(&audio->audmgr, &cfg);
	if (rc < 0)
		return rc;

	if (msm_adsp_enable(audio->audpre)) {
		MM_ERR("msm_adsp_enable(audpre) failed\n");
		return -ENODEV;
	}
	if (msm_adsp_enable(audio->audrec)) {
		MM_ERR("msm_adsp_enable(audrec) failed\n");
		return -ENODEV;
	}

	audio->enabled = 1;
	audio_in_dsp_enable(audio, 1);

	return 0;
}

/* must be called with audio->lock held */
static int audio_in_disable(struct audio_in *audio)
{
	if (audio->enabled) {
		audio->enabled = 0;

		audio_in_dsp_enable(audio, 0);

		wake_up(&audio->wait);

		msm_adsp_disable(audio->audrec);
		msm_adsp_disable(audio->audpre);
		audmgr_disable(&audio->audmgr);
	}
	return 0;
}

/* ------------------- dsp --------------------- */
static void audpre_dsp_event(void *data, unsigned id, size_t len,
			    void (*getevent)(void *ptr, size_t len))
{
	uint16_t msg[2];
	getevent(msg, sizeof(msg));

	switch (id) {
	case AUDPREPROC_MSG_CMD_CFG_DONE_MSG:
		MM_INFO("type %d, status_flag %d\n", msg[0], msg[1]);
		break;
	case AUDPREPROC_MSG_ERROR_MSG_ID:
		MM_INFO("err_index %d\n", msg[0]);
		break;
	case ADSP_MESSAGE_ID:
		MM_DBG("Received ADSP event: module enable(audpreproctask)\n");
		break;
	default:
		MM_ERR("unknown event %d\n", id);
	}
}

struct audio_frame {
	uint16_t count_low;
	uint16_t count_high;
	uint16_t bytes;
	uint16_t unknown;
	unsigned char samples[];
} __attribute__((packed));

static void audio_in_get_dsp_frames(struct audio_in *audio)
{
	struct audio_frame *frame;
	uint32_t index;
	unsigned long flags;

		index = audio->in_head;

		/* XXX check for bogus frame size? */

		frame = (void *) (((char *)audio->in[index].data) -
				 sizeof(*frame));
		spin_lock_irqsave(&audio->dsp_lock, flags);
		audio->in[index].size = frame->bytes;

		audio->in_head = (audio->in_head + 1) & (FRAME_NUM - 1);

		/* If overflow, move the tail index foward. */
		if (audio->in_head == audio->in_tail)
			audio->in_tail = (audio->in_tail + 1) & (FRAME_NUM - 1);
		else
			audio->in_count++;

		audio_dsp_read_buffer(audio, audio->dsp_cnt++);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);

		wake_up(&audio->wait);
}

static void audrec_dsp_event(void *data, unsigned id, size_t len,
			    void (*getevent)(void *ptr, size_t len))
{
	struct audio_in *audio = data;
	uint16_t msg[3];
	getevent(msg, sizeof(msg));

	switch (id) {
	case AUDREC_MSG_CMD_CFG_DONE_MSG:
		if (msg[0] & AUDREC_MSG_CFG_DONE_TYPE_0_UPDATE) {
			if (msg[0] & AUDREC_MSG_CFG_DONE_TYPE_0_ENA) {
				MM_INFO("CFG ENABLED\n");
				audio_in_encoder_config(audio);
			} else {
				MM_INFO("CFG SLEEP\n");
				audio->running = 0;
				audio->tx_agc_enable = 0;
				audio->ns_enable = 0;
				audio->iir_enable = 0;
			}
		} else {
			MM_INFO("CMD_CFG_DONE %x\n", msg[0]);
		}
		break;
	case AUDREC_MSG_CMD_AREC_PARAM_CFG_DONE_MSG: {
		MM_INFO("PARAM CFG DONE\n");
		audio->running = 1;
		audio_dsp_set_tx_agc(audio);
		audio_dsp_set_ns(audio);
		audio_dsp_set_iir(audio);
		break;
	}
	case AUDREC_MSG_FATAL_ERR_MSG:
		MM_ERR("ERROR %x\n", msg[0]);
		break;
	case AUDREC_MSG_PACKET_READY_MSG:
/* REC_DBG("type %x, count %d", msg[0], (msg[1] | (msg[2] << 16))); */
		audio_in_get_dsp_frames(audio);
		break;
	case ADSP_MESSAGE_ID:
		MM_DBG("Received ADSP event: module \
				enable/disable(audrectask)\n");
		break;
	default:
		MM_ERR("unknown event %d\n", id);
	}
}

struct msm_adsp_ops audpre_adsp_ops = {
	.event = audpre_dsp_event,
};

struct msm_adsp_ops audrec_adsp_ops = {
	.event = audrec_dsp_event,
};


#define audio_send_queue_pre(audio, cmd, len) \
	msm_adsp_write(audio->audpre, QDSP_uPAudPreProcCmdQueue, cmd, len)
#define audio_send_queue_recbs(audio, cmd, len) \
	msm_adsp_write(audio->audrec, QDSP_uPAudRecBitStreamQueue, cmd, len)
#define audio_send_queue_rec(audio, cmd, len) \
	msm_adsp_write(audio->audrec, \
	QDSP_uPAudRecCmdQueue, cmd, len)

/* Convert Bit Rate to Record Quality field of DSP */
static unsigned int bitrate_to_record_quality(unsigned int sample_rate,
    unsigned int channel, unsigned int bit_rate) {
	unsigned int temp;

	temp = sample_rate * channel;
	MM_DBG(" sample rate *  channel = %d \n", temp);
	/* To represent in Q12 fixed format */
	temp = (bit_rate * 4096) / temp;
	MM_DBG(" Record Quality = 0x%8x \n", temp);
	return temp;
}

static int audio_dsp_set_tx_agc(struct audio_in *audio)
{
	audpreproc_cmd_cfg_agc_params cmd;

	memset(&cmd, 0, sizeof(cmd));

	audio->tx_agc_cfg.cmd_id = AUDPREPROC_CMD_CFG_AGC_PARAMS;
	if (audio->tx_agc_enable) {
		/* cmd.tx_agc_param_mask = 0xFE00 from sample code */
		audio->tx_agc_cfg.tx_agc_param_mask =
		(1 << AUDPREPROC_CMD_TX_AGC_PARAM_MASK_COMP_SLOPE) |
		(1 << AUDPREPROC_CMD_TX_AGC_PARAM_MASK_COMP_TH) |
		(1 << AUDPREPROC_CMD_TX_AGC_PARAM_MASK_EXP_SLOPE) |
		(1 << AUDPREPROC_CMD_TX_AGC_PARAM_MASK_EXP_TH) |
		(1 << AUDPREPROC_CMD_TX_AGC_PARAM_MASK_COMP_AIG_FLAG) |
		(1 << AUDPREPROC_CMD_TX_AGC_PARAM_MASK_COMP_STATIC_GAIN) |
		(1 << AUDPREPROC_CMD_TX_AGC_PARAM_MASK_TX_AGC_ENA_FLAG);
		audio->tx_agc_cfg.tx_agc_enable_flag =
			AUDPREPROC_CMD_TX_AGC_ENA_FLAG_ENA;
		/* cmd.param_mask = 0xFFF0 from sample code */
		audio->tx_agc_cfg.param_mask =
			(1 << AUDPREPROC_CMD_PARAM_MASK_RMS_TAY) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_RELEASEK) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_DELAY) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_ATTACKK) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_LEAKRATE_SLOW) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_LEAKRATE_FAST) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_AIG_RELEASEK) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_AIG_MIN) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_AIG_MAX) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_LEAK_UP) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_LEAK_DOWN) |
			(1 << AUDPREPROC_CMD_PARAM_MASK_AIG_ATTACKK);
	} else {
		audio->tx_agc_cfg.tx_agc_param_mask =
			(1 << AUDPREPROC_CMD_TX_AGC_PARAM_MASK_TX_AGC_ENA_FLAG);
		audio->tx_agc_cfg.tx_agc_enable_flag =
			AUDPREPROC_CMD_TX_AGC_ENA_FLAG_DIS;
	}
	cmd = audio->tx_agc_cfg;

	return audio_send_queue_pre(audio, &cmd, sizeof(cmd));
}

static int audio_enable_tx_agc(struct audio_in *audio, int enable)
{
	if (audio->tx_agc_enable != enable) {
		audio->tx_agc_enable = enable;
		if (audio->running)
			audio_dsp_set_tx_agc(audio);
	}
	return 0;
}

static int audio_dsp_set_ns(struct audio_in *audio)
{
	audpreproc_cmd_cfg_ns_params cmd;

	memset(&cmd, 0, sizeof(cmd));

	audio->ns_cfg.cmd_id = AUDPREPROC_CMD_CFG_NS_PARAMS;

	if (audio->ns_enable) {
		/* cmd.ec_mode_new is fixed as 0x0064 when enable
		 * from sample code */
		audio->ns_cfg.ec_mode_new =
			AUDPREPROC_CMD_EC_MODE_NEW_NS_ENA |
			AUDPREPROC_CMD_EC_MODE_NEW_HB_ENA |
			AUDPREPROC_CMD_EC_MODE_NEW_VA_ENA;
	} else {
		audio->ns_cfg.ec_mode_new =
			AUDPREPROC_CMD_EC_MODE_NEW_NLMS_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_DES_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_NS_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_CNI_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_NLES_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_HB_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_VA_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_PCD_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_FEHI_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_NEHI_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_NLPP_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_FNE_DIS |
			AUDPREPROC_CMD_EC_MODE_NEW_PRENLMS_DIS;
	}
	cmd = audio->ns_cfg;

	return audio_send_queue_pre(audio, &cmd, sizeof(cmd));
}

static int audio_enable_ns(struct audio_in *audio, int enable)
{
	if (audio->ns_enable != enable) {
		audio->ns_enable = enable;
		if (audio->running)
			audio_dsp_set_ns(audio);
	}
	return 0;
}

static int audio_dsp_set_iir(struct audio_in *audio)
{
	audpreproc_cmd_cfg_iir_tuning_filter_params cmd;

	memset(&cmd, 0, sizeof(cmd));

	audio->iir_cfg.cmd_id = AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS;

	if (audio->iir_enable)
		/* cmd.active_flag is 0xFFFF from sample code but 0x0001 here */
		audio->iir_cfg.active_flag = AUDPREPROC_CMD_IIR_ACTIVE_FLAG_ENA;
	else
		audio->iir_cfg.active_flag = AUDPREPROC_CMD_IIR_ACTIVE_FLAG_DIS;

	cmd = audio->iir_cfg;

	return audio_send_queue_pre(audio, &cmd, sizeof(cmd));
}

static int audio_enable_iir(struct audio_in *audio, int enable)
{
	if (audio->iir_enable != enable) {
		audio->iir_enable = enable;
		if (audio->running)
			audio_dsp_set_iir(audio);
	}
	return 0;
}

static int audio_in_dsp_enable(struct audio_in *audio, int enable)
{
	audrec_cmd_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_CFG;
	cmd.type_0 = enable ? AUDREC_CMD_TYPE_0_ENA : AUDREC_CMD_TYPE_0_DIS;
	cmd.type_0 |= (AUDREC_CMD_TYPE_0_UPDATE | audio->type);
	cmd.type_1 = 0;

	return audio_send_queue_rec(audio, &cmd, sizeof(cmd));
}

static int audio_in_encoder_config(struct audio_in *audio)
{
	audrec_cmd_arec0param_cfg cmd;
	uint16_t *data = (void *) audio->data;
	unsigned n;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_AREC0PARAM_CFG;
	cmd.ptr_to_extpkt_buffer_msw = audio->phys >> 16;
	cmd.ptr_to_extpkt_buffer_lsw = audio->phys;
	cmd.buf_len = FRAME_NUM; /* Both WAV and AAC use 8 frames */
	cmd.samp_rate_index = audio->samp_rate_index;
	cmd.stereo_mode = audio->channel_mode; /* 0 for mono, 1 for stereo */

	/* cmd.rec_quality is based on user set bit rate / sample rate /
	 * channel
	 */
	cmd.rec_quality = audio->record_quality;

	/* prepare buffer pointers:
	 * Mono: 1024 samples + 4 halfword header
	 * Stereo: 2048 samples + 4 halfword header
	 * AAC
	 * Mono/Stere: 768 + 4 halfword header
	 */
	for (n = 0; n < FRAME_NUM; n++) {
		audio->in[n].data = data + 4;
		if (audio->type == AUDREC_CMD_TYPE_0_INDEX_WAV)
			data += (4 + (audio->channel_mode ? 2048 : 1024));
		else if (audio->type == AUDREC_CMD_TYPE_0_INDEX_AAC)
			data += (4 + 768);
	}

	return audio_send_queue_rec(audio, &cmd, sizeof(cmd));
}

static int audio_dsp_read_buffer(struct audio_in *audio, uint32_t read_cnt)
{
	audrec_cmd_packet_ext_ptr cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_PACKET_EXT_PTR;
	/* Both WAV and AAC use AUDREC_CMD_TYPE_0 */
	cmd.type = AUDREC_CMD_TYPE_0;
	cmd.curr_rec_count_msw = read_cnt >> 16;
	cmd.curr_rec_count_lsw = read_cnt;

	return audio_send_queue_recbs(audio, &cmd, sizeof(cmd));
}

/* ------------------- device --------------------- */

static void audio_flush(struct audio_in *audio)
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
}

static long audio_in_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct audio_in *audio = file->private_data;
	int rc;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		stats.byte_count = atomic_read(&audio->in_bytes);
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START:
		rc = audio_in_enable(audio);
		break;
	case AUDIO_STOP:
		rc = audio_in_disable(audio);
		audio->stopped = 1;
		break;
	case AUDIO_FLUSH:
		if (audio->stopped) {
			/* Make sure we're stopped and we wake any threads
			 * that might be blocked holding the read_lock.
			 * While audio->stopped read threads will always
			 * exit immediately.
			 */
			wake_up(&audio->wait);
			mutex_lock(&audio->read_lock);
			audio_flush(audio);
			mutex_unlock(&audio->read_lock);
		}
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config cfg;
		/* The below code is to make mutual exclusive between
		 * AUDIO_SET_CONFIG and AUDIO_SET_STREAM_CONFIG.
		 * Allow any one IOCTL.
		 */
		if (audio->buffer_cfg_ioctl == AUDIO_SET_STREAM_CONFIG) {
			rc = -EINVAL;
			break;
		}
		if (copy_from_user(&cfg, (void *) arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}
		if (cfg.channel_count == 1) {
			cfg.channel_count = AUDREC_CMD_STEREO_MODE_MONO;
		} else if (cfg.channel_count == 2) {
			cfg.channel_count = AUDREC_CMD_STEREO_MODE_STEREO;
		} else {
			rc = -EINVAL;
			break;
		}

		if (cfg.type == 0) {
			cfg.type = AUDREC_CMD_TYPE_0_INDEX_WAV;
		} else if (cfg.type == 1) {
			cfg.type = AUDREC_CMD_TYPE_0_INDEX_AAC;
		} else {
			rc = -EINVAL;
			break;
		}
		audio->samp_rate = convert_samp_rate(cfg.sample_rate);
		audio->samp_rate_index =
		  convert_dsp_samp_index(cfg.sample_rate);
		audio->channel_mode = cfg.channel_count;
		audio->buffer_size =
				audio->channel_mode ? STEREO_DATA_SIZE
							: MONO_DATA_SIZE;
		audio->type = cfg.type;
		audio->buffer_cfg_ioctl = AUDIO_SET_CONFIG;
		rc = 0;
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config cfg;
		cfg.buffer_size = audio->buffer_size;
		cfg.buffer_count = FRAME_NUM;
		cfg.sample_rate = convert_samp_index(audio->samp_rate);
		if (audio->channel_mode == AUDREC_CMD_STEREO_MODE_MONO)
			cfg.channel_count = 1;
		else
			cfg.channel_count = 2;
		if (audio->type == AUDREC_CMD_TYPE_0_INDEX_WAV)
			cfg.type = 0;
		else
			cfg.type = 1;
		cfg.unused[0] = 0;
		cfg.unused[1] = 0;
		cfg.unused[2] = 0;
		if (copy_to_user((void *) arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}
	case AUDIO_GET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		cfg.buffer_size = audio->buffer_size;
		cfg.buffer_count = FRAME_NUM;
		if (copy_to_user((void *)arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}
	case AUDIO_SET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		/* The below code is to make mutual exclusive between
		 * AUDIO_SET_CONFIG and AUDIO_SET_STREAM_CONFIG.
		 * Allow any one IOCTL.
		 */
		if (audio->buffer_cfg_ioctl == AUDIO_SET_CONFIG) {
			rc = -EINVAL;
			break;
		}
		if (copy_from_user(&cfg, (void *)arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		} else
			rc = 0;
		audio->buffer_size = cfg.buffer_size;
		/* The IOCTL is only of AAC, set the encoder as AAC */
		audio->type = 1;
		audio->buffer_cfg_ioctl = AUDIO_SET_STREAM_CONFIG;
		break;
	}
	case AUDIO_GET_AAC_ENC_CONFIG: {
		struct msm_audio_aac_enc_config cfg;
		if (audio->channel_mode == AUDREC_CMD_STEREO_MODE_MONO)
			cfg.channels = 1;
		else
			cfg.channels = 2;
		cfg.sample_rate = convert_samp_index(audio->samp_rate);
		cfg.bit_rate = audio->bit_rate;
		cfg.stream_format = AUDIO_AAC_FORMAT_RAW;
		if (copy_to_user((void *)arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}
	case AUDIO_SET_AAC_ENC_CONFIG: {
		struct msm_audio_aac_enc_config cfg;
		unsigned int record_quality;
		if (copy_from_user(&cfg, (void *)arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}
		if (cfg.stream_format != AUDIO_AAC_FORMAT_RAW) {
			MM_ERR("unsupported AAC format\n");
			rc = -EINVAL;
			break;
		}
		record_quality = bitrate_to_record_quality(cfg.sample_rate,
					cfg.channels, cfg.bit_rate);
		/* Range of Record Quality Supported by DSP, Q12 format */
		if ((record_quality < 0x800) || (record_quality > 0x4000)) {
			MM_ERR("Unsupported bit rate \n");
			rc = -EINVAL;
			break;
		}
		if (cfg.channels == 1) {
			cfg.channels = AUDREC_CMD_STEREO_MODE_MONO;
		} else if (cfg.channels == 2) {
			cfg.channels = AUDREC_CMD_STEREO_MODE_STEREO;
		} else {
			rc = -EINVAL;
			break;
		}
		audio->samp_rate = convert_samp_rate(cfg.sample_rate);
		audio->samp_rate_index =
		  convert_dsp_samp_index(cfg.sample_rate);
		audio->channel_mode = cfg.channels;
		audio->bit_rate = cfg.bit_rate;
		audio->record_quality = record_quality;
		MM_DBG(" Record Quality = 0x%8x \n", audio->record_quality);
		rc = 0;
		break;
	}
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

static ssize_t audio_in_read(struct file *file,
				char __user *buf,
				size_t count, loff_t *pos)
{
	struct audio_in *audio = file->private_data;
	unsigned long flags;
	const char __user *start = buf;
	void *data;
	uint32_t index;
	uint32_t size;
	int rc = 0;

	mutex_lock(&audio->read_lock);
	while (count > 0) {
		rc = wait_event_interruptible(
			audio->wait, (audio->in_count > 0) || audio->stopped);
		if (rc < 0)
			break;

		if (audio->stopped && !audio->in_count) {
			rc = 0;/* End of File */
			break;
		}

		index = audio->in_tail;
		data = (uint8_t *) audio->in[index].data;
		size = audio->in[index].size;
		if (count >= size) {
			/* order the reads on the buffer */
			dma_coherent_post_ops();
			if (copy_to_user(buf, data, size)) {
				rc = -EFAULT;
				break;
			}
			spin_lock_irqsave(&audio->dsp_lock, flags);
			if (index != audio->in_tail) {
			/* overrun -- data is invalid and we need to retry */
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
		if (audio->type == AUDREC_CMD_TYPE_0_INDEX_AAC)
			break; /* AAC only read one frame */
	}
	mutex_unlock(&audio->read_lock);

	if (buf > start)
		return buf - start;

	return rc;
}

static ssize_t audio_in_write(struct file *file,
				const char __user *buf,
				size_t count, loff_t *pos)
{
	return -EINVAL;
}

static int audio_in_release(struct inode *inode, struct file *file)
{
	struct audio_in *audio = file->private_data;

	mutex_lock(&audio->lock);
	audio_in_disable(audio);
	audio_flush(audio);
	msm_adsp_put(audio->audrec);
	msm_adsp_put(audio->audpre);
	audio->audrec = NULL;
	audio->audpre = NULL;
	audio->opened = 0;
	mutex_unlock(&audio->lock);
	return 0;
}

struct audio_in the_audio_in;

static int audio_in_open(struct inode *inode, struct file *file)
{
	struct audio_in *audio = &the_audio_in;
	int rc;

	mutex_lock(&audio->lock);
	if (audio->opened) {
		rc = -EBUSY;
		goto done;
	}

	/* Settings will be re-config at AUDIO_SET_CONFIG,
	 * but at least we need to have initial config
	 */
	audio->samp_rate = RPC_AUD_DEF_SAMPLE_RATE_11025;
	audio->samp_rate_index = AUDREC_CMD_SAMP_RATE_INDX_11025;
	audio->channel_mode = AUDREC_CMD_STEREO_MODE_MONO;
	audio->buffer_size = MONO_DATA_SIZE;
	audio->type = AUDREC_CMD_TYPE_0_INDEX_WAV;

	/* For AAC, bit rate hard coded, default settings is
	 * sample rate (11025) x channel count (1) x recording quality (1.75)
	 * = 19293 bps  */
	audio->bit_rate = 19293;
	audio->record_quality = 0x1c00;

	rc = audmgr_open(&audio->audmgr);
	if (rc)
		goto done;
	rc = msm_adsp_get("AUDPREPROCTASK", &audio->audpre,
				&audpre_adsp_ops, audio);
	if (rc)
		goto done;
	rc = msm_adsp_get("AUDRECTASK", &audio->audrec,
			   &audrec_adsp_ops, audio);
	if (rc)
		goto done;

	audio->dsp_cnt = 0;
	audio->stopped = 0;
	audio->buffer_cfg_ioctl = 0; /* No valid ioctl set */

	audio_flush(audio);

	file->private_data = audio;
	audio->opened = 1;
	rc = 0;
done:
	mutex_unlock(&audio->lock);
	return rc;
}

static long audpre_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio_in *audio = file->private_data;
	int rc = 0, enable;
	uint16_t enable_mask;

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_ENABLE_AUDPRE:
		if (copy_from_user(&enable_mask, (void *) arg,
						sizeof(enable_mask))) {
			rc = -EFAULT;
			break;
		}

		enable = (enable_mask & AGC_ENABLE) ? 1 : 0;
		audio_enable_tx_agc(audio, enable);
		enable = (enable_mask & NS_ENABLE) ? 1 : 0;
		audio_enable_ns(audio, enable);
		enable = (enable_mask & TX_IIR_ENABLE) ? 1 : 0;
		audio_enable_iir(audio, enable);
		break;

	case AUDIO_SET_AGC:
		if (copy_from_user(&audio->tx_agc_cfg, (void *) arg,
						sizeof(audio->tx_agc_cfg)))
			rc = -EFAULT;
		break;

	case AUDIO_SET_NS:
		if (copy_from_user(&audio->ns_cfg, (void *) arg,
						sizeof(audio->ns_cfg)))
			rc = -EFAULT;
		break;

	case AUDIO_SET_TX_IIR:
		if (copy_from_user(&audio->iir_cfg, (void *) arg,
						sizeof(audio->iir_cfg)))
			rc = -EFAULT;
		break;

	default:
		rc = -EINVAL;
	}

	mutex_unlock(&audio->lock);
	return rc;
}

static int audpre_open(struct inode *inode, struct file *file)
{
	struct audio_in *audio = &the_audio_in;

	file->private_data = audio;

	return 0;
}

static struct file_operations audio_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_in_open,
	.release	= audio_in_release,
	.read		= audio_in_read,
	.write		= audio_in_write,
	.unlocked_ioctl	= audio_in_ioctl,
};

struct miscdevice audio_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_in",
	.fops	= &audio_fops,
};

static const struct file_operations audpre_fops = {
	.owner		= THIS_MODULE,
	.open		= audpre_open,
	.unlocked_ioctl	= audpre_ioctl,
};

struct miscdevice audpre_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_preproc_ctl",
	.fops	= &audpre_fops,
};

static int __init audio_in_init(void)
{
	the_audio_in.data = dma_alloc_coherent(NULL, DMASZ,
					       &the_audio_in.phys, GFP_KERNEL);
	if (!the_audio_in.data) {
		MM_ERR("Unable to allocate DMA buffer\n");
		return -ENOMEM;
	}

	mutex_init(&the_audio_in.lock);
	mutex_init(&the_audio_in.read_lock);
	spin_lock_init(&the_audio_in.dsp_lock);
	init_waitqueue_head(&the_audio_in.wait);
	return misc_register(&audio_in_misc) || misc_register(&audpre_misc);
}

device_initcall(audio_in_init);

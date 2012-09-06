/* arch/arm/mach-msm/qdsp5/audio_pcm_in.c
 *
 * pcm audio input device
 *
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This code is based in part on arch/arm/mach-msm/qdsp5v2/audio_pcm_in.c,
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
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
#include <linux/msm_ion.h>

#include <linux/delay.h>

#include <linux/msm_audio.h>

#include <mach/msm_memtypes.h>
#include <linux/memory_alloc.h>

#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/msm_adsp.h>
#include <mach/msm_rpcrouter.h>

#include "audmgr.h"

#include <mach/qdsp5/qdsp5audpreproc.h>
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
#define DMASZ			(FRAME_SIZE * FRAME_NUM)
#define MSM_AUD_BUFFER_UPDATE_WAIT_MS 2000

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

	struct msm_adsp_module *audrec;
	const char *module_name;
	unsigned queue_ids;
	uint16_t enc_id; /* Session Id */

	/* configuration to use on next enable */
	uint32_t samp_rate;
	uint32_t channel_mode;
	uint32_t buffer_size; /* 2048 for mono, 4096 for stereo */
	uint32_t enc_type; /* 0 for PCM */
	uint32_t mode; /* Tunnel for PCM */
	uint32_t dsp_cnt;
	uint32_t in_head; /* next buffer dsp will write */
	uint32_t in_tail; /* next buffer read() will read */
	uint32_t in_count; /* number of buffers available to read() */

	unsigned short samp_rate_index;
	uint32_t audrec_obj_idx ;

	struct audmgr audmgr;

	/* data allocated for various buffers */
	char *data;
	dma_addr_t phys;

	int opened;
	int enabled;
	int running;
	int stopped; /* set when stopped, cleared on flush */
	struct audrec_session_info session_info; /*audrec session info*/

	/* audpre settings */
	int tx_agc_enable;
	audpreproc_cmd_cfg_agc_params tx_agc_cfg;
	int ns_enable;
	audpreproc_cmd_cfg_ns_params ns_cfg;
	/* For different sample rate, the coeff might be different. *
	 * All the coeff should be passed from user space	    */
	int iir_enable;
	audpreproc_cmd_cfg_iir_tuning_filter_params iir_cfg;
	struct ion_client *client;
	struct ion_handle *output_buff_handle;
};

static int audpcm_in_dsp_enable(struct audio_in *audio, int enable);
static int audpcm_in_encmem_config(struct audio_in *audio);
static int audpcm_in_encparam_config(struct audio_in *audio);
static int audpcm_in_dsp_read_buffer(struct audio_in *audio, uint32_t read_cnt);
static void audpcm_in_flush(struct audio_in *audio);
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
	default:	return AUDREC_CMD_SAMP_RATE_INDX_11025;
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
	default:				return 11025;
	}
}

/* ------------------- dsp --------------------- */
static void audpre_dsp_event(void *data, unsigned id,  void *event_data)
{

	uint16_t *msg = event_data;

	if (!msg)
		return;

	switch (id) {
	case AUDPREPROC_MSG_CMD_CFG_DONE_MSG:
		MM_DBG("type %d, status_flag %d\n",\
			msg[0], msg[1]);
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


/* must be called with audio->lock held */
static int audpcm_in_enable(struct audio_in *audio)
{
	struct audmgr_config cfg;
	int rc;

	if (audio->enabled)
		return 0;

	cfg.tx_rate = audio->samp_rate;
	cfg.rx_rate = RPC_AUD_DEF_SAMPLE_RATE_NONE;
	cfg.def_method = RPC_AUD_DEF_METHOD_RECORD;
	cfg.codec = RPC_AUD_DEF_CODEC_PCM;
	cfg.snd_method = RPC_SND_METHOD_MIDI;

	rc = audmgr_enable(&audio->audmgr, &cfg);
	if (rc < 0)
		return rc;

	if (audpreproc_enable(audio->enc_id, &audpre_dsp_event, audio)) {
		MM_ERR("msm_adsp_enable(audpreproc) failed\n");
		audmgr_disable(&audio->audmgr);
		return -ENODEV;
	}

	if (msm_adsp_enable(audio->audrec)) {
		audpreproc_disable(audio->enc_id, audio);
		audmgr_disable(&audio->audmgr);
		MM_ERR("msm_adsp_enable(audrec) failed\n");
		return -ENODEV;
	}

	audio->enabled = 1;
	audpcm_in_dsp_enable(audio, 1);

	/*update aurec session info in audpreproc layer*/
	audio->session_info.session_id = audio->enc_id;
	audio->session_info.sampling_freq =
			convert_samp_index(audio->samp_rate);
	audpreproc_update_audrec_info(&audio->session_info);

	return 0;
}

/* must be called with audio->lock held */
static int audpcm_in_disable(struct audio_in *audio)
{
	if (audio->enabled) {
		audio->enabled = 0;

		audpcm_in_dsp_enable(audio, 0);

		audio->stopped = 1;
		wake_up(&audio->wait);

		msm_adsp_disable(audio->audrec);
		audpreproc_disable(audio->enc_id, audio);
		/*reset the sampling frequency information at audpreproc layer*/
		audio->session_info.sampling_freq = 0;
		audpreproc_update_audrec_info(&audio->session_info);
		audmgr_disable(&audio->audmgr);
	}
	return 0;
}

struct audio_frame {
	uint16_t count_low;
	uint16_t count_high;
	uint16_t bytes;
	uint16_t unknown;
	unsigned char samples[];
} __packed;

static void audpcm_in_get_dsp_frames(struct audio_in *audio)
{
	struct audio_frame *frame;
	uint32_t index;
	unsigned long flags;

	index = audio->in_head;

	frame = (void *) (((char *)audio->in[index].data) -
			sizeof(*frame));
	spin_lock_irqsave(&audio->dsp_lock, flags);
	audio->in[index].size = frame->bytes;

	audio->in_head = (audio->in_head + 1) & (FRAME_NUM - 1);

	/* If overflow, move the tail index foward. */
	if (audio->in_head == audio->in_tail) {
		audio->in_tail = (audio->in_tail + 1) & (FRAME_NUM - 1);
		MM_ERR("Error! not able to keep up the read\n");
	} else
		audio->in_count++;

	audpcm_in_dsp_read_buffer(audio, audio->dsp_cnt++);
	spin_unlock_irqrestore(&audio->dsp_lock, flags);

	wake_up(&audio->wait);
}

static void audrec_dsp_event(void *data, unsigned id, size_t len,
			    void (*getevent)(void *ptr, size_t len))
{
	struct audio_in *audio = NULL;
	uint16_t msg[3];

	if (data)
		audio = data;
	else {
		MM_ERR("invalid data for event %x\n", id);
		return;
	}

	getevent(msg, sizeof(msg));

	switch (id) {
	case AUDREC_MSG_CMD_CFG_DONE_MSG: {
		if (msg[0] & AUDREC_MSG_CFG_DONE_ENC_ENA) {
			audio->audrec_obj_idx = msg[1];
			MM_INFO("CFG ENABLED\n");
			audpcm_in_encmem_config(audio);
		} else {
			MM_INFO("CFG SLEEP\n");
			audio->running = 0;
			audio->tx_agc_enable = 0;
			audio->ns_enable = 0;
			audio->iir_enable = 0;
		}
		break;
	}
	case AUDREC_MSG_CMD_AREC_MEM_CFG_DONE_MSG: {
		MM_DBG("AREC_MEM_CFG_DONE_MSG\n");
		audpcm_in_encparam_config(audio);
		break;
	}
	case AUDREC_MSG_CMD_AREC_PARAM_CFG_DONE_MSG: {
		MM_INFO("PARAM CFG DONE\n");
		audio->running = 1;
		audio_dsp_set_tx_agc(audio);
		audio_dsp_set_ns(audio);
		audio_dsp_set_iir(audio);
		break;
	}
	case AUDREC_MSG_NO_EXT_PKT_AVAILABLE_MSG: {
		MM_DBG("ERROR %x\n", msg[0]);
		break;
	}
	case AUDREC_MSG_PACKET_READY_MSG: {
		struct audrec_msg_packet_ready_msg pkt_ready_msg;

		getevent(&pkt_ready_msg, AUDREC_MSG_PACKET_READY_MSG_LEN);
		MM_DBG("UP_PACKET_READY_MSG: write cnt msw  %d \
		write cnt lsw %d read cnt msw %d  read cnt lsw %d \n",\
		pkt_ready_msg.pkt_counter_msw, \
		pkt_ready_msg.pkt_counter_lsw, \
		pkt_ready_msg.pkt_read_cnt_msw, \
		pkt_ready_msg.pkt_read_cnt_lsw);

		audpcm_in_get_dsp_frames(audio);
		break;
	}
	case ADSP_MESSAGE_ID: {
		MM_DBG("Received ADSP event: module \
				enable/disable(audrectask)\n");
		break;
	}
	default:
		MM_ERR("unknown event %d\n", id);
	}
}

static struct msm_adsp_ops audrec_adsp_ops = {
	.event = audrec_dsp_event,
};

#define audio_send_queue_recbs(audio, cmd, len) \
	msm_adsp_write(audio->audrec, ((audio->queue_ids & 0xFFFF0000) >> 16),\
			cmd, len)
#define audio_send_queue_rec(audio, cmd, len) \
	msm_adsp_write(audio->audrec, (audio->queue_ids & 0x0000FFFF),\
			cmd, len)

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
		audio->tx_agc_cfg.tx_agc_param_mask =
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

	return audpreproc_dsp_set_agc(&cmd, sizeof(cmd));
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

	return audpreproc_dsp_set_ns(&cmd, sizeof(cmd));
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

	return audpreproc_dsp_set_iir(&cmd, sizeof(cmd));
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

static int audpcm_in_dsp_enable(struct audio_in *audio, int enable)
{
	struct audrec_cmd_enc_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_ENC_CFG;

	cmd.audrec_enc_type = (audio->enc_type & 0xFF) |
	(enable ? AUDREC_CMD_ENC_ENA : AUDREC_CMD_ENC_DIS);
	/* Don't care */
	cmd.audrec_obj_idx = audio->audrec_obj_idx;

	return audio_send_queue_rec(audio, &cmd, sizeof(cmd));
}

static int audpcm_in_encmem_config(struct audio_in *audio)
{
	struct audrec_cmd_arecmem_cfg cmd;
	uint16_t cnt = 0;
	uint16_t *data = (void *) audio->data;

	memset(&cmd, 0, sizeof(cmd));

	cmd.cmd_id = AUDREC_CMD_ARECMEM_CFG;
	cmd.audrec_obj_idx = audio->audrec_obj_idx;
	/* Rate at which packet complete message comes */
	cmd.audrec_up_pkt_intm_cnt = 1;
	cmd.audrec_extpkt_buffer_msw = audio->phys >> 16;
	cmd.audrec_extpkt_buffer_lsw = audio->phys;
	/* Max Buffer no available for frames */
	cmd.audrec_extpkt_buffer_num = FRAME_NUM;

	/* prepare buffer pointers:
	 * Mono: 1024 samples + 4 halfword header
	 * Stereo: 2048 samples + 4 halfword header
	 */
	for (cnt = 0; cnt < FRAME_NUM; cnt++) {
		audio->in[cnt].data = data + 4;
			data += (4 + (audio->channel_mode ? 2048 : 1024));
	}

	return audio_send_queue_rec(audio, &cmd, sizeof(cmd));
}

static int audpcm_in_encparam_config(struct audio_in *audio)
{
	struct audrec_cmd_arecparam_wav_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDREC_CMD_ARECPARAM_CFG;
	cmd.common.audrec_obj_idx = audio->audrec_obj_idx;
	cmd.samp_rate_idx = audio->samp_rate_index;
	cmd.stereo_mode = audio->channel_mode; /* 0 for mono, 1 for stereo */

	return audio_send_queue_rec(audio, &cmd, sizeof(cmd));
}

static int audpcm_in_dsp_read_buffer(struct audio_in *audio, uint32_t read_cnt)
{
	audrec_cmd_packet_ext_ptr cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AUDREC_CMD_PACKET_EXT_PTR;
	cmd.type = audio->audrec_obj_idx;
	cmd.curr_rec_count_msw = read_cnt >> 16;
	cmd.curr_rec_count_lsw = read_cnt;

	return audio_send_queue_recbs(audio, &cmd, sizeof(cmd));
}

/* ------------------- device --------------------- */

static void audpcm_in_flush(struct audio_in *audio)
{
	int i;

	audio->dsp_cnt = 0;
	audio->in_head = 0;
	audio->in_tail = 0;
	audio->in_count = 0;
	for (i = FRAME_NUM-1; i >= 0; i--) {
		audio->in[i].size = 0;
		audio->in[i].read = 0;
	}
}

static long audpcm_in_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct audio_in *audio = file->private_data;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		stats.byte_count = atomic_read(&audio->in_bytes);
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START: {
		rc = audpcm_in_enable(audio);
		audio->stopped = 0;
		break;
	}
	case AUDIO_STOP:
		rc = audpcm_in_disable(audio);
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
			audpcm_in_flush(audio);
			mutex_unlock(&audio->read_lock);
		}
		break;
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config cfg;

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

		audio->samp_rate = convert_samp_rate(cfg.sample_rate);
		audio->samp_rate_index =
		  convert_dsp_samp_index(cfg.sample_rate);
		audio->channel_mode = cfg.channel_count;
		audio->buffer_size =
				audio->channel_mode ? STEREO_DATA_SIZE
							: MONO_DATA_SIZE;
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
		cfg.type = 0;
		cfg.unused[0] = 0;
		cfg.unused[1] = 0;
		cfg.unused[2] = 0;
		if (copy_to_user((void *) arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		break;
	}
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

static ssize_t audpcm_in_read(struct file *file,
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
		rc = wait_event_interruptible_timeout(
			audio->wait, (audio->in_count > 0) || audio->stopped,
			msecs_to_jiffies(MSM_AUD_BUFFER_UPDATE_WAIT_MS));
		if (rc == 0) {
			rc = -ETIMEDOUT;
			break;
		} else if (rc < 0) {
			break;
		}

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
				/* overrun -- data is invalid and we need to
				 * retry
				 */
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

static ssize_t audpcm_in_write(struct file *file,
				const char __user *buf,
				size_t count, loff_t *pos)
{
	return -EINVAL;
}

static int audpcm_in_release(struct inode *inode, struct file *file)
{
	struct audio_in *audio = file->private_data;

	mutex_lock(&audio->lock);
	audpcm_in_disable(audio);
	audpcm_in_flush(audio);
	audpreproc_aenc_free(audio->enc_id);
	msm_adsp_put(audio->audrec);
	audio->audrec = NULL;
	audio->opened = 0;
	if (audio->data) {
		ion_unmap_kernel(audio->client, audio->output_buff_handle);
		ion_free(audio->client, audio->output_buff_handle);
		audio->data = NULL;
	}
	ion_client_destroy(audio->client);
	mutex_unlock(&audio->lock);
	return 0;
}

static struct audio_in the_audio_in;

static int audpcm_in_open(struct inode *inode, struct file *file)
{
	struct audio_in *audio = &the_audio_in;
	int rc;
	int len = 0;
	unsigned long ionflag = 0;
	ion_phys_addr_t addr = 0;
	struct ion_handle *handle = NULL;
	struct ion_client *client = NULL;

	int encid;
	mutex_lock(&audio->lock);
	if (audio->opened) {
		rc = -EBUSY;
		goto done;
	}

	/* Settings will be re-config at AUDIO_SET_CONFIG,
	 * but at least we need to have initial config
	 */
	audio->mode = MSM_AUD_ENC_MODE_TUNNEL;
	audio->samp_rate = RPC_AUD_DEF_SAMPLE_RATE_11025;
	audio->samp_rate_index = AUDREC_CMD_SAMP_RATE_INDX_11025;
	audio->channel_mode = AUDREC_CMD_STEREO_MODE_MONO;
	audio->buffer_size = MONO_DATA_SIZE;
	audio->enc_type = AUDREC_CMD_TYPE_0_INDEX_WAV | audio->mode;

	rc = audmgr_open(&audio->audmgr);
	if (rc)
		goto done;
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

	audio->dsp_cnt = 0;
	audio->stopped = 0;

	audpcm_in_flush(audio);

	client = msm_ion_client_create(UINT_MAX, "Audio_PCM_in_client");
	if (IS_ERR_OR_NULL(client)) {
		MM_ERR("Unable to create ION client\n");
		rc = -ENOMEM;
		goto client_create_error;
	}
	audio->client = client;

	MM_DBG("allocating mem sz = %d\n", DMASZ);
	handle = ion_alloc(client, DMASZ, SZ_4K,
		ION_HEAP(ION_AUDIO_HEAP_ID));
	if (IS_ERR_OR_NULL(handle)) {
		MM_ERR("Unable to create allocate O/P buffers\n");
		rc = -ENOMEM;
		goto output_buff_alloc_error;
	}

	audio->output_buff_handle = handle;

	rc = ion_phys(client , handle, &addr, &len);
	if (rc) {
		MM_ERR("O/P buffers:Invalid phy: %x sz: %x\n",
			(unsigned int) addr, (unsigned int) len);
		rc = -ENOMEM;
		goto output_buff_get_phys_error;
	} else {
		MM_INFO("O/P buffers:valid phy: %x sz: %x\n",
			(unsigned int) addr, (unsigned int) len);
	}
	audio->phys = (int32_t)addr;

	rc = ion_handle_get_flags(client, handle, &ionflag);
	if (rc) {
		MM_ERR("could not get flags for the handle\n");
		rc = -ENOMEM;
		goto output_buff_get_flags_error;
	}

	audio->data = ion_map_kernel(client, handle, ionflag);
	if (IS_ERR(audio->data)) {
		MM_ERR("could not map read buffers,freeing instance 0x%08x\n",
				(int)audio);
		rc = -ENOMEM;
		goto output_buff_map_error;
	}
	MM_DBG("read buf: phy addr 0x%08x kernel addr 0x%08x\n",
		audio->phys, (int)audio->data);

	file->private_data = audio;
	audio->opened = 1;
	rc = 0;
done:
	mutex_unlock(&audio->lock);
	return rc;
output_buff_map_error:
output_buff_get_phys_error:
output_buff_get_flags_error:
	ion_free(client, audio->output_buff_handle);
output_buff_alloc_error:
	ion_client_destroy(client);
client_create_error:
	msm_adsp_put(audio->audrec);
	audpreproc_aenc_free(audio->enc_id);
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

static const struct file_operations audio_fops = {
	.owner		= THIS_MODULE,
	.open		= audpcm_in_open,
	.release	= audpcm_in_release,
	.read		= audpcm_in_read,
	.write		= audpcm_in_write,
	.unlocked_ioctl	= audpcm_in_ioctl,
};

static struct miscdevice audpcm_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_in",
	.fops	= &audio_fops,
};

static const struct file_operations audpre_fops = {
	.owner		= THIS_MODULE,
	.open		= audpre_open,
	.unlocked_ioctl	= audpre_ioctl,
};

static struct miscdevice audpre_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_preproc_ctl",
	.fops	= &audpre_fops,
};

static int __init audpcm_in_init(void)
{

	mutex_init(&the_audio_in.lock);
	mutex_init(&the_audio_in.read_lock);
	spin_lock_init(&the_audio_in.dsp_lock);
	init_waitqueue_head(&the_audio_in.wait);
	return misc_register(&audpcm_in_misc) || misc_register(&audpre_misc);
}
device_initcall(audpcm_in_init);

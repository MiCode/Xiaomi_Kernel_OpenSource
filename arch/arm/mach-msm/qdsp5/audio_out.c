/* arch/arm/mach-msm/qdsp5/audio_out.c
 *
 * pcm audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/pm_qos.h>

#include <linux/msm_audio.h>

#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/msm_adsp.h>
#include <mach/cpuidle.h>

#include "audmgr.h"

#include <mach/qdsp5/audio_acdb_def.h>
#include <mach/qdsp5/qdsp5audppcmdi.h>
#include <mach/qdsp5/qdsp5audppmsg.h>
#include <mach/qdsp5/qdsp5audpp.h>

#include <mach/htc_pwrsink.h>
#include <mach/debug_mm.h>

#include "evlog.h"

#define LOG_AUDIO_EVENTS 1
#define LOG_AUDIO_FAULTS 0

#define SRS_ID_GLOBAL			0x00000001
#define SRS_ID_WOWHD			0x00000002
#define SRS_ID_CSHP			0x00000003
#define SRS_ID_HPF			0x00000004
#define SRS_ID_PEQ			0x00000005
#define SRS_ID_HL			0x00000006

#define SRS_MASK_G 1
#define SRS_MASK_W 2
#define SRS_MASK_C 4
#define SRS_MASK_HP 8
#define SRS_MASK_P 16
#define SRS_MASK_HL 32


enum {
	EV_NULL,
	EV_OPEN,
	EV_WRITE,
	EV_RETURN,
	EV_IOCTL,
	EV_WRITE_WAIT,
	EV_WAIT_EVENT,
	EV_FILL_BUFFER,
	EV_SEND_BUFFER,
	EV_DSP_EVENT,
	EV_ENABLE,
};

#if (LOG_AUDIO_EVENTS != 1)
static inline void LOG(unsigned id, unsigned arg) {}
#else
static const char *pcm_log_strings[] = {
	"NULL",
	"OPEN",
	"WRITE",
	"RETURN",
	"IOCTL",
	"WRITE_WAIT",
	"WAIT_EVENT",
	"FILL_BUFFER",
	"SEND_BUFFER",
	"DSP_EVENT",
	"ENABLE",
};

DECLARE_LOG(pcm_log, 64, pcm_log_strings);

static int __init _pcm_log_init(void)
{
	return ev_log_init(&pcm_log);
}
module_init(_pcm_log_init);

#define LOG(id,arg) ev_log_write(&pcm_log, id, arg)
#endif





#define BUFSZ (5248)
#define DMASZ (BUFSZ * 2)

#define COMMON_OBJ_ID 6

struct buffer {
	void *data;
	unsigned size;
	unsigned used;
	unsigned addr;
};

struct audio {
	struct buffer out[2];

	spinlock_t dsp_lock;

	uint8_t out_head;
	uint8_t out_tail;
	uint8_t out_needed; /* number of buffers the dsp is waiting for */

	atomic_t out_bytes;

	struct mutex lock;
	struct mutex write_lock;
	wait_queue_head_t wait;

	/* configuration to use on next enable */
	uint32_t out_sample_rate;
	uint32_t out_channel_mode;
	uint32_t out_weight;
	uint32_t out_buffer_size;

	struct audmgr audmgr;

	/* data allocated for various buffers */
	char *data;
	dma_addr_t phys;

	int teos; /* valid only if tunnel mode & no data left for decoder */
	int opened;
	int enabled;
	int running;
	int stopped; /* set when stopped, cleared on flush */

	struct wake_lock wakelock;
	struct pm_qos_request pm_qos_req;

	audpp_cmd_cfg_object_params_volume vol_pan;
};

struct audio_copp {
	int mbadrc_enable;
	int mbadrc_needs_commit;
	char *mbadrc_data;
	dma_addr_t mbadrc_phys;

	audpp_cmd_cfg_object_params_mbadrc mbadrc;

	int eq_enable;
	int eq_needs_commit;
	audpp_cmd_cfg_object_params_eqalizer eq;

	int rx_iir_enable;
	int rx_iir_needs_commit;
	audpp_cmd_cfg_object_params_pcm iir;

	audpp_cmd_cfg_object_params_volume vol_pan;

	int qconcert_plus_enable;
	int qconcert_plus_needs_commit;

	int srs_enable;
	int srs_needs_commit;
	int srs_feature_mask;
	audpp_cmd_cfg_object_params_qconcert qconcert_plus;
	int srs_current_feature_mask;
	uint32_t audpp_disabled_features;

	int status;
	int opened;
	struct mutex lock;

	struct audpp_event_callback ecb;

	struct audpp_cmd_cfg_object_params_srstm_g g;
	struct audpp_cmd_cfg_object_params_srstm_w w;
	struct audpp_cmd_cfg_object_params_srstm_c c;
	struct audpp_cmd_cfg_object_params_srstm_h h;
	struct audpp_cmd_cfg_object_params_srstm_p p;
	struct audpp_cmd_cfg_object_params_srstm_l l;
} the_audio_copp;

static void audio_prevent_sleep(struct audio *audio)
{
	MM_DBG("\n"); /* Macro prints the file name and function */
	wake_lock(&audio->wakelock);
	pm_qos_update_request(&audio->pm_qos_req,
			      msm_cpuidle_get_deep_idle_latency());
}

static void audio_allow_sleep(struct audio *audio)
{
	pm_qos_update_request(&audio->pm_qos_req, PM_QOS_DEFAULT_VALUE);
	wake_unlock(&audio->wakelock);
	MM_DBG("\n"); /* Macro prints the file name and function */
}

static int audio_dsp_out_enable(struct audio *audio, int yes);
static int audio_dsp_send_buffer(struct audio *audio, unsigned id, unsigned len);

static void audio_dsp_event(void *private, unsigned id, uint16_t *msg);
static int audio_enable_srs_trumedia(struct audio_copp *audio_copp, int enable);
/* must be called with audio->lock held */
static int audio_enable(struct audio *audio)
{
	struct audmgr_config cfg;
	int rc;

	MM_DBG("\n"); /* Macro prints the file name and function */

	if (audio->enabled)
		return 0;	

	/* refuse to start if we're not ready */
	if (!audio->out[0].used || !audio->out[1].used)
		return -EIO;

	/* we start buffers 0 and 1, so buffer 0 will be the
	 * next one the dsp will want
	 */
	audio->out_tail = 0;
	audio->out_needed = 0;

	cfg.tx_rate = RPC_AUD_DEF_SAMPLE_RATE_NONE;
	cfg.rx_rate = RPC_AUD_DEF_SAMPLE_RATE_48000;
	cfg.def_method = RPC_AUD_DEF_METHOD_HOST_PCM;
	cfg.codec = RPC_AUD_DEF_CODEC_PCM;
	cfg.snd_method = RPC_SND_METHOD_MIDI;

	audio_prevent_sleep(audio);	
	rc = audmgr_enable(&audio->audmgr, &cfg);
	if (rc < 0) {
		audio_allow_sleep(audio);
		return rc;
	}

	if (audpp_enable(-1, audio_dsp_event, audio)) {
		MM_ERR("audpp_enable() failed\n");
		audmgr_disable(&audio->audmgr);
		audio_allow_sleep(audio);
		return -ENODEV;
	}

	audio->enabled = 1;
	htc_pwrsink_set(PWRSINK_AUDIO, 100);
	return 0;
}

/* must be called with audio->lock held */
static int audio_disable(struct audio *audio)
{
	MM_DBG("\n"); /* Macro prints the file name and function */
	if (audio->enabled) {
		audio->enabled = 0;
		audio_dsp_out_enable(audio, 0);

		audpp_disable(-1, audio);

		audio->stopped = 1;
		wake_up(&audio->wait);
		audmgr_disable(&audio->audmgr);
		audio->out_needed = 0;
		audio_allow_sleep(audio);
	}
	return 0;
}

void audio_commit_pending_pp_params(void *priv, unsigned id, uint16_t *msg)
{
	struct audio_copp *audio_copp = priv;

	if (audio_copp == NULL) {
		MM_ERR("NULL audio copp pointer\n");
		return;
	}

	if (AUDPP_MSG_CFG_MSG == id && msg[0] == AUDPP_MSG_ENA_DIS) {
		audio_copp->audpp_disabled_features = 0;
		return;
	}
	if (AUDPP_MSG_CFG_MSG == id && msg[0] == AUDPP_MSG_ENA_ENA)
		audio_copp->audpp_disabled_features = 0;

	if (!audio_copp->status)
		return;

	if (id == AUDPP_MSG_PP_DISABLE_FEEDBACK) {
		audio_copp->audpp_disabled_features |=
			((uint32_t)(msg[AUDPP_DISABLE_FEATS_MSW] << 16) |
			 msg[AUDPP_DISABLE_FEATS_LSW]);
		MM_DBG("AUDPP disable feedback: %x",
				audio_copp->audpp_disabled_features);
		return;
	} else if (id == AUDPP_MSG_PP_FEATS_RE_ENABLE) {
		MM_DBG("AUDPP re-enable messaage: %x, acdb_enabled %d",
			audio_copp->audpp_disabled_features, is_acdb_enabled());
		if (!is_acdb_enabled()) {
			if ((audio_copp->audpp_disabled_features &
				(1 << AUDPP_CMD_MBADRC)) ==
				(1 << AUDPP_CMD_MBADRC)) {
				audpp_dsp_set_mbadrc(COMMON_OBJ_ID,
						audio_copp->mbadrc_enable,
						&audio_copp->mbadrc);
			}
			if ((audio_copp->audpp_disabled_features &
				(1 << AUDPP_CMD_EQUALIZER)) ==
				(1 << AUDPP_CMD_EQUALIZER)) {
				audpp_dsp_set_eq(COMMON_OBJ_ID,
						audio_copp->eq_enable,
						&audio_copp->eq);
			}
			if ((audio_copp->audpp_disabled_features &
				(1 << AUDPP_CMD_IIR_TUNING_FILTER)) ==
				(1 << AUDPP_CMD_IIR_TUNING_FILTER)) {
				audpp_dsp_set_rx_iir(COMMON_OBJ_ID,
						audio_copp->rx_iir_enable,
						&audio_copp->iir);
			}
			if ((audio_copp->audpp_disabled_features &
				(1 << AUDPP_CMD_QCONCERT)) ==
					(1 << AUDPP_CMD_QCONCERT)) {
				audpp_dsp_set_qconcert_plus(COMMON_OBJ_ID,
					audio_copp->qconcert_plus_enable,
					&audio_copp->qconcert_plus);
			}
		}
		if ((audio_copp->audpp_disabled_features & (1 << AUDPP_CMD_SRS))
			== (1 << AUDPP_CMD_SRS)) {
			if (audio_copp->srs_current_feature_mask & SRS_MASK_W)
				audpp_dsp_set_rx_srs_trumedia_w(&audio_copp->w);
			if (audio_copp->srs_current_feature_mask & SRS_MASK_C)
				audpp_dsp_set_rx_srs_trumedia_c(&audio_copp->c);
			if (audio_copp->srs_current_feature_mask & SRS_MASK_HP)
				audpp_dsp_set_rx_srs_trumedia_h(&audio_copp->h);
			if (audio_copp->srs_current_feature_mask & SRS_MASK_P)
				audpp_dsp_set_rx_srs_trumedia_p(&audio_copp->p);
			if (audio_copp->srs_current_feature_mask & SRS_MASK_HL)
				audpp_dsp_set_rx_srs_trumedia_l(&audio_copp->l);
			if (audio_copp->srs_current_feature_mask & SRS_MASK_G)
				audpp_dsp_set_rx_srs_trumedia_g(&audio_copp->g);
		}
		audio_copp->audpp_disabled_features = 0;
		return;
	}

	if (!is_acdb_enabled()) {
		audpp_dsp_set_mbadrc(COMMON_OBJ_ID, audio_copp->mbadrc_enable,
						&audio_copp->mbadrc);

		audpp_dsp_set_eq(COMMON_OBJ_ID, audio_copp->eq_enable,
						&audio_copp->eq);
		audpp_dsp_set_rx_iir(COMMON_OBJ_ID, audio_copp->rx_iir_enable,
							&audio_copp->iir);
		audpp_dsp_set_vol_pan(COMMON_OBJ_ID, &audio_copp->vol_pan);

		audpp_dsp_set_qconcert_plus(COMMON_OBJ_ID,
				audio_copp->qconcert_plus_enable,
				&audio_copp->qconcert_plus);
	}
	audio_enable_srs_trumedia(audio_copp, true);
}
EXPORT_SYMBOL(audio_commit_pending_pp_params);

/* ------------------- dsp --------------------- */
static void audio_dsp_event(void *private, unsigned id, uint16_t *msg)
{
	struct audio *audio = private;
	struct buffer *frame;
	unsigned long flags;

	LOG(EV_DSP_EVENT, id);
	switch (id) {
	case AUDPP_MSG_HOST_PCM_INTF_MSG: {
		unsigned id = msg[2];
		unsigned idx = msg[3] - 1;

		/* MM_INFO("HOST_PCM id %d idx %d\n", id, idx); */
		if (id != AUDPP_MSG_HOSTPCM_ID_ARM_RX) {
			MM_ERR("bogus id\n");
			break;
		}
		if (idx > 1) {
			MM_ERR("bogus buffer idx\n");
			break;
		}

		spin_lock_irqsave(&audio->dsp_lock, flags);
		if (audio->running) {
			atomic_add(audio->out[idx].used, &audio->out_bytes);
			audio->out[idx].used = 0;

			frame = audio->out + audio->out_tail;
			if (frame->used) {
				audio_dsp_send_buffer(
					audio, audio->out_tail, frame->used);
				audio->out_tail ^= 1;
			} else {
				audio->out_needed++;
			}
			wake_up(&audio->wait);
		}
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		break;
	}
	case AUDPP_MSG_PCMDMAMISSED:
		MM_INFO("PCMDMAMISSED %d\n", msg[0]);
		audio->teos = 1;
		wake_up(&audio->wait);
		break;
	case AUDPP_MSG_CFG_MSG:
		if (msg[0] == AUDPP_MSG_ENA_ENA) {
			LOG(EV_ENABLE, 1);
			MM_DBG("CFG_MSG ENABLE\n");
			audio->out_needed = 0;
			audio->running = 1;
			audpp_dsp_set_vol_pan(5, &audio->vol_pan);
			audio_dsp_out_enable(audio, 1);
		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			LOG(EV_ENABLE, 0);
			MM_DBG("CFG_MSG DISABLE\n");
			audio->running = 0;
		} else {
			MM_ERR("CFG_MSG %d?\n", msg[0]);
		}
		break;
	default:
		MM_ERR("UNKNOWN (%d)\n", id);
	}
}

static int audio_dsp_out_enable(struct audio *audio, int yes)
{
	audpp_cmd_pcm_intf cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id	= AUDPP_CMD_PCM_INTF_2; 
	cmd.object_num	= AUDPP_CMD_PCM_INTF_OBJECT_NUM;
	cmd.config	= AUDPP_CMD_PCM_INTF_CONFIG_CMD_V;
	cmd.intf_type	= AUDPP_CMD_PCM_INTF_RX_ENA_ARMTODSP_V;

	if (yes) {
		cmd.write_buf1LSW	= audio->out[0].addr;
		cmd.write_buf1MSW	= audio->out[0].addr >> 16;
		if (audio->out[0].used)
			cmd.write_buf1_len	= audio->out[0].used;
		else
			cmd.write_buf1_len	= audio->out[0].size;
		cmd.write_buf2LSW	= audio->out[1].addr;
		cmd.write_buf2MSW	= audio->out[1].addr >> 16;
		if (audio->out[1].used)
			cmd.write_buf2_len	= audio->out[1].used;
		else
			cmd.write_buf2_len	= audio->out[1].size;
		cmd.arm_to_rx_flag	= AUDPP_CMD_PCM_INTF_ENA_V;
		cmd.weight_decoder_to_rx = audio->out_weight;
		cmd.weight_arm_to_rx	= 1;
		cmd.partition_number_arm_to_dsp = 0;
		cmd.sample_rate		= audio->out_sample_rate;
		cmd.channel_mode	= audio->out_channel_mode;
	}
	
	return audpp_send_queue2(&cmd, sizeof(cmd));
}

static int audio_dsp_send_buffer(struct audio *audio, unsigned idx, unsigned len)
{
	audpp_cmd_pcm_intf_send_buffer cmd;
	
	cmd.cmd_id		= AUDPP_CMD_PCM_INTF_2;
	cmd.host_pcm_object	= AUDPP_CMD_PCM_INTF_OBJECT_NUM;
	cmd.config		= AUDPP_CMD_PCM_INTF_BUFFER_CMD_V;
	cmd.intf_type		= AUDPP_CMD_PCM_INTF_RX_ENA_ARMTODSP_V;
	cmd.dsp_to_arm_buf_id	= 0;
	cmd.arm_to_dsp_buf_id	= idx + 1;
	cmd.arm_to_dsp_buf_len	= len;

	LOG(EV_SEND_BUFFER, idx);
	dma_coherent_pre_ops();
	return audpp_send_queue2(&cmd, sizeof(cmd));
}

/* ------------------- device --------------------- */

static int audio_enable_mbadrc(struct audio_copp *audio_copp, int enable)
{
	if (audio_copp->mbadrc_enable == enable &&
				!audio_copp->mbadrc_needs_commit)
		return 0;

	audio_copp->mbadrc_enable = enable;
	if (is_audpp_enable()) {
		audpp_dsp_set_mbadrc(COMMON_OBJ_ID, enable,
						&audio_copp->mbadrc);
		audio_copp->mbadrc_needs_commit = 0;
	}

	return 0;
}

static int audio_enable_eq(struct audio_copp *audio_copp, int enable)
{
	if (audio_copp->eq_enable == enable &&
				!audio_copp->eq_needs_commit)
		return 0;

	audio_copp->eq_enable = enable;

	if (is_audpp_enable()) {
		audpp_dsp_set_eq(COMMON_OBJ_ID, enable, &audio_copp->eq);
		audio_copp->eq_needs_commit = 0;
	}
	return 0;
}

static int audio_enable_rx_iir(struct audio_copp *audio_copp, int enable)
{
	if (audio_copp->rx_iir_enable == enable &&
				!audio_copp->rx_iir_needs_commit)
		return 0;

	audio_copp->rx_iir_enable = enable;

	if (is_audpp_enable()) {
		audpp_dsp_set_rx_iir(COMMON_OBJ_ID, enable, &audio_copp->iir);
		audio_copp->rx_iir_needs_commit = 0;
	}
	return 0;
}

static int audio_enable_srs_trumedia(struct audio_copp *audio_copp, int enable)
{

	if (!audio_copp->srs_needs_commit)
		return 0;

	audio_copp->srs_enable = enable;

	MM_DBG("Enable SRS flags 0x%x enable %d\n",
		audio_copp->srs_feature_mask, enable);
	if (is_audpp_enable()) {
		MM_DBG("Updating audpp for srs\n");
		if (audio_copp->srs_feature_mask & SRS_MASK_W)
			audpp_dsp_set_rx_srs_trumedia_w(&audio_copp->w);
		if (audio_copp->srs_feature_mask & SRS_MASK_C)
			audpp_dsp_set_rx_srs_trumedia_c(&audio_copp->c);
		if (audio_copp->srs_feature_mask & SRS_MASK_HP)
			audpp_dsp_set_rx_srs_trumedia_h(&audio_copp->h);
		if (audio_copp->srs_feature_mask & SRS_MASK_P)
			audpp_dsp_set_rx_srs_trumedia_p(&audio_copp->p);
		if (audio_copp->srs_feature_mask & SRS_MASK_HL)
			audpp_dsp_set_rx_srs_trumedia_l(&audio_copp->l);
		if (audio_copp->srs_feature_mask & SRS_MASK_G)
			audpp_dsp_set_rx_srs_trumedia_g(&audio_copp->g);

		audio_copp->srs_current_feature_mask =
			audio_copp->srs_feature_mask;
		audio_copp->srs_needs_commit = 0;
		audio_copp->srs_feature_mask = 0;
	}
	return 0;
}

static int audio_enable_vol_pan(struct audio_copp *audio_copp)
{
	if (is_audpp_enable())
		audpp_dsp_set_vol_pan(COMMON_OBJ_ID, &audio_copp->vol_pan);
	return 0;
}

static int audio_enable_qconcert_plus(struct audio_copp *audio_copp, int enable)
{
	if (audio_copp->qconcert_plus_enable == enable &&
				!audio_copp->qconcert_plus_needs_commit)
		return 0;

	audio_copp->qconcert_plus_enable = enable;

	if (is_audpp_enable()) {
		audpp_dsp_set_qconcert_plus(COMMON_OBJ_ID, enable,
					&audio_copp->qconcert_plus);
		audio_copp->qconcert_plus_needs_commit = 0;
	}
	return 0;
}

static void audio_flush(struct audio *audio)
{
	audio->out[0].used = 0;
	audio->out[1].used = 0;
	audio->out_head = 0;
	audio->out_tail = 0;
	audio->stopped = 0;
}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = -EINVAL;
	unsigned long flags = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		stats.byte_count = atomic_read(&audio->out_bytes);
		if (copy_to_user((void*) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	switch (cmd) {
	case AUDIO_SET_VOLUME:
		spin_lock_irqsave(&audio->dsp_lock, flags);
		audio->vol_pan.volume = arg;
		if (audio->running)
			audpp_dsp_set_vol_pan(5, &audio->vol_pan);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		return 0;

	case AUDIO_SET_PAN:
		spin_lock_irqsave(&audio->dsp_lock, flags);
		audio->vol_pan.pan = arg;
		if (audio->running)
			audpp_dsp_set_vol_pan(5, &audio->vol_pan);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		return 0;
	}

	LOG(EV_IOCTL, cmd);
	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START:
		rc = audio_enable(audio);
		break;
	case AUDIO_STOP:
		rc = audio_disable(audio);
		break;
	case AUDIO_FLUSH:
		if (audio->stopped) {
			/* Make sure we're stopped and we wake any threads
			 * that might be blocked holding the write_lock.
			 * While audio->stopped write threads will always
			 * exit immediately.
			 */
			wake_up(&audio->wait);
			mutex_lock(&audio->write_lock);
			audio_flush(audio);
			mutex_unlock(&audio->write_lock);
		}
		break;
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config config;
		if (copy_from_user(&config, (void*) arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		if (config.channel_count == 1) {
			config.channel_count = AUDPP_CMD_PCM_INTF_MONO_V;
		} else if (config.channel_count == 2) {
			config.channel_count= AUDPP_CMD_PCM_INTF_STEREO_V;
		} else {
			rc = -EINVAL;
			break;
		}
		audio->out_sample_rate = config.sample_rate;
		audio->out_channel_mode = config.channel_count;
		rc = 0;
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config config;
		config.buffer_size = BUFSZ;
		config.buffer_count = 2;
		config.sample_rate = audio->out_sample_rate;
		if (audio->out_channel_mode == AUDPP_CMD_PCM_INTF_MONO_V) {
			config.channel_count = 1;
		} else {
			config.channel_count = 2;
		}
		config.unused[0] = 0;
		config.unused[1] = 0;
		config.unused[2] = 0;
		if (copy_to_user((void*) arg, &config, sizeof(config))) {
			rc = -EFAULT;
		} else {
			rc = 0;
		}
		break;
	}
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

/* Only useful in tunnel-mode */
static int audio_fsync(struct file *file, loff_t a, loff_t b, int datasync)
{
	struct audio *audio = file->private_data;
	int rc = 0;

	if (!audio->running)
		return -EINVAL;

	mutex_lock(&audio->write_lock);

	rc = wait_event_interruptible(audio->wait,
		(!audio->out[0].used &&
		!audio->out[1].used));

	if (rc < 0)
		goto done;

	/* pcm dmamiss message is sent continously when
	 * decoder is starved so no race condition concern
	 */

	audio->teos = 0;

	rc = wait_event_interruptible(audio->wait,
		audio->teos);

done:
	mutex_unlock(&audio->write_lock);
	return rc;
}

static ssize_t audio_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	return -EINVAL;
}

static inline int rt_policy(int policy)
{
	if (unlikely(policy == SCHED_FIFO) || unlikely(policy == SCHED_RR))
		return 1;
	return 0;
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_policy(p->policy);
}

static ssize_t audio_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *pos)
{
	struct sched_param s = { .sched_priority = 1 };
	struct audio *audio = file->private_data;
	unsigned long flags;
	const char __user *start = buf;
	struct buffer *frame;
	size_t xfer;
	int old_prio = current->rt_priority;
	int old_policy = current->policy;
	int cap_nice = cap_raised(current_cap(), CAP_SYS_NICE);
	int rc = 0;

	LOG(EV_WRITE, count | (audio->running << 28) | (audio->stopped << 24));

	/* just for this write, set us real-time */
	if (!task_has_rt_policy(current)) {
		struct cred *new = prepare_creds();
		cap_raise(new->cap_effective, CAP_SYS_NICE);
		commit_creds(new);
		if ((sched_setscheduler(current, SCHED_RR, &s)) < 0)
			MM_ERR("sched_setscheduler failed\n");
	}

	mutex_lock(&audio->write_lock);
	while (count > 0) {
		frame = audio->out + audio->out_head;

		LOG(EV_WAIT_EVENT, 0);
		rc = wait_event_interruptible(audio->wait,
					      (frame->used == 0) || (audio->stopped));
		LOG(EV_WAIT_EVENT, 1);

		if (rc < 0)
			break;
		if (audio->stopped) {
			rc = -EBUSY;
			break;
		}
		xfer = count > frame->size ? frame->size : count;
		if (copy_from_user(frame->data, buf, xfer)) {
			rc = -EFAULT;
			break;
		}
		frame->used = xfer;
		audio->out_head ^= 1;
		count -= xfer;
		buf += xfer;

		spin_lock_irqsave(&audio->dsp_lock, flags);
		LOG(EV_FILL_BUFFER, audio->out_head ^ 1);
		frame = audio->out + audio->out_tail;
		if (frame->used && audio->out_needed) {
			audio_dsp_send_buffer(audio, audio->out_tail, frame->used);
			audio->out_tail ^= 1;
			audio->out_needed--;
		}
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
	}

	mutex_unlock(&audio->write_lock);

	/* restore scheduling policy and priority */
	if (!rt_policy(old_policy)) {
		struct sched_param v = { .sched_priority = old_prio };
		if ((sched_setscheduler(current, old_policy, &v)) < 0)
			MM_ERR("sched_setscheduler failed\n");
		if (likely(!cap_nice)) {
			struct cred *new = prepare_creds();
			cap_lower(new->cap_effective, CAP_SYS_NICE);
			commit_creds(new);
		}
	}

	LOG(EV_RETURN,(buf > start) ? (buf - start) : rc);
	if (buf > start)
		return buf - start;
	return rc;	
}

static int audio_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	LOG(EV_OPEN, 0);
	mutex_lock(&audio->lock);
	audio_disable(audio);
	audio_flush(audio);
	audio->opened = 0;
	mutex_unlock(&audio->lock);
	htc_pwrsink_set(PWRSINK_AUDIO, 0);
	return 0;
}

struct audio the_audio;

static int audio_open(struct inode *inode, struct file *file)
{
	struct audio *audio = &the_audio;
	int rc;

	mutex_lock(&audio->lock);

	if (audio->opened) {
		MM_ERR("busy\n");
		rc = -EBUSY;
		goto done;
	}

	if (!audio->data) {
		audio->data = dma_alloc_coherent(NULL, DMASZ, 
						 &audio->phys, GFP_KERNEL);
		if (!audio->data) {
			MM_ERR("could not allocate DMA buffers\n");
			rc = -ENOMEM;
			goto done;
		}
	}

	rc = audmgr_open(&audio->audmgr);
	if (rc)
		goto done;

	audio->out_buffer_size = BUFSZ;
	audio->out_sample_rate = 48000;
	audio->out_channel_mode = AUDPP_CMD_PCM_INTF_STEREO_V;
	audio->out_weight = 100;

	audio->out[0].data = audio->data + 0;
	audio->out[0].addr = audio->phys + 0;
	audio->out[0].size = BUFSZ;
	
	audio->out[1].data = audio->data + BUFSZ;
	audio->out[1].addr = audio->phys + BUFSZ;
	audio->out[1].size = BUFSZ;

	audio->vol_pan.volume = 0x2000;
	audio->vol_pan.pan = 0x0;

	audio_flush(audio);

	file->private_data = audio;
	audio->opened = 1;
	rc = 0;
	LOG(EV_OPEN, 1);
done:
	mutex_unlock(&audio->lock);
	return rc;
}

static long audpp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio_copp *audio_copp = file->private_data;
	int rc = 0, enable;
	uint16_t enable_mask;
	int prev_state;
	uint32_t to_set, size = 0;
	void *tmpbuf, *srs_params = NULL;

	mutex_lock(&audio_copp->lock);
	switch (cmd) {
	case AUDIO_ENABLE_AUDPP:
		if (copy_from_user(&enable_mask, (void *) arg,
						sizeof(enable_mask))) {
			rc = -EFAULT;
			break;
		}

		enable = ((enable_mask & ADRC_ENABLE) ||
				(enable_mask & MBADRC_ENABLE)) ? 1 : 0;
		audio_enable_mbadrc(audio_copp, enable);
		enable = (enable_mask & EQ_ENABLE) ? 1 : 0;
		audio_enable_eq(audio_copp, enable);
		enable = (enable_mask & IIR_ENABLE) ? 1 : 0;
		audio_enable_rx_iir(audio_copp, enable);
		enable = (enable_mask & QCONCERT_PLUS_ENABLE) ? 1 : 0;
		audio_enable_qconcert_plus(audio_copp, enable);
		enable = (enable_mask & SRS_ENABLE) ? 1 : 0;
		audio_enable_srs_trumedia(audio_copp, enable);
		break;

	case AUDIO_SET_MBADRC: {
		uint32_t mbadrc_coeff_buf;
		prev_state = audio_copp->mbadrc_enable;
		audio_copp->mbadrc_enable = 0;
		if (copy_from_user(&audio_copp->mbadrc.num_bands, (void *) arg,
				sizeof(audio_copp->mbadrc) -
				(AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN + 2)))
			rc = -EFAULT;
		else if (audio_copp->mbadrc.ext_buf_size) {
			mbadrc_coeff_buf = (uint32_t) ((char *) arg +
					sizeof(audio_copp->mbadrc) -
				(AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN + 2));
			if ((copy_from_user(audio_copp->mbadrc_data,
					(void *) mbadrc_coeff_buf,
					AUDPP_MBADRC_EXTERNAL_BUF_SIZE * 2))) {
				rc = -EFAULT;
				break;
			}
			audio_copp->mbadrc.ext_buf_lsw =
					audio_copp->mbadrc_phys & 0xFFFF;
			audio_copp->mbadrc.ext_buf_msw =
				((audio_copp->mbadrc_phys & 0xFFFF0000) >> 16);
		}
		audio_copp->mbadrc_enable = prev_state;
		if (!rc)
			audio_copp->mbadrc_needs_commit = 1;
		break;
	}

	case AUDIO_SET_ADRC: {
			struct audpp_cmd_cfg_object_params_adrc adrc;
			prev_state = audio_copp->mbadrc_enable;
			audio_copp->mbadrc_enable = 0;
			if (copy_from_user(&adrc.compression_th, (void *) arg,
							sizeof(adrc) - 2)) {
				rc = -EFAULT;
				audio_copp->mbadrc_enable = prev_state;
				break;
			}
			audio_copp->mbadrc.num_bands = 1;
			audio_copp->mbadrc.down_samp_level = 8;
			audio_copp->mbadrc.adrc_delay = adrc.adrc_delay;
			audio_copp->mbadrc.ext_buf_size = 0;
			audio_copp->mbadrc.ext_partition = 0;
			audio_copp->mbadrc.adrc_band[0].subband_enable = 1;
			audio_copp->mbadrc.adrc_band[0].adrc_sub_mute = 0;
			audio_copp->mbadrc.adrc_band[0].rms_time =
								adrc.rms_time;
			audio_copp->mbadrc.adrc_band[0].compression_th =
							adrc.compression_th;
			audio_copp->mbadrc.adrc_band[0].compression_slope =
							adrc.compression_slope;
			audio_copp->mbadrc.adrc_band[0].attack_const_lsw =
							adrc.attack_const_lsw;
			audio_copp->mbadrc.adrc_band[0].attack_const_msw =
							adrc.attack_const_msw;
			audio_copp->mbadrc.adrc_band[0].release_const_lsw =
							adrc.release_const_lsw;
			audio_copp->mbadrc.adrc_band[0].release_const_msw =
							adrc.release_const_msw;
			audio_copp->mbadrc.adrc_band[0].makeup_gain = 0x2000;
			audio_copp->mbadrc_enable = prev_state;
			audio_copp->mbadrc_needs_commit = 1;
			break;
		}

	case AUDIO_SET_EQ:
		prev_state = audio_copp->eq_enable;
		audio_copp->eq_enable = 0;
		if (copy_from_user(&audio_copp->eq.num_bands, (void *) arg,
				sizeof(audio_copp->eq) -
				(AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN + 2)))
			rc = -EFAULT;
		audio_copp->eq_enable = prev_state;
		audio_copp->eq_needs_commit = 1;
		break;

	case AUDIO_SET_RX_IIR:
		prev_state = audio_copp->rx_iir_enable;
		audio_copp->rx_iir_enable = 0;
		if (copy_from_user(&audio_copp->iir.num_bands, (void *) arg,
				sizeof(audio_copp->iir) -
				(AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN + 2)))
			rc = -EFAULT;
		audio_copp->rx_iir_enable = prev_state;
		audio_copp->rx_iir_needs_commit = 1;
		break;

	case AUDIO_SET_VOLUME:
		audio_copp->vol_pan.volume = arg;
		audio_enable_vol_pan(audio_copp);
		break;

	case AUDIO_SET_PAN:
		audio_copp->vol_pan.pan = arg;
		audio_enable_vol_pan(audio_copp);
		break;

	case AUDIO_SET_QCONCERT_PLUS:
		prev_state = audio_copp->qconcert_plus_enable;
		audio_copp->qconcert_plus_enable = 0;
		if (copy_from_user(&audio_copp->qconcert_plus.op_mode,
				(void *) arg,
				sizeof(audio_copp->qconcert_plus) -
				(AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN + 2)))
			rc = -EFAULT;
		audio_copp->qconcert_plus_enable = prev_state;
		audio_copp->qconcert_plus_needs_commit = 1;
		break;

	case AUDIO_SET_SRS_TRUMEDIA_PARAM: {
		prev_state = audio_copp->srs_enable;
		audio_copp->srs_enable = 0;

		if (copy_from_user(&to_set, (void *)arg, sizeof(uint32_t))) {
			rc = -EFAULT;
			break;
		}
		switch (to_set) {
		case SRS_ID_GLOBAL:
			srs_params = (void *)audio_copp->g.v;
			size = sizeof(audio_copp->g.v);
			audio_copp->srs_feature_mask |= SRS_MASK_G;
			break;
		case SRS_ID_WOWHD:
			srs_params = (void *)audio_copp->w.v;
			size = sizeof(audio_copp->w.v);
			audio_copp->srs_feature_mask |= SRS_MASK_W;
			break;
		case SRS_ID_CSHP:
			srs_params = (void *)audio_copp->c.v;
			size = sizeof(audio_copp->c.v);
			audio_copp->srs_feature_mask |= SRS_MASK_C;
			break;
		case SRS_ID_HPF:
			srs_params = (void *)audio_copp->h.v;
			size = sizeof(audio_copp->h.v);
			audio_copp->srs_feature_mask |= SRS_MASK_HP;
			break;
		case SRS_ID_PEQ:
			srs_params = (void *)audio_copp->p.v;
			size = sizeof(audio_copp->p.v);
			audio_copp->srs_feature_mask |= SRS_MASK_P;
			break;
		case SRS_ID_HL:
			srs_params = (void *)audio_copp->l.v;
			size = sizeof(audio_copp->l.v);
			audio_copp->srs_feature_mask |= SRS_MASK_HL;
			break;
		default:
			MM_ERR("SRS TruMedia error: invalid ioctl\n");
			rc = -EINVAL;
		}

		if (rc >= 0) {
			tmpbuf = kzalloc(sizeof(uint32_t) + size , GFP_KERNEL);
			if (!tmpbuf) {
				MM_ERR("SRS TruMedia error: no kernel mem\n");
				rc = -ENOMEM;
			} else {
				if (copy_from_user(tmpbuf, (void *)arg,
						sizeof(uint32_t) + size))
					rc = -EFAULT;
				memcpy(srs_params,
					&(((uint32_t *)tmpbuf)[1]), size);
				kfree(tmpbuf);
			}
		}

		MM_DBG("Ioctl SRS flags=0x%x\n", audio_copp->srs_feature_mask);
		if (rc < 0)
			MM_ERR("SRS TruMedia error setting params failed.\n");
		else{
			audio_copp->srs_needs_commit = 1;
			audio_copp->srs_enable = prev_state;
		}
		break;
	}

	default:
		rc = -EINVAL;
	}

	mutex_unlock(&audio_copp->lock);
	return rc;
}

static int audpp_open(struct inode *inode, struct file *file)
{
	struct audio_copp *audio_copp = &the_audio_copp;
	int rc;

	mutex_lock(&audio_copp->lock);
	if (audio_copp->opened) {
		mutex_unlock(&audio_copp->lock);
		return -EBUSY;
	}

	audio_copp->opened = 1;

	if (!audio_copp->status) {
		audio_copp->ecb.fn = audio_commit_pending_pp_params;
		audio_copp->ecb.private = audio_copp;
		rc = audpp_register_event_callback(&audio_copp->ecb);
		if (rc) {
			audio_copp->opened = 0;
			mutex_unlock(&audio_copp->lock);
			return rc;
		}
		audio_copp->mbadrc_data = dma_alloc_coherent(NULL,
				AUDPP_MBADRC_EXTERNAL_BUF_SIZE * 2,
				 &audio_copp->mbadrc_phys, GFP_KERNEL);
		if (!audio_copp->mbadrc_data) {
			MM_ERR("could not allocate DMA buffers\n");
			audio_copp->opened = 0;
			audpp_unregister_event_callback(&audio_copp->ecb);
			mutex_unlock(&audio_copp->lock);
			return -ENOMEM;
		}
		audio_copp->vol_pan.volume = 0x2000;
		audio_copp->vol_pan.pan = 0x0;
		audio_copp->status = 1;
	}

	file->private_data = audio_copp;
	mutex_unlock(&audio_copp->lock);

	return 0;
}

static int audpp_release(struct inode *inode, struct file *file)
{
	struct audio_copp *audio_copp = &the_audio_copp;

	audio_copp->opened = 0;

	return 0;
}

static struct file_operations audio_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_open,
	.release	= audio_release,
	.read		= audio_read,
	.write		= audio_write,
	.unlocked_ioctl	= audio_ioctl,
	.fsync		= audio_fsync,
};

static struct file_operations audpp_fops = {
	.owner		= THIS_MODULE,
	.open		= audpp_open,
	.release	= audpp_release,
	.unlocked_ioctl	= audpp_ioctl,
};

struct miscdevice audio_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_out",
	.fops	= &audio_fops,
};

struct miscdevice audpp_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_ctl",
	.fops	= &audpp_fops,
};

static int __init audio_init(void)
{
	mutex_init(&the_audio.lock);
	mutex_init(&the_audio.write_lock);
	mutex_init(&the_audio_copp.lock);
	spin_lock_init(&the_audio.dsp_lock);
	init_waitqueue_head(&the_audio.wait);
	wake_lock_init(&the_audio.wakelock, WAKE_LOCK_SUSPEND, "audio_pcm");
	pm_qos_add_request(&the_audio.pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	return (misc_register(&audio_misc) || misc_register(&audpp_misc));
}

device_initcall(audio_init);

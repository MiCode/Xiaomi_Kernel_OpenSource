/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/msm_audio_qcp.h>
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <sound/q6asm.h>
#include <sound/apr_audio.h>
#include "audio_utils.h"

/* Buffer with meta*/
#define PCM_BUF_SIZE		(4096 + sizeof(struct meta_in))

/* Maximum 10 frames in buffer with meta */
#define FRAME_SIZE		(1 + ((23+sizeof(struct meta_out_dsp)) * 10))

void q6asm_evrc_in_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload, void *priv)
{
	struct q6audio_in * audio = (struct q6audio_in *)priv;
	unsigned long flags;

	pr_debug("%s:session id %d: opcode - %d\n", __func__,
			audio->ac->session, opcode);

	spin_lock_irqsave(&audio->dsp_lock, flags);
	switch (opcode) {
	case ASM_DATA_EVENT_READ_DONE:
		audio_in_get_dsp_frames(audio, token, payload);
		break;
	case ASM_DATA_EVENT_WRITE_DONE:
		atomic_inc(&audio->in_count);
		wake_up(&audio->write_wait);
		break;
	case ASM_DATA_CMDRSP_EOS:
		audio->eos_rsp = 1;
		wake_up(&audio->read_wait);
		break;
	case ASM_STREAM_CMDRSP_GET_ENCDEC_PARAM:
		break;
	case ASM_STREAM_CMDRSP_GET_PP_PARAMS:
		break;
	case ASM_SESSION_EVENT_TX_OVERFLOW:
		pr_err("%s:session id %d: ASM_SESSION_EVENT_TX_OVERFLOW\n",
				__func__, audio->ac->session);
		break;
	default:
		pr_err("%s:session id %d: Ignore opcode[0x%x]\n", __func__,
				audio->ac->session, opcode);
		break;
	}
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
}

/* ------------------- device --------------------- */
static long evrc_in_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct q6audio_in  *audio = file->private_data;
	int rc = 0;
	int cnt = 0;

	switch (cmd) {
	case AUDIO_START: {
		struct msm_audio_evrc_enc_config *enc_cfg;
		enc_cfg = audio->enc_cfg;
		pr_debug("%s:session id %d: default buf alloc[%d]\n", __func__,
				audio->ac->session, audio->buf_alloc);
		if (audio->enabled == 1) {
			pr_info("%s:AUDIO_START already over\n", __func__);
			rc = 0;
			break;
		}
		rc = audio_in_buf_alloc(audio);
		if (rc < 0) {
			pr_err("%s:session id %d: buffer allocation failed\n",
				__func__, audio->ac->session);
			break;
		}

		/* rate_modulation_cmd set to zero
			 currently not configurable from user space */
		rc = q6asm_enc_cfg_blk_evrc(audio->ac,
			audio->buf_cfg.frames_per_buf,
			enc_cfg->min_bit_rate,
			enc_cfg->max_bit_rate, 0);

		if (rc < 0) {
			pr_err("%s:session id %d: cmd evrc media format block\
				failed\n", __func__, audio->ac->session);
			break;
		}
		if (audio->feedback == NON_TUNNEL_MODE) {
			rc = q6asm_media_format_block_pcm(audio->ac,
				audio->pcm_cfg.sample_rate,
				audio->pcm_cfg.channel_count);

			if (rc < 0) {
				pr_err("%s:session id %d: media format block\
				failed\n", __func__, audio->ac->session);
				break;
			}
		}
		pr_debug("%s:session id %d: AUDIO_START enable[%d]\n",
				__func__, audio->ac->session, audio->enabled);
		rc = audio_in_enable(audio);
		if (!rc) {
			audio->enabled = 1;
		} else {
			audio->enabled = 0;
			pr_err("%s:session id %d: Audio Start procedure failed\
				rc=%d\n", __func__, audio->ac->session, rc);
			break;
		}
		while (cnt++ < audio->str_cfg.buffer_count)
			q6asm_read(audio->ac); /* Push buffer to DSP */
		rc = 0;
		pr_debug("%s:session id %d: AUDIO_START success enable[%d]\n",
				__func__, audio->ac->session, audio->enabled);
		break;
	}
	case AUDIO_STOP: {
		pr_debug("%s:session id %d: AUDIO_STOP\n", __func__,
				audio->ac->session);
		rc = audio_in_disable(audio);
		if (rc  < 0) {
			pr_err("%s:session id %d: Audio Stop procedure failed\
				rc=%d\n", __func__, audio->ac->session, rc);
			break;
		}
		break;
	}
	case AUDIO_GET_EVRC_ENC_CONFIG: {
		if (copy_to_user((void *)arg, audio->enc_cfg,
			sizeof(struct msm_audio_evrc_enc_config)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_SET_EVRC_ENC_CONFIG: {
		struct msm_audio_evrc_enc_config cfg;
		struct msm_audio_evrc_enc_config *enc_cfg;
		enc_cfg = audio->enc_cfg;

		if (copy_from_user(&cfg, (void *) arg,
				sizeof(struct msm_audio_evrc_enc_config))) {
			rc = -EFAULT;
			break;
		}

		if (cfg.min_bit_rate > 4 ||
			 cfg.min_bit_rate < 1 ||
			 (cfg.min_bit_rate == 2)) {
			pr_err("%s:session id %d: invalid min bitrate\n",
					__func__, audio->ac->session);
			rc = -EINVAL;
			break;
		}
		if (cfg.max_bit_rate > 4 ||
			 cfg.max_bit_rate < 1 ||
			 (cfg.max_bit_rate == 2)) {
			pr_err("%s:session id %d: invalid max bitrate\n",
				__func__, audio->ac->session);
			rc = -EINVAL;
			break;
		}
		enc_cfg->min_bit_rate = cfg.min_bit_rate;
		enc_cfg->max_bit_rate = cfg.max_bit_rate;
		pr_debug("%s:session id %d: min_bit_rate= 0x%x\
			max_bit_rate=0x%x\n", __func__,
			audio->ac->session, enc_cfg->min_bit_rate,
			enc_cfg->max_bit_rate);
		break;
	}
	default:
		rc = -EINVAL;
	}
	return rc;
}

static int evrc_in_open(struct inode *inode, struct file *file)
{
	struct q6audio_in *audio = NULL;
	struct msm_audio_evrc_enc_config *enc_cfg;
	int rc = 0;

	audio = kzalloc(sizeof(struct q6audio_in), GFP_KERNEL);

	if (audio == NULL) {
		pr_err("%s: Could not allocate memory for evrc\
				driver\n", __func__);
		return -ENOMEM;
	}
	/* Allocate memory for encoder config param */
	audio->enc_cfg = kzalloc(sizeof(struct msm_audio_evrc_enc_config),
				GFP_KERNEL);
	if (audio->enc_cfg == NULL) {
		pr_err("%s:session id %d: Could not allocate memory for aac\
				config param\n", __func__, audio->ac->session);
		kfree(audio);
		return -ENOMEM;
	}
	enc_cfg = audio->enc_cfg;
	mutex_init(&audio->lock);
	mutex_init(&audio->read_lock);
	mutex_init(&audio->write_lock);
	spin_lock_init(&audio->dsp_lock);
	init_waitqueue_head(&audio->read_wait);
	init_waitqueue_head(&audio->write_wait);

	/* Settings will be re-config at AUDIO_SET_CONFIG,
	* but at least we need to have initial config
	*/
	audio->str_cfg.buffer_size = FRAME_SIZE;
	audio->str_cfg.buffer_count = FRAME_NUM;
	audio->min_frame_size = 23;
	audio->max_frames_per_buf = 10;
	audio->pcm_cfg.buffer_size = PCM_BUF_SIZE;
	audio->pcm_cfg.buffer_count = PCM_BUF_COUNT;
	enc_cfg->min_bit_rate = 4;
	enc_cfg->max_bit_rate = 4;
	audio->pcm_cfg.channel_count = 1;
	audio->pcm_cfg.sample_rate = 8000;
	audio->buf_cfg.meta_info_enable = 0x01;
	audio->buf_cfg.frames_per_buf = 0x01;

	audio->ac = q6asm_audio_client_alloc((app_cb)q6asm_evrc_in_cb,
				(void *)audio);

	if (!audio->ac) {
		pr_err("%s: Could not allocate memory for audio\
				client\n", __func__);
		kfree(audio->enc_cfg);
		kfree(audio);
		return -ENOMEM;
	}

	/* open evrc encoder in T/NT mode */
	if ((file->f_mode & FMODE_WRITE) &&
		(file->f_mode & FMODE_READ)) {
		audio->feedback = NON_TUNNEL_MODE;
		rc = q6asm_open_read_write(audio->ac, FORMAT_EVRC,
					FORMAT_LINEAR_PCM);
		if (rc < 0) {
			pr_err("%s:session id %d: NT mode Open failed rc=%d\n",
					__func__, audio->ac->session, rc);
			rc = -ENODEV;
			goto fail;
		}
		pr_info("%s:session id %d: NT mode encoder success\n",
				__func__, audio->ac->session);
	} else if (!(file->f_mode & FMODE_WRITE) &&
				(file->f_mode & FMODE_READ)) {
		audio->feedback = TUNNEL_MODE;
		rc = q6asm_open_read(audio->ac, FORMAT_EVRC);
		if (rc < 0) {
			pr_err("%s:session id %d: T mode Open failed rc=%d\n",
					__func__, audio->ac->session, rc);
			rc = -ENODEV;
			goto fail;
		}
		/* register for tx overflow (valid for tunnel mode only) */
		rc = q6asm_reg_tx_overflow(audio->ac, 0x01);
		if (rc < 0) {
			pr_err("%s:session id %d: TX Overflow registration\
				failed rc=%d\n", __func__,
				audio->ac->session, rc);
			rc = -ENODEV;
			goto fail;
		}
		pr_info("%s:session id %d: T mode encoder success\n", __func__,
				audio->ac->session);
	} else {
		pr_err("%s:session id %d: Unexpected mode\n", __func__,
				audio->ac->session);
		rc = -EACCES;
		goto fail;
	}

	audio->opened = 1;
	atomic_set(&audio->in_count, PCM_BUF_COUNT);
	atomic_set(&audio->out_count, 0x00);
	audio->enc_ioctl = evrc_in_ioctl;
	file->private_data = audio;

	pr_info("%s:session id %d: success\n", __func__, audio->ac->session);
	return 0;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio->enc_cfg);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_in_fops = {
	.owner		= THIS_MODULE,
	.open		= evrc_in_open,
	.release	= audio_in_release,
	.read		= audio_in_read,
	.write		= audio_in_write,
	.unlocked_ioctl	= audio_in_ioctl,
};

struct miscdevice audio_evrc_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_evrc_in",
	.fops	= &audio_in_fops,
};

static int __init evrc_in_init(void)
{
	return misc_register(&audio_evrc_in_misc);
}

device_initcall(evrc_in_init);

/* Copyright (c) 2010-2016, The Linux Foundation. All rights reserved.
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
#include <linux/msm_audio_g711.h>
#include <linux/atomic.h>
#include <linux/compat.h>
#include <asm/ioctls.h>
#include "audio_utils.h"

#ifdef CONFIG_COMPAT
#undef PROC_ADD
#endif
/* Buffer with meta*/
#define PCM_BUF_SIZE		(4096 + sizeof(struct meta_in))

/* Maximum 10 frames in buffer with meta */
#define FRAME_SIZE		(1 + ((320+sizeof(struct meta_out_dsp)) * 10))
static long g711_in_ioctl_shared(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct q6audio_in  *audio = file->private_data;
	int rc = 0;
	int cnt = 0;

	switch (cmd) {
	case AUDIO_START: {
		struct msm_audio_g711_enc_config *enc_cfg;

		enc_cfg = (struct msm_audio_g711_enc_config *)audio->enc_cfg;
		pr_debug("%s:session id %d: default buf alloc[%d]\n", __func__,
				audio->ac->session, audio->buf_alloc);
		if (audio->enabled == 1) {
			rc = 0;
			break;
		}
		rc = audio_in_buf_alloc(audio);
		if (rc < 0) {
			pr_err("%s:session id %d: buffer allocation failed rc=%d\n",
				__func__, audio->ac->session, rc);
			break;
		}
		pr_debug("%s: sample rate %d", __func__, enc_cfg->sample_rate);
		rc = q6asm_enc_cfg_blk_g711(audio->ac,
			audio->buf_cfg.frames_per_buf,
			enc_cfg->sample_rate);

		if (rc < 0) {
			pr_err("%s:session id %d: cmd g711 media format block failed rc=%d\n",
					__func__, audio->ac->session, rc);
			break;
		}
		if (audio->feedback == NON_TUNNEL_MODE) {
			rc = q6asm_media_format_block_pcm(audio->ac,
				audio->pcm_cfg.sample_rate,
				audio->pcm_cfg.channel_count);

			if (rc < 0) {
				pr_err("%s:session id %d: media format block failed rc=%d\n",
					__func__, audio->ac->session, rc);
				break;
			}
		}
		pr_debug("%s:session id %d: AUDIO_START enable[%d]\n", __func__,
				audio->ac->session, audio->enabled);
		rc = audio_in_enable(audio);
		if (!rc) {
			audio->enabled = 1;
		} else {
			audio->enabled = 0;
			pr_err("%s:session id %d: Audio Start procedure failed rc=%d\n",
					__func__, audio->ac->session, rc);
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
			pr_err("%s:session id %d: Audio Stop procedure failed rc=%d\n",
				__func__, audio->ac->session,
					rc);
			break;
		}
		break;
	}
	case AUDIO_SET_G711_ENC_CONFIG: {
		struct msm_audio_g711_enc_config *cfg;
		struct msm_audio_g711_enc_config *enc_cfg;

		enc_cfg = (struct msm_audio_g711_enc_config *)audio->enc_cfg;

		cfg = (struct msm_audio_g711_enc_config *)arg;
		if (cfg == NULL) {
			pr_err("%s: NULL config pointer\n", __func__);
			rc = -EINVAL;
			break;
		}
		if (cfg->sample_rate != 8000 &&
			 cfg->sample_rate != 16000) {
			pr_err("%s:session id %d: invalid sample rate\n",
					__func__, audio->ac->session);
			rc = -EINVAL;
			break;
		}
		enc_cfg->sample_rate = cfg->sample_rate;
		pr_debug("%s:session id %d: sample_rate= 0x%x",
			__func__,
			audio->ac->session, enc_cfg->sample_rate);
		break;
	}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
		rc = -ENOIOCTLCMD;
	}
	return rc;
}

static long g711_in_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct q6audio_in  *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START:
	case AUDIO_STOP: {
		rc = g711_in_ioctl_shared(file, cmd, arg);
		break;
	}
	case AUDIO_GET_G711_ENC_CONFIG: {
		if (copy_to_user((void *)arg, audio->enc_cfg,
			sizeof(struct msm_audio_g711_enc_config))) {
			pr_err(
				"%s: copy_to_user for AUDIO_GET_g711_ENC_CONFIG failed",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_SET_G711_ENC_CONFIG: {
		struct msm_audio_g711_enc_config cfg;

		if (copy_from_user(&cfg, (void *) arg,
				sizeof(cfg))) {
			pr_err(
				"%s: copy_from_user for AUDIO_GET_G711_ENC_CONFIG failed",
				__func__);
			rc = -EFAULT;
			break;
		}
		rc = g711_in_ioctl_shared(file, cmd, (unsigned long)&cfg);
		if (rc)
			pr_err("%s:AUDIO_GET_G711_ENC_CONFIG failed. Rc= %d\n",
				__func__, rc);
		break;
	}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
		rc = -ENOIOCTLCMD;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
struct msm_audio_g711_enc_config32 {
	uint32_t sample_rate;
};

enum {
	AUDIO_SET_G711_ENC_CONFIG_32 = _IOW(AUDIO_IOCTL_MAGIC,
	  (AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_g711_enc_config32),
	AUDIO_GET_G711_ENC_CONFIG_32 = _IOR(AUDIO_IOCTL_MAGIC,
	  (AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_g711_enc_config32)
};

static long g711_in_compat_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct q6audio_in  *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START:
	case AUDIO_STOP: {
		rc = g711_in_ioctl_shared(file, cmd, arg);
		break;
	}
	case AUDIO_GET_G711_ENC_CONFIG_32: {
		struct msm_audio_g711_enc_config32 cfg_32;
		struct msm_audio_g711_enc_config32 *enc_cfg;

		enc_cfg = (struct msm_audio_g711_enc_config32 *)audio->enc_cfg;
		cfg_32.sample_rate = enc_cfg->sample_rate;
		if (copy_to_user((void *)arg, &cfg_32,
			sizeof(cfg_32))) {
			pr_err("%s: copy_to_user for AUDIO_GET_G711_ENC_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_SET_G711_ENC_CONFIG_32: {
		struct msm_audio_g711_enc_config32 cfg_32;
		struct msm_audio_g711_enc_config32 cfg;

		if (copy_from_user(&cfg_32, (void *) arg,
				sizeof(cfg_32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_G711_ENC_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		cfg.sample_rate = cfg_32.sample_rate;
		cmd = AUDIO_SET_G711_ENC_CONFIG;
		rc = g711_in_ioctl_shared(file, cmd, (unsigned long)&cfg);
		if (rc)
			pr_err("%s:AUDIO_SET_G711_ENC_CONFIG failed. rc= %d\n",
				__func__, rc);
		break;
	}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
		rc = -ENOIOCTLCMD;
	}
	return rc;
}
#else
#define g711_in_compat_ioctl NULL
#endif

static int g711_in_open(struct inode *inode, struct file *file)
{
	struct q6audio_in *audio = NULL;
	struct msm_audio_g711_enc_config *enc_cfg;
	int rc = 0;

	audio = kzalloc(sizeof(struct q6audio_in), GFP_KERNEL);

	if (audio == NULL)
		return -ENOMEM;
	/* Allocate memory for encoder config param */
	audio->enc_cfg = kzalloc(sizeof(struct msm_audio_g711_enc_config),
				GFP_KERNEL);
	if (audio->enc_cfg == NULL) {
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

	/*
	 * Settings will be re-config at AUDIO_SET_CONFIG,
	 * but at least we need to have initial config
	 */
	audio->str_cfg.buffer_size = FRAME_SIZE;
	audio->str_cfg.buffer_count = FRAME_NUM;
	audio->min_frame_size = 320;
	audio->max_frames_per_buf = 10;
	audio->pcm_cfg.buffer_size = PCM_BUF_SIZE;
	audio->pcm_cfg.buffer_count = PCM_BUF_COUNT;
	enc_cfg->sample_rate = 8000;
	audio->pcm_cfg.channel_count = 1;
	audio->pcm_cfg.sample_rate = 8000;
	audio->buf_cfg.meta_info_enable = 0x01;
	audio->buf_cfg.frames_per_buf = 0x01;
	audio->event_abort = 0;

	audio->ac = q6asm_audio_client_alloc((app_cb)q6asm_in_cb,
				(void *)audio);

	if (!audio->ac) {
		kfree(audio->enc_cfg);
		kfree(audio);
		return -ENOMEM;
	}

	/* open g711 encoder in T/NT mode */
	if ((file->f_mode & FMODE_WRITE) &&
		(file->f_mode & FMODE_READ)) {
		audio->feedback = NON_TUNNEL_MODE;
		rc = q6asm_open_read_write(audio->ac, FORMAT_G711_MLAW_FS,
					FORMAT_LINEAR_PCM);
		if (rc < 0) {
			pr_err("%s:session id %d: NT mode Open failed rc=%d\n",
				__func__, audio->ac->session, rc);
			rc = -ENODEV;
			goto fail;
		}
	} else if (!(file->f_mode & FMODE_WRITE) &&
				(file->f_mode & FMODE_READ)) {
		audio->feedback = TUNNEL_MODE;
		rc = q6asm_open_read(audio->ac, FORMAT_G711_MLAW_FS);
		if (rc < 0) {
			pr_err("%s:session id %d: T mode Open failed rc=%d\n",
				__func__, audio->ac->session, rc);
			rc = -ENODEV;
			goto fail;
		}
		/* register for tx overflow (valid for tunnel mode only) */
		rc = q6asm_reg_tx_overflow(audio->ac, 0x01);
		if (rc < 0) {
			pr_err("%s:session id %d: TX Overflow registration failed rc=%d\n",
				__func__, audio->ac->session, rc);
			rc = -ENODEV;
			goto fail;
		}
	} else {
		pr_err("%s:session id %d: Unexpected mode\n", __func__,
				audio->ac->session);
		rc = -EACCES;
		goto fail;
	}

	audio->opened = 1;
	audio->reset_event = false;
	atomic_set(&audio->in_count, PCM_BUF_COUNT);
	atomic_set(&audio->out_count, 0x00);
	audio->enc_compat_ioctl = g711_in_compat_ioctl;
	audio->enc_ioctl = g711_in_ioctl;
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
	.open		= g711_in_open,
	.release	= audio_in_release,
	.read		= audio_in_read,
	.write		= audio_in_write,
	.unlocked_ioctl = audio_in_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = audio_in_compat_ioctl,
#endif
};

struct miscdevice audio_g711mlaw_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_g711mlaw_in",
	.fops	= &audio_in_fops,
};

static int __init g711mlaw_in_init(void)
{
	return misc_register(&audio_g711mlaw_in_misc);
}

device_initcall(g711mlaw_in_init);

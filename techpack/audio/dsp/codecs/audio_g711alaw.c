/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/msm_audio_g711_dec.h>
#include <linux/compat.h>
#include "audio_utils_aio.h"

static struct miscdevice audio_g711alaw_misc;
static struct ws_mgr audio_g711_ws_mgr;

static const struct file_operations audio_g711_debug_fops = {
	.read = audio_aio_debug_read,
	.open = audio_aio_debug_open,
};

static struct dentry *config_debugfs_create_file(const char *name, void *data)
{
	return debugfs_create_file(name, S_IFREG | 0444,
				NULL, (void *)data, &audio_g711_debug_fops);
}

static int g711_channel_map(u8 *channel_mapping, uint32_t channels);

static long audio_ioctl_shared(struct file *file, unsigned int cmd,
						void *arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		struct asm_g711_dec_cfg g711_dec_cfg;
		struct msm_audio_g711_dec_config *g711_dec_config;
		u8 channel_mapping[PCM_FORMAT_MAX_NUM_CHANNEL];

		memset(channel_mapping, 0, PCM_FORMAT_MAX_NUM_CHANNEL);
		memset(&g711_dec_cfg, 0, sizeof(g711_dec_cfg));

		if (g711_channel_map(channel_mapping,
			audio->pcm_cfg.channel_count)) {
			pr_err("%s: setting channel map failed %d\n",
					__func__, audio->pcm_cfg.channel_count);
		}

		pr_debug("%s[%pK]: AUDIO_START session_id[%d]\n", __func__,
						audio, audio->ac->session);
		if (audio->feedback == NON_TUNNEL_MODE) {
			/* Configure PCM output block */
			rc = q6asm_enc_cfg_blk_pcm_v2(audio->ac,
					audio->pcm_cfg.sample_rate,
					audio->pcm_cfg.channel_count,
					16, /*bits per sample*/
					false, false, channel_mapping);
			if (rc < 0) {
				pr_err("%s: pcm output block config failed rc=%d\n",
						 __func__, rc);
				break;
			}
		}
		g711_dec_config =
			(struct msm_audio_g711_dec_config *)audio->codec_cfg;
		g711_dec_cfg.sample_rate = g711_dec_config->sample_rate;
		/* Configure Media format block */
		rc = q6asm_media_format_block_g711(audio->ac, &g711_dec_cfg,
							audio->ac->stream_id);
		if (rc < 0) {
			pr_err("%s: cmd media format block failed rc=%d\n",
				__func__, rc);
			break;
		}
		rc = audio_aio_enable(audio);
		audio->eos_rsp = 0;
		audio->eos_flag = 0;
		if (!rc) {
			audio->enabled = 1;
		} else {
			audio->enabled = 0;
			pr_err("%s: Audio Start procedure failed rc=%d\n",
						__func__, rc);
			break;
		}
		pr_debug("%s: AUDIO_START success enable[%d]\n",
					 __func__, audio->enabled);
		if (audio->stopped == 1)
			audio->stopped = 0;
		break;
	}
	default:
		pr_debug("%s: Unknown ioctl cmd = %d", __func__, cmd);
		break;
	}
	return rc;
}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		rc = audio_ioctl_shared(file, cmd, (void *)arg);
		break;
	}
	case AUDIO_GET_G711_DEC_CONFIG: {
		if (copy_to_user((void *)arg, audio->codec_cfg,
			sizeof(struct msm_audio_g711_dec_config))) {
			pr_err("%s: copy_to_user for AUDIO_GET_G711_DEC_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_SET_G711_DEC_CONFIG: {
		if (copy_from_user(audio->codec_cfg, (void *)arg,
			sizeof(struct msm_audio_g711_dec_config))) {
			pr_err("%s: copy_from_user for AUDIO_SET_G711_DEC_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	default: {
		rc = audio->codec_ioctl(file, cmd, arg);
		if (rc)
			pr_err("%s: Failed in audio_aio_ioctl: %d cmd=%d\n",
				__func__, rc, cmd);
		break;
	}
	}
	return  rc;
}

#ifdef CONFIG_COMPAT
struct msm_audio_g711_dec_config_32 {
	u32 sample_rate;
};

enum {
	AUDIO_SET_G711_DEC_CONFIG_32 =  _IOW(AUDIO_IOCTL_MAGIC,
	(AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_g711_dec_config_32),
	AUDIO_GET_G711_DEC_CONFIG_32 =  _IOR(AUDIO_IOCTL_MAGIC,
	(AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_g711_dec_config_32)
};

static long audio_compat_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		rc = audio_ioctl_shared(file, cmd, (void *)arg);
		break;
	}
	case AUDIO_GET_G711_DEC_CONFIG_32: {
		struct msm_audio_g711_dec_config *g711_dec_config;
		struct msm_audio_g711_dec_config_32 g711_dec_config_32;

		memset(&g711_dec_config_32, 0, sizeof(g711_dec_config_32));

		g711_dec_config =
			(struct msm_audio_g711_dec_config *)audio->codec_cfg;
		g711_dec_config_32.sample_rate = g711_dec_config->sample_rate;

		if (copy_to_user((void *)arg, &g711_dec_config_32,
			sizeof(g711_dec_config_32))) {
			pr_err("%s: copy_to_user for AUDIO_GET_G711_DEC_CONFIG_32 failed\n",
				 __func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_SET_G711_DEC_CONFIG_32: {
		struct msm_audio_g711_dec_config *g711_dec_config;
		struct msm_audio_g711_dec_config_32 g711_dec_config_32;

		memset(&g711_dec_config_32, 0, sizeof(g711_dec_config_32));

		if (copy_from_user(&g711_dec_config_32, (void *)arg,
			sizeof(g711_dec_config_32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_G711_DEC_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}

		g711_dec_config =
			(struct msm_audio_g711_dec_config *)audio->codec_cfg;
		g711_dec_config->sample_rate = g711_dec_config_32.sample_rate;

		break;
	}
	default: {
		rc = audio->codec_compat_ioctl(file, cmd, arg);
		if (rc)
			pr_err("%s: Failed in audio_aio_compat_ioctl: %d cmd=%d\n",
				__func__, rc, cmd);
		break;
	}
	}
	return rc;
}
#else
#define audio_compat_ioctl NULL
#endif

static int audio_open(struct inode *inode, struct file *file)
{
	struct q6audio_aio *audio = NULL;
	int rc = 0;
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_g711_" + 5];

	audio = kzalloc(sizeof(struct q6audio_aio), GFP_KERNEL);

	if (!audio)
		return -ENOMEM;
	audio->codec_cfg = kzalloc(sizeof(struct msm_audio_g711_dec_config),
					GFP_KERNEL);
	if (!audio->codec_cfg) {
		kfree(audio);
		return -ENOMEM;
	}

	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN;
	audio->miscdevice = &audio_g711alaw_misc;
	audio->wakelock_voted = false;
	audio->audio_ws_mgr = &audio_g711_ws_mgr;

	init_waitqueue_head(&audio->event_wait);

	audio->ac = q6asm_audio_client_alloc((app_cb) q6_audio_cb,
					     (void *)audio);

	if (!audio->ac) {
		pr_err("%s: Could not allocate memory for audio client\n",
					 __func__);
		kfree(audio->codec_cfg);
		kfree(audio);
		return -ENOMEM;
	}
	rc = audio_aio_open(audio, file);
	if (rc < 0) {
		pr_err("%s: audio_aio_open rc=%d\n",
			__func__, rc);
		goto fail;
	}
	/* open in T/NT mode */ /*foramt:G711_ALAW*/
	if ((file->f_mode & FMODE_WRITE) && (file->f_mode & FMODE_READ)) {
		rc = q6asm_open_read_write(audio->ac, FORMAT_LINEAR_PCM,
					   FORMAT_G711_ALAW_FS);
		if (rc < 0) {
			pr_err("%s: NT mode Open failed rc=%d\n", __func__, rc);
			goto fail;
		}
		audio->feedback = NON_TUNNEL_MODE;
		/* open G711 decoder, expected frames is always 1*/
		audio->buf_cfg.frames_per_buf = 0x01;
		audio->buf_cfg.meta_info_enable = 0x01;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
		rc = q6asm_open_write(audio->ac, FORMAT_G711_ALAW_FS);
		if (rc < 0) {
			pr_err("%s: T mode Open failed rc=%d\n", __func__, rc);
			goto fail;
		}
		audio->feedback = TUNNEL_MODE;
		audio->buf_cfg.meta_info_enable = 0x00;
	} else {
		pr_err("%s: %d mode is not supported mode\n",
				__func__, file->f_mode);
		rc = -EACCES;
		goto fail;
	}

	snprintf(name, sizeof(name), "msm_g711_%04x", audio->ac->session);
	audio->dentry = config_debugfs_create_file(name, (void *)audio);

	if (IS_ERR_OR_NULL(audio->dentry))
		pr_debug("%s: debugfs_create_file failed\n", __func__);
	pr_debug("%s: g711dec success mode[%d]session[%d]\n", __func__,
						audio->feedback,
						audio->ac->session);
	return rc;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio->codec_cfg);
	kfree(audio);
	return rc;
}

static int g711_channel_map(u8 *channel_mapping, uint32_t channels)
{
	u8 *lchannel_mapping;

	lchannel_mapping = channel_mapping;
	pr_debug("%s: channels passed: %d\n", __func__, channels);
	if (channels == 1)  {
		lchannel_mapping[0] = PCM_CHANNEL_FC;
	} else if (channels == 2) {
		lchannel_mapping[0] = PCM_CHANNEL_FL;
		lchannel_mapping[1] = PCM_CHANNEL_FR;
	} else if (channels == 3) {
		lchannel_mapping[0] = PCM_CHANNEL_FC;
		lchannel_mapping[1] = PCM_CHANNEL_FL;
		lchannel_mapping[2] = PCM_CHANNEL_FR;
	} else if (channels == 4) {
		lchannel_mapping[0] = PCM_CHANNEL_FC;
		lchannel_mapping[1] = PCM_CHANNEL_FL;
		lchannel_mapping[2] = PCM_CHANNEL_FR;
		lchannel_mapping[3] = PCM_CHANNEL_CS;
	} else if (channels == 5) {
		lchannel_mapping[0] = PCM_CHANNEL_FC;
		lchannel_mapping[1] = PCM_CHANNEL_FL;
		lchannel_mapping[2] = PCM_CHANNEL_FR;
		lchannel_mapping[3] = PCM_CHANNEL_LS;
		lchannel_mapping[4] = PCM_CHANNEL_RS;
	} else if (channels == 6) {
		lchannel_mapping[0] = PCM_CHANNEL_FC;
		lchannel_mapping[1] = PCM_CHANNEL_FL;
		lchannel_mapping[2] = PCM_CHANNEL_FR;
		lchannel_mapping[3] = PCM_CHANNEL_LS;
		lchannel_mapping[4] = PCM_CHANNEL_RS;
		lchannel_mapping[5] = PCM_CHANNEL_LFE;
	} else if (channels == 7) {
		lchannel_mapping[0] = PCM_CHANNEL_FC;
		lchannel_mapping[1] = PCM_CHANNEL_FL;
		lchannel_mapping[2] = PCM_CHANNEL_FR;
		lchannel_mapping[3] = PCM_CHANNEL_LS;
		lchannel_mapping[4] = PCM_CHANNEL_RS;
		lchannel_mapping[5] = PCM_CHANNEL_CS;
		lchannel_mapping[6] = PCM_CHANNEL_LFE;
	} else if (channels == 8) {
		lchannel_mapping[0] = PCM_CHANNEL_FC;
		lchannel_mapping[1] = PCM_CHANNEL_FLC;
		lchannel_mapping[2] = PCM_CHANNEL_FRC;
		lchannel_mapping[3] = PCM_CHANNEL_FL;
		lchannel_mapping[4] = PCM_CHANNEL_FR;
		lchannel_mapping[5] = PCM_CHANNEL_LS;
		lchannel_mapping[6] = PCM_CHANNEL_RS;
		lchannel_mapping[7] = PCM_CHANNEL_LFE;
	} else {
		pr_err("%s: ERROR.unsupported num_ch = %u\n",
				__func__, channels);
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations audio_g711_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_aio_release,
	.unlocked_ioctl = audio_ioctl,
	.compat_ioctl = audio_compat_ioctl,
	.fsync = audio_aio_fsync,
};

static struct miscdevice audio_g711alaw_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_g711alaw",
	.fops = &audio_g711_fops,
};

static int __init audio_g711alaw_init(void)
{
	int ret = misc_register(&audio_g711alaw_misc);

	if (ret == 0)
		device_init_wakeup(audio_g711alaw_misc.this_device, true);
	audio_g711_ws_mgr.ref_cnt = 0;
	mutex_init(&audio_g711_ws_mgr.ws_lock);

	return ret;
}
static void __exit audio_g711alaw_exit(void)
{
	misc_deregister(&audio_g711alaw_misc);
	mutex_destroy(&audio_g711_ws_mgr.ws_lock);
}

device_initcall(audio_g711alaw_init);
__exitcall(audio_g711alaw_exit);

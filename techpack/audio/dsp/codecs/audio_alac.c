/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/msm_audio_alac.h>
#include <linux/compat.h>
#include "audio_utils_aio.h"

static struct miscdevice audio_alac_misc;
static struct ws_mgr audio_alac_ws_mgr;

static const struct file_operations audio_alac_debug_fops = {
	.read = audio_aio_debug_read,
	.open = audio_aio_debug_open,
};

static struct dentry *config_debugfs_create_file(const char *name, void *data)
{
	return debugfs_create_file(name, S_IFREG | 0444,
				NULL, (void *)data, &audio_alac_debug_fops);
}

static int alac_channel_map(u8 *channel_mapping, uint32_t channels);

static long audio_ioctl_shared(struct file *file, unsigned int cmd,
						void *arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		struct asm_alac_cfg alac_cfg;
		struct msm_audio_alac_config *alac_config;
		u8 channel_mapping[PCM_FORMAT_MAX_NUM_CHANNEL];

		memset(channel_mapping, 0, PCM_FORMAT_MAX_NUM_CHANNEL);

		if (alac_channel_map(channel_mapping,
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
				pr_err("pcm output block config failed\n");
				break;
			}
		}
		alac_config = (struct msm_audio_alac_config *)audio->codec_cfg;
		alac_cfg.frame_length = alac_config->frameLength;
		alac_cfg.compatible_version = alac_config->compatVersion;
		alac_cfg.bit_depth = alac_config->bitDepth;
		alac_cfg.pb = alac_config->pb;
		alac_cfg.mb = alac_config->mb;
		alac_cfg.kb = alac_config->kb;
		alac_cfg.num_channels = alac_config->channelCount;
		alac_cfg.max_run = alac_config->maxRun;
		alac_cfg.max_frame_bytes = alac_config->maxSize;
		alac_cfg.avg_bit_rate = alac_config->averageBitRate;
		alac_cfg.sample_rate = alac_config->sampleRate;
		alac_cfg.channel_layout_tag = alac_config->channelLayout;
		pr_debug("%s: frame_length %d compatible_version %d bit_depth %d pb %d mb %d kb %d num_channels %d max_run %d max_frame_bytes %d avg_bit_rate %d sample_rate %d channel_layout_tag %d\n",
				__func__, alac_config->frameLength,
				alac_config->compatVersion,
				alac_config->bitDepth, alac_config->pb,
				alac_config->mb, alac_config->kb,
				alac_config->channelCount, alac_config->maxRun,
				alac_config->maxSize,
				alac_config->averageBitRate,
				alac_config->sampleRate,
				alac_config->channelLayout);
		/* Configure Media format block */
		rc = q6asm_media_format_block_alac(audio->ac, &alac_cfg,
							audio->ac->stream_id);
		if (rc < 0) {
			pr_err("cmd media format block failed\n");
			break;
		}
		rc = audio_aio_enable(audio);
		audio->eos_rsp = 0;
		audio->eos_flag = 0;
		if (!rc) {
			audio->enabled = 1;
		} else {
			audio->enabled = 0;
			pr_err("Audio Start procedure failed rc=%d\n", rc);
			break;
		}
		pr_debug("AUDIO_START success enable[%d]\n", audio->enabled);
		if (audio->stopped == 1)
			audio->stopped = 0;
		break;
	}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
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
	case AUDIO_GET_ALAC_CONFIG: {
		if (copy_to_user((void *)arg, audio->codec_cfg,
			sizeof(struct msm_audio_alac_config))) {
			pr_err("%s:copy_to_user for AUDIO_GET_ALAC_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_SET_ALAC_CONFIG: {
		if (copy_from_user(audio->codec_cfg, (void *)arg,
			sizeof(struct msm_audio_alac_config))) {
			pr_err("%s:copy_from_user for AUDIO_SET_ALAC_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	default: {
		rc = audio->codec_ioctl(file, cmd, arg);
		if (rc)
			pr_err("Failed in utils_ioctl: %d\n", rc);
		break;
	}
	}
	return rc;
}

#ifdef CONFIG_COMPAT
struct msm_audio_alac_config_32 {
	u32 frameLength;
	u8 compatVersion;
	u8 bitDepth;
	u8 pb;
	u8 mb;
	u8 kb;
	u8 channelCount;
	u16 maxRun;
	u32 maxSize;
	u32 averageBitRate;
	u32 sampleRate;
	u32 channelLayout;
};

enum {
	AUDIO_GET_ALAC_CONFIG_32 =  _IOR(AUDIO_IOCTL_MAGIC,
	(AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_alac_config_32),
	AUDIO_SET_ALAC_CONFIG_32 =  _IOW(AUDIO_IOCTL_MAGIC,
	(AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_alac_config_32)
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
	case AUDIO_GET_ALAC_CONFIG_32: {
		struct msm_audio_alac_config *alac_config;
		struct msm_audio_alac_config_32 alac_config_32;

		memset(&alac_config_32, 0, sizeof(alac_config_32));

		alac_config = (struct msm_audio_alac_config *)audio->codec_cfg;
		alac_config_32.frameLength = alac_config->frameLength;
		alac_config_32.compatVersion =
				alac_config->compatVersion;
		alac_config_32.bitDepth = alac_config->bitDepth;
		alac_config_32.pb = alac_config->pb;
		alac_config_32.mb = alac_config->mb;
		alac_config_32.kb = alac_config->kb;
		alac_config_32.channelCount = alac_config->channelCount;
		alac_config_32.maxRun = alac_config->maxRun;
		alac_config_32.maxSize = alac_config->maxSize;
		alac_config_32.averageBitRate = alac_config->averageBitRate;
		alac_config_32.sampleRate = alac_config->sampleRate;
		alac_config_32.channelLayout = alac_config->channelLayout;

		if (copy_to_user((void *)arg, &alac_config_32,
			sizeof(alac_config_32))) {
			pr_err("%s: copy_to_user for GET_ALAC_CONFIG_32 failed\n",
				 __func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_SET_ALAC_CONFIG_32: {
		struct msm_audio_alac_config *alac_config;
		struct msm_audio_alac_config_32 alac_config_32;

		if (copy_from_user(&alac_config_32, (void *)arg,
			sizeof(alac_config_32))) {
			pr_err("%s: copy_from_user for SET_ALAC_CONFIG_32 failed\n"
				, __func__);
			rc = -EFAULT;
			break;
		}
		alac_config = (struct msm_audio_alac_config *)audio->codec_cfg;
		alac_config->frameLength = alac_config_32.frameLength;
		alac_config->compatVersion =
				alac_config_32.compatVersion;
		alac_config->bitDepth = alac_config_32.bitDepth;
		alac_config->pb = alac_config_32.pb;
		alac_config->mb = alac_config_32.mb;
		alac_config->kb = alac_config_32.kb;
		alac_config->channelCount = alac_config_32.channelCount;
		alac_config->maxRun = alac_config_32.maxRun;
		alac_config->maxSize = alac_config_32.maxSize;
		alac_config->averageBitRate = alac_config_32.averageBitRate;
		alac_config->sampleRate = alac_config_32.sampleRate;
		alac_config->channelLayout = alac_config_32.channelLayout;

		break;
	}
	default: {
		rc = audio->codec_compat_ioctl(file, cmd, arg);
		if (rc)
			pr_err("Failed in utils_ioctl: %d\n", rc);
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
	char name[sizeof "msm_alac_" + 5];

	audio = kzalloc(sizeof(struct q6audio_aio), GFP_KERNEL);
	if (!audio)
		return -ENOMEM;

	audio->codec_cfg = kzalloc(sizeof(struct msm_audio_alac_config),
					GFP_KERNEL);
	if (!audio->codec_cfg) {
		kfree(audio);
		return -ENOMEM;
	}

	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN;
	audio->miscdevice = &audio_alac_misc;
	audio->wakelock_voted = false;
	audio->audio_ws_mgr = &audio_alac_ws_mgr;

	audio->ac = q6asm_audio_client_alloc((app_cb) q6_audio_cb,
					     (void *)audio);

	if (!audio->ac) {
		pr_err("Could not allocate memory for audio client\n");
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
	/* open in T/NT mode */
	if ((file->f_mode & FMODE_WRITE) && (file->f_mode & FMODE_READ)) {
		rc = q6asm_open_read_write(audio->ac, FORMAT_LINEAR_PCM,
					   FORMAT_ALAC);
		if (rc < 0) {
			pr_err("NT mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = NON_TUNNEL_MODE;
		/* open ALAC decoder, expected frames is always 1*/
		audio->buf_cfg.frames_per_buf = 0x01;
		audio->buf_cfg.meta_info_enable = 0x01;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
		rc = q6asm_open_write(audio->ac, FORMAT_ALAC);
		if (rc < 0) {
			pr_err("T mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = TUNNEL_MODE;
		audio->buf_cfg.meta_info_enable = 0x00;
	} else {
		pr_err("Not supported mode\n");
		rc = -EACCES;
		goto fail;
	}

	snprintf(name, sizeof(name), "msm_alac_%04x", audio->ac->session);
	audio->dentry = config_debugfs_create_file(name, (void *)audio);

	if (IS_ERR_OR_NULL(audio->dentry))
		pr_debug("debugfs_create_file failed\n");
	pr_debug("%s:alacdec success mode[%d]session[%d]\n", __func__,
						audio->feedback,
						audio->ac->session);
	return rc;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio->codec_cfg);
	kfree(audio);
	return rc;
}

static int alac_channel_map(u8 *channel_mapping, uint32_t channels)
{
	u8 *lchannel_mapping;

	lchannel_mapping = channel_mapping;
	pr_debug("%s:  channels passed: %d\n", __func__, channels);
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

static const struct file_operations audio_alac_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_aio_release,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audio_aio_fsync,
	.compat_ioctl = audio_compat_ioctl
};

static struct miscdevice audio_alac_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_alac",
	.fops = &audio_alac_fops,
};

static int __init audio_alac_init(void)
{
	int ret = misc_register(&audio_alac_misc);

	if (ret == 0)
		device_init_wakeup(audio_alac_misc.this_device, true);
	audio_alac_ws_mgr.ref_cnt = 0;
	mutex_init(&audio_alac_ws_mgr.ws_lock);

	return ret;
}

device_initcall(audio_alac_init);

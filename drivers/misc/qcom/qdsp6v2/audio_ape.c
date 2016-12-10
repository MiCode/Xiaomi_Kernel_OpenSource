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
#include <linux/msm_audio_ape.h>
#include <linux/compat.h>
#include "audio_utils_aio.h"

static struct miscdevice audio_ape_misc;
static struct ws_mgr audio_ape_ws_mgr;

static const struct file_operations audio_ape_debug_fops = {
	.read = audio_aio_debug_read,
	.open = audio_aio_debug_open,
};
static struct dentry *config_debugfs_create_file(const char *name, void *data)
{
	return debugfs_create_file(name, S_IFREG | 0444,
			NULL, (void *)data, &audio_ape_debug_fops);
}

static long audio_ioctl_shared(struct file *file, unsigned int cmd,
						void *arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		struct asm_ape_cfg ape_cfg;
		struct msm_audio_ape_config *ape_config;

		pr_debug("%s[%pK]: AUDIO_START session_id[%d]\n", __func__,
						audio, audio->ac->session);
		if (audio->feedback == NON_TUNNEL_MODE) {
			/* Configure PCM output block */
			rc = q6asm_enc_cfg_blk_pcm(audio->ac,
					audio->pcm_cfg.sample_rate,
					audio->pcm_cfg.channel_count);
			if (rc < 0) {
				pr_err("pcm output block config failed\n");
				break;
			}
		}
		ape_config = (struct msm_audio_ape_config *)audio->codec_cfg;
		ape_cfg.compatible_version = ape_config->compatibleVersion;
		ape_cfg.compression_level = ape_config->compressionLevel;
		ape_cfg.format_flags = ape_config->formatFlags;
		ape_cfg.blocks_per_frame = ape_config->blocksPerFrame;
		ape_cfg.final_frame_blocks = ape_config->finalFrameBlocks;
		ape_cfg.total_frames = ape_config->totalFrames;
		ape_cfg.bits_per_sample = ape_config->bitsPerSample;
		ape_cfg.num_channels = ape_config->numChannels;
		ape_cfg.sample_rate = ape_config->sampleRate;
		ape_cfg.seek_table_present = ape_config->seekTablePresent;
		pr_debug("%s: compatibleVersion %d compressionLevel %d formatFlags %d blocksPerFrame %d finalFrameBlocks %d totalFrames %d bitsPerSample %d numChannels %d sampleRate %d seekTablePresent %d\n",
				__func__, ape_config->compatibleVersion,
				ape_config->compressionLevel,
				ape_config->formatFlags,
				ape_config->blocksPerFrame,
				ape_config->finalFrameBlocks,
				ape_config->totalFrames,
				ape_config->bitsPerSample,
				ape_config->numChannels,
				ape_config->sampleRate,
				ape_config->seekTablePresent);
		/* Configure Media format block */
		rc = q6asm_media_format_block_ape(audio->ac, &ape_cfg,
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
	case AUDIO_GET_APE_CONFIG: {
		if (copy_to_user((void *)arg, audio->codec_cfg,
			sizeof(struct msm_audio_ape_config))) {
			pr_err("%s:copy_to_user for AUDIO_GET_APE_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_SET_APE_CONFIG: {
		if (copy_from_user(audio->codec_cfg, (void *)arg,
			sizeof(struct msm_audio_ape_config))) {
			pr_err("%s:copy_from_user for AUDIO_SET_APE_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	default: {
		pr_debug("%s[%pK]: Calling utils ioctl\n", __func__, audio);
		rc = audio->codec_ioctl(file, cmd, arg);
		if (rc)
			pr_err("Failed in utils_ioctl: %d\n", rc);
		break;
	}
	}
	return rc;
}

#ifdef CONFIG_COMPAT
struct msm_audio_ape_config_32 {
	u16 compatibleVersion;
	u16 compressionLevel;
	u32 formatFlags;
	u32 blocksPerFrame;
	u32 finalFrameBlocks;
	u32 totalFrames;
	u16 bitsPerSample;
	u16 numChannels;
	u32 sampleRate;
	u32 seekTablePresent;

};

enum {
	AUDIO_GET_APE_CONFIG_32 =  _IOR(AUDIO_IOCTL_MAGIC,
	(AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_ape_config_32),
	AUDIO_SET_APE_CONFIG_32 =  _IOW(AUDIO_IOCTL_MAGIC,
	(AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_ape_config_32)
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
	case AUDIO_GET_APE_CONFIG_32: {
		struct msm_audio_ape_config *ape_config;
		struct msm_audio_ape_config_32 ape_config_32;

		memset(&ape_config_32, 0, sizeof(ape_config_32));

		ape_config = (struct msm_audio_ape_config *)audio->codec_cfg;
		ape_config_32.compatibleVersion = ape_config->compatibleVersion;
		ape_config_32.compressionLevel =
				ape_config->compressionLevel;
		ape_config_32.formatFlags = ape_config->formatFlags;
		ape_config_32.blocksPerFrame = ape_config->blocksPerFrame;
		ape_config_32.finalFrameBlocks = ape_config->finalFrameBlocks;
		ape_config_32.totalFrames = ape_config->totalFrames;
		ape_config_32.bitsPerSample = ape_config->bitsPerSample;
		ape_config_32.numChannels = ape_config->numChannels;
		ape_config_32.sampleRate = ape_config->sampleRate;
		ape_config_32.seekTablePresent = ape_config->seekTablePresent;

		if (copy_to_user((void *)arg, &ape_config_32,
			sizeof(ape_config_32))) {
			pr_err("%s: copy_to_user for GET_APE_CONFIG_32 failed\n",
				 __func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_SET_APE_CONFIG_32: {
		struct msm_audio_ape_config *ape_config;
		struct msm_audio_ape_config_32 ape_config_32;

		if (copy_from_user(&ape_config_32, (void *)arg,
			sizeof(ape_config_32))) {
			pr_err("%s: copy_from_user for SET_APE_CONFIG_32 failed\n"
				, __func__);
			rc = -EFAULT;
			break;
		}
		ape_config = (struct msm_audio_ape_config *)audio->codec_cfg;
		ape_config->compatibleVersion = ape_config_32.compatibleVersion;
		ape_config->compressionLevel =
				ape_config_32.compressionLevel;
		ape_config->formatFlags = ape_config_32.formatFlags;
		ape_config->blocksPerFrame = ape_config_32.blocksPerFrame;
		ape_config->finalFrameBlocks = ape_config_32.finalFrameBlocks;
		ape_config->totalFrames = ape_config_32.totalFrames;
		ape_config->bitsPerSample = ape_config_32.bitsPerSample;
		ape_config->numChannels = ape_config_32.numChannels;
		ape_config->sampleRate = ape_config_32.sampleRate;
		ape_config->seekTablePresent = ape_config_32.seekTablePresent;

		break;
	}
	default: {
		pr_debug("%s[%pK]: Calling utils ioctl\n", __func__, audio);
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
	char name[sizeof "msm_ape_" + 5];

	audio = kzalloc(sizeof(struct q6audio_aio), GFP_KERNEL);
	if (!audio)
		return -ENOMEM;

	audio->codec_cfg = kzalloc(sizeof(struct msm_audio_ape_config),
					GFP_KERNEL);
	if (!audio->codec_cfg) {
		kfree(audio);
		return -ENOMEM;
	}

	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN;
	audio->miscdevice = &audio_ape_misc;
	audio->wakelock_voted = false;
	audio->audio_ws_mgr = &audio_ape_ws_mgr;

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
					   FORMAT_APE);
		if (rc < 0) {
			pr_err("NT mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = NON_TUNNEL_MODE;
		/* open APE decoder, expected frames is always 1*/
		audio->buf_cfg.frames_per_buf = 0x01;
		audio->buf_cfg.meta_info_enable = 0x01;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
		rc = q6asm_open_write(audio->ac, FORMAT_APE);
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

	snprintf(name, sizeof(name), "msm_ape_%04x", audio->ac->session);
	audio->dentry = config_debugfs_create_file(name, (void *)audio);

	if (IS_ERR_OR_NULL(audio->dentry))
		pr_debug("debugfs_create_file failed\n");
	pr_debug("%s:apedec success mode[%d]session[%d]\n", __func__,
						audio->feedback,
						audio->ac->session);
	return rc;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio->codec_cfg);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_ape_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_aio_release,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audio_aio_fsync,
	.compat_ioctl = audio_compat_ioctl
};

static struct miscdevice audio_ape_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_ape",
	.fops = &audio_ape_fops,
};

static int __init audio_ape_init(void)
{
	int ret = misc_register(&audio_ape_misc);

	if (ret == 0)
		device_init_wakeup(audio_ape_misc.this_device, true);
	audio_ape_ws_mgr.ref_cnt = 0;
	mutex_init(&audio_ape_ws_mgr.ws_lock);

	return ret;
}

device_initcall(audio_ape_init);

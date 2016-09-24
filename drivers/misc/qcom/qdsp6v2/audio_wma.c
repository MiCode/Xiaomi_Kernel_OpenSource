/* wma audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2009-2015, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/msm_audio_wma.h>
#include <linux/compat.h>
#include <linux/wakelock.h>
#include "audio_utils_aio.h"

struct miscdevice audio_wma_misc;
struct ws_mgr audio_wma_ws_mgr;

#ifdef CONFIG_DEBUG_FS
static const struct file_operations audio_wma_debug_fops = {
	.read = audio_aio_debug_read,
	.open = audio_aio_debug_open,
};
#endif

static long audio_ioctl_shared(struct file *file, unsigned int cmd,
						void *arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		struct asm_wma_cfg wma_cfg;
		struct msm_audio_wma_config_v2 *wma_config;
		pr_debug("%s[%p]: AUDIO_START session_id[%d]\n", __func__,
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
		wma_config = (struct msm_audio_wma_config_v2 *)audio->codec_cfg;
		wma_cfg.format_tag = wma_config->format_tag;
		wma_cfg.ch_cfg = wma_config->numchannels;
		wma_cfg.sample_rate =  wma_config->samplingrate;
		wma_cfg.avg_bytes_per_sec = wma_config->avgbytespersecond;
		wma_cfg.block_align = wma_config->block_align;
		wma_cfg.valid_bits_per_sample =
				wma_config->validbitspersample;
		wma_cfg.ch_mask =  wma_config->channelmask;
		wma_cfg.encode_opt = wma_config->encodeopt;
		/* Configure Media format block */
		rc = q6asm_media_format_block_wma(audio->ac, &wma_cfg);
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
	case AUDIO_GET_WMA_CONFIG_V2: {
		if (copy_to_user((void *)arg, audio->codec_cfg,
			sizeof(struct msm_audio_wma_config_v2))) {
			pr_err("%s:copy_to_user for AUDIO_SET_WMA_CONFIG_V2 failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_SET_WMA_CONFIG_V2: {
		if (copy_from_user(audio->codec_cfg, (void *)arg,
			sizeof(struct msm_audio_wma_config_v2))) {
			pr_err("%s:copy_from_user for AUDIO_SET_WMA_CONFIG_V2 failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	default: {
		pr_debug("%s[%p]: Calling utils ioctl\n", __func__, audio);
		rc = audio->codec_ioctl(file, cmd, arg);
		if (rc)
			pr_err("Failed in utils_ioctl: %d\n", rc);
		break;
	}
	}
	return rc;
}

#ifdef CONFIG_COMPAT
struct msm_audio_wma_config_v2_32 {
	u16 format_tag;
	u16 numchannels;
	u32 samplingrate;
	u32 avgbytespersecond;
	u16 block_align;
	u16 validbitspersample;
	u32 channelmask;
	u16 encodeopt;
};

enum {
	AUDIO_GET_WMA_CONFIG_V2_32 =  _IOR(AUDIO_IOCTL_MAGIC,
	(AUDIO_MAX_COMMON_IOCTL_NUM+2), struct msm_audio_wma_config_v2_32),
	AUDIO_SET_WMA_CONFIG_V2_32 =  _IOW(AUDIO_IOCTL_MAGIC,
	(AUDIO_MAX_COMMON_IOCTL_NUM+3), struct msm_audio_wma_config_v2_32)
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
	case AUDIO_GET_WMA_CONFIG_V2_32: {
		struct msm_audio_wma_config_v2 *wma_config;
		struct msm_audio_wma_config_v2_32 wma_config_32;

		memset(&wma_config_32, 0, sizeof(wma_config_32));

		wma_config = (struct msm_audio_wma_config_v2 *)audio->codec_cfg;
		wma_config_32.format_tag = wma_config->format_tag;
		wma_config_32.numchannels = wma_config->numchannels;
		wma_config_32.samplingrate = wma_config->samplingrate;
		wma_config_32.avgbytespersecond = wma_config->avgbytespersecond;
		wma_config_32.block_align = wma_config->block_align;
		wma_config_32.validbitspersample =
					wma_config->validbitspersample;
		wma_config_32.channelmask = wma_config->channelmask;
		wma_config_32.encodeopt = wma_config->encodeopt;
		if (copy_to_user((void *)arg, &wma_config_32,
			sizeof(wma_config_32))) {
			pr_err("%s: copy_to_user for GET_WMA_CONFIG_V2_32 failed\n",
				 __func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_SET_WMA_CONFIG_V2_32: {
		struct msm_audio_wma_config_v2 *wma_config;
		struct msm_audio_wma_config_v2_32 wma_config_32;

		if (copy_from_user(&wma_config_32, (void *)arg,
			sizeof(wma_config_32))) {
			pr_err("%s: copy_from_user for SET_WMA_CONFIG_V2_32 failed\n"
				, __func__);
			rc = -EFAULT;
			break;
		}
		wma_config = (struct msm_audio_wma_config_v2 *)audio->codec_cfg;
		wma_config->format_tag = wma_config_32.format_tag;
		wma_config->numchannels = wma_config_32.numchannels;
		wma_config->samplingrate = wma_config_32.samplingrate;
		wma_config->avgbytespersecond = wma_config_32.avgbytespersecond;
		wma_config->block_align = wma_config_32.block_align;
		wma_config->validbitspersample =
				wma_config_32.validbitspersample;
		wma_config->channelmask = wma_config_32.channelmask;
		wma_config->encodeopt = wma_config_32.encodeopt;
		break;
	}
	default: {
		pr_debug("%s[%p]: Calling utils ioctl\n", __func__, audio);
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

#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_wma_" + 5];
#endif
	audio = kzalloc(sizeof(struct q6audio_aio), GFP_KERNEL);

	if (audio == NULL) {
		pr_err("Could not allocate memory for wma decode driver\n");
		return -ENOMEM;
	}
	audio->codec_cfg = kzalloc(sizeof(struct msm_audio_wma_config_v2),
					GFP_KERNEL);
	if (audio->codec_cfg == NULL) {
		pr_err("%s:Could not allocate memory for wma"
			"config\n", __func__);
		kfree(audio);
		return -ENOMEM;
	}

	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN;
	audio->miscdevice = &audio_wma_misc;
	audio->wakelock_voted = false;
	audio->audio_ws_mgr = &audio_wma_ws_mgr;

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
					   FORMAT_WMA_V9);
		if (rc < 0) {
			pr_err("NT mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = NON_TUNNEL_MODE;
		/* open WMA decoder, expected frames is always 1*/
		audio->buf_cfg.frames_per_buf = 0x01;
		audio->buf_cfg.meta_info_enable = 0x01;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
		rc = q6asm_open_write(audio->ac, FORMAT_WMA_V9);
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

#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof name, "msm_wma_%04x", audio->ac->session);
	audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
					    NULL, (void *)audio,
					    &audio_wma_debug_fops);

	if (IS_ERR(audio->dentry))
		pr_debug("debugfs_create_file failed\n");
#endif
	pr_info("%s:wmadec success mode[%d]session[%d]\n", __func__,
						audio->feedback,
						audio->ac->session);
	return rc;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio->codec_cfg);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_wma_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_aio_release,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audio_aio_fsync,
	.compat_ioctl = audio_compat_ioctl
};

struct miscdevice audio_wma_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_wma",
	.fops = &audio_wma_fops,
};

static int __init audio_wma_init(void)
{
	int ret = misc_register(&audio_wma_misc);

	if (ret == 0)
		device_init_wakeup(audio_wma_misc.this_device, true);
	audio_wma_ws_mgr.ref_cnt = 0;
	mutex_init(&audio_wma_ws_mgr.ws_lock);

	return ret;
}

device_initcall(audio_wma_init);

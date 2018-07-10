/* wmapro audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2009-2017, The Linux Foundation. All rights reserved.
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
#include <linux/msm_audio_wmapro.h>
#include <linux/compat.h>
#include "audio_utils_aio.h"

static struct miscdevice audio_wmapro_misc;
static struct ws_mgr audio_wmapro_ws_mgr;

#ifdef CONFIG_DEBUG_FS
static const struct file_operations audio_wmapro_debug_fops = {
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
		struct asm_wmapro_cfg wmapro_cfg;
		struct msm_audio_wmapro_config *wmapro_config;
		pr_debug("%s: AUDIO_START session_id[%d]\n", __func__,
						audio->ac->session);
		if (audio->feedback == NON_TUNNEL_MODE) {
			/* Configure PCM output block */
			rc = q6asm_enc_cfg_blk_pcm_v2(audio->ac,
					audio->pcm_cfg.sample_rate,
					audio->pcm_cfg.channel_count,
					16, /* bits per sample */
					true, /* use default channel map */
					true, /* use back channel map flavor */
					NULL);
			if (rc < 0) {
				pr_err("pcm output block config failed\n");
				break;
			}
		}
		wmapro_config = (struct msm_audio_wmapro_config *)
				audio->codec_cfg;
		if ((wmapro_config->formattag == 0x162) ||
		(wmapro_config->formattag == 0x163) ||
		(wmapro_config->formattag == 0x166) ||
		(wmapro_config->formattag == 0x167)) {
			wmapro_cfg.format_tag = wmapro_config->formattag;
		} else {
			pr_err("%s:AUDIO_START failed: formattag = %d\n",
				__func__, wmapro_config->formattag);
			rc = -EINVAL;
			break;
		}
		if (wmapro_config->numchannels > 0) {
			wmapro_cfg.ch_cfg = wmapro_config->numchannels;
		} else {
			pr_err("%s:AUDIO_START failed: channels = %d\n",
				__func__, wmapro_config->numchannels);
			rc = -EINVAL;
			break;
		}
		if (wmapro_config->samplingrate > 0) {
			wmapro_cfg.sample_rate = wmapro_config->samplingrate;
		} else {
			pr_err("%s:AUDIO_START failed: sample_rate = %d\n",
				__func__, wmapro_config->samplingrate);
			rc = -EINVAL;
			break;
		}
		wmapro_cfg.avg_bytes_per_sec =
				wmapro_config->avgbytespersecond;
		if ((wmapro_config->asfpacketlength <= 13376) &&
			(wmapro_config->asfpacketlength > 0)) {
			wmapro_cfg.block_align =
				wmapro_config->asfpacketlength;
		} else {
			pr_err("%s:AUDIO_START failed: block_align = %d\n",
				__func__, wmapro_config->asfpacketlength);
			rc = -EINVAL;
			break;
		}
		if ((wmapro_config->validbitspersample == 16) ||
			(wmapro_config->validbitspersample == 24)) {
			wmapro_cfg.valid_bits_per_sample =
				wmapro_config->validbitspersample;
		} else {
			pr_err("%s:AUDIO_START failed: bitspersample = %d\n",
				__func__, wmapro_config->validbitspersample);
			rc = -EINVAL;
			break;
		}
		wmapro_cfg.ch_mask = wmapro_config->channelmask;
		wmapro_cfg.encode_opt = wmapro_config->encodeopt;
		wmapro_cfg.adv_encode_opt =
				wmapro_config->advancedencodeopt;
		wmapro_cfg.adv_encode_opt2 =
				wmapro_config->advancedencodeopt2;
		/* Configure Media format block */
		rc = q6asm_media_format_block_wmapro(audio->ac, &wmapro_cfg,
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
		pr_err("%s: Unkown ioctl cmd %d\n", __func__, cmd);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_GET_WMAPRO_CONFIG: {
		if (copy_to_user((void *)arg, audio->codec_cfg,
			 sizeof(struct msm_audio_wmapro_config))) {
			pr_err("%s: copy_to_user for AUDIO_GET_WMAPRO_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_SET_WMAPRO_CONFIG: {
		if (copy_from_user(audio->codec_cfg, (void *)arg,
			sizeof(struct msm_audio_wmapro_config))) {
			pr_err("%s: copy_from_user for AUDIO_SET_WMAPRO_CONFIG_V2 failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_START: {
		rc = audio_ioctl_shared(file, cmd, (void *)arg);
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

struct msm_audio_wmapro_config32 {
	u16 armdatareqthr;
	u8  validbitspersample;
	u8  numchannels;
	u16 formattag;
	u32 samplingrate;
	u32 avgbytespersecond;
	u16 asfpacketlength;
	u32 channelmask;
	u16 encodeopt;
	u16 advancedencodeopt;
	u32 advancedencodeopt2;
};

enum {
	AUDIO_GET_WMAPRO_CONFIG_32 = _IOR(AUDIO_IOCTL_MAGIC,
	  (AUDIO_MAX_COMMON_IOCTL_NUM+0), struct msm_audio_wmapro_config32),
	AUDIO_SET_WMAPRO_CONFIG_32 = _IOW(AUDIO_IOCTL_MAGIC,
	  (AUDIO_MAX_COMMON_IOCTL_NUM+1), struct msm_audio_wmapro_config32)
};

static long audio_compat_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_GET_WMAPRO_CONFIG_32: {
		struct msm_audio_wmapro_config *wmapro_config;
		struct msm_audio_wmapro_config32 wmapro_config_32;

		memset(&wmapro_config_32, 0, sizeof(wmapro_config_32));

		wmapro_config =
			(struct msm_audio_wmapro_config *)audio->codec_cfg;
		wmapro_config_32.armdatareqthr = wmapro_config->armdatareqthr;
		wmapro_config_32.validbitspersample =
					wmapro_config->validbitspersample;
		wmapro_config_32.numchannels = wmapro_config->numchannels;
		wmapro_config_32.formattag = wmapro_config->formattag;
		wmapro_config_32.samplingrate = wmapro_config->samplingrate;
		wmapro_config_32.avgbytespersecond =
					wmapro_config->avgbytespersecond;
		wmapro_config_32.asfpacketlength =
					wmapro_config->asfpacketlength;
		wmapro_config_32.channelmask = wmapro_config->channelmask;
		wmapro_config_32.encodeopt = wmapro_config->encodeopt;
		wmapro_config_32.advancedencodeopt =
					wmapro_config->advancedencodeopt;
		wmapro_config_32.advancedencodeopt2 =
					wmapro_config->advancedencodeopt2;

		if (copy_to_user((void *)arg, &wmapro_config_32,
			 sizeof(struct msm_audio_wmapro_config32))) {
			pr_err("%s: copy_to_user for AUDIO_GET_WMAPRO_CONFIG_V2_32 failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_SET_WMAPRO_CONFIG_32: {
		struct msm_audio_wmapro_config *wmapro_config;
		struct msm_audio_wmapro_config32 wmapro_config_32;

		if (copy_from_user(&wmapro_config_32, (void *)arg,
			sizeof(struct msm_audio_wmapro_config32))) {
			pr_err(
				"%s: copy_from_user for AUDIO_SET_WMAPRO_CONFG_V2_32 failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		wmapro_config =
			(struct msm_audio_wmapro_config *)audio->codec_cfg;
		wmapro_config->armdatareqthr = wmapro_config_32.armdatareqthr;
		wmapro_config->validbitspersample =
					wmapro_config_32.validbitspersample;
		wmapro_config->numchannels = wmapro_config_32.numchannels;
		wmapro_config->formattag = wmapro_config_32.formattag;
		wmapro_config->samplingrate = wmapro_config_32.samplingrate;
		wmapro_config->avgbytespersecond =
					wmapro_config_32.avgbytespersecond;
		wmapro_config->asfpacketlength =
					wmapro_config_32.asfpacketlength;
		wmapro_config->channelmask = wmapro_config_32.channelmask;
		wmapro_config->encodeopt = wmapro_config_32.encodeopt;
		wmapro_config->advancedencodeopt =
					wmapro_config_32.advancedencodeopt;
		wmapro_config->advancedencodeopt2 =
					wmapro_config_32.advancedencodeopt2;
		break;
	}
	case AUDIO_START: {
		rc = audio_ioctl_shared(file, cmd, (void *)arg);
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

#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_wmapro_" + 5];
#endif
	audio = kzalloc(sizeof(struct q6audio_aio), GFP_KERNEL);

	if (audio == NULL) {
		pr_err("Could not allocate memory for wma decode driver\n");
		return -ENOMEM;
	}
	audio->codec_cfg = kzalloc(sizeof(struct msm_audio_wmapro_config),
					GFP_KERNEL);
	if (audio->codec_cfg == NULL) {
		pr_err("%s: Could not allocate memory for wmapro"
			"config\n", __func__);
		kfree(audio);
		return -ENOMEM;
	}


	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN;
	audio->miscdevice = &audio_wmapro_misc;
	audio->wakelock_voted = false;
	audio->audio_ws_mgr = &audio_wmapro_ws_mgr;

	init_waitqueue_head(&audio->event_wait);

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
					   FORMAT_WMA_V10PRO);
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
		rc = q6asm_open_write(audio->ac, FORMAT_WMA_V10PRO);
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
	snprintf(name, sizeof name, "msm_wmapro_%04x", audio->ac->session);
	audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
					    NULL, (void *)audio,
					    &audio_wmapro_debug_fops);

	if (IS_ERR(audio->dentry))
		pr_debug("debugfs_create_file failed\n");
#endif
	pr_info("%s:wmapro decoder open success, session_id = %d\n", __func__,
				audio->ac->session);
	return rc;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio->codec_cfg);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_wmapro_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_aio_release,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audio_aio_fsync,
	.compat_ioctl = audio_compat_ioctl
};

static struct miscdevice audio_wmapro_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_wmapro",
	.fops = &audio_wmapro_fops,
};

static int __init audio_wmapro_init(void)
{
	int ret = misc_register(&audio_wmapro_misc);

	if (ret == 0)
		device_init_wakeup(audio_wmapro_misc.this_device, true);
	audio_wmapro_ws_mgr.ref_cnt = 0;
	mutex_init(&audio_wmapro_ws_mgr.ws_lock);

	return ret;
}

device_initcall(audio_wmapro_init);

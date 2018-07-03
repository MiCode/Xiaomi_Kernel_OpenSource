/* amr-wbplus audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2010-2014,2016, The Linux Foundation. All rights reserved.
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
#include <linux/msm_audio_amrwbplus.h>
#include <linux/compat.h>
#include "audio_utils_aio.h"

#ifdef CONFIG_DEBUG_FS
static const struct file_operations audio_amrwbplus_debug_fops = {
	.read = audio_aio_debug_read,
	.open = audio_aio_debug_open,
};
static void config_debug_fs(struct q6audio_aio *audio)
{
	if (audio != NULL) {
		char name[sizeof("msm_amrwbplus_") + 5];
		snprintf(name, sizeof(name), "msm_amrwbplus_%04x",
			audio->ac->session);
		audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
						NULL, (void *)audio,
						&audio_amrwbplus_debug_fops);
		if (IS_ERR(audio->dentry))
			pr_debug("debugfs_create_file failed\n");
	}
}
#else
static void config_debug_fs(struct q6audio_aio *audio)
{
}
#endif

static long audio_ioctl_shared(struct file *file, unsigned int cmd,
					void *arg)
{
	struct asm_amrwbplus_cfg q6_amrwbplus_cfg;
	struct msm_audio_amrwbplus_config_v2 *amrwbplus_drv_config;
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		pr_err("%s[%pK]: AUDIO_START session_id[%d]\n", __func__,
			audio, audio->ac->session);
		if (audio->feedback == NON_TUNNEL_MODE) {
			/* Configure PCM output block */
			rc = q6asm_enc_cfg_blk_pcm(audio->ac,
			audio->pcm_cfg.sample_rate,
			audio->pcm_cfg.channel_count,
			false);
			if (rc < 0) {
				pr_err("pcm output block config failed\n");
				break;
			}
		}
		amrwbplus_drv_config =
		(struct msm_audio_amrwbplus_config_v2 *)audio->codec_cfg;

		q6_amrwbplus_cfg.size_bytes     =
			amrwbplus_drv_config->size_bytes;
		q6_amrwbplus_cfg.version        =
			amrwbplus_drv_config->version;
		q6_amrwbplus_cfg.num_channels   =
			amrwbplus_drv_config->num_channels;
		q6_amrwbplus_cfg.amr_band_mode  =
			amrwbplus_drv_config->amr_band_mode;
		q6_amrwbplus_cfg.amr_dtx_mode   =
			amrwbplus_drv_config->amr_dtx_mode;
		q6_amrwbplus_cfg.amr_frame_fmt  =
			amrwbplus_drv_config->amr_frame_fmt;
		q6_amrwbplus_cfg.amr_lsf_idx    =
			amrwbplus_drv_config->amr_lsf_idx;

		rc = q6asm_media_format_block_amrwbplus(audio->ac,
							&q6_amrwbplus_cfg);
		if (rc < 0) {
			pr_err("q6asm_media_format_block_amrwb+ failed...\n");
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
		pr_debug("%s:AUDIO_START sessionid[%d]enable[%d]\n", __func__,
			audio->ac->session,
			audio->enabled);
		if (audio->stopped == 1)
			audio->stopped = 0;
			break;
		}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static long audio_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
		rc = audio_ioctl_shared(file, cmd, (void *)arg);
		break;
	}
	case AUDIO_GET_AMRWBPLUS_CONFIG_V2: {
		if ((audio) && (arg) && (audio->codec_cfg)) {
			if (copy_to_user((void *)arg, audio->codec_cfg,
				sizeof(struct msm_audio_amrwbplus_config_v2))) {
				rc = -EFAULT;
				pr_err("%s: copy_to_user for AUDIO_GET_AMRWBPLUS_CONFIG_V2 failed\n",
					__func__);
				break;
			}
			} else {
				pr_err("%s: wb+ config v2 invalid parameters\n"
					, __func__);
				rc = -EFAULT;
				break;
			}
		break;
	}
	case AUDIO_SET_AMRWBPLUS_CONFIG_V2: {
		if ((audio) && (arg) && (audio->codec_cfg)) {
			if (copy_from_user(audio->codec_cfg, (void *)arg,
				sizeof(struct msm_audio_amrwbplus_config_v2))) {
				rc = -EFAULT;
				pr_err("%s: copy_from_user for AUDIO_SET_AMRWBPLUS_CONFIG_V2 failed\n",
					__func__);
				break;
			}
			} else {
				pr_err("%s: wb+ config invalid parameters\n",
					__func__);
				rc = -EFAULT;
				break;
			}
		break;
	}
	default: {
		pr_debug("%s[%pK]: Calling utils ioctl\n", __func__, audio);
		rc = audio->codec_ioctl(file, cmd, arg);
		break;
	}
	}
	return rc;
}
#ifdef CONFIG_COMPAT
struct msm_audio_amrwbplus_config_v2_32 {
	u32 size_bytes;
	u32 version;
	u32 num_channels;
	u32 amr_band_mode;
	u32 amr_dtx_mode;
	u32 amr_frame_fmt;
	u32 amr_lsf_idx;
};

enum {
	AUDIO_GET_AMRWBPLUS_CONFIG_V2_32 = _IOR(AUDIO_IOCTL_MAGIC,
		(AUDIO_MAX_COMMON_IOCTL_NUM+2),
		struct msm_audio_amrwbplus_config_v2_32),
	AUDIO_SET_AMRWBPLUS_CONFIG_V2_32 = _IOW(AUDIO_IOCTL_MAGIC,
		(AUDIO_MAX_COMMON_IOCTL_NUM+3),
		struct msm_audio_amrwbplus_config_v2_32)
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
	case AUDIO_GET_AMRWBPLUS_CONFIG_V2_32: {
		if (audio && arg && (audio->codec_cfg)) {
			struct msm_audio_amrwbplus_config_v2 *amrwbplus_config;
			struct msm_audio_amrwbplus_config_v2_32
						amrwbplus_config_32;

			memset(&amrwbplus_config_32, 0,
					sizeof(amrwbplus_config_32));

			amrwbplus_config =
				(struct msm_audio_amrwbplus_config_v2 *)
				audio->codec_cfg;
			amrwbplus_config_32.size_bytes =
					amrwbplus_config->size_bytes;
			amrwbplus_config_32.version =
					amrwbplus_config->version;
			amrwbplus_config_32.num_channels =
					amrwbplus_config->num_channels;
			amrwbplus_config_32.amr_band_mode =
					amrwbplus_config->amr_band_mode;
			amrwbplus_config_32.amr_dtx_mode =
					amrwbplus_config->amr_dtx_mode;
			amrwbplus_config_32.amr_frame_fmt =
					amrwbplus_config->amr_frame_fmt;
			amrwbplus_config_32.amr_lsf_idx =
					amrwbplus_config->amr_lsf_idx;

			if (copy_to_user((void *)arg, &amrwbplus_config_32,
				sizeof(amrwbplus_config_32))) {
				rc = -EFAULT;
				pr_err("%s: copy_to_user for AUDIO_GET_AMRWBPLUS_CONFIG_V2_32 failed\n"
					, __func__);
			}
		} else {
			pr_err("%s: wb+ Get config v2 invalid parameters\n"
				, __func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_SET_AMRWBPLUS_CONFIG_V2_32: {
		if ((audio) && (arg) && (audio->codec_cfg)) {
			struct msm_audio_amrwbplus_config_v2 *amrwbplus_config;
			struct msm_audio_amrwbplus_config_v2_32
							amrwbplus_config_32;

			if (copy_from_user(&amrwbplus_config_32, (void *)arg,
			sizeof(struct msm_audio_amrwbplus_config_v2_32))) {
				rc = -EFAULT;
				pr_err("%s: copy_from_user for AUDIO_SET_AMRWBPLUS_CONFIG_V2_32 failed\n"
					, __func__);
				break;
			}
			amrwbplus_config =
			 (struct msm_audio_amrwbplus_config_v2 *)
						audio->codec_cfg;
			amrwbplus_config->size_bytes =
					amrwbplus_config_32.size_bytes;
			amrwbplus_config->version =
					amrwbplus_config_32.version;
			amrwbplus_config->num_channels =
					amrwbplus_config_32.num_channels;
			amrwbplus_config->amr_band_mode =
					amrwbplus_config_32.amr_band_mode;
			amrwbplus_config->amr_dtx_mode =
					amrwbplus_config_32.amr_dtx_mode;
			amrwbplus_config->amr_frame_fmt =
					amrwbplus_config_32.amr_frame_fmt;
			amrwbplus_config->amr_lsf_idx =
					amrwbplus_config_32.amr_lsf_idx;
		} else {
			pr_err("%s: wb+ config invalid parameters\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	default: {
		pr_debug("%s[%pK]: Calling utils ioctl\n", __func__, audio);
		rc = audio->codec_compat_ioctl(file, cmd, arg);
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

	audio = kzalloc(sizeof(struct q6audio_aio), GFP_KERNEL);

	if (audio == NULL) {
		pr_err("kzalloc failed for amrwb+ decode driver\n");
		return -ENOMEM;
	}
	audio->codec_cfg =
	kzalloc(sizeof(struct msm_audio_amrwbplus_config_v2), GFP_KERNEL);
	if (audio->codec_cfg == NULL) {
		pr_err("%s:failed kzalloc for amrwb+ config structure",
			__func__);
		kfree(audio);
		return -ENOMEM;
	}
	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN;

	audio->ac =
	q6asm_audio_client_alloc((app_cb) q6_audio_cb, (void *)audio);

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
					FORMAT_AMR_WB_PLUS);
		if (rc < 0) {
			pr_err("amrwbplus NT mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = NON_TUNNEL_MODE;
		audio->buf_cfg.frames_per_buf = 0x01;
		audio->buf_cfg.meta_info_enable = 0x01;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
			rc = q6asm_open_write(audio->ac, FORMAT_AMR_WB_PLUS);
			if (rc < 0) {
				pr_err("wb+ T mode Open failed rc=%d\n", rc);
				rc = -ENODEV;
				goto fail;
			}
		audio->feedback = TUNNEL_MODE;
		audio->buf_cfg.meta_info_enable = 0x00;
	} else {
		pr_err("audio_amrwbplus Not supported mode\n");
		rc = -EACCES;
		goto fail;
	}

	config_debug_fs(audio);
	pr_debug("%s: AMRWBPLUS dec success mode[%d]session[%d]\n", __func__,
		audio->feedback,
		audio->ac->session);
	return 0;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio->codec_cfg);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_amrwbplus_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_aio_release,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audio_aio_fsync,
	.compat_ioctl = audio_compat_ioctl
};

struct miscdevice audio_amrwbplus_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_amrwbplus",
	.fops = &audio_amrwbplus_fops,
};

static int __init audio_amrwbplus_init(void)
{
	return misc_register(&audio_amrwbplus_misc);
}

device_initcall(audio_amrwbplus_init);

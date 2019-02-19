/* mp3 audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#include "audio_utils_aio.h"

static struct miscdevice audio_mp3_misc;
static struct ws_mgr audio_mp3_ws_mgr;

#ifdef CONFIG_DEBUG_FS
static const struct file_operations audio_mp3_debug_fops = {
	.read = audio_aio_debug_read,
	.open = audio_aio_debug_open,
};
#endif

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct q6audio_aio *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_START: {
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

		rc = audio_aio_enable(audio);
		audio->eos_rsp = 0;
		audio->eos_flag = 0;
		if (!rc) {
			rc = enable_volume_ramp(audio);
			if (rc < 0) {
				pr_err("%s: Failed to enable volume ramp\n",
					__func__);
			}
			audio->enabled = 1;
		} else {
			audio->enabled = 0;
			pr_err("Audio Start procedure failed rc=%d\n", rc);
			break;
		}
		pr_info("%s: AUDIO_START sessionid[%d]enable[%d]\n", __func__,
						audio->ac->session,
						audio->enabled);
		if (audio->stopped == 1)
			audio->stopped = 0;
		break;
	}
	default:
		pr_debug("%s[%pK]: Calling utils ioctl\n", __func__, audio);
		rc = audio->codec_ioctl(file, cmd, arg);
	}
	return rc;
}

static int audio_open(struct inode *inode, struct file *file)
{
	struct q6audio_aio *audio = NULL;
	int rc = 0;

#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_mp3_" + 5];
#endif
	audio = kzalloc(sizeof(struct q6audio_aio), GFP_KERNEL);

	if (audio == NULL)
		return -ENOMEM;

	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN;
	audio->miscdevice = &audio_mp3_misc;
	audio->wakelock_voted = false;
	audio->audio_ws_mgr = &audio_mp3_ws_mgr;

	audio->ac = q6asm_audio_client_alloc((app_cb) q6_audio_cb,
					     (void *)audio);

	if (!audio->ac) {
		pr_err("Could not allocate memory for audio client\n");
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
					   FORMAT_MP3);
		if (rc < 0) {
			pr_err("NT mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = NON_TUNNEL_MODE;
		/* open MP3 decoder, expected frames is always 1
		 * audio->buf_cfg.frames_per_buf = 0x01;
		 */
		audio->buf_cfg.meta_info_enable = 0x01;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
		rc = q6asm_open_write(audio->ac, FORMAT_MP3);
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
	snprintf(name, sizeof(name), "msm_mp3_%04x", audio->ac->session);
	audio->dentry = debugfs_create_file(name, S_IFREG | 0444,
					    NULL, (void *)audio,
					    &audio_mp3_debug_fops);

	if (IS_ERR(audio->dentry))
		pr_debug("debugfs_create_file failed\n");
#endif
	pr_info("%s:mp3dec success mode[%d]session[%d]\n", __func__,
						audio->feedback,
						audio->ac->session);
	return rc;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_mp3_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_aio_release,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audio_aio_fsync,
};

static struct miscdevice audio_mp3_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_mp3",
	.fops = &audio_mp3_fops,
};

int __init audio_mp3_init(void)
{
	int ret = misc_register(&audio_mp3_misc);

	if (ret == 0)
		device_init_wakeup(audio_mp3_misc.this_device, true);
	audio_mp3_ws_mgr.ref_cnt = 0;
	mutex_init(&audio_mp3_ws_mgr.ws_lock);

	return ret;
}

void audio_mp3_exit(void)
{
	mutex_destroy(&audio_mp3_ws_mgr.ws_lock);
	misc_deregister(&audio_mp3_misc);
}

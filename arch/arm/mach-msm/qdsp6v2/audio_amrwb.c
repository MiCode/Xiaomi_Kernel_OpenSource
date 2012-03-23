/* amrwb audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

static void q6_audio_amrwb_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload, void *priv)
{
	struct q6audio_aio *audio = (struct q6audio_aio *)priv;

	pr_debug("%s:opcde = %d token = 0x%x\n", __func__, opcode, token);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE:
	case ASM_DATA_EVENT_READ_DONE:
	case ASM_DATA_CMDRSP_EOS:
		audio_aio_cb(opcode, token, payload, audio);
		break;
	default:
		pr_debug("%s:Unhandled event = 0x%8x\n", __func__, opcode);
		break;
	}
}

#ifdef CONFIG_DEBUG_FS
static const struct file_operations audio_amrwb_debug_fops = {
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
		pr_debug("%s: AUDIO_START sessionid[%d]enable[%d]\n", __func__,
						audio->ac->session,
						audio->enabled);
		if (audio->stopped == 1)
			audio->stopped = 0;
		break;
	}
	default:
		pr_debug("%s[%p]: Calling utils ioctl\n", __func__, audio);
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
	char name[sizeof "msm_amrwb_" + 5];
#endif
	audio = kzalloc(sizeof(struct q6audio_aio), GFP_KERNEL);

	if (audio == NULL) {
		pr_err("Could not allocate memory for aac decode driver\n");
		return -ENOMEM;
	}
	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN;

	audio->ac = q6asm_audio_client_alloc((app_cb) q6_audio_amrwb_cb,
					     (void *)audio);

	if (!audio->ac) {
		pr_err("Could not allocate memory for audio client\n");
		kfree(audio);
		return -ENOMEM;
	}

	/* open in T/NT mode */
	if ((file->f_mode & FMODE_WRITE) && (file->f_mode & FMODE_READ)) {
		rc = q6asm_open_read_write(audio->ac, FORMAT_LINEAR_PCM,
					   FORMAT_AMRWB);
		if (rc < 0) {
			pr_err("NT mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = NON_TUNNEL_MODE;
		audio->buf_cfg.frames_per_buf = 0x01;
		audio->buf_cfg.meta_info_enable = 0x01;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
		rc = q6asm_open_write(audio->ac, FORMAT_AMRWB);
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
	rc = audio_aio_open(audio, file);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: audio_aio_open failed\n", __func__);
		goto fail;
	}

#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof name, "msm_amrwb_%04x", audio->ac->session);
	audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
					    NULL, (void *)audio,
					    &audio_amrwb_debug_fops);

	if (IS_ERR(audio->dentry))
		pr_debug("debugfs_create_file failed\n");
#endif
	pr_info("%s: AMRWB dec success mode[%d]session[%d]\n", __func__,
						audio->feedback,
						audio->ac->session);
	return 0;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_amrwb_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_aio_release,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audio_aio_fsync,
};

struct miscdevice audio_amrwb_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_amrwb",
	.fops = &audio_amrwb_fops,
};

static int __init audio_amrwb_init(void)
{
	return misc_register(&audio_amrwb_misc);
}

device_initcall(audio_amrwb_init);

/* arch/arm/mach-msm/qdsp5/audio_fm.c
 *
 * pcm audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/msm_audio.h>
#include <mach/debug_mm.h>

#include "audmgr.h"

struct audio {
	struct mutex lock;
	int opened;
	int enabled;
	int running;
	struct audmgr audmgr;
	uint16_t volume;
};

static struct audio fm_audio;

/* must be called with audio->lock held */
static int audio_enable(struct audio *audio)
{
	struct audmgr_config cfg;
	int rc = 0;

	MM_DBG("\n"); /* Macro prints the file name and function */

	if (audio->enabled)
		return 0;

	cfg.tx_rate = RPC_AUD_DEF_SAMPLE_RATE_48000;
	cfg.rx_rate = RPC_AUD_DEF_SAMPLE_RATE_48000;
	cfg.def_method = RPC_AUD_DEF_METHOD_HOST_PCM;
	cfg.codec = RPC_AUD_DEF_CODEC_PCM;
	cfg.snd_method = RPC_SND_METHOD_VOICE;

	rc = audmgr_enable(&audio->audmgr, &cfg);
	if (rc < 0)
		return rc;

	audio->enabled = 1;
	return rc;
}

/* must be called with audio->lock held */
static int audio_disable(struct audio *audio)
{
	MM_DBG("\n"); /* Macro prints the file name and function */
	if (audio->enabled) {
		audio->enabled = 0;
		audmgr_disable(&audio->audmgr);
	}
	return 0;
}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = -EINVAL;

	MM_DBG("cmd %d", cmd);

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START:
		MM_DBG("AUDIO_START\n");
		rc = audio_enable(audio);
		break;
	case AUDIO_STOP:
		MM_DBG("AUDIO_STOP\n");
		rc = audio_disable(audio);
		audio->running = 0;
		audio->enabled = 0;
		break;

	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

static int audio_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	MM_DBG("audio instance 0x%08x freeing\n", (int)audio);
	mutex_lock(&audio->lock);
	audio_disable(audio);
	audio->running = 0;
	audio->enabled = 0;
	audio->opened = 0;
	mutex_unlock(&audio->lock);
	return 0;
}

static int audio_open(struct inode *inode, struct file *file)
{
	struct audio *audio = &fm_audio;
	int rc = 0;

	MM_DBG("\n"); /* Macro prints the file name and function */
	mutex_lock(&audio->lock);

	if (audio->opened) {
		MM_ERR("busy\n");
		rc = -EBUSY;
		goto done;
	}

	rc = audmgr_open(&audio->audmgr);

	if (rc) {
		MM_ERR("%s: failed to register listnet\n", __func__);
		goto done;
	}

	file->private_data = audio;
	audio->opened = 1;

done:
	mutex_unlock(&audio->lock);
	return rc;
}

static const struct file_operations audio_fm_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_open,
	.release	= audio_release,
	.unlocked_ioctl	= audio_ioctl,
};

struct miscdevice audio_fm_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_fm",
	.fops	= &audio_fm_fops,
};

static int __init audio_init(void)
{
	struct audio *audio = &fm_audio;

	mutex_init(&audio->lock);
	return misc_register(&audio_fm_misc);
}

device_initcall(audio_init);

MODULE_DESCRIPTION("MSM FM driver");
MODULE_LICENSE("GPL v2");

/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * Based on the mp3 native driver in arch/arm/mach-msm/qdsp5v2/audio_mp3.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/android_pmem.h>
#include <linux/msm_audio.h>
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/msm_adsp.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <sound/q6afe.h>

#define SESSION_ID_FM  (MAX_SESSIONS + 1)
#define FM_ENABLE	0x1
#define FM_DISABLE	0x0
#define FM_COPP		0x7

struct audio {
	struct mutex lock;

	int opened;
	int enabled;
	int running;

	uint16_t fm_source;
	uint16_t fm_src_copp_id;
	uint16_t fm_dest;
	uint16_t fm_dst_copp_id;
	uint16_t dec_id;
	uint32_t device_events;
	uint16_t volume;
};


static struct audio fm_audio;
static int fm_audio_enable(struct audio *audio)
{
	if (audio->enabled)
		return 0;

	pr_info("%s: fm dest= %08x fm_source = %08x\n", __func__,
		audio->fm_dst_copp_id, audio->fm_src_copp_id);

	/* do afe loopback here */

	if (audio->fm_dest && audio->fm_source) {
		if (afe_loopback(FM_ENABLE, audio->fm_dst_copp_id,
					audio->fm_src_copp_id) < 0) {
			pr_err("%s: afe_loopback failed\n", __func__);
		}

		audio->running = 1;
	}

	audio->enabled = 1;
	return 0;
}

static void fm_audio_listner(u32 evt_id, union auddev_evt_data *evt_payload,
			void *private_data)
{
	struct audio *audio = (struct audio *) private_data;
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		pr_info("%s :AUDDEV_EVT_DEV_RDY\n", __func__);
		if (evt_payload->routing_id == FM_COPP) {
			audio->fm_source = 1;
			audio->fm_src_copp_id = FM_COPP;
		} else {
			audio->fm_dest = 1;
			audio->fm_dst_copp_id = evt_payload->routing_id;
		}

		if (audio->enabled &&
			audio->fm_dest &&
			audio->fm_source) {

			afe_loopback_gain(audio->fm_src_copp_id,
						audio->volume);
			afe_loopback(FM_ENABLE, audio->fm_dst_copp_id,
						audio->fm_src_copp_id);
			audio->running = 1;
		}
		break;
	case AUDDEV_EVT_DEV_RLS:
		pr_info("%s: AUDDEV_EVT_DEV_RLS\n", __func__);
		if (evt_payload->routing_id == audio->fm_src_copp_id)
			audio->fm_source = 0;
		else
			audio->fm_dest = 0;
		if (audio->running
			&& (!audio->fm_dest || !audio->fm_source)) {
			afe_loopback(FM_DISABLE, audio->fm_dst_copp_id,
						audio->fm_src_copp_id);
			audio->running = 0;
		} else {
			pr_err("%s: device switch happened\n", __func__);
		}
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		pr_debug("%s: AUDDEV_EVT_STREAM_VOL_CHG\n", __func__);
		if (audio->fm_source) {
			audio->volume = evt_payload->session_vol;
			afe_loopback_gain(audio->fm_src_copp_id,
					audio->volume);
		}
		break;

	default:
		pr_err("%s: ERROR:wrong event %08x\n", __func__, evt_id);
		break;
	}
}

static int fm_audio_disable(struct audio *audio)
{

	/* break the AFE loopback here */
	afe_loopback(FM_DISABLE, audio->fm_dst_copp_id, audio->fm_src_copp_id);
	return 0;
}

static long fm_audio_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = -EINVAL;


	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START:
		pr_info("%s: AUDIO_START\n", __func__);
		rc = fm_audio_enable(audio);
		break;
	case AUDIO_STOP:
		pr_info("%s: AUDIO_STOP\n", __func__);
		rc = fm_audio_disable(audio);
		audio->running = 0;
		audio->enabled = 0;
		break;
	case AUDIO_GET_SESSION_ID:
		if (copy_to_user((void *) arg, &audio->dec_id,
					sizeof(unsigned short)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	default:
		rc = -EINVAL;
		pr_err("%s: Un supported IOCTL\n", __func__);
	}
	mutex_unlock(&audio->lock);
	return rc;
}

static int fm_audio_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	pr_debug("audio instance 0x%08x freeing\n", (int)audio);
	mutex_lock(&audio->lock);
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, audio->dec_id);
	fm_audio_disable(audio);
	audio->running = 0;
	audio->enabled = 0;
	audio->opened = 0;
	mutex_unlock(&audio->lock);
	return 0;
}

static int fm_audio_open(struct inode *inode, struct file *file)
{
	struct audio *audio = &fm_audio;
	int rc = 0;


	if (audio->opened)
		return -EPERM;

	/* Allocate the decoder */
	audio->dec_id = SESSION_ID_FM;

	audio->running = 0;
	audio->fm_source = 0;
	audio->fm_dest = 0;

	audio->device_events = AUDDEV_EVT_DEV_RDY
				|AUDDEV_EVT_DEV_RLS|
				AUDDEV_EVT_STREAM_VOL_CHG;

	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_DEC,
					audio->dec_id,
					fm_audio_listner,
					(void *)audio);

	if (rc) {
		pr_err("%s: failed to register listnet\n", __func__);
		goto event_err;
	}

	audio->opened = 1;
	file->private_data = audio;

event_err:
	return rc;
}

static const struct file_operations audio_fm_fops = {
	.owner		= THIS_MODULE,
	.open		= fm_audio_open,
	.release	= fm_audio_release,
	.unlocked_ioctl	= fm_audio_ioctl,
};

struct miscdevice audio_fm_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_fm",
	.fops	= &audio_fm_fops,
};

static int __init fm_audio_init(void)
{
	struct audio *audio = &fm_audio;

	mutex_init(&audio->lock);
	return misc_register(&audio_fm_misc);
}

device_initcall(fm_audio_init);

MODULE_DESCRIPTION("MSM FM driver");
MODULE_LICENSE("GPL v2");

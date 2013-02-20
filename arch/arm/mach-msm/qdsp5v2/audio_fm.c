/* Copyright (c) 2009-2011, The Linux Foundation. All rights reserved.
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
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/android_pmem.h>
#include <linux/msm_audio.h>
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/msm_adsp.h>
#include <mach/debug_mm.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/qdsp5v2/afe.h>
#include <mach/qdsp5v2/acdb_commands.h>
#include <mach/qdsp5v2/audio_acdbi.h>
#include <mach/qdsp5v2/audio_acdb_def.h>

#define SESSION_ID_FM 6
#define FM_ENABLE	0xFFFF
#define FM_DISABLE	0x0
#define FM_COPP		0x2
/* Macro specifies maximum FM routing
	possible */
#define FM_MAX_RX_ROUTE	0x2

struct fm_rx_calib_gain {
	uint16_t device_id;
	struct auddev_evt_devinfo dev_details;
	struct  acdb_calib_gain_rx  calib_rx;
};

struct audio {
	struct mutex lock;

	int opened;
	int enabled;
	int running;

	uint16_t dec_id;
	uint16_t source;
	uint16_t fm_source;
	uint16_t fm_mask;
	uint32_t device_events;
	uint16_t volume;
	struct fm_rx_calib_gain fm_calibration_rx[FM_MAX_RX_ROUTE];
};

static struct audio fm_audio;

/* must be called with audio->lock held */
static int audio_enable(struct audio *audio)
{
	int rc = 0;
	if (audio->enabled)
		return 0;

	MM_DBG("fm mask= %08x fm_source = %08x\n",
			audio->fm_mask, audio->fm_source);
	if (audio->fm_mask && audio->fm_source) {
		rc = afe_config_fm_codec(FM_ENABLE, audio->fm_mask);
		if (!rc)
			audio->running = 1;
		/* Routed to icodec rx path */
		if ((audio->fm_mask & AFE_HW_PATH_CODEC_RX) ==
				AFE_HW_PATH_CODEC_RX) {
			afe_config_fm_calibration_gain(
			audio->fm_calibration_rx[0].device_id,
			audio->fm_calibration_rx[0].calib_rx.audppcalgain);
		}
		/* Routed to aux codec rx path */
		if ((audio->fm_mask & AFE_HW_PATH_AUXPCM_RX) ==
				AFE_HW_PATH_AUXPCM_RX){
			afe_config_fm_calibration_gain(
			audio->fm_calibration_rx[1].device_id,
			audio->fm_calibration_rx[1].calib_rx.audppcalgain);
		}
	}

	audio->enabled = 1;
	return rc;
}

static void fm_listner(u32 evt_id, union auddev_evt_data *evt_payload,
			void *private_data)
{
	struct audio *audio = (struct audio *) private_data;
	struct auddev_evt_devinfo *devinfo =
			(struct auddev_evt_devinfo *)evt_payload;
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		MM_DBG(":AUDDEV_EVT_DEV_RDY\n");
		if (evt_payload->routing_id == FM_COPP)
			audio->fm_source = 1;
		else
			audio->source = (0x1 << evt_payload->routing_id);

		if (audio->source & 0x1)
			audio->fm_mask = 0x1;
		else if (audio->source & 0x2)
			audio->fm_mask = 0x3;
		else
			audio->fm_mask = 0x0;

		if (!audio->enabled
			|| !audio->fm_mask
			|| !audio->fm_source)
			break;
		else {
			afe_config_fm_codec(FM_ENABLE, audio->fm_mask);
			audio->running = 1;
		}
		break;
	case AUDDEV_EVT_DEV_RLS:
		MM_DBG(":AUDDEV_EVT_DEV_RLS\n");
		if (evt_payload->routing_id == FM_COPP)
			audio->fm_source = 0;
		else
			audio->source &= ~(0x1 << evt_payload->routing_id);

		if (audio->source & 0x1)
			audio->fm_mask = 0x1;
		else if (audio->source & 0x2)
			audio->fm_mask = 0x3;
		else
			audio->fm_mask = 0x0;

		if (audio->running
			&& (!audio->fm_mask || !audio->fm_source)) {
			afe_config_fm_codec(FM_DISABLE, audio->fm_mask);
			audio->running = 0;
		}
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		MM_DBG(":AUDDEV_EVT_STREAM_VOL_CHG, stream vol \n");
		audio->volume = evt_payload->session_vol;
		afe_config_fm_volume(audio->volume);
		break;
	case AUDDEV_EVT_DEVICE_INFO:{
		struct acdb_get_block get_block;
		int rc = 0;
		MM_DBG(":AUDDEV_EVT_DEVICE_INFO\n");
		MM_DBG("sample_rate = %d\n", devinfo->sample_rate);
		MM_DBG("acdb_id = %d\n", devinfo->acdb_id);
		/* Applucable only for icodec rx and aux codec rx path
			and fm stream routed to it */
		if (((devinfo->dev_id == 0x00) || (devinfo->dev_id == 0x01)) &&
			(devinfo->sessions && (1 << audio->dec_id))) {
			/* Query ACDB driver for calib gain, only if difference
				in device */
			if ((audio->fm_calibration_rx[devinfo->dev_id].
				dev_details.acdb_id != devinfo->acdb_id) ||
				(audio->fm_calibration_rx[devinfo->dev_id].
				dev_details.sample_rate !=
					devinfo->sample_rate)) {
				audio->fm_calibration_rx[devinfo->dev_id].
					dev_details.dev_id = devinfo->dev_id;
				audio->fm_calibration_rx[devinfo->dev_id].
					dev_details.sample_rate =
						devinfo->sample_rate;
				audio->fm_calibration_rx[devinfo->dev_id].
					dev_details.dev_type =
						devinfo->dev_type;
				audio->fm_calibration_rx[devinfo->dev_id].
					dev_details.sessions =
						devinfo->sessions;
				/* Query ACDB driver for calibration gain */
				get_block.acdb_id = devinfo->acdb_id;
				get_block.sample_rate_id = devinfo->sample_rate;
				get_block.interface_id =
					IID_AUDIO_CALIBRATION_GAIN_RX;
				get_block.algorithm_block_id =
					ABID_AUDIO_CALIBRATION_GAIN_RX;
				get_block.total_bytes =
					sizeof(struct  acdb_calib_gain_rx);
				get_block.buf_ptr = (u32 *)
				&audio->fm_calibration_rx[devinfo->dev_id].
				calib_rx;

				rc = acdb_get_calibration_data(&get_block);
				if (rc < 0) {
					MM_ERR("Unable to get calibration"\
						"gain\n");
					/* Set to unity incase of error */
					audio->\
					fm_calibration_rx[devinfo->dev_id].
					calib_rx.audppcalgain = 0x2000;
				} else
					MM_DBG("calibration gain = 0x%8x\n",
						*(get_block.buf_ptr));
			}
			if (audio->running) {
				afe_config_fm_calibration_gain(
				audio->fm_calibration_rx[devinfo->dev_id].
					device_id,
				audio->fm_calibration_rx[devinfo->dev_id].
					calib_rx.audppcalgain);
				}
			}
		break;
	}
	default:
		MM_DBG(":ERROR:wrong event\n");
		break;
	}
}
/* must be called with audio->lock held */
static int audio_disable(struct audio *audio)
{
	MM_DBG("\n"); /* Macro prints the file name and function */
	return afe_config_fm_codec(FM_DISABLE, audio->source);
}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = -EINVAL;

	MM_DBG("cmd = %d\n", cmd);

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
	case AUDIO_GET_SESSION_ID:
		if (copy_to_user((void *) arg, &audio->dec_id,
					sizeof(unsigned short)))
			rc = -EFAULT;
		else
			rc = 0;
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
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, audio->dec_id);
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


	if (audio->opened)
		return -EPERM;

	/* Allocate the decoder */
	audio->dec_id = SESSION_ID_FM;

	audio->running = 0;
	audio->fm_source = 0;
	audio->fm_mask = 0;

	/* Initialize the calibration gain structure */
	audio->fm_calibration_rx[0].device_id = AFE_HW_PATH_CODEC_RX;
	audio->fm_calibration_rx[1].device_id = AFE_HW_PATH_AUXPCM_RX;
	audio->fm_calibration_rx[0].calib_rx.audppcalgain = 0x2000;
	audio->fm_calibration_rx[1].calib_rx.audppcalgain = 0x2000;
	audio->fm_calibration_rx[0].dev_details.acdb_id = PSEUDO_ACDB_ID;
	audio->fm_calibration_rx[1].dev_details.acdb_id = PSEUDO_ACDB_ID;

	audio->device_events = AUDDEV_EVT_DEV_RDY
				|AUDDEV_EVT_DEV_RLS|
				AUDDEV_EVT_STREAM_VOL_CHG|
				AUDDEV_EVT_DEVICE_INFO;

	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_DEC,
					audio->dec_id,
					fm_listner,
					(void *)audio);

	if (rc) {
		MM_ERR("%s: failed to register listnet\n", __func__);
		goto event_err;
	}

	audio->opened = 1;
	file->private_data = audio;

event_err:
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

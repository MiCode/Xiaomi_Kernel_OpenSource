/* arch/arm/mach-msm/qdsp6/audiov2/audio_ctrl.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 HTC Corporation
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/msm_audio.h>

#include <mach/msm_qdsp6_audiov2.h>

#define BUFSZ (0)

static DEFINE_MUTEX(voice_lock);
static int voice_started;

static struct audio_client *voc_clnt;

static int q6_voice_start(void)
{
	int rc = 0;

	mutex_lock(&voice_lock);

	if (voice_started) {
		pr_err("voice: busy\n");
		rc = -EBUSY;
		goto done;
	}

	voc_clnt = q6voice_open();
	if (!voc_clnt) {
		pr_err("voice: open voice failed.\n");
		rc = -ENOMEM;
		goto done;
	}

	voice_started = 1;
done:
	mutex_unlock(&voice_lock);
	return rc;
}

static int q6_voice_stop(void)
{
	mutex_lock(&voice_lock);
	if (voice_started) {
		q6voice_close(voc_clnt);
		voice_started = 0;
	}
	mutex_unlock(&voice_lock);
	return 0;
}

static int q6_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int q6_ioctl(struct inode *inode, struct file *file,
		    unsigned int cmd, unsigned long arg)
{
	int rc;
	uint32_t n;
	uint32_t id[2];

	switch (cmd) {
	case AUDIO_SWITCH_DEVICE:
		rc = copy_from_user(&n, (void *)arg, sizeof(n));
		if (!rc)
			rc = q6audio_do_routing(n);
		break;
	case AUDIO_SET_VOLUME:
		rc = copy_from_user(&n, (void *)arg, sizeof(n));
		if (!rc)
			rc = q6audio_set_rx_volume(n);
		break;
	case AUDIO_SET_MUTE:
		rc = copy_from_user(&n, (void *)arg, sizeof(n));
		if (!rc)
			rc = q6audio_set_tx_mute(n);
		break;
	case AUDIO_UPDATE_ACDB:
		rc = copy_from_user(&id, (void *)arg, sizeof(id));
		if (!rc)
			rc = q6audio_update_acdb(id[0], id[1]);
		break;
	case AUDIO_START_VOICE:
		rc = q6_voice_start();
		break;
	case AUDIO_STOP_VOICE:
		rc = q6_voice_stop();
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}


static int q6_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations q6_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= q6_open,
	.ioctl		= q6_ioctl,
	.release	= q6_release,
};

struct miscdevice q6_control_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_audio_ctl",
	.fops	= &q6_dev_fops,
};


static int __init q6_audio_ctl_init(void)
{
	return misc_register(&q6_control_device);
}

device_initcall(q6_audio_ctl_init);

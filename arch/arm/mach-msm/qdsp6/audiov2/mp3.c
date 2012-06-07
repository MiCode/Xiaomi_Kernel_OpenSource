/* arch/arm/mach-msm/qdsp6/audiov2/mp3.c
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
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>

#include <linux/msm_audio.h>

#include <mach/msm_qdsp6_audiov2.h>
#include "dal_audio.h"
#include "dal_audio_format.h"

#define BUFSZ (8192)
#define DMASZ (BUFSZ * 2)

struct mp3 {
	struct mutex lock;
	struct audio_client *ac;
	struct msm_audio_config cfg;
};

static long mp3_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mp3 *mp3 = file->private_data;
	struct adsp_open_command rpc;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		memset(&stats, 0, sizeof(stats));
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&mp3->lock);
	switch (cmd) {
	case AUDIO_SET_VOLUME:
		break;
	case AUDIO_START:
		memset(&rpc, 0, sizeof(rpc));
		rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_WRITE;
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_PLAYBACK;
		rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
		rpc.format_block.standard.format = ADSP_AUDIO_FORMAT_MP3;
		rpc.format_block.standard.channels = mp3->cfg.channel_count;
		rpc.format_block.standard.bits_per_sample = 16;
		rpc.format_block.standard.sampling_rate = mp3->cfg.sample_rate;
		rpc.format_block.standard.is_signed = 1;
		rpc.format_block.standard.is_interleaved = 0;
		rpc.buf_max_size = BUFSZ;
		q6audio_start(mp3->ac, (void *) &rpc, sizeof(rpc));
		break;
	case AUDIO_STOP:
		break;
	case AUDIO_FLUSH:
		break;
	case AUDIO_SET_CONFIG:
		if (copy_from_user(&mp3->cfg, (void *) arg,
			sizeof(struct msm_audio_config))) {
			rc = -EFAULT;
			break;
		}
		if (mp3->cfg.channel_count < 1 || mp3->cfg.channel_count > 2) {
			rc = -EINVAL;
			break;
		}
		break;
	case AUDIO_GET_CONFIG:
		if (copy_to_user((void *) arg, &mp3->cfg,
			sizeof(struct msm_audio_config))) {
			rc = -EFAULT;
		}
		break;
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&mp3->lock);
	return rc;
}

static int mp3_open(struct inode *inode, struct file *file)
{

	struct mp3 *mp3;
	mp3 = kzalloc(sizeof(struct mp3), GFP_KERNEL);

	if (!mp3)
		return -ENOMEM;

	mutex_init(&mp3->lock);
	file->private_data = mp3;
	mp3->ac = q6audio_open(AUDIO_FLAG_WRITE, BUFSZ);
	if (!mp3->ac) {
		kfree(mp3);
		return -ENOMEM;
	}
	mp3->cfg.channel_count = 2;
	mp3->cfg.buffer_count = 2;
	mp3->cfg.buffer_size = BUFSZ;
	mp3->cfg.unused[0] = 0;
	mp3->cfg.unused[1] = 0;
	mp3->cfg.unused[2] = 0;
	mp3->cfg.sample_rate = 48000;

	return 0;
}

static ssize_t mp3_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *pos)
{
	struct mp3 *mp3 = file->private_data;
	struct audio_client *ac;
	struct audio_buffer *ab;
	const char __user *start = buf;
	int xfer;

	if (!mp3->ac)
		mp3_ioctl(file, AUDIO_START, 0);

	ac = mp3->ac;
	if (!ac)
		return -ENODEV;

	while (count > 0) {
		ab = ac->buf + ac->cpu_buf;

		if (ab->used)
			wait_event(ac->wait, (ab->used == 0));

		xfer = count;
		if (xfer > ab->size)
			xfer = ab->size;

		if (copy_from_user(ab->data, buf, xfer))
			return -EFAULT;

		buf += xfer;
		count -= xfer;

		ab->used = xfer;
		q6audio_write(ac, ab);
		ac->cpu_buf ^= 1;
	}

	return buf - start;
}

static int mp3_fsync(struct file *f, int datasync)
{
	struct mp3 *mp3 = f->private_data;
	if (mp3->ac)
		return q6audio_async(mp3->ac);
	return -ENODEV;
}

static int mp3_release(struct inode *inode, struct file *file)
{
	struct mp3 *mp3 = file->private_data;
	if (mp3->ac)
		q6audio_close(mp3->ac);
	kfree(mp3);
	return 0;
}

static const struct file_operations mp3_fops = {
	.owner		= THIS_MODULE,
	.open		= mp3_open,
	.write		= mp3_write,
	.fsync		= mp3_fsync,
	.release	= mp3_release,
	.unlocked_ioctl	= mp3_ioctl,
};

struct miscdevice mp3_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_mp3",
	.fops	= &mp3_fops,
};

static int __init mp3_init(void)
{
	return misc_register(&mp3_misc);
}

device_initcall(mp3_init);

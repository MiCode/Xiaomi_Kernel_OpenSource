/* arch/arm/mach-msm/qdsp6/audiov2/pcm_out.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Author: Brian Swetland <swetland@google.com>
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

struct pcm {
	struct mutex lock;
	struct audio_client *ac;
	struct msm_audio_config cfg;

};

static long pcm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct pcm *pcm = file->private_data;
	struct adsp_open_command rpc;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		memset(&stats, 0, sizeof(stats));
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&pcm->lock);
	switch (cmd) {
	case AUDIO_START:
		memset(&rpc, 0, sizeof(rpc));
		rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_WRITE;
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_PLAYBACK;
		rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
		rpc.format_block.standard.format = ADSP_AUDIO_FORMAT_PCM;
		rpc.format_block.standard.channels = pcm->cfg.channel_count;
		rpc.format_block.standard.bits_per_sample = 16;
		rpc.format_block.standard.sampling_rate = pcm->cfg.sample_rate;
		rpc.format_block.standard.is_signed = 1;
		rpc.format_block.standard.is_interleaved = 1;
		rpc.buf_max_size = BUFSZ;
		q6audio_start(pcm->ac, (void *) &rpc, sizeof(rpc));
		break;
	case AUDIO_STOP:
		break;
	case AUDIO_FLUSH:
		break;
	case AUDIO_SET_CONFIG:
		if (copy_from_user(&pcm->cfg, (void *) arg,
				 sizeof(struct msm_audio_config))) {
			rc = -EFAULT;
			break;
		}
		if (pcm->cfg.channel_count < 1 || pcm->cfg.channel_count > 2) {
			rc = -EINVAL;
			break;
		}

		break;
	case AUDIO_GET_CONFIG:
		if (copy_to_user((void *) arg, &pcm->cfg,
				 sizeof(struct msm_audio_config))) {
			rc = -EFAULT;
		}
		break;

	default:
		rc = -EINVAL;
	}

	mutex_unlock(&pcm->lock);
	return rc;
}

static int pcm_open(struct inode *inode, struct file *file)
{
	struct pcm *pcm;
	pcm = kzalloc(sizeof(struct pcm), GFP_KERNEL);

	if (!pcm)
		return -ENOMEM;

	mutex_init(&pcm->lock);
	file->private_data = pcm;
	pcm->ac = q6audio_open(AUDIO_FLAG_WRITE, BUFSZ);
	if (!pcm->ac) {
		kfree(pcm);
		return -ENOMEM;
	}
	pcm->cfg.channel_count = 2;
	pcm->cfg.buffer_count = 2;
	pcm->cfg.buffer_size = BUFSZ;
	pcm->cfg.unused[0] = 0;
	pcm->cfg.unused[1] = 0;
	pcm->cfg.unused[2] = 0;
	pcm->cfg.sample_rate = 48000;

	return 0;
}

static ssize_t pcm_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *pos)
{
	struct pcm *pcm = file->private_data;
	struct audio_client *ac;
	struct audio_buffer *ab;
	const char __user *start = buf;
	int xfer;

	ac = pcm->ac;
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

		ab->used = 1;
		ab->actual_size = xfer;
		q6audio_write(ac, ab);
		ac->cpu_buf ^= 1;
	}

	return buf - start;
}

static int pcm_release(struct inode *inode, struct file *file)
{
	struct pcm *pcm = file->private_data;
	if (pcm->ac)
		q6audio_close(pcm->ac);
	kfree(pcm);
	return 0;
}

static const struct file_operations pcm_fops = {
	.owner		= THIS_MODULE,
	.open		= pcm_open,
	.write		= pcm_write,
	.release	= pcm_release,
	.unlocked_ioctl	= pcm_ioctl,
};

struct miscdevice pcm_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_out",
	.fops	= &pcm_fops,
};

static int __init pcm_init(void)
{
	return misc_register(&pcm_misc);
}

device_initcall(pcm_init);

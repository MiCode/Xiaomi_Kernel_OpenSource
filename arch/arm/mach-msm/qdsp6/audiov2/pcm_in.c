/* arch/arm/mach-msm/qdsp6/audiov2/pcm_in.c
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

#define BUFSZ (4096)
#define DMASZ (BUFSZ * 2)


struct pcm {
	struct mutex lock;
	struct msm_audio_config cfg;
	struct audio_client *audio_client;
};

static long q6_in_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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
		tx_clk_freq = pcm->cfg.sample_rate;

		memset(&rpc, 0, sizeof(rpc));

		rpc.format_block.standard.format = ADSP_AUDIO_FORMAT_PCM;
		rpc.format_block.standard.channels = pcm->cfg.channel_count;
		rpc.format_block.standard.bits_per_sample = 16;
		rpc.format_block.standard.sampling_rate = pcm->cfg.sample_rate;
		rpc.format_block.standard.is_signed = 1;
		rpc.format_block.standard.is_interleaved = 1;

		rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_READ;
		rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_RECORD;
		rpc.buf_max_size = BUFSZ;
		q6audio_start(pcm->audio_client, &rpc, sizeof(rpc));
		break;
	case AUDIO_STOP:
		break;
	case AUDIO_FLUSH:
		break;
	case AUDIO_SET_VOLUME:
		break;
	case AUDIO_SET_CONFIG:
		if (copy_from_user(&pcm->cfg, (void *) arg,
				 sizeof(struct msm_audio_config))) {
			rc = -EFAULT;
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

static int q6_in_open(struct inode *inode, struct file *file)
{

	struct pcm *pcm;
	pcm = kmalloc(sizeof(struct pcm), GFP_KERNEL);
	if (pcm == NULL) {
		pr_err("Could not allocate memory for pcm driver\n");
		return -ENOMEM;
	}
	mutex_init(&pcm->lock);
	file->private_data = pcm;
	pcm->audio_client = q6audio_open(AUDIO_FLAG_READ, BUFSZ);
	if (!pcm->audio_client) {
		kfree(pcm);
		return -ENOMEM;
	}
	pcm->cfg.channel_count = 1;
	pcm->cfg.buffer_count = 2;
	pcm->cfg.buffer_size = BUFSZ;
	pcm->cfg.unused[0] = 0;
	pcm->cfg.unused[1] = 0;
	pcm->cfg.unused[2] = 0;
	pcm->cfg.sample_rate = 8000;

	return 0;
}

static ssize_t q6_in_read(struct file *file, char __user *buf,
			  size_t count, loff_t *pos)
{
	struct audio_client *ac;
	struct audio_buffer *ab;
	const char __user *start = buf;
	struct pcm *pcm = file->private_data;
	int xfer;
	int res;

	mutex_lock(&pcm->lock);
	ac = pcm->audio_client;
	if (!ac) {
		res = -ENODEV;
		goto fail;
	}
	while (count > 0) {
		ab = ac->buf + ac->cpu_buf;

		if (ab->used)
			wait_event(ac->wait, (ab->used == 0));

		xfer = count;
		if (xfer > ab->size)
			xfer = ab->size;

		if (copy_to_user(buf, ab->data, xfer)) {
			res = -EFAULT;
			goto fail;
		}

		buf += xfer;
		count -= xfer;

		ab->used = 1;
		q6audio_read(ac, ab);
		ac->cpu_buf ^= 1;
	}
fail:
	res = buf - start;
	mutex_unlock(&pcm->lock);

	return res;
}

static int q6_in_release(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct pcm *pcm = file->private_data;

	mutex_lock(&pcm->lock);
	if (pcm->audio_client)
		rc = q6audio_close(pcm->audio_client);
	mutex_unlock(&pcm->lock);
	kfree(pcm);
	return rc;
}

static const struct file_operations q6_in_fops = {
	.owner		= THIS_MODULE,
	.open		= q6_in_open,
	.read		= q6_in_read,
	.release	= q6_in_release,
	.unlocked_ioctl	= q6_in_ioctl,
};

struct miscdevice q6_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_in",
	.fops	= &q6_in_fops,
};

static int __init q6_in_init(void)
{
	return misc_register(&q6_in_misc);
}

device_initcall(q6_in_init);

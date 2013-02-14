/*
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 HTC Corporation
 * Copyright (c) 2009, The Linux Foundation. All rights reserved.
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

#include <linux/msm_audio_qcp.h>
#include <mach/msm_qdsp6_audiov2.h>
#include "dal_audio.h"
#include "dal_audio_format.h"
#include <mach/debug_mm.h>


struct evrc {
	struct mutex lock;
	struct msm_audio_evrc_enc_config cfg;
	struct msm_audio_stream_config str_cfg;
	struct audio_client *audio_client;
};


static long q6_evrc_in_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct evrc *evrc = file->private_data;
	struct adsp_open_command rpc;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		memset(&stats, 0, sizeof(stats));
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&evrc->lock);
	switch (cmd) {
	case AUDIO_START:
		if (evrc->audio_client) {
			rc = -EBUSY;
			break;
		} else {
			evrc->audio_client = q6audio_open(AUDIO_FLAG_READ,
						evrc->str_cfg.buffer_size);

			if (!evrc->audio_client) {
				kfree(evrc);
				rc = -ENOMEM;
				break;
			}
		}

		tx_clk_freq = 8000;

		memset(&rpc, 0, sizeof(rpc));

		rpc.format_block.standard.format = ADSP_AUDIO_FORMAT_EVRC_FS;
		rpc.format_block.standard.channels = 1;
		rpc.format_block.standard.bits_per_sample = 16;
		rpc.format_block.standard.sampling_rate = 8000;
		rpc.format_block.standard.is_signed = 1;
		rpc.format_block.standard.is_interleaved = 0;

		rpc.hdr.opcode = ADSP_AUDIO_IOCTL_CMD_OPEN_READ;
		rpc.device = ADSP_AUDIO_DEVICE_ID_DEFAULT;
		rpc.stream_context = ADSP_AUDIO_DEVICE_CONTEXT_RECORD;
		rpc.buf_max_size = evrc->str_cfg.buffer_size;
		rpc.config.evrc.min_rate = evrc->cfg.min_bit_rate;
		rpc.config.evrc.max_rate = evrc->cfg.max_bit_rate;

		q6audio_start(evrc->audio_client, &rpc, sizeof(rpc));
		break;
	case AUDIO_STOP:
		break;
	case AUDIO_FLUSH:
		break;
	case AUDIO_SET_VOLUME:
		break;
	case AUDIO_GET_STREAM_CONFIG:
		if (copy_to_user((void *)arg, &evrc->str_cfg,
				sizeof(struct msm_audio_stream_config)))
			rc = -EFAULT;
		break;
	case AUDIO_SET_STREAM_CONFIG:
		if (copy_from_user(&evrc->str_cfg, (void *)arg,
			sizeof(struct msm_audio_stream_config))) {
			rc = -EFAULT;
			break;
		}

		if (evrc->str_cfg.buffer_size < 23) {
			pr_err("[%s:%s] Buffer size too small\n", __MM_FILE__,
					__func__);
			rc = -EINVAL;
			break;
		}

		if (evrc->str_cfg.buffer_count != 2)
			pr_info("[%s:%s] Buffer count set to 2\n", __MM_FILE__,
					__func__);
		break;
	case AUDIO_SET_EVRC_ENC_CONFIG:
		if (copy_from_user(&evrc->cfg, (void *) arg,
				 sizeof(struct msm_audio_evrc_enc_config)))
			rc = -EFAULT;

		if (evrc->cfg.min_bit_rate > 4 || evrc->cfg.min_bit_rate < 1) {
			pr_err("[%s:%s] invalid min bitrate\n", __MM_FILE__,
					__func__);
			rc = -EINVAL;
		}
		if (evrc->cfg.max_bit_rate > 4 || evrc->cfg.max_bit_rate < 1) {
			pr_err("[%s:%s] invalid max bitrate\n", __MM_FILE__,
					__func__);
			rc = -EINVAL;
		}
		break;
	case AUDIO_GET_EVRC_ENC_CONFIG:
		if (copy_to_user((void *) arg, &evrc->cfg,
				 sizeof(struct msm_audio_evrc_enc_config)))
			rc = -EFAULT;
		break;

	default:
		rc = -EINVAL;
	}

	mutex_unlock(&evrc->lock);
	return rc;
}

static int q6_evrc_in_open(struct inode *inode, struct file *file)
{
	struct evrc *evrc;
	evrc = kmalloc(sizeof(struct evrc), GFP_KERNEL);
	if (evrc == NULL) {
		pr_err("[%s:%s] Could not allocate memory for evrc driver\n",
				__MM_FILE__, __func__);
		return -ENOMEM;
	}

	mutex_init(&evrc->lock);
	file->private_data = evrc;
	evrc->audio_client = NULL;
	evrc->str_cfg.buffer_size = 23;
	evrc->str_cfg.buffer_count = 2;
	evrc->cfg.cdma_rate = CDMA_RATE_FULL;
	evrc->cfg.min_bit_rate = 1;
	evrc->cfg.max_bit_rate = 4;

	return 0;
}

static ssize_t q6_evrc_in_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct audio_client *ac;
	struct audio_buffer *ab;
	const char __user *start = buf;
	struct evrc *evrc = file->private_data;
	int xfer = 0;
	int res;

	mutex_lock(&evrc->lock);
	ac = evrc->audio_client;
	if (!ac) {
		res = -ENODEV;
		goto fail;
	}
	while (count > xfer) {
		ab = ac->buf + ac->cpu_buf;

		if (ab->used)
			wait_event(ac->wait, (ab->used == 0));

		xfer = ab->actual_size;

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

	res = buf - start;

fail:
	mutex_unlock(&evrc->lock);

	return res;
}

static int q6_evrc_in_release(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct evrc *evrc = file->private_data;

	mutex_lock(&evrc->lock);
	if (evrc->audio_client)
		rc = q6audio_close(evrc->audio_client);
	mutex_unlock(&evrc->lock);
	kfree(evrc);
	return rc;
}

static const struct file_operations q6_evrc_in_fops = {
	.owner		= THIS_MODULE,
	.open		= q6_evrc_in_open,
	.read		= q6_evrc_in_read,
	.release	= q6_evrc_in_release,
	.unlocked_ioctl	= q6_evrc_in_ioctl,
};

struct miscdevice q6_evrc_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_evrc_in",
	.fops	= &q6_evrc_in_fops,
};

static int __init q6_evrc_in_init(void)
{
	return misc_register(&q6_evrc_in_misc);
}

device_initcall(q6_evrc_in_init);

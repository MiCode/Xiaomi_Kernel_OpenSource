/* arch/arm/mach-msm/qdsp6/pcm_in.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 HTC Corporation
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

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>

#include <linux/msm_audio.h>

#include <mach/msm_qdsp6_audio.h>
#include <mach/debug_mm.h>

struct pcm {
	struct audio_client *ac;
	uint32_t sample_rate;
	uint32_t channel_count;
	uint32_t buffer_size;
	uint32_t rec_mode;
};

#define BUFSZ (256)

void audio_client_dump(struct audio_client *ac);

static long q6_in_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct pcm *pcm = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_SET_VOLUME:
		pr_debug("[%s:%s] SET_VOLUME\n", __MM_FILE__, __func__);
		break;
	case AUDIO_GET_STATS: {
		struct msm_audio_stats stats;
		pr_debug("[%s:%s] GET_STATS\n", __MM_FILE__, __func__);
		memset(&stats, 0, sizeof(stats));
		if (copy_to_user((void*) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}
	case AUDIO_START: {
		uint32_t acdb_id;
		pr_debug("[%s:%s] AUDIO_START\n", __MM_FILE__, __func__);
		rc = 0;

		if (arg == 0) {
			acdb_id = 0;
		} else if (copy_from_user(&acdb_id, (void*) arg, sizeof(acdb_id))) {
			rc = -EFAULT;
			break;
		}

		if (pcm->ac) {
			pr_err("[%s:%s] active session already existing\n",
				__MM_FILE__, __func__);
			rc = -EBUSY;
		} else {
			pcm->ac = q6audio_open_pcm(pcm->buffer_size,
					pcm->sample_rate, pcm->channel_count,
					pcm->rec_mode, acdb_id);
			if (!pcm->ac) {
				pr_err("[%s:%s] pcm open session failed\n",
					__MM_FILE__, __func__);
				rc = -ENOMEM;
			}
		}
		break;
	}
	case AUDIO_STOP:
		pr_debug("[%s:%s] AUDIO_STOP\n", __MM_FILE__, __func__);
		break;
	case AUDIO_FLUSH:
		break;
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config config;
		if (copy_from_user(&config, (void*) arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		pr_debug("[%s:%s] SET_CONFIG: samplerate = %d, channels = %d\n",
			__MM_FILE__, __func__, config.sample_rate,
			config.channel_count);
		if (!config.channel_count || config.channel_count > 2) {
			rc = -EINVAL;
			pr_err("[%s:%s] invalid channelcount %d\n",
			__MM_FILE__, __func__, config.channel_count);
			break;
		}
		if (config.sample_rate < 8000 || config.sample_rate > 48000) {
			rc = -EINVAL;
			pr_err("[%s:%s] invalid samplerate %d\n", __MM_FILE__,
				__func__, config.sample_rate);
			break;
		}
		if (config.buffer_size < 128 || config.buffer_size > 8192) {
			rc = -EINVAL;
			pr_err("[%s:%s] invalid buffsize %d\n", __MM_FILE__,
				__func__, config.buffer_size);
			break;
		}

		pcm->sample_rate = config.sample_rate;
		pcm->channel_count = config.channel_count;
		pcm->buffer_size = config.buffer_size;
		break;
	}
	case AUDIO_SET_INCALL: {
		struct msm_voicerec_mode voicerec_mode;
		pr_debug("[%s:%s] SET_INCALL\n", __MM_FILE__, __func__);
		if (copy_from_user(&voicerec_mode, (void *)arg,
			sizeof(struct msm_voicerec_mode)))
			return -EFAULT;
		if (voicerec_mode.rec_mode != AUDIO_FLAG_READ &&
			voicerec_mode.rec_mode != AUDIO_FLAG_INCALL_MIXED) {
			pcm->rec_mode = AUDIO_FLAG_READ;
			pr_err("[%s:%s] invalid rec_mode\n", __MM_FILE__,
					__func__);
			rc = -EINVAL;
		} else
			pcm->rec_mode = voicerec_mode.rec_mode;
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config config;
		config.buffer_size = pcm->buffer_size;
		config.buffer_count = 2;
		config.sample_rate = pcm->sample_rate;
		config.channel_count = pcm->channel_count;
		config.unused[0] = 0;
		config.unused[1] = 0;
		config.unused[2] = 0;
		if (copy_to_user((void*) arg, &config, sizeof(config))) {
			rc = -EFAULT;
		}
		pr_debug("[%s:%s] GET_CONFIG: samplerate = %d, channels = %d\n",
			__MM_FILE__, __func__, config.sample_rate,
			config.channel_count);
		break;
	}
	default:
		rc = -EINVAL;
	}
	pr_debug("[%s:%s] rc = %d\n", __MM_FILE__, __func__, rc);
	return rc;
}

static int q6_in_open(struct inode *inode, struct file *file)
{
	struct pcm *pcm;

	pr_info("[%s:%s] open\n", __MM_FILE__, __func__);
	pcm = kzalloc(sizeof(struct pcm), GFP_KERNEL);

	if (!pcm)
		return -ENOMEM;

	pcm->channel_count = 1;
	pcm->sample_rate = 8000;
	pcm->buffer_size = BUFSZ;
	pcm->rec_mode = AUDIO_FLAG_READ;
	file->private_data = pcm;
	return 0;
}

static ssize_t q6_in_read(struct file *file, char __user *buf,
			  size_t count, loff_t *pos)
{
	struct pcm *pcm = file->private_data;
	struct audio_client *ac;
	struct audio_buffer *ab;
	const char __user *start = buf;
	int xfer;
	int res;

	pr_debug("[%s:%s] count = %d\n", __MM_FILE__, __func__, count);
	ac = pcm->ac;
	if (!ac) {
		res = -ENODEV;
		goto fail;
	}
	while (count > 0) {
		ab = ac->buf + ac->cpu_buf;

		if (ab->used)
			if (!wait_event_timeout(ac->wait, (ab->used == 0), 5*HZ)) {
				audio_client_dump(ac);
				pr_err("[%s:%s] timeout. dsp dead?\n",
						__MM_FILE__, __func__);
				q6audio_dsp_not_responding();
			}
		pr_debug("[%s:%s] ab->data = %p, cpu_buf = %d", __MM_FILE__,
			__func__, ab->data, ac->cpu_buf);
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
	return res;
}

static int q6_in_release(struct inode *inode, struct file *file)
{

	int rc = 0;
	struct pcm *pcm = file->private_data;
	if (pcm->ac)
		rc = q6audio_close(pcm->ac);
	kfree(pcm);
	pr_info("[%s:%s] release\n", __MM_FILE__, __func__);
	return rc;
}

static struct file_operations q6_in_fops = {
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

static int __init q6_in_init(void) {
	return misc_register(&q6_in_misc);
}

device_initcall(q6_in_init);

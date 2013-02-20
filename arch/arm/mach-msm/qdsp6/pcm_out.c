/* arch/arm/mach-msm/qdsp6/pcm_out.c
 *
 * Copyright (C) 2009 Google, Inc.
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

void audio_client_dump(struct audio_client *ac);

#define BUFSZ (3072)

struct pcm {
	struct mutex lock;
	struct audio_client *ac;
	uint32_t sample_rate;
	uint32_t channel_count;
	size_t buffer_size;
};

static long pcm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct pcm *pcm = file->private_data;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		memset(&stats, 0, sizeof(stats));
		if (copy_to_user((void*) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&pcm->lock);
	switch (cmd) {
	case AUDIO_SET_VOLUME: {
		int vol;
		if (!pcm->ac) {
			pr_err("%s: cannot set volume before AUDIO_START!\n",
				__func__);
			rc = -EINVAL;
			break;
		}
		if (copy_from_user(&vol, (void*) arg, sizeof(vol))) {
			rc = -EFAULT;
			break;
		}
		pr_debug("[%s:%s] SET_VOLUME: vol = %d\n", __MM_FILE__,
			__func__, vol);
		rc = q6audio_set_stream_volume(pcm->ac, vol);
		break;
	}
	case AUDIO_START: {
		uint32_t acdb_id;
		pr_debug("[%s:%s] AUDIO_START\n", __MM_FILE__, __func__);
		if (arg == 0) {
			acdb_id = 0;
		} else if (copy_from_user(&acdb_id, (void*) arg, sizeof(acdb_id))) {
			pr_info("[%s:%s] copy acdb_id from user failed\n",
					__MM_FILE__, __func__);
			rc = -EFAULT;
			break;
		}
		if (pcm->ac) {
			pr_err("[%s:%s] active session already existing\n",
				__MM_FILE__, __func__);
			rc = -EBUSY;
		} else {
			pcm->ac = q6audio_open_pcm(pcm->buffer_size,
						pcm->sample_rate,
						pcm->channel_count,
						AUDIO_FLAG_WRITE, acdb_id);
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
		if (pcm->ac) {
			rc = -EBUSY;
			pr_err("[%s:%s] active session already existing\n",
				__MM_FILE__, __func__);
			break;
		}
		if (copy_from_user(&config, (void*) arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		pr_debug("[%s:%s] SET_CONFIG: samplerate = %d, channels = %d\n",
			__MM_FILE__, __func__, config.sample_rate,
			config.channel_count);
		if (config.channel_count < 1 || config.channel_count > 2) {
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
	case AUDIO_SET_EQ: {
		struct msm_audio_eq_stream_config eq_config;
		pr_debug("[%s:%s] SET_EQ\n", __MM_FILE__, __func__);
		if (copy_from_user(&eq_config, (void *) arg,
						sizeof(eq_config))) {
			rc = -EFAULT;
			break;
		}
		rc = q6audio_set_stream_eq_pcm(pcm->ac, (void *) &eq_config);
		break;
	}
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&pcm->lock);
	pr_debug("[%s:%s] rc = %d\n", __MM_FILE__, __func__, rc);
	return rc;
}

static int pcm_open(struct inode *inode, struct file *file)
{
	struct pcm *pcm;

	pr_info("[%s:%s] open\n", __MM_FILE__, __func__);
	pcm = kzalloc(sizeof(struct pcm), GFP_KERNEL);

	if (!pcm)
		return -ENOMEM;

	mutex_init(&pcm->lock);
	pcm->channel_count = 2;
	pcm->sample_rate = 44100;
	pcm->buffer_size = BUFSZ;
	file->private_data = pcm;
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

	pr_debug("[%s:%s] count = %d\n", __MM_FILE__, __func__, count);
	if (!pcm->ac)
		pcm_ioctl(file, AUDIO_START, 0);

	ac = pcm->ac;
	if (!ac)
		return -ENODEV;

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
	pr_info("[%s:%s] release\n", __MM_FILE__, __func__);
	return 0;
}

static struct file_operations pcm_fops = {
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

static int __init pcm_init(void) {
	return misc_register(&pcm_misc);
}

device_initcall(pcm_init);

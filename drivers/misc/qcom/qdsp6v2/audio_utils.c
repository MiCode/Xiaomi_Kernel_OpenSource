/* Copyright (c) 2010-2014, 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
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
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/compat.h>
#include <asm/ioctls.h>
#include "audio_utils.h"

/*
 * Define maximum buffer size. Below values are chosen considering the higher
 * values used among all native drivers.
 */
#define MAX_FRAME_SIZE	1536
#define MAX_FRAMES	5
#define META_SIZE	(sizeof(struct meta_out_dsp))
#define MAX_BUFFER_SIZE	(1 + ((MAX_FRAME_SIZE + META_SIZE) * MAX_FRAMES))

static int audio_in_pause(struct q6audio_in  *audio)
{
	int rc;

	rc = q6asm_cmd(audio->ac, CMD_PAUSE);
	if (rc < 0)
		pr_err("%s:session id %d: pause cmd failed rc=%d\n", __func__,
				audio->ac->session, rc);

	return rc;
}

static int audio_in_flush(struct q6audio_in  *audio)
{
	int rc;

	pr_debug("%s:session id %d: flush\n", __func__, audio->ac->session);
	/* Flush if session running */
	if (audio->enabled) {
		/* Implicitly issue a pause to the encoder before flushing */
		rc = audio_in_pause(audio);
		if (rc < 0) {
			pr_err("%s:session id %d: pause cmd failed rc=%d\n",
				 __func__, audio->ac->session, rc);
			return rc;
		}

		rc = q6asm_cmd(audio->ac, CMD_FLUSH);
		if (rc < 0) {
			pr_err("%s:session id %d: flush cmd failed rc=%d\n",
				__func__, audio->ac->session, rc);
			return rc;
		}
		/* 2nd arg: 0 -> run immediately
		   3rd arg: 0 -> msw_ts, 4th arg: 0 ->lsw_ts */
		q6asm_run(audio->ac, 0x00, 0x00, 0x00);
		pr_debug("Rerun the session\n");
	}
	audio->rflush = 1;
	audio->wflush = 1;
	memset(audio->out_frame_info, 0, sizeof(audio->out_frame_info));
	wake_up(&audio->read_wait);
	/* get read_lock to ensure no more waiting read thread */
	mutex_lock(&audio->read_lock);
	audio->rflush = 0;
	mutex_unlock(&audio->read_lock);
	wake_up(&audio->write_wait);
	/* get write_lock to ensure no more waiting write thread */
	mutex_lock(&audio->write_lock);
	audio->wflush = 0;
	mutex_unlock(&audio->write_lock);
	pr_debug("%s:session id %d: in_bytes %d\n", __func__,
			audio->ac->session, atomic_read(&audio->in_bytes));
	pr_debug("%s:session id %d: in_samples %d\n", __func__,
			audio->ac->session, atomic_read(&audio->in_samples));
	atomic_set(&audio->in_bytes, 0);
	atomic_set(&audio->in_samples, 0);
	atomic_set(&audio->out_count, 0);
	return 0;
}

/* must be called with audio->lock held */
int audio_in_enable(struct q6audio_in  *audio)
{
	if (audio->enabled)
		return 0;

	/* 2nd arg: 0 -> run immediately
		3rd arg: 0 -> msw_ts, 4th arg: 0 ->lsw_ts */
	return q6asm_run(audio->ac, 0x00, 0x00, 0x00);
}

/* must be called with audio->lock held */
int audio_in_disable(struct q6audio_in  *audio)
{
	int rc = 0;
	if (!audio->stopped) {
		audio->enabled = 0;
		audio->opened = 0;
		pr_debug("%s:session id %d: inbytes[%d] insamples[%d]\n",
				__func__, audio->ac->session,
				atomic_read(&audio->in_bytes),
				atomic_read(&audio->in_samples));

		rc = q6asm_cmd(audio->ac, CMD_CLOSE);
		if (rc < 0)
			pr_err("%s:session id %d: Failed to close the session rc=%d\n",
				__func__, audio->ac->session,
				rc);
		audio->stopped = 1;
		memset(audio->out_frame_info, 0,
				sizeof(audio->out_frame_info));
		wake_up(&audio->read_wait);
		wake_up(&audio->write_wait);
	}
	pr_debug("%s:session id %d: enabled[%d]\n", __func__,
			audio->ac->session, audio->enabled);
	return rc;
}

int audio_in_buf_alloc(struct q6audio_in *audio)
{
	int rc = 0;

	switch (audio->buf_alloc) {
	case NO_BUF_ALLOC:
		if (audio->feedback == NON_TUNNEL_MODE) {
			rc = q6asm_audio_client_buf_alloc(IN,
				audio->ac,
				ALIGN_BUF_SIZE(audio->pcm_cfg.buffer_size),
				audio->pcm_cfg.buffer_count);
			if (rc < 0) {
				pr_err("%s:session id %d: Buffer Alloc failed\n",
						__func__,
						audio->ac->session);
				rc = -ENOMEM;
				break;
			}
			audio->buf_alloc |= BUF_ALLOC_IN;
		}
		rc = q6asm_audio_client_buf_alloc(OUT, audio->ac,
				ALIGN_BUF_SIZE(audio->str_cfg.buffer_size),
				audio->str_cfg.buffer_count);
		if (rc < 0) {
			pr_err("%s:session id %d: Buffer Alloc failed rc=%d\n",
					__func__, audio->ac->session, rc);
			rc = -ENOMEM;
			break;
		}
		audio->buf_alloc |= BUF_ALLOC_OUT;
		break;
	case BUF_ALLOC_IN:
		rc = q6asm_audio_client_buf_alloc(OUT, audio->ac,
				ALIGN_BUF_SIZE(audio->str_cfg.buffer_size),
				audio->str_cfg.buffer_count);
		if (rc < 0) {
			pr_err("%s:session id %d: Buffer Alloc failed rc=%d\n",
					__func__, audio->ac->session, rc);
			rc = -ENOMEM;
			break;
		}
		audio->buf_alloc |= BUF_ALLOC_OUT;
		break;
	case BUF_ALLOC_OUT:
		if (audio->feedback == NON_TUNNEL_MODE) {
			rc = q6asm_audio_client_buf_alloc(IN, audio->ac,
				ALIGN_BUF_SIZE(audio->pcm_cfg.buffer_size),
				audio->pcm_cfg.buffer_count);
			if (rc < 0) {
				pr_err("%s:session id %d: Buffer Alloc failed\n",
					__func__,
					audio->ac->session);
				rc = -ENOMEM;
				break;
			}
			audio->buf_alloc |= BUF_ALLOC_IN;
		}
		break;
	default:
		pr_debug("%s:session id %d: buf[%d]\n", __func__,
					audio->ac->session, audio->buf_alloc);
	}

	return rc;
}

int audio_in_set_config(struct file *file,
		struct msm_audio_config *cfg)
{
	int rc = 0;
	struct q6audio_in  *audio = file->private_data;

	if (audio->feedback != NON_TUNNEL_MODE) {
		pr_err("%s:session id %d: Not sufficient permission to change the record mode\n",
			__func__, audio->ac->session);
		rc = -EACCES;
		goto ret;
	}
	if ((cfg->buffer_count > PCM_BUF_COUNT) ||
		(cfg->buffer_count == 1))
		cfg->buffer_count = PCM_BUF_COUNT;

	audio->pcm_cfg.buffer_count = cfg->buffer_count;
	audio->pcm_cfg.buffer_size  = cfg->buffer_size;
	audio->pcm_cfg.channel_count = cfg->channel_count;
	audio->pcm_cfg.sample_rate = cfg->sample_rate;
	if (audio->opened && audio->feedback == NON_TUNNEL_MODE) {
		rc = q6asm_audio_client_buf_alloc(IN, audio->ac,
			ALIGN_BUF_SIZE(audio->pcm_cfg.buffer_size),
			audio->pcm_cfg.buffer_count);
		if (rc < 0) {
			pr_err("%s:session id %d: Buffer Alloc failed\n",
				__func__, audio->ac->session);
			rc = -ENOMEM;
			goto ret;
		}
	}
	audio->buf_alloc |= BUF_ALLOC_IN;
	rc = 0;
	pr_debug("%s:session id %d: AUDIO_SET_CONFIG %d %d\n", __func__,
			audio->ac->session, audio->pcm_cfg.buffer_count,
			audio->pcm_cfg.buffer_size);
ret:
	return rc;
}
/* ------------------- device --------------------- */
static long audio_in_ioctl_shared(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct q6audio_in  *audio = file->private_data;
	int rc = 0;

	switch (cmd) {
	case AUDIO_FLUSH: {
		/* Make sure we're stopped and we wake any threads
		* that might be blocked holding the read_lock.
		* While audio->stopped read threads will always
		* exit immediately.
		*/
		rc = audio_in_flush(audio);
		if (rc < 0)
			pr_err("%s:session id %d: Flush Fail rc=%d\n",
				__func__, audio->ac->session, rc);
		else { /* Register back the flushed read buffer with DSP */
			int cnt = 0;
			while (cnt++ < audio->str_cfg.buffer_count)
				q6asm_read(audio->ac); /* Push buffer to DSP */
			pr_debug("register the read buffer\n");
		}
		break;
	}
	case AUDIO_PAUSE: {
		pr_debug("%s:session id %d: AUDIO_PAUSE\n", __func__,
					audio->ac->session);
		if (audio->enabled)
			audio_in_pause(audio);
		break;
	}
	case AUDIO_GET_SESSION_ID: {
		if (copy_to_user((void *) arg, &audio->ac->session,
			sizeof(u16))) {
			pr_err("%s: copy_to_user for AUDIO_GET_SESSION_ID failed\n",
				__func__);
			rc = -EFAULT;
		}
		break;
	}
	default:
		pr_err("%s: Unknown ioctl cmd = %d", __func__, cmd);
		rc = -EINVAL;
	}
	return rc;
}

long audio_in_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct q6audio_in  *audio = file->private_data;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		memset(&stats, 0, sizeof(stats));
		stats.byte_count = atomic_read(&audio->in_bytes);
		stats.sample_count = atomic_read(&audio->in_samples);
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return rc;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_FLUSH:
	case AUDIO_PAUSE:
	case AUDIO_GET_SESSION_ID:
		rc = audio_in_ioctl_shared(file, cmd, arg);
		break;
	case AUDIO_GET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		memset(&cfg, 0, sizeof(cfg));
		cfg.buffer_size = audio->str_cfg.buffer_size;
		cfg.buffer_count = audio->str_cfg.buffer_count;
		if (copy_to_user((void *)arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		pr_debug("%s:session id %d: AUDIO_GET_STREAM_CONFIG %d %d\n",
				__func__, audio->ac->session, cfg.buffer_size,
				cfg.buffer_count);
		break;
	}
	case AUDIO_SET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		if (copy_from_user(&cfg, (void *)arg, sizeof(cfg))) {
			pr_err("%s: copy_from_user for AUDIO_SET_STREAM_CONFIG failed\n"
				, __func__);
			rc = -EFAULT;
			break;
		}
		/* Minimum single frame size,
		   but with in maximum frames number */
		if ((cfg.buffer_size < (audio->min_frame_size+ \
			sizeof(struct meta_out_dsp))) ||
			(cfg.buffer_count < FRAME_NUM)) {
			rc = -EINVAL;
			break;
		}
		if (cfg.buffer_size > MAX_BUFFER_SIZE) {
			rc = -EINVAL;
			break;
		}
		audio->str_cfg.buffer_size = cfg.buffer_size;
		audio->str_cfg.buffer_count = cfg.buffer_count;
		if (audio->opened) {
			rc = q6asm_audio_client_buf_alloc(OUT, audio->ac,
				ALIGN_BUF_SIZE(audio->str_cfg.buffer_size),
				audio->str_cfg.buffer_count);
			if (rc < 0) {
				pr_err("%s: session id %d: Buffer Alloc failed rc=%d\n",
					__func__, audio->ac->session, rc);
				rc = -ENOMEM;
				break;
			}
		}
		audio->buf_alloc |= BUF_ALLOC_OUT;
		rc = 0;
		pr_debug("%s:session id %d: AUDIO_SET_STREAM_CONFIG %d %d\n",
				__func__, audio->ac->session,
				audio->str_cfg.buffer_size,
				audio->str_cfg.buffer_count);
		break;
	}
	case AUDIO_SET_BUF_CFG: {
		struct msm_audio_buf_cfg  cfg;
		if (copy_from_user(&cfg, (void *)arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}
		if ((audio->feedback == NON_TUNNEL_MODE) &&
			!cfg.meta_info_enable) {
			rc = -EFAULT;
			break;
		}

		/* Restrict the num of frames per buf to coincide with
		 * default buf size */
		if (cfg.frames_per_buf > audio->max_frames_per_buf) {
			rc = -EFAULT;
			break;
		}
		audio->buf_cfg.meta_info_enable = cfg.meta_info_enable;
		audio->buf_cfg.frames_per_buf = cfg.frames_per_buf;
		pr_debug("%s:session id %d: Set-buf-cfg: meta[%d] framesperbuf[%d]\n",
				__func__,
				audio->ac->session, cfg.meta_info_enable,
				cfg.frames_per_buf);
		break;
	}
	case AUDIO_GET_BUF_CFG: {
		pr_debug("%s:session id %d: Get-buf-cfg: meta[%d] framesperbuf[%d]\n",
			__func__,
			audio->ac->session, audio->buf_cfg.meta_info_enable,
			audio->buf_cfg.frames_per_buf);

		if (copy_to_user((void *)arg, &audio->buf_cfg,
					sizeof(struct msm_audio_buf_cfg)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_GET_CONFIG: {
		if (copy_to_user((void *)arg, &audio->pcm_cfg,
					sizeof(struct msm_audio_config)))
			rc = -EFAULT;
		break;

	}
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config cfg;
		if (copy_from_user(&cfg, (void *)arg, sizeof(cfg))) {
			pr_err("%s: copy_from_user for AUDIO_SET_CONFIG failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		rc = audio_in_set_config(file, &cfg);
		break;
	}
	default:
		/* call codec specific ioctl */
		rc = audio->enc_ioctl(file, cmd, arg);
	}
	mutex_unlock(&audio->lock);
	return rc;
}

#ifdef CONFIG_COMPAT
struct msm_audio_stats32 {
	u32 byte_count;
	u32 sample_count;
	u32 unused[2];
};

struct msm_audio_stream_config32 {
	u32 buffer_size;
	u32 buffer_count;
};

struct msm_audio_config32 {
	u32 buffer_size;
	u32 buffer_count;
	u32 channel_count;
	u32 sample_rate;
	u32 type;
	u32 meta_field;
	u32 bits;
	u32 unused[3];
};

struct msm_audio_buf_cfg32 {
	u32 meta_info_enable;
	u32 frames_per_buf;
};

enum {
	AUDIO_GET_CONFIG_32 = _IOR(AUDIO_IOCTL_MAGIC, 3,
			struct msm_audio_config32),
	AUDIO_SET_CONFIG_32 = _IOW(AUDIO_IOCTL_MAGIC, 4,
			struct msm_audio_config32),
	AUDIO_GET_STATS_32 = _IOR(AUDIO_IOCTL_MAGIC, 5,
			struct msm_audio_stats32),
	AUDIO_SET_STREAM_CONFIG_32 = _IOW(AUDIO_IOCTL_MAGIC, 80,
			struct msm_audio_stream_config32),
	AUDIO_GET_STREAM_CONFIG_32 = _IOR(AUDIO_IOCTL_MAGIC, 81,
			struct msm_audio_stream_config32),
	AUDIO_SET_BUF_CFG_32 = _IOW(AUDIO_IOCTL_MAGIC, 94,
			struct msm_audio_buf_cfg32),
	AUDIO_GET_BUF_CFG_32 = _IOW(AUDIO_IOCTL_MAGIC, 93,
			struct msm_audio_buf_cfg32),
};

long audio_in_compat_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct q6audio_in  *audio = file->private_data;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS_32) {
		struct msm_audio_stats32 stats_32;
		memset(&stats_32, 0, sizeof(stats_32));
		stats_32.byte_count = atomic_read(&audio->in_bytes);
		stats_32.sample_count = atomic_read(&audio->in_samples);
		if (copy_to_user((void *) arg, &stats_32, sizeof(stats_32))) {
			pr_err("%s: copy_to_user failed for AUDIO_GET_STATS_32\n",
				__func__);
			return -EFAULT;
		}
		return rc;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_FLUSH:
	case AUDIO_PAUSE:
	case AUDIO_GET_SESSION_ID:
		rc = audio_in_ioctl_shared(file, cmd, arg);
		break;
	case AUDIO_GET_STREAM_CONFIG_32: {
		struct msm_audio_stream_config32 cfg_32;
		memset(&cfg_32, 0, sizeof(cfg_32));
		cfg_32.buffer_size = audio->str_cfg.buffer_size;
		cfg_32.buffer_count = audio->str_cfg.buffer_count;
		if (copy_to_user((void *)arg, &cfg_32, sizeof(cfg_32))) {
			pr_err("%s: Copy to user failed\n", __func__);
			rc = -EFAULT;
		}
		pr_debug("%s:session id %d: AUDIO_GET_STREAM_CONFIG %d %d\n",
				__func__, audio->ac->session,
				cfg_32.buffer_size,
				cfg_32.buffer_count);
		break;
	}
	case AUDIO_SET_STREAM_CONFIG_32: {
		struct msm_audio_stream_config32 cfg_32;
		struct msm_audio_stream_config cfg;
		if (copy_from_user(&cfg_32, (void *)arg, sizeof(cfg_32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_STREAM_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		cfg.buffer_size = cfg_32.buffer_size;
		cfg.buffer_count = cfg_32.buffer_count;
		/* Minimum single frame size,
		 * but with in maximum frames number */
		if ((cfg.buffer_size < (audio->min_frame_size +
			sizeof(struct meta_out_dsp))) ||
			(cfg.buffer_count < FRAME_NUM)) {
			rc = -EINVAL;
			break;
		}
		audio->str_cfg.buffer_size = cfg.buffer_size;
		audio->str_cfg.buffer_count = cfg.buffer_count;
		if (audio->opened) {
			rc = q6asm_audio_client_buf_alloc(OUT, audio->ac,
				ALIGN_BUF_SIZE(audio->str_cfg.buffer_size),
				audio->str_cfg.buffer_count);
			if (rc < 0) {
				pr_err("%s: session id %d:\n",
					__func__, audio->ac->session);
				pr_err("Buffer Alloc failed rc=%d\n", rc);
				rc = -ENOMEM;
				break;
			}
		}
		audio->buf_alloc |= BUF_ALLOC_OUT;
		pr_debug("%s:session id %d: AUDIO_SET_STREAM_CONFIG %d %d\n",
				__func__, audio->ac->session,
				audio->str_cfg.buffer_size,
				audio->str_cfg.buffer_count);
		break;
	}
	case AUDIO_SET_BUF_CFG_32: {
		struct msm_audio_buf_cfg32 cfg_32;
		struct msm_audio_buf_cfg cfg;
		if (copy_from_user(&cfg_32, (void *)arg, sizeof(cfg_32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_BUG_CFG_32 failed",
				__func__);
			rc = -EFAULT;
			break;
		}
		cfg.meta_info_enable = cfg_32.meta_info_enable;
		cfg.frames_per_buf = cfg_32.frames_per_buf;

		if ((audio->feedback == NON_TUNNEL_MODE) &&
			!cfg.meta_info_enable) {
			rc = -EFAULT;
			break;
		}

		/* Restrict the num of frames per buf to coincide with
		 * default buf size */
		if (cfg.frames_per_buf > audio->max_frames_per_buf) {
			rc = -EFAULT;
			break;
		}
		audio->buf_cfg.meta_info_enable = cfg.meta_info_enable;
		audio->buf_cfg.frames_per_buf = cfg.frames_per_buf;
		pr_debug("%s:session id %d: Set-buf-cfg: meta[%d] framesperbuf[%d]\n",
			__func__, audio->ac->session, cfg.meta_info_enable,
			cfg.frames_per_buf);
		break;
	}
	case AUDIO_GET_BUF_CFG_32: {
		struct msm_audio_buf_cfg32 cfg_32;
		pr_debug("%s:session id %d: Get-buf-cfg: meta[%d] framesperbuf[%d]\n",
			__func__,
			audio->ac->session, audio->buf_cfg.meta_info_enable,
			audio->buf_cfg.frames_per_buf);
		cfg_32.meta_info_enable = audio->buf_cfg.meta_info_enable;
		cfg_32.frames_per_buf = audio->buf_cfg.frames_per_buf;

		if (copy_to_user((void *)arg, &cfg_32,
			sizeof(struct msm_audio_buf_cfg32))) {
			pr_err("%s: Copy to user failed\n", __func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_GET_CONFIG_32: {
		struct msm_audio_config32 cfg_32;
		memset(&cfg_32, 0, sizeof(cfg_32));
		cfg_32.buffer_size = audio->pcm_cfg.buffer_size;
		cfg_32.buffer_count = audio->pcm_cfg.buffer_count;
		cfg_32.channel_count = audio->pcm_cfg.channel_count;
		cfg_32.sample_rate = audio->pcm_cfg.sample_rate;
		cfg_32.type = audio->pcm_cfg.type;
		cfg_32.meta_field = audio->pcm_cfg.meta_field;
		cfg_32.bits = audio->pcm_cfg.bits;

		if (copy_to_user((void *)arg, &cfg_32,
					sizeof(struct msm_audio_config32))) {
			pr_err("%s: Copy to user failed\n", __func__);
			rc = -EFAULT;
		}
		break;
	}
	case AUDIO_SET_CONFIG_32: {
		struct msm_audio_config32 cfg_32;
		struct msm_audio_config cfg;
		if (copy_from_user(&cfg_32, (void *)arg, sizeof(cfg_32))) {
			pr_err("%s: copy_from_user for AUDIO_SET_CONFIG_32 failed\n",
				__func__);
			rc = -EFAULT;
			break;
		}
		cfg.buffer_size = cfg_32.buffer_size;
		cfg.buffer_count = cfg_32.buffer_count;
		cfg.channel_count = cfg_32.channel_count;
		cfg.sample_rate = cfg_32.sample_rate;
		cfg.type = cfg_32.type;
		cfg.meta_field = cfg_32.meta_field;
		cfg.bits = cfg_32.bits;
		rc = audio_in_set_config(file, &cfg);
		break;
	}
	default:
		  /* call codec specific ioctl */
		  rc = audio->enc_compat_ioctl(file, cmd, arg);
	}
	mutex_unlock(&audio->lock);
	return rc;
}
#endif

ssize_t audio_in_read(struct file *file,
				char __user *buf,
				size_t count, loff_t *pos)
{
	struct q6audio_in  *audio = file->private_data;
	const char __user *start = buf;
	unsigned char *data;
	uint32_t offset = 0;
	uint32_t size = 0;
	int rc = 0;
	uint32_t idx;
	struct meta_out_dsp meta;
	uint32_t bytes_to_copy = 0;
	uint32_t mfield_size = (audio->buf_cfg.meta_info_enable == 0) ? 0 :
		(sizeof(unsigned char) +
		(sizeof(struct meta_out_dsp)*(audio->buf_cfg.frames_per_buf)));

	memset(&meta, 0, sizeof(meta));
	pr_debug("%s:session id %d: read - %zd\n", __func__, audio->ac->session,
			count);
	if (!audio->enabled)
		return -EFAULT;
	mutex_lock(&audio->read_lock);
	while (count > 0) {
		rc = wait_event_interruptible(
			audio->read_wait,
			((atomic_read(&audio->out_count) > 0) ||
			(audio->stopped) ||
			 audio->rflush || audio->eos_rsp ||
			audio->event_abort));

		if (audio->event_abort) {
			rc = -EIO;
			break;
		}


		if (rc < 0)
			break;

		if ((audio->stopped && !(atomic_read(&audio->out_count))) ||
			audio->rflush) {
			pr_debug("%s:session id %d: driver in stop state or flush,No more buf to read",
				__func__,
				audio->ac->session);
			rc = 0;/* End of File */
			break;
		}
		if (!(atomic_read(&audio->out_count)) &&
			(audio->eos_rsp == 1) &&
			(count >= (sizeof(unsigned char) +
				sizeof(struct meta_out_dsp)))) {
			unsigned char num_of_frames;
			pr_info("%s:session id %d: eos %d at output\n",
				__func__, audio->ac->session, audio->eos_rsp);
			if (buf != start)
				break;
			num_of_frames = 0xFF;
			if (copy_to_user(buf, &num_of_frames,
					sizeof(unsigned char))) {
				rc = -EFAULT;
				break;
			}
			buf += sizeof(unsigned char);
			meta.frame_size = 0xFFFF;
			meta.encoded_pcm_samples = 0xFFFF;
			meta.msw_ts = 0x00;
			meta.lsw_ts = 0x00;
			meta.nflags = AUD_EOS_SET;
			audio->eos_rsp = 0;
			if (copy_to_user(buf, &meta, sizeof(meta))) {
				rc = -EFAULT;
				break;
			}
			buf += sizeof(meta);
			break;
		}
		data = (unsigned char *)q6asm_is_cpu_buf_avail(OUT, audio->ac,
						&size, &idx);
		if ((count >= (size + mfield_size)) && data) {
			if (audio->buf_cfg.meta_info_enable) {
				if (copy_to_user(buf,
					&audio->out_frame_info[idx][0],
					sizeof(unsigned char))) {
					rc = -EFAULT;
					break;
				}
				bytes_to_copy =
					(size + audio->out_frame_info[idx][1]);
				/* Number of frames information copied */
				buf += sizeof(unsigned char);
				count -= sizeof(unsigned char);
			} else {
				offset = audio->out_frame_info[idx][1];
				bytes_to_copy = size;
			}

			pr_debug("%s:session id %d: offset=%d nr of frames= %d\n",
					__func__, audio->ac->session,
					audio->out_frame_info[idx][1],
					audio->out_frame_info[idx][0]);

			if (copy_to_user(buf, &data[offset], bytes_to_copy)) {
				rc = -EFAULT;
				break;
			}
			count -= bytes_to_copy;
			buf += bytes_to_copy;
		} else {
			pr_err("%s:session id %d: short read data[%pK] bytesavail[%d]bytesrequest[%zd]\n",
				__func__,
				audio->ac->session,
				data, size, count);
		}
		atomic_dec(&audio->out_count);
		q6asm_read(audio->ac);
		break;
	}
	mutex_unlock(&audio->read_lock);

	pr_debug("%s:session id %d: read: %zd bytes\n", __func__,
			audio->ac->session, (buf-start));
	if (buf > start)
		return buf - start;
	return rc;
}

static int extract_meta_info(char *buf, unsigned long *msw_ts,
		unsigned long *lsw_ts, unsigned int *flags)
{
	struct meta_in *meta = (struct meta_in *)buf;
	*msw_ts = meta->ntimestamp.highpart;
	*lsw_ts = meta->ntimestamp.lowpart;
	*flags = meta->nflags;
	return 0;
}

ssize_t audio_in_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *pos)
{
	struct q6audio_in *audio = file->private_data;
	const char __user *start = buf;
	size_t xfer = 0;
	char *cpy_ptr;
	int rc = 0;
	unsigned char *data;
	uint32_t size = 0;
	uint32_t idx = 0;
	uint32_t nflags = 0;
	unsigned long msw_ts = 0;
	unsigned long lsw_ts = 0;
	uint32_t mfield_size = (audio->buf_cfg.meta_info_enable == 0) ? 0 :
			sizeof(struct meta_in);

	pr_debug("%s:session id %d: to write[%zd]\n", __func__,
			audio->ac->session, count);
	if (!audio->enabled)
		return -EFAULT;
	mutex_lock(&audio->write_lock);

	while (count > 0) {
		rc = wait_event_interruptible(audio->write_wait,
				     ((atomic_read(&audio->in_count) > 0) ||
				      (audio->stopped) ||
				      (audio->wflush) || (audio->event_abort)));

		if (audio->event_abort) {
			rc = -EIO;
			break;
		}

		if (rc < 0)
			break;
		if (audio->stopped || audio->wflush) {
			pr_debug("%s: session id %d: stop or flush\n", __func__,
					audio->ac->session);
			rc = -EBUSY;
			break;
		}
		/* if no PCM data, might have only eos buffer
		   such case do not hold cpu buffer */
		if ((buf == start) && (count == mfield_size)) {
			char eos_buf[sizeof(struct meta_in)];
			/* Processing begining of user buffer */
			if (copy_from_user(eos_buf, buf, mfield_size)) {
				rc = -EFAULT;
				break;
			}
			/* Check if EOS flag is set and buffer has
			 * contains just meta field
			 */
			extract_meta_info(eos_buf, &msw_ts, &lsw_ts,
						&nflags);
			buf += mfield_size;
			/* send the EOS and return */
			pr_debug("%s:session id %d: send EOS 0x%8x\n",
				__func__,
				audio->ac->session, nflags);
			break;
		}
		data = (unsigned char *)q6asm_is_cpu_buf_avail(IN, audio->ac,
						&size, &idx);
		if (!data) {
			pr_debug("%s:session id %d: No buf available\n",
				__func__, audio->ac->session);
			continue;
		}
		cpy_ptr = data;
		if (audio->buf_cfg.meta_info_enable) {
			if (buf == start) {
				/* Processing beginning of user buffer */
				if (copy_from_user(cpy_ptr, buf, mfield_size)) {
					rc = -EFAULT;
					break;
				}
				/* Check if EOS flag is set and buffer has
				* contains just meta field
				*/
				extract_meta_info(cpy_ptr, &msw_ts, &lsw_ts,
						&nflags);
				buf += mfield_size;
				count -= mfield_size;
			} else {
				pr_debug("%s:session id %d: continuous buffer\n",
						__func__, audio->ac->session);
			}
		}
		xfer = (count > (audio->pcm_cfg.buffer_size)) ?
				(audio->pcm_cfg.buffer_size) : count;

		if (copy_from_user(cpy_ptr, buf, xfer)) {
			rc = -EFAULT;
			break;
		}
		rc = q6asm_write(audio->ac, xfer, msw_ts, lsw_ts, 0x00);
		if (rc < 0) {
			rc = -EFAULT;
			break;
		}
		atomic_dec(&audio->in_count);
		count -= xfer;
		buf += xfer;
	}
	mutex_unlock(&audio->write_lock);
	pr_debug("%s:session id %d: eos_condition 0x%x buf[0x%pK] start[0x%pK]\n",
				__func__, audio->ac->session,
				nflags, buf, start);
	if (nflags & AUD_EOS_SET) {
		rc = q6asm_cmd(audio->ac, CMD_EOS);
		pr_info("%s:session id %d: eos %d at input\n", __func__,
				audio->ac->session, audio->eos_rsp);
	}
	pr_debug("%s:session id %d: Written %zd Avail Buf[%d]", __func__,
			audio->ac->session, (buf - start - mfield_size),
			atomic_read(&audio->in_count));
	if (!rc) {
		if (buf > start)
			return buf - start;
	}
	return rc;
}

int audio_in_release(struct inode *inode, struct file *file)
{
	struct q6audio_in  *audio = file->private_data;
	pr_info("%s: session id %d\n", __func__, audio->ac->session);
	mutex_lock(&audio->lock);
	audio_in_disable(audio);
	q6asm_audio_client_free(audio->ac);
	mutex_unlock(&audio->lock);
	kfree(audio->enc_cfg);
	kfree(audio->codec_cfg);
	kfree(audio);
	return 0;
}


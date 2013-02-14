/*
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 HTC Corporation
 * Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/wait.h>

#include <linux/msm_audio_qcp.h>
#include <mach/msm_qdsp6_audio.h>
#include "dal_audio_format.h"
#include <mach/debug_mm.h>

#define QCELP_FC_BUFF_CNT 10
#define QCELP_READ_TIMEOUT 2000
struct qcelp_fc_buff {
	struct mutex lock;
	int empty;
	void *data;
	int size;
	int actual_size;
};

struct qcelp_fc {
	struct task_struct *task;
	wait_queue_head_t fc_wq;
	struct qcelp_fc_buff fc_buff[QCELP_FC_BUFF_CNT];
	int buff_index;
};

struct qcelp {
	struct mutex lock;
	struct msm_audio_qcelp_enc_config cfg;
	struct msm_audio_stream_config str_cfg;
	struct audio_client *audio_client;
	struct msm_voicerec_mode voicerec_mode;
	struct qcelp_fc *qcelp_fc;
};


static int q6_qcelp_flowcontrol(void *data)
{
	struct audio_client *ac;
	struct audio_buffer *ab;
	struct qcelp *qcelp = data;
	int buff_index = 0;
	int xfer = 0;
	struct qcelp_fc *fc;


	ac = qcelp->audio_client;
	fc = qcelp->qcelp_fc;
	if (!ac) {
		pr_err("[%s:%s] audio_client is NULL\n", __MM_FILE__, __func__);
		return 0;
	}

	while (!kthread_should_stop()) {
		ab = ac->buf + ac->cpu_buf;
		if (ab->used)
			wait_event(ac->wait, (ab->used == 0));

		pr_debug("[%s:%s] ab->data = %p, cpu_buf = %d", __MM_FILE__,
			__func__, ab->data, ac->cpu_buf);
		xfer = ab->actual_size;


		mutex_lock(&(fc->fc_buff[buff_index].lock));
		if (!fc->fc_buff[buff_index].empty) {
			pr_err("[%s:%s] flow control buffer[%d] not read!\n",
					__MM_FILE__, __func__, buff_index);
		}

		if (fc->fc_buff[buff_index].size < xfer) {
			pr_err("[%s:%s] buffer %d too small\n", __MM_FILE__,
					__func__, buff_index);
			memcpy(fc->fc_buff[buff_index].data, ab->data,
					fc->fc_buff[buff_index].size);
			fc->fc_buff[buff_index].empty = 0;
			fc->fc_buff[buff_index].actual_size =
					fc->fc_buff[buff_index].size;
		} else {
			memcpy(fc->fc_buff[buff_index].data, ab->data, xfer);
			fc->fc_buff[buff_index].empty = 0;
			fc->fc_buff[buff_index].actual_size = xfer;
		}
		mutex_unlock(&(fc->fc_buff[buff_index].lock));
		/*wake up client, if any*/
		wake_up(&fc->fc_wq);

		buff_index++;
		if (buff_index >= QCELP_FC_BUFF_CNT)
			buff_index = 0;

		ab->used = 1;

		q6audio_read(ac, ab);
		ac->cpu_buf ^= 1;
	}

	return 0;
}
static long q6_qcelp_in_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	struct qcelp *qcelp = file->private_data;
	int rc = 0;
	int i = 0;
	struct qcelp_fc *fc;
	int size = 0;

	mutex_lock(&qcelp->lock);
	switch (cmd) {
	case AUDIO_SET_VOLUME:
		pr_debug("[%s:%s] SET_VOLUME\n", __MM_FILE__, __func__);
		break;
	case AUDIO_GET_STATS:
	{
		struct msm_audio_stats stats;
		pr_debug("[%s:%s] GET_STATS\n", __MM_FILE__, __func__);
		memset(&stats, 0, sizeof(stats));
		if (copy_to_user((void *) arg, &stats,
					sizeof(stats)))
			return -EFAULT;
		return 0;
	}
	case AUDIO_START:
	{
		uint32_t acdb_id;
		pr_debug("[%s:%s] AUDIO_START\n", __MM_FILE__, __func__);
		if (arg == 0) {
			acdb_id = 0;
		} else {
			if (copy_from_user(&acdb_id,
				(void *) arg, sizeof(acdb_id))) {
				rc = -EFAULT;
				break;
			}
		}
		if (qcelp->audio_client) {
			pr_err("[%s:%s] active session already existing\n",
				__MM_FILE__, __func__);
			rc = -EBUSY;
			break;
		} else {
			qcelp->audio_client = q6audio_open_qcp(
				qcelp->str_cfg.buffer_size,
				qcelp->cfg.min_bit_rate,
				qcelp->cfg.max_bit_rate,
				qcelp->voicerec_mode.rec_mode,
				ADSP_AUDIO_FORMAT_V13K_FS,
				acdb_id);

			if (!qcelp->audio_client) {
				pr_err("[%s:%s] qcelp open session failed\n",
					__MM_FILE__, __func__);
				kfree(qcelp);
				rc = -ENOMEM;
				break;
			}
		}

		/*allocate flow control buffers*/
		fc = qcelp->qcelp_fc;
		size = qcelp->str_cfg.buffer_size;
		for (i = 0; i < QCELP_FC_BUFF_CNT; ++i) {
			mutex_init(&(fc->fc_buff[i].lock));
			fc->fc_buff[i].empty = 1;
			fc->fc_buff[i].data = kmalloc(size, GFP_KERNEL);
			if (fc->fc_buff[i].data == NULL) {
				pr_err("[%s:%s] No memory for FC buffers\n",
						__MM_FILE__, __func__);
				rc = -ENOMEM;
				goto fc_fail;
			}
			fc->fc_buff[i].size = size;
			fc->fc_buff[i].actual_size = 0;
		}

		/*create flow control thread*/
		fc->task = kthread_run(q6_qcelp_flowcontrol,
				qcelp, "qcelp_flowcontrol");
		if (IS_ERR(fc->task)) {
			rc = PTR_ERR(fc->task);
			pr_err("[%s:%s] error creating flow control thread\n",
					__MM_FILE__, __func__);
			goto fc_fail;
		}
		break;
fc_fail:
		/*free flow control buffers*/
		--i;
		for (; i >=  0; i--) {
			kfree(fc->fc_buff[i].data);
			fc->fc_buff[i].data = NULL;
		}
		break;
	}
	case AUDIO_STOP:
		pr_debug("[%s:%s] AUDIO_STOP\n", __MM_FILE__, __func__);
		break;
	case AUDIO_FLUSH:
		break;
	case AUDIO_SET_INCALL: {
		pr_debug("[%s:%s] SET_INCALL\n", __MM_FILE__, __func__);
		if (copy_from_user(&qcelp->voicerec_mode,
			(void *)arg, sizeof(struct msm_voicerec_mode)))
			rc = -EFAULT;

		if (qcelp->voicerec_mode.rec_mode != AUDIO_FLAG_READ
			&& qcelp->voicerec_mode.rec_mode !=
			AUDIO_FLAG_INCALL_MIXED) {
			qcelp->voicerec_mode.rec_mode = AUDIO_FLAG_READ;
			pr_err("[%s:%s] Invalid rec_mode\n", __MM_FILE__,
					__func__);
			rc = -EINVAL;
		}
		break;
	}
	case AUDIO_GET_STREAM_CONFIG:
		if (copy_to_user((void *)arg, &qcelp->str_cfg,
				sizeof(struct msm_audio_stream_config)))
			rc = -EFAULT;
		pr_debug("[%s:%s] GET_STREAM_CONFIG: buffsz=%d, buffcnt=%d\n",
			 __MM_FILE__, __func__, qcelp->str_cfg.buffer_size,
			qcelp->str_cfg.buffer_count);
		break;
	case AUDIO_SET_STREAM_CONFIG:
		if (copy_from_user(&qcelp->str_cfg, (void *)arg,
			sizeof(struct msm_audio_stream_config))) {
			rc = -EFAULT;
			break;
		}
		pr_debug("[%s:%s] SET_STREAM_CONFIG: buffsz=%d, buffcnt=%d\n",
			 __MM_FILE__, __func__, qcelp->str_cfg.buffer_size,
			qcelp->str_cfg.buffer_count);

		if (qcelp->str_cfg.buffer_size < 35) {
			pr_err("[%s:%s] Buffer size too small\n", __MM_FILE__,
					__func__);
			rc = -EINVAL;
			break;
		}

		if (qcelp->str_cfg.buffer_count != 2)
			pr_info("[%s:%s] Buffer count set to 2\n", __MM_FILE__,
					__func__);
		break;
	case AUDIO_SET_QCELP_ENC_CONFIG:
		if (copy_from_user(&qcelp->cfg, (void *) arg,
				sizeof(struct msm_audio_qcelp_enc_config)))
			rc = -EFAULT;
		pr_debug("[%s:%s] SET_QCELP_ENC_CONFIG\n", __MM_FILE__,
			__func__);

		if (qcelp->cfg.min_bit_rate > 4 ||
			 qcelp->cfg.min_bit_rate < 1) {

			pr_err("[%s:%s] invalid min bitrate\n", __MM_FILE__,
					__func__);
			rc = -EINVAL;
		}
		if (qcelp->cfg.max_bit_rate > 4 ||
			 qcelp->cfg.max_bit_rate < 1) {

			pr_err("[%s:%s] invalid max bitrate\n", __MM_FILE__,
					__func__);
			rc = -EINVAL;
		}

		break;
	case AUDIO_GET_QCELP_ENC_CONFIG:
		if (copy_to_user((void *) arg, &qcelp->cfg,
			 sizeof(struct msm_audio_qcelp_enc_config)))
			rc = -EFAULT;
		pr_debug("[%s:%s] GET_QCELP_ENC_CONFIG\n", __MM_FILE__,
			__func__);
		break;

	default:
		rc = -EINVAL;
	}
	mutex_unlock(&qcelp->lock);
	pr_debug("[%s:%s] rc = %d\n", __MM_FILE__, __func__, rc);
	return rc;
}

static int q6_qcelp_in_open(struct inode *inode, struct file *file)
{
	struct qcelp *qcelp;
	struct qcelp_fc *fc;
	int i;
	pr_info("[%s:%s] open\n", __MM_FILE__, __func__);
	qcelp = kmalloc(sizeof(struct qcelp), GFP_KERNEL);
	if (qcelp == NULL) {
		pr_err("[%s:%s] Could not allocate memory for qcelp driver\n",
				__MM_FILE__, __func__);
		return -ENOMEM;
	}

	mutex_init(&qcelp->lock);
	file->private_data = qcelp;
	qcelp->audio_client = NULL;
	qcelp->str_cfg.buffer_size = 35;
	qcelp->str_cfg.buffer_count = 2;
	qcelp->cfg.cdma_rate = CDMA_RATE_FULL;
	qcelp->cfg.min_bit_rate = 1;
	qcelp->cfg.max_bit_rate = 4;
	qcelp->voicerec_mode.rec_mode = AUDIO_FLAG_READ;

	qcelp->qcelp_fc = kmalloc(sizeof(struct qcelp_fc), GFP_KERNEL);
	if (qcelp->qcelp_fc == NULL) {
		pr_err("[%s:%s] Could not allocate memory for qcelp_fc\n",
				__MM_FILE__, __func__);
		kfree(qcelp);
		return -ENOMEM;
	}
	fc = qcelp->qcelp_fc;
	fc->task = NULL;
	fc->buff_index = 0;
	for (i = 0; i < QCELP_FC_BUFF_CNT; ++i) {
		fc->fc_buff[i].data = NULL;
		fc->fc_buff[i].size = 0;
		fc->fc_buff[i].actual_size = 0;
	}
	/*initialize wait queue head*/
	init_waitqueue_head(&fc->fc_wq);
	return 0;
}

static ssize_t q6_qcelp_in_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct audio_client *ac;
	const char __user *start = buf;
	struct qcelp *qcelp = file->private_data;
	struct qcelp_fc *fc;
	int xfer = 0;
	int res = 0;

	pr_debug("[%s:%s] count = %d\n", __MM_FILE__, __func__, count);
	mutex_lock(&qcelp->lock);
	ac = qcelp->audio_client;
	if (!ac) {
		res = -ENODEV;
		goto fail;
	}
	fc = qcelp->qcelp_fc;
	while (count > xfer) {
		/*wait for buffer to full*/
		if (fc->fc_buff[fc->buff_index].empty != 0) {
			res = wait_event_interruptible_timeout(fc->fc_wq,
				(fc->fc_buff[fc->buff_index].empty == 0),
				msecs_to_jiffies(QCELP_READ_TIMEOUT));

			pr_debug("[%s:%s] buff_index = %d\n", __MM_FILE__,
				__func__, fc->buff_index);
			if (res == 0) {
				pr_err("[%s:%s] Timeout!\n", __MM_FILE__,
						__func__);
				res = -ETIMEDOUT;
				goto fail;
			} else if (res < 0) {
				pr_err("[%s:%s] Returning on Interrupt\n",
					__MM_FILE__, __func__);
				goto fail;
			}
		}
		/*lock the buffer*/
		mutex_lock(&(fc->fc_buff[fc->buff_index].lock));
		xfer = fc->fc_buff[fc->buff_index].actual_size;

		if (xfer > count) {
			mutex_unlock(&(fc->fc_buff[fc->buff_index].lock));
			pr_err("[%s:%s] read failed! byte count too small\n",
					__MM_FILE__, __func__);
			res = -EINVAL;
			goto fail;
		}

		if (copy_to_user(buf, fc->fc_buff[fc->buff_index].data,	xfer)) {
			mutex_unlock(&(fc->fc_buff[fc->buff_index].lock));
			pr_err("[%s:%s] copy_to_user failed at index %d\n",
					__MM_FILE__, __func__, fc->buff_index);
			res = -EFAULT;
			goto fail;
		}
		buf += xfer;
		count -= xfer;

		fc->fc_buff[fc->buff_index].empty = 1;
		fc->fc_buff[fc->buff_index].actual_size = 0;

		mutex_unlock(&(fc->fc_buff[fc->buff_index].lock));
		++(fc->buff_index);
		if (fc->buff_index >= QCELP_FC_BUFF_CNT)
			fc->buff_index = 0;
	}
	res = buf - start;

fail:
	mutex_unlock(&qcelp->lock);

	return res;
}

static int q6_qcelp_in_release(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct qcelp *qcelp = file->private_data;
	int i = 0;
	struct qcelp_fc *fc;

	mutex_lock(&qcelp->lock);
	fc = qcelp->qcelp_fc;
	kthread_stop(fc->task);
	fc->task = NULL;

	/*free flow control buffers*/
	for (i = 0; i < QCELP_FC_BUFF_CNT; ++i) {
		kfree(fc->fc_buff[i].data);
		fc->fc_buff[i].data = NULL;
	}
	kfree(fc);

	if (qcelp->audio_client)
		rc = q6audio_close(qcelp->audio_client);
	mutex_unlock(&qcelp->lock);
	kfree(qcelp);
	pr_info("[%s:%s] release\n", __MM_FILE__, __func__);
	return rc;
}

static const struct file_operations q6_qcelp_in_fops = {
	.owner		= THIS_MODULE,
	.open		= q6_qcelp_in_open,
	.read		= q6_qcelp_in_read,
	.release	= q6_qcelp_in_release,
	.unlocked_ioctl	= q6_qcelp_in_ioctl,
};

struct miscdevice q6_qcelp_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_qcelp_in",
	.fops	= &q6_qcelp_in_fops,
};

static int __init q6_qcelp_in_init(void)
{
	return misc_register(&q6_qcelp_in_misc);
}

device_initcall(q6_qcelp_in_init);

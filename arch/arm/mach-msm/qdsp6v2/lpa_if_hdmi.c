/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/msm_audio.h>
#include <mach/msm_hdmi_audio.h>
#include <mach/audio_dma_msm8k.h>
#include <sound/dai.h>
#include "q6core.h"

#define DMA_ALLOC_BUF_SZ		(SZ_4K * 16)

#define HDMI_AUDIO_FIFO_WATER_MARK	4

struct audio_buffer {
	dma_addr_t phys;
	void *data;
	uint32_t size;
	uint32_t used;	/* 1 = CPU is waiting for DMA to consume this buf */
	uint32_t actual_size;	/* actual number of bytes read by DMA */
};

struct lpa_if {
	struct mutex lock;
	struct msm_audio_config cfg;
	struct audio_buffer audio_buf[6];
	int cpu_buf;		/* next buffer the CPU will touch */
	int dma_buf;		/* next buffer the DMA will touch */
	u8 *buffer;
	dma_addr_t buffer_phys;
	u32 dma_ch;
	wait_queue_head_t wait;
	u32 config;
	u32 dma_period_sz;
	unsigned int num_periods;
};

static struct lpa_if  *lpa_if_ptr;

static unsigned int dma_buf_index;

static irqreturn_t lpa_if_irq(int intrsrc, void *data)
{
	struct lpa_if *lpa_if = data;
	int dma_ch = 0;
	unsigned int pending;

	if (lpa_if)
		dma_ch = lpa_if->dma_ch;
	else {
		pr_err("invalid lpa_if\n");
		return IRQ_NONE;
	}

	pending = (intrsrc
		   & (UNDER_CH(dma_ch) | PER_CH(dma_ch) | ERR_CH(dma_ch)));

	if (pending & UNDER_CH(dma_ch))
		pr_err("under run\n");
	if (pending & ERR_CH(dma_ch))
		pr_err("DMA %x Master Error\n", dma_ch);

	if (pending & PER_CH(dma_ch)) {

		lpa_if->audio_buf[lpa_if->dma_buf].used = 0;

		pr_debug("dma_buf %d  used %d\n", lpa_if->dma_buf,
			lpa_if->audio_buf[lpa_if->dma_buf].used);
		lpa_if->dma_buf++;
		lpa_if->dma_buf = lpa_if->dma_buf % lpa_if->cfg.buffer_count;

		if (lpa_if->dma_buf == lpa_if->cpu_buf)
			pr_err("Err:both dma_buf and cpu_buf are on same index\n");
		wake_up(&lpa_if->wait);
	}
	return IRQ_HANDLED;
}


int lpa_if_start(struct lpa_if *lpa_if)
{
	pr_debug("buf1 0x%x, buf2 0x%x dma_ch %d\n",
		(unsigned int)lpa_if->audio_buf[0].data,
		(unsigned int)lpa_if->audio_buf[1].data, lpa_if->dma_ch);

	dai_start_hdmi(lpa_if->dma_ch);

	hdmi_audio_enable(1, HDMI_AUDIO_FIFO_WATER_MARK);

	hdmi_audio_packet_enable(1);
	return 0;
}

int lpa_if_config(struct lpa_if *lpa_if)
{
	struct dai_dma_params dma_params;

	dma_params.src_start = lpa_if->buffer_phys;
	dma_params.buffer = lpa_if->buffer;
	dma_params.buffer_size = lpa_if->dma_period_sz * lpa_if->num_periods;
	dma_params.period_size = lpa_if->dma_period_sz;
	dma_params.channels = 2;

	lpa_if->dma_ch = 4;
	dai_set_params(lpa_if->dma_ch, &dma_params);

	register_dma_irq_handler(lpa_if->dma_ch, lpa_if_irq, (void *)lpa_if);

	mb();
	pr_debug("lpa_if 0x%08x  buf_vir 0x%08x   buf_phys 0x%08x  "
		"config %u\n", (u32)lpa_if, (u32) (lpa_if->buffer),
		lpa_if->buffer_phys, lpa_if->config);

	pr_debug("user_buf_cnt %u user_buf_size %u\n",
			lpa_if->cfg.buffer_count, lpa_if->cfg.buffer_size);

	lpa_if->config = 1;

	lpa_if_start(lpa_if);

	return 0;
}


static long lpa_if_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct lpa_if *lpa_if = file->private_data;
	int rc = 0;
	unsigned int i;
	pr_debug("cmd %u\n", cmd);

	mutex_lock(&lpa_if->lock);

	switch (cmd) {
	case AUDIO_START:
		pr_debug("AUDIO_START\n");

		if (dma_buf_index == 2) {
			if (!lpa_if->config) {
				rc = lpa_if_config(lpa_if);
				if (rc)
					pr_err("lpa_if_config failed\n");
			}
		} else {
			pr_err("did not receved two buffer for "
				"AUDIO_STAR\n");
			rc =  -EPERM;
		}
		break;

	case AUDIO_STOP:
		pr_debug("AUDIO_STOP\n");
		break;

	case AUDIO_FLUSH:
		pr_debug("AUDIO_FLUSH\n");
		break;


	case AUDIO_GET_CONFIG:
		pr_debug("AUDIO_GET_CONFIG\n");
		if (copy_to_user((void *)arg, &lpa_if->cfg,
				 sizeof(struct msm_audio_config))) {
			rc = -EFAULT;
		}
		break;
	case AUDIO_SET_CONFIG: {
		/*  Setting default rate as 48khz */
		unsigned int cur_sample_rate =
			HDMI_SAMPLE_RATE_48KHZ;
		struct msm_audio_config config;

		pr_debug("AUDIO_SET_CONFIG\n");
		if (copy_from_user(&config, (void *)arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		lpa_if->dma_period_sz = config.buffer_size;
		if ((lpa_if->dma_period_sz * lpa_if->num_periods) >
			DMA_ALLOC_BUF_SZ) {
			pr_err("Dma buffer size greater than allocated size\n");
			return -EINVAL;
		}
		pr_debug("Dma_period_sz %d\n", lpa_if->dma_period_sz);
		if (lpa_if->dma_period_sz < (2 * SZ_4K))
			lpa_if->num_periods = 6;
		pr_debug("No. of Periods %d\n", lpa_if->num_periods);

		lpa_if->cfg.buffer_count = lpa_if->num_periods;
		lpa_if->cfg.buffer_size = lpa_if->dma_period_sz *
						lpa_if->num_periods;

		for (i = 0; i < lpa_if->cfg.buffer_count; i++) {
			lpa_if->audio_buf[i].phys =
				lpa_if->buffer_phys + i * lpa_if->dma_period_sz;
			lpa_if->audio_buf[i].data =
				lpa_if->buffer + i * lpa_if->dma_period_sz;
			lpa_if->audio_buf[i].size = lpa_if->dma_period_sz;
			lpa_if->audio_buf[i].used = 0;
		}

		pr_debug("Sample rate %d\n", config.sample_rate);
		switch (config.sample_rate) {
		case 48000:
			cur_sample_rate = HDMI_SAMPLE_RATE_48KHZ;
			break;
		case 44100:
			cur_sample_rate = HDMI_SAMPLE_RATE_44_1KHZ;
			break;
		case 32000:
			cur_sample_rate = HDMI_SAMPLE_RATE_32KHZ;
			break;
		case 88200:
			cur_sample_rate = HDMI_SAMPLE_RATE_88_2KHZ;
			break;
		case 96000:
			cur_sample_rate = HDMI_SAMPLE_RATE_96KHZ;
			break;
		case 176400:
			cur_sample_rate = HDMI_SAMPLE_RATE_176_4KHZ;
			break;
		case 192000:
			cur_sample_rate = HDMI_SAMPLE_RATE_192KHZ;
			break;
		default:
			cur_sample_rate = HDMI_SAMPLE_RATE_48KHZ;
		}
		if (cur_sample_rate != hdmi_msm_audio_get_sample_rate())
			hdmi_msm_audio_sample_rate_reset(cur_sample_rate);
		else
			pr_debug("Previous sample rate and current"
				"sample rate are same\n");
		break;
	}
	default:
		pr_err("UnKnown Ioctl\n");
		rc = -EINVAL;
	}

	mutex_unlock(&lpa_if->lock);

	return rc;
}


static int lpa_if_open(struct inode *inode, struct file *file)
{
	pr_debug("\n");

	file->private_data = lpa_if_ptr;
	dma_buf_index = 0;
	lpa_if_ptr->cpu_buf = 2;
	lpa_if_ptr->dma_buf = 0;
	lpa_if_ptr->num_periods = 4;

	core_req_bus_bandwith(AUDIO_IF_BUS_ID, 100000, 0);
	mb();

	return 0;
}

static inline int rt_policy(int policy)
{
	if (unlikely(policy == SCHED_FIFO) || unlikely(policy == SCHED_RR))
		return 1;
	return 0;
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_policy(p->policy);
}
static ssize_t lpa_if_write(struct file *file, const char __user *buf,
		size_t count, loff_t *pos)
{
	struct lpa_if *lpa_if = file->private_data;
	struct audio_buffer *ab;
	const char __user *start = buf;
	int xfer, rc;
	struct sched_param s = { .sched_priority = 1 };
	int old_prio = current->rt_priority;
	int old_policy = current->policy;
	int cap_nice = cap_raised(current_cap(), CAP_SYS_NICE);

	 /* just for this write, set us real-time */
	if (!task_has_rt_policy(current)) {
		struct cred *new = prepare_creds();
		cap_raise(new->cap_effective, CAP_SYS_NICE);
		commit_creds(new);
		if ((sched_setscheduler(current, SCHED_RR, &s)) < 0)
			pr_err("sched_setscheduler failed\n");
	}
	mutex_lock(&lpa_if->lock);

	if (dma_buf_index < 2) {

		ab = lpa_if->audio_buf + dma_buf_index;

		if (copy_from_user(ab->data, buf, count)) {
			pr_err("copy from user failed\n");
			rc = 0;
			goto end;

		}
		mb();
		pr_debug("prefill: count %u  audio_buf[%u].size %u\n",
			 count, dma_buf_index, ab->size);

		ab->used = 1;
		dma_buf_index++;
		rc =  count;
		goto end;
	}

	if (lpa_if->config != 1) {
		pr_err("AUDIO_START did not happen\n");
		rc = 0;
		goto end;
	}

	while (count > 0) {

		ab = lpa_if->audio_buf + lpa_if->cpu_buf;

		rc = wait_event_timeout(lpa_if->wait, (ab->used == 0), 10 * HZ);
		if (!rc) {
			pr_err("wait_event_timeout failed\n");
			rc =  buf - start;
			goto end;
		}

		xfer = count;

		if (xfer > lpa_if->dma_period_sz)
			xfer = lpa_if->dma_period_sz;

		if (copy_from_user(ab->data, buf, xfer)) {
			pr_err("copy from user failed\n");
			rc = buf - start;
			goto end;
		}

		mb();
		buf += xfer;
		count -= xfer;
		ab->used = 1;

		pr_debug("xfer %d, size %d, used %d cpu_buf %d\n",
			xfer, ab->size, ab->used, lpa_if->cpu_buf);
		lpa_if->cpu_buf++;
		lpa_if->cpu_buf = lpa_if->cpu_buf % lpa_if->cfg.buffer_count;
	}
	rc = buf - start;
end:
	mutex_unlock(&lpa_if->lock);
	/* restore old scheduling policy */
	if (!rt_policy(old_policy)) {
		struct sched_param v = { .sched_priority = old_prio };
		if ((sched_setscheduler(current, old_policy, &v)) < 0)
			pr_err("sched_setscheduler failed\n");
		if (likely(!cap_nice)) {
			struct cred *new = prepare_creds();
			cap_lower(new->cap_effective, CAP_SYS_NICE);
			commit_creds(new);
		}
	}
	return rc;
}

static int lpa_if_release(struct inode *inode, struct file *file)
{
	struct lpa_if *lpa_if = file->private_data;

	hdmi_audio_packet_enable(0);

	wait_for_dma_cnt_stop(lpa_if->dma_ch);

	hdmi_audio_enable(0, HDMI_AUDIO_FIFO_WATER_MARK);

	if (lpa_if->config) {
		unregister_dma_irq_handler(lpa_if->dma_ch);
		dai_stop_hdmi(lpa_if->dma_ch);
		lpa_if->config = 0;
	}
	core_req_bus_bandwith(AUDIO_IF_BUS_ID, 0, 0);

	if (hdmi_msm_audio_get_sample_rate() != HDMI_SAMPLE_RATE_48KHZ)
		hdmi_msm_audio_sample_rate_reset(HDMI_SAMPLE_RATE_48KHZ);

	return 0;
}

static const struct file_operations lpa_if_fops = {
	.owner = THIS_MODULE,
	.open = lpa_if_open,
	.write = lpa_if_write,
	.release = lpa_if_release,
	.unlocked_ioctl = lpa_if_ioctl,
};

struct miscdevice lpa_if_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_lpa_if_out",
	.fops = &lpa_if_fops,
};

static int __init lpa_if_init(void)
{
	int rc;

	lpa_if_ptr = kzalloc(sizeof(struct lpa_if), GFP_KERNEL);
	if (!lpa_if_ptr) {
		pr_info("No mem for lpa-if\n");
		return -ENOMEM;
	}

	mutex_init(&lpa_if_ptr->lock);
	init_waitqueue_head(&lpa_if_ptr->wait);

	lpa_if_ptr->buffer = dma_alloc_coherent(NULL, DMA_ALLOC_BUF_SZ,
				    &(lpa_if_ptr->buffer_phys), GFP_KERNEL);
	if (!lpa_if_ptr->buffer) {
		pr_err("dma_alloc_coherent failed\n");
		kfree(lpa_if_ptr);
		return -ENOMEM;
	}

	pr_info("lpa_if_ptr 0x%08x   buf_vir 0x%08x   buf_phy 0x%08x "
		" buf_zise %u\n", (u32)lpa_if_ptr,
		(u32)(lpa_if_ptr->buffer), lpa_if_ptr->buffer_phys,
		DMA_ALLOC_BUF_SZ);

	rc =  misc_register(&lpa_if_misc);
	if (rc < 0) {
		pr_err("misc_register failed\n");

		dma_free_coherent(NULL, DMA_ALLOC_BUF_SZ, lpa_if_ptr->buffer,
				lpa_if_ptr->buffer_phys);
		kfree(lpa_if_ptr);
	}
	return rc;
}

device_initcall(lpa_if_init);

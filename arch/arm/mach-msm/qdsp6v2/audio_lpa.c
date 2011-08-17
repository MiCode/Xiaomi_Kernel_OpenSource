/* low power audio output device
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/list.h>
#include <linux/android_pmem.h>
#include <linux/slab.h>
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include <mach/msm_adsp.h>
#include <sound/q6asm.h>
#include <sound/apr_audio.h>
#include "audio_lpa.h"

#include <linux/msm_audio.h>
#include <linux/wakelock.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>

#include <mach/debug_mm.h>
#include <linux/fs.h>

#define MAX_BUF 3
#define BUFSZ (524288)

#define AUDDEC_DEC_PCM 0

#define AUDLPA_EVENT_NUM 10 /* Default number of pre-allocated event packets */

#define __CONTAINS(r, v, l) ({					\
	typeof(r) __r = r;					\
	typeof(v) __v = v;					\
	typeof(v) __e = __v + l;				\
	int res = ((__v >= __r->vaddr) &&			\
		(__e <= __r->vaddr + __r->len));		\
	res;							\
})

#define CONTAINS(r1, r2) ({					\
	typeof(r2) __r2 = r2;					\
	__CONTAINS(r1, __r2->vaddr, __r2->len);			\
})

#define IN_RANGE(r, v) ({					\
	typeof(r) __r = r;					\
	typeof(v) __vv = v;					\
	int res = ((__vv >= __r->vaddr) &&			\
		(__vv < (__r->vaddr + __r->len)));		\
	res;							\
})

#define OVERLAPS(r1, r2) ({					\
	typeof(r1) __r1 = r1;					\
	typeof(r2) __r2 = r2;					\
	typeof(__r2->vaddr) __v = __r2->vaddr;			\
	typeof(__v) __e = __v + __r2->len - 1;			\
	int res = (IN_RANGE(__r1, __v) || IN_RANGE(__r1, __e));	\
	res;							\
})

struct audlpa_event {
	struct list_head list;
	int event_type;
	union msm_audio_event_payload payload;
};

struct audlpa_pmem_region {
	struct list_head list;
	struct file *file;
	int fd;
	void *vaddr;
	unsigned long paddr;
	unsigned long kvaddr;
	unsigned long len;
	unsigned ref_cnt;
};

struct audlpa_buffer_node {
	struct list_head list;
	struct msm_audio_aio_buf buf;
	unsigned long paddr;
};

struct audlpa_dec {
	char *name;
	int dec_attrb;
	long (*ioctl)(struct file *, unsigned int, unsigned long);
	int (*set_params)(void *);
};

static void audlpa_post_event(struct audio *audio, int type,
	union msm_audio_event_payload payload);
static unsigned long audlpa_pmem_fixup(struct audio *audio, void *addr,
				unsigned long len, int ref_up);
static void audlpa_async_send_data(struct audio *audio, unsigned needed,
				uint32_t token);
static int audlpa_pause(struct audio *audio);
static void audlpa_unmap_pmem_region(struct audio *audio);
static long pcm_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int audlpa_set_pcm_params(void *data);

struct audlpa_dec audlpa_decs[] = {
	{"msm_pcm_lp_dec", AUDDEC_DEC_PCM, &pcm_ioctl,
		&audlpa_set_pcm_params},
};

static void lpa_listner(u32 evt_id, union auddev_evt_data *evt_payload,
			void *private_data)
{
	struct audio *audio = (struct audio *) private_data;
	int rc  = 0;

	switch (evt_id) {
	case AUDDEV_EVT_STREAM_VOL_CHG:
		audio->volume = evt_payload->session_vol;
		pr_debug("%s: AUDDEV_EVT_STREAM_VOL_CHG, stream vol %d, "
				 "enabled = %d\n", __func__, audio->volume,
				 audio->out_enabled);
		if (audio->out_enabled == 1) {
			if (audio->ac) {
				rc = q6asm_set_volume(audio->ac, audio->volume);
				if (rc < 0) {
					pr_err("%s: Send Volume command failed"
						" rc=%d\n", __func__, rc);
				}
			}
		}
		break;
	default:
		pr_err("%s:ERROR:wrong event\n", __func__);
		break;
	}
}

static void audlpa_prevent_sleep(struct audio *audio)
{
	pr_debug("%s:\n", __func__);
	wake_lock(&audio->wakelock);
}

static void audlpa_allow_sleep(struct audio *audio)
{
	pr_debug("%s:\n", __func__);
	wake_unlock(&audio->wakelock);
}

/* must be called with audio->lock held */
static int audio_enable(struct audio *audio)
{
	pr_debug("%s\n", __func__);

	return q6asm_run(audio->ac, 0, 0, 0);

}

static void audlpa_async_flush(struct audio *audio)
{
	struct audlpa_buffer_node *buf_node;
	struct list_head *ptr, *next;
	union msm_audio_event_payload payload;
	int rc = 0;

	pr_debug("%s:out_enabled = %d, drv_status = 0x%x\n", __func__,
			audio->out_enabled, audio->drv_status);
	if (audio->out_enabled) {
		list_for_each_safe(ptr, next, &audio->out_queue) {
			buf_node = list_entry(ptr, struct audlpa_buffer_node,
						list);
			list_del(&buf_node->list);
			payload.aio_buf = buf_node->buf;
				audlpa_post_event(audio, AUDIO_EVENT_WRITE_DONE,
								  payload);
				kfree(buf_node);
		}
		/* Implicitly issue a pause to the decoder before flushing if
		   it is not in pause state */
		if (!(audio->drv_status & ADRV_STATUS_PAUSE)) {
			rc = audlpa_pause(audio);
			if (rc < 0)
				pr_err("%s: pause cmd failed rc=%d\n", __func__,
					rc);
		}

		rc = q6asm_cmd(audio->ac, CMD_FLUSH);
		if (rc < 0)
			pr_err("%s: flush cmd failed rc=%d\n", __func__, rc);

		audio->drv_status &= ~ADRV_STATUS_OBUF_GIVEN;
		audio->out_needed = 0;

		if (audio->stopped == 0) {
			rc = audio_enable(audio);
			if (rc < 0)
				pr_err("%s: audio enable failed\n", __func__);
			else {
				audio->out_enabled = 1;
				audio->out_needed = 1;
				if (audio->drv_status & ADRV_STATUS_PAUSE)
					audio->drv_status &= ~ADRV_STATUS_PAUSE;
			}
		}
		wake_up(&audio->write_wait);
	}
}

/* must be called with audio->lock held */
static int audio_disable(struct audio *audio)
{
	int rc = 0;

	pr_debug("%s:%d %d\n", __func__, audio->opened, audio->out_enabled);

	if (audio->opened) {
		audio->out_enabled = 0;
		audio->opened = 0;
		rc = q6asm_cmd(audio->ac, CMD_CLOSE);
		if (rc < 0)
			pr_err("%s: CLOSE cmd failed\n", __func__);
		else
			pr_debug("%s: rxed CLOSE resp\n", __func__);
		audio->drv_status &= ~ADRV_STATUS_OBUF_GIVEN;
		wake_up(&audio->write_wait);
		audio->out_needed = 0;
	}
	return rc;
}
static int audlpa_pause(struct audio *audio)
{
	int rc = 0;

	pr_debug("%s, enabled = %d\n", __func__,
			audio->out_enabled);
	if (audio->out_enabled) {
		rc = q6asm_cmd(audio->ac, CMD_PAUSE);
		if (rc < 0)
			pr_err("%s: pause cmd failed rc=%d\n", __func__, rc);

	} else
		pr_err("%s: Driver not enabled\n", __func__);
	return rc;
}

/* ------------------- dsp --------------------- */
static void audlpa_async_send_data(struct audio *audio, unsigned needed,
				uint32_t token)
{
	unsigned long flags;
	struct audio_client *ac;
	int rc = 0;

	pr_debug("%s:\n", __func__);
	spin_lock_irqsave(&audio->dsp_lock, flags);

	pr_debug("%s: needed = %d, out_needed = %d, token = 0x%x\n",
			  __func__, needed, audio->out_needed, token);
	if (needed && !audio->wflush) {
		audio->out_needed = 1;
		if (audio->drv_status & ADRV_STATUS_OBUF_GIVEN) {
			/* pop one node out of queue */
			union msm_audio_event_payload evt_payload;
			struct audlpa_buffer_node *used_buf;

			used_buf = list_first_entry(&audio->out_queue,
				struct audlpa_buffer_node, list);
			if (token == used_buf->paddr) {
				pr_debug("%s, Release: addr: %lx,"
					" token = 0x%x\n", __func__,
					used_buf->paddr, token);
				list_del(&used_buf->list);
				evt_payload.aio_buf = used_buf->buf;
				audlpa_post_event(audio, AUDIO_EVENT_WRITE_DONE,
								  evt_payload);
				kfree(used_buf);
				audio->drv_status &= ~ADRV_STATUS_OBUF_GIVEN;
			}
		}
	}
	pr_debug("%s: out_needed = %d, stopped = %d, drv_status = 0x%x\n",
			 __func__, audio->out_needed, audio->stopped,
			 audio->drv_status);
	if (audio->out_needed && (audio->stopped == 0)) {
		struct audlpa_buffer_node *next_buf;
		struct audio_aio_write_param param;
		if (!list_empty(&audio->out_queue)) {
			pr_debug("%s: list not empty\n", __func__);
			next_buf = list_first_entry(&audio->out_queue,
					struct audlpa_buffer_node, list);
			if (next_buf) {
				pr_debug("%s: Send: addr: %lx\n", __func__,
						 next_buf->paddr);
				ac = audio->ac;
				param.paddr = next_buf->paddr;
				param.len = next_buf->buf.data_len;
				param.msw_ts = 0;
				param.lsw_ts = 0;
				/* No time stamp valid */
				param.flags = NO_TIMESTAMP;
				param.uid = next_buf->paddr;
				rc = q6asm_async_write(ac, &param);
				if (rc < 0)
					pr_err("%s:q6asm_async_write failed\n",
						__func__);
				audio->out_needed = 0;
				audio->drv_status |= ADRV_STATUS_OBUF_GIVEN;
			}
		} else if (list_empty(&audio->out_queue) &&
				   (audio->drv_status & ADRV_STATUS_FSYNC)) {
			pr_debug("%s: list is empty, reached EOS\n", __func__);
			wake_up(&audio->write_wait);
		}
	}

	spin_unlock_irqrestore(&audio->dsp_lock, flags);
}

static int audlpa_events_pending(struct audio *audio)
{
	int empty;

	spin_lock(&audio->event_queue_lock);
	empty = !list_empty(&audio->event_queue);
	spin_unlock(&audio->event_queue_lock);
	return empty || audio->event_abort;
}

static void audlpa_reset_event_queue(struct audio *audio)
{
	struct audlpa_event *drv_evt;
	struct list_head *ptr, *next;

	spin_lock(&audio->event_queue_lock);
	list_for_each_safe(ptr, next, &audio->event_queue) {
		drv_evt = list_first_entry(&audio->event_queue,
			struct audlpa_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	list_for_each_safe(ptr, next, &audio->free_event_queue) {
		drv_evt = list_first_entry(&audio->free_event_queue,
			struct audlpa_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	spin_unlock(&audio->event_queue_lock);

	return;
}

static long audlpa_process_event_req(struct audio *audio, void __user *arg)
{
	long rc;
	struct msm_audio_event usr_evt;
	struct audlpa_event *drv_evt = NULL;
	int timeout;

	if (copy_from_user(&usr_evt, arg, sizeof(struct msm_audio_event)))
		return -EFAULT;

	timeout = (int) usr_evt.timeout_ms;

	if (timeout > 0) {
		rc = wait_event_interruptible_timeout(
			audio->event_wait, audlpa_events_pending(audio),
			msecs_to_jiffies(timeout));
		if (rc == 0)
			return -ETIMEDOUT;
	} else {
		rc = wait_event_interruptible(
			audio->event_wait, audlpa_events_pending(audio));
	}

	if (rc < 0)
		return rc;

	if (audio->event_abort) {
		audio->event_abort = 0;
		return -ENODEV;
	}

	rc = 0;

	spin_lock(&audio->event_queue_lock);
	if (!list_empty(&audio->event_queue)) {
		drv_evt = list_first_entry(&audio->event_queue,
			struct audlpa_event, list);
		list_del(&drv_evt->list);
	}
	if (drv_evt) {
		usr_evt.event_type = drv_evt->event_type;
		usr_evt.event_payload = drv_evt->payload;
		list_add_tail(&drv_evt->list, &audio->free_event_queue);
	} else
		rc = -1;
	spin_unlock(&audio->event_queue_lock);

	if (drv_evt->event_type == AUDIO_EVENT_WRITE_DONE ||
	    drv_evt->event_type == AUDIO_EVENT_READ_DONE) {
		pr_debug("%s: AUDIO_EVENT_WRITE_DONE completing\n", __func__);
		mutex_lock(&audio->lock);
		audlpa_pmem_fixup(audio, drv_evt->payload.aio_buf.buf_addr,
				  drv_evt->payload.aio_buf.buf_len, 0);
		mutex_unlock(&audio->lock);
	}
	if (!rc && copy_to_user(arg, &usr_evt, sizeof(usr_evt)))
		rc = -EFAULT;

	return rc;
}

static int audlpa_pmem_check(struct audio *audio,
		void *vaddr, unsigned long len)
{
	struct audlpa_pmem_region *region_elt;
	struct audlpa_pmem_region t = { .vaddr = vaddr, .len = len };

	list_for_each_entry(region_elt, &audio->pmem_region_queue, list) {
		if (CONTAINS(region_elt, &t) || CONTAINS(&t, region_elt) ||
		    OVERLAPS(region_elt, &t)) {
			pr_err("%s: region (vaddr %p len %ld)"
				" clashes with registered region"
				" (vaddr %p paddr %p len %ld)\n",
				__func__, vaddr, len,
				region_elt->vaddr,
				(void *)region_elt->paddr,
				region_elt->len);
			return -EINVAL;
		}
	}

	return 0;
}

static int audlpa_pmem_add(struct audio *audio,
	struct msm_audio_pmem_info *info)
{
	unsigned long paddr, kvaddr, len;
	struct file *file;
	struct audlpa_pmem_region *region;
	int rc = -EINVAL;

	pr_debug("%s:\n", __func__);
	region = kmalloc(sizeof(*region), GFP_KERNEL);

	if (!region) {
		rc = -ENOMEM;
		goto end;
	}

	if (get_pmem_file(info->fd, &paddr, &kvaddr, &len, &file)) {
		kfree(region);
		goto end;
	}

	rc = audlpa_pmem_check(audio, info->vaddr, len);
	if (rc < 0) {
		put_pmem_file(file);
		kfree(region);
		goto end;
	}

	region->vaddr = info->vaddr;
	region->fd = info->fd;
	region->paddr = paddr;
	region->kvaddr = kvaddr;
	region->len = len;
	region->file = file;
	region->ref_cnt = 0;
	pr_debug("%s: add region paddr %lx vaddr %p, len %lu\n", __func__,
			 region->paddr, region->vaddr,
			 region->len);
	list_add_tail(&region->list, &audio->pmem_region_queue);
	rc = q6asm_memory_map(audio->ac, (uint32_t)paddr, IN, (uint32_t)len, 1);
	if (rc < 0)
		pr_err("%s: memory map failed\n", __func__);
end:
	return rc;
}

static int audlpa_pmem_remove(struct audio *audio,
	struct msm_audio_pmem_info *info)
{
	struct audlpa_pmem_region *region;
	struct list_head *ptr, *next;
	int rc = -EINVAL;

	list_for_each_safe(ptr, next, &audio->pmem_region_queue) {
		region = list_entry(ptr, struct audlpa_pmem_region, list);

		if ((region != NULL) && (region->fd == info->fd) &&
		    (region->vaddr == info->vaddr)) {
			if (region->ref_cnt) {
				pr_debug("%s: region %p in use ref_cnt %d\n",
					__func__, region, region->ref_cnt);
				break;
			}
			rc = q6asm_memory_unmap(audio->ac,
						(uint32_t)region->paddr,
						IN);
			if (rc < 0)
				pr_err("%s: memory unmap failed\n", __func__);

			list_del(&region->list);
			put_pmem_file(region->file);
			kfree(region);
			rc = 0;
			break;
		}
	}

	return rc;
}

static int audlpa_pmem_lookup_vaddr(struct audio *audio, void *addr,
		     unsigned long len, struct audlpa_pmem_region **region)
{
	struct audlpa_pmem_region *region_elt;

	int match_count = 0;

	*region = NULL;

	/* returns physical address or zero */
	list_for_each_entry(region_elt, &audio->pmem_region_queue,
		list) {
		if (addr >= region_elt->vaddr &&
		    addr < region_elt->vaddr + region_elt->len &&
		    addr + len <= region_elt->vaddr + region_elt->len) {
			/* offset since we could pass vaddr inside a registerd
			 * pmem buffer
			 */

			match_count++;
			if (!*region)
				*region = region_elt;
		}
	}

	if (match_count > 1) {
		pr_err("%s: multiple hits for vaddr %p, len %ld\n", __func__,
			   addr, len);
		list_for_each_entry(region_elt,
		  &audio->pmem_region_queue, list) {
			if (addr >= region_elt->vaddr &&
			    addr < region_elt->vaddr + region_elt->len &&
			    addr + len <= region_elt->vaddr + region_elt->len)
				pr_err("%s: \t%p, %ld --> %p\n", __func__,
					   region_elt->vaddr, region_elt->len,
					   (void *)region_elt->paddr);
		}
	}

	return *region ? 0 : -1;
}

unsigned long audlpa_pmem_fixup(struct audio *audio, void *addr,
		    unsigned long len, int ref_up)
{
	struct audlpa_pmem_region *region;
	unsigned long paddr;
	int ret;

	ret = audlpa_pmem_lookup_vaddr(audio, addr, len, &region);
	if (ret) {
		pr_err("%s: lookup (%p, %ld) failed\n", __func__, addr, len);
		return 0;
	}
	if (ref_up)
		region->ref_cnt++;
	else
		region->ref_cnt--;
	paddr = region->paddr + (addr - region->vaddr);
	return paddr;
}

/* audio -> lock must be held at this point */
static int audlpa_aio_buf_add(struct audio *audio, unsigned dir,
	void __user *arg)
{
	struct audlpa_buffer_node *buf_node;

	buf_node = kmalloc(sizeof(*buf_node), GFP_KERNEL);

	if (!buf_node)
		return -ENOMEM;

	if (copy_from_user(&buf_node->buf, arg, sizeof(buf_node->buf))) {
		kfree(buf_node);
		return -EFAULT;
	}

	buf_node->paddr = audlpa_pmem_fixup(
		audio, buf_node->buf.buf_addr,
		buf_node->buf.buf_len, 1);
	if (dir) {
		/* write */
		if (!buf_node->paddr ||
		    (buf_node->paddr & 0x1) ||
		    (buf_node->buf.data_len & 0x1)) {
			kfree(buf_node);
			return -EINVAL;
		}
		list_add_tail(&buf_node->list, &audio->out_queue);
		pr_debug("%s, Added to list: addr: %lx, length = %d\n",
			__func__, buf_node->paddr, buf_node->buf.data_len);
		audlpa_async_send_data(audio, 0, 0);
	} else {
		/* read */
	}
	return 0;
}

static int config(struct audio *audio)
{
	int rc = 0;
	if (!audio->out_prefill) {
		if (audio->codec_ops.set_params != NULL) {
			rc = audio->codec_ops.set_params(audio);
			audio->out_prefill = 1;
		}
	}
	return rc;
}

void q6_audlpa_out_cb(uint32_t opcode, uint32_t token,
			uint32_t *payload, void *priv)
{
	struct audio *audio = (struct audio *) priv;

	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE:
		pr_debug("%s: ASM_DATA_EVENT_WRITE_DONE, token = 0x%x\n",
				 __func__, token);
		audlpa_async_send_data(audio, 1, token);
		break;
	case ASM_DATA_EVENT_EOS:
	case ASM_DATA_CMDRSP_EOS:
		pr_debug("%s: ASM_DATA_CMDRSP_EOS, teos = %d\n", __func__,
				 audio->teos);
		if (audio->teos == 0) {
			audio->teos = 1;
			wake_up(&audio->write_wait);
		}
		break;
	case ASM_SESSION_CMDRSP_GET_SESSION_TIME:
		break;
	default:
		break;
	}
}

static long pcm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	pr_debug("%s: cmd = %d\n", __func__, cmd);
	return -EINVAL;
}

static int audlpa_set_pcm_params(void *data)
{
	struct audio *audio = (struct audio *)data;
	int rc;

	rc = q6asm_media_format_block_pcm(audio->ac, audio->out_sample_rate,
					audio->out_channel_mode);
	if (rc < 0)
		pr_err("%s: Format block pcm failed\n", __func__);
	return rc;
}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = -EINVAL;
	uint64_t timestamp;
	uint64_t temp;

	pr_debug("%s: audio_ioctl() cmd = %d\n", __func__, cmd);

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;

		pr_debug("%s: AUDIO_GET_STATS cmd\n", __func__);
		memset(&stats, 0, sizeof(stats));
		timestamp = q6asm_get_session_time(audio->ac);
		if (timestamp < 0) {
			pr_err("%s: Get Session Time return value =%lld\n",
				__func__, timestamp);
			return -EAGAIN;
		}
		temp = (timestamp * 2 * audio->out_channel_mode);
		temp = temp * (audio->out_sample_rate/1000);
		temp = div_u64(temp, 1000);
		audio->bytes_consumed = (uint32_t)(temp & 0xFFFFFFFF);
		stats.byte_count = audio->bytes_consumed;
		stats.unused[0]  = (uint32_t)((temp >> 32) & 0xFFFFFFFF);
		pr_debug("%s: bytes_consumed:lsb = %d, msb = %d,"
			"timestamp = %lld\n", __func__,
			audio->bytes_consumed, stats.unused[0], timestamp);
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
				return -EFAULT;
		return 0;
	}

	switch (cmd) {
	case AUDIO_ENABLE_AUDPP:
		break;

	case AUDIO_SET_VOLUME:
		break;

	case AUDIO_SET_PAN:
		break;

	case AUDIO_SET_EQ:
		break;
	}

	if (cmd == AUDIO_GET_EVENT) {
		pr_debug("%s: AUDIO_GET_EVENT\n", __func__);
		if (mutex_trylock(&audio->get_event_lock)) {
			rc = audlpa_process_event_req(audio,
				(void __user *) arg);
			mutex_unlock(&audio->get_event_lock);
		} else
			rc = -EBUSY;
		return rc;
	}

	if (cmd == AUDIO_ABORT_GET_EVENT) {
		audio->event_abort = 1;
		wake_up(&audio->event_wait);
		return 0;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START:
		pr_info("%s: AUDIO_START: Session %d\n", __func__,
			audio->ac->session);
		if (!audio->opened) {
			pr_err("%s: Driver not opened\n", __func__);
			rc = -EFAULT;
			goto fail;
		}
		rc = config(audio);
		if (rc) {
			pr_err("%s: Out Configuration failed\n", __func__);
			rc = -EFAULT;
			goto fail;
		}

		rc = audio_enable(audio);
		if (rc) {
			pr_err("%s: audio enable failed\n", __func__);
			rc = -EFAULT;
			goto fail;
		} else {
			struct asm_softpause_params param = {
				.enable = SOFT_PAUSE_ENABLE,
				.period = SOFT_PAUSE_PERIOD,
				.step = SOFT_PAUSE_STEP,
				.rampingcurve = SOFT_PAUSE_CURVE_LINEAR,
			};
			audio->out_enabled = 1;
			audio->out_needed = 1;
			rc = q6asm_set_volume(audio->ac, audio->volume);
			if (rc < 0)
				pr_err("%s: Send Volume command failed rc=%d\n",
					__func__, rc);
			rc = q6asm_set_softpause(audio->ac, &param);
			if (rc < 0)
				pr_err("%s: Send SoftPause Param failed rc=%d\n",
					__func__, rc);
			rc = q6asm_set_lrgain(audio->ac, 0x2000, 0x2000);
			if (rc < 0)
				pr_err("%s: Send channel gain failed rc=%d\n",
					__func__, rc);
			/* disable mute by default */
			rc = q6asm_set_mute(audio->ac, 0);
			if (rc < 0)
				pr_err("%s: Send mute command failed rc=%d\n",
					__func__, rc);
			if (!list_empty(&audio->out_queue))
				pr_err("%s: write_list is not empty!!!\n",
					__func__);
			if (audio->stopped == 1)
				audio->stopped = 0;
			audlpa_prevent_sleep(audio);
		}
		break;

	case AUDIO_STOP:
		pr_info("%s: AUDIO_STOP: session_id:%d\n", __func__,
			audio->ac->session);
		audio->stopped = 1;
		audlpa_async_flush(audio);
		audio->out_enabled = 0;
		audio->out_needed = 0;
		audio->drv_status &= ~ADRV_STATUS_PAUSE;
		audlpa_allow_sleep(audio);
		break;

	case AUDIO_FLUSH:
		pr_debug("%s: AUDIO_FLUSH: session_id:%d\n", __func__,
			audio->ac->session);
		audio->wflush = 1;
		if (audio->out_enabled)
			audlpa_async_flush(audio);
		else
			audio->wflush = 0;
		audio->wflush = 0;
		break;

	case AUDIO_SET_CONFIG:{
		struct msm_audio_config config;
		pr_debug("%s: AUDIO_SET_CONFIG\n", __func__);
		if (copy_from_user(&config, (void *) arg, sizeof(config))) {
			rc = -EFAULT;
			pr_err("%s: ERROR: copy from user\n", __func__);
			break;
		}
		if (!((config.channel_count == 1) ||
			(config.channel_count == 2))) {
			rc = -EINVAL;
			pr_err("%s: ERROR: config.channel_count == %d\n",
				__func__, config.channel_count);
			break;
		}

		if (!((config.bits == 8) || (config.bits == 16) ||
			  (config.bits == 24))) {
			rc = -EINVAL;
			pr_err("%s: ERROR: config.bits = %d\n", __func__,
				config.bits);
			break;
		}
		audio->out_sample_rate = config.sample_rate;
		audio->out_channel_mode = config.channel_count;
		audio->out_bits = config.bits;
		audio->buffer_count = config.buffer_count;
		audio->buffer_size = config.buffer_size;
		rc = 0;
		break;
	}

	case AUDIO_GET_CONFIG:{
		struct msm_audio_config config;
		config.buffer_count = audio->buffer_count;
		config.buffer_size = audio->buffer_size;
		config.sample_rate = audio->out_sample_rate;
		config.channel_count = audio->out_channel_mode;
		config.bits = audio->out_bits;

		config.meta_field = 0;
		config.unused[0] = 0;
		config.unused[1] = 0;
		config.unused[2] = 0;
		if (copy_to_user((void *) arg, &config, sizeof(config)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}

	case AUDIO_PAUSE:
		pr_debug("%s: AUDIO_PAUSE %ld\n", __func__, arg);
		if (arg == 1) {
			rc = audlpa_pause(audio);
			if (rc < 0)
				pr_err("%s: pause FAILED rc=%d\n", __func__,
					rc);
			audio->drv_status |= ADRV_STATUS_PAUSE;
		} else if (arg == 0) {
			if (audio->drv_status & ADRV_STATUS_PAUSE) {
				rc = audio_enable(audio);
				if (rc)
					pr_err("%s: audio enable failed\n",
						__func__);
				else {
					audio->drv_status &= ~ADRV_STATUS_PAUSE;
					audio->out_enabled = 1;
				}
			}
		}
		break;

	case AUDIO_REGISTER_PMEM: {
			struct msm_audio_pmem_info info;
			pr_debug("%s: AUDIO_REGISTER_PMEM\n", __func__);
			if (copy_from_user(&info, (void *) arg, sizeof(info)))
				rc = -EFAULT;
			else
				rc = audlpa_pmem_add(audio, &info);
			break;
		}

	case AUDIO_DEREGISTER_PMEM: {
			struct msm_audio_pmem_info info;
			pr_debug("%s: AUDIO_DEREGISTER_PMEM\n", __func__);
			if (copy_from_user(&info, (void *) arg, sizeof(info)))
				rc = -EFAULT;
			else
				rc = audlpa_pmem_remove(audio, &info);
			break;
		}
	case AUDIO_ASYNC_WRITE:
		pr_debug("%s: AUDIO_ASYNC_WRITE\n", __func__);
		if (audio->drv_status & ADRV_STATUS_FSYNC)
			rc = -EBUSY;
		else
			rc = audlpa_aio_buf_add(audio, 1, (void __user *) arg);
		break;

	case AUDIO_GET_SESSION_ID:
		if (copy_to_user((void *) arg, &audio->ac->session,
					sizeof(unsigned short)))
			return -EFAULT;
		rc = 0;
		break;

	default:
		rc = audio->codec_ops.ioctl(file, cmd, arg);
	}
fail:
	mutex_unlock(&audio->lock);
	return rc;
}

/* Only useful in tunnel-mode */
int audlpa_async_fsync(struct audio *audio)
{
	int rc = 0;

	pr_info("%s:Session %d\n", __func__, audio->ac->session);

	/* Blocking client sends more data */
	mutex_lock(&audio->lock);
	audio->drv_status |= ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	mutex_lock(&audio->write_lock);
	audio->teos = 0;

	rc = wait_event_interruptible(audio->write_wait,
					((list_empty(&audio->out_queue)) ||
					audio->wflush || audio->stopped));

	if (audio->wflush || audio->stopped)
		goto flush_event;

	if (rc < 0) {
		pr_err("%s: wait event for list_empty failed, rc = %d\n",
			__func__, rc);
		goto done;
	}

	rc = q6asm_cmd(audio->ac, CMD_EOS);

	if (rc < 0) {
		pr_err("%s: q6asm_cmd failed, rc = %d", __func__, rc);
		goto done;
	}
	rc = wait_event_interruptible_timeout(audio->write_wait,
				  (audio->teos || audio->wflush ||
				  audio->stopped), 5*HZ);

	if (rc < 0) {
		pr_err("%s: wait event for teos failed, rc = %d\n", __func__,
			rc);
		goto done;
	}

	if (audio->teos == 1) {
		rc = audio_enable(audio);
		if (rc)
			pr_err("%s: audio enable failed\n", __func__);
		else {
			audio->drv_status &= ~ADRV_STATUS_PAUSE;
			audio->out_enabled = 1;
			audio->out_needed = 1;
		}
	}

flush_event:
	if (audio->stopped || audio->wflush)
		rc = -EBUSY;

done:
	mutex_unlock(&audio->write_lock);
	mutex_lock(&audio->lock);
	audio->drv_status &= ~ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	return rc;
}

int audlpa_fsync(struct file *file, int datasync)
{
	struct audio *audio = file->private_data;

	return audlpa_async_fsync(audio);
}

static void audlpa_reset_pmem_region(struct audio *audio)
{
	struct audlpa_pmem_region *region;
	struct list_head *ptr, *next;

	list_for_each_safe(ptr, next, &audio->pmem_region_queue) {
		region = list_entry(ptr, struct audlpa_pmem_region, list);
		list_del(&region->list);
		put_pmem_file(region->file);
		kfree(region);
	}

	return;
}

static void audlpa_unmap_pmem_region(struct audio *audio)
{
	struct audlpa_pmem_region *region;
	struct list_head *ptr, *next;
	int rc = -EINVAL;

	pr_debug("%s:\n", __func__);
	list_for_each_safe(ptr, next, &audio->pmem_region_queue) {
		region = list_entry(ptr, struct audlpa_pmem_region, list);
		pr_debug("%s: phy_address = 0x%lx\n", __func__, region->paddr);
		if (region != NULL) {
			rc = q6asm_memory_unmap(audio->ac,
						(uint32_t)region->paddr, IN);
			if (rc < 0)
				pr_err("%s: memory unmap failed\n", __func__);
		}
	}
}

static int audio_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	pr_info("%s: audio instance 0x%08x freeing, session %d\n", __func__,
		(int)audio, audio->ac->session);

	mutex_lock(&audio->lock);
	audio->wflush = 1;
	if (audio->out_enabled)
		audlpa_async_flush(audio);
	audio->wflush = 0;
	audlpa_unmap_pmem_region(audio);
	audio_disable(audio);
	msm_clear_session_id(audio->ac->session);
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, audio->ac->session);
	q6asm_audio_client_free(audio->ac);
	audlpa_reset_pmem_region(audio);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&audio->suspend_ctl.node);
#endif
	audio->opened = 0;
	audio->out_enabled = 0;
	audio->out_prefill = 0;
	audio->event_abort = 1;
	wake_up(&audio->event_wait);
	audlpa_reset_event_queue(audio);
	pmem_kfree(audio->phys);
	if (audio->stopped == 0)
		audlpa_allow_sleep(audio);
	wake_lock_destroy(&audio->wakelock);

	mutex_unlock(&audio->lock);
#ifdef CONFIG_DEBUG_FS
	if (audio->dentry)
		debugfs_remove(audio->dentry);
#endif
	kfree(audio);
	return 0;
}

static void audlpa_post_event(struct audio *audio, int type,
	union msm_audio_event_payload payload)
{
	struct audlpa_event *e_node = NULL;

	spin_lock(&audio->event_queue_lock);

	pr_debug("%s:\n", __func__);
	if (!list_empty(&audio->free_event_queue)) {
		e_node = list_first_entry(&audio->free_event_queue,
			struct audlpa_event, list);
		list_del(&e_node->list);
	} else {
		e_node = kmalloc(sizeof(struct audlpa_event), GFP_ATOMIC);
		if (!e_node) {
			pr_err("%s: No mem to post event %d\n", __func__, type);
			return;
		}
	}

	e_node->event_type = type;
	e_node->payload = payload;

	list_add_tail(&e_node->list, &audio->event_queue);
	spin_unlock(&audio->event_queue_lock);
	wake_up(&audio->event_wait);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void audlpa_suspend(struct early_suspend *h)
{
	struct audlpa_suspend_ctl *ctl =
		container_of(h, struct audlpa_suspend_ctl, node);
	union msm_audio_event_payload payload;

	pr_debug("%s:\n", __func__);
	audlpa_post_event(ctl->audio, AUDIO_EVENT_SUSPEND, payload);
}

static void audlpa_resume(struct early_suspend *h)
{
	struct audlpa_suspend_ctl *ctl =
		container_of(h, struct audlpa_suspend_ctl, node);
	union msm_audio_event_payload payload;

	pr_debug("%s:\n", __func__);
	audlpa_post_event(ctl->audio, AUDIO_EVENT_RESUME, payload);
}
#endif

#ifdef CONFIG_DEBUG_FS
static ssize_t audlpa_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t audlpa_debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	const int debug_bufmax = 4096;
	static char buffer[4096];
	int n = 0;
	struct audio *audio = file->private_data;

	mutex_lock(&audio->lock);
	n = scnprintf(buffer, debug_bufmax, "opened %d\n", audio->opened);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"out_enabled %d\n", audio->out_enabled);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"stopped %d\n", audio->stopped);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"volume %x\n", audio->volume);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"sample rate %d\n",
					audio->out_sample_rate);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"channel mode %d\n",
					audio->out_channel_mode);
	mutex_unlock(&audio->lock);
	/* Following variables are only useful for debugging when
	 * when playback halts unexpectedly. Thus, no mutual exclusion
	 * enforced
	 */
	n += scnprintf(buffer + n, debug_bufmax - n,
					"wflush %d\n", audio->wflush);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"running %d\n", audio->running);
	n += scnprintf(buffer + n, debug_bufmax - n,
					"out_needed %d\n", audio->out_needed);
	buffer[n] = 0;
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static const struct file_operations audlpa_debug_fops = {
	.read = audlpa_debug_read,
	.open = audlpa_debug_open,
};
#endif

static int audio_open(struct inode *inode, struct file *file)
{
	struct audio *audio = NULL;
	int rc, i, dec_attrb = 0;
	struct audlpa_event *e_node = NULL;
#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_lpa_" + 5];
#endif
	char wake_lock_name[24];

	/* Allocate audio instance, set to zero */
	audio = kzalloc(sizeof(struct audio), GFP_KERNEL);
	if (!audio) {
		pr_err("%s: no memory to allocate audio instance\n", __func__);
		rc = -ENOMEM;
		goto done;
	}

	if ((file->f_mode & FMODE_WRITE) && !(file->f_mode & FMODE_READ)) {
		pr_debug("%s: Tunnel Mode playback\n", __func__);
	} else {
		kfree(audio);
		rc = -EACCES;
		goto done;
	}

	/* Allocate the decoder based on inode minor number*/
	audio->minor_no = iminor(inode);
	dec_attrb |= audlpa_decs[audio->minor_no].dec_attrb;
	audio->codec_ops.ioctl = audlpa_decs[audio->minor_no].ioctl;
	audio->codec_ops.set_params = audlpa_decs[audio->minor_no].set_params;
	audio->buffer_size = BUFSZ;
	audio->buffer_count = MAX_BUF;

	audio->ac = q6asm_audio_client_alloc((app_cb)q6_audlpa_out_cb,
					(void *)audio);
	if (!audio->ac) {
		pr_err("%s: Could not allocate memory for lpa client\n",
								__func__);
		rc = -ENOMEM;
		goto err;
	}
	rc = q6asm_open_write(audio->ac, FORMAT_LINEAR_PCM);
	if (rc < 0) {
		pr_err("%s: lpa out open failed\n", __func__);
		goto err;
	}

	pr_debug("%s: Set mode to AIO session[%d]\n",
						__func__,
						audio->ac->session);
	rc = q6asm_set_io_mode(audio->ac, ASYNC_IO_MODE);
	if (rc < 0)
		pr_err("%s: Set IO mode failed\n", __func__);


	/* Initialize all locks of audio instance */
	mutex_init(&audio->lock);
	mutex_init(&audio->write_lock);
	mutex_init(&audio->get_event_lock);
	spin_lock_init(&audio->dsp_lock);
	init_waitqueue_head(&audio->write_wait);
	INIT_LIST_HEAD(&audio->out_queue);
	INIT_LIST_HEAD(&audio->pmem_region_queue);
	INIT_LIST_HEAD(&audio->free_event_queue);
	INIT_LIST_HEAD(&audio->event_queue);
	init_waitqueue_head(&audio->wait);
	init_waitqueue_head(&audio->event_wait);
	spin_lock_init(&audio->event_queue_lock);
	snprintf(wake_lock_name, sizeof wake_lock_name, "audio_lpa_%x",
		audio->ac->session);
	wake_lock_init(&audio->wakelock, WAKE_LOCK_SUSPEND, wake_lock_name);

	audio->out_sample_rate = 44100;
	audio->out_channel_mode = 2;
	audio->out_bits = 16;
	audio->volume = 0x2000;

	file->private_data = audio;
	audio->opened = 1;
	audio->out_enabled = 0;
	audio->out_prefill = 0;
	audio->bytes_consumed = 0;

	audio->device_events = AUDDEV_EVT_STREAM_VOL_CHG;
	audio->drv_status &= ~ADRV_STATUS_PAUSE;

	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_DEC,
					audio->ac->session,
					lpa_listner,
					(void *)audio);
	if (rc) {
		pr_err("%s: failed to register listner\n", __func__);
		goto err;
	}

#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof name, "msm_lpa_%04x", audio->ac->session);
	audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
			NULL, (void *) audio, &audlpa_debug_fops);

	if (IS_ERR(audio->dentry))
		pr_err("%s: debugfs_create_file failed\n", __func__);
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	audio->suspend_ctl.node.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	audio->suspend_ctl.node.resume = audlpa_resume;
	audio->suspend_ctl.node.suspend = audlpa_suspend;
	audio->suspend_ctl.audio = audio;
	register_early_suspend(&audio->suspend_ctl.node);
#endif
	for (i = 0; i < AUDLPA_EVENT_NUM; i++) {
		e_node = kmalloc(sizeof(struct audlpa_event), GFP_KERNEL);
		if (e_node)
			list_add_tail(&e_node->list, &audio->free_event_queue);
		else {
			pr_err("%s: event pkt alloc failed\n", __func__);
			break;
		}
	}
	pr_info("%s: audio instance 0x%08x created session[%d]\n", __func__,
						(int)audio,
						audio->ac->session);
done:
	return rc;
err:
	q6asm_audio_client_free(audio->ac);
	pmem_kfree(audio->phys);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_lpa_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_open,
	.release	= audio_release,
	.unlocked_ioctl	= audio_ioctl,
	.fsync		= audlpa_fsync,
};

static dev_t audlpa_devno;
static struct class *audlpa_class;
struct audlpa_device {
	const char *name;
	struct device *device;
	struct cdev cdev;
};

static struct audlpa_device *audlpa_devices;

static void audlpa_create(struct audlpa_device *adev, const char *name,
			struct device *parent, dev_t devt)
{
	struct device *dev;
	int rc;

	dev = device_create(audlpa_class, parent, devt, "%s", name);
	if (IS_ERR(dev))
		return;

	cdev_init(&adev->cdev, &audio_lpa_fops);
	adev->cdev.owner = THIS_MODULE;

	rc = cdev_add(&adev->cdev, devt, 1);
	if (rc < 0) {
		device_destroy(audlpa_class, devt);
	} else {
		adev->device = dev;
		adev->name = name;
	}
}

static int __init audio_init(void)
{
	int rc;
	int n = ARRAY_SIZE(audlpa_decs);

	audlpa_devices = kzalloc(sizeof(struct audlpa_device) * n, GFP_KERNEL);
	if (!audlpa_devices)
		return -ENOMEM;

	audlpa_class = class_create(THIS_MODULE, "audlpa");
	if (IS_ERR(audlpa_class))
		goto fail_create_class;

	rc = alloc_chrdev_region(&audlpa_devno, 0, n, "msm_audio_lpa");
	if (rc < 0)
		goto fail_alloc_region;

	for (n = 0; n < ARRAY_SIZE(audlpa_decs); n++) {
		audlpa_create(audlpa_devices + n,
				audlpa_decs[n].name, NULL,
				MKDEV(MAJOR(audlpa_devno), n));
	}

	return 0;

fail_alloc_region:
	class_unregister(audlpa_class);
	return rc;
fail_create_class:
	kfree(audlpa_devices);
	return -ENOMEM;
}

static void __exit audio_exit(void)
{
	class_unregister(audlpa_class);
	kfree(audlpa_devices);
}

module_init(audio_init);
module_exit(audio_exit);

MODULE_DESCRIPTION("MSM LPA driver");
MODULE_LICENSE("GPL v2");

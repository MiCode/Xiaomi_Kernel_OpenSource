/* wmapro audio output device
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

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/msm_audio.h>
#include <linux/msm_audio_wmapro.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/android_pmem.h>
#include <linux/slab.h>
#include <asm/ioctls.h>
#include <asm/atomic.h>
#include <sound/q6asm.h>
#include <sound/apr_audio.h>

#define ADRV_STATUS_AIO_INTF 0x00000001	/* AIO interface */
#define ADRV_STATUS_FSYNC 0x00000008
#define ADRV_STATUS_PAUSE 0x00000010

#define TUNNEL_MODE     0x0000
#define NON_TUNNEL_MODE 0x0001
#define AUDWMAPRO_EOS_SET  0x00000001

/* Default number of pre-allocated event packets */
#define AUDWMAPRO_EVENT_NUM 10

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

struct timestamp {
	unsigned long lowpart;
	unsigned long highpart;
} __attribute__ ((packed));

struct meta_in {
	unsigned char reserved[18];
	unsigned short offset;
	struct timestamp ntimestamp;
	unsigned int nflags;
} __attribute__ ((packed));

struct meta_out_dsp{
	u32 offset_to_frame;
	u32 frame_size;
	u32 encoded_pcm_samples;
	u32 msw_ts;
	u32 lsw_ts;
	u32 nflags;
} __attribute__ ((packed));

struct dec_meta_out{
	unsigned int reserved[7];
	unsigned int num_of_frames;
	struct meta_out_dsp meta_out_dsp[];
} __attribute__ ((packed));

/* General meta field to store meta info
locally */
union  meta_data {
	struct dec_meta_out meta_out;
	struct meta_in meta_in;
} __attribute__ ((packed));

struct audwmapro_event {
	struct list_head list;
	int event_type;
	union msm_audio_event_payload payload;
};

struct audwmapro_pmem_region {
	struct list_head list;
	struct file *file;
	int fd;
	void *vaddr;
	unsigned long paddr;
	unsigned long kvaddr;
	unsigned long len;
	unsigned ref_cnt;
};

struct audwmapro_buffer_node {
	struct list_head list;
	struct msm_audio_aio_buf buf;
	unsigned long paddr;
	unsigned long token;
	void		*kvaddr;
	union meta_data meta_info;
};

struct q6audio;

struct audwmapro_drv_operations {
	void (*out_flush) (struct q6audio *);
	void (*in_flush) (struct q6audio *);
	int (*fsync)(struct q6audio *);
};

#define PCM_BUF_COUNT		(2)
/* Buffer with meta */
#define PCM_BUFSZ_MIN		((4*1024) + sizeof(struct dec_meta_out))

/* FRAME_NUM must be a power of two */
#define FRAME_NUM		(2)
#define FRAME_SIZE		((4*1024) + sizeof(struct meta_in))

struct q6audio {
	atomic_t in_bytes;
	atomic_t in_samples;

	struct msm_audio_stream_config str_cfg;
	struct msm_audio_buf_cfg        buf_cfg;
	struct msm_audio_config pcm_cfg;
	struct msm_audio_wmapro_config wmapro_config;

	struct audio_client *ac;

	struct mutex lock;
	struct mutex read_lock;
	struct mutex write_lock;
	struct mutex get_event_lock;
	wait_queue_head_t cmd_wait;
	wait_queue_head_t write_wait;
	wait_queue_head_t event_wait;
	spinlock_t dsp_lock;
	spinlock_t event_queue_lock;

#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif
	struct list_head out_queue;	/* queue to retain output buffers */
	struct list_head in_queue;	/* queue to retain input buffers */
	struct list_head free_event_queue;
	struct list_head event_queue;
	struct list_head pmem_region_queue;	/* protected by lock */
	struct audwmapro_drv_operations drv_ops;
	union msm_audio_event_payload eos_write_payload;

	uint32_t drv_status;
	int event_abort;
	int eos_rsp;
	int eos_flag;
	int opened;
	int enabled;
	int stopped;
	int feedback;
	int rflush;		/* Read  flush */
	int wflush;		/* Write flush */
};

static int insert_eos_buf(struct q6audio *audio,
	struct audwmapro_buffer_node *buf_node) {
	struct dec_meta_out *eos_buf = buf_node->kvaddr;
	eos_buf->num_of_frames = 0xFFFFFFFF;
	eos_buf->meta_out_dsp[0].offset_to_frame = 0x0;
	eos_buf->meta_out_dsp[0].nflags = AUDWMAPRO_EOS_SET;
	return sizeof(struct dec_meta_out) +
		sizeof(eos_buf->meta_out_dsp[0]);
}

/* Routine which updates read buffers of driver/dsp,
   for flush operation as DSP output might not have proper
   value set */
static int insert_meta_data(struct q6audio *audio,
	struct audwmapro_buffer_node *buf_node) {
	struct dec_meta_out *meta_data = buf_node->kvaddr;
	meta_data->num_of_frames = 0x0;
	meta_data->meta_out_dsp[0].offset_to_frame = 0x0;
	meta_data->meta_out_dsp[0].nflags = 0x0;
	return sizeof(struct dec_meta_out) +
		sizeof(meta_data->meta_out_dsp[0]);
}

static void extract_meta_info(struct q6audio *audio,
	struct audwmapro_buffer_node *buf_node, int dir)
{
	if (dir) { /* Read */
		if (audio->buf_cfg.meta_info_enable)
			memcpy(&buf_node->meta_info.meta_in,
			(char *)buf_node->kvaddr, sizeof(struct meta_in));
		else
			memset(&buf_node->meta_info.meta_in,
			0, sizeof(struct meta_in));
		pr_debug("i/p: msw_ts 0x%lx lsw_ts 0x%lx nflags 0x%8x\n",
			buf_node->meta_info.meta_in.ntimestamp.highpart,
			buf_node->meta_info.meta_in.ntimestamp.lowpart,
			buf_node->meta_info.meta_in.nflags);
	} else { /* Write */
		memcpy((char *)buf_node->kvaddr,
			&buf_node->meta_info.meta_out,
			sizeof(struct dec_meta_out));
		pr_debug("o/p: msw_ts 0x%8x lsw_ts 0x%8x nflags 0x%8x\n",
		((struct dec_meta_out *)buf_node->kvaddr)->\
				meta_out_dsp[0].msw_ts,
		((struct dec_meta_out *)buf_node->kvaddr)->\
				meta_out_dsp[0].lsw_ts,
		((struct dec_meta_out *)buf_node->kvaddr)->\
				meta_out_dsp[0].nflags);
	}
}

static int audwmapro_pmem_lookup_vaddr(struct q6audio *audio, void *addr,
	unsigned long len, struct audwmapro_pmem_region **region)
{
	struct audwmapro_pmem_region *region_elt;

	int match_count = 0;

	*region = NULL;

	/* returns physical address or zero */
	list_for_each_entry(region_elt, &audio->pmem_region_queue, list) {
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
		pr_err("multiple hits for vaddr %p, len %ld\n", addr, len);
		list_for_each_entry(region_elt, &audio->pmem_region_queue,
					list) {
			if (addr >= region_elt->vaddr &&
			    addr < region_elt->vaddr + region_elt->len &&
			    addr + len <= region_elt->vaddr + region_elt->len)
				pr_err("\t%p, %ld --> %p\n", region_elt->vaddr,
				       region_elt->len,
				       (void *)region_elt->paddr);
		}
	}

	return *region ? 0 : -1;
}

static unsigned long audwmapro_pmem_fixup(struct q6audio *audio, void *addr,
	unsigned long len, int ref_up, void **kvaddr)
{
	struct audwmapro_pmem_region *region;
	unsigned long paddr;
	int ret;

	ret = audwmapro_pmem_lookup_vaddr(audio, addr, len, &region);
	if (ret) {
		pr_err("lookup (%p, %ld) failed\n", addr, len);
		return 0;
	}
	if (ref_up)
		region->ref_cnt++;
	else
		region->ref_cnt--;
	pr_debug("found region %p ref_cnt %d\n", region, region->ref_cnt);
	paddr = region->paddr + (addr - region->vaddr);
	/* provide kernel virtual address for accessing meta information */
	if (kvaddr)
		*kvaddr = (void *) (region->kvaddr + (addr - region->vaddr));
	return paddr;
}

static void audwmapro_post_event(struct q6audio *audio, int type,
			      union msm_audio_event_payload payload)
{
	struct audwmapro_event *e_node = NULL;
	unsigned long flags;

	spin_lock_irqsave(&audio->event_queue_lock, flags);

	if (!list_empty(&audio->free_event_queue)) {
		e_node = list_first_entry(&audio->free_event_queue,
					  struct audwmapro_event, list);
		list_del(&e_node->list);
	} else {
		e_node = kmalloc(sizeof(struct audwmapro_event), GFP_ATOMIC);
		if (!e_node) {
			pr_err("No mem to post event %d\n", type);
			spin_unlock_irqrestore(&audio->event_queue_lock, flags);
			return;
		}
	}

	e_node->event_type = type;
	e_node->payload = payload;

	list_add_tail(&e_node->list, &audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	wake_up(&audio->event_wait);
}

static int audwmapro_enable(struct q6audio *audio)
{
	/* 2nd arg: 0 -> run immediately
	   3rd arg: 0 -> msw_ts, 4th arg: 0 ->lsw_ts */
	return q6asm_run(audio->ac, 0x00, 0x00, 0x00);
}

static int audwmapro_disable(struct q6audio *audio)
{
	int rc = 0;
	if (audio->opened) {
		audio->enabled = 0;
		audio->opened = 0;
		pr_debug("%s: inbytes[%d] insamples[%d]\n", __func__,
			 atomic_read(&audio->in_bytes),
			 atomic_read(&audio->in_samples));
		/* Close the session */
		rc = q6asm_cmd(audio->ac, CMD_CLOSE);
		if (rc < 0)
			pr_err("Failed to close the session rc=%d\n", rc);
		audio->stopped = 1;
		wake_up(&audio->write_wait);
		wake_up(&audio->cmd_wait);
	}
	pr_debug("enabled[%d]\n", audio->enabled);
	return rc;
}

static int audwmapro_pause(struct q6audio *audio)
{
	int rc = 0;

	pr_info("%s, enabled = %d\n", __func__,
			audio->enabled);
	if (audio->enabled) {
		rc = q6asm_cmd(audio->ac, CMD_PAUSE);
		if (rc < 0)
			pr_err("%s: pause cmd failed rc=%d\n", __func__, rc);

	} else
		pr_err("%s: Driver not enabled\n", __func__);
	return rc;
}

static int audwmapro_flush(struct q6audio *audio)
{
	int rc;

	if (audio->enabled) {
		/* Implicitly issue a pause to the decoder before flushing if
		   it is not in pause state */
		if (!(audio->drv_status & ADRV_STATUS_PAUSE)) {
			rc = audwmapro_pause(audio);
			if (rc < 0)
				pr_err("%s: pause cmd failed rc=%d\n", __func__,
					rc);
			else
				audio->drv_status |= ADRV_STATUS_PAUSE;
		}
		rc = q6asm_cmd(audio->ac, CMD_FLUSH);
		if (rc < 0)
			pr_err("%s: flush cmd failed rc=%d\n", __func__, rc);
		/* Not in stop state, reenable the stream */
		if (audio->stopped == 0) {
			rc = audwmapro_enable(audio);
			if (rc)
				pr_err("%s:audio re-enable failed\n", __func__);
			else {
				audio->enabled = 1;
				if (audio->drv_status & ADRV_STATUS_PAUSE)
					audio->drv_status &= ~ADRV_STATUS_PAUSE;
			}
		}
	}
	pr_debug("in_bytes %d\n", atomic_read(&audio->in_bytes));
	pr_debug("in_samples %d\n", atomic_read(&audio->in_samples));
	atomic_set(&audio->in_bytes, 0);
	atomic_set(&audio->in_samples, 0);
	return 0;
}

static void audwmapro_async_read(struct q6audio *audio,
		struct audwmapro_buffer_node *buf_node)
{
	struct audio_client *ac;
	struct audio_aio_read_param param;
	int rc;

	pr_debug("%s: Send read buff %p phy %lx len %d\n", __func__, buf_node,
			buf_node->paddr, buf_node->buf.buf_len);
	ac = audio->ac;
	/* Provide address so driver can append nr frames information */
	param.paddr = buf_node->paddr +
			sizeof(struct dec_meta_out);
	param.len = buf_node->buf.buf_len -
			sizeof(struct dec_meta_out);
	param.uid = param.paddr;
	/* Write command will populate paddr as token */
	buf_node->token = param.paddr;
	rc = q6asm_async_read(ac, &param);
	if (rc < 0)
		pr_err("%s:failed\n", __func__);
}

static void audwmapro_async_write(struct q6audio *audio,
		struct audwmapro_buffer_node *buf_node)
{
	int rc;
	struct audio_client *ac;
	struct audio_aio_write_param param;

	pr_debug("%s: Send write buff %p phy %lx len %d\n", __func__, buf_node,
		 buf_node->paddr, buf_node->buf.data_len);

	ac = audio->ac;
	/* Offset with  appropriate meta */
	param.paddr = buf_node->paddr + sizeof(struct meta_in);
	param.len = buf_node->buf.data_len - sizeof(struct meta_in);
	param.msw_ts = buf_node->meta_info.meta_in.ntimestamp.highpart;
	param.lsw_ts = buf_node->meta_info.meta_in.ntimestamp.lowpart;
	/* If no meta_info enaled, indicate no time stamp valid */
	if (audio->buf_cfg.meta_info_enable)
		param.flags = 0;
	else
		param.flags = 0xFF00;
	param.uid = param.paddr;
	/* Read command will populate paddr as token */
	buf_node->token = param.paddr;
	rc = q6asm_async_write(ac, &param);
	if (rc < 0)
		pr_err("%s:failed\n", __func__);
}

/* Write buffer to DSP / Handle Ack from DSP */
static void audwmapro_async_write_ack(struct q6audio *audio, uint32_t token,
		uint32_t *payload)
{
	unsigned long flags;
	union msm_audio_event_payload event_payload;
	struct audwmapro_buffer_node *used_buf;

	/* No active flush in progress */
	if (audio->wflush)
		return;

	spin_lock_irqsave(&audio->dsp_lock, flags);
	BUG_ON(list_empty(&audio->out_queue));
	used_buf = list_first_entry(&audio->out_queue,
				    struct audwmapro_buffer_node, list);
	if (token == used_buf->token) {
		list_del(&used_buf->list);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		pr_debug("consumed buffer\n");
		event_payload.aio_buf = used_buf->buf;
		audwmapro_post_event(audio, AUDIO_EVENT_WRITE_DONE,
				  event_payload);
		kfree(used_buf);
		if (list_empty(&audio->out_queue) &&
			   (audio->drv_status & ADRV_STATUS_FSYNC)) {
			pr_debug("%s: list is empty, reached EOS in\
				Tunnel\n", __func__);
			wake_up(&audio->write_wait);
		}
	} else {
		pr_err("expected=%lx ret=%x\n", used_buf->token, token);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
	}
}

/* Read buffer from DSP / Handle Ack from DSP */
static void audwmapro_async_read_ack(struct q6audio *audio, uint32_t token,
		uint32_t *payload)
{
	unsigned long flags;
	union msm_audio_event_payload event_payload;
	struct audwmapro_buffer_node *filled_buf;

	/* No active flush in progress */
	if (audio->rflush)
		return;

	/* Statistics of read */
	atomic_add(payload[2], &audio->in_bytes);
	atomic_add(payload[7], &audio->in_samples);

	spin_lock_irqsave(&audio->dsp_lock, flags);
	BUG_ON(list_empty(&audio->in_queue));
	filled_buf = list_first_entry(&audio->in_queue,
				      struct audwmapro_buffer_node, list);
	if (token == (filled_buf->token)) {
		list_del(&filled_buf->list);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		event_payload.aio_buf = filled_buf->buf;
		/* Read done Buffer due to flush/normal condition
		   after EOS event, so append EOS buffer */
		if (audio->eos_rsp == 0x1) {
			event_payload.aio_buf.data_len =
					insert_eos_buf(audio, filled_buf);
			/* Reset flag back to indicate eos intimated */
			audio->eos_rsp = 0;
		} else {
			filled_buf->meta_info.meta_out.num_of_frames =
				payload[7];
			pr_debug("nr of frames 0x%8x\n",
				filled_buf->meta_info.meta_out.num_of_frames);
			event_payload.aio_buf.data_len = payload[2] + \
			payload[3] + \
			sizeof(struct dec_meta_out);
			extract_meta_info(audio, filled_buf, 0);
			audio->eos_rsp = 0;
		}
		audwmapro_post_event(audio, AUDIO_EVENT_READ_DONE,
				event_payload);
		kfree(filled_buf);
	} else {
		pr_err("expected=%lx ret=%x\n", filled_buf->token, token);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
	}
}

static void q6_audwmapro_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload, void *priv)
{
	struct q6audio *audio = (struct q6audio *)priv;

	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE:
		pr_debug("%s:ASM_DATA_EVENT_WRITE_DONE token = 0x%x\n",
			 __func__, token);
		audwmapro_async_write_ack(audio, token, payload);
		break;
	case ASM_DATA_EVENT_READ_DONE:
		pr_debug("%s:ASM_DATA_EVENT_READ_DONE token = 0x%x\n",
			 __func__, token);
		audwmapro_async_read_ack(audio, token, payload);
		break;
	case ASM_DATA_CMDRSP_EOS:
		/* EOS Handle */
		pr_debug("%s:ASM_DATA_CMDRSP_EOS\n", __func__);
		if (audio->feedback) { /* Non-Tunnel mode */
			audio->eos_rsp = 1;
			/* propagate input EOS i/p buffer,
			   after receiving DSP acknowledgement */
			if (audio->eos_flag &&
				(audio->eos_write_payload.aio_buf.buf_addr)) {
				audwmapro_post_event(audio,
					AUDIO_EVENT_WRITE_DONE,
					audio->eos_write_payload);
				memset(&audio->eos_write_payload , 0,
					sizeof(union msm_audio_event_payload));
				audio->eos_flag = 0;
			}
		} else { /* Tunnel mode */
			audio->eos_rsp = 1;
			wake_up(&audio->write_wait);
			wake_up(&audio->cmd_wait);
		}
		break;
	default:
		pr_debug("%s:Unhandled event = 0x%8x\n", __func__, opcode);
		break;
	}
}

/* ------------------- device --------------------- */
static void audwmapro_async_out_flush(struct q6audio *audio)
{
	struct audwmapro_buffer_node *buf_node;
	struct list_head *ptr, *next;
	union msm_audio_event_payload payload;
	unsigned long flags;

	pr_debug("%s\n", __func__);
	/* EOS followed by flush, EOS response not guranteed, free EOS i/p
	   buffer */
	spin_lock_irqsave(&audio->dsp_lock, flags);
	if (audio->eos_flag && (audio->eos_write_payload.aio_buf.buf_addr)) {
		pr_debug("%s: EOS followed by flush received,acknowledge eos"\
			" i/p buffer immediately\n", __func__);
		audwmapro_post_event(audio, AUDIO_EVENT_WRITE_DONE,
					audio->eos_write_payload);
		memset(&audio->eos_write_payload , 0,
			sizeof(union msm_audio_event_payload));
	}
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
	list_for_each_safe(ptr, next, &audio->out_queue) {
		buf_node = list_entry(ptr, struct audwmapro_buffer_node, list);
		list_del(&buf_node->list);
		payload.aio_buf = buf_node->buf;
		audwmapro_post_event(audio, AUDIO_EVENT_WRITE_DONE, payload);
		kfree(buf_node);
		pr_debug("%s: Propagate WRITE_DONE during flush\n", __func__);
	}
}

static void audwmapro_async_in_flush(struct q6audio *audio)
{
	struct audwmapro_buffer_node *buf_node;
	struct list_head *ptr, *next;
	union msm_audio_event_payload payload;

	pr_debug("%s\n", __func__);
	list_for_each_safe(ptr, next, &audio->in_queue) {
		buf_node = list_entry(ptr, struct audwmapro_buffer_node, list);
		list_del(&buf_node->list);
		/* Forcefull send o/p eos buffer after flush, if no eos response
		 * received by dsp even after sending eos command */
		if ((audio->eos_rsp != 1) && audio->eos_flag) {
			pr_debug("%s: send eos on o/p buffer during flush\n",\
				__func__);
			payload.aio_buf = buf_node->buf;
			payload.aio_buf.data_len =
					insert_eos_buf(audio, buf_node);
			audio->eos_flag = 0;
		} else {
			payload.aio_buf = buf_node->buf;
			payload.aio_buf.data_len =
					insert_meta_data(audio, buf_node);
		}
		audwmapro_post_event(audio, AUDIO_EVENT_READ_DONE, payload);
		kfree(buf_node);
		pr_debug("%s: Propagate READ_DONE during flush\n", __func__);
	}
}

static void audwmapro_ioport_reset(struct q6audio *audio)
{
	if (audio->drv_status & ADRV_STATUS_AIO_INTF) {
		/* If fsync is in progress, make sure
		 * return value of fsync indicates
		 * abort due to flush
		 */
		if (audio->drv_status & ADRV_STATUS_FSYNC) {
			pr_debug("fsync in progress\n");
			audio->drv_ops.out_flush(audio);
		} else
			audio->drv_ops.out_flush(audio);
		audio->drv_ops.in_flush(audio);
	}
}

static int audwmapro_events_pending(struct q6audio *audio)
{
	unsigned long flags;
	int empty;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	empty = !list_empty(&audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	return empty || audio->event_abort;
}

static void audwmapro_reset_event_queue(struct q6audio *audio)
{
	unsigned long flags;
	struct audwmapro_event *drv_evt;
	struct list_head *ptr, *next;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	list_for_each_safe(ptr, next, &audio->event_queue) {
		drv_evt = list_first_entry(&audio->event_queue,
					   struct audwmapro_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	list_for_each_safe(ptr, next, &audio->free_event_queue) {
		drv_evt = list_first_entry(&audio->free_event_queue,
					   struct audwmapro_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	return;
}

static long audwmapro_process_event_req(struct q6audio *audio,
		void __user *arg)
{
	long rc;
	struct msm_audio_event usr_evt;
	struct audwmapro_event *drv_evt = NULL;
	int timeout;
	unsigned long flags;

	if (copy_from_user(&usr_evt, arg, sizeof(struct msm_audio_event)))
		return -EFAULT;

	timeout = (int)usr_evt.timeout_ms;

	if (timeout > 0) {
		rc = wait_event_interruptible_timeout(audio->event_wait,
						      audwmapro_events_pending
						      (audio),
						      msecs_to_jiffies
						      (timeout));
		if (rc == 0)
			return -ETIMEDOUT;
	} else {
		rc = wait_event_interruptible(audio->event_wait,
					      audwmapro_events_pending(audio));
	}

	if (rc < 0)
		return rc;

	if (audio->event_abort) {
		audio->event_abort = 0;
		return -ENODEV;
	}

	rc = 0;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	if (!list_empty(&audio->event_queue)) {
		drv_evt = list_first_entry(&audio->event_queue,
					   struct audwmapro_event, list);
		list_del(&drv_evt->list);
	}
	if (drv_evt) {
		usr_evt.event_type = drv_evt->event_type;
		usr_evt.event_payload = drv_evt->payload;
		list_add_tail(&drv_evt->list, &audio->free_event_queue);
	} else {
		pr_err("Unexpected path\n");
		spin_unlock_irqrestore(&audio->event_queue_lock, flags);
		return -EPERM;
	}
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	if (drv_evt->event_type == AUDIO_EVENT_WRITE_DONE) {
		pr_debug("posted AUDIO_EVENT_WRITE_DONE to user\n");
		mutex_lock(&audio->write_lock);
		audwmapro_pmem_fixup(audio, drv_evt->payload.aio_buf.buf_addr,
				  drv_evt->payload.aio_buf.buf_len, 0, 0);
		mutex_unlock(&audio->write_lock);
	} else if (drv_evt->event_type == AUDIO_EVENT_READ_DONE) {
		pr_debug("posted AUDIO_EVENT_READ_DONE to user\n");
		mutex_lock(&audio->read_lock);
		audwmapro_pmem_fixup(audio, drv_evt->payload.aio_buf.buf_addr,
				  drv_evt->payload.aio_buf.buf_len, 0, 0);
		mutex_unlock(&audio->read_lock);
	}

	/* Some read buffer might be held up in DSP,release all
	   Once EOS indicated*/
	if (audio->eos_rsp && !list_empty(&audio->in_queue)) {
		pr_debug("Send flush command to release read buffers"\
		" held up in DSP\n");
		audwmapro_flush(audio);
	}

	if (copy_to_user(arg, &usr_evt, sizeof(usr_evt)))
		rc = -EFAULT;

	return rc;
}

static int audwmapro_pmem_check(struct q6audio *audio,
			     void *vaddr, unsigned long len)
{
	struct audwmapro_pmem_region *region_elt;
	struct audwmapro_pmem_region t = {.vaddr = vaddr, .len = len };

	list_for_each_entry(region_elt, &audio->pmem_region_queue, list) {
		if (CONTAINS(region_elt, &t) || CONTAINS(&t, region_elt) ||
		    OVERLAPS(region_elt, &t)) {
			pr_err("region (vaddr %p len %ld)"
			       " clashes with registered region"
			       " (vaddr %p paddr %p len %ld)\n",
			       vaddr, len,
			       region_elt->vaddr,
			       (void *)region_elt->paddr, region_elt->len);
			return -EINVAL;
		}
	}

	return 0;
}

static int audwmapro_pmem_add(struct q6audio *audio,
			   struct msm_audio_pmem_info *info)
{
	unsigned long paddr, kvaddr, len;
	struct file *file;
	struct audwmapro_pmem_region *region;
	int rc = -EINVAL;

	pr_debug("%s\n", __func__);
	region = kmalloc(sizeof(*region), GFP_KERNEL);

	if (!region) {
		rc = -ENOMEM;
		goto end;
	}

	if (get_pmem_file(info->fd, &paddr, &kvaddr, &len, &file)) {
		kfree(region);
		goto end;
	}

	rc = audwmapro_pmem_check(audio, info->vaddr, len);
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
	pr_debug("add region paddr %lx vaddr %p, len %lu kvaddr %lx\n",
		region->paddr, region->vaddr, region->len, region->kvaddr);
	list_add_tail(&region->list, &audio->pmem_region_queue);

	rc = q6asm_memory_map(audio->ac, (uint32_t) paddr, IN, (uint32_t) len,
			1);
	if (rc < 0)
		pr_err("%s: memory map failed\n", __func__);
end:
	return rc;
}

static int audwmapro_pmem_remove(struct q6audio *audio,
			      struct msm_audio_pmem_info *info)
{
	struct audwmapro_pmem_region *region;
	struct list_head *ptr, *next;
	int rc = -EINVAL;

	pr_debug("info fd %d vaddr %p\n", info->fd, info->vaddr);

	list_for_each_safe(ptr, next, &audio->pmem_region_queue) {
		region = list_entry(ptr, struct audwmapro_pmem_region, list);

		if ((region->fd == info->fd) &&
				(region->vaddr == info->vaddr)) {
			if (region->ref_cnt) {
				pr_debug("region %p in use ref_cnt %d\n",
					 region, region->ref_cnt);
				break;
			}
			pr_debug("remove region fd %d vaddr %p\n",
					info->fd, info->vaddr);
			rc = q6asm_memory_unmap(audio->ac,
						(uint32_t) region->paddr, IN);
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

/* audio -> lock must be held at this point */
static int audwmapro_aio_buf_add(struct q6audio *audio, unsigned dir,
			      void __user *arg)
{
	unsigned long flags;
	struct audwmapro_buffer_node *buf_node;

	buf_node = kzalloc(sizeof(*buf_node), GFP_KERNEL);

	if (!buf_node)
		return -ENOMEM;

	if (copy_from_user(&buf_node->buf, arg, sizeof(buf_node->buf))) {
		kfree(buf_node);
		return -EFAULT;
	}

	pr_debug("node %p dir %x buf_addr %p buf_len %d data_len \
			%d\n", buf_node, dir, buf_node->buf.buf_addr,
			buf_node->buf.buf_len, buf_node->buf.data_len);

	buf_node->paddr = audwmapro_pmem_fixup(audio, buf_node->buf.buf_addr,
					    buf_node->buf.buf_len, 1,
					    &buf_node->kvaddr);
	if (dir) {
		/* write */
		if (!buf_node->paddr ||
		    (buf_node->paddr & 0x1) ||
		    (!audio->feedback && !buf_node->buf.data_len)) {
			kfree(buf_node);
			return -EINVAL;
		}
		extract_meta_info(audio, buf_node, 1);
		/* Not a EOS buffer */
		if (!(buf_node->meta_info.meta_in.nflags & AUDWMAPRO_EOS_SET)) {
			spin_lock_irqsave(&audio->dsp_lock, flags);
			audwmapro_async_write(audio, buf_node);
			/* EOS buffer handled in driver */
			list_add_tail(&buf_node->list, &audio->out_queue);
			spin_unlock_irqrestore(&audio->dsp_lock, flags);
		}
		if (buf_node->meta_info.meta_in.nflags & AUDWMAPRO_EOS_SET) {
			if (!audio->wflush) {
				pr_debug("%s:Send EOS cmd at i/p\n", __func__);
				/* Driver will forcefully post writedone event
				   once eos ack recived from DSP*/
				audio->eos_write_payload.aio_buf =\
						buf_node->buf;
				audio->eos_flag = 1;
				audio->eos_rsp = 0;
				q6asm_cmd(audio->ac, CMD_EOS);
				kfree(buf_node);
			} else { /* Flush in progress, send back i/p EOS buffer
				    as is */
				union msm_audio_event_payload event_payload;
				event_payload.aio_buf = buf_node->buf;
				audwmapro_post_event(audio,
					AUDIO_EVENT_WRITE_DONE,
					event_payload);
				kfree(buf_node);
			}
		}
	} else {
		/* read */
		if (!buf_node->paddr ||
		    (buf_node->paddr & 0x1) ||
		    (buf_node->buf.buf_len < PCM_BUFSZ_MIN)) {
			kfree(buf_node);
			return -EINVAL;
		}
		/* No EOS reached */
		if (!audio->eos_rsp) {
			spin_lock_irqsave(&audio->dsp_lock, flags);
			audwmapro_async_read(audio, buf_node);
			/* EOS buffer handled in driver */
			list_add_tail(&buf_node->list, &audio->in_queue);
			spin_unlock_irqrestore(&audio->dsp_lock, flags);
		}
		/* EOS reached at input side fake all upcoming read buffer to
		   indicate the same */
		else {
			union msm_audio_event_payload event_payload;
			event_payload.aio_buf = buf_node->buf;
			event_payload.aio_buf.data_len =
				insert_eos_buf(audio, buf_node);
			pr_debug("%s: propagate READ_DONE as EOS done\n",\
					__func__);
			audwmapro_post_event(audio, AUDIO_EVENT_READ_DONE,
					event_payload);
			kfree(buf_node);
		}
	}
	return 0;
}

/* TBD: Only useful in tunnel-mode */
int audwmapro_async_fsync(struct q6audio *audio)
{
	int rc = 0;

	/* Blocking client sends more data */
	mutex_lock(&audio->lock);
	audio->drv_status |= ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	pr_info("%s:\n", __func__);

	mutex_lock(&audio->write_lock);
	audio->eos_rsp = 0;

	rc = wait_event_interruptible(audio->write_wait,
					(list_empty(&audio->out_queue)) ||
					audio->wflush || audio->stopped);

	if (rc < 0) {
		pr_err("%s: wait event for list_empty failed, rc = %d\n",
			__func__, rc);
		goto done;
	}

	rc = q6asm_cmd(audio->ac, CMD_EOS);

	if (rc < 0)
		pr_err("%s: q6asm_cmd failed, rc = %d", __func__, rc);

	rc = wait_event_interruptible(audio->write_wait,
				  (audio->eos_rsp || audio->wflush ||
				  audio->stopped));

	if (rc < 0) {
		pr_err("%s: wait event for eos_rsp failed, rc = %d\n", __func__,
			rc);
		goto done;
	}

	if (audio->eos_rsp == 1) {
		rc = audwmapro_enable(audio);
		if (rc)
			pr_err("%s: audio enable failed\n", __func__);
		else {
			audio->drv_status &= ~ADRV_STATUS_PAUSE;
			audio->enabled = 1;
		}
	}

	if (audio->stopped || audio->wflush)
		rc = -EBUSY;

done:
	mutex_unlock(&audio->write_lock);
	mutex_lock(&audio->lock);
	audio->drv_status &= ~ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	return rc;
}

int audwmapro_fsync(struct file *file, int datasync)
{
	struct q6audio *audio = file->private_data;

	if (!audio->enabled || audio->feedback)
		return -EINVAL;

	return audio->drv_ops.fsync(audio);
}

static void audwmapro_reset_pmem_region(struct q6audio *audio)
{
	struct audwmapro_pmem_region *region;
	struct list_head *ptr, *next;

	list_for_each_safe(ptr, next, &audio->pmem_region_queue) {
		region = list_entry(ptr, struct audwmapro_pmem_region, list);
		list_del(&region->list);
		put_pmem_file(region->file);
		kfree(region);
	}

	return;
}

#ifdef CONFIG_DEBUG_FS
static ssize_t audwmapro_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t audwmapro_debug_read(struct file *file, char __user * buf,
				 size_t count, loff_t *ppos)
{
	const int debug_bufmax = 4096;
	static char buffer[4096];
	int n = 0;
	struct q6audio *audio = file->private_data;

	mutex_lock(&audio->lock);
	n = scnprintf(buffer, debug_bufmax, "opened %d\n", audio->opened);
	n += scnprintf(buffer + n, debug_bufmax - n,
		       "enabled %d\n", audio->enabled);
	n += scnprintf(buffer + n, debug_bufmax - n,
		       "stopped %d\n", audio->stopped);
	n += scnprintf(buffer + n, debug_bufmax - n,
		       "feedback %d\n", audio->feedback);
	mutex_unlock(&audio->lock);
	/* Following variables are only useful for debugging when
	 * when playback halts unexpectedly. Thus, no mutual exclusion
	 * enforced
	 */
	n += scnprintf(buffer + n, debug_bufmax - n,
		       "wflush %d\n", audio->wflush);
	n += scnprintf(buffer + n, debug_bufmax - n,
		       "rflush %d\n", audio->rflush);
	n += scnprintf(buffer + n, debug_bufmax - n,
		       "inqueue empty %d\n", list_empty(&audio->in_queue));
	n += scnprintf(buffer + n, debug_bufmax - n,
		       "outqueue empty %d\n", list_empty(&audio->out_queue));
	buffer[n] = 0;
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static const struct file_operations audwmapro_debug_fops = {
	.read = audwmapro_debug_read,
	.open = audwmapro_debug_open,
};
#endif

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct q6audio *audio = file->private_data;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		stats.byte_count = atomic_read(&audio->in_bytes);
		stats.sample_count = atomic_read(&audio->in_samples);
		if (copy_to_user((void *)arg, &stats, sizeof(stats)))
			return -EFAULT;
		return rc;
	}

	if (cmd == AUDIO_GET_EVENT) {
		pr_debug("AUDIO_GET_EVENT\n");
		if (mutex_trylock(&audio->get_event_lock)) {
			rc = audwmapro_process_event_req(audio,
						      (void __user *)arg);
			mutex_unlock(&audio->get_event_lock);
		} else
			rc = -EBUSY;
		return rc;
	}

	if (cmd == AUDIO_ASYNC_WRITE) {
		mutex_lock(&audio->write_lock);
		if (audio->drv_status & ADRV_STATUS_FSYNC)
			rc = -EBUSY;
		else {
			if (audio->enabled)
				rc = audwmapro_aio_buf_add(audio, 1,
						(void __user *)arg);
			else
				rc = -EPERM;
		}
		mutex_unlock(&audio->write_lock);
		return rc;
	}

	if (cmd == AUDIO_ASYNC_READ) {
		mutex_lock(&audio->read_lock);
		if ((audio->feedback) && (audio->enabled))
			rc = audwmapro_aio_buf_add(audio, 0,
					(void __user *)arg);
		else
			rc = -EPERM;
		mutex_unlock(&audio->read_lock);
		return rc;
	}

	if (cmd == AUDIO_ABORT_GET_EVENT) {
		audio->event_abort = 1;
		wake_up(&audio->event_wait);
		return 0;
	}

	mutex_lock(&audio->lock);
	switch (cmd) {
	case AUDIO_START: {
		struct asm_wmapro_cfg wmapro_cfg;
		if (audio->feedback == NON_TUNNEL_MODE) {
			/* Configure PCM output block */
			rc = q6asm_enc_cfg_blk_pcm(audio->ac,
					audio->pcm_cfg.sample_rate,
					audio->pcm_cfg.channel_count);
			if (rc < 0) {
				pr_err("pcm output block config failed\n");
				break;
			}
		}
		if ((audio->wmapro_config.formattag == 0x162) ||
		(audio->wmapro_config.formattag == 0x166)) {
			wmapro_cfg.format_tag = audio->wmapro_config.formattag;
		} else {
			pr_err("%s:AUDIO_START failed: formattag = %d\n",
				__func__, audio->wmapro_config.formattag);
			rc = -EINVAL;
			break;
		}
		if ((audio->wmapro_config.numchannels == 1) ||
		(audio->wmapro_config.numchannels == 2)) {
			wmapro_cfg.ch_cfg = audio->wmapro_config.numchannels;
		} else {
			pr_err("%s:AUDIO_START failed: channels = %d\n",
				__func__, audio->wmapro_config.numchannels);
			rc = -EINVAL;
			break;
		}
		if ((audio->wmapro_config.samplingrate <= 48000) ||
		(audio->wmapro_config.samplingrate > 0)) {
			wmapro_cfg.sample_rate =
				audio->wmapro_config.samplingrate;
		} else {
			pr_err("%s:AUDIO_START failed: sample_rate = %d\n",
				__func__, audio->wmapro_config.samplingrate);
			rc = -EINVAL;
			break;
		}
		wmapro_cfg.avg_bytes_per_sec =
				audio->wmapro_config.avgbytespersecond;
		if ((audio->wmapro_config.asfpacketlength <= 13376) ||
		(audio->wmapro_config.asfpacketlength > 0)) {
			wmapro_cfg.block_align =
				audio->wmapro_config.asfpacketlength;
		} else {
			pr_err("%s:AUDIO_START failed: block_align = %d\n",
				__func__, audio->wmapro_config.asfpacketlength);
			rc = -EINVAL;
			break;
		}
		if (audio->wmapro_config.validbitspersample == 16) {
			wmapro_cfg.valid_bits_per_sample =
				audio->wmapro_config.validbitspersample;
		} else {
			pr_err("%s:AUDIO_START failed: bitspersample = %d\n",
				__func__,
				audio->wmapro_config.validbitspersample);
			rc = -EINVAL;
			break;
		}
		if ((audio->wmapro_config.channelmask  == 4) ||
		(audio->wmapro_config.channelmask == 3)) {
			wmapro_cfg.ch_mask =  audio->wmapro_config.channelmask;
		} else {
			pr_err("%s:AUDIO_START failed: channel_mask = %d\n",
				__func__, audio->wmapro_config.channelmask);
			rc = -EINVAL;
			break;
		}
		wmapro_cfg.encode_opt = audio->wmapro_config.encodeopt;
		wmapro_cfg.adv_encode_opt =
				audio->wmapro_config.advancedencodeopt;
		wmapro_cfg.adv_encode_opt2 =
				audio->wmapro_config.advancedencodeopt2;
		/* Configure Media format block */
		rc = q6asm_media_format_block_wmapro(audio->ac, &wmapro_cfg);
		if (rc < 0) {
			pr_err("cmd media format block failed\n");
			break;
		}
		rc = audwmapro_enable(audio);
		audio->eos_rsp = 0;
		audio->eos_flag = 0;
		if (!rc) {
			audio->enabled = 1;
		} else {
			audio->enabled = 0;
			pr_err("Audio Start procedure failed rc=%d\n", rc);
			break;
		}
		pr_debug("AUDIO_START success enable[%d]\n", audio->enabled);
		if (audio->stopped == 1)
			audio->stopped = 0;
		break;
	}
	case AUDIO_STOP: {
		pr_debug("AUDIO_STOP\n");
		audio->stopped = 1;
		audwmapro_flush(audio);
		audio->enabled = 0;
		audio->drv_status &= ~ADRV_STATUS_PAUSE;
		if (rc < 0) {
			pr_err("Audio Stop procedure failed rc=%d\n", rc);
			break;
		}
		break;
	}
	case AUDIO_PAUSE: {
		pr_debug("AUDIO_PAUSE %ld\n", arg);
		if (arg == 1) {
			rc = audwmapro_pause(audio);
			if (rc < 0)
				pr_err("%s: pause FAILED rc=%d\n", __func__,
						rc);
				audio->drv_status |= ADRV_STATUS_PAUSE;
		} else if (arg == 0) {
			if (audio->drv_status & ADRV_STATUS_PAUSE) {
				rc = audwmapro_enable(audio);
				if (rc)
					pr_err("%s: audio enable failed\n",
						__func__);
				else {
					audio->drv_status &= ~ADRV_STATUS_PAUSE;
					audio->enabled = 1;
				}
			}
		}
		break;
	}
	case AUDIO_FLUSH: {
		pr_debug("AUDIO_FLUSH\n");
		audio->rflush = 1;
		audio->wflush = 1;
		/* Flush DSP */
		rc = audwmapro_flush(audio);
		/* Flush input / Output buffer in software*/
		audwmapro_ioport_reset(audio);
		if (rc < 0) {
			pr_err("AUDIO_FLUSH interrupted\n");
			rc = -EINTR;
		} else {
			audio->rflush = 0;
			audio->wflush = 0;
		}
		audio->eos_flag = 0;
		audio->eos_rsp = 0;
		break;
	}
	case AUDIO_REGISTER_PMEM: {
		struct msm_audio_pmem_info info;
		pr_debug("AUDIO_REGISTER_PMEM\n");
		if (copy_from_user(&info, (void *)arg, sizeof(info)))
			rc = -EFAULT;
		else
			rc = audwmapro_pmem_add(audio, &info);
		break;
	}
	case AUDIO_DEREGISTER_PMEM: {
		struct msm_audio_pmem_info info;
		pr_debug("AUDIO_DEREGISTER_PMEM\n");
		if (copy_from_user(&info, (void *)arg, sizeof(info)))
			rc = -EFAULT;
		else
			rc = audwmapro_pmem_remove(audio, &info);
		break;
	}
	case AUDIO_GET_WMAPRO_CONFIG: {
		if (copy_to_user((void *)arg, &audio->wmapro_config,
				 sizeof(struct msm_audio_wmapro_config))) {
			rc = -EFAULT;
			break;
		}
		break;
		}
	case AUDIO_SET_WMAPRO_CONFIG: {
		if (copy_from_user(&audio->wmapro_config, (void *)arg,
				 sizeof(struct msm_audio_wmapro_config))) {
			rc = -EFAULT;
			break;
		}
		break;
		}
	case AUDIO_GET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		memset(&cfg, 0, sizeof(cfg));
		cfg.buffer_size = audio->str_cfg.buffer_size;
		cfg.buffer_count = audio->str_cfg.buffer_count;
		pr_debug("GET STREAM CFG %d %d\n", cfg.buffer_size,
			 cfg.buffer_count);
		if (copy_to_user((void *)arg, &cfg, sizeof(cfg)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_SET_STREAM_CONFIG: {
		struct msm_audio_stream_config cfg;
		pr_debug("SET STREAM CONFIG\n");
		if (copy_from_user(&cfg, (void *)arg, sizeof(cfg))) {
			rc = -EFAULT;
			break;
		}
		audio->str_cfg.buffer_size = FRAME_SIZE;
		audio->str_cfg.buffer_count = FRAME_NUM;
		rc = 0;
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config cfg;
		if (copy_to_user((void *)arg, &audio->pcm_cfg, sizeof(cfg)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config config;
		if (copy_from_user(&config, (void *)arg, sizeof(config))) {
				rc = -EFAULT;
				break;
		}
		if (audio->feedback != NON_TUNNEL_MODE) {
			pr_err("Not sufficient permission to"
				       "change the playback mode\n");
			rc = -EACCES;
			break;
		}
		if ((config.buffer_count > PCM_BUF_COUNT) ||
		    (config.buffer_count == 1))
			config.buffer_count = PCM_BUF_COUNT;

		if (config.buffer_size < PCM_BUFSZ_MIN)
			config.buffer_size = PCM_BUFSZ_MIN;

		audio->pcm_cfg.buffer_count = config.buffer_count;
		audio->pcm_cfg.buffer_size = config.buffer_size;
		audio->pcm_cfg.channel_count = config.channel_count;
		audio->pcm_cfg.sample_rate = config.sample_rate;
		rc = 0;
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

		audio->buf_cfg.meta_info_enable = cfg.meta_info_enable;
		pr_debug("%s:session id %d: Set-buf-cfg: meta[%d]", __func__,
				audio->ac->session, cfg.meta_info_enable);
		break;
	}
	case AUDIO_GET_BUF_CFG: {
		pr_debug("%s:session id %d: Get-buf-cfg: meta[%d]\
			framesperbuf[%d]\n", __func__,
			audio->ac->session, audio->buf_cfg.meta_info_enable,
			audio->buf_cfg.frames_per_buf);

		if (copy_to_user((void *)arg, &audio->buf_cfg,
					sizeof(struct msm_audio_buf_cfg)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_GET_SESSION_ID: {
			if (copy_to_user((void *)arg, &audio->ac->session,
					 sizeof(unsigned short))) {
				rc = -EFAULT;
			}
			break;
		}
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

static int audio_release(struct inode *inode, struct file *file)
{
	struct q6audio *audio = file->private_data;
	mutex_lock(&audio->lock);
	audwmapro_disable(audio);
	audio->drv_ops.out_flush(audio);
	audio->drv_ops.in_flush(audio);
	audwmapro_reset_pmem_region(audio);
	audio->event_abort = 1;
	wake_up(&audio->event_wait);
	audwmapro_reset_event_queue(audio);
	q6asm_audio_client_free(audio->ac);
	mutex_unlock(&audio->lock);
	mutex_destroy(&audio->lock);
	mutex_destroy(&audio->read_lock);
	mutex_destroy(&audio->write_lock);
	mutex_destroy(&audio->get_event_lock);
#ifdef CONFIG_DEBUG_FS
	if (audio->dentry)
		debugfs_remove(audio->dentry);
#endif
	kfree(audio);
	pr_info("%s: wmapro decoder success\n", __func__);
	return 0;
}

static int audio_open(struct inode *inode, struct file *file)
{
	struct q6audio *audio = NULL;
	int rc = 0;
	int i;
	struct audwmapro_event *e_node = NULL;

#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_wmapro_" + 5];
#endif
	audio = kzalloc(sizeof(struct q6audio), GFP_KERNEL);

	if (audio == NULL) {
		pr_err("Could not allocate memory for wma decode driver\n");
		return -ENOMEM;
	}

	/* Settings will be re-config at AUDIO_SET_CONFIG,
	 * but at least we need to have initial config
	 */
	audio->str_cfg.buffer_size = FRAME_SIZE;
	audio->str_cfg.buffer_count = FRAME_NUM;
	audio->pcm_cfg.buffer_size = PCM_BUFSZ_MIN;
	audio->pcm_cfg.buffer_count = PCM_BUF_COUNT;
	audio->pcm_cfg.sample_rate = 48000;
	audio->pcm_cfg.channel_count = 2;

	audio->ac = q6asm_audio_client_alloc((app_cb) q6_audwmapro_cb,
					     (void *)audio);

	if (!audio->ac) {
		pr_err("Could not allocate memory for audio client\n");
		kfree(audio);
		return -ENOMEM;
	}
	/* Only AIO interface */
	if (file->f_flags & O_NONBLOCK) {
		pr_debug("set to aio interface\n");
		audio->drv_status |= ADRV_STATUS_AIO_INTF;
		audio->drv_ops.out_flush = audwmapro_async_out_flush;
		audio->drv_ops.in_flush = audwmapro_async_in_flush;
		audio->drv_ops.fsync = audwmapro_async_fsync;
		q6asm_set_io_mode(audio->ac, ASYNC_IO_MODE);
	} else {
		pr_err("SIO interface not supported\n");
		rc = -EACCES;
		goto fail;
	}

	/* open in T/NT mode */
	if ((file->f_mode & FMODE_WRITE) && (file->f_mode & FMODE_READ)) {
		rc = q6asm_open_read_write(audio->ac, FORMAT_LINEAR_PCM,
					   FORMAT_WMA_V10PRO);
		if (rc < 0) {
			pr_err("NT mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = NON_TUNNEL_MODE;
		/* open WMA decoder, expected frames is always 1*/
		audio->buf_cfg.frames_per_buf = 0x01;
		audio->buf_cfg.meta_info_enable = 0x01;
	} else if ((file->f_mode & FMODE_WRITE) &&
			!(file->f_mode & FMODE_READ)) {
		rc = q6asm_open_write(audio->ac, FORMAT_WMA_V10PRO);
		if (rc < 0) {
			pr_err("T mode Open failed rc=%d\n", rc);
			rc = -ENODEV;
			goto fail;
		}
		audio->feedback = TUNNEL_MODE;
		audio->buf_cfg.meta_info_enable = 0x00;
	} else {
		pr_err("Not supported mode\n");
		rc = -EACCES;
		goto fail;
	}
	/* Initialize all locks of audio instance */
	mutex_init(&audio->lock);
	mutex_init(&audio->read_lock);
	mutex_init(&audio->write_lock);
	mutex_init(&audio->get_event_lock);
	spin_lock_init(&audio->dsp_lock);
	spin_lock_init(&audio->event_queue_lock);
	init_waitqueue_head(&audio->cmd_wait);
	init_waitqueue_head(&audio->write_wait);
	init_waitqueue_head(&audio->event_wait);
	INIT_LIST_HEAD(&audio->out_queue);
	INIT_LIST_HEAD(&audio->in_queue);
	INIT_LIST_HEAD(&audio->pmem_region_queue);
	INIT_LIST_HEAD(&audio->free_event_queue);
	INIT_LIST_HEAD(&audio->event_queue);

	audio->drv_ops.out_flush(audio);
	audio->opened = 1;
	file->private_data = audio;

#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof name, "msm_wmapro_%04x", audio->ac->session);
	audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
					    NULL, (void *)audio,
					    &audwmapro_debug_fops);

	if (IS_ERR(audio->dentry))
		pr_debug("debugfs_create_file failed\n");
#endif
	for (i = 0; i < AUDWMAPRO_EVENT_NUM; i++) {
		e_node = kmalloc(sizeof(struct audwmapro_event), GFP_KERNEL);
		if (e_node)
			list_add_tail(&e_node->list, &audio->free_event_queue);
		else {
			pr_err("event pkt alloc failed\n");
			break;
		}
	}
	pr_info("%s:wmapro decoder open success, session_id = %d\n", __func__,
				audio->ac->session);
	return 0;
fail:
	q6asm_audio_client_free(audio->ac);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_wmapro_fops = {
	.owner = THIS_MODULE,
	.open = audio_open,
	.release = audio_release,
	.unlocked_ioctl = audio_ioctl,
	.fsync = audwmapro_fsync,
};

struct miscdevice audwmapro_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_wmapro",
	.fops = &audio_wmapro_fops,
};

static int __init audio_wmapro_init(void)
{
	return misc_register(&audwmapro_misc);
}

device_initcall(audio_wmapro_init);

/* arch/arm/mach-msm/qdsp5v2/audio_pcm.c
 *
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

#include <asm/ioctls.h>
#include <asm/atomic.h>

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/list.h>
#include <linux/android_pmem.h>
#include <linux/memory_alloc.h>
#include <linux/slab.h>
#include <linux/msm_audio.h>
#include <mach/msm_adsp.h>

#include <mach/qdsp5v2/audio_dev_ctl.h>

#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/msm_subsystem_map.h>
#include <mach/qdsp5v2/qdsp5audppcmdi.h>
#include <mach/qdsp5v2/qdsp5audppmsg.h>
#include <mach/qdsp5v2/qdsp5audplaycmdi.h>
#include <mach/qdsp5v2/qdsp5audplaymsg.h>
#include <mach/qdsp5v2/audpp.h>
#include <mach/debug_mm.h>
#include <mach/msm_memtypes.h>

#define ADRV_STATUS_AIO_INTF 0x00000001
#define ADRV_STATUS_OBUF_GIVEN 0x00000002
#define ADRV_STATUS_IBUF_GIVEN 0x00000004
#define ADRV_STATUS_FSYNC 0x00000008

/* Size must be power of 2 */
#define BUFSZ_MAX 32768
#define BUFSZ_MIN 4096
#define DMASZ_MAX (BUFSZ_MAX * 2)
#define DMASZ_MIN (BUFSZ_MIN * 2)

#define AUDDEC_DEC_PCM 0

/* Decoder status received from AUDPPTASK */
#define  AUDPP_DEC_STATUS_SLEEP	0
#define  AUDPP_DEC_STATUS_INIT  1
#define  AUDPP_DEC_STATUS_CFG   2
#define  AUDPP_DEC_STATUS_PLAY  3

#define AUDPCM_EVENT_NUM 10 /* Default number of pre-allocated event packets */

#define __CONTAINS(r, v, l) ({					\
	typeof(r) __r = r;					\
	typeof(v) __v = v;					\
	typeof(v) __e = __v + l;				\
	int res = ((__v >= __r->vaddr) && 			\
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

struct buffer {
	void *data;
	unsigned size;
	unsigned used;		/* Input usage actual DSP produced PCM size  */
	unsigned addr;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
struct audpcm_suspend_ctl {
  struct early_suspend node;
  struct audio *audio;
};
#endif

struct audpcm_event {
	struct list_head list;
	int event_type;
	union msm_audio_event_payload payload;
};

struct audpcm_pmem_region {
	struct list_head list;
	struct file *file;
	int fd;
	void *vaddr;
	unsigned long paddr;
	unsigned long kvaddr;
	unsigned long len;
	unsigned ref_cnt;
};

struct audpcm_buffer_node {
	struct list_head list;
	struct msm_audio_aio_buf buf;
	unsigned long paddr;
};

struct audpcm_drv_operations {
	void (*send_data)(struct audio *, unsigned);
	void (*out_flush)(struct audio *);
	int (*fsync)(struct audio *);
};

struct audio {
	struct buffer out[2];

	spinlock_t dsp_lock;

	uint8_t out_head;
	uint8_t out_tail;
	uint8_t out_needed; /* number of buffers the dsp is waiting for */
	unsigned out_dma_sz;
	struct list_head out_queue; /* queue to retain output buffers */
	atomic_t out_bytes;

	struct mutex lock;
	struct mutex write_lock;
	wait_queue_head_t write_wait;

	struct msm_adsp_module *audplay;

	/* configuration to use on next enable */
	uint32_t out_sample_rate;
	uint32_t out_channel_mode;
	uint32_t out_bits; /* bits per sample */

	/* data allocated for various buffers */
	char *data;
	int32_t phys;
	struct msm_mapped_buffer *map_v_write;
	uint32_t drv_status;
	int wflush; /* Write flush */
	int opened;
	int enabled;
	int running;
	int stopped; /* set when stopped, cleared on flush */
	int teos; /* valid only if tunnel mode & no data left for decoder */
	enum msm_aud_decoder_state dec_state; /* Represents decoder state */
	int reserved; /* A byte is being reserved */
	char rsv_byte; /* Handle odd length user data */

	const char *module_name;
	unsigned queue_id;
	uint32_t device_events;

	unsigned volume;

	uint16_t dec_id;
	int16_t source;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct audpcm_suspend_ctl suspend_ctl;
#endif

#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif
	wait_queue_head_t wait;
	struct list_head free_event_queue;
	struct list_head event_queue;
	wait_queue_head_t event_wait;
	spinlock_t event_queue_lock;
	struct mutex get_event_lock;
	int event_abort;
	/* AV sync Info */
	int avsync_flag;              /* Flag to indicate feedback from DSP */
	wait_queue_head_t avsync_wait;/* Wait queue for AV Sync Message     */
	/* flags, 48 bits sample/bytes counter per channel */
	uint16_t avsync[AUDPP_AVSYNC_CH_COUNT * AUDPP_AVSYNC_NUM_WORDS + 1];

	struct list_head pmem_region_queue;
	struct audpcm_drv_operations drv_ops;
};

static int auddec_dsp_config(struct audio *audio, int enable);
static void audpp_cmd_cfg_adec_params(struct audio *audio);
static void audplay_send_data(struct audio *audio, unsigned needed);
static void audio_dsp_event(void *private, unsigned id, uint16_t *msg);
static void audpcm_post_event(struct audio *audio, int type,
	union msm_audio_event_payload payload);
static unsigned long audpcm_pmem_fixup(struct audio *audio, void *addr,
	unsigned long len, int ref_up);

static void pcm_listner(u32 evt_id, union auddev_evt_data *evt_payload,
			void *private_data)
{
	struct audio *audio = (struct audio *) private_data;
	switch (evt_id) {
	case AUDDEV_EVT_DEV_RDY:
		MM_DBG("AUDDEV_EVT_DEV_RDY\n");
		audio->source |= (0x1 << evt_payload->routing_id);
		if (audio->running == 1 && audio->enabled == 1)
			audpp_route_stream(audio->dec_id, audio->source);
		break;
	case AUDDEV_EVT_DEV_RLS:
		MM_DBG("AUDDEV_EVT_DEV_RLS\n");
		audio->source &= ~(0x1 << evt_payload->routing_id);
		if (audio->running == 1 && audio->enabled == 1)
			audpp_route_stream(audio->dec_id, audio->source);
		break;
	case AUDDEV_EVT_STREAM_VOL_CHG:
		audio->volume = evt_payload->session_vol;
		MM_DBG("AUDDEV_EVT_STREAM_VOL_CHG, stream vol %d\n",
				audio->volume);
		if (audio->running)
			audpp_set_volume_and_pan(audio->dec_id, audio->volume,
					0, POPP);
		break;
	default:
		MM_ERR("ERROR:wrong event\n");
		break;
	}
}
/* must be called with audio->lock held */
static int audio_enable(struct audio *audio)
{
	MM_DBG("\n"); /* Macro prints the file name and function */

	if (audio->enabled)
		return 0;

	audio->dec_state = MSM_AUD_DECODER_STATE_NONE;
	audio->out_tail = 0;
	audio->out_needed = 0;

	if (msm_adsp_enable(audio->audplay)) {
		MM_ERR("msm_adsp_enable(audplay) failed\n");
		return -ENODEV;
	}

	if (audpp_enable(audio->dec_id, audio_dsp_event, audio)) {
		MM_ERR("audpp_enable() failed\n");
		msm_adsp_disable(audio->audplay);
		return -ENODEV;
	}

	audio->enabled = 1;
	return 0;
}

/* must be called with audio->lock held */
static int audio_disable(struct audio *audio)
{
	int rc = 0;
	MM_DBG("\n"); /* Macro prints the file name and function */
	if (audio->enabled) {
		audio->enabled = 0;
		audio->dec_state = MSM_AUD_DECODER_STATE_NONE;
		auddec_dsp_config(audio, 0);
		rc = wait_event_interruptible_timeout(audio->wait,
				audio->dec_state != MSM_AUD_DECODER_STATE_NONE,
				msecs_to_jiffies(MSM_AUD_DECODER_WAIT_MS));
		if (rc == 0)
			rc = -ETIMEDOUT;
		else if (audio->dec_state != MSM_AUD_DECODER_STATE_CLOSE)
			rc = -EFAULT;
		else
			rc = 0;
		wake_up(&audio->write_wait);
		msm_adsp_disable(audio->audplay);
		audpp_disable(audio->dec_id, audio);
		audio->out_needed = 0;
	}
	return rc;
}

/* ------------------- dsp --------------------- */
static void audplay_dsp_event(void *data, unsigned id, size_t len,
			      void (*getevent) (void *ptr, size_t len))
{
	struct audio *audio = data;
	uint32_t msg[28];
	getevent(msg, sizeof(msg));

	MM_DBG("msg_id=%x\n", id);

	switch (id) {
	case AUDPLAY_MSG_DEC_NEEDS_DATA:
		audio->drv_ops.send_data(audio, 1);
		break;

	case ADSP_MESSAGE_ID:
		MM_DBG("Received ADSP event:module audplaytask\n");
		break;

	default:
		MM_ERR("unexpected message from decoder\n");
		break;
	}
}

static void audio_dsp_event(void *private, unsigned id, uint16_t *msg)
{
	struct audio *audio = private;

	switch (id) {
	case AUDPP_MSG_STATUS_MSG:{
			unsigned status = msg[1];

			switch (status) {
			case AUDPP_DEC_STATUS_SLEEP: {
				uint16_t reason = msg[2];
				MM_DBG("decoder status:sleep reason=0x%04x\n",
						reason);
				if ((reason == AUDPP_MSG_REASON_MEM)
						|| (reason ==
						AUDPP_MSG_REASON_NODECODER)) {
					audio->dec_state =
						MSM_AUD_DECODER_STATE_FAILURE;
					wake_up(&audio->wait);
				} else if (reason == AUDPP_MSG_REASON_NONE) {
					/* decoder is in disable state */
					audio->dec_state =
						MSM_AUD_DECODER_STATE_CLOSE;
					wake_up(&audio->wait);
				}
				break;
			}
			case AUDPP_DEC_STATUS_INIT:
				MM_DBG("decoder status: init \n");
				audpp_cmd_cfg_adec_params(audio);
				break;

			case AUDPP_DEC_STATUS_CFG:
				MM_DBG("decoder status: cfg \n");
				break;
			case AUDPP_DEC_STATUS_PLAY:
				MM_DBG("decoder status: play \n");
				audpp_route_stream(audio->dec_id,
						audio->source);
				audio->dec_state =
					MSM_AUD_DECODER_STATE_SUCCESS;
				wake_up(&audio->wait);
				break;
			default:
				MM_ERR("unknown decoder status\n");
				break;
			}
			break;
		}
	case AUDPP_MSG_CFG_MSG:
		if (msg[0] == AUDPP_MSG_ENA_ENA) {
			MM_DBG("CFG_MSG ENABLE\n");
			auddec_dsp_config(audio, 1);
			audio->out_needed = 0;
			audio->running = 1;
			audpp_set_volume_and_pan(audio->dec_id, audio->volume,
					0, POPP);
		} else if (msg[0] == AUDPP_MSG_ENA_DIS) {
			MM_DBG("CFG_MSG DISABLE\n");
			audio->running = 0;
		} else {
			MM_ERR("audio_dsp_event: CFG_MSG %d?\n", msg[0]);
		}
		break;
	case AUDPP_MSG_FLUSH_ACK:
		MM_DBG("FLUSH_ACK\n");
		audio->wflush = 0;
		wake_up(&audio->write_wait);
		break;

	case AUDPP_MSG_PCMDMAMISSED:
		MM_DBG("PCMDMAMISSED\n");
		audio->teos = 1;
		wake_up(&audio->write_wait);
		break;

	case AUDPP_MSG_AVSYNC_MSG:
		pr_info("%s: AVSYNC_MSG\n", __func__);
		memcpy(&audio->avsync[0], msg, sizeof(audio->avsync));
		audio->avsync_flag = 1;
		wake_up(&audio->avsync_wait);
		break;

	default:
		MM_DBG("audio_dsp_event: UNKNOWN (%d)\n", id);
	}

}


struct msm_adsp_ops audpcmdec_adsp_ops = {
	.event = audplay_dsp_event,
};


#define audplay_send_queue0(audio, cmd, len) \
	msm_adsp_write(audio->audplay, audio->queue_id, \
			cmd, len)

static int auddec_dsp_config(struct audio *audio, int enable)
{
	struct audpp_cmd_cfg_dec_type cfg_dec_cmd;

	memset(&cfg_dec_cmd, 0, sizeof(cfg_dec_cmd));

	cfg_dec_cmd.cmd_id = AUDPP_CMD_CFG_DEC_TYPE;
	if (enable)
		cfg_dec_cmd.dec_cfg = AUDPP_CMD_UPDATDE_CFG_DEC |
				AUDPP_CMD_ENA_DEC_V | AUDDEC_DEC_PCM;
	else
		cfg_dec_cmd.dec_cfg = AUDPP_CMD_UPDATDE_CFG_DEC |
				AUDPP_CMD_DIS_DEC_V;
	cfg_dec_cmd.dm_mode = 0x0;
	cfg_dec_cmd.stream_id = audio->dec_id;
	return audpp_send_queue1(&cfg_dec_cmd, sizeof(cfg_dec_cmd));
}

static void audpp_cmd_cfg_adec_params(struct audio *audio)
{
	struct audpp_cmd_cfg_adec_params_wav cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.common.cmd_id = AUDPP_CMD_CFG_ADEC_PARAMS;
	cmd.common.length = AUDPP_CMD_CFG_ADEC_PARAMS_WAV_LEN >> 1;
	cmd.common.dec_id = audio->dec_id;
	cmd.common.input_sampling_frequency = audio->out_sample_rate;
	cmd.stereo_cfg = audio->out_channel_mode;
	cmd.pcm_width = audio->out_bits;
	cmd.sign = 0;
	audpp_send_queue2(&cmd, sizeof(cmd));
}

static int audplay_dsp_send_data_avail(struct audio *audio,
					unsigned idx, unsigned len)
{
	struct audplay_cmd_bitstream_data_avail cmd;

	cmd.cmd_id		= AUDPLAY_CMD_BITSTREAM_DATA_AVAIL;
	cmd.decoder_id		= audio->dec_id;
	cmd.buf_ptr		= audio->out[idx].addr;
	cmd.buf_size		= len/2;
	cmd.partition_number	= 0;
	/* complete writes to the input buffer */
	wmb();
	return audplay_send_queue0(audio, &cmd, sizeof(cmd));
}

static void audpcm_async_send_data(struct audio *audio, unsigned needed)
{
	unsigned long flags;

	if (!audio->running)
		return;

	spin_lock_irqsave(&audio->dsp_lock, flags);

	if (needed && !audio->wflush) {
		audio->out_needed = 1;
		if (audio->drv_status & ADRV_STATUS_OBUF_GIVEN) {
			/* pop one node out of queue */
			union msm_audio_event_payload payload;
			struct audpcm_buffer_node *used_buf;

			MM_DBG("consumed\n");

			BUG_ON(list_empty(&audio->out_queue));
			used_buf = list_first_entry(&audio->out_queue,
				struct audpcm_buffer_node, list);
			list_del(&used_buf->list);
			payload.aio_buf = used_buf->buf;
			audpcm_post_event(audio, AUDIO_EVENT_WRITE_DONE,
				payload);
			kfree(used_buf);
			audio->drv_status &= ~ADRV_STATUS_OBUF_GIVEN;
		}
	}
	if (audio->out_needed) {
		struct audpcm_buffer_node *next_buf;
		struct audplay_cmd_bitstream_data_avail cmd;
		if (!list_empty(&audio->out_queue)) {
			next_buf = list_first_entry(&audio->out_queue,
					struct audpcm_buffer_node, list);
			MM_DBG("next_buf %p\n", next_buf);
			if (next_buf) {
				MM_DBG("next buf phy %lx len %d\n",
				next_buf->paddr, next_buf->buf.data_len);

				cmd.cmd_id = AUDPLAY_CMD_BITSTREAM_DATA_AVAIL;
				if (next_buf->buf.data_len)
					cmd.decoder_id = audio->dec_id;
				else {
					cmd.decoder_id = -1;
					MM_DBG("input EOS signaled\n");
				}
				cmd.buf_ptr	= (unsigned) next_buf->paddr;
				cmd.buf_size = next_buf->buf.data_len >> 1;
				cmd.partition_number	= 0;
				/* complete writes to the input buffer */
				wmb();
				audplay_send_queue0(audio, &cmd, sizeof(cmd));
				audio->out_needed = 0;
				audio->drv_status |= ADRV_STATUS_OBUF_GIVEN;
			}
		}
	}
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
}

static void audplay_send_data(struct audio *audio, unsigned needed)
{
	struct buffer *frame;
	unsigned long flags;

	if (!audio->running)
		return;

	spin_lock_irqsave(&audio->dsp_lock, flags);

	if (needed && !audio->wflush) {
		/* We were called from the callback because the DSP
		 * requested more data.  Note that the DSP does want
		 * more data, and if a buffer was in-flight, mark it
		 * as available (since the DSP must now be done with
		 * it).
		 */
		audio->out_needed = 1;
		frame = audio->out + audio->out_tail;
		if (frame->used == 0xffffffff) {
			MM_DBG("frame %d free\n", audio->out_tail);
			frame->used = 0;
			audio->out_tail ^= 1;
			wake_up(&audio->write_wait);
		}
	}

	if (audio->out_needed) {
		/* If the DSP currently wants data and we have a
		 * buffer available, we will send it and reset
		 * the needed flag.  We'll mark the buffer as in-flight
		 * so that it won't be recycled until the next buffer
		 * is requested
		 */

		frame = audio->out + audio->out_tail;
		if (frame->used) {
			BUG_ON(frame->used == 0xffffffff);
			MM_DBG("frame %d busy\n", audio->out_tail);
			audplay_dsp_send_data_avail(audio, audio->out_tail,
					frame->used);
			frame->used = 0xffffffff;
			audio->out_needed = 0;
		}
	}
	spin_unlock_irqrestore(&audio->dsp_lock, flags);
}

/* ------------------- device --------------------- */
static void audpcm_async_flush(struct audio *audio)
{
	struct audpcm_buffer_node *buf_node;
	struct list_head *ptr, *next;
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	list_for_each_safe(ptr, next, &audio->out_queue) {
		buf_node = list_entry(ptr, struct audpcm_buffer_node, list);
		list_del(&buf_node->list);
		payload.aio_buf = buf_node->buf;
		audpcm_post_event(audio, AUDIO_EVENT_WRITE_DONE,
				payload);
		kfree(buf_node);
	}
	audio->drv_status &= ~ADRV_STATUS_OBUF_GIVEN;
	audio->out_needed = 0;
	atomic_set(&audio->out_bytes, 0);
}

static void audio_flush(struct audio *audio)
{
	audio->out[0].used = 0;
	audio->out[1].used = 0;
	audio->out_head = 0;
	audio->out_tail = 0;
	audio->reserved = 0;
	audio->out_needed = 0;
	atomic_set(&audio->out_bytes, 0);
}

static void audio_ioport_reset(struct audio *audio)
{
	if (audio->drv_status & ADRV_STATUS_AIO_INTF) {
		/* If fsync is in progress, make sure
		 * return value of fsync indicates
		 * abort due to flush
		 */
		if (audio->drv_status & ADRV_STATUS_FSYNC) {
			MM_DBG("fsync in progress\n");
			wake_up(&audio->write_wait);
			mutex_lock(&audio->write_lock);
			audio->drv_ops.out_flush(audio);
			mutex_unlock(&audio->write_lock);
		} else
			audio->drv_ops.out_flush(audio);
	} else {
		/* Make sure read/write thread are free from
		 * sleep and knowing that system is not able
		 * to process io request at the moment
		 */
		wake_up(&audio->write_wait);
		mutex_lock(&audio->write_lock);
		audio->drv_ops.out_flush(audio);
		mutex_unlock(&audio->write_lock);
	}
	audio->avsync_flag = 1;
	wake_up(&audio->avsync_wait);
}

static int audpcm_events_pending(struct audio *audio)
{
	unsigned long flags;
	int empty;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	empty = !list_empty(&audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	return empty || audio->event_abort;
}

static void audpcm_reset_event_queue(struct audio *audio)
{
	unsigned long flags;
	struct audpcm_event *drv_evt;
	struct list_head *ptr, *next;

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	list_for_each_safe(ptr, next, &audio->event_queue) {
		drv_evt = list_first_entry(&audio->event_queue,
			struct audpcm_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	list_for_each_safe(ptr, next, &audio->free_event_queue) {
		drv_evt = list_first_entry(&audio->free_event_queue,
			struct audpcm_event, list);
		list_del(&drv_evt->list);
		kfree(drv_evt);
	}
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	return;
}

static long audpcm_process_event_req(struct audio *audio, void __user *arg)
{
	long rc;
	struct msm_audio_event usr_evt;
	struct audpcm_event *drv_evt = NULL;
	int timeout;
	unsigned long flags;

	if (copy_from_user(&usr_evt, arg, sizeof(struct msm_audio_event)))
		return -EFAULT;

	timeout = (int) usr_evt.timeout_ms;

	if (timeout > 0) {
		rc = wait_event_interruptible_timeout(
			audio->event_wait, audpcm_events_pending(audio),
			msecs_to_jiffies(timeout));
		if (rc == 0)
			return -ETIMEDOUT;
	} else {
		rc = wait_event_interruptible(
			audio->event_wait, audpcm_events_pending(audio));
	}

	if (rc < 0)
		return rc;

	if (audio->event_abort) {
		audio->event_abort = 0;
		return -ENODEV;
	}

	spin_lock_irqsave(&audio->event_queue_lock, flags);
	if (!list_empty(&audio->event_queue)) {
		drv_evt = list_first_entry(&audio->event_queue,
			struct audpcm_event, list);
		list_del(&drv_evt->list);
	}
	if (drv_evt) {
		usr_evt.event_type = drv_evt->event_type;
		usr_evt.event_payload = drv_evt->payload;
		list_add_tail(&drv_evt->list, &audio->free_event_queue);
	} else
		rc = -1;
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);

	if (drv_evt && drv_evt->event_type == AUDIO_EVENT_WRITE_DONE) {
		mutex_lock(&audio->lock);
		audpcm_pmem_fixup(audio, drv_evt->payload.aio_buf.buf_addr,
				  drv_evt->payload.aio_buf.buf_len, 0);
		mutex_unlock(&audio->lock);
	}
	if (!rc && copy_to_user(arg, &usr_evt, sizeof(usr_evt)))
		rc = -EFAULT;

	return rc;
}

static int audpcm_pmem_check(struct audio *audio,
		void *vaddr, unsigned long len)
{
	struct audpcm_pmem_region *region_elt;
	struct audpcm_pmem_region t = { .vaddr = vaddr, .len = len };

	list_for_each_entry(region_elt, &audio->pmem_region_queue, list) {
		if (CONTAINS(region_elt, &t) || CONTAINS(&t, region_elt) ||
		    OVERLAPS(region_elt, &t)) {
			MM_ERR("region (vaddr %p len %ld)"
				" clashes with registered region"
				" (vaddr %p paddr %p len %ld)\n",
				vaddr, len,
				region_elt->vaddr,
				(void *)region_elt->paddr,
				region_elt->len);
			return -EINVAL;
		}
	}

	return 0;
}

static int audpcm_pmem_add(struct audio *audio,
	struct msm_audio_pmem_info *info)
{
	unsigned long paddr, kvaddr, len;
	struct file *file;
	struct audpcm_pmem_region *region;
	int rc = -EINVAL;

	MM_DBG("\n"); /* Macro prints the file name and function */
	region = kmalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	if (get_pmem_file(info->fd, &paddr, &kvaddr, &len, &file)) {
		kfree(region);
		return -EINVAL;
	}

	rc = audpcm_pmem_check(audio, info->vaddr, len);
	if (rc < 0) {
		put_pmem_file(file);
		kfree(region);
		return rc;
	}

	region->vaddr = info->vaddr;
	region->fd = info->fd;
	region->paddr = paddr;
	region->kvaddr = kvaddr;
	region->len = len;
	region->file = file;
	region->ref_cnt = 0;
	MM_DBG("add region paddr %lx vaddr %p, len %lu\n", region->paddr,
			region->vaddr, region->len);
	list_add_tail(&region->list, &audio->pmem_region_queue);
	return rc;
}

static int audpcm_pmem_remove(struct audio *audio,
	struct msm_audio_pmem_info *info)
{
	struct audpcm_pmem_region *region;
	struct list_head *ptr, *next;
	int rc = -EINVAL;

	MM_DBG("info fd %d vaddr %p\n", info->fd, info->vaddr);

	list_for_each_safe(ptr, next, &audio->pmem_region_queue) {
		region = list_entry(ptr, struct audpcm_pmem_region, list);

		if ((region->fd == info->fd) &&
		    (region->vaddr == info->vaddr)) {
			if (region->ref_cnt) {
				MM_DBG("region %p in use ref_cnt %d\n", region,
						region->ref_cnt);
				break;
			}
			MM_DBG("remove region fd %d vaddr %p \n", info->fd,
					info->vaddr);
			list_del(&region->list);
			put_pmem_file(region->file);
			kfree(region);
			rc = 0;
			break;
		}
	}

	return rc;
}

static int audpcm_pmem_lookup_vaddr(struct audio *audio, void *addr,
		     unsigned long len, struct audpcm_pmem_region **region)
{
	struct audpcm_pmem_region *region_elt;

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
		MM_ERR("multiple hits for vaddr %p, len %ld\n", addr, len);
		list_for_each_entry(region_elt,
		  &audio->pmem_region_queue, list) {
			if (addr >= region_elt->vaddr &&
			    addr < region_elt->vaddr + region_elt->len &&
			    addr + len <= region_elt->vaddr + region_elt->len)
				MM_ERR("\t%p, %ld --> %p\n",
					region_elt->vaddr,
					region_elt->len,
					(void *)region_elt->paddr);
		}
	}

	return *region ? 0 : -1;
}

static unsigned long audpcm_pmem_fixup(struct audio *audio, void *addr,
		    unsigned long len, int ref_up)
{
	struct audpcm_pmem_region *region;
	unsigned long paddr;
	int ret;

	ret = audpcm_pmem_lookup_vaddr(audio, addr, len, &region);
	if (ret) {
		MM_ERR("lookup (%p, %ld) failed\n", addr, len);
		return 0;
	}
	if (ref_up)
		region->ref_cnt++;
	else
		region->ref_cnt--;
	MM_DBG("found region %p ref_cnt %d\n", region, region->ref_cnt);
	paddr = region->paddr + (addr - region->vaddr);
	return paddr;
}

/* audio -> lock must be held at this point */
static int audpcm_aio_buf_add(struct audio *audio, unsigned dir,
	void __user *arg)
{
	unsigned long flags;
	struct audpcm_buffer_node *buf_node;

	buf_node = kmalloc(sizeof(*buf_node), GFP_KERNEL);

	if (!buf_node)
		return -ENOMEM;

	if (copy_from_user(&buf_node->buf, arg, sizeof(buf_node->buf))) {
		kfree(buf_node);
		return -EFAULT;
	}

	MM_DBG("node %p dir %x buf_addr %p buf_len %d data_len %d\n",
			buf_node, dir, buf_node->buf.buf_addr,
			buf_node->buf.buf_len, buf_node->buf.data_len);

	buf_node->paddr = audpcm_pmem_fixup(
		audio, buf_node->buf.buf_addr,
		buf_node->buf.buf_len, 1);
	if (dir) {
		/* write */
		if (!buf_node->paddr ||
		    (buf_node->paddr & 0x1) ||
		    (buf_node->buf.data_len & 0x1) ||
		    (!buf_node->buf.data_len)) {
			kfree(buf_node);
			return -EINVAL;
		}
		spin_lock_irqsave(&audio->dsp_lock, flags);
		list_add_tail(&buf_node->list, &audio->out_queue);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		audio->drv_ops.send_data(audio, 0);
	}

	MM_DBG("Add buf_node %p paddr %lx\n", buf_node, buf_node->paddr);

	return 0;
}

static int audio_get_avsync_data(struct audio *audio,
						struct msm_audio_stats *stats)
{
	int rc = -EINVAL;
	unsigned long flags;

	local_irq_save(flags);
	if (audio->dec_id == audio->avsync[0] && audio->avsync_flag) {
		/* av_sync sample count */
		stats->sample_count = (audio->avsync[2] << 16) |
						(audio->avsync[3]);

		/* av_sync byte_count */
		stats->byte_count = (audio->avsync[5] << 16) |
						(audio->avsync[6]);

		audio->avsync_flag = 0;
		rc = 0;
	}
	local_irq_restore(flags);
	return rc;

}

static long audio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audio *audio = file->private_data;
	int rc = 0;

	MM_DBG("cmd = %d\n", cmd);

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;

		audio->avsync_flag = 0;
		memset(&stats, 0, sizeof(stats));
		if (audpp_query_avsync(audio->dec_id) < 0)
			return rc;

		rc = wait_event_interruptible_timeout(audio->avsync_wait,
				(audio->avsync_flag == 1),
				msecs_to_jiffies(AUDPP_AVSYNC_EVENT_TIMEOUT));

		if (rc < 0)
			return rc;
		else if ((rc > 0) || ((rc == 0) && (audio->avsync_flag == 1))) {
			if (audio_get_avsync_data(audio, &stats) < 0)
				return rc;

			if (copy_to_user((void *)arg, &stats, sizeof(stats)))
				return -EFAULT;
			return 0;
		} else
			return -EAGAIN;
	}
	if (cmd == AUDIO_SET_VOLUME) {
		unsigned long flags;
		spin_lock_irqsave(&audio->dsp_lock, flags);
		audio->volume = arg;
		if (audio->running)
			audpp_set_volume_and_pan(audio->dec_id, arg, 0,
					POPP);
		spin_unlock_irqrestore(&audio->dsp_lock, flags);
		return 0;
	}
	if (cmd == AUDIO_GET_EVENT) {
		MM_DBG("AUDIO_GET_EVENT\n");
		if (mutex_trylock(&audio->get_event_lock)) {
			rc = audpcm_process_event_req(audio,
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
		MM_DBG("AUDIO_START\n");
		rc = audio_enable(audio);
		if (!rc) {
			rc = wait_event_interruptible_timeout(audio->wait,
				audio->dec_state != MSM_AUD_DECODER_STATE_NONE,
				msecs_to_jiffies(MSM_AUD_DECODER_WAIT_MS));
			MM_INFO("dec_state %d rc = %d\n", audio->dec_state, rc);

			if (audio->dec_state != MSM_AUD_DECODER_STATE_SUCCESS)
				rc = -ENODEV;
			else
				rc = 0;
		}
		break;
	case AUDIO_STOP:
		MM_DBG("AUDIO_STOP\n");
		rc = audio_disable(audio);
		audio->stopped = 1;
		audio_ioport_reset(audio);
		audio->stopped = 0;
		break;
	case AUDIO_FLUSH:
		MM_DBG("AUDIO_FLUSH\n");
		audio->wflush = 1;
		audio_ioport_reset(audio);
		if (audio->running) {
			audpp_flush(audio->dec_id);
			rc = wait_event_interruptible(audio->write_wait,
				!audio->wflush);
			if (rc < 0) {
				MM_ERR("AUDIO_FLUSH interrupted\n");
				rc = -EINTR;
			}
		} else {
			audio->wflush = 0;
		}
		break;

	case AUDIO_SET_CONFIG: {
		struct msm_audio_config config;
		if (copy_from_user(&config, (void *) arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		if (config.channel_count == 1) {
			config.channel_count = AUDPP_CMD_PCM_INTF_MONO_V;
		} else if (config.channel_count == 2) {
			config.channel_count = AUDPP_CMD_PCM_INTF_STEREO_V;
		} else {
			rc = -EINVAL;
			break;
		}
		if (config.bits == 8)
			config.bits = AUDPP_CMD_WAV_PCM_WIDTH_8;
		else if (config.bits == 16)
			config.bits = AUDPP_CMD_WAV_PCM_WIDTH_16;
		else if (config.bits == 24)
			config.bits = AUDPP_CMD_WAV_PCM_WIDTH_24;
		else {
			rc = -EINVAL;
			break;
		}
		audio->out_sample_rate = config.sample_rate;
		audio->out_channel_mode = config.channel_count;
		audio->out_bits = config.bits;
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config config;
		config.buffer_size = (audio->out_dma_sz >> 1);
		config.buffer_count = 2;
		config.sample_rate = audio->out_sample_rate;
		if (audio->out_channel_mode == AUDPP_CMD_PCM_INTF_MONO_V)
			config.channel_count = 1;
		else
			config.channel_count = 2;
		if (audio->out_bits == AUDPP_CMD_WAV_PCM_WIDTH_8)
			config.bits = 8;
		else if (audio->out_bits == AUDPP_CMD_WAV_PCM_WIDTH_24)
			config.bits = 24;
		else
			config.bits = 16;
		config.unused[0] = 0;
		config.unused[1] = 0;

		if (copy_to_user((void *) arg, &config, sizeof(config)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	}

	case AUDIO_PAUSE:
		MM_DBG("AUDIO_PAUSE %ld\n", arg);
		rc = audpp_pause(audio->dec_id, (int) arg);
		break;

	case AUDIO_REGISTER_PMEM: {
			struct msm_audio_pmem_info info;
			MM_DBG("AUDIO_REGISTER_PMEM\n");
			if (copy_from_user(&info, (void *) arg, sizeof(info)))
				rc = -EFAULT;
			else
				rc = audpcm_pmem_add(audio, &info);
			break;
		}

	case AUDIO_DEREGISTER_PMEM: {
			struct msm_audio_pmem_info info;
			MM_DBG("AUDIO_DEREGISTER_PMEM\n");
			if (copy_from_user(&info, (void *) arg, sizeof(info)))
				rc = -EFAULT;
			else
				rc = audpcm_pmem_remove(audio, &info);
			break;
		}

	case AUDIO_ASYNC_WRITE:
		if (audio->drv_status & ADRV_STATUS_FSYNC)
			rc = -EBUSY;
		else
			rc = audpcm_aio_buf_add(audio, 1, (void __user *) arg);
		break;

	case AUDIO_ASYNC_READ:
		MM_ERR("AUDIO_ASYNC_READ not supported\n");
		rc = -EPERM;
		break;

	case AUDIO_GET_SESSION_ID:
		if (copy_to_user((void *) arg, &audio->dec_id,
					sizeof(unsigned short)))
			return -EFAULT;
		break;
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&audio->lock);
	return rc;
}

/* Only useful in tunnel-mode */
int audpcm_async_fsync(struct audio *audio)
{
	int rc = 0;

	MM_DBG("\n"); /* Macro prints the file name and function */

	/* Blocking client sends more data */
	mutex_lock(&audio->lock);
	audio->drv_status |= ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	mutex_lock(&audio->write_lock);
	/* pcm dmamiss message is sent continously
	 * when decoder is starved so no race
	 * condition concern
	 */
	audio->teos = 0;

	rc = wait_event_interruptible(audio->write_wait,
		(audio->teos && audio->out_needed &&
		list_empty(&audio->out_queue))
		|| audio->wflush || audio->stopped);

	if (audio->stopped || audio->wflush)
		rc = -EBUSY;

	mutex_unlock(&audio->write_lock);
	mutex_lock(&audio->lock);
	audio->drv_status &= ~ADRV_STATUS_FSYNC;
	mutex_unlock(&audio->lock);

	return rc;
}

int audpcm_sync_fsync(struct audio *audio)
{
	struct buffer *frame;
	int rc = 0;

	MM_DBG("\n"); /* Macro prints the file name and function */

	mutex_lock(&audio->write_lock);

	rc = wait_event_interruptible(audio->write_wait,
		(!audio->out[0].used &&
		!audio->out[1].used &&
		audio->out_needed) || audio->wflush);

	if (rc < 0)
		goto done;
	else if (audio->wflush) {
		rc = -EBUSY;
		goto done;
	}

	if (audio->reserved) {
		MM_DBG("send reserved byte\n");
		frame = audio->out + audio->out_tail;
		((char *) frame->data)[0] = audio->rsv_byte;
		((char *) frame->data)[1] = 0;
		frame->used = 2;
		audio->drv_ops.send_data(audio, 0);

		rc = wait_event_interruptible(audio->write_wait,
			(!audio->out[0].used &&
			!audio->out[1].used &&
			audio->out_needed) || audio->wflush);

		if (rc < 0)
			goto done;
		else if (audio->wflush) {
			rc = -EBUSY;
			goto done;
		}
	}

	/* pcm dmamiss message is sent continously
	 * when decoder is starved so no race
	 * condition concern
	 */
	audio->teos = 0;

	rc = wait_event_interruptible(audio->write_wait,
		audio->teos || audio->wflush);

	if (audio->wflush)
		rc = -EBUSY;

done:
	mutex_unlock(&audio->write_lock);
	return rc;
}

int audpcm_fsync(struct file *file, int datasync)
{
	struct audio *audio = file->private_data;

	if (!audio->running)
		return -EINVAL;

	return audio->drv_ops.fsync(audio);
}

static ssize_t audio_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *pos)
{
	struct audio *audio = file->private_data;
	const char __user *start = buf;
	struct buffer *frame;
	size_t xfer;
	char *cpy_ptr;
	int rc = 0;
	unsigned dsize;

	if (audio->drv_status & ADRV_STATUS_AIO_INTF)
		return -EPERM;

	MM_DBG("cnt=%d\n", count);

	mutex_lock(&audio->write_lock);
	while (count > 0) {
		frame = audio->out + audio->out_head;
		cpy_ptr = frame->data;
		dsize = 0;
		rc = wait_event_interruptible(audio->write_wait,
					      (frame->used == 0)
					      || (audio->stopped)
						  || (audio->wflush));
		if (rc < 0)
			break;
		if (audio->stopped || audio->wflush) {
			rc = -EBUSY;
			break;
		}

		if (audio->reserved) {
			MM_DBG("append reserved byte %x\n", audio->rsv_byte);
			*cpy_ptr = audio->rsv_byte;
			xfer = (count > (frame->size - 1)) ?
				frame->size - 1 : count;
			cpy_ptr++;
			dsize = 1;
			audio->reserved = 0;
		} else
			xfer = (count > frame->size) ? frame->size : count;

		if (copy_from_user(cpy_ptr, buf, xfer)) {
			rc = -EFAULT;
			break;
		}

		dsize += xfer;
		if (dsize & 1) {
			audio->rsv_byte = ((char *) frame->data)[dsize - 1];
			MM_DBG("odd length buf reserve last byte %x\n",
					audio->rsv_byte);
			audio->reserved = 1;
			dsize--;
		}
		count -= xfer;
		buf += xfer;

		if (dsize > 0) {
			audio->out_head ^= 1;
			frame->used = dsize;
			audio->drv_ops.send_data(audio, 0);
		}
	}
	mutex_unlock(&audio->write_lock);
	if (buf > start)
		return buf - start;

	return rc;
}

static void audpcm_reset_pmem_region(struct audio *audio)
{
	struct audpcm_pmem_region *region;
	struct list_head *ptr, *next;

	list_for_each_safe(ptr, next, &audio->pmem_region_queue) {
		region = list_entry(ptr, struct audpcm_pmem_region, list);
		list_del(&region->list);
		put_pmem_file(region->file);
		kfree(region);
	}

	return;
}

static int audio_release(struct inode *inode, struct file *file)
{
	struct audio *audio = file->private_data;

	MM_INFO("audio instance 0x%08x freeing\n", (int)audio);

	mutex_lock(&audio->lock);
	auddev_unregister_evt_listner(AUDDEV_CLNT_DEC, audio->dec_id);
	audio_disable(audio);
	audio->drv_ops.out_flush(audio);
	audpcm_reset_pmem_region(audio);

	msm_adsp_put(audio->audplay);
	audpp_adec_free(audio->dec_id);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&audio->suspend_ctl.node);
#endif
	audio->opened = 0;
	audio->event_abort = 1;
	wake_up(&audio->event_wait);
	audpcm_reset_event_queue(audio);
	if (audio->data) {
		msm_subsystem_unmap_buffer(audio->map_v_write);
		free_contiguous_memory_by_paddr(audio->phys);
	}
	mutex_unlock(&audio->lock);
#ifdef CONFIG_DEBUG_FS
	if (audio->dentry)
		debugfs_remove(audio->dentry);
#endif
	kfree(audio);
	return 0;
}

static void audpcm_post_event(struct audio *audio, int type,
	union msm_audio_event_payload payload)
{
	struct audpcm_event *e_node = NULL;
	unsigned long flags;

	spin_lock_irqsave(&audio->event_queue_lock, flags);

	if (!list_empty(&audio->free_event_queue)) {
		e_node = list_first_entry(&audio->free_event_queue,
			struct audpcm_event, list);
		list_del(&e_node->list);
	} else {
		e_node = kmalloc(sizeof(struct audpcm_event), GFP_ATOMIC);
		if (!e_node) {
			MM_ERR("No mem to post event %d\n", type);
			return;
		}
	}

	e_node->event_type = type;
	e_node->payload = payload;

	list_add_tail(&e_node->list, &audio->event_queue);
	spin_unlock_irqrestore(&audio->event_queue_lock, flags);
	wake_up(&audio->event_wait);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void audpcm_suspend(struct early_suspend *h)
{
	struct audpcm_suspend_ctl *ctl =
		container_of(h, struct audpcm_suspend_ctl, node);
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	audpcm_post_event(ctl->audio, AUDIO_EVENT_SUSPEND, payload);
}

static void audpcm_resume(struct early_suspend *h)
{
	struct audpcm_suspend_ctl *ctl =
		container_of(h, struct audpcm_suspend_ctl, node);
	union msm_audio_event_payload payload;

	MM_DBG("\n"); /* Macro prints the file name and function */
	audpcm_post_event(ctl->audio, AUDIO_EVENT_RESUME, payload);
}
#endif

#ifdef CONFIG_DEBUG_FS
static ssize_t audpcm_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t audpcm_debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	const int debug_bufmax = 4096;
	static char buffer[4096];
	int n = 0;
	struct audio *audio = file->private_data;

	mutex_lock(&audio->lock);
	n = scnprintf(buffer, debug_bufmax, "opened %d\n", audio->opened);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "enabled %d\n", audio->enabled);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "stopped %d\n", audio->stopped);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "out_buf_sz %d\n", audio->out[0].size);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "volume %x \n", audio->volume);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "sample rate %d \n", audio->out_sample_rate);
	n += scnprintf(buffer + n, debug_bufmax - n,
		"channel mode %d \n", audio->out_channel_mode);
	mutex_unlock(&audio->lock);
	/* Following variables are only useful for debugging when
	 * when playback halts unexpectedly. Thus, no mutual exclusion
	 * enforced
	 */
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "wflush %d\n", audio->wflush);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "running %d \n", audio->running);
	n += scnprintf(buffer + n, debug_bufmax - n,
				"dec state %d \n", audio->dec_state);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "out_needed %d \n", audio->out_needed);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "out_head %d \n", audio->out_head);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "out_tail %d \n", audio->out_tail);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "out[0].used %d \n", audio->out[0].used);
	n += scnprintf(buffer + n, debug_bufmax - n,
				   "out[1].used %d \n", audio->out[1].used);
	buffer[n] = 0;
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static const struct file_operations audpcm_debug_fops = {
	.read = audpcm_debug_read,
	.open = audpcm_debug_open,
};
#endif

static int audio_open(struct inode *inode, struct file *file)
{
	struct audio *audio = NULL;
	int rc, i, dec_attrb, decid;
	struct audpcm_event *e_node = NULL;
	unsigned pmem_sz = DMASZ_MAX;

#ifdef CONFIG_DEBUG_FS
	/* 4 bytes represents decoder number, 1 byte for terminate string */
	char name[sizeof "msm_pcm_dec_" + 5];
#endif

	/* Allocate audio instance, set to zero */
	audio = kzalloc(sizeof(struct audio), GFP_KERNEL);
	if (!audio) {
		MM_ERR("no memory to allocate audio instance \n");
		rc = -ENOMEM;
		goto done;
	}
	MM_INFO("audio instance 0x%08x created\n", (int)audio);

	/* Allocate the decoder */
	dec_attrb = AUDDEC_DEC_PCM;
	if (file->f_mode & FMODE_READ) {
		MM_ERR("Non-Tunneled mode not supported\n");
		rc = -EPERM;
		kfree(audio);
		goto done;
	} else
		dec_attrb |= MSM_AUD_MODE_TUNNEL;

	decid = audpp_adec_alloc(dec_attrb, &audio->module_name,
			&audio->queue_id);
	if (decid < 0) {
		MM_ERR("No free decoder available, freeing instance 0x%08x\n",
				(int)audio);
		rc = -ENODEV;
		kfree(audio);
		goto done;
	}
	audio->dec_id = decid & MSM_AUD_DECODER_MASK;

	/* AIO interface */
	if (file->f_flags & O_NONBLOCK) {
		MM_DBG("set to aio interface\n");
		audio->drv_status |= ADRV_STATUS_AIO_INTF;
		audio->drv_ops.send_data = audpcm_async_send_data;
		audio->drv_ops.out_flush = audpcm_async_flush;
		audio->drv_ops.fsync = audpcm_async_fsync;
	} else {
		MM_DBG("set to std io interface\n");
		while (pmem_sz >= DMASZ_MIN) {
			MM_DBG("pmemsz = %d\n", pmem_sz);
			audio->phys = allocate_contiguous_ebi_nomap(pmem_sz,
								SZ_4K);
			if (audio->phys) {
				audio->map_v_write = msm_subsystem_map_buffer(
							audio->phys, pmem_sz,
							MSM_SUBSYSTEM_MAP_KADDR
							, NULL, 0);
				if (IS_ERR(audio->map_v_write)) {
					MM_ERR("could not map write phys\
						address freeing instance \
						0x%08x\n", (int)audio);
					rc = -ENOMEM;
					free_contiguous_memory_by_paddr(
								audio->phys);
					audpp_adec_free(audio->dec_id);
					kfree(audio);
					goto done;
				}
				audio->data = audio->map_v_write->vaddr;
				MM_DBG("write buf: phy addr 0x%08x \
						kernel addr 0x%08x\n",
						audio->phys, (int)audio->data);
				break;
			} else if (pmem_sz == DMASZ_MIN) {
				MM_ERR("could not allocate write buffers \
					freeing instance 0x%08x\n", (int)audio);
				rc = -ENOMEM;
				audpp_adec_free(audio->dec_id);
				kfree(audio);
				goto done;
			} else
				pmem_sz >>= 1;
		}
		audio->out_dma_sz = pmem_sz;
		audio->drv_ops.send_data = audplay_send_data;
		audio->drv_ops.out_flush = audio_flush;
		audio->drv_ops.fsync = audpcm_sync_fsync;
		audio->out[0].data = audio->data + 0;
		audio->out[0].addr = audio->phys + 0;
		audio->out[0].size = (audio->out_dma_sz >> 1);

		audio->out[1].data = audio->data + audio->out[0].size;
		audio->out[1].addr = audio->phys + audio->out[0].size;
		audio->out[1].size = audio->out[0].size;
	}

	rc = msm_adsp_get(audio->module_name, &audio->audplay,
			&audpcmdec_adsp_ops, audio);
	if (rc) {
		MM_ERR("failed to get %s module, freeing instance 0x%08x\n",
				audio->module_name, (int)audio);
		goto err;
	}

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
	init_waitqueue_head(&audio->avsync_wait);

	audio->out_sample_rate = 44100;
	audio->out_channel_mode = AUDPP_CMD_PCM_INTF_STEREO_V;
	audio->out_bits = AUDPP_CMD_WAV_PCM_WIDTH_16;
	audio->volume = 0x7FFF;
	audio->drv_ops.out_flush(audio);

	file->private_data = audio;
	audio->opened = 1;

	audio->device_events = AUDDEV_EVT_DEV_RDY
				|AUDDEV_EVT_DEV_RLS|
				AUDDEV_EVT_STREAM_VOL_CHG;

	rc = auddev_register_evt_listner(audio->device_events,
					AUDDEV_CLNT_DEC,
					audio->dec_id,
					pcm_listner,
					(void *)audio);
	if (rc) {
		MM_ERR("failed to register listnet\n");
		goto event_err;
	}

#ifdef CONFIG_DEBUG_FS
	snprintf(name, sizeof name, "msm_pcm_dec_%04x", audio->dec_id);
	audio->dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
		NULL, (void *) audio, &audpcm_debug_fops);

	if (IS_ERR(audio->dentry))
		MM_ERR("debugfs_create_file failed\n");
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	audio->suspend_ctl.node.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	audio->suspend_ctl.node.resume = audpcm_resume;
	audio->suspend_ctl.node.suspend = audpcm_suspend;
	audio->suspend_ctl.audio = audio;
	register_early_suspend(&audio->suspend_ctl.node);
#endif
	for (i = 0; i < AUDPCM_EVENT_NUM; i++) {
		e_node = kmalloc(sizeof(struct audpcm_event), GFP_KERNEL);
		if (e_node)
			list_add_tail(&e_node->list, &audio->free_event_queue);
		else {
			MM_ERR("event pkt alloc failed\n");
			break;
		}
	}
done:
	return rc;
event_err:
	msm_adsp_put(audio->audplay);
err:
	if (audio->data) {
		msm_subsystem_unmap_buffer(audio->map_v_write);
		free_contiguous_memory_by_paddr(audio->phys);
	}
	audpp_adec_free(audio->dec_id);
	kfree(audio);
	return rc;
}

static const struct file_operations audio_pcm_fops = {
	.owner		= THIS_MODULE,
	.open		= audio_open,
	.release	= audio_release,
	.write		= audio_write,
	.unlocked_ioctl	= audio_ioctl,
	.fsync = audpcm_fsync,
};

struct miscdevice audio_pcm_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_dec",
	.fops	= &audio_pcm_fops,
};

static int __init audio_init(void)
{
	return misc_register(&audio_pcm_misc);
}

device_initcall(audio_init);

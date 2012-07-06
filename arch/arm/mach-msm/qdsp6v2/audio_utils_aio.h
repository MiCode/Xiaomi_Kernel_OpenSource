/* Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/ion.h>
#include <asm/ioctls.h>
#include <asm/atomic.h>
#include "q6audio_common.h"

#define TUNNEL_MODE     0x0000
#define NON_TUNNEL_MODE 0x0001

#define ADRV_STATUS_AIO_INTF 0x00000001 /* AIO interface */
#define ADRV_STATUS_FSYNC 0x00000008
#define ADRV_STATUS_PAUSE 0x00000010
#define AUDIO_DEC_EOS_SET  0x00000001
#define AUDIO_EVENT_NUM		10

#define __CONTAINS(r, v, l) ({                                  \
	typeof(r) __r = r;                                      \
	typeof(v) __v = v;                                      \
	typeof(v) __e = __v + l;                                \
	int res = ((__v >= __r->vaddr) &&                       \
		(__e <= __r->vaddr + __r->len));                \
	res;                                                    \
})

#define CONTAINS(r1, r2) ({                                     \
	typeof(r2) __r2 = r2;                                   \
	__CONTAINS(r1, __r2->vaddr, __r2->len);                 \
})

#define IN_RANGE(r, v) ({                                       \
	typeof(r) __r = r;                                      \
	typeof(v) __vv = v;                                     \
	int res = ((__vv >= __r->vaddr) &&                      \
		(__vv < (__r->vaddr + __r->len)));              \
	res;                                                    \
})

#define OVERLAPS(r1, r2) ({                                     \
	typeof(r1) __r1 = r1;                                   \
	typeof(r2) __r2 = r2;                                   \
	typeof(__r2->vaddr) __v = __r2->vaddr;                  \
	typeof(__v) __e = __v + __r2->len - 1;                  \
	int res = (IN_RANGE(__r1, __v) || IN_RANGE(__r1, __e)); \
	res;                                                    \
})

struct timestamp {
	unsigned long lowpart;
	unsigned long highpart;
} __packed;

struct meta_out_dsp {
	u32 offset_to_frame;
	u32 frame_size;
	u32 encoded_pcm_samples;
	u32 msw_ts;
	u32 lsw_ts;
	u32 nflags;
} __packed;

struct dec_meta_in {
	unsigned char reserved[18];
	unsigned short offset;
	struct timestamp ntimestamp;
	unsigned int nflags;
} __packed;

struct dec_meta_out {
	unsigned int reserved[7];
	unsigned int num_of_frames;
	struct meta_out_dsp meta_out_dsp[];
} __packed;

/* General meta field to store meta info
locally */
union  meta_data {
	struct dec_meta_out meta_out;
	struct dec_meta_in meta_in;
} __packed;

#define PCM_BUF_COUNT           (2)
/* Buffer with meta */
#define PCM_BUFSZ_MIN           ((4*1024) + sizeof(struct dec_meta_out))

/* FRAME_NUM must be a power of two */
#define FRAME_NUM               (2)
#define FRAME_SIZE	((4*1536) + sizeof(struct dec_meta_in))

struct audio_aio_ion_region {
	struct list_head list;
	struct ion_handle *handle;
	int fd;
	void *vaddr;
	unsigned long paddr;
	unsigned long kvaddr;
	unsigned long len;
	unsigned ref_cnt;
};

struct audio_aio_event {
	struct list_head list;
	int event_type;
	union msm_audio_event_payload payload;
};

struct audio_aio_buffer_node {
	struct list_head list;
	struct msm_audio_aio_buf buf;
	unsigned long paddr;
	unsigned long token;
	void            *kvaddr;
	union meta_data meta_info;
};

struct q6audio_aio;
struct audio_aio_drv_operations {
	void (*out_flush) (struct q6audio_aio *);
	void (*in_flush) (struct q6audio_aio *);
};

struct q6audio_aio {
	atomic_t in_bytes;
	atomic_t in_samples;

	struct msm_audio_stream_config str_cfg;
	struct msm_audio_buf_cfg        buf_cfg;
	struct msm_audio_config pcm_cfg;
	void *codec_cfg;

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
	struct list_head out_queue;     /* queue to retain output buffers */
	struct list_head in_queue;      /* queue to retain input buffers */
	struct list_head free_event_queue;
	struct list_head event_queue;
	struct list_head ion_region_queue;     /* protected by lock */
	struct ion_client *client;
	struct audio_aio_drv_operations drv_ops;
	union msm_audio_event_payload eos_write_payload;

	uint32_t drv_status;
	int event_abort;
	int eos_rsp;
	int eos_flag;
	int opened;
	int enabled;
	int stopped;
	int feedback;
	int rflush;             /* Read  flush */
	int wflush;             /* Write flush */
	long (*codec_ioctl)(struct file *, unsigned int, unsigned long);
};

void audio_aio_async_write_ack(struct q6audio_aio *audio, uint32_t token,
				uint32_t *payload);

void audio_aio_async_read_ack(struct q6audio_aio *audio, uint32_t token,
			uint32_t *payload);

int insert_eos_buf(struct q6audio_aio *audio,
		struct audio_aio_buffer_node *buf_node);

void extract_meta_out_info(struct q6audio_aio *audio,
		struct audio_aio_buffer_node *buf_node, int dir);

int audio_aio_open(struct q6audio_aio *audio, struct file *file);
int audio_aio_enable(struct q6audio_aio  *audio);
void audio_aio_post_event(struct q6audio_aio *audio, int type,
		union msm_audio_event_payload payload);
int audio_aio_release(struct inode *inode, struct file *file);
long audio_aio_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int audio_aio_fsync(struct file *file, loff_t start, loff_t end, int datasync);
void audio_aio_async_out_flush(struct q6audio_aio *audio);
void audio_aio_async_in_flush(struct q6audio_aio *audio);
#ifdef CONFIG_DEBUG_FS
ssize_t audio_aio_debug_open(struct inode *inode, struct file *file);
ssize_t audio_aio_debug_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos);
#endif

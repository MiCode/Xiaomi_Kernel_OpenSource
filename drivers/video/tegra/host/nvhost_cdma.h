/*
 * drivers/video/tegra/host/nvhost_cdma.h
 *
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_CDMA_H
#define __NVHOST_CDMA_H

#include <linux/sched.h>
#include <linux/semaphore.h>

#include <linux/nvhost.h>
#include <linux/list.h>

struct nvhost_syncpt;
struct nvhost_userctx_timeout;
struct nvhost_job;
struct mem_mgr;
struct mem_handle;

/*
 * cdma
 *
 * This is in charge of a host command DMA channel.
 * Sends ops to a push buffer, and takes responsibility for unpinning
 * (& possibly freeing) of memory after those ops have completed.
 * Producer:
 *	begin
 *		push - send ops to the push buffer
 *	end - start command DMA and enqueue handles to be unpinned
 * Consumer:
 *	update - call to update sync queue and push buffer, unpin memory
 */

struct mem_mgr_handle {
	struct mem_mgr *client;
	struct mem_handle *handle;
};

struct push_buffer {
	struct mem_handle *mem;		/* handle to pushbuffer memory */
	u32 *mapped;			/* mapped pushbuffer memory */
	struct sg_table *sgt;
	dma_addr_t phys;		/* physical address of pushbuffer */
	u32 fence;			/* index we've written */
	u32 cur;			/* index to write to */
	struct mem_mgr_handle *client_handle; /* handle for each opcode pair */
};

struct buffer_timeout {
	struct delayed_work wq;		/* work queue */
	bool initialized;		/* timer one-time setup flag */
	u32 syncpt_id;			/* buffer completion syncpt id */
	u32 syncpt_val;			/* syncpt value when completed */
	ktime_t start_ktime;		/* starting time */
	/* context timeout information */
	struct nvhost_hwctx *ctx;
	int clientid;
	bool timeout_debug_dump;
};

enum cdma_event {
	CDMA_EVENT_NONE,		/* not waiting for any event */
	CDMA_EVENT_SYNC_QUEUE_EMPTY,	/* wait for empty sync queue */
	CDMA_EVENT_PUSH_BUFFER_SPACE	/* wait for space in push buffer */
};

struct nvhost_cdma {
	struct mutex lock;		/* controls access to shared state */
	struct semaphore sem;		/* signalled when event occurs */
	enum cdma_event event;		/* event that sem is waiting for */
	unsigned int slots_used;	/* pb slots used in current submit */
	unsigned int slots_free;	/* pb slots free in current submit */
	unsigned int first_get;		/* DMAGET value, where submit begins */
	unsigned int last_put;		/* last value written to DMAPUT */
	struct push_buffer push_buffer;	/* channel's push buffer */
	struct list_head sync_queue;	/* job queue */
	struct buffer_timeout timeout;	/* channel's timeout state/wq */
	bool running;
	bool torndown;
	int high_prio_count;
	int med_prio_count;
	int low_prio_count;
};

#define cdma_to_channel(cdma) container_of(cdma, struct nvhost_channel, cdma)
#define cdma_to_dev(cdma) nvhost_get_host(cdma_to_channel(cdma)->dev)
#define cdma_to_memmgr(cdma) ((cdma_to_dev(cdma))->memmgr)
#define pb_to_cdma(pb) container_of(pb, struct nvhost_cdma, push_buffer)

int	nvhost_cdma_init(struct nvhost_cdma *cdma);
void	nvhost_cdma_deinit(struct nvhost_cdma *cdma);
void	nvhost_cdma_stop(struct nvhost_cdma *cdma);
int	nvhost_cdma_begin(struct nvhost_cdma *cdma, struct nvhost_job *job);
void	nvhost_cdma_push(struct nvhost_cdma *cdma, u32 op1, u32 op2);
void	nvhost_cdma_push_gather(struct nvhost_cdma *cdma,
		struct mem_mgr *client,
		struct mem_handle *handle, u32 offset, u32 op1, u32 op2);
void	nvhost_cdma_end(struct nvhost_cdma *cdma,
		struct nvhost_job *job);
void	nvhost_cdma_update(struct nvhost_cdma *cdma);
int	nvhost_cdma_flush(struct nvhost_cdma *cdma, int timeout);
void	nvhost_cdma_peek(struct nvhost_cdma *cdma,
		u32 dmaget, int slot, u32 *out);
unsigned int nvhost_cdma_wait_locked(struct nvhost_cdma *cdma,
		enum cdma_event event);
void nvhost_cdma_update_sync_queue(struct nvhost_cdma *cdma,
		struct nvhost_syncpt *syncpt, struct platform_device *dev);
#endif

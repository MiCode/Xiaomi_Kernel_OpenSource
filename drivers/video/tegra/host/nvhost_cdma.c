/*
 * drivers/video/tegra/host/nvhost_cdma.c
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

#include "nvhost_cdma.h"
#include "nvhost_channel.h"
#include "nvhost_job.h"
#include "nvhost_hwctx.h"
#include "dev.h"
#include "debug.h"
#include "nvhost_memmgr.h"
#include "chip_support.h"
#include <asm/cacheflush.h>

#include <linux/slab.h>
#include <linux/kfifo.h>
#include <trace/events/nvhost.h>
#include <linux/interrupt.h>

/*
 * TODO:
 *   stats
 *     - for figuring out what to optimize further
 *   resizable push buffer
 *     - some channels hardly need any, some channels (3d) could use more
 */

/**
 * Add an entry to the sync queue.
 */
static void add_to_sync_queue(struct nvhost_cdma *cdma,
			      struct nvhost_job *job,
			      u32 nr_slots,
			      u32 first_get)
{
	BUG_ON(job->syncpt_id == NVSYNCPT_INVALID);

	job->first_get = first_get;
	job->num_slots = nr_slots;
	nvhost_job_get(job);
	list_add_tail(&job->list, &cdma->sync_queue);

	switch (job->priority) {
	case NVHOST_PRIORITY_HIGH:
		cdma->high_prio_count++;
		break;
	case NVHOST_PRIORITY_MEDIUM:
		cdma->med_prio_count++;
		break;
	case NVHOST_PRIORITY_LOW:
		cdma->low_prio_count++;
		break;
	}
}

/**
 * Return the status of the cdma's sync queue or push buffer for the given event
 *  - sq empty: returns 1 for empty, 0 for not empty (as in "1 empty queue" :-)
 *  - pb space: returns the number of free slots in the channel's push buffer
 * Must be called with the cdma lock held.
 */
static unsigned int cdma_status_locked(struct nvhost_cdma *cdma,
		enum cdma_event event)
{
	switch (event) {
	case CDMA_EVENT_SYNC_QUEUE_EMPTY:
		return list_empty(&cdma->sync_queue) ? 1 : 0;
	case CDMA_EVENT_PUSH_BUFFER_SPACE: {
		struct push_buffer *pb = &cdma->push_buffer;
		BUG_ON(!cdma_pb_op().space);
		return cdma_pb_op().space(pb);
	}
	default:
		return 0;
	}
}

/**
 * Sleep (if necessary) until the requested event happens
 *   - CDMA_EVENT_SYNC_QUEUE_EMPTY : sync queue is completely empty.
 *     - Returns 1
 *   - CDMA_EVENT_PUSH_BUFFER_SPACE : there is space in the push buffer
 *     - Return the amount of space (> 0)
 * Must be called with the cdma lock held.
 */
unsigned int nvhost_cdma_wait_locked(struct nvhost_cdma *cdma,
		enum cdma_event event)
{
	for (;;) {
		unsigned int space = cdma_status_locked(cdma, event);
		if (space)
			return space;

		trace_nvhost_wait_cdma(cdma_to_channel(cdma)->dev->name,
				event);

		/* If somebody has managed to already start waiting, yield */
		if (cdma->event != CDMA_EVENT_NONE) {
			mutex_unlock(&cdma->lock);
			schedule();
			mutex_lock(&cdma->lock);
			continue;
		}
		cdma->event = event;

		mutex_unlock(&cdma->lock);
		down(&cdma->sem);
		mutex_lock(&cdma->lock);
	}
	return 0;
}

/**
 * Start timer for a buffer submition that has completed yet.
 * Must be called with the cdma lock held.
 */
static void cdma_start_timer_locked(struct nvhost_cdma *cdma,
		struct nvhost_job *job)
{
	BUG_ON(!job);
	if (cdma->timeout.clientid) {
		/* timer already started */
		return;
	}

	cdma->timeout.ctx = job->hwctx;
	cdma->timeout.clientid = job->clientid;
	cdma->timeout.syncpt_id = job->syncpt_id;
	cdma->timeout.syncpt_val = job->syncpt_end;
	cdma->timeout.start_ktime = ktime_get();
	cdma->timeout.timeout_debug_dump = job->timeout_debug_dump;

	schedule_delayed_work(&cdma->timeout.wq,
			msecs_to_jiffies(job->timeout));
}

/**
 * Stop timer when a buffer submition completes.
 * Must be called with the cdma lock held.
 */
static void stop_cdma_timer_locked(struct nvhost_cdma *cdma)
{
	cancel_delayed_work(&cdma->timeout.wq);
	cdma->timeout.ctx = NULL;
	cdma->timeout.clientid = 0;
}

/**
 * For all sync queue entries that have already finished according to the
 * current sync point registers:
 *  - unpin & unref their mems
 *  - pop their push buffer slots
 *  - remove them from the sync queue
 * This is normally called from the host code's worker thread, but can be
 * called manually if necessary.
 * Must be called with the cdma lock held.
 */
static void update_cdma_locked(struct nvhost_cdma *cdma)
{
	bool signal = false;
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_syncpt *sp = &dev->syncpt;
	struct nvhost_job *job, *n;

	/* If CDMA is stopped, queue is cleared and we can return */
	if (!cdma->running)
		return;

	/*
	 * Walk the sync queue, reading the sync point registers as necessary,
	 * to consume as many sync queue entries as possible without blocking
	 */
	list_for_each_entry_safe(job, n, &cdma->sync_queue, list) {
		BUG_ON(job->syncpt_id == NVSYNCPT_INVALID);

		/* Check whether this syncpt has completed, and bail if not */
		if (!nvhost_syncpt_is_expired(sp,
				job->syncpt_id, job->syncpt_end)) {
			/* Start timer on next pending syncpt */
			if (job->timeout)
				cdma_start_timer_locked(cdma, job);
			break;
		}

		/* Cancel timeout, when a buffer completes */
		if (cdma->timeout.clientid)
			stop_cdma_timer_locked(cdma);

		/* Unpin the memory */
		nvhost_job_unpin(job);

		/* Pop push buffer slots */
		if (job->num_slots) {
			struct push_buffer *pb = &cdma->push_buffer;
			BUG_ON(!cdma_pb_op().pop_from);
			cdma_pb_op().pop_from(pb, job->num_slots);
			if (cdma->event == CDMA_EVENT_PUSH_BUFFER_SPACE)
				signal = true;
		}

		list_del(&job->list);

		switch (job->priority) {
		case NVHOST_PRIORITY_HIGH:
			cdma->high_prio_count--;
			break;
		case NVHOST_PRIORITY_MEDIUM:
			cdma->med_prio_count--;
			break;
		case NVHOST_PRIORITY_LOW:
			cdma->low_prio_count--;
			break;
		}

		nvhost_job_put(job);
	}

	if (list_empty(&cdma->sync_queue) &&
				cdma->event == CDMA_EVENT_SYNC_QUEUE_EMPTY)
			signal = true;

	/* Wake up CdmaWait() if the requested event happened */
	if (signal) {
		cdma->event = CDMA_EVENT_NONE;
		up(&cdma->sem);
	}
}

void nvhost_cdma_update_sync_queue(struct nvhost_cdma *cdma,
		struct nvhost_syncpt *syncpt, struct platform_device *dev)
{
	u32 get_restart;
	u32 syncpt_incrs;
	struct nvhost_job *job = NULL;
	u32 syncpt_val;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	syncpt_val = nvhost_syncpt_update_min(syncpt, cdma->timeout.syncpt_id);

	dev_dbg(&dev->dev,
		"%s: starting cleanup (thresh %d)\n",
		__func__, syncpt_val);

	/*
	 * Move the sync_queue read pointer to the first entry that hasn't
	 * completed based on the current HW syncpt value. It's likely there
	 * won't be any (i.e. we're still at the head), but covers the case
	 * where a syncpt incr happens just prior/during the teardown.
	 */

	dev_dbg(&dev->dev,
		"%s: skip completed buffers still in sync_queue\n",
		__func__);

	list_for_each_entry(job, &cdma->sync_queue, list) {
		if (syncpt_val < job->syncpt_end)
			break;

		nvhost_job_dump(&dev->dev, job);
	}

	/*
	 * Walk the sync_queue, first incrementing with the CPU syncpts that
	 * are partially executed (the first buffer) or fully skipped while
	 * still in the current context (slots are also NOP-ed).
	 *
	 * At the point contexts are interleaved, syncpt increments must be
	 * done inline with the pushbuffer from a GATHER buffer to maintain
	 * the order (slots are modified to be a GATHER of syncpt incrs).
	 *
	 * Note: save in get_restart the location where the timed out buffer
	 * started in the PB, so we can start the refetch from there (with the
	 * modified NOP-ed PB slots). This lets things appear to have completed
	 * properly for this buffer and resources are freed.
	 */

	dev_dbg(&dev->dev,
		"%s: perform CPU incr on pending same ctx buffers\n",
		__func__);

	get_restart = cdma->last_put;
	if (!list_empty(&cdma->sync_queue))
		get_restart = job->first_get;

	/* do CPU increments as long as this context continues */
	list_for_each_entry_from(job, &cdma->sync_queue, list) {
		/* different context, gets us out of this loop */
		if (job->clientid != cdma->timeout.clientid)
			break;

		nvhost_job_dump(&dev->dev, job);

		/* won't need a timeout when replayed */
		job->timeout = 0;

		syncpt_incrs = job->syncpt_end - syncpt_val;
		dev_dbg(&dev->dev,
			"%s: CPU incr (%d)\n", __func__, syncpt_incrs);

		/* safe to use CPU to incr syncpts */
		cdma_op().timeout_cpu_incr(cdma,
				job->first_get,
				syncpt_incrs,
				job->syncpt_end,
				job->num_slots,
				pdata->waitbases);

		syncpt_val += syncpt_incrs;
	}

	list_for_each_entry_from(job, &cdma->sync_queue, list)
		if (job->clientid == cdma->timeout.clientid)
			job->timeout = 500;

	dev_dbg(&dev->dev,
		"%s: finished sync_queue modification\n", __func__);

	/* roll back DMAGET and start up channel again */
	cdma_op().timeout_teardown_end(cdma, get_restart);

	if (cdma->timeout.ctx)
		cdma->timeout.ctx->has_timedout = true;
}

/**
 * Create a cdma
 */
int nvhost_cdma_init(struct nvhost_cdma *cdma)
{
	int err;
	struct push_buffer *pb = &cdma->push_buffer;
	BUG_ON(!cdma_pb_op().init);
	mutex_init(&cdma->lock);
	sema_init(&cdma->sem, 0);

	INIT_LIST_HEAD(&cdma->sync_queue);

	cdma->event = CDMA_EVENT_NONE;
	cdma->running = false;
	cdma->torndown = false;

	err = cdma_pb_op().init(pb);
	if (err)
		return err;
	return 0;
}

/**
 * Destroy a cdma
 */
void nvhost_cdma_deinit(struct nvhost_cdma *cdma)
{
	struct push_buffer *pb = &cdma->push_buffer;

	BUG_ON(!cdma_pb_op().destroy);
	BUG_ON(cdma->running);
	cdma_pb_op().destroy(pb);
	cdma_op().timeout_destroy(cdma);
}

/**
 * Begin a cdma submit
 */
int nvhost_cdma_begin(struct nvhost_cdma *cdma, struct nvhost_job *job)
{
	mutex_lock(&cdma->lock);

	if (job->timeout) {
		/* init state on first submit with timeout value */
		if (!cdma->timeout.initialized) {
			int err;
			BUG_ON(!cdma_op().timeout_init);
			err = cdma_op().timeout_init(cdma,
				job->syncpt_id);
			if (err) {
				mutex_unlock(&cdma->lock);
				return err;
			}
		}
	}
	if (!cdma->running) {
		BUG_ON(!cdma_op().start);
		cdma_op().start(cdma);
	}
	cdma->slots_free = 0;
	cdma->slots_used = 0;
	cdma->first_get = cdma_pb_op().putptr(&cdma->push_buffer);
	return 0;
}

static void trace_write_gather(struct nvhost_cdma *cdma,
		struct mem_handle *ref,
		u32 offset, u32 words)
{
	void *mem = NULL;

	if (nvhost_debug_trace_cmdbuf) {
		mem = mem_op().mmap(ref);
		if (IS_ERR_OR_NULL(mem))
			mem = NULL;
	};

	if (mem) {
		u32 i;
		/*
		 * Write in batches of 128 as there seems to be a limit
		 * of how much you can output to ftrace at once.
		 */
		for (i = 0; i < words; i += TRACE_MAX_LENGTH) {
			trace_nvhost_cdma_push_gather(
				cdma_to_channel(cdma)->dev->name,
				(u32)ref,
				min(words - i, TRACE_MAX_LENGTH),
				offset + i * sizeof(u32),
				mem);
		}
		mem_op().munmap(ref, mem);
	}
}

/**
 * Push two words into a push buffer slot
 * Blocks as necessary if the push buffer is full.
 */
void nvhost_cdma_push(struct nvhost_cdma *cdma, u32 op1, u32 op2)
{
	if (nvhost_debug_trace_cmdbuf)
		trace_nvhost_cdma_push(cdma_to_channel(cdma)->dev->name,
				op1, op2);

	nvhost_cdma_push_gather(cdma, NULL, NULL, 0, op1, op2);
}

/**
 * Push two words into a push buffer slot
 * Blocks as necessary if the push buffer is full.
 */
void nvhost_cdma_push_gather(struct nvhost_cdma *cdma,
		struct mem_mgr *client, struct mem_handle *handle,
		u32 offset, u32 op1, u32 op2)
{
	u32 slots_free = cdma->slots_free;
	struct push_buffer *pb = &cdma->push_buffer;
	BUG_ON(!cdma_pb_op().push_to);
	BUG_ON(!cdma_op().kick);

	if (handle)
		trace_write_gather(cdma, handle, offset, op1 & 0xffff);

	if (slots_free == 0) {
		cdma_op().kick(cdma);
		slots_free = nvhost_cdma_wait_locked(cdma,
				CDMA_EVENT_PUSH_BUFFER_SPACE);
	}
	cdma->slots_free = slots_free - 1;
	cdma->slots_used++;
	cdma_pb_op().push_to(pb, client, handle, op1, op2);
}

/**
 * End a cdma submit
 * Kick off DMA, add job to the sync queue, and a number of slots to be freed
 * from the pushbuffer. The handles for a submit must all be pinned at the same
 * time, but they can be unpinned in smaller chunks.
 */
void nvhost_cdma_end(struct nvhost_cdma *cdma,
		struct nvhost_job *job)
{
	bool was_idle = list_empty(&cdma->sync_queue);

	BUG_ON(!cdma_op().kick);
	cdma_op().kick(cdma);

	BUG_ON(job->syncpt_id == NVSYNCPT_INVALID);

	add_to_sync_queue(cdma,
			job,
			cdma->slots_used,
			cdma->first_get);

	/* start timer on idle -> active transitions */
	if (job->timeout && was_idle)
		cdma_start_timer_locked(cdma, job);

	trace_nvhost_cdma_end(job->ch->dev->name,
			job->priority,
			job->ch->cdma.high_prio_count,
			job->ch->cdma.med_prio_count,
			job->ch->cdma.low_prio_count);

	mutex_unlock(&cdma->lock);
}

/**
 * Update cdma state according to current sync point values
 */
void nvhost_cdma_update(struct nvhost_cdma *cdma)
{
	mutex_lock(&cdma->lock);
	update_cdma_locked(cdma);
	mutex_unlock(&cdma->lock);
}

/**
 * Wait for push buffer to be empty.
 * @cdma pointer to channel cdma
 * @timeout timeout in ms
 * Returns -ETIME if timeout was reached, zero if push buffer is empty.
 */
int nvhost_cdma_flush(struct nvhost_cdma *cdma, int timeout)
{
	unsigned int space, err = 0;
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);

	trace_nvhost_cdma_flush(cdma_to_channel(cdma)->dev->name, timeout);

	/*
	 * Wait for at most timeout ms. Recalculate timeout at each iteration
	 * to better keep within given timeout.
	 */
	while(!err && time_before(jiffies, end_jiffies)) {
		int timeout_jiffies = end_jiffies - jiffies;

		mutex_lock(&cdma->lock);
		space = cdma_status_locked(cdma,
				CDMA_EVENT_SYNC_QUEUE_EMPTY);
		if (space) {
			mutex_unlock(&cdma->lock);
			return 0;
		}

		/*
		 * Wait for sync queue to become empty. If there is already
		 * an event pending, we need to poll.
		 */
		if (cdma->event != CDMA_EVENT_NONE) {
			mutex_unlock(&cdma->lock);
			schedule();
		} else {
			cdma->event = CDMA_EVENT_SYNC_QUEUE_EMPTY;

			mutex_unlock(&cdma->lock);
			err = down_timeout(&cdma->sem,
					jiffies_to_msecs(timeout_jiffies));
		}
	}
	return err;
}

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

/*
 * KGSL drawobj management
 * A drawobj is a single submission from userland.  The drawobj
 * encapsulates everything about the submission : command buffers, flags and
 * sync points.
 *
 * Sync points are events that need to expire before the
 * drawobj can be queued to the hardware. All synpoints are contained in an
 * array of kgsl_drawobj_sync_event structs in the drawobj. There can be
 * multiple types of events both internal ones (GPU events) and external
 * triggers. As the events expire bits are cleared in a pending bitmap stored
 * in the drawobj. The GPU will submit the command as soon as the bitmap
 * goes to zero indicating no more pending events.
 */

#include <linux/slab.h>
#include <linux/dma-fence-array.h>

#include "adreno_drawctxt.h"
#include "kgsl_compat.h"
#include "kgsl_device.h"
#include "kgsl_drawobj.h"
#include "kgsl_eventlog.h"
#include "kgsl_sync.h"
#include "kgsl_timeline.h"
#include "kgsl_trace.h"

/*
 * Define an kmem cache for the memobj structures since we
 * allocate and free them so frequently
 */
static struct kmem_cache *memobjs_cache;

static void syncobj_destroy_object(struct kgsl_drawobj *drawobj)
{
	struct kgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);
	int i;

	for (i = 0; i < syncobj->numsyncs; i++) {
		struct kgsl_drawobj_sync_event *event = &syncobj->synclist[i];

		if (event->type == KGSL_CMD_SYNCPOINT_TYPE_FENCE) {
			struct event_fence_info *priv = event->priv;

			if (priv) {
				kfree(priv->fences);
				kfree(priv);
			}

			if (event->handle) {
				struct kgsl_sync_fence_cb *kcb = event->handle;

				dma_fence_put(kcb->fence);
				kfree(kcb);
			}

		} else if (event->type == KGSL_CMD_SYNCPOINT_TYPE_TIMELINE) {
			kfree(event->priv);
		}
	}

	kfree(syncobj->synclist);
	kfree(syncobj);
}

static void cmdobj_destroy_object(struct kgsl_drawobj *drawobj)
{
	kfree(CMDOBJ(drawobj));
}

static void bindobj_destroy_object(struct kgsl_drawobj *drawobj)
{
	kfree(BINDOBJ(drawobj));
}

static void timelineobj_destroy_object(struct kgsl_drawobj *drawobj)
{
	kfree(TIMELINEOBJ(drawobj));
}

void kgsl_drawobj_destroy_object(struct kref *kref)
{
	struct kgsl_drawobj *drawobj = container_of(kref,
		struct kgsl_drawobj, refcount);

	kgsl_context_put(drawobj->context);
	drawobj->destroy_object(drawobj);
}

void kgsl_dump_syncpoints(struct kgsl_device *device,
	struct kgsl_drawobj_sync *syncobj)
{
	struct kgsl_drawobj_sync_event *event;
	unsigned int i;

	for (i = 0; i < syncobj->numsyncs; i++) {
		event = &syncobj->synclist[i];

		if (!kgsl_drawobj_event_pending(syncobj, i))
			continue;

		switch (event->type) {
		case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP: {
			unsigned int retired;

			 kgsl_readtimestamp(event->device,
				event->context, KGSL_TIMESTAMP_RETIRED,
				&retired);

			dev_err(device->dev,
				"  [timestamp] context %u timestamp %u (retired %u)\n",
				event->context->id, event->timestamp,
				retired);
			break;
		}
		case KGSL_CMD_SYNCPOINT_TYPE_FENCE: {
			int j;
			struct event_fence_info *info = event->priv;

			for (j = 0; info && j < info->num_fences; j++)
				dev_err(device->dev, "[%d]  fence: %s\n",
					i, info->fences[j].name);
			break;
		}
		case KGSL_CMD_SYNCPOINT_TYPE_TIMELINE: {
			int j;
			struct event_timeline_info *info = event->priv;

			for (j = 0; info && info[j].timeline; j++)
				dev_err(device->dev, "[%d]  timeline: %d seqno %lld\n",
					i, info[j].timeline, info[j].seqno);
			break;
		}
		}
	}
}

static void syncobj_timer(struct timer_list *t)
{
	struct kgsl_device *device;
	struct kgsl_drawobj_sync *syncobj = from_timer(syncobj, t, timer);
	struct kgsl_drawobj *drawobj;
	struct kgsl_drawobj_sync_event *event;
	unsigned int i;

	if (syncobj == NULL)
		return;

	drawobj = DRAWOBJ(syncobj);

	if (!kref_get_unless_zero(&drawobj->refcount))
		return;

	if (drawobj->context == NULL) {
		kgsl_drawobj_put(drawobj);
		return;
	}

	device = drawobj->context->device;

	dev_err(device->dev,
		"kgsl: possible gpu syncpoint deadlock for context %u timestamp %u\n",
		drawobj->context->id, drawobj->timestamp);

	set_bit(ADRENO_CONTEXT_FENCE_LOG, &drawobj->context->priv);
	kgsl_context_dump(drawobj->context);
	clear_bit(ADRENO_CONTEXT_FENCE_LOG, &drawobj->context->priv);

	dev_err(device->dev, "      pending events:\n");

	for (i = 0; i < syncobj->numsyncs; i++) {
		event = &syncobj->synclist[i];

		if (!kgsl_drawobj_event_pending(syncobj, i))
			continue;

		switch (event->type) {
		case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
			dev_err(device->dev, "       [%u] TIMESTAMP %u:%u\n",
				i, event->context->id, event->timestamp);
			break;
		case KGSL_CMD_SYNCPOINT_TYPE_FENCE: {
			int j;
			struct event_fence_info *info = event->priv;

			for (j = 0; info && j < info->num_fences; j++)
				dev_err(device->dev, "       [%u] FENCE %s\n",
					i, info->fences[j].name);
			break;
		}
		case KGSL_CMD_SYNCPOINT_TYPE_TIMELINE: {
			int j;
			struct event_timeline_info *info = event->priv;
			struct dma_fence *fence = event->fence;
			bool retired = false;
			bool signaled = test_bit(DMA_FENCE_FLAG_SIGNALED_BIT,
					&fence->flags);
			const char *str = NULL;

			if (fence->ops->signaled && fence->ops->signaled(fence))
				retired = true;

			if (!retired)
				str = "not retired";
			else if (retired && signaled)
				str = "signaled";
			else if (retired && !signaled)
				str = "retired but not signaled";
			dev_err(device->dev, "       [%u] FENCE %s\n",
				i, str);
			for (j = 0; info && info[j].timeline; j++)
				dev_err(device->dev, "       TIMELINE %d SEQNO %lld\n",
					info[j].timeline, info[j].seqno);
			break;
		}
		}
	}

	kgsl_drawobj_put(drawobj);
	dev_err(device->dev, "--gpu syncpoint deadlock print end--\n");
}

/*
 * a generic function to retire a pending sync event and (possibly) kick the
 * dispatcher.
 * Returns false if the event was already marked for cancellation in another
 * thread. This function should return true if this thread is responsible for
 * freeing up the memory, and the event will not be cancelled.
 */
static bool drawobj_sync_expire(struct kgsl_device *device,
	struct kgsl_drawobj_sync_event *event)
{
	struct kgsl_drawobj_sync *syncobj = event->syncobj;
	/*
	 * Clear the event from the pending mask - if it is already clear, then
	 * leave without doing anything useful
	 */
	if (!test_and_clear_bit(event->id, &syncobj->pending))
		return false;

	/*
	 * If no more pending events, delete the timer and schedule the command
	 * for dispatch
	 */
	if (!kgsl_drawobj_events_pending(event->syncobj)) {
		del_timer(&syncobj->timer);

		if (device->ftbl->drawctxt_sched)
			device->ftbl->drawctxt_sched(device,
				event->syncobj->base.context);
	}
	return true;
}

/*
 * This function is called by the GPU event when the sync event timestamp
 * expires
 */
static void drawobj_sync_func(struct kgsl_device *device,
		struct kgsl_event_group *group, void *priv, int result)
{
	struct kgsl_drawobj_sync_event *event = priv;

	trace_syncpoint_timestamp_expire(event->syncobj,
		event->context, event->timestamp);

	/*
	 * Put down the context ref count only if
	 * this thread successfully clears the pending bit mask.
	 */
	if (drawobj_sync_expire(device, event))
		kgsl_context_put(event->context);

	kgsl_drawobj_put(&event->syncobj->base);
}

static void drawobj_sync_timeline_fence_work(struct work_struct *work)
{
	struct kgsl_drawobj_sync_event *event = container_of(work,
		struct kgsl_drawobj_sync_event, work);

	dma_fence_put(event->fence);
	kgsl_drawobj_put(&event->syncobj->base);
}

static void trace_syncpoint_timeline_fence(struct kgsl_drawobj_sync *syncobj,
	struct dma_fence *f, bool expire)
{
	struct dma_fence_array *array = to_dma_fence_array(f);
	struct dma_fence **fences = &f;
	u32 num_fences = 1;
	int i;

	if (array) {
		num_fences = array->num_fences;
		fences = array->fences;
	}

	for (i = 0; i < num_fences; i++) {
		char fence_name[KGSL_FENCE_NAME_LEN];

		snprintf(fence_name, sizeof(fence_name), "%s:%llu",
			fences[i]->ops->get_timeline_name(fences[i]),
			fences[i]->seqno);
		if (expire) {
			trace_syncpoint_fence_expire(syncobj, fence_name);
			log_kgsl_syncpoint_fence_expire_event(
			syncobj->base.context->id, fence_name);
		} else {
			trace_syncpoint_fence(syncobj, fence_name);
			log_kgsl_syncpoint_fence_event(
			syncobj->base.context->id, fence_name);
		}
	}
}

static void drawobj_sync_timeline_fence_callback(struct dma_fence *f,
		struct dma_fence_cb *cb)
{
	struct kgsl_drawobj_sync_event *event = container_of(cb,
		struct kgsl_drawobj_sync_event, cb);

	trace_syncpoint_timeline_fence(event->syncobj, f, true);

	/*
	 * Mark the event as synced and then fire off a worker to handle
	 * removing the fence
	 */
	if (drawobj_sync_expire(event->device, event))
		queue_work(kgsl_driver.lockless_workqueue, &event->work);
}

static void syncobj_destroy(struct kgsl_drawobj *drawobj)
{
	struct kgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);
	unsigned int i;

	/* Zap the canary timer */
	del_timer_sync(&syncobj->timer);

	/*
	 * Clear all pending events - this will render any subsequent async
	 * callbacks harmless
	 */
	for (i = 0; i < syncobj->numsyncs; i++) {
		struct kgsl_drawobj_sync_event *event = &syncobj->synclist[i];

		/*
		 * Don't do anything if the event has already expired.
		 * If this thread clears the pending bit mask then it is
		 * responsible for doing context put.
		 */
		if (!test_and_clear_bit(i, &syncobj->pending))
			continue;

		switch (event->type) {
		case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
			kgsl_cancel_event(drawobj->device,
				&event->context->events, event->timestamp,
				drawobj_sync_func, event);
			/*
			 * Do context put here to make sure the context is alive
			 * till this thread cancels kgsl event.
			 */
			kgsl_context_put(event->context);
			break;
		case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
			kgsl_sync_fence_async_cancel(event->handle);
			kgsl_drawobj_put(drawobj);
			break;
		case KGSL_CMD_SYNCPOINT_TYPE_TIMELINE:
			dma_fence_remove_callback(event->fence, &event->cb);
			dma_fence_put(event->fence);
			kgsl_drawobj_put(drawobj);
			break;
		}
	}

	/*
	 * If we cancelled an event, there's a good chance that the context is
	 * on a dispatcher queue, so schedule to get it removed.
	 */
	if (!bitmap_empty(&syncobj->pending, KGSL_MAX_SYNCPOINTS) &&
		drawobj->device->ftbl->drawctxt_sched)
		drawobj->device->ftbl->drawctxt_sched(drawobj->device,
							drawobj->context);

}

static void timelineobj_destroy(struct kgsl_drawobj *drawobj)
{
	struct kgsl_drawobj_timeline *timelineobj = TIMELINEOBJ(drawobj);
	int i;

	for (i = 0; i < timelineobj->count; i++)
		kgsl_timeline_put(timelineobj->timelines[i].timeline);

	kvfree(timelineobj->timelines);
	timelineobj->timelines = NULL;
	timelineobj->count = 0;
}

static void bindobj_destroy(struct kgsl_drawobj *drawobj)
{
	struct kgsl_drawobj_bind *bindobj = BINDOBJ(drawobj);

	kgsl_sharedmem_put_bind_op(bindobj->bind);
}

static void cmdobj_destroy(struct kgsl_drawobj *drawobj)
{
	struct kgsl_drawobj_cmd *cmdobj = CMDOBJ(drawobj);
	struct kgsl_memobj_node *mem, *tmpmem;

	/*
	 * Release the refcount on the mem entry associated with the
	 * ib profiling buffer
	 */
	if (cmdobj->base.flags & KGSL_DRAWOBJ_PROFILING)
		kgsl_mem_entry_put(cmdobj->profiling_buf_entry);

	/* Destroy the command list */
	list_for_each_entry_safe(mem, tmpmem, &cmdobj->cmdlist, node) {
		list_del_init(&mem->node);
		kmem_cache_free(memobjs_cache, mem);
	}

	/* Destroy the memory list */
	list_for_each_entry_safe(mem, tmpmem, &cmdobj->memlist, node) {
		list_del_init(&mem->node);
		kmem_cache_free(memobjs_cache, mem);
	}

	if (drawobj->type & CMDOBJ_TYPE) {
		atomic_dec(&drawobj->context->proc_priv->cmd_count);
		atomic_dec(&drawobj->context->proc_priv->period->active_cmds);
	}
}

/**
 * kgsl_drawobj_destroy() - Destroy a kgsl object structure
 * @obj: Pointer to the kgsl object to destroy
 *
 * Start the process of destroying a command batch.  Cancel any pending events
 * and decrement the refcount.  Asynchronous events can still signal after
 * kgsl_drawobj_destroy has returned.
 */
void kgsl_drawobj_destroy(struct kgsl_drawobj *drawobj)
{
	if (IS_ERR_OR_NULL(drawobj))
		return;

	drawobj->destroy(drawobj);

	kgsl_drawobj_put(drawobj);
}

static bool drawobj_sync_fence_func(void *priv)
{
	struct kgsl_drawobj_sync_event *event = priv;
	struct event_fence_info *info = event->priv;
	int i;

	for (i = 0; info && i < info->num_fences; i++) {
		trace_syncpoint_fence_expire(event->syncobj,
			info->fences[i].name);
		log_kgsl_syncpoint_fence_expire_event(
		event->syncobj->base.context->id, info->fences[i].name);
	}

	/*
	 * Only call kgsl_drawobj_put() if it's not marked for cancellation
	 * in another thread.
	 */
	if (drawobj_sync_expire(event->device, event)) {
		kgsl_drawobj_put(&event->syncobj->base);
		return true;
	}
	return false;
}

static struct event_timeline_info *
drawobj_get_sync_timeline_priv(void __user *uptr, u64 usize, u32 count)
{
	int i;
	struct event_timeline_info *priv;

	/* Make sure we don't accidently overflow count */
	if (count == UINT_MAX)
		return NULL;

	priv = kcalloc(count + 1, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return NULL;

	for (i = 0; i < count; i++, uptr += usize) {
		struct kgsl_timeline_val val;

		if (copy_struct_from_user(&val, sizeof(val), uptr, usize))
			continue;

		priv[i].timeline = val.timeline;
		priv[i].seqno = val.seqno;
	}

	priv[i].timeline = 0;
	return priv;
}

static int drawobj_add_sync_timeline(struct kgsl_device *device,

		struct kgsl_drawobj_sync *syncobj, void __user *uptr,
		u64 usize)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(syncobj);
	struct kgsl_cmd_syncpoint_timeline sync;
	struct kgsl_drawobj_sync_event *event;
	struct dma_fence *fence;
	unsigned int id;
	int ret;

	if (copy_struct_from_user(&sync, sizeof(sync), uptr, usize))
		return -EFAULT;

	fence = kgsl_timelines_to_fence_array(device, sync.timelines,
		sync.count, sync.timelines_size, false);
	if (IS_ERR(fence))
		return PTR_ERR(fence);

	kref_get(&drawobj->refcount);

	id = syncobj->numsyncs++;

	event = &syncobj->synclist[id];

	event->id = id;
	event->type = KGSL_CMD_SYNCPOINT_TYPE_TIMELINE;
	event->syncobj = syncobj;
	event->device = device;
	event->context = NULL;
	event->fence = fence;
	INIT_WORK(&event->work, drawobj_sync_timeline_fence_work);

	INIT_LIST_HEAD(&event->cb.node);

	event->priv =
		drawobj_get_sync_timeline_priv(u64_to_user_ptr(sync.timelines),
			sync.timelines_size, sync.count);

	/* Set pending flag before adding callback to avoid race */
	set_bit(event->id, &syncobj->pending);

	ret = dma_fence_add_callback(event->fence,
		&event->cb, drawobj_sync_timeline_fence_callback);

	if (ret) {
		clear_bit(event->id, &syncobj->pending);

		if (dma_fence_is_signaled(event->fence)) {
			trace_syncpoint_fence_expire(syncobj, "signaled");
			log_kgsl_syncpoint_fence_expire_event(
			syncobj->base.context->id, "signaled");
			dma_fence_put(event->fence);
			ret = 0;
		}

		kgsl_drawobj_put(drawobj);
		return ret;
	}

	trace_syncpoint_timeline_fence(event->syncobj, event->fence, false);
	return 0;
}

static int drawobj_add_sync_fence(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj, void __user *data,
		u64 datasize)
{
	struct kgsl_cmd_syncpoint_fence sync;
	struct kgsl_drawobj *drawobj = DRAWOBJ(syncobj);
	struct kgsl_drawobj_sync_event *event;
	struct event_fence_info *priv;
	unsigned int id, i;

	if (copy_struct_from_user(&sync, sizeof(sync), data, datasize))
		return -EFAULT;

	kref_get(&drawobj->refcount);

	id = syncobj->numsyncs++;

	event = &syncobj->synclist[id];

	event->id = id;
	event->type = KGSL_CMD_SYNCPOINT_TYPE_FENCE;
	event->syncobj = syncobj;
	event->device = device;
	event->context = NULL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);

	set_bit(event->id, &syncobj->pending);

	event->handle = kgsl_sync_fence_async_wait(sync.fd,
				drawobj_sync_fence_func, event, priv);

	event->priv = priv;

	if (IS_ERR_OR_NULL(event->handle)) {
		int ret = PTR_ERR(event->handle);

		clear_bit(event->id, &syncobj->pending);
		event->handle = NULL;

		kgsl_drawobj_put(drawobj);

		/*
		 * If ret == 0 the fence was already signaled - print a trace
		 * message so we can track that
		 */
		if (ret == 0) {
			trace_syncpoint_fence_expire(syncobj, "signaled");
			log_kgsl_syncpoint_fence_expire_event(
				syncobj->base.context->id, "signaled");
		}

		return ret;
	}

	for (i = 0; priv && i < priv->num_fences; i++) {
		trace_syncpoint_fence(syncobj, priv->fences[i].name);
		log_kgsl_syncpoint_fence_event(syncobj->base.context->id,
			priv->fences[i].name);
	}

	return 0;
}

/* drawobj_add_sync_timestamp() - Add a new sync point for a sync obj
 * @device: KGSL device
 * @syncobj: KGSL sync obj to add the sync point to
 * @priv: Private structure passed by the user
 *
 * Add a new sync point timestamp event to the sync obj.
 */
static int drawobj_add_sync_timestamp(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj,
		struct kgsl_cmd_syncpoint_timestamp *timestamp)

{
	struct kgsl_drawobj *drawobj = DRAWOBJ(syncobj);
	struct kgsl_context *context = kgsl_context_get(device,
		timestamp->context_id);
	struct kgsl_drawobj_sync_event *event;
	int ret = -EINVAL;
	unsigned int id;

	if (context == NULL)
		return -EINVAL;

	/*
	 * We allow somebody to create a sync point on their own context.
	 * This has the effect of delaying a command from submitting until the
	 * dependent command has cleared.  That said we obviously can't let them
	 * create a sync point on a future timestamp.
	 */

	if (context == drawobj->context) {
		unsigned int queued;

		kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_QUEUED,
			&queued);

		if (timestamp_cmp(timestamp->timestamp, queued) > 0) {
			dev_err(device->dev,
				     "Cannot create syncpoint for future timestamp %d (current %d)\n",
				     timestamp->timestamp, queued);
			goto done;
		}
	}

	kref_get(&drawobj->refcount);

	id = syncobj->numsyncs++;

	event = &syncobj->synclist[id];
	event->id = id;

	event->type = KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP;
	event->syncobj = syncobj;
	event->context = context;
	event->timestamp = timestamp->timestamp;
	event->device = device;

	set_bit(event->id, &syncobj->pending);

	ret = kgsl_add_event(device, &context->events, timestamp->timestamp,
		drawobj_sync_func, event);

	if (ret) {
		clear_bit(event->id, &syncobj->pending);
		kgsl_drawobj_put(drawobj);
	} else {
		trace_syncpoint_timestamp(syncobj, context,
			timestamp->timestamp);
	}

done:
	if (ret)
		kgsl_context_put(context);

	return ret;
}

static int drawobj_add_sync_timestamp_from_user(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj, void __user *data,
		u64 datasize)
{
	struct kgsl_cmd_syncpoint_timestamp timestamp;

	if (copy_struct_from_user(&timestamp, sizeof(timestamp),
			data, datasize))
		return -EFAULT;

	return drawobj_add_sync_timestamp(device, syncobj, &timestamp);
}

/**
 * kgsl_drawobj_sync_add_sync() - Add a sync point to a command
 * batch
 * @device: Pointer to the KGSL device struct for the GPU
 * @syncobj: Pointer to the sync obj
 * @sync: Pointer to the user-specified struct defining the syncpoint
 *
 * Create a new sync point in the sync obj based on the
 * user specified parameters
 */
int kgsl_drawobj_sync_add_sync(struct kgsl_device *device,
	struct kgsl_drawobj_sync *syncobj,
	struct kgsl_cmd_syncpoint *sync)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(syncobj);

	if (sync->type != KGSL_CMD_SYNCPOINT_TYPE_FENCE)
		syncobj->flags |= KGSL_SYNCOBJ_SW;

	if (sync->type == KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP)
		return drawobj_add_sync_timestamp_from_user(device,
			syncobj, sync->priv, sync->size);
	else if (sync->type == KGSL_CMD_SYNCPOINT_TYPE_FENCE)
		return drawobj_add_sync_fence(device,
			syncobj, sync->priv, sync->size);
	else if (sync->type == KGSL_CMD_SYNCPOINT_TYPE_TIMELINE)
		return drawobj_add_sync_timeline(device,
			syncobj, sync->priv, sync->size);

	dev_err(device->dev, "bad syncpoint type %d for ctxt %u\n",
		sync->type, drawobj->context->id);

	return -EINVAL;
}

static void add_profiling_buffer(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj,
		uint64_t gpuaddr, uint64_t size,
		unsigned int id, uint64_t offset)
{
	struct kgsl_mem_entry *entry;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	u64 start;

	if (!(drawobj->flags & KGSL_DRAWOBJ_PROFILING))
		return;

	/* Only the first buffer entry counts - ignore the rest */
	if (cmdobj->profiling_buf_entry != NULL)
		return;

	if (id != 0)
		entry = kgsl_sharedmem_find_id(drawobj->context->proc_priv,
				id);
	else
		entry = kgsl_sharedmem_find(drawobj->context->proc_priv,
			gpuaddr);

	if (entry != NULL) {
		start = id ? (entry->memdesc.gpuaddr + offset) : gpuaddr;
		/*
		 * Make sure there is enough room in the object to store the
		 * entire profiling buffer object
		 */
		if (!kgsl_gpuaddr_in_memdesc(&entry->memdesc, gpuaddr, size) ||
			!kgsl_gpuaddr_in_memdesc(&entry->memdesc, start,
				sizeof(struct kgsl_drawobj_profiling_buffer))) {
			kgsl_mem_entry_put(entry);
			entry = NULL;
		}
	}

	if (entry == NULL) {
		dev_err(device->dev,
			"ignore bad profile buffer ctxt %u id %d offset %lld gpuaddr %llx size %lld\n",
			drawobj->context->id, id, offset, gpuaddr, size);
		return;
	}

	cmdobj->profiling_buffer_gpuaddr = start;
	cmdobj->profiling_buf_entry = entry;
}

/**
 * kgsl_drawobj_cmd_add_ibdesc() - Add a legacy ibdesc to a command
 * batch
 * @cmdobj: Pointer to the ib
 * @ibdesc: Pointer to the user-specified struct defining the memory or IB
 *
 * Create a new memory entry in the ib based on the
 * user specified parameters
 */
int kgsl_drawobj_cmd_add_ibdesc(struct kgsl_device *device,
	struct kgsl_drawobj_cmd *cmdobj, struct kgsl_ibdesc *ibdesc)
{
	uint64_t gpuaddr = (uint64_t) ibdesc->gpuaddr;
	uint64_t size = (uint64_t) ibdesc->sizedwords << 2;
	struct kgsl_memobj_node *mem;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

	/* sanitize the ibdesc ctrl flags */
	ibdesc->ctrl &= KGSL_IBDESC_MEMLIST | KGSL_IBDESC_PROFILING_BUFFER;

	if (drawobj->flags & KGSL_DRAWOBJ_MEMLIST &&
			ibdesc->ctrl & KGSL_IBDESC_MEMLIST) {
		if (ibdesc->ctrl & KGSL_IBDESC_PROFILING_BUFFER) {
			add_profiling_buffer(device, cmdobj,
					gpuaddr, size, 0, 0);
			return 0;
		}
	}

	/* Ignore if SYNC or MARKER is specified */
	if (drawobj->type & (SYNCOBJ_TYPE | MARKEROBJ_TYPE))
		return 0;

	mem = kmem_cache_alloc(memobjs_cache, GFP_KERNEL);
	if (mem == NULL)
		return -ENOMEM;

	mem->gpuaddr = gpuaddr;
	mem->size = size;
	mem->priv = 0;
	mem->id = 0;
	mem->offset = 0;
	mem->flags = 0;

	if (drawobj->flags & KGSL_DRAWOBJ_MEMLIST &&
			ibdesc->ctrl & KGSL_IBDESC_MEMLIST)
		/* add to the memlist */
		list_add_tail(&mem->node, &cmdobj->memlist);
	else {
		/* set the preamble flag if directed to */
		if (drawobj->context->flags & KGSL_CONTEXT_PREAMBLE &&
				list_empty(&cmdobj->cmdlist))
			mem->flags = KGSL_CMDLIST_CTXTSWITCH_PREAMBLE;

		/* add to the cmd list */
		list_add_tail(&mem->node, &cmdobj->cmdlist);
	}

	return 0;
}

static int drawobj_init(struct kgsl_device *device,
	struct kgsl_context *context, struct kgsl_drawobj *drawobj,
	int type)
{
	/*
	 * Increase the reference count on the context so it doesn't disappear
	 * during the lifetime of this object
	 */
	if (!_kgsl_context_get(context))
		return -ENOENT;

	kref_init(&drawobj->refcount);

	drawobj->device = device;
	drawobj->context = context;
	drawobj->type = type;

	return 0;
}

static int get_aux_command(void __user *ptr, u64 generic_size,
		int type, void *auxcmd, size_t auxcmd_size)
{
	struct kgsl_gpu_aux_command_generic generic;
	u64 size;

	if (copy_struct_from_user(&generic, sizeof(generic), ptr, generic_size))
		return -EFAULT;

	if (generic.type != type)
		return -EINVAL;

	size = min_t(u64, auxcmd_size, generic.size);
	if (copy_from_user(auxcmd, u64_to_user_ptr(generic.priv), size))
		return -EFAULT;

	return 0;
}

struct kgsl_drawobj_timeline *
kgsl_drawobj_timeline_create(struct kgsl_device *device,
		struct kgsl_context *context)
{
	int ret;
	struct kgsl_drawobj_timeline *timelineobj =
		kzalloc(sizeof(*timelineobj), GFP_KERNEL);

	if (!timelineobj)
		return ERR_PTR(-ENOMEM);

	ret = drawobj_init(device, context, &timelineobj->base,
		TIMELINEOBJ_TYPE);
	if (ret) {
		kfree(timelineobj);
		return ERR_PTR(ret);
	}

	timelineobj->base.destroy = timelineobj_destroy;
	timelineobj->base.destroy_object = timelineobj_destroy_object;

	return timelineobj;
}

int kgsl_drawobj_add_timeline(struct kgsl_device_private *dev_priv,
		struct kgsl_drawobj_timeline *timelineobj,
		void __user *src, u64 cmdsize)
{
	struct kgsl_gpu_aux_command_timeline cmd;
	int i, ret;

	memset(&cmd, 0, sizeof(cmd));

	ret = get_aux_command(src, cmdsize,
		KGSL_GPU_AUX_COMMAND_TIMELINE, &cmd, sizeof(cmd));
	if (ret)
		return ret;

	if (!cmd.count)
		return -EINVAL;

	timelineobj->timelines = kvcalloc(cmd.count,
		sizeof(*timelineobj->timelines),
		GFP_KERNEL | __GFP_NORETRY | __GFP_NOWARN);
	if (!timelineobj->timelines)
		return -ENOMEM;

	src = u64_to_user_ptr(cmd.timelines);

	for (i = 0; i < cmd.count; i++) {
		struct kgsl_timeline_val val;

		if (copy_struct_from_user(&val, sizeof(val), src,
			cmd.timelines_size)) {
			ret = -EFAULT;
			goto err;
		}

		if (val.padding) {
			ret = -EINVAL;
			goto err;
		}

		timelineobj->timelines[i].timeline =
			kgsl_timeline_by_id(dev_priv->device,
				val.timeline);

		if (!timelineobj->timelines[i].timeline) {
			ret = -ENODEV;
			goto err;
		}

		trace_kgsl_drawobj_timeline(val.timeline, val.seqno);
		timelineobj->timelines[i].seqno = val.seqno;

		src += cmd.timelines_size;
	}

	timelineobj->count = cmd.count;
	return 0;
err:
	for (i = 0; i < cmd.count; i++)
		kgsl_timeline_put(timelineobj->timelines[i].timeline);

	kvfree(timelineobj->timelines);
	timelineobj->timelines = NULL;
	return ret;
}

static void kgsl_drawobj_bind_callback(struct kgsl_sharedmem_bind_op *op)
{
	struct kgsl_drawobj_bind *bindobj = op->data;
	struct kgsl_drawobj *drawobj = DRAWOBJ(bindobj);
	struct kgsl_device *device = drawobj->device;

	set_bit(KGSL_BINDOBJ_STATE_DONE, &bindobj->state);

	/* Re-schedule the context */
	if (device->ftbl->drawctxt_sched)
		device->ftbl->drawctxt_sched(device,
			drawobj->context);

	/* Put back the reference we took when we started the operation */
	kgsl_context_put(drawobj->context);
	kgsl_drawobj_put(drawobj);
}

int kgsl_drawobj_add_bind(struct kgsl_device_private *dev_priv,
		struct kgsl_drawobj_bind *bindobj,
		void __user *src, u64 cmdsize)
{
	struct kgsl_gpu_aux_command_bind cmd;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_sharedmem_bind_op *op;
	int ret;

	ret = get_aux_command(src, cmdsize,
		KGSL_GPU_AUX_COMMAND_BIND, &cmd, sizeof(cmd));
	if (ret)
		return ret;

	op = kgsl_sharedmem_create_bind_op(private, cmd.target,
		u64_to_user_ptr(cmd.rangeslist), cmd.numranges,
		cmd.rangesize);

	if (IS_ERR(op))
		return PTR_ERR(op);

	op->callback = kgsl_drawobj_bind_callback;
	op->data = bindobj;

	bindobj->bind = op;
	return 0;
}

struct kgsl_drawobj_bind *kgsl_drawobj_bind_create(struct kgsl_device *device,
		struct kgsl_context *context)
{
	int ret;
	struct kgsl_drawobj_bind *bindobj =
		kzalloc(sizeof(*bindobj), GFP_KERNEL);

	if (!bindobj)
		return ERR_PTR(-ENOMEM);

	ret = drawobj_init(device, context, &bindobj->base, BINDOBJ_TYPE);
	if (ret) {
		kfree(bindobj);
		return ERR_PTR(ret);
	}

	bindobj->base.destroy = bindobj_destroy;
	bindobj->base.destroy_object = bindobj_destroy_object;

	return bindobj;
}

/**
 * kgsl_drawobj_sync_create() - Create a new sync obj
 * structure
 * @device: Pointer to a KGSL device struct
 * @context: Pointer to a KGSL context struct
 *
 * Allocate an new kgsl_drawobj_sync structure
 */
struct kgsl_drawobj_sync *kgsl_drawobj_sync_create(struct kgsl_device *device,
		struct kgsl_context *context)
{
	struct kgsl_drawobj_sync *syncobj =
		kzalloc(sizeof(*syncobj), GFP_KERNEL);
	int ret;

	if (!syncobj)
		return ERR_PTR(-ENOMEM);

	ret = drawobj_init(device, context, &syncobj->base, SYNCOBJ_TYPE);
	if (ret) {
		kfree(syncobj);
		return ERR_PTR(ret);
	}

	syncobj->base.destroy = syncobj_destroy;
	syncobj->base.destroy_object = syncobj_destroy_object;

	timer_setup(&syncobj->timer, syncobj_timer, 0);

	return syncobj;
}

/**
 * kgsl_drawobj_cmd_create() - Create a new command obj
 * structure
 * @device: Pointer to a KGSL device struct
 * @context: Pointer to a KGSL context struct
 * @flags: Flags for the command obj
 * @type: type of cmdobj MARKER/CMD
 *
 * Allocate a new kgsl_drawobj_cmd structure
 */
struct kgsl_drawobj_cmd *kgsl_drawobj_cmd_create(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int flags,
		unsigned int type)
{
	struct kgsl_drawobj_cmd *cmdobj = kzalloc(sizeof(*cmdobj), GFP_KERNEL);
	int ret;

	if (!cmdobj)
		return ERR_PTR(-ENOMEM);

	ret = drawobj_init(device, context, &cmdobj->base,
		(type & (CMDOBJ_TYPE | MARKEROBJ_TYPE)));
	if (ret) {
		kfree(cmdobj);
		return ERR_PTR(ret);
	}

	cmdobj->base.destroy = cmdobj_destroy;
	cmdobj->base.destroy_object = cmdobj_destroy_object;

	/* sanitize our flags for drawobjs */
	cmdobj->base.flags = flags & (KGSL_DRAWOBJ_CTX_SWITCH
		| KGSL_DRAWOBJ_MARKER
		| KGSL_DRAWOBJ_END_OF_FRAME
		| KGSL_DRAWOBJ_PWR_CONSTRAINT
		| KGSL_DRAWOBJ_MEMLIST
		| KGSL_DRAWOBJ_PROFILING
		| KGSL_DRAWOBJ_PROFILING_KTIME
		| KGSL_DRAWOBJ_START_RECURRING
		| KGSL_DRAWOBJ_STOP_RECURRING);

	INIT_LIST_HEAD(&cmdobj->cmdlist);
	INIT_LIST_HEAD(&cmdobj->memlist);
	cmdobj->requeue_cnt = 0;

	if (!(type & CMDOBJ_TYPE))
		return cmdobj;

	atomic_inc(&context->proc_priv->cmd_count);
	atomic_inc(&context->proc_priv->period->active_cmds);
	spin_lock(&device->work_period_lock);
	if (!__test_and_set_bit(KGSL_WORK_PERIOD, &device->flags)) {
		mod_timer(&device->work_period_timer,
			  jiffies + msecs_to_jiffies(KGSL_WORK_PERIOD_MS));
		device->gpu_period.begin = ktime_get_ns();
	}

	/* Take a refcount here and put it back in kgsl_work_period_timer() */
	if (!__test_and_set_bit(KGSL_WORK_PERIOD, &context->proc_priv->period->flags))
		kref_get(&context->proc_priv->period->refcount);

	spin_unlock(&device->work_period_lock);

	return cmdobj;
}

#ifdef CONFIG_COMPAT
static int add_ibdesc_list_compat(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj, void __user *ptr, int count)
{
	int i, ret = 0;
	struct kgsl_ibdesc_compat ibdesc32;
	struct kgsl_ibdesc ibdesc;

	for (i = 0; i < count; i++) {
		memset(&ibdesc32, 0, sizeof(ibdesc32));

		if (copy_from_user(&ibdesc32, ptr, sizeof(ibdesc32))) {
			ret = -EFAULT;
			break;
		}

		ibdesc.gpuaddr = (unsigned long) ibdesc32.gpuaddr;
		ibdesc.sizedwords = (size_t) ibdesc32.sizedwords;
		ibdesc.ctrl = (unsigned int) ibdesc32.ctrl;

		ret = kgsl_drawobj_cmd_add_ibdesc(device, cmdobj, &ibdesc);
		if (ret)
			break;

		ptr += sizeof(ibdesc32);
	}

	return ret;
}

static int add_syncpoints_compat(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj, void __user *ptr, int count)
{
	struct kgsl_cmd_syncpoint_compat sync32;
	struct kgsl_cmd_syncpoint sync;
	int i, ret = 0;

	for (i = 0; i < count; i++) {
		memset(&sync32, 0, sizeof(sync32));

		if (copy_from_user(&sync32, ptr, sizeof(sync32))) {
			ret = -EFAULT;
			break;
		}

		sync.type = sync32.type;
		sync.priv = compat_ptr(sync32.priv);
		sync.size = (size_t) sync32.size;

		ret = kgsl_drawobj_sync_add_sync(device, syncobj, &sync);
		if (ret)
			break;

		ptr += sizeof(sync32);
	}

	return ret;
}
#else
static int add_ibdesc_list_compat(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj, void __user *ptr, int count)
{
	return -EINVAL;
}

static int add_syncpoints_compat(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj, void __user *ptr, int count)
{
	return -EINVAL;
}
#endif

/* Returns:
 *   -EINVAL: Bad data
 *   0: All data fields are empty (nothing to do)
 *   1: All list information is valid
 */
static int _verify_input_list(unsigned int count, void __user *ptr,
		unsigned int size)
{
	/* Return early if nothing going on */
	if (count == 0 && ptr == NULL && size == 0)
		return 0;

	/* Sanity check inputs */
	if (count == 0 || ptr == NULL || size == 0)
		return -EINVAL;

	return 1;
}

int kgsl_drawobj_cmd_add_ibdesc_list(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj, void __user *ptr, int count)
{
	struct kgsl_ibdesc ibdesc;
	struct kgsl_drawobj *baseobj = DRAWOBJ(cmdobj);
	int i, ret;

	/* Ignore everything if this is a MARKER */
	if (baseobj->type & MARKEROBJ_TYPE)
		return 0;

	ret = _verify_input_list(count, ptr, sizeof(ibdesc));
	if (ret <= 0)
		return -EINVAL;

	if (is_compat_task())
		return add_ibdesc_list_compat(device, cmdobj, ptr, count);

	for (i = 0; i < count; i++) {
		memset(&ibdesc, 0, sizeof(ibdesc));

		if (copy_from_user(&ibdesc, ptr, sizeof(ibdesc)))
			return -EFAULT;

		ret = kgsl_drawobj_cmd_add_ibdesc(device, cmdobj, &ibdesc);
		if (ret)
			return ret;

		ptr += sizeof(ibdesc);
	}

	return 0;
}

int kgsl_drawobj_sync_add_syncpoints(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj, void __user *ptr, int count)
{
	struct kgsl_cmd_syncpoint sync;
	int i, ret;

	if (count == 0)
		return 0;

	syncobj->synclist = kcalloc(count,
		sizeof(struct kgsl_drawobj_sync_event), GFP_KERNEL);

	if (syncobj->synclist == NULL)
		return -ENOMEM;

	if (is_compat_task())
		return add_syncpoints_compat(device, syncobj, ptr, count);

	for (i = 0; i < count; i++) {
		memset(&sync, 0, sizeof(sync));

		if (copy_from_user(&sync, ptr, sizeof(sync)))
			return -EFAULT;

		ret = kgsl_drawobj_sync_add_sync(device, syncobj, &sync);
		if (ret)
			return ret;

		ptr += sizeof(sync);
	}

	return 0;
}

static int kgsl_drawobj_add_memobject(struct list_head *head,
		struct kgsl_command_object *obj)
{
	struct kgsl_memobj_node *mem;

	mem = kmem_cache_alloc(memobjs_cache, GFP_KERNEL);
	if (mem == NULL)
		return -ENOMEM;

	mem->gpuaddr = obj->gpuaddr;
	mem->size = obj->size;
	mem->id = obj->id;
	mem->offset = obj->offset;
	mem->flags = obj->flags;
	mem->priv = 0;

	list_add_tail(&mem->node, head);
	return 0;
}

#define CMDLIST_FLAGS \
	(KGSL_CMDLIST_IB | \
	 KGSL_CMDLIST_CTXTSWITCH_PREAMBLE | \
	 KGSL_CMDLIST_IB_PREAMBLE)

/* This can only accept MARKEROBJ_TYPE and CMDOBJ_TYPE */
int kgsl_drawobj_cmd_add_cmdlist(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj, void __user *ptr,
		unsigned int size, unsigned int count)
{
	struct kgsl_command_object obj;
	struct kgsl_drawobj *baseobj = DRAWOBJ(cmdobj);
	int i, ret;

	/* Ignore everything if this is a MARKER */
	if (baseobj->type & MARKEROBJ_TYPE)
		return 0;

	ret = _verify_input_list(count, ptr, size);
	if (ret <= 0)
		return ret;

	for (i = 0; i < count; i++) {
		if (copy_struct_from_user(&obj, sizeof(obj), ptr, size))
			return -EFAULT;

		/* Sanity check the flags */
		if (!(obj.flags & CMDLIST_FLAGS)) {
			dev_err(device->dev,
				     "invalid cmdobj ctxt %u flags %d id %d offset %llu addr %llx size %llu\n",
				     baseobj->context->id, obj.flags, obj.id,
				     obj.offset, obj.gpuaddr, obj.size);
			return -EINVAL;
		}

		ret = kgsl_drawobj_add_memobject(&cmdobj->cmdlist, &obj);
		if (ret)
			return ret;

		ptr += sizeof(obj);
	}

	return 0;
}

int kgsl_drawobj_cmd_add_memlist(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj, void __user *ptr,
		unsigned int size, unsigned int count)
{
	struct kgsl_command_object obj;
	struct kgsl_drawobj *baseobj = DRAWOBJ(cmdobj);
	int i, ret;

	/* Ignore everything if this is a MARKER */
	if (baseobj->type & MARKEROBJ_TYPE)
		return 0;

	ret = _verify_input_list(count, ptr, size);
	if (ret <= 0)
		return ret;

	for (i = 0; i < count; i++) {
		if (copy_struct_from_user(&obj, sizeof(obj), ptr, size))
			return -EFAULT;

		if (!(obj.flags & KGSL_OBJLIST_MEMOBJ)) {
			dev_err(device->dev,
				     "invalid memobj ctxt %u flags %d id %d offset %lld addr %lld size %lld\n",
				     DRAWOBJ(cmdobj)->context->id, obj.flags,
				     obj.id, obj.offset, obj.gpuaddr,
				     obj.size);
			return -EINVAL;
		}

		if (obj.flags & KGSL_OBJLIST_PROFILE)
			add_profiling_buffer(device, cmdobj, obj.gpuaddr,
				obj.size, obj.id, obj.offset);
		else {
			ret = kgsl_drawobj_add_memobject(&cmdobj->memlist,
				&obj);
			if (ret)
				return ret;
		}

		ptr += sizeof(obj);
	}

	return 0;
}

struct kgsl_drawobj_sync *
kgsl_drawobj_create_timestamp_syncobj(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp)
{
	struct kgsl_drawobj_sync *syncobj;
	struct kgsl_cmd_syncpoint_timestamp priv;
	int ret;

	syncobj = kgsl_drawobj_sync_create(device, context);
	if (IS_ERR(syncobj))
		return syncobj;

	syncobj->synclist = kzalloc(sizeof(*syncobj->synclist), GFP_KERNEL);
	if (!syncobj->synclist) {
		kgsl_drawobj_destroy(DRAWOBJ(syncobj));
		return ERR_PTR(-ENOMEM);
	}

	priv.timestamp = timestamp;
	priv.context_id = context->id;

	ret = drawobj_add_sync_timestamp(device, syncobj, &priv);
	if (ret) {
		kgsl_drawobj_destroy(DRAWOBJ(syncobj));
		return ERR_PTR(ret);
	}

	return syncobj;
}

int kgsl_drawobj_sync_add_synclist(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj, void __user *ptr,
		unsigned int size, unsigned int count)
{
	struct kgsl_command_syncpoint syncpoint;
	struct kgsl_cmd_syncpoint sync;
	int i, ret;

	/* If creating a sync and the data is not there or wrong then error */
	ret = _verify_input_list(count, ptr, size);
	if (ret <= 0)
		return -EINVAL;

	syncobj->synclist = kcalloc(count,
		sizeof(struct kgsl_drawobj_sync_event), GFP_KERNEL);

	if (syncobj->synclist == NULL)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		if (copy_struct_from_user(&syncpoint, sizeof(syncpoint), ptr, size))
			return -EFAULT;

		sync.type = syncpoint.type;
		sync.priv = u64_to_user_ptr(syncpoint.priv);
		sync.size = syncpoint.size;

		ret = kgsl_drawobj_sync_add_sync(device, syncobj, &sync);
		if (ret)
			return ret;

		ptr += sizeof(syncpoint);
	}

	return 0;
}

void kgsl_drawobjs_cache_exit(void)
{
	kmem_cache_destroy(memobjs_cache);
}

int kgsl_drawobjs_cache_init(void)
{
	memobjs_cache = KMEM_CACHE(kgsl_memobj_node, 0);

	if (!memobjs_cache)
		return -ENOMEM;

	return 0;
}

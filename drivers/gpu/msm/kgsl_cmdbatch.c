/* Copyright (c) 2008-2015, The Linux Foundation. All rights reserved.
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

/*
 * KGSL command batch management
 * A command batch is a single submission from userland.  The cmdbatch
 * encapsulates everything about the submission : command buffers, flags and
 * sync points.
 *
 * Sync points are events that need to expire before the
 * cmdbatch can be queued to the hardware. For each sync point a
 * kgsl_cmdbatch_sync_event struct is created and added to a list in the
 * cmdbatch. There can be multiple types of events both internal ones (GPU
 * events) and external triggers. As the events expire the struct is deleted
 * from the list. The GPU will submit the command batch as soon as the list
 * goes empty indicating that all the sync points have been met.
 */

#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/compat.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_cmdbatch.h"
#include "kgsl_sync.h"
#include "kgsl_trace.h"
#include "kgsl_compat.h"
#include "adreno.h"

/*
 * Define an kmem cache for the memobj structures since we allocate and free
 * them so frequently
 */
static struct kmem_cache *memobjs_cache;

/**
 * kgsl_cmdbatch_put() - Decrement the refcount for a command batch object
 * @cmdbatch: Pointer to the command batch object
 */
static inline void kgsl_cmdbatch_put(struct kgsl_cmdbatch *cmdbatch)
{
	if (cmdbatch)
		kref_put(&cmdbatch->refcount, kgsl_cmdbatch_destroy_object);
}

/*
 * KGSL command batch management
 * A command batch is a single submission from userland.  The cmdbatch
 * encapsulates everything about the submission : command buffers, flags and
 * sync points.
 *
 * Sync points are events that need to expire before the
 * cmdbatch can be queued to the hardware. For each sync point a
 * kgsl_cmdbatch_sync_event struct is created and added to a list in the
 * cmdbatch. There can be multiple types of events both internal ones (GPU
 * events) and external triggers. As the events expire the struct is deleted
 * from the list. The GPU will submit the command batch as soon as the list
 * goes empty indicating that all the sync points have been met.
 */

void kgsl_dump_syncpoints(struct kgsl_device *device,
	struct kgsl_cmdbatch *cmdbatch)
{
	struct kgsl_cmdbatch_sync_event *event;

	/* Print all the pending sync objects */
	list_for_each_entry(event, &cmdbatch->synclist, node) {

		switch (event->type) {
		case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP: {
			unsigned int retired;

			 kgsl_readtimestamp(event->device,
				event->context, KGSL_TIMESTAMP_RETIRED,
				&retired);

			dev_err(device->dev,
				"  [timestamp] context %d timestamp %d (retired %d)\n",
				event->context->id, event->timestamp,
				retired);
			break;
		}
		case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
			if (event->handle)
				dev_err(device->dev, "  fence: [%p] %s\n",
					event->handle->fence,
					event->handle->name);
			else
				dev_err(device->dev, "  fence: invalid\n");
			break;
		}
	}
}

static void _kgsl_cmdbatch_timer(unsigned long data)
{
	struct kgsl_device *device;
	struct kgsl_cmdbatch *cmdbatch = (struct kgsl_cmdbatch *) data;
	struct kgsl_cmdbatch_sync_event *event;

	if (cmdbatch == NULL || cmdbatch->context == NULL)
		return;

	device = cmdbatch->context->device;

	dev_err(device->dev,
		"kgsl: possible gpu syncpoint deadlock for context %d timestamp %d\n",
		cmdbatch->context->id, cmdbatch->timestamp);

	set_bit(CMDBATCH_FLAG_FENCE_LOG, &cmdbatch->priv);
	kgsl_context_dump(cmdbatch->context);
	clear_bit(CMDBATCH_FLAG_FENCE_LOG, &cmdbatch->priv);

	spin_lock(&cmdbatch->lock);
	/* Print all the fences */
	list_for_each_entry(event, &cmdbatch->synclist, node) {
		if (KGSL_CMD_SYNCPOINT_TYPE_FENCE == event->type &&
			event->handle && event->handle->fence)
			kgsl_sync_fence_log(event->handle->fence);
	}
	spin_unlock(&cmdbatch->lock);
	dev_err(device->dev, "--gpu syncpoint deadlock print end--\n");
}

/**
 * kgsl_cmdbatch_sync_event_destroy() - Destroy a sync event object
 * @kref: Pointer to the kref structure for this object
 *
 * Actually destroy a sync event object.  Called from
 * kgsl_cmdbatch_sync_event_put.
 */
static void kgsl_cmdbatch_sync_event_destroy(struct kref *kref)
{
	struct kgsl_cmdbatch_sync_event *event = container_of(kref,
		struct kgsl_cmdbatch_sync_event, refcount);

	kgsl_cmdbatch_put(event->cmdbatch);
	kfree(event);
}

/**
 * kgsl_cmdbatch_sync_event_put() - Decrement the refcount for a
 *                                  sync event object
 * @event: Pointer to the sync event object
 */
static inline void kgsl_cmdbatch_sync_event_put(
	struct kgsl_cmdbatch_sync_event *event)
{
	kref_put(&event->refcount, kgsl_cmdbatch_sync_event_destroy);
}

/**
 * kgsl_cmdbatch_destroy_object() - Destroy a cmdbatch object
 * @kref: Pointer to the kref structure for this object
 *
 * Actually destroy a command batch object.  Called from kgsl_cmdbatch_put
 */
void kgsl_cmdbatch_destroy_object(struct kref *kref)
{
	struct kgsl_cmdbatch *cmdbatch = container_of(kref,
		struct kgsl_cmdbatch, refcount);

	kgsl_context_put(cmdbatch->context);

	kfree(cmdbatch);
}
EXPORT_SYMBOL(kgsl_cmdbatch_destroy_object);

/*
 * a generic function to retire a pending sync event and (possibly)
 * kick the dispatcher
 */
static void kgsl_cmdbatch_sync_expire(struct kgsl_device *device,
	struct kgsl_cmdbatch_sync_event *event)
{
	struct kgsl_cmdbatch_sync_event *e, *tmp;
	int sched = 0;
	int removed = 0;

	/*
	 * We may have cmdbatch timer running, which also uses same lock,
	 * take a lock with software interrupt disabled (bh) to avoid
	 * spin lock recursion.
	 */
	spin_lock_bh(&event->cmdbatch->lock);

	/*
	 * sync events that are contained by a cmdbatch which has been
	 * destroyed may have already been removed from the synclist
	 */

	list_for_each_entry_safe(e, tmp, &event->cmdbatch->synclist, node) {
		if (e == event) {
			list_del_init(&event->node);
			removed = 1;
			break;
		}
	}

	sched = list_empty(&event->cmdbatch->synclist) ? 1 : 0;
	spin_unlock_bh(&event->cmdbatch->lock);

	/* If the list is empty delete the canary timer */
	if (sched)
		del_timer_sync(&event->cmdbatch->timer);

	/*
	 * if this is the last event in the list then tell
	 * the GPU device that the cmdbatch can be submitted
	 */

	if (sched && device->ftbl->drawctxt_sched)
		device->ftbl->drawctxt_sched(device, event->cmdbatch->context);

	/* Put events that have been removed from the synclist */
	if (removed)
		kgsl_cmdbatch_sync_event_put(event);
}

/*
 * This function is called by the GPU event when the sync event timestamp
 * expires
 */
static void kgsl_cmdbatch_sync_func(struct kgsl_device *device,
		struct kgsl_event_group *group, void *priv, int result)
{
	struct kgsl_cmdbatch_sync_event *event = priv;

	trace_syncpoint_timestamp_expire(event->cmdbatch,
		event->context, event->timestamp);

	kgsl_cmdbatch_sync_expire(device, event);
	kgsl_context_put(event->context);
	/* Put events that have signaled */
	kgsl_cmdbatch_sync_event_put(event);
}

static inline void _free_memobj_list(struct list_head *list)
{
	struct kgsl_memobj_node *mem, *tmpmem;

	/* Free the cmd mem here */
	list_for_each_entry_safe(mem, tmpmem, list, node) {
		list_del_init(&mem->node);
		kmem_cache_free(memobjs_cache, mem);
	}
}

/**
 * kgsl_cmdbatch_destroy() - Destroy a cmdbatch structure
 * @cmdbatch: Pointer to the command batch object to destroy
 *
 * Start the process of destroying a command batch.  Cancel any pending events
 * and decrement the refcount.  Asynchronous events can still signal after
 * kgsl_cmdbatch_destroy has returned.
 */
void kgsl_cmdbatch_destroy(struct kgsl_cmdbatch *cmdbatch)
{
	struct kgsl_cmdbatch_sync_event *event, *tmpsync;
	LIST_HEAD(cancel_synclist);
	int sched = 0;

	if (IS_ERR_OR_NULL(cmdbatch))
		return;

	/* Zap the canary timer */
	del_timer_sync(&cmdbatch->timer);

	/* non-bh because we just destroyed timer */
	spin_lock(&cmdbatch->lock);

	/* Empty the synclist before canceling events */
	list_splice_init(&cmdbatch->synclist, &cancel_synclist);
	spin_unlock(&cmdbatch->lock);

	/*
	 * Finish canceling events outside the cmdbatch spinlock and
	 * require the cancel function to return if the event was
	 * successfully canceled meaning that the event is guaranteed
	 * not to signal the callback. This guarantee ensures that
	 * the reference count for the event and cmdbatch is correct.
	 */
	list_for_each_entry_safe(event, tmpsync, &cancel_synclist, node) {

		sched = 1;
		if (event->type == KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP) {
			/*
			 * Timestamp events are guaranteed to signal
			 * when canceled
			 */
			kgsl_cancel_event(cmdbatch->device,
				&event->context->events, event->timestamp,
				kgsl_cmdbatch_sync_func, event);
		} else if (event->type == KGSL_CMD_SYNCPOINT_TYPE_FENCE) {
			/* Put events that are successfully canceled */
			if (kgsl_sync_fence_async_cancel(event->handle))
				kgsl_cmdbatch_sync_event_put(event);
		}

		/* Put events that have been removed from the synclist */
		list_del_init(&event->node);
		kgsl_cmdbatch_sync_event_put(event);
	}

	/*
	 * Release the the refcount on the mem entry associated with the
	 * cmdbatch profiling buffer
	 */
	if (cmdbatch->flags & KGSL_CMDBATCH_PROFILING)
		kgsl_mem_entry_put(cmdbatch->profiling_buf_entry);

	/* Destroy the cmdlist we created */
	_free_memobj_list(&cmdbatch->cmdlist);

	/* Destroy the memlist we created */
	_free_memobj_list(&cmdbatch->memlist);

	/*
	 * If we cancelled an event, there's a good chance that the context is
	 * on a dispatcher queue, so schedule to get it removed.
	 */
	if (sched && cmdbatch->device->ftbl->drawctxt_sched)
		cmdbatch->device->ftbl->drawctxt_sched(cmdbatch->device,
							cmdbatch->context);

	kgsl_cmdbatch_put(cmdbatch);
}
EXPORT_SYMBOL(kgsl_cmdbatch_destroy);

/*
 * A callback that gets registered with kgsl_sync_fence_async_wait and is fired
 * when a fence is expired
 */
static void kgsl_cmdbatch_sync_fence_func(void *priv)
{
	struct kgsl_cmdbatch_sync_event *event = priv;

	trace_syncpoint_fence_expire(event->cmdbatch,
		event->handle ? event->handle->name : "unknown");

	kgsl_cmdbatch_sync_expire(event->device, event);
	/* Put events that have signaled */
	kgsl_cmdbatch_sync_event_put(event);
}

/* kgsl_cmdbatch_add_sync_fence() - Add a new sync fence syncpoint
 * @device: KGSL device
 * @cmdbatch: KGSL cmdbatch to add the sync point to
 * @priv: Private sructure passed by the user
 *
 * Add a new fence sync syncpoint to the cmdbatch.
 */
static int kgsl_cmdbatch_add_sync_fence(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void *priv)
{
	struct kgsl_cmd_syncpoint_fence *sync = priv;
	struct kgsl_cmdbatch_sync_event *event;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (event == NULL)
		return -ENOMEM;

	kref_get(&cmdbatch->refcount);

	event->type = KGSL_CMD_SYNCPOINT_TYPE_FENCE;
	event->cmdbatch = cmdbatch;
	event->device = device;
	event->context = NULL;

	/*
	 * Initial kref is to ensure async callback does not free the
	 * event before this function sets the event handle
	 */
	kref_init(&event->refcount);

	/*
	 * Add it to the list first to account for the possiblity that the
	 * callback will happen immediately after the call to
	 * kgsl_sync_fence_async_wait. Decrement the event refcount when
	 * removing from the synclist.
	 */

	kref_get(&event->refcount);

	/* non-bh because, we haven't started cmdbatch timer yet */
	spin_lock(&cmdbatch->lock);
	list_add(&event->node, &cmdbatch->synclist);
	spin_unlock(&cmdbatch->lock);

	/*
	 * Increment the reference count for the async callback.
	 * Decrement when the callback is successfully canceled, when
	 * the callback is signaled or if the async wait fails.
	 */

	kref_get(&event->refcount);
	event->handle = kgsl_sync_fence_async_wait(sync->fd,
		kgsl_cmdbatch_sync_fence_func, event);

	if (IS_ERR_OR_NULL(event->handle)) {
		int ret = PTR_ERR(event->handle);

		event->handle = NULL;

		/* Failed to add the event to the async callback */
		kgsl_cmdbatch_sync_event_put(event);

		/* Remove event from the synclist */
		spin_lock(&cmdbatch->lock);
		list_del(&event->node);
		spin_unlock(&cmdbatch->lock);
		kgsl_cmdbatch_sync_event_put(event);

		/* Event no longer needed by this function */
		kgsl_cmdbatch_sync_event_put(event);

		/*
		 * If ret == 0 the fence was already signaled - print a trace
		 * message so we can track that
		 */
		if (ret == 0)
			trace_syncpoint_fence_expire(cmdbatch, "signaled");

		return ret;
	}

	trace_syncpoint_fence(cmdbatch, event->handle->name);

	/*
	 * Event was successfully added to the synclist, the async
	 * callback and handle to cancel event has been set.
	 */
	kgsl_cmdbatch_sync_event_put(event);

	return 0;
}

/* kgsl_cmdbatch_add_sync_timestamp() - Add a new sync point for a cmdbatch
 * @device: KGSL device
 * @cmdbatch: KGSL cmdbatch to add the sync point to
 * @priv: Private sructure passed by the user
 *
 * Add a new sync point timestamp event to the cmdbatch.
 */
static int kgsl_cmdbatch_add_sync_timestamp(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void *priv)
{
	struct kgsl_cmd_syncpoint_timestamp *sync = priv;
	struct kgsl_context *context = kgsl_context_get(cmdbatch->device,
		sync->context_id);
	struct kgsl_cmdbatch_sync_event *event;
	int ret = -EINVAL;

	if (context == NULL)
		return -EINVAL;

	/*
	 * We allow somebody to create a sync point on their own context.
	 * This has the effect of delaying a command from submitting until the
	 * dependent command has cleared.  That said we obviously can't let them
	 * create a sync point on a future timestamp.
	 */

	if (context == cmdbatch->context) {
		unsigned int queued;
		kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_QUEUED,
			&queued);

		if (timestamp_cmp(sync->timestamp, queued) > 0) {
			KGSL_DRV_ERR(device,
			"Cannot create syncpoint for future timestamp %d (current %d)\n",
				sync->timestamp, queued);
			goto done;
		}
	}

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (event == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	kref_get(&cmdbatch->refcount);

	event->type = KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP;
	event->cmdbatch = cmdbatch;
	event->context = context;
	event->timestamp = sync->timestamp;
	event->device = device;

	/*
	 * Two krefs are required to support events. The first kref is for
	 * the synclist which holds the event in the cmdbatch. The second
	 * kref is for the callback which can be asynchronous and be called
	 * after kgsl_cmdbatch_destroy. The kref should be put when the event
	 * is removed from the synclist, if the callback is successfully
	 * canceled or when the callback is signaled.
	 */
	kref_init(&event->refcount);
	kref_get(&event->refcount);

	/* non-bh because, we haven't started cmdbatch timer yet */
	spin_lock(&cmdbatch->lock);
	list_add(&event->node, &cmdbatch->synclist);
	spin_unlock(&cmdbatch->lock);

	ret = kgsl_add_event(device, &context->events, sync->timestamp,
		kgsl_cmdbatch_sync_func, event);

	if (ret) {
		spin_lock(&cmdbatch->lock);
		list_del(&event->node);
		spin_unlock(&cmdbatch->lock);

		kgsl_cmdbatch_put(cmdbatch);
		kfree(event);
	} else {
		trace_syncpoint_timestamp(cmdbatch, context, sync->timestamp);
	}

done:
	if (ret)
		kgsl_context_put(context);

	return ret;
}

/**
 * kgsl_cmdbatch_add_sync() - Add a sync point to a command batch
 * @device: Pointer to the KGSL device struct for the GPU
 * @cmdbatch: Pointer to the cmdbatch
 * @sync: Pointer to the user-specified struct defining the syncpoint
 *
 * Create a new sync point in the cmdbatch based on the user specified
 * parameters
 */
int kgsl_cmdbatch_add_sync(struct kgsl_device *device,
	struct kgsl_cmdbatch *cmdbatch,
	struct kgsl_cmd_syncpoint *sync)
{
	void *priv;
	int ret, psize;
	int (*func)(struct kgsl_device *device, struct kgsl_cmdbatch *cmdbatch,
			void *priv);

	switch (sync->type) {
	case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
		psize = sizeof(struct kgsl_cmd_syncpoint_timestamp);
		func = kgsl_cmdbatch_add_sync_timestamp;
		break;
	case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
		psize = sizeof(struct kgsl_cmd_syncpoint_fence);
		func = kgsl_cmdbatch_add_sync_fence;
		break;
	default:
		KGSL_DRV_ERR(device,
			"bad syncpoint type ctxt %d type 0x%x size %zu\n",
			cmdbatch->context->id, sync->type, sync->size);
		return -EINVAL;
	}

	if (sync->size != psize) {
		KGSL_DRV_ERR(device,
			"bad syncpoint size ctxt %d type 0x%x size %zu\n",
			cmdbatch->context->id, sync->type, sync->size);
		return -EINVAL;
	}

	priv = kzalloc(sync->size, GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	if (copy_from_user(priv, sync->priv, sync->size)) {
		kfree(priv);
		return -EFAULT;
	}

	ret = func(device, cmdbatch, priv);
	kfree(priv);

	return ret;
}

static void add_profiling_buffer(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, uint64_t gpuaddr, uint64_t size,
		unsigned int id, uint64_t offset)
{
	struct kgsl_mem_entry *entry;

	if (!(cmdbatch->flags & KGSL_CMDBATCH_PROFILING))
		return;

	/* Only the first buffer entry counts - ignore the rest */
	if (cmdbatch->profiling_buf_entry != NULL)
		return;

	if (id != 0) {
		entry = kgsl_sharedmem_find_id(cmdbatch->context->proc_priv,
				id);

		/* Make sure the offset is in range */
		if (entry && offset > entry->memdesc.size) {
			kgsl_mem_entry_put(entry);
			entry = NULL;
		}
	} else {
		entry = kgsl_sharedmem_find_region(cmdbatch->context->proc_priv,
			gpuaddr, size);
	}

	if (entry == NULL) {
		KGSL_DRV_ERR(device,
			"ignore bad profile buffer ctxt %d id %d offset %lld gpuaddr %llx size %lld\n",
			cmdbatch->context->id, id, offset, gpuaddr, size);
		return;
	}

	cmdbatch->profiling_buf_entry = entry;

	if (id != 0)
		cmdbatch->profiling_buffer_gpuaddr =
			entry->memdesc.gpuaddr + offset;
	else
		cmdbatch->profiling_buffer_gpuaddr = gpuaddr;
}

/**
 * kgsl_cmdbatch_add_ibdesc() - Add a legacy ibdesc to a command batch
 * @cmdbatch: Pointer to the cmdbatch
 * @ibdesc: Pointer to the user-specified struct defining the memory or IB
 *
 * Create a new memory entry in the cmdbatch based on the user specified
 * parameters
 */
int kgsl_cmdbatch_add_ibdesc(struct kgsl_device *device,
	struct kgsl_cmdbatch *cmdbatch, struct kgsl_ibdesc *ibdesc)
{
	struct kgsl_memobj_node *mem;

	mem = kmem_cache_alloc(memobjs_cache, GFP_KERNEL);
	if (mem == NULL)
		return -ENOMEM;

	mem->gpuaddr = (uint64_t) ibdesc->gpuaddr;
	mem->size = (uint64_t) ibdesc->sizedwords << 2;
	mem->priv = 0;
	mem->id = 0;
	mem->offset = 0;
	mem->flags = 0;

	/* sanitize the ibdesc ctrl flags */
	ibdesc->ctrl &= KGSL_IBDESC_MEMLIST | KGSL_IBDESC_PROFILING_BUFFER;

	if (cmdbatch->flags & KGSL_CMDBATCH_MEMLIST &&
			ibdesc->ctrl & KGSL_IBDESC_MEMLIST) {
		if (ibdesc->ctrl & KGSL_IBDESC_PROFILING_BUFFER) {
			add_profiling_buffer(device, cmdbatch, mem->gpuaddr,
					mem->size, 0, 0);
			return 0;
		}

		/* add to the memlist */
		list_add_tail(&mem->node, &cmdbatch->memlist);

		/*
		 * If the memlist contains a cmdbatch profiling buffer, store
		 * the mem_entry containing the buffer and the gpuaddr at
		 * which the buffer can be found
		 */
		if (cmdbatch->flags & KGSL_CMDBATCH_PROFILING &&
			ibdesc->ctrl & KGSL_IBDESC_PROFILING_BUFFER &&
			!cmdbatch->profiling_buf_entry) {
			cmdbatch->profiling_buf_entry =
				kgsl_sharedmem_find_region(
				cmdbatch->context->proc_priv, mem->gpuaddr,
				mem->size);
			if (!cmdbatch->profiling_buf_entry) {
				WARN_ONCE(1,
				"No mem entry for profiling buf, gpuaddr=%llx\n",
				mem->gpuaddr);
				return 0;
			}

			cmdbatch->profiling_buffer_gpuaddr = mem->gpuaddr;
		}
	} else {
		/* Ignore if SYNC or MARKER is specified */
		if (cmdbatch->flags &
			(KGSL_CMDBATCH_SYNC | KGSL_CMDBATCH_MARKER))
			return 0;

		/* set the preamble flag if directed to */
		if (cmdbatch->context->flags & KGSL_CONTEXT_PREAMBLE &&
			list_empty(&cmdbatch->cmdlist))
			mem->flags = KGSL_CMDLIST_CTXTSWITCH_PREAMBLE;

		/* add to the cmd list */
		list_add_tail(&mem->node, &cmdbatch->cmdlist);
	}

	return 0;
}

/**
 * kgsl_cmdbatch_create() - Create a new cmdbatch structure
 * @device: Pointer to a KGSL device struct
 * @context: Pointer to a KGSL context struct
 * @flags: Flags for the cmdbatch
 *
 * Allocate an new cmdbatch structure
 */
struct kgsl_cmdbatch *kgsl_cmdbatch_create(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int flags)
{
	struct kgsl_cmdbatch *cmdbatch = kzalloc(sizeof(*cmdbatch), GFP_KERNEL);
	if (cmdbatch == NULL)
		return ERR_PTR(-ENOMEM);

	/*
	 * Increase the reference count on the context so it doesn't disappear
	 * during the lifetime of this command batch
	 */

	if (!_kgsl_context_get(context)) {
		kfree(cmdbatch);
		return ERR_PTR(-EINVAL);
	}

	kref_init(&cmdbatch->refcount);
	INIT_LIST_HEAD(&cmdbatch->cmdlist);
	INIT_LIST_HEAD(&cmdbatch->synclist);
	INIT_LIST_HEAD(&cmdbatch->memlist);
	spin_lock_init(&cmdbatch->lock);

	cmdbatch->device = device;
	cmdbatch->context = context;
	/* sanitize our flags for cmdbatches */
	cmdbatch->flags = flags & (KGSL_CMDBATCH_CTX_SWITCH
				| KGSL_CMDBATCH_MARKER
				| KGSL_CMDBATCH_END_OF_FRAME
				| KGSL_CMDBATCH_SYNC
				| KGSL_CMDBATCH_PWR_CONSTRAINT
				| KGSL_CMDBATCH_MEMLIST
				| KGSL_CMDBATCH_PROFILING);

	/* Add a timer to help debug sync deadlocks */
	setup_timer(&cmdbatch->timer, _kgsl_cmdbatch_timer,
		(unsigned long) cmdbatch);

	return cmdbatch;
}

#ifdef CONFIG_COMPAT
static int add_ibdesc_list_compat(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr, int count)
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

		ret = kgsl_cmdbatch_add_ibdesc(device, cmdbatch, &ibdesc);
		if (ret)
			break;

		ptr += sizeof(ibdesc32);
	}

	return ret;
}

static int add_syncpoints_compat(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr, int count)
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

		ret = kgsl_cmdbatch_add_sync(device, cmdbatch, &sync);
		if (ret)
			break;

		ptr += sizeof(sync32);
	}

	return ret;
}
#else
static int add_ibdesc_list_compat(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr, int count)
{
	return -EINVAL;
}

static int add_syncpoints_compat(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr, int count)
{
	return -EINVAL;
}
#endif

int kgsl_cmdbatch_add_ibdesc_list(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr, int count)
{
	struct kgsl_ibdesc ibdesc;
	int i, ret;

	if (is_compat_task())
		return add_ibdesc_list_compat(device, cmdbatch, ptr, count);

	for (i = 0; i < count; i++) {
		memset(&ibdesc, 0, sizeof(ibdesc));

		if (copy_from_user(&ibdesc, ptr, sizeof(ibdesc)))
			return -EFAULT;

		ret = kgsl_cmdbatch_add_ibdesc(device, cmdbatch, &ibdesc);
		if (ret)
			return ret;

		ptr += sizeof(ibdesc);
	}

	return 0;
}

int kgsl_cmdbatch_add_syncpoints(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr, int count)
{
	struct kgsl_cmd_syncpoint sync;
	int i, ret;

	if (is_compat_task())
		return add_syncpoints_compat(device, cmdbatch, ptr, count);

	for (i = 0; i < count; i++) {
		memset(&sync, 0, sizeof(sync));

		if (copy_from_user(&sync, ptr, sizeof(sync)))
			return -EFAULT;

		ret = kgsl_cmdbatch_add_sync(device, cmdbatch, &sync);
		if (ret)
			return ret;

		ptr += sizeof(sync);
	}

	return 0;
}

static int kgsl_cmdbatch_add_object(struct list_head *head,
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

int kgsl_cmdbatch_add_cmdlist(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr,
		unsigned int size, unsigned int count)
{
	struct kgsl_command_object obj;
	int i, ret = 0;

	/* Return early if nothing going on */
	if (count == 0 && ptr == NULL && size == 0)
		return 0;

	/* Sanity check inputs */
	if (count == 0 || ptr == NULL || size == 0)
		return -EINVAL;

	/* Ignore all if SYNC or MARKER is specified */
	if (cmdbatch->flags & (KGSL_CMDBATCH_SYNC | KGSL_CMDBATCH_MARKER))
		return 0;

	for (i = 0; i < count; i++) {
		memset(&obj, 0, sizeof(obj));

		ret = _copy_from_user(&obj, ptr, sizeof(obj), size);
		if (ret)
			return ret;

		/* Sanity check the flags */
		if (!(obj.flags & CMDLIST_FLAGS)) {
			KGSL_DRV_ERR(device,
				"invalid cmdobj ctxt %d flags %d id %d offset %lld addr %lld size %lld\n",
				cmdbatch->context->id, obj.flags, obj.id,
				obj.offset, obj.gpuaddr, obj.size);
			return -EINVAL;
		}

		ret = kgsl_cmdbatch_add_object(&cmdbatch->cmdlist, &obj);
		if (ret)
			return ret;

		ptr += sizeof(obj);
	}

	return 0;
}

int kgsl_cmdbatch_add_memlist(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr,
		unsigned int size, unsigned int count)
{
	struct kgsl_command_object obj;
	int i, ret = 0;

	/* Return early if nothing going on */
	if (count == 0 && ptr == NULL && size == 0)
		return 0;

	/* Sanity check inputs */
	if (count == 0 || ptr == NULL || size == 0)
		return -EINVAL;

	for (i = 0; i < count; i++) {
		memset(&obj, 0, sizeof(obj));

		ret = _copy_from_user(&obj, ptr, sizeof(obj), size);
		if (ret)
			return ret;

		if (!(obj.flags & KGSL_OBJLIST_MEMOBJ)) {
			KGSL_DRV_ERR(device,
				"invalid memobj ctxt %d flags %d id %d offset %lld addr %lld size %lld\n",
				cmdbatch->context->id, obj.flags, obj.id,
				obj.offset, obj.gpuaddr, obj.size);
			return -EINVAL;
		}

		if (obj.flags & KGSL_OBJLIST_PROFILE)
			add_profiling_buffer(device, cmdbatch, obj.gpuaddr,
				obj.size, obj.id, obj.offset);
		else {
			ret = kgsl_cmdbatch_add_object(&cmdbatch->memlist,
				&obj);
			if (ret)
				return ret;
		}

		ptr += sizeof(obj);
	}

	return 0;
}

int kgsl_cmdbatch_add_synclist(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr,
		unsigned int size, unsigned int count)
{
	struct kgsl_command_syncpoint syncpoint;
	struct kgsl_cmd_syncpoint sync;
	int i, ret = 0;

	/* Return early if nothing going on */
	if (count == 0 && ptr == NULL && size == 0)
		return 0;

	/* Sanity check inputs */
	if (count == 0 || ptr == NULL || size == 0)
		return -EINVAL;

	for (i = 0; i < count; i++) {
		memset(&syncpoint, 0, sizeof(syncpoint));

		ret = _copy_from_user(&syncpoint, ptr, sizeof(syncpoint), size);
		if (ret)
			return ret;

		sync.type = syncpoint.type;
		sync.priv = (void __user *) (uintptr_t) syncpoint.priv;
		sync.size = syncpoint.size;

		ret = kgsl_cmdbatch_add_sync(device, cmdbatch, &sync);
		if (ret)
			return ret;

		ptr += sizeof(syncpoint);
	}

	return 0;
}

void kgsl_cmdbatch_exit(void)
{
	if (memobjs_cache != NULL)
		kmem_cache_destroy(memobjs_cache);
}

int kgsl_cmdbatch_init(void)
{
	memobjs_cache = KMEM_CACHE(kgsl_memobj_node, 0);
	if (memobjs_cache == NULL) {
		KGSL_CORE_ERR("failed to create memobjs_cache");
		return -ENOMEM;
	}

	return 0;
}

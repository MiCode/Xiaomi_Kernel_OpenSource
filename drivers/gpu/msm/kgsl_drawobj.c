/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/compat.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_drawobj.h"
#include "kgsl_sync.h"
#include "kgsl_trace.h"
#include "kgsl_compat.h"

/*
 * Define an kmem cache for the memobj structures since we allocate and free
 * them so frequently
 */
static struct kmem_cache *memobjs_cache;

/**
 * kgsl_drawobj_put() - Decrement the refcount for a drawobj object
 * @drawobj: Pointer to the drawobj object
 */
static inline void kgsl_drawobj_put(struct kgsl_drawobj *drawobj)
{
	if (drawobj)
		kref_put(&drawobj->refcount, kgsl_drawobj_destroy_object);
}

void kgsl_dump_syncpoints(struct kgsl_device *device,
	struct kgsl_drawobj *drawobj)
{
	struct kgsl_drawobj_sync_event *event;
	unsigned int i;

	for (i = 0; i < drawobj->numsyncs; i++) {
		event = &drawobj->synclist[i];

		if (!kgsl_drawobj_event_pending(drawobj, i))
			continue;

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
				dev_err(device->dev, "  fence: [%pK] %s\n",
					event->handle->fence,
					event->handle->name);
			else
				dev_err(device->dev, "  fence: invalid\n");
			break;
		}
	}
}

static void _kgsl_drawobj_timer(unsigned long data)
{
	struct kgsl_device *device;
	struct kgsl_drawobj *drawobj = (struct kgsl_drawobj *) data;
	struct kgsl_drawobj_sync_event *event;
	unsigned int i;

	if (drawobj == NULL || drawobj->context == NULL)
		return;

	device = drawobj->context->device;

	dev_err(device->dev,
		"kgsl: possible gpu syncpoint deadlock for context %d timestamp %d\n",
		drawobj->context->id, drawobj->timestamp);

	set_bit(DRAWOBJ_FLAG_FENCE_LOG, &drawobj->priv);
	kgsl_context_dump(drawobj->context);
	clear_bit(DRAWOBJ_FLAG_FENCE_LOG, &drawobj->priv);

	dev_err(device->dev, "      pending events:\n");

	for (i = 0; i < drawobj->numsyncs; i++) {
		event = &drawobj->synclist[i];

		if (!kgsl_drawobj_event_pending(drawobj, i))
			continue;

		switch (event->type) {
		case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
			dev_err(device->dev, "       [%d] TIMESTAMP %d:%d\n",
				i, event->context->id, event->timestamp);
			break;
		case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
			if (event->handle != NULL) {
				dev_err(device->dev, "       [%d] FENCE %s\n",
				i, event->handle->fence ?
					event->handle->fence->name : "NULL");
				kgsl_sync_fence_log(event->handle->fence);
			}
			break;
		}
	}

	dev_err(device->dev, "--gpu syncpoint deadlock print end--\n");
}

/**
 * kgsl_drawobj_destroy_object() - Destroy a drawobj object
 * @kref: Pointer to the kref structure for this object
 *
 * Actually destroy a drawobj object.  Called from kgsl_drawobj_put
 */
void kgsl_drawobj_destroy_object(struct kref *kref)
{
	struct kgsl_drawobj *drawobj = container_of(kref,
		struct kgsl_drawobj, refcount);

	kgsl_context_put(drawobj->context);

	kfree(drawobj->synclist);
	kfree(drawobj);
}
EXPORT_SYMBOL(kgsl_drawobj_destroy_object);

/*
 * a generic function to retire a pending sync event and (possibly)
 * kick the dispatcher
 */
static void kgsl_drawobj_sync_expire(struct kgsl_device *device,
	struct kgsl_drawobj_sync_event *event)
{
	/*
	 * Clear the event from the pending mask - if it is already clear, then
	 * leave without doing anything useful
	 */
	if (!test_and_clear_bit(event->id, &event->drawobj->pending))
		return;

	/*
	 * If no more pending events, delete the timer and schedule the command
	 * for dispatch
	 */
	if (!kgsl_drawobj_events_pending(event->drawobj)) {
		del_timer_sync(&event->drawobj->timer);

		if (device->ftbl->drawctxt_sched)
			device->ftbl->drawctxt_sched(device,
				event->drawobj->context);
	}
}

/*
 * This function is called by the GPU event when the sync event timestamp
 * expires
 */
static void kgsl_drawobj_sync_func(struct kgsl_device *device,
		struct kgsl_event_group *group, void *priv, int result)
{
	struct kgsl_drawobj_sync_event *event = priv;

	trace_syncpoint_timestamp_expire(event->drawobj,
		event->context, event->timestamp);

	kgsl_drawobj_sync_expire(device, event);
	kgsl_context_put(event->context);
	kgsl_drawobj_put(event->drawobj);
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
 * kgsl_drawobj_destroy() - Destroy a drawobj structure
 * @drawobj: Pointer to the drawobj object to destroy
 *
 * Start the process of destroying a drawobj.  Cancel any pending events
 * and decrement the refcount.  Asynchronous events can still signal after
 * kgsl_drawobj_destroy has returned.
 */
void kgsl_drawobj_destroy(struct kgsl_drawobj *drawobj)
{
	unsigned int i;
	unsigned long pending;

	if (IS_ERR_OR_NULL(drawobj))
		return;

	/* Zap the canary timer */
	del_timer_sync(&drawobj->timer);

	/*
	 * Copy off the pending list and clear all pending events - this will
	 * render any subsequent asynchronous callback harmless
	 */
	bitmap_copy(&pending, &drawobj->pending, KGSL_MAX_SYNCPOINTS);
	bitmap_zero(&drawobj->pending, KGSL_MAX_SYNCPOINTS);

	/*
	 * Clear all pending events - this will render any subsequent async
	 * callbacks harmless
	 */

	for (i = 0; i < drawobj->numsyncs; i++) {
		struct kgsl_drawobj_sync_event *event = &drawobj->synclist[i];

		/* Don't do anything if the event has already expired */
		if (!test_bit(i, &pending))
			continue;

		switch (event->type) {
		case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
			kgsl_cancel_event(drawobj->device,
				&event->context->events, event->timestamp,
				kgsl_drawobj_sync_func, event);
			break;
		case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
			if (kgsl_sync_fence_async_cancel(event->handle))
				kgsl_drawobj_put(drawobj);
			break;
		}
	}

	/*
	 * Release the the refcount on the mem entry associated with the
	 * drawobj profiling buffer
	 */
	if (drawobj->flags & KGSL_DRAWOBJ_PROFILING)
		kgsl_mem_entry_put(drawobj->profiling_buf_entry);

	/* Destroy the cmdlist we created */
	_free_memobj_list(&drawobj->cmdlist);

	/* Destroy the memlist we created */
	_free_memobj_list(&drawobj->memlist);

	/*
	 * If we cancelled an event, there's a good chance that the context is
	 * on a dispatcher queue, so schedule to get it removed.
	 */
	if (!bitmap_empty(&pending, KGSL_MAX_SYNCPOINTS) &&
		drawobj->device->ftbl->drawctxt_sched)
		drawobj->device->ftbl->drawctxt_sched(drawobj->device,
							drawobj->context);

	kgsl_drawobj_put(drawobj);
}
EXPORT_SYMBOL(kgsl_drawobj_destroy);

/*
 * A callback that gets registered with kgsl_sync_fence_async_wait and is fired
 * when a fence is expired
 */
static void kgsl_drawobj_sync_fence_func(void *priv)
{
	struct kgsl_drawobj_sync_event *event = priv;

	trace_syncpoint_fence_expire(event->drawobj,
		event->handle ? event->handle->name : "unknown");

	kgsl_drawobj_sync_expire(event->device, event);

	kgsl_drawobj_put(event->drawobj);
}

/* kgsl_drawobj_add_sync_fence() - Add a new sync fence syncpoint
 * @device: KGSL device
 * @drawobj: KGSL drawobj to add the sync point to
 * @priv: Private structure passed by the user
 *
 * Add a new fence sync syncpoint to the drawobj.
 */
static int kgsl_drawobj_add_sync_fence(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void *priv)
{
	struct kgsl_cmd_syncpoint_fence *sync = priv;
	struct kgsl_drawobj_sync_event *event;
	unsigned int id;

	kref_get(&drawobj->refcount);

	id = drawobj->numsyncs++;

	event = &drawobj->synclist[id];

	event->id = id;
	event->type = KGSL_CMD_SYNCPOINT_TYPE_FENCE;
	event->drawobj = drawobj;
	event->device = device;
	event->context = NULL;

	set_bit(event->id, &drawobj->pending);

	event->handle = kgsl_sync_fence_async_wait(sync->fd,
		kgsl_drawobj_sync_fence_func, event);

	if (IS_ERR_OR_NULL(event->handle)) {
		int ret = PTR_ERR(event->handle);

		clear_bit(event->id, &drawobj->pending);
		event->handle = NULL;

		kgsl_drawobj_put(drawobj);

		/*
		 * If ret == 0 the fence was already signaled - print a trace
		 * message so we can track that
		 */
		if (ret == 0)
			trace_syncpoint_fence_expire(drawobj, "signaled");

		return ret;
	}

	trace_syncpoint_fence(drawobj, event->handle->name);

	return 0;
}

/* kgsl_drawobj_add_sync_timestamp() - Add a new sync point for a drawobj
 * @device: KGSL device
 * @drawobj: KGSL drawobj to add the sync point to
 * @priv: Private structure passed by the user
 *
 * Add a new sync point timestamp event to the drawobj.
 */
static int kgsl_drawobj_add_sync_timestamp(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void *priv)
{
	struct kgsl_cmd_syncpoint_timestamp *sync = priv;
	struct kgsl_context *context = kgsl_context_get(drawobj->device,
		sync->context_id);
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

		if (timestamp_cmp(sync->timestamp, queued) > 0) {
			KGSL_DRV_ERR(device,
			"Cannot create syncpoint for future timestamp %d (current %d)\n",
				sync->timestamp, queued);
			goto done;
		}
	}

	kref_get(&drawobj->refcount);

	id = drawobj->numsyncs++;

	event = &drawobj->synclist[id];
	event->id = id;

	event->type = KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP;
	event->drawobj = drawobj;
	event->context = context;
	event->timestamp = sync->timestamp;
	event->device = device;

	set_bit(event->id, &drawobj->pending);

	ret = kgsl_add_event(device, &context->events, sync->timestamp,
		kgsl_drawobj_sync_func, event);

	if (ret) {
		clear_bit(event->id, &drawobj->pending);
		kgsl_drawobj_put(drawobj);
	} else {
		trace_syncpoint_timestamp(drawobj, context, sync->timestamp);
	}

done:
	if (ret)
		kgsl_context_put(context);

	return ret;
}

/**
 * kgsl_drawobj_add_sync() - Add a sync point to a drawobj
 * @device: Pointer to the KGSL device struct for the GPU
 * @drawobj: Pointer to the drawobj
 * @sync: Pointer to the user-specified struct defining the syncpoint
 *
 * Create a new sync point in the drawobj based on the user specified
 * parameters
 */
int kgsl_drawobj_add_sync(struct kgsl_device *device,
	struct kgsl_drawobj *drawobj,
	struct kgsl_cmd_syncpoint *sync)
{
	void *priv;
	int ret, psize;
	int (*func)(struct kgsl_device *device, struct kgsl_drawobj *drawobj,
			void *priv);

	switch (sync->type) {
	case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
		psize = sizeof(struct kgsl_cmd_syncpoint_timestamp);
		func = kgsl_drawobj_add_sync_timestamp;
		break;
	case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
		psize = sizeof(struct kgsl_cmd_syncpoint_fence);
		func = kgsl_drawobj_add_sync_fence;
		break;
	default:
		KGSL_DRV_ERR(device,
			"bad syncpoint type ctxt %d type 0x%x size %zu\n",
			drawobj->context->id, sync->type, sync->size);
		return -EINVAL;
	}

	if (sync->size != psize) {
		KGSL_DRV_ERR(device,
			"bad syncpoint size ctxt %d type 0x%x size %zu\n",
			drawobj->context->id, sync->type, sync->size);
		return -EINVAL;
	}

	priv = kzalloc(sync->size, GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	if (copy_from_user(priv, sync->priv, sync->size)) {
		kfree(priv);
		return -EFAULT;
	}

	ret = func(device, drawobj, priv);
	kfree(priv);

	return ret;
}

static void add_profiling_buffer(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, uint64_t gpuaddr, uint64_t size,
		unsigned int id, uint64_t offset)
{
	struct kgsl_mem_entry *entry;

	if (!(drawobj->flags & KGSL_DRAWOBJ_PROFILING))
		return;

	/* Only the first buffer entry counts - ignore the rest */
	if (drawobj->profiling_buf_entry != NULL)
		return;

	if (id != 0)
		entry = kgsl_sharedmem_find_id(drawobj->context->proc_priv,
				id);
	else
		entry = kgsl_sharedmem_find(drawobj->context->proc_priv,
			gpuaddr);

	if (entry != NULL) {
		if (!kgsl_gpuaddr_in_memdesc(&entry->memdesc, gpuaddr, size)) {
			kgsl_mem_entry_put(entry);
			entry = NULL;
		}
	}

	if (entry == NULL) {
		KGSL_DRV_ERR(device,
			"ignore bad profile buffer ctxt %d id %d offset %lld gpuaddr %llx size %lld\n",
			drawobj->context->id, id, offset, gpuaddr, size);
		return;
	}

	drawobj->profiling_buf_entry = entry;

	if (id != 0)
		drawobj->profiling_buffer_gpuaddr =
			entry->memdesc.gpuaddr + offset;
	else
		drawobj->profiling_buffer_gpuaddr = gpuaddr;
}

/**
 * kgsl_drawobj_add_ibdesc() - Add a legacy ibdesc to a drawobj
 * @drawobj: Pointer to the drawobj
 * @ibdesc: Pointer to the user-specified struct defining the memory or IB
 *
 * Create a new memory entry in the drawobj based on the user specified
 * parameters
 */
int kgsl_drawobj_add_ibdesc(struct kgsl_device *device,
	struct kgsl_drawobj *drawobj, struct kgsl_ibdesc *ibdesc)
{
	uint64_t gpuaddr = (uint64_t) ibdesc->gpuaddr;
	uint64_t size = (uint64_t) ibdesc->sizedwords << 2;
	struct kgsl_memobj_node *mem;

	/* sanitize the ibdesc ctrl flags */
	ibdesc->ctrl &= KGSL_IBDESC_MEMLIST | KGSL_IBDESC_PROFILING_BUFFER;

	if (drawobj->flags & KGSL_DRAWOBJ_MEMLIST &&
			ibdesc->ctrl & KGSL_IBDESC_MEMLIST) {
		if (ibdesc->ctrl & KGSL_IBDESC_PROFILING_BUFFER) {
			add_profiling_buffer(device, drawobj,
					gpuaddr, size, 0, 0);
			return 0;
		}
	}

	if (drawobj->flags & (KGSL_DRAWOBJ_SYNC | KGSL_DRAWOBJ_MARKER))
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
			ibdesc->ctrl & KGSL_IBDESC_MEMLIST) {
		/* add to the memlist */
		list_add_tail(&mem->node, &drawobj->memlist);
	} else {
		/* set the preamble flag if directed to */
		if (drawobj->context->flags & KGSL_CONTEXT_PREAMBLE &&
			list_empty(&drawobj->cmdlist))
			mem->flags = KGSL_CMDLIST_CTXTSWITCH_PREAMBLE;

		/* add to the cmd list */
		list_add_tail(&mem->node, &drawobj->cmdlist);
	}

	return 0;
}

/**
 * kgsl_drawobj_create() - Create a new drawobj structure
 * @device: Pointer to a KGSL device struct
 * @context: Pointer to a KGSL context struct
 * @flags: Flags for the drawobj
 *
 * Allocate an new drawobj structure
 */
struct kgsl_drawobj *kgsl_drawobj_create(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int flags)
{
	struct kgsl_drawobj *drawobj = kzalloc(sizeof(*drawobj), GFP_KERNEL);

	if (drawobj == NULL)
		return ERR_PTR(-ENOMEM);

	/*
	 * Increase the reference count on the context so it doesn't disappear
	 * during the lifetime of this drawobj
	 */

	if (!_kgsl_context_get(context)) {
		kfree(drawobj);
		return ERR_PTR(-ENOENT);
	}

	kref_init(&drawobj->refcount);
	INIT_LIST_HEAD(&drawobj->cmdlist);
	INIT_LIST_HEAD(&drawobj->memlist);

	drawobj->device = device;
	drawobj->context = context;
	/* sanitize our flags for drawobj's */
	drawobj->flags = flags & (KGSL_DRAWOBJ_CTX_SWITCH
				| KGSL_DRAWOBJ_MARKER
				| KGSL_DRAWOBJ_END_OF_FRAME
				| KGSL_DRAWOBJ_SYNC
				| KGSL_DRAWOBJ_PWR_CONSTRAINT
				| KGSL_DRAWOBJ_MEMLIST
				| KGSL_DRAWOBJ_PROFILING
				| KGSL_DRAWOBJ_PROFILING_KTIME);

	/* Add a timer to help debug sync deadlocks */
	setup_timer(&drawobj->timer, _kgsl_drawobj_timer,
		(unsigned long) drawobj);

	return drawobj;
}

#ifdef CONFIG_COMPAT
static int add_ibdesc_list_compat(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr, int count)
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

		ret = kgsl_drawobj_add_ibdesc(device, drawobj, &ibdesc);
		if (ret)
			break;

		ptr += sizeof(ibdesc32);
	}

	return ret;
}

static int add_syncpoints_compat(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr, int count)
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

		ret = kgsl_drawobj_add_sync(device, drawobj, &sync);
		if (ret)
			break;

		ptr += sizeof(sync32);
	}

	return ret;
}
#else
static int add_ibdesc_list_compat(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr, int count)
{
	return -EINVAL;
}

static int add_syncpoints_compat(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr, int count)
{
	return -EINVAL;
}
#endif

int kgsl_drawobj_add_ibdesc_list(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr, int count)
{
	struct kgsl_ibdesc ibdesc;
	int i, ret;

	if (is_compat_task())
		return add_ibdesc_list_compat(device, drawobj, ptr, count);

	for (i = 0; i < count; i++) {
		memset(&ibdesc, 0, sizeof(ibdesc));

		if (copy_from_user(&ibdesc, ptr, sizeof(ibdesc)))
			return -EFAULT;

		ret = kgsl_drawobj_add_ibdesc(device, drawobj, &ibdesc);
		if (ret)
			return ret;

		ptr += sizeof(ibdesc);
	}

	return 0;
}

int kgsl_drawobj_add_syncpoints(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr, int count)
{
	struct kgsl_cmd_syncpoint sync;
	int i, ret;

	if (count == 0)
		return 0;

	if (count > KGSL_MAX_SYNCPOINTS)
		return -EINVAL;

	drawobj->synclist = kcalloc(count,
		sizeof(struct kgsl_drawobj_sync_event), GFP_KERNEL);

	if (drawobj->synclist == NULL)
		return -ENOMEM;

	if (is_compat_task())
		return add_syncpoints_compat(device, drawobj, ptr, count);

	for (i = 0; i < count; i++) {
		memset(&sync, 0, sizeof(sync));

		if (copy_from_user(&sync, ptr, sizeof(sync)))
			return -EFAULT;

		ret = kgsl_drawobj_add_sync(device, drawobj, &sync);
		if (ret)
			return ret;

		ptr += sizeof(sync);
	}

	return 0;
}

static int kgsl_drawobj_add_object(struct list_head *head,
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

int kgsl_drawobj_add_cmdlist(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr,
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
	if (drawobj->flags & (KGSL_DRAWOBJ_SYNC | KGSL_DRAWOBJ_MARKER))
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
				drawobj->context->id, obj.flags, obj.id,
				obj.offset, obj.gpuaddr, obj.size);
			return -EINVAL;
		}

		ret = kgsl_drawobj_add_object(&drawobj->cmdlist, &obj);
		if (ret)
			return ret;

		ptr += sizeof(obj);
	}

	return 0;
}

int kgsl_drawobj_add_memlist(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr,
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
				drawobj->context->id, obj.flags, obj.id,
				obj.offset, obj.gpuaddr, obj.size);
			return -EINVAL;
		}

		if (obj.flags & KGSL_OBJLIST_PROFILE)
			add_profiling_buffer(device, drawobj, obj.gpuaddr,
				obj.size, obj.id, obj.offset);
		else {
			ret = kgsl_drawobj_add_object(&drawobj->memlist,
				&obj);
			if (ret)
				return ret;
		}

		ptr += sizeof(obj);
	}

	return 0;
}

int kgsl_drawobj_add_synclist(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr,
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

	if (count > KGSL_MAX_SYNCPOINTS)
		return -EINVAL;

	drawobj->synclist = kcalloc(count,
		sizeof(struct kgsl_drawobj_sync_event), GFP_KERNEL);

	if (drawobj->synclist == NULL)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		memset(&syncpoint, 0, sizeof(syncpoint));

		ret = _copy_from_user(&syncpoint, ptr, sizeof(syncpoint), size);
		if (ret)
			return ret;

		sync.type = syncpoint.type;
		sync.priv = to_user_ptr(syncpoint.priv);
		sync.size = syncpoint.size;

		ret = kgsl_drawobj_add_sync(device, drawobj, &sync);
		if (ret)
			return ret;

		ptr += sizeof(syncpoint);
	}

	return 0;
}

void kgsl_drawobj_exit(void)
{
	if (memobjs_cache != NULL)
		kmem_cache_destroy(memobjs_cache);
}

int kgsl_drawobj_init(void)
{
	memobjs_cache = KMEM_CACHE(kgsl_memobj_node, 0);
	if (memobjs_cache == NULL) {
		KGSL_CORE_ERR("failed to create memobjs_cache");
		return -ENOMEM;
	}

	return 0;
}

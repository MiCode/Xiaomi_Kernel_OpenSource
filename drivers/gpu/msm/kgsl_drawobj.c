/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
 * Define an kmem cache for the memobj & sparseobj structures since we
 * allocate and free them so frequently
 */
static struct kmem_cache *memobjs_cache;
static struct kmem_cache *sparseobjs_cache;


void kgsl_drawobj_destroy_object(struct kref *kref)
{
	struct kgsl_drawobj *drawobj = container_of(kref,
		struct kgsl_drawobj, refcount);
	struct kgsl_drawobj_sync *syncobj;

	kgsl_context_put(drawobj->context);

	switch (drawobj->type) {
	case SYNCOBJ_TYPE:
		syncobj = SYNCOBJ(drawobj);
		kfree(syncobj->synclist);
		kfree(syncobj);
		break;
	case CMDOBJ_TYPE:
	case MARKEROBJ_TYPE:
		kfree(CMDOBJ(drawobj));
		break;
	case SPARSEOBJ_TYPE:
		kfree(SPARSEOBJ(drawobj));
		break;
	}
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
				"  [timestamp] context %d timestamp %d (retired %d)\n",
				event->context->id, event->timestamp,
				retired);
			break;
		}
		case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
			dev_err(device->dev, "  fence: %s\n",
					event->fence_name);
			break;
		}
	}
}

static void syncobj_timer(unsigned long data)
{
	struct kgsl_device *device;
	struct kgsl_drawobj_sync *syncobj = (struct kgsl_drawobj_sync *) data;
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
		"kgsl: possible gpu syncpoint deadlock for context %d timestamp %d\n",
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
			dev_err(device->dev, "       [%d] TIMESTAMP %d:%d\n",
				i, event->context->id, event->timestamp);
			break;
		case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
			dev_err(device->dev, "       [%d] FENCE %s\n",
					i, event->fence_name);
			break;
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
		del_timer_sync(&syncobj->timer);

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

	drawobj_sync_expire(device, event);
	kgsl_context_put(event->context);
	kgsl_drawobj_put(&event->syncobj->base);
}

static inline void memobj_list_free(struct list_head *list)
{
	struct kgsl_memobj_node *mem, *tmpmem;

	/* Free the cmd mem here */
	list_for_each_entry_safe(mem, tmpmem, list, node) {
		list_del_init(&mem->node);
		kmem_cache_free(memobjs_cache, mem);
	}
}

static void drawobj_destroy_sparse(struct kgsl_drawobj *drawobj)
{
	struct kgsl_sparseobj_node *mem, *tmpmem;
	struct list_head *list = &SPARSEOBJ(drawobj)->sparselist;

	/* Free the sparse mem here */
	list_for_each_entry_safe(mem, tmpmem, list, node) {
		list_del_init(&mem->node);
		kmem_cache_free(sparseobjs_cache, mem);
	}
}

static void drawobj_destroy_sync(struct kgsl_drawobj *drawobj)
{
	struct kgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);
	unsigned long pending = 0;
	unsigned int i;

	/* Zap the canary timer */
	del_timer_sync(&syncobj->timer);

	/*
	 * Copy off the pending list and clear each pending event atomically -
	 * this will render any subsequent asynchronous callback harmless.
	 * This marks each event for deletion. If any pending fence callbacks
	 * run between now and the actual cancel, the associated structures
	 * are kfreed only in the cancel call.
	 */
	for_each_set_bit(i, &syncobj->pending, KGSL_MAX_SYNCPOINTS) {
		if (test_and_clear_bit(i, &syncobj->pending))
			__set_bit(i, &pending);
	}

	/*
	 * Clear all pending events - this will render any subsequent async
	 * callbacks harmless
	 */
	for (i = 0; i < syncobj->numsyncs; i++) {
		struct kgsl_drawobj_sync_event *event = &syncobj->synclist[i];

		/* Don't do anything if the event has already expired */
		if (!test_bit(i, &pending))
			continue;

		switch (event->type) {
		case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
			kgsl_cancel_event(drawobj->device,
				&event->context->events, event->timestamp,
				drawobj_sync_func, event);
			break;
		case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
			kgsl_sync_fence_async_cancel(event->handle);
			kgsl_drawobj_put(drawobj);
			break;
		}
	}

	/*
	 * If we cancelled an event, there's a good chance that the context is
	 * on a dispatcher queue, so schedule to get it removed.
	 */
	if (!bitmap_empty(&pending, KGSL_MAX_SYNCPOINTS) &&
		drawobj->device->ftbl->drawctxt_sched)
		drawobj->device->ftbl->drawctxt_sched(drawobj->device,
							drawobj->context);

}

static void drawobj_destroy_cmd(struct kgsl_drawobj *drawobj)
{
	struct kgsl_drawobj_cmd *cmdobj = CMDOBJ(drawobj);

	/*
	 * Release the refcount on the mem entry associated with the
	 * ib profiling buffer
	 */
	if (cmdobj->base.flags & KGSL_DRAWOBJ_PROFILING)
		kgsl_mem_entry_put(cmdobj->profiling_buf_entry);

	/* Destroy the cmdlist we created */
	memobj_list_free(&cmdobj->cmdlist);

	/* Destroy the memlist we created */
	memobj_list_free(&cmdobj->memlist);
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
	if (!drawobj)
		return;

	if (drawobj->type & SYNCOBJ_TYPE)
		drawobj_destroy_sync(drawobj);
	else if (drawobj->type & (CMDOBJ_TYPE | MARKEROBJ_TYPE))
		drawobj_destroy_cmd(drawobj);
	else if (drawobj->type == SPARSEOBJ_TYPE)
		drawobj_destroy_sparse(drawobj);
	else
		return;

	kgsl_drawobj_put(drawobj);
}
EXPORT_SYMBOL(kgsl_drawobj_destroy);

static bool drawobj_sync_fence_func(void *priv)
{
	struct kgsl_drawobj_sync_event *event = priv;

	trace_syncpoint_fence_expire(event->syncobj, event->fence_name);

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

/* drawobj_add_sync_fence() - Add a new sync fence syncpoint
 * @device: KGSL device
 * @syncobj: KGSL sync obj to add the sync point to
 * @priv: Private structure passed by the user
 *
 * Add a new fence sync syncpoint to the sync obj.
 */
static int drawobj_add_sync_fence(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj, void *priv)
{
	struct kgsl_cmd_syncpoint_fence *sync = priv;
	struct kgsl_drawobj *drawobj = DRAWOBJ(syncobj);
	struct kgsl_drawobj_sync_event *event;
	unsigned int id;

	kref_get(&drawobj->refcount);

	id = syncobj->numsyncs++;

	event = &syncobj->synclist[id];

	event->id = id;
	event->type = KGSL_CMD_SYNCPOINT_TYPE_FENCE;
	event->syncobj = syncobj;
	event->device = device;
	event->context = NULL;

	set_bit(event->id, &syncobj->pending);

	event->handle = kgsl_sync_fence_async_wait(sync->fd,
				drawobj_sync_fence_func, event,
				event->fence_name, sizeof(event->fence_name));

	if (IS_ERR_OR_NULL(event->handle)) {
		int ret = PTR_ERR(event->handle);

		clear_bit(event->id, &syncobj->pending);
		event->handle = NULL;

		kgsl_drawobj_put(drawobj);

		/*
		 * If ret == 0 the fence was already signaled - print a trace
		 * message so we can track that
		 */
		if (ret == 0)
			trace_syncpoint_fence_expire(syncobj, "signaled");

		return ret;
	}

	trace_syncpoint_fence(syncobj, event->fence_name);

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
		struct kgsl_drawobj_sync *syncobj, void *priv)
{
	struct kgsl_cmd_syncpoint_timestamp *sync = priv;
	struct kgsl_drawobj *drawobj = DRAWOBJ(syncobj);
	struct kgsl_context *context = kgsl_context_get(device,
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

	id = syncobj->numsyncs++;

	event = &syncobj->synclist[id];
	event->id = id;

	event->type = KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP;
	event->syncobj = syncobj;
	event->context = context;
	event->timestamp = sync->timestamp;
	event->device = device;

	set_bit(event->id, &syncobj->pending);

	ret = kgsl_add_event(device, &context->events, sync->timestamp,
		drawobj_sync_func, event);

	if (ret) {
		clear_bit(event->id, &syncobj->pending);
		kgsl_drawobj_put(drawobj);
	} else {
		trace_syncpoint_timestamp(syncobj, context, sync->timestamp);
	}

done:
	if (ret)
		kgsl_context_put(context);

	return ret;
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
	void *priv;
	int ret, psize;
	struct kgsl_drawobj *drawobj = DRAWOBJ(syncobj);
	int (*func)(struct kgsl_device *device,
			struct kgsl_drawobj_sync *syncobj,
			void *priv);

	switch (sync->type) {
	case KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
		psize = sizeof(struct kgsl_cmd_syncpoint_timestamp);
		func = drawobj_add_sync_timestamp;
		break;
	case KGSL_CMD_SYNCPOINT_TYPE_FENCE:
		psize = sizeof(struct kgsl_cmd_syncpoint_fence);
		func = drawobj_add_sync_fence;
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

	ret = func(device, syncobj, priv);
	kfree(priv);

	return ret;
}

static void add_profiling_buffer(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj,
		uint64_t gpuaddr, uint64_t size,
		unsigned int id, uint64_t offset)
{
	struct kgsl_mem_entry *entry;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

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

	cmdobj->profiling_buf_entry = entry;

	if (id != 0)
		cmdobj->profiling_buffer_gpuaddr =
			entry->memdesc.gpuaddr + offset;
	else
		cmdobj->profiling_buffer_gpuaddr = gpuaddr;
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

static void *_drawobj_create(struct kgsl_device *device,
	struct kgsl_context *context, unsigned int size,
	unsigned int type)
{
	void *obj = kzalloc(size, GFP_KERNEL);
	struct kgsl_drawobj *drawobj;

	if (obj == NULL)
		return ERR_PTR(-ENOMEM);

	/*
	 * Increase the reference count on the context so it doesn't disappear
	 * during the lifetime of this object
	 */
	if (!_kgsl_context_get(context)) {
		kfree(obj);
		return ERR_PTR(-ENOENT);
	}

	drawobj = obj;

	kref_init(&drawobj->refcount);

	drawobj->device = device;
	drawobj->context = context;
	drawobj->type = type;

	return obj;
}

/**
 * kgsl_drawobj_sparse_create() - Create a new sparse obj structure
 * @device: Pointer to a KGSL device struct
 * @context: Pointer to a KGSL context struct
 * @flags: Flags for the sparse obj
 *
 * Allocate an new kgsl_drawobj_sparse structure
 */
struct kgsl_drawobj_sparse *kgsl_drawobj_sparse_create(
		struct kgsl_device *device,
		struct kgsl_context *context, unsigned int flags)
{
	struct kgsl_drawobj_sparse *sparseobj = _drawobj_create(device,
		context, sizeof(*sparseobj), SPARSEOBJ_TYPE);

	if (!IS_ERR(sparseobj))
		INIT_LIST_HEAD(&sparseobj->sparselist);

	return sparseobj;
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
	struct kgsl_drawobj_sync *syncobj = _drawobj_create(device,
		context, sizeof(*syncobj), SYNCOBJ_TYPE);

	/* Add a timer to help debug sync deadlocks */
	if (!IS_ERR(syncobj))
		setup_timer(&syncobj->timer, syncobj_timer,
				(unsigned long) syncobj);

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
	struct kgsl_drawobj_cmd *cmdobj = _drawobj_create(device,
		context, sizeof(*cmdobj),
		(type & (CMDOBJ_TYPE | MARKEROBJ_TYPE)));

	if (!IS_ERR(cmdobj)) {
		/* sanitize our flags for drawobj's */
		cmdobj->base.flags = flags & (KGSL_DRAWOBJ_CTX_SWITCH
				| KGSL_DRAWOBJ_MARKER
				| KGSL_DRAWOBJ_END_OF_FRAME
				| KGSL_DRAWOBJ_PWR_CONSTRAINT
				| KGSL_DRAWOBJ_MEMLIST
				| KGSL_DRAWOBJ_PROFILING
				| KGSL_DRAWOBJ_PROFILING_KTIME);

		INIT_LIST_HEAD(&cmdobj->cmdlist);
		INIT_LIST_HEAD(&cmdobj->memlist);
	}

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

static int kgsl_drawobj_add_sparseobject(struct list_head *head,
		struct kgsl_sparse_binding_object *obj, unsigned int virt_id)
{
	struct kgsl_sparseobj_node *mem;

	mem = kmem_cache_alloc(sparseobjs_cache, GFP_KERNEL);
	if (mem == NULL)
		return -ENOMEM;

	mem->virt_id = virt_id;
	mem->obj.id = obj->id;
	mem->obj.virtoffset = obj->virtoffset;
	mem->obj.physoffset = obj->physoffset;
	mem->obj.size = obj->size;
	mem->obj.flags = obj->flags;

	list_add_tail(&mem->node, head);
	return 0;
}

int kgsl_drawobj_sparse_add_sparselist(struct kgsl_device *device,
		struct kgsl_drawobj_sparse *sparseobj, unsigned int id,
		void __user *ptr, unsigned int size, unsigned int count)
{
	struct kgsl_sparse_binding_object obj;
	int i, ret = 0;

	ret = _verify_input_list(count, ptr, size);
	if (ret <= 0)
		return ret;

	for (i = 0; i < count; i++) {
		memset(&obj, 0, sizeof(obj));

		ret = _copy_from_user(&obj, ptr, sizeof(obj), size);
		if (ret)
			return ret;

		if (!(obj.flags & (KGSL_SPARSE_BIND | KGSL_SPARSE_UNBIND)))
			return -EINVAL;

		ret = kgsl_drawobj_add_sparseobject(&sparseobj->sparselist,
			&obj, id);
		if (ret)
			return ret;

		ptr += sizeof(obj);
	}

	sparseobj->size = size;
	sparseobj->count = count;

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
		memset(&obj, 0, sizeof(obj));

		ret = _copy_from_user(&obj, ptr, sizeof(obj), size);
		if (ret)
			return ret;

		/* Sanity check the flags */
		if (!(obj.flags & CMDLIST_FLAGS)) {
			KGSL_DRV_ERR(device,
				"invalid cmdobj ctxt %d flags %d id %d offset %lld addr %lld size %lld\n",
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
		memset(&obj, 0, sizeof(obj));

		ret = _copy_from_user(&obj, ptr, sizeof(obj), size);
		if (ret)
			return ret;

		if (!(obj.flags & KGSL_OBJLIST_MEMOBJ)) {
			KGSL_DRV_ERR(device,
				"invalid memobj ctxt %d flags %d id %d offset %lld addr %lld size %lld\n",
				DRAWOBJ(cmdobj)->context->id, obj.flags,
				obj.id, obj.offset, obj.gpuaddr, obj.size);
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
		memset(&syncpoint, 0, sizeof(syncpoint));

		ret = _copy_from_user(&syncpoint, ptr, sizeof(syncpoint), size);
		if (ret)
			return ret;

		sync.type = syncpoint.type;
		sync.priv = to_user_ptr(syncpoint.priv);
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
	kmem_cache_destroy(sparseobjs_cache);
}

int kgsl_drawobjs_cache_init(void)
{
	memobjs_cache = KMEM_CACHE(kgsl_memobj_node, 0);
	sparseobjs_cache = KMEM_CACHE(kgsl_sparseobj_node, 0);

	if (!memobjs_cache || !sparseobjs_cache)
		return -ENOMEM;

	return 0;
}

/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asm/current.h>

#include "kgsl_sync.h"

static void kgsl_sync_timeline_signal(struct kgsl_sync_timeline *timeline,
	unsigned int timestamp);

static const struct fence_ops kgsl_sync_fence_ops;

static struct kgsl_sync_fence *kgsl_sync_fence_create(
					struct kgsl_context *context,
					unsigned int timestamp)
{
	struct kgsl_sync_fence *kfence;
	struct kgsl_sync_timeline *ktimeline = context->ktimeline;
	unsigned long flags;

	/* Get a refcount to the timeline. Put when released */
	if (!kref_get_unless_zero(&ktimeline->kref))
		return NULL;

	kfence = kzalloc(sizeof(*kfence), GFP_KERNEL);
	if (kfence == NULL) {
		kgsl_sync_timeline_put(ktimeline);
		KGSL_DRV_ERR(context->device, "Couldn't allocate fence\n");
		return NULL;
	}

	kfence->parent = ktimeline;
	kfence->context_id = context->id;
	kfence->timestamp = timestamp;

	fence_init(&kfence->fence, &kgsl_sync_fence_ops, &ktimeline->lock,
		ktimeline->fence_context, timestamp);

	/*
	 * sync_file_create() takes a refcount to the fence. This refcount is
	 * put when the fence is signaled.
	 */
	kfence->sync_file = sync_file_create(&kfence->fence);

	if (kfence->sync_file == NULL) {
		kgsl_sync_timeline_put(ktimeline);
		KGSL_DRV_ERR(context->device, "Create sync_file failed\n");
		kfree(kfence);
		return NULL;
	}

	spin_lock_irqsave(&ktimeline->lock, flags);
	list_add_tail(&kfence->child_list, &ktimeline->child_list_head);
	spin_unlock_irqrestore(&ktimeline->lock, flags);

	return kfence;
}

static void kgsl_sync_fence_release(struct fence *fence)
{
	struct kgsl_sync_fence *kfence = (struct kgsl_sync_fence *)fence;

	kgsl_sync_timeline_put(kfence->parent);
	kfree(kfence);
}

/* Called with ktimeline->lock held */
bool kgsl_sync_fence_has_signaled(struct fence *fence)
{
	struct kgsl_sync_fence *kfence = (struct kgsl_sync_fence *)fence;
	struct kgsl_sync_timeline *ktimeline = kfence->parent;
	unsigned int ts = kfence->timestamp;

	return (timestamp_cmp(ktimeline->last_timestamp, ts) >= 0);
}

bool kgsl_enable_signaling(struct fence *fence)
{
	return !kgsl_sync_fence_has_signaled(fence);
}

struct kgsl_sync_fence_event_priv {
	struct kgsl_context *context;
	unsigned int timestamp;
};

/**
 * kgsl_sync_fence_event_cb - Event callback for a fence timestamp event
 * @device - The KGSL device that expired the timestamp
 * @context- Pointer to the context that owns the event
 * @priv: Private data for the callback
 * @result - Result of the event (retired or canceled)
 *
 * Signal a fence following the expiration of a timestamp
 */

static void kgsl_sync_fence_event_cb(struct kgsl_device *device,
		struct kgsl_event_group *group, void *priv, int result)
{
	struct kgsl_sync_fence_event_priv *ev = priv;

	kgsl_sync_timeline_signal(ev->context->ktimeline, ev->timestamp);
	kgsl_context_put(ev->context);
	kfree(ev);
}

static int _add_fence_event(struct kgsl_device *device,
	struct kgsl_context *context, unsigned int timestamp)
{
	struct kgsl_sync_fence_event_priv *event;
	int ret;

	event = kmalloc(sizeof(*event), GFP_KERNEL);
	if (event == NULL)
		return -ENOMEM;

	/*
	 * Increase the refcount for the context to keep it through the
	 * callback
	 */
	if (!_kgsl_context_get(context)) {
		kfree(event);
		return -ENOENT;
	}

	event->context = context;
	event->timestamp = timestamp;

	ret = kgsl_add_event(device, &context->events, timestamp,
		kgsl_sync_fence_event_cb, event);

	if (ret) {
		kgsl_context_put(context);
		kfree(event);
	}

	return ret;
}

/* Only to be used if creating a related event failed */
static void kgsl_sync_cancel(struct kgsl_sync_fence *kfence)
{
	spin_lock(&kfence->parent->lock);
	if (!list_empty(&kfence->child_list)) {
		list_del_init(&kfence->child_list);
		fence_put(&kfence->fence);
	}
	spin_unlock(&kfence->parent->lock);
}

/**
 * kgsl_add_fence_event - Create a new fence event
 * @device - KGSL device to create the event on
 * @timestamp - Timestamp to trigger the event
 * @data - Return fence fd stored in struct kgsl_timestamp_event_fence
 * @len - length of the fence event
 * @owner - driver instance that owns this event
 * @returns 0 on success or error code on error
 *
 * Create a fence and register an event to signal the fence when
 * the timestamp expires
 */

int kgsl_add_fence_event(struct kgsl_device *device,
	u32 context_id, u32 timestamp, void __user *data, int len,
	struct kgsl_device_private *owner)
{
	struct kgsl_timestamp_event_fence priv;
	struct kgsl_context *context;
	struct kgsl_sync_fence *kfence = NULL;
	int ret = -EINVAL;
	unsigned int cur;

	priv.fence_fd = -1;

	if (len != sizeof(priv))
		return -EINVAL;

	context = kgsl_context_get_owner(owner, context_id);

	if (context == NULL)
		return -EINVAL;

	if (test_bit(KGSL_CONTEXT_PRIV_INVALID, &context->priv))
		goto out;

	kfence = kgsl_sync_fence_create(context, timestamp);
	if (kfence == NULL) {
		KGSL_DRV_CRIT_RATELIMIT(device,
					"kgsl_sync_fence_create failed\n");
		ret = -ENOMEM;
		goto out;
	}

	priv.fence_fd = get_unused_fd_flags(0);
	if (priv.fence_fd < 0) {
		KGSL_DRV_CRIT_RATELIMIT(device,
			"Unable to get a file descriptor: %d\n",
			priv.fence_fd);
		ret = priv.fence_fd;
		goto out;
	}

	/*
	 * If the timestamp hasn't expired yet create an event to trigger it.
	 * Otherwise, just signal the fence - there is no reason to go through
	 * the effort of creating a fence we don't need.
	 */

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED, &cur);

	if (timestamp_cmp(cur, timestamp) >= 0) {
		ret = 0;
		kgsl_sync_timeline_signal(context->ktimeline, cur);
	} else {
		ret = _add_fence_event(device, context, timestamp);
		if (ret)
			goto out;
	}

	if (copy_to_user(data, &priv, sizeof(priv))) {
		ret = -EFAULT;
		goto out;
	}
	fd_install(priv.fence_fd, kfence->sync_file->file);

out:
	kgsl_context_put(context);
	if (ret) {
		if (priv.fence_fd >= 0)
			put_unused_fd(priv.fence_fd);

		if (kfence) {
			kgsl_sync_cancel(kfence);
			/*
			 * Put the refcount of sync file. This will release
			 * kfence->fence as well.
			 */
			fput(kfence->sync_file->file);
		}
	}
	return ret;
}

static unsigned int kgsl_sync_fence_get_timestamp(
					struct kgsl_sync_timeline *ktimeline,
					enum kgsl_timestamp_type type)
{
	unsigned int ret = 0;

	if (ktimeline->device == NULL)
		return 0;

	kgsl_readtimestamp(ktimeline->device, ktimeline->context, type, &ret);

	return ret;
}

static void kgsl_sync_timeline_value_str(struct fence *fence,
					char *str, int size)
{
	struct kgsl_sync_fence *kfence = (struct kgsl_sync_fence *)fence;
	struct kgsl_sync_timeline *ktimeline = kfence->parent;

	unsigned int timestamp_retired;
	unsigned int timestamp_queued;

	if (!kref_get_unless_zero(&ktimeline->kref))
		return;

	/*
	 * This callback can be called before the device and spinlock are
	 * initialized in struct kgsl_sync_timeline. kgsl_sync_get_timestamp()
	 * will check if device is NULL and return 0. Queued and retired
	 * timestamp of the context will be reported as 0, which is correct
	 * because the context and timeline are just getting initialized.
	 */
	timestamp_retired = kgsl_sync_fence_get_timestamp(ktimeline,
					KGSL_TIMESTAMP_RETIRED);
	timestamp_queued = kgsl_sync_fence_get_timestamp(ktimeline,
					KGSL_TIMESTAMP_QUEUED);

	snprintf(str, size, "%u queued:%u retired:%u",
		ktimeline->last_timestamp,
		timestamp_queued, timestamp_retired);

	kgsl_sync_timeline_put(ktimeline);
}

static void kgsl_sync_fence_value_str(struct fence *fence, char *str, int size)
{
	struct kgsl_sync_fence *kfence = (struct kgsl_sync_fence *)fence;

	snprintf(str, size, "%u", kfence->timestamp);
}

static const char *kgsl_sync_fence_driver_name(struct fence *fence)
{
	return "kgsl-timeline";
}

static const char *kgsl_sync_timeline_name(struct fence *fence)
{
	struct kgsl_sync_fence *kfence = (struct kgsl_sync_fence *)fence;
	struct kgsl_sync_timeline *ktimeline = kfence->parent;

	return ktimeline->name;
}

int kgsl_sync_timeline_create(struct kgsl_context *context)
{
	struct kgsl_sync_timeline *ktimeline;

	/*
	 * Generate a name which includes the thread name, thread id, process
	 * name, process id, and context id. This makes it possible to
	 * identify the context of a timeline in the sync dump.
	 */
	char ktimeline_name[sizeof(ktimeline->name)] = {};

	/* Put context when timeline is released */
	if (!_kgsl_context_get(context))
		return -ENOENT;

	snprintf(ktimeline_name, sizeof(ktimeline_name),
		"%s_%d-%.15s(%d)-%.15s(%d)",
		context->device->name, context->id,
		current->group_leader->comm, current->group_leader->pid,
		current->comm, current->pid);

	ktimeline = kzalloc(sizeof(*ktimeline), GFP_KERNEL);
	if (ktimeline == NULL) {
		kgsl_context_put(context);
		return -ENOMEM;
	}

	kref_init(&ktimeline->kref);
	strlcpy(ktimeline->name, ktimeline_name, KGSL_TIMELINE_NAME_LEN);
	ktimeline->fence_context = fence_context_alloc(1);
	ktimeline->last_timestamp = 0;
	INIT_LIST_HEAD(&ktimeline->child_list_head);
	spin_lock_init(&ktimeline->lock);
	ktimeline->device = context->device;
	ktimeline->context = context;

	context->ktimeline = ktimeline;

	return 0;
}

static void kgsl_sync_timeline_signal(struct kgsl_sync_timeline *ktimeline,
					unsigned int timestamp)
{
	unsigned long flags;
	struct kgsl_sync_fence *kfence, *next;

	if (!kref_get_unless_zero(&ktimeline->kref))
		return;

	spin_lock_irqsave(&ktimeline->lock, flags);
	if (timestamp_cmp(timestamp, ktimeline->last_timestamp) > 0)
		ktimeline->last_timestamp = timestamp;

	list_for_each_entry_safe(kfence, next, &ktimeline->child_list_head,
				child_list) {
		if (fence_is_signaled_locked(&kfence->fence)) {
			list_del_init(&kfence->child_list);
			fence_put(&kfence->fence);
		}
	}

	spin_unlock_irqrestore(&ktimeline->lock, flags);
	kgsl_sync_timeline_put(ktimeline);
}

void kgsl_sync_timeline_destroy(struct kgsl_context *context)
{
	kfree(context->ktimeline);
}

static void kgsl_sync_timeline_release(struct kref *kref)
{
	struct kgsl_sync_timeline *ktimeline =
		container_of(kref, struct kgsl_sync_timeline, kref);

	/*
	 * Only put the context refcount here. The context destroy function
	 * will call kgsl_sync_timeline_destroy() to kfree it
	 */
	kgsl_context_put(ktimeline->context);
}

void kgsl_sync_timeline_put(struct kgsl_sync_timeline *ktimeline)
{
	if (ktimeline)
		kref_put(&ktimeline->kref, kgsl_sync_timeline_release);
}

static const struct fence_ops kgsl_sync_fence_ops = {
	.get_driver_name = kgsl_sync_fence_driver_name,
	.get_timeline_name = kgsl_sync_timeline_name,
	.enable_signaling = kgsl_enable_signaling,
	.signaled = kgsl_sync_fence_has_signaled,
	.wait = fence_default_wait,
	.release = kgsl_sync_fence_release,

	.fence_value_str = kgsl_sync_fence_value_str,
	.timeline_value_str = kgsl_sync_timeline_value_str,
};

static void kgsl_sync_fence_callback(struct fence *fence, struct fence_cb *cb)
{
	struct kgsl_sync_fence_cb *kcb = (struct kgsl_sync_fence_cb *)cb;

	/*
	 * If the callback is marked for cancellation in a separate thread,
	 * let the other thread do the cleanup.
	 */
	if (kcb->func(kcb->priv)) {
		fence_put(kcb->fence);
		kfree(kcb);
	}
}

static void kgsl_get_fence_name(struct fence *fence,
	char *fence_name, int name_len)
{
	char *ptr = fence_name;
	char *last = fence_name + name_len;

	ptr +=  snprintf(ptr, last - ptr, "%s %s",
			fence->ops->get_driver_name(fence),
			fence->ops->get_timeline_name(fence));

	if ((ptr + 2) >= last)
		return;

	if (fence->ops->fence_value_str) {
		ptr += snprintf(ptr, last - ptr, ": ");
		fence->ops->fence_value_str(fence, ptr, last - ptr);
	}
}

struct kgsl_sync_fence_cb *kgsl_sync_fence_async_wait(int fd,
	bool (*func)(void *priv), void *priv, char *fence_name, int name_len)
{
	struct kgsl_sync_fence_cb *kcb;
	struct fence *fence;
	int status;

	fence = sync_file_get_fence(fd);
	if (fence == NULL)
		return ERR_PTR(-EINVAL);

	/* create the callback */
	kcb = kzalloc(sizeof(*kcb), GFP_ATOMIC);
	if (kcb == NULL) {
		fence_put(fence);
		return ERR_PTR(-ENOMEM);
	}

	kcb->fence = fence;
	kcb->priv = priv;
	kcb->func = func;

	if (fence_name)
		kgsl_get_fence_name(fence, fence_name, name_len);

	/* if status then error or signaled */
	status = fence_add_callback(fence, &kcb->fence_cb,
				kgsl_sync_fence_callback);

	if (status) {
		kfree(kcb);
		if (!fence_is_signaled(fence))
			kcb = ERR_PTR(status);
		else
			kcb = NULL;
		fence_put(fence);
	}

	return kcb;
}

/*
 * Cancel the fence async callback and do the cleanup. The caller must make
 * sure that the callback (if run before cancelling) returns false, so that
 * no other thread frees the pointer.
 */
void kgsl_sync_fence_async_cancel(struct kgsl_sync_fence_cb *kcb)
{
	if (kcb == NULL)
		return;

	/*
	 * After fence_remove_callback() returns, the fence callback is
	 * either not called at all, or completed without freeing kcb.
	 * This thread can then put the fence refcount and free kcb.
	 */
	fence_remove_callback(kcb->fence, &kcb->fence_cb);
	fence_put(kcb->fence);
	kfree(kcb);
}

struct kgsl_syncsource {
	struct kref refcount;
	char name[KGSL_TIMELINE_NAME_LEN];
	int id;
	struct kgsl_process_private *private;
	struct list_head child_list_head;
	spinlock_t lock;
};

struct kgsl_syncsource_fence {
	struct fence fence;
	struct kgsl_syncsource *parent;
	struct list_head child_list;
};

static const struct fence_ops kgsl_syncsource_fence_ops;

long kgsl_ioctl_syncsource_create(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_syncsource *syncsource = NULL;
	struct kgsl_syncsource_create *param = data;
	int ret = -EINVAL;
	int id = 0;
	struct kgsl_process_private *private = dev_priv->process_priv;
	char name[KGSL_TIMELINE_NAME_LEN];

	if (!kgsl_process_private_get(private))
		return ret;

	syncsource = kzalloc(sizeof(*syncsource), GFP_KERNEL);
	if (syncsource == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	snprintf(name, sizeof(name), "kgsl-syncsource-pid-%d",
			current->group_leader->pid);

	kref_init(&syncsource->refcount);
	strlcpy(syncsource->name, name, KGSL_TIMELINE_NAME_LEN);
	syncsource->private = private;
	INIT_LIST_HEAD(&syncsource->child_list_head);
	spin_lock_init(&syncsource->lock);

	idr_preload(GFP_KERNEL);
	spin_lock(&private->syncsource_lock);
	id = idr_alloc(&private->syncsource_idr, syncsource, 1, 0, GFP_NOWAIT);
	if (id > 0) {
		syncsource->id = id;
		param->id = id;
		ret = 0;
	} else {
		ret = id;
	}

	spin_unlock(&private->syncsource_lock);
	idr_preload_end();

out:
	if (ret) {
		kgsl_process_private_put(private);
		kfree(syncsource);
	}

	return ret;
}

static struct kgsl_syncsource *
kgsl_syncsource_get(struct kgsl_process_private *private, int id)
{
	int result = 0;
	struct kgsl_syncsource *syncsource = NULL;

	spin_lock(&private->syncsource_lock);

	syncsource = idr_find(&private->syncsource_idr, id);
	if (syncsource)
		result = kref_get_unless_zero(&syncsource->refcount);

	spin_unlock(&private->syncsource_lock);

	return result ? syncsource : NULL;
}

static void kgsl_syncsource_destroy(struct kref *kref)
{
	struct kgsl_syncsource *syncsource = container_of(kref,
						struct kgsl_syncsource,
						refcount);

	struct kgsl_process_private *private = syncsource->private;

	/* Done with process private. Release the refcount */
	kgsl_process_private_put(private);

	kfree(syncsource);
}

void kgsl_syncsource_put(struct kgsl_syncsource *syncsource)
{
	if (syncsource)
		kref_put(&syncsource->refcount, kgsl_syncsource_destroy);
}

static void kgsl_syncsource_cleanup(struct kgsl_process_private *private,
				struct kgsl_syncsource *syncsource)
{
	struct kgsl_syncsource_fence *sfence, *next;

	/* Signal all fences to release any callbacks */
	spin_lock(&syncsource->lock);

	list_for_each_entry_safe(sfence, next, &syncsource->child_list_head,
				child_list) {
		fence_signal_locked(&sfence->fence);
		list_del_init(&sfence->child_list);
	}

	spin_unlock(&syncsource->lock);

	/* put reference from syncsource creation */
	kgsl_syncsource_put(syncsource);
}

long kgsl_ioctl_syncsource_destroy(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_syncsource_destroy *param = data;
	struct kgsl_syncsource *syncsource = NULL;
	struct kgsl_process_private *private = dev_priv->process_priv;

	spin_lock(&private->syncsource_lock);
	syncsource = idr_find(&private->syncsource_idr, param->id);

	if (syncsource == NULL) {
		spin_unlock(&private->syncsource_lock);
		return -EINVAL;
	}

	if (syncsource->id != 0) {
		idr_remove(&private->syncsource_idr, syncsource->id);
		syncsource->id = 0;
	}
	spin_unlock(&private->syncsource_lock);

	kgsl_syncsource_cleanup(private, syncsource);
	return 0;
}

long kgsl_ioctl_syncsource_create_fence(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_syncsource_create_fence *param = data;
	struct kgsl_syncsource *syncsource = NULL;
	int ret = -EINVAL;
	struct kgsl_syncsource_fence *sfence = NULL;
	struct sync_file *sync_file = NULL;
	int fd = -1;

	/*
	 * Take a refcount that is released when the fence is released
	 * (or if fence can't be added to the syncsource).
	 */
	syncsource = kgsl_syncsource_get(dev_priv->process_priv,
					param->id);
	if (syncsource == NULL)
		goto out;

	sfence = kzalloc(sizeof(*sfence), GFP_KERNEL);
	if (sfence == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	sfence->parent = syncsource;

	/* Use a new fence context for each fence */
	fence_init(&sfence->fence, &kgsl_syncsource_fence_ops,
		&syncsource->lock, fence_context_alloc(1), 1);

	sync_file = sync_file_create(&sfence->fence);

	if (sync_file == NULL) {
		KGSL_DRV_ERR(dev_priv->device, "Create sync_file failed\n");
		ret = -ENOMEM;
		goto out;
	}

	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		ret = -EBADF;
		goto out;
	}
	ret = 0;

	fd_install(fd, sync_file->file);

	param->fence_fd = fd;

	spin_lock(&syncsource->lock);
	list_add_tail(&sfence->child_list, &syncsource->child_list_head);
	spin_unlock(&syncsource->lock);
out:
	/*
	 * We're transferring ownership of the fence to the sync file.
	 * The sync file takes an extra refcount when it is created, so put
	 * our refcount.
	 */
	if (sync_file)
		fence_put(&sfence->fence);

	if (ret) {
		if (sync_file)
			fput(sync_file->file);
		else if (sfence)
			fence_put(&sfence->fence);
		else
			kgsl_syncsource_put(syncsource);
	}

	return ret;
}

static int kgsl_syncsource_signal(struct kgsl_syncsource *syncsource,
					struct fence *fence)
{
	struct kgsl_syncsource_fence *sfence, *next;
	int ret = -EINVAL;

	spin_lock(&syncsource->lock);

	list_for_each_entry_safe(sfence, next, &syncsource->child_list_head,
				child_list) {
		if (fence == &sfence->fence) {
			fence_signal_locked(fence);
			list_del_init(&sfence->child_list);

			ret = 0;
			break;
		}
	}

	spin_unlock(&syncsource->lock);

	return ret;
}

long kgsl_ioctl_syncsource_signal_fence(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int ret = -EINVAL;
	struct kgsl_syncsource_signal_fence *param = data;
	struct kgsl_syncsource *syncsource = NULL;
	struct fence *fence = NULL;

	syncsource = kgsl_syncsource_get(dev_priv->process_priv,
					param->id);
	if (syncsource == NULL)
		goto out;

	fence = sync_file_get_fence(param->fence_fd);
	if (fence == NULL) {
		ret = -EBADF;
		goto out;
	}

	ret = kgsl_syncsource_signal(syncsource, fence);
out:
	if (fence)
		fence_put(fence);
	if (syncsource)
		kgsl_syncsource_put(syncsource);
	return ret;
}

static void kgsl_syncsource_fence_release(struct fence *fence)
{
	struct kgsl_syncsource_fence *sfence =
			(struct kgsl_syncsource_fence *)fence;

	/* Signal if it's not signaled yet */
	kgsl_syncsource_signal(sfence->parent, fence);

	/* Release the refcount on the syncsource */
	kgsl_syncsource_put(sfence->parent);

	kfree(sfence);
}

void kgsl_syncsource_process_release_syncsources(
		struct kgsl_process_private *private)
{
	struct kgsl_syncsource *syncsource;
	int next = 0;

	while (1) {
		spin_lock(&private->syncsource_lock);
		syncsource = idr_get_next(&private->syncsource_idr, &next);

		if (syncsource == NULL) {
			spin_unlock(&private->syncsource_lock);
			break;
		}

		if (syncsource->id != 0) {
			idr_remove(&private->syncsource_idr, syncsource->id);
			syncsource->id = 0;
		}
		spin_unlock(&private->syncsource_lock);

		kgsl_syncsource_cleanup(private, syncsource);
		next = next + 1;
	}
}

static const char *kgsl_syncsource_get_timeline_name(struct fence *fence)
{
	struct kgsl_syncsource_fence *sfence =
			(struct kgsl_syncsource_fence *)fence;
	struct kgsl_syncsource *syncsource = sfence->parent;

	return syncsource->name;
}

static bool kgsl_syncsource_enable_signaling(struct fence *fence)
{
	return true;
}

static const char *kgsl_syncsource_driver_name(struct fence *fence)
{
	return "kgsl-syncsource-timeline";
}

static void kgsl_syncsource_fence_value_str(struct fence *fence,
						char *str, int size)
{
	/*
	 * Each fence is independent of the others on the same timeline.
	 * We use a different context for each of them.
	 */
	snprintf(str, size, "%llu", fence->context);
}

static const struct fence_ops kgsl_syncsource_fence_ops = {
	.get_driver_name = kgsl_syncsource_driver_name,
	.get_timeline_name = kgsl_syncsource_get_timeline_name,
	.enable_signaling = kgsl_syncsource_enable_signaling,
	.wait = fence_default_wait,
	.release = kgsl_syncsource_fence_release,

	.fence_value_str = kgsl_syncsource_fence_value_str,
};


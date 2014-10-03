/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/oneshot_sync.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asm/current.h>

#include "kgsl_sync.h"

static void kgsl_sync_timeline_signal(struct sync_timeline *timeline,
	unsigned int timestamp);

static struct sync_pt *kgsl_sync_pt_create(struct sync_timeline *timeline,
	unsigned int timestamp)
{
	struct sync_pt *pt;
	pt = sync_pt_create(timeline, (int) sizeof(struct kgsl_sync_pt));
	if (pt) {
		struct kgsl_sync_pt *kpt = (struct kgsl_sync_pt *) pt;
		kpt->timestamp = timestamp;
	}
	return pt;
}

/*
 * This should only be called on sync_pts which have been created but
 * not added to a fence.
 */
static void kgsl_sync_pt_destroy(struct sync_pt *pt)
{
	sync_pt_free(pt);
}

static struct sync_pt *kgsl_sync_pt_dup(struct sync_pt *pt)
{
	struct kgsl_sync_pt *kpt = (struct kgsl_sync_pt *) pt;
	return kgsl_sync_pt_create(pt->parent, kpt->timestamp);
}

static int kgsl_sync_pt_has_signaled(struct sync_pt *pt)
{
	struct kgsl_sync_pt *kpt = (struct kgsl_sync_pt *) pt;
	struct kgsl_sync_timeline *ktimeline =
		 (struct kgsl_sync_timeline *) pt->parent;
	unsigned int ts = kpt->timestamp;
	unsigned int last_ts = ktimeline->last_timestamp;
	if (timestamp_cmp(last_ts, ts) >= 0) {
		/* signaled */
		return 1;
	}
	return 0;
}

static int kgsl_sync_pt_compare(struct sync_pt *a, struct sync_pt *b)
{
	struct kgsl_sync_pt *kpt_a = (struct kgsl_sync_pt *) a;
	struct kgsl_sync_pt *kpt_b = (struct kgsl_sync_pt *) b;
	unsigned int ts_a = kpt_a->timestamp;
	unsigned int ts_b = kpt_b->timestamp;
	return timestamp_cmp(ts_a, ts_b);
}

struct kgsl_fence_event_priv {
	struct kgsl_context *context;
	unsigned int timestamp;
};

/**
 * kgsl_fence_event_cb - Event callback for a fence timestamp event
 * @device - The KGSL device that expired the timestamp
 * @context- Pointer to the context that owns the event
 * @priv: Private data for the callback
 * @result - Result of the event (retired or canceled)
 *
 * Signal a fence following the expiration of a timestamp
 */

static void kgsl_fence_event_cb(struct kgsl_device *device,
		struct kgsl_event_group *group, void *priv, int result)
{
	struct kgsl_fence_event_priv *ev = priv;
	kgsl_sync_timeline_signal(ev->context->timeline, ev->timestamp);
	kgsl_context_put(ev->context);
	kfree(ev);
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
	struct kgsl_fence_event_priv *event;
	struct kgsl_timestamp_event_fence priv;
	struct kgsl_context *context;
	struct sync_pt *pt;
	struct sync_fence *fence = NULL;
	int ret = -EINVAL;
	char fence_name[sizeof(fence->name)] = {};

	priv.fence_fd = -1;

	if (len != sizeof(priv))
		return -EINVAL;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (event == NULL)
		return -ENOMEM;

	mutex_lock(&device->mutex);

	context = kgsl_context_get_owner(owner, context_id);

	if (context == NULL)
		goto unlock;

	event->context = context;
	event->timestamp = timestamp;

	pt = kgsl_sync_pt_create(context->timeline, timestamp);
	if (pt == NULL) {
		KGSL_DRV_ERR(device, "kgsl_sync_pt_create failed\n");
		ret = -ENOMEM;
		goto unlock;
	}
	snprintf(fence_name, sizeof(fence_name),
		"%s-pid-%d-ctx-%d-ts-%d",
		device->name, current->group_leader->pid,
		context_id, timestamp);


	fence = sync_fence_create(fence_name, pt);
	if (fence == NULL) {
		/* only destroy pt when not added to fence */
		kgsl_sync_pt_destroy(pt);
		KGSL_DRV_ERR(device, "sync_fence_create failed\n");
		ret = -ENOMEM;
		goto unlock;
	}

	priv.fence_fd = get_unused_fd_flags(0);
	if (priv.fence_fd < 0) {
		KGSL_DRV_ERR(device, "Unable to get a file descriptor: %d\n",
			priv.fence_fd);
		ret = priv.fence_fd;
		goto unlock;
	}
	sync_fence_install(fence, priv.fence_fd);

	/* Unlock the mutex before copying to user */
	mutex_unlock(&device->mutex);

	if (copy_to_user(data, &priv, sizeof(priv))) {
		ret = -EFAULT;
		goto out;
	}

	/*
	 * Hold the context ref-count for the event - it will get released in
	 * the callback
	 */
	ret = kgsl_add_event(device, &context->events, timestamp,
		kgsl_fence_event_cb, event);

	if (ret)
		goto out;

	return 0;

unlock:
	mutex_unlock(&device->mutex);

out:
	if (priv.fence_fd >= 0)
		put_unused_fd(priv.fence_fd);

	if (fence)
		sync_fence_put(fence);

	kgsl_context_put(context);
	kfree(event);
	return ret;
}

static unsigned int kgsl_sync_get_timestamp(
	struct kgsl_sync_timeline *ktimeline, enum kgsl_timestamp_type type)
{
	unsigned int ret = 0;

	struct kgsl_context *context = kgsl_context_get(ktimeline->device,
			ktimeline->context_id);

	if (context)
		kgsl_readtimestamp(ktimeline->device, context, type, &ret);

	kgsl_context_put(context);
	return ret;
}

static void kgsl_sync_timeline_value_str(struct sync_timeline *sync_timeline,
					 char *str, int size)
{
	struct kgsl_sync_timeline *ktimeline =
		(struct kgsl_sync_timeline *) sync_timeline;
	unsigned int timestamp_retired = kgsl_sync_get_timestamp(ktimeline,
		KGSL_TIMESTAMP_RETIRED);
	unsigned int timestamp_queued = kgsl_sync_get_timestamp(ktimeline,
		KGSL_TIMESTAMP_QUEUED);
	snprintf(str, size, "%u queued:%u retired:%u",
		ktimeline->last_timestamp,
		timestamp_queued, timestamp_retired);
}

static void kgsl_sync_pt_value_str(struct sync_pt *sync_pt,
				   char *str, int size)
{
	struct kgsl_sync_pt *kpt = (struct kgsl_sync_pt *) sync_pt;
	snprintf(str, size, "%u", kpt->timestamp);
}

static int kgsl_sync_fill_driver_data(struct sync_pt *sync_pt, void *data,
					int size)
{
	struct kgsl_sync_pt *kpt = (struct kgsl_sync_pt *) sync_pt;

	if (size < sizeof(kpt->timestamp))
		return -ENOMEM;

	memcpy(data, &kpt->timestamp, sizeof(kpt->timestamp));
	return sizeof(kpt->timestamp);
}

static void kgsl_sync_timeline_release_obj(struct sync_timeline *sync_timeline)
{
	/*
	 * Make sure to free the timeline only after destroy flag is set.
	 * This is to avoid further accessing to the timeline from KGSL and
	 * also to catch any unbalanced kref of timeline.
	 */
	BUG_ON(sync_timeline && (sync_timeline->destroyed != true));
}
static const struct sync_timeline_ops kgsl_sync_timeline_ops = {
	.driver_name = "kgsl-timeline",
	.dup = kgsl_sync_pt_dup,
	.has_signaled = kgsl_sync_pt_has_signaled,
	.compare = kgsl_sync_pt_compare,
	.timeline_value_str = kgsl_sync_timeline_value_str,
	.pt_value_str = kgsl_sync_pt_value_str,
	.fill_driver_data = kgsl_sync_fill_driver_data,
	.release_obj = kgsl_sync_timeline_release_obj,
};

int kgsl_sync_timeline_create(struct kgsl_context *context)
{
	struct kgsl_sync_timeline *ktimeline;

	/* Generate a name which includes the thread name, thread id, process
	 * name, process id, and context id. This makes it possible to
	 * identify the context of a timeline in the sync dump. */
	char ktimeline_name[sizeof(context->timeline->name)] = {};
	snprintf(ktimeline_name, sizeof(ktimeline_name),
		"%s_%.15s(%d)-%.15s(%d)-%d",
		context->device->name,
		current->group_leader->comm, current->group_leader->pid,
		current->comm, current->pid, context->id);

	context->timeline = sync_timeline_create(&kgsl_sync_timeline_ops,
		(int) sizeof(struct kgsl_sync_timeline), ktimeline_name);
	if (context->timeline == NULL)
		return -EINVAL;

	ktimeline = (struct kgsl_sync_timeline *) context->timeline;
	ktimeline->last_timestamp = 0;
	ktimeline->device = context->device;
	ktimeline->context_id = context->id;

	return 0;
}

static void kgsl_sync_timeline_signal(struct sync_timeline *timeline,
	unsigned int timestamp)
{
	struct kgsl_sync_timeline *ktimeline =
		(struct kgsl_sync_timeline *) timeline;

	if (timestamp_cmp(timestamp, ktimeline->last_timestamp) > 0)
		ktimeline->last_timestamp = timestamp;
	sync_timeline_signal(timeline);
}

void kgsl_sync_timeline_destroy(struct kgsl_context *context)
{
	sync_timeline_destroy(context->timeline);
}

static void kgsl_sync_callback(struct sync_fence *fence,
	struct sync_fence_waiter *waiter)
{
	struct kgsl_sync_fence_waiter *kwaiter =
		(struct kgsl_sync_fence_waiter *) waiter;
	kwaiter->func(kwaiter->priv);
	sync_fence_put(kwaiter->fence);
	kfree(kwaiter);
}

struct kgsl_sync_fence_waiter *kgsl_sync_fence_async_wait(int fd,
	void (*func)(void *priv), void *priv)
{
	struct kgsl_sync_fence_waiter *kwaiter;
	struct sync_fence *fence;
	int status;

	fence = sync_fence_fdget(fd);
	if (fence == NULL)
		return ERR_PTR(-EINVAL);

	/* create the waiter */
	kwaiter = kzalloc(sizeof(*kwaiter), GFP_ATOMIC);
	if (kwaiter == NULL) {
		sync_fence_put(fence);
		return ERR_PTR(-ENOMEM);
	}
	kwaiter->fence = fence;
	kwaiter->priv = priv;
	kwaiter->func = func;
	sync_fence_waiter_init((struct sync_fence_waiter *) kwaiter,
		kgsl_sync_callback);

	/* if status then error or signaled */
	status = sync_fence_wait_async(fence,
		(struct sync_fence_waiter *) kwaiter);
	if (status) {
		kfree(kwaiter);
		sync_fence_put(fence);
		if (status < 0)
			kwaiter = ERR_PTR(status);
		else
			kwaiter = NULL;
	}

	return kwaiter;
}

int kgsl_sync_fence_async_cancel(struct kgsl_sync_fence_waiter *kwaiter)
{
	if (kwaiter == NULL)
		return 0;

	if (sync_fence_cancel_async(kwaiter->fence,
		(struct sync_fence_waiter *) kwaiter) == 0) {
		sync_fence_put(kwaiter->fence);
		kfree(kwaiter);
		return 1;
	}
	return 0;
}

#ifdef CONFIG_ONESHOT_SYNC

struct kgsl_syncsource {
	struct kref refcount;
	int id;
	struct kgsl_process_private *private;
	struct oneshot_sync_timeline *oneshot;
};

long kgsl_ioctl_syncsource_create(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_syncsource *syncsource = NULL;
	struct kgsl_syncsource_create *param = data;
	int ret = -EINVAL;
	int id = 0;
	struct kgsl_process_private *private = dev_priv->process_priv;
	char name[32];

	syncsource = kzalloc(sizeof(*syncsource), GFP_KERNEL);
	if (syncsource == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	snprintf(name, sizeof(name), "kgsl-syncsource-pid-%d",
			current->group_leader->pid);

	syncsource->oneshot = oneshot_timeline_create(name);
	if (syncsource->oneshot == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	mutex_lock(&private->process_private_mutex);
	id = idr_alloc(&private->syncsource_idr, syncsource, 1, 0, GFP_KERNEL);
	if (id > 0) {
		kref_init(&syncsource->refcount);
		syncsource->id = id;
		syncsource->private = private;

		param->id = id;
		ret = 0;
	} else {
		ret = id;
	}
	mutex_unlock(&private->process_private_mutex);
out:
	if (ret) {
		if (syncsource && syncsource->oneshot)
			oneshot_timeline_destroy(syncsource->oneshot);
		kfree(syncsource);
	}

	return ret;
}

static struct kgsl_syncsource *
kgsl_syncsource_get(struct kgsl_process_private *private, int id)
{
	int result = 0;
	struct kgsl_syncsource *syncsource = NULL;

	mutex_lock(&private->process_private_mutex);

	syncsource = idr_find(&private->syncsource_idr, id);
	if (syncsource)
		result = kref_get_unless_zero(&syncsource->refcount);

	mutex_unlock(&private->process_private_mutex);

	return result ? syncsource : NULL;
}

static void kgsl_syncsource_destroy(struct kref *kref)
{
	struct kgsl_syncsource *syncsource = container_of(kref,
						struct kgsl_syncsource,
						refcount);

	struct kgsl_process_private *private = syncsource->private;

	mutex_lock(&private->process_private_mutex);
	if (syncsource->id != 0) {
		idr_remove(&private->syncsource_idr, syncsource->id);
		syncsource->id = 0;
	}
	oneshot_timeline_destroy(syncsource->oneshot);
	mutex_unlock(&private->process_private_mutex);

	kfree(syncsource);
}

void kgsl_syncsource_put(struct kgsl_syncsource *syncsource)
{
	if (syncsource)
		kref_put(&syncsource->refcount, kgsl_syncsource_destroy);
}

long kgsl_ioctl_syncsource_destroy(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_syncsource_destroy *param = data;
	struct kgsl_syncsource *syncsource = NULL;
	struct kgsl_process_private *private;

	syncsource = kgsl_syncsource_get(dev_priv->process_priv,
				     param->id);

	if (syncsource == NULL)
		return -EINVAL;

	private = syncsource->private;

	mutex_lock(&private->process_private_mutex);
	idr_remove(&private->syncsource_idr, param->id);
	syncsource->id = 0;
	mutex_unlock(&private->process_private_mutex);

	/* put reference from syncsource creation */
	kgsl_syncsource_put(syncsource);
	/* put reference from getting the syncsource above */
	kgsl_syncsource_put(syncsource);
	return 0;
}

long kgsl_ioctl_syncsource_create_fence(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_syncsource_create_fence *param = data;
	struct kgsl_syncsource *syncsource = NULL;
	int ret = -EINVAL;
	struct sync_fence *fence = NULL;
	int fd = -1;
	char name[32];


	syncsource = kgsl_syncsource_get(dev_priv->process_priv,
					param->id);
	if (syncsource == NULL)
		goto out;

	snprintf(name, sizeof(name), "kgsl-syncsource-pid-%d-%d",
			current->group_leader->pid, syncsource->id);

	fence = oneshot_fence_create(syncsource->oneshot, name);
	if (fence == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		ret = -EBADF;
		goto out;
	}
	ret = 0;

	sync_fence_install(fence, fd);

	param->fence_fd = fd;
out:
	if (ret) {
		if (fence)
			sync_fence_put(fence);
	}
	kgsl_syncsource_put(syncsource);
	return ret;
}

long kgsl_ioctl_syncsource_signal_fence(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int ret = -EINVAL;
	struct kgsl_syncsource_signal_fence *param = data;
	struct kgsl_syncsource *syncsource = NULL;
	struct sync_fence *fence = NULL;

	syncsource = kgsl_syncsource_get(dev_priv->process_priv,
					param->id);
	if (syncsource == NULL)
		goto out;

	fence = sync_fence_fdget(param->fence_fd);
	if (fence == NULL) {
		ret = -EBADF;
		goto out;
	}

	ret = oneshot_fence_signal(syncsource->oneshot, fence);
out:
	if (fence)
		sync_fence_put(fence);
	kgsl_syncsource_put(syncsource);
	return ret;
}
#endif

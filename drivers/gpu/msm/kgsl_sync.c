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
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asm/current.h>

#include "kgsl_sync.h"

struct sync_pt *kgsl_sync_pt_create(struct sync_timeline *timeline,
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
void kgsl_sync_pt_destroy(struct sync_pt *pt)
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
 * @priv - private data for the event
 * @context_id - the context id that goes with the timestamp
 * @timestamp - the timestamp that triggered the event
 *
 * Signal a fence following the expiration of a timestamp
 */

static inline void kgsl_fence_event_cb(struct kgsl_device *device,
	void *priv, u32 context_id, u32 timestamp, u32 type)
{
	struct kgsl_fence_event_priv *ev = priv;
	kgsl_sync_timeline_signal(ev->context->timeline, ev->timestamp);
	kgsl_context_put(ev->context);
	kfree(ev);
}

static int _add_fence_event(struct kgsl_device *device,
	struct kgsl_context *context, unsigned int timestamp)
{
	struct kgsl_fence_event_priv *event;
	int ret;

	event = kmalloc(sizeof(*event), GFP_KERNEL);
	if (event == NULL)
		return -ENOMEM;

	/*
	 * Increase the refcount for the context to keep it through the
	 * callback
	 */

	_kgsl_context_get(context);

	event->context = context;
	event->timestamp = timestamp;

	ret = kgsl_add_event(device, context->id, timestamp,
		kgsl_fence_event_cb, event, context->dev_priv);

	if (ret) {
		kgsl_context_put(context);
		kfree(event);
	}

	return ret;
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
	struct sync_pt *pt;
	struct sync_fence *fence = NULL;
	int ret = -EINVAL;
	char fence_name[sizeof(fence->name)] = {};
	unsigned int cur;

	priv.fence_fd = -1;

	if (len != sizeof(priv))
		return -EINVAL;

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);

	context = kgsl_context_get_owner(owner, context_id);

	if (context == NULL)
		goto unlock;

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

	/*
	 * If the timestamp hasn't expired yet create an event to trigger it.
	 * Otherwise, just signal the fence - there is no reason to go through
	 * the effort of creating a fence we don't need.
	 */

	cur = kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED);

	if (timestamp_cmp(cur, timestamp) >= 0)
		kgsl_sync_timeline_signal(context->timeline, cur);
	else {
		ret = _add_fence_event(device, context, timestamp);
		if (ret)
			goto unlock;
	}

	kgsl_context_put(context);

	/* Unlock the mutex before copying to user */
	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

	if (copy_to_user(data, &priv, sizeof(priv))) {
		ret = -EFAULT;
		goto out;
	}

	return 0;

unlock:
	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

out:
	if (priv.fence_fd >= 0)
		put_unused_fd(priv.fence_fd);

	if (fence)
		sync_fence_put(fence);

	kgsl_context_put(context);
	return ret;
}

static unsigned int kgsl_sync_get_timestamp(
	struct kgsl_sync_timeline *ktimeline, enum kgsl_timestamp_type type)
{
	unsigned int ret = 0;

	struct kgsl_context *context = kgsl_context_get(ktimeline->device,
			ktimeline->context_id);

	if (context)
		ret = kgsl_readtimestamp(ktimeline->device, context, type);

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
	snprintf(str, size, "%u retired:%u", ktimeline->last_timestamp,
		timestamp_retired);
}

static void kgsl_sync_pt_value_str(struct sync_pt *sync_pt,
				   char *str, int size)
{
	struct kgsl_sync_pt *kpt = (struct kgsl_sync_pt *) sync_pt;
	snprintf(str, size, "%u", kpt->timestamp);
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

void kgsl_sync_timeline_signal(struct sync_timeline *timeline,
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

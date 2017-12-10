/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
	struct kgsl_context *context, unsigned int timestamp)
{
	struct sync_pt *pt;
	pt = sync_pt_create(timeline, (int) sizeof(struct kgsl_sync_pt));
	if (pt) {
		struct kgsl_sync_pt *kpt = (struct kgsl_sync_pt *) pt;
		kpt->context = context;
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
	return kgsl_sync_pt_create(pt->parent, kpt->context, kpt->timestamp);
}

static int kgsl_sync_pt_has_signaled(struct sync_pt *pt)
{
	struct kgsl_sync_pt *kpt = (struct kgsl_sync_pt *) pt;
	struct kgsl_sync_timeline *ktimeline =
		 (struct kgsl_sync_timeline *) pt->parent;
	unsigned int ts = kpt->timestamp;
	int ret = 0;

	spin_lock(&ktimeline->lock);
	ret = (timestamp_cmp(ktimeline->last_timestamp, ts) >= 0);
	spin_unlock(&ktimeline->lock);

	return ret;
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
	if (!_kgsl_context_get(context)) {
		kfree(event);
		return -ENOENT;
	}

	event->context = context;
	event->timestamp = timestamp;
	event->context = context;

	ret = kgsl_add_event(device, &context->events, timestamp,
		kgsl_fence_event_cb, event);

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

	context = kgsl_context_get_owner(owner, context_id);

	if (context == NULL)
		return -EINVAL;

	if (test_bit(KGSL_CONTEXT_PRIV_INVALID, &context->priv))
		goto out;

	pt = kgsl_sync_pt_create(context->timeline, context, timestamp);
	if (pt == NULL) {
		KGSL_DRV_CRIT_RATELIMIT(device, "kgsl_sync_pt_create failed\n");
		ret = -ENOMEM;
		goto out;
	}
	snprintf(fence_name, sizeof(fence_name),
		"%s-pid-%d-ctx-%d-ts-%u",
		device->name, current->group_leader->pid,
		context_id, timestamp);


	fence = sync_fence_create(fence_name, pt);
	if (fence == NULL) {
		/* only destroy pt when not added to fence */
		kgsl_sync_pt_destroy(pt);
		KGSL_DRV_CRIT_RATELIMIT(device, "sync_fence_create failed\n");
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
		kgsl_sync_timeline_signal(context->timeline, cur);
	} else {
		ret = _add_fence_event(device, context, timestamp);
		if (ret)
			goto out;
	}

	if (copy_to_user(data, &priv, sizeof(priv))) {
		ret = -EFAULT;
		goto out;
	}
	sync_fence_install(fence, priv.fence_fd);
out:
	kgsl_context_put(context);
	if (ret) {
		if (priv.fence_fd >= 0)
			put_unused_fd(priv.fence_fd);

		if (fence)
			sync_fence_put(fence);
	}
	return ret;
}

static unsigned int kgsl_sync_get_timestamp(
	struct kgsl_sync_timeline *ktimeline, enum kgsl_timestamp_type type)
{
	unsigned int ret = 0;
	struct kgsl_context *context;

	if (ktimeline->device == NULL)
		return 0;

	context = kgsl_context_get(ktimeline->device,
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

	/*
	 * This callback can be called before the device and spinlock are
	 * initialized in struct kgsl_sync_timeline. kgsl_sync_get_timestamp()
	 * will check if device is NULL and return 0. Queued and retired
	 * timestamp of the context will be reported as 0, which is correct
	 * because the context and timeline are just getting initialized.
	 */
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

static void kgsl_sync_pt_log(struct sync_pt *sync_pt)
{
	struct kgsl_sync_pt *kpt = (struct kgsl_sync_pt *) sync_pt;
	pr_info("-----\n");
	kgsl_context_dump(kpt->context);
	pr_info("-----\n");
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
	.pt_log = kgsl_sync_pt_log,
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

	spin_lock_init(&ktimeline->lock);
	return 0;
}

static void kgsl_sync_timeline_signal(struct sync_timeline *timeline,
	unsigned int timestamp)
{
	struct kgsl_sync_timeline *ktimeline =
		(struct kgsl_sync_timeline *) timeline;

	spin_lock(&ktimeline->lock);
	if (timestamp_cmp(timestamp, ktimeline->last_timestamp) > 0)
		ktimeline->last_timestamp = timestamp;
	spin_unlock(&ktimeline->lock);

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

	strlcpy(kwaiter->name, fence->name, sizeof(kwaiter->name));

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

	kref_init(&syncsource->refcount);
	syncsource->private = private;

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

	spin_lock(&private->syncsource_lock);
	if (syncsource->id != 0) {
		idr_remove(&private->syncsource_idr, syncsource->id);
		syncsource->id = 0;
	}
	oneshot_timeline_destroy(syncsource->oneshot);
	spin_unlock(&private->syncsource_lock);

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
	struct kgsl_process_private *private = dev_priv->process_priv;

	spin_lock(&private->syncsource_lock);
	syncsource = idr_find(&private->syncsource_idr, param->id);

	if (syncsource) {
		idr_remove(&private->syncsource_idr, param->id);
		syncsource->id = 0;
	}

	spin_unlock(&private->syncsource_lock);

	if (syncsource == NULL)
		return -EINVAL;

	/* put reference from syncsource creation */
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
		if (fd >= 0)
			put_unused_fd(fd);

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

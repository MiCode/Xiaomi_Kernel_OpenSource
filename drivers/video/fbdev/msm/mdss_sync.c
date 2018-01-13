/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/fence.h>
#include <linux/sync_file.h>

#include "mdss_sync.h"

#define MDSS_SYNC_NAME_SIZE		64
#define MDSS_SYNC_DRIVER_NAME	"mdss"

/**
 * struct mdss_fence - sync fence context
 * @base: base sync fence object
 * @name: name of this sync fence
 * @fence_list: linked list of outstanding sync fence
 */
struct mdss_fence {
	struct fence base;
	char name[MDSS_SYNC_NAME_SIZE];
	struct list_head fence_list;
};

/**
 * struct mdss_timeline - sync timeline context
 * @kref: reference count of timeline
 * @lock: serialization lock for timeline and fence update
 * @name: name of timeline
 * @fence_name: fence name prefix
 * @next_value: next commit sequence number
 * @value: current retired sequence number
 * @context: fence context identifier
 * @fence_list_head: linked list of outstanding sync fence
 */
struct mdss_timeline {
	struct kref kref;
	spinlock_t lock;
	char name[MDSS_SYNC_NAME_SIZE];
	u32 next_value;
	u32 value;
	u64 context;
	struct list_head fence_list_head;
};

/*
 * to_mdss_fence - get mdss fence from fence base object
 * @fence: Pointer to fence base object
 */
static struct mdss_fence *to_mdss_fence(struct fence *fence)
{
	return container_of(fence, struct mdss_fence, base);
}

/*
 * to_mdss_timeline - get mdss timeline from fence base object
 * @fence: Pointer to fence base object
 */
static struct mdss_timeline *to_mdss_timeline(struct fence *fence)
{
	return container_of(fence->lock, struct mdss_timeline, lock);
}

/*
 * mdss_free_timeline - Free the given timeline object
 * @kref: Pointer to timeline kref object.
 */
static void mdss_free_timeline(struct kref *kref)
{
	struct mdss_timeline *tl =
		container_of(kref, struct mdss_timeline, kref);

	kfree(tl);
}

/*
 * mdss_put_timeline - Put the given timeline object
 * @tl: Pointer to timeline object.
 */
static void mdss_put_timeline(struct mdss_timeline *tl)
{
	if (!tl) {
		pr_err("invalid parameters\n");
		return;
	}

	kref_put(&tl->kref, mdss_free_timeline);
}

/*
 * mdss_get_timeline - Get the given timeline object
 * @tl: Pointer to timeline object.
 */
static void mdss_get_timeline(struct mdss_timeline *tl)
{
	if (!tl) {
		pr_err("invalid parameters\n");
		return;
	}

	kref_get(&tl->kref);
}

static const char *mdss_fence_get_driver_name(struct fence *fence)
{
	return MDSS_SYNC_DRIVER_NAME;
}

static const char *mdss_fence_get_timeline_name(struct fence *fence)
{
	struct mdss_timeline *tl = to_mdss_timeline(fence);

	return tl->name;
}

static bool mdss_fence_enable_signaling(struct fence *fence)
{
	return true;
}

static bool mdss_fence_signaled(struct fence *fence)
{
	struct mdss_timeline *tl = to_mdss_timeline(fence);
	bool status;

	status = ((s32) (tl->value - fence->seqno)) >= 0;
	pr_debug("status:%d fence seq:%d and timeline:%s:%d next %d\n",
			status, fence->seqno, tl->name,
			tl->value, tl->next_value);
	return status;
}

static void mdss_fence_release(struct fence *fence)
{
	struct mdss_fence *f = to_mdss_fence(fence);
	unsigned long flags;

	spin_lock_irqsave(fence->lock, flags);
	if (!list_empty(&f->fence_list))
		list_del(&f->fence_list);
	spin_unlock_irqrestore(fence->lock, flags);
	mdss_put_timeline(to_mdss_timeline(fence));
	kfree_rcu(f, base.rcu);
}

static void mdss_fence_value_str(struct fence *fence, char *str, int size)
{
	snprintf(str, size, "%u", fence->seqno);
}

static void mdss_fence_timeline_value_str(struct fence *fence, char *str,
		int size)
{
	struct mdss_timeline *tl = to_mdss_timeline(fence);

	snprintf(str, size, "%u", tl->value);
}

static struct fence_ops mdss_fence_ops = {
	.get_driver_name = mdss_fence_get_driver_name,
	.get_timeline_name = mdss_fence_get_timeline_name,
	.enable_signaling = mdss_fence_enable_signaling,
	.signaled = mdss_fence_signaled,
	.wait = fence_default_wait,
	.release = mdss_fence_release,
	.fence_value_str = mdss_fence_value_str,
	.timeline_value_str = mdss_fence_timeline_value_str,
};

/*
 * mdss_create_timeline - Create timeline object with the given name
 * @name: Pointer to name character string.
 */
struct mdss_timeline *mdss_create_timeline(const char *name)
{
	struct mdss_timeline *tl;

	if (!name) {
		pr_err("invalid parameters\n");
		return NULL;
	}

	tl = kzalloc(sizeof(struct mdss_timeline), GFP_KERNEL);
	if (!tl)
		return NULL;

	kref_init(&tl->kref);
	snprintf(tl->name, sizeof(tl->name), "%s", name);
	spin_lock_init(&tl->lock);
	tl->context = fence_context_alloc(1);
	INIT_LIST_HEAD(&tl->fence_list_head);

	return tl;
}

/*
 * mdss_destroy_timeline - Destroy the given timeline object
 * @tl: Pointer to timeline object.
 */
void mdss_destroy_timeline(struct mdss_timeline *tl)
{
	mdss_put_timeline(tl);
}

/*
 * mdss_inc_timeline_locked - Increment timeline by given amount
 * @tl: Pointer to timeline object.
 * @increment: the amount to increase the timeline by.
 */
static int mdss_inc_timeline_locked(struct mdss_timeline *tl,
		int increment)
{
	struct mdss_fence *f, *next;

	tl->value += increment;
	list_for_each_entry_safe(f, next, &tl->fence_list_head, fence_list) {
		if (fence_is_signaled_locked(&f->base)) {
			pr_debug("%s signaled\n", f->name);
			list_del_init(&f->fence_list);
		}
	}

	return 0;
}

/*
 * mdss_resync_timeline - Resync timeline to last committed value
 * @tl: Pointer to timeline object.
 */
void mdss_resync_timeline(struct mdss_timeline *tl)
{
	unsigned long flags;
	s32 val;

	if (!tl) {
		pr_err("invalid parameters\n");
		return;
	}

	spin_lock_irqsave(&tl->lock, flags);
	val = tl->next_value - tl->value;
	if (val > 0) {
		pr_warn("flush %s:%d\n", tl->name, val);
		mdss_inc_timeline_locked(tl, val);
	}
	spin_unlock_irqrestore(&tl->lock, flags);
}

/*
 * mdss_get_sync_fence - Create fence object from the given timeline
 * @tl: Pointer to timeline object
 * @timestamp: Pointer to timestamp of the returned fence. Null if not required.
 * Return: pointer fence created on give time line.
 */
struct mdss_fence *mdss_get_sync_fence(
		struct mdss_timeline *tl, const char *fence_name,
		u32 *timestamp, int offset)
{
	struct mdss_fence *f;
	unsigned long flags;
	u32 val;

	if (!tl) {
		pr_err("invalid parameters\n");
		return NULL;
	}

	f = kzalloc(sizeof(struct mdss_fence), GFP_KERNEL);
	if (!f)
		return NULL;

	INIT_LIST_HEAD(&f->fence_list);
	spin_lock_irqsave(&tl->lock, flags);
	val = tl->next_value + offset;
	tl->next_value += 1;
	fence_init(&f->base, &mdss_fence_ops, &tl->lock, tl->context, val);
	list_add_tail(&f->fence_list, &tl->fence_list_head);
	mdss_get_timeline(tl);
	spin_unlock_irqrestore(&tl->lock, flags);
	snprintf(f->name, sizeof(f->name), "%s_%u", fence_name, val);

	if (timestamp)
		*timestamp = val;

	pr_debug("fence created at val=%u tl->name %s next_value %d value %d offset %d\n",
			val, tl->name, tl->next_value, tl->value, offset);

	return (struct mdss_fence *) &f->base;
}

/*
 * mdss_inc_timeline - Increment timeline by given amount
 * @tl: Pointer to timeline object.
 * @increment: the amount to increase the timeline by.
 */
int mdss_inc_timeline(struct mdss_timeline *tl, int increment)
{
	unsigned long flags;
	int rc;

	if (!tl) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&tl->lock, flags);
	rc = mdss_inc_timeline_locked(tl, increment);
	spin_unlock_irqrestore(&tl->lock, flags);

	return rc;
}

/*
 * mdss_get_timeline_commit_ts - Return commit tick of given timeline
 * @tl: Pointer to timeline object.
 */
u32 mdss_get_timeline_commit_ts(struct mdss_timeline *tl)
{
	if (!tl) {
		pr_err("invalid parameters\n");
		return 0;
	}

	return tl->next_value;
}

/*
 * mdss_get_timeline_retire_ts - Return retire tick of given timeline
 * @tl: Pointer to timeline object.
 */
u32 mdss_get_timeline_retire_ts(struct mdss_timeline *tl)
{
	if (!tl) {
		pr_err("invalid parameters\n");
		return 0;
	}

	return tl->value;
}

/*
 * mdss_put_sync_fence - Destroy given fence object
 * @fence: Pointer to fence object.
 */
void mdss_put_sync_fence(struct mdss_fence *fence)
{
	if (!fence) {
		pr_err("invalid parameters\n");
		return;
	}

	fence_put((struct fence *) fence);
}

/*
 * mdss_wait_sync_fence - Wait until fence signal or timeout
 * @fence: Pointer to fence object.
 * @timeout: maximum wait time, in msec, for fence to signal.
 */
int mdss_wait_sync_fence(struct mdss_fence *fence,
		long timeout)
{
	int rc;

	if (!fence) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	rc = fence_wait_timeout((struct fence *) fence, false,
			msecs_to_jiffies(timeout));
	if (rc > 0) {
		pr_debug("fence signaled\n");
		rc = 0;
	} else if (rc == 0) {
		pr_debug("fence timeout\n");
		rc = -ETIMEDOUT;
	}

	return rc;
}

/*
 * mdss_get_fd_sync_fence - Get fence object of given file descriptor
 * @fd: File description of fence object.
 */
struct mdss_fence *mdss_get_fd_sync_fence(int fd)
{
	return (struct mdss_fence *) sync_file_get_fence(fd);
}

/*
 * mdss_get_sync_fence_fd - Get file descriptor of given fence object
 * @fence: Pointer to fence object.
 * Return: File descriptor on success, or error code on error
 */
int mdss_get_sync_fence_fd(struct mdss_fence *fence)
{
	int fd;
	struct sync_file *sync_file;

	if (!fence) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		pr_err("fail to get unused fd\n");
		return fd;
	}

	sync_file = sync_file_create((struct fence *) fence);
	if (!sync_file) {
		put_unused_fd(fd);
		pr_err("failed to create sync file\n");
		return -ENOMEM;
	}

	fd_install(fd, sync_file->file);

	return fd;
}

/*
 * mdss_put_sync_fence - Destroy given fence object
 * @fence: Pointer to fence object.
 * Return: fence name
 */
const char *mdss_get_sync_fence_name(struct mdss_fence *fence)
{
	if (!fence) {
		pr_err("invalid parameters\n");
		return NULL;
	}

	return fence->name;
}

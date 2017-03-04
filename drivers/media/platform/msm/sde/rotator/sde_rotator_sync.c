/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include "sde_rotator_util.h"
#include "sde_rotator_sync.h"

#define SDE_ROT_SYNC_NAME_SIZE		64
#define SDE_ROT_SYNC_DRIVER_NAME	"sde_rot"

/**
 * struct sde_rot_fence - sync fence context
 * @base: base sync fence object
 * @name: name of this sync fence
 * @fence_list: linked list of outstanding sync fence
 */
struct sde_rot_fence {
	struct fence base;
	char name[SDE_ROT_SYNC_NAME_SIZE];
	struct list_head fence_list;
};

/**
 * struct sde_rot_timeline - sync timeline context
 * @kref: reference count of timeline
 * @lock: serialization lock for timeline and fence update
 * @name: name of timeline
 * @fence_name: fence name prefix
 * @next_value: next commit sequence number
 * @curr_value: current retired sequence number
 * @context: fence context identifier
 * @fence_list_head: linked list of outstanding sync fence
 */
struct sde_rot_timeline {
	struct kref kref;
	spinlock_t lock;
	char name[SDE_ROT_SYNC_NAME_SIZE];
	char fence_name[SDE_ROT_SYNC_NAME_SIZE];
	u32 next_value;
	u32 curr_value;
	u64 context;
	struct list_head fence_list_head;
};

/*
 * to_sde_rot_fence - get rotator fence from fence base object
 * @fence: Pointer to fence base object
 */
static struct sde_rot_fence *to_sde_rot_fence(struct fence *fence)
{
	return container_of(fence, struct sde_rot_fence, base);
}

/*
 * to_sde_rot_timeline - get rotator timeline from fence base object
 * @fence: Pointer to fence base object
 */
static struct sde_rot_timeline *to_sde_rot_timeline(struct fence *fence)
{
	return container_of(fence->lock, struct sde_rot_timeline, lock);
}

/*
 * sde_rotator_free_timeline - Free the given timeline object
 * @kref: Pointer to timeline kref object.
 */
static void sde_rotator_free_timeline(struct kref *kref)
{
	struct sde_rot_timeline *tl =
		container_of(kref, struct sde_rot_timeline, kref);

	kfree(tl);
}

/*
 * sde_rotator_put_timeline - Put the given timeline object
 * @tl: Pointer to timeline object.
 */
static void sde_rotator_put_timeline(struct sde_rot_timeline *tl)
{
	if (!tl) {
		SDEROT_ERR("invalid parameters\n");
		return;
	}

	kref_put(&tl->kref, sde_rotator_free_timeline);
}

/*
 * sde_rotator_get_timeline - Get the given timeline object
 * @tl: Pointer to timeline object.
 */
static void sde_rotator_get_timeline(struct sde_rot_timeline *tl)
{
	if (!tl) {
		SDEROT_ERR("invalid parameters\n");
		return;
	}

	kref_get(&tl->kref);
}

static const char *sde_rot_fence_get_driver_name(struct fence *fence)
{
	return SDE_ROT_SYNC_DRIVER_NAME;
}

static const char *sde_rot_fence_get_timeline_name(struct fence *fence)
{
	struct sde_rot_timeline *tl = to_sde_rot_timeline(fence);

	return tl->name;
}

static bool sde_rot_fence_enable_signaling(struct fence *fence)
{
	return true;
}

static bool sde_rot_fence_signaled(struct fence *fence)
{
	struct sde_rot_timeline *tl = to_sde_rot_timeline(fence);
	bool status;

	status = ((s32) (tl->curr_value - fence->seqno)) >= 0;
	SDEROT_DBG("status:%d fence seq:%d and timeline:%d\n",
			status, fence->seqno, tl->curr_value);
	return status;
}

static void sde_rot_fence_release(struct fence *fence)
{
	struct sde_rot_fence *f = to_sde_rot_fence(fence);
	unsigned long flags;

	spin_lock_irqsave(fence->lock, flags);
	if (!list_empty(&f->fence_list))
		list_del(&f->fence_list);
	spin_unlock_irqrestore(fence->lock, flags);
	sde_rotator_put_timeline(to_sde_rot_timeline(fence));
	kfree_rcu(f, base.rcu);
}

static void sde_rot_fence_value_str(struct fence *fence, char *str, int size)
{
	snprintf(str, size, "%u", fence->seqno);
}

static void sde_rot_fence_timeline_value_str(struct fence *fence, char *str,
		int size)
{
	struct sde_rot_timeline *tl = to_sde_rot_timeline(fence);

	snprintf(str, size, "%u", tl->curr_value);
}

static struct fence_ops sde_rot_fence_ops = {
	.get_driver_name = sde_rot_fence_get_driver_name,
	.get_timeline_name = sde_rot_fence_get_timeline_name,
	.enable_signaling = sde_rot_fence_enable_signaling,
	.signaled = sde_rot_fence_signaled,
	.wait = fence_default_wait,
	.release = sde_rot_fence_release,
	.fence_value_str = sde_rot_fence_value_str,
	.timeline_value_str = sde_rot_fence_timeline_value_str,
};

/*
 * sde_rotator_create_timeline - Create timeline object with the given name
 * @name: Pointer to name character string.
 */
struct sde_rot_timeline *sde_rotator_create_timeline(const char *name)
{
	struct sde_rot_timeline *tl;

	if (!name) {
		SDEROT_ERR("invalid parameters\n");
		return NULL;
	}

	tl = kzalloc(sizeof(struct sde_rot_timeline), GFP_KERNEL);
	if (!tl)
		return NULL;

	kref_init(&tl->kref);
	snprintf(tl->name, sizeof(tl->name), "rot_timeline_%s", name);
	snprintf(tl->fence_name, sizeof(tl->fence_name), "rot_fence_%s", name);
	spin_lock_init(&tl->lock);
	tl->context = fence_context_alloc(1);
	INIT_LIST_HEAD(&tl->fence_list_head);

	return tl;
}

/*
 * sde_rotator_destroy_timeline - Destroy the given timeline object
 * @tl: Pointer to timeline object.
 */
void sde_rotator_destroy_timeline(struct sde_rot_timeline *tl)
{
	sde_rotator_put_timeline(tl);
}

/*
 * sde_rotator_inc_timeline_locked - Increment timeline by given amount
 * @tl: Pointer to timeline object.
 * @increment: the amount to increase the timeline by.
 */
static int sde_rotator_inc_timeline_locked(struct sde_rot_timeline *tl,
		int increment)
{
	struct sde_rot_fence *f, *next;

	tl->curr_value += increment;
	list_for_each_entry_safe(f, next, &tl->fence_list_head, fence_list) {
		if (fence_is_signaled_locked(&f->base)) {
			SDEROT_DBG("%s signaled\n", f->name);
			list_del_init(&f->fence_list);
		}
	}

	return 0;
}

/*
 * sde_rotator_resync_timeline - Resync timeline to last committed value
 * @tl: Pointer to timeline object.
 */
void sde_rotator_resync_timeline(struct sde_rot_timeline *tl)
{
	unsigned long flags;
	s32 val;

	if (!tl) {
		SDEROT_ERR("invalid parameters\n");
		return;
	}

	spin_lock_irqsave(&tl->lock, flags);
	val = tl->next_value - tl->curr_value;
	if (val > 0) {
		SDEROT_WARN("flush %s:%d\n", tl->name, val);
		sde_rotator_inc_timeline_locked(tl, val);
	}
	spin_unlock_irqrestore(&tl->lock, flags);
}

/*
 * sde_rotator_get_sync_fence - Create fence object from the given timeline
 * @tl: Pointer to timeline object
 * @fence_fd: Pointer to file descriptor associated with the returned fence.
 *		Null if not required.
 * @timestamp: Pointer to timestamp of the returned fence. Null if not required.
 */
struct sde_rot_sync_fence *sde_rotator_get_sync_fence(
		struct sde_rot_timeline *tl, int *fence_fd, u32 *timestamp)
{
	struct sde_rot_fence *f;
	unsigned long flags;
	u32 val;

	if (!tl) {
		SDEROT_ERR("invalid parameters\n");
		return NULL;
	}

	f = kzalloc(sizeof(struct sde_rot_fence), GFP_KERNEL);
	if (!f)
		return NULL;

	INIT_LIST_HEAD(&f->fence_list);
	spin_lock_irqsave(&tl->lock, flags);
	val = ++(tl->next_value);
	fence_init(&f->base, &sde_rot_fence_ops, &tl->lock, tl->context, val);
	list_add_tail(&f->fence_list, &tl->fence_list_head);
	sde_rotator_get_timeline(tl);
	spin_unlock_irqrestore(&tl->lock, flags);
	snprintf(f->name, sizeof(f->name), "%s_%u", tl->fence_name, val);

	if (fence_fd)
		*fence_fd = sde_rotator_get_sync_fence_fd(
				(struct sde_rot_sync_fence *) &f->base);

	if (timestamp)
		*timestamp = val;

	SDEROT_DBG("output sync fence created at val=%u\n", val);

	return (struct sde_rot_sync_fence *) &f->base;
}

/*
 * sde_rotator_inc_timeline - Increment timeline by given amount
 * @tl: Pointer to timeline object.
 * @increment: the amount to increase the timeline by.
 */
int sde_rotator_inc_timeline(struct sde_rot_timeline *tl, int increment)
{
	unsigned long flags;
	int rc;

	if (!tl) {
		SDEROT_ERR("invalid parameters\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&tl->lock, flags);
	rc = sde_rotator_inc_timeline_locked(tl, increment);
	spin_unlock_irqrestore(&tl->lock, flags);

	return rc;
}

/*
 * sde_rotator_get_timeline_commit_ts - Return commit tick of given timeline
 * @tl: Pointer to timeline object.
 */
u32 sde_rotator_get_timeline_commit_ts(struct sde_rot_timeline *tl)
{
	if (!tl) {
		SDEROT_ERR("invalid parameters\n");
		return 0;
	}

	return tl->next_value;
}

/*
 * sde_rotator_get_timeline_retire_ts - Return retire tick of given timeline
 * @tl: Pointer to timeline object.
 */
u32 sde_rotator_get_timeline_retire_ts(struct sde_rot_timeline *tl)
{
	if (!tl) {
		SDEROT_ERR("invalid parameters\n");
		return 0;
	}

	return tl->curr_value;
}

/*
 * sde_rotator_put_sync_fence - Destroy given fence object
 * @fence: Pointer to fence object.
 */
void sde_rotator_put_sync_fence(struct sde_rot_sync_fence *fence)
{
	if (!fence) {
		SDEROT_ERR("invalid parameters\n");
		return;
	}

	fence_put((struct fence *) fence);
}

/*
 * sde_rotator_wait_sync_fence - Wait until fence signal or timeout
 * @fence: Pointer to fence object.
 * @timeout: maximum wait time, in msec, for fence to signal.
 */
int sde_rotator_wait_sync_fence(struct sde_rot_sync_fence *fence,
		long timeout)
{
	int rc;

	if (!fence) {
		SDEROT_ERR("invalid parameters\n");
		return -EINVAL;
	}

	rc = fence_wait_timeout((struct fence *) fence, false,
			msecs_to_jiffies(timeout));
	if (rc > 0) {
		SDEROT_DBG("fence signaled\n");
		rc = 0;
	} else if (rc == 0) {
		SDEROT_DBG("fence timeout\n");
		rc = -ETIMEDOUT;
	}

	return rc;
}

/*
 * sde_rotator_get_sync_fence_fd - Get fence object of given file descriptor
 * @fd: File description of fence object.
 */
struct sde_rot_sync_fence *sde_rotator_get_fd_sync_fence(int fd)
{
	return (struct sde_rot_sync_fence *) sync_file_get_fence(fd);
}

/*
 * sde_rotator_get_sync_fence_fd - Get file descriptor of given fence object
 * @fence: Pointer to fence object.
 */
int sde_rotator_get_sync_fence_fd(struct sde_rot_sync_fence *fence)
{
	int fd;
	struct sync_file *sync_file;

	if (!fence) {
		SDEROT_ERR("invalid parameters\n");
		return -EINVAL;
	}

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		SDEROT_ERR("fail to get unused fd\n");
		return fd;
	}

	sync_file = sync_file_create((struct fence *) fence);
	if (!sync_file) {
		put_unused_fd(fd);
		SDEROT_ERR("failed to create sync file\n");
		return -ENOMEM;
	}

	fd_install(fd, sync_file->file);

	return fd;
}

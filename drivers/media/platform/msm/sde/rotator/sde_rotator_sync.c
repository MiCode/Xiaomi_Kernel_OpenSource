/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <sync.h>
#include <sw_sync.h>

#include "sde_rotator_util.h"
#include "sde_rotator_sync.h"

struct sde_rot_timeline {
	struct mutex lock;
	struct sw_sync_timeline *timeline;
	u32 next_value;
	char fence_name[32];
};
/*
 * sde_rotator_create_timeline - Create timeline object with the given name
 * @name: Pointer to name character string.
 */
struct sde_rot_timeline *sde_rotator_create_timeline(const char *name)
{
	char tl_name[32];
	struct sde_rot_timeline *tl;

	if (!name) {
		SDEROT_ERR("invalid parameters\n");
		return NULL;
	}

	tl = kzalloc(sizeof(struct sde_rot_timeline), GFP_KERNEL);
	if (!tl)
		return NULL;

	snprintf(tl_name, sizeof(tl_name), "rot_timeline_%s", name);
	SDEROT_DBG("timeline name=%s\n", tl_name);
	tl->timeline = sw_sync_timeline_create(tl_name);
	if (!tl->timeline) {
		SDEROT_ERR("fail to allocate timeline\n");
		kfree(tl);
		return NULL;
	}

	snprintf(tl->fence_name, sizeof(tl->fence_name), "rot_fence_%s", name);
	mutex_init(&tl->lock);
	tl->next_value = 0;

	return tl;
}

/*
 * sde_rotator_destroy_timeline - Destroy the given timeline object
 * @tl: Pointer to timeline object.
 */
void sde_rotator_destroy_timeline(struct sde_rot_timeline *tl)
{
	if (!tl) {
		SDEROT_ERR("invalid parameters\n");
		return;
	}

	if (tl->timeline)
		sync_timeline_destroy((struct sync_timeline *) tl->timeline);

	kfree(tl);
}

/*
 * sde_rotator_resync_timeline - Resync timeline to last committed value
 * @tl: Pointer to timeline object.
 */
void sde_rotator_resync_timeline(struct sde_rot_timeline *tl)
{
	int val;

	if (!tl || !tl->timeline) {
		SDEROT_ERR("invalid parameters\n");
		return;
	}
	mutex_lock(&tl->lock);
	val = tl->next_value - tl->timeline->value;
	if (val > 0) {
		SDEROT_WARN("flush %s:%d\n", tl->fence_name, val);
		sw_sync_timeline_inc(tl->timeline, val);
	}
	mutex_unlock(&tl->lock);
}

/*
 * sde_rotator_get_sync_fence - Create fence object from the given timeline
 * @tl: Pointer to timeline object
 * @fence_fd: Pointer to file descriptor associated with the returned fence.
 *		Null if not required.
 * @timestamp: Pointer to timestamp of the returned fence. Null if not required.
 */
struct sde_rot_sync_fence *sde_rotator_get_sync_fence(
		struct sde_rot_timeline *tl, int *fence_fd,
		u32 *timestamp)
{
	u32 val;
	struct sync_pt *sync_pt;
	struct sync_fence *fence;

	if (!tl || !tl->timeline) {
		SDEROT_ERR("invalid parameters\n");
		return NULL;
	}

	mutex_lock(&tl->lock);
	val = tl->next_value + 1;

	sync_pt = sw_sync_pt_create(tl->timeline, val);
	if (sync_pt == NULL) {
		SDEROT_ERR("cannot create sync point\n");
		goto sync_pt_create_err;
	}

	/* create fence */
	fence = sync_fence_create(tl->fence_name, sync_pt);
	if (fence == NULL) {
		SDEROT_ERR("%s: cannot create fence\n",
				tl->fence_name);
		goto sync_fence_create_err;
	}

	if (fence_fd) {
		int fd = get_unused_fd_flags(0);

		if (fd < 0) {
			SDEROT_ERR("get_unused_fd_flags failed error:0x%x\n",
					fd);
			goto get_fd_err;
		}

		sync_fence_install(fence, fd);
		*fence_fd = fd;
	}

	if (timestamp)
		*timestamp = val;

	tl->next_value++;
	mutex_unlock(&tl->lock);
	SDEROT_DBG("output sync point created at val=%u\n", val);

	return (struct sde_rot_sync_fence *) fence;
get_fd_err:
	SDEROT_DBG("sys_fence_put c:%p\n", fence);
	sync_fence_put(fence);
sync_fence_create_err:
	sync_pt_free(sync_pt);
sync_pt_create_err:
	mutex_unlock(&tl->lock);
	return NULL;
}

/*
 * sde_rotator_inc_timeline - Increment timeline by given amount
 * @tl: Pointer to timeline object.
 * @increment: the amount to increase the timeline by.
 */
int sde_rotator_inc_timeline(struct sde_rot_timeline *tl, int increment)
{
	if (!tl || !tl->timeline) {
		SDEROT_ERR("invalid parameters\n");
		return -EINVAL;
	}

	mutex_lock(&tl->lock);
	sw_sync_timeline_inc(tl->timeline, increment);
	mutex_unlock(&tl->lock);

	return 0;
}

/*
 * sde_rotator_get_timeline_commit_ts - Return commit tick of given timeline
 * @tl: Pointer to timeline object.
 */
u32 sde_rotator_get_timeline_commit_ts(struct sde_rot_timeline *tl)
{
	if (!tl)
		return 0;

	return tl->next_value;
}

/*
 * sde_rotator_get_timeline_retire_ts - Return retire tick of given timeline
 * @tl: Pointer to timeline object.
 */
u32 sde_rotator_get_timeline_retire_ts(struct sde_rot_timeline *tl)
{
	if (!tl || !tl->timeline) {
		SDEROT_ERR("invalid parameters\n");
		return 0;
	}

	return tl->timeline->value;
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

	sync_fence_put((struct sync_fence *) fence);
}

/*
 * sde_rotator_wait_sync_fence - Wait until fence signal or timeout
 * @fence: Pointer to fence object.
 * @timeout: maximum wait time, in msec, for fence to signal.
 */
int sde_rotator_wait_sync_fence(struct sde_rot_sync_fence *fence,
		long timeout)
{
	if (!fence)
		return -EINVAL;

	return sync_fence_wait((struct sync_fence *) fence, timeout);
}

/*
 * sde_rotator_get_sync_fence_fd - Get fence object of given file descriptor
 * @fd: File description of fence object.
 */
struct sde_rot_sync_fence *sde_rotator_get_fd_sync_fence(int fd)
{
	return (struct sde_rot_sync_fence *) sync_fence_fdget(fd);
}

/*
 * sde_rotator_get_sync_fence_fd - Get file descriptor of given fence object
 * @fence: Pointer to fence object.
 */
int sde_rotator_get_sync_fence_fd(struct sde_rot_sync_fence *fence)
{
	int fd;

	if (!fence) {
		SDEROT_ERR("invalid parameters\n");
		return -EINVAL;
	}

	fd = get_unused_fd_flags(0);

	if (fd < 0) {
		SDEROT_ERR("fail to get unused fd\n");
		return fd;
	}

	sync_fence_install((struct sync_fence *) fence, fd);

	return fd;
}


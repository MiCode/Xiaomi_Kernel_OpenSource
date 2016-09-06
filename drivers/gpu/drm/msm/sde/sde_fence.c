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

#include <sync.h>
#include <sw_sync.h>
#include "msm_drv.h"
#include "sde_kms.h"
#include "sde_fence.h"

void *sde_sync_get(uint64_t fd)
{
	/* force signed compare, fdget accepts an int argument */
	return (signed int)fd >= 0 ? sync_fence_fdget(fd) : NULL;
}

void sde_sync_put(void *fence)
{
	if (fence)
		sync_fence_put(fence);
}

int sde_sync_wait(void *fence, long timeout_ms)
{
	if (!fence)
		return -EINVAL;
	return sync_fence_wait(fence, timeout_ms);
}

uint32_t sde_sync_get_name_prefix(void *fence)
{
	char *name;
	uint32_t i, prefix;

	if (!fence)
		return 0x0;

	name = ((struct sync_fence *)fence)->name;
	prefix = 0x0;
	for (i = 0; i < sizeof(uint32_t) && name[i]; ++i)
		prefix = (prefix << CHAR_BIT) | name[i];

	return prefix;
}

#if IS_ENABLED(CONFIG_SW_SYNC)
/**
 * _sde_fence_create_fd - create fence object and return an fd for it
 * This function is NOT thread-safe.
 * @timeline: Timeline to associate with fence
 * @name: Name for fence
 * @val: Timeline value at which to signal the fence
 * Return: File descriptor on success, or error code on error
 */
static int _sde_fence_create_fd(void *timeline, const char *name, uint32_t val)
{
	struct sync_pt *sync_pt;
	struct sync_fence *fence;
	signed int fd = -EINVAL;

	if (!timeline) {
		SDE_ERROR("invalid timeline\n");
		goto exit;
	}

	if (!name)
		name = "sde_fence";

	/* create sync point */
	sync_pt = sw_sync_pt_create(timeline, val);
	if (sync_pt == NULL) {
		SDE_ERROR("failed to create sync point, %s\n", name);
		goto exit;
	}

	/* create fence */
	fence = sync_fence_create(name, sync_pt);
	if (fence == NULL) {
		sync_pt_free(sync_pt);
		SDE_ERROR("couldn't create fence, %s\n", name);
		goto exit;
	}

	/* create fd */
	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		SDE_ERROR("failed to get_unused_fd_flags(), %s\n", name);
		sync_fence_put(fence);
		goto exit;
	}

	sync_fence_install(fence, fd);
exit:
	return fd;
}

/**
 * SDE_FENCE_TIMELINE_NAME - macro for accessing s/w timeline's name
 * @fence: Pointer to sde fence structure
 * @drm_id: ID number of owning DRM Object
 * Returns: Pointer to timeline name string
 */
#define SDE_FENCE_TIMELINE_NAME(fence) \
	(((struct sw_sync_timeline *)fence->timeline)->obj.name)

int sde_fence_init(struct sde_fence *fence,
		const char *name,
		uint32_t drm_id)
{
	if (!fence) {
		SDE_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}

	fence->timeline = sw_sync_timeline_create(name ? name : "sde");
	if (!fence->timeline) {
		SDE_ERROR("failed to create timeline\n");
		return -ENOMEM;
	}

	fence->commit_count = 0;
	fence->done_count = 0;
	fence->drm_id = drm_id;

	mutex_init(&fence->fence_lock);
	return 0;

}

void sde_fence_deinit(struct sde_fence *fence)
{
	if (!fence) {
		SDE_ERROR("invalid fence\n");
		return;
	}

	mutex_destroy(&fence->fence_lock);
	if (fence->timeline)
		sync_timeline_destroy(fence->timeline);
}

int sde_fence_prepare(struct sde_fence *fence)
{
	if (!fence) {
		SDE_ERROR("invalid fence\n");
		return -EINVAL;
	}

	mutex_lock(&fence->fence_lock);
	++fence->commit_count;
	SDE_EVT32(fence->drm_id, fence->commit_count, fence->done_count);
	mutex_unlock(&fence->fence_lock);
	return 0;
}

int sde_fence_create(struct sde_fence *fence, uint64_t *val, int offset)
{
	uint32_t trigger_value;
	int fd, rc = -EINVAL;

	if (!fence || !fence->timeline || !val) {
		SDE_ERROR("invalid argument(s), fence %pK, pval %pK\n",
				fence, val);
	} else  {
		/*
		 * Allow created fences to have a constant offset with respect
		 * to the timeline. This allows us to delay the fence signalling
		 * w.r.t. the commit completion (e.g., an offset of +1 would
		 * cause fences returned during a particular commit to signal
		 * after an additional delay of one commit, rather than at the
		 * end of the current one.
		 */
		mutex_lock(&fence->fence_lock);
		trigger_value = fence->commit_count + (int32_t)offset;
		fd = _sde_fence_create_fd(fence->timeline,
				SDE_FENCE_TIMELINE_NAME(fence),
				trigger_value);
		*val = fd;

		SDE_EVT32(fence->drm_id, trigger_value, fd);
		mutex_unlock(&fence->fence_lock);

		if (fd >= 0)
			rc = 0;
	}

	return rc;
}

void sde_fence_signal(struct sde_fence *fence, bool is_error)
{
	if (!fence || !fence->timeline) {
		SDE_ERROR("invalid fence, %pK\n", fence);
		return;
	}

	mutex_lock(&fence->fence_lock);
	if ((fence->done_count - fence->commit_count) < 0)
		++fence->done_count;
	else
		SDE_ERROR("detected extra signal attempt!\n");

	/*
	 * Always advance 'done' counter,
	 * but only advance timeline if !error
	 */
	if (!is_error) {
		int32_t val;

		val = fence->done_count;
		val -= ((struct sw_sync_timeline *)
				fence->timeline)->value;
		if (val < 0)
			SDE_ERROR("invalid value\n");
		else
			sw_sync_timeline_inc(fence->timeline, (int)val);
	}

	SDE_EVT32(fence->drm_id, fence->done_count,
			((struct sw_sync_timeline *) fence->timeline)->value);

	mutex_unlock(&fence->fence_lock);
}
#endif

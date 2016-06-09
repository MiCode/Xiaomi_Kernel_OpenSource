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

#if IS_ENABLED(CONFIG_SW_SYNC)
void *sde_sync_timeline_create(const char *name)
{
	if (!name)
		name = "";
	return sw_sync_timeline_create(name);
}

int sde_sync_fence_create(void *timeline, const char *name, int val)
{
	struct sync_pt *sync_pt;
	struct sync_fence *fence;
	signed int fd = -EINVAL;

	if (!timeline) {
		DRM_ERROR("Invalid timeline\n");
		return fd;
	}

	if (val < 0) {
		DRM_ERROR("Invalid fence value %d\n", val);
		return fd;
	}

	if (!name)
		name = "";

	/* create sync point */
	sync_pt = sw_sync_pt_create(timeline, val);
	if (sync_pt == NULL) {
		DRM_ERROR("Failed to create sync point, %s\n", name);
		return fd;
	}

	/* create fence */
	fence = sync_fence_create(name, sync_pt);
	if (fence == NULL) {
		sync_pt_free(sync_pt);
		DRM_ERROR("Couldn't create fence, %s\n", name);
		return fd;
	}

	/* create fd */
	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		DRM_ERROR("Failed to get_unused_fd_flags(), %s\n", name);
		sync_fence_put(fence);
		return fd;
	}

	sync_fence_install(fence, fd);

	return fd;
}

void sde_sync_timeline_inc(void *timeline, int val)
{
	if (!timeline)
		DRM_ERROR("Invalid timeline\n");
	else if (val <= 0)
		DRM_ERROR("Invalid increment, %d\n", val);
	else
		sw_sync_timeline_inc(timeline, val);
}
#endif

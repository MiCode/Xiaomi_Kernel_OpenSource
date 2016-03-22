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

#ifndef SDE_ROTATOR_SYNC_H
#define SDE_ROTATOR_SYNC_H

#include <linux/types.h>
#include <linux/errno.h>

struct sde_rot_sync_fence;
struct sde_rot_timeline;

#if defined(CONFIG_SYNC) && defined(CONFIG_SW_SYNC)
struct sde_rot_timeline *sde_rotator_create_timeline(const char *name);

void sde_rotator_destroy_timeline(struct sde_rot_timeline *tl);

struct sde_rot_sync_fence *sde_rotator_get_sync_fence(
		struct sde_rot_timeline *tl, int *fence_fd, u32 *timestamp);

void sde_rotator_resync_timeline(struct sde_rot_timeline *tl);

u32 sde_rotator_get_timeline_commit_ts(struct sde_rot_timeline *tl);

u32 sde_rotator_get_timeline_retire_ts(struct sde_rot_timeline *tl);

int sde_rotator_inc_timeline(struct sde_rot_timeline *tl, int increment);

void sde_rotator_put_sync_fence(struct sde_rot_sync_fence *fence);

int sde_rotator_wait_sync_fence(struct sde_rot_sync_fence *fence,
		long timeout);

struct sde_rot_sync_fence *sde_rotator_get_fd_sync_fence(int fd);

int sde_rotator_get_sync_fence_fd(struct sde_rot_sync_fence *fence);

#else
static inline
struct sde_rot_timeline *sde_rotator_create_timeline(const char *name)
{
	return NULL;
}

static inline
void sde_rotator_destroy_timeline(struct sde_rot_timeline *tl)
{
}

static inline
struct sde_rot_sync_fence *sde_rotator_get_sync_fence(
		struct sde_rot_timeline *tl, int *fence_fd, u32 *timestamp)
{
	return NULL;
}

static inline
void sde_rotator_resync_timeline(struct sde_rot_timeline *tl)
{
}

static inline
int sde_rotator_inc_timeline(struct sde_rot_timeline *tl, int increment)
{
	return 0;
}

static inline
u32 sde_rotator_get_timeline_commit_ts(struct sde_rot_timeline *tl)
{
	return 0;
}

static inline
u32 sde_rotator_get_timeline_retire_ts(struct sde_rot_timeline *tl)
{
	return 0;
}

static inline
void sde_rotator_put_sync_fence(struct sde_rot_sync_fence *fence)
{
}

static inline
int sde_rotator_wait_sync_fence(struct sde_rot_sync_fence *fence,
		long timeout)
{
	return 0;
}

static inline
struct sde_rot_sync_fence *sde_rotator_get_fd_sync_fence(int fd)
{
	return NULL;
}

static inline
int sde_rotator_get_sync_fence_fd(struct sde_rot_sync_fence *fence)
{
	return -EBADF;
}
#endif

#endif /* SDE_ROTATOR_SYNC_H */

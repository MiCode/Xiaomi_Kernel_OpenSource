/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#ifndef LINUX_ONESHOT_SYNC_H
#define LINUX_ONESHOT_SYNC_H

#include <uapi/linux/oneshot_sync.h>

struct oneshot_sync_timeline;
struct sync_fence;

#ifdef CONFIG_ONESHOT_SYNC

struct oneshot_sync_timeline *oneshot_timeline_create(const char *name);

void oneshot_timeline_destroy(struct oneshot_sync_timeline *);

struct sync_fence *oneshot_fence_create(struct oneshot_sync_timeline *,
					const char *name);

int oneshot_fence_signal(struct oneshot_sync_timeline *, struct sync_fence *);

#else

static inline struct oneshot_sync_timeline *
oneshot_timeline_create(const char *name)
{
	return NULL;
}

void oneshot_timeline_destroy(struct oneshot_sync_timeline *timeline)
{
}

struct sync_fence *oneshot_fence_create(struct oneshot_sync_timeline *timeline,
					const char *name)
{
	return NULL;
}

int oneshot_fence_signal(struct oneshot_sync_timeline *timeline,
			struct sync_fence *fence)
{
	return -EINVAL;
}

#endif

#endif /* LINUX_ONESHOT_SYNC_H */

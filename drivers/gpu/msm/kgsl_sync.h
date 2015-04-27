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
#ifndef __KGSL_SYNC_H
#define __KGSL_SYNC_H

#include <linux/sync.h>
#include "kgsl_device.h"

struct kgsl_sync_timeline {
	struct sync_timeline timeline;
	unsigned int last_timestamp;
	struct kgsl_device *device;
	u32 context_id;
	spinlock_t lock;
};

struct kgsl_sync_pt {
	struct sync_pt pt;
	struct kgsl_context *context;
	unsigned int timestamp;
};

struct kgsl_sync_fence_waiter {
	struct sync_fence_waiter waiter;
	struct sync_fence *fence;
	char name[32];
	void (*func)(void *priv);
	void *priv;
};

struct kgsl_syncsource;

#if defined(CONFIG_SYNC)
int kgsl_add_fence_event(struct kgsl_device *device,
	u32 context_id, u32 timestamp, void __user *data, int len,
	struct kgsl_device_private *owner);
int kgsl_sync_timeline_create(struct kgsl_context *context);
void kgsl_sync_timeline_destroy(struct kgsl_context *context);
struct kgsl_sync_fence_waiter *kgsl_sync_fence_async_wait(int fd,
	void (*func)(void *priv), void *priv);
int kgsl_sync_fence_async_cancel(struct kgsl_sync_fence_waiter *waiter);
static inline void kgsl_sync_fence_log(struct sync_fence *fence)
{
}
#else
static inline int kgsl_add_fence_event(struct kgsl_device *device,
	u32 context_id, u32 timestamp, void __user *data, int len,
	struct kgsl_device_private *owner)
{
	return -EINVAL;
}

static inline int kgsl_sync_timeline_create(struct kgsl_context *context)
{
	context->timeline = NULL;
	return 0;
}

static inline void kgsl_sync_timeline_destroy(struct kgsl_context *context)
{
}

static inline struct
kgsl_sync_fence_waiter *kgsl_sync_fence_async_wait(int fd,
	void (*func)(void *priv), void *priv)
{
	return NULL;
}

static inline int
kgsl_sync_fence_async_cancel(struct kgsl_sync_fence_waiter *waiter)
{
	return 1;
}

static inline void kgsl_sync_fence_log(struct sync_fence *fence)
{
}

#endif

#ifdef CONFIG_ONESHOT_SYNC
long kgsl_ioctl_syncsource_create(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_syncsource_destroy(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_syncsource_create_fence(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_syncsource_signal_fence(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);

void kgsl_syncsource_put(struct kgsl_syncsource *syncsource);

#else
static inline long
kgsl_ioctl_syncsource_create(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	return -ENOIOCTLCMD;
}

static inline long
kgsl_ioctl_syncsource_destroy(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	return -ENOIOCTLCMD;
}

static inline long
kgsl_ioctl_syncsource_create_fence(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	return -ENOIOCTLCMD;
}

static inline long
kgsl_ioctl_syncsource_signal_fence(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	return -ENOIOCTLCMD;
}

static inline void kgsl_syncsource_put(struct kgsl_syncsource *syncsource)
{

}
#endif

#endif /* __KGSL_SYNC_H */

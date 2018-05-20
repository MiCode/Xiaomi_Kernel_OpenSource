/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_FENCE_H_
#define _SDE_FENCE_H_

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>

#ifndef CHAR_BIT
#define CHAR_BIT 8 /* define this if limits.h not available */
#endif

#define SDE_FENCE_NAME_SIZE	24

/**
 * struct sde_fence_context - release/retire fence context/timeline structure
 * @commit_count: Number of detected commits since bootup
 * @done_count: Number of completed commits since bootup
 * @drm_id: ID number of owning DRM Object
 * @ref: kref counter on timeline
 * @lock: spinlock for fence counter protection
 * @list_lock: spinlock for timeline protection
 * @context: fence context
 * @list_head: fence list to hold all the fence created on this context
 * @name: name of fence context/timeline
 */
struct sde_fence_context {
	unsigned int commit_count;
	unsigned int done_count;
	uint32_t drm_id;
	struct kref kref;
	spinlock_t lock;
	spinlock_t list_lock;
	u64 context;
	struct list_head fence_list_head;
	char name[SDE_FENCE_NAME_SIZE];
};

/**
 * enum sde_fence_event - sde fence event as hint fence operation
 * @SDE_FENCE_SIGNAL: Signal the fence cleanly with current timeline
 * @SDE_FENCE_RESET_TIMELINE: Reset timeline of the fence context
 * @SDE_FENCE_SIGNAL: Signal the fence but indicate error throughfence status
 */
enum sde_fence_event {
	SDE_FENCE_SIGNAL,
	SDE_FENCE_RESET_TIMELINE,
	SDE_FENCE_SIGNAL_ERROR
};

#if IS_ENABLED(CONFIG_SYNC_FILE)
/**
 * sde_sync_get - Query sync fence object from a file handle
 *
 * On success, this function also increments the refcount of the sync fence
 *
 * @fd: Integer sync fence handle
 *
 * Return: Pointer to sync fence object, or NULL
 */
void *sde_sync_get(uint64_t fd);

/**
 * sde_sync_put - Releases a sync fence object acquired by @sde_sync_get
 *
 * This function decrements the sync fence's reference count; the object will
 * be released if the reference count goes to zero.
 *
 * @fence: Pointer to sync fence
 */
void sde_sync_put(void *fence);

/**
 * sde_sync_wait - Query sync fence object from a file handle
 *
 * @fence: Pointer to sync fence
 * @timeout_ms: Time to wait, in milliseconds. Waits forever if timeout_ms < 0
 *
 * Return:
 * Zero if timed out
 * -ERESTARTSYS if wait interrupted
 * remaining jiffies in all other success cases.
 */
signed long sde_sync_wait(void *fence, long timeout_ms);

/**
 * sde_sync_get_name_prefix - get integer representation of fence name prefix
 * @fence: Pointer to opaque fence structure
 *
 * Return: 32-bit integer containing first 4 characters of fence name,
 *         big-endian notation
 */
uint32_t sde_sync_get_name_prefix(void *fence);

/**
 * sde_fence_init - initialize fence object
 * @fence: Pointer to crtc fence object
 * @drm_id: ID number of owning DRM Object
 * @name: Timeline name
 * Returns: Zero on success
 */
int sde_fence_init(struct sde_fence_context *fence,
		const char *name,
		uint32_t drm_id);

/**
 * sde_fence_deinit - deinit fence container
 * @fence: Pointer fence container
 */
void sde_fence_deinit(struct sde_fence_context *fence);

/**
 * sde_fence_prepare - prepare to return fences for current commit
 * @fence: Pointer fence container
 * Returns: Zero on success
 */
void sde_fence_prepare(struct sde_fence_context *fence);
/**
 * sde_fence_create - create output fence object
 * @fence: Pointer fence container
 * @val: Pointer to output value variable, fence fd will be placed here
 * @offset: Fence signal commit offset, e.g., +1 to signal on next commit
 * Returns: Zero on success
 */
int sde_fence_create(struct sde_fence_context *fence, uint64_t *val,
							uint32_t offset);

/**
 * sde_fence_signal - advance fence timeline to signal outstanding fences
 * @fence: Pointer fence container
 * @ts: fence timestamp
 * @fence_event: fence event to indicate nature of fence signal.
 */
void sde_fence_signal(struct sde_fence_context *fence, ktime_t ts,
		enum sde_fence_event fence_event);

/**
 * sde_fence_timeline_status - prints fence timeline status
 * @fence: Pointer fence container
 * @drm_obj Pointer to drm object associated with fence timeline
 */
void sde_fence_timeline_status(struct sde_fence_context *ctx,
					struct drm_mode_object *drm_obj);

#else
static inline void *sde_sync_get(uint64_t fd)
{
	return NULL;
}

static inline void sde_sync_put(void *fence)
{
}

static inline signed long sde_sync_wait(void *fence, long timeout_ms)
{
	return 0;
}

static inline uint32_t sde_sync_get_name_prefix(void *fence)
{
	return 0x0;
}
static inline int sde_fence_init(struct sde_fence_context *fence,
		const char *name,
		uint32_t drm_id)
{
	/* do nothing */
	return 0;
}

static inline void sde_fence_deinit(struct sde_fence_context *fence)
{
	/* do nothing */
}

static inline int sde_fence_get(struct sde_fence_context *fence, uint64_t *val)
{
	return -EINVAL;
}

static inline void sde_fence_signal(struct sde_fence_context *fence,
						ktime_t ts, bool reset_timeline)
{
	/* do nothing */
}

static inline void sde_fence_prepare(struct sde_fence_context *fence)
{
	/* do nothing */
}

static inline int sde_fence_create(struct sde_fence_context *fence,
						uint64_t *val, uint32_t offset)
{
	return 0;
}

static inline void sde_fence_timeline_status(struct sde_fence_context *ctx,
					struct drm_mode_object *drm_obj);
{
	/* do nothing */
}
#endif /* IS_ENABLED(CONFIG_SW_SYNC) */

#endif /* _SDE_FENCE_H_ */

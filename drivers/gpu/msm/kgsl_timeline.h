/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __KGSL_TIMELINE_H
#define __KGSL_TIMELINE_H

/**
 * struct kgsl_timeline - Container for a timeline object
 */
struct kgsl_timeline {
	/** @context: dma-fence timeline context */
	u64 context;
	/** @id: Timeline identifier */
	int id;
	/** @value: Current value of the timeline */
	u64 value;
	/** @fence_lock: Lock to protect @fences */
	spinlock_t fence_lock;
	/** @lock: Lock to use for locking each fence in @fences */
	spinlock_t lock;
	/** @ref: Reference count for the struct */
	struct kref ref;
	/** @fences: sorted list of active fences */
	struct list_head fences;
	/** @name: Name of the timeline for debugging */
	const char name[32];
	/** @dev_priv: pointer to the owning device instance */
	struct kgsl_device_private *dev_priv;
};

/**
 * kgsl_timeline_signal - Signal the timeline
 * @timeline: Pointer to a timeline container
 * @seqno: Seqeuence number to signal
 *
 * Advance @timeline to sequence number @seqno and signal any fences that might
 * have expired.
 */
void kgsl_timeline_signal(struct kgsl_timeline *timeline, u64 seqno);

/**
 * kgsl_timeline_destroy - Timeline destroy callback
 * @kref: Refcount pointer for the timeline
 *
 * Reference count callback for the timeline called when the all the object
 * references have been released.
 */
void kgsl_timeline_destroy(struct kref *kref);

/**
 * kgsl_timeline_fence_alloc - Allocate a new fence on a timeline
 * @timeline: Pointer to a timeline container
 * @seqno: Sequence number for the new fence to wait for
 *
 * Create and return a new fence on the timeline that will expire when the
 * timeline value is greater or equal to @seqno.
 * Return: A pointer to the newly created fence
 */
struct dma_fence *kgsl_timeline_fence_alloc(struct kgsl_timeline *timeline,
		u64 seqno);

/**
 * kgsl_timeline_by_id - Look up a timeline by an id
 * @device: A KGSL device handle
 * @id: Lookup identifier
 *
 * Find and return the timeline associated with identifer @id.
 * Return: A pointer to a timeline or PTR_ERR() encoded error on failure.
 */
struct kgsl_timeline *kgsl_timeline_by_id(struct kgsl_device *device,
		u32 id);

/**
 * kgsl_timeline_get - Get a reference to an existing timeline
 * @timeline: Pointer to a timeline container
 *
 * Get a new reference to the timeline and return the pointer back to the user.
 * Return: The pointer to the timeline or PTR_ERR encoded error on failure
 */
struct kgsl_timeline *kgsl_timeline_get(struct kgsl_timeline *timeline);

/**
 * kgsl_timeline_put - Release a reference to a timeline
 * @timeline: Pointer to a timeline container
 *
 * Release a reference to a timeline and destroy it if there are no other
 * references
 */
static inline void kgsl_timeline_put(struct kgsl_timeline *timeline)
{
	if (!IS_ERR_OR_NULL(timeline))
		kref_put(&timeline->ref, kgsl_timeline_destroy);
}

/**
 * kgsl_timelines_to_fence_array - Return a dma-fence array of timeline fences
 * @device: A KGSL device handle
 * @timelines: Userspace pointer to an array of &struct kgsl_timeline_val
 * @count: Number of entries in @timelines
 * @usize: Size of each entry in @timelines
 * @any: True if the fence should expire on any timeline expiring or false if it
 * should wait until all timelines have expired
 *
 * Give a list of &struct kgsl_timeline_val entries, create a dma-fence-array
 * containing fences for each timeline/seqno pair. If @any is set the
 * dma-fence-array will be set to expire if any of the encapsulated timeline
 * fences expire.  If @any is false, then the fence will wait for ALL of the
 * encapsulated timeline fences to expire.
 */
struct dma_fence *kgsl_timelines_to_fence_array(struct kgsl_device *device,
		u64 timelines, u32 count, u64 usize, bool any);

#endif

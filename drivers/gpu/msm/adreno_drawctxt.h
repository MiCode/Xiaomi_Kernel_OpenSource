/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002,2007-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __ADRENO_DRAWCTXT_H
#define __ADRENO_DRAWCTXT_H

#include <linux/types.h>

#include "kgsl_device.h"

struct adreno_context_type {
	unsigned int type;
	const char *str;
};

#define ADRENO_CONTEXT_DRAWQUEUE_SIZE 128
#define SUBMIT_RETIRE_TICKS_SIZE 7

struct kgsl_device;
struct adreno_device;
struct kgsl_device_private;

/**
 * struct adreno_context - Adreno GPU draw context
 * @timestamp: Last issued context-specific timestamp
 * @internal_timestamp: Global timestamp of the last issued command
 *			NOTE: guarded by device->mutex, not drawctxt->mutex!
 * @type: Context type (GL, CL, RS)
 * @mutex: Mutex to protect the drawqueue
 * @drawqueue: Queue of drawobjs waiting to be dispatched for this
 *			context
 * @drawqueue_head: Head of the drawqueue queue
 * @drawqueue_tail: Tail of the drawqueue queue
 * @wq: Workqueue structure for contexts to sleep pending room in the queue
 * @waiting: Workqueue structure for contexts waiting for a timestamp or event
 * @timeout: Workqueue structure for contexts waiting to invalidate
 * @queued: Number of commands queued in the drawqueue
 * @fault_policy: GFT fault policy set in _skip_cmd();
 * @debug_root: debugfs entry for this context.
 * @queued_timestamp: The last timestamp that was queued on this context
 * @rb: The ringbuffer in which this context submits commands.
 * @submitted_timestamp: The last timestamp that was submitted for this context
 * @submit_retire_ticks: Array to hold command obj execution times from submit
 *                       to retire
 * @ticks_index: The index into submit_retire_ticks[] where the new delta will
 *		 be written.
 * @active_node: Linkage for nodes in active_list
 * @active_time: Time when this context last seen
 */
struct adreno_context {
	struct kgsl_context base;
	unsigned int timestamp;
	unsigned int internal_timestamp;
	unsigned int type;
	spinlock_t lock;

	/* Dispatcher */
	struct kgsl_drawobj *drawqueue[ADRENO_CONTEXT_DRAWQUEUE_SIZE];
	unsigned int drawqueue_head;
	unsigned int drawqueue_tail;

	wait_queue_head_t wq;
	wait_queue_head_t waiting;
	wait_queue_head_t timeout;

	int queued;
	unsigned int fault_policy;
	struct dentry *debug_root;
	unsigned int queued_timestamp;
	struct adreno_ringbuffer *rb;
	unsigned int submitted_timestamp;
	uint64_t submit_retire_ticks[SUBMIT_RETIRE_TICKS_SIZE];
	int ticks_index;

	struct list_head active_node;
	unsigned long active_time;
	/** @gmu_context_queue: Queue to dispatch submissions to GMU */
	struct kgsl_memdesc gmu_context_queue;
	/** @gmu_hw_fence_queue: Queue for GMU to store hardware fences for this context */
	struct kgsl_memdesc gmu_hw_fence_queue;
	/** @hw_fence_list: List of hardware fences(sorted by timestamp) not yet submitted to GMU */
	struct list_head hw_fence_list;
	/** @hw_fence_ts: timestamp of the last hardware fence in the fence list */
	u32 hw_fence_ts;
	/** @hw_fence_count: Number of hardware fences not yet sent to Tx Queue */
	u32 hw_fence_count;
	/** @syncobj_timestamp: Timestamp to check whether GMU has consumed a syncobj */
	u32 syncobj_timestamp;
};

/* Flag definitions for flag field in adreno_context */

/**
 * enum adreno_context_priv - Private flags for an adreno draw context
 * @ADRENO_CONTEXT_FAULT - set if the context has faulted (and recovered)
 * @ADRENO_CONTEXT_GPU_HANG - Context has caused a GPU hang
 * @ADRENO_CONTEXT_GPU_HANG_FT - Context has caused a GPU hang
 *      and fault tolerance was successful
 * @ADRENO_CONTEXT_SKIP_EOF - Context skip IBs until the next end of frame
 *      marker.
 * @ADRENO_CONTEXT_FORCE_PREAMBLE - Force the preamble for the next submission.
 * @ADRENO_CONTEXT_SKIP_CMD - Context's drawobj's skipped during
	fault tolerance.
 * @ADRENO_CONTEXT_FENCE_LOG - Dump fences on this context.
 */
enum adreno_context_priv {
	ADRENO_CONTEXT_FAULT = KGSL_CONTEXT_PRIV_DEVICE_SPECIFIC,
	ADRENO_CONTEXT_GPU_HANG,
	ADRENO_CONTEXT_GPU_HANG_FT,
	ADRENO_CONTEXT_SKIP_EOF,
	ADRENO_CONTEXT_FORCE_PREAMBLE,
	ADRENO_CONTEXT_SKIP_CMD,
	ADRENO_CONTEXT_FENCE_LOG,
	/**
	 * @ADRENO_CONTEXT_DRAIN_HW_FENCE: Signal any hardware fences that weren't dispatched to
	 * GMU
	 */
	ADRENO_CONTEXT_DRAIN_HW_FENCE,
};

struct kgsl_context *adreno_drawctxt_create(
			struct kgsl_device_private *dev_priv,
			uint32_t *flags);

void adreno_drawctxt_detach(struct kgsl_context *context);

void adreno_drawctxt_destroy(struct kgsl_context *context);

struct adreno_ringbuffer;

int adreno_drawctxt_wait(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout);

void adreno_drawctxt_invalidate(struct kgsl_device *device,
		struct kgsl_context *context);

void adreno_drawctxt_dump(struct kgsl_device *device,
		struct kgsl_context *context);

/**
 * adreno_drawctxt_detached - Helper function to check if a context is detached
 * @drawctxt: Adreno drawctxt to check
 *
 * Return: True if the context isn't null and it has been detached
 */
static inline bool adreno_drawctxt_detached(struct adreno_context *drawctxt)
{
	return (drawctxt && kgsl_context_detached(&drawctxt->base));
}

/**
 * adreno_put_drawctxt_on_timestamp - Put the refcount on the drawctxt when the
 * timestamp expires
 * @device: A KGSL device handle
 * @drawctxt: The draw context to put away
 * @rb: The ringbuffer that will trigger the timestamp event
 * @timestamp: The timestamp on @rb that will trigger the event
 *
 * Add an event to put the refcount on @drawctxt after @timestamp expires on
 * @rb. This is used by the context switch to safely put away the context after
 * a new context is switched in.
 */
void adreno_put_drawctxt_on_timestamp(struct kgsl_device *device,
		struct adreno_context *drawctxt,
		struct adreno_ringbuffer *rb, u32 timestamp);

/**
 * adreno_drawctxt_get_pagetable - Helper function to return the pagetable for a
 * context
 * @drawctxt: The adreno draw context to query
 *
 * Return: A pointer to the pagetable for the process that owns the context or
 * NULL
 */
static inline struct kgsl_pagetable *
adreno_drawctxt_get_pagetable(struct adreno_context *drawctxt)
{
	if (drawctxt)
		return drawctxt->base.proc_priv->pagetable;

	return NULL;
}

/**
 * adreno_drawctxt_set_guilty - Mark a context as guilty and invalidate it
 * @device: Pointer to a GPU device handle
 * @context: Poniter to the context to invalidate
 *
 * Mark the specified context as guilty and invalidate it
 */
void adreno_drawctxt_set_guilty(struct kgsl_device *device,
		struct kgsl_context *context);
#endif  /* __ADRENO_DRAWCTXT_H */

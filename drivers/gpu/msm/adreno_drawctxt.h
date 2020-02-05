/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002,2007-2020, The Linux Foundation. All rights reserved.
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
};

struct kgsl_context *adreno_drawctxt_create(
			struct kgsl_device_private *dev_priv,
			uint32_t *flags);

void adreno_drawctxt_detach(struct kgsl_context *context);

void adreno_drawctxt_destroy(struct kgsl_context *context);

struct adreno_ringbuffer;
int adreno_drawctxt_switch(struct adreno_device *adreno_dev,
				struct adreno_ringbuffer *rb,
				struct adreno_context *drawctxt);

int adreno_drawctxt_wait(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout);

void adreno_drawctxt_invalidate(struct kgsl_device *device,
		struct kgsl_context *context);

void adreno_drawctxt_dump(struct kgsl_device *device,
		struct kgsl_context *context);

#endif  /* __ADRENO_DRAWCTXT_H */

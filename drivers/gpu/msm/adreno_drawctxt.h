/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
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
#ifndef __ADRENO_DRAWCTXT_H
#define __ADRENO_DRAWCTXT_H

#include "adreno_pm4types.h"

/* Symbolic table for the adreno draw context type */
#define ADRENO_DRAWCTXT_TYPES \
	{ KGSL_CONTEXT_TYPE_ANY, "any" }, \
	{ KGSL_CONTEXT_TYPE_GL, "GL" }, \
	{ KGSL_CONTEXT_TYPE_CL, "CL" }, \
	{ KGSL_CONTEXT_TYPE_C2D, "C2D" }, \
	{ KGSL_CONTEXT_TYPE_RS, "RS" }, \
	{ KGSL_CONTEXT_TYPE_UNKNOWN, "UNKNOWN" }

struct adreno_context_type {
	unsigned int type;
	const char *str;
};

#define ADRENO_CONTEXT_CMDQUEUE_SIZE 128

#define ADRENO_CONTEXT_STATE_ACTIVE 0
#define ADRENO_CONTEXT_STATE_INVALID 1

struct kgsl_device;
struct adreno_device;
struct kgsl_device_private;
struct kgsl_context;

/**
 * struct adreno_context - Adreno GPU draw context
 * @timestamp: Last issued context-specific timestamp
 * @internal_timestamp: Global timestamp of the last issued command
 *			NOTE: guarded by device->mutex, not drawctxt->mutex!
 * @state: Current state of the context
 * @priv: Internal flags
 * @type: Context type (GL, CL, RS)
 * @mutex: Mutex to protect the cmdqueue
 * @cmdqueue: Queue of command batches waiting to be dispatched for this context
 * @cmdqueue_head: Head of the cmdqueue queue
 * @cmdqueue_tail: Tail of the cmdqueue queue
 * @pending: Priority list node for the dispatcher list of pending contexts
 * @wq: Workqueue structure for contexts to sleep pending room in the queue
 * @waiting: Workqueue structure for contexts waiting for a timestamp or event
 * @queued: Number of commands queued in the cmdqueue
 * @ops: Context switch functions for this context.
 * @fault_policy: GFT fault policy set in cmdbatch_skip_cmd();
 */
struct adreno_context {
	struct kgsl_context base;
	unsigned int timestamp;
	unsigned int internal_timestamp;
	int state;
	unsigned long priv;
	unsigned int type;
	struct mutex mutex;

	/* Dispatcher */
	struct kgsl_cmdbatch *cmdqueue[ADRENO_CONTEXT_CMDQUEUE_SIZE];
	unsigned int cmdqueue_head;
	unsigned int cmdqueue_tail;

	struct plist_node pending;
	wait_queue_head_t wq;
	wait_queue_head_t waiting;

	int queued;
	unsigned int fault_policy;
};

/**
 * enum adreno_context_priv - Private flags for an adreno draw context
 * @ADRENO_CONTEXT_FAULT - set if the context has faulted (and recovered)
 * @ADRENO_CONTEXT_GPU_HANG - Context has caused a GPU hang
 * @ADRENO_CONTEXT_GPU_HANG_FT - Context has caused a GPU hang
 *      and fault tolerance was successful
 * @ADRENO_CONTEXT_SKIP_EOF - Context skip IBs until the next end of frame
 *      marker.
 * @ADRENO_CONTEXT_FORCE_PREAMBLE - Force the preamble for the next submission.
 * @ADRENO_CONTEXT_SKIP_CMD - Context's command batch is skipped during
	fault tolerance.
 */
enum adreno_context_priv {
	ADRENO_CONTEXT_FAULT = 0,
	ADRENO_CONTEXT_GPU_HANG,
	ADRENO_CONTEXT_GPU_HANG_FT,
	ADRENO_CONTEXT_SKIP_EOF,
	ADRENO_CONTEXT_FORCE_PREAMBLE,
	ADRENO_CONTEXT_SKIP_CMD,
};

struct kgsl_context *adreno_drawctxt_create(struct kgsl_device_private *,
			uint32_t *flags);

int adreno_drawctxt_detach(struct kgsl_context *context);

void adreno_drawctxt_destroy(struct kgsl_context *context);

void adreno_drawctxt_sched(struct kgsl_device *device,
		struct kgsl_context *context);

int adreno_drawctxt_switch(struct adreno_device *adreno_dev,
				struct adreno_context *drawctxt,
				unsigned int flags);

int adreno_drawctxt_wait(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout);

void adreno_drawctxt_invalidate(struct kgsl_device *device,
		struct kgsl_context *context);

#endif  /* __ADRENO_DRAWCTXT_H */

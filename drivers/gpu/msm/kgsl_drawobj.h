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

#ifndef __KGSL_DRAWOBJ_H
#define __KGSL_DRAWOBJ_H

#define KGSL_DRAWOBJ_FLAGS \
	{ KGSL_DRAWOBJ_MARKER, "MARKER" }, \
	{ KGSL_DRAWOBJ_CTX_SWITCH, "CTX_SWITCH" }, \
	{ KGSL_DRAWOBJ_SYNC, "SYNC" }, \
	{ KGSL_DRAWOBJ_END_OF_FRAME, "EOF" }, \
	{ KGSL_DRAWOBJ_PWR_CONSTRAINT, "PWR_CONSTRAINT" }, \
	{ KGSL_DRAWOBJ_SUBMIT_IB_LIST, "IB_LIST" }

/**
 * struct kgsl_drawobj - KGSl command descriptor
 * @device: KGSL GPU device that the command was created for
 * @context: KGSL context that created the command
 * @timestamp: Timestamp assigned to the command
 * @flags: flags
 * @priv: Internal flags
 * @fault_policy: Internal policy describing how to handle this command in case
 * of a fault
 * @fault_recovery: recovery actions actually tried for this batch
 * @refcount: kref structure to maintain the reference count
 * @cmdlist: List of IBs to issue
 * @memlist: List of all memory used in this drawobj
 * @synclist: Array of context/timestamp tuples to wait for before issuing
 * @numsyncs: Number of sync entries in the array
 * @pending: Bitmask of sync events that are active
 * @timer: a timer used to track possible sync timeouts for this drawobj
 * @marker_timestamp: For markers, the timestamp of the last "real" command that
 * was queued
 * @profiling_buf_entry: Mem entry containing the profiling buffer
 * @profiling_buffer_gpuaddr: GPU virt address of the profile buffer added here
 * for easy access
 * @profile_index: Index to store the start/stop ticks in the kernel profiling
 * buffer
 * @submit_ticks: Variable to hold ticks at the time of drawobj submit.
 * @global_ts: The ringbuffer timestamp corresponding to this drawobj
 * @timeout_jiffies: For a syncpoint drawobj the jiffies at which the
 * timer will expire
 * This structure defines an atomic batch of command buffers issued from
 * userspace.
 */
struct kgsl_drawobj {
	struct kgsl_device *device;
	struct kgsl_context *context;
	uint32_t timestamp;
	uint32_t flags;
	unsigned long priv;
	unsigned long fault_policy;
	unsigned long fault_recovery;
	struct kref refcount;
	struct list_head cmdlist;
	struct list_head memlist;
	struct kgsl_drawobj_sync_event *synclist;
	unsigned int numsyncs;
	unsigned long pending;
	struct timer_list timer;
	unsigned int marker_timestamp;
	struct kgsl_mem_entry *profiling_buf_entry;
	uint64_t profiling_buffer_gpuaddr;
	unsigned int profile_index;
	uint64_t submit_ticks;
	unsigned int global_ts;
	unsigned long timeout_jiffies;
};

/**
 * struct kgsl_drawobj_sync_event
 * @id: identifer (positiion within the pending bitmap)
 * @type: Syncpoint type
 * @drawobj: Pointer to the drawobj that owns the sync event
 * @context: Pointer to the KGSL context that owns the drawobj
 * @timestamp: Pending timestamp for the event
 * @handle: Pointer to a sync fence handle
 * @device: Pointer to the KGSL device
 */
struct kgsl_drawobj_sync_event {
	unsigned int id;
	int type;
	struct kgsl_drawobj *drawobj;
	struct kgsl_context *context;
	unsigned int timestamp;
	struct kgsl_sync_fence_waiter *handle;
	struct kgsl_device *device;
};

/**
 * enum kgsl_drawobj_priv - Internal drawobj flags
 * @DRAWOBJ_FLAG_SKIP - skip the entire drawobj
 * @DRAWOBJ_FLAG_FORCE_PREAMBLE - Force the preamble on for the drawobj
 * @DRAWOBJ_FLAG_WFI - Force wait-for-idle for the submission
 * @DRAWOBJ_FLAG_PROFILE - store the start / retire ticks for the drawobj
 * in the profiling buffer
 * @DRAWOBJ_FLAG_FENCE_LOG - Set if the drawobj is dumping fence logs via the
 * drawobj timer - this is used to avoid recursion
 */

enum kgsl_drawobj_priv {
	DRAWOBJ_FLAG_SKIP = 0,
	DRAWOBJ_FLAG_FORCE_PREAMBLE,
	DRAWOBJ_FLAG_WFI,
	DRAWOBJ_FLAG_PROFILE,
	DRAWOBJ_FLAG_FENCE_LOG,
};


int kgsl_drawobj_add_memobj(struct kgsl_drawobj *drawobj,
		struct kgsl_ibdesc *ibdesc);

int kgsl_drawobj_add_sync(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj,
		struct kgsl_cmd_syncpoint *sync);

struct kgsl_drawobj *kgsl_drawobj_create(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int flags);
int kgsl_drawobj_add_ibdesc(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, struct kgsl_ibdesc *ibdesc);
int kgsl_drawobj_add_ibdesc_list(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr, int count);
int kgsl_drawobj_add_syncpoints(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr, int count);
int kgsl_drawobj_add_cmdlist(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr,
		unsigned int size, unsigned int count);
int kgsl_drawobj_add_memlist(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr,
		unsigned int size, unsigned int count);
int kgsl_drawobj_add_synclist(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj, void __user *ptr,
		unsigned int size, unsigned int count);

int kgsl_drawobj_init(void);
void kgsl_drawobj_exit(void);

void kgsl_dump_syncpoints(struct kgsl_device *device,
	struct kgsl_drawobj *drawobj);

void kgsl_drawobj_destroy(struct kgsl_drawobj *drawobj);

void kgsl_drawobj_destroy_object(struct kref *kref);

static inline bool kgsl_drawobj_events_pending(struct kgsl_drawobj *drawobj)
{
	return !bitmap_empty(&drawobj->pending, KGSL_MAX_SYNCPOINTS);
}

static inline bool kgsl_drawobj_event_pending(struct kgsl_drawobj *drawobj,
		unsigned int bit)
{
	if (bit >= KGSL_MAX_SYNCPOINTS)
		return false;

	return test_bit(bit, &drawobj->pending);
}

#endif /* __KGSL_DRAWOBJ_H */

/* Copyright (c) 2008-2015, The Linux Foundation. All rights reserved.
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

#ifndef __KGSL_CMDBATCH_H
#define __KGSL_CMDBATCH_H

#define KGSL_CMDBATCH_FLAGS \
	{ KGSL_CMDBATCH_MARKER, "MARKER" }, \
	{ KGSL_CMDBATCH_CTX_SWITCH, "CTX_SWITCH" }, \
	{ KGSL_CMDBATCH_SYNC, "SYNC" }, \
	{ KGSL_CMDBATCH_END_OF_FRAME, "EOF" }, \
	{ KGSL_CMDBATCH_PWR_CONSTRAINT, "PWR_CONSTRAINT" }, \
	{ KGSL_CMDBATCH_SUBMIT_IB_LIST, "IB_LIST" }

/**
 * struct kgsl_cmdbatch - KGSl command descriptor
 * @device: KGSL GPU device that the command was created for
 * @context: KGSL context that created the command
 * @timestamp: Timestamp assigned to the command
 * @flags: flags
 * @priv: Internal flags
 * @fault_policy: Internal policy describing how to handle this command in case
 * of a fault
 * @fault_recovery: recovery actions actually tried for this batch
 * @expires: Point in time when the cmdbatch is considered to be hung
 * @refcount: kref structure to maintain the reference count
 * @cmdlist: List of IBs to issue
 * @memlist: List of all memory used in this command batch
 * @synclist: Array of context/timestamp tuples to wait for before issuing
 * @numsyncs: Number of sync entries in the array
 * @pending: Bitmask of sync events that are active
 * @timer: a timer used to track possible sync timeouts for this cmdbatch
 * @marker_timestamp: For markers, the timestamp of the last "real" command that
 * was queued
 * @profiling_buf_entry: Mem entry containing the profiling buffer
 * @profiling_buffer_gpuaddr: GPU virt address of the profile buffer added here
 * for easy access
 * @profile_index: Index to store the start/stop ticks in the kernel profiling
 * buffer
 * @submit_ticks: Variable to hold ticks at the time of cmdbatch submit.
 * @global_ts: The ringbuffer timestamp corresponding to this cmdbatch
 * @timeout_jiffies: For a syncpoint cmdbatch the jiffies at which the
 * timer will expire
 * This structure defines an atomic batch of command buffers issued from
 * userspace.
 */
struct kgsl_cmdbatch {
	struct kgsl_device *device;
	struct kgsl_context *context;
	uint32_t timestamp;
	uint32_t flags;
	unsigned long priv;
	unsigned long fault_policy;
	unsigned long fault_recovery;
	unsigned long expires;
	struct kref refcount;
	struct list_head cmdlist;
	struct list_head memlist;
	struct kgsl_cmdbatch_sync_event *synclist;
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
 * struct kgsl_cmdbatch_sync_event
 * @id: identifer (positiion within the pending bitmap)
 * @type: Syncpoint type
 * @cmdbatch: Pointer to the cmdbatch that owns the sync event
 * @context: Pointer to the KGSL context that owns the cmdbatch
 * @timestamp: Pending timestamp for the event
 * @handle: Pointer to a sync fence handle
 * @device: Pointer to the KGSL device
 */
struct kgsl_cmdbatch_sync_event {
	unsigned int id;
	int type;
	struct kgsl_cmdbatch *cmdbatch;
	struct kgsl_context *context;
	unsigned int timestamp;
	struct kgsl_sync_fence_waiter *handle;
	struct kgsl_device *device;
};

/**
 * enum kgsl_cmdbatch_priv - Internal cmdbatch flags
 * @CMDBATCH_FLAG_SKIP - skip the entire command batch
 * @CMDBATCH_FLAG_FORCE_PREAMBLE - Force the preamble on for the cmdbatch
 * @CMDBATCH_FLAG_WFI - Force wait-for-idle for the submission
 * @CMDBATCH_FLAG_PROFILE - store the start / retire ticks for the command batch
 * in the profiling buffer
 * @CMDBATCH_FLAG_FENCE_LOG - Set if the cmdbatch is dumping fence logs via the
 * cmdbatch timer - this is used to avoid recursion
 */

enum kgsl_cmdbatch_priv {
	CMDBATCH_FLAG_SKIP = 0,
	CMDBATCH_FLAG_FORCE_PREAMBLE,
	CMDBATCH_FLAG_WFI,
	CMDBATCH_FLAG_PROFILE,
	CMDBATCH_FLAG_FENCE_LOG,
};


int kgsl_cmdbatch_add_memobj(struct kgsl_cmdbatch *cmdbatch,
		struct kgsl_ibdesc *ibdesc);

int kgsl_cmdbatch_add_sync(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch,
		struct kgsl_cmd_syncpoint *sync);

struct kgsl_cmdbatch *kgsl_cmdbatch_create(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int flags);
int kgsl_cmdbatch_add_ibdesc(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, struct kgsl_ibdesc *ibdesc);
int kgsl_cmdbatch_add_ibdesc_list(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr, int count);
int kgsl_cmdbatch_add_syncpoints(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr, int count);
int kgsl_cmdbatch_add_cmdlist(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr,
		unsigned int size, unsigned int count);
int kgsl_cmdbatch_add_memlist(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr,
		unsigned int size, unsigned int count);
int kgsl_cmdbatch_add_synclist(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, void __user *ptr,
		unsigned int size, unsigned int count);

int kgsl_cmdbatch_init(void);
void kgsl_cmdbatch_exit(void);

void kgsl_dump_syncpoints(struct kgsl_device *device,
	struct kgsl_cmdbatch *cmdbatch);

void kgsl_cmdbatch_destroy(struct kgsl_cmdbatch *cmdbatch);

void kgsl_cmdbatch_destroy_object(struct kref *kref);

static inline bool kgsl_cmdbatch_events_pending(struct kgsl_cmdbatch *cmdbatch)
{
	return !bitmap_empty(&cmdbatch->pending, KGSL_MAX_SYNCPOINTS);
}

static inline bool kgsl_cmdbatch_event_pending(struct kgsl_cmdbatch *cmdbatch,
		unsigned int bit)
{
	if (bit >= KGSL_MAX_SYNCPOINTS)
		return false;

	return test_bit(bit, &cmdbatch->pending);
}

#endif /* __KGSL_CMDBATCH_H */

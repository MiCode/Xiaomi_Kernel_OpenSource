/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __KGSL_DRAWOBJ_H
#define __KGSL_DRAWOBJ_H

#include <linux/dma-fence.h>
#include <linux/irq_work.h>
#include <linux/kref.h>

#define DRAWOBJ(obj) (&obj->base)
#define SYNCOBJ(obj) \
	container_of(obj, struct kgsl_drawobj_sync, base)
#define CMDOBJ(obj) \
	container_of(obj, struct kgsl_drawobj_cmd, base)

#define CMDOBJ_TYPE     BIT(0)
#define MARKEROBJ_TYPE  BIT(1)
#define SYNCOBJ_TYPE    BIT(2)
#define TIMELINEOBJ_TYPE    BIT(4)

/**
 * struct kgsl_drawobj - KGSL drawobj descriptor
 * @device: KGSL GPU device that the command was created for
 * @context: KGSL context that created the command
 * @type: Object type
 * @timestamp: Timestamp assigned to the command
 * @flags: flags
 * @refcount: kref structure to maintain the reference count
 */
struct kgsl_drawobj {
	struct kgsl_device *device;
	struct kgsl_context *context;
	uint32_t type;
	uint32_t timestamp;
	unsigned long flags;
	struct kref refcount;
	/** @destroy: Callbak function to take down the object */
	void (*destroy)(struct kgsl_drawobj *drawobj);
	/** @destroy_object: Callback function to free the object memory */
	void (*destroy_object)(struct kgsl_drawobj *drawobj);
};

/**
 * struct kgsl_drawobj_cmd - KGSL command obj, This covers marker
 * cmds also since markers are special form of cmds that do not
 * need their cmds to be executed.
 * @base: Base kgsl_drawobj, this needs to be the first entry
 * @priv: Internal flags
 * @global_ts: The ringbuffer timestamp corresponding to this
 *    command obj
 * @fault_policy: Internal policy describing how to handle this command in case
 * of a fault
 * @fault_recovery: recovery actions actually tried for this batch
 *     be hung
 * @refcount: kref structure to maintain the reference count
 * @cmdlist: List of IBs to issue
 * @memlist: List of all memory used in this command batch
 * @marker_timestamp: For markers, the timestamp of the last "real" command that
 * was queued
 * @profiling_buf_entry: Mem entry containing the profiling buffer
 * @profiling_buffer_gpuaddr: GPU virt address of the profile buffer added here
 * for easy access
 * @profile_index: Index to store the start/stop ticks in the kernel profiling
 * buffer
 * @submit_ticks: Variable to hold ticks at the time of
 *     command obj submit.

 */
struct kgsl_drawobj_cmd {
	struct kgsl_drawobj base;
	unsigned long priv;
	unsigned int global_ts;
	unsigned long fault_policy;
	unsigned long fault_recovery;
	struct list_head cmdlist;
	struct list_head memlist;
	unsigned int marker_timestamp;
	struct kgsl_mem_entry *profiling_buf_entry;
	uint64_t profiling_buffer_gpuaddr;
	unsigned int profile_index;
	uint64_t submit_ticks;
};

/**
 * struct kgsl_drawobj_sync - KGSL sync object
 * @base: Base kgsl_drawobj, this needs to be the first entry
 * @synclist: Array of context/timestamp tuples to wait for before issuing
 * @numsyncs: Number of sync entries in the array
 * @pending: Bitmask of sync events that are active
 * @timer: a timer used to track possible sync timeouts for this
 *         sync obj
 * @timeout_jiffies: For a sync obj the jiffies at
 * which the timer will expire
 */
struct kgsl_drawobj_sync {
	struct kgsl_drawobj base;
	struct kgsl_drawobj_sync_event *synclist;
	unsigned int numsyncs;
	unsigned long pending;
	struct timer_list timer;
	unsigned long timeout_jiffies;
};

/**
 * struct kgsl_drawobj_timeline - KGSL timeline signal operation
 */
struct kgsl_drawobj_timeline {
	/** @base: &struct kgsl_drawobj container */
	struct kgsl_drawobj base;
	struct {
		/** @timeline: Pointer to a &struct kgsl_timeline */
		struct kgsl_timeline *timeline;
		/** @seqno: Sequence number to signal */
		u64 seqno;
	} *timelines;
	/** @count: Number of items in timelines */
	int count;
};

static inline struct kgsl_drawobj_timeline *
TIMELINEOBJ(struct kgsl_drawobj *obj)
{
	return container_of(obj, struct kgsl_drawobj_timeline, base);
}

#define KGSL_FENCE_NAME_LEN 74

struct fence_info {
	char name[KGSL_FENCE_NAME_LEN];
};

struct event_fence_info {
	struct fence_info *fences;
	int num_fences;
};

struct event_timeline_info {
	u64 seqno;
	u32 timeline;
};

/**
 * struct kgsl_drawobj_sync_event
 * @id: identifer (positiion within the pending bitmap)
 * @type: Syncpoint type
 * @syncobj: Pointer to the syncobj that owns the sync event
 * @context: KGSL context for whose timestamp we want to
 *           register this event
 * @timestamp: Pending timestamp for the event
 * @handle: Pointer to a sync fence handle
 * @device: Pointer to the KGSL device
 */
struct kgsl_drawobj_sync_event {
	unsigned int id;
	int type;
	struct kgsl_drawobj_sync *syncobj;
	struct kgsl_context *context;
	unsigned int timestamp;
	struct kgsl_sync_fence_cb *handle;
	struct kgsl_device *device;
	/** @priv: Type specific private information */
	void *priv;
	/**
	 * @fence: Pointer to a dma fence for KGSL_CMD_SYNCPOINT_TYPE_TIMELINE
	 * events
	 */
	struct dma_fence *fence;
	/** @cb: Callback struct for KGSL_CMD_SYNCPOINT_TYPE_TIMELINE */
	struct dma_fence_cb cb;
	/** @work : irq worker for KGSL_CMD_SYNCPOINT_TYPE_TIMELINE */
	struct irq_work work;
};

#define KGSL_DRAWOBJ_FLAGS \
	{ KGSL_DRAWOBJ_MARKER, "MARKER" }, \
	{ KGSL_DRAWOBJ_CTX_SWITCH, "CTX_SWITCH" }, \
	{ KGSL_DRAWOBJ_SYNC, "SYNC" }, \
	{ KGSL_DRAWOBJ_END_OF_FRAME, "EOF" }, \
	{ KGSL_DRAWOBJ_PWR_CONSTRAINT, "PWR_CONSTRAINT" }, \
	{ KGSL_DRAWOBJ_SUBMIT_IB_LIST, "IB_LIST" }

/**
 * enum kgsl_drawobj_cmd_priv - Internal command obj flags
 * @CMDOBJ_SKIP - skip the entire command obj
 * @CMDOBJ_FORCE_PREAMBLE - Force the preamble on for
 *           command obj
 * @CMDOBJ_WFI - Force wait-for-idle for the submission
 * @CMDOBJ_PROFILE - store the start / retire ticks for
 * @CMDOBJ_FAULT - Mark the command object as faulted
 * the command obj in the profiling buffer
 */
enum kgsl_drawobj_cmd_priv {
	CMDOBJ_SKIP = 0,
	CMDOBJ_FORCE_PREAMBLE,
	CMDOBJ_WFI,
	CMDOBJ_PROFILE,
	CMDOBJ_FAULT,
};

struct kgsl_ibdesc;
struct kgsl_cmd_syncpoint;

struct kgsl_drawobj_cmd *kgsl_drawobj_cmd_create(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int flags,
		unsigned int type);
int kgsl_drawobj_cmd_add_ibdesc(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj, struct kgsl_ibdesc *ibdesc);
int kgsl_drawobj_cmd_add_ibdesc_list(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj, void __user *ptr, int count);
int kgsl_drawobj_cmd_add_cmdlist(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj, void __user *ptr,
		unsigned int size, unsigned int count);
int kgsl_drawobj_cmd_add_memlist(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj, void __user *ptr,
		unsigned int size, unsigned int count);

struct kgsl_drawobj_sync *kgsl_drawobj_sync_create(struct kgsl_device *device,
		struct kgsl_context *context);
int kgsl_drawobj_sync_add_syncpoints(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj, void __user *ptr,
		int count);
int kgsl_drawobj_sync_add_synclist(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj, void __user *ptr,
		unsigned int size, unsigned int count);
int kgsl_drawobj_sync_add_sync(struct kgsl_device *device,
		struct kgsl_drawobj_sync *syncobj,
		struct kgsl_cmd_syncpoint *sync);

int kgsl_drawobjs_cache_init(void);
void kgsl_drawobjs_cache_exit(void);

void kgsl_dump_syncpoints(struct kgsl_device *device,
	struct kgsl_drawobj_sync *syncobj);

void kgsl_drawobj_destroy(struct kgsl_drawobj *drawobj);

void kgsl_drawobj_destroy_object(struct kref *kref);

static inline bool kgsl_drawobj_events_pending(
		struct kgsl_drawobj_sync *syncobj)
{
	return !bitmap_empty(&syncobj->pending, KGSL_MAX_SYNCPOINTS);
}

static inline bool kgsl_drawobj_event_pending(
		struct kgsl_drawobj_sync *syncobj, unsigned int bit)
{
	if (bit >= KGSL_MAX_SYNCPOINTS)
		return false;

	return test_bit(bit, &syncobj->pending);
}

static inline void kgsl_drawobj_put(struct kgsl_drawobj *drawobj)
{
	if (drawobj)
		kref_put(&drawobj->refcount, kgsl_drawobj_destroy_object);
}

/**
 * kgsl_drawobj_create_timestamp_syncobj - Create a syncobj for a timestamp
 * @device: A GPU device handle
 * @context: Draw context for the syncobj
 * @timestamp: Timestamp to sync on
 *
 * Create a sync object for @timestamp on @context.
 * Return: A pointer to the sync object
 */
struct kgsl_drawobj_sync *
kgsl_drawobj_create_timestamp_syncobj(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp);


/**
 * kgsl_drawobj_timeline_create - Create a timeline draw object
 * @device: A GPU device handle
 * @context: Draw context for the drawobj
 *
 * Create a timeline draw object on @context.
 * Return: A pointer to the draw object
 */
struct kgsl_drawobj_timeline *
kgsl_drawobj_timeline_create(struct kgsl_device *device,
		struct kgsl_context *context);

/**
 * kgsl_drwobj_add_timeline - Add a timeline to a timeline drawobj
 * @dev_priv: Pointer to the process private data
 * @timelineobj: Pointer to a timeline drawobject
 * @src: Ponter to the &struct kgsl_timeline_val from userspace
 * @cmdsize: size of the object in @src
 *
 * Add a timeline to an draw object.
 * Return: 0 on success or negative on failure
 */
int kgsl_drawobj_add_timeline(struct kgsl_device_private *dev_priv,
		struct kgsl_drawobj_timeline *timelineobj,
		void __user *src, u64 cmdsize);

#endif /* __KGSL_DRAWOBJ_H */

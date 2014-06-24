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
#ifndef __KGSL_DEVICE_H
#define __KGSL_DEVICE_H

#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include "kgsl.h"
#include "kgsl_mmu.h"
#include "kgsl_pwrctrl.h"
#include "kgsl_log.h"
#include "kgsl_pwrscale.h"
#include "kgsl_snapshot.h"

#include <linux/sync.h>

#define KGSL_TIMEOUT_NONE           0
#define KGSL_TIMEOUT_DEFAULT        0xFFFFFFFF
#define KGSL_TIMEOUT_PART           50 /* 50 msec */

#define FIRST_TIMEOUT (HZ / 2)

#define KGSL_IOCTL_FUNC(_cmd, _func) \
	[_IOC_NR((_cmd))] = \
		{ .cmd = (_cmd), .func = (_func) }

/* KGSL device state is initialized to INIT when platform_probe		*
 * sucessfully initialized the device.  Once a device has been opened	*
 * (started) it becomes active.  NAP implies that only low latency	*
 * resources (for now clocks on some platforms) are off.  SLEEP implies	*
 * that the KGSL module believes a device is idle (has been inactive	*
 * past its timer) and all system resources are released.  SUSPEND is	*
 * requested by the kernel and will be enforced upon all open devices.	*/

#define KGSL_STATE_NONE		0x00000000
#define KGSL_STATE_INIT		0x00000001
#define KGSL_STATE_ACTIVE	0x00000002
#define KGSL_STATE_NAP		0x00000004
#define KGSL_STATE_SLEEP	0x00000008
#define KGSL_STATE_SUSPEND	0x00000010
#define KGSL_STATE_HUNG		0x00000020
#define KGSL_STATE_SLUMBER	0x00000080

#define KGSL_GRAPHICS_MEMORY_LOW_WATERMARK  0x1000000

#define KGSL_IS_PAGE_ALIGNED(addr) (!((addr) & (~PAGE_MASK)))

/**
 * enum kgsl_event_results - result codes passed to an event callback when the
 * event is retired or cancelled
 * @KGSL_EVENT_RETIRED: The timestamp associated with the event retired
 * successflly
 * @KGSL_EVENT_CANCELLED: The event was cancelled before the event was fired
 */
enum kgsl_event_results {
	KGSL_EVENT_RETIRED = 1,
	KGSL_EVENT_CANCELLED = 2,
};

#define KGSL_FLAG_WAKE_ON_TOUCH BIT(0)

/*
 * "list" of event types for ftrace symbolic magic
 */

#define KGSL_EVENT_TYPES \
	{ KGSL_EVENT_RETIRED, "retired" }, \
	{ KGSL_EVENT_CANCELLED, "cancelled" }

#define KGSL_CONTEXT_FLAGS \
	{ KGSL_CONTEXT_NO_GMEM_ALLOC , "NO_GMEM_ALLOC" }, \
	{ KGSL_CONTEXT_PREAMBLE, "PREAMBLE" }, \
	{ KGSL_CONTEXT_TRASH_STATE, "TRASH_STATE" }, \
	{ KGSL_CONTEXT_CTX_SWITCH, "CTX_SWITCH" }, \
	{ KGSL_CONTEXT_PER_CONTEXT_TS, "PER_CONTEXT_TS" }, \
	{ KGSL_CONTEXT_USER_GENERATED_TS, "USER_TS" }, \
	{ KGSL_CONTEXT_NO_FAULT_TOLERANCE, "NO_FT" }, \
	{ KGSL_CONTEXT_PWR_CONSTRAINT, "PWR" }, \
	{ KGSL_CONTEXT_SAVE_GMEM, "SAVE_GMEM" }

#define KGSL_CMDBATCH_FLAGS \
	{ KGSL_CMDBATCH_MARKER, "MARKER" }, \
	{ KGSL_CMDBATCH_CTX_SWITCH, "CTX_SWITCH" }, \
	{ KGSL_CMDBATCH_SYNC, "SYNC" }, \
	{ KGSL_CMDBATCH_END_OF_FRAME, "EOF" }, \
	{ KGSL_CMDBATCH_PWR_CONSTRAINT, "PWR_CONSTRAINT" }, \
	{ KGSL_CMDBATCH_SUBMIT_IB_LIST, "IB_LIST" }

#define KGSL_CONTEXT_TYPES \
	{ KGSL_CONTEXT_TYPE_ANY, "ANY" }, \
	{ KGSL_CONTEXT_TYPE_GL, "GL" }, \
	{ KGSL_CONTEXT_TYPE_CL, "CL" }, \
	{ KGSL_CONTEXT_TYPE_C2D, "C2D" }, \
	{ KGSL_CONTEXT_TYPE_RS, "RS" }

#define KGSL_CONTEXT_ID(_context) \
	((_context != NULL) ? (_context)->id : KGSL_MEMSTORE_GLOBAL)

/* Allocate 512K for the snapshot static region*/
#define KGSL_SNAPSHOT_MEMSIZE (512 * 1024)

struct kgsl_device;
struct platform_device;
struct kgsl_device_private;
struct kgsl_context;
struct kgsl_power_stats;
struct kgsl_event;
struct kgsl_cmdbatch;
struct kgsl_snapshot;

struct kgsl_functable {
	/* Mandatory functions - these functions must be implemented
	   by the client device.  The driver will not check for a NULL
	   pointer before calling the hook.
	 */
	void (*regread) (struct kgsl_device *device,
		unsigned int offsetwords, unsigned int *value);
	void (*regwrite) (struct kgsl_device *device,
		unsigned int offsetwords, unsigned int value);
	int (*idle) (struct kgsl_device *device);
	bool (*isidle) (struct kgsl_device *device);
	int (*suspend_context) (struct kgsl_device *device);
	int (*init) (struct kgsl_device *device);
	int (*start) (struct kgsl_device *device, int priority);
	int (*stop) (struct kgsl_device *device);
	int (*getproperty) (struct kgsl_device *device,
		enum kgsl_property_type type, void __user *value,
		size_t sizebytes);
	int (*getproperty_compat) (struct kgsl_device *device,
		enum kgsl_property_type type, void __user *value,
		size_t sizebytes);
	int (*waittimestamp) (struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp,
		unsigned int msecs);
	int (*readtimestamp) (struct kgsl_device *device, void *priv,
		enum kgsl_timestamp_type type, unsigned int *timestamp);
	int (*issueibcmds) (struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_cmdbatch *cmdbatch,
		uint32_t *timestamps);
	void (*power_stats)(struct kgsl_device *device,
		struct kgsl_power_stats *stats);
	unsigned int (*gpuid)(struct kgsl_device *device, unsigned int *chipid);
	void (*snapshot)(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot, struct kgsl_context *context);
	irqreturn_t (*irq_handler)(struct kgsl_device *device);
	int (*drain)(struct kgsl_device *device);
	/* Optional functions - these functions are not mandatory.  The
	   driver will check that the function pointer is not NULL before
	   calling the hook */
	int (*setstate) (struct kgsl_device *device, unsigned int context_id,
			uint32_t flags);
	struct kgsl_context *(*drawctxt_create) (struct kgsl_device_private *,
						uint32_t *flags);
	int (*drawctxt_detach) (struct kgsl_context *context);
	void (*drawctxt_destroy) (struct kgsl_context *context);
	long (*ioctl) (struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);
	long (*compat_ioctl) (struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);
	int (*setproperty) (struct kgsl_device_private *dev_priv,
		enum kgsl_property_type type, void __user *value,
		unsigned int sizebytes);
	int (*setproperty_compat) (struct kgsl_device_private *dev_priv,
		enum kgsl_property_type type, void __user *value,
		unsigned int sizebytes);
	void (*drawctxt_sched)(struct kgsl_device *device,
		struct kgsl_context *context);
	void (*resume)(struct kgsl_device *device);
	void (*regulator_enable)(struct kgsl_device *);
	bool (*is_hw_collapsible)(struct kgsl_device *);
	void (*regulator_disable)(struct kgsl_device *);
};

typedef long (*kgsl_ioctl_func_t)(struct kgsl_device_private *,
	unsigned int, void *);

struct kgsl_ioctl {
	unsigned int cmd;
	kgsl_ioctl_func_t func;
};

long kgsl_ioctl_helper(struct file *filep, unsigned int cmd,
			const struct kgsl_ioctl *ioctl_funcs,
			unsigned int array_size, unsigned long arg);

/* Flag to mark the memobj_node as a preamble */
#define MEMOBJ_PREAMBLE BIT(0)
/* Flag to mark that the memobj_node should not go to the hadrware */
#define MEMOBJ_SKIP BIT(1)

/**
 * struct kgsl_memobj_node - Memory object descriptor
 * @node: Local list node for the cmdbatch
 * @cmdbatch: Cmdbatch the node belongs to
 * @addr: memory start address
 * @sizedwords: size of memory @addr
 * @flags: any special case flags
 */
struct kgsl_memobj_node {
	struct list_head node;
	unsigned long gpuaddr;
	size_t sizedwords;
	unsigned long priv;
};

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
 * @synclist: List of context/timestamp tuples to wait for before issuing
 * @timer: a timer used to track possible sync timeouts for this cmdbatch
 * @marker_timestamp: For markers, the timestamp of the last "real" command that
 * was queued
 *
 * This struture defines an atomic batch of command buffers issued from
 * userspace.
 */
struct kgsl_cmdbatch {
	struct kgsl_device *device;
	struct kgsl_context *context;
	spinlock_t lock;
	uint32_t timestamp;
	uint32_t flags;
	unsigned long priv;
	unsigned long fault_policy;
	unsigned long fault_recovery;
	unsigned long expires;
	struct kref refcount;
	struct list_head cmdlist;
	struct list_head memlist;
	struct list_head synclist;
	struct timer_list timer;
	unsigned int marker_timestamp;
};

/**
 * struct kgsl_cmdbatch_sync_event
 * @type: Syncpoint type
 * @node: Local list node for the cmdbatch sync point list
 * @cmdbatch: Pointer to the cmdbatch that owns the sync event
 * @context: Pointer to the KGSL context that owns the cmdbatch
 * @timestamp: Pending timestamp for the event
 * @handle: Pointer to a sync fence handle
 * @device: Pointer to the KGSL device
 * @refcount: Allow event to be destroyed asynchronously
 */
struct kgsl_cmdbatch_sync_event {
	int type;
	struct list_head node;
	struct kgsl_cmdbatch *cmdbatch;
	struct kgsl_context *context;
	unsigned int timestamp;
	struct kgsl_sync_fence_waiter *handle;
	struct kgsl_device *device;
	struct kref refcount;
};

/**
 * enum kgsl_cmdbatch_priv - Internal cmdbatch flags
 * @CMDBATCH_FLAG_SKIP - skip the entire command batch
 * @CMDBATCH_FLAG_FORCE_PREAMBLE - Force the preamble on for the cmdbatch
 * @CMDBATCH_FLAG_WFI - Force wait-for-idle for the submission
 */

enum kgsl_cmdbatch_priv {
	CMDBATCH_FLAG_SKIP = 0,
	CMDBATCH_FLAG_FORCE_PREAMBLE,
	CMDBATCH_FLAG_WFI,
};

struct kgsl_device {
	struct device *dev;
	const char *name;
	unsigned int ver_major;
	unsigned int ver_minor;
	uint32_t flags;
	enum kgsl_deviceid id;

	/* Starting physical address for GPU registers */
	unsigned long reg_phys;

	/* Starting Kernel virtual address for GPU registers */
	void *reg_virt;

	/* Total memory size for all GPU registers */
	unsigned int reg_len;

	/* Kernel virtual address for GPU shader memory */
	void *shader_mem_virt;

	/* Starting physical address for GPU shader memory */
	unsigned long shader_mem_phys;

	/* GPU shader memory size */
	unsigned int shader_mem_len;
	struct kgsl_memdesc memstore;
	const char *iomemname;
	const char *shadermemname;

	struct kgsl_mmu mmu;
	struct completion hwaccess_gate;
	struct completion cmdbatch_gate;
	const struct kgsl_functable *ftbl;
	struct work_struct idle_check_ws;
	struct timer_list idle_timer;
	struct kgsl_pwrctrl pwrctrl;
	int open_count;

	struct mutex mutex;
	uint32_t state;
	uint32_t requested_state;

	atomic_t active_cnt;

	wait_queue_head_t wait_queue;
	wait_queue_head_t active_cnt_wq;
	struct workqueue_struct *work_queue;
	struct platform_device *pdev;
	struct dentry *d_debugfs;
	struct idr context_idr;
	rwlock_t context_lock;

	struct {
		void *ptr;
		size_t size;
	} snapshot_memory;

	struct kgsl_snapshot *snapshot;

	u32 snapshot_faultcount;	/* Total number of faults since boot */
	struct kobject snapshot_kobj;

	/* Logging levels */
	int cmd_log;
	int ctxt_log;
	int drv_log;
	int mem_log;
	int pwr_log;
	struct kgsl_pwrscale pwrscale;
	struct work_struct event_work;

	int reset_counter; /* Track how many GPU core resets have occured */
	int cff_dump_enable;
	struct workqueue_struct *events_wq;
};


#define KGSL_DEVICE_COMMON_INIT(_dev) \
	.hwaccess_gate = COMPLETION_INITIALIZER((_dev).hwaccess_gate),\
	.cmdbatch_gate = COMPLETION_INITIALIZER((_dev).cmdbatch_gate),\
	.idle_check_ws = __WORK_INITIALIZER((_dev).idle_check_ws,\
			kgsl_idle_check),\
	.event_work  = __WORK_INITIALIZER((_dev).event_work,\
			kgsl_process_events),\
	.context_idr = IDR_INIT((_dev).context_idr),\
	.wait_queue = __WAIT_QUEUE_HEAD_INITIALIZER((_dev).wait_queue),\
	.active_cnt_wq = __WAIT_QUEUE_HEAD_INITIALIZER((_dev).active_cnt_wq),\
	.mutex = __MUTEX_INITIALIZER((_dev).mutex),\
	.state = KGSL_STATE_INIT,\
	.ver_major = DRIVER_VERSION_MAJOR,\
	.ver_minor = DRIVER_VERSION_MINOR


/**
 * enum bits for struct kgsl_context.priv
 * @KGSL_CONTEXT_PRIV_DETACHED  - The context has been destroyed by userspace
 *	and is no longer using the gpu.
 * @KGSL_CONTEXT_PRIV_INVALID - The context has been destroyed by the kernel
 *	because it caused a GPU fault.
 * @KGSL_CONTEXT_PRIV_PAGEFAULT - The context has caused a page fault.
 * @KGSL_CONTEXT_PRIV_DEVICE_SPECIFIC - this value and higher values are
 *	reserved for devices specific use.
 */
enum kgsl_context_priv {
	KGSL_CONTEXT_PRIV_DETACHED = 0,
	KGSL_CONTEXT_PRIV_INVALID,
	KGSL_CONTEXT_PRIV_PAGEFAULT,
	KGSL_CONTEXT_PRIV_DEVICE_SPECIFIC = 16,
};

struct kgsl_process_private;

/**
 * struct kgsl_context - The context fields that are valid for a user defined
 * context
 * @refcount: kref object for reference counting the context
 * @id: integer identifier for the context
 * @priority; The context's priority to submit commands to GPU
 * @tid: task that created this context.
 * @dev_priv: pointer to the owning device instance
 * @proc_priv: pointer to process private, the process that allocated the
 * context
 * @priv: in-kernel context flags, use KGSL_CONTEXT_* values
 * @reset_status: status indication whether a gpu reset occured and whether
 * this context was responsible for causing it
 * @wait_on_invalid_ts: flag indicating if this context has tried to wait on a
 * bad timestamp
 * @timeline: sync timeline used to create fences that can be signaled when a
 * sync_pt timestamp expires
 * @events: A kgsl_event_group for this context - contains the list of GPU
 * events
 * @pagefault_ts: global timestamp of the pagefault, if KGSL_CONTEXT_PAGEFAULT
 * is set.
 * @flags: flags from userspace controlling the behavior of this context
 * @pwr_constraint: power constraint from userspace for this context
 * @fault_count: number of times gpu hanged in last _context_throttle_time ms
 * @fault_time: time of the first gpu hang in last _context_throttle_time ms
 */
struct kgsl_context {
	struct kref refcount;
	uint32_t id;
	uint32_t priority;
	pid_t tid;
	struct kgsl_device_private *dev_priv;
	struct kgsl_process_private *proc_priv;
	unsigned long priv;
	struct kgsl_device *device;
	unsigned int reset_status;
	bool wait_on_invalid_ts;
	struct sync_timeline *timeline;
	struct kgsl_event_group events;
	unsigned int pagefault_ts;
	unsigned int flags;
	struct kgsl_pwr_constraint pwr_constraint;
	unsigned int fault_count;
	unsigned long fault_time;
};

/**
 * struct kgsl_process_private -  Private structure for a KGSL process (across
 * all devices)
 * @priv: Internal flags, use KGSL_PROCESS_* values
 * @pid: ID for the task owner of the process
 * @comm: task name of the process
 * @mem_lock: Spinlock to protect the process memory lists
 * @refcount: kref object for reference counting the process
 * @process_private_mutex: Mutex to synchronize access to the process struct
 * @mem_rb: RB tree node for the memory owned by this process
 * @idr: Iterator for assigning IDs to memory allocations
 * @pagetable: Pointer to the pagetable owned by this process
 * @kobj: Pointer to a kobj for the sysfs directory for this process
 * @debug_root: Pointer to the debugfs root for this process
 * @stats: Memory allocation statistics for this process
 * @syncsource_idr: sync sources created by this process
 */
struct kgsl_process_private {
	unsigned long priv;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	spinlock_t mem_lock;

	/* General refcount for process private struct obj */
	struct kref refcount;
	/* Mutex to synchronize access to each process_private struct obj */
	struct mutex process_private_mutex;

	struct rb_root mem_rb;
	struct idr mem_idr;
	struct kgsl_pagetable *pagetable;
	struct list_head list;
	struct kobject kobj;
	struct dentry *debug_root;

	struct {
		unsigned int cur;
		unsigned int max;
	} stats[KGSL_MEM_ENTRY_MAX];

	struct idr syncsource_idr;
};

/**
 * enum kgsl_process_priv_flags - Private flags for kgsl_process_private
 * @KGSL_PROCESS_INIT: Set if the process structure has been set up
 */
enum kgsl_process_priv_flags {
	KGSL_PROCESS_INIT = 0,
};

struct kgsl_device_private {
	struct kgsl_device *device;
	struct kgsl_process_private *process_priv;
};

/**
 * struct kgsl_snapshot - details for a specific snapshot instance
 * @start: Pointer to the start of the static snapshot region
 * @size: Size of the current snapshot instance
 * @ptr: Pointer to the next block of memory to write to during snapshotting
 * @remain: Bytes left in the snapshot region
 * @timestamp: Timestamp of the snapshot instance (in seconds since boot)
 * @mempool: Pointer to the memory pool for storing memory objects
 * @mempool_size: Size of the memory pool
 * @obj_list: List of frozen GPU buffers that are waiting to be dumped.
 * @cp_list: List of IB's to be dumped.
 * @work: worker to dump the frozen memory
 * @dump_gate: completion gate signaled by worker when it is finished.
 * @process: the process that caused the hang, if known.
 */
struct kgsl_snapshot {
	u8 *start;
	size_t size;
	u8 *ptr;
	size_t remain;
	unsigned long timestamp;
	u8 *mempool;
	size_t mempool_size;
	struct list_head obj_list;
	struct list_head cp_list;
	struct work_struct work;
	struct completion dump_gate;
	struct kgsl_process_private *process;
};

/**
 * struct kgsl_snapshot_object  - GPU memory in the snapshot
 * @gpuaddr: The GPU address identified during snapshot
 * @size: The buffer size identified during snapshot
 * @offset: offset from start of the allocated kgsl_mem_entry
 * @type: SNAPSHOT_OBJ_TYPE_* identifier.
 * @entry: the reference counted memory entry for this buffer
 * @node: node for kgsl_snapshot.obj_list
 */
struct kgsl_snapshot_object {
	unsigned int gpuaddr;
	unsigned int size;
	unsigned int offset;
	int type;
	struct kgsl_mem_entry *entry;
	struct list_head node;
};

/**
 * struct kgsl_protected_registers - Protected register range
 * @base: Offset of the range to be protected
 * @range: Range (# of registers = 2 ** range)
 */
struct kgsl_protected_registers {
	unsigned int base;
	int range;
};

struct kgsl_device *kgsl_get_device(int dev_idx);

static inline void kgsl_process_add_stats(struct kgsl_process_private *priv,
	unsigned int type, size_t size)
{
	priv->stats[type].cur += size;
	if (priv->stats[type].max < priv->stats[type].cur)
		priv->stats[type].max = priv->stats[type].cur;
}

static inline void kgsl_regread(struct kgsl_device *device,
				unsigned int offsetwords,
				unsigned int *value)
{
	device->ftbl->regread(device, offsetwords, value);
}

static inline void kgsl_regwrite(struct kgsl_device *device,
				 unsigned int offsetwords,
				 unsigned int value)
{
	device->ftbl->regwrite(device, offsetwords, value);
}

static inline int kgsl_idle(struct kgsl_device *device)
{
	return device->ftbl->idle(device);
}

static inline unsigned int kgsl_gpuid(struct kgsl_device *device,
	unsigned int *chipid)
{
	return device->ftbl->gpuid(device, chipid);
}

static inline int kgsl_create_device_sysfs_files(struct device *root,
	const struct device_attribute **list)
{
	int ret = 0, i;
	for (i = 0; list[i] != NULL; i++)
		ret |= device_create_file(root, list[i]);
	return ret;
}

static inline void kgsl_remove_device_sysfs_files(struct device *root,
	const struct device_attribute **list)
{
	int i;
	for (i = 0; list[i] != NULL; i++)
		device_remove_file(root, list[i]);
}

static inline struct kgsl_mmu *
kgsl_get_mmu(struct kgsl_device *device)
{
	return (struct kgsl_mmu *) (device ? &device->mmu : NULL);
}

static inline struct kgsl_device *kgsl_device_from_dev(struct device *dev)
{
	int i;

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		if (kgsl_driver.devp[i] && kgsl_driver.devp[i]->dev == dev)
			return kgsl_driver.devp[i];
	}

	return NULL;
}

static inline int kgsl_create_device_workqueue(struct kgsl_device *device)
{
	device->work_queue = create_singlethread_workqueue(device->name);
	if (!device->work_queue) {
		KGSL_DRV_ERR(device,
			     "create_singlethread_workqueue(%s) failed\n",
			     device->name);
		return -EINVAL;
	}
	return 0;
}

int kgsl_readtimestamp(struct kgsl_device *device, void *priv,
		enum kgsl_timestamp_type type, unsigned int *timestamp);

int kgsl_check_timestamp(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp);

int kgsl_device_platform_probe(struct kgsl_device *device);

void kgsl_device_platform_remove(struct kgsl_device *device);

const char *kgsl_pwrstate_to_str(unsigned int state);

int kgsl_device_snapshot_init(struct kgsl_device *device);
int kgsl_device_snapshot(struct kgsl_device *device,
			struct kgsl_context *context);
void kgsl_device_snapshot_close(struct kgsl_device *device);
void kgsl_snapshot_save_frozen_objs(struct work_struct *work);

void kgsl_events_init(void);
void kgsl_events_exit(void);

void kgsl_del_event_group(struct kgsl_event_group *group);

void kgsl_add_event_group(struct kgsl_event_group *group,
		struct kgsl_context *context, const char *name,
		readtimestamp_func readtimestamp, void *priv);

void kgsl_cancel_events_timestamp(struct kgsl_device *device,
		struct kgsl_event_group *group, unsigned int timestamp);
void kgsl_cancel_events(struct kgsl_device *device,
		struct kgsl_event_group *group);
void kgsl_cancel_event(struct kgsl_device *device,
		struct kgsl_event_group *group, unsigned int timestamp,
		kgsl_event_func func, void *priv);
int kgsl_add_event(struct kgsl_device *device, struct kgsl_event_group *group,
		unsigned int timestamp, kgsl_event_func func, void *priv);
void kgsl_process_event_group(struct kgsl_device *device,
	struct kgsl_event_group *group);

void kgsl_process_events(struct work_struct *work);

void kgsl_context_destroy(struct kref *kref);

int kgsl_context_init(struct kgsl_device_private *, struct kgsl_context
		*context);
int kgsl_context_detach(struct kgsl_context *context);

int kgsl_memfree_find_entry(pid_t pid, unsigned long *gpuaddr,
	unsigned long *size, unsigned int *flags);

/**
 * kgsl_context_put() - Release context reference count
 * @context: Pointer to the KGSL context to be released
 *
 * Reduce the reference count on a KGSL context and destroy it if it is no
 * longer needed
 */
static inline void
kgsl_context_put(struct kgsl_context *context)
{
	if (context)
		kref_put(&context->refcount, kgsl_context_destroy);
}

/**
 * kgsl_context_detached() - check if a context is detached
 * @context: the context
 *
 * Check if a context has been destroyed by userspace and is only waiting
 * for reference counts to go away. This check is used to weed out
 * contexts that shouldn't use the gpu so NULL is considered detached.
 */
static inline bool kgsl_context_detached(struct kgsl_context *context)
{
	return (context == NULL || test_bit(KGSL_CONTEXT_PRIV_DETACHED,
						&context->priv));
}

/**
 * kgsl_context_invalid() - check if a context is invalid
 * @context: the context
 *
 * Check if a context has been invalidated by the kernel and may no
 * longer use the GPU.
 */
static inline bool kgsl_context_invalid(struct kgsl_context *context)
{
	return (context == NULL || test_bit(KGSL_CONTEXT_PRIV_INVALID,
						&context->priv));
}


/**
 * kgsl_context_get() - get a pointer to a KGSL context
 * @device: Pointer to the KGSL device that owns the context
 * @id: Context ID
 *
 * Find the context associated with the given ID number, increase the reference
 * count on it and return it.  The caller must make sure that this call is
 * paired with a kgsl_context_put.  This function is for internal use because it
 * doesn't validate the ownership of the context with the calling process - use
 * kgsl_context_get_owner for that
 */
static inline struct kgsl_context *kgsl_context_get(struct kgsl_device *device,
		uint32_t id)
{
	int result = 0;
	struct kgsl_context *context = NULL;

	read_lock(&device->context_lock);

	context = idr_find(&device->context_idr, id);

	/* Don't return a context that has been detached */
	if (kgsl_context_detached(context))
		context = NULL;
	else
		result = kref_get_unless_zero(&context->refcount);

	read_unlock(&device->context_lock);

	if (!result)
		return NULL;
	return context;
}

/**
* _kgsl_context_get() - lightweight function to just increment the ref count
* @context: Pointer to the KGSL context
*
* Get a reference to the specified KGSL context structure. This is a
* lightweight way to just increase the refcount on a known context rather than
* walking through kgsl_context_get and searching the iterator
*/
static inline int _kgsl_context_get(struct kgsl_context *context)
{
	int ret = 0;

	if (context) {
		ret = kref_get_unless_zero(&context->refcount);
		/*
		 * We shouldn't realistically fail kref_get_unless_zero unless
		 * we did something really dumb so make the failure both public
		 * and painful
		 */

		WARN_ON(!ret);
	}

	return ret;
}

/**
 * kgsl_context_get_owner() - get a pointer to a KGSL context in a specific
 * process
 * @dev_priv: Pointer to the process struct
 * @id: Context ID to return
 *
 * Find the context associated with the given ID number, increase the reference
 * count on it and return it.  The caller must make sure that this call is
 * paired with a kgsl_context_put. This function validates that the context id
 * given is owned by the dev_priv instancet that is passed in.  See
 * kgsl_context_get for the internal version that doesn't do the check
 */
static inline struct kgsl_context *kgsl_context_get_owner(
		struct kgsl_device_private *dev_priv, uint32_t id)
{
	struct kgsl_context *context;

	context = kgsl_context_get(dev_priv->device, id);

	/* Verify that the context belongs to current calling fd. */
	if (context != NULL && context->dev_priv != dev_priv) {
		kgsl_context_put(context);
		return NULL;
	}

	return context;
}


void kgsl_cmdbatch_destroy(struct kgsl_cmdbatch *cmdbatch);

void kgsl_cmdbatch_destroy_object(struct kref *kref);

/**
* kgsl_process_private_get() - increment the refcount on a kgsl_process_private
*   struct
* @process: Pointer to the KGSL process_private
*
* Returns 0 if the structure is invalid and a reference count could not be
* obtained, nonzero otherwise.
*/
static inline int kgsl_process_private_get(struct kgsl_process_private *process)
{
	int ret = 0;
	if (process != NULL)
		ret = kref_get_unless_zero(&process->refcount);
	return ret;
}

void kgsl_process_private_put(struct kgsl_process_private *private);


struct kgsl_process_private *kgsl_process_private_find(pid_t pid);

/**
 * kgsl_cmdbatch_put() - Decrement the refcount for a command batch object
 * @cmdbatch: Pointer to the command batch object
 */
static inline void kgsl_cmdbatch_put(struct kgsl_cmdbatch *cmdbatch)
{
	if (cmdbatch)
		kref_put(&cmdbatch->refcount, kgsl_cmdbatch_destroy_object);
}

/**
 * kgsl_property_read_u32() - Read a u32 property from the device tree
 * @device: Pointer to the KGSL device
 * @prop: String name of the property to query
 * @ptr: Pointer to the variable to store the property
 */
static inline int kgsl_property_read_u32(struct kgsl_device *device,
	const char *prop, unsigned int *ptr)
{
	return of_property_read_u32(device->pdev->dev.of_node, prop, ptr);
}

/**
 * kgsl_sysfs_store() - parse a string from a sysfs store function
 * @buf: Incoming string to parse
 * @ptr: Pointer to an unsigned int to store the value
 */
static inline int kgsl_sysfs_store(const char *buf, unsigned int *ptr)
{
	unsigned int val;
	int rc;

	rc = kstrtou32(buf, 0, &val);
	if (rc)
		return rc;

	if (ptr)
		*ptr = val;

	return 0;
}

/*
 * A helper macro to print out "not enough memory functions" - this
 * makes it easy to standardize the messages as well as cut down on
 * the number of strings in the binary
 */
#define SNAPSHOT_ERR_NOMEM(_d, _s) \
	KGSL_DRV_ERR((_d), \
	"snapshot: not enough snapshot memory for section %s\n", (_s))

/**
 * struct kgsl_snapshot_registers - list of registers to snapshot
 * @regs: Pointer to an array of register ranges
 * @count: Number of entries in the array
 */
struct kgsl_snapshot_registers {
	unsigned int *regs;
	int count;
	int dump;
	unsigned int *snap_addr;
};

/**
 * struct kgsl_snapshot_registers_list - list of register lists
 * @registers: Pointer to an array of register lists
 * @count: Number of entries in the array
 */
struct kgsl_snapshot_registers_list {
	struct kgsl_snapshot_registers *registers;
	int count;
};

size_t kgsl_snapshot_dump_regs(struct kgsl_device *device, u8 *snapshot,
	size_t remain, void *priv);

void kgsl_snapshot_indexed_registers(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot, unsigned int index,
	unsigned int data, unsigned int start, unsigned int count);

int kgsl_snapshot_get_object(struct kgsl_snapshot *snapshot,
	struct kgsl_process_private *process, unsigned int gpuaddr,
	unsigned int size, unsigned int type);

int kgsl_snapshot_have_object(struct kgsl_snapshot *snapshot,
	struct kgsl_process_private *process,
	unsigned int gpuaddr, unsigned int size);

struct adreno_ib_object_list;

int kgsl_snapshot_add_ib_obj_list(struct kgsl_snapshot *snapshot,
	struct adreno_ib_object_list *ib_obj_list);

void kgsl_snapshot_dump_skipped_regs(struct kgsl_device *device,
	struct kgsl_snapshot_registers_list *list);

void kgsl_snapshot_add_section(struct kgsl_device *device, u16 id,
	struct kgsl_snapshot *snapshot,
	size_t (*func)(struct kgsl_device *, u8 *, size_t, void *),
	void *priv);

#endif  /* __KGSL_DEVICE_H */

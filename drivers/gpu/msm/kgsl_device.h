/* Copyright (c) 2002,2007-2018, The Linux Foundation. All rights reserved.
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

#include "kgsl.h"
#include "kgsl_mmu.h"
#include "kgsl_pwrctrl.h"
#include "kgsl_log.h"
#include "kgsl_pwrscale.h"
#include "kgsl_snapshot.h"
#include "kgsl_sharedmem.h"
#include "kgsl_drawobj.h"
#include "kgsl_gmu_core.h"

#define KGSL_IOCTL_FUNC(_cmd, _func) \
	[_IOC_NR((_cmd))] = \
		{ .cmd = (_cmd), .func = (_func) }

/*
 * KGSL device state is initialized to INIT when platform_probe		*
 * successfully initialized the device.  Once a device has been opened	*
 * (started) it becomes active.  NAP implies that only low latency	*
 * resources (for now clocks on some platforms) are off.  SLEEP implies	*
 * that the KGSL module believes a device is idle (has been inactive	*
 * past its timer) and all system resources are released.  SUSPEND is	*
 * requested by the kernel and will be enforced upon all open devices.	*
 * RESET indicates that GPU or GMU hang happens. KGSL is handling	*
 * snapshot or recover GPU from hang.					*
 */

#define KGSL_STATE_NONE		0x00000000
#define KGSL_STATE_INIT		0x00000001
#define KGSL_STATE_ACTIVE	0x00000002
#define KGSL_STATE_NAP		0x00000004
#define KGSL_STATE_SUSPEND	0x00000010
#define KGSL_STATE_AWARE	0x00000020
#define KGSL_STATE_SLUMBER	0x00000080
#define KGSL_STATE_RESET	0x00000100

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
	{ KGSL_CONTEXT_NO_GMEM_ALLOC, "NO_GMEM_ALLOC" }, \
	{ KGSL_CONTEXT_PREAMBLE, "PREAMBLE" }, \
	{ KGSL_CONTEXT_TRASH_STATE, "TRASH_STATE" }, \
	{ KGSL_CONTEXT_CTX_SWITCH, "CTX_SWITCH" }, \
	{ KGSL_CONTEXT_PER_CONTEXT_TS, "PER_CONTEXT_TS" }, \
	{ KGSL_CONTEXT_USER_GENERATED_TS, "USER_TS" }, \
	{ KGSL_CONTEXT_NO_FAULT_TOLERANCE, "NO_FT" }, \
	{ KGSL_CONTEXT_INVALIDATE_ON_FAULT, "INVALIDATE_ON_FAULT" }, \
	{ KGSL_CONTEXT_PWR_CONSTRAINT, "PWR" }, \
	{ KGSL_CONTEXT_SAVE_GMEM, "SAVE_GMEM" }, \
	{ KGSL_CONTEXT_IFH_NOP, "IFH_NOP" }, \
	{ KGSL_CONTEXT_SECURE, "SECURE" }, \
	{ KGSL_CONTEXT_NO_SNAPSHOT, "NO_SNAPSHOT" }, \
	{ KGSL_CONTEXT_SPARSE, "SPARSE" }


#define KGSL_CONTEXT_TYPES \
	{ KGSL_CONTEXT_TYPE_ANY, "ANY" }, \
	{ KGSL_CONTEXT_TYPE_GL, "GL" }, \
	{ KGSL_CONTEXT_TYPE_CL, "CL" }, \
	{ KGSL_CONTEXT_TYPE_C2D, "C2D" }, \
	{ KGSL_CONTEXT_TYPE_RS, "RS" }, \
	{ KGSL_CONTEXT_TYPE_VK, "VK" }

#define KGSL_CONTEXT_ID(_context) \
	((_context != NULL) ? (_context)->id : KGSL_MEMSTORE_GLOBAL)

/* Allocate 600K for the snapshot static region*/
#define KGSL_SNAPSHOT_MEMSIZE (600 * 1024)
#define MAX_L3_LEVELS	3

struct kgsl_device;
struct platform_device;
struct kgsl_device_private;
struct kgsl_context;
struct kgsl_power_stats;
struct kgsl_event;
struct kgsl_snapshot;

struct kgsl_functable {
	/* Mandatory functions - these functions must be implemented
	 * by the client device.  The driver will not check for a NULL
	 * pointer before calling the hook.
	 */
	void (*regread)(struct kgsl_device *device,
		unsigned int offsetwords, unsigned int *value);
	void (*regwrite)(struct kgsl_device *device,
		unsigned int offsetwords, unsigned int value);
	int (*idle)(struct kgsl_device *device);
	bool (*isidle)(struct kgsl_device *device);
	int (*suspend_context)(struct kgsl_device *device);
	int (*init)(struct kgsl_device *device);
	int (*start)(struct kgsl_device *device, int priority);
	int (*stop)(struct kgsl_device *device);
	int (*getproperty)(struct kgsl_device *device,
		unsigned int type, void __user *value,
		size_t sizebytes);
	int (*getproperty_compat)(struct kgsl_device *device,
		unsigned int type, void __user *value,
		size_t sizebytes);
	int (*waittimestamp)(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp,
		unsigned int msecs);
	int (*readtimestamp)(struct kgsl_device *device, void *priv,
		enum kgsl_timestamp_type type, unsigned int *timestamp);
	int (*queue_cmds)(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
		uint32_t count, uint32_t *timestamp);
	void (*power_stats)(struct kgsl_device *device,
		struct kgsl_power_stats *stats);
	unsigned int (*gpuid)(struct kgsl_device *device, unsigned int *chipid);
	void (*snapshot)(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot, struct kgsl_context *context);
	irqreturn_t (*irq_handler)(struct kgsl_device *device);
	int (*drain)(struct kgsl_device *device);
	struct kgsl_device_private * (*device_private_create)(void);
	void (*device_private_destroy)(struct kgsl_device_private *dev_priv);
	/*
	 * Optional functions - these functions are not mandatory.  The
	 * driver will check that the function pointer is not NULL before
	 * calling the hook
	 */
	struct kgsl_context *(*drawctxt_create)(struct kgsl_device_private *,
						uint32_t *flags);
	void (*drawctxt_detach)(struct kgsl_context *context);
	void (*drawctxt_destroy)(struct kgsl_context *context);
	void (*drawctxt_dump)(struct kgsl_device *device,
		struct kgsl_context *context);
	long (*ioctl)(struct kgsl_device_private *dev_priv,
		unsigned int cmd, unsigned long arg);
	long (*compat_ioctl)(struct kgsl_device_private *dev_priv,
		unsigned int cmd, unsigned long arg);
	int (*setproperty)(struct kgsl_device_private *dev_priv,
		unsigned int type, void __user *value,
		unsigned int sizebytes);
	int (*setproperty_compat)(struct kgsl_device_private *dev_priv,
		unsigned int type, void __user *value,
		unsigned int sizebytes);
	void (*drawctxt_sched)(struct kgsl_device *device,
		struct kgsl_context *context);
	void (*resume)(struct kgsl_device *device);
	int (*regulator_enable)(struct kgsl_device *);
	bool (*is_hw_collapsible)(struct kgsl_device *);
	void (*regulator_disable)(struct kgsl_device *);
	void (*pwrlevel_change_settings)(struct kgsl_device *device,
		unsigned int prelevel, unsigned int postlevel, bool post);
	void (*regulator_disable_poll)(struct kgsl_device *device);
	void (*clk_set_options)(struct kgsl_device *device,
		const char *name, struct clk *clk, bool on);
	void (*gpu_model)(struct kgsl_device *device, char *str,
		size_t bufsz);
	void (*stop_fault_timer)(struct kgsl_device *device);
	void (*dispatcher_halt)(struct kgsl_device *device);
	void (*dispatcher_unhalt)(struct kgsl_device *device);
};

struct kgsl_ioctl {
	unsigned int cmd;
	long (*func)(struct kgsl_device_private *, unsigned int, void *);
};

long kgsl_ioctl_helper(struct file *filep, unsigned int cmd, unsigned long arg,
		const struct kgsl_ioctl *cmds, int len);

/* Flag to mark the memobj_node as a preamble */
#define MEMOBJ_PREAMBLE BIT(0)
/* Flag to mark that the memobj_node should not go to the hadrware */
#define MEMOBJ_SKIP BIT(1)

/**
 * struct kgsl_memobj_node - Memory object descriptor
 * @node: Local list node for the object
 * @id: GPU memory ID for the object
 * offset: Offset within the object
 * @gpuaddr: GPU address for the object
 * @flags: External flags passed by the user
 * @priv: Internal flags set by the driver
 */
struct kgsl_memobj_node {
	struct list_head node;
	unsigned int id;
	uint64_t offset;
	uint64_t gpuaddr;
	uint64_t size;
	unsigned long flags;
	unsigned long priv;
};

/**
 * struct kgsl_sparseobj_node - Sparse object descriptor
 * @node: Local list node for the sparse cmdbatch
 * @virt_id: Virtual ID to bind/unbind
 * @obj:  struct kgsl_sparse_binding_object
 */
struct kgsl_sparseobj_node {
	struct list_head node;
	unsigned int virt_id;
	struct kgsl_sparse_binding_object obj;
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
	void __iomem *reg_virt;

	/* Total memory size for all GPU registers */
	unsigned int reg_len;

	/* Kernel virtual address for GPU shader memory */
	void __iomem *shader_mem_virt;

	/* Starting physical address for GPU shader memory */
	unsigned long shader_mem_phys;

	/* GPU shader memory size */
	unsigned int shader_mem_len;
	struct kgsl_memdesc memstore;
	struct kgsl_memdesc scratch;
	const char *iomemname;
	const char *shadermemname;

	struct kgsl_mmu mmu;
	struct gmu_core_device gmu_core;
	struct completion hwaccess_gate;
	struct completion halt_gate;
	const struct kgsl_functable *ftbl;
	struct work_struct idle_check_ws;
	struct timer_list idle_timer;
	struct kgsl_pwrctrl pwrctrl;
	int open_count;

	/* For GPU inline submission */
	uint32_t submit_now;
	spinlock_t submit_lock;
	bool slumber;

	struct mutex mutex;
	uint32_t state;
	uint32_t requested_state;

	atomic_t active_cnt;

	wait_queue_head_t wait_queue;
	wait_queue_head_t active_cnt_wq;
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
	bool force_panic;		/* Force panic after snapshot dump */
	bool prioritize_unrecoverable;	/* Overwrite with new GMU snapshots */

	/* Use CP Crash dumper to get GPU snapshot*/
	bool snapshot_crashdumper;
	/* Use HOST side register reads to get GPU snapshot*/
	bool snapshot_legacy;

	struct kobject snapshot_kobj;

	struct kobject ppd_kobj;

	/* Logging levels */
	int cmd_log;
	int ctxt_log;
	int drv_log;
	int mem_log;
	int pwr_log;
	struct kgsl_pwrscale pwrscale;

	int reset_counter; /* Track how many GPU core resets have occurred */
	struct workqueue_struct *events_wq;

	struct device *busmondev; /* pseudo dev for GPU BW voting governor */

	/* Number of active contexts seen globally for this device */
	int active_context_count;
	struct kobject *gpu_sysfs_kobj;
	struct clk *l3_clk;
	unsigned int l3_freq[MAX_L3_LEVELS];
	unsigned int num_l3_pwrlevels;
	/* store current L3 vote to determine if we should change our vote */
	unsigned int cur_l3_pwrlevel;
};

#define KGSL_MMU_DEVICE(_mmu) \
	container_of((_mmu), struct kgsl_device, mmu)

#define KGSL_DEVICE_COMMON_INIT(_dev) \
	.hwaccess_gate = COMPLETION_INITIALIZER((_dev).hwaccess_gate),\
	.halt_gate = COMPLETION_INITIALIZER((_dev).halt_gate),\
	.idle_check_ws = __WORK_INITIALIZER((_dev).idle_check_ws,\
			kgsl_idle_check),\
	.context_idr = IDR_INIT,\
	.wait_queue = __WAIT_QUEUE_HEAD_INITIALIZER((_dev).wait_queue),\
	.active_cnt_wq = __WAIT_QUEUE_HEAD_INITIALIZER((_dev).active_cnt_wq),\
	.mutex = __MUTEX_INITIALIZER((_dev).mutex),\
	.state = KGSL_STATE_NONE,\
	.ver_major = DRIVER_VERSION_MAJOR,\
	.ver_minor = DRIVER_VERSION_MINOR


/**
 * enum bits for struct kgsl_context.priv
 * @KGSL_CONTEXT_PRIV_SUBMITTED - The context has submitted commands to gpu.
 * @KGSL_CONTEXT_PRIV_DETACHED  - The context has been destroyed by userspace
 *	and is no longer using the gpu.
 * @KGSL_CONTEXT_PRIV_INVALID - The context has been destroyed by the kernel
 *	because it caused a GPU fault.
 * @KGSL_CONTEXT_PRIV_PAGEFAULT - The context has caused a page fault.
 * @KGSL_CONTEXT_PRIV_DEVICE_SPECIFIC - this value and higher values are
 *	reserved for devices specific use.
 */
enum kgsl_context_priv {
	KGSL_CONTEXT_PRIV_SUBMITTED = 0,
	KGSL_CONTEXT_PRIV_DETACHED,
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
 * @reset_status: status indication whether a gpu reset occurred and whether
 * this context was responsible for causing it
 * @timeline: sync timeline used to create fences that can be signaled when a
 * sync_pt timestamp expires
 * @events: A kgsl_event_group for this context - contains the list of GPU
 * events
 * @flags: flags from userspace controlling the behavior of this context
 * @pwr_constraint: power constraint from userspace for this context
 * @fault_count: number of times gpu hanged in last _context_throttle_time ms
 * @fault_time: time of the first gpu hang in last _context_throttle_time ms
 * @user_ctxt_record: memory descriptor used by CP to save/restore VPC data
 * across preemption
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
	struct kgsl_sync_timeline *ktimeline;
	struct kgsl_event_group events;
	unsigned int flags;
	struct kgsl_pwr_constraint pwr_constraint;
	struct kgsl_pwr_constraint l3_pwr_constraint;
	unsigned int fault_count;
	unsigned long fault_time;
	struct kgsl_mem_entry *user_ctxt_record;
};

#define _context_comm(_c) \
	(((_c) && (_c)->proc_priv) ? (_c)->proc_priv->comm : "unknown")

/*
 * Print log messages with the context process name/pid:
 * [...] kgsl kgsl-3d0: kgsl-api-test[22182]:
 */

#define pr_context(_d, _c, fmt, args...) \
		dev_err((_d)->dev, "%s[%d]: " fmt, \
		_context_comm((_c)), \
		(_c)->proc_priv->pid, ##args)

/**
 * struct kgsl_process_private -  Private structure for a KGSL process (across
 * all devices)
 * @priv: Internal flags, use KGSL_PROCESS_* values
 * @pid: ID for the task owner of the process
 * @comm: task name of the process
 * @mem_lock: Spinlock to protect the process memory lists
 * @refcount: kref object for reference counting the process
 * @idr: Iterator for assigning IDs to memory allocations
 * @pagetable: Pointer to the pagetable owned by this process
 * @kobj: Pointer to a kobj for the sysfs directory for this process
 * @debug_root: Pointer to the debugfs root for this process
 * @stats: Memory allocation statistics for this process
 * @gpumem_mapped: KGSL memory mapped in the process address space
 * @syncsource_idr: sync sources created by this process
 * @syncsource_lock: Spinlock to protect the syncsource idr
 * @fd_count: Counter for the number of FDs for this process
 * @ctxt_count: Count for the number of contexts for this process
 * @ctxt_count_lock: Spinlock to protect ctxt_count
 */
struct kgsl_process_private {
	unsigned long priv;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	spinlock_t mem_lock;
	struct kref refcount;
	struct idr mem_idr;
	struct kgsl_pagetable *pagetable;
	struct list_head list;
	struct kobject kobj;
	struct dentry *debug_root;
	struct {
		uint64_t cur;
		uint64_t max;
	} stats[KGSL_MEM_ENTRY_MAX];
	uint64_t gpumem_mapped;
	struct idr syncsource_idr;
	spinlock_t syncsource_lock;
	int fd_count;
	atomic_t ctxt_count;
	spinlock_t ctxt_count_lock;
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
 * @ib1base: Active IB1 base address at the time of fault
 * @ib2base: Active IB2 base address at the time of fault
 * @ib1size: Number of DWORDS pending in IB1 at the time of fault
 * @ib2size: Number of DWORDS pending in IB2 at the time of fault
 * @ib1dumped: Active IB1 dump status to sansphot binary
 * @ib2dumped: Active IB2 dump status to sansphot binary
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
 * @sysfs_read: Count of current reads via sysfs
 * @first_read: True until the snapshot read is started
 * @gmu_fault: Snapshot collected when GMU fault happened
 * @recovered: True if GPU was recovered after previous snapshot
 */
struct kgsl_snapshot {
	uint64_t ib1base;
	uint64_t ib2base;
	unsigned int ib1size;
	unsigned int ib2size;
	bool ib1dumped;
	bool ib2dumped;
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
	unsigned int sysfs_read;
	bool first_read;
	bool gmu_fault;
	bool recovered;
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
	uint64_t gpuaddr;
	uint64_t size;
	uint64_t offset;
	int type;
	struct kgsl_mem_entry *entry;
	struct list_head node;
};

struct kgsl_device *kgsl_get_device(int dev_idx);

static inline void kgsl_process_add_stats(struct kgsl_process_private *priv,
	unsigned int type, uint64_t size)
{
	priv->stats[type].cur += size;
	if (priv->stats[type].max < priv->stats[type].cur)
		priv->stats[type].max = priv->stats[type].cur;
}

static inline bool kgsl_is_register_offset(struct kgsl_device *device,
				unsigned int offsetwords)
{
	return ((offsetwords * sizeof(uint32_t)) < device->reg_len);
}

static inline void kgsl_regread(struct kgsl_device *device,
				unsigned int offsetwords,
				unsigned int *value)
{
	if (kgsl_is_register_offset(device, offsetwords))
		device->ftbl->regread(device, offsetwords, value);
	else if (gmu_core_is_register_offset(device, offsetwords))
		gmu_core_regread(device, offsetwords, value);
	else {
		WARN(1, "Out of bounds register read: 0x%x\n", offsetwords);
		*value = 0;
	}
}

static inline void kgsl_regwrite(struct kgsl_device *device,
				 unsigned int offsetwords,
				 unsigned int value)
{
	if (kgsl_is_register_offset(device, offsetwords))
		device->ftbl->regwrite(device, offsetwords, value);
	else if (gmu_core_is_register_offset(device, offsetwords))
		gmu_core_regwrite(device, offsetwords, value);
	else
		WARN(1, "Out of bounds register write: 0x%x\n", offsetwords);
}

static inline void kgsl_regrmw(struct kgsl_device *device,
		unsigned int offsetwords,
		unsigned int mask, unsigned int bits)
{
	unsigned int val = 0;

	kgsl_regread(device, offsetwords, &val);
	val &= ~mask;
	kgsl_regwrite(device, offsetwords, val | bits);
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

static inline struct kgsl_device *kgsl_device_from_dev(struct device *dev)
{
	int i;

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		if (kgsl_driver.devp[i] && kgsl_driver.devp[i]->dev == dev)
			return kgsl_driver.devp[i];
	}

	return NULL;
}

static inline int kgsl_state_is_awake(struct kgsl_device *device)
{
	if (device->state == KGSL_STATE_ACTIVE ||
		device->state == KGSL_STATE_AWARE)
		return true;
	else if (gmu_core_isenabled(device) &&
			test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return true;
	else
		return false;
}

int kgsl_readtimestamp(struct kgsl_device *device, void *priv,
		enum kgsl_timestamp_type type, unsigned int *timestamp);

int kgsl_check_timestamp(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp);

int kgsl_device_platform_probe(struct kgsl_device *device);

void kgsl_device_platform_remove(struct kgsl_device *device);

const char *kgsl_pwrstate_to_str(unsigned int state);

int kgsl_device_snapshot_init(struct kgsl_device *device);
void kgsl_device_snapshot(struct kgsl_device *device,
			struct kgsl_context *context, bool gmu_fault);
void kgsl_device_snapshot_close(struct kgsl_device *device);

void kgsl_events_init(void);
void kgsl_events_exit(void);

void kgsl_context_detach(struct kgsl_context *context);

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
bool kgsl_event_pending(struct kgsl_device *device,
		struct kgsl_event_group *group, unsigned int timestamp,
		kgsl_event_func func, void *priv);
int kgsl_add_event(struct kgsl_device *device, struct kgsl_event_group *group,
		unsigned int timestamp, kgsl_event_func func, void *priv);
void kgsl_process_event_group(struct kgsl_device *device,
	struct kgsl_event_group *group);
void kgsl_flush_event_group(struct kgsl_device *device,
		struct kgsl_event_group *group);
void kgsl_process_event_groups(struct kgsl_device *device);

void kgsl_context_destroy(struct kref *kref);

int kgsl_context_init(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context);

void kgsl_context_dump(struct kgsl_context *context);

int kgsl_memfree_find_entry(pid_t ptname, uint64_t *gpuaddr,
	uint64_t *size, uint64_t *flags, pid_t *pid);

long kgsl_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

long kgsl_ioctl_copy_in(unsigned int kernel_cmd, unsigned int user_cmd,
		unsigned long arg, unsigned char *ptr);

long kgsl_ioctl_copy_out(unsigned int kernel_cmd, unsigned int user_cmd,
		unsigned long arg, unsigned char *ptr);

void kgsl_sparse_bind(struct kgsl_process_private *private,
		struct kgsl_drawobj_sparse *sparse);

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

	if (context)
		ret = kref_get_unless_zero(&context->refcount);

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

/**
 * kgsl_process_private_get() - increment the refcount on a
 * kgsl_process_private struct
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
	const unsigned int *regs;
	unsigned int count;
};

size_t kgsl_snapshot_dump_registers(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv);

void kgsl_snapshot_indexed_registers(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot, unsigned int index,
	unsigned int data, unsigned int start, unsigned int count);

int kgsl_snapshot_get_object(struct kgsl_snapshot *snapshot,
	struct kgsl_process_private *process, uint64_t gpuaddr,
	uint64_t size, unsigned int type);

int kgsl_snapshot_have_object(struct kgsl_snapshot *snapshot,
	struct kgsl_process_private *process,
	uint64_t gpuaddr, uint64_t size);

struct adreno_ib_object_list;

int kgsl_snapshot_add_ib_obj_list(struct kgsl_snapshot *snapshot,
	struct adreno_ib_object_list *ib_obj_list);

void kgsl_snapshot_add_section(struct kgsl_device *device, u16 id,
	struct kgsl_snapshot *snapshot,
	size_t (*func)(struct kgsl_device *, u8 *, size_t, void *),
	void *priv);

/**
 * struct kgsl_pwr_limit - limit structure for each client
 * @node: Local list node for the limits list
 * @level: requested power level
 * @device: pointer to the device structure
 */
struct kgsl_pwr_limit {
	struct list_head node;
	unsigned int level;
	struct kgsl_device *device;
};

#endif  /* __KGSL_DEVICE_H */

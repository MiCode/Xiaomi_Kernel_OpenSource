/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __KGSL_DEVICE_H
#define __KGSL_DEVICE_H

#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <trace/events/gpu_mem.h>

#include "kgsl.h"
#include "kgsl_drawobj.h"
#include "kgsl_mmu.h"
#include "kgsl_regmap.h"

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
 * snapshot or recover GPU from hang. MINBW implies that DDR BW vote is	*
 * set to non-zero minimum value.
 */

#define KGSL_STATE_NONE		0x00000000
#define KGSL_STATE_INIT		0x00000001
#define KGSL_STATE_ACTIVE	0x00000002
#define KGSL_STATE_NAP		0x00000004
#define KGSL_STATE_SUSPEND	0x00000010
#define KGSL_STATE_AWARE	0x00000020
#define KGSL_STATE_SLUMBER	0x00000080
#define KGSL_STATE_MINBW	0x00000100

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

/*
 * "list" of event types for ftrace symbolic magic
 */

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
	{ KGSL_CONTEXT_LPAC, "LPAC" }, \
	{ KGSL_CONTEXT_NO_SNAPSHOT, "NO_SNAPSHOT" }

#define KGSL_CONTEXT_ID(_context) \
	((_context != NULL) ? (_context)->id : KGSL_MEMSTORE_GLOBAL)

struct kgsl_device;
struct platform_device;
struct kgsl_device_private;
struct kgsl_context;
struct kgsl_power_stats;
struct kgsl_event;
struct kgsl_snapshot;
struct kgsl_sync_fence;

struct kgsl_functable {
	/* Mandatory functions - these functions must be implemented
	 * by the client device.  The driver will not check for a NULL
	 * pointer before calling the hook.
	 */
	int (*suspend_context)(struct kgsl_device *device);
	int (*first_open)(struct kgsl_device *device);
	int (*last_close)(struct kgsl_device *device);
	int (*start)(struct kgsl_device *device, int priority);
	int (*stop)(struct kgsl_device *device);
	int (*getproperty)(struct kgsl_device *device,
		struct kgsl_device_getproperty *param);
	int (*getproperty_compat)(struct kgsl_device *device,
		struct kgsl_device_getproperty *param);
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
	void (*snapshot)(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot, struct kgsl_context *context,
		struct kgsl_context *context_lpac);
	/** @drain_and_idle: Drain the GPU and wait for it to idle */
	int (*drain_and_idle)(struct kgsl_device *device);
	struct kgsl_device_private * (*device_private_create)(void);
	void (*device_private_destroy)(struct kgsl_device_private *dev_priv);
	/*
	 * Optional functions - these functions are not mandatory.  The
	 * driver will check that the function pointer is not NULL before
	 * calling the hook
	 */
	struct kgsl_context *(*drawctxt_create)
				(struct kgsl_device_private *dev_priv,
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
	int (*regulator_enable)(struct kgsl_device *device);
	bool (*is_hw_collapsible)(struct kgsl_device *device);
	void (*regulator_disable)(struct kgsl_device *device);
	void (*pwrlevel_change_settings)(struct kgsl_device *device,
		unsigned int prelevel, unsigned int postlevel, bool post);
	void (*clk_set_options)(struct kgsl_device *device,
		const char *name, struct clk *clk, bool on);
	/**
	 * @query_property_list: query the list of properties
	 * supported by the device. If 'list' is NULL just return the total
	 * number of properties available otherwise copy up to 'count' items
	 * into the list and return the total number of items copied.
	 */
	int (*query_property_list)(struct kgsl_device *device, u32 *list,
		u32 count);
	bool (*is_hwcg_on)(struct kgsl_device *device);
	/** @gpu_clock_set: Target specific function to set gpu frequency */
	int (*gpu_clock_set)(struct kgsl_device *device, u32 pwrlevel);
	/** @gpu_bus_set: Target specific function to set gpu bandwidth */
	int (*gpu_bus_set)(struct kgsl_device *device, int bus_level, u32 ab);
	void (*deassert_gbif_halt)(struct kgsl_device *device);
	/** @queue_recurring_cmd: Queue recurring commands to GMU */
	int (*queue_recurring_cmd)(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_drawobj *drawobj);
	/** @dequeue_recurring_cmd: Dequeue recurring commands from GMU */
	int (*dequeue_recurring_cmd)(struct kgsl_device *device,
		struct kgsl_context *context);
	/** @create_hw_fence: Create a hardware fence */
	void (*create_hw_fence)(struct kgsl_device *device, struct kgsl_sync_fence *kfence);
	/** @register_gdsc_notifier: Target specific function to register gdsc notifier */
	int (*register_gdsc_notifier)(struct kgsl_device *device);
};

struct kgsl_ioctl {
	unsigned int cmd;
	long (*func)(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data);
};

long kgsl_ioctl_helper(struct file *filep, unsigned int cmd, unsigned long arg,
		const struct kgsl_ioctl *cmds, int len);

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

struct kgsl_device {
	struct device *dev;
	const char *name;
	u32 id;

	/* Kernel virtual address for GPU shader memory */
	void __iomem *shader_mem_virt;

	/* Starting kernel virtual address for QDSS GFX DBG register block */
	void __iomem *qdss_gfx_virt;

	struct kgsl_memdesc *memstore;
	struct kgsl_memdesc *scratch;

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
	/** @total_mapped: To trace overall gpu memory usage */
	atomic64_t total_mapped;

	wait_queue_head_t active_cnt_wq;
	struct platform_device *pdev;
	struct dentry *d_debugfs;
	struct idr context_idr;
	rwlock_t context_lock;

	struct {
		void *ptr;
		dma_addr_t dma_handle;
		u32 size;
	} snapshot_memory;

	struct kgsl_snapshot *snapshot;
	/** @panic_nb: notifier block to capture GPU snapshot on kernel panic */
	struct notifier_block panic_nb;
	struct {
		void *ptr;
		u32 size;
	} snapshot_memory_atomic;

	u32 snapshot_faultcount;	/* Total number of faults since boot */
	bool force_panic;		/* Force panic after snapshot dump */
	bool skip_ib_capture;		/* Skip IB capture after snapshot */
	bool prioritize_unrecoverable;	/* Overwrite with new GMU snapshots */
	bool set_isdb_breakpoint;	/* Set isdb registers before snapshot */
	bool snapshot_atomic;		/* To capture snapshot in atomic context*/
	/* Use CP Crash dumper to get GPU snapshot*/
	bool snapshot_crashdumper;
	/* Use HOST side register reads to get GPU snapshot*/
	bool snapshot_legacy;
	/* Use to dump the context record in bytes */
	u64 snapshot_ctxt_record_size;

	struct kobject snapshot_kobj;

	struct kgsl_pwrscale pwrscale;

	int reset_counter; /* Track how many GPU core resets have occurred */
	struct workqueue_struct *events_wq;

	/* Number of active contexts seen globally for this device */
	int active_context_count;
	struct kobject gpu_sysfs_kobj;
	unsigned int l3_freq[3];
	unsigned int num_l3_pwrlevels;
	/* store current L3 vote to determine if we should change our vote */
	unsigned int cur_l3_pwrlevel;
	/** @globals: List of global memory objects */
	struct list_head globals;
	/** @globlal_map: bitmap for global memory allocations */
	unsigned long *global_map;
	/* @qdss_desc: Memory descriptor for the QDSS region if applicable */
	struct kgsl_memdesc *qdss_desc;
	/* @qtimer_desc: Memory descriptor for the QDSS region if applicable */
	struct kgsl_memdesc *qtimer_desc;
	/** @event_groups: List of event groups for this device */
	struct list_head event_groups;
	/** @event_groups_lock: A R/W lock for the events group list */
	rwlock_t event_groups_lock;
	/** @speed_bin: Speed bin for the GPU device if applicable */
	u32 speed_bin;
	/** @gmu_fault: Set when a gmu or rgmu fault is encountered */
	bool gmu_fault;
	/** @regmap: GPU register map */
	struct kgsl_regmap regmap;
	/** @timelines: Iterator for assigning IDs to timelines */
	struct idr timelines;
	/** @timelines_lock: Spinlock to protect the timelines idr */
	spinlock_t timelines_lock;
	/** @fence_trace_array: A local trace array for fence debugging */
	struct trace_array *fence_trace_array;
	/** @l3_vote: Enable/Disable l3 voting */
	bool l3_vote;
	/** @pdev_loaded: Flag to test if platform driver is probed */
	bool pdev_loaded;
	/** @nh: Pointer to head of the SRCU notifier chain */
	struct srcu_notifier_head nh;
	/** @freq_limiter_irq_clear: reset controller to clear freq limiter irq */
	struct reset_control *freq_limiter_irq_clear;
	/** @freq_limiter_intr_num: The interrupt number for freq limiter */
	int freq_limiter_intr_num;
	/** @bcl_data_kobj: Kobj for bcl_data sysfs node */
	struct kobject bcl_data_kobj;
	/** @idle_jiffies: Latest idle jiffies */
	unsigned long idle_jiffies;

	/** @work_period_timer: Timer to capture application GPU work stats */
	struct timer_list work_period_timer;
	/** work_period_lock: Lock to protect process application GPU work periods */
	spinlock_t work_period_lock;
	/** work_period_ws: Worker thread to emulate application GPU work event */
	struct work_struct work_period_ws;
	/** @flags: Flags for gpu_period stats */
	unsigned long flags;
	struct {
		u64 begin;
		u64 end;
	} gpu_period;
	/** @dump_all_ibs: Whether to dump all ibs in snapshot */
	bool dump_all_ibs;
};

#define KGSL_MMU_DEVICE(_mmu) \
	container_of((_mmu), struct kgsl_device, mmu)

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

#define KGSL_MAX_FAULT_ENTRIES 40

/* Maintain faults observed within threshold time (in milliseconds) */
#define KGSL_MAX_FAULT_TIME_THRESHOLD 5000

/**
 * struct kgsl_fault_node - GPU fault descriptor
 * @node: List node for list of faults
 * @type: Type of fault
 * @priv: Pointer to type specific fault
 * @time: Time when fault was observed
 */
struct kgsl_fault_node {
	struct list_head node;
	u32 type;
	void *priv;
	ktime_t time;
};

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
 * @total_fault_count: number of times gpu faulted in this context
 * @last_faulted_cmd_ts: last faulted command batch timestamp
 * @gmu_registered: whether context is registered with gmu or not
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
	ktime_t fault_time;
	struct kgsl_mem_entry *user_ctxt_record;
	unsigned int total_fault_count;
	unsigned int last_faulted_cmd_ts;
	bool gmu_registered;
	/**
	 * @gmu_dispatch_queue: dispatch queue id to which this context will be
	 * submitted
	 */
	u32 gmu_dispatch_queue;
	/** @faults: List of @kgsl_fault_node to store fault information */
	struct list_head faults;
	/** @fault_lock: Mutex to protect faults */
	struct mutex fault_lock;
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
		pid_nr((_c)->proc_priv->pid), ##args)

/**
 * struct kgsl_process_private -  Private structure for a KGSL process (across
 * all devices)
 * @priv: Internal flags, use KGSL_PROCESS_* values
 * @pid: Identification structure for the task owner of the process
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
 * @frame_count: Count for the number of frames processed
 */
struct kgsl_process_private {
	unsigned long priv;
	struct pid *pid;
	char comm[TASK_COMM_LEN];
	spinlock_t mem_lock;
	struct kref refcount;
	struct idr mem_idr;
	struct kgsl_pagetable *pagetable;
	struct list_head list;
	struct list_head reclaim_list;
	struct kobject kobj;
	struct dentry *debug_root;
	struct {
		atomic64_t cur;
		uint64_t max;
	} stats[KGSL_MEM_ENTRY_MAX];
	atomic64_t gpumem_mapped;
	struct idr syncsource_idr;
	spinlock_t syncsource_lock;
	int fd_count;
	atomic_t ctxt_count;
	spinlock_t ctxt_count_lock;
	atomic64_t frame_count;
	/**
	 * @state: state consisting KGSL_PROC_STATE and KGSL_PROC_PINNED_STATE
	 */
	unsigned long state;
	/**
	 * @unpinned_page_count: The number of pages unpinned for reclaim
	 */
	atomic_t unpinned_page_count;
	/**
	 * @fg_work: Work struct to schedule foreground work
	 */
	struct work_struct fg_work;
	/**
	 * @reclaim_lock: Mutex lock to protect KGSL_PROC_PINNED_STATE
	 */
	struct mutex reclaim_lock;
	/** @period: Stats for GPU utilization */
	struct gpu_work_period *period;
	/**
	 * @cmd_count: The number of cmds that are active for the process
	 */
	atomic_t cmd_count;
	/**
	 * @kobj_memtype: Pointer to a kobj for memtype sysfs directory for this
	 * process
	 */
	struct kobject kobj_memtype;
	/**
	 * @private_mutex: Mutex lock to protect kgsl_process_private
	 */
	struct mutex private_mutex;
	/**
	 * @cmdline: Cmdline string of the process
	 */
	char *cmdline;
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
 * @recovered: True if GPU was recovered after previous snapshot
 */
struct kgsl_snapshot {
	uint64_t ib1base;
	uint64_t ib2base;
	unsigned int ib1size;
	unsigned int ib2size;
	bool ib1dumped;
	bool ib2dumped;
	u64 ib1base_lpac;
	u64 ib2base_lpac;
	u32 ib1size_lpac;
	u32 ib2size_lpac;
	bool ib1dumped_lpac;
	bool ib2dumped_lpac;
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
	struct kgsl_process_private *process_lpac;
	unsigned int sysfs_read;
	bool first_read;
	bool recovered;
	struct kgsl_device *device;
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

static inline void kgsl_regread(struct kgsl_device *device,
				unsigned int offsetwords,
				unsigned int *value)
{
	*value = kgsl_regmap_read(&device->regmap, offsetwords);
}

static inline void kgsl_regread64(struct kgsl_device *device,
				u32 offsetwords_lo, u32 offsetwords_hi,
				u64 *value)
{
	u32 val_lo = 0, val_hi = 0;

	val_lo = kgsl_regmap_read(&device->regmap, offsetwords_lo);
	val_hi = kgsl_regmap_read(&device->regmap, offsetwords_hi);

	*value = (((u64)val_hi << 32) | val_lo);
}

static inline void kgsl_regwrite(struct kgsl_device *device,
				 unsigned int offsetwords,
				 unsigned int value)
{
	kgsl_regmap_write(&device->regmap, value, offsetwords);
}

static inline void kgsl_regrmw(struct kgsl_device *device,
		unsigned int offsetwords,
		unsigned int mask, unsigned int bits)
{
	kgsl_regmap_rmw(&device->regmap, offsetwords, mask, bits);
}

static inline bool kgsl_state_is_awake(struct kgsl_device *device)
{
	return (device->state == KGSL_STATE_ACTIVE ||
		device->state == KGSL_STATE_AWARE);
}

static inline bool kgsl_state_is_nap_or_minbw(struct kgsl_device *device)
{
	return (device->state == KGSL_STATE_NAP ||
		device->state == KGSL_STATE_MINBW);
}

/**
 * kgsl_start_idle_timer - Start the idle timer
 * @device: A KGSL device handle
 *
 * Start the idle timer to expire in 'interval_timeout' milliseconds
 */
static inline void kgsl_start_idle_timer(struct kgsl_device *device)
{
	device->idle_jiffies = jiffies + msecs_to_jiffies(device->pwrctrl.interval_timeout);
	mod_timer(&device->idle_timer, device->idle_jiffies);
}

int kgsl_readtimestamp(struct kgsl_device *device, void *priv,
		enum kgsl_timestamp_type type, unsigned int *timestamp);

bool kgsl_check_timestamp(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp);

int kgsl_device_platform_probe(struct kgsl_device *device);

void kgsl_device_platform_remove(struct kgsl_device *device);

const char *kgsl_pwrstate_to_str(unsigned int state);

/**
 * kgsl_device_snapshot_probe - add resources for the device GPU snapshot
 * @device: The device to initialize
 * @size: The size of the static region to allocate
 *
 * Allocate memory for a GPU snapshot for the specified device,
 * and create the sysfs files to manage it
 */
void kgsl_device_snapshot_probe(struct kgsl_device *device, u32 size);

void kgsl_device_snapshot(struct kgsl_device *device,
			struct kgsl_context *context, struct kgsl_context *context_lpac,
			bool gmu_fault);
void kgsl_device_snapshot_close(struct kgsl_device *device);

void kgsl_events_init(void);
void kgsl_events_exit(void);

/**
 * kgsl_device_events_probe - Set up events for the KGSL device
 * @device: A KGSL GPU device handle
 *
 * Set up the list and lock for GPU events for this device
 */
void kgsl_device_events_probe(struct kgsl_device *device);

/**
 * kgsl_device_events_remove - Remove all event groups from the KGSL device
 * @device: A KGSL GPU device handle
 *
 * Remove all of the GPU event groups from the device and warn if any of them
 * still have events pending
 */
void kgsl_device_events_remove(struct kgsl_device *device);

void kgsl_context_detach(struct kgsl_context *context);

/**
 * kgsl_del_event_group - Remove a GPU event group from a device
 * @device: A KGSL GPU device handle
 * @group: Event group to be removed
 *
 * Remove the specified group from the list of event groups on @device.
 */
void kgsl_del_event_group(struct kgsl_device *device,
		struct kgsl_event_group *group);

/**
 * kgsl_add_event_group - Add a new GPU event group
 * @device: A KGSL GPU device handle
 * @group: Pointer to the new group to add to the list
 * @context: Context that owns the group (or NULL for global)
 * @readtimestamp: Function pointer to the readtimestamp function to call when
 * processing events
 * @priv: Priv member to pass to the readtimestamp function
 * @fmt: The format string to use to build the event name
 * @...: Arguments for the format string
 */
void kgsl_add_event_group(struct kgsl_device *device,
		struct kgsl_event_group *group,
		struct kgsl_context *context, readtimestamp_func readtimestamp,
		void *priv, const char *fmt, ...);

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

/**
 * kgsl_context_type - Return a symbolic string for the context type
 * @type: Context type
 *
 * Return: Symbolic string representing the context type
 */
const char *kgsl_context_type(int type);

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

/** kgsl_context_is_bad - Check if a context is detached or invalid
 * @context: Pointer to a KGSL context handle
 *
 * Return: True if the context has been detached or is invalid
 */
static inline bool kgsl_context_is_bad(struct kgsl_context *context)
{
	return (kgsl_context_detached(context) ||
		kgsl_context_invalid(context));
}

/** kgsl_check_context_state - Check if a context is bad or invalid
 *  @context: Pointer to a KGSL context handle
 *
 * Return: True if the context has been marked bad or invalid
 */
static inline int kgsl_check_context_state(struct kgsl_context *context)
{
	if (kgsl_context_invalid(context))
		return -EDEADLK;

	if (kgsl_context_detached(context))
		return -ENOENT;

	return 0;
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
	if (process != NULL)
		return kref_get_unless_zero(&process->refcount);
	return 0;
}

void kgsl_process_private_put(struct kgsl_process_private *private);


struct kgsl_process_private *kgsl_process_private_find(pid_t pid);

/*
 * A helper macro to print out "not enough memory functions" - this
 * makes it easy to standardize the messages as well as cut down on
 * the number of strings in the binary
 */
#define SNAPSHOT_ERR_NOMEM(_d, _s) \
	dev_err((_d)->dev, \
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
 * kgsl_of_property_read_ddrtype - Get property from devicetree based on
 * the type of DDR.
 * @node: Devicetree node
 * @base: prefix string of the property
 * @ptr:  Pointer to store the value of the property
 *
 * First look up the devicetree property based on the prefix string and DDR
 * type. If property is not specified per DDR type, then look for the property
 * based on prefix string only.
 *
 * Return: 0 on success or error code on failure.
 */
int kgsl_of_property_read_ddrtype(struct device_node *node, const char *base,
		u32 *ptr);

/**
 * kgsl_query_property_list - Get a list of valid properties
 * @device: A KGSL device handle
 * @list: Pointer to a list of u32s
 * @count: Number of items in @list
 *
 * Populate a list with the IDs for supported properties. If @list is NULL,
 * just return the number of properties available, otherwise fill up to @count
 * items in the list with property identifiers.
 *
 * Returns the number of total properties if @list is NULL or the number of
 * properties copied to @list.
 */
int kgsl_query_property_list(struct kgsl_device *device, u32 *list, u32 count);

static inline bool kgsl_mmu_has_feature(struct kgsl_device *device,
		enum kgsl_mmu_feature feature)
{
	return test_bit(feature, &device->mmu.features);
}

static inline void kgsl_mmu_set_feature(struct kgsl_device *device,
	      enum kgsl_mmu_feature feature)
{
	set_bit(feature, &device->mmu.features);
}

/**
 * kgsl_add_fault - Add fault information for a context
 * @context: Pointer to the KGSL context
 * @type: type of fault info
 * @priv: Pointer to type specific fault info
 *
 * Return: 0 on success or error code on failure.
 */
int kgsl_add_fault(struct kgsl_context *context, u32 type, void *priv);

/**
 * kgsl_free_faults - Free fault information for a context
 * @context: Pointer to the KGSL context
 */
void kgsl_free_faults(struct kgsl_context *context);

/**
 * kgsl_trace_gpu_mem_total - Overall gpu memory usage tracking which includes
 * process allocations, imported dmabufs and kgsl globals
 * @device: A KGSL device handle
 * @delta: delta of total mapped memory size
 */
#ifdef CONFIG_TRACE_GPU_MEM
static inline void kgsl_trace_gpu_mem_total(struct kgsl_device *device,
						s64 delta)
{
	u64 total_size;

	total_size = atomic64_add_return(delta, &device->total_mapped);
	trace_gpu_mem_total(0, 0, total_size);
}
#else
static inline void kgsl_trace_gpu_mem_total(struct kgsl_device *device,
						s64 delta) {}
#endif

/*
 * kgsl_context_is_lpac() - Checks if context is LPAC
 * @context: KGSL context to check
 *
 * Function returns true if context is LPAC else false
 */
static inline bool kgsl_context_is_lpac(struct kgsl_context *context)
{
	if (context == NULL)
		return false;

	return (context->flags & KGSL_CONTEXT_LPAC) ? true : false;
}

#endif  /* __KGSL_DEVICE_H */

/*
 * include/linux/tegra_profiler.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __TEGRA_PROFILER_H
#define __TEGRA_PROFILER_H

#include <linux/ioctl.h>

#define QUADD_SAMPLES_VERSION	22
#define QUADD_IO_VERSION	10

#define QUADD_IO_VERSION_DYNAMIC_RB		5
#define QUADD_IO_VERSION_RB_MAX_FILL_COUNT	6
#define QUADD_IO_VERSION_MOD_STATE_STATUS_FIELD	7
#define QUADD_IO_VERSION_BT_KERNEL_CTX		8
#define QUADD_IO_VERSION_GET_MMAP		9
#define QUADD_IO_VERSION_BT_UNWIND_TABLES	10

#define QUADD_SAMPLE_VERSION_THUMB_MODE_FLAG	17
#define QUADD_SAMPLE_VERSION_GROUP_SAMPLES	18
#define QUADD_SAMPLE_VERSION_THREAD_STATE_FLD	19
#define QUADD_SAMPLE_VERSION_BT_UNWIND_TABLES	22

#define QUADD_MAX_COUNTERS	32
#define QUADD_MAX_PROCESS	64

#define QUADD_DEVICE_NAME	"quadd"
#define QUADD_AUTH_DEVICE_NAME	"quadd_auth"

#define QUADD_MOD_DEVICE_NAME		"quadd_mod"
#define QUADD_MOD_AUTH_DEVICE_NAME	"quadd_mod_auth"

#define QUADD_IOCTL	100

/*
 * Setup params (profiling frequency, etc.)
 */
#define IOCTL_SETUP _IOW(QUADD_IOCTL, 0, struct quadd_parameters)

/*
 * Start profiling.
 */
#define IOCTL_START _IO(QUADD_IOCTL, 1)

/*
 * Stop profiling.
 */
#define IOCTL_STOP _IO(QUADD_IOCTL, 2)

/*
 * Getting capabilities
 */
#define IOCTL_GET_CAP _IOR(QUADD_IOCTL, 3, struct quadd_comm_cap)

/*
 * Getting state of module
 */
#define IOCTL_GET_STATE _IOR(QUADD_IOCTL, 4, struct quadd_module_state)

/*
 * Getting version of module
 */
#define IOCTL_GET_VERSION _IOR(QUADD_IOCTL, 5, struct quadd_module_version)

/*
 * Send exception-handling tables info
 */
#define IOCTL_SET_EXTAB _IOW(QUADD_IOCTL, 6, struct quadd_extables)

#define QUADD_CPUMODE_TEGRA_POWER_CLUSTER_LP	(1 << 29)	/* LP CPU */
#define QUADD_CPUMODE_THUMB			(1 << 30)	/* thumb mode */

enum quadd_events_id {
	QUADD_EVENT_TYPE_CPU_CYCLES = 0,

	QUADD_EVENT_TYPE_INSTRUCTIONS,
	QUADD_EVENT_TYPE_BRANCH_INSTRUCTIONS,
	QUADD_EVENT_TYPE_BRANCH_MISSES,
	QUADD_EVENT_TYPE_BUS_CYCLES,

	QUADD_EVENT_TYPE_L1_DCACHE_READ_MISSES,
	QUADD_EVENT_TYPE_L1_DCACHE_WRITE_MISSES,
	QUADD_EVENT_TYPE_L1_ICACHE_MISSES,

	QUADD_EVENT_TYPE_L2_DCACHE_READ_MISSES,
	QUADD_EVENT_TYPE_L2_DCACHE_WRITE_MISSES,
	QUADD_EVENT_TYPE_L2_ICACHE_MISSES,

	QUADD_EVENT_TYPE_MAX,
};

struct event_data {
	int event_source;
	int event_id;

	u32 val;
	u32 prev_val;
};

enum quadd_record_type {
	QUADD_RECORD_TYPE_SAMPLE = 1,
	QUADD_RECORD_TYPE_MMAP,
	QUADD_RECORD_TYPE_MA,
	QUADD_RECORD_TYPE_COMM,
	QUADD_RECORD_TYPE_DEBUG,
	QUADD_RECORD_TYPE_HEADER,
	QUADD_RECORD_TYPE_POWER_RATE,
	QUADD_RECORD_TYPE_ADDITIONAL_SAMPLE,
};

enum quadd_event_source {
	QUADD_EVENT_SOURCE_PMU = 1,
	QUADD_EVENT_SOURCE_PL310,
};

enum quadd_cpu_mode {
	QUADD_CPU_MODE_KERNEL = 1,
	QUADD_CPU_MODE_USER,
	QUADD_CPU_MODE_NONE,
};

typedef u32 quadd_bt_addr_t;

#pragma pack(push, 1)

#define QUADD_SAMPLE_UNW_METHOD_SHIFT	0
#define QUADD_SAMPLE_UNW_METHOD_MASK	(1 << QUADD_SAMPLE_UNW_METHOD_SHIFT)

enum {
	QUADD_UNW_METHOD_FP = 0,
	QUADD_UNW_METHOD_EHT,
};

#define QUADD_SAMPLE_URC_SHIFT		1
#define QUADD_SAMPLE_URC_MASK		(0x0f << QUADD_SAMPLE_URC_SHIFT)

enum {
	QUADD_URC_SUCCESS = 0,
	QUADD_URC_FAILURE,
	QUADD_URC_IDX_NOT_FOUND,
	QUADD_URC_TBL_NOT_EXIST,
	QUADD_URC_EACCESS,
	QUADD_URC_TBL_IS_CORRUPT,
	QUADD_URC_CANTUNWIND,
	QUADD_URC_UNHANDLED_INSTRUCTION,
	QUADD_URC_REFUSE_TO_UNWIND,
	QUADD_URC_SP_INCORRECT,
	QUADD_URC_SPARE_ENCODING,
	QUADD_URC_UNSUPPORTED_PR,
};

struct quadd_sample_data {
	u64 ip;
	u32 pid;
	u64 time;

	u16	cpu:6,
		user_mode:1,
		lp_mode:1,
		thumb_mode:1,
		state:1,
		in_interrupt:1,
		reserved:5;

	u8 callchain_nr;
	u32 events_flags;
};

struct quadd_mmap_data {
	u32 pid;

	u64 addr;
	u64 len;

	u8 user_mode:1;
	u16 filename_length;
};

struct quadd_ma_data {
	u32 pid;
	u64 time;

	u32 vm_size;
	u32 rss_size;
};

struct quadd_power_rate_data {
	u64 time;

	u8 nr_cpus;

	u32 gpu;
	u32 emc;
};

struct quadd_additional_sample {
	u8 type;

	u32 values[6];
	u16 extra_length;
};

enum {
	QM_DEBUG_SAMPLE_TYPE_SCHED_IN = 1,
	QM_DEBUG_SAMPLE_TYPE_SCHED_OUT,

	QM_DEBUG_SAMPLE_TYPE_TIMER_HANDLE,
	QM_DEBUG_SAMPLE_TYPE_TIMER_START,
	QM_DEBUG_SAMPLE_TYPE_TIMER_CANCEL,
	QM_DEBUG_SAMPLE_TYPE_TIMER_FORWARD,

	QM_DEBUG_SAMPLE_TYPE_READ_COUNTER,

	QM_DEBUG_SAMPLE_TYPE_SOURCE_START,
	QM_DEBUG_SAMPLE_TYPE_SOURCE_STOP,
};

struct quadd_debug_data {
	u8 type;

	u32 pid;
	u64 time;

	u16	cpu:6,
		user_mode:1,
		lp_mode:1,
		thumb_mode:1,
		reserved:7;

	u32 extra_value[2];
	u16 extra_length;
};

#define QUADD_HEADER_MAGIC	0x1122

struct quadd_header_data {
	u16 magic;
	u16 version;

	u32	backtrace:1,
		use_freq:1,
		system_wide:1,
		power_rate:1,
		debug_samples:1,
		get_mmap:1,
		reserved:26;	/* reserved fields for future extensions */

	u32 freq;
	u16 ma_freq;
	u16 power_rate_freq;

	u8 nr_events;
	u16 extra_length;
};

struct quadd_record_data {
	u8 record_type;

	/* sample: it should be the biggest size */
	union {
		struct quadd_sample_data	sample;
		struct quadd_mmap_data		mmap;
		struct quadd_ma_data		ma;
		struct quadd_debug_data		debug;
		struct quadd_header_data	hdr;
		struct quadd_power_rate_data	power_rate;
		struct quadd_additional_sample	additional_sample;
	};
} __aligned(4);

#pragma pack(4)

#define QUADD_MAX_PACKAGE_NAME	320

enum {
	QUADD_PARAM_IDX_SIZE_OF_RB	= 0,
	QUADD_PARAM_IDX_EXTRA		= 1,
};

#define QUADD_PARAM_EXTRA_GET_MMAP		(1 << 0)
#define QUADD_PARAM_EXTRA_BT_FP			(1 << 1)
#define QUADD_PARAM_EXTRA_BT_UNWIND_TABLES	(1 << 2)

struct quadd_parameters {
	u32 freq;
	u32 ma_freq;
	u32 power_rate_freq;

	u64	backtrace:1,
		use_freq:1,
		system_wide:1,
		debug_samples:1;

	u32 pids[QUADD_MAX_PROCESS];
	u32 nr_pids;

	u8 package_name[QUADD_MAX_PACKAGE_NAME];

	u32 events[QUADD_MAX_COUNTERS];
	u32 nr_events;

	u32 reserved[16];	/* reserved fields for future extensions */
};

struct quadd_events_cap {
	u32	cpu_cycles:1,
		instructions:1,
		branch_instructions:1,
		branch_misses:1,
		bus_cycles:1,

		l1_dcache_read_misses:1,
		l1_dcache_write_misses:1,
		l1_icache_misses:1,

		l2_dcache_read_misses:1,
		l2_dcache_write_misses:1,
		l2_icache_misses:1;
};

enum {
	QUADD_COMM_CAP_IDX_EXTRA = 0,
};

#define QUADD_COMM_CAP_EXTRA_BT_KERNEL_CTX	(1 << 0)
#define QUADD_COMM_CAP_EXTRA_GET_MMAP		(1 << 1)
#define QUADD_COMM_CAP_EXTRA_GROUP_SAMPLES	(1 << 2)
#define QUADD_COMM_CAP_EXTRA_BT_UNWIND_TABLES	(1 << 3)

struct quadd_comm_cap {
	u32	pmu:1,
		power_rate:1,
		l2_cache:1,
		l2_multiple_events:1,
		tegra_lp_cluster:1,
		blocked_read:1;

	struct quadd_events_cap events_cap;

	u32 reserved[16];	/* reserved fields for future extensions */
};

enum {
	QUADD_MOD_STATE_IDX_RB_MAX_FILL_COUNT = 0,
	QUADD_MOD_STATE_IDX_STATUS,
};

#define QUADD_MOD_STATE_STATUS_IS_ACTIVE	(1 << 0)
#define QUADD_MOD_STATE_STATUS_IS_AUTH_OPEN	(1 << 1)

struct quadd_module_state {
	u64 nr_all_samples;
	u64 nr_skipped_samples;

	u32 buffer_size;
	u32 buffer_fill_size;

	u32 reserved[16];	/* reserved fields for future extensions */
};

struct quadd_module_version {
	u8 branch[32];
	u8 version[16];

	u32 samples_version;
	u32 io_version;

	u32 reserved[4];	/* reserved fields for future extensions */
};

struct quadd_sec_info {
	u64 addr;
	u64 length;
};

struct quadd_extables {
	u64 vm_start;
	u64 vm_end;

	struct quadd_sec_info extab;
	struct quadd_sec_info exidx;

	u32 reserved[4];	/* reserved fields for future extensions */
};

#pragma pack(pop)

#ifdef __KERNEL__

struct task_struct;
struct vm_area_struct;

#ifdef CONFIG_TEGRA_PROFILER
extern void __quadd_task_sched_in(struct task_struct *prev,
				  struct task_struct *task);
extern void __quadd_task_sched_out(struct task_struct *prev,
				   struct task_struct *next);

extern void __quadd_event_mmap(struct vm_area_struct *vma);

static inline void quadd_task_sched_in(struct task_struct *prev,
				       struct task_struct *task)
{
	__quadd_task_sched_in(prev, task);
}

static inline void quadd_task_sched_out(struct task_struct *prev,
					struct task_struct *next)
{
	__quadd_task_sched_out(prev, next);
}

static inline void quadd_event_mmap(struct vm_area_struct *vma)
{
	__quadd_event_mmap(vma);
}

#else	/* CONFIG_TEGRA_PROFILER */

static inline void quadd_task_sched_in(struct task_struct *prev,
				       struct task_struct *task)
{
}

static inline void quadd_task_sched_out(struct task_struct *prev,
					struct task_struct *next)
{
}

static inline void quadd_event_mmap(struct vm_area_struct *vma)
{
}

#endif	/* CONFIG_TEGRA_PROFILER */

#endif	/* __KERNEL__ */

#endif  /* __TEGRA_PROFILER_H */

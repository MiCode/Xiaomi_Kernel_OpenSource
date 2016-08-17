/*
 * include/linux/tegra_profiler.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#define QUADD_SAMPLES_VERSION	16
#define QUADD_IO_VERSION	4

#define QUADD_MAX_COUNTERS	32
#define QUADD_MAX_PROCESS	64

#define QUADD_DEVICE_NAME	"quadd"
#define QUADD_AUTH_DEVICE_NAME	"quadd_auth"

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


#define QUADD_HRT_SCHED_IN_FUNC		"finish_task_switch"

#define QM_TEGRA_POWER_CLUSTER_LP	(1 << 29) /* LP CPU */

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

#pragma pack(push, 4)

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

struct quadd_sample_data {
	u32 event_id;

	u32 ip;
	u32 pid;
	u64 time;
	u32 cpu;
	u64 period;

	u32 callchain_nr;
};

struct quadd_mmap_data {
	u32 pid;
	u32 addr;
	u64 len;
	u64 pgoff;

	u32 filename_length;
};

struct quadd_ma_data {
	u32 pid;
	u64 time;

	u64 vm_size;
	u64 rss_size;
};

struct quadd_power_rate_data {
	u64 time;

	u32 nr_cpus;

	u32 gpu;
	u32 emc;
};

struct quadd_additional_sample {
	u32 type;

	u32 values[8];
	u32 extra_length;
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
	u32 type;

	u32 cpu;
	u32 pid;
	u64 time;

	u64 timer_period;

	u32 extra_value1;
	u32 extra_value2;
	u32 extra_value3;
};


struct quadd_header_data {
	u32 version;

	u32	backtrace:1,
		use_freq:1,
		system_wide:1,
		power_rate:1,
		debug_samples:1;

	u64 period;
	u32 ma_period;
	u32 power_rate_period;

	u32 reserved[4];	/* reserved fields for future extensions */
};

#define QUADD_RECORD_MAGIC	0x33557799

struct quadd_record_data {
	u32 magic;	/* for debug */
	u32 record_type;
	u32 cpu_mode;

	union {
		struct quadd_sample_data	sample;
		struct quadd_mmap_data		mmap;
		struct quadd_ma_data		ma;
		struct quadd_debug_data		debug;
		struct quadd_header_data	hdr;
		struct quadd_power_rate_data	power_rate;
		struct quadd_additional_sample	additional_sample;
	};
};

#define QUADD_MAX_PACKAGE_NAME	320

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

#pragma pack(pop)

#endif  /* __TEGRA_PROFILER_H */

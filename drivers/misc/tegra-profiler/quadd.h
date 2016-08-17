/*
 * drivers/misc/tegra-profiler/quadd.h
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

#ifndef __QUADD_H
#define __QUADD_H

#include <linux/tegra_profiler.h>

/* #define QUADD_USE_EMULATE_COUNTERS	1 */

struct event_data;
struct quadd_comm_data_interface;
struct quadd_hrt_ctx;
struct quadd_mmap_ctx;

struct quadd_event_source_interface {
	int (*enable)(void);
	void (*disable)(void);
	void (*start)(void);
	void (*stop)(void);
	int (*read)(struct event_data *events);
	int (*set_events)(int *events, int size);
	int (*get_supported_events)(int *events);
};

struct source_info {
	int supported_events[QUADD_MAX_COUNTERS];
	int nr_supported_events;

	int active;
};

struct quadd_ctx {
	struct quadd_parameters param;

	struct quadd_event_source_interface *pmu;
	struct source_info pmu_info;

	struct quadd_event_source_interface *pl310;
	struct source_info pl310_info;

	struct quadd_comm_data_interface *comm;
	struct quadd_hrt_ctx *hrt;
	struct quadd_mmap_ctx *mmap;

	atomic_t started;
};

#endif	/* __QUADD_H */

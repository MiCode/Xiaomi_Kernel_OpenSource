/*
 * include/trace/events/nvpower.h
 *
 * NVIDIA Tegra specific power events.
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM nvpower

#if !defined(_TRACE_NVPOWER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVPOWER_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

#ifndef _NV_PWR_EVENT_AVOID_DOUBLE_DEFINING
#define _NV_PWR_EVENT_AVOID_DOUBLE_DEFINING

enum {
	NVPOWER_CPU_CLUSTER_START,
	NVPOWER_CPU_CLUSTER_DONE,
};

enum {
	NVPOWER_CPU_POWERGATE_ENTRY,
	NVPOWER_CPU_POWERGATE_EXIT,
};

enum {
	NVPOWER_MC_CLK_STOP_ENTRY,
	NVPOWER_MC_CLK_STOP_EXIT,
};

#endif

TRACE_EVENT(nvcpu_cluster,

	TP_PROTO(int state, int start, int target),

	TP_ARGS(state, start, target),

	TP_STRUCT__entry(
		__field(u64, state)
		__field(u32, start)
		__field(u32, target)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->start = start;
		__entry->target = target;
	),

	TP_printk("state=%lu, start=0x%08lx, target=0x%08lx",
		  (unsigned long)__entry->state,
		  (unsigned long)__entry->start,
		  (unsigned long)__entry->target)
);

extern u32 notrace tegra_read_usec_raw(void);

TRACE_EVENT(nvcpu_powergate,

	TP_PROTO(int state),

	TP_ARGS(state),

	TP_STRUCT__entry(
		__field(u32, counter)
		__field(u32, state)
	),

	TP_fast_assign(
		__entry->counter = tegra_read_usec_raw();
		__entry->state = state;
	),

	TP_printk("counter=%lu, state=%lu",
		  (unsigned long)__entry->counter,
		  (unsigned long)__entry->state)
);

TRACE_EVENT(nvmc_clk_stop,

	TP_PROTO(int state, unsigned long sleep),

	TP_ARGS(state, sleep),

	TP_STRUCT__entry(
		__field(u32, counter)
		__field(u32, sleep)
		__field(u32, state)
	),

	TP_fast_assign(
		__entry->counter = tegra_read_usec_raw();
		__entry->sleep = sleep;
		__entry->state = state;
	),

	TP_printk("counter=%lu, sleep=%lu state=%lu",
		  (unsigned long)__entry->counter,
		  (unsigned long)__entry->sleep,
		  (unsigned long)__entry->state)
);
#endif /* _TRACE_NVPOWER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#undef TRACE_SYSTEM
#define TRACE_SYSTEM process_reclaim

#if !defined(_TRACE_EVENT_PROCESSRECLAIM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_PROCESSRECLAIM_H

#include <linux/tracepoint.h>
#include <linux/types.h>
#include <linux/sched.h>

TRACE_EVENT(process_reclaim,

	TP_PROTO(int tasksize,
		short oom_score_adj,
		int nr_scanned, int nr_reclaimed,
		int per_swap_size, int total_sz,
		int nr_to_reclaim),

	TP_ARGS(tasksize, oom_score_adj, nr_scanned,
			nr_reclaimed, per_swap_size,
			total_sz, nr_to_reclaim),

	TP_STRUCT__entry(
		__field(int, tasksize)
		__field(short, oom_score_adj)
		__field(int, nr_scanned)
		__field(int, nr_reclaimed)
		__field(int, per_swap_size)
		__field(int, total_sz)
		__field(int, nr_to_reclaim)
	),

	TP_fast_assign(
		__entry->tasksize	= tasksize;
		__entry->oom_score_adj	= oom_score_adj;
		__entry->nr_scanned	= nr_scanned;
		__entry->nr_reclaimed	= nr_reclaimed;
		__entry->per_swap_size	= per_swap_size;
		__entry->total_sz	= total_sz;
		__entry->nr_to_reclaim	= nr_to_reclaim;
	),

	TP_printk("%d, %hd, %d, %d, %d, %d, %d",
			__entry->tasksize, __entry->oom_score_adj,
			__entry->nr_scanned, __entry->nr_reclaimed,
			__entry->per_swap_size, __entry->total_sz,
			__entry->nr_to_reclaim)
);

TRACE_EVENT(process_reclaim_eff,

	TP_PROTO(int efficiency, int reclaim_avg_efficiency),

	TP_ARGS(efficiency, reclaim_avg_efficiency),

	TP_STRUCT__entry(
		__field(int, efficiency)
		__field(int, reclaim_avg_efficiency)
	),

	TP_fast_assign(
		__entry->efficiency	= efficiency;
		__entry->reclaim_avg_efficiency	= reclaim_avg_efficiency;
	),

	TP_printk("%d, %d", __entry->efficiency,
		__entry->reclaim_avg_efficiency)
);

#endif

#include <trace/define_trace.h>


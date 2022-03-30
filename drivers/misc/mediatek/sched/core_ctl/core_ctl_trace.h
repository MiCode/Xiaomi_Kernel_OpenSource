/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM core_ctl

#if !defined(_CORE_CTL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _CORE_CTL_TRACE_H

#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(core_ctl_algo_info,

	TP_PROTO(
		unsigned int big_cpu_ts,
		unsigned int heaviest_thres,
		unsigned int max_util,
		unsigned int active_cpus,
		unsigned int *orig_need_cpus),

	TP_ARGS(big_cpu_ts, heaviest_thres, max_util, active_cpus, orig_need_cpus),

	TP_STRUCT__entry(
		__field(unsigned int, big_cpu_ts)
		__field(unsigned int, heaviest_thres)
		__field(unsigned int, max_util)
		__field(unsigned int, active_cpus)
		__array(unsigned int, orig_need_cpus, 3)
	),

	TP_fast_assign(
		__entry->big_cpu_ts = big_cpu_ts;
		__entry->heaviest_thres = heaviest_thres;
		__entry->max_util = max_util;
		__entry->active_cpus = active_cpus;
		memcpy(__entry->orig_need_cpus, orig_need_cpus, sizeof(unsigned int)*3);
	),

	TP_printk("big_cpu_ts=%u heaviest_thres=%u max_util=%u active_cpus=%x orig_need_cpus=%u|%u|%u",
		__entry->big_cpu_ts,
		__entry->heaviest_thres,
		__entry->max_util,
		__entry->active_cpus,
		__entry->orig_need_cpus[0],
		__entry->orig_need_cpus[1],
		__entry->orig_need_cpus[2])
);

TRACE_EVENT(core_ctl_demand_eval,

		TP_PROTO(
			unsigned int cid,
			unsigned int old_need,
			unsigned int new_need,
			unsigned int active_cpus,
			unsigned int min_cpus,
			unsigned int max_cpus,
			unsigned int boost,
			unsigned int enable,
			unsigned int updated),

		TP_ARGS(
			cid,
			old_need,
			new_need,
			active_cpus,
			min_cpus,
			max_cpus,
			boost,
			enable,
			updated),

		TP_STRUCT__entry(
			__field(u32, cid)
			__field(u32, old_need)
			__field(u32, new_need)
			__field(u32, active_cpus)
			__field(u32, min_cpus)
			__field(u32, max_cpus)
			__field(u32, boost)
			__field(u32, enable)
			__field(u32, updated)
		),

		TP_fast_assign(
			__entry->cid = cid;
			__entry->old_need = old_need;
			__entry->new_need = new_need;
			__entry->active_cpus = active_cpus;
			__entry->min_cpus = min_cpus;
			__entry->max_cpus = max_cpus;
			__entry->boost = boost;
			__entry->enable = enable;
			__entry->updated = updated;
		),

		TP_printk("cid=%u, old=%u, new=%u, act=%u min=%u max=%u bst=%u enbl=%u update=%u",
			__entry->cid,
			__entry->old_need,
			__entry->new_need,
			__entry->active_cpus,
			__entry->min_cpus,
			__entry->max_cpus,
			__entry->boost,
			__entry->enable,
			__entry->updated)
);

TRACE_EVENT(core_ctl_update_nr_over_thres,

	TP_PROTO(
		unsigned int *nr_up,
		unsigned int *nr_down,
		unsigned int *need_spread_cpus),

	TP_ARGS(nr_up, nr_down, need_spread_cpus),

	TP_STRUCT__entry(
		__array(unsigned int, nr_up, 3)
		__array(unsigned int, nr_down, 3)
		__array(unsigned int, need_spread_cpus, 3)
	),

	TP_fast_assign(
		memcpy(__entry->nr_up, nr_up, sizeof(unsigned int) * 3);
		memcpy(__entry->nr_down, nr_down, sizeof(unsigned int) * 3);
		memcpy(__entry->need_spread_cpus, need_spread_cpus, sizeof(unsigned int) * 3);
	),

	TP_printk("nr_up=%u|%u|%u nr_down=%u|%u|%u need_spread_cpus=%u|%u|%u",
		__entry->nr_up[0], __entry->nr_up[1], __entry->nr_up[2],
		__entry->nr_down[0], __entry->nr_down[1], __entry->nr_down[2],
		__entry->need_spread_cpus[0], __entry->need_spread_cpus[1],
		__entry->need_spread_cpus[2])
);

#endif /*_CORE_CTL_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH core_ctl
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE core_ctl_trace
/* This part must be outside protection */
#include <trace/define_trace.h>

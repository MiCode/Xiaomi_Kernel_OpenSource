/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM core_ctl

#if !defined(_CORE_CTL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _CORE_CTL_TRACE_H

#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(core_ctl_info,

	TP_PROTO(
		int ts_cpu,
		unsigned int btask_thresh,
		int util,
		unsigned int isolate_cpus,
		unsigned int *need_cpus),

	TP_ARGS(ts_cpu, btask_thresh, util, isolate_cpus, need_cpus),

	TP_STRUCT__entry(
		__field(int, ts_cpu)
		__field(unsigned int, btask_thresh)
		__field(int, util)
		__field(u32, isolate_cpus)
		__array(unsigned int, need_cpus, 3)
	),

	TP_fast_assign(
		__entry->ts_cpu = ts_cpu;
		__entry->btask_thresh = btask_thresh;
		__entry->util = util;
		__entry->isolate_cpus = isolate_cpus;
		memcpy(__entry->need_cpus, need_cpus, sizeof(unsigned int)*3);
	),

	TP_printk("ts_cpu=%d btask_thresh=%u util=%d isolate_cpus=%x need_cpus=%u|%u|%u",
		__entry->ts_cpu,
		__entry->btask_thresh, __entry->util,
		__entry->isolate_cpus,
		__entry->need_cpus[0], __entry->need_cpus[1], __entry->need_cpus[2])
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

TRACE_EVENT(core_ctl_update_nr_btask,

	TP_PROTO(
		int *nr_up,
		int *nr_down,
		int *max_nr_down),

	TP_ARGS(nr_up, nr_down, max_nr_down),

	TP_STRUCT__entry(
		__array(int, nr_up, 3)
		__array(int, nr_down, 3)
		__array(int, max_nr_down, 3)
	),

	TP_fast_assign(
		memcpy(__entry->nr_up, nr_up, sizeof(unsigned int) * 3);
		memcpy(__entry->nr_down, nr_down, sizeof(unsigned int) * 3);
		memcpy(__entry->max_nr_down, max_nr_down, sizeof(unsigned int) * 3);
	),

	TP_printk("nr_up=%d|%d|%d nr_down=%d|%d|%d max_nr_down=%d|%d|%d ",
		__entry->nr_up[0], __entry->nr_up[1], __entry->nr_up[2],
		__entry->nr_down[0], __entry->nr_down[1], __entry->nr_down[2],
		__entry->max_nr_down[0], __entry->max_nr_down[1], __entry->max_nr_down[2])
);

#endif /*_CORE_CTL_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE core_ctl_trace
/* This part must be outside protection */
#include <trace/define_trace.h>

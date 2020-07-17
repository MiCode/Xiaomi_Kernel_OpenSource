/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#if !defined(_MDSS_PLL_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _MDSS_PLL_TRACE_H_

#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mdss_pll
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE pll_trace


TRACE_EVENT(mdss_pll_lock_start,
	TP_PROTO(
			u64 vco_cached_rate,
			s64 vco_current_rate,
			u32 cached_cfg0,
			u32 cached_cfg1,
			u32 cached_outdiv,
			u32 resource_ref_cnt),
	TP_ARGS(
			vco_cached_rate,
			vco_current_rate,
			cached_cfg0,
			cached_cfg1,
			cached_outdiv,
			resource_ref_cnt),
	TP_STRUCT__entry(
			__field(u64, vco_cached_rate)
			__field(s64, vco_current_rate)
			__field(u32, cached_cfg0)
			__field(u32, cached_cfg1)
			__field(u32, cached_outdiv)
			__field(u32, resource_ref_cnt)

	),
	TP_fast_assign(
			__entry->vco_cached_rate = vco_cached_rate;
			__entry->vco_current_rate = vco_current_rate;
			__entry->cached_cfg0 = cached_cfg0;
			__entry->cached_cfg1 = cached_cfg1;
			__entry->cached_outdiv = cached_outdiv;
			__entry->resource_ref_cnt = resource_ref_cnt;
	),
	 TP_printk(
		"vco_cached_rate=%llu vco_current_rate=%lld cached_cfg0=%d cached_cfg1=%d cached_outdiv=%d resource_ref_cnt=%d",
			__entry->vco_cached_rate,
			__entry->vco_current_rate,
			__entry->cached_cfg0,
			__entry->cached_cfg1,
			__entry->cached_outdiv,
			__entry->resource_ref_cnt)
);

TRACE_EVENT(pll_tracing_mark_write,
	TP_PROTO(int pid, const char *name, bool trace_begin),
	TP_ARGS(pid, name, trace_begin),
	TP_STRUCT__entry(
			__field(int, pid)
			__string(trace_name, name)
			__field(bool, trace_begin)
	),
	TP_fast_assign(
			__entry->pid = pid;
			__assign_str(trace_name, name);
			__entry->trace_begin = trace_begin;
	),
	TP_printk("%s|%d|%s", __entry->trace_begin ? "B" : "E",
		__entry->pid, __get_str(trace_name))
)

TRACE_EVENT(mdss_pll_trace_counter,
	TP_PROTO(int pid, char *name, int value),
	TP_ARGS(pid, name, value),
	TP_STRUCT__entry(
			__field(int, pid)
			__string(counter_name, name)
			__field(int, value)
	),
	TP_fast_assign(
			__entry->pid = current->tgid;
			__assign_str(counter_name, name);
			__entry->value = value;
	),
	TP_printk("%d|%s|%d", __entry->pid,
			__get_str(counter_name), __entry->value)
)

#define MDSS_PLL_ATRACE_END(name) trace_pll_tracing_mark_write(current->tgid,\
		name, 0)
#define MDSS_PLL_ATRACE_BEGIN(name) trace_pll_tracing_mark_write(current->tgid,\
		name, 1)
#define MDSS_PLL_ATRACE_FUNC() MDSS_PLL_ATRACE_BEGIN(__func__)
#define MDSS_PLL_ATRACE_INT(name, value) \
	trace_mdss_pll_trace_counter(current->tgid, name, value)


#endif /* _MDSS_PLL_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>

/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM perf

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_PERF_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_PERF_H

#include <trace/hooks/vendor_hooks.h>

struct perf_event;
DECLARE_RESTRICTED_HOOK(android_rvh_armv8pmu_counter_overflowed,
	TP_PROTO(struct perf_event *event),
	TP_ARGS(event), 1);

struct perf_cpu_pmu_context;
DECLARE_RESTRICTED_HOOK(android_rvh_perf_rotate_context,
	TP_PROTO(struct perf_cpu_pmu_context *cpc),
	TP_ARGS(cpc), 1);

#endif /* _TRACE_HOOK_PERF_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

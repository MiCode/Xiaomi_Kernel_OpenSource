/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM timekeeping

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_TIMEKEEPING_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_TIMEKEEPING_H

#include <trace/hooks/vendor_hooks.h>

struct timekeeper;
DECLARE_RESTRICTED_HOOK(android_rvh_tk_based_time_sync,
	TP_PROTO(struct timekeeper *tk),
	TP_ARGS(tk), 1);

#endif /* _TRACE_HOOK_TIMEKEEPING_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM health_report

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_HEALTH_REPORT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_HEALTH_REPORT_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(android_vh_health_report,
	TP_PROTO(unsigned int err_code, const char *func, unsigned int line,
		unsigned long *entries, unsigned int nr_entries),
	TP_ARGS(err_code, func, line, entries, nr_entries));


#endif /* _TRACE_HOOK_HEALTH_REPORT_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

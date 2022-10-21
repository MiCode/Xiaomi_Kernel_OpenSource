#undef TRACE_SYSTEM
#define TRACE_SYSTEM kperfevents_mm

#if !defined(_TRACE_KPERFEVENTS_MM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_KPERFEVENTS_MM_H

#include <linux/types.h>
#include <linux/tracepoint.h>

DECLARE_TRACE(kperfevents_mm_slowpath,

	TP_PROTO(s32 order, u64 running_duration_ns, u64 spent_duration_ns),

	TP_ARGS(order, running_duration_ns, spent_duration_ns)
);

#endif /* _TRACE_KPERFEVENTS_MM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

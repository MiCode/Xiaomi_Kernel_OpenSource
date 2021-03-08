#undef TRACE_SYSTEM
#define TRACE_SYSTEM kperfevents_sched

#if !defined(_TRACE_KPERFEVENTS_SCHED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_KPERFEVENTS_SCHED_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/sched.h>

DECLARE_TRACE(kperfevents_sched_wait,

	TP_PROTO(struct task_struct *target, u64 delay, bool interruptible),

	TP_ARGS(target, delay, interruptible)
);

#endif /* _TRACE_KPERFEVENTS_SCHED_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM wqlockup
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_WQLOCKUP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_WQLOCKUP_H
#include <trace/hooks/vendor_hooks.h>
/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */
DECLARE_RESTRICTED_HOOK(android_rvh_alloc_workqueue,
	TP_PROTO(struct workqueue_struct *wq, unsigned int *flags, int *max_active),
	TP_ARGS(wq, flags, max_active), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_create_worker,
	TP_PROTO(struct task_struct *p, struct workqueue_attrs *attrs),
	TP_ARGS(p, attrs), 1);

DECLARE_HOOK(android_vh_wq_lockup_pool,
	TP_PROTO(int cpu, unsigned long pool_ts),
	TP_ARGS(cpu, pool_ts));

DECLARE_HOOK(android_vh_wq_queue_work,
	TP_PROTO(struct work_struct *work, char *wq_name, unsigned int wq_flags, unsigned int cpu),
	TP_ARGS(work, wq_name, wq_flags, cpu));

DECLARE_HOOK(android_vh_wq_wake_idle_worker,
	TP_PROTO(struct task_struct *worker, struct work_struct *work),
	TP_ARGS(worker, work));

#ifndef __GENKSYMS__
DECLARE_RESTRICTED_HOOK(android_rvh_alloc_and_link_pwqs,
	TP_PROTO(struct workqueue_struct *wq, int *ret, bool *skip),
	TP_ARGS(wq, ret, skip), 1);
#else
DECLARE_HOOK(android_rvh_alloc_and_link_pwqs,
	TP_PROTO(struct workqueue_struct *wq, int *ret, bool *skip),
	TP_ARGS(wq, ret, skip));
#endif

#endif /* _TRACE_HOOK_WQLOCKUP_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

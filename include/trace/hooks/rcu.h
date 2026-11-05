/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM rcu

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_RCU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_RCU_H

#include <trace/hooks/vendor_hooks.h>
struct rt_mutex;

DECLARE_HOOK(android_vh_sync_rcu_wait_start,
	TP_PROTO(struct task_struct *self),
	TP_ARGS(self));

DECLARE_HOOK(android_vh_sync_rcu_wait_end,
	TP_PROTO(struct task_struct *self),
	TP_ARGS(self));

DECLARE_HOOK(android_vh_rcu_boost_start,
	TP_PROTO(struct rt_mutex *mtx, struct task_struct *t),
	TP_ARGS(mtx, t));

DECLARE_HOOK(android_vh_rcu_boost_end,
	TP_PROTO(struct rt_mutex *mtx, struct task_struct *t),
	TP_ARGS(mtx, t));

#endif /* _TRACE_HOOK_RCU_H */

/* This part must be outside protection */
#include <trace/define_trace.h>


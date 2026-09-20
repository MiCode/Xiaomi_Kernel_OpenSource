/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM gic

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_GIC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_GIC_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_RESTRICTED_HOOK(android_rvh_fiq_dump,
	TP_PROTO(struct pt_regs *regs),
	TP_ARGS(regs), 1);

#endif /* _TRACE_HOOK_GIC_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016, 2018-2019 The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM scm

#if !defined(_TRACE_SCM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCM_H
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/arm-smccc.h>

TRACE_EVENT(scm_call,

	TP_PROTO(const unsigned long *a, const struct arm_smccc_res *r,
		 const s64 delta),

	TP_ARGS(a, r, delta),

	TP_STRUCT__entry(
		__array(u64, args, 8)
		__array(unsigned long, ret, 4)
		__field(s64, delta)
	),

	TP_fast_assign(
		memcpy(__entry->args, a, sizeof(__entry->args));
		__entry->ret[0] = r->a0;
		__entry->ret[1] = r->a1;
		__entry->ret[2] = r->a2;
		__entry->ret[3] = r->a3;
		__entry->delta = delta;
	),

	TP_printk("%3lld [%#llx %#llx %#llx %#llx %#llx %#llx %#llx %#llx] (%#lx %#lx %#lx %#lx)",
		__entry->delta,
		__entry->args[0], __entry->args[1], __entry->args[2],
		__entry->args[3], __entry->args[4], __entry->args[5],
		__entry->args[6], __entry->args[7],
		__entry->ret[0], __entry->ret[1],
		__entry->ret[2], __entry->ret[3])
);
#endif /* _TRACE_SCM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

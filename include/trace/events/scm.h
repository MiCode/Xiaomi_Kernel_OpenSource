/* Copyright (c) 2016, 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM scm

#if !defined(_TRACE_SCM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCM_H
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <soc/qcom/scm.h>

TRACE_EVENT(scm_call_start,

	TP_PROTO(u64 x0, struct scm_desc *p),

	TP_ARGS(x0, p),

	TP_STRUCT__entry(
		__field(u64, x0)
		__field(u32, arginfo)
		__array(u64, args, MAX_SCM_ARGS)
		__field(u64, x5)
	),

	TP_fast_assign(
		__entry->x0		= x0;
		__entry->arginfo	= p->arginfo;
		memcpy(__entry->args, p->args, sizeof(__entry->args));
		__entry->x5		= p->x5;
	),

	TP_printk("func id=%#llx (args: %#x, %#llx, %#llx, %#llx, %#llx)",
		__entry->x0, __entry->arginfo, __entry->args[0],
		__entry->args[1], __entry->args[2], __entry->x5)
);


TRACE_EVENT(scm_call_end,

	TP_PROTO(struct scm_desc *p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__array(u64, ret, MAX_SCM_RETS)
	),

	TP_fast_assign(
		memcpy(__entry->ret, p->ret, sizeof(__entry->ret));
	),

	TP_printk("ret: %#llx, %#llx, %#llx",
		__entry->ret[0], __entry->ret[1], __entry->ret[2])
);
#endif /* _TRACE_SCM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

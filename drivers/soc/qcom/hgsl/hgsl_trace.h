/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM hgsl_trace

#if !defined(_HGSL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _HGSL_TRACE_H

#include <linux/tracepoint.h>

#include "hgsl.h"

TRACE_EVENT(issue_cmd,
	TP_PROTO(uint32_t context_id, uint32_t ts, int ret, char *protocol),

	TP_ARGS(context_id, ts, ret, protocol),

	TP_STRUCT__entry(
			__field(u32, context_id)
			__field(u32, ts)
			__field(int, ret)
			__string(protocol, protocol)
	),

	TP_fast_assign(
			__entry->context_id = context_id;
			__entry->ts = ts;
			__entry->ret = ret;
			__assign_str(protocol, protocol);
	),

	TP_printk("hgsl_issue_cmds, context:%d ts:%d ret:%d protocol:%s",
			__entry->context_id, __entry->ts, __entry->ret,
			__get_str(protocol))
);

TRACE_EVENT(retire_ts,
	TP_PROTO(uint32_t context_id, uint32_t ts),

	TP_ARGS(context_id, ts),

	TP_STRUCT__entry(
			__field(u32, context_id)
			__field(u32, ts)
	),

	TP_fast_assign(
			__entry->context_id = context_id;
			__entry->ts = ts;
	),

	TP_printk("hgsl_retire_ts, context:%d ts:%d",
			__entry->context_id, __entry->ts)
);

TRACE_EVENT(tcsr_isr,
	TP_PROTO(uint32_t status),

	TP_ARGS(status),

	TP_STRUCT__entry(
			__field(u32, status)
	),

	TP_fast_assign(
			__entry->status = status;
	),

	TP_printk("hgsl_db_int, status:%d",
			__entry->status)
);


#endif /* if !defined(_HGSL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>

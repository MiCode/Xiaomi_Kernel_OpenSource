/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM met_vpusys_events
#if !defined(_TRACE_MET_VPUSYS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MET_VPUSYS_EVENTS_H
#include <linux/tracepoint.h>

#define MX_LEN_STR_DESC (128)
TRACE_EVENT(__MET_PACKET__,
	TP_PROTO(unsigned long long wclk, char action, int core,
		int sessid, char *str_desc, int val),
	TP_ARGS(wclk, action, core, sessid, str_desc, val),
	TP_STRUCT__entry(
		__field(unsigned long long, wclk)
		__field(int, action)
		__field(int, core)
		__field(int, sessid)
		__array(char, str_desc, MX_LEN_STR_DESC)
		__field(int, val)
		),
	TP_fast_assign(
		__entry->wclk = wclk;
		__entry->action = action;
		__entry->core = core;
		__entry->sessid = sessid;
		snprintf(__entry->str_desc, MX_LEN_STR_DESC, "%s", str_desc);
		__entry->val = val;
	),
	TP_printk(
		"WCLK=%llu,ACTION=%c,TASK=VPU.internal.core%d,PID=0,SESS=%d,DESC=%s,VAL=%d,",
		__entry->wclk,
		__entry->action,
		__entry->core,
		__entry->sessid,
		__entry->str_desc,
		__entry->val)
);
#endif /* _TRACE_MET_VPUSYS_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE met_vpusys_events
#include <trace/define_trace.h>


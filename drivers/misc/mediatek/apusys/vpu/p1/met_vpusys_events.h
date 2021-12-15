// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM met_vpusys_events
#if !defined(_TRACE_MET_VPUSYS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MET_VPUSYS_EVENTS_H
#include <linux/tracepoint.h>

#ifndef VPU_MET_PM_MAX
#define VPU_MET_PM_MAX 8
#endif

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
		if (snprintf(__entry->str_desc,
			MX_LEN_STR_DESC, "%s", str_desc) < 0)
			__entry->str_desc[0] = '\0';
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

TRACE_EVENT(VPU__polling,
	TP_PROTO(int core, int value1, int value2, int value3, int value4),
	TP_ARGS(core, value1, value2, value3, value4),
	TP_STRUCT__entry(
		__field(int, core)
		__field(int, value1)
		__field(int, value2)
		__field(int, value3)
		__field(int, value4)
		),
	TP_fast_assign(
		__entry->core = core;
		__entry->value1 = value1;
		__entry->value2 = value2;
		__entry->value3 = value3;
		__entry->value4 = value4;
	),
	TP_printk(
		"_id=c%d, instruction_cnt=%d, idma_active=%d,uncached_data_stall=%d, icache_miss_stall=%d",
		__entry->core,
		__entry->value1,
		__entry->value2,
		__entry->value3,
		__entry->value4)
);

TRACE_EVENT(VPU__pm,
	TP_PROTO(int core, const u32 *pm),
	TP_ARGS(core, pm),
	TP_STRUCT__entry(
		__field(int, core)
		__array(u32, pm, VPU_MET_PM_MAX)
		),
	TP_fast_assign(
		__entry->core = core;
		memcpy(__entry->pm, pm, sizeof(__entry->pm));
	),
	TP_printk(
		"_id=c%d, 0:%u, 1:%u, 2:%u, 3:%u, 4:%u, 5:%u, 6:%u, 7:%u",
		__entry->core,
		__entry->pm[0],
		__entry->pm[1],
		__entry->pm[2],
		__entry->pm[3],
		__entry->pm[4],
		__entry->pm[5],
		__entry->pm[6],
		__entry->pm[7])
);

#endif /* _TRACE_MET_VPUSYS_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE met_vpusys_events
#include <trace/define_trace.h>


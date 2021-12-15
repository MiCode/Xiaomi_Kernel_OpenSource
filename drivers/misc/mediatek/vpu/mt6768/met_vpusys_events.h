/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/


/*
 * Take VPU__D2D as an example:
 * Define 2 ftrace event:
 *            1. enter event
 *            2. leave event
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM met_vpusys_events
#if !defined(_TRACE_MET_VPUSYS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MET_VPUSYS_EVENTS_H
#include <linux/tracepoint.h>

#define MX_LEN_STR_DESC (128)
TRACE_EVENT(__MET_PACKET__,
	TP_PROTO(unsigned long long wclk, char action, int core, int pid,
		int sessid, char *str_desc, int val),
	TP_ARGS(wclk, action, core, pid, sessid, str_desc, val),
	TP_STRUCT__entry(
		__field(unsigned long long, wclk)
		__field(int, action)
		__field(int, core)
		__field(int, pid)
		__field(int, sessid)
		__array(char, str_desc, MX_LEN_STR_DESC)
		__field(int, val)
		),
	TP_fast_assign(
		__entry->wclk = wclk;
		__entry->action = action;
		__entry->core = core;
		__entry->pid = pid;
		__entry->sessid = sessid;
		strncpy(__entry->str_desc, str_desc, MX_LEN_STR_DESC);
		__entry->val = val;
	),
	TP_printk(
		"WCLK=%llu,ACTION=%c,TASK=VPU.internal.core%d,PID=%d,SESS=%d,DESC=%s,VAL=%d,",
		__entry->wclk,
		__entry->action,
		__entry->core,
		__entry->pid,
		__entry->sessid,
		__entry->str_desc,
		__entry->val)
);


TRACE_EVENT(VPU__D2D_enter,
	TP_PROTO(int core, int algo_id, int dsp_freq),
	TP_ARGS(core, algo_id, dsp_freq),
	TP_STRUCT__entry(
		__field(int, core)
		__field(int, algo_id)
		__field(int, dsp_freq)
		),
	TP_fast_assign(
		__entry->core = core;
		__entry->algo_id = algo_id;
		__entry->dsp_freq = dsp_freq;
	),
	TP_printk("_id=c%da%d, dsp%d_freq=%d", __entry->core, __entry->algo_id,
		__entry->core, __entry->dsp_freq)
);

TRACE_EVENT(VPU__D2D_leave,
	TP_PROTO(int core, int algo_id, int dummy),
	TP_ARGS(core, algo_id, dummy),
	TP_STRUCT__entry(
		__field(int, core)
		__field(int, algo_id)
		__field(int, dummy)
	),
	TP_fast_assign(
		__entry->core = core;
		__entry->algo_id = algo_id;
		__entry->dummy = dummy;
	),
	TP_printk("_id=c%da%d", __entry->core, __entry->algo_id)
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
		"_id=c%d, instruction_cnt=%d, idma_active=%d, uncached_data_stall=%d, icache_miss_stall=%d",
		__entry->core,
		__entry->value1,
		__entry->value2,
		__entry->value3,
		__entry->value4)
);

TRACE_EVENT(VPU__DVFS,
	TP_PROTO(int vcore_opp, int dsp_freq, int ipu_if_freq, int dsp1_freq,
		int dsp2_freq),
	TP_ARGS(vcore_opp, dsp_freq, ipu_if_freq, dsp1_freq, dsp2_freq),
	TP_STRUCT__entry(
		__field(int, vcore_opp)
		__field(int, dsp_freq)
		__field(int, ipu_if_freq)
		__field(int, dsp1_freq)
		__field(int, dsp2_freq)
		),
	TP_fast_assign(
		__entry->vcore_opp = vcore_opp;
		__entry->dsp_freq = dsp_freq;
		__entry->ipu_if_freq = ipu_if_freq;
		__entry->dsp1_freq = dsp1_freq;
		__entry->dsp2_freq = dsp2_freq;
	),
	TP_printk(
		"vcore_opp=%d, dsp_freq=%d, ipu_if_freq=%d, dsp1_freq=%d, dsp2_freq=%d",
		__entry->vcore_opp,
		__entry->dsp_freq,
		__entry->ipu_if_freq,
		__entry->dsp1_freq,
		__entry->dsp2_freq)
);


#endif /* _TRACE_MET_VPUSYS_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE met_vpusys_events
#include <trace/define_trace.h>


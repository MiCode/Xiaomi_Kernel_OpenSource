/*
 * Copyright (C) 2020 MediaTek Inc.
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
#define TRACE_SYSTEM vpu_events
#if !defined(_VPU_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _VPU_EVENTS_H
#include <linux/tracepoint.h>

TRACE_EVENT(vpu_cmd,
	TP_PROTO(int core, int prio, char *algo, int cmd, uint64_t start_time,
		int ret, int algo_ret, int result),
	TP_ARGS(core, prio, algo, cmd, start_time, ret, algo_ret, result),
	TP_STRUCT__entry(
		__field(int, core)
		__field(int, prio)
		__array(char, algo, ALGO_NAMELEN)
		__field(int, cmd)
		__field(uint64_t, start_time)
		__field(int, ret)
		__field(int, algo_ret)
		__field(int, result)
	),
	TP_fast_assign(
		__entry->core = core;
		__entry->prio = prio;
		__entry->start_time = start_time;
		snprintf(__entry->algo, ALGO_NAMELEN, "%s", algo);
		__entry->cmd = cmd;
		__entry->ret = ret;
		__entry->algo_ret = algo_ret;
		__entry->result = result;
	),
	TP_printk(
		"vpu%d,prio=%d,%s,cmd=%xh,start_time=%lld,ret=%d,alg_ret=%d,result=%d",
		__entry->core,
		__entry->prio,
		__entry->algo,
		__entry->cmd,
		__entry->start_time,
		__entry->ret,
		__entry->algo_ret,
		__entry->result)
);

TRACE_EVENT(vpu_dmp,
	TP_PROTO(int core, char *stage, uint32_t pc),
	TP_ARGS(core, stage, pc),
	TP_STRUCT__entry(
		__field(int, core)
		__array(char, stage, STAGE_NAMELEN)
		__field(uint32_t, pc)
	),
	TP_fast_assign(
		__entry->core = core;
		snprintf(__entry->stage, STAGE_NAMELEN, "%s", stage);
		__entry->pc = pc;
	),
	TP_printk(
		"vpu%d,dump=%s,pc=0x%x\n",
		__entry->core,
		__entry->stage,
		__entry->pc)
);
#endif /* if !defined(_VPU_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE vpu_events
#include <trace/define_trace.h>


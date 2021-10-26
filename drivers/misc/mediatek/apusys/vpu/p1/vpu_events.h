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
	TP_PROTO(int core, int prio, char *algo, int cmd, int boost,
		uint64_t start_time, int ret, int algo_ret, int result),
	TP_ARGS(core, prio, algo, cmd, boost, start_time, ret, algo_ret,
		result),
	TP_STRUCT__entry(
		__field(int, core)
		__field(int, prio)
		__array(char, algo, ALGO_NAMELEN)
		__field(int, cmd)
		__field(int, boost)
		__field(uint64_t, start_time)
		__field(int, ret)
		__field(int, algo_ret)
		__field(int, result)
	),
	TP_fast_assign(
		__entry->core = core;
		__entry->prio = prio;
		__entry->start_time = start_time;
		if (snprintf(__entry->algo, ALGO_NAMELEN, "%s", algo) <= 0)
			__entry->algo[0] = '\0';
		__entry->cmd = cmd;
		__entry->boost = boost;
		__entry->ret = ret;
		__entry->algo_ret = algo_ret;
		__entry->result = result;
	),
	TP_printk(
		"vpu%d,prio=%d,%s,cmd=%xh,boost=%d,start_time=%lld,ret=%d,alg_ret=%d,result=%d",
		__entry->core,
		__entry->prio,
		__entry->algo,
		__entry->cmd,
		__entry->boost,
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
		if (snprintf(__entry->stage, STAGE_NAMELEN, "%s", stage) <= 0)
			__entry->stage[0] = '\0';
		__entry->pc = pc;
	),
	TP_printk(
		"vpu%d,dump=%s,pc=0x%x",
		__entry->core,
		__entry->stage,
		__entry->pc)
);

TRACE_EVENT(vpu_wait,
	TP_PROTO(int core, uint32_t donest, uint32_t info00, uint32_t info25,
		uint32_t pc),
	TP_ARGS(core, donest, info00, info25, pc),
	TP_STRUCT__entry(
		__field(int, core)
		__field(uint32_t, donest)
		__field(uint32_t, info00)
		__field(uint32_t, info25)
		__field(uint32_t, pc)
	),
	TP_fast_assign(
		__entry->core = core;
		__entry->donest = donest;
		__entry->info00 = info00;
		__entry->info25 = info25;
		__entry->pc = pc;
	),
	TP_printk(
		"vpu%d,donest=0x%x,info00=0x%x,info25=0x%x,pc=0x%x",
		__entry->core,
		__entry->donest,
		__entry->info00,
		__entry->info25,
		__entry->pc)
);

#endif /* if !defined(_VPU_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE vpu_events
#include <trace/define_trace.h>


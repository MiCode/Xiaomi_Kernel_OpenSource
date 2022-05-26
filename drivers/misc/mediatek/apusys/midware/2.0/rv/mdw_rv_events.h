/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mdw_rv_events
#if !defined(__MDW_RV_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __MDW_RV_EVENTS_H__
#include <linux/tracepoint.h>

#define MDW_TAG_CMD_PRINT \
	"%s,pid=%d,tgid=%d,uid=0x%llx,kid=0x%llx,rvid=0x%llx,"\
	"num_subcmds=%u,num_cmdbufs=%u,"\
	"priority=%u,softlimit=%u,pwr_dtime=%u,"\
	"sc_rets=0x%llx"\

TRACE_EVENT(mdw_rv_cmd,
	TP_PROTO(bool done,
		pid_t pid,
		pid_t tgid,
		uint64_t uid,
		uint64_t kid,
		uint64_t rvid,
		uint32_t num_subcmds,
		uint32_t num_cmdbufs,
		uint32_t priority,
		uint32_t softlimit,
		uint32_t pwr_dtime,
		uint64_t sc_rets
		),
	TP_ARGS(done, pid, tgid, uid, kid, rvid,
		num_subcmds, num_cmdbufs,
		priority, softlimit,
		pwr_dtime, sc_rets
		),
	TP_STRUCT__entry(
		__field(bool, done)
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__field(uint64_t, uid)
		__field(uint64_t, kid)
		__field(uint64_t, rvid)
		__field(uint32_t, num_subcmds)
		__field(uint32_t, num_cmdbufs)
		__field(uint32_t, priority)
		__field(uint32_t, softlimit)
		__field(uint32_t, pwr_dtime)
		__field(uint64_t, sc_rets)
	),
	TP_fast_assign(
		__entry->done = done;
		__entry->pid = pid;
		__entry->tgid = tgid;
		__entry->uid = uid;
		__entry->kid = kid;
		__entry->rvid = rvid;
		__entry->num_subcmds = num_subcmds;
		__entry->num_cmdbufs = num_cmdbufs;
		__entry->priority = priority;
		__entry->softlimit = softlimit;
		__entry->pwr_dtime = pwr_dtime;
		__entry->sc_rets = sc_rets;
	),
	TP_printk(
		MDW_TAG_CMD_PRINT,
		__entry->done == false ? "start":"end",
		__entry->pid,
		__entry->tgid,
		__entry->uid,
		__entry->kid,
		__entry->rvid,
		__entry->num_subcmds,
		__entry->num_cmdbufs,
		__entry->priority,
		__entry->softlimit,
		__entry->pwr_dtime,
		__entry->sc_rets
	)
);
#undef MDW_TAG_CMD_PRINT

#endif /* #if !defined(__MDW_RV_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ) */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mdw_rv_events
#include <trace/define_trace.h>


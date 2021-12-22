// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mdw_events
#if !defined(__MDW_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __MDW_EVENTS_H__
#include <linux/tracepoint.h>

#define MDW_TAG_CMD_PRINT "%s,pid=%d,tgid=%d,cmd_id=0x%llx,"\
	"sc_info=0x%llx,dev_name=%s,"\
	"multi_info=0x%llx,"\
	"exec_param=0x%llx,"\
	"tcm_info=0x%llx,"\
	"boost=%u,ip_time=%u,ret=%d\n"\

TRACE_EVENT(mdw_cmd,
	TP_PROTO(uint32_t done, pid_t pid, pid_t tgid,
		uint64_t cmd_id,
		uint64_t sc_info,
		char *dev_name,
		uint64_t multi_info,
		uint64_t exec_info,
		uint64_t tcm_info,
		uint32_t boost, uint32_t ip_time,
		int ret
		),
	TP_ARGS(done, pid, tgid, cmd_id, sc_info, dev_name,
		multi_info, exec_info, tcm_info,
		boost, ip_time, ret
		),
	TP_STRUCT__entry(
		__field(uint32_t, done)
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__field(uint64_t, cmd_id)
		__field(uint64_t, sc_info)
		__array(char, dev_name, 16)
		__field(uint64_t, multi_info)
		__field(uint64_t, exec_info)
		__field(uint64_t, tcm_info)
		__field(uint32_t, boost)
		__field(uint32_t, ip_time)
		__field(uint32_t, ret)
	),
	TP_fast_assign(
		__entry->done = done;
		__entry->pid = pid;
		__entry->tgid = tgid;
		__entry->cmd_id = cmd_id;
		__entry->sc_info = sc_info;
		if (snprintf(__entry->dev_name, 16,
			"%s", dev_name) < 0)
			return;
		__entry->multi_info = multi_info;
		__entry->multi_info = exec_info;
		__entry->multi_info = tcm_info;
		__entry->boost = boost;
		__entry->done = ip_time;
		__entry->ret = ret;
	),
	TP_printk(
		MDW_TAG_CMD_PRINT,
		__entry->done == 0 ? "start":"end",
		__entry->pid,
		__entry->tgid,
		__entry->cmd_id,
		__entry->sc_info,
		__entry->dev_name,
		__entry->multi_info,
		__entry->exec_info,
		__entry->tcm_info,
		__entry->boost,
		__entry->ip_time,
		__entry->ret
	)
);
#undef MDW_TAG_CMD_PRINT

#endif /* #if !defined(_MDW_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ) */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mdw_events
#include <trace/define_trace.h>


/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM lpm_trace_event

#if !defined(__LPM_TRACE_EVENT_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __LPM_TRACE_EVENT_H__

#include <linux/tracepoint.h>
#include "lpm_trace_def.h"

/* Define the trace event bellow as you need */

TRACE_EVENT(lpm_trace,
	TP_PROTO(struct lpm_trace_debug_t *t),
	TP_ARGS(t),
	TP_STRUCT__entry(
		__array(char, _datas, LPM_TRACE_EVENT_MESG_MAX)
	),
	TP_fast_assign(
		memcpy(__entry->_datas, t->_datas,
		       LPM_TRACE_EVENT_MESG_MAX);
	),
	TP_printk("%s", (char *)__entry->_datas)
);

TRACE_EVENT(SPM__resource_req_0,
	TP_PROTO(unsigned int md, unsigned int conn, unsigned int scp,
		unsigned int adsp, unsigned int ufs, unsigned int msdc,
		unsigned int disp, unsigned int apu, unsigned int spm),
	TP_ARGS(md, conn, scp, adsp, ufs, msdc, disp, apu, spm),
	TP_STRUCT__entry(__field(unsigned int, md)
		__field(unsigned int, conn)
		__field(unsigned int, scp)
		__field(unsigned int, adsp)
		__field(unsigned int, ufs)
		__field(unsigned int, msdc)
		__field(unsigned int, disp)
		__field(unsigned int, apu)
		__field(unsigned int, spm)
	),
	TP_fast_assign(__entry->md = md;
		__entry->conn = conn;
		__entry->scp = scp;
		__entry->adsp = adsp;
		__entry->ufs = ufs;
		__entry->msdc = msdc;
		__entry->disp = disp;
		__entry->apu = apu;
		__entry->spm = spm;
	),
	TP_printk("%d, %d, %d, %d, %d, %d, %d, %d, %d",
		__entry->md, __entry->conn,
		__entry->scp, __entry->adsp,
		__entry->ufs, __entry->msdc,
		__entry->disp, __entry->apu,
		__entry->spm)
);
#endif

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE

#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE lpm_trace_event

#include <trace/define_trace.h>


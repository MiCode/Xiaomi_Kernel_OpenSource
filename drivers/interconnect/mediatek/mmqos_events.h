/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmqos_events

#if !defined(_TRACE_MMQOS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMQOS_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(mmqos__larb_port_avg_bw,
	TP_PROTO(int larb, int port, int avg_bw),
	TP_ARGS(larb, port, avg_bw),
	TP_STRUCT__entry(
		__field(int, larb)
		__field(int, port)
		__field(int, avg_bw)
	),
	TP_fast_assign(
		__entry->larb = larb;
		__entry->port = port;
		__entry->avg_bw = avg_bw;
	),
	TP_printk("larb%d_%d=%d",
		(int)__entry->larb,
		(int)__entry->port,
		(int)__entry->avg_bw)
);
TRACE_EVENT(mmqos__larb_port_peak_bw,
	TP_PROTO(int larb, int port, int peak_bw),
	TP_ARGS(larb, port, peak_bw),
	TP_STRUCT__entry(
		__field(int, larb)
		__field(int, port)
		__field(int, peak_bw)
	),
	TP_fast_assign(
		__entry->larb = larb;
		__entry->port = port;
		__entry->peak_bw = peak_bw;
	),
	TP_printk("larb%d_%d=%d",
		(int)__entry->larb,
		(int)__entry->port,
		(int)__entry->peak_bw)
);
TRACE_EVENT(mmqos__larb_avg_bw,
	TP_PROTO(int larb, int bw),
	TP_ARGS(larb, bw),
	TP_STRUCT__entry(
		__field(int, larb)
		__field(int, bw)
	),
	TP_fast_assign(
		__entry->larb = larb;
		__entry->bw = bw;
	),
	TP_printk("larb%d=%d",
		(int)__entry->larb,
		(int)__entry->bw)
);
TRACE_EVENT(mmqos__larb_peak_bw,
	TP_PROTO(int larb, int bw),
	TP_ARGS(larb, bw),
	TP_STRUCT__entry(
		__field(int, larb)
		__field(int, bw)
	),
	TP_fast_assign(
		__entry->larb = larb;
		__entry->bw = bw;
	),
	TP_printk("larb%d=%d",
		(int)__entry->larb,
		(int)__entry->bw)
);
TRACE_EVENT(mmqos__chn_bw,
	TP_PROTO(int comm_id, int chn_id, int s_r, int s_w, int h_r, int h_w),
	TP_ARGS(comm_id, chn_id, s_r, s_w, h_r, h_w),
	TP_STRUCT__entry(
		__field(int, comm_id)
		__field(int, chn_id)
		__field(int, s_r)
		__field(int, s_w)
		__field(int, h_r)
		__field(int, h_w)
	),
	TP_fast_assign(
		__entry->comm_id = comm_id;
		__entry->chn_id = chn_id;
		__entry->s_r = s_r;
		__entry->s_w = s_w;
		__entry->h_r = h_r;
		__entry->h_w = h_w;
	),
	TP_printk("comm%d_%d_s_r=%d, comm%d_%d_s_w=%d, comm%d_%d_h_r=%d, comm%d_%d_h_w=%d",
		(int)__entry->comm_id,
		(int)__entry->chn_id,
		(int)__entry->s_r,
		(int)__entry->comm_id,
		(int)__entry->chn_id,
		(int)__entry->s_w,
		(int)__entry->comm_id,
		(int)__entry->chn_id,
		(int)__entry->h_r,
		(int)__entry->comm_id,
		(int)__entry->chn_id,
		(int)__entry->h_w)
);
#endif /* _TRACE_MMQOS_EVENTS_H */

#undef TRACE_INCLUDE_FILE
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mmqos_events

/* This part must be outside protection */
#include <trace/define_trace.h>

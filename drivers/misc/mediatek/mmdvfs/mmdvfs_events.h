/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmdvfs_events

#if !defined(_TRACE_MMDVFS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMDVFS_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(mmdvfs__record_opp_v1,
	TP_PROTO(unsigned long rec, unsigned long opp),
	TP_ARGS(rec, opp),
	TP_STRUCT__entry(
		__field(unsigned long, rec)
		__field(unsigned long, opp)
	),
	TP_fast_assign(
		__entry->rec = rec;
		__entry->opp = opp;
	),
	TP_printk("rec_%lu=%lu",
		(unsigned long)__entry->rec,
		(unsigned long)__entry->opp)
);

TRACE_EVENT(mmdvfs__record_opp_v3,
	TP_PROTO(unsigned long rec, unsigned long opp),
	TP_ARGS(rec, opp),
	TP_STRUCT__entry(
		__field(unsigned long, rec)
		__field(unsigned long, opp)
	),
	TP_fast_assign(
		__entry->rec = rec;
		__entry->opp = opp;
	),
	TP_printk("rec_%lu=%lu",
		(unsigned long)__entry->rec,
		(unsigned long)__entry->opp)
);

TRACE_EVENT(mmdvfs__request_opp_v3,
	TP_PROTO(unsigned long user, unsigned long opp),
	TP_ARGS(user, opp),
	TP_STRUCT__entry(
		__field(unsigned long, user)
		__field(unsigned long, opp)
	),
	TP_fast_assign(
		__entry->user = user;
		__entry->opp = opp;
	),
	TP_printk("user_%lu=%lu",
		(unsigned long)__entry->user,
		(unsigned long)__entry->opp)
);

#endif /* _TRACE_MMDVFS_EVENTS_H */

#undef TRACE_INCLUDE_FILE
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mmdvfs_events

/* This part must be outside protection */
#include <trace/define_trace.h>

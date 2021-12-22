// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#define MNOC_PMU_POLL_STR1 "c1=%u, c2=%u, c3=%u, c4=%u, c5=%u, c6=%u, "
#define MNOC_PMU_POLL_STR2 "c7=%u, c8=%u, c9=%u, c10=%u, c11=%u, "
#define MNOC_PMU_POLL_STR3 "c12=%u, c13=%u, c14=%u, c15=%u, c16=%u"
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mnoc_met_events
#if !defined(_TRACE_MNOC_MET_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MNOC_MET_EVENTS_H
#include <linux/tracepoint.h>
#include "mnoc_hw.h"
TRACE_EVENT(mnoc_pmu_polling,
	TP_PROTO(u32 *c),
	TP_ARGS(c),
	TP_STRUCT__entry(
		__array(u32, c, NR_MNOC_PMU_CNTR)
		),
	TP_fast_assign(
		memcpy(__entry->c, c, NR_MNOC_PMU_CNTR * sizeof(u32));
	),
	TP_printk(MNOC_PMU_POLL_STR1 MNOC_PMU_POLL_STR2 MNOC_PMU_POLL_STR3,
		__entry->c[0],
		__entry->c[1],
		__entry->c[2],
		__entry->c[3],
		__entry->c[4],
		__entry->c[5],
		__entry->c[6],
		__entry->c[7],
		__entry->c[8],
		__entry->c[9],
		__entry->c[10],
		__entry->c[11],
		__entry->c[12],
		__entry->c[13],
		__entry->c[14],
		__entry->c[15])
);

#endif /* _TRACE_MNOC_MET_EVENTS_H */
/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mnoc_met_events
#include <trace/define_trace.h>

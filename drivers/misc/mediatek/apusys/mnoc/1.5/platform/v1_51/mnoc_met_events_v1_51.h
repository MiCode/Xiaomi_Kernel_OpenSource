/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#define MNOC_PMU_POLL_STR1 "c1=%u, c2=%u, c3=%u, c4=%u, c5=%u, c6=%u, "
#define MNOC_PMU_POLL_STR2 "c7=%u, c8=%u, c9=%u, c10=%u, c11=%u, "
#define MNOC_PMU_POLL_STR3 "c12=%u, c13=%u, c14=%u, c15=%u, c16=%u, "

#define MNOC_PMU_POLL_STR4 "c17=%u, c18=%u, c19=%u, c20=%u, c21=%u, c22=%u, "
#define MNOC_PMU_POLL_STR5 "c23=%u, c24=%u, c25=%u, c26=%u, c27=%u, "
#define MNOC_PMU_POLL_STR6 "c28=%u, c29=%u, c30=%u, c31=%u, c32=%u, "

#define MNOC_PMU_POLL_STR7 "c33=%u, c34=%u, c35=%u, c36=%u, c37=%u, c38=%u, "
#define MNOC_PMU_POLL_STR8 "c39=%u, c40=%u, c41=%u, c42=%u, c43=%u, "
#define MNOC_PMU_POLL_STR9 "c44=%u, c45=%u, c46=%u, c47=%u, c48=%u, "

#define MNOC_PMU_POLL_STR10 "c49=%u, c50=%u, c51=%u, c52=%u, c53=%u, c54=%u, "
#define MNOC_PMU_POLL_STR11 "c55=%u, c56=%u, c57=%u, c58=%u, c59=%u, "
#define MNOC_PMU_POLL_STR12 "c60=%u, c61=%u, c62=%u, c63=%u, c64=%u, "

#define MNOC_PMU_POLL_STR13 "c65=%u, c66=%u, c67=%u, c68=%u, c69=%u, c70=%u, "
#define MNOC_PMU_POLL_STR14 "c71=%u, c72=%u, c73=%u, c74=%u, c75=%u, "
#define MNOC_PMU_POLL_STR15 "c76=%u, c77=%u, c78=%u, c79=%u, c80=%u"
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mnoc_met_events
#if !defined(_TRACE_MNOC_MET_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MNOC_MET_EVENTS_H
#include <linux/tracepoint.h>
#include "mnoc_hw.h"
TRACE_EVENT(mnoc_pmu_polling,
	TP_PROTO(u32 c[NR_MNOC_PMU_CNTR]),
	TP_ARGS(c),
	TP_STRUCT__entry(
		__array(u32, c, NR_MNOC_PMU_CNTR)
		),
	TP_fast_assign(
		memcpy(__entry->c, c, NR_MNOC_PMU_CNTR * sizeof(u32));
	),
	TP_printk(MNOC_PMU_POLL_STR1 MNOC_PMU_POLL_STR2 MNOC_PMU_POLL_STR3
		MNOC_PMU_POLL_STR4 MNOC_PMU_POLL_STR5 MNOC_PMU_POLL_STR6
		MNOC_PMU_POLL_STR7 MNOC_PMU_POLL_STR8 MNOC_PMU_POLL_STR9
		MNOC_PMU_POLL_STR10 MNOC_PMU_POLL_STR11 MNOC_PMU_POLL_STR12
		MNOC_PMU_POLL_STR13 MNOC_PMU_POLL_STR14 MNOC_PMU_POLL_STR15,
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
		__entry->c[15],
		__entry->c[16],
		__entry->c[17],
		__entry->c[18],
		__entry->c[19],
		__entry->c[20],
		__entry->c[21],
		__entry->c[22],
		__entry->c[23],
		__entry->c[24],
		__entry->c[25],
		__entry->c[26],
		__entry->c[27],
		__entry->c[28],
		__entry->c[29],
		__entry->c[30],
		__entry->c[31],
		__entry->c[32],
		__entry->c[33],
		__entry->c[34],
		__entry->c[35],
		__entry->c[36],
		__entry->c[37],
		__entry->c[38],
		__entry->c[39],
		__entry->c[40],
		__entry->c[41],
		__entry->c[42],
		__entry->c[43],
		__entry->c[44],
		__entry->c[45],
		__entry->c[46],
		__entry->c[47],
		__entry->c[48],
		__entry->c[49],
		__entry->c[50],
		__entry->c[51],
		__entry->c[52],
		__entry->c[53],
		__entry->c[54],
		__entry->c[55],
		__entry->c[56],
		__entry->c[57],
		__entry->c[58],
		__entry->c[59],
		__entry->c[60],
		__entry->c[61],
		__entry->c[62],
		__entry->c[63],
		__entry->c[64],
		__entry->c[65],
		__entry->c[66],
		__entry->c[67],
		__entry->c[68],
		__entry->c[69],
		__entry->c[70],
		__entry->c[71],
		__entry->c[72],
		__entry->c[73],
		__entry->c[74],
		__entry->c[75],
		__entry->c[76],
		__entry->c[77],
		__entry->c[78],
		__entry->c[79])
);

#endif /* _TRACE_MNOC_MET_EVENTS_H */
/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mnoc_met_events
#include <trace/define_trace.h>

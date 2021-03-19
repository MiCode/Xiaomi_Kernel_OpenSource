/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define MDLA_POLL_STR1 "_id=c%d, c1=%u, c2=%u, c3=%u, c4=%u, c5=%u, "
#define MDLA_POLL_STR2 "c6=%u, c7=%u, c8=%u, c9=%u, c10=%u, c11=%u, "
#define MDLA_POLL_STR3 "c12=%u, c13=%u, c14=%u, c15=%u"

#undef TRACE_SYSTEM
#define TRACE_SYSTEM met_mdlasys_events
#if !defined(_TRACE_MET_MDLASYS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MET_MDLASYS_EVENTS_H
#include <linux/tracepoint.h>
#include <linux/types.h>
#include <utilities/mdla_util.h>

TRACE_EVENT(mdla_polling,
	TP_PROTO(u32 core_id,
		u32 *c),
	TP_ARGS(core_id, c),
	TP_STRUCT__entry(
		__field(u32, core_id)
		__array(u32, c, MDLA_PMU_COUNTERS)
		),
	TP_fast_assign(
		__entry->core_id = core_id;
		memcpy(__entry->c, c, MDLA_PMU_COUNTERS * sizeof(u32));
	),
	TP_printk(MDLA_POLL_STR1 MDLA_POLL_STR2 MDLA_POLL_STR3,
		__entry->core_id,
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
		__entry->c[14])
);

TRACE_EVENT(mdla_cmd_enter,
	TP_PROTO(u32 core_id, int vmdla_opp, int dsp_freq,
		int ipu_if_freq, int mdla_freq),
	TP_ARGS(core_id, vmdla_opp, dsp_freq, ipu_if_freq,
		mdla_freq),
	TP_STRUCT__entry(
		__field(u32, core_id)
		__field(int, vmdla_opp)
		__field(int, dsp_freq)
		__field(int, ipu_if_freq)
		__field(int, mdla_freq)
		),
	TP_fast_assign(
		__entry->core_id = core_id;
		__entry->vmdla_opp = vmdla_opp;
		__entry->dsp_freq = dsp_freq;
		__entry->ipu_if_freq = ipu_if_freq;
		__entry->mdla_freq = mdla_freq;
	),
	TP_printk("_id=c%d, %s=%d, %s=%d, %s=%d, %s=%d",
			__entry->core_id,
			"vmdla_opp", __entry->vmdla_opp,
			"dsp_freq", __entry->dsp_freq,
			"ipu_if_freq", __entry->ipu_if_freq,
			"mdla_freq", __entry->mdla_freq)
);

TRACE_EVENT(mdla_cmd_leave,
	TP_PROTO(u32 core_id, int dummy),
	TP_ARGS(core_id, dummy),
	TP_STRUCT__entry(
		__field(u32, core_id)
		__field(int, dummy)
	),
	TP_fast_assign(
		__entry->core_id = core_id;
		__entry->dummy = dummy;
	),
	TP_printk("_id=c%d", __entry->core_id)
);

#endif /* _TRACE_MET_MDLASYS_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
//#define TRACE_INCLUDE_PATH ../../drivers/misc/mediatek/apusys/mdla/utilities/
#define TRACE_INCLUDE_PATH ./utilities
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE met_mdlasys_events
#include <trace/define_trace.h>



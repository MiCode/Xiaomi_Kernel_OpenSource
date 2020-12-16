/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM apu_power_events
#if !defined(_TRACE_APUSYS_FREQ_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_APUSYS_FREQ_EVENTS_H
#include <linux/tracepoint.h>
#include "apupw_tag.h"
#include "apu_common.h"

TRACE_EVENT(APUSYS_DFS,
	TP_PROTO(struct apupwr_tag_pwr *pw),
	TP_ARGS(pw),
	TP_STRUCT__entry(
		__field(unsigned int, dsp_freq)
		__field(unsigned int, dsp1_freq)
		__field(unsigned int, dsp2_freq)
		__field(unsigned int, dsp3_freq)
		__field(unsigned int, mdla0_freq)
		__field(unsigned int, mdla1_freq)
		__field(unsigned int, ipuif_freq)
	),
	TP_fast_assign(
		__entry->dsp_freq = pw->dsp_freq;
		__entry->dsp1_freq = pw->dsp1_freq;
		__entry->dsp2_freq = pw->dsp2_freq;
		__entry->dsp3_freq = pw->dsp3_freq;
		__entry->mdla0_freq = pw->dsp5_freq;
		__entry->mdla1_freq = pw->dsp6_freq;
		__entry->ipuif_freq = pw->ipuif_freq;
	),
	TP_printk("conn=%d,vpu0=%d,vpu1=%d,vpu2=%d,mdla0=%d,mdla1=%d,ipuif=%d",
		TOMHZ(__entry->dsp_freq), TOMHZ(__entry->dsp1_freq),
		TOMHZ(__entry->dsp2_freq), TOMHZ(__entry->dsp3_freq),
		TOMHZ(__entry->mdla0_freq), TOMHZ(__entry->mdla1_freq),
		TOMHZ(__entry->ipuif_freq))
);

#endif /* _TRACE_APUSYS_FREQ_EVENTS_H */
/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE events/apu_power_events
#include <trace/define_trace.h>

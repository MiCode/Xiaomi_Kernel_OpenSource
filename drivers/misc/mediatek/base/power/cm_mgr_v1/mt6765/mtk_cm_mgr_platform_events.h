/*
 * Copyright (C) 2017 MediaTek Inc.
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
#define TRACE_SYSTEM cm_mgr_events

#if !defined(_TRACE_MTK_CM_MGR_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_CM_MGR_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(CM_MGR__stall_raio_0,
	TP_PROTO(unsigned int ratio_ratio),
	TP_ARGS(ratio_ratio),
	TP_STRUCT__entry(
		__field(unsigned int, ratio_ratio)
	),
	TP_fast_assign(
		__entry->ratio_ratio = ratio_ratio;
	),
	TP_printk("mcucfg_reg__MP0_CPU_AVG_STALL_RATIO=%d",
		__entry->ratio_ratio)
);

TRACE_EVENT(CM_MGR__stall_raio_1,
	TP_PROTO(unsigned int ratio_ratio),
	TP_ARGS(ratio_ratio),
	TP_STRUCT__entry(
		__field(unsigned int, ratio_ratio)
	),
	TP_fast_assign(
		__entry->ratio_ratio = ratio_ratio;
	),
	TP_printk("mcucfg_reg__MP1_CPU_AVG_STALL_RATIO=%d",
		__entry->ratio_ratio)
);

#endif /* _TRACE_MTK_CM_MGR_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ./
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_cm_mgr_platform_events
#include <trace/define_trace.h>



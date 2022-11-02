/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_swpm_events

#if !defined(_SWPM_TRACKER_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _SWPM_TRACKER_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(swpm_power,
	TP_PROTO(char *power),
	TP_ARGS(power),
	TP_STRUCT__entry(
		__string(power_str, power)
	),
	TP_fast_assign(
		__assign_str(power_str, power);
	),
	TP_printk("%s", __get_str(power_str))
);

TRACE_EVENT(swpm_power_idx,
	TP_PROTO(char *power_idx),
	TP_ARGS(power_idx),
	TP_STRUCT__entry(
		__string(power_idx_str, power_idx)
	),
	TP_fast_assign(
		__assign_str(power_idx_str, power_idx);
	),
	TP_printk("%s", __get_str(power_idx_str))
);

#endif /* _SWPM_TRACKER_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE swpm_tracker_trace
#include <trace/define_trace.h>

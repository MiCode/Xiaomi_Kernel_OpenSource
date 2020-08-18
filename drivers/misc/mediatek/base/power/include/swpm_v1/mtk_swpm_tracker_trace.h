/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#if !defined(_SWPM_TRACKER_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _SWPM_TRACKER_TRACE_H

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_swpm_tracker
#define TRACE_INCLUDE_FILE mtk_swpm_tracker_trace

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

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_swpm_tracker_trace
/* This part must be outside protection */
#include <trace/define_trace.h>

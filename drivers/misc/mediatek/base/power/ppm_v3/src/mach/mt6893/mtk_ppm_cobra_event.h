/*
 * Copyright (C) 2020 MediaTek Inc.
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
#define TRACE_SYSTEM cobra_events

#if !defined(_TRACE_MTK_COBRA_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_COBRA_EVENTS_H

#include <linux/tracepoint.h>



TRACE_EVENT(PPM__cobra_setting,
		TP_PROTO(unsigned int cur_power,
			int delta_pwr,
			unsigned int cl,
			unsigned int opp,
			unsigned int opp_pwr),
		TP_ARGS(cur_power, delta_pwr, cl, opp, opp_pwr),
		TP_STRUCT__entry(
			__field(unsigned int, cur_power)
			__field(int, delta_pwr)
			__field(unsigned int, cl)
			__field(unsigned int, opp)
			__field(unsigned int, opp_pwr)
			),
		TP_fast_assign(
			__entry->cur_power = cur_power;
			__entry->delta_pwr = delta_pwr;
			__entry->cl = cl;
			__entry->opp = opp;
			__entry->opp_pwr = opp_pwr;
			),
		TP_printk("cur_pwr=%d delta_pwr=%d cl=%d opp=%d opp_pwr: %d",
				__entry->cur_power, __entry->delta_pwr,
				__entry->cl, __entry->opp, __entry->opp_pwr)
);

#endif /* _TRACE_MTK_COBRA_EVENTS_H */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ./
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_ppm_cobra_event
#include <trace/define_trace.h>


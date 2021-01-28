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

TRACE_EVENT(CM_MGR__stall_ratio,
	TP_PROTO(int _id,
		unsigned int _ratio),
	TP_ARGS(_id,
		_ratio),
	TP_STRUCT__entry(
		__field(int, _id)
		__field(unsigned int, _ratio)
	),
	TP_fast_assign(
		__entry->_id = _id;
		__entry->_ratio = _ratio;
	),
	TP_printk("CPU0_config_reg__CPU_AVG_STALL_RATIO=%d, _id=%d",
		__entry->_ratio, __entry->_id)
);

TRACE_EVENT(CM_MGR__perf_hint,
	TP_PROTO(int _force,
		int _enable,
		int _opp,
		int _base,
		int _count,
		int _foece_count),
	TP_ARGS(_force,
		_enable,
		_opp,
		_base,
		_count,
		_foece_count),
	TP_STRUCT__entry(
		__field(int, _force)
		__field(int, _enable)
		__field(int, _opp)
		__field(int, _base)
		__field(int, _count)
		__field(int, _foece_count)
	),
	TP_fast_assign(
		__entry->_force = _force;
		__entry->_enable = _enable;
		__entry->_opp = _opp;
		__entry->_base = _base;
		__entry->_count = _count;
		__entry->_foece_count = _foece_count;
	),
	TP_printk("cm_mgr perf hint %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d",
			"force", __entry->_force,
			"enable", __entry->_enable,
			"opp", __entry->_opp,
			"base", __entry->_base,
			"count", __entry->_count,
			"force_count", __entry->_foece_count)
);

#endif /* _TRACE_MTK_CM_MGR_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ./
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_cm_mgr_platform_events
#include <trace/define_trace.h>



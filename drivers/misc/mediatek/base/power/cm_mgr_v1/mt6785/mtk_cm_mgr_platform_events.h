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

TRACE_EVENT(CM_MGR__stall_raio,
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

#endif /* _TRACE_MTK_CM_MGR_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ./
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_cm_mgr_platform_events
#include <trace/define_trace.h>



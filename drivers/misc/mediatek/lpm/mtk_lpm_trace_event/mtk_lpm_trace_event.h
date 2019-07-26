/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_lpm_trace_event

#if !defined(__MTK_LPM_TRACE_EVENT_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __MTK_LPM_TRACE_EVENT_H__

#include <linux/tracepoint.h>
#include "mtk_lpm_trace_def.h"

TRACE_EVENT(mtk_lpm_trace,
	TP_PROTO(struct mtk_lpm_trace_debug_t *t),
	TP_ARGS(t),
	TP_STRUCT__entry(
		__array(char, _datas, MTK_LPM_TRACE_EVENT_MESG_MAX)
	),
	TP_fast_assign(
		memcpy(__entry->_datas, t->_datas,
		       MTK_LPM_TRACE_EVENT_MESG_MAX);
	),
	TP_printk("%s", (char *)__entry->_datas)
);
#endif

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE

#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mtk_lpm_trace_event

#include <trace/define_trace.h>


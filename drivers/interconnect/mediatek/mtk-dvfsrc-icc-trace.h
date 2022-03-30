/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_pm_qos_icc

#if !defined(_TRACE_MTK_QOS_ICC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_QOS_ICC_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>
#include <linux/trace_events.h>

#define TPS(x)  tracepoint_string(x)

DECLARE_EVENT_CLASS(mtk_pm_qos_request,

	TP_PROTO(int mtk_pm_qos_class, s32 value, const char *owner),

	TP_ARGS(mtk_pm_qos_class, value, owner),

	TP_STRUCT__entry(
		__field(int, mtk_pm_qos_class)
		__field(s32, value)
		__array(char, owner, 40)
	),

	TP_fast_assign(
		__entry->mtk_pm_qos_class = mtk_pm_qos_class;
		__entry->value = value;
		strncpy(__entry->owner, owner, 39);
	),

	TP_printk("pm_qos_class=%d value=%d owner=%s",
		  __entry->mtk_pm_qos_class,
		  __entry->value,
		   __entry->owner)
);

DEFINE_EVENT(mtk_pm_qos_request, mtk_pm_qos_update_request,

	TP_PROTO(int mtk_pm_qos_class, s32 value, const char *owner),

	TP_ARGS(mtk_pm_qos_class, value, owner)
);

#endif /* _TRACE_MTK_QOS_ICC_H */
/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk-dvfsrc-icc-trace

/* This part must be outside protection */
#include <trace/define_trace.h>

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_pm_qos

#if !defined(_TRACE_MTK_QOS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_QOS_H

#include <linux/ktime.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <linux/tracepoint.h>
#include <linux/trace_events.h>

#define TPS(x)  tracepoint_string(x)

DECLARE_EVENT_CLASS(mtk_pm_qos_request,

	TP_PROTO(int mtk_pm_qos_class, s32 value, char *owner),

	TP_ARGS(mtk_pm_qos_class, value, owner),

	TP_STRUCT__entry(
		__field(int,                    mtk_pm_qos_class)
		__field(s32,                    value)
		__string(owner,                 owner)
	),

	TP_fast_assign(
		__entry->mtk_pm_qos_class = mtk_pm_qos_class;
		__entry->value = value;
		__assign_str(owner, owner);
	),

	TP_printk("pm_qos_class=%s value=%d owner=%s",
		  __print_symbolic(__entry->mtk_pm_qos_class,
			{ MTK_PM_QOS_MEMORY_BANDWIDTH,	"QOS_BW" },
			{ MTK_PM_QOS_HRT_BANDWIDTH,	"QOS_HRTBW" },
			{ MTK_PM_QOS_DDR_OPP,	"QOS_DDR" },
			{ MTK_PM_QOS_VCORE_OPP,	"QOS_VCORE" },
			{ MTK_PM_QOS_SCP_VCORE_REQUEST,	"QOS_SCP" }),
		  __entry->value, __get_str(owner))
);

DEFINE_EVENT(mtk_pm_qos_request, mtk_pm_qos_add_request,

	TP_PROTO(int mtk_pm_qos_class, s32 value, char *owner),

	TP_ARGS(mtk_pm_qos_class, value, owner)
);

DEFINE_EVENT(mtk_pm_qos_request, mtk_pm_qos_update_request,

	TP_PROTO(int mtk_pm_qos_class, s32 value, char *owner),

	TP_ARGS(mtk_pm_qos_class, value, owner)
);

DEFINE_EVENT(mtk_pm_qos_request, mtk_pm_qos_remove_request,

	TP_PROTO(int mtk_pm_qos_class, s32 value, char *owner),

	TP_ARGS(mtk_pm_qos_class, value, owner)
);


#endif /* _TRACE_MTK_QOS_H */
/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk-pm-qos-trace

/* This part must be outside protection */
#include <trace/define_trace.h>

/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_wifi_trace

#if !defined(_TRACE_MTK_WIFI_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_WIFI_TRACE_H

#include <linux/tracepoint.h>

#define WIFI_LOG_MSG_MAX (512)

TRACE_EVENT(wifi_standalone_log,
	TP_PROTO(const char *str),

	TP_ARGS(str),

	TP_STRUCT__entry(
		__string(msg, str)
	),

	TP_fast_assign(
		__assign_str(msg, str);
	),

	TP_printk("[wlan]%s", __get_str(msg))
);

#endif
/* if !defined(_TRACE_MTK_WIFI_TRACE_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_wifi_trace
#include <trace/define_trace.h>

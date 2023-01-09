/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM touch_boost

#if !defined(_MTK_TOUCH_BOOST_TRACE_EVENT_H__) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_TOUCH_BOOST_TRACE_EVENT_H__

#include <linux/tracepoint.h>

TRACE_EVENT(touch_boost,
	TP_PROTO(
	const char *buf
	),

	TP_ARGS(buf),

	TP_STRUCT__entry(
	__string(buf, buf)
	),

	TP_fast_assign(
	__assign_str(buf, buf);
	),

	TP_printk("%s",
	__get_str(buf)
	)
);
#endif /* _MTK_TOUCH_BOOST_TRACE_EVENT_H__ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE touch_boost_trace_event
#include <trace/define_trace.h>


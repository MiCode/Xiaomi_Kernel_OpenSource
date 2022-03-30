/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM layer_layout

#if !defined(_MTK_LAYER_LAYOUT_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_LAYER_LAYOUT_TRACE_H_

#include <linux/tracepoint.h>

TRACE_EVENT(layer_layout,
	TP_PROTO(char *msg),

	TP_ARGS(msg),

	TP_STRUCT__entry(
		__string(msg, msg)
	),

	TP_fast_assign(
		__assign_str(msg, msg);
	),

	TP_printk("%s", __get_str(msg))
);

TRACE_EVENT(layer_bw,
	TP_PROTO(char *msg),

	TP_ARGS(msg),

	TP_STRUCT__entry(
		__string(msg, msg)
	),

	TP_fast_assign(
		__assign_str(msg, msg);
	),

	TP_printk("%s", __get_str(msg))
);

#endif /* _MTK_LAYER_LAYOUT_TRACE_H_ */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_layer_layout_trace
#include <trace/define_trace.h>

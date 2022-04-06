/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs_mtk

#if !defined(_TRACE_EVENT_UFS_MEDIATEK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_UFS_MEDIATEK_H

#include <linux/tracepoint.h>

TRACE_EVENT(ufs_mtk_event,
	TP_PROTO(unsigned int type, unsigned int data),
	TP_ARGS(type, data),

	TP_STRUCT__entry(
		__field(unsigned int, type)
		__field(unsigned int, data)
	),

	TP_fast_assign(
		__entry->type = type;
		__entry->data = data;
	),

	TP_printk("ufs:event=%u data=%u",
		  __entry->type, __entry->data)
);

TRACE_EVENT(ufs_mtk_clk_scale,
	TP_PROTO(struct ufs_clk_info *sel_clki, bool scale_up),
	TP_ARGS(sel_clki, scale_up),

	TP_STRUCT__entry(
		__field(struct ufs_clk_info *, sel_clki)
		__field(struct clk *, parent_clk)
		__field(bool, scale_up)
	),

	TP_fast_assign(
		__entry->sel_clki = sel_clki;
		__entry->parent_clk = clk_get_parent(sel_clki->clk);
		__entry->scale_up = scale_up;
	),

	TP_printk("ufs: clk (%s) scaled %s @%d, parent rate=%d",
		  __entry->sel_clki->name,
		  __entry->scale_up ? "up" : "down",
		  clk_get_rate(__entry->sel_clki->clk),
		  clk_get_rate(__entry->parent_clk))
);

#endif

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH ../../drivers/scsi/ufs/
#define TRACE_INCLUDE_FILE ufs-mediatek-trace
#include <trace/define_trace.h>

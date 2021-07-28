/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM vmm

#if !defined(_TRACE_ISPDVFS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ISPDVFS_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(vmm__update_voltage,
	TP_PROTO(int voltage),
	TP_ARGS(voltage),
	TP_STRUCT__entry(
		__field(int, voltage)
	),
	TP_fast_assign(
		__entry->voltage = voltage;
	),
	TP_printk("vmm_voltage=%d",
		(int)__entry->voltage)
);

#endif /* _TRACE_ISPDVFS_EVENTS_H */

#undef TRACE_INCLUDE_FILE
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mtk-vmm-trace

/* This part must be outside protection */
#include <trace/define_trace.h>

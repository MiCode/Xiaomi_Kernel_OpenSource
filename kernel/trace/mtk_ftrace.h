/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_ftrace

#if !defined(_TRACE_MTK_FTRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_FTRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(tracing_on,

	TP_PROTO(int on, unsigned long ip),

	TP_ARGS(on, ip),

	TP_STRUCT__entry(
		__field(int, on)
		__field(unsigned long, ip)
	),

	TP_fast_assign(
		__entry->on = on;
		__entry->ip = ip;
	),

	TP_printk("ftrace is %s caller=%ps",
		__entry->on ? "enabled" : "disabled",
		(void *)__entry->ip)
);

#endif /* _TRACE_MTK_FTRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mtk_ftrace

/* This part must be outside protection */
#include <trace/define_trace.h>

/* MTK function define from here */
#include <linux/string.h>
#include <linux/seq_file.h>

#ifdef CONFIG_MTK_FTRACER
struct trace_buffer;
void print_enabled_events(struct trace_buffer *buf, struct seq_file *m);
void update_buf_size(unsigned long size);
bool boot_ftrace_check(unsigned long trace_en);
#if IS_BUILTIN(CONFIG_MTPROF)
extern bool mt_boot_finish(void);
#endif
#endif/* CONFIG_TRACING && CONFIG_MTK_FTRACER */

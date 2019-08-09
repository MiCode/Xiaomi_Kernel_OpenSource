/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef _MTK_FTRACE_H
#define  _MTK_FTRACE_H

#include <linux/string.h>
#include <linux/seq_file.h>

#ifdef CONFIG_MTK_KERNEL_MARKER
void trace_begin(char *name);
void trace_counter(char *name, int count);
void trace_end(void);
#else
#define trace_begin(name)
#define trace_counter(name, count)
#define trace_end()
#endif

struct trace_array;
extern bool ring_buffer_expanded;
ssize_t tracing_resize_ring_buffer(struct trace_array *tr,
				   unsigned long size, int cpu_id);

#ifdef CONFIG_MTK_SCHED_TRACERS
struct trace_buffer;
void print_enabled_events(struct trace_buffer *buf, struct seq_file *m);
void update_buf_size(unsigned long size);
bool boot_ftrace_check(unsigned long trace_en);
#ifdef CONFIG_MTPROF
extern int boot_finish;
#endif
#else
#define print_enabled_events(b, m)
#endif/* CONFIG_TRACING && CONFIG_MTK_SCHED_TRACERS */
#endif

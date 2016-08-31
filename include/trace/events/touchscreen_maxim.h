/*
 * include/trace/events/touchscreen_maxim.h
 *
 * Maxim touchscreen event logging to ftrace.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM touchscreen_maxim

#if !defined(_TRACE_TOUCHSCREEN_MAXIM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TOUCHSCREEN_MAXIM_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(touchscreen_maxim_irq,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
		__field(const char *, name)
	),
	TP_fast_assign(
		__entry->name = name;
	),
	TP_printk("name=%s",
	  __entry->name)
);

#endif /*  _TRACE_TOUCHSCREEN_MAXIM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

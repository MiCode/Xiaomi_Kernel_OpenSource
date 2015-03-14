/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM almk

#if !defined(_TRACE_EVENT_ALMK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_ALMK_H

#include <linux/tracepoint.h>
#include <linux/types.h>

TRACE_EVENT(almk_vmpressure,

	TP_PROTO(unsigned long pressure,
		int other_free,
		int other_file),

	TP_ARGS(pressure, other_free, other_file),

	TP_STRUCT__entry(
		__field(unsigned long, pressure)
		__field(int, other_free)
		__field(int, other_file)
	),

	TP_fast_assign(
		__entry->pressure	= pressure;
		__entry->other_free	= other_free;
		__entry->other_file	= other_file;
	),

	TP_printk("%lu, %d, %d",
			__entry->pressure, __entry->other_free,
			__entry->other_file)
);

TRACE_EVENT(almk_shrink,

	TP_PROTO(int tsize,
		 int vmp,
		 int other_free,
		 int other_file,
		 short adj),

	TP_ARGS(tsize, vmp, other_free, other_file, adj),

	TP_STRUCT__entry(
		__field(int, tsize)
		__field(int, vmp)
		__field(int, other_free)
		__field(int, other_file)
		__field(short, adj)
	),

	TP_fast_assign(
		__entry->tsize		= tsize;
		__entry->vmp		= vmp;
		__entry->other_free     = other_free;
		__entry->other_file     = other_file;
		__entry->adj		= adj;
	),

	TP_printk("%d, %d, %d, %d, %d",
		__entry->tsize,
		__entry->vmp,
		__entry->other_free,
		__entry->other_file,
		__entry->adj)
);

#endif

#include <trace/define_trace.h>


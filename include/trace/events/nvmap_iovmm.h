/*
 * include/trace/events/nvmap.h
 *
 * NvMap event logging to ftrace.
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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
#define TRACE_SYSTEM nvmap_iovmm

#if !defined(_TRACE_NVMAP_IOVMM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVMAP_IOVMM_H

#include <linux/nvmap.h>
#include <linux/tracepoint.h>

TRACE_EVENT(tegra_iovmm_create_vm,
	TP_PROTO(const char *name, u32 iova_start, u32 iova_end),

	TP_ARGS(name, iova_start, iova_end),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, iova_start)
		__field(u32, iova_end)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->iova_start = iova_start;
		__entry->iova_end = iova_end;
	),

	TP_printk("name=%s, iova_start=0x%x, iova_end=0x%x",
		__entry->name, __entry->iova_start, __entry->iova_end)
);

TRACE_EVENT(tegra_iovmm_free_vm,
	TP_PROTO(const char *name, u32 iova_start, u32 iova_end),

	TP_ARGS(name, iova_start, iova_end),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, iova_start)
		__field(u32, iova_end)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->iova_start = iova_start;
		__entry->iova_end = iova_end;
	),

	TP_printk("name=%s, iova_start=0x%x, iova_end=0x%x",
		__entry->name, __entry->iova_start, __entry->iova_end)
);

#endif /* _TRACE_NVMAP_IOVMM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM msm_bus

#if !defined(_TRACE_MSM_BUS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MSM_BUS_H

#include <linux/tracepoint.h>

TRACE_EVENT(bus_update_request,

	TP_PROTO(int sec, int nsec, const char *name, unsigned int index,
		int src, int dest, unsigned long long ab,
		unsigned long long ib),

	TP_ARGS(sec, nsec, name, index, src, dest, ab, ib),

	TP_STRUCT__entry(
		__field(int, sec)
		__field(int, nsec)
		__string(name, name)
		__field(u32, index)
		__field(int, src)
		__field(int, dest)
		__field(u64, ab)
		__field(u64, ib)
	),

	TP_fast_assign(
		__entry->sec = sec;
		__entry->nsec = nsec;
		__assign_str(name, name);
		__entry->index = index;
		__entry->src = src;
		__entry->dest = dest;
		__entry->ab = ab;
		__entry->ib = ib;
	),

	TP_printk("time= %d.%d name=%s index=%u src=%d dest=%d ab=%llu ib=%llu",
		__entry->sec,
		__entry->nsec,
		__get_str(name),
		(unsigned int)__entry->index,
		__entry->src,
		__entry->dest,
		(unsigned long long)__entry->ab,
		(unsigned long long)__entry->ib)
);
#endif
#define TRACE_INCLUDE_FILE trace_msm_bus
#include <trace/define_trace.h>

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
#if !defined(_TRACER_PKT_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACER_PKT_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tracer_pkt
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE tracer_pkt_private

#include <linux/tracepoint.h>

TRACE_EVENT(tracer_pkt_event,

	TP_PROTO(uint32_t id, uint32_t *cc),

	TP_ARGS(id, cc),

	TP_STRUCT__entry(
		__field(uint32_t, id)
		__field(uint32_t, cc1)
		__field(uint32_t, cc2)
		__field(uint32_t, cc3)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->cc1 = cc[0];
		__entry->cc2 = cc[1];
		__entry->cc3 = cc[2];
	),

	TP_printk("CC - 0x%08x:0x%08x:0x%08x, ID - %d",
		__entry->cc1, __entry->cc2, __entry->cc3, __entry->id)
);
#endif /*_TRACER_PKT_TRACE_H*/

#include <trace/define_trace.h>


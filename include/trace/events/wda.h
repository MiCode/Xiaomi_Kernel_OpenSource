/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM wda

#if !defined(_TRACE_WDA_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_WDA_H

#include <linux/tracepoint.h>

TRACE_EVENT(wda_set_powersave_mode,

	TP_PROTO(int enable),

	TP_ARGS(enable),

	TP_STRUCT__entry(
		__field(int, enable)
	),

	TP_fast_assign(
		__entry->enable = enable;
	),

	TP_printk("set powersave mode to %s",
		__entry->enable ? "enable" : "disable")
);

TRACE_EVENT(wda_client_state_up,

	TP_PROTO(u32 instance, u32 ep_type, u32 iface),

	TP_ARGS(instance, ep_type, iface),

	TP_STRUCT__entry(
		__field(u32, instance)
		__field(u32, ep_type)
		__field(u32, iface)
	),

	TP_fast_assign(
		__entry->instance = instance;
		__entry->ep_type = ep_type;
		__entry->iface = iface;
	),

	TP_printk("Client: Connected with WDA instance=%u ep_type=%u i_id=%u",
		__entry->instance, __entry->ep_type, __entry->iface)
);

TRACE_EVENT(wda_client_state_down,

	TP_PROTO(int from_cb),

	TP_ARGS(from_cb),

	TP_STRUCT__entry(
		__field(int, from_cb)
	),

	TP_fast_assign(
		__entry->from_cb = from_cb;
	),

	TP_printk("Client: Connection with WDA lost Exit by callback %d",
		  __entry->from_cb)
);

#endif /* _TRACE_WDA_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

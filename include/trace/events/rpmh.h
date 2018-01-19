/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM rpmh

#if !defined(_TRACE_RPMH_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_RPMH_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(rpmh_ack_recvd,

	TP_PROTO(const char *s, int m, u32 addr, int errno),

	TP_ARGS(s, m, addr, errno),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(int, m)
		__field(u32, addr)
		__field(int, errno)
	),

	TP_fast_assign(
		__entry->name = s;
		__entry->m = m;
		__entry->addr = addr;
		__entry->errno = errno;
	),

	TP_printk("%s: ack: tcs-m:%d addr: 0x%08x errno: %d",
			__entry->name, __entry->m, __entry->addr, __entry->errno)
);

DEFINE_EVENT(rpmh_ack_recvd, rpmh_notify_irq,
	TP_PROTO(const char *s, int m, u32 addr, int err),
	TP_ARGS(s, m, addr, err)
);

DEFINE_EVENT(rpmh_ack_recvd, rpmh_notify,
	TP_PROTO(const char *s, int m, u32 addr, int err),
	TP_ARGS(s, m, addr, err)
);

TRACE_EVENT(rpmh_send_msg,

	TP_PROTO(const char *s, unsigned long b, int m, int n, u32 h, u32 a,
							u32 v, bool c, bool t),

	TP_ARGS(s, b, m, n, h, a, v, c, t),

	TP_STRUCT__entry(
		__field(const char*, name)
		__field(unsigned long, base)
		__field(int, m)
		__field(int, n)
		__field(u32, hdr)
		__field(u32, addr)
		__field(u32, data)
		__field(bool, complete)
		__field(bool, trigger)
	),

	TP_fast_assign(
		__entry->name = s;
		__entry->base = b;
		__entry->m = m;
		__entry->n = n;
		__entry->hdr = h;
		__entry->addr = a;
		__entry->data = v;
		__entry->complete = c;
		__entry->trigger = t;
	),

	TP_printk("%s: reg: 0x%08lx send-msg: tcs(m): %d cmd(n): %d msgid: 0x%08x addr: 0x%08x data: 0x%08x complete: %d trigger: %d",
			__entry->name, __entry->base, __entry->m,
			__entry->n, __entry->hdr, __entry->addr,
			__entry->data, __entry->complete, __entry->trigger)
);

TRACE_EVENT(rpmh_control_msg,

	TP_PROTO(const char *s, u32 v),

	TP_ARGS(s, v),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, data)
	),

	TP_fast_assign(
		__entry->name = s;
		__entry->data = v;
	),

	TP_printk("%s: ctrl-msg: data: 0x%08x",
			__entry->name, __entry->data)
);

#endif /* _TRACE_RPMH_H */

#define TRACE_INCLUDE_FILE rpmh
#include <trace/define_trace.h>

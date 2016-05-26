/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

	TP_PROTO(int m, u32 addr, int errno),

	TP_ARGS(m, addr, errno),

	TP_STRUCT__entry(
		__field(int, m)
		__field(u32, addr)
		__field(int, errno)
	),

	TP_fast_assign(
		__entry->m = m;
		__entry->addr = addr;
		__entry->errno = errno;
	),

	TP_printk("ack: tcs-m:%d addr: 0x%08x errno: %d",
			__entry->m, __entry->addr, __entry->errno)
);

DEFINE_EVENT(rpmh_ack_recvd, rpmh_notify_irq,
	TP_PROTO(int m, u32 addr, int err),
	TP_ARGS(m, addr, err)
);

DEFINE_EVENT(rpmh_ack_recvd, rpmh_notify,
	TP_PROTO(int m, u32 addr, int err),
	TP_ARGS(m, addr, err)
);

TRACE_EVENT(rpmh_send_msg,

	TP_PROTO(void *b, int m, int n, u32 h, u32 a, u32 v, bool c),

	TP_ARGS(b, m, n, h, a, v, c),

	TP_STRUCT__entry(
		__field(void *, base)
		__field(int, m)
		__field(int, n)
		__field(u32, hdr)
		__field(u32, addr)
		__field(u32, data)
		__field(bool, complete)
	),

	TP_fast_assign(
		__entry->base = b;
		__entry->m = m;
		__entry->n = n;
		__entry->hdr = h;
		__entry->addr = a;
		__entry->data = v;
		__entry->complete = c;
	),

	TP_printk("msg: base: 0x%p  tcs(m): %d cmd(n): %d msgid: 0x%08x addr: 0x%08x data: 0x%08x complete: %d",
			__entry->base + (672 * __entry->m) + (20 * __entry->n),
			__entry->m, __entry->n, __entry->hdr,
			__entry->addr, __entry->data, __entry->complete)
);

TRACE_EVENT(rpmh_control_msg,

	TP_PROTO(void *r, u32 v),

	TP_ARGS(r, v),

	TP_STRUCT__entry(
		__field(void *, reg)
		__field(u32, data)
	),

	TP_fast_assign(
		__entry->reg = r;
		__entry->data = v;
	),

	TP_printk("ctrl-msg: reg: 0x%p data: 0x%08x",
			__entry->reg, __entry->data)
);

#endif /* _TRACE_RPMH_H */

#define TRACE_INCLUDE_FILE rpmh
#include <trace/define_trace.h>

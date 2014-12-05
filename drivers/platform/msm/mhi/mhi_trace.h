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
#define TRACE_SYSTEM mhi
#define TRACE_INCLUDE_FILE mhi_trace

#if !defined(_TRACE_SCHED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MHI_TRACE_H_

#include <linux/tracepoint.h>
#include "mhi.h"

DECLARE_EVENT_CLASS(mhi_handler_template,

	TP_PROTO(int state),

	TP_ARGS(state),

	TP_STRUCT__entry(
		__field(int, state)
		),

	TP_fast_assign(
		__entry->state = state;
	),

	TP_printk("State is %d", __entry->state)
)

DEFINE_EVENT(mhi_handler_template, mhi_state,

	TP_PROTO(int state),

	TP_ARGS(state)
);

DECLARE_EVENT_CLASS(mhi_tre_template,

	TP_PROTO(union mhi_xfer_pkt *tre, int chan, int dir),

	TP_ARGS(tre, chan, dir),

	TP_STRUCT__entry(
		__field(void *, tre_addr)
		__field(unsigned long long, buf_ptr)
		__field(unsigned int, buf_len)
		__field(int, chan)
		__field(int, dir)
		),

	TP_fast_assign(
		__entry->tre_addr = tre;
		__entry->buf_ptr = tre->data_tx_pkt.buffer_ptr;
		__entry->buf_len = tre->data_tx_pkt.buf_len;
		__entry->chan = chan;
		__entry->dir = dir;
	),

	TP_printk("CHAN: %d TRE: 0x%p BUF: 0x%llx LEN: 0x%x DIR:%s",
		__entry->chan, __entry->tre_addr,
		__entry->buf_ptr, __entry->buf_len,
		__entry->dir ? "IN" : "OUT")
)

DEFINE_EVENT(mhi_tre_template, mhi_tre,

	TP_PROTO(union mhi_xfer_pkt *tre, int chan, int dir),

	TP_ARGS(tre, chan, dir)
);

DECLARE_EVENT_CLASS(mhi_ev_template,

	TP_PROTO(union mhi_event_pkt *ev),

	TP_ARGS(ev),

	TP_STRUCT__entry(
		__field(void *, ev_addr)
		__field(unsigned long long, tre_addr)
		__field(int, tre_len)
		__field(int, chan)
		),

	TP_fast_assign(
		__entry->ev_addr = ev;
		__entry->tre_addr = ev->xfer_event_pkt.xfer_ptr;
		__entry->tre_len = MHI_EV_READ_LEN(EV_LEN, ev);
		__entry->chan = MHI_EV_READ_CHID(EV_CHID, ev);
	),

	TP_printk("CHAN: %d EVENT 0x%p TRE_p: 0x%llx LEN: 0x%x",
		__entry->chan, __entry->ev_addr,
		__entry->tre_addr, __entry->tre_len)
)

DEFINE_EVENT(mhi_ev_template, mhi_ev,

	TP_PROTO(union mhi_event_pkt *mhi_ev),

	TP_ARGS(mhi_ev)
);

DECLARE_EVENT_CLASS(mhi_msi_template,

	TP_PROTO(int msi),

	TP_ARGS(msi),

	TP_STRUCT__entry(
		__field(int, msi)
		),

	TP_fast_assign(
		__entry->msi = msi;
	),

	TP_printk("MSI received %d", __entry->msi)
)

DEFINE_EVENT(mhi_msi_template, mhi_msi,

	TP_PROTO(int msi),

	TP_ARGS(msi)
);
#endif /* _MHI_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>

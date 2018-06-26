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
#define TRACE_SYSTEM dfc

#if !defined(_TRACE_DFC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DFC_H

#include <linux/tracepoint.h>

TRACE_EVENT(dfc_qmi_tc,

	TP_PROTO(u8 bearer_id, u32 flow_id, u32 grant, int qlen,
		 u32 tcm_handle, int enable),

	TP_ARGS(bearer_id, flow_id, grant, qlen, tcm_handle, enable),

	TP_STRUCT__entry(
		__field(u8, bid)
		__field(u32, fid)
		__field(u32, grant)
		__field(int, qlen)
		__field(u32, tcm_handle)
		__field(int, enable)
	),

	TP_fast_assign(
		__entry->bid = bearer_id;
		__entry->fid = flow_id;
		__entry->grant = grant;
		__entry->qlen = qlen;
		__entry->tcm_handle = tcm_handle;
		__entry->enable = enable;
	),

	TP_printk("bearer_id=%u grant=%u qdisc_len=%d flow_id=%u "
		  "tcm_handle=0x%x %s",
		__entry->bid, __entry->grant, __entry->qlen, __entry->fid,
		__entry->tcm_handle,
		__entry->enable ? "enable" : "disable")
);

TRACE_EVENT(dfc_flow_ind,

	TP_PROTO(int src, int idx, u8 mux_id, u8 bearer_id, u32 grant,
		 u16 seq_num, u8 ack_req),

	TP_ARGS(src, idx, mux_id, bearer_id, grant, seq_num, ack_req),

	TP_STRUCT__entry(
		__field(int, src)
		__field(int, idx)
		__field(u8, mid)
		__field(u8, bid)
		__field(u32, grant)
		__field(u16, seq)
		__field(u8, ack_req)
	),

	TP_fast_assign(
		__entry->src = src;
		__entry->idx = idx;
		__entry->mid = mux_id;
		__entry->bid = bearer_id;
		__entry->grant = grant;
		__entry->seq = seq_num;
		__entry->ack_req = ack_req;
	),

	TP_printk("src=%d idx[%d]: mux_id=%u bearer_id=%u grant=%u "
		  "seq_num=%u ack_req=%u",
		__entry->src, __entry->idx, __entry->mid, __entry->bid,
		__entry->grant, __entry->seq, __entry->ack_req)
);

TRACE_EVENT(dfc_flow_check,

	TP_PROTO(u8 bearer_id, unsigned int len, u32 grant),

	TP_ARGS(bearer_id, len, grant),

	TP_STRUCT__entry(
		__field(u8, bearer_id)
		__field(unsigned int, len)
		__field(u32, grant)
	),

	TP_fast_assign(
		__entry->bearer_id = bearer_id;
		__entry->len = len;
		__entry->grant = grant;
	),

	TP_printk("bearer_id=%u skb_len=%u current_grant=%u",
		__entry->bearer_id, __entry->len, __entry->grant)
);

TRACE_EVENT(dfc_flow_info,

	TP_PROTO(u8 bearer_id, u32 flow_id, int ip_type, u32 handle, int add),

	TP_ARGS(bearer_id, flow_id, ip_type, handle, add),

	TP_STRUCT__entry(
		__field(u8, bid)
		__field(u32, fid)
		__field(int, ip)
		__field(u32, handle)
		__field(int, action)
	),

	TP_fast_assign(
		__entry->bid = bearer_id;
		__entry->fid = flow_id;
		__entry->ip = ip_type;
		__entry->handle = handle;
		__entry->action = add;
	),

	TP_printk("%s: bearer_id=%u flow_id=%u ip_type=%d tcm_handle=0x%x",
		__entry->action ? "add flow" : "delete flow",
		__entry->bid, __entry->fid, __entry->ip, __entry->handle)
);

TRACE_EVENT(dfc_client_state_up,

	TP_PROTO(int idx, u32 instance, u32 ep_type, u32 iface),

	TP_ARGS(idx, instance, ep_type, iface),

	TP_STRUCT__entry(
		__field(int, idx)
		__field(u32, instance)
		__field(u32, ep_type)
		__field(u32, iface)
	),

	TP_fast_assign(
		__entry->idx = idx;
		__entry->instance = instance;
		__entry->ep_type = ep_type;
		__entry->iface = iface;
	),

	TP_printk("Client[%d]: Connection established with DFC Service "
		  "instance=%u ep_type=%u iface_id=%u",
		__entry->idx, __entry->instance,
		__entry->ep_type, __entry->iface)
);

TRACE_EVENT(dfc_client_state_down,

	TP_PROTO(int idx, int from_cb),

	TP_ARGS(idx, from_cb),

	TP_STRUCT__entry(
		__field(int, idx)
		__field(int, from_cb)
	),

	TP_fast_assign(
		__entry->idx = idx;
		__entry->from_cb = from_cb;
	),

	TP_printk("Client[%d]: Connection with DFC service lost. "
		  "Exit by callback %d",
		  __entry->idx, __entry->from_cb)
);

#endif /* _TRACE_DFC_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

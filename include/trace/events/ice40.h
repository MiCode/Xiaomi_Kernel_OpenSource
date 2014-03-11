/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM ice40

#if !defined(_TRACE_ICE40_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ICE40_H

#include <linux/tracepoint.h>
#include <linux/usb.h>

TRACE_EVENT(ice40_reg_write,

	TP_PROTO(u8 addr, u8 val, u8 cmd0, u8 cmd1, int ret),

	TP_ARGS(addr, val, cmd0, cmd1, ret),

	TP_STRUCT__entry(
		__field(u8, addr)
		__field(u8, val)
		__field(u8, cmd0)
		__field(u8, cmd1)
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->addr = addr;
		__entry->val = val;
		__entry->cmd0 = cmd0;
		__entry->cmd1 = cmd1;
		__entry->ret = ret;
	),

	TP_printk("addr = %x val = %x cmd0 = %x cmd1 = %x ret = %d",
			__entry->addr, __entry->val, __entry->cmd0,
			__entry->cmd1, __entry->ret)
);

TRACE_EVENT(ice40_reg_read,

	TP_PROTO(u8 addr, u8 cmd0, int ret),

	TP_ARGS(addr, cmd0, ret),

	TP_STRUCT__entry(
		__field(u8, addr)
		__field(u8, cmd0)
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->addr = addr;
		__entry->cmd0 = cmd0;
		__entry->ret = ret;
	),

	TP_printk("addr = %x cmd0 = %x ret = %x", __entry->addr,
			__entry->cmd0, __entry->ret)
);

TRACE_EVENT(ice40_hub_control,

	TP_PROTO(u16 req, u16 val, u16 index, u16 len, int ret),

	TP_ARGS(req, val, index, len, ret),

	TP_STRUCT__entry(
		__field(u16, req)
		__field(u16, val)
		__field(u16, index)
		__field(u16, len)
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->req = req;
		__entry->val = val;
		__entry->index = index;
		__entry->len = len;
		__entry->ret = ret;
	),

	TP_printk("req = %x val = %x index = %x len = %x ret = %d",
			__entry->req, __entry->val, __entry->index,
			__entry->len, __entry->ret)
);

TRACE_EVENT(ice40_ep0,

	TP_PROTO(const char *state),

	TP_ARGS(state),

	TP_STRUCT__entry(
		__string(state, state)
	),

	TP_fast_assign(
		__assign_str(state, state);
	),

	TP_printk("ep0 state: %s", __get_str(state))
);

TRACE_EVENT(ice40_urb_enqueue,

	TP_PROTO(struct urb *urb),

	TP_ARGS(urb),

	TP_STRUCT__entry(
		__field(u16, epnum)
		__field(u8, dir)
		__field(u8, type)
		__field(u32, len)
	),

	TP_fast_assign(
		__entry->epnum = usb_pipeendpoint(urb->pipe);
		__entry->dir = usb_urb_dir_in(urb);
		__entry->type = usb_pipebulk(urb->pipe);
		__entry->len = urb->transfer_buffer_length;
	),

	TP_printk("URB_LOG: E: ep %d %s %s len %d", __entry->epnum,
			__entry->dir ? "In" : "Out",
			__entry->type ? "Bulk" : "ctrl",
			__entry->len)
);

TRACE_EVENT(ice40_urb_dequeue,

	TP_PROTO(struct urb *urb),

	TP_ARGS(urb),

	TP_STRUCT__entry(
		__field(u16, epnum)
		__field(u8, dir)
		__field(u8, type)
		__field(u32, len)
		__field(int, reason)
	),

	TP_fast_assign(
		__entry->epnum = usb_pipeendpoint(urb->pipe);
		__entry->dir = usb_urb_dir_in(urb);
		__entry->type = usb_pipebulk(urb->pipe);
		__entry->len = urb->transfer_buffer_length;
		__entry->reason = urb->unlinked;
	),

	TP_printk("URB_LOG: D: ep %d %s %s len %d reason %d",
			__entry->epnum,
			__entry->dir ? "In" : "Out",
			__entry->type ? "Bulk" : "ctrl",
			__entry->len, __entry->reason)
);

TRACE_EVENT(ice40_urb_done,

	TP_PROTO(struct urb *urb, int result),

	TP_ARGS(urb, result),

	TP_STRUCT__entry(
		__field(int, result)
		__field(u16, epnum)
		__field(u8, dir)
		__field(u8, type)
		__field(u32, len)
		__field(u32, actual)
	),

	TP_fast_assign(
		__entry->result = result;
		__entry->epnum = usb_pipeendpoint(urb->pipe);
		__entry->dir = usb_urb_dir_in(urb);
		__entry->type = usb_pipebulk(urb->pipe);
		__entry->len = urb->transfer_buffer_length;
		__entry->actual = urb->actual_length;
	),

	TP_printk("URB_LOG: C: ep %d %s %s len %d actual %d result %d",
			__entry->epnum, __entry->dir ? "In" : "Out",
			__entry->type ? "Bulk" : "ctrl", __entry->len,
			__entry->actual, __entry->result)
);

TRACE_EVENT(ice40_bus_suspend,

	TP_PROTO(u8 status),

	TP_ARGS(status),

	TP_STRUCT__entry(
		__field(u8, status)
	),

	TP_fast_assign(
		__entry->status = status;
	),

	TP_printk("bus_suspend status %d", __entry->status)
);

TRACE_EVENT(ice40_bus_resume,

	TP_PROTO(u8 status),

	TP_ARGS(status),

	TP_STRUCT__entry(
		__field(u8, status)
	),

	TP_fast_assign(
		__entry->status = status;
	),

	TP_printk("bus_resume status %d", __entry->status)
);

TRACE_EVENT(ice40_setup,

	TP_PROTO(const char *token, int ret),

	TP_ARGS(token, ret),

	TP_STRUCT__entry(
		__string(token, token)
		__field(int, ret)
	),

	TP_fast_assign(
		__assign_str(token, token);
		__entry->ret = ret;
	),

	TP_printk("Trace: SETUP %s ret %d",
		__get_str(token), __entry->ret)
);

TRACE_EVENT(ice40_in,

	TP_PROTO(u16 ep, const char *token, u8 len, u8 expected, int ret),

	TP_ARGS(ep, token, len, expected, ret),

	TP_STRUCT__entry(
		__field(u16, ep)
		__string(token, token)
		__field(u8, len)
		__field(u8, expected)
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->ep = ep;
		__assign_str(token, token);
		__entry->len = len;
		__entry->expected = expected;
		__entry->ret = ret;
	),

	TP_printk("Trace: %d IN %s len %d expected %d ret %d",
			__entry->ep, __get_str(token),
			__entry->len, __entry->expected,
			__entry->ret)
);

TRACE_EVENT(ice40_out,

	TP_PROTO(u16 ep, const char *token, u8 len, int ret),

	TP_ARGS(ep, token, len, ret),

	TP_STRUCT__entry(
		__field(u16, ep)
		__string(token, token)
		__field(u8, len)
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->ep = ep;
		__assign_str(token, token);
		__entry->len = len;
		__entry->ret = ret;
	),

	TP_printk("Trace: %d OUT %s len %d ret %d",
			__entry->ep, __get_str(token),
			__entry->len, __entry->ret)
);
#endif /* if !defined(_TRACE_ICE40_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>

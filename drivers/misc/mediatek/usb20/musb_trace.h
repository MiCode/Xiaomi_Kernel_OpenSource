/* SPDX-License-Identifier: GPL-2.0 */
/*
 * musb_trace.h - MUSB Controller Trace Support
 *
 * Copyright (C) 2022 MediaTek Inc.
 *
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM musb

#if !defined(__MUSB_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define __MUSB_TRACE_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/usb.h>
#include "musb_core.h"

#define MUSB_MSG_MAX   500

TRACE_EVENT(musb_log,
	TP_PROTO(struct musb *musb, struct va_format *vaf),
	TP_ARGS(musb, vaf),
	TP_STRUCT__entry(
		__string(name, dev_name(musb->controller))
		__dynamic_array(char, msg, MUSB_MSG_MAX)
	),
	TP_fast_assign(
		__assign_str(name, dev_name(musb->controller));
		vsnprintf(__get_str(msg), MUSB_MSG_MAX, vaf->fmt, *vaf->va);
	),
	TP_printk("%s: %s", __get_str(name), __get_str(msg))
);

DECLARE_EVENT_CLASS(musb_regb,
	TP_PROTO(void *caller, const void  __iomem *addr,
		 unsigned int offset, u8 data),
	TP_ARGS(caller, addr, offset, data),
	TP_STRUCT__entry(
		__field(void *, caller)
		__field(const void __iomem *, addr)
		__field(unsigned int, offset)
		__field(u8, data)
	),
	TP_fast_assign(
		__entry->caller = caller;
		__entry->addr = addr;
		__entry->offset = offset;
		__entry->data = data;
	),
	TP_printk("%pS: %p + %04x: %02x",
		__entry->caller, __entry->addr, __entry->offset, __entry->data)
);

DEFINE_EVENT(musb_regb, musb_readb,
	TP_PROTO(void *caller, const void __iomem *addr,
		 unsigned int offset, u8 data),
	TP_ARGS(caller, addr, offset, data)
);

DEFINE_EVENT(musb_regb, musb_writeb,
	TP_PROTO(void *caller, const void __iomem *addr,
		 unsigned int offset, u8 data),
	TP_ARGS(caller, addr, offset, data)
);

DECLARE_EVENT_CLASS(musb_regw,
	TP_PROTO(void *caller, const void __iomem *addr,
		 unsigned int offset, u16 data),
	TP_ARGS(caller, addr, offset, data),
	TP_STRUCT__entry(
		__field(void *, caller)
		__field(const void __iomem *, addr)
		__field(unsigned int, offset)
		__field(u16, data)
	),
	TP_fast_assign(
		__entry->caller = caller;
		__entry->addr = addr;
		__entry->offset = offset;
		__entry->data = data;
	),
	TP_printk("%pS: %p + %04x: %04x",
		__entry->caller, __entry->addr, __entry->offset, __entry->data)
);

DEFINE_EVENT(musb_regw, musb_readw,
	TP_PROTO(void *caller, const void __iomem *addr,
		 unsigned int offset, u16 data),
	TP_ARGS(caller, addr, offset, data)
);

DEFINE_EVENT(musb_regw, musb_writew,
	TP_PROTO(void *caller, const void __iomem *addr,
		 unsigned int offset, u16 data),
	TP_ARGS(caller, addr, offset, data)
);

DECLARE_EVENT_CLASS(musb_regl,
	TP_PROTO(void *caller, const void __iomem *addr,
		 unsigned int offset, u32 data),
	TP_ARGS(caller, addr, offset, data),
	TP_STRUCT__entry(
		__field(void *, caller)
		__field(const void __iomem *, addr)
		__field(unsigned int, offset)
		__field(u32, data)
	),
	TP_fast_assign(
		__entry->caller = caller;
		__entry->addr = addr;
		__entry->offset = offset;
		__entry->data = data;
	),
	TP_printk("%pS: %p + %04x: %08x",
		__entry->caller, __entry->addr, __entry->offset, __entry->data)
);

DEFINE_EVENT(musb_regl, musb_readl,
	TP_PROTO(void *caller, const void __iomem *addr,
		 unsigned int offset, u32 data),
	TP_ARGS(caller, addr, offset, data)
);

DEFINE_EVENT(musb_regl, musb_writel,
	TP_PROTO(void *caller, const void __iomem *addr,
		 unsigned int offset, u32 data),
	TP_ARGS(caller, addr, offset, data)
);

TRACE_EVENT(musb_isr,
	TP_PROTO(struct musb *musb),
	TP_ARGS(musb),
	TP_STRUCT__entry(
		__string(name, dev_name(musb->controller))
		__field(u8, int_usb)
		__field(u16, int_tx)
		__field(u16, int_rx)
	),
	TP_fast_assign(
		__assign_str(name, dev_name(musb->controller));
		__entry->int_usb = musb->int_usb;
		__entry->int_tx = musb->int_tx;
		__entry->int_rx = musb->int_rx;
	),
	TP_printk("%s: usb %02x, tx %04x, rx %04x",
		__get_str(name), __entry->int_usb,
		__entry->int_tx, __entry->int_rx
	)
);

DECLARE_EVENT_CLASS(musb_urb,
	TP_PROTO(struct musb *musb, struct urb *urb),
	TP_ARGS(musb, urb),
	TP_STRUCT__entry(
		__string(name, dev_name(musb->controller))
		__field(struct urb *, urb)
		__field(unsigned int, pipe)
		__field(int, status)
		__field(unsigned int, flag)
		__field(u32, buf_len)
		__field(u32, actual_len)
	),
	TP_fast_assign(
		__assign_str(name, dev_name(musb->controller));
		__entry->urb = urb;
		__entry->pipe = urb->pipe;
		__entry->status = urb->status;
		__entry->flag = urb->transfer_flags;
		__entry->buf_len = urb->transfer_buffer_length;
		__entry->actual_len = urb->actual_length;
	),
	TP_printk("%s: %p, dev%d ep%d%s, flag 0x%x, len %d/%d, status %d",
			__get_str(name), __entry->urb,
			usb_pipedevice(__entry->pipe),
			usb_pipeendpoint(__entry->pipe),
			usb_pipein(__entry->pipe) ? "in" : "out",
			__entry->flag,
			__entry->actual_len, __entry->buf_len,
			__entry->status
	)
);

DEFINE_EVENT(musb_urb, musb_urb_start,
	TP_PROTO(struct musb *musb, struct urb *urb),
	TP_ARGS(musb, urb)
);

DEFINE_EVENT(musb_urb, musb_urb_gb,
	TP_PROTO(struct musb *musb, struct urb *urb),
	TP_ARGS(musb, urb)
);

DEFINE_EVENT(musb_urb, musb_urb_rx,
	TP_PROTO(struct musb *musb, struct urb *urb),
	TP_ARGS(musb, urb)
);

DEFINE_EVENT(musb_urb, musb_urb_tx,
	TP_PROTO(struct musb *musb, struct urb *urb),
	TP_ARGS(musb, urb)
);

DEFINE_EVENT(musb_urb, musb_urb_enq,
	TP_PROTO(struct musb *musb, struct urb *urb),
	TP_ARGS(musb, urb)
);

DEFINE_EVENT(musb_urb, musb_urb_deq,
	TP_PROTO(struct musb *musb, struct urb *urb),
	TP_ARGS(musb, urb)
);

DECLARE_EVENT_CLASS(musb_req,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req),
	TP_STRUCT__entry(
		__field(struct usb_request *, req)
		__field(u8, is_tx)
		__field(u8, epnum)
		__field(int, status)
		__field(unsigned int, buf_len)
		__field(unsigned int, actual_len)
		__field(unsigned int, zero)
		__field(unsigned int, short_not_ok)
		__field(unsigned int, no_interrupt)
	),
	TP_fast_assign(
		__entry->req = &req->request;
		__entry->is_tx = req->tx;
		__entry->epnum = req->epnum;
		__entry->status = req->request.status;
		__entry->buf_len = req->request.length;
		__entry->actual_len = req->request.actual;
		__entry->zero = req->request.zero;
		__entry->short_not_ok = req->request.short_not_ok;
		__entry->no_interrupt = req->request.no_interrupt;
	),
	TP_printk("%p, ep%d %s, %s%s%s, len %d/%d, status %d",
			__entry->req, __entry->epnum,
			__entry->is_tx ? "tx/IN" : "rx/OUT",
			__entry->zero ? "Z" : "z",
			__entry->short_not_ok ? "S" : "s",
			__entry->no_interrupt ? "I" : "i",
			__entry->actual_len, __entry->buf_len,
			__entry->status
	)
);

DEFINE_EVENT(musb_req, musb_req_gb,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req)
);

DEFINE_EVENT(musb_req, musb_req_tx,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req)
);

DEFINE_EVENT(musb_req, musb_req_rx,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req)
);

DEFINE_EVENT(musb_req, musb_req_alloc,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req)
);

DEFINE_EVENT(musb_req, musb_req_free,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req)
);

DEFINE_EVENT(musb_req, musb_req_start,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req)
);

DEFINE_EVENT(musb_req, musb_req_enq,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req)
);

DEFINE_EVENT(musb_req, musb_req_deq,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req)
);

DEFINE_EVENT(musb_req, musb_g_giveback,
	TP_PROTO(struct musb_request *req),
	TP_ARGS(req)
);

DECLARE_EVENT_CLASS(musb_log_ep,
	TP_PROTO(struct musb_ep *musb_ep),
	TP_ARGS(musb_ep),
	TP_STRUCT__entry(
		__string(name, musb_ep->name)
		__field(unsigned int, type)
		__field(unsigned int, maxp)
		__field(unsigned int, mult)
		__field(unsigned int, maxburst)
		__field(unsigned int, direction)
	),
	TP_fast_assign(
		__assign_str(name, musb_ep->name);
		__entry->type = musb_ep->type;
		__entry->maxp = musb_ep->end_point.maxpacket;
		__entry->mult = musb_ep->end_point.mult;
		__entry->maxburst = musb_ep->end_point.maxburst;
		__entry->direction = musb_ep->is_in;
	),
	TP_printk("%s: type %d maxp %d mult %d burst %d :%c",
		__get_str(name), __entry->type,
		__entry->maxp, __entry->mult,
		__entry->maxburst,
		__entry->direction ? '<' : '>'
	)
);

DEFINE_EVENT(musb_log_ep, musb_gadget_enable,
	TP_PROTO(struct musb_ep *musb_ep),
	TP_ARGS(musb_ep)
);

DEFINE_EVENT(musb_log_ep, musb_gadget_disable,
	TP_PROTO(struct musb_ep *musb_ep),
	TP_ARGS(musb_ep)
);

DECLARE_EVENT_CLASS(musb_host_log_ep,
	TP_PROTO(struct urb *urb),
	TP_ARGS(urb),
	TP_STRUCT__entry(
		__field(void *, urb)
		__field(unsigned int, pipe)
		__field(unsigned int, stream)
		__field(int, status)
		__field(unsigned int, flags)
		__field(int, num_mapped_sgs)
		__field(int, num_sgs)
		__field(int, length)
		__field(int, actual)
		__field(int, epnum)
		__field(int, dir_in)
		__field(int, type)
		__field(int, slot_id)
	),
	TP_fast_assign(
		__entry->urb = urb;
		__entry->pipe = urb->pipe;
		__entry->stream = urb->stream_id;
		__entry->status = urb->status;
		__entry->flags = urb->transfer_flags;
		__entry->num_mapped_sgs = urb->num_mapped_sgs;
		__entry->num_sgs = urb->num_sgs;
		__entry->length = urb->transfer_buffer_length;
		__entry->actual = urb->actual_length;
		__entry->epnum = usb_endpoint_num(&urb->ep->desc);
		__entry->dir_in = usb_endpoint_dir_in(&urb->ep->desc);
		__entry->type = usb_endpoint_type(&urb->ep->desc);
		__entry->slot_id = urb->dev->slot_id;
	),
	TP_printk("ep%d%s-%s: urb %p pipe %u slot %d length %d/%d sgs %d/%d stream %d flags %08x",
			__entry->epnum, __entry->dir_in ? "in" : "out",
			__print_symbolic(__entry->type,
				   { USB_ENDPOINT_XFER_INT,	"intr" },
				   { USB_ENDPOINT_XFER_CONTROL,	"control" },
				   { USB_ENDPOINT_XFER_BULK,	"bulk" },
				   { USB_ENDPOINT_XFER_ISOC,	"isoc" }),
			__entry->urb, __entry->pipe, __entry->slot_id,
			__entry->actual, __entry->length, __entry->num_mapped_sgs,
			__entry->num_sgs, __entry->stream, __entry->flags
		)
);

DEFINE_EVENT(musb_host_log_ep, musb_host_urb_giveback,
	TP_PROTO(struct urb *urb),
	TP_ARGS(urb)
);

#endif /* __MUSB_TRACE_H */

/* this part has to be here */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE musb_trace

#include <trace/define_trace.h>

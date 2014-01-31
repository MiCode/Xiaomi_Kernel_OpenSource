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
#define TRACE_SYSTEM rmnet_data
#define TRACE_INCLUDE_FILE rmnet_data_trace

#if !defined(_TRACE_MSM_LOW_POWER_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _RMNET_DATA_TRACE_H_

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(rmnet_handler_template,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb),

	TP_STRUCT__entry(
		__field(void *, skbaddr)
		__field(unsigned int, len)
		__string(name, skb->dev->name)
	),

	TP_fast_assign(
		__entry->skbaddr = skb;
		__entry->len = skb->len;
		__assign_str(name, skb->dev->name);
	),

	TP_printk("dev=%s skbaddr=%p len=%u",
		__get_str(name), __entry->skbaddr, __entry->len)
)

DEFINE_EVENT(rmnet_handler_template, rmnet_egress_handler,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb)
);

DEFINE_EVENT(rmnet_handler_template, rmnet_ingress_handler,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb)
);

DEFINE_EVENT(rmnet_handler_template, rmnet_vnd_start_xmit,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb)
);

DEFINE_EVENT(rmnet_handler_template, __rmnet_deliver_skb,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb)
);

#endif /* _RMNET_DATA_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>


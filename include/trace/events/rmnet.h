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
#define TRACE_SYSTEM rmnet

#if !defined(_TRACE_RMNET_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_RMNET_H

#include <linux/skbuff.h>
#include <linux/tracepoint.h>

TRACE_EVENT(rmnet_xmit_skb,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb),

	TP_STRUCT__entry(
		__string(dev_name, skb->dev->name)
		__field(unsigned int, len)
	),

	TP_fast_assign(
		__assign_str(dev_name, skb->dev->name);
		__entry->len = skb->len;
	),

	TP_printk("dev_name=%s len=%u", __get_str(dev_name), __entry->len)
);

#endif /* _TRACE_RMNET_H */

#include <trace/define_trace.h>

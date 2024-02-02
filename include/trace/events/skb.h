/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM skb

#if !defined(_TRACE_SKB_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SKB_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/tracepoint.h>

/*
 * Tracepoint for free an sk_buff:
 */
TRACE_EVENT(kfree_skb,

	TP_PROTO(struct sk_buff *skb, void *location),

	TP_ARGS(skb, location),

	TP_STRUCT__entry(
		__field(	void *,		skbaddr		)
		__field(	void *,		location	)
		__field(	unsigned short,	protocol	)
	),

	TP_fast_assign(
		__entry->skbaddr = skb;
		__entry->location = location;
		__entry->protocol = ntohs(skb->protocol);
	),

	TP_printk("skbaddr=%p protocol=%u location=%p",
		__entry->skbaddr, __entry->protocol, __entry->location)
);

TRACE_EVENT(consume_skb,

	TP_PROTO(struct sk_buff *skb),

	TP_ARGS(skb),

	TP_STRUCT__entry(
		__field(	void *,	skbaddr	)
	),

	TP_fast_assign(
		__entry->skbaddr = skb;
	),

	TP_printk("skbaddr=%p", __entry->skbaddr)
);

TRACE_EVENT(print_skb_gso,

	TP_PROTO(struct sk_buff *skb, __be16 src, __be16 dest),

	TP_ARGS(skb, src, dest),

	TP_STRUCT__entry(
		__field(void *,	skbaddr)
		__field(int, len)
		__field(int, data_len)
		__field(__be16, src)
		__field(__be16, dest)
	),

	TP_fast_assign(
		__entry->skbaddr = skb;
		__entry->len = skb->len;
		__entry->data_len = skb->data_len;
		__entry->src = src;
		__entry->dest = dest;
	),

	TP_printk("GSO: skbaddr=%pK, len=%d, data_len=%d, src=%u, dest=%u",
		__entry->skbaddr, __entry->len, __entry->data_len,
		be16_to_cpu(__entry->src), be16_to_cpu(__entry->dest))
);

TRACE_EVENT(skb_copy_datagram_iovec,

	TP_PROTO(const struct sk_buff *skb, int len),

	TP_ARGS(skb, len),

	TP_STRUCT__entry(
		__field(	const void *,		skbaddr		)
		__field(	int,			len		)
	),

	TP_fast_assign(
		__entry->skbaddr = skb;
		__entry->len = len;
	),

	TP_printk("skbaddr=%p len=%d", __entry->skbaddr, __entry->len)
);

#endif /* _TRACE_SKB_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ipa
#define TRACE_INCLUDE_FILE ipa_trace

#if !defined(_IPA_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _IPA_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(
	intr_to_poll3,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);

TRACE_EVENT(
	poll_to_intr3,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);

TRACE_EVENT(
	idle_sleep_enter3,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);

TRACE_EVENT(
	idle_sleep_exit3,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);

TRACE_EVENT(
	rmnet_ipa_netifni3,

	TP_PROTO(unsigned long rx_pkt_cnt),

	TP_ARGS(rx_pkt_cnt),

	TP_STRUCT__entry(
		__field(unsigned long,	rx_pkt_cnt)
	),

	TP_fast_assign(
		__entry->rx_pkt_cnt = rx_pkt_cnt;
	),

	TP_printk("rx_pkt_cnt=%lu", __entry->rx_pkt_cnt)
);

TRACE_EVENT(
	rmnet_ipa_netifrx3,

	TP_PROTO(unsigned long rx_pkt_cnt),

	TP_ARGS(rx_pkt_cnt),

	TP_STRUCT__entry(
		__field(unsigned long,	rx_pkt_cnt)
	),

	TP_fast_assign(
		__entry->rx_pkt_cnt = rx_pkt_cnt;
	),

	TP_printk("rx_pkt_cnt=%lu", __entry->rx_pkt_cnt)
);

TRACE_EVENT(
	rmnet_ipa_netif_rcv_skb3,

	TP_PROTO(const struct sk_buff *skb, unsigned long rx_pkt_cnt),

	TP_ARGS(skb, rx_pkt_cnt),

	TP_STRUCT__entry(
		__string(name,			skb->dev->name)
		__field(const void *,	skbaddr)
		__field(u16,			protocol)
		__field(unsigned int,	len)
		__field(unsigned int,	data_len)
		__field(unsigned long,	rx_pkt_cnt)
	),

	TP_fast_assign(
		__assign_str(name, skb->dev->name);
		__entry->skbaddr = skb;
		__entry->protocol = ntohs(skb->protocol);
		__entry->len = skb->len;
		__entry->data_len = skb->data_len;
		__entry->rx_pkt_cnt = rx_pkt_cnt;
	),

	TP_printk("dev=%s skbaddr=%p protocol=0x%04x len=%u data_len=%u rx_pkt_cnt=%lu",
		__get_str(name),
		__entry->skbaddr,
		__entry->protocol,
		__entry->len,
		__entry->data_len,
		__entry->rx_pkt_cnt)
);

TRACE_EVENT(
	ipa3_rx_poll_num,

	TP_PROTO(int poll_num),

	TP_ARGS(poll_num),

	TP_STRUCT__entry(
		__field(int,	poll_num)
	),

	TP_fast_assign(
		__entry->poll_num = poll_num;
	),

	TP_printk("each_poll_aggr_pkt_num=%d", __entry->poll_num)
);

TRACE_EVENT(
	ipa3_rx_poll_cnt,

	TP_PROTO(int poll_num),

	TP_ARGS(poll_num),

	TP_STRUCT__entry(
		__field(int,	poll_num)
	),

	TP_fast_assign(
		__entry->poll_num = poll_num;
	),

	TP_printk("napi_overall_poll_pkt_cnt=%d", __entry->poll_num)
);

TRACE_EVENT(
	ipa3_napi_schedule,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);

TRACE_EVENT(
	ipa3_napi_poll_entry,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);


TRACE_EVENT(
	ipa3_napi_poll_exit,

	TP_PROTO(unsigned long client, u32 cnt, u32 len),

	TP_ARGS(client, cnt, len),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
		__field(unsigned int,   cnt)
		__field(unsigned int,   len)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->cnt = cnt;
		__entry->len = len;
	),

	TP_printk("client=%lu napi weight cnt = %d sys->len = %d", __entry->client, __entry->cnt,  __entry->len)
);

TRACE_EVENT(
	ipa3_tx_dp,

	TP_PROTO(const struct sk_buff *skb, unsigned long client),

	TP_ARGS(skb, client),

	TP_STRUCT__entry(
		__string(name,			skb->dev->name)
		__field(const void *,	skbaddr)
		__field(u16,			protocol)
		__field(unsigned int,	len)
		__field(unsigned int,	data_len)
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__assign_str(name, skb->dev->name);
		__entry->skbaddr = skb;
		__entry->protocol = ntohs(skb->protocol);
		__entry->len = skb->len;
		__entry->data_len = skb->data_len;
		__entry->client = client;
	),

	TP_printk("dev=%s skbaddr=%p protocol=0x%04x len=%u data_len=%u client=%lu",
		__get_str(name),
		__entry->skbaddr,
		__entry->protocol,
		__entry->len,
		__entry->data_len,
		__entry->client)
);

TRACE_EVENT(
	ipa3_tx_done,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);

TRACE_EVENT(
	ipa3_lan_rx_pyld_hdlr_entry,

	TP_PROTO(const struct sk_buff *skb, unsigned long client),

	TP_ARGS(skb, client),

	TP_STRUCT__entry(
		__field(unsigned int,	len)
		__field(unsigned int,	data_len)
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->len = skb->len;
		__entry->data_len = skb->data_len;
		__entry->client = client;
	),

	TP_printk("len=%u data_len=%u client=%lu",
		__entry->len,
		__entry->data_len,
		__entry->client)
);

TRACE_EVENT(
	ipa3_lan_rx_pyld_hdlr_exit,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);

TRACE_EVENT(
	ipa3_lan_rx_cb_entry,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);

TRACE_EVENT(
	ipa3_lan_rx_cb_exit,

	TP_PROTO(unsigned long client),

	TP_ARGS(client),

	TP_STRUCT__entry(
		__field(unsigned long,	client)
	),

	TP_fast_assign(
		__entry->client = client;
	),

	TP_printk("client=%lu", __entry->client)
);
#endif /* _IPA_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../techpack/dataipa/drivers/platform/msm/ipa/ipa_v3
#include <trace/define_trace.h>

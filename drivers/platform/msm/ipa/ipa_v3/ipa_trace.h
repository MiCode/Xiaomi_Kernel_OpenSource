/* SPDX-License-Identifier: GPL-2.0 */
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


#endif /* _IPA_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/platform/msm/ipa/ipa_v3
#include <trace/define_trace.h>

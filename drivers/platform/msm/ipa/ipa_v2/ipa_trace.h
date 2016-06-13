/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM ipa
#define TRACE_INCLUDE_FILE ipa_trace

#if !defined(_IPA_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _IPA_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(
	intr_to_poll,

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
	poll_to_intr,

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
	idle_sleep_enter,

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
	idle_sleep_exit,

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
	rmnet_ipa_netifni,

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
	rmnet_ipa_netifrx,

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
	rmnet_ipa_netif_rcv_skb,

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
#endif /* _IPA_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>

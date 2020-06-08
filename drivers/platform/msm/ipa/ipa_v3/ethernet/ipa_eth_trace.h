/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM ipa_eth

#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE ipa_eth_trace

#if !defined(_IPA_ETH_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _IPA_ETH_TRACE_H

#include <linux/tracepoint.h>

#include "ipa_eth_i.h"

/*
 * Tracepoint for tracking all packets received through IPA LAN exception.
 */
TRACE_EVENT(lan_rx_skb,
	TP_PROTO(struct ipa_eth_channel *ch, struct sk_buff *skb),
	TP_ARGS(ch, skb),
	TP_STRUCT__entry(
		__field(const char *, dev)
		__field(const struct ethhdr *, eth_hdr)
		__field(unsigned int, eth_proto)
		__field(unsigned int, skb_len)
		__field(u64, count_total)
		__field(u64, count_drops)
		__field(u64, count_loopback)
	),
	TP_fast_assign(
		__entry->dev = ch->eth_dev->net_dev->name;
		__entry->eth_hdr = (struct ethhdr *)skb->data;
		__entry->eth_proto = ntohs(__entry->eth_hdr->h_proto);
		__entry->skb_len = skb->len;
		__entry->count_total = ch->exception_total;
		__entry->count_drops = ch->exception_drops;
		__entry->count_loopback = ch->exception_loopback;
	),
	TP_printk(
		"dev=%s count_total=%llu count_drops=%llu count_loopback=%llu eth_proto=%04X skb_len=%u",
		__entry->dev, __entry->count_total, __entry->count_drops,
		__entry->count_loopback, __entry->eth_proto, __entry->skb_len
	)
);

/*
 * Tracepoint for ethernet link activity check by PM.
 */
TRACE_EVENT(net_check_active,
	TP_PROTO(
		struct ipa_eth_device *eth_dev,
		struct rtnl_link_stats64 *old_stats,
		struct rtnl_link_stats64 *new_stats,
		unsigned long assume_active
	),
	TP_ARGS(eth_dev, old_stats, new_stats, assume_active),
	TP_STRUCT__entry(
		__field(const char *, dev)
		__field(u64, old_rx_packets)
		__field(u64, new_rx_packets)
		__field(u64, diff_rx_packets)
		__field(unsigned long, assume_active)
	),
	TP_fast_assign(
		__entry->dev = eth_dev->net_dev->name;
		__entry->old_rx_packets = old_stats->rx_packets;
		__entry->new_rx_packets = new_stats->rx_packets;
		__entry->diff_rx_packets =
			__entry->new_rx_packets - __entry->old_rx_packets;
		__entry->assume_active = assume_active;
	),
	TP_printk("dev=%s assume_active=%lu rx_total=%llu rx_diff=+%llu",
		__entry->dev, __entry->assume_active,
		__entry->new_rx_packets, __entry->diff_rx_packets
	)
);

#endif /* _IPA_ETH_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

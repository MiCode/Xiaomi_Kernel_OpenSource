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
		__field(int, queue)
		__field(int, ep_num)
		__field(const struct ethhdr *, eth_hdr)
		__field(unsigned int, eth_proto)
		__field(unsigned int, skb_len)
	),
	TP_fast_assign(
		__entry->dev = ch->eth_dev->net_dev->name;
		__entry->queue = ch->queue;
		__entry->ep_num = ch->ipa_ep_num;
		__entry->eth_hdr = (struct ethhdr *)skb->data;
		__entry->eth_proto = ntohs(__entry->eth_hdr->h_proto);
		__entry->skb_len = skb->len;
	),
	TP_printk("dev=%s queue=%d ep_num=%d eth_proto=%04X skb_len=%u",
		__entry->dev, __entry->queue, __entry->ep_num,
		__entry->eth_proto, __entry->skb_len
	)
);

#endif /* _IPA_ETH_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

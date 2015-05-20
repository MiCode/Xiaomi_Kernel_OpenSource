/*
 * Copyright 2013-2014  Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Packet filtering
 *
 * Checks if differnt type of packets should be filterd.
 *	1. Gratuitous ARP
 *	2. Unsolicited Neighbor Advertisement
 *	3. Frames Encrypted using the GTK
 */

#ifndef PACKET_FILTER_H
#define PACKET_FILTER_H
#include <linux/if_arp.h>
#include <net/ip_fib.h>
#include <net/ip.h>
#include <net/ndisc.h>
#include <net/ipv6.h>

/**
 * ieee80211_is_shared_gtk - packet is GTK
 * @skb: the input packet, must be an ethernet frame already
 *
 * Return: %true if the packet is Encrypted using the GTK .
 * This is used to drop packets that shouldn't occur because the AP implements
 * a proxy service.
 */
static inline bool ieee80211_is_shared_gtk(struct sk_buff *skb)
{
	const struct ethhdr *eth = (void *)skb->data;
	const struct iphdr *ipv4;
	const struct ipv6hdr *ipv6;
	const struct in6_addr *saddr;
	struct fib_result res;
	struct flowi4 fl4;

	switch (eth->h_proto) {
	case cpu_to_be16(ETH_P_IP):
		ipv4 = (void *)(eth + 1);
		fl4.daddr = ipv4->daddr;
		fl4.saddr = ipv4->saddr;

		if (!fib_lookup(dev_net(skb->dev), &fl4, &res))
			if (res.type == RTN_MULTICAST ||
			    res.type == RTN_BROADCAST)
				return true;
		break;
	case cpu_to_be16(ETH_P_IPV6):
		ipv6 = (void *)(eth + 1);
		saddr = &ipv6->saddr;

		if (ipv6_addr_is_multicast(saddr))
			return true;
	}

	return false;
}

/**
 * ieee80211_is_gratuitous_arp_unsolicited_na - packet is grat. ARP/unsol. NA
 * @skb: the input packet, must be an ethernet frame already
 *
 * Return: %true if the packet is a gratuitous ARP or unsolicited NA packet.
 * This is used to drop packets that shouldn't occur because the AP implements
 * a proxy service.
 */
static inline bool
ieee80211_is_gratuitous_arp_unsolicited_na(struct sk_buff *skb)
{
	const struct ethhdr *eth = (void *)skb->data;

	const struct {
		struct arphdr hdr;
		u8 ar_sha[ETH_ALEN];
		u8 ar_sip[4];
		u8 ar_tha[ETH_ALEN];
		u8 ar_tip[4];
	} __packed *arp;
	const struct ipv6hdr *ipv6;
	const struct icmp6hdr *icmpv6;

	switch (eth->h_proto) {
	case cpu_to_be16(ETH_P_ARP):
		/* can't say - but will probably be dropped later anyway */
		if (!pskb_may_pull(skb, sizeof(*eth) + sizeof(*arp)))
			return false;

		arp = (void *)(eth + 1);

		if ((arp->hdr.ar_op == cpu_to_be16(ARPOP_REPLY) ||
		     arp->hdr.ar_op == cpu_to_be16(ARPOP_REQUEST)) &&
		    !memcmp(arp->ar_sip, arp->ar_tip, sizeof(arp->ar_sip)))
			return true;
		break;
	case cpu_to_be16(ETH_P_IPV6):
		/* can't say - but will probably be dropped later anyway */
		if (!pskb_may_pull(skb, sizeof(*eth) + sizeof(*ipv6) +
					sizeof(*icmpv6)))
			return false;

		ipv6 = (void *)(eth + 1);
		icmpv6 = (void *)(ipv6 + 1);

		if (icmpv6->icmp6_type == NDISC_NEIGHBOUR_ADVERTISEMENT &&
		    !icmpv6->icmp6_solicited)
			return true;
		break;
	default:
		/*
		 * no need to support other protocols, proxy service isn't
		 * specified for any others
		 */
		break;
	}

	return false;
}

#endif /* PACKET_FILTER_H */

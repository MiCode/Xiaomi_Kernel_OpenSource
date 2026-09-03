// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <linux/icmp.h>
#include <linux/ipv6.h>
#include <net/ip6_checksum.h>

#include "sfp.h"
#include "sfp_hash.h"

#define SFP_SPECIAL_TTL_MINUS 2
#define DEFAULT_BUFFER_PREFIX_OFFSET 14

unsigned int sfp_stats_bytes;

static void FP_PRT_PKT(int dbg_lvl, void *data, int outif, char *iface)
{
	void *l3hdhr;
	struct iphdr *ip4hdr;
	struct ipv6hdr *ip6hdr;
	u32 l4offset;
	u8 ver, l4proto;

	if (!(fp_dbg_lvl & dbg_lvl))
		return;

	if (!data)
		return;

	l3hdhr = data;
	ver = ((struct iphdr *)l3hdhr)->version;

	switch (ver) {
	case 0x4:
	{
		u32 src, dst;

		ip4hdr = (struct iphdr *)l3hdhr;
		l4offset = ip4hdr->ihl << 2;
		l4proto = ip4hdr->protocol;
		src = ntohl(ip4hdr->saddr);
		dst = ntohl(ip4hdr->daddr);
		switch (l4proto) {
		case IP_L4_PROTO_TCP:
		{
			struct tcphdr *hp = (struct tcphdr *)(data + l4offset);

			FP_PRT_DBG(dbg_lvl,
				   "%s:SIP=%pI4, DIP=%pI4, IP/TCP, " IPID
				   ", cksum(%x), " TCP_FMT ", len=%d, %s, out if (%d).\n",
				   iface, &src, &dst,
				   ntohs(ip4hdr->id),
				   hp->check,
				   ntohl(hp->seq),
				   ntohl(hp->ack_seq),
				   ntohs(hp->source),
				   ntohs(hp->dest),
				   ntohs(ip4hdr->tot_len),
				   get_tcp_flag(hp),
				   outif);
			return;
		}
		case IP_L4_PROTO_UDP:
		{
			struct udphdr *hp = (struct udphdr *)(data + l4offset);

			FP_PRT_DBG(dbg_lvl,
				   "%s:SIP=%pI4, DIP=%pI4, IP/UDP, " IPID
				   ", cksum(%x), %d -> %d, len=%d, out if (%d).\n",
				   iface, &src, &dst,
				   ntohs(ip4hdr->id),
				   hp->check,
				   ntohs(hp->source),
				   ntohs(hp->dest),
				   ntohs(ip4hdr->tot_len),
				   outif);
			return;
		}
		default:
			FP_PRT_DBG(dbg_lvl,
				   "%s:SIP=%pI4, DIP=%pI4, IP/%d, " IPID
				   ", len=%d, out if (%d).\n",
				   iface, &src, &dst,
				   ip4hdr->protocol,
				   ntohs(ip4hdr->id),
				   ntohs(ip4hdr->tot_len),
				   outif);
			return;
		}
	}
	case 0x6:
	{
		struct in6_addr src;
		struct in6_addr dst;

		ip6hdr = (struct ipv6hdr *)l3hdhr;
		l4offset = sizeof(struct ipv6hdr);
		l4proto = ip6hdr->nexthdr;
		src = ip6hdr->saddr;
		dst = ip6hdr->daddr;

		switch (l4proto) {
		case IP_L4_PROTO_TCP:
		{
			struct tcphdr *hp = (struct tcphdr *)(data + l4offset);

			FP_PRT_DBG(dbg_lvl,
				   "%s:SIP=%pI6, DIP=%pI6, IP6/TCP, cksum(%x), "
				   TCP_FMT
				   ", len=%d, %s, out if (%d).\n",
				   iface, &src, &dst,
				   hp->check,
				   ntohl(hp->seq),
				   ntohl(hp->ack_seq),
				   ntohs(hp->source),
				   ntohs(hp->dest),
				   ntohs(ip6hdr->payload_len),
				   get_tcp_flag(hp),
				   outif);
			return;
		}
		case IP_L4_PROTO_UDP:
		{
			struct udphdr *hp = (struct udphdr *)(data + l4offset);

			FP_PRT_DBG(dbg_lvl,
				   "%s:SIP=%pI6, DIP=%pI6, IP6/UDP, cksum(%x), %d -> %d, len=%d, out if (%d).\n",
				   iface, &src, &dst,
				   hp->check,
				   ntohs(hp->source),
				   ntohs(hp->dest),
				   ntohs(ip6hdr->payload_len),
				   outif);
			return;
		}
		default:
			FP_PRT_DBG(dbg_lvl,
				   "%s:SIP=%pI6, DIP=%pI6, IP6/%d, len=%d, out if (%d).\n",
				   iface, &src, &dst,
				   ip6hdr->nexthdr,
				   ntohs(ip6hdr->payload_len),
				   outif);
			return;
		}
	}
	default:
		FP_PRT_DBG(FP_PRT_DEBUG, "unknown pkt type.\n");
		return;
	}
}

/*
 *  Check the L4 protocol type and
 *  return the source port and destination port
 *  from the packet.
 */
static u8 sfp_check_l4proto(u16 l3proto,
			    void *iph,
			    u16 *dstport,
			    u16 *srcport)
{
	u8 proto;

	if (l3proto == NFPROTO_IPV4) {
		struct iphdr *iphdr2 = (struct iphdr *)iph;

		proto = iphdr2->protocol;
	} else {
		struct ipv6hdr *ip6hdr = (struct ipv6hdr *)iph;

		proto = ip6hdr->nexthdr;
	}
	switch (proto) {
	case IP_L4_PROTO_TCP:
		fallthrough;
	case IP_L4_PROTO_UDP:
		fallthrough;
	case IP_L4_PROTO_ICMP:
		fallthrough;
	case IP_L4_PROTO_ICMP6:
		break;
	default:
		/* Skip NAT processing at all */
		proto = IP_L4_PROTO_NULL;
	}
	return proto;
}

void sfp_update_csum_incremental(__sum16 *check, struct sk_buff *skb,
				 u8 l3proto, __be32 oldip, __be32 newip,
				 __be16 oldport, __be16 newport)
{
	if (l3proto == NFPROTO_IPV4) {
		inet_proto_csum_replace4(check, skb, oldip, newip, true);
		inet_proto_csum_replace2(check, skb, oldport, newport, false);
	} else {
		inet_proto_csum_replace2(check, skb, oldport, newport, false);
	}
}

/* Do checksum */
void sfp_update_checksum(void *ipheader,
			 int  pkt_totlen,
			 u16 l3proto,
			 u32 l3offset,
			 u8 l4proto,
			 u32 l4offset)
{
	struct iphdr *iphhdr;
	struct ipv6hdr *ip6hdr;

	/* Do tcp checksum */
	switch (l4proto) {
	case IP_L4_PROTO_TCP:
	{
		unsigned int offset;
		__wsum payload_csum;
		struct tcphdr *th;

		th = (struct tcphdr *)(ipheader + l4offset);
		th->check = 0;
		offset = l4offset + th->doff * 4;
		payload_csum = csum_partial(ipheader + offset,
					    pkt_totlen - offset,
					    0);
		if (l3proto == NFPROTO_IPV4) {
			iphhdr = (struct iphdr *)ipheader;
			th->check = tcp_v4_check(pkt_totlen - l4offset,
						 iphhdr->saddr,
						 iphhdr->daddr,
						 csum_partial(th,
							      th->doff << 2,
							      payload_csum));
		} else {
			ip6hdr = (struct ipv6hdr *)ipheader;
			th->check = tcp_v6_check(pkt_totlen - l4offset,
						 (struct in6_addr *)&ip6hdr->saddr,
						 (struct in6_addr *)&ip6hdr->daddr,
						 csum_partial(th, th->doff << 2,
							      payload_csum));
		}
		return;
	}
	case IP_L4_PROTO_UDP:
	{
		/* Do udp checksum */
		int l4len, offset;
		struct udphdr *uh;
		__wsum payload_csum;

		uh = (struct udphdr *)(ipheader + l4offset);
		uh->check = 0;
		l4len = 8;
		offset = l4offset + 8;
		payload_csum = csum_partial(ipheader + offset,
					    pkt_totlen - offset,
					    0);
		if (l3proto == NFPROTO_IPV4) {
			iphhdr = (struct iphdr *)ipheader;
			uh->check = udp_v4_check(pkt_totlen - l4offset,
						 iphhdr->saddr,
						 iphhdr->daddr,
						 csum_partial(uh,
							      8,
							      payload_csum));
		} else {
			ip6hdr = (struct ipv6hdr *)ipheader;

			uh->check =
				udp_v6_check(pkt_totlen - l4offset,
					     (struct in6_addr *)&ip6hdr->saddr,
					     (struct in6_addr *)&ip6hdr->daddr,
					     csum_partial(uh, 8, payload_csum));
		}
		return;
	}
	default:
		return;
	}
}

/*
 * Update the ip header and tcp header
 * and add mac header
 */
static bool sfp_update_pkt_header(int ifindex,
				  void *data,
				  u32 l3offset,
				  u32 l4offset,
				  struct sfp_trans_tuple ret_info,
				  struct nf_conntrack_tuple *tuple)
{
	struct tcphdr *ptcphdr;
	struct udphdr *pudphdr;
	struct icmphdr *picmphdr;
	struct icmp6hdr *picmp6hdr;
	struct ethhdr *peth;
	u8 l4proto;
	u8 l3proto;
	struct iphdr *iphhdr;
	struct ipv6hdr *ip6hdr;
	u8 *l3data;
	struct sk_buff *skb;
	__be32 oldsrcip = 0;
	__be32 olddstip = 0;
	__be32 newsrcip = 0;
	__be32 newdstip = 0;
	__be16 oldsrcport, newsrcport;
	__be16 olddstport, newdstport;
	__be16 oldicmpid, newicmpid;
	__be16 oldicmp6id, newicmp6id;
	u8 dir = 0;
	__sum16 *check;

	skb = (struct sk_buff *)data;
	l3data = skb_network_header(skb);

	peth = (struct ethhdr *)(l3data - ETH_HLEN);

	l3proto = ret_info.trans_info.l3_proto;
	l4proto = ret_info.trans_info.l4_proto;
	dir = tuple->dst.dir;

	if (l4proto == IP_L4_PROTO_TCP) {
		ptcphdr = (struct tcphdr *)((unsigned char *)l3data + l4offset);
		if (ptcphdr->fin == 1 || ptcphdr->rst == 1) {
			FP_PRT_DBG(FP_PRT_DEBUG,
				   "Get TCP FIN or RST pkt(%u-%u).(%d-%d)\n",
				   ntohs(ptcphdr->dest),
				   ntohs(ptcphdr->source),
				   ptcphdr->fin,
				   ptcphdr->rst);
			return false;
		}
	}

	switch (l3proto) {
	case NFPROTO_IPV4:
		iphhdr = (struct iphdr *)l3data;
		oldsrcip = iphhdr->saddr;
		olddstip = iphhdr->daddr;
		iphhdr->daddr = ret_info.trans_info.dst_ip.ip;
		newdstip = iphhdr->daddr;
		iphhdr->saddr = ret_info.trans_info.src_ip.ip;
		newsrcip = iphhdr->saddr;
		/* make it special, easy to distinguish */
		if (iphhdr->ttl > SFP_SPECIAL_TTL_MINUS)
			iphhdr->ttl -= SFP_SPECIAL_TTL_MINUS;
		else
			iphhdr->ttl--;
		ip_send_check(iphhdr);
		peth->h_proto = htons(ETH_P_IP);
		break;
	case NFPROTO_IPV6:
		ip6hdr = (struct ipv6hdr *)l3data;
		if (ip6hdr->hop_limit > SFP_SPECIAL_TTL_MINUS)
			ip6hdr->hop_limit -= SFP_SPECIAL_TTL_MINUS;
		else
			ip6hdr->hop_limit--;
		peth->h_proto = htons(ETH_P_IPV6);
		break;
	default:
		break;
	}
	switch (l4proto) {
	case IP_L4_PROTO_TCP:
		ptcphdr = (struct tcphdr *)((unsigned char *)l3data + l4offset);
		check = &ptcphdr->check;
		oldsrcport = ptcphdr->source;
		olddstport = ptcphdr->dest;
		ptcphdr->dest = ret_info.trans_info.dst_l4_info.all;
		newdstport = ptcphdr->dest;
		ptcphdr->source = ret_info.trans_info.src_l4_info.all;
		newsrcport = ptcphdr->source;
		ether_addr_copy(peth->h_dest, ret_info.trans_mac_info.dst_mac);
		ether_addr_copy(peth->h_source, ret_info.trans_mac_info.src_mac);
		sfp_update_csum_incremental(check, skb, l3proto, oldsrcip,
					    newsrcip, oldsrcport, newsrcport);
		sfp_update_csum_incremental(check, skb, l3proto, olddstip,
					    newdstip, olddstport, newdstport);
		break;
	case IP_L4_PROTO_UDP:
		pudphdr = (struct udphdr *)((unsigned char *)l3data + l4offset);
		check = &pudphdr->check;
		if (*check == 0)
			break;
		oldsrcport = pudphdr->source;
		olddstport = pudphdr->dest;
		pudphdr->dest = ret_info.trans_info.dst_l4_info.all;
		newdstport = pudphdr->dest;
		pudphdr->source = ret_info.trans_info.src_l4_info.all;
		newsrcport = pudphdr->source;
		ether_addr_copy(peth->h_dest, ret_info.trans_mac_info.dst_mac);
		ether_addr_copy(peth->h_source, ret_info.trans_mac_info.src_mac);
		sfp_update_csum_incremental(check, skb, l3proto, oldsrcip,
					    newsrcip, oldsrcport, newsrcport);
		sfp_update_csum_incremental(check, skb, l3proto, olddstip,
					    newdstip, olddstport, newdstport);
		break;
	case IP_L4_PROTO_ICMP:
		picmphdr =
			(struct icmphdr *)((unsigned char *)l3data + l4offset);
		/*icmp id in dst_l4_info*/
		check = &picmphdr->checksum;
		oldicmpid = picmphdr->un.echo.id;
		picmphdr->un.echo.id = ret_info.trans_info.dst_l4_info.all;
		newicmpid = picmphdr->un.echo.id;
		ether_addr_copy(peth->h_dest, ret_info.trans_mac_info.dst_mac);
		ether_addr_copy(peth->h_source, ret_info.trans_mac_info.src_mac);
		inet_proto_csum_replace2(check, skb, oldicmpid,
								 newicmpid, false);
		break;
	case IP_L4_PROTO_ICMP6:
		picmp6hdr =
			(struct icmp6hdr *)((unsigned char *)l3data + l4offset);
		check = &picmp6hdr->icmp6_cksum;
		oldicmp6id = picmp6hdr->icmp6_identifier;
		picmp6hdr->icmp6_identifier =
			ret_info.trans_info.dst_l4_info.all;
		newicmp6id = picmp6hdr->icmp6_identifier;
		ether_addr_copy(peth->h_dest, ret_info.trans_mac_info.dst_mac);
		ether_addr_copy(peth->h_source, ret_info.trans_mac_info.src_mac);
		inet_proto_csum_replace2(check, skb, oldicmp6id,
								 newicmp6id, false);
		break;
	default:
		FP_PRT_DBG(FP_PRT_DEBUG, "Unexpected IP protocol.\n");
		return false;
	}

	sfp_stats_bytes += skb->len;

	return true;
}

static void sfp_maybe_trim_skb(struct sk_buff *skb)
{
	struct iphdr *iph;
	struct ipv6hdr *ipv6h;
	u32 len;

	iph = ip_hdr(skb);

	if (skb->protocol == htons(ETH_P_IP)) {
		len = ntohs(iph->tot_len);
		if (len != skb->len)
			pskb_trim_rcsum(skb, len);
	} else if (skb->protocol == htons(ETH_P_IPV6)) {
		ipv6h = (struct ipv6hdr *)iph;
		len = ntohs(ipv6h->payload_len) + sizeof(struct ipv6hdr);
		if (len != skb->len)
			pskb_trim_rcsum(skb, len);
	}
}

static bool sfp_check_filter_frag(struct sk_buff *skb)
{
	int err;
	bool is_frag = false;
	unsigned int ptr = 0;
	struct iphdr *piphdr;

	piphdr = ip_hdr(skb);

	if (piphdr->version == 0x04) {
		is_frag = ip_is_fragment(ip_hdr(skb));
	} else if (piphdr->version == 0x6) {
		err = ipv6_find_hdr(skb, &ptr, NEXTHDR_FRAGMENT, NULL, NULL);
		if (err >= 0) {
			FP_PRT_DBG(FP_PRT_DEBUG, "skb %p is v6 frag\n", skb);
			is_frag = true;
		}
	}

	return is_frag;
}

static int sfp_check_mod_pkts(u32 ifindex,
			      void *data,
			      u16 l3proto,
			      void *iph)
{
	struct sk_buff *skb;
	u8  proto;
	u16 srcport, dstport;
	struct sfp_trans_tuple ret_info;
	int out_ifindex;
	struct iphdr *iphdr2;
	__be32 dip, sip;
	struct ipv6hdr *ip6hdr;
	struct nf_conntrack_tuple tuple;
	u32 l3offset;
	u32 l4offset;
	u8 *ip_header;
	u32 totlen;
	int ret = -SFP_FAIL;
	bool sfp_ret = false;

	if (l3proto == NFPROTO_IPV4) {
		iphdr2 = (struct iphdr *)iph;
		dip = iphdr2->daddr;
		sip = iphdr2->saddr;
		proto = sfp_check_l4proto(l3proto, (void *)iphdr2,
					  &dstport, &srcport);
	} else {
		ip6hdr = (struct ipv6hdr *)iph;
		proto = sfp_check_l4proto(l3proto, (void *)ip6hdr,
					  &dstport, &srcport);
	}

	if (proto == IP_L4_PROTO_NULL) {
		/* NAT not supported for this protocol */
		FP_PRT_DBG(FP_PRT_DEBUG, "proto [%d] no supported.\n", proto);
		return ret; /* No support protocol */
	}

	memset(&tuple, 0, sizeof(struct nf_conntrack_tuple));
	skb = (struct sk_buff *)data;
	sfp_ret = sfp_pkt_to_tuple(skb->data,
				   skb_network_offset(skb),
				   &tuple,
				   &l4offset);
	l3offset = skb_network_offset(skb);
	ip_header = skb->data + skb_network_offset(skb);
	totlen = skb->len;

	if (!sfp_ret)
		return ret;

	/* Lookup fwd entries accordingly with 5 tuple key */
	ret = check_sfp_fwd_table(&tuple, &ret_info);
	if (ret != SFP_OK) {
		FP_PRT_DBG(FP_PRT_DEBUG, "SFP_MGR: no fwd entries.\n");
		return -ret;
	}
	out_ifindex = ret_info.out_ifindex;

	sfp_ret = sfp_update_pkt_header(ifindex,
					data, l3offset,
					l4offset, ret_info, &tuple);
	if (!sfp_ret)
		return -SFP_FAIL;

	if (skb->dev && skb->dev->header_ops)
		skb_push(skb, ETH_HLEN);

	return out_ifindex;
}

/*
 * 1.check the data if it will be forward. If it will be,
 * modify the header according to spread fast path forwarding
 * table sfpRuleDB and the return value is zero.
 * If it will be not, the return value is not zero.
 * Parameter:
 *    1.IN int in_if: SFP_INTERFACE_LTE or SFP_INTERFACE_USB
 *    2.IN unsigned char *pDataHeader: From USB, the pDataHeader is skb pointer,
 *      and From LTE,the pDataHeader is head pointer.
 *    3.INOUT int *offset:From USB, *offset is null,and from LTE,
 *      the offset is the interval between header which is ip header or
 *      mac header and head pointer.
 *    4.INOUT int *pDataLen: the pkt total length
 *    5.OUT int *out_if: the interface is which the pkt will be sent to.
 *    Return: 0. the pkt will  be forward and the ip header and udp or
 *                tcp header is ready.
 *            >0. the pkt should be sent to network subsystem.
 */
int soft_fastpath_process(int in_if,
			  void *data_header,
			  unsigned short *offset,
			  int *plen,
			  int *out_if)
{
	struct iphdr *piphdr;
	struct sk_buff *skb;
	char *link_dir;

	if (get_sfp_enable() == 0)
		return 1;

	if (in_if == SFP_INTERFACE_LTE)
		link_dir = "LTE IN";
	else
		link_dir = "USB IN";

	/* Check whether is ip or ip6 header */
	skb = (struct sk_buff *)data_header;

	if (!get_sfp_tether_scheme()) {
		if (is_banned_ipa_netdev(skb->dev))
			return 1;
	}

	skb_reset_network_header(skb);
	piphdr = ip_hdr(skb);

	/* aqc driver may pass some pkts with 6 bytes paddings */
	sfp_maybe_trim_skb(skb);

	/* ignore the frag-ed pkt in hook func */
	if (sfp_check_filter_frag(skb))
		return 1;
	/*
	 * Some ethernet drivers(For example, atlantic-fwd)
	 * support a feature NETIF_F_SG, so the skb from them
	 * could be nonlinear. The payload may be scattered
	 * into different pages.
	 * If we do not linearize it, the csum will be incorrect.
	 * If return -ENOMEM, we do not free it, we pass it to
	 * IP stack.
	 */
	if (skb_linearize(skb)) {
		FP_PRT_DBG(FP_PRT_WARN,
			   "nomem to linear, pass to IP stack\n");
		return -ENOMEM;
	} else {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "skb_linearize, obtain the ip header again\n");
		piphdr = ip_hdr(skb);
	}

	if (piphdr->version == 0x04) {
		*out_if = sfp_check_mod_pkts(in_if,
					     (void *)skb,
					     NFPROTO_IPV4,
					     piphdr);

		FP_PRT_PKT(FP_PRT_INFO, piphdr, *out_if, link_dir);
		if (*out_if < 0)
			return 1;
		else
			return 0;
	} else {
		struct ipv6hdr *ip6hdr = (struct ipv6hdr *)piphdr;
		*out_if = sfp_check_mod_pkts(in_if,
					     (void *)skb,
					     NFPROTO_IPV6,
					     ip6hdr);

		FP_PRT_PKT(FP_PRT_INFO, piphdr, *out_if, link_dir);
		if (*out_if < 0)
			return 1;
		else
			return 0;
	}

	return 1;
}
EXPORT_SYMBOL(soft_fastpath_process);

struct net_device *netdev_get_by_index(int ifindex)
{
	struct net_device *dev;

	if (!ifindex)
		return NULL;
	rcu_read_lock();
	dev = dev_get_by_index_rcu(&init_net, ifindex);
	if (dev)
		dev_hold(dev);
	rcu_read_unlock();
	return dev;
}
EXPORT_SYMBOL(netdev_get_by_index);


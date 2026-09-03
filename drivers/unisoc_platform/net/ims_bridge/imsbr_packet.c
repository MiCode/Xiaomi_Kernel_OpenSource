// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2016 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "sprd-imsbr: " fmt

#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ratelimit.h>
#include <linux/skbuff.h>
#include <net/flow.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <net/icmp.h>
#include <net/route.h>
#include <net/udp.h>
#include <uapi/linux/ims_bridge/ims_bridge.h>
#include <net/ip6_checksum.h>
#include <net/tcp.h>

#include "imsbr_core.h"
#include "imsbr_sipc.h"
#include "imsbr_hooks.h"
#include "imsbr_packet.h"

static int tcpdump_enable __read_mostly;
module_param(tcpdump_enable, int, 0644);

static unsigned int vowifi_in_mark __read_mostly;
module_param(vowifi_in_mark, uint, 0644);

uint imsbr_frag_size __read_mostly = IMSBR_PACKET_MAXSZ;
module_param_named(frag_size, imsbr_frag_size, uint, 0644);

/* XXX - when app vpn is enabled, a specific ip rule will be added:
 * "ip rule from all fwmark 0x0/0x20000 iif lo uidrange 0-99999 lookup tun0".
 * Thus if the packet's input device is "lo", it maybe routed to "tun0" and
 * stolen to the userspace, so use "dummy0" instead of "lo" here.
 */
static char *input_device __read_mostly = "dummy0";
module_param(input_device, charp, 0644);

static char *backhole_device __read_mostly = "dummy0";
module_param(backhole_device, charp, 0644);

static DEFINE_MUTEX(imsbr_reasm_lock);
static struct sk_buff *imsbr_reasm_skb;

/* Prevent one direction's log suppress another, we do rate limit
 * separately
 */
static DEFINE_RATELIMIT_STATE(rlimit_tocp_pkt, 5 * HZ, 5);
static DEFINE_RATELIMIT_STATE(rlimit_fromcp_pkt, 5 * HZ, 5);

static struct sk_buff *imsbr_allocskb(int size, gfp_t priority)
{
	struct sk_buff *skb;
	int tlen;

	tlen = size + MAX_HEADER;
	skb = alloc_skb(tlen, priority);
	if (!skb) {
		IMSBR_STAT_INC(imsbr_stats->sk_buff_fail);
		pr_err("alloc_skb len=%d failed\n", tlen);
		return NULL;
	}

	skb_reserve(skb, MAX_HEADER - sizeof(struct ethhdr));
	skb_reset_mac_header(skb);
	skb_reserve(skb, sizeof(struct ethhdr));
	skb_reset_network_header(skb);

	return skb;
}

static struct sk_buff *imsbr_pkt2skb(void *pkt, int pktlen)
{
	struct sk_buff *skb;

	skb = imsbr_allocskb(pktlen, GFP_KERNEL);
	if (!skb)
		return NULL;

	unalign_memcpy(skb->data, pkt, pktlen);
	skb_put(skb, pktlen);

	return skb;
}

static void imsbr_flowi4_init(struct flowi4 *fl4, struct iphdr *ip)
{
	__u8 proto;
	void *l4ptr;

	proto = ip->protocol;

	memset(fl4, 0, sizeof(*fl4));
	fl4->flowi4_tos = RT_TOS(ip->tos);
	fl4->flowi4_proto = proto;
	fl4->flowi4_scope = RT_SCOPE_UNIVERSE;
	fl4->flowi4_flags = FLOWI_FLAG_ANYSRC;
	fl4->saddr = ip->saddr;
	fl4->daddr = ip->daddr;

	/* Don't worry, this data buffer must be linear! */
	l4ptr = (char *)ip + (ip->ihl * 4);
	if (proto == IPPROTO_UDP || proto == IPPROTO_TCP) {
		struct udphdr *uh = l4ptr;

		fl4->fl4_sport = uh->source;
		fl4->fl4_dport = uh->dest;
	} else if (proto == IPPROTO_ICMP) {
		struct icmphdr *ih = l4ptr;

		fl4->fl4_icmp_type = ih->type;
		fl4->fl4_icmp_code = ih->code;
	}
}

static void imsbr_packet_output_v4(struct sk_buff *skb, struct iphdr *ip)
{
	struct flowi4 fl4;
	struct rtable *rt;
	int err;

	imsbr_flowi4_init(&fl4, ip);
	security_skb_classify_flow(skb, flowi4_to_flowi_common(&fl4));

	rt = ip_route_output_key(&init_net, &fl4);
	if (IS_ERR(rt)) {
		IMSBR_STAT_INC(imsbr_stats->ip_route_fail);
		pr_err_ratelimited("ip_route_output_key s=%pI4 d=%pI4 e=%ld\n",
				   &ip->saddr, &ip->daddr, PTR_ERR(rt));
		kfree_skb(skb);
		return;
	}

	skb_dst_set(skb, &rt->dst);
	err = ip_local_out(&init_net, skb->sk, skb);
	if (unlikely(err)) {
		IMSBR_STAT_INC(imsbr_stats->ip_output_fail);
		pr_err_ratelimited("ip_local_out err=%d\n", err);
	}
}

static void imsbr_flowi6_init(struct flowi6 *fl6, struct ipv6hdr *ip6)
{
	__u8 proto;
	void *l4ptr;

	proto = ip6->nexthdr;
	memset(fl6, 0, sizeof(*fl6));
	fl6->flowi6_proto = proto;
	fl6->saddr = ip6->saddr;
	fl6->daddr = ip6->daddr;

	/* Don't worry, this data buffer must be linear! */
	l4ptr = (char *)ip6 + sizeof(*ip6);
	if (proto == IPPROTO_UDP || proto == IPPROTO_TCP) {
		struct udphdr *uh = l4ptr;

		fl6->fl6_sport = uh->source;
		fl6->fl6_dport = uh->dest;
	} else if (proto == IPPROTO_ICMPV6) {
		struct icmp6hdr *ih = l4ptr;

		fl6->fl6_icmp_type = ih->icmp6_type;
		fl6->fl6_icmp_code = ih->icmp6_code;
	}
}

static void imsbr_packet_output_v6(struct sk_buff *skb, struct ipv6hdr *ip6)
{
	struct flowi6 fl6;
	struct dst_entry *dst;
	int err;

	imsbr_flowi6_init(&fl6, ip6);
	security_skb_classify_flow(skb, flowi6_to_flowi_common(&fl6));
	fl6.flowi6_mark = skb->mark;
	dst = ip6_route_output(&init_net, NULL, &fl6);
	if (unlikely(dst->error)) {
		IMSBR_STAT_INC(imsbr_stats->ip6_route_fail);
		pr_err_ratelimited("ip6_route_output s=%pI6c d=%pI6c e=%d\n",
				   &ip6->saddr, &ip6->daddr, dst->error);
		dst_release(dst);
		goto freeit;
	}

	dst = xfrm_lookup(&init_net, dst, flowi6_to_flowi(&fl6), NULL, 0);
	if (IS_ERR(dst)) {
		IMSBR_STAT_INC(imsbr_stats->xfrm_lookup_fail);
		pr_err_ratelimited("xfrm_lookup s=%pI6c d=%pI6c p=%d e=%ld\n",
				   &ip6->saddr, &ip6->daddr, ip6->nexthdr,
				   PTR_ERR(dst));
freeit:
		kfree_skb(skb);
		return;
	}

	skb_dst_set(skb, dst);
	err = ip6_local_out(&init_net, skb->sk, skb);
	if (unlikely(err)) {
		IMSBR_STAT_INC(imsbr_stats->ip6_output_fail);
		pr_err_ratelimited("ip6_local_out err=%d\n", err);
	}
}

static void imsbr_packet_output(struct sk_buff *skb)
{
	struct iphdr *ip = ip_hdr(skb);

	/* Packets from cp are treated as the local generated packets, also
	 * CP's mtu is ALWAYS less than AP's, so we need not consider the
	 * TCP fragment issue.
	 */
	skb->ignore_df = 1;

	if (ip->version == 4) {
		imsbr_packet_output_v4(skb, ip);
	} else {
		/* Ipv6 route rule 11000 can not prevent apps to hit this rule,
		 * so add mark 0x80000000 for the rule to avoid apps hitting.
		 * For vowifi pkts,they need add mark for all.
		 */
		skb->mark |= 0x80000000;
		imsbr_packet_output_v6(skb, ipv6_hdr(skb));
	}
}

static void imsbr_packet_input(struct sk_buff *skb)
{
	struct iphdr *ip = ip_hdr(skb);
	struct net_device *dev;
	struct ethhdr *eth;

	dev = dev_get_by_name(&init_net, input_device);

	if (dev) {
		eth = (struct ethhdr *)skb_mac_header(skb);
		if (ip->version == 4)
			eth->h_proto = htons(ETH_P_IP);
		else
			eth->h_proto = htons(ETH_P_IPV6);

		eth_zero_addr(eth->h_dest);
		eth_zero_addr(eth->h_source);

		skb->protocol = eth->h_proto;
		skb->dev = dev;
		netif_rx_ni(skb);
		dev_put(dev);
	} else {
		kfree_skb(skb);
		pr_crit("device %s is not exist!\n", input_device);
	}
}

void imsbr_dumpcap(struct sk_buff *skb)
{
	const char mac[ETH_ALEN] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
	struct sk_buff *nskb;
	struct ethhdr *eth;
	struct iphdr *ip;
	struct net_device *dev;

	nskb = skb_realloc_headroom(skb, sizeof(struct ethhdr));
	if (!nskb) {
		IMSBR_STAT_INC(imsbr_stats->sk_buff_fail);
		pr_err("skb realloc headroom fail, len=%d\n", skb->len);
		return;
	}

	skb_push(nskb, sizeof(struct ethhdr));
	skb_reset_mac_header(nskb);

	ip = (struct iphdr *)skb_network_header(nskb);
	eth = (struct ethhdr *)skb_mac_header(nskb);
	if (ip->version == 4)
		eth->h_proto = htons(ETH_P_IP);
	else
		eth->h_proto = htons(ETH_P_IPV6);

	ether_addr_copy(eth->h_dest, mac);
	ether_addr_copy(eth->h_source, mac);

	dev = dev_get_by_name(&init_net, backhole_device);
	if (dev) {
		nskb->protocol = eth->h_proto;
		nskb->dev = dev;
		dev_queue_xmit(nskb);
		dev_put(dev);
	} else {
		kfree_skb(nskb);
	}
}

static void imsbr_packet_info(char *prefix, struct sk_buff *skb)
{
	struct iphdr *ip = ip_hdr(skb);

	if (ip->version == 4) {
		pr_info("%s: src=%pI4 dst=%pI4 p=%u len=%u mark=%#x\n",
			prefix, &ip->saddr, &ip->daddr, ip->protocol,
			ntohs(ip->tot_len), skb->mark);
	} else {
		struct ipv6hdr *ip6 = ipv6_hdr(skb);

		pr_info("%s: src=%pI6c dst=%pI6c p=%u len=%u mark=%#x\n",
			prefix, &ip6->saddr, &ip6->daddr, ip6->nexthdr,
			ntohs(ip6->payload_len), skb->mark);
	}
}

static int imsbr_fragsz(void)
{
	int min_val;

	min_val = min_t(int, imsbr_frag_size, IMSBR_PACKET_MAXSZ);
	if (imsbr_frag_size)
		return min_val;

	return IMSBR_PACKET_MAXSZ;
}

static void imsbr_valid_checksum(struct sk_buff *skb)
{
	int transport_offset;
	struct tcphdr *th;
	struct udphdr *uh;
	__wsum payload_csum;
	uint16_t his_sum = 0;
	uint16_t calc_sum = 0;

	struct iphdr *ip = ip_hdr(skb);

	if (ip->version == 6) {//v6 csum
		struct ipv6hdr *ipv6h = ipv6_hdr(skb);

		transport_offset = skb->len - skb->data_len - ntohs(ipv6h->payload_len);
		skb_set_transport_header(skb, transport_offset);

		if (ipv6h->nexthdr == NEXTHDR_TCP) {
			th = tcp_hdr(skb);
			payload_csum = csum_partial((char *)th + th->doff * 4,
							ntohs(ipv6h->payload_len) - th->doff * 4,
							0);//payload

			his_sum = th->check;
			th->check = 0;
			th->check = tcp_v6_check(skb->len - transport_offset,
						(struct in6_addr *)&ipv6h->saddr,
						(struct in6_addr *)&ipv6h->daddr,
						csum_partial((char *)th, th->doff << 2,
						payload_csum));//tcp header + pseudo head+ payload

			calc_sum = th->check;
		} else if (ipv6h->nexthdr == NEXTHDR_UDP) {
			uh = udp_hdr(skb);
			his_sum = uh->check;
			uh->check = 0;

			payload_csum = csum_partial((char *)uh + 8,
							ntohs(ipv6h->payload_len) - 8, 0);

			uh->check = udp_v6_check(skb->len - transport_offset,
						(struct in6_addr *)&ipv6h->saddr,
						(struct in6_addr *)&ipv6h->daddr,
						csum_partial((char *)uh, 8,
						payload_csum));

			calc_sum = uh->check;
		}
	} else {//v4 csum
		transport_offset = ip->ihl * 4;
		skb_set_transport_header(skb, transport_offset);

		if (ip->protocol == IPPROTO_TCP) {
			th = tcp_hdr(skb);
			his_sum = th->check;
			th->check = 0;

			payload_csum = csum_partial((char *)th + th->doff * 4,
							ntohs(ip->tot_len) - th->doff * 4
							- transport_offset, 0);

			th->check = tcp_v4_check(skb->len - transport_offset,
						ip->saddr, ip->daddr,
						csum_partial((char *)th, th->doff << 2,
						payload_csum));

			calc_sum = th->check;
		} else if (ip->protocol == IPPROTO_UDP) {
			uh = udp_hdr(skb);
			his_sum = uh->check;
			uh->check = 0;

			payload_csum = csum_partial((char *)uh + 8,
							ntohs(ip->tot_len) - 8 - transport_offset,
							0);

			uh->check = udp_v4_check(skb->len - transport_offset,
						ip->saddr, ip->daddr,
						csum_partial((char *)uh, 8,
						payload_csum));

			calc_sum = uh->check;
		}
	}

	pr_info("history checksum is 0x%x, calculated is 0x%x",
			ntohs(his_sum), ntohs(calc_sum));

	if (his_sum != calc_sum) {
		pr_info("his_sum != calc_sum, replace!");
	}
}

static void imsbr_frag_send(struct sk_buff *skb)
{
	struct imsbr_packet _pkthdr;
	struct imsbr_packet *pkt;
	struct sblock blk;
	char *src;
	int totlen = skb->len;
	int offset = 0;
	int len;

	pr_info_ratelimited("pktlen %d exceed max %d, frag it!\n", skb->len,
			    imsbr_fragsz());

	imsbr_valid_checksum(skb);

	IMSBR_STAT_INC(imsbr_stats->frag_create);

	while (totlen > 0) {
		len = totlen < imsbr_fragsz() ? totlen : imsbr_fragsz();

		if (imsbr_sblock_get(&imsbr_data, &blk, len)) {
			pr_err("frag %d packet fail!\n", skb->len);
			IMSBR_STAT_INC(imsbr_stats->frag_fail);
			return;
		}

		pr_debug("frag offset=%d len=%d reasm_tlen=%d\n",
			 offset, len, skb->len);

		pkt = blk.addr;

		INIT_IMSBR_PACKET(&_pkthdr, skb->len);
		_pkthdr.frag_off = offset;
		unalign_memcpy(pkt, &_pkthdr, sizeof(_pkthdr));

		src = skb->data + offset;
		unalign_memcpy(pkt->packet, src, len);
		imsbr_sblock_send(&imsbr_data, &blk, len);

		totlen -= len;
		offset += len;
	}

	IMSBR_STAT_INC(imsbr_stats->frag_ok);
}

#ifdef CONFIG_SPRD_IMS_BRIDGE_TEST

void call_packet_function(struct call_p_function *cpf)
{
	cpf->pkt2skb = imsbr_pkt2skb;
	cpf->dumpcap = imsbr_dumpcap;
	cpf->frag_send = imsbr_frag_send;
}

#endif

void imsbr_packet_relay2cp(struct sk_buff *skb)
{
	struct sblock blk;
	int len = skb->len;

	IMSBR_STAT_INC(imsbr_stats->pkts_tocp);
	if (__ratelimit(&rlimit_tocp_pkt))
		imsbr_packet_info("relay packet to cp", skb);

	if (unlikely(tcpdump_enable))
		imsbr_dumpcap(skb);

	if (skb_linearize(skb)) {
		IMSBR_STAT_INC(imsbr_stats->sk_buff_fail);
		pr_err("linearize fail, len=%d, drop it!\n", len);
		goto freeit;
	}

	if (likely(len <= imsbr_fragsz())) {
		struct imsbr_packet _pkthdr;
		struct imsbr_packet *pkt;

		if (imsbr_sblock_get(&imsbr_data, &blk, len))
			goto freeit;

		pkt = blk.addr;

		INIT_IMSBR_PACKET(&_pkthdr, len);
		unalign_memcpy(pkt, &_pkthdr, sizeof(_pkthdr));
		unalign_memcpy(pkt->packet, skb->data, len);

		imsbr_sblock_send(&imsbr_data, &blk, len);
	} else {
		imsbr_frag_send(skb);
	}

freeit:
	kfree_skb(skb);
}

static struct sk_buff *
imsbr_pkt_reasm(struct imsbr_packet *phdr, void *pktdata, int pktlen)
{
	const int huge_pktlen = 8192;
	int reasm_tlen = phdr->reasm_tlen;
	char *dst;

	pr_debug("reasm offset=%d len=%d reasm_tlen=%d\n",
		 phdr->frag_off, pktlen, phdr->reasm_tlen);

	mutex_lock(&imsbr_reasm_lock);
	if (phdr->frag_off == 0) {
		IMSBR_STAT_INC(imsbr_stats->reasm_request);
		if (unlikely(reasm_tlen >= huge_pktlen)) {
			pr_info("start to reasm a huge packet len=%d\n",
				reasm_tlen);
		}

		/* Drop residual sk_buff */
		if (imsbr_reasm_skb)
			kfree_skb(imsbr_reasm_skb);
		imsbr_reasm_skb = imsbr_allocskb(reasm_tlen, GFP_KERNEL);
		if (!imsbr_reasm_skb)
			goto fail;
	} else if (!imsbr_reasm_skb) {
		pr_err("First frag dropped, cannot reasm!\n");
		goto fail;
	}

	if (reasm_tlen > skb_availroom(imsbr_reasm_skb)) {
		pr_crit("reasm_tlen %d is wrong, first frag alloc room is %d\n",
			reasm_tlen, skb_availroom(imsbr_reasm_skb));
		goto fail;
	}

	dst = imsbr_reasm_skb->data + phdr->frag_off;
	unalign_memcpy(dst, pktdata, pktlen);

	if (phdr->frag_off + pktlen == reasm_tlen) {
		struct sk_buff *skb = imsbr_reasm_skb;

		imsbr_reasm_skb = NULL;
		IMSBR_STAT_INC(imsbr_stats->reasm_ok);

		skb_put(skb, reasm_tlen);
		mutex_unlock(&imsbr_reasm_lock);
		return skb;
	}

	/**
	 * XXX - If a packet was fraged to 1#,2#,3# and 3# was lost,
	 * we will *leak* imsbr_reasm_skb util the next frag packet
	 * arrival, but it is not worth to *fix* this "bug", this will
	 * just increase complexity. Also *leak* is very hard to
	 * occur!
	 */
	mutex_unlock(&imsbr_reasm_lock);
	return NULL;

fail:
	IMSBR_STAT_INC(imsbr_stats->reasm_fail);
	if (imsbr_reasm_skb) {
		kfree_skb(imsbr_reasm_skb);
		imsbr_reasm_skb = NULL;
	}

	mutex_unlock(&imsbr_reasm_lock);
	return NULL;
}

static bool imsbr_packet_invalid(struct imsbr_packet *phdr, int pktlen)
{
	if (phdr->version != IMSBR_PACKET_VER) {
		pr_err("pkt version %d is not same as %d\n", phdr->version,
		       IMSBR_PACKET_VER);
		return true;
	}

	if (unlikely(phdr->frag_off + pktlen > phdr->reasm_tlen)) {
		pr_err("offset %d+%d is larger than reasm_tlen %d\n",
		       phdr->frag_off, pktlen, phdr->reasm_tlen);
		return true;
	}

	return false;
}

void imsbr_process_packet(struct imsbr_sipc *sipc, struct sblock *blk,
			  bool freeit)
{
	struct nf_conntrack_tuple nft_tuple;
	struct imsbr_packet _pkthdr;
	struct imsbr_flow *flow;
	struct sk_buff *skb;
	int socket_type;
	char *pktdata;
	int pktlen;
	int media_type;

	IMSBR_STAT_INC(imsbr_stats->pkts_fromcp);

	unalign_memcpy(&_pkthdr, blk->addr, sizeof(_pkthdr));

	pktdata = (char *)blk->addr + sizeof(_pkthdr);
	pktlen = blk->length - sizeof(_pkthdr);

	if (imsbr_packet_invalid(&_pkthdr, pktlen)) {
		if (likely(freeit))
			imsbr_sblock_release(sipc, blk);
		return;
	}

	/* No frags */
	if (likely(_pkthdr.frag_off == 0 && _pkthdr.reasm_tlen == pktlen))
		skb = imsbr_pkt2skb(pktdata, pktlen);
	else
		skb = imsbr_pkt_reasm(&_pkthdr, pktdata, pktlen);

	if (likely(freeit))
		imsbr_sblock_release(sipc, blk);

	if (unlikely(!skb))
		return;

	if (tcpdump_enable)
		imsbr_dumpcap(skb);

	/* Default to local input */
	socket_type = IMSBR_SOCKET_AP;
	if (imsbr_parse_nfttuple(&init_net, skb, &nft_tuple))
		goto end;

	imsbr_nfct_debug("fromcp", skb, &nft_tuple);

	rcu_read_lock();
	flow = imsbr_flow_match(&nft_tuple);
	if (flow) {
		socket_type = flow->socket_type;
		media_type = flow->media_type;
	}
	rcu_read_unlock();

end:
	if (socket_type == IMSBR_SOCKET_CP) {
		if (__ratelimit(&rlimit_fromcp_pkt))
			imsbr_packet_info("process packet from cp out", skb);
		if (media_type == IMSBR_MEDIA_SIP) {
			skb->mark |= IMSBR_SKB_MARK_SIP;
		} else if (media_type == IMSBR_MEDIA_IKE) {
			skb->mark |= IMSBR_SKB_MARK_IKE;
		} else if (media_type == IMSBR_MEDIA_RTP_AUDIO ||
			   media_type == IMSBR_MEDIA_RTCP_AUDIO) {
			skb->mark |= IMSBR_SKB_MARK_VOICE;
		}

		imsbr_packet_output(skb);
	} else {
		skb->mark = vowifi_in_mark;

		if (__ratelimit(&rlimit_fromcp_pkt))
			imsbr_packet_info("process packet from cp in", skb);

		imsbr_packet_input(skb);
	}
}

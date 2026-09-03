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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/netfilter_ipv6.h>
#include <linux/skbuff.h>
#include <linux/icmpv6.h>
#include <linux/in.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/esp.h>
#include <net/udp.h>
#include <net/xfrm.h>
#include <uapi/linux/ims_bridge/ims_bridge.h>
#include "imsbr_core.h"
#include "imsbr_packet.h"
#include "imsbr_hooks.h"
#include "imsbr_netlink.h"

static int xfrm_frag_enable __read_mostly;
module_param(xfrm_frag_enable, int, 0644);
struct espheader esphs[MAX_ESPS];
static bool is_icmp_error(struct nf_conntrack_tuple *nft)
{
	u8 protonum = nft->dst.protonum;
	__u8 type = nft->dst.u.icmp.type;

	switch (protonum) {
	case IPPROTO_ICMP:
		switch (type) {
		case ICMP_DEST_UNREACH:
		case ICMP_SOURCE_QUENCH:
		case ICMP_TIME_EXCEEDED:
		case ICMP_PARAMETERPROB:
		case ICMP_REDIRECT:
			return true;
		}
		fallthrough;
	case IPPROTO_ICMPV6:
		if (type < 128)
			return true;
	}

	return false;
}

int imsbr_parse_nfttuple(struct net *net, struct sk_buff *skb,
			 struct nf_conntrack_tuple *nft)
{
	struct nf_conntrack_tuple innertuple, origtuple;
	struct iphdr *ip = ip_hdr(skb);
	unsigned int inner_nhoff;
	u16 l3num;

	if (ip->version == 4) {
		l3num = AF_INET;
		inner_nhoff = skb_network_offset(skb) + ip_hdrlen(skb) +
			      sizeof(struct icmphdr);
	} else if (ip->version == 6) {
		l3num = AF_INET6;
		inner_nhoff = skb_network_offset(skb) + sizeof(struct ipv6hdr) +
			      sizeof(struct icmp6hdr);
	} else {
		pr_err("parse tuple fail, ver=%d\n", ip->version);
		goto fail;
	}

	if (!nf_ct_get_tuplepr(skb, skb_network_offset(skb), l3num, net,
			       nft)) {
		pr_err("parse tuple fail, len=%d, nhoff=%d, l3num=%d\n",
		       skb->len, skb_network_offset(skb), l3num);
		goto fail;
	}

	if (is_icmp_error(nft)) {
		/* If parse the inner tuple fail, we will just use the outer
		 * tuple!
		 */
		if (!nf_ct_get_tuplepr(skb, inner_nhoff, l3num, net,
				       &origtuple))
			return 0;

		rcu_read_lock();
		/**
		 * Ordinarily, we'd expect the inverted tupleproto, but it's
		 * been preserved inside the ICMP.
		 */
		if (!nf_ct_invert_tuple(&innertuple, &origtuple)) {
			rcu_read_unlock();
			return 0;
		}

		rcu_read_unlock();
		*nft = innertuple;
	}

	return 0;

fail:
	IMSBR_STAT_INC(imsbr_stats->nfct_get_fail);
	return -EINVAL;
}

static int imsbr_get_tuple(struct net *net, struct sk_buff *skb,
			   struct nf_conntrack_tuple *nft)
{
	enum ip_conntrack_info ctinfo;
	const struct nf_conn *ct;
	int dir;

	ct = nf_ct_get(skb, &ctinfo);
	if (likely(ct)) {
		dir = CTINFO2DIR(ctinfo);
		*nft = ct->tuplehash[dir].tuple;
		return 0;
	}

	IMSBR_STAT_INC(imsbr_stats->nfct_slow_path);
	return imsbr_parse_nfttuple(net, skb, nft);
}

static void
imsbr_modify_esp_seq(unsigned int spi, unsigned int seq)
{
	int i;

	for (i = 0; i < MAX_ESPS; i++) {
		if (esphs[i].spi == spi && esphs[i].seq < seq) {
			esphs[i].seq = seq;
			break;
		}
	}
}

/*for cp vowifi */
#define IP_V_FLAG 0x40
#define IP_V6_FLAG 0x60
#define TYPE_UDP 0x11

static bool imsbr_packet_is_ike_auth(unsigned char *ptr, unsigned int len)
{
	unsigned int ub_begin, ub_end, i;
	unsigned char pkt_type;

	/*IKE authentication packeet has 00 00 00 00 next to UDP header*/
	if ((ptr[0] & IP_V_FLAG) == 0x40) {
		ub_begin = 28;
		ub_end = 32;
		pkt_type = ptr[9];
		if (ptr[20] == 0x01 && ptr[21] == 0xf4) {
			pr_info("this is ike packet DO SA INIT!");
			return true;
		}
	} else if ((ptr[0] & IP_V6_FLAG) == 0x60) {
		ub_begin = 48;
		ub_end = 52;
		pkt_type = ptr[6];
	} else {
		pr_info("this is not ike packet!");
		return false;
	}

	if (pkt_type == TYPE_UDP) {
		for (i = ub_begin; i < ub_end; i++) {
			if (ptr[i] != 0x00)
				return false;
		}
		pr_info("this is ike packet !!!\n");
		return true;
	}
	pr_info("This is not ike packet and return false\n");
	return false;
}

static int imsbr_send_new_mtu2cp(struct sk_buff *skb)
{
	struct iphdr *iph, *iph1;
	struct icmphdr *icmph;
	struct udphdr *uh;
	struct sblock blk;
	u16 new_mtu;

	iph = ip_hdr(skb);

	if (skb->mark == MARK_FRAG_NEED && iph->protocol == IPPROTO_ICMP) {
		iph1 = (struct iphdr *)(skb->data + (iph->ihl << 2) +
					sizeof(struct icmphdr));
		uh = (struct udphdr *)((unsigned char *)iph1 + (iph1->ihl << 2));
		if (uh && ntohs(uh->dest) == ESP_PORT) {
			icmph = (struct icmphdr *)(skb->data + (iph->ihl << 2));
			new_mtu = ntohs(icmph->un.frag.mtu);
			if (!imsbr_build_cmd("new-mtu", &blk, &new_mtu,
					     sizeof(u16)))
				imsbr_sblock_send(&imsbr_ctrl, &blk,
						  sizeof(u16));

			return 1;
		}
	}

	return 0;
}

static unsigned int nf_imsbr_input(void *priv,
				   struct sk_buff *skb,
				   const struct nf_hook_state *state)
{
	struct nf_conntrack_tuple nft;
	struct imsbr_flow *flow;
	struct iphdr *iph;
	struct udphdr *uh;
	struct ip_esp_hdr *esph;

	if (!atomic_read(&imsbr_enabled))
		return NF_ACCEPT;

	if (imsbr_get_tuple(state->net, skb, &nft))
		return NF_ACCEPT;

	imsbr_nfct_debug("input", skb, &nft);

	iph = ip_hdr(skb);

	if (imsbr_send_new_mtu2cp(skb)) {
		kfree_skb(skb);
		return NF_STOLEN;
	}

	if (iph && (iph->version == 4 && iph->protocol == IPPROTO_UDP)) {
		uh = udp_hdr(skb);
		if (uh && ntohs(uh->source) == ESP_PORT) {
			esph = (struct ip_esp_hdr *)((char *)uh + 8);
			if (imsbr_spi_match(ntohl(esph->spi))) {
				pr_info("spi 0x%x matched, update seq to %d\n",
					ntohl(esph->spi), ntohl(esph->seq_no));
				imsbr_modify_esp_seq(ntohl(esph->spi),
						     ntohl(esph->seq_no));
			}
		}
	}

	/* rcu_read_lock hold by netfilter hook outside */
	flow = imsbr_flow_match(&nft);
	if (!flow)
		return NF_ACCEPT;

	/*for cp vowifi */
	if (flow->socket_type == IMSBR_SOCKET_CP &&
	    flow->media_type == IMSBR_MEDIA_IKE) {
		if (imsbr_packet_is_ike_auth(skb->data, skb->len)) {
			imsbr_packet_relay2cp(skb);
			pr_info("this is ike pkt, go to cp socket !");
			return NF_STOLEN;
		}
		return NF_ACCEPT;//now is esp packet
	}

	/* c2k: downlink ike pkt always go to ap socket */
	if (flow->media_type == IMSBR_MEDIA_IKE) {
		pr_info("input skb=%p is ike pkt, go to ap socket\n", skb);
		return NF_ACCEPT;
	}
	/* Considering handover, link_type maybe LINK_AP or LINK_CP, meanwhile
	 * socket_type maybe SOCKET_AP or SOCKET_CP.
	 *
	 * But in INPUT hook, we can ignore the link_type and do the decision
	 * based on socket_type.
	 */
	if (flow->socket_type == IMSBR_SOCKET_CP) {
		pr_debug("input skb=%p relay to cp\n", skb);
		imsbr_packet_relay2cp(skb);
		return NF_STOLEN;
	}

	return NF_ACCEPT;
}

static unsigned int nf_imsbr_output(void *priv,
				    struct sk_buff *skb,
				    const struct nf_hook_state *state)
{
	struct nf_conntrack_tuple nft;
	struct imsbr_flow *flow;
	int sim, link_type;
	struct iphdr *iph;
	struct udphdr *uh;
	struct ip_esp_hdr *esph;
	unsigned int transport_offset;

	if (!atomic_read(&imsbr_enabled))
		return NF_ACCEPT;

	if (imsbr_get_tuple(state->net, skb, &nft))
		return NF_ACCEPT;

	imsbr_nfct_debug("output", skb, &nft);

	iph = ip_hdr(skb);
	if (iph && (iph->version == 4 && iph->protocol == IPPROTO_UDP)) {
		transport_offset = iph->ihl * 4;
		skb_set_transport_header(skb, transport_offset);
		uh = udp_hdr(skb);
		if (uh && ntohs(uh->dest) == ESP_PORT) {
			esph = (struct ip_esp_hdr *)((char *)uh +
				sizeof(struct udphdr));
			if (imsbr_spi_match(ntohl(esph->spi))) {
				pr_info("output spi %x matched, update seq to %d\n",
					ntohl(esph->spi), ntohl(esph->seq_no));
				imsbr_modify_esp_seq(ntohl(esph->spi),
						     ntohl(esph->seq_no));
			}
		}
	}

	/* rcu_read_lock hold by netfilter hook outside */
	flow = imsbr_flow_match(&nft);
	if (!flow)
		return NF_ACCEPT;

	/* c2k: downlink ike pkt always go to ap socket */
	if (flow->media_type == IMSBR_MEDIA_IKE) {
		pr_info("output skb=%p is ike pkt, go to wlan0\n", skb);
		return NF_ACCEPT;
	}

	/* This means the packet was relayed from CP, so there's no need to
	 * have a further check!
	 */
	if (flow->socket_type == IMSBR_SOCKET_CP)
		return NF_ACCEPT;

	link_type = flow->link_type;
	sim = flow->sim_card;

	/* We assume that WIFI is at AP and LTE is at CP, but this may be
	 * broken in the future...
	 */
	if ((link_type == IMSBR_LINK_CP && !imsbr_in_lte2wifi(sim)) ||
	    (link_type == IMSBR_LINK_AP && imsbr_in_wifi2lte(sim))) {
		pr_debug("output skb=%p relay to cp\n", skb);
		/* Complete checksum manually if hw checksum offload is on */
		if (skb->ip_summed == CHECKSUM_PARTIAL)
			skb_checksum_help(skb);
		imsbr_packet_relay2cp(skb);
		return NF_STOLEN;
	}

	if (cur_lp_state == IMSBR_LOWPOWER_START) {
		pr_info("lowpower output skb=%p relay to cp\n", skb);
		imsbr_packet_relay2cp(skb);
		return NF_STOLEN;
	}

	return NF_ACCEPT;
}

static void ipv4_copy_metadata(struct sk_buff *to, struct sk_buff *from)
{
	to->pkt_type = from->pkt_type;
	to->priority = from->priority;
	to->protocol = from->protocol;
	to->skb_iif = from->skb_iif;
	skb_dst_drop(to);
	skb_dst_copy(to, from);
	to->dev = from->dev;
	to->mark = from->mark;

	skb_copy_hash(to, from);

#ifdef CONFIG_NET_SCHED
	to->tc_index = from->tc_index;
#endif
	nf_copy(to, from);
	skb_ext_copy(to, from);
#if IS_ENABLED(CONFIG_IP_VS)
	to->ipvs_property = from->ipvs_property;
#endif
	skb_copy_secmark(to, from);
}

static void ipv6_copy_metadata(struct sk_buff *to, struct sk_buff *from)
{
	to->pkt_type = from->pkt_type;
	to->priority = from->priority;
	to->protocol = from->protocol;
	skb_dst_drop(to);
	skb_dst_set(to, dst_clone(skb_dst(from)));
	to->dev = from->dev;
	to->mark = from->mark;

	skb_copy_hash(to, from);

#ifdef CONFIG_NET_SCHED
	to->tc_index = from->tc_index;
#endif
	nf_copy(to, from);
	skb_ext_copy(to, from);
	skb_copy_secmark(to, from);
}

void ipv4_options_fragment(struct sk_buff *skb)
{
	unsigned char *optptr = skb_network_header(skb) + sizeof(struct iphdr);
	struct ip_options *opt = &(IPCB(skb)->opt);
	int  l = opt->optlen;
	int  optlen;

	while (l > 0) {
		switch (*optptr) {
		case IPOPT_END:
			return;
		case IPOPT_NOOP:
			l--;
			optptr++;
			continue;
		}
		optlen = optptr[1];
		if (optlen < 2 || optlen > l)
			return;
		if (!IPOPT_COPIED(*optptr))
			memset(optptr, IPOPT_NOOP, optlen);
		l -= optlen;
		optptr += optlen;
	}
	opt->ts = 0;
	opt->rr = 0;
	opt->rr_needaddr = 0;
	opt->ts_needaddr = 0;
	opt->ts_needtime = 0;
}

int ipv4_do_xfrm_fragment(struct net *net, struct sock *sk, struct sk_buff *skb,
			  int pmtu,
			  int (*output)(struct net *,
					struct sock *,
					struct sk_buff *))
{
	struct iphdr *iph;
	int ptr;
	struct net_device *dev;
	struct sk_buff *skb2;
	unsigned int mtu, hlen, left, len, ll_rs;
	int offset;
	__be16 not_last_frag;
	struct rtable *rt = skb_rtable(skb);
	int err = 0;

	dev = rt->dst.dev;
	/* for offloaded checksums cleanup checksum before fragmentation */
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		err = skb_checksum_help(skb);
		if (err)
			goto fail;
	}
	/*Point into the IP datagram header.*/
	iph = ip_hdr(skb);
	/*Setup starting values.*/

	hlen = iph->ihl * 4;
	mtu = pmtu - hlen;	/* Size of data space */
	IPCB(skb)->flags |= IPSKB_FRAG_COMPLETE;

	ll_rs = LL_RESERVED_SPACE(rt->dst.dev);
	/* When frag_list is given, use it. First, check its validity:
	 * some transformers could create wrong frag_list or break existing
	 * one, it is not prohibited. In this case fall back to copying.
	 *
	 * LATER: this step can be merged to real generation of fragments,
	 * we can switch to copy when see the first bad fragment.
	 */
	if (skb_has_frag_list(skb)) {
		struct sk_buff *frag, *frag2;
		int first_len = skb_pagelen(skb);

		if (first_len - hlen > mtu ||
		    ((first_len - hlen) & 7) ||
		    ip_is_fragment(iph) ||
		    skb_cloned(skb) ||
		    skb_headroom(skb) < ll_rs)
			goto slow_path;

		skb_walk_frags(skb, frag) {
			/* Correct geometry. */
			if (frag->len > mtu ||
			    ((frag->len & 7) && frag->next) ||
			    skb_headroom(frag) < hlen + ll_rs)
				goto slow_path_clean;

			/* Partially cloned skb? */
			if (skb_shared(frag))
				goto slow_path_clean;

			/*BUG_ON(frag->sk);*/
			WARN_ON(frag->sk);
			if (skb->sk) {
				frag->sk = skb->sk;
				frag->destructor = sock_wfree;
			}
			skb->truesize -= frag->truesize;
		}

		/* Everything is OK. Generate! */

		err = 0;
		offset = 0;
		frag = skb_shinfo(skb)->frag_list;
		skb_frag_list_init(skb);
		skb->data_len = first_len - skb_headlen(skb);
		skb->len = first_len;
		iph->tot_len = htons(first_len);
		iph->frag_off = htons(IP_MF);
		ip_send_check(iph);

		for (;;) {
			/* Prepare header of the next frame,
			 * before previous one went down.
			 */
			if (frag) {
				frag->ip_summed = CHECKSUM_NONE;
				skb_reset_transport_header(frag);
				__skb_push(frag, hlen);
				skb_reset_network_header(frag);
				memcpy(skb_network_header(frag), iph, hlen);
				iph = ip_hdr(frag);
				iph->tot_len = htons(frag->len);
				ipv4_copy_metadata(frag, skb);
				if (offset == 0)
					ipv4_options_fragment(frag);
				offset += skb->len - hlen;
				iph->frag_off = htons(offset >> 3);
				if (frag->next)
					iph->frag_off |= htons(IP_MF);
				/* Ready, complete checksum */
				ip_send_check(iph);
			}

			err = output(net, sk, skb);

			if (!err)
				IP_INC_STATS(net, IPSTATS_MIB_FRAGCREATES);
			if (err || !frag)
				break;

			skb = frag;
			frag = skb->next;
			skb->next = NULL;
		}

		if (err == 0) {
			IP_INC_STATS(net, IPSTATS_MIB_FRAGOKS);
			return 0;
		}

		while (frag) {
			skb = frag->next;
			kfree_skb(frag);
			frag = skb;
		}
		IP_INC_STATS(net, IPSTATS_MIB_FRAGFAILS);
		return err;

slow_path_clean:
		skb_walk_frags(skb, frag2) {
			if (frag2 == frag)
				break;
			frag2->sk = NULL;
			frag2->destructor = NULL;
			skb->truesize += frag2->truesize;
		}
	}

slow_path:
	iph = ip_hdr(skb);

	left = skb->len - hlen;		/* Space per frame */
	ptr = hlen;		/* Where to start from */

	/*Fragment the datagram.*/

	offset = (ntohs(iph->frag_off) & IP_OFFSET) << 3;
	not_last_frag = iph->frag_off & htons(IP_MF);

	/*Keep copying data until we run out.*/

	while (left > 0) {
		len = left;
		/* IF: it doesn't fit, use 'mtu' - the data space left */
		if (len > mtu)
			len = mtu;
		/* IF: we are not sending up to and including the packet end
		 * then align the next start on an eight byte boundary
		 */
		if (len < left)
			len &= ~7;

		/* Allocate buffer */
		skb2 = alloc_skb(len + hlen + ll_rs, GFP_ATOMIC);
		if (!skb2) {
			err = -ENOMEM;
			goto fail;
		}

		/*Set up data on packet*/
		ipv4_copy_metadata(skb2, skb);
		skb_reserve(skb2, ll_rs);
		skb_put(skb2, len + hlen);
		skb_reset_network_header(skb2);
		skb2->transport_header = skb2->network_header + hlen;

		/*Charge the memory for the fragment to any owner
		 *it might possess
		 */
		if (skb->sk)
			skb_set_owner_w(skb2, skb->sk);

		/*Copy the packet header into the new buffer.*/

		skb_copy_from_linear_data(skb, skb_network_header(skb2), hlen);

		/*Copy a block of the IP datagram.*/
		if (skb_copy_bits(skb, ptr, skb_transport_header(skb2), len)) {
			/*BUG();*/
			kfree_skb(skb2);
			goto fail;
		}
		left -= len;

		/*Fill in the new header fields.*/
		iph = ip_hdr(skb2);
		iph->frag_off = htons((offset >> 3));

		if (IPCB(skb)->flags & IPSKB_FRAG_PMTU)
			iph->frag_off |= htons(IP_DF);

		/* ANK: dirty, but effective trick. Upgrade options only if
		 * the segment to be fragmented was THE FIRST (otherwise,
		 * options are already fixed) and make it ONCE
		 * on the initial skb, so that all the following fragments
		 * will inherit fixed options.
		 */
		if (offset == 0)
			ipv4_options_fragment(skb);

		/*Added AC : If we are fragmenting a fragment that's not the
		 *last fragment then keep MF on each bit
		 */
		if (left > 0 || not_last_frag)
			iph->frag_off |= htons(IP_MF);
		ptr += len;
		offset += len;

		/*Put this fragment into the sending queue.*/
		iph->tot_len = htons(len + hlen);

		ip_send_check(iph);

		err = output(net, sk, skb2);
		if (err)
			goto fail;

		IP_INC_STATS(net, IPSTATS_MIB_FRAGCREATES);
	}
	consume_skb(skb);
	IP_INC_STATS(net, IPSTATS_MIB_FRAGOKS);
	return err;

fail:
	kfree_skb(skb);
	IP_INC_STATS(net, IPSTATS_MIB_FRAGFAILS);
	return err;
}

int ipv6_do_xfrm_fragent(struct net *net, struct sock *sk, struct sk_buff *skb,
			 int pmtu,
			 int (*output)(struct net *,
				       struct sock *,
				       struct sk_buff *))
{
	struct sk_buff *frag;
	struct rt6_info *rt = (struct rt6_info *)skb_dst(skb);
	struct ipv6_pinfo *np = skb->sk && !dev_recursion_level() ?
				inet6_sk(skb->sk) : NULL;
	struct ipv6hdr *tmp_hdr;
	struct frag_hdr *fh;
	unsigned int mtu, hlen, left, len;
	int hroom, troom;
	__be32 frag_id;
	int ptr, offset = 0, err = 0;
	u8 *prevhdr, nexthdr = 0;

	err = ip6_find_1stfragopt(skb, &prevhdr);
	if (err < 0)
		goto fail;

	hlen = err;
	nexthdr = *prevhdr;

	/*mtu = ip6_skb_dst_mtu(skb);*/
	mtu = pmtu;
	/* We must not fragment if the socket is set to force MTU discovery
	 * or if the skb it not generated by a local socket.
	 */
	if (unlikely(!skb->ignore_df && skb->len > mtu))
		goto fail_toobig;

	if (IP6CB(skb)->frag_max_size) {
		if (IP6CB(skb)->frag_max_size > mtu)
			goto fail_toobig;

		/* don't send fragments larger than what we received */
		mtu = IP6CB(skb)->frag_max_size;
		if (mtu < IPV6_MIN_MTU)
			mtu = IPV6_MIN_MTU;
	}

	if (np && np->frag_size < mtu) {
		if (np->frag_size)
			mtu = np->frag_size;
	}
	if (mtu < hlen + sizeof(struct frag_hdr) + 8)
		goto fail_toobig;
	mtu -= hlen + sizeof(struct frag_hdr);

	frag_id = ipv6_select_ident(net, &ipv6_hdr(skb)->daddr,
				    &ipv6_hdr(skb)->saddr);
	/* After check the ip_summed,and do skb_checksum_help,
	 * otherwise it will crash.
	 */
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		err = skb_checksum_help(skb);
		if (err)
			goto fail;
	}

	hroom = LL_RESERVED_SPACE(rt->dst.dev);
	if (skb_has_frag_list(skb)) {
		int first_len = skb_pagelen(skb);
		struct sk_buff *frag2;

		if (first_len - hlen > mtu ||
		    ((first_len - hlen) & 7) ||
		    skb_cloned(skb) ||
		    skb_headroom(skb) < (hroom + sizeof(struct frag_hdr)))
			goto slow_path;

		skb_walk_frags(skb, frag) {
			/* Correct geometry. */
			if (frag->len > mtu ||
			    ((frag->len & 7) && frag->next) ||
			    skb_headroom(frag) < (hlen + hroom +
						  sizeof(struct frag_hdr)))
				goto slow_path_clean;

			/* Partially cloned skb? */
			if (skb_shared(frag))
				goto slow_path_clean;

			WARN_ON(frag->sk);
			if (skb->sk) {
				frag->sk = skb->sk;
				frag->destructor = sock_wfree;
			}
			skb->truesize -= frag->truesize;
		}

		err = 0;
		offset = 0;
		/* BUILD HEADER */

		*prevhdr = NEXTHDR_FRAGMENT;
		tmp_hdr = kmemdup(skb_network_header(skb), hlen, GFP_ATOMIC);
		if (!tmp_hdr) {
			IP6_INC_STATS(net, ip6_dst_idev(skb_dst(skb)),
				      IPSTATS_MIB_FRAGFAILS);
			err = -ENOMEM;
			goto fail;
		}
		frag = skb_shinfo(skb)->frag_list;
		skb_frag_list_init(skb);

		__skb_pull(skb, hlen);
		fh = (struct frag_hdr *)
		      __skb_push(skb, sizeof(struct frag_hdr));

		__skb_push(skb, hlen);
		skb_reset_network_header(skb);
		memcpy(skb_network_header(skb), tmp_hdr, hlen);

		fh->nexthdr = nexthdr;
		fh->reserved = 0;
		fh->frag_off = htons(IP6_MF);
		fh->identification = frag_id;

		first_len = skb_pagelen(skb);
		skb->data_len = first_len - skb_headlen(skb);
		skb->len = first_len;
		ipv6_hdr(skb)->payload_len = htons(first_len -
						   sizeof(struct ipv6hdr));

		dst_hold(&rt->dst);

		for (;;) {
			/* Prepare header of the next frame,
			 * before previous one went down.
			 */
			if (frag) {
				frag->ip_summed = CHECKSUM_NONE;
				skb_reset_transport_header(frag);
				fh = (struct frag_hdr *)
				      __skb_push(frag, sizeof(struct frag_hdr));
				__skb_push(frag, hlen);
				skb_reset_network_header(frag);
				memcpy(skb_network_header(frag),
				       tmp_hdr,
				       hlen);
				offset += (skb->len - hlen -
					   sizeof(struct frag_hdr));
				fh->nexthdr = nexthdr;
				fh->reserved = 0;
				fh->frag_off = htons(offset);
				if (frag->next)
					fh->frag_off |= htons(IP6_MF);
				fh->identification = frag_id;
				ipv6_hdr(frag)->payload_len =
						htons(frag->len -
						      sizeof(struct ipv6hdr));
				ipv6_copy_metadata(frag, skb);
			}

			err = output(net, sk, skb);
			if (!err)
				IP6_INC_STATS(net, ip6_dst_idev(&rt->dst),
					      IPSTATS_MIB_FRAGCREATES);

			if (err || !frag)
				break;

			skb = frag;
			frag = skb->next;
			skb->next = NULL;
		}

		kfree(tmp_hdr);

		if (err == 0) {
			IP6_INC_STATS(net, ip6_dst_idev(&rt->dst),
				      IPSTATS_MIB_FRAGOKS);
			ip6_rt_put(rt);
			return 0;
		}

		kfree_skb_list(frag);

		IP6_INC_STATS(net, ip6_dst_idev(&rt->dst),
			      IPSTATS_MIB_FRAGFAILS);
		ip6_rt_put(rt);
		return err;

slow_path_clean:
		skb_walk_frags(skb, frag2) {
			if (frag2 == frag)
				break;
			frag2->sk = NULL;
			frag2->destructor = NULL;
			skb->truesize += frag2->truesize;
		}
	}

slow_path:
	left = skb->len - hlen;		/*Space per frame */
	ptr = hlen;			/*Where to start from */

	/*Fragment the datagram.*/

	*prevhdr = NEXTHDR_FRAGMENT;
	troom = rt->dst.dev->needed_tailroom;

	/*Keep copying data until we run out.*/
	while (left > 0) {
		len = left;
		/* IF: it doesn't fit, use 'mtu' - the data space left */
		if (len > mtu)
			len = mtu;
		/*IF: we are not sending up to and including the packet end
		 *then align the next start on an eight byte boundary
		 */
		if (len < left)
			len &= ~7;

		/* Allocate buffer */
		frag = alloc_skb(len + hlen + sizeof(struct frag_hdr) +
				 hroom + troom, GFP_ATOMIC);
		if (!frag) {
			IP6_INC_STATS(net, ip6_dst_idev(skb_dst(skb)),
				      IPSTATS_MIB_FRAGFAILS);
			err = -ENOMEM;
			goto fail;
		}

		/*Set up data on packet*/

		ipv6_copy_metadata(frag, skb);
		skb_reserve(frag, hroom);
		skb_put(frag, len + hlen + sizeof(struct frag_hdr));
		skb_reset_network_header(frag);
		fh = (struct frag_hdr *)(skb_network_header(frag) + hlen);
		frag->transport_header = (frag->network_header + hlen +
					  sizeof(struct frag_hdr));

		/*Charge the memory for the fragment to any owner
		 *it might possess
		 */
		if (skb->sk)
			skb_set_owner_w(frag, skb->sk);

		/*Copy the packet header into the new buffer.*/
		skb_copy_from_linear_data(skb, skb_network_header(frag), hlen);

		/*Build fragment header.*/
		fh->nexthdr = nexthdr;
		fh->reserved = 0;
		fh->identification = frag_id;

		/*Copy a block of the IP datagram.*/
		WARN_ON(skb_copy_bits(skb, ptr, skb_transport_header(frag),
				      len));
		left -= len;

		fh->frag_off = htons(offset);
		if (left > 0)
			fh->frag_off |= htons(IP6_MF);
		ipv6_hdr(frag)->payload_len =
				htons(frag->len - sizeof(struct ipv6hdr));

		ptr += len;
		offset += len;

		/*Put this fragment into the sending queue.*/
		err = output(net, sk, frag);
		if (err)
			goto fail;

		IP6_INC_STATS(net, ip6_dst_idev(skb_dst(skb)),
			      IPSTATS_MIB_FRAGCREATES);
	}
	IP6_INC_STATS(net, ip6_dst_idev(skb_dst(skb)),
		      IPSTATS_MIB_FRAGOKS);
	consume_skb(skb);
	return err;

fail_toobig:
	if (skb->sk && dst_allfrag(skb_dst(skb)))
		sk_nocaps_add(skb->sk, NETIF_F_GSO_MASK);

	skb->dev = skb_dst(skb)->dev;
	icmpv6_send(skb, ICMPV6_PKT_TOOBIG, 0, mtu);
	err = -EMSGSIZE;

fail:
	IP6_INC_STATS(net, ip6_dst_idev(skb_dst(skb)),
		      IPSTATS_MIB_FRAGFAILS);
	kfree_skb(skb);
	return err;
}

static int nf_imsbr_output_after_fragment(struct net *net,
					  struct sock *sock, struct sk_buff *skb)
{
	const struct xfrm_state_afinfo *afinfo;
	struct xfrm_state *x = skb_dst(skb)->xfrm;
	int ret = -EAFNOSUPPORT;

	if (!x)
		return dst_output(net, sock, skb);

	rcu_read_lock();
	imsbr_dumpcap(skb);
	afinfo = xfrm_state_afinfo_get_rcu(x->outer_mode.family);
	if (likely(afinfo))
		ret = afinfo->output(net, sock, skb);
	else
		kfree_skb(skb);

	rcu_read_unlock();
	return ret;
}

static unsigned int nf_imsbr_ipv4_frag_output(void *priv,
					      struct sk_buff *skb,
					      const struct nf_hook_state *state)
{
	struct net *net;
	struct nf_conntrack_tuple nft;
	struct imsbr_flow *flow;
	struct inet_sock *inet;
	struct iphdr *ipv4h = ip_hdr(skb);
	struct udphdr *uh = udp_hdr(skb);
	struct dst_entry *dst = skb_dst(skb);
	struct xfrm_state *x = dst->xfrm;
	int pmtu = 1400;
	unsigned int transport_offset;

	if (!xfrm_frag_enable)
		return NF_ACCEPT;

	if (imsbr_get_tuple(state->net, skb, &nft))
		return NF_ACCEPT;

	imsbr_nfct_debug("output", skb, &nft);

	/* rcu_read_lock hold by netfilter hook outside */
	flow = imsbr_flow_match(&nft);
	if (!flow)
		return NF_ACCEPT;

	if (!x)
		return NF_ACCEPT;

	inet = skb->sk ? inet_sk(skb->sk) : NULL;
	pmtu = (inet && inet->pmtudisc == IP_PMTUDISC_PROBE) ? dst->dev->mtu : dst_mtu(dst);
	pmtu = pmtu - x->props.header_len - x->props.trailer_len;
	if (unlikely(pmtu < 0)) {
		printk_ratelimited(KERN_ERR
	"The mtu is too small,pmtu=%d,to keep communication,set the pmtu quals to 1400\n",
				pmtu);
		pmtu = (1400
			- x->props.header_len
			- x->props.trailer_len);
	}

	if (ipv4h->protocol == IPPROTO_UDP) {
		transport_offset = ipv4h->ihl * 4;
		skb_set_transport_header(skb, transport_offset);
		uh = udp_hdr(skb);
	}

	if (skb->len > pmtu &&
	    ((ipv4h->protocol == IPPROTO_UDP && ntohs(uh->dest) == 5060) ||
	    ipv4h->protocol == IPPROTO_ESP)) {
		int segs = 0;
		int seg_pmtu = 0;

		imsbr_dumpcap(skb);
		segs = skb->len / pmtu;
		segs++;
		seg_pmtu = skb->len / segs;
		seg_pmtu = (seg_pmtu / 4 + 1) * 4;
		if (ipv4h->protocol == IPPROTO_ESP && skb->ignore_df == 0)
			skb->ignore_df = 1;

		seg_pmtu += ipv4h->ihl * 4;
		if (seg_pmtu > pmtu)
			seg_pmtu = pmtu;
		printk_ratelimited(KERN_ERR
	"IPv4:The pkt is average divided into %d parts with mtu %d.\n",
		segs, seg_pmtu);
		net = xs_net(x);
		ipv4_do_xfrm_fragment(net, skb->sk, skb, seg_pmtu,
				      nf_imsbr_output_after_fragment);
		return NF_STOLEN;
	}
	return NF_ACCEPT;
}

static unsigned int nf_imsbr_ipv6_frag_output(void *priv,
					      struct sk_buff *skb,
					      const struct nf_hook_state *state)
{
	struct net *net;
	struct nf_conntrack_tuple nft;
	struct imsbr_flow *flow;
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
	struct udphdr *uh = udp_hdr(skb);
	struct dst_entry *dst = skb_dst(skb);
	struct xfrm_state *x = dst->xfrm;
	struct ipv6_pinfo *np = skb->sk ? inet6_sk(skb->sk) : NULL;
	int pmtu = 1400;
	int transport_offset;

	if (!xfrm_frag_enable)
		return NF_ACCEPT;

	if (imsbr_get_tuple(state->net, skb, &nft))
		return NF_ACCEPT;

	imsbr_nfct_debug("output", skb, &nft);

	/* rcu_read_lock hold by netfilter hook outside */
	flow = imsbr_flow_match(&nft);
	if (!flow)
		return NF_ACCEPT;

	if (!x)
		return NF_ACCEPT;

	pmtu = (np && np->pmtudisc == IPV6_PMTUDISC_PROBE) ? skb_dst(skb)->dev->mtu :
		dst_mtu(skb_dst(skb));
	pmtu = pmtu - x->props.header_len - x->props.trailer_len;
	if (unlikely(pmtu < 0)) {
		printk_ratelimited(KERN_ERR
	"The mtu is too small,pmtu=%d,to keep communication,set the pmtu quals to 1400\n",
				pmtu);
		pmtu = (1400
			- x->props.header_len
			- x->props.trailer_len);
	}

	if (ipv6h->nexthdr == NEXTHDR_UDP) {
		transport_offset = skb->len - skb->data_len - ntohs(ipv6h->payload_len);
		skb_set_transport_header(skb, transport_offset);
		uh = udp_hdr(skb);
	}

	if (skb->len > pmtu &&
	    ((ipv6h->nexthdr == NEXTHDR_UDP && ntohs(uh->dest) == 5060) ||
	    (ipv6h->nexthdr == NEXTHDR_ESP && x->props.mode == XFRM_MODE_TUNNEL))) {
		int segs = 0;
		int seg_pmtu = 0;

		segs = skb->len / pmtu;
		segs++;
		seg_pmtu = skb->len / segs;
		seg_pmtu = (seg_pmtu / 4 + 1) * 4;
		if (seg_pmtu > pmtu)
			seg_pmtu = pmtu;
		else if (seg_pmtu < 1280)
			seg_pmtu = 1280;
		printk_ratelimited(KERN_ERR
	"IPv6: The pkt is divided into %d parts with mtu %d.\n",
		segs, seg_pmtu);
		if (skb->ignore_df == 0)
			skb->ignore_df = 1;

		net = xs_net(x);
		ipv6_do_xfrm_fragent(net, skb->sk, skb, seg_pmtu,
				     nf_imsbr_output_after_fragment);
		return NF_STOLEN;
	}
	return NF_ACCEPT;
}

static struct nf_hook_ops nf_imsbr_ops[] __read_mostly = {
	{
		.hook		= nf_imsbr_input,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP_PRI_FILTER - 1,
	},
	{
		.hook		= nf_imsbr_output,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP_PRI_FILTER - 1,
	},
	{
		.hook		= nf_imsbr_ipv4_frag_output,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP_PRI_MANGLE - 1,
	},
	{
		.hook		= nf_imsbr_ipv6_frag_output,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP6_PRI_MANGLE - 1,
	},
	{
		.hook		= nf_imsbr_input,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP6_PRI_FILTER - 1,
	},
	{
		.hook		= nf_imsbr_output,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP6_PRI_FILTER - 1,
	},
};

static int __net_init imsbr_nf_register(struct net *net)
{
	return nf_register_net_hooks(net, nf_imsbr_ops,
				    ARRAY_SIZE(nf_imsbr_ops));
}

static void __net_init imsbr_nf_unregister(struct net *net)
{
	nf_unregister_net_hooks(net, nf_imsbr_ops, ARRAY_SIZE(nf_imsbr_ops));
}

static struct pernet_operations imsbr_net_ops = {
	.init = imsbr_nf_register,
	.exit = imsbr_nf_unregister,
};

int __init imsbr_hooks_init(void)
{
	int err;

	pr_debug("Registering netfilter hooks\n");

	err = register_pernet_subsys(&imsbr_net_ops);
	if (err)
		pr_err("nf_register_hooks: err %d\n", err);

	return 0;
}

void imsbr_hooks_exit(void)
{
	pr_debug("Unregistering netfilter hooks\n");

	unregister_pernet_subsys(&imsbr_net_ops);
}

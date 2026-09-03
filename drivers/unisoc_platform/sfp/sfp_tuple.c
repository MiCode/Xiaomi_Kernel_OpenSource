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
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/sysctl.h>
#include <net/route.h>
#include <net/ip.h>

#include <linux/netfilter_ipv4.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>
#include <net/netfilter/nf_nat_helper.h>
#include <net/netfilter/ipv4/nf_defrag_ipv4.h>
#include <net/netfilter/nf_log.h>

#include <sfp.h>

static bool sfp_tcp_pkt_to_tuple(const void *data,
				 unsigned int dataoff,
				 struct nf_conntrack_tuple *tuple)
{
	const struct tcphdr *hp;

	hp = (struct tcphdr *)(data + dataoff);
	if (!hp)
		return false;

	tuple->src.u.tcp.port = hp->source;
	tuple->dst.u.tcp.port = hp->dest;

	return true;
}

static bool sfp_udp_pkt_to_tuple(const void *data,
				 unsigned int dataoff,
				 struct nf_conntrack_tuple *tuple)
{
	const struct udphdr *hp;

	/* Actually only need first 8 bytes. */
	hp = (struct udphdr *)(data + dataoff);
	if (!hp)
		return false;

	tuple->src.u.udp.port = hp->source;
	tuple->dst.u.udp.port = hp->dest;

	return true;
}

static bool sfp_icmp_pkt_to_tuple(const void *data,
				  unsigned int dataoff,
				  struct nf_conntrack_tuple *tuple)
{
	const struct icmphdr *hp;

	/* Actually only need first 8 bytes. */
	hp = (struct icmphdr *)(data + dataoff);
	if (!hp)
		return false;

	if (hp->type != ICMP_ECHO && hp->type != ICMP_ECHOREPLY)
		return false;

	tuple->dst.u.icmp.type = hp->type;
	tuple->src.u.icmp.id = hp->un.echo.id;
	tuple->dst.u.icmp.code = hp->code;

	return true;
}

static bool sfp_icmp6_pkt_to_tuple(const void *data,
				   unsigned int dataoff,
				   struct nf_conntrack_tuple *tuple)
{
	const struct icmp6hdr *hp;

	/* Actually only need first 8 bytes. */
	hp = (struct icmp6hdr *)(data + dataoff);
	if (!hp)
		return false;

	if (hp->icmp6_type != ICMPV6_ECHO_REQUEST &&
	    hp->icmp6_type != ICMPV6_ECHO_REPLY)
		return false;

	tuple->dst.u.icmp.type = hp->icmp6_type;
	tuple->src.u.icmp.id = hp->icmp6_identifier;
	tuple->dst.u.icmp.code = hp->icmp6_code;

	return true;
}

static int sfp_ipv6_skip_exthdr(void *data, u8 start, u8 *nexthdrp)
{
	u8 nexthdr = *nexthdrp;

	while (ipv6_ext_hdr(nexthdr)) {
		struct ipv6_opt_hdr *hp;

		if (nexthdr == NEXTHDR_NONE)
			return -1;
		hp = (struct ipv6_opt_hdr *)(data + start);

		nexthdr = hp->nexthdr;
		start += hp->hdrlen;
	}
	*nexthdrp = nexthdr;
	return start;
}

bool sfp_pkt_to_tuple(void *data,
		      u32 offset,
		      struct nf_conntrack_tuple *tuple,
		      u32 *l4offsetp)
{
	void *l3hdhr;
	struct iphdr *ip4hdr;
	struct ipv6hdr *ip6hdr;
	u32 l4offset;
	u8 l4proto;
	bool ret = false;

	l3hdhr = data + offset;
	if (((struct iphdr *)l3hdhr)->version == 0x4) {
		ip4hdr = (struct iphdr *)l3hdhr;
		tuple->src.u3.ip = ip4hdr->saddr;
		tuple->dst.u3.ip = ip4hdr->daddr;
		l4offset = offset + (ip4hdr->ihl << 2);
		l4proto = ip4hdr->protocol;
		tuple->src.l3num = NFPROTO_IPV4;
	} else {
		u32 extoff;
		u8 nexthdr;

		ip6hdr = (struct ipv6hdr *)l3hdhr;
		extoff = offset + sizeof(struct ipv6hdr);
		nexthdr = ip6hdr->nexthdr;
		memcpy(tuple->src.u3.ip6, ip6hdr->saddr.s6_addr,
		       sizeof(tuple->src.u3.ip6));
		memcpy(tuple->dst.u3.ip6, ip6hdr->daddr.s6_addr,
		       sizeof(tuple->dst.u3.ip6));
		l4offset = sfp_ipv6_skip_exthdr(data, extoff, &nexthdr);
		l4proto = nexthdr;
		tuple->src.l3num = NFPROTO_IPV6;
	}
	*l4offsetp = l4offset;
	tuple->dst.protonum = l4proto;
	tuple->dst.dir = IP_CT_DIR_ORIGINAL;

	switch (l4proto) {
	case IP_L4_PROTO_TCP:
		ret = sfp_tcp_pkt_to_tuple(data, l4offset, tuple);
		break;
	case IP_L4_PROTO_UDP:
		ret = sfp_udp_pkt_to_tuple(data, l4offset, tuple);
		break;
	case IP_L4_PROTO_ICMP:
		ret = sfp_icmp_pkt_to_tuple(data, l4offset, tuple);
		break;
	case IP_L4_PROTO_ICMP6:
		ret = sfp_icmp6_pkt_to_tuple(data, l4offset, tuple);
		break;
	default:
		return false;
	}

	return ret;
}

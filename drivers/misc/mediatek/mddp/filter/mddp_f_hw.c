/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/dsfield.h>
#include <net/xfrm.h>
#include "mddp_track.h"
#include "mddp_f_desc.h"
#include "mddp_f_proto.h"
#include "mddp_ctrl.h"
#include "mddp_f_tuple.c"

static inline void mddp_f_ip4_tcp(
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	struct tuple *t,
	struct interface *iface,
	void *l3_header,
	void *l4_header)
{
	struct nat_tuple *found_nat_tuple;
	struct tcpheader *tcp = (struct tcpheader *) l4_header;
	uint8_t mddp_md_version = mddp_get_md_version();

	t->nat.s.tcp.port = tcp->th_sport;
	t->nat.d.tcp.port = tcp->th_dport;

	if (mddp_md_version == 0) {
		/* Do not match filter */
		desc->flag |= DESC_FLAG_TRACK_NAT;
		return;
	}

	found_nat_tuple = mddp_f_get_nat_tuple_ip4_tcpudp(t);
	if (likely(found_nat_tuple))
		desc->flag |= DESC_FLAG_FASTPATH;
	else
		desc->flag |= DESC_FLAG_TRACK_NAT;
}

static inline void mddp_f_ip4_udp(
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	struct tuple *t,
	struct interface *iface,
	void *l3_header,
	void *l4_header)
{
	struct nat_tuple *found_nat_tuple;
	struct udpheader *udp = (struct udpheader *) l4_header;
	uint8_t mddp_md_version = mddp_get_md_version();

	t->nat.s.udp.port = udp->uh_sport;
	t->nat.d.udp.port = udp->uh_dport;

	if (mddp_md_version == 0) {
		/* Do not match filter */
		desc->flag |= DESC_FLAG_TRACK_NAT;
		return;
	}

	found_nat_tuple = mddp_f_get_nat_tuple_ip4_tcpudp(t);
	if (likely(found_nat_tuple))
		desc->flag |= DESC_FLAG_FASTPATH;
	else
		desc->flag |= DESC_FLAG_TRACK_NAT;
}

static inline void mddp_f_ip6_tcp_lan(
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	struct router_tuple *t,
	struct interface *iface,
	void *l3_header,
	void *l4_header)
{
	struct router_tuple *found_router_tuple;
	struct tcpheader *tcp = (struct tcpheader *) l4_header;
	uint8_t mddp_md_version = mddp_get_md_version();

	t->in.tcp.port = tcp->th_sport;
	t->out.tcp.port = tcp->th_dport;

	if (mddp_md_version == 0) {
		/* Do not match filter */
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
		return;
	}

	found_router_tuple = mddp_f_get_router_tuple_tcpudp(t);
	if (likely(found_router_tuple))
		desc->flag |= DESC_FLAG_FASTPATH;
	else
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
}

static inline void mddp_f_ip6_udp_lan(
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	struct router_tuple *t,
	struct interface *iface,
	void *l3_header,
	void *l4_header)
{
	struct router_tuple *found_router_tuple;
	struct udpheader *udp = (struct udpheader *) l4_header;
	uint8_t mddp_md_version = mddp_get_md_version();

	t->in.udp.port = udp->uh_sport;
	t->out.udp.port = udp->uh_dport;

	if (mddp_md_version == 0) {
		/* Do not match filter */
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
		return;
	}

	found_router_tuple = mddp_f_get_router_tuple_tcpudp(t);
	if (likely(found_router_tuple))
		desc->flag |= DESC_FLAG_FASTPATH;
	else
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
}

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
	struct nat_tuple *found_nat_tuple = NULL;
	struct tcpheader *tcp = (struct tcpheader *) l4_header;
	uint8_t mddp_md_version = mddp_get_md_version();
	bool ret;

	t->nat.s.tcp.port = tcp->th_sport;
	t->nat.d.tcp.port = tcp->th_dport;

	if (!mddp_md_version) {
		/* Do not match filter */
		desc->flag |= DESC_FLAG_TRACK_NAT;
		return;
	}

	ret = mddp_f_check_pkt_need_track_nat_tuple_ip4(t, &found_nat_tuple);
	pr_debug("%s: IPv4 TCP is_need_track[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x], dev[%x].\n",
		__func__, ret, found_nat_tuple, t->nat.src, t->nat.dst,
		t->nat.proto, t->nat.s.tcp.port, t->nat.d.tcp.port, t->dev_in);
	if (ret == true)
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
	struct nat_tuple *found_nat_tuple = NULL;
	struct udpheader *udp = (struct udpheader *) l4_header;
	uint8_t mddp_md_version = mddp_get_md_version();
	bool ret;

	t->nat.s.udp.port = udp->uh_sport;
	t->nat.d.udp.port = udp->uh_dport;

	if (!mddp_md_version) {
		/* Do not match filter */
		desc->flag |= DESC_FLAG_TRACK_NAT;
		return;
	}

	ret = mddp_f_check_pkt_need_track_nat_tuple_ip4(t, &found_nat_tuple);
	pr_debug("%s: IPv4 UDP is_need_track[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x], dev[%x].\n",
		__func__, ret, found_nat_tuple, t->nat.src, t->nat.dst,
		t->nat.proto, t->nat.s.udp.port, t->nat.d.udp.port, t->dev_in);
	if (ret == true)
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
	struct router_tuple *found_router_tuple = NULL;
	struct tcpheader *tcp = (struct tcpheader *) l4_header;
	uint8_t mddp_md_version = mddp_get_md_version();
	bool ret;

	t->in.tcp.port = tcp->th_sport;
	t->out.tcp.port = tcp->th_dport;

	if (!mddp_md_version) {
		/* Do not match filter */
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
		return;
	}

	ret = mddp_f_check_pkt_need_track_router_tuple(t, &found_router_tuple);
	pr_debug("%s: IPv6 TCP is_need_track[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x], dev[%x].\n",
		__func__, ret, found_router_tuple, &t->saddr, &t->daddr,
		t->proto, t->in.tcp.port, t->out.tcp.port, t->dev_src);
	if (ret == true)
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
	struct router_tuple *found_router_tuple = NULL;
	struct udpheader *udp = (struct udpheader *) l4_header;
	uint8_t mddp_md_version = mddp_get_md_version();
	bool ret;

	t->in.udp.port = udp->uh_sport;
	t->out.udp.port = udp->uh_dport;

	if (!mddp_md_version) {
		/* Do not match filter */
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
		return;
	}

	ret = mddp_f_check_pkt_need_track_router_tuple(t, &found_router_tuple);
	pr_debug("%s: IPv6 UDP tuple. ret[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x], dev[%x].\n",
		__func__, ret, found_router_tuple, &t->saddr, &t->daddr,
		t->proto, t->in.tcp.port, t->out.tcp.port, t->dev_src);
	if (ret == true)
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
}


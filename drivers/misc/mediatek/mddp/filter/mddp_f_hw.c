// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

static inline void mddp_f_ip4_tcp(
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	struct tuple *t,
	void *l3_header,
	void *l4_header)
{
	struct nat_tuple *found_nat_tuple = NULL;
	struct tcpheader *tcp = (struct tcpheader *) l4_header;
	bool ret;

	t->nat.s.tcp.port = tcp->th_sport;
	t->nat.d.tcp.port = tcp->th_dport;

	ret = mddp_f_check_pkt_need_track_nat_tuple_ip4(t, &found_nat_tuple);
	MDDP_F_LOG(MDDP_LL_DEBUG,
		"%s: IPv4 TCP is_need_track[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x], dev[%x].\n",
		__func__, ret, found_nat_tuple, t->nat.src, t->nat.dst,
		t->nat.proto, t->nat.s.tcp.port, t->nat.d.tcp.port, t->dev_in);
	if (ret == true)
		desc->flag |= DESC_FLAG_TRACK_NAT;
}

static inline void mddp_f_ip4_udp(
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	struct tuple *t,
	void *l3_header,
	void *l4_header)
{
	struct nat_tuple *found_nat_tuple = NULL;
	struct udpheader *udp = (struct udpheader *) l4_header;
	bool ret;

	t->nat.s.udp.port = udp->uh_sport;
	t->nat.d.udp.port = udp->uh_dport;

	ret = mddp_f_check_pkt_need_track_nat_tuple_ip4(t, &found_nat_tuple);
	MDDP_F_LOG(MDDP_LL_DEBUG,
		"%s: IPv4 UDP is_need_track[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x], dev[%x].\n",
		__func__, ret, found_nat_tuple, t->nat.src, t->nat.dst,
		t->nat.proto, t->nat.s.udp.port, t->nat.d.udp.port, t->dev_in);
	if (ret == true)
		desc->flag |= DESC_FLAG_TRACK_NAT;
}

static inline void mddp_f_ip6_tcp_lan(
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	struct router_tuple *t,
	void *l3_header,
	void *l4_header)
{
	struct router_tuple *found_router_tuple = NULL;
	struct tcpheader *tcp = (struct tcpheader *) l4_header;
	bool ret;

	t->in.tcp.port = tcp->th_sport;
	t->out.tcp.port = tcp->th_dport;

	ret = mddp_f_check_pkt_need_track_router_tuple(t, &found_router_tuple);
	MDDP_F_LOG(MDDP_LL_DEBUG,
		"%s: IPv6 TCP is_need_track[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x], dev[%x].\n",
		__func__, ret, found_router_tuple, &t->saddr, &t->daddr,
		t->proto, t->in.tcp.port, t->out.tcp.port, t->dev_src);
	if (ret == true)
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
}

static inline void mddp_f_ip6_udp_lan(
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	struct router_tuple *t,
	void *l3_header,
	void *l4_header)
{
	struct router_tuple *found_router_tuple = NULL;
	struct udpheader *udp = (struct udpheader *) l4_header;
	bool ret;

	t->in.udp.port = udp->uh_sport;
	t->out.udp.port = udp->uh_dport;

	ret = mddp_f_check_pkt_need_track_router_tuple(t, &found_router_tuple);
	MDDP_F_LOG(MDDP_LL_DEBUG,
		"%s: IPv6 UDP tuple. ret[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x], dev[%x].\n",
		__func__, ret, found_router_tuple, &t->saddr, &t->daddr,
		t->proto, t->in.tcp.port, t->out.tcp.port, t->dev_src);
	if (ret == true)
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
}


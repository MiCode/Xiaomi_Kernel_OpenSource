// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

struct router_tuple {
	struct list_head list;

	struct in6_addr saddr;
	struct in6_addr daddr;

	struct net_device *dev_src;
	struct net_device *dev_dst;

	struct timer_list timeout_used;

	u_int32_t last_cnt;
	u_int32_t curr_cnt;
	bool is_need_tag;

	union {
		u_int16_t all;
		struct {
			u_int16_t port;
		} tcp;
		struct {
			u_int16_t port;
		} udp;
	} in;
	union {
		u_int16_t all;
		struct {
			u_int16_t port;
		} tcp;
		struct {
			u_int16_t port;
		} udp;
	} out;
	u_int8_t proto;
};

static inline void ipv6_addr_copy(
	struct in6_addr *a1,
	const struct in6_addr *a2)
{
	memcpy(a1, a2, sizeof(struct in6_addr));
}

static int mddp_f_max_router = MD_DIRECT_TETHERING_RULE_NUM;
static struct kmem_cache *mddp_f_router_tuple_cache;

static spinlock_t mddp_f_router_tuple_lock;
#define MDDP_F_ROUTER_TUPLE_INIT_LOCK(LOCK) spin_lock_init((LOCK))
#define MDDP_F_ROUTER_TUPLE_LOCK(LOCK, FLAG) spin_lock_irqsave((LOCK), (FLAG))
#define MDDP_F_ROUTER_TUPLE_UNLOCK(LOCK, FLAG) spin_unlock_irqrestore((LOCK), (FLAG))

/* Have to be power of 2 */
#define ROUTER_TUPLE_HASH_SIZE	(MD_DIRECT_TETHERING_RULE_NUM)

static struct list_head *router_tuple_hash;
static unsigned int router_tuple_hash_rnd;

#define HASH_ROUTER_TUPLE_TCPUDP(t) \
	(jhash_3words(((t)->saddr.s6_addr32[0] ^ (t)->saddr.s6_addr32[3]), \
	((t)->daddr.s6_addr32[0] ^ (t)->daddr.s6_addr32[3]), \
	((t)->in.all | ((t)->out.all << 16)), \
	router_tuple_hash_rnd) & (ROUTER_TUPLE_HASH_SIZE - 1))

static int mddp_f_router_cnt;

static int32_t mddp_f_init_router_tuple(void)
{
	int i;

	MDDP_F_ROUTER_TUPLE_INIT_LOCK(&mddp_f_router_tuple_lock);

	/* get 4 bytes random number */
	get_random_bytes(&router_tuple_hash_rnd, 4);

	/* allocate memory for router hash table */
	router_tuple_hash =
		vmalloc(sizeof(struct list_head) * ROUTER_TUPLE_HASH_SIZE);
	if (!router_tuple_hash)
		return -ENOMEM;

	/* init hash table */
	for (i = 0; i < ROUTER_TUPLE_HASH_SIZE; i++)
		INIT_LIST_HEAD(&router_tuple_hash[i]);

	mddp_f_router_tuple_cache =
		kmem_cache_create("mddp_f_router_tuple",
					sizeof(struct router_tuple), 0,
					SLAB_HWCACHE_ALIGN, NULL);
	return 0;
}

static void mddp_f_del_router_tuple_w_unlock(struct router_tuple *t,
		unsigned long flag)
{
	MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Del router tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);

	mddp_f_router_cnt--;

	/* remove from the list */
	if (t->list.next != LIST_POISON1 && t->list.prev != LIST_POISON2) {
		list_del(&t->list);
	} else {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Del router tuple fail, tuple[%p], next[%p], prev[%p].\n",
				__func__, t, t->list.next, t->list.prev);
		WARN_ON(1);
	}
	MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);

	kmem_cache_free(mddp_f_router_tuple_cache, t);
}

static void mddp_f_timeout_router_tuple(unsigned long data)
{
	struct router_tuple *t = (struct router_tuple *)data;
	unsigned long flag;

	MDDP_F_ROUTER_TUPLE_LOCK(&mddp_f_router_tuple_lock, flag);
	if (t->curr_cnt == t->last_cnt)
		mddp_f_del_router_tuple_w_unlock(t, flag);
	else {
		t->is_need_tag = true;

		MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);

		mod_timer(&t->timeout_used, jiffies + HZ * USED_TIMEOUT);
	}
}

static bool mddp_f_add_router_tuple_tcpudp(struct router_tuple *t)
{
	unsigned long flag;
	unsigned int hash;
	struct router_tuple *found_router_tuple;

	MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Add new tcpudp router tuple[%p] with src_port[%d] & proto[%d].\n",
			__func__, t, t->in.all, t->proto);

	if (mddp_f_router_cnt >= mddp_f_max_router) {
		kmem_cache_free(mddp_f_router_tuple_cache, t);

		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: TCPUDP router is full, tuple[%p], next[%p], prev[%p].\n",
				__func__, t, t->list.next, t->list.prev);
		return false;
	}
	hash = HASH_ROUTER_TUPLE_TCPUDP(t);

	MDDP_F_ROUTER_TUPLE_LOCK(&mddp_f_router_tuple_lock, flag);
	INIT_LIST_HEAD(&t->list);
	/* prevent from duplicating */
	list_for_each_entry(found_router_tuple,
				&router_tuple_hash[hash], list) {
		if (found_router_tuple->dev_src != t->dev_src)
			continue;
		if (!ipv6_addr_equal(&found_router_tuple->saddr, &t->saddr))
			continue;
		if (!ipv6_addr_equal(&found_router_tuple->daddr, &t->daddr))
			continue;
		if (found_router_tuple->proto != t->proto)
			continue;
		if (found_router_tuple->in.all != t->in.all)
			continue;
		if (found_router_tuple->out.all != t->out.all)
			continue;
		MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);
		MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: TCPUDP router is duplicated, tuple[%p].\n",
				__func__, t);
		kmem_cache_free(mddp_f_router_tuple_cache, t);
		return false;   /* duplication */
	}

	t->last_cnt = 0;
	t->curr_cnt = 0;
	t->is_need_tag = false;

	/* add to the list */
	list_add_tail(&t->list, &router_tuple_hash[hash]);

	mddp_f_router_cnt++;
	MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);

	MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Add tcpudp router tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);

	/* init timer and start it */
	setup_timer(&t->timeout_used,
			mddp_f_timeout_router_tuple, (unsigned long)t);
	t->timeout_used.expires = jiffies + HZ * USED_TIMEOUT;

	add_timer(&t->timeout_used);

	return true;
}

static inline struct router_tuple *mddp_f_get_router_tuple_tcpudp(
	struct router_tuple *t)
{
	unsigned long flag;
	unsigned int hash;
	struct router_tuple *found_router_tuple;
	int not_match;

	hash = HASH_ROUTER_TUPLE_TCPUDP(t);

	MDDP_F_ROUTER_TUPLE_LOCK(&mddp_f_router_tuple_lock, flag);
	list_for_each_entry(found_router_tuple,
				&router_tuple_hash[hash], list) {
		not_match = 0;
		not_match +=
			(!found_router_tuple->dev_dst) ? 1 : 0;
		not_match +=
			(!found_router_tuple->dev_src) ? 1 : 0;
		not_match +=
			(!ipv6_addr_equal(&found_router_tuple->saddr,
							&t->saddr)) ? 1 : 0;
		not_match +=
			(!ipv6_addr_equal(&found_router_tuple->daddr,
							&t->daddr)) ? 1 : 0;
		not_match +=
			(found_router_tuple->proto != t->proto) ? 1 : 0;
		not_match +=
			(found_router_tuple->in.all != t->in.all) ? 1 : 0;
		not_match +=
			(found_router_tuple->out.all != t->out.all) ? 1 : 0;
		if (unlikely(not_match))
			continue;

		MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);
		return found_router_tuple;
	}
	/* not found */
	MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);
	return 0;
}

static inline struct router_tuple *mddp_f_get_router_tuple_tcpudp_wo_lock(
	struct router_tuple *t)
{
	unsigned int hash;
	struct router_tuple *found_router_tuple;
	int not_match;

	hash = HASH_ROUTER_TUPLE_TCPUDP(t);

	list_for_each_entry(found_router_tuple,
				&router_tuple_hash[hash], list) {
		not_match = 0;
		not_match +=
			(!found_router_tuple->dev_dst) ? 1 : 0;
		not_match +=
			(!found_router_tuple->dev_src) ? 1 : 0;
		not_match +=
			(!ipv6_addr_equal(&found_router_tuple->saddr,
							&t->saddr)) ? 1 : 0;
		not_match +=
			(!ipv6_addr_equal(&found_router_tuple->daddr,
							&t->daddr)) ? 1 : 0;
		not_match +=
			(found_router_tuple->proto != t->proto) ? 1 : 0;
		not_match +=
			(found_router_tuple->in.all != t->in.all) ? 1 : 0;
		not_match +=
			(found_router_tuple->out.all != t->out.all) ? 1 : 0;
		if (unlikely(not_match))
			continue;

		return found_router_tuple;
	}
	/* not found */
	return 0;
}

static inline bool mddp_f_check_pkt_need_track_router_tuple(
	struct router_tuple *t,
	struct router_tuple **matched_tuple)
{
	unsigned long flag;
	unsigned int hash;
	struct router_tuple *found_router_tuple;
	int not_match;

	hash = HASH_ROUTER_TUPLE_TCPUDP(t);

	MDDP_F_ROUTER_TUPLE_LOCK(&mddp_f_router_tuple_lock, flag);
	list_for_each_entry(found_router_tuple,
				&router_tuple_hash[hash], list) {
		not_match = 0;
		not_match +=
			(!found_router_tuple->dev_dst) ? 1 : 0;
		not_match +=
			(!found_router_tuple->dev_src) ? 1 : 0;
		not_match +=
			(!ipv6_addr_equal(&found_router_tuple->saddr,
							&t->saddr)) ? 1 : 0;
		not_match +=
			(!ipv6_addr_equal(&found_router_tuple->daddr,
							&t->daddr)) ? 1 : 0;
		not_match +=
			(found_router_tuple->proto != t->proto) ? 1 : 0;
		not_match +=
			(found_router_tuple->in.all != t->in.all) ? 1 : 0;
		not_match +=
			(found_router_tuple->out.all != t->out.all) ? 1 : 0;
		if (unlikely(not_match))
			continue;

		*matched_tuple = found_router_tuple;
		found_router_tuple->curr_cnt++;

		MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: check tcpudp router tuple[%p], last_cnt[%d], curr_cnt[%d], need_tag[%d].\n",
			__func__, found_router_tuple,
			found_router_tuple->last_cnt,
			found_router_tuple->curr_cnt,
			found_router_tuple->is_need_tag);

		if (found_router_tuple->is_need_tag == true) {
			found_router_tuple->is_need_tag = false;
			MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);

			return true;
		}
		MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);

		return false;
	}

	/* not found */
	MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);
	return true;
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




#define IPC_HDR_IS_V6(_ip_hdr) \
	(0x60 == (*((unsigned char *)(_ip_hdr)) & 0xf0))



//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------
static inline void _mddp_f_in_tail_v6(
	struct mddp_f_desc *desc,
	struct sk_buff *skb)
{
	struct mddp_f_cb *cb;
	struct ip6header *ip6;
	struct tcpheader *tcp;
	struct udpheader *udp;

	cb = add_track_table(skb, desc);
	if (!cb) {
		MDDP_F_LOG(MDDP_LL_WARN,
				"%s: Add track table failed, skb[%p], desc[%p]!\n",
				__func__, skb, desc);
		return;
	}

	cb->flag = desc->flag;

	if (desc->flag & DESC_FLAG_TRACK_ROUTER) {
		/* Now only support IPv6 (20130104) */
		if (desc->flag & DESC_FLAG_IPV6) {
			ip6 = (struct ip6header *) (skb->data + desc->l3_off);

			cb->dev = skb->dev;
		} else {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Invalid router flag[%x], skb[%p]!\n",
					__func__, desc->flag, skb);

			return;
		}

		ipv6_addr_copy((struct in6_addr *)(&cb->src), &ip6->saddr);
		ipv6_addr_copy((struct in6_addr *)(&cb->dst), &ip6->daddr);
		switch (ip6->nexthdr) {
		case IPPROTO_TCP:
			tcp = (struct tcpheader *) (skb->data + desc->l4_off);

			cb->sport = tcp->th_sport;
			cb->dport = tcp->th_dport;
			break;
		case IPPROTO_UDP:
			udp = (struct udpheader *) (skb->data + desc->l4_off);

			cb->sport = udp->uh_sport;
			cb->dport = udp->uh_dport;
			break;
		default:
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"IPv6 BUG, %s: Should not reach here, skb[%p], protocol[%d].\n",
					__func__, skb, ip6->nexthdr);
			break;
		}
		cb->proto = ip6->nexthdr;

		return;
	}
	MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: Invalid flag[%x], skb[%p]!\n",
				__func__, desc->flag, skb);
}

static inline void _mddp_f_in_router(
	unsigned char *offset2,
	struct mddp_f_desc *desc,
	struct sk_buff *skb)
{
	/* IPv6 */
	struct router_tuple t6;
	struct ip6header *ip6;
	__be16 ip6_frag_off;

	void *l4_header;


	if (IPC_HDR_IS_V6(offset2)) {	/* only support ROUTE */
		desc->flag |= DESC_FLAG_IPV6;

		ip6 = (struct ip6header *) offset2;

		MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: IPv6 pre-routing, skb[%p], protocol[%d].\n",
				__func__, skb, ip6->nexthdr);

		t6.proto = ip6->nexthdr;
		desc->l3_len =
			ipv6_skip_exthdr(skb,
				desc->l3_off + sizeof(struct ip6header),
				&t6.proto, &ip6_frag_off) - desc->l3_off;
		desc->l4_off = desc->l3_off + desc->l3_len;
		l4_header = skb->data + desc->l4_off;

		/* ip fragmentation? */
		if (unlikely(ip6_frag_off > 0)) {
			desc->flag |= DESC_FLAG_IPFRAG;
			return;
		}

		t6.dev_src = skb->dev;
		ipv6_addr_copy((struct in6_addr *)(&(t6.saddr)), &(ip6->saddr));
		ipv6_addr_copy((struct in6_addr *)(&(t6.daddr)), &(ip6->daddr));

		switch (t6.proto) {
		case IPPROTO_TCP:
			mddp_f_ip6_tcp_lan(desc, skb, &t6, ip6, l4_header);
			return;
		case IPPROTO_UDP:
			mddp_f_ip6_udp_lan(desc, skb, &t6, ip6, l4_header);
			return;
		default:
			{
				desc->flag |= DESC_FLAG_UNKNOWN_PROTOCOL;
				return;
			}
		}
	} else {
		memset(desc, 0, sizeof(*desc));	/* avoid compiler warning */
		desc->flag |= DESC_FLAG_UNKNOWN_ETYPE;
		return;
	}
}

static int mddp_f_in_nf_v6(struct sk_buff *skb)
{
	struct mddp_f_desc desc;

	/* HW */
	desc.flag = 0;
	desc.l3_off = 0;


	skb_set_network_header(skb, desc.l3_off);

	_mddp_f_in_router(skb->data, &desc, skb);
	if (desc.flag & DESC_FLAG_NOMEM) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: No memory, flag[%x], skb[%p].\n",
				__func__, desc.flag, skb);

		return 2;
	}
	if (desc.flag & (DESC_FLAG_UNKNOWN_ETYPE |
			DESC_FLAG_UNKNOWN_PROTOCOL | DESC_FLAG_IPFRAG)) {
		/* un-handled packet, so pass it to kernel stack */
		MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Un-handled packet, pass it to kernel stack, flag[%x], skb[%p].\n",
					__func__, desc.flag, skb);
		return 0;
	}

	_mddp_f_in_tail_v6(&desc, skb);

	return 0;		/* original path */
}

static void mddp_f_out_nf_ipv6(
	struct sk_buff *skb,
	struct net_device *out,
	struct mddp_f_cb *cb,
	int l3_off,
	struct mddp_f_track_table_t *curr_track_table)
{
	struct ip6header *ip6 = (struct ip6header *) (skb->data + l3_off);
	unsigned char nexthdr;
	__be16 frag_off;
	int l4_off = 0;
	struct nf_conn *nat_ip_conntrack;
	enum ip_conntrack_info ctinfo;
	struct router_tuple t;
	struct router_tuple *found_router_tuple;
	struct tcpheader *tcp;
	struct udpheader *udp;
	unsigned char tcp_state;
	unsigned char ext_offset;
	unsigned long flag;
	bool is_uplink;
	int not_match = 0;
	unsigned int tuple_hit_cnt = 0;
	int ret;

	memset(&t, 0, sizeof(struct router_tuple));
	nexthdr = ip6->nexthdr;
	l4_off = ipv6_skip_exthdr(skb, l3_off + sizeof(struct ip6header),
							&nexthdr, &frag_off);
	switch (nexthdr) {
	case IPPROTO_TCP:
		tcp = (struct tcpheader *) (skb->data + l4_off);
		nat_ip_conntrack = nf_ct_get(skb, &ctinfo);
		tcp_state = nat_ip_conntrack->proto.tcp.state;
		ext_offset = nat_ip_conntrack->ext->offset[NF_CT_EXT_HELPER];

		if (!nat_ip_conntrack) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Null ip conntrack, skb[%p] is filtered out.\n",
					__func__, skb);
			goto out;
		}

		if (nat_ip_conntrack->ext && ext_offset) { /* helper */
			MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: skb[%p] is filtered out, ext[%p], ext_offset[%d].\n",
					__func__, skb,
					nat_ip_conntrack->ext, ext_offset);
			goto out;
		}

		if (mddp_f_contentfilter
				&& (tcp->th_dport == htons(80)
				|| tcp->th_sport == htons(80))
				&& nat_ip_conntrack->mark != 0x80000000) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Invalid parameter, contentfilter[%d], dport[%x], sport[%x], mark[%x], skb[%p] is filtered out,.\n",
					__func__,
					mddp_f_contentfilter, tcp->th_dport,
					tcp->th_sport, nat_ip_conntrack->mark,
					skb);
			goto out;
		}
		if (tcp_state >= TCP_CONNTRACK_FIN_WAIT
				&& tcp_state <=	TCP_CONNTRACK_CLOSE) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Invalid TCP state[%d], skb[%p] is filtered out.\n",
					__func__, tcp_state, skb);
			goto out;
		} else if (tcp_state !=	TCP_CONNTRACK_ESTABLISHED) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: TCP state[%d] is not in ESTABLISHED state, skb[%p] is filtered out.\n",
					__func__, tcp_state, skb);
			goto out;
		}
		if (cb->dev == out) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"BUG %s,%d: in_dev[%p] name[%s] and out_dev[%p] name [%s] are same.\n",
					__func__, __LINE__, cb->dev,
					cb->dev->name, out, out->name);
			goto out;
		}

		if (mddp_f_is_support_wan_dev(out->name) == true) {
			is_uplink = true;

			not_match +=
				(!ipv6_addr_equal(&ip6->saddr,
				(struct in6_addr *)(&cb->src))) ? 1 : 0;
			not_match +=
				(!ipv6_addr_equal(&ip6->daddr,
				(struct in6_addr *)(&cb->dst))) ? 1 : 0;
			not_match += (ip6->nexthdr != cb->proto) ? 1 : 0;
			not_match += (tcp->th_sport != cb->sport) ? 1 : 0;
			not_match += (tcp->th_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				MDDP_F_LOG(MDDP_LL_INFO,
					"%s: IPv6 TCP UL tag not_match[%d], ip_p[%d], sport[%x], dport[%x], cb_proto[%d], cb_sport[%x], cb_dport[%x].\n",
					__func__, not_match, ip6->nexthdr,
					tcp->th_sport, tcp->th_dport, cb->proto,
					cb->sport, cb->dport);

				goto out;
			}

			ipv6_addr_copy(&(t.saddr),
					(struct in6_addr *)&(cb->src));
			ipv6_addr_copy(&(t.daddr),
					(struct in6_addr *)&(cb->dst));
			t.proto = cb->proto;
			t.in.tcp.port = cb->sport;
			t.out.tcp.port = cb->dport;
			t.dev_src = cb->dev;
		} else {
			is_uplink = false;

			/* Do not tag TCP DL packet */
			MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Do not tag IPv6 TCP DL.\n",
					__func__);

			goto out;
		}


		/* Tag this packet for MD tracking */
		if (skb_headroom(skb) > sizeof(struct mddp_f_tag_packet_t)) {
			MDDP_F_ROUTER_TUPLE_LOCK(&mddp_f_router_tuple_lock, flag);
			found_router_tuple =
				mddp_f_get_router_tuple_tcpudp_wo_lock(&t);

			if (found_router_tuple) {
				tuple_hit_cnt = found_router_tuple->curr_cnt;
				found_router_tuple->last_cnt = 0;
				found_router_tuple->curr_cnt = 0;
				MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);

				MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: tuple[%p] is found!!\n",
					__func__, found_router_tuple);
				ret = mddp_f_tag_packet(is_uplink,
						skb, out, cb, nexthdr,
						ip6->version, tuple_hit_cnt);
				if (ret == 0)
					MDDP_F_LOG(MDDP_LL_NOTICE,
						"%s: Add IPv6 TCP MDDP tag, is_uplink[%d], skb[%p], tcp_checksum[%x].\n",
						__func__, is_uplink,
						skb, tcp->th_sum);

				goto out;
			} else {
				MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);

				ret = mddp_f_tag_packet(is_uplink,
						skb, out, cb, nexthdr,
						ip6->version, tuple_hit_cnt);
				if (ret == 0)
					MDDP_F_LOG(MDDP_LL_NOTICE,
						"%s: Add IPv6 TCP MDDP tag, is_uplink[%d], skb[%p], tcp_checksum[%x].\n",
						__func__, is_uplink,
						skb, tcp->th_sum);
			}

			/* Save tuple to avoid tag many packets */
			found_router_tuple = kmem_cache_alloc(
					mddp_f_router_tuple_cache, GFP_ATOMIC);
			found_router_tuple->dev_src = cb->dev;
			found_router_tuple->dev_dst = out;
			ipv6_addr_copy(&found_router_tuple->saddr, &ip6->saddr);
			ipv6_addr_copy(&found_router_tuple->daddr, &ip6->daddr);
			found_router_tuple->in.tcp.port = tcp->th_sport;
			found_router_tuple->out.tcp.port = tcp->th_dport;
			found_router_tuple->proto = nexthdr;

			mddp_f_add_router_tuple_tcpudp(found_router_tuple);
		} else {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Headroom of skb[%p] is not enough to add MDDP tag, headroom[%d].\n",
					__func__, skb, skb_headroom(skb));
		}
		break;
	case IPPROTO_UDP:
		udp = (struct udpheader *)(skb->data + l4_off);
		nat_ip_conntrack = nf_ct_get(skb, &ctinfo);
		ext_offset = nat_ip_conntrack->ext->offset[NF_CT_EXT_HELPER];

		if (!nat_ip_conntrack) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Null ip conntrack, skb[%p] is filtered out.\n",
					__func__, skb);
			goto out;
		}

		if (nat_ip_conntrack->ext && ext_offset) { /* helper */
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: skb[%p] is filtered out, ext[%p], ext_offset[%d].\n",
					__func__, skb,
				nat_ip_conntrack->ext, ext_offset);
			goto out;
		}

		if (cb->dev == out)	{
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"BUG %s,%d: in_dev[%p] name[%s] and out_dev[%p] name [%s] are same.\n",
					__func__, __LINE__, cb->dev,
					cb->dev->name, out, out->name);
			goto out;
		}

		if (mddp_f_is_support_wan_dev(out->name) == true) {
			is_uplink = true;

			not_match +=
				(!ipv6_addr_equal(&ip6->saddr,
					(struct in6_addr *)(&cb->src))) ? 1 : 0;
			not_match +=
				(!ipv6_addr_equal(&ip6->daddr,
					(struct in6_addr *)(&cb->dst))) ? 1 : 0;
			not_match += (ip6->nexthdr != cb->proto) ? 1 : 0;
			not_match += (udp->uh_sport != cb->sport) ? 1 : 0;
			not_match += (udp->uh_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				MDDP_F_LOG(MDDP_LL_INFO,
					"%s: IPv6 UDP UL tag not_match[%d], ip_p[%d], sport[%x], dport[%x], cb_proto[%d], cb_sport[%x], cb_dport[%x].\n",
					__func__, not_match, ip6->nexthdr,
					udp->uh_sport, udp->uh_dport, cb->proto,
					cb->sport, cb->dport);

				goto out;
			}

			ipv6_addr_copy(&(t.saddr),
					(struct in6_addr *)&(cb->src));
			ipv6_addr_copy(&(t.daddr),
					(struct in6_addr *)&(cb->dst));
			t.proto = cb->proto;
			t.in.tcp.port = cb->sport;
			t.out.tcp.port = cb->dport;
			t.dev_src = cb->dev;
		} else {
			is_uplink = false;

			not_match +=
				(!ipv6_addr_equal(&ip6->saddr,
					(struct in6_addr *)(&cb->src))) ? 1 : 0;
			not_match +=
				(!ipv6_addr_equal(&ip6->daddr,
					(struct in6_addr *)(&cb->dst))) ? 1 : 0;
			not_match += (ip6->nexthdr != cb->proto) ? 1 : 0;
			not_match += (udp->uh_sport != cb->sport) ? 1 : 0;
			not_match += (udp->uh_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				MDDP_F_LOG(MDDP_LL_INFO,
					"%s: IPv6 UDP DL tag not_match[%d], ip_p[%d], sport[%x], dport[%x], cb_proto[%d], cb_sport[%x], cb_dport[%x].\n",
					__func__, not_match, ip6->nexthdr,
					udp->uh_sport, udp->uh_dport, cb->proto,
					cb->sport, cb->dport);

				goto out;
			}
		}

		if (udp->uh_sport != 67 && udp->uh_sport != 68) {
			/* Don't fastpath dhcp packet */

			/* Tag this packet for MD tracking */
			if (skb_headroom(skb)
					> sizeof(struct mddp_f_tag_packet_t)) {
				MDDP_F_ROUTER_TUPLE_LOCK(&mddp_f_router_tuple_lock, flag);
				found_router_tuple =
					mddp_f_get_router_tuple_tcpudp_wo_lock(
							&t);

				if (found_router_tuple) {
					tuple_hit_cnt =
						found_router_tuple->curr_cnt;
					found_router_tuple->last_cnt = 0;
					found_router_tuple->curr_cnt = 0;
					MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock,
							flag);

					if (is_uplink == false) {
						MDDP_F_LOG(MDDP_LL_DEBUG,
							"%s: No need to tag UDP DL.\n",
							__func__);
						goto out;
					}

					MDDP_F_LOG(MDDP_LL_DEBUG,
						"%s: tuple[%p] is found!!\n",
						__func__, found_router_tuple);
					ret = mddp_f_tag_packet(is_uplink, skb,
								out, cb,
								nexthdr,
								ip6->version,
								tuple_hit_cnt);
					if (ret == 0)
						MDDP_F_LOG(MDDP_LL_NOTICE,
							"%s: Add IPv6 UDP MDDP tag, is_uplink[%d], skb[%p], udp_checksum[%x].\n",
							__func__, is_uplink,
							skb, udp->uh_check);

					goto out;
				} else {
					MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock,
							flag);

					ret = mddp_f_tag_packet(is_uplink, skb,
								out, cb,
								nexthdr,
								ip6->version,
								tuple_hit_cnt);
					if (ret == 0)
						MDDP_F_LOG(MDDP_LL_NOTICE,
							"%s: Add IPv6 UDP MDDP tag, is_uplink[%d], skb[%p], udp_checksum[%x].\n",
							__func__, is_uplink,
							skb, udp->uh_check);
				}

				/* Save tuple to avoid tag many packets */
				found_router_tuple = kmem_cache_alloc(
						mddp_f_router_tuple_cache,
						GFP_ATOMIC);
				found_router_tuple->dev_src = cb->dev;
				found_router_tuple->dev_dst = out;
				ipv6_addr_copy(&found_router_tuple->saddr,
						&ip6->saddr);
				ipv6_addr_copy(&found_router_tuple->daddr,
						&ip6->daddr);
				found_router_tuple->in.udp.port = udp->uh_sport;
				found_router_tuple->out.udp.port =
						udp->uh_dport;
				found_router_tuple->proto = nexthdr;

				mddp_f_add_router_tuple_tcpudp(
						found_router_tuple);
			} else {
				MDDP_F_LOG(MDDP_LL_NOTICE,
						"%s: Headroom of skb[%p] is not enough to add MDDP tag, headroom[%d].\n",
						__func__,
						skb, skb_headroom(skb));
			}
		} else {
			MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Don't track DHCP packet, s_port[%d], skb[%p] is filtered out.\n",
					__func__, cb->sport, skb);
		}
		break;
	default:
		MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Not TCP/UDP packet, protocal[%d], skb[%p] is filtered out.\n",
					__func__, nexthdr, skb);
		break;
	}

out:
	put_track_table(curr_track_table);
}

static void mddp_f_out_nf_v6(struct sk_buff *skb, struct net_device *out)
{
	struct mddp_f_cb *cb;
	struct mddp_f_track_table_t *curr_track_table;

	cb = search_and_hold_track_table(skb, &curr_track_table);
	if (!cb) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Cannot find cb, skb[%p].\n",
					__func__, skb);
		return;
	}

	if (cb->dev == out) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: in_dev[%p] name[%s] and out_dev[%p] name[%s] are same, don't track skb[%p].\n",
				__func__, cb->dev,
				cb->dev->name, out, out->name, skb);
		goto out;
	}

	if (cb->dev == NULL || out == NULL) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Each of in_dev[%p] or out_dev[%p] is NULL, don't track skb[%p].\n",
					__func__, cb->dev, out, skb);
		goto out;
	}

	if (cb->flag & DESC_FLAG_TRACK_ROUTER) {
		int l3_off = 0;

		if (cb->flag & DESC_FLAG_IPV6) {
			mddp_f_out_nf_ipv6(skb, out, cb, l3_off,
							curr_track_table);
			return;

		} else {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Invalid IPv6 flag[%x], skb[%p].\n",
					__func__, cb->flag, skb);
		}

	} else {
		MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: No need to track, skb[%p], cb->flag[%x].\n",
					__func__, skb, cb->flag);
	}

out:
	put_track_table(curr_track_table);
}

static uint32_t mddp_nfhook_prerouting_v6
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	if (mddp_f_suspend_s == 1)
		return NF_ACCEPT;

	if (!mddp_is_acted_state(MDDP_APP_TYPE_ALL))
		return NF_ACCEPT;

	if (unlikely(!state->in || !skb->dev || !skb_mac_header_was_set(skb))) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Invalid param, in(%p), dev(%p), mac(%d)!\n",
			__func__, state->in, skb->dev,
			skb_mac_header_was_set(skb));
		return NF_ACCEPT;
	}

	if ((state->in->priv_flags & IFF_EBRIDGE) ||
			(state->in->flags & IFF_LOOPBACK)) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Invalid flag, priv_flags(%x), flags(%x)!\n",
			__func__, state->in->priv_flags, state->in->flags);
		return NF_ACCEPT;
	}

	if (!mddp_f_is_support_dev(state->in->name)) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Unsupport device, state->in->name(%s)!\n",
			__func__, state->in->name);
		return NF_ACCEPT;
	}

	mddp_f_in_nf_v6(skb);

	return NF_ACCEPT;
}

static uint32_t mddp_nfhook_postrouting_v6
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	if (!mddp_is_acted_state(MDDP_APP_TYPE_ALL))
		return NF_ACCEPT;

	if (unlikely(!state->out || !skb->dev ||
				(skb_headroom(skb) < ETH_HLEN))) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Invalid parameter, out(%p), dev(%p), headroom(%d)\n",
			__func__, state->out, skb->dev, skb_headroom(skb));
		goto out;
	}

	if ((state->out->priv_flags & IFF_EBRIDGE) ||
			(state->out->flags & IFF_LOOPBACK)) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Invalid flag, priv_flags(%x), flags(%x).\n",
			__func__, state->out->priv_flags, state->out->flags);
		return NF_ACCEPT;
	}

	if (!mddp_f_is_support_dev(state->out->name)) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Unsuport device,state->out->name(%s).\n",
			__func__, state->out->name);
		return NF_ACCEPT;
	}

	mddp_f_out_nf_v6(skb, state->out);

out:
	return NF_ACCEPT;
}


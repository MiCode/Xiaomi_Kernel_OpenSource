// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

struct router_tuple {
	struct list_head list;

	struct in6_addr saddr;
	struct in6_addr daddr;

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

static int mddp_f_max_router = 10 * MD_DIRECT_TETHERING_RULE_NUM;
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

static atomic_t mddp_f_router_cnt = ATOMIC_INIT(0);
static struct wait_queue_head router_wq;

static int32_t mddp_f_init_router_tuple(void)
{
	int i;

	MDDP_F_ROUTER_TUPLE_INIT_LOCK(&mddp_f_router_tuple_lock);
	init_waitqueue_head(&router_wq);

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

static void mddp_f_uninit_router_tuple(void)
{
	wait_event(router_wq, !atomic_read(&mddp_f_router_cnt));
	kmem_cache_destroy(mddp_f_router_tuple_cache);
	vfree(router_tuple_hash);
}

static void mddp_f_del_router_tuple_w_unlock(struct router_tuple *t,
		unsigned long flag)
{
	MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Del router tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);

	atomic_dec(&mddp_f_router_cnt);

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

static void mddp_f_timeout_router_tuple(struct timer_list *timer)
{
	struct router_tuple *t = from_timer(t, timer, timeout_used);
	unsigned long flag;

	if (unlikely(atomic_read(&mddp_filter_quit))) {
		kmem_cache_free(mddp_f_router_tuple_cache, t);
		if (atomic_dec_and_test(&mddp_f_router_cnt))
			wake_up(&router_wq);
		return;
	}

	MDDP_F_ROUTER_TUPLE_LOCK(&mddp_f_router_tuple_lock, flag);
	if (t->curr_cnt == t->last_cnt)
		mddp_f_del_router_tuple_w_unlock(t, flag);
	else {
		t->is_need_tag = true;
		t->last_cnt = t->curr_cnt;

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

	if (atomic_read(&mddp_f_router_cnt) >= mddp_f_max_router) {
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

	atomic_inc(&mddp_f_router_cnt);
	MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);

	MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Add tcpudp router tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);

	/* init timer and start it */
	timer_setup(&t->timeout_used, mddp_f_timeout_router_tuple, 0);
	t->timeout_used.expires = jiffies + HZ * USED_TIMEOUT;

	add_timer(&t->timeout_used);

	return true;
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
		"%s: IPv6 TCP is_need_track[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x].\n",
		__func__, ret, found_router_tuple, &t->saddr, &t->daddr,
		t->proto, t->in.tcp.port, t->out.tcp.port);
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
		"%s: IPv6 UDP tuple. ret[%d], found_tuple[%p], src_ip[%x], dst_ip[%x], ip_p[%d], sport[%x], dport[%x],.\n",
		__func__, ret, found_router_tuple, &t->saddr, &t->daddr,
		t->proto, t->in.tcp.port, t->out.tcp.port);
	if (ret == true)
		desc->flag |= DESC_FLAG_TRACK_ROUTER;
}




#define IPC_HDR_IS_V6(_ip_hdr) \
	(0x60 == (*((unsigned char *)(_ip_hdr)) & 0xf0))



//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------

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
	if (desc.flag & (DESC_FLAG_UNKNOWN_ETYPE |
			DESC_FLAG_UNKNOWN_PROTOCOL | DESC_FLAG_IPFRAG)) {
		/* un-handled packet, so pass it to kernel stack */
		MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Un-handled packet, pass it to kernel stack, flag[%x], skb[%p].\n",
					__func__, desc.flag, skb);
		return 1;
	}

	return !(desc.flag & DESC_FLAG_TRACK_ROUTER);
}

static void mddp_f_out_nf_ipv6(struct sk_buff *skb, struct mddp_f_cb *cb)
{
	struct ip6header *ip6 = (struct ip6header *) skb_network_header(skb);
	unsigned char nexthdr;
	struct router_tuple t;
	struct router_tuple *found_router_tuple;
	struct tcpheader *tcp;
	struct udpheader *udp;
	unsigned long flag;
	unsigned int tuple_hit_cnt = 0;
	int ret;

	memset(&t, 0, sizeof(struct router_tuple));
	nexthdr = ip6->nexthdr;
	cb->proto = nexthdr;
	cb->ip_ver = ip6->version;
	switch (nexthdr) {
	case IPPROTO_TCP:
		tcp = (struct tcpheader *) (skb_network_header(skb) + sizeof(struct ip6header));

		ipv6_addr_copy(&(t.saddr), (struct in6_addr *)&(ip6->saddr));
		ipv6_addr_copy(&(t.daddr), (struct in6_addr *)&(ip6->daddr));
		t.proto = ip6->nexthdr;
		t.in.tcp.port = tcp->th_sport;
		t.out.tcp.port = tcp->th_dport;

		/* Tag this packet for MD tracking */
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
			ret = mddp_f_tag_packet(skb, cb, tuple_hit_cnt);
			if (ret == 0)
				MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Add IPv6 TCP MDDP tag, is_uplink[%d], skb[%p], tcp_checksum[%x].\n",
					__func__, cb->is_uplink,
					skb, tcp->th_sum);

		} else {
			MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock, flag);

			/* Save tuple to avoid tag many packets */
			found_router_tuple = kmem_cache_alloc(
						mddp_f_router_tuple_cache, GFP_ATOMIC);
			if (found_router_tuple == NULL) {
				MDDP_F_LOG(MDDP_LL_NOTICE,
						"%s: kmem_cache_alloc() failed\n",
						__func__);
				goto out;
			}

			ipv6_addr_copy(&found_router_tuple->saddr, &ip6->saddr);
			ipv6_addr_copy(&found_router_tuple->daddr, &ip6->daddr);
			found_router_tuple->in.tcp.port = tcp->th_sport;
			found_router_tuple->out.tcp.port = tcp->th_dport;
			found_router_tuple->proto = nexthdr;

			mddp_f_add_router_tuple_tcpudp(found_router_tuple);

			ret = mddp_f_tag_packet(skb, cb, tuple_hit_cnt);
			if (ret == 0)
				MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Add IPv6 TCP MDDP tag, is_uplink[%d], skb[%p], tcp_checksum[%x].\n",
					__func__, cb->is_uplink,
					skb, tcp->th_sum);
		}
		break;
	case IPPROTO_UDP:
		udp = (struct udpheader *) (skb_network_header(skb) + sizeof(struct ip6header));

		ipv6_addr_copy(&(t.saddr), (struct in6_addr *)&(ip6->saddr));
		ipv6_addr_copy(&(t.daddr), (struct in6_addr *)&(ip6->daddr));
		t.proto = ip6->nexthdr;
		t.in.udp.port = udp->uh_sport;
		t.out.udp.port = udp->uh_sport;

		if (udp->uh_sport != 67 && udp->uh_sport != 68) {
			/* Don't fastpath dhcp packet */

			/* Tag this packet for MD tracking */
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

				MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: tuple[%p] is found!!\n",
					__func__, found_router_tuple);
				ret = mddp_f_tag_packet(skb, cb,
							tuple_hit_cnt);
				if (ret == 0)
					MDDP_F_LOG(MDDP_LL_NOTICE,
						"%s: Add IPv6 UDP MDDP tag, is_uplink[%d], skb[%p], udp_checksum[%x].\n",
						__func__, cb->is_uplink,
						skb, udp->uh_check);

			} else {
				MDDP_F_ROUTER_TUPLE_UNLOCK(&mddp_f_router_tuple_lock,
						flag);

				/* Save tuple to avoid tag many packets */
				found_router_tuple = kmem_cache_alloc(
						mddp_f_router_tuple_cache, GFP_ATOMIC);
				if (found_router_tuple == NULL) {
					MDDP_F_LOG(MDDP_LL_NOTICE,
							"%s: kmem_cache_alloc() failed\n",
							__func__);
					goto out;
				}

				ipv6_addr_copy(&found_router_tuple->saddr, &ip6->saddr);
				ipv6_addr_copy(&found_router_tuple->daddr, &ip6->daddr);
				found_router_tuple->in.udp.port = udp->uh_sport;
				found_router_tuple->out.udp.port = udp->uh_dport;
				found_router_tuple->proto = nexthdr;

				mddp_f_add_router_tuple_tcpudp(found_router_tuple);

				ret = mddp_f_tag_packet(skb, cb,
							tuple_hit_cnt);
				if (ret == 0)
					MDDP_F_LOG(MDDP_LL_NOTICE,
						"%s: Add IPv6 UDP MDDP tag, is_uplink[%d], skb[%p], udp_checksum[%x].\n",
						__func__, cb->is_uplink,
						skb, udp->uh_check);
			}
		} else {
			MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Don't track DHCP packet, s_port[%d], skb[%p] is filtered out.\n",
					__func__, udp->uh_sport, skb);
		}
		break;
	default:
		MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Not TCP/UDP packet, protocal[%d], skb[%p] is filtered out.\n",
					__func__, nexthdr, skb);
		break;
	}

out:
	return;
}

static void mddp_nfhook_postrouting_v6(struct sk_buff *skb)
{
	struct mddp_f_cb cb;

	if (skb->skb_iif == 0) {
		MDDP_F_LOG(MDDP_LL_DEBUG, "%s: skb_iif is zero, packet is not from lan\n",
			   __func__);
		return;
	}

	memset(&cb, 0, sizeof(cb));
	cb.is_uplink = true;
	cb.wan = skb->dev;
	cb.lan = mddp_f_is_support_lan_dev(skb->skb_iif);
	if (!cb.lan)
		return;

	if (mddp_f_in_nf_v6(skb))
		return;

	mddp_f_out_nf_ipv6(skb, &cb);
}

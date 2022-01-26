// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

struct tuple {
	struct {
		u_int32_t src;	/*src */
		u_int32_t dst;	/*dst */
		union {
			u_int16_t all;
			struct {
				u_int16_t port;
			} tcp;
			struct {
				u_int16_t port;
			} udp;
			struct {
				u_int16_t callid;
			} gre;
		} s;
		union {
			u_int16_t all;
			struct {
				u_int16_t port;
			} tcp;
			struct {
				u_int16_t port;
			} udp;
			struct {
				u_int16_t callid;
			} gre;
		} d;
		u_int8_t proto;
	} nat;
	struct net_device *dev_in;
};

struct nat_tuple {
	struct list_head list;

	u_int32_t src_ip;
	u_int32_t dst_ip;
	u_int32_t nat_src_ip;
	u_int32_t nat_dst_ip;
	union {
		u_int16_t all;
		struct {
			u_int16_t port;
		} tcp;
		struct {
			u_int16_t port;
		} udp;
	} src;
	union {
		u_int16_t all;
		struct {
			u_int16_t port;
		} tcp;
		struct {
			u_int16_t port;
		} udp;
	} dst;
	union {
		u_int16_t all;
		struct {
			u_int16_t port;
		} tcp;
		struct {
			u_int16_t port;
		} udp;
	} nat_src;
	union {
		u_int16_t all;
		struct {
			u_int16_t port;
		} tcp;
		struct {
			u_int16_t port;
		} udp;
	} nat_dst;

	u_int8_t proto;

	struct net_device *dev_src;
	struct net_device *dev_dst;

	struct timer_list timeout_used;

	u_int32_t last_cnt;
	u_int32_t curr_cnt;
	bool is_need_tag;
};

static int mddp_f_max_nat = 10 * MD_DIRECT_TETHERING_RULE_NUM;
static struct kmem_cache *mddp_f_nat_tuple_cache;

static spinlock_t mddp_f_nat_tuple_lock;
#define MDDP_F_NAT_TUPLE_INIT_LOCK(LOCK) spin_lock_init((LOCK))
#define MDDP_F_NAT_TUPLE_LOCK(LOCK, FLAG) spin_lock_irqsave((LOCK), (FLAG))
#define MDDP_F_NAT_TUPLE_UNLOCK(LOCK, FLAG) spin_unlock_irqrestore((LOCK), (FLAG))

/* Have to be power of 2 */
#define NAT_TUPLE_HASH_SIZE		(MD_DIRECT_TETHERING_RULE_NUM)

static struct list_head *nat_tuple_hash;
static unsigned int nat_tuple_hash_rnd;

#define HASH_TUPLE_TCPUDP(t) \
	(jhash_3words((t)->nat.src, ((t)->nat.dst ^ (t)->nat.proto), \
	((t)->nat.s.all | ((t)->nat.d.all << 16)), \
	nat_tuple_hash_rnd) & (NAT_TUPLE_HASH_SIZE - 1))

#define HASH_NAT_TUPLE_TCPUDP(t) \
	(jhash_3words((t)->src_ip, ((t)->dst_ip ^ (t)->proto), \
	((t)->src.all | ((t)->dst.all << 16)), \
	nat_tuple_hash_rnd) & (NAT_TUPLE_HASH_SIZE - 1))

static int mddp_f_nat_cnt;

static int32_t mddp_f_init_nat_tuple(void)
{
	int i;

	MDDP_F_NAT_TUPLE_INIT_LOCK(&mddp_f_nat_tuple_lock);

	/* get 4 bytes random number */
	get_random_bytes(&nat_tuple_hash_rnd, 4);

	/* allocate memory for two nat hash tables */
	nat_tuple_hash =
		vmalloc(sizeof(struct list_head) * NAT_TUPLE_HASH_SIZE);
	if (!nat_tuple_hash)
		return -ENOMEM;


	/* init hash table */
	for (i = 0; i < NAT_TUPLE_HASH_SIZE; i++)
		INIT_LIST_HEAD(&nat_tuple_hash[i]);

	mddp_f_nat_tuple_cache =
		kmem_cache_create("mddp_f_nat_tuple",
					sizeof(struct nat_tuple), 0,
					SLAB_HWCACHE_ALIGN, NULL);

	return 0;
}

static void mddp_f_del_nat_tuple_w_unlock(struct nat_tuple *t, unsigned long flag)
{
	MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Del nat tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);

	mddp_f_nat_cnt--;

	/* remove from the list */
	if (t->list.next != LIST_POISON1 && t->list.prev != LIST_POISON2) {
		list_del(&t->list);
	} else {
		MDDP_F_LOG(MDDP_LL_WARN,
				"%s: Del nat tuple fail, tuple[%p], next[%p], prev[%p].\n",
				__func__, t, t->list.next, t->list.prev);
		WARN_ON(1);
	}
	MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);

	kmem_cache_free(mddp_f_nat_tuple_cache, t);
}

static void mddp_f_timeout_nat_tuple(unsigned long data)
{
	struct nat_tuple *t = (struct nat_tuple *)data;
	unsigned long flag;

	MDDP_F_NAT_TUPLE_LOCK(&mddp_f_nat_tuple_lock, flag);
	if (t->curr_cnt == t->last_cnt)
		mddp_f_del_nat_tuple_w_unlock(t, flag);
	else {
		t->is_need_tag = true;
		t->last_cnt = t->curr_cnt;

		MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);

		mod_timer(&t->timeout_used, jiffies + HZ * USED_TIMEOUT);
	}
}

static bool mddp_f_add_nat_tuple(struct nat_tuple *t)
{
	unsigned long flag;
	unsigned int hash;
	struct nat_tuple *found_nat_tuple;

	MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Add new nat tuple[%p] with src_port[%d] & proto[%d].\n",
			__func__, t, t->src.all, t->proto);

	if (mddp_f_nat_cnt >= mddp_f_max_nat) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Nat tuple table is full! Tuple[%p] is about to free.\n",
				__func__, t);
		kmem_cache_free(mddp_f_nat_tuple_cache, t);
		return false;
	}

	switch (t->proto) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
		hash = HASH_NAT_TUPLE_TCPUDP(t);
		break;
	default:
		kmem_cache_free(mddp_f_nat_tuple_cache, t);
		return false;
	}

	MDDP_F_NAT_TUPLE_LOCK(&mddp_f_nat_tuple_lock, flag);
	INIT_LIST_HEAD(&t->list);
	/* prevent from duplicating */
	list_for_each_entry(found_nat_tuple, &nat_tuple_hash[hash], list) {
		if (found_nat_tuple->src_ip != t->src_ip)
			continue;
		if (found_nat_tuple->dst_ip != t->dst_ip)
			continue;
		if (found_nat_tuple->proto != t->proto)
			continue;
		if (found_nat_tuple->src.all != t->src.all)
			continue;
		if (found_nat_tuple->dst.all != t->dst.all)
			continue;
		MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);
		MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: Nat tuple[%p] is duplicated!\n",
				__func__, t);
		kmem_cache_free(mddp_f_nat_tuple_cache, t);
		return false;   /* duplication */
	}

	t->last_cnt = 0;
	t->curr_cnt = 0;
	t->is_need_tag = false;

	/* add to the list */
	list_add_tail(&t->list, &nat_tuple_hash[hash]);
	mddp_f_nat_cnt++;
	MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);

	MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: Add nat tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);

	/* init timer and start it */
	setup_timer(&t->timeout_used,
			mddp_f_timeout_nat_tuple, (unsigned long)t);
	t->timeout_used.expires = jiffies + HZ * USED_TIMEOUT;

	add_timer(&t->timeout_used);

	return true;
}

static inline struct nat_tuple *mddp_f_get_nat_tuple_ip4_tcpudp(
	struct tuple *t)
{
	unsigned long flag;
	unsigned int hash;
	int not_match;
	struct nat_tuple *found_nat_tuple;

	hash = HASH_TUPLE_TCPUDP(t);

	MDDP_F_NAT_TUPLE_LOCK(&mddp_f_nat_tuple_lock, flag);
	list_for_each_entry(found_nat_tuple, &nat_tuple_hash[hash], list) {
		not_match = 0;
		not_match +=
			(found_nat_tuple->dev_src != t->dev_in) ? 1 : 0;
		not_match +=
			(!found_nat_tuple->dev_dst) ? 1 : 0;
		not_match +=
			(found_nat_tuple->src_ip != t->nat.src) ? 1 : 0;
		not_match +=
			(found_nat_tuple->dst_ip != t->nat.dst) ? 1 : 0;
		not_match +=
			(found_nat_tuple->proto != t->nat.proto) ? 1 : 0;
		not_match +=
			(found_nat_tuple->src.all != t->nat.s.all) ? 1 : 0;
		not_match +=
			(found_nat_tuple->dst.all != t->nat.d.all) ? 1 : 0;
		if (unlikely(not_match))
			continue;

		MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);
		return found_nat_tuple;
	}
	MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);
	/* not found */
	return 0;
}

static inline struct nat_tuple *mddp_f_get_nat_tuple_ip4_tcpudp_wo_lock(
	struct tuple *t)
{
	unsigned int hash;
	int not_match;
	struct nat_tuple *found_nat_tuple;

	hash = HASH_TUPLE_TCPUDP(t);

	list_for_each_entry(found_nat_tuple, &nat_tuple_hash[hash], list) {
		not_match = 0;
		not_match +=
			(found_nat_tuple->dev_src != t->dev_in) ? 1 : 0;
		not_match +=
			(!found_nat_tuple->dev_dst) ? 1 : 0;
		not_match +=
			(found_nat_tuple->src_ip != t->nat.src) ? 1 : 0;
		not_match +=
			(found_nat_tuple->dst_ip != t->nat.dst) ? 1 : 0;
		not_match +=
			(found_nat_tuple->proto != t->nat.proto) ? 1 : 0;
		not_match +=
			(found_nat_tuple->src.all != t->nat.s.all) ? 1 : 0;
		not_match +=
			(found_nat_tuple->dst.all != t->nat.d.all) ? 1 : 0;
		if (unlikely(not_match))
			continue;

		return found_nat_tuple;
	}
	/* not found */
	return 0;
}

static inline bool mddp_f_check_pkt_need_track_nat_tuple_ip4(
	struct tuple *t,
	struct nat_tuple **matched_tuple)
{
	unsigned long flag;
	unsigned int hash;
	int not_match;
	struct nat_tuple *found_nat_tuple;

	hash = HASH_TUPLE_TCPUDP(t);

	MDDP_F_NAT_TUPLE_LOCK(&mddp_f_nat_tuple_lock, flag);
	list_for_each_entry(found_nat_tuple, &nat_tuple_hash[hash], list) {
		not_match = 0;
		not_match +=
			(found_nat_tuple->dev_src != t->dev_in) ? 1 : 0;
		not_match +=
			(!found_nat_tuple->dev_dst) ? 1 : 0;
		not_match +=
			(found_nat_tuple->src_ip != t->nat.src) ? 1 : 0;
		not_match +=
			(found_nat_tuple->dst_ip != t->nat.dst) ? 1 : 0;
		not_match +=
			(found_nat_tuple->proto != t->nat.proto) ? 1 : 0;
		not_match +=
			(found_nat_tuple->src.all != t->nat.s.all) ? 1 : 0;
		not_match +=
			(found_nat_tuple->dst.all != t->nat.d.all) ? 1 : 0;
		if (unlikely(not_match))
			continue;

		*matched_tuple = found_nat_tuple;
		found_nat_tuple->curr_cnt++;

		MDDP_F_LOG(MDDP_LL_DEBUG,
			"%s: check tcpudp nat tuple[%p], last_cnt[%d], curr_cnt[%d], need_tag[%d].\n",
			__func__, found_nat_tuple,
			found_nat_tuple->last_cnt,
			found_nat_tuple->curr_cnt,
			found_nat_tuple->is_need_tag);

		if (found_nat_tuple->is_need_tag == true) {
			found_nat_tuple->is_need_tag = false;
			MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);

			return true;
		}
		MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);

		return false;
	}
	MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);

	/* not found */
	return true;
}

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

#define IPC_HDR_IS_V4(_ip_hdr) \
	(0x40 == (*((unsigned char *)(_ip_hdr)) & 0xf0))




//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------

static inline void _mddp_f_in_tail_v4(
	struct mddp_f_desc *desc,
	struct sk_buff *skb)
{
	struct mddp_f_cb *cb;
	struct ip4header *ip;
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

	if (desc->flag & DESC_FLAG_TRACK_NAT) {

		cb->dev = skb->dev;

		ip = (struct ip4header *) (skb->data + desc->l3_off);

		cb->src[0] = ip->ip_src;
		cb->dst[0] = ip->ip_dst;
		switch (ip->ip_p) {
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
					"IPv4 BUG, %s: Should not reach here, skb[%p], protocol[%d].\n",
					__func__, skb, ip->ip_p);
			break;
		}
		cb->proto = ip->ip_p;
		cb->v4_ip_id = ip->ip_id;

		return;
	}
	MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: Invalid flag[%x], skb[%p]!\n",
				__func__, desc->flag, skb);
}

static inline void _mddp_f_in_nat(
	unsigned char *offset2,
	struct mddp_f_desc *desc,
	struct sk_buff *skb)
{
	/* IPv4 */
	struct tuple t4;
	struct ip4header *ip;

	void *l4_header;


	if (likely(IPC_HDR_IS_V4(offset2))) {	/* support NAT and ROUTE */
		desc->flag |= DESC_FLAG_IPV4;

		ip = (struct ip4header *) offset2;

		MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: IPv4 pre-routing, skb[%p], ip_id[%x], checksum[%x], protocol[%d], in_dev[%s].\n",
				__func__, skb, ip->ip_id,
				ip->ip_sum, ip->ip_p, skb->dev->name);

		/* ip fragmentation? */
		if (ip->ip_off & 0xff3f) {
			desc->flag |= DESC_FLAG_IPFRAG;
			return;
		}

		desc->l3_len = ip->ip_hl << 2;
		desc->l4_off = desc->l3_off + desc->l3_len;
		l4_header = skb->data + desc->l4_off;

		t4.nat.src = ip->ip_src;
		t4.nat.dst = ip->ip_dst;
		t4.nat.proto = ip->ip_p;
		t4.dev_in = skb->dev;

		switch (ip->ip_p) {
		case IPPROTO_TCP:
			mddp_f_ip4_tcp(desc, skb, &t4, ip, l4_header);
			return;
		case IPPROTO_UDP:
			mddp_f_ip4_udp(desc, skb, &t4, ip, l4_header);
			return;
		default:
			desc->flag |= DESC_FLAG_UNKNOWN_PROTOCOL;
			return;
		}
	} else {
		memset(desc, 0, sizeof(*desc));	/* avoid compiler warning */
		desc->flag |= DESC_FLAG_UNKNOWN_ETYPE;
		return;
	}
}

static int mddp_f_in_nf_v4(struct sk_buff *skb)
{
	struct mddp_f_desc desc;

	/* HW */
	desc.flag = 0;
	desc.l3_off = 0;


	skb_set_network_header(skb, desc.l3_off);

	_mddp_f_in_nat(skb->data, &desc, skb);
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

	_mddp_f_in_tail_v4(&desc, skb);

	return 0;		/* original path */
}

static void mddp_f_out_nf_ipv4(
	struct sk_buff *skb,
	struct net_device *out,
	unsigned char *offset2,
	struct mddp_f_cb *cb,
	struct mddp_f_track_table_t *curr_track_table)
{
	struct nf_conn *nat_ip_conntrack;
	enum ip_conntrack_info ctinfo;
	struct tuple t;
	struct nat_tuple *found_nat_tuple;
	struct tcpheader *tcp;
	struct udpheader *udp;
	unsigned char tcp_state;
	unsigned char ext_offset;
	unsigned long flag;
	bool is_uplink;
	int not_match = 0;
	unsigned int tuple_hit_cnt = 0;
	int ret;
	struct ip4header *ip = (struct ip4header *) offset2;

	memset(&t, 0, sizeof(struct tuple));

	MDDP_F_LOG(MDDP_LL_INFO,
				"%s: IPv4 add rule, skb[%p], cb->proto[%d], ip->ip_p[%d], offset2[%p], ip_id[%x], checksum[%x].\n",
				__func__, skb, cb->proto, ip->ip_p,
				offset2, ip->ip_id, ip->ip_sum);

	if (cb->proto != ip->ip_p) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: IPv4 protocol mismatch, cb->proto[%d], ip->ip_p[%d], skb[%p] is filtered out.\n",
				__func__, cb->proto, ip->ip_p, skb);
		goto out;
	}

	offset2 += (ip->ip_hl << 2);

	switch (ip->ip_p) {
	case IPPROTO_TCP:
		tcp = (struct tcpheader *) offset2;
		nat_ip_conntrack = nf_ct_get(skb, &ctinfo);
		if (!nat_ip_conntrack) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Null ip conntrack, skb[%p] is filtered out.\n",
					__func__, skb);
			goto out;
		}

		tcp_state = nat_ip_conntrack->proto.tcp.state;
		if (nat_ip_conntrack->ext) { /* helper */
			ext_offset = nat_ip_conntrack->ext->offset[NF_CT_EXT_HELPER];
			if (ext_offset) {
				MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: skb[%p] is filtered out, ext[%p], ext_offset[%d].\n",
					__func__, skb,
					nat_ip_conntrack->ext, ext_offset);
				goto out;
			}
		}

		if (mddp_f_contentfilter
				&& (tcp->th_dport == htons(80)
				|| tcp->th_sport == htons(80))
				&& nat_ip_conntrack->mark != 0x80000000) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Invalid parameter, contentfilter[%d], dport[%x], sport[%x], mark[%x], skb[%p] is filtered out,.\n",
					__func__, mddp_f_contentfilter,
					tcp->th_dport,
					tcp->th_sport,
					nat_ip_conntrack->mark, skb);
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

		if (mddp_f_is_support_wan_dev(out->name) == true) {
			is_uplink = true;

			not_match += (ip->ip_dst != cb->dst[0]) ? 1 : 0;
			not_match += (ip->ip_p != cb->proto) ? 1 : 0;
			not_match += (ip->ip_id != cb->v4_ip_id) ? 1 : 0;
			not_match += (tcp->th_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				MDDP_F_LOG(MDDP_LL_INFO,
					"%s: IPv4 TCP UL tag not_match[%d], ip_dst[%x], ip_p[%d], ip_id[%x], dport[%x], cb_dst[%x], cb_proto[%d], cb_ip_id[%x], cb_dport[%x].\n",
					__func__, not_match, ip->ip_dst,
					ip->ip_p, ip->ip_id,
					tcp->th_dport, cb->dst[0],
					cb->proto, cb->v4_ip_id,
					cb->dport);

				goto out;
			}

			t.nat.src = cb->src[0];
			t.nat.dst = cb->dst[0];
			t.nat.proto = cb->proto;
			t.nat.s.tcp.port = cb->sport;
			t.nat.d.tcp.port = cb->dport;
			t.dev_in = cb->dev;
		} else {
			/* Do not tag TCP DL packet */
			MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Do not tag IPv4 TCP DL.\n",
					__func__);

			goto out;
		}

		/* Tag this packet for MD tracking */
		if (skb_headroom(skb) > sizeof(struct mddp_f_tag_packet_t)) {
			MDDP_F_NAT_TUPLE_LOCK(&mddp_f_nat_tuple_lock, flag);
			found_nat_tuple =
				mddp_f_get_nat_tuple_ip4_tcpudp_wo_lock(&t);

			if (found_nat_tuple) {
				tuple_hit_cnt = found_nat_tuple->curr_cnt;
				found_nat_tuple->last_cnt = 0;
				found_nat_tuple->curr_cnt = 0;
				MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);

				MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: tuple[%p] is found!!\n",
					__func__, found_nat_tuple);
				ret = mddp_f_tag_packet(is_uplink,
						skb, out, cb, ip->ip_p,
						ip->ip_v, tuple_hit_cnt);
				if (ret == 0)
					MDDP_F_LOG(MDDP_LL_NOTICE,
						"%s: Add IPv4 TCP MDDP tag, is_uplink[%d], skb[%p], ip_id[%x], ip_checksum[%x].\n",
						__func__, is_uplink, skb,
						ip->ip_id, ip->ip_sum);

				goto out;
			} else {
				MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock, flag);

				/* Save tuple to avoid tag many packets */
				found_nat_tuple = kmem_cache_alloc(
							mddp_f_nat_tuple_cache, GFP_ATOMIC);
				if (found_nat_tuple == NULL) {
					MDDP_F_LOG(MDDP_LL_NOTICE,
							"%s: kmem_cache_alloc() failed\n",
							__func__);
					goto out;
				}

				found_nat_tuple->src_ip = cb->src[0];
				found_nat_tuple->dst_ip = cb->dst[0];
				found_nat_tuple->nat_src_ip = ip->ip_src;
				found_nat_tuple->nat_dst_ip = ip->ip_dst;
				found_nat_tuple->src.tcp.port = cb->sport;
				found_nat_tuple->dst.tcp.port = cb->dport;
				found_nat_tuple->nat_src.tcp.port = tcp->th_sport;
				found_nat_tuple->nat_dst.tcp.port = tcp->th_dport;
				found_nat_tuple->proto = ip->ip_p;
				found_nat_tuple->dev_src = cb->dev;
				found_nat_tuple->dev_dst = out;

				mddp_f_add_nat_tuple(found_nat_tuple);

				ret = mddp_f_tag_packet(is_uplink,
						skb, out, cb, ip->ip_p,
						ip->ip_v, tuple_hit_cnt);
				if (ret == 0)
					MDDP_F_LOG(MDDP_LL_NOTICE,
							"%s: Add IPv4 TCP MDDP tag, is_uplink[%d], skb[%p], ip_id[%x], ip_checksum[%x].\n",
							__func__, is_uplink,
							skb, ip->ip_id,
							ip->ip_sum);
			}
		} else {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Headroom of skb[%p] is not enough to add MDDP tag, headroom[%d].\n",
					__func__, skb, skb_headroom(skb));
		}
		break;
	case IPPROTO_UDP:
		udp = (struct udpheader *) offset2;
		nat_ip_conntrack = nf_ct_get(skb, &ctinfo);
		if (!nat_ip_conntrack) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Null ip conntrack, skb[%p] is filtered out.\n",
					__func__, skb);
			goto out;
		}

		if (nat_ip_conntrack->ext) { /* helper */
			ext_offset = nat_ip_conntrack->ext->offset[NF_CT_EXT_HELPER];
			if (ext_offset) {
				MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: skb[%p] is filtered out, ext[%p], ext_offset[%d].\n",
					__func__, skb,
					nat_ip_conntrack->ext, ext_offset);
				goto out;
			}
		}

		if (mddp_f_is_support_wan_dev(out->name) == true) {
			is_uplink = true;

			not_match += (ip->ip_dst != cb->dst[0]) ? 1 : 0;
			not_match += (ip->ip_p != cb->proto) ? 1 : 0;
			not_match += (ip->ip_id != cb->v4_ip_id) ? 1 : 0;
			not_match += (udp->uh_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				MDDP_F_LOG(MDDP_LL_INFO,
					"%s: IPv4 UDP UL tag not_match[%d], ip_dst[%x], ip_p[%d], ip_id[%x], dport[%x], cb_dst[%x], cb_proto[%d], cb_ip_id[%x], cb_dport[%x].\n",
					__func__, not_match, ip->ip_dst,
					ip->ip_p, ip->ip_id,
					udp->uh_dport, cb->dst[0],
					cb->proto, cb->v4_ip_id,
					cb->dport);

				goto out;
			}

			t.nat.src = cb->src[0];
			t.nat.dst = cb->dst[0];
			t.nat.proto = cb->proto;
			t.nat.s.udp.port = cb->sport;
			t.nat.d.udp.port = cb->dport;
			t.dev_in = cb->dev;
		} else {
			is_uplink = false;

			not_match += (ip->ip_src != cb->src[0]) ? 1 : 0;
			not_match += (ip->ip_p != cb->proto) ? 1 : 0;
			not_match += (ip->ip_id != cb->v4_ip_id) ? 1 : 0;
			not_match += (udp->uh_sport != cb->sport) ? 1 : 0;
			if (not_match) {
				MDDP_F_LOG(MDDP_LL_INFO,
					"%s: IPv4 UDP DL tag not_match[%d], ip_src[%x], ip_p[%d], ip_id[%x], sport[%x], cb_src[%x], cb_proto[%d], cb_ip_id[%x], cb_sport[%x].\n",
					__func__, not_match, ip->ip_src,
					ip->ip_p, ip->ip_id,
					udp->uh_sport, cb->src[0],
					cb->proto, cb->v4_ip_id,
					cb->sport);

				goto out;
			}
		}

		if (cb->sport != 67 && cb->sport != 68) {
			/* Don't fastpath dhcp packet */

			/* Tag this packet for MD tracking */
			if (skb_headroom(skb)
					> sizeof(struct mddp_f_tag_packet_t)) {
				MDDP_F_NAT_TUPLE_LOCK(&mddp_f_nat_tuple_lock, flag);
				found_nat_tuple =
					mddp_f_get_nat_tuple_ip4_tcpudp_wo_lock(
							&t);

				if (found_nat_tuple) {
					tuple_hit_cnt =
						found_nat_tuple->curr_cnt;
					found_nat_tuple->last_cnt = 0;
					found_nat_tuple->curr_cnt = 0;
					MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock,
							flag);

					MDDP_F_LOG(MDDP_LL_DEBUG,
							"%s: tuple[%p] is found!!\n",
							__func__,
							found_nat_tuple);
					if (is_uplink == false) {
						MDDP_F_LOG(MDDP_LL_DEBUG,
							"%s: No need to tag UDP DL.\n",
							__func__);
						goto out;
					}

					ret = mddp_f_tag_packet(is_uplink,
							skb, out, cb, ip->ip_p,
							ip->ip_v,
							tuple_hit_cnt);
					if (ret == 0)
						MDDP_F_LOG(MDDP_LL_NOTICE,
							"%s: Add IPv4 UDP MDDP tag, is_uplink[%d], skb[%p], ip_id[%x], ip_checksum[%x].\n",
							__func__, is_uplink,
							skb, ip->ip_id,
							ip->ip_sum);

					goto out;
				} else {
					MDDP_F_NAT_TUPLE_UNLOCK(&mddp_f_nat_tuple_lock,
							flag);

					/* Save tuple to avoid tag many packets */
					found_nat_tuple = kmem_cache_alloc(
								mddp_f_nat_tuple_cache, GFP_ATOMIC);
					if (found_nat_tuple == NULL) {
						MDDP_F_LOG(MDDP_LL_NOTICE,
								"%s: kmem_cache_alloc() failed\n",
								__func__);
						goto out;
					}

					found_nat_tuple->src_ip = cb->src[0];
					found_nat_tuple->dst_ip = cb->dst[0];
					found_nat_tuple->nat_src_ip = ip->ip_src;
					found_nat_tuple->nat_dst_ip = ip->ip_dst;
					found_nat_tuple->src.udp.port = cb->sport;
					found_nat_tuple->dst.udp.port = cb->dport;
					found_nat_tuple->nat_src.udp.port = udp->uh_sport;
					found_nat_tuple->nat_dst.udp.port = udp->uh_dport;
					found_nat_tuple->proto = ip->ip_p;
					found_nat_tuple->dev_src = cb->dev;
					found_nat_tuple->dev_dst = out;

					mddp_f_add_nat_tuple(found_nat_tuple);

					ret = mddp_f_tag_packet(is_uplink,
							skb, out, cb, ip->ip_p,
							ip->ip_v,
							tuple_hit_cnt);
					if (ret == 0)
						MDDP_F_LOG(MDDP_LL_NOTICE,
							"%s: Add IPv4 UDP MDDP tag, is_uplink[%d], skb[%p], ip_id[%x], ip_checksum[%x].\n",
							__func__, is_uplink,
							skb, ip->ip_id,
							ip->ip_sum);
				}
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
					__func__, ip->ip_p, skb);
		break;
	}

out:
	put_track_table(curr_track_table);
}

static void mddp_f_out_nf_v4(struct sk_buff *skb, struct net_device *out)
{
	unsigned char *offset2 = skb->data;
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

	if (cb->flag & DESC_FLAG_TRACK_NAT) {
		if (IPC_HDR_IS_V4(offset2)) {
			mddp_f_out_nf_ipv4(skb, out, offset2, cb,
							curr_track_table);
			return;

		} else {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Wrong IPv4 version[%d], offset[%p], flag[%x], skb[%p].\n",
					__func__, IPC_HDR_IS_V4(offset2),
					offset2, cb->flag, skb);
		}


	} else {
		MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: No need to track, skb[%p], cb->flag[%x].\n",
					__func__, skb, cb->flag);
	}

out:
	put_track_table(curr_track_table);
}

static uint32_t mddp_nfhook_prerouting_v4
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
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

	mddp_f_in_nf_v4(skb);

	return NF_ACCEPT;
}

static uint32_t mddp_nfhook_postrouting_v4
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
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

	mddp_f_out_nf_v4(skb, state->out);

out:
	return NF_ACCEPT;
}


// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <net/arp.h>
#include <net/ip6_route.h>
#include <net/ipv6.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/route.h>

#include "mddp_ctrl.h"
#include "mddp_debug.h"
#include "mddp_dev.h"
#include "mddp_filter.h"
#include "mddp_f_config.h"
#include "mddp_f_desc.h"
#include "mddp_f_dev.h"
#include "mddp_f_proto.h"
#include "mddp_f_tuple.h"
#include "mddp_ipc.h"
#include "mtk_ccci_common.h"

static int mddp_f_max_nat = MD_DIRECT_TETHERING_RULE_NUM;
static int mddp_f_max_router = MD_DIRECT_TETHERING_RULE_NUM;
static struct kmem_cache *mddp_f_nat_tuple_cache;
static struct kmem_cache *mddp_f_router_tuple_cache;

static spinlock_t mddp_f_tuple_lock;
#define MDDP_F_TUPLE_INIT_LOCK(LOCK) spin_lock_init((LOCK))
#define MDDP_F_TUPLE_LOCK(LOCK, FLAG) spin_lock_irqsave((LOCK), (FLAG))
#define MDDP_F_TUPLE_UNLOCK(LOCK, FLAG) spin_unlock_irqrestore((LOCK), (FLAG))

#include "mddp_f_tuple.c"
#include "mddp_f_hw.c"

#define TRACK_TABLE_INIT_LOCK(TABLE) \
		spin_lock_init((&(TABLE).lock))
#define TRACK_TABLE_LOCK(TABLE, flags) \
		spin_lock_irqsave((&(TABLE).lock), (flags))
#define TRACK_TABLE_UNLOCK(TABLE, flags) \
		spin_unlock_irqrestore((&(TABLE).lock), (flags))

#define INTERFACE_TYPE_LAN	0
#define INTERFACE_TYPE_WAN	1
#define INTERFACE_TYPE_IOC	2

enum mddp_f_rule_tag_info_e {
	MDDP_RULE_TAG_NORMAL_PACKET = 0,
	MDDP_RULE_TAG_FAKE_DL_NAT_PACKET,
};

struct mddp_f_cb {
	u_int32_t flag;
	u_int32_t src[4];	/* IPv4 use src[0] */
	u_int32_t dst[4];	/* IPv4 use dst[0] */
	u_int16_t sport;
	u_int16_t dport;
	struct net_device *dev;
	u_int16_t v4_ip_id;
	u_int8_t proto;
};

#define MDDP_F_MAX_TRACK_NUM 512
#define MDDP_F_MAX_TRACK_TABLE_LIST 16
#define MDDP_F_TABLE_BUFFER_NUM 3000

struct mddp_f_track_table_t {
	struct mddp_f_cb cb;
	unsigned int ref_count;
	void *tracked_address;
	unsigned long jiffies;
	struct mddp_f_track_table_t *next_track_table;
};

struct mddp_f_track_table_list_t {
	struct mddp_f_track_table_t *table;
	spinlock_t lock;
};

static void del_all_track_table(void);

static inline void ipv6_addr_copy(
	struct in6_addr *a1,
	const struct in6_addr *a2)
{
	memcpy(a1, a2, sizeof(struct in6_addr));
}

#define IPC_HDR_IS_V4(_ip_hdr) \
	(0x40 == (*((unsigned char *)(_ip_hdr)) & 0xf0))
#define IPC_HDR_IS_V6(_ip_hdr) \
	(0x60 == (*((unsigned char *)(_ip_hdr)) & 0xf0))



static u32 mddp_f_jhash_initval __read_mostly;

static int mddp_f_contentfilter;
module_param(mddp_f_contentfilter, int, 0000);

static spinlock_t mddp_f_lock;
#define MDDP_F_INIT_LOCK(LOCK) spin_lock_init((LOCK))
#define MDDP_F_LOCK(LOCK, FLAG) spin_lock_irqsave((LOCK), (FLAG))
#define MDDP_F_UNLOCK(LOCK, FLAG) spin_unlock_irqrestore((LOCK), (FLAG))

static uint32_t mddp_f_suspend_s;

static struct mddp_f_track_table_list_t mddp_f_track[MDDP_F_MAX_TRACK_NUM];
static struct mddp_f_track_table_list_t mddp_f_table_buffer;
static unsigned int buffer_cnt;

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
struct mddp_f_set_ct_timeout_req_t {
	uint32_t                udp_ct_timeout;
	uint32_t                tcp_ct_timeout;
	uint8_t                 rsv[4];
};

struct mddp_f_set_ct_timeout_rsp_t {
	uint32_t                udp_ct_timeout;
	uint32_t                tcp_ct_timeout;
	uint8_t                 result;
	uint8_t                 rsv[3];
};

//------------------------------------------------------------------------------
// Function prototype.
//------------------------------------------------------------------------------
static uint32_t mddp_nfhook_prerouting
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
static uint32_t mddp_nfhook_postrouting
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);

//------------------------------------------------------------------------------
// Registered callback function.
//------------------------------------------------------------------------------
static struct nf_hook_ops mddp_nf_ops[] __read_mostly = {
	{
		.hook           = mddp_nfhook_prerouting,
		.pf             = NFPROTO_IPV4,
		.hooknum        = NF_INET_PRE_ROUTING,
		.priority       = NF_IP_PRI_FIRST + 1,
	},
	{
		.hook           = mddp_nfhook_postrouting,
		.pf             = NFPROTO_IPV4,
		.hooknum        = NF_INET_POST_ROUTING,
		.priority       = NF_IP_PRI_LAST,
	},
	{
		.hook           = mddp_nfhook_prerouting,
		.pf             = NFPROTO_IPV6,
		.hooknum        = NF_INET_PRE_ROUTING,
		.priority       = NF_IP6_PRI_FIRST + 1,
	},
	{
		.hook           = mddp_nfhook_postrouting,
		.pf             = NFPROTO_IPV6,
		.hooknum        = NF_INET_POST_ROUTING,
		.priority       = NF_IP6_PRI_LAST,
	},
};

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------
static void mddp_f_init_table_buffer(void)
{
	int i;
	struct mddp_f_track_table_t *track_table;
	struct mddp_f_track_table_t *next_track_table = NULL;

	TRACK_TABLE_INIT_LOCK(mddp_f_table_buffer);
	mddp_f_table_buffer.table = NULL;

	for (i = 0; i < MDDP_F_TABLE_BUFFER_NUM; i++) {
		track_table = kmalloc(sizeof(struct mddp_f_track_table_t),
								GFP_ATOMIC);
		if (!track_table)
			continue;

		memset(track_table, 0, sizeof(struct mddp_f_track_table_t));
		track_table->next_track_table = next_track_table;
		next_track_table = track_table;

		buffer_cnt++;
	}
	mddp_f_table_buffer.table = track_table;

	MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Total table buffer num[%d], table head[%p]!\n",
				__func__, buffer_cnt,
				mddp_f_table_buffer.table);
}

static struct mddp_f_track_table_t *mddp_f_get_table_buffer(void)
{
	unsigned long flags;
	struct mddp_f_track_table_t *track_table = NULL;

	TRACK_TABLE_LOCK(mddp_f_table_buffer, flags);

	if (mddp_f_table_buffer.table) {
		track_table = mddp_f_table_buffer.table;
		mddp_f_table_buffer.table =
				mddp_f_table_buffer.table->next_track_table;

		buffer_cnt--;
	}

	TRACK_TABLE_UNLOCK(mddp_f_table_buffer, flags);

	return track_table;
}

static void mddp_f_put_table_buffer(struct mddp_f_track_table_t *track_table)
{
	unsigned long flags;

	TRACK_TABLE_LOCK(mddp_f_table_buffer, flags);

	track_table->next_track_table = mddp_f_table_buffer.table;
	mddp_f_table_buffer.table = track_table;
	buffer_cnt++;

	TRACK_TABLE_UNLOCK(mddp_f_table_buffer, flags);
}

static inline struct mddp_f_cb *_insert_track_table(
	struct sk_buff *skb,
	struct mddp_f_track_table_t *track_table)
{

	track_table->ref_count = 0;
	track_table->tracked_address = (void *)skb;
	track_table->jiffies = jiffies;
	track_table->next_track_table = NULL;

	return &track_table->cb;
}

static inline void _remove_track_table(
	unsigned int index,
	bool is_first_track_table,
	struct mddp_f_track_table_t *prev_track_table,
	struct mddp_f_track_table_t *curr_track_table)
{
	if (is_first_track_table == true)
		mddp_f_track[index].table = curr_track_table->next_track_table;
	else
		prev_track_table->next_track_table =
					curr_track_table->next_track_table;

	mddp_f_put_table_buffer(curr_track_table);

}

static void mddp_f_init_track_table(void)
{
	int i;

	for (i = 0; i < MDDP_F_MAX_TRACK_NUM; i++) {
		TRACK_TABLE_INIT_LOCK(mddp_f_track[i]);
		mddp_f_track[i].table = NULL;
	}
}

static void dest_track_table(void)
{
	del_all_track_table();
}

static struct mddp_f_cb *add_track_table(
	struct sk_buff *skb,
	struct mddp_f_desc *desc)
{
	struct mddp_f_cb *cb = NULL;
	unsigned int hash;
	unsigned long flags;
	unsigned int list_cnt = 0;
	bool is_first_track_table = true;
	struct mddp_f_track_table_t *track_table = NULL;
	struct mddp_f_track_table_t *curr_track_table = NULL;
	struct mddp_f_track_table_t *prev_track_table = NULL;
	struct mddp_f_track_table_t *tmp_track_table = NULL;

	if (!skb || !desc) {
		MDDP_F_LOG(MDDP_LL_WARN,
				"%s: Null input, skb[%p], desc[%p].\n",
				__func__, skb, desc);
		goto out;
	}

	if ((desc->flag & DESC_FLAG_IPV4) || (desc->flag & DESC_FLAG_IPV6)) {
		/* Get hash value */
		hash = jhash_1word(((unsigned long)skb & 0xFFFFFFFF),
				  mddp_f_jhash_initval) % MDDP_F_MAX_TRACK_NUM;
	} else {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Not a IPv4/v6 packet, flag[%x], skb[%p].\n",
				__func__, desc->flag, skb);
		goto out;
	}

	track_table = mddp_f_get_table_buffer();
	if (track_table)
		memset(track_table, 0, sizeof(struct mddp_f_track_table_t));
	else
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Table buffer is used up, cannot get table buffer!.\n",
				__func__);


	TRACK_TABLE_LOCK(mddp_f_track[hash], flags);
	curr_track_table = mddp_f_track[hash].table;

	if (!curr_track_table) {
		mddp_f_track[hash].table = track_table;
	} else {
		while (curr_track_table) {
			if (list_cnt >= MDDP_F_MAX_TRACK_TABLE_LIST) {
				MDDP_F_LOG(MDDP_LL_NOTICE,
						"%s: Reach max hash list! Don't add track table[%p], skb[%p], hash[%d], remain buffer[%d].\n",
						__func__, track_table,
						skb, hash, buffer_cnt+1);

				if (track_table)
					mddp_f_put_table_buffer(track_table);

				TRACK_TABLE_UNLOCK(mddp_f_track[hash], flags);
				goto out;
			}

			tmp_track_table = curr_track_table->next_track_table;

			if (curr_track_table->ref_count == 0) {
				/* Remove timeout track table */
				if (time_after(jiffies,
					(curr_track_table->jiffies +
					msecs_to_jiffies(10)))) {
					MDDP_F_LOG(MDDP_LL_INFO,
					"%s: Remove timeout track table[%p], skb[%p], hash[%d], is_first_track_table[%d], remain buffer[%d].\n",
					__func__, curr_track_table,
					skb, hash,
					is_first_track_table,
					buffer_cnt+1);

					_remove_track_table(hash,
							is_first_track_table,
							prev_track_table,
							curr_track_table);
					curr_track_table = tmp_track_table;
					continue;
				}

				/* Remove track table of duplicated skb */
				if (unlikely(curr_track_table->tracked_address
						== (void *)skb)) {
					MDDP_F_LOG(MDDP_LL_INFO,
						"%s: Remove track table[%p] of duplicated skb[%p], hash[%d], is_first_track_table[%d], remain buffer[%d].\n",
						__func__, curr_track_table,
						skb, hash,
						is_first_track_table,
						buffer_cnt+1);

					_remove_track_table(hash,
							is_first_track_table,
							prev_track_table,
							curr_track_table);
					curr_track_table = tmp_track_table;
					continue;
				}
			}

			is_first_track_table = false;
			prev_track_table = curr_track_table;
			curr_track_table = tmp_track_table;
			list_cnt++;
		}

		if (is_first_track_table == true)
			mddp_f_track[hash].table = track_table;
		else
			prev_track_table->next_track_table = track_table;
	}

	/* insert current skb into track table */
	if (track_table)
		cb = _insert_track_table(skb, track_table);

	TRACK_TABLE_UNLOCK(mddp_f_track[hash], flags);

	MDDP_F_LOG(MDDP_LL_INFO,
				"%s: Add track table, skb[%p], hash[%d], is_first_track_table[%d], track_table[%p].\n",
				__func__, skb, hash,
				is_first_track_table,
				track_table);

out:
	return cb;
}

static struct mddp_f_cb *search_and_hold_track_table(
	struct sk_buff *skb,
	struct mddp_f_track_table_t **curr_track_table)
{
	unsigned int hash;
	unsigned long flags;
	struct mddp_f_cb *cb = NULL;
	bool is_first_track_table = true;
	struct mddp_f_track_table_t *prev_track_table = NULL;

	hash = jhash_1word(((unsigned long)skb & 0xFFFFFFFF),
				mddp_f_jhash_initval) % MDDP_F_MAX_TRACK_NUM;

	MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: 1. Search track table, skb[%p], hash[%d].\n",
				__func__, skb, hash);

	TRACK_TABLE_LOCK(mddp_f_track[hash], flags);
	*curr_track_table = mddp_f_track[hash].table;
	while (*curr_track_table) {
		if (((*curr_track_table)->tracked_address == (void *)skb)
			&& ((*curr_track_table)->ref_count == 0)) {
			/* Pop the track table from hash list */
			if (is_first_track_table == true)
				mddp_f_track[hash].table =
					(*curr_track_table)->next_track_table;
			else
				prev_track_table->next_track_table =
					(*curr_track_table)->next_track_table;

			(*curr_track_table)->ref_count++;
			cb = &((*curr_track_table)->cb);

			break;
		}

		is_first_track_table = false;
		prev_track_table = *curr_track_table;
		*curr_track_table = (*curr_track_table)->next_track_table;
	}
	TRACK_TABLE_UNLOCK(mddp_f_track[hash], flags);

	return cb;
}

static void put_track_table(
	struct mddp_f_track_table_t *curr_track_table)
{
	curr_track_table->ref_count--;
	if (curr_track_table->ref_count == 0) {
		MDDP_F_LOG(MDDP_LL_DEBUG,
				"%s: Remove track table track_table[%p], remain buffer[%d].\n",
				__func__, curr_track_table, buffer_cnt+1);
		mddp_f_put_table_buffer(curr_track_table);
	} else {
		WARN_ON(1);
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Invalid ref_count of track table[%p], ref_cnt[%d].\n",
				__func__, curr_track_table,
				curr_track_table->ref_count);
	}
}

static void del_all_track_table(void)
{
	int i;
	unsigned long flags;
	struct mddp_f_track_table_t *curr_track_table = NULL;
	struct mddp_f_track_table_t *tmp_track_table = NULL;

	MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: Delete all track table!\n", __func__);

	/* Free table buffer on hash table */
	for (i = 0; i < MDDP_F_MAX_TRACK_NUM; i++) {
		TRACK_TABLE_LOCK(mddp_f_track[i], flags);
		curr_track_table = mddp_f_track[i].table;

		while (curr_track_table) {
			tmp_track_table = curr_track_table->next_track_table;
			kfree(curr_track_table);
			curr_track_table = tmp_track_table;
		}
		mddp_f_track[i].table = NULL;
		TRACK_TABLE_UNLOCK(mddp_f_track[i], flags);
	}

	/* Free unused table buffer */
	TRACK_TABLE_LOCK(mddp_f_table_buffer, flags);
	curr_track_table = mddp_f_table_buffer.table;

	while (curr_track_table) {
		tmp_track_table = curr_track_table->next_track_table;
		kfree(curr_track_table);
		curr_track_table = tmp_track_table;
	}
	TRACK_TABLE_UNLOCK(mddp_f_table_buffer, flags);
}

#define MDDP_F_DEVICE_REGISTERING_HASH_SIZE (32)
static spinlock_t device_registering_lock;
static struct list_head *device_registering_hash;
struct device_registering {
	struct list_head list;
	struct net_device *dev;
};

static inline unsigned int mddp_f_get_hash_by_device(struct net_device *dev)
{
	unsigned int ret;

	ret = jhash_1word((uintptr_t) dev, 0)
				% MDDP_F_DEVICE_REGISTERING_HASH_SIZE;

	return ret;
}

static inline void mddp_f_release_device(struct device_registering *device)
{
	list_del(&device->list);
	dev_put(device->dev);
	device->dev = NULL;
	kfree(device);
}

static int mddp_f_notifier_init(void)
{
	int ret = 0;

	int i;

	spin_lock_init(&device_registering_lock);
	device_registering_hash =
			vmalloc(sizeof(struct list_head)
					* MDDP_F_DEVICE_REGISTERING_HASH_SIZE);

	for (i = 0; i < MDDP_F_DEVICE_REGISTERING_HASH_SIZE; i++)
		INIT_LIST_HEAD(&device_registering_hash[i]);

	return ret;
}

static void mddp_f_notifier_dest(void)
{
	unsigned long flags;
	struct device_registering *found_device;
	int i;

	/* release all registered devices */
	spin_lock_irqsave(&device_registering_lock, flags);
	for (i = 0; i < MDDP_F_DEVICE_REGISTERING_HASH_SIZE; i++) {
		list_for_each_entry(found_device, &device_registering_hash[i],
							list) {
			mddp_f_release_device(found_device);
		}
	}
	spin_unlock_irqrestore(&device_registering_lock, flags);
	vfree(device_registering_hash);
}

static void mddp_f_init_jhash(void)
{
	get_random_bytes(&mddp_f_jhash_initval, sizeof(mddp_f_jhash_initval));
}

/*
 *	Return Value: added extension tag length.
 */
static int mddp_f_e_tag_packet(
	bool is_uplink,
	struct sk_buff *skb,
	unsigned char ip_ver,
	struct mddp_f_cb *cb,
	unsigned int hit_cnt)
{
	struct mddp_f_e_tag_common_t *skb_e_tag;
	struct mddp_f_e_tag_mac_t e_tag_mac;
	struct ethhdr *eth_hdr;
	int tag_len, etag_len;

	struct dst_entry *dst = skb_dst(skb);
	struct rtable *rt = (struct rtable *)dst;
	u32 nexthop_v4;
	const struct in6_addr *nexthop_v6;
	struct neighbour *neigh;

	/* extension tag for MAC address */

	/* headroom check */
	tag_len = sizeof(struct mddp_f_tag_packet_t);
	etag_len = sizeof(struct mddp_f_e_tag_common_t) +
			sizeof(struct mddp_f_e_tag_mac_t);
	if (skb_headroom(skb) < (tag_len + etag_len)) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Add MDDP Etag Fail, headroom[%d], tag_len[%d], etag_len[%d]\n",
				__func__, skb_headroom(skb),
				tag_len, etag_len);
		return -1;
	}

	skb_e_tag = (struct mddp_f_e_tag_common_t *)
		((unsigned char *)skb->head + tag_len);
	skb_e_tag->type = MDDP_E_TAG_MAC;
	skb_e_tag->len  = etag_len;

	if (is_uplink == true) {
		if (!skb_mac_header_was_set(skb)) {
			MDDP_F_LOG(MDDP_LL_WARN,
				"%s: Add MDDP Etag Fail, is_uplink[%d], mac_hdr_set[%d]\n",
				__func__, is_uplink,
				skb_mac_header_was_set(skb));
			return -1;
		}
		eth_hdr = (struct ethhdr *)skb_mac_header(skb);

		memcpy(e_tag_mac.mac_addr, eth_hdr->h_source,
				sizeof(e_tag_mac.mac_addr));
	} else {
		rcu_read_lock_bh();
		if (ip_ver == 4) {
			nexthop_v4 = (__force u32)rt_nexthop(rt,
					ip_hdr(skb)->daddr);
			neigh = __ipv4_neigh_lookup_noref(dst->dev, nexthop_v4);
		} else if (ip_ver == 6) {
			nexthop_v6 = rt6_nexthop((struct rt6_info *)dst,
					&ipv6_hdr(skb)->daddr);
			neigh = __ipv6_neigh_lookup_noref(dst->dev, nexthop_v6);
		} else {
			rcu_read_unlock_bh();
			MDDP_F_LOG(MDDP_LL_WARN,
					"%s: Add MDDP Etag Fail, ip_ver[%d]\n",
					__func__, ip_ver);
			return -1;
		}

		if (neigh == NULL) {
			rcu_read_unlock_bh();
			MDDP_F_LOG(MDDP_LL_WARN,
					"%s: Add MDDP Etag Fail, neigh[%x]\n",
					__func__, neigh);
			return -1;
		}

		if (neigh->nud_state & NUD_VALID)
			neigh_ha_snapshot(e_tag_mac.mac_addr,
					neigh, neigh->dev);

		rcu_read_unlock_bh();
	}

	e_tag_mac.access_cnt = hit_cnt;

	memcpy(skb_e_tag->value, &e_tag_mac, sizeof(struct mddp_f_e_tag_mac_t));

	MDDP_F_LOG(MDDP_LL_INFO,
			"%s: Add MDDP Etag, type[%x], len[%d], mac[%02x:%02x:%02x:%02x:%02x:%02x], access_cnt[%d]\n",
			__func__, skb_e_tag->type, skb_e_tag->len,
			e_tag_mac.mac_addr[0], e_tag_mac.mac_addr[1],
			e_tag_mac.mac_addr[2], e_tag_mac.mac_addr[3],
			e_tag_mac.mac_addr[4], e_tag_mac.mac_addr[5],
			e_tag_mac.access_cnt);

	return etag_len;
}


/*
 *  Please discuss with CCMNI owner before adding extension tag!
 *  tag length should be controlled.
 */
static int _mddp_f_e_tag_packet(
	bool is_uplink,
	struct sk_buff *skb,
	unsigned char ip_ver,
	struct mddp_f_tag_packet_t *skb_tag,
	struct mddp_f_cb *cb,
	unsigned int hit_cnt)
{
	int etag_len;
	int ret = 0;

	/* Add Extension tag */
	etag_len = mddp_f_e_tag_packet(is_uplink, skb, ip_ver, cb, hit_cnt);

	if (etag_len < 0) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Add MDDP etag FAIL! Clean tag. etag_len[%d], is_uplink[%d], skb[%p], ip_ver[%d], skb_tag[%p]\n",
				__func__, etag_len, is_uplink,
				skb, ip_ver, skb_tag);

		memset(skb->head, 0, skb_tag->tag_len);
		skb_tag->tag_len = 0;
		ret = -EBADF;
	} else {
		skb_tag->tag_len += etag_len;
	}

	return ret;
}

static int mddp_f_tag_packet(
	bool is_uplink,
	struct sk_buff *skb,
	struct net_device *out,
	struct mddp_f_cb *cb,
	unsigned char protocol,
	unsigned char ip_ver,
	unsigned int hit_cnt)
{
	struct mddp_f_tag_packet_t *skb_tag;
	struct sk_buff *fake_skb;
	int ret = 0;


	if (is_uplink == true) { /* uplink*/
		skb_tag = (struct mddp_f_tag_packet_t *)skb->head;
		skb_tag->guard_pattern = MDDP_TAG_PATTERN;
		skb_tag->version = __MDDP_VERSION__;
		skb_tag->tag_len = sizeof(struct mddp_f_tag_packet_t);
		skb_tag->v2.tag_info = MDDP_RULE_TAG_NORMAL_PACKET;
		skb_tag->v2.lan_netif_id =
			mddp_f_dev_name_to_netif_id(cb->dev->name);
		skb_tag->v2.port = cb->sport;
		skb_tag->v2.ip = cb->src[0];  /* Don't care IPv6 IP */

		/* Add Extension tag */
		ret = _mddp_f_e_tag_packet(is_uplink, skb, ip_ver,
				skb_tag, cb, hit_cnt);

		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Add MDDP UL tag, guard_pattern[%x], version[%d], tag_len[%d], tag_info[%d], lan_netif_id[%x], port[%x], ip[%x], skb[%p].\n",
				__func__, skb_tag->guard_pattern,
				skb_tag->version,
				skb_tag->tag_len,
				skb_tag->v2.tag_info,
				skb_tag->v2.lan_netif_id,
				skb_tag->v2.port, skb_tag->v2.ip,
				skb);
	} else { /* downlink */
		if (mddp_f_is_support_lan_dev(cb->dev->name) == true) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Both in and out devices are lan devices. Do not tag the packet! out_device[%s], in_device[%s].\n",
					__func__,
					out->name, cb->dev->name);
			return -EFAULT;
		}

		fake_skb = skb_copy(skb, GFP_ATOMIC);
		fake_skb->dev = cb->dev;
		skb_tag = (struct mddp_f_tag_packet_t *)fake_skb->head;
		skb_tag->guard_pattern = MDDP_TAG_PATTERN;
		skb_tag->version = __MDDP_VERSION__;
		skb_tag->tag_len = sizeof(struct mddp_f_tag_packet_t);
		skb_tag->v2.tag_info = MDDP_RULE_TAG_FAKE_DL_NAT_PACKET;
		skb_tag->v2.lan_netif_id =
				mddp_f_dev_name_to_netif_id(out->name);
		skb_tag->v2.port = cb->dport;
		skb_tag->v2.ip = cb->dst[0];  /* Don't care IPv6 IP */

		/* Add Extension tag */
		ret = _mddp_f_e_tag_packet(is_uplink, fake_skb, ip_ver,
				skb_tag, cb, hit_cnt);

		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Add MDDP DL tag, guard_pattern[%x], version[%d], tag_len[%d], tag_info[%d], lan_netif_id[%x], port[%x], ip[%x], skb[%p], fake_skb[%p].\n",
				__func__, skb_tag->guard_pattern,
				skb_tag->version, skb_tag->tag_len,
				skb_tag->v2.tag_info,
				skb_tag->v2.lan_netif_id,
				skb_tag->v2.port, skb_tag->v2.ip,
				skb, fake_skb);

		dev_queue_xmit(fake_skb);
	}

	return ret;
}

static inline void _mddp_f_in_tail(
	u_int8_t iface,
	struct mddp_f_desc *desc,
	struct sk_buff *skb)
{
	struct mddp_f_cb *cb;
	struct ip4header *ip;
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
	} else if (desc->flag & DESC_FLAG_TRACK_ROUTER) {
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

static inline void _mddp_f_in_nat(
	unsigned char *offset2,
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	int iface)
{
	/* IPv4 */
	struct tuple t4;
	struct ip4header *ip;
	/* IPv6 */
	struct router_tuple t6;
	struct ip6header *ip6;
	__be16 ip6_frag_off;

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
	} else if (IPC_HDR_IS_V6(offset2)) {	/* only support ROUTE */
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

static inline int mddp_f_in_internal(int iface, struct sk_buff *skb)
{
	struct mddp_f_desc desc;

	/* HW */
	desc.flag = 0;
	desc.l3_off = 0;


	skb_set_network_header(skb, desc.l3_off);

	_mddp_f_in_nat(skb->data, &desc, skb, iface);
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

	_mddp_f_in_tail(iface, &desc, skb);

	return 0;		/* original path */
}

static int mddp_f_in_nf(int iface, struct sk_buff *skb)
{
	int ret;
	/* JQ: Remove lock */
	/* unsigned long flag; */

	/* MDDP_F_LOCK(&mddp_f_lock, flag); */
	ret = mddp_f_in_internal(iface, skb);
	/* MDDP_F_UNLOCK(&mddp_f_lock, flag); */
	return ret;
}

static void mddp_f_out_nf_ipv4(
	int iface,
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
			is_uplink = false;

			/* Do not tag TCP DL packet */
			MDDP_F_LOG(MDDP_LL_DEBUG,
					"%s: Do not tag IPv4 TCP DL.\n",
					__func__);

			goto out;
		}

		/* Tag this packet for MD tracking */
		if (skb_headroom(skb) > sizeof(struct mddp_f_tag_packet_t)) {
			MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
			found_nat_tuple =
				mddp_f_get_nat_tuple_ip4_tcpudp_wo_lock(&t);

			if (found_nat_tuple) {
				tuple_hit_cnt = found_nat_tuple->curr_cnt;
				found_nat_tuple->last_cnt = 0;
				found_nat_tuple->curr_cnt = 0;
				MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);

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
				MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);

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

			/* Save tuple to avoid tag many packets */
			if (!found_nat_tuple) {
				found_nat_tuple = kmem_cache_alloc(
						mddp_f_nat_tuple_cache,
						GFP_ATOMIC);
				found_nat_tuple->src_ip = cb->src[0];
				found_nat_tuple->dst_ip = cb->dst[0];
				found_nat_tuple->nat_src_ip = ip->ip_src;
				found_nat_tuple->nat_dst_ip = ip->ip_dst;
				found_nat_tuple->src.tcp.port = cb->sport;
				found_nat_tuple->dst.tcp.port = cb->dport;
				found_nat_tuple->nat_src.tcp.port =
					tcp->th_sport;
				found_nat_tuple->nat_dst.tcp.port =
					tcp->th_dport;
				found_nat_tuple->proto = ip->ip_p;
				found_nat_tuple->dev_src = cb->dev;
				found_nat_tuple->dev_dst = out;

				mddp_f_add_nat_tuple(found_nat_tuple);
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
				MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
				found_nat_tuple =
					mddp_f_get_nat_tuple_ip4_tcpudp_wo_lock(
							&t);

				if (found_nat_tuple) {
					tuple_hit_cnt =
						found_nat_tuple->curr_cnt;
					found_nat_tuple->last_cnt = 0;
					found_nat_tuple->curr_cnt = 0;
					MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock,
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
					MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock,
							flag);

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

				/* Save tuple to avoid tag many packets */
				found_nat_tuple = kmem_cache_alloc(
							mddp_f_nat_tuple_cache,
							GFP_ATOMIC);
				found_nat_tuple->src_ip = cb->src[0];
				found_nat_tuple->dst_ip = cb->dst[0];
				found_nat_tuple->nat_src_ip = ip->ip_src;
				found_nat_tuple->nat_dst_ip = ip->ip_dst;
				found_nat_tuple->src.udp.port = cb->sport;
				found_nat_tuple->dst.udp.port = cb->dport;
				found_nat_tuple->nat_src.udp.port =
					udp->uh_sport;
				found_nat_tuple->nat_dst.udp.port =
					udp->uh_dport;
				found_nat_tuple->proto = ip->ip_p;
				found_nat_tuple->dev_src = cb->dev;
				found_nat_tuple->dev_dst = out;

				mddp_f_add_nat_tuple(found_nat_tuple);
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

static void mddp_f_out_nf_ipv6(
	int iface,
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
			MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
			found_router_tuple =
				mddp_f_get_router_tuple_tcpudp_wo_lock(&t);

			if (found_router_tuple) {
				tuple_hit_cnt = found_router_tuple->curr_cnt;
				found_router_tuple->last_cnt = 0;
				found_router_tuple->curr_cnt = 0;
				MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);

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
				MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);

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
				MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
				found_router_tuple =
					mddp_f_get_router_tuple_tcpudp_wo_lock(
							&t);

				if (found_router_tuple) {
					tuple_hit_cnt =
						found_router_tuple->curr_cnt;
					found_router_tuple->last_cnt = 0;
					found_router_tuple->curr_cnt = 0;
					MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock,
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
					MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock,
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

static void mddp_f_out_nf(int iface, struct sk_buff *skb, struct net_device *out)
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
			mddp_f_out_nf_ipv4(iface, skb, out, offset2, cb,
							curr_track_table);
			return;

		} else {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Wrong IPv4 version[%d], offset[%p], flag[%x], skb[%p].\n",
					__func__, IPC_HDR_IS_V4(offset2),
					offset2, cb->flag, skb);
		}


	} else if (cb->flag & DESC_FLAG_TRACK_ROUTER) {
		int l3_off = 0;

		if (cb->flag & DESC_FLAG_IPV6) {
			mddp_f_out_nf_ipv6(iface, skb, out, cb, l3_off,
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

static uint32_t mddp_nfhook_prerouting
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

	mddp_f_in_nf(0, skb);
	return NF_ACCEPT;
}

static uint32_t mddp_nfhook_postrouting
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

	mddp_f_out_nf(0, skb, state->out);

out:
	return NF_ACCEPT;
}

static int32_t mddp_ct_update(void *buf, uint32_t buf_len)
{
	struct mddp_ct_timeout_ind_t   *ct_ind;
	struct mddp_ct_nat_table_t     *entry;
	uint32_t                        read_cnt = 0;
	uint32_t                        i;

	ct_ind = (struct mddp_ct_timeout_ind_t *)buf;
	read_cnt = sizeof(ct_ind->entry_num);

	for (i = 0; i < ct_ind->entry_num; i++) {
		entry = &(ct_ind->nat_table[i]);
		read_cnt += sizeof(struct mddp_ct_nat_table_t);
		if (read_cnt > buf_len) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Invalid buf_len(%u), i(%u), read_cnt(%u)!\n",
					__func__, buf_len, i, read_cnt);
			break;
		}

		MDDP_F_LOG(MDDP_LL_INFO,
				"%s: Update conntrack private(%u.%u.%u.%u:%u), target(%u.%u.%u.%u:%u), public(%u.%u.%u.%u:%u)\n",
				__func__,
				entry->private_ip[0], entry->private_ip[1],
				entry->private_ip[2], entry->private_ip[3],
				entry->private_port,
				entry->target_ip[0], entry->target_ip[1],
				entry->target_ip[2], entry->target_ip[3],
				entry->target_port,
				entry->public_ip[0], entry->public_ip[1],
				entry->public_ip[2], entry->public_ip[3],
				entry->public_port);

		// Send IND to upper module.
		mddp_dev_response(MDDP_APP_TYPE_ALL,
			MDDP_CMCMD_CT_IND,
			true,
			(uint8_t *)entry,
			sizeof(struct mddp_ct_nat_table_t));
	}

	return 0;
}

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
int32_t mddp_f_suspend_tag(void)
{
	struct mddp_md_msg_t           *md_msg;
	struct mddp_app_t              *app;

	MDDP_F_LOG(MDDP_LL_NOTICE, "%s: MDDP suspend tag.\n", __func__);
	mddp_f_suspend_s = 1;

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t), GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return 0;
	}

	md_msg->msg_id = IPC_MSG_ID_MDFPM_SUSPEND_TAG_ACK;
	md_msg->data_len = 0;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);
	mddp_ipc_send_md(app, md_msg, MDFPM_USER_ID_MDFPM);

	return 0;
}

int32_t mddp_f_resume_tag(void)
{
	struct mddp_md_msg_t           *md_msg;
	struct mddp_app_t              *app;

	MDDP_F_LOG(MDDP_LL_NOTICE, "%s: MDDP resume tag.\n", __func__);
	mddp_f_suspend_s = 0;

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t), GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return 0;
	}

	md_msg->msg_id = IPC_MSG_ID_MDFPM_RESUME_TAG_ACK;
	md_msg->data_len = 0;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);
	mddp_ipc_send_md(app, md_msg, MDFPM_USER_ID_MDFPM);

	return 0;
}

int32_t mddp_f_msg_hdlr(uint32_t msg_id, void *buf, uint32_t buf_len)
{
	int32_t                                 ret = 0;

	switch (msg_id) {
	case IPC_MSG_ID_DPFM_CT_TIMEOUT_IND:
		mddp_ct_update(buf, buf_len);
		ret = 0;
		break;

	case IPC_MSG_ID_DPFM_SET_CT_TIMEOUT_VALUE_RSP:
		ret = 0;
		break;

	default:
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Unaccepted msg_id(%d)!\n",
				__func__, msg_id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int32_t mddp_f_set_ct_value(uint8_t *buf, uint32_t buf_len)
{
	uint32_t                                md_status;
	struct mddp_md_msg_t                   *md_msg;
	struct mddp_dev_req_set_ct_value_t     *in_req;
	struct mddp_f_set_ct_timeout_req_t      ct_req;

	if (buf_len != sizeof(struct mddp_dev_req_set_ct_value_t)) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Invalid parameter, buf_len(%d)!\n",
				__func__, buf_len);
		return -EINVAL;
	}

	md_status = exec_ccci_kern_func_by_md_id(0, ID_GET_MD_STATE, NULL, 0);

	if (md_status != MD_STATE_READY) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Invalid state, md_status(%d)!\n",
				__func__, md_status);
		return -ENODEV;
	}

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + sizeof(ct_req),
			GFP_ATOMIC);
	if (unlikely(!md_msg))
		return -EAGAIN;

	in_req = (struct mddp_dev_req_set_ct_value_t *)buf;

	memset(&ct_req, 0, sizeof(ct_req));
	ct_req.tcp_ct_timeout = in_req->tcp_ct_timeout;
	ct_req.udp_ct_timeout = in_req->udp_ct_timeout;

	md_msg->msg_id = IPC_MSG_ID_DPFM_SET_CT_TIMEOUT_VALUE_REQ;
	md_msg->data_len = sizeof(ct_req);
	memcpy(md_msg->data, &ct_req, sizeof(ct_req));
	mddp_ipc_send_md(NULL, md_msg, MDFPM_USER_ID_DPFM);

	return 0;
}

//------------------------------------------------------------------------------
// Kernel functions.
//------------------------------------------------------------------------------
static int __net_init mddp_nf_register(struct net *net)
{
	return nf_register_net_hooks(net, mddp_nf_ops,
					ARRAY_SIZE(mddp_nf_ops));
}

static void __net_exit mddp_nf_unregister(struct net *net)
{
	nf_unregister_net_hooks(net, mddp_nf_ops,
					ARRAY_SIZE(mddp_nf_ops));
}

static struct pernet_operations mddp_net_ops = {
	.init = mddp_nf_register,
	.exit = mddp_nf_unregister,
};

int32_t mddp_filter_init(void)
{
	int ret = 0;

	ret = register_pernet_subsys(&mddp_net_ops);
	if (ret < 0) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Cannot register hooks(%d)!\n",
				__func__, ret);
		return ret;
	}

	MDDP_F_INIT_LOCK(&mddp_f_lock);
	MDDP_F_TUPLE_INIT_LOCK(&mddp_f_tuple_lock);

	mddp_f_init_table_buffer();
	mddp_f_init_track_table();
	mddp_f_init_jhash();

	ret = mddp_f_init_nat_tuple();
	if (ret < 0) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Cannot init nat tuple(%d)!\n",
				__func__, ret);
		return ret;
	}

	ret = mddp_f_init_router_tuple();
	if (ret < 0) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Cannot init router tuple(%d)!\n",
				__func__, ret);
		return ret;
	}

	mddp_f_nat_tuple_cache =
		kmem_cache_create("mddp_f_nat_tuple",
					sizeof(struct nat_tuple), 0,
					SLAB_HWCACHE_ALIGN, NULL);
	mddp_f_router_tuple_cache =
		kmem_cache_create("mddp_f_router_tuple",
					sizeof(struct router_tuple), 0,
					SLAB_HWCACHE_ALIGN, NULL);

	ret = mddp_f_notifier_init();
	if (ret < 0) {
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: mddp_f_notifier_init failed with ret = %d\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

void mddp_filter_uninit(void)
{
	unregister_pernet_subsys(&mddp_net_ops);
	mddp_f_notifier_dest();
	dest_track_table();
}

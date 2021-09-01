// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <net/arp.h>
#include <net/ip6_route.h>
#include <net/ip.h>
#include <net/ip6_checksum.h>
#include <net/ipv6.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/route.h>
#include <linux/in6.h>
#include <linux/timer.h>

#include "mddp_ctrl.h"
#include "mddp_debug.h"
#include "mddp_dev.h"
#include "mddp_filter.h"
#include "mddp_f_config.h"
#include "mddp_f_desc.h"
#include "mddp_f_dev.h"
#include "mddp_f_proto.h"
#include "mddp_ipc.h"
#include "mtk_ccci_common.h"


#define USED_TIMEOUT 1

#define TRACK_TABLE_INIT_LOCK(TABLE) \
		spin_lock_init((&(TABLE).lock))
#define TRACK_TABLE_LOCK(TABLE, flags) \
		spin_lock_irqsave((&(TABLE).lock), (flags))
#define TRACK_TABLE_UNLOCK(TABLE, flags) \
		spin_unlock_irqrestore((&(TABLE).lock), (flags))

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


static u32 mddp_f_jhash_initval __read_mostly;

static int mddp_f_contentfilter;
module_param(mddp_f_contentfilter, int, 0000);

static struct mddp_f_track_table_list_t mddp_f_track[MDDP_F_MAX_TRACK_NUM];
static struct mddp_f_track_table_list_t mddp_f_table_buffer;
static unsigned int buffer_cnt;

static uint32_t mddp_netfilter_is_hook;

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
static uint32_t mddp_nfhook_prerouting_v4
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
static uint32_t mddp_nfhook_prerouting_v6
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
static uint32_t mddp_nfhook_postrouting_v4
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
static uint32_t mddp_nfhook_postrouting_v6
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);

static int32_t mddp_f_init_nat_tuple(void);
static void mddp_f_uninit_nat_tuple(void);
static int32_t mddp_f_init_router_tuple(void);
static void mddp_f_uninit_router_tuple(void);
//------------------------------------------------------------------------------
// Registered callback function.
//------------------------------------------------------------------------------
static struct nf_hook_ops mddp_nf_ops[] __read_mostly = {
	{
		.hook           = mddp_nfhook_prerouting_v4,
		.pf             = NFPROTO_IPV4,
		.hooknum        = NF_INET_PRE_ROUTING,
		.priority       = NF_IP_PRI_FIRST + 1,
	},
	{
		.hook           = mddp_nfhook_postrouting_v4,
		.pf             = NFPROTO_IPV4,
		.hooknum        = NF_INET_POST_ROUTING,
		.priority       = NF_IP_PRI_LAST,
	},
	{
		.hook           = mddp_nfhook_prerouting_v6,
		.pf             = NFPROTO_IPV6,
		.hooknum        = NF_INET_PRE_ROUTING,
		.priority       = NF_IP6_PRI_FIRST + 1,
	},
	{
		.hook           = mddp_nfhook_postrouting_v6,
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
	struct mddp_f_cb *cb,
	unsigned char ip_ver,
	unsigned int hit_cnt)
{
	struct net_device *lan;
	struct mddp_f_tag_packet_t *skb_tag;
	struct mddp_f_e_tag_common_t *skb_e_tag;
	struct mddp_f_e_tag_mac_t e_tag_mac;
	struct neighbour *neigh = NULL;
	int tag_len, etag_len;


	if (is_uplink)
		lan = cb->dev;
	else
		lan = skb->dev;

	if (ip_ver == 4)
		neigh = neigh_lookup(&arp_tbl, &ip_hdr(skb)->daddr, lan);
	if (ip_ver == 6)
		neigh = neigh_lookup(&nd_tbl,  &ipv6_hdr(skb)->daddr, lan);

	if ((neigh == NULL) || !(neigh->nud_state & NUD_VALID)) {
		MDDP_F_LOG(MDDP_LL_WARN,
				"%s: Add MDDP Etag Fail, neigh[%x]\n",
				__func__, neigh);
		if (neigh)
			neigh_release(neigh);
		return -1;
	}

	neigh_ha_snapshot(e_tag_mac.mac_addr, neigh, neigh->dev);
	if (neigh)
		neigh_release(neigh);
	e_tag_mac.access_cnt = hit_cnt;

	tag_len = sizeof(struct mddp_f_tag_packet_t);
	etag_len = sizeof(struct mddp_f_e_tag_common_t) +
			sizeof(struct mddp_f_e_tag_mac_t);

	/* extension tag for MAC address */
	skb_e_tag = (struct mddp_f_e_tag_common_t *)
		(skb_tail_pointer(skb) + tag_len);
	skb_e_tag->type = MDDP_E_TAG_MAC;
	skb_e_tag->len  = etag_len;

	memcpy(skb_e_tag->value, &e_tag_mac, sizeof(struct mddp_f_e_tag_mac_t));

	MDDP_F_LOG(MDDP_LL_INFO,
			"%s: Add MDDP Etag,  mac[%02x:%02x:%02x:%02x:%02x:%02x], access_cnt[%d]\n",
			__func__, e_tag_mac.mac_addr[0], e_tag_mac.mac_addr[1],
			e_tag_mac.mac_addr[2], e_tag_mac.mac_addr[3],
			e_tag_mac.mac_addr[4], e_tag_mac.mac_addr[5],
			e_tag_mac.access_cnt);

	skb_tag = (struct mddp_f_tag_packet_t *) skb_tail_pointer(skb);
	skb_tag->guard_pattern = MDDP_TAG_PATTERN;
	skb_tag->version = __MDDP_VERSION__;
	skb_tag->tag_len = tag_len + etag_len;
	skb_tag->v2.tag_info = MDDP_RULE_TAG_FAKE_DL_NAT_PACKET;
	skb_tag->v2.lan_netif_id = mddp_f_dev_name_to_netif_id(lan->name);
	if (is_uplink == true) {
		skb_tag->v2.port = cb->sport;
		skb_tag->v2.ip = cb->src[0];  /* Don't care IPv6 IP */
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Add MDDP UL tag, lan_netif_id[%x], port[%x], ip[%x], skb[%p].\n",
				__func__, skb_tag->v2.lan_netif_id,
				skb_tag->v2.port, skb_tag->v2.ip, skb);
	} else {
		skb_tag->v2.port = cb->dport;
		skb_tag->v2.ip = cb->dst[0];  /* Don't care IPv6 IP */
		MDDP_F_LOG(MDDP_LL_NOTICE,
				"%s: Add MDDP DL tag, lan_netif_id[%x], port[%x], ip[%x], skb[%p].\n",
				__func__, skb_tag->v2.lan_netif_id,
				skb_tag->v2.port, skb_tag->v2.ip, skb);
	}
	__skb_put(skb, skb_tag->tag_len);

	mddp_enqueue_dstate(MDDP_DSTATE_ID_NEW_TAG,
				skb_tag->v2.ip, skb_tag->v2.port);

	return 0;
}

static struct sk_buff *mddp_f_skb_tag(struct sk_buff *skb, bool is_uplink, struct mddp_f_cb *cb,
				      unsigned char protocol, unsigned char ip_ver)
{
	uint16_t port;
	int len, ntail;
	struct net_device *dev;
	struct sk_buff *fake_skb;
	struct tcphdr *tcph;
	struct udphdr *udph;

	ntail = sizeof(struct mddp_f_tag_packet_t) +
		sizeof(struct mddp_f_e_tag_common_t) +
		sizeof(struct mddp_f_e_tag_mac_t);

	if (is_uplink)
		dev = skb->dev;
	else
		dev = cb->dev;

	fake_skb = netdev_alloc_skb(dev, dev->mtu);
	if (!fake_skb)
		return NULL;
	len = skb->len;
	if (skb->len > (dev->mtu - ntail))
		len = dev->mtu - ntail;
	__skb_put_data(fake_skb, skb_network_header(skb), len);
	skb_reset_mac_header(fake_skb);
	skb_reset_network_header(fake_skb);

	if (ip_ver == 4) {
		struct iphdr *iph;

		iph = ip_hdr(fake_skb);
		if (is_uplink) {
			iph->daddr = cb->src[0];
			cb->src[0] = iph->saddr;
			iph->saddr = cb->dst[0];
		}
		iph->tot_len = htons(fake_skb->len);
		iph->check = 0;
		len = iph->ihl * 4;
		skb_set_transport_header(fake_skb, len);

		if (protocol == IPPROTO_UDP) {
			udph = (void *)(skb_network_header(fake_skb) + len);
			if (is_uplink) {
				port = udph->dest;
				udph->dest = cb->sport;
				cb->sport = udph->source;
				udph->source = port;
			}
			udph->len = htons(fake_skb->len - len);
		}
		if (protocol == IPPROTO_TCP) {
			tcph = (void *)(skb_network_header(fake_skb) + len);
			if (is_uplink) {
				port = tcph->dest;
				tcph->dest = cb->sport;
				cb->sport = tcph->source;
				tcph->source = port;
			}
		}
	}
	if (ip_ver == 6) {
		struct in6_addr addr;
		struct ipv6hdr *iph;

		iph = ipv6_hdr(fake_skb);
		if (is_uplink) {
			addr = iph->saddr;
			iph->saddr = iph->daddr;
			iph->daddr = addr;
		}
		len = sizeof(struct ipv6hdr);
		iph->payload_len = htons(fake_skb->len - len);
		skb_set_transport_header(fake_skb, len);

		if (protocol == IPPROTO_UDP) {
			udph = (void *)(skb_network_header(fake_skb) + len);
			if (is_uplink) {
				port = udph->source;
				udph->source = udph->dest;
				udph->dest = port;
			}
			udph->len = htons(fake_skb->len - len);
		}

		if (protocol == IPPROTO_TCP) {
			tcph = (void *)(skb_network_header(fake_skb) + len);
			if (is_uplink) {
				port = tcph->source;
				tcph->source = tcph->dest;
				tcph->dest = port;
			}
		}
	}

	fake_skb->priority = skb->priority;
	fake_skb->protocol = skb->protocol;
	fake_skb->pkt_type = skb->pkt_type;

	return fake_skb;
}

static int mddp_f_tag_packet(
	bool is_uplink,
	struct sk_buff *skb,
	struct mddp_f_cb *cbp,
	unsigned char protocol,
	unsigned char ip_ver,
	unsigned int hit_cnt)
{
	struct sk_buff *fake_skb;
	struct mddp_f_cb cb;
	int ret;

	cb = *cbp;
	if (is_uplink != true) {
		if (mddp_f_is_support_lan_dev(cb.dev->name) == true) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Both in and out devices are lan devices. Do not tag the packet! out_device[%s], in_device[%s].\n",
					__func__,
					skb->dev->name, cb.dev->name);
			return -EFAULT;
		}
	}

	fake_skb = mddp_f_skb_tag(skb, is_uplink, &cb, protocol, ip_ver);
	if (fake_skb == NULL) {
		MDDP_F_LOG(MDDP_LL_NOTICE, "%s: skb_copy() failed\n", __func__);
		return -ENOMEM;
	}

	/* Add Extension tag */
	ret = mddp_f_e_tag_packet(is_uplink, fake_skb, &cb, ip_ver, hit_cnt);
	if (ret < 0) {
		dev_kfree_skb(fake_skb);
		return -EFAULT;
	}

	fake_skb->dev->netdev_ops->ndo_start_xmit(fake_skb, fake_skb->dev);

	return 0;
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
	mddp_netfilter_unhook();

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

	mddp_enqueue_dstate(MDDP_DSTATE_ID_SUSPEND_TAG);

	return 0;
}

int32_t mddp_f_resume_tag(void)
{
	struct mddp_md_msg_t           *md_msg;
	struct mddp_app_t              *app;

	MDDP_F_LOG(MDDP_LL_NOTICE, "%s: MDDP resume tag.\n", __func__);
	mddp_netfilter_hook();

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

	mddp_enqueue_dstate(MDDP_DSTATE_ID_RESUME_TAG);

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


void mddp_netfilter_hook(void)
{
	if (mddp_netfilter_is_hook == 0) {
		int ret = 0;

		ret = register_pernet_subsys(&mddp_net_ops);
		if (ret < 0) {
			MDDP_F_LOG(MDDP_LL_NOTICE,
					"%s: Cannot register hooks(%d)!\n",
					__func__, ret);
		} else {
			mddp_netfilter_is_hook = 1;
		}
	}
}

void mddp_netfilter_unhook(void)
{
	if (mddp_netfilter_is_hook == 1) {
		unregister_pernet_subsys(&mddp_net_ops);
		mddp_netfilter_is_hook = 0;
	}
}

int32_t mddp_filter_init(void)
{
	int ret = 0;

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

	return 0;
}

static atomic_t mddp_filter_quit = ATOMIC_INIT(0);
void mddp_filter_uninit(void)
{
	mddp_netfilter_unhook();
	atomic_set(&mddp_filter_quit, 1);
	mddp_f_uninit_nat_tuple();
	mddp_f_uninit_router_tuple();
	dest_track_table();
}

#include "mddp_filter_v4.c"
#include "mddp_filter_v6.c"

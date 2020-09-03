/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MDDP_TRACK_H
#define _MDDP_TRACK_H


#include <linux/slab.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/nf_conntrack_tcp.h>
#include <linux/netfilter/nf_conntrack_proto_gre.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack_core.h>
#include "mddp_f_config.h"
#include "mddp_f_tuple.h"

extern int mddp_f_max_nat;
extern int mddp_f_max_router;

extern struct kmem_cache *mddp_f_nat_tuple_cache;
extern struct kmem_cache *mddp_f_router_tuple_cache;

extern spinlock_t mddp_f_lock;
#define MDDP_F_INIT_LOCK(LOCK) spin_lock_init((LOCK))
#define MDDP_F_LOCK(LOCK, FLAG) spin_lock_irqsave((LOCK), (FLAG))
#define MDDP_F_UNLOCK(LOCK, FLAG) spin_unlock_irqrestore((LOCK), (FLAG))

extern spinlock_t mddp_f_tuple_lock;
#define MDDP_F_TUPLE_INIT_LOCK(LOCK) spin_lock_init((LOCK))
#define MDDP_F_TUPLE_LOCK(LOCK, FLAG) spin_lock_irqsave((LOCK), (FLAG))
#define MDDP_F_TUPLE_UNLOCK(LOCK, FLAG) spin_unlock_irqrestore((LOCK), (FLAG))

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

void del_all_track_table(void);

static inline void ipv6_addr_copy(
	struct in6_addr *a1,
	const struct in6_addr *a2)
{
	memcpy(a1, a2, sizeof(struct in6_addr));
}

#endif /* _MDDP_TRACK_H */

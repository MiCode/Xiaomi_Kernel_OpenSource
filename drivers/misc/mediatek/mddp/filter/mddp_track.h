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
#include <linux/if_vlan.h>
#include "mddp_f_config.h"
#include "mddp_f_tuple.h"

extern int mddp_f_max_nat;
extern int mddp_f_max_router;

extern struct kmem_cache *mddp_f_nat_tuple_cache;
extern struct kmem_cache *mddp_f_router_tuple_cache;

#ifdef PW_RESET
void pm_reset_traffic(void);
#else
#define pm_reset_traffic()
#endif

extern spinlock_t mddp_f_lock;
#define MDDP_F_INIT_LOCK(LOCK) spin_lock_init((LOCK))
#define MDDP_F_LOCK(LOCK, FLAG) spin_lock_irqsave((LOCK), (FLAG))
#define MDDP_F_UNLOCK(LOCK, FLAG) spin_unlock_irqrestore((LOCK), (FLAG))

#ifdef CONFIG_PREEMPT_RT_FULL
extern raw_spinlock_t mddp_f_tuple_lock;
#define MDDP_F_TUPLE_INIT_LOCK(LOCK) raw_spin_lock_init((LOCK))
#define MDDP_F_TUPLE_LOCK(LOCK, FLAG) raw_spin_lock_irqsave((LOCK), (FLAG))
#define MDDP_F_TUPLE_UNLOCK(LOCK, FLAG) \
		raw_spin_unlock_irqrestore((LOCK), (FLAG))
#else
extern spinlock_t mddp_f_tuple_lock;
#define MDDP_F_TUPLE_INIT_LOCK(LOCK) spin_lock_init((LOCK))
#define MDDP_F_TUPLE_LOCK(LOCK, FLAG) spin_lock_irqsave((LOCK), (FLAG))
#define MDDP_F_TUPLE_UNLOCK(LOCK, FLAG) spin_unlock_irqrestore((LOCK), (FLAG))
#endif

#ifdef MDDP_F_NO_KERNEL_SUPPORT
#ifdef CONFIG_PREEMPT_RT_FULL
#define TRACK_TABLE_INIT_LOCK(TABLE) \
		raw_spin_lock_init((&(TABLE).lock))
#define TRACK_TABLE_LOCK(TABLE, flags) \
		raw_spin_lock_irqsave((&(TABLE).lock), (flags))
#define TRACK_TABLE_UNLOCK(TABLE, flags) \
		raw_spin_unlock_irqrestore((&(TABLE).lock), (flags))
#else
#define TRACK_TABLE_INIT_LOCK(TABLE) \
		spin_lock_init((&(TABLE).lock))
#define TRACK_TABLE_LOCK(TABLE, flags) \
		spin_lock_irqsave((&(TABLE).lock), (flags))
#define TRACK_TABLE_UNLOCK(TABLE, flags) \
		spin_unlock_irqrestore((&(TABLE).lock), (flags))
#endif
#endif

#define INTERFACE_TYPE_LAN	0
#define INTERFACE_TYPE_WAN	1
#define INTERFACE_TYPE_IOC	2

enum mddp_f_rule_tag_info_e {
	MDDP_RULE_TAG_NORMAL_PACKET = 0,
	MDDP_RULE_TAG_FAKE_DL_NAT_PACKET,
};

struct interface {
	int type;
	int ready;
	struct net_device *dev;
	unsigned int global_count;
	unsigned int syn_count;
	unsigned int udp_count;
	unsigned int icmp_count;
	unsigned int icmp_unreach_count;
	unsigned int is_bridge;
#ifdef ENABLE_PORTBASE_QOS
	char dscp_mark;
#endif
};

extern struct interface ifaces[];

struct mddp_f_cb {
	u_int32_t flag;
	u_int32_t src[4];	/* IPv4 use src[0] */
	u_int32_t dst[4];	/* IPv4 use dst[0] */
	u_int16_t sport;
	u_int16_t dport;
	u_int8_t proto;
#ifndef MDDP_F_NETFILTER
	u_int8_t iface;
#else
	struct net_device *dev;
#endif
	u_int8_t vlevel;
#ifndef MDDP_F_NETFILTER
	u_int8_t rfu1[1];
#endif
	u_int16_t vlan[MAX_VLAN_LEVEL];
	u_int8_t mac[6];
#ifndef MDDP_F_NETFILTER
	u_int8_t rfu2[2];
#endif
	u_int32_t xfrm_dst_ptr;
	u_int32_t ipmac_ptr;
	u_int16_t v4_ip_id;
};

#ifdef MDDP_F_NO_KERNEL_SUPPORT
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
#ifdef CONFIG_PREEMPT_RT_FULL
	raw_spinlock_t lock;
#else
	spinlock_t lock;
#endif
};

void del_all_track_table(void);
#endif

static inline void ipv6_addr_copy(
	struct in6_addr *a1,
	const struct in6_addr *a2)
{
	memcpy(a1, a2, sizeof(struct in6_addr));
}

#endif /* _MDDP_TRACK_H */

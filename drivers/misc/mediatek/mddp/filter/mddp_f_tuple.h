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

#ifndef _MDDP_F_TUPLE_H
#define _MDDP_F_TUPLE_H

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include "mddp_f_config.h"

#define MAX_VLAN_LEVEL	2
#define USED_TIMEOUT	(1/2) /* 500ms */

/* Have to be power of 2 */
#define NAT_TUPLE_HASH_SIZE		(MD_DIRECT_TETHERING_RULE_NUM)
#define ROUTER_TUPLE_HASH_SIZE	(MD_DIRECT_TETHERING_RULE_NUM)


struct tuple {
	union {
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
		struct {
			union {
				struct {
					u_int8_t dst_unused[2];
					u_int8_t dst_mac[6];
				} __attribute__ ((__packed__));
				struct {
					u_int16_t dst_unused2;
					u_int16_t dst_mac1;
					u_int32_t dst_mac2;
				};
			};
			u_int8_t vlevel_src;
			u_int16_t vlan_src[MAX_VLAN_LEVEL];
		} bridge;
	};
#ifndef MDDP_F_NETFILTER
	u_int8_t iface_in;
#else
	struct net_device *dev_in;
#endif
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

#ifndef MDDP_F_NETFILTER
	u_int8_t iface_src;
	u_int8_t iface_dst;
#else
	struct net_device *dev_src;
	struct net_device *dev_dst;
#endif

	struct timer_list timeout_used;
};

struct router_tuple {
	struct list_head list;

	struct in6_addr saddr;
	struct in6_addr daddr;

#ifndef MDDP_F_NETFILTER
	u_int8_t iface_src;
	u_int8_t iface_dst;
#else
	struct net_device *dev_src;
	struct net_device *dev_dst;
#endif

	struct timer_list timeout_used;

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

#define HASH_TUPLE_TCPUDP(t) \
	(jhash_3words((t)->nat.src, ((t)->nat.dst ^ (t)->nat.proto), \
	((t)->nat.s.all | ((t)->nat.d.all << 16)), \
	nat_tuple_hash_rnd) & (NAT_TUPLE_HASH_SIZE - 1))

#define HASH_NAT_TUPLE_TCPUDP(t) \
	(jhash_3words((t)->src_ip, ((t)->dst_ip ^ (t)->proto), \
	((t)->src.all | ((t)->dst.all << 16)), \
	nat_tuple_hash_rnd) & (NAT_TUPLE_HASH_SIZE - 1))

#define HASH_ROUTER_TUPLE_TCPUDP(t) \
	(jhash_3words(((t)->saddr.s6_addr32[0] ^ (t)->saddr.s6_addr32[3]), \
	((t)->daddr.s6_addr32[0] ^ (t)->daddr.s6_addr32[3]), \
	((t)->in.all | ((t)->out.all << 16)), \
	router_tuple_hash_rnd) & (ROUTER_TUPLE_HASH_SIZE - 1))

#define HASH_ROUTER_TUPLE(t) \
	(jhash_3words((t)->saddr.s6_addr32[2], \
	(t)->daddr.s6_addr32[2], (t)->daddr.s6_addr32[3], \
	router_tuple_hash_rnd) & (ROUTER_TUPLE_HASH_SIZE - 1))

void mddp_f_init_nat_tuple(void);
bool mddp_f_add_nat_tuple(struct nat_tuple *t);
void mddp_f_del_nat_tuple(struct nat_tuple *t);

void mddp_f_init_router_tuple(void);
bool mddp_f_add_router_tuple_tcpudp(struct router_tuple *t);
void mddp_f_del_router_tuple(struct router_tuple *t);

#endif /* _MDDP_F_TUPLE_H */

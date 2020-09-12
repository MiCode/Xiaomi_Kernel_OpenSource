/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MDDP_F_TUPLE_H
#define _MDDP_F_TUPLE_H

#include <linux/in6.h>
#include <linux/timer.h>

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

#endif /* _MDDP_F_TUPLE_H */

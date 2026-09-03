/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __URSP_H__
#define __URSP_H__

/* Includes */
#include <linux/soc/sprd/ursp.h>
#include <linux/types.h>

#include "ursp_hash.h"

#define URSP_OK		0x00
#define URSP_FAIL	0x01
#define URSP_RST_FLAG	0x03

enum {
	IP_L4_PROTO_TCP		= 6,
	IP_L4_PROTO_UDP		= 17,
};

enum ursp_tcp_flag_set {
	TCP_SYN_SET,
	TCP_SYNACK_SET,
	TCP_FIN_SET,
	TCP_ACK_SET,
	TCP_RST_SET,
	TCP_NONE_SET,
};

struct ursp_conn {
	struct nf_conntrack_tuple tuple;
	u32 hash;
	u32 fin_rst_flag;
	u32 tcp_sure_flag;
	u8 out_ifindex;
	u32 uid;
};

struct ursp_fwd_entry {
	struct hlist_node entry_lst;
	struct rcu_head		rcu;
	struct ursp_conn *ursp_ct;
	struct timer_list timeout;
};

struct urspfwd_iter_state {
	int bucket;
	int count;
};

#define FP_DEBUG
#define FP_PRT_WARN		0x2
#define FP_PRT_DEBUG	0x4
#define FP_PRT_ALL		0x3

#ifdef FP_DEBUG
extern unsigned int fp_dbg;
#define URSP_LOG_TAG  "URSP"
#define FP_PRT_DBG(FLG, fmt, arg...) {\
	if (fp_dbg & (FLG))\
		pr_info("URSP:" fmt, ##arg);\
	}
#else
#define FP_PRT_DBG(FLG, fmt, arg...)
#endif

extern struct hlist_head ursp_fwd_entries[URSP_ENTRIES_HASH_SIZE];
extern struct net init_net;
extern spinlock_t ursp_lock;
extern int sysctl_net_ursp_enable;

static inline void FP_PRT_TUPLE_INFO(int dbg_lvl,
				     const struct nf_conntrack_tuple *tuple)
{
	if (tuple->src.l3num == NFPROTO_IPV4) {
		u32 dst = ntohl(tuple->dst.u3.ip);

		FP_PRT_DBG(dbg_lvl, "ip %pI4 - proto [%u] - port [%u]\n",
			   &dst, tuple->dst.protonum, ntohs(tuple->dst.u.all));
	} else {
		struct in6_addr dst = tuple->dst.u3.in6;

		FP_PRT_DBG(dbg_lvl, "ipv6 %pI6 - proto [%u] - port [%u]\n",
			   &dst, tuple->dst.protonum, ntohs(tuple->dst.u.all));
	}
}

static inline bool ursp_tcp_rst_chk(struct nf_conn *ct)
{
	return (ct->proto.tcp.last_index == TCP_RST_SET) ? true : false;
}

static inline bool ursp_tcp_fin_chk(struct nf_conn *ct)
{
	return (ct->proto.tcp.last_index == TCP_FIN_SET) ? true : false;
}

static inline bool ursp_tcp_flag_chk(struct nf_conn *ct)
{
	return (ct->proto.tcp.last_index == TCP_RST_SET ||
		ct->proto.tcp.last_index == TCP_FIN_SET) ?
		true : false;
}

static inline bool ursp_ct_tuple_equal(struct ursp_fwd_entry *t1,
				       const struct nf_conntrack_tuple *t2)
{
	return __nf_ct_tuple_dst_equal(&t1->ursp_ct->tuple, t2);
}

int nf_ursp_conntrack_init(void);
int add_in_ursp_fwd_table(struct ursp_conn *ursp_ct);
int delete_in_ursp_fwd_table(struct ursp_conn *ursp_ct);
void ursp_fwd_entry_free(struct rcu_head *head);
int ursp_fwd_entry_timer_init(struct ursp_fwd_entry *new_entry);
int get_ursp_enable(void);
int ursp_proc_create(void);

#endif

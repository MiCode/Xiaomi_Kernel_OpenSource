// SPDX-License-Identifier: GPL-2.0-only
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
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/sysctl.h>
#include <net/route.h>
#include <net/ip.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>

#include "ursp.h"
#include "ursp_hash.h"

extern spinlock_t ursp_lock;

void ursp_conntrack_in(struct net *net, u_int8_t pf,
		       unsigned int hooknum,
		       struct sk_buff *skb)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	enum ip_conntrack_dir dir;
	struct nf_conntrack_tuple *tuple;
	u8 l4proto;
	struct ursp_fwd_entry *curr_fwd_entry;
	struct ursp_conn *ursp_ct;
	u32 hash;

	if (!ct)
		return;

	dir = CTINFO2DIR(ctinfo);
	tuple = &ct->tuplehash[dir].tuple;
	l4proto = tuple->dst.protonum;

	if (l4proto != IP_L4_PROTO_TCP)
		return;

	if (!ursp_tcp_flag_chk(ct))
		return;

	FP_PRT_DBG(FP_PRT_DEBUG, "ursp_hook: fin or rst.\n");

	/* check whether is created */
	hash = ursp_hash_conntrack(tuple);
	rcu_read_lock_bh();
	hlist_for_each_entry_rcu(curr_fwd_entry,
				 &ursp_fwd_entries[hash],
				 entry_lst) {
		if (ursp_ct_tuple_equal(curr_fwd_entry, tuple)) {
			ursp_ct = curr_fwd_entry->ursp_ct;
			spin_lock_bh(&ursp_lock);
			if (ursp_tcp_fin_chk(ct) &&
			    ursp_ct->fin_rst_flag == 0) {
				FP_PRT_DBG(FP_PRT_DEBUG,
					   "fin, 2MSL start %p\n", ursp_ct);
				mod_timer(&curr_fwd_entry->timeout,
					  jiffies + URSP_TCP_TIME_WAIT);
				ursp_ct->fin_rst_flag++;
			} else if (ursp_tcp_rst_chk(ct) &&
				   ursp_ct->fin_rst_flag < URSP_RST_FLAG) {
				FP_PRT_DBG(FP_PRT_DEBUG,
					   "rst, will del 10s later %p\n",
					   ursp_ct);
				mod_timer(&curr_fwd_entry->timeout,
					  jiffies + URSP_TCP_CLOSE_TIME);
				ursp_ct->fin_rst_flag += URSP_RST_FLAG;
			}
			spin_unlock_bh(&ursp_lock);
			rcu_read_unlock_bh();
			return;
		}
	}
	rcu_read_unlock_bh();
}

unsigned int nf_v4_ursp_conntrack_in(struct net *net, u_int8_t pf,
				     unsigned int hooknum,
				     struct sk_buff *skb)
{
	ursp_conntrack_in(net, pf, hooknum, skb);
	return NF_ACCEPT;
}

static unsigned int ipv4_ursp_conntrack_in(void *priv,
					   struct sk_buff *skb,
					   const struct nf_hook_state *state)
{
	return nf_v4_ursp_conntrack_in(state->net, PF_INET, state->hook, skb);
}

static unsigned int
ipv4_ursp_conntrack_postrouting(void *priv, struct sk_buff *skb,
				const struct nf_hook_state *state)
{
	if (sysctl_net_ursp_enable == 1)
		ursp_filter_fwd_create_entries(PF_INET, skb, state);
	return NF_ACCEPT;
}

unsigned int nf_v6_ursp_conntrack_in(struct net *net, u_int8_t pf,
				     unsigned int hooknum,
				     struct sk_buff *skb)
{
	ursp_conntrack_in(net, pf, hooknum, skb);
	return NF_ACCEPT;
}

static unsigned int ipv6_ursp_conntrack_in(void *priv,
					   struct sk_buff *skb,
					   const struct nf_hook_state *state)
{
	return nf_v6_ursp_conntrack_in(state->net, PF_INET6, state->hook, skb);
}

static unsigned int
ipv6_ursp_conntrack_postrouting(void *priv, struct sk_buff *skb,
				const struct nf_hook_state *state)
{
	if (sysctl_net_ursp_enable == 1)
		ursp_filter_fwd_create_entries(PF_INET6, skb, state);
	return NF_ACCEPT;
}

static struct nf_hook_ops ipv4_ursp_conntrack_ops[] __read_mostly = {
	{
		.hook		= ipv4_ursp_conntrack_in,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_POST_ROUTING,
		.priority	= NF_IP_PRI_CONNTRACK - 1,
	},
	{
		.hook		= ipv4_ursp_conntrack_postrouting,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_POST_ROUTING,
		.priority	= NF_IP_PRI_CONNTRACK_CONFIRM - 1,
	},
	{
		.hook		= ipv4_ursp_conntrack_in,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority	= NF_IP_PRI_CONNTRACK_CONFIRM - 1,
	}
};

static struct nf_hook_ops ipv6_ursp_conntrack_ops[] __read_mostly = {
	{
		.hook		= ipv6_ursp_conntrack_in,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_POST_ROUTING,
		.priority	= NF_IP6_PRI_CONNTRACK - 1,
	},
	{
		.hook		= ipv6_ursp_conntrack_postrouting,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_POST_ROUTING,
		.priority	= NF_IP_PRI_CONNTRACK_CONFIRM - 1,
	},
	{
		.hook		= ipv6_ursp_conntrack_in,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority	= NF_IP_PRI_CONNTRACK_CONFIRM - 1,
	}
};

int __init nf_ursp_conntrack_init(void)
{
	int ret;

	ret = nf_register_net_hooks(&init_net, ipv4_ursp_conntrack_ops,
				    ARRAY_SIZE(ipv4_ursp_conntrack_ops));
	if (ret < 0) {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "v4 can't register hooks.\n");
		return ret;
	}
	ret = nf_register_net_hooks(&init_net, ipv6_ursp_conntrack_ops,
				    ARRAY_SIZE(ipv6_ursp_conntrack_ops));
	if (ret < 0) {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "v6 can't register hooks.\n");
		return ret;
	}
	return ret;
}


// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#include "sfp.h"
#include "sfp_hash.h"

extern spinlock_t mgr_lock;
static void sfp_conntrack_in(struct net *net, u_int8_t pf,
			     unsigned int hooknum,
			     struct sk_buff *skb)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	enum ip_conntrack_dir dir;
	struct nf_conntrack_tuple *tuple;
	u8  l4proto;
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;
	struct sfp_conn *sfp_ct;
	u32 hash;

	/* packets behind rst may not have a ct */
	if (!ct)
		return;

	dir = CTINFO2DIR(ctinfo);
	tuple = &ct->tuplehash[dir].tuple;
	l4proto = tuple->dst.protonum;

	if (l4proto != IP_L4_PROTO_TCP)
		return;

	if (!sfp_tcp_flag_chk(ct))
		return;

	FP_PRT_DBG(FP_PRT_DEBUG, "sfp_hook: fin or rst.\n");

	/* Check whether is created */
	hash = sfp_hash_conntrack(tuple);
	rcu_read_lock_bh();
	hlist_for_each_entry_rcu(tuple_hash,
				 &mgr_fwd_entries[hash],
				 entry_lst) {
		if (sfp_ct_tuple_equal(tuple_hash, tuple)) {
			sfp_ct = sfp_ct_tuplehash_to_ctrack(tuple_hash);
			spin_lock_bh(&mgr_lock);
			if (sfp_tcp_fin_chk(ct) &&
			    sfp_ct->fin_rst_flag == 0) {
				FP_PRT_DBG(FP_PRT_DEBUG,
					   "fin, 2MSL start %p\n",
					   sfp_ct);

				mod_timer(&sfp_ct->timeout,
					  jiffies + SFP_TCP_TIME_WAIT);
				sfp_ct->fin_rst_flag++;
			} else if (sfp_tcp_rst_chk(ct) &&
				sfp_ct->fin_rst_flag < SFP_RST_FLAG) {
				FP_PRT_DBG(FP_PRT_DEBUG,
					   "rst, will delete 10s later %p\n",
					   sfp_ct);

				mod_timer(&sfp_ct->timeout,
					  jiffies + SFP_TCP_CT_WAITING);
				sfp_ct->fin_rst_flag += SFP_RST_FLAG;
			}
			spin_unlock_bh(&mgr_lock);
			rcu_read_unlock_bh();
			return;
		}
	}
	rcu_read_unlock_bh();
}

static unsigned int nf_soft_fastpath_process(struct sk_buff *skb)
{
	int out_index = 0;
	int ret = 0;
	int err = 0;
	struct net_device *dev;

	ret = soft_fastpath_process(SFP_INTERFACE_LTE,
				    (void *)skb,
					NULL, NULL, &out_index);
	if (!ret) {
		dev = netdev_get_by_index(out_index);
		if (!dev) {
			pr_err("fail to get dev, out idx %d\n", out_index);
			dev_kfree_skb_any(skb);
			return NF_STOLEN;
		}

		/* update skb dev */
		skb->dev = dev;

		err = dev_queue_xmit(skb);
		if (err)
			pr_warn("fast xmit fail, out idx %d, err %x\n",
					out_index, err);
		dev_put(dev);
		return NF_STOLEN;
	}
	return NF_ACCEPT;
}

static unsigned int ipv4_sfp_pre_routing(void *priv,
					 struct sk_buff *skb,
					 const struct nf_hook_state *state)
{
	if (sysctl_net_sfp_enable == 1 &&
	    (get_sfp_tether_scheme() == SFP_SOFT_PATH ||
	     get_sfp_tether_scheme() == SFP_HALF_PATH))
		return nf_soft_fastpath_process(skb);
	return NF_ACCEPT;
}

static unsigned int ipv6_sfp_pre_routing(void *priv,
					 struct sk_buff *skb,
					 const struct nf_hook_state *state)
{

	if (sysctl_net_sfp_enable == 1 &&
	    (get_sfp_tether_scheme() == SFP_SOFT_PATH ||
	     get_sfp_tether_scheme() == SFP_HALF_PATH))
		return nf_soft_fastpath_process(skb);
	return NF_ACCEPT;
}

static unsigned int nf_v4_sfp_conntrack_in(struct net *net, u_int8_t pf,
					   unsigned int hooknum,
					   struct sk_buff *skb)
{
	sfp_conntrack_in(net, pf, hooknum, skb);
	return NF_ACCEPT;
}

static unsigned int ipv4_sfp_conntrack_in(void *priv,
					  struct sk_buff *skb,
					  const struct nf_hook_state *state)
{
	return nf_v4_sfp_conntrack_in(state->net, PF_INET, state->hook, skb);
}

static unsigned int
ipv4_sfp_conntrack_forward(void *priv,
			   struct sk_buff *skb,
			   const struct nf_hook_state *state)
{
	sfp_filter_mgr_fwd_create_entries(PF_INET, skb);
	return NF_ACCEPT;
}

static unsigned int nf_v6_sfp_conntrack_in(struct net *net, u_int8_t pf,
					   unsigned int hooknum,
					   struct sk_buff *skb)
{
	sfp_conntrack_in(net, pf, hooknum, skb);
	return NF_ACCEPT;
}

static unsigned int ipv6_sfp_conntrack_in(void *priv,
					  struct sk_buff *skb,
					  const struct nf_hook_state *state)
{
	return nf_v6_sfp_conntrack_in(state->net, PF_INET6, state->hook, skb);
}

static unsigned int
ipv6_sfp_conntrack_forward(void *priv,
			   struct sk_buff *skb,
			   const struct nf_hook_state *state)
{
	sfp_filter_mgr_fwd_create_entries(PF_INET6, skb);
	return NF_ACCEPT;
}

static struct nf_hook_ops ipv4_sfp_conntrack_ops[] __read_mostly = {
	{
		.hook		= ipv4_sfp_conntrack_in,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_FORWARD,
		.priority	= NF_IP_PRI_CONNTRACK - 1,
	},
	{
		.hook		= ipv4_sfp_conntrack_forward,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_FORWARD,
		.priority	= NF_IP_PRI_CONNTRACK_CONFIRM - 1,
	},
	{
		.hook		= ipv4_sfp_pre_routing,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority	= NF_IP_PRI_MANGLE - 1,
	},
};

static struct nf_hook_ops ipv6_sfp_conntrack_ops[] __read_mostly = {
	{
		.hook		= ipv6_sfp_conntrack_in,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_FORWARD,
		.priority	= NF_IP6_PRI_CONNTRACK - 1,
	},
	{
		.hook		= ipv6_sfp_conntrack_forward,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_FORWARD,
		.priority	= NF_IP_PRI_CONNTRACK_CONFIRM - 1,
	},
	{
		.hook           = ipv6_sfp_pre_routing,
		.pf             = NFPROTO_IPV6,
		.hooknum        = NF_INET_PRE_ROUTING,
		.priority       = NF_IP6_PRI_MANGLE - 1,
	},
};

int __init nf_sfp_conntrack_init(void)
{
	int ret;

	ret = nf_register_net_hooks(&init_net, ipv4_sfp_conntrack_ops,
				    ARRAY_SIZE(ipv4_sfp_conntrack_ops));
	if (ret < 0) {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "v4 can't register hooks.\n");
		return ret;
	}
	ret = nf_register_net_hooks(&init_net, ipv6_sfp_conntrack_ops,
				    ARRAY_SIZE(ipv6_sfp_conntrack_ops));
	if (ret < 0) {
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "v6 can't register hooks.\n");
		nf_unregister_net_hooks(&init_net, ipv4_sfp_conntrack_ops,
				    ARRAY_SIZE(ipv4_sfp_conntrack_ops));
		return ret;
	}
	return ret;
}

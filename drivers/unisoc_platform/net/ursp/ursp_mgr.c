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

#include <linux/netfilter/nf_conntrack_common.h>
#include <net/net_namespace.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/route.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/module.h>

#include "ursp.h"
#include "ursp_hash.h"
#include "ursp_netlink.h"

spinlock_t ursp_lock;/* Spinlock for ursp*/

unsigned int fp_dbg = FP_PRT_ALL;

static const char * const ursp_netdev[] = {"seth_lte", "sipa_eth", NULL};

static void ursp_fwd_entry_death_by_timeout(struct timer_list *t)
{
	struct ursp_fwd_entry *ursp_fwd_entry;

	ursp_fwd_entry = from_timer(ursp_fwd_entry, t, timeout);

	if (!ursp_fwd_entry) {
		FP_PRT_DBG(FP_PRT_DEBUG, "URSP:ursp_entry was free when time out.\n");
		return;
	}

	FP_PRT_DBG(FP_PRT_DEBUG, "URSP:<<check death by timeout(%p)>>.\n", ursp_fwd_entry);

	spin_lock_bh(&ursp_lock);
	delete_in_ursp_fwd_table(ursp_fwd_entry->ursp_ct);
	spin_unlock_bh(&ursp_lock);
}

int ursp_fwd_entry_timer_init(struct ursp_fwd_entry *new_entry)
{
	struct nf_conntrack_tuple tuple;

	tuple = new_entry->ursp_ct->tuple;

	rcu_read_lock_bh();

	timer_setup(&new_entry->timeout,
		    ursp_fwd_entry_death_by_timeout, 0);

	if (tuple.dst.protonum == IP_L4_PROTO_TCP)
		new_entry->timeout.expires = jiffies + DEFAULT_URSP_TCP_AGING_TIME;
	else if (tuple.dst.protonum == IP_L4_PROTO_UDP)
		new_entry->timeout.expires = jiffies + DEFAULT_URSP_UDP_AGING_TIME;

	add_timer(&new_entry->timeout);

	rcu_read_unlock_bh();

	return 0;
}

int ursp_check_netdevice_change(struct net_device *dev)
{
	int i = 0;
	int ifindex = 0;

	dev_hold(dev);
	while (ursp_netdev[i]) {
		if ((dev->name[0] != '\0') &&
		    (strncasecmp(dev->name, ursp_netdev[i],
		     strlen(ursp_netdev[i])) == 0)) {
			ifindex = dev->ifindex;
			break;
		}
		i++;
	}
	dev_put(dev);
	return ifindex;
}

static int create_ursp_fwd_entries(u8 pf, struct sk_buff *skb,
				   struct nf_conn *ct)
{
	struct ursp_conn *new_ursp_ct;
	struct ursp_fwd_entry *ursp_fwd_entry;
	struct nf_conntrack_tuple *tuple =
		&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	u8 l4proto = tuple->dst.protonum;
	u32 hash;
	unsigned long newtime;

	if (!ursp_check_netdevice_change(skb->dev))
		return 0;

	if (!skb->sk)
		return 0;

	rcu_read_lock_bh();
	spin_lock_bh(&ursp_lock);
	hash = ursp_hash_conntrack(tuple);
	hlist_for_each_entry_rcu(ursp_fwd_entry,
				 &ursp_fwd_entries[hash],
				 entry_lst) {
		if (ursp_ct_tuple_equal(ursp_fwd_entry, tuple)) {
			FP_PRT_DBG(FP_PRT_DEBUG, "URSP:tuple hash exist, ignore\n");
			if (l4proto == IP_L4_PROTO_UDP) {
				newtime = jiffies + DEFAULT_URSP_UDP_AGING_TIME;
				if (newtime - ursp_fwd_entry->timeout.expires >= HZ)
					mod_timer(&ursp_fwd_entry->timeout, newtime);
			}
			spin_unlock_bh(&ursp_lock);
			rcu_read_unlock_bh();
			return 1;
		}
	}

	new_ursp_ct = kmalloc(sizeof(*new_ursp_ct), GFP_KERNEL);
	if (!new_ursp_ct) {
		spin_unlock_bh(&ursp_lock);
		rcu_read_unlock_bh();
		return -ENOMEM;
	}

	memset(new_ursp_ct, 0, sizeof(struct ursp_conn));
	new_ursp_ct->tuple = *tuple;
	new_ursp_ct->hash = hash;
	new_ursp_ct->uid = from_kuid_munged(current_user_ns(), sock_i_uid(skb->sk));
	new_ursp_ct->out_ifindex = skb->dev->ifindex;
	add_in_ursp_fwd_table(new_ursp_ct);
	spin_unlock_bh(&ursp_lock);
	rcu_read_unlock_bh();

	return 0;
}

int ursp_filter_fwd_create_entries(u8 pf, struct sk_buff *skb,
				   const struct nf_hook_state *state)
{
	enum ip_conntrack_info ctinfo;
	struct net *net;
	struct nf_conn *ct;
	struct nf_conntrack_tuple *tuple;
	u8 l4proto;

	if (!get_ursp_enable())
		return 0;

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct)
		return 0;

	if (!test_bit(IPS_CONFIRMED_BIT, &ct->status))
		return 0;

	tuple = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	l4proto = tuple->dst.protonum;

	net = nf_ct_net(ct);

	if (!net)
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "invalid net, won't do ursp\n");

	if (l4proto == IPPROTO_TCP && ursp_tcp_flag_chk(ct))
		return 0;

	if (l4proto == IPPROTO_TCP &&
	    test_bit(IPS_ASSURED_BIT, &ct->status))
		ursp_ct_tcp_sure(pf, skb, ct);

	if (l4proto == IPPROTO_UDP)
		create_ursp_fwd_entries(pf, skb, ct);

	return 0;
}

int ursp_ct_tcp_sure(u8 pf, struct sk_buff *skb, struct nf_conn *ct)
{
	struct nf_conntrack_tuple *tuple;
	struct ursp_fwd_entry *curr_fwd_entry;
	struct ursp_conn *ursp_ct;
	u32 hash;

	if (!ursp_check_netdevice_change(skb->dev))
		return 0;

	tuple =
	(struct nf_conntrack_tuple *)&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	hash = ursp_hash_conntrack(tuple);
	rcu_read_lock_bh();
	spin_lock_bh(&ursp_lock);
	hlist_for_each_entry_rcu(curr_fwd_entry,
				 &ursp_fwd_entries[hash],
				 entry_lst) {
		if (ursp_ct_tuple_equal(curr_fwd_entry, tuple)) {
			spin_unlock_bh(&ursp_lock);
			rcu_read_unlock_bh();
			return 1;
		}
	}

	if (!skb->sk) {
		spin_unlock_bh(&ursp_lock);
		rcu_read_unlock_bh();
		return 0;
	}
	ursp_ct = kmalloc(sizeof(*ursp_ct), GFP_ATOMIC);

	if (!ursp_ct) {
		spin_unlock_bh(&ursp_lock);
		rcu_read_unlock_bh();
		return -ENOMEM;
	}

	memset(ursp_ct, 0, sizeof(struct ursp_conn));
	ursp_ct->tuple = *tuple;
	ursp_ct->hash = hash;
	ursp_ct->uid = from_kuid_munged(current_user_ns(), sock_i_uid(skb->sk));
	ursp_ct->out_ifindex = skb->dev->ifindex;
	FP_PRT_TUPLE_INFO(FP_PRT_DEBUG, tuple);
	ursp_ct->tcp_sure_flag = 1;
	add_in_ursp_fwd_table(ursp_ct);
	spin_unlock_bh(&ursp_lock);
	rcu_read_unlock_bh();
	return 1;
}
EXPORT_SYMBOL(ursp_ct_tcp_sure);

void ursp_clear_fwd_table(int ifindex)
{
	struct ursp_fwd_entry *ursp_fwd_entry;
	struct ursp_conn *ursp_ct;
	int i;

	if (ifindex == 0)
		return;

	spin_lock_bh(&ursp_lock);
	rcu_read_lock_bh();
	for (i = 0; i < URSP_ENTRIES_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(ursp_fwd_entry,
					 &ursp_fwd_entries[i],
					 entry_lst) {
			ursp_ct = ursp_fwd_entry->ursp_ct;
			if (ursp_ct->out_ifindex == ifindex) {
				del_timer(&ursp_fwd_entry->timeout);
				hlist_del_rcu(&ursp_fwd_entry->entry_lst);
				ursp_notify(ursp_ct, CLOSE_EVENT);
				call_rcu(&ursp_fwd_entry->rcu,
					 ursp_fwd_entry_free);
				kfree(ursp_ct);
			}
		}
	}
	rcu_read_unlock_bh();
	spin_unlock_bh(&ursp_lock);
}

static int ursp_if_netdev_event_handler(struct notifier_block *nb,
					unsigned long event, void *ptr)
{
	int ifindex;
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);

	switch (event) {
	case NETDEV_DOWN:
		ifindex = ursp_check_netdevice_change(dev);
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "iface_stat: DOWN dev ifindex = %d\n",
			   ifindex);
		ursp_clear_fwd_table(ifindex);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block ursp_if_netdev_notifier_blk = {
	.notifier_call = ursp_if_netdev_event_handler,
};

void ursp_entries_hash_init(void)
{
	int i;

	FP_PRT_DBG(FP_PRT_DEBUG, "initializing ursp entries hash table\n");
	for (i = 0; i < URSP_ENTRIES_HASH_SIZE; i++)
		INIT_HLIST_HEAD(&ursp_fwd_entries[i]);
}

int ursp_init(void)
{
	spin_lock_init(&ursp_lock);
	ursp_entries_hash_init();
	return 0;
}

static int __init init_ursp_module(void)
{
	int status;

	register_netdevice_notifier(&ursp_if_netdev_notifier_blk);
	status = ursp_init();
	ursp_proc_create();
	nl_ursp_init();
	if (status != URSP_OK)
		return -EPERM;

	nf_ursp_conntrack_init();
	return 0;
}

static void __exit exit_ursp_module(void)
{
	nl_ursp_exit();
	unregister_netdevice_notifier(&ursp_if_netdev_notifier_blk);
}

late_initcall(init_ursp_module);
module_exit(exit_ursp_module);
MODULE_ALIAS("platform:SPRD URSP.");
MODULE_AUTHOR("Cathy Cai <cathy.cai@unisoc.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

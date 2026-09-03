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
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/sysctl.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/inet_hashtables.h>
#include <net/addrconf.h>
#include <net/ipv6.h>
#include "sla_net_stats_netlink.h"
#include "sla.h"

//default interval 1 second
#define DEFAULT_UPDATE_INTERVAL		(1)
#define BAD_RTT_THRESHOLD		(50000)
#define BAD_RTT_DURATION		(5)
#define NOTE_RTT_THRESHOLD		(100000)
#define NOTE_RTT_DURATION		(30)
#define NOTE_RTT_THRESHOLD_2		(50000)
#define NOTE_RTT_DURATION_2		(60)
//#define DUMP_LOG

struct net_stats {
	struct list_head	list;
	struct net_device	*dev;
	unsigned long		last_rx_bytes;
	unsigned long		last_tx_bytes;
	unsigned long		rtt;
	unsigned long		sum_rtt;
	unsigned long		num_rtt;
	unsigned long		bad_rtt_num;
	unsigned long		note_rtt_num;
	unsigned long		note_rtt_num_2;
	unsigned long		rx_rate;
	unsigned long		tx_rate;
	unsigned long		retrans_bytes;
	unsigned long		total_tx_bytes;
	unsigned char		retran_rate;  //retransmission rate example 1  starnd for 1%
};

static struct list_head netstats_list;
static struct timer_list net_stats_timer;
static spinlock_t netstats_lock;
static int netstats_enable;

void sla_net_stats_set(int val)
{
	netstats_enable = val;
}

static struct net_stats *add_net_device(struct net_device *dev)
{
	struct net_stats *entry;

	if (!strcmp(dev->name, "lo")
		|| !strcmp(dev->name, "dummy0")
		|| !strcmp(dev->name, "sipa_dummy0")) {
		return NULL;
	}
	entry = kmalloc(sizeof(struct net_stats), GFP_KERNEL);

	if (!entry)
		return NULL;

	memset(entry, 0, sizeof(struct net_stats));
	entry->dev = dev;

	spin_lock_bh(&netstats_lock);
	list_add(&entry->list, &netstats_list);
	spin_unlock_bh(&netstats_lock);

	return entry;
}

static void remove_net_device(struct net_device *dev)
{
	struct net_stats *entry, *tmp;

	spin_lock_bh(&netstats_lock);
	list_for_each_entry_safe(entry, tmp, &netstats_list, list) {
		if (entry->dev == dev) {
			list_del(&entry->list);
			kfree(entry);
			break;
		}
	}
	spin_unlock_bh(&netstats_lock);
}

static int is_alread_add(struct net_device *dev)
{
	struct net_stats *entry, *tmp;
	int exist = 0;

	spin_lock_bh(&netstats_lock);
	list_for_each_entry_safe(entry, tmp, &netstats_list, list) {
		if (entry->dev == dev) {
			exist = 1;
			break;
		}
	}
	spin_unlock_bh(&netstats_lock);

	return exist;
}

static int netstats_dev_event_handler(struct notifier_block *unused, unsigned long event, void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);

	if (dev) {
		switch (event) {
		case NETDEV_UP:
			if (!is_alread_add(dev))
				add_net_device(dev);
			break;
		case NETDEV_DOWN:
			remove_net_device(dev);
			break;
		default:
			break;
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block netstatsdev_notifier = {
	 .notifier_call = netstats_dev_event_handler
};

struct net_stats *get_netstats_by_dev(struct net_device *dev)
{
	struct net_stats *entry, *tmp, *result = NULL;

	list_for_each_entry_safe(entry, tmp, &netstats_list, list) {
		if (entry->dev == dev) {
			result = entry;
			break;
		}
	}

	return result;
}

static void net_stats_calc_rtt(void)
{
	struct net_stats *entry, *tmp;

	spin_lock_bh(&netstats_lock);
	list_for_each_entry_safe(entry, tmp, &netstats_list, list) {
		if (entry->num_rtt) {
			entry->rtt = entry->sum_rtt / entry->num_rtt;
			entry->sum_rtt  = 0;
			entry->num_rtt = 0;
		} else {
			entry->rtt = 0;
		}

		if (entry->rtt >= BAD_RTT_THRESHOLD)
			entry->bad_rtt_num++;
		else
			entry->bad_rtt_num = 0;

		if (entry->rtt < NOTE_RTT_THRESHOLD)
			entry->note_rtt_num++;
		else
			entry->note_rtt_num = 0;

		if (entry->rtt <= NOTE_RTT_THRESHOLD_2)
			entry->note_rtt_num_2++;
		else
			entry->note_rtt_num_2 = 0;

		if (entry->total_tx_bytes) {
			entry->retran_rate = entry->retrans_bytes * 100 / entry->total_tx_bytes;
			entry->total_tx_bytes = 0;
			entry->retrans_bytes = 0;
		} else {
			entry->retran_rate = 0;
		}
	}
	spin_unlock_bh(&netstats_lock);
}

static void net_stats_estimate_dev_speed(void)
{
	struct rtnl_link_stats64 stats;
	struct net_stats *entry, *tmp;

	spin_lock_bh(&netstats_lock);
	list_for_each_entry_safe(entry, tmp, &netstats_list, list) {
		dev_get_stats(entry->dev, &stats);

		entry->rx_rate = stats.rx_bytes - entry->last_rx_bytes;
		entry->tx_rate = stats.tx_bytes - entry->last_tx_bytes;
		entry->last_rx_bytes = stats.rx_bytes;
		entry->last_tx_bytes = stats.tx_bytes;
	}
	spin_unlock_bh(&netstats_lock);
}

static void net_stats_statistic_rtt_and_retran(void)
{
	struct hlist_nulls_node *node;
	struct inet_ehash_bucket *head;
	struct sock *sk;
	spinlock_t *lock;
	struct net_stats *pnet_stats = NULL;
	struct dst_entry  *dst;
	int i;

	for (i = 0; i <= tcp_hashinfo.ehash_mask; i++) {
		head = &(tcp_hashinfo.ehash[i]);

		if (hlist_nulls_empty(&head->chain))
			continue;

		lock = inet_ehash_lockp(&tcp_hashinfo, i);
		spin_lock_bh(lock);
		sk_nulls_for_each(sk, node, &head->chain) {
			const struct tcp_sock *tp = tcp_sk(sk);

			if (sk->sk_state != TCP_ESTABLISHED)
				continue;

			dst = sk_dst_get(sk);
			if (dst == NULL)
				continue;

			if (!strcmp(dst->dev->name, "lo"))
				continue;

			spin_lock_bh(&netstats_lock);
			pnet_stats = get_netstats_by_dev(dst->dev);

			if (pnet_stats == NULL) {
				spin_unlock_bh(&netstats_lock);
				continue;
			}

			pnet_stats->sum_rtt += tp->srtt_us >> 3;
			pnet_stats->num_rtt++;
			pnet_stats->retrans_bytes +=  tp->bytes_retrans;
			pnet_stats->total_tx_bytes += tp->bytes_sent;
			spin_unlock_bh(&netstats_lock);
		}
		spin_unlock_bh(lock);
	}
}

static void net_stats_init_stats_dev(void)
{
	struct net_device *dev;
	struct rtnl_link_stats64 stats;
	struct net_stats *entry;

	for_each_netdev(&init_net, dev) {
		if (!(dev->flags & IFF_UP))
			continue;

		dev_get_stats(dev, &stats);

		//Add the up interface to netstats_list
		entry = add_net_device(dev);
		if (entry) {
			entry->last_rx_bytes = stats.rx_bytes;
			entry->last_tx_bytes = stats.tx_bytes;
		}
	}
}

static void net_stats_notfiy(void)
{
	struct sla_net_stats_info info;
	struct net_stats *entry, *tmp;

	spin_lock_bh(&netstats_lock);
	list_for_each_entry_safe(entry, tmp, &netstats_list, list) {
		info.if_index = entry->dev->ifindex;
		info.rtt = entry->rtt / 1000;
		info.tx_rate = entry->tx_rate * 8;
		info.rx_rate = entry->rx_rate * 8;
		info.retran_rate = entry->retran_rate;

		/* when RTT >= BAD_RTT_THRESHOLD us has been
		 * persistent for BAD_RTT_DURATION seconds
		 */
		if (entry->bad_rtt_num >= BAD_RTT_DURATION) {
			info.msg = SLA_NL_NET_STATS_MSG;
			sla_netlink_notify(SLA_NL_NET_STATS_MSG, &info);
			entry->bad_rtt_num = 0;
		}

		/* when RTT < NOTE_RTT_THRESHOLD us has been persistent for
		 * NOTE_RTT_DURATION seconds
		 */
		if (entry->note_rtt_num >= NOTE_RTT_DURATION) {
			info.msg = SLA_NL_NET_STATS_GOOD_RTT;
			sla_netlink_notify(SLA_NL_NET_STATS_GOOD_RTT, &info);
			entry->note_rtt_num = 0;
		}

		/* when RTT < NOTE_RTT_THRESHOLD_2 us has been persistent for
		 * NOTE_RTT_DURATION_2 seconds
		 */
		if (entry->note_rtt_num_2 >= NOTE_RTT_DURATION_2) {
			info.msg = SLA_NL_NET_STATS_GOOD_RTT_2;
			sla_netlink_notify(SLA_NL_NET_STATS_GOOD_RTT_2, &info);
			entry->note_rtt_num_2 = 0;
		}
	}
	spin_unlock_bh(&netstats_lock);
}

static void net_stats_timer_handler(struct timer_list *t)
{
	if (netstats_enable) {
		net_stats_estimate_dev_speed();
		net_stats_statistic_rtt_and_retran();
		net_stats_calc_rtt();
#ifdef DUMP_LOG
		sla_net_stats_print();
#endif
		net_stats_notfiy();
	}
	mod_timer(&net_stats_timer, jiffies + msecs_to_jiffies(1000));
}

int sla_net_stats_get_netinfo(int if_index, struct sla_net_stats_info *info)
{
	struct net_stats *entry, *tmp;
	int ret = -1;

	if (info == NULL)
		return -1;

	spin_lock_bh(&netstats_lock);
	list_for_each_entry_safe(entry, tmp, &netstats_list, list) {
		if (entry->dev->ifindex == if_index) {
			info->if_index = if_index;
			info->rtt = entry->rtt / 1000;
			info->tx_rate = entry->tx_rate * 8;
			info->rx_rate = entry->rx_rate * 8;
			info->retran_rate = entry->retran_rate;
			ret = 0;
			break;
		}
	}
	spin_unlock_bh(&netstats_lock);

	return ret;
}

int sla_net_stats_init(void)
{
	spin_lock_init(&netstats_lock);
	INIT_LIST_HEAD(&netstats_list);

	net_stats_init_stats_dev();
	register_netdevice_notifier(&netstatsdev_notifier);

	timer_setup(&net_stats_timer, net_stats_timer_handler, 0);
	mod_timer(&net_stats_timer, jiffies + msecs_to_jiffies(1000));

	return 0;
}

void sla_net_stats_exit(void)
{
	struct net_stats *entry, *tmp;

	del_timer_sync(&net_stats_timer);
	unregister_netdevice_notifier(&netstatsdev_notifier);

	spin_lock_bh(&netstats_lock);
	list_for_each_entry_safe(entry, tmp, &netstats_list, list) {
		list_del(&entry->list);
		kfree(entry);
	}
	spin_unlock_bh(&netstats_lock);
}


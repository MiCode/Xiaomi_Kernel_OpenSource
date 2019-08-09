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

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/jhash.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include "mddp_track.h"
#include "mddp_f_proto.h"
#include "mddp_f_tuple.h"
#include "mddp_f_desc.h"

struct list_head *nat_tuple_hash;
unsigned int nat_tuple_hash_rnd;

struct list_head *router_tuple_hash;
unsigned int router_tuple_hash_rnd;

int mddp_f_nat_cnt;
int mddp_f_router_cnt;

void mddp_f_init_router_tuple(void)
{
	int i;

	/* get 4 bytes random number */
	get_random_bytes(&router_tuple_hash_rnd, 4);

	/* allocate memory for bridge hash table */
	router_tuple_hash =
		vmalloc(sizeof(struct list_head) * ROUTER_TUPLE_HASH_SIZE);

	/* init hash table */
	for (i = 0; i < ROUTER_TUPLE_HASH_SIZE; i++)
		INIT_LIST_HEAD(&router_tuple_hash[i]);
}

void mddp_f_init_nat_tuple(void)
{
	int i;

	/* get 4 bytes random number */
	get_random_bytes(&nat_tuple_hash_rnd, 4);

	/* allocate memory for two nat hash tables */
	nat_tuple_hash =
		vmalloc(sizeof(struct list_head) * NAT_TUPLE_HASH_SIZE);

	/* init hash table */
	for (i = 0; i < NAT_TUPLE_HASH_SIZE; i++)
		INIT_LIST_HEAD(&nat_tuple_hash[i]);
}

void mddp_f_del_nat_tuple(struct nat_tuple *t)
{
	unsigned long flag;

	pr_info("%s: Del nat tuple[%p], next[%p], prev[%p].\n",
		__func__, t, t->list.next, t->list.prev);

	//del_timer(&t->timeout_used);

	MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
	mddp_f_nat_cnt--;

	/* remove from the list */
	if (t->list.next != LIST_POISON1 && t->list.prev != LIST_POISON2) {
		list_del(&t->list);
	} else {
		pr_notice("%s: Del nat tuple fail, tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);
		WARN_ON(1);
	}
	MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);

	kmem_cache_free(mddp_f_nat_tuple_cache, t);
}
EXPORT_SYMBOL(mddp_f_del_nat_tuple);

void mddp_f_timeout_nat_tuple(unsigned long data)
{
	struct nat_tuple *t = (struct nat_tuple *)data;

	mddp_f_del_nat_tuple(t);
}
EXPORT_SYMBOL(mddp_f_timeout_nat_tuple);

bool mddp_f_add_nat_tuple(struct nat_tuple *t)
{
	unsigned long flag;
	unsigned int hash;
	struct nat_tuple *found_nat_tuple;

	pr_info("%s: Add new nat tuple[%p] with src_port[%d] & proto[%d].\n",
		__func__, t, t->src.all, t->proto);
	pr_debug("%s: 1. Before add nat tuple[%p], next[%p], prev[%p].\n",
		__func__, t, t->list.next, t->list.prev);

	if (mddp_f_nat_cnt >= mddp_f_max_nat) {
		pr_notice("%s: Nat tuple table is full! Tuple[%p] is about to free.\n",
			__func__, t);
		kmem_cache_free(mddp_f_nat_tuple_cache, t);
		return false;
	}

	switch (t->proto) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
		hash = HASH_NAT_TUPLE_TCPUDP(t);
		break;
	default:
		kmem_cache_free(mddp_f_nat_tuple_cache, t);
		return false;
	}

	MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
	INIT_LIST_HEAD(&t->list);
	/* prevent from duplicating */
	list_for_each_entry(found_nat_tuple, &nat_tuple_hash[hash], list) {
		pr_debug("%s: 2. Add nat tuple, found_nat_tuple[%p], src_ip[%x], dst_ip[%x], proto[%x].\n",
			__func__, found_nat_tuple, found_nat_tuple->src_ip,
			found_nat_tuple->dst_ip, found_nat_tuple->proto);
		pr_debug("%s: 3. Add nat tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);
		if (found_nat_tuple->src_ip != t->src_ip)
			continue;
		if (found_nat_tuple->dst_ip != t->dst_ip)
			continue;
		if (found_nat_tuple->proto != t->proto)
			continue;
		if (found_nat_tuple->src.all != t->src.all)
			continue;
		if (found_nat_tuple->dst.all != t->dst.all)
			continue;
		MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);
		pr_info("%s: Nat tuple[%p] is duplicated!\n", __func__, t);
		kmem_cache_free(mddp_f_nat_tuple_cache, t);
		return false;   /* duplication */
	}
	/* add to the list */
	list_add_tail(&t->list, &nat_tuple_hash[hash]);
	mddp_f_nat_cnt++;
	MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);

	pr_info("%s: Add nat tuple[%p], next[%p], prev[%p].\n",
		__func__, t, t->list.next, t->list.prev);

	/* init timer and start it */
	init_timer(&t->timeout_used);
	t->timeout_used.data = (unsigned long)t;
	t->timeout_used.function = mddp_f_timeout_nat_tuple;
	t->timeout_used.expires = jiffies + HZ * USED_TIMEOUT;

	add_timer(&t->timeout_used);

	return true;
}

static inline struct nat_tuple *mddp_f_get_nat_tuple_ip4_tcpudp(
	struct tuple *t)
{
	unsigned long flag;
	unsigned int hash;
	int not_match;
	struct nat_tuple *found_nat_tuple;

	hash = HASH_TUPLE_TCPUDP(t);

	MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
	list_for_each_entry(found_nat_tuple, &nat_tuple_hash[hash], list) {
		not_match = 0;
#ifndef MDDP_F_NETFILTER
		not_match +=
			(!ifaces[found_nat_tuple->iface_src].ready) ? 1 : 0;
		not_match +=
			(!ifaces[found_nat_tuple->iface_dst].ready) ? 1 : 0;
#else
		not_match +=
			(found_nat_tuple->dev_src != t->dev_in) ? 1 : 0;
		not_match +=
			(!found_nat_tuple->dev_dst) ? 1 : 0;
#endif
		not_match +=
			(found_nat_tuple->src_ip != t->nat.src) ? 1 : 0;
		not_match +=
			(found_nat_tuple->dst_ip != t->nat.dst) ? 1 : 0;
		not_match +=
			(found_nat_tuple->proto != t->nat.proto) ? 1 : 0;
		not_match +=
			(found_nat_tuple->src.all != t->nat.s.all) ? 1 : 0;
		not_match +=
			(found_nat_tuple->dst.all != t->nat.d.all) ? 1 : 0;
		if (unlikely(not_match))
			continue;

		MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);
		return found_nat_tuple;
	}
	MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);
	/* not found */
	return 0;
}

void mddp_f_del_router_tuple(struct router_tuple *t)
{
	unsigned long flag;

	pr_info("%s: Del router tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);

	//del_timer(&t->timeout_used);

	MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
	mddp_f_router_cnt--;

	/* remove from the list */
	if (t->list.next != LIST_POISON1 && t->list.prev != LIST_POISON2) {
		list_del(&t->list);
	} else {
		pr_notice("%s: Del router tuple fail, tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);
		WARN_ON(1);
	}
	MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);

	kmem_cache_free(mddp_f_router_tuple_cache, t);
}
EXPORT_SYMBOL(mddp_f_del_router_tuple);

void mddp_f_timeout_router_tuple(unsigned long data)
{
	struct router_tuple *t = (struct router_tuple *)data;

	mddp_f_del_router_tuple(t);
}
EXPORT_SYMBOL(mddp_f_timeout_router_tuple);

bool mddp_f_add_router_tuple_tcpudp(struct router_tuple *t)
{
	unsigned long flag;
	unsigned int hash;
	struct router_tuple *found_router_tuple;

	pr_info("%s: Add new tcpudp router tuple[%p] with src_port[%d] & proto[%d].\n",
			__func__, t, t->in.all, t->proto);

	if (mddp_f_router_cnt >= mddp_f_max_router) {
		kmem_cache_free(mddp_f_router_tuple_cache, t);

		pr_notice("%s: TCPUDP router is full, tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);
		return false;
	}
	hash = HASH_ROUTER_TUPLE_TCPUDP(t);

	MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
	INIT_LIST_HEAD(&t->list);
	/* prevent from duplicating */
	list_for_each_entry(found_router_tuple,
				&router_tuple_hash[hash], list) {
#ifndef MDDP_F_NETFILTER
		if (found_router_tuple->iface_src != t->iface_src)
			continue;
#else
		if (found_router_tuple->dev_src != t->dev_src)
			continue;
#endif
		if (!ipv6_addr_equal(&found_router_tuple->saddr, &t->saddr))
			continue;
		if (!ipv6_addr_equal(&found_router_tuple->daddr, &t->daddr))
			continue;
		if (found_router_tuple->proto != t->proto)
			continue;
		if (found_router_tuple->in.all != t->in.all)
			continue;
		if (found_router_tuple->out.all != t->out.all)
			continue;
		MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);
		pr_info("%s: TCPUDP router is duplicated, tuple[%p].\n",
			__func__, t);
		kmem_cache_free(mddp_f_router_tuple_cache, t);
		return false;   /* duplication */
	}
	/* add to the list */
	list_add_tail(&t->list, &router_tuple_hash[hash]);

	mddp_f_router_cnt++;
	MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);

	pr_info("%s: Add tcpudp router tuple[%p], next[%p], prev[%p].\n",
			__func__, t, t->list.next, t->list.prev);

	/* init timer and start it */
	init_timer(&t->timeout_used);
	t->timeout_used.data = (unsigned long)t;
	t->timeout_used.function = mddp_f_timeout_router_tuple;
	t->timeout_used.expires = jiffies + HZ * USED_TIMEOUT;

	add_timer(&t->timeout_used);

	return true;
}

static inline struct router_tuple *mddp_f_get_router_tuple_tcpudp(
	struct router_tuple *t)
{
	unsigned long flag;
	unsigned int hash;
	struct router_tuple *found_router_tuple;
	int not_match;

	hash = HASH_ROUTER_TUPLE_TCPUDP(t);

	MDDP_F_TUPLE_LOCK(&mddp_f_tuple_lock, flag);
	list_for_each_entry(found_router_tuple,
				&router_tuple_hash[hash], list) {
		not_match = 0;
#ifndef MDDP_F_NETFILTER
		not_match +=
			(!ifaces[found_router_tuple->iface_dst].ready) ? 1 : 0;
		not_match +=
			(!ifaces[found_router_tuple->iface_src].ready) ? 1 : 0;
#else
		not_match +=
			(!found_router_tuple->dev_dst) ? 1 : 0;
		not_match +=
			(!found_router_tuple->dev_src) ? 1 : 0;
#endif
		not_match +=
			(!ipv6_addr_equal(&found_router_tuple->saddr,
							&t->saddr)) ? 1 : 0;
		not_match +=
			(!ipv6_addr_equal(&found_router_tuple->daddr,
							&t->daddr)) ? 1 : 0;
		not_match +=
			(found_router_tuple->proto != t->proto) ? 1 : 0;
		not_match +=
			(found_router_tuple->in.all != t->in.all) ? 1 : 0;
		not_match +=
			(found_router_tuple->out.all != t->out.all) ? 1 : 0;
		if (unlikely(not_match))
			continue;

		MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);
		return found_router_tuple;
	}
	/* not found */
	MDDP_F_TUPLE_UNLOCK(&mddp_f_tuple_lock, flag);
	return 0;
}


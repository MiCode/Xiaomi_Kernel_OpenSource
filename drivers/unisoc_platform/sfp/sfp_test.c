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
#define pr_fmt(fmt) "sfp: " fmt

#include <linux/major.h>
#include <linux/ip.h>
#include <net/netfilter/nf_nat.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/timer.h>
#include <linux/skbuff.h>

#include "sfp.h"
#include "sfp_ipa.h"

static struct task_struct *snfp_test_taskn;

//static struct hlist_head test_fwd_entries[SFP_ENTRIES_HASH_SIZE];
static __be32 saddr = cpu_to_be32(0x2A89A8C0);
static __be32 daddr = cpu_to_be32(0x98965DCC);
static __be16 srcport = cpu_to_be16(0x6BFC);
static __be16 dstport = cpu_to_be16(0x5000);
static __be32 tgt_dstip = cpu_to_be32(0x0A0A0A01);
static __be32 tgt_srcip = cpu_to_be32(0x0A0A0A0A);
static __be16 tgt_srcport = cpu_to_be16(5000);
static __be16 tgt_dstport = cpu_to_be16(50000);
int test_count;
int test_result;

#define IP_1 0x0A0A0A01 /* pc */
#define IP_2 0x0A0A0A02 /* modem ip */
#define IP_3 0x0A0A0A03 /* server */

enum {
	SFP_TEST_PASS,
	SFP_TEST_FAIL,
	SFP_TEST_INPROGRESS,
};

static struct timer_list ipa_timer;
struct tuple_info {
	u32 ip1;
	u32 ip2;
	u32 ip3;
	u16 s_port;
	u16 d_port;
	u8 protonum;
};

static struct tuple_info g_tuples[] = {
	{IP_1, IP_2, IP_3, 10283, 12345, 17},
	{IP_1, IP_2, IP_3, 10363, 12345, 17},
	{IP_1, IP_2, IP_3, 10819, 12345, 17},
	{IP_1, IP_2, IP_3, 10828, 12345, 17},
	{IP_1, IP_2, IP_3, 11082, 12345, 17},
	{IP_1, IP_2, IP_3, 11645, 12345, 17},
	{IP_1, IP_2, IP_3, 11816, 12345, 17},
	{IP_1, IP_2, IP_3, 11887, 12345, 17},
};

struct sfp_test_fwd_entry {
	struct hlist_node entry_lst;
	struct nf_conntrack_tuple tuple;
	struct rcu_head	 rcu;
};

static void test_sfp_fwd_entry_free(struct rcu_head *head)
{
	struct sfp_test_fwd_entry *sfp_fwd_entry;

	sfp_fwd_entry = container_of(head, struct sfp_test_fwd_entry, rcu);
	kfree(sfp_fwd_entry);
}

static void make_ct_tuple(struct tuple_info *ti, struct nf_conn *ct)
{
	memset(ct, 0, sizeof(*ct));
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip = htonl(ti->ip1);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.l3num = NFPROTO_IPV4;
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.all = htons(ti->s_port);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.protonum = ti->protonum;
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.all = htons(ti->d_port);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u3.ip = htonl(ti->ip3);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.dir = 0;

	ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip = htonl(ti->ip3);
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.l3num = NFPROTO_IPV4;
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u.all = htons(ti->d_port);
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.protonum = ti->protonum;
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u.all = htons(ti->s_port);
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u3.ip = htonl(ti->ip2);
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.dir = 1;
}

static void sfp_test_init_mac(struct sfp_conn *sfp_ct)
{
	u8 orig_src[ETH_ALEN] = {0x11, 0, 0, 0, 0, 0x11};
	u8 orig_dst[ETH_ALEN] = {0x11, 0, 0, 0, 0, 0x22};
	u8 reply_src[ETH_ALEN] = {0x11, 0, 0, 0, 0, 0x44};
	u8 reply_dst[ETH_ALEN] = {0x11, 0, 0, 0, 0, 0x33};
	struct sfp_mgr_fwd_tuple_hash *thptr;

	thptr = &sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	mac_addr_copy(thptr->ssfp_fwd_tuple.orig_mac_info.src_mac,
		      orig_src);
	mac_addr_copy(thptr->ssfp_fwd_tuple.orig_mac_info.dst_mac,
		      orig_dst);
	mac_addr_copy(thptr->ssfp_fwd_tuple.trans_mac_info.src_mac,
		      reply_dst);
	mac_addr_copy(thptr->ssfp_fwd_tuple.trans_mac_info.dst_mac,
		      reply_src);

	thptr = &sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	mac_addr_copy(thptr->ssfp_fwd_tuple.orig_mac_info.src_mac,
		      reply_src);
	mac_addr_copy(thptr->ssfp_fwd_tuple.orig_mac_info.dst_mac,
		      reply_dst);
	mac_addr_copy(thptr->ssfp_fwd_tuple.trans_mac_info.src_mac,
		      orig_dst);
	mac_addr_copy(thptr->ssfp_fwd_tuple.trans_mac_info.dst_mac,
		      orig_src);
}

static int start_insert_and_print_thread(void)
{
	struct sfp_test_fwd_entry *tuple_entry;
	struct sfp_test_fwd_entry *ta_tuple_entry;
	struct sfp_test_fwd_entry *test_entry;
	u32 hash;
	int i;
	int count_x, count_y;

	while (test_count) {
		tuple_entry = kmalloc(sizeof(*tuple_entry), GFP_ATOMIC);
		if (!tuple_entry)
			return -ENOMEM;

		memset(tuple_entry, 0, sizeof(struct sfp_test_fwd_entry));
		tuple_entry->tuple.src.u3.ip = saddr;
		tuple_entry->tuple.src.l3num = AF_INET;
		tuple_entry->tuple.src.u.all = srcport;
		tuple_entry->tuple.dst.protonum = 0x06;
		tuple_entry->tuple.dst.u.all = dstport;
		tuple_entry->tuple.dst.u3.ip = daddr;

		hash = sfp_hash_conntrack(&tuple_entry->tuple);

		hlist_add_head_rcu(&tuple_entry->entry_lst,
				   &mgr_fwd_entries[hash]);

		ta_tuple_entry = kmalloc(sizeof(*ta_tuple_entry),
					 GFP_ATOMIC);

		if (!ta_tuple_entry)
			break;

		ta_tuple_entry->tuple.src.u3.ip = tgt_srcip;
		ta_tuple_entry->tuple.src.l3num = AF_INET;
		ta_tuple_entry->tuple.src.u.all = tgt_srcport;
		ta_tuple_entry->tuple.dst.protonum = 0x06;
		ta_tuple_entry->tuple.dst.u.all = tgt_dstport;
		ta_tuple_entry->tuple.dst.u3.ip = tgt_dstip;

		hash = sfp_hash_conntrack(&ta_tuple_entry->tuple);

		hlist_add_head_rcu(&ta_tuple_entry->entry_lst,
				   &mgr_fwd_entries[hash]);
		msleep(20);
		test_count--;
		be16_add_cpu(&srcport, 1);
		be16_add_cpu(&tgt_dstport, 1);
	}
	msleep(20);
	count_x = 0;
	count_y = 0;
	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		count_y = 0;
		hlist_for_each_entry_rcu(test_entry,
					 &mgr_fwd_entries[i],
					 entry_lst) {
			msleep(20);
			hlist_del_rcu(&test_entry->entry_lst);
			call_rcu(&test_entry->rcu, test_sfp_fwd_entry_free);
		}
	}

	return 0;
}

static int snfp_test_thread_1(void *arg)
{
	start_insert_and_print_thread();
	return 0;
}

/* test adding rules to mgr_fwd_entries/sfp_fwd_entries,
 * We will consider this case pass as long as no sysdump occurs
 */
static int snfp_test_thread_2(void *arg)
{
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;
	int in_ifindex = 1;
	int out_ifindex = 2;
	int out_ipaifindex = 0x1;
	int in_ipaifindex = 0x6;

	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	u32 hash, hash_inv;

	test_result = SFP_TEST_INPROGRESS;

	make_ct_tuple(&g_tuples[0], &ct);
	sysctl_net_sfp_tether_scheme = 1;

	new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
	if (!new_sfp_ct)
		return -ENOMEM;

	FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
	memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

	sfp_ct_init(&ct, new_sfp_ct);
	sfp_test_init_mac(new_sfp_ct);

	/* Out net device id */
	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = in_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = out_ipaifindex;

	hash = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_ORIGINAL] = hash;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash]);

	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = out_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = in_ipaifindex;

	hash_inv = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_REPLY] = hash_inv;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash_inv]);

	sfp_fwd_hash_add(new_sfp_ct);
	sfp_clear_fwd_table(out_ifindex);
	pr_info("%s test pass\n", __func__);

	test_result = SFP_TEST_PASS;
	return 0;
}

static unsigned char testdata1[] = {
	/* mac */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
	/* ipv4 len 52 */
	0x45, 0x00, 0x00, 0x34, 0x51, 0x1d, 0x40, 0x00,
	0x40, 0x11, 0x1d, 0x70, 0x0a, 0x0a, 0x0a, 0x01,
	0x0a, 0x0a, 0x0a, 0x03,
	/* udp len 8 */
	0x28, 0x2b, 0x30, 0x39, 0x00, 0x20, 0xe3, 0x1d,
	/*payload len 24*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,};

static unsigned char testdata2[] = {
	/* mac */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
	/* ipv4 len 52 */
	0x45, 0x00, 0x00, 0x34, 0x51, 0x1d, 0x40, 0x00,
	0x40, 0x11, 0x1d, 0x70, 0x0a, 0x0a, 0x0a, 0x06,
	0x0a, 0x0a, 0x0a, 0x07,
	/* udp len 8 */
	0x28, 0x2b, 0x30, 0x39, 0x00, 0x20, 0xe3, 0x1d,
	/*payload len 24*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,};

static unsigned char testdata_nated[] = {
	/* mac */
	0x11, 0x00, 0x00, 0x00, 0x00, 0x44, 0x11, 0x00,
	0x00, 0x00, 0x00, 0x33, 0x08, 0x00,
	/* ipv4 len 52 , rtt - 1*/
	0x45, 0x00, 0x00, 0x34, 0x51, 0x1d, 0x40, 0x00,
	0x3f, 0x11, 0xc2, 0x83, 0x0a, 0x0a, 0x0a, 0x02,
	0x0a, 0x0a, 0x0a, 0x03,
	/* udp len 8 */
	0x28, 0x2b, 0x30, 0x39, 0x00, 0x20, 0x7f, 0x31,
	/*payload len 24*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,};

/* test unmatched skb, this skb will match one rule and be nated */
static int snfp_test_thread_3(void *arg)
{
	int ret;
	bool test_ret;
	int out_index = 0;
	struct sk_buff *skb;
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;
	int in_ifindex = 1;
	int out_ifindex = 2;
	int out_ipaifindex = 0x1;
	int in_ipaifindex = 0x6;

	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	u32 hash, hash_inv;
	/* length=80, ipv6 with an extension router header */

	test_result = SFP_TEST_INPROGRESS;

	make_ct_tuple(&g_tuples[0], &ct);
	sysctl_net_sfp_tether_scheme = 1;
	sysctl_net_sfp_enable = 1;
	new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
	if (!new_sfp_ct)
		return -ENOMEM;

	FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
	memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

	sfp_ct_init(&ct, new_sfp_ct);
	sfp_test_init_mac(new_sfp_ct);

	/* Out net device id */
	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = in_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = out_ipaifindex;

	hash = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_ORIGINAL] = hash;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash]);

	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = out_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = in_ipaifindex;

	hash_inv = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_REPLY] = hash_inv;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash_inv]);

	sfp_fwd_hash_add(new_sfp_ct);

	skb = dev_alloc_skb(66);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, 66);
	memcpy(skb->data, testdata1, ARRAY_SIZE(testdata1));

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);
	skb_reset_network_header(skb);

	ret = soft_fastpath_process(SFP_INTERFACE_LTE,
				    (void *)skb, NULL, NULL, &out_index);

	pr_info("skb ret %d out_index %d - %42ph\n",
		ret, out_index, skb->data);

	test_ret = (memcmp(testdata_nated, skb->data, 66) == 0) &&
		   (out_index == 2);

	sfp_clear_fwd_table(out_ifindex);
	if (test_ret) {
		test_result = SFP_TEST_PASS;
		pr_info("%s test pass\n", __func__);
	} else {
		test_result = SFP_TEST_FAIL;
		pr_info("%s test fail\n", __func__);
	}
	return 0;
}

/* test unmatched skb, this skb does not match any rules and remain the same */
static int snfp_test_thread_4(void *arg)
{
	int ret;
	bool test_ret;
	int out_index = 0;
	struct sk_buff *skb;
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;
	int in_ifindex = 1;
	int out_ifindex = 2;
	int out_ipaifindex = 0x1;
	int in_ipaifindex = 0x6;

	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	u32 hash, hash_inv;

	test_result = SFP_TEST_INPROGRESS;

	make_ct_tuple(&g_tuples[0], &ct);
	sysctl_net_sfp_tether_scheme = 1;
	sysctl_net_sfp_enable = 1;
	new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
	if (!new_sfp_ct)
		return -ENOMEM;

	FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
	memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

	sfp_ct_init(&ct, new_sfp_ct);
	sfp_test_init_mac(new_sfp_ct);

	/* Out net device id */
	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = in_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = out_ipaifindex;

	hash = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_ORIGINAL] = hash;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash]);

	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = out_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = in_ipaifindex;

	hash_inv = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_REPLY] = hash_inv;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash_inv]);

	sfp_fwd_hash_add(new_sfp_ct);

	skb = dev_alloc_skb(66);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, 66);
	memcpy(skb->data, testdata2, ARRAY_SIZE(testdata2));

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);
	skb_reset_network_header(skb);

	ret = soft_fastpath_process(SFP_INTERFACE_LTE,
				    (void *)skb, NULL, NULL, &out_index);
	skb_push(skb, ETH_HLEN);

	pr_info("skb ret %d out_index %d - %42ph\n",
		ret, out_index, skb->data);
	test_ret = (memcmp(testdata2, skb->data, 66) == 0);

	sfp_clear_fwd_table(out_ifindex);
	if (test_ret) {
		test_result = SFP_TEST_PASS;
		pr_info("%s test pass\n", __func__);
	} else {
		test_result = SFP_TEST_FAIL;
		pr_info("%s test fail\n", __func__);
	}

	return 0;
}

static int snfp_test_thread_5(void *arg)
{
	int ret;
	bool test_ret;
	int out_index = 0;
	struct sk_buff *skb;
	struct sk_buff *copy_skb;
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;
	int in_ifindex = 1;
	int out_ifindex = 2;
	int out_ipaifindex = 0x1;
	int in_ipaifindex = 0x6;
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	u32 hash, hash_inv;
	/* length=80, ipv6 with an extension router header */

	test_result = SFP_TEST_INPROGRESS;

	make_ct_tuple(&g_tuples[0], &ct);
	sysctl_net_sfp_tether_scheme = 1;
	sysctl_net_sfp_enable = 1;
	new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
	if (!new_sfp_ct)
		return -ENOMEM;

	FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
	memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

	sfp_ct_init(&ct, new_sfp_ct);
	sfp_test_init_mac(new_sfp_ct);

	/* Out net device id */
	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = in_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = out_ipaifindex;

	hash = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_ORIGINAL] = hash;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash]);

	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = out_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = in_ipaifindex;

	hash_inv = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_REPLY] = hash_inv;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash_inv]);

	sfp_fwd_hash_add(new_sfp_ct);

	skb = dev_alloc_skb(66);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, 66);
	memcpy(skb->data, testdata1, ARRAY_SIZE(testdata1));

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);
	skb_reset_network_header(skb);

	copy_skb = skb_copy(skb, GFP_ATOMIC);
	if (!copy_skb) {
		pr_err("skb_copy failed,return!\n");
		return -ENOMEM;
	}

	ret = soft_fastpath_process(SFP_INTERFACE_LTE,
				    (void *)skb, NULL, NULL, &out_index);

	pr_info("skb ret %d out_index %d - %42ph\n",
		ret, out_index, skb->data);

	test_ret = (memcmp(testdata_nated, skb->data, 66) == 0) &&
		   (out_index == 2);

	/* we shorten the timeout length for test */
	mod_timer(&new_sfp_ct->timeout, jiffies + 2 * HZ);
	/* after 3 seconds, the rule should expired */
	msleep(3000);

	ret = soft_fastpath_process(SFP_INTERFACE_LTE,
				    (void *)copy_skb, NULL, NULL, &out_index);
	skb_push(copy_skb, ETH_HLEN);

	test_ret = test_ret && (memcmp(testdata1, copy_skb->data, 66) == 0);

	pr_info("skb ret %d out_index %d - %42ph\n",
		ret, out_index, copy_skb->data);

	sfp_clear_fwd_table(out_ifindex);
	if (test_ret) {
		test_result = SFP_TEST_PASS;
		pr_info("%s test pass\n", __func__);
	} else {
		test_result = SFP_TEST_FAIL;
		pr_info("%s test fail\n", __func__);
	}

	return 0;
}

static int snfp_test_thread_6(void *arg)
{
	int ret;
	bool test_ret;
	int out_index = 0;
	struct sk_buff *skb;
	struct sk_buff *copy_skb1, *copy_skb2;
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;
	int in_ifindex = 1;
	int out_ifindex = 2;
	int out_ipaifindex = 0x1;
	int in_ipaifindex = 0x6;
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	u32 hash, hash_inv;
	/* length=80, ipv6 with an extension router header */

	test_result = SFP_TEST_INPROGRESS;

	make_ct_tuple(&g_tuples[0], &ct);
	sysctl_net_sfp_tether_scheme = 1;
	sysctl_net_sfp_enable = 1;
	new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
	if (!new_sfp_ct)
		return -ENOMEM;

	FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
	memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

	sfp_ct_init(&ct, new_sfp_ct);
	sfp_test_init_mac(new_sfp_ct);

	/* Out net device id */
	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = in_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = out_ipaifindex;

	hash = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_ORIGINAL] = hash;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash]);

	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = out_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = in_ipaifindex;

	hash_inv = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_REPLY] = hash_inv;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash_inv]);

	sfp_fwd_hash_add(new_sfp_ct);

	skb = dev_alloc_skb(66);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, 66);
	memcpy(skb->data, testdata1, ARRAY_SIZE(testdata1));

	copy_skb1 = skb_copy(skb, GFP_ATOMIC);
	if (!copy_skb1) {
		pr_err("skb_copy1 failed,return!\n");
		return -ENOMEM;
	}

	copy_skb2 = skb_copy(skb, GFP_ATOMIC);
	if (!copy_skb2) {
		pr_err("skb_copy2 failed,return!\n");
		return -ENOMEM;
	}

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);
	skb_reset_network_header(skb);

	msleep(10000);
	/* renew the timers to 40sec */
	ret = soft_fastpath_process(SFP_INTERFACE_LTE,
				    (void *)skb, NULL, NULL, &out_index);
	test_ret = (memcmp(testdata_nated, skb->data, 66) == 0) &&
		   (out_index == 2);
	pr_info("skb ret %d out_index %d - %42ph\n",
		ret, out_index, skb->data);

	msleep(35000);

	skb_reset_mac_header(copy_skb1);
	skb_pull(copy_skb1, ETH_HLEN);
	skb_reset_network_header(copy_skb1);
	/* the rule not expire yet */
	ret = soft_fastpath_process(SFP_INTERFACE_LTE,
				    (void *)copy_skb1, NULL, NULL, &out_index);
	test_ret = test_ret &&
		   (memcmp(testdata_nated, copy_skb1->data, 66) == 0);
	pr_info("copy_skb1 ret %d out_index %d - %42ph\n",
		ret, out_index, copy_skb1->data);
	msleep(41000);

	skb_reset_mac_header(copy_skb2);
	skb_pull(copy_skb2, ETH_HLEN);
	skb_reset_network_header(copy_skb2);
	/* the rule expire already */
	ret = soft_fastpath_process(SFP_INTERFACE_LTE,
				    (void *)copy_skb2, NULL, NULL, &out_index);
	skb_push(copy_skb2, ETH_HLEN);
	test_ret = test_ret && (memcmp(testdata1, copy_skb2->data, 66) == 0);
	pr_info("copy_skb2 ret %d out_index %d - %42ph\n",
		ret, out_index, copy_skb2->data);

	sfp_clear_fwd_table(out_ifindex);
	if (test_ret) {
		test_result = SFP_TEST_PASS;
		pr_info("%s test pass\n", __func__);
	} else {
		test_result = SFP_TEST_FAIL;
		pr_info("%s test fail\n", __func__);
	}

	return 0;
}

static void ipa_time_out_test(struct timer_list *t)
{
	struct fwd_entry *cur_entry;
	int i;
	int cnt = atomic_read(&fwd_tbl.entry_cnt);
	u8 *v_hash = sfp_get_hash_vtbl(sfp_tbl_id());

	pr_info("%s init\n", __func__);

	cur_entry = (struct fwd_entry *)(v_hash + 2048 * 8);

	for (i = 0; i < cnt; i++) {
		cur_entry->time_stamp = htonl(jiffies + i);
		cur_entry++;
	}
}

static void setup_ipa_timer(void)
{
	timer_setup(&ipa_timer,
		    ipa_time_out_test,
		    (unsigned long)NULL);

	ipa_timer.expires = jiffies + 20 * HZ;
	add_timer(&ipa_timer);
}

static void sfp_mgr_hash_test_init(struct nf_conn *ct,
				   struct sfp_conn *new_sfp_ct)
{
	u32 in_ifindex = 2, out_ifindex = 5;
	u32 in_ipaifindex = 1, out_ipaifindex = 1;
	u32 hash, hash_inv;
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;

	sfp_ct_init(ct, new_sfp_ct);

	sfp_test_init_mac(new_sfp_ct);

	/* Out net device id */
	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = in_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = out_ipaifindex;

	rcu_read_lock_bh();
	hash = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_ORIGINAL] = hash;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash]);

	tuple_hash = &new_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = out_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = in_ipaifindex;

	hash_inv = sfp_hash_conntrack(&tuple_hash->tuple);
	new_sfp_ct->hash[IP_CT_DIR_REPLY] = hash_inv;
	hlist_add_head_rcu(&tuple_hash->entry_lst,
			   &mgr_fwd_entries[hash_inv]);

	FP_PRT_DBG(FP_PRT_WARN,
		   "add mgr tbl, orig_hash[%u], reply_hash[%u]\n",
		    hash, hash_inv);
	rcu_read_unlock_bh();
}

static void sfp_ipa_timer_test(void)
{
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	int i;

	g_tuples[0].s_port = 10000;
	setup_ipa_timer();
	for (i = 0; i < 4; i++) {
		make_ct_tuple(&g_tuples[0], &ct);
		g_tuples[0].s_port++;
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		sfp_ipa_hash_add(new_sfp_ct);
	}
}

static void sfp_ipa_timer_test_2(void)
{
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	int i;

	setup_ipa_timer();
	for (i = 0; i < ARRAY_SIZE(g_tuples); i++) {
		make_ct_tuple(&g_tuples[i], &ct);
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		sfp_ipa_hash_add(new_sfp_ct);

		if (i == 3)
			msleep(60 * 1000);
	}
}

static void sfp_ipa_tbl_add_delete(void)
{
	struct sfp_conn *new_sfp_ct, *old_sfp_ct;
	struct nf_conn ct;
	int i;
	const struct sfp_mgr_fwd_tuple_hash *f_entry;
	u32 hash;

	for (i = 0; i < 5; i++) {
		make_ct_tuple(&g_tuples[0], &ct);
		g_tuples[0].s_port++;
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		/*stop timer in this case*/
		del_timer(&new_sfp_ct->timeout);

		sfp_ipa_hash_add(new_sfp_ct);

		if (i == 1)
			old_sfp_ct = new_sfp_ct;

		if (i == 3) {
			f_entry = &old_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
			hash = old_sfp_ct->hash[IP_CT_DIR_ORIGINAL];
			sfp_ipa_fwd_delete(f_entry, hash);
			f_entry = &old_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
			hash = old_sfp_ct->hash[IP_CT_DIR_REPLY];
			sfp_ipa_fwd_delete(f_entry, hash);
		}
	}
	msleep(10000);
	sfp_ipa_fwd_clear();
	clear_sfp_mgr_table();
}

static void sfp_ipa_tbl_hash_collision(void)
{
	struct sfp_conn *new_sfp_ct, *old_sfp_ct;
	struct nf_conn ct;
	int i;
	const struct sfp_mgr_fwd_tuple_hash *f_entry;
	u32 hash;

	for (i = 0; i < ARRAY_SIZE(g_tuples); i++) {
		make_ct_tuple(&g_tuples[i], &ct);
		FP_PRT_DBG(FP_PRT_ERR, "%d\n", i);

		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		/*stop timer in this case*/
		del_timer(&new_sfp_ct->timeout);

		sfp_ipa_hash_add(new_sfp_ct);

		msleep(20);
		if (i == 3)
			old_sfp_ct = new_sfp_ct;

		if (i == 5) {
			FP_PRT_DBG(FP_PRT_ERR, "delete\n");
			f_entry = &old_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
			hash = old_sfp_ct->hash[IP_CT_DIR_ORIGINAL];
			sfp_ipa_fwd_delete(f_entry, hash);
			f_entry = &old_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
			hash = old_sfp_ct->hash[IP_CT_DIR_REPLY];
			sfp_ipa_fwd_delete(f_entry, hash);
		}
	}

	msleep(10000);
	sfp_ipa_fwd_clear();
	clear_sfp_mgr_table();
}

static void sfp_ipa_tbl_add_2048(int n)
{
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	int i;

	FP_PRT_DBG(FP_PRT_ERR, "test %d\n", n);
	g_tuples[0].s_port = 10000;
	for (i = 0; i < n; i++) {
		make_ct_tuple(&g_tuples[0], &ct);
		g_tuples[0].s_port++;
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p, %d\n", new_sfp_ct, i);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		/*stop timer in this case*/
		del_timer(&new_sfp_ct->timeout);

		sfp_ipa_hash_add(new_sfp_ct);
		msleep(20);
	}
	msleep(10000);
	sfp_ipa_fwd_clear();
	clear_sfp_mgr_table();
}

static void sfp_ipa_tbl_add_2048_timer(void)
{
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	int i;

	setup_ipa_timer();
	g_tuples[0].s_port = 10000;
	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		make_ct_tuple(&g_tuples[0], &ct);
		g_tuples[0].s_port++;
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_WARN, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		sfp_ipa_hash_add(new_sfp_ct);
		msleep(20);
	}
	msleep(100 * 1000);

	sfp_ipa_swap_tbl();
}

static int snfp_test_thread_10(void *arg)
{
	sfp_ipa_tbl_add_delete();
	return 0;
}

static int snfp_test_thread_11(void *arg)
{
	sfp_ipa_timer_test();
	return 0;
}

static int snfp_test_thread_12(void *arg)
{
	sfp_ipa_tbl_add_2048(2048);

	return 0;
}

static int snfp_test_thread_13(void *arg)
{
	sfp_ipa_tbl_hash_collision();
	return 0;
}

static int snfp_test_thread_14(void *arg)
{
	sfp_ipa_timer_test_2();
	return 0;
}

static int snfp_test_thread_15(void *arg)
{
	sfp_ipa_tbl_add_2048_timer();
	return 0;
}

static int snfp_test_thread_n(void *arg)
{
	FP_PRT_DBG(FP_PRT_WARN, "Sfp test doesn't support > 15\n");

	return 0;
}

int sfp_test_init(int count)
{
	if (count == 1) {
		test_count = 10;
		snfp_test_taskn = kthread_run(snfp_test_thread_1,
					      NULL, "snfp_test_1");
		if (IS_ERR(snfp_test_taskn))
			return 1;
	}

	if (count == 2) {
		snfp_test_taskn = kthread_run(snfp_test_thread_2,
					      NULL, "snfp_test_2");
		if (IS_ERR(snfp_test_taskn))
			return 1;
	}

	if (count == 3) {
		snfp_test_taskn = kthread_run(snfp_test_thread_3,
					      NULL, "snfp_test_3");
		if (IS_ERR(snfp_test_taskn))
			return 1;
	}

	if (count == 4) {
		snfp_test_taskn = kthread_run(snfp_test_thread_4,
					      NULL, "snfp_test_4");
		if (IS_ERR(snfp_test_taskn))
			return 1;
	}

	if (count == 5) {
		snfp_test_taskn = kthread_run(snfp_test_thread_5,
					      NULL, "snfp_test_5");
		if (IS_ERR(snfp_test_taskn))
			return 1;
	}

	if (count == 6) {
		snfp_test_taskn = kthread_run(snfp_test_thread_6,
					      NULL, "snfp_test_6");
		if (IS_ERR(snfp_test_taskn))
			return 1;
	}

	if (!get_sfp_tether_scheme()) {
		if (count == 10) {
			snfp_test_taskn = kthread_run(snfp_test_thread_10,
						      NULL,
						      "ipa_hash_init_delete");
			if (IS_ERR(snfp_test_taskn))
				return 1;
		}

		if (count == 11) {
			snfp_test_taskn = kthread_run(snfp_test_thread_11,
						      NULL,
					      "ipa_hash_timer");
			if (IS_ERR(snfp_test_taskn))
				return 1;
		}

		if (count == 12) {
			snfp_test_taskn = kthread_run(snfp_test_thread_12,
						      NULL,
						      "ipa_hash_tbl_2048");
			if (IS_ERR(snfp_test_taskn))
				return 1;
		}

		if (count == 13) {
			snfp_test_taskn = kthread_run(snfp_test_thread_13,
						      NULL,
						      "hash_collision");
			if (IS_ERR(snfp_test_taskn))
				return 1;
		}

		if (count == 14) {
			snfp_test_taskn = kthread_run(snfp_test_thread_14,
						      NULL,
						      "timer_test_2");
			if (IS_ERR(snfp_test_taskn))
				return 1;
		}

		if (count == 15) {
			snfp_test_taskn = kthread_run(snfp_test_thread_15,
						      NULL,
						      "add_2048_timer");
			if (IS_ERR(snfp_test_taskn))
				return 1;
		}

		if (count > 15) {
			snfp_test_taskn = kthread_run(snfp_test_thread_n,
						      &count,
						      "ipa_hash_tbl_2048");
			if (IS_ERR(snfp_test_taskn))
				return 1;
		}
	}
	return 0;
}

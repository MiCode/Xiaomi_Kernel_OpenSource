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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <net/route.h>
#include <linux/proc_fs.h>
#include <linux/netfilter/x_tables.h>
#include <linux/rculist.h>
#include <net/netfilter/nf_nat.h>
#include <linux/timer.h>

#include "sfp.h"
#include "sfp_hash.h"

/* Forward entry in fast path */
struct hlist_head sfp_fwd_entries[SFP_ENTRIES_HASH_SIZE];

int add_in_sfp_fwd_table(const struct sfp_mgr_fwd_tuple_hash *fwd_hash_entry,
			 struct sfp_conn *sfp_ct)
{
	u8 status = SFP_FAIL;
	struct sfp_fwd_entry *cur_entry;
	struct sfp_fwd_entry *new_entry;
	u32 hash;

	if (!fwd_hash_entry)
		return -EPERM;

	hash = sfp_hash_conntrack(&fwd_hash_entry->tuple);

	rcu_read_lock_bh();
	hlist_for_each_entry_rcu(cur_entry,
				 &sfp_fwd_entries[hash],
				 entry_lst) {
		if (nf_ct_tuple_equal(&cur_entry->tuple,
				      &fwd_hash_entry->tuple)) {
			status = SFP_OK;
			break;
		}
	}
	rcu_read_unlock_bh();

	if (status == SFP_OK) {
		FP_PRT_DBG(FP_PRT_DEBUG, "fwd exists,return.[%u]\n", hash);
		return -EPERM;
	}

	new_entry = kmalloc(sizeof(*new_entry), GFP_ATOMIC);
	if (!new_entry)
		return -ENOMEM;

	memset(new_entry, 0, sizeof(struct sfp_fwd_entry));
	new_entry->sfp_ct = sfp_ct;
	new_entry->tuple = fwd_hash_entry->tuple;
	new_entry->ssfp_trans_tuple.trans_mac_info =
			fwd_hash_entry->ssfp_fwd_tuple.trans_mac_info;
	new_entry->ssfp_trans_tuple.trans_info =
			fwd_hash_entry->ssfp_fwd_tuple.trans_info;
	new_entry->ssfp_trans_tuple.in_ifindex =
			fwd_hash_entry->ssfp_fwd_tuple.in_ifindex;
	new_entry->ssfp_trans_tuple.out_ifindex =
			fwd_hash_entry->ssfp_fwd_tuple.out_ifindex;
	new_entry->ssfp_trans_tuple.out_ipaifindex =
			fwd_hash_entry->ssfp_fwd_tuple.out_ipaifindex;
	new_entry->ssfp_trans_tuple.count = 0;
	new_entry->ssfp_trans_tuple.fwd_flags =
			fwd_hash_entry->ssfp_fwd_tuple.fwd_flags;
	hlist_add_head_rcu(&new_entry->entry_lst, &sfp_fwd_entries[hash]);
	status = SFP_OK;
	FP_PRT_DBG(FP_PRT_ERR, "add sfp_fwd_entries %s %d [%u]\n",
		   __func__, __LINE__, hash);

	return status;
}

void sfp_fwd_hash_add(struct sfp_conn *sfp_ct)
{
	add_in_sfp_fwd_table(&sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL],
			     sfp_ct);
	add_in_sfp_fwd_table(&sfp_ct->tuplehash[IP_CT_DIR_REPLY],
			     sfp_ct);
}

static void sfp_fwd_entry_free(struct rcu_head *head)
{
	struct sfp_fwd_entry *sfp_fwd_entry;

	sfp_fwd_entry = container_of(head, struct sfp_fwd_entry, rcu);
	kfree(sfp_fwd_entry);
}

int delete_in_sfp_fwd_table(const struct sfp_mgr_fwd_tuple_hash *fwd_hash_entry)
{
	struct sfp_fwd_entry *cur_entry;
	u32 hash;

	if (!fwd_hash_entry)
		return 0;

	rcu_read_lock_bh();
	hash = sfp_hash_conntrack(&fwd_hash_entry->tuple);
	hlist_for_each_entry_rcu(cur_entry,
				 &sfp_fwd_entries[hash],
				 entry_lst) {
		if (nf_ct_tuple_equal(&cur_entry->tuple,
				      &fwd_hash_entry->tuple)) {
			hlist_del_rcu(&cur_entry->entry_lst);
			call_rcu(&cur_entry->rcu, sfp_fwd_entry_free);
		}
	}
	rcu_read_unlock_bh();

	return 0;
}
EXPORT_SYMBOL(delete_in_sfp_fwd_table);

int get_sfp_fwd_entry_count(struct sfp_mgr_fwd_tuple_hash *fwd_hash_entry)
{
	struct sfp_fwd_entry *cur_entry;
	u32 hash;
	int confirm = 0;

	if (!fwd_hash_entry)
		return 0;

	rcu_read_lock_bh();
	hash = sfp_hash_conntrack(&fwd_hash_entry->tuple);

	hlist_for_each_entry_rcu(cur_entry,
				 &sfp_fwd_entries[hash],
				 entry_lst) {
		if (nf_ct_tuple_equal(&cur_entry->tuple,
				      &fwd_hash_entry->tuple)) {
			FP_PRT_DBG(FP_PRT_DEBUG,
				   "SFP_FWD: fwd = %u, cur= %u\n",
				   fwd_hash_entry->ssfp_fwd_tuple.count,
				   cur_entry->ssfp_trans_tuple.count);
			if (cur_entry->ssfp_trans_tuple.count !=
			    fwd_hash_entry->ssfp_fwd_tuple.count) {
				fwd_hash_entry->ssfp_fwd_tuple.count =
				cur_entry->ssfp_trans_tuple.count;
				confirm = 1;
			}
			break;
		}
	}
	rcu_read_unlock_bh();
	return confirm;
}
EXPORT_SYMBOL(get_sfp_fwd_entry_count);

/*clear the whole forward table*/
void clear_sfp_fwd_table(void)
{
	struct sfp_fwd_entry *sfp_fwd_entry;
	int i;

	rcu_read_lock_bh();
	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(sfp_fwd_entry,
					 &sfp_fwd_entries[i],
					 entry_lst) {
			hlist_del_rcu(&sfp_fwd_entry->entry_lst);
			call_rcu(&sfp_fwd_entry->rcu, sfp_fwd_entry_free);
		}
	}
	rcu_read_unlock_bh();
}
EXPORT_SYMBOL(clear_sfp_fwd_table);

static void sfp_mode_timer(struct sfp_conn *ul_sfp_ct)
{
	struct sfp_conn *sfp_ct = ul_sfp_ct;
	u8 l3_proto;

	if (!sfp_ct || sfp_ct->fin_rst_flag > 0)
		return;

	l3_proto = sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.protonum;

	if (l3_proto != IP_L4_PROTO_TCP) {
		unsigned long newtime = jiffies + sysctl_udp_aging_time;

		if (newtime - sfp_ct->timeout.expires >= HZ) {
			spin_lock_bh(&mgr_lock);
			mod_timer(&sfp_ct->timeout, newtime);
			spin_unlock_bh(&mgr_lock);
		}
	}
}

int check_sfp_fwd_table(struct nf_conntrack_tuple *tuple,
			struct sfp_trans_tuple *ret_info)
{
	struct sfp_fwd_entry *curr_entry;
	u32 hash;
	int ret = SFP_FAIL;

	hash = sfp_hash_conntrack(tuple);
	rcu_read_lock_bh();
	hlist_for_each_entry_rcu(curr_entry,
				 &sfp_fwd_entries[hash], entry_lst) {
		if (sfp_ct_nf_tuple_equal(&curr_entry->tuple, tuple)) {
			/* Find the hash fwd entry */
			curr_entry->ssfp_trans_tuple.count++;
			*ret_info = curr_entry->ssfp_trans_tuple;
			tuple->dst.dir = curr_entry->tuple.dst.dir;

			sfp_mode_timer(curr_entry->sfp_ct);
			break;
		}
	}

	if (curr_entry)
		ret =  SFP_OK;

	rcu_read_unlock_bh();

	return ret;
}
EXPORT_SYMBOL(check_sfp_fwd_table);

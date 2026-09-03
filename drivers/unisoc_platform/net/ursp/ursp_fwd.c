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

#include "ursp.h"
#include "ursp_hash.h"
#include "ursp_netlink.h"

struct hlist_head ursp_fwd_entries[URSP_ENTRIES_HASH_SIZE];

int add_in_ursp_fwd_table(struct ursp_conn *ursp_ct)
{
	u8 status = URSP_FAIL;
	struct ursp_fwd_entry *new_entry;
	u32 hash;

	if (!ursp_ct)
		return -EPERM;

	hash = ursp_hash_conntrack(&ursp_ct->tuple);
	new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry)
		return -ENOMEM;

	memset(new_entry, 0, sizeof(struct ursp_fwd_entry));
	new_entry->ursp_ct = ursp_ct;

	ursp_fwd_entry_timer_init(new_entry);

	FP_PRT_DBG(FP_PRT_DEBUG, "add ursp_fwd_entries [%u]\n", hash);
	FP_PRT_TUPLE_INFO(FP_PRT_DEBUG, &new_entry->ursp_ct->tuple);
	hlist_add_head_rcu(&new_entry->entry_lst, &ursp_fwd_entries[hash]);
	ursp_notify(ursp_ct, ESTABLISH_EVENT);
	status = URSP_OK;

	return status;
}

void ursp_fwd_entry_free(struct rcu_head *head)
{
	struct ursp_fwd_entry *ursp_fwd_entry;

	ursp_fwd_entry = container_of(head, struct ursp_fwd_entry, rcu);
	kfree(ursp_fwd_entry);
}
EXPORT_SYMBOL(ursp_fwd_entry_free);

int delete_in_ursp_fwd_table(struct ursp_conn *ursp_ct)
{
	struct ursp_fwd_entry *curr_entry;

	u32 hash;

	if (!ursp_ct)
		return 0;

	rcu_read_lock_bh();

	hash = ursp_hash_conntrack(&ursp_ct->tuple);

	hlist_for_each_entry_rcu(curr_entry,
				 &ursp_fwd_entries[hash],
				 entry_lst) {
		if (ursp_ct_tuple_equal(curr_entry,
					&ursp_ct->tuple)) {
			FP_PRT_DBG(FP_PRT_DEBUG, "delete ursp_fwd_entries [%u]\n", hash);
			FP_PRT_TUPLE_INFO(FP_PRT_DEBUG, &curr_entry->ursp_ct->tuple);
			hlist_del_rcu(&curr_entry->entry_lst);
			ursp_notify(ursp_ct, CLOSE_EVENT);
			kfree(ursp_ct);
			call_rcu(&curr_entry->rcu, ursp_fwd_entry_free);
			break;
		}
	}

	rcu_read_unlock_bh();

	return 0;
}
EXPORT_SYMBOL(delete_in_ursp_fwd_table);

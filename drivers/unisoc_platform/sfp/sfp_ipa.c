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
#include "sfp_ipa.h"

#define IPA_HASH_ITEM_LEN 8
#define IPA_HASH_TABLE_SIZE (IPA_HASH_ITEM_LEN * SFP_ENTRIES_HASH_SIZE)

#if IS_ENABLED(CONFIG_UNISOC_SIPA_V3)
#define IPA_HASH_FWD_ENTRY_SIZE 120
#else
#define IPA_HASH_FWD_ENTRY_SIZE 96
#endif

#define IPA_DEFAULT_NUM 4096
#define IPA_DEFAULT_INCR (128 * IPA_HASH_FWD_ENTRY_SIZE)
#define DEFAULT_IPA_TBL_SIZE (IPA_HASH_TABLE_SIZE +\
	IPA_DEFAULT_NUM * IPA_HASH_FWD_ENTRY_SIZE)
#define ipa_tmo(a, b) ((long)(b) == (long)(a))
#define TCP_CT(ct) (\
	(ct)->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.protonum ==\
	IPPROTO_TCP)

#define IPA_UPD_TBL_TIMER (10 * HZ)
#define BIT64TO40(x) ((x) & 0x000000ffffffffffULL)

static atomic_t tbl_id = ATOMIC_INIT(0);
#define NEW_TBL_ID (atomic_read(&tbl_id))

struct sfp_fwd_hash_tbl fwd_tbl;
#define IPA_HASH_COLLISION			0
#define IPA_HASH_APPEND			1
#define IPA_HASH_THRESHOLD		100
static bool fwd_tbl_init_flag;

int sfp_tbl_id(void)
{
	return NEW_TBL_ID == 0 ? T1 : T0;
}

static inline void sfp_swap_tbl_id(void)
{
	if (NEW_TBL_ID == T0)
		atomic_set(&tbl_id, T1);
	else
		atomic_set(&tbl_id, T0);
}

static inline int sfp_entry_cnt(void)
{
	return atomic_read(&fwd_tbl.entry_cnt);
}

int sfp_ipa_fwd_add(enum ip_conntrack_dir dir, struct sfp_conn *sfp_ct)
{
	struct sfp_mgr_fwd_tuple_hash *fwd_hash_entry;
	u8 status = SFP_FAIL;
	struct sfp_fwd_entry *cur_entry;
	struct sfp_fwd_entry *new_entry;
	u32 hash;

	fwd_hash_entry = &sfp_ct->tuplehash[dir];

	hash = sfp_ct->hash[dir];

	hlist_for_each_entry(cur_entry,
			     &fwd_tbl.sfp_fwd_entries[hash],
			     entry_lst) {
		if (nf_ct_tuple_equal(&cur_entry->tuple,
				      &fwd_hash_entry->tuple)) {
			status = SFP_OK;
			break;
		}
	}

	if (status == SFP_OK) {
		FP_PRT_DBG(FP_PRT_DEBUG, "fwd exists,return.[%u]\n", hash);
		return -1;
	}

	new_entry = kmalloc(sizeof(*new_entry), GFP_ATOMIC);
	if (!new_entry)
		return -ENOMEM;

	FP_PRT_DBG(FP_PRT_DEBUG, "new fwd entry %p, hash [%u]\n",
		   new_entry, hash);
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

	if (fwd_tbl.op_flag != IPA_HASH_COLLISION &&
	    hlist_empty(&fwd_tbl.sfp_fwd_entries[hash])) {
		fwd_tbl.op_flag = IPA_HASH_APPEND;
		fwd_tbl.append_cnt++;
	} else {
		fwd_tbl.op_flag = IPA_HASH_COLLISION;
	}
	hlist_add_head(&new_entry->entry_lst, &fwd_tbl.sfp_fwd_entries[hash]);
	atomic_inc(&fwd_tbl.entry_cnt);
	fwd_tbl.hash_lst[dir] = hash;

	return SFP_OK;
}

static int sfp_ipa_get_entry(int id, u32 hash, struct fwd_entry **fwd_ptr)
{
	struct hd_hash_tbl *h_tbl;
	u32 haddr;
	int cnt = 0, offset;
	u8 *vaddr = NULL;
	u8 *ptr = sfp_get_hash_vtbl(id);
	dma_addr_t htbl = sfp_get_hash_htbl(id);

	if (!ptr)
		return cnt;

	h_tbl = (struct hd_hash_tbl *)ptr;
	h_tbl += hash;
	haddr = ntohl(h_tbl->haddr);
	cnt = ntohs(h_tbl->num);

	offset = haddr - htbl;
	vaddr = ptr + offset;

	if (cnt)
		*fwd_ptr = (struct fwd_entry *)(vaddr);
	else
		*fwd_ptr = NULL;

	return cnt;
}

static void sfp_set_ipa_hash_tbl(int id, u32 hash, u16 n, dma_addr_t haddr)
{
	struct hd_hash_tbl *h_tbl;
	u8 *ptr = sfp_get_hash_vtbl(id);

	if (!ptr)
		return;

	h_tbl = (struct hd_hash_tbl *)ptr;
	h_tbl += hash;
	if (!n)
		h_tbl->haddr = 0;

	h_tbl->num = htons(n);
}

void sfp_ipa_entry_delete(u32 hash)
{
	struct fwd_entry *fwd_ptr;
	int cnt = sfp_ipa_get_entry(NEW_TBL_ID, hash, &fwd_ptr);

	if (!cnt--) {
		FP_PRT_DBG(FP_PRT_ERR,
			   "%s: delete hash %u fail\n", __func__, hash);
		return;
	}

	FP_PRT_DBG(FP_PRT_INFO,
		   "%s: [%u] cnt %d\n", __func__, hash, cnt);
	sfp_set_ipa_hash_tbl(NEW_TBL_ID, hash, cnt, 0);
}

static void sfp_ipa_fwd_entry_free(struct sfp_fwd_entry *sfp_fwd_entry)
{
	kfree(sfp_fwd_entry);
}

int sfp_ipa_fwd_delete(const struct sfp_mgr_fwd_tuple_hash *fwd_hash_entry,
		       u32 hash)
{
	struct sfp_fwd_entry *cur_entry;
	struct hlist_node *n;

	if (!fwd_hash_entry)
		return 0;

	hlist_for_each_entry_safe(cur_entry, n,
				  &fwd_tbl.sfp_fwd_entries[hash],
				  entry_lst) {
		if (nf_ct_tuple_equal(&cur_entry->tuple,
				      &fwd_hash_entry->tuple)) {
			FP_PRT_DBG(FP_PRT_WARN,
				   "ipa time out: delete ipa fwd entry %p, %d\n",
				   cur_entry, hash);
			hlist_del(&cur_entry->entry_lst);
			atomic_dec(&fwd_tbl.entry_cnt);
			sfp_ipa_fwd_entry_free(cur_entry);
			break;
		}
	}
	return 0;
}

static void sfp_tuple_to_entry(struct fwd_entry *entry,
			       const struct sfp_fwd_entry *cur_entry)
{
	entry->orig_info.src_ip.all[0] = cur_entry->tuple.src.u3.all[0];
	entry->orig_info.src_ip.all[1] = cur_entry->tuple.src.u3.all[1];
	entry->orig_info.src_ip.all[2] = cur_entry->tuple.src.u3.all[2];
	entry->orig_info.src_ip.all[3] = cur_entry->tuple.src.u3.all[3];
	entry->orig_info.dst_ip.all[0] = cur_entry->tuple.dst.u3.all[0];
	entry->orig_info.dst_ip.all[1] = cur_entry->tuple.dst.u3.all[1];
	entry->orig_info.dst_ip.all[2] = cur_entry->tuple.dst.u3.all[2];
	entry->orig_info.dst_ip.all[3] = cur_entry->tuple.dst.u3.all[3];

	entry->orig_info.src_l4_info.all = cur_entry->tuple.src.u.all;
	entry->orig_info.dst_l4_info.all = cur_entry->tuple.dst.u.all;
	entry->orig_info.l3_proto = cur_entry->tuple.src.l3num;
	entry->orig_info.l4_proto = cur_entry->tuple.dst.protonum;

	entry->trans_info = cur_entry->ssfp_trans_tuple.trans_info;
	entry->trans_mac_info = cur_entry->ssfp_trans_tuple.trans_mac_info;

	entry->out_ifindex = cur_entry->ssfp_trans_tuple.out_ipaifindex;
	entry->fwd_flags = 0;
	entry->time_stamp = 0;

#if IS_ENABLED(CONFIG_UNISOC_SIPA_V3)
	/* replace mac header in default */
	entry->mac_info_opts = 1;

	/* no flowctl in default */
	entry->pkt_drop_th = 0;
	entry->pkt_current_idx = 0;
	entry->pkt_current_cnt = 0;
	entry->pkt_drop_cnt = 0;

	entry->reserve1 = 0;
	entry->reserve2 = 0;
#else
	entry->reserve = 0;
#endif
}

void print_hash_tbl(char *p, int len)
{
	int i;
	char str[60] = {0};
	char *off = str;

	if (!p)
		return;

	for (i = 0; i < len; i++) {
		if (i > 0 && i % 16 == 0) {
			FP_PRT_DBG(FP_PRT_DETAIL, "%s", str);
			memset(str, 0, sizeof(str));
			off = str;
		}
		off += sprintf(off, "%02hhx ", *(p + i));
	}
	FP_PRT_DBG(FP_PRT_DETAIL, "%s\n\n", str);
}

static void sfp_dma_hash_tbl_init(u8 *ipa_tbl_ptr, int n)
{
	struct sfp_fwd_entry *cur_entry;
	struct hd_hash_tbl *hash_tbl;
	int cnt = 0, node_cnt;
	u8 *offset;
	dma_addr_t haddr;
	dma_addr_t phy_addr = sfp_get_hash_htbl(NEW_TBL_ID);
	int i;

	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		hash_tbl = (struct hd_hash_tbl *)ipa_tbl_ptr + i;
		node_cnt = 0;
		hlist_for_each_entry(cur_entry,
				     &fwd_tbl.sfp_fwd_entries[i],
				     entry_lst) {
			offset = ipa_tbl_ptr + IPA_HASH_TABLE_SIZE
				+ cnt * IPA_HASH_FWD_ENTRY_SIZE;
			haddr = phy_addr + IPA_HASH_TABLE_SIZE
				+ cnt * IPA_HASH_FWD_ENTRY_SIZE;
			if (cnt++ >= n) {
				FP_PRT_DBG(FP_PRT_ERR,
					   "IPA: fwd entry number err!\n");
				return;
			}

			FP_PRT_DBG(FP_PRT_INFO,
				   "IPA: hash %d, idx %d, %p, %llx, %d\n",
				   i, node_cnt, offset, (u64)haddr, cnt);

			if (!hash_tbl->haddr)
				hash_tbl->haddr = htonl((u32)haddr);

			node_cnt++;
			sfp_tuple_to_entry((struct fwd_entry *)offset,
					   cur_entry);

			if (fp_dbg_lvl & FP_PRT_DETAIL)
				print_hash_tbl((char *)offset,
					       IPA_HASH_FWD_ENTRY_SIZE);
		}

		if (node_cnt) {
			char *p = (char *)hash_tbl;

			hash_tbl->num = htons(node_cnt);
			FP_PRT_DBG(FP_PRT_DETAIL,
				   "IPA Hash Tbl: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				   *p, *(p + 1), *(p + 2), *(p + 3),
				   *(p + 4), *(p + 5), *(p + 6), *(p + 7));
		}
	}
}

void sfp_ipa_swap_tbl(void)
{
#if IS_ENABLED(CONFIG_UNISOC_SIPA_V3)
	sipa_swap_hash_table(&fwd_tbl.ipa_tbl_mgr.tbl[NEW_TBL_ID].sipa_tbl);
#endif
	FP_PRT_DBG(FP_PRT_DEBUG,
		   "##swap tbl %llx to ipa, depth %d, index %d\n",
		   fwd_tbl.ipa_tbl_mgr.tbl[NEW_TBL_ID].sipa_tbl.tbl_phy_addr,
		   fwd_tbl.ipa_tbl_mgr.tbl[NEW_TBL_ID].sipa_tbl.depth,
		   NEW_TBL_ID);

	sfp_swap_tbl_id();
}

static void sfp_set_hash_entry(int hash, u8 *vptr, u32 offset, int n)
{
	struct hd_hash_tbl *hash_tbl;
	struct fwd_entry *hash_entry;
	struct sfp_fwd_entry *cur_entry;
	dma_addr_t htbl = sfp_get_hash_htbl(NEW_TBL_ID);
	dma_addr_t haddr;

	hash_tbl = (struct hd_hash_tbl *)vptr;
	hash_tbl += hash;
	hash_tbl->num = htons(1);

	haddr = htbl + offset;
	hash_tbl->haddr = htonl(haddr);

	FP_PRT_DBG(FP_PRT_INFO, "%s [%d] %p, %llx, %x, %d\n",
		   __func__, hash, hash_tbl, (u64)haddr, offset, n);

	cur_entry = hlist_entry(fwd_tbl.sfp_fwd_entries[hash].first,
				struct sfp_fwd_entry,
				entry_lst);
	hash_entry = (struct fwd_entry *)(vptr + offset);

	FP_PRT_DBG(FP_PRT_INFO,
		   "cur_entry %p %p\n", cur_entry, hash_entry);
	sfp_tuple_to_entry(hash_entry, cur_entry);

	if (fp_dbg_lvl & FP_PRT_DETAIL)
		print_hash_tbl((char *)hash_entry,
			       IPA_HASH_FWD_ENTRY_SIZE);
}

static void sfp_ipa_append_hash(void)
{
	u8 *vptr = sfp_get_hash_vtbl(NEW_TBL_ID);
	u32 offset;
	int hash;
	int n = sfp_entry_cnt();

	hash = fwd_tbl.hash_lst[IP_CT_DIR_ORIGINAL];
	offset = IPA_HASH_TABLE_SIZE +
		(n - 2) * IPA_HASH_FWD_ENTRY_SIZE;
	sfp_set_hash_entry(hash, vptr, offset, n);

	hash = fwd_tbl.hash_lst[IP_CT_DIR_REPLY];
	offset += IPA_HASH_FWD_ENTRY_SIZE;
	sfp_set_hash_entry(hash, vptr, offset, n);
}

static void sfp_ipa_hash_tbl_sync(void)
{
	sfp_ipa_append_hash();
}

static int sfp_ipa_update_hash_slow(void)
{
	int n, true_size;
	u8 *ipa_tbl_ptr;

	n = sfp_entry_cnt();
	ipa_tbl_ptr = sfp_get_hash_vtbl(NEW_TBL_ID);

	true_size = IPA_HASH_TABLE_SIZE + n * IPA_HASH_FWD_ENTRY_SIZE;

	FP_PRT_DBG(FP_PRT_INFO, "%s entries %d\n", __func__, n);
	memset(ipa_tbl_ptr, 0, true_size);

	sfp_dma_hash_tbl_init(ipa_tbl_ptr, n);

	/* Clear the collision */
	fwd_tbl.op_flag = IPA_HASH_APPEND;

	return SFP_OK;
}

int sfp_ipa_hash_add(struct sfp_conn *sfp_ct)
{
	if (atomic_read(&fwd_tbl.entry_cnt) >= IPA_DEFAULT_NUM - 1) {
		FP_PRT_DBG(FP_PRT_ERR,
			   "out of entry mem!\n");

		return -1;
	}

	spin_lock_bh(&fwd_tbl.sp_lock);
	if (sfp_ipa_fwd_add(IP_CT_DIR_ORIGINAL, sfp_ct) ==  SFP_OK &&
	    sfp_ipa_fwd_add(IP_CT_DIR_REPLY, sfp_ct) == SFP_OK) {
		if (fwd_tbl.op_flag == IPA_HASH_COLLISION ||
		    fwd_tbl.append_cnt > IPA_HASH_THRESHOLD) {
			FP_PRT_DBG(FP_PRT_DEBUG, "slow path!\n");
			sfp_ipa_update_hash_slow();
			sfp_ipa_swap_tbl();
			sfp_ipa_update_hash_slow();
			fwd_tbl.append_cnt = 0;
		} else if (fwd_tbl.op_flag == IPA_HASH_APPEND) {
			FP_PRT_DBG(FP_PRT_DEBUG,
				   "append %d!\n", fwd_tbl.append_cnt);
			sfp_ipa_append_hash();
			sfp_ipa_swap_tbl();
			sfp_ipa_hash_tbl_sync();
		}
	}
	spin_unlock_bh(&fwd_tbl.sp_lock);
	return 0;
}

static bool sfp_get_ipa_ts(enum ip_conntrack_dir dir,
			   struct sfp_conn *sfp_ct, u32 *ts)
{
	struct fwd_entry *fwd_ptr = NULL;
	int i, cnt;
	int id = sfp_tbl_id();

	cnt = sfp_ipa_get_entry(id, sfp_ct->hash[dir], &fwd_ptr);

	if (!cnt) {
		FP_PRT_DBG(FP_PRT_ERR,
			   "%s no hash entry [%u]!\n",
			   __func__, sfp_ct->hash[dir]);
		return false;
	}

	for (i = 0; i < cnt && fwd_ptr; i++, fwd_ptr++) {
		if (sfp_ipa_tuple_equal(&sfp_ct->tuplehash[dir].tuple,
					fwd_ptr)) {
			*ts = ntohl(fwd_ptr->time_stamp);
			return true;
		}
	}

	return false;
}

bool sfp_ipa_tbl_timeout(struct sfp_conn *sfp_ct)
{
	u32 ts, ts_orig_new, ts_repl_new;

#if IS_ENABLED(CONFIG_UNISOC_SIPA_V3)
	/* before we read timestamp,
	 * we need to flush ipa cache back to ddr,
	 * ensure to get the latest entry
	 */
	if (sipa_hal_set_hash_sync_req()) {
		FP_PRT_DBG(FP_PRT_DEBUG, "fail to check ts\n");
		return true;
	}
#endif

	if (!sfp_get_ipa_ts(IP_CT_DIR_ORIGINAL, sfp_ct, &ts_orig_new) ||
	    !sfp_get_ipa_ts(IP_CT_DIR_REPLY, sfp_ct, &ts_repl_new)) {
		FP_PRT_DBG(FP_PRT_ERR,
			   "get orig ts fail, hash [%u]!\n",
			   sfp_ct->hash[IP_CT_DIR_ORIGINAL]);
		return false;
	}

	ts = sfp_get_ipa_latest_ts(ts_orig_new, ts_repl_new);

	FP_PRT_DBG(FP_PRT_INFO,
		   "%s: sfp_ct %p, ts %d, sfp_ct->ts %d, ts orig %d, ts repl %d\n",
		   __func__, sfp_ct,
		   ntohl(ts), ntohl(sfp_ct->ts), ntohl(ts_orig_new), ntohl(ts_repl_new));

	if (sfp_ct->ts != ts) {
		sfp_ct->ts = ts;
		FP_PRT_DBG(FP_PRT_INFO,
			   "not timeout, hash [%u]!\n",
			   sfp_ct->hash[IP_CT_DIR_ORIGINAL]);
		mod_timer(&sfp_ct->timeout, jiffies + sysctl_udp_aging_time);
		return false;
	}

	return true;
}

static int sfp_ipa_alloc_tbl(int sz)
{
	int ret = 0;
	u8 *v0, *v1;
	dma_addr_t h0, h1;

	v0 = (u8 *)dma_ipa_alloc(NULL, sz, &h0, GFP_KERNEL);

	if (!v0) {
		FP_PRT_DBG(FP_PRT_ERR, "IPA: alloc tbl 0 fail!!\n");
		ret = -1;
		goto exit;
	}

	v1 = (u8 *)dma_ipa_alloc(NULL, sz, &h1, GFP_KERNEL);

	if (!v1) {
		FP_PRT_DBG(FP_PRT_ERR,	"IPA: alloc tbl 1 fail!!\n");
		dma_ipa_free(NULL, sz, v0, h0);
		ret = -1;
		goto exit;
	}

	FP_PRT_DBG(FP_PRT_INFO,
		   "v0 %p, h0 %llx, v1 %p, h1 %llx\n",
		   v0, (u64)h0, v1, (u64)h1);

	fwd_tbl.ipa_tbl_mgr.tbl[T0].h_tbl.v_addr = v0;
	fwd_tbl.ipa_tbl_mgr.tbl[T0].h_tbl.handle = h0;
	fwd_tbl.ipa_tbl_mgr.tbl[T0].h_tbl.sz = sz;
	fwd_tbl.ipa_tbl_mgr.tbl[T0].sipa_tbl.depth = SFP_ENTRIES_HASH_SIZE;
	fwd_tbl.ipa_tbl_mgr.tbl[T0].sipa_tbl.tbl_phy_addr = (u64)h0;

	fwd_tbl.ipa_tbl_mgr.tbl[T1].h_tbl.v_addr = v1;
	fwd_tbl.ipa_tbl_mgr.tbl[T1].h_tbl.handle = h1;
	fwd_tbl.ipa_tbl_mgr.tbl[T1].h_tbl.sz = sz;
	fwd_tbl.ipa_tbl_mgr.tbl[T1].sipa_tbl.depth = SFP_ENTRIES_HASH_SIZE;
	fwd_tbl.ipa_tbl_mgr.tbl[T1].sipa_tbl.tbl_phy_addr = (u64)h1;

	/*zero hash tbl*/
	sfp_clear_all_ipa_tbl();
exit:
	return ret;
}

static inline void sfp_clear_ipa_tbl(int id)
{
	if (fwd_tbl_init_flag)
		memset(fwd_tbl.ipa_tbl_mgr.tbl[id].h_tbl.v_addr,
		       0, IPA_HASH_TABLE_SIZE);
}

void sfp_clear_all_ipa_tbl(void)
{
	sfp_clear_ipa_tbl(T0);
	sfp_clear_ipa_tbl(T1);
}

void sfp_ipa_fwd_clear(void)
{
	struct sfp_fwd_entry *sfp_fwd_entry;
	struct hlist_node *n;
	int i;

	spin_lock_bh(&fwd_tbl.sp_lock);
	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		hlist_for_each_entry_safe(sfp_fwd_entry,
					  n,
					  &fwd_tbl.sfp_fwd_entries[i],
					  entry_lst) {
			hlist_del(&sfp_fwd_entry->entry_lst);
			sfp_ipa_fwd_entry_free(sfp_fwd_entry);
			atomic_dec(&fwd_tbl.entry_cnt);
		}
	}
	sfp_clear_all_ipa_tbl();
	spin_unlock_bh(&fwd_tbl.sp_lock);
	FP_PRT_DBG(FP_PRT_INFO, "%s %d\n", __func__, sfp_entry_cnt());
}

void sfp_destroy_ipa_tbl(void)
{
	dma_ipa_free(NULL,
		     fwd_tbl.ipa_tbl_mgr.tbl[T0].h_tbl.sz,
		     fwd_tbl.ipa_tbl_mgr.tbl[T0].h_tbl.v_addr,
		     fwd_tbl.ipa_tbl_mgr.tbl[T0].h_tbl.handle);

	dma_ipa_free(NULL,
		     fwd_tbl.ipa_tbl_mgr.tbl[T1].h_tbl.sz,
		     fwd_tbl.ipa_tbl_mgr.tbl[T1].h_tbl.v_addr,
		     fwd_tbl.ipa_tbl_mgr.tbl[T1].h_tbl.handle);

	memset(fwd_tbl.ipa_tbl_mgr.tbl, 0,
	       sizeof(fwd_tbl.ipa_tbl_mgr.tbl));
}

static int sfp_init_ipa_tbl(void)
{
	int value;

	value = sfp_ipa_alloc_tbl(DEFAULT_IPA_TBL_SIZE);
	return value;
}

void sfp_ipa_init(void)
{
	int result;

	spin_lock_init(&fwd_tbl.sp_lock);
	atomic_set(&fwd_tbl.entry_cnt, 0);
	fwd_tbl.op_flag = IPA_HASH_APPEND;
	fwd_tbl.append_cnt = 0;

	result = sfp_init_ipa_tbl();
	if (!result)
		fwd_tbl_init_flag = true;

}

bool sfp_ipa_ipv6_check(const struct sk_buff *skb,
			u16 *first, u8 *nexthdr, u32 *off)
{
	struct frag_hdr _frag;
	int err;
	const struct frag_hdr *fh;
	unsigned int ptr = 0;

	err = ipv6_find_hdr(skb, &ptr, NEXTHDR_FRAGMENT, NULL, NULL);
	if (err < 0)
		return false;

	fh = skb_header_pointer(skb, ptr, sizeof(_frag), &_frag);
	if (!fh)
		return false;

	*first = ntohs(fh->frag_off) & ~0x7;
	*nexthdr = fh->nexthdr;

	*off = skb->transport_header - skb->network_header;

	return true;
}

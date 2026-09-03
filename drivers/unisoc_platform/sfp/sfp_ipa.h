/* SPDX-License-Identifier: GPL-2.0-only */
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
#ifndef SFP_IPA_H_
#define SFP_IPA_H_
#include "sfp.h"

static inline void *dma_ipa_alloc(struct device *dev, size_t size,
				  dma_addr_t *dma_handle, gfp_t flag)
{
	return dma_alloc_coherent(sfp_get_ipa_dev(), size, dma_handle, flag);
}

static inline void dma_ipa_free(struct device *dev, size_t size,
				void *cpu_addr, dma_addr_t dma_handle)
{
	dma_free_coherent(sfp_get_ipa_dev(), size, cpu_addr, dma_handle);
}

static inline u8 *sfp_get_hash_vtbl(int id)
{
	return fwd_tbl.ipa_tbl_mgr.tbl[id].h_tbl.v_addr;
}

static inline dma_addr_t sfp_get_hash_htbl(int id)
{
	return fwd_tbl.ipa_tbl_mgr.tbl[id].h_tbl.handle;
}

static inline u32 sfp_get_ipa_latest_ts(u32 t1, u32 t2)
{
	if (t1 >= t2)
		return t1;
	else
		return t2;
}

static inline bool sfp_ipa_tuple_src_equal(const struct nf_conntrack_tuple *t1,
					   const struct fwd_entry *t2)
{
	return (!memcmp(&t1->src.u3, &t2->orig_info.src_ip, 16) &&
		t1->src.u.all == t2->orig_info.src_l4_info.all &&
		t1->src.l3num == t2->orig_info.l3_proto);
}

static inline bool sfp_ipa_tuple_dst_equal(const struct nf_conntrack_tuple *t1,
					   const struct fwd_entry *t2)
{
	return (!memcmp(&t1->dst.u3, &t2->orig_info.dst_ip, 16) &&
		t1->dst.u.all == t2->orig_info.dst_l4_info.all &&
		t1->dst.protonum == t2->orig_info.l4_proto);
}

static inline bool sfp_ipa_tuple_equal(const struct nf_conntrack_tuple *t1,
				       const struct fwd_entry *t2)
{
	return sfp_ipa_tuple_src_equal(t1, t2) &&
	       sfp_ipa_tuple_dst_equal(t1, t2);
}
#endif

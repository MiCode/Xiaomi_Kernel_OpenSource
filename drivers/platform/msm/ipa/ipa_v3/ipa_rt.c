/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/idr.h>
#include "ipa_i.h"
#include "ipahal/ipahal.h"

#define IPA_RT_INDEX_BITMAP_SIZE	(32)
#define IPA_RT_STATUS_OF_ADD_FAILED	(-1)
#define IPA_RT_STATUS_OF_DEL_FAILED	(-1)
#define IPA_RT_STATUS_OF_MDFY_FAILED (-1)

#define IPA_RT_GET_RULE_TYPE(__entry) \
	( \
	((__entry)->rule.hashable) ? \
	(IPA_RULE_HASHABLE) : (IPA_RULE_NON_HASHABLE) \
	)

/**
 * __ipa_generate_rt_hw_rule_v3_0() - generates the routing hardware rule
 * @ip: the ip address family type
 * @entry: routing entry
 * @buf: output buffer, buf == NULL means
 *		caller wants to know the size of the rule as seen
 *		by HW so they did not pass a valid buffer, we will use a
 *		scratch buffer instead.
 *		With this scheme we are going to
 *		generate the rule twice, once to know size using scratch
 *		buffer and second to write the rule to the actual caller
 *		supplied buffer which is of required size
 *
 * Returns:	0 on success, negative on failure
 *
 * caller needs to hold any needed locks to ensure integrity
 *
 */
int __ipa_generate_rt_hw_rule_v3_0(enum ipa_ip_type ip,
		struct ipa3_rt_entry *entry, u8 *buf)
{
	struct ipa3_rt_rule_hw_hdr *rule_hdr;
	const struct ipa_rt_rule *rule =
		(const struct ipa_rt_rule *)&entry->rule;
	u16 en_rule = 0;
	u32 tmp[IPA_RT_FLT_HW_RULE_BUF_SIZE/4];
	u8 *start;
	int pipe_idx;
	struct ipa3_hdr_entry *hdr_entry;
	struct ipa3_hdr_proc_ctx_entry *hdr_proc_entry;

	if (buf == NULL) {
		memset(tmp, 0, IPA_RT_FLT_HW_RULE_BUF_SIZE);
		buf = (u8 *)tmp;
	} else
		if ((long)buf & IPA_HW_RULE_START_ALIGNMENT) {
			IPAERR("buff is not rule start aligned\n");
			return -EPERM;
		}

	start = buf;
	rule_hdr = (struct ipa3_rt_rule_hw_hdr *)buf;
	pipe_idx = ipa3_get_ep_mapping(entry->rule.dst);
	if (pipe_idx == -1) {
		IPAERR_RL("Wrong destination pipe specified in RT rule\n");
		WARN_ON(1);
		return -EPERM;
	}
	if (!IPA_CLIENT_IS_CONS(entry->rule.dst)) {
		IPAERR_RL("No RT rule on IPA_client_producer pipe.\n");
		IPAERR_RL("pipe_idx: %d dst_pipe: %d\n",
				pipe_idx, entry->rule.dst);
		WARN_ON(1);
		return -EPERM;
	}
	rule_hdr->u.hdr.pipe_dest_idx = pipe_idx;
	/* Adding check to confirm still
	 * header entry present in header table or not
	 */

	if (entry->hdr) {
		hdr_entry = ipa3_id_find(entry->rule.hdr_hdl);
		if (!hdr_entry || hdr_entry->cookie != IPA_HDR_COOKIE) {
			IPAERR_RL("Header entry already deleted\n");
			return -EINVAL;
		}
	} else if (entry->proc_ctx) {
		hdr_proc_entry = ipa3_id_find(entry->rule.hdr_proc_ctx_hdl);
		if (!hdr_proc_entry ||
			hdr_proc_entry->cookie != IPA_PROC_HDR_COOKIE) {
			IPAERR_RL("Proc header entry already deleted\n");
			return -EINVAL;
		}
	}


	if (entry->proc_ctx || (entry->hdr && entry->hdr->is_hdr_proc_ctx)) {
		struct ipa3_hdr_proc_ctx_entry *proc_ctx;
		proc_ctx = (entry->proc_ctx) ? : entry->hdr->proc_ctx;
		if ((proc_ctx == NULL) ||
			(proc_ctx->cookie != IPA_PROC_HDR_COOKIE)) {
			rule_hdr->u.hdr.proc_ctx = 0;
			rule_hdr->u.hdr.hdr_offset = 0;
		} else {
			rule_hdr->u.hdr.system =
				!ipa3_ctx->hdr_proc_ctx_tbl_lcl;
			BUG_ON(proc_ctx->offset_entry->offset & 31);
			rule_hdr->u.hdr.proc_ctx = 1;
			rule_hdr->u.hdr.hdr_offset =
				(proc_ctx->offset_entry->offset +
				ipa3_ctx->hdr_proc_ctx_tbl.start_offset) >> 5;
		}
	} else if ((entry->hdr != NULL) &&
		(entry->hdr->cookie == IPA_HDR_COOKIE)) {
		rule_hdr->u.hdr.system = !ipa3_ctx->hdr_tbl_lcl;
		BUG_ON(entry->hdr->offset_entry->offset & 3);
		rule_hdr->u.hdr.proc_ctx = 0;
		rule_hdr->u.hdr.hdr_offset =
				entry->hdr->offset_entry->offset >> 2;
	} else {
		rule_hdr->u.hdr.proc_ctx = 0;
		rule_hdr->u.hdr.hdr_offset = 0;
	}
	BUG_ON(entry->prio & ~0x3FF);
	rule_hdr->u.hdr.priority = entry->prio;
	rule_hdr->u.hdr.retain_hdr = rule->retain_hdr;
	BUG_ON(entry->rule_id & ~0x3FF);
	rule_hdr->u.hdr.rule_id = entry->rule_id;
	buf += sizeof(struct ipa3_rt_rule_hw_hdr);

	if (ipa3_generate_hw_rule(ip, &rule->attrib, &buf, &en_rule)) {
		IPAERR("fail to generate hw rule\n");
		return -EPERM;
	}

	IPADBG_LOW("en_rule 0x%x\n", en_rule);

	rule_hdr->u.hdr.en_rule = en_rule;
	ipa3_write_64(rule_hdr->u.word, (u8 *)rule_hdr);

	if (entry->hw_len == 0) {
		entry->hw_len = buf - start;
	} else if (entry->hw_len != (buf - start)) {
		IPAERR("hw_len differs b/w passes passed=0x%x calc=0x%td\n",
			entry->hw_len, (buf - start));
		return -EPERM;
	}

	return 0;
}

/**
 * ipa_translate_rt_tbl_to_hw_fmt() - translate the routing driver structures
 *  (rules and tables) to HW format and fill it in the given buffers
 * @ip: the ip address family type
 * @rlt: the type of the rules to translate (hashable or non-hashable)
 * @base: the rules body buffer to be filled
 * @hdr: the rules header (addresses/offsets) buffer to be filled
 * @body_ofst: the offset of the rules body from the rules header at
 *  ipa sram
 * @apps_start_idx: the first rt table index of apps tables
 *
 * Returns: 0 on success, negative on failure
 *
 * caller needs to hold any needed locks to ensure integrity
 *
 */
static int ipa_translate_rt_tbl_to_hw_fmt(enum ipa_ip_type ip,
	enum ipa_rule_type rlt, u8 *base, u8 *hdr,
	u32 body_ofst, u32 apps_start_idx)
{
	struct ipa3_rt_tbl_set *set;
	struct ipa3_rt_tbl *tbl;
	struct ipa_mem_buffer tbl_mem;
	u8 *tbl_mem_buf;
	struct ipa3_rt_entry *entry;
	int res;
	u64 offset;
	u8 *body_i;

	set = &ipa3_ctx->rt_tbl_set[ip];
	body_i = base;
	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		if (tbl->sz[rlt] == 0)
			continue;
		if (tbl->in_sys[rlt]) {
			/* only body (no header) with rule-set terminator */
			tbl_mem.size = tbl->sz[rlt] -
				IPA_HW_TBL_HDR_WIDTH + IPA_HW_TBL_WIDTH;
			tbl_mem.base =
				dma_alloc_coherent(ipa3_ctx->pdev, tbl_mem.size,
				&tbl_mem.phys_base, GFP_KERNEL);
			if (!tbl_mem.base) {
				IPAERR_RL("fail to alloc DMA buf of size %d\n",
					tbl_mem.size);
				goto err;
			}
			if (tbl_mem.phys_base & IPA_HW_TBL_SYSADDR_ALIGNMENT) {
				IPAERR("sys rt tbl address is not aligned\n");
				goto align_err;
			}

			/* update the hdr at the right index */
			ipa3_write_64(tbl_mem.phys_base,
					hdr + ((tbl->idx - apps_start_idx) *
					IPA_HW_TBL_HDR_WIDTH));

			tbl_mem_buf = tbl_mem.base;
			memset(tbl_mem_buf, 0, tbl_mem.size);

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_rt_rule_list,
					link) {
				if (IPA_RT_GET_RULE_TYPE(entry) != rlt)
					continue;
				res = ipa3_ctx->ctrl->ipa_generate_rt_hw_rule(
					ip,
					entry,
					tbl_mem_buf);
				if (res) {
					IPAERR_RL("failed to gen HW RT rule\n");
					goto align_err;
				}
				tbl_mem_buf += entry->hw_len;
			}

			/* write the rule-set terminator */
			tbl_mem_buf = ipa3_write_64(0, tbl_mem_buf);

			if (tbl->curr_mem[rlt].phys_base) {
				WARN_ON(tbl->prev_mem[rlt].phys_base);
				tbl->prev_mem[rlt] = tbl->curr_mem[rlt];
			}
			tbl->curr_mem[rlt] = tbl_mem;
		} else {
			offset = body_i - base + body_ofst;
			if (offset & IPA_HW_TBL_LCLADDR_ALIGNMENT) {
				IPAERR("ofst isn't lcl addr aligned %llu\n",
					offset);
				goto err;
			}

			/* update the hdr at the right index */
			ipa3_write_64(IPA_HW_TBL_OFSET_TO_LCLADDR(offset),
				hdr + ((tbl->idx - apps_start_idx) *
				IPA_HW_TBL_HDR_WIDTH));

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_rt_rule_list,
					link) {
				if (IPA_RT_GET_RULE_TYPE(entry) != rlt)
					continue;
				res = ipa3_ctx->ctrl->ipa_generate_rt_hw_rule(
					ip,
					entry,
					body_i);
				if (res) {
					IPAERR_RL("failed to gen HW RT rule\n");
					goto err;
				}
				body_i += entry->hw_len;
			}

			/* write the rule-set terminator */
			body_i = ipa3_write_64(0, body_i);

			/**
			 * advance body_i to next table alignment as local tables
			 * are order back-to-back
			 */
			body_i += IPA_HW_TBL_LCLADDR_ALIGNMENT;
			body_i = (u8 *)((long)body_i &
				~IPA_HW_TBL_LCLADDR_ALIGNMENT);
		}
	}

	return 0;

align_err:
	dma_free_coherent(ipa3_ctx->pdev, tbl_mem.size,
		tbl_mem.base, tbl_mem.phys_base);
err:
	return -EPERM;
}

static void __ipa_reap_sys_rt_tbls(enum ipa_ip_type ip)
{
	struct ipa3_rt_tbl *tbl;
	struct ipa3_rt_tbl *next;
	struct ipa3_rt_tbl_set *set;
	int i;

	set = &ipa3_ctx->rt_tbl_set[ip];
	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		for (i = 0; i < IPA_RULE_TYPE_MAX; i++) {
			if (tbl->prev_mem[i].phys_base) {
				IPADBG_LOW(
				"reaping sys rt tbl name=%s ip=%d rlt=%d\n",
				tbl->name, ip, i);
				dma_free_coherent(ipa3_ctx->pdev,
					tbl->prev_mem[i].size,
					tbl->prev_mem[i].base,
					tbl->prev_mem[i].phys_base);
				memset(&tbl->prev_mem[i], 0,
					sizeof(tbl->prev_mem[i]));
			}
		}
	}

	set = &ipa3_ctx->reap_rt_tbl_set[ip];
	list_for_each_entry_safe(tbl, next, &set->head_rt_tbl_list, link) {
		for (i = 0; i < IPA_RULE_TYPE_MAX; i++) {
			WARN_ON(tbl->prev_mem[i].phys_base != 0);
			if (tbl->curr_mem[i].phys_base) {
				IPADBG_LOW(
				"reaping sys rt tbl name=%s ip=%d rlt=%d\n",
				tbl->name, ip, i);
				dma_free_coherent(ipa3_ctx->pdev,
					tbl->curr_mem[i].size,
					tbl->curr_mem[i].base,
					tbl->curr_mem[i].phys_base);
			}
		}
		list_del(&tbl->link);
		kmem_cache_free(ipa3_ctx->rt_tbl_cache, tbl);
	}
}

/**
 * ipa_alloc_init_rt_tbl_hdr() - allocate and initialize buffers for
 *  rt tables headers to be filled into sram
 * @ip: the ip address family type
 * @hash_hdr: address of the dma buffer for the hashable rt tbl header
 * @nhash_hdr: address of the dma buffer for the non-hashable rt tbl header
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_alloc_init_rt_tbl_hdr(enum ipa_ip_type ip,
	struct ipa_mem_buffer *hash_hdr, struct ipa_mem_buffer *nhash_hdr)
{
	int num_index;
	u64 *hash_entr;
	u64 *nhash_entr;
	int i;

	if (ip == IPA_IP_v4)
		num_index = IPA_MEM_PART(v4_apps_rt_index_hi) -
			IPA_MEM_PART(v4_apps_rt_index_lo) + 1;
	else
		num_index = IPA_MEM_PART(v6_apps_rt_index_hi) -
			IPA_MEM_PART(v6_apps_rt_index_lo) + 1;

	hash_hdr->size = num_index * IPA_HW_TBL_HDR_WIDTH;
	hash_hdr->base = dma_alloc_coherent(ipa3_ctx->pdev, hash_hdr->size,
		&hash_hdr->phys_base, GFP_KERNEL);
	if (!hash_hdr->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", hash_hdr->size);
		goto err;
	}

	nhash_hdr->size = num_index * IPA_HW_TBL_HDR_WIDTH;
	nhash_hdr->base = dma_alloc_coherent(ipa3_ctx->pdev, nhash_hdr->size,
		&nhash_hdr->phys_base, GFP_KERNEL);
	if (!nhash_hdr->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", nhash_hdr->size);
		goto nhash_alloc_fail;
	}

	hash_entr = (u64 *)hash_hdr->base;
	nhash_entr = (u64 *)nhash_hdr->base;
	for (i = 0; i < num_index; i++) {
		*hash_entr = ipa3_ctx->empty_rt_tbl_mem.phys_base;
		*nhash_entr = ipa3_ctx->empty_rt_tbl_mem.phys_base;
		hash_entr++;
		nhash_entr++;
	}

	return 0;

nhash_alloc_fail:
	dma_free_coherent(ipa3_ctx->pdev, hash_hdr->size,
		hash_hdr->base, hash_hdr->phys_base);
err:
	return -ENOMEM;
}

/**
 * ipa_prep_rt_tbl_for_cmt() - preparing the rt table for commit
 *  assign priorities to the rules, calculate their sizes and calculate
 *  the overall table size
 * @ip: the ip address family type
 * @tbl: the rt tbl to be prepared
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_prep_rt_tbl_for_cmt(enum ipa_ip_type ip,
	struct ipa3_rt_tbl *tbl)
{
	struct ipa3_rt_entry *entry;
	u16 prio_i = 1;
	int res;

	tbl->sz[IPA_RULE_HASHABLE] = 0;
	tbl->sz[IPA_RULE_NON_HASHABLE] = 0;

	list_for_each_entry(entry, &tbl->head_rt_rule_list, link) {

		entry->prio = entry->rule.max_prio ?
			IPA_RULE_MAX_PRIORITY : prio_i++;

		if (entry->prio > IPA_RULE_MIN_PRIORITY) {
			IPAERR("cannot allocate new priority for rule\n");
			return -EPERM;
		}

		res = ipa3_ctx->ctrl->ipa_generate_rt_hw_rule(
				ip,
				entry,
				NULL);
		if (res) {
			IPAERR_RL("failed to calculate HW RT rule size\n");
			return -EPERM;
		}

		IPADBG_LOW("RT rule id (handle) %d hw_len %u priority %u\n",
			entry->id, entry->hw_len, entry->prio);

		if (entry->rule.hashable)
			tbl->sz[IPA_RULE_HASHABLE] += entry->hw_len;
		else
			tbl->sz[IPA_RULE_NON_HASHABLE] += entry->hw_len;
	}

	if ((tbl->sz[IPA_RULE_HASHABLE] +
		tbl->sz[IPA_RULE_NON_HASHABLE]) == 0) {
		WARN_ON_RATELIMIT_IPA(1);
		IPAERR_RL("rt tbl %s is with zero total size\n", tbl->name);
	}

	if (tbl->sz[IPA_RULE_HASHABLE])
		tbl->sz[IPA_RULE_HASHABLE] += IPA_HW_TBL_HDR_WIDTH;
	if (tbl->sz[IPA_RULE_NON_HASHABLE])
		tbl->sz[IPA_RULE_NON_HASHABLE] += IPA_HW_TBL_HDR_WIDTH;

	IPADBG_LOW("RT tbl index %u hash_sz %u non-hash sz %u\n", tbl->idx,
		tbl->sz[IPA_RULE_HASHABLE], tbl->sz[IPA_RULE_NON_HASHABLE]);

	return 0;
}

/**
 * ipa_get_rt_tbl_lcl_bdy_size() - calc the overall memory needed on sram
 *  to hold the hashable and non-hashable rt rules tables bodies
 * @ip: the ip address family type
 * @hash_bdy_sz[OUT]: size on local sram for all tbls hashable rules
 * @nhash_bdy_sz[OUT]: size on local sram for all tbls non-hashable rules
 *
 * Return: none
 */
static void ipa_get_rt_tbl_lcl_bdy_size(enum ipa_ip_type ip,
	u32 *hash_bdy_sz, u32 *nhash_bdy_sz)
{
	struct ipa3_rt_tbl_set *set;
	struct ipa3_rt_tbl *tbl;

	*hash_bdy_sz = 0;
	*nhash_bdy_sz = 0;

	set = &ipa3_ctx->rt_tbl_set[ip];
	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		if (!tbl->in_sys[IPA_RULE_HASHABLE] &&
			tbl->sz[IPA_RULE_HASHABLE]) {
			*hash_bdy_sz += tbl->sz[IPA_RULE_HASHABLE];
			*hash_bdy_sz -=  IPA_HW_TBL_HDR_WIDTH;
			/* for table terminator */
			*hash_bdy_sz += IPA_HW_TBL_WIDTH;
			/* align the start of local rule-set */
			*hash_bdy_sz += IPA_HW_TBL_LCLADDR_ALIGNMENT;
			*hash_bdy_sz &= ~IPA_HW_TBL_LCLADDR_ALIGNMENT;
		}
		if (!tbl->in_sys[IPA_RULE_NON_HASHABLE] &&
			tbl->sz[IPA_RULE_NON_HASHABLE]) {
			*nhash_bdy_sz += tbl->sz[IPA_RULE_NON_HASHABLE];
			*nhash_bdy_sz -=  IPA_HW_TBL_HDR_WIDTH;
			/* for table terminator */
			*nhash_bdy_sz += IPA_HW_TBL_WIDTH;
			/* align the start of local rule-set */
			*nhash_bdy_sz += IPA_HW_TBL_LCLADDR_ALIGNMENT;
			*nhash_bdy_sz &= ~IPA_HW_TBL_LCLADDR_ALIGNMENT;
		}
	}
}

/**
 * ipa_generate_rt_hw_tbl_img() - generates the rt hw tbls.
 *  headers and bodies are being created into buffers that will be filled into
 *  the local memory (sram)
 * @ip: the ip address family type
 * @hash_hdr: address of the dma buffer containing hashable rules tbl headers
 * @nhash_hdr: address of the dma buffer containing
 *	non-hashable rules tbl headers
 * @hash_bdy: address of the dma buffer containing hashable local rules
 * @nhash_bdy: address of the dma buffer containing non-hashable local rules
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_generate_rt_hw_tbl_img(enum ipa_ip_type ip,
	struct ipa_mem_buffer *hash_hdr, struct ipa_mem_buffer *nhash_hdr,
	struct ipa_mem_buffer *hash_bdy, struct ipa_mem_buffer *nhash_bdy)
{
	u32 hash_bdy_start_ofst, nhash_bdy_start_ofst;
	u32 apps_start_idx;
	struct ipa3_rt_tbl_set *set;
	struct ipa3_rt_tbl *tbl;
	u32 hash_bdy_sz;
	u32 nhash_bdy_sz;
	int rc = 0;

	if (ip == IPA_IP_v4) {
		nhash_bdy_start_ofst = IPA_MEM_PART(apps_v4_rt_nhash_ofst) -
			IPA_MEM_PART(v4_rt_nhash_ofst);
		hash_bdy_start_ofst = IPA_MEM_PART(apps_v4_rt_hash_ofst) -
			IPA_MEM_PART(v4_rt_hash_ofst);
		apps_start_idx = IPA_MEM_PART(v4_apps_rt_index_lo);
	} else {
		nhash_bdy_start_ofst = IPA_MEM_PART(apps_v6_rt_nhash_ofst) -
			IPA_MEM_PART(v6_rt_nhash_ofst);
		hash_bdy_start_ofst = IPA_MEM_PART(apps_v6_rt_hash_ofst) -
			IPA_MEM_PART(v6_rt_hash_ofst);
		apps_start_idx = IPA_MEM_PART(v6_apps_rt_index_lo);
	}

	if (!ipa3_ctx->rt_idx_bitmap[ip]) {
		IPAERR("no rt tbls present\n");
		rc = -EPERM;
		goto no_rt_tbls;
	}

	if (ipa_alloc_init_rt_tbl_hdr(ip, hash_hdr, nhash_hdr)) {
		IPAERR("fail to alloc and init rt tbl hdr\n");
		rc = -ENOMEM;
		goto no_rt_tbls;
	}

	set = &ipa3_ctx->rt_tbl_set[ip];
	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		if (ipa_prep_rt_tbl_for_cmt(ip, tbl)) {
			rc = -EPERM;
			goto prep_failed;
		}
	}

	ipa_get_rt_tbl_lcl_bdy_size(ip, &hash_bdy_sz, &nhash_bdy_sz);
	IPADBG_LOW("total rt tbl local body sizes: hash %u nhash %u\n",
		hash_bdy_sz, nhash_bdy_sz);

	hash_bdy->size = hash_bdy_sz + IPA_HW_TBL_BLK_SIZE_ALIGNMENT;
	hash_bdy->size &= ~IPA_HW_TBL_BLK_SIZE_ALIGNMENT;
	nhash_bdy->size = nhash_bdy_sz + IPA_HW_TBL_BLK_SIZE_ALIGNMENT;
	nhash_bdy->size &= ~IPA_HW_TBL_BLK_SIZE_ALIGNMENT;

	if (hash_bdy->size) {
		hash_bdy->base = dma_alloc_coherent(ipa3_ctx->pdev,
			hash_bdy->size, &hash_bdy->phys_base, GFP_KERNEL);
		if (!hash_bdy->base) {
			IPAERR("fail to alloc DMA buff of size %d\n",
				hash_bdy->size);
			rc = -ENOMEM;
			goto prep_failed;
		}
		memset(hash_bdy->base, 0, hash_bdy->size);
	}

	if (nhash_bdy->size) {
		nhash_bdy->base = dma_alloc_coherent(ipa3_ctx->pdev,
			nhash_bdy->size, &nhash_bdy->phys_base, GFP_KERNEL);
		if (!nhash_bdy->base) {
			IPAERR("fail to alloc DMA buff of size %d\n",
				hash_bdy->size);
			rc = -ENOMEM;
			goto nhash_bdy_fail;
		}
		memset(nhash_bdy->base, 0, nhash_bdy->size);
	}

	if (ipa_translate_rt_tbl_to_hw_fmt(ip, IPA_RULE_HASHABLE,
		hash_bdy->base, hash_hdr->base,
		hash_bdy_start_ofst, apps_start_idx)) {
		IPAERR("fail to translate hashable rt tbls to hw format\n");
		rc = -EPERM;
		goto translate_fail;
	}
	if (ipa_translate_rt_tbl_to_hw_fmt(ip, IPA_RULE_NON_HASHABLE,
		nhash_bdy->base, nhash_hdr->base,
		nhash_bdy_start_ofst, apps_start_idx)) {
		IPAERR("fail to translate non-hashable rt tbls to hw format\n");
		rc = -EPERM;
		goto translate_fail;
	}

	return rc;

translate_fail:
	if (nhash_bdy->size)
		dma_free_coherent(ipa3_ctx->pdev, nhash_bdy->size,
			nhash_bdy->base, nhash_bdy->phys_base);
nhash_bdy_fail:
	if (hash_bdy->size)
		dma_free_coherent(ipa3_ctx->pdev, hash_bdy->size,
			hash_bdy->base, hash_bdy->phys_base);
prep_failed:
	dma_free_coherent(ipa3_ctx->pdev, hash_hdr->size,
		hash_hdr->base, hash_hdr->phys_base);
	dma_free_coherent(ipa3_ctx->pdev, nhash_hdr->size,
		nhash_hdr->base, nhash_hdr->phys_base);
no_rt_tbls:
	return rc;
}

/**
 * ipa_rt_valid_lcl_tbl_size() - validate if the space allocated for rt tbl
 *  bodies at the sram is enough for the commit
 * @ipt: the ip address family type
 * @rlt: the rule type (hashable or non-hashable)
 *
 * Return: true if enough space available or false in other cases
 */
static bool ipa_rt_valid_lcl_tbl_size(enum ipa_ip_type ipt,
	enum ipa_rule_type rlt, struct ipa_mem_buffer *bdy)
{
	u16 avail;

	if (ipt == IPA_IP_v4)
		avail = (rlt == IPA_RULE_HASHABLE) ?
			IPA_MEM_PART(apps_v4_rt_hash_size) :
			IPA_MEM_PART(apps_v4_rt_nhash_size);
	else
		avail = (rlt == IPA_RULE_HASHABLE) ?
			IPA_MEM_PART(apps_v6_rt_hash_size) :
			IPA_MEM_PART(apps_v6_rt_nhash_size);

	if (bdy->size <= avail)
		return true;

	IPAERR("tbl too big, needed %d avail %d ipt %d rlt %d\n",
		bdy->size, avail, ipt, rlt);
	return false;
}

/**
 * __ipa_commit_rt_v3() - commit rt tables to the hw
 * commit the headers and the bodies if are local with internal cache flushing
 * @ipt: the ip address family type
 *
 * Return: 0 on success, negative on failure
 */
int __ipa_commit_rt_v3(enum ipa_ip_type ip)
{
	struct ipa3_desc desc[5];
	struct ipahal_imm_cmd_register_write reg_write_cmd = {0};
	struct ipahal_imm_cmd_dma_shared_mem  mem_cmd = {0};
	struct ipahal_imm_cmd_pyld *cmd_pyld[5];
	int num_cmd = 0;
	struct ipa_mem_buffer hash_bdy, nhash_bdy;
	struct ipa_mem_buffer hash_hdr, nhash_hdr;
	u32 num_modem_rt_index;
	int rc = 0;
	u32 lcl_hash_hdr, lcl_nhash_hdr;
	u32 lcl_hash_bdy, lcl_nhash_bdy;
	bool lcl_hash, lcl_nhash;
	struct ipahal_reg_fltrt_hash_flush flush;
	struct ipahal_reg_valmask valmask;
	int i;

	memset(desc, 0, sizeof(desc));
	memset(cmd_pyld, 0, sizeof(cmd_pyld));

	if (ip == IPA_IP_v4) {
		num_modem_rt_index =
			IPA_MEM_PART(v4_modem_rt_index_hi) -
			IPA_MEM_PART(v4_modem_rt_index_lo) + 1;
		lcl_hash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_rt_hash_ofst) +
			num_modem_rt_index * IPA_HW_TBL_HDR_WIDTH;
		lcl_nhash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_rt_nhash_ofst) +
			num_modem_rt_index * IPA_HW_TBL_HDR_WIDTH;
		lcl_hash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_rt_hash_ofst);
		lcl_nhash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_rt_nhash_ofst);
		lcl_hash = ipa3_ctx->ip4_rt_tbl_hash_lcl;
		lcl_nhash = ipa3_ctx->ip4_rt_tbl_nhash_lcl;
	} else {
		num_modem_rt_index =
			IPA_MEM_PART(v6_modem_rt_index_hi) -
			IPA_MEM_PART(v6_modem_rt_index_lo) + 1;
		lcl_hash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_rt_hash_ofst) +
			num_modem_rt_index * IPA_HW_TBL_HDR_WIDTH;
		lcl_nhash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_rt_nhash_ofst) +
			num_modem_rt_index * IPA_HW_TBL_HDR_WIDTH;
		lcl_hash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_rt_hash_ofst);
		lcl_nhash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_rt_nhash_ofst);
		lcl_hash = ipa3_ctx->ip6_rt_tbl_hash_lcl;
		lcl_nhash = ipa3_ctx->ip6_rt_tbl_nhash_lcl;
	}

	if (ipa_generate_rt_hw_tbl_img(ip,
		&hash_hdr, &nhash_hdr, &hash_bdy, &nhash_bdy)) {
		IPAERR("fail to generate RT HW TBL image. IP %d\n", ip);
		rc = -EFAULT;
		goto fail_gen;
	}

	if (!ipa_rt_valid_lcl_tbl_size(ip, IPA_RULE_HASHABLE, &hash_bdy)) {
		rc = -EFAULT;
		goto fail_size_valid;
	}
	if (!ipa_rt_valid_lcl_tbl_size(ip, IPA_RULE_NON_HASHABLE,
		&nhash_bdy)) {
		rc = -EFAULT;
		goto fail_size_valid;
	}

	/* flushing ipa internal hashable rt rules cache */
	memset(&flush, 0, sizeof(flush));
	if (ip == IPA_IP_v4)
		flush.v4_rt = true;
	else
		flush.v6_rt = true;
	ipahal_get_fltrt_hash_flush_valmask(&flush, &valmask);
	reg_write_cmd.skip_pipeline_clear = false;
	reg_write_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	reg_write_cmd.offset = ipahal_get_reg_ofst(IPA_FILT_ROUT_HASH_FLUSH);
	reg_write_cmd.value = valmask.val;
	reg_write_cmd.value_mask = valmask.mask;
	cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_REGISTER_WRITE, &reg_write_cmd, false);
	if (!cmd_pyld[num_cmd]) {
		IPAERR("fail construct register_write imm cmd. IP %d\n", ip);
		goto fail_size_valid;
	}
	desc[num_cmd].opcode =
		ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_REGISTER_WRITE);
	desc[num_cmd].pyld = cmd_pyld[num_cmd]->data;
	desc[num_cmd].len = cmd_pyld[num_cmd]->len;
	desc[num_cmd].type = IPA_IMM_CMD_DESC;
	num_cmd++;

	mem_cmd.is_read = false;
	mem_cmd.skip_pipeline_clear = false;
	mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	mem_cmd.size = nhash_hdr.size;
	mem_cmd.system_addr = nhash_hdr.phys_base;
	mem_cmd.local_addr = lcl_nhash_hdr;
	cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
	if (!cmd_pyld[num_cmd]) {
		IPAERR("fail construct dma_shared_mem imm cmd. IP %d\n", ip);
		goto fail_imm_cmd_construct;
	}
	desc[num_cmd].opcode =
		ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
	desc[num_cmd].pyld = cmd_pyld[num_cmd]->data;
	desc[num_cmd].len = cmd_pyld[num_cmd]->len;
	desc[num_cmd].type = IPA_IMM_CMD_DESC;
	num_cmd++;

	mem_cmd.is_read = false;
	mem_cmd.skip_pipeline_clear = false;
	mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	mem_cmd.size = hash_hdr.size;
	mem_cmd.system_addr = hash_hdr.phys_base;
	mem_cmd.local_addr = lcl_hash_hdr;
	cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
	if (!cmd_pyld[num_cmd]) {
		IPAERR("fail construct dma_shared_mem imm cmd. IP %d\n", ip);
		goto fail_imm_cmd_construct;
	}
	desc[num_cmd].opcode =
		ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
	desc[num_cmd].pyld = cmd_pyld[num_cmd]->data;
	desc[num_cmd].len = cmd_pyld[num_cmd]->len;
	desc[num_cmd].type = IPA_IMM_CMD_DESC;
	num_cmd++;

	if (lcl_nhash) {
		mem_cmd.is_read = false;
		mem_cmd.skip_pipeline_clear = false;
		mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		mem_cmd.size = nhash_bdy.size;
		mem_cmd.system_addr = nhash_bdy.phys_base;
		mem_cmd.local_addr = lcl_nhash_bdy;
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR("fail construct dma_shared_mem cmd. IP %d\n",
				ip);
			goto fail_imm_cmd_construct;
		}
		desc[num_cmd].opcode =
			ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
		desc[num_cmd].pyld = cmd_pyld[num_cmd]->data;
		desc[num_cmd].len = cmd_pyld[num_cmd]->len;
		desc[num_cmd].type = IPA_IMM_CMD_DESC;
		num_cmd++;
	}
	if (lcl_hash) {
		mem_cmd.is_read = false;
		mem_cmd.skip_pipeline_clear = false;
		mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		mem_cmd.size = hash_bdy.size;
		mem_cmd.system_addr = hash_bdy.phys_base;
		mem_cmd.local_addr = lcl_hash_bdy;
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR("fail construct dma_shared_mem cmd. IP %d\n",
				ip);
			goto fail_imm_cmd_construct;
		}
		desc[num_cmd].opcode =
			ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
		desc[num_cmd].pyld = cmd_pyld[num_cmd]->data;
		desc[num_cmd].len = cmd_pyld[num_cmd]->len;
		desc[num_cmd].type = IPA_IMM_CMD_DESC;
		num_cmd++;
	}

	if (ipa3_send_cmd(num_cmd, desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
		goto fail_imm_cmd_construct;
	}

	IPADBG_LOW("Hashable HEAD\n");
	IPA_DUMP_BUFF(hash_hdr.base, hash_hdr.phys_base, hash_hdr.size);

	IPADBG_LOW("Non-Hashable HEAD\n");
	IPA_DUMP_BUFF(nhash_hdr.base, nhash_hdr.phys_base, nhash_hdr.size);

	if (hash_bdy.size) {
		IPADBG_LOW("Hashable BODY\n");
		IPA_DUMP_BUFF(hash_bdy.base,
			hash_bdy.phys_base, hash_bdy.size);
	}

	if (nhash_bdy.size) {
		IPADBG_LOW("Non-Hashable BODY\n");
		IPA_DUMP_BUFF(nhash_bdy.base,
			nhash_bdy.phys_base, nhash_bdy.size);
	}

	__ipa_reap_sys_rt_tbls(ip);

fail_imm_cmd_construct:
	for (i = 0 ; i < num_cmd ; i++)
		ipahal_destroy_imm_cmd(cmd_pyld[i]);
fail_size_valid:
	dma_free_coherent(ipa3_ctx->pdev, hash_hdr.size,
		hash_hdr.base, hash_hdr.phys_base);
	dma_free_coherent(ipa3_ctx->pdev, nhash_hdr.size,
		nhash_hdr.base, nhash_hdr.phys_base);

	if (hash_bdy.size)
		dma_free_coherent(ipa3_ctx->pdev, hash_bdy.size,
			hash_bdy.base, hash_bdy.phys_base);

	if (nhash_bdy.size)
		dma_free_coherent(ipa3_ctx->pdev, nhash_bdy.size,
			nhash_bdy.base, nhash_bdy.phys_base);
fail_gen:
	return rc;
}

/**
 * __ipa3_find_rt_tbl() - find the routing table
 *			which name is given as parameter
 * @ip:	[in] the ip address family type of the wanted routing table
 * @name:	[in] the name of the wanted routing table
 *
 * Returns: the routing table which name is given as parameter, or NULL if it
 * doesn't exist
 */
struct ipa3_rt_tbl *__ipa3_find_rt_tbl(enum ipa_ip_type ip, const char *name)
{
	struct ipa3_rt_tbl *entry;
	struct ipa3_rt_tbl_set *set;

	if (strnlen(name, IPA_RESOURCE_NAME_MAX) == IPA_RESOURCE_NAME_MAX) {
		IPAERR_RL("Name too long: %s\n", name);
		return NULL;
	}

	set = &ipa3_ctx->rt_tbl_set[ip];
	list_for_each_entry(entry, &set->head_rt_tbl_list, link) {
		if (!strcmp(name, entry->name))
			return entry;
	}

	return NULL;
}

/**
 * ipa3_query_rt_index() - find the routing table index
 *			which name and ip type are given as parameters
 * @in:	[out] the index of the wanted routing table
 *
 * Returns: the routing table which name is given as parameter, or NULL if it
 * doesn't exist
 */
int ipa3_query_rt_index(struct ipa_ioc_get_rt_tbl_indx *in)
{
	struct ipa3_rt_tbl *entry;

	if (in->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	/* check if this table exists */
	entry = __ipa3_find_rt_tbl(in->ip, in->name);
	if (!entry) {
		mutex_unlock(&ipa3_ctx->lock);
		return -EFAULT;
	}
	in->idx  = entry->idx;
	mutex_unlock(&ipa3_ctx->lock);
	return 0;
}

static struct ipa3_rt_tbl *__ipa_add_rt_tbl(enum ipa_ip_type ip,
		const char *name)
{
	struct ipa3_rt_tbl *entry;
	struct ipa3_rt_tbl_set *set;
	int i;
	int id;
	int max_tbl_indx;

	if (name == NULL) {
		IPAERR_RL("no tbl name\n");
		goto error;
	}

	if (ip == IPA_IP_v4) {
		max_tbl_indx =
			max(IPA_MEM_PART(v4_modem_rt_index_hi),
			IPA_MEM_PART(v4_apps_rt_index_hi));
	} else if (ip == IPA_IP_v6) {
		max_tbl_indx =
			max(IPA_MEM_PART(v6_modem_rt_index_hi),
			IPA_MEM_PART(v6_apps_rt_index_hi));
	} else {
		IPAERR_RL("bad ip family type\n");
		goto error;
	}

	set = &ipa3_ctx->rt_tbl_set[ip];
	/* check if this table exists */
	entry = __ipa3_find_rt_tbl(ip, name);
	if (!entry) {
		entry = kmem_cache_zalloc(ipa3_ctx->rt_tbl_cache, GFP_KERNEL);
		if (!entry) {
			IPAERR("failed to alloc RT tbl object\n");
			goto error;
		}
		/* find a routing tbl index */
		for (i = 0; i < IPA_RT_INDEX_BITMAP_SIZE; i++) {
			if (!test_bit(i, &ipa3_ctx->rt_idx_bitmap[ip])) {
				entry->idx = i;
				set_bit(i, &ipa3_ctx->rt_idx_bitmap[ip]);
				break;
			}
		}
		if (i == IPA_RT_INDEX_BITMAP_SIZE) {
			IPAERR("not free RT tbl indices left\n");
			goto fail_rt_idx_alloc;
		}
		if (i > max_tbl_indx) {
			IPAERR("rt tbl index is above max\n");
			goto fail_rt_idx_alloc;
		}

		INIT_LIST_HEAD(&entry->head_rt_rule_list);
		INIT_LIST_HEAD(&entry->link);
		strlcpy(entry->name, name, IPA_RESOURCE_NAME_MAX);
		entry->set = set;
		entry->cookie = IPA_RT_TBL_COOKIE;
		entry->in_sys[IPA_RULE_HASHABLE] = (ip == IPA_IP_v4) ?
			!ipa3_ctx->ip4_rt_tbl_hash_lcl :
			!ipa3_ctx->ip6_rt_tbl_hash_lcl;
		entry->in_sys[IPA_RULE_NON_HASHABLE] = (ip == IPA_IP_v4) ?
			!ipa3_ctx->ip4_rt_tbl_nhash_lcl :
			!ipa3_ctx->ip6_rt_tbl_nhash_lcl;
		set->tbl_cnt++;
		idr_init(&entry->rule_ids);
		list_add(&entry->link, &set->head_rt_tbl_list);

		IPADBG("add rt tbl idx=%d tbl_cnt=%d ip=%d\n", entry->idx,
				set->tbl_cnt, ip);

		id = ipa3_id_alloc(entry);
		if (id < 0) {
			IPAERR_RL("failed to add to tree\n");
			WARN_ON_RATELIMIT_IPA(1);
			goto ipa_insert_failed;
		}
		entry->id = id;
	}

	return entry;
ipa_insert_failed:
	set->tbl_cnt--;
	list_del(&entry->link);
	idr_destroy(&entry->rule_ids);
fail_rt_idx_alloc:
	entry->cookie = 0;
	kmem_cache_free(ipa3_ctx->rt_tbl_cache, entry);
error:
	return NULL;
}

static int __ipa_del_rt_tbl(struct ipa3_rt_tbl *entry)
{
	enum ipa_ip_type ip = IPA_IP_MAX;
	u32 id;
	struct ipa3_rt_tbl_set *rset;

	if (entry == NULL || (entry->cookie != IPA_RT_TBL_COOKIE)) {
		IPAERR_RL("bad parms\n");
		return -EINVAL;
	}
	id = entry->id;
	if (ipa3_id_find(id) == NULL) {
		IPAERR_RL("lookup failed\n");
		return -EPERM;
	}

	if (entry->set == &ipa3_ctx->rt_tbl_set[IPA_IP_v4])
		ip = IPA_IP_v4;
	else if (entry->set == &ipa3_ctx->rt_tbl_set[IPA_IP_v6])
		ip = IPA_IP_v6;
	else {
		WARN_ON_RATELIMIT_IPA(1);
		return -EPERM;
	}

	rset = &ipa3_ctx->reap_rt_tbl_set[ip];

	idr_destroy(&entry->rule_ids);
	if (entry->in_sys[IPA_RULE_HASHABLE] ||
		entry->in_sys[IPA_RULE_NON_HASHABLE]) {
		list_move(&entry->link, &rset->head_rt_tbl_list);
		clear_bit(entry->idx, &ipa3_ctx->rt_idx_bitmap[ip]);
		entry->set->tbl_cnt--;
		IPADBG("del sys rt tbl_idx=%d tbl_cnt=%d ip=%d\n",
			entry->idx, entry->set->tbl_cnt, ip);
	} else {
		list_del(&entry->link);
		clear_bit(entry->idx, &ipa3_ctx->rt_idx_bitmap[ip]);
		entry->set->tbl_cnt--;
		IPADBG("del rt tbl_idx=%d tbl_cnt=%d ip=%d\n",
			entry->idx, entry->set->tbl_cnt, ip);
		kmem_cache_free(ipa3_ctx->rt_tbl_cache, entry);
	}

	/* remove the handle from the database */
	ipa3_id_remove(id);
	return 0;
}

static int __ipa_rt_validate_rule_id(u16 rule_id)
{
	if (!rule_id)
		return 0;

	if ((rule_id < IPA_RULE_ID_MIN) ||
		(rule_id >= IPA_RULE_ID_MAX)) {
		IPAERR_RL("Invalid rule_id provided 0x%x\n",
			rule_id);
		return -EPERM;
	}

	return 0;
}
static int __ipa_rt_validate_hndls(const struct ipa_rt_rule *rule,
				struct ipa3_hdr_entry **hdr,
				struct ipa3_hdr_proc_ctx_entry **proc_ctx)
{
	if (rule->hdr_hdl && rule->hdr_proc_ctx_hdl) {
		IPAERR_RL("rule contains both hdr_hdl and hdr_proc_ctx_hdl\n");
		return -EPERM;
	}

	if (rule->hdr_hdl) {
		*hdr = ipa3_id_find(rule->hdr_hdl);
		if ((*hdr == NULL) || ((*hdr)->cookie != IPA_HDR_COOKIE)) {
			IPAERR_RL("rt rule does not point to valid hdr\n");
			return -EPERM;
		}
	} else if (rule->hdr_proc_ctx_hdl) {
		*proc_ctx = ipa3_id_find(rule->hdr_proc_ctx_hdl);
		if ((*proc_ctx == NULL) ||
			((*proc_ctx)->cookie != IPA_PROC_HDR_COOKIE)) {

			IPAERR_RL("rt rule does not point to valid proc ctx\n");
			return -EPERM;
		}
	}

	return 0;
}

static int __ipa_create_rt_entry(struct ipa3_rt_entry **entry,
		const struct ipa_rt_rule *rule,
		struct ipa3_rt_tbl *tbl, struct ipa3_hdr_entry *hdr,
		struct ipa3_hdr_proc_ctx_entry *proc_ctx,
		u16 rule_id, bool user)
{
	int id;

	*entry = kmem_cache_zalloc(ipa3_ctx->rt_rule_cache, GFP_KERNEL);
	if (!*entry) {
		IPAERR("failed to alloc RT rule object\n");
		goto error;
	}
	INIT_LIST_HEAD(&(*entry)->link);
	(*(entry))->cookie = IPA_RT_RULE_COOKIE;
	(*(entry))->rule = *rule;
	(*(entry))->tbl = tbl;
	(*(entry))->hdr = hdr;
	(*(entry))->proc_ctx = proc_ctx;
	if (rule_id) {
		id = rule_id;
		(*(entry))->rule_id_valid = 1;
	} else {
		id = ipa3_alloc_rule_id(&tbl->rule_ids);
		if (id < 0) {
			IPAERR_RL("failed to allocate rule id\n");
			WARN_ON_RATELIMIT_IPA(1);
			goto alloc_rule_id_fail;
		}
	}
	(*(entry))->rule_id = id;
	(*(entry))->ipacm_installed = user;

	return 0;

alloc_rule_id_fail:
	kmem_cache_free(ipa3_ctx->rt_rule_cache, *entry);
error:
	return -EPERM;
}

static int __ipa_finish_rt_rule_add(struct ipa3_rt_entry *entry, u32 *rule_hdl,
		struct ipa3_rt_tbl *tbl)
{
	int id;

	tbl->rule_cnt++;
	if (entry->hdr)
		entry->hdr->ref_cnt++;
	else if (entry->proc_ctx)
		entry->proc_ctx->ref_cnt++;
	id = ipa3_id_alloc(entry);
	if (id < 0) {
		IPAERR_RL("failed to add to tree\n");
		WARN_ON_RATELIMIT_IPA(1);
		goto ipa_insert_failed;
	}
	IPADBG("add rt rule tbl_idx=%d rule_cnt=%d rule_id=%d\n",
		tbl->idx, tbl->rule_cnt, entry->rule_id);
	*rule_hdl = id;
	entry->id = id;

	return 0;

ipa_insert_failed:
	if (entry->hdr)
		entry->hdr->ref_cnt--;
	else if (entry->proc_ctx)
		entry->proc_ctx->ref_cnt--;
	idr_remove(&tbl->rule_ids, entry->rule_id);
	list_del(&entry->link);
	kmem_cache_free(ipa3_ctx->rt_rule_cache, entry);
	return -EPERM;
}

static int __ipa_add_rt_rule(enum ipa_ip_type ip, const char *name,
		const struct ipa_rt_rule *rule, u8 at_rear, u32 *rule_hdl,
		u16 rule_id, bool user)
{
	struct ipa3_rt_tbl *tbl;
	struct ipa3_rt_entry *entry;
	struct ipa3_hdr_entry *hdr = NULL;
	struct ipa3_hdr_proc_ctx_entry *proc_ctx = NULL;

	if (__ipa_rt_validate_hndls(rule, &hdr, &proc_ctx))
		goto error;

	if (__ipa_rt_validate_rule_id(rule_id))
		goto error;

	tbl = __ipa_add_rt_tbl(ip, name);
	if (tbl == NULL || (tbl->cookie != IPA_RT_TBL_COOKIE)) {
		IPAERR_RL("failed adding rt tbl name = %s\n",
			name ? name : "");
		goto error;
	}
	/*
	 * do not allow any rules to be added at end of the "default" routing
	 * tables
	 */
	if (!strcmp(tbl->name, IPA_DFLT_RT_TBL_NAME) &&
	    (tbl->rule_cnt > 0) && (at_rear != 0)) {
		IPAERR_RL("cannot add rule at end of tbl rule_cnt=%d at_rear=%d"
				, tbl->rule_cnt, at_rear);
		goto error;
	}

	if (__ipa_create_rt_entry(&entry, rule, tbl, hdr, proc_ctx,
		rule_id, user))
		goto error;

	if (at_rear)
		list_add_tail(&entry->link, &tbl->head_rt_rule_list);
	else
		list_add(&entry->link, &tbl->head_rt_rule_list);

	if (__ipa_finish_rt_rule_add(entry, rule_hdl, tbl))
		goto error;

	return 0;

error:
	return -EPERM;
}

static int __ipa_add_rt_rule_after(struct ipa3_rt_tbl *tbl,
		const struct ipa_rt_rule *rule, u32 *rule_hdl,
		struct ipa3_rt_entry **add_after_entry)
{
	struct ipa3_rt_entry *entry;
	struct ipa3_hdr_entry *hdr = NULL;
	struct ipa3_hdr_proc_ctx_entry *proc_ctx = NULL;

	if (!*add_after_entry)
		goto error;

	if (__ipa_rt_validate_hndls(rule, &hdr, &proc_ctx))
		goto error;

	if (__ipa_create_rt_entry(&entry, rule, tbl, hdr, proc_ctx, 0, true))
		goto error;

	list_add(&entry->link, &((*add_after_entry)->link));

	if (__ipa_finish_rt_rule_add(entry, rule_hdl, tbl))
		goto error;

	/*
	 * prepare for next insertion
	 */
	*add_after_entry = entry;

	return 0;

error:
	*add_after_entry = NULL;
	return -EPERM;
}

/**
 * ipa3_add_rt_rule() - Add the specified routing rules to SW and optionally
 * commit to IPA HW
 * @rules:	[inout] set of routing rules to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */

int ipa3_add_rt_rule(struct ipa_ioc_add_rt_rule *rules)
{
	return ipa3_add_rt_rule_usr(rules, false);
}
/**
 * ipa3_add_rt_rule_usr() - Add the specified routing rules to SW and optionally
 * commit to IPA HW
 * @rules:		[inout] set of routing rules to add
 * @user_only:	[in] indicate installed by userspace module
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */

int ipa3_add_rt_rule_usr(struct ipa_ioc_add_rt_rule *rules, bool user_only)
{
	int i;
	int ret;

	if (rules == NULL || rules->num_rules == 0 || rules->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < rules->num_rules; i++) {
		if (__ipa_add_rt_rule(rules->ip, rules->rt_tbl_name,
					&rules->rules[i].rule,
					rules->rules[i].at_rear,
					&rules->rules[i].rt_rule_hdl,
					0,
					user_only)) {
			IPAERR_RL("failed to add rt rule %d\n", i);
			rules->rules[i].status = IPA_RT_STATUS_OF_ADD_FAILED;
		} else {
			rules->rules[i].status = 0;
		}
	}

	if (rules->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_rt(rules->ip)) {
			ret = -EPERM;
			goto bail;
		}

	ret = 0;
bail:
	mutex_unlock(&ipa3_ctx->lock);
	return ret;
}

/**
 * ipa3_add_rt_rule_ext() - Add the specified routing rules to SW with rule id
 * and optionally commit to IPA HW
 * @rules:	[inout] set of routing rules to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_add_rt_rule_ext(struct ipa_ioc_add_rt_rule_ext *rules)
{
	int i;
	int ret;

	if (rules == NULL || rules->num_rules == 0 || rules->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < rules->num_rules; i++) {
		if (__ipa_add_rt_rule(rules->ip, rules->rt_tbl_name,
					&rules->rules[i].rule,
					rules->rules[i].at_rear,
					&rules->rules[i].rt_rule_hdl,
					rules->rules[i].rule_id, true)) {
			IPAERR_RL("failed to add rt rule %d\n", i);

			rules->rules[i].status = IPA_RT_STATUS_OF_ADD_FAILED;
		} else {
			rules->rules[i].status = 0;
		}
	}

	if (rules->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_rt(rules->ip)) {
			ret = -EPERM;
			goto bail;
		}

	ret = 0;
bail:
	mutex_unlock(&ipa3_ctx->lock);
	return ret;
}

/**
 * ipa3_add_rt_rule_after() - Add the given routing rules after the
 * specified rule to SW and optionally commit to IPA HW
 * @rules:	[inout] set of routing rules to add + handle where to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_add_rt_rule_after(struct ipa_ioc_add_rt_rule_after *rules)
{
	int i;
	int ret = 0;
	struct ipa3_rt_tbl *tbl = NULL;
	struct ipa3_rt_entry *entry = NULL;

	if (rules == NULL || rules->num_rules == 0 || rules->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);

	tbl = __ipa3_find_rt_tbl(rules->ip, rules->rt_tbl_name);
	if (tbl == NULL || (tbl->cookie != IPA_RT_TBL_COOKIE)) {
		IPAERR_RL("failed finding rt tbl name = %s\n",
			rules->rt_tbl_name ? rules->rt_tbl_name : "");
		ret = -EINVAL;
		goto bail;
	}

	if (tbl->rule_cnt <= 0) {
		IPAERR_RL("tbl->rule_cnt <= 0");
		ret = -EINVAL;
		goto bail;
	}

	entry = ipa3_id_find(rules->add_after_hdl);
	if (!entry) {
		IPAERR_RL("failed finding rule %d in rt tbls\n",
			rules->add_after_hdl);
		ret = -EINVAL;
		goto bail;
	}

	if (entry->cookie != IPA_RT_RULE_COOKIE) {
		IPAERR_RL("Invalid cookie value =  %u rule %d in rt tbls\n",
			entry->cookie, rules->add_after_hdl);
		ret = -EINVAL;
		goto bail;
	}

	if (entry->tbl != tbl) {
		IPAERR_RL("given rt rule does not match the table\n");
		ret = -EINVAL;
		goto bail;
	}

	/*
	 * do not allow any rules to be added at end of the "default" routing
	 * tables
	 */
	if (!strcmp(tbl->name, IPA_DFLT_RT_TBL_NAME) &&
			(&entry->link == tbl->head_rt_rule_list.prev)) {
		IPAERR_RL("cannot add rule at end of tbl rule_cnt=%d\n",
			tbl->rule_cnt);
		ret = -EINVAL;
		goto bail;
	}

	/*
	 * we add all rules one after the other, if one insertion fails, it cuts
	 * the chain (all following will receive fail status) following calls to
	 * __ipa_add_rt_rule_after will fail (entry == NULL)
	 */

	for (i = 0; i < rules->num_rules; i++) {
		if (__ipa_add_rt_rule_after(tbl,
					&rules->rules[i].rule,
					&rules->rules[i].rt_rule_hdl,
					&entry)) {
			IPAERR_RL("failed to add rt rule %d\n", i);
			rules->rules[i].status = IPA_RT_STATUS_OF_ADD_FAILED;
		} else {
			rules->rules[i].status = 0;
		}
	}

	if (rules->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_rt(rules->ip)) {
			IPAERR_RL("failed to commit\n");
			ret = -EPERM;
			goto bail;
		}

	ret = 0;
	goto bail;

bail:
	mutex_unlock(&ipa3_ctx->lock);
	return ret;
}

int __ipa3_del_rt_rule(u32 rule_hdl)
{
	struct ipa3_rt_entry *entry;
	int id;
	struct ipa3_hdr_entry *hdr_entry;
	struct ipa3_hdr_proc_ctx_entry *hdr_proc_entry;

	entry = ipa3_id_find(rule_hdl);

	if (entry == NULL) {
		IPAERR_RL("lookup failed\n");
		return -EINVAL;
	}

	if (entry->cookie != IPA_RT_RULE_COOKIE) {
		IPAERR_RL("bad params\n");
		return -EINVAL;
	}

	if (!strcmp(entry->tbl->name, IPA_DFLT_RT_TBL_NAME)) {
		IPADBG("Deleting rule from default rt table idx=%u\n",
			entry->tbl->idx);
		if (entry->tbl->rule_cnt == 1) {
			IPAERR_RL("Default tbl last rule cannot be deleted\n");
			return -EINVAL;
		}
	}

	/* Adding check to confirm still
	 * header entry present in header table or not
	 */

	if (entry->hdr) {
		hdr_entry = ipa3_id_find(entry->rule.hdr_hdl);
		if (!hdr_entry || hdr_entry->cookie != IPA_HDR_COOKIE) {
			IPAERR_RL("Header entry already deleted\n");
			return -EINVAL;
		}
	} else if (entry->proc_ctx) {
		hdr_proc_entry = ipa3_id_find(entry->rule.hdr_proc_ctx_hdl);
		if (!hdr_proc_entry ||
			hdr_proc_entry->cookie != IPA_PROC_HDR_COOKIE) {
			IPAERR_RL("Proc header entry already deleted\n");
			return -EINVAL;
		}
	}

	if (entry->hdr)
		__ipa3_release_hdr(entry->hdr->id);
	else if (entry->proc_ctx)
		__ipa3_release_hdr_proc_ctx(entry->proc_ctx->id);
	list_del(&entry->link);
	entry->tbl->rule_cnt--;
	IPADBG("del rt rule tbl_idx=%d rule_cnt=%d rule_id=%d\n ref_cnt=%u",
		entry->tbl->idx, entry->tbl->rule_cnt,
		entry->rule_id, entry->tbl->ref_cnt);
		/* if rule id was allocated from idr, remove it */
	if (!entry->rule_id_valid)
		idr_remove(&entry->tbl->rule_ids, entry->rule_id);
	if (entry->tbl->rule_cnt == 0 && entry->tbl->ref_cnt == 0) {
		if (__ipa_del_rt_tbl(entry->tbl))
			IPAERR_RL("fail to del RT tbl\n");
	}
	entry->cookie = 0;
	id = entry->id;
	kmem_cache_free(ipa3_ctx->rt_rule_cache, entry);

	/* remove the handle from the database */
	ipa3_id_remove(id);

	return 0;
}

/**
 * ipa3_del_rt_rule() - Remove the specified routing rules to SW and optionally
 * commit to IPA HW
 * @hdls:	[inout] set of routing rules to delete
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls)
{
	int i;
	int ret;

	if (hdls == NULL || hdls->num_hdls == 0 || hdls->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa3_del_rt_rule(hdls->hdl[i].hdl)) {
			IPAERR_RL("failed to del rt rule %i\n", i);
			hdls->hdl[i].status = IPA_RT_STATUS_OF_DEL_FAILED;
		} else {
			hdls->hdl[i].status = 0;
		}
	}

	if (hdls->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_rt(hdls->ip)) {
			ret = -EPERM;
			goto bail;
		}

	ret = 0;
bail:
	mutex_unlock(&ipa3_ctx->lock);
	return ret;
}

/**
 * ipa_commit_rt_rule() - Commit the current SW routing table of specified type
 * to IPA HW
 * @ip:	The family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_commit_rt(enum ipa_ip_type ip)
{
	int ret;

	if (ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	/*
	 * issue a commit on the filtering module of same IP type since
	 * filtering rules point to routing tables
	 */
	if (ipa3_commit_flt(ip))
		return -EPERM;

	mutex_lock(&ipa3_ctx->lock);
	if (ipa3_ctx->ctrl->ipa3_commit_rt(ip)) {
		ret = -EPERM;
		goto bail;
	}

	ret = 0;
bail:
	mutex_unlock(&ipa3_ctx->lock);
	return ret;
}

/**
 * ipa3_reset_rt() - reset the current SW routing table of specified type
 * (does not commit to HW)
 * @ip:			[in] The family of routing tables
 * @user_only:	[in] indicate delete rules installed by userspace
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_reset_rt(enum ipa_ip_type ip, bool user_only)
{
	struct ipa3_rt_tbl *tbl;
	struct ipa3_rt_tbl *tbl_next;
	struct ipa3_rt_tbl_set *set;
	struct ipa3_rt_entry *rule;
	struct ipa3_rt_entry *rule_next;
	struct ipa3_rt_tbl_set *rset;
	u32 apps_start_idx;
	int id;
	bool tbl_user = false;

	if (ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	if (ip == IPA_IP_v4)
		apps_start_idx =
			IPA_MEM_PART(v4_apps_rt_index_lo);
	else
		apps_start_idx =
			IPA_MEM_PART(v6_apps_rt_index_lo);

	/*
	 * issue a reset on the filtering module of same IP type since
	 * filtering rules point to routing tables
	 */
	if (ipa3_reset_flt(ip, user_only))
		IPAERR_RL("fail to reset flt ip=%d\n", ip);

	set = &ipa3_ctx->rt_tbl_set[ip];
	rset = &ipa3_ctx->reap_rt_tbl_set[ip];
	mutex_lock(&ipa3_ctx->lock);
	IPADBG("reset rt ip=%d\n", ip);
	list_for_each_entry_safe(tbl, tbl_next, &set->head_rt_tbl_list, link) {
		tbl_user = false;
		list_for_each_entry_safe(rule, rule_next,
					 &tbl->head_rt_rule_list, link) {
			if (ipa3_id_find(rule->id) == NULL) {
				WARN_ON_RATELIMIT_IPA(1);
				mutex_unlock(&ipa3_ctx->lock);
				return -EFAULT;
			}

			/* indicate if tbl used for user-specified rules*/
			if (rule->ipacm_installed) {
				IPADBG("tbl_user %d, tbl-index %d\n",
				tbl_user, tbl->id);
				tbl_user = true;
			}
			/*
			 * for the "default" routing tbl, remove all but the
			 *  last rule
			 */
			if (tbl->idx == apps_start_idx && tbl->rule_cnt == 1)
				continue;

			if (!user_only ||
				rule->ipacm_installed) {
				list_del(&rule->link);
				tbl->rule_cnt--;
				if (rule->hdr)
					__ipa3_release_hdr(rule->hdr->id);
				else if (rule->proc_ctx)
					__ipa3_release_hdr_proc_ctx(
						rule->proc_ctx->id);
				rule->cookie = 0;
				idr_remove(&tbl->rule_ids, rule->rule_id);
				id = rule->id;
				kmem_cache_free(ipa3_ctx->rt_rule_cache, rule);

				/* remove the handle from the database */
				ipa3_id_remove(id);
			}
		}

		if (ipa3_id_find(tbl->id) == NULL) {
			WARN_ON_RATELIMIT_IPA(1);
			mutex_unlock(&ipa3_ctx->lock);
			return -EFAULT;
		}
		id = tbl->id;

		/* do not remove the "default" routing tbl which has index 0 */
		if (tbl->idx != apps_start_idx) {
			if (!user_only || tbl_user) {
				idr_destroy(&tbl->rule_ids);
				if (tbl->in_sys[IPA_RULE_HASHABLE] ||
					tbl->in_sys[IPA_RULE_NON_HASHABLE]) {
					list_move(&tbl->link,
						&rset->head_rt_tbl_list);
					clear_bit(tbl->idx,
					  &ipa3_ctx->rt_idx_bitmap[ip]);
					set->tbl_cnt--;
					IPADBG("rst tbl_idx=%d cnt=%d\n",
						tbl->idx, set->tbl_cnt);
				} else {
					list_del(&tbl->link);
					set->tbl_cnt--;
					clear_bit(tbl->idx,
					  &ipa3_ctx->rt_idx_bitmap[ip]);
					IPADBG("rst rt tbl_idx=%d tbl_cnt=%d\n",
						tbl->idx, set->tbl_cnt);
					kmem_cache_free(ipa3_ctx->rt_tbl_cache,
						tbl);
				}
				/* remove the handle from the database */
				ipa3_id_remove(id);
			}
		}
	}

	/* commit the change to IPA-HW */
	if (ipa3_ctx->ctrl->ipa3_commit_rt(IPA_IP_v4) ||
		ipa3_ctx->ctrl->ipa3_commit_rt(IPA_IP_v6)) {
		IPAERR("fail to commit rt-rule\n");
		WARN_ON_RATELIMIT_IPA(1);
		mutex_unlock(&ipa3_ctx->lock);
		return -EPERM;
	}
	mutex_unlock(&ipa3_ctx->lock);

	return 0;
}

/**
 * ipa3_get_rt_tbl() - lookup the specified routing table and return handle if it
 * exists, if lookup succeeds the routing table ref cnt is increased
 * @lookup:	[inout] routing table to lookup and its handle
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 *	Caller should call ipa3_put_rt_tbl later if this function succeeds
 */
int ipa3_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup)
{
	struct ipa3_rt_tbl *entry;
	int result = -EFAULT;

	if (lookup == NULL || lookup->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}
	mutex_lock(&ipa3_ctx->lock);
	entry = __ipa3_find_rt_tbl(lookup->ip, lookup->name);
	if (entry && entry->cookie == IPA_RT_TBL_COOKIE) {
		if (entry->ref_cnt == U32_MAX) {
			IPAERR_RL("fail: ref count crossed limit\n");
			goto ret;
		}
		entry->ref_cnt++;
		lookup->hdl = entry->id;

		/* commit for get */
		if (ipa3_ctx->ctrl->ipa3_commit_rt(lookup->ip))
			IPAERR_RL("fail to commit RT tbl\n");

		result = 0;
	}

ret:
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_put_rt_tbl() - Release the specified routing table handle
 * @rt_tbl_hdl:	[in] the routing table handle to release
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_put_rt_tbl(u32 rt_tbl_hdl)
{
	struct ipa3_rt_tbl *entry;
	enum ipa_ip_type ip = IPA_IP_MAX;
	int result = 0;

	mutex_lock(&ipa3_ctx->lock);
	entry = ipa3_id_find(rt_tbl_hdl);
	if (entry == NULL) {
		IPAERR_RL("lookup failed\n");
		result = -EINVAL;
		goto ret;
	}

	if ((entry->cookie != IPA_RT_TBL_COOKIE) || entry->ref_cnt == 0) {
		IPAERR_RL("bad parms\n");
		result = -EINVAL;
		goto ret;
	}

	if (entry->set == &ipa3_ctx->rt_tbl_set[IPA_IP_v4])
		ip = IPA_IP_v4;
	else if (entry->set == &ipa3_ctx->rt_tbl_set[IPA_IP_v6])
		ip = IPA_IP_v6;
	else {
		WARN_ON_RATELIMIT_IPA(1);
		result = -EINVAL;
		goto ret;
	}

	entry->ref_cnt--;
	if (entry->ref_cnt == 0 && entry->rule_cnt == 0) {
		IPADBG("zero ref_cnt, delete rt tbl (idx=%u)\n",
			entry->idx);
		if (__ipa_del_rt_tbl(entry))
			IPAERR_RL("fail to del RT tbl\n");
		/* commit for put */
		if (ipa3_ctx->ctrl->ipa3_commit_rt(ip))
			IPAERR_RL("fail to commit RT tbl\n");
	}

	result = 0;

ret:
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}


static int __ipa_mdfy_rt_rule(struct ipa_rt_rule_mdfy *rtrule)
{
	struct ipa3_rt_entry *entry;
	struct ipa3_hdr_entry *hdr = NULL;
	struct ipa3_hdr_proc_ctx_entry *proc_ctx = NULL;
	struct ipa3_hdr_entry *hdr_entry;
	struct ipa3_hdr_proc_ctx_entry *hdr_proc_entry;
	if (rtrule->rule.hdr_hdl) {
		hdr = ipa3_id_find(rtrule->rule.hdr_hdl);
		if ((hdr == NULL) || (hdr->cookie != IPA_HDR_COOKIE)) {
			IPAERR_RL("rt rule does not point to valid hdr\n");
			goto error;
		}
	} else if (rtrule->rule.hdr_proc_ctx_hdl) {
		proc_ctx = ipa3_id_find(rtrule->rule.hdr_proc_ctx_hdl);
		if ((proc_ctx == NULL) ||
			(proc_ctx->cookie != IPA_PROC_HDR_COOKIE)) {
			IPAERR_RL("rt rule does not point to valid proc ctx\n");
			goto error;
		}
	}

	entry = ipa3_id_find(rtrule->rt_rule_hdl);
	if (entry == NULL) {
		IPAERR_RL("lookup failed\n");
		goto error;
	}

	if (entry->cookie != IPA_RT_RULE_COOKIE) {
		IPAERR_RL("bad params\n");
		goto error;
	}

	/* Adding check to confirm still
	 * header entry present in header table or not
	 */

	if (entry->hdr) {
		hdr_entry = ipa3_id_find(entry->rule.hdr_hdl);
		if (!hdr_entry || hdr_entry->cookie != IPA_HDR_COOKIE) {
			IPAERR_RL("Header entry already deleted\n");
			return -EPERM;
		}
	} else if (entry->proc_ctx) {
		hdr_proc_entry = ipa3_id_find(entry->rule.hdr_proc_ctx_hdl);
		if (!hdr_proc_entry ||
			hdr_proc_entry->cookie != IPA_PROC_HDR_COOKIE) {
			IPAERR_RL("Proc header entry already deleted\n");
			return -EPERM;
		}
	}

	if (entry->hdr)
		entry->hdr->ref_cnt--;
	if (entry->proc_ctx)
		entry->proc_ctx->ref_cnt--;

	entry->rule = rtrule->rule;
	entry->hdr = hdr;
	entry->proc_ctx = proc_ctx;

	if (entry->hdr)
		entry->hdr->ref_cnt++;
	if (entry->proc_ctx)
		entry->proc_ctx->ref_cnt++;

	entry->hw_len = 0;
	entry->prio = 0;

	return 0;

error:
	return -EPERM;
}

/**
 * ipa3_mdfy_rt_rule() - Modify the specified routing rules in SW and optionally
 * commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_mdfy_rt_rule(struct ipa_ioc_mdfy_rt_rule *hdls)
{
	int i;
	int result;

	if (hdls == NULL || hdls->num_rules == 0 || hdls->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < hdls->num_rules; i++) {
		if (__ipa_mdfy_rt_rule(&hdls->rules[i])) {
			IPAERR_RL("failed to mdfy rt rule %i\n", i);
			hdls->rules[i].status = IPA_RT_STATUS_OF_MDFY_FAILED;
		} else {
			hdls->rules[i].status = 0;
		}
	}

	if (hdls->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_rt(hdls->ip)) {
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_set_rt_tuple_mask() - Sets the rt tuple masking for the given tbl
 *  table index must be for AP EP (not modem)
 *  updates the the routing masking values without changing the flt ones.
 *
 * @tbl_idx: routing table index to configure the tuple masking
 * @tuple: the tuple members masking
 * Returns:	0 on success, negative on failure
 *
 */
int ipa3_set_rt_tuple_mask(int tbl_idx, struct ipahal_reg_hash_tuple *tuple)
{
	struct ipahal_reg_fltrt_hash_tuple fltrt_tuple;

	if (!tuple) {
		IPAERR("bad tuple\n");
		return -EINVAL;
	}

	if (tbl_idx >=
		max(IPA_MEM_PART(v6_rt_num_index),
		IPA_MEM_PART(v4_rt_num_index)) ||
		tbl_idx < 0) {
		IPAERR("bad table index\n");
		return -EINVAL;
	}

	if (tbl_idx >= IPA_MEM_PART(v4_modem_rt_index_lo) &&
		tbl_idx <= IPA_MEM_PART(v4_modem_rt_index_hi)) {
		IPAERR("cannot configure modem v4 rt tuple by AP\n");
		return -EINVAL;
	}

	if (tbl_idx >= IPA_MEM_PART(v6_modem_rt_index_lo) &&
		tbl_idx <= IPA_MEM_PART(v6_modem_rt_index_hi)) {
		IPAERR("cannot configure modem v6 rt tuple by AP\n");
		return -EINVAL;
	}

	ipahal_read_reg_n_fields(IPA_ENDP_FILTER_ROUTER_HSH_CFG_n,
		tbl_idx, &fltrt_tuple);
	fltrt_tuple.rt = *tuple;
	ipahal_write_reg_n_fields(IPA_ENDP_FILTER_ROUTER_HSH_CFG_n,
		tbl_idx, &fltrt_tuple);

	return 0;
}

/**
 * ipa3_rt_read_tbl_from_hw() -Read routing table from IPA HW
 * @tbl_idx: routing table index
 * @ip_type: IPv4 or IPv6 table
 * @hashable: hashable or non-hashable table
 * @entry: array to fill the table entries
 * @num_entry: number of entries in entry array. set by the caller to indicate
 *  entry array size. Then set by this function as an output parameter to
 *  indicate the number of entries in the array
 *
 * This function reads the filtering table from IPA SRAM and prepares an array
 * of entries. This function is mainly used for debugging purposes.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_rt_read_tbl_from_hw(u32 tbl_idx,
	enum ipa_ip_type ip_type,
	bool hashable,
	struct ipa3_debugfs_rt_entry entry[],
	int *num_entry)
{
	u64 tbl_entry_in_hdr_ofst;
	u64 *tbl_entry_in_hdr;
	struct ipa3_rt_rule_hw_hdr *hdr;
	u8 *buf;
	int rule_idx;
	u8 rule_size;
	void *ipa_sram_mmio;

	IPADBG_LOW("tbl_idx=%d ip_type=%d hashable=%d\n",
		tbl_idx, ip_type, hashable);

	if (ip_type == IPA_IP_v4 && tbl_idx >= IPA_MEM_PART(v4_rt_num_index)) {
		IPAERR("Invalid params\n");
		return -EFAULT;
	}

	if (ip_type == IPA_IP_v6 && tbl_idx >= IPA_MEM_PART(v6_rt_num_index)) {
		IPAERR("Invalid params\n");
		return -EFAULT;
	}

	/* map IPA SRAM */
	ipa_sram_mmio = ioremap(ipa3_ctx->ipa_wrapper_base +
				ipa3_ctx->ctrl->ipa_reg_base_ofst +
				ipahal_get_reg_n_ofst(
					IPA_SRAM_DIRECT_ACCESS_n,
					0),
				ipa3_ctx->smem_sz);
	if (!ipa_sram_mmio) {
		IPAERR("fail to ioremap IPA SRAM\n");
		return -ENOMEM;
	}

	memset(entry, 0, sizeof(*entry) * (*num_entry));
	if (hashable) {
		if (ip_type == IPA_IP_v4)
			tbl_entry_in_hdr_ofst =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v4_rt_hash_ofst) +
				tbl_idx * IPA_HW_TBL_HDR_WIDTH;
		else
			tbl_entry_in_hdr_ofst =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v6_rt_hash_ofst) +
				tbl_idx * IPA_HW_TBL_HDR_WIDTH;
	} else {
		if (ip_type == IPA_IP_v4)
			tbl_entry_in_hdr_ofst =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v4_rt_nhash_ofst) +
				tbl_idx * IPA_HW_TBL_HDR_WIDTH;
		else
			tbl_entry_in_hdr_ofst =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v6_rt_nhash_ofst) +
				tbl_idx * IPA_HW_TBL_HDR_WIDTH;
	}

	IPADBG_LOW("tbl_entry_in_hdr_ofst=0x%llx\n", tbl_entry_in_hdr_ofst);

	tbl_entry_in_hdr = ipa_sram_mmio + tbl_entry_in_hdr_ofst;

	/* for tables which reside in DDR access it from the virtual memory */
	if (!(*tbl_entry_in_hdr & 0x1)) {
		/* system */
		struct ipa3_rt_tbl_set *set;
		struct ipa3_rt_tbl *tbl;

		set = &ipa3_ctx->rt_tbl_set[ip_type];
		hdr = NULL;
		list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
			if (tbl->idx == tbl_idx)
				hdr = tbl->curr_mem[hashable ?
					IPA_RULE_HASHABLE :
					IPA_RULE_NON_HASHABLE].base;
		}

		if (!hdr)
			hdr = ipa3_ctx->empty_rt_tbl_mem.base;
	} else {
		/* local */
		hdr = (void *)((u8 *)tbl_entry_in_hdr -
			tbl_idx * IPA_HW_TBL_HDR_WIDTH +
			(*tbl_entry_in_hdr - 1) / 16);
	}
	IPADBG("*tbl_entry_in_hdr=0x%llx\n", *tbl_entry_in_hdr);
	IPADBG("hdr=0x%p\n", hdr);


	rule_idx = 0;
	while (rule_idx < *num_entry) {
		IPADBG_LOW("*((u64 *)hdr)=0x%llx\n", *((u64 *)hdr));
		if (*((u64 *)hdr) == 0)
			break;

		entry[rule_idx].eq_attrib.rule_eq_bitmap = hdr->u.hdr.en_rule;
		entry[rule_idx].retain_hdr = hdr->u.hdr.retain_hdr;
		entry[rule_idx].prio = hdr->u.hdr.priority;
		entry[rule_idx].rule_id = hdr->u.hdr.rule_id;
		entry[rule_idx].dst = hdr->u.hdr.pipe_dest_idx;
		entry[rule_idx].hdr_ofset = hdr->u.hdr.hdr_offset;
		entry[rule_idx].is_proc_ctx = hdr->u.hdr.proc_ctx;
		entry[rule_idx].system = hdr->u.hdr.system;
		buf = (u8 *)(hdr + 1);
		IPADBG("buf=0x%p\n", buf);

		ipa3_generate_eq_from_hw_rule(&entry[rule_idx].eq_attrib, buf,
			&rule_size);
		IPADBG_LOW("rule_size=%d\n", rule_size);
		hdr = (void *)(buf + rule_size);
		IPADBG_LOW("hdr=0x%p\n", hdr);
		rule_idx++;
	}

	*num_entry = rule_idx;
	iounmap(ipa_sram_mmio);

	return 0;
}

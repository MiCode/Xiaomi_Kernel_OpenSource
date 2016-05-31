/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#include "ipa_i.h"
#include "ipahal/ipahal.h"

#define IPA_FLT_TABLE_INDEX_NOT_FOUND		(-1)
#define IPA_FLT_STATUS_OF_ADD_FAILED		(-1)
#define IPA_FLT_STATUS_OF_DEL_FAILED		(-1)
#define IPA_FLT_STATUS_OF_MDFY_FAILED		(-1)

#define IPA_FLT_GET_RULE_TYPE(__entry) \
	( \
	((__entry)->rule.hashable) ? \
	(IPA_RULE_HASHABLE):(IPA_RULE_NON_HASHABLE) \
	)

static int ipa3_generate_hw_rule_from_eq(
		const struct ipa_ipfltri_rule_eq *attrib, u8 **buf)
{
	int num_offset_meq_32 = attrib->num_offset_meq_32;
	int num_ihl_offset_range_16 = attrib->num_ihl_offset_range_16;
	int num_ihl_offset_meq_32 = attrib->num_ihl_offset_meq_32;
	int num_offset_meq_128 = attrib->num_offset_meq_128;
	int i;
	int extra_bytes;
	u8 *extra;
	u8 *rest;

	extra_bytes = ipa3_calc_extra_wrd_bytes(attrib);
	/* only 3 eq does not have extra word param, 13 out of 16 is the number
	 * of equations that needs extra word param*/
	if (extra_bytes > 13) {
		IPAERR("too much extra bytes\n");
		return -EPERM;
	} else if (extra_bytes > IPA_HW_TBL_HDR_WIDTH) {
		/* two extra words */
		extra = *buf;
		rest = *buf + IPA_HW_TBL_HDR_WIDTH * 2;
	} else if (extra_bytes > 0) {
		/* single exra word */
		extra = *buf;
		rest = *buf + IPA_HW_TBL_HDR_WIDTH;
	} else {
		/* no extra words */
		extra = NULL;
		rest = *buf;
	}

	if (attrib->tos_eq_present)
		extra = ipa3_write_8(attrib->tos_eq, extra);

	if (attrib->protocol_eq_present)
		extra = ipa3_write_8(attrib->protocol_eq, extra);

	if (attrib->tc_eq_present)
		extra = ipa3_write_8(attrib->tc_eq, extra);

	if (num_offset_meq_128) {
		extra = ipa3_write_8(attrib->offset_meq_128[0].offset, extra);
		for (i = 0; i < 8; i++)
			rest = ipa3_write_8(attrib->offset_meq_128[0].mask[i],
				rest);
		for (i = 0; i < 8; i++)
			rest = ipa3_write_8(attrib->offset_meq_128[0].value[i],
				rest);
		for (i = 8; i < 16; i++)
			rest = ipa3_write_8(attrib->offset_meq_128[0].mask[i],
				rest);
		for (i = 8; i < 16; i++)
			rest = ipa3_write_8(attrib->offset_meq_128[0].value[i],
				rest);
		num_offset_meq_128--;
	}

	if (num_offset_meq_128) {
		extra = ipa3_write_8(attrib->offset_meq_128[1].offset, extra);
		for (i = 0; i < 8; i++)
			rest = ipa3_write_8(attrib->offset_meq_128[1].mask[i],
				rest);
		for (i = 0; i < 8; i++)
			rest = ipa3_write_8(attrib->offset_meq_128[1].value[i],
				rest);
		for (i = 8; i < 16; i++)
			rest = ipa3_write_8(attrib->offset_meq_128[1].mask[i],
				rest);
		for (i = 8; i < 16; i++)
			rest = ipa3_write_8(attrib->offset_meq_128[1].value[i],
				rest);
		num_offset_meq_128--;
	}

	if (num_offset_meq_32) {
		extra = ipa3_write_8(attrib->offset_meq_32[0].offset, extra);
		rest = ipa3_write_32(attrib->offset_meq_32[0].mask, rest);
		rest = ipa3_write_32(attrib->offset_meq_32[0].value, rest);
		num_offset_meq_32--;
	}

	if (num_offset_meq_32) {
		extra = ipa3_write_8(attrib->offset_meq_32[1].offset, extra);
		rest = ipa3_write_32(attrib->offset_meq_32[1].mask, rest);
		rest = ipa3_write_32(attrib->offset_meq_32[1].value, rest);
		num_offset_meq_32--;
	}

	if (num_ihl_offset_meq_32) {
		extra = ipa3_write_8(attrib->ihl_offset_meq_32[0].offset,
		extra);

		rest = ipa3_write_32(attrib->ihl_offset_meq_32[0].mask, rest);
		rest = ipa3_write_32(attrib->ihl_offset_meq_32[0].value, rest);
		num_ihl_offset_meq_32--;
	}

	if (num_ihl_offset_meq_32) {
		extra = ipa3_write_8(attrib->ihl_offset_meq_32[1].offset,
		extra);

		rest = ipa3_write_32(attrib->ihl_offset_meq_32[1].mask, rest);
		rest = ipa3_write_32(attrib->ihl_offset_meq_32[1].value, rest);
		num_ihl_offset_meq_32--;
	}

	if (attrib->metadata_meq32_present) {
		rest = ipa3_write_32(attrib->metadata_meq32.mask, rest);
		rest = ipa3_write_32(attrib->metadata_meq32.value, rest);
	}

	if (num_ihl_offset_range_16) {
		extra = ipa3_write_8(attrib->ihl_offset_range_16[0].offset,
		extra);

		rest = ipa3_write_16(attrib->ihl_offset_range_16[0].range_high,
				rest);
		rest = ipa3_write_16(attrib->ihl_offset_range_16[0].range_low,
				rest);
		num_ihl_offset_range_16--;
	}

	if (num_ihl_offset_range_16) {
		extra = ipa3_write_8(attrib->ihl_offset_range_16[1].offset,
		extra);

		rest = ipa3_write_16(attrib->ihl_offset_range_16[1].range_high,
				rest);
		rest = ipa3_write_16(attrib->ihl_offset_range_16[1].range_low,
				rest);
		num_ihl_offset_range_16--;
	}

	if (attrib->ihl_offset_eq_32_present) {
		extra = ipa3_write_8(attrib->ihl_offset_eq_32.offset, extra);
		rest = ipa3_write_32(attrib->ihl_offset_eq_32.value, rest);
	}

	if (attrib->ihl_offset_eq_16_present) {
		extra = ipa3_write_8(attrib->ihl_offset_eq_16.offset, extra);
		rest = ipa3_write_16(attrib->ihl_offset_eq_16.value, rest);
		rest = ipa3_write_16(0, rest);
	}

	if (attrib->fl_eq_present)
		rest = ipa3_write_32(attrib->fl_eq & 0xFFFFF, rest);

	extra = ipa3_pad_to_64(extra);
	rest = ipa3_pad_to_64(rest);
	*buf = rest;

	return 0;
}

/**
 * ipa3_generate_flt_hw_rule() - generates the filtering hardware rule
 * @ip: the ip address family type
 * @entry: filtering entry
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
static int ipa3_generate_flt_hw_rule(enum ipa_ip_type ip,
		struct ipa3_flt_entry *entry, u8 *buf)
{
	struct ipa3_flt_rule_hw_hdr *hdr;
	const struct ipa_flt_rule *rule =
		(const struct ipa_flt_rule *)&entry->rule;
	u16 en_rule = 0;
	u32 tmp[IPA_RT_FLT_HW_RULE_BUF_SIZE/4];
	u8 *start;

	if (buf == NULL) {
		memset(tmp, 0, IPA_RT_FLT_HW_RULE_BUF_SIZE);
		buf = (u8 *)tmp;
	} else {
		if ((long)buf & IPA_HW_RULE_START_ALIGNMENT) {
			IPAERR("buff is not rule start aligned\n");
			return -EPERM;
		}
	}

	start = buf;
	hdr = (struct ipa3_flt_rule_hw_hdr *)buf;
	hdr->u.hdr.action = entry->rule.action;
	hdr->u.hdr.retain_hdr =  entry->rule.retain_hdr;
	if (entry->rt_tbl)
		hdr->u.hdr.rt_tbl_idx = entry->rt_tbl->idx;
	else
		hdr->u.hdr.rt_tbl_idx = entry->rule.rt_tbl_idx;
	hdr->u.hdr.rsvd1 = 0;
	hdr->u.hdr.rsvd2 = 0;
	hdr->u.hdr.rsvd3 = 0;
	BUG_ON(entry->prio & ~0x3FF);
	hdr->u.hdr.priority = entry->prio;
	BUG_ON(entry->rule_id & ~0x3FF);
	hdr->u.hdr.rule_id = entry->rule_id;
	buf += sizeof(struct ipa3_flt_rule_hw_hdr);

	if (rule->eq_attrib_type) {
		if (ipa3_generate_hw_rule_from_eq(&rule->eq_attrib, &buf)) {
			IPAERR("fail to generate hw rule from eq\n");
			return -EPERM;
		}
		en_rule = rule->eq_attrib.rule_eq_bitmap;
	} else {
		if (ipa3_generate_hw_rule(ip, &rule->attrib, &buf, &en_rule)) {
			IPAERR("fail to generate hw rule\n");
			return -EPERM;
		}
	}

	IPADBG_LOW("en_rule=0x%x, action=%d, rt_idx=%d, retain_hdr=%d\n",
		en_rule,
		hdr->u.hdr.action,
		hdr->u.hdr.rt_tbl_idx,
		hdr->u.hdr.retain_hdr);
	IPADBG_LOW("priority=%d, rule_id=%d\n",
		hdr->u.hdr.priority,
		hdr->u.hdr.rule_id);

	hdr->u.hdr.en_rule = en_rule;
	ipa3_write_64(hdr->u.word, (u8 *)hdr);

	if (entry->hw_len == 0) {
		entry->hw_len = buf - start;
	} else if (entry->hw_len != (buf - start)) {
		IPAERR("hw_len differs b/w passes passed=0x%x calc=0x%td\n",
			entry->hw_len, (buf - start));
		return -EPERM;
	}

	return 0;
}

static void __ipa_reap_sys_flt_tbls(enum ipa_ip_type ip, enum ipa_rule_type rlt)
{
	struct ipa3_flt_tbl *tbl;
	int i;

	IPADBG_LOW("reaping sys flt tbls ip=%d rlt=%d\n", ip, rlt);

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;

		tbl = &ipa3_ctx->flt_tbl[i][ip];
		if (tbl->prev_mem[rlt].phys_base) {
			IPADBG_LOW("reaping flt tbl (prev) pipe=%d\n", i);
			dma_free_coherent(ipa3_ctx->pdev,
				tbl->prev_mem[rlt].size,
				tbl->prev_mem[rlt].base,
				tbl->prev_mem[rlt].phys_base);
			memset(&tbl->prev_mem[rlt], 0,
				sizeof(tbl->prev_mem[rlt]));
		}

		if (list_empty(&tbl->head_flt_rule_list)) {
			if (tbl->curr_mem[rlt].phys_base) {
				IPADBG_LOW("reaping flt tbl (curr) pipe=%d\n",
					i);
				dma_free_coherent(ipa3_ctx->pdev,
					tbl->curr_mem[rlt].size,
					tbl->curr_mem[rlt].base,
					tbl->curr_mem[rlt].phys_base);
				memset(&tbl->curr_mem[rlt], 0,
					sizeof(tbl->curr_mem[rlt]));
			}
		}
	}
}

/**
 * ipa_alloc_init_flt_tbl_hdr() - allocate and initialize buffers for
 *  flt tables headers to be filled into sram
 * @ip: the ip address family type
 * @hash_hdr: address of the dma buffer for the hashable flt tbl header
 * @nhash_hdr: address of the dma buffer for the non-hashable flt tbl header
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_alloc_init_flt_tbl_hdr(enum ipa_ip_type ip,
	struct ipa_mem_buffer *hash_hdr, struct ipa_mem_buffer *nhash_hdr)
{
	int num_hdrs;
	u64 *hash_entr;
	u64 *nhash_entr;
	int i;

	num_hdrs = ipa3_ctx->ep_flt_num;

	hash_hdr->size = num_hdrs * IPA_HW_TBL_HDR_WIDTH;
	hash_hdr->base = dma_alloc_coherent(ipa3_ctx->pdev, hash_hdr->size,
		&hash_hdr->phys_base, GFP_KERNEL);
	if (!hash_hdr->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", hash_hdr->size);
		goto err;
	}

	nhash_hdr->size = num_hdrs * IPA_HW_TBL_HDR_WIDTH;
	nhash_hdr->base = dma_alloc_coherent(ipa3_ctx->pdev, nhash_hdr->size,
		&nhash_hdr->phys_base, GFP_KERNEL);
	if (!nhash_hdr->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", nhash_hdr->size);
		goto nhash_alloc_fail;
	}

	hash_entr = (u64 *)hash_hdr->base;
	nhash_entr = (u64 *)nhash_hdr->base;
	for (i = 0; i < num_hdrs; i++) {
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
 * ipa_prep_flt_tbl_for_cmt() - preparing the flt table for commit
 *  assign priorities to the rules, calculate their sizes and calculate
 *  the overall table size
 * @ip: the ip address family type
 * @tbl: the flt tbl to be prepared
 * @pipe_idx: the ep pipe appropriate for the given tbl
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_prep_flt_tbl_for_cmt(enum ipa_ip_type ip,
	struct ipa3_flt_tbl *tbl, int pipe_idx)
{
	struct ipa3_flt_entry *entry;
	u16 prio_i = 1;

	tbl->sz[IPA_RULE_HASHABLE] = 0;
	tbl->sz[IPA_RULE_NON_HASHABLE] = 0;

	list_for_each_entry(entry, &tbl->head_flt_rule_list, link) {

		entry->prio = entry->rule.max_prio ?
			IPA_RULE_MAX_PRIORITY : prio_i++;

		if (entry->prio > IPA_RULE_MIN_PRIORITY) {
			IPAERR("cannot allocate new priority for flt rule\n");
			return -EPERM;
		}

		if (ipa3_generate_flt_hw_rule(ip, entry, NULL)) {
			IPAERR("failed to calculate HW FLT rule size\n");
			return -EPERM;
		}
		IPADBG_LOW("pipe %d hw_len %d priority %u\n",
			pipe_idx, entry->hw_len, entry->prio);

		if (entry->rule.hashable)
			tbl->sz[IPA_RULE_HASHABLE] += entry->hw_len;
		else
			tbl->sz[IPA_RULE_NON_HASHABLE] += entry->hw_len;
	}

	if ((tbl->sz[IPA_RULE_HASHABLE] +
		tbl->sz[IPA_RULE_NON_HASHABLE]) == 0) {
		IPADBG_LOW("flt tbl pipe %d is with zero total size\n",
			pipe_idx);
		return 0;
	}

	/* for the header word */
	if (tbl->sz[IPA_RULE_HASHABLE])
		tbl->sz[IPA_RULE_HASHABLE] += IPA_HW_TBL_HDR_WIDTH;
	if (tbl->sz[IPA_RULE_NON_HASHABLE])
		tbl->sz[IPA_RULE_NON_HASHABLE] += IPA_HW_TBL_HDR_WIDTH;

	IPADBG_LOW("FLT tbl pipe idx %d hash sz %u non-hash sz %u\n", pipe_idx,
		tbl->sz[IPA_RULE_HASHABLE], tbl->sz[IPA_RULE_NON_HASHABLE]);

	return 0;
}

/**
 * ipa_get_flt_tbl_lcl_bdy_size() - calc the overall memory needed on sram
 *  to hold the hashable and non-hashable flt rules tables bodies
 * @ip: the ip address family type
 * @hash_bdy_sz[OUT]: size on local sram for all tbls hashable rules
 * @nhash_bdy_sz[OUT]: size on local sram for all tbls non-hashable rules
 *
 * Return: none
 */
static void ipa_get_flt_tbl_lcl_bdy_size(enum ipa_ip_type ip,
	u32 *hash_bdy_sz, u32 *nhash_bdy_sz)
{
	struct ipa3_flt_tbl *tbl;
	int i;

	*hash_bdy_sz = 0;
	*nhash_bdy_sz = 0;

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;
		tbl = &ipa3_ctx->flt_tbl[i][ip];
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
 * ipa_translate_flt_tbl_to_hw_fmt() - translate the flt driver structures
 *  (rules and tables) to HW format and fill it in the given buffers
 * @ip: the ip address family type
 * @rlt: the type of the rules to translate (hashable or non-hashable)
 * @base: the rules body buffer to be filled
 * @hdr: the rules header (addresses/offsets) buffer to be filled
 * @body_ofst: the offset of the rules body from the rules header at
 *  ipa sram
 *
 * Returns: 0 on success, negative on failure
 *
 * caller needs to hold any needed locks to ensure integrity
 *
 */
static int ipa_translate_flt_tbl_to_hw_fmt(enum ipa_ip_type ip,
	enum ipa_rule_type rlt, u8 *base, u8 *hdr, u32 body_ofst)
{
	u64 offset;
	u8 *body_i;
	int res;
	struct ipa3_flt_entry *entry;
	u8 *tbl_mem_buf;
	struct ipa_mem_buffer tbl_mem;
	struct ipa3_flt_tbl *tbl;
	int i;
	int hdr_idx = 0;

	body_i = base;
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;
		tbl = &ipa3_ctx->flt_tbl[i][ip];
		if (tbl->sz[rlt] == 0) {
			hdr_idx++;
			continue;
		}
		if (tbl->in_sys[rlt]) {
			/* only body (no header) with rule-set terminator */
			tbl_mem.size = tbl->sz[rlt] -
				IPA_HW_TBL_HDR_WIDTH + IPA_HW_TBL_WIDTH;
			tbl_mem.base =
				dma_alloc_coherent(ipa3_ctx->pdev, tbl_mem.size,
				&tbl_mem.phys_base, GFP_KERNEL);
			if (!tbl_mem.base) {
				IPAERR("fail to alloc DMA buf of size %d\n",
					tbl_mem.size);
				goto err;
			}
			if (tbl_mem.phys_base & IPA_HW_TBL_SYSADDR_ALIGNMENT) {
				IPAERR("sys rt tbl address is not aligned\n");
				goto align_err;
			}

			/* update the hdr at the right index */
			ipa3_write_64(tbl_mem.phys_base, hdr +
				hdr_idx * IPA_HW_TBL_HDR_WIDTH);

			tbl_mem_buf = tbl_mem.base;
			memset(tbl_mem_buf, 0, tbl_mem.size);

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_flt_rule_list,
				link) {
				if (IPA_FLT_GET_RULE_TYPE(entry) != rlt)
					continue;
				res = ipa3_generate_flt_hw_rule(
					ip, entry, tbl_mem_buf);
				if (res) {
					IPAERR("failed to gen HW FLT rule\n");
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
				hdr + hdr_idx * IPA_HW_TBL_HDR_WIDTH);

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_flt_rule_list,
				link) {
				if (IPA_FLT_GET_RULE_TYPE(entry) != rlt)
					continue;
				res = ipa3_generate_flt_hw_rule(
					ip, entry, body_i);
				if (res) {
					IPAERR("failed to gen HW FLT rule\n");
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
		hdr_idx++;
	}

	return 0;

align_err:
	dma_free_coherent(ipa3_ctx->pdev, tbl_mem.size,
		tbl_mem.base, tbl_mem.phys_base);
err:
	return -EPERM;
}

/**
 * ipa_generate_flt_hw_tbl_img() - generates the flt hw tbls.
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
static int ipa_generate_flt_hw_tbl_img(enum ipa_ip_type ip,
	struct ipa_mem_buffer *hash_hdr, struct ipa_mem_buffer *nhash_hdr,
	struct ipa_mem_buffer *hash_bdy, struct ipa_mem_buffer *nhash_bdy)
{
	u32 hash_bdy_start_ofst, nhash_bdy_start_ofst;
	u32 hash_bdy_sz;
	u32 nhash_bdy_sz;
	struct ipa3_flt_tbl *tbl;
	int rc = 0;
	int i;

	if (ip == IPA_IP_v4) {
		nhash_bdy_start_ofst = IPA_MEM_PART(apps_v4_flt_nhash_ofst) -
			IPA_MEM_PART(v4_flt_nhash_ofst);
		hash_bdy_start_ofst = IPA_MEM_PART(apps_v4_flt_hash_ofst) -
			IPA_MEM_PART(v4_flt_hash_ofst);
	} else {
		nhash_bdy_start_ofst = IPA_MEM_PART(apps_v6_flt_nhash_ofst) -
			IPA_MEM_PART(v6_flt_nhash_ofst);
		hash_bdy_start_ofst = IPA_MEM_PART(apps_v6_flt_hash_ofst) -
			IPA_MEM_PART(v6_flt_hash_ofst);
	}

	if (ipa_alloc_init_flt_tbl_hdr(ip, hash_hdr, nhash_hdr)) {
		IPAERR("fail to alloc and init flt tbl hdr\n");
		rc = -ENOMEM;
		goto no_flt_tbls;
	}

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;
		tbl = &ipa3_ctx->flt_tbl[i][ip];
		if (ipa_prep_flt_tbl_for_cmt(ip, tbl, i)) {
			rc = -EPERM;
			goto prep_failed;
		}
	}

	ipa_get_flt_tbl_lcl_bdy_size(ip, &hash_bdy_sz, &nhash_bdy_sz);
	IPADBG_LOW("total flt tbl local body sizes: hash %u nhash %u\n",
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

	if (ipa_translate_flt_tbl_to_hw_fmt(ip, IPA_RULE_HASHABLE,
		hash_bdy->base, hash_hdr->base, hash_bdy_start_ofst)) {
		IPAERR("fail to translate hashable flt tbls to hw format\n");
		rc = -EPERM;
		goto translate_fail;
	}
	if (ipa_translate_flt_tbl_to_hw_fmt(ip, IPA_RULE_NON_HASHABLE,
		nhash_bdy->base, nhash_hdr->base, nhash_bdy_start_ofst)) {
		IPAERR("fail to translate non-hash flt tbls to hw format\n");
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
no_flt_tbls:
	return rc;
}

/**
 * ipa_flt_valid_lcl_tbl_size() - validate if the space allocated for flt
 * tbl bodies at the sram is enough for the commit
 * @ipt: the ip address family type
 * @rlt: the rule type (hashable or non-hashable)
 *
 * Return: true if enough space available or false in other cases
 */
static bool ipa_flt_valid_lcl_tbl_size(enum ipa_ip_type ipt,
	enum ipa_rule_type rlt, struct ipa_mem_buffer *bdy)
{
	u16 avail;

	if (!bdy) {
		IPAERR("Bad parameters, bdy = NULL\n");
		return false;
	}

	if (ipt == IPA_IP_v4)
		avail = (rlt == IPA_RULE_HASHABLE) ?
			IPA_MEM_PART(apps_v4_flt_hash_size) :
			IPA_MEM_PART(apps_v4_flt_nhash_size);
	else
		avail = (rlt == IPA_RULE_HASHABLE) ?
			IPA_MEM_PART(apps_v6_flt_hash_size) :
			IPA_MEM_PART(apps_v6_flt_nhash_size);

	if (bdy->size <= avail)
		return true;

	IPAERR("tbl too big, needed %d avail %d ipt %d rlt %d\n",
	       bdy->size, avail, ipt, rlt);
	return false;
}

/**
 * ipa_flt_alloc_cmd_buffers() - alloc descriptors and imm cmds
 *  payload pointers buffers for headers and bodies of flt structure
 *  as well as place for flush imm.
 * @ipt: the ip address family type
 * @desc: [OUT] descriptor buffer
 * @cmd: [OUT] imm commands payload pointers buffer
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_flt_alloc_cmd_buffers(enum ipa_ip_type ip,
	struct ipa3_desc **desc, struct ipahal_imm_cmd_pyld ***cmd_pyld)
{
	u16 entries;

	/* +3: 2 for bodies (hashable and non-hashable) and 1 for flushing */
	entries = (ipa3_ctx->ep_flt_num) * 2 + 3;

	*desc = kcalloc(entries, sizeof(**desc), GFP_ATOMIC);
	if (*desc == NULL) {
		IPAERR("fail to alloc desc blob ip %d\n", ip);
		goto fail_desc_alloc;
	}

	*cmd_pyld = kcalloc(entries, sizeof(**cmd_pyld), GFP_ATOMIC);
	if (*cmd_pyld == NULL) {
		IPAERR("fail to alloc cmd pyld blob ip %d\n", ip);
		goto fail_cmd_alloc;
	}

	return 0;

fail_cmd_alloc:
	kfree(*desc);
fail_desc_alloc:
	return -ENOMEM;
}

/**
 * ipa_flt_skip_pipe_config() - skip ep flt configuration or not?
 *  will skip according to pre-configuration or modem pipes
 * @pipe: the EP pipe index
 *
 * Return: true if to skip, false otherwize
 */
static bool ipa_flt_skip_pipe_config(int pipe)
{
	if (ipa_is_modem_pipe(pipe)) {
		IPADBG_LOW("skip %d - modem owned pipe\n", pipe);
		return true;
	}

	if (ipa3_ctx->skip_ep_cfg_shadow[pipe]) {
		IPADBG_LOW("skip %d\n", pipe);
		return true;
	}

	if ((ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_WAN_PROD) == pipe
		&& ipa3_ctx->modem_cfg_emb_pipe_flt)) {
		IPADBG_LOW("skip %d\n", pipe);
		return true;
	}

	return false;
}

/**
 * __ipa_commit_flt_v3() - commit flt tables to the hw
 *  commit the headers and the bodies if are local with internal cache flushing.
 *  The headers (and local bodies) will first be created into dma buffers and
 *  then written via IC to the SRAM
 * @ipt: the ip address family type
 *
 * Return: 0 on success, negative on failure
 */
int __ipa_commit_flt_v3(enum ipa_ip_type ip)
{
	struct ipa_mem_buffer hash_bdy, nhash_bdy;
	struct ipa_mem_buffer hash_hdr, nhash_hdr;
	int rc = 0;
	struct ipa3_desc *desc;
	struct ipahal_imm_cmd_register_write reg_write_cmd = {0};
	struct ipahal_imm_cmd_dma_shared_mem mem_cmd = {0};
	struct ipahal_imm_cmd_pyld **cmd_pyld;
	int num_cmd = 0;
	int i;
	int hdr_idx;
	u32 lcl_hash_hdr, lcl_nhash_hdr;
	u32 lcl_hash_bdy, lcl_nhash_bdy;
	bool lcl_hash, lcl_nhash;
	struct ipahal_reg_fltrt_hash_flush flush;
	struct ipahal_reg_valmask valmask;

	if (ip == IPA_IP_v4) {
		lcl_hash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_flt_hash_ofst) +
			IPA_HW_TBL_HDR_WIDTH; /* to skip the bitmap */
		lcl_nhash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_flt_nhash_ofst) +
			IPA_HW_TBL_HDR_WIDTH; /* to skip the bitmap */
		lcl_hash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_flt_hash_ofst);
		lcl_nhash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_flt_nhash_ofst);
		lcl_hash = ipa3_ctx->ip4_flt_tbl_hash_lcl;
		lcl_nhash = ipa3_ctx->ip4_flt_tbl_nhash_lcl;
	} else {
		lcl_hash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_flt_hash_ofst) +
			IPA_HW_TBL_HDR_WIDTH; /* to skip the bitmap */
		lcl_nhash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_flt_nhash_ofst) +
			IPA_HW_TBL_HDR_WIDTH; /* to skip the bitmap */
		lcl_hash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_flt_hash_ofst);
		lcl_nhash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_flt_nhash_ofst);
		lcl_hash = ipa3_ctx->ip6_flt_tbl_hash_lcl;
		lcl_nhash = ipa3_ctx->ip6_flt_tbl_nhash_lcl;
	}

	if (ipa_generate_flt_hw_tbl_img(ip,
		&hash_hdr, &nhash_hdr, &hash_bdy, &nhash_bdy)) {
		IPAERR("fail to generate FLT HW TBL image. IP %d\n", ip);
		rc = -EFAULT;
		goto fail_gen;
	}

	if (!ipa_flt_valid_lcl_tbl_size(ip, IPA_RULE_HASHABLE, &hash_bdy)) {
		rc = -EFAULT;
		goto fail_size_valid;
	}
	if (!ipa_flt_valid_lcl_tbl_size(ip, IPA_RULE_NON_HASHABLE,
		&nhash_bdy)) {
		rc = -EFAULT;
		goto fail_size_valid;
	}

	if (ipa_flt_alloc_cmd_buffers(ip, &desc, &cmd_pyld)) {
		rc = -ENOMEM;
		goto fail_size_valid;
	}

	/* flushing ipa internal hashable flt rules cache */
	memset(&flush, 0, sizeof(flush));
	if (ip == IPA_IP_v4)
		flush.v4_flt = true;
	else
		flush.v6_flt = true;
	ipahal_get_fltrt_hash_flush_valmask(&flush, &valmask);
	reg_write_cmd.skip_pipeline_clear = false;
	reg_write_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	reg_write_cmd.offset = ipahal_get_reg_ofst(IPA_FILT_ROUT_HASH_FLUSH);
	reg_write_cmd.value = valmask.val;
	reg_write_cmd.value_mask = valmask.mask;
	cmd_pyld[0] = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_REGISTER_WRITE, &reg_write_cmd, false);
	if (!cmd_pyld[0]) {
		IPAERR("fail construct register_write imm cmd: IP %d\n", ip);
		rc = -EFAULT;
		goto fail_reg_write_construct;
	}
	desc[0].opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_REGISTER_WRITE);
	desc[0].pyld = cmd_pyld[0]->data;
	desc[0].len = cmd_pyld[0]->len;
	desc[0].type = IPA_IMM_CMD_DESC;
	num_cmd++;

	hdr_idx = 0;
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i)) {
			IPADBG_LOW("skip %d - not filtering pipe\n", i);
			continue;
		}

		if (ipa_flt_skip_pipe_config(i)) {
			hdr_idx++;
			continue;
		}

		IPADBG_LOW("Prepare imm cmd for hdr at index %d for pipe %d\n",
			hdr_idx, i);

		mem_cmd.is_read = false;
		mem_cmd.skip_pipeline_clear = false;
		mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		mem_cmd.size = IPA_HW_TBL_HDR_WIDTH;
		mem_cmd.system_addr = nhash_hdr.phys_base +
			hdr_idx * IPA_HW_TBL_HDR_WIDTH;
		mem_cmd.local_addr = lcl_nhash_hdr +
			hdr_idx * IPA_HW_TBL_HDR_WIDTH;
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR("fail construct dma_shared_mem cmd: IP = %d\n",
				ip);
			goto fail_imm_cmd_construct;
		}
		desc[num_cmd].opcode =
			ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
		desc[num_cmd].pyld = cmd_pyld[num_cmd]->data;
		desc[num_cmd].len = cmd_pyld[num_cmd]->len;
		desc[num_cmd++].type = IPA_IMM_CMD_DESC;

		mem_cmd.is_read = false;
		mem_cmd.skip_pipeline_clear = false;
		mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		mem_cmd.size = IPA_HW_TBL_HDR_WIDTH;
		mem_cmd.system_addr = hash_hdr.phys_base +
			hdr_idx * IPA_HW_TBL_HDR_WIDTH;
		mem_cmd.local_addr = lcl_hash_hdr +
			hdr_idx * IPA_HW_TBL_HDR_WIDTH;
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR("fail construct dma_shared_mem cmd: IP = %d\n",
				ip);
			goto fail_imm_cmd_construct;
		}
		desc[num_cmd].opcode =
			ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
		desc[num_cmd].pyld = cmd_pyld[num_cmd]->data;
		desc[num_cmd].len = cmd_pyld[num_cmd]->len;
		desc[num_cmd++].type = IPA_IMM_CMD_DESC;

		hdr_idx++;
	}

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
			IPAERR("fail construct dma_shared_mem cmd: IP = %d\n",
				ip);
			goto fail_imm_cmd_construct;
		}
		desc[num_cmd].opcode =
			ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
		desc[num_cmd].pyld = cmd_pyld[num_cmd]->data;
		desc[num_cmd].len = cmd_pyld[num_cmd]->len;
		desc[num_cmd++].type = IPA_IMM_CMD_DESC;
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
			IPAERR("fail construct dma_shared_mem cmd: IP = %d\n",
				ip);
			goto fail_imm_cmd_construct;
		}
		desc[num_cmd].opcode =
			ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
		desc[num_cmd].pyld = cmd_pyld[num_cmd]->data;
		desc[num_cmd].len = cmd_pyld[num_cmd]->len;
		desc[num_cmd++].type = IPA_IMM_CMD_DESC;
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

	__ipa_reap_sys_flt_tbls(ip, IPA_RULE_HASHABLE);
	__ipa_reap_sys_flt_tbls(ip, IPA_RULE_NON_HASHABLE);

fail_imm_cmd_construct:
	for (i = 0 ; i < num_cmd ; i++)
		ipahal_destroy_imm_cmd(cmd_pyld[i]);
fail_reg_write_construct:
	kfree(desc);
	kfree(cmd_pyld);
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

static int __ipa_validate_flt_rule(const struct ipa_flt_rule *rule,
		struct ipa3_rt_tbl **rt_tbl, enum ipa_ip_type ip)
{
	if (rule->action != IPA_PASS_TO_EXCEPTION) {
		if (!rule->eq_attrib_type) {
			if (!rule->rt_tbl_hdl) {
				IPAERR("invalid RT tbl\n");
				goto error;
			}

			*rt_tbl = ipa3_id_find(rule->rt_tbl_hdl);
			if (*rt_tbl == NULL) {
				IPAERR("RT tbl not found\n");
				goto error;
			}

			if ((*rt_tbl)->cookie != IPA_COOKIE) {
				IPAERR("RT table cookie is invalid\n");
				goto error;
			}
		} else {
			if (rule->rt_tbl_idx > ((ip == IPA_IP_v4) ?
				IPA_MEM_PART(v4_modem_rt_index_hi) :
				IPA_MEM_PART(v6_modem_rt_index_hi))) {
				IPAERR("invalid RT tbl\n");
				goto error;
			}
		}
	}

	if (rule->rule_id) {
		if (rule->rule_id >= IPA_RULE_ID_MIN_VAL &&
		    rule->rule_id <= IPA_RULE_ID_MAX_VAL) {
			IPAERR("invalid rule_id provided 0x%x\n"
				"rule_id 0x%x - 0x%x  are auto generated\n",
				rule->rule_id,
				IPA_RULE_ID_MIN_VAL,
				IPA_RULE_ID_MAX_VAL);
			goto error;
		}
	}

	return 0;

error:
	return -EPERM;
}

static int __ipa_create_flt_entry(struct ipa3_flt_entry **entry,
		const struct ipa_flt_rule *rule, struct ipa3_rt_tbl *rt_tbl,
		struct ipa3_flt_tbl *tbl)
{
	int id;

	*entry = kmem_cache_zalloc(ipa3_ctx->flt_rule_cache, GFP_KERNEL);
	if (!*entry) {
		IPAERR("failed to alloc FLT rule object\n");
		goto error;
	}
	INIT_LIST_HEAD(&((*entry)->link));
	(*entry)->rule = *rule;
	(*entry)->cookie = IPA_COOKIE;
	(*entry)->rt_tbl = rt_tbl;
	(*entry)->tbl = tbl;
	if (rule->rule_id) {
		id = rule->rule_id;
	} else {
		id = ipa3_alloc_rule_id(&tbl->rule_ids);
		if (id < 0) {
			IPAERR("failed to allocate rule id\n");
			WARN_ON(1);
			goto rule_id_fail;
		}
	}
	(*entry)->rule_id = id;

	return 0;

rule_id_fail:
	kmem_cache_free(ipa3_ctx->flt_rule_cache, *entry);
error:
	return -EPERM;
}

static int __ipa_finish_flt_rule_add(struct ipa3_flt_tbl *tbl,
		struct ipa3_flt_entry *entry, u32 *rule_hdl)
{
	int id;

	tbl->rule_cnt++;
	if (entry->rt_tbl)
		entry->rt_tbl->ref_cnt++;
	id = ipa3_id_alloc(entry);
	if (id < 0) {
		IPAERR("failed to add to tree\n");
		WARN_ON(1);
	}
	*rule_hdl = id;
	entry->id = id;
	IPADBG_LOW("add flt rule rule_cnt=%d\n", tbl->rule_cnt);

	return 0;
}

static int __ipa_add_flt_rule(struct ipa3_flt_tbl *tbl, enum ipa_ip_type ip,
			      const struct ipa_flt_rule *rule, u8 add_rear,
			      u32 *rule_hdl)
{
	struct ipa3_flt_entry *entry;
	struct ipa3_rt_tbl *rt_tbl = NULL;

	if (__ipa_validate_flt_rule(rule, &rt_tbl, ip))
		goto error;

	if (__ipa_create_flt_entry(&entry, rule, rt_tbl, tbl))
		goto error;

	if (add_rear) {
		if (tbl->sticky_rear)
			list_add_tail(&entry->link,
					tbl->head_flt_rule_list.prev);
		else
			list_add_tail(&entry->link, &tbl->head_flt_rule_list);
	} else {
		list_add(&entry->link, &tbl->head_flt_rule_list);
	}

	__ipa_finish_flt_rule_add(tbl, entry, rule_hdl);

	return 0;

error:
	return -EPERM;
}

static int __ipa_add_flt_rule_after(struct ipa3_flt_tbl *tbl,
				const struct ipa_flt_rule *rule,
				u32 *rule_hdl,
				enum ipa_ip_type ip,
				struct ipa3_flt_entry **add_after_entry)
{
	struct ipa3_flt_entry *entry;
	struct ipa3_rt_tbl *rt_tbl = NULL;

	if (!*add_after_entry)
		goto error;

	if (rule == NULL || rule_hdl == NULL) {
		IPAERR("bad parms rule=%p rule_hdl=%p\n", rule,
				rule_hdl);
		goto error;
	}

	if (__ipa_validate_flt_rule(rule, &rt_tbl, ip))
		goto error;

	if (__ipa_create_flt_entry(&entry, rule, rt_tbl, tbl))
		goto error;

	list_add(&entry->link, &((*add_after_entry)->link));

	__ipa_finish_flt_rule_add(tbl, entry, rule_hdl);

	/*
	 * prepare for next insertion
	 */
	*add_after_entry = entry;

	return 0;

error:
	*add_after_entry = NULL;
	return -EPERM;
}

static int __ipa_del_flt_rule(u32 rule_hdl)
{
	struct ipa3_flt_entry *entry;
	int id;

	entry = ipa3_id_find(rule_hdl);
	if (entry == NULL) {
		IPAERR("lookup failed\n");
		return -EINVAL;
	}

	if (entry->cookie != IPA_COOKIE) {
		IPAERR("bad params\n");
		return -EINVAL;
	}
	id = entry->id;

	list_del(&entry->link);
	entry->tbl->rule_cnt--;
	if (entry->rt_tbl)
		entry->rt_tbl->ref_cnt--;
	IPADBG("del flt rule rule_cnt=%d rule_id=%d\n",
		entry->tbl->rule_cnt, entry->rule_id);
	entry->cookie = 0;
	/* if rule id was allocated from idr, remove it */
	if (entry->rule_id >= IPA_RULE_ID_MIN_VAL &&
	    entry->rule_id <= IPA_RULE_ID_MAX_VAL)
		idr_remove(&entry->tbl->rule_ids, entry->rule_id);
	kmem_cache_free(ipa3_ctx->flt_rule_cache, entry);

	/* remove the handle from the database */
	ipa3_id_remove(id);

	return 0;
}

static int __ipa_mdfy_flt_rule(struct ipa_flt_rule_mdfy *frule,
		enum ipa_ip_type ip)
{
	struct ipa3_flt_entry *entry;
	struct ipa3_rt_tbl *rt_tbl = NULL;

	entry = ipa3_id_find(frule->rule_hdl);
	if (entry == NULL) {
		IPAERR("lookup failed\n");
		goto error;
	}

	if (entry->cookie != IPA_COOKIE) {
		IPAERR("bad params\n");
		goto error;
	}

	if (entry->rt_tbl)
		entry->rt_tbl->ref_cnt--;

	if (frule->rule.action != IPA_PASS_TO_EXCEPTION) {
		if (!frule->rule.eq_attrib_type) {
			if (!frule->rule.rt_tbl_hdl) {
				IPAERR("invalid RT tbl\n");
				goto error;
			}

			rt_tbl = ipa3_id_find(frule->rule.rt_tbl_hdl);
			if (rt_tbl == NULL) {
				IPAERR("RT tbl not found\n");
				goto error;
			}

			if (rt_tbl->cookie != IPA_COOKIE) {
				IPAERR("RT table cookie is invalid\n");
				goto error;
			}
		} else {
			if (frule->rule.rt_tbl_idx > ((ip == IPA_IP_v4) ?
				IPA_MEM_PART(v4_modem_rt_index_hi) :
				IPA_MEM_PART(v6_modem_rt_index_hi))) {
				IPAERR("invalid RT tbl\n");
				goto error;
			}
		}
	}

	entry->rule = frule->rule;
	entry->rt_tbl = rt_tbl;
	if (entry->rt_tbl)
		entry->rt_tbl->ref_cnt++;
	entry->hw_len = 0;
	entry->prio = 0;

	return 0;

error:
	return -EPERM;
}

static int __ipa_add_flt_get_ep_idx(enum ipa_client_type ep, int *ipa_ep_idx)
{
	*ipa_ep_idx = ipa3_get_ep_mapping(ep);
	if (*ipa_ep_idx == IPA_FLT_TABLE_INDEX_NOT_FOUND) {
		IPAERR("ep not valid ep=%d\n", ep);
		return -EINVAL;
	}
	if (ipa3_ctx->ep[*ipa_ep_idx].valid == 0)
		IPADBG("ep not connected ep_idx=%d\n", *ipa_ep_idx);

	if (!ipa_is_ep_support_flt(*ipa_ep_idx)) {
		IPAERR("ep do not support filtering ep=%d\n", ep);
		return -EINVAL;
	}

	return 0;
}

static int __ipa_add_ep_flt_rule(enum ipa_ip_type ip, enum ipa_client_type ep,
				 const struct ipa_flt_rule *rule, u8 add_rear,
				 u32 *rule_hdl)
{
	struct ipa3_flt_tbl *tbl;
	int ipa_ep_idx;

	if (rule == NULL || rule_hdl == NULL || ep >= IPA_CLIENT_MAX) {
		IPAERR("bad parms rule=%p rule_hdl=%p ep=%d\n", rule,
				rule_hdl, ep);

		return -EINVAL;
	}

	if (__ipa_add_flt_get_ep_idx(ep, &ipa_ep_idx))
		return -EINVAL;

	tbl = &ipa3_ctx->flt_tbl[ipa_ep_idx][ip];
	IPADBG_LOW("add ep flt rule ip=%d ep=%d\n", ip, ep);

	return __ipa_add_flt_rule(tbl, ip, rule, add_rear, rule_hdl);
}

/**
 * ipa3_add_flt_rule() - Add the specified filtering rules to SW and optionally
 * commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_add_flt_rule(struct ipa_ioc_add_flt_rule *rules)
{
	int i;
	int result;

	if (rules == NULL || rules->num_rules == 0 ||
			rules->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");

		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < rules->num_rules; i++) {
		if (!rules->global)
			result = __ipa_add_ep_flt_rule(rules->ip, rules->ep,
					&rules->rules[i].rule,
					rules->rules[i].at_rear,
					&rules->rules[i].flt_rule_hdl);
		else
			result = -1;

		if (result) {
			IPAERR("failed to add flt rule %d\n", i);
			rules->rules[i].status = IPA_FLT_STATUS_OF_ADD_FAILED;
		} else {
			rules->rules[i].status = 0;
		}
	}

	if (rules->global) {
		IPAERR("no support for global filter rules\n");
		result = -EPERM;
		goto bail;
	}

	if (rules->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_flt(rules->ip)) {
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_add_flt_rule_after() - Add the specified filtering rules to SW after
 *  the rule which its handle is given and optionally commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_add_flt_rule_after(struct ipa_ioc_add_flt_rule_after *rules)
{
	int i;
	int result;
	struct ipa3_flt_tbl *tbl;
	int ipa_ep_idx;
	struct ipa3_flt_entry *entry;

	if (rules == NULL || rules->num_rules == 0 ||
			rules->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	if (rules->ep >= IPA_CLIENT_MAX) {
		IPAERR("bad parms ep=%d\n", rules->ep);
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);

	if (__ipa_add_flt_get_ep_idx(rules->ep, &ipa_ep_idx)) {
		result = -EINVAL;
		goto bail;
	}

	tbl = &ipa3_ctx->flt_tbl[ipa_ep_idx][rules->ip];

	entry = ipa3_id_find(rules->add_after_hdl);
	if (entry == NULL) {
		IPAERR("lookup failed\n");
		result = -EINVAL;
		goto bail;
	}

	if (entry->tbl != tbl) {
		IPAERR("given entry does not match the table\n");
		result = -EINVAL;
		goto bail;
	}

	if (tbl->sticky_rear)
		if (&entry->link == tbl->head_flt_rule_list.prev) {
			IPAERR("cannot add rule at end of a sticky table");
			result = -EINVAL;
			goto bail;
		}

	IPADBG("add ep flt rule ip=%d ep=%d after hdl %d\n",
			rules->ip, rules->ep, rules->add_after_hdl);

	/*
	 * we add all rules one after the other, if one insertion fails, it cuts
	 * the chain (all following will receive fail status) following calls to
	 * __ipa_add_flt_rule_after will fail (entry == NULL)
	 */

	for (i = 0; i < rules->num_rules; i++) {
		result = __ipa_add_flt_rule_after(tbl,
				&rules->rules[i].rule,
				&rules->rules[i].flt_rule_hdl,
				rules->ip,
				&entry);

		if (result) {
			IPAERR("failed to add flt rule %d\n", i);
			rules->rules[i].status = IPA_FLT_STATUS_OF_ADD_FAILED;
		} else {
			rules->rules[i].status = 0;
		}
	}

	if (rules->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_flt(rules->ip)) {
			IPAERR("failed to commit flt rules\n");
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_del_flt_rule() - Remove the specified filtering rules from SW and
 * optionally commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls)
{
	int i;
	int result;

	if (hdls == NULL || hdls->num_hdls == 0 || hdls->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa_del_flt_rule(hdls->hdl[i].hdl)) {
			IPAERR("failed to del flt rule %i\n", i);
			hdls->hdl[i].status = IPA_FLT_STATUS_OF_DEL_FAILED;
		} else {
			hdls->hdl[i].status = 0;
		}
	}

	if (hdls->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_flt(hdls->ip)) {
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_mdfy_flt_rule() - Modify the specified filtering rules in SW and optionally
 * commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_mdfy_flt_rule(struct ipa_ioc_mdfy_flt_rule *hdls)
{
	int i;
	int result;

	if (hdls == NULL || hdls->num_rules == 0 || hdls->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < hdls->num_rules; i++) {
		if (__ipa_mdfy_flt_rule(&hdls->rules[i], hdls->ip)) {
			IPAERR("failed to mdfy flt rule %i\n", i);
			hdls->rules[i].status = IPA_FLT_STATUS_OF_MDFY_FAILED;
		} else {
			hdls->rules[i].status = 0;
		}
	}

	if (hdls->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_flt(hdls->ip)) {
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}


/**
 * ipa3_commit_flt() - Commit the current SW filtering table of specified type to
 * IPA HW
 * @ip:	[in] the family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_commit_flt(enum ipa_ip_type ip)
{
	int result;

	if (ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);

	if (ipa3_ctx->ctrl->ipa3_commit_flt(ip)) {
		result = -EPERM;
		goto bail;
	}
	result = 0;

bail:
	mutex_unlock(&ipa3_ctx->lock);

	return result;
}

/**
 * ipa3_reset_flt() - Reset the current SW filtering table of specified type
 * (does not commit to HW)
 * @ip:	[in] the family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_reset_flt(enum ipa_ip_type ip)
{
	struct ipa3_flt_tbl *tbl;
	struct ipa3_flt_entry *entry;
	struct ipa3_flt_entry *next;
	int i;
	int id;

	if (ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;

		tbl = &ipa3_ctx->flt_tbl[i][ip];
		list_for_each_entry_safe(entry, next, &tbl->head_flt_rule_list,
				link) {
			if (ipa3_id_find(entry->id) == NULL) {
				WARN_ON(1);
				mutex_unlock(&ipa3_ctx->lock);
				return -EFAULT;
			}
			list_del(&entry->link);
			entry->tbl->rule_cnt--;
			if (entry->rt_tbl)
				entry->rt_tbl->ref_cnt--;
			/* if rule id was allocated from idr, remove it */
			if (entry->rule_id >= IPA_RULE_ID_MIN_VAL &&
			    entry->rule_id <= IPA_RULE_ID_MAX_VAL)
				idr_remove(&entry->tbl->rule_ids,
						entry->rule_id);
			entry->cookie = 0;
			id = entry->id;
			kmem_cache_free(ipa3_ctx->flt_rule_cache, entry);

			/* remove the handle from the database */
			ipa3_id_remove(id);
		}
	}
	mutex_unlock(&ipa3_ctx->lock);

	return 0;
}

void ipa3_install_dflt_flt_rules(u32 ipa_ep_idx)
{
	struct ipa3_flt_tbl *tbl;
	struct ipa3_ep_context *ep = &ipa3_ctx->ep[ipa_ep_idx];
	struct ipa_flt_rule rule;

	if (!ipa_is_ep_support_flt(ipa_ep_idx)) {
		IPADBG("cannot add flt rules to non filtering pipe num %d\n",
			ipa_ep_idx);
		return;
	}

	memset(&rule, 0, sizeof(rule));

	mutex_lock(&ipa3_ctx->lock);
	tbl = &ipa3_ctx->flt_tbl[ipa_ep_idx][IPA_IP_v4];
	tbl->sticky_rear = true;
	rule.action = IPA_PASS_TO_EXCEPTION;
	__ipa_add_flt_rule(tbl, IPA_IP_v4, &rule, false,
			&ep->dflt_flt4_rule_hdl);
	ipa3_ctx->ctrl->ipa3_commit_flt(IPA_IP_v4);

	tbl = &ipa3_ctx->flt_tbl[ipa_ep_idx][IPA_IP_v6];
	tbl->sticky_rear = true;
	rule.action = IPA_PASS_TO_EXCEPTION;
	__ipa_add_flt_rule(tbl, IPA_IP_v6, &rule, false,
			&ep->dflt_flt6_rule_hdl);
	ipa3_ctx->ctrl->ipa3_commit_flt(IPA_IP_v6);
	mutex_unlock(&ipa3_ctx->lock);
}

void ipa3_delete_dflt_flt_rules(u32 ipa_ep_idx)
{
	struct ipa3_ep_context *ep = &ipa3_ctx->ep[ipa_ep_idx];

	mutex_lock(&ipa3_ctx->lock);
	if (ep->dflt_flt4_rule_hdl) {
		__ipa_del_flt_rule(ep->dflt_flt4_rule_hdl);
		ipa3_ctx->ctrl->ipa3_commit_flt(IPA_IP_v4);
		ep->dflt_flt4_rule_hdl = 0;
	}
	if (ep->dflt_flt6_rule_hdl) {
		__ipa_del_flt_rule(ep->dflt_flt6_rule_hdl);
		ipa3_ctx->ctrl->ipa3_commit_flt(IPA_IP_v6);
		ep->dflt_flt6_rule_hdl = 0;
	}
	mutex_unlock(&ipa3_ctx->lock);
}

/**
 * ipa3_set_flt_tuple_mask() - Sets the flt tuple masking for the given pipe
 *  Pipe must be for AP EP (not modem) and support filtering
 *  updates the the filtering masking values without changing the rt ones.
 *
 * @pipe_idx: filter pipe index to configure the tuple masking
 * @tuple: the tuple members masking
 * Returns:	0 on success, negative on failure
 *
 */
int ipa3_set_flt_tuple_mask(int pipe_idx, struct ipahal_reg_hash_tuple *tuple)
{
	struct ipahal_reg_fltrt_hash_tuple fltrt_tuple;

	if (!tuple) {
		IPAERR("bad tuple\n");
		return -EINVAL;
	}

	if (pipe_idx >= ipa3_ctx->ipa_num_pipes || pipe_idx < 0) {
		IPAERR("bad pipe index!\n");
		return -EINVAL;
	}

	if (!ipa_is_ep_support_flt(pipe_idx)) {
		IPAERR("pipe %d not filtering pipe\n", pipe_idx);
		return -EINVAL;
	}

	if (ipa_is_modem_pipe(pipe_idx)) {
		IPAERR("modem pipe tuple is not configured by AP\n");
		return -EINVAL;
	}

	ipahal_read_reg_n_fields(IPA_ENDP_FILTER_ROUTER_HSH_CFG_n,
		pipe_idx, &fltrt_tuple);
	fltrt_tuple.flt = *tuple;
	ipahal_write_reg_n_fields(IPA_ENDP_FILTER_ROUTER_HSH_CFG_n,
		pipe_idx, &fltrt_tuple);

	return 0;
}

/**
 * ipa3_flt_read_tbl_from_hw() -Read filtering table from IPA HW
 * @pipe_idx: IPA endpoint index
 * @ip_type: IPv4 or IPv6 table
 * @hashable: hashable or non-hashable table
 * @entry: array to fill the table entries
 * @num_entry: number of entries in entry array. set by the caller to indicate
 *  entry array size. Then set by this function as an output parameter to
 *  indicate the number of entries in the array
 *
 * This function reads the filtering table from IPA SRAM and prepares an array
 * of entries. This function is mainly used for debugging purposes.

 * Returns:	0 on success, negative on failure
 */
int ipa3_flt_read_tbl_from_hw(u32 pipe_idx,
	enum ipa_ip_type ip_type,
	bool hashable,
	struct ipa3_flt_entry entry[],
	int *num_entry)
{
	int tbl_entry_idx;
	u64 tbl_entry_in_hdr_ofst;
	u64 *tbl_entry_in_hdr;
	struct ipa3_flt_rule_hw_hdr *hdr;
	u8 *buf;
	int rule_idx;
	u8 rule_size;
	int i;
	void *ipa_sram_mmio;

	IPADBG("pipe_idx=%d ip_type=%d hashable=%d\n",
		pipe_idx, ip_type, hashable);

	if (pipe_idx >= ipa3_ctx->ipa_num_pipes ||
	    ip_type >= IPA_IP_MAX ||
	    !entry || !num_entry) {
		IPAERR("Invalid params\n");
		return -EFAULT;
	}

	if (!ipa_is_ep_support_flt(pipe_idx)) {
		IPAERR("pipe %d does not support filtering\n", pipe_idx);
		return -EINVAL;
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
	/* calculate the offset of the tbl entry */
	tbl_entry_idx = 1; /* to skip the bitmap */
	for (i = 0; i < pipe_idx; i++)
		if (ipa3_ctx->ep_flt_bitmap & (1 << i))
			tbl_entry_idx++;

	if (hashable) {
		if (ip_type == IPA_IP_v4)
			tbl_entry_in_hdr_ofst =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v4_flt_hash_ofst) +
				tbl_entry_idx * IPA_HW_TBL_HDR_WIDTH;
		else
			tbl_entry_in_hdr_ofst =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v6_flt_hash_ofst) +
				tbl_entry_idx * IPA_HW_TBL_HDR_WIDTH;
	} else {
		if (ip_type == IPA_IP_v4)
			tbl_entry_in_hdr_ofst =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v4_flt_nhash_ofst) +
				tbl_entry_idx * IPA_HW_TBL_HDR_WIDTH;
		else
			tbl_entry_in_hdr_ofst =
				ipa3_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v6_flt_nhash_ofst) +
				tbl_entry_idx * IPA_HW_TBL_HDR_WIDTH;
	}

	IPADBG("tbl_entry_in_hdr_ofst=0x%llx\n", tbl_entry_in_hdr_ofst);

	tbl_entry_in_hdr = ipa_sram_mmio + tbl_entry_in_hdr_ofst;

	/* for tables resides in DDR access it from the virtual memory */
	if (*tbl_entry_in_hdr & 0x1) {
		/* local */
		hdr = (void *)((u8 *)tbl_entry_in_hdr -
			tbl_entry_idx * IPA_HW_TBL_HDR_WIDTH +
			(*tbl_entry_in_hdr - 1) / 16);
	} else {
		/* system */
		if (hashable)
			hdr = ipa3_ctx->flt_tbl[pipe_idx][ip_type].
				curr_mem[IPA_RULE_HASHABLE].base;
		else
			hdr = ipa3_ctx->flt_tbl[pipe_idx][ip_type].
				curr_mem[IPA_RULE_NON_HASHABLE].base;

		if (!hdr)
			hdr = ipa3_ctx->empty_rt_tbl_mem.base;
	}
	IPADBG("*tbl_entry_in_hdr=0x%llx\n", *tbl_entry_in_hdr);
	IPADBG("hdr=0x%p\n", hdr);

	rule_idx = 0;
	while (rule_idx < *num_entry) {
		IPADBG("*((u64 *)hdr)=0x%llx\n", *((u64 *)hdr));
		if (*((u64 *)hdr) == 0)
			break;
		entry[rule_idx].rule.eq_attrib_type = true;
		entry[rule_idx].rule.eq_attrib.rule_eq_bitmap =
			hdr->u.hdr.en_rule;
		entry[rule_idx].rule.action = hdr->u.hdr.action;
		entry[rule_idx].rule.retain_hdr = hdr->u.hdr.retain_hdr;
		entry[rule_idx].rule.rt_tbl_idx = hdr->u.hdr.rt_tbl_idx;
		entry[rule_idx].prio = hdr->u.hdr.priority;
		entry[rule_idx].rule_id = entry->rule.rule_id =
			hdr->u.hdr.rule_id;
		buf = (u8 *)(hdr + 1);
		IPADBG("buf=0x%p\n", buf);

		ipa3_generate_eq_from_hw_rule(&entry[rule_idx].rule.eq_attrib,
			buf, &rule_size);
		IPADBG("rule_size=%d\n", rule_size);
		hdr = (void *)(buf + rule_size);
		IPADBG("hdr=0x%p\n", hdr);
		rule_idx++;
	}

	*num_entry = rule_idx;
	iounmap(ipa_sram_mmio);

	return 0;
}

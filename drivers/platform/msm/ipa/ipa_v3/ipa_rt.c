/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include "ipahal/ipahal_fltrt.h"

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
 * ipa_generate_rt_hw_rule() - Generated the RT H/W single rule
 *  This func will do the preparation core driver work and then calls
 *  the HAL layer for the real work.
 * @ip: the ip address family type
 * @entry: routing entry
 * @buf: output buffer, buf == NULL means
 *	caller wants to know the size of the rule as seen
 *	by HW so they did not pass a valid buffer, we will use a
 *	scratch buffer instead.
 *	With this scheme we are going to
 *	generate the rule twice, once to know size using scratch
 *	buffer and second to write the rule to the actual caller
 *	supplied buffer which is of required size
 *
 * Returns: 0 on success, negative on failure
 *
 * caller needs to hold any needed locks to ensure integrity
 */
static int ipa_generate_rt_hw_rule(enum ipa_ip_type ip,
	struct ipa3_rt_entry *entry, u8 *buf)
{
	struct ipahal_rt_rule_gen_params gen_params;
	int res = 0;

	memset(&gen_params, 0, sizeof(gen_params));

	gen_params.ipt = ip;
	gen_params.dst_pipe_idx = ipa3_get_ep_mapping(entry->rule.dst);
	if (gen_params.dst_pipe_idx == -1) {
		IPAERR("Wrong destination pipe specified in RT rule\n");
		WARN_ON(1);
		return -EPERM;
	}
	if (!IPA_CLIENT_IS_CONS(entry->rule.dst)) {
		IPAERR("No RT rule on IPA_client_producer pipe.\n");
		IPAERR("pipe_idx: %d dst_pipe: %d\n",
				gen_params.dst_pipe_idx, entry->rule.dst);
		WARN_ON(1);
		return -EPERM;
	}

	if (entry->proc_ctx || (entry->hdr && entry->hdr->is_hdr_proc_ctx)) {
		struct ipa3_hdr_proc_ctx_entry *proc_ctx;
		proc_ctx = (entry->proc_ctx) ? : entry->hdr->proc_ctx;
		gen_params.hdr_lcl = ipa3_ctx->hdr_proc_ctx_tbl_lcl;
		gen_params.hdr_type = IPAHAL_RT_RULE_HDR_PROC_CTX;
		gen_params.hdr_ofst = proc_ctx->offset_entry->offset +
			ipa3_ctx->hdr_proc_ctx_tbl.start_offset;
	} else if (entry->hdr) {
		gen_params.hdr_lcl = ipa3_ctx->hdr_tbl_lcl;
		gen_params.hdr_type = IPAHAL_RT_RULE_HDR_RAW;
		gen_params.hdr_ofst = entry->hdr->offset_entry->offset;
	} else {
		gen_params.hdr_type = IPAHAL_RT_RULE_HDR_NONE;
		gen_params.hdr_ofst = 0;
	}

	gen_params.priority = entry->prio;
	gen_params.id = entry->rule_id;
	gen_params.rule = (const struct ipa_rt_rule *)&entry->rule;

	res = ipahal_rt_generate_hw_rule(&gen_params, &entry->hw_len, buf);
	if (res)
		IPAERR("failed to generate rt h/w rule\n");

	return res;
}

/**
 * ipa_translate_rt_tbl_to_hw_fmt() - translate the routing driver structures
 *  (rules and tables) to HW format and fill it in the given buffers
 * @ip: the ip address family type
 * @rlt: the type of the rules to translate (hashable or non-hashable)
 * @base: the rules body buffer to be filled
 * @hdr: the rules header (addresses/offsets) buffer to be filled
 * @body_ofst: the offset of the rules body from the rules header at
 *  ipa sram (for local body usage)
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
			/* only body (no header) */
			tbl_mem.size = tbl->sz[rlt] -
				ipahal_get_hw_tbl_hdr_width();
			if (ipahal_fltrt_allocate_hw_sys_tbl(&tbl_mem)) {
				IPAERR("fail to alloc sys tbl of size %d\n",
					tbl_mem.size);
				goto err;
			}

			if (ipahal_fltrt_write_addr_to_hdr(tbl_mem.phys_base,
				hdr, tbl->idx - apps_start_idx, true)) {
				IPAERR("fail to wrt sys tbl addr to hdr\n");
				goto hdr_update_fail;
			}

			tbl_mem_buf = tbl_mem.base;

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_rt_rule_list,
					link) {
				if (IPA_RT_GET_RULE_TYPE(entry) != rlt)
					continue;
				res = ipa_generate_rt_hw_rule(ip, entry,
					tbl_mem_buf);
				if (res) {
					IPAERR("failed to gen HW RT rule\n");
					goto hdr_update_fail;
				}
				tbl_mem_buf += entry->hw_len;
			}

			if (tbl->curr_mem[rlt].phys_base) {
				WARN_ON(tbl->prev_mem[rlt].phys_base);
				tbl->prev_mem[rlt] = tbl->curr_mem[rlt];
			}
			tbl->curr_mem[rlt] = tbl_mem;
		} else {
			offset = body_i - base + body_ofst;

			/* update the hdr at the right index */
			if (ipahal_fltrt_write_addr_to_hdr(offset, hdr,
				tbl->idx - apps_start_idx, true)) {
				IPAERR("fail to wrt lcl tbl ofst to hdr\n");
				goto hdr_update_fail;
			}

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_rt_rule_list,
					link) {
				if (IPA_RT_GET_RULE_TYPE(entry) != rlt)
					continue;
				res = ipa_generate_rt_hw_rule(ip, entry,
					body_i);
				if (res) {
					IPAERR("failed to gen HW RT rule\n");
					goto err;
				}
				body_i += entry->hw_len;
			}

			/**
			 * advance body_i to next table alignment as local tables
			 * are order back-to-back
			 */
			body_i += ipahal_get_lcl_tbl_addr_alignment();
			body_i = (u8 *)((long)body_i &
				~ipahal_get_lcl_tbl_addr_alignment());
		}
	}

	return 0;

hdr_update_fail:
	ipahal_free_dma_mem(&tbl_mem);
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
				ipahal_free_dma_mem(&tbl->prev_mem[i]);
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
				ipahal_free_dma_mem(&tbl->curr_mem[i]);
			}
		}
		list_del(&tbl->link);
		kmem_cache_free(ipa3_ctx->rt_tbl_cache, tbl);
	}
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
	int prio_i;
	int res;
	int max_prio;
	u32 hdr_width;

	tbl->sz[IPA_RULE_HASHABLE] = 0;
	tbl->sz[IPA_RULE_NON_HASHABLE] = 0;

	max_prio = ipahal_get_rule_max_priority();

	prio_i = max_prio;
	list_for_each_entry(entry, &tbl->head_rt_rule_list, link) {

		if (entry->rule.max_prio) {
			entry->prio = max_prio;
		} else {
			if (ipahal_rule_decrease_priority(&prio_i)) {
				IPAERR("cannot rule decrease priority - %d\n",
					prio_i);
				return -EPERM;
			}
			entry->prio = prio_i;
		}

		res = ipa_generate_rt_hw_rule(ip, entry, NULL);
		if (res) {
			IPAERR("failed to calculate HW RT rule size\n");
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
		WARN_ON(1);
		IPAERR("rt tbl %s is with zero total size\n", tbl->name);
	}

	hdr_width = ipahal_get_hw_tbl_hdr_width();

	if (tbl->sz[IPA_RULE_HASHABLE])
		tbl->sz[IPA_RULE_HASHABLE] += hdr_width;
	if (tbl->sz[IPA_RULE_NON_HASHABLE])
		tbl->sz[IPA_RULE_NON_HASHABLE] += hdr_width;

	IPADBG("RT tbl index %u hash_sz %u non-hash sz %u\n", tbl->idx,
		tbl->sz[IPA_RULE_HASHABLE], tbl->sz[IPA_RULE_NON_HASHABLE]);

	return 0;
}

/**
 * ipa_generate_rt_hw_tbl_img() - generates the rt hw tbls.
 *  headers and bodies (sys bodies) are being created into buffers that will
 *  be filled into the local memory (sram)
 * @ip: the ip address family type
 * @alloc_params: IN/OUT parameters to hold info regard the tables headers
 *  and bodies on DDR (DMA buffers), and needed info for the allocation
 *  that the HAL needs
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_generate_rt_hw_tbl_img(enum ipa_ip_type ip,
	struct ipahal_fltrt_alloc_imgs_params *alloc_params)
{
	u32 hash_bdy_start_ofst, nhash_bdy_start_ofst;
	u32 apps_start_idx;
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

	if (ipahal_fltrt_allocate_hw_tbl_imgs(alloc_params)) {
		IPAERR("fail to allocate RT HW TBL images. IP %d\n", ip);
		rc = -ENOMEM;
		goto allocate_fail;
	}

	if (ipa_translate_rt_tbl_to_hw_fmt(ip, IPA_RULE_HASHABLE,
		alloc_params->hash_bdy.base, alloc_params->hash_hdr.base,
		hash_bdy_start_ofst, apps_start_idx)) {
		IPAERR("fail to translate hashable rt tbls to hw format\n");
		rc = -EPERM;
		goto translate_fail;
	}
	if (ipa_translate_rt_tbl_to_hw_fmt(ip, IPA_RULE_NON_HASHABLE,
		alloc_params->nhash_bdy.base, alloc_params->nhash_hdr.base,
		nhash_bdy_start_ofst, apps_start_idx)) {
		IPAERR("fail to translate non-hashable rt tbls to hw format\n");
		rc = -EPERM;
		goto translate_fail;
	}

	return rc;

translate_fail:
	if (alloc_params->hash_hdr.size)
		ipahal_free_dma_mem(&alloc_params->hash_hdr);
	ipahal_free_dma_mem(&alloc_params->nhash_hdr);
	if (alloc_params->hash_bdy.size)
		ipahal_free_dma_mem(&alloc_params->hash_bdy);
	if (alloc_params->nhash_bdy.size)
		ipahal_free_dma_mem(&alloc_params->nhash_bdy);
allocate_fail:
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
	struct ipahal_fltrt_alloc_imgs_params alloc_params;
	u32 num_modem_rt_index;
	int rc = 0;
	u32 lcl_hash_hdr, lcl_nhash_hdr;
	u32 lcl_hash_bdy, lcl_nhash_bdy;
	bool lcl_hash, lcl_nhash;
	struct ipahal_reg_fltrt_hash_flush flush;
	struct ipahal_reg_valmask valmask;
	int i;
	struct ipa3_rt_tbl_set *set;
	struct ipa3_rt_tbl *tbl;
	u32 tbl_hdr_width;

	tbl_hdr_width = ipahal_get_hw_tbl_hdr_width();
	memset(desc, 0, sizeof(desc));
	memset(cmd_pyld, 0, sizeof(cmd_pyld));
	memset(&alloc_params, 0, sizeof(alloc_params));
	alloc_params.ipt = ip;

	if (ip == IPA_IP_v4) {
		num_modem_rt_index =
			IPA_MEM_PART(v4_modem_rt_index_hi) -
			IPA_MEM_PART(v4_modem_rt_index_lo) + 1;
		lcl_hash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_rt_hash_ofst) +
			num_modem_rt_index * tbl_hdr_width;
		lcl_nhash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_rt_nhash_ofst) +
			num_modem_rt_index * tbl_hdr_width;
		lcl_hash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_rt_hash_ofst);
		lcl_nhash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_rt_nhash_ofst);
		lcl_hash = ipa3_ctx->ip4_rt_tbl_hash_lcl;
		lcl_nhash = ipa3_ctx->ip4_rt_tbl_nhash_lcl;
		alloc_params.tbls_num = IPA_MEM_PART(v4_apps_rt_index_hi) -
			IPA_MEM_PART(v4_apps_rt_index_lo) + 1;
	} else {
		num_modem_rt_index =
			IPA_MEM_PART(v6_modem_rt_index_hi) -
			IPA_MEM_PART(v6_modem_rt_index_lo) + 1;
		lcl_hash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_rt_hash_ofst) +
			num_modem_rt_index * tbl_hdr_width;
		lcl_nhash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_rt_nhash_ofst) +
			num_modem_rt_index * tbl_hdr_width;
		lcl_hash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_rt_hash_ofst);
		lcl_nhash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_rt_nhash_ofst);
		lcl_hash = ipa3_ctx->ip6_rt_tbl_hash_lcl;
		lcl_nhash = ipa3_ctx->ip6_rt_tbl_nhash_lcl;
		alloc_params.tbls_num = IPA_MEM_PART(v6_apps_rt_index_hi) -
			IPA_MEM_PART(v6_apps_rt_index_lo) + 1;
	}

	if (!ipa3_ctx->rt_idx_bitmap[ip]) {
		IPAERR("no rt tbls present\n");
		rc = -EPERM;
		goto no_rt_tbls;
	}

	set = &ipa3_ctx->rt_tbl_set[ip];
	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		if (ipa_prep_rt_tbl_for_cmt(ip, tbl)) {
			rc = -EPERM;
			goto no_rt_tbls;
		}
		if (!tbl->in_sys[IPA_RULE_HASHABLE] &&
			tbl->sz[IPA_RULE_HASHABLE]) {
			alloc_params.num_lcl_hash_tbls++;
			alloc_params.total_sz_lcl_hash_tbls +=
				tbl->sz[IPA_RULE_HASHABLE];
			alloc_params.total_sz_lcl_hash_tbls -= tbl_hdr_width;
		}
		if (!tbl->in_sys[IPA_RULE_NON_HASHABLE] &&
			tbl->sz[IPA_RULE_NON_HASHABLE]) {
			alloc_params.num_lcl_nhash_tbls++;
			alloc_params.total_sz_lcl_nhash_tbls +=
				tbl->sz[IPA_RULE_NON_HASHABLE];
			alloc_params.total_sz_lcl_nhash_tbls -= tbl_hdr_width;
		}
	}

	if (ipa_generate_rt_hw_tbl_img(ip, &alloc_params)) {
		IPAERR("fail to generate RT HW TBL images. IP %d\n", ip);
		rc = -EFAULT;
		goto no_rt_tbls;
	}

	if (!ipa_rt_valid_lcl_tbl_size(ip, IPA_RULE_HASHABLE,
		&alloc_params.hash_bdy)) {
		rc = -EFAULT;
		goto fail_size_valid;
	}
	if (!ipa_rt_valid_lcl_tbl_size(ip, IPA_RULE_NON_HASHABLE,
		&alloc_params.nhash_bdy)) {
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
	mem_cmd.size = alloc_params.nhash_hdr.size;
	mem_cmd.system_addr = alloc_params.nhash_hdr.phys_base;
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
	mem_cmd.size = alloc_params.hash_hdr.size;
	mem_cmd.system_addr = alloc_params.hash_hdr.phys_base;
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
		mem_cmd.size = alloc_params.nhash_bdy.size;
		mem_cmd.system_addr = alloc_params.nhash_bdy.phys_base;
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
		mem_cmd.size = alloc_params.hash_bdy.size;
		mem_cmd.system_addr = alloc_params.hash_bdy.phys_base;
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
	IPA_DUMP_BUFF(alloc_params.hash_hdr.base,
		alloc_params.hash_hdr.phys_base, alloc_params.hash_hdr.size);

	IPADBG_LOW("Non-Hashable HEAD\n");
	IPA_DUMP_BUFF(alloc_params.nhash_hdr.base,
		alloc_params.nhash_hdr.phys_base, alloc_params.nhash_hdr.size);

	if (alloc_params.hash_bdy.size) {
		IPADBG_LOW("Hashable BODY\n");
		IPA_DUMP_BUFF(alloc_params.hash_bdy.base,
			alloc_params.hash_bdy.phys_base,
			alloc_params.hash_bdy.size);
	}

	if (alloc_params.nhash_bdy.size) {
		IPADBG_LOW("Non-Hashable BODY\n");
		IPA_DUMP_BUFF(alloc_params.nhash_bdy.base,
			alloc_params.nhash_bdy.phys_base,
			alloc_params.nhash_bdy.size);
	}

	__ipa_reap_sys_rt_tbls(ip);

fail_imm_cmd_construct:
	for (i = 0 ; i < num_cmd ; i++)
		ipahal_destroy_imm_cmd(cmd_pyld[i]);
fail_size_valid:
	if (alloc_params.hash_hdr.size)
		ipahal_free_dma_mem(&alloc_params.hash_hdr);
	ipahal_free_dma_mem(&alloc_params.nhash_hdr);
	if (alloc_params.hash_bdy.size)
		ipahal_free_dma_mem(&alloc_params.hash_bdy);
	if (alloc_params.nhash_bdy.size)
		ipahal_free_dma_mem(&alloc_params.nhash_bdy);

no_rt_tbls:
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
		IPAERR("Name too long: %s\n", name);
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
		IPAERR("bad parm\n");
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
		IPAERR("no tbl name\n");
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
		IPAERR("bad ip family type\n");
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
		entry->cookie = IPA_COOKIE;
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
			IPAERR("failed to add to tree\n");
			WARN_ON(1);
		}
		entry->id = id;
	}

	return entry;

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

	if (entry == NULL || (entry->cookie != IPA_COOKIE)) {
		IPAERR("bad parms\n");
		return -EINVAL;
	}
	id = entry->id;
	if (ipa3_id_find(id) == NULL) {
		IPAERR("lookup failed\n");
		return -EPERM;
	}

	if (entry->set == &ipa3_ctx->rt_tbl_set[IPA_IP_v4])
		ip = IPA_IP_v4;
	else if (entry->set == &ipa3_ctx->rt_tbl_set[IPA_IP_v6])
		ip = IPA_IP_v6;
	else
		WARN_ON(1);

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

static int __ipa_rt_validate_hndls(const struct ipa_rt_rule *rule,
				struct ipa3_hdr_entry **hdr,
				struct ipa3_hdr_proc_ctx_entry **proc_ctx)
{
	if (rule->hdr_hdl && rule->hdr_proc_ctx_hdl) {
		IPAERR("rule contains both hdr_hdl and hdr_proc_ctx_hdl\n");
		return -EPERM;
	}

	if (rule->hdr_hdl) {
		*hdr = ipa3_id_find(rule->hdr_hdl);
		if ((*hdr == NULL) || ((*hdr)->cookie != IPA_COOKIE)) {
			IPAERR("rt rule does not point to valid hdr\n");
			return -EPERM;
		}
	} else if (rule->hdr_proc_ctx_hdl) {
		*proc_ctx = ipa3_id_find(rule->hdr_proc_ctx_hdl);
		if ((*proc_ctx == NULL) ||
			((*proc_ctx)->cookie != IPA_COOKIE)) {

			IPAERR("rt rule does not point to valid proc ctx\n");
			return -EPERM;
		}
	}

	return 0;
}

static int __ipa_create_rt_entry(struct ipa3_rt_entry **entry,
		const struct ipa_rt_rule *rule,
		struct ipa3_rt_tbl *tbl, struct ipa3_hdr_entry *hdr,
		struct ipa3_hdr_proc_ctx_entry *proc_ctx)
{
	int id;

	*entry = kmem_cache_zalloc(ipa3_ctx->rt_rule_cache, GFP_KERNEL);
	if (!*entry) {
		IPAERR("failed to alloc RT rule object\n");
		goto error;
	}
	INIT_LIST_HEAD(&(*entry)->link);
	(*(entry))->cookie = IPA_COOKIE;
	(*(entry))->rule = *rule;
	(*(entry))->tbl = tbl;
	(*(entry))->hdr = hdr;
	(*(entry))->proc_ctx = proc_ctx;
	id = ipa3_alloc_rule_id(&tbl->rule_ids);
	if (id < 0) {
		IPAERR("failed to allocate rule id\n");
		WARN_ON(1);
		goto alloc_rule_id_fail;
	}
	(*(entry))->rule_id = id;

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
		IPAERR("failed to add to tree\n");
		WARN_ON(1);
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
		const struct ipa_rt_rule *rule, u8 at_rear, u32 *rule_hdl)
{
	struct ipa3_rt_tbl *tbl;
	struct ipa3_rt_entry *entry;
	struct ipa3_hdr_entry *hdr = NULL;
	struct ipa3_hdr_proc_ctx_entry *proc_ctx = NULL;

	if (__ipa_rt_validate_hndls(rule, &hdr, &proc_ctx))
		goto error;


	tbl = __ipa_add_rt_tbl(ip, name);
	if (tbl == NULL || (tbl->cookie != IPA_COOKIE)) {
		IPAERR("failed adding rt tbl name = %s\n",
			name ? name : "");
		goto error;
	}
	/*
	 * do not allow any rules to be added at end of the "default" routing
	 * tables
	 */
	if (!strcmp(tbl->name, IPA_DFLT_RT_TBL_NAME) &&
	    (tbl->rule_cnt > 0) && (at_rear != 0)) {
		IPAERR("cannot add rule at end of tbl rule_cnt=%d at_rear=%d\n",
		       tbl->rule_cnt, at_rear);
		goto error;
	}

	if (__ipa_create_rt_entry(&entry, rule, tbl, hdr, proc_ctx))
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

	if (__ipa_create_rt_entry(&entry, rule, tbl, hdr, proc_ctx))
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
					&rules->rules[i].rt_rule_hdl)) {
			IPAERR("failed to add rt rule %d\n", i);
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
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);

	tbl = __ipa3_find_rt_tbl(rules->ip, rules->rt_tbl_name);
	if (tbl == NULL || (tbl->cookie != IPA_COOKIE)) {
		IPAERR("failed finding rt tbl name = %s\n",
			rules->rt_tbl_name ? rules->rt_tbl_name : "");
		ret = -EINVAL;
		goto bail;
	}

	if (tbl->rule_cnt <= 0) {
		IPAERR("tbl->rule_cnt <= 0");
		ret = -EINVAL;
		goto bail;
	}

	entry = ipa3_id_find(rules->add_after_hdl);
	if (!entry) {
		IPAERR("failed finding rule %d in rt tbls\n",
			rules->add_after_hdl);
		ret = -EINVAL;
		goto bail;
	}

	if (entry->tbl != tbl) {
		IPAERR("given rt rule does not match the table\n");
		ret = -EINVAL;
		goto bail;
	}

	/*
	 * do not allow any rules to be added at end of the "default" routing
	 * tables
	 */
	if (!strcmp(tbl->name, IPA_DFLT_RT_TBL_NAME) &&
			(&entry->link == tbl->head_rt_rule_list.prev)) {
		IPAERR("cannot add rule at end of tbl rule_cnt=%d\n",
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
			IPAERR("failed to add rt rule %d\n", i);
			rules->rules[i].status = IPA_RT_STATUS_OF_ADD_FAILED;
		} else {
			rules->rules[i].status = 0;
		}
	}

	if (rules->commit)
		if (ipa3_ctx->ctrl->ipa3_commit_rt(rules->ip)) {
			IPAERR("failed to commit\n");
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

	entry = ipa3_id_find(rule_hdl);

	if (entry == NULL) {
		IPAERR("lookup failed\n");
		return -EINVAL;
	}

	if (entry->cookie != IPA_COOKIE) {
		IPAERR("bad params\n");
		return -EINVAL;
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
	idr_remove(&entry->tbl->rule_ids, entry->rule_id);
	if (entry->tbl->rule_cnt == 0 && entry->tbl->ref_cnt == 0) {
		if (__ipa_del_rt_tbl(entry->tbl))
			IPAERR("fail to del RT tbl\n");
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
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa3_del_rt_rule(hdls->hdl[i].hdl)) {
			IPAERR("failed to del rt rule %i\n", i);
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
		IPAERR("bad parm\n");
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
 * @ip:	The family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_reset_rt(enum ipa_ip_type ip)
{
	struct ipa3_rt_tbl *tbl;
	struct ipa3_rt_tbl *tbl_next;
	struct ipa3_rt_tbl_set *set;
	struct ipa3_rt_entry *rule;
	struct ipa3_rt_entry *rule_next;
	struct ipa3_rt_tbl_set *rset;
	u32 apps_start_idx;
	int id;

	if (ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
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
	if (ipa3_reset_flt(ip))
		IPAERR("fail to reset flt ip=%d\n", ip);

	set = &ipa3_ctx->rt_tbl_set[ip];
	rset = &ipa3_ctx->reap_rt_tbl_set[ip];
	mutex_lock(&ipa3_ctx->lock);
	IPADBG("reset rt ip=%d\n", ip);
	list_for_each_entry_safe(tbl, tbl_next, &set->head_rt_tbl_list, link) {
		list_for_each_entry_safe(rule, rule_next,
					 &tbl->head_rt_rule_list, link) {
			if (ipa3_id_find(rule->id) == NULL) {
				WARN_ON(1);
				mutex_unlock(&ipa3_ctx->lock);
				return -EFAULT;
			}

			/*
			 * for the "default" routing tbl, remove all but the
			 *  last rule
			 */
			if (tbl->idx == apps_start_idx && tbl->rule_cnt == 1)
				continue;

			list_del(&rule->link);
			tbl->rule_cnt--;
			if (rule->hdr)
				__ipa3_release_hdr(rule->hdr->id);
			else if (rule->proc_ctx)
				__ipa3_release_hdr_proc_ctx(rule->proc_ctx->id);
			rule->cookie = 0;
			idr_remove(&tbl->rule_ids, rule->rule_id);
			id = rule->id;
			kmem_cache_free(ipa3_ctx->rt_rule_cache, rule);

			/* remove the handle from the database */
			ipa3_id_remove(id);
		}

		if (ipa3_id_find(tbl->id) == NULL) {
			WARN_ON(1);
			mutex_unlock(&ipa3_ctx->lock);
			return -EFAULT;
		}
		id = tbl->id;

		/* do not remove the "default" routing tbl which has index 0 */
		if (tbl->idx != apps_start_idx) {
			idr_destroy(&tbl->rule_ids);
			if (tbl->in_sys[IPA_RULE_HASHABLE] ||
				tbl->in_sys[IPA_RULE_NON_HASHABLE]) {
				list_move(&tbl->link, &rset->head_rt_tbl_list);
				clear_bit(tbl->idx,
					  &ipa3_ctx->rt_idx_bitmap[ip]);
				set->tbl_cnt--;
				IPADBG("rst sys rt tbl_idx=%d tbl_cnt=%d\n",
						tbl->idx, set->tbl_cnt);
			} else {
				list_del(&tbl->link);
				set->tbl_cnt--;
				clear_bit(tbl->idx,
					  &ipa3_ctx->rt_idx_bitmap[ip]);
				IPADBG("rst rt tbl_idx=%d tbl_cnt=%d\n",
						tbl->idx, set->tbl_cnt);
				kmem_cache_free(ipa3_ctx->rt_tbl_cache, tbl);
			}
			/* remove the handle from the database */
			ipa3_id_remove(id);
		}
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
		IPAERR("bad parm\n");
		return -EINVAL;
	}
	mutex_lock(&ipa3_ctx->lock);
	entry = __ipa3_find_rt_tbl(lookup->ip, lookup->name);
	if (entry && entry->cookie == IPA_COOKIE) {
		if (entry->ref_cnt == U32_MAX) {
			IPAERR("fail: ref count crossed limit\n");
			goto ret;
		}
		entry->ref_cnt++;
		lookup->hdl = entry->id;

		/* commit for get */
		if (ipa3_ctx->ctrl->ipa3_commit_rt(lookup->ip))
			IPAERR("fail to commit RT tbl\n");

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
	int result;

	mutex_lock(&ipa3_ctx->lock);
	entry = ipa3_id_find(rt_tbl_hdl);
	if (entry == NULL) {
		IPAERR("lookup failed\n");
		result = -EINVAL;
		goto ret;
	}

	if ((entry->cookie != IPA_COOKIE) || entry->ref_cnt == 0) {
		IPAERR("bad parms\n");
		result = -EINVAL;
		goto ret;
	}

	if (entry->set == &ipa3_ctx->rt_tbl_set[IPA_IP_v4])
		ip = IPA_IP_v4;
	else if (entry->set == &ipa3_ctx->rt_tbl_set[IPA_IP_v6])
		ip = IPA_IP_v6;
	else
		WARN_ON(1);

	entry->ref_cnt--;
	if (entry->ref_cnt == 0 && entry->rule_cnt == 0) {
		IPADBG("zero ref_cnt, delete rt tbl (idx=%u)\n",
			entry->idx);
		if (__ipa_del_rt_tbl(entry))
			IPAERR("fail to del RT tbl\n");
		/* commit for put */
		if (ipa3_ctx->ctrl->ipa3_commit_rt(ip))
			IPAERR("fail to commit RT tbl\n");
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

	if (rtrule->rule.hdr_hdl) {
		hdr = ipa3_id_find(rtrule->rule.hdr_hdl);
		if ((hdr == NULL) || (hdr->cookie != IPA_COOKIE)) {
			IPAERR("rt rule does not point to valid hdr\n");
			goto error;
		}
	} else if (rtrule->rule.hdr_proc_ctx_hdl) {
		proc_ctx = ipa3_id_find(rtrule->rule.hdr_proc_ctx_hdl);
		if ((proc_ctx == NULL) || (proc_ctx->cookie != IPA_COOKIE)) {
			IPAERR("rt rule does not point to valid proc ctx\n");
			goto error;
		}
	}

	entry = ipa3_id_find(rtrule->rt_rule_hdl);
	if (entry == NULL) {
		IPAERR("lookup failed\n");
		goto error;
	}

	if (entry->cookie != IPA_COOKIE) {
		IPAERR("bad params\n");
		goto error;
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
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < hdls->num_rules; i++) {
		if (__ipa_mdfy_rt_rule(&hdls->rules[i])) {
			IPAERR("failed to mdfy rt rule %i\n", i);
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
 * This function reads the routing table from IPA SRAM and prepares an array
 * of entries. This function is mainly used for debugging purposes.
 *
 * If empty table or Modem Apps table, zero entries will be returned.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_rt_read_tbl_from_hw(u32 tbl_idx, enum ipa_ip_type ip_type,
	bool hashable, struct ipahal_rt_rule_entry entry[], int *num_entry)
{
	void *ipa_sram_mmio;
	u64 hdr_base_ofst;
	int res = 0;
	u64 tbl_addr;
	bool is_sys;
	struct ipa_mem_buffer *sys_tbl_mem;
	u8 *rule_addr;
	int rule_idx;

	IPADBG_LOW("tbl_idx=%d ip_t=%d hashable=%d entry=0x%p num_entry=0x%p\n",
		tbl_idx, ip_type, hashable, entry, num_entry);

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
		ipahal_get_reg_n_ofst(IPA_SRAM_DIRECT_ACCESS_n,
			ipa3_ctx->smem_restricted_bytes / 4),
		ipa3_ctx->smem_sz);
	if (!ipa_sram_mmio) {
		IPAERR("fail to ioremap IPA SRAM\n");
		return -ENOMEM;
	}

	memset(entry, 0, sizeof(*entry) * (*num_entry));
	if (hashable) {
		if (ip_type == IPA_IP_v4)
			hdr_base_ofst =
				IPA_MEM_PART(v4_rt_hash_ofst);
		else
			hdr_base_ofst =
				IPA_MEM_PART(v6_rt_hash_ofst);
	} else {
		if (ip_type == IPA_IP_v4)
			hdr_base_ofst =
				IPA_MEM_PART(v4_rt_nhash_ofst);
		else
			hdr_base_ofst =
				IPA_MEM_PART(v6_rt_nhash_ofst);
	}

	IPADBG_LOW("hdr_base_ofst=0x%llx\n", hdr_base_ofst);

	res = ipahal_fltrt_read_addr_from_hdr(ipa_sram_mmio + hdr_base_ofst,
		tbl_idx, &tbl_addr, &is_sys);
	if (res) {
		IPAERR("failed to read table address from header structure\n");
		goto bail;
	}
	IPADBG_LOW("rt tbl %d: tbl_addr=0x%llx is_sys=%d\n",
		tbl_idx, tbl_addr, is_sys);
	if (!tbl_addr) {
		IPAERR("invalid rt tbl addr\n");
		res = -EFAULT;
		goto bail;
	}

	/* for tables which reside in DDR access it from the virtual memory */
	if (is_sys) {
		struct ipa3_rt_tbl_set *set;
		struct ipa3_rt_tbl *tbl;

		set = &ipa3_ctx->rt_tbl_set[ip_type];
		rule_addr = NULL;
		list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
			if (tbl->idx == tbl_idx) {
				sys_tbl_mem = &(tbl->curr_mem[hashable ?
					IPA_RULE_HASHABLE :
					IPA_RULE_NON_HASHABLE]);
				if (sys_tbl_mem->phys_base &&
					sys_tbl_mem->phys_base != tbl_addr) {
					IPAERR("mismatch:parsed=%llx sw=%pad\n"
						, tbl_addr,
						&sys_tbl_mem->phys_base);
				}
				if (sys_tbl_mem->phys_base)
					rule_addr = sys_tbl_mem->base;
				else
					rule_addr = NULL;
			}
		}
	} else {
		rule_addr = ipa_sram_mmio + hdr_base_ofst + tbl_addr;
	}

	IPADBG_LOW("First rule addr 0x%p\n", rule_addr);

	if (!rule_addr) {
		/* Modem table in system memory or empty table */
		*num_entry = 0;
		goto bail;
	}

	rule_idx = 0;
	while (rule_idx < *num_entry) {
		res = ipahal_rt_parse_hw_rule(rule_addr, &entry[rule_idx]);
		if (res) {
			IPAERR("failed parsing rt rule\n");
			goto bail;
		}

		IPADBG_LOW("rule_size=%d\n", entry[rule_idx].rule_size);
		if (!entry[rule_idx].rule_size)
			break;

		rule_addr += entry[rule_idx].rule_size;
		rule_idx++;
	}
	*num_entry = rule_idx;
bail:
	iounmap(ipa_sram_mmio);
	return res;
}

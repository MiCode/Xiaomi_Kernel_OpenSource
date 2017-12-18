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

#include "ipa_i.h"
#include "ipahal/ipahal.h"
#include "ipahal/ipahal_fltrt.h"

#define IPA_FLT_STATUS_OF_ADD_FAILED		(-1)
#define IPA_FLT_STATUS_OF_DEL_FAILED		(-1)
#define IPA_FLT_STATUS_OF_MDFY_FAILED		(-1)

#define IPA_FLT_GET_RULE_TYPE(__entry) \
	( \
	((__entry)->rule.hashable) ? \
	(IPA_RULE_HASHABLE):(IPA_RULE_NON_HASHABLE) \
	)

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
	struct ipahal_flt_rule_gen_params gen_params;
	int res = 0;

	memset(&gen_params, 0, sizeof(gen_params));

	gen_params.ipt = ip;
	if (entry->rt_tbl)
		gen_params.rt_tbl_idx = entry->rt_tbl->idx;
	else
		gen_params.rt_tbl_idx = entry->rule.rt_tbl_idx;

	gen_params.priority = entry->prio;
	gen_params.id = entry->rule_id;
	gen_params.rule = (const struct ipa_flt_rule *)&entry->rule;

	res = ipahal_flt_generate_hw_rule(&gen_params, &entry->hw_len, buf);
	if (res)
		IPAERR_RL("failed to generate flt h/w rule\n");

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
			ipahal_free_dma_mem(&tbl->prev_mem[rlt]);
		}

		if (list_empty(&tbl->head_flt_rule_list)) {
			if (tbl->curr_mem[rlt].phys_base) {
				IPADBG_LOW("reaping flt tbl (curr) pipe=%d\n",
					i);
				ipahal_free_dma_mem(&tbl->curr_mem[rlt]);
			}
		}
	}
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
	int prio_i;
	int max_prio;
	u32 hdr_width;

	tbl->sz[IPA_RULE_HASHABLE] = 0;
	tbl->sz[IPA_RULE_NON_HASHABLE] = 0;

	max_prio = ipahal_get_rule_max_priority();

	prio_i = max_prio;
	list_for_each_entry(entry, &tbl->head_flt_rule_list, link) {

		if (entry->rule.max_prio) {
			entry->prio = max_prio;
		} else {
			if (ipahal_rule_decrease_priority(&prio_i)) {
				IPAERR("cannot decrease rule priority - %d\n",
					prio_i);
				return -EPERM;
			}
			entry->prio = prio_i;
		}

		if (ipa3_generate_flt_hw_rule(ip, entry, NULL)) {
			IPAERR("failed to calculate HW FLT rule size\n");
			return -EPERM;
		}
		IPADBG_LOW("pipe %d rule_id(handle) %u hw_len %d priority %u\n",
			pipe_idx, entry->rule_id, entry->hw_len, entry->prio);

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

	hdr_width = ipahal_get_hw_tbl_hdr_width();

	/* for the header word */
	if (tbl->sz[IPA_RULE_HASHABLE])
		tbl->sz[IPA_RULE_HASHABLE] += hdr_width;
	if (tbl->sz[IPA_RULE_NON_HASHABLE])
		tbl->sz[IPA_RULE_NON_HASHABLE] += hdr_width;

	IPADBG_LOW("FLT tbl pipe idx %d hash sz %u non-hash sz %u\n", pipe_idx,
		tbl->sz[IPA_RULE_HASHABLE], tbl->sz[IPA_RULE_NON_HASHABLE]);

	return 0;
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
			/* only body (no header) */
			tbl_mem.size = tbl->sz[rlt] -
				ipahal_get_hw_tbl_hdr_width();
			if (ipahal_fltrt_allocate_hw_sys_tbl(&tbl_mem)) {
				IPAERR("fail to alloc sys tbl of size %d\n",
					tbl_mem.size);
				goto err;
			}

			if (ipahal_fltrt_write_addr_to_hdr(tbl_mem.phys_base,
				hdr, hdr_idx, true)) {
				IPAERR("fail to wrt sys tbl addr to hdr\n");
				goto hdr_update_fail;
			}

			tbl_mem_buf = tbl_mem.base;

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_flt_rule_list,
				link) {
				if (IPA_FLT_GET_RULE_TYPE(entry) != rlt)
					continue;
				res = ipa3_generate_flt_hw_rule(
					ip, entry, tbl_mem_buf);
				if (res) {
					IPAERR("failed to gen HW FLT rule\n");
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
				hdr_idx, true)) {
				IPAERR("fail to wrt lcl tbl ofst to hdr\n");
				goto hdr_update_fail;
			}

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

			/**
			 * advance body_i to next table alignment as local
			 * tables are order back-to-back
			 */
			body_i += ipahal_get_lcl_tbl_addr_alignment();
			body_i = (u8 *)((long)body_i &
				~ipahal_get_lcl_tbl_addr_alignment());
		}
		hdr_idx++;
	}

	return 0;

hdr_update_fail:
	ipahal_free_dma_mem(&tbl_mem);
err:
	return -EPERM;
}

/**
 * ipa_generate_flt_hw_tbl_img() - generates the flt hw tbls.
 *  headers and bodies are being created into buffers that will be filled into
 *  the local memory (sram)
 * @ip: the ip address family type
 * @alloc_params: In and Out parameters for the allocations of the buffers
 *  4 buffers: hdr and bdy, each hashable and non-hashable
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_generate_flt_hw_tbl_img(enum ipa_ip_type ip,
	struct ipahal_fltrt_alloc_imgs_params *alloc_params)
{
	u32 hash_bdy_start_ofst, nhash_bdy_start_ofst;
	int rc = 0;

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

	if (ipahal_fltrt_allocate_hw_tbl_imgs(alloc_params)) {
		IPAERR_RL("fail to allocate FLT HW TBL images. IP %d\n", ip);
		rc = -ENOMEM;
		goto allocate_failed;
	}

	if (ipa_translate_flt_tbl_to_hw_fmt(ip, IPA_RULE_HASHABLE,
		alloc_params->hash_bdy.base, alloc_params->hash_hdr.base,
		hash_bdy_start_ofst)) {
		IPAERR_RL("fail to translate hashable flt tbls to hw format\n");
		rc = -EPERM;
		goto translate_fail;
	}
	if (ipa_translate_flt_tbl_to_hw_fmt(ip, IPA_RULE_NON_HASHABLE,
		alloc_params->nhash_bdy.base, alloc_params->nhash_hdr.base,
		nhash_bdy_start_ofst)) {
		IPAERR_RL("fail to translate non-hash flt tbls to hw format\n");
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
allocate_failed:
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
 * @entries: the number of entries
 * @desc: [OUT] descriptor buffer
 * @cmd: [OUT] imm commands payload pointers buffer
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_flt_alloc_cmd_buffers(enum ipa_ip_type ip, u16 entries,
	struct ipa3_desc **desc, struct ipahal_imm_cmd_pyld ***cmd_pyld)
{
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

	if ((ipa3_get_ep_mapping(IPA_CLIENT_APPS_WAN_PROD) == pipe
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
	struct ipahal_fltrt_alloc_imgs_params alloc_params;
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
	u32 tbl_hdr_width;
	struct ipa3_flt_tbl *tbl;
	u16 entries;

	tbl_hdr_width = ipahal_get_hw_tbl_hdr_width();
	memset(&alloc_params, 0, sizeof(alloc_params));
	alloc_params.ipt = ip;
	alloc_params.tbls_num = ipa3_ctx->ep_flt_num;

	if (ip == IPA_IP_v4) {
		lcl_hash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_flt_hash_ofst) +
			tbl_hdr_width; /* to skip the bitmap */
		lcl_nhash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_flt_nhash_ofst) +
			tbl_hdr_width; /* to skip the bitmap */
		lcl_hash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_flt_hash_ofst);
		lcl_nhash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_flt_nhash_ofst);
		lcl_hash = ipa3_ctx->ip4_flt_tbl_hash_lcl;
		lcl_nhash = ipa3_ctx->ip4_flt_tbl_nhash_lcl;
	} else {
		lcl_hash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_flt_hash_ofst) +
			tbl_hdr_width; /* to skip the bitmap */
		lcl_nhash_hdr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_flt_nhash_ofst) +
			tbl_hdr_width; /* to skip the bitmap */
		lcl_hash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_flt_hash_ofst);
		lcl_nhash_bdy = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_flt_nhash_ofst);
		lcl_hash = ipa3_ctx->ip6_flt_tbl_hash_lcl;
		lcl_nhash = ipa3_ctx->ip6_flt_tbl_nhash_lcl;
	}

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;
		tbl = &ipa3_ctx->flt_tbl[i][ip];
		if (ipa_prep_flt_tbl_for_cmt(ip, tbl, i)) {
			rc = -EPERM;
			goto prep_failed;
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

	if (ipa_generate_flt_hw_tbl_img(ip, &alloc_params)) {
		IPAERR_RL("fail to generate FLT HW TBL image. IP %d\n", ip);
		rc = -EFAULT;
		goto prep_failed;
	}

	if (!ipa_flt_valid_lcl_tbl_size(ip, IPA_RULE_HASHABLE,
		&alloc_params.hash_bdy)) {
		rc = -EFAULT;
		goto fail_size_valid;
	}
	if (!ipa_flt_valid_lcl_tbl_size(ip, IPA_RULE_NON_HASHABLE,
		&alloc_params.nhash_bdy)) {
		rc = -EFAULT;
		goto fail_size_valid;
	}

	/* +3: 2 for bodies (hashable and non-hashable) and 1 for flushing */
	entries = (ipa3_ctx->ep_flt_num) * 2 + 3;

	if (ipa_flt_alloc_cmd_buffers(ip, entries, &desc, &cmd_pyld)) {
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
	ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
	++num_cmd;

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

		if (num_cmd + 1 >= entries) {
			IPAERR("number of commands is out of range: IP = %d\n",
				ip);
			rc = -ENOBUFS;
			goto fail_imm_cmd_construct;
		}

		IPADBG_LOW("Prepare imm cmd for hdr at index %d for pipe %d\n",
			hdr_idx, i);

		mem_cmd.is_read = false;
		mem_cmd.skip_pipeline_clear = false;
		mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		mem_cmd.size = tbl_hdr_width;
		mem_cmd.system_addr = alloc_params.nhash_hdr.phys_base +
			hdr_idx * tbl_hdr_width;
		mem_cmd.local_addr = lcl_nhash_hdr +
			hdr_idx * tbl_hdr_width;
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR("fail construct dma_shared_mem cmd: IP = %d\n",
				ip);
			rc = -ENOMEM;
			goto fail_imm_cmd_construct;
		}
		ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
		++num_cmd;

		mem_cmd.is_read = false;
		mem_cmd.skip_pipeline_clear = false;
		mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		mem_cmd.size = tbl_hdr_width;
		mem_cmd.system_addr = alloc_params.hash_hdr.phys_base +
			hdr_idx * tbl_hdr_width;
		mem_cmd.local_addr = lcl_hash_hdr +
			hdr_idx * tbl_hdr_width;
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR("fail construct dma_shared_mem cmd: IP = %d\n",
				ip);
			rc = -ENOMEM;
			goto fail_imm_cmd_construct;
		}
		ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
		++num_cmd;

		++hdr_idx;
	}

	if (lcl_nhash) {
		if (num_cmd >= entries) {
			IPAERR("number of commands is out of range: IP = %d\n",
				ip);
			rc = -ENOBUFS;
			goto fail_imm_cmd_construct;
		}

		mem_cmd.is_read = false;
		mem_cmd.skip_pipeline_clear = false;
		mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		mem_cmd.size = alloc_params.nhash_bdy.size;
		mem_cmd.system_addr = alloc_params.nhash_bdy.phys_base;
		mem_cmd.local_addr = lcl_nhash_bdy;
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR("fail construct dma_shared_mem cmd: IP = %d\n",
				ip);
			rc = -ENOMEM;
			goto fail_imm_cmd_construct;
		}
		ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
		++num_cmd;
	}
	if (lcl_hash) {
		if (num_cmd >= entries) {
			IPAERR("number of commands is out of range: IP = %d\n",
				ip);
			rc = -ENOBUFS;
			goto fail_imm_cmd_construct;
		}

		mem_cmd.is_read = false;
		mem_cmd.skip_pipeline_clear = false;
		mem_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		mem_cmd.size = alloc_params.hash_bdy.size;
		mem_cmd.system_addr = alloc_params.hash_bdy.phys_base;
		mem_cmd.local_addr = lcl_hash_bdy;
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR("fail construct dma_shared_mem cmd: IP = %d\n",
				ip);
			rc = -ENOMEM;
			goto fail_imm_cmd_construct;
		}
		ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
		++num_cmd;
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

	__ipa_reap_sys_flt_tbls(ip, IPA_RULE_HASHABLE);
	__ipa_reap_sys_flt_tbls(ip, IPA_RULE_NON_HASHABLE);

fail_imm_cmd_construct:
	for (i = 0 ; i < num_cmd ; i++)
		ipahal_destroy_imm_cmd(cmd_pyld[i]);
fail_reg_write_construct:
	kfree(desc);
	kfree(cmd_pyld);
fail_size_valid:
	if (alloc_params.hash_hdr.size)
		ipahal_free_dma_mem(&alloc_params.hash_hdr);
	ipahal_free_dma_mem(&alloc_params.nhash_hdr);
	if (alloc_params.hash_bdy.size)
		ipahal_free_dma_mem(&alloc_params.hash_bdy);
	if (alloc_params.nhash_bdy.size)
		ipahal_free_dma_mem(&alloc_params.nhash_bdy);
prep_failed:
	return rc;
}

static int __ipa_validate_flt_rule(const struct ipa_flt_rule *rule,
		struct ipa3_rt_tbl **rt_tbl, enum ipa_ip_type ip)
{
	if (rule->action != IPA_PASS_TO_EXCEPTION) {
		if (!rule->eq_attrib_type) {
			if (!rule->rt_tbl_hdl) {
				IPAERR_RL("invalid RT tbl\n");
				goto error;
			}

			*rt_tbl = ipa3_id_find(rule->rt_tbl_hdl);
			if (*rt_tbl == NULL) {
				IPAERR_RL("RT tbl not found\n");
				goto error;
			}

			if ((*rt_tbl)->cookie != IPA_RT_TBL_COOKIE) {
				IPAERR_RL("RT table cookie is invalid\n");
				goto error;
			}
		} else {
			if (rule->rt_tbl_idx > ((ip == IPA_IP_v4) ?
				IPA_MEM_PART(v4_modem_rt_index_hi) :
				IPA_MEM_PART(v6_modem_rt_index_hi))) {
				IPAERR_RL("invalid RT tbl\n");
				goto error;
			}
		}
	} else {
		if (rule->rt_tbl_idx > 0) {
			IPAERR("invalid RT tbl\n");
			goto error;
		}
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		if (rule->pdn_idx) {
			if (rule->action == IPA_PASS_TO_EXCEPTION ||
				rule->action == IPA_PASS_TO_ROUTING) {
				IPAERR_RL(
					"PDN index should be 0 when action is not pass to NAT\n");
				goto error;
			} else {
				if (rule->pdn_idx >= IPA_MAX_PDN_NUM) {
					IPAERR_RL("PDN index %d is too large\n",
						rule->pdn_idx);
					goto error;
				}
			}
		}
	}

	if (rule->rule_id) {
		if ((rule->rule_id < ipahal_get_rule_id_hi_bit()) ||
		(rule->rule_id >= ((ipahal_get_rule_id_hi_bit()<<1)-1))) {
			IPAERR_RL("invalid rule_id provided 0x%x\n"
				"rule_id with bit 0x%x are auto generated\n",
				rule->rule_id, ipahal_get_rule_id_hi_bit());
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
	(*entry)->cookie = IPA_FLT_COOKIE;
	(*entry)->rt_tbl = rt_tbl;
	(*entry)->tbl = tbl;
	if (rule->rule_id) {
		id = rule->rule_id;
	} else {
		id = ipa3_alloc_rule_id(tbl->rule_ids);
		if (id < 0) {
			IPAERR_RL("failed to allocate rule id\n");
			WARN_ON_RATELIMIT_IPA(1);
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
		IPAERR_RL("failed to add to tree\n");
		WARN_ON_RATELIMIT_IPA(1);
		goto ipa_insert_failed;
	}
	*rule_hdl = id;
	entry->id = id;
	IPADBG_LOW("add flt rule rule_cnt=%d\n", tbl->rule_cnt);

	return 0;
ipa_insert_failed:
	if (entry->rt_tbl)
		entry->rt_tbl->ref_cnt--;
	tbl->rule_cnt--;
	return -EPERM;
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

	if (__ipa_finish_flt_rule_add(tbl, entry, rule_hdl))
		goto ipa_insert_failed;

	return 0;
ipa_insert_failed:
	list_del(&entry->link);
	/* if rule id was allocated from idr, remove it */
	if ((entry->rule_id < ipahal_get_rule_id_hi_bit()) &&
		(entry->rule_id >= ipahal_get_low_rule_id()))
		idr_remove(entry->tbl->rule_ids, entry->rule_id);
	kmem_cache_free(ipa3_ctx->flt_rule_cache, entry);

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
		IPAERR_RL("bad parms rule=%p rule_hdl=%p\n", rule,
				rule_hdl);
		goto error;
	}

	if (__ipa_validate_flt_rule(rule, &rt_tbl, ip))
		goto error;

	if (__ipa_create_flt_entry(&entry, rule, rt_tbl, tbl))
		goto error;

	list_add(&entry->link, &((*add_after_entry)->link));

	if (__ipa_finish_flt_rule_add(tbl, entry, rule_hdl))
		goto ipa_insert_failed;

	/*
	 * prepare for next insertion
	 */
	*add_after_entry = entry;

	return 0;

ipa_insert_failed:
	list_del(&entry->link);
	/* if rule id was allocated from idr, remove it */
	if ((entry->rule_id < ipahal_get_rule_id_hi_bit()) &&
		(entry->rule_id >= ipahal_get_low_rule_id()))
		idr_remove(entry->tbl->rule_ids, entry->rule_id);
	kmem_cache_free(ipa3_ctx->flt_rule_cache, entry);

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
		IPAERR_RL("lookup failed\n");
		return -EINVAL;
	}

	if (entry->cookie != IPA_FLT_COOKIE) {
		IPAERR_RL("bad params\n");
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
	if ((entry->rule_id < ipahal_get_rule_id_hi_bit()) &&
		(entry->rule_id >= ipahal_get_low_rule_id()))
		idr_remove(entry->tbl->rule_ids, entry->rule_id);

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
		IPAERR_RL("lookup failed\n");
		goto error;
	}

	if (entry->cookie != IPA_FLT_COOKIE) {
		IPAERR_RL("bad params\n");
		goto error;
	}

	if (entry->rt_tbl)
		entry->rt_tbl->ref_cnt--;

	if (frule->rule.action != IPA_PASS_TO_EXCEPTION) {
		if (!frule->rule.eq_attrib_type) {
			if (!frule->rule.rt_tbl_hdl) {
				IPAERR_RL("invalid RT tbl\n");
				goto error;
			}

			rt_tbl = ipa3_id_find(frule->rule.rt_tbl_hdl);
			if (rt_tbl == NULL) {
				IPAERR_RL("RT tbl not found\n");
				goto error;
			}

			if (rt_tbl->cookie != IPA_RT_TBL_COOKIE) {
				IPAERR_RL("RT table cookie is invalid\n");
				goto error;
			}
		} else {
			if (frule->rule.rt_tbl_idx > ((ip == IPA_IP_v4) ?
				IPA_MEM_PART(v4_modem_rt_index_hi) :
				IPA_MEM_PART(v6_modem_rt_index_hi))) {
				IPAERR_RL("invalid RT tbl\n");
				goto error;
			}
		}
	} else {
		if (frule->rule.rt_tbl_idx > 0) {
			IPAERR_RL("invalid RT tbl\n");
			goto error;
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
	if (*ipa_ep_idx < 0) {
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
		IPAERR_RL("bad parms rule=%p rule_hdl=%p ep=%d\n", rule,
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
		IPAERR_RL("bad parm\n");
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
			IPAERR_RL("failed to add flt rule %d\n", i);
			rules->rules[i].status = IPA_FLT_STATUS_OF_ADD_FAILED;
		} else {
			rules->rules[i].status = 0;
		}
	}

	if (rules->global) {
		IPAERR_RL("no support for global filter rules\n");
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
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	if (rules->ep >= IPA_CLIENT_MAX) {
		IPAERR_RL("bad parms ep=%d\n", rules->ep);
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
		IPAERR_RL("lookup failed\n");
		result = -EINVAL;
		goto bail;
	}

	if (entry->cookie != IPA_FLT_COOKIE) {
		IPAERR_RL("Invalid cookie value =  %u flt hdl id = %d\n",
			entry->cookie, rules->add_after_hdl);
		result = -EINVAL;
		goto bail;
	}

	if (entry->tbl != tbl) {
		IPAERR_RL("given entry does not match the table\n");
		result = -EINVAL;
		goto bail;
	}

	if (tbl->sticky_rear)
		if (&entry->link == tbl->head_flt_rule_list.prev) {
			IPAERR_RL("cannot add rule at end of a sticky table");
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
			IPAERR_RL("failed to add flt rule %d\n", i);
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
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa_del_flt_rule(hdls->hdl[i].hdl)) {
			IPAERR_RL("failed to del flt rule %i\n", i);
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
 * ipa3_mdfy_flt_rule() - Modify the specified filtering rules in SW and
 * optionally commit to IPA HW
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
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < hdls->num_rules; i++) {
		if (__ipa_mdfy_flt_rule(&hdls->rules[i], hdls->ip)) {
			IPAERR_RL("failed to mdfy flt rule %i\n", i);
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
 * ipa3_commit_flt() - Commit the current SW filtering table of specified type
 * to IPA HW
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
		IPAERR_RL("bad parm\n");
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
		IPAERR_RL("bad parm\n");
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
				WARN_ON_RATELIMIT_IPA(1);
				mutex_unlock(&ipa3_ctx->lock);
				return -EFAULT;
			}
			list_del(&entry->link);
			entry->tbl->rule_cnt--;
			if (entry->rt_tbl)
				entry->rt_tbl->ref_cnt--;
			/* if rule id was allocated from idr, remove it */
			if ((entry->rule_id < ipahal_get_rule_id_hi_bit()) &&
				(entry->rule_id >= ipahal_get_low_rule_id()))
				idr_remove(entry->tbl->rule_ids,
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
	rule.action = IPA_PASS_TO_EXCEPTION;
	__ipa_add_flt_rule(tbl, IPA_IP_v4, &rule, true,
			&ep->dflt_flt4_rule_hdl);
	ipa3_ctx->ctrl->ipa3_commit_flt(IPA_IP_v4);
	tbl->sticky_rear = true;

	tbl = &ipa3_ctx->flt_tbl[ipa_ep_idx][IPA_IP_v6];
	rule.action = IPA_PASS_TO_EXCEPTION;
	__ipa_add_flt_rule(tbl, IPA_IP_v6, &rule, true,
			&ep->dflt_flt6_rule_hdl);
	ipa3_ctx->ctrl->ipa3_commit_flt(IPA_IP_v6);
	tbl->sticky_rear = true;
	mutex_unlock(&ipa3_ctx->lock);
}

void ipa3_delete_dflt_flt_rules(u32 ipa_ep_idx)
{
	struct ipa3_ep_context *ep = &ipa3_ctx->ep[ipa_ep_idx];
	struct ipa3_flt_tbl *tbl;

	mutex_lock(&ipa3_ctx->lock);
	if (ep->dflt_flt4_rule_hdl) {
		tbl = &ipa3_ctx->flt_tbl[ipa_ep_idx][IPA_IP_v4];
		__ipa_del_flt_rule(ep->dflt_flt4_rule_hdl);
		ipa3_ctx->ctrl->ipa3_commit_flt(IPA_IP_v4);
		/* Reset the sticky flag. */
		tbl->sticky_rear = false;
		ep->dflt_flt4_rule_hdl = 0;
	}
	if (ep->dflt_flt6_rule_hdl) {
		tbl = &ipa3_ctx->flt_tbl[ipa_ep_idx][IPA_IP_v6];
		__ipa_del_flt_rule(ep->dflt_flt6_rule_hdl);
		ipa3_ctx->ctrl->ipa3_commit_flt(IPA_IP_v6);
		/* Reset the sticky flag. */
		tbl->sticky_rear = false;
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
 *
 * If empty table or Modem Apps table, zero entries will be returned.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_flt_read_tbl_from_hw(u32 pipe_idx, enum ipa_ip_type ip_type,
	bool hashable, struct ipahal_flt_rule_entry entry[], int *num_entry)
{
	void *ipa_sram_mmio;
	u64 hdr_base_ofst;
	int tbl_entry_idx;
	int i;
	int res = 0;
	u64 tbl_addr;
	bool is_sys;
	u8 *rule_addr;
	struct ipa_mem_buffer *sys_tbl_mem;
	int rule_idx;

	IPADBG("pipe_idx=%d ip=%d hashable=%d entry=0x%p num_entry=0x%p\n",
		pipe_idx, ip_type, hashable, entry, num_entry);

	if (pipe_idx >= ipa3_ctx->ipa_num_pipes || ip_type >= IPA_IP_MAX ||
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
				IPA_MEM_PART(v4_flt_hash_ofst);
		else
			hdr_base_ofst =
				IPA_MEM_PART(v6_flt_hash_ofst);
	} else {
		if (ip_type == IPA_IP_v4)
			hdr_base_ofst =
				IPA_MEM_PART(v4_flt_nhash_ofst);
		else
			hdr_base_ofst =
				IPA_MEM_PART(v6_flt_nhash_ofst);
	}

	/* calculate the index of the tbl entry */
	tbl_entry_idx = 1; /* skip the bitmap */
	for (i = 0; i < pipe_idx; i++)
		if (ipa3_ctx->ep_flt_bitmap & (1 << i))
			tbl_entry_idx++;

	IPADBG("hdr_base_ofst=0x%llx tbl_entry_idx=%d\n",
		hdr_base_ofst, tbl_entry_idx);

	res = ipahal_fltrt_read_addr_from_hdr(ipa_sram_mmio + hdr_base_ofst,
		tbl_entry_idx, &tbl_addr, &is_sys);
	if (res) {
		IPAERR("failed to read table address from header structure\n");
		goto bail;
	}
	IPADBG("flt tbl ep=%d: tbl_addr=0x%llx is_sys=%d\n",
		pipe_idx, tbl_addr, is_sys);
	if (!tbl_addr) {
		IPAERR("invalid flt tbl addr\n");
		res = -EFAULT;
		goto bail;
	}

	/* for tables resides in DDR access it from the virtual memory */
	if (is_sys) {
		sys_tbl_mem = &ipa3_ctx->flt_tbl[pipe_idx][ip_type].
			curr_mem[hashable ? IPA_RULE_HASHABLE :
				IPA_RULE_NON_HASHABLE];
		if (sys_tbl_mem->phys_base &&
			sys_tbl_mem->phys_base != tbl_addr) {
			IPAERR("mismatch addr: parsed=%llx sw=%pad\n",
				tbl_addr, &sys_tbl_mem->phys_base);
		}
		if (sys_tbl_mem->phys_base)
			rule_addr = sys_tbl_mem->base;
		else
			rule_addr = NULL;
	} else {
		rule_addr = ipa_sram_mmio + hdr_base_ofst + tbl_addr;
	}

	IPADBG("First rule addr 0x%p\n", rule_addr);

	if (!rule_addr) {
		/* Modem table in system memory or empty table */
		*num_entry = 0;
		goto bail;
	}

	rule_idx = 0;
	while (rule_idx < *num_entry) {
		res = ipahal_flt_parse_hw_rule(rule_addr, &entry[rule_idx]);
		if (res) {
			IPAERR("failed parsing flt rule\n");
			goto bail;
		}

		IPADBG("rule_size=%d\n", entry[rule_idx].rule_size);
		if (!entry[rule_idx].rule_size)
			break;

		rule_addr += entry[rule_idx].rule_size;
		rule_idx++;
	}
	*num_entry = rule_idx;
bail:
	iounmap(ipa_sram_mmio);
	return 0;
}

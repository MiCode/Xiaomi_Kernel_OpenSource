/* Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
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
#include "ipa_i.h"

#define IPA_RT_TABLE_INDEX_NOT_FOUND	(-1)
#define IPA_RT_TABLE_WORD_SIZE		(4)
#define IPA_RT_INDEX_BITMAP_SIZE	(32)
#define IPA_RT_TABLE_MEMORY_ALLIGNMENT	(127)
#define IPA_RT_ENTRY_MEMORY_ALLIGNMENT	(3)
#define IPA_RT_BIT_MASK			(0x1)
#define IPA_RT_STATUS_OF_ADD_FAILED	(-1)
#define IPA_RT_STATUS_OF_DEL_FAILED	(-1)
#define IPA_RT_STATUS_OF_MDFY_FAILED (-1)

/**
 * __ipa_generate_rt_hw_rule_v2() - generates the routing hardware rule
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
int __ipa_generate_rt_hw_rule_v2(enum ipa_ip_type ip,
		struct ipa_rt_entry *entry, u8 *buf)
{
	struct ipa_rt_rule_hw_hdr *rule_hdr;
	const struct ipa_rt_rule *rule =
		(const struct ipa_rt_rule *)&entry->rule;
	u16 en_rule = 0;
	u32 tmp[IPA_RT_FLT_HW_RULE_BUF_SIZE/4];
	u8 *start;
	int pipe_idx;
	struct ipa_hdr_entry *hdr_entry;

	if (buf == NULL) {
		memset(tmp, 0, (IPA_RT_FLT_HW_RULE_BUF_SIZE/4));
		buf = (u8 *)tmp;
	}

	start = buf;
	rule_hdr = (struct ipa_rt_rule_hw_hdr *)buf;
	pipe_idx = ipa2_get_ep_mapping(entry->rule.dst);
	if (pipe_idx == -1) {
		IPAERR("Wrong destination pipe specified in RT rule\n");
		WARN_ON(1);
		return -EPERM;
	}
	if (!IPA_CLIENT_IS_CONS(entry->rule.dst)) {
		IPAERR("No RT rule on IPA_client_producer pipe.\n");
		IPAERR("pipe_idx: %d dst_pipe: %d\n",
				pipe_idx, entry->rule.dst);
		WARN_ON(1);
		return -EPERM;
	}
	rule_hdr->u.hdr.pipe_dest_idx = pipe_idx;
	rule_hdr->u.hdr.system = !ipa_ctx->hdr_tbl_lcl;

	/* Adding check to confirm still
	 * header entry present in header table or not
	 */

	if (entry->hdr) {
		hdr_entry = ipa_id_find(entry->rule.hdr_hdl);
		if (!hdr_entry || hdr_entry->cookie != IPA_HDR_COOKIE) {
			IPAERR_RL("Header entry already deleted\n");
			return -EPERM;
		}
	}
	if (entry->hdr) {
		if (entry->hdr->cookie == IPA_HDR_COOKIE) {
			rule_hdr->u.hdr.hdr_offset =
				entry->hdr->offset_entry->offset >> 2;
		} else {
			IPAERR("Entry hdr deleted by user = %d cookie = %u\n",
				entry->hdr->user_deleted, entry->hdr->cookie);
			WARN_ON(1);
			rule_hdr->u.hdr.hdr_offset = 0;
		}
	} else {
		rule_hdr->u.hdr.hdr_offset = 0;
	}
	buf += sizeof(struct ipa_rt_rule_hw_hdr);

	if (ipa_generate_hw_rule(ip, &rule->attrib, &buf, &en_rule)) {
		IPAERR("fail to generate hw rule\n");
		return -EPERM;
	}

	IPADBG_LOW("en_rule 0x%x\n", en_rule);

	rule_hdr->u.hdr.en_rule = en_rule;
	ipa_write_32(rule_hdr->u.word, (u8 *)rule_hdr);

	if (entry->hw_len == 0) {
		entry->hw_len = buf - start;
	} else if (entry->hw_len != (buf - start)) {
		IPAERR(
		"hw_len differs b/w passes passed=0x%x calc=0x%zxtd\n",
		entry->hw_len,
		(buf - start));
		return -EPERM;
	}

	return 0;
}

/**
 * __ipa_generate_rt_hw_rule_v2_5() - generates the routing hardware rule
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
int __ipa_generate_rt_hw_rule_v2_5(enum ipa_ip_type ip,
		struct ipa_rt_entry *entry, u8 *buf)
{
	struct ipa_rt_rule_hw_hdr *rule_hdr;
	const struct ipa_rt_rule *rule =
		(const struct ipa_rt_rule *)&entry->rule;
	u16 en_rule = 0;
	u32 tmp[IPA_RT_FLT_HW_RULE_BUF_SIZE/4];
	u8 *start;
	int pipe_idx;
	struct ipa_hdr_entry *hdr_entry;
	struct ipa_hdr_proc_ctx_entry *hdr_proc_entry;

	if (buf == NULL) {
		memset(tmp, 0, IPA_RT_FLT_HW_RULE_BUF_SIZE);
		buf = (u8 *)tmp;
	}

	start = buf;
	rule_hdr = (struct ipa_rt_rule_hw_hdr *)buf;
	pipe_idx = ipa2_get_ep_mapping(entry->rule.dst);
	if (pipe_idx == -1) {
		IPAERR("Wrong destination pipe specified in RT rule\n");
		WARN_ON(1);
		return -EPERM;
	}
	if (!IPA_CLIENT_IS_CONS(entry->rule.dst)) {
		IPAERR("No RT rule on IPA_client_producer pipe.\n");
		IPAERR("pipe_idx: %d dst_pipe: %d\n",
				pipe_idx, entry->rule.dst);
		WARN_ON(1);
		return -EPERM;
	}
	rule_hdr->u.hdr_v2_5.pipe_dest_idx = pipe_idx;
	/* Adding check to confirm still
	 * header entry present in header table or not
	 */

	if (entry->hdr) {
		hdr_entry = ipa_id_find(entry->rule.hdr_hdl);
		if (!hdr_entry || hdr_entry->cookie != IPA_HDR_COOKIE) {
			IPAERR_RL("Header entry already deleted\n");
			return -EPERM;
		}
	} else if (entry->proc_ctx) {
		hdr_proc_entry = ipa_id_find(entry->rule.hdr_proc_ctx_hdl);
		if (!hdr_proc_entry ||
			hdr_proc_entry->cookie != IPA_PROC_HDR_COOKIE) {
			IPAERR_RL("Proc header entry already deleted\n");
			return -EPERM;
		}
	}
	if (entry->proc_ctx || (entry->hdr && entry->hdr->is_hdr_proc_ctx)) {
		struct ipa_hdr_proc_ctx_entry *proc_ctx;

		proc_ctx = (entry->proc_ctx) ? : entry->hdr->proc_ctx;
		rule_hdr->u.hdr_v2_5.system = !ipa_ctx->hdr_proc_ctx_tbl_lcl;
		ipa_assert_on(proc_ctx->offset_entry->offset & 31);
		rule_hdr->u.hdr_v2_5.proc_ctx = 1;
		rule_hdr->u.hdr_v2_5.hdr_offset =
			(proc_ctx->offset_entry->offset +
			ipa_ctx->hdr_proc_ctx_tbl.start_offset) >> 5;
	} else if (entry->hdr) {
		rule_hdr->u.hdr_v2_5.system = !ipa_ctx->hdr_tbl_lcl;
		ipa_assert_on(entry->hdr->offset_entry->offset & 3);
		rule_hdr->u.hdr_v2_5.proc_ctx = 0;
		rule_hdr->u.hdr_v2_5.hdr_offset =
				entry->hdr->offset_entry->offset >> 2;
	} else {
		rule_hdr->u.hdr_v2_5.proc_ctx = 0;
		rule_hdr->u.hdr_v2_5.hdr_offset = 0;
	}
	buf += sizeof(struct ipa_rt_rule_hw_hdr);

	if (ipa_generate_hw_rule(ip, &rule->attrib, &buf, &en_rule)) {
		IPAERR("fail to generate hw rule\n");
		return -EPERM;
	}

	IPADBG("en_rule 0x%x\n", en_rule);

	rule_hdr->u.hdr_v2_5.en_rule = en_rule;
	ipa_write_32(rule_hdr->u.word, (u8 *)rule_hdr);

	if (entry->hw_len == 0) {
		entry->hw_len = buf - start;
	} else if (entry->hw_len != (buf - start)) {
		IPAERR("hw_len differs b/w passes passed=0x%x calc=0x%zxtd\n",
			entry->hw_len, (buf - start));
		return -EPERM;
	}

	return 0;
}

/**
 * __ipa_generate_rt_hw_rule_v2_6L() - generates the routing hardware rule
 * @ip: the ip address family type
 * @entry: routing entry
 * @buf: output buffer, buf == NULL means that the caller wants to know the size
 *       of the rule as seen by HW so they did not pass a valid buffer, we will
 *       use a scratch buffer instead.
 *       With this scheme we are going to generate the rule twice, once to know
 *       size using scratch buffer and second to write the rule to the actual
 *       caller supplied buffer which is of required size.
 *
 * Returns:	0 on success, negative on failure
 *
 * caller needs to hold any needed locks to ensure integrity
 *
 */
int __ipa_generate_rt_hw_rule_v2_6L(enum ipa_ip_type ip,
		struct ipa_rt_entry *entry, u8 *buf)
{
	/* Same implementation as IPAv2 */
	return __ipa_generate_rt_hw_rule_v2(ip, entry, buf);
}

/**
 * ipa_get_rt_hw_tbl_size() - returns the size of HW routing table
 * @ip: the ip address family type
 * @hdr_sz: header size
 * @max_rt_idx: maximal index
 *
 * Returns:	size on success, negative on failure
 *
 * caller needs to hold any needed locks to ensure integrity
 *
 * the MSB set in rt_idx_bitmap indicates the size of hdr of routing tbl
 */
static int ipa_get_rt_hw_tbl_size(enum ipa_ip_type ip, u32 *hdr_sz,
		int *max_rt_idx)
{
	struct ipa_rt_tbl_set *set;
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_entry *entry;
	u32 total_sz = 0;
	u32 tbl_sz;
	u32 bitmap = ipa_ctx->rt_idx_bitmap[ip];
	int highest_bit_set = IPA_RT_TABLE_INDEX_NOT_FOUND;
	int i;
	int res;

	*hdr_sz = 0;
	set = &ipa_ctx->rt_tbl_set[ip];

	for (i = 0; i < IPA_RT_INDEX_BITMAP_SIZE; i++) {
		if (bitmap & IPA_RT_BIT_MASK)
			highest_bit_set = i;
		bitmap >>= 1;
	}

	*max_rt_idx = highest_bit_set;
	if (highest_bit_set == IPA_RT_TABLE_INDEX_NOT_FOUND) {
		IPAERR("no rt tbls present\n");
		total_sz = IPA_RT_TABLE_WORD_SIZE;
		*hdr_sz = IPA_RT_TABLE_WORD_SIZE;
		return total_sz;
	}

	*hdr_sz = (highest_bit_set + 1) * IPA_RT_TABLE_WORD_SIZE;
	total_sz += *hdr_sz;
	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		tbl_sz = 0;
		list_for_each_entry(entry, &tbl->head_rt_rule_list, link) {
			res = ipa_ctx->ctrl->ipa_generate_rt_hw_rule(
				ip,
				entry,
				NULL);
			if (res) {
				IPAERR("failed to find HW RT rule size\n");
				return -EPERM;
			}
			tbl_sz += entry->hw_len;
		}

		if (tbl_sz)
			tbl->sz = tbl_sz + IPA_RT_TABLE_WORD_SIZE;

		if (tbl->in_sys)
			continue;

		if (tbl_sz) {
			/* add the terminator */
			total_sz += (tbl_sz + IPA_RT_TABLE_WORD_SIZE);
			/* every rule-set should start at word boundary */
			total_sz = (total_sz + IPA_RT_ENTRY_MEMORY_ALLIGNMENT) &
						~IPA_RT_ENTRY_MEMORY_ALLIGNMENT;
		}
	}

	IPADBG("RT HW TBL SZ %d HDR SZ %d IP %d\n", total_sz, *hdr_sz, ip);

	return total_sz;
}

static int ipa_generate_rt_hw_tbl_common(enum ipa_ip_type ip, u8 *base, u8 *hdr,
		u32 body_ofst, u32 apps_start_idx)
{
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_entry *entry;
	struct ipa_rt_tbl_set *set;
	u32 offset;
	u8 *body;
	struct ipa_mem_buffer rt_tbl_mem;
	u8 *rt_tbl_mem_body;
	int res;

	/* build the rt tbl in the DMA buffer to submit to IPA HW */
	body = base;

	set = &ipa_ctx->rt_tbl_set[ip];
	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		if (!tbl->in_sys) {
			offset = body - base + body_ofst;
			if (offset & IPA_RT_ENTRY_MEMORY_ALLIGNMENT) {
				IPAERR("offset is not word multiple %d\n",
						offset);
				goto proc_err;
			}

			/* convert offset to words from bytes */
			offset &= ~IPA_RT_ENTRY_MEMORY_ALLIGNMENT;
			/* rule is at an offset from base */
			offset |= IPA_RT_BIT_MASK;

			/* update the hdr at the right index */
			ipa_write_32(offset, hdr +
					((tbl->idx - apps_start_idx) *
					 IPA_RT_TABLE_WORD_SIZE));

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_rt_rule_list,
					link) {
				res = ipa_ctx->ctrl->ipa_generate_rt_hw_rule(
					ip,
					entry,
					body);
				if (res) {
					IPAERR("failed to gen HW RT rule\n");
					goto proc_err;
				}
				body += entry->hw_len;
			}

			/* write the rule-set terminator */
			body = ipa_write_32(0, body);
			if ((long)body & IPA_RT_ENTRY_MEMORY_ALLIGNMENT)
				/* advance body to next word boundary */
				body = body + (IPA_RT_TABLE_WORD_SIZE -
					      ((long)body &
					      IPA_RT_ENTRY_MEMORY_ALLIGNMENT));
		} else {
			if (tbl->sz == 0) {
				IPAERR("cannot generate 0 size table\n");
				goto proc_err;
			}

			/* allocate memory for the RT tbl */
			rt_tbl_mem.size = tbl->sz;
			rt_tbl_mem.base =
			   dma_alloc_coherent(ipa_ctx->pdev, rt_tbl_mem.size,
					   &rt_tbl_mem.phys_base, GFP_KERNEL);
			if (!rt_tbl_mem.base) {
				IPAERR("fail to alloc DMA buff of size %d\n",
						rt_tbl_mem.size);
				WARN_ON(1);
				goto proc_err;
			}

			WARN_ON(rt_tbl_mem.phys_base &
					IPA_RT_ENTRY_MEMORY_ALLIGNMENT);
			rt_tbl_mem_body = rt_tbl_mem.base;
			memset(rt_tbl_mem.base, 0, rt_tbl_mem.size);
			/* update the hdr at the right index */
			ipa_write_32(rt_tbl_mem.phys_base,
					hdr + ((tbl->idx - apps_start_idx) *
					IPA_RT_TABLE_WORD_SIZE));
			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_rt_rule_list,
					link) {
				res = ipa_ctx->ctrl->ipa_generate_rt_hw_rule(
					ip,
					entry,
					rt_tbl_mem_body);
				if (res) {
					IPAERR("failed to gen HW RT rule\n");
					WARN_ON(1);
					goto rt_table_mem_alloc_failed;
				}
				rt_tbl_mem_body += entry->hw_len;
			}

			/* write the rule-set terminator */
			rt_tbl_mem_body = ipa_write_32(0, rt_tbl_mem_body);

			if (tbl->curr_mem.phys_base) {
				WARN_ON(tbl->prev_mem.phys_base);
				tbl->prev_mem = tbl->curr_mem;
			}
			tbl->curr_mem = rt_tbl_mem;
		}
	}

	return 0;

rt_table_mem_alloc_failed:
	dma_free_coherent(ipa_ctx->pdev, rt_tbl_mem.size,
			  rt_tbl_mem.base, rt_tbl_mem.phys_base);
proc_err:
	return -EPERM;
}


/**
 * ipa_generate_rt_hw_tbl() - generates the routing hardware table
 * @ip:	[in] the ip address family type
 * @mem:	[out] buffer to put the filtering table
 *
 * Returns:	0 on success, negative on failure
 */
static int ipa_generate_rt_hw_tbl_v1_1(enum ipa_ip_type ip,
		struct ipa_mem_buffer *mem)
{
	u32 hdr_sz;
	u8 *hdr;
	u8 *body;
	u8 *base;
	int max_rt_idx;
	int i;
	int res;

	res = ipa_get_rt_hw_tbl_size(ip, &hdr_sz, &max_rt_idx);
	if (res < 0) {
		IPAERR("ipa_get_rt_hw_tbl_size failed %d\n", res);
		goto error;
	}

	mem->size = res;
	mem->size = (mem->size + IPA_RT_TABLE_MEMORY_ALLIGNMENT) &
				~IPA_RT_TABLE_MEMORY_ALLIGNMENT;

	if (mem->size == 0) {
		IPAERR("rt tbl empty ip=%d\n", ip);
		goto error;
	}
	mem->base = dma_alloc_coherent(ipa_ctx->pdev, mem->size,
			&mem->phys_base, GFP_KERNEL);
	if (!mem->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem->size);
		goto error;
	}

	memset(mem->base, 0, mem->size);

	/* build the rt tbl in the DMA buffer to submit to IPA HW */
	base = hdr = (u8 *)mem->base;
	body = base + hdr_sz;

	/* setup all indices to point to the empty sys rt tbl */
	for (i = 0; i <= max_rt_idx; i++)
		ipa_write_32(ipa_ctx->empty_rt_tbl_mem.phys_base,
				hdr + (i * IPA_RT_TABLE_WORD_SIZE));

	if (ipa_generate_rt_hw_tbl_common(ip, base, hdr, 0, 0)) {
		IPAERR("fail to generate RT tbl\n");
		goto proc_err;
	}

	return 0;

proc_err:
	dma_free_coherent(ipa_ctx->pdev, mem->size, mem->base, mem->phys_base);
	mem->base = NULL;
error:
	return -EPERM;
}

static void __ipa_reap_sys_rt_tbls(enum ipa_ip_type ip)
{
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_tbl *next;
	struct ipa_rt_tbl_set *set;

	set = &ipa_ctx->rt_tbl_set[ip];
	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		if (tbl->prev_mem.phys_base) {
			IPADBG_LOW("reaping rt");
			IPADBG_LOW("tbl name=%s ip=%d\n",
				tbl->name, ip);
			dma_free_coherent(ipa_ctx->pdev, tbl->prev_mem.size,
					tbl->prev_mem.base,
					tbl->prev_mem.phys_base);
			memset(&tbl->prev_mem, 0, sizeof(tbl->prev_mem));
		}
	}

	set = &ipa_ctx->reap_rt_tbl_set[ip];
	list_for_each_entry_safe(tbl, next, &set->head_rt_tbl_list, link) {
		list_del(&tbl->link);
		WARN_ON(tbl->prev_mem.phys_base != 0);
		if (tbl->curr_mem.phys_base) {
			IPADBG_LOW("reaping sys");
			IPADBG_LOW("rt tbl name=%s ip=%d\n",
				tbl->name, ip);
			dma_free_coherent(ipa_ctx->pdev, tbl->curr_mem.size,
					tbl->curr_mem.base,
					tbl->curr_mem.phys_base);
			kmem_cache_free(ipa_ctx->rt_tbl_cache, tbl);
		}
	}
}

int __ipa_commit_rt_v1_1(enum ipa_ip_type ip)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer *mem;
	void *cmd;
	struct ipa_ip_v4_routing_init *v4;
	struct ipa_ip_v6_routing_init *v6;
	u16 avail;
	u16 size;
	gfp_t flag = GFP_KERNEL | (ipa_ctx->use_dma_zone ? GFP_DMA : 0);

	mem = kmalloc(sizeof(struct ipa_mem_buffer), GFP_KERNEL);
	if (!mem) {
		IPAERR("failed to alloc memory object\n");
		goto fail_alloc_mem;
	}

	if (ip == IPA_IP_v4) {
		avail = ipa_ctx->ip4_rt_tbl_lcl ? IPA_MEM_v1_RAM_V4_RT_SIZE :
			IPA_MEM_PART(v4_rt_size_ddr);
		size = sizeof(struct ipa_ip_v4_routing_init);
	} else {
		avail = ipa_ctx->ip6_rt_tbl_lcl ? IPA_MEM_v1_RAM_V6_RT_SIZE :
			IPA_MEM_PART(v6_rt_size_ddr);
		size = sizeof(struct ipa_ip_v6_routing_init);
	}
	cmd = kmalloc(size, flag);
	if (!cmd) {
		IPAERR("failed to alloc immediate command object\n");
		goto fail_alloc_cmd;
	}

	if (ipa_generate_rt_hw_tbl_v1_1(ip, mem)) {
		IPAERR("fail to generate RT HW TBL ip %d\n", ip);
		goto fail_hw_tbl_gen;
	}

	if (mem->size > avail) {
		IPAERR("tbl too big, needed %d avail %d\n", mem->size, avail);
		goto fail_send_cmd;
	}

	if (ip == IPA_IP_v4) {
		v4 = (struct ipa_ip_v4_routing_init *)cmd;
		desc.opcode = IPA_IP_V4_ROUTING_INIT;
		v4->ipv4_rules_addr = mem->phys_base;
		v4->size_ipv4_rules = mem->size;
		v4->ipv4_addr = IPA_MEM_v1_RAM_V4_RT_OFST;
		IPADBG("putting Routing IPv4 rules to phys 0x%x",
				v4->ipv4_addr);
	} else {
		v6 = (struct ipa_ip_v6_routing_init *)cmd;
		desc.opcode = IPA_IP_V6_ROUTING_INIT;
		v6->ipv6_rules_addr = mem->phys_base;
		v6->size_ipv6_rules = mem->size;
		v6->ipv6_addr = IPA_MEM_v1_RAM_V6_RT_OFST;
		IPADBG("putting Routing IPv6 rules to phys 0x%x",
				v6->ipv6_addr);
	}

	desc.pyld = cmd;
	desc.len = size;
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem->base, mem->phys_base, mem->size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		goto fail_send_cmd;
	}

	__ipa_reap_sys_rt_tbls(ip);
	dma_free_coherent(ipa_ctx->pdev, mem->size, mem->base, mem->phys_base);
	kfree(cmd);
	kfree(mem);

	return 0;

fail_send_cmd:
	if (mem->base)
		dma_free_coherent(ipa_ctx->pdev, mem->size, mem->base,
				mem->phys_base);
fail_hw_tbl_gen:
	kfree(cmd);
fail_alloc_cmd:
	kfree(mem);
fail_alloc_mem:
	return -EPERM;
}

static int ipa_generate_rt_hw_tbl_v2(enum ipa_ip_type ip,
		struct ipa_mem_buffer *mem, struct ipa_mem_buffer *head)
{
	u32 hdr_sz;
	u8 *hdr;
	u8 *body;
	u8 *base;
	int max_rt_idx;
	int i;
	u32 *entr;
	int num_index;
	u32 body_start_offset;
	u32 apps_start_idx;
	int res;

	if (ip == IPA_IP_v4) {
		num_index = IPA_MEM_PART(v4_apps_rt_index_hi) -
			IPA_MEM_PART(v4_apps_rt_index_lo) + 1;
		body_start_offset = IPA_MEM_PART(apps_v4_rt_ofst) -
			IPA_MEM_PART(v4_rt_ofst);
		apps_start_idx = IPA_MEM_PART(v4_apps_rt_index_lo);
	} else {
		num_index = IPA_MEM_PART(v6_apps_rt_index_hi) -
			IPA_MEM_PART(v6_apps_rt_index_lo) + 1;
		body_start_offset = IPA_MEM_PART(apps_v6_rt_ofst) -
			IPA_MEM_PART(v6_rt_ofst);
		apps_start_idx = IPA_MEM_PART(v6_apps_rt_index_lo);
	}

	head->size = num_index * 4;
	head->base = dma_alloc_coherent(ipa_ctx->pdev, head->size,
			&head->phys_base, GFP_KERNEL);
	if (!head->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", head->size);
		goto err;
	}
	entr = (u32 *)head->base;
	hdr = (u8 *)head->base;
	for (i = 1; i <= num_index; i++) {
		*entr = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entr++;
	}

	res = ipa_get_rt_hw_tbl_size(ip, &hdr_sz, &max_rt_idx);
	if (res < 0) {
		IPAERR("ipa_get_rt_hw_tbl_size failed %d\n", res);
		goto base_err;
	}

	mem->size = res;
	mem->size -= hdr_sz;
	mem->size = (mem->size + IPA_RT_TABLE_MEMORY_ALLIGNMENT) &
				~IPA_RT_TABLE_MEMORY_ALLIGNMENT;

	if (mem->size > 0) {
		mem->base = dma_alloc_coherent(ipa_ctx->pdev, mem->size,
				&mem->phys_base, GFP_KERNEL);
		if (!mem->base) {
			IPAERR("fail to alloc DMA buff of size %d\n",
					mem->size);
			goto base_err;
		}
		memset(mem->base, 0, mem->size);
	}

	/* build the rt tbl in the DMA buffer to submit to IPA HW */
	body = base = (u8 *)mem->base;

	if (ipa_generate_rt_hw_tbl_common(ip, base, hdr, body_start_offset,
				apps_start_idx)) {
		IPAERR("fail to generate RT tbl\n");
		goto proc_err;
	}

	return 0;

proc_err:
	if (mem->size)
		dma_free_coherent(ipa_ctx->pdev, mem->size, mem->base,
			mem->phys_base);
base_err:
	dma_free_coherent(ipa_ctx->pdev, head->size, head->base,
			head->phys_base);
err:
	return -EPERM;
}

int __ipa_commit_rt_v2(enum ipa_ip_type ip)
{
	struct ipa_desc desc[2];
	struct ipa_mem_buffer body;
	struct ipa_mem_buffer head;
	struct ipa_hw_imm_cmd_dma_shared_mem *cmd1 = NULL;
	struct ipa_hw_imm_cmd_dma_shared_mem *cmd2 = NULL;
	gfp_t flag = GFP_KERNEL | (ipa_ctx->use_dma_zone ? GFP_DMA : 0);
	u16 avail;
	u32 num_modem_rt_index;
	int rc = 0;
	u32 local_addr1;
	u32 local_addr2;
	bool lcl;

	memset(desc, 0, 2 * sizeof(struct ipa_desc));

	if (ip == IPA_IP_v4) {
		avail = ipa_ctx->ip4_rt_tbl_lcl ?
			IPA_MEM_PART(apps_v4_rt_size) :
			IPA_MEM_PART(v4_rt_size_ddr);
		num_modem_rt_index =
			IPA_MEM_PART(v4_modem_rt_index_hi) -
			IPA_MEM_PART(v4_modem_rt_index_lo) + 1;
		local_addr1 = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_rt_ofst) +
			num_modem_rt_index * 4;
		local_addr2 = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_rt_ofst);
		lcl = ipa_ctx->ip4_rt_tbl_lcl;
	} else {
		avail = ipa_ctx->ip6_rt_tbl_lcl ?
			IPA_MEM_PART(apps_v6_rt_size) :
			IPA_MEM_PART(v6_rt_size_ddr);
		num_modem_rt_index =
			IPA_MEM_PART(v6_modem_rt_index_hi) -
			IPA_MEM_PART(v6_modem_rt_index_lo) + 1;
		local_addr1 = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_rt_ofst) +
			num_modem_rt_index * 4;
		local_addr2 = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_rt_ofst);
		lcl = ipa_ctx->ip6_rt_tbl_lcl;
	}

	if (ipa_generate_rt_hw_tbl_v2(ip, &body, &head)) {
		IPAERR("fail to generate RT HW TBL ip %d\n", ip);
		rc = -EFAULT;
		goto fail_gen;
	}

	if (body.size > avail) {
		IPAERR("tbl too big, needed %d avail %d\n", body.size, avail);
		rc = -EFAULT;
		goto fail_send_cmd;
	}

	cmd1 = kzalloc(sizeof(struct ipa_hw_imm_cmd_dma_shared_mem),
		flag);
	if (cmd1 == NULL) {
		IPAERR("Failed to alloc immediate command object\n");
		rc = -ENOMEM;
		goto fail_send_cmd;
	}

	cmd1->size = head.size;
	cmd1->system_addr = head.phys_base;
	cmd1->local_addr = local_addr1;
	desc[0].opcode = IPA_DMA_SHARED_MEM;
	desc[0].pyld = (void *)cmd1;
	desc[0].len = sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
	desc[0].type = IPA_IMM_CMD_DESC;

	if (lcl) {
		cmd2 = kzalloc(sizeof(struct ipa_hw_imm_cmd_dma_shared_mem),
			flag);
		if (cmd2 == NULL) {
			IPAERR("Failed to alloc immediate command object\n");
			rc = -ENOMEM;
			goto fail_send_cmd1;
		}

		cmd2->size = body.size;
		cmd2->system_addr = body.phys_base;
		cmd2->local_addr = local_addr2;

		desc[1].opcode = IPA_DMA_SHARED_MEM;
		desc[1].pyld = (void *)cmd2;
		desc[1].len = sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
		desc[1].type = IPA_IMM_CMD_DESC;

		if (ipa_send_cmd(2, desc)) {
			IPAERR("fail to send immediate command\n");
			rc = -EFAULT;
			goto fail_send_cmd2;
		}
	} else {
		if (ipa_send_cmd(1, desc)) {
			IPAERR("fail to send immediate command\n");
			rc = -EFAULT;
			goto fail_send_cmd1;
		}
	}

	IPADBG("HEAD\n");
	IPA_DUMP_BUFF(head.base, head.phys_base, head.size);
	if (body.size) {
		IPADBG("BODY\n");
		IPA_DUMP_BUFF(body.base, body.phys_base, body.size);
	}
	__ipa_reap_sys_rt_tbls(ip);

fail_send_cmd2:
	kfree(cmd2);
fail_send_cmd1:
	kfree(cmd1);
fail_send_cmd:
	dma_free_coherent(ipa_ctx->pdev, head.size, head.base, head.phys_base);
	if (body.size)
		dma_free_coherent(ipa_ctx->pdev, body.size, body.base,
				body.phys_base);
fail_gen:
	return rc;
}

/**
 * __ipa_find_rt_tbl() - find the routing table
 *			which name is given as parameter
 * @ip:	[in] the ip address family type of the wanted routing table
 * @name:	[in] the name of the wanted routing table
 *
 * Returns: the routing table which name is given as parameter, or NULL if it
 * doesn't exist
 */
struct ipa_rt_tbl *__ipa_find_rt_tbl(enum ipa_ip_type ip, const char *name)
{
	struct ipa_rt_tbl *entry;
	struct ipa_rt_tbl_set *set;

	set = &ipa_ctx->rt_tbl_set[ip];
	list_for_each_entry(entry, &set->head_rt_tbl_list, link) {
		if (!strcmp(name, entry->name))
			return entry;
	}

	return NULL;
}

/**
 * ipa2_query_rt_index() - find the routing table index
 *			which name and ip type are given as parameters
 * @in:	[out] the index of the wanted routing table
 *
 * Returns: the routing table which name is given as parameter, or NULL if it
 * doesn't exist
 */
int ipa2_query_rt_index(struct ipa_ioc_get_rt_tbl_indx *in)
{
	struct ipa_rt_tbl *entry;

	if (in->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	/* check if this table exists */
	in->name[IPA_RESOURCE_NAME_MAX-1] = '\0';
	entry = __ipa_find_rt_tbl(in->ip, in->name);
	if (!entry) {
		mutex_unlock(&ipa_ctx->lock);
		return -EFAULT;
	}

	in->idx  = entry->idx;
	mutex_unlock(&ipa_ctx->lock);
	return 0;
}

static struct ipa_rt_tbl *__ipa_add_rt_tbl(enum ipa_ip_type ip,
		const char *name)
{
	struct ipa_rt_tbl *entry;
	struct ipa_rt_tbl_set *set;
	int i;
	int id;

	if (ip >= IPA_IP_MAX || name == NULL) {
		IPAERR("bad parm\n");
		goto error;
	}

	set = &ipa_ctx->rt_tbl_set[ip];
	/* check if this table exists */
	entry = __ipa_find_rt_tbl(ip, name);
	if (!entry) {
		entry = kmem_cache_zalloc(ipa_ctx->rt_tbl_cache, GFP_KERNEL);
		if (!entry) {
			IPAERR("failed to alloc RT tbl object\n");
			goto error;
		}
		/* find a routing tbl index */
		for (i = 0; i < IPA_RT_INDEX_BITMAP_SIZE; i++) {
			if (!test_bit(i, &ipa_ctx->rt_idx_bitmap[ip])) {
				entry->idx = i;
				set_bit(i, &ipa_ctx->rt_idx_bitmap[ip]);
				break;
			}
		}
		if (i == IPA_RT_INDEX_BITMAP_SIZE) {
			IPAERR("not free RT tbl indices left\n");
			goto fail_rt_idx_alloc;
		}

		INIT_LIST_HEAD(&entry->head_rt_rule_list);
		INIT_LIST_HEAD(&entry->link);
		strlcpy(entry->name, name, IPA_RESOURCE_NAME_MAX);
		entry->set = set;
		entry->cookie = IPA_RT_TBL_COOKIE;
		entry->in_sys = (ip == IPA_IP_v4) ?
			!ipa_ctx->ip4_rt_tbl_lcl : !ipa_ctx->ip6_rt_tbl_lcl;
		set->tbl_cnt++;
		list_add(&entry->link, &set->head_rt_tbl_list);

		IPADBG("add rt tbl idx=%d tbl_cnt=%d ip=%d\n", entry->idx,
				set->tbl_cnt, ip);

		id = ipa_id_alloc(entry);
		if (id < 0) {
			IPAERR("failed to add to tree\n");
			WARN_ON(1);
			goto ipa_insert_failed;
		}
		entry->id = id;
	}

	return entry;

ipa_insert_failed:
	set->tbl_cnt--;
	list_del(&entry->link);
fail_rt_idx_alloc:
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->rt_tbl_cache, entry);
error:
	return NULL;
}

static int __ipa_del_rt_tbl(struct ipa_rt_tbl *entry)
{
	enum ipa_ip_type ip = IPA_IP_MAX;
	u32 id;

	if (entry == NULL || (entry->cookie != IPA_RT_TBL_COOKIE)) {
		IPAERR_RL("bad parms\n");
		return -EINVAL;
	}
	id = entry->id;
	if (ipa_id_find(id) == NULL) {
		IPAERR_RL("lookup failed\n");
		return -EPERM;
	}

	if (entry->set == &ipa_ctx->rt_tbl_set[IPA_IP_v4])
		ip = IPA_IP_v4;
	else if (entry->set == &ipa_ctx->rt_tbl_set[IPA_IP_v6])
		ip = IPA_IP_v6;
	else {
		WARN_ON(1);
		return -EPERM;
	}


	if (!entry->in_sys) {
		list_del(&entry->link);
		clear_bit(entry->idx, &ipa_ctx->rt_idx_bitmap[ip]);
		entry->set->tbl_cnt--;
		IPADBG_LOW("del rt tbl_idx=%d tbl_cnt=%d\n", entry->idx,
				entry->set->tbl_cnt);
		kmem_cache_free(ipa_ctx->rt_tbl_cache, entry);
	} else {
		list_move(&entry->link,
				&ipa_ctx->reap_rt_tbl_set[ip].head_rt_tbl_list);
		clear_bit(entry->idx, &ipa_ctx->rt_idx_bitmap[ip]);
		entry->set->tbl_cnt--;
		IPADBG_LOW("del sys rt tbl_idx=%d tbl_cnt=%d\n", entry->idx,
				entry->set->tbl_cnt);
	}

	/* remove the handle from the database */
	ipa_id_remove(id);
	return 0;
}

static int __ipa_add_rt_rule(enum ipa_ip_type ip, const char *name,
		const struct ipa_rt_rule *rule, u8 at_rear, u32 *rule_hdl,
		bool user)
{
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_entry *entry;
	struct ipa_hdr_entry *hdr = NULL;
	struct ipa_hdr_proc_ctx_entry *proc_ctx = NULL;
	int id;

	if (rule->hdr_hdl && rule->hdr_proc_ctx_hdl) {
		IPAERR("rule contains both hdr_hdl and hdr_proc_ctx_hdl\n");
		goto error;
	}

	if (rule->hdr_hdl) {
		hdr = ipa_id_find(rule->hdr_hdl);
		if ((hdr == NULL) || (hdr->cookie != IPA_HDR_COOKIE)) {
			IPAERR("rt rule does not point to valid hdr\n");
			goto error;
		}
	} else if (rule->hdr_proc_ctx_hdl) {
		proc_ctx = ipa_id_find(rule->hdr_proc_ctx_hdl);
		if ((proc_ctx == NULL) ||
			(proc_ctx->cookie != IPA_PROC_HDR_COOKIE)) {
			IPAERR("rt rule does not point to valid proc ctx\n");
			goto error;
		}
	}


	tbl = __ipa_add_rt_tbl(ip, name);
	if (tbl == NULL || (tbl->cookie != IPA_RT_TBL_COOKIE)) {
		IPAERR("bad params\n");
		goto error;
	}
	/*
	 * do not allow any rules to be added at end of the "default" routing
	 * tables
	 */
	if (!strcmp(tbl->name, IPA_DFLT_RT_TBL_NAME) &&
	    (tbl->rule_cnt > 0)) {
		IPAERR_RL("cannot add rules to default rt table\n");
		goto error;
	}

	entry = kmem_cache_zalloc(ipa_ctx->rt_rule_cache, GFP_KERNEL);
	if (!entry) {
		IPAERR("failed to alloc RT rule object\n");
		goto error;
	}
	INIT_LIST_HEAD(&entry->link);
	entry->cookie = IPA_RT_RULE_COOKIE;
	entry->rule = *rule;
	entry->tbl = tbl;
	entry->hdr = hdr;
	entry->proc_ctx = proc_ctx;
	if (at_rear)
		list_add_tail(&entry->link, &tbl->head_rt_rule_list);
	else
		list_add(&entry->link, &tbl->head_rt_rule_list);
	tbl->rule_cnt++;
	if (entry->hdr)
		entry->hdr->ref_cnt++;
	else if (entry->proc_ctx)
		entry->proc_ctx->ref_cnt++;
	id = ipa_id_alloc(entry);
	if (id < 0) {
		IPAERR("failed to add to tree\n");
		WARN_ON(1);
		goto ipa_insert_failed;
	}
	IPADBG_LOW("add rt rule tbl_idx=%d", tbl->idx);
	IPADBG_LOW("rule_cnt=%d\n", tbl->rule_cnt);
	*rule_hdl = id;
	entry->id = id;
	entry->ipacm_installed = user;

	return 0;

ipa_insert_failed:
	if (entry->hdr)
		entry->hdr->ref_cnt--;
	else if (entry->proc_ctx)
		entry->proc_ctx->ref_cnt--;
	list_del(&entry->link);
	kmem_cache_free(ipa_ctx->rt_rule_cache, entry);
error:
	return -EPERM;
}

/**
 * ipa2_add_rt_rule() - Add the specified routing rules to SW and optionally
 * commit to IPA HW
 * @rules:	[inout] set of routing rules to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_add_rt_rule(struct ipa_ioc_add_rt_rule *rules)
{
	return ipa2_add_rt_rule_usr(rules, false);
}

/**
 * ipa2_add_rt_rule_usr() - Add the specified routing rules to SW and optionally
 * commit to IPA HW
 * @rules:		[inout] set of routing rules to add
 * @user_only:	[in] indicate installed by userspace module
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_add_rt_rule_usr(struct ipa_ioc_add_rt_rule *rules, bool user_only)
{
	int i;
	int ret;

	if (rules == NULL || rules->num_rules == 0 || rules->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < rules->num_rules; i++) {
		rules->rt_tbl_name[IPA_RESOURCE_NAME_MAX-1] = '\0';
		if (__ipa_add_rt_rule(rules->ip, rules->rt_tbl_name,
					&rules->rules[i].rule,
					rules->rules[i].at_rear,
					&rules->rules[i].rt_rule_hdl,
					user_only)) {
			IPAERR_RL("failed to add rt rule %d\n", i);
			rules->rules[i].status = IPA_RT_STATUS_OF_ADD_FAILED;
		} else {
			rules->rules[i].status = 0;
		}
	}

	if (rules->commit)
		if (ipa_ctx->ctrl->ipa_commit_rt(rules->ip)) {
			ret = -EPERM;
			goto bail;
		}

	ret = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return ret;
}

int __ipa_del_rt_rule(u32 rule_hdl)
{
	struct ipa_rt_entry *entry;
	int id;
	struct ipa_hdr_entry *hdr_entry;
	struct ipa_hdr_proc_ctx_entry *hdr_proc_entry;

	entry = ipa_id_find(rule_hdl);

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
		hdr_entry = ipa_id_find(entry->rule.hdr_hdl);
		if (!hdr_entry || hdr_entry->cookie != IPA_HDR_COOKIE) {
			IPAERR_RL("Header entry already deleted\n");
			return -EINVAL;
		}
	} else if (entry->proc_ctx) {
		hdr_proc_entry = ipa_id_find(entry->rule.hdr_proc_ctx_hdl);
		if (!hdr_proc_entry ||
			hdr_proc_entry->cookie != IPA_PROC_HDR_COOKIE) {
			IPAERR_RL("Proc header entry already deleted\n");
			return -EINVAL;
		}
	}

	if (entry->hdr)
		__ipa_release_hdr(entry->hdr->id);
	else if (entry->proc_ctx)
		__ipa_release_hdr_proc_ctx(entry->proc_ctx->id);
	list_del(&entry->link);
	entry->tbl->rule_cnt--;
	IPADBG_LOW("del rt rule tbl_idx=%d rule_cnt=%d\n", entry->tbl->idx,
			entry->tbl->rule_cnt);
	if (entry->tbl->rule_cnt == 0 && entry->tbl->ref_cnt == 0) {
		if (__ipa_del_rt_tbl(entry->tbl))
			IPAERR_RL("fail to del RT tbl\n");
	}
	entry->cookie = 0;
	id = entry->id;
	kmem_cache_free(ipa_ctx->rt_rule_cache, entry);

	/* remove the handle from the database */
	ipa_id_remove(id);

	return 0;
}

/**
 * ipa2_del_rt_rule() - Remove the specified routing rules to SW and optionally
 * commit to IPA HW
 * @hdls:	[inout] set of routing rules to delete
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls)
{
	int i;
	int ret;

	if (hdls == NULL || hdls->num_hdls == 0 || hdls->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa_del_rt_rule(hdls->hdl[i].hdl)) {
			IPAERR_RL("failed to del rt rule %i\n", i);
			hdls->hdl[i].status = IPA_RT_STATUS_OF_DEL_FAILED;
		} else {
			hdls->hdl[i].status = 0;
		}
	}

	if (hdls->commit)
		if (ipa_ctx->ctrl->ipa_commit_rt(hdls->ip)) {
			ret = -EPERM;
			goto bail;
		}

	ret = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return ret;
}

/**
 * ipa2_commit_rt_rule() - Commit the current SW routing table of specified type
 * to IPA HW
 * @ip:	The family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_commit_rt(enum ipa_ip_type ip)
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
	if (ipa2_commit_flt(ip))
		return -EPERM;

	mutex_lock(&ipa_ctx->lock);
	if (ipa_ctx->ctrl->ipa_commit_rt(ip)) {
		ret = -EPERM;
		goto bail;
	}

	ret = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return ret;
}

/**
 * ipa2_reset_rt() - reset the current SW routing table of specified type
 * (does not commit to HW)
 * @ip:			[in] The family of routing tables
 * @user_only:	[in] indicate delete rules installed by userspace
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_reset_rt(enum ipa_ip_type ip, bool user_only)
{
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_tbl *tbl_next;
	struct ipa_rt_tbl_set *set;
	struct ipa_rt_entry *rule;
	struct ipa_rt_entry *rule_next;
	struct ipa_rt_tbl_set *rset;
	u32 apps_start_idx;
	struct ipa_hdr_entry *hdr_entry;
	struct ipa_hdr_proc_ctx_entry *hdr_proc_entry;
	int id;
	bool tbl_user = false;

	if (ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	if (ipa_ctx->ipa_hw_type >= IPA_HW_v2_0) {
		if (ip == IPA_IP_v4)
			apps_start_idx = IPA_MEM_PART(v4_apps_rt_index_lo);
		else
			apps_start_idx = IPA_MEM_PART(v6_apps_rt_index_lo);
	} else {
		apps_start_idx = 0;
	}

	/*
	 * issue a reset on the filtering module of same IP type since
	 * filtering rules point to routing tables
	 */
	if (ipa2_reset_flt(ip, user_only))
		IPAERR_RL("fail to reset flt ip=%d\n", ip);

	set = &ipa_ctx->rt_tbl_set[ip];
	rset = &ipa_ctx->reap_rt_tbl_set[ip];
	mutex_lock(&ipa_ctx->lock);
	IPADBG("reset rt ip=%d\n", ip);
	list_for_each_entry_safe(tbl, tbl_next, &set->head_rt_tbl_list, link) {
		tbl_user = false;
		list_for_each_entry_safe(rule, rule_next,
					 &tbl->head_rt_rule_list, link) {
			if (ipa_id_find(rule->id) == NULL) {
				WARN_ON(1);
				mutex_unlock(&ipa_ctx->lock);
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
				if (rule->hdr) {
					hdr_entry = ipa_id_find(
						rule->rule.hdr_hdl);
					if (!hdr_entry ||
					hdr_entry->cookie != IPA_HDR_COOKIE) {
						IPAERR_RL(
						"Header already deleted\n");
						mutex_unlock(&ipa_ctx->lock);
						return -EINVAL;
					}
				} else if (rule->proc_ctx) {
					hdr_proc_entry =
						ipa_id_find(
						rule->rule.hdr_proc_ctx_hdl);
					if (!hdr_proc_entry ||
						hdr_proc_entry->cookie !=
						IPA_PROC_HDR_COOKIE) {
						IPAERR_RL(
						"Proc entry already deleted\n");
						mutex_unlock(&ipa_ctx->lock);
						return -EINVAL;
					}
				}
				tbl->rule_cnt--;
				if (rule->hdr)
					__ipa_release_hdr(rule->hdr->id);
				else if (rule->proc_ctx)
					__ipa_release_hdr_proc_ctx(
						rule->proc_ctx->id);
				rule->cookie = 0;
				id = rule->id;
				kmem_cache_free(ipa_ctx->rt_rule_cache, rule);

				/* remove the handle from the database */
				ipa_id_remove(id);
			}
		}

		if (ipa_id_find(tbl->id) == NULL) {
			WARN_ON(1);
			mutex_unlock(&ipa_ctx->lock);
			return -EFAULT;
		}
		id = tbl->id;

		/* do not remove the "default" routing tbl which has index 0 */
		if (tbl->idx != apps_start_idx) {
			if (!user_only || tbl_user) {
				if (!tbl->in_sys) {
					list_del(&tbl->link);
					set->tbl_cnt--;
					clear_bit(tbl->idx,
						&ipa_ctx->rt_idx_bitmap[ip]);
					IPADBG("rst rt tbl_idx=%d tbl_cnt=%d\n",
							tbl->idx, set->tbl_cnt);
					kmem_cache_free(ipa_ctx->rt_tbl_cache,
						tbl);
				} else {
					list_move(&tbl->link,
						&rset->head_rt_tbl_list);
					clear_bit(tbl->idx,
						&ipa_ctx->rt_idx_bitmap[ip]);
					set->tbl_cnt--;
					IPADBG("rst tbl_idx=%d cnt=%d\n",
							tbl->idx, set->tbl_cnt);
				}
				/* remove the handle from the database */
				ipa_id_remove(id);
			}
		}
	}

	/* commit the change to IPA-HW */
	if (ipa_ctx->ctrl->ipa_commit_rt(IPA_IP_v4) ||
		ipa_ctx->ctrl->ipa_commit_rt(IPA_IP_v6)) {
		IPAERR("fail to commit rt-rule\n");
		WARN_ON_RATELIMIT_IPA(1);
		mutex_unlock(&ipa_ctx->lock);
		return -EPERM;
	}
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}

/**
 * ipa2_get_rt_tbl() - lookup the specified routing table and return handle if
 * it exists, if lookup succeeds the routing table ref cnt is increased
 * @lookup:	[inout] routing table to lookup and its handle
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 *	Caller should call ipa_put_rt_tbl later if this function succeeds
 */
int ipa2_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup)
{
	struct ipa_rt_tbl *entry;
	int result = -EFAULT;

	if (lookup == NULL || lookup->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}
	mutex_lock(&ipa_ctx->lock);
	lookup->name[IPA_RESOURCE_NAME_MAX-1] = '\0';
	entry = __ipa_find_rt_tbl(lookup->ip, lookup->name);
	if (entry && entry->cookie == IPA_RT_TBL_COOKIE) {
		if (entry->ref_cnt == U32_MAX) {
			IPAERR("fail: ref count crossed limit\n");
			goto ret;
		}
		entry->ref_cnt++;
		lookup->hdl = entry->id;

		/* commit for get */
		if (ipa_ctx->ctrl->ipa_commit_rt(lookup->ip))
			IPAERR_RL("fail to commit RT tbl\n");

		result = 0;
	}

ret:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}

/**
 * ipa2_put_rt_tbl() - Release the specified routing table handle
 * @rt_tbl_hdl:	[in] the routing table handle to release
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_put_rt_tbl(u32 rt_tbl_hdl)
{
	struct ipa_rt_tbl *entry;
	enum ipa_ip_type ip = IPA_IP_MAX;
	int result = 0;

	mutex_lock(&ipa_ctx->lock);
	entry = ipa_id_find(rt_tbl_hdl);
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

	if (entry->set == &ipa_ctx->rt_tbl_set[IPA_IP_v4])
		ip = IPA_IP_v4;
	else if (entry->set == &ipa_ctx->rt_tbl_set[IPA_IP_v6])
		ip = IPA_IP_v6;
	else {
		WARN_ON(1);
		result = -EINVAL;
		goto ret;
	}

	entry->ref_cnt--;
	if (entry->ref_cnt == 0 && entry->rule_cnt == 0) {
		if (__ipa_del_rt_tbl(entry))
			IPAERR_RL("fail to del RT tbl\n");
		/* commit for put */
		if (ipa_ctx->ctrl->ipa_commit_rt(ip))
			IPAERR_RL("fail to commit RT tbl\n");
	}

	result = 0;

ret:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}


static int __ipa_mdfy_rt_rule(struct ipa_rt_rule_mdfy *rtrule)
{
	struct ipa_rt_entry *entry;
	struct ipa_hdr_entry *hdr = NULL;
	struct ipa_hdr_entry *hdr_entry;

	if (rtrule->rule.hdr_hdl) {
		hdr = ipa_id_find(rtrule->rule.hdr_hdl);
		if ((hdr == NULL) || (hdr->cookie != IPA_HDR_COOKIE)) {
			IPAERR_RL("rt rule does not point to valid hdr\n");
			goto error;
		}
	}

	entry = ipa_id_find(rtrule->rt_rule_hdl);
	if (entry == NULL) {
		IPAERR_RL("lookup failed\n");
		goto error;
	}

	if (entry->cookie != IPA_RT_RULE_COOKIE) {
		IPAERR_RL("bad params\n");
		goto error;
	}

	if (!strcmp(entry->tbl->name, IPA_DFLT_RT_TBL_NAME)) {
		IPAERR_RL("Default tbl rule cannot be modified\n");
		return -EINVAL;
	}

	/* Adding check to confirm still
	 * header entry present in header table or not
	 */

	if (entry->hdr) {
		hdr_entry = ipa_id_find(entry->rule.hdr_hdl);
		if (!hdr_entry || hdr_entry->cookie != IPA_HDR_COOKIE) {
			IPAERR_RL("Header entry already deleted\n");
			return -EPERM;
		}
	}
	if (entry->hdr)
		entry->hdr->ref_cnt--;

	entry->rule = rtrule->rule;
	entry->hdr = hdr;

	if (entry->hdr)
		entry->hdr->ref_cnt++;

	return 0;

error:
	return -EPERM;
}

/**
 * ipa2_mdfy_rt_rule() - Modify the specified routing rules in SW and optionally
 * commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_mdfy_rt_rule(struct ipa_ioc_mdfy_rt_rule *hdls)
{
	int i;
	int result;

	if (hdls == NULL || hdls->num_rules == 0 || hdls->ip >= IPA_IP_MAX) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < hdls->num_rules; i++) {
		if (__ipa_mdfy_rt_rule(&hdls->rules[i])) {
			IPAERR_RL("failed to mdfy rt rule %i\n", i);
			hdls->rules[i].status = IPA_RT_STATUS_OF_MDFY_FAILED;
		} else {
			hdls->rules[i].status = 0;
		}
	}

	if (hdls->commit)
		if (ipa_ctx->ctrl->ipa_commit_rt(hdls->ip)) {
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}

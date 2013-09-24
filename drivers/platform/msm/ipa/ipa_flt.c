/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define IPA_FLT_TABLE_WORD_SIZE			(4)
#define IPA_FLT_ENTRY_MEMORY_ALLIGNMENT		(0x3)
#define IPA_FLT_BIT_MASK			(0x1)
#define IPA_FLT_TABLE_INDEX_NOT_FOUND		(-1)
#define IPA_FLT_STATUS_OF_ADD_FAILED		(-1)
#define IPA_FLT_STATUS_OF_DEL_FAILED		(-1)

/**
 * ipa_generate_flt_hw_rule() - generates the filtering hardware rule
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
static int ipa_generate_flt_hw_rule(enum ipa_ip_type ip,
		struct ipa_flt_entry *entry, u8 *buf)
{
	struct ipa_flt_rule_hw_hdr *hdr;
	const struct ipa_flt_rule *rule =
		(const struct ipa_flt_rule *)&entry->rule;
	u16 en_rule = 0;
	u32 tmp[IPA_RT_FLT_HW_RULE_BUF_SIZE/4];
	u8 *start;

	if (buf == NULL) {
		memset(tmp, 0, IPA_RT_FLT_HW_RULE_BUF_SIZE);
		buf = (u8 *)tmp;
	}

	start = buf;
	hdr = (struct ipa_flt_rule_hw_hdr *)buf;
	hdr->u.hdr.action = entry->rule.action;
	hdr->u.hdr.retain_hdr =  entry->rule.retain_hdr;
	hdr->u.hdr.to_uc = entry->rule.to_uc;
	if (entry->rt_tbl)
		hdr->u.hdr.rt_tbl_idx = entry->rt_tbl->idx;
	else
		/* for excp action flt rules, rt tbl index is meaningless */
		hdr->u.hdr.rt_tbl_idx = 0;
	hdr->u.hdr.rsvd = 0;
	buf += sizeof(struct ipa_flt_rule_hw_hdr);

	if (ipa_generate_hw_rule(ip, &rule->attrib, &buf, &en_rule)) {
		IPAERR("fail to generate hw rule\n");
		return -EPERM;
	}

	IPADBG("en_rule 0x%x, action=%d, rt_idx=%d, uc=%d, retain_hdr=%d\n",
			en_rule,
			hdr->u.hdr.action,
			hdr->u.hdr.rt_tbl_idx,
			hdr->u.hdr.to_uc,
			hdr->u.hdr.retain_hdr);

	hdr->u.hdr.en_rule = en_rule;
	ipa_write_32(hdr->u.word, (u8 *)hdr);

	if (entry->hw_len == 0) {
		entry->hw_len = buf - start;
	} else if (entry->hw_len != (buf - start)) {
		IPAERR("hw_len differs b/w passes passed=%x calc=%x\n",
		       entry->hw_len, (buf - start));
		return -EPERM;
	}

	return 0;
}

/**
 * ipa_get_flt_hw_tbl_size() - returns the size of HW filtering table
 * @ip: the ip address family type
 * @hdr_sz: header size
 *
 * Returns:	0 on success, negative on failure
 *
 * caller needs to hold any needed locks to ensure integrity
 *
 */
static int ipa_get_flt_hw_tbl_size(enum ipa_ip_type ip, u32 *hdr_sz)
{
	struct ipa_flt_tbl *tbl;
	struct ipa_flt_entry *entry;
	u32 total_sz = 0;
	u32 rule_set_sz;
	int i;

	*hdr_sz = 0;
	tbl = &ipa_ctx->glob_flt_tbl[ip];
	rule_set_sz = 0;
	list_for_each_entry(entry, &tbl->head_flt_rule_list, link) {
		if (ipa_generate_flt_hw_rule(ip, entry, NULL)) {
			IPAERR("failed to find HW FLT rule size\n");
			return -EPERM;
		}
		IPADBG("glob ip %d len %d\n", ip, entry->hw_len);
		rule_set_sz += entry->hw_len;
	}

	if (rule_set_sz) {
		tbl->sz = rule_set_sz + IPA_FLT_TABLE_WORD_SIZE;
		/* this rule-set uses a word in header block */
		*hdr_sz += IPA_FLT_TABLE_WORD_SIZE;
		if (!tbl->in_sys) {
			/* add the terminator */
			total_sz += (rule_set_sz + IPA_FLT_TABLE_WORD_SIZE);
			total_sz = (total_sz +
					IPA_FLT_ENTRY_MEMORY_ALLIGNMENT) &
					~IPA_FLT_ENTRY_MEMORY_ALLIGNMENT;
		}
	}

	for (i = 0; i < IPA_NUM_PIPES; i++) {
		tbl = &ipa_ctx->flt_tbl[i][ip];
		rule_set_sz = 0;
		list_for_each_entry(entry, &tbl->head_flt_rule_list, link) {
			if (ipa_generate_flt_hw_rule(ip, entry, NULL)) {
				IPAERR("failed to find HW FLT rule size\n");
				return -EPERM;
			}
			IPADBG("pipe %d len %d\n", i, entry->hw_len);
			rule_set_sz += entry->hw_len;
		}

		if (rule_set_sz) {
			tbl->sz = rule_set_sz + IPA_FLT_TABLE_WORD_SIZE;
			/* this rule-set uses a word in header block */
			*hdr_sz += IPA_FLT_TABLE_WORD_SIZE;
			if (!tbl->in_sys) {
				/* add the terminator */
				total_sz += (rule_set_sz +
					    IPA_FLT_TABLE_WORD_SIZE);
				total_sz = (total_sz +
					IPA_FLT_ENTRY_MEMORY_ALLIGNMENT) &
					~IPA_FLT_ENTRY_MEMORY_ALLIGNMENT;
			}
		}
	}

	*hdr_sz += IPA_FLT_TABLE_WORD_SIZE;
	total_sz += *hdr_sz;
	IPADBG("FLT HW TBL SZ %d HDR SZ %d IP %d\n", total_sz, *hdr_sz, ip);

	return total_sz;
}

/**
 * ipa_generate_flt_hw_tbl() - generates the filtering hardware table
 * @ip:	[in] the ip address family type
 * @mem:	[out] buffer to put the filtering table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_generate_flt_hw_tbl(enum ipa_ip_type ip, struct ipa_mem_buffer *mem)
{
	struct ipa_flt_tbl *tbl;
	struct ipa_flt_entry *entry;
	u32 hdr_top = 0;
	int i;
	u32 hdr_sz;
	u32 offset;
	u8 *hdr;
	u8 *body;
	u8 *base;
	struct ipa_mem_buffer flt_tbl_mem;
	u8 *ftbl_membody;

	mem->size = ipa_get_flt_hw_tbl_size(ip, &hdr_sz);
	mem->size = IPA_HW_TABLE_ALIGNMENT(mem->size);

	if (mem->size == 0) {
		IPAERR("flt tbl empty ip=%d\n", ip);
		goto error;
	}
	mem->base = dma_alloc_coherent(NULL, mem->size, &mem->phys_base,
			GFP_KERNEL);
	if (!mem->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem->size);
		goto error;
	}

	memset(mem->base, 0, mem->size);

	/* build the flt tbl in the DMA buffer to submit to IPA HW */
	base = hdr = (u8 *)mem->base;
	body = base + hdr_sz;

	/* write a dummy header to move cursor */
	hdr = ipa_write_32(hdr_top, hdr);

	tbl = &ipa_ctx->glob_flt_tbl[ip];

	if (!list_empty(&tbl->head_flt_rule_list)) {
		hdr_top |= IPA_FLT_BIT_MASK;
		if (!tbl->in_sys) {
			offset = body - base;
			if (offset & IPA_FLT_ENTRY_MEMORY_ALLIGNMENT) {
				IPAERR("offset is not word multiple %d\n",
						offset);
				goto proc_err;
			}

			offset &= ~IPA_FLT_ENTRY_MEMORY_ALLIGNMENT;
			/* rule is at an offset from base */
			offset |= IPA_FLT_BIT_MASK;
			hdr = ipa_write_32(offset, hdr);

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_flt_rule_list,
					link) {
				if (ipa_generate_flt_hw_rule(ip, entry, body)) {
					IPAERR("failed to gen HW FLT rule\n");
					goto proc_err;
				}
				body += entry->hw_len;
			}

			/* write the rule-set terminator */
			body = ipa_write_32(0, body);
			if ((u32)body & IPA_FLT_ENTRY_MEMORY_ALLIGNMENT)
				/* advance body to next word boundary */
				body = body + (IPA_FLT_TABLE_WORD_SIZE -
					((u32)body &
					IPA_FLT_ENTRY_MEMORY_ALLIGNMENT));
		} else {
			WARN_ON(tbl->sz == 0);
			/* allocate memory for the flt tbl */
			flt_tbl_mem.size = tbl->sz;
			flt_tbl_mem.base =
			   dma_alloc_coherent(NULL, flt_tbl_mem.size,
					   &flt_tbl_mem.phys_base, GFP_KERNEL);
			if (!flt_tbl_mem.base) {
				IPAERR("fail to alloc DMA buff of size %d\n",
						flt_tbl_mem.size);
				WARN_ON(1);
				goto proc_err;
			}

			WARN_ON(flt_tbl_mem.phys_base &
				IPA_FLT_ENTRY_MEMORY_ALLIGNMENT);
			ftbl_membody = flt_tbl_mem.base;
			memset(flt_tbl_mem.base, 0, flt_tbl_mem.size);
			hdr = ipa_write_32(flt_tbl_mem.phys_base, hdr);

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_flt_rule_list,
					link) {
				if (ipa_generate_flt_hw_rule(ip, entry,
							ftbl_membody)) {
					IPAERR("failed to gen HW FLT rule\n");
					WARN_ON(1);
				}
				ftbl_membody += entry->hw_len;
			}

			/* write the rule-set terminator */
			ftbl_membody = ipa_write_32(0, ftbl_membody);
			if (tbl->curr_mem.phys_base) {
				WARN_ON(tbl->prev_mem.phys_base);
				tbl->prev_mem = tbl->curr_mem;
			}
			tbl->curr_mem = flt_tbl_mem;
		}
	}

	for (i = 0; i < IPA_NUM_PIPES; i++) {
		tbl = &ipa_ctx->flt_tbl[i][ip];
		if (!list_empty(&tbl->head_flt_rule_list)) {
			/* pipe "i" is at bit "i+1" */
			hdr_top |= (1 << (i + 1));
			if (!tbl->in_sys) {
				offset = body - base;
				if (offset & IPA_FLT_ENTRY_MEMORY_ALLIGNMENT) {
					IPAERR("ofst is not word multiple %d\n",
					       offset);
					goto proc_err;
				}
				offset &= ~IPA_FLT_ENTRY_MEMORY_ALLIGNMENT;
				/* rule is at an offset from base */
				offset |= IPA_FLT_BIT_MASK;
				hdr = ipa_write_32(offset, hdr);

				/* generate the rule-set */
				list_for_each_entry(entry,
						&tbl->head_flt_rule_list,
						link) {
					if (ipa_generate_flt_hw_rule(ip, entry,
								body)) {
						IPAERR("fail gen FLT rule\n");
						goto proc_err;
					}
					body += entry->hw_len;
				}

				/* write the rule-set terminator */
				body = ipa_write_32(0, body);
				if ((u32)body & IPA_FLT_ENTRY_MEMORY_ALLIGNMENT)
					/* advance body to next word boundary */
					body = body + (IPA_FLT_TABLE_WORD_SIZE -
						((u32)body &
					IPA_FLT_ENTRY_MEMORY_ALLIGNMENT));
			} else {
				WARN_ON(tbl->sz == 0);
				/* allocate memory for the flt tbl */
				flt_tbl_mem.size = tbl->sz;
				flt_tbl_mem.base =
				   dma_alloc_coherent(NULL, flt_tbl_mem.size,
						   &flt_tbl_mem.phys_base,
						   GFP_KERNEL);
				if (!flt_tbl_mem.base) {
					IPAERR("fail alloc DMA buff size %d\n",
							flt_tbl_mem.size);
					WARN_ON(1);
					goto proc_err;
				}

				WARN_ON(flt_tbl_mem.phys_base &
				IPA_FLT_ENTRY_MEMORY_ALLIGNMENT);

				ftbl_membody = flt_tbl_mem.base;
				memset(flt_tbl_mem.base, 0, flt_tbl_mem.size);
				hdr = ipa_write_32(flt_tbl_mem.phys_base, hdr);

				/* generate the rule-set */
				list_for_each_entry(entry,
						&tbl->head_flt_rule_list,
						link) {
					if (ipa_generate_flt_hw_rule(ip, entry,
							ftbl_membody)) {
						IPAERR("fail gen FLT rule\n");
						WARN_ON(1);
					}
					ftbl_membody += entry->hw_len;
				}

				/* write the rule-set terminator */
				ftbl_membody =
					ipa_write_32(0, ftbl_membody);
				if (tbl->curr_mem.phys_base) {
					WARN_ON(tbl->prev_mem.phys_base);
					tbl->prev_mem = tbl->curr_mem;
				}
				tbl->curr_mem = flt_tbl_mem;
			}
		}
	}

	/* now write the hdr_top */
	ipa_write_32(hdr_top, base);

	return 0;
proc_err:
	dma_free_coherent(NULL, mem->size, mem->base, mem->phys_base);
	mem->base = NULL;
error:

	return -EPERM;
}

static void __ipa_reap_sys_flt_tbls(enum ipa_ip_type ip)
{
	struct ipa_flt_tbl *tbl;
	int i;

	tbl = &ipa_ctx->glob_flt_tbl[ip];
	if (tbl->prev_mem.phys_base) {
		IPADBG("reaping glob flt tbl (prev) ip=%d\n", ip);
		dma_free_coherent(NULL, tbl->prev_mem.size, tbl->prev_mem.base,
				tbl->prev_mem.phys_base);
		memset(&tbl->prev_mem, 0, sizeof(tbl->prev_mem));
	}

	if (list_empty(&tbl->head_flt_rule_list)) {
		if (tbl->curr_mem.phys_base) {
			IPADBG("reaping glob flt tbl (curr) ip=%d\n", ip);
			dma_free_coherent(NULL, tbl->curr_mem.size,
					tbl->curr_mem.base,
					tbl->curr_mem.phys_base);
			memset(&tbl->curr_mem, 0, sizeof(tbl->curr_mem));
		}
	}

	for (i = 0; i < IPA_NUM_PIPES; i++) {
		tbl = &ipa_ctx->flt_tbl[i][ip];
		if (tbl->prev_mem.phys_base) {
			IPADBG("reaping flt tbl (prev) pipe=%d ip=%d\n", i, ip);
			dma_free_coherent(NULL, tbl->prev_mem.size,
					tbl->prev_mem.base,
					tbl->prev_mem.phys_base);
			memset(&tbl->prev_mem, 0, sizeof(tbl->prev_mem));
		}

		if (list_empty(&tbl->head_flt_rule_list)) {
			if (tbl->curr_mem.phys_base) {
				IPADBG("reaping flt tbl (curr) pipe=%d ip=%d\n",
						i, ip);
				dma_free_coherent(NULL, tbl->curr_mem.size,
						tbl->curr_mem.base,
						tbl->curr_mem.phys_base);
				memset(&tbl->curr_mem, 0,
						sizeof(tbl->curr_mem));
			}
		}
	}
}

static int __ipa_commit_flt(enum ipa_ip_type ip)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer *mem;
	void *cmd;
	struct ipa_ip_v4_filter_init *v4;
	struct ipa_ip_v6_filter_init *v6;
	u16 avail;
	u16 size;

	mem = kmalloc(sizeof(struct ipa_mem_buffer), GFP_KERNEL);
	if (!mem) {
		IPAERR("failed to alloc memory object\n");
		goto fail_alloc_mem;
	}

	if (ip == IPA_IP_v4) {
		avail = IPA_RAM_V4_FLT_SIZE;
		size = sizeof(struct ipa_ip_v4_filter_init);
	} else {
		avail = IPA_RAM_V6_FLT_SIZE;
		size = sizeof(struct ipa_ip_v6_filter_init);
	}
	cmd = kmalloc(size, GFP_KERNEL);
	if (!cmd) {
		IPAERR("failed to alloc immediate command object\n");
		goto fail_alloc_cmd;
	}

	if (ipa_generate_flt_hw_tbl(ip, mem)) {
		IPAERR("fail to generate FLT HW TBL ip %d\n", ip);
		goto fail_hw_tbl_gen;
	}

	if (mem->size > avail) {
		IPAERR("tbl too big, needed %d avail %d\n", mem->size, avail);
		goto fail_send_cmd;
	}

	if (ip == IPA_IP_v4) {
		v4 = (struct ipa_ip_v4_filter_init *)cmd;
		desc.opcode = IPA_IP_V4_FILTER_INIT;
		v4->ipv4_rules_addr = mem->phys_base;
		v4->size_ipv4_rules = mem->size;
		v4->ipv4_addr = ipa_ctx->ctrl->sram_flt_ipv4_ofst;
	} else {
		v6 = (struct ipa_ip_v6_filter_init *)cmd;
		desc.opcode = IPA_IP_V6_FILTER_INIT;
		v6->ipv6_rules_addr = mem->phys_base;
		v6->size_ipv6_rules = mem->size;
		v6->ipv6_addr = ipa_ctx->ctrl->sram_flt_ipv6_ofst;
	}

	desc.pyld = cmd;
	desc.len = size;
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem->base, mem->phys_base, mem->size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		goto fail_send_cmd;
	}

	__ipa_reap_sys_flt_tbls(ip);
	dma_free_coherent(NULL, mem->size, mem->base, mem->phys_base);
	kfree(cmd);
	kfree(mem);

	return 0;

fail_send_cmd:
	if (mem->phys_base)
		dma_free_coherent(NULL, mem->size, mem->base, mem->phys_base);
fail_hw_tbl_gen:
	kfree(cmd);
fail_alloc_cmd:
	kfree(mem);
fail_alloc_mem:

	return -EPERM;
}

static int __ipa_add_flt_rule(struct ipa_flt_tbl *tbl, enum ipa_ip_type ip,
			      const struct ipa_flt_rule *rule, u8 add_rear,
			      u32 *rule_hdl)
{
	struct ipa_flt_entry *entry;
	struct ipa_tree_node *node;

	if (rule->action != IPA_PASS_TO_EXCEPTION) {
		if (!rule->rt_tbl_hdl) {
			IPAERR("flt rule does not point to valid RT tbl\n");
			goto error;
		}

		if (ipa_search(&ipa_ctx->rt_tbl_hdl_tree,
					rule->rt_tbl_hdl) == NULL) {
			IPAERR("RT tbl not found\n");
			goto error;
		}

		if (((struct ipa_rt_tbl *)rule->rt_tbl_hdl)->cookie !=
				IPA_COOKIE) {
			IPAERR("RT table cookie is invalid\n");
			goto error;
		}
	}

	node = kmem_cache_zalloc(ipa_ctx->tree_node_cache, GFP_KERNEL);
	if (!node) {
		IPAERR("failed to alloc tree node object\n");
		goto error;
	}

	entry = kmem_cache_zalloc(ipa_ctx->flt_rule_cache, GFP_KERNEL);
	if (!entry) {
		IPAERR("failed to alloc FLT rule object\n");
		goto mem_alloc_fail;
	}
	INIT_LIST_HEAD(&entry->link);
	entry->rule = *rule;
	entry->cookie = IPA_COOKIE;
	entry->rt_tbl = (struct ipa_rt_tbl *)rule->rt_tbl_hdl;
	entry->tbl = tbl;
	if (add_rear)
		list_add_tail(&entry->link, &tbl->head_flt_rule_list);
	else
		list_add(&entry->link, &tbl->head_flt_rule_list);
	tbl->rule_cnt++;
	if (entry->rt_tbl)
		entry->rt_tbl->ref_cnt++;
	*rule_hdl = (u32)entry;
	IPADBG("add flt rule rule_cnt=%d\n", tbl->rule_cnt);

	node->hdl = *rule_hdl;
	if (ipa_insert(&ipa_ctx->flt_rule_hdl_tree, node)) {
		IPAERR("failed to add to tree\n");
		WARN_ON(1);
	}

	return 0;

mem_alloc_fail:
	kmem_cache_free(ipa_ctx->tree_node_cache, node);
error:

	return -EPERM;
}

static int __ipa_del_flt_rule(u32 rule_hdl)
{
	struct ipa_flt_entry *entry = (struct ipa_flt_entry *)rule_hdl;
	struct ipa_tree_node *node;

	node = ipa_search(&ipa_ctx->flt_rule_hdl_tree, rule_hdl);
	if (node == NULL) {
		IPAERR("lookup failed\n");

		return -EINVAL;
	}

	if (entry == NULL || (entry->cookie != IPA_COOKIE)) {
		IPAERR("bad params\n");

		return -EINVAL;
	}

	list_del(&entry->link);
	entry->tbl->rule_cnt--;
	if (entry->rt_tbl)
		entry->rt_tbl->ref_cnt--;
	IPADBG("del flt rule rule_cnt=%d\n", entry->tbl->rule_cnt);
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->flt_rule_cache, entry);

	/* remove the handle from the database */
	rb_erase(&node->node, &ipa_ctx->flt_rule_hdl_tree);
	kmem_cache_free(ipa_ctx->tree_node_cache, node);

	return 0;
}

static int __ipa_add_global_flt_rule(enum ipa_ip_type ip,
		const struct ipa_flt_rule *rule, u8 add_rear, u32 *rule_hdl)
{
	struct ipa_flt_tbl *tbl;

	if (rule == NULL || rule_hdl == NULL) {
		IPAERR("bad parms rule=%p rule_hdl=%p\n", rule, rule_hdl);

		return -EINVAL;
	}

	tbl = &ipa_ctx->glob_flt_tbl[ip];
	IPADBG("add global flt rule ip=%d\n", ip);

	return __ipa_add_flt_rule(tbl, ip, rule, add_rear, rule_hdl);
}

static int __ipa_add_ep_flt_rule(enum ipa_ip_type ip, enum ipa_client_type ep,
				 const struct ipa_flt_rule *rule, u8 add_rear,
				 u32 *rule_hdl)
{
	struct ipa_flt_tbl *tbl;
	int ipa_ep_idx;

	if (rule == NULL || rule_hdl == NULL || ep >= IPA_CLIENT_MAX) {
		IPAERR("bad parms rule=%p rule_hdl=%p ep=%d\n", rule,
				rule_hdl, ep);

		return -EINVAL;
	}
	ipa_ep_idx = ipa_get_ep_mapping(ipa_ctx->mode, ep);
	if (ipa_ep_idx == IPA_FLT_TABLE_INDEX_NOT_FOUND) {
		IPAERR("ep not valid ep=%d\n", ep);
		return -EINVAL;
	}
	if (ipa_ctx->ep[ipa_ep_idx].valid == 0)
		IPADBG("ep not connected ep_idx=%d\n", ipa_ep_idx);

	tbl = &ipa_ctx->flt_tbl[ipa_ep_idx][ip];
	IPADBG("add ep flt rule ip=%d ep=%d\n", ip, ep);

	return __ipa_add_flt_rule(tbl, ip, rule, add_rear, rule_hdl);
}

/**
 * ipa_add_flt_rule() - Add the specified filtering rules to SW and optionally
 * commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_flt_rule(struct ipa_ioc_add_flt_rule *rules)
{
	int i;
	int result;

	if (rules == NULL || rules->num_rules == 0 ||
			rules->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");

		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < rules->num_rules; i++) {
		if (rules->global)
			result = __ipa_add_global_flt_rule(rules->ip,
					&rules->rules[i].rule,
					rules->rules[i].at_rear,
					&rules->rules[i].flt_rule_hdl);
		else
			result = __ipa_add_ep_flt_rule(rules->ip, rules->ep,
					&rules->rules[i].rule,
					rules->rules[i].at_rear,
					&rules->rules[i].flt_rule_hdl);
		if (result) {
			IPAERR("failed to add flt rule %d\n", i);
			rules->rules[i].status = IPA_FLT_STATUS_OF_ADD_FAILED;
		} else {
			rules->rules[i].status = 0;
		}
	}

	if (rules->commit)
		if (__ipa_commit_flt(rules->ip)) {
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}
EXPORT_SYMBOL(ipa_add_flt_rule);

/**
 * ipa_del_flt_rule() - Remove the specified filtering rules from SW and
 * optionally commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls)
{
	int i;
	int result;

	if (hdls == NULL || hdls->num_hdls == 0 || hdls->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa_del_flt_rule(hdls->hdl[i].hdl)) {
			IPAERR("failed to del rt rule %i\n", i);
			hdls->hdl[i].status = IPA_FLT_STATUS_OF_DEL_FAILED;
		} else {
			hdls->hdl[i].status = 0;
		}
	}

	if (hdls->commit)
		if (__ipa_commit_flt(hdls->ip)) {
			mutex_unlock(&ipa_ctx->lock);
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}
EXPORT_SYMBOL(ipa_del_flt_rule);

/**
 * ipa_commit_flt() - Commit the current SW filtering table of specified type to
 * IPA HW
 * @ip:	[in] the family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_commit_flt(enum ipa_ip_type ip)
{
	int result;

	if (ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);

	if (__ipa_commit_flt(ip)) {
		result = -EPERM;
		goto bail;
	}
	result = 0;

bail:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}
EXPORT_SYMBOL(ipa_commit_flt);

/**
 * ipa_reset_flt() - Reset the current SW filtering table of specified type
 * (does not commit to HW)
 * @ip:	[in] the family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_reset_flt(enum ipa_ip_type ip)
{
	struct ipa_flt_tbl *tbl;
	struct ipa_flt_entry *entry;
	struct ipa_flt_entry *next;
	struct ipa_tree_node *node;
	int i;

	if (ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	tbl = &ipa_ctx->glob_flt_tbl[ip];
	mutex_lock(&ipa_ctx->lock);
	IPADBG("reset flt ip=%d\n", ip);
	list_for_each_entry_safe(entry, next, &tbl->head_flt_rule_list, link) {
		node = ipa_search(&ipa_ctx->flt_rule_hdl_tree, (u32)entry);
		if (node == NULL)
			WARN_ON(1);

		if ((ip == IPA_IP_v4 &&
		     entry->rule.attrib.attrib_mask == IPA_FLT_PROTOCOL &&
		     entry->rule.attrib.u.v4.protocol ==
		      IPA_INVALID_L4_PROTOCOL) ||
		    (ip == IPA_IP_v6 &&
		     entry->rule.attrib.attrib_mask == IPA_FLT_NEXT_HDR &&
		     entry->rule.attrib.u.v6.next_hdr ==
		      IPA_INVALID_L4_PROTOCOL))
			continue;

		list_del(&entry->link);
		entry->tbl->rule_cnt--;
		if (entry->rt_tbl)
			entry->rt_tbl->ref_cnt--;
		entry->cookie = 0;
		kmem_cache_free(ipa_ctx->flt_rule_cache, entry);

		/* remove the handle from the database */
		rb_erase(&node->node, &ipa_ctx->flt_rule_hdl_tree);
		kmem_cache_free(ipa_ctx->tree_node_cache, node);
	}

	for (i = 0; i < IPA_NUM_PIPES; i++) {
		tbl = &ipa_ctx->flt_tbl[i][ip];
		list_for_each_entry_safe(entry, next, &tbl->head_flt_rule_list,
				link) {
			node = ipa_search(&ipa_ctx->flt_rule_hdl_tree,
					(u32)entry);
			if (node == NULL)
				WARN_ON(1);
			list_del(&entry->link);
			entry->tbl->rule_cnt--;
			if (entry->rt_tbl)
				entry->rt_tbl->ref_cnt--;
			entry->cookie = 0;
			kmem_cache_free(ipa_ctx->flt_rule_cache, entry);

			/* remove the handle from the database */
			rb_erase(&node->node, &ipa_ctx->flt_rule_hdl_tree);
			kmem_cache_free(ipa_ctx->tree_node_cache, node);
		}
	}
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(ipa_reset_flt);

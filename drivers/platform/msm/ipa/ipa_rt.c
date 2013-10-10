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

/**
 * ipa_generate_rt_hw_rule() - generates the routing hardware rule
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
static int ipa_generate_rt_hw_rule(enum ipa_ip_type ip,
		struct ipa_rt_entry *entry, u8 *buf)
{
	struct ipa_rt_rule_hw_hdr *rule_hdr;
	const struct ipa_rt_rule *rule =
		(const struct ipa_rt_rule *)&entry->rule;
	u16 en_rule = 0;
	u32 tmp[IPA_RT_FLT_HW_RULE_BUF_SIZE/4];
	u8 *start;
	int pipe_idx;

	if (buf == NULL) {
		memset(tmp, 0, IPA_RT_FLT_HW_RULE_BUF_SIZE);
		buf = (u8 *)tmp;
	}

	start = buf;
	rule_hdr = (struct ipa_rt_rule_hw_hdr *)buf;
	pipe_idx = ipa_get_ep_mapping(ipa_ctx->mode,
			entry->rule.dst);
	if (pipe_idx == -1) {
		IPAERR("Wrong destination pipe specified in RT rule\n");
		WARN_ON(1);
		return -EPERM;
	}
	rule_hdr->u.hdr.pipe_dest_idx = pipe_idx;
	rule_hdr->u.hdr.system = !ipa_ctx->hdr_tbl_lcl;
	if (entry->hdr) {
		rule_hdr->u.hdr.hdr_offset =
			entry->hdr->offset_entry->offset >> 2;
	} else {
		rule_hdr->u.hdr.hdr_offset = 0;
	}
	buf += sizeof(struct ipa_rt_rule_hw_hdr);

	if (ipa_generate_hw_rule(ip, &rule->attrib, &buf, &en_rule)) {
		IPAERR("fail to generate hw rule\n");
		return -EPERM;
	}

	IPADBG("en_rule 0x%x\n", en_rule);

	rule_hdr->u.hdr.en_rule = en_rule;
	ipa_write_32(rule_hdr->u.word, (u8 *)rule_hdr);

	if (entry->hw_len == 0) {
		entry->hw_len = buf - start;
	} else if (entry->hw_len != (buf - start)) {
			IPAERR(
			"hw_len differs b/w passes passed=0x%x calc=0x%x\n",
			entry->hw_len,
			(buf - start));
			return -EPERM;
		}

	return 0;
}

/**
 * ipa_get_rt_hw_tbl_size() - returns the size of HW routing table
 * @ip: the ip address family type
 * @hdr_sz: header size
 * @max_rt_idx: maximal index
 *
 * Returns:	0 on success, negative on failure
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
			if (ipa_generate_rt_hw_rule(ip, entry, NULL)) {
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

/**
 * ipa_generate_rt_hw_tbl() - generates the routing hardware table
 * @ip:	[in] the ip address family type
 * @mem:	[out] buffer to put the filtering table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_generate_rt_hw_tbl(enum ipa_ip_type ip, struct ipa_mem_buffer *mem)
{
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_entry *entry;
	struct ipa_rt_tbl_set *set;
	u32 hdr_sz;
	u32 offset;
	u8 *hdr;
	u8 *body;
	u8 *base;
	struct ipa_mem_buffer rt_tbl_mem;
	u8 *rt_tbl_mem_body;
	int max_rt_idx;
	int i;

	mem->size = ipa_get_rt_hw_tbl_size(ip, &hdr_sz, &max_rt_idx);
	mem->size = (mem->size + IPA_RT_TABLE_MEMORY_ALLIGNMENT) &
				~IPA_RT_TABLE_MEMORY_ALLIGNMENT;

	if (mem->size == 0) {
		IPAERR("rt tbl empty ip=%d\n", ip);
		goto error;
	}
	mem->base = dma_alloc_coherent(NULL, mem->size, &mem->phys_base,
			GFP_KERNEL);
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

	set = &ipa_ctx->rt_tbl_set[ip];
	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		offset = body - base;
		if (offset & IPA_RT_ENTRY_MEMORY_ALLIGNMENT) {
			IPAERR("offset is not word multiple %d\n", offset);
			goto proc_err;
		}

		if (!tbl->in_sys) {
			/* convert offset to words from bytes */
			offset &= ~IPA_RT_ENTRY_MEMORY_ALLIGNMENT;
			/* rule is at an offset from base */
			offset |= IPA_RT_BIT_MASK;

			/* update the hdr at the right index */
			ipa_write_32(offset, hdr +
					(tbl->idx * IPA_RT_TABLE_WORD_SIZE));

			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_rt_rule_list,
					link) {
				if (ipa_generate_rt_hw_rule(ip, entry, body)) {
					IPAERR("failed to gen HW RT rule\n");
					goto proc_err;
				}
				body += entry->hw_len;
			}

			/* write the rule-set terminator */
			body = ipa_write_32(0, body);
			if ((u32)body & IPA_RT_ENTRY_MEMORY_ALLIGNMENT)
				/* advance body to next word boundary */
				body = body + (IPA_RT_TABLE_WORD_SIZE -
					      ((u32)body &
					      IPA_RT_ENTRY_MEMORY_ALLIGNMENT));
		} else {
			WARN_ON(tbl->sz == 0);
			/* allocate memory for the RT tbl */
			rt_tbl_mem.size = tbl->sz;
			rt_tbl_mem.base =
			   dma_alloc_coherent(NULL, rt_tbl_mem.size,
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
					hdr + (tbl->idx *
					IPA_RT_TABLE_WORD_SIZE));
			/* generate the rule-set */
			list_for_each_entry(entry, &tbl->head_rt_rule_list,
					link) {
				if (ipa_generate_rt_hw_rule(ip, entry,
							rt_tbl_mem_body)) {
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
	dma_free_coherent(NULL, rt_tbl_mem.size,
			  rt_tbl_mem.base, rt_tbl_mem.phys_base);
proc_err:
	dma_free_coherent(NULL, mem->size, mem->base, mem->phys_base);
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
			IPADBG("reaping rt tbl name=%s ip=%d\n", tbl->name, ip);
			dma_free_coherent(NULL, tbl->prev_mem.size,
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
			IPADBG("reaping sys rt tbl name=%s ip=%d\n", tbl->name,
					ip);
			dma_free_coherent(NULL, tbl->curr_mem.size,
					tbl->curr_mem.base,
					tbl->curr_mem.phys_base);
			kmem_cache_free(ipa_ctx->rt_tbl_cache, tbl);
		}
	}
}

static int __ipa_commit_rt(enum ipa_ip_type ip)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer *mem;
	void *cmd;
	struct ipa_ip_v4_routing_init *v4;
	struct ipa_ip_v6_routing_init *v6;
	u16 avail;
	u16 size;

	mem = kmalloc(sizeof(struct ipa_mem_buffer), GFP_KERNEL);
	if (!mem) {
		IPAERR("failed to alloc memory object\n");
		goto fail_alloc_mem;
	}

	if (ip == IPA_IP_v4) {
		avail = ipa_ctx->ip4_rt_tbl_lcl ? IPA_RAM_V4_RT_SIZE :
			IPA_RAM_V4_RT_SIZE_DDR;
		size = sizeof(struct ipa_ip_v4_routing_init);
	} else {
		avail = ipa_ctx->ip6_rt_tbl_lcl ? IPA_RAM_V6_RT_SIZE :
			IPA_RAM_V6_RT_SIZE_DDR;
		size = sizeof(struct ipa_ip_v6_routing_init);
	}
	cmd = kmalloc(size, GFP_KERNEL);
	if (!cmd) {
		IPAERR("failed to alloc immediate command object\n");
		goto fail_alloc_cmd;
	}

	if (ipa_generate_rt_hw_tbl(ip, mem)) {
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
		v4->ipv4_addr = ipa_ctx->ctrl->sram_rt_ipv4_ofst;
		IPADBG("putting Routing IPv4 rules to phys 0x%x",
				v4->ipv4_addr);
	} else {
		v6 = (struct ipa_ip_v6_routing_init *)cmd;
		desc.opcode = IPA_IP_V6_ROUTING_INIT;
		v6->ipv6_rules_addr = mem->phys_base;
		v6->size_ipv6_rules = mem->size;
		v6->ipv6_addr = ipa_ctx->ctrl->sram_rt_ipv6_ofst;
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
	dma_free_coherent(NULL, mem->size, mem->base, mem->phys_base);
	kfree(cmd);
	kfree(mem);

	return 0;

fail_send_cmd:
	if (mem->base)
		dma_free_coherent(NULL, mem->size, mem->base, mem->phys_base);
fail_hw_tbl_gen:
	kfree(cmd);
fail_alloc_cmd:
	kfree(mem);
fail_alloc_mem:
	return -EPERM;
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
		if (!strncmp(name, entry->name, IPA_RESOURCE_NAME_MAX))
			return entry;
	}

	return NULL;
}

static struct ipa_rt_tbl *__ipa_add_rt_tbl(enum ipa_ip_type ip,
		const char *name)
{
	struct ipa_rt_tbl *entry;
	struct ipa_rt_tbl_set *set;
	struct ipa_tree_node *node;
	int i;

	node = kmem_cache_zalloc(ipa_ctx->tree_node_cache, GFP_KERNEL);
	if (!node) {
		IPAERR("failed to alloc tree node object\n");
		goto node_alloc_fail;
	}

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
		entry->cookie = IPA_COOKIE;
		entry->in_sys = (ip == IPA_IP_v4) ?
			!ipa_ctx->ip4_rt_tbl_lcl : !ipa_ctx->ip6_rt_tbl_lcl;
		set->tbl_cnt++;
		list_add(&entry->link, &set->head_rt_tbl_list);

		IPADBG("add rt tbl idx=%d tbl_cnt=%d ip=%d\n", entry->idx,
				set->tbl_cnt, ip);

		node->hdl = (u32)entry;
		if (ipa_insert(&ipa_ctx->rt_tbl_hdl_tree, node)) {
			IPAERR("failed to add to tree\n");
			WARN_ON(1);
		}
	} else {
		kmem_cache_free(ipa_ctx->tree_node_cache, node);
	}

	return entry;

fail_rt_idx_alloc:
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->rt_tbl_cache, entry);
error:
	kmem_cache_free(ipa_ctx->tree_node_cache, node);
node_alloc_fail:
	return NULL;
}

static int __ipa_del_rt_tbl(struct ipa_rt_tbl *entry)
{
	struct ipa_tree_node *node;
	enum ipa_ip_type ip = IPA_IP_MAX;

	if (entry == NULL || (entry->cookie != IPA_COOKIE)) {
		IPAERR("bad parms\n");
		return -EINVAL;
	}
	node = ipa_search(&ipa_ctx->rt_tbl_hdl_tree, (u32)entry);
	if (node == NULL) {
		IPAERR("lookup failed\n");
		return -EPERM;
	}

	if (entry->set == &ipa_ctx->rt_tbl_set[IPA_IP_v4])
		ip = IPA_IP_v4;
	else if (entry->set == &ipa_ctx->rt_tbl_set[IPA_IP_v6])
		ip = IPA_IP_v6;
	else
		WARN_ON(1);

	if (!entry->in_sys) {
		list_del(&entry->link);
		clear_bit(entry->idx, &ipa_ctx->rt_idx_bitmap[ip]);
		entry->set->tbl_cnt--;
		IPADBG("del rt tbl_idx=%d tbl_cnt=%d\n", entry->idx,
				entry->set->tbl_cnt);
		kmem_cache_free(ipa_ctx->rt_tbl_cache, entry);
	} else {
		list_move(&entry->link,
				&ipa_ctx->reap_rt_tbl_set[ip].head_rt_tbl_list);
		clear_bit(entry->idx, &ipa_ctx->rt_idx_bitmap[ip]);
		entry->set->tbl_cnt--;
		IPADBG("del sys rt tbl_idx=%d tbl_cnt=%d\n", entry->idx,
				entry->set->tbl_cnt);
	}

	/* remove the handle from the database */
	rb_erase(&node->node, &ipa_ctx->rt_tbl_hdl_tree);
	kmem_cache_free(ipa_ctx->tree_node_cache, node);

	return 0;
}

static int __ipa_add_rt_rule(enum ipa_ip_type ip, const char *name,
		const struct ipa_rt_rule *rule, u8 at_rear, u32 *rule_hdl)
{
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_entry *entry;
	struct ipa_tree_node *node;

	if (rule->hdr_hdl &&
	    ((ipa_search(&ipa_ctx->hdr_hdl_tree, rule->hdr_hdl) == NULL) ||
	     ((struct ipa_hdr_entry *)rule->hdr_hdl)->cookie != IPA_COOKIE)) {
		IPAERR("rt rule does not point to valid hdr\n");
		goto error;
	}

	node = kmem_cache_zalloc(ipa_ctx->tree_node_cache, GFP_KERNEL);
	if (!node) {
		IPAERR("failed to alloc tree node object\n");
		goto error;
	}

	tbl = __ipa_add_rt_tbl(ip, name);
	if (tbl == NULL || (tbl->cookie != IPA_COOKIE)) {
		IPAERR("bad params\n");
		goto fail_rt_tbl_sanity;
	}
	/*
	 * do not allow any rules to be added at end of the "default" routing
	 * tables
	 */
	if (!strncmp(tbl->name, IPA_DFLT_RT_TBL_NAME, IPA_RESOURCE_NAME_MAX) &&
	    (tbl->rule_cnt > 0) && (at_rear != 0)) {
		IPAERR("cannot add rule at end of tbl rule_cnt=%d at_rear=%d\n",
		       tbl->rule_cnt, at_rear);
		goto fail_rt_tbl_sanity;
	}

	entry = kmem_cache_zalloc(ipa_ctx->rt_rule_cache, GFP_KERNEL);
	if (!entry) {
		IPAERR("failed to alloc RT rule object\n");
		goto fail_rt_tbl_sanity;
	}
	INIT_LIST_HEAD(&entry->link);
	entry->cookie = IPA_COOKIE;
	entry->rule = *rule;
	entry->tbl = tbl;
	entry->hdr = (struct ipa_hdr_entry *)rule->hdr_hdl;
	if (at_rear)
		list_add_tail(&entry->link, &tbl->head_rt_rule_list);
	else
		list_add(&entry->link, &tbl->head_rt_rule_list);
	tbl->rule_cnt++;
	if (entry->hdr)
		entry->hdr->ref_cnt++;
	IPADBG("add rt rule tbl_idx=%d rule_cnt=%d\n", tbl->idx, tbl->rule_cnt);
	*rule_hdl = (u32)entry;

	node->hdl = *rule_hdl;
	if (ipa_insert(&ipa_ctx->rt_rule_hdl_tree, node)) {
		IPAERR("failed to add to tree\n");
		WARN_ON(1);
		goto ipa_insert_failed;
	}

	return 0;

ipa_insert_failed:
	list_del(&entry->link);
	kmem_cache_free(ipa_ctx->rt_rule_cache, entry);
fail_rt_tbl_sanity:
	kmem_cache_free(ipa_ctx->tree_node_cache, node);
error:
	return -EPERM;
}

/**
 * ipa_add_rt_rule() - Add the specified routing rules to SW and optionally
 * commit to IPA HW
 * @rules:	[inout] set of routing rules to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_rt_rule(struct ipa_ioc_add_rt_rule *rules)
{
	int i;
	int ret;

	if (rules == NULL || rules->num_rules == 0 || rules->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
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
		if (__ipa_commit_rt(rules->ip)) {
			ret = -EPERM;
			goto bail;
		}

	ret = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(ipa_add_rt_rule);

int __ipa_del_rt_rule(u32 rule_hdl)
{
	struct ipa_rt_entry *entry = (struct ipa_rt_entry *)rule_hdl;
	struct ipa_tree_node *node;

	node = ipa_search(&ipa_ctx->rt_rule_hdl_tree, rule_hdl);
	if (node == NULL) {
		IPAERR("lookup failed\n");
		return -EINVAL;
	}

	if (entry == NULL || (entry->cookie != IPA_COOKIE)) {
		IPAERR("bad params\n");
		return -EINVAL;
	}

	if (entry->hdr)
		__ipa_release_hdr((u32)entry->hdr);
	list_del(&entry->link);
	entry->tbl->rule_cnt--;
	IPADBG("del rt rule tbl_idx=%d rule_cnt=%d\n", entry->tbl->idx,
			entry->tbl->rule_cnt);
	if (entry->tbl->rule_cnt == 0 && entry->tbl->ref_cnt == 0) {
		if (__ipa_del_rt_tbl(entry->tbl))
			IPAERR("fail to del RT tbl\n");
	}
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->rt_rule_cache, entry);

	/* remove the handle from the database */
	rb_erase(&node->node, &ipa_ctx->rt_rule_hdl_tree);
	kmem_cache_free(ipa_ctx->tree_node_cache, node);

	return 0;
}

/**
 * ipa_del_rt_rule() - Remove the specified routing rules to SW and optionally
 * commit to IPA HW
 * @hdls:	[inout] set of routing rules to delete
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls)
{
	int i;
	int ret;

	if (hdls == NULL || hdls->num_hdls == 0 || hdls->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa_del_rt_rule(hdls->hdl[i].hdl)) {
			IPAERR("failed to del rt rule %i\n", i);
			hdls->hdl[i].status = IPA_RT_STATUS_OF_DEL_FAILED;
		} else {
			hdls->hdl[i].status = 0;
		}
	}

	if (hdls->commit)
		if (__ipa_commit_rt(hdls->ip)) {
			ret = -EPERM;
			goto bail;
		}

	ret = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(ipa_del_rt_rule);

/**
 * ipa_commit_rt_rule() - Commit the current SW routing table of specified type
 * to IPA HW
 * @ip:	The family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_commit_rt(enum ipa_ip_type ip)
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
	if (ipa_commit_flt(ip))
		return -EPERM;

	mutex_lock(&ipa_ctx->lock);
	if (__ipa_commit_rt(ip)) {
		ret = -EPERM;
		goto bail;
	}

	ret = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(ipa_commit_rt);

/**
 * ipa_reset_rt() - reset the current SW routing table of specified type
 * (does not commit to HW)
 * @ip:	The family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_reset_rt(enum ipa_ip_type ip)
{
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_tbl *tbl_next;
	struct ipa_rt_tbl_set *set;
	struct ipa_rt_entry *rule;
	struct ipa_rt_entry *rule_next;
	struct ipa_tree_node *node;
	struct ipa_rt_tbl_set *rset;

	if (ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	/*
	 * issue a reset on the filtering module of same IP type since
	 * filtering rules point to routing tables
	 */
	if (ipa_reset_flt(ip))
		IPAERR("fail to reset flt ip=%d\n", ip);

	set = &ipa_ctx->rt_tbl_set[ip];
	rset = &ipa_ctx->reap_rt_tbl_set[ip];
	mutex_lock(&ipa_ctx->lock);
	IPADBG("reset rt ip=%d\n", ip);
	list_for_each_entry_safe(tbl, tbl_next, &set->head_rt_tbl_list, link) {
		list_for_each_entry_safe(rule, rule_next,
					 &tbl->head_rt_rule_list, link) {
			node = ipa_search(&ipa_ctx->rt_rule_hdl_tree,
					  (u32)rule);
			if (node == NULL)
				WARN_ON(1);

			/*
			 * for the "default" routing tbl, remove all but the
			 *  last rule
			 */
			if (tbl->idx == 0 && tbl->rule_cnt == 1)
				continue;

			list_del(&rule->link);
			tbl->rule_cnt--;
			if (rule->hdr)
				__ipa_release_hdr((u32)rule->hdr);
			rule->cookie = 0;
			kmem_cache_free(ipa_ctx->rt_rule_cache, rule);

			/* remove the handle from the database */
			rb_erase(&node->node, &ipa_ctx->rt_rule_hdl_tree);
			kmem_cache_free(ipa_ctx->tree_node_cache, node);
		}

		node = ipa_search(&ipa_ctx->rt_tbl_hdl_tree, (u32)tbl);
		if (node  == NULL)
			WARN_ON(1);

		/* do not remove the "default" routing tbl which has index 0 */
		if (tbl->idx != 0) {
			if (!tbl->in_sys) {
				list_del(&tbl->link);
				set->tbl_cnt--;
				clear_bit(tbl->idx,
					  &ipa_ctx->rt_idx_bitmap[ip]);
				IPADBG("rst rt tbl_idx=%d tbl_cnt=%d\n",
						tbl->idx, set->tbl_cnt);
				kmem_cache_free(ipa_ctx->rt_tbl_cache, tbl);
			} else {
				list_move(&tbl->link, &rset->head_rt_tbl_list);
				clear_bit(tbl->idx,
					  &ipa_ctx->rt_idx_bitmap[ip]);
				set->tbl_cnt--;
				IPADBG("rst sys rt tbl_idx=%d tbl_cnt=%d\n",
						tbl->idx, set->tbl_cnt);
			}
			/* remove the handle from the database */
			rb_erase(&node->node, &ipa_ctx->rt_tbl_hdl_tree);
			kmem_cache_free(ipa_ctx->tree_node_cache, node);
		}
	}
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(ipa_reset_rt);

/**
 * ipa_get_rt_tbl() - lookup the specified routing table and return handle if it
 * exists, if lookup succeeds the routing table ref cnt is increased
 * @lookup:	[inout] routing table to lookup and its handle
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 *	Caller should call ipa_put_rt_tbl later if this function succeeds
 */
int ipa_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup)
{
	struct ipa_rt_tbl *entry;
	int result = -EFAULT;

	if (lookup == NULL || lookup->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}
	mutex_lock(&ipa_ctx->lock);
	entry = __ipa_add_rt_tbl(lookup->ip, lookup->name);
	if (entry && entry->cookie == IPA_COOKIE) {
		entry->ref_cnt++;
		lookup->hdl = (uint32_t)entry;

		/* commit for get */
		if (__ipa_commit_rt(lookup->ip))
			IPAERR("fail to commit RT tbl\n");

		result = 0;
	}
	mutex_unlock(&ipa_ctx->lock);

	return result;
}
EXPORT_SYMBOL(ipa_get_rt_tbl);

/**
 * ipa_put_rt_tbl() - Release the specified routing table handle
 * @rt_tbl_hdl:	[in] the routing table handle to release
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_put_rt_tbl(u32 rt_tbl_hdl)
{
	struct ipa_rt_tbl *entry = (struct ipa_rt_tbl *)rt_tbl_hdl;
	struct ipa_tree_node *node;
	enum ipa_ip_type ip = IPA_IP_MAX;
	int result;

	mutex_lock(&ipa_ctx->lock);
	node = ipa_search(&ipa_ctx->rt_tbl_hdl_tree, rt_tbl_hdl);
	if (node == NULL) {
		IPAERR("lookup failed\n");
		result = -EINVAL;
		goto ret;
	}

	if (entry == NULL || (entry->cookie != IPA_COOKIE) ||
			entry->ref_cnt == 0) {
		IPAERR("bad parms\n");
		result = -EINVAL;
		goto ret;
	}

	if (entry->set == &ipa_ctx->rt_tbl_set[IPA_IP_v4])
		ip = IPA_IP_v4;
	else if (entry->set == &ipa_ctx->rt_tbl_set[IPA_IP_v6])
		ip = IPA_IP_v6;
	else
		WARN_ON(1);

	entry->ref_cnt--;
	if (entry->ref_cnt == 0 && entry->rule_cnt == 0) {
		if (__ipa_del_rt_tbl(entry))
			IPAERR("fail to del RT tbl\n");
		/* commit for put */
		if (__ipa_commit_rt(ip))
			IPAERR("fail to commit RT tbl\n");
	}

	result = 0;

ret:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}
EXPORT_SYMBOL(ipa_put_rt_tbl);

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

#define IPA_FLT_TABLE_WORD_SIZE			(4)
#define IPA_FLT_ENTRY_MEMORY_ALLIGNMENT		(0x3)
#define IPA_FLT_BIT_MASK			(0x1)
#define IPA_FLT_TABLE_INDEX_NOT_FOUND		(-1)
#define IPA_FLT_STATUS_OF_ADD_FAILED		(-1)
#define IPA_FLT_STATUS_OF_DEL_FAILED		(-1)
#define IPA_FLT_STATUS_OF_MDFY_FAILED		(-1)

static int ipa_generate_hw_rule_from_eq(
		const struct ipa_ipfltri_rule_eq *attrib, u8 **buf)
{
	int num_offset_meq_32 = attrib->num_offset_meq_32;
	int num_ihl_offset_range_16 = attrib->num_ihl_offset_range_16;
	int num_ihl_offset_meq_32 = attrib->num_ihl_offset_meq_32;
	int num_offset_meq_128 = attrib->num_offset_meq_128;
	int i;

	if (attrib->tos_eq_present) {
		*buf = ipa_write_8(attrib->tos_eq, *buf);
		*buf = ipa_pad_to_32(*buf);
	}

	if (attrib->protocol_eq_present) {
		*buf = ipa_write_8(attrib->protocol_eq, *buf);
		*buf = ipa_pad_to_32(*buf);
	}

	if (num_offset_meq_32) {
		*buf = ipa_write_8(attrib->offset_meq_32[0].offset, *buf);
		*buf = ipa_write_32(attrib->offset_meq_32[0].mask, *buf);
		*buf = ipa_write_32(attrib->offset_meq_32[0].value, *buf);
		*buf = ipa_pad_to_32(*buf);
		num_offset_meq_32--;
	}

	if (num_offset_meq_32) {
		*buf = ipa_write_8(attrib->offset_meq_32[1].offset, *buf);
		*buf = ipa_write_32(attrib->offset_meq_32[1].mask, *buf);
		*buf = ipa_write_32(attrib->offset_meq_32[1].value, *buf);
		*buf = ipa_pad_to_32(*buf);
		num_offset_meq_32--;
	}

	if (num_ihl_offset_range_16) {
		*buf = ipa_write_8(attrib->ihl_offset_range_16[0].offset, *buf);
		*buf = ipa_write_16(attrib->ihl_offset_range_16[0].range_high,
				*buf);
		*buf = ipa_write_16(attrib->ihl_offset_range_16[0].range_low,
				*buf);
		*buf = ipa_pad_to_32(*buf);
		num_ihl_offset_range_16--;
	}

	if (num_ihl_offset_range_16) {
		*buf = ipa_write_8(attrib->ihl_offset_range_16[1].offset, *buf);
		*buf = ipa_write_16(attrib->ihl_offset_range_16[1].range_high,
				*buf);
		*buf = ipa_write_16(attrib->ihl_offset_range_16[1].range_low,
				*buf);
		*buf = ipa_pad_to_32(*buf);
		num_ihl_offset_range_16--;
	}

	if (attrib->ihl_offset_eq_16_present) {
		*buf = ipa_write_8(attrib->ihl_offset_eq_16.offset, *buf);
		*buf = ipa_write_16(attrib->ihl_offset_eq_16.value, *buf);
		*buf = ipa_pad_to_32(*buf);
	}

	if (attrib->ihl_offset_eq_32_present) {
		*buf = ipa_write_8(attrib->ihl_offset_eq_32.offset, *buf);
		*buf = ipa_write_32(attrib->ihl_offset_eq_32.value, *buf);
		*buf = ipa_pad_to_32(*buf);
	}

	if (num_ihl_offset_meq_32) {
		*buf = ipa_write_8(attrib->ihl_offset_meq_32[0].offset, *buf);
		*buf = ipa_write_32(attrib->ihl_offset_meq_32[0].mask, *buf);
		*buf = ipa_write_32(attrib->ihl_offset_meq_32[0].value, *buf);
		*buf = ipa_pad_to_32(*buf);
		num_ihl_offset_meq_32--;
	}

	/* TODO check layout of 16 byte mask and value */
	if (num_offset_meq_128) {
		*buf = ipa_write_8(attrib->offset_meq_128[0].offset, *buf);
		for (i = 0; i < 16; i++)
			*buf = ipa_write_8(attrib->offset_meq_128[0].mask[i],
					*buf);
		for (i = 0; i < 16; i++)
			*buf = ipa_write_8(attrib->offset_meq_128[0].value[i],
					*buf);
		*buf = ipa_pad_to_32(*buf);
		num_offset_meq_128--;
	}

	if (num_offset_meq_128) {
		*buf = ipa_write_8(attrib->offset_meq_128[1].offset, *buf);
		for (i = 0; i < 16; i++)
			*buf = ipa_write_8(attrib->offset_meq_128[1].mask[i],
					*buf);
		for (i = 0; i < 16; i++)
			*buf = ipa_write_8(attrib->offset_meq_128[1].value[i],
					*buf);
		*buf = ipa_pad_to_32(*buf);
		num_offset_meq_128--;
	}

	if (attrib->tc_eq_present) {
		*buf = ipa_write_8(attrib->tc_eq, *buf);
		*buf = ipa_pad_to_32(*buf);
	}

	if (attrib->fl_eq_present) {
		*buf = ipa_write_32(attrib->fl_eq, *buf);
		*buf = ipa_pad_to_32(*buf);
	}

	if (num_ihl_offset_meq_32) {
		*buf = ipa_write_8(attrib->ihl_offset_meq_32[1].offset, *buf);
		*buf = ipa_write_32(attrib->ihl_offset_meq_32[1].mask, *buf);
		*buf = ipa_write_32(attrib->ihl_offset_meq_32[1].value, *buf);
		*buf = ipa_pad_to_32(*buf);
		num_ihl_offset_meq_32--;
	}

	if (attrib->metadata_meq32_present) {
		*buf = ipa_write_8(attrib->metadata_meq32.offset, *buf);
		*buf = ipa_write_32(attrib->metadata_meq32.mask, *buf);
		*buf = ipa_write_32(attrib->metadata_meq32.value, *buf);
		*buf = ipa_pad_to_32(*buf);
	}

	if (attrib->ipv4_frag_eq_present)
		*buf = ipa_pad_to_32(*buf);

	return 0;
}

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
		hdr->u.hdr.rt_tbl_idx = entry->rule.rt_tbl_idx;
	hdr->u.hdr.rsvd = 0;
	buf += sizeof(struct ipa_flt_rule_hw_hdr);

	if (rule->eq_attrib_type) {
		if (ipa_generate_hw_rule_from_eq(&rule->eq_attrib, &buf)) {
			IPAERR("fail to generate hw rule\n");
			return -EPERM;
		}
		en_rule = rule->eq_attrib.rule_eq_bitmap;
	} else {
		if (ipa_generate_hw_rule(ip, &rule->attrib, &buf, &en_rule)) {
			IPAERR("fail to generate hw rule\n");
			return -EPERM;
		}
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
		IPAERR("hw_len differs b/w passes passed=%x calc=%td\n",
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
 * Returns:	size on success, negative on failure
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

	for (i = 0; i < ipa_ctx->ipa_num_pipes; i++) {
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

static int ipa_generate_flt_hw_tbl_common(enum ipa_ip_type ip, u8 *base,
		u8 *hdr, u32 body_start_offset, u8 *hdr2, u32 *hdr_top)
{
	struct ipa_flt_tbl *tbl;
	struct ipa_flt_entry *entry;
	int i;
	u32 offset;
	u8 *body;
	struct ipa_mem_buffer flt_tbl_mem;
	u8 *ftbl_membody;

	*hdr_top = 0;
	body = base;

#define IPA_WRITE_FLT_HDR(idx, val) {			\
	if (idx <= 5) {					\
		*((u32 *)hdr + 1 + idx) = val;		\
	} else if (idx >= 6 && idx <= 10) {		\
		WARN_ON(1);				\
	} else if (idx >= 11 && idx <= 19) {		\
		*((u32 *)hdr2 + idx - 11) = val;	\
	} else {					\
		WARN_ON(1);				\
	}						\
}

	tbl = &ipa_ctx->glob_flt_tbl[ip];

	if (!list_empty(&tbl->head_flt_rule_list)) {
		*hdr_top |= IPA_FLT_BIT_MASK;

		if (!tbl->in_sys) {
			offset = body - base + body_start_offset;
			if (offset & IPA_FLT_ENTRY_MEMORY_ALLIGNMENT) {
				IPAERR("offset is not word multiple %d\n",
						offset);
				goto proc_err;
			}

			offset &= ~IPA_FLT_ENTRY_MEMORY_ALLIGNMENT;
			/* rule is at an offset from base */
			offset |= IPA_FLT_BIT_MASK;

			if (hdr2)
				*(u32 *)hdr = offset;
			else
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
			if ((long)body & IPA_FLT_ENTRY_MEMORY_ALLIGNMENT)
				/* advance body to next word boundary */
				body = body + (IPA_FLT_TABLE_WORD_SIZE -
					((long)body &
					IPA_FLT_ENTRY_MEMORY_ALLIGNMENT));
		} else {
			if (tbl->sz == 0) {
				IPAERR("tbl size is 0\n");
				WARN_ON(1);
				goto proc_err;
			}

			/* allocate memory for the flt tbl */
			flt_tbl_mem.size = tbl->sz;
			flt_tbl_mem.base =
			   dma_alloc_coherent(ipa_ctx->pdev, flt_tbl_mem.size,
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

			if (hdr2)
				*(u32 *)hdr = flt_tbl_mem.phys_base;
			else
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

	for (i = 0; i < ipa_ctx->ipa_num_pipes; i++) {
		tbl = &ipa_ctx->flt_tbl[i][ip];
		if (!list_empty(&tbl->head_flt_rule_list)) {
			/* pipe "i" is at bit "i+1" */
			*hdr_top |= (1 << (i + 1));

			if (!tbl->in_sys) {
				offset = body - base + body_start_offset;
				if (offset & IPA_FLT_ENTRY_MEMORY_ALLIGNMENT) {
					IPAERR("ofst is not word multiple %d\n",
					       offset);
					goto proc_err;
				}
				offset &= ~IPA_FLT_ENTRY_MEMORY_ALLIGNMENT;
				/* rule is at an offset from base */
				offset |= IPA_FLT_BIT_MASK;

				if (hdr2)
					IPA_WRITE_FLT_HDR(i, offset)
				else
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
				if ((long)body &
					IPA_FLT_ENTRY_MEMORY_ALLIGNMENT)
					/* advance body to next word boundary */
					body = body + (IPA_FLT_TABLE_WORD_SIZE -
						((long)body &
					IPA_FLT_ENTRY_MEMORY_ALLIGNMENT));
			} else {
				if (tbl->sz == 0) {
					IPAERR("tbl size is 0\n");
					WARN_ON(1);
					goto proc_err;
				}

				/* allocate memory for the flt tbl */
				flt_tbl_mem.size = tbl->sz;
				flt_tbl_mem.base =
				   dma_alloc_coherent(ipa_ctx->pdev,
						   flt_tbl_mem.size,
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

				if (hdr2)
					IPA_WRITE_FLT_HDR(i,
						flt_tbl_mem.phys_base)
				else
					hdr = ipa_write_32(
						flt_tbl_mem.phys_base, hdr);

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

	return 0;

proc_err:
	return -EPERM;
}


/**
 * ipa_generate_flt_hw_tbl() - generates the filtering hardware table
 * @ip:	[in] the ip address family type
 * @mem:	[out] buffer to put the filtering table
 *
 * Returns:	0 on success, negative on failure
 */
static int ipa_generate_flt_hw_tbl_v1_1(enum ipa_ip_type ip,
		struct ipa_mem_buffer *mem)
{
	u32 hdr_top = 0;
	u32 hdr_sz;
	u8 *hdr;
	u8 *body;
	u8 *base;
	int res;

	res = ipa_get_flt_hw_tbl_size(ip, &hdr_sz);
	if (res < 0) {
		IPAERR("ipa_get_flt_hw_tbl_size failed %d\n", res);
		return res;
	}

	mem->size = res;
	mem->size = IPA_HW_TABLE_ALIGNMENT(mem->size);

	if (mem->size == 0) {
		IPAERR("flt tbl empty ip=%d\n", ip);
		goto error;
	}
	mem->base = dma_alloc_coherent(ipa_ctx->pdev, mem->size,
			&mem->phys_base, GFP_KERNEL);
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

	if (ipa_generate_flt_hw_tbl_common(ip, body, hdr, hdr_sz, 0,
				&hdr_top)) {
		IPAERR("fail to generate FLT HW table\n");
		goto proc_err;
	}

	/* now write the hdr_top */
	ipa_write_32(hdr_top, base);

	IPA_DUMP_BUFF(mem->base, mem->phys_base, mem->size);

	return 0;

proc_err:
	dma_free_coherent(ipa_ctx->pdev, mem->size, mem->base, mem->phys_base);
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
		dma_free_coherent(ipa_ctx->pdev, tbl->prev_mem.size,
				tbl->prev_mem.base, tbl->prev_mem.phys_base);
		memset(&tbl->prev_mem, 0, sizeof(tbl->prev_mem));
	}

	if (list_empty(&tbl->head_flt_rule_list)) {
		if (tbl->curr_mem.phys_base) {
			IPADBG("reaping glob flt tbl (curr) ip=%d\n", ip);
			dma_free_coherent(ipa_ctx->pdev, tbl->curr_mem.size,
					tbl->curr_mem.base,
					tbl->curr_mem.phys_base);
			memset(&tbl->curr_mem, 0, sizeof(tbl->curr_mem));
		}
	}

	for (i = 0; i < ipa_ctx->ipa_num_pipes; i++) {
		tbl = &ipa_ctx->flt_tbl[i][ip];
		if (tbl->prev_mem.phys_base) {
			IPADBG("reaping flt tbl (prev) pipe=%d ip=%d\n", i, ip);
			dma_free_coherent(ipa_ctx->pdev, tbl->prev_mem.size,
					tbl->prev_mem.base,
					tbl->prev_mem.phys_base);
			memset(&tbl->prev_mem, 0, sizeof(tbl->prev_mem));
		}

		if (list_empty(&tbl->head_flt_rule_list)) {
			if (tbl->curr_mem.phys_base) {
				IPADBG("reaping flt tbl (curr) pipe=%d ip=%d\n",
						i, ip);
				dma_free_coherent(ipa_ctx->pdev,
						tbl->curr_mem.size,
						tbl->curr_mem.base,
						tbl->curr_mem.phys_base);
				memset(&tbl->curr_mem, 0,
						sizeof(tbl->curr_mem));
			}
		}
	}
}

int __ipa_commit_flt_v1_1(enum ipa_ip_type ip)
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
		avail = ipa_ctx->ip4_flt_tbl_lcl ? IPA_MEM_v1_RAM_V4_FLT_SIZE :
			IPA_MEM_PART(v4_flt_size_ddr);
		size = sizeof(struct ipa_ip_v4_filter_init);
	} else {
		avail = ipa_ctx->ip6_flt_tbl_lcl ? IPA_MEM_v1_RAM_V6_FLT_SIZE :
			IPA_MEM_PART(v6_flt_size_ddr);
		size = sizeof(struct ipa_ip_v6_filter_init);
	}
	cmd = kmalloc(size, GFP_KERNEL);
	if (!cmd) {
		IPAERR("failed to alloc immediate command object\n");
		goto fail_alloc_cmd;
	}

	if (ipa_generate_flt_hw_tbl_v1_1(ip, mem)) {
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
		v4->ipv4_addr = IPA_MEM_v1_RAM_V4_FLT_OFST;
	} else {
		v6 = (struct ipa_ip_v6_filter_init *)cmd;
		desc.opcode = IPA_IP_V6_FILTER_INIT;
		v6->ipv6_rules_addr = mem->phys_base;
		v6->size_ipv6_rules = mem->size;
		v6->ipv6_addr = IPA_MEM_v1_RAM_V6_FLT_OFST;
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
	dma_free_coherent(ipa_ctx->pdev, mem->size, mem->base, mem->phys_base);
	kfree(cmd);
	kfree(mem);

	return 0;

fail_send_cmd:
	if (mem->phys_base)
		dma_free_coherent(ipa_ctx->pdev, mem->size, mem->base,
				mem->phys_base);
fail_hw_tbl_gen:
	kfree(cmd);
fail_alloc_cmd:
	kfree(mem);
fail_alloc_mem:

	return -EPERM;
}

static int ipa_generate_flt_hw_tbl_v2(enum ipa_ip_type ip,
		struct ipa_mem_buffer *mem, struct ipa_mem_buffer *head1,
		struct ipa_mem_buffer *head2)
{
	int i;
	u32 hdr_sz;
	int num_words;
	u32 *entr;
	u32 body_start_offset;
	u32 hdr_top;
	int res;

	if (ip == IPA_IP_v4)
		body_start_offset = IPA_MEM_PART(apps_v4_flt_ofst) -
			IPA_MEM_PART(v4_flt_ofst);
	else
		body_start_offset = IPA_MEM_PART(apps_v6_flt_ofst) -
			IPA_MEM_PART(v6_flt_ofst);

	num_words = 7;
	head1->size = num_words * 4;
	head1->base = dma_alloc_coherent(ipa_ctx->pdev, head1->size,
			&head1->phys_base, GFP_KERNEL);
	if (!head1->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", head1->size);
		goto err;
	}
	entr = (u32 *)head1->base;
	for (i = 0; i < num_words; i++) {
		*entr = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entr++;
	}

	num_words = 9;
	head2->size = num_words * 4;
	head2->base = dma_alloc_coherent(ipa_ctx->pdev, head2->size,
			&head2->phys_base, GFP_KERNEL);
	if (!head2->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", head2->size);
		goto head_err;
	}
	entr = (u32 *)head2->base;
	for (i = 0; i < num_words; i++) {
		*entr = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entr++;
	}

	res = ipa_get_flt_hw_tbl_size(ip, &hdr_sz);
	if (res < 0) {
		IPAERR("ipa_get_flt_hw_tbl_size failed %d\n", res);
		goto body_err;
	}

	mem->size = res;
	mem->size -= hdr_sz;
	mem->size = IPA_HW_TABLE_ALIGNMENT(mem->size);

	if (mem->size) {
		mem->base = dma_alloc_coherent(ipa_ctx->pdev, mem->size,
				&mem->phys_base, GFP_KERNEL);
		if (!mem->base) {
			IPAERR("fail to alloc DMA buff of size %d\n",
					mem->size);
			goto body_err;
		}
		memset(mem->base, 0, mem->size);
	}

	if (ipa_generate_flt_hw_tbl_common(ip, mem->base, head1->base,
				body_start_offset, head2->base, &hdr_top)) {
		IPAERR("fail to generate FLT HW table\n");
		goto proc_err;
	}

	IPADBG("HEAD1\n");
	IPA_DUMP_BUFF(head1->base, head1->phys_base, head1->size);
	IPADBG("HEAD2\n");
	IPA_DUMP_BUFF(head2->base, head2->phys_base, head2->size);
	if (mem->size) {
		IPADBG("BODY\n");
		IPA_DUMP_BUFF(mem->base, mem->phys_base, mem->size);
	}

	return 0;

proc_err:
	if (mem->size)
		dma_free_coherent(ipa_ctx->pdev, mem->size, mem->base,
				mem->phys_base);
body_err:
	dma_free_coherent(ipa_ctx->pdev, head2->size, head2->base,
			head2->phys_base);
head_err:
	dma_free_coherent(ipa_ctx->pdev, head1->size, head1->base,
			head1->phys_base);
err:
	return -EPERM;
}

int __ipa_commit_flt_v2(enum ipa_ip_type ip)
{
	struct ipa_desc *desc;
	struct ipa_hw_imm_cmd_dma_shared_mem *cmd;
	struct ipa_mem_buffer body;
	struct ipa_mem_buffer head1;
	struct ipa_mem_buffer head2;
	int rc = 0;
	u32 local_addrb;
	u32 local_addrh;
	bool lcl;
	int num_desc = 0;
	int i;
	u16 avail;

	desc = kzalloc(16 * sizeof(*desc), GFP_ATOMIC);
	if (desc == NULL) {
		IPAERR("fail to alloc desc blob ip %d\n", ip);
		rc = -ENOMEM;
		goto fail_desc;
	}

	cmd = kzalloc(16 * sizeof(*cmd), GFP_ATOMIC);
	if (cmd == NULL) {
		IPAERR("fail to alloc cmd blob ip %d\n", ip);
		rc = -ENOMEM;
		goto fail_imm;
	}

	if (ip == IPA_IP_v4) {
		avail = ipa_ctx->ip4_flt_tbl_lcl ?
			IPA_MEM_PART(apps_v4_flt_size) :
			IPA_MEM_PART(v4_flt_size_ddr);
		local_addrh = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_flt_ofst) + 4;
		local_addrb = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v4_flt_ofst);
		lcl = ipa_ctx->ip4_flt_tbl_lcl;
	} else {
		avail = ipa_ctx->ip6_flt_tbl_lcl ?
			IPA_MEM_PART(apps_v6_flt_size) :
			IPA_MEM_PART(v6_flt_size_ddr);
		local_addrh = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_flt_ofst) + 4;
		local_addrb = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(apps_v6_flt_ofst);
		lcl = ipa_ctx->ip6_flt_tbl_lcl;
	}

	if (ipa_generate_flt_hw_tbl_v2(ip, &body, &head1, &head2)) {
		IPAERR("fail to generate FLT HW TBL ip %d\n", ip);
		rc = -EFAULT;
		goto fail_gen;
	}

	if (body.size > avail) {
		IPAERR("tbl too big, needed %d avail %d\n", body.size, avail);
		goto fail_send_cmd;
	}

	cmd[num_desc].size = 4;
	cmd[num_desc].system_addr = head1.phys_base;
	cmd[num_desc].local_addr = local_addrh;

	desc[num_desc].opcode = IPA_DMA_SHARED_MEM;
	desc[num_desc].pyld = &cmd[num_desc];
	desc[num_desc].len = sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
	desc[num_desc++].type = IPA_IMM_CMD_DESC;

	for (i = 0; i < 6; i++) {
		if (ipa_ctx->skip_ep_cfg_shadow[i]) {
			IPADBG("skip %d\n", i);
			continue;
		}

		if (ipa2_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS) == i ||
			ipa2_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS) == i ||
			ipa2_get_ep_mapping(IPA_CLIENT_APPS_CMD_PROD) == i ||
			(ipa2_get_ep_mapping(IPA_CLIENT_APPS_LAN_WAN_PROD) == i
			&& ipa_ctx->modem_cfg_emb_pipe_flt)) {
			IPADBG("skip %d\n", i);
			continue;
		}

		if (ip == IPA_IP_v4) {
			local_addrh = ipa_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v4_flt_ofst) +
				8 + i * 4;
		} else {
			local_addrh = ipa_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v6_flt_ofst) +
				8 + i * 4;
		}
		cmd[num_desc].size = 4;
		cmd[num_desc].system_addr = head1.phys_base + 4 + i * 4;
		cmd[num_desc].local_addr = local_addrh;

		desc[num_desc].opcode = IPA_DMA_SHARED_MEM;
		desc[num_desc].pyld = &cmd[num_desc];
		desc[num_desc].len =
			sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
		desc[num_desc++].type = IPA_IMM_CMD_DESC;
	}

	for (i = 11; i < ipa_ctx->ipa_num_pipes; i++) {
		if (ipa_ctx->skip_ep_cfg_shadow[i]) {
			IPADBG("skip %d\n", i);
			continue;
		}
		if (ipa2_get_ep_mapping(IPA_CLIENT_APPS_LAN_WAN_PROD) == i &&
			ipa_ctx->modem_cfg_emb_pipe_flt) {
			IPADBG("skip %d\n", i);
			continue;
		}
		if (ip == IPA_IP_v4) {
			local_addrh = ipa_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v4_flt_ofst) +
				13 * 4 + (i - 11) * 4;
		} else {
			local_addrh = ipa_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v6_flt_ofst) +
				13 * 4 + (i - 11) * 4;
		}
		cmd[num_desc].size = 4;
		cmd[num_desc].system_addr = head2.phys_base + (i - 11) * 4;
		cmd[num_desc].local_addr = local_addrh;

		desc[num_desc].opcode = IPA_DMA_SHARED_MEM;
		desc[num_desc].pyld = &cmd[num_desc];
		desc[num_desc].len =
			sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
		desc[num_desc++].type = IPA_IMM_CMD_DESC;
	}

	if (lcl) {
		cmd[num_desc].size = body.size;
		cmd[num_desc].system_addr = body.phys_base;
		cmd[num_desc].local_addr = local_addrb;

		desc[num_desc].opcode = IPA_DMA_SHARED_MEM;
		desc[num_desc].pyld = &cmd[num_desc];
		desc[num_desc].len =
			sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
		desc[num_desc++].type = IPA_IMM_CMD_DESC;

		if (ipa_send_cmd(num_desc, desc)) {
			IPAERR("fail to send immediate command\n");
			rc = -EFAULT;
			goto fail_send_cmd;
		}
	} else {
		if (ipa_send_cmd(num_desc, desc)) {
			IPAERR("fail to send immediate command\n");
			rc = -EFAULT;
			goto fail_send_cmd;
		}
	}

	__ipa_reap_sys_flt_tbls(ip);

fail_send_cmd:
	if (body.size)
		dma_free_coherent(ipa_ctx->pdev, body.size, body.base,
				body.phys_base);
	dma_free_coherent(ipa_ctx->pdev, head1.size, head1.base,
			head1.phys_base);
	dma_free_coherent(ipa_ctx->pdev, head2.size, head2.base,
			head2.phys_base);
fail_gen:
	kfree(cmd);
fail_imm:
	kfree(desc);
fail_desc:
	return rc;
}

static int __ipa_add_flt_rule(struct ipa_flt_tbl *tbl, enum ipa_ip_type ip,
			      const struct ipa_flt_rule *rule, u8 add_rear,
			      u32 *rule_hdl)
{
	struct ipa_flt_entry *entry;
	struct ipa_rt_tbl *rt_tbl = NULL;
	int id;

	if (rule->action != IPA_PASS_TO_EXCEPTION) {
		if (!rule->eq_attrib_type) {
			if (!rule->rt_tbl_hdl) {
				IPAERR("invalid RT tbl\n");
				goto error;
			}

			rt_tbl = ipa_id_find(rule->rt_tbl_hdl);
			if (rt_tbl == NULL) {
				IPAERR("RT tbl not found\n");
				goto error;
			}

			if (rt_tbl->cookie != IPA_COOKIE) {
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

	entry = kmem_cache_zalloc(ipa_ctx->flt_rule_cache, GFP_KERNEL);
	if (!entry) {
		IPAERR("failed to alloc FLT rule object\n");
		goto error;
	}
	INIT_LIST_HEAD(&entry->link);
	entry->rule = *rule;
	entry->cookie = IPA_COOKIE;
	entry->rt_tbl = rt_tbl;
	entry->tbl = tbl;
	if (add_rear) {
		if (tbl->sticky_rear)
			list_add_tail(&entry->link,
					tbl->head_flt_rule_list.prev);
		else
			list_add_tail(&entry->link, &tbl->head_flt_rule_list);
	} else {
		list_add(&entry->link, &tbl->head_flt_rule_list);
	}
	tbl->rule_cnt++;
	if (entry->rt_tbl)
		entry->rt_tbl->ref_cnt++;
	id = ipa_id_alloc(entry);
	if (id < 0) {
		IPAERR("failed to add to tree\n");
		WARN_ON(1);
	}
	*rule_hdl = id;
	entry->id = id;
	IPADBG("add flt rule rule_cnt=%d\n", tbl->rule_cnt);

	return 0;

error:
	return -EPERM;
}

static int __ipa_del_flt_rule(u32 rule_hdl)
{
	struct ipa_flt_entry *entry;
	int id;

	entry = ipa_id_find(rule_hdl);
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
	IPADBG("del flt rule rule_cnt=%d\n", entry->tbl->rule_cnt);
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->flt_rule_cache, entry);

	/* remove the handle from the database */
	ipa_id_remove(id);

	return 0;
}

static int __ipa_mdfy_flt_rule(struct ipa_flt_rule_mdfy *frule,
		enum ipa_ip_type ip)
{
	struct ipa_flt_entry *entry;
	struct ipa_rt_tbl *rt_tbl = NULL;

	entry = ipa_id_find(frule->rule_hdl);
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

			rt_tbl = ipa_id_find(frule->rule.rt_tbl_hdl);
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

	return 0;

error:
	return -EPERM;
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
	ipa_ep_idx = ipa2_get_ep_mapping(ep);
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
 * ipa2_add_flt_rule() - Add the specified filtering rules to SW and optionally
 * commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_add_flt_rule(struct ipa_ioc_add_flt_rule *rules)
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
		if (ipa_ctx->ctrl->ipa_commit_flt(rules->ip)) {
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}

/**
 * ipa2_del_flt_rule() - Remove the specified filtering rules from SW and
 * optionally commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls)
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
		if (ipa_ctx->ctrl->ipa_commit_flt(hdls->ip)) {
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}

/**
 * ipa2_mdfy_flt_rule() - Modify the specified filtering rules in SW and optionally
 * commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_mdfy_flt_rule(struct ipa_ioc_mdfy_flt_rule *hdls)
{
	int i;
	int result;

	if (hdls == NULL || hdls->num_rules == 0 || hdls->ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < hdls->num_rules; i++) {
		if (__ipa_mdfy_flt_rule(&hdls->rules[i], hdls->ip)) {
			IPAERR("failed to mdfy rt rule %i\n", i);
			hdls->rules[i].status = IPA_FLT_STATUS_OF_MDFY_FAILED;
		} else {
			hdls->rules[i].status = 0;
		}
	}

	if (hdls->commit)
		if (ipa_ctx->ctrl->ipa_commit_flt(hdls->ip)) {
			result = -EPERM;
			goto bail;
		}
	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}


/**
 * ipa2_commit_flt() - Commit the current SW filtering table of specified type to
 * IPA HW
 * @ip:	[in] the family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_commit_flt(enum ipa_ip_type ip)
{
	int result;

	if (ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);

	if (ipa_ctx->ctrl->ipa_commit_flt(ip)) {
		result = -EPERM;
		goto bail;
	}
	result = 0;

bail:
	mutex_unlock(&ipa_ctx->lock);

	return result;
}

/**
 * ipa2_reset_flt() - Reset the current SW filtering table of specified type
 * (does not commit to HW)
 * @ip:	[in] the family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_reset_flt(enum ipa_ip_type ip)
{
	struct ipa_flt_tbl *tbl;
	struct ipa_flt_entry *entry;
	struct ipa_flt_entry *next;
	int i;
	int id;

	if (ip >= IPA_IP_MAX) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	tbl = &ipa_ctx->glob_flt_tbl[ip];
	mutex_lock(&ipa_ctx->lock);
	IPADBG("reset flt ip=%d\n", ip);
	list_for_each_entry_safe(entry, next, &tbl->head_flt_rule_list, link) {
		if (ipa_id_find(entry->id) == NULL) {
			WARN_ON(1);
			mutex_unlock(&ipa_ctx->lock);
			return -EFAULT;
		}

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
		id = entry->id;
		kmem_cache_free(ipa_ctx->flt_rule_cache, entry);

		/* remove the handle from the database */
		ipa_id_remove(id);
	}

	for (i = 0; i < ipa_ctx->ipa_num_pipes; i++) {
		tbl = &ipa_ctx->flt_tbl[i][ip];
		list_for_each_entry_safe(entry, next, &tbl->head_flt_rule_list,
				link) {
			if (ipa_id_find(entry->id) == NULL) {
				WARN_ON(1);
				mutex_unlock(&ipa_ctx->lock);
				return -EFAULT;
			}
			list_del(&entry->link);
			entry->tbl->rule_cnt--;
			if (entry->rt_tbl)
				entry->rt_tbl->ref_cnt--;
			entry->cookie = 0;
			id = entry->id;
			kmem_cache_free(ipa_ctx->flt_rule_cache, entry);

			/* remove the handle from the database */
			ipa_id_remove(id);
		}
	}
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}

void ipa_install_dflt_flt_rules(u32 ipa_ep_idx)
{
	struct ipa_flt_tbl *tbl;
	struct ipa_ep_context *ep = &ipa_ctx->ep[ipa_ep_idx];
	struct ipa_flt_rule rule;

	memset(&rule, 0, sizeof(rule));

	mutex_lock(&ipa_ctx->lock);
	tbl = &ipa_ctx->flt_tbl[ipa_ep_idx][IPA_IP_v4];
	tbl->sticky_rear = true;
	rule.action = IPA_PASS_TO_EXCEPTION;
	__ipa_add_flt_rule(tbl, IPA_IP_v4, &rule, false,
			&ep->dflt_flt4_rule_hdl);
	ipa_ctx->ctrl->ipa_commit_flt(IPA_IP_v4);

	tbl = &ipa_ctx->flt_tbl[ipa_ep_idx][IPA_IP_v6];
	tbl->sticky_rear = true;
	rule.action = IPA_PASS_TO_EXCEPTION;
	__ipa_add_flt_rule(tbl, IPA_IP_v6, &rule, false,
			&ep->dflt_flt6_rule_hdl);
	ipa_ctx->ctrl->ipa_commit_flt(IPA_IP_v6);
	mutex_unlock(&ipa_ctx->lock);
}

void ipa_delete_dflt_flt_rules(u32 ipa_ep_idx)
{
	struct ipa_ep_context *ep = &ipa_ctx->ep[ipa_ep_idx];

	mutex_lock(&ipa_ctx->lock);
	if (ep->dflt_flt4_rule_hdl) {
		__ipa_del_flt_rule(ep->dflt_flt4_rule_hdl);
		ipa_ctx->ctrl->ipa_commit_flt(IPA_IP_v4);
		ep->dflt_flt4_rule_hdl = 0;
	}
	if (ep->dflt_flt6_rule_hdl) {
		__ipa_del_flt_rule(ep->dflt_flt6_rule_hdl);
		ipa_ctx->ctrl->ipa_commit_flt(IPA_IP_v6);
		ep->dflt_flt6_rule_hdl = 0;
	}
	mutex_unlock(&ipa_ctx->lock);
}

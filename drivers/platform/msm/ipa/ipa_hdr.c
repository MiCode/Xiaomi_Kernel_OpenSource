/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

static const u32 ipa_hdr_bin_sz[IPA_HDR_BIN_MAX] = { 8, 16, 24, 36, 60};

/**
 * ipa_generate_hdr_hw_tbl() - generates the headers table
 * @mem:	[out] buffer to put the header table
 *
 * Returns:	0 on success, negative on failure
 */
static int ipa_generate_hdr_hw_tbl(struct ipa_mem_buffer *mem)
{
	struct ipa_hdr_entry *entry;

	mem->size = ipa_ctx->hdr_tbl.end;

	if (mem->size == 0) {
		IPAERR("hdr tbl empty\n");
		return -EPERM;
	}
	IPADBG("tbl_sz=%d\n", ipa_ctx->hdr_tbl.end);

	mem->base = dma_alloc_coherent(ipa_ctx->pdev, mem->size,
			&mem->phys_base, GFP_KERNEL);
	if (!mem->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem->size);
		return -ENOMEM;
	}

	memset(mem->base, 0, mem->size);
	list_for_each_entry(entry, &ipa_ctx->hdr_tbl.head_hdr_entry_list,
			link) {
		IPADBG("hdr of len %d ofst=%d\n", entry->hdr_len,
				entry->offset_entry->offset);
		memcpy(mem->base + entry->offset_entry->offset, entry->hdr,
				entry->hdr_len);
	}

	return 0;
}

/*
 * __ipa_commit_hdr() commits hdr to hardware
 * This function needs to be called with a locked mutex.
 */
int __ipa_commit_hdr_v1(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer *mem;
	struct ipa_hdr_init_local *cmd;
	u16 len;

	mem = kmalloc(sizeof(struct ipa_mem_buffer), GFP_KERNEL);
	if (!mem) {
		IPAERR("failed to alloc memory object\n");
		goto fail_alloc_mem;
	}

	/* the immediate command param size is same for both local and system */
	len = sizeof(struct ipa_hdr_init_local);

	/*
	 * we can use init_local ptr for init_system due to layout of the
	 * struct
	 */
	cmd = kmalloc(len, GFP_KERNEL);
	if (!cmd) {
		IPAERR("failed to alloc immediate command object\n");
		goto fail_alloc_cmd;
	}

	if (ipa_generate_hdr_hw_tbl(mem)) {
		IPAERR("fail to generate HDR HW TBL\n");
		goto fail_hw_tbl_gen;
	}

	if (ipa_ctx->hdr_tbl_lcl) {
		if (mem->size > IPA_v1_RAM_HDR_SIZE) {
			IPAERR("tbl too big, needed %d avail %d\n", mem->size,
				IPA_v1_RAM_HDR_SIZE);
			goto fail_send_cmd;
		}
	} else {
		if (mem->size > IPA_RAM_HDR_SIZE_DDR) {
			IPAERR("tbl too big, needed %d avail %d\n", mem->size,
				IPA_RAM_HDR_SIZE_DDR);
			goto fail_send_cmd;
		}
	}

	cmd->hdr_table_src_addr = mem->phys_base;
	if (ipa_ctx->hdr_tbl_lcl) {
		cmd->size_hdr_table = mem->size;
		cmd->hdr_table_dst_addr = IPA_v1_RAM_HDR_OFST;
		desc.opcode = IPA_HDR_INIT_LOCAL;
	} else {
		desc.opcode = IPA_HDR_INIT_SYSTEM;
	}
	desc.pyld = cmd;
	desc.len = sizeof(struct ipa_hdr_init_local);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem->base, mem->phys_base, mem->size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		goto fail_send_cmd;
	}

	if (ipa_ctx->hdr_tbl_lcl) {
		dma_free_coherent(ipa_ctx->pdev, mem->size, mem->base,
				mem->phys_base);
	} else {
		if (ipa_ctx->hdr_mem.phys_base) {
			dma_free_coherent(ipa_ctx->pdev, ipa_ctx->hdr_mem.size,
					  ipa_ctx->hdr_mem.base,
					  ipa_ctx->hdr_mem.phys_base);
		}
		ipa_ctx->hdr_mem = *mem;
	}
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

int __ipa_commit_hdr_v2(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_hdr_init_system cmd;
	struct ipa_hw_imm_cmd_dma_shared_mem dma_cmd;
	int rc = -EFAULT;

	if (ipa_generate_hdr_hw_tbl(&mem)) {
		IPAERR("fail to generate HDR HW TBL\n");
		goto end;
	}

	if (ipa_ctx->hdr_tbl_lcl) {
		if (mem.size > IPA_v2_RAM_APPS_HDR_SIZE) {
			IPAERR("tbl too big, needed %d avail %d\n", mem.size,
				IPA_v2_RAM_APPS_HDR_SIZE);
			goto end;
		} else {
			dma_cmd.system_addr = mem.phys_base;
			dma_cmd.size = mem.size;
			dma_cmd.local_addr = ipa_ctx->smem_restricted_bytes +
				IPA_v2_RAM_APPS_HDR_OFST;
			desc.opcode = IPA_DMA_SHARED_MEM;
			desc.pyld = &dma_cmd;
			desc.len =
				sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
		}
	} else {
		if (mem.size > IPA_RAM_HDR_SIZE_DDR) {
			IPAERR("tbl too big, needed %d avail %d\n", mem.size,
				IPA_RAM_HDR_SIZE_DDR);
			goto end;
		} else {
			cmd.hdr_table_addr = mem.phys_base;
			desc.opcode = IPA_HDR_INIT_SYSTEM;
			desc.pyld = &cmd;
			desc.len = sizeof(struct ipa_hdr_init_system);
		}
	}

	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc))
		IPAERR("fail to send immediate command\n");
	else
		rc = 0;

	if (ipa_ctx->hdr_tbl_lcl) {
		dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base,
				mem.phys_base);
	} else {
		if (!rc) {
			if (ipa_ctx->hdr_mem.phys_base)
				dma_free_coherent(ipa_ctx->pdev,
						ipa_ctx->hdr_mem.size,
						ipa_ctx->hdr_mem.base,
						ipa_ctx->hdr_mem.phys_base);
			ipa_ctx->hdr_mem = mem;
		}
	}

end:
	return rc;
}

static int __ipa_add_hdr(struct ipa_hdr_add *hdr)
{
	struct ipa_hdr_entry *entry;
	struct ipa_hdr_offset_entry *offset;
	u32 bin;
	struct ipa_hdr_tbl *htbl = &ipa_ctx->hdr_tbl;
	int id;

	if (hdr->hdr_len == 0) {
		IPAERR("bad parm\n");
		goto error;
	}

	entry = kmem_cache_zalloc(ipa_ctx->hdr_cache, GFP_KERNEL);
	if (!entry) {
		IPAERR("failed to alloc hdr object\n");
		goto error;
	}

	INIT_LIST_HEAD(&entry->link);

	memcpy(entry->hdr, hdr->hdr, hdr->hdr_len);
	entry->hdr_len = hdr->hdr_len;
	strlcpy(entry->name, hdr->name, IPA_RESOURCE_NAME_MAX);
	entry->is_partial = hdr->is_partial;
	entry->is_eth2_ofst_valid = hdr->is_eth2_ofst_valid;
	entry->eth2_ofst = hdr->eth2_ofst;
	entry->cookie = IPA_COOKIE;

	if (hdr->hdr_len <= ipa_hdr_bin_sz[IPA_HDR_BIN0])
		bin = IPA_HDR_BIN0;
	else if (hdr->hdr_len <= ipa_hdr_bin_sz[IPA_HDR_BIN1])
		bin = IPA_HDR_BIN1;
	else if (hdr->hdr_len <= ipa_hdr_bin_sz[IPA_HDR_BIN2])
		bin = IPA_HDR_BIN2;
	else if (hdr->hdr_len <= ipa_hdr_bin_sz[IPA_HDR_BIN3])
		bin = IPA_HDR_BIN3;
	else if (hdr->hdr_len <= ipa_hdr_bin_sz[IPA_HDR_BIN4])
		bin = IPA_HDR_BIN4;
	else {
		IPAERR("unexpected hdr len %d\n", hdr->hdr_len);
		goto bad_hdr_len;
	}

	if (list_empty(&htbl->head_free_offset_list[bin])) {
		offset = kmem_cache_zalloc(ipa_ctx->hdr_offset_cache,
					   GFP_KERNEL);
		if (!offset) {
			IPAERR("failed to alloc hdr offset object\n");
			goto bad_hdr_len;
		}
		INIT_LIST_HEAD(&offset->link);
		/*
		 * for a first item grow, set the bin and offset which are set
		 * in stone
		 */
		offset->offset = htbl->end;
		offset->bin = bin;
		htbl->end += ipa_hdr_bin_sz[bin];
		list_add(&offset->link,
				&htbl->head_offset_list[bin]);
	} else {
		/* get the first free slot */
		offset =
		    list_first_entry(&htbl->head_free_offset_list[bin],
				     struct ipa_hdr_offset_entry, link);
		list_move(&offset->link, &htbl->head_offset_list[bin]);
	}

	entry->offset_entry = offset;
	list_add(&entry->link, &htbl->head_hdr_entry_list);
	htbl->hdr_cnt++;
	IPADBG("add hdr of sz=%d hdr_cnt=%d ofst=%d\n", hdr->hdr_len,
			htbl->hdr_cnt, offset->offset);

	id = ipa_id_alloc(entry);
	if (id < 0) {
		IPAERR("failed to alloc id\n");
		WARN_ON(1);
	}
	entry->id = id;
	hdr->hdr_hdl = id;
	entry->ref_cnt++;

	return 0;

bad_hdr_len:
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->hdr_cache, entry);
error:
	return -EPERM;
}

int __ipa_del_hdr(u32 hdr_hdl)
{
	struct ipa_hdr_entry *entry;
	struct ipa_hdr_tbl *htbl = &ipa_ctx->hdr_tbl;

	entry = ipa_id_find(hdr_hdl);
	if (entry == NULL) {
		IPAERR("lookup failed\n");
		return -EINVAL;
	}

	if (!entry || (entry->cookie != IPA_COOKIE)) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	IPADBG("del hdr of sz=%d hdr_cnt=%d ofst=%d\n", entry->hdr_len,
			htbl->hdr_cnt, entry->offset_entry->offset);

	if (--entry->ref_cnt) {
		IPADBG("hdr_hdl %x ref_cnt %d\n", hdr_hdl, entry->ref_cnt);
		return 0;
	}

	/* move the offset entry to appropriate free list */
	list_move(&entry->offset_entry->link,
		  &htbl->head_free_offset_list[entry->offset_entry->bin]);
	list_del(&entry->link);
	htbl->hdr_cnt--;
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->hdr_cache, entry);

	/* remove the handle from the database */
	ipa_id_remove(hdr_hdl);

	return 0;
}

/**
 * ipa_add_hdr() - add the specified headers to SW and optionally commit them to
 * IPA HW
 * @hdrs:	[inout] set of headers to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_hdr(struct ipa_ioc_add_hdr *hdrs)
{
	int i;
	int result = -EFAULT;

	if (hdrs == NULL || hdrs->num_hdrs == 0) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	IPADBG("adding %d headers to IPA driver internal data struct\n",
			hdrs->num_hdrs);
	for (i = 0; i < hdrs->num_hdrs; i++) {
		if (__ipa_add_hdr(&hdrs->hdr[i])) {
			IPAERR("failed to add hdr %d\n", i);
			hdrs->hdr[i].status = -1;
		} else {
			hdrs->hdr[i].status = 0;
		}
	}

	if (hdrs->commit) {
		IPADBG("committing all headers to IPA core");
		if (ipa_ctx->ctrl->ipa_commit_hdr()) {
			result = -EPERM;
			goto bail;
		}
	}
	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return result;
}
EXPORT_SYMBOL(ipa_add_hdr);

/**
 * ipa_del_hdr() - Remove the specified headers from SW and optionally commit them
 * to IPA HW
 * @hdls:	[inout] set of headers to delete
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_del_hdr(struct ipa_ioc_del_hdr *hdls)
{
	int i;
	int result = -EFAULT;

	if (hdls == NULL || hdls->num_hdls == 0) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa_del_hdr(hdls->hdl[i].hdl)) {
			IPAERR("failed to del hdr %i\n", i);
			hdls->hdl[i].status = -1;
		} else {
			hdls->hdl[i].status = 0;
		}
	}

	if (hdls->commit) {
		if (ipa_ctx->ctrl->ipa_commit_hdr()) {
			result = -EPERM;
			goto bail;
		}
	}
	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return result;
}
EXPORT_SYMBOL(ipa_del_hdr);

/**
 * ipa_commit_hdr() - commit to IPA HW the current header table in SW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_commit_hdr(void)
{
	int result = -EFAULT;

	/*
	 * issue a commit on the routing module since routing rules point to
	 * header table entries
	 */
	if (ipa_commit_rt(IPA_IP_v4))
		return -EPERM;
	if (ipa_commit_rt(IPA_IP_v6))
		return -EPERM;

	mutex_lock(&ipa_ctx->lock);
	if (ipa_ctx->ctrl->ipa_commit_hdr()) {
		result = -EPERM;
		goto bail;
	}
	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return result;
}
EXPORT_SYMBOL(ipa_commit_hdr);

/**
 * ipa_reset_hdr() - reset the current header table in SW (does not commit to
 * HW)
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_reset_hdr(void)
{
	struct ipa_hdr_entry *entry;
	struct ipa_hdr_entry *next;
	struct ipa_hdr_offset_entry *off_entry;
	struct ipa_hdr_offset_entry *off_next;
	int i;

	/*
	 * issue a reset on the routing module since routing rules point to
	 * header table entries
	 */
	if (ipa_reset_rt(IPA_IP_v4))
		IPAERR("fail to reset v4 rt\n");
	if (ipa_reset_rt(IPA_IP_v6))
		IPAERR("fail to reset v4 rt\n");

	mutex_lock(&ipa_ctx->lock);
	IPADBG("reset hdr\n");
	list_for_each_entry_safe(entry, next,
			&ipa_ctx->hdr_tbl.head_hdr_entry_list, link) {

		/* do not remove the default header */
		if (!strcmp(entry->name, IPA_LAN_RX_HDR_NAME))
			continue;

		if (ipa_id_find(entry->id) == NULL) {
			WARN_ON(1);
			mutex_unlock(&ipa_ctx->lock);
			return -EFAULT;
		}
		list_del(&entry->link);
		entry->cookie = 0;
		kmem_cache_free(ipa_ctx->hdr_cache, entry);

		/* remove the handle from the database */
		ipa_id_remove(entry->id);

	}
	for (i = 0; i < IPA_HDR_BIN_MAX; i++) {
		list_for_each_entry_safe(off_entry, off_next,
					 &ipa_ctx->hdr_tbl.head_offset_list[i],
					 link) {

			/*
			 * do not remove the default exception header which is
			 * at offset 0
			 */
			if (off_entry->offset == 0)
				continue;

			list_del(&off_entry->link);
			kmem_cache_free(ipa_ctx->hdr_offset_cache, off_entry);
		}
		list_for_each_entry_safe(off_entry, off_next,
				&ipa_ctx->hdr_tbl.head_free_offset_list[i],
				link) {
			list_del(&off_entry->link);
			kmem_cache_free(ipa_ctx->hdr_offset_cache, off_entry);
		}
	}
	/* there is one header of size 8 */
	ipa_ctx->hdr_tbl.end = 8;
	ipa_ctx->hdr_tbl.hdr_cnt = 1;
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}
EXPORT_SYMBOL(ipa_reset_hdr);

static struct ipa_hdr_entry *__ipa_find_hdr(const char *name)
{
	struct ipa_hdr_entry *entry;

	list_for_each_entry(entry, &ipa_ctx->hdr_tbl.head_hdr_entry_list,
			link) {
		if (!strncmp(name, entry->name, IPA_RESOURCE_NAME_MAX))
			return entry;
	}

	return NULL;
}

/**
 * ipa_get_hdr() - Lookup the specified header resource
 * @lookup:	[inout] header to lookup and its handle
 *
 * lookup the specified header resource and return handle if it exists
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 *		Caller should call ipa_put_hdr later if this function succeeds
 */
int ipa_get_hdr(struct ipa_ioc_get_hdr *lookup)
{
	struct ipa_hdr_entry *entry;
	int result = -1;

	if (lookup == NULL) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}
	mutex_lock(&ipa_ctx->lock);
	entry = __ipa_find_hdr(lookup->name);
	if (entry) {
		lookup->hdl = entry->id;
		result = 0;
	}
	mutex_unlock(&ipa_ctx->lock);

	return result;
}
EXPORT_SYMBOL(ipa_get_hdr);

/**
 * __ipa_release_hdr() - drop reference to header and cause
 * deletion if reference count permits
 * @hdr_hdl:	[in] handle of header to be released
 *
 * Returns:	0 on success, negative on failure
 */
int __ipa_release_hdr(u32 hdr_hdl)
{
	int result = 0;

	if (__ipa_del_hdr(hdr_hdl)) {
		IPADBG("fail to del hdr %x\n", hdr_hdl);
		result = -EFAULT;
		goto bail;
	}

	/* commit for put */
	if (ipa_ctx->ctrl->ipa_commit_hdr()) {
		IPAERR("fail to commit hdr\n");
		result = -EFAULT;
		goto bail;
	}

bail:
	return result;
}

/**
 * ipa_put_hdr() - Release the specified header handle
 * @hdr_hdl:	[in] the header handle to release
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_put_hdr(u32 hdr_hdl)
{
	struct ipa_hdr_entry *entry;
	int result = -EFAULT;

	mutex_lock(&ipa_ctx->lock);

	entry = ipa_id_find(hdr_hdl);
	if (entry == NULL) {
		IPAERR("lookup failed\n");
		result = -EINVAL;
		goto bail;
	}

	if (entry == NULL || entry->cookie != IPA_COOKIE) {
		IPAERR("bad params\n");
		result = -EINVAL;
		goto bail;
	}

	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return result;
}
EXPORT_SYMBOL(ipa_put_hdr);

/**
 * ipa_copy_hdr() - Lookup the specified header resource and return a copy of it
 * @copy:	[inout] header to lookup and its copy
 *
 * lookup the specified header resource and return a copy of it (along with its
 * attributes) if it exists, this would be called for partial headers
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_copy_hdr(struct ipa_ioc_copy_hdr *copy)
{
	struct ipa_hdr_entry *entry;
	int result = -EFAULT;

	if (copy == NULL) {
		IPAERR("bad parm\n");
		return -EINVAL;
	}
	mutex_lock(&ipa_ctx->lock);
	entry = __ipa_find_hdr(copy->name);
	if (entry) {
		memcpy(copy->hdr, entry->hdr, entry->hdr_len);
		copy->hdr_len = entry->hdr_len;
		copy->is_partial = entry->is_partial;
		copy->is_eth2_ofst_valid = entry->is_eth2_ofst_valid;
		copy->eth2_ofst = entry->eth2_ofst;
		result = 0;
	}
	mutex_unlock(&ipa_ctx->lock);

	return result;
}
EXPORT_SYMBOL(ipa_copy_hdr);



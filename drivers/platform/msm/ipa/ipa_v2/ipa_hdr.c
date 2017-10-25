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

static const u32 ipa_hdr_bin_sz[IPA_HDR_BIN_MAX] = { 8, 16, 24, 36, 60};
static const u32 ipa_hdr_proc_ctx_bin_sz[IPA_HDR_PROC_CTX_BIN_MAX] = { 32, 64};

#define HDR_TYPE_IS_VALID(type) \
	((type) >= 0 && (type) < IPA_HDR_L2_MAX)

#define HDR_PROC_TYPE_IS_VALID(type) \
	((type) >= 0 && (type) < IPA_HDR_PROC_MAX)

/* uCP command numbers */
#define IPA_HDR_UCP_802_3_TO_802_3 6
#define IPA_HDR_UCP_802_3_TO_ETHII 7
#define IPA_HDR_UCP_ETHII_TO_802_3 8
#define IPA_HDR_UCP_ETHII_TO_ETHII 9

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
		if (entry->is_hdr_proc_ctx)
			continue;
		IPADBG("hdr of len %d ofst=%d\n", entry->hdr_len,
				entry->offset_entry->offset);
		memcpy(mem->base + entry->offset_entry->offset, entry->hdr,
				entry->hdr_len);
	}

	return 0;
}

static void ipa_hdr_proc_ctx_to_hw_format(struct ipa_mem_buffer *mem,
	u32 hdr_base_addr)
{
	struct ipa_hdr_proc_ctx_entry *entry;

	list_for_each_entry(entry,
			&ipa_ctx->hdr_proc_ctx_tbl.head_proc_ctx_entry_list,
			link) {
		IPADBG("processing type %d ofst=%d\n",
			entry->type, entry->offset_entry->offset);
		if (entry->type == IPA_HDR_PROC_NONE) {
			struct ipa_hdr_proc_ctx_add_hdr_seq *ctx;

			ctx = (struct ipa_hdr_proc_ctx_add_hdr_seq *)
				(mem->base + entry->offset_entry->offset);
			ctx->hdr_add.tlv.type = IPA_PROC_CTX_TLV_TYPE_HDR_ADD;
			ctx->hdr_add.tlv.length = 1;
			ctx->hdr_add.tlv.value = entry->hdr->hdr_len;
			ctx->hdr_add.hdr_addr = (entry->hdr->is_hdr_proc_ctx) ?
				entry->hdr->phys_base :
				hdr_base_addr +
				entry->hdr->offset_entry->offset;
			IPADBG("header address 0x%x\n",
				ctx->hdr_add.hdr_addr);
			ctx->end.type = IPA_PROC_CTX_TLV_TYPE_END;
			ctx->end.length = 0;
			ctx->end.value = 0;
		} else {
			struct ipa_hdr_proc_ctx_add_hdr_cmd_seq *ctx;

			ctx = (struct ipa_hdr_proc_ctx_add_hdr_cmd_seq *)
				(mem->base + entry->offset_entry->offset);
			ctx->hdr_add.tlv.type = IPA_PROC_CTX_TLV_TYPE_HDR_ADD;
			ctx->hdr_add.tlv.length = 1;
			ctx->hdr_add.tlv.value = entry->hdr->hdr_len;
			ctx->hdr_add.hdr_addr = (entry->hdr->is_hdr_proc_ctx) ?
				entry->hdr->phys_base :
				hdr_base_addr +
				entry->hdr->offset_entry->offset;
			IPADBG("header address 0x%x\n",
				ctx->hdr_add.hdr_addr);
			ctx->cmd.type = IPA_PROC_CTX_TLV_TYPE_PROC_CMD;
			ctx->cmd.length = 0;
			if (entry->type == IPA_HDR_PROC_ETHII_TO_ETHII)
				ctx->cmd.value = IPA_HDR_UCP_ETHII_TO_ETHII;
			else if (entry->type == IPA_HDR_PROC_ETHII_TO_802_3)
				ctx->cmd.value = IPA_HDR_UCP_ETHII_TO_802_3;
			else if (entry->type == IPA_HDR_PROC_802_3_TO_ETHII)
				ctx->cmd.value = IPA_HDR_UCP_802_3_TO_ETHII;
			else if (entry->type == IPA_HDR_PROC_802_3_TO_802_3)
				ctx->cmd.value = IPA_HDR_UCP_802_3_TO_802_3;
			IPADBG("command id %d\n", ctx->cmd.value);
			ctx->end.type = IPA_PROC_CTX_TLV_TYPE_END;
			ctx->end.length = 0;
			ctx->end.value = 0;
		}
	}
}

/**
 * ipa_generate_hdr_proc_ctx_hw_tbl() -
 * generates the headers processing context table.
 * @mem:		[out] buffer to put the processing context table
 * @aligned_mem:	[out] actual processing context table (with alignment).
 *			Processing context table needs to be 8 Bytes aligned.
 *
 * Returns:	0 on success, negative on failure
 */
static int ipa_generate_hdr_proc_ctx_hw_tbl(u32 hdr_sys_addr,
	struct ipa_mem_buffer *mem, struct ipa_mem_buffer *aligned_mem)
{
	u32 hdr_base_addr;

	mem->size = (ipa_ctx->hdr_proc_ctx_tbl.end) ? : 4;

	/* make sure table is aligned */
	mem->size += IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE;

	IPADBG("tbl_sz=%d\n", ipa_ctx->hdr_proc_ctx_tbl.end);

	mem->base = dma_alloc_coherent(ipa_ctx->pdev, mem->size,
			&mem->phys_base, GFP_KERNEL);
	if (!mem->base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem->size);
		return -ENOMEM;
	}

	aligned_mem->phys_base =
		IPA_HDR_PROC_CTX_TABLE_ALIGNMENT(mem->phys_base);
	aligned_mem->base = mem->base +
		(aligned_mem->phys_base - mem->phys_base);
	aligned_mem->size = mem->size - IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE;
	memset(aligned_mem->base, 0, aligned_mem->size);
	hdr_base_addr = (ipa_ctx->hdr_tbl_lcl) ? IPA_MEM_PART(apps_hdr_ofst) :
		hdr_sys_addr;
	ipa_hdr_proc_ctx_to_hw_format(aligned_mem, hdr_base_addr);

	return 0;
}

/*
 * __ipa_commit_hdr() commits hdr to hardware
 * This function needs to be called with a locked mutex.
 */
int __ipa_commit_hdr_v1_1(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer *mem;
	struct ipa_hdr_init_local *cmd;
	u16 len;
	gfp_t flag = GFP_KERNEL | (ipa_ctx->use_dma_zone ? GFP_DMA : 0);

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
	cmd = kmalloc(len, flag);
	if (!cmd) {
		IPAERR("failed to alloc immediate command object\n");
		goto fail_alloc_cmd;
	}

	if (ipa_generate_hdr_hw_tbl(mem)) {
		IPAERR("fail to generate HDR HW TBL\n");
		goto fail_hw_tbl_gen;
	}

	if (ipa_ctx->hdr_tbl_lcl) {
		if (mem->size > IPA_MEM_v1_RAM_HDR_SIZE) {
			IPAERR("tbl too big, needed %d avail %d\n", mem->size,
				IPA_MEM_v1_RAM_HDR_SIZE);
			goto fail_send_cmd;
		}
	} else {
		if (mem->size > IPA_MEM_PART(apps_hdr_size_ddr)) {
			IPAERR("tbl too big, needed %d avail %d\n", mem->size,
				IPA_MEM_PART(apps_hdr_size_ddr));
			goto fail_send_cmd;
		}
	}

	cmd->hdr_table_src_addr = mem->phys_base;
	if (ipa_ctx->hdr_tbl_lcl) {
		cmd->size_hdr_table = mem->size;
		cmd->hdr_table_dst_addr = IPA_MEM_v1_RAM_HDR_OFST;
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
	struct ipa_hdr_init_system *cmd = NULL;
	struct ipa_hw_imm_cmd_dma_shared_mem *dma_cmd = NULL;
	gfp_t flag = GFP_ATOMIC | (ipa_ctx->use_dma_zone ? GFP_DMA : 0);
	int rc = -EFAULT;

	if (ipa_generate_hdr_hw_tbl(&mem)) {
		IPAERR("fail to generate HDR HW TBL\n");
		goto end;
	}

	if (ipa_ctx->hdr_tbl_lcl) {
		if (mem.size > IPA_MEM_PART(apps_hdr_size)) {
			IPAERR("tbl too big, needed %d avail %d\n", mem.size,
				IPA_MEM_PART(apps_hdr_size));
			goto fail_send_cmd;
		} else {
			dma_cmd = kzalloc(sizeof(*dma_cmd), flag);
			if (dma_cmd == NULL) {
				IPAERR("fail to alloc immediate cmd\n");
				rc = -ENOMEM;
				goto fail_send_cmd;
			}

			dma_cmd->system_addr = mem.phys_base;
			dma_cmd->size = mem.size;
			dma_cmd->local_addr = ipa_ctx->smem_restricted_bytes +
				IPA_MEM_PART(apps_hdr_ofst);
			desc.opcode = IPA_DMA_SHARED_MEM;
			desc.pyld = (void *)dma_cmd;
			desc.len =
				sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
		}
	} else {
		if (mem.size > IPA_MEM_PART(apps_hdr_size_ddr)) {
			IPAERR("tbl too big, needed %d avail %d\n", mem.size,
				IPA_MEM_PART(apps_hdr_size_ddr));
			goto fail_send_cmd;
		} else {
			cmd = kzalloc(sizeof(*cmd), flag);
			if (cmd == NULL) {
				IPAERR("fail to alloc hdr init cmd\n");
				rc = -ENOMEM;
				goto fail_send_cmd;
			}
			cmd->hdr_table_addr = mem.phys_base;
			desc.opcode = IPA_HDR_INIT_SYSTEM;
			desc.pyld = (void *)cmd;
			desc.len = sizeof(struct ipa_hdr_init_system);
		}
	}

	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc))
		IPAERR("fail to send immediate command\n");
	else
		rc = 0;

	kfree(dma_cmd);
	kfree(cmd);

fail_send_cmd:
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
		} else {
			dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base,
					mem.phys_base);
		}
	}

end:
	return rc;
}

int __ipa_commit_hdr_v2_5(void)
{
	struct ipa_desc desc[2];
	struct ipa_mem_buffer hdr_mem;
	struct ipa_mem_buffer ctx_mem;
	struct ipa_mem_buffer aligned_ctx_mem;
	struct ipa_hdr_init_system *hdr_init_cmd = NULL;
	struct ipa_hw_imm_cmd_dma_shared_mem *dma_cmd_hdr = NULL;
	struct ipa_hw_imm_cmd_dma_shared_mem *dma_cmd_ctx = NULL;
	struct ipa_register_write *reg_write_cmd = NULL;
	gfp_t flag = GFP_ATOMIC | (ipa_ctx->use_dma_zone ? GFP_DMA : 0);
	int rc = -EFAULT;
	u32 proc_ctx_size;
	u32 proc_ctx_ofst;
	u32 proc_ctx_size_ddr;

	memset(desc, 0, 2 * sizeof(struct ipa_desc));

	if (ipa_generate_hdr_hw_tbl(&hdr_mem)) {
		IPAERR("fail to generate HDR HW TBL\n");
		goto end;
	}

	if (ipa_generate_hdr_proc_ctx_hw_tbl(hdr_mem.phys_base, &ctx_mem,
	    &aligned_ctx_mem)) {
		IPAERR("fail to generate HDR PROC CTX HW TBL\n");
		goto end;
	}

	if (ipa_ctx->hdr_tbl_lcl) {
		if (hdr_mem.size > IPA_MEM_PART(apps_hdr_size)) {
			IPAERR("tbl too big needed %d avail %d\n", hdr_mem.size,
				IPA_MEM_PART(apps_hdr_size));
			goto fail_send_cmd1;
		} else {
			dma_cmd_hdr = kzalloc(sizeof(*dma_cmd_hdr), flag);
			if (dma_cmd_hdr == NULL) {
				IPAERR("fail to alloc immediate cmd\n");
				rc = -ENOMEM;
				goto fail_send_cmd1;
			}
			dma_cmd_hdr->system_addr = hdr_mem.phys_base;
			dma_cmd_hdr->size = hdr_mem.size;
			dma_cmd_hdr->local_addr =
				ipa_ctx->smem_restricted_bytes +
				IPA_MEM_PART(apps_hdr_ofst);
			desc[0].opcode = IPA_DMA_SHARED_MEM;
			desc[0].pyld = (void *)dma_cmd_hdr;
			desc[0].len =
				sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
		}
	} else {
		if (hdr_mem.size > IPA_MEM_PART(apps_hdr_size_ddr)) {
			IPAERR("tbl too big needed %d avail %d\n", hdr_mem.size,
				IPA_MEM_PART(apps_hdr_size_ddr));
			goto fail_send_cmd1;
		} else {
			hdr_init_cmd = kzalloc(sizeof(*hdr_init_cmd),
				flag);
			if (hdr_init_cmd == NULL) {
				IPAERR("fail to alloc immediate cmd\n");
				rc = -ENOMEM;
				goto fail_send_cmd1;
			}
			hdr_init_cmd->hdr_table_addr = hdr_mem.phys_base;
			desc[0].opcode = IPA_HDR_INIT_SYSTEM;
			desc[0].pyld = (void *)hdr_init_cmd;
			desc[0].len = sizeof(struct ipa_hdr_init_system);
		}
	}
	desc[0].type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(hdr_mem.base, hdr_mem.phys_base, hdr_mem.size);

	proc_ctx_size = IPA_MEM_PART(apps_hdr_proc_ctx_size);
	proc_ctx_ofst = IPA_MEM_PART(apps_hdr_proc_ctx_ofst);
	if (ipa_ctx->hdr_proc_ctx_tbl_lcl) {
		if (aligned_ctx_mem.size > proc_ctx_size) {
			IPAERR("tbl too big needed %d avail %d\n",
				aligned_ctx_mem.size,
				proc_ctx_size);
			goto fail_send_cmd1;
		} else {
			dma_cmd_ctx = kzalloc(sizeof(*dma_cmd_ctx),
				flag);
			if (dma_cmd_ctx == NULL) {
				IPAERR("fail to alloc immediate cmd\n");
				rc = -ENOMEM;
				goto fail_send_cmd1;
			}
			dma_cmd_ctx->system_addr = aligned_ctx_mem.phys_base;
			dma_cmd_ctx->size = aligned_ctx_mem.size;
			dma_cmd_ctx->local_addr =
				ipa_ctx->smem_restricted_bytes +
				proc_ctx_ofst;
			desc[1].opcode = IPA_DMA_SHARED_MEM;
			desc[1].pyld = (void *)dma_cmd_ctx;
			desc[1].len =
				sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
		}
	} else {
		proc_ctx_size_ddr = IPA_MEM_PART(apps_hdr_proc_ctx_size_ddr);
		if (aligned_ctx_mem.size > proc_ctx_size_ddr) {
			IPAERR("tbl too big, needed %d avail %d\n",
				aligned_ctx_mem.size,
				proc_ctx_size_ddr);
			goto fail_send_cmd1;
		} else {
			reg_write_cmd = kzalloc(sizeof(*reg_write_cmd),
				flag);
			if (reg_write_cmd == NULL) {
				IPAERR("fail to alloc immediate cmd\n");
				rc = -ENOMEM;
				goto fail_send_cmd1;
			}
			reg_write_cmd->offset =
				IPA_SYS_PKT_PROC_CNTXT_BASE_OFST;
			reg_write_cmd->value = aligned_ctx_mem.phys_base;
			reg_write_cmd->value_mask =
				~(IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE - 1);
			desc[1].pyld = (void *)reg_write_cmd;
			desc[1].opcode = IPA_REGISTER_WRITE;
			desc[1].len = sizeof(*reg_write_cmd);
		}
	}
	desc[1].type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(ctx_mem.base, ctx_mem.phys_base, ctx_mem.size);

	if (ipa_send_cmd(2, desc))
		IPAERR("fail to send immediate command\n");
	else
		rc = 0;

fail_send_cmd1:

	kfree(dma_cmd_hdr);
	kfree(hdr_init_cmd);
	kfree(dma_cmd_ctx);
	kfree(reg_write_cmd);

	if (ipa_ctx->hdr_proc_ctx_tbl_lcl) {
		dma_free_coherent(ipa_ctx->pdev, ctx_mem.size,
			ctx_mem.base,
			ctx_mem.phys_base);
	} else {
		if (!rc) {
			if (ipa_ctx->hdr_proc_ctx_mem.phys_base)
				dma_free_coherent(ipa_ctx->pdev,
					ipa_ctx->hdr_proc_ctx_mem.size,
					ipa_ctx->hdr_proc_ctx_mem.base,
					ipa_ctx->hdr_proc_ctx_mem.phys_base);
			ipa_ctx->hdr_proc_ctx_mem = ctx_mem;
		} else {
			dma_free_coherent(ipa_ctx->pdev, ctx_mem.size,
				ctx_mem.base,
				ctx_mem.phys_base);
		}
	}

	if (ipa_ctx->hdr_tbl_lcl) {
		dma_free_coherent(ipa_ctx->pdev, hdr_mem.size,
			hdr_mem.base,
			hdr_mem.phys_base);
	} else {
		if (!rc) {
			if (ipa_ctx->hdr_mem.phys_base)
				dma_free_coherent(ipa_ctx->pdev,
				ipa_ctx->hdr_mem.size,
				ipa_ctx->hdr_mem.base,
				ipa_ctx->hdr_mem.phys_base);
			ipa_ctx->hdr_mem = hdr_mem;
		} else {
			dma_free_coherent(ipa_ctx->pdev, hdr_mem.size,
				hdr_mem.base,
				hdr_mem.phys_base);
		}
	}
end:
	return rc;
}

/**
 * __ipa_commit_hdr_v2_6L() - Commits a header to the IPA HW.
 *
 * This function needs to be called with a locked mutex.
 */
int __ipa_commit_hdr_v2_6L(void)
{
	/* Same implementation as IPAv2 */
	return __ipa_commit_hdr_v2();
}

static int __ipa_add_hdr_proc_ctx(struct ipa_hdr_proc_ctx_add *proc_ctx,
	bool add_ref_hdr)
{
	struct ipa_hdr_entry *hdr_entry;
	struct ipa_hdr_proc_ctx_entry *entry;
	struct ipa_hdr_proc_ctx_offset_entry *offset = NULL;
	u32 bin;
	struct ipa_hdr_proc_ctx_tbl *htbl = &ipa_ctx->hdr_proc_ctx_tbl;
	int id;
	int needed_len;
	int mem_size;

	IPADBG("processing type %d hdr_hdl %d\n",
		proc_ctx->type, proc_ctx->hdr_hdl);

	if (!HDR_PROC_TYPE_IS_VALID(proc_ctx->type)) {
		IPAERR_RL("invalid processing type %d\n", proc_ctx->type);
		return -EINVAL;
	}

	hdr_entry = ipa_id_find(proc_ctx->hdr_hdl);
	if (!hdr_entry || (hdr_entry->cookie != IPA_HDR_COOKIE)) {
		IPAERR_RL("hdr_hdl is invalid\n");
		return -EINVAL;
	}

	entry = kmem_cache_zalloc(ipa_ctx->hdr_proc_ctx_cache, GFP_KERNEL);
	if (!entry) {
		IPAERR("failed to alloc proc_ctx object\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&entry->link);

	entry->type = proc_ctx->type;
	entry->hdr = hdr_entry;
	if (add_ref_hdr)
		hdr_entry->ref_cnt++;
	entry->cookie = IPA_PROC_HDR_COOKIE;

	needed_len = (proc_ctx->type == IPA_HDR_PROC_NONE) ?
			sizeof(struct ipa_hdr_proc_ctx_add_hdr_seq) :
			sizeof(struct ipa_hdr_proc_ctx_add_hdr_cmd_seq);

	if (needed_len <= ipa_hdr_proc_ctx_bin_sz[IPA_HDR_PROC_CTX_BIN0]) {
		bin = IPA_HDR_PROC_CTX_BIN0;
	} else if (needed_len <=
			ipa_hdr_proc_ctx_bin_sz[IPA_HDR_PROC_CTX_BIN1]) {
		bin = IPA_HDR_PROC_CTX_BIN1;
	} else {
		IPAERR_RL("unexpected needed len %d\n", needed_len);
		WARN_ON(1);
		goto bad_len;
	}

	mem_size = (ipa_ctx->hdr_proc_ctx_tbl_lcl) ?
		IPA_MEM_PART(apps_hdr_proc_ctx_size) :
		IPA_MEM_PART(apps_hdr_proc_ctx_size_ddr);
	if (list_empty(&htbl->head_free_offset_list[bin])) {
		if (htbl->end + ipa_hdr_proc_ctx_bin_sz[bin] > mem_size) {
			IPAERR_RL("hdr proc ctx table overflow\n");
			goto bad_len;
		}

		offset = kmem_cache_zalloc(ipa_ctx->hdr_proc_ctx_offset_cache,
					   GFP_KERNEL);
		if (!offset) {
			IPAERR("failed to alloc offset object\n");
			goto bad_len;
		}
		INIT_LIST_HEAD(&offset->link);
		/*
		 * for a first item grow, set the bin and offset which are set
		 * in stone
		 */
		offset->offset = htbl->end;
		offset->bin = bin;
		htbl->end += ipa_hdr_proc_ctx_bin_sz[bin];
		list_add(&offset->link,
				&htbl->head_offset_list[bin]);
	} else {
		/* get the first free slot */
		offset =
		    list_first_entry(&htbl->head_free_offset_list[bin],
				    struct ipa_hdr_proc_ctx_offset_entry, link);
		list_move(&offset->link, &htbl->head_offset_list[bin]);
	}

	entry->offset_entry = offset;
	list_add(&entry->link, &htbl->head_proc_ctx_entry_list);
	htbl->proc_ctx_cnt++;
	IPADBG("add proc ctx of sz=%d cnt=%d ofst=%d\n", needed_len,
			htbl->proc_ctx_cnt, offset->offset);

	id = ipa_id_alloc(entry);
	if (id < 0) {
		IPAERR("failed to alloc id\n");
		WARN_ON(1);
		goto ipa_insert_failed;
	}
	entry->id = id;
	proc_ctx->proc_ctx_hdl = id;
	entry->ref_cnt++;

	return 0;

ipa_insert_failed:
	list_move(&offset->link,
		&htbl->head_free_offset_list[offset->bin]);
	entry->offset_entry = NULL;
	list_del(&entry->link);
	htbl->proc_ctx_cnt--;

bad_len:
	if (add_ref_hdr)
		hdr_entry->ref_cnt--;
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->hdr_proc_ctx_cache, entry);
	return -EPERM;
}


static int __ipa_add_hdr(struct ipa_hdr_add *hdr)
{
	struct ipa_hdr_entry *entry;
	struct ipa_hdr_offset_entry *offset = NULL;
	u32 bin;
	struct ipa_hdr_tbl *htbl = &ipa_ctx->hdr_tbl;
	int id;
	int mem_size;
	gfp_t flag = GFP_KERNEL | (ipa_ctx->use_dma_zone ? GFP_DMA : 0);

	if (hdr->hdr_len == 0 || hdr->hdr_len > IPA_HDR_MAX_SIZE) {
		IPAERR_RL("bad parm\n");
		goto error;
	}

	if (!HDR_TYPE_IS_VALID(hdr->type)) {
		IPAERR_RL("invalid hdr type %d\n", hdr->type);
		goto error;
	}

	entry = kmem_cache_zalloc(ipa_ctx->hdr_cache, flag);
	if (!entry) {
		IPAERR("failed to alloc hdr object\n");
		goto error;
	}

	INIT_LIST_HEAD(&entry->link);

	memcpy(entry->hdr, hdr->hdr, hdr->hdr_len);
	entry->hdr_len = hdr->hdr_len;
	strlcpy(entry->name, hdr->name, IPA_RESOURCE_NAME_MAX);
	entry->is_partial = hdr->is_partial;
	entry->type = hdr->type;
	entry->is_eth2_ofst_valid = hdr->is_eth2_ofst_valid;
	entry->eth2_ofst = hdr->eth2_ofst;
	entry->cookie = IPA_HDR_COOKIE;

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
		IPAERR_RL("unexpected hdr len %d\n", hdr->hdr_len);
		goto bad_hdr_len;
	}

	mem_size = (ipa_ctx->hdr_tbl_lcl) ? IPA_MEM_PART(apps_hdr_size) :
		IPA_MEM_PART(apps_hdr_size_ddr);

	if (list_empty(&htbl->head_free_offset_list[bin])) {
		/*
		 * if header does not fit to table, place it in DDR
		 * This is valid for IPA 2.5 and on,
		 * with the exception of IPA2.6L.
		 */
		if (htbl->end + ipa_hdr_bin_sz[bin] > mem_size) {
			if (ipa_ctx->ipa_hw_type != IPA_HW_v2_5) {
				IPAERR("not enough room for header\n");
				goto bad_hdr_len;
			} else {
				entry->is_hdr_proc_ctx = true;
				entry->phys_base = dma_map_single(ipa_ctx->pdev,
					entry->hdr,
					entry->hdr_len,
					DMA_TO_DEVICE);
				if (dma_mapping_error(ipa_ctx->pdev,
					entry->phys_base)) {
					IPAERR("dma_map_single failureed\n");
					goto fail_dma_mapping;
				}
			}
		} else {
			entry->is_hdr_proc_ctx = false;
			offset = kmem_cache_zalloc(ipa_ctx->hdr_offset_cache,
						   GFP_KERNEL);
			if (!offset) {
				IPAERR("failed to alloc hdr offset object\n");
				goto bad_hdr_len;
			}
			INIT_LIST_HEAD(&offset->link);
			/*
			 * for a first item grow, set the bin and offset which
			 * are set in stone
			 */
			offset->offset = htbl->end;
			offset->bin = bin;
			htbl->end += ipa_hdr_bin_sz[bin];
			list_add(&offset->link,
					&htbl->head_offset_list[bin]);
			entry->offset_entry = offset;
		}
	} else {
		entry->is_hdr_proc_ctx = false;
		/* get the first free slot */
		offset =
		list_first_entry(&htbl->head_free_offset_list[bin],
				struct ipa_hdr_offset_entry, link);
		list_move(&offset->link, &htbl->head_offset_list[bin]);
		entry->offset_entry = offset;
	}

	list_add(&entry->link, &htbl->head_hdr_entry_list);
	htbl->hdr_cnt++;
	if (entry->is_hdr_proc_ctx)
		IPADBG("add hdr of sz=%d hdr_cnt=%d phys_base=%pa\n",
			hdr->hdr_len,
			htbl->hdr_cnt,
			&entry->phys_base);
	else
		IPADBG("add hdr of sz=%d hdr_cnt=%d ofst=%d\n",
			hdr->hdr_len,
			htbl->hdr_cnt,
			entry->offset_entry->offset);

	id = ipa_id_alloc(entry);
	if (id < 0) {
		IPAERR("failed to alloc id\n");
		WARN_ON(1);
		goto ipa_insert_failed;
	}
	entry->id = id;
	hdr->hdr_hdl = id;
	entry->ref_cnt++;

	if (entry->is_hdr_proc_ctx) {
		struct ipa_hdr_proc_ctx_add proc_ctx;

		IPADBG("adding processing context for header %s\n", hdr->name);
		proc_ctx.type = IPA_HDR_PROC_NONE;
		proc_ctx.hdr_hdl = id;
		if (__ipa_add_hdr_proc_ctx(&proc_ctx, false)) {
			IPAERR("failed to add hdr proc ctx\n");
			goto fail_add_proc_ctx;
		}
		entry->proc_ctx = ipa_id_find(proc_ctx.proc_ctx_hdl);
	}

	return 0;

fail_add_proc_ctx:
	entry->ref_cnt--;
	hdr->hdr_hdl = 0;
	ipa_id_remove(id);
ipa_insert_failed:
	if (entry->is_hdr_proc_ctx) {
		dma_unmap_single(ipa_ctx->pdev, entry->phys_base,
			entry->hdr_len, DMA_TO_DEVICE);
	} else {
		if (offset)
			list_move(&offset->link,
			&htbl->head_free_offset_list[offset->bin]);
		entry->offset_entry = NULL;
	}
	htbl->hdr_cnt--;
	list_del(&entry->link);

fail_dma_mapping:
	entry->is_hdr_proc_ctx = false;
bad_hdr_len:
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->hdr_cache, entry);
error:
	return -EPERM;
}

static int __ipa_del_hdr_proc_ctx(u32 proc_ctx_hdl,
	bool release_hdr, bool by_user)
{
	struct ipa_hdr_proc_ctx_entry *entry;
	struct ipa_hdr_proc_ctx_tbl *htbl = &ipa_ctx->hdr_proc_ctx_tbl;

	entry = ipa_id_find(proc_ctx_hdl);
	if (!entry || (entry->cookie != IPA_PROC_HDR_COOKIE)) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	IPADBG("del ctx proc cnt=%d ofst=%d\n",
		htbl->proc_ctx_cnt, entry->offset_entry->offset);

	if (by_user && entry->user_deleted) {
		IPAERR_RL("proc_ctx already deleted by user\n");
		return -EINVAL;
	}

	if (by_user)
		entry->user_deleted = true;

	if (--entry->ref_cnt) {
		IPADBG("proc_ctx_hdl %x ref_cnt %d\n",
			proc_ctx_hdl, entry->ref_cnt);
		return 0;
	}

	if (release_hdr)
		__ipa_del_hdr(entry->hdr->id, false);

	/* move the offset entry to appropriate free list */
	list_move(&entry->offset_entry->link,
		&htbl->head_free_offset_list[entry->offset_entry->bin]);
	list_del(&entry->link);
	htbl->proc_ctx_cnt--;
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->hdr_proc_ctx_cache, entry);

	/* remove the handle from the database */
	ipa_id_remove(proc_ctx_hdl);

	return 0;
}


int __ipa_del_hdr(u32 hdr_hdl, bool by_user)
{
	struct ipa_hdr_entry *entry;
	struct ipa_hdr_tbl *htbl = &ipa_ctx->hdr_tbl;

	entry = ipa_id_find(hdr_hdl);
	if (entry == NULL) {
		IPAERR_RL("lookup failed\n");
		return -EINVAL;
	}

	if (entry->cookie != IPA_HDR_COOKIE) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	if (entry->is_hdr_proc_ctx)
		IPADBG("del hdr of sz=%d hdr_cnt=%d phys_base=%pa\n",
			entry->hdr_len, htbl->hdr_cnt, &entry->phys_base);
	else
		IPADBG("del hdr of sz=%d hdr_cnt=%d ofst=%d\n", entry->hdr_len,
			htbl->hdr_cnt, entry->offset_entry->offset);

	if (by_user && entry->user_deleted) {
		IPAERR_RL("hdr already deleted by user\n");
		return -EINVAL;
	}

	if (by_user)
		entry->user_deleted = true;

	if (--entry->ref_cnt) {
		IPADBG("hdr_hdl %x ref_cnt %d\n", hdr_hdl, entry->ref_cnt);
		return 0;
	}

	if (entry->is_hdr_proc_ctx) {
		dma_unmap_single(ipa_ctx->pdev,
			entry->phys_base,
			entry->hdr_len,
			DMA_TO_DEVICE);
		__ipa_del_hdr_proc_ctx(entry->proc_ctx->id, false, false);
	} else {
		/* move the offset entry to appropriate free list */
		list_move(&entry->offset_entry->link,
			&htbl->head_free_offset_list[entry->offset_entry->bin]);
	}
	list_del(&entry->link);
	htbl->hdr_cnt--;
	entry->cookie = 0;
	kmem_cache_free(ipa_ctx->hdr_cache, entry);

	/* remove the handle from the database */
	ipa_id_remove(hdr_hdl);

	return 0;
}

/**
 * ipa2_add_hdr() - add the specified headers to SW and optionally commit them
 * to IPA HW
 * @hdrs:	[inout] set of headers to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_add_hdr(struct ipa_ioc_add_hdr *hdrs)
{
	int i;
	int result = -EFAULT;

	if (unlikely(!ipa_ctx)) {
		IPAERR_RL("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (hdrs == NULL || hdrs->num_hdrs == 0) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	IPADBG("adding %d headers to IPA driver internal data struct\n",
			hdrs->num_hdrs);
	for (i = 0; i < hdrs->num_hdrs; i++) {
		if (__ipa_add_hdr(&hdrs->hdr[i])) {
			IPAERR_RL("failed to add hdr %d\n", i);
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

/**
 * ipa2_del_hdr_by_user() - Remove the specified headers
 * from SW and optionally commit them to IPA HW
 * @hdls:	[inout] set of headers to delete
 * @by_user:	Operation requested by user?
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_del_hdr_by_user(struct ipa_ioc_del_hdr *hdls, bool by_user)
{
	int i;
	int result = -EFAULT;

	if (unlikely(!ipa_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (hdls == NULL || hdls->num_hdls == 0) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa_del_hdr(hdls->hdl[i].hdl, by_user)) {
			IPAERR_RL("failed to del hdr %i\n", i);
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

/**
 * ipa2_del_hdr() - Remove the specified headers from SW
 * and optionally commit them to IPA HW
 * @hdls:	[inout] set of headers to delete
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_del_hdr(struct ipa_ioc_del_hdr *hdls)
{
	return ipa2_del_hdr_by_user(hdls, false);
}

/**
 * ipa2_add_hdr_proc_ctx() - add the specified headers to SW
 * and optionally commit them to IPA HW
 * @proc_ctxs:	[inout] set of processing context headers to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_add_hdr_proc_ctx(struct ipa_ioc_add_hdr_proc_ctx *proc_ctxs)
{
	int i;
	int result = -EFAULT;

	if (ipa_ctx->ipa_hw_type <= IPA_HW_v2_0 ||
	    ipa_ctx->ipa_hw_type == IPA_HW_v2_6L) {
		IPAERR_RL("Processing context not supported on IPA HW %d\n",
			ipa_ctx->ipa_hw_type);
		return -EFAULT;
	}

	if (proc_ctxs == NULL || proc_ctxs->num_proc_ctxs == 0) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	IPADBG("adding %d header processing contextes to IPA driver\n",
			proc_ctxs->num_proc_ctxs);
	for (i = 0; i < proc_ctxs->num_proc_ctxs; i++) {
		if (__ipa_add_hdr_proc_ctx(&proc_ctxs->proc_ctx[i], true)) {
			IPAERR_RL("failed to add hdr pric ctx %d\n", i);
			proc_ctxs->proc_ctx[i].status = -1;
		} else {
			proc_ctxs->proc_ctx[i].status = 0;
		}
	}

	if (proc_ctxs->commit) {
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

/**
 * ipa2_del_hdr_proc_ctx_by_user() -
 * Remove the specified processing context headers from SW and
 * optionally commit them to IPA HW.
 * @hdls:	[inout] set of processing context headers to delete
 * @by_user:	Operation requested by user?
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_del_hdr_proc_ctx_by_user(struct ipa_ioc_del_hdr_proc_ctx *hdls,
	bool by_user)
{
	int i;
	int result;

	if (ipa_ctx->ipa_hw_type <= IPA_HW_v2_0 ||
	    ipa_ctx->ipa_hw_type == IPA_HW_v2_6L) {
		IPAERR("Processing context not supported on IPA HW %d\n",
			ipa_ctx->ipa_hw_type);
		return -EFAULT;
	}

	if (hdls == NULL || hdls->num_hdls == 0) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}

	mutex_lock(&ipa_ctx->lock);
	for (i = 0; i < hdls->num_hdls; i++) {
		if (__ipa_del_hdr_proc_ctx(hdls->hdl[i].hdl, true, by_user)) {
			IPAERR_RL("failed to del hdr %i\n", i);
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

/**
 * ipa2_del_hdr_proc_ctx() -
 * Remove the specified processing context headers from SW and
 * optionally commit them to IPA HW.
 * @hdls:	[inout] set of processing context headers to delete
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_del_hdr_proc_ctx(struct ipa_ioc_del_hdr_proc_ctx *hdls)
{
	return ipa2_del_hdr_proc_ctx_by_user(hdls, false);
}

/**
 * ipa2_commit_hdr() - commit to IPA HW the current header table in SW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_commit_hdr(void)
{
	int result = -EFAULT;

	/*
	 * issue a commit on the routing module since routing rules point to
	 * header table entries
	 */
	if (ipa2_commit_rt(IPA_IP_v4))
		return -EPERM;
	if (ipa2_commit_rt(IPA_IP_v6))
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

/**
 * ipa2_reset_hdr() - reset the current header table in SW (does not commit to
 * HW)
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_reset_hdr(void)
{
	struct ipa_hdr_entry *entry;
	struct ipa_hdr_entry *next;
	struct ipa_hdr_proc_ctx_entry *ctx_entry;
	struct ipa_hdr_proc_ctx_entry *ctx_next;
	struct ipa_hdr_offset_entry *off_entry;
	struct ipa_hdr_offset_entry *off_next;
	struct ipa_hdr_proc_ctx_offset_entry *ctx_off_entry;
	struct ipa_hdr_proc_ctx_offset_entry *ctx_off_next;
	int i;

	/*
	 * issue a reset on the routing module since routing rules point to
	 * header table entries
	 */
	if (ipa2_reset_rt(IPA_IP_v4))
		IPAERR("fail to reset v4 rt\n");
	if (ipa2_reset_rt(IPA_IP_v6))
		IPAERR("fail to reset v4 rt\n");

	mutex_lock(&ipa_ctx->lock);
	IPADBG("reset hdr\n");
	list_for_each_entry_safe(entry, next,
			&ipa_ctx->hdr_tbl.head_hdr_entry_list, link) {

		/* do not remove the default header */
		if (!strcmp(entry->name, IPA_LAN_RX_HDR_NAME)) {
			if (entry->is_hdr_proc_ctx) {
				mutex_unlock(&ipa_ctx->lock);
				WARN_ON(1);
				IPAERR("default header is proc ctx\n");
				return -EFAULT;
			}
			continue;
		}

		if (ipa_id_find(entry->id) == NULL) {
			mutex_unlock(&ipa_ctx->lock);
			WARN_ON(1);
			return -EFAULT;
		}
		if (entry->is_hdr_proc_ctx) {
			dma_unmap_single(ipa_ctx->pdev,
				entry->phys_base,
				entry->hdr_len,
				DMA_TO_DEVICE);
			entry->proc_ctx = NULL;
		}
		list_del(&entry->link);
		entry->ref_cnt = 0;
		entry->cookie = 0;

		/* remove the handle from the database */
		ipa_id_remove(entry->id);
		kmem_cache_free(ipa_ctx->hdr_cache, entry);

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

	IPADBG("reset hdr proc ctx\n");
	list_for_each_entry_safe(
		ctx_entry,
		ctx_next,
		&ipa_ctx->hdr_proc_ctx_tbl.head_proc_ctx_entry_list,
		link) {

		if (ipa_id_find(ctx_entry->id) == NULL) {
			mutex_unlock(&ipa_ctx->lock);
			WARN_ON(1);
			return -EFAULT;
		}
		list_del(&ctx_entry->link);
		ctx_entry->ref_cnt = 0;
		ctx_entry->cookie = 0;

		/* remove the handle from the database */
		ipa_id_remove(ctx_entry->id);
		kmem_cache_free(ipa_ctx->hdr_proc_ctx_cache, ctx_entry);

	}
	for (i = 0; i < IPA_HDR_PROC_CTX_BIN_MAX; i++) {
		list_for_each_entry_safe(ctx_off_entry, ctx_off_next,
				&ipa_ctx->hdr_proc_ctx_tbl.head_offset_list[i],
				link) {

			list_del(&ctx_off_entry->link);
			kmem_cache_free(ipa_ctx->hdr_proc_ctx_offset_cache,
					ctx_off_entry);
		}
		list_for_each_entry_safe(ctx_off_entry, ctx_off_next,
			    &ipa_ctx->hdr_proc_ctx_tbl.head_free_offset_list[i],
			    link) {
			list_del(&ctx_off_entry->link);
			kmem_cache_free(ipa_ctx->hdr_proc_ctx_offset_cache,
				ctx_off_entry);
		}
	}
	ipa_ctx->hdr_proc_ctx_tbl.end = 0;
	ipa_ctx->hdr_proc_ctx_tbl.proc_ctx_cnt = 0;
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}

static struct ipa_hdr_entry *__ipa_find_hdr(const char *name)
{
	struct ipa_hdr_entry *entry;

	list_for_each_entry(entry, &ipa_ctx->hdr_tbl.head_hdr_entry_list,
			link) {
		if (!strcmp(name, entry->name))
			return entry;
	}

	return NULL;
}

/**
 * ipa2_get_hdr() - Lookup the specified header resource
 * @lookup:	[inout] header to lookup and its handle
 *
 * lookup the specified header resource and return handle if it exists
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 *		Caller should call ipa_put_hdr later if this function succeeds
 */
int ipa2_get_hdr(struct ipa_ioc_get_hdr *lookup)
{
	struct ipa_hdr_entry *entry;
	int result = -1;

	if (unlikely(!ipa_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (lookup == NULL) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}
	mutex_lock(&ipa_ctx->lock);
	lookup->name[IPA_RESOURCE_NAME_MAX-1] = '\0';
	entry = __ipa_find_hdr(lookup->name);
	if (entry) {
		lookup->hdl = entry->id;
		result = 0;
	}
	mutex_unlock(&ipa_ctx->lock);

	return result;
}

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

	if (__ipa_del_hdr(hdr_hdl, false)) {
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
 * __ipa_release_hdr_proc_ctx() - drop reference to processing context
 *  and cause deletion if reference count permits
 * @proc_ctx_hdl:	[in] handle of processing context to be released
 *
 * Returns:	0 on success, negative on failure
 */
int __ipa_release_hdr_proc_ctx(u32 proc_ctx_hdl)
{
	int result = 0;

	if (__ipa_del_hdr_proc_ctx(proc_ctx_hdl, true, false)) {
		IPADBG("fail to del hdr %x\n", proc_ctx_hdl);
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
 * ipa2_put_hdr() - Release the specified header handle
 * @hdr_hdl:	[in] the header handle to release
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_put_hdr(u32 hdr_hdl)
{
	struct ipa_hdr_entry *entry;
	int result = -EFAULT;

	mutex_lock(&ipa_ctx->lock);

	entry = ipa_id_find(hdr_hdl);
	if (entry == NULL) {
		IPAERR_RL("lookup failed\n");
		result = -EINVAL;
		goto bail;
	}

	if (entry->cookie != IPA_HDR_COOKIE) {
		IPAERR_RL("invalid header entry\n");
		result = -EINVAL;
		goto bail;
	}

	result = 0;
bail:
	mutex_unlock(&ipa_ctx->lock);
	return result;
}

/**
 * ipa2_copy_hdr() - Lookup the specified header resource and return a copy of
 * it
 * @copy:	[inout] header to lookup and its copy
 *
 * lookup the specified header resource and return a copy of it (along with its
 * attributes) if it exists, this would be called for partial headers
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_copy_hdr(struct ipa_ioc_copy_hdr *copy)
{
	struct ipa_hdr_entry *entry;
	int result = -EFAULT;

	if (copy == NULL) {
		IPAERR_RL("bad parm\n");
		return -EINVAL;
	}
	mutex_lock(&ipa_ctx->lock);
	copy->name[IPA_RESOURCE_NAME_MAX-1] = '\0';
	entry = __ipa_find_hdr(copy->name);
	if (entry) {
		memcpy(copy->hdr, entry->hdr, entry->hdr_len);
		copy->hdr_len = entry->hdr_len;
		copy->type = entry->type;
		copy->is_partial = entry->is_partial;
		copy->is_eth2_ofst_valid = entry->is_eth2_ofst_valid;
		copy->eth2_ofst = entry->eth2_ofst;
		result = 0;
	}
	mutex_unlock(&ipa_ctx->lock);

	return result;
}

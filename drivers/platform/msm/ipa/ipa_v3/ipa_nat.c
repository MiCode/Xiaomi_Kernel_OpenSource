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

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include "ipa_i.h"
#include "ipahal/ipahal.h"

#define IPA_NAT_PHYS_MEM_OFFSET  0
#define IPA_NAT_PHYS_MEM_SIZE  IPA_RAM_NAT_SIZE

#define IPA_NAT_TEMP_MEM_SIZE 128

enum nat_table_type {
	IPA_NAT_BASE_TBL = 0,
	IPA_NAT_EXPN_TBL = 1,
	IPA_NAT_INDX_TBL = 2,
	IPA_NAT_INDEX_EXPN_TBL = 3,
};

#define NAT_TABLE_ENTRY_SIZE_BYTE 32
#define NAT_INTEX_TABLE_ENTRY_SIZE_BYTE 4


static int ipa3_nat_vma_fault_remap(
	 struct vm_area_struct *vma, struct vm_fault *vmf)
{
	IPADBG("\n");
	vmf->page = NULL;

	return VM_FAULT_SIGBUS;
}

/* VMA related file operations functions */
static struct vm_operations_struct ipa3_nat_remap_vm_ops = {
	.fault = ipa3_nat_vma_fault_remap,
};

static int ipa3_nat_open(struct inode *inode, struct file *filp)
{
	struct ipa3_nat_mem *nat_ctx;

	IPADBG("\n");
	nat_ctx = container_of(inode->i_cdev, struct ipa3_nat_mem, cdev);
	filp->private_data = nat_ctx;
	IPADBG("return\n");

	return 0;
}

static int ipa3_nat_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long vsize = vma->vm_end - vma->vm_start;
	struct ipa3_nat_mem *nat_ctx =
		(struct ipa3_nat_mem *)filp->private_data;
	unsigned long phys_addr;
	int result;

	mutex_lock(&nat_ctx->lock);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (nat_ctx->is_sys_mem) {
		IPADBG("Mapping system memory\n");
		if (nat_ctx->is_mapped) {
			IPAERR("mapping already exists, only 1 supported\n");
			result = -EINVAL;
			goto bail;
		}
		IPADBG("map sz=0x%zx\n", nat_ctx->size);
		result =
			dma_mmap_coherent(
				 ipa3_ctx->pdev, vma,
				 nat_ctx->vaddr, nat_ctx->dma_handle,
				 nat_ctx->size);

		if (result) {
			IPAERR("unable to map memory. Err:%d\n", result);
			goto bail;
		}
		ipa3_ctx->nat_mem.nat_base_address = nat_ctx->vaddr;
	} else {
		IPADBG("Mapping shared(local) memory\n");
		IPADBG("map sz=0x%lx\n", vsize);

		if ((IPA_NAT_PHYS_MEM_SIZE == 0) ||
				(vsize > IPA_NAT_PHYS_MEM_SIZE)) {
			result = -EINVAL;
			goto bail;
		}
		phys_addr = ipa3_ctx->ipa_wrapper_base +
			ipa3_ctx->ctrl->ipa_reg_base_ofst +
			ipahal_get_reg_n_ofst(IPA_SRAM_DIRECT_ACCESS_n,
			IPA_NAT_PHYS_MEM_OFFSET);

		if (remap_pfn_range(
			 vma, vma->vm_start,
			 phys_addr >> PAGE_SHIFT, vsize, vma->vm_page_prot)) {
			IPAERR("remap failed\n");
			result = -EAGAIN;
			goto bail;
		}
		ipa3_ctx->nat_mem.nat_base_address = (void *)vma->vm_start;
	}
	nat_ctx->is_mapped = true;
	vma->vm_ops = &ipa3_nat_remap_vm_ops;
	IPADBG("return\n");
	result = 0;
bail:
	mutex_unlock(&nat_ctx->lock);
	return result;
}

static const struct file_operations ipa3_nat_fops = {
	.owner = THIS_MODULE,
	.open = ipa3_nat_open,
	.mmap = ipa3_nat_mmap
};

/**
 * ipa3_allocate_temp_nat_memory() - Allocates temp nat memory
 *
 * Called during nat table delete
 */
void ipa3_allocate_temp_nat_memory(void)
{
	struct ipa3_nat_mem *nat_ctx = &(ipa3_ctx->nat_mem);
	int gfp_flags = GFP_KERNEL | __GFP_ZERO;

	nat_ctx->tmp_vaddr =
		dma_alloc_coherent(ipa3_ctx->pdev, IPA_NAT_TEMP_MEM_SIZE,
				&nat_ctx->tmp_dma_handle, gfp_flags);

	if (nat_ctx->tmp_vaddr == NULL) {
		IPAERR("Temp Memory alloc failed\n");
		nat_ctx->is_tmp_mem = false;
		return;
	}

	nat_ctx->is_tmp_mem = true;
	IPADBG("IPA NAT allocated temp memory successfully\n");
}

/**
 * ipa3_create_nat_device() - Create the NAT device
 *
 * Called during ipa init to create nat device
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_create_nat_device(void)
{
	struct ipa3_nat_mem *nat_ctx = &(ipa3_ctx->nat_mem);
	int result;

	IPADBG("\n");

	mutex_lock(&nat_ctx->lock);
	nat_ctx->class = class_create(THIS_MODULE, NAT_DEV_NAME);
	if (IS_ERR(nat_ctx->class)) {
		IPAERR("unable to create the class\n");
		result = -ENODEV;
		goto vaddr_alloc_fail;
	}
	result = alloc_chrdev_region(&nat_ctx->dev_num,
					0,
					1,
					NAT_DEV_NAME);
	if (result) {
		IPAERR("alloc_chrdev_region err.\n");
		result = -ENODEV;
		goto alloc_chrdev_region_fail;
	}

	nat_ctx->dev =
	   device_create(nat_ctx->class, NULL, nat_ctx->dev_num, nat_ctx,
			"%s", NAT_DEV_NAME);

	if (IS_ERR(nat_ctx->dev)) {
		IPAERR("device_create err:%ld\n", PTR_ERR(nat_ctx->dev));
		result = -ENODEV;
		goto device_create_fail;
	}

	cdev_init(&nat_ctx->cdev, &ipa3_nat_fops);
	nat_ctx->cdev.owner = THIS_MODULE;
	nat_ctx->cdev.ops = &ipa3_nat_fops;

	result = cdev_add(&nat_ctx->cdev, nat_ctx->dev_num, 1);
	if (result) {
		IPAERR("cdev_add err=%d\n", -result);
		goto cdev_add_fail;
	}
	IPADBG("ipa nat dev added successful. major:%d minor:%d\n",
			MAJOR(nat_ctx->dev_num),
			MINOR(nat_ctx->dev_num));

	nat_ctx->is_dev = true;
	ipa3_allocate_temp_nat_memory();
	IPADBG("IPA NAT device created successfully\n");
	result = 0;
	goto bail;

cdev_add_fail:
	device_destroy(nat_ctx->class, nat_ctx->dev_num);
device_create_fail:
	unregister_chrdev_region(nat_ctx->dev_num, 1);
alloc_chrdev_region_fail:
	class_destroy(nat_ctx->class);
vaddr_alloc_fail:
	if (nat_ctx->vaddr) {
		IPADBG("Releasing system memory\n");
		dma_free_coherent(
			 ipa3_ctx->pdev, nat_ctx->size,
			 nat_ctx->vaddr, nat_ctx->dma_handle);
		nat_ctx->vaddr = NULL;
		nat_ctx->dma_handle = 0;
		nat_ctx->size = 0;
	}

bail:
	mutex_unlock(&nat_ctx->lock);

	return result;
}

/**
 * ipa3_allocate_nat_device() - Allocates memory for the NAT device
 * @mem:	[in/out] memory parameters
 *
 * Called by NAT client driver to allocate memory for the NAT entries. Based on
 * the request size either shared or system memory will be used.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem)
{
	struct ipa3_nat_mem *nat_ctx = &(ipa3_ctx->nat_mem);
	int gfp_flags = GFP_KERNEL | __GFP_ZERO;
	int result;

	IPADBG("passed memory size %zu\n", mem->size);

	mutex_lock(&nat_ctx->lock);
	if (strcmp(mem->dev_name, NAT_DEV_NAME)) {
		IPAERR_RL("Nat device name mismatch\n");
		IPAERR_RL("Expect: %s Recv: %s\n", NAT_DEV_NAME, mem->dev_name);
		result = -EPERM;
		goto bail;
	}

	if (nat_ctx->is_dev != true) {
		IPAERR("Nat device not created successfully during boot up\n");
		result = -EPERM;
		goto bail;
	}

	if (nat_ctx->is_dev_init == true) {
		IPAERR("Device already init\n");
		result = 0;
		goto bail;
	}

	if (mem->size <= 0 ||
			nat_ctx->is_dev_init == true) {
		IPAERR_RL("Invalid Parameters or device is already init\n");
		result = -EPERM;
		goto bail;
	}

	if (mem->size > IPA_NAT_PHYS_MEM_SIZE) {
		IPADBG("Allocating system memory\n");
		nat_ctx->is_sys_mem = true;
		nat_ctx->vaddr =
		   dma_alloc_coherent(ipa3_ctx->pdev, mem->size,
				   &nat_ctx->dma_handle, gfp_flags);
		if (nat_ctx->vaddr == NULL) {
			IPAERR("memory alloc failed\n");
			result = -ENOMEM;
			goto bail;
		}
		nat_ctx->size = mem->size;
	} else {
		IPADBG("using shared(local) memory\n");
		nat_ctx->is_sys_mem = false;
	}

	nat_ctx->is_dev_init = true;
	IPADBG("IPA NAT dev init successfully\n");
	result = 0;

bail:
	mutex_unlock(&nat_ctx->lock);

	return result;
}

/* IOCTL function handlers */
/**
 * ipa3_nat_init_cmd() - Post IP_V4_NAT_INIT command to IPA HW
 * @init:	[in] initialization command attributes
 *
 * Called by NAT client driver to post IP_V4_NAT_INIT command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_nat_init_cmd(struct ipa_ioc_v4_nat_init *init)
{
#define TBL_ENTRY_SIZE 32
#define INDX_TBL_ENTRY_SIZE 4

	struct ipahal_imm_cmd_pyld *nop_cmd_pyld = NULL;
	struct ipa3_desc desc[2];
	struct ipahal_imm_cmd_ip_v4_nat_init cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld = NULL;
	int result;
	u32 offset = 0;
	size_t tmp;

	IPADBG("\n");
	if (init->table_entries == 0) {
		IPADBG("Table entries is zero\n");
		return -EPERM;
	}

	/* check for integer overflow */
	if (init->ipv4_rules_offset >
		UINT_MAX - (TBL_ENTRY_SIZE * (init->table_entries + 1))) {
		IPAERR_RL("Detected overflow\n");
		return -EPERM;
	}
	/* Check Table Entry offset is not
	   beyond allocated size */
	tmp = init->ipv4_rules_offset +
		(TBL_ENTRY_SIZE * (init->table_entries + 1));
	if (tmp > ipa3_ctx->nat_mem.size) {
		IPAERR_RL("Table rules offset not valid\n");
		IPAERR_RL("offset:%d entrys:%d size:%zu mem_size:%zu\n",
			init->ipv4_rules_offset, (init->table_entries + 1),
			tmp, ipa3_ctx->nat_mem.size);
		return -EPERM;
	}

	/* check for integer overflow */
	if (init->expn_rules_offset >
		(UINT_MAX - (TBL_ENTRY_SIZE * init->expn_table_entries))) {
		IPAERR_RL("Detected overflow\n");
		return -EPERM;
	}
	/* Check Expn Table Entry offset is not
	   beyond allocated size */
	tmp = init->expn_rules_offset +
		(TBL_ENTRY_SIZE * init->expn_table_entries);
	if (tmp > ipa3_ctx->nat_mem.size) {
		IPAERR_RL("Expn Table rules offset not valid\n");
		IPAERR_RL("offset:%d entrys:%d size:%zu mem_size:%zu\n",
			init->expn_rules_offset, init->expn_table_entries,
			tmp, ipa3_ctx->nat_mem.size);
		return -EPERM;
	}

  /* check for integer overflow */
	if (init->index_offset >
		UINT_MAX - (INDX_TBL_ENTRY_SIZE * (init->table_entries + 1))) {
		IPAERR_RL("Detected overflow\n");
		return -EPERM;
	}
	/* Check Indx Table Entry offset is not
	   beyond allocated size */
	tmp = init->index_offset +
		(INDX_TBL_ENTRY_SIZE * (init->table_entries + 1));
	if (tmp > ipa3_ctx->nat_mem.size) {
		IPAERR_RL("Indx Table rules offset not valid\n");
		IPAERR_RL("offset:%d entrys:%d size:%zu mem_size:%zu\n",
			init->index_offset, (init->table_entries + 1),
			tmp, ipa3_ctx->nat_mem.size);
		return -EPERM;
	}

  /* check for integer overflow */
	if (init->index_expn_offset >
		UINT_MAX - (INDX_TBL_ENTRY_SIZE * init->expn_table_entries)) {
		IPAERR_RL("Detected overflow\n");
		return -EPERM;
	}
	/* Check Expn Table entry offset is not
	   beyond allocated size */
	tmp = init->index_expn_offset +
		(INDX_TBL_ENTRY_SIZE * init->expn_table_entries);
	if (tmp > ipa3_ctx->nat_mem.size) {
		IPAERR_RL("Indx Expn Table rules offset not valid\n");
		IPAERR_RL("offset:%d entrys:%d size:%zu mem_size:%zu\n",
			init->index_expn_offset, init->expn_table_entries,
			tmp, ipa3_ctx->nat_mem.size);
		return -EPERM;
	}

	memset(&desc, 0, sizeof(desc));
	/* NO-OP IC for ensuring that IPA pipeline is empty */
	nop_cmd_pyld =
		ipahal_construct_nop_imm_cmd(false, IPAHAL_HPS_CLEAR, false);
	if (!nop_cmd_pyld) {
		IPAERR("failed to construct NOP imm cmd\n");
		result = -ENOMEM;
		goto bail;
	}

	desc[0].opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_REGISTER_WRITE);
	desc[0].type = IPA_IMM_CMD_DESC;
	desc[0].callback = NULL;
	desc[0].user1 = NULL;
	desc[0].user2 = 0;
	desc[0].pyld = nop_cmd_pyld->data;
	desc[0].len = nop_cmd_pyld->len;

	if (ipa3_ctx->nat_mem.vaddr) {
		IPADBG("using system memory for nat table\n");
		cmd.ipv4_rules_addr_shared = false;
		cmd.ipv4_expansion_rules_addr_shared = false;
		cmd.index_table_addr_shared = false;
		cmd.index_table_expansion_addr_shared = false;

		offset = UINT_MAX - ipa3_ctx->nat_mem.dma_handle;

		if ((init->ipv4_rules_offset > offset) ||
				(init->expn_rules_offset > offset) ||
				(init->index_offset > offset) ||
				(init->index_expn_offset > offset)) {
			IPAERR_RL("Failed due to integer overflow\n");
			IPAERR_RL("nat.mem.dma_handle: 0x%pa\n",
				&ipa3_ctx->nat_mem.dma_handle);
			IPAERR_RL("ipv4_rules_offset: 0x%x\n",
				init->ipv4_rules_offset);
			IPAERR_RL("expn_rules_offset: 0x%x\n",
				init->expn_rules_offset);
			IPAERR_RL("index_offset: 0x%x\n",
				init->index_offset);
			IPAERR_RL("index_expn_offset: 0x%x\n",
				init->index_expn_offset);
			result = -EPERM;
			goto free_nop;
		}
		cmd.ipv4_rules_addr =
			ipa3_ctx->nat_mem.dma_handle + init->ipv4_rules_offset;
		IPADBG("ipv4_rules_offset:0x%x\n", init->ipv4_rules_offset);

		cmd.ipv4_expansion_rules_addr =
		   ipa3_ctx->nat_mem.dma_handle + init->expn_rules_offset;
		IPADBG("expn_rules_offset:0x%x\n", init->expn_rules_offset);

		cmd.index_table_addr =
			ipa3_ctx->nat_mem.dma_handle + init->index_offset;
		IPADBG("index_offset:0x%x\n", init->index_offset);

		cmd.index_table_expansion_addr =
		   ipa3_ctx->nat_mem.dma_handle + init->index_expn_offset;
		IPADBG("index_expn_offset:0x%x\n", init->index_expn_offset);
	} else {
		IPADBG("using shared(local) memory for nat table\n");
		cmd.ipv4_rules_addr_shared = true;
		cmd.ipv4_expansion_rules_addr_shared = true;
		cmd.index_table_addr_shared = true;
		cmd.index_table_expansion_addr_shared = true;

		cmd.ipv4_rules_addr = init->ipv4_rules_offset +
				IPA_RAM_NAT_OFST;

		cmd.ipv4_expansion_rules_addr = init->expn_rules_offset +
				IPA_RAM_NAT_OFST;

		cmd.index_table_addr = init->index_offset  +
				IPA_RAM_NAT_OFST;

		cmd.index_table_expansion_addr = init->index_expn_offset +
				IPA_RAM_NAT_OFST;
	}
	cmd.table_index = init->tbl_index;
	IPADBG("Table index:0x%x\n", cmd.table_index);
	cmd.size_base_tables = init->table_entries;
	IPADBG("Base Table size:0x%x\n", cmd.size_base_tables);
	cmd.size_expansion_tables = init->expn_table_entries;
	IPADBG("Expansion Table size:0x%x\n", cmd.size_expansion_tables);
	cmd.public_ip_addr = init->ip_addr;
	IPADBG("Public ip address:0x%x\n", cmd.public_ip_addr);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_V4_NAT_INIT, &cmd, false);
	if (!cmd_pyld) {
		IPAERR_RL("Fail to construct ip_v4_nat_init imm cmd\n");
		result = -EPERM;
		goto free_nop;
	}

	desc[1].opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_IP_V4_NAT_INIT);
	desc[1].type = IPA_IMM_CMD_DESC;
	desc[1].callback = NULL;
	desc[1].user1 = NULL;
	desc[1].user2 = 0;
	desc[1].pyld = cmd_pyld->data;
	desc[1].len = cmd_pyld->len;
	IPADBG("posting v4 init command\n");
	if (ipa3_send_cmd(2, desc)) {
		IPAERR("Fail to send immediate command\n");
		result = -EPERM;
		goto destroy_imm_cmd;
	}

	ipa3_ctx->nat_mem.public_ip_addr = init->ip_addr;
	IPADBG("Table ip address:0x%x", ipa3_ctx->nat_mem.public_ip_addr);

	ipa3_ctx->nat_mem.ipv4_rules_addr =
	 (char *)ipa3_ctx->nat_mem.nat_base_address + init->ipv4_rules_offset;
	IPADBG("ipv4_rules_addr: 0x%p\n",
				 ipa3_ctx->nat_mem.ipv4_rules_addr);

	ipa3_ctx->nat_mem.ipv4_expansion_rules_addr =
	 (char *)ipa3_ctx->nat_mem.nat_base_address + init->expn_rules_offset;
	IPADBG("ipv4_expansion_rules_addr: 0x%p\n",
				 ipa3_ctx->nat_mem.ipv4_expansion_rules_addr);

	ipa3_ctx->nat_mem.index_table_addr =
		 (char *)ipa3_ctx->nat_mem.nat_base_address +
		 init->index_offset;
	IPADBG("index_table_addr: 0x%p\n",
				 ipa3_ctx->nat_mem.index_table_addr);

	ipa3_ctx->nat_mem.index_table_expansion_addr =
	 (char *)ipa3_ctx->nat_mem.nat_base_address + init->index_expn_offset;
	IPADBG("index_table_expansion_addr: 0x%p\n",
				 ipa3_ctx->nat_mem.index_table_expansion_addr);

	IPADBG("size_base_tables: %d\n", init->table_entries);
	ipa3_ctx->nat_mem.size_base_tables  = init->table_entries;

	IPADBG("size_expansion_tables: %d\n", init->expn_table_entries);
	ipa3_ctx->nat_mem.size_expansion_tables = init->expn_table_entries;

	IPADBG("return\n");
	result = 0;
destroy_imm_cmd:
	ipahal_destroy_imm_cmd(cmd_pyld);
free_nop:
	ipahal_destroy_imm_cmd(nop_cmd_pyld);
bail:
	return result;
}

/**
 * ipa3_nat_dma_cmd() - Post NAT_DMA command to IPA HW
 * @dma:	[in] initialization command attributes
 *
 * Called by NAT client driver to post NAT_DMA command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma)
{
#define NUM_OF_DESC 2

	struct ipahal_imm_cmd_pyld *nop_cmd_pyld = NULL;
	struct ipahal_imm_cmd_nat_dma cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld = NULL;
	struct ipa3_desc *desc = NULL;
	u16 size = 0, cnt = 0;
	int ret = 0;

	IPADBG("\n");
	if (dma->entries <= 0) {
		IPAERR_RL("Invalid number of commands %d\n",
			dma->entries);
		ret = -EPERM;
		goto bail;
	}

	for (cnt = 0; cnt < dma->entries; cnt++) {
		if (dma->dma[cnt].table_index >= 1) {
			IPAERR_RL("Invalid table index %d\n",
				dma->dma[cnt].table_index);
			ret = -EPERM;
			goto bail;
		}

		switch (dma->dma[cnt].base_addr) {
		case IPA_NAT_BASE_TBL:
			if (dma->dma[cnt].offset >=
				(ipa3_ctx->nat_mem.size_base_tables + 1) *
				NAT_TABLE_ENTRY_SIZE_BYTE) {
				IPAERR_RL("Invalid offset %d\n",
					dma->dma[cnt].offset);
				ret = -EPERM;
				goto bail;
			}

			break;

		case IPA_NAT_EXPN_TBL:
			if (dma->dma[cnt].offset >=
				ipa3_ctx->nat_mem.size_expansion_tables *
				NAT_TABLE_ENTRY_SIZE_BYTE) {
				IPAERR_RL("Invalid offset %d\n",
					dma->dma[cnt].offset);
				ret = -EPERM;
				goto bail;
			}

			break;

		case IPA_NAT_INDX_TBL:
			if (dma->dma[cnt].offset >=
				(ipa3_ctx->nat_mem.size_base_tables + 1) *
				NAT_INTEX_TABLE_ENTRY_SIZE_BYTE) {
				IPAERR_RL("Invalid offset %d\n",
					dma->dma[cnt].offset);
				ret = -EPERM;
				goto bail;
			}

			break;

		case IPA_NAT_INDEX_EXPN_TBL:
			if (dma->dma[cnt].offset >=
				ipa3_ctx->nat_mem.size_expansion_tables *
				NAT_INTEX_TABLE_ENTRY_SIZE_BYTE) {
				IPAERR_RL("Invalid offset %d\n",
					dma->dma[cnt].offset);
				ret = -EPERM;
				goto bail;
			}

			break;

		default:
			IPAERR_RL("Invalid base_addr %d\n",
				dma->dma[cnt].base_addr);
			ret = -EPERM;
			goto bail;
		}
	}

	size = sizeof(struct ipa3_desc) * NUM_OF_DESC;
	desc = kzalloc(size, GFP_KERNEL);
	if (desc == NULL) {
		IPAERR("Failed to alloc memory\n");
		ret = -ENOMEM;
		goto bail;
	}

	/* NO-OP IC for ensuring that IPA pipeline is empty */
	nop_cmd_pyld =
		ipahal_construct_nop_imm_cmd(false, IPAHAL_HPS_CLEAR, false);
	if (!nop_cmd_pyld) {
		IPAERR("Failed to construct NOP imm cmd\n");
		ret = -ENOMEM;
		goto bail;
	}
	desc[0].type = IPA_IMM_CMD_DESC;
	desc[0].opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_REGISTER_WRITE);
	desc[0].callback = NULL;
	desc[0].user1 = NULL;
	desc[0].user2 = 0;
	desc[0].pyld = nop_cmd_pyld->data;
	desc[0].len = nop_cmd_pyld->len;

	for (cnt = 0; cnt < dma->entries; cnt++) {
		cmd.table_index = dma->dma[cnt].table_index;
		cmd.base_addr = dma->dma[cnt].base_addr;
		cmd.offset = dma->dma[cnt].offset;
		cmd.data = dma->dma[cnt].data;
		cmd_pyld = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_NAT_DMA, &cmd, false);
		if (!cmd_pyld) {
			IPAERR_RL("Fail to construct nat_dma imm cmd\n");
			continue;
		}
		desc[1].type = IPA_IMM_CMD_DESC;
		desc[1].opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_NAT_DMA);
		desc[1].callback = NULL;
		desc[1].user1 = NULL;
		desc[1].user2 = 0;
		desc[1].pyld = cmd_pyld->data;
		desc[1].len = cmd_pyld->len;

		ret = ipa3_send_cmd(NUM_OF_DESC, desc);
		if (ret == -EPERM)
			IPAERR("Fail to send immediate command %d\n", cnt);
		ipahal_destroy_imm_cmd(cmd_pyld);
	}

bail:
	if (desc != NULL)
		kfree(desc);

	if (nop_cmd_pyld != NULL)
		ipahal_destroy_imm_cmd(nop_cmd_pyld);

	return ret;
}

/**
 * ipa3_nat_free_mem_and_device() - free the NAT memory and remove the device
 * @nat_ctx:	[in] the IPA NAT memory to free
 *
 * Called by NAT client driver to free the NAT memory and remove the device
 */
void ipa3_nat_free_mem_and_device(struct ipa3_nat_mem *nat_ctx)
{
	IPADBG("\n");
	mutex_lock(&nat_ctx->lock);

	if (nat_ctx->is_sys_mem) {
		IPADBG("freeing the dma memory\n");
		dma_free_coherent(
			 ipa3_ctx->pdev, nat_ctx->size,
			 nat_ctx->vaddr, nat_ctx->dma_handle);
		nat_ctx->size = 0;
		nat_ctx->vaddr = NULL;
	}
	nat_ctx->is_mapped = false;
	nat_ctx->is_sys_mem = false;
	nat_ctx->is_dev_init = false;

	mutex_unlock(&nat_ctx->lock);
	IPADBG("return\n");
}

/**
 * ipa3_nat_del_cmd() - Delete a NAT table
 * @del:	[in] delete table table table parameters
 *
 * Called by NAT client driver to delete the nat table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_nat_del_cmd(struct ipa_ioc_v4_nat_del *del)
{
	struct ipahal_imm_cmd_pyld *nop_cmd_pyld = NULL;
	struct ipa3_desc desc[2];
	struct ipahal_imm_cmd_ip_v4_nat_init cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	bool mem_type_shared = true;
	u32 base_addr = IPA_NAT_PHYS_MEM_OFFSET;
	int result;

	IPADBG("\n");
	if (ipa3_ctx->nat_mem.is_tmp_mem) {
		IPAERR("using temp memory during nat del\n");
		mem_type_shared = false;
		base_addr = ipa3_ctx->nat_mem.tmp_dma_handle;
	}

	if (del->public_ip_addr == 0) {
		IPADBG("Bad Parameter\n");
		result = -EPERM;
		goto bail;
	}

	memset(&desc, 0, sizeof(desc));
	/* NO-OP IC for ensuring that IPA pipeline is empty */
	nop_cmd_pyld =
		ipahal_construct_nop_imm_cmd(false, IPAHAL_HPS_CLEAR, false);
	if (!nop_cmd_pyld) {
		IPAERR("Failed to construct NOP imm cmd\n");
		result = -ENOMEM;
		goto bail;
	}
	desc[0].opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_REGISTER_WRITE);
	desc[0].type = IPA_IMM_CMD_DESC;
	desc[0].callback = NULL;
	desc[0].user1 = NULL;
	desc[0].user2 = 0;
	desc[0].pyld = nop_cmd_pyld->data;
	desc[0].len = nop_cmd_pyld->len;

	cmd.table_index = del->table_index;
	cmd.ipv4_rules_addr = base_addr;
	cmd.ipv4_rules_addr_shared = mem_type_shared;
	cmd.ipv4_expansion_rules_addr = base_addr;
	cmd.ipv4_expansion_rules_addr_shared = mem_type_shared;
	cmd.index_table_addr = base_addr;
	cmd.index_table_addr_shared = mem_type_shared;
	cmd.index_table_expansion_addr = base_addr;
	cmd.index_table_expansion_addr_shared = mem_type_shared;
	cmd.size_base_tables = 0;
	cmd.size_expansion_tables = 0;
	cmd.public_ip_addr = 0;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_V4_NAT_INIT, &cmd, false);
	if (!cmd_pyld) {
		IPAERR_RL("Fail to construct ip_v4_nat_init imm cmd\n");
		result = -EPERM;
		goto destroy_regwrt_imm_cmd;
	}
	desc[1].opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_IP_V4_NAT_INIT);
	desc[1].type = IPA_IMM_CMD_DESC;
	desc[1].callback = NULL;
	desc[1].user1 = NULL;
	desc[1].user2 = 0;
	desc[1].pyld = cmd_pyld->data;
	desc[1].len = cmd_pyld->len;

	if (ipa3_send_cmd(2, desc)) {
		IPAERR("Fail to send immediate command\n");
		result = -EPERM;
		goto destroy_imm_cmd;
	}

	ipa3_ctx->nat_mem.size_base_tables = 0;
	ipa3_ctx->nat_mem.size_expansion_tables = 0;
	ipa3_ctx->nat_mem.public_ip_addr = 0;
	ipa3_ctx->nat_mem.ipv4_rules_addr = 0;
	ipa3_ctx->nat_mem.ipv4_expansion_rules_addr = 0;
	ipa3_ctx->nat_mem.index_table_addr = 0;
	ipa3_ctx->nat_mem.index_table_expansion_addr = 0;

	ipa3_nat_free_mem_and_device(&ipa3_ctx->nat_mem);
	IPADBG("return\n");
	result = 0;

destroy_imm_cmd:
	ipahal_destroy_imm_cmd(cmd_pyld);
destroy_regwrt_imm_cmd:
	ipahal_destroy_imm_cmd(nop_cmd_pyld);
bail:
	return result;
}

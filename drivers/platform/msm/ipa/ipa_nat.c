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

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include "ipa_i.h"

#define IPA_NAT_PHYS_MEM_OFFSET  0
#define IPA_NAT_PHYS_MEM_SIZE  IPA_RAM_NAT_SIZE

#define IPA_NAT_SYSTEM_MEMORY  0
#define IPA_NAT_SHARED_MEMORY  1

static int ipa_nat_vma_fault_remap(
	 struct vm_area_struct *vma, struct vm_fault *vmf)
{
	IPADBG("\n");
	vmf->page = NULL;

	return VM_FAULT_SIGBUS;
}

/* VMA related file operations functions */
static struct vm_operations_struct ipa_nat_remap_vm_ops = {
	.fault = ipa_nat_vma_fault_remap,
};

static int ipa_nat_open(struct inode *inode, struct file *filp)
{
	struct ipa_nat_mem *nat_ctx;
	IPADBG("\n");
	nat_ctx = container_of(inode->i_cdev, struct ipa_nat_mem, cdev);
	filp->private_data = nat_ctx;
	IPADBG("return\n");
	return 0;
}

static int ipa_nat_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long vsize = vma->vm_end - vma->vm_start;
	struct ipa_nat_mem *nat_ctx = (struct ipa_nat_mem *)filp->private_data;
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
		IPADBG("map sz=0x%x\n", nat_ctx->size);
		result =
			dma_mmap_coherent(
				 NULL, vma,
				 nat_ctx->vaddr, nat_ctx->dma_handle,
				 nat_ctx->size);

		if (result) {
			IPAERR("unable to map memory. Err:%d\n", result);
			goto bail;
		}
		ipa_ctx->nat_mem.nat_base_address = nat_ctx->vaddr;
	} else {
		IPADBG("Mapping shared(local) memory\n");
		IPADBG("map sz=0x%lx\n", vsize);
		phys_addr = ipa_ctx->ipa_wrapper_base + IPA_REG_BASE_OFST +
			IPA_SRAM_DIRECT_ACCESS_N_OFST(IPA_NAT_PHYS_MEM_OFFSET);

		if (remap_pfn_range(
			 vma, vma->vm_start,
			 phys_addr >> PAGE_SHIFT, vsize, vma->vm_page_prot)) {
			IPAERR("remap failed\n");
			result = -EAGAIN;
			goto bail;
		}
		ipa_ctx->nat_mem.nat_base_address = (void *)vma->vm_start;
	}
	nat_ctx->is_mapped = true;
	vma->vm_ops = &ipa_nat_remap_vm_ops;
	IPADBG("return\n");
	result = 0;
bail:
	mutex_unlock(&nat_ctx->lock);
	return result;
}

static const struct file_operations ipa_nat_fops = {
	.owner = THIS_MODULE,
	.open = ipa_nat_open,
	.mmap = ipa_nat_mmap
};

/**
 * allocate_nat_device() - Allocates memory for the NAT device
 * @mem:	[in/out] memory parameters
 *
 * Called by NAT client driver to allocate memory for the NAT entries. Based on
 * the request size either shared or system memory will be used.
 *
 * Returns:	0 on success, negative on failure
 */
int allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem)
{
	struct ipa_nat_mem *nat_ctx = &(ipa_ctx->nat_mem);
	int gfp_flags = GFP_KERNEL | __GFP_ZERO;
	int result;

	IPADBG("passed memory size %d\n", mem->size);

	mutex_lock(&nat_ctx->lock);
	if (mem->size <= 0 || !strlen(mem->dev_name)
			|| nat_ctx->is_dev_init == true) {
		IPADBG("Invalid Parameters or device is already init\n");
		result = -EPERM;
		goto bail;
	}

	if (mem->size > IPA_NAT_PHYS_MEM_SIZE) {
		IPADBG("Allocating system memory\n");
		nat_ctx->is_sys_mem = true;
		nat_ctx->vaddr =
		   dma_alloc_coherent(NULL, mem->size, &nat_ctx->dma_handle,
				       gfp_flags);
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

	nat_ctx->class = class_create(THIS_MODULE, mem->dev_name);
	if (IS_ERR(nat_ctx->class)) {
		IPAERR("unable to create the class\n");
		result = -ENODEV;
		goto vaddr_alloc_fail;
	}
	result = alloc_chrdev_region(&nat_ctx->dev_num,
					0,
					1,
					mem->dev_name);
	if (result) {
		IPAERR("alloc_chrdev_region err.\n");
		result = -ENODEV;
		goto alloc_chrdev_region_fail;
	}

	nat_ctx->dev =
	   device_create(nat_ctx->class, NULL, nat_ctx->dev_num, nat_ctx,
			 mem->dev_name);

	if (IS_ERR(nat_ctx->dev)) {
		IPAERR("device_create err:%ld\n", PTR_ERR(nat_ctx->dev));
		result = -ENODEV;
		goto device_create_fail;
	}

	cdev_init(&nat_ctx->cdev, &ipa_nat_fops);
	nat_ctx->cdev.owner = THIS_MODULE;
	nat_ctx->cdev.ops = &ipa_nat_fops;

	result = cdev_add(&nat_ctx->cdev, nat_ctx->dev_num, 1);
	if (result) {
		IPAERR("cdev_add err=%d\n", -result);
		goto cdev_add_fail;
	}
	nat_ctx->is_dev_init = true;
	IPADBG("IPA NAT driver init successfully\n");
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
			 NULL, nat_ctx->size,
			 nat_ctx->vaddr, nat_ctx->dma_handle);
		nat_ctx->vaddr = NULL;
		nat_ctx->dma_handle = 0;
		nat_ctx->size = 0;
	}
bail:
	mutex_unlock(&nat_ctx->lock);

	return result;
}

/* IOCTL function handlers */
/**
 * ipa_nat_init_cmd() - Post IP_V4_NAT_INIT command to IPA HW
 * @init:	[in] initialization command attributes
 *
 * Called by NAT client driver to post IP_V4_NAT_INIT command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_nat_init_cmd(struct ipa_ioc_v4_nat_init *init)
{
	struct ipa_desc desc = { 0 };
	struct ipa_ip_v4_nat_init *cmd;
	u16 size = sizeof(struct ipa_ip_v4_nat_init);
	int result;

	IPADBG("\n");
	if (init->tbl_index < 0 || init->table_entries <= 0) {
		IPADBG("Table index or entries is zero\n");
		result = -EPERM;
		goto bail;
	}
	cmd = kmalloc(size, GFP_KERNEL);
	if (!cmd) {
		IPAERR("Failed to alloc immediate command object\n");
		result = -ENOMEM;
		goto bail;
	}
	if (ipa_ctx->nat_mem.vaddr) {
		IPADBG("using system memory for nat table\n");
		cmd->ipv4_rules_addr_type = IPA_NAT_SYSTEM_MEMORY;
		cmd->ipv4_expansion_rules_addr_type = IPA_NAT_SYSTEM_MEMORY;
		cmd->index_table_addr_type = IPA_NAT_SYSTEM_MEMORY;
		cmd->index_table_expansion_addr_type = IPA_NAT_SYSTEM_MEMORY;

		cmd->ipv4_rules_addr =
			ipa_ctx->nat_mem.dma_handle + init->ipv4_rules_offset;
		IPADBG("ipv4_rules_offset:0x%x\n", init->ipv4_rules_offset);

		cmd->ipv4_expansion_rules_addr =
		   ipa_ctx->nat_mem.dma_handle + init->expn_rules_offset;
		IPADBG("expn_rules_offset:0x%x\n", init->expn_rules_offset);

		cmd->index_table_addr =
			ipa_ctx->nat_mem.dma_handle + init->index_offset;
		IPADBG("index_offset:0x%x\n", init->index_offset);

		cmd->index_table_expansion_addr =
		   ipa_ctx->nat_mem.dma_handle + init->index_expn_offset;
		IPADBG("index_expn_offset:0x%x\n", init->index_expn_offset);
	} else {
		IPADBG("using shared(local) memory for nat table\n");
		cmd->ipv4_rules_addr_type = IPA_NAT_SHARED_MEMORY;
		cmd->ipv4_expansion_rules_addr_type = IPA_NAT_SHARED_MEMORY;
		cmd->index_table_addr_type = IPA_NAT_SHARED_MEMORY;
		cmd->index_table_expansion_addr_type = IPA_NAT_SHARED_MEMORY;

		cmd->ipv4_rules_addr = init->ipv4_rules_offset +
				ipa_ctx->ctrl->sram_nat_ipv4_ofst;

		cmd->ipv4_expansion_rules_addr = init->expn_rules_offset +
				ipa_ctx->ctrl->sram_nat_ipv4_ofst;

		cmd->index_table_addr = init->index_offset  +
				ipa_ctx->ctrl->sram_nat_ipv4_ofst;

		cmd->index_table_expansion_addr = init->index_expn_offset +
				ipa_ctx->ctrl->sram_nat_ipv4_ofst;
	}
	cmd->table_index = init->tbl_index;
	IPADBG("Table index:0x%x\n", cmd->table_index);
	cmd->size_base_tables = init->table_entries;
	IPADBG("Base Table size:0x%x\n", cmd->size_base_tables);
	cmd->size_expansion_tables = init->expn_table_entries;
	IPADBG("Expansion Table size:0x%x\n", cmd->size_expansion_tables);
	cmd->public_ip_addr = init->ip_addr;
	IPADBG("Public ip address:0x%x\n", cmd->public_ip_addr);
	desc.opcode = IPA_IP_V4_NAT_INIT;
	desc.type = IPA_IMM_CMD_DESC;
	desc.callback = NULL;
	desc.user1 = NULL;
	desc.user2 = NULL;
	desc.pyld = (void *)cmd;
	desc.len = size;
	IPADBG("posting v4 init command\n");
	if (ipa_send_cmd(1, &desc)) {
		IPAERR("Fail to send immediate command\n");
		result = -EPERM;
		goto free_cmd;
	}

	ipa_ctx->nat_mem.public_ip_addr = init->ip_addr;
	IPADBG("Table ip address:0x%x", ipa_ctx->nat_mem.public_ip_addr);

	ipa_ctx->nat_mem.ipv4_rules_addr =
	 (char *)ipa_ctx->nat_mem.nat_base_address + init->ipv4_rules_offset;
	IPADBG("ipv4_rules_addr: 0x%p\n",
				 ipa_ctx->nat_mem.ipv4_rules_addr);

	ipa_ctx->nat_mem.ipv4_expansion_rules_addr =
	 (char *)ipa_ctx->nat_mem.nat_base_address + init->expn_rules_offset;
	IPADBG("ipv4_expansion_rules_addr: 0x%p\n",
				 ipa_ctx->nat_mem.ipv4_expansion_rules_addr);

	ipa_ctx->nat_mem.index_table_addr =
		 (char *)ipa_ctx->nat_mem.nat_base_address + init->index_offset;
	IPADBG("index_table_addr: 0x%p\n",
				 ipa_ctx->nat_mem.index_table_addr);

	ipa_ctx->nat_mem.index_table_expansion_addr =
	 (char *)ipa_ctx->nat_mem.nat_base_address + init->index_expn_offset;
	IPADBG("index_table_expansion_addr: 0x%p\n",
				 ipa_ctx->nat_mem.index_table_expansion_addr);

	IPADBG("size_base_tables: %d\n", init->table_entries);
	ipa_ctx->nat_mem.size_base_tables  = init->table_entries;

	IPADBG("size_expansion_tables: %d\n", init->expn_table_entries);
	ipa_ctx->nat_mem.size_expansion_tables = init->expn_table_entries;

	IPADBG("return\n");
	result = 0;
free_cmd:
	kfree(cmd);
bail:
	return result;
}

/**
 * ipa_nat_dma_cmd() - Post NAT_DMA command to IPA HW
 * @dma:	[in] initialization command attributes
 *
 * Called by NAT client driver to post NAT_DMA command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma)
{
	struct ipa_nat_dma *cmd = NULL;
	struct ipa_desc *desc = NULL;
	u16 size = 0, cnt = 0;
	int ret = 0;

	IPADBG("\n");
	if (dma->entries <= 0) {
		IPADBG("Invalid number of commands\n");
		ret = -EPERM;
		goto bail;
	}
	size = sizeof(struct ipa_desc) * dma->entries;
	desc = kmalloc(size, GFP_KERNEL);
	if (desc == NULL) {
		IPAERR("Failed to alloc memory\n");
		ret = -ENOMEM;
		goto bail;
	}
	size = sizeof(struct ipa_nat_dma) * dma->entries;
	cmd = kmalloc(size, GFP_KERNEL);
	if (cmd == NULL) {
		IPAERR("Failed to alloc memory\n");
		ret = -ENOMEM;
		goto bail;
	}
	for (cnt = 0; cnt < dma->entries; cnt++) {
		cmd[cnt].table_index = dma->dma[cnt].table_index;
		cmd[cnt].base_addr = dma->dma[cnt].base_addr;
		cmd[cnt].offset = dma->dma[cnt].offset;
		cmd[cnt].data = dma->dma[cnt].data;
		desc[cnt].type = IPA_IMM_CMD_DESC;
		desc[cnt].opcode = IPA_NAT_DMA;
		desc[cnt].callback = NULL;
		desc[cnt].user1 = NULL;

		desc[cnt].user2 = NULL;

		desc[cnt].len = sizeof(struct ipa_nat_dma);
		desc[cnt].pyld = (void *)&cmd[cnt];

		ret = ipa_send_cmd(1, &desc[cnt]);
		if (ret == -EPERM)
			IPAERR("Fail to send immediate command %d\n", cnt);
	}

bail:
	kfree(cmd);
	kfree(desc);

	return ret;
}

/**
 * ipa_nat_free_mem_and_device() - free the NAT memory and remove the device
 * @nat_ctx:	[in] the IPA NAT memory to free
 *
 * Called by NAT client driver to free the NAT memory and remove the device
 */
void ipa_nat_free_mem_and_device(struct ipa_nat_mem *nat_ctx)
{
	IPADBG("\n");
	mutex_lock(&nat_ctx->lock);

	if (nat_ctx->is_sys_mem) {
		IPADBG("freeing the dma memory\n");
		dma_free_coherent(
			 NULL, nat_ctx->size,
			 nat_ctx->vaddr, nat_ctx->dma_handle);
		nat_ctx->size = 0;
		nat_ctx->vaddr = NULL;
	}
	nat_ctx->is_mapped = false;
	nat_ctx->is_sys_mem = false;
	cdev_del(&nat_ctx->cdev);
	device_destroy(nat_ctx->class, nat_ctx->dev_num);
	unregister_chrdev_region(nat_ctx->dev_num, 1);
	class_destroy(nat_ctx->class);
	nat_ctx->is_dev_init = false;

	mutex_unlock(&nat_ctx->lock);
	IPADBG("return\n");
	return;
}

/**
 * ipa_nat_del_cmd() - Delete a NAT table
 * @del:	[in] delete table table table parameters
 *
 * Called by NAT client driver to delete the nat table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_nat_del_cmd(struct ipa_ioc_v4_nat_del *del)
{
	struct ipa_desc desc = { 0 };
	struct ipa_ip_v4_nat_init *cmd;
	u16 size = sizeof(struct ipa_ip_v4_nat_init);
	u8 mem_type = IPA_NAT_SHARED_MEMORY;
	u32 base_addr = IPA_NAT_PHYS_MEM_OFFSET;
	int result;

	IPADBG("\n");
	if (del->table_index < 0 || del->public_ip_addr == 0) {
		IPADBG("Bad Parameter\n");
		result = -EPERM;
		goto bail;
	}
	cmd = kmalloc(size, GFP_KERNEL);
	if (cmd == NULL) {
		IPAERR("Failed to alloc immediate command object\n");
		result = -ENOMEM;
		goto bail;
	}
	cmd->table_index = del->table_index;
	cmd->ipv4_rules_addr = base_addr;
	cmd->ipv4_rules_addr_type = mem_type;
	cmd->ipv4_expansion_rules_addr = base_addr;
	cmd->ipv4_expansion_rules_addr_type = mem_type;
	cmd->index_table_addr = base_addr;
	cmd->index_table_addr_type = mem_type;
	cmd->index_table_expansion_addr = base_addr;
	cmd->index_table_expansion_addr_type = mem_type;
	cmd->size_base_tables = 0;
	cmd->size_expansion_tables = 0;
	cmd->public_ip_addr = del->public_ip_addr;

	desc.opcode = IPA_IP_V4_NAT_INIT;
	desc.type = IPA_IMM_CMD_DESC;
	desc.callback = NULL;
	desc.user1 = NULL;
	desc.user2 = NULL;
	desc.pyld = (void *)cmd;
	desc.len = size;
	if (ipa_send_cmd(1, &desc)) {
		IPAERR("Fail to send immediate command\n");
		result = -EPERM;
		goto free_mem;
	}

	ipa_nat_free_mem_and_device(&ipa_ctx->nat_mem);
	IPADBG("return\n");
	result = 0;
free_mem:
	kfree(cmd);
bail:
	return result;
}

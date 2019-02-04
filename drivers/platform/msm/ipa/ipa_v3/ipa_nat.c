/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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
#include "ipahal/ipahal_nat.h"

/*
 * The following for adding code (ie. for EMULATION) not found on x86.
 */
#if defined(CONFIG_IPA_EMULATION)
# include "ipa_emulation_stubs.h"
#endif

#define IPA_NAT_PHYS_MEM_OFFSET  0
#define IPA_IPV6CT_PHYS_MEM_OFFSET  0
#define IPA_NAT_PHYS_MEM_SIZE  IPA_RAM_NAT_SIZE
#define IPA_IPV6CT_PHYS_MEM_SIZE  IPA_RAM_IPV6CT_SIZE

#define IPA_NAT_IPV6CT_TEMP_MEM_SIZE 128

#define IPA_NAT_MAX_NUM_OF_INIT_CMD_DESC 3
#define IPA_IPV6CT_MAX_NUM_OF_INIT_CMD_DESC 2
#define IPA_MAX_NUM_OF_TABLE_DMA_CMD_DESC 4

/*
 * The base table max entries is limited by index into table 13 bits number.
 * Limit the memory size required by user to prevent kernel memory starvation
 */
#define IPA_TABLE_MAX_ENTRIES 8192
#define MAX_ALLOC_NAT_SIZE(size) (IPA_TABLE_MAX_ENTRIES * size)

enum ipa_nat_ipv6ct_table_type {
	IPA_NAT_BASE_TBL = 0,
	IPA_NAT_EXPN_TBL = 1,
	IPA_NAT_INDX_TBL = 2,
	IPA_NAT_INDEX_EXPN_TBL = 3,
	IPA_IPV6CT_BASE_TBL = 4,
	IPA_IPV6CT_EXPN_TBL = 5
};

static int ipa3_nat_ipv6ct_vma_fault_remap(struct vm_fault *vmf)
{
	vmf->page = NULL;

	IPADBG("\n");
	return VM_FAULT_SIGBUS;
}

/* VMA related file operations functions */
static const struct vm_operations_struct ipa3_nat_ipv6ct_remap_vm_ops = {
	.fault = ipa3_nat_ipv6ct_vma_fault_remap,
};

static int ipa3_nat_ipv6ct_open(struct inode *inode, struct file *filp)
{
	struct ipa3_nat_ipv6ct_common_mem *dev;

	IPADBG("\n");
	dev = container_of(inode->i_cdev,
		struct ipa3_nat_ipv6ct_common_mem, cdev);
	filp->private_data = dev;
	IPADBG("return\n");

	return 0;
}

static int ipa3_nat_ipv6ct_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct ipa3_nat_ipv6ct_common_mem *dev =
		(struct ipa3_nat_ipv6ct_common_mem *)filp->private_data;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long phys_addr;
	int result = 0;
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_AP);

	IPADBG("\n");

	if (!dev->is_dev_init) {
		IPAERR("attempt to mmap %s before dev init\n", dev->name);
		return -EPERM;
	}

	mutex_lock(&dev->lock);
	if (!dev->is_mem_allocated) {
		IPAERR_RL("attempt to mmap %s before the memory allocation\n",
			dev->name);
		result = -EPERM;
		goto bail;
	}

	if (dev->is_sys_mem) {
		if (dev->is_mapped) {
			IPAERR("%s already mapped, only 1 mapping supported\n",
				dev->name);
			result = -EINVAL;
			goto bail;
		}
	} else {
		if ((dev->phys_mem_size == 0) || (vsize > dev->phys_mem_size)) {
			IPAERR_RL("wrong parameters to %s mapping\n",
				dev->name);
			result = -EINVAL;
			goto bail;
		}
	}
	/* check if smmu enable & dma_coherent mode */
	if (!cb->valid ||
		!is_device_dma_coherent(cb->dev)) {
		vma->vm_page_prot =
		pgprot_noncached(vma->vm_page_prot);
		IPADBG("App smmu enable in DMA mode\n");
	}

	if (dev->is_sys_mem) {
		IPADBG("Mapping system memory\n");
		IPADBG("map sz=0x%zx\n", dev->size);
		result =
			dma_mmap_coherent(
				ipa3_ctx->pdev, vma,
				dev->vaddr, dev->dma_handle,
				dev->size);
		if (result) {
			IPAERR("unable to map memory. Err:%d\n", result);
			goto bail;
		}
		dev->base_address = dev->vaddr;
	} else {
		IPADBG("Mapping shared(local) memory\n");
		IPADBG("map sz=0x%lx\n", vsize);

		phys_addr = ipa3_ctx->ipa_wrapper_base +
			ipa3_ctx->ctrl->ipa_reg_base_ofst +
			ipahal_get_reg_n_ofst(IPA_SW_AREA_RAM_DIRECT_ACCESS_n,
				dev->smem_offset);

		if (remap_pfn_range(
			vma, vma->vm_start,
			phys_addr >> PAGE_SHIFT, vsize, vma->vm_page_prot)) {
			IPAERR("remap failed\n");
			result = -EAGAIN;
			goto bail;
		}
		dev->base_address = (void *)vma->vm_start;
	}
	result = 0;
	vma->vm_ops = &ipa3_nat_ipv6ct_remap_vm_ops;
	dev->is_mapped = true;
	IPADBG("return\n");
bail:
	mutex_unlock(&dev->lock);
	return result;
}

static const struct file_operations ipa3_nat_ipv6ct_fops = {
	.owner = THIS_MODULE,
	.open = ipa3_nat_ipv6ct_open,
	.mmap = ipa3_nat_ipv6ct_mmap
};

/**
 * ipa3_allocate_nat_ipv6ct_tmp_memory() - Allocates the NAT\IPv6CT temp memory
 */
static struct ipa3_nat_ipv6ct_tmp_mem *ipa3_nat_ipv6ct_allocate_tmp_memory(void)
{
	struct ipa3_nat_ipv6ct_tmp_mem *tmp_mem;
	gfp_t gfp_flags = GFP_KERNEL | __GFP_ZERO;

	IPADBG("\n");

	tmp_mem = kzalloc(sizeof(*tmp_mem), GFP_KERNEL);
	if (tmp_mem == NULL)
		return NULL;

	tmp_mem->vaddr =
		dma_alloc_coherent(ipa3_ctx->pdev, IPA_NAT_IPV6CT_TEMP_MEM_SIZE,
			&tmp_mem->dma_handle, gfp_flags);
	if (tmp_mem->vaddr == NULL)
		goto bail_tmp_mem;

	IPADBG("IPA successfully allocated temp memory\n");
	return tmp_mem;

bail_tmp_mem:
	kfree(tmp_mem);
	return NULL;
}

static int ipa3_nat_ipv6ct_init_device(
	struct ipa3_nat_ipv6ct_common_mem *dev,
	const char *name,
	u32 phys_mem_size,
	u32 smem_offset,
	struct ipa3_nat_ipv6ct_tmp_mem *tmp_mem)
{
	int result;

	IPADBG("Init %s\n", name);

	if (strnlen(name, IPA_DEV_NAME_MAX_LEN) == IPA_DEV_NAME_MAX_LEN) {
		IPAERR("device name is too long\n");
		return -ENODEV;
	}

	strlcpy(dev->name, name, IPA_DEV_NAME_MAX_LEN);

	dev->class = class_create(THIS_MODULE, name);
	if (IS_ERR(dev->class)) {
		IPAERR("unable to create the class for %s\n", name);
		return -ENODEV;
	}
	result = alloc_chrdev_region(&dev->dev_num, 0, 1, name);
	if (result) {
		IPAERR("alloc_chrdev_region err. for %s\n", name);
		result = -ENODEV;
		goto alloc_chrdev_region_fail;
	}

	dev->dev = device_create(dev->class, NULL, dev->dev_num, NULL, name);

	if (IS_ERR(dev->dev)) {
		IPAERR("device_create err:%ld\n", PTR_ERR(dev->dev));
		result = -ENODEV;
		goto device_create_fail;
	}

	cdev_init(&dev->cdev, &ipa3_nat_ipv6ct_fops);
	dev->cdev.owner = THIS_MODULE;

	mutex_init(&dev->lock);
	mutex_lock(&dev->lock);

	result = cdev_add(&dev->cdev, dev->dev_num, 1);
	if (result) {
		IPAERR("cdev_add err=%d\n", -result);
		goto cdev_add_fail;
	}

	dev->phys_mem_size = phys_mem_size;
	dev->smem_offset = smem_offset;

	dev->is_dev_init = true;
	dev->tmp_mem = tmp_mem;
	mutex_unlock(&dev->lock);

	IPADBG("ipa dev %s added successful. major:%d minor:%d\n", name,
		MAJOR(dev->dev_num), MINOR(dev->dev_num));
	return 0;

cdev_add_fail:
	mutex_unlock(&dev->lock);
	device_destroy(dev->class, dev->dev_num);
device_create_fail:
	unregister_chrdev_region(dev->dev_num, 1);
alloc_chrdev_region_fail:
	class_destroy(dev->class);
	return result;
}

static void ipa3_nat_ipv6ct_destroy_device(
	struct ipa3_nat_ipv6ct_common_mem *dev)
{
	IPADBG("\n");

	mutex_lock(&dev->lock);

	if (dev->tmp_mem != NULL &&
		ipa3_ctx->nat_mem.is_tmp_mem_allocated == false) {
		dev->tmp_mem = NULL;
	} else if (dev->tmp_mem != NULL &&
		ipa3_ctx->nat_mem.is_tmp_mem_allocated) {
		dma_free_coherent(ipa3_ctx->pdev, IPA_NAT_IPV6CT_TEMP_MEM_SIZE,
			dev->tmp_mem->vaddr, dev->tmp_mem->dma_handle);
		kfree(dev->tmp_mem);
		dev->tmp_mem = NULL;
		ipa3_ctx->nat_mem.is_tmp_mem_allocated = false;
	}
	device_destroy(dev->class, dev->dev_num);
	unregister_chrdev_region(dev->dev_num, 1);
	class_destroy(dev->class);
	dev->is_dev_init = false;
	mutex_unlock(&dev->lock);

	IPADBG("return\n");
}

/**
 * ipa3_nat_ipv6ct_init_devices() - Initialize the NAT and IPv6CT devices
 *
 * Called during IPA init to create memory device
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_nat_ipv6ct_init_devices(void)
{
	struct ipa3_nat_ipv6ct_tmp_mem *tmp_mem;
	int result;

	IPADBG("\n");

	/*
	 * Allocate NAT/IPv6CT temporary memory. The memory is never deleted,
	 * because provided to HW once NAT or IPv6CT table is deleted.
	 */
	tmp_mem = ipa3_nat_ipv6ct_allocate_tmp_memory();

	if (tmp_mem == NULL) {
		IPAERR("unable to allocate tmp_mem\n");
		return -ENOMEM;
	}
	ipa3_ctx->nat_mem.is_tmp_mem_allocated = true;

	if (ipa3_nat_ipv6ct_init_device(
		&ipa3_ctx->nat_mem.dev,
		IPA_NAT_DEV_NAME,
		IPA_NAT_PHYS_MEM_SIZE,
		IPA_NAT_PHYS_MEM_OFFSET,
		tmp_mem)) {
		IPAERR("unable to create nat device\n");
		result = -ENODEV;
		goto fail_init_nat_dev;
	}

	if ((ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) &&
		ipa3_nat_ipv6ct_init_device(
			&ipa3_ctx->ipv6ct_mem.dev,
			IPA_IPV6CT_DEV_NAME,
			IPA_IPV6CT_PHYS_MEM_SIZE,
			IPA_IPV6CT_PHYS_MEM_OFFSET,
			tmp_mem)) {
		IPAERR("unable to create IPv6CT device\n");
		result = -ENODEV;
		goto fail_init_ipv6ct_dev;
	}

	return 0;

fail_init_ipv6ct_dev:
	ipa3_nat_ipv6ct_destroy_device(&ipa3_ctx->nat_mem.dev);
fail_init_nat_dev:
	if (tmp_mem != NULL && ipa3_ctx->nat_mem.is_tmp_mem_allocated) {
		dma_free_coherent(ipa3_ctx->pdev, IPA_NAT_IPV6CT_TEMP_MEM_SIZE,
			tmp_mem->vaddr, tmp_mem->dma_handle);
		kfree(tmp_mem);
		ipa3_ctx->nat_mem.is_tmp_mem_allocated = false;
	}
	return result;
}

/**
 * ipa3_nat_ipv6ct_destroy_devices() - destroy the NAT and IPv6CT devices
 *
 * Called during IPA init to destroy nat device
 */
void ipa3_nat_ipv6ct_destroy_devices(void)
{
	ipa3_nat_ipv6ct_destroy_device(&ipa3_ctx->nat_mem.dev);
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		ipa3_nat_ipv6ct_destroy_device(&ipa3_ctx->ipv6ct_mem.dev);
}

static int ipa3_nat_ipv6ct_allocate_mem(struct ipa3_nat_ipv6ct_common_mem *dev,
	struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc,
	enum ipahal_nat_type nat_type)
{
	gfp_t gfp_flags = GFP_KERNEL | __GFP_ZERO;
	int result = 0;
	size_t nat_entry_size;

	IPADBG("passed memory size %zu for %s\n",
		table_alloc->size, dev->name);

	if (!dev->is_dev_init) {
		IPAERR("%s hasn't been initialized\n", dev->name);
		result = -EPERM;
		goto bail;
	}

	if (dev->is_mem_allocated) {
		IPAERR("Memory already allocated\n");
		result = -EPERM;
		goto bail;
	}

	ipahal_nat_entry_size(nat_type, &nat_entry_size);
	if (table_alloc->size > MAX_ALLOC_NAT_SIZE(nat_entry_size)) {
		IPAERR("Trying allocate more size = %zu, Max allowed = %zu\n",
				table_alloc->size,
				MAX_ALLOC_NAT_SIZE(nat_entry_size));
		result = -EPERM;
		goto bail;
	}

	if (!table_alloc->size) {
		IPAERR_RL("Invalid Parameters\n");
		result = -EPERM;
		goto bail;
	}

	if (table_alloc->size > IPA_NAT_PHYS_MEM_SIZE) {
		IPADBG("Allocating system memory\n");
		dev->is_sys_mem = true;
		dev->vaddr =
		   dma_alloc_coherent(ipa3_ctx->pdev, table_alloc->size,
			   &dev->dma_handle, gfp_flags);
		if (dev->vaddr == NULL) {
			IPAERR("memory alloc failed\n");
			result = -ENOMEM;
			goto bail;
		}
		dev->size = table_alloc->size;
	} else {
		IPADBG("using shared(local) memory\n");
		dev->is_sys_mem = false;
	}

	IPADBG("return\n");

bail:
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
	int result;
	struct ipa_ioc_nat_ipv6ct_table_alloc tmp;

	tmp.size = mem->size;
	tmp.offset = 0;

	result = ipa3_allocate_nat_table(&tmp);
	if (result)
		goto bail;

	mem->offset = tmp.offset;

bail:
	return result;
}

/**
 * ipa3_allocate_nat_table() - Allocates memory for the NAT table
 * @table_alloc: [in/out] memory parameters
 *
 * Called by NAT client to allocate memory for the table entries.
 * Based on the request size either shared or system memory will be used.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_allocate_nat_table(struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc)
{
	struct ipa3_nat_mem *nat_ctx = &(ipa3_ctx->nat_mem);
	gfp_t gfp_flags = GFP_KERNEL | __GFP_ZERO;
	int result;

	IPADBG("\n");

	mutex_lock(&nat_ctx->dev.lock);

	result = ipa3_nat_ipv6ct_allocate_mem(&nat_ctx->dev, table_alloc,
							IPAHAL_NAT_IPV4);
	if (result)
		goto bail;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		size_t pdn_entry_size;
		struct ipa_mem_buffer *pdn_mem = &nat_ctx->pdn_mem;

		ipahal_nat_entry_size(IPAHAL_NAT_IPV4_PDN, &pdn_entry_size);
		pdn_mem->size = pdn_entry_size * IPA_MAX_PDN_NUM;
		if (IPA_MEM_PART(pdn_config_size) < pdn_mem->size) {
			IPAERR(
				"number of PDN entries exceeds SRAM available space\n");
			result = -ENOMEM;
			goto fail_alloc_pdn;
		}

		pdn_mem->base = dma_alloc_coherent(ipa3_ctx->pdev,
			pdn_mem->size,
			&pdn_mem->phys_base,
			gfp_flags);
		if (!pdn_mem->base) {
			IPAERR("fail to allocate PDN memory\n");
			result = -ENOMEM;
			goto fail_alloc_pdn;
		}
		memset(pdn_mem->base, 0, pdn_mem->size);
		IPADBG("IPA NAT dev allocated PDN memory successfully\n");
	}

	nat_ctx->dev.is_mem_allocated = true;
	IPADBG("IPA NAT dev init successfully\n");
	mutex_unlock(&nat_ctx->dev.lock);

	IPADBG("return\n");

	return 0;

fail_alloc_pdn:
	if (nat_ctx->dev.vaddr) {
		dma_free_coherent(ipa3_ctx->pdev, table_alloc->size,
			nat_ctx->dev.vaddr, nat_ctx->dev.dma_handle);
		nat_ctx->dev.vaddr = NULL;
	}
bail:
	mutex_unlock(&nat_ctx->dev.lock);

	return result;
}

/**
 * ipa3_allocate_ipv6ct_table() - Allocates memory for the IPv6CT table
 * @table_alloc: [in/out] memory parameters
 *
 * Called by IPv6CT client to allocate memory for the table entries.
 * Based on the request size either shared or system memory will be used.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_allocate_ipv6ct_table(
	struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc)
{
	int result;

	IPADBG("\n");

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		IPAERR_RL("IPv6 connection tracking isn't supported\n");
		return -EPERM;
	}

	mutex_lock(&ipa3_ctx->ipv6ct_mem.dev.lock);

	result = ipa3_nat_ipv6ct_allocate_mem(
		&ipa3_ctx->ipv6ct_mem.dev, table_alloc, IPAHAL_NAT_IPV6CT);
	if (result)
		goto bail;

	ipa3_ctx->ipv6ct_mem.dev.is_mem_allocated = true;
	IPADBG("IPA IPv6CT dev init successfully\n");

bail:
	mutex_unlock(&ipa3_ctx->ipv6ct_mem.dev.lock);
	return result;
}

static int ipa3_nat_ipv6ct_check_table_params(
	struct ipa3_nat_ipv6ct_common_mem *dev,
	uint32_t offset, uint16_t entries_num,
	enum ipahal_nat_type nat_type)
{
	int result;
	size_t entry_size, table_size;

	result = ipahal_nat_entry_size(nat_type, &entry_size);
	if (result) {
		IPAERR("Failed to retrieve size of entry for %s\n",
			ipahal_nat_type_str(nat_type));
		return result;
	}
	table_size = entry_size * entries_num;

	/* check for integer overflow */
	if (offset > UINT_MAX - table_size) {
		IPAERR_RL("Detected overflow\n");
		return -EPERM;
	}

	/* Check offset is not beyond allocated size */
	if (dev->size < offset + table_size) {
		IPAERR_RL("Table offset not valid\n");
		IPAERR_RL("offset:%d entries:%d table_size:%zu mem_size:%zu\n",
			offset, entries_num, table_size, dev->size);
		return -EPERM;
	}

	if (dev->is_sys_mem && offset > UINT_MAX - dev->dma_handle) {
		IPAERR_RL("Failed due to integer overflow\n");
		IPAERR_RL("%s dma_handle: 0x%pa offset: 0x%x\n",
			dev->name, &dev->dma_handle, offset);
		return -EPERM;
	}

	return 0;
}

static inline void ipa3_nat_ipv6ct_create_init_cmd(
	struct ipahal_imm_cmd_nat_ipv6ct_init_common *table_init_cmd,
	bool is_shared,
	dma_addr_t base_addr,
	uint8_t tbl_index,
	uint32_t base_table_offset,
	uint32_t expn_table_offset,
	uint16_t table_entries,
	uint16_t expn_table_entries,
	const char *table_name)
{
	table_init_cmd->base_table_addr_shared = is_shared;
	table_init_cmd->expansion_table_addr_shared = is_shared;

	table_init_cmd->base_table_addr = base_addr + base_table_offset;
	IPADBG("%s base table offset:0x%x\n", table_name, base_table_offset);

	table_init_cmd->expansion_table_addr = base_addr + expn_table_offset;
	IPADBG("%s expn table offset:0x%x\n", table_name, expn_table_offset);

	table_init_cmd->table_index = tbl_index;
	IPADBG("%s table index:0x%x\n", table_name, tbl_index);

	table_init_cmd->size_base_table = table_entries;
	IPADBG("%s base table size:0x%x\n", table_name, table_entries);

	table_init_cmd->size_expansion_table = expn_table_entries;
	IPADBG("%s expansion table size:0x%x\n",
		table_name, expn_table_entries);
}

static inline void ipa3_nat_ipv6ct_init_device_structure(
	struct ipa3_nat_ipv6ct_common_mem *dev,
	uint32_t base_table_offset,
	uint32_t expn_table_offset,
	uint16_t table_entries,
	uint16_t expn_table_entries)
{
	dev->base_table_addr = (char *)dev->base_address + base_table_offset;
	IPADBG("%s base_table_addr: 0x%pK\n", dev->name, dev->base_table_addr);

	dev->expansion_table_addr =
		(char *)dev->base_address + expn_table_offset;
	IPADBG("%s expansion_table_addr: 0x%pK\n",
		dev->name, dev->expansion_table_addr);

	IPADBG("%s table_entries: %d\n", dev->name, table_entries);
	dev->table_entries = table_entries;

	IPADBG("%s expn_table_entries: %d\n", dev->name, expn_table_entries);
	dev->expn_table_entries = expn_table_entries;
}

static void ipa3_nat_create_init_cmd(
	struct ipa_ioc_v4_nat_init *init,
	bool is_shared,
	dma_addr_t base_addr,
	struct ipahal_imm_cmd_ip_v4_nat_init *cmd)
{
	IPADBG("\n");

	ipa3_nat_ipv6ct_create_init_cmd(
		&cmd->table_init,
		is_shared,
		base_addr,
		init->tbl_index,
		init->ipv4_rules_offset,
		init->expn_rules_offset,
		init->table_entries,
		init->expn_table_entries,
		ipa3_ctx->nat_mem.dev.name);

	cmd->index_table_addr_shared = is_shared;
	cmd->index_table_expansion_addr_shared = is_shared;

	cmd->index_table_addr =
		base_addr + init->index_offset;
	IPADBG("index_offset:0x%x\n", init->index_offset);

	cmd->index_table_expansion_addr =
		base_addr + init->index_expn_offset;
	IPADBG("index_expn_offset:0x%x\n", init->index_expn_offset);
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		/*
		 * starting IPAv4.0 public ip field changed to store the
		 * PDN config table offset in SMEM
		 */
		cmd->public_addr_info = IPA_MEM_PART(pdn_config_ofst);
		IPADBG("pdn config base:0x%x\n", cmd->public_addr_info);
	} else {
		cmd->public_addr_info = init->ip_addr;
		IPADBG("Public IP address:%pI4h\n", &cmd->public_addr_info);
	}

	IPADBG("return\n");
}

static void ipa3_nat_create_modify_pdn_cmd(
	struct ipahal_imm_cmd_dma_shared_mem *mem_cmd, bool zero_mem)
{
	size_t pdn_entry_size, mem_size;

	IPADBG("\n");

	ipahal_nat_entry_size(IPAHAL_NAT_IPV4_PDN, &pdn_entry_size);
	mem_size = pdn_entry_size * IPA_MAX_PDN_NUM;

	if (zero_mem)
		memset(ipa3_ctx->nat_mem.pdn_mem.base, 0, mem_size);

	/* Copy the PDN config table to SRAM */
	mem_cmd->is_read = false;
	mem_cmd->skip_pipeline_clear = false;
	mem_cmd->pipeline_clear_options = IPAHAL_HPS_CLEAR;
	mem_cmd->size = mem_size;
	mem_cmd->system_addr = ipa3_ctx->nat_mem.pdn_mem.phys_base;
	mem_cmd->local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(pdn_config_ofst);

	IPADBG("return\n");
}

static int ipa3_nat_send_init_cmd(struct ipahal_imm_cmd_ip_v4_nat_init *cmd,
	bool zero_pdn_table)
{
	struct ipa3_desc desc[IPA_NAT_MAX_NUM_OF_INIT_CMD_DESC];
	struct ipahal_imm_cmd_pyld *cmd_pyld[IPA_NAT_MAX_NUM_OF_INIT_CMD_DESC];
	int i, num_cmd = 0, result;

	IPADBG("\n");

	/* NO-OP IC for ensuring that IPA pipeline is empty */
	cmd_pyld[num_cmd] =
		ipahal_construct_nop_imm_cmd(false, IPAHAL_HPS_CLEAR, false);
	if (!cmd_pyld[num_cmd]) {
		IPAERR("failed to construct NOP imm cmd\n");
		return -ENOMEM;
	}

	ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
	++num_cmd;

	cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_V4_NAT_INIT, cmd, false);
	if (!cmd_pyld[num_cmd]) {
		IPAERR_RL("fail to construct NAT init imm cmd\n");
		result = -EPERM;
		goto destroy_imm_cmd;
	}

	ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
	++num_cmd;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		struct ipahal_imm_cmd_dma_shared_mem mem_cmd = { 0 };

		if (num_cmd >= IPA_NAT_MAX_NUM_OF_INIT_CMD_DESC) {
			IPAERR("number of commands is out of range\n");
			result = -ENOBUFS;
			goto destroy_imm_cmd;
		}

		/* Copy the PDN config table to SRAM */
		ipa3_nat_create_modify_pdn_cmd(&mem_cmd, zero_pdn_table);
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR(
				"fail construct dma_shared_mem cmd: for pdn table");
			result = -ENOMEM;
			goto destroy_imm_cmd;
		}
		ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
		++num_cmd;
		IPADBG("added PDN table copy cmd\n");
	}

	result = ipa3_send_cmd(num_cmd, desc);
	if (result) {
		IPAERR("fail to send NAT init immediate command\n");
		goto destroy_imm_cmd;
	}

	IPADBG("return\n");

destroy_imm_cmd:
	for (i = 0; i < num_cmd; ++i)
		ipahal_destroy_imm_cmd(cmd_pyld[i]);

	return result;
}

static int ipa3_ipv6ct_send_init_cmd(struct ipahal_imm_cmd_ip_v6_ct_init *cmd)
{
	struct ipa3_desc desc[IPA_IPV6CT_MAX_NUM_OF_INIT_CMD_DESC];
	struct ipahal_imm_cmd_pyld
		*cmd_pyld[IPA_IPV6CT_MAX_NUM_OF_INIT_CMD_DESC];
	int i, num_cmd = 0, result;

	IPADBG("\n");

	/* NO-OP IC for ensuring that IPA pipeline is empty */
	cmd_pyld[num_cmd] =
		ipahal_construct_nop_imm_cmd(false, IPAHAL_HPS_CLEAR, false);
	if (!cmd_pyld[num_cmd]) {
		IPAERR("failed to construct NOP imm cmd\n");
		return -ENOMEM;
	}

	ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
	++num_cmd;

	if (num_cmd >= IPA_IPV6CT_MAX_NUM_OF_INIT_CMD_DESC) {
		IPAERR("number of commands is out of range\n");
		result = -ENOBUFS;
		goto destroy_imm_cmd;
	}

	cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_V6_CT_INIT, cmd, false);
	if (!cmd_pyld[num_cmd]) {
		IPAERR_RL("fail to construct IPv6CT init imm cmd\n");
		result = -EPERM;
		goto destroy_imm_cmd;
	}

	ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
	++num_cmd;

	result = ipa3_send_cmd(num_cmd, desc);
	if (result) {
		IPAERR("Fail to send IPv6CT init immediate command\n");
		goto destroy_imm_cmd;
	}

	IPADBG("return\n");

destroy_imm_cmd:
	for (i = 0; i < num_cmd; ++i)
		ipahal_destroy_imm_cmd(cmd_pyld[i]);

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
	struct ipahal_imm_cmd_ip_v4_nat_init cmd;
	int result;

	IPADBG("\n");

	if (!ipa3_ctx->nat_mem.dev.is_mapped) {
		IPAERR_RL("attempt to init %s before mmap\n",
			ipa3_ctx->nat_mem.dev.name);
		return -EPERM;
	}

	if (init->tbl_index >= 1) {
		IPAERR_RL("Unsupported table index %d\n", init->tbl_index);
		return -EPERM;
	}

	if (init->table_entries == 0) {
		IPAERR_RL("Table entries is zero\n");
		return -EPERM;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		&ipa3_ctx->nat_mem.dev,
		init->ipv4_rules_offset,
		init->table_entries + 1,
		IPAHAL_NAT_IPV4);
	if (result) {
		IPAERR_RL("Bad params for NAT base table\n");
		return result;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		&ipa3_ctx->nat_mem.dev,
		init->expn_rules_offset,
		init->expn_table_entries,
		IPAHAL_NAT_IPV4);
	if (result) {
		IPAERR_RL("Bad params for NAT expansion table\n");
		return result;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		&ipa3_ctx->nat_mem.dev,
		init->index_offset,
		init->table_entries + 1,
		IPAHAL_NAT_IPV4_INDEX);
	if (result) {
		IPAERR_RL("Bad params for index table\n");
		return result;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		&ipa3_ctx->nat_mem.dev,
		init->index_expn_offset,
		init->expn_table_entries,
		IPAHAL_NAT_IPV4_INDEX);
	if (result) {
		IPAERR_RL("Bad params for index expansion table\n");
		return result;
	}

	if (ipa3_ctx->nat_mem.dev.is_sys_mem) {
		IPADBG("using system memory for nat table\n");
		/*
		 * Safe to process, since integer overflow was
		 * checked in ipa3_nat_ipv6ct_check_table_params
		 */
		ipa3_nat_create_init_cmd(init, false,
			ipa3_ctx->nat_mem.dev.dma_handle, &cmd);
	} else {
		IPADBG("using shared(local) memory for nat table\n");
		ipa3_nat_create_init_cmd(init, true, IPA_RAM_NAT_OFST, &cmd);
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		struct ipa_pdn_entry *pdn_entries;

		/* store ip in pdn entries cache array */
		pdn_entries = ipa3_ctx->nat_mem.pdn_mem.base;
		pdn_entries[0].public_ip = init->ip_addr;
		pdn_entries[0].dst_metadata = 0;
		pdn_entries[0].src_metadata = 0;
		pdn_entries[0].resrvd = 0;

		IPADBG("Public ip address:0x%x\n", init->ip_addr);
	}

	IPADBG("posting NAT init command\n");
	result = ipa3_nat_send_init_cmd(&cmd, false);
	if (result) {
		IPAERR("Fail to send NAT init immediate command\n");
		return result;
	}

	ipa3_nat_ipv6ct_init_device_structure(
		&ipa3_ctx->nat_mem.dev,
		init->ipv4_rules_offset,
		init->expn_rules_offset,
		init->table_entries,
		init->expn_table_entries);

	ipa3_ctx->nat_mem.public_ip_addr = init->ip_addr;
	IPADBG("Public IP address:%pI4h\n", &ipa3_ctx->nat_mem.public_ip_addr);

	ipa3_ctx->nat_mem.index_table_addr =
		 (char *)ipa3_ctx->nat_mem.dev.base_address +
		 init->index_offset;
	IPADBG("index_table_addr: 0x%pK\n",
				 ipa3_ctx->nat_mem.index_table_addr);

	ipa3_ctx->nat_mem.index_table_expansion_addr =
	 (char *)ipa3_ctx->nat_mem.dev.base_address + init->index_expn_offset;
	IPADBG("index_table_expansion_addr: 0x%pK\n",
				 ipa3_ctx->nat_mem.index_table_expansion_addr);

	ipa3_ctx->nat_mem.dev.is_hw_init = true;
	IPADBG("return\n");
	return 0;
}

/**
 * ipa3_ipv6ct_init_cmd() - Post IP_V6_CONN_TRACK_INIT command to IPA HW
 * @init:	[in] initialization command attributes
 *
 * Called by NAT client driver to post IP_V6_CONN_TRACK_INIT command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_ipv6ct_init_cmd(struct ipa_ioc_ipv6ct_init *init)
{
	struct ipahal_imm_cmd_ip_v6_ct_init cmd;
	int result;

	IPADBG("\n");

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		IPAERR_RL("IPv6 connection tracking isn't supported\n");
		return -EPERM;
	}

	if (!ipa3_ctx->ipv6ct_mem.dev.is_mapped) {
		IPAERR_RL("attempt to init %s before mmap\n",
			ipa3_ctx->ipv6ct_mem.dev.name);
		return -EPERM;
	}

	if (init->tbl_index >= 1) {
		IPAERR_RL("Unsupported table index %d\n", init->tbl_index);
		return -EPERM;
	}

	if (init->table_entries == 0) {
		IPAERR_RL("Table entries is zero\n");
		return -EPERM;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		&ipa3_ctx->ipv6ct_mem.dev,
		init->base_table_offset,
		init->table_entries + 1,
		IPAHAL_NAT_IPV6CT);
	if (result) {
		IPAERR_RL("Bad params for IPv6CT base table\n");
		return result;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		&ipa3_ctx->ipv6ct_mem.dev,
		init->expn_table_offset,
		init->expn_table_entries,
		IPAHAL_NAT_IPV6CT);
	if (result) {
		IPAERR_RL("Bad params for IPv6CT expansion table\n");
		return result;
	}

	if (ipa3_ctx->ipv6ct_mem.dev.is_sys_mem) {
		IPADBG("using system memory for nat table\n");
		/*
		 * Safe to process, since integer overflow was
		 * checked in ipa3_nat_ipv6ct_check_table_params
		 */
		ipa3_nat_ipv6ct_create_init_cmd(
			&cmd.table_init,
			false,
			ipa3_ctx->ipv6ct_mem.dev.dma_handle,
			init->tbl_index,
			init->base_table_offset,
			init->expn_table_offset,
			init->table_entries,
			init->expn_table_entries,
			ipa3_ctx->ipv6ct_mem.dev.name);
	} else {
		IPADBG("using shared(local) memory for nat table\n");
		ipa3_nat_ipv6ct_create_init_cmd(
			&cmd.table_init,
			true,
			IPA_RAM_IPV6CT_OFST,
			init->tbl_index,
			init->base_table_offset,
			init->expn_table_offset,
			init->table_entries,
			init->expn_table_entries,
			ipa3_ctx->ipv6ct_mem.dev.name);
	}

	IPADBG("posting ip_v6_ct_init imm command\n");
	result = ipa3_ipv6ct_send_init_cmd(&cmd);
	if (result) {
		IPAERR("fail to send IPv6CT init immediate command\n");
		return result;
	}

	ipa3_nat_ipv6ct_init_device_structure(
		&ipa3_ctx->ipv6ct_mem.dev,
		init->base_table_offset,
		init->expn_table_offset,
		init->table_entries,
		init->expn_table_entries);

	ipa3_ctx->ipv6ct_mem.dev.is_hw_init = true;
	IPADBG("return\n");
	return 0;
}

/**
 * ipa3_nat_mdfy_pdn() - Modify a PDN entry in PDN config table in IPA SRAM
 * @mdfy_pdn:	[in] PDN info to be written to SRAM
 *
 * Called by NAT client driver to modify an entry in the PDN config table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_nat_mdfy_pdn(struct ipa_ioc_nat_pdn_entry *mdfy_pdn)
{
	struct ipahal_imm_cmd_dma_shared_mem mem_cmd = { 0 };
	struct ipa3_desc desc;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	int result = 0;
	struct ipa3_nat_mem *nat_ctx = &(ipa3_ctx->nat_mem);
	struct ipa_pdn_entry *pdn_entries = NULL;

	IPADBG("\n");

	mutex_lock(&nat_ctx->dev.lock);

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		IPAERR_RL("IPA HW does not support multi PDN\n");
		result = -EPERM;
		goto bail;
	}

	if (!nat_ctx->dev.is_mem_allocated) {
		IPAERR_RL(
			"attempt to modify a PDN entry before the PDN table memory allocation\n");
		result = -EPERM;
		goto bail;
	}

	if (mdfy_pdn->pdn_index > (IPA_MAX_PDN_NUM - 1)) {
		IPAERR_RL("pdn index out of range %d\n", mdfy_pdn->pdn_index);
		result = -EPERM;
		goto bail;
	}

	pdn_entries = nat_ctx->pdn_mem.base;

	/* store ip in pdn entries cache array */
	pdn_entries[mdfy_pdn->pdn_index].public_ip =
		mdfy_pdn->public_ip;
	pdn_entries[mdfy_pdn->pdn_index].dst_metadata =
		mdfy_pdn->dst_metadata;
	pdn_entries[mdfy_pdn->pdn_index].src_metadata =
		mdfy_pdn->src_metadata;

	/* mark tethering bit for remote modem */
	if (ipa3_ctx->ipa_hw_type == IPA_HW_v4_1)
		pdn_entries[mdfy_pdn->pdn_index].src_metadata |=
			IPA_QMAP_TETH_BIT;

	IPADBG("Modify PDN in index: %d Public ip address:%pI4h\n",
		mdfy_pdn->pdn_index,
		&pdn_entries[mdfy_pdn->pdn_index].public_ip);
	IPADBG("Modify PDN dst metadata: 0x%x src metadata: 0x%x\n",
		pdn_entries[mdfy_pdn->pdn_index].dst_metadata,
		pdn_entries[mdfy_pdn->pdn_index].src_metadata);

	/* Copy the PDN config table to SRAM */
	ipa3_nat_create_modify_pdn_cmd(&mem_cmd, false);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &mem_cmd, false);
	if (!cmd_pyld) {
		IPAERR(
			"fail construct dma_shared_mem cmd: for pdn table\n");
		result = -ENOMEM;
		goto bail;
	}
	ipa3_init_imm_cmd_desc(&desc, cmd_pyld);

	IPADBG("sending PDN table copy cmd\n");
	result = ipa3_send_cmd(1, &desc);
	if (result)
		IPAERR("Fail to send PDN table copy immediate command\n");

	ipahal_destroy_imm_cmd(cmd_pyld);

	IPADBG("return\n");

bail:
	mutex_unlock(&nat_ctx->dev.lock);
	return result;
}

static uint32_t ipa3_nat_ipv6ct_calculate_table_size(uint8_t base_addr)
{
	size_t entry_size;
	u32 entries_num;
	enum ipahal_nat_type nat_type;

	switch (base_addr) {
	case IPA_NAT_BASE_TBL:
		entries_num = ipa3_ctx->nat_mem.dev.table_entries + 1;
		nat_type = IPAHAL_NAT_IPV4;
		break;
	case IPA_NAT_EXPN_TBL:
		entries_num = ipa3_ctx->nat_mem.dev.expn_table_entries;
		nat_type = IPAHAL_NAT_IPV4;
		break;
	case IPA_NAT_INDX_TBL:
		entries_num = ipa3_ctx->nat_mem.dev.table_entries + 1;
		nat_type = IPAHAL_NAT_IPV4_INDEX;
		break;
	case IPA_NAT_INDEX_EXPN_TBL:
		entries_num = ipa3_ctx->nat_mem.dev.expn_table_entries;
		nat_type = IPAHAL_NAT_IPV4_INDEX;
		break;
	case IPA_IPV6CT_BASE_TBL:
		entries_num = ipa3_ctx->ipv6ct_mem.dev.table_entries + 1;
		nat_type = IPAHAL_NAT_IPV6CT;
		break;
	case IPA_IPV6CT_EXPN_TBL:
		entries_num = ipa3_ctx->ipv6ct_mem.dev.expn_table_entries;
		nat_type = IPAHAL_NAT_IPV6CT;
		break;
	default:
		IPAERR_RL("Invalid base_addr %d for table DMA command\n",
			base_addr);
		return 0;
	}

	ipahal_nat_entry_size(nat_type, &entry_size);
	return entry_size * entries_num;
}

static int ipa3_table_validate_table_dma_one(struct ipa_ioc_nat_dma_one *param)
{
	uint32_t table_size;

	if (param->table_index >= 1) {
		IPAERR_RL("Unsupported table index %d\n", param->table_index);
		return -EPERM;
	}

	switch (param->base_addr) {
	case IPA_NAT_BASE_TBL:
	case IPA_NAT_EXPN_TBL:
	case IPA_NAT_INDX_TBL:
	case IPA_NAT_INDEX_EXPN_TBL:
		if (!ipa3_ctx->nat_mem.dev.is_hw_init) {
			IPAERR_RL("attempt to write to %s before HW int\n",
				ipa3_ctx->nat_mem.dev.name);
			return -EPERM;
		}
		break;
	case IPA_IPV6CT_BASE_TBL:
	case IPA_IPV6CT_EXPN_TBL:
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
			IPAERR_RL("IPv6 connection tracking isn't supported\n");
			return -EPERM;
		}

		if (!ipa3_ctx->ipv6ct_mem.dev.is_hw_init) {
			IPAERR_RL("attempt to write to %s before HW int\n",
				ipa3_ctx->ipv6ct_mem.dev.name);
			return -EPERM;
		}
		break;
	default:
		IPAERR_RL("Invalid base_addr %d for table DMA command\n",
			param->base_addr);
		return -EPERM;
	}

	table_size = ipa3_nat_ipv6ct_calculate_table_size(param->base_addr);
	if (!table_size) {
		IPAERR_RL("Failed to calculate table size for base_addr %d\n",
			param->base_addr);
		return -EPERM;
	}

	if (param->offset >= table_size) {
		IPAERR_RL("Invalid offset %d for table DMA command\n",
			param->offset);
		IPAERR_RL("table_index %d base addr %d size %d\n",
			param->table_index, param->base_addr, table_size);
		return -EPERM;
	}

	return 0;
}


/**
 * ipa3_table_dma_cmd() - Post TABLE_DMA command to IPA HW
 * @dma:	[in] initialization command attributes
 *
 * Called by NAT/IPv6CT clients to post TABLE_DMA command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_table_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma)
{
	struct ipahal_imm_cmd_table_dma cmd;
	enum ipahal_imm_cmd_name cmd_name = IPA_IMM_CMD_NAT_DMA;
	struct ipahal_imm_cmd_pyld *cmd_pyld[IPA_MAX_NUM_OF_TABLE_DMA_CMD_DESC];
	struct ipa3_desc desc[IPA_MAX_NUM_OF_TABLE_DMA_CMD_DESC];
	uint8_t cnt, num_cmd = 0;
	int result = 0;

	IPADBG("\n");
	if (!dma->entries ||
		dma->entries >= IPA_MAX_NUM_OF_TABLE_DMA_CMD_DESC) {
		IPAERR_RL("Invalid number of entries %d\n",
			dma->entries);
		result = -EPERM;
		goto bail;
	}

	if (!ipa3_ctx->nat_mem.dev.is_dev_init) {
		IPAERR_RL("NAT hasn't been initialized\n");
		return -EPERM;
	}

	for (cnt = 0; cnt < dma->entries; ++cnt) {
		result = ipa3_table_validate_table_dma_one(&dma->dma[cnt]);
		if (result) {
			IPAERR_RL("Table DMA command parameter %d is invalid\n",
				cnt);
			goto bail;
		}
	}

	/* NO-OP IC for ensuring that IPA pipeline is empty */
	cmd_pyld[num_cmd] =
		ipahal_construct_nop_imm_cmd(false, IPAHAL_HPS_CLEAR, false);
	if (!cmd_pyld[num_cmd]) {
		IPAERR("Failed to construct NOP imm cmd\n");
		result = -ENOMEM;
		goto destroy_imm_cmd;
	}
	ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
	++num_cmd;

	/* NAT_DMA was renamed to TABLE_DMA starting from IPAv4 */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		cmd_name = IPA_IMM_CMD_TABLE_DMA;

	for (cnt = 0; cnt < dma->entries; ++cnt) {
		cmd.table_index = dma->dma[cnt].table_index;
		cmd.base_addr = dma->dma[cnt].base_addr;
		cmd.offset = dma->dma[cnt].offset;
		cmd.data = dma->dma[cnt].data;
		cmd_pyld[num_cmd] =
			ipahal_construct_imm_cmd(cmd_name, &cmd, false);
		if (!cmd_pyld[num_cmd]) {
			IPAERR_RL("Fail to construct table_dma imm cmd\n");
			result = -ENOMEM;
			goto destroy_imm_cmd;
		}
		ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
		++num_cmd;
	}
	result = ipa3_send_cmd(num_cmd, desc);
	if (result)
		IPAERR("Fail to send table_dma immediate command\n");

	IPADBG("return\n");

destroy_imm_cmd:
	for (cnt = 0; cnt < num_cmd; ++cnt)
		ipahal_destroy_imm_cmd(cmd_pyld[cnt]);
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
	return ipa3_table_dma_cmd(dma);
}

static void ipa3_nat_ipv6ct_free_mem(struct ipa3_nat_ipv6ct_common_mem *dev)
{
	IPADBG("\n");
	if (!dev->is_mem_allocated) {
		IPADBG("attempt to delete %s before memory allocation\n",
			dev->name);
		/* Deletion of partly initialized table is not an error */
		goto clear;
	}

	if (dev->is_sys_mem) {
		IPADBG("freeing the dma memory for %s\n", dev->name);
		dma_free_coherent(
			ipa3_ctx->pdev, dev->size,
			dev->vaddr, dev->dma_handle);
		dev->size = 0;
		dev->vaddr = NULL;
	}

	dev->is_mem_allocated = false;

clear:
	dev->table_entries = 0;
	dev->expn_table_entries = 0;
	dev->base_table_addr = NULL;
	dev->expansion_table_addr = NULL;

	dev->is_hw_init = false;
	dev->is_mapped = false;
	dev->is_sys_mem = false;

	IPADBG("return\n");
}

static int ipa3_nat_ipv6ct_create_del_table_cmd(
	uint8_t tbl_index,
	u32 base_addr,
	struct ipa3_nat_ipv6ct_common_mem *dev,
	struct ipahal_imm_cmd_nat_ipv6ct_init_common *table_init_cmd)
{
	bool mem_type_shared = true;

	IPADBG("\n");

	if (tbl_index >= 1) {
		IPAERR_RL("Unsupported table index %d\n", tbl_index);
		return -EPERM;
	}

	if (dev->tmp_mem != NULL) {
		IPADBG("using temp memory during %s del\n", dev->name);
		mem_type_shared = false;
		base_addr = dev->tmp_mem->dma_handle;
	}

	table_init_cmd->table_index = tbl_index;
	table_init_cmd->base_table_addr = base_addr;
	table_init_cmd->base_table_addr_shared = mem_type_shared;
	table_init_cmd->expansion_table_addr = base_addr;
	table_init_cmd->expansion_table_addr_shared = mem_type_shared;
	table_init_cmd->size_base_table = 0;
	table_init_cmd->size_expansion_table = 0;
	IPADBG("return\n");

	return 0;
}

static int ipa3_nat_send_del_table_cmd(uint8_t tbl_index)
{
	struct ipahal_imm_cmd_ip_v4_nat_init cmd;
	int result;

	IPADBG("\n");

	result = ipa3_nat_ipv6ct_create_del_table_cmd(
		tbl_index,
		IPA_NAT_PHYS_MEM_OFFSET,
		&ipa3_ctx->nat_mem.dev,
		&cmd.table_init);
	if (result) {
		IPAERR(
			"Fail to create immediate command to delete NAT table\n");
		return result;
	}

	cmd.index_table_addr = cmd.table_init.base_table_addr;
	cmd.index_table_addr_shared = cmd.table_init.base_table_addr_shared;
	cmd.index_table_expansion_addr = cmd.index_table_addr;
	cmd.index_table_expansion_addr_shared = cmd.index_table_addr_shared;
	cmd.public_addr_info = 0;

	IPADBG("posting NAT delete command\n");
	result = ipa3_nat_send_init_cmd(&cmd, true);
	if (result) {
		IPAERR("Fail to send NAT delete immediate command\n");
		return result;
	}

	IPADBG("return\n");
	return 0;
}

static int ipa3_ipv6ct_send_del_table_cmd(uint8_t tbl_index)
{
	struct ipahal_imm_cmd_ip_v6_ct_init cmd;
	int result;

	IPADBG("\n");

	result = ipa3_nat_ipv6ct_create_del_table_cmd(
		tbl_index,
		IPA_IPV6CT_PHYS_MEM_OFFSET,
		&ipa3_ctx->ipv6ct_mem.dev,
		&cmd.table_init);
	if (result) {
		IPAERR(
			"Fail to create immediate command to delete IPv6CT table\n");
		return result;
	}

	IPADBG("posting IPv6CT delete command\n");
	result = ipa3_ipv6ct_send_init_cmd(&cmd);
	if (result) {
		IPAERR("Fail to send IPv6CT delete immediate command\n");
		return result;
	}

	IPADBG("return\n");
	return 0;
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
	struct ipa_ioc_nat_ipv6ct_table_del tmp;

	tmp.table_index = del->table_index;

	return ipa3_del_nat_table(&tmp);
}

/**
 * ipa3_del_nat_table() - Delete the NAT table
 * @del:	[in] delete table parameters
 *
 * Called by NAT client to delete the table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_del_nat_table(struct ipa_ioc_nat_ipv6ct_table_del *del)
{
	int result = 0;

	IPADBG("\n");
	if (!ipa3_ctx->nat_mem.dev.is_dev_init) {
		IPAERR("NAT hasn't been initialized\n");
		return -EPERM;
	}

	mutex_lock(&ipa3_ctx->nat_mem.dev.lock);

	if (ipa3_ctx->nat_mem.dev.is_hw_init) {
		result = ipa3_nat_send_del_table_cmd(del->table_index);
		if (result) {
			IPAERR(
				"Fail to send immediate command to delete NAT table\n");
			goto bail;
		}
	}

	ipa3_ctx->nat_mem.public_ip_addr = 0;
	ipa3_ctx->nat_mem.index_table_addr = 0;
	ipa3_ctx->nat_mem.index_table_expansion_addr = 0;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0 &&
		ipa3_ctx->nat_mem.dev.is_mem_allocated) {
		IPADBG("freeing the PDN memory\n");
		dma_free_coherent(ipa3_ctx->pdev,
			ipa3_ctx->nat_mem.pdn_mem.size,
			ipa3_ctx->nat_mem.pdn_mem.base,
			ipa3_ctx->nat_mem.pdn_mem.phys_base);
		ipa3_ctx->nat_mem.pdn_mem.base = NULL;
		ipa3_ctx->nat_mem.dev.is_mem_allocated = false;
	}

	ipa3_nat_ipv6ct_free_mem(&ipa3_ctx->nat_mem.dev);
	IPADBG("return\n");

bail:
	mutex_unlock(&ipa3_ctx->nat_mem.dev.lock);
	return result;
}

/**
 * ipa3_del_ipv6ct_table() - Delete the IPv6CT table
 * @del:	[in] delete table parameters
 *
 * Called by IPv6CT client to delete the table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_del_ipv6ct_table(struct ipa_ioc_nat_ipv6ct_table_del *del)
{
	int result = 0;

	IPADBG("\n");

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		IPAERR_RL("IPv6 connection tracking isn't supported\n");
		return -EPERM;
	}

	if (!ipa3_ctx->ipv6ct_mem.dev.is_dev_init) {
		IPAERR("IPv6 connection tracking hasn't been initialized\n");
		return -EPERM;
	}

	mutex_lock(&ipa3_ctx->ipv6ct_mem.dev.lock);

	if (ipa3_ctx->ipv6ct_mem.dev.is_hw_init) {
		result = ipa3_ipv6ct_send_del_table_cmd(del->table_index);
		if (result) {
			IPAERR(
				"Fail to send immediate command to delete IPv6CT table\n");
			goto bail;
		}
	}

	ipa3_nat_ipv6ct_free_mem(&ipa3_ctx->ipv6ct_mem.dev);
	IPADBG("return\n");

bail:
	mutex_unlock(&ipa3_ctx->ipv6ct_mem.dev.lock);
	return result;
}


/* Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

#define IPA_NAT_PHYS_MEM_OFFSET IPA_MEM_PART(nat_tbl_ofst)
#define IPA_NAT_PHYS_MEM_SIZE  IPA_RAM_NAT_SIZE

#define IPA_IPV6CT_PHYS_MEM_OFFSET  0
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

#define IPA_VALID_TBL_INDEX(ti) \
	((ti) == 0)

enum ipa_nat_ipv6ct_table_type {
	IPA_NAT_BASE_TBL = 0,
	IPA_NAT_EXPN_TBL = 1,
	IPA_NAT_INDX_TBL = 2,
	IPA_NAT_INDEX_EXPN_TBL = 3,
	IPA_IPV6CT_BASE_TBL = 4,
	IPA_IPV6CT_EXPN_TBL = 5
};

static bool sram_compatible;

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


static inline const char *ipa3_nat_mem_in_as_str(
	enum ipa3_nat_mem_in nmi)
{
	switch (nmi) {
	case IPA_NAT_MEM_IN_DDR:
		return "IPA_NAT_MEM_IN_DDR";
	case IPA_NAT_MEM_IN_SRAM:
		return "IPA_NAT_MEM_IN_SRAM";
	default:
		break;
	}
	return "INVALID_MEM_TYPE";
}

static inline char *ipa_ioc_v4_nat_init_as_str(
	struct ipa_ioc_v4_nat_init *ptr,
	char                       *buf,
	uint32_t                    buf_sz)
{
	if (ptr && buf && buf_sz) {
		snprintf(
			buf, buf_sz,
			"V4 NAT INIT: tbl_index(0x%02X) ipv4_rules_offset(0x%08X) expn_rules_offset(0x%08X) index_offset(0x%08X) index_expn_offset(0x%08X) table_entries(0x%04X) expn_table_entries(0x%04X) ip_addr(0x%08X)",
			ptr->tbl_index,
			ptr->ipv4_rules_offset,
			ptr->expn_rules_offset,
			ptr->index_offset,
			ptr->index_expn_offset,
			ptr->table_entries,
			ptr->expn_table_entries,
			ptr->ip_addr);
	}
	return buf;
}

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

static int ipa3_nat_ipv6ct_mmap(
	struct file *filp,
	struct vm_area_struct *vma)
{
	struct ipa3_nat_ipv6ct_common_mem *dev =
		(struct ipa3_nat_ipv6ct_common_mem *) filp->private_data;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_AP);
	struct ipa3_nat_mem          *nm_ptr;
	struct ipa3_nat_mem_loc_data *mld_ptr;
	enum ipa3_nat_mem_in          nmi;

	int result = 0;

	IPADBG("In\n");

	if (!dev->is_dev_init) {
		IPAERR("Attempt to mmap %s before dev init\n",
			   dev->name);
		result = -EPERM;
		goto bail;
	}

	mutex_lock(&dev->lock);

	/*
	 * Check if no smmu or non dma coherent
	 */
	if (!cb->valid || !is_device_dma_coherent(cb->dev)) {

		IPADBG("Either smmu valid=%u and/or DMA coherent=%u false\n",
			   cb->valid, is_device_dma_coherent(cb->dev));

		vma->vm_page_prot =
			pgprot_noncached(vma->vm_page_prot);
	}

	if (dev->is_nat_mem) {

		nm_ptr = (struct ipa3_nat_mem *) dev;
		nmi    = nm_ptr->last_alloc_loc;

		if (!IPA_VALID_NAT_MEM_IN(nmi)) {
			IPAERR_RL("Bad ipa3_nat_mem_in type\n");
			result = -EPERM;
			goto unlock;
		}

		mld_ptr = &nm_ptr->mem_loc[nmi];

		if (!mld_ptr->vaddr) {
			IPAERR_RL(
			 "Attempt to mmap %s before the memory allocation\n",
			 dev->name);
			result = -EPERM;
			goto unlock;
		}

		if (mld_ptr->is_mapped) {
			IPAERR("%s already mapped, only 1 mapping supported\n",
				   dev->name);
			result = -EINVAL;
			goto unlock;
		}

		if (nmi == IPA_NAT_MEM_IN_SRAM) {
			if (dev->phys_mem_size == 0 ||
				dev->phys_mem_size > vsize) {
				IPAERR_RL(
				 "%s err vsize(0x%X) phys_mem_size(0x%X)\n",
				 dev->name, vsize, dev->phys_mem_size);
				result = -EINVAL;
				goto unlock;
			}
		}

		mld_ptr->base_address = NULL;

		IPADBG("Mapping V4 NAT: %s\n",
			   ipa3_nat_mem_in_as_str(nmi));

		if (nmi == IPA_NAT_MEM_IN_DDR) {

			IPADBG("map sz=0x%zx -> vma size=0x%08x\n",
				   mld_ptr->table_alloc_size,
				   vsize);

			result =
				dma_mmap_coherent(
					ipa3_ctx->pdev,
					vma,
					mld_ptr->vaddr,
					mld_ptr->dma_handle,
					mld_ptr->table_alloc_size);

			if (result) {
				IPAERR(
				 "dma_mmap_coherent failed. Err:%d\n",
				 result);
				goto unlock;
			}

			mld_ptr->base_address = mld_ptr->vaddr;

		} else { /* nmi == IPA_NAT_MEM_IN_SRAM */

			IPADBG("map phys_mem_size(0x%08X) -> vma sz(0x%08X)\n",
				   dev->phys_mem_size, vsize);

			vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

			result = vm_iomap_memory(
				vma, mld_ptr->phys_addr, dev->phys_mem_size);

			if (result) {
				IPAERR("vm_iomap_memory failed. Err:%d\n",
					   result);
				goto unlock;
			}

			mld_ptr->base_address = mld_ptr->vaddr;
		}

		mld_ptr->is_mapped = true;

	} else { /* dev->is_ipv6ct_mem */

		if (!dev->vaddr) {
			IPAERR_RL(
			 "Attempt to mmap %s before the memory allocation\n",
			 dev->name);
			result = -EPERM;
			goto unlock;
		}

		if (dev->is_mapped) {
			IPAERR("%s already mapped, only 1 mapping supported\n",
				   dev->name);
			result = -EINVAL;
			goto unlock;
		}

		dev->base_address = NULL;

		IPADBG("Mapping V6 CT: %s\n",
			   ipa3_nat_mem_in_as_str(IPA_NAT_MEM_IN_DDR));

		IPADBG("map sz=0x%zx -> vma size=0x%08x\n",
			   dev->table_alloc_size,
			   vsize);

		result =
			dma_mmap_coherent(
				ipa3_ctx->pdev,
				vma,
				dev->vaddr,
				dev->dma_handle,
				dev->table_alloc_size);

		if (result) {
			IPAERR("dma_mmap_coherent failed. Err:%d\n", result);
			goto unlock;
		}

		dev->base_address = dev->vaddr;

		dev->is_mapped = true;
	}

	vma->vm_ops = &ipa3_nat_ipv6ct_remap_vm_ops;

unlock:
	mutex_unlock(&dev->lock);

bail:
	IPADBG("Out\n");

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
	const char                        *name,
	u32                                phys_mem_size,
	u32                                phys_mem_ofst,
	struct ipa3_nat_ipv6ct_tmp_mem    *tmp_mem)
{
	int result = 0;

	IPADBG("In: Init of %s\n", name);

	mutex_init(&dev->lock);

	dev->is_nat_mem    = IS_NAT_MEM_DEV(dev);
	dev->is_ipv6ct_mem = IS_IPV6CT_MEM_DEV(dev);

	if (strnlen(name, IPA_DEV_NAME_MAX_LEN) == IPA_DEV_NAME_MAX_LEN) {
		IPAERR("device name is too long\n");
		result = -ENODEV;
		goto bail;
	}

	strlcpy(dev->name, name, IPA_DEV_NAME_MAX_LEN);

	dev->class = class_create(THIS_MODULE, name);

	if (IS_ERR(dev->class)) {
		IPAERR("unable to create the class for %s\n", name);
		result = -ENODEV;
		goto bail;
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

	mutex_lock(&dev->lock);

	result = cdev_add(&dev->cdev, dev->dev_num, 1);

	if (result) {
		IPAERR("cdev_add err=%d\n", -result);
		goto cdev_add_fail;
	}

	dev->tmp_mem       = tmp_mem;
	dev->phys_mem_size = phys_mem_size;
	dev->phys_mem_ofst = phys_mem_ofst;
	dev->is_dev_init   = true;

	mutex_unlock(&dev->lock);

	IPADBG("ipa dev %s added successfully. major:%d minor:%d\n", name,
			  MAJOR(dev->dev_num), MINOR(dev->dev_num));

	result = 0;

	goto bail;

cdev_add_fail:
	mutex_unlock(&dev->lock);
	device_destroy(dev->class, dev->dev_num);

device_create_fail:
	unregister_chrdev_region(dev->dev_num, 1);

alloc_chrdev_region_fail:
	class_destroy(dev->class);

bail:
	IPADBG("Out\n");

	return result;
}

static void ipa3_nat_ipv6ct_destroy_device(
	struct ipa3_nat_ipv6ct_common_mem *dev)
{
	IPADBG("In\n");

	mutex_lock(&dev->lock);

	if (dev->tmp_mem) {
		if (ipa3_ctx->nat_mem.is_tmp_mem_allocated) {
			dma_free_coherent(
				ipa3_ctx->pdev,
				IPA_NAT_IPV6CT_TEMP_MEM_SIZE,
				dev->tmp_mem->vaddr,
				dev->tmp_mem->dma_handle);
			kfree(dev->tmp_mem);
			dev->tmp_mem = NULL;
			ipa3_ctx->nat_mem.is_tmp_mem_allocated = false;
		}
		dev->tmp_mem = NULL;
	}

	device_destroy(dev->class, dev->dev_num);

	unregister_chrdev_region(dev->dev_num, 1);

	class_destroy(dev->class);

	dev->is_dev_init = false;

	mutex_unlock(&dev->lock);

	IPADBG("Out\n");
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

static int ipa3_nat_ipv6ct_allocate_mem(
	struct ipa3_nat_ipv6ct_common_mem *dev,
	struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc,
	enum ipahal_nat_type nat_type)
{
	gfp_t gfp_flags = GFP_KERNEL | __GFP_ZERO;
	size_t nat_entry_size;

	struct ipa3_nat_mem *nm_ptr;
	struct ipa3_nat_mem_loc_data *mld_ptr;
	uintptr_t tmp_ptr;

	int    result = 0;

	IPADBG("In: Requested alloc size %zu for %s\n",
			  table_alloc->size, dev->name);

	if (!table_alloc->size) {
		IPAERR_RL("Invalid Parameters\n");
		result = -EPERM;
		goto bail;
	}

	if (!dev->is_dev_init) {
		IPAERR("%s hasn't been initialized\n", dev->name);
		result = -EPERM;
		goto bail;
	}

	if ((dev->is_nat_mem    && nat_type != IPAHAL_NAT_IPV4) ||
		(dev->is_ipv6ct_mem && nat_type != IPAHAL_NAT_IPV6CT)) {
		IPAERR("%s dev type(%s) and nat_type(%s) mismatch\n",
			   dev->name,
			   (dev->is_nat_mem) ? "V4" : "V6",
			   ipahal_nat_type_str(nat_type));
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

	if (nat_type == IPAHAL_NAT_IPV4) {

		nm_ptr = (struct ipa3_nat_mem *) dev;

		if (table_alloc->size <= IPA_NAT_PHYS_MEM_SIZE) {
			/*
			 * CAN fit in SRAM, hence we'll use SRAM...
			 */
			IPADBG("V4 NAT with size 0x%08X will reside in: %s\n",
				   table_alloc->size,
				   ipa3_nat_mem_in_as_str(IPA_NAT_MEM_IN_SRAM));

			if (nm_ptr->sram_in_use) {
				IPAERR("Memory already allocated\n");
				result = -EPERM;
				goto bail;
			}

			mld_ptr = &nm_ptr->mem_loc[IPA_NAT_MEM_IN_SRAM];

			mld_ptr->table_alloc_size = table_alloc->size;

			mld_ptr->phys_addr =
				ipa3_ctx->ipa_wrapper_base +
				ipa3_ctx->ctrl->ipa_reg_base_ofst +
				ipahal_get_reg_n_ofst(
					IPA_SW_AREA_RAM_DIRECT_ACCESS_n,
					0) +
				IPA_NAT_PHYS_MEM_OFFSET;

			mld_ptr->io_vaddr = ioremap(
				mld_ptr->phys_addr, IPA_NAT_PHYS_MEM_SIZE);

			if (mld_ptr->io_vaddr == NULL) {
				IPAERR("ioremap failed\n");
				result = -ENOMEM;
				goto bail;
			}

			tmp_ptr = (uintptr_t) mld_ptr->io_vaddr;

			mld_ptr->vaddr = (void *) tmp_ptr;

			nm_ptr->sram_in_use    = true;
			nm_ptr->last_alloc_loc = IPA_NAT_MEM_IN_SRAM;

		} else {

			/*
			 * CAN NOT fit in SRAM, hence we'll allocate DDR...
			 */
			IPADBG("V4 NAT with size 0x%08X will reside in: %s\n",
				   table_alloc->size,
				   ipa3_nat_mem_in_as_str(IPA_NAT_MEM_IN_DDR));

			if (nm_ptr->ddr_in_use) {
				IPAERR("Memory already allocated\n");
				result = -EPERM;
				goto bail;
			}

			mld_ptr = &nm_ptr->mem_loc[IPA_NAT_MEM_IN_DDR];

			mld_ptr->table_alloc_size = table_alloc->size;

			mld_ptr->vaddr =
				dma_alloc_coherent(
					ipa3_ctx->pdev,
					mld_ptr->table_alloc_size,
					&mld_ptr->dma_handle,
					gfp_flags);

			if (mld_ptr->vaddr == NULL) {
				IPAERR("memory alloc failed\n");
				result = -ENOMEM;
				goto bail;
			}

			nm_ptr->ddr_in_use     = true;
			nm_ptr->last_alloc_loc = IPA_NAT_MEM_IN_DDR;
		}
	} else {
		if (nat_type == IPAHAL_NAT_IPV6CT) {

			IPADBG("V6 CT with size 0x%08X will reside in: %s\n",
				   table_alloc->size,
				   ipa3_nat_mem_in_as_str(IPA_NAT_MEM_IN_DDR));

			dev->table_alloc_size = table_alloc->size;

			dev->vaddr =
				dma_alloc_coherent(
					ipa3_ctx->pdev,
					dev->table_alloc_size,
					&dev->dma_handle,
					gfp_flags);

			if (dev->vaddr == NULL) {
				IPAERR("memory alloc failed\n");
				result = -ENOMEM;
				goto bail;
			}
		}
	}

bail:
	IPADBG("Out\n");

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
int ipa3_allocate_nat_table(
	struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc)
{
	struct ipa3_nat_mem          *nm_ptr = &(ipa3_ctx->nat_mem);
	struct ipa3_nat_mem_loc_data *mld_ptr;

	int result;

	IPADBG("table size:%u offset:%u\n",
		   table_alloc->size, table_alloc->offset);

	mutex_lock(&nm_ptr->dev.lock);

	result = ipa3_nat_ipv6ct_allocate_mem(
		&nm_ptr->dev,
		table_alloc,
		IPAHAL_NAT_IPV4);

	if (result)
		goto bail;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0
		&&
		nm_ptr->pdn_mem.base == NULL) {

		gfp_t gfp_flags = GFP_KERNEL | __GFP_ZERO;
		size_t pdn_entry_size;
		struct ipa_mem_buffer *pdn_mem_ptr = &nm_ptr->pdn_mem;

		ipahal_nat_entry_size(IPAHAL_NAT_IPV4_PDN, &pdn_entry_size);

		pdn_mem_ptr->size = pdn_entry_size * IPA_MAX_PDN_NUM;

		if (IPA_MEM_PART(pdn_config_size) < pdn_mem_ptr->size) {
			IPAERR(
				"number of PDN entries exceeds SRAM available space\n");
			result = -ENOMEM;
			goto fail_alloc_pdn;
		}

		pdn_mem_ptr->base =
			dma_alloc_coherent(
				ipa3_ctx->pdev,
				pdn_mem_ptr->size,
				&pdn_mem_ptr->phys_base,
				gfp_flags);

		if (pdn_mem_ptr->base == NULL) {
			IPAERR("fail to allocate PDN memory\n");
			result = -ENOMEM;
			goto fail_alloc_pdn;
		}

		memset(pdn_mem_ptr->base, 0, pdn_mem_ptr->size);

		IPADBG("IPA NAT dev allocated PDN memory successfully\n");
	}

	IPADBG("IPA NAT dev init successfully\n");

	mutex_unlock(&nm_ptr->dev.lock);

	IPADBG("return\n");

	return 0;

fail_alloc_pdn:
	mld_ptr = &nm_ptr->mem_loc[nm_ptr->last_alloc_loc];

	if (nm_ptr->last_alloc_loc == IPA_NAT_MEM_IN_DDR) {
		if (mld_ptr->vaddr) {
			dma_free_coherent(
				ipa3_ctx->pdev,
				mld_ptr->table_alloc_size,
				mld_ptr->vaddr,
				mld_ptr->dma_handle);
			mld_ptr->vaddr = NULL;
		}
	}

	if (nm_ptr->last_alloc_loc == IPA_NAT_MEM_IN_SRAM) {
		if (mld_ptr->io_vaddr) {
			iounmap(mld_ptr->io_vaddr);
			mld_ptr->io_vaddr = NULL;
			mld_ptr->vaddr    = NULL;
		}
	}

bail:
	mutex_unlock(&nm_ptr->dev.lock);

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
		&ipa3_ctx->ipv6ct_mem.dev,
		table_alloc,
		IPAHAL_NAT_IPV6CT);

	if (result)
		goto bail;

	IPADBG("IPA IPv6CT dev init successfully\n");

bail:
	mutex_unlock(&ipa3_ctx->ipv6ct_mem.dev.lock);
	return result;
}

static int ipa3_nat_ipv6ct_check_table_params(
	struct ipa3_nat_ipv6ct_common_mem *dev,
	enum ipa3_nat_mem_in nmi,
	uint32_t offset,
	uint16_t entries_num,
	enum ipahal_nat_type nat_type)
{
	size_t entry_size, table_size, orig_alloc_size;

	struct ipa3_nat_mem *nm_ptr;
	struct ipa3_nat_mem_loc_data *mld_ptr;

	int ret = 0;

	IPADBG("In\n");

	IPADBG(
		"v4(%u) v6(%u) nmi(%s) ofst(%u) ents(%u) nt(%s)\n",
		dev->is_nat_mem,
		dev->is_ipv6ct_mem,
		ipa3_nat_mem_in_as_str(nmi),
		offset,
		entries_num,
		ipahal_nat_type_str(nat_type));

	if (dev->is_ipv6ct_mem) {

		orig_alloc_size = dev->table_alloc_size;

		if (offset > UINT_MAX - dev->dma_handle) {
			IPAERR_RL("Failed due to integer overflow\n");
			IPAERR_RL("%s dma_handle: 0x%pa offset: 0x%x\n",
					  dev->name, &dev->dma_handle, offset);
			ret = -EPERM;
			goto bail;
		}

	} else { /* dev->is_nat_mem */

		nm_ptr = (struct ipa3_nat_mem *) dev;

		mld_ptr         = &nm_ptr->mem_loc[nmi];
		orig_alloc_size = mld_ptr->table_alloc_size;

		if (nmi == IPA_NAT_MEM_IN_DDR) {
			if (offset > UINT_MAX - mld_ptr->dma_handle) {
				IPAERR_RL("Failed due to integer overflow\n");
				IPAERR_RL("%s dma_handle: 0x%pa offset: 0x%x\n",
				  dev->name, &mld_ptr->dma_handle, offset);
				ret = -EPERM;
				goto bail;
			}
		}
	}

	ret = ipahal_nat_entry_size(nat_type, &entry_size);

	if (ret) {
		IPAERR("Failed to retrieve size of entry for %s\n",
			   ipahal_nat_type_str(nat_type));
		goto bail;
	}

	table_size = entry_size * entries_num;

	/* check for integer overflow */
	if (offset > UINT_MAX - table_size) {
		IPAERR_RL("Detected overflow\n");
		ret = -EPERM;
		goto bail;
	}

	/* Check offset is not beyond allocated size */
	if (offset + table_size > orig_alloc_size) {
		IPAERR_RL("Table offset not valid\n");
		IPAERR_RL("offset:%d entries:%d table_size:%zu mem_size:%zu\n",
		  offset, entries_num, table_size, orig_alloc_size);
		ret = -EPERM;
		goto bail;
	}

bail:
	IPADBG("Out\n");

	return ret;
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

static inline bool chk_sram_offset_alignment(
	uintptr_t addr,
	u32       mask)
{
	if (addr & (uintptr_t) mask) {
		IPAERR("sram addr(%pK) is not properly aligned\n", addr);
		return false;
	}
	return true;
}

static inline int ipa3_nat_ipv6ct_init_device_structure(
	struct ipa3_nat_ipv6ct_common_mem *dev,
	enum ipa3_nat_mem_in nmi,
	uint32_t base_table_offset,
	uint32_t expn_table_offset,
	uint16_t table_entries,
	uint16_t expn_table_entries,
	uint32_t index_offset,
	uint32_t index_expn_offset,
	uint8_t  focus_change)
{
	int ret = 0;

	IPADBG("In\n");

	IPADBG(
		"v4(%u) v6(%u) nmi(%s) bto(%u) eto(%u) t_ents(%u) et_ents(%u) io(%u) ieo(%u)\n",
		dev->is_nat_mem,
		dev->is_ipv6ct_mem,
		ipa3_nat_mem_in_as_str(nmi),
		base_table_offset,
		expn_table_offset,
		table_entries,
		expn_table_entries,
		index_offset,
		index_expn_offset);

	if (dev->is_ipv6ct_mem) {

		IPADBG("v6\n");

		dev->base_table_addr =
			(char *) dev->base_address + base_table_offset;

		IPADBG("%s base_table_addr: 0x%pK\n",
			   dev->name, dev->base_table_addr);

		dev->expansion_table_addr =
			(char *) dev->base_address + expn_table_offset;

		IPADBG("%s expansion_table_addr: 0x%pK\n",
			   dev->name, dev->expansion_table_addr);

		IPADBG("%s table_entries: %d\n",
			   dev->name, table_entries);

		dev->table_entries = table_entries;

		IPADBG("%s expn_table_entries: %d\n",
			   dev->name, expn_table_entries);

		dev->expn_table_entries = expn_table_entries;

	} else if (dev->is_nat_mem) {

		struct ipa3_nat_mem *nm_ptr = (struct ipa3_nat_mem *) dev;
		struct ipa3_nat_mem_loc_data *mld_p =
			&nm_ptr->mem_loc[nmi];

		IPADBG("v4\n");

		nm_ptr->active_table = nmi;

		mld_p->base_table_addr =
			(char *) mld_p->base_address + base_table_offset;

		IPADBG("%s base_table_addr: 0x%pK\n",
				  dev->name, mld_p->base_table_addr);

		mld_p->expansion_table_addr =
			(char *) mld_p->base_address + expn_table_offset;

		IPADBG("%s expansion_table_addr: 0x%pK\n",
				  dev->name, mld_p->expansion_table_addr);

		IPADBG("%s table_entries: %d\n",
				  dev->name, table_entries);

		mld_p->table_entries = table_entries;

		IPADBG("%s expn_table_entries: %d\n",
				  dev->name, expn_table_entries);

		mld_p->expn_table_entries = expn_table_entries;

		mld_p->index_table_addr =
			(char *) mld_p->base_address + index_offset;

		IPADBG("index_table_addr: 0x%pK\n",
				  mld_p->index_table_addr);

		mld_p->index_table_expansion_addr =
			(char *) mld_p->base_address + index_expn_offset;

		IPADBG("index_table_expansion_addr: 0x%pK\n",
				  mld_p->index_table_expansion_addr);

		if (nmi == IPA_NAT_MEM_IN_DDR) {
			if (focus_change)
				nm_ptr->switch2ddr_cnt++;
		} else {
			/*
			 * The IPA wants certain SRAM addresses
			 * to have particular low order bits to
			 * be zero.  We test here to ensure...
			 */
			if (!chk_sram_offset_alignment(
				 (uintptr_t) mld_p->base_table_addr,
				 31) ||
				!chk_sram_offset_alignment(
				 (uintptr_t) mld_p->expansion_table_addr,
				 31) ||
				!chk_sram_offset_alignment(
				 (uintptr_t) mld_p->index_table_addr,
				 3) ||
				!chk_sram_offset_alignment(
				 (uintptr_t) mld_p->index_table_expansion_addr,
				 3)) {
				ret = -ENODEV;
				goto done;
			}

			if (focus_change)
				nm_ptr->switch2sram_cnt++;
		}
	}

done:
	IPADBG("Out\n");

	return ret;
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

	if (zero_mem && ipa3_ctx->nat_mem.pdn_mem.base)
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
int ipa3_nat_init_cmd(
	struct ipa_ioc_v4_nat_init *init)
{
	struct ipa3_nat_ipv6ct_common_mem *dev = &ipa3_ctx->nat_mem.dev;
	struct ipa3_nat_mem *nm_ptr = (struct ipa3_nat_mem *) dev;
	enum ipa3_nat_mem_in nmi;
	struct ipa3_nat_mem_loc_data *mld_ptr;

	struct ipahal_imm_cmd_ip_v4_nat_init cmd;

	int  result;

	IPADBG("In\n");

	if (!sram_compatible) {
		init->mem_type     = 0;
		init->focus_change = 0;
	}

	nmi = init->mem_type;

	IPADBG("tbl_index(%d) table_entries(%u)\n",
			  init->tbl_index,
			  init->table_entries);

	memset(&cmd, 0, sizeof(cmd));

	if (!IPA_VALID_TBL_INDEX(init->tbl_index)) {
		IPAERR_RL("Unsupported table index %d\n",
				  init->tbl_index);
		result = -EPERM;
		goto bail;
	}

	if (init->table_entries == 0) {
		IPAERR_RL("Table entries is zero\n");
		result = -EPERM;
		goto bail;
	}

	if (!IPA_VALID_NAT_MEM_IN(nmi)) {
		IPAERR_RL("Bad ipa3_nat_mem_in type\n");
		result = -EPERM;
		goto bail;
	}

	IPADBG("nmi(%s)\n", ipa3_nat_mem_in_as_str(nmi));

	mld_ptr = &nm_ptr->mem_loc[nmi];

	if (!mld_ptr->is_mapped) {
		IPAERR_RL("Attempt to init %s before mmap\n", dev->name);
		result = -EPERM;
		goto bail;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		dev, nmi,
		init->ipv4_rules_offset,
		init->table_entries + 1,
		IPAHAL_NAT_IPV4);

	if (result) {
		IPAERR_RL("Bad params for NAT base table\n");
		goto bail;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		dev, nmi,
		init->expn_rules_offset,
		init->expn_table_entries,
		IPAHAL_NAT_IPV4);

	if (result) {
		IPAERR_RL("Bad params for NAT expansion table\n");
		goto bail;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		dev, nmi,
		init->index_offset,
		init->table_entries + 1,
		IPAHAL_NAT_IPV4_INDEX);

	if (result) {
		IPAERR_RL("Bad params for index table\n");
		goto bail;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		dev, nmi,
		init->index_expn_offset,
		init->expn_table_entries,
		IPAHAL_NAT_IPV4_INDEX);

	if (result) {
		IPAERR_RL("Bad params for index expansion table\n");
		goto bail;
	}

	IPADBG("Table memory becoming active: %s\n",
		   ipa3_nat_mem_in_as_str(nmi));

	if (nmi == IPA_NAT_MEM_IN_DDR) {
		ipa3_nat_create_init_cmd(
			init,
			false,
			mld_ptr->dma_handle,
			&cmd);
	} else {
		ipa3_nat_create_init_cmd(
			init,
			true,
			IPA_RAM_NAT_OFST,
			&cmd);
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0
		&&
		nm_ptr->pdn_mem.base
		&&
		!init->focus_change) {

		struct ipahal_nat_pdn_entry pdn_entry;

		/* store ip in pdn entry cache array */
		pdn_entry.public_ip = init->ip_addr;
		pdn_entry.src_metadata = 0;
		pdn_entry.dst_metadata = 0;

		result = ipahal_nat_construct_entry(
			IPAHAL_NAT_IPV4_PDN,
			&pdn_entry,
			nm_ptr->pdn_mem.base);

		if (result) {
			IPAERR("Fail to construct NAT pdn entry\n");
			goto bail;
		}
	}

	IPADBG("Posting NAT init command\n");

	result = ipa3_nat_send_init_cmd(&cmd, false);

	if (result) {
		IPAERR("Fail to send NAT init immediate command\n");
		goto bail;
	}

	result = ipa3_nat_ipv6ct_init_device_structure(
		dev, nmi,
		init->ipv4_rules_offset,
		init->expn_rules_offset,
		init->table_entries,
		init->expn_table_entries,
		init->index_offset,
		init->index_expn_offset,
		init->focus_change);

	if (result) {
		IPAERR("Table offset initialization failure\n");
		goto bail;
	}

	nm_ptr->public_ip_addr = init->ip_addr;

	IPADBG("Public IP address:%pI4h\n", &nm_ptr->public_ip_addr);

	dev->is_hw_init = true;

bail:
	IPADBG("Out\n");

	return result;
}

/**
 * ipa3_ipv6ct_init_cmd() - Post IP_V6_CONN_TRACK_INIT command to IPA HW
 * @init:	[in] initialization command attributes
 *
 * Called by NAT client driver to post IP_V6_CONN_TRACK_INIT command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_ipv6ct_init_cmd(
	struct ipa_ioc_ipv6ct_init *init)
{
	struct ipa3_nat_ipv6ct_common_mem *dev = &ipa3_ctx->ipv6ct_mem.dev;

	struct ipahal_imm_cmd_ip_v6_ct_init cmd;

	int result;

	IPADBG("In\n");

	memset(&cmd, 0, sizeof(cmd));

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		IPAERR_RL("IPv6 connection tracking isn't supported\n");
		return -EPERM;
	}

	if (!IPA_VALID_TBL_INDEX(init->tbl_index)) {
		IPAERR_RL("Unsupported table index %d\n", init->tbl_index);
		return -EPERM;
	}

	if (init->table_entries == 0) {
		IPAERR_RL("Table entries is zero\n");
		return -EPERM;
	}

	if (!dev->is_mapped) {
		IPAERR_RL("attempt to init %s before mmap\n",
				  dev->name);
		return -EPERM;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		dev, IPA_NAT_MEM_IN_DDR,
		init->base_table_offset,
		init->table_entries + 1,
		IPAHAL_NAT_IPV6CT);

	if (result) {
		IPAERR_RL("Bad params for IPv6CT base table\n");
		return result;
	}

	result = ipa3_nat_ipv6ct_check_table_params(
		dev, IPA_NAT_MEM_IN_DDR,
		init->expn_table_offset,
		init->expn_table_entries,
		IPAHAL_NAT_IPV6CT);

	if (result) {
		IPAERR_RL("Bad params for IPv6CT expansion table\n");
		return result;
	}

	IPADBG("Will install v6 NAT in: %s\n",
		   ipa3_nat_mem_in_as_str(IPA_NAT_MEM_IN_DDR));

	ipa3_nat_ipv6ct_create_init_cmd(
		&cmd.table_init,
		false,
		dev->dma_handle,
		init->tbl_index,
		init->base_table_offset,
		init->expn_table_offset,
		init->table_entries,
		init->expn_table_entries,
		dev->name);

	IPADBG("posting ip_v6_ct_init imm command\n");

	result = ipa3_ipv6ct_send_init_cmd(&cmd);

	if (result) {
		IPAERR("fail to send IPv6CT init immediate command\n");
		return result;
	}

	ipa3_nat_ipv6ct_init_device_structure(
		dev,
		IPA_NAT_MEM_IN_DDR,
		init->base_table_offset,
		init->expn_table_offset,
		init->table_entries,
		init->expn_table_entries,
		0, 0, 0);

	dev->is_hw_init = true;

	IPADBG("Out\n");

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
int ipa3_nat_mdfy_pdn(
	struct ipa_ioc_nat_pdn_entry *mdfy_pdn)
{
	struct ipa3_nat_ipv6ct_common_mem *dev = &ipa3_ctx->nat_mem.dev;
	struct ipa3_nat_mem *nm_ptr = (struct ipa3_nat_mem *) dev;
	struct ipa_mem_buffer *pdn_mem_ptr = &nm_ptr->pdn_mem;

	struct ipahal_imm_cmd_dma_shared_mem mem_cmd = { 0 };
	struct ipahal_nat_pdn_entry pdn_fields = { 0 };
	struct ipa3_desc desc = { 0 };
	struct ipahal_imm_cmd_pyld *cmd_pyld;

	size_t entry_size;

	int result = 0;

	IPADBG("In\n");

	mutex_lock(&dev->lock);

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		IPAERR_RL("IPA HW does not support multi PDN\n");
		result = -EPERM;
		goto bail;
	}

	if (pdn_mem_ptr->base == NULL) {
		IPAERR_RL(
			"Attempt to modify a PDN entry before the PDN table memory allocation\n");
		result = -EPERM;
		goto bail;
	}

	if (mdfy_pdn->pdn_index > (IPA_MAX_PDN_NUM - 1)) {
		IPAERR_RL("pdn index out of range %d\n", mdfy_pdn->pdn_index);
		result = -EPERM;
		goto bail;
	}

	/*
	 * Store ip in pdn entry cache array
	 */
	pdn_fields.public_ip    = mdfy_pdn->public_ip;
	pdn_fields.dst_metadata = mdfy_pdn->dst_metadata;
	pdn_fields.src_metadata = mdfy_pdn->src_metadata;

	/*
	 * Mark tethering bit for remote modem
	 */
	if (ipa3_ctx->ipa_hw_type == IPA_HW_v4_1) {
		pdn_fields.src_metadata |= IPA_QMAP_TETH_BIT;
	}

	/*
	 * Get size of the entry
	 */
	result = ipahal_nat_entry_size(
		IPAHAL_NAT_IPV4_PDN,
		&entry_size);

	if (result) {
		IPAERR("Failed to retrieve pdn entry size\n");
		goto bail;
	}

	result = ipahal_nat_construct_entry(
		IPAHAL_NAT_IPV4_PDN,
		&pdn_fields,
		(pdn_mem_ptr->base + (mdfy_pdn->pdn_index)*(entry_size)));

	if (result) {
		IPAERR("Fail to construct NAT pdn entry\n");
		goto bail;
	}

	IPADBG("Modify PDN in index: %d Public ip address:%pI4h\n",
		mdfy_pdn->pdn_index,
		&pdn_fields.public_ip);

	IPADBG("Modify PDN dst metadata: 0x%x src metadata: 0x%x\n",
		pdn_fields.dst_metadata,
		pdn_fields.src_metadata);

	/*
	 * Copy the PDN config table to SRAM
	 */
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

bail:
	mutex_unlock(&dev->lock);

	IPADBG("Out\n");

	return result;
}

static uint32_t ipa3_nat_ipv6ct_calculate_table_size(
	enum ipa3_nat_mem_in nmi,
	uint8_t base_addr)
{
	size_t entry_size;
	u32 num_entries;
	enum ipahal_nat_type nat_type;
	struct ipa3_nat_mem_loc_data *mld_ptr = &ipa3_ctx->nat_mem.mem_loc[nmi];

	switch (base_addr) {
	case IPA_NAT_BASE_TBL:
		num_entries = mld_ptr->table_entries + 1;
		nat_type = IPAHAL_NAT_IPV4;
		break;
	case IPA_NAT_EXPN_TBL:
		num_entries = mld_ptr->expn_table_entries;
		nat_type = IPAHAL_NAT_IPV4;
		break;
	case IPA_NAT_INDX_TBL:
		num_entries = mld_ptr->table_entries + 1;
		nat_type = IPAHAL_NAT_IPV4_INDEX;
		break;
	case IPA_NAT_INDEX_EXPN_TBL:
		num_entries = mld_ptr->expn_table_entries;
		nat_type = IPAHAL_NAT_IPV4_INDEX;
		break;
	case IPA_IPV6CT_BASE_TBL:
		num_entries = ipa3_ctx->ipv6ct_mem.dev.table_entries + 1;
		nat_type = IPAHAL_NAT_IPV6CT;
		break;
	case IPA_IPV6CT_EXPN_TBL:
		num_entries = ipa3_ctx->ipv6ct_mem.dev.expn_table_entries;
		nat_type = IPAHAL_NAT_IPV6CT;
		break;
	default:
		IPAERR_RL("Invalid base_addr %d for table DMA command\n",
			base_addr);
		return 0;
	}

	ipahal_nat_entry_size(nat_type, &entry_size);

	return entry_size * num_entries;
}

static int ipa3_table_validate_table_dma_one(
	enum ipa3_nat_mem_in        nmi,
	struct ipa_ioc_nat_dma_one *param)
{
	uint32_t table_size;

	if (param->table_index >= 1) {
		IPAERR_RL("Unsupported table index %u\n", param->table_index);
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
		IPADBG("nmi(%s)\n", ipa3_nat_mem_in_as_str(nmi));
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

	table_size = ipa3_nat_ipv6ct_calculate_table_size(
		nmi,
		param->base_addr);

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
int ipa3_table_dma_cmd(
	struct ipa_ioc_nat_dma_cmd *dma)
{
	struct ipa3_nat_ipv6ct_common_mem *dev = &ipa3_ctx->nat_mem.dev;

	enum ipahal_imm_cmd_name cmd_name = IPA_IMM_CMD_NAT_DMA;

	struct ipahal_imm_cmd_table_dma cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld[IPA_MAX_NUM_OF_TABLE_DMA_CMD_DESC];
	struct ipa3_desc desc[IPA_MAX_NUM_OF_TABLE_DMA_CMD_DESC];

	uint8_t cnt, num_cmd = 0;

	int result = 0;

	IPADBG("In\n");

	if (!sram_compatible)
		dma->mem_type = 0;

	memset(&cmd, 0, sizeof(cmd));

	if (!dev->is_dev_init) {
		IPAERR_RL("NAT hasn't been initialized\n");
		result = -EPERM;
		goto bail;
	}

	if (!IPA_VALID_NAT_MEM_IN(dma->mem_type)) {
		IPAERR_RL("Invalid ipa3_nat_mem_in type (%u)\n",
				  dma->mem_type);
		result = -EPERM;
		goto bail;
	}

	IPADBG("nmi(%s)\n", ipa3_nat_mem_in_as_str(dma->mem_type));

	if (!dma->entries || dma->entries > IPA_MAX_NUM_OF_TABLE_DMA_CMD_DESC) {
		IPAERR_RL("Invalid number of entries %d\n",
				  dma->entries);
		result = -EPERM;
		goto bail;
	}

	for (cnt = 0; cnt < dma->entries; ++cnt) {

		result = ipa3_table_validate_table_dma_one(
			dma->mem_type, &dma->dma[cnt]);

		if (result) {
			IPAERR_RL("Table DMA command parameter %d is invalid\n",
					  cnt);
			goto bail;
		}
	}

	/*
	 * NO-OP IC for ensuring that IPA pipeline is empty
	 */
	cmd_pyld[num_cmd] =
		ipahal_construct_nop_imm_cmd(false, IPAHAL_HPS_CLEAR, false);

	if (!cmd_pyld[num_cmd]) {
		IPAERR("Failed to construct NOP imm cmd\n");
		result = -ENOMEM;
		goto destroy_imm_cmd;
	}

	ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);

	++num_cmd;

	/*
	 * NAT_DMA was renamed to TABLE_DMA starting from IPAv4
	 */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		cmd_name = IPA_IMM_CMD_TABLE_DMA;

	for (cnt = 0; cnt < dma->entries; ++cnt) {

		cmd.table_index = dma->dma[cnt].table_index;
		cmd.base_addr   = dma->dma[cnt].base_addr;
		cmd.offset      = dma->dma[cnt].offset;
		cmd.data        = dma->dma[cnt].data;

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

destroy_imm_cmd:
	for (cnt = 0; cnt < num_cmd; ++cnt)
		ipahal_destroy_imm_cmd(cmd_pyld[cnt]);

bail:
	IPADBG("Out\n");

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

static void ipa3_nat_ipv6ct_free_mem(
	struct ipa3_nat_ipv6ct_common_mem *dev)
{
	struct ipa3_nat_mem *nm_ptr;
	struct ipa3_nat_mem_loc_data *mld_ptr;

	if (dev->is_ipv6ct_mem) {

		IPADBG("In: v6\n");

		if (dev->vaddr) {
			IPADBG("Freeing dma memory for %s\n", dev->name);

			dma_free_coherent(
				ipa3_ctx->pdev,
				dev->table_alloc_size,
				dev->vaddr,
				dev->dma_handle);
		}

		dev->vaddr                = NULL;
		dev->dma_handle           = 0;
		dev->table_alloc_size     = 0;
		dev->base_table_addr      = NULL;
		dev->expansion_table_addr = NULL;
		dev->table_entries        = 0;
		dev->expn_table_entries   = 0;

		dev->is_hw_init           = false;
		dev->is_mapped            = false;
	} else {
		if (dev->is_nat_mem) {

			IPADBG("In: v4\n");

			nm_ptr = (struct ipa3_nat_mem *) dev;

			if (nm_ptr->ddr_in_use) {

				nm_ptr->ddr_in_use = false;

				mld_ptr = &nm_ptr->mem_loc[IPA_NAT_MEM_IN_DDR];

				if (mld_ptr->vaddr) {
					IPADBG("Freeing dma memory for %s\n",
						   dev->name);

					dma_free_coherent(
						ipa3_ctx->pdev,
						mld_ptr->table_alloc_size,
						mld_ptr->vaddr,
						mld_ptr->dma_handle);
				}

				mld_ptr->vaddr                      = NULL;
				mld_ptr->dma_handle                 = 0;
				mld_ptr->table_alloc_size           = 0;
				mld_ptr->table_entries              = 0;
				mld_ptr->expn_table_entries         = 0;
				mld_ptr->base_table_addr            = NULL;
				mld_ptr->expansion_table_addr       = NULL;
				mld_ptr->index_table_addr           = NULL;
				mld_ptr->index_table_expansion_addr = NULL;
			}

			if (nm_ptr->sram_in_use) {

				nm_ptr->sram_in_use = false;

				mld_ptr = &nm_ptr->mem_loc[IPA_NAT_MEM_IN_SRAM];

				if (mld_ptr->io_vaddr) {
					IPADBG("Unmappung sram memory for %s\n",
						   dev->name);
					iounmap(mld_ptr->io_vaddr);
				}

				mld_ptr->io_vaddr                   = NULL;
				mld_ptr->vaddr                      = NULL;
				mld_ptr->dma_handle                 = 0;
				mld_ptr->table_alloc_size           = 0;
				mld_ptr->table_entries              = 0;
				mld_ptr->expn_table_entries         = 0;
				mld_ptr->base_table_addr            = NULL;
				mld_ptr->expansion_table_addr       = NULL;
				mld_ptr->index_table_addr           = NULL;
				mld_ptr->index_table_expansion_addr = NULL;
			}

			memset(nm_ptr->mem_loc, 0, sizeof(nm_ptr->mem_loc));
		}
	}

	IPADBG("Out\n");
}

static int ipa3_nat_ipv6ct_create_del_table_cmd(
	uint8_t tbl_index,
	u32 base_addr,
	struct ipa3_nat_ipv6ct_common_mem *dev,
	struct ipahal_imm_cmd_nat_ipv6ct_init_common *table_init_cmd)
{
	bool mem_type_shared = true;

	IPADBG("In: tbl_index(%u) base_addr(%u) v4(%u) v6(%u)\n",
			  tbl_index,
			  base_addr,
			  dev->is_nat_mem,
			  dev->is_ipv6ct_mem);

	if (!IPA_VALID_TBL_INDEX(tbl_index)) {
		IPAERR_RL("Unsupported table index %d\n", tbl_index);
		return -EPERM;
	}

	if (dev->tmp_mem) {
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

	IPADBG("Out\n");

	return 0;
}

static int ipa3_nat_send_del_table_cmd(
	uint8_t tbl_index)
{
	struct ipahal_imm_cmd_ip_v4_nat_init cmd;
	int result = 0;

	IPADBG("In\n");

	result =
		ipa3_nat_ipv6ct_create_del_table_cmd(
			tbl_index,
			IPA_NAT_PHYS_MEM_OFFSET,
			&ipa3_ctx->nat_mem.dev,
			&cmd.table_init);

	if (result) {
		IPAERR(
			"Fail to create immediate command to delete NAT table\n");
		goto bail;
	}

	cmd.index_table_addr =
		cmd.table_init.base_table_addr;
	cmd.index_table_addr_shared =
		cmd.table_init.base_table_addr_shared;
	cmd.index_table_expansion_addr =
		cmd.index_table_addr;
	cmd.index_table_expansion_addr_shared =
		cmd.index_table_addr_shared;
	cmd.public_addr_info = 0;

	IPADBG("Posting NAT delete command\n");

	result = ipa3_nat_send_init_cmd(&cmd, true);

	if (result) {
		IPAERR("Fail to send NAT delete immediate command\n");
		goto bail;
	}

bail:
	IPADBG("Out\n");

	return result;
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
int ipa3_del_nat_table(
	struct ipa_ioc_nat_ipv6ct_table_del *del)
{
	struct ipa3_nat_ipv6ct_common_mem *dev = &ipa3_ctx->nat_mem.dev;
	struct ipa3_nat_mem *nm_ptr = (struct ipa3_nat_mem *) dev;
	struct ipa3_nat_mem_loc_data *mld_ptr;
	enum ipa3_nat_mem_in nmi;

	int result = 0;

	IPADBG("In\n");

	if (!sram_compatible)
		del->mem_type = 0;

	nmi = del->mem_type;

	if (!dev->is_dev_init) {
		IPAERR("NAT hasn't been initialized\n");
		result = -EPERM;
		goto bail;
	}

	if (!IPA_VALID_TBL_INDEX(del->table_index)) {
		IPAERR_RL("Unsupported table index %d\n",
				  del->table_index);
		result = -EPERM;
		goto bail;
	}

	if (!IPA_VALID_NAT_MEM_IN(nmi)) {
		IPAERR_RL("Bad ipa3_nat_mem_in type\n");
		result = -EPERM;
		goto bail;
	}

	IPADBG("nmi(%s)\n", ipa3_nat_mem_in_as_str(nmi));

	mld_ptr = &nm_ptr->mem_loc[nmi];

	mutex_lock(&dev->lock);

	if (dev->is_hw_init) {

		result = ipa3_nat_send_del_table_cmd(del->table_index);

		if (result) {
			IPAERR(
				"Fail to send immediate command to delete NAT table\n");
			goto unlock;
		}
	}

	nm_ptr->public_ip_addr              = 0;

	mld_ptr->index_table_addr           = NULL;
	mld_ptr->index_table_expansion_addr = NULL;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0
		&&
		nm_ptr->pdn_mem.base) {

		struct ipa_mem_buffer *pdn_mem_ptr = &nm_ptr->pdn_mem;

		IPADBG("Freeing the PDN memory\n");

		dma_free_coherent(
			ipa3_ctx->pdev,
			pdn_mem_ptr->size,
			pdn_mem_ptr->base,
			pdn_mem_ptr->phys_base);

		pdn_mem_ptr->base = NULL;
	}

	ipa3_nat_ipv6ct_free_mem(dev);

unlock:
	mutex_unlock(&dev->lock);

bail:
	IPADBG("Out\n");

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
int ipa3_del_ipv6ct_table(
	struct ipa_ioc_nat_ipv6ct_table_del *del)
{
	struct ipa3_nat_ipv6ct_common_mem *dev = &ipa3_ctx->ipv6ct_mem.dev;

	int result = 0;

	IPADBG("In\n");

	if (!sram_compatible)
		del->mem_type = 0;

	if (!dev->is_dev_init) {
		IPAERR("IPv6 connection tracking hasn't been initialized\n");
		result = -EPERM;
		goto bail;
	}

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		IPAERR_RL("IPv6 connection tracking isn't supported\n");
		result = -EPERM;
		goto bail;
	}

	mutex_lock(&dev->lock);

	if (dev->is_hw_init) {
		result = ipa3_ipv6ct_send_del_table_cmd(del->table_index);

		if (result) {
			IPAERR("ipa3_ipv6ct_send_del_table_cmd() fail\n");
			goto unlock;
		}
	}

	ipa3_nat_ipv6ct_free_mem(&ipa3_ctx->ipv6ct_mem.dev);

unlock:
	mutex_unlock(&dev->lock);

bail:
	IPADBG("Out\n");

	return result;
}

int ipa3_nat_get_sram_info(
	struct ipa_nat_in_sram_info *info_ptr)
{
	struct ipa3_nat_ipv6ct_common_mem *dev = &ipa3_ctx->nat_mem.dev;

	int ret = 0;

	IPADBG("In\n");

	if (!info_ptr) {
		IPAERR("Bad argument passed\n");
		ret = -EINVAL;
		goto bail;
	}

	if (!dev->is_dev_init) {
		IPAERR_RL("NAT hasn't been initialized\n");
		ret = -EPERM;
		goto bail;
	}

	sram_compatible = true;

	memset(info_ptr,
		   0,
		   sizeof(struct ipa_nat_in_sram_info));

	/*
	 * Size of SRAM set aside for the NAT table.
	 */
	info_ptr->sram_mem_available_for_nat = IPA_RAM_NAT_SIZE;

	/*
	 * If table's phys addr in SRAM is not page aligned, it will be
	 * offset into the mmap'd VM by the amount calculated below.  This
	 * value can be used by the app, so that it can know where the
	 * table actually lives in the mmap'd VM...
	 */
	info_ptr->nat_table_offset_into_mmap =
		(ipa3_ctx->ipa_wrapper_base +
		 ipa3_ctx->ctrl->ipa_reg_base_ofst +
		 ipahal_get_reg_n_ofst(
			 IPA_SW_AREA_RAM_DIRECT_ACCESS_n,
			 0) +
		 IPA_RAM_NAT_OFST) & ~PAGE_MASK;

	/*
	 * If the offset above plus the size of the NAT table causes the
	 * table to extend beyond the next page boundary, the app needs to
	 * know it, so that it can increase the size used in the mmap
	 * request...
	 */
	info_ptr->best_nat_in_sram_size_rqst =
		roundup(
			info_ptr->nat_table_offset_into_mmap +
			IPA_RAM_NAT_SIZE,
			PAGE_SIZE);

bail:
	IPADBG("Out\n");

	return ret;
}

/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */
/*
 * This file contains entry functions for memory management of ISP driver
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/highmem.h>	/* for kmap */
#include <linux/io.h>		/* for page_to_phys */

#include "hmm/hmm.h"
#include "hmm/hmm_pool.h"
#include "hmm/hmm_bo.h"
#include "hmm/hmm_bo_dev.h"

#include "atomisp_internal.h"
#include "asm/cacheflush.h"
#include "mmu/isp_mmu.h"
#include "mmu/sh_mmu_mrfld.h"
#include "mmu/sh_mmu_mfld.h"

#ifdef USE_SSSE3
#include <asm/ssse3.h>
#endif

struct hmm_bo_device bo_device;
struct hmm_pool	dynamic_pool;
struct hmm_pool	reserved_pool;
static ia_css_ptr dummy_ptr;
bool atomisp_hmm_is_2400;

int hmm_init(void)
{
	int ret;

	if (atomisp_hmm_is_2400)
		ret = hmm_bo_device_init(&bo_device, &sh_mmu_mrfld,
					 ISP_VM_START, ISP_VM_SIZE);
	else
		ret = hmm_bo_device_init(&bo_device, &sh_mmu_mfld,
					 ISP_VM_START, ISP_VM_SIZE);

	if (ret)
		dev_err(atomisp_dev, "hmm_bo_device_init failed.\n");

	/*
	 * As hmm use NULL to indicate invalid ISP virtual address,
	 * and ISP_VM_START is defined to 0 too, so we allocate
	 * one piece of dummy memory, which should return value 0,
	 * at the beginning, to avoid hmm_alloc return 0 in the
	 * further allocation.
	 */
	dummy_ptr = hmm_alloc(1, HMM_BO_PRIVATE, 0, 0, HMM_UNCACHED);
	return ret;
}

void hmm_cleanup(void)
{
	/*
	 * free dummy memory first
	 */
	hmm_free(dummy_ptr);
	dummy_ptr = 0;

	hmm_bo_device_exit(&bo_device);
}

void hmm_cleanup_mmu_l2(void)
{
	hmm_bo_device_cleanup_mmu_l2(&bo_device);
}

ia_css_ptr hmm_alloc(size_t bytes, enum hmm_bo_type type,
		int from_highmem, void *userptr, bool cached)
{
	unsigned int pgnr;
	struct hmm_buffer_object *bo;
	int ret;

	/*Get page number from size*/
	pgnr = size_to_pgnr_ceil(bytes);

	/*Buffer object structure init*/
	bo = hmm_bo_create(&bo_device, pgnr);
	if (!bo) {
		dev_err(atomisp_dev, "hmm_bo_create failed.\n");
		goto create_bo_err;
	}

	/*Allocate virtual address in ISP virtual space*/
	ret = hmm_bo_alloc_vm(bo);
	if (ret) {
		dev_err(atomisp_dev,
			    "hmm_bo_alloc_vm failed.\n");
		goto alloc_vm_err;
	}

	/*Allocate pages for memory*/
	ret = hmm_bo_alloc_pages(bo, type, from_highmem, userptr, cached);
	if (ret) {
		dev_err(atomisp_dev,
			    "hmm_bo_alloc_pages failed.\n");
		goto alloc_page_err;
	}

	/*Combind the virtual address and pages togather*/
	ret = hmm_bo_bind(bo);
	if (ret) {
		dev_err(atomisp_dev, "hmm_bo_bind failed.\n");
		goto bind_err;
	}
	return bo->vm_node->start;

bind_err:
	hmm_bo_free_pages(bo);
alloc_page_err:
	hmm_bo_free_vm(bo);
alloc_vm_err:
	hmm_bo_unref(bo);
create_bo_err:
	return 0;
}

void hmm_free(ia_css_ptr virt)
{
	struct hmm_buffer_object *bo;

	bo = hmm_bo_device_search_start(&bo_device, (unsigned int)virt);

	if (!bo) {
		dev_err(atomisp_dev,
			    "can not find buffer object start with "
			    "address 0x%x\n", (unsigned int)virt);
		return;
	}

	hmm_bo_unbind(bo);

	hmm_bo_free_pages(bo);

	hmm_bo_free_vm(bo);

	hmm_bo_unref(bo);
}

static inline int hmm_check_bo(struct hmm_buffer_object *bo, unsigned int ptr)
{
	if (!bo) {
		dev_err(atomisp_dev,
			    "can not find buffer object contains "
			    "address 0x%x\n", ptr);
		return -EINVAL;
	}

	if (!hmm_bo_page_allocated(bo)) {
		dev_err(atomisp_dev,
			    "buffer object has no page allocated.\n");
		return -EINVAL;
	}

	if (!hmm_bo_vm_allocated(bo)) {
		dev_err(atomisp_dev,
			    "buffer object has no virtual address"
			    " space allocated.\n");
		return -EINVAL;
	}

	return 0;
}

/*Read function in ISP memory management*/
static int load_and_flush(ia_css_ptr virt, void *data, unsigned int bytes)
{
	unsigned int ptr;
	struct hmm_buffer_object *bo;
	unsigned int idx, offset, len;
	char *src, *des;
	int ret;

	ptr = (unsigned int)virt;

	bo = hmm_bo_device_search_in_range(&bo_device, ptr);
	ret = hmm_check_bo(bo, ptr);
	if (ret)
		return ret;

	des = (char *)data;
	while (bytes) {
		idx = (ptr - bo->vm_node->start) >> PAGE_SHIFT;
		offset = (ptr - bo->vm_node->start) - (idx << PAGE_SHIFT);

		src = (char *)kmap(bo->page_obj[idx].page);
		if (!src) {
			dev_err(atomisp_dev,
				    "kmap buffer object page failed: "
				    "pg_idx = %d\n", idx);
			return -EINVAL;
		}

		src += offset;

		if ((bytes + offset) >= PAGE_SIZE) {
			len = PAGE_SIZE - offset;
			bytes -= len;
		} else {
			len = bytes;
			bytes = 0;
		}

		ptr += len;	/* update ptr for next loop */

		if (des) {

#ifdef USE_SSSE3
			_ssse3_memcpy(des, src, len);
#else
			memcpy(des, src, len);
#endif
			des += len;
		}

		clflush_cache_range(src, len);

		kunmap(bo->page_obj[idx].page);
	}

	return 0;
}

/*Read function in ISP memory management*/
int hmm_load(ia_css_ptr virt, void *data, unsigned int bytes)
{
	if (!data) {
		dev_err(atomisp_dev,
			 "hmm_load NULL argument\n");
		return -EINVAL;
	}
	return load_and_flush(virt, data, bytes);
}

/*Flush hmm data from the data cache*/
int hmm_flush(ia_css_ptr virt, unsigned int bytes)
{
	return load_and_flush(virt, NULL, bytes);
}

/*Write function in ISP memory management*/
int hmm_store(ia_css_ptr virt, const void *data, unsigned int bytes)
{
	unsigned int ptr;
	struct hmm_buffer_object *bo;
	unsigned int idx, offset, len;
	char *src, *des;
	int ret;

	ptr = (unsigned int)virt;

	bo = hmm_bo_device_search_in_range(&bo_device, ptr);
	ret = hmm_check_bo(bo, ptr);
	if (ret)
		return ret;

	src = (char *)data;
	while (bytes) {
		idx = (ptr - bo->vm_node->start) >> PAGE_SHIFT;
		offset = (ptr - bo->vm_node->start) - (idx << PAGE_SHIFT);

		if (in_atomic())
			des = (char *)kmap_atomic(bo->page_obj[idx].page);
		else
			des = (char *)kmap(bo->page_obj[idx].page);

		if (!des) {
			dev_err(atomisp_dev,
				    "kmap buffer object page failed: "
				    "pg_idx = %d\n", idx);
			return -EINVAL;
		}

		des += offset;

		if ((bytes + offset) >= PAGE_SIZE) {
			len = PAGE_SIZE - offset;
			bytes -= len;
		} else {
			len = bytes;
			bytes = 0;
		}

		ptr += len;

#ifdef USE_SSSE3
		_ssse3_memcpy(des, src, len);
#else
		memcpy(des, src, len);
#endif
		src += len;

		clflush_cache_range(des, len);

		if (in_atomic())
			/*
			 * Note: kunmap_atomic requires return addr from
			 * kmap_atomic, not the page. See linux/highmem.h
			 */
			kunmap_atomic(des - offset);
		else
			kunmap(bo->page_obj[idx].page);
	}

	return 0;
}

/*memset function in ISP memory management*/
int hmm_set(ia_css_ptr virt, int c, unsigned int bytes)
{
	unsigned int ptr;
	struct hmm_buffer_object *bo;
	unsigned int idx, offset, len;
	char *des;
	int ret;

	ptr = virt;

	bo = hmm_bo_device_search_in_range(&bo_device, ptr);
	ret = hmm_check_bo(bo, ptr);
	if (ret)
		return ret;

	while (bytes) {
		idx = (ptr - bo->vm_node->start) >> PAGE_SHIFT;
		offset = (ptr - bo->vm_node->start) - (idx << PAGE_SHIFT);

		des = (char *)kmap(bo->page_obj[idx].page);
		if (!des) {
			dev_err(atomisp_dev,
				    "kmap buffer object page failed: "
				    "pg_idx = %d\n", idx);
			return -EINVAL;
		}
		des += offset;

		if ((bytes + offset) >= PAGE_SIZE) {
			len = PAGE_SIZE - offset;
			bytes -= len;
		} else {
			len = bytes;
			bytes = 0;
		}

		ptr += len;

		memset(des, c, len);

		clflush_cache_range(des, len);

		kunmap(bo->page_obj[idx].page);
	}

	return 0;
}

/*Virtual address to physical address convert*/
phys_addr_t hmm_virt_to_phys(ia_css_ptr virt)
{
	unsigned int ptr = (unsigned int)virt;
	unsigned int idx, offset;
	struct hmm_buffer_object *bo;

	bo = hmm_bo_device_search_in_range(&bo_device, ptr);
	if (!bo) {
		dev_err(atomisp_dev,
			    "can not find buffer object contains "
			    "address 0x%x\n", ptr);
		return -1;
	}

	idx = (ptr - bo->vm_node->start) >> PAGE_SHIFT;
	offset = (ptr - bo->vm_node->start) - (idx << PAGE_SHIFT);

	return page_to_phys(bo->page_obj[idx].page) + offset;
}

int hmm_mmap(struct vm_area_struct *vma, ia_css_ptr virt)
{
	struct hmm_buffer_object *bo;

	bo = hmm_bo_device_search_start(&bo_device, virt);
	if (!bo) {
		dev_err(atomisp_dev,
			    "can not find buffer object start with "
			    "address 0x%x\n", virt);
		return -EINVAL;
	}

	return hmm_bo_mmap(vma, bo);
}

/*Map ISP virtual address into IA virtual address*/
void *hmm_vmap(ia_css_ptr virt)
{
	struct hmm_buffer_object *bo;

	bo = hmm_bo_device_search_start(&bo_device, virt);
	if (!bo) {
		dev_err(atomisp_dev,
			    "can not find buffer object start with address 0x%x\n",
			    virt);
		return NULL;
	}

	return hmm_bo_vmap(bo);
}

void hmm_vunmap(ia_css_ptr virt)
{
	struct hmm_buffer_object *bo;

	bo = hmm_bo_device_search_start(&bo_device, virt);
	if (!bo) {
		dev_warn(atomisp_dev,
			"can not find buffer object start with address 0x%x\n",
			virt);
		return;
	}

	return hmm_bo_vunmap(bo);
}

int hmm_pool_register(unsigned int pool_size,
			enum hmm_pool_type pool_type)
{
	switch (pool_type) {
	case HMM_POOL_TYPE_RESERVED:
		reserved_pool.pops = &reserved_pops;
		return reserved_pool.pops->pool_init(&reserved_pool.pool_info,
							pool_size);
	case HMM_POOL_TYPE_DYNAMIC:
		dynamic_pool.pops = &dynamic_pops;
		return dynamic_pool.pops->pool_init(&dynamic_pool.pool_info,
							pool_size);
	default:
		dev_err(atomisp_dev, "invalid pool type.\n");
		return -EINVAL;
	}
}

void hmm_pool_unregister(enum hmm_pool_type pool_type)
{
	switch (pool_type) {
	case HMM_POOL_TYPE_RESERVED:
		if (reserved_pool.pops && reserved_pool.pops->pool_exit)
			reserved_pool.pops->pool_exit(&reserved_pool.pool_info);
		break;
	case HMM_POOL_TYPE_DYNAMIC:
		if (dynamic_pool.pops && dynamic_pool.pops->pool_exit)
			dynamic_pool.pops->pool_exit(&dynamic_pool.pool_info);
		break;
	default:
		dev_err(atomisp_dev, "invalid pool type.\n");
		break;
	}

	return;
}

void *hmm_isp_vaddr_to_host_vaddr(ia_css_ptr ptr)
{
	return hmm_vmap(ptr);
	/* vmunmap will be done in hmm_bo_release() */
}

ia_css_ptr hmm_host_vaddr_to_hrt_vaddr(const void *ptr)
{
	struct hmm_buffer_object *bo;

	bo = hmm_bo_device_search_vmap_start(&bo_device, ptr);
	if (bo)
		return bo->vm_node->start;

	dev_err(atomisp_dev,
		"can not find buffer object whose kernel virtual address is %p\n",
		ptr);
	return 0;
}

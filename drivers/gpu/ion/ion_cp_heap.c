/*
 * drivers/gpu/ion/ion_cp_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>

#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/memory_alloc.h>
#include <linux/seq_file.h>
#include <linux/fmem.h>
#include <linux/iommu.h>
#include <mach/msm_memtypes.h>
#include <mach/scm.h>
#include <mach/iommu_domains.h>
 #include "ion_priv.h"

#include <asm/mach/map.h>

/**
 * struct ion_cp_heap - container for the heap and shared heap data

 * @heap:	the heap information structure
 * @pool:	memory pool to allocate from.
 * @base:	the base address of the memory pool.
 * @permission_type:	Identifier for the memory used by SCM for protecting
 *			and unprotecting memory.
 * @secure_base:	Base address used when securing a heap that is shared.
 * @secure_size:	Size used when securing a heap that is shared.
 * @lock:	mutex to protect shared access.
 * @heap_protected:	Indicates whether heap has been protected or not.
 * @allocated_bytes:	the total number of allocated bytes from the pool.
 * @total_size:	the total size of the memory pool.
 * @request_region:	function pointer to call when first mapping of memory
 *			occurs.
 * @release_region:	function pointer to call when last mapping of memory
 *			unmapped.
 * @bus_id: token used with request/release region.
 * @kmap_cached_count:	the total number of times this heap has been mapped in
 *			kernel space (cached).
 * @kmap_uncached_count:the total number of times this heap has been mapped in
 *			kernel space (un-cached).
 * @umap_count:	the total number of times this heap has been mapped in
 *		user space.
 * @reusable: indicates if the memory should be reused via fmem.
 * @reserved_vrange: reserved virtual address range for use with fmem
 */
struct ion_cp_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
	unsigned int permission_type;
	ion_phys_addr_t secure_base;
	size_t secure_size;
	struct mutex lock;
	unsigned int heap_protected;
	unsigned long allocated_bytes;
	unsigned long total_size;
	int (*request_region)(void *);
	int (*release_region)(void *);
	void *bus_id;
	unsigned long kmap_cached_count;
	unsigned long kmap_uncached_count;
	unsigned long umap_count;
	int reusable;
	void *reserved_vrange;
};

enum {
	HEAP_NOT_PROTECTED = 0,
	HEAP_PROTECTED = 1,
};

static int ion_cp_protect_mem(unsigned int phy_base, unsigned int size,
			unsigned int permission_type);

static int ion_cp_unprotect_mem(unsigned int phy_base, unsigned int size,
				unsigned int permission_type);

/**
 * Get the total number of kernel mappings.
 * Must be called with heap->lock locked.
 */
static unsigned long ion_cp_get_total_kmap_count(
					const struct ion_cp_heap *cp_heap)
{
	return cp_heap->kmap_cached_count + cp_heap->kmap_uncached_count;
}

/**
 * Protects memory if heap is unsecured heap. Also ensures that we are in
 * the correct FMEM state if this heap is a reusable heap.
 * Must be called with heap->lock locked.
 */
static int ion_cp_protect(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	int ret_value = 0;

	if (cp_heap->heap_protected == HEAP_NOT_PROTECTED) {
		/* Make sure we are in C state when the heap is protected. */
		if (cp_heap->reusable && !cp_heap->allocated_bytes) {
			ret_value = fmem_set_state(FMEM_C_STATE);
			if (ret_value)
				goto out;
		}

		ret_value = ion_cp_protect_mem(cp_heap->secure_base,
				cp_heap->secure_size, cp_heap->permission_type);
		if (ret_value) {
			pr_err("Failed to protect memory for heap %s - "
				"error code: %d\n", heap->name, ret_value);

			if (cp_heap->reusable && !cp_heap->allocated_bytes) {
				if (fmem_set_state(FMEM_T_STATE) != 0)
					pr_err("%s: unable to transition heap to T-state\n",
						__func__);
			}
		} else {
			cp_heap->heap_protected = HEAP_PROTECTED;
			pr_debug("Protected heap %s @ 0x%lx\n",
				heap->name, cp_heap->base);
		}
	}
out:
	return ret_value;
}

/**
 * Unprotects memory if heap is secure heap. Also ensures that we are in
 * the correct FMEM state if this heap is a reusable heap.
 * Must be called with heap->lock locked.
 */
static void ion_cp_unprotect(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	if (cp_heap->heap_protected == HEAP_PROTECTED) {
		int error_code = ion_cp_unprotect_mem(
			cp_heap->secure_base, cp_heap->secure_size,
			cp_heap->permission_type);
		if (error_code) {
			pr_err("Failed to un-protect memory for heap %s - "
				"error code: %d\n", heap->name, error_code);
		} else  {
			cp_heap->heap_protected = HEAP_NOT_PROTECTED;
			pr_debug("Un-protected heap %s @ 0x%x\n", heap->name,
				(unsigned int) cp_heap->base);

			if (cp_heap->reusable && !cp_heap->allocated_bytes) {
				if (fmem_set_state(FMEM_T_STATE) != 0)
					pr_err("%s: unable to transition heap to T-state",
						__func__);
			}
		}
	}
}

ion_phys_addr_t ion_cp_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align,
				      unsigned long flags)
{
	unsigned long offset;
	unsigned long secure_allocation = flags & ION_SECURE;

	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	mutex_lock(&cp_heap->lock);
	if (!secure_allocation && cp_heap->heap_protected == HEAP_PROTECTED) {
		mutex_unlock(&cp_heap->lock);
		pr_err("ION cannot allocate un-secure memory from protected"
			" heap %s\n", heap->name);
		return ION_CP_ALLOCATE_FAIL;
	}

	if (secure_allocation &&
	    (cp_heap->umap_count > 0 || cp_heap->kmap_cached_count > 0)) {
		mutex_unlock(&cp_heap->lock);
		pr_err("ION cannot allocate secure memory from heap with "
			"outstanding mappings: User space: %lu, kernel space "
			"(cached): %lu\n", cp_heap->umap_count,
					   cp_heap->kmap_cached_count);
		return ION_CP_ALLOCATE_FAIL;
	}

	/*
	 * if this is the first reusable allocation, transition
	 * the heap
	 */
	if (cp_heap->reusable && !cp_heap->allocated_bytes) {
		if (fmem_set_state(FMEM_C_STATE) != 0) {
			mutex_unlock(&cp_heap->lock);
			return ION_RESERVED_ALLOCATE_FAIL;
		}
	}

	cp_heap->allocated_bytes += size;
	mutex_unlock(&cp_heap->lock);

	offset = gen_pool_alloc_aligned(cp_heap->pool,
					size, ilog2(align));

	if (!offset) {
		mutex_lock(&cp_heap->lock);
		if ((cp_heap->total_size -
		      cp_heap->allocated_bytes) > size)
			pr_debug("%s: heap %s has enough memory (%lx) but"
				" the allocation of size %lx still failed."
				" Memory is probably fragmented.\n",
				__func__, heap->name,
				cp_heap->total_size -
				cp_heap->allocated_bytes, size);

		cp_heap->allocated_bytes -= size;

		if (cp_heap->reusable && !cp_heap->allocated_bytes) {
			if (fmem_set_state(FMEM_T_STATE) != 0)
				pr_err("%s: unable to transition heap to T-state\n",
					__func__);
		}
		mutex_unlock(&cp_heap->lock);

		return ION_CP_ALLOCATE_FAIL;
	}

	return offset;
}

void ion_cp_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	if (addr == ION_CP_ALLOCATE_FAIL)
		return;
	gen_pool_free(cp_heap->pool, addr, size);

	mutex_lock(&cp_heap->lock);
	cp_heap->allocated_bytes -= size;

	if (cp_heap->reusable && !cp_heap->allocated_bytes) {
		if (fmem_set_state(FMEM_T_STATE) != 0)
			pr_err("%s: unable to transition heap to T-state\n",
				__func__);
	}
	mutex_unlock(&cp_heap->lock);
}

static int ion_cp_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	*addr = buffer->priv_phys;
	*len = buffer->size;
	return 0;
}

static int ion_cp_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	buffer->priv_phys = ion_cp_allocate(heap, size, align, flags);
	return buffer->priv_phys == ION_CP_ALLOCATE_FAIL ? -ENOMEM : 0;
}

static void ion_cp_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;

	ion_cp_free(heap, buffer->priv_phys, buffer->size);
	buffer->priv_phys = ION_CP_ALLOCATE_FAIL;
}

struct scatterlist *ion_cp_heap_create_sglist(struct ion_buffer *buffer)
{
	struct scatterlist *sglist;

	sglist = vmalloc(sizeof(*sglist));
	if (!sglist)
		return ERR_PTR(-ENOMEM);

	sg_init_table(sglist, 1);
	sglist->length = buffer->size;
	sglist->offset = 0;
	sglist->dma_address = buffer->priv_phys;

	return sglist;
}

struct scatterlist *ion_cp_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	return ion_cp_heap_create_sglist(buffer);
}

void ion_cp_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	if (buffer->sglist)
		vfree(buffer->sglist);
}

/**
 * Call request region for SMI memory of this is the first mapping.
 */
static int ion_cp_request_region(struct ion_cp_heap *cp_heap)
{
	int ret_value = 0;
	if ((cp_heap->umap_count + ion_cp_get_total_kmap_count(cp_heap)) == 0)
		if (cp_heap->request_region)
			ret_value = cp_heap->request_region(cp_heap->bus_id);
	return ret_value;
}

/**
 * Call release region for SMI memory of this is the last un-mapping.
 */
static int ion_cp_release_region(struct ion_cp_heap *cp_heap)
{
	int ret_value = 0;
	if ((cp_heap->umap_count + ion_cp_get_total_kmap_count(cp_heap)) == 0)
		if (cp_heap->release_region)
			ret_value = cp_heap->release_region(cp_heap->bus_id);
	return ret_value;
}

void *ion_map_fmem_buffer(struct ion_buffer *buffer, unsigned long phys_base,
				void *virt_base, unsigned long flags)
{
	int ret;
	unsigned int offset = buffer->priv_phys - phys_base;
	unsigned long start = ((unsigned long)virt_base) + offset;
	const struct mem_type *type = ION_IS_CACHED(flags) ?
				get_mem_type(MT_DEVICE_CACHED) :
				get_mem_type(MT_DEVICE);

	if (phys_base > buffer->priv_phys)
		return NULL;


	ret = ioremap_page_range(start, start + buffer->size,
			buffer->priv_phys, __pgprot(type->prot_pte));

	if (!ret)
		return (void *)start;
	else
		return NULL;
}

void *ion_cp_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer,
				   unsigned long flags)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	void *ret_value = NULL;

	mutex_lock(&cp_heap->lock);
	if ((cp_heap->heap_protected == HEAP_NOT_PROTECTED) ||
	    ((cp_heap->heap_protected == HEAP_PROTECTED) &&
	      !ION_IS_CACHED(flags))) {

		if (ion_cp_request_region(cp_heap)) {
			mutex_unlock(&cp_heap->lock);
			return NULL;
		}

		if (cp_heap->reusable) {
			ret_value = ion_map_fmem_buffer(buffer, cp_heap->base,
					cp_heap->reserved_vrange, flags);

		} else {
			if (ION_IS_CACHED(flags))
				ret_value = ioremap_cached(buffer->priv_phys,
							   buffer->size);
			else
				ret_value = ioremap(buffer->priv_phys,
						    buffer->size);
		}

		if (!ret_value) {
			ion_cp_release_region(cp_heap);
		} else {
			if (ION_IS_CACHED(buffer->flags))
				++cp_heap->kmap_cached_count;
			else
				++cp_heap->kmap_uncached_count;
		}
	}
	mutex_unlock(&cp_heap->lock);
	return ret_value;
}

void ion_cp_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	if (cp_heap->reusable)
		unmap_kernel_range((unsigned long)buffer->vaddr, buffer->size);
	else
		__arch_iounmap(buffer->vaddr);

	buffer->vaddr = NULL;

	mutex_lock(&cp_heap->lock);
	if (ION_IS_CACHED(buffer->flags))
		--cp_heap->kmap_cached_count;
	else
		--cp_heap->kmap_uncached_count;
	ion_cp_release_region(cp_heap);
	mutex_unlock(&cp_heap->lock);

	return;
}

int ion_cp_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			struct vm_area_struct *vma, unsigned long flags)
{
	int ret_value = -EAGAIN;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	mutex_lock(&cp_heap->lock);
	if (cp_heap->heap_protected == HEAP_NOT_PROTECTED) {
		if (ion_cp_request_region(cp_heap)) {
			mutex_unlock(&cp_heap->lock);
			return -EINVAL;
		}

		if (!ION_IS_CACHED(flags))
			vma->vm_page_prot = pgprot_writecombine(
							vma->vm_page_prot);

		ret_value =  remap_pfn_range(vma, vma->vm_start,
			__phys_to_pfn(buffer->priv_phys) + vma->vm_pgoff,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot);

		if (ret_value)
			ion_cp_release_region(cp_heap);
		else
			++cp_heap->umap_count;
	}
	mutex_unlock(&cp_heap->lock);
	return ret_value;
}

void ion_cp_heap_unmap_user(struct ion_heap *heap,
			struct ion_buffer *buffer)
{
	struct ion_cp_heap *cp_heap =
			container_of(heap, struct ion_cp_heap, heap);

	mutex_lock(&cp_heap->lock);
	--cp_heap->umap_count;
	ion_cp_release_region(cp_heap);
	mutex_unlock(&cp_heap->lock);
}

int ion_cp_cache_ops(struct ion_heap *heap, struct ion_buffer *buffer,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	unsigned long vstart, pstart;

	pstart = buffer->priv_phys + offset;
	vstart = (unsigned long)vaddr;

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		clean_caches(vstart, length, pstart);
		break;
	case ION_IOC_INV_CACHES:
		invalidate_caches(vstart, length, pstart);
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		clean_and_invalidate_caches(vstart, length, pstart);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ion_cp_print_debug(struct ion_heap *heap, struct seq_file *s)
{
	unsigned long total_alloc;
	unsigned long total_size;
	unsigned long umap_count;
	unsigned long kmap_count;
	unsigned long heap_protected;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	mutex_lock(&cp_heap->lock);
	total_alloc = cp_heap->allocated_bytes;
	total_size = cp_heap->total_size;
	umap_count = cp_heap->umap_count;
	kmap_count = ion_cp_get_total_kmap_count(cp_heap);
	heap_protected = cp_heap->heap_protected == HEAP_PROTECTED;
	mutex_unlock(&cp_heap->lock);

	seq_printf(s, "total bytes currently allocated: %lx\n", total_alloc);
	seq_printf(s, "total heap size: %lx\n", total_size);
	seq_printf(s, "umapping count: %lx\n", umap_count);
	seq_printf(s, "kmapping count: %lx\n", kmap_count);
	seq_printf(s, "heap protected: %s\n", heap_protected ? "Yes" : "No");
	seq_printf(s, "reusable: %s\n", cp_heap->reusable  ? "Yes" : "No");

	return 0;
}

int ion_cp_secure_heap(struct ion_heap *heap)
{
	int ret_value;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	mutex_lock(&cp_heap->lock);
	if (cp_heap->umap_count == 0 && cp_heap->kmap_cached_count == 0) {
		ret_value = ion_cp_protect(heap);
	} else {
		pr_err("ION cannot secure heap with outstanding mappings: "
		       "User space: %lu, kernel space (cached): %lu\n",
		       cp_heap->umap_count, cp_heap->kmap_cached_count);
		ret_value = -EINVAL;
	}

	mutex_unlock(&cp_heap->lock);
	return ret_value;
}

int ion_cp_unsecure_heap(struct ion_heap *heap)
{
	int ret_value = 0;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	mutex_lock(&cp_heap->lock);
	ion_cp_unprotect(heap);
	mutex_unlock(&cp_heap->lock);
	return ret_value;
}

static int ion_cp_heap_map_iommu(struct ion_buffer *buffer,
				struct ion_iommu_map *data,
				unsigned int domain_num,
				unsigned int partition_num,
				unsigned long align,
				unsigned long iova_length,
				unsigned long flags)
{
	struct iommu_domain *domain;
	int ret = 0;
	unsigned long extra;
	int prot = ION_IS_CACHED(flags) ? 1 : 0;
	struct scatterlist *sglist = 0;

	data->mapped_size = iova_length;

	if (!msm_use_iommu()) {
		data->iova_addr = buffer->priv_phys;
		return 0;
	}

	extra = iova_length - buffer->size;

	data->iova_addr = msm_allocate_iova_address(domain_num, partition_num,
						data->mapped_size, align);

	if (!data->iova_addr) {
		ret = -ENOMEM;
		goto out;
	}

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		ret = -ENOMEM;
		goto out1;
	}

	sglist = ion_cp_heap_create_sglist(buffer);
	if (IS_ERR_OR_NULL(sglist)) {
		ret = -ENOMEM;
		goto out1;
	}
	ret = iommu_map_range(domain, data->iova_addr, sglist,
			      buffer->size, prot);
	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, domain);
		goto out1;
	}

	if (extra) {
		unsigned long extra_iova_addr = data->iova_addr + buffer->size;
		ret = msm_iommu_map_extra(domain, extra_iova_addr, extra, prot);
		if (ret)
			goto out2;
	}
	vfree(sglist);
	return ret;

out2:
	iommu_unmap_range(domain, data->iova_addr, buffer->size);
out1:
	if (!IS_ERR_OR_NULL(sglist))
		vfree(sglist);
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);
out:
	return ret;
}

static void ion_cp_heap_unmap_iommu(struct ion_iommu_map *data)
{
	unsigned int domain_num;
	unsigned int partition_num;
	struct iommu_domain *domain;

	if (!msm_use_iommu())
		return;

	domain_num = iommu_map_domain(data);
	partition_num = iommu_map_partition(data);

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		WARN(1, "Could not get domain %d. Corruption?\n", domain_num);
		return;
	}

	iommu_unmap_range(domain, data->iova_addr, data->mapped_size);
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				data->mapped_size);

	return;
}

static struct ion_heap_ops cp_heap_ops = {
	.allocate = ion_cp_heap_allocate,
	.free = ion_cp_heap_free,
	.phys = ion_cp_heap_phys,
	.map_user = ion_cp_heap_map_user,
	.unmap_user = ion_cp_heap_unmap_user,
	.map_kernel = ion_cp_heap_map_kernel,
	.unmap_kernel = ion_cp_heap_unmap_kernel,
	.map_dma = ion_cp_heap_map_dma,
	.unmap_dma = ion_cp_heap_unmap_dma,
	.cache_op = ion_cp_cache_ops,
	.print_debug = ion_cp_print_debug,
	.secure_heap = ion_cp_secure_heap,
	.unsecure_heap = ion_cp_unsecure_heap,
	.map_iommu = ion_cp_heap_map_iommu,
	.unmap_iommu = ion_cp_heap_unmap_iommu,
};

struct ion_heap *ion_cp_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_cp_heap *cp_heap;
	int ret;

	cp_heap = kzalloc(sizeof(*cp_heap), GFP_KERNEL);
	if (!cp_heap)
		return ERR_PTR(-ENOMEM);

	mutex_init(&cp_heap->lock);

	cp_heap->pool = gen_pool_create(12, -1);
	if (!cp_heap->pool)
		goto free_heap;

	cp_heap->base = heap_data->base;
	ret = gen_pool_add(cp_heap->pool, cp_heap->base, heap_data->size, -1);
	if (ret < 0)
		goto destroy_pool;

	cp_heap->allocated_bytes = 0;
	cp_heap->umap_count = 0;
	cp_heap->kmap_cached_count = 0;
	cp_heap->kmap_uncached_count = 0;
	cp_heap->total_size = heap_data->size;
	cp_heap->heap.ops = &cp_heap_ops;
	cp_heap->heap.type = ION_HEAP_TYPE_CP;
	cp_heap->heap_protected = HEAP_NOT_PROTECTED;
	cp_heap->secure_base = cp_heap->base;
	cp_heap->secure_size = heap_data->size;
	if (heap_data->extra_data) {
		struct ion_cp_heap_pdata *extra_data =
				heap_data->extra_data;
		cp_heap->reusable = extra_data->reusable;
		cp_heap->reserved_vrange = extra_data->virt_addr;
		cp_heap->permission_type = extra_data->permission_type;
		if (extra_data->secure_size) {
			cp_heap->secure_base = extra_data->secure_base;
			cp_heap->secure_size = extra_data->secure_size;
		}
		if (extra_data->setup_region)
			cp_heap->bus_id = extra_data->setup_region();
		if (extra_data->request_region)
			cp_heap->request_region = extra_data->request_region;
		if (extra_data->release_region)
			cp_heap->release_region = extra_data->release_region;
	}
	return &cp_heap->heap;

destroy_pool:
	gen_pool_destroy(cp_heap->pool);

free_heap:
	kfree(cp_heap);

	return ERR_PTR(-ENOMEM);
}

void ion_cp_heap_destroy(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
	     container_of(heap, struct  ion_cp_heap, heap);

	gen_pool_destroy(cp_heap->pool);
	kfree(cp_heap);
	cp_heap = NULL;
}


/*  SCM related code for locking down memory for content protection */

#define SCM_CP_LOCK_CMD_ID	0x1
#define SCM_CP_PROTECT		0x1
#define SCM_CP_UNPROTECT	0x0

struct cp_lock_msg {
	unsigned int start;
	unsigned int end;
	unsigned int permission_type;
	unsigned char lock;
} __attribute__ ((__packed__));


static int ion_cp_protect_mem(unsigned int phy_base, unsigned int size,
			      unsigned int permission_type)
{
	struct cp_lock_msg cmd;
	cmd.start = phy_base;
	cmd.end = phy_base + size;
	cmd.permission_type = permission_type;
	cmd.lock = SCM_CP_PROTECT;

	return scm_call(SCM_SVC_CP, SCM_CP_LOCK_CMD_ID,
			&cmd, sizeof(cmd), NULL, 0);
}

static int ion_cp_unprotect_mem(unsigned int phy_base, unsigned int size,
				unsigned int permission_type)
{
	struct cp_lock_msg cmd;
	cmd.start = phy_base;
	cmd.end = phy_base + size;
	cmd.permission_type = permission_type;
	cmd.lock = SCM_CP_UNPROTECT;

	return scm_call(SCM_SVC_CP, SCM_CP_LOCK_CMD_ID,
			&cmd, sizeof(cmd), NULL, 0);
}

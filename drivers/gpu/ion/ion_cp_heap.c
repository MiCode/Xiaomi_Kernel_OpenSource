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
#include <mach/msm_memtypes.h>
#include <mach/scm.h>
#include "ion_priv.h"

#include <asm/mach/map.h>

/**
 * struct ion_cp_heap - container for the heap and shared heap data

 * @heap:	the heap information structure
 * @pool:	memory pool to allocate from.
 * @base:	the base address of the memory pool.
 * @permission_type:	Identifier for the memory used by SCM for protecting
 *			and unprotecting memory.
 * @lock:	mutex to protect shared access.
 * @heap_secured:	Identifies the heap_id as secure or not.
 * @allocated_bytes:	the total number of allocated bytes from the pool.
 * @total_size:	the total size of the memory pool.
 * @request_region:	function pointer to call when first mapping of memory
 *			occurs.
 * @release_region:	function pointer to call when last mapping of memory
 *			unmapped.
 * @bus_id: token used with request/release region.
 * @kmap_count:	the total number of times this heap has been mapped in
 *		kernel space.
 * @umap_count:	the total number of times this heap has been mapped in
 *		user space.
 * @alloc_count:the total number of times this heap has been allocated
 */
struct ion_cp_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
	unsigned int permission_type;
	struct mutex lock;
	unsigned int heap_secured;
	unsigned long allocated_bytes;
	unsigned long total_size;
	int (*request_region)(void *);
	int (*release_region)(void *);
	void *bus_id;
	unsigned long kmap_count;
	unsigned long umap_count;
	unsigned long alloc_count;
};

enum {
	NON_SECURED_HEAP = 0,
	SECURED_HEAP = 1,
};

static int ion_cp_protect_mem(unsigned int phy_base, unsigned int size,
			unsigned int permission_type);

static int ion_cp_unprotect_mem(unsigned int phy_base, unsigned int size,
				unsigned int permission_type);


/**
 * Protects memory if heap is unsecured heap.
 * Must be called with heap->lock locked.
 */
static int ion_cp_protect(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	int ret_value = 0;

	if (cp_heap->heap_secured == NON_SECURED_HEAP) {
		int ret_value = ion_cp_protect_mem(cp_heap->base,
				cp_heap->total_size, cp_heap->permission_type);
		if (ret_value) {
			pr_err("Failed to protect memory for heap %s - "
				"error code: %d", heap->name, ret_value);
		} else {
			cp_heap->heap_secured = SECURED_HEAP;
			pr_debug("Protected heap %s @ 0x%x",
				heap->name, (unsigned int) cp_heap->base);
		}
	}
	return ret_value;
}

/**
 * Unprotects memory if heap is secure heap.
 * Must be called with heap->lock locked.
 */
static void ion_cp_unprotect(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	if (cp_heap->heap_secured == SECURED_HEAP) {
		int error_code = ion_cp_unprotect_mem(
			cp_heap->base, cp_heap->total_size,
			cp_heap->permission_type);
		if (error_code) {
			pr_err("Failed to un-protect memory for heap %s - "
				"error code: %d", heap->name, error_code);
		} else  {
			cp_heap->heap_secured = NON_SECURED_HEAP;
			pr_debug("Un-protected heap %s @ 0x%x", heap->name,
				(unsigned int) cp_heap->base);
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

	if (!secure_allocation && cp_heap->heap_secured == SECURED_HEAP) {
		mutex_unlock(&cp_heap->lock);
		pr_err("ION cannot allocate un-secure memory from protected"
			" heap %s", heap->name);
		return ION_CP_ALLOCATE_FAIL;
	}

	if (secure_allocation && cp_heap->umap_count > 0) {
		mutex_unlock(&cp_heap->lock);
		pr_err("ION cannot allocate secure memory from heap with "
			"outstanding user space mappings for heap %s",
			heap->name);
		return ION_CP_ALLOCATE_FAIL;
	}

	if (secure_allocation && ion_cp_protect(heap)) {
		mutex_unlock(&cp_heap->lock);
		return ION_CP_ALLOCATE_FAIL;
	}

	cp_heap->allocated_bytes += size;
	++cp_heap->alloc_count;
	mutex_unlock(&cp_heap->lock);

	offset = gen_pool_alloc_aligned(cp_heap->pool,
					size, ilog2(align));

	if (!offset) {
		mutex_lock(&cp_heap->lock);
		if ((cp_heap->total_size -
		      cp_heap->allocated_bytes) > size)
			pr_debug("%s: heap %s has enough memory (%lx) but"
				" the allocation of size %lx still failed."
				" Memory is probably fragmented.",
				__func__, heap->name,
				cp_heap->total_size -
				cp_heap->allocated_bytes, size);

		cp_heap->allocated_bytes -= size;
		--cp_heap->alloc_count;

		if (cp_heap->alloc_count == 0)
			ion_cp_unprotect(heap);

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
	--cp_heap->alloc_count;

	if (cp_heap->alloc_count == 0)
		ion_cp_unprotect(heap);
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


/**
 * Checks if user space mapping is allowed.
 * NOTE: Will increment the mapping count if
 * mapping is allowed.
 * Will fail mapping if heap is secured.
 */
static unsigned int is_user_mapping_allowed(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	mutex_lock(&cp_heap->lock);

	if (cp_heap->heap_secured == SECURED_HEAP) {
		mutex_unlock(&cp_heap->lock);
		return 0;
	}
	++cp_heap->umap_count;

	mutex_unlock(&cp_heap->lock);
	return 1;
}

struct scatterlist *ion_cp_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	struct scatterlist *sglist;
	struct page *page = phys_to_page(buffer->priv_phys);

	if (page == NULL)
		return NULL;

	sglist = vmalloc(sizeof(*sglist));
	if (!sglist)
		return ERR_PTR(-ENOMEM);

	sg_init_table(sglist, 1);
	sg_set_page(sglist, page, buffer->size, 0);

	return sglist;
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
	if ((cp_heap->umap_count+cp_heap->kmap_count) == 1)
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
	if ((cp_heap->umap_count + cp_heap->kmap_count) == 0)
		if (cp_heap->release_region)
			ret_value = cp_heap->release_region(cp_heap->bus_id);
	return ret_value;
}

void *ion_cp_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer,
				   unsigned long flags)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	void *ret_value;

	mutex_lock(&cp_heap->lock);

	if (cp_heap->heap_secured == SECURED_HEAP && ION_IS_CACHED(flags)) {
		pr_err("Unable to map secured heap %s as cached", heap->name);
		mutex_unlock(&cp_heap->lock);
		return NULL;
	}

	++cp_heap->kmap_count;

	if (ion_cp_request_region(cp_heap)) {
		--cp_heap->kmap_count;
		mutex_unlock(&cp_heap->lock);
		return NULL;
	}
	mutex_unlock(&cp_heap->lock);

	if (ION_IS_CACHED(flags))
		ret_value = ioremap_cached(buffer->priv_phys, buffer->size);
	else
		ret_value = ioremap(buffer->priv_phys, buffer->size);

	if (!ret_value) {
		mutex_lock(&cp_heap->lock);
		--cp_heap->kmap_count;
		ion_cp_release_region(cp_heap);
		mutex_unlock(&cp_heap->lock);
	}
	return ret_value;
}

void ion_cp_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	__arch_iounmap(buffer->vaddr);
	buffer->vaddr = NULL;

	mutex_lock(&cp_heap->lock);
	--cp_heap->kmap_count;
	ion_cp_release_region(cp_heap);
	mutex_unlock(&cp_heap->lock);

	return;
}

int ion_cp_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			struct vm_area_struct *vma, unsigned long flags)
{
	int ret_value = -EAGAIN;
	if (is_user_mapping_allowed(heap)) {

		struct ion_cp_heap *cp_heap =
			container_of(heap, struct ion_cp_heap, heap);

		mutex_lock(&cp_heap->lock);
		if (ion_cp_request_region(cp_heap)) {
			mutex_unlock(&cp_heap->lock);
			return -EINVAL;
		}
		mutex_unlock(&cp_heap->lock);

		 if (ION_IS_CACHED(flags))
			ret_value =  remap_pfn_range(vma, vma->vm_start,
				__phys_to_pfn(buffer->priv_phys) +
				vma->vm_pgoff,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot);
		else
			ret_value = remap_pfn_range(vma, vma->vm_start,
				__phys_to_pfn(buffer->priv_phys) +
				vma->vm_pgoff,
				vma->vm_end - vma->vm_start,
				pgprot_noncached(vma->vm_page_prot));

		 if (ret_value) {
			mutex_lock(&cp_heap->lock);
			--cp_heap->umap_count;
			ion_cp_release_region(cp_heap);
			mutex_unlock(&cp_heap->lock);
		}
	}
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

static unsigned long ion_cp_get_allocated(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	unsigned long allocated_bytes;

	mutex_lock(&cp_heap->lock);
	allocated_bytes = cp_heap->allocated_bytes;
	mutex_unlock(&cp_heap->lock);

	return allocated_bytes;
}

static unsigned long ion_cp_get_total(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	return cp_heap->total_size;
}

int ion_cp_secure_heap(struct ion_heap *heap)
{
	int ret_value;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	mutex_lock(&cp_heap->lock);
	ret_value = ion_cp_protect(heap);
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
	.get_allocated = ion_cp_get_allocated,
	.get_total = ion_cp_get_total,
	.secure_heap = ion_cp_secure_heap,
	.unsecure_heap = ion_cp_unsecure_heap,
};

static unsigned long ion_cp_get_base(unsigned long size, int memory_type)
{
	switch (memory_type) {
	case ION_EBI_TYPE:
		return allocate_contiguous_ebi_nomap(size, PAGE_SIZE);
		break;
	case ION_SMI_TYPE:
		return allocate_contiguous_memory_nomap(size, MEMTYPE_SMI,
							PAGE_SIZE);
		break;
	default:
		return 0;
	}
}


struct ion_heap *ion_cp_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_cp_heap *cp_heap;
	int ret;

	cp_heap = kzalloc(sizeof(*cp_heap), GFP_KERNEL);
	if (!cp_heap)
		return ERR_PTR(-ENOMEM);

	heap_data->base = ion_cp_get_base(heap_data->size,
					heap_data->memory_type);
	if (!heap_data->base) {
		pr_err("%s: could not get memory for heap %s"
			" (id %x)\n", __func__, heap_data->name,
			heap_data->id);
		goto free_heap;
	}

	mutex_init(&cp_heap->lock);

	cp_heap->pool = gen_pool_create(12, -1);
	if (!cp_heap->pool)
		goto free_heap;

	cp_heap->base = heap_data->base;
	ret = gen_pool_add(cp_heap->pool, cp_heap->base, heap_data->size, -1);
	if (ret < 0)
		goto destroy_pool;

	cp_heap->permission_type = heap_data->permission_type;
	cp_heap->allocated_bytes = 0;
	cp_heap->alloc_count = 0;
	cp_heap->umap_count = 0;
	cp_heap->kmap_count = 0;
	cp_heap->total_size = heap_data->size;
	cp_heap->heap.ops = &cp_heap_ops;
	cp_heap->heap.type = ION_HEAP_TYPE_CP;
	cp_heap->heap_secured = NON_SECURED_HEAP;
	if (heap_data->setup_region)
		cp_heap->bus_id = heap_data->setup_region();
	if (heap_data->request_region)
		cp_heap->request_region = heap_data->request_region;
	if (heap_data->release_region)
		cp_heap->release_region = heap_data->release_region;

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
};


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

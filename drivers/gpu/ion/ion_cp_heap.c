/*
 * drivers/gpu/ion/ion_cp_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/msm_ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/memory_alloc.h>
#include <linux/seq_file.h>
#include <linux/fmem.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <trace/events/kmem.h>

#include <asm/mach/map.h>

#include <mach/msm_memtypes.h>
#include <mach/scm.h>
#include <mach/iommu_domains.h>

#include "ion_priv.h"

#include <asm/mach/map.h>
#include <asm/cacheflush.h>

#include "msm/ion_cp_common.h"
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
 * @iommu_iova: saved iova when mapping full heap at once.
 * @iommu_partition: partition used to map full heap.
 * @reusable: indicates if the memory should be reused via fmem.
 * @reserved_vrange: reserved virtual address range for use with fmem
 * @iommu_map_all:	Indicates whether we should map whole heap into IOMMU.
 * @iommu_2x_map_domain: Indicates the domain to use for overmapping.
 * @has_outer_cache:    set to 1 if outer cache is used, 0 otherwise.
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
	int (*heap_request_region)(void *);
	int (*heap_release_region)(void *);
	void *bus_id;
	unsigned long kmap_cached_count;
	unsigned long kmap_uncached_count;
	unsigned long umap_count;
	unsigned long iommu_iova[MAX_DOMAINS];
	unsigned long iommu_partition[MAX_DOMAINS];
	int reusable;
	void *reserved_vrange;
	int iommu_map_all;
	int iommu_2x_map_domain;
	unsigned int has_outer_cache;
	atomic_t protect_cnt;
	void *cpu_addr;
	size_t heap_size;
	dma_addr_t handle;
	int cma;
	int allow_non_secure_allocation;
};

enum {
	HEAP_NOT_PROTECTED = 0,
	HEAP_PROTECTED = 1,
};

#define DMA_ALLOC_TRIES	5

static int allocate_heap_memory(struct ion_heap *heap)
{
	struct device *dev = heap->priv;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	int ret;
	int tries = 0;
	DEFINE_DMA_ATTRS(attrs);
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);


	if (cp_heap->cpu_addr)
		return 0;

	while (!cp_heap->cpu_addr && (++tries < DMA_ALLOC_TRIES)) {
		cp_heap->cpu_addr = dma_alloc_attrs(dev,
						cp_heap->heap_size,
						&(cp_heap->handle),
						0,
						&attrs);
		if (!cp_heap->cpu_addr) {
			trace_ion_cp_alloc_retry(tries);
			msleep(20);
		}
	}

	if (!cp_heap->cpu_addr)
		goto out;

	cp_heap->base = cp_heap->handle;

	cp_heap->pool = gen_pool_create(12, -1);
	if (!cp_heap->pool)
		goto out_free;

	ret = gen_pool_add(cp_heap->pool, cp_heap->base,
				cp_heap->heap_size, -1);
	if (ret < 0)
		goto out_pool;

	return 0;

out_pool:
	gen_pool_destroy(cp_heap->pool);
out_free:
	dma_free_coherent(dev, cp_heap->heap_size, cp_heap->cpu_addr,
				cp_heap->handle);
out:
	return ION_CP_ALLOCATE_FAIL;
}

static void free_heap_memory(struct ion_heap *heap)
{
	struct device *dev = heap->priv;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	/* release memory */
	dma_free_coherent(dev, cp_heap->heap_size, cp_heap->cpu_addr,
				cp_heap->handle);
	gen_pool_destroy(cp_heap->pool);
	cp_heap->pool = NULL;
	cp_heap->cpu_addr = 0;
}



/**
 * Get the total number of kernel mappings.
 * Must be called with heap->lock locked.
 */
static unsigned long ion_cp_get_total_kmap_count(
					const struct ion_cp_heap *cp_heap)
{
	return cp_heap->kmap_cached_count + cp_heap->kmap_uncached_count;
}

static int ion_on_first_alloc(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	int ret_value;

	if (cp_heap->reusable) {
		ret_value = fmem_set_state(FMEM_C_STATE);
		if (ret_value)
			return 1;
	}

	if (cp_heap->cma) {
		ret_value = allocate_heap_memory(heap);
		if (ret_value)
			return 1;
	}
	return 0;
}

static void ion_on_last_free(struct ion_heap *heap)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	if (cp_heap->reusable)
		if (fmem_set_state(FMEM_T_STATE) != 0)
			pr_err("%s: unable to transition heap to T-state\n",
				__func__);

	if (cp_heap->cma)
		free_heap_memory(heap);
}

/**
 * Protects memory if heap is unsecured heap. Also ensures that we are in
 * the correct FMEM state if this heap is a reusable heap.
 * Must be called with heap->lock locked.
 */
static int ion_cp_protect(struct ion_heap *heap, int version, void *data)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	int ret_value = 0;

	if (atomic_inc_return(&cp_heap->protect_cnt) == 1) {
		/* Make sure we are in C state when the heap is protected. */
		if (!cp_heap->allocated_bytes)
			if (ion_on_first_alloc(heap))
				goto out;

		ret_value = ion_cp_protect_mem(cp_heap->secure_base,
				cp_heap->secure_size, cp_heap->permission_type,
				version, data);
		if (ret_value) {
			pr_err("Failed to protect memory for heap %s - "
				"error code: %d\n", heap->name, ret_value);

			if (!cp_heap->allocated_bytes)
				ion_on_last_free(heap);

			atomic_dec(&cp_heap->protect_cnt);
		} else {
			cp_heap->heap_protected = HEAP_PROTECTED;
			pr_debug("Protected heap %s @ 0x%pa\n",
				heap->name, &cp_heap->base);
		}
	}
out:
	pr_debug("%s: protect count is %d\n", __func__,
		atomic_read(&cp_heap->protect_cnt));
	BUG_ON(atomic_read(&cp_heap->protect_cnt) < 0);
	return ret_value;
}

/**
 * Unprotects memory if heap is secure heap. Also ensures that we are in
 * the correct FMEM state if this heap is a reusable heap.
 * Must be called with heap->lock locked.
 */
static void ion_cp_unprotect(struct ion_heap *heap, int version, void *data)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	if (atomic_dec_and_test(&cp_heap->protect_cnt)) {
		int error_code = ion_cp_unprotect_mem(
			cp_heap->secure_base, cp_heap->secure_size,
			cp_heap->permission_type, version, data);
		if (error_code) {
			pr_err("Failed to un-protect memory for heap %s - "
				"error code: %d\n", heap->name, error_code);
		} else  {
			cp_heap->heap_protected = HEAP_NOT_PROTECTED;
			pr_debug("Un-protected heap %s @ 0x%x\n", heap->name,
				(unsigned int) cp_heap->base);

			if (!cp_heap->allocated_bytes)
				ion_on_last_free(heap);
		}
	}
	pr_debug("%s: protect count is %d\n", __func__,
		atomic_read(&cp_heap->protect_cnt));
	BUG_ON(atomic_read(&cp_heap->protect_cnt) < 0);
}

ion_phys_addr_t ion_cp_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align,
				      unsigned long flags)
{
	unsigned long offset;
	unsigned long secure_allocation = flags & ION_SECURE;
	unsigned long force_contig = flags & ION_FORCE_CONTIGUOUS;

	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);

	mutex_lock(&cp_heap->lock);
	if (!secure_allocation && cp_heap->heap_protected == HEAP_PROTECTED) {
		mutex_unlock(&cp_heap->lock);
		pr_err("ION cannot allocate un-secure memory from protected"
			" heap %s\n", heap->name);
		return ION_CP_ALLOCATE_FAIL;
	}

	if (!force_contig && !secure_allocation &&
	     !cp_heap->allow_non_secure_allocation) {
		mutex_unlock(&cp_heap->lock);
		pr_debug("%s: non-secure allocation disallowed from this heap\n",
			__func__);
		return ION_CP_ALLOCATE_FAIL;
	}

	/*
	 * The check above already checked for non-secure allocations when the
	 * heap is protected. HEAP_PROTECTED implies that this must be a secure
	 * allocation. If the heap is protected and there are userspace or
	 * cached kernel mappings, something has gone wrong in the security
	 * model.
	 */
	if (cp_heap->heap_protected == HEAP_PROTECTED) {
		BUG_ON(cp_heap->umap_count != 0);
		BUG_ON(cp_heap->kmap_cached_count != 0);
	}

	/*
	 * if this is the first reusable allocation, transition
	 * the heap
	 */
	if (!cp_heap->allocated_bytes)
		if (ion_on_first_alloc(heap)) {
			mutex_unlock(&cp_heap->lock);
			return ION_RESERVED_ALLOCATE_FAIL;
		}

	cp_heap->allocated_bytes += size;
	mutex_unlock(&cp_heap->lock);

	offset = gen_pool_alloc_aligned(cp_heap->pool,
					size, ilog2(align));

	if (!offset) {
		mutex_lock(&cp_heap->lock);
		cp_heap->allocated_bytes -= size;
		if ((cp_heap->total_size -
		     cp_heap->allocated_bytes) >= size)
			pr_debug("%s: heap %s has enough memory (%lx) but"
				" the allocation of size %lx still failed."
				" Memory is probably fragmented.\n",
				__func__, heap->name,
				cp_heap->total_size -
				cp_heap->allocated_bytes, size);
		if (!cp_heap->allocated_bytes &&
			cp_heap->heap_protected == HEAP_NOT_PROTECTED)
			ion_on_last_free(heap);
		mutex_unlock(&cp_heap->lock);

		return ION_CP_ALLOCATE_FAIL;
	}

	return offset;
}

static void iommu_unmap_all(unsigned long domain_num,
			    struct ion_cp_heap *cp_heap)
{
	unsigned long left_to_unmap = cp_heap->total_size;
	unsigned long page_size = SZ_64K;

	struct iommu_domain *domain = msm_get_iommu_domain(domain_num);
	if (domain) {
		unsigned long temp_iova = cp_heap->iommu_iova[domain_num];

		while (left_to_unmap) {
			iommu_unmap(domain, temp_iova, page_size);
			temp_iova += page_size;
			left_to_unmap -= page_size;
		}
		if (domain_num == cp_heap->iommu_2x_map_domain)
			msm_iommu_unmap_extra(domain, temp_iova,
					      cp_heap->total_size, SZ_64K);
	} else {
		pr_err("Unable to get IOMMU domain %lu\n", domain_num);
	}
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

	if (!cp_heap->allocated_bytes &&
		cp_heap->heap_protected == HEAP_NOT_PROTECTED)
		ion_on_last_free(heap);

	/* Unmap everything if we previously mapped the whole heap at once. */
	if (!cp_heap->allocated_bytes) {
		unsigned int i;
		for (i = 0; i < MAX_DOMAINS; ++i) {
			if (cp_heap->iommu_iova[i]) {
				unsigned long vaddr_len = cp_heap->total_size;

				if (i == cp_heap->iommu_2x_map_domain)
					vaddr_len <<= 1;
				iommu_unmap_all(i, cp_heap);

				msm_free_iova_address(cp_heap->iommu_iova[i], i,
						cp_heap->iommu_partition[i],
						vaddr_len);
			}
			cp_heap->iommu_iova[i] = 0;
			cp_heap->iommu_partition[i] = 0;
		}
	}
	mutex_unlock(&cp_heap->lock);
}

static int ion_cp_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	struct ion_cp_buffer *buf = buffer->priv_virt;

	*addr = buf->buffer;
	*len = buffer->size;
	return 0;
}

static int ion_cp_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct ion_cp_buffer *buf;
	phys_addr_t addr;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ION_CP_ALLOCATE_FAIL;

	addr = ion_cp_allocate(heap, size, align, flags);
	if (addr == ION_CP_ALLOCATE_FAIL)
		return -ENOMEM;

	buf->buffer = addr;
	buf->want_delayed_unsecure = 0;
	atomic_set(&buf->secure_cnt, 0);
	mutex_init(&buf->lock);
	buf->is_secure = flags & ION_SECURE ? 1 : 0;
	buffer->priv_virt = buf;

	return 0;
}

static void ion_cp_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_cp_buffer *buf = buffer->priv_virt;

	ion_cp_free(heap, buf->buffer, buffer->size);
	WARN_ON(atomic_read(&buf->secure_cnt));
	WARN_ON(atomic_read(&buf->map_cnt));
	kfree(buf);

	buffer->priv_virt = NULL;
}

struct sg_table *ion_cp_heap_create_sg_table(struct ion_buffer *buffer)
{
	size_t chunk_size = buffer->size;
	struct sg_table *table;
	int ret, i, n_chunks;
	struct scatterlist *sg;
	struct ion_cp_buffer *buf = buffer->priv_virt;

	if (ION_IS_CACHED(buffer->flags))
		chunk_size = PAGE_SIZE;
	else if (buf->is_secure && IS_ALIGNED(buffer->size, SZ_1M))
		chunk_size = SZ_1M;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);

	n_chunks = DIV_ROUND_UP(buffer->size, chunk_size);

	ret = sg_alloc_table(table, n_chunks, GFP_KERNEL);
	if (ret)
		goto err0;

	for_each_sg(table->sgl, sg, table->nents, i) {
		sg_dma_address(sg) = buf->buffer + i * chunk_size;
		sg->length = chunk_size;
		sg->offset = 0;
	}

	return table;
err0:
	kfree(table);
	return ERR_PTR(ret);
}

struct sg_table *ion_cp_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	return ion_cp_heap_create_sg_table(buffer);
}

void ion_cp_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	if (buffer->sg_table)
		sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	buffer->sg_table = 0;
}

/**
 * Call request region for SMI memory of this is the first mapping.
 */
static int ion_cp_request_region(struct ion_cp_heap *cp_heap)
{
	int ret_value = 0;
	if ((cp_heap->umap_count + ion_cp_get_total_kmap_count(cp_heap)) == 0)
		if (cp_heap->heap_request_region)
			ret_value = cp_heap->heap_request_region(
					cp_heap->bus_id);
	return ret_value;
}

/**
 * Call release region for SMI memory of this is the last un-mapping.
 */
static int ion_cp_release_region(struct ion_cp_heap *cp_heap)
{
	int ret_value = 0;
	if ((cp_heap->umap_count + ion_cp_get_total_kmap_count(cp_heap)) == 0)
		if (cp_heap->heap_release_region)
			ret_value = cp_heap->heap_release_region(
					cp_heap->bus_id);
	return ret_value;
}

void *ion_map_fmem_buffer(struct ion_buffer *buffer, unsigned long phys_base,
				void *virt_base, unsigned long flags)
{
	int ret;
	struct ion_cp_buffer *buf = buffer->priv_virt;
	unsigned int offset = buf->buffer - phys_base;
	unsigned long start = ((unsigned long)virt_base) + offset;
	const struct mem_type *type = ION_IS_CACHED(flags) ?
				get_mem_type(MT_DEVICE_CACHED) :
				get_mem_type(MT_DEVICE);

	if (phys_base > buf->buffer)
		return NULL;


	ret = ioremap_pages(start, buf->buffer, buffer->size, type);

	if (!ret)
		return (void *)start;
	else
		return NULL;
}

void *ion_cp_heap_map_kernel(struct ion_heap *heap, struct ion_buffer *buffer)
{
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	void *ret_value = NULL;
	struct ion_cp_buffer *buf = buffer->priv_virt;

	mutex_lock(&cp_heap->lock);
	if ((cp_heap->heap_protected == HEAP_NOT_PROTECTED) ||
	    ((cp_heap->heap_protected == HEAP_PROTECTED) &&
	      !ION_IS_CACHED(buffer->flags))) {

		if (ion_cp_request_region(cp_heap)) {
			mutex_unlock(&cp_heap->lock);
			return NULL;
		}

		if (cp_heap->reusable) {
			ret_value = ion_map_fmem_buffer(buffer, cp_heap->base,
				cp_heap->reserved_vrange, buffer->flags);
		} else if (cp_heap->cma) {
			int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
			struct page **pages = vmalloc(
						sizeof(struct page *) * npages);
			int i;
			pgprot_t pgprot;

			if (ION_IS_CACHED(buffer->flags))
				pgprot = PAGE_KERNEL;
			else
				pgprot = pgprot_writecombine(PAGE_KERNEL);

			for (i = 0; i < npages; i++) {
				pages[i] = phys_to_page(buf->buffer +
						i * PAGE_SIZE);
			}
			ret_value = vmap(pages, npages, VM_IOREMAP, pgprot);
			vfree(pages);
		} else {
			if (ION_IS_CACHED(buffer->flags))
				ret_value = ioremap_cached(buf->buffer,
							   buffer->size);
			else
				ret_value = ioremap(buf->buffer,
						    buffer->size);
		}

		if (!ret_value) {
			ion_cp_release_region(cp_heap);
		} else {
			if (ION_IS_CACHED(buffer->flags))
				++cp_heap->kmap_cached_count;
			else
				++cp_heap->kmap_uncached_count;
			atomic_inc(&buf->map_cnt);
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
	struct ion_cp_buffer *buf = buffer->priv_virt;

	if (cp_heap->reusable)
		unmap_kernel_range((unsigned long)buffer->vaddr, buffer->size);
	else if (cp_heap->cma)
		vunmap(buffer->vaddr);
	else
		__arm_iounmap(buffer->vaddr);

	buffer->vaddr = NULL;

	mutex_lock(&cp_heap->lock);
	if (ION_IS_CACHED(buffer->flags))
		--cp_heap->kmap_cached_count;
	else
		--cp_heap->kmap_uncached_count;

	atomic_dec(&buf->map_cnt);
	ion_cp_release_region(cp_heap);
	mutex_unlock(&cp_heap->lock);

	return;
}

int ion_cp_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			struct vm_area_struct *vma)
{
	int ret_value = -EAGAIN;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	struct ion_cp_buffer *buf = buffer->priv_virt;

	mutex_lock(&cp_heap->lock);
	if (cp_heap->heap_protected == HEAP_NOT_PROTECTED && !buf->is_secure) {
		if (ion_cp_request_region(cp_heap)) {
			mutex_unlock(&cp_heap->lock);
			return -EINVAL;
		}

		if (!ION_IS_CACHED(buffer->flags))
			vma->vm_page_prot = pgprot_writecombine(
							vma->vm_page_prot);

		ret_value =  remap_pfn_range(vma, vma->vm_start,
			__phys_to_pfn(buf->buffer) + vma->vm_pgoff,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot);

		if (ret_value) {
			ion_cp_release_region(cp_heap);
		} else {
			atomic_inc(&buf->map_cnt);
			++cp_heap->umap_count;
		}

	}
	mutex_unlock(&cp_heap->lock);
	return ret_value;
}

void ion_cp_heap_unmap_user(struct ion_heap *heap,
			struct ion_buffer *buffer)
{
	struct ion_cp_heap *cp_heap =
			container_of(heap, struct ion_cp_heap, heap);
	struct ion_cp_buffer *buf = buffer->priv_virt;

	mutex_lock(&cp_heap->lock);
	--cp_heap->umap_count;
	atomic_dec(&buf->map_cnt);
	ion_cp_release_region(cp_heap);
	mutex_unlock(&cp_heap->lock);
}

int ion_cp_cache_ops(struct ion_heap *heap, struct ion_buffer *buffer,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	void (*outer_cache_op)(phys_addr_t, phys_addr_t);
	struct ion_cp_heap *cp_heap =
	     container_of(heap, struct  ion_cp_heap, heap);
	struct ion_cp_buffer *buf = buffer->priv_virt;

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		dmac_clean_range(vaddr, vaddr + length);
		outer_cache_op = outer_clean_range;
		break;
	case ION_IOC_INV_CACHES:
		dmac_inv_range(vaddr, vaddr + length);
		outer_cache_op = outer_inv_range;
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		dmac_flush_range(vaddr, vaddr + length);
		outer_cache_op = outer_flush_range;
		break;
	default:
		return -EINVAL;
	}

	if (cp_heap->has_outer_cache) {
		unsigned long pstart = buf->buffer + offset;
		outer_cache_op(pstart, pstart + length);
	}
	return 0;
}

static int ion_cp_print_debug(struct ion_heap *heap, struct seq_file *s,
			      const struct rb_root *mem_map)
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

	if (mem_map) {
		unsigned long base = cp_heap->base;
		unsigned long size = cp_heap->total_size;
		unsigned long end = base+size;
		unsigned long last_end = base;
		struct rb_node *n;

		seq_printf(s, "\nMemory Map\n");
		seq_printf(s, "%16.s %14.s %14.s %14.s\n",
			   "client", "start address", "end address",
			   "size (hex)");

		for (n = rb_first(mem_map); n; n = rb_next(n)) {
			struct mem_map_data *data =
					rb_entry(n, struct mem_map_data, node);
			const char *client_name = "(null)";

			if (last_end < data->addr) {
				phys_addr_t da;

				da = data->addr-1;
				seq_printf(s, "%16.s %14pa %14pa %14lu (%lx)\n",
					   "FREE", &last_end, &da,
					   data->addr-last_end,
					   data->addr-last_end);
			}

			if (data->client_name)
				client_name = data->client_name;

			seq_printf(s, "%16.s %14pa %14pa %14lu (%lx)\n",
				   client_name, &data->addr,
				   &data->addr_end,
				   data->size, data->size);
			last_end = data->addr_end+1;
		}
		if (last_end < end) {
			seq_printf(s, "%16.s %14lx %14lx %14lu (%lx)\n", "FREE",
				last_end, end-1, end-last_end, end-last_end);
		}
	}

	return 0;
}

int ion_cp_secure_heap(struct ion_heap *heap, int version, void *data)
{
	int ret_value;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	mutex_lock(&cp_heap->lock);
	if (cp_heap->umap_count == 0 && cp_heap->kmap_cached_count == 0) {
		ret_value = ion_cp_protect(heap, version, data);
	} else {
		pr_err("ION cannot secure heap with outstanding mappings: "
		       "User space: %lu, kernel space (cached): %lu\n",
		       cp_heap->umap_count, cp_heap->kmap_cached_count);
		ret_value = -EINVAL;
	}

	mutex_unlock(&cp_heap->lock);
	return ret_value;
}

int ion_cp_unsecure_heap(struct ion_heap *heap, int version, void *data)
{
	int ret_value = 0;
	struct ion_cp_heap *cp_heap =
		container_of(heap, struct ion_cp_heap, heap);
	mutex_lock(&cp_heap->lock);
	ion_cp_unprotect(heap, version, data);
	mutex_unlock(&cp_heap->lock);
	return ret_value;
}

static int iommu_map_all(unsigned long domain_num, struct ion_cp_heap *cp_heap,
			int partition, unsigned long prot)
{
	unsigned long left_to_map = cp_heap->total_size;
	unsigned long page_size = SZ_64K;
	int ret_value = 0;
	unsigned long virt_addr_len = cp_heap->total_size;
	struct iommu_domain *domain = msm_get_iommu_domain(domain_num);

	/* If we are mapping into the video domain we need to map twice the
	 * size of the heap to account for prefetch issue in video core.
	 */
	if (domain_num == cp_heap->iommu_2x_map_domain)
		virt_addr_len <<= 1;

	if (cp_heap->total_size & (SZ_64K-1)) {
		pr_err("Heap size is not aligned to 64K, cannot map into IOMMU\n");
		ret_value = -EINVAL;
	}
	if (cp_heap->base & (SZ_64K-1)) {
		pr_err("Heap physical address is not aligned to 64K, cannot map into IOMMU\n");
		ret_value = -EINVAL;
	}
	if (!ret_value && domain) {
		unsigned long temp_phys = cp_heap->base;
		unsigned long temp_iova;

		ret_value = msm_allocate_iova_address(domain_num, partition,
						virt_addr_len, SZ_64K,
						&temp_iova);

		if (ret_value) {
			pr_err("%s: could not allocate iova from domain %lu, partition %d\n",
				__func__, domain_num, partition);
			goto out;
		}
		cp_heap->iommu_iova[domain_num] = temp_iova;

		while (left_to_map) {
			int ret = iommu_map(domain, temp_iova, temp_phys,
					page_size, prot);
			if (ret) {
				pr_err("%s: could not map %lx in domain %p, error: %d\n",
					__func__, temp_iova, domain, ret);
				ret_value = -EAGAIN;
				goto free_iova;
			}
			temp_iova += page_size;
			temp_phys += page_size;
			left_to_map -= page_size;
		}
		if (domain_num == cp_heap->iommu_2x_map_domain)
			ret_value = msm_iommu_map_extra(domain, temp_iova,
							cp_heap->base,
							cp_heap->total_size,
							SZ_64K, prot);
		if (ret_value)
			goto free_iova;
	} else {
		pr_err("Unable to get IOMMU domain %lu\n", domain_num);
		ret_value = -ENOMEM;
	}
	goto out;

free_iova:
	msm_free_iova_address(cp_heap->iommu_iova[domain_num], domain_num,
			      partition, virt_addr_len);
out:
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
	struct ion_cp_heap *cp_heap =
		container_of(buffer->heap, struct ion_cp_heap, heap);
	int prot = IOMMU_WRITE | IOMMU_READ;
	struct ion_cp_buffer *buf = buffer->priv_virt;
	prot |= ION_IS_CACHED(flags) ? IOMMU_CACHE : 0;

	data->mapped_size = iova_length;

	if (!msm_use_iommu()) {
		data->iova_addr = buf->buffer;
		return 0;
	}

	if (cp_heap->iommu_iova[domain_num]) {
		/* Already mapped. */
		unsigned long offset = buf->buffer - cp_heap->base;
		data->iova_addr = cp_heap->iommu_iova[domain_num] + offset;
		return 0;
	} else if (cp_heap->iommu_map_all) {
		ret = iommu_map_all(domain_num, cp_heap, partition_num, prot);
		if (!ret) {
			unsigned long offset =
					buf->buffer - cp_heap->base;
			data->iova_addr =
				cp_heap->iommu_iova[domain_num] + offset;
			cp_heap->iommu_partition[domain_num] = partition_num;
			/*
			clear delayed map flag so that we don't interfere
			with this feature (we are already delaying).
			*/
			data->flags &= ~ION_IOMMU_UNMAP_DELAYED;
			return 0;
		} else {
			cp_heap->iommu_iova[domain_num] = 0;
			cp_heap->iommu_partition[domain_num] = 0;
			return ret;
		}
	}

	extra = iova_length - buffer->size;

	ret = msm_allocate_iova_address(domain_num, partition_num,
						data->mapped_size, align,
						&data->iova_addr);

	if (ret)
		goto out;

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		ret = -ENOMEM;
		goto out1;
	}

	ret = iommu_map_range(domain, data->iova_addr, buffer->sg_table->sgl,
			      buffer->size, prot);
	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, domain);
		goto out1;
	}

	if (extra) {
		unsigned long extra_iova_addr = data->iova_addr + buffer->size;
		unsigned long phys_addr = sg_phys(buffer->sg_table->sgl);
		ret = msm_iommu_map_extra(domain, extra_iova_addr, phys_addr,
					extra, SZ_4K, prot);
		if (ret)
			goto out2;
	}
	return ret;

out2:
	iommu_unmap_range(domain, data->iova_addr, buffer->size);
out1:
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
	struct ion_cp_heap *cp_heap =
		container_of(data->buffer->heap, struct ion_cp_heap, heap);

	if (!msm_use_iommu())
		return;


	domain_num = iommu_map_domain(data);

	/* If we are mapping everything we'll wait to unmap until everything
	   is freed. */
	if (cp_heap->iommu_iova[domain_num])
		return;

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
	.secure_buffer = ion_cp_secure_buffer,
	.unsecure_buffer = ion_cp_unsecure_buffer,
};

struct ion_heap *ion_cp_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_cp_heap *cp_heap;
	int ret;

	cp_heap = kzalloc(sizeof(*cp_heap), GFP_KERNEL);
	if (!cp_heap)
		return ERR_PTR(-ENOMEM);

	mutex_init(&cp_heap->lock);


	cp_heap->allocated_bytes = 0;
	cp_heap->umap_count = 0;
	cp_heap->kmap_cached_count = 0;
	cp_heap->kmap_uncached_count = 0;
	cp_heap->total_size = heap_data->size;
	cp_heap->heap.ops = &cp_heap_ops;
	cp_heap->heap.type = (enum ion_heap_type) ION_HEAP_TYPE_CP;
	cp_heap->heap_protected = HEAP_NOT_PROTECTED;
	cp_heap->secure_base = heap_data->base;
	cp_heap->secure_size = heap_data->size;
	cp_heap->has_outer_cache = heap_data->has_outer_cache;
	cp_heap->heap_size = heap_data->size;

	atomic_set(&cp_heap->protect_cnt, 0);
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
			cp_heap->heap_request_region =
				extra_data->request_region;
		if (extra_data->release_region)
			cp_heap->heap_release_region =
				extra_data->release_region;
		cp_heap->iommu_map_all =
				extra_data->iommu_map_all;
		cp_heap->iommu_2x_map_domain =
				extra_data->iommu_2x_map_domain;
		cp_heap->cma = extra_data->is_cma;
		cp_heap->allow_non_secure_allocation =
			extra_data->allow_nonsecure_alloc;

	}

	if (cp_heap->cma) {
		cp_heap->pool = NULL;
		cp_heap->cpu_addr = 0;
		cp_heap->heap.priv = heap_data->priv;
	} else {
		cp_heap->pool = gen_pool_create(12, -1);
		if (!cp_heap->pool)
			goto free_heap;

		cp_heap->base = heap_data->base;
		ret = gen_pool_add(cp_heap->pool, cp_heap->base,
					heap_data->size, -1);
		if (ret < 0)
			goto destroy_pool;

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

void ion_cp_heap_get_base(struct ion_heap *heap, unsigned long *base,
		unsigned long *size) \
{
	struct ion_cp_heap *cp_heap =
	     container_of(heap, struct  ion_cp_heap, heap);
	*base = cp_heap->base;
	*size = cp_heap->total_size;
}



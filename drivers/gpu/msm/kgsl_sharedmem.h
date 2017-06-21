/* Copyright (c) 2002,2007-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __KGSL_SHAREDMEM_H
#define __KGSL_SHAREDMEM_H

#include <linux/dma-mapping.h>

#include "kgsl_mmu.h"

struct kgsl_device;
struct kgsl_process_private;

#define KGSL_CACHE_OP_INV       0x01
#define KGSL_CACHE_OP_FLUSH     0x02
#define KGSL_CACHE_OP_CLEAN     0x03

int kgsl_sharedmem_alloc_contig(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc,
			uint64_t size);

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc);

int kgsl_sharedmem_readl(const struct kgsl_memdesc *memdesc,
			uint32_t *dst,
			uint64_t offsetbytes);

int kgsl_sharedmem_writel(struct kgsl_device *device,
			const struct kgsl_memdesc *memdesc,
			uint64_t offsetbytes,
			uint32_t src);

int kgsl_sharedmem_readq(const struct kgsl_memdesc *memdesc,
			uint64_t *dst,
			uint64_t offsetbytes);

int kgsl_sharedmem_writeq(struct kgsl_device *device,
			const struct kgsl_memdesc *memdesc,
			uint64_t offsetbytes,
			uint64_t src);

int kgsl_sharedmem_set(struct kgsl_device *device,
			const struct kgsl_memdesc *memdesc,
			uint64_t offsetbytes, unsigned int value,
			uint64_t sizebytes);

int kgsl_cache_range_op(struct kgsl_memdesc *memdesc,
			uint64_t offset, uint64_t size,
			unsigned int op);

void kgsl_process_init_sysfs(struct kgsl_device *device,
		struct kgsl_process_private *private);
void kgsl_process_uninit_sysfs(struct kgsl_process_private *private);

int kgsl_sharedmem_init_sysfs(void);
void kgsl_sharedmem_uninit_sysfs(void);

int kgsl_allocate_user(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc,
		uint64_t size, uint64_t flags);

void kgsl_get_memory_usage(char *str, size_t len, uint64_t memflags);

int kgsl_sharedmem_page_alloc_user(struct kgsl_memdesc *memdesc,
				uint64_t size);

#define MEMFLAGS(_flags, _mask, _shift) \
	((unsigned int) (((_flags) & (_mask)) >> (_shift)))

/*
 * kgsl_memdesc_get_align - Get alignment flags from a memdesc
 * @memdesc - the memdesc
 *
 * Returns the alignment requested, as power of 2 exponent.
 */
static inline int
kgsl_memdesc_get_align(const struct kgsl_memdesc *memdesc)
{
	return MEMFLAGS(memdesc->flags, KGSL_MEMALIGN_MASK,
		KGSL_MEMALIGN_SHIFT);
}

/*
 * kgsl_memdesc_get_pagesize - Get pagesize based on alignment
 * @memdesc - the memdesc
 *
 * Returns the pagesize based on memdesc alignment
 */
static inline int
kgsl_memdesc_get_pagesize(const struct kgsl_memdesc *memdesc)
{
	return (1 << kgsl_memdesc_get_align(memdesc));
}

/*
 * kgsl_memdesc_get_cachemode - Get cache mode of a memdesc
 * @memdesc: the memdesc
 *
 * Returns a KGSL_CACHEMODE* value.
 */
static inline int
kgsl_memdesc_get_cachemode(const struct kgsl_memdesc *memdesc)
{
	return MEMFLAGS(memdesc->flags, KGSL_CACHEMODE_MASK,
		KGSL_CACHEMODE_SHIFT);
}

static inline unsigned int
kgsl_memdesc_get_memtype(const struct kgsl_memdesc *memdesc)
{
	return MEMFLAGS(memdesc->flags, KGSL_MEMTYPE_MASK,
		KGSL_MEMTYPE_SHIFT);
}
/*
 * kgsl_memdesc_set_align - Set alignment flags of a memdesc
 * @memdesc - the memdesc
 * @align - alignment requested, as a power of 2 exponent.
 */
static inline int
kgsl_memdesc_set_align(struct kgsl_memdesc *memdesc, unsigned int align)
{
	if (align > 32)
		align = 32;

	memdesc->flags &= ~(uint64_t)KGSL_MEMALIGN_MASK;
	memdesc->flags |= (uint64_t)((align << KGSL_MEMALIGN_SHIFT) &
					KGSL_MEMALIGN_MASK);
	return 0;
}

/**
 * kgsl_memdesc_usermem_type - return buffer type
 * @memdesc - the memdesc
 *
 * Returns a KGSL_MEM_ENTRY_* value for this buffer, which
 * identifies if was allocated by us, or imported from
 * another allocator.
 */
static inline unsigned int
kgsl_memdesc_usermem_type(const struct kgsl_memdesc *memdesc)
{
	return MEMFLAGS(memdesc->flags, KGSL_MEMFLAGS_USERMEM_MASK,
		KGSL_MEMFLAGS_USERMEM_SHIFT);
}

/**
 * memdesg_sg_dma() - Turn a dma_addr (from CMA) into a sg table
 * @memdesc: Pointer to the memdesc structure
 * @addr: Physical address from the dma_alloc function
 * @size: Size of the chunk
 *
 * Create a sg table for the contigious chunk specified by addr and size.
 */
static inline int
memdesc_sg_dma(struct kgsl_memdesc *memdesc,
		phys_addr_t addr, uint64_t size)
{
	int ret;
	struct page *page = phys_to_page(addr);

	memdesc->sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (memdesc->sgt == NULL)
		return -ENOMEM;

	ret = sg_alloc_table(memdesc->sgt, 1, GFP_KERNEL);
	if (ret) {
		kfree(memdesc->sgt);
		memdesc->sgt = NULL;
		return ret;
	}

	sg_set_page(memdesc->sgt->sgl, page, (size_t) size, 0);
	return 0;
}

/*
 * kgsl_memdesc_is_global - is this a globally mapped buffer?
 * @memdesc: the memdesc
 *
 * Returns nonzero if this is a global mapping, 0 otherwise
 */
static inline int kgsl_memdesc_is_global(const struct kgsl_memdesc *memdesc)
{
	return (memdesc->priv & KGSL_MEMDESC_GLOBAL) != 0;
}

/*
 * kgsl_memdesc_is_secured - is this a secure buffer?
 * @memdesc: the memdesc
 *
 * Returns true if this is a secure mapping, false otherwise
 */
static inline bool kgsl_memdesc_is_secured(const struct kgsl_memdesc *memdesc)
{
	return memdesc && (memdesc->priv & KGSL_MEMDESC_SECURE);
}

/*
 * kgsl_memdesc_has_guard_page - is the last page a guard page?
 * @memdesc - the memdesc
 *
 * Returns nonzero if there is a guard page, 0 otherwise
 */
static inline int
kgsl_memdesc_has_guard_page(const struct kgsl_memdesc *memdesc)
{
	return (memdesc->priv & KGSL_MEMDESC_GUARD_PAGE) != 0;
}

/*
 * kgsl_memdesc_guard_page_size - returns guard page size
 * @memdesc - the memdesc
 *
 * Returns guard page size
 */
static inline uint64_t
kgsl_memdesc_guard_page_size(const struct kgsl_memdesc *memdesc)
{
	if (!kgsl_memdesc_has_guard_page(memdesc))
		return 0;

	if (kgsl_memdesc_is_secured(memdesc)) {
		if (memdesc->pagetable != NULL &&
				memdesc->pagetable->mmu != NULL)
			return memdesc->pagetable->mmu->secure_align_mask + 1;
	}

	return PAGE_SIZE;
}

/*
 * kgsl_memdesc_use_cpu_map - use the same virtual mapping on CPU and GPU?
 * @memdesc - the memdesc
 */
static inline int
kgsl_memdesc_use_cpu_map(const struct kgsl_memdesc *memdesc)
{
	return (memdesc->flags & KGSL_MEMFLAGS_USE_CPU_MAP) != 0;
}

/*
 * kgsl_memdesc_footprint - get the size of the mmap region
 * @memdesc - the memdesc
 *
 * The entire memdesc must be mapped. Additionally if the
 * CPU mapping is going to be mirrored, there must be room
 * for the guard page to be mapped so that the address spaces
 * match up.
 */
static inline uint64_t
kgsl_memdesc_footprint(const struct kgsl_memdesc *memdesc)
{
	return  memdesc->size + kgsl_memdesc_guard_page_size(memdesc);
}

/*
 * kgsl_allocate_global() - Allocate GPU accessible memory that will be global
 * across all processes
 * @device: The device pointer to which the memdesc belongs
 * @memdesc: Pointer to a KGSL memory descriptor for the memory allocation
 * @size: size of the allocation
 * @flags: Allocation flags that control how the memory is mapped
 * @priv: Priv flags that controls memory attributes
 *
 * Allocate contiguous memory for internal use and add the allocation to the
 * list of global pagetable entries that will be mapped at the same address in
 * all pagetables.  This is for use for device wide GPU allocations such as
 * ringbuffers.
 */
static inline int kgsl_allocate_global(struct kgsl_device *device,
	struct kgsl_memdesc *memdesc, uint64_t size, uint64_t flags,
	unsigned int priv, const char *name)
{
	int ret;

	memdesc->flags = flags;
	memdesc->priv = priv;

	if (((memdesc->priv & KGSL_MEMDESC_CONTIG) != 0) ||
		(kgsl_mmu_get_mmutype(device) == KGSL_MMU_TYPE_NONE))
		ret = kgsl_sharedmem_alloc_contig(device, memdesc,
						(size_t) size);
	else {
		ret = kgsl_sharedmem_page_alloc_user(memdesc, (size_t) size);
		if (ret == 0) {
			if (kgsl_memdesc_map(memdesc) == NULL) {
				kgsl_sharedmem_free(memdesc);
				ret = -ENOMEM;
			}
		}
	}

	if (ret == 0)
		kgsl_mmu_add_global(device, memdesc, name);

	return ret;
}

/**
 * kgsl_free_global() - Free a device wide GPU allocation and remove it from the
 * global pagetable entry list
 *
 * @device: Pointer to the device
 * @memdesc: Pointer to the GPU memory descriptor to free
 *
 * Remove the specific memory descriptor from the global pagetable entry list
 * and free it
 */
static inline void kgsl_free_global(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc)
{
	kgsl_mmu_remove_global(device, memdesc);
	kgsl_sharedmem_free(memdesc);
}

void kgsl_sharedmem_set_noretry(bool val);
bool kgsl_sharedmem_get_noretry(void);

/**
 * kgsl_alloc_sgt_from_pages() - Allocate a sg table
 *
 * @memdesc: memory descriptor of the allocation
 *
 * Allocate and return pointer to a sg table
 */
static inline struct sg_table *kgsl_alloc_sgt_from_pages(
				struct kgsl_memdesc *m)
{
	int ret;
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (sgt == NULL)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table_from_pages(sgt, m->pages, m->page_count, 0,
					m->size, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		return ERR_PTR(ret);
	}

	return sgt;
}

/**
 * kgsl_free_sgt() - Free a sg table structure
 *
 * @sgt: sg table pointer to be freed
 *
 * Free the sg table allocated using sgt and free the
 * sgt structure itself
 */
static inline void kgsl_free_sgt(struct sg_table *sgt)
{
	if (sgt != NULL) {
		sg_free_table(sgt);
		kfree(sgt);
	}
}

#include "kgsl_pool.h"

/**
 * kgsl_get_page_size() - Get supported pagesize
 * @size: Size of the page
 * @align: Desired alignment of the size
 *
 * Return supported pagesize
 */
#ifndef CONFIG_ALLOC_BUFFERS_IN_4K_CHUNKS
static inline int kgsl_get_page_size(size_t size, unsigned int align)
{
	if (align >= ilog2(SZ_1M) && size >= SZ_1M &&
		kgsl_pool_avaialable(SZ_1M))
		return SZ_1M;
	else if (align >= ilog2(SZ_64K) && size >= SZ_64K &&
		kgsl_pool_avaialable(SZ_64K))
		return SZ_64K;
	else if (align >= ilog2(SZ_8K) && size >= SZ_8K &&
		kgsl_pool_avaialable(SZ_8K))
		return SZ_8K;
	else
		return PAGE_SIZE;
}
#else
static inline int kgsl_get_page_size(size_t size, unsigned int align)
{
	return PAGE_SIZE;
}
#endif

#endif /* __KGSL_SHAREDMEM_H */

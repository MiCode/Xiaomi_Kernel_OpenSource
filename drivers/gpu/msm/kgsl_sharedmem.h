/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002, 2007-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_SHAREDMEM_H
#define __KGSL_SHAREDMEM_H

#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "kgsl.h"
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

void kgsl_memdesc_init(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, uint64_t flags);

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

void kgsl_free_secure_page(struct page *page);

struct page *kgsl_alloc_secure_page(void);

/**
 * kgsl_free_pages() - Free pages in the pages array
 * @memdesc: memdesc that has the array to be freed
 *
 * Free the pages in the pages array of memdesc. If pool
 * is configured, pages are added back to the pool.
 * If shmem is used for allocation, kgsl refcount on the page
 * is decremented.
 */
void kgsl_free_pages(struct kgsl_memdesc *memdesc);

/**
 * kgsl_free_pages_from_sgt() - Free scatter-gather list
 * @memdesc: pointer of the memdesc which has the sgt to be freed
 *
 * Free the sg list by collapsing any physical adjacent pages.
 * If pool is configured, pages are added back to the pool.
 * If shmem is used for allocation, kgsl refcount on the page
 * is decremented.
 */
void kgsl_free_pages_from_sgt(struct kgsl_memdesc *memdesc);

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
 * kgsl_memdesc_sg_dma - Turn a dma_addr (from CMA) into a sg table
 * @memdesc: Pointer to a memory descriptor
 * @addr: Physical address from the dma_alloc function
 * @size: Size of the chunk
 *
 * Create a sg table for the contiguous chunk specified by addr and size.
 *
 * Return: 0 on success or negative on failure.
 */
int kgsl_memdesc_sg_dma(struct kgsl_memdesc *memdesc,
		phys_addr_t addr, u64 size);

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

/**
 * kgsl_memdesc_is_reclaimed - check if a buffer is reclaimed
 * @memdesc: the memdesc
 *
 * Return: true if the memdesc pages were reclaimed, false otherwise
 */
static inline bool kgsl_memdesc_is_reclaimed(const struct kgsl_memdesc *memdesc)
{
	return memdesc && (memdesc->priv & KGSL_MEMDESC_RECLAIMED);
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
	return ALIGN(memdesc->size + kgsl_memdesc_guard_page_size(memdesc),
		PAGE_SIZE);
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
int kgsl_allocate_global(struct kgsl_device *device,
	struct kgsl_memdesc *memdesc, uint64_t size, uint64_t flags,
	unsigned int priv, const char *name);

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
void kgsl_free_global(struct kgsl_device *device, struct kgsl_memdesc *memdesc);

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
#if !defined(CONFIG_QCOM_KGSL_USE_SHMEM) && \
	!defined(CONFIG_ALLOC_BUFFERS_IN_4K_CHUNKS)
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

/**
 * kgsl_gfp_mask() - get gfp_mask to be used
 * @page_order: order of the page
 *
 * Get the gfp_mask to be used for page allocation
 * based on the order of the page
 *
 * Return appropriate gfp_mask
 */
unsigned int kgsl_gfp_mask(unsigned int page_order);

/**
 * kgsl_zero_page() - zero out a page
 * @p: pointer to the struct page
 * @order: order of the page
 *
 * Map a page into kernel and zero it out
 */
void kgsl_zero_page(struct page *page, unsigned int order);

/**
 * kgsl_flush_page - flush a page
 * @page: pointer to the struct page
 *
 * Map a page into kernel and flush it
 */
void kgsl_flush_page(struct page *page);

/**
 * struct kgsl_process_attribute - basic attribute for a process
 * @attr: Underlying struct attribute
 * @show: Attribute show function
 * @store: Attribute store function
 */
struct kgsl_process_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj,
			struct kgsl_process_attribute *attr, char *buf);
	ssize_t (*store)(struct kobject *kobj,
		struct kgsl_process_attribute *attr, const char *buf,
		ssize_t count);
};

#define PROCESS_ATTR(_name, _mode, _show, _store) \
	static struct kgsl_process_attribute attr_##_name = \
			__ATTR(_name, _mode, _show, _store)

#endif /* __KGSL_SHAREDMEM_H */

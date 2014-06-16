/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include "kgsl_mmu.h"
#include <linux/slab.h>
#include <linux/kmemleak.h>
#include <linux/iommu.h>

#include "kgsl_log.h"

struct kgsl_device;
struct kgsl_process_private;

#define KGSL_CACHE_OP_INV       0x01
#define KGSL_CACHE_OP_FLUSH     0x02
#define KGSL_CACHE_OP_CLEAN     0x03

int kgsl_sharedmem_page_alloc_user(struct kgsl_memdesc *memdesc,
				struct kgsl_pagetable *pagetable,
				size_t size);

int kgsl_cma_alloc_coherent(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable, size_t size);

int kgsl_cma_alloc_secure(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, size_t size);

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc);

int kgsl_sharedmem_readl(const struct kgsl_memdesc *memdesc,
			uint32_t *dst,
			unsigned int offsetbytes);

int kgsl_sharedmem_writel(struct kgsl_device *device,
			const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			uint32_t src);

int kgsl_sharedmem_set(struct kgsl_device *device,
			const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes, unsigned int value,
			unsigned int sizebytes);

int kgsl_cache_range_op(struct kgsl_memdesc *memdesc,
			size_t offset, size_t size,
			unsigned int op);

int kgsl_process_init_sysfs(struct kgsl_device *device,
		struct kgsl_process_private *private);
void kgsl_process_uninit_sysfs(struct kgsl_process_private *private);

int kgsl_sharedmem_init_sysfs(void);
void kgsl_sharedmem_uninit_sysfs(void);

/*
 * kgsl_memdesc_get_align - Get alignment flags from a memdesc
 * @memdesc - the memdesc
 *
 * Returns the alignment requested, as power of 2 exponent.
 */
static inline int
kgsl_memdesc_get_align(const struct kgsl_memdesc *memdesc)
{
	return (memdesc->flags & KGSL_MEMALIGN_MASK) >> KGSL_MEMALIGN_SHIFT;
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
	return (memdesc->flags & KGSL_CACHEMODE_MASK) >> KGSL_CACHEMODE_SHIFT;
}

/*
 * kgsl_memdesc_set_align - Set alignment flags of a memdesc
 * @memdesc - the memdesc
 * @align - alignment requested, as a power of 2 exponent.
 */
static inline int
kgsl_memdesc_set_align(struct kgsl_memdesc *memdesc, unsigned int align)
{
	if (align > 32) {
		KGSL_CORE_ERR("Alignment too big, restricting to 2^32\n");
		align = 32;
	}

	memdesc->flags &= ~KGSL_MEMALIGN_MASK;
	memdesc->flags |= (align << KGSL_MEMALIGN_SHIFT) & KGSL_MEMALIGN_MASK;
	return 0;
}

/*
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
	return (memdesc->flags & KGSL_MEMFLAGS_USERMEM_MASK)
		>> KGSL_MEMFLAGS_USERMEM_SHIFT;
}

static inline unsigned int kgsl_get_sg_pa(struct scatterlist *sg)
{
	/*
	 * Try sg_dma_address first to support ion carveout
	 * regions which do not work with sg_phys().
	 */
	unsigned int pa = sg_dma_address(sg);
	if (pa == 0)
		pa = sg_phys(sg);
	return pa;
}

static inline int
memdesc_sg_phys(struct kgsl_memdesc *memdesc,
		phys_addr_t physaddr, size_t size)
{
	memdesc->sg = kgsl_malloc(sizeof(struct scatterlist));
	if (memdesc->sg == NULL)
		return -ENOMEM;

	if (!is_vmalloc_addr(memdesc->sg))
		kmemleak_not_leak(memdesc->sg);

	memdesc->sglen = 1;
	sg_init_table(memdesc->sg, 1);
	memdesc->sg[0].length = size;
	memdesc->sg[0].offset = 0;
	memdesc->sg[0].dma_address = physaddr;
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
 * kgsl_memdesc_use_cpu_map - use the same virtual mapping on CPU and GPU?
 * @memdesc - the memdesc
 */
static inline int
kgsl_memdesc_use_cpu_map(const struct kgsl_memdesc *memdesc)
{
	return (memdesc->flags & KGSL_MEMFLAGS_USE_CPU_MAP) != 0;
}

/*
 * kgsl_memdesc_mmapsize - get the size of the mmap region
 * @memdesc - the memdesc
 *
 * The entire memdesc must be mapped. Additionally if the
 * CPU mapping is going to be mirrored, there must be room
 * for the guard page to be mapped so that the address spaces
 * match up.
 */
static inline size_t
kgsl_memdesc_mmapsize(const struct kgsl_memdesc *memdesc)
{
	size_t size = memdesc->size;
	if (kgsl_memdesc_has_guard_page(memdesc))
		size += SZ_4K;
	return size;
}

static inline int
kgsl_allocate_user(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable,
		size_t size, unsigned int flags)
{
	int ret;

	if (size == 0)
		return -EINVAL;

	memdesc->flags = flags;

	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE) {
		size = ALIGN(size, PAGE_SIZE);
		ret = kgsl_cma_alloc_coherent(device, memdesc, pagetable, size);
	} else if (flags & KGSL_MEMFLAGS_SECURE)
		ret = kgsl_cma_alloc_secure(device, memdesc, size);
	else
		ret = kgsl_sharedmem_page_alloc_user(memdesc, pagetable, size);

	return ret;
}

static inline int
kgsl_allocate_contiguous(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, size_t size)
{
	int ret;

	size = ALIGN(size, PAGE_SIZE);

	ret = kgsl_cma_alloc_coherent(device, memdesc, NULL, size);
	if (!ret && (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE))
		memdesc->gpuaddr = memdesc->physaddr;

	return ret;
}

/*
 * kgsl_allocate_global() - Allocate GPU accessible memory that will be global
 * across all processes
 * @device: The device pointer to which the memdesc belongs
 * @memdesc: Pointer to a KGSL memory descriptor for the memory allocation
 * @size: size of the allocation
 * @flags: Allocation flags that control how the memory is mapped
 *
 * Allocate contiguous memory for internal use and add the allocation to the
 * list of global pagetable entries that will be mapped at the same address in
 * all pagetables.  This is for use for device wide GPU allocations such as
 * ringbuffers.
 */
static inline int kgsl_allocate_global(struct kgsl_device *device,
	struct kgsl_memdesc *memdesc, size_t size, unsigned int flags)
{
	int ret;

	memdesc->flags = flags;

	ret = kgsl_allocate_contiguous(device, memdesc, size);

	if (!ret) {
		ret = kgsl_add_global_pt_entry(device, memdesc);
		if (ret)
			kgsl_sharedmem_free(memdesc);
	}

	return ret;
}

/**
 * kgsl_free_global() - Free a device wide GPU allocation and remove it from the
 * global pagetable entry list
 *
 * @memdesc: Pointer to the GPU memory descriptor to free
 *
 * Remove the specific memory descriptor from the global pagetable entry list
 * and free it
 */
static inline void kgsl_free_global(struct kgsl_memdesc *memdesc)
{
	kgsl_remove_global_pt_entry(memdesc);
	kgsl_sharedmem_free(memdesc);
}

#endif /* __KGSL_SHAREDMEM_H */

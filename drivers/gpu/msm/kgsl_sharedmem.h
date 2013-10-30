/* Copyright (c) 2002,2007-2013, The Linux Foundation. All rights reserved.
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
#include <linux/vmalloc.h>
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

extern struct kgsl_memdesc_ops kgsl_page_alloc_ops;

int kgsl_sharedmem_page_alloc(struct kgsl_memdesc *memdesc,
			   struct kgsl_pagetable *pagetable, size_t size);

int kgsl_sharedmem_page_alloc_user(struct kgsl_memdesc *memdesc,
				struct kgsl_pagetable *pagetable,
				size_t size);

int kgsl_sharedmem_alloc_coherent(struct kgsl_memdesc *memdesc, size_t size);

int kgsl_sharedmem_ebimem_user(struct kgsl_memdesc *memdesc,
			     struct kgsl_pagetable *pagetable,
			     size_t size);

int kgsl_sharedmem_ebimem(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			size_t size);

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

void kgsl_cache_range_op(struct kgsl_memdesc *memdesc, int op);

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

int
kgsl_sharedmem_map_vma(struct vm_area_struct *vma,
			const struct kgsl_memdesc *memdesc);

/*
 * For relatively small sglists, it is preferable to use kzalloc
 * rather than going down the vmalloc rat hole.  If the size of
 * the sglist is < PAGE_SIZE use kzalloc otherwise fallback to
 * vmalloc
 */

static inline void *kgsl_sg_alloc(unsigned int sglen)
{
	if (sglen >= ULONG_MAX / sizeof(struct scatterlist))
		return NULL;

	if ((sglen * sizeof(struct scatterlist)) <  PAGE_SIZE)
		return kzalloc(sglen * sizeof(struct scatterlist), GFP_KERNEL);
	else
		return vmalloc(sglen * sizeof(struct scatterlist));
}

static inline void kgsl_sg_free(void *ptr, unsigned int sglen)
{
	if ((sglen * sizeof(struct scatterlist)) < PAGE_SIZE)
		kfree(ptr);
	else
		vfree(ptr);
}

static inline int
memdesc_sg_phys(struct kgsl_memdesc *memdesc,
		phys_addr_t physaddr, unsigned int size)
{
	memdesc->sg = kgsl_sg_alloc(1);
	if (memdesc->sg == NULL)
		return -ENOMEM;

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
 * kgsl_memdesc_protflags - get mmu protection flags
 * @memdesc - the memdesc
 * Returns a mask of GSL_PT_PAGE* or IOMMU* values based
 * on the memdesc flags.
 */
static inline unsigned int
kgsl_memdesc_protflags(const struct kgsl_memdesc *memdesc)
{
	unsigned int protflags = 0;
	enum kgsl_mmutype mmutype = kgsl_mmu_get_mmutype();

	if (mmutype == KGSL_MMU_TYPE_GPU) {
		protflags = GSL_PT_PAGE_RV;
		if (!(memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY))
			protflags |= GSL_PT_PAGE_WV;
	} else if (mmutype == KGSL_MMU_TYPE_IOMMU) {
		protflags = IOMMU_READ;
		if (!(memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY))
			protflags |= IOMMU_WRITE;
	}
	return protflags;
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
static inline unsigned int
kgsl_memdesc_mmapsize(const struct kgsl_memdesc *memdesc)
{
	unsigned int size = memdesc->size;
	if (kgsl_memdesc_use_cpu_map(memdesc) &&
		kgsl_memdesc_has_guard_page(memdesc))
		size += SZ_4K;
	return size;
}

static inline int
kgsl_allocate(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable, size_t size)
{
	int ret;
	memdesc->priv |= (KGSL_MEMTYPE_KERNEL << KGSL_MEMTYPE_SHIFT);
	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE)
		return kgsl_sharedmem_ebimem(memdesc, pagetable, size);

	ret = kgsl_sharedmem_page_alloc(memdesc, pagetable, size);
	if (ret)
		return ret;
	ret = kgsl_mmu_get_gpuaddr(pagetable, memdesc);
	if (ret) {
		kgsl_sharedmem_free(memdesc);
		return ret;
	}
	ret = kgsl_mmu_map(pagetable, memdesc);
	if (ret)
		kgsl_sharedmem_free(memdesc);
	return ret;
}

static inline int
kgsl_allocate_user(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable,
		size_t size, unsigned int flags)
{
	int ret;

	if (size == 0)
		return -EINVAL;

	memdesc->flags = flags;

	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE)
		ret = kgsl_sharedmem_ebimem_user(memdesc, pagetable, size);
	else
		ret = kgsl_sharedmem_page_alloc_user(memdesc, pagetable, size);

	return ret;
}

static inline int
kgsl_allocate_contiguous(struct kgsl_memdesc *memdesc, size_t size)
{
	int ret  = kgsl_sharedmem_alloc_coherent(memdesc, size);
	if (!ret && (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE))
		memdesc->gpuaddr = memdesc->physaddr;

	memdesc->flags |= (KGSL_MEMTYPE_KERNEL << KGSL_MEMTYPE_SHIFT);
	return ret;
}

#endif /* __KGSL_SHAREDMEM_H */

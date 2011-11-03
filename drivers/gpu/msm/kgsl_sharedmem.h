/* Copyright (c) 2002,2007-2011, Code Aurora Forum. All rights reserved.
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

struct kgsl_device;
struct kgsl_process_private;

#define KGSL_CACHE_OP_INV       0x01
#define KGSL_CACHE_OP_FLUSH     0x02
#define KGSL_CACHE_OP_CLEAN     0x03

/** Set if the memdesc describes cached memory */
#define KGSL_MEMFLAGS_CACHED    0x00000001

struct kgsl_memdesc_ops {
	int (*vmflags)(struct kgsl_memdesc *);
	int (*vmfault)(struct kgsl_memdesc *, struct vm_area_struct *,
		       struct vm_fault *);
	void (*free)(struct kgsl_memdesc *memdesc);
};

extern struct kgsl_memdesc_ops kgsl_vmalloc_ops;

int kgsl_sharedmem_vmalloc(struct kgsl_memdesc *memdesc,
			   struct kgsl_pagetable *pagetable, size_t size);

int kgsl_sharedmem_vmalloc_user(struct kgsl_memdesc *memdesc,
				struct kgsl_pagetable *pagetable,
				size_t size, int flags);

int kgsl_sharedmem_alloc_coherent(struct kgsl_memdesc *memdesc, size_t size);

int kgsl_sharedmem_ebimem_user(struct kgsl_memdesc *memdesc,
			     struct kgsl_pagetable *pagetable,
			     size_t size, int flags);

int kgsl_sharedmem_ebimem(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			size_t size);

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc);

int kgsl_sharedmem_readl(const struct kgsl_memdesc *memdesc,
			uint32_t *dst,
			unsigned int offsetbytes);

int kgsl_sharedmem_writel(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			uint32_t src);

int kgsl_sharedmem_set(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes, unsigned int value,
			unsigned int sizebytes);

void kgsl_cache_range_op(struct kgsl_memdesc *memdesc, int op);

void kgsl_process_init_sysfs(struct kgsl_process_private *private);
void kgsl_process_uninit_sysfs(struct kgsl_process_private *private);

int kgsl_sharedmem_init_sysfs(void);
void kgsl_sharedmem_uninit_sysfs(void);

static inline int
memdesc_sg_phys(struct kgsl_memdesc *memdesc,
		unsigned int physaddr, unsigned int size)
{
	struct page *page = phys_to_page(physaddr);

	memdesc->sg = kmalloc(sizeof(struct scatterlist) * 1, GFP_KERNEL);
	if (memdesc->sg == NULL)
		return -ENOMEM;

	memdesc->sglen = 1;
	sg_init_table(memdesc->sg, 1);
	sg_set_page(&memdesc->sg[0], page, size, 0);
	return 0;
}

static inline int
kgsl_allocate(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable, size_t size)
{
	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE)
		return kgsl_sharedmem_ebimem(memdesc, pagetable, size);
	return kgsl_sharedmem_vmalloc(memdesc, pagetable, size);
}

static inline int
kgsl_allocate_user(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable,
		size_t size, unsigned int flags)
{
	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE)
		return kgsl_sharedmem_ebimem_user(memdesc, pagetable, size,
						  flags);
	return kgsl_sharedmem_vmalloc_user(memdesc, pagetable, size, flags);
}

static inline int
kgsl_allocate_contiguous(struct kgsl_memdesc *memdesc, size_t size)
{
	int ret  = kgsl_sharedmem_alloc_coherent(memdesc, size);
	if (!ret && (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE))
		memdesc->gpuaddr = memdesc->physaddr;
	return ret;
}

#endif /* __KGSL_SHAREDMEM_H */

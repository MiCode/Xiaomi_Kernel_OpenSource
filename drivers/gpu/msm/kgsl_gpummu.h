/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#ifndef __KGSL_GPUMMU_H
#define __KGSL_GPUMMU_H

#define GSL_PT_PAGE_BITS_MASK	0x00000007
#define GSL_PT_PAGE_ADDR_MASK	PAGE_MASK

#define GSL_MMU_INT_MASK \
	(MH_INTERRUPT_MASK__AXI_READ_ERROR | \
	 MH_INTERRUPT_MASK__AXI_WRITE_ERROR)

/* Macros to manage TLB flushing */
#define GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS     (sizeof(unsigned char) * 8)
#define GSL_TLBFLUSH_FILTER_GET(superpte)			     \
	      (*((unsigned char *)				    \
	      (((unsigned int)gpummu_pt->tlbflushfilter.base)    \
	      + (superpte / GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS))))
#define GSL_TLBFLUSH_FILTER_SETDIRTY(superpte)				\
	      (GSL_TLBFLUSH_FILTER_GET((superpte)) |= 1 <<	    \
	      (superpte % GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS))
#define GSL_TLBFLUSH_FILTER_ISDIRTY(superpte)			 \
	      (GSL_TLBFLUSH_FILTER_GET((superpte)) &		  \
	      (1 << (superpte % GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS)))
#define GSL_TLBFLUSH_FILTER_RESET() memset(gpummu_pt->tlbflushfilter.base,\
				      0, gpummu_pt->tlbflushfilter.size)

extern struct kgsl_mmu_ops gpummu_ops;
extern struct kgsl_mmu_pt_ops gpummu_pt_ops;

struct kgsl_tlbflushfilter {
	unsigned int *base;
	unsigned int size;
};

struct kgsl_gpummu_pt {
	struct kgsl_memdesc  base;
	unsigned int   last_superpte;
	/* Maintain filter to manage tlb flushing */
	struct kgsl_tlbflushfilter tlbflushfilter;
};

struct kgsl_ptpool_chunk {
	size_t size;
	unsigned int count;
	int dynamic;

	void *data;
	phys_addr_t phys;

	unsigned long *bitmap;
	struct list_head list;
};

struct kgsl_ptpool {
	size_t ptsize;
	struct mutex lock;
	struct list_head list;
	int entries;
	int static_entries;
	int chunks;
};

void *kgsl_gpummu_ptpool_init(int entries);
void kgsl_gpummu_ptpool_destroy(void *ptpool);

#endif /* __KGSL_GPUMMU_H */

/* arch/arm/mach-msm/include/mach/memory.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
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
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H
#include <linux/types.h>

/* physical offset of RAM */
#define PLAT_PHYS_OFFSET UL(CONFIG_PHYS_OFFSET)

#ifndef __ASSEMBLY__
void *allocate_contiguous_ebi(unsigned long, unsigned long, int);
phys_addr_t allocate_contiguous_ebi_nomap(unsigned long, unsigned long);
void clean_and_invalidate_caches(unsigned long, unsigned long, unsigned long);
void clean_caches(unsigned long, unsigned long, unsigned long);
void invalidate_caches(unsigned long, unsigned long, unsigned long);
int msm_get_memory_type_from_name(const char *memtype_name);

#ifdef CONFIG_CACHE_L2X0
extern void l2x0_cache_sync(void);
#define finish_arch_switch(prev)     do { l2x0_cache_sync(); } while (0)
#endif

#define MAX_HOLE_ADDRESS    (PHYS_OFFSET + 0x10000000)
extern phys_addr_t memory_hole_offset;
extern phys_addr_t memory_hole_start;
extern phys_addr_t memory_hole_end;
extern unsigned long memory_hole_align;
extern unsigned long virtual_hole_start;
extern unsigned long virtual_hole_end;
#ifdef CONFIG_DONT_MAP_HOLE_AFTER_MEMBANK0
void find_memory_hole(void);

#define MEM_HOLE_END_PHYS_OFFSET (memory_hole_end)
#define MEM_HOLE_PAGE_OFFSET (PAGE_OFFSET + memory_hole_offset + \
				memory_hole_align)

#define __phys_to_virt(phys)				\
	(unsigned long)\
	((MEM_HOLE_END_PHYS_OFFSET && ((phys) >= MEM_HOLE_END_PHYS_OFFSET)) ? \
	(phys) - MEM_HOLE_END_PHYS_OFFSET + MEM_HOLE_PAGE_OFFSET :	\
	(phys) - PHYS_OFFSET + PAGE_OFFSET)

#define __virt_to_phys(virt)				\
	(unsigned long)\
	((MEM_HOLE_END_PHYS_OFFSET && ((virt) >= MEM_HOLE_PAGE_OFFSET)) ? \
	(virt) - MEM_HOLE_PAGE_OFFSET + MEM_HOLE_END_PHYS_OFFSET :	\
	(virt) - PAGE_OFFSET + PHYS_OFFSET)
#endif

/*
 * Need a temporary unique variable that no one will ever see to
 * hold the compat string. Line number gives this easily.
 * Need another layer of indirection to get __LINE__ to expand
 * properly as opposed to appending and ending up with
 * __compat___LINE__
 */
#define __CONCAT(a, b)	___CONCAT(a, b)
#define ___CONCAT(a, b)	a ## b

#define EXPORT_COMPAT(com)	\
static char *__CONCAT(__compat_, __LINE__)  __used \
	__attribute((__section__(".exportcompat.init"))) = com

extern char *__compat_exports_start[];
extern char *__compat_exports_end[];

#endif

#if defined CONFIG_ARCH_MSM_SCORPION || defined CONFIG_ARCH_MSM_KRAIT
#define arch_has_speculative_dfetch()	1
#endif

#endif

#define CONSISTENT_DMA_SIZE	(SZ_1M * 14)

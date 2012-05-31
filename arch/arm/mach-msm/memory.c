/* arch/arm/mach-msm/memory.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/memory_alloc.h>
#include <linux/memblock.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/mach/map.h>
#include <asm/cacheflush.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <linux/hardirq.h>
#if defined(CONFIG_MSM_NPA_REMOTE)
#include "npa_remote.h"
#include <linux/completion.h>
#include <linux/err.h>
#endif
#include <linux/android_pmem.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <../../mm/mm.h>
#include <linux/fmem.h>

void *strongly_ordered_page;
char strongly_ordered_mem[PAGE_SIZE*2-4];

void map_page_strongly_ordered(void)
{
#if defined(CONFIG_ARCH_MSM7X27) && !defined(CONFIG_ARCH_MSM7X27A)
	long unsigned int phys;
	struct map_desc map;

	if (strongly_ordered_page)
		return;

	strongly_ordered_page = (void*)PFN_ALIGN((int)&strongly_ordered_mem);
	phys = __pa(strongly_ordered_page);

	map.pfn = __phys_to_pfn(phys);
	map.virtual = MSM_STRONGLY_ORDERED_PAGE;
	map.length = PAGE_SIZE;
	map.type = MT_DEVICE_STRONGLY_ORDERED;
	create_mapping(&map);

	printk(KERN_ALERT "Initialized strongly ordered page successfully\n");
#endif
}
EXPORT_SYMBOL(map_page_strongly_ordered);

void write_to_strongly_ordered_memory(void)
{
#if defined(CONFIG_ARCH_MSM7X27) && !defined(CONFIG_ARCH_MSM7X27A)
	if (!strongly_ordered_page) {
		if (!in_interrupt())
			map_page_strongly_ordered();
		else {
			printk(KERN_ALERT "Cannot map strongly ordered page in "
				"Interrupt Context\n");
			/* capture it here before the allocation fails later */
			BUG();
		}
	}
	*(int *)MSM_STRONGLY_ORDERED_PAGE = 0;
#endif
}
EXPORT_SYMBOL(write_to_strongly_ordered_memory);

/* These cache related routines make the assumption (if outer cache is
 * available) that the associated physical memory is contiguous.
 * They will operate on all (L1 and L2 if present) caches.
 */
void clean_and_invalidate_caches(unsigned long vstart,
	unsigned long length, unsigned long pstart)
{
	dmac_flush_range((void *)vstart, (void *) (vstart + length));
	outer_flush_range(pstart, pstart + length);
}

void clean_caches(unsigned long vstart,
	unsigned long length, unsigned long pstart)
{
	dmac_clean_range((void *)vstart, (void *) (vstart + length));
	outer_clean_range(pstart, pstart + length);
}

void invalidate_caches(unsigned long vstart,
	unsigned long length, unsigned long pstart)
{
	dmac_inv_range((void *)vstart, (void *) (vstart + length));
	outer_inv_range(pstart, pstart + length);
}

void * __init alloc_bootmem_aligned(unsigned long size, unsigned long alignment)
{
	void *unused_addr = NULL;
	unsigned long addr, tmp_size, unused_size;

	/* Allocate maximum size needed, see where it ends up.
	 * Then free it -- in this path there are no other allocators
	 * so we can depend on getting the same address back
	 * when we allocate a smaller piece that is aligned
	 * at the end (if necessary) and the piece we really want,
	 * then free the unused first piece.
	 */

	tmp_size = size + alignment - PAGE_SIZE;
	addr = (unsigned long)alloc_bootmem(tmp_size);
	free_bootmem(__pa(addr), tmp_size);

	unused_size = alignment - (addr % alignment);
	if (unused_size)
		unused_addr = alloc_bootmem(unused_size);

	addr = (unsigned long)alloc_bootmem(size);
	if (unused_size)
		free_bootmem(__pa(unused_addr), unused_size);

	return (void *)addr;
}

int (*change_memory_power)(u64, u64, int);

int platform_physical_remove_pages(u64 start, u64 size)
{
	if (!change_memory_power)
		return 0;
	return change_memory_power(start, size, MEMORY_DEEP_POWERDOWN);
}

int platform_physical_active_pages(u64 start, u64 size)
{
	if (!change_memory_power)
		return 0;
	return change_memory_power(start, size, MEMORY_ACTIVE);
}

int platform_physical_low_power_pages(u64 start, u64 size)
{
	if (!change_memory_power)
		return 0;
	return change_memory_power(start, size, MEMORY_SELF_REFRESH);
}

char *memtype_name[] = {
	"SMI_KERNEL",
	"SMI",
	"EBI0",
	"EBI1"
};

struct reserve_info *reserve_info;

static unsigned long stable_size(struct membank *mb,
	unsigned long unstable_limit)
{
	unsigned long upper_limit = mb->start + mb->size;

	if (!unstable_limit)
		return mb->size;

	/* Check for 32 bit roll-over */
	if (upper_limit >= mb->start) {
		/* If we didn't roll over we can safely make the check below */
		if (upper_limit <= unstable_limit)
			return mb->size;
	}

	if (mb->start >= unstable_limit)
		return 0;
	return unstable_limit - mb->start;
}

/* stable size of all memory banks contiguous to and below this one */
static unsigned long total_stable_size(unsigned long bank)
{
	int i;
	struct membank *mb = &meminfo.bank[bank];
	int memtype = reserve_info->paddr_to_memtype(mb->start);
	unsigned long size;

	size = stable_size(mb, reserve_info->low_unstable_address);
	for (i = bank - 1, mb = &meminfo.bank[bank - 1]; i >= 0; i--, mb--) {
		if (mb->start + mb->size != (mb + 1)->start)
			break;
		if (reserve_info->paddr_to_memtype(mb->start) != memtype)
			break;
		size += stable_size(mb, reserve_info->low_unstable_address);
	}
	return size;
}

static void __init calculate_reserve_limits(void)
{
	int i;
	struct membank *mb;
	int memtype;
	struct memtype_reserve *mt;
	unsigned long size;

	for (i = 0, mb = &meminfo.bank[0]; i < meminfo.nr_banks; i++, mb++)  {
		memtype = reserve_info->paddr_to_memtype(mb->start);
		if (memtype == MEMTYPE_NONE) {
			pr_warning("unknown memory type for bank at %lx\n",
				(long unsigned int)mb->start);
			continue;
		}
		mt = &reserve_info->memtype_reserve_table[memtype];
		size = total_stable_size(i);
		mt->limit = max(mt->limit, size);
	}
}

static void __init adjust_reserve_sizes(void)
{
	int i;
	struct memtype_reserve *mt;

	mt = &reserve_info->memtype_reserve_table[0];
	for (i = 0; i < MEMTYPE_MAX; i++, mt++) {
		if (mt->flags & MEMTYPE_FLAGS_1M_ALIGN)
			mt->size = (mt->size + SECTION_SIZE - 1) & SECTION_MASK;
		if (mt->size > mt->limit) {
			pr_warning("%lx size for %s too large, setting to %lx\n",
				mt->size, memtype_name[i], mt->limit);
			mt->size = mt->limit;
		}
	}
}

static void __init reserve_memory_for_mempools(void)
{
	int i, memtype, membank_type;
	struct memtype_reserve *mt;
	struct membank *mb;
	int ret;
	unsigned long size;

	mt = &reserve_info->memtype_reserve_table[0];
	for (memtype = 0; memtype < MEMTYPE_MAX; memtype++, mt++) {
		if (mt->flags & MEMTYPE_FLAGS_FIXED || !mt->size)
			continue;

		/* We know we will find memory bank(s) of the proper size
		 * as we have limited the size of the memory pool for
		 * each memory type to the largest total size of the memory
		 * banks which are contiguous and of the correct memory type.
		 * Choose the memory bank with the highest physical
		 * address which is large enough, so that we will not
		 * take memory from the lowest memory bank which the kernel
		 * is in (and cause boot problems) and so that we might
		 * be able to steal memory that would otherwise become
		 * highmem. However, do not use unstable memory.
		 */
		for (i = meminfo.nr_banks - 1; i >= 0; i--) {
			mb = &meminfo.bank[i];
			membank_type =
				reserve_info->paddr_to_memtype(mb->start);
			if (memtype != membank_type)
				continue;
			size = total_stable_size(i);
			if (size >= mt->size) {
				size = stable_size(mb,
					reserve_info->low_unstable_address);
				if (!size)
					continue;
				/* mt->size may be larger than size, all this
				 * means is that we are carving the memory pool
				 * out of multiple contiguous memory banks.
				 */
				mt->start = mb->start + (size - mt->size);
				ret = memblock_remove(mt->start, mt->size);
				BUG_ON(ret);
				break;
			}
		}
	}
}

static void __init initialize_mempools(void)
{
	struct mem_pool *mpool;
	int memtype;
	struct memtype_reserve *mt;

	mt = &reserve_info->memtype_reserve_table[0];
	for (memtype = 0; memtype < MEMTYPE_MAX; memtype++, mt++) {
		if (!mt->size)
			continue;
		mpool = initialize_memory_pool(mt->start, mt->size, memtype);
		if (!mpool)
			pr_warning("failed to create %s mempool\n",
				memtype_name[memtype]);
	}
}

#define  MAX_FIXED_AREA_SIZE 0x11000000

void __init msm_reserve(void)
{
	unsigned long msm_fixed_area_size;
	unsigned long msm_fixed_area_start;

	memory_pool_init();
	reserve_info->calculate_reserve_sizes();

	msm_fixed_area_size = reserve_info->fixed_area_size;
	msm_fixed_area_start = reserve_info->fixed_area_start;
	if (msm_fixed_area_size)
		if (msm_fixed_area_start > reserve_info->low_unstable_address
			- MAX_FIXED_AREA_SIZE)
			reserve_info->low_unstable_address =
			msm_fixed_area_start;

	calculate_reserve_limits();
	adjust_reserve_sizes();
	reserve_memory_for_mempools();
	initialize_mempools();
}

static int get_ebi_memtype(void)
{
	/* on 7x30 and 8x55 "EBI1 kernel PMEM" is really on EBI0 */
	if (cpu_is_msm7x30() || cpu_is_msm8x55())
		return MEMTYPE_EBI0;
	return MEMTYPE_EBI1;
}

void *allocate_contiguous_ebi(unsigned long size,
	unsigned long align, int cached)
{
	return allocate_contiguous_memory(size, get_ebi_memtype(),
		align, cached);
}
EXPORT_SYMBOL(allocate_contiguous_ebi);

unsigned long allocate_contiguous_ebi_nomap(unsigned long size,
	unsigned long align)
{
	return _allocate_contiguous_memory_nomap(size, get_ebi_memtype(),
		align, __builtin_return_address(0));
}
EXPORT_SYMBOL(allocate_contiguous_ebi_nomap);

/* emulation of the deprecated pmem_kalloc and pmem_kfree */
int32_t pmem_kalloc(const size_t size, const uint32_t flags)
{
	int pmem_memtype;
	int memtype = MEMTYPE_NONE;
	int ebi1_memtype = MEMTYPE_EBI1;
	unsigned int align;
	int32_t paddr;

	switch (flags & PMEM_ALIGNMENT_MASK) {
	case PMEM_ALIGNMENT_4K:
		align = SZ_4K;
		break;
	case PMEM_ALIGNMENT_1M:
		align = SZ_1M;
		break;
	default:
		pr_alert("Invalid alignment %x\n",
			(flags & PMEM_ALIGNMENT_MASK));
		return -EINVAL;
	}

	/* on 7x30 and 8x55 "EBI1 kernel PMEM" is really on EBI0 */
	if (cpu_is_msm7x30() || cpu_is_msm8x55())
			ebi1_memtype = MEMTYPE_EBI0;

	pmem_memtype = flags & PMEM_MEMTYPE_MASK;
	if (pmem_memtype == PMEM_MEMTYPE_EBI1)
		memtype = ebi1_memtype;
	else if (pmem_memtype == PMEM_MEMTYPE_SMI)
		memtype = MEMTYPE_SMI_KERNEL;
	else {
		pr_alert("Invalid memory type %x\n",
			flags & PMEM_MEMTYPE_MASK);
		return -EINVAL;
	}

	paddr = _allocate_contiguous_memory_nomap(size, memtype, align,
		__builtin_return_address(0));

	if (!paddr && pmem_memtype == PMEM_MEMTYPE_SMI)
		paddr = _allocate_contiguous_memory_nomap(size,
			ebi1_memtype, align, __builtin_return_address(0));

	if (!paddr)
		return -ENOMEM;
	return paddr;
}
EXPORT_SYMBOL(pmem_kalloc);

int pmem_kfree(const int32_t physaddr)
{
	free_contiguous_memory_by_paddr(physaddr);

	return 0;
}
EXPORT_SYMBOL(pmem_kfree);

unsigned int msm_ttbr0;

void store_ttbr0(void)
{
	/* Store TTBR0 for post-mortem debugging purposes. */
	asm("mrc p15, 0, %0, c2, c0, 0\n"
		: "=r" (msm_ttbr0));
}

int request_fmem_c_region(void *unused)
{
	return fmem_set_state(FMEM_C_STATE);
}

int release_fmem_c_region(void *unused)
{
	return fmem_set_state(FMEM_T_STATE);
}

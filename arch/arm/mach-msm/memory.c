/* arch/arm/mach-msm/memory.c
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

#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/memory_alloc.h>
#include <linux/memblock.h>
#include <asm/memblock.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/mach/map.h>
#include <asm/cacheflush.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/memory.h>
#include <linux/hardirq.h>
#if defined(CONFIG_MSM_NPA_REMOTE)
#include "npa_remote.h"
#include <linux/completion.h>
#include <linux/err.h>
#endif
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <linux/sched.h>
#include <linux/of_fdt.h>

/* fixme */
#include <asm/tlbflush.h>
#include <../../mm/mm.h>

#if defined(CONFIG_ARCH_MSM7X27)
static void *strongly_ordered_page;
static char strongly_ordered_mem[PAGE_SIZE*2-4];

void __init map_page_strongly_ordered(void)
{
	long unsigned int phys;
	struct map_desc map[1];

	if (strongly_ordered_page)
		return;

	strongly_ordered_page = (void*)PFN_ALIGN((int)&strongly_ordered_mem);
	phys = __pa(strongly_ordered_page);

	map[0].pfn = __phys_to_pfn(phys);
	map[0].virtual = MSM_STRONGLY_ORDERED_PAGE;
	map[0].length = PAGE_SIZE;
	map[0].type = MT_MEMORY_SO;
	iotable_init(map, ARRAY_SIZE(map));

	printk(KERN_ALERT "Initialized strongly ordered page successfully\n");
}
#else
void map_page_strongly_ordered(void) { }
#endif

#if defined(CONFIG_ARCH_MSM7X27)
void write_to_strongly_ordered_memory(void)
{
	*(int *)MSM_STRONGLY_ORDERED_PAGE = 0;
}
#else
void write_to_strongly_ordered_memory(void) { }
#endif
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

char *memtype_name[] = {
	"SMI_KERNEL",
	"SMI",
	"EBI0",
	"EBI1"
};

struct reserve_info *reserve_info;

/**
 * calculate_reserve_limits() - calculate reserve limits for all
 * memtypes
 *
 * for each memtype in the reserve_info->memtype_reserve_table, sets
 * the `limit' field to the largest size of any memblock of that
 * memtype.
 */
static void __init calculate_reserve_limits(void)
{
	struct memblock_region *mr;
	int memtype;
	struct memtype_reserve *mt;

	for_each_memblock(memory, mr) {
		memtype = reserve_info->paddr_to_memtype(mr->base);
		if (memtype == MEMTYPE_NONE) {
			pr_warning("unknown memory type for region at %lx\n",
				(long unsigned int)mr->base);
			continue;
		}
		mt = &reserve_info->memtype_reserve_table[memtype];
		mt->limit = max_t(unsigned long, mt->limit, mr->size);
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
			pr_warning("%pa size for %s too large, setting to %pa\n",
				&mt->size, memtype_name[i], &mt->limit);
			mt->size = mt->limit;
		}
	}
}

static void __init reserve_memory_for_mempools(void)
{
	int memtype;
	struct memtype_reserve *mt;
	phys_addr_t alignment;

	mt = &reserve_info->memtype_reserve_table[0];
	for (memtype = 0; memtype < MEMTYPE_MAX; memtype++, mt++) {
		if (mt->flags & MEMTYPE_FLAGS_FIXED || !mt->size)
			continue;
		alignment = (mt->flags & MEMTYPE_FLAGS_1M_ALIGN) ?
			SZ_1M : PAGE_SIZE;
		mt->start = arm_memblock_steal(mt->size, alignment);
		BUG_ON(!mt->start);
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
	if (reserve_info->calculate_reserve_sizes)
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

phys_addr_t allocate_contiguous_ebi_nomap(unsigned long size,
	unsigned long align)
{
	return _allocate_contiguous_memory_nomap(size, get_ebi_memtype(),
		align, __builtin_return_address(0));
}
EXPORT_SYMBOL(allocate_contiguous_ebi_nomap);

unsigned int msm_ttbr0;

void store_ttbr0(void)
{
	/* Store TTBR0 for post-mortem debugging purposes. */
	asm("mrc p15, 0, %0, c2, c0, 0\n"
		: "=r" (msm_ttbr0));
}

static char * const memtype_names[] = {
	[MEMTYPE_SMI_KERNEL] = "SMI_KERNEL",
	[MEMTYPE_SMI]	= "SMI",
	[MEMTYPE_EBI0] = "EBI0",
	[MEMTYPE_EBI1] = "EBI1",
};

int msm_get_memory_type_from_name(const char *memtype_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(memtype_names); i++) {
		if (memtype_names[i] &&
		    strcmp(memtype_name, memtype_names[i]) == 0)
			return i;
	}

	pr_err("Could not find memory type %s\n", memtype_name);
	return -EINVAL;
}

static int reserve_memory_type(const char *mem_name,
				struct memtype_reserve *reserve_table,
				int size)
{
	int ret = msm_get_memory_type_from_name(mem_name);

	if (ret >= 0) {
		reserve_table[ret].size += size;
		ret = 0;
	}
	return ret;
}

static int __init check_for_compat(unsigned long node)
{
	char **start = __compat_exports_start;

	for ( ; start < __compat_exports_end; start++)
		if (of_flat_dt_is_compatible(node, *start))
			return 1;

	return 0;
}

int __init dt_scan_for_memory_reserve(unsigned long node, const char *uname,
		int depth, void *data)
{
	char *memory_name_prop;
	unsigned int *memory_remove_prop;
	unsigned long memory_name_prop_length;
	unsigned long memory_remove_prop_length;
	unsigned long memory_size_prop_length;
	unsigned int *memory_size_prop;
	unsigned int *memory_reserve_prop;
	unsigned long memory_reserve_prop_length;
	unsigned int memory_size;
	unsigned int memory_start;
	unsigned int num_holes = 0;
	int i;
	int ret;

	memory_name_prop = of_get_flat_dt_prop(node,
						"qcom,memory-reservation-type",
						&memory_name_prop_length);
	memory_remove_prop = of_get_flat_dt_prop(node,
						"qcom,memblock-remove",
						&memory_remove_prop_length);

	memory_reserve_prop = of_get_flat_dt_prop(node,
						"qcom,memblock-reserve",
						&memory_reserve_prop_length);

	if (memory_name_prop || memory_remove_prop || memory_reserve_prop) {
		if (!check_for_compat(node))
			goto out;
	} else {
		goto out;
	}

	if (memory_name_prop) {
		if (strnlen(memory_name_prop, memory_name_prop_length) == 0) {
			WARN(1, "Memory name was malformed\n");
			goto mem_remove;
		}

		memory_size_prop = of_get_flat_dt_prop(node,
						"qcom,memory-reservation-size",
						&memory_size_prop_length);

		if (memory_size_prop &&
		    (memory_size_prop_length == sizeof(unsigned int))) {
			memory_size = be32_to_cpu(*memory_size_prop);

			if (reserve_memory_type(memory_name_prop,
						data, memory_size) == 0)
				pr_info("%s reserved %s size %x\n",
					uname, memory_name_prop, memory_size);
			else
				WARN(1, "Node %s reserve failed\n",
						uname);
		} else {
			WARN(1, "Node %s specified bad/nonexistent size\n",
					uname);
		}
	}

mem_remove:

	if (memory_remove_prop) {
		if (!memory_remove_prop_length || (memory_remove_prop_length %
				(2 * sizeof(unsigned int)) != 0)) {
			WARN(1, "Memory remove malformed\n");
			goto mem_reserve;
		}

		num_holes = memory_remove_prop_length /
					(2 * sizeof(unsigned int));

		for (i = 0; i < (num_holes * 2); i += 2) {
			memory_start = be32_to_cpu(memory_remove_prop[i]);
			memory_size = be32_to_cpu(memory_remove_prop[i+1]);

			ret = memblock_remove(memory_start, memory_size);
			if (ret)
				WARN(1, "Failed to remove memory %x-%x\n",
				memory_start, memory_start+memory_size);
			else
				pr_info("Node %s removed memory %x-%x\n", uname,
				memory_start, memory_start+memory_size);
		}
	}

mem_reserve:

	if (memory_reserve_prop) {
		if (memory_reserve_prop_length != (2*sizeof(unsigned int))) {
			WARN(1, "Memory reserve malformed\n");
			goto out;
		}

		memory_start = be32_to_cpu(memory_reserve_prop[0]);
		memory_size = be32_to_cpu(memory_reserve_prop[1]);

		ret = memblock_reserve(memory_start, memory_size);
		if (ret)
			WARN(1, "Failed to reserve memory %x-%x\n",
				memory_start, memory_start+memory_size);
		else
			pr_info("Node %s memblock_reserve memory %x-%x\n",
				uname, memory_start, memory_start+memory_size);
	}

out:
	return 0;
}

/* Function to remove any meminfo blocks which are of size zero */
static void merge_meminfo(void)
{
	int i = 0;

	while (i < meminfo.nr_banks) {
		struct membank *bank = &meminfo.bank[i];

		if (bank->size == 0) {
			memmove(bank, bank + 1,
			(meminfo.nr_banks - i) * sizeof(*bank));
			meminfo.nr_banks--;
			continue;
		}
		i++;
	}
}

/*
 * Function to scan the device tree and adjust the meminfo table to
 * reflect the memory holes.
 */
int __init dt_scan_for_memory_hole(unsigned long node, const char *uname,
		int depth, void *data)
{
	unsigned int *memory_remove_prop;
	unsigned long memory_remove_prop_length;
	unsigned long hole_start;
	unsigned long hole_size;
	unsigned int num_holes = 0;
	int i = 0;

	memory_remove_prop = of_get_flat_dt_prop(node,
						"qcom,memblock-remove",
						&memory_remove_prop_length);

	if (memory_remove_prop) {
		if (!check_for_compat(node))
			goto out;
	} else {
		goto out;
	}

	if (memory_remove_prop) {
		if (!memory_remove_prop_length || (memory_remove_prop_length %
			(2 * sizeof(unsigned int)) != 0)) {
			WARN(1, "Memory remove malformed\n");
			goto out;
		}

		num_holes = memory_remove_prop_length /
					(2 * sizeof(unsigned int));

		for (i = 0; i < (num_holes * 2); i += 2) {
			hole_start = be32_to_cpu(memory_remove_prop[i]);
			hole_size = be32_to_cpu(memory_remove_prop[i+1]);

			adjust_meminfo(hole_start, hole_size);
		}
	}

out:
	return 0;
}

/*
 * Split the memory bank to reflect the hole, if present,
 * using the start and end of the memory hole.
 */
void adjust_meminfo(unsigned long start, unsigned long size)
{
	int i;

	for (i = 0; i < meminfo.nr_banks; i++) {
		struct membank *bank = &meminfo.bank[i];

		if (((start + size) <= (bank->start + bank->size)) &&
			(start >= bank->start)) {
			memmove(bank + 1, bank,
				(meminfo.nr_banks - i) * sizeof(*bank));
			meminfo.nr_banks++;
			i++;

			bank->size = start - bank->start;
			bank[1].start = (start + size);
			bank[1].size -= (bank->size + size);
			bank[1].highmem = 0;
			merge_meminfo();
		}
	}
}

unsigned long get_ddr_size(void)
{
	unsigned int i;
	unsigned long ret = 0;

	for (i = 0; i < meminfo.nr_banks; i++)
		ret += meminfo.bank[i].size;

	return ret;
}

/* Provide a string that anonymous device tree allocations (those not
 * directly associated with any driver) can use for their "compatible"
 * field */
EXPORT_COMPAT("qcom,msm-contig-mem");

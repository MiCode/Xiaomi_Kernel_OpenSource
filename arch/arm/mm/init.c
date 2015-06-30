/*
 *  linux/arch/arm/mm/init.c
 *
 *  Copyright (C) 1995-2005 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/export.h>
#include <linux/nodemask.h>
#include <linux/initrd.h>
#include <linux/of_fdt.h>
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/sort.h>
#include <linux/dma-contiguous.h>

#include <asm/mach-types.h>
#include <asm/memblock.h>
#include <asm/prom.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/sizes.h>
#include <asm/tlb.h>
#include <asm/fixmap.h>
#include <asm/cputype.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "mm.h"

static unsigned long phys_initrd_start __initdata = 0;
static unsigned long phys_initrd_size __initdata = 0;
int msm_krait_need_wfe_fixup;
EXPORT_SYMBOL(msm_krait_need_wfe_fixup);

static int __init early_initrd(char *p)
{
	unsigned long start, size;
	char *endp;

	start = memparse(p, &endp);
	if (*endp == ',') {
		size = memparse(endp + 1, NULL);

		phys_initrd_start = start;
		phys_initrd_size = size;
	}
	return 0;
}
early_param("initrd", early_initrd);

static int __init parse_tag_initrd(const struct tag *tag)
{
	printk(KERN_WARNING "ATAG_INITRD is deprecated; "
		"please update your bootloader.\n");
	phys_initrd_start = __virt_to_phys(tag->u.initrd.start);
	phys_initrd_size = tag->u.initrd.size;
	return 0;
}

__tagtable(ATAG_INITRD, parse_tag_initrd);

static int __init parse_tag_initrd2(const struct tag *tag)
{
	phys_initrd_start = tag->u.initrd.start;
	phys_initrd_size = tag->u.initrd.size;
	return 0;
}

__tagtable(ATAG_INITRD2, parse_tag_initrd2);

#ifdef CONFIG_OF_FLATTREE
void __init early_init_dt_setup_initrd_arch(unsigned long start, unsigned long end)
{
	phys_initrd_start = start;
	phys_initrd_size = end - start;
}
#endif /* CONFIG_OF_FLATTREE */

/*
 * This keeps memory configuration data used by a couple memory
 * initialization functions, as well as show_mem() for the skipping
 * of holes in the memory map.  It is populated by arm_add_memory().
 */
struct meminfo meminfo;

void show_mem(unsigned int filter)
{
	int free = 0, total = 0, reserved = 0;
	int shared = 0, cached = 0, slab = 0, i;
	struct meminfo * mi = &meminfo;

	printk("Mem-info:\n");
	show_free_areas(filter);

	for_each_bank (i, mi) {
		struct membank *bank = &mi->bank[i];
		unsigned int pfn1, pfn2;
		struct page *page, *end;

		pfn1 = bank_pfn_start(bank);
		pfn2 = bank_pfn_end(bank);

		page = pfn_to_page(pfn1);
		end  = pfn_to_page(pfn2 - 1) + 1;

		do {
			total++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (PageSlab(page))
				slab++;
			else if (!page_count(page))
				free++;
			else
				shared += page_count(page) - 1;
			page++;
#ifdef CONFIG_SPARSEMEM
			pfn1++;
			if (!(pfn1 % PAGES_PER_SECTION))
				page = pfn_to_page(pfn1);
		} while (pfn1 < pfn2);
#else
		} while (page < end);
#endif
	}

	printk("%d pages of RAM\n", total);
	printk("%d free pages\n", free);
	printk("%d reserved pages\n", reserved);
	printk("%d slab pages\n", slab);
	printk("%d pages shared\n", shared);
	printk("%d pages swap cached\n", cached);
}

static void __init find_limits(unsigned long *min, unsigned long *max_low,
			       unsigned long *max_high)
{
	struct meminfo *mi = &meminfo;
	int i;

	/* This assumes the meminfo array is properly sorted */
	*min = bank_pfn_start(&mi->bank[0]);
	for_each_bank (i, mi)
		if (mi->bank[i].highmem)
				break;
	*max_low = bank_pfn_end(&mi->bank[i - 1]);
	*max_high = bank_pfn_end(&mi->bank[mi->nr_banks - 1]);
}

static void __init arm_bootmem_init(unsigned long start_pfn,
	unsigned long end_pfn)
{
	struct memblock_region *reg;
	unsigned int boot_pages;
	phys_addr_t bitmap;
	pg_data_t *pgdat;

	/*
	 * Allocate the bootmem bitmap page.  This must be in a region
	 * of memory which has already been mapped.
	 */
	boot_pages = bootmem_bootmap_pages(end_pfn - start_pfn);
	bitmap = memblock_alloc_base(boot_pages << PAGE_SHIFT, L1_CACHE_BYTES,
				__pfn_to_phys(end_pfn));

	/*
	 * Initialise the bootmem allocator, handing the
	 * memory banks over to bootmem.
	 */
	node_set_online(0);
	pgdat = NODE_DATA(0);
	init_bootmem_node(pgdat, __phys_to_pfn(bitmap), start_pfn, end_pfn);

	/* Free the lowmem regions from memblock into bootmem. */
	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (end >= end_pfn)
			end = end_pfn;
		if (start >= end)
			break;

		free_bootmem(__pfn_to_phys(start), (end - start) << PAGE_SHIFT);
	}

	/* Reserve the lowmem memblock reserved regions in bootmem. */
	for_each_memblock(reserved, reg) {
		unsigned long start = memblock_region_reserved_base_pfn(reg);
		unsigned long end = memblock_region_reserved_end_pfn(reg);

		if (end >= end_pfn)
			end = end_pfn;
		if (start >= end)
			break;

		reserve_bootmem(__pfn_to_phys(start),
			        (end - start) << PAGE_SHIFT, BOOTMEM_DEFAULT);
	}
}

#ifdef CONFIG_ZONE_DMA

unsigned long arm_dma_zone_size __read_mostly;
EXPORT_SYMBOL(arm_dma_zone_size);

/*
 * The DMA mask corresponding to the maximum bus address allocatable
 * using GFP_DMA.  The default here places no restriction on DMA
 * allocations.  This must be the smallest DMA mask in the system,
 * so a successful GFP_DMA allocation will always satisfy this.
 */
phys_addr_t arm_dma_limit;

static void __init arm_adjust_dma_zone(unsigned long *size, unsigned long *hole,
	unsigned long dma_size)
{
	if (size[0] <= dma_size)
		return;

	size[ZONE_NORMAL] = size[0] - dma_size;
	size[ZONE_DMA] = dma_size;
	hole[ZONE_NORMAL] = hole[0];
	hole[ZONE_DMA] = 0;
}
#endif

void __init setup_dma_zone(struct machine_desc *mdesc)
{
#ifdef CONFIG_ZONE_DMA
	if (mdesc->dma_zone_size) {
		arm_dma_zone_size = mdesc->dma_zone_size;
		arm_dma_limit = PHYS_OFFSET + arm_dma_zone_size - 1;
	} else
		arm_dma_limit = 0xffffffff;
#endif
}

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
static void __init arm_bootmem_free_hmnm(unsigned long max_low,
	unsigned long max_high)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES];
	struct memblock_region *reg;

	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));

	max_zone_pfns[0] = max_low;
#ifdef CONFIG_HIGHMEM
	max_zone_pfns[ZONE_HIGHMEM] = max_high;
#endif
	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		memblock_set_node(PFN_PHYS(start), PFN_PHYS(end - start), 0);
	}
	free_area_init_nodes(max_zone_pfns);
}

#else
static void __init arm_bootmem_free(unsigned long min, unsigned long max_low,
	unsigned long max_high)
{
	unsigned long zone_size[MAX_NR_ZONES], zhole_size[MAX_NR_ZONES];
	struct memblock_region *reg;

	/*
	 * initialise the zones.
	 */
	memset(zone_size, 0, sizeof(zone_size));

	/*
	 * The memory size has already been determined.  If we need
	 * to do anything fancy with the allocation of this memory
	 * to the zones, now is the time to do it.
	 */
	zone_size[0] = max_low - min;
#ifdef CONFIG_HIGHMEM
	zone_size[ZONE_HIGHMEM] = max_high - max_low;
#endif

	/*
	 * Calculate the size of the holes.
	 *  holes = node_size - sum(bank_sizes)
	 */
	memcpy(zhole_size, zone_size, sizeof(zhole_size));
	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (start < max_low) {
			unsigned long low_end = min(end, max_low);
			zhole_size[0] -= low_end - start;
		}
#ifdef CONFIG_HIGHMEM
		if (end > max_low) {
			unsigned long high_start = max(start, max_low);
			zhole_size[ZONE_HIGHMEM] -= end - high_start;
		}
#endif
	}

#ifdef CONFIG_ZONE_DMA
	/*
	 * Adjust the sizes according to any special requirements for
	 * this machine type.
	 */
	if (arm_dma_zone_size)
		arm_adjust_dma_zone(zone_size, zhole_size,
			arm_dma_zone_size >> PAGE_SHIFT);
#endif

	free_area_init_node(0, zone_size, min, zhole_size);
}
#endif

#ifdef CONFIG_HAVE_ARCH_PFN_VALID
int pfn_valid(unsigned long pfn)
{
	return memblock_is_memory(__pfn_to_phys(pfn));
}
EXPORT_SYMBOL(pfn_valid);
#endif

#ifndef CONFIG_SPARSEMEM
static void __init arm_memory_present(void)
{
}
#else
static void __init arm_memory_present(void)
{
	struct meminfo *mi = &meminfo;
	int i;
	for_each_bank(i, mi) {
		memory_present(0, bank_pfn_start(&mi->bank[i]),
				bank_pfn_end(&mi->bank[i]));
	}
}
#endif

static bool arm_memblock_steal_permitted = true;

phys_addr_t __init arm_memblock_steal(phys_addr_t size, phys_addr_t align)
{
	phys_addr_t phys;

	BUG_ON(!arm_memblock_steal_permitted);

	phys = memblock_alloc_base(size, align, MEMBLOCK_ALLOC_ANYWHERE);
	memblock_free(phys, size);
	memblock_remove(phys, size);

	return phys;
}

static int __init meminfo_cmp(const void *_a, const void *_b)
{
	const struct membank *a = _a, *b = _b;
	long cmp = bank_pfn_start(a) - bank_pfn_start(b);
	return cmp < 0 ? -1 : cmp > 0 ? 1 : 0;
}

phys_addr_t memory_hole_offset;
EXPORT_SYMBOL(memory_hole_offset);
phys_addr_t memory_hole_start;
EXPORT_SYMBOL(memory_hole_start);
phys_addr_t memory_hole_end;
EXPORT_SYMBOL(memory_hole_end);
unsigned long memory_hole_align;
EXPORT_SYMBOL(memory_hole_align);
unsigned long virtual_hole_start;
unsigned long virtual_hole_end;

#ifdef CONFIG_DONT_MAP_HOLE_AFTER_MEMBANK0
void find_memory_hole(void)
{
	int i;
	phys_addr_t hole_start;
	phys_addr_t hole_size;
	unsigned long hole_end_virt;

	/*
	 * Find the start and end of the hole, using meminfo.
	 */
	for (i = 0; i < (meminfo.nr_banks - 1); i++) {
		if ((meminfo.bank[i].start + meminfo.bank[i].size) !=
						meminfo.bank[i+1].start) {
			if (meminfo.bank[i].start + meminfo.bank[i].size
							<= MAX_HOLE_ADDRESS) {

				hole_start = meminfo.bank[i].start +
							meminfo.bank[i].size;
				hole_size = meminfo.bank[i+1].start -
								hole_start;

				if (memory_hole_start == 0 &&
							memory_hole_end == 0) {
					memory_hole_start = hole_start;
					memory_hole_end = hole_start +
								hole_size;
				} else if ((memory_hole_end -
					memory_hole_start) <= hole_size) {
					memory_hole_start = hole_start;
					memory_hole_end = hole_start +
								hole_size;
				}
			}
		}
	}

	memory_hole_offset = memory_hole_start - PHYS_OFFSET;
	if (!IS_ALIGNED(memory_hole_start, SECTION_SIZE)) {
		pr_err("memory_hole_start %pa is not aligned to %lx\n",
			&memory_hole_start, SECTION_SIZE);
		BUG();
	}
	if (!IS_ALIGNED(memory_hole_end, SECTION_SIZE)) {
		pr_err("memory_hole_end %pa is not aligned to %lx\n",
			&memory_hole_end, SECTION_SIZE);
		BUG();
	}

	hole_end_virt = __phys_to_virt(memory_hole_end);

	if ((!IS_ALIGNED(hole_end_virt, PMD_SIZE) &&
	     IS_ALIGNED(memory_hole_end, PMD_SIZE)) ||
	     (IS_ALIGNED(hole_end_virt, PMD_SIZE) &&
	      !IS_ALIGNED(memory_hole_end, PMD_SIZE))) {
		memory_hole_align = !IS_ALIGNED(hole_end_virt, PMD_SIZE) ?
					hole_end_virt & ~PMD_MASK :
					memory_hole_end & ~PMD_MASK;
		virtual_hole_start = hole_end_virt;
		virtual_hole_end = hole_end_virt + memory_hole_align;
		pr_info("Physical memory hole is not aligned. There will be a virtual memory hole from %lx to %lx\n",
			virtual_hole_start, virtual_hole_end);
	}
}

#endif

void __init arm_memblock_init(struct meminfo *mi, struct machine_desc *mdesc)
{
	int i;

	sort(&meminfo.bank, meminfo.nr_banks, sizeof(meminfo.bank[0]), meminfo_cmp, NULL);

	for (i = 0; i < mi->nr_banks; i++)
		memblock_add(mi->bank[i].start, mi->bank[i].size);

	/* Register the kernel text, kernel data and initrd with memblock. */
#ifdef CONFIG_XIP_KERNEL
	memblock_reserve(__pa(_sdata), _end - _sdata);
#else
	memblock_reserve(__pa(_stext), _end - _stext);
#endif
#ifdef CONFIG_BLK_DEV_INITRD
	if (phys_initrd_size &&
	    !memblock_is_region_memory(phys_initrd_start, phys_initrd_size)) {
		pr_err("INITRD: 0x%08lx+0x%08lx is not a memory region - disabling initrd\n",
		       phys_initrd_start, phys_initrd_size);
		phys_initrd_start = phys_initrd_size = 0;
	}
	if (phys_initrd_size &&
	    memblock_is_region_reserved(phys_initrd_start, phys_initrd_size)) {
		pr_err("INITRD: 0x%08lx+0x%08lx overlaps in-use memory region - disabling initrd\n",
		       phys_initrd_start, phys_initrd_size);
		phys_initrd_start = phys_initrd_size = 0;
	}
	if (phys_initrd_size) {
		memblock_reserve(phys_initrd_start, phys_initrd_size);

		/* Now convert initrd to virtual addresses */
		initrd_start = __phys_to_virt(phys_initrd_start);
		initrd_end = initrd_start + phys_initrd_size;
	}
#endif

	arm_mm_memblock_reserve();
	arm_dt_memblock_reserve();

	/* reserve any platform specific memblock areas */
	if (mdesc->reserve)
		mdesc->reserve();

	/*
	 * reserve memory for DMA contigouos allocations,
	 * must come from DMA area inside low memory
	 */
	dma_contiguous_reserve(min(arm_dma_limit, arm_lowmem_limit));

	arm_memblock_steal_permitted = false;
	memblock_allow_resize();
	memblock_dump_all();
}

#ifdef CONFIG_MEMORY_HOTPLUG_SPARSE
int _early_pfn_valid(unsigned long pfn)
{
	struct meminfo *mi = &meminfo;
	unsigned int left = 0, right = mi->nr_banks;

	do {
		unsigned int mid = (right + left) / 2;
		struct membank *bank = &mi->bank[mid];

		if (pfn < bank_pfn_start(bank))
			right = mid;
		else if (pfn >= bank_pfn_end(bank))
			left = mid + 1;
		else
			return 1;
	} while (left < right);
	return 0;
}
EXPORT_SYMBOL(_early_pfn_valid);
#endif

void __init bootmem_init(void)
{
	unsigned long min, max_low, max_high;

	max_low = max_high = 0;

	find_limits(&min, &max_low, &max_high);

	arm_bootmem_init(min, max_low);

	/*
	 * Sparsemem tries to allocate bootmem in memory_present(),
	 * so must be done after the fixed reservations
	 */
	arm_memory_present();

	/*
	 * sparse_init() needs the bootmem allocator up and running.
	 */
	sparse_init();

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
	arm_bootmem_free_hmnm(max_low, max_high);
#else
	/*
	 * Now free the memory - free_area_init_node needs
	 * the sparse mem_map arrays initialized by sparse_init()
	 * for memmap_init_zone(), otherwise all PFNs are invalid.
	 */
	arm_bootmem_free(min, max_low, max_high);
#endif

	/*
	 * This doesn't seem to be used by the Linux memory manager any
	 * more, but is used by ll_rw_block.  If we can get rid of it, we
	 * also get rid of some of the stuff above as well.
	 *
	 * Note: max_low_pfn and max_pfn reflect the number of _pages_ in
	 * the system, not the maximum PFN.
	 */
	max_low_pfn = max_low - PHYS_PFN_OFFSET;
	max_pfn = max_high - PHYS_PFN_OFFSET;
}

static inline int free_area(unsigned long pfn, unsigned long end, char *s)
{
	unsigned int pages = 0, size = (end - pfn) << (PAGE_SHIFT - 10);

	for (; pfn < end; pfn++) {
		struct page *page = pfn_to_page(pfn);
		ClearPageReserved(page);
		init_page_count(page);
		__free_page(page);
		pages++;
	}

	if (size && s)
		printk(KERN_INFO "Freeing %s memory: %dK\n", s, size);

	return pages;
}

/*
 * Poison init memory with an undefined instruction (ARM) or a branch to an
 * undefined instruction (Thumb).
 */
static inline void poison_init_mem(void *s, size_t count)
{
	u32 *p = (u32 *)s;
	for (; count != 0; count -= 4)
		*p++ = 0xe7fddef0;
}

static inline void
free_memmap(unsigned long start_pfn, unsigned long end_pfn)
{
	struct page *start_pg, *end_pg;
	unsigned long pg, pgend;

	/*
	 * Convert start_pfn/end_pfn to a struct page pointer.
	 */
	start_pg = pfn_to_page(start_pfn - 1) + 1;
	end_pg = pfn_to_page(end_pfn - 1) + 1;

	/*
	 * Convert to physical addresses, and
	 * round start upwards and end downwards.
	 */
	pg = (unsigned long)PAGE_ALIGN(__pa(start_pg));
	pgend = (unsigned long)__pa(end_pg) & PAGE_MASK;

	/*
	 * If there are free pages between these,
	 * free the section of the memmap array.
	 */
	if (pg < pgend)
		free_bootmem(pg, pgend - pg);
}

/*
 * The mem_map array can get very big.  Free as much of the unused portion of
 * the mem_map that we are allowed to. The page migration code moves pages
 * in blocks that are rounded per the MAX_ORDER_NR_PAGES definition, so we
 * can't free mem_map entries that may be dereferenced in this manner.
 */
static void __init free_unused_memmap(struct meminfo *mi)
{
	unsigned long bank_start, prev_bank_end = 0;
	unsigned int i;

	/*
	 * This relies on each bank being in address order.
	 * The banks are sorted previously in bootmem_init().
	 */
	for_each_bank(i, mi) {
		struct membank *bank = &mi->bank[i];

		bank_start = round_down(bank_pfn_start(bank),
					MAX_ORDER_NR_PAGES);

#ifdef CONFIG_SPARSEMEM
		/*
		 * Take care not to free memmap entries that don't exist
		 * due to SPARSEMEM sections which aren't present.
		 */
		bank_start = min(bank_start,
				 ALIGN(prev_bank_end, PAGES_PER_SECTION));
#else
		/*
		 * Align down here since the VM subsystem insists that the
		 * memmap entries are valid from the bank start aligned to
		 * MAX_ORDER_NR_PAGES.
		 */
		bank_start = round_down(bank_start, MAX_ORDER_NR_PAGES);
#endif
		/*
		 * If we had a previous bank, and there is a space
		 * between the current bank and the previous, free it.
		 */
		if (prev_bank_end && prev_bank_end < bank_start)
			free_memmap(prev_bank_end, bank_start);

		prev_bank_end = round_up(bank_pfn_end(bank),
					 MAX_ORDER_NR_PAGES);
	}

#ifdef CONFIG_SPARSEMEM
	if (!IS_ALIGNED(prev_bank_end, PAGES_PER_SECTION))
		free_memmap(prev_bank_end,
			    ALIGN(prev_bank_end, PAGES_PER_SECTION));
#endif
}

static void __init free_highpages(void)
{
#ifdef CONFIG_HIGHMEM
	unsigned long max_low = max_low_pfn + PHYS_PFN_OFFSET;
	struct memblock_region *mem, *res;

	/* set highmem page free */
	for_each_memblock(memory, mem) {
		unsigned long start = memblock_region_memory_base_pfn(mem);
		unsigned long end = memblock_region_memory_end_pfn(mem);

		/* Ignore complete lowmem entries */
		if (end <= max_low)
			continue;

		/* Truncate partial highmem entries */
		if (start < max_low)
			start = max_low;

		/* Find and exclude any reserved regions */
		for_each_memblock(reserved, res) {
			unsigned long res_start, res_end;

			res_start = memblock_region_reserved_base_pfn(res);
			res_end = memblock_region_reserved_end_pfn(res);

			if (res_end < start)
				continue;
			if (res_start < start)
				res_start = start;
			if (res_start > end)
				res_start = end;
			if (res_end > end)
				res_end = end;
			if (res_start != start)
				totalhigh_pages += free_area(start, res_start,
							     NULL);
			start = res_end;
			if (start == end)
				break;
		}

		/* And now free anything which remains */
		if (start < end)
			totalhigh_pages += free_area(start, end, NULL);
	}
	totalram_pages += totalhigh_pages;
#endif
}

#define MLK(b, t) b, t, ((t) - (b)) >> 10
#define MLM(b, t) b, t, ((t) - (b)) >> 20
#define MLK_ROUNDUP(b, t) b, t, DIV_ROUND_UP(((t) - (b)), SZ_1K)

#ifdef CONFIG_ENABLE_VMALLOC_SAVING
static void print_vmalloc_lowmem_info(void)
{
	int i;
	void *va_start, *va_end;

	printk(KERN_NOTICE
		"	   vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n",
		MLM(VMALLOC_START, VMALLOC_END));

	for (i = meminfo.nr_banks - 1; i >= 0; i--) {
		if (!meminfo.bank[i].highmem) {
			va_start = __va(meminfo.bank[i].start);
			va_end = __va(meminfo.bank[i].start +
						meminfo.bank[i].size);
			printk(KERN_NOTICE
			 "	    lowmem : 0x%08lx - 0x%08lx   (%4ld MB)\n",
			MLM((unsigned long)va_start, (unsigned long)va_end));
		}
		if (i && ((meminfo.bank[i-1].start + meminfo.bank[i-1].size) !=
			   meminfo.bank[i].start)) {
			phys_addr_t end_phys;

			if((meminfo.bank[i-1].start + meminfo.bank[i-1].size) > arm_lowmem_limit)
				continue;

			if(meminfo.bank[i].start > arm_lowmem_limit)
				end_phys = arm_lowmem_limit;
			else
				end_phys = meminfo.bank[i].start;

			if (meminfo.bank[i-1].start + meminfo.bank[i-1].size
				   <= MAX_HOLE_ADDRESS) {
				va_start = __va(meminfo.bank[i-1].start
						+ meminfo.bank[i-1].size);
				va_end = __va(end_phys);
				printk(KERN_NOTICE
				"	   vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n",
					   MLM((unsigned long)va_start,
						   (unsigned long)va_end));
			}
		}
	}
}
#endif

/*
 * mem_init() marks the free areas in the mem_map and tells us how much
 * memory is free.  This is done after various parts of the system have
 * claimed their memory after the kernel image.
 */
void __init mem_init(void)
{
	unsigned long reserved_pages, free_pages;
	struct memblock_region *reg;
	int i;
#ifdef CONFIG_HAVE_TCM
	/* These pointers are filled in on TCM detection */
	extern u32 dtcm_end;
	extern u32 itcm_end;
#endif

	max_mapnr   = pfn_to_page(max_pfn + PHYS_PFN_OFFSET) - mem_map;

	/* this will put all unused low memory onto the freelists */
	free_unused_memmap(&meminfo);

	totalram_pages += free_all_bootmem();

#ifdef CONFIG_SA1111
	/* now that our DMA memory is actually so designated, we can free it */
	totalram_pages += free_area(PHYS_PFN_OFFSET,
				    __phys_to_pfn(__pa(swapper_pg_dir)), NULL);
#endif

	free_highpages();

	reserved_pages = free_pages = 0;

	for_each_bank(i, &meminfo) {
		struct membank *bank = &meminfo.bank[i];
		unsigned int pfn1, pfn2;
		struct page *page, *end;

		pfn1 = bank_pfn_start(bank);
		pfn2 = bank_pfn_end(bank);

		page = pfn_to_page(pfn1);
		end  = pfn_to_page(pfn2 - 1) + 1;

		do {
			if (PageReserved(page))
				reserved_pages++;
			else if (!page_count(page))
				free_pages++;
			page++;
#ifdef CONFIG_SPARSEMEM
			pfn1++;
			if (!(pfn1 % PAGES_PER_SECTION))
				page = pfn_to_page(pfn1);
		} while (pfn1 < pfn2);
#else
		} while (page < end);
#endif
	}

	/*
	 * Since our memory may not be contiguous, calculate the
	 * real number of pages we have in this system
	 */
	printk(KERN_INFO "Memory:");
	num_physpages = 0;
	for_each_memblock(memory, reg) {
		unsigned long pages = memblock_region_memory_end_pfn(reg) -
			memblock_region_memory_base_pfn(reg);
		num_physpages += pages;
		printk(" %ldMB", pages >> (20 - PAGE_SHIFT));
	}
	printk(" = %luMB total\n", num_physpages >> (20 - PAGE_SHIFT));

	printk(KERN_NOTICE "Memory: %luk/%luk available, %luk reserved, %luK highmem\n",
		nr_free_pages() << (PAGE_SHIFT-10),
		free_pages << (PAGE_SHIFT-10),
		reserved_pages << (PAGE_SHIFT-10),
		totalhigh_pages << (PAGE_SHIFT-10));

	printk(KERN_NOTICE "Virtual kernel memory layout:\n"
			"    vector  : 0x%08lx - 0x%08lx   (%4ld kB)\n"
#ifdef CONFIG_ARM_USE_USER_ACCESSIBLE_TIMERS
			"    timers  : 0x%08lx - 0x%08lx   (%4ld kB)\n"
#endif
#ifdef CONFIG_HAVE_TCM
			"    DTCM    : 0x%08lx - 0x%08lx   (%4ld kB)\n"
			"    ITCM    : 0x%08lx - 0x%08lx   (%4ld kB)\n"
#endif
			"    fixmap  : 0x%08lx - 0x%08lx   (%4ld kB)\n",
			MLK(UL(CONFIG_VECTORS_BASE), UL(CONFIG_VECTORS_BASE) +
				(PAGE_SIZE)),
#ifdef CONFIG_ARM_USE_USER_ACCESSIBLE_TIMERS
			MLK(UL(CONFIG_ARM_USER_ACCESSIBLE_TIMER_BASE),
				UL(CONFIG_ARM_USER_ACCESSIBLE_TIMER_BASE)
					+ (PAGE_SIZE)),
#endif
#ifdef CONFIG_HAVE_TCM
			MLK(DTCM_OFFSET, (unsigned long) dtcm_end),
			MLK(ITCM_OFFSET, (unsigned long) itcm_end),
#endif
			MLK(FIXADDR_START, FIXADDR_TOP));
#ifdef CONFIG_ENABLE_VMALLOC_SAVING
	print_vmalloc_lowmem_info();
#else
	printk(KERN_NOTICE
		   "    vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n"
		   "    lowmem  : 0x%08lx - 0x%08lx   (%4ld MB)\n",
		   MLM(VMALLOC_START, VMALLOC_END),
		   MLM(PAGE_OFFSET, (unsigned long)high_memory));
#endif

	printk(KERN_NOTICE
#ifdef CONFIG_HIGHMEM
		   "    pkmap   : 0x%08lx - 0x%08lx   (%4ld MB)\n"
#endif
#ifdef CONFIG_MODULES
		   "    modules : 0x%08lx - 0x%08lx   (%4ld MB)\n"
#endif
		   "      .text : 0x%p" " - 0x%p" "   (%4d kB)\n"
		   "      .init : 0x%p" " - 0x%p" "   (%4d kB)\n"
		   "      .data : 0x%p" " - 0x%p" "   (%4d kB)\n"
		   "       .bss : 0x%p" " - 0x%p" "   (%4d kB)\n",
#ifdef CONFIG_HIGHMEM
		   MLM(PKMAP_BASE, (PKMAP_BASE) + (LAST_PKMAP) *
				(PAGE_SIZE)),
#endif
#ifdef CONFIG_MODULES
		   MLM(MODULES_VADDR, MODULES_END),
#endif

		   MLK_ROUNDUP(_text, _etext),
		   MLK_ROUNDUP(__init_begin, __init_end),
		   MLK_ROUNDUP(_sdata, _edata),
		   MLK_ROUNDUP(__bss_start, __bss_stop));

	/*
	 * Check boundaries twice: Some fundamental inconsistencies can
	 * be detected at build time already.
	 */
#ifdef CONFIG_MMU
	BUILD_BUG_ON(TASK_SIZE				> MODULES_VADDR);
	BUG_ON(TASK_SIZE				> MODULES_VADDR);
#endif

#ifdef CONFIG_HIGHMEM
	BUILD_BUG_ON(PKMAP_BASE + LAST_PKMAP * PAGE_SIZE > PAGE_OFFSET);
	BUG_ON(PKMAP_BASE + LAST_PKMAP * PAGE_SIZE	> PAGE_OFFSET);
#endif

	if (PAGE_SIZE >= 16384 && num_physpages <= 128) {
		extern int sysctl_overcommit_memory;
		/*
		 * On a machine this small we won't get
		 * anywhere without overcommit, so turn
		 * it on by default.
		 */
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}

#undef MLK
#undef MLM
#undef MLK_ROUNDUP
void free_initmem(void)
{
	unsigned long reclaimed_initmem;
#ifdef CONFIG_HAVE_TCM
	extern char __tcm_start, __tcm_end;

	poison_init_mem(&__tcm_start, &__tcm_end - &__tcm_start);
	totalram_pages += free_area(__phys_to_pfn(__pa(&__tcm_start)),
				    __phys_to_pfn(__pa(&__tcm_end)),
				    "TCM link");
#endif

#ifdef CONFIG_STRICT_MEMORY_RWX
	poison_init_mem((char *)__arch_info_begin,
		__init_end - (char *)__arch_info_begin);
	reclaimed_initmem = free_area(__phys_to_pfn(__pa(__arch_info_begin)),
				    __phys_to_pfn(__pa(__init_end)),
				    "init");
	totalram_pages += reclaimed_initmem;
#else
	poison_init_mem(__init_begin, __init_end - __init_begin);
	if (!machine_is_integrator() && !machine_is_cintegrator()) {
		reclaimed_initmem = free_area(__phys_to_pfn(__pa(__init_begin)),
					    __phys_to_pfn(__pa(__init_end)),
					    "init");
		totalram_pages += reclaimed_initmem;
	}
#endif
}

#ifdef CONFIG_BLK_DEV_INITRD

static int keep_initrd;

void free_initrd_mem(unsigned long start, unsigned long end)
{
	unsigned long reclaimed_initrd_mem;

	if (!keep_initrd) {
		poison_init_mem((void *)start, PAGE_ALIGN(end) - start);
		reclaimed_initrd_mem = free_area(__phys_to_pfn(__pa(start)),
						 __phys_to_pfn(__pa(end)),
						 "initrd");
		totalram_pages += reclaimed_initrd_mem;
	}
}

static int __init keepinitrd_setup(char *__unused)
{
	keep_initrd = 1;
	return 1;
}

__setup("keepinitrd", keepinitrd_setup);
#endif

#ifdef CONFIG_MSM_KRAIT_WFE_FIXUP
static int __init msm_krait_wfe_init(void)
{
	unsigned int val, midr;
	midr = read_cpuid_id() & 0xffffff00;
	if ((midr == 0x511f0400) || (midr == 0x510f0600)) {
		asm volatile("mrc p15, 7, %0, c15, c0, 5" : "=r" (val));
		msm_krait_need_wfe_fixup = (val & 0x10000) ? 1 : 0;
	}
	return 0;
}
pure_initcall(msm_krait_wfe_init);
#endif

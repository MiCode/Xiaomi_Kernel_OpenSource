/*
 * Based on arch/arm/mm/init.c
 *
 * Copyright (C) 1995-2005 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mman.h>
#include <linux/nodemask.h>
#include <linux/initrd.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/sort.h>
#include <linux/of_fdt.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/efi.h>
#include <linux/swiotlb.h>
#include <linux/mm.h>

#include <asm/boot.h>
#include <asm/fixmap.h>
#include <asm/kasan.h>
#include <asm/kernel-pgtable.h>
#include <asm/memory.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/sizes.h>
#include <asm/tlb.h>
#include <asm/alternative.h>

#include "mm.h"

/*
 * We need to be able to catch inadvertent references to memstart_addr
 * that occur (potentially in generic code) before arm64_memblock_init()
 * executes, which assigns it its actual value. So use a default value
 * that cannot be mistaken for a real physical address.
 */
s64 memstart_addr __read_mostly = -1;
phys_addr_t arm64_dma_phys_limit __read_mostly;

#ifdef CONFIG_BLK_DEV_INITRD
static int __init early_initrd(char *p)
{
	unsigned long start, size;
	char *endp;

	start = memparse(p, &endp);
	if (*endp == ',') {
		size = memparse(endp + 1, NULL);

		initrd_start = start;
		initrd_end = start + size;
	}
	return 0;
}
early_param("initrd", early_initrd);
#endif

/*
 * Return the maximum physical address for ZONE_DMA (DMA_BIT_MASK(32)). It
 * currently assumes that for memory starting above 4G, 32-bit devices will
 * use a DMA offset.
 */
static phys_addr_t __init max_zone_dma_phys(void)
{
	phys_addr_t offset = memblock_start_of_DRAM() & GENMASK_ULL(63, 32);
	return min(offset + (1ULL << 32), memblock_end_of_DRAM());
}

static void __init zone_sizes_init(unsigned long min, unsigned long max)
{
	struct memblock_region *reg;
	unsigned long zone_size[MAX_NR_ZONES], zhole_size[MAX_NR_ZONES];
	unsigned long max_dma = min;

	memset(zone_size, 0, sizeof(zone_size));

	/* 4GB maximum for 32-bit only capable devices */
#ifdef CONFIG_ZONE_DMA
	max_dma = PFN_DOWN(arm64_dma_phys_limit);
	zone_size[ZONE_DMA] = max_dma - min;
#endif
	zone_size[ZONE_NORMAL] = max - max_dma;

	memcpy(zhole_size, zone_size, sizeof(zhole_size));

	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (start >= max)
			continue;

#ifdef CONFIG_ZONE_DMA
		if (start < max_dma) {
			unsigned long dma_end = min(end, max_dma);
			zhole_size[ZONE_DMA] -= dma_end - start;
		}
#endif
		if (end > max_dma) {
			unsigned long normal_end = min(end, max);
			unsigned long normal_start = max(start, max_dma);
			zhole_size[ZONE_NORMAL] -= normal_end - normal_start;
		}
	}

	free_area_init_node(0, zone_size, min, zhole_size);
}

#ifdef CONFIG_HAVE_ARCH_PFN_VALID
int pfn_valid(unsigned long pfn)
{
	phys_addr_t addr = pfn << PAGE_SHIFT;

	if ((addr >> PAGE_SHIFT) != pfn)
		return 0;
	return memblock_is_map_memory(addr);
}
EXPORT_SYMBOL(pfn_valid);
#endif

#ifndef CONFIG_SPARSEMEM
static void __init arm64_memory_present(void)
{
}
#else
static void __init arm64_memory_present(void)
{
	struct memblock_region *reg;

	for_each_memblock(memory, reg)
		memory_present(0, memblock_region_memory_base_pfn(reg),
			       memblock_region_memory_end_pfn(reg));
}
#endif

static phys_addr_t memory_limit = (phys_addr_t)ULLONG_MAX;
static phys_addr_t bootloader_memory_limit;

/*
 * Limit the memory size that was specified via FDT.
 */
static int __init early_mem(char *p)
{
	if (!p)
		return 1;

	memory_limit = memparse(p, &p) & PAGE_MASK;
	pr_notice("Memory limited to %lldMB\n", memory_limit >> 20);

	return 0;
}
early_param("mem", early_mem);

void __init arm64_memblock_init(void)
{
	const s64 linear_region_size = -(s64)PAGE_OFFSET;

	/*
	 * Ensure that the linear region takes up exactly half of the kernel
	 * virtual address space. This way, we can distinguish a linear address
	 * from a kernel/module/vmalloc address by testing a single bit.
	 */
	BUILD_BUG_ON(linear_region_size != BIT(VA_BITS - 1));

	/*
	 * Select a suitable value for the base of physical memory.
	 */
	memstart_addr = round_down(memblock_start_of_DRAM(),
				   ARM64_MEMSTART_ALIGN);

	/*
	 * Remove the memory that we will not be able to cover with the
	 * linear mapping. Take care not to clip the kernel which may be
	 * high in memory.
	 */
	memblock_remove(max_t(u64, memstart_addr + linear_region_size,
			__pa_symbol(_end)), ULLONG_MAX);
	if (memstart_addr + linear_region_size < memblock_end_of_DRAM()) {
		/* ensure that memstart_addr remains sufficiently aligned */
		memstart_addr = round_up(memblock_end_of_DRAM() - linear_region_size,
					 ARM64_MEMSTART_ALIGN);
		memblock_remove(0, memstart_addr);
	}

	/*
	 * Apply the memory limit if it was set. Since the kernel may be loaded
	 * high up in memory, add back the kernel region that must be accessible
	 * via the linear mapping.
	 */
	if (memory_limit != (phys_addr_t)ULLONG_MAX) {
		/*
		 * Save bootloader imposed memory limit before we overwirte
		 * memblock.
		 */
		bootloader_memory_limit = memblock_end_of_DRAM();
		memblock_enforce_memory_limit(memory_limit);
		memblock_add(__pa_symbol(_text), (u64)(_end - _text));
	}

	if (IS_ENABLED(CONFIG_RANDOMIZE_BASE)) {
		extern u16 memstart_offset_seed;
		u64 range = linear_region_size -
			    (memblock_end_of_DRAM() - memblock_start_of_DRAM());

		/*
		 * If the size of the linear region exceeds, by a sufficient
		 * margin, the size of the region that the available physical
		 * memory spans, randomize the linear region as well.
		 */
		if (memstart_offset_seed > 0 && range >= ARM64_MEMSTART_ALIGN) {
			range /= ARM64_MEMSTART_ALIGN;
			memstart_addr -= ARM64_MEMSTART_ALIGN *
					 ((range * memstart_offset_seed) >> 16);
		}
	}

	/*
	 * Register the kernel text, kernel data, initrd, and initial
	 * pagetables with memblock.
	 */
	memblock_reserve(__pa_symbol(_text), _end - _text);
#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start) {
		memblock_reserve(initrd_start, initrd_end - initrd_start);

		/* the generic initrd code expects virtual addresses */
		initrd_start = __phys_to_virt(initrd_start);
		initrd_end = __phys_to_virt(initrd_end);
	}
#endif

	early_init_fdt_scan_reserved_mem();

	/* 4GB maximum for 32-bit only capable devices */
	if (IS_ENABLED(CONFIG_ZONE_DMA))
		arm64_dma_phys_limit = max_zone_dma_phys();
	else
		arm64_dma_phys_limit = PHYS_MASK + 1;
	high_memory = __va(memblock_end_of_DRAM() - 1) + 1;
	dma_contiguous_reserve(arm64_dma_phys_limit);

	memblock_allow_resize();
	memblock_dump_all();
}

void __init bootmem_init(void)
{
	unsigned long min, max;

	min = PFN_UP(memblock_start_of_DRAM());
	max = PFN_DOWN(memblock_end_of_DRAM());

	early_memtest(min << PAGE_SHIFT, max << PAGE_SHIFT);

	/*
	 * Sparsemem tries to allocate bootmem in memory_present(), so must be
	 * done after the fixed reservations.
	 */
	arm64_memory_present();

	sparse_init();
	zone_sizes_init(min, max);

	max_pfn = max_low_pfn = max;
}

#ifndef CONFIG_SPARSEMEM_VMEMMAP
static inline void free_memmap(unsigned long start_pfn, unsigned long end_pfn)
{
	struct page *start_pg, *end_pg;
	unsigned long pg, pgend;

	/*
	 * Convert start_pfn/end_pfn to a struct page pointer.
	 */
	start_pg = pfn_to_page(start_pfn - 1) + 1;
	end_pg = pfn_to_page(end_pfn - 1) + 1;

	/*
	 * Convert to physical addresses, and round start upwards and end
	 * downwards.
	 */
	pg = (unsigned long)PAGE_ALIGN(__pa(start_pg));
	pgend = (unsigned long)__pa(end_pg) & PAGE_MASK;

	/*
	 * If there are free pages between these, free the section of the
	 * memmap array.
	 */
	if (pg < pgend)
		free_bootmem(pg, pgend - pg);
}

/*
 * The mem_map array can get very big. Free the unused area of the memory map.
 */
static void __init free_unused_memmap(void)
{
	unsigned long start, prev_end = 0;
	struct memblock_region *reg;

	for_each_memblock(memory, reg) {
		start = __phys_to_pfn(reg->base);

#ifdef CONFIG_SPARSEMEM
		/*
		 * Take care not to free memmap entries that don't exist due
		 * to SPARSEMEM sections which aren't present.
		 */
		start = min(start, ALIGN(prev_end, PAGES_PER_SECTION));
#endif
		/*
		 * If we had a previous bank, and there is a space between the
		 * current bank and the previous, free it.
		 */
		if (prev_end && prev_end < start)
			free_memmap(prev_end, start);

		/*
		 * Align up here since the VM subsystem insists that the
		 * memmap entries are valid from the bank end aligned to
		 * MAX_ORDER_NR_PAGES.
		 */
		prev_end = ALIGN(__phys_to_pfn(reg->base + reg->size),
				 MAX_ORDER_NR_PAGES);
	}

#ifdef CONFIG_SPARSEMEM
	if (!IS_ALIGNED(prev_end, PAGES_PER_SECTION))
		free_memmap(prev_end, ALIGN(prev_end, PAGES_PER_SECTION));
#endif
}
#endif	/* !CONFIG_SPARSEMEM_VMEMMAP */

/*
 * mem_init() marks the free areas in the mem_map and tells us how much memory
 * is free.  This is done after various parts of the system have claimed their
 * memory after the kernel image.
 */
void __init mem_init(void)
{
	swiotlb_init(1);

	set_max_mapnr(pfn_to_page(max_pfn) - mem_map);

#ifndef CONFIG_SPARSEMEM_VMEMMAP
	free_unused_memmap();
#endif
	/* this will put all unused low memory onto the freelists */
	free_all_bootmem();

	mem_init_print_info(NULL);

#define MLK(b, t) b, t, ((t) - (b)) >> 10
#define MLM(b, t) b, t, ((t) - (b)) >> 20
#define MLG(b, t) b, t, ((t) - (b)) >> 30
#define MLK_ROUNDUP(b, t) b, t, DIV_ROUND_UP(((t) - (b)), SZ_1K)

	pr_notice("Virtual kernel memory layout:\n"
#ifdef CONFIG_KASAN
		  "    kasan   : 0x%16lx - 0x%16lx   (%6ld GB)\n"
#endif
		  "    modules : 0x%16lx - 0x%16lx   (%6ld MB)\n"
		  "    vmalloc : 0x%16lx - 0x%16lx   (%6ld GB)\n"
		  "      .init : 0x%p" " - 0x%p" "   (%6ld KB)\n"
		  "      .text : 0x%p" " - 0x%p" "   (%6ld KB)\n"
		  "    .rodata : 0x%p" " - 0x%p" "   (%6ld KB)\n"
		  "      .data : 0x%p" " - 0x%p" "   (%6ld KB)\n"
#ifdef CONFIG_SPARSEMEM_VMEMMAP
		  "    vmemmap : 0x%16lx - 0x%16lx   (%6ld GB maximum)\n"
		  "              0x%16lx - 0x%16lx   (%6ld MB actual)\n"
#endif
		  "    fixed   : 0x%16lx - 0x%16lx   (%6ld KB)\n"
		  "    PCI I/O : 0x%16lx - 0x%16lx   (%6ld MB)\n"
		  "    memory  : 0x%16lx - 0x%16lx   (%6ld MB)\n",
#ifdef CONFIG_KASAN
		  MLG(KASAN_SHADOW_START, KASAN_SHADOW_END),
#endif
		  MLM(MODULES_VADDR, MODULES_END),
		  MLG(VMALLOC_START, VMALLOC_END),
		  MLK_ROUNDUP(__init_begin, __init_end),
		  MLK_ROUNDUP(_text, _etext),
		  MLK_ROUNDUP(__start_rodata, __init_begin),
		  MLK_ROUNDUP(_sdata, _edata),
#ifdef CONFIG_SPARSEMEM_VMEMMAP
		  MLG(VMEMMAP_START,
		      VMEMMAP_START + VMEMMAP_SIZE),
		  MLM((unsigned long)phys_to_page(memblock_start_of_DRAM()),
		      (unsigned long)virt_to_page(high_memory)),
#endif
		  MLK(FIXADDR_START, FIXADDR_TOP),
		  MLM(PCI_IO_START, PCI_IO_END),
		  MLM(__phys_to_virt(memblock_start_of_DRAM()),
		      (unsigned long)high_memory));

#undef MLK
#undef MLM
#undef MLK_ROUNDUP

	/*
	 * Check boundaries twice: Some fundamental inconsistencies can be
	 * detected at build time already.
	 */
#ifdef CONFIG_COMPAT
	BUILD_BUG_ON(TASK_SIZE_32			> TASK_SIZE_64);
#endif
	BUILD_BUG_ON(TASK_SIZE_64			> MODULES_VADDR);
	BUG_ON(TASK_SIZE_64				> MODULES_VADDR);

	if (PAGE_SIZE >= 16384 && get_num_physpages() <= 128) {
		extern int sysctl_overcommit_memory;
		/*
		 * On a machine this small we won't get anywhere without
		 * overcommit, so turn it on by default.
		 */
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}

static inline void poison_init_mem(void *s, size_t count)
{
	memset(s, 0, count);
}

void free_initmem(void)
{
	free_initmem_default(0);
	fixup_init();
}

#ifdef CONFIG_BLK_DEV_INITRD

static int keep_initrd __initdata;

void __init free_initrd_mem(unsigned long start, unsigned long end)
{
	if (!keep_initrd)
		free_reserved_area((void *)start, (void *)end, 0, "initrd");
}

static int __init keepinitrd_setup(char *__unused)
{
	keep_initrd = 1;
	return 1;
}

__setup("keepinitrd", keepinitrd_setup);
#endif

#ifdef CONFIG_KERNEL_TEXT_RDONLY
void set_kernel_text_ro(void)
{
	unsigned long start = PFN_ALIGN(_stext);
	unsigned long end = PFN_ALIGN(_etext);

	/*
	 * Set the kernel identity mapping for text RO.
	 */
	set_memory_ro(start, (end - start) >> PAGE_SHIFT);
}
#endif
/*
 * Dump out memory limit information on panic.
 */
static int dump_mem_limit(struct notifier_block *self, unsigned long v, void *p)
{
	if (memory_limit != (phys_addr_t)ULLONG_MAX) {
		pr_emerg("Memory Limit: %llu MB\n", memory_limit >> 20);
	} else {
		pr_emerg("Memory Limit: none\n");
	}
	return 0;
}

static struct notifier_block mem_limit_notifier = {
	.notifier_call = dump_mem_limit,
};

static int __init register_mem_limit_dumper(void)
{
	atomic_notifier_chain_register(&panic_notifier_list,
				       &mem_limit_notifier);
	return 0;
}
__initcall(register_mem_limit_dumper);

#ifdef CONFIG_MEMORY_HOTPLUG
int arch_add_memory(int nid, u64 start, u64 size, bool for_device)
{
	pg_data_t *pgdat;
	struct zone *zone;
	unsigned long start_pfn = start >> PAGE_SHIFT;
	unsigned long nr_pages = size >> PAGE_SHIFT;
	unsigned long end_pfn = start_pfn + nr_pages;
	unsigned long max_sparsemem_pfn = 1UL << (MAX_PHYSMEM_BITS-PAGE_SHIFT);
	int ret;

	if (end_pfn > max_sparsemem_pfn) {
		pr_err("end_pfn too big");
		return -1;
	}
	hotplug_paging(start, size);

	/*
	 * Mark the first page in the range as unusable. This is needed
	 * because __add_section (within __add_pages) wants pfn_valid
	 * of it to be false, and in arm64 pfn falid is implemented by
	 * just checking at the nomap flag for existing blocks.
	 *
	 * A small trick here is that __add_section() requires only
	 * phys_start_pfn (that is the first pfn of a section) to be
	 * invalid. Regardless of whether it was assumed (by the function
	 * author) that all pfns within a section are either all valid
	 * or all invalid, it allows to avoid looping twice (once here,
	 * second when memblock_clear_nomap() is called) through all
	 * pfns of the section and modify only one pfn. Thanks to that,
	 * further, in __add_zone() only this very first pfn is skipped
	 * and corresponding page is not flagged reserved. Therefore it
	 * is enough to correct this setup only for it.
	 *
	 * When arch_add_memory() returns the walk_memory_range() function
	 * is called and passed with online_memory_block() callback,
	 * which execution finally reaches the memory_block_action()
	 * function, where also only the first pfn of a memory block is
	 * checked to be reserved. Above, it was first pfn of a section,
	 * here it is a block but
	 * (drivers/base/memory.c):
	 *     sections_per_block = block_sz / MIN_MEMORY_BLOCK_SIZE;
	 * (include/linux/memory.h):
	 *     #define MIN_MEMORY_BLOCK_SIZE     (1UL << SECTION_SIZE_BITS)
	 * so we can consider block and section equivalently
	 */
	memblock_mark_nomap(start, 1<<PAGE_SHIFT);

	pgdat = NODE_DATA(nid);

	zone = pgdat->node_zones +
		zone_for_memory(nid, start, size, ZONE_NORMAL, for_device);
	ret = __add_pages(nid, zone, start_pfn, nr_pages);

	/*
	 * Make the pages usable after they have been added.
	 * This will make pfn_valid return true
	 */
	memblock_clear_nomap(start, 1<<PAGE_SHIFT);

	/*
	 * This is a hack to avoid having to mix arch specific code
	 * into arch independent code. SetPageReserved is supposed
	 * to be called by __add_zone (within __add_section, within
	 * __add_pages). However, when it is called there, it assumes that
	 * pfn_valid returns true.  For the way pfn_valid is implemented
	 * in arm64 (a check on the nomap flag), the only way to make
	 * this evaluate true inside __add_zone is to clear the nomap
	 * flags of blocks in architecture independent code.
	 *
	 * To avoid this, we set the Reserved flag here after we cleared
	 * the nomap flag in the line above.
	 */
	SetPageReserved(pfn_to_page(start_pfn));

	if (ret)
		pr_warn("%s: Problem encountered in __add_pages() ret=%d\n",
			__func__, ret);

	return ret;
}

#ifdef CONFIG_MEMORY_HOTREMOVE
static void kernel_physical_mapping_remove(unsigned long start,
	unsigned long end)
{
	start = (unsigned long)__va(start);
	end = (unsigned long)__va(end);

	remove_pagetable(start, end, true);

}

int arch_remove_memory(u64 start, u64 size)
{
	unsigned long start_pfn = start >> PAGE_SHIFT;
	unsigned long nr_pages = size >> PAGE_SHIFT;
	struct page *page = pfn_to_page(start_pfn);
	struct zone *zone;
	int ret = 0;

	zone = page_zone(page);
	ret = __remove_pages(zone, start_pfn, nr_pages);
	WARN_ON_ONCE(ret);

	kernel_physical_mapping_remove(start, start + size);

	return ret;
}

#endif /* CONFIG_MEMORY_HOTREMOVE */
static int arm64_online_page(struct page *page)
{
	unsigned long target_pfn = page_to_pfn(page);
	unsigned long limit = __phys_to_pfn(bootloader_memory_limit);

	if (target_pfn >= limit)
		return -EINVAL;

	__online_page_set_limits(page);
	__online_page_increment_counters(page);
	__online_page_free(page);

	return 0;
}

static int __init arm64_memory_hotplug_init(void)
{
	set_online_page_callback(&arm64_online_page);
	return 0;
}
subsys_initcall(arm64_memory_hotplug_init);
#endif /* CONFIG_MEMORY_HOTPLUG */

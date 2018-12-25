/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#define pr_fmt(fmt) "ZMC: " fmt
#define CONFIG_MTK_ZONE_MOVABLE_CMA_DEBUG

#include <linux/types.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/printk.h>
#include <linux/memblock.h>
#include <linux/page-isolation.h>

#include "mt-plat/mtk_meminfo.h"
#include "single_cma.h"

static struct cma *cma[MAX_CMA_AREAS];
static phys_addr_t movable_min = ULONG_MAX;
static phys_addr_t movable_max;

phys_addr_t zmc_max_zone_dma_phys = 0xc0000000ULL;
bool zmc_reserved_mem_inited;

#define END_OF_REGISTER ((void *)(0x7a6d63))
enum ZMC_ZONE_ORDER {
	ZMC_LOCATE_MOVABLE,
	ZMC_LOCATE_NORMAL,
	NR_ZMC_LOCATIONS,
};
static struct single_cma_registration *single_cma_list[NR_ZMC_LOCATIONS][4] = {
	/* CMA region need to locate at NORMAL zone */
	[ZMC_LOCATE_NORMAL] = {
#ifdef CONFIG_MTK_MEMORY_LOWPOWER
		&memory_lowpower_registration,
#endif
		END_OF_REGISTER
	},
	/* MOVABLE, instead */
	[ZMC_LOCATE_MOVABLE] = {
#ifdef CONFIG_MTK_SSMR
		&memory_ssmr_registration,
#endif
		END_OF_REGISTER
	},
};

#ifdef CONFIG_MTK_MEMORY_LOWPOWER
/* 512MB alignment for DRAM size > 4GB */
#define ZMC_CHECK_FIX_ALIGNMENT	(0x20000000ULL)
/*
 * Resize ZMC base & size according to total physical size (T).
 * After resizing, zmc_max_zone_dma_phys will be,
 * 0xc0000000 if T <= 4GB
 * 0xc0000000 ~ 0x100000000 if 4GB < T <= 6GB
 * 0x100000000 if T > 6GB
 */
static void __init check_and_fix_base(struct reserved_mem *rmem,
		phys_addr_t total_phys_size)
{
	phys_addr_t new_zmc_base, return_size;

	pr_info("%s: total phys size: %pa\n", __func__, &total_phys_size);

	/* No need to fix if the size of DRAM is less or equal to 4GB */
	if (total_phys_size <= 0x100000000ULL)
		return;

	/* Find a new base */
	new_zmc_base = round_up(
			memblock_start_of_DRAM() + (total_phys_size >> 1),
			ZMC_CHECK_FIX_ALIGNMENT);

	/* Don't exceed the limitation of DMA zone */
	zmc_max_zone_dma_phys = new_zmc_base = min(new_zmc_base,
			(phys_addr_t)(1ULL << 32));

	/* Resize it */
	if (rmem->base < new_zmc_base) {
		return_size = new_zmc_base - rmem->base;
		memblock_free(rmem->base, return_size);
		memblock_add(rmem->base, return_size);
		rmem->base = new_zmc_base;
		rmem->size -= return_size;
		pr_info("%s: new base: %pa, new size: %pa\n",
				__func__, &rmem->base, &rmem->size);
	}
}
#else	/* !CONFIG_MTK_MEMORY_LOWPOWER */
static void __init check_and_fix_base(struct reserved_mem *rmem,
		phys_addr_t total_phys_size)
{
	/* do nothing */
}
#endif

static bool __init zmc_is_the_last(struct reserved_mem *rmem)
{
	phys_addr_t phys_end = memblock_end_of_DRAM();
	phys_addr_t rmem_end_max = rmem->base + rmem->size +
		(pageblock_nr_pages << PAGE_SHIFT);

	pr_info("%s: phys end: %pa, rmem end max: %pa\n",
			__func__, &phys_end, &rmem_end_max);

	if (rmem_end_max >= phys_end)
		return true;

	return false;
}

static int __init zmc_memory_init(struct reserved_mem *rmem)
{
	int ret;
	int order, i;
	int cma_area_count = 0;
	phys_addr_t zmc_size = rmem->size;
	phys_addr_t total_phys_size = memblock_phys_mem_size();

	pr_info("%s, name: %s, base: %pa, size: %pa\n", __func__,
			rmem->name, &rmem->base, &rmem->size);

	if (total_phys_size > 0x80000000ULL &&
			rmem->base < zmc_max_zone_dma_phys) {
		pr_info("[Fail] Unsupported memory range under 0x%lx (DMA max range).\n",
				(unsigned long)zmc_max_zone_dma_phys);
		pr_info("Abort reserve memory.\n");
		memblock_free(rmem->base, rmem->size);
		memblock_add(rmem->base, rmem->size);
		return -1;
	}

	if (!zmc_is_the_last(rmem)) {
		pr_info("[Fail] ZMC is not the last\n");
		memblock_free(rmem->base, rmem->size);
		memblock_add(rmem->base, rmem->size);
		return -1;
	}

	check_and_fix_base(rmem, total_phys_size);

	/*
	 * Init CMAs -
	 * dts range: |...............................|
	 *             r->base
	 *                            r->base + r->size
	 *
	 * init order:                <---accu_size---|
	 */
	for (order = 0; order < NR_ZMC_LOCATIONS; order++) {
		struct single_cma_registration *p;

		pr_info("Start to zone: %d\n", order);
		for (i = 0; i < ARRAY_SIZE(single_cma_list[order]); i++) {
			phys_addr_t start, end;

			p = single_cma_list[order][i];
			if (p == END_OF_REGISTER)
				break;

			if (p->preinit) {
				if (p->preinit(rmem))
					break;
			}

			end = rmem->base + rmem->size;
			pr_info("::[%s]: size: %pa, align: %pa\n",
					p->name, &p->size, &p->align);
			pr_info("::[%pa-%pa] remain of rmem\n",
					&rmem->base, &end);

			if (p->flag & ZMC_ALLOC_ALL)
				start = rmem->base;
			else {
				phys_addr_t alignment;

				/* cma alignment */
				alignment = PAGE_SIZE <<
					max(MAX_ORDER - 1, pageblock_order);
				alignment = max(p->align, alignment);

				start = round_down(rmem->base +
						   rmem->size -
						   p->size, alignment);

				if (start < rmem->base) {
					pr_info("::[Reserve fail]: insufficient memory.\n");
					pr_info("::[%pa - %pa] remain of rmem\n",
							&rmem->base, &end);
					pr_info("::[%pa - %pa] to cma init\n",
							&start, &end);
					continue;
				}
			}

			pr_info("::cma_init_reserved_mem - [%pa - %pa]\n",
					&start, &end);
			ret = cma_init_reserved_mem(start,
						    end - start,
						    0,
						    &cma[cma_area_count]);
			if (ret) {
				pr_info(":: %s cma failed at %d, ret: %d\n",
						__func__, cma_area_count, ret);
				continue;
			}

			if (p->flag & ZMC_ALLOC_ALL)
				p->size = zmc_size;
			else
				p->size = end - start;

			rmem->size -= end - start;

			if (order == ZMC_LOCATE_MOVABLE) {
				movable_min = min(movable_min, start);
				movable_max = max(movable_max, end);
				pr_info("===> MOVABLE ZONE: Update range[%pa,%pa)\n",
						&movable_min, &movable_max);
			}

			if (p->init)
				p->init(cma[cma_area_count]);

			cma_area_count++;
			zmc_reserved_mem_inited = true;
			pr_info("::[PASS]: %s[%pa-%pa] (rmem->size=%pa)\n",
					p->name, &start, &end, &rmem->size);
		}
	}

	/* Putback remaining rmem to memblock.memory */
	if (rmem->size > 0) {
		memblock_free(rmem->base, rmem->size);
		memblock_add(rmem->base, rmem->size);
	}

	return 0;
}
RESERVEDMEM_OF_DECLARE(zone_movable_cma_init, "mediatek,zone_movable_cma",
			zmc_memory_init);

bool is_zmc_inited(void)
{
	return zmc_reserved_mem_inited;
}

void zmc_get_range(phys_addr_t *base, phys_addr_t *size)
{
	if (movable_max > movable_min) {
		pr_info("Query return: [%pa,%pa)\n",
				&movable_min, &movable_max);
		*base = movable_min;
		*size = movable_max - movable_min;
	} else {
		*base = *size = 0;
	}
}

static bool system_mem_status_ok(unsigned long count)
{
	struct pglist_data *pgdat;
	enum zone_type zoneidx;
	struct zone *z;
	unsigned long free = 0, file = 0;
	unsigned long high_wmark = 0;

	/* Go through all zones below OPT_ZONE_MOVABLE_CMA */
	for_each_online_pgdat(pgdat) {
		for (zoneidx = 0; zoneidx < OPT_ZONE_MOVABLE_CMA; zoneidx++) {
			z = pgdat->node_zones + zoneidx;
			free += zone_page_state(z, NR_FREE_PAGES);
			file += (zone_page_state(z, NR_ZONE_ACTIVE_FILE) +
				 zone_page_state(z, NR_ZONE_INACTIVE_FILE));
			high_wmark += high_wmark_pages(z) +
				z->nr_reserved_highatomic;
		}
	}

	pr_info("%s: free(%lu) file(%lu) high(%lu) count(%lu)\n",
			__func__, free, file, high_wmark, count);

	/* Hope the system has as less memory reclaim as possible */
	high_wmark += count;
	if (free < high_wmark)
		return false;

	return true;
}

static bool zmc_check_mem_status_ok(unsigned long count)
{
	struct pglist_data *pgdat;
	struct zone *z;
	unsigned long free = 0, available = 0, minus = 0;

	/* Check OPT_ZONE_MOVABLE_CMA first */
	for_each_online_pgdat(pgdat) {
		z = pgdat->node_zones + OPT_ZONE_MOVABLE_CMA;
		free += zone_page_state(z, NR_FREE_PAGES);
		available += zone_page_state(z, NR_FREE_PAGES);
		minus += zone_page_state(z, NR_ZONE_INACTIVE_ANON) +
			zone_page_state(z, NR_ZONE_ACTIVE_ANON) +
			zone_page_state(z, NR_ZONE_INACTIVE_FILE) +
			zone_page_state(z, NR_ZONE_ACTIVE_FILE);
		available += minus;
	}

	pr_info("%s: count(%lu) free(%lu) available(%lu) minus(%lu)\n",
			__func__, count, free, available, minus);

	/*
	 * Could "minus" be put into remaining area?
	 * If not, check lower zones' memory status.
	 */
	if (available > count && (minus <= (available - count)))
		return true;

	/* Remaining needed size */
	if (count > free)
		count -= free;

	/* If we need more lower zones' space... */
	return system_mem_status_ok(count);
}

struct page *zmc_cma_alloc(struct cma *cma, int count,
		unsigned int align, struct single_cma_registration *p)
{
#ifdef CONFIG_ARCH_MT6757
	struct page *candidate, *abandon = NULL;
#endif

	/* Check current memory status before proceeding */
	if (p->prio >= ZMC_CHECK_MEM_STAT && !zmc_check_mem_status_ok(count)) {
		pr_info("%s: mem status is not ok\n", __func__);
		return NULL;
	}

	zmc_notifier_call_chain(ZMC_EVENT_ALLOC_MOVABLE, NULL);

	if (!zmc_reserved_mem_inited)
		return cma_alloc(cma, count, align);

	/*
	 * Pre-check with cma bitmap. If there is no enough
	 * memory in zone movable cma, provide error handling
	 * for memory reclaim or abort cma_alloc.
	 */
	if (!cma_alloc_range_ok(cma, count, align)) {
		pr_info("No more space in zone movable cma\n");
		return NULL;
	}

#ifdef CONFIG_ARCH_MT6757
#define ABANDON_PFN	(0xc0000)
retry:
	candidate = cma_alloc(cma, count, align);

	if (abandon != NULL)
		cma_release(cma, abandon, count);

	if (p->prio == ZMC_SSMR &&
			candidate != NULL &&
			page_to_pfn(candidate) == ABANDON_PFN) {
		abandon = candidate;
		pr_info("%s %p is abandoned\n", __func__, candidate);
		goto retry;
	}

	return candidate;
#else
	return cma_alloc(cma, count, align);
#endif
}

bool zmc_cma_release(struct cma *cma, struct page *pages, int count)
{
	if (!zmc_reserved_mem_inited)
		return cma_release(cma, pages, count);

	return cma_release(cma, pages, count);
}

static int __init fix_up_normal_zone(void)
{
	struct pglist_data *pgdat;
	unsigned long flags;

	for_each_online_pgdat(pgdat) {
		struct zone *z;
		unsigned long zone_start_pfn;
		unsigned long zone_end_pfn;
		unsigned long pfn;
		struct page *page;
		int pages = 0;

		z = pgdat->node_zones + OPT_ZONE_MOVABLE_CMA;
		zone_start_pfn = z->zone_start_pfn;
		zone_end_pfn = zone_start_pfn + z->present_pages;
		spin_lock_irqsave(&z->lock, flags);
		for (pfn = zone_start_pfn;
				pfn < zone_end_pfn;
				pfn += pageblock_nr_pages) {
			if (!pfn_valid(pfn))
				continue;

			page = pfn_to_page(pfn);
			if (is_migrate_cma_page(page))
				continue;

			set_pageblock_migratetype(page, MIGRATE_CMA);
			pages += move_freepages_block(z, page, MIGRATE_CMA);
		}
		__mod_zone_page_state(z, NR_FREE_CMA_PAGES, pages);
		spin_unlock_irqrestore(&z->lock, flags);
	}
	return 0;
}
core_initcall(fix_up_normal_zone);

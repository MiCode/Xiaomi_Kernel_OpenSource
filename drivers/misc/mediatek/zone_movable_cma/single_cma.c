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
#define pr_fmt(fmt) "zone_movable_cma: " fmt
#define CONFIG_MTK_ZONE_MOVABLE_CMA_DEBUG

#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/printk.h>
#include <linux/memblock.h>
#include <linux/mm.h>

#include "mt-plat/mtk_meminfo.h"
#include "single_cma.h"

struct cma *cma;
static unsigned long shrunk_size;
bool zmc_reserved_mem_inited = false;

static struct single_cma_registration *single_cma_list[] = {
#ifdef CONFIG_MTK_MEMORY_LOWPOWER
	&memory_lowpower_registration,
#endif
#ifdef CONFIG_MTK_SVP
	&memory_ssvp_registration,
#endif
};

void zmc_check_registions(void)
{
	const int entries = ARRAY_SIZE(single_cma_list);
	int i;
	struct single_cma_registration *regist;

	for (i = 0; i < entries; i++) {
		regist = single_cma_list[i];

		if (regist->size > cma_get_size(cma)) {
			if (regist->flag & ZMC_ALLOC_ALL) {
				pr_info("[ZMC_ALLOC_ALL]: shrink size to cma_get_size: 0x%lx\n", cma_get_size(cma));
				regist->size = cma_get_size(cma);
			} else {
				pr_info("[%s] reserve fail due to large size without ZMC_ALLOC_ALL\n", regist->name);
				regist->reserve_fail = true;
			}
		}
	}
}

phys_addr_t zmc_shrink_cma_range(void)
{
	int i;
	const int entries = ARRAY_SIZE(single_cma_list);
	struct single_cma_registration *regist;
	phys_addr_t alignment;
	phys_addr_t max = 0;
	phys_addr_t max_align = 0;

	for (i = 0; i < entries; i++) {
		regist = single_cma_list[i];

		pr_info("[%s]: size:%lx, align:%lx, flag:0x%lx\n", regist->name,
				(unsigned long)regist->size,
				(unsigned long)regist->align,
				regist->flag);

		if (regist->reserve_fail)
			continue;

		if (regist->size > max)
			max = regist->size;

		if (regist->align > max_align)
			max_align = regist->align;
	}

	/* ensure minimal alignment requied by mm core */
	alignment = PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order);

	alignment = max(alignment, max_align);

	if (cma_get_size(cma) > max) {
		phys_addr_t new_base;

		/* Check new_base */
		new_base = ALIGN(cma_get_base(cma) + cma_get_size(cma) - max, alignment);
		if (new_base < cma_get_base(cma)) {
			pr_warn("%s: mismatched base(0x%lx) new_base(%p)\n",
					__func__, (unsigned long)(cma_get_base(cma)), &new_base);
			goto orig;
		}

		/* Compute reasonable shrunk_size & new size */
		shrunk_size = new_base - cma_get_base(cma);
		max = cma_get_size(cma) - shrunk_size;

		pr_info("[Resize-START] ZONE_MOVABLE to size: %pa\n", &max);
		cma_resize_front(cma, shrunk_size >> PAGE_SHIFT);
		pr_info("[Resize-DONE]  ZONE_MOVABLE [0x%lx:0x%lx]\n",
				(unsigned long)(cma_get_base(cma)),
				(unsigned long)(cma_get_base(cma) + cma_get_size(cma)));
	}

orig:
	return cma_get_base(cma);
}

static int zmc_memory_init(struct reserved_mem *rmem)
{
	int ret;
	int nr_registed = ARRAY_SIZE(single_cma_list);
	int i;

	pr_alert("%s, name: %s, base: %pa, size: %pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	if (nr_registed == 0) {
		pr_alert("[FREE ALL CMA MEMORY]: No registed zmc nodes.");
		memblock_free(rmem->base, rmem->size);
		return 0;
	}
	/* init cma area */
	ret = cma_init_reserved_mem(rmem->base, rmem->size , 0, &cma);

	if (ret) {
		pr_err("%s cma failed, ret: %d\n", __func__, ret);
		return 1;
	}
	zmc_check_registions();

	zmc_reserved_mem_inited = true;

	zmc_shrink_cma_range();

	for (i = 0; i < nr_registed; i++) {
		if (single_cma_list[i]->init &&
				!single_cma_list[i]->reserve_fail)
			single_cma_list[i]->init(cma);
	}
	return 0;
}
RESERVEDMEM_OF_DECLARE(zone_movable_cma_init, "mediatek,zone_movable_cma",
			zmc_memory_init);

struct page *zmc_cma_alloc(struct cma *cma, int count, unsigned int align, struct single_cma_registration *p)
{
#ifdef CONFIG_ARCH_MT6757
	struct page *candidate, *abandon = NULL;
#endif

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

	if (p->prio == ZMC_SSVP &&
			candidate != NULL && page_to_pfn(candidate) == ABANDON_PFN) {
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

static int zmc_free_pageblock(phys_addr_t base, phys_addr_t size)
{
	unsigned long base_pfn = __phys_to_pfn(base);
	unsigned long pfn = base_pfn;
	unsigned nr_to_free_pageblock = size >> pageblock_order;
	struct zone *zone;
	int i;

	zone = page_zone(pfn_to_page(pfn));
	for (i = 0; i < nr_to_free_pageblock; i++) {
		unsigned j;

		base_pfn = pfn;
		for (j = pageblock_nr_pages; j; --j, pfn++) {
			WARN_ON_ONCE(!pfn_valid(pfn));
			/*
			 * alloc_contig_range requires the pfn range
			 * specified to be in the same zone. Make this
			 * simple by forcing the entire CMA resv range
			 * to be in the same zone.
			 */
			if (page_zone(pfn_to_page(pfn)) != zone)
				goto err;
		}
		free_cma_reserved_pageblock(pfn_to_page(base_pfn));
	}
	if (i > 0)
		pr_info("resize ZONE_MOVABLE done!\n");

	return 0;

err:
	return -EINVAL;
}

static int zmc_free_areas(void)
{
	phys_addr_t base = cma_get_base(cma) - shrunk_size;

	pr_info("Raw input: base:0x%pa, shrunk_size:0x%pa\n", &base, &shrunk_size);
	return zmc_free_pageblock(base, shrunk_size);
}

static int __init zmc_resize(void)
{
	if (!zmc_reserved_mem_inited) {
		pr_alert("uninited cma, start zone_movable_cma fail!\n");
		return -1;
	}

	return zmc_free_areas();
}
core_initcall(zmc_resize);

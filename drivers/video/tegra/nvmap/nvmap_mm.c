/*
 * drivers/video/tegra/nvmap/nvmap_mm.c
 *
 * Some MM related functionality specific to nvmap.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "nvmap_priv.h"

void inner_flush_cache_all(void)
{
#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS_ON_ONE_CPU
	v7_flush_kern_cache_all();
#else
	on_each_cpu(v7_flush_kern_cache_all, NULL, 1);
#endif
}

void inner_clean_cache_all(void)
{
#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS_ON_ONE_CPU
	v7_clean_kern_cache_all(NULL);
#else
	on_each_cpu(v7_clean_kern_cache_all, NULL, 1);
#endif
}

#ifndef CONFIG_NVMAP_CPA
static void flush_cache(struct page **pages, int numpages)
{
	unsigned int i;
	bool flush_inner = true;
	unsigned long base;

#if defined(CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS)
	if (numpages >= (cache_maint_inner_threshold >> PAGE_SHIFT)) {
		inner_flush_cache_all();
		flush_inner = false;
	}
#endif

	for (i = 0; i < numpages; i++) {
		if (flush_inner)
			__flush_dcache_page(page_mapping(pages[i]), pages[i]);
		base = page_to_phys(pages[i]);
		outer_flush_range(base, base + PAGE_SIZE);
	}
}
#endif

int nvmap_set_pages_array_uc(struct page **pages, int addrinarray)
{
#ifdef CONFIG_NVMAP_CPA
	return set_pages_array_uc(pages, addrinarray);
#else
	flush_cache(pages, addrinarray);
	return 0;
#endif
}

int nvmap_set_pages_array_wc(struct page **pages, int addrinarray)
{
#ifdef CONFIG_NVMAP_CPA
	return set_pages_array_wc(pages, addrinarray);
#else
	flush_cache(pages, addrinarray);
	return 0;
#endif
}

int nvmap_set_pages_array_iwb(struct page **pages, int addrinarray)
{
#ifdef CONFIG_NVMAP_CPA
	return set_pages_array_iwb(pages, addrinarray);
#else
	flush_cache(pages, addrinarray);
	return 0;
#endif
}

int nvmap_set_pages_array_wb(struct page **pages, int addrinarray)
{
#ifdef CONFIG_NVMAP_CPA
	return set_pages_array_wb(pages, addrinarray);
#else
	return 0;
#endif
}

/*
 * Perform cache op on the list of memory regions within passed handles.
 * A memory region within handle[i] is identified by offsets[i], sizes[i]
 *
 * sizes[i] == 0  is a special case which causes handle wide operation,
 * this is done by replacing offsets[i] = 0, sizes[i] = handles[i]->size.
 * So, the input arrays sizes, offsets  are not guaranteed to be read-only
 *
 * This will optimze the op if it can.
 * In the case that all the handles together are larger than the inner cache
 * maint threshold it is possible to just do an entire inner cache flush.
 */
int nvmap_do_cache_maint_list(struct nvmap_handle **handles, u32 *offsets,
			      u32 *sizes, int op, int nr)
{
	int i;
	u64 total = 0;

	for (i = 0; i < nr; i++)
		total += sizes[i] ? sizes[i] : handles[i]->size;

	/* Full flush in the case the passed list is bigger than our
	 * threshold. */
	if (total >= cache_maint_inner_threshold) {
		inner_flush_cache_all();
	} else {
		for (i = 0; i < nr; i++) {
			u32 size = sizes[i] ? sizes[i] : handles[i]->size;
			u32 offset = sizes[i] ? offsets[i] : 0;
			int err = __nvmap_do_cache_maint(handles[i]->owner,
							 handles[i], offset,
							 offset + size,
							 op, false);
			if (err)
				return err;
		}
	}

	return 0;
}

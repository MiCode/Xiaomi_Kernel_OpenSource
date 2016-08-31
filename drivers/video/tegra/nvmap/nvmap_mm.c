/*
 * drivers/video/tegra/nvmap/nvmap_mm.c
 *
 * Some MM related functionality specific to nvmap.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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

/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/version.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_pool.h"

/**
 * struct kgsl_page_pool - Structure to hold information for the pool
 * @pool_order: Page order describing the size of the page
 * @page_count: Number of pages currently present in the pool
 * @reserved_pages: Number of pages reserved at init for the pool
 * @allocation_allowed: Tells if reserved pool gets exhausted, can we allocate
 * from system memory
 * @list_lock: Spinlock for page list in the pool
 * @page_list: List of pages held/reserved in this pool
 */
struct kgsl_page_pool {
	unsigned int pool_order;
	int page_count;
	unsigned int reserved_pages;
	bool allocation_allowed;
	spinlock_t list_lock;
	struct list_head page_list;
};

static struct kgsl_page_pool kgsl_pools[] = {
	{
		.pool_order = 0,
		.reserved_pages = 2048,
		.allocation_allowed = true,
		.list_lock = __SPIN_LOCK_UNLOCKED(kgsl_pools[0].list_lock),
		.page_list = LIST_HEAD_INIT(kgsl_pools[0].page_list),
	},
#ifndef CONFIG_ALLOC_BUFFERS_IN_4K_CHUNKS
	{
		.pool_order = 1,
		.reserved_pages = 1024,
		.allocation_allowed = true,
		.list_lock = __SPIN_LOCK_UNLOCKED(kgsl_pools[1].list_lock),
		.page_list = LIST_HEAD_INIT(kgsl_pools[1].page_list),
	},
	{
		.pool_order = 4,
		.reserved_pages = 256,
		.allocation_allowed = false,
		.list_lock = __SPIN_LOCK_UNLOCKED(kgsl_pools[2].list_lock),
		.page_list = LIST_HEAD_INIT(kgsl_pools[2].page_list),
	},
	{
		.pool_order = 8,
		.reserved_pages = 32,
		.allocation_allowed = false,
		.list_lock = __SPIN_LOCK_UNLOCKED(kgsl_pools[3].list_lock),
		.page_list = LIST_HEAD_INIT(kgsl_pools[3].page_list),
	},

#endif
};

#define KGSL_NUM_POOLS ARRAY_SIZE(kgsl_pools)

/* Returns KGSL pool corresponding to input page order*/
static struct kgsl_page_pool *
_kgsl_get_pool_from_order(unsigned int order)
{
	int i;

	for (i = 0; i < KGSL_NUM_POOLS; i++) {
		if (kgsl_pools[i].pool_order == order)
			return &kgsl_pools[i];
	}

	return NULL;
}

/* Map the page into kernel and zero it out */
static void
_kgsl_pool_zero_page(struct page *p, unsigned int pool_order)
{
	int i;

	for (i = 0; i < (1 << pool_order); i++) {
		struct page *page = nth_page(p, i);
		void *addr = kmap_atomic(page);

		memset(addr, 0, PAGE_SIZE);
		dmac_flush_range(addr, addr + PAGE_SIZE);
		kunmap_atomic(addr);
	}
}

/* Add a page to specified pool */
static void
_kgsl_pool_add_page(struct kgsl_page_pool *pool, struct page *p)
{
	_kgsl_pool_zero_page(p, pool->pool_order);

	spin_lock(&pool->list_lock);
	list_add_tail(&p->lru, &pool->page_list);
	pool->page_count++;
	spin_unlock(&pool->list_lock);
}

/* Returns a page from specified pool */
static struct page *
_kgsl_pool_get_page(struct kgsl_page_pool *pool)
{
	struct page *p = NULL;

	spin_lock(&pool->list_lock);
	if (pool->page_count) {
		p = list_first_entry(&pool->page_list, struct page, lru);
		pool->page_count--;
		list_del(&p->lru);
	}
	spin_unlock(&pool->list_lock);

	return p;
}

/* Returns the number of pages in specified pool */
static int
kgsl_pool_size(struct kgsl_page_pool *kgsl_pool)
{
	int size;

	spin_lock(&kgsl_pool->list_lock);
	size = kgsl_pool->page_count * (1 << kgsl_pool->pool_order);
	spin_unlock(&kgsl_pool->list_lock);

	return size;
}

/* Returns the number of pages in all kgsl page pools */
static int kgsl_pool_size_total(void)
{
	int i;
	int total = 0;

	for (i = 0; i < KGSL_NUM_POOLS; i++)
		total += kgsl_pool_size(&kgsl_pools[i]);
	return total;
}

/*
 * This will shrink the specified pool by num_pages or its pool_size,
 * whichever is smaller.
 */
static unsigned int
_kgsl_pool_shrink(struct kgsl_page_pool *pool, int num_pages)
{
	int j;
	unsigned int pcount = 0;

	if (pool == NULL || num_pages <= 0)
		return pcount;

	for (j = 0; j < num_pages >> pool->pool_order; j++) {
		struct page *page = _kgsl_pool_get_page(pool);

		if (page != NULL) {
			__free_pages(page, pool->pool_order);
			pcount += (1 << pool->pool_order);
		} else {
			/* Break as this pool is empty */
			break;
		}
	}

	return pcount;
}

/*
 * This function reduces the total pool size
 * to number of pages specified by target_pages.
 *
 * If target_pages are greater than current pool size
 * nothing needs to be done otherwise remove
 * (current_pool_size - target_pages) pages from pool
 * starting from higher order pool.
 */
static unsigned long
kgsl_pool_reduce(unsigned int target_pages, bool exit)
{
	int total_pages = 0;
	int i;
	int nr_removed;
	struct kgsl_page_pool *pool;
	unsigned long pcount = 0;

	total_pages = kgsl_pool_size_total();

	for (i = (KGSL_NUM_POOLS - 1); i >= 0; i--) {
		pool = &kgsl_pools[i];

		/*
		 * Only reduce the pool sizes for pools which are allowed to
		 * allocate memory unless we are at close, in which case the
		 * reserved memory for all pools needs to be freed
		 */
		if (!pool->allocation_allowed && !exit)
			continue;

		total_pages -= pcount;

		nr_removed = total_pages - target_pages;
		if (nr_removed <= 0)
			return pcount;

		/* Round up to integral number of pages in this pool */
		nr_removed = ALIGN(nr_removed, 1 << pool->pool_order);

		/* Remove nr_removed pages from this pool*/
		pcount += _kgsl_pool_shrink(pool, nr_removed);
	}

	return pcount;
}

/**
 * kgsl_pool_free_sgt() - Free scatter-gather list
 * @sgt: pointer of the sg list
 *
 * Free the sg list by collapsing any physical adjacent pages.
 * Pages are added back to the pool, if pool has sufficient space
 * otherwise they are given back to system.
 */

void kgsl_pool_free_sgt(struct sg_table *sgt)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		/*
		 * sg_alloc_table_from_pages() will collapse any physically
		 * adjacent pages into a single scatterlist entry. We cannot
		 * just call __free_pages() on the entire set since we cannot
		 * ensure that the size is a whole order. Instead, free each
		 * page or compound page group individually.
		 */
		struct page *p = sg_page(sg), *next;
		unsigned int count;
		unsigned int j = 0;

		while (j < (sg->length/PAGE_SIZE)) {
			count = 1 << compound_order(p);
			next = nth_page(p, count);
			kgsl_pool_free_page(p);

			p = next;
			j += count;
		}
	}
}

/**
 * kgsl_pool_free_pages() - Free pages in the pages array
 * @pages: pointer of the pages array
 *
 * Free the pages by collapsing any physical adjacent pages.
 * Pages are added back to the pool, if pool has sufficient space
 * otherwise they are given back to system.
 */
void kgsl_pool_free_pages(struct page **pages, unsigned int pcount)
{
	int i;

	if (pages == NULL || pcount == 0)
		return;

	for (i = 0; i < pcount;) {
		/*
		 * Free each page or compound page group individually.
		 */
		struct page *p = pages[i];

		i += 1 << compound_order(p);
		kgsl_pool_free_page(p);
	}
}
static int kgsl_pool_idx_lookup(unsigned int order)
{
	int i;

	for (i = 0; i < KGSL_NUM_POOLS; i++)
		if (order == kgsl_pools[i].pool_order)
			return i;

	return -ENOMEM;
}

/**
 * kgsl_pool_alloc_page() - Allocate a page of requested size
 * @page_size: Size of the page to be allocated
 * @pages: pointer to hold list of pages, should be big enough to hold
 * requested page
 * @len: Length of array pages.
 *
 * Return total page count on success and negative value on failure
 */
int kgsl_pool_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align)
{
	int j;
	int pcount = 0;
	struct kgsl_page_pool *pool;
	struct page *page = NULL;
	struct page *p = NULL;
	int order = get_order(*page_size);
	int pool_idx;

	if ((pages == NULL) || pages_len < (*page_size >> PAGE_SHIFT))
		return -EINVAL;

	pool = _kgsl_get_pool_from_order(order);
	if (pool == NULL)
		return -EINVAL;

	pool_idx = kgsl_pool_idx_lookup(order);
	page = _kgsl_pool_get_page(pool);

	/* Allocate a new page if not allocated from pool */
	if (page == NULL) {
		gfp_t gfp_mask = kgsl_gfp_mask(order);

		/* Only allocate non-reserved memory for certain pools */
		if (!pool->allocation_allowed && pool_idx > 0) {
			*page_size = PAGE_SIZE <<
					kgsl_pools[pool_idx-1].pool_order;
			*align = ilog2(*page_size);
			return -EAGAIN;
		}

		page = alloc_pages(gfp_mask, order);

		if (!page) {
			if (pool_idx > 0) {
				/* Retry with lower order pages */
				*page_size = PAGE_SIZE <<
					kgsl_pools[pool_idx-1].pool_order;
				*align = ilog2(*page_size);
				return -EAGAIN;
			} else
				return -ENOMEM;
		}

		_kgsl_pool_zero_page(page, order);
	}

	for (j = 0; j < (*page_size >> PAGE_SHIFT); j++) {
		p = nth_page(page, j);
		pages[pcount] = p;
		pcount++;
	}

	return pcount;
}

void kgsl_pool_free_page(struct page *page)
{
	struct kgsl_page_pool *pool;
	int page_order;

	if (page == NULL)
		return;

	page_order = compound_order(page);

	pool = _kgsl_get_pool_from_order(page_order);
	if (pool != NULL) {
		_kgsl_pool_add_page(pool, page);
		return;
	}

	/* Give back to system as not added to pool */
	__free_pages(page, page_order);
}

static void kgsl_pool_reserve_pages(void)
{
	int i, j;

	for (i = 0; i < KGSL_NUM_POOLS; i++) {
		struct page *page;

		for (j = 0; j < kgsl_pools[i].reserved_pages; j++) {
			int order = kgsl_pools[i].pool_order;
			gfp_t gfp_mask = kgsl_gfp_mask(order);

			page = alloc_pages(gfp_mask, order);
			if (page != NULL)
				_kgsl_pool_add_page(&kgsl_pools[i], page);
		}
	}
}

/* Functions for the shrinker */

static unsigned long
kgsl_pool_shrink_scan_objects(struct shrinker *shrinker,
					struct shrink_control *sc)
{
	/* nr represents number of pages to be removed*/
	int nr = sc->nr_to_scan;
	int total_pages = kgsl_pool_size_total();

	/* Target pages represents new  pool size */
	int target_pages = (nr > total_pages) ? 0 : (total_pages - nr);

	/* Reduce pool size to target_pages */
	return kgsl_pool_reduce(target_pages, false);
}

static unsigned long
kgsl_pool_shrink_count_objects(struct shrinker *shrinker,
					struct shrink_control *sc)
{
	/* Return total pool size as everything in pool can be freed */
	return kgsl_pool_size_total();
}

/* Shrinker callback data*/
static struct shrinker kgsl_pool_shrinker = {
	.count_objects = kgsl_pool_shrink_count_objects,
	.scan_objects = kgsl_pool_shrink_scan_objects,
	.seeks = DEFAULT_SEEKS,
	.batch = 0,
};

void kgsl_init_page_pools(void)
{
	/* Reserve the appropriate number of pages for each pool */
	kgsl_pool_reserve_pages();

	/* Initialize shrinker */
	register_shrinker(&kgsl_pool_shrinker);
}

void kgsl_exit_page_pools(void)
{
	/* Release all pages in pools, if any.*/
	kgsl_pool_reduce(0, true);

	/* Unregister shrinker */
	unregister_shrinker(&kgsl_pool_shrinker);
}


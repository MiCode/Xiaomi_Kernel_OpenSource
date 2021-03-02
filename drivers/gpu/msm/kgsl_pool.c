// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/highmem.h>
#include <linux/of.h>
#include <linux/scatterlist.h>

#include "kgsl_device.h"
#include "kgsl_pool.h"
#include "kgsl_sharedmem.h"

/**
 * struct kgsl_page_pool - Structure to hold information for the pool
 * @pool_order: Page order describing the size of the page
 * @page_count: Number of pages currently present in the pool
 * @allocation_allowed: Tells if reserved pool gets exhausted, can we allocate
 * from system memory
 * @list_lock: Spinlock for page list in the pool
 * @page_list: List of pages held/reserved in this pool
 */
struct kgsl_page_pool {
	unsigned int pool_order;
	int page_count;
	bool allocation_allowed;
	spinlock_t list_lock;
	struct list_head page_list;
};

static struct kgsl_page_pool kgsl_pools[4];
static int kgsl_num_pools;
static int kgsl_pool_max_pages;

static void kgsl_pool_free_page(struct page *page);

/* Return the index of the pool for the specified order */
static int kgsl_get_pool_index(int order)
{
	int i;

	for (i = 0; i < kgsl_num_pools; i++) {
		if (kgsl_pools[i].pool_order == order)
			return i;
	}

	return -EINVAL;
}

/* Returns KGSL pool corresponding to input page order*/
static struct kgsl_page_pool *
_kgsl_get_pool_from_order(int order)
{
	int index = kgsl_get_pool_index(order);

	return index >= 0 ? &kgsl_pools[index] : NULL;
}

static void kgsl_pool_sync_for_device(struct device *dev, struct page *page,
		size_t size)
{
	struct scatterlist sg;

	/* The caller may choose not to specify a device on purpose */
	if (!dev)
		return;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, size, 0);
	sg_dma_address(&sg) = page_to_phys(page);

	dma_sync_sg_for_device(dev, &sg, 1, DMA_BIDIRECTIONAL);
}

/* Map the page into kernel and zero it out */
static void
_kgsl_pool_zero_page(struct page *p, unsigned int pool_order,
		struct device *dev)
{
	int i;

	for (i = 0; i < (1 << pool_order); i++) {
		struct page *page = nth_page(p, i);

		clear_highpage(page);
	}

	kgsl_pool_sync_for_device(dev, p, PAGE_SIZE << pool_order);
}

/* Add a page to specified pool */
static void
_kgsl_pool_add_page(struct kgsl_page_pool *pool, struct page *p)
{
	if (!p)
		return;

	/*
	 * Sanity check to make sure we don't re-pool a page that
	 * somebody else has a reference to.
	 */
	if (WARN_ON(unlikely(page_count(p) > 1))) {
		__free_pages(p, pool->pool_order);
		return;
	}

	spin_lock(&pool->list_lock);
	list_add_tail(&p->lru, &pool->page_list);
	pool->page_count++;
	spin_unlock(&pool->list_lock);
	mod_node_page_state(page_pgdat(p),  NR_KERNEL_MISC_RECLAIMABLE,
				(1 << pool->pool_order));
}

/* Returns a page from specified pool */
static struct page *
_kgsl_pool_get_page(struct kgsl_page_pool *pool)
{
	struct page *p = NULL;

	spin_lock(&pool->list_lock);

	p = list_first_entry_or_null(&pool->page_list, struct page, lru);
	if (p == NULL) {
		spin_unlock(&pool->list_lock);
		return NULL;
	}
	pool->page_count--;
	list_del(&p->lru);
	spin_unlock(&pool->list_lock);
	mod_node_page_state(page_pgdat(p), NR_KERNEL_MISC_RECLAIMABLE,
				-(1 << pool->pool_order));
	return p;
}

/* Returns the number of pages in all kgsl page pools */
static int kgsl_pool_size_total(void)
{
	int i;
	int total = 0;

	for (i = 0; i < kgsl_num_pools; i++) {
		struct kgsl_page_pool *kgsl_pool = &kgsl_pools[i];

		spin_lock(&kgsl_pool->list_lock);
		total += kgsl_pool->page_count * (1 << kgsl_pool->pool_order);
		spin_unlock(&kgsl_pool->list_lock);
	}

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

		if (!page)
			break;

		__free_pages(page, pool->pool_order);
		pcount += (1 << pool->pool_order);
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

	for (i = (kgsl_num_pools - 1); i >= 0; i--) {
		pool = &kgsl_pools[i];

		/*
		 * Only reduce the pool sizes for pools which are allowed to
		 * allocate memory unless we are at close, in which case the
		 * reserved memory for all pools needs to be freed
		 */
		if (!pool->allocation_allowed && !exit)
			continue;

		nr_removed = total_pages - target_pages - pcount;
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

void kgsl_pool_free_pages(struct page **pages, unsigned int pcount)
{
	int i;

	if (!pages)
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

static int kgsl_pool_get_retry_order(unsigned int order)
{
	int i;

	for (i = kgsl_num_pools-1; i > 0; i--)
		if (order >= kgsl_pools[i].pool_order)
			return kgsl_pools[i].pool_order;

	return 0;
}

static gfp_t kgsl_gfp_mask(int page_order)
{
	gfp_t gfp_mask = __GFP_HIGHMEM;

	if (page_order > 0) {
		gfp_mask |= __GFP_COMP | __GFP_NORETRY | __GFP_NOWARN;
		gfp_mask &= ~__GFP_RECLAIM;
	} else
		gfp_mask |= GFP_KERNEL;

	if (kgsl_sharedmem_get_noretry())
		gfp_mask |= __GFP_NORETRY | __GFP_NOWARN;

	return gfp_mask;
}

/*
 * Return true if the pool of specified page size is supported
 * or no pools are supported otherwise return false.
 */
static bool kgsl_pool_available(unsigned int page_size)
{
	int order = ilog2(page_size >> PAGE_SHIFT);

	if (!kgsl_num_pools)
		return true;

	return (kgsl_get_pool_index(order) >= 0);
}

static int kgsl_get_page_size(size_t size, unsigned int align)
{
	if (align >= ilog2(SZ_1M) && size >= SZ_1M &&
		kgsl_pool_available(SZ_1M))
		return SZ_1M;
	else if (align >= ilog2(SZ_64K) && size >= SZ_64K &&
		kgsl_pool_available(SZ_64K))
		return SZ_64K;
	else if (align >= ilog2(SZ_8K) && size >= SZ_8K &&
		kgsl_pool_available(SZ_8K))
		return SZ_8K;

	return PAGE_SIZE;
}

/*
 * kgsl_pool_alloc_page() - Allocate a page of requested size
 * @page_size: Size of the page to be allocated
 * @pages: pointer to hold list of pages, should be big enough to hold
 * requested page
 * @len: Length of array pages.
 *
 * Return total page count on success and negative value on failure
 */
static int kgsl_pool_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align,
			struct device *dev)
{
	int j;
	int pcount = 0;
	struct kgsl_page_pool *pool;
	struct page *page = NULL;
	struct page *p = NULL;
	int order = get_order(*page_size);
	int pool_idx;
	size_t size = 0;

	if ((pages == NULL) || pages_len < (*page_size >> PAGE_SHIFT))
		return -EINVAL;

	/* If the pool is not configured get pages from the system */
	if (!kgsl_num_pools) {
		gfp_t gfp_mask = kgsl_gfp_mask(order);

		page = alloc_pages(gfp_mask, order);
		if (page == NULL) {
			/* Retry with lower order pages */
			if (order > 0) {
				size = PAGE_SIZE << --order;
				goto eagain;

			} else
				return -ENOMEM;
		}
		goto done;
	}

	pool = _kgsl_get_pool_from_order(order);
	if (pool == NULL) {
		/* Retry with lower order pages */
		if (order > 0) {
			size = PAGE_SIZE << kgsl_pool_get_retry_order(order);
			goto eagain;
		} else {
			/*
			 * Fall back to direct allocation in case
			 * pool with zero order is not present
			 */
			gfp_t gfp_mask = kgsl_gfp_mask(order);

			page = alloc_pages(gfp_mask, order);
			if (page == NULL)
				return -ENOMEM;
			goto done;
		}
	}

	pool_idx = kgsl_get_pool_index(order);
	page = _kgsl_pool_get_page(pool);

	/* Allocate a new page if not allocated from pool */
	if (page == NULL) {
		gfp_t gfp_mask = kgsl_gfp_mask(order);

		/* Only allocate non-reserved memory for certain pools */
		if (!pool->allocation_allowed && pool_idx > 0) {
			size = PAGE_SIZE <<
					kgsl_pools[pool_idx-1].pool_order;
			goto eagain;
		}

		page = alloc_pages(gfp_mask, order);

		if (!page) {
			if (pool_idx > 0) {
				/* Retry with lower order pages */
				size = PAGE_SIZE <<
					kgsl_pools[pool_idx-1].pool_order;
				goto eagain;
			} else
				return -ENOMEM;
		}
	}

done:
	_kgsl_pool_zero_page(page, order, dev);

	for (j = 0; j < (*page_size >> PAGE_SHIFT); j++) {
		p = nth_page(page, j);
		pages[pcount] = p;
		pcount++;
	}

	return pcount;

eagain:
	*page_size = kgsl_get_page_size(size, ilog2(size));
	*align = ilog2(*page_size);
	return -EAGAIN;
}

int kgsl_pool_alloc_pages(u64 size, struct page ***pages, struct device *dev)
{
	int count = 0;
	int npages = size >> PAGE_SHIFT;
	struct page **local = kvcalloc(npages, sizeof(*local), GFP_KERNEL);
	u32 page_size, align;
	u64 len = size;

	if (!local)
		return -ENOMEM;

	/* Start with 1MB alignment to get the biggest page we can */
	align = ilog2(SZ_1M);

	page_size = kgsl_get_page_size(len, align);

	while (len) {
		int ret = kgsl_pool_alloc_page(&page_size, &local[count],
			npages, &align, dev);

		if (ret == -EAGAIN)
			continue;
		else if (ret <= 0) {
			int i;

			for (i = 0; i < count; ) {
				int n = 1 << compound_order(local[i]);

				kgsl_pool_free_page(local[i]);
				i += n;
			}
			kvfree(local);

			if (!kgsl_sharedmem_get_noretry())
				pr_err_ratelimited("kgsl: out of memory: only allocated %lldKb of %lldKb requested\n",
					(size - len) >> 10, size >> 10);

			return -ENOMEM;
		}

		count += ret;
		npages -= ret;
		len -= page_size;

		page_size = kgsl_get_page_size(len, align);
	}

	*pages = local;

	return count;
}

static void kgsl_pool_free_page(struct page *page)
{
	struct kgsl_page_pool *pool;
	int page_order;

	if (page == NULL)
		return;

	page_order = compound_order(page);

	if (!kgsl_pool_max_pages ||
			(kgsl_pool_size_total() < kgsl_pool_max_pages)) {
		pool = _kgsl_get_pool_from_order(page_order);
		if (pool != NULL) {
			_kgsl_pool_add_page(pool, page);
			return;
		}
	}

	/* Give back to system as not added to pool */
	__free_pages(page, page_order);
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

static void kgsl_pool_reserve_pages(struct kgsl_page_pool *pool,
		struct device_node *node)
{
	u32 reserved = 0;
	int i;

	of_property_read_u32(node, "qcom,mempool-reserved", &reserved);

	/* Limit the total number of reserved pages to 4096 */
	reserved = min_t(u32, reserved, 4096);

	for (i = 0; i < reserved; i++) {
		gfp_t gfp_mask = kgsl_gfp_mask(pool->pool_order);
		struct page *page;

		page = alloc_pages(gfp_mask, pool->pool_order);
		_kgsl_pool_add_page(pool, page);
	}
}

static int kgsl_of_parse_mempool(struct kgsl_page_pool *pool,
		struct device_node *node)
{
	u32 size;
	int order;

	if (of_property_read_u32(node, "qcom,mempool-page-size", &size))
		return -EINVAL;

	order = ilog2(size >> PAGE_SHIFT);

	if (order > 8) {
		pr_err("kgsl: %pOF: pool order %d is too big\n", node, order);
		return -EINVAL;
	}

	pool->pool_order = order;
	pool->allocation_allowed = of_property_read_bool(node,
		"qcom,mempool-allocate");

	spin_lock_init(&pool->list_lock);
	INIT_LIST_HEAD(&pool->page_list);

	kgsl_pool_reserve_pages(pool, node);

	return 0;
}

void kgsl_probe_page_pools(void)
{
	struct device_node *node, *child;
	int index = 0;

	node = of_find_compatible_node(NULL, NULL, "qcom,gpu-mempools");
	if (!node)
		return;

	/* Get Max pages limit for mempool */
	of_property_read_u32(node, "qcom,mempool-max-pages",
			&kgsl_pool_max_pages);

	for_each_child_of_node(node, child) {
		if (!kgsl_of_parse_mempool(&kgsl_pools[index], child))
			index++;

		if (index == ARRAY_SIZE(kgsl_pools)) {
			of_node_put(child);
			break;
		}
	}

	kgsl_num_pools = index;
	of_node_put(node);

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


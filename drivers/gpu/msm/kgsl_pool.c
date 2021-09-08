// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/debugfs.h>
#include <linux/highmem.h>
#include <linux/mempool.h>
#include <linux/of.h>
#include <linux/scatterlist.h>

#include "kgsl_device.h"
#include "kgsl_pool.h"
#include "kgsl_sharedmem.h"
#include "kgsl_trace.h"

#ifdef CONFIG_QCOM_KGSL_SORT_POOL

struct kgsl_pool_page_entry {
	phys_addr_t physaddr;
	struct page *page;
	struct rb_node node;
};

static struct kmem_cache *addr_page_cache;

/**
 * struct kgsl_page_pool - Structure to hold information for the pool
 * @pool_order: Page order describing the size of the page
 * @page_count: Number of pages currently present in the pool
 * @reserved_pages: Number of pages reserved at init for the pool
 * @list_lock: Spinlock for page list in the pool
 * @pool_rbtree: RB tree with all pages held/reserved in this pool
 * @mempool: Mempool to pre-allocate tracking structs for pages in this pool
 */
struct kgsl_page_pool {
	unsigned int pool_order;
	unsigned int page_count;
	unsigned int reserved_pages;
	spinlock_t list_lock;
	struct rb_root pool_rbtree;
	mempool_t *mempool;
};

static void *_pool_entry_alloc(gfp_t gfp_mask, void *arg)
{
	return kmem_cache_alloc(addr_page_cache, gfp_mask);
}

static void _pool_entry_free(void *element, void *arg)
{
	return kmem_cache_free(addr_page_cache, element);
}

static int
__kgsl_pool_add_page(struct kgsl_page_pool *pool, struct page *p)
{
	struct rb_node **node, *parent;
	struct kgsl_pool_page_entry *new_page, *entry;
	gfp_t gfp_mask = GFP_KERNEL & ~__GFP_DIRECT_RECLAIM;

	new_page = pool->mempool ? mempool_alloc(pool->mempool, gfp_mask) :
			kmem_cache_alloc(addr_page_cache, gfp_mask);
	if (new_page == NULL)
		return -ENOMEM;

	spin_lock(&pool->list_lock);
	node = &pool->pool_rbtree.rb_node;
	new_page->physaddr = page_to_phys(p);
	new_page->page = p;

	while (*node != NULL) {
		parent = *node;
		entry = rb_entry(parent, struct kgsl_pool_page_entry, node);

		if (new_page->physaddr < entry->physaddr)
			node = &parent->rb_left;
		else
			node = &parent->rb_right;
	}

	rb_link_node(&new_page->node, parent, node);
	rb_insert_color(&new_page->node, &pool->pool_rbtree);
	pool->page_count++;
	spin_unlock(&pool->list_lock);

	return 0;
}

static struct page *
__kgsl_pool_get_page(struct kgsl_page_pool *pool)
{
	struct rb_node *node;
	struct kgsl_pool_page_entry *entry;
	struct page *p;

	node = rb_first(&pool->pool_rbtree);
	if (!node)
		return NULL;

	entry = rb_entry(node, struct kgsl_pool_page_entry, node);
	p = entry->page;
	rb_erase(&entry->node, &pool->pool_rbtree);
	if (pool->mempool)
		mempool_free(entry, pool->mempool);
	else
		kmem_cache_free(addr_page_cache, entry);
	pool->page_count--;
	return p;
}

static void kgsl_pool_list_init(struct kgsl_page_pool *pool)
{
	pool->pool_rbtree = RB_ROOT;
}

static void kgsl_pool_cache_init(void)
{
	addr_page_cache =  KMEM_CACHE(kgsl_pool_page_entry, 0);
}

static void kgsl_pool_cache_destroy(void)
{
	kmem_cache_destroy(addr_page_cache);
}

static void kgsl_destroy_page_pool(struct kgsl_page_pool *pool)
{
	mempool_destroy(pool->mempool);
}

#else
/**
 * struct kgsl_page_pool - Structure to hold information for the pool
 * @pool_order: Page order describing the size of the page
 * @page_count: Number of pages currently present in the pool
 * @reserved_pages: Number of pages reserved at init for the pool
 * @list_lock: Spinlock for page list in the pool
 * @page_list: List of pages held/reserved in this pool
 */
struct kgsl_page_pool {
	unsigned int pool_order;
	unsigned int page_count;
	unsigned int reserved_pages;
	spinlock_t list_lock;
	struct list_head page_list;
};

static int
__kgsl_pool_add_page(struct kgsl_page_pool *pool, struct page *p)
{
	spin_lock(&pool->list_lock);
	list_add_tail(&p->lru, &pool->page_list);
	pool->page_count++;
	spin_unlock(&pool->list_lock);

	return 0;
}

static struct page *
__kgsl_pool_get_page(struct kgsl_page_pool *pool)
{
	struct page *p;

	p = list_first_entry_or_null(&pool->page_list, struct page, lru);
	if (p) {
		pool->page_count--;
		list_del(&p->lru);
	}

	return p;
}

static void kgsl_pool_list_init(struct kgsl_page_pool *pool)
{
	INIT_LIST_HEAD(&pool->page_list);
}

static void kgsl_pool_cache_init(void)
{
}

static void kgsl_pool_cache_destroy(void)
{
}

static void kgsl_destroy_page_pool(struct kgsl_page_pool *pool)
{
}
#endif

static struct kgsl_page_pool kgsl_pools[6];
static int kgsl_num_pools;
static int kgsl_pool_max_pages;

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

	if (__kgsl_pool_add_page(pool, p)) {
		__free_pages(p, pool->pool_order);
		trace_kgsl_pool_free_page(pool->pool_order);
		return;
	}

	trace_kgsl_pool_add_page(pool->pool_order, pool->page_count);
	mod_node_page_state(page_pgdat(p),  NR_KERNEL_MISC_RECLAIMABLE,
				(1 << pool->pool_order));
}

/* Returns a page from specified pool */
static struct page *
_kgsl_pool_get_page(struct kgsl_page_pool *pool)
{
	struct page *p = NULL;

	spin_lock(&pool->list_lock);
	p = __kgsl_pool_get_page(pool);
	spin_unlock(&pool->list_lock);
	if (p != NULL) {
		trace_kgsl_pool_get_page(pool->pool_order, pool->page_count);
		mod_node_page_state(page_pgdat(p), NR_KERNEL_MISC_RECLAIMABLE,
				-(1 << pool->pool_order));
	}
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

/* Returns the total number of pages in all pools excluding reserved pages */
static unsigned long kgsl_pool_size_nonreserved(void)
{
	int i;
	unsigned long total = 0;

	for (i = 0; i < kgsl_num_pools; i++) {
		struct kgsl_page_pool *pool = &kgsl_pools[i];

		spin_lock(&pool->list_lock);
		if (pool->page_count > pool->reserved_pages)
			total += (pool->page_count - pool->reserved_pages) *
					(1 << pool->pool_order);
		spin_unlock(&pool->list_lock);
	}

	return total;
}

/*
 * Returns a page from specified pool only if pool
 * currently holds more number of pages than reserved
 * pages.
 */
static struct page *
_kgsl_pool_get_nonreserved_page(struct kgsl_page_pool *pool)
{
	struct page *p = NULL;

	spin_lock(&pool->list_lock);
	if (pool->page_count <= pool->reserved_pages) {
		spin_unlock(&pool->list_lock);
		return NULL;
	}

	p = __kgsl_pool_get_page(pool);
	spin_unlock(&pool->list_lock);
	if (p != NULL) {
		trace_kgsl_pool_get_page(pool->pool_order, pool->page_count);
		mod_node_page_state(page_pgdat(p), NR_KERNEL_MISC_RECLAIMABLE,
				-(1 << pool->pool_order));
	}
	return p;
}

/*
 * This will shrink the specified pool by num_pages or by
 * (page_count - reserved_pages), whichever is smaller.
 */
static unsigned int
_kgsl_pool_shrink(struct kgsl_page_pool *pool,
			unsigned int num_pages, bool exit)
{
	int j;
	unsigned int pcount = 0;
	struct page *(*get_page)(struct kgsl_page_pool *) =
		_kgsl_pool_get_nonreserved_page;

	if (pool == NULL || num_pages == 0)
		return pcount;

	num_pages = (num_pages + (1 << pool->pool_order) - 1) >>
				pool->pool_order;

	/* This is to ensure that we free reserved pages */
	if (exit)
		get_page = _kgsl_pool_get_page;

	for (j = 0; j < num_pages; j++) {
		struct page *page = get_page(pool);

		if (!page)
			break;

		__free_pages(page, pool->pool_order);
		pcount += (1 << pool->pool_order);
		trace_kgsl_pool_free_page(pool->pool_order);
	}

	return pcount;
}

/*
 * This function removes number of pages specified by
 * target_pages from the total pool size.
 *
 * Remove target_pages from the pool, starting from higher order pool.
 */
static unsigned long
kgsl_pool_reduce(int target_pages, bool exit)
{
	int i, ret;
	unsigned long pcount = 0;

	for (i = (kgsl_num_pools - 1); i >= 0; i--) {
		if (target_pages <= 0)
			return pcount;

		/* Remove target_pages pages from this pool */
		ret = _kgsl_pool_shrink(&kgsl_pools[i], target_pages, exit);
		target_pages -= ret;
		pcount += ret;
	}

	return pcount;
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

/*
 * Return true if the pool of specified page size is supported
 * or no pools are supported otherwise return false.
 */
static bool kgsl_pool_available(unsigned int page_size)
{
	int order = get_order(page_size);

	if (!kgsl_num_pools)
		return true;

	return (kgsl_get_pool_index(order) >= 0);
}

int kgsl_get_page_size(size_t size, unsigned int align)
{
	size_t pool;

	for (pool = SZ_1M; pool > PAGE_SIZE; pool >>= 1)
		if ((align >= ilog2(pool)) && (size >= pool) &&
			kgsl_pool_available(pool))
			return pool;

	return PAGE_SIZE;
}

int kgsl_pool_alloc_page(int *page_size, struct page **pages,
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
		trace_kgsl_pool_alloc_page_system(order);
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
			trace_kgsl_pool_alloc_page_system(order);
			goto done;
		}
	}

	pool_idx = kgsl_get_pool_index(order);
	page = _kgsl_pool_get_page(pool);

	/* Allocate a new page if not allocated from pool */
	if (page == NULL) {
		gfp_t gfp_mask = kgsl_gfp_mask(order);

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
		trace_kgsl_pool_alloc_page_system(order);
	}

done:
	kgsl_zero_page(page, order, dev);

	for (j = 0; j < (*page_size >> PAGE_SHIFT); j++) {
		p = nth_page(page, j);
		pages[pcount] = p;
		pcount++;
	}

	return pcount;

eagain:
	trace_kgsl_pool_try_page_lower(get_order(*page_size));
	*page_size = kgsl_get_page_size(size, ilog2(size));
	*align = ilog2(*page_size);
	return -EAGAIN;
}

void kgsl_pool_free_page(struct page *page)
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
	trace_kgsl_pool_free_page(page_order);
}

/* Functions for the shrinker */

static unsigned long
kgsl_pool_shrink_scan_objects(struct shrinker *shrinker,
					struct shrink_control *sc)
{
	/* sc->nr_to_scan represents number of pages to be removed*/
	unsigned long pcount = kgsl_pool_reduce(sc->nr_to_scan, false);

	/* If pools are exhausted return SHRINK_STOP */
	return pcount ? pcount : SHRINK_STOP;
}

static unsigned long
kgsl_pool_shrink_count_objects(struct shrinker *shrinker,
					struct shrink_control *sc)
{
	/*
	 * Return non-reserved pool size as we don't
	 * want shrinker to free reserved pages.
	 */
	return kgsl_pool_size_nonreserved();
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
	pool->reserved_pages = min_t(u32, reserved, 4096);

#if IS_ENABLED(CONFIG_QCOM_KGSL_SORT_POOL)
	/*
	 * Pre-allocate tracking structs for reserved_pages so that
	 * the pool can hold them even in low memory conditions
	 */
	pool->mempool = mempool_create(pool->reserved_pages,
			_pool_entry_alloc, _pool_entry_free, NULL);
#endif

	for (i = 0; i < pool->reserved_pages; i++) {
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

	order = get_order(size);

	if (order > 8) {
		pr_err("kgsl: %pOF: pool order %d is too big\n", node, order);
		return -EINVAL;
	}

	pool->pool_order = order;

	spin_lock_init(&pool->list_lock);
	kgsl_pool_list_init(pool);

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

	kgsl_pool_cache_init();

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
	int i;

	/* Release all pages in pools, if any.*/
	kgsl_pool_reduce(INT_MAX, true);

	/* Unregister shrinker */
	unregister_shrinker(&kgsl_pool_shrinker);

	/* Destroy helper structures */
	for (i = 0; i < kgsl_num_pools; i++)
		kgsl_destroy_page_pool(&kgsl_pools[i]);

	/* Destroy the kmem cache */
	kgsl_pool_cache_destroy();
}


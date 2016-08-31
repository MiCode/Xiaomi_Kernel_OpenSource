/*
 * drivers/video/tegra/nvmap/nvmap_pp.c
 *
 * Manage page pools to speed up page allocation.
 *
 * Copyright (c) 2009-2013, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/moduleparam.h>
#include <linux/shrinker.h>

#include "nvmap_priv.h"

#define NVMAP_TEST_PAGE_POOL_SHRINKER 1
static bool enable_pp = 1;
static int pool_size[NVMAP_NUM_POOLS];

static char *s_memtype_str[] = {
	"uc",
	"wc",
	"iwb",
	"wb",
};

static inline void nvmap_page_pool_lock(struct nvmap_page_pool *pool)
{
	mutex_lock(&pool->lock);
}

static inline void nvmap_page_pool_unlock(struct nvmap_page_pool *pool)
{
	mutex_unlock(&pool->lock);
}

static struct page *nvmap_page_pool_alloc_locked(struct nvmap_page_pool *pool)
{
	struct page *page = NULL;

	if (pool->npages > 0) {
		page = pool->page_array[--pool->npages];
		pool->page_array[pool->npages] = NULL;
		atomic_dec(&page->_count);
		BUG_ON(atomic_read(&page->_count) != 1);
	}
	return page;
}

struct page *nvmap_page_pool_alloc(struct nvmap_page_pool *pool)
{
	struct page *page = NULL;

	if (pool) {
		nvmap_page_pool_lock(pool);
		page = nvmap_page_pool_alloc_locked(pool);
		nvmap_page_pool_unlock(pool);
	}
	return page;
}

static bool nvmap_page_pool_release_locked(struct nvmap_page_pool *pool,
					    struct page *page)
{
	int ret = false;

	if (enable_pp && pool->npages < pool->max_pages) {
		atomic_inc(&page->_count);
		BUG_ON(atomic_read(&page->_count) != 2);
		BUG_ON(pool->page_array[pool->npages] != NULL);
		pool->page_array[pool->npages++] = page;
		ret = true;
	}
	return ret;
}

bool nvmap_page_pool_release(struct nvmap_page_pool *pool, struct page *page)
{
	int ret = false;

	if (pool) {
		nvmap_page_pool_lock(pool);
		ret = nvmap_page_pool_release_locked(pool, page);
		nvmap_page_pool_unlock(pool);
	}
	return ret;
}

static int nvmap_page_pool_get_available_count(struct nvmap_page_pool *pool)
{
	return pool->npages;
}

static int nvmap_page_pool_free(struct nvmap_page_pool *pool, int nr_free)
{
	int err;
	int i = nr_free;
	int idx = 0;
	struct page *page;

	if (!nr_free)
		return nr_free;
	nvmap_page_pool_lock(pool);
	while (i) {
		page = nvmap_page_pool_alloc_locked(pool);
		if (!page)
			break;
		pool->shrink_array[idx++] = page;
		i--;
	}

	if (idx) {
		/* This op should never fail. */
		err = nvmap_set_pages_array_wb(pool->shrink_array, idx);
		BUG_ON(err);
	}

	while (idx--)
		__free_page(pool->shrink_array[idx]);
	nvmap_page_pool_unlock(pool);
	return i;
}

ulong nvmap_page_pool_get_unused_pages(void)
{
	unsigned int i;
	int total = 0;
	struct nvmap_share *share;

	if (!nvmap_dev)
		return 0;

	share = nvmap_get_share_from_dev(nvmap_dev);
	if (!share)
		return 0;

	for (i = 0; i < NVMAP_NUM_POOLS; i++)
		total += nvmap_page_pool_get_available_count(&share->pools[i]);

	return total;
}

static void nvmap_page_pool_resize(struct nvmap_page_pool *pool, int size)
{
	int available_pages;
	int pages_to_release = 0;
	struct page **page_array = NULL;
	struct page **shrink_array = NULL;

	if (size == pool->max_pages)
		return;
repeat:
	nvmap_page_pool_free(pool, pages_to_release);
	nvmap_page_pool_lock(pool);
	available_pages = nvmap_page_pool_get_available_count(pool);
	if (available_pages > size) {
		nvmap_page_pool_unlock(pool);
		pages_to_release = available_pages - size;
		goto repeat;
	}

	if (size == 0) {
		vfree(pool->page_array);
		vfree(pool->shrink_array);
		pool->page_array = pool->shrink_array = NULL;
		goto out;
	}

	page_array = vzalloc(sizeof(struct page *) * size);
	shrink_array = vzalloc(sizeof(struct page *) * size);
	if (!page_array || !shrink_array)
		goto fail;

	memcpy(page_array, pool->page_array,
		pool->npages * sizeof(struct page *));
	vfree(pool->page_array);
	vfree(pool->shrink_array);
	pool->page_array = page_array;
	pool->shrink_array = shrink_array;
out:
	pr_debug("%s pool resized to %d from %d pages",
		s_memtype_str[pool->flags], size, pool->max_pages);
	pool->max_pages = size;
	goto exit;
fail:
	vfree(page_array);
	vfree(shrink_array);
	pr_err("failed");
exit:
	nvmap_page_pool_unlock(pool);
}

static int nvmap_page_pool_shrink(struct shrinker *shrinker,
				  struct shrink_control *sc)
{
	unsigned int i;
	unsigned int pool_offset;
	struct nvmap_page_pool *pool;
	int shrink_pages = sc->nr_to_scan;
	static atomic_t start_pool = ATOMIC_INIT(-1);
	struct nvmap_share *share = nvmap_get_share_from_dev(nvmap_dev);

	if (!shrink_pages)
		goto out;

	pr_debug("sh_pages=%d", shrink_pages);

	for (i = 0; i < NVMAP_NUM_POOLS && shrink_pages; i++) {
		pool_offset = atomic_add_return(1, &start_pool) %
				NVMAP_NUM_POOLS;
		pool = &share->pools[pool_offset];
		shrink_pages = nvmap_page_pool_free(pool, shrink_pages);
	}
out:
	return nvmap_page_pool_get_unused_pages();
}

static struct shrinker nvmap_page_pool_shrinker = {
	.shrink = nvmap_page_pool_shrink,
	.seeks = 1,
};

static void shrink_page_pools(int *total_pages, int *available_pages)
{
	struct shrink_control sc;

	if (*total_pages == 0) {
		sc.gfp_mask = GFP_KERNEL;
		sc.nr_to_scan = 0;
		*total_pages = nvmap_page_pool_shrink(NULL, &sc);
	}
	sc.nr_to_scan = *total_pages;
	*available_pages = nvmap_page_pool_shrink(NULL, &sc);
}

#if NVMAP_TEST_PAGE_POOL_SHRINKER
static int shrink_pp;
static int shrink_set(const char *arg, const struct kernel_param *kp)
{
	int cpu = smp_processor_id();
	unsigned long long t1, t2;
	int total_pages, available_pages;

	param_set_int(arg, kp);

	if (shrink_pp) {
		total_pages = shrink_pp;
		t1 = cpu_clock(cpu);
		shrink_page_pools(&total_pages, &available_pages);
		t2 = cpu_clock(cpu);
		pr_debug("shrink page pools: time=%lldns, "
			"total_pages_released=%d, free_pages_available=%d",
			t2-t1, total_pages, available_pages);
	}
	return 0;
}

static int shrink_get(char *buff, const struct kernel_param *kp)
{
	return param_get_int(buff, kp);
}

static struct kernel_param_ops shrink_ops = {
	.get = shrink_get,
	.set = shrink_set,
};

module_param_cb(shrink_page_pools, &shrink_ops, &shrink_pp, 0644);
#endif

static int enable_pp_set(const char *arg, const struct kernel_param *kp)
{
	int total_pages, available_pages, ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (!enable_pp) {
		total_pages = 0;
		shrink_page_pools(&total_pages, &available_pages);
		pr_info("disabled page pools and released pages, "
			"total_pages_released=%d, free_pages_available=%d",
			total_pages, available_pages);
	}
	return 0;
}

static int enable_pp_get(char *buff, const struct kernel_param *kp)
{
	return param_get_int(buff, kp);
}

static struct kernel_param_ops enable_pp_ops = {
	.get = enable_pp_get,
	.set = enable_pp_set,
};

module_param_cb(enable_page_pools, &enable_pp_ops, &enable_pp, 0644);

#define POOL_SIZE_SET(m, i) \
static int pool_size_##m##_set(const char *arg, const struct kernel_param *kp) \
{ \
	struct nvmap_share *share = nvmap_get_share_from_dev(nvmap_dev); \
	param_set_int(arg, kp); \
	nvmap_page_pool_resize(&share->pools[i], pool_size[i]); \
	return 0; \
}

#define POOL_SIZE_GET(m) \
static int pool_size_##m##_get(char *buff, const struct kernel_param *kp) \
{ \
	return param_get_int(buff, kp); \
}

#define POOL_SIZE_OPS(m) \
static struct kernel_param_ops pool_size_##m##_ops = { \
	.get = pool_size_##m##_get, \
	.set = pool_size_##m##_set, \
};

#define POOL_SIZE_MOUDLE_PARAM_CB(m, i) \
module_param_cb(m##_pool_size, &pool_size_##m##_ops, &pool_size[i], 0644)

POOL_SIZE_SET(uc, NVMAP_HANDLE_UNCACHEABLE);
POOL_SIZE_GET(uc);
POOL_SIZE_OPS(uc);
POOL_SIZE_MOUDLE_PARAM_CB(uc, NVMAP_HANDLE_UNCACHEABLE);

POOL_SIZE_SET(wc, NVMAP_HANDLE_WRITE_COMBINE);
POOL_SIZE_GET(wc);
POOL_SIZE_OPS(wc);
POOL_SIZE_MOUDLE_PARAM_CB(wc, NVMAP_HANDLE_WRITE_COMBINE);

POOL_SIZE_SET(iwb, NVMAP_HANDLE_INNER_CACHEABLE);
POOL_SIZE_GET(iwb);
POOL_SIZE_OPS(iwb);
POOL_SIZE_MOUDLE_PARAM_CB(iwb, NVMAP_HANDLE_INNER_CACHEABLE);

POOL_SIZE_SET(wb, NVMAP_HANDLE_CACHEABLE);
POOL_SIZE_GET(wb);
POOL_SIZE_OPS(wb);
POOL_SIZE_MOUDLE_PARAM_CB(wb, NVMAP_HANDLE_CACHEABLE);

int nvmap_page_pool_init(struct nvmap_page_pool *pool, int flags)
{
	static int reg = 1;
	struct sysinfo info;
#ifdef CONFIG_NVMAP_PAGE_POOLS_INIT_FILLUP
	int i;
	int err;
	struct page *page;
	int pages_to_fill;
	int highmem_pages = 0;
	typedef int (*set_pages_array) (struct page **pages, int addrinarray);
	set_pages_array s_cpa[] = {
		nvmap_set_pages_array_uc,
		nvmap_set_pages_array_wc,
		nvmap_set_pages_array_iwb,
		nvmap_set_pages_array_wb
	};
#endif

	BUG_ON(flags >= NVMAP_NUM_POOLS);
	memset(pool, 0x0, sizeof(*pool));
	mutex_init(&pool->lock);
	pool->flags = flags;

	/* No default pool for cached memory. */
	if (flags == NVMAP_HANDLE_CACHEABLE)
		return 0;

#if !defined(CONFIG_OUTER_CACHE)
	/* If outer cache is not enabled or don't exist, cacheable and
	 * inner cacheable memory are same. For cacheable memory, there
	 * is no need of page pool as there is no need to flush cache and
	 * change page attributes.
	 */
	if (flags == NVMAP_HANDLE_INNER_CACHEABLE)
		return 0;
#endif

	si_meminfo(&info);
	if (!pool_size[flags] && !CONFIG_NVMAP_PAGE_POOL_SIZE)
		/* Use 3/8th of total ram for page pools.
		 * 1/8th for uc, 1/8th for wc and 1/8th for iwb.
		 */
		pool->max_pages = info.totalram >> 3;
	else
		pool->max_pages = CONFIG_NVMAP_PAGE_POOL_SIZE;

	if (pool->max_pages <= 0 || pool->max_pages >= info.totalram)
		goto fail;
	pool_size[flags] = pool->max_pages;
	pr_info("nvmap %s page pool size=%d pages\n",
		s_memtype_str[flags], pool->max_pages);
	pool->page_array = vzalloc(sizeof(struct page *) * pool->max_pages);
	pool->shrink_array = vzalloc(sizeof(struct page *) * pool->max_pages);
	if (!pool->page_array || !pool->shrink_array)
		goto fail;

	if (reg) {
		reg = 0;
		register_shrinker(&nvmap_page_pool_shrinker);
	}

#ifdef CONFIG_NVMAP_PAGE_POOLS_INIT_FILLUP
	pages_to_fill = CONFIG_NVMAP_PAGE_POOLS_INIT_FILLUP_SIZE * SZ_1M /
			PAGE_SIZE;
	pages_to_fill = pages_to_fill ? : pool->max_pages;

	nvmap_page_pool_lock(pool);
	for (i = 0; i < pages_to_fill; i++) {
		page = alloc_page(GFP_NVMAP);
		if (!page)
			goto do_cpa;
		if (!nvmap_page_pool_release_locked(pool, page)) {
			__free_page(page);
			goto do_cpa;
		}
		if (PageHighMem(page))
			highmem_pages++;
	}
	si_meminfo(&info);
	pr_info("nvmap pool = %s, highmem=%d, pool_size=%d,"
		"totalram=%lu, freeram=%lu, totalhigh=%lu, freehigh=%lu\n",
		s_memtype_str[flags], highmem_pages, pool->max_pages,
		info.totalram, info.freeram, info.totalhigh, info.freehigh);
do_cpa:
	if (pool->npages) {
		err = (*s_cpa[flags])(pool->page_array, pool->npages);
		BUG_ON(err);
	}
	nvmap_page_pool_unlock(pool);
#endif
	return 0;
fail:
	pool->max_pages = 0;
	vfree(pool->shrink_array);
	vfree(pool->page_array);
	return -ENOMEM;
}

/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2017,2019,2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __KGSL_POOL_H
#define __KGSL_POOL_H

#ifdef CONFIG_QCOM_KGSL_USE_SHMEM
static inline void kgsl_probe_page_pools(void) { }
static inline void kgsl_exit_page_pools(void) { }
static inline int kgsl_get_page_size(size_t size, unsigned int align)
{
	return PAGE_SIZE;
}

static inline int kgsl_pool_page_count_get(void *data, u64 *val)
{
	return 0;
}

static inline int kgsl_pool_reserved_get(void *data, u64 *val)
{
	return 0;
}

static inline int kgsl_pool_size_total(void)
{
	return 0;
}
#else
/**
 * kgsl_pool_free_page - Frees the page and adds it back to pool/system memory
 * @page: Pointer to page struct that needs to be freed
 */
void kgsl_pool_free_page(struct page *page);

/**
 * kgsl_get_page_size - Get supported pagesize
 * @size: Size of the page
 * @align: Desired alignment of the size
 *
 * Return largest available page size from pools that can be used to meet
 * given size and alignment requirements
 */
int kgsl_get_page_size(size_t size, unsigned int align);

/**
 * kgsl_pool_alloc_page - Allocate a page of requested size
 * @page_size: Size of the page to be allocated
 * @pages: pointer to hold list of pages, should be big enough to hold
 * requested page
 * @len: Length of array pages
 *
 * Return total page count on success and negative value on failure
 */
int kgsl_pool_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align,
			struct device *dev);

/**
 * kgsl_pool_free_pages - Free pages in an pages array
 * @pages: pointer to an array of page structs
 * @page_count: Number of entries in @pages
 *
 * Free the pages by collapsing any physical adjacent pages.
 * Pages are added back to the pool, if pool has sufficient space
 * otherwise they are given back to system.
 */
void kgsl_pool_free_pages(struct page **pages, unsigned int page_count);

/* Debugfs node functions */
int kgsl_pool_reserved_get(void *data, u64 *val);
int kgsl_pool_page_count_get(void *data, u64 *val);

/**
 * kgsl_pool_size_total - Return the number of pages in all kgsl page pools
 */
int kgsl_pool_size_total(void);

/**
 * kgsl_probe_page_pools - Initialize the memory pools
 */
void kgsl_probe_page_pools(void);

/**
 * kgsl_exit_page_pools - Free outstanding pooled memory
 */
void kgsl_exit_page_pools(void);

#endif
#endif /* __KGSL_POOL_H */


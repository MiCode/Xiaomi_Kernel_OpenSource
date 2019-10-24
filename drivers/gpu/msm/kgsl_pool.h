/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2017,2019 The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_POOL_H
#define __KGSL_POOL_H

void kgsl_pool_free_sgt(struct sg_table *sgt);

/**
 * kgsl_pool_alloc_pages - Allocate an array of pages from the pool
 * @size: Size of the allocation
 * @pages: Pointer to an array of pages
 * @dev: A &struct device pointer
 *
 * Allocate a list of pages and store it in the pointer pointed to by @pages.
 * @dev specifies a &struct device that is used to call dma_sync_sg_for_device
 * to synchronize the caches. If @dev isn't specified, no cache maintenance
 * will be performed.
 *
 * Return: The number of entries in the array pointed to by @page or negative
 * on error.
 */
int kgsl_pool_alloc_pages(u64 size, struct page ***pages, struct device *dev);

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

/**
 * kgsl_probe_page_pools - Initialize the memory pools pools
 */
void kgsl_probe_page_pools(void);

/**
 * kgsl_exit_page_pools - Free outstanding pooled memory
 */
void kgsl_exit_page_pools(void);
#endif /* __KGSL_POOL_H */


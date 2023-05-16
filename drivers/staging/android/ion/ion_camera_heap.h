/*
 * Copyright (c) 2019. Xiaomi Technology Co.
 *
 * All Rights Reserved.
 *
 * Confidential and Proprietary - Xiaomi Technology Co.
 */

#include <soc/qcom/secure_buffer.h>
#include "ion.h"

#ifndef _ION_CAMERA_HEAP_H
#define _ION_CAMERA_HEAP_H

#ifndef CONFIG_ALLOC_BUFFERS_IN_4K_CHUNKS
#if defined(CONFIG_IOMMU_IO_PGTABLE_ARMV7S)
static const unsigned int orders[] = {8, 4, 0};
static const unsigned int cam_reserved_counts[] = {1024, 2048, 3096};
#else
static const unsigned int orders[] = {8, 4, 0};
static const unsigned int cam_reserved_counts[] = {20, 1600, 35840};
#endif
#else
static const unsigned int orders[] = {0};
static const unsigned int cam_reserved_counts[] = {0};
#endif

#define NUM_ORDERS ARRAY_SIZE(orders)

#define ION_KTHREAD_NICE_VAL 10

enum ion_kthread_type {
	ION_KTHREAD_UNCACHED,
	ION_KTHREAD_CACHED,
	ION_MAX_NUM_KTHREADS
};

struct ion_camera_heap {
	struct ion_heap heap;
	struct ion_page_pool *uncached_pools[MAX_ORDER];
	struct ion_page_pool *cached_pools[MAX_ORDER];
	/* worker threads to refill the pool */
	struct task_struct *kworker[ION_MAX_NUM_KTHREADS];
	struct ion_page_pool *secure_pools[VMID_LAST][MAX_ORDER];
	struct ion_page_pool *reserved_pools[MAX_ORDER];
	/* Prevents unnecessary page splitting */
	struct mutex split_page_mutex;
};

struct page_info {
	struct page *page;
	bool from_pool;
	unsigned int order;
	struct list_head list;
};

int camera_camera_order_to_index(unsigned int order);

bool reserved_pools_full(struct ion_camera_heap *heap,unsigned int order);

int reserved_pool_page_count(struct ion_camera_heap *heap, unsigned int order);

void free_camera_buffer_page(struct ion_camera_heap *heap,
		      struct ion_buffer *buffer, struct page *page,
		      unsigned int order);

#endif /* _ION_CAMERA_HEAP_H */

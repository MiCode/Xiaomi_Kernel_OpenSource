/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */
#include <soc/qcom/secure_buffer.h>
#include "msm_ion_priv.h"

#ifndef _ION_MSM_SYSTEM_HEAP_H
#define _ION_MSM_SYSTEM_HEAP_H

#ifndef CONFIG_ALLOC_BUFFERS_IN_4K_CHUNKS
#if defined(CONFIG_IOMMU_IO_PGTABLE_ARMV7S) && !defined(CONFIG_64BIT) && !defined(CONFIG_ARM_LPAE)
static const unsigned int orders[] = {8, 4, 0};
#else
static const unsigned int orders[] = {9, 4, 0};
#endif
#else
static const unsigned int orders[] = {0};
#endif

#define NUM_ORDERS ARRAY_SIZE(orders)

#define ION_KTHREAD_NICE_VAL 10

#define to_msm_system_heap(_heap) \
	container_of(to_msm_ion_heap(_heap), struct ion_msm_system_heap, heap)

enum ion_kthread_type {
	ION_KTHREAD_UNCACHED,
	ION_KTHREAD_CACHED,
	ION_MAX_NUM_KTHREADS
};

struct ion_msm_system_heap {
	struct msm_ion_heap heap;
	struct ion_msm_page_pool *uncached_pools[MAX_ORDER];
	struct ion_msm_page_pool *cached_pools[MAX_ORDER];
	/* worker threads to refill the pool */
	struct task_struct *kworker[ION_MAX_NUM_KTHREADS];
	struct ion_msm_page_pool *secure_pools[VMID_LAST][MAX_ORDER];
	/* Prevents unnecessary page splitting */
	struct mutex split_page_mutex;
};

struct page_info {
	struct page *page;
	bool from_pool;
	unsigned int order;
	struct list_head list;
};

int order_to_index(unsigned int order);

void free_buffer_page(struct ion_msm_system_heap *heap,
		      struct ion_buffer *buffer, struct page *page,
		      unsigned int order);

#endif /* _ION_MSM_SYSTEM_HEAP_H */

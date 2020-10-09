/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */
#include "msm_ion_priv.h"
#include "ion_msm_system_heap.h"

#ifndef _ION_SYSTEM_SECURE_HEAP_H
#define _ION_SYSTEM_SECURE_HEAP_H

struct page *alloc_from_secure_pool_order(struct ion_msm_system_heap *heap,
					  struct ion_buffer *buffer,
					  unsigned long order);

struct page *split_page_from_secure_pool(struct ion_msm_system_heap *heap,
					 struct ion_buffer *buffer);

int ion_secure_page_pool_shrink(struct ion_msm_system_heap *sys_heap,
				int vmid, int order_idx, int nr_to_scan);

#endif /* _ION_SYSTEM_SECURE_HEAP_H */

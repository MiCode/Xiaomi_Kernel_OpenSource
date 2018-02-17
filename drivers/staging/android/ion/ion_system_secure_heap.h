/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "ion.h"
#include "ion_system_heap.h"

#ifndef _ION_SYSTEM_SECURE_HEAP_H
#define _ION_SYSTEM_SECURE_HEAP_H

int ion_system_secure_heap_prefetch(struct ion_heap *heap, void *data);
int ion_system_secure_heap_drain(struct ion_heap *heap, void *data);

struct page *alloc_from_secure_pool_order(struct ion_system_heap *heap,
					  struct ion_buffer *buffer,
					  unsigned long order);

struct page *split_page_from_secure_pool(struct ion_system_heap *heap,
					 struct ion_buffer *buffer);

int ion_secure_page_pool_shrink(
		struct ion_system_heap *sys_heap,
		int vmid, int order_idx, int nr_to_scan);

#endif /* _ION_SYSTEM_SECURE_HEAP_H */

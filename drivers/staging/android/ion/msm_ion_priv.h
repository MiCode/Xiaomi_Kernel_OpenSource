/*
 * drivers/gpu/ion/ion_priv.h
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MSM_ION_PRIV_H
#define _MSM_ION_PRIV_H

#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/seq_file.h>

/**
 * struct mem_map_data - represents information about the memory map for a heap
 * @node:		list node used to store in the list of mem_map_data
 * @addr:		start address of memory region.
 * @addr:		end address of memory region.
 * @size:		size of memory region
 * @client_name:		name of the client who owns this buffer.
 *
 */
struct mem_map_data {
	struct list_head node;
	ion_phys_addr_t addr;
	ion_phys_addr_t addr_end;
	unsigned long size;
	const char *client_name;
};

struct ion_heap *ion_iommu_heap_create(struct ion_platform_heap *);
void ion_iommu_heap_destroy(struct ion_heap *);

struct ion_heap *ion_cp_heap_create(struct ion_platform_heap *);
void ion_cp_heap_destroy(struct ion_heap *);

long msm_ion_custom_ioctl(struct ion_client *client,
				unsigned int cmd,
				unsigned long arg);

#ifdef CONFIG_CMA
struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *);
void ion_cma_heap_destroy(struct ion_heap *);

struct ion_heap *ion_secure_cma_heap_create(struct ion_platform_heap *);
void ion_secure_cma_heap_destroy(struct ion_heap *);

int ion_secure_cma_prefetch(struct ion_heap *heap, void *data);

int ion_secure_cma_drain_pool(struct ion_heap *heap, void *unused);

#else
static inline int ion_secure_cma_prefetch(struct ion_heap *heap, void *data)
{
	return -ENODEV;
}

static inline int ion_secure_cma_drain_pool(struct ion_heap *heap, void *unused)
{
	return -ENODEV;
}



#endif

struct ion_heap *ion_removed_heap_create(struct ion_platform_heap *);
void ion_removed_heap_destroy(struct ion_heap *);

#define ION_CP_ALLOCATE_FAIL -1
#define ION_RESERVED_ALLOCATE_FAIL -1

/**
 * ion_do_cache_op - do cache operations.
 *
 * @client - pointer to ION client.
 * @handle - pointer to buffer handle.
 * @uaddr -  virtual address to operate on.
 * @offset - offset from physical address.
 * @len - Length of data to do cache operation on.
 * @cmd - Cache operation to perform:
 *		ION_IOC_CLEAN_CACHES
 *		ION_IOC_INV_CACHES
 *		ION_IOC_CLEAN_INV_CACHES
 *
 * Returns 0 on success
 */
int ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *uaddr, unsigned long offset, unsigned long len,
			unsigned int cmd);

void ion_cp_heap_get_base(struct ion_heap *heap, unsigned long *base,
			unsigned long *size);

void ion_mem_map_show(struct ion_heap *heap);

int ion_heap_allow_secure_allocation(enum ion_heap_type type);

int ion_heap_allow_heap_secure(enum ion_heap_type type);

int ion_heap_allow_handle_secure(enum ion_heap_type type);

/**
 * ion_create_chunked_sg_table - helper function to create sg table
 * with specified chunk size
 * @buffer_base:	The starting address used for the sg dma address
 * @chunk_size:		The size of each entry in the sg table
 * @total_size:		The total size of the sg table (i.e. the sum of the
 *			entries). This will be rounded up to the nearest
 *			multiple of `chunk_size'
 */
struct sg_table *ion_create_chunked_sg_table(phys_addr_t buffer_base,
					size_t chunk_size, size_t total_size);
#endif /* _MSM_ION_PRIV_H */

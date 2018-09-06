/*
 * Copyright (c) 2016, 2018, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_MSM_ION_H
#define _MSM_MSM_ION_H

#include "../ion.h"
#include "../../uapi/msm_ion.h"

enum ion_permission_type {
	IPT_TYPE_MM_CARVEOUT = 0,
	IPT_TYPE_MFC_SHAREDMEM = 1,
	IPT_TYPE_MDP_WRITEBACK = 2,
};

/*
 * This flag allows clients when mapping into the IOMMU to specify to
 * defer un-mapping from the IOMMU until the buffer memory is freed.
 */
#define ION_IOMMU_UNMAP_DELAYED 1

/*
 * This flag allows clients to defer unsecuring a buffer until the buffer
 * is actually freed.
 */
#define ION_UNSECURE_DELAYED	1

/**
 * struct ion_cp_heap_pdata - defines a content protection heap in the given
 * platform
 * @permission_type:	Memory ID used to identify the memory to TZ
 * @align:		Alignment requirement for the memory
 * @secure_base:	Base address for securing the heap.
 *			Note: This might be different from actual base address
 *			of this heap in the case of a shared heap.
 * @secure_size:	Memory size for securing the heap.
 *			Note: This might be different from actual size
 *			of this heap in the case of a shared heap.
 * @fixed_position	If nonzero, position in the fixed area.
 * @iommu_map_all:	Indicates whether we should map whole heap into IOMMU.
 * @iommu_2x_map_domain: Indicates the domain to use for overmapping.
 * @request_ion_region:	function to be called when the number of allocations
 *			goes from 0 -> 1
 * @release_ion_region:	function to be called when the number of allocations
 *			goes from 1 -> 0
 * @setup_ion_region:	function to be called upon ion registration
 * @allow_nonsecure_alloc: allow non-secure allocations from this heap. For
 *			secure heaps, this flag must be set so allow non-secure
 *			allocations. For non-secure heaps, this flag is ignored.
 *
 */
struct ion_cp_heap_pdata {
	enum ion_permission_type permission_type;
	unsigned int align;
	ion_phys_addr_t secure_base; /* Base addr used when heap is shared */
	size_t secure_size; /* Size used for securing heap when heap is shared*/
	int is_cma;
	enum ion_fixed_position fixed_position;
	int iommu_map_all;
	int iommu_2x_map_domain;
	int (*request_ion_region)(void *);
	int (*release_ion_region)(void *);
	void *(*setup_ion_region)(void);
	int allow_nonsecure_alloc;
};

/**
 * struct ion_co_heap_pdata - defines a carveout heap in the given platform
 * @adjacent_mem_id:	Id of heap that this heap must be adjacent to.
 * @align:		Alignment requirement for the memory
 * @fixed_position	If nonzero, position in the fixed area.
 * @request_ion_region:	function to be called when the number of allocations
 *			goes from 0 -> 1
 * @release_ion_region:	function to be called when the number of allocations
 *			goes from 1 -> 0
 * @setup_ion_region:	function to be called upon ion registration
 * @memory_type:Memory type used for the heap
 *
 */
struct ion_co_heap_pdata {
	int adjacent_mem_id;
	unsigned int align;
	enum ion_fixed_position fixed_position;
	int (*request_ion_region)(void *);
	int (*release_ion_region)(void *);
	void *(*setup_ion_region)(void);
};

struct msm_ion_prefetch_info {
	struct list_head list;
	int heap_id;
	unsigned long *sizes;
	int nr_sizes;
};

/**
 * struct ion_cma_pdata - extra data for CMA regions
 * @default_prefetch_size - default size to use for prefetching
 */
struct ion_cma_pdata {
	unsigned long default_prefetch_size;
};

#ifdef CONFIG_ION
/**
 *  msm_ion_client_create - allocate a client using the ion_device specified in
 *				drivers/staging/android/ion/msm/msm_ion.c
 *
 * name is the same as ion_client_create, return values
 * are the same as ion_client_create.
 */

struct ion_client *msm_ion_client_create(const char *name);

/**
 * ion_handle_get_flags - get the flags for a given handle
 *
 * @client - client who allocated the handle
 * @handle - handle to get the flags
 * @flags - pointer to store the flags
 *
 * Gets the current flags for a handle. These flags indicate various options
 * of the buffer (caching, security, etc.)
 */
int ion_handle_get_flags(struct ion_client *client, struct ion_handle *handle,
				unsigned long *flags);



/**
 * ion_handle_get_size - get the allocated size of a given handle
 *
 * @client - client who allocated the handle
 * @handle - handle to get the size
 * @size - pointer to store the size
 *
 * gives the allocated size of a handle. returns 0 on success, negative
 * value on error
 *
 * NOTE: This is intended to be used only to get a size to pass to map_iommu.
 * You should *NOT* rely on this for any other usage.
 */

int ion_handle_get_size(struct ion_client *client, struct ion_handle *handle,
			size_t *size);
/**
 * msm_ion_do_cache_op - do cache operations.
 *
 * @client - pointer to ION client.
 * @handle - pointer to buffer handle.
 * @vaddr -  virtual address to operate on.
 * @len - Length of data to do cache operation on.
 * @cmd - Cache operation to perform:
 *		ION_IOC_CLEAN_CACHES
 *		ION_IOC_INV_CACHES
 *		ION_IOC_CLEAN_INV_CACHES
 *
 * Returns 0 on success
 */
int msm_ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *vaddr, unsigned long len, unsigned int cmd);

int msm_ion_do_cache_offset_op(
		struct ion_client *client, struct ion_handle *handle,
		void *vaddr, unsigned int offset, unsigned long len,
		unsigned int cmd);

bool is_buffer_hlos_assigned(struct ion_buffer *buffer);

#else
static inline struct ion_client *msm_ion_client_create(const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline int ion_handle_get_size(struct ion_client *client,
				struct ion_handle *handle, size_t *size)
{
	return -ENODEV;
}

static inline int msm_ion_do_cache_op(struct ion_client *client,
			struct ion_handle *handle, void *vaddr,
			unsigned long len, unsigned int cmd)
{
	return -ENODEV;
}

int msm_ion_do_cache_offset_op(
		struct ion_client *client, struct ion_handle *handle,
		void *vaddr, unsigned int offset, unsigned long len,
		unsigned int cmd)
{
	return -ENODEV;
}

static bool is_buffer_hlos_assigned(struct ion_buffer *buffer)
{
	return true;
}

#endif /* CONFIG_ION */

#endif

/*
 *
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_MSM_ION_H
#define _LINUX_MSM_ION_H

#include <uapi/linux/msm_ion.h>

/*
 * This flag allows clients when mapping into the IOMMU to specify to
 * defer un-mapping from the IOMMU until the buffer memory is freed.
 */
#define ION_IOMMU_UNMAP_DELAYED 1

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
 * @reusable		Flag indicating whether this heap is reusable of not.
 *			(see FMEM)
 * @mem_is_fmem		Flag indicating whether this memory is coming from fmem
 *			or not.
 * @fixed_position	If nonzero, position in the fixed area.
 * @virt_addr:		Virtual address used when using fmem.
 * @iommu_map_all:	Indicates whether we should map whole heap into IOMMU.
 * @iommu_2x_map_domain: Indicates the domain to use for overmapping.
 * @request_region:	function to be called when the number of allocations
 *			goes from 0 -> 1
 * @release_region:	function to be called when the number of allocations
 *			goes from 1 -> 0
 * @setup_region:	function to be called upon ion registration
 * @memory_type:Memory type used for the heap
 *
 */
struct ion_cp_heap_pdata {
	enum ion_permission_type permission_type;
	unsigned int align;
	ion_phys_addr_t secure_base; /* Base addr used when heap is shared */
	size_t secure_size; /* Size used for securing heap when heap is shared*/
	int reusable;
	int mem_is_fmem;
	enum ion_fixed_position fixed_position;
	int iommu_map_all;
	int iommu_2x_map_domain;
	ion_virt_addr_t *virt_addr;
	int (*request_region)(void *);
	int (*release_region)(void *);
	void *(*setup_region)(void);
	enum ion_memory_types memory_type;
};

/**
 * struct ion_co_heap_pdata - defines a carveout heap in the given platform
 * @adjacent_mem_id:	Id of heap that this heap must be adjacent to.
 * @align:		Alignment requirement for the memory
 * @mem_is_fmem		Flag indicating whether this memory is coming from fmem
 *			or not.
 * @fixed_position	If nonzero, position in the fixed area.
 * @request_region:	function to be called when the number of allocations
 *			goes from 0 -> 1
 * @release_region:	function to be called when the number of allocations
 *			goes from 1 -> 0
 * @setup_region:	function to be called upon ion registration
 * @memory_type:Memory type used for the heap
 *
 */
struct ion_co_heap_pdata {
	int adjacent_mem_id;
	unsigned int align;
	int mem_is_fmem;
	enum ion_fixed_position fixed_position;
	int (*request_region)(void *);
	int (*release_region)(void *);
	void *(*setup_region)(void);
	enum ion_memory_types memory_type;
};

#ifdef CONFIG_ION
/**
 * msm_ion_secure_heap - secure a heap. Wrapper around ion_secure_heap.
 *
  * @heap_id - heap id to secure.
 *
 * Secure a heap
 * Returns 0 on success
 */
int msm_ion_secure_heap(int heap_id);

/**
 * msm_ion_unsecure_heap - unsecure a heap. Wrapper around ion_unsecure_heap.
 *
  * @heap_id - heap id to secure.
 *
 * Un-secure a heap
 * Returns 0 on success
 */
int msm_ion_unsecure_heap(int heap_id);

/**
 * msm_ion_secure_heap_2_0 - secure a heap using 2.0 APIs
 *  Wrapper around ion_secure_heap.
 *
 * @heap_id - heap id to secure.
 * @usage - usage hint to TZ
 *
 * Secure a heap
 * Returns 0 on success
 */
int msm_ion_secure_heap_2_0(int heap_id, enum cp_mem_usage usage);

/**
 * msm_ion_unsecure_heap - unsecure a heap secured with 3.0 APIs.
 * Wrapper around ion_unsecure_heap.
 *
 * @heap_id - heap id to secure.
 * @usage - usage hint to TZ
 *
 * Un-secure a heap
 * Returns 0 on success
 */
int msm_ion_unsecure_heap_2_0(int heap_id, enum cp_mem_usage usage);
#else
static inline int msm_ion_secure_heap(int heap_id)
{
	return -ENODEV;

}

static inline int msm_ion_unsecure_heap(int heap_id)
{
	return -ENODEV;
}

static inline int msm_ion_secure_heap_2_0(int heap_id, enum cp_mem_usage usage)
{
	return -ENODEV;
}

static inline int msm_ion_unsecure_heap_2_0(int heap_id,
					enum cp_mem_usage usage)
{
	return -ENODEV;
}
#endif /* CONFIG_ION */

#endif

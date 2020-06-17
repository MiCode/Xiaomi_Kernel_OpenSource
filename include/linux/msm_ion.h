/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_ION_H
#define _MSM_ION_H

#include <linux/bitmap.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <uapi/linux/msm_ion.h>

struct ion_prefetch_region {
	u64 size;
	u32 vmid;
};

#if IS_ENABLED(CONFIG_ION_MSM_HEAPS)

struct device *msm_ion_heap_device_by_id(int heap_id);

static inline unsigned int ion_get_flags_num_vm_elems(unsigned int flags)
{
	unsigned long vm_flags = flags & ION_FLAGS_CP_MASK;

	return ((unsigned int)bitmap_weight(&vm_flags, BITS_PER_LONG));
}

int ion_populate_vm_list(unsigned long flags, unsigned int *vm_list,
			 int nelems);

int msm_ion_heap_prefetch(int heap_id, struct ion_prefetch_region *regions,
			  int nr_regions);

int msm_ion_heap_drain(int heap_id, struct ion_prefetch_region *regions,
		       int nr_regions);

int get_ion_flags(u32 vmid);

bool msm_ion_heap_is_secure(int heap_id);

int msm_ion_heap_add_memory(int heap_id, struct sg_table *sgt);

int msm_ion_heap_remove_memory(int heap_id, struct sg_table *sgt);

int msm_ion_dma_buf_lock(struct dma_buf *dmabuf);

void msm_ion_dma_buf_unlock(struct dma_buf *dmabuf);

#else

static inline struct device *msm_ion_heap_device_by_id(int heap_id)
{
	return ERR_PTR(-ENODEV);
}

static inline unsigned int ion_get_flags_num_vm_elems(unsigned int flags)
{
	return 0;
}

static inline int ion_populate_vm_list(unsigned long flags,
				       unsigned int *vm_list, int nelems)
{
	return -EINVAL;
}

static inline int msm_ion_heap_prefetch(int heap_id,
					struct ion_prefetch_region *regions,
					int nr_regions)
{
	return -ENODEV;
}

static inline int msm_ion_heap_drain(int heap_id,
				     struct ion_prefetch_region *regions,
				     int nr_regions)
{
	return -ENODEV;
}

static inline int get_ion_flags(u32 vmid)
{
	return -EINVAL;
}

static inline bool msm_ion_heap_is_secure(int heap_id)
{
	return false;
}

static inline int msm_ion_heap_add_memory(int heap_id, struct sg_table *sgt)
{
	return -ENODEV;
}

static inline int msm_ion_heap_remove_memory(int heap_id, struct sg_table *sgt)
{
	return -ENODEV;
}

static inline int msm_ion_dma_buf_lock(struct dma_buf *dmabuf)
{
	return -ENODEV;
}

static inline void msm_ion_dma_buf_unlock(struct dma_buf *dmabuf)
{
}

#endif /* CONFIG_ION_MSM_HEAPS */
#endif /* _MSM_ION_H */

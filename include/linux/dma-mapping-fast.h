/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#ifndef __LINUX_DMA_MAPPING_FAST_H
#define __LINUX_DMA_MAPPING_FAST_H

#include <linux/iommu.h>
#include <linux/io-pgtable-fast.h>

struct dma_iommu_mapping;
struct io_pgtable_ops;

struct dma_fast_smmu_mapping {
	struct device		*dev;
	struct iommu_domain	*domain;
	dma_addr_t	 base;
	size_t		 size;
	size_t		 num_4k_pages;

	u32		min_iova_align;
	struct page	*guard_page;

	unsigned int	bitmap_size;
	unsigned long	*bitmap;
	unsigned long	next_start;
	unsigned long	upcoming_stale_bit;
	bool		have_stale_tlbs;

	dma_addr_t	pgtbl_dma_handle;
	struct io_pgtable_ops *pgtbl_ops;

	spinlock_t	lock;
	struct notifier_block notifier;
};

#ifdef CONFIG_IOMMU_IO_PGTABLE_FAST
int fast_smmu_init_mapping(struct device *dev,
			    struct dma_iommu_mapping *mapping);
void fast_smmu_release_mapping(struct kref *kref);
#else
static inline int fast_smmu_init_mapping(struct device *dev,
					  struct dma_iommu_mapping *mapping)
{
	return -ENODEV;
}

static inline void fast_smmu_release_mapping(struct kref *kref)
{
}
#endif

#endif /* __LINUX_DMA_MAPPING_FAST_H */

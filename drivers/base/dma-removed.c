/*
 *
 *  Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *  Copyright (C) 2000-2004 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/sizes.h>

#define NO_KERNEL_MAPPING_DUMMY	0x2222

void *removed_alloc(struct device *dev, size_t size, dma_addr_t *handle,
		    gfp_t gfp, struct dma_attrs *attrs)
{
	bool no_kernel_mapping = dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING,
					attrs);
	unsigned long pfn;
	unsigned long order = get_order(size);
	void *addr = NULL;

	size = PAGE_ALIGN(size);

	if (!(gfp & __GFP_WAIT))
		return NULL;

	pfn = dma_alloc_from_contiguous(dev, size >> PAGE_SHIFT, order);

	if (pfn) {
		addr = ioremap(__pfn_to_phys(pfn), size);
		if (WARN_ON(!addr)) {
			dma_release_from_contiguous(dev, pfn, order);
		} else {
			memset(addr, 0, size);
			if (no_kernel_mapping) {
				iounmap(addr);
				addr = (void *)NO_KERNEL_MAPPING_DUMMY;
			}
			*handle = __pfn_to_phys(pfn);
		}
	}

	return addr;
}


int removed_mmap(struct device *dev, struct vm_area_struct *vma,
		 void *cpu_addr, dma_addr_t dma_addr, size_t size,
		 struct dma_attrs *attrs)
{
	return -ENXIO;
}

void removed_free(struct device *dev, size_t size, void *cpu_addr,
		  dma_addr_t handle, struct dma_attrs *attrs)
{
	bool no_kernel_mapping = dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING,
					attrs);

	if (!no_kernel_mapping)
		iounmap(cpu_addr);
	dma_release_from_contiguous(dev, __phys_to_pfn(handle),
					size >> PAGE_SHIFT);
}

static dma_addr_t removed_map_page(struct device *dev, struct page *page,
			unsigned long offset, size_t size,
			enum dma_data_direction dir,
			struct dma_attrs *attrs)
{
	return ~(dma_addr_t)0;
}

static void removed_unmap_page(struct device *dev, dma_addr_t dma_handle,
		size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	return;
}

static int removed_map_sg(struct device *dev, struct scatterlist *sg,
			int nents, enum dma_data_direction dir,
			struct dma_attrs *attrs)
{
	return 0;
}

static void removed_unmap_sg(struct device *dev,
			struct scatterlist *sg, int nents,
			enum dma_data_direction dir,
			struct dma_attrs *attrs)
{
	return;
}

static void removed_sync_single_for_cpu(struct device *dev,
			dma_addr_t dma_handle, size_t size,
			enum dma_data_direction dir)
{
	return;
}

void removed_sync_single_for_device(struct device *dev,
			dma_addr_t dma_handle, size_t size,
			enum dma_data_direction dir)
{
	return;
}

void removed_sync_sg_for_cpu(struct device *dev,
			struct scatterlist *sg, int nents,
			enum dma_data_direction dir)
{
	return;
}

void removed_sync_sg_for_device(struct device *dev,
			struct scatterlist *sg, int nents,
			enum dma_data_direction dir)
{
	return;
}

struct dma_map_ops removed_dma_ops = {
	.alloc			= removed_alloc,
	.free			= removed_free,
	.mmap			= removed_mmap,
	.map_page		= removed_map_page,
	.unmap_page		= removed_unmap_page,
	.map_sg			= removed_map_sg,
	.unmap_sg		= removed_unmap_sg,
	.sync_single_for_cpu	= removed_sync_single_for_cpu,
	.sync_single_for_device	= removed_sync_single_for_device,
	.sync_sg_for_cpu	= removed_sync_sg_for_cpu,
	.sync_sg_for_device	= removed_sync_sg_for_device,
};
EXPORT_SYMBOL(removed_dma_ops);



/*
 *
 *  Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 *  Copyright (C) 2000-2004 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/ioport.h>
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
#include <linux/spinlock.h>
#include <asm/dma-contiguous.h>
#include <asm/tlbflush.h>

struct removed_region {
	phys_addr_t	base;
	int		nr_pages;
	unsigned long	*bitmap;
	int		fixup;
	struct mutex	lock;
};

#define NO_KERNEL_MAPPING_DUMMY	0x2222

static int dma_init_removed_memory(phys_addr_t phys_addr, size_t size,
				struct removed_region **mem)
{
	struct removed_region *dma_mem = NULL;
	int pages = size >> PAGE_SHIFT;
	int bitmap_size = BITS_TO_LONGS(pages) * sizeof(long);

	dma_mem = kzalloc(sizeof(struct removed_region), GFP_KERNEL);
	if (!dma_mem)
		goto out;
	dma_mem->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!dma_mem->bitmap)
		goto free1_out;

	dma_mem->base = phys_addr;
	dma_mem->nr_pages = pages;
	mutex_init(&dma_mem->lock);

	*mem = dma_mem;

	return 0;

free1_out:
	kfree(dma_mem);
out:
	return -ENOMEM;
}

static int dma_assign_removed_region(struct device *dev,
					struct removed_region *mem)
{
	if (dev->removed_mem)
		return -EBUSY;

	dev->removed_mem = mem;
	return 0;
}

static void adapt_iomem_resource(unsigned long base_pfn, unsigned long end_pfn)
{
	struct resource *res, *conflict;
	resource_size_t cstart, cend;

	res = kzalloc(sizeof(*res), GFP_KERNEL);
	if (!res)
		return;

	res->name  = "System RAM";
	res->start = __pfn_to_phys(base_pfn);
	res->end = __pfn_to_phys(end_pfn) - 1;
	res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;

	conflict = request_resource_conflict(&iomem_resource, res);
	if (!conflict) {
		pr_err("Removed memory: no conflict resource found\n");
		kfree(res);
		goto done;
	}

	cstart = conflict->start;
	cend = conflict->end;
	if ((cstart == res->start) && (cend == res->end)) {
		release_resource(conflict);
	} else if ((res->start >= cstart) && (res->start <= cend)) {
		if (res->start == cstart) {
			adjust_resource(conflict, res->end + 1,
					cend - res->end);
		} else if (res->end == cend) {
			adjust_resource(conflict, cstart,
					res->start - cstart);
		} else {
			adjust_resource(conflict, cstart,
					res->start - cstart);
			res->start = res->end + 1;
			res->end = cend;
			request_resource(&iomem_resource, res);
			goto done;
		}
	} else {
		pr_err("Removed memory: incorrect resource conflict start=%llx end=%llx\n",
				(unsigned long long) conflict->start,
				(unsigned long long) conflict->end);
	}

	kfree(res);
done:
	return;
}

#ifdef CONFIG_FLATMEM
static void free_memmap(unsigned long start_pfn, unsigned long end_pfn)
{
	struct page *start_pg, *end_pg;
	unsigned long pg, pgend;

	start_pfn = ALIGN(start_pfn, pageblock_nr_pages);
	end_pfn = round_down(end_pfn, pageblock_nr_pages);
	/*
	 * Convert start_pfn/end_pfn to a struct page pointer.
	 */
	start_pg = pfn_to_page(start_pfn - 1) + 1;
	end_pg = pfn_to_page(end_pfn - 1) + 1;

	/*
	 * Convert to physical addresses, and round start upwards and end
	 * downwards.
	 */
	pg = (unsigned long)PAGE_ALIGN(__pa(start_pg));
	pgend = (unsigned long)__pa(end_pg) & PAGE_MASK;

	/*
	 * If there are free pages between these, free the section of the
	 * memmap array.
	 */
	if (pg < pgend)
		free_bootmem_late(pg, pgend - pg);
}
#else
static void free_memmap(unsigned long start_pfn, unsigned long end_pfn)
{
}
#endif

static int _clear_pte(pte_t *pte, pgtable_t token, unsigned long addr,
			    void *data)
{
	pte_clear(&init_mm, addr, pte);
	return 0;
}

static void clear_mapping(unsigned long addr, unsigned long size)
{
	apply_to_page_range(&init_mm, addr, size, _clear_pte, NULL);
	/* ensure ptes are updated */
	mb();
	flush_tlb_kernel_range(addr, addr + size);
}

static void removed_region_fixup(struct removed_region *dma_mem, int index)
{
	unsigned long fixup_size;
	unsigned long base_pfn;
	unsigned long flags;

	if (index > dma_mem->nr_pages)
		return;

	/* carve-out */
	flags = memblock_region_resize_late_begin();
	memblock_free(dma_mem->base, dma_mem->nr_pages * PAGE_SIZE);
	memblock_remove(dma_mem->base, index * PAGE_SIZE);
	memblock_region_resize_late_end(flags);

	/* clear page-mappings */
	base_pfn = dma_mem->base >> PAGE_SHIFT;
	if (!PageHighMem(pfn_to_page(base_pfn))) {
		clear_mapping((unsigned long) phys_to_virt(dma_mem->base),
				index * PAGE_SIZE);
	}

	/* free page objects */
	free_memmap(base_pfn, base_pfn + index);

	/* return remaining area to system */
	fixup_size = (dma_mem->nr_pages - index) * PAGE_SIZE;
	free_bootmem_late(dma_mem->base + index * PAGE_SIZE, fixup_size);

	/*
	 * release freed resource region so as to show up under iomem resource
	 * list
	 */
	adapt_iomem_resource(base_pfn, base_pfn + index);

	/* limit the fixup region */
	dma_mem->nr_pages = index;
}

void *removed_alloc(struct device *dev, size_t size, dma_addr_t *handle,
		    gfp_t gfp, struct dma_attrs *attrs)
{
	bool no_kernel_mapping = dma_get_attr(DMA_ATTR_NO_KERNEL_MAPPING,
					attrs);
	bool skip_zeroing = dma_get_attr(DMA_ATTR_SKIP_ZEROING, attrs);
	int pageno;
	unsigned long order;
	void *addr = NULL;
	struct removed_region *dma_mem = dev->removed_mem;
	int nbits;
	unsigned int align;

	if (!gfpflags_allow_blocking(gfp))
		return NULL;

	size = PAGE_ALIGN(size);
	nbits = size >> PAGE_SHIFT;
	order = get_order(size);

	if (order > get_order(SZ_1M))
		order = get_order(SZ_1M);

	align = (1 << order) - 1;


	mutex_lock(&dma_mem->lock);
	pageno = bitmap_find_next_zero_area(dma_mem->bitmap, dma_mem->nr_pages,
						0, nbits, align);

	if (pageno < dma_mem->nr_pages) {
		phys_addr_t base = dma_mem->base + pageno * PAGE_SIZE;
		*handle = base;

		bitmap_set(dma_mem->bitmap, pageno, nbits);

		if (dma_mem->fixup) {
			removed_region_fixup(dma_mem, pageno + nbits);
			dma_mem->fixup = 0;
		}

		if (no_kernel_mapping && skip_zeroing) {
			addr = (void *)NO_KERNEL_MAPPING_DUMMY;
			goto out;
		}

		addr = ioremap(base, size);
		if (WARN_ON(!addr)) {
			bitmap_clear(dma_mem->bitmap, pageno, nbits);
		} else {
			if (!skip_zeroing)
				memset_io(addr, 0, size);
			if (no_kernel_mapping) {
				iounmap(addr);
				addr = (void *)NO_KERNEL_MAPPING_DUMMY;
			}
			*handle = base;
		}
	}

out:
	mutex_unlock(&dma_mem->lock);
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
	struct removed_region *dma_mem = dev->removed_mem;

	size = PAGE_ALIGN(size);
	if (!no_kernel_mapping)
		iounmap(cpu_addr);
	mutex_lock(&dma_mem->lock);
	bitmap_clear(dma_mem->bitmap, (handle - dma_mem->base) >> PAGE_SHIFT,
				size >> PAGE_SHIFT);
	mutex_unlock(&dma_mem->lock);
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

void *removed_remap(struct device *dev, void *cpu_addr, dma_addr_t handle,
			size_t size, struct dma_attrs *attrs)
{
	return ioremap(handle, size);
}

void removed_unremap(struct device *dev, void *remapped_address, size_t size)
{
	iounmap(remapped_address);
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
	.remap			= removed_remap,
	.unremap		= removed_unremap,
};
EXPORT_SYMBOL(removed_dma_ops);

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>

static int rmem_dma_device_init(struct reserved_mem *rmem, struct device *dev)
{
        struct removed_region *mem = rmem->priv;
        if (!mem && dma_init_removed_memory(rmem->base, rmem->size, &mem)) {
                pr_info("Reserved memory: failed to init DMA memory pool at %pa, size %ld MiB\n",
                        &rmem->base, (unsigned long)rmem->size / SZ_1M);
                return -EINVAL;
        }
	mem->fixup = rmem->fixup;
	set_dma_ops(dev, &removed_dma_ops);
        rmem->priv = mem;
        dma_assign_removed_region(dev, mem);
	return 0;
}

static void rmem_dma_device_release(struct reserved_mem *rmem,
                                    struct device *dev)
{
        dev->dma_mem = NULL;
}

static const struct reserved_mem_ops removed_mem_ops = {
        .device_init    = rmem_dma_device_init,
        .device_release = rmem_dma_device_release,
};

static int __init removed_dma_setup(struct reserved_mem *rmem)
{
	unsigned long node = rmem->fdt_node;
	int nomap, fixup;

	nomap = of_get_flat_dt_prop(node, "no-map", NULL) != NULL;
	fixup = of_get_flat_dt_prop(node, "no-map-fixup", NULL) != NULL;

	if (nomap && fixup) {
		pr_err("Removed memory: nomap & nomap-fixup can't co-exist\n");
		return -EINVAL;
	}

	rmem->fixup = fixup;
	if (rmem->fixup) {
		/* Architecture specific contiguous memory fixup only for
		 * no-map-fixup to split mappings
		 */
		dma_contiguous_early_fixup(rmem->base, rmem->size);
	}

        rmem->ops = &removed_mem_ops;
        pr_info("Removed memory: created DMA memory pool at %pa, size %ld MiB\n",
                &rmem->base, (unsigned long)rmem->size / SZ_1M);
        return 0;
}
RESERVEDMEM_OF_DECLARE(dma, "removed-dma-pool", removed_dma_setup);
#endif

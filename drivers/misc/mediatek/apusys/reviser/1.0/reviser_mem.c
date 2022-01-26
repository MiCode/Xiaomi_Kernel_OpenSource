/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <asm/mman.h>

#include "reviser_cmn.h"
#include "reviser_mem.h"
#include "reviser_ioctl.h"
#include "reviser_reg.h"

static int __reviser_get_sgt(const char *buf,
		size_t len, struct sg_table *sgt)
{
	struct page **pages = NULL;
	unsigned int nr_pages;
	unsigned int index;
	const char *p;
	int ret;

	nr_pages = DIV_ROUND_UP((unsigned long)buf + len, PAGE_SIZE)
		- ((unsigned long)buf / PAGE_SIZE);
	pages = kmalloc_array(nr_pages, sizeof(struct page *), GFP_KERNEL);

	if (!pages)
		return -ENOMEM;

	p = buf - offset_in_page(buf);
	LOG_DEBUG("start p: %llx buf: %llx\n",
			(uint64_t)p, (uint64_t)buf);

	for (index = 0; index < nr_pages; index++) {
		if (is_vmalloc_addr(p))
			pages[index] = vmalloc_to_page(p);
		else
			pages[index] = kmap_to_page((void *)p);
		if (!pages[index]) {
			kfree(pages);
			LOG_ERR("map failed\n");
			return -EFAULT;
		}
		p += PAGE_SIZE;
		//LOG_DEBUG("p: %llx PAGE_SIZE: %llx\n",
		//		(uint64_t)p, (uint64_t)PAGE_SIZE);
	}


	ret = sg_alloc_table_from_pages(sgt, pages, index,
		offset_in_page(buf), len, GFP_KERNEL);
	kfree(pages);
	if (ret) {
		LOG_ERR("sg_alloc_table_from_pages: %d\n", ret);
		return ret;
	}



	LOG_DEBUG("buf: %p, len: %lx, sgt: %p nr_pages: %d\n",
		buf, len, sgt, nr_pages);

	return 0;
}

static dma_addr_t __reviser_get_iova(struct device *dev,
		struct scatterlist *sg, unsigned int nents,	size_t len,
		dma_addr_t given_iova)
{
	dma_addr_t mask;
	dma_addr_t iova = 0;
	int ret;

	mask = (given_iova + len - 1) | BOUNDARY_MASK;
	given_iova |= BOUNDARY_MASK;

	LOG_DEBUG("mask: %llx given_iova: %llx\n",
			(uint64_t)mask, (uint64_t)given_iova);

	dma_set_mask_and_coherent(dev, mask);

	ret = dma_map_sg_attrs(dev, sg, nents,
		DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);

	if (ret <= 0) {
		LOG_ERR("dma_map_sg_attrs: failed with %d\n", ret);
		return 0;
	}

	iova = sg_dma_address(&sg[0]);

	LOG_DEBUG("sg_dma_address: size: %lx, mapped iova: 0x%llx\n",
		len, (uint64_t)iova);


	return iova;

}

int reviser_mem_free(struct reviser_mem *mem)
{
	kfree((void *) mem->kva);
	LOG_DEBUG("Done\n");
	return 0;
}

int reviser_mem_alloc(struct device *dev, struct reviser_mem *mem)
{
	int ret = 0;
	void *kva;
	dma_addr_t iova;

	kva = kvmalloc(mem->size, GFP_KERNEL);

	if (!kva) {
		dev_info(dev, "%s: kvmalloc: failed\n",
			__func__);
		ret = -ENOMEM;
		goto error;
	}
	memset((void *)kva, 0, mem->size);

	if (__reviser_get_sgt(kva, mem->size, &mem->sgt)) {
		dev_info(dev, "%s: __reviser_get_sgt: failed\n",
					__func__);
		ret = -ENOMEM;
		goto error;
	}

	iova = __reviser_get_iova(dev, mem->sgt.sgl, mem->sgt.nents,
			mem->size, REMAP_DRAM_BASE);

	if ((!iova) || ((uint32_t)iova != REMAP_DRAM_BASE)) {
		LOG_ERR("iova wrong (0x%llx)\n", iova);
		goto error;
	}

	/*
	 * Avoid a kmemleak false positive.
	 * The pointer is using for debugging,
	 * but it will be used by other apusys HW
	 */
	kmemleak_no_scan(kva);

	mem->kva = (uint64_t)kva;
	mem->iova = (uint32_t)iova;

	LOG_INFO("mem(0x%x/%d/0x%llx)\n",
			mem->iova, mem->size, mem->kva);

	goto out;

error:
	kvfree(kva);
out:
	return ret;

}

int reviser_mem_invalidate(struct device *dev, struct reviser_mem *mem)
{
	dma_sync_sg_for_cpu(dev, mem->sgt.sgl, mem->sgt.nents,
		DMA_FROM_DEVICE);

	return 0;
}







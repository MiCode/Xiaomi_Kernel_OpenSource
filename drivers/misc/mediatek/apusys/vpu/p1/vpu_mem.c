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

#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <mt-plat/aee.h>

#include "vpu_cfg.h"
#include "vpu_mem.h"
#include "vpu_debug.h"

static int vpu_map_kva_to_sgt(
	const char *buf, size_t len, struct sg_table *sgt);

static dma_addr_t vpu_map_sg_to_iova(
	struct platform_device *pdev, struct scatterlist *sg,
	unsigned int nents, size_t len, dma_addr_t given_iova);

static void vpu_dump_sg(struct scatterlist *s, unsigned int nents)
{
	unsigned int i;

	if (!s || !vpu_debug_on(VPU_DBG_MEM))
		return;

	for (i = 0; i < nents; i++) {
		struct page *p = sg_page(&s[i]);
		phys_addr_t phys = page_to_phys(p);

		pr_info("%s: sg[%d]: pfn: %lx, pa: %lx, len: %lx, dma_addr: %lx\n",
			__func__, i,
			(unsigned long) page_to_pfn(p),
			(unsigned long) phys,
			(unsigned long) s[i].length,
			(unsigned long) s[i].dma_address);
	}
}

static void vpu_dump_sgt(struct sg_table *sgt)
{
	if (!sgt || !sgt->sgl)
		return;

	vpu_dump_sg(sgt->sgl, sgt->nents);
}

static int
vpu_mem_alloc(struct platform_device *pdev,
	struct vpu_iova *i, dma_addr_t given_iova)
{
	int ret = 0;
	void *kva;
	dma_addr_t iova;

	if (!i) {
		ret = -EINVAL;
		goto out;
	}

	vpu_mem_debug("%s: size: 0x%x, given iova: 0x%llx (%s alloc)\n",
		__func__, i->size, (u64)given_iova,
		(given_iova == VPU_IOVA_END) ? "dynamic" : "static");

	kva = kvmalloc(i->size, GFP_KERNEL);

	if (!kva) {
		dev_info(&pdev->dev, "%s: kvmalloc: failed\n",
			__func__);
		ret = -ENOMEM;
		goto error;
	}

	vpu_mem_debug("%s: kvmalloc: %llx\n", __func__, (uint64_t)kva);

	ret = vpu_map_kva_to_sgt(kva, i->size, &i->sgt);

	if (ret)
		goto error;

	iova = vpu_map_sg_to_iova(pdev, i->sgt.sgl, i->sgt.nents,
		i->size, given_iova);

	if (!iova)
		goto error;

	i->m.va = (uint64_t)kva;
	i->m.pa = (uint32_t)iova;
	i->m.length = i->size;

	goto out;
error:
	kvfree(kva);
out:
	return ret;
}

void vpu_mem_free(struct vpu_mem *m)
{
	kvfree((void *)m->va);
}

static int
vpu_map_kva_to_sgt(const char *buf, size_t len, struct sg_table *sgt)
{
	struct page **pages = NULL;
	unsigned int nr_pages;
	unsigned int index;
	const char *p;
	int ret;

	vpu_mem_debug("%s: buf: %p, len: %lx, sgt: %p\n",
		__func__, buf, len, sgt);

	nr_pages = DIV_ROUND_UP((unsigned long)buf + len, PAGE_SIZE)
		- ((unsigned long)buf / PAGE_SIZE);
	pages = kmalloc_array(nr_pages, sizeof(struct page *), GFP_KERNEL);

	if (!pages)
		return -ENOMEM;

	p = buf - offset_in_page(buf);

	for (index = 0; index < nr_pages; index++) {
		if (is_vmalloc_addr(p))
			pages[index] = vmalloc_to_page(p);
		else
			pages[index] = kmap_to_page((void *)p);
		if (!pages[index]) {
			pr_info("%s: map failed\n", __func__);
			ret = -EFAULT;
			goto out;
		}
		p += PAGE_SIZE;
	}

	vpu_mem_debug("%s: nr_pages: %d\n", __func__, nr_pages);

	ret = sg_alloc_table_from_pages(sgt, pages, index,
		offset_in_page(buf), len, GFP_KERNEL);

	if (ret) {
		pr_info("%s: sg_alloc_table_from_pages: %d\n",
			__func__, ret);
		goto out;
	}

	vpu_dump_sgt(sgt);
out:
	kfree(pages);
	return ret;
}

static dma_addr_t
vpu_map_sg_to_iova(
	struct platform_device *pdev, struct scatterlist *sg,
	unsigned int nents, size_t len, dma_addr_t given_iova)
{
	dma_addr_t mask;
	dma_addr_t iova = 0;
	bool dyn_alloc = false;
	bool match = false;
	int ret;

	if (!sg)
		return 0;

	if (given_iova >= VPU_IOVA_END) {
		dyn_alloc = true;
		mask = (VPU_IOVA_END - 1) | VPU_IOVA_BANK;
		given_iova |= VPU_IOVA_BANK;
		vpu_mem_debug("%s: dev: %p, len: %zx, given_iova mask: %llx (dynamic alloc)\n",
			__func__, &pdev->dev,
			len, (u64)given_iova);
	} else {
		mask = (given_iova + len - 1) | VPU_IOVA_BANK;
		given_iova |= VPU_IOVA_BANK;
		vpu_mem_debug("%s: dev: %p, len: %zx, given_iova start ~ end(mask): %llx ~ %llx\n",
			__func__, &pdev->dev,
			len, (u64)given_iova, (u64)mask);
	}

	dma_set_mask_and_coherent(&pdev->dev, mask);

	ret = dma_map_sg_attrs(&pdev->dev, sg, nents,
		DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);

	if (ret <= 0) {
		dev_info(&pdev->dev,
			"%s: dma_map_sg_attrs: failed with %d\n",
			__func__, ret);
		return 0;
	}

	iova = sg_dma_address(&sg[0]);

	if (given_iova == iova)
		match = true;

	dev_info(&pdev->dev,
		"%s: sg_dma_address: size: %lx, mapped iova: 0x%llx %s\n",
		__func__, len, (u64)iova,
		dyn_alloc ? "(dynamic alloc)" :
		(match ? "(static alloc)" : "(unexpected)"));

	if (!dyn_alloc && !match)
		vpu_aee_warn("VPU", "iova mapping error");

	return iova;
}

static dma_addr_t
vpu_map_to_iova(struct platform_device *pdev, void *addr, size_t len,
	dma_addr_t given_iova, struct sg_table *sgt)
{
	dma_addr_t iova = 0;
	int ret;

	if (!sgt)
		goto out;

	ret = vpu_map_kva_to_sgt(addr, len, sgt);

	if (ret)
		goto out;

	iova = vpu_map_sg_to_iova(pdev, sgt->sgl, sgt->nents, len, given_iova);
out:
	return iova;
}

dma_addr_t vpu_iova_alloc(struct platform_device *pdev,
	struct vpu_iova *i)
{
	int ret = 0;
	dma_addr_t iova = 0;
	/* mt6885, mt6873 maps va to iova */
	unsigned long base = (unsigned long)vpu_drv->bin_va;

	if (!pdev || !i || !i->size)
		goto out;

	iova = i->addr ? i->addr : VPU_IOVA_END;

	i->sgt.sgl = NULL;
	i->m.handle = NULL;
	i->m.va = 0;
	i->m.pa = 0;
	i->m.length = 0;

	/* allocate kvm and map */
	if (i->bin == VPU_MEM_ALLOC) {
		ret = vpu_mem_alloc(pdev, i, iova);
		iova = i->m.pa;
	/* map from vpu firmware loaded at bootloader */
	} else if (i->size) {
		iova = vpu_map_to_iova(pdev,
			(void *)(base + i->bin), i->size, iova,
			&i->sgt);
	} else {
		dev_info(&pdev->dev,
			"%s: unknown setting (%x, %x, %x)\n",
			__func__, i->addr, i->bin, i->size);
		iova = 0;
	}

out:
	return iova;
}

void vpu_iova_free(struct device *dev, struct vpu_iova *i)
{
	vpu_mem_free(&i->m);
	if (i->sgt.sgl) {
		dma_unmap_sg_attrs(dev, i->sgt.sgl,
				   i->sgt.nents, DMA_BIDIRECTIONAL,
				   DMA_ATTR_SKIP_CPU_SYNC);
		sg_free_table(&i->sgt);
	}
}

void vpu_iova_sync_for_device(struct device *dev,
	struct vpu_iova *i)
{
	dma_sync_sg_for_device(dev, i->sgt.sgl, i->sgt.nents,
		DMA_TO_DEVICE);
}

void vpu_iova_sync_for_cpu(struct device *dev,
	struct vpu_iova *i)
{
	dma_sync_sg_for_cpu(dev, i->sgt.sgl, i->sgt.nents,
		DMA_FROM_DEVICE);
}

int vpu_iova_dts(struct platform_device *pdev,
	const char *name, struct vpu_iova *i)
{
	if (of_property_read_u32_array(pdev->dev.of_node,
			name, &i->addr, 3)) {
		dev_info(&pdev->dev, "%s: vpu: unable to get %s\n",
			__func__, name);
		return -ENODEV;
	}

	dev_info(&pdev->dev, "%s: %s: addr: %08xh, size: %08xh, bin: %08xh\n",
		__func__, name, i->addr, i->size, i->bin);

	return 0;
}


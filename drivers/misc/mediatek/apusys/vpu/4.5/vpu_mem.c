// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/bitmap.h>
#include <mt-plat/aee.h>

#include "apu_bmap.h"
#include "vpu_cfg.h"
#include "vpu_mem.h"
#include "vpu_debug.h"

static int vpu_mem_init_v1(void)
{
	return 0;
}

static void vpu_mem_exit_v1(void)
{
}

static int vpu_mem_init_v2(void)
{
	struct vpu_config *cfg = vpu_drv->vp->cfg;

	vpu_drv->ab.au = PAGE_SIZE;
	vpu_drv->ab.start = cfg->iova_start;
	vpu_drv->ab.end = cfg->iova_end;
	apu_bmap_init(&vpu_drv->ab, "vpu_mem");

	return 0;
}

static void vpu_mem_exit_v2(void)
{
	apu_bmap_exit(&vpu_drv->ab);
}

static struct page **vpu_map_kva_to_sgt(
	const char *buf, size_t len, struct sg_table *sgt);

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
	struct vpu_config *cfg = vpu_drv->vp->cfg;
	struct vpu_mem_ops *mops = vpu_drv->vp->mops;

	if (!i) {
		ret = -EINVAL;
		goto out;
	}

	vpu_mem_debug("%s: size: 0x%x, given iova: 0x%llx (%s alloc)\n",
		__func__, i->size, (u64)given_iova,
		(given_iova == cfg->iova_end) ? "dynamic" : "static");

	kva = kvmalloc(i->size, GFP_KERNEL);

	if (!kva) {
		ret = -ENOMEM;
		goto error;
	}

	vpu_mem_debug("%s: kvmalloc: %llx\n", __func__, (uint64_t)kva);

	i->pages = vpu_map_kva_to_sgt(kva, i->size, &i->sgt);

	if (IS_ERR_OR_NULL(i->pages)) {
		ret = IS_ERR(i->pages) ? PTR_ERR(i->pages) : -ENOMEM;
		i->pages = NULL;
		goto error;
	}

	iova = mops->map_sg_to_iova(pdev, i->sgt.sgl, i->sgt.nents,
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

static struct page **
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
		return ERR_PTR(-ENOMEM);

	p = buf - offset_in_page(buf);

	for (index = 0; index < nr_pages; index++) {
		if (is_vmalloc_addr(p))
			pages[index] = vmalloc_to_page(p);
		else
			pages[index] = kmap_to_page((void *)p);
		if (!pages[index]) {
			kfree(pages);
			pr_info("%s: map failed\n", __func__);
			return ERR_PTR(-EFAULT);
		}
		p += PAGE_SIZE;
	}

	vpu_mem_debug("%s: nr_pages: %d\n", __func__, nr_pages);

	ret = sg_alloc_table_from_pages(sgt, pages, index,
		offset_in_page(buf), len, GFP_KERNEL);

	if (ret) {
		pr_info("%s: sg_alloc_table_from_pages: %d\n",
			__func__, ret);
		kfree(pages);
		return ERR_PTR(ret);
	}

	vpu_dump_sgt(sgt);

	return pages;
}

static dma_addr_t
vpu_map_sg_to_iova_v2(
	struct platform_device *pdev, struct scatterlist *sg,
	unsigned int nents,	size_t len, dma_addr_t given_iova)
{
	struct iommu_domain *domain;
	dma_addr_t iova = 0;
	size_t size = 0;
	struct vpu_config *cfg = vpu_drv->vp->cfg;
	int prot = IOMMU_READ | IOMMU_WRITE;
	uint32_t addr;
	u64 bank = cfg->iova_bank;

	domain = iommu_get_domain_for_dev(&pdev->dev);

	if (given_iova < cfg->iova_end) {
		vpu_mem_debug("%s: dev: %p, len: %zx, given_iova: %llx (static alloc)\n",
			__func__, &pdev->dev, len, (u64)given_iova);

		iova = apu_bmap_alloc(&vpu_drv->ab, len, given_iova);
		if (!iova)
			goto err;

		if (iova != given_iova) {
			dev_info(&pdev->dev,
				"%s: given iova: %llx, apu_bmap_alloc returned: %llx\n",
				__func__, (u64)given_iova, (u64)iova);
			goto err;
		}
	} else {
		vpu_mem_debug("%s: dev: %p, len: %zx, given_iova: %llx (dynamic alloc)\n",
			__func__, &pdev->dev, len, (u64)given_iova);
		iova = apu_bmap_alloc(&vpu_drv->ab, len, 0);
		if (!iova)
			goto err;
	}

	iova = iova | bank;
	vpu_mem_debug("%s: dev: %p, len: %zx, iova: %llx\n",
		__func__, &pdev->dev, len, (u64)iova);

	size = iommu_map_sg(domain, iova, sg, nents, prot);

	if (size == 0) {
		dev_info(&pdev->dev,
			"%s: iommu_map_sg: len: %zx, iova: %llx, failed\n",
			__func__, len, (u64)iova, nents);
		goto err;
	} else if (size != len) {
		dev_info(&pdev->dev,
			"%s: iommu_map_sg: len: %zx, iova: %llx, mismatch with mapped size: %zx\n",
			__func__, len, (u64)iova, size);
		goto err;
	}

	return iova;

err:
	if (iova)
		apu_bmap_free(&vpu_drv->ab, len, iova);

	return 0;
}


static dma_addr_t
vpu_map_sg_to_iova_v1(
	struct platform_device *pdev, struct scatterlist *sg,
	unsigned int nents,	size_t len, dma_addr_t given_iova)
{
	dma_addr_t mask;
	dma_addr_t iova = 0;
	bool dyn_alloc = false;
	bool match = false;
	int ret;
	struct vpu_config *cfg = vpu_drv->vp->cfg;
	u64 bank = cfg->iova_bank;
	u32 iova_end = cfg->iova_end;

	if (!sg)
		return 0;

	if (given_iova >= iova_end) {
		dyn_alloc = true;
		mask = (iova_end - 1) | bank;
		given_iova |= bank;
		vpu_mem_debug("%s: dev: %p, len: %zx, given_iova mask: %llx (dynamic alloc)\n",
			__func__, &pdev->dev,
			len, (u64)given_iova);
	} else {
		mask = (given_iova + len - 1) | bank;
		given_iova |= bank;
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
	dma_addr_t given_iova, struct sg_table *sgt, struct page ***pages)
{
	struct vpu_mem_ops *mops = vpu_drv->vp->mops;
	dma_addr_t iova = 0;
	struct page **p;

	if (!sgt || !pages)
		goto out;

	p = vpu_map_kva_to_sgt(addr, len, sgt);

	if (IS_ERR_OR_NULL(p))
		goto out;

	iova = mops->map_sg_to_iova(pdev, sgt->sgl, sgt->nents,
		len, given_iova);
	*pages = p;

out:
	return iova;
}

static dma_addr_t vpu_iova_alloc(struct platform_device *pdev,
	struct vpu_iova *i)
{
	int ret = 0;
	dma_addr_t iova = 0;
	/* mt6885, mt6873 maps va to iova */
	unsigned long base = (unsigned long)vpu_drv->bin_va;
	struct vpu_config *cfg = vpu_drv->vp->cfg;

	if (!pdev || !i || !i->size)
		goto out;

	iova = i->addr ? i->addr : cfg->iova_end;

	i->sgt.sgl = NULL;
	i->pages = NULL;
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
			&i->sgt, &i->pages);
	} else {
		dev_info(&pdev->dev,
			"%s: unknown setting (%x, %x, %x)\n",
			__func__, i->addr, i->bin, i->size);
		iova = 0;
	}

out:
	return iova;
}

static void vpu_unmap_iova_from_sg_v1(struct device *dev, struct vpu_iova *i)
{
	if (i->sgt.sgl) {
		dma_unmap_sg_attrs(dev, i->sgt.sgl,
			i->sgt.nents, DMA_BIDIRECTIONAL,
			DMA_ATTR_SKIP_CPU_SYNC);
			sg_free_table(&i->sgt);
	}
	kfree(i->pages);
}

static void vpu_unmap_iova_from_sg_v2(struct device *dev, struct vpu_iova *i)
{
	struct iommu_domain *domain;

	domain = iommu_get_domain_for_dev(dev);
	if (i->sgt.sgl) {
		iommu_unmap(domain, i->m.pa, i->m.length);
		sg_free_table(&i->sgt);
	}
	kfree(i->pages);
	apu_bmap_free(&vpu_drv->ab, i->m.pa, i->m.length);
}

static void vpu_iova_free(struct device *dev, struct vpu_iova *i)
{
	struct vpu_mem_ops *mops = vpu_drv->vp->mops;

	vpu_mem_free(&i->m);
	mops->unmap_iova_from_sg(dev, i);
}

static void vpu_iova_sync_for_device(struct device *dev,
	struct vpu_iova *i)
{
	dma_sync_sg_for_device(dev, i->sgt.sgl, i->sgt.nents,
		DMA_TO_DEVICE);
}

static void vpu_iova_sync_for_cpu(struct device *dev,
	struct vpu_iova *i)
{
	dma_sync_sg_for_cpu(dev, i->sgt.sgl, i->sgt.nents,
		DMA_FROM_DEVICE);
}

static int vpu_iova_dts(struct platform_device *pdev,
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

/* v1: Uses mtk_iommu's iova allocator
 * AOSP: The iova address must be aligned to its size
 */
struct vpu_mem_ops vpu_mops_v1 = {
	.init = vpu_mem_init_v1,
	.exit = vpu_mem_exit_v1,
	.alloc = vpu_iova_alloc,
	.free = vpu_iova_free,
	.map_sg_to_iova = vpu_map_sg_to_iova_v1,
	.unmap_iova_from_sg = vpu_unmap_iova_from_sg_v1,
	.sync_for_cpu = vpu_iova_sync_for_cpu,
	.sync_for_device = vpu_iova_sync_for_device,
	.dts = vpu_iova_dts,
};

/* v2: uses apu's iova allocator (apu_bmap) */
struct vpu_mem_ops vpu_mops_v2 = {
	.init = vpu_mem_init_v2,
	.exit = vpu_mem_exit_v2,
	.alloc = vpu_iova_alloc,
	.free = vpu_iova_free,
	.map_sg_to_iova = vpu_map_sg_to_iova_v2,
	.unmap_iova_from_sg = vpu_unmap_iova_from_sg_v2,
	.sync_for_cpu = vpu_iova_sync_for_cpu,
	.sync_for_device = vpu_iova_sync_for_device,
	.dts = vpu_iova_dts,
};


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
#include <linux/list_sort.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>

#include "apu_bmap.h"
#include "vpu_cfg.h"
#include "vpu_mem.h"
#include "vpu_debug.h"

static void vpu_iova_free(struct device *dev, struct vpu_iova *i);

static int vpu_iova_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct vpu_iova *ia, *ib;

	ia = list_entry(a, struct vpu_iova, list);
	ib = list_entry(b, struct vpu_iova, list);

	if (ia->iova < ib->iova)
		return -1;
	if (ia->iova > ib->iova)
		return 1;

	return 0;
}

static void vpu_iova_show(struct seq_file *s)
{
	struct vpu_iova *i;
	struct list_head *ptr, *tmp;

	mutex_lock(&vpu_drv->vi_lock);
	list_sort(NULL, &vpu_drv->vi, vpu_iova_cmp);
	list_for_each_safe(ptr, tmp, &vpu_drv->vi) {
		i = list_entry(ptr, struct vpu_iova, list);
		seq_puts(s, "[");
		vpu_seq_time(s, i->time);
		seq_printf(s,
			"] iova: %llx, addr: %x, size %x, bin: %x, m.pa: %x, m.len: %x\n",
			i->iova, i->addr, i->size, i->bin,
			i->m.pa, i->m.length);
	}
	mutex_unlock(&vpu_drv->vi_lock);
}

static int vpu_mem_init_v1(void)
{
	INIT_LIST_HEAD(&vpu_drv->vi);
	mutex_init(&vpu_drv->vi_lock);

	return 0;
}

static void vpu_mem_exit_v1(void)
{
	struct vpu_iova *i;
	struct list_head *ptr, *tmp;
	uint32_t nsec;
	uint64_t t;
	int remain = 0;

	mutex_lock(&vpu_drv->vi_lock);
	list_sort(NULL, &vpu_drv->vi, vpu_iova_cmp);
	list_for_each_safe(ptr, tmp, &vpu_drv->vi) {
		i = list_entry(ptr, struct vpu_iova, list);
		t = i->time;
		nsec = do_div(t, 1000000000);
		pr_info(
			"%s: [%lu.%06lu] iova: %llx, addr: %x, size %x, bin: %x, m.pa: %x, m.len: %x\n",
			__func__, (unsigned long)t, (unsigned long)nsec/1000,
			i->iova, i->addr, i->size, i->bin,
			i->m.pa, i->m.length);
		list_del(&i->list);
		i->list.prev = i->list.next = NULL;
		vpu_iova_free(vpu_drv->iova_dev, i);
		remain++;
	}
	mutex_unlock(&vpu_drv->vi_lock);

	if (remain)
		pr_info("%s: WARNING: there were %d unrelease iova.\n",
			__func__, remain);
}

static int vpu_mem_init_v2(void)
{
	struct vpu_config *cfg = vpu_drv->vp->cfg;

	vpu_drv->ab.au = PAGE_SIZE;
	vpu_drv->ab.start = cfg->iova_start;
	vpu_drv->ab.end = cfg->iova_end;
	apu_bmap_init(&vpu_drv->ab, "vpu_mem");

	return vpu_mem_init_v1();
}

static void vpu_mem_exit_v2(void)
{
	apu_bmap_exit(&vpu_drv->ab);
	vpu_mem_exit_v1();
}

static int vpu_map_kva_to_sgt(
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
vpu_mem_alloc(struct device *dev,
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

	ret = vpu_map_kva_to_sgt(kva, i->size, &i->sgt);
	if (ret)
		goto error;

	iova = mops->map_sg_to_iova(dev, i->sgt.sgl, i->sgt.nents,
		i->size, given_iova);

	if (!iova)
		goto error;

	i->m.va = (uint64_t)kva;
	i->m.pa = (uint64_t)iova;
	i->m.length = i->size;
	i->iova = iova;

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
vpu_map_sg_to_iova_v2(
	struct device *dev, struct scatterlist *sg,
	unsigned int nents, size_t len, dma_addr_t given_iova)
{
	struct iommu_domain *domain;
	dma_addr_t iova = 0;
	size_t size = 0;
	struct vpu_config *cfg = vpu_drv->vp->cfg;
	struct vpu_device *vd = dev_get_drvdata(dev);
	int prot = IOMMU_READ | IOMMU_WRITE;
	u64 bank = cfg->iova_bank;

	domain = iommu_get_domain_for_dev(dev);

	vpu_mem_debug("%s: %s: len: %zx, given_iova: %llx (%s alloc)\n",
		__func__, vd->name, len, (u64)given_iova,
		(given_iova < cfg->iova_end) ? "static" : "dynamic");

	if (given_iova < cfg->iova_end) {  /* Static IOVA allocation */
		iova = apu_bmap_alloc(&vpu_drv->ab, len, given_iova);
		if (!iova)
			goto err;
		/* Static: must be allocated on the given address */
		if (iova != given_iova) {
			dev_info(dev,
				"%s: given iova: %llx, apu_bmap_alloc returned: %llx\n",
				__func__, (u64)given_iova, (u64)iova);
			apu_bmap_free(&vpu_drv->ab, iova, len);
			goto err;
		}
	} else {  /* Dynamic IOVA allocation */
		/* Dynamic: Allocate from heap first */
		iova = apu_bmap_alloc(&vpu_drv->ab, len, cfg->iova_heap);
		if (!iova) {
			/* Dynamic: Try to allocate again from iova start */
			iova = apu_bmap_alloc(&vpu_drv->ab, len, 0);
			if (!iova)
				goto err;
		}
	}

	iova = iova | bank;
	vpu_mem_debug("%s: %s: len: %zx, iova: %llx\n",
		__func__, vd->name, len, (u64)iova);

	size = iommu_map_sg(domain, iova, sg, nents, prot);

	if (size == 0) {
		dev_info(dev,
			"%s: iommu_map_sg: len: %zx, iova: %llx, failed\n",
			__func__, len, (u64)iova, nents);
		goto err;
	} else if (size != len) {
		dev_info(dev,
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
	struct device *dev, struct scatterlist *sg,
	unsigned int nents,	size_t len, dma_addr_t given_iova)
{
	dma_addr_t mask;
	dma_addr_t iova = 0;
	bool dyn_alloc = false;
	bool match = false;
	int ret;
	struct vpu_config *cfg = vpu_drv->vp->cfg;
	struct vpu_device *vd = dev_get_drvdata(dev);
	u64 bank = cfg->iova_bank;
	u32 iova_end = cfg->iova_end;

	if (!sg)
		return 0;

	if (given_iova >= iova_end) {
		dyn_alloc = true;
		mask = (iova_end - 1) | bank;
		given_iova |= bank;
		vpu_mem_debug("%s: %s: len: %zx, given_iova mask: %llx (dynamic alloc)\n",
			__func__, vd->name,
			len, (u64)given_iova);
	} else {
		mask = (given_iova + len - 1) | bank;
		given_iova |= bank;
		vpu_mem_debug("%s: %s: len: %zx, given_iova start ~ end(mask): %llx ~ %llx\n",
			__func__, vd->name,
			len, (u64)given_iova, (u64)mask);
	}

	dma_set_mask_and_coherent(dev, mask);

	ret = dma_map_sg_attrs(dev, sg, nents,
		DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);

	if (ret <= 0) {
		dev_info(dev,
			"%s: dma_map_sg_attrs: failed with %d\n",
			__func__, ret);
		return 0;
	}

	iova = sg_dma_address(&sg[0]);

	if (given_iova == iova)
		match = true;

	dev_info(dev,
		"%s: sg_dma_address: size: %lx, mapped iova: 0x%llx %s\n",
		__func__, len, (u64)iova,
		dyn_alloc ? "(dynamic alloc)" :
		(match ? "(static alloc)" : "(unexpected)"));

	if (!dyn_alloc && !match)
		vpu_aee_warn("VPU", "iova mapping error");

	return iova;
}

static dma_addr_t
vpu_map_to_iova(struct device *dev, void *addr, size_t len,
	dma_addr_t given_iova, struct sg_table *sgt)
{
	struct vpu_mem_ops *mops = vpu_drv->vp->mops;
	dma_addr_t iova = 0;
	int ret;

	if (!sgt)
		goto out;

	ret = vpu_map_kva_to_sgt(addr, len, sgt);

	if (ret)
		goto out;

	iova = mops->map_sg_to_iova(dev, sgt->sgl, sgt->nents,
		len, given_iova);

out:
	return iova;
}

static dma_addr_t vpu_iova_alloc(struct device *dev,
	struct vpu_iova *i)
{
	int ret = 0;
	dma_addr_t iova = 0;
	/* mt6885, mt6873 maps va to iova */
	unsigned long base = (unsigned long)vpu_drv->bin_va;
	struct vpu_config *cfg = vpu_drv->vp->cfg;
	struct vpu_device *vd = dev_get_drvdata(dev);

	if (!dev || !i || !i->size)
		goto out;

	iova = i->addr ? i->addr : cfg->iova_end;

	i->sgt.sgl = NULL;
	i->m.handle = NULL;
	i->m.va = 0;
	i->m.pa = 0;
	i->m.length = 0;
	i->time = sched_clock();

	/* allocate kvm and map */
	if (i->bin == VPU_MEM_ALLOC) {
		ret = vpu_mem_alloc(dev, i, iova);
		iova = i->iova;
	/* map from vpu firmware loaded at bootloader */
	} else if (i->size) {
		iova = vpu_map_to_iova(dev,
			(void *)(base + i->bin), i->size, iova,
			&i->sgt);
		i->iova = (uint64_t)iova;
	} else {
		dev_info(dev,
			"%s: unknown setting (%x, %x, %x)\n",
			__func__, i->addr, i->bin, i->size);
		iova = 0;
	}

	vpu_mem_debug("%s: %s: iova: 0x%llx, size: %x\n",
		__func__, vd->name, i->iova, i->size);
	mutex_lock(&vpu_drv->vi_lock);
	list_add_tail(&i->list, &vpu_drv->vi);
	mutex_unlock(&vpu_drv->vi_lock);

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
}

static void vpu_unmap_iova_from_sg_v2(struct device *dev, struct vpu_iova *i)
{
	struct vpu_device *vd = dev_get_drvdata(dev);
	struct iommu_domain *domain;
	dma_addr_t iova = i->iova;
	size_t size = i->size;
	size_t ret;

	vpu_mem_debug("%s: %s: len: %zx, iova: %llx\n",
		__func__, vd->name, size, (u64)iova);

	domain = iommu_get_domain_for_dev(dev);
	if (i->sgt.sgl) {
		ret = iommu_unmap(domain, iova, size);
		if (ret != size)
			dev_info(dev,
				"%s: iommu_unmap iova: %llx, returned: %zx, expected: %zx\n",
				__func__, (u64)iova, ret, size);

		sg_free_table(&i->sgt);
	}
	apu_bmap_free(&vpu_drv->ab, i->m.pa, i->m.length);
}

static void vpu_iova_free(struct device *dev, struct vpu_iova *i)
{
	struct vpu_mem_ops *mops = vpu_drv->vp->mops;
	struct vpu_device *vd = dev_get_drvdata(dev);

	/* skip, if already deleted by .exit() */
	if (i->list.prev && i->list.next) {
		mutex_lock(&vpu_drv->vi_lock);
		list_del(&i->list);
		mutex_unlock(&vpu_drv->vi_lock);
	}

	if (i->iova && i->size) {
		vpu_mem_debug("%s: %s: iova: 0x%llx, size: %x\n",
			__func__, vd->name, i->iova, i->size);
		vpu_mem_free(&i->m);
		mops->unmap_iova_from_sg(dev, i);
	}
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

static int vpu_iova_dts(struct device *dev,
	const char *name, struct vpu_iova *i)
{
	if (of_property_read_u32_array(dev->of_node,
			name, &i->addr, 3)) {
		dev_info(dev, "%s: vpu: unable to get %s\n",
			__func__, name);
		return -ENODEV;
	}

	dev_info(dev, "%s: %s: addr: %08xh, size: %08xh, bin: %08xh\n",
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
	.show = vpu_iova_show,
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
	.show = vpu_iova_show,
};


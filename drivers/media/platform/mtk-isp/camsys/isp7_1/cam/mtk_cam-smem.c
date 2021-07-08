// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <asm/cacheflush.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/dma-buf.h>
#include <linux/remoteproc.h>

#ifndef CONFIG_MTK_SCP
#include <linux/platform_data/mtk_ccd.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/remoteproc/mtk_ccd_mem.h>
#include <uapi/linux/mtk_ccd_controls.h>
#endif

#include "mtk_cam.h"
#include "mtk_cam-smem.h"

#ifdef CONFIG_MTK_SCP
static const struct dma_map_ops smem_dma_ops;
#endif

struct mtk_cam_smem_dev {
	struct device *dev;
	struct sg_table sgt;
	struct page **smem_pages;
	dma_addr_t smem_base;
	dma_addr_t smem_dma_base;
	int smem_size;
};

struct dma_coherent_mem {
	void		*virt_base;
	dma_addr_t	device_base;
	unsigned long	pfn_base;
	int		size;
	int		flags;
	unsigned long	*bitmap;
	spinlock_t	spinlock; /* dma_coherent_mem attributes protection */
	bool		use_dev_dma_pfn_offset;
};

dma_addr_t mtk_cam_smem_iova_to_scp_addr(struct device *dev,
					 dma_addr_t iova)
{
	struct iommu_domain *domain;
	dma_addr_t addr, limit;
	struct mtk_cam_smem_dev *smem_dev = dev_get_drvdata(dev);

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		dev_dbg(dev, "No iommu group domain\n");
		return 0;
	}

	addr = iommu_iova_to_phys(domain, iova);
	limit = smem_dev->smem_base + smem_dev->smem_size;
	if (addr < smem_dev->smem_base || addr >= limit) {
		dev_dbg(dev,
			"Unexpected scp_addr:%pad must >= %pad and < %pad)\n",
			&addr, &smem_dev->smem_base, &limit);
		return 0;
	}
	return addr;
}

#ifdef CONFIG_MTK_SCP
static int mtk_cam_smem_get_sgtable(struct device *dev,
				    struct sg_table *sgt,
				    void *cpu_addr, dma_addr_t dma_addr,
				    size_t size, unsigned long attrs)
{
	struct mtk_cam_smem_dev *smem_dev = dev_get_drvdata(dev);
	size_t pages_count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	dma_addr_t scp_addr = mtk_cam_smem_iova_to_scp_addr(dev, dma_addr);
	u32 pages_start = (scp_addr - smem_dev->smem_base) >> PAGE_SHIFT;

	dev_dbg(dev,
		"%s:page:%u va:%pK scp addr:%pad, aligned size:%zu pages:%zu\n",
		__func__, pages_start, cpu_addr, &scp_addr, size, pages_count);

	return sg_alloc_table_from_pages(sgt,
		smem_dev->smem_pages + pages_start,
		pages_count, 0, size, GFP_KERNEL);
}

static void *mtk_cam_smem_get_cpu_addr(struct mtk_cam_smem_dev *smem_dev,
				       dma_addr_t addr)
{
	struct device *dev = smem_dev->dev;
	struct dma_coherent_mem *dma_mem = dev->dma_mem;

	if (addr < smem_dev->smem_base ||
	    addr > smem_dev->smem_base + smem_dev->smem_size) {
		dev_dbg(dev, "Invalid scp_addr %pad from sg\n", &addr);
		return NULL;
	}
	return dma_mem->virt_base + (addr - smem_dev->smem_base);
}

static void mtk_cam_smem_sync_sg_for_cpu(struct device *dev,
					 struct scatterlist *sgl, int nelems,
					 enum dma_data_direction dir)
{
	struct mtk_cam_smem_dev *smem_dev = dev_get_drvdata(dev);
	dma_addr_t scp_addr = sg_phys(sgl);
	void *cpu_addr = mtk_cam_smem_get_cpu_addr(smem_dev, scp_addr);

	dev_dbg(dev,
		"__dma_unmap_area:scp_addr:%pad,vaddr:%pK,size:%d,dir:%d\n",
		&scp_addr, cpu_addr, sgl->length, dir);
	__dma_unmap_area(cpu_addr, sgl->length, dir);
}

static void mtk_cam_smem_sync_sg_for_device(struct device *dev,
					    struct scatterlist *sgl,
					    int nelems,
					    enum dma_data_direction dir)
{
	struct mtk_cam_smem_dev *smem_dev = dev_get_drvdata(dev);
	dma_addr_t scp_addr = sg_phys(sgl);
	void *cpu_addr = mtk_cam_smem_get_cpu_addr(smem_dev, scp_addr);

	dev_dbg(dev,
		"__dma_map_area:scp_addr:%pad,vaddr:%pK,size:%d,dir:%d\n",
		&scp_addr, cpu_addr, sgl->length, dir);
	__dma_map_area(cpu_addr, sgl->length, dir);
}

static void mtk_cam_smem_setup_dma_ops(struct device *dev,
				       const struct dma_map_ops *smem_ops)
{
	memcpy((void *)smem_ops, dev->dma_ops, sizeof(*smem_ops));
	smem_ops->get_sgtable = mtk_cam_smem_get_sgtable;
	smem_ops->sync_sg_for_device = mtk_cam_smem_sync_sg_for_device;
	smem_ops->sync_sg_for_cpu = mtk_cam_smem_sync_sg_for_cpu;
	set_dma_ops(dev, smem_ops);
}

static int mtk_cam_reserved_drm_sg_init(struct mtk_cam_smem_dev *smem_dev)
{
	u32 size_align, n_pages;
	struct device *dev = smem_dev->dev;
	struct sg_table *sgt = &smem_dev->sgt;
	struct page **pages;
	dma_addr_t dma_addr;
	unsigned int i;
	int ret;

	smem_dev->smem_base = scp_get_reserve_mem_phys(SCP_ISP_MEM2_ID);
	smem_dev->smem_size = scp_get_reserve_mem_size(SCP_ISP_MEM2_ID);

	if (!smem_dev->smem_base || !smem_dev->smem_size)
		return -EPROBE_DEFER;

	dev_info(dev, "%s dev:0x%pK base:%pad size:%u MiB\n",
		 __func__,
		 smem_dev->dev,
		 &smem_dev->smem_base,
		 (smem_dev->smem_size / SZ_1M));

	size_align = PAGE_ALIGN(smem_dev->smem_size);
	n_pages = size_align >> PAGE_SHIFT;

	pages = kmalloc_array(n_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	for (i = 0; i < n_pages; i++)
		pages[i] = phys_to_page(smem_dev->smem_base + i * PAGE_SIZE);

	ret = sg_alloc_table_from_pages(sgt, pages, n_pages, 0,
					size_align, GFP_KERNEL);
	if (ret) {
		dev_dbg(dev, "failed to alloca sg table:%d\n", ret);
		goto fail_table_alloc;
	}
	sgt->nents = dma_map_sg_attrs(dev, sgt->sgl, sgt->orig_nents,
				      DMA_BIDIRECTIONAL,
				      DMA_ATTR_SKIP_CPU_SYNC);
	if (!sgt->nents) {
		dev_dbg(dev, "failed to dma sg map\n");
		goto fail_map;
	}

	dma_addr = sg_dma_address(sgt->sgl);
	ret = dma_declare_coherent_memory(dev, smem_dev->smem_base,
					  dma_addr, size_align,
					  DMA_MEMORY_EXCLUSIVE);
	if (ret) {
		dev_dbg(dev, "Unable to declare smem  memory:%d\n", ret);
		goto fail_map;
	}

	dev_info(dev, "Coherent mem pa:%pad/%pad, size:%d\n",
		 &smem_dev->smem_base, &dma_addr, size_align);

	smem_dev->smem_size = size_align;
	smem_dev->smem_pages = pages;
	smem_dev->smem_dma_base = dma_addr;

	return 0;

fail_map:
	sg_free_table(sgt);
fail_table_alloc:
	while (n_pages--)
		__free_page(pages[n_pages]);
	kfree(pages);

	return -ENOMEM;
}

/* DMA memory related helper functions */
static void mtk_cam_memdev_release(struct device *dev)
{
	vb2_dma_contig_clear_max_seg_size(dev);
}

static struct device *mtk_cam_alloc_smem_dev(struct device *dev,
					     const char *name)
{
	struct device *child;
	int ret;

	child = devm_kzalloc(dev, sizeof(*child), GFP_KERNEL);
	if (!child)
		return NULL;

	child->parent = dev;
	child->iommu_group = dev->iommu_group;
	child->release = mtk_cam_memdev_release;
	dev_set_name(child, name);
	set_dma_ops(child, get_dma_ops(dev));
	child->dma_mask = dev->dma_mask;
	ret = dma_set_coherent_mask(child, DMA_BIT_MASK(32));
	if (ret)
		return NULL;

	vb2_dma_contig_set_max_seg_size(child, DMA_BIT_MASK(32));

	if (device_register(child)) {
		device_del(child);
		return NULL;
	}

	return child;
}
#endif

static int mtk_cam_composer_dma_init(struct mtk_cam_device *cam)
{
	(void) cam;
	return -1;
}

int mtk_cam_reserved_memory_init(struct mtk_cam_device *cam)
{
	int ret;

	ret = mtk_cam_composer_dma_init(cam);
	if (ret)
		return ret;

	return 0;
}

void mtk_cam_reserved_memory_uninit(struct mtk_cam_device *cam)
{
	(void) cam;
}

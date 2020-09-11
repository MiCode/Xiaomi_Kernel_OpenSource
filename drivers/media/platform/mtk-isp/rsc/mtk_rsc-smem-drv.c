/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/iommu.h>
#include <asm/cacheflush.h>

#define MTK_RSC_SMEM_DEV_NAME "MTK-RSC-SMEM"

struct mtk_rsc_smem_drv {
	struct platform_device *pdev;
	struct sg_table sgt;
	struct page **smem_pages;
	int num_smem_pages;
	phys_addr_t smem_base;
	dma_addr_t smem_dma_base;
	int smem_size;
};

static struct reserved_mem *isp_rsc_reserved_smem;

static int mtk_rsc_smem_setup_dma_ops(struct device *smem_dev,
				     const struct dma_map_ops *smem_ops);

static int mtk_rsc_smem_get_sgtable(struct device *dev,
				   struct sg_table *sgt,
				   void *cpu_addr, dma_addr_t dma_addr,
				   size_t size, unsigned long attrs);

static const struct dma_map_ops smem_dma_ops = {
	.get_sgtable = mtk_rsc_smem_get_sgtable,
};

static int mtk_rsc_smem_init(struct mtk_rsc_smem_drv **mtk_rsc_smem_drv_out,
			    struct platform_device *pdev)
{
	struct mtk_rsc_smem_drv *rsc_sys = NULL;
	struct device *dev = &pdev->dev;

	rsc_sys = devm_kzalloc(dev,
			       sizeof(*rsc_sys), GFP_KERNEL);

	rsc_sys->pdev = pdev;

	*mtk_rsc_smem_drv_out = rsc_sys;

	return 0;
}

static int mtk_rsc_smem_drv_probe(struct platform_device *pdev)
{
	struct mtk_rsc_smem_drv *smem_drv = NULL;
	int r = 0;
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "probe mtk_rsc_smem_drv\n");

	r = mtk_rsc_smem_init(&smem_drv, pdev);

	if (!smem_drv)
		return -ENOMEM;

	dev_set_drvdata(dev, smem_drv);

	if (isp_rsc_reserved_smem) {
		dma_addr_t dma_addr;
		phys_addr_t addr;
		struct iommu_domain *smem_dom;
		int i = 0;
		int size_align = 0;
		struct page **pages = NULL;
		int n_pages = 0;
		struct sg_table *sgt = &smem_drv->sgt;

		size_align = round_down(isp_rsc_reserved_smem->size,
					PAGE_SIZE);
		n_pages = size_align >> PAGE_SHIFT;

		pages = kmalloc_array(n_pages, sizeof(struct page *),
				      GFP_KERNEL);

		if (!pages)
			return -ENOMEM;

		for (i = 0; i < n_pages; i++)
			pages[i] = phys_to_page(isp_rsc_reserved_smem->base
						+ i * PAGE_SIZE);

		r = sg_alloc_table_from_pages(sgt, pages, n_pages, 0,
					      size_align, GFP_KERNEL);

		if (r) {
			dev_dbg(dev, "failed to get alloca sg table\n");
			return -ENOMEM;
		}

		dma_map_sg_attrs(dev, sgt->sgl, sgt->nents,
				 DMA_BIDIRECTIONAL,
				 DMA_ATTR_SKIP_CPU_SYNC);

		dma_addr = sg_dma_address(sgt->sgl);
		smem_dom = iommu_get_domain_for_dev(dev);
		addr = iommu_iova_to_phys(smem_dom, dma_addr);

		if (addr != isp_rsc_reserved_smem->base)
			dev_dbg(dev,
				"incorrect pa(%llx) from iommu_iova_to_phys, should be %llx\n",
			(unsigned long long)addr,
			(unsigned long long)isp_rsc_reserved_smem->base);

		r = dma_declare_coherent_memory(dev,
						isp_rsc_reserved_smem->base,
			dma_addr, size_align, DMA_MEMORY_EXCLUSIVE);

		dev_dbg(dev,
			"Coherent mem base(%llx,%llx),size(%lx),ret(%d)\n",
			isp_rsc_reserved_smem->base,
			dma_addr, size_align, r);

		smem_drv->smem_base = isp_rsc_reserved_smem->base;
		smem_drv->smem_size = size_align;
		smem_drv->smem_pages = pages;
		smem_drv->num_smem_pages = n_pages;
		smem_drv->smem_dma_base = dma_addr;

		dev_dbg(dev, "smem_drv setting (%llx,%lx,%llx,%d)\n",
			smem_drv->smem_base, smem_drv->smem_size,
			(unsigned long long)smem_drv->smem_pages,
			smem_drv->num_smem_pages);
	}

	r = mtk_rsc_smem_setup_dma_ops(dev, &smem_dma_ops);

	return r;
}

phys_addr_t mtk_rsc_smem_iova_to_phys(struct device *dev,
				     dma_addr_t iova)
{
		struct iommu_domain *smem_dom;
		phys_addr_t addr;
		phys_addr_t limit;
		struct mtk_rsc_smem_drv *smem_dev =
			dev_get_drvdata(dev);

		if (!smem_dev)
			return 0;

		smem_dom = iommu_get_domain_for_dev(dev);

		if (!smem_dom)
			return 0;

		addr = iommu_iova_to_phys(smem_dom, iova);

		limit = smem_dev->smem_base + smem_dev->smem_size;

		if (addr < smem_dev->smem_base || addr >= limit) {
			dev_dbg(dev,
				"Unexpected paddr %pa (must >= %pa and <%pa)\n",
				&addr, &smem_dev->smem_base, &limit);
			return 0;
		}
		dev_dbg(dev, "Pa verifcation pass: %pa(>=%pa, <%pa)\n",
			&addr, &smem_dev->smem_base, &limit);
		return addr;
}

static int mtk_rsc_smem_drv_remove(struct platform_device *pdev)
{
	struct mtk_rsc_smem_drv *smem_drv =
		dev_get_drvdata(&pdev->dev);

	kfree(smem_drv->smem_pages);
	return 0;
}

static int mtk_rsc_smem_drv_suspend(struct device *dev)
{
	return 0;
}

static int mtk_rsc_smem_drv_resume(struct device *dev)
{
	return 0;
}

static int mtk_rsc_smem_drv_dummy_cb(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops mtk_rsc_smem_drv_pm_ops = {
	SET_RUNTIME_PM_OPS(&mtk_rsc_smem_drv_dummy_cb,
			   &mtk_rsc_smem_drv_dummy_cb, NULL)
	SET_SYSTEM_SLEEP_PM_OPS
		(&mtk_rsc_smem_drv_suspend, &mtk_rsc_smem_drv_resume)
};

static const struct of_device_id mtk_rsc_smem_drv_of_match[] = {
	{
		.compatible = "mediatek,rsc_smem",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_rsc_smem_drv_of_match);

static struct platform_driver mtk_rsc_smem_driver = {
	.probe = mtk_rsc_smem_drv_probe,
	.remove = mtk_rsc_smem_drv_remove,
	.driver = {
		.name = MTK_RSC_SMEM_DEV_NAME,
		.of_match_table =
			of_match_ptr(mtk_rsc_smem_drv_of_match),
		.pm = &mtk_rsc_smem_drv_pm_ops,
	},
};

static int __init mtk_rsc_smem_dma_setup(struct reserved_mem
					*rmem)
{
	unsigned long node = rmem->fdt_node;

	if (of_get_flat_dt_prop(node, "reusable", NULL))
		return -EINVAL;

	if (!of_get_flat_dt_prop(node, "no-map", NULL)) {
		pr_debug("Reserved memory: regions without no-map are not yet supported\n");
		return -EINVAL;
	}

	isp_rsc_reserved_smem = rmem;

	pr_debug("Reserved memory: created DMA memory pool at %pa, size %ld MiB\n",
		 &rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}

RESERVEDMEM_OF_DECLARE(mtk_rsc_smem,
		       "mediatek,reserve-memory-rsc_smem",
		       mtk_rsc_smem_dma_setup);

int __init mtk_rsc_smem_drv_init(void)
{
	int ret = 0;

	pr_debug("platform_driver_register: mtk_rsc_smem_driver\n");
	ret = platform_driver_register(&mtk_rsc_smem_driver);

	if (ret)
		pr_debug("rsc smem drv init failed, driver didn't probe\n");

	return ret;
}
subsys_initcall(mtk_rsc_smem_drv_init);

void __exit mtk_rsc_smem_drv_ext(void)
{
	platform_driver_unregister(&mtk_rsc_smem_driver);
}
module_exit(mtk_rsc_smem_drv_ext);

/********************************************
 * MTK RSC SMEM DMA ops *
 ********************************************/

struct dma_coherent_mem {
	void		*virt_base;
	dma_addr_t	device_base;
	unsigned long	pfn_base;
	int		size;
	int		flags;
	unsigned long	*bitmap;
	spinlock_t	spinlock; /* protect the members in dma_coherent_mem */
	bool		use_dev_dma_pfn_offset;
};

static struct dma_coherent_mem *dev_get_coherent_memory(struct device *dev)
{
	if (dev && dev->dma_mem)
		return dev->dma_mem;
	return NULL;
}

static int mtk_rsc_smem_get_sgtable(struct device *dev,
				   struct sg_table *sgt,
	void *cpu_addr, dma_addr_t dma_addr,
	size_t size, unsigned long attrs)
{
	struct mtk_rsc_smem_drv *smem_dev = dev_get_drvdata(dev);
	int n_pages_align = 0;
	int size_align = 0;
	int page_start = 0;
	unsigned long long offset_p = 0;
	unsigned long long offset_d = 0;

	phys_addr_t paddr = mtk_rsc_smem_iova_to_phys(dev, dma_addr);

	offset_d = (unsigned long long)dma_addr -
		(unsigned long long)smem_dev->smem_dma_base;

	offset_p = (unsigned long long)paddr -
		(unsigned long long)smem_dev->smem_base;

	dev_dbg(dev, "%s:dma_addr:%llx,cpu_addr:%llx,pa:%llx,size:%d\n",
		__func__,
		(unsigned long long)dma_addr,
		(unsigned long long)cpu_addr,
		(unsigned long long)paddr,
		size
		);

	dev_dbg(dev, "%s:offset p:%llx,offset d:%llx\n",
		__func__,
		(unsigned long long)offset_p,
		(unsigned long long)offset_d
		);

	size_align = round_up(size, PAGE_SIZE);
	n_pages_align = size_align >> PAGE_SHIFT;
	page_start = offset_p >> PAGE_SHIFT;

	dev_dbg(dev,
		"%s:page idx:%d,page pa:%llx,pa:%llx, aligned size:%d\n",
		__func__,
		page_start,
		(unsigned long long)page_to_phys(*(smem_dev->smem_pages
			+ page_start)),
		(unsigned long long)paddr,
		size_align
		);

	if (!smem_dev) {
		dev_dbg(dev, "can't get sgtable from smem_dev\n");
		return -EINVAL;
	}

	dev_dbg(dev, "get sgt of the smem: %d pages\n", n_pages_align);

	return sg_alloc_table_from_pages(sgt,
		smem_dev->smem_pages + page_start,
		n_pages_align,
		0, size_align, GFP_KERNEL);
}

static void *mtk_rsc_smem_get_cpu_addr(struct mtk_rsc_smem_drv *smem_dev,
				      struct scatterlist *sg)
{
	struct device *dev = &smem_dev->pdev->dev;
	struct dma_coherent_mem *dma_mem =
		dev_get_coherent_memory(dev);

	phys_addr_t addr = (phys_addr_t)sg_phys(sg);

	if (addr < smem_dev->smem_base ||
	    addr > smem_dev->smem_base + smem_dev->smem_size) {
		dev_dbg(dev, "Invalid paddr 0x%llx from sg\n", addr);
		return NULL;
	}

	return dma_mem->virt_base + (addr - smem_dev->smem_base);
}

static void mtk_rsc_smem_sync_sg_for_cpu(struct device *dev,
					struct scatterlist *sgl, int nelems,
					enum dma_data_direction dir)
{
	struct mtk_rsc_smem_drv *smem_dev =
		dev_get_drvdata(dev);
	void *cpu_addr;

	cpu_addr = mtk_rsc_smem_get_cpu_addr(smem_dev, sgl);

	dev_dbg(dev,
		"__dma_unmap_area:paddr(0x%llx),vaddr(0x%llx),size(%d)\n",
		(unsigned long long)sg_phys(sgl),
		(unsigned long long)cpu_addr,
		sgl->length);

	__dma_unmap_area(cpu_addr, sgl->length, dir);
}

static void mtk_rsc_smem_sync_sg_for_device(struct device *dev,
					   struct scatterlist *sgl, int nelems,
					   enum dma_data_direction dir)
{
	struct mtk_rsc_smem_drv *smem_dev =
			dev_get_drvdata(dev);
	void *cpu_addr;

	cpu_addr = mtk_rsc_smem_get_cpu_addr(smem_dev, sgl);

	dev_dbg(dev,
		"__dma_map_area:paddr(0x%llx),vaddr(0x%llx),size(%d)\n",
		(unsigned long long)sg_phys(sgl),
		(unsigned long long)cpu_addr,
		sgl->length);

	__dma_map_area(cpu_addr, sgl->length, dir);
}

static int mtk_rsc_smem_setup_dma_ops(struct device *dev,
				     const struct dma_map_ops *smem_ops)
{
	if (!dev->dma_ops)
		return -EINVAL;

	memcpy((void *)smem_ops, dev->dma_ops, sizeof(*smem_ops));

	smem_ops->get_sgtable =
		mtk_rsc_smem_get_sgtable;
	smem_ops->sync_sg_for_device =
		mtk_rsc_smem_sync_sg_for_device;
	smem_ops->sync_sg_for_cpu =
		mtk_rsc_smem_sync_sg_for_cpu;

	dev->dma_ops = smem_ops;

	return 0;
}

void mtk_rsc_smem_enable_mpu(struct device *dev)
{
	dev_dbg(dev, "MPU enabling func is not ready now\n");
}

MODULE_AUTHOR("Frederic Chen <frederic.chen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek RSC shared memory driver");

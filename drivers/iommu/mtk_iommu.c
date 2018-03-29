/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author:	YongWu <yong.wu@mediatek.com>
 *		Honghui Zhang <honghui.zhang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/errno.h>
#include <asm/device.h>
#include <linux/memblock.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#include <asm/dma-iommu.h>
#include <asm/dma-mapping.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/module.h>

#include "mtk_iommu_platform.h"
#include "mtk_iommu_reg_mt2701.h"

static struct device *pimudev;

static int mtk_iommu_domain_init(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *mtk_domain;

	mtk_domain = devm_kzalloc(pimudev, sizeof(*mtk_domain), GFP_KERNEL);
	if (!mtk_domain)
		return -ENOMEM;

	mtk_domain->pgtableva = dma_alloc_coherent(pimudev, MTK_IOMMU_PGT_SZ,
						   &mtk_domain->pgtablepa, GFP_KERNEL);
	if (!mtk_domain->pgtableva) {
		dev_err(pimudev, "dma_alloc_coherent pagetable fail\n");
		return -ENOMEM;
	}

	if (!IS_ALIGNED(mtk_domain->pgtablepa, MTK_IOMMU_PGT_SZ)) {
		dev_err(pimudev, "pagetable not aligned pa 0x%x, va 0x%p align 0x%x\n",
			mtk_domain->pgtablepa, mtk_domain->pgtableva,
			MTK_IOMMU_PGT_SZ);
		dma_free_coherent(pimudev, MTK_IOMMU_PGT_SZ,
				  mtk_domain->pgtableva, mtk_domain->pgtablepa);
		return -ENOMEM;
	}

	memset(mtk_domain->pgtableva, 0, MTK_IOMMU_PGT_SZ);

	spin_lock_init(&mtk_domain->pgtlock);
	spin_lock_init(&mtk_domain->portlock);
	domain->priv = mtk_domain;
	mtk_domain->domain = domain;
	return 0;
}

static void mtk_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *mtk_domain = domain->priv;

	dma_free_coherent(mtk_domain->piommuinfo->dev, MTK_IOMMU_PGT_SZ,
			  mtk_domain->pgtableva, mtk_domain->pgtablepa);

	devm_kfree(mtk_domain->piommuinfo->dev, mtk_domain);
	domain->priv = NULL;
}

static int mtk_iommu_dev_enable_iommu(struct mtk_iommu_info *imuinfo,
				      struct device *dev, bool enable)
{
	struct of_phandle_args out_args = {0};
	struct device *imudev = imuinfo->dev;
	int i = 0, ret = 0;

	while (!of_parse_phandle_with_args(dev->of_node, "iommus",
					   "#iommu-cells", i++, &out_args)) {
		if (out_args.np != imudev->of_node)
			continue;
		if (out_args.args_count != 1) {
			dev_err(imudev, "invalid #iommu-cells property for IOMMU\n");
			return -EINVAL;
		}

		dev_dbg(imudev, "%s iommu @ port:%d\n",
			enable ? "enable" : "disable", out_args.args[0]);

		ret = imuinfo->imucfg->config_port(imuinfo,
			out_args.args[0], enable);
		if (ret) {
			dev_err(imudev, "iommu config port error %d\n", ret);
			return ret;
		}
	}
	return ret;
}

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct mtk_iommu_domain *mtk_domain = domain->priv;

	return mtk_iommu_dev_enable_iommu(mtk_domain->piommuinfo, dev, true);
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct mtk_iommu_domain *mtk_domain = domain->priv;

	mtk_iommu_dev_enable_iommu(mtk_domain->piommuinfo, dev, false);
}

/* will parse the dt in probe, this will do nothing */
static int mtk_iommu_add_device(struct device *dev)
{
	return 0;
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;

	spin_lock_irqsave(&priv->pgtlock, flags);
	priv->piommuinfo->imucfg->map(priv, (unsigned int)iova, paddr, size);
	spin_unlock_irqrestore(&priv->pgtlock, flags);

	return 0;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			      unsigned long iova, size_t size)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;
	int unmapped_size;

	spin_lock_irqsave(&priv->pgtlock, flags);
	unmapped_size = priv->piommuinfo->imucfg->unmap(priv,
							(unsigned int)iova, size);
	spin_unlock_irqrestore(&priv->pgtlock, flags);

	return unmapped_size;
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *priv = domain->priv;
	unsigned long flags;
	phys_addr_t phys;

	spin_lock_irqsave(&priv->pgtlock, flags);
	phys = priv->piommuinfo->imucfg->iova_to_phys(priv,
						      (unsigned int)iova);
	spin_unlock_irqrestore(&priv->pgtlock, flags);
	return phys;
}

const struct iommu_ops mtk_iommu_ops = {
	.domain_init	= mtk_iommu_domain_init,
	.domain_destroy	= mtk_iommu_domain_destroy,
	.attach_dev	= mtk_iommu_attach_device,
	.detach_dev	= mtk_iommu_detach_device,
	.add_device	= mtk_iommu_add_device,
	.map		= mtk_iommu_map,
	.unmap		= mtk_iommu_unmap,
	.iova_to_phys	= mtk_iommu_iova_to_phys,
	.pgsize_bitmap	= PAGE_SIZE,
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{
		.compatible = "mediatek,mt2701-m4u",
		.data = &mtk_iommu_mt2701_cfg,
	},
	{ }
};

static int mtk_iommu_probe(struct platform_device *pdev)
{
	int ret;
	struct dma_iommu_mapping *mtk_mapping;
	struct mtk_iommu_info *piommu;
	struct iommu_domain *domain;
	struct mtk_iommu_domain *mtk_domain;
	const struct of_device_id *of_id;
	void *protect_va;

	piommu = devm_kzalloc(&pdev->dev, sizeof(*piommu), GFP_KERNEL);
	if (!piommu)
		return -ENOMEM;

	of_id = of_match_node(mtk_iommu_of_ids, pdev->dev.of_node);
	if (!of_id)
		return -ENODEV;

	piommu->imucfg = of_id->data;
	piommu->dev = &pdev->dev;
	ret = piommu->imucfg->dt_parse(pdev, piommu);
	if (ret)
		return ret;

	protect_va = devm_kzalloc(&pdev->dev, MTK_PROTECT_PA_ALIGN*2,
				  GFP_KERNEL);
	if (!protect_va)
		return -ENOMEM;

	piommu->protect_va = protect_va;
	piommu->protect_base = virt_to_phys(piommu->protect_va);

	piommu->iova_base = 0;
	piommu->iova_size = (MTK_IOMMU_PGT_SZ / sizeof(int))
			    * MT2701_IOMMU_PAGE_SIZE;

	/*
	 * create mapping will call domain_init, which will use pimudev,
	 * so this need to be set before calling arm_iommu_create_mapping.
	 */
	pimudev = &pdev->dev;
	mtk_mapping = arm_iommu_create_mapping(&platform_bus_type,
					       piommu->iova_base,
					       piommu->iova_size);
	if (IS_ERR(mtk_mapping))
		return PTR_ERR(mtk_mapping);

	domain = mtk_mapping->domain;
	mtk_domain = domain->priv;
	mtk_domain->piommuinfo = piommu;
	piommu->pgt_basepa = mtk_domain->pgtablepa;
	ret = piommu->imucfg->hw_init(piommu);
	if (ret) {
		dev_err(piommu->dev, "IOMMU HW init failed\n");
		goto err_release_mapping;
	}

	piommu->dev->archdata.iommu = mtk_mapping;

	ret = devm_request_irq(piommu->dev, piommu->irq,
			       piommu->imucfg->iommu_isr, IRQF_TRIGGER_NONE,
			       dev_name(piommu->dev), (void *)mtk_domain);
	if (ret) {
		dev_err(piommu->dev, "IRQ request %d failed\n",
			piommu->irq);
		goto err_release_mapping;
	}

	dev_set_drvdata(piommu->dev, piommu);
	dev_info(piommu->dev, "iommu probe suc\n");

	return 0;

err_release_mapping:
	piommu->dev->archdata.iommu = NULL;
	arm_iommu_release_mapping(mtk_mapping);
	piommu->imucfg->hw_deinit(piommu);
	pimudev = NULL;
	return ret;
}

static int mtk_iommu_remove(struct platform_device *pdev)
{
	struct dma_iommu_mapping *mtk_mapping;
	struct mtk_iommu_info *piommu = dev_get_drvdata(&pdev->dev);

	dev_info(piommu->dev, "iommu_remove\n");
	mtk_mapping = to_dma_iommu_mapping(&pdev->dev);
	arm_iommu_release_mapping(mtk_mapping);

	return 0;
}

static struct platform_driver mtk_iommu_driver = {
	.probe	= mtk_iommu_probe,
	.remove	= mtk_iommu_remove,
	.driver	= {
		.name = "mtk-iommu",
		.of_match_table = mtk_iommu_of_ids,
	}
};

static int __init mtk_iommu_init(void)
{
	int ret;

	ret = bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);
	if (ret)
		return ret;

	ret = platform_driver_register(&mtk_iommu_driver);
	if (ret)
		bus_set_iommu(&platform_bus_type, NULL);

	return ret;
}

subsys_initcall(mtk_iommu_init);

MODULE_AUTHOR("Yong Wu <yong.wu@mediatek.com>");
MODULE_AUTHOR("Honghui Zhang <honghui.zhang@mediatek.com>");
MODULE_DESCRIPTION("IOMMU API for MTK architected implementations");
MODULE_LICENSE("GPL v2");

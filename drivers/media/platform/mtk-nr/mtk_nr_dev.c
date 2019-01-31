/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Huiguo.Zhu <huiguo.zhu@mediatek.com>
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

#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/errno.h>

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include <soc/mediatek/smi.h>
#include <linux/mfd/syscon.h>
#include <linux/pm_runtime.h>
#include <linux/iommu.h>
#include <linux/regmap.h>

#include "mtk_nr_def.h"
#include "mtk_nr_ctrl.h"
#include "mtk_nr_m2m.h"

static const char * const _ap_mtk_nr_clk_name[MTK_NR_CLK_CNT] = {
	"bdp_nr_b",
	"bdp_nr_d",
	"bdp_nr_agent",
	"bdp_bridge_b",
	"bdp_bridge_d",
	"bdp_larb_d",
	"smi_common1"
};

static const struct mtk_nr_pix_max mtk_nr_size_max = {
	.org_w = 4096,
	.org_h = 2176,
	.target_w = 4096,
	.target_h = 2176,
};

static const struct mtk_nr_pix_min mtk_nr_size_min = {
	.org_w = 16,
	.org_h = 16,
	.target_w = 16,
	.target_h = 16,
};

static const struct mtk_nr_pix_align mtk_nr_size_align = {
	.org_w = 16,
	.org_h = 16,
	.target_w = 16,
	.target_h = 16,
};

static const struct mtk_nr_variant mtk_nr_default_variant = {
	.pix_max = &mtk_nr_size_max,
	.pix_min = &mtk_nr_size_min,
	.pix_align = &mtk_nr_size_align,
};

static const struct of_device_id mtk_nr_match[] = {
	{
	 .compatible = "mediatek,mt2712-nr",
	 .data = NULL,
	},
	{},
};


MODULE_DEVICE_TABLE(of, mtk_nr_match);

static int mtk_nr_clock_on(struct mtk_nr_dev *nr)
{
	int ret;
	int i;

	for (i = 0; i < MTK_NR_CLK_CNT; i++) {
		ret = clk_prepare_enable(nr->clks[i]);
		if (ret) {
			dev_err(&nr->pdev->dev, "[NR] fail to enable clk %s\n", _ap_mtk_nr_clk_name[i]);
			for (i -= 1; i >= 0; i--)
				clk_disable_unprepare(nr->clks[i]);
			break;
		}
	}

	return ret;
}

static void mtk_nr_clock_off(struct mtk_nr_dev *nr)
{
	int i;

	for (i = 0; i < MTK_NR_CLK_CNT; i++)
		clk_disable_unprepare(nr->clks[i]);
}

static irqreturn_t MTK_NR_IrqHandler(int irq, void *dev_id)
{
	struct mtk_nr_dev *nr_dev = dev_id;

	MTK_NR_Clear_Irq((unsigned long)nr_dev->nr_reg_base);

	atomic_set(&nr_dev->wait_nr_irq_flag, 1);

	wake_up_interruptible(&nr_dev->wait_nr_irq_handle);

	return IRQ_HANDLED;
}

static int mtk_nr_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mtk_nr_dev *nr;
	struct device *dev = &pdev->dev;
	struct device_node *node;
	int ret;
	int i;
	struct iommu_domain *domain;

	domain = iommu_get_domain_for_dev(&pdev->dev);
	if (!domain) {
		dev_err(dev, "[NR][IOMMU]waiting iommu driver ready\n");
		return -EPROBE_DEFER;
	}

	nr = devm_kzalloc(dev, sizeof(struct mtk_nr_dev), GFP_KERNEL);
	if (!nr)
		return -ENOMEM;

	nr->id = pdev->id;
	nr->variant = &mtk_nr_default_variant;
	nr->pdev = pdev;

	for (i = 0; i < MTK_NR_CLK_CNT; i++) {
		nr->clks[i] = devm_clk_get(dev, _ap_mtk_nr_clk_name[i]);
		if (IS_ERR(nr->clks[i])) {
			dev_err(dev, "[NR]fail to get clk[%d] %s\n", i, _ap_mtk_nr_clk_name[i]);
			return PTR_ERR(nr->clks[i]);
		}
	}

	init_waitqueue_head(&nr->irq_queue);
	init_waitqueue_head(&nr->wait_nr_irq_handle);
	mutex_init(&nr->lock);

	nr->workqueue = create_singlethread_workqueue(MTK_NR_MODULE_NAME);
	if (!nr->workqueue) {
		dev_err(dev, "[NR]unable to alloc workqueue\n");
		ret = -ENOMEM;
		goto err_workqueue_create;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "[NR]failed to get MEM resource\n");
		ret =  -ENXIO;
		goto err_register_map;
	}

	nr->nr_reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(nr->nr_reg_base)) {
		dev_err(dev, "[NR] map nr va addr error 0x%lx\n", (unsigned long)nr->nr_reg_base);
		ret = PTR_ERR(nr->nr_reg_base);
		goto err_register_map;
	}

	node = of_parse_phandle(pdev->dev.of_node, "mediatek,bdpsys-regmap", 0);
	if (node) {
		nr->bdpsys_reg_base = syscon_node_to_regmap(node);
		if (IS_ERR(nr->bdpsys_reg_base)) {
			dev_err(dev, "[NR]get fail _bdpsys_reg_base = 0x%lx\n", (unsigned long)nr->bdpsys_reg_base);
			ret =  PTR_ERR(nr->bdpsys_reg_base);
			goto err_register_map;
		}
	} else {
		dev_err(dev, "[NR]fail to get bdpsys node\n");
		ret = -EINVAL;
		goto err_register_map;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "[NR]failed to get IRQ resource\n");
		ret = -ENXIO;
		goto err_irq_map;
	}

	nr->irq = platform_get_irq(pdev, 0);
	if (nr->irq < 0) {
		ret = -ENODEV;
		goto err_irq_map;
	}

	ret = devm_request_irq(dev, nr->irq, MTK_NR_IrqHandler, 0, pdev->name, nr);
	if (ret) {
		dev_err(dev, "[NR]failed to install irq (%d)\n", ret);
		ret = -ENXIO;
		goto err_irq_map;
	}

	ret = v4l2_device_register(dev, &nr->v4l2_dev);
	if (ret) {
		dev_err(dev, "[NR]Failed to register v4l2 device\n");
		ret = -EINVAL;
		goto err_v4l2_dev_register;
	}

	ret = mtk_nr_register_m2m_device(nr);
	if (ret)
		goto err_m2m_dev_register;

	/*
	 * if device has no max_seg_size set, we assume that there is no limit
	 * and force it to DMA_BIT_MASK(32) to always use contiguous mappings
	 * in DMA address space
	 */
	ret = vb2_dma_contig_set_max_seg_size(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		goto err_dam_param;

	platform_set_drvdata(pdev, nr);

	pm_runtime_enable(dev);

	return 0;

err_dam_param:
	mtk_nr_unregister_m2m_device(nr);

err_m2m_dev_register:
	v4l2_device_unregister(&nr->v4l2_dev);

err_v4l2_dev_register:
	devm_free_irq(dev, res->start, nr);

err_irq_map:
err_register_map:
	flush_workqueue(nr->workqueue);
	destroy_workqueue(nr->workqueue);

err_workqueue_create:
	mutex_destroy(&nr->lock);

	return ret;
}

static int mtk_nr_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct mtk_nr_dev *nr = platform_get_drvdata(pdev);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		dev_err(&pdev->dev, "[NR]fail to get irq resource\n");
	else
		devm_free_irq(&pdev->dev, res->start, nr);

	pm_runtime_disable(&pdev->dev);
	vb2_dma_contig_clear_max_seg_size(&pdev->dev);

	mtk_nr_unregister_m2m_device(nr);
	v4l2_device_unregister(&nr->v4l2_dev);

	flush_workqueue(nr->workqueue);
	destroy_workqueue(nr->workqueue);

	mutex_destroy(&nr->lock);

	return 0;
}

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int mtk_nr_pm_suspend(struct device *dev)
{
	struct mtk_nr_dev *nr = dev_get_drvdata(dev);

	mtk_nr_clock_off(nr);

	return 0;
}

static int mtk_nr_pm_resume(struct device *dev)
{
	int ret;
	struct mtk_nr_dev *nr = dev_get_drvdata(dev);

	ret = mtk_nr_clock_on(nr);

	if (!ret)
		MTK_NR_Param_Init((unsigned long)nr->nr_reg_base);

	return ret;
}
#endif				/* CONFIG_PM_RUNTIME || CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_SLEEP
static int mtk_nr_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_nr_pm_suspend(dev);
}

static int mtk_nr_resume(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_nr_pm_resume(dev);
}
#endif				/* CONFIG_PM_SLEEP */

static const struct dev_pm_ops mtk_nr_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_nr_suspend, mtk_nr_resume)
	    SET_RUNTIME_PM_OPS(mtk_nr_pm_suspend, mtk_nr_pm_resume, NULL)
};

static struct platform_driver mtk_nr_driver = {
	.probe = mtk_nr_probe,
	.remove = mtk_nr_remove,
	.driver = {
		   .name = MTK_NR_MODULE_NAME,
		   .owner = THIS_MODULE,
		   .pm = &mtk_nr_pm_ops,
		   .of_match_table = mtk_nr_match,
		}
};

module_platform_driver(mtk_nr_driver);

MODULE_AUTHOR("HUiguo Zhu <huiguo.zhu@mediatek.com>");
MODULE_DESCRIPTION("Mediatek Noise Reduce driver");
MODULE_LICENSE("GPL");

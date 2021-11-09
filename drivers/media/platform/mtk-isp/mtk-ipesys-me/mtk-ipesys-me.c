// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include "mtk_imgsys-engine.h"
#include "mtk-ipesys-me.h"
#ifdef TF_DUMP
#include "mtk_iommu_ext.h"
#include <dt-bindings/memory/mt8195-larb-port.h>
#endif
#include "iommu_debug.h"

#ifdef TF_DUMP_71_1
#include <dt-bindings/memory/mt6983-larb-port.h>
#elif defined(TF_DUMP_71_2)
#include <dt-bindings/memory/mt6879-larb-port.h>
#endif

struct clk_bulk_data imgsys_isp7_me_clks[] = {
	{ .id = "ME_CG_IPE" },
	{ .id = "ME_CG_IPE_TOP" },
	{ .id = "ME_CG" },
	{ .id = "ME_CG_LARB12" },
};

static struct ipesys_me_device *me_dev;

#if defined(TF_DUMP_71_1) || defined(TF_DUMP_71_2)
int ME_TranslationFault_callback(int port, dma_addr_t mva, void *data)
{

	void __iomem *meRegBA = 0L;
	unsigned int i;
	/* iomap registers */
	meRegBA = me_dev->regs;
	if (!meRegBA) {
		pr_info("%s Unable to ioremap dip registers\n",
		__func__);
	}
	for (i = ME_CTL_OFFSET; i <= ME_CTL_OFFSET + ME_CTL_RANGE_TF; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x15320000 + i),
		(unsigned int)ioread32((void *)(meRegBA + i)),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0xC))));
	}

	return 1;
}
#endif

void ipesys_me_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	#ifdef ME_CLK_CTRL
	int ret;
	#endif

	pr_info("%s: +\n", __func__);

	#ifdef ME_CLK_CTRL
	pm_runtime_get_sync(me_dev->dev);
	ret = clk_bulk_prepare_enable(me_dev->me_clk.clk_num, me_dev->me_clk.clks);
	if (ret) {
		pr_info("failed to enable clock:%d\n", ret);
		return;
	}
	#endif
#if defined(TF_DUMP_71_1)
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IPE_ME_RDMA,
	(mtk_iommu_fault_callback_t)ME_TranslationFault_callback,
	NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_IPE_ME_WDMA,
	(mtk_iommu_fault_callback_t)ME_TranslationFault_callback,
	NULL, false);
#elif defined(TF_DUMP_71_2)
	mtk_iommu_register_fault_callback(M4U_LARB12_PORT4,
	(mtk_iommu_fault_callback_t)ME_TranslationFault_callback,
	NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB12_PORT5,
	(mtk_iommu_fault_callback_t)ME_TranslationFault_callback,
	NULL, false);
#endif
	pr_info("%s: -\n", __func__);
}
EXPORT_SYMBOL(ipesys_me_set_initial_value);

void ipesys_me_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	pr_debug("%s: +\n", __func__);
	#ifdef ME_CLK_CTRL
	pm_runtime_put_sync(me_dev->dev);
	clk_bulk_disable_unprepare(me_dev->me_clk.clk_num, me_dev->me_clk.clks);
	#endif
	pr_debug("%s: -\n", __func__);
}
EXPORT_SYMBOL(ipesys_me_uninit);

void ipesys_me_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
			unsigned int engine)
{
	void __iomem *meRegBA = 0L;
	unsigned int i;

	pr_info("%s\n", __func__);
	/* iomap registers */
	meRegBA = me_dev->regs;
	if (!meRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
	}
	dev_info(imgsys_dev->dev, "%s: dump me regs\n", __func__);
	for (i = ME_CTL_OFFSET; i <= ME_CTL_OFFSET + ME_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x15320000 + i),
		(unsigned int)ioread32((void *)(meRegBA + i)),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0xC))));
	}
}
EXPORT_SYMBOL(ipesys_me_debug_dump);

void ipesys_me_debug_dump_local(void)
{
	void __iomem *meRegBA = 0L;
	unsigned int i;

	pr_info("%s\n", __func__);
	/* iomap registers */
	meRegBA = me_dev->regs;
	if (!meRegBA) {
		pr_info("ipesys %s Unable to ioremap dip registers\n",
			__func__);
	}
	pr_info("ipesys %s: dump me regs\n", __func__);
	for (i = ME_CTL_OFFSET; i <= ME_CTL_OFFSET + ME_CTL_RANGE; i += 0x10) {
		pr_info("ipesys %s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x15320000 + i),
		(unsigned int)ioread32((void *)(meRegBA + i)),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0xC))));
	}
}
EXPORT_SYMBOL(ipesys_me_debug_dump_local);



struct device *ipesys_me_getdev(void)
{
	return me_dev->dev;
}

static int mtk_ipesys_me_probe(struct platform_device *pdev)
{
	int ret = 0;
	int ret_result = 0;
	struct device_link *link;
	int larbs_num;
	struct device_node *larb_node;
	struct platform_device *larb_pdev;

	pr_info("mtk ipesys me probe start\n");
	me_dev = devm_kzalloc(&pdev->dev, sizeof(struct ipesys_me_device) * 1, GFP_KERNEL);
	if (!me_dev)
		return -ENOMEM;

	me_dev->dev = &pdev->dev;
	me_dev->me_clk.clk_num = ARRAY_SIZE(imgsys_isp7_me_clks);
	me_dev->me_clk.clks = imgsys_isp7_me_clks;
	me_dev->regs = of_iomap(pdev->dev.of_node, 0);
	ret = devm_clk_bulk_get(&pdev->dev, me_dev->me_clk.clk_num, me_dev->me_clk.clks);
	if (ret) {
		pr_info("failed to get raw clock:%d\n", ret);
	}

	larbs_num = of_count_phandle_with_args(pdev->dev.of_node,
						"mediatek,larb", NULL);
	dev_info(me_dev->dev, "%d larbs to be added", larbs_num);

	larb_node = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!larb_node) {
		dev_info(me_dev->dev,
			"%s: larb node not found\n", __func__);
	}

	larb_pdev = of_find_device_by_node(larb_node);
	if (!larb_pdev) {
		of_node_put(larb_node);
		dev_info(me_dev->dev,
			"%s: larb device not found\n", __func__);
	}
	of_node_put(larb_node);

	link = device_link_add(&pdev->dev, &larb_pdev->dev,
			DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);

	if (!link)
		dev_info(me_dev->dev, "unable to link SMI LARB\n");

	pm_runtime_enable(&pdev->dev);

	pr_info("mtk imgsys me probe done\n");

	return ret_result;
}

static int mtk_ipesys_me_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	devm_kfree(&pdev->dev, me_dev);

	return 0;
}

static int __maybe_unused mtk_ipesys_me_runtime_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int __maybe_unused mtk_ipesys_me_runtime_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int __maybe_unused mtk_ipesys_me_pm_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int __maybe_unused mtk_ipesys_me_pm_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops mtk_ipesys_me_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		mtk_ipesys_me_pm_suspend,
		mtk_ipesys_me_pm_resume
	)
	SET_RUNTIME_PM_OPS(
		mtk_ipesys_me_runtime_suspend,
		mtk_ipesys_me_runtime_resume,
		NULL
	)
};

static const struct of_device_id mtk_ipesys_me_of_match[] = {
	{ .compatible = "mediatek,ipesys-me", },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_ipesys_me_of_match);

static struct platform_driver mtk_ipesys_me_driver = {
	.probe   = mtk_ipesys_me_probe,
	.remove  = mtk_ipesys_me_remove,
	.driver  = {
		.name = "camera-me",
		.owner	= THIS_MODULE,
		.pm = &mtk_ipesys_me_pm_ops,
		.of_match_table = mtk_ipesys_me_of_match,
	}
};

module_platform_driver(mtk_ipesys_me_driver);

MODULE_AUTHOR("Marvin Lin <marvin.lin@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek ME driver");


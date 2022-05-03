// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Marvin Lin <Marvin.Lin@mediatek.com>
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
#include "./../mtk_imgsys-engine.h"
#include "mtk_imgsys-me.h"
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

void imgsys_me_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
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
#ifndef CONFIG_FPGA_EARLY_PORTING
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
#endif
	pr_info("%s: -\n", __func__);
}
//EXPORT_SYMBOL(imgsys_me_set_initial_value);

void imgsys_me_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	pr_debug("%s: +\n", __func__);
	#ifdef ME_CLK_CTRL
	pm_runtime_put_sync(me_dev->dev);
	clk_bulk_disable_unprepare(me_dev->me_clk.clk_num, me_dev->me_clk.clks);
	#endif
	pr_debug("%s: -\n", __func__);
}
//EXPORT_SYMBOL(ipesys_me_uninit);

void imgsys_me_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
			unsigned int engine)
{
	void __iomem *meRegBA = 0L;
	unsigned int i;

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
//EXPORT_SYMBOL(ipesys_me_debug_dump);

void ipesys_me_debug_dump_local(void)
{
	void __iomem *meRegBA = 0L;
	unsigned int i;

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
//EXPORT_SYMBOL(ipesys_me_debug_dump_local);

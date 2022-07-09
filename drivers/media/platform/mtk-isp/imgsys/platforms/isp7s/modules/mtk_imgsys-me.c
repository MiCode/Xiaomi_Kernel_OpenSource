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
#include "iommu_debug.h"

#ifdef WPE_TF_DUMP_7S_1
#include <dt-bindings/memory/mt6985-larb-port.h>
#elif defined(WPE_TF_DUMP_7S_2)
#include <dt-bindings/memory/mt6886-larb-port.h>
#endif


struct clk_bulk_data imgsys_isp7_me_clks[] = {
	{ .id = "ME_CG_IPE" },
	{ .id = "ME_CG_IPE_TOP" },
	{ .id = "ME_CG" },
	{ .id = "ME_CG_LARB12" },
};

//static struct ipesys_me_device *me_dev;
static void __iomem *g_meRegBA;
static void __iomem *g_mmgRegBA;


#if defined(WPE_TF_DUMP_7S_1) || defined(WPE_TF_DUMP_7S_2)
int ME_TranslationFault_callback(int port, dma_addr_t mva, void *data)
{

	void __iomem *meRegBA = 0L;
	unsigned int i;

	/* iomap registers */
	meRegBA = g_meRegBA;
	if (!meRegBA) {
		pr_info("%s Unable to ioremap me registers\n",
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

int MMG_TranslationFault_callback(int port, dma_addr_t mva, void *data)
{

	void __iomem *mmgRegBA = 0L;
	unsigned int i;

	/* iomap registers */
	mmgRegBA = g_mmgRegBA;
	if (!mmgRegBA) {
		pr_info("%s Unable to ioremap mmg registers\n",
		__func__);
	}

	for (i = MMG_CTL_OFFSET; i <= MMG_CTL_OFFSET + MMG_CTL_RANGE_TF; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x15330000 + i),
		(unsigned int)ioread32((void *)(mmgRegBA + i)),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0xC))));
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

	g_meRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ME);
	g_mmgRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ME_MMG);

#if defined(WPE_TF_DUMP_7S_1) || defined(WPE_TF_DUMP_7S_2)
	mtk_iommu_register_fault_callback(M4U_PORT_L12_ME_RDMA_0,
	(mtk_iommu_fault_callback_t)ME_TranslationFault_callback,
	NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_ME_WDMA_0,
	(mtk_iommu_fault_callback_t)ME_TranslationFault_callback,
	NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_MEMMG_RDMA_0,
	(mtk_iommu_fault_callback_t)MMG_TranslationFault_callback,
	NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L12_MEMMG_WDMA_0,
	(mtk_iommu_fault_callback_t)MMG_TranslationFault_callback,
	NULL, false);
#endif
	pr_info("%s: -\n", __func__);
}
//EXPORT_SYMBOL(imgsys_me_set_initial_value);

void imgsys_me_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	pr_debug("%s: +\n", __func__);
	if (g_meRegBA) {
		iounmap(g_meRegBA);
		g_meRegBA = 0L;
	}
	if (g_mmgRegBA) {
		iounmap(g_mmgRegBA);
		g_mmgRegBA = 0L;
	}
	pr_debug("%s: -\n", __func__);
}
//EXPORT_SYMBOL(ipesys_me_uninit);

void imgsys_me_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
			unsigned int engine)
{
	void __iomem *meRegBA = 0L;
	void __iomem *mmgRegBA = 0L;
	unsigned int i;

	/* iomap registers */
	meRegBA = g_meRegBA;
	if (!meRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap ME registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
	}
	mmgRegBA = g_mmgRegBA;
	if (!mmgRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap MMG registers\n",
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
	dev_info(imgsys_dev->dev, "%s: dump mmg regs\n", __func__);
	for (i = MMG_CTL_OFFSET; i <= MMG_CTL_OFFSET + MMG_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x15330000 + i),
		(unsigned int)ioread32((void *)(mmgRegBA + i)),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0xC))));
	}
}
//EXPORT_SYMBOL(ipesys_me_debug_dump);

void ipesys_me_debug_dump_local(void)
{
	void __iomem *meRegBA = 0L;
	void __iomem *mmgRegBA = 0L;
	unsigned int i;

	/* iomap registers */
	meRegBA = g_meRegBA;
	if (!meRegBA) {
		pr_info("imgsys %s Unable to ioremap me registers\n",
			__func__);
	}
	mmgRegBA = g_mmgRegBA;
	if (!mmgRegBA) {
		pr_info("imgsys %s Unable to ioremap mmg registers\n",
			__func__);
	}
	pr_info("imgsys %s: dump me regs\n", __func__);
	for (i = ME_CTL_OFFSET; i <= ME_CTL_OFFSET + ME_CTL_RANGE; i += 0x10) {
		pr_info("imgsys %s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x15320000 + i),
		(unsigned int)ioread32((void *)(meRegBA + i)),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0xC))));
	}
	pr_info("imgsys %s: dump mmg regs\n", __func__);
	for (i = MMG_CTL_OFFSET; i <= MMG_CTL_OFFSET + MMG_CTL_RANGE; i += 0x10) {
		pr_info("imgsys %s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x15330000 + i),
		(unsigned int)ioread32((void *)(mmgRegBA + i)),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0xC))));
	}
}
//EXPORT_SYMBOL(ipesys_me_debug_dump_local);

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include "mtk_imgsys-dip.h"

const struct mtk_imgsys_init_array mtk_imgsys_dip_init_ary[] = {
	{0x094, 0x80000000},	/* DIPCTL_D1A_DIPCTL_INT1_EN */
	{0x0A0, 0x0},	/* DIPCTL_D1A_DIPCTL_INT2_EN */
	{0x0AC, 0x0},	/* DIPCTL_D1A_DIPCTL_INT3_EN */
	{0x0C4, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT1_EN */
	{0x0D0, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT2_EN */
	{0x0DC, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT3_EN */
	{0x208, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR0_CTL */
	{0x218, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR1_CTL */
	{0x228, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR2_CTL */
	{0x238, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR3_CTL */
	{0x248, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR4_CTL */
	{0x258, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR5_CTL */
	{0x268, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR6_CTL */
	{0x278, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR7_CTL */
	{0x288, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR8_CTL */
	{0x298, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR9_CTL */
	{0x2A8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR10_CTL */
	{0x2B8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR11_CTL */
	{0x2C8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR12_CTL */
	{0x2D8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR13_CTL */
	{0x2E8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR14_CTL */
	{0x2F8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR15_CTL */
	{0x308, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR16_CTL */
	{0x318, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR17_CTL */
	{0x328, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR18_CTL */
};

#define DIP_HW_SET 2

void __iomem *gdipRegBA[DIP_HW_SET] = {0L};

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int hw_idx = 0, ary_idx = 0;

	pr_debug("%s: +\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	for (hw_idx = REG_MAP_E_DIP; hw_idx <= REG_MAP_E_DIP_NR; hw_idx++) {
		/* iomap registers */
		ary_idx = hw_idx - REG_MAP_E_DIP;
		gdipRegBA[ary_idx] = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		if (!gdipRegBA[ary_idx]) {
			dev_info(imgsys_dev->dev,
				"%s: error: unable to iomap dip_%d registers, devnode(%s).\n",
				__func__, hw_idx, imgsys_dev->dev->of_node->name);
			continue;
		}
	}

	pr_debug("%s: -\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_dip_set_initial_value);

void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *dipRegBA = 0L;
	void __iomem *ofset = NULL;
	unsigned int i;

	/* iomap registers */
	dipRegBA = gdipRegBA[0]; // dip: 0x15100000

for (i = 0 ; i < sizeof(mtk_imgsys_dip_init_ary)/sizeof(struct mtk_imgsys_init_array) ; i++) {
	ofset = dipRegBA + mtk_imgsys_dip_init_ary[i].ofset;
	writel(mtk_imgsys_dip_init_ary[i].val, ofset);
}
}
EXPORT_SYMBOL(imgsys_dip_set_hw_initial_value);

void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine)
{
	void __iomem *dipRegBA = 0L;
	unsigned int i;

	pr_info("%s: +\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	/* iomap registers */
	dipRegBA = gdipRegBA[0]; // dip: 0x15100000

	dev_info(imgsys_dev->dev, "%s: dump dip ctl regs\n", __func__);
	for (i = TOP_CTL_OFFSET; i <= TOP_CTL_OFFSET + TOP_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = DMATOP_OFFSET; i <= DMATOP_OFFSET + DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = RDMA_OFFSET; i <= RDMA_OFFSET + RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = WDMA_OFFSET; i <= WDMA_OFFSET + WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump nr3d ctl regs\n", __func__);
	for (i = NR3D_CTL_OFFSET; i <= NR3D_CTL_OFFSET + NR3D_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump tnr ctl regs\n", __func__);
	for (i = TNR_CTL_OFFSET; i <= TNR_CTL_OFFSET + TNR_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dipRegBA = gdipRegBA[1]; // dip_nr: 0x15150000
	dev_info(imgsys_dev->dev, "%s: dump mcrop regs\n", __func__);
	for (i = MCRP_OFFSET; i <= MCRP_OFFSET + MCRP_RANGE; i += 0x8) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15150000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)));
	}


	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = N_DMATOP_OFFSET; i <= N_DMATOP_OFFSET + N_DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15150000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15150000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15150000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = N_RDMA_OFFSET; i <= N_RDMA_OFFSET + N_RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15150000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15150000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15150000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = N_WDMA_OFFSET; i <= N_WDMA_OFFSET + N_WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15150000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15150000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15150000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dipRegBA = gdipRegBA[0]; // dip: 0x15100000

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x18 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x1*/
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_tnc\n", __func__);
	iowrite32(0x1801, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: tnc_debug: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x0 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x0~0xD */
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_nr3d\n", __func__);
	iowrite32(0x13, (void *)(dipRegBA + NR3D_DBG_SEL));
	iowrite32(0x00001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_sot_latch_32~1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x20001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_eot_latch_32~1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x10001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_sot_latch_33~39: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x30001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_eot_latch_33~39: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x40001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif4~1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x50001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif8~5: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x60001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif12~9: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x70001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif16~13: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x80001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif20~17: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x90001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif24~21: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0xA0001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif28~25: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0xB0001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif32~29: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0xC0001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif36~33: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0xD0001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif39~37: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));

	//dev_dbg(imgsys_dev->dev, "%s: +\n",__func__);
	//
	pr_info("%s: -\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_dip_debug_dump);

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i;

	pr_debug("%s: +\n", __func__);

	for (i = 0; i < DIP_HW_SET; i++) {
		iounmap(gdipRegBA[i]);
		gdipRegBA[i] = 0L;
	}

	pr_debug("%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_dip_uninit);

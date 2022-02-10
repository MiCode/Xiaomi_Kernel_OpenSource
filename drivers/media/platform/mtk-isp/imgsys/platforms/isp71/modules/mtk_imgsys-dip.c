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
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = DMATOP_OFFSET; i <= DMATOP_OFFSET + DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = RDMA_OFFSET; i <= RDMA_OFFSET + RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = WDMA_OFFSET; i <= WDMA_OFFSET + WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump nr3d ctl regs\n", __func__);
	for (i = NR3D_CTL_OFFSET; i <= NR3D_CTL_OFFSET + NR3D_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump tnr ctl regs\n", __func__);
	for (i = TNR_CTL_OFFSET; i <= TNR_CTL_OFFSET + TNR_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dipRegBA = gdipRegBA[1]; // dip_nr: 0x15150000
	dev_info(imgsys_dev->dev, "%s: dump mcrop regs\n", __func__);
	for (i = MCRP_OFFSET; i <= MCRP_OFFSET + MCRP_RANGE; i += 0x8) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}


	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = N_DMATOP_OFFSET; i <= N_DMATOP_OFFSET + N_DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = N_RDMA_OFFSET; i <= N_RDMA_OFFSET + N_RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = N_WDMA_OFFSET; i <= N_WDMA_OFFSET + N_WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dipRegBA = gdipRegBA[0]; // dip: 0x15100000

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x88 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x1*/
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_tnc\n", __func__);
	iowrite32(0x18801, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: tnc_debug: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));


for (i = 0; i <= 6; i++) {
	iowrite32((0x15 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000015 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x115 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000115 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x215 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000215 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x315 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000315 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x415 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000415 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x515 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000515 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x615 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000615 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x715 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000715 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x815 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000815 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
}

for (i = 0; i <= 6; i++) {
	iowrite32((0x23 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000023 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x123 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000123 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x223 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000223 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x323 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000323 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x423 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000423 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x523 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000523 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x623 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000623 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x723 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000723 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x823 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000823 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x923 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000923 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
}

	dipRegBA = gdipRegBA[1]; // dip: 0x15150000

for (i = 0; i <= 9; i++) {
	iowrite32((0x3 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000003 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x103 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000103 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x203 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000203 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x303 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000303 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x403 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000403 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x503 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000503 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x603 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000603 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x703 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000703 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x803 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000803 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
}

for (i = 0; i <= 9; i++) {
	iowrite32((0x17 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000017 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x117 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000117 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x217 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000217 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x317 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000317 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x417 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000417 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x517 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000517 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x617 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000617 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x717 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000717 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x817 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000817 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x917 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000917 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
}

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

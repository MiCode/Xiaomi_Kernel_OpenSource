// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Floria Huang <floria.huang@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>

#ifdef TF_DUMP
#include <dt-bindings/memory/mt8195-larb-port.h>
#include "mtk_iommu_ext.h"
#endif

#include "mtk_imgsys-wpe.h"

#define WPE_A_BASE        (0x15200000)
#define WPE_BASE_OFST     (0x10000)

//CTL_MOD_EN
#define PQDIP_DL  0x40000
#define DIP_DL    0x80000
#define TRAW_DL   0x100000

// for CQ_THR0_CTL ~ CQ_THR14CTL
#define CQ_THRX_CTL_EN (1L << 0)
#define CQ_THRX_CTL_MODE (1L << 4)//immediately mode
#define CQ_THRX_CTL	(CQ_THRX_CTL_EN | CQ_THRX_CTL_MODE)

const struct mtk_imgsys_init_array
			mtk_imgsys_wpe_init_ary[] = {
	{0x0018, 0x80000000}, /* WPE_TOP_CTL_INT_EN, en w-clr */
	{0x0024, 0xFFFFFFFF}, /* WPE_TOP_CTL_INT_STATUSX, w-clr */
	{0x00D4, 0x80000000}, /* WPE_TOP_CQ_IRQ_EN, en w-clr */
	{0x00DC, 0xFFFFFFFF}, /* WPE_TOP_CQ_IRQ_STX, w-clr */
	{0x00E0, 0x80000000}, /* WPE_TOP_CQ_IRQ_EN2, en w-clr */
	{0x00E8, 0xFFFFFFFF}, /* WPE_TOP_CQ_IRQ_STX2, w-clr */
	{0x00EC, 0x80000000}, /* WPE_TOP_CQ_IRQ_EN3, en w-clr */
	{0x00F4, 0xFFFFFFFF}, /* WPE_TOP_CQ_IRQ_STX3, w-clr */
	{0x0204, 0x00000002}, /* WPE_CACHE_RWCTL_CTL */
	{0x03D4, 0x80000000}, /* WPE_DMA_DMA_ERR_CTRL */
	{0x0A10, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR0_CTL */
	{0x0A1C, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR1_CTL */
	{0x0A28, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR2_CTL */
	{0x0A34, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR3_CTL */
	{0x0A40, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR4_CTL */
	{0x0A4C, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR5_CTL */
	{0x0A58, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR6_CTL */
	{0x0A64, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR7_CTL */
	{0x0A70, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR8_CTL */
	{0x0A7C, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR9_CTL */
	{0x0A88, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR10_CTL */
	{0x0A94, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR11_CTL */
	{0x0AA0, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR12_CTL */
	{0x0AAC, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR13_CTL */
	{0x0AB8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR14_CTL */
};
#define WPE_INIT_ARRAY_COUNT  ARRAY_SIZE(mtk_imgsys_wpe_init_ary)

struct imgsys_reg_range {
	uint32_t str;
	uint32_t end;
};
const struct imgsys_reg_range wpe_regs[] = {
	{0x0000, 0x0164}, /* TOP,VECI,VEC2I */
	{0x0200, 0x0248}, /* CACHE */
	{0x0300, 0x032C}, /* WPEO */
	{0x0340, 0x036C}, /* WPEO2 */
	{0x0380, 0x03A8}, /* MSKO */
	{0x03C0, 0x0408}, /* DMA */
	{0x0440, 0x0448}, /* TDRI */
	{0x04C0, 0x0504}, /* VGEN */
	{0x0540, 0x05D4}, /* PSP */
	{0x0600, 0x0620}, /* C24,C02 */
	{0x0640, 0x0654}, /* DL CROP */
	{0x0680, 0x0694}, /* DMA CROP */
	{0x06C0, 0x0784}, /* DEC,PAK */
	{0x07C0, 0x07D0}, /* TOP2 */
	{0x0800, 0x080C},
	{0x0A00, 0x0AF0}, /* DIPCQ_W1 */
};
#define WPE_REG_ARRAY_COUNT	ARRAY_SIZE(wpe_regs)

#ifdef TF_DUMP
int imgsys_wpe_tfault_callback(int port,
	unsigned long mva, void *data)
{
	void __iomem *wpeRegBA = (void __iomem *) data;
	unsigned int i, j;
	unsigned int wpeBase = 0;
	unsigned int engine;

	pr_debug("%s: +\n", __func__);

	if (data == NULL) {
		pr_info("%s: pull data pointer!\n", __func__);
		return 0;
	}

	switch (port) {
	case M4U_PORT_L11_IMG_WPE_EIS_RDMA0_A:
	case M4U_PORT_L11_IMG_WPE_EIS_RDMA1_A:
	case M4U_PORT_L11_IMG_WPE_EIS_WDMA0_A:
	case M4U_PORT_L11_IMG_WPE_EIS_CQ0_A:
	case M4U_PORT_L11_IMG_WPE_EIS_CQ1_A:
		engine = REG_MAP_E_WPE_EIS;
		break;
	case M4U_PORT_L11_IMG_WPE_TNR_RDMA0_A:
	case M4U_PORT_L11_IMG_WPE_TNR_RDMA1_A:
	case M4U_PORT_L11_IMG_WPE_TNR_WDMA0_A:
	case M4U_PORT_L11_IMG_WPE_TNR_CQ0_A:
	case M4U_PORT_L11_IMG_WPE_TNR_CQ1_A:
		engine = REG_MAP_E_WPE_TNR;
		break;
	default:
		pr_info("%s: TF port doesn't belongs to WPE.\n\n", __func__, port);
		break;
	};

	/* iomap registers */
	pr_info("%s: ==== Dump WPE_%d, TF port: 0x%x =====",
		__func__, (engine - REG_MAP_E_WPE_EIS), port);

	//
	wpeBase = WPE_A_BASE + (engine - REG_MAP_E_WPE_EIS) * WPE_BASE_OFST;
	for (j = 0; j < WPE_REG_ARRAY_COUNT; j++) {
		for (i = wpe_regs[j].str; i <= wpe_regs[j].end; i += 0x10) {
			pr_info("%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X", __func__,
				(unsigned int)(wpeBase + i),
				(unsigned int)ioread32((void *)(wpeRegBA + i)),
				(unsigned int)ioread32((void *)(wpeRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(wpeRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(wpeRegBA + i + 0xC)));
		}
	}

	return 1;
}
#endif

void imgsys_wpe_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *wpeRegBA = 0L;
	void __iomem *ofset = NULL;
	unsigned int i = 0;
	unsigned int hw_idx = 0;

	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	/* iomap registers */
	for (hw_idx = REG_MAP_E_WPE_EIS; hw_idx <= REG_MAP_E_WPE_TNR; hw_idx++) {
		wpeRegBA = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		for (i = 0 ; i < WPE_INIT_ARRAY_COUNT ; i++) {
			ofset = wpeRegBA + mtk_imgsys_wpe_init_ary[i].ofset;
			writel(mtk_imgsys_wpe_init_ary[i].val, ofset);
		}

#ifdef TF_DUMP
		if (hw_idx == REG_MAP_E_WPE_EIS) {
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_EIS_RDMA0_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_EIS_RDMA1_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_EIS_WDMA0_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_EIS_CQ0_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_EIS_CQ1_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
		} else {
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_TNR_RDMA0_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_TNR_RDMA1_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_TNR_WDMA0_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_TNR_CQ0_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
			mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_WPE_TNR_CQ1_A,
					(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
					(void *) wpeRegBA);
		}
#endif
	}

	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}

void imgsys_wpe_debug_dl_dump(struct mtk_imgsys_dev *imgsys_dev,
							void __iomem *wpeRegBA)
{
	unsigned int dbg_sel_value[3] = {0x0, 0x0, 0x0};
	unsigned int debug_value[3] = {0x0, 0x0, 0x0};
	unsigned int sel_value[3] = {0x0, 0x0, 0x0};

	dbg_sel_value[0] = (0xC << 12); //pqdip
	dbg_sel_value[1] = (0xD << 12); //DIP
	dbg_sel_value[2] = (0xE << 12); //TRAW

	//line & pix cnt
	writel((dbg_sel_value[0] | (0x1 << 8)), (wpeRegBA + 0x48));
	sel_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	writel((dbg_sel_value[1] | (0x1 << 8)), (wpeRegBA + 0x48));
	sel_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	writel((dbg_sel_value[2] | (0x1 << 8)), (wpeRegBA + 0x48));
	sel_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	dev_info(imgsys_dev->dev,
	  "%s: [0x48]dbg_sel,[0x4C](31:16)LnCnt(15:0)PixCnt: PQDIP[0x%x]0x%x, DIP[0x%x]0x%x, TRAW[0x%x]0x%x",
	  __func__,
	  sel_value[0], debug_value[0], sel_value[1], debug_value[1],
	  sel_value[2], debug_value[2]);

	//req/rdy status (output)
	writel((dbg_sel_value[0] | (0x0 << 8)), (wpeRegBA + 0x48));
	sel_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	writel((dbg_sel_value[1] | (0x0 << 8)), (wpeRegBA + 0x48));
	sel_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	writel((dbg_sel_value[2] | (0x0 << 8)), (wpeRegBA + 0x48));
	sel_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	dev_info(imgsys_dev->dev,
	  "%s: [0x48]dbg_sel,[0x4C]REQ/RDY: PQDIP[0x%x]%d/%d, DIP[0x%x]%d/%d, TRAW[0x%x]%d/%d",
	  __func__,
	  sel_value[0], ((debug_value[0] >> 24) & 0x1), ((debug_value[0] >> 23) & 0x1),
	  sel_value[1], ((debug_value[1] >> 24) & 0x1), ((debug_value[1] >> 23) & 0x1),
	  sel_value[2], ((debug_value[2] >> 24) & 0x1), ((debug_value[2] >> 23) & 0x1));
}
EXPORT_SYMBOL(imgsys_wpe_set_initial_value);

void imgsys_wpe_debug_cq_dump(struct mtk_imgsys_dev *imgsys_dev,
							void __iomem *wpeRegBA)
{
	unsigned int dbg_sel_value = 0x0;
	unsigned int debug_value[5] = {0x0};
	unsigned int sel_value[5] = {0x0};

	debug_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + 0xA10));
	debug_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + 0xA1C));
	if (!debug_value[0] || !debug_value[1]) {
		dev_info(imgsys_dev->dev, "%s: No cq_thr enabled! cq0:0x%x, cq1:0x%x",
			__func__, debug_value[0], debug_value[1]);
		return;
	}

	dbg_sel_value = (0x18 << 12);//cq_p2_eng

	//line & pix cnt
	writel((dbg_sel_value | 0x0), (wpeRegBA + 0x48));
	sel_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	writel((dbg_sel_value | 0x1), (wpeRegBA + 0x48));
	sel_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	writel((dbg_sel_value | 0x2), (wpeRegBA + 0x48));
	sel_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	writel((dbg_sel_value | 0x3), (wpeRegBA + 0x48));
	sel_value[3] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[3] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	writel((dbg_sel_value | 0x4), (wpeRegBA + 0x48));
	sel_value[4] = (unsigned int)ioread32((void *)(wpeRegBA + 0x48));
	debug_value[4] = (unsigned int)ioread32((void *)(wpeRegBA + 0x4C));

	dev_info(imgsys_dev->dev,
		"%s: [0x48]dbg_sel,[0x4C]cq_st[0x%x]0x%x, dma_dbg[0x%x]0x%x, dma_req[0x%x]0x%x, dma_rdy[0x%x]0x%x, dma_valid[0x%x]0x%x",
		__func__,
		sel_value[0], debug_value[0], sel_value[1], debug_value[1],
		sel_value[2], debug_value[2], sel_value[3], debug_value[3],
		sel_value[4], debug_value[4]);
}


void imgsys_wpe_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine)
{
	void __iomem *wpeRegBA = 0L;
	unsigned int i, j, ctl_en;
	unsigned int hw_idx = 0;
	unsigned int wpeBase = 0;
	unsigned int startHw = REG_MAP_E_WPE_EIS, endHW = REG_MAP_E_WPE_TNR;

	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	if ((engine & IMGSYS_ENG_WPE_EIS) && !(engine & IMGSYS_ENG_WPE_TNR))
		endHW = REG_MAP_E_WPE_EIS;

	if (!(engine & IMGSYS_ENG_WPE_EIS) && (engine & IMGSYS_ENG_WPE_TNR))
		startHw = REG_MAP_E_WPE_TNR;


	/* iomap registers */
	for (hw_idx = startHw; hw_idx <= endHW; hw_idx++) {
		wpeRegBA = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		wpeBase = WPE_A_BASE + (hw_idx - REG_MAP_E_WPE_EIS) * WPE_BASE_OFST;
		if (!wpeRegBA) {
			dev_info(imgsys_dev->dev,
				"%s: Unable to ioremap wpe registers, devnode(%s).\n",
					__func__, imgsys_dev->dev->of_node->name);
		}
		dev_info(imgsys_dev->dev, "%s: ==== Dump WPE_%d =====",
		  __func__, (hw_idx - REG_MAP_E_WPE_EIS));

		//DL
		ctl_en = (unsigned int)ioread32((void *)(wpeRegBA + 0x4));
		if (ctl_en & (PQDIP_DL|DIP_DL|TRAW_DL)) {
			dev_info(imgsys_dev->dev, "%s: WPE Done: %d", __func__,
			  !(ioread32((void *)(wpeRegBA))) &&
			  (ioread32((void *)(wpeRegBA + 0x24)) & 0x1));
			dev_info(imgsys_dev->dev,
			  "%s: WPE_DL: PQDIP(%d), DIP(%d), TRAW(%d)", __func__,
			  (ctl_en & PQDIP_DL) > 0, (ctl_en & DIP_DL) > 0, (ctl_en & TRAW_DL) > 0);
			imgsys_wpe_debug_dl_dump(imgsys_dev, wpeRegBA);
		}

		imgsys_wpe_debug_cq_dump(imgsys_dev, wpeRegBA);

		//
		for (j = 0; j < WPE_REG_ARRAY_COUNT; j++) {
			for (i = wpe_regs[j].str; i <= wpe_regs[j].end; i += 0x10) {
				dev_info(imgsys_dev->dev,
					"%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X", __func__,
				(unsigned int)(wpeBase + i),
				(unsigned int)ioread32((void *)(wpeRegBA + i)),
				(unsigned int)ioread32((void *)(wpeRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(wpeRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(wpeRegBA + i + 0xC)));
			}
		}

	}
	//
	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_wpe_debug_dump);

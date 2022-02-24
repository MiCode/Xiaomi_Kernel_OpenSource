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
#include <soc/mediatek/smi.h>

#include "iommu_debug.h"
#ifdef WPE_TF_DUMP_71_1
#include <dt-bindings/memory/mt6983-larb-port.h>

#elif defined(WPE_TF_DUMP_71_2)
#include <dt-bindings/memory/mt6879-larb-port.h>
#endif

#define M4U_PORT_DUMMY_EIS  (0)
#define M4U_PORT_DUMMY_TNR  (1)
#define M4U_PORT_DUMMY_LITE (2)

#include "mtk_imgsys-wpe.h"

#define WPE_A_BASE        (0x15200000)
const unsigned int mtk_imgsys_wpe_base_ofst[] = {0x0, 0x300000, 0x400000};
#define WPE_HW_NUM        ARRAY_SIZE(mtk_imgsys_wpe_base_ofst)

//CTL_MOD_EN
#define PQDIP_DL  0x40000
#define DIP_DL    0x80000
#define TRAW_DL   0x100000

// for CQ_THR0_CTL ~ CQ_THR14CTL
#define CQ_THRX_CTL_EN (1L << 0)
#define CQ_THRX_CTL_MODE (1L << 4)//immediately mode
#define CQ_THRX_CTL	(CQ_THRX_CTL_EN | CQ_THRX_CTL_MODE)

// register ofst
#define WPE_REG_DBG_SET     (0x48)
#define WPE_REG_DBG_PORT    (0x4C)
#define WPE_REG_CQ_THR0_CTL (0xB08)
#define WPE_REG_CQ_THR1_CTL (0xB18)
#define WPE_REG_DEC_CTL1    (0x784)

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
	{0x0B08, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR0_CTL */
	{0x0B18, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR1_CTL */
	{0x0B28, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR2_CTL */
	{0x0B38, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR3_CTL */
	{0x0B48, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR4_CTL */
	{0x0B58, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR5_CTL */
	{0x0B68, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR6_CTL */
	{0x0B78, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR7_CTL */
	{0x0B88, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR8_CTL */
	{0x0B98, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR9_CTL */
	{0x0BA8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR10_CTL */
	{0x0BB8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR11_CTL */
	{0x0BC8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR12_CTL */
	{0x0BD8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR13_CTL */
	{0x0BE8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR14_CTL */
};
#define WPE_INIT_ARRAY_COUNT  ARRAY_SIZE(mtk_imgsys_wpe_init_ary)

struct imgsys_reg_range {
	uint32_t str;
	uint32_t end;
};
const struct imgsys_reg_range wpe_regs[] = {
	{0x0000, 0x0164}, /* TOP,VECI,VEC2I */
	{0x0200, 0x027C}, /* CACHE */
	{0x0300, 0x032C}, /* WPEO */
	{0x0340, 0x0368}, /* WPEO2 */
	{0x0380, 0x03A8}, /* MSKO */
	{0x03C0, 0x0408}, /* DMA */
	{0x0440, 0x0450}, /* TDRI */
	{0x04C0, 0x0508}, /* VGEN */
	{0x0540, 0x05D4}, /* PSP */
	{0x0600, 0x0620}, /* C24,C02 */
	{0x0640, 0x0654}, /* DL CROP */
	{0x0680, 0x0694}, /* DMA CROP */
	{0x06C0, 0x07B4}, /* DEC,PAK */
	{0x07C0, 0x07D0}, /* TOP2 */
	{0x0800, 0x080C},
	{0x0B00, 0x0C34}, /* DIPCQ_W1 */
};
#define WPE_REG_ARRAY_COUNT	ARRAY_SIZE(wpe_regs)

void __iomem *gWpeRegBA[WPE_HW_NUM] = {0L};

int imgsys_wpe_tfault_callback(int port,
	dma_addr_t mva, void *data)
{
	void __iomem *wpeRegBA = 0L;
	unsigned int i, j;
	unsigned int wpeBase = 0;
	unsigned int engine;

	pr_debug("%s: +\n", __func__);

	switch (port) {
#ifdef WPE_TF_DUMP_71_1
	case M4U_PORT_L11_IMG2_WPE_RDMA0:
	case M4U_PORT_L11_IMG2_WPE_RDMA1:
	case M4U_PORT_L11_IMG2_WPE_RDMA_4P0:
	case M4U_PORT_L11_IMG2_WPE_RDMA_4P1:
	case M4U_PORT_L11_IMG2_WPE_WDMA0:
	case M4U_PORT_L11_IMG2_WPE_WDMA_4P0:
	case M4U_PORT_L11_IMG2_WPE_CQ0:
	case M4U_PORT_L11_IMG2_WPE_CQ1:
#elif defined(WPE_TF_DUMP_71_2)
	case M4U_LARB11_PORT0:
	case M4U_LARB11_PORT1:
	case M4U_LARB11_PORT2:
	case M4U_LARB11_PORT3:
	case M4U_LARB11_PORT18:
	case M4U_LARB11_PORT19:
	case M4U_LARB11_PORT4:
	case M4U_LARB11_PORT5:
#else
	case M4U_PORT_DUMMY_EIS:
#endif
		engine = REG_MAP_E_WPE_EIS;
		break;
#ifdef WPE_TF_DUMP_71_1
	case M4U_PORT_L22_IMG2_WPE_RDMA0:
	case M4U_PORT_L22_IMG2_WPE_RDMA1:
	case M4U_PORT_L22_IMG2_WPE_RDMA_4P0:
	case M4U_PORT_L22_IMG2_WPE_RDMA_4P1:
	case M4U_PORT_L22_IMG2_WPE_WDMA0:
	case M4U_PORT_L22_IMG2_WPE_WDMA_4P0:
	case M4U_PORT_L22_IMG2_WPE_CQ0:
	case M4U_PORT_L22_IMG2_WPE_CQ1:
#elif defined(WPE_TF_DUMP_71_2)
	case M4U_LARB22_PORT0:
	case M4U_LARB22_PORT1:
	case M4U_LARB22_PORT2:
	case M4U_LARB22_PORT3:
	case M4U_LARB22_PORT18:
	case M4U_LARB22_PORT19:
	case M4U_LARB22_PORT4:
	case M4U_LARB22_PORT5:
#else
	case M4U_PORT_DUMMY_TNR:
#endif
		engine = REG_MAP_E_WPE_TNR;
		break;
#ifdef WPE_TF_DUMP_71_1
	case M4U_PORT_L23_IMG2_WPE_RDMA0:
	case M4U_PORT_L23_IMG2_WPE_RDMA1:
	case M4U_PORT_L23_IMG2_WPE_RDMA_4P0:
	case M4U_PORT_L23_IMG2_WPE_RDMA_4P1:
	case M4U_PORT_L23_IMG2_WPE_WDMA0:
	case M4U_PORT_L23_IMG2_WPE_WDMA_4P0:
	case M4U_PORT_L23_IMG2_WPE_CQ0:
	case M4U_PORT_L23_IMG2_WPE_CQ1:
#elif defined(WPE_TF_DUMP_71_2)
	case M4U_LARB23_PORT0:
	case M4U_LARB23_PORT1:
	case M4U_LARB23_PORT2:
	case M4U_LARB23_PORT3:
	case M4U_LARB23_PORT18:
	case M4U_LARB23_PORT19:
	case M4U_LARB23_PORT4:
	case M4U_LARB23_PORT5:
#else
	case M4U_PORT_DUMMY_LITE:
#endif
		engine = REG_MAP_E_WPE_LITE;
		break;
	default:
		pr_info("%s: TF port doesn't belongs to WPE.\n\n", __func__, port);
		return 0;
	};

	mtk_smi_dbg_hang_detect("WPE");

	/* iomap registers */
	wpeRegBA = gWpeRegBA[engine - REG_MAP_E_WPE_EIS];
	if (!wpeRegBA) {
		pr_info("%s: WPE_%d, RegBA=0", __func__);
		return 1;
	}

	pr_info("%s: ==== Dump WPE_%d, TF port: 0x%x =====",
		__func__, (engine - REG_MAP_E_WPE_EIS), port);

	//
	wpeBase = WPE_A_BASE + mtk_imgsys_wpe_base_ofst[(engine - REG_MAP_E_WPE_EIS)];
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

void imgsys_wpe_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int hw_idx = 0, ary_idx = 0;

	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	for (hw_idx = REG_MAP_E_WPE_EIS; hw_idx <= REG_MAP_E_WPE_LITE; hw_idx++) {
		/* iomap registers */
		ary_idx = hw_idx - REG_MAP_E_WPE_EIS;
		gWpeRegBA[ary_idx] = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		if (!gWpeRegBA[ary_idx]) {
			dev_info(imgsys_dev->dev,
				"%s: error: unable to iomap wpe_%d registers, devnode(%s).\n",
				__func__, hw_idx, imgsys_dev->dev->of_node->name);
			continue;
		}
	}

#ifdef WPE_TF_DUMP_71_1
	//wpe_eis
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG2_WPE_RDMA0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG2_WPE_RDMA1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG2_WPE_RDMA_4P0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG2_WPE_RDMA_4P1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, 0);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG2_WPE_WDMA0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, 0);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG2_WPE_WDMA_4P0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, 0);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG2_WPE_CQ0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG2_WPE_CQ1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	//wpe_tnr
	mtk_iommu_register_fault_callback(M4U_PORT_L22_IMG2_WPE_RDMA0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L22_IMG2_WPE_RDMA1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L22_IMG2_WPE_RDMA_4P0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L22_IMG2_WPE_RDMA_4P1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L22_IMG2_WPE_WDMA0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L22_IMG2_WPE_WDMA_4P0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L22_IMG2_WPE_CQ0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L22_IMG2_WPE_CQ1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	//wpe_lite
	mtk_iommu_register_fault_callback(M4U_PORT_L23_IMG2_WPE_RDMA0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L23_IMG2_WPE_RDMA1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L23_IMG2_WPE_RDMA_4P0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L23_IMG2_WPE_RDMA_4P0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L23_IMG2_WPE_WDMA0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L23_IMG2_WPE_WDMA_4P0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L23_IMG2_WPE_CQ0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_PORT_L23_IMG2_WPE_CQ1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
#elif defined(WPE_TF_DUMP_71_2)
	//wpe_eis
	mtk_iommu_register_fault_callback(M4U_LARB11_PORT0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB11_PORT1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB11_PORT2,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB11_PORT3,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, 0);
	mtk_iommu_register_fault_callback(M4U_LARB11_PORT18,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, 0);
	mtk_iommu_register_fault_callback(M4U_LARB11_PORT19,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, 0);
	mtk_iommu_register_fault_callback(M4U_LARB11_PORT4,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB11_PORT5,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	//wpe_tnr
	mtk_iommu_register_fault_callback(M4U_LARB22_PORT0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB22_PORT1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB22_PORT2,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB22_PORT3,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB22_PORT18,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB22_PORT19,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB22_PORT4,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB22_PORT5,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	//wpe_lite
	mtk_iommu_register_fault_callback(M4U_LARB23_PORT0,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB23_PORT1,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB23_PORT2,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB23_PORT23,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB23_PORT18,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB23_PORT19,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB23_PORT4,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
	mtk_iommu_register_fault_callback(M4U_LARB23_PORT5,
			(mtk_iommu_fault_callback_t)imgsys_wpe_tfault_callback,
			NULL, false);
#endif

	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_wpe_set_initial_value);

void imgsys_wpe_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *ofset = NULL;
	unsigned int i = 0;
	unsigned int hw_idx = 0, ary_idx = 0;

	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	for (hw_idx = REG_MAP_E_WPE_EIS; hw_idx <= REG_MAP_E_WPE_LITE; hw_idx++) {
		/* iomap registers */
		ary_idx = hw_idx - REG_MAP_E_WPE_EIS;
		for (i = 0 ; i < WPE_INIT_ARRAY_COUNT ; i++) {
			ofset = gWpeRegBA[ary_idx] + mtk_imgsys_wpe_init_ary[i].ofset;
			writel(mtk_imgsys_wpe_init_ary[i].val, ofset);
		}
	}

	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_wpe_set_hw_initial_value);

void imgsys_wpe_debug_ufo_dump(struct mtk_imgsys_dev *imgsys_dev,
							void __iomem *wpeRegBA)
{
	unsigned int i;
	unsigned int debug_value[55] = {0x0};
	unsigned int sel_value = 0x0;

	writel((0xB<<12), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	for (i = 0; i < 55; i++) {
		writel((i + 0xC00), (wpeRegBA + WPE_REG_DEC_CTL1));
		debug_value[i] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));
	}

	dev_info(imgsys_dev->dev,
	  "%s: [0x%x]dbg_sel: 0x%X, [0x%x]dec_ctrl1 [0x%x]ufo_st",
	  __func__, WPE_REG_DBG_SET, sel_value, WPE_REG_DEC_CTL1, WPE_REG_DBG_PORT);

	for (i = 0; i <= 10; i++) {
		dev_info(imgsys_dev->dev,
		  "%s: [0x%x] 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X",
		  __func__, (unsigned int)(0xC00+i*5),
		  debug_value[i*5+0], debug_value[i*5+1], debug_value[i*5+2],
		  debug_value[i*5+3], debug_value[i*5+4]);
	}
}

void imgsys_wpe_debug_cache_dump(struct mtk_imgsys_dev *imgsys_dev,
							void __iomem *wpeRegBA)
{
	unsigned int i, j, count;
	unsigned int debug_value[20] = {0x0};
	unsigned int sel_value[20] = {0x0};
	unsigned int top_sel[2] = {0x0}; //[16:31]
	unsigned int mod_sel[20] = {0x0}; //[0:7]

	//cache debug 1
	top_sel[0] = 0x02;
	top_sel[1] = 0x1C;
	mod_sel[0] = 0x1C;
	for (i = 0; i <= 1; i++) { //top_sel
		count = 0;
		for (j = 2; j <= 5; j++, count++) { //dbg_sel [8:11]
			sel_value[count] = ((top_sel[i] << 12) | (j << 8) | mod_sel[0]);
			writel(sel_value[count], (wpeRegBA + WPE_REG_DBG_SET));
			debug_value[count] = (unsigned int)ioread32(
			  (void *)(wpeRegBA + WPE_REG_DBG_PORT));
		}
		dev_info(imgsys_dev->dev,
		  "%s: [0x%x]top:0x%X, dbg:2~5, mod:0x%x=0x%x..,[0x%x]0x%X 0x%X 0x%X 0x%X",
		  __func__, WPE_REG_DBG_SET, top_sel[i], mod_sel[0], sel_value[0],
		  WPE_REG_DBG_PORT,
		  debug_value[0], debug_value[1], debug_value[2], debug_value[3]);
	}

	//cache debug 2
	top_sel[0] = 0x1F;
	mod_sel[0] = 20;
	mod_sel[1] = 21;
	mod_sel[2] = 28;
	mod_sel[3] = 29;
	mod_sel[4] = 32;
	mod_sel[5] = 33;
	mod_sel[6] = 36;
	mod_sel[7] = 37;
	for (i = 2; i <= 5; i++) { //dbg_sel [8:11]
		count = 0;
		for (j = 0; j <= 7; j++, count++) { //mod_sel
			sel_value[count] = ((top_sel[0] << 12) | (i << 8) | mod_sel[j]);
			writel(sel_value[count], (wpeRegBA + WPE_REG_DBG_SET));
			debug_value[count] = (unsigned int)ioread32(
			  (void *)(wpeRegBA + WPE_REG_DBG_PORT));
		}
		dev_info(imgsys_dev->dev,
		  "%s: [0x%x]top:0x%X, dbg:0x%x, mod:20,21,28,29,32,33,36,37=0x%x..,[0x%x]0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X",
		  __func__, WPE_REG_DBG_SET, top_sel[0], i, sel_value[0], WPE_REG_DBG_PORT,
		  debug_value[0], debug_value[1], debug_value[2], debug_value[3],
		  debug_value[4], debug_value[5], debug_value[6], debug_value[7]);
	}

	//cache debug 2
	top_sel[0] = 0x1F;
	mod_sel[0] = 4;
	mod_sel[1] = 5;
	mod_sel[2] = 6;
	mod_sel[3] = 7;
	mod_sel[4] = 12;
	mod_sel[5] = 13;
	mod_sel[6] = 40;
	mod_sel[7] = 41;
	mod_sel[8] = 44;
	mod_sel[9] = 45;
	mod_sel[10] = 48;
	mod_sel[11] = 49;
	mod_sel[12] = 74;
	mod_sel[13] = 75;
	mod_sel[14] = 78;
	mod_sel[15] = 79;
	mod_sel[16] = 82;
	mod_sel[17] = 83;
	mod_sel[18] = 86;
	mod_sel[19] = 87;
	for (i = 0; i <= 1; i++) { //dbg_sel [8:11]
		count = 0;
		for (j = 0; j <= 19; j++, count++) { //mod_sel
			sel_value[count] = ((top_sel[0] << 12) | (i << 8) | mod_sel[j]);
			writel(sel_value[count], (wpeRegBA + WPE_REG_DBG_SET));
			debug_value[count] = (unsigned int)ioread32(
			  (void *)(wpeRegBA + WPE_REG_DBG_PORT));
		}
		dev_info(imgsys_dev->dev,
		  "%s: [0x%x]top:0x%X, dbg:0x%x, mod:4~7,12,13,40,41,44,45=0x%x..,[0x%x]0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X",
		  __func__, WPE_REG_DBG_SET, top_sel[0], i, sel_value[0], WPE_REG_DBG_PORT,
		  debug_value[0], debug_value[1], debug_value[2], debug_value[3],
		  debug_value[4], debug_value[5], debug_value[6], debug_value[7],
		  debug_value[8], debug_value[9]);
		dev_info(imgsys_dev->dev,
		  "%s: [0x%x]top:0x%X, dbg:0x%x, mod:48,49,74,75,78,79,82,83,86,87=0x%x..,[0x%x]0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X",
		  __func__, WPE_REG_DBG_SET, top_sel[0], i, sel_value[10], WPE_REG_DBG_PORT,
		  debug_value[10], debug_value[11], debug_value[12], debug_value[13],
		  debug_value[14], debug_value[15], debug_value[16], debug_value[17],
		  debug_value[18], debug_value[19]);
	}

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
	writel((dbg_sel_value[0] | (0x1 << 8)), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	writel((dbg_sel_value[1] | (0x1 << 8)), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	writel((dbg_sel_value[2] | (0x1 << 8)), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	dev_info(imgsys_dev->dev,
	  "%s: [0x%x]dbg_sel,[0x%x](31:16)LnCnt(15:0)PixCnt: PQDIP[0x%x]0x%x, DIP[0x%x]0x%x, TRAW[0x%x]0x%x",
	  __func__, WPE_REG_DBG_SET, WPE_REG_DBG_PORT,
	  sel_value[0], debug_value[0], sel_value[1], debug_value[1],
	  sel_value[2], debug_value[2]);

	//req/rdy status (output)
	writel((dbg_sel_value[0] | (0x0 << 8)), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	writel((dbg_sel_value[1] | (0x0 << 8)), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	writel((dbg_sel_value[2] | (0x0 << 8)), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	dev_info(imgsys_dev->dev,
	  "%s: [0x%x]dbg_sel,[0x%x]val/REQ/RDY: PQDIP[0x%x]0x%x/%d/%d, DIP[0x%x]0x%x/%d/%d, TRAW[0x%x]0x%x/%d/%d",
	  __func__, WPE_REG_DBG_SET, WPE_REG_DBG_PORT,
	  sel_value[0], debug_value[0],
	   ((debug_value[0] >> 24) & 0x1), ((debug_value[0] >> 23) & 0x1),
	  sel_value[1], debug_value[1],
	   ((debug_value[1] >> 24) & 0x1), ((debug_value[1] >> 23) & 0x1),
	  sel_value[2], debug_value[2],
	   ((debug_value[2] >> 24) & 0x1), ((debug_value[2] >> 23) & 0x1));
}

void imgsys_wpe_debug_cq_dump(struct mtk_imgsys_dev *imgsys_dev,
							void __iomem *wpeRegBA)
{
	unsigned int dbg_sel_value = 0x0;
	unsigned int debug_value[5] = {0x0};
	unsigned int sel_value[5] = {0x0};

	debug_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_CQ_THR0_CTL));
	debug_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_CQ_THR1_CTL));
	if (!debug_value[0] || !debug_value[1]) {
		dev_info(imgsys_dev->dev, "%s: No cq_thr enabled! cq0:0x%x, cq1:0x%x",
			__func__, debug_value[0], debug_value[1]);
		return;
	}

	dbg_sel_value = (0x18 << 12);//cq_p2_eng

	//line & pix cnt
	writel((dbg_sel_value | 0x0), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[0] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	writel((dbg_sel_value | 0x1), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[1] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	writel((dbg_sel_value | 0x2), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[2] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	writel((dbg_sel_value | 0x3), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[3] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[3] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	writel((dbg_sel_value | 0x4), (wpeRegBA + WPE_REG_DBG_SET));
	sel_value[4] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_SET));
	debug_value[4] = (unsigned int)ioread32((void *)(wpeRegBA + WPE_REG_DBG_PORT));

	dev_info(imgsys_dev->dev,
		"%s: [0x%x]dbg_sel,[0x%x]cq_st[0x%x]0x%x, dma_dbg[0x%x]0x%x, dma_req[0x%x]0x%x, dma_rdy[0x%x]0x%x, dma_valid[0x%x]0x%x",
		__func__, WPE_REG_DBG_SET, WPE_REG_DBG_PORT,
		sel_value[0], debug_value[0], sel_value[1], debug_value[1],
		sel_value[2], debug_value[2], sel_value[3], debug_value[3],
		sel_value[4], debug_value[4]);
}


void imgsys_wpe_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine)
{
	void __iomem *wpeRegBA = 0L;
	unsigned int i, j, ctl_en;
	unsigned int hw_idx = 0, ofst_idx;
	unsigned int wpeBase = 0;
	unsigned int startHw = REG_MAP_E_WPE_EIS, endHW = REG_MAP_E_WPE_TNR;

	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	if ((engine & IMGSYS_ENG_WPE_EIS) && !(engine & IMGSYS_ENG_WPE_TNR))
		endHW = REG_MAP_E_WPE_EIS;

	if (!(engine & IMGSYS_ENG_WPE_EIS) && (engine & IMGSYS_ENG_WPE_TNR))
		startHw = REG_MAP_E_WPE_TNR;

	if ((engine & IMGSYS_ENG_WPE_LITE))
		startHw = endHW = REG_MAP_E_WPE_LITE;


	/* iomap registers */
	for (hw_idx = startHw; hw_idx <= endHW; hw_idx++) {
		ofst_idx = hw_idx - REG_MAP_E_WPE_EIS;
		if (ofst_idx >= WPE_HW_NUM)
			continue;

		wpeBase = WPE_A_BASE + mtk_imgsys_wpe_base_ofst[ofst_idx];
		wpeRegBA = gWpeRegBA[ofst_idx];
		if (!wpeRegBA) {
			dev_info(imgsys_dev->dev, "%s: WPE_%d, RegBA = 0", __func__);
			continue;
		}
		dev_info(imgsys_dev->dev, "%s: ==== Dump WPE_%d =====",
		  __func__, ofst_idx);

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

		//UFO
		if (ctl_en & 0x400) {
			imgsys_wpe_debug_ufo_dump(imgsys_dev, wpeRegBA);
			imgsys_wpe_debug_ufo_dump(imgsys_dev, wpeRegBA); //twice
			imgsys_wpe_debug_cache_dump(imgsys_dev, wpeRegBA);
		}

	}
	//
	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_wpe_debug_dump);

void imgsys_wpe_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i;

	pr_debug("%s: +\n", __func__);

	for (i = 0; i < WPE_HW_NUM; i++) {
		iounmap(gWpeRegBA[i]);
		gWpeRegBA[i] = 0L;
	}

	pr_debug("%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_wpe_uninit);

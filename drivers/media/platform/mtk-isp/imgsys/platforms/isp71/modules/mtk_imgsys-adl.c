// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: ChenHung Yang <chenhung.yang@mediatek.com>
 *
 */

 // Standard C header file

// kernel header file
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>

// mtk imgsys local header file

// Local header file
#include "mtk_imgsys-adl.h"

static void __iomem *g_adlARegBase;
static void __iomem *g_adlBRegBase;

const struct mtk_imgsys_init_array
			adl_init_info[] = {
	{ 0x20, 0x80000000 }  // Enable write clear
};

#define ADL_INIT_VALUE_COUNT  (sizeof(adl_init_info) / \
	sizeof(struct mtk_imgsys_init_array))
#define ADL_HARDWARE_COUNT    (2)

static void imgsys_adl_hw_reset(struct mtk_imgsys_dev *imgsys_dev)
{
	//int32_t count;

	/* IPUDMA part soft reset flow
	 * 1.	Assert sw_rst_trig=1
	 * 2.	Wait sw_rst_stat == 1
	 * 3.	Assert csr_rdma_0_hw_rst=1 (rdma hw rst)
	 * 4.	Deassert sw_rst_trig=0 and hw_rst=0
	 * 5.	Start to normal setting
	 */

	/* Reset ADL_A */
	//uint32_t value = ioread32((void *)(g_adlARegBase));
	//value |= ((0x1 << 8) | (0x1 << 9));
	//iowrite32(value, g_adlARegBase);

	//count = 0;
	//while(count < 1000000) {
	//	value = ioread32((void *)(g_adlARegBase));
	//	if ((value & 0x3) == 0x3) {
	//		break;
	//	}
	//	count++;
	//}

	//value = ioread32((void *)(g_adlARegBase));
	//value &= ~((0x1 << 8) | (0x1 << 9));
	//iowrite32(value, g_adlARegBase);

	/* Reset ADL_B */
	//value = ioread32((void *)(g_adlBRegBase));
	//value |= ((0x1 << 8) | (0x1 << 9));
	//iowrite32(value, g_adlARegBase);

	//count = 0;
	//while(count < 1000000) {
	//	value = ioread32((void *)(g_adlARegBase));
	//	if ((value & 0x3) == 0x3) {
	//		break;
	//	}
	//	count++;
	//}

	//value = ioread32((void *)(g_adlARegBase));
	//value &= ~((0x1 << 8) | (0x1 << 9));
	//iowrite32(value, g_adlARegBase);
}

void imgsys_adl_init(struct mtk_imgsys_dev *imgsys_dev)
{
	//void __iomem *adlBase = 0L;
	//void __iomem *offset = NULL;
	//unsigned int i = 0, HwIdx = 0;

	//g_adlARegBase = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A);  // 0x15005300
	//g_adlBRegBase = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_B);  // 0x15007300

	imgsys_adl_hw_reset(imgsys_dev);

	//for (HwIdx = 0; HwIdx < ADL_HARDWARE_COUNT; HwIdx++) {
	//	if (HwIdx == 0) {
	//		adlBase = g_adlARegBase;
	//	} else {
	//		adlBase = g_adlBRegBase;
	//	}

	//	if (!adlBase) {
	//		pr_info("%s: hw(%d)null reg base\n", __func__, HwIdx);
	//		break;
	//	}

	//	for (i = 0 ; i < ADL_INIT_VALUE_COUNT ; i++) {
	//		offset = adlBase + adl_init_info[i].ofset;
	//		writel(adl_init_info[i].val, offset);
	//	}
	//}

	pr_info("%s\n", __func__);
}

static uint32_t dump_debug_data(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t eng_base, void __iomem *reg_base, uint32_t debug_cmd)
{
	void __iomem *sel_reg  = (void *)(reg_base + ADL_REG_DBG_SEL);
	void __iomem *data_reg = (void *)(reg_base + ADL_REG_DBG_PORT);
	uint32_t value;

	iowrite32(debug_cmd, sel_reg);
	value = (unsigned int)ioread32(data_reg);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		debug_cmd, eng_base + ADL_REG_DBG_PORT, value);

	return value;
}

static void dump_dma_debug_data(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t eng_base, void __iomem *reg_base)
{
	uint32_t debug_cmd;

	/* ?? debug out port: IMGADL_ADL_DMA_0_DEBUG */

	/* checksum */
	debug_cmd = (0x00 << 8) | (0x1 << 0);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* line_pix_cnt_tmp */
	debug_cmd = (0x00 << 8) | (0x2 << 0);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* line_pix_cnt */
	debug_cmd = (0x00 << 8) | (0x3 << 0);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* smi debug data */
	debug_cmd = (0x00 << 8) | (0x5 << 0);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);
}

static void dump_cq_debug_data(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t eng_base, void __iomem *reg_base)
{

	/* ?? debug out port: IMGADL_ADL_CQ_DEBUG */

	uint32_t debug_cmd;
	//void __iomem *pCQEn = (void *)(reg_base + TRAW_DIPCQ_CQ_EN);

	/* arx/atx/drx/dtx_state */
	debug_cmd = (0x02 << 8);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);
	/* Thr(0~3)_state */
	debug_cmd = (0x02 << 8) | (0x01 << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* Set DIPCQ_CQ_EN[28] to 1 ??*/
	//iowrite32(0x10000000, pCQEn);

	/* cqd0_checksum0 */
	debug_cmd = (0x02 << 8);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqd0_checksum1 */
	debug_cmd = (0x02 << 8) | (0x01 << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqd0_checksum2 */
	debug_cmd = (0x02 << 8) | (0x02 << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqd1_checksum0 */
	debug_cmd = (0x02 << 8) | (0x04 << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqd1_checksum1 */
	debug_cmd = (0x02 << 8) | (0x05 << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqd1_checksum2 */
	debug_cmd = (0x02 << 8) | (0x06 << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqa0_checksum0 */
	debug_cmd = (0x02 << 8) | (0x08 << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqa0_checksum1 */
	debug_cmd = (0x02 << 8) | (0x09 << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqa0_checksum2 */
	debug_cmd = (0x02 << 8) | (0x0A << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqa1_checksum0 */
	debug_cmd = (0x02 << 8) | (0x0C << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqa1_checksum1 */
	debug_cmd = (0x02 << 8) | (0x0D << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* cqa1_checksum2 */
	debug_cmd = (0x02 << 8) | (0x0E << 4);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);
}

static void dump_cq_rdma_status(struct mtk_imgsys_dev *imgsys_dev,
		uint32_t eng_base, void __iomem *reg_base)
{
	/* ?? debug out port: IMGADL_ADL_CQ_DEBUG */

	uint32_t debug_cmd;

	/* 0x3: cq_rdma_req_st     */
	debug_cmd = (0x3 << 8);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* 0x4: cq_rdma_rdy_st     */
	debug_cmd = (0x4 << 8);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);

	/* 0x5: cq_rdma_valid      */
	debug_cmd = (0x5 << 8);
	dump_debug_data(imgsys_dev, eng_base, reg_base, debug_cmd);
}

void imgsys_adl_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t engine)
{
	if (engine & IMGSYS_ENG_ADL_A) {
		/* 0x0: adl_dma_debug_data */
		dump_dma_debug_data(imgsys_dev, REG_MAP_E_ADL_A, g_adlARegBase);

		/* 0x1: cq_dma_debug_data */
		dump_debug_data(imgsys_dev, REG_MAP_E_ADL_A, g_adlARegBase, (0x1 << 8));

		/* 0x2: cq_debug_data */
		dump_cq_debug_data(imgsys_dev, REG_MAP_E_ADL_A, g_adlARegBase);

		/* 0x3 ~ 0x5 cq rdma status */
		dump_cq_rdma_status(imgsys_dev, REG_MAP_E_ADL_A, g_adlARegBase);
	} else if (engine & IMGSYS_ENG_ADL_B) {
		/* 0x0: adl_dma_debug_data */
		dump_dma_debug_data(imgsys_dev, REG_MAP_E_ADL_B, g_adlBRegBase);

		/* 0x1: cq_dma_debug_data  */
		dump_debug_data(imgsys_dev, REG_MAP_E_ADL_B, g_adlBRegBase, (0x1 << 8));

		/* 0x2: cq_debug_data */
		dump_cq_debug_data(imgsys_dev, REG_MAP_E_ADL_B, g_adlBRegBase);

		/* 0x3 ~ 0x5 cq rdma status */
		dump_cq_rdma_status(imgsys_dev, REG_MAP_E_ADL_B, g_adlBRegBase);
	}
}

void imgsys_adl_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	//if (g_adlARegBase) {
	//	iounmap(g_adlARegBase);
	//	g_adlARegBase = 0L;
	//}

	//if (g_adlBRegBase) {
	//	iounmap(g_adlBRegBase);
	//	g_adlBRegBase = 0L;
	//}

	pr_info("%s\n", __func__);
}

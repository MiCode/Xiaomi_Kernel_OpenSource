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

static void __iomem *g_adl_a_va;
static void __iomem *g_adl_b_va;

const struct mtk_imgsys_init_array
	adl_init_info[] = {
	{ 0x20, 0x80000000 }	// Enable write clear
};

#define ADL_INIT_VALUE_COUNT  (sizeof(adl_init_info) / \
	sizeof(struct mtk_imgsys_init_array))
#define ADL_HARDWARE_COUNT    (2)

static void reset_adl_hardware(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t reg_base_pa, void __iomem *reg_base_va)
{
	int32_t count;

	/* IPUDMA part soft reset flow
	 * 1.	Assert sw_rst_trig=1
	 * 2.	Wait sw_rst_stat == 1
	 * 3.	Assert csr_rdma_0_hw_rst=1 (rdma hw rst)
	 * 4.	Deassert sw_rst_trig=0 and hw_rst=0
	 * 5.	Start to normal setting
	 */

	/* Reset ADL_A */
	uint32_t value = ioread32((void *)(reg_base_va));

	value |= ((0x1 << 8) | (0x1 << 9));
	iowrite32(value, reg_base_va);

	count = 0;
	while (count < 1000) {
		value = ioread32((void *)(reg_base_va));
		if ((value & 0x3) == 0x3)
			break;
		count++;
	}

	value = ioread32((void *)(reg_base_va));
	value &= ~((0x1 << 8) | (0x1 << 9));
	iowrite32(value, reg_base_va);
}

static void init_adl_hardware(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t reg_base_pa, void __iomem *reg_base_va)
{
	int32_t index;
	void __iomem *offsetVA;

	for (index = 0; index < ADL_INIT_VALUE_COUNT ; index++) {
		offsetVA = reg_base_va + adl_init_info[index].ofset;
		writel(adl_init_info[index].val, offsetVA);
	}
}

static void dump_adl_register(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t reg_base_pa, void __iomem *reg_base_va, uint32_t size)
{
	int32_t index;

	for (index = 0; index <= size; index += 0x10) {
		dev_info(imgsys_dev->dev,
			"%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X", __func__,
			(uint32_t)(reg_base_pa + index),
			(uint32_t)ioread32((void *)(reg_base_va + index)),
			(uint32_t)ioread32((void *)(reg_base_va + index + 0x4)),
			(uint32_t)ioread32((void *)(reg_base_va + index + 0x8)),
			(uint32_t)ioread32((void *)(reg_base_va + index + 0xC)));
	}
}

void imgsys_adl_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	uint32_t reg_base_pa;
	void __iomem *reg_base_va;
	int32_t index;

	pr_info("%s: +\n", __func__);

	// ADL_A: 0x15005300
	g_adl_a_va = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A);

	// ADL_B: 0x15007300
	g_adl_b_va = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_B);

	for (index = 0; index < ADL_HARDWARE_COUNT; index++) {
		if (index == 0) {
			reg_base_pa = ADL_A_REG_BASE;
			reg_base_va = g_adl_a_va;
		} else {
			reg_base_pa = ADL_B_REG_BASE;
			reg_base_va = g_adl_b_va;
		}

		if (reg_base_va) {
			pr_info("%s: hw(%d) null reg base\n", __func__, index);
			break;
		}

	  reset_adl_hardware(imgsys_dev, reg_base_pa, reg_base_va);

	  init_adl_hardware(imgsys_dev, reg_base_pa, reg_base_va);
	}

	pr_info("%s: -\n", __func__);
}

static uint32_t dump_debug_data(struct mtk_imgsys_dev *imgsys_dev,
	void __iomem *sel_reg_va, uint32_t debug_cmd, uint32_t data_reg_pa,
	void __iomem *data_reg_va)
{
	uint32_t value;

	iowrite32(debug_cmd, sel_reg_va);
	value = (uint32_t)ioread32(data_reg_va);
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)\n", __func__,
		debug_cmd, data_reg_pa, value);

	return value;
}

static void dump_dma_debug_data(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t reg_base_pa, void __iomem *reg_base_va)
{
	void __iomem *sel_reg_va  = (void *)(reg_base_va + ADL_REG_DBG_SEL);
	void __iomem *data_reg_va = (void *)(reg_base_va + ADL_REG_DMA_DBG);
	uint32_t data_reg_pa = reg_base_pa + ADL_REG_DMA_DBG;
	uint32_t debug_cmd;

	/* checksum */
	debug_cmd = (0x00 << 8) | (0x1 << 0);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* line_pix_cnt_tmp */
	debug_cmd = (0x00 << 8) | (0x2 << 0);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* line_pix_cnt */
	debug_cmd = (0x00 << 8) | (0x3 << 0);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* smi debug data */
	debug_cmd = (0x00 << 8) | (0x5 << 0);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);
}

static void dump_cq_debug_data(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t reg_base_pa, void __iomem *reg_base_va)
{
	void __iomem *sel_reg_va  = (void *)(reg_base_va + ADL_REG_DBG_SEL);
	void __iomem *data_reg_va = (void *)(reg_base_va + ADL_REG_CQ_DBG);
	uint32_t data_reg_pa = reg_base_pa + ADL_REG_CQ_DBG;
	uint32_t debug_cmd;

	/* cqd0_checksum0 */
	debug_cmd = (0x02 << 8) | (0x00 << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqd0_checksum1 */
	debug_cmd = (0x02 << 8) | (0x01 << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqd0_checksum2 */
	debug_cmd = (0x02 << 8) | (0x02 << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqd1_checksum0 */
	debug_cmd = (0x02 << 8) | (0x04 << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqd1_checksum1 */
	debug_cmd = (0x02 << 8) | (0x05 << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqd1_checksum2 */
	debug_cmd = (0x02 << 8) | (0x06 << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqa0_checksum0 */
	debug_cmd = (0x02 << 8) | (0x08 << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqa0_checksum1 */
	debug_cmd = (0x02 << 8) | (0x09 << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqa0_checksum2 */
	debug_cmd = (0x02 << 8) | (0x0A << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqa1_checksum0 */
	debug_cmd = (0x02 << 8) | (0x0C << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqa1_checksum1 */
	debug_cmd = (0x02 << 8) | (0x0D << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* cqa1_checksum2 */
	debug_cmd = (0x02 << 8) | (0x0E << 4);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);
}

static void dump_cq_rdma_status(struct mtk_imgsys_dev *imgsys_dev,
		uint32_t reg_base_pa, void __iomem *reg_base_va)
{
	void __iomem *sel_reg_va  = (void *)(reg_base_va + ADL_REG_DBG_SEL);
	void __iomem *data_reg_va = (void *)(reg_base_va + ADL_REG_CQ_DMA);
	uint32_t data_reg_pa = reg_base_pa + ADL_REG_CQ_DMA;
	uint32_t debug_cmd;

	/* 0x3: cq_rdma_req_st     */
	debug_cmd = (0x3 << 8);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* 0x4: cq_rdma_rdy_st     */
	debug_cmd = (0x4 << 8);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);

	/* 0x5: cq_rdma_valid      */
	debug_cmd = (0x5 << 8);
	dump_debug_data(imgsys_dev, sel_reg_va, debug_cmd, data_reg_pa, data_reg_va);
}

void imgsys_adl_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t engine)
{
	if (engine & IMGSYS_ENG_ADL_A) {
		/* 0x0: adl_dma_debug_data */
		dump_dma_debug_data(imgsys_dev, ADL_A_REG_BASE, g_adl_a_va);

		/* 0x1: cq_dma_debug_data */
		// dump_debug_data(imgsys_dev, ADL_A_REG_BASE, g_adl_a_va, (0x1 << 8));

		/* 0x2: cq_debug_data */
		dump_cq_debug_data(imgsys_dev, ADL_A_REG_BASE, g_adl_a_va);

		/* 0x3 ~ 0x5 cq rdma status */
		dump_cq_rdma_status(imgsys_dev, ADL_A_REG_BASE, g_adl_a_va);

		/* dump adl register map */
		dump_adl_register(imgsys_dev, ADL_A_REG_BASE, g_adl_a_va, 0x1000);
	}

	if (engine & IMGSYS_ENG_ADL_B) {
		/* 0x0: adl_dma_debug_data */
		dump_dma_debug_data(imgsys_dev, ADL_B_REG_BASE, g_adl_b_va);

		/* 0x1: cq_dma_debug_data  */
		// dump_debug_data(imgsys_dev, ADL_B_REG_BASE, g_adl_b_va, (0x1 << 8));

		/* 0x2: cq_debug_data */
		dump_cq_debug_data(imgsys_dev, ADL_B_REG_BASE, g_adl_b_va);

		/* 0x3 ~ 0x5 cq rdma status */
		dump_cq_rdma_status(imgsys_dev, ADL_B_REG_BASE, g_adl_b_va);

		/* dump adl register map */
		dump_adl_register(imgsys_dev, ADL_B_REG_BASE, g_adl_b_va, 0x1000);
	}
}

void imgsys_adl_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	pr_info("%s+\n", __func__);

	if (g_adl_a_va) {
		iounmap(g_adl_a_va);
		g_adl_a_va = 0L;
	}

	if (g_adl_b_va) {
		iounmap(g_adl_b_va);
		g_adl_b_va = 0L;
	}

	pr_info("%s-\n", __func__);
}

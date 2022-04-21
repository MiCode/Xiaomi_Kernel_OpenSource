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
	{0x0A8, 0x80000000},	/* DIPCTL_D1A_DIPCTL_INT2_EN */
};

#define DIP_HW_SET 3

#define	DIP_INIT_ARRAY_COUNT	1

void __iomem *gdipRegBA[DIP_HW_SET] = {0L};

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int hw_idx = 0, ary_idx = 0;


	for (hw_idx = REG_MAP_E_DIP; hw_idx <= REG_MAP_E_DIP_NR2; hw_idx++) {
		/* iomap registers */
		ary_idx = hw_idx - REG_MAP_E_DIP;
		gdipRegBA[ary_idx] = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		if (!gdipRegBA[ary_idx]) {
			pr_info("%s:unable to iomap dip_%d reg, devnode(%s)\n",
				__func__, hw_idx);
			continue;
		}
	}


}

void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *dipRegBA = 0L;
	void __iomem *ofset = NULL;
	unsigned int i;


	/* iomap registers */
	dipRegBA = gdipRegBA[0];

	for (i = 0 ; i < DIP_INIT_ARRAY_COUNT; i++) {
		ofset = dipRegBA + mtk_imgsys_dip_init_ary[i].ofset;
		writel(mtk_imgsys_dip_init_ary[i].val, ofset);
	}

}

void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine)
{
	void __iomem *dipRegBA = 0L;
	unsigned int i;
	unsigned int DbgCmd = 0;
	void __iomem *DbgSel = 0L;
	void __iomem *DbgPort = 0L;
	unsigned int DbgData = 0;
	unsigned int DbgOutReg = 0;

	pr_info("%s: +\n", __func__);

	/* 0x15100000~ */
	dipRegBA = gdipRegBA[0];
	/* ctrl reg */
	for (i = TOP_CTL_OFT; i <= (TOP_CTL_OFT + TOP_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* DMA reg */
	for (i = DMATOP_OFT; i <= (DMATOP_OFT + DMATOP_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* NR3D */
	for (i = NR3D_CTL_OFT; i <= (NR3D_CTL_OFT + NR3D_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* SNRS */
	for (i = SNRS_CTL_OFT; i <= (SNRS_CTL_OFT + SNRS_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* UNP_D1~C20_D1 */
	for (i = UNP_D1_CTL_OFT; i <= (UNP_D1_CTL_OFT + UNP_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* SMT_D1~PAK_D2 */
	for (i = SMT_D1_CTL_OFT; i <= (SMT_D1_CTL_OFT + SMT_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	/* 0x15154000~ */
	dipRegBA = gdipRegBA[1];
	/* SNR_D1~PCRP_D16*/
	for (i = SNR_D1_CTL_OFT; i <= (SNR_D1_CTL_OFT + SNR_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* EE_D1~URZS2T_D5*/
	for (i = EE_D1_CTL_OFT; i <= (EE_D1_CTL_OFT + EE_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* TNC_BCE */
	for (i = TNC_BCE_CTL_OFT; i <= (TNC_BCE_CTL_OFT + TNC_BCE_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* TNC_BCE */
	for (i = TNC_BCE_CTL_OFT; i <= (TNC_BCE_CTL_OFT + TNC_BCE_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* TNC_TILE */
	for (i = TNC_TILE_CTL_OFT; i <= (TNC_TILE_CTL_OFT + TNC_TILE_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* TNC_C2G~TNC_TNC_CTL */
	for (i = TNC_C2G_CTL_OFT; i <= (TNC_C2G_CTL_OFT + TNC_C2G_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* TNC_C3D */
	for (i = TNC_C3D_CTL_OFT; i <= (TNC_C3D_CTL_OFT + TNC_C3D_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	/* 0x15161000~ */
	dipRegBA = gdipRegBA[2];
	/* VIPI_D1~SMTCI_D9 */
	for (i = VIPI_D1_CTL_OFT; i <= (VIPI_D1_CTL_OFT + VIPI_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR2_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* SNRCSI_D1~SMTO_D9 */
	for (i = SNRCSI_D1_CTL_OFT; i <= (SNRCSI_D1_CTL_OFT + SNRCSI_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR2_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* SMTCO_D4~DRZS8T_D1 */
	for (i = SMTCO_D4_CTL_OFT; i <= (SMTCO_D4_CTL_OFT + SMTCO_D4_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR2_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* DRZH2N_D2 */
	for (i = DRZH2N_D2_CTL_OFT; i <= (DRZH2N_D2_CTL_OFT + DRZH2N_D2_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR2_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}


	/* 0x15100000~ */
	dipRegBA = gdipRegBA[0];

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x18 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x1*/
	DbgPort = (void *)(dipRegBA + DIPCTL_DBG_OUT);
	DbgSel = (void *)(dipRegBA + DIPCTL_DBG_SEL);
	DbgOutReg = DIP_TOP_ADDR + DIPCTL_DBG_OUT;

	DbgCmd = 0x1801;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x0 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x0~0xD */
	DbgCmd = 0x13;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0x20001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0x10001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0x30001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0x40001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0x50001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0x60001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0x70001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0x80001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0x90001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0xA0001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0xB0001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0xC0001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);
	DbgCmd = 0xD0001;
	iowrite32(DbgCmd, (void *)DbgSel);
	DbgData = (unsigned int)ioread32((void *)DbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		DbgCmd, DbgOutReg, DbgData);

	pr_info("%s: -\n", __func__);

}

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i;

	for (i = 0; i < DIP_HW_SET; i++) {
		iounmap(gdipRegBA[i]);
		gdipRegBA[i] = 0L;
	}
}

/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __SOC_MEDIATEK_INFRACFG_H
#define __SOC_MEDIATEK_INFRACFG_H

#define MT8173_TOP_AXI_PROT_EN_MCI_M2		BIT(0)
#define MT8173_TOP_AXI_PROT_EN_MM_M0		BIT(1)
#define MT8173_TOP_AXI_PROT_EN_MM_M1		BIT(2)
#define MT8173_TOP_AXI_PROT_EN_MMAPB_S		BIT(6)
#define MT8173_TOP_AXI_PROT_EN_L2C_M2		BIT(9)
#define MT8173_TOP_AXI_PROT_EN_L2SS_SMI		BIT(11)
#define MT8173_TOP_AXI_PROT_EN_L2SS_ADD		BIT(12)
#define MT8173_TOP_AXI_PROT_EN_CCI_M2		BIT(13)
#define MT8173_TOP_AXI_PROT_EN_MFG_S		BIT(14)
#define MT8173_TOP_AXI_PROT_EN_PERI_M0		BIT(15)
#define MT8173_TOP_AXI_PROT_EN_PERI_M1		BIT(16)
#define MT8173_TOP_AXI_PROT_EN_DEBUGSYS		BIT(17)
#define MT8173_TOP_AXI_PROT_EN_CQ_DMA		BIT(18)
#define MT8173_TOP_AXI_PROT_EN_GCPU		BIT(19)
#define MT8173_TOP_AXI_PROT_EN_IOMMU		BIT(20)
#define MT8173_TOP_AXI_PROT_EN_MFG_M0		BIT(21)
#define MT8173_TOP_AXI_PROT_EN_MFG_M1		BIT(22)
#define MT8173_TOP_AXI_PROT_EN_MFG_SNOOP_OUT	BIT(23)
#define MT8173_TOP_AXI_PROT_EN_CA7_ADB		BIT(29)
#define MT8173_TOP_AXI_PROT_EN_CA15_ADB		BIT(30)

#define MT8173_TOP_AXI_PROT_EN1_L2C_SRAM	BIT(2)

#define MT8163_TOP_AXI_PROT_EN_CCI_M1		BIT(0)
#define MT8163_TOP_AXI_PROT_EN_MM_M0		BIT(1)
#define MT8163_TOP_AXI_PROT_EN_MMAPB_S		BIT(6)
#define MT8163_TOP_AXI_PROT_EN_L2C_M2		BIT(9)
#define MT8163_TOP_AXI_PROT_EN_L2SS_SMI		BIT(11)
#define MT8163_TOP_AXI_PROT_EN_L2SS_AFF		BIT(12)
#define MT8163_TOP_AXI_PROT_EN_CCI_M2		BIT(13)
#define MT8163_TOP_AXI_PROT_EN_CONN2EMI		BIT(14)
#define MT8163_TOP_AXI_PROT_EN_CONN2PERI	BIT(15)
#define MT8163_TOP_AXI_PROT_EN_DEBUGSYS		BIT(17)
#define MT8163_TOP_AXI_PROT_EN_CQ_DMA		BIT(18)
#define MT8163_TOP_AXI_PROT_EN_GCPU		BIT(19)
#define MT8163_TOP_AXI_PROT_EN_IOMMU		BIT(20)
#define MT8163_TOP_AXI_PROT_EN_MFG_M0		BIT(21)
#define MT8163_TOP_AXI_PROT_EN_MFG_M1		BIT(22)
#define MT8163_TOP_AXI_PROT_EN_MFG_SNOOP_OUT	BIT(23)

int mtk_infracfg_set_bus_protection(struct regmap *infracfg, u32 mask);
int mtk_infracfg_clear_bus_protection(struct regmap *infracfg, u32 mask);
int mtk_infracfg_set_bus_protection1(struct regmap *infracfg, u32 mask);
int mtk_infracfg_clear_bus_protection1(struct regmap *infracfg, u32 mask);

#endif /* __SOC_MEDIATEK_INFRACFG_H */

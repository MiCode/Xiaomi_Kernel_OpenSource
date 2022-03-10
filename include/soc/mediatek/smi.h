/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef MTK_IOMMU_SMI_H
#define MTK_IOMMU_SMI_H

#include <linux/bitops.h>
#include <linux/device.h>

#ifdef CONFIG_MTK_SMI

#define MTK_LARB_NR_MAX		32

#define MTK_SMI_MMU_EN(port)	BIT(port)

#define RESET_CELL_NUM			(2)
#define MAX_LARB_FOR_CLAMP		(6)
#define MAX_COMMON_FOR_CLAMP	(3)

struct mtk_smi_larb_iommu {
	struct device *dev;
	unsigned int   mmu;
};

struct mtk_smi_iommu {
	unsigned int larb_nr;
	struct mtk_smi_larb_iommu larb_imu[MTK_LARB_NR_MAX];
};

#endif
#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
#include <linux/platform_device.h>

struct mtk_smi_pair {
	u32 off;
	u32 val;
};

struct mtk_smi_dev {
	u32 id;
	struct device *dev;
	void __iomem *base;
	u32 *mmu;

	u32 nr_clks;
	struct clk **clks;
	atomic_t clk_cnts;

	u32 nr_conf_pairs;
	struct mtk_smi_pair *conf_pairs;

	u32 nr_scen_pairs;
	struct mtk_smi_pair **scen_pairs;

	u32	power_reset_pa[MAX_LARB_FOR_CLAMP];
	void __iomem *power_reset_reg[MAX_LARB_FOR_CLAMP];
	u32	power_reset_value[MAX_LARB_FOR_CLAMP];

	u32	comm_reset_pa[MAX_LARB_FOR_CLAMP];
	void __iomem *comm_reset_reg[MAX_LARB_FOR_CLAMP];
	u32	comm_reset_value[MAX_LARB_FOR_CLAMP];
	bool comm_reset;

	bool comm_clamp_enable;
	u32	comm_clamp_value[MAX_LARB_FOR_CLAMP];
};

s32 mtk_smi_clk_enable(struct mtk_smi_dev *smi);
void mtk_smi_clk_disable(struct mtk_smi_dev *smi);

struct mtk_smi_dev *mtk_smi_dev_get(const u32 id);
s32 mtk_smi_conf_set(const struct mtk_smi_dev *smi, const u32 scen_id);

s32 smi_register(void);
s32 smi_get_dev_num(void);
s32 smi_larb_port_check(void);
#endif

#endif

/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Honghui Zhang <honghui.zhang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_IOMMU_H_
#define _MTK_IOMMU_H_

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <soc/mediatek/smi.h>

#include "io-pgtable.h"

struct mtk_iommu_suspend_reg {
	u32				standard_axi_mode;
	u32				dcm_dis;
	u32				ctrl_reg;
	u32				int_control0;
	u32				int_main_control;
	u32				ivrp_paddr;
	u32				vld_pa_range;
	u32				pt_base;
	u32				wr_ctrl;
	u32				dummy;
};

enum mtk_iommu_plat {
	M4U_MT2701,
	M4U_MT2712,
	M4U_MT8167,
	M4U_MT8168,
	M4U_MT8173,
	M4U_MT8183,
	iommu_mt6xxx_v0,
};

struct mtk_iommu_resv_iova_region;
struct mtk_iommu_plat_data {
	enum mtk_iommu_plat m4u_plat;
	bool has_4gb_mode;
	int iommu_cnt;
	/* The larb-id may be remapped in the smi-common. */
	bool larbid_remap_enable;
	unsigned int larbid_in_common[MTK_LARB_NR_MAX];

	/* reserve/dir-mapping iova region data */
	const char spec_device_comp[32];
	const unsigned int spec_cnt;
	const struct mtk_iommu_resv_iova_region *spec_region;
};

struct mtk_iommu_domain;

#ifdef CONFIG_MTK_IOMMU_V2
struct mtk_iommu_pgtable {
	spinlock_t			pgtlock; /* lock for page table */
	struct io_pgtable_cfg		cfg;
	struct io_pgtable_ops		*iop;
	struct list_head		m4u_dom;
	spinlock_t	domain_lock; /* lock for page table */
	unsigned int domain_count;
	unsigned int init_domain_id;
};

struct mtk_iommu_domain {
	unsigned int		id;
	struct iommu_domain		domain;
	struct iommu_group		*group;
#ifndef CONFIG_ARM64
	struct dma_iommu_mapping *mapping;
	unsigned int		resv_status;
#endif
	struct mtk_iommu_pgtable	*pgtable;
	struct mtk_iommu_data *data;
	struct list_head list;
};

#define IOMMU_CLK_ID_COUNT (2)
struct mtk_iommu_clks {
	unsigned int	nr_clks;
	struct clk *clks[IOMMU_CLK_ID_COUNT];
};
#endif

struct mtk_iommu_data {
	void __iomem *base;
	int irq;
	void __iomem *base_sec;
	int irq_sec;
	struct device *dev;
	struct clk *bclk;
	phys_addr_t protect_base; /* protect memory base */
	struct mtk_iommu_suspend_reg reg;
#ifdef CONFIG_MTK_IOMMU_V2
	struct mtk_iommu_pgtable	*pgtable;
	struct mtk_iommu_clks		*m4u_clks;
	unsigned int		power_id;
#else
	struct mtk_iommu_domain	*m4u_dom;
	struct iommu_group *m4u_group;
#endif
	struct mtk_smi_iommu smi_imu; /* SMI larb iommu info */
	bool enable_4GB;   /* Dram is over 4gb */
	bool tlb_flush_active;

	struct iommu_device iommu;
	const struct mtk_iommu_plat_data *plat_data;

	struct list_head list;
	unsigned int m4uid;
};

static inline int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static inline void release_of(struct device *dev, void *data)
{
	of_node_put(data);
}

static inline int mtk_iommu_bind(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);

	return component_bind_all(dev, &data->smi_imu);
}

static inline void mtk_iommu_unbind(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);

	component_unbind_all(dev, &data->smi_imu);
}

#endif

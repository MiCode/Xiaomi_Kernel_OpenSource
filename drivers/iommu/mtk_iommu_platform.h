/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author:	YongWu <yong.wu@mediatek.com>
 *		Honghui Zhang <honghui.zhang@mediatek.com>
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
#ifndef MTK_IOMMU_PLATFORM_H
#define MTK_IOMMU_PLATFORM_H

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>

enum {
	M4U_ID_0 = 0,
	M4U_ID_1,
	M4U_ID_ALL
};

#define MTK_IOMMU_PGT_SZ	SZ_1M		/*2M err*/
#define MTK_PROTECT_PA_ALIGN	128
#define MTK_IOMMU_LARB_MAX_NR	8
#define MTK_IOMMU_PORT_MAX_NR	100
#define MT2701_IOMMU_PAGE_SIZE	0x1000

struct mtk_iommu_info {
	void __iomem			*base;
	void __iomem			*l2_base;
	int				irq;
	int				larb_nr;
	unsigned int			iova_base;
	size_t				iova_size;
	dma_addr_t			pgt_basepa;
	struct device			*dev;
	struct clk			*bclk;
	dma_addr_t			protect_base;
	unsigned int			*protect_va;
	const struct mtk_iommu_cfg	*imucfg;
};

struct mtk_iommu_domain {
	unsigned int		*pgtableva;
	dma_addr_t		pgtablepa;
	spinlock_t		pgtlock;
	spinlock_t		portlock;
	struct mtk_iommu_info	*piommuinfo;
	struct iommu_domain	*domain;
};

/*hw config*/
struct mtk_iommu_cfg {
	unsigned int l2_offset;
	unsigned int m4u_port_in_larbx[MTK_IOMMU_LARB_MAX_NR];
	unsigned int m4u_port_nr;

	/*hw function*/
	int (*dt_parse)(struct platform_device *pdev,
			struct mtk_iommu_info *piommu);
	int (*hw_init)(const struct mtk_iommu_info *piommu);
	void (*hw_deinit)(const struct mtk_iommu_info *piommu);
	int (*map)(struct mtk_iommu_domain *mtkdomain, unsigned int iova,
		   phys_addr_t paddr, size_t size);
	size_t (*unmap)(struct mtk_iommu_domain *mtkdomain, unsigned int iova,
			size_t size);
	phys_addr_t (*iova_to_phys)(struct mtk_iommu_domain *mtkdomain,
				    unsigned int iova);
	int (*config_port)(struct mtk_iommu_info *piommu,
			   int portid, bool enable);
	irq_handler_t iommu_isr;

	void (*invalid_tlb)(const struct mtk_iommu_info *piommu,
			    unsigned int m4u_id, int isinvall,
			    unsigned int iova_start,
			    unsigned int iova_end);

	void (*clear_intr)(void __iomem *base);
};

extern const struct mtk_iommu_cfg mtk_iommu_mt2701_cfg;

#endif

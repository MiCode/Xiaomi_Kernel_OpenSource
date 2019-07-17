/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef M4U_DEBUG_H
#define M4U_DEBUG_H

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/of.h>

typedef int (*mtk_iommu_fault_callback_t)(int port,
				dma_addr_t mva, void *cb_data);

bool report_custom_iommu_fault(
	u32 fault_iova,
	u32 fault_pa,
	u32 fault_id, bool is_vpu);

int mtk_iommu_register_fault_callback(int port,
			       mtk_iommu_fault_callback_t fn,
			       void *cb_data);

/* port: comes from "include/dt-binding/memort/mtxxx-larb-port.h" */
int mtk_iommu_unregister_fault_callback(int port);

void mtk_iova_dbg_alloc(struct device *dev, dma_addr_t iova, size_t size);

void mtk_iova_dbg_free(dma_addr_t iova, size_t size);

void mtk_iova_dbg_dump(struct seq_file *s);

#endif

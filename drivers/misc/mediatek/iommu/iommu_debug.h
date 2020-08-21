/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt)    "[iommu_debug] " fmt

#ifndef IOMMU_DEBUG_H
#define IOMMU_DEBUG_H

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/of.h>

typedef int (*mtk_iommu_fault_callback_t)(int port,
				dma_addr_t mva, void *cb_data);

void report_custom_iommu_fault(
	u64 fault_iova,
	u64 fault_pa,
	u32 fault_id, bool is_vpu);

int mtk_iommu_register_fault_callback(int port,
			       mtk_iommu_fault_callback_t fn,
			       void *cb_data, bool is_vpu);

/* port: comes from "include/dt-binding/memort/mtxxx-larb-port.h" */
int mtk_iommu_unregister_fault_callback(int port, bool is_vpu);

#endif

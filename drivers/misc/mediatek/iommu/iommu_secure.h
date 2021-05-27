/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#ifndef IOMMU_SECURE_H
#define IOMMU_SECURE_H

int mtk_iommu_sec_bk_init(uint32_t type, uint32_t id);
int mtk_iommu_sec_bk_tf(uint32_t type, uint32_t id, u64 *iova, u64 *pa, u32 *fault_id);

#endif

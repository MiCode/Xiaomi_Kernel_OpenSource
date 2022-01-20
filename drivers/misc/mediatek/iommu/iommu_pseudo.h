/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */
#ifndef IOMMU_PSEUDO_H
#define IOMMU_PSEUDO_H

enum mtk_iommu_sec_id {
	SEC_ID_SEC_CAM = 0,
	SEC_ID_SVP,
	SEC_ID_SDSP,
	SEC_ID_WFD,
	SEC_ID_COUNT
};

int mtk_iommu_sec_init(int mtk_iommu_sec_id);
bool is_iommu_sec_on_mtee(void);

#endif

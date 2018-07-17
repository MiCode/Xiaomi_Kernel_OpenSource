/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#ifndef __ARM_SMMU_ERRATA_H__
#define __ARM_SMMU_ERRATA_H__

#define ARM_SMMU_MIN_IOVA_ALIGN SZ_16K
#define ARM_SMMU_GUARD_PROT (IOMMU_READ | IOMMU_WRITE | IOMMU_GUARD)

#ifdef CONFIG_ARM_SMMU

struct page *arm_smmu_errata_get_guard_page(int vmid);
#else

static inline struct page *arm_smmu_errata_get_guard_page(
				int vmid)
{
	return NULL;
}
#endif
#endif

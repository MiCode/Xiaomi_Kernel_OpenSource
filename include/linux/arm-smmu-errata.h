/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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

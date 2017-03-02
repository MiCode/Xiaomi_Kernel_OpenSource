/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MSM_IOMMU_H_
#define _MSM_IOMMU_H_

#include "msm_mmu.h"

struct msm_iommu {
	struct msm_mmu base;
	struct iommu_domain *domain;
	int cb;
	phys_addr_t ttbr0;
	uint32_t contextidr;
	bool allow_dynamic;

	struct clk *clocks[5];
	int nr_clocks;

	bool is_coherent;
};
#define to_msm_iommu(x) container_of(x, struct msm_iommu, base)

static inline bool msm_iommu_allow_dynamic(struct msm_mmu *mmu)
{
	struct msm_iommu *iommu = to_msm_iommu(mmu);

	return iommu->allow_dynamic;
}

static inline bool msm_iommu_coherent(struct msm_mmu *mmu)
{
	struct msm_iommu *iommu = to_msm_iommu(mmu);

	return iommu->is_coherent;
}
#endif

/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_IOMMU_PAGETABLE_H
#define __ARCH_ARM_MACH_MSM_IOMMU_PAGETABLE_H

struct msm_iommu_pt;

void msm_iommu_pagetable_init(void);
int msm_iommu_pagetable_alloc(struct msm_iommu_pt *pt);
void msm_iommu_pagetable_free(struct msm_iommu_pt *pt);
int msm_iommu_pagetable_map(struct msm_iommu_pt *pt, unsigned long va,
			phys_addr_t pa, size_t len, int prot);
size_t msm_iommu_pagetable_unmap(struct msm_iommu_pt *pt, unsigned long va,
				size_t len);
int msm_iommu_pagetable_map_range(struct msm_iommu_pt *pt, unsigned int va,
			struct scatterlist *sg, unsigned int len, int prot);
void msm_iommu_pagetable_unmap_range(struct msm_iommu_pt *pt, unsigned int va,
				unsigned int len);
phys_addr_t msm_iommu_iova_to_phys_soft(struct iommu_domain *domain,
						phys_addr_t va);
#endif

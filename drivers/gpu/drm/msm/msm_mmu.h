/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MSM_MMU_H__
#define __MSM_MMU_H__

#include <linux/iommu.h>

struct msm_mmu;
struct msm_gpu;

enum msm_mmu_domain_type {
	MSM_SMMU_DOMAIN_UNSECURE,
	MSM_SMMU_DOMAIN_NRT_UNSECURE,
	MSM_SMMU_DOMAIN_SECURE,
	MSM_SMMU_DOMAIN_NRT_SECURE,
	MSM_SMMU_DOMAIN_MAX,
};

struct msm_mmu_funcs {
	int (*attach)(struct msm_mmu *mmu, const char **names, int cnt);
	void (*detach)(struct msm_mmu *mmu, const char **names, int cnt);
	int (*map)(struct msm_mmu *mmu, uint32_t iova, struct sg_table *sgt,
			unsigned len, int prot);
	int (*unmap)(struct msm_mmu *mmu, uint32_t iova, struct sg_table *sgt,
			unsigned len);
	int (*map_sg)(struct msm_mmu *mmu, struct sg_table *sgt,
			enum dma_data_direction dir);
	void (*unmap_sg)(struct msm_mmu *mmu, struct sg_table *sgt,
		enum dma_data_direction dir);
	int (*map_dma_buf)(struct msm_mmu *mmu, struct sg_table *sgt,
			struct dma_buf *dma_buf, int dir);
	void (*unmap_dma_buf)(struct msm_mmu *mmu, struct sg_table *sgt,
			struct dma_buf *dma_buf, int dir);
	void (*destroy)(struct msm_mmu *mmu);
};

struct msm_mmu {
	const struct msm_mmu_funcs *funcs;
	struct device *dev;
};

static inline void msm_mmu_init(struct msm_mmu *mmu, struct device *dev,
		const struct msm_mmu_funcs *funcs)
{
	mmu->dev = dev;
	mmu->funcs = funcs;
}

struct msm_mmu *msm_iommu_new(struct device *dev, struct iommu_domain *domain);
struct msm_mmu *msm_gpummu_new(struct device *dev, struct msm_gpu *gpu);
struct msm_mmu *msm_smmu_new(struct device *dev,
	enum msm_mmu_domain_type domain);

#endif /* __MSM_MMU_H__ */

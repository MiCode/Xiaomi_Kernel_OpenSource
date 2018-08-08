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

enum msm_mmu_domain_type {
	MSM_SMMU_DOMAIN_UNSECURE,
	MSM_SMMU_DOMAIN_NRT_UNSECURE,
	MSM_SMMU_DOMAIN_SECURE,
	MSM_SMMU_DOMAIN_NRT_SECURE,
	MSM_SMMU_DOMAIN_MAX,
};

enum msm_iommu_domain_type {
	MSM_IOMMU_DOMAIN_DEFAULT,
	MSM_IOMMU_DOMAIN_USER,
	MSM_IOMMU_DOMAIN_SECURE,
};

struct msm_mmu_funcs {
	int (*attach)(struct msm_mmu *mmu, const char **names, int cnt);
	void (*detach)(struct msm_mmu *mmu);
	int (*map)(struct msm_mmu *mmu, uint64_t iova, struct sg_table *sgt,
			u32 flags, void *priv);
	void (*unmap)(struct msm_mmu *mmu, uint64_t iova, struct sg_table *sgt,
			void *priv);
	void (*destroy)(struct msm_mmu *mmu);
	void (*enable)(struct msm_mmu *mmu);
	void (*disable)(struct msm_mmu *mmu);
	int (*early_splash_map)(struct msm_mmu *mmu, uint64_t iova,
			struct sg_table *sgt, u32 flags);
	void (*early_splash_unmap)(struct msm_mmu *mmu, uint64_t iova,
			struct sg_table *sgt);
	int (*set_property)(struct msm_mmu *mmu,
				enum iommu_attr attr, void *data);
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

/* Create a new SDE mmu device */
struct msm_mmu *msm_smmu_new(struct device *dev,
	enum msm_mmu_domain_type domain);

/* Create a new legacy MDP4 or GPU mmu device */
struct msm_mmu *msm_iommu_new(struct device *parent,
		enum msm_iommu_domain_type type, struct iommu_domain *domain);

/* Create a new dynamic domain for GPU */
struct msm_mmu *msm_iommu_new_dynamic(struct msm_mmu *orig);

static inline void msm_mmu_enable(struct msm_mmu *mmu)
{
	if (mmu->funcs->enable)
		mmu->funcs->enable(mmu);
}

static inline void msm_mmu_disable(struct msm_mmu *mmu)
{
	if (mmu->funcs->disable)
		mmu->funcs->disable(mmu);
}

/* SDE smmu driver initialize and cleanup functions */
int __init msm_smmu_driver_init(void);
void __exit msm_smmu_driver_cleanup(void);

/* register custom fault handler for a specific domain */
void msm_smmu_register_fault_handler(struct msm_mmu *mmu,
	iommu_fault_handler_t handler);

#endif /* __MSM_MMU_H__ */

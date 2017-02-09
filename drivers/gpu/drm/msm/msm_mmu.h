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
#include <linux/dma-buf.h>

struct msm_mmu;
struct msm_gpu;
struct msm_gem_object;
struct msm_mmu_dev;

/* range of mmu */
#define MSM_MMU_GLOBAL_MEM_SIZE    SZ_64M
#define MSM_MMU_GLOBAL_MEM_BASE    0xf8000000

#define MSM_MMU_SECURE_SIZE        SZ_256M
#define MSM_MMU_SECURE_END         MSM_MMU_GLOBAL_MEM_BASE
#define MSM_MMU_SECURE_BASE        \
	(MSM_MMU_GLOBAL_MEM_BASE - MSM_MMU_SECURE_SIZE)

#define MSM_MMU_VA_BASE32          0x300000
#define MSM_MMU_VA_END32           (0xC0000000 - SZ_16M)
#define MSM_MMU_VA_SIZE32          (MSM_MMU_VA_END32 - MSM_MMU_VA_BASE32)

#define MSM_MMU_VA_BASE64          0x500000000ULL
#define MSM_MMU_VA_END64           0x600000000ULL
#define MSM_MMU_VA_SIZE64          (MSM_MMU_VA_END64 - MSM_MMU_VA_BASE64)

enum msm_mmu_domain_type {
	MSM_SMMU_DOMAIN_UNSECURE = 0,
	MSM_SMMU_DOMAIN_NRT_UNSECURE,
	MSM_SMMU_DOMAIN_SECURE,
	MSM_SMMU_DOMAIN_NRT_SECURE,
	MSM_SMMU_DOMAIN_GPU_UNSECURE,
	MSM_SMMU_DOMAIN_GPU_SECURE,
	MSM_SMMU_DOMAIN_MAX,
};

/* mmu name */
#define MSM_MMU_UNSECURE        0xFFFF0001
#define MSM_MMU_NRT_UNSECURE    0xFFFF0002
#define MSM_MMU_SECURE          0xFFFF0003
#define MSM_MMU_NRT_SECURE      0xFFFF0004
#define MSM_MMU_GPU_UNSECURE    0xFFFF0005
#define MSM_MMU_GPU_SECURE      0xFFFF0006

/* MMU has register retention */
#define MSM_MMU_RETENTION  BIT(1)
/* MMU requires the TLB to be flushed on map */
#define MSM_MMU_FLUSH_TLB_ON_MAP BIT(2)
/* MMU uses global pagetable */
#define MSM_MMU_GLOBAL_PAGETABLE BIT(3)
/* MMU uses hypervisor for content protection */
#define MSM_MMU_HYP_SECURE_ALLOC BIT(4)
/* Force 32 bit, even if the MMU can do 64 bit */
#define MSM_MMU_FORCE_32BIT BIT(5)
/* 64 bit address is live */
#define MSM_MMU_64BIT BIT(6)
/* MMU can do coherent hardware table walks */
#define MSM_MMU_COHERENT_HTW BIT(7)
/* The MMU supports non-contigious pages */
#define MSM_MMU_PAGED BIT(8)
/* The device requires a guard page */
#define MSM_MMU_NEED_GUARD_PAGE BIT(9)

struct msm_mmu_funcs {
	int (*attach)(struct msm_mmu *mmu, const char **names, int cnt);
	void (*detach)(struct msm_mmu *mmu, const char **names, int cnt);
	int (*map)(struct msm_mmu *mmu, dma_addr_t iova,
			struct sg_table *sgt, int prot);
	int (*unmap)(struct msm_mmu *mmu, dma_addr_t iova,
			struct sg_table *sgt);
	int (*bo_map)(struct msm_mmu *mmu, struct msm_gem_object *bo);
	int (*bo_unmap)(struct msm_mmu *mmu, struct msm_gem_object *bo);

	/* For switching in multi-context */
	u64 (*get_ttbr0)(struct msm_mmu *mmu);
	u32 (*get_contextidr)(struct msm_mmu *mmu);
	u32 (*get_cb_num)(struct msm_mmu *mmu);

	/* iova management. */
	int (*get_iovaddr)(struct msm_mmu *mmu, struct msm_gem_object *bo,
			uint64_t *iova);
	int (*put_iovaddr)(struct msm_mmu *mmu, struct msm_gem_object *bo);

	bool (*addr_in_range)(struct msm_mmu *mmu, uint64_t);
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
	struct kref refcount;
	const struct msm_mmu_funcs *funcs;
	struct device *dev;
	struct drm_device *drm_dev;
	uint32_t name;
	int id;
};

#define MMU_OP_VALID(_mmu, _field) \
	(((_mmu) != NULL) && \
	 ((_mmu)->funcs != NULL) && \
	 ((_mmu)->funcs->_field != NULL))

struct msm_mmu_dev_funcs {
	int (*get_cb_num)(struct msm_mmu_dev *mmu_dev);
	void (*set_cb_num)(struct msm_mmu_dev *mmu_dev, uint32_t cb_num);
	int (*cpu_set_pt)(struct msm_mmu_dev *mmu_dev, struct msm_mmu *mmu);
};

struct msm_mmu_dev {
	struct device *dev;
	const struct msm_mmu_dev_funcs *funcs;
};

#define MMU_DEV_OP_VALID(_mmu, _field) \
	(((_mmu) != NULL) && \
	 ((_mmu)->funcs != NULL) && \
	 ((_mmu)->funcs->_field != NULL))

static inline void msm_mmu_init(struct msm_mmu *mmu, struct drm_device *drm_dev,
	struct device *dev, uint32_t name, const struct msm_mmu_funcs *funcs)
{
	mmu->drm_dev = drm_dev;
	mmu->dev = dev;
	mmu->name = name;
	mmu->funcs = funcs;
	mmu->id = -1;
	kref_init(&mmu->refcount);
}

static inline void msm_mmu_dev_init(struct msm_mmu_dev *mdev,
		struct device *dev, const struct msm_mmu_dev_funcs *funcs)
{
	mdev->dev = dev;
	mdev->funcs = funcs;
}

struct msm_mmu *msm_smmu_new(struct drm_device *drm_dev, struct device *dev,
	enum msm_mmu_domain_type domain);
struct msm_mmu *msm_iommu_new(struct drm_device *dev,
	struct msm_mmu_dev *mmu_dev, uint32_t name);
#endif /* __MSM_MMU_H__ */

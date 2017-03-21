/*
 * Copyright (C) 2016 Red Hat
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

#include "msm_drv.h"
#include "msm_gem.h"
#include "msm_iommu.h"

static void
msm_gem_address_space_destroy(struct kref *kref)
{
	struct msm_gem_address_space *aspace = container_of(kref,
			struct msm_gem_address_space, kref);

	if (aspace->ops->destroy)
		aspace->ops->destroy(aspace);

	kfree(aspace);
}

void msm_gem_address_space_put(struct msm_gem_address_space *aspace)
{
	if (aspace)
		kref_put(&aspace->kref, msm_gem_address_space_destroy);
}

/* SDE address space operations */
static void smmu_aspace_unmap_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		void *priv)
{
	struct dma_buf *buf = priv;

	if (buf)
		aspace->mmu->funcs->unmap_dma_buf(aspace->mmu,
			sgt, buf, DMA_BIDIRECTIONAL);
	else
		aspace->mmu->funcs->unmap_sg(aspace->mmu, sgt,
			DMA_BIDIRECTIONAL);

	vma->iova = 0;

	msm_gem_address_space_put(aspace);
}


static int smmu_aspace_map_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		void *priv, unsigned int flags)
{
	struct dma_buf *buf = priv;
	int ret;

	if (buf)
		ret = aspace->mmu->funcs->map_dma_buf(aspace->mmu, sgt, buf,
			DMA_BIDIRECTIONAL);
	else
		ret = aspace->mmu->funcs->map_sg(aspace->mmu, sgt,
			DMA_BIDIRECTIONAL);

	if (!ret)
		vma->iova = sg_dma_address(sgt->sgl);

	/* Get a reference to the aspace to keep it around */
	kref_get(&aspace->kref);

	return ret;
}

static const struct msm_gem_aspace_ops smmu_aspace_ops = {
	.map = smmu_aspace_map_vma,
	.unmap = smmu_aspace_unmap_vma,
};

struct msm_gem_address_space *
msm_gem_smmu_address_space_create(struct device *dev, struct msm_mmu *mmu,
		const char *name)
{
	struct msm_gem_address_space *aspace;

	if (!mmu)
		return ERR_PTR(-EINVAL);

	aspace = kzalloc(sizeof(*aspace), GFP_KERNEL);
	if (!aspace)
		return ERR_PTR(-ENOMEM);

	aspace->name = name;
	aspace->mmu = mmu;
	aspace->ops = &smmu_aspace_ops;

	kref_init(&aspace->kref);

	return aspace;
}

/* GPU address space operations */
struct msm_iommu_aspace {
	struct msm_gem_address_space base;
	struct drm_mm mm;
};

#define to_iommu_aspace(aspace) \
	((struct msm_iommu_aspace *) \
	 container_of(aspace, struct msm_iommu_aspace, base))

static void iommu_aspace_unmap_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt, void *priv)
{
	if (!vma->iova)
		return;

	if (aspace->mmu)
		aspace->mmu->funcs->unmap(aspace->mmu, vma->iova, sgt);

	drm_mm_remove_node(&vma->node);

	vma->iova = 0;

	msm_gem_address_space_put(aspace);
}

static int iommu_aspace_map_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt, void *priv,
		unsigned int flags)
{
	struct msm_iommu_aspace *local = to_iommu_aspace(aspace);
	size_t size = 0;
	struct scatterlist *sg;
	int ret, i;
	int iommu_flags = IOMMU_READ;

	if (!(flags & MSM_BO_GPU_READONLY))
		iommu_flags |= IOMMU_WRITE;

	if (flags & MSM_BO_PRIVILEGED)
		iommu_flags |= IOMMU_PRIV;

	if ((flags & MSM_BO_CACHED) && msm_iommu_coherent(aspace->mmu))
		iommu_flags |= IOMMU_CACHE;

	if (WARN_ON(drm_mm_node_allocated(&vma->node)))
		return 0;

	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		size += sg->length + sg->offset;

	ret = drm_mm_insert_node(&local->mm, &vma->node, size >> PAGE_SHIFT,
			0, DRM_MM_SEARCH_DEFAULT);
	if (ret)
		return ret;

	vma->iova = vma->node.start << PAGE_SHIFT;

	if (aspace->mmu)
		ret = aspace->mmu->funcs->map(aspace->mmu, vma->iova, sgt,
			iommu_flags);

	/* Get a reference to the aspace to keep it around */
	kref_get(&aspace->kref);

	return ret;
}

static void iommu_aspace_destroy(struct msm_gem_address_space *aspace)
{
	struct msm_iommu_aspace *local = to_iommu_aspace(aspace);

	drm_mm_takedown(&local->mm);
	aspace->mmu->funcs->destroy(aspace->mmu);
}

static const struct msm_gem_aspace_ops msm_iommu_aspace_ops = {
	.map = iommu_aspace_map_vma,
	.unmap = iommu_aspace_unmap_vma,
	.destroy = iommu_aspace_destroy,
};

static struct msm_gem_address_space *
msm_gem_address_space_new(struct msm_mmu *mmu, const char *name,
		uint64_t start, uint64_t end)
{
	struct msm_iommu_aspace *local;

	if (!mmu)
		return ERR_PTR(-EINVAL);

	local = kzalloc(sizeof(*local), GFP_KERNEL);
	if (!local)
		return ERR_PTR(-ENOMEM);

	drm_mm_init(&local->mm, (start >> PAGE_SHIFT),
		(end >> PAGE_SHIFT) - 1);

	local->base.name = name;
	local->base.mmu = mmu;
	local->base.ops = &msm_iommu_aspace_ops;

	kref_init(&local->base.kref);

	return &local->base;
}

int msm_gem_map_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		void *priv, unsigned int flags)
{
	if (aspace && aspace->ops->map)
		return aspace->ops->map(aspace, vma, sgt, priv, flags);

	return -EINVAL;
}

void msm_gem_unmap_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt, void *priv)
{
	if (aspace && aspace->ops->unmap)
		aspace->ops->unmap(aspace, vma, sgt, priv);
}

struct msm_gem_address_space *
msm_gem_address_space_create(struct device *dev, struct iommu_domain *domain,
		const char *name)
{
	struct msm_mmu *mmu = msm_iommu_new(dev, domain);

	if (IS_ERR(mmu))
		return (struct msm_gem_address_space *) mmu;

	return msm_gem_address_space_new(mmu, name,
		domain->geometry.aperture_start,
		domain->geometry.aperture_end);
}

/* Create a new dynamic instance */
struct msm_gem_address_space *
msm_gem_address_space_create_instance(struct msm_mmu *parent, const char *name,
		uint64_t start, uint64_t end)
{
	struct msm_mmu *child = msm_iommu_new_dynamic(parent);

	if (IS_ERR(child))
		return (struct msm_gem_address_space *) child;

	return msm_gem_address_space_new(child, name, start, end);
}

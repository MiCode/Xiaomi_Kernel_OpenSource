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

	if (aspace->va_len)
		drm_mm_takedown(&aspace->mm);

	aspace->mmu->funcs->destroy(aspace->mmu);

	kfree(aspace);
}

void msm_gem_address_space_put(struct msm_gem_address_space *aspace)
{
	if (aspace)
		kref_put(&aspace->kref, msm_gem_address_space_destroy);
}

static struct msm_gem_address_space *
msm_gem_address_space_new(struct msm_mmu *mmu, const char *name,
		uint64_t start, uint64_t end)
{
	struct msm_gem_address_space *aspace;

	if (!mmu)
		return ERR_PTR(-EINVAL);

	aspace = kzalloc(sizeof(*aspace), GFP_KERNEL);
	if (!aspace)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&aspace->lock);
	aspace->name = name;
	aspace->mmu = mmu;

	aspace->va_len = end - start;

	if (aspace->va_len)
		drm_mm_init(&aspace->mm, (start >> PAGE_SHIFT),
			(end >> PAGE_SHIFT) - 1);

	kref_init(&aspace->kref);

	return aspace;
}

static int allocate_iova(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		u64 *iova)
{
	struct scatterlist *sg;
	size_t size = 0;
	int ret, i;

	if (!aspace->va_len)
		return 0;

	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		size += sg->length + sg->offset;

	spin_lock(&aspace->lock);

	if (WARN_ON(drm_mm_node_allocated(&vma->node))) {
		spin_unlock(&aspace->lock);
		return 0;
	}
	ret = drm_mm_insert_node(&aspace->mm, &vma->node,
			size >> PAGE_SHIFT, 0, DRM_MM_SEARCH_DEFAULT);

	spin_unlock(&aspace->lock);

	if (!ret && iova)
		*iova = vma->node.start << PAGE_SHIFT;

	return ret;
}

int msm_gem_map_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		void *priv, unsigned int flags)
{
	u64 iova = 0;
	int ret;

	if (!aspace)
		return -EINVAL;

	ret = allocate_iova(aspace, vma, sgt, &iova);
	if (ret)
		return ret;

	ret = aspace->mmu->funcs->map(aspace->mmu, iova, sgt,
		flags, priv);

	if (ret) {
		spin_lock(&aspace->lock);
		if (drm_mm_node_allocated(&vma->node))
			drm_mm_remove_node(&vma->node);
		spin_unlock(&aspace->lock);

		return ret;
	}

	vma->iova = sg_dma_address(sgt->sgl);
	kref_get(&aspace->kref);

	return 0;
}

void msm_gem_unmap_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt, void *priv)
{
	if (!aspace || !vma->iova)
		return;

	aspace->mmu->funcs->unmap(aspace->mmu, vma->iova, sgt, priv);

	spin_lock(&aspace->lock);
	if (drm_mm_node_allocated(&vma->node))
		drm_mm_remove_node(&vma->node);
	spin_unlock(&aspace->lock);

	vma->iova = 0;

	msm_gem_address_space_put(aspace);
}

struct msm_gem_address_space *
msm_gem_smmu_address_space_create(struct device *dev, struct msm_mmu *mmu,
		const char *name)
{
	return msm_gem_address_space_new(mmu, name, 0, 0);
}

struct msm_gem_address_space *
msm_gem_address_space_create(struct device *dev, struct iommu_domain *domain,
		int type, const char *name)
{
	struct msm_mmu *mmu = msm_iommu_new(dev, type, domain);

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

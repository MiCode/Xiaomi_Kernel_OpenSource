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
#include "msm_mmu.h"

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
}


static int smmu_aspace_map_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		void *priv, unsigned int flags)
{
	struct dma_buf *buf = priv;
	int ret;

	if (!aspace || !aspace->domain_attached)
		return -EINVAL;

	if (buf)
		ret = aspace->mmu->funcs->map_dma_buf(aspace->mmu, sgt, buf,
			DMA_BIDIRECTIONAL, flags);
	else
		ret = aspace->mmu->funcs->map_sg(aspace->mmu, sgt,
			DMA_BIDIRECTIONAL);

	if (!ret)
		vma->iova = sg_dma_address(sgt->sgl);

	return ret;
}

static void smmu_aspace_destroy(struct msm_gem_address_space *aspace)
{
	aspace->mmu->funcs->destroy(aspace->mmu);
}

static void smmu_aspace_add_to_active(
		struct msm_gem_address_space *aspace,
		struct msm_gem_object *msm_obj)
{
	WARN_ON(!mutex_is_locked(&aspace->dev->struct_mutex));
	list_move_tail(&msm_obj->iova_list, &aspace->active_list);
	msm_obj->in_active_list = true;
}

static void smmu_aspace_remove_from_active(
		struct msm_gem_address_space *aspace,
		struct msm_gem_object *obj)
{
	struct msm_gem_object *msm_obj, *next;

	WARN_ON(!mutex_is_locked(&aspace->dev->struct_mutex));

	list_for_each_entry_safe(msm_obj, next, &aspace->active_list,
			iova_list) {
		if (msm_obj == obj) {
			msm_obj->in_active_list = false;
			list_del(&msm_obj->iova_list);
			break;
		}
	}
}

static int smmu_aspace_register_cb(
		struct msm_gem_address_space *aspace,
		void (*cb)(void *, bool),
		void *cb_data)
{
	struct aspace_client *aclient = NULL;
	struct aspace_client *temp;

	if (!aspace)
		return -EINVAL;

	if (!aspace->domain_attached)
		return -EACCES;

	aclient = kzalloc(sizeof(*aclient), GFP_KERNEL);
	if (!aclient)
		return -ENOMEM;

	aclient->cb = cb;
	aclient->cb_data = cb_data;
	INIT_LIST_HEAD(&aclient->list);

	/* check if callback is already registered */
	mutex_lock(&aspace->dev->struct_mutex);
	list_for_each_entry(temp, &aspace->clients, list) {
		if ((temp->cb == aclient->cb) &&
			(temp->cb_data == aclient->cb_data)) {
			kfree(aclient);
			mutex_unlock(&aspace->dev->struct_mutex);
			return -EEXIST;
		}
	}

	list_move_tail(&aclient->list, &aspace->clients);
	mutex_unlock(&aspace->dev->struct_mutex);

	return 0;
}

static int smmu_aspace_unregister_cb(
		struct msm_gem_address_space *aspace,
		void (*cb)(void *, bool),
		void *cb_data)
{
	struct aspace_client *aclient = NULL;
	int rc = -ENOENT;

	if (!aspace || !cb)
		return -EINVAL;

	mutex_lock(&aspace->dev->struct_mutex);
	list_for_each_entry(aclient, &aspace->clients, list) {
		if ((aclient->cb == cb) &&
			(aclient->cb_data == cb_data)) {
			list_del(&aclient->list);
			kfree(aclient);
			rc = 0;
			break;
		}
	}
	mutex_unlock(&aspace->dev->struct_mutex);

	return rc;
}


static const struct msm_gem_aspace_ops smmu_aspace_ops = {
	.map = smmu_aspace_map_vma,
	.unmap = smmu_aspace_unmap_vma,
	.destroy = smmu_aspace_destroy,
	.add_to_active = smmu_aspace_add_to_active,
	.remove_from_active = smmu_aspace_remove_from_active,
	.register_cb = smmu_aspace_register_cb,
	.unregister_cb = smmu_aspace_unregister_cb,
};

struct msm_gem_address_space *
msm_gem_smmu_address_space_create(struct drm_device *dev, struct msm_mmu *mmu,
		const char *name)
{
	struct msm_gem_address_space *aspace;

	if (!mmu)
		return ERR_PTR(-EINVAL);

	aspace = kzalloc(sizeof(*aspace), GFP_KERNEL);
	if (!aspace)
		return ERR_PTR(-ENOMEM);

	aspace->dev = dev;
	aspace->name = name;
	aspace->mmu = mmu;
	aspace->ops = &smmu_aspace_ops;
	INIT_LIST_HEAD(&aspace->active_list);
	INIT_LIST_HEAD(&aspace->clients);

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
}

static int iommu_aspace_map_vma(struct msm_gem_address_space *aspace,
		struct msm_gem_vma *vma, struct sg_table *sgt,
		void *priv, unsigned int flags)
{
	struct msm_iommu_aspace *local = to_iommu_aspace(aspace);
	size_t size = 0;
	struct scatterlist *sg;
	int ret = 0, i;

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
		ret = aspace->mmu->funcs->map(aspace->mmu, vma->iova,
			sgt, IOMMU_READ | IOMMU_WRITE);

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

void
msm_gem_address_space_destroy(struct msm_gem_address_space *aspace)
{
	if (aspace && aspace->ops->destroy)
		aspace->ops->destroy(aspace);

	kfree(aspace);
}

void msm_gem_add_obj_to_aspace_active_list(
		struct msm_gem_address_space *aspace,
		struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	if (aspace && aspace->ops && aspace->ops->add_to_active)
		aspace->ops->add_to_active(aspace, msm_obj);
}

void msm_gem_remove_obj_from_aspace_active_list(
		struct msm_gem_address_space *aspace,
		struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	if (aspace && aspace->ops && aspace->ops->remove_from_active)
		aspace->ops->remove_from_active(aspace, msm_obj);
}

int msm_gem_address_space_register_cb(struct msm_gem_address_space *aspace,
		void (*cb)(void *, bool),
		void *cb_data)
{
	if (aspace && aspace->ops && aspace->ops->register_cb)
		return aspace->ops->register_cb(aspace, cb, cb_data);

	return -EINVAL;
}

int msm_gem_address_space_unregister_cb(struct msm_gem_address_space *aspace,
		void (*cb)(void *, bool),
		void *cb_data)
{
	if (aspace && aspace->ops && aspace->ops->unregister_cb)
		return aspace->ops->unregister_cb(aspace, cb, cb_data);

	return -EINVAL;
}


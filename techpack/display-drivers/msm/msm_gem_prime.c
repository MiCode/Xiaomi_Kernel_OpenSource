/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
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

#include "msm_drv.h"
#include "msm_gem.h"
#include "msm_mmu.h"
#include "msm_kms.h"

#include <drm/drm_drv.h>

#include <linux/qcom-dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/ion.h>
#include <linux/msm_ion.h>

struct sg_table *msm_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	int npages = obj->size >> PAGE_SHIFT;

	if (WARN_ON(!msm_obj->pages))  /* should have already pinned! */
		return NULL;

	return drm_prime_pages_to_sg(obj->dev, msm_obj->pages, npages);
}

void *msm_gem_prime_vmap(struct drm_gem_object *obj)
{
	return msm_gem_get_vaddr(obj);
}

void msm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
{
	msm_gem_put_vaddr(obj);
}

int msm_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	int ret;

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret < 0)
		return ret;

	return msm_gem_mmap_obj(vma->vm_private_data, vma);
}

struct drm_gem_object *msm_gem_prime_import_sg_table(struct drm_device *dev,
		struct dma_buf_attachment *attach, struct sg_table *sg)
{
	return msm_gem_import(dev, attach->dmabuf, sg);
}

int msm_gem_prime_pin(struct drm_gem_object *obj)
{
	if (!obj->import_attach)
		msm_gem_get_pages(obj);
	return 0;
}

void msm_gem_prime_unpin(struct drm_gem_object *obj)
{
	if (!obj->import_attach)
		msm_gem_put_pages(obj);
}

struct dma_resv *msm_gem_prime_res_obj(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);

	return msm_obj->resv;
}


struct drm_gem_object *msm_gem_prime_import(struct drm_device *dev,
					    struct dma_buf *dma_buf)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt = NULL;
	struct drm_gem_object *obj;
	struct device *attach_dev = NULL;
	struct msm_drm_private *priv;
	struct msm_kms *kms;
	int ret;
	bool lazy_unmap = true;
	u32 domain;

	if (!dma_buf || !dev->dev_private)
		return ERR_PTR(-EINVAL);

	priv = dev->dev_private;
	kms = priv->kms;

	if (dma_buf->priv && !dma_buf->ops->begin_cpu_access) {
		obj = dma_buf->priv;
		if (obj->dev == dev) {
			/*
			 * Importing dmabuf exported from out own gem increases
			 * refcount on gem itself instead of f_count of dmabuf.
			 */
			drm_gem_object_get(obj);
			return obj;
		}
	}

	if (!dev->driver->gem_prime_import_sg_table) {
		DRM_ERROR("NULL gem_prime_import_sg_table\n");
		return ERR_PTR(-EINVAL);
	}

	get_dma_buf(dma_buf);

	if (!kms || !kms->funcs->get_address_space_device) {
		DRM_ERROR("invalid kms ops\n");
		ret = -EINVAL;
		goto fail_put;
	}

	/*
	 * - attach default drm device for all S2 only buffers or
	 *   when IOMMU is not available
	 * - avoid using lazying unmap feature as it doesn't add
	 * any value without nested translations
	 */
	if (!iommu_present(&platform_bus_type)) {
		attach_dev = dev->dev;
		lazy_unmap = false;
	} else {
		domain = MSM_SMMU_DOMAIN_UNSECURE;
		attach_dev = kms->funcs->get_address_space_device(kms, domain);
	}

	/*
	 * While transitioning from secure use-cases, the secure/non-secure
	 * context bank might still not be attached back, while the
	 * prime_fd_to_handle call is made for the next frame. Attach those
	 * buffers to default drm device and reattaching with the correct
	 * context-bank will be handled in msm_gem_delayed_import.
	 */
	if (!attach_dev) {
		DRM_DEBUG("attaching dma buf with default drm device\n");
		attach_dev = dev->dev;
	}

	attach = dma_buf_attach(dma_buf, attach_dev);
	if (IS_ERR(attach)) {
		DRM_ERROR("dma_buf_attach failure, err=%ld\n", PTR_ERR(attach));
		return ERR_CAST(attach);
	}

	/*
	 * For cached buffers where CPU access is required, dma_map_attachment
	 * must be called now to allow user-space to perform cpu sync begin/end
	 * otherwise do delayed mapping during the commit.
	 */
	if (lazy_unmap)
		attach->dma_map_attrs |= DMA_ATTR_DELAYED_UNMAP;
	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		DRM_ERROR(
		"dma_buf_map_attachment failure, err=%d\n", ret);
		goto fail_detach;
	}

	/*
	 * If importing a NULL sg table (i.e. for uncached buffers),
	 * create a drm gem object with only the dma buf attachment.
	 */
	obj = dev->driver->gem_prime_import_sg_table(dev, attach, sgt);
	if (IS_ERR(obj)) {
		ret = PTR_ERR(obj);
		DRM_ERROR("gem_prime_import_sg_table failure, err=%d\n", ret);
		goto fail_unmap;
	}

	obj->import_attach = attach;

	return obj;

fail_unmap:
	if (sgt)
		dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
fail_detach:
	dma_buf_detach(dma_buf, attach);
fail_put:
	dma_buf_put(dma_buf);

	return ERR_PTR(ret);
}

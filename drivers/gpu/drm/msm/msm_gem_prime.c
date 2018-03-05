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

#include "msm_drv.h"
#include "msm_gem.h"

#include <linux/dma-buf.h>
#include <linux/ion.h>

struct sg_table *msm_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct msm_gem_object *msm_obj = to_msm_bo(obj);
	int npages = obj->size >> PAGE_SHIFT;

	if (WARN_ON(!msm_obj->pages))  /* should have already pinned! */
		return NULL;

	return drm_prime_pages_to_sg(msm_obj->pages, npages);
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

struct reservation_object *msm_gem_prime_res_obj(struct drm_gem_object *obj)
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
	struct device *attach_dev;
	unsigned long flags = 0;
	int ret;

	if (!dma_buf)
		return ERR_PTR(-EINVAL);

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

	attach_dev = dev->dev;
	attach = dma_buf_attach(dma_buf, attach_dev);
	if (IS_ERR(attach)) {
		DRM_ERROR("dma_buf_attach failure, err=%ld\n", PTR_ERR(attach));
		return ERR_CAST(attach);
	}

	get_dma_buf(dma_buf);

	/*
	 * For cached buffers where CPU access is required, dma_map_attachment
	 * must be called now to allow user-space to perform cpu sync begin/end
	 * otherwise do delayed mapping during the commit.
	 */
	ret = dma_buf_get_flags(dma_buf, &flags);
	if (ret) {
		DRM_ERROR("dma_buf_get_flags failure, err=%d\n", ret);
		goto fail_put;
	} else if (flags & ION_FLAG_CACHED) {
		attach->dma_map_attrs |= DMA_ATTR_DELAYED_UNMAP;
		sgt = dma_buf_map_attachment(
				attach, DMA_BIDIRECTIONAL);
		if (IS_ERR(sgt)) {
			ret = PTR_ERR(sgt);
			DRM_ERROR(
			"dma_buf_map_attachment failure, err=%d\n",
				ret);
			goto fail_detach;
		}
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

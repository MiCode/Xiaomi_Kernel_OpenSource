// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/dma-buf.h>
#include <linux/pm_runtime.h>

#include <drm/drm_gem.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_prime.h>

#include "sprd_drm.h"
#include "sprd_gem.h"

static const struct drm_gem_object_funcs sprd_gem_object_funcs = {
	.free = sprd_gem_free_object,
	.get_sg_table = sprd_gem_prime_get_sg_table,
	.vmap = drm_gem_cma_vmap,
	.vm_ops = &drm_gem_cma_vm_ops,
};

static struct sprd_gem_obj *sprd_gem_obj_create(struct drm_device *drm,
						unsigned long size)
{
	struct sprd_gem_obj *sprd_gem;
	struct drm_gem_object *obj;
	int ret;

	sprd_gem = kzalloc(sizeof(*sprd_gem), GFP_KERNEL);
	if (!sprd_gem)
		return ERR_PTR(-ENOMEM);

	obj = &sprd_gem->base;
	obj->funcs = &sprd_gem_object_funcs;

	ret = drm_gem_object_init(drm, &sprd_gem->base, size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		goto error;
	}

	ret = drm_gem_create_mmap_offset(&sprd_gem->base);
	if (ret) {
		drm_gem_object_release(&sprd_gem->base);
		goto error;
	}

	return sprd_gem;

error:
	kfree(sprd_gem);
	return ERR_PTR(ret);
}

void sprd_gem_free_object(struct drm_gem_object *obj)
{
	struct sprd_gem_obj *sprd_gem = to_sprd_gem_obj(obj);

	DRM_DEBUG("gem = %p\n", obj);

	if (sprd_gem->vaddr)
		dma_free_wc(obj->dev->dev, obj->size,
			sprd_gem->vaddr, sprd_gem->dma_addr);
	else if (sprd_gem->sgtb)
		drm_prime_gem_destroy(obj, sprd_gem->sgtb);

	drm_gem_object_release(obj);

	kfree(sprd_gem);
}

int sprd_gem_dumb_create(struct drm_file *file_priv, struct drm_device *drm,
			    struct drm_mode_create_dumb *args)
{
	struct sprd_gem_obj *sprd_gem;
	int ret;

	args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	args->size = round_up(args->pitch * args->height, PAGE_SIZE);

	sprd_gem = sprd_gem_obj_create(drm, args->size);
	if (IS_ERR(sprd_gem))
		return PTR_ERR(sprd_gem);

	sprd_gem->vaddr = dma_alloc_wc(drm->dev, args->size,
			&sprd_gem->dma_addr, GFP_KERNEL | __GFP_NOWARN);
	DRM_INFO("args->pitch:%u args->size:%llu vaddr:0x%px\n",
              args->pitch, args->size, sprd_gem->vaddr);
	if (!sprd_gem->vaddr) {
		DRM_ERROR("failed to allocate buffer with size %llu\n",
			  args->size);
		ret = -ENOMEM;
		goto error;
	}

	ret = drm_gem_handle_create(file_priv, &sprd_gem->base, &args->handle);
	if (ret)
		goto error;

	drm_gem_object_put(&sprd_gem->base);

	return 0;

error:
	sprd_gem_free_object(&sprd_gem->base);
	return ret;
}

int sprd_gem_object_mmap(struct drm_gem_object *obj,
				   struct vm_area_struct *vma)

{
	struct sprd_gem_obj *sprd_gem = to_sprd_gem_obj(obj);
	int ret;

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	ret = dma_mmap_wc(obj->dev->dev, vma,
				    sprd_gem->vaddr, sprd_gem->dma_addr,
				    vma->vm_end - vma->vm_start);

	if (ret)
		drm_gem_vm_close(vma);

	return ret;
}

int sprd_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	obj = vma->vm_private_data;

	return sprd_gem_object_mmap(obj, vma);
}

int sprd_gem_prime_mmap(struct drm_gem_object *obj,
			    struct vm_area_struct *vma)
{
	int ret;

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret)
		return ret;

	return sprd_gem_object_mmap(obj, vma);
}

struct sg_table *sprd_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct sprd_gem_obj *sprd_gem = to_sprd_gem_obj(obj);
	struct sg_table *sgtb;
	int ret;

	sgtb = kzalloc(sizeof(*sgtb), GFP_KERNEL);
	if (!sgtb)
		return ERR_PTR(-ENOMEM);

	ret = dma_get_sgtable(obj->dev->dev, sgtb, sprd_gem->vaddr,
			      sprd_gem->dma_addr, obj->size);
	if (ret) {
		DRM_ERROR("failed to allocate sg_table, %d\n", ret);
		kfree(sgtb);
		return ERR_PTR(ret);
	}

	return sgtb;
}

struct drm_gem_object *sprd_gem_prime_import_sg_table(struct drm_device *drm,
		struct dma_buf_attachment *attach, struct sg_table *sgtb)
{
	struct sprd_gem_obj *sprd_gem;

	sprd_gem = sprd_gem_obj_create(drm, attach->dmabuf->size);
	if (IS_ERR(sprd_gem))
		return ERR_CAST(sprd_gem);

	DRM_DEBUG("gem = %p\n", &sprd_gem->base);

	if (sgtb->nents == 1)
		sprd_gem->dma_addr = sg_dma_address(sgtb->sgl);

	sprd_gem->sgtb = sgtb;

	if (!(strcmp(attach->dmabuf->exp_name, "system") &&
	      strcmp(attach->dmabuf->exp_name, "system-uncached")))
		sprd_gem->need_iommu = true;

	return &sprd_gem->base;
}

/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem.h>
#include <linux/dma-buf.h>
#include <linux/reservation.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"

/*
 * mtk specific framebuffer structure.
 *
 * @fb: drm framebuffer object.
 * @gem_obj: array of gem objects.
 */
struct mtk_drm_fb {
	struct drm_framebuffer	base;
	/* For now we only support a single plane */
	struct drm_gem_object	*gem_obj;
};

#define to_mtk_fb(x) container_of(x, struct mtk_drm_fb, base)

struct drm_gem_object *mtk_fb_get_gem_obj(struct drm_framebuffer *fb)
{
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);

	return mtk_fb->gem_obj;
}

static int mtk_drm_fb_create_handle(struct drm_framebuffer *fb,
				    struct drm_file *file_priv,
				    unsigned int *handle)
{
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);

	return drm_gem_handle_create(file_priv, mtk_fb->gem_obj, handle);
}

static void mtk_drm_fb_destroy(struct drm_framebuffer *fb)
{
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);

	drm_framebuffer_cleanup(fb);

	drm_gem_object_unreference_unlocked(mtk_fb->gem_obj);

	kfree(mtk_fb);
}

static const struct drm_framebuffer_funcs mtk_drm_fb_funcs = {
	.create_handle = mtk_drm_fb_create_handle,
	.destroy = mtk_drm_fb_destroy,
};

static struct mtk_drm_fb *mtk_drm_framebuffer_init(struct drm_device *dev,
					struct drm_mode_fb_cmd2 *mode,
					struct drm_gem_object *obj)
{
	struct mtk_drm_fb *mtk_fb;
	int ret;

	if (drm_format_num_planes(mode->pixel_format) != 1)
		return ERR_PTR(-EINVAL);

	mtk_fb = kzalloc(sizeof(*mtk_fb), GFP_KERNEL);
	if (!mtk_fb)
		return ERR_PTR(-ENOMEM);

	drm_helper_mode_fill_fb_struct(&mtk_fb->base, mode);

	mtk_fb->gem_obj = obj;

	ret = drm_framebuffer_init(dev, &mtk_fb->base, &mtk_drm_fb_funcs);
	if (ret) {
		DRM_ERROR("failed to initialize framebuffer\n");
		kfree(mtk_fb);
		return ERR_PTR(ret);
	}

	return mtk_fb;
}

#ifdef CONFIG_DRM_MEDIATEK_FBDEV
static int mtk_drm_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct drm_fb_helper *helper = info->par;
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(helper->fb);

	return mtk_drm_gem_mmap_buf(mtk_fb->gem_obj, vma);
}

static struct fb_ops mtk_fb_ops = {
	.owner = THIS_MODULE,
	.fb_fillrect = sys_fillrect,
	.fb_copyarea = sys_copyarea,
	.fb_imageblit = sys_imageblit,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_blank = drm_fb_helper_blank,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_mmap = mtk_drm_fb_mmap,
};

static int mtk_fbdev_probe(struct drm_fb_helper *helper,
			   struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = helper->dev;
	struct drm_mode_fb_cmd2 mode = { 0 };
	struct mtk_drm_fb *mtk_fb;
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_gem_object *gem;
	struct fb_info *info;
	struct drm_framebuffer *fb;
	unsigned long offset;
	size_t size;
	int err;

	mode.width = sizes->surface_width;
	mode.height = sizes->surface_height;
	mode.pitches[0] = sizes->surface_width *
			  DIV_ROUND_UP(sizes->surface_bpp, 8);
	mode.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
						      sizes->surface_depth);

	mode.height = mode.height;/* << 1; for fb use? */
	size = mode.pitches[0] * mode.height;
	dev_info(dev->dev, "mtk_fbdev_probe %dx%d bpp %d pitch %d size %zu\n",
		 mode.width, mode.height, sizes->surface_bpp, mode.pitches[0],
		 size);

	mtk_gem = mtk_drm_gem_create(dev, size, true);
	if (IS_ERR(mtk_gem)) {
		err = PTR_ERR(mtk_gem);
		goto fini;
	}

	gem = &mtk_gem->base;

	mtk_fb = mtk_drm_framebuffer_init(dev, &mode, gem);
	if (IS_ERR(mtk_fb)) {
		dev_err(dev->dev, "failed to allocate DRM framebuffer\n");
		err = PTR_ERR(mtk_fb);
		goto free;
	}
	fb = &mtk_fb->base;

	info = framebuffer_alloc(0, dev->dev);
	if (!info) {
		dev_err(dev->dev, "failed to allocate framebuffer info\n");
		err = PTR_ERR(info);
		goto release;
	}

	helper->fb = fb;
	helper->fbdev = info;

	info->par = helper;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->fbops = &mtk_fb_ops;

	err = fb_alloc_cmap(&info->cmap, 256, 0);
	if (err < 0) {
		dev_err(dev->dev, "failed to allocate color map: %d\n", err);
		goto destroy;
	}

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(info, helper, fb->width, fb->height);

	offset = info->var.xoffset * DIV_ROUND_UP(fb->bits_per_pixel, 8);
	offset += info->var.yoffset * fb->pitches[0];

	strcpy(info->fix.id, "mtk");
	/* dev->mode_config.fb_base = (resource_size_t)bo->paddr; */
	info->var.yres = info->var.yres_virtual;/* >> 1; for fb use? */
	info->fix.smem_start = mtk_gem->dma_addr + offset;
	info->fix.smem_len = size;
	info->screen_base = mtk_gem->kvaddr + offset;
	info->screen_size = size;

	return 0;

destroy:
	drm_framebuffer_unregister_private(fb);
	mtk_drm_fb_destroy(fb);
release:
	framebuffer_release(info);
free:
	mtk_drm_gem_free_object(&mtk_gem->base);
fini:
	dev_err(dev->dev, "mtk_fbdev_probe fail\n");
	return err;
}

static const struct drm_fb_helper_funcs mtk_drm_fb_helper_funcs = {
	.fb_probe = mtk_fbdev_probe,
};

int mtk_fbdev_create(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *fbdev;
	int ret;

	fbdev = devm_kzalloc(dev->dev, sizeof(*fbdev), GFP_KERNEL);
	if (!fbdev)
		return -ENOMEM;

	drm_fb_helper_prepare(dev, fbdev, &mtk_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, fbdev, dev->mode_config.num_crtc,
				 dev->mode_config.num_connector);
	if (ret) {
		dev_err(dev->dev, "failed to initialize DRM FB helper\n");
		goto fini;
	}

	ret = drm_fb_helper_single_add_all_connectors(fbdev);
	if (ret) {
		dev_err(dev->dev, "failed to add connectors\n");
		goto fini;
	}

	ret = drm_fb_helper_initial_config(fbdev, FBDEV_BPP);
	if (ret) {
		dev_err(dev->dev, "failed to set initial configuration\n");
		goto fini;
	}
	priv->fb_helper = fbdev;

	return 0;

fini:
	drm_fb_helper_fini(fbdev);

	return ret;
}

void mtk_fbdev_destroy(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *helper = priv->fb_helper;
	struct fb_info *info = helper->fbdev;

	if (info) {
		int err;

		err = unregister_framebuffer(info);
		if (err < 0)
			DRM_DEBUG_KMS("failed to unregister framebuffer\n");

		if (info->cmap.len)
			fb_dealloc_cmap(&info->cmap);

		framebuffer_release(info);
	}

	if (helper->fb) {
		drm_framebuffer_unregister_private(helper->fb);
		mtk_drm_fb_destroy(helper->fb);
	}

	drm_fb_helper_fini(helper);
}

void mtk_drm_mode_output_poll_changed(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;

	if (priv->fb_helper)
		drm_fb_helper_hotplug_event(priv->fb_helper);
}
#endif

/*
 * Wait for any exclusive fence in fb's gem object's reservation object.
 *
 * Returns -ERESTARTSYS if interrupted, else 0.
 */
int mtk_fb_wait(struct drm_framebuffer *fb)
{
	struct drm_gem_object *gem;
	struct reservation_object *resv;
	long ret;

	if (!fb)
		return 0;

	gem = mtk_fb_get_gem_obj(fb);
	if (!gem || !gem->dma_buf || !gem->dma_buf->resv)
		return 0;

	resv = gem->dma_buf->resv;
	ret = reservation_object_wait_timeout_rcu(resv, false, true,
						  MAX_SCHEDULE_TIMEOUT);
	/* MAX_SCHEDULE_TIMEOUT on success, -ERESTARTSYS if interrupted */
	if (WARN_ON(ret < 0))
		return ret;

	return 0;
}

struct drm_framebuffer *mtk_drm_mode_fb_create(struct drm_device *dev,
					       struct drm_file *file,
					       struct drm_mode_fb_cmd2 *cmd)
{
	struct mtk_drm_fb *mtk_fb;
	struct drm_gem_object *gem;
	unsigned int width = cmd->width;
	unsigned int height = cmd->height;
	unsigned int size, bpp;
	int ret;

	if (drm_format_num_planes(cmd->pixel_format) != 1)
		return ERR_PTR(-EINVAL);

	gem = drm_gem_object_lookup(dev, file, cmd->handles[0]);
	if (!gem)
		return ERR_PTR(-ENOENT);

	bpp = drm_format_plane_cpp(cmd->pixel_format, 0);
	size = (height - 1) * cmd->pitches[0] + width * bpp;
	size += cmd->offsets[0];

	if (gem->size < size) {
		ret = -EINVAL;
		goto unreference;
	}

	mtk_fb = mtk_drm_framebuffer_init(dev, cmd, gem);
	if (IS_ERR(mtk_fb)) {
		ret = PTR_ERR(mtk_fb);
		goto unreference;
	}

	return &mtk_fb->base;

unreference:
	drm_gem_object_unreference_unlocked(gem);
	return ERR_PTR(ret);
}

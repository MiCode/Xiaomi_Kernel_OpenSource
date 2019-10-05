/*
 * Copyright (c) 2016 MediaTek Inc.
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

#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fbdev.h"
#include "mtk_drm_assert.h"

#define to_drm_private(x) container_of(x, struct mtk_drm_private, fb_helper)
#define ALIGN_TO_32(x) ALIGN_TO(x, 32)

#define MTK_LEGACY_FB_MAP
#ifndef MTK_LEGACY_FB_MAP
static int mtk_drm_fbdev_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct drm_fb_helper *helper = info->par;
	struct mtk_drm_private *private = helper->dev->dev_private;

	return mtk_drm_gem_mmap_buf(private->fbdev_bo, vma);
}
#endif
static struct fb_ops mtk_fbdev_ops = {
	.owner = THIS_MODULE,
	.fb_fillrect = drm_fb_helper_cfb_fillrect,
	.fb_copyarea = drm_fb_helper_cfb_copyarea,
	.fb_imageblit = drm_fb_helper_cfb_imageblit,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_blank = drm_fb_helper_blank,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_setcmap = drm_fb_helper_setcmap,
#ifndef MTK_LEGACY_FB_MAP
	.fb_mmap = mtk_drm_fbdev_mmap,
#endif
};

bool mtk_drm_lcm_is_connect(void)
{
	struct device_node *chosen_node;

	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node,
			"atag,videolfb", (int *)&size);
		if (videolfb_tag) {
			DDPINFO("[DT][videolfb] islcmconnected = %d\n",
				videolfb_tag->islcmfound);

			return videolfb_tag->islcmfound;
		}

		DDPINFO("[DT][videolfb] videolfb_tag not found\n");
	} else {
		DDPINFO("[DT][videolfb] of_chosen not found\n");
	}

	return false;
}

int _parse_tag_videolfb(unsigned int *vramsize, phys_addr_t *fb_base,
			unsigned int *fps)
{
	struct device_node *chosen_node;

	*fps = 6000;
	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node, "atag,videolfb", (int *)&size);
		if (videolfb_tag) {
			*vramsize = videolfb_tag->vram;
			*fb_base = videolfb_tag->fb_base;
			*fps = videolfb_tag->fps;
			if (*fps == 0)
				*fps = 6000;
			DDPINFO("[DT][videolfb] fb_base	  = 0x%lx\n",
				(unsigned long)*fb_base);
			DDPINFO("[DT][videolfb] vram	  = 0x%x (%d)\n",
				*vramsize, *vramsize);
			DDPINFO("[DT][videolfb] fps        = %d\n", *fps);

			return 0;
		}

		DDPINFO("[DT][videolfb] videolfb_tag not found\n");
		goto found;
	} else {
		DDPINFO("[DT][videolfb] of_chosen not found\n");
	}
	return -1;

found:
	DDPINFO("[DT][videolfb] fb_base    = 0x%lx\n", (unsigned long)*fb_base);
	DDPINFO("[DT][videolfb] vram       = 0x%x (%d)\n", *vramsize,
		*vramsize);
	DDPINFO("[DT][videolfb] fps	   = %d\n", *fps);

	return 0;
}

static int mtk_fbdev_probe(struct drm_fb_helper *helper,
			   struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = helper->dev;
	struct mtk_drm_private *private = helper->dev->dev_private;
	struct drm_mode_fb_cmd2 mode = {0};
	struct mtk_drm_gem_obj *mtk_gem;
	struct fb_info *info;
	struct drm_framebuffer *fb;
	unsigned int bytes_per_pixel, vramsize, fps;
	unsigned long offset;
	size_t size;
	int err;
	phys_addr_t fb_base;

	bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);
	mode.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
						      sizes->surface_depth);

	if (_parse_tag_videolfb(&vramsize, &fb_base, &fps) < 0) {
		mode.width = sizes->surface_width;
		mode.height = sizes->surface_height;
		mode.pitches[0] = sizes->surface_width * bytes_per_pixel;
		size = mode.pitches[0] * mode.height;
		mtk_gem = mtk_drm_gem_create(dev, size, true);
		if (IS_ERR(mtk_gem))
			return PTR_ERR(mtk_gem);
	} else {
		mode.width = ALIGN_TO_32(sizes->surface_width);
		/* LK pre-allocate triple buffer */
		mode.height = ALIGN_TO_32(sizes->surface_height) * 3;
		mode.pitches[0] =
			ALIGN_TO_32(sizes->surface_width) * bytes_per_pixel;
		size = mode.pitches[0] * mode.height;
		mtk_gem = mtk_drm_fb_gem_insert(dev, size, fb_base, vramsize);
		if (IS_ERR(mtk_gem))
			return PTR_ERR(mtk_gem);
	}

	private->fbdev_bo = &mtk_gem->base;

	info = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(info)) {
		err = PTR_ERR(info);
		dev_err(dev->dev, "failed to allocate framebuffer info, %d\n",
			err);
		goto err_gem_free_object;
	}

	fb = mtk_drm_framebuffer_create(dev, &mode, private->fbdev_bo);
	if (IS_ERR(fb)) {
		err = PTR_ERR(fb);
		dev_err(dev->dev, "failed to allocate DRM framebuffer, %d\n",
			err);
		goto err_release_fbi;
	}
	helper->fb = fb;

	info->par = helper;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->fbops = &mtk_fbdev_ops;

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->format->depth);
	drm_fb_helper_fill_var(info, helper, sizes->fb_width, sizes->fb_height);

	dev->mode_config.fb_base = fb_base;
	info->screen_base = mtk_gem->kvaddr;
	info->screen_size = size;
	info->fix.smem_len = size;
	info->fix.smem_start = fb_base;

	mtk_drm_assert_fb_init(mtk_gem->kvaddr, mtk_gem->dma_addr,
			       sizes->surface_width, sizes->surface_height);

	DRM_DEBUG_KMS("FB [%ux%u]-%u offset=%lu size=%zd\n", fb->width,
		      fb->height, fb->format->depth, offset, size);

	info->skip_vt_switch = true;
	return 0;

err_release_fbi:
err_gem_free_object:
	mtk_drm_gem_free_object(&mtk_gem->base);
	return err;
}

static const struct drm_fb_helper_funcs mtk_drm_fb_helper_funcs = {
	.fb_probe = mtk_fbdev_probe,
};

int mtk_fbdev_init(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *helper = &priv->fb_helper;
	int ret;

	DDPINFO("%s+\n", __func__);
	if (!dev->mode_config.num_crtc || !dev->mode_config.num_connector)
		return -EINVAL;

	drm_fb_helper_prepare(dev, helper, &mtk_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, helper,
				 dev->mode_config.num_connector);
	if (ret) {
		dev_err(dev->dev, "failed to initialize DRM FB helper, %d\n",
			ret);
		goto fini;
	}

	ret = drm_fb_helper_single_add_all_connectors(helper);
	if (ret) {
		dev_err(dev->dev, "failed to add connectors, %d\n", ret);
		goto fini;
	}

	ret = drm_fb_helper_initial_config(helper, 32);
	if (ret) {
		dev_err(dev->dev, "failed to set initial configuration, %d\n",
			ret);
		goto fini;
	}
	DDPINFO("%s-\n", __func__);

	return 0;

fini:
	drm_fb_helper_fini(helper);

	return ret;
}

void mtk_fbdev_fini(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *helper = &priv->fb_helper;

	drm_fb_helper_unregister_fbi(helper);

	if (helper->fb) {
		drm_framebuffer_unregister_private(helper->fb);
		drm_framebuffer_remove(helper->fb);
	}

	drm_fb_helper_fini(helper);
}

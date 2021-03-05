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

#include <linux/gfp.h>
#include <linux/kmemleak.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem.h>
#include <drm/drm_crtc.h>
#include <drm/drm_atomic_helper.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fbdev.h"
#include "mtk_drm_assert.h"
#include "mtk_log.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_helper.h"
#include "mtk_log.h"
#include "mtk_drm_mmp.h"

#define to_drm_private(x) container_of(x, struct mtk_drm_private, fb_helper)
#define ALIGN_TO_32(x) ALIGN_TO(x, 32)

struct fb_info *debug_info;

unsigned int mtk_drm_fb_fm_auto_test(struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *drm_dev = fb_helper->dev;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *private;
	int ret = 0;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
			typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return -1;
	}
	mtk_crtc = to_mtk_crtc(crtc);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!mtk_crtc->enabled || mtk_crtc->ddp_mode == DDP_NO_USE) {
		DDPINFO("crtc 0 is already sleep, skip\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	private = drm_dev->dev_private;
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_IDLE_MGR)) {
		mtk_drm_set_idlemgr(crtc, 0, 0);
	}

	ret = mtk_crtc_lcm_ATA(crtc);

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_IDLE_MGR))
		mtk_drm_set_idlemgr(crtc, 1, 0);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (ret == 0)
		DDPPR_ERR("ATA LCM failed\n");
	else
		DDPPR_ERR("ATA LCM passed\n");

	return ret;
}

static int mtk_drm_fb_ioctl(struct fb_info *info, unsigned int cmd,
		       unsigned long arg)
{
	switch (cmd) {
	case MTKFB_FACTORY_AUTO_TEST:
	{
		unsigned int result = 0;
		void __user *argp = (void __user *)arg;

		DDPMSG("factory mode: lcm auto test\n");
		result = mtk_drm_fb_fm_auto_test(info);
		return copy_to_user(argp, &result, sizeof(result)) ?
					-EFAULT : 0;
	}
	default:
		DDPINFO("%s: Not support:info=0x%p, cmd=0x%08x, arg=0x%08lx\n",
			     __func__, info, (unsigned int)cmd, arg);
		break;
	}
	return 0;
}

static int mtk_drm_fb_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *drm_dev = fb_helper->dev;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct cmdq_pkt *cmdq_handle;
	int ret;

	ret = drm_fb_helper_pan_display(var, info);

	crtc = list_first_entry(&(drm_dev)->mode_config.crtc_list,
			typeof(*crtc), head);

	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return ret;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);

	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle,
			DDP_FIRST_PATH, 0);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return ret;
}

/* used when early porting, test pan display*/

void disp_get_fb_address(unsigned long *fbVirAddr)
{
	*fbVirAddr = (unsigned long)debug_info->screen_base;
	pr_info(
		  "%s fbdev->fb_va_base = 0x%p\n",
		  __func__, debug_info->screen_base);
}

int pan_display_test(int frame_num, int bpp)
{
	int i, j;
	int Bpp = bpp / 8;
	unsigned char *fb_va;
	unsigned int fb_size;
	int w, h, fb_h;
	int yoffset_max;
	int yoffset;

	debug_info->var.yoffset = 0;
	disp_get_fb_address((unsigned long *)&fb_va);
	if (!fb_va)
		return 0;

	if (!mtk_crtc_frame_buffer_existed())
		return 0;

	fb_size = debug_info->fix.smem_len;
	w = debug_info->var.xres;
	h = debug_info->var.yres;
	fb_h = fb_size / (ALIGN_TO(w, 32) * Bpp) - 10;

	pr_info("%s: frame_num=%d,bpp=%d, w=%d,h=%d,fb_h=%d\n",
		__func__, frame_num, bpp, w, h, fb_h);

	for (i = 0; i < fb_h; i++)
		for (j = 0; j < w; j++) {
			int x = (i * ALIGN_TO(w, 32) + j) * Bpp;

			fb_va[x++] = (i + j) % 256;
			fb_va[x++] = (i + j) % 256;
			fb_va[x++] = (i + j) % 256;
			if (Bpp == 4)
				fb_va[x++] = 255;
		}

	debug_info->var.bits_per_pixel = bpp;

	yoffset_max = fb_h - h;
	yoffset = 0;
	for (i = 0; i < frame_num; i++, yoffset += 10) {

		if (yoffset >= yoffset_max)
			yoffset = 0;

		debug_info->var.xoffset = 0;
		debug_info->var.yoffset = yoffset;
		mtk_drm_fb_pan_display(&debug_info->var, debug_info);
	}

	DDPMSG("%s, %d--\n", __func__, __LINE__);
	return 0;
}


#define MTK_LEGACY_FB_MAP
#ifndef MTK_LEGACY_FB_MAP
static int mtk_drm_fbdev_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct drm_fb_helper *helper = info->par;
	struct mtk_drm_private *private = helper->dev->dev_private;

	debug_info = info;
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
	.fb_pan_display = mtk_drm_fb_pan_display,
	.fb_setcmap = drm_fb_helper_setcmap,
#ifndef MTK_LEGACY_FB_MAP
	.fb_mmap = mtk_drm_fbdev_mmap,
#endif
	.fb_ioctl = mtk_drm_fb_ioctl,
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
		if (videolfb_tag)
			return videolfb_tag->islcmfound;

		DDPINFO("[DT][videolfb] videolfb_tag not found\n");
	} else {
		DDPINFO("[DT][videolfb] of_chosen not found\n");
	}

	return false;
}

int _parse_tag_videolfb(unsigned int *vramsize, phys_addr_t *fb_base,
			unsigned int *fps)
{
#ifdef CONFIG_MTK_DISP_NO_LK
		return -1;
#else
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
#endif
}

int free_fb_buf(void)
{
	unsigned long va_start = 0;
	unsigned long va_end = 0;
	phys_addr_t fb_base;
	unsigned int vramsize, fps;

	_parse_tag_videolfb(&vramsize, &fb_base, &fps);

	if (!fb_base) {
		DDPINFO("%s:get fb pa error\n", __func__);
		return -1;
	}

	va_start = (unsigned long)__va(fb_base);
	va_end = (unsigned long)__va(fb_base + (unsigned long)vramsize);
	if (va_start)
		free_reserved_area((void *)va_start,
				   (void *)va_end, 0xff, "fbmem");
	else
		DDPINFO("%s:va invalid\n", __func__);

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
	unsigned int bytes_per_pixel, vramsize = 0, fps = 0;
	size_t size;
	int err;
	phys_addr_t fb_base = 0;

	DDPMSG("%s+\n", __func__);
	bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);
	mode.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
						      sizes->surface_depth);

	if (_parse_tag_videolfb(&vramsize, &fb_base, &fps) < 0) {
		DDPINFO("[DT][videolfb] fb_base   = 0x%lx\n",
			(unsigned long)fb_base);
		DDPINFO("[DT][videolfb] vram	  = 0x%x (%d)\n",
			vramsize, vramsize);
		DDPINFO("[DT][videolfb] fps	    = %d\n", fps);

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
		kmemleak_ignore(mtk_gem);
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
	debug_info = info;

#if !defined(CONFIG_DRM_MTK_DISABLE_AEE_LAYER)
	mtk_drm_assert_fb_init(dev,
			       sizes->surface_width, sizes->surface_height);
#endif

	DRM_DEBUG_KMS("FB [%ux%u]-%u size=%zd\n", fb->width,
		      fb->height, fb->format->depth, size);

	info->skip_vt_switch = true;

	DDPMSG("%s-\n", __func__);
	return 0;

err_release_fbi:
err_gem_free_object:
	mtk_drm_gem_free_object(&mtk_gem->base);
	return err;
}

static const struct drm_fb_helper_funcs mtk_drm_fb_helper_funcs = {
	.fb_probe = mtk_fbdev_probe,
};

static int mtk_drm_fb_add_one_connector(struct drm_device *dev,
	struct drm_fb_helper *helper)
{
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct drm_encoder *encoder;
	const struct drm_connector_helper_funcs *helper_private;
	int ret = 0;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		helper_private = connector->helper_private;
		if (helper_private->best_encoder)
			encoder = helper_private->best_encoder(connector);
		else
			encoder = drm_atomic_helper_best_encoder(connector);
		if (encoder && (encoder->possible_crtcs & 0x1)) {
			ret = drm_fb_helper_add_one_connector(
				helper, connector);
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	return ret;
}

int mtk_fbdev_init(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *helper = &priv->fb_helper;
	int ret;

	DDPMSG("%s+\n", __func__);
	if (!dev->mode_config.num_crtc || !dev->mode_config.num_connector)
		return -EINVAL;

	drm_fb_helper_prepare(dev, helper, &mtk_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, helper, 1);
	if (ret) {
		dev_err(dev->dev, "failed to initialize DRM FB helper, %d\n",
			ret);
		goto fini;
	}

	ret = mtk_drm_fb_add_one_connector(dev, helper);
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
	DDPMSG("%s-\n", __func__);

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

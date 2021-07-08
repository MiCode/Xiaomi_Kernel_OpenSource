/*
 * @File
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this file
 * under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(CONFIG_DRM_FBDEV_EMULATION)
#include <linux/version.h>
#include <linux/export.h>
#include <linux/mm.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0))
#include <drm/drmP.h>
#endif
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "drm_pdp_gem.h"
#include "kernel_compatibility.h"

#define FBDEV_NAME "pdpdrmfb"

static struct fb_ops pdp_fbdev_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_debug_enter = drm_fb_helper_debug_enter,
	.fb_debug_leave = drm_fb_helper_debug_leave,
};


static struct fb_info *
pdp_fbdev_helper_alloc(struct drm_fb_helper *helper)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0))
	struct device *dev = helper->dev->dev;
	struct fb_info *info;
	int ret;

	info = framebuffer_alloc(0, dev);
	if (!info)
		return ERR_PTR(-ENOMEM);

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret)
		goto err_release;

	info->apertures = alloc_apertures(1);
	if (!info->apertures) {
		ret = -ENOMEM;
		goto err_free_cmap;
	}

	helper->fbdev = info;

	return info;

err_free_cmap:
	fb_dealloc_cmap(&info->cmap);
err_release:
	framebuffer_release(info);
	return ERR_PTR(ret);
#else
	return drm_fb_helper_alloc_fbi(helper);
#endif
}

static inline void
pdp_fbdev_helper_fill_info(struct drm_fb_helper *helper,
			   struct drm_fb_helper_surface_size *sizes,
			   struct fb_info *info,
			   struct drm_mode_fb_cmd2 __maybe_unused *mode_cmd)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
	drm_fb_helper_fill_fix(info, mode_cmd->pitches[0], helper->fb->depth);
	drm_fb_helper_fill_var(info, helper, sizes->fb_width,
			       sizes->fb_height);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0))
	drm_fb_helper_fill_fix(info, mode_cmd->pitches[0],
			       helper->fb->format->depth);
	drm_fb_helper_fill_var(info, helper, helper->fb->width,
			       helper->fb->height);
#else
	drm_fb_helper_fill_info(info, helper, sizes);
#endif
}

static int pdp_fbdev_probe(struct drm_fb_helper *helper,
			   struct drm_fb_helper_surface_size *sizes)
{
	struct pdp_fbdev *pdp_fbdev =
		container_of(helper, struct pdp_fbdev, helper);
	struct drm_framebuffer *fb =
		to_drm_framebuffer(&pdp_fbdev->fb);
	struct pdp_gem_private *gem_priv = pdp_fbdev->priv->gem_priv;
	struct drm_device *dev = helper->dev;
	struct drm_mode_fb_cmd2 mode_cmd;
	struct pdp_gem_object *pdp_obj;
	struct drm_gem_object *obj;
	struct fb_info *info;
	void __iomem *vaddr;
	size_t obj_size;
	int err;

	if (helper->fb)
		return 0;

	mutex_lock(&dev->struct_mutex);

	/* Create a framebuffer */
	info = pdp_fbdev_helper_alloc(helper);
	if (!info) {
		err = -ENOMEM;
		goto err_unlock_dev;
	}

	memset(&mode_cmd, 0, sizeof(mode_cmd));
	mode_cmd.pitches[0] =
		sizes->surface_width * DIV_ROUND_UP(sizes->surface_bpp, 8);
	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
							  sizes->surface_depth);
	obj_size = PAGE_ALIGN(mode_cmd.height * mode_cmd.pitches[0]);

	obj = pdp_gem_object_create(dev, gem_priv, obj_size, 0);
	if (IS_ERR(obj)) {
		err = PTR_ERR(obj);
		goto err_unlock_dev;
	}

	pdp_obj = to_pdp_obj(obj);

	vaddr = ioremap(pdp_obj->cpu_addr, obj->size);
	if (!vaddr) {
		err = PTR_ERR(vaddr);
		goto err_gem_destroy;
	}

	/* Zero fb memory, fb_memset accounts for iomem address space */
	fb_memset(vaddr, 0, obj_size);

	err = pdp_modeset_validate_init(pdp_fbdev->priv, &mode_cmd,
					&pdp_fbdev->fb, obj);
	if (err)
		goto err_gem_unmap;

	helper->fb = fb;
	helper->fbdev = info;

	/* Fill out the Linux framebuffer info */
	strlcpy(info->fix.id, FBDEV_NAME, sizeof(info->fix.id));
	pdp_fbdev_helper_fill_info(helper, sizes, info, &mode_cmd);
	info->par = helper;
	info->flags = FBINFO_DEFAULT | FBINFO_HWACCEL_DISABLED;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0))
	info->flags |= FBINFO_CAN_FORCE_OUTPUT;
#endif
	info->fbops = &pdp_fbdev_ops;
	info->fix.smem_start = pdp_obj->cpu_addr;
	info->fix.smem_len = obj_size;
	info->screen_base = vaddr;
	info->screen_size = obj_size;
	info->apertures->ranges[0].base = pdp_obj->cpu_addr;
	info->apertures->ranges[0].size = obj_size;

	mutex_unlock(&dev->struct_mutex);
	return 0;

err_gem_unmap:
	iounmap(vaddr);

err_gem_destroy:
	pdp_gem_object_free_priv(gem_priv, obj);

err_unlock_dev:
	mutex_unlock(&dev->struct_mutex);

	DRM_ERROR(FBDEV_NAME " - %s failed (err=%d)\n", __func__, err);
	return err;
}

static const struct drm_fb_helper_funcs pdp_fbdev_helper_funcs = {
	.fb_probe = pdp_fbdev_probe,
};

struct pdp_fbdev *pdp_fbdev_create(struct pdp_drm_private *dev_priv)
{
	struct pdp_fbdev *pdp_fbdev;
	int err;

	pdp_fbdev = kzalloc(sizeof(*pdp_fbdev), GFP_KERNEL);
	if (!pdp_fbdev)
		return ERR_PTR(-ENOMEM);

	drm_fb_helper_prepare(dev_priv->dev, &pdp_fbdev->helper,
			      &pdp_fbdev_helper_funcs);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
	err = drm_fb_helper_init(dev_priv->dev, &pdp_fbdev->helper, 1, 1);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
	err = drm_fb_helper_init(dev_priv->dev, &pdp_fbdev->helper, 1);
#else
	err = drm_fb_helper_init(dev_priv->dev, &pdp_fbdev->helper);
#endif
	if (err)
		goto err_free_fbdev;

	pdp_fbdev->priv = dev_priv;
	pdp_fbdev->preferred_bpp = 32;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0))
	drm_fb_helper_single_add_all_connectors(&pdp_fbdev->helper);
#endif

	/* Call ->fb_probe() */
	err = drm_fb_helper_initial_config(&pdp_fbdev->helper, pdp_fbdev->preferred_bpp);
	if (err)
		goto err_fb_helper_fini;

	DRM_DEBUG_DRIVER(FBDEV_NAME " - fb device registered\n");
	return pdp_fbdev;

err_fb_helper_fini:
	drm_fb_helper_fini(&pdp_fbdev->helper);

err_free_fbdev:
	kfree(pdp_fbdev);

	DRM_ERROR(FBDEV_NAME " - %s, failed (err=%d)\n", __func__, err);
	return ERR_PTR(err);
}

void pdp_fbdev_destroy(struct pdp_fbdev *pdp_fbdev)
{
	struct pdp_framebuffer *pdp_fb;
	struct pdp_gem_object *pdp_obj;
	struct drm_framebuffer *fb;
	struct fb_info *info;

	if (!pdp_fbdev)
		return;

	drm_fb_helper_unregister_fbi(&pdp_fbdev->helper);
	pdp_fb = &pdp_fbdev->fb;

	pdp_obj = to_pdp_obj(pdp_fb->obj[0]);
	if (pdp_obj) {
		info = pdp_fbdev->helper.fbdev;
		iounmap((void __iomem *)info->screen_base);
	}

	drm_gem_object_put(pdp_fb->obj[0]);

	drm_fb_helper_fini(&pdp_fbdev->helper);

	fb = to_drm_framebuffer(pdp_fb);

	/**
	 * If the driver's probe function hasn't been called
	 * (due to deferred setup of the framebuffer device),
	 * then the framebuffer won't have been initialised.
	 * Check this before attempting to clean it up.
	 */
	if (fb && fb->dev)
		drm_framebuffer_cleanup(fb);

	kfree(pdp_fbdev);
}
#endif /* CONFIG_DRM_FBDEV_EMULATION */

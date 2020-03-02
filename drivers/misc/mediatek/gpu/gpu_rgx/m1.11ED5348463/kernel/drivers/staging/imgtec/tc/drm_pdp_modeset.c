/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "drm_pdp_drv.h"

#include <linux/moduleparam.h>
#include <linux/version.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
#include <drm/drm_gem.h>
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
#define drm_gem_fb_create(...) pdp_framebuffer_create(__VA_ARGS__)
#else
#include <drm/drm_gem_framebuffer_helper.h>
#endif

#if defined(PDP_USE_ATOMIC)
#include <drm/drm_atomic_helper.h>
#endif

#include "kernel_compatibility.h"

#define PDP_WIDTH_MIN			640
#define PDP_WIDTH_MAX			1280
#define PDP_HEIGHT_MIN			480
#define PDP_HEIGHT_MAX			1024

#define ODIN_PDP_WIDTH_MAX		1920
#define ODIN_PDP_HEIGHT_MAX		1080

#define PLATO_PDP_WIDTH_MAX		1920
#define PLATO_PDP_HEIGHT_MAX	1080

static bool async_flip_enable = true;

module_param(async_flip_enable, bool, 0444);

MODULE_PARM_DESC(async_flip_enable,
		 "Enable support for 'faked' async flipping (default: Y)");

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
static void pdp_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct pdp_framebuffer *pdp_fb = to_pdp_framebuffer(fb);

	DRM_DEBUG_DRIVER("[FB:%d]\n", fb->base.id);

	drm_framebuffer_cleanup(fb);

	drm_gem_object_put_unlocked(pdp_fb->obj[0]);

	kfree(pdp_fb);
}

static int pdp_framebuffer_create_handle(struct drm_framebuffer *fb,
					 struct drm_file *file,
					 unsigned int *handle)
{
	struct pdp_framebuffer *pdp_fb = to_pdp_framebuffer(fb);

	DRM_DEBUG_DRIVER("[FB:%d]\n", fb->base.id);

	return drm_gem_handle_create(file, pdp_fb->obj[0], handle);
}

static const struct drm_framebuffer_funcs pdp_framebuffer_funcs = {
	.destroy = pdp_framebuffer_destroy,
	.create_handle = pdp_framebuffer_create_handle,
	.dirty = NULL,
};

static struct drm_framebuffer *
pdp_framebuffer_create(struct drm_device *dev,
		       struct drm_file *file,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) || \
	(defined(CHROMIUMOS_KERNEL) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)))
		       const
#endif
		       struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct pdp_drm_private *dev_priv = dev->dev_private;
	struct drm_gem_object *obj;
	struct pdp_framebuffer *pdp_fb;
	int err;

	obj = drm_gem_object_lookup(file, mode_cmd->handles[0]);
	if (!obj) {
		DRM_ERROR("failed to find buffer with handle %u\n",
			  mode_cmd->handles[0]);
		err = -ENOENT;
		goto err_out;
	}

	pdp_fb = kzalloc(sizeof(*pdp_fb), GFP_KERNEL);
	if (!pdp_fb) {
		err = -ENOMEM;
		goto err_obj_put;
	}

	drm_helper_mode_fill_fb_struct(dev_priv->dev, &pdp_fb->base, mode_cmd);
	pdp_fb->obj[0] = obj;

	err = drm_framebuffer_init(dev_priv->dev, &pdp_fb->base,
				   &pdp_framebuffer_funcs);
	if (err) {
		DRM_ERROR("failed to initialise framebuffer (err=%d)\n", err);
		goto err_free_fb;
	}

	DRM_DEBUG_DRIVER("[FB:%d]\n", pdp_fb->base.base.id);

	return &pdp_fb->base;

err_free_fb:
	kfree(pdp_fb);
err_obj_put:
	drm_gem_object_put_unlocked(obj);
err_out:
	return ERR_PTR(err);
}
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)) */


/*************************************************************************
 * DRM mode config callbacks
 **************************************************************************/

static struct drm_framebuffer *
pdp_fb_create(struct drm_device *dev,
			struct drm_file *file,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) || \
	(defined(CHROMIUMOS_KERNEL) && \
	      (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)))
			const
#endif
			struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_framebuffer *fb;

	switch (mode_cmd->pixel_format) {
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
		break;
	default:
		DRM_ERROR_RATELIMITED("pixel format not supported (format = %u)\n",
			  mode_cmd->pixel_format);
		return ERR_PTR(-EINVAL);
	}

	if (mode_cmd->flags & DRM_MODE_FB_INTERLACED) {
		DRM_ERROR_RATELIMITED("interlaced framebuffers not supported\n");
		return ERR_PTR(-EINVAL);
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	if (mode_cmd->modifier[0] != DRM_FORMAT_MOD_NONE) {
		DRM_ERROR_RATELIMITED("format modifier 0x%llx is not supported\n",
			  mode_cmd->modifier[0]);
		return ERR_PTR(-EINVAL);
	}
#endif

	fb = drm_gem_fb_create(dev, file, mode_cmd);
	if (IS_ERR(fb))
		goto out;

	DRM_DEBUG_DRIVER("[FB:%d]\n", fb->base.id);

out:
	return fb;
}

static const struct drm_mode_config_funcs pdp_mode_config_funcs = {
	.fb_create = pdp_fb_create,
	.output_poll_changed = NULL,
#if defined(PDP_USE_ATOMIC)
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
#endif
};


int pdp_modeset_early_init(struct pdp_drm_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;
	int err;

	drm_mode_config_init(dev);

	dev->mode_config.funcs = &pdp_mode_config_funcs;
	dev->mode_config.min_width = PDP_WIDTH_MIN;
	dev->mode_config.min_height = PDP_HEIGHT_MIN;

	switch (dev_priv->version) {
	case PDP_VERSION_APOLLO:
		dev->mode_config.max_width = PDP_WIDTH_MAX;
		dev->mode_config.max_height = PDP_HEIGHT_MAX;
		break;
	case PDP_VERSION_ODIN:
		dev->mode_config.max_width = ODIN_PDP_WIDTH_MAX;
		dev->mode_config.max_height = ODIN_PDP_HEIGHT_MAX;
		break;
	case PDP_VERSION_PLATO:
		dev->mode_config.max_width = PLATO_PDP_WIDTH_MAX;
		dev->mode_config.max_height = PLATO_PDP_HEIGHT_MAX;
		break;
	default:
		BUG();
	}

	DRM_INFO("max_width is %d\n",
		dev->mode_config.max_width);
	DRM_INFO("max_height is %d\n",
		dev->mode_config.max_height);

	dev->mode_config.fb_base = 0;
	dev->mode_config.async_page_flip = async_flip_enable;

	DRM_INFO("%s async flip support is %s\n",
		 dev->driver->name, async_flip_enable ? "enabled" : "disabled");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	dev->mode_config.allow_fb_modifiers = true;
#endif

	dev_priv->plane = pdp_plane_create(dev, DRM_PLANE_TYPE_PRIMARY);
	if (IS_ERR(dev_priv->plane)) {
		DRM_ERROR("failed to create a primary plane\n");
		err = PTR_ERR(dev_priv->plane);
		goto err_config_cleanup;
	}

	dev_priv->crtc = pdp_crtc_create(dev, 0, dev_priv->plane);
	if (IS_ERR(dev_priv->crtc)) {
		DRM_ERROR("failed to create a CRTC\n");
		err = PTR_ERR(dev_priv->crtc);
		goto err_config_cleanup;
	}

	switch (dev_priv->version) {
	case PDP_VERSION_APOLLO:
	case PDP_VERSION_ODIN:
		dev_priv->connector = pdp_dvi_connector_create(dev);
		if (IS_ERR(dev_priv->connector)) {
			DRM_ERROR("failed to create a connector\n");
			err = PTR_ERR(dev_priv->connector);
			goto err_config_cleanup;
		}

		dev_priv->encoder = pdp_tmds_encoder_create(dev);
		if (IS_ERR(dev_priv->encoder)) {
			DRM_ERROR("failed to create an encoder\n");
			err = PTR_ERR(dev_priv->encoder);
			goto err_config_cleanup;
		}

		err = drm_connector_attach_encoder(dev_priv->connector,
						   dev_priv->encoder);
		if (err) {
			DRM_ERROR("failed to attach [ENCODER:%d:%s] to [CONNECTOR:%d:%s] (err=%d)\n",
				  dev_priv->encoder->base.id,
				  dev_priv->encoder->name,
				  dev_priv->connector->base.id,
				  dev_priv->connector->name,
				  err);
			goto err_config_cleanup;
		}
		break;
	case PDP_VERSION_PLATO:
		// PLATO connectors are created in HDMI component driver
		break;
	default:
		BUG();
	}

	DRM_DEBUG_DRIVER("initialised\n");

	return 0;

err_config_cleanup:
	drm_mode_config_cleanup(dev);

	return err;
}

int pdp_modeset_late_init(struct pdp_drm_private *dev_priv)
{
	struct drm_device *ddev = dev_priv->dev;

	drm_mode_config_reset(ddev);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0))
	if (dev_priv->connector != NULL) {
		int err;

		err = drm_connector_register(dev_priv->connector);
		if (err) {
			DRM_ERROR("[CONNECTOR:%d:%s] failed to register (err=%d)\n",
				  dev_priv->connector->base.id,
				  dev_priv->connector->name,
				  err);
			return err;
		}
	}
#endif
	return 0;
}

void pdp_modeset_early_cleanup(struct pdp_drm_private *dev_priv)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0))
	if (dev_priv->connector != NULL)
		drm_connector_unregister(dev_priv->connector);
#endif
}

void pdp_modeset_late_cleanup(struct pdp_drm_private *dev_priv)
{
	drm_mode_config_cleanup(dev_priv->dev);

	DRM_DEBUG_DRIVER("cleaned up\n");
}

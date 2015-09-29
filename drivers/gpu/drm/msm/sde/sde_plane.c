/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sde_kms.h"

struct sde_plane {
	struct drm_plane base;
	const char *name;
	uint32_t nformats;
	uint32_t formats[32];
};
#define to_sde_plane(x) container_of(x, struct sde_plane, base)

static int sde_plane_update(struct drm_plane *plane,
		struct drm_crtc *crtc, struct drm_framebuffer *fb,
		int crtc_x, int crtc_y,
		unsigned int crtc_w, unsigned int crtc_h,
		uint32_t src_x, uint32_t src_y,
		uint32_t src_w, uint32_t src_h)
{
	return 0;
}

static int sde_plane_disable(struct drm_plane *plane)
{
	return 0;
}

static void sde_plane_destroy(struct drm_plane *plane)
{
	struct sde_plane *sde_plane = to_sde_plane(plane);
	struct msm_drm_private *priv = plane->dev->dev_private;

	if (priv->kms)
		sde_plane_disable(plane);

	drm_plane_cleanup(plane);

	kfree(sde_plane);
}

/* helper to install properties which are common to planes and crtcs */
void sde_plane_install_properties(struct drm_plane *plane,
		struct drm_mode_object *obj)
{
}

int sde_plane_set_property(struct drm_plane *plane,
		struct drm_property *property, uint64_t val)
{
	return -EINVAL;
}

static const struct drm_plane_funcs sde_plane_funcs = {
		.update_plane = sde_plane_update,
		.disable_plane = sde_plane_disable,
		.destroy = sde_plane_destroy,
		.set_property = sde_plane_set_property,
};

void sde_plane_set_scanout(struct drm_plane *plane,
		struct drm_framebuffer *fb)
{
}

int sde_plane_mode_set(struct drm_plane *plane,
		struct drm_crtc *crtc, struct drm_framebuffer *fb,
		int crtc_x, int crtc_y,
		unsigned int crtc_w, unsigned int crtc_h,
		uint32_t src_x, uint32_t src_y,
		uint32_t src_w, uint32_t src_h)
{
	return 0;
}

/* initialize plane */
struct drm_plane *sde_plane_init(struct drm_device *dev, bool private_plane)
{
	struct drm_plane *plane = NULL;
	struct sde_plane *sde_plane;
	int ret;
	enum drm_plane_type type;

	sde_plane = kzalloc(sizeof(*sde_plane), GFP_KERNEL);
	if (!sde_plane) {
		ret = -ENOMEM;
		goto fail;
	}

	plane = &sde_plane->base;

	type = private_plane ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
	drm_universal_plane_init(dev, plane, 0xff, &sde_plane_funcs,
				 sde_plane->formats, sde_plane->nformats,
				 type);

	sde_plane_install_properties(plane, &plane->base);

	return plane;

fail:
	if (plane)
		sde_plane_destroy(plane);

	return ERR_PTR(ret);
}

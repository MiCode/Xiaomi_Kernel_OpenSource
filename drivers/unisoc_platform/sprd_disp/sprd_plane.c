// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/dma-buf.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_plane_helper.h>

#include "sprd_crtc.h"
#include "sprd_drm.h"
#include "sprd_gem.h"
#include "sprd_plane.h"

static int sprd_plane_prepare_fb(struct drm_plane *plane,
				struct drm_plane_state *new_state)
{
	struct drm_plane_state *curr_state = plane->state;
	struct sprd_crtc *crtc = to_sprd_crtc(new_state->crtc);

	if ((curr_state->fb == new_state->fb) || !new_state->fb)
		return 0;

	if (crtc->ops->prepare_fb)
		crtc->ops->prepare_fb(crtc, new_state);

	return 0;
}

static void sprd_plane_cleanup_fb(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct drm_plane_state *curr_state = plane->state;
	struct sprd_crtc *crtc = to_sprd_crtc(old_state->crtc);

	if ((curr_state->fb == old_state->fb) || !old_state->fb)
		return;

	if (crtc->ops->cleanup_fb)
		crtc->ops->cleanup_fb(crtc, old_state);
}

static void sprd_plane_atomic_update(struct drm_plane *drm_plane,
					struct drm_atomic_state *old_state)
{
	struct drm_plane_state *drm_state = drm_plane->state;
	struct sprd_crtc *crtc = to_sprd_crtc(drm_state->crtc);
	struct sprd_plane *plane = to_sprd_plane(drm_plane);
	struct sprd_plane_state *state = to_sprd_plane_state(drm_state);
	struct sprd_layer_state *layer = &state->layer;
	struct drm_gem_object *obj;
	struct sprd_gem_obj *sprd_gem;
	int i;

	if (!drm_state->fb)
		return;

	if (drm_plane->state->crtc->state->active_changed) {
		DRM_DEBUG("resume or suspend, no need to update plane\n");
		return;
	}

	if (layer->pallete_en) {
		layer->index = plane->index;
		layer->dst_x = drm_state->crtc_x;
		layer->dst_y = drm_state->crtc_y;
		layer->dst_w = drm_state->crtc_w;
		layer->dst_h = drm_state->crtc_h;
		layer->alpha = drm_state->alpha;
		layer->blending = drm_state->pixel_blend_mode;
		crtc->pending_planes++;
		DRM_DEBUG("%s() pallete_color = %u, index = %u\n",
			__func__, layer->pallete_color, layer->index);
		return;
	}

	layer->index = plane->index;
	layer->src_x = drm_state->src_x >> 16;
	layer->src_y = drm_state->src_y >> 16;
	layer->src_w = drm_state->src_w >> 16;
	layer->src_h = drm_state->src_h >> 16;
	layer->dst_x = drm_state->crtc_x;
	layer->dst_y = drm_state->crtc_y;
	layer->dst_w = drm_state->crtc_w;
	layer->dst_h = drm_state->crtc_h;
	layer->alpha = drm_state->alpha;
	layer->rotation = drm_state->rotation;
	layer->blending = drm_state->pixel_blend_mode;
	layer->rotation = drm_state->rotation;
	layer->planes = drm_state->fb->format->num_planes;
	layer->format = drm_state->fb->format->format;

	DRM_DEBUG("%s() alpha = %u, blending = %u, rotation = %u, y2r_coef = %u\n",
		  __func__, layer->alpha, layer->blending,
		  layer->rotation, layer->y2r_coef);

	DRM_DEBUG("%s() xfbc = %u, hsize_r = %u, hsize_y = %u, hsize_uv = %u\n",
		  __func__, layer->xfbc, layer->fbc_hsize_r,
		  layer->fbc_hsize_y, layer->fbc_hsize_uv);

	for (i = 0; i < layer->planes; i++) {
		obj = drm_gem_fb_get_obj(drm_state->fb, i);
		sprd_gem = to_sprd_gem_obj(obj);
		layer->addr[i] = sprd_gem->dma_addr + drm_state->fb->offsets[i];
		layer->pitch[i] = drm_state->fb->pitches[i];
	}

	crtc->pending_planes++;
}

static int sprd_plane_atomic_check(struct drm_plane *plane,
				  struct drm_atomic_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	return 0;
}

static const struct drm_plane_helper_funcs sprd_plane_helper_funcs = {
	.prepare_fb = sprd_plane_prepare_fb,
	.cleanup_fb = sprd_plane_cleanup_fb,
	.atomic_check = sprd_plane_atomic_check,
	.atomic_update = sprd_plane_atomic_update,
};

static void sprd_plane_reset(struct drm_plane *drm_plane)
{
	struct sprd_plane *plane = to_sprd_plane(drm_plane);
	struct sprd_plane_state *state;

	DRM_INFO("%s()\n", __func__);

	if (drm_plane->state) {
		__drm_atomic_helper_plane_destroy_state(drm_plane->state);

		state = to_sprd_plane_state(drm_plane->state);
		memset(state, 0, sizeof(*state));
	} else {
		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (!state)
			return;
		drm_plane->state = &state->base;
	}

	state->base.plane = drm_plane;
	state->base.zpos = plane->index;
	state->base.alpha = 255;
	state->base.pixel_blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
}

static struct drm_plane_state *
sprd_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct sprd_plane_state *state;
	struct sprd_plane_state *old_state = to_sprd_plane_state(plane->state);

	DRM_DEBUG("%s()\n", __func__);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &state->base);

	WARN_ON(state->base.plane != plane);

	state->layer = old_state->layer;

	return &state->base;
}

static void sprd_plane_atomic_destroy_state(struct drm_plane *plane,
					    struct drm_plane_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	__drm_atomic_helper_plane_destroy_state(state);
	kfree(to_sprd_plane_state(state));
}

static int sprd_plane_atomic_set_property(struct drm_plane *drm_plane,
					  struct drm_plane_state *drm_state,
					  struct drm_property *property,
					  u64 val)
{
	struct sprd_plane *plane = to_sprd_plane(drm_plane);
	struct sprd_plane_state *state = to_sprd_plane_state(drm_state);
	struct sprd_layer_state *layer = &state->layer;

	DRM_DEBUG("%s() name = %s, val = %llu\n",
		  __func__, property->name, val);

	if (property == plane->fbc_enabled_property)
		layer->xfbc = val;
	else if (property == plane->fbc_hsize_r_property)
		layer->fbc_hsize_r = val;
	else if (property == plane->fbc_hsize_y_property)
		layer->fbc_hsize_y = val;
	else if (property == plane->fbc_hsize_uv_property)
		layer->fbc_hsize_uv = val;
	else if (property == plane->y2r_coef_property)
		layer->y2r_coef = val;
	else if (property == plane->pallete_en_property)
		layer->pallete_en = val;
	else if (property == plane->pallete_color_property)
		layer->pallete_color = val;
	else if (property == plane->secure_en_property)
		layer->secure_en = val;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int sprd_plane_atomic_get_property(struct drm_plane *drm_plane,
					  const struct drm_plane_state *drm_state,
					  struct drm_property *property,
					  u64 *val)
{
	struct sprd_plane *plane = to_sprd_plane(drm_plane);
	const struct sprd_plane_state *state = to_sprd_plane_state(drm_state);
	const struct sprd_layer_state *layer = &state->layer;

	DRM_DEBUG("%s() name = %s\n", __func__, property->name);

	if (property == plane->fbc_enabled_property)
		*val = layer->xfbc;
	else if (property == plane->fbc_hsize_r_property)
		*val = layer->fbc_hsize_r;
	else if (property == plane->fbc_hsize_y_property)
		*val = layer->fbc_hsize_y;
	else if (property == plane->fbc_hsize_uv_property)
		*val = layer->fbc_hsize_uv;
	else if (property == plane->y2r_coef_property)
		*val = layer->y2r_coef;
	else if (property == plane->pallete_en_property)
		*val = layer->pallete_en;
	else if (property == plane->pallete_color_property)
		*val = layer->pallete_color;
	else if (property == plane->secure_en_property)
		*val = layer->secure_en;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static const struct drm_plane_funcs sprd_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = sprd_plane_reset,
	.atomic_duplicate_state = sprd_plane_atomic_duplicate_state,
	.atomic_destroy_state = sprd_plane_atomic_destroy_state,
	.atomic_set_property = sprd_plane_atomic_set_property,
	.atomic_get_property = sprd_plane_atomic_get_property,
};

static int sprd_plane_create_properties(struct sprd_plane *plane, int index)
{
	struct drm_property *prop;
	unsigned int supported_modes = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
				       BIT(DRM_MODE_BLEND_PREMULTI) |
				       BIT(DRM_MODE_BLEND_COVERAGE);

	/* create rotation property */
	drm_plane_create_rotation_property(&plane->base,
					   DRM_MODE_ROTATE_0,
					   DRM_MODE_ROTATE_MASK |
					   DRM_MODE_REFLECT_MASK);

	/* create alpha property */
	drm_plane_create_alpha_property(&plane->base);

	/* create blend mode property */
	drm_plane_create_blend_mode_property(&plane->base, supported_modes);

	/* create zpos property */
	drm_plane_create_zpos_immutable_property(&plane->base, index);

	/* create fbc enabled property */
	prop = drm_property_create_range(plane->base.dev, 0,
			"FBC enabled", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&plane->base.base, prop, 0);
	plane->fbc_enabled_property = prop;

	/* create fbc header size property */
	prop = drm_property_create_range(plane->base.dev, 0,
			"FBC header size RGB", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&plane->base.base, prop, 0);
	plane->fbc_hsize_r_property = prop;

	prop = drm_property_create_range(plane->base.dev, 0,
			"FBC header size Y", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&plane->base.base, prop, 0);
	plane->fbc_hsize_y_property = prop;

	prop = drm_property_create_range(plane->base.dev, 0,
			"FBC header size UV", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&plane->base.base, prop, 0);
	plane->fbc_hsize_uv_property = prop;

	/* create y2r coef property */
	prop = drm_property_create_range(plane->base.dev, 0,
			"YUV2RGB coef", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&plane->base.base, prop, 0);
	plane->y2r_coef_property = prop;

	/* create pallete enable property */
	prop = drm_property_create_range(plane->base.dev, 0,
			"pallete enable", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&plane->base.base, prop, 0);
	plane->pallete_en_property = prop;

	/* create pallete color property */
	prop = drm_property_create_range(plane->base.dev, 0,
			"pallete color", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&plane->base.base, prop, 0);
	plane->pallete_color_property = prop;

	/* create secure enable property */
	prop = drm_property_create_range(plane->base.dev, 0,
			"secure enable", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&plane->base.base, prop, 0);
	plane->secure_en_property = prop;

	return 0;
}

struct sprd_plane *sprd_plane_init(struct drm_device *drm,
					struct sprd_crtc_capability *cap)
{
	struct sprd_plane *planes = NULL;
	int err, i;
	enum drm_plane_type type;

	planes = devm_kcalloc(drm->dev, cap->max_layers, sizeof(*planes), GFP_KERNEL);
	if (!planes)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < cap->max_layers; i++) {
		type = i==0 ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
		err = drm_universal_plane_init(drm, &planes[i].base,
					       1 << drm->mode_config.num_crtc,
					       &sprd_plane_funcs, cap->fmts_ptr,
					       cap->fmts_cnt, NULL, type, NULL);
		if (err) {
			DRM_ERROR("failed to initialize primary plane\n");
			return ERR_PTR(err);
		}

		drm_plane_helper_add(&planes[i].base, &sprd_plane_helper_funcs);

		sprd_plane_create_properties(&planes[i], i);

		planes[i].index = i;
	}

	return planes;
}
